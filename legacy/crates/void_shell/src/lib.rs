//! # Void Shell (vsh)
//!
//! Shell layer for Void Engine providing:
//! - Text-based command interface
//! - Command parsing and execution
//! - Built-in commands
//! - Script execution (VoidScript)
//! - Session management
//!
//! ## Architecture
//!
//! ```text
//! User Input ──► Parser ──► Command ──► Executor ──► Kernel
//!                              │
//!                              ▼
//!                         Built-ins
//! ```
//!
//! ## Usage
//!
//! The shell runs as a REPL (Read-Eval-Print Loop):
//! ```text
//! vsh> help
//! vsh> list apps
//! vsh> spawn "my_app" --layer 1
//! vsh> status
//! ```

pub mod command;
pub mod parser;
pub mod executor;
pub mod builtins;
pub mod session;
pub mod output;
pub mod history;

pub use command::{Command, CommandResult, CommandError};
pub use parser::{Parser, ParseError, Token};
pub use executor::{Executor, ExecutionContext};
pub use session::{Session, SessionId, SessionConfig};
pub use output::{Output, OutputLine, OutputLevel};
pub use history::{History, HistoryEntry};

use parking_lot::RwLock;
use std::sync::Arc;
use std::collections::HashMap;
use thiserror::Error;

/// Shell errors
#[derive(Debug, Error)]
pub enum ShellError {
    #[error("Parse error: {0}")]
    ParseError(#[from] ParseError),

    #[error("Command error: {0}")]
    CommandError(#[from] CommandError),

    #[error("Session not found: {0}")]
    SessionNotFound(String),

    #[error("Permission denied: {0}")]
    PermissionDenied(String),

    #[error("Not implemented: {0}")]
    NotImplemented(String),
}

/// Shell configuration
#[derive(Debug, Clone)]
pub struct ShellConfig {
    /// Prompt string
    pub prompt: String,
    /// Enable history
    pub history_enabled: bool,
    /// Maximum history entries
    pub max_history: usize,
    /// Enable tab completion
    pub tab_completion: bool,
    /// Welcome message
    pub welcome_message: Option<String>,
}

impl Default for ShellConfig {
    fn default() -> Self {
        Self {
            prompt: "vsh> ".to_string(),
            history_enabled: true,
            max_history: 1000,
            tab_completion: true,
            welcome_message: Some("Void Shell v0.1.0 - Type 'help' for commands".to_string()),
        }
    }
}

/// The main shell instance
pub struct Shell {
    /// Configuration
    config: ShellConfig,
    /// Command parser
    parser: Parser,
    /// Command executor
    executor: Arc<RwLock<Executor>>,
    /// Active sessions
    sessions: RwLock<HashMap<SessionId, Session>>,
    /// Current session ID
    current_session: RwLock<Option<SessionId>>,
    /// Next session ID
    next_session_id: std::sync::atomic::AtomicU64,
}

impl Shell {
    /// Create a new shell with default configuration
    pub fn new() -> Self {
        Self::with_config(ShellConfig::default())
    }

    /// Create a new shell with custom configuration
    pub fn with_config(config: ShellConfig) -> Self {
        Self {
            config,
            parser: Parser::new(),
            executor: Arc::new(RwLock::new(Executor::new())),
            sessions: RwLock::new(HashMap::new()),
            current_session: RwLock::new(None),
            next_session_id: std::sync::atomic::AtomicU64::new(1),
        }
    }

    /// Get the prompt string
    pub fn prompt(&self) -> &str {
        &self.config.prompt
    }

    /// Get welcome message
    pub fn welcome(&self) -> Option<&str> {
        self.config.welcome_message.as_deref()
    }

    /// Execute a command line
    pub fn execute(&self, input: &str) -> Result<CommandResult, ShellError> {
        // Skip empty lines
        let input = input.trim();
        if input.is_empty() {
            return Ok(CommandResult::empty());
        }

        // Skip comments
        if input.starts_with('#') {
            return Ok(CommandResult::empty());
        }

        // Parse command
        let command = self.parser.parse(input)?;

        // Get current session context
        let session = self.current_session();

        // Build execution context
        let ctx = ExecutionContext {
            session: session.clone(),
            working_dir: session.as_ref().map(|s| s.working_dir().to_string()),
            env: session.as_ref()
                .map(|s| s.environment().clone())
                .unwrap_or_default(),
        };

        // Execute
        let result = self.executor.write().execute(command, ctx)?;

        // Add to history if enabled
        if self.config.history_enabled {
            if let Some(mut session) = session {
                session.history_mut().add(input.to_string());
            }
        }

        Ok(result)
    }

    /// Get completions for partial input
    pub fn complete(&self, partial: &str) -> Vec<String> {
        self.executor.read().complete(partial)
    }

    /// Create a new session
    pub fn create_session(&self) -> SessionId {
        let id = SessionId::new(
            self.next_session_id.fetch_add(1, std::sync::atomic::Ordering::Relaxed)
        );

        let session = Session::new(id, SessionConfig::default());
        self.sessions.write().insert(id, session);

        // Set as current if no current session
        let mut current = self.current_session.write();
        if current.is_none() {
            *current = Some(id);
        }

        id
    }

    /// Get a session by ID
    pub fn get_session(&self, id: SessionId) -> Option<Session> {
        self.sessions.read().get(&id).cloned()
    }

    /// Get current session
    pub fn current_session(&self) -> Option<Session> {
        let current = *self.current_session.read();
        current.and_then(|id| self.get_session(id))
    }

    /// Switch to a session
    pub fn switch_session(&self, id: SessionId) -> Result<(), ShellError> {
        if !self.sessions.read().contains_key(&id) {
            return Err(ShellError::SessionNotFound(format!("{:?}", id)));
        }
        *self.current_session.write() = Some(id);
        Ok(())
    }

    /// Close a session
    pub fn close_session(&self, id: SessionId) -> Option<Session> {
        let session = self.sessions.write().remove(&id);

        // If current session was closed, pick another
        let mut current = self.current_session.write();
        if *current == Some(id) {
            *current = self.sessions.read().keys().next().copied();
        }

        session
    }

    /// List all session IDs
    pub fn list_sessions(&self) -> Vec<SessionId> {
        self.sessions.read().keys().copied().collect()
    }

    /// Get the executor (for adding custom commands)
    pub fn executor(&self) -> Arc<RwLock<Executor>> {
        self.executor.clone()
    }

    /// Run a script (multiple commands)
    pub fn run_script(&self, script: &str) -> Vec<Result<CommandResult, ShellError>> {
        script.lines()
            .filter(|line| !line.trim().is_empty() && !line.trim().starts_with('#'))
            .map(|line| self.execute(line))
            .collect()
    }
}

impl Default for Shell {
    fn default() -> Self {
        Self::new()
    }
}

/// Interactive REPL runner
pub struct Repl {
    shell: Shell,
    running: bool,
}

impl Repl {
    /// Create a new REPL
    pub fn new(shell: Shell) -> Self {
        Self {
            shell,
            running: false,
        }
    }

    /// Check if REPL should continue
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// Start the REPL
    pub fn start(&mut self) -> Output {
        self.running = true;

        let mut output = Output::new();

        // Print welcome message
        if let Some(welcome) = self.shell.welcome() {
            output.add_info(welcome);
        }

        // Create initial session
        self.shell.create_session();

        output
    }

    /// Process a single line of input
    pub fn process_line(&mut self, input: &str) -> Output {
        let mut output = Output::new();

        // Check for exit commands
        if input.trim() == "exit" || input.trim() == "quit" {
            self.running = false;
            output.add_info("Goodbye!");
            return output;
        }

        // Execute the command
        match self.shell.execute(input) {
            Ok(result) => {
                if let Some(msg) = result.message() {
                    output.add_line(OutputLine::new(OutputLevel::Info, msg.to_string()));
                }
                for line in result.output_lines() {
                    output.add_line(line.clone());
                }
            }
            Err(e) => {
                output.add_error(&e.to_string());
            }
        }

        output
    }

    /// Get the prompt
    pub fn prompt(&self) -> &str {
        self.shell.prompt()
    }

    /// Get completions
    pub fn complete(&self, partial: &str) -> Vec<String> {
        self.shell.complete(partial)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shell_creation() {
        let shell = Shell::new();
        assert_eq!(shell.prompt(), "vsh> ");
    }

    #[test]
    fn test_execute_empty() {
        let shell = Shell::new();
        let result = shell.execute("");
        assert!(result.is_ok());
    }

    #[test]
    fn test_execute_comment() {
        let shell = Shell::new();
        let result = shell.execute("# this is a comment");
        assert!(result.is_ok());
    }

    #[test]
    fn test_session_management() {
        let shell = Shell::new();

        let id1 = shell.create_session();
        let id2 = shell.create_session();

        assert!(shell.get_session(id1).is_some());
        assert!(shell.get_session(id2).is_some());

        assert_eq!(shell.list_sessions().len(), 2);

        shell.close_session(id1);
        assert!(shell.get_session(id1).is_none());
        assert_eq!(shell.list_sessions().len(), 1);
    }

    #[test]
    fn test_repl() {
        let shell = Shell::new();
        let mut repl = Repl::new(shell);

        assert!(!repl.is_running());

        let output = repl.start();
        assert!(repl.is_running());
        assert!(!output.lines().is_empty());

        let output = repl.process_line("exit");
        assert!(!repl.is_running());
    }

    #[test]
    fn test_help_command() {
        let shell = Shell::new();
        shell.create_session();
        let result = shell.execute("help");
        assert!(result.is_ok());
    }
}
