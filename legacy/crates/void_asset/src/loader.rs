//! Asset Loader - Pluggable asset loading system
//!
//! Loaders are responsible for transforming raw asset data into usable game data.
//! They can be registered dynamically by plugins.

use crate::handle::{AssetId, Handle, LoadState};
use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use alloc::collections::BTreeMap;
use core::any::{Any, TypeId};

/// Error during asset loading
#[derive(Debug, Clone)]
pub enum LoadError {
    /// Asset not found
    NotFound(String),
    /// IO error
    IoError(String),
    /// Parse/decode error
    ParseError(String),
    /// Unsupported format
    UnsupportedFormat(String),
    /// Dependency failed to load
    DependencyFailed(AssetId),
    /// Custom error
    Custom(String),
}

impl core::fmt::Display for LoadError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::NotFound(path) => write!(f, "Asset not found: {}", path),
            Self::IoError(msg) => write!(f, "IO error: {}", msg),
            Self::ParseError(msg) => write!(f, "Parse error: {}", msg),
            Self::UnsupportedFormat(fmt) => write!(f, "Unsupported format: {}", fmt),
            Self::DependencyFailed(id) => write!(f, "Dependency failed: {:?}", id),
            Self::Custom(msg) => write!(f, "{}", msg),
        }
    }
}

/// Result type for asset loading
pub type LoadResult<T> = Result<T, LoadError>;

/// Context provided to loaders during loading
pub struct LoadContext<'a> {
    /// Path of the asset being loaded
    pub path: &'a str,
    /// Raw asset data
    pub data: &'a [u8],
    /// Asset ID being loaded
    pub id: AssetId,
    /// Dependencies discovered during loading
    pub dependencies: Vec<AssetId>,
    /// Loader registry for loading sub-assets
    loader_registry: Option<&'a LoaderRegistry>,
}

impl<'a> LoadContext<'a> {
    /// Create a new load context
    pub fn new(path: &'a str, data: &'a [u8], id: AssetId) -> Self {
        Self {
            path,
            data,
            id,
            dependencies: Vec::new(),
            loader_registry: None,
        }
    }

    /// Set the loader registry
    pub fn with_registry(mut self, registry: &'a LoaderRegistry) -> Self {
        self.loader_registry = Some(registry);
        self
    }

    /// Get file extension
    pub fn extension(&self) -> Option<&str> {
        self.path.rsplit('.').next()
    }

    /// Add a dependency
    pub fn add_dependency(&mut self, id: AssetId) {
        if !self.dependencies.contains(&id) {
            self.dependencies.push(id);
        }
    }

    /// Read data as string (UTF-8)
    pub fn read_string(&self) -> LoadResult<&str> {
        core::str::from_utf8(self.data)
            .map_err(|e| LoadError::ParseError(alloc::format!("Invalid UTF-8: {}", e)))
    }
}

/// Trait for asset loaders
pub trait AssetLoader: Send + Sync {
    /// Asset type this loader produces
    type Asset: Send + Sync + 'static;

    /// File extensions this loader handles
    fn extensions(&self) -> &[&str];

    /// Load an asset from raw data
    fn load(&self, ctx: &mut LoadContext) -> LoadResult<Self::Asset>;

    /// Get the type ID of the asset
    fn asset_type_id(&self) -> TypeId {
        TypeId::of::<Self::Asset>()
    }

    /// Get the type name of the asset
    fn asset_type_name(&self) -> &'static str {
        core::any::type_name::<Self::Asset>()
    }
}

/// Type-erased asset loader
pub trait ErasedLoader: Send + Sync {
    /// File extensions this loader handles
    fn extensions(&self) -> &[&str];

    /// Load an asset into a boxed Any
    fn load_erased(&self, ctx: &mut LoadContext) -> LoadResult<Box<dyn Any + Send + Sync>>;

    /// Get the asset type ID
    fn asset_type_id(&self) -> TypeId;

    /// Get the asset type name
    fn asset_type_name(&self) -> &'static str;
}

impl<L: AssetLoader> ErasedLoader for L {
    fn extensions(&self) -> &[&str] {
        AssetLoader::extensions(self)
    }

    fn load_erased(&self, ctx: &mut LoadContext) -> LoadResult<Box<dyn Any + Send + Sync>> {
        self.load(ctx).map(|asset| Box::new(asset) as Box<dyn Any + Send + Sync>)
    }

    fn asset_type_id(&self) -> TypeId {
        AssetLoader::asset_type_id(self)
    }

    fn asset_type_name(&self) -> &'static str {
        AssetLoader::asset_type_name(self)
    }
}

/// Registry of asset loaders
pub struct LoaderRegistry {
    /// Loaders by extension -> list of indices into all_loaders
    by_extension: BTreeMap<String, Vec<usize>>,
    /// Loaders by asset type -> list of indices into all_loaders
    by_type: BTreeMap<TypeId, Vec<usize>>,
    /// All registered loaders
    all_loaders: Vec<Box<dyn ErasedLoader>>,
}

impl LoaderRegistry {
    /// Create a new loader registry
    pub fn new() -> Self {
        Self {
            by_extension: BTreeMap::new(),
            by_type: BTreeMap::new(),
            all_loaders: Vec::new(),
        }
    }

    /// Register a loader
    pub fn register<L: AssetLoader + 'static>(&mut self, loader: L) {
        let type_id = loader.asset_type_id();
        let boxed: Box<dyn ErasedLoader> = Box::new(loader);
        let idx = self.all_loaders.len();

        // Register by extension
        for &ext in boxed.extensions() {
            self.by_extension
                .entry(ext.to_lowercase())
                .or_default()
                .push(idx);
        }

        // Register by type
        self.by_type
            .entry(type_id)
            .or_default()
            .push(idx);

        self.all_loaders.push(boxed);
    }

    /// Register an erased loader
    pub fn register_erased(&mut self, loader: Box<dyn ErasedLoader>) {
        let type_id = loader.asset_type_id();
        let idx = self.all_loaders.len();

        for &ext in loader.extensions() {
            self.by_extension
                .entry(ext.to_lowercase())
                .or_default()
                .push(idx);
        }

        self.by_type
            .entry(type_id)
            .or_default()
            .push(idx);

        self.all_loaders.push(loader);
    }

    /// Get loader indices for an extension
    pub fn loader_indices_for_extension(&self, ext: &str) -> Option<&[usize]> {
        self.by_extension.get(&ext.to_lowercase()).map(|v| v.as_slice())
    }

    /// Get loader indices for a type
    pub fn loader_indices_for_type(&self, type_id: TypeId) -> Option<&[usize]> {
        self.by_type.get(&type_id).map(|v| v.as_slice())
    }

    /// Get a loader by index
    pub fn get_loader(&self, idx: usize) -> Option<&dyn ErasedLoader> {
        self.all_loaders.get(idx).map(|b| b.as_ref())
    }

    /// Load an asset using the appropriate loader
    pub fn load(&self, ctx: &mut LoadContext) -> LoadResult<Box<dyn Any + Send + Sync>> {
        let ext = ctx.extension()
            .ok_or_else(|| LoadError::UnsupportedFormat("No file extension".into()))?
            .to_lowercase();

        let indices = self.loader_indices_for_extension(&ext)
            .ok_or_else(|| LoadError::UnsupportedFormat(ext.clone().into()))?;

        if indices.is_empty() {
            return Err(LoadError::UnsupportedFormat(ext.into()));
        }

        // Try each loader until one succeeds
        let mut last_error = None;
        for &idx in indices {
            if let Some(loader) = self.get_loader(idx) {
                match loader.load_erased(ctx) {
                    Ok(asset) => return Ok(asset),
                    Err(e) => last_error = Some(e),
                }
            }
        }

        Err(last_error.unwrap_or_else(|| LoadError::UnsupportedFormat(ext.into())))
    }

    /// Check if a format is supported
    pub fn supports_extension(&self, ext: &str) -> bool {
        self.by_extension.contains_key(&ext.to_lowercase())
    }

    /// Get all registered extensions
    pub fn extensions(&self) -> impl Iterator<Item = &str> {
        self.by_extension.keys().map(|s| s.as_str())
    }
}

impl Default for LoaderRegistry {
    fn default() -> Self {
        Self::new()
    }
}

// Clone implementation for Box<dyn ErasedLoader> using raw pointers
trait CloneBox {
    fn clone(&self) -> Box<dyn ErasedLoader>;
}

/// Builder for registering loaders with a fluent API
pub struct LoaderBuilder<'a> {
    registry: &'a mut LoaderRegistry,
}

impl<'a> LoaderBuilder<'a> {
    /// Create a new loader builder
    pub fn new(registry: &'a mut LoaderRegistry) -> Self {
        Self { registry }
    }

    /// Register a loader
    pub fn add<L: AssetLoader + 'static>(self, loader: L) -> Self {
        self.registry.register(loader);
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TextAsset(String);

    struct TextLoader;

    impl AssetLoader for TextLoader {
        type Asset = TextAsset;

        fn extensions(&self) -> &[&str] {
            &["txt", "text"]
        }

        fn load(&self, ctx: &mut LoadContext) -> LoadResult<Self::Asset> {
            let text = ctx.read_string()?;
            Ok(TextAsset(text.to_string()))
        }
    }

    #[test]
    fn test_loader_registry() {
        let mut registry = LoaderRegistry::new();
        registry.register(TextLoader);

        assert!(registry.supports_extension("txt"));
        assert!(registry.supports_extension("TXT")); // Case insensitive
        assert!(registry.supports_extension("text"));
        assert!(!registry.supports_extension("png"));
    }

    #[test]
    fn test_load_text_asset() {
        let mut registry = LoaderRegistry::new();
        registry.register(TextLoader);

        let data = b"Hello, World!";
        let mut ctx = LoadContext::new("test.txt", data, AssetId::new(1));

        let result = registry.load(&mut ctx);
        assert!(result.is_ok());

        let asset = result.unwrap();
        let text = asset.downcast_ref::<TextAsset>().unwrap();
        assert_eq!(text.0, "Hello, World!");
    }
}
