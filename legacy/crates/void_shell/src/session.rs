//! Shell session management
//!
//! Manages shell sessions with their own history, environment, and state.

use std::collections::HashMap;
use serde::{Serialize, Deserialize};

use crate::history::History;

/// Session identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct SessionId(u64);

impl SessionId {
    /// Create a new session ID
    pub fn new(id: u64) -> Self {
        Self(id)
    }

    /// Get raw ID value
    pub fn raw(&self) -> u64 {
        self.0
    }
}

/// Session configuration
#[derive(Debug, Clone)]
pub struct SessionConfig {
    /// Enable command history
    pub history_enabled: bool,
    /// Maximum history entries
    pub max_history: usize,
    /// Initial working directory
    pub working_dir: String,
    /// Initial environment variables
    pub initial_env: HashMap<String, String>,
}

impl Default for SessionConfig {
    fn default() -> Self {
        Self {
            history_enabled: true,
            max_history: 1000,
            working_dir: "/".to_string(),
            initial_env: HashMap::new(),
        }
    }
}

/// Shell session state
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SessionState {
    /// Session is active
    Active,
    /// Session is suspended
    Suspended,
    /// Session is closing
    Closing,
    /// Session is closed
    Closed,
}

/// A shell session
#[derive(Clone)]
pub struct Session {
    /// Session ID
    id: SessionId,
    /// Session configuration
    config: SessionConfig,
    /// Current state
    state: SessionState,
    /// Command history
    history: History,
    /// Environment variables
    environment: HashMap<String, String>,
    /// Working directory
    working_dir: String,
    /// Session variables
    variables: HashMap<String, String>,
    /// Creation timestamp
    created_at: std::time::Instant,
}

impl Session {
    /// Create a new session
    pub fn new(id: SessionId, config: SessionConfig) -> Self {
        let working_dir = config.working_dir.clone();
        let environment = config.initial_env.clone();
        let max_history = config.max_history;

        Self {
            id,
            config,
            state: SessionState::Active,
            history: History::new(max_history),
            environment,
            working_dir,
            variables: HashMap::new(),
            created_at: std::time::Instant::now(),
        }
    }

    /// Get session ID
    pub fn id(&self) -> SessionId {
        self.id
    }

    /// Get session state
    pub fn state(&self) -> SessionState {
        self.state
    }

    /// Set session state
    pub fn set_state(&mut self, state: SessionState) {
        self.state = state;
    }

    /// Get history (immutable)
    pub fn history(&self) -> &History {
        &self.history
    }

    /// Get history (mutable)
    pub fn history_mut(&mut self) -> &mut History {
        &mut self.history
    }

    /// Get environment variables
    pub fn environment(&self) -> &HashMap<String, String> {
        &self.environment
    }

    /// Get environment variable
    pub fn get_env(&self, key: &str) -> Option<&str> {
        self.environment.get(key).map(|s| s.as_str())
    }

    /// Set environment variable
    pub fn set_env(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.environment.insert(key.into(), value.into());
    }

    /// Remove environment variable
    pub fn unset_env(&mut self, key: &str) -> Option<String> {
        self.environment.remove(key)
    }

    /// Get working directory
    pub fn working_dir(&self) -> &str {
        &self.working_dir
    }

    /// Set working directory
    pub fn set_working_dir(&mut self, dir: impl Into<String>) {
        self.working_dir = dir.into();
    }

    /// Get session variable
    pub fn get_var(&self, key: &str) -> Option<&str> {
        self.variables.get(key).map(|s| s.as_str())
    }

    /// Set session variable
    pub fn set_var(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.variables.insert(key.into(), value.into());
    }

    /// Remove session variable
    pub fn unset_var(&mut self, key: &str) -> Option<String> {
        self.variables.remove(key)
    }

    /// Get session uptime
    pub fn uptime(&self) -> std::time::Duration {
        self.created_at.elapsed()
    }

    /// Check if session is active
    pub fn is_active(&self) -> bool {
        self.state == SessionState::Active
    }

    /// Suspend the session
    pub fn suspend(&mut self) {
        self.state = SessionState::Suspended;
    }

    /// Resume the session
    pub fn resume(&mut self) {
        if self.state == SessionState::Suspended {
            self.state = SessionState::Active;
        }
    }

    /// Close the session
    pub fn close(&mut self) {
        self.state = SessionState::Closing;
        // Cleanup would happen here
        self.state = SessionState::Closed;
    }
}

impl std::fmt::Debug for Session {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Session")
            .field("id", &self.id)
            .field("state", &self.state)
            .field("working_dir", &self.working_dir)
            .field("env_count", &self.environment.len())
            .field("history_size", &self.history.len())
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_session_creation() {
        let id = SessionId::new(1);
        let session = Session::new(id, SessionConfig::default());

        assert_eq!(session.id(), id);
        assert!(session.is_active());
        assert_eq!(session.working_dir(), "/");
    }

    #[test]
    fn test_session_environment() {
        let id = SessionId::new(1);
        let mut session = Session::new(id, SessionConfig::default());

        session.set_env("PATH", "/usr/bin");
        assert_eq!(session.get_env("PATH"), Some("/usr/bin"));

        session.unset_env("PATH");
        assert!(session.get_env("PATH").is_none());
    }

    #[test]
    fn test_session_state_transitions() {
        let id = SessionId::new(1);
        let mut session = Session::new(id, SessionConfig::default());

        assert_eq!(session.state(), SessionState::Active);

        session.suspend();
        assert_eq!(session.state(), SessionState::Suspended);

        session.resume();
        assert_eq!(session.state(), SessionState::Active);

        session.close();
        assert_eq!(session.state(), SessionState::Closed);
    }

    #[test]
    fn test_session_variables() {
        let id = SessionId::new(1);
        let mut session = Session::new(id, SessionConfig::default());

        session.set_var("MY_VAR", "value");
        assert_eq!(session.get_var("MY_VAR"), Some("value"));
    }
}
