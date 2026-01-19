//! # Void Services
//!
//! System services layer for the Metaverse OS.
//!
//! Provides core services that apps can use:
//! - Asset loading and caching
//! - Session management
//! - Audio playback
//! - Network communication
//!
//! ## Service Architecture
//!
//! Services are managed by the ServiceRegistry which handles:
//! - Service lifecycle (start/stop/restart)
//! - Health monitoring
//! - Inter-service communication via channels
//!
//! ## Usage
//!
//! ```ignore
//! let mut registry = ServiceRegistry::new();
//!
//! // Register services
//! registry.register("assets", Box::new(AssetService::new()));
//! registry.register("sessions", Box::new(SessionService::new()));
//!
//! // Start all services
//! registry.start_all().await?;
//!
//! // Get service handle
//! if let Some(assets) = registry.get::<AssetService>("assets") {
//!     let handle = assets.load("scene.json").await?;
//! }
//! ```

pub mod service;
pub mod registry;
pub mod asset;
pub mod session;
pub mod audio;
pub mod event_bus;
pub mod network;

#[cfg(feature = "audio-backend")]
pub mod audio_backend;

pub use service::{Service, ServiceId, ServiceState, ServiceError, ServiceResult};
pub use registry::ServiceRegistry;
pub use asset::{AssetService, AssetHandle, AssetState, AssetError};
pub use session::{SessionService, UserSession, SessionId, SessionError};
pub use audio::{AudioService, AudioHandle, AudioState};
pub use event_bus::{EventBus, Event, EventHandler};

#[cfg(feature = "audio-backend")]
pub use audio_backend::AudioBackend;

use thiserror::Error;

/// Service layer errors
#[derive(Debug, Error)]
pub enum ServicesError {
    #[error("Service error: {0}")]
    Service(#[from] ServiceError),

    #[error("Asset error: {0}")]
    Asset(#[from] AssetError),

    #[error("Session error: {0}")]
    Session(#[from] SessionError),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("Channel error")]
    Channel,
}

pub type ServicesResult<T> = Result<T, ServicesError>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_services_import() {
        // Verify types can be imported
        let _id: ServiceId = ServiceId::new("test");
        let _state = ServiceState::Stopped;
    }
}
