//! Dynamic library loading for C++ classes
//!
//! Handles loading, symbol resolution, and unloading of C++ shared libraries.

use crate::error::{CppError, Result};
use crate::ffi::*;
use libloading::{Library, Symbol};
use parking_lot::RwLock;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::path::{Path, PathBuf};

/// Information about a loaded C++ class
#[derive(Debug, Clone)]
pub struct ClassInfo {
    /// Class name
    pub name: String,
    /// Size in bytes
    pub size: usize,
    /// Alignment requirement
    pub alignment: usize,
    /// Whether the class supports serialization
    pub supports_serialization: bool,
}

/// Information about a loaded library
#[derive(Debug, Clone)]
pub struct LibraryInfo {
    /// Library file path
    pub path: PathBuf,
    /// Library name
    pub name: String,
    /// Version string
    pub version: String,
    /// API version
    pub api_version: u32,
    /// Available classes
    pub classes: Vec<ClassInfo>,
}

/// A loaded C++ dynamic library
pub struct CppLibrary {
    /// The underlying library handle
    library: Library,
    /// Library info
    pub info: LibraryInfo,
    /// Cached class info pointers
    class_info_cache: RwLock<HashMap<String, *const FfiClassInfo>>,
    /// Cached vtables
    vtable_cache: RwLock<HashMap<String, *const FfiClassVTable>>,
    /// Function to get class vtable
    get_class_vtable: Option<GetClassVTableFn>,
}

// Safety: Library handle is thread-safe when properly used
unsafe impl Send for CppLibrary {}
unsafe impl Sync for CppLibrary {}

impl CppLibrary {
    /// Load a C++ library from a path
    pub fn load(path: impl AsRef<Path>) -> Result<Self> {
        let path = path.as_ref();

        // Load the library
        let library = unsafe {
            Library::new(path).map_err(|e| CppError::load_error(path, e.to_string()))?
        };

        // Get library info function
        let get_info: Symbol<GetLibraryInfoFn> = unsafe {
            library.get(b"void_get_library_info\0")
                .map_err(|_| CppError::symbol_not_found(
                    path.display().to_string(),
                    "void_get_library_info"
                ))?
        };

        // Get library info
        let ffi_info = get_info();

        // Check API version
        if ffi_info.api_version != VOID_CPP_API_VERSION {
            return Err(CppError::VersionMismatch {
                library_version: ffi_info.api_version.to_string(),
                expected_version: VOID_CPP_API_VERSION.to_string(),
            });
        }

        // Extract strings
        let name = if ffi_info.name.is_null() {
            path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("unknown")
                .to_string()
        } else {
            unsafe { CStr::from_ptr(ffi_info.name) }
                .to_string_lossy()
                .into_owned()
        };

        let version = if ffi_info.version.is_null() {
            "0.0.0".to_string()
        } else {
            unsafe { CStr::from_ptr(ffi_info.version) }
                .to_string_lossy()
                .into_owned()
        };

        // Get class info function
        let get_class_info: Symbol<GetClassInfoFn> = unsafe {
            library.get(b"void_get_class_info\0")
                .map_err(|_| CppError::symbol_not_found(
                    path.display().to_string(),
                    "void_get_class_info"
                ))?
        };

        // Load class information
        let mut classes = Vec::new();
        for i in 0..ffi_info.class_count {
            let class_info_ptr = get_class_info(i);
            if !class_info_ptr.is_null() {
                let class_info = unsafe { &*class_info_ptr };
                let class_name = if class_info.name.is_null() {
                    continue;
                } else {
                    unsafe { CStr::from_ptr(class_info.name) }
                        .to_string_lossy()
                        .into_owned()
                };

                classes.push(ClassInfo {
                    name: class_name,
                    size: class_info.size,
                    alignment: class_info.alignment,
                    supports_serialization: false, // Will be updated from vtable
                });
            }
        }

        // Try to get vtable function (optional)
        let get_class_vtable: Option<GetClassVTableFn> = unsafe {
            library.get(b"void_get_class_vtable\0").ok().map(|s: Symbol<GetClassVTableFn>| *s)
        };

        let info = LibraryInfo {
            path: path.to_path_buf(),
            name,
            version,
            api_version: ffi_info.api_version,
            classes,
        };

        log::info!(
            "Loaded C++ library '{}' v{} with {} classes",
            info.name,
            info.version,
            info.classes.len()
        );

        Ok(Self {
            library,
            info,
            class_info_cache: RwLock::new(HashMap::new()),
            vtable_cache: RwLock::new(HashMap::new()),
            get_class_vtable,
        })
    }

    /// Get information about a specific class
    pub fn get_class_info(&self, class_name: &str) -> Option<&ClassInfo> {
        self.info.classes.iter().find(|c| c.name == class_name)
    }

    /// Get the raw class info pointer (for FFI)
    pub fn get_class_info_ptr(&self, class_name: &str) -> Result<*const FfiClassInfo> {
        // Check cache first
        {
            let cache = self.class_info_cache.read();
            if let Some(ptr) = cache.get(class_name) {
                return Ok(*ptr);
            }
        }

        // Find class index
        let index = self.info.classes.iter()
            .position(|c| c.name == class_name)
            .ok_or_else(|| CppError::ClassNotFound(class_name.to_string()))?;

        // Get the function
        let get_class_info: Symbol<GetClassInfoFn> = unsafe {
            self.library.get(b"void_get_class_info\0")
                .map_err(|_| CppError::symbol_not_found(
                    self.info.path.display().to_string(),
                    "void_get_class_info"
                ))?
        };

        let ptr = get_class_info(index as u32);
        if ptr.is_null() {
            return Err(CppError::ClassNotFound(class_name.to_string()));
        }

        // Cache it
        self.class_info_cache.write().insert(class_name.to_string(), ptr);

        Ok(ptr)
    }

    /// Get the vtable for a class
    pub fn get_class_vtable(&self, class_name: &str) -> Result<*const FfiClassVTable> {
        // Check cache first
        {
            let cache = self.vtable_cache.read();
            if let Some(ptr) = cache.get(class_name) {
                return Ok(*ptr);
            }
        }

        // Get vtable function
        let get_vtable = self.get_class_vtable
            .ok_or_else(|| CppError::symbol_not_found(
                self.info.path.display().to_string(),
                "void_get_class_vtable"
            ))?;

        // Get vtable
        let class_name_c = CString::new(class_name)
            .map_err(|_| CppError::InvalidState("Invalid class name".into()))?;
        let ptr = get_vtable(class_name_c.as_ptr());

        if ptr.is_null() {
            return Err(CppError::ClassNotFound(class_name.to_string()));
        }

        // Cache it
        self.vtable_cache.write().insert(class_name.to_string(), ptr);

        Ok(ptr)
    }

    /// Create an instance of a class
    pub fn create_instance(&self, class_name: &str) -> Result<CppHandle> {
        let class_info_ptr = self.get_class_info_ptr(class_name)?;
        let class_info = unsafe { &*class_info_ptr };

        let create_fn = class_info.create_fn
            .ok_or_else(|| CppError::instance_creation_failed(
                class_name,
                "No create function"
            ))?;

        let handle = create_fn();
        if handle.is_null() {
            return Err(CppError::instance_creation_failed(
                class_name,
                "Create function returned null"
            ));
        }

        Ok(handle)
    }

    /// Destroy an instance
    pub fn destroy_instance(&self, class_name: &str, handle: CppHandle) -> Result<()> {
        if handle.is_null() {
            return Ok(());
        }

        let class_info_ptr = self.get_class_info_ptr(class_name)?;
        let class_info = unsafe { &*class_info_ptr };

        if let Some(destroy_fn) = class_info.destroy_fn {
            destroy_fn(handle);
        }

        Ok(())
    }

    /// Check if the library contains a class
    pub fn has_class(&self, class_name: &str) -> bool {
        self.info.classes.iter().any(|c| c.name == class_name)
    }

    /// Get the library path
    pub fn path(&self) -> &Path {
        &self.info.path
    }

    /// Get the library name
    pub fn name(&self) -> &str {
        &self.info.name
    }

    /// Get the list of class names
    pub fn class_names(&self) -> Vec<&str> {
        self.info.classes.iter().map(|c| c.name.as_str()).collect()
    }

    /// Get a symbol from the library
    ///
    /// # Safety
    /// The caller must ensure the symbol type matches the actual function signature.
    pub unsafe fn get_symbol<T>(&self, name: &[u8]) -> Option<libloading::Symbol<'_, T>> {
        self.library.get(name).ok()
    }
}

impl Drop for CppLibrary {
    fn drop(&mut self) {
        log::debug!("Unloading C++ library '{}'", self.info.name);
        // Library is automatically unloaded when dropped
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_library_info() {
        let info = LibraryInfo {
            path: PathBuf::from("test.dll"),
            name: "test".to_string(),
            version: "1.0.0".to_string(),
            api_version: VOID_CPP_API_VERSION,
            classes: vec![
                ClassInfo {
                    name: "TestClass".to_string(),
                    size: 64,
                    alignment: 8,
                    supports_serialization: true,
                }
            ],
        };

        assert_eq!(info.name, "test");
        assert_eq!(info.classes.len(), 1);
        assert_eq!(info.classes[0].name, "TestClass");
    }
}
