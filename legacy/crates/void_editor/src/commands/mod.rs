//! Command pattern implementation for undo/redo support.
//!
//! All editor modifications that should be undoable must go through
//! the command system.

mod command;
mod entity_commands;
mod transform_commands;

pub use command::{Command, CommandResult, CommandError};
pub use entity_commands::{
    CreateEntityCommand,
    DeleteEntityCommand,
    DuplicateEntityCommand,
    DeleteMultipleCommand,
    ReparentCommand,
};
pub use transform_commands::{SetTransformCommand, MoveCommand, RotateCommand, ScaleCommand};

use crate::core::EditorState;

/// Execute a command and add it to history.
pub fn execute(state: &mut EditorState, mut cmd: Box<dyn Command>) -> CommandResult {
    cmd.execute(state)?;
    state.history.push(cmd);
    Ok(())
}

/// Execute a command without adding to history (for internal use).
pub fn execute_silent(state: &mut EditorState, mut cmd: Box<dyn Command>) -> CommandResult {
    cmd.execute(state)
}
