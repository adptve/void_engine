//! Command representation
//!
//! Defines the command structure and results.

use std::collections::HashMap;
use crate::output::OutputLine;

/// Command error
#[derive(Debug, thiserror::Error)]
pub enum CommandError {
    #[error("Unknown command: {0}")]
    UnknownCommand(String),

    #[error("Invalid arguments: {0}")]
    InvalidArguments(String),

    #[error("Missing argument: {0}")]
    MissingArgument(String),

    #[error("Execution failed: {0}")]
    ExecutionFailed(String),

    #[error("Permission denied: {0}")]
    PermissionDenied(String),
}

/// A parsed command
#[derive(Debug, Clone)]
pub struct Command {
    /// Command name
    pub name: String,
    /// Positional arguments
    pub args: Vec<String>,
    /// Named options (--key value or --flag)
    pub options: HashMap<String, Option<String>>,
    /// Short options (-x)
    pub short_options: Vec<char>,
    /// Piped command (cmd1 | cmd2)
    pub pipe_to: Option<Box<Command>>,
    /// Background execution (&)
    pub background: bool,
}

impl Command {
    /// Create a new command
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            args: Vec::new(),
            options: HashMap::new(),
            short_options: Vec::new(),
            pipe_to: None,
            background: false,
        }
    }

    /// Add a positional argument
    pub fn arg(mut self, arg: impl Into<String>) -> Self {
        self.args.push(arg.into());
        self
    }

    /// Add a named option with value
    pub fn option(mut self, key: impl Into<String>, value: impl Into<String>) -> Self {
        self.options.insert(key.into(), Some(value.into()));
        self
    }

    /// Add a flag option (no value)
    pub fn flag(mut self, key: impl Into<String>) -> Self {
        self.options.insert(key.into(), None);
        self
    }

    /// Add a short option
    pub fn short(mut self, opt: char) -> Self {
        self.short_options.push(opt);
        self
    }

    /// Set pipe target
    pub fn pipe(mut self, target: Command) -> Self {
        self.pipe_to = Some(Box::new(target));
        self
    }

    /// Set background execution
    pub fn background(mut self) -> Self {
        self.background = true;
        self
    }

    /// Check if an option is set
    pub fn has_option(&self, key: &str) -> bool {
        self.options.contains_key(key)
    }

    /// Get option value
    pub fn get_option(&self, key: &str) -> Option<&str> {
        self.options.get(key).and_then(|v| v.as_deref())
    }

    /// Check if short option is set
    pub fn has_short(&self, opt: char) -> bool {
        self.short_options.contains(&opt)
    }

    /// Get first argument
    pub fn first_arg(&self) -> Option<&str> {
        self.args.first().map(|s| s.as_str())
    }

    /// Get argument at index
    pub fn get_arg(&self, index: usize) -> Option<&str> {
        self.args.get(index).map(|s| s.as_str())
    }

    /// Get required argument
    pub fn require_arg(&self, index: usize, name: &str) -> Result<&str, CommandError> {
        self.get_arg(index)
            .ok_or_else(|| CommandError::MissingArgument(name.to_string()))
    }

    /// Get required option
    pub fn require_option(&self, key: &str) -> Result<&str, CommandError> {
        self.get_option(key)
            .ok_or_else(|| CommandError::MissingArgument(format!("--{}", key)))
    }
}

/// Result of command execution
#[derive(Debug, Clone, Default)]
pub struct CommandResult {
    /// Success status
    success: bool,
    /// Exit code (0 = success)
    exit_code: i32,
    /// Main message
    message: Option<String>,
    /// Output lines
    output: Vec<OutputLine>,
    /// Structured data (for piping)
    data: Option<serde_json::Value>,
}

impl CommandResult {
    /// Create a successful result
    pub fn success() -> Self {
        Self {
            success: true,
            exit_code: 0,
            message: None,
            output: Vec::new(),
            data: None,
        }
    }

    /// Create empty result (for no-op commands)
    pub fn empty() -> Self {
        Self::success()
    }

    /// Create a failure result
    pub fn failure(message: impl Into<String>) -> Self {
        Self {
            success: false,
            exit_code: 1,
            message: Some(message.into()),
            output: Vec::new(),
            data: None,
        }
    }

    /// Create result with message
    pub fn with_message(message: impl Into<String>) -> Self {
        Self {
            success: true,
            exit_code: 0,
            message: Some(message.into()),
            output: Vec::new(),
            data: None,
        }
    }

    /// Add output line
    pub fn add_line(mut self, line: OutputLine) -> Self {
        self.output.push(line);
        self
    }

    /// Add multiple output lines
    pub fn add_lines(mut self, lines: Vec<OutputLine>) -> Self {
        self.output.extend(lines);
        self
    }

    /// Set structured data
    pub fn with_data(mut self, data: serde_json::Value) -> Self {
        self.data = Some(data);
        self
    }

    /// Set exit code
    pub fn with_exit_code(mut self, code: i32) -> Self {
        self.exit_code = code;
        self.success = code == 0;
        self
    }

    /// Check if successful
    pub fn is_success(&self) -> bool {
        self.success
    }

    /// Get exit code
    pub fn exit_code(&self) -> i32 {
        self.exit_code
    }

    /// Get message
    pub fn message(&self) -> Option<&str> {
        self.message.as_deref()
    }

    /// Get output lines
    pub fn output_lines(&self) -> &[OutputLine] {
        &self.output
    }

    /// Get structured data
    pub fn data(&self) -> Option<&serde_json::Value> {
        self.data.as_ref()
    }
}

/// Command handler trait
pub trait CommandHandler: Send + Sync {
    /// Get the command name
    fn name(&self) -> &str;

    /// Get command description
    fn description(&self) -> &str;

    /// Get usage string
    fn usage(&self) -> &str {
        self.name()
    }

    /// Execute the command
    fn execute(&self, cmd: &Command, ctx: &crate::ExecutionContext) -> Result<CommandResult, CommandError>;

    /// Get completions for arguments
    fn complete(&self, _partial: &str, _ctx: &crate::ExecutionContext) -> Vec<String> {
        Vec::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_command_builder() {
        let cmd = Command::new("spawn")
            .arg("my_app")
            .option("layer", "1")
            .flag("verbose")
            .short('v');

        assert_eq!(cmd.name, "spawn");
        assert_eq!(cmd.first_arg(), Some("my_app"));
        assert_eq!(cmd.get_option("layer"), Some("1"));
        assert!(cmd.has_option("verbose"));
        assert!(cmd.has_short('v'));
    }

    #[test]
    fn test_command_result() {
        let result = CommandResult::with_message("Done!")
            .with_exit_code(0);

        assert!(result.is_success());
        assert_eq!(result.message(), Some("Done!"));
        assert_eq!(result.exit_code(), 0);
    }

    #[test]
    fn test_failure_result() {
        let result = CommandResult::failure("Something went wrong");

        assert!(!result.is_success());
        assert_eq!(result.exit_code(), 1);
    }
}
