//! Built-in shell commands
//!
//! Core commands available in every shell session.

use crate::command::{Command, CommandHandler, CommandResult, CommandError};
use crate::executor::ExecutionContext;
use crate::output::{OutputLine, OutputLevel};

/// Help command - shows available commands
pub struct HelpCommand {
    /// Cached command list (populated lazily)
    commands: Vec<(String, String)>,
}

impl HelpCommand {
    pub fn new() -> Self {
        Self {
            commands: vec![
                ("help".into(), "Show this help message".into()),
                ("echo".into(), "Print arguments".into()),
                ("clear".into(), "Clear the screen".into()),
                ("history".into(), "Show command history".into()),
                ("env".into(), "Show/set environment variables".into()),
                ("alias".into(), "Manage command aliases".into()),
                ("version".into(), "Show shell version".into()),
                ("status".into(), "Show system status".into()),
                ("list".into(), "List apps, layers, or assets".into()),
                ("spawn".into(), "Spawn a new app".into()),
                ("kill".into(), "Terminate an app".into()),
                ("exit".into(), "Exit the shell".into()),
            ],
        }
    }
}

impl CommandHandler for HelpCommand {
    fn name(&self) -> &str {
        "help"
    }

    fn description(&self) -> &str {
        "Show available commands and their descriptions"
    }

    fn usage(&self) -> &str {
        "help [command]"
    }

    fn execute(&self, cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        if let Some(topic) = cmd.first_arg() {
            // Show help for specific command
            for (name, desc) in &self.commands {
                if name == topic {
                    return Ok(CommandResult::with_message(format!("{}: {}", name, desc)));
                }
            }
            return Ok(CommandResult::with_message(format!("Unknown command: {}", topic)));
        }

        // Show all commands
        let mut result = CommandResult::success();
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Available commands:".to_string()));

        for (name, desc) in &self.commands {
            result = result.add_line(OutputLine::new(
                OutputLevel::Info,
                format!("  {:12} - {}", name, desc)
            ));
        }

        Ok(result)
    }

    fn complete(&self, partial: &str, _ctx: &ExecutionContext) -> Vec<String> {
        self.commands.iter()
            .filter(|(name, _)| name.starts_with(partial))
            .map(|(name, _)| name.clone())
            .collect()
    }
}

/// Echo command - print arguments
pub struct EchoCommand;

impl CommandHandler for EchoCommand {
    fn name(&self) -> &str {
        "echo"
    }

    fn description(&self) -> &str {
        "Print arguments to output"
    }

    fn usage(&self) -> &str {
        "echo [text...]"
    }

    fn execute(&self, cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let message = cmd.args.join(" ");
        Ok(CommandResult::with_message(message).with_data(serde_json::json!(cmd.args.join(" "))))
    }
}

/// Clear command - clear screen
pub struct ClearCommand;

impl CommandHandler for ClearCommand {
    fn name(&self) -> &str {
        "clear"
    }

    fn description(&self) -> &str {
        "Clear the screen"
    }

    fn execute(&self, _cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        // In a real terminal, this would send ANSI escape codes
        // For now, just return a special marker
        Ok(CommandResult::success()
            .with_data(serde_json::json!({"action": "clear"})))
    }
}

/// History command - show command history
pub struct HistoryCommand;

impl CommandHandler for HistoryCommand {
    fn name(&self) -> &str {
        "history"
    }

    fn description(&self) -> &str {
        "Show command history"
    }

    fn usage(&self) -> &str {
        "history [count]"
    }

    fn execute(&self, cmd: &Command, ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let count: usize = cmd.first_arg()
            .and_then(|s| s.parse().ok())
            .unwrap_or(10);

        let Some(session) = &ctx.session else {
            return Ok(CommandResult::with_message("No active session"));
        };

        let history = session.history();
        let entries = history.recent(count);

        if entries.is_empty() {
            return Ok(CommandResult::with_message("No history"));
        }

        let mut result = CommandResult::success();
        for (i, entry) in entries.iter().enumerate() {
            result = result.add_line(OutputLine::new(
                OutputLevel::Info,
                format!("{:4}  {}", history.len() - entries.len() + i + 1, entry.command())
            ));
        }

        Ok(result)
    }
}

/// Env command - show/set environment variables
pub struct EnvCommand;

impl CommandHandler for EnvCommand {
    fn name(&self) -> &str {
        "env"
    }

    fn description(&self) -> &str {
        "Show or set environment variables"
    }

    fn usage(&self) -> &str {
        "env [name] [value] | env --set name=value | env --unset name"
    }

    fn execute(&self, cmd: &Command, ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        // Show all env vars
        if cmd.args.is_empty() {
            let mut result = CommandResult::success();
            for (key, value) in &ctx.env {
                result = result.add_line(OutputLine::new(
                    OutputLevel::Info,
                    format!("{}={}", key, value)
                ));
            }
            return Ok(result);
        }

        // Get specific var
        if cmd.args.len() == 1 {
            let key = &cmd.args[0];
            if let Some(value) = ctx.env.get(key) {
                return Ok(CommandResult::with_message(format!("{}={}", key, value)));
            } else {
                return Ok(CommandResult::with_message(format!("{} is not set", key)));
            }
        }

        // Set var (key value)
        if cmd.args.len() >= 2 {
            let key = &cmd.args[0];
            let value = cmd.args[1..].join(" ");
            return Ok(CommandResult::with_message(format!("Set {}={}", key, value))
                .with_data(serde_json::json!({
                    "action": "set_env",
                    "key": key,
                    "value": value
                })));
        }

        Ok(CommandResult::success())
    }
}

/// Alias command - manage aliases
pub struct AliasCommand;

impl CommandHandler for AliasCommand {
    fn name(&self) -> &str {
        "alias"
    }

    fn description(&self) -> &str {
        "Manage command aliases"
    }

    fn usage(&self) -> &str {
        "alias [name] [command] | alias --remove name"
    }

    fn execute(&self, cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        // Show all aliases (placeholder - would need access to executor)
        if cmd.args.is_empty() {
            return Ok(CommandResult::with_message("Use 'alias name command' to create an alias"));
        }

        // Create alias
        if cmd.args.len() >= 2 {
            let name = &cmd.args[0];
            let target = cmd.args[1..].join(" ");
            return Ok(CommandResult::with_message(format!("Alias '{}' -> '{}'", name, target))
                .with_data(serde_json::json!({
                    "action": "add_alias",
                    "name": name,
                    "target": target
                })));
        }

        Ok(CommandResult::success())
    }
}

/// Version command
pub struct VersionCommand;

impl CommandHandler for VersionCommand {
    fn name(&self) -> &str {
        "version"
    }

    fn description(&self) -> &str {
        "Show shell and engine version"
    }

    fn execute(&self, _cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let mut result = CommandResult::success();
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Void Shell (vsh) v0.1.0".to_string()));
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Void Engine Runtime v0.1.0".to_string()));
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Kernel: v0.1.0".to_string()));
        Ok(result)
    }
}

/// Status command - system status
pub struct StatusCommand;

impl CommandHandler for StatusCommand {
    fn name(&self) -> &str {
        "status"
    }

    fn description(&self) -> &str {
        "Show system status"
    }

    fn execute(&self, _cmd: &Command, ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let mut result = CommandResult::success();

        result = result.add_line(OutputLine::new(OutputLevel::Info, "=== System Status ===".to_string()));

        // Session info
        if let Some(session) = &ctx.session {
            result = result.add_line(OutputLine::new(OutputLevel::Info,
                format!("Session: {:?}", session.id())
            ));
        }

        // Placeholder stats
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Kernel: Running".to_string()));
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Apps: 0 active".to_string()));
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Layers: 1 active".to_string()));
        result = result.add_line(OutputLine::new(OutputLevel::Info, "Backend: Vulkan".to_string()));

        Ok(result)
    }
}

/// List command - list apps, layers, assets
pub struct ListCommand;

impl CommandHandler for ListCommand {
    fn name(&self) -> &str {
        "list"
    }

    fn description(&self) -> &str {
        "List apps, layers, or assets"
    }

    fn usage(&self) -> &str {
        "list [apps|layers|assets|presenters]"
    }

    fn execute(&self, cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let category = cmd.first_arg().unwrap_or("apps");

        let mut result = CommandResult::success();

        match category {
            "apps" => {
                result = result.add_line(OutputLine::new(OutputLevel::Info, "No apps currently running".to_string()));
            }
            "layers" => {
                result = result.add_line(OutputLine::new(OutputLevel::Info, "Active layers:".to_string()));
                result = result.add_line(OutputLine::new(OutputLevel::Info, "  [0] Base (Content)".to_string()));
            }
            "assets" => {
                result = result.add_line(OutputLine::new(OutputLevel::Info, "Asset registry: 0 assets".to_string()));
            }
            "presenters" => {
                result = result.add_line(OutputLine::new(OutputLevel::Info, "Presenters:".to_string()));
                result = result.add_line(OutputLine::new(OutputLevel::Info, "  [1] Desktop (primary)".to_string()));
            }
            other => {
                return Err(CommandError::InvalidArguments(
                    format!("Unknown category: {}. Use: apps, layers, assets, presenters", other)
                ));
            }
        }

        Ok(result)
    }

    fn complete(&self, partial: &str, _ctx: &ExecutionContext) -> Vec<String> {
        let options = ["apps", "layers", "assets", "presenters"];
        options.iter()
            .filter(|s| s.starts_with(partial))
            .map(|s| s.to_string())
            .collect()
    }
}

/// Spawn command - spawn a new app
pub struct SpawnCommand;

impl CommandHandler for SpawnCommand {
    fn name(&self) -> &str {
        "spawn"
    }

    fn description(&self) -> &str {
        "Spawn a new app"
    }

    fn usage(&self) -> &str {
        "spawn <app_name> [--layer N] [--namespace NS]"
    }

    fn execute(&self, cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let app_name = cmd.require_arg(0, "app_name")?;
        let layer = cmd.get_option("layer").unwrap_or("0");
        let namespace = cmd.get_option("namespace").unwrap_or("default");

        Ok(CommandResult::with_message(format!(
            "Spawning app '{}' in namespace '{}' on layer {}",
            app_name, namespace, layer
        )).with_data(serde_json::json!({
            "action": "spawn_app",
            "app_name": app_name,
            "layer": layer.parse::<u32>().unwrap_or(0),
            "namespace": namespace
        })))
    }
}

/// Kill command - terminate an app
pub struct KillCommand;

impl CommandHandler for KillCommand {
    fn name(&self) -> &str {
        "kill"
    }

    fn description(&self) -> &str {
        "Terminate an app"
    }

    fn usage(&self) -> &str {
        "kill <app_id|app_name> [--force]"
    }

    fn execute(&self, cmd: &Command, _ctx: &ExecutionContext) -> Result<CommandResult, CommandError> {
        let target = cmd.require_arg(0, "app_id or app_name")?;
        let force = cmd.has_option("force") || cmd.has_short('f');

        Ok(CommandResult::with_message(format!(
            "{} app '{}'",
            if force { "Force killing" } else { "Terminating" },
            target
        )).with_data(serde_json::json!({
            "action": "kill_app",
            "target": target,
            "force": force
        })))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_help_command() {
        let help = HelpCommand::new();
        let cmd = Command::new("help");
        let result = help.execute(&cmd, &ExecutionContext::default()).unwrap();
        assert!(result.is_success());
    }

    #[test]
    fn test_echo_command() {
        let echo = EchoCommand;
        let cmd = Command::new("echo").arg("hello").arg("world");
        let result = echo.execute(&cmd, &ExecutionContext::default()).unwrap();
        assert_eq!(result.message(), Some("hello world"));
    }

    #[test]
    fn test_version_command() {
        let version = VersionCommand;
        let cmd = Command::new("version");
        let result = version.execute(&cmd, &ExecutionContext::default()).unwrap();
        assert!(result.is_success());
    }

    #[test]
    fn test_list_command() {
        let list = ListCommand;

        let cmd = Command::new("list").arg("apps");
        let result = list.execute(&cmd, &ExecutionContext::default()).unwrap();
        assert!(result.is_success());

        let cmd = Command::new("list").arg("invalid");
        let result = list.execute(&cmd, &ExecutionContext::default());
        assert!(matches!(result, Err(CommandError::InvalidArguments(_))));
    }

    #[test]
    fn test_spawn_command() {
        let spawn = SpawnCommand;
        let cmd = Command::new("spawn")
            .arg("my_app")
            .option("layer", "1");
        let result = spawn.execute(&cmd, &ExecutionContext::default()).unwrap();
        assert!(result.message().unwrap().contains("my_app"));
    }

    #[test]
    fn test_spawn_missing_arg() {
        let spawn = SpawnCommand;
        let cmd = Command::new("spawn");
        let result = spawn.execute(&cmd, &ExecutionContext::default());
        assert!(matches!(result, Err(CommandError::MissingArgument(_))));
    }
}
