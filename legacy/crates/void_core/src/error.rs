//! Error types for the core library

use core::fmt;
use alloc::boxed::Box;
use alloc::string::String;

/// The core error type
#[derive(Debug, Clone)]
pub enum Error {
    /// Plugin-related error
    Plugin(PluginError),
    /// Type registry error
    TypeRegistry(TypeRegistryError),
    /// Hot-reload error
    HotReload(HotReloadError),
    /// Handle error
    Handle(HandleError),
    /// Generic error with message
    Message(Box<str>),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Plugin(e) => write!(f, "Plugin error: {}", e),
            Error::TypeRegistry(e) => write!(f, "Type registry error: {}", e),
            Error::HotReload(e) => write!(f, "Hot-reload error: {}", e),
            Error::Handle(e) => write!(f, "Handle error: {}", e),
            Error::Message(msg) => write!(f, "{}", msg),
        }
    }
}

/// Result type alias
pub type Result<T> = core::result::Result<T, Error>;

/// Plugin-specific errors
#[derive(Debug, Clone)]
pub enum PluginError {
    /// Plugin not found
    NotFound(Box<str>),
    /// Plugin already registered
    AlreadyRegistered(Box<str>),
    /// Dependency not satisfied
    MissingDependency { plugin: Box<str>, dependency: Box<str> },
    /// Version mismatch
    VersionMismatch { expected: Box<str>, found: Box<str> },
    /// Plugin initialization failed
    InitFailed(Box<str>),
    /// Plugin is in invalid state for operation
    InvalidState(Box<str>),
}

impl fmt::Display for PluginError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PluginError::NotFound(id) => write!(f, "Plugin not found: {}", id),
            PluginError::AlreadyRegistered(id) => write!(f, "Plugin already registered: {}", id),
            PluginError::MissingDependency { plugin, dependency } => {
                write!(f, "Plugin '{}' requires missing dependency '{}'", plugin, dependency)
            }
            PluginError::VersionMismatch { expected, found } => {
                write!(f, "Version mismatch: expected {}, found {}", expected, found)
            }
            PluginError::InitFailed(msg) => write!(f, "Plugin initialization failed: {}", msg),
            PluginError::InvalidState(msg) => write!(f, "Invalid plugin state: {}", msg),
        }
    }
}

impl From<PluginError> for Error {
    fn from(e: PluginError) -> Self {
        Error::Plugin(e)
    }
}

/// Type registry errors
#[derive(Debug, Clone)]
pub enum TypeRegistryError {
    /// Type not registered
    NotRegistered(Box<str>),
    /// Type already registered
    AlreadyRegistered(Box<str>),
    /// Type mismatch during cast
    TypeMismatch { expected: Box<str>, found: Box<str> },
}

impl fmt::Display for TypeRegistryError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TypeRegistryError::NotRegistered(name) => write!(f, "Type not registered: {}", name),
            TypeRegistryError::AlreadyRegistered(name) => write!(f, "Type already registered: {}", name),
            TypeRegistryError::TypeMismatch { expected, found } => {
                write!(f, "Type mismatch: expected {}, found {}", expected, found)
            }
        }
    }
}

impl From<TypeRegistryError> for Error {
    fn from(e: TypeRegistryError) -> Self {
        Error::TypeRegistry(e)
    }
}

/// Hot-reload errors
#[derive(Debug, Clone)]
pub enum HotReloadError {
    /// Snapshot creation failed
    SnapshotFailed(Box<str>),
    /// Restore from snapshot failed
    RestoreFailed(Box<str>),
    /// Incompatible version for reload
    IncompatibleVersion { old: Box<str>, new: Box<str> },
    /// File watching error
    WatchError(Box<str>),
}

impl fmt::Display for HotReloadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HotReloadError::SnapshotFailed(msg) => write!(f, "Snapshot failed: {}", msg),
            HotReloadError::RestoreFailed(msg) => write!(f, "Restore failed: {}", msg),
            HotReloadError::IncompatibleVersion { old, new } => {
                write!(f, "Incompatible versions: {} -> {}", old, new)
            }
            HotReloadError::WatchError(msg) => write!(f, "Watch error: {}", msg),
        }
    }
}

impl From<HotReloadError> for Error {
    fn from(e: HotReloadError) -> Self {
        Error::HotReload(e)
    }
}

/// Handle errors
#[derive(Debug, Clone)]
pub enum HandleError {
    /// Handle is null
    Null,
    /// Handle is stale (generation mismatch)
    Stale,
    /// Handle index out of bounds
    OutOfBounds,
}

impl fmt::Display for HandleError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HandleError::Null => write!(f, "Handle is null"),
            HandleError::Stale => write!(f, "Handle is stale (already freed)"),
            HandleError::OutOfBounds => write!(f, "Handle index out of bounds"),
        }
    }
}

impl From<HandleError> for Error {
    fn from(e: HandleError) -> Self {
        Error::Handle(e)
    }
}

impl From<&str> for Error {
    fn from(s: &str) -> Self {
        Error::Message(s.into())
    }
}

impl From<String> for Error {
    fn from(s: String) -> Self {
        Error::Message(s.into_boxed_str())
    }
}
