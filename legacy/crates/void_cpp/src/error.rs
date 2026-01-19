//! Error types for the C++ scripting system

use std::path::PathBuf;
use thiserror::Error;

/// Result type for C++ operations
pub type Result<T> = std::result::Result<T, CppError>;

/// Errors that can occur in the C++ scripting system
#[derive(Debug, Error)]
pub enum CppError {
    /// Failed to load dynamic library
    #[error("Failed to load library '{path}': {message}")]
    LoadError {
        path: PathBuf,
        message: String,
    },

    /// Library does not contain required symbol
    #[error("Symbol '{symbol}' not found in library '{library}'")]
    SymbolNotFound {
        library: String,
        symbol: String,
    },

    /// Class not found in registry
    #[error("Class '{0}' not registered")]
    ClassNotFound(String),

    /// Instance not found
    #[error("Instance {0:?} not found")]
    InstanceNotFound(crate::instance::InstanceId),

    /// Failed to create instance
    #[error("Failed to create instance of '{class_name}': {message}")]
    InstanceCreationFailed {
        class_name: String,
        message: String,
    },

    /// Invalid library format
    #[error("Invalid library format: {0}")]
    InvalidLibraryFormat(String),

    /// Hot-reload error
    #[error("Hot-reload failed: {0}")]
    HotReloadFailed(String),

    /// Serialization error
    #[error("Serialization failed: {0}")]
    SerializationError(String),

    /// Deserialization error
    #[error("Deserialization failed: {0}")]
    DeserializationError(String),

    /// Property error
    #[error("Property error: {0}")]
    PropertyError(String),

    /// FFI call failed
    #[error("FFI call failed: {0}")]
    FfiCallFailed(String),

    /// Invalid state
    #[error("Invalid state: {0}")]
    InvalidState(String),

    /// IO error
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),

    /// Library already loaded
    #[error("Library '{0}' is already loaded")]
    LibraryAlreadyLoaded(PathBuf),

    /// Version mismatch
    #[error("Version mismatch: library version {library_version}, expected {expected_version}")]
    VersionMismatch {
        library_version: String,
        expected_version: String,
    },
}

impl CppError {
    /// Create a load error
    pub fn load_error(path: impl Into<PathBuf>, message: impl Into<String>) -> Self {
        CppError::LoadError {
            path: path.into(),
            message: message.into(),
        }
    }

    /// Create a symbol not found error
    pub fn symbol_not_found(library: impl Into<String>, symbol: impl Into<String>) -> Self {
        CppError::SymbolNotFound {
            library: library.into(),
            symbol: symbol.into(),
        }
    }

    /// Create an instance creation failed error
    pub fn instance_creation_failed(class_name: impl Into<String>, message: impl Into<String>) -> Self {
        CppError::InstanceCreationFailed {
            class_name: class_name.into(),
            message: message.into(),
        }
    }
}
