//! Session service
//!
//! Manages user sessions, authentication, and session state.

use std::any::Any;
use std::collections::HashMap;
use std::time::{Duration, Instant};
use thiserror::Error;
use serde::{Serialize, Deserialize};

use crate::service::{Service, ServiceId, ServiceState, ServiceHealth, ServiceConfig, ServiceResult};

/// Session identifier
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct SessionId(String);

impl SessionId {
    /// Create a new session ID
    pub fn new(id: impl Into<String>) -> Self {
        Self(id.into())
    }

    /// Generate a random session ID
    pub fn generate() -> Self {
        use std::time::SystemTime;
        let nanos = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();
        Self(format!("session_{:x}", nanos))
    }

    /// Get the session ID string
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl std::fmt::Display for SessionId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Session state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SessionState {
    /// Session created but not active
    Created,
    /// Session is active
    Active,
    /// Session is suspended
    Suspended,
    /// Session is expired
    Expired,
    /// Session is terminated
    Terminated,
}

/// User session data
#[derive(Debug, Clone)]
pub struct UserSession {
    /// Session ID
    pub id: SessionId,
    /// User ID (if authenticated)
    pub user_id: Option<String>,
    /// Display name
    pub display_name: String,
    /// Session state
    pub state: SessionState,
    /// Creation time
    pub created_at: Instant,
    /// Last activity time
    pub last_activity: Instant,
    /// Session variables
    pub variables: HashMap<String, String>,
    /// Permissions/capabilities
    pub permissions: Vec<String>,
    /// Metadata
    pub metadata: HashMap<String, String>,
}

impl UserSession {
    /// Create a new session
    pub fn new(display_name: impl Into<String>) -> Self {
        let now = Instant::now();
        Self {
            id: SessionId::generate(),
            user_id: None,
            display_name: display_name.into(),
            state: SessionState::Created,
            created_at: now,
            last_activity: now,
            variables: HashMap::new(),
            permissions: Vec::new(),
            metadata: HashMap::new(),
        }
    }

    /// Create an authenticated session
    pub fn authenticated(user_id: impl Into<String>, display_name: impl Into<String>) -> Self {
        let mut session = Self::new(display_name);
        session.user_id = Some(user_id.into());
        session
    }

    /// Activate the session
    pub fn activate(&mut self) {
        self.state = SessionState::Active;
        self.touch();
    }

    /// Suspend the session
    pub fn suspend(&mut self) {
        self.state = SessionState::Suspended;
    }

    /// Resume a suspended session
    pub fn resume(&mut self) {
        if self.state == SessionState::Suspended {
            self.state = SessionState::Active;
            self.touch();
        }
    }

    /// Terminate the session
    pub fn terminate(&mut self) {
        self.state = SessionState::Terminated;
    }

    /// Update last activity time
    pub fn touch(&mut self) {
        self.last_activity = Instant::now();
    }

    /// Check if session is active
    pub fn is_active(&self) -> bool {
        self.state == SessionState::Active
    }

    /// Check if session is expired
    pub fn is_expired(&self, timeout: Duration) -> bool {
        self.last_activity.elapsed() > timeout
    }

    /// Get session age
    pub fn age(&self) -> Duration {
        self.created_at.elapsed()
    }

    /// Get idle time
    pub fn idle_time(&self) -> Duration {
        self.last_activity.elapsed()
    }

    /// Set a session variable
    pub fn set_var(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.variables.insert(key.into(), value.into());
    }

    /// Get a session variable
    pub fn get_var(&self, key: &str) -> Option<&str> {
        self.variables.get(key).map(|s| s.as_str())
    }

    /// Check if session has a permission
    pub fn has_permission(&self, permission: &str) -> bool {
        self.permissions.iter().any(|p| p == permission || p == "*")
    }

    /// Grant a permission
    pub fn grant_permission(&mut self, permission: impl Into<String>) {
        let perm = permission.into();
        if !self.permissions.contains(&perm) {
            self.permissions.push(perm);
        }
    }

    /// Revoke a permission
    pub fn revoke_permission(&mut self, permission: &str) {
        self.permissions.retain(|p| p != permission);
    }
}

/// Session errors
#[derive(Debug, Error)]
pub enum SessionError {
    #[error("Session not found: {0}")]
    NotFound(String),

    #[error("Session expired: {0}")]
    Expired(String),

    #[error("Session already exists: {0}")]
    AlreadyExists(String),

    #[error("Authentication failed: {0}")]
    AuthFailed(String),

    #[error("Permission denied: {0}")]
    PermissionDenied(String),

    #[error("Invalid state transition")]
    InvalidState,
}

/// Session service configuration
#[derive(Debug, Clone)]
pub struct SessionServiceConfig {
    /// Base service config
    pub service: ServiceConfig,
    /// Session timeout duration
    pub session_timeout: Duration,
    /// Maximum concurrent sessions
    pub max_sessions: usize,
    /// Allow anonymous sessions
    pub allow_anonymous: bool,
    /// Session cleanup interval
    pub cleanup_interval: Duration,
}

impl Default for SessionServiceConfig {
    fn default() -> Self {
        Self {
            service: ServiceConfig::new("sessions"),
            session_timeout: Duration::from_secs(3600), // 1 hour
            max_sessions: 1000,
            allow_anonymous: true,
            cleanup_interval: Duration::from_secs(60),
        }
    }
}

/// Session service
pub struct SessionService {
    /// Configuration
    config: SessionServiceConfig,
    /// Current state
    state: ServiceState,
    /// Active sessions
    sessions: HashMap<SessionId, UserSession>,
    /// User ID to session mapping (for authenticated users)
    user_sessions: HashMap<String, SessionId>,
    /// Statistics
    stats: SessionStats,
}

/// Session statistics
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct SessionStats {
    /// Total sessions created
    pub total_created: u64,
    /// Total sessions terminated
    pub total_terminated: u64,
    /// Total sessions expired
    pub total_expired: u64,
    /// Peak concurrent sessions
    pub peak_sessions: usize,
}

impl SessionService {
    /// Create a new session service
    pub fn new(config: SessionServiceConfig) -> Self {
        Self {
            config,
            state: ServiceState::Stopped,
            sessions: HashMap::new(),
            user_sessions: HashMap::new(),
            stats: SessionStats::default(),
        }
    }

    /// Create with default configuration
    pub fn with_defaults() -> Self {
        Self::new(SessionServiceConfig::default())
    }

    /// Create a new session
    pub fn create_session(&mut self, display_name: impl Into<String>) -> Result<SessionId, SessionError> {
        if self.sessions.len() >= self.config.max_sessions {
            // Try to cleanup expired sessions first
            self.cleanup_expired();
            if self.sessions.len() >= self.config.max_sessions {
                return Err(SessionError::AuthFailed("Max sessions reached".to_string()));
            }
        }

        let mut session = UserSession::new(display_name);
        session.activate();

        let id = session.id.clone();
        self.sessions.insert(id.clone(), session);

        self.stats.total_created += 1;
        if self.sessions.len() > self.stats.peak_sessions {
            self.stats.peak_sessions = self.sessions.len();
        }

        Ok(id)
    }

    /// Create an authenticated session
    pub fn create_authenticated_session(
        &mut self,
        user_id: impl Into<String>,
        display_name: impl Into<String>,
    ) -> Result<SessionId, SessionError> {
        let user_id = user_id.into();

        // Check if user already has a session
        if let Some(existing_id) = self.user_sessions.get(&user_id).cloned() {
            // Terminate old session
            self.terminate_session(&existing_id)?;
        }

        if self.sessions.len() >= self.config.max_sessions {
            self.cleanup_expired();
            if self.sessions.len() >= self.config.max_sessions {
                return Err(SessionError::AuthFailed("Max sessions reached".to_string()));
            }
        }

        let mut session = UserSession::authenticated(&user_id, display_name);
        session.activate();

        let id = session.id.clone();
        self.sessions.insert(id.clone(), session);
        self.user_sessions.insert(user_id, id.clone());

        self.stats.total_created += 1;
        if self.sessions.len() > self.stats.peak_sessions {
            self.stats.peak_sessions = self.sessions.len();
        }

        Ok(id)
    }

    /// Get a session
    pub fn get_session(&self, id: &SessionId) -> Option<&UserSession> {
        self.sessions.get(id)
    }

    /// Get a mutable session
    pub fn get_session_mut(&mut self, id: &SessionId) -> Option<&mut UserSession> {
        self.sessions.get_mut(id)
    }

    /// Get session by user ID
    pub fn get_user_session(&self, user_id: &str) -> Option<&UserSession> {
        self.user_sessions.get(user_id)
            .and_then(|sid| self.sessions.get(sid))
    }

    /// Touch a session (update last activity)
    pub fn touch_session(&mut self, id: &SessionId) -> Result<(), SessionError> {
        let session = self.sessions.get_mut(id)
            .ok_or_else(|| SessionError::NotFound(id.to_string()))?;

        if session.is_expired(self.config.session_timeout) {
            session.state = SessionState::Expired;
            return Err(SessionError::Expired(id.to_string()));
        }

        session.touch();
        Ok(())
    }

    /// Terminate a session
    pub fn terminate_session(&mut self, id: &SessionId) -> Result<(), SessionError> {
        let session = self.sessions.remove(id)
            .ok_or_else(|| SessionError::NotFound(id.to_string()))?;

        if let Some(user_id) = &session.user_id {
            self.user_sessions.remove(user_id);
        }

        self.stats.total_terminated += 1;
        Ok(())
    }

    /// Cleanup expired sessions
    pub fn cleanup_expired(&mut self) -> usize {
        let timeout = self.config.session_timeout;
        let expired: Vec<SessionId> = self.sessions.iter()
            .filter(|(_, s)| s.is_expired(timeout))
            .map(|(id, _)| id.clone())
            .collect();

        let count = expired.len();
        for id in expired {
            if let Some(session) = self.sessions.remove(&id) {
                if let Some(user_id) = &session.user_id {
                    self.user_sessions.remove(user_id);
                }
                self.stats.total_expired += 1;
            }
        }

        count
    }

    /// Get active session count
    pub fn active_count(&self) -> usize {
        self.sessions.values().filter(|s| s.is_active()).count()
    }

    /// Get total session count
    pub fn total_count(&self) -> usize {
        self.sessions.len()
    }

    /// Get statistics
    pub fn stats(&self) -> &SessionStats {
        &self.stats
    }

    /// List all session IDs
    pub fn list_sessions(&self) -> Vec<SessionId> {
        self.sessions.keys().cloned().collect()
    }
}

impl Service for SessionService {
    fn id(&self) -> &ServiceId {
        &self.config.service.id
    }

    fn state(&self) -> ServiceState {
        self.state
    }

    fn health(&self) -> ServiceHealth {
        if self.state == ServiceState::Running {
            let mut health = ServiceHealth::healthy();
            health.metrics.insert("active_sessions".to_string(), self.active_count() as f64);
            health.metrics.insert("total_sessions".to_string(), self.total_count() as f64);
            health.metrics.insert("capacity_used".to_string(),
                self.total_count() as f64 / self.config.max_sessions as f64);
            health
        } else {
            ServiceHealth::default()
        }
    }

    fn config(&self) -> &ServiceConfig {
        &self.config.service
    }

    fn start(&mut self) -> ServiceResult<()> {
        self.state = ServiceState::Running;
        Ok(())
    }

    fn stop(&mut self) -> ServiceResult<()> {
        // Terminate all sessions
        let ids: Vec<SessionId> = self.sessions.keys().cloned().collect();
        for id in ids {
            let _ = self.terminate_session(&id);
        }
        self.state = ServiceState::Stopped;
        Ok(())
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_session_id() {
        let id = SessionId::generate();
        assert!(id.as_str().starts_with("session_"));

        let id2 = SessionId::new("custom");
        assert_eq!(id2.as_str(), "custom");
    }

    #[test]
    fn test_create_session() {
        let mut service = SessionService::with_defaults();
        service.start().unwrap();

        let id = service.create_session("test user").unwrap();
        assert!(service.get_session(&id).is_some());

        let session = service.get_session(&id).unwrap();
        assert_eq!(session.display_name, "test user");
        assert!(session.is_active());
    }

    #[test]
    fn test_authenticated_session() {
        let mut service = SessionService::with_defaults();
        service.start().unwrap();

        let id = service.create_authenticated_session("user123", "John").unwrap();
        let session = service.get_session(&id).unwrap();

        assert_eq!(session.user_id, Some("user123".to_string()));
        assert_eq!(session.display_name, "John");

        // Get by user ID
        let session2 = service.get_user_session("user123").unwrap();
        assert_eq!(session2.id, id);
    }

    #[test]
    fn test_session_variables() {
        let mut session = UserSession::new("test");
        session.set_var("theme", "dark");
        assert_eq!(session.get_var("theme"), Some("dark"));
        assert_eq!(session.get_var("unknown"), None);
    }

    #[test]
    fn test_session_permissions() {
        let mut session = UserSession::new("test");

        assert!(!session.has_permission("admin"));

        session.grant_permission("admin");
        assert!(session.has_permission("admin"));

        session.revoke_permission("admin");
        assert!(!session.has_permission("admin"));

        // Wildcard permission
        session.grant_permission("*");
        assert!(session.has_permission("anything"));
    }

    #[test]
    fn test_terminate_session() {
        let mut service = SessionService::with_defaults();
        service.start().unwrap();

        let id = service.create_session("test").unwrap();
        assert_eq!(service.total_count(), 1);

        service.terminate_session(&id).unwrap();
        assert_eq!(service.total_count(), 0);
        assert!(service.get_session(&id).is_none());
    }

    #[test]
    fn test_session_timeout() {
        let config = SessionServiceConfig {
            session_timeout: Duration::from_millis(1),
            ..Default::default()
        };

        let mut service = SessionService::new(config);
        service.start().unwrap();

        let id = service.create_session("test").unwrap();

        // Wait for expiry
        std::thread::sleep(Duration::from_millis(10));

        // Touch should fail due to expiry
        let result = service.touch_session(&id);
        assert!(matches!(result, Err(SessionError::Expired(_))));
    }

    #[test]
    fn test_cleanup_expired() {
        let config = SessionServiceConfig {
            session_timeout: Duration::from_millis(1),
            ..Default::default()
        };

        let mut service = SessionService::new(config);
        service.start().unwrap();

        service.create_session("test1").unwrap();
        service.create_session("test2").unwrap();

        std::thread::sleep(Duration::from_millis(10));

        let cleaned = service.cleanup_expired();
        assert_eq!(cleaned, 2);
        assert_eq!(service.total_count(), 0);
    }

    #[test]
    fn test_replace_user_session() {
        let mut service = SessionService::with_defaults();
        service.start().unwrap();

        let id1 = service.create_authenticated_session("user1", "First").unwrap();
        assert_eq!(service.total_count(), 1);

        // Create new session for same user - should replace
        let id2 = service.create_authenticated_session("user1", "Second").unwrap();
        assert_eq!(service.total_count(), 1);
        assert_ne!(id1, id2);

        let session = service.get_user_session("user1").unwrap();
        assert_eq!(session.display_name, "Second");
    }

    #[test]
    fn test_service_lifecycle() {
        let mut service = SessionService::with_defaults();

        assert_eq!(service.state(), ServiceState::Stopped);

        service.start().unwrap();
        assert_eq!(service.state(), ServiceState::Running);

        service.create_session("test").unwrap();
        assert_eq!(service.total_count(), 1);

        service.stop().unwrap();
        assert_eq!(service.state(), ServiceState::Stopped);
        assert_eq!(service.total_count(), 0);
    }
}
