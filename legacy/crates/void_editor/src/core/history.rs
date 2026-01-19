//! Undo/Redo history with command pattern and transaction support.
//!
//! All editor modifications go through the history system to enable
//! undo/redo. Commands can be grouped into transactions for atomic
//! multi-step operations.

use crate::commands::{Command, CommandResult};

/// A group of commands executed as a single undoable unit.
pub struct Transaction {
    pub name: String,
    pub commands: Vec<Box<dyn Command>>,
}

impl Transaction {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            commands: Vec::new(),
        }
    }

    pub fn push(&mut self, cmd: Box<dyn Command>) {
        self.commands.push(cmd);
    }

    pub fn is_empty(&self) -> bool {
        self.commands.is_empty()
    }
}

/// Undo/redo history stack.
pub struct UndoHistory {
    /// Commands that can be undone
    undo_stack: Vec<Box<dyn Command>>,
    /// Commands that can be redone
    redo_stack: Vec<Box<dyn Command>>,
    /// Maximum history size
    max_size: usize,
    /// Current open transaction
    current_transaction: Option<Transaction>,
    /// Whether history has been modified since last save
    dirty: bool,
}

impl Default for UndoHistory {
    fn default() -> Self {
        Self::new()
    }
}

impl UndoHistory {
    /// Default maximum history size.
    pub const DEFAULT_MAX_SIZE: usize = 100;

    pub fn new() -> Self {
        Self::with_capacity(Self::DEFAULT_MAX_SIZE)
    }

    pub fn with_capacity(max_size: usize) -> Self {
        Self {
            undo_stack: Vec::new(),
            redo_stack: Vec::new(),
            max_size,
            current_transaction: None,
            dirty: false,
        }
    }

    /// Check if there are commands to undo.
    pub fn can_undo(&self) -> bool {
        !self.undo_stack.is_empty()
    }

    /// Check if there are commands to redo.
    pub fn can_redo(&self) -> bool {
        !self.redo_stack.is_empty()
    }

    /// Get the description of the next undo command.
    pub fn undo_description(&self) -> Option<&str> {
        self.undo_stack.last().map(|c| c.description())
    }

    /// Get the description of the next redo command.
    pub fn redo_description(&self) -> Option<&str> {
        self.redo_stack.last().map(|c| c.description())
    }

    /// Check and clear the dirty flag.
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Mark as saved (clears dirty flag).
    pub fn mark_saved(&mut self) {
        self.dirty = false;
    }

    /// Begin a new transaction.
    /// Commands added during a transaction are grouped as one undo unit.
    pub fn begin_transaction(&mut self, name: impl Into<String>) {
        if self.current_transaction.is_some() {
            log::warn!("Beginning transaction while one is already open");
        }
        self.current_transaction = Some(Transaction::new(name));
    }

    /// Commit the current transaction.
    pub fn commit_transaction(&mut self) {
        if let Some(transaction) = self.current_transaction.take() {
            if !transaction.is_empty() {
                self.push_command(Box::new(TransactionCommand(transaction)));
            }
        }
    }

    /// Rollback the current transaction.
    pub fn rollback_transaction(&mut self) {
        self.current_transaction = None;
    }

    /// Check if a transaction is currently open.
    pub fn in_transaction(&self) -> bool {
        self.current_transaction.is_some()
    }

    /// Push a command that has already been executed.
    pub fn push(&mut self, cmd: Box<dyn Command>) {
        if let Some(ref mut transaction) = self.current_transaction {
            transaction.push(cmd);
        } else {
            self.push_command(cmd);
        }
    }

    fn push_command(&mut self, cmd: Box<dyn Command>) {
        // Check if we can merge with the previous command
        if let Some(last) = self.undo_stack.last() {
            if last.can_merge(cmd.as_ref()) {
                // Commands can be merged - the new command replaces the old
                self.undo_stack.pop();
            }
        }

        self.undo_stack.push(cmd);
        self.redo_stack.clear(); // Clear redo on new action
        self.dirty = true;

        // Trim if over limit
        while self.undo_stack.len() > self.max_size {
            self.undo_stack.remove(0);
        }
    }

    /// Pop a command from the undo stack.
    pub fn pop_undo(&mut self) -> Option<Box<dyn Command>> {
        let cmd = self.undo_stack.pop();
        if cmd.is_some() {
            self.dirty = true;
        }
        cmd
    }

    /// Pop a command from the redo stack.
    pub fn pop_redo(&mut self) -> Option<Box<dyn Command>> {
        let cmd = self.redo_stack.pop();
        if cmd.is_some() {
            self.dirty = true;
        }
        cmd
    }

    /// Push a command to the undo stack (for redo completion).
    pub fn push_to_undo(&mut self, cmd: Box<dyn Command>) {
        self.undo_stack.push(cmd);
        self.dirty = true;
    }

    /// Push a command to the redo stack (for undo completion).
    pub fn push_to_redo(&mut self, cmd: Box<dyn Command>) {
        self.redo_stack.push(cmd);
        self.dirty = true;
    }

    /// Clear all history.
    pub fn clear(&mut self) {
        self.undo_stack.clear();
        self.redo_stack.clear();
        self.current_transaction = None;
        self.dirty = false;
    }

    /// Get the number of commands in the undo stack.
    pub fn undo_count(&self) -> usize {
        self.undo_stack.len()
    }

    /// Get the number of commands in the redo stack.
    pub fn redo_count(&self) -> usize {
        self.redo_stack.len()
    }
}

/// A command that wraps a transaction.
struct TransactionCommand(Transaction);

impl Command for TransactionCommand {
    fn description(&self) -> &str {
        &self.0.name
    }

    fn execute(&mut self, state: &mut crate::core::EditorState) -> CommandResult {
        for cmd in &mut self.0.commands {
            cmd.execute(state)?;
        }
        Ok(())
    }

    fn undo(&mut self, state: &mut crate::core::EditorState) -> CommandResult {
        // Undo in reverse order
        for cmd in self.0.commands.iter_mut().rev() {
            cmd.undo(state)?;
        }
        Ok(())
    }

    fn can_merge(&self, _other: &dyn Command) -> bool {
        false // Transactions cannot merge
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestCommand {
        value: i32,
        executed: bool,
    }

    impl Command for TestCommand {
        fn description(&self) -> &str {
            "Test"
        }

        fn execute(&mut self, _state: &mut crate::core::EditorState) -> CommandResult {
            self.executed = true;
            Ok(())
        }

        fn undo(&mut self, _state: &mut crate::core::EditorState) -> CommandResult {
            self.executed = false;
            Ok(())
        }

        fn can_merge(&self, _other: &dyn Command) -> bool {
            false
        }
    }

    #[test]
    fn test_history_basic() {
        let mut history = UndoHistory::new();

        assert!(!history.can_undo());
        assert!(!history.can_redo());

        history.push(Box::new(TestCommand { value: 1, executed: true }));

        assert!(history.can_undo());
        assert!(!history.can_redo());
    }

    #[test]
    fn test_history_undo_redo() {
        let mut history = UndoHistory::new();

        history.push(Box::new(TestCommand { value: 1, executed: true }));
        history.push(Box::new(TestCommand { value: 2, executed: true }));

        assert_eq!(history.undo_count(), 2);

        // Pop from undo stack (simulating undo)
        if let Some(cmd) = history.pop_undo() {
            history.push_to_redo(cmd);
        }
        assert_eq!(history.undo_count(), 1);
        assert_eq!(history.redo_count(), 1);

        // Pop from redo stack (simulating redo)
        if let Some(cmd) = history.pop_redo() {
            history.push_to_undo(cmd);
        }
        assert_eq!(history.undo_count(), 2);
        assert_eq!(history.redo_count(), 0);
    }
}
