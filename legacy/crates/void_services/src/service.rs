//! Service trait and core types
//!
//! Defines the base Service trait that all system services implement.

use std::any::Any;
use std::fmt;
use thiserror::Error;
use serde::{Serialize, Deserialize};

/// Service identifier
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ServiceId(String);

impl ServiceId {
    /// Create a new service ID
    pub fn new(name: impl Into<String>) -> Self {
        Self(name.into())
    }

    /// Get the service name
    pub fn name(&self) -> &str {
        &self.0
    }
}

impl fmt::Display for ServiceId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Service lifecycle state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ServiceState {
    /// Service is not running
    Stopped,
    /// Service is starting up
    Starting,
    /// Service is running
    Running,
    /// Service is stopping
    Stopping,
    /// Service has failed
    Failed,
    /// Service is degraded but operational
    Degraded,
}

impl ServiceState {
    /// Check if service is operational (Running or Degraded)
    pub fn is_operational(&self) -> bool {
        matches!(self, Self::Running | Self::Degraded)
    }

    /// Check if service is in transition
    pub fn is_transitioning(&self) -> bool {
        matches!(self, Self::Starting | Self::Stopping)
    }
}

/// Service health status
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceHealth {
    /// Current state
    pub state: ServiceState,
    /// Health score (0.0 = dead, 1.0 = perfect)
    pub health_score: f32,
    /// Last error message if any
    pub last_error: Option<String>,
    /// Time since last successful operation (ms)
    pub last_success_ms: Option<u64>,
    /// Additional metrics
    pub metrics: std::collections::HashMap<String, f64>,
}

impl Default for ServiceHealth {
    fn default() -> Self {
        Self {
            state: ServiceState::Stopped,
            health_score: 0.0,
            last_error: None,
            last_success_ms: None,
            metrics: std::collections::HashMap::new(),
        }
    }
}

impl ServiceHealth {
    /// Create a healthy status
    pub fn healthy() -> Self {
        Self {
            state: ServiceState::Running,
            health_score: 1.0,
            ..Default::default()
        }
    }

    /// Create a degraded status
    pub fn degraded(reason: impl Into<String>) -> Self {
        Self {
            state: ServiceState::Degraded,
            health_score: 0.5,
            last_error: Some(reason.into()),
            ..Default::default()
        }
    }

    /// Create a failed status
    pub fn failed(error: impl Into<String>) -> Self {
        Self {
            state: ServiceState::Failed,
            health_score: 0.0,
            last_error: Some(error.into()),
            ..Default::default()
        }
    }
}

/// Service errors
#[derive(Debug, Error)]
pub enum ServiceError {
    #[error("Service not found: {0}")]
    NotFound(String),

    #[error("Service already exists: {0}")]
    AlreadyExists(String),

    #[error("Service not running: {0}")]
    NotRunning(String),

    #[error("Service start failed: {0}")]
    StartFailed(String),

    #[error("Service stop failed: {0}")]
    StopFailed(String),

    #[error("Service operation timeout")]
    Timeout,

    #[error("Invalid state transition: {from:?} -> {to:?}")]
    InvalidTransition { from: ServiceState, to: ServiceState },

    #[error("Internal error: {0}")]
    Internal(String),
}

pub type ServiceResult<T> = Result<T, ServiceError>;

/// Service configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceConfig {
    /// Service ID
    pub id: ServiceId,
    /// Auto-restart on failure
    pub auto_restart: bool,
    /// Maximum restart attempts
    pub max_restarts: u32,
    /// Restart delay in milliseconds
    pub restart_delay_ms: u64,
    /// Health check interval in milliseconds
    pub health_check_interval_ms: u64,
    /// Startup timeout in milliseconds
    pub startup_timeout_ms: u64,
    /// Shutdown timeout in milliseconds
    pub shutdown_timeout_ms: u64,
}

impl Default for ServiceConfig {
    fn default() -> Self {
        Self {
            id: ServiceId::new("default"),
            auto_restart: true,
            max_restarts: 3,
            restart_delay_ms: 1000,
            health_check_interval_ms: 5000,
            startup_timeout_ms: 30000,
            shutdown_timeout_ms: 10000,
        }
    }
}

impl ServiceConfig {
    /// Create a new config with the given ID
    pub fn new(id: impl Into<String>) -> Self {
        Self {
            id: ServiceId::new(id),
            ..Default::default()
        }
    }
}

/// Base trait for all services
pub trait Service: Send + Sync {
    /// Get the service ID
    fn id(&self) -> &ServiceId;

    /// Get current state
    fn state(&self) -> ServiceState;

    /// Get health status
    fn health(&self) -> ServiceHealth;

    /// Get configuration
    fn config(&self) -> &ServiceConfig;

    /// Start the service
    fn start(&mut self) -> ServiceResult<()>;

    /// Stop the service
    fn stop(&mut self) -> ServiceResult<()>;

    /// Restart the service
    fn restart(&mut self) -> ServiceResult<()> {
        self.stop()?;
        self.start()
    }

    /// Type erasure for downcasting
    fn as_any(&self) -> &dyn Any;

    /// Mutable type erasure for downcasting
    fn as_any_mut(&mut self) -> &mut dyn Any;
}

/// Service dependency declaration
#[derive(Debug, Clone)]
pub struct ServiceDependency {
    /// ID of the required service
    pub service_id: ServiceId,
    /// Whether this dependency is required (hard) or optional (soft)
    pub required: bool,
}

impl ServiceDependency {
    /// Create a required dependency
    pub fn required(id: impl Into<String>) -> Self {
        Self {
            service_id: ServiceId::new(id),
            required: true,
        }
    }

    /// Create an optional dependency
    pub fn optional(id: impl Into<String>) -> Self {
        Self {
            service_id: ServiceId::new(id),
            required: false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_service_id() {
        let id = ServiceId::new("test-service");
        assert_eq!(id.name(), "test-service");
        assert_eq!(id.to_string(), "test-service");
    }

    #[test]
    fn test_service_state() {
        assert!(ServiceState::Running.is_operational());
        assert!(ServiceState::Degraded.is_operational());
        assert!(!ServiceState::Stopped.is_operational());
        assert!(ServiceState::Starting.is_transitioning());
        assert!(!ServiceState::Running.is_transitioning());
    }

    #[test]
    fn test_service_health() {
        let healthy = ServiceHealth::healthy();
        assert_eq!(healthy.state, ServiceState::Running);
        assert!((healthy.health_score - 1.0).abs() < 0.001);

        let degraded = ServiceHealth::degraded("test reason");
        assert_eq!(degraded.state, ServiceState::Degraded);
        assert!(degraded.last_error.is_some());

        let failed = ServiceHealth::failed("test error");
        assert_eq!(failed.state, ServiceState::Failed);
        assert!((failed.health_score - 0.0).abs() < 0.001);
    }

    #[test]
    fn test_service_config() {
        let config = ServiceConfig::new("my-service");
        assert_eq!(config.id.name(), "my-service");
        assert!(config.auto_restart);
        assert_eq!(config.max_restarts, 3);
    }

    #[test]
    fn test_service_dependency() {
        let required = ServiceDependency::required("database");
        assert!(required.required);
        assert_eq!(required.service_id.name(), "database");

        let optional = ServiceDependency::optional("cache");
        assert!(!optional.required);
    }
}
