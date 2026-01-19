//! Compositor error types

use thiserror::Error;

/// Compositor errors
#[derive(Debug, Error)]
pub enum CompositorError {
    #[error("No display device found")]
    NoDisplay,

    #[error("No GPU device found")]
    NoGpu,

    #[error("Session error: {0}")]
    Session(String),

    #[error("DRM error: {0}")]
    Drm(String),

    #[error("Input error: {0}")]
    Input(String),

    #[error("Render error: {0}")]
    Render(String),

    #[error("Frame timing error: {0}")]
    FrameTiming(String),

    #[error("Backend not available: {0}")]
    BackendUnavailable(String),

    #[error("Configuration error: {0}")]
    Config(String),

    #[error("Platform not supported")]
    PlatformNotSupported,
}

/// Result type for compositor operations
pub type CompositorResult<T> = Result<T, CompositorError>;
