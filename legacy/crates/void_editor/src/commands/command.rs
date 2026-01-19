//! Command trait and result types.

use std::any::Any;
use crate::core::EditorState;

/// Result type for command execution.
pub type CommandResult = Result<(), CommandError>;

/// Errors that can occur during command execution.
#[derive(Clone, Debug)]
pub enum CommandError {
    /// Entity not found
    EntityNotFound(crate::core::EntityId),
    /// Invalid operation
    InvalidOperation(String),
    /// Generic error
    Other(String),
}

impl std::fmt::Display for CommandError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CommandError::EntityNotFound(id) => write!(f, "Entity not found: {}", id),
            CommandError::InvalidOperation(msg) => write!(f, "Invalid operation: {}", msg),
            CommandError::Other(msg) => write!(f, "Error: {}", msg),
        }
    }
}

impl std::error::Error for CommandError {}

/// A command that can be executed, undone, and redone.
///
/// Commands are the primary way to modify editor state. They capture
/// both the action and its inverse, enabling undo/redo.
///
/// # Example
///
/// ```ignore
/// struct MoveCommand {
///     entity_id: EntityId,
///     old_position: [f32; 3],
///     new_position: [f32; 3],
/// }
///
/// impl Command for MoveCommand {
///     fn description(&self) -> &str { "Move Entity" }
///
///     fn execute(&mut self, state: &mut EditorState) -> CommandResult {
///         if let Some(entity) = state.get_entity_mut(self.entity_id) {
///             entity.transform.position = self.new_position;
///         }
///         Ok(())
///     }
///
///     fn undo(&mut self, state: &mut EditorState) -> CommandResult {
///         if let Some(entity) = state.get_entity_mut(self.entity_id) {
///             entity.transform.position = self.old_position;
///         }
///         Ok(())
///     }
/// }
/// ```
pub trait Command: Send + Sync {
    /// Human-readable description for the undo/redo menu.
    fn description(&self) -> &str;

    /// Execute the command, modifying the editor state.
    fn execute(&mut self, state: &mut EditorState) -> CommandResult;

    /// Undo the command, restoring the previous state.
    fn undo(&mut self, state: &mut EditorState) -> CommandResult;

    /// Check if this command can be merged with another.
    /// Merging combines consecutive similar commands (e.g., multiple small moves)
    /// into a single undoable action.
    fn can_merge(&self, _other: &dyn Command) -> bool {
        false
    }

}

/// A no-op command for testing.
pub struct NoopCommand;

impl Command for NoopCommand {
    fn description(&self) -> &str {
        "No Operation"
    }

    fn execute(&mut self, _state: &mut EditorState) -> CommandResult {
        Ok(())
    }

    fn undo(&mut self, _state: &mut EditorState) -> CommandResult {
        Ok(())
    }
}
