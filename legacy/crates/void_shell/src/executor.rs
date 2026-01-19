//! Command executor
//!
//! Executes parsed commands.

use std::collections::HashMap;
use std::sync::Arc;

use crate::command::{Command, CommandHandler, CommandResult, CommandError};
use crate::session::Session;
use crate::builtins;

/// Execution context
#[derive(Debug, Clone, Default)]
pub struct ExecutionContext {
    /// Current session
    pub session: Option<Session>,
    /// Working directory
    pub working_dir: Option<String>,
    /// Environment variables
    pub env: HashMap<String, String>,
}

impl ExecutionContext {
    /// Get environment variable
    pub fn get_env(&self, key: &str) -> Option<&str> {
        self.env.get(key).map(|s| s.as_str())
    }

    /// Set environment variable
    pub fn set_env(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.env.insert(key.into(), value.into());
    }
}

/// Command executor
pub struct Executor {
    /// Registered command handlers
    handlers: HashMap<String, Arc<dyn CommandHandler>>,
    /// Command aliases
    aliases: HashMap<String, String>,
}

impl Executor {
    /// Create a new executor with built-in commands
    pub fn new() -> Self {
        let mut executor = Self {
            handlers: HashMap::new(),
            aliases: HashMap::new(),
        };

        // Register built-in commands
        executor.register(Arc::new(builtins::HelpCommand::new()));
        executor.register(Arc::new(builtins::EchoCommand));
        executor.register(Arc::new(builtins::ClearCommand));
        executor.register(Arc::new(builtins::HistoryCommand));
        executor.register(Arc::new(builtins::EnvCommand));
        executor.register(Arc::new(builtins::AliasCommand));
        executor.register(Arc::new(builtins::VersionCommand));
        executor.register(Arc::new(builtins::StatusCommand));
        executor.register(Arc::new(builtins::ListCommand));
        executor.register(Arc::new(builtins::SpawnCommand));
        executor.register(Arc::new(builtins::KillCommand));

        // Default aliases
        executor.add_alias("?", "help");
        executor.add_alias("cls", "clear");
        executor.add_alias("ls", "list");
        executor.add_alias("h", "history");

        executor
    }

    /// Register a command handler
    pub fn register(&mut self, handler: Arc<dyn CommandHandler>) {
        self.handlers.insert(handler.name().to_string(), handler);
    }

    /// Unregister a command handler
    pub fn unregister(&mut self, name: &str) -> Option<Arc<dyn CommandHandler>> {
        self.handlers.remove(name)
    }

    /// Add an alias
    pub fn add_alias(&mut self, alias: &str, target: &str) {
        self.aliases.insert(alias.to_string(), target.to_string());
    }

    /// Remove an alias
    pub fn remove_alias(&mut self, alias: &str) -> Option<String> {
        self.aliases.remove(alias)
    }

    /// Get all aliases
    pub fn aliases(&self) -> &HashMap<String, String> {
        &self.aliases
    }

    /// Execute a command
    pub fn execute(&mut self, mut cmd: Command, ctx: ExecutionContext) -> Result<CommandResult, CommandError> {
        // Resolve alias
        if let Some(target) = self.aliases.get(&cmd.name).cloned() {
            cmd.name = target;
        }

        // Find handler
        let handler = self.handlers.get(&cmd.name)
            .cloned()
            .ok_or_else(|| CommandError::UnknownCommand(cmd.name.clone()))?;

        // Execute
        let result = handler.execute(&cmd, &ctx)?;

        // Handle piped command
        if let Some(piped_cmd) = cmd.pipe_to {
            // Pass output to next command (simplified - just concat args)
            let mut next_cmd = *piped_cmd;
            if let Some(data) = result.data() {
                if let Some(s) = data.as_str() {
                    next_cmd.args.insert(0, s.to_string());
                }
            }
            return self.execute(next_cmd, ctx);
        }

        Ok(result)
    }

    /// Get completions for partial input
    pub fn complete(&self, partial: &str) -> Vec<String> {
        let partial_lower = partial.to_lowercase();
        let mut completions: Vec<String> = Vec::new();

        // Complete command names
        for name in self.handlers.keys() {
            if name.starts_with(&partial_lower) {
                completions.push(name.clone());
            }
        }

        // Complete aliases
        for alias in self.aliases.keys() {
            if alias.starts_with(&partial_lower) {
                completions.push(alias.clone());
            }
        }

        completions.sort();
        completions.dedup();
        completions
    }

    /// Get all command names
    pub fn command_names(&self) -> Vec<&str> {
        self.handlers.keys().map(|s| s.as_str()).collect()
    }

    /// Get handler for a command
    pub fn get_handler(&self, name: &str) -> Option<Arc<dyn CommandHandler>> {
        // Resolve alias first
        let resolved = self.aliases.get(name).map(|s| s.as_str()).unwrap_or(name);
        self.handlers.get(resolved).cloned()
    }
}

impl Default for Executor {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_executor_creation() {
        let executor = Executor::new();
        assert!(!executor.command_names().is_empty());
    }

    #[test]
    fn test_builtin_help() {
        let mut executor = Executor::new();
        let cmd = Command::new("help");
        let result = executor.execute(cmd, ExecutionContext::default());
        assert!(result.is_ok());
    }

    #[test]
    fn test_unknown_command() {
        let mut executor = Executor::new();
        let cmd = Command::new("nonexistent");
        let result = executor.execute(cmd, ExecutionContext::default());
        assert!(matches!(result, Err(CommandError::UnknownCommand(_))));
    }

    #[test]
    fn test_alias_resolution() {
        let mut executor = Executor::new();
        executor.add_alias("h", "help");

        let cmd = Command::new("h");
        let result = executor.execute(cmd, ExecutionContext::default());
        assert!(result.is_ok());
    }

    #[test]
    fn test_completions() {
        let executor = Executor::new();
        let completions = executor.complete("he");
        assert!(completions.contains(&"help".to_string()));
    }
}
