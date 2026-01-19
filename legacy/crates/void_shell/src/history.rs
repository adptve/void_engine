//! Command history management
//!
//! Tracks and navigates command history.

use std::collections::VecDeque;
use serde::{Serialize, Deserialize};

/// A single history entry
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HistoryEntry {
    /// The command that was executed
    command: String,
    /// Timestamp (Unix epoch seconds)
    timestamp: u64,
    /// Exit code of the command
    exit_code: Option<i32>,
    /// Optional session ID
    session_id: Option<u64>,
}

impl HistoryEntry {
    /// Create a new history entry
    pub fn new(command: String) -> Self {
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0);

        Self {
            command,
            timestamp,
            exit_code: None,
            session_id: None,
        }
    }

    /// Create with exit code
    pub fn with_exit_code(mut self, code: i32) -> Self {
        self.exit_code = Some(code);
        self
    }

    /// Create with session ID
    pub fn with_session(mut self, session_id: u64) -> Self {
        self.session_id = Some(session_id);
        self
    }

    /// Get the command
    pub fn command(&self) -> &str {
        &self.command
    }

    /// Get the timestamp
    pub fn timestamp(&self) -> u64 {
        self.timestamp
    }

    /// Get the exit code
    pub fn exit_code(&self) -> Option<i32> {
        self.exit_code
    }

    /// Get the session ID
    pub fn session_id(&self) -> Option<u64> {
        self.session_id
    }
}

/// Command history
#[derive(Debug, Clone)]
pub struct History {
    /// History entries
    entries: VecDeque<HistoryEntry>,
    /// Maximum entries
    max_entries: usize,
    /// Current navigation position
    position: Option<usize>,
    /// Temporary current line (for navigation)
    current_line: Option<String>,
}

impl History {
    /// Create a new history with max size
    pub fn new(max_entries: usize) -> Self {
        Self {
            entries: VecDeque::with_capacity(max_entries.min(1000)),
            max_entries,
            position: None,
            current_line: None,
        }
    }

    /// Add a command to history
    pub fn add(&mut self, command: String) {
        // Skip empty commands
        if command.trim().is_empty() {
            return;
        }

        // Skip duplicates of the last command
        if let Some(last) = self.entries.back() {
            if last.command == command {
                return;
            }
        }

        // Add entry
        let entry = HistoryEntry::new(command);
        self.entries.push_back(entry);

        // Trim to max size
        while self.entries.len() > self.max_entries {
            self.entries.pop_front();
        }

        // Reset navigation
        self.reset_navigation();
    }

    /// Add a command with exit code
    pub fn add_with_result(&mut self, command: String, exit_code: i32) {
        if command.trim().is_empty() {
            return;
        }

        if let Some(last) = self.entries.back() {
            if last.command == command {
                return;
            }
        }

        let entry = HistoryEntry::new(command).with_exit_code(exit_code);
        self.entries.push_back(entry);

        while self.entries.len() > self.max_entries {
            self.entries.pop_front();
        }

        self.reset_navigation();
    }

    /// Get entry at index (0 = oldest)
    pub fn get(&self, index: usize) -> Option<&HistoryEntry> {
        self.entries.get(index)
    }

    /// Get most recent entries
    pub fn recent(&self, count: usize) -> Vec<&HistoryEntry> {
        let len = self.entries.len();
        let start = len.saturating_sub(count);
        self.entries.range(start..).collect()
    }

    /// Get all entries
    pub fn all(&self) -> Vec<&HistoryEntry> {
        self.entries.iter().collect()
    }

    /// Get number of entries
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    /// Clear all history
    pub fn clear(&mut self) {
        self.entries.clear();
        self.reset_navigation();
    }

    /// Search history for commands containing pattern
    pub fn search(&self, pattern: &str) -> Vec<&HistoryEntry> {
        let pattern_lower = pattern.to_lowercase();
        self.entries.iter()
            .filter(|e| e.command.to_lowercase().contains(&pattern_lower))
            .collect()
    }

    /// Search backwards from current position
    pub fn search_backward(&mut self, pattern: &str) -> Option<&str> {
        let pattern_lower = pattern.to_lowercase();
        let start = self.position.unwrap_or(self.entries.len());

        for i in (0..start).rev() {
            if self.entries[i].command.to_lowercase().contains(&pattern_lower) {
                self.position = Some(i);
                return Some(&self.entries[i].command);
            }
        }

        None
    }

    /// Navigate up (to older commands)
    pub fn up(&mut self, current: &str) -> Option<&str> {
        if self.entries.is_empty() {
            return None;
        }

        // Save current line if first navigation
        if self.position.is_none() {
            self.current_line = Some(current.to_string());
            self.position = Some(self.entries.len() - 1);
            return Some(&self.entries.back()?.command);
        }

        // Move up
        let pos = self.position?;
        if pos > 0 {
            self.position = Some(pos - 1);
            return Some(&self.entries[pos - 1].command);
        }

        // Already at oldest
        self.entries.front().map(|e| e.command.as_str())
    }

    /// Navigate down (to newer commands)
    pub fn down(&mut self) -> Option<&str> {
        let pos = self.position?;

        if pos + 1 >= self.entries.len() {
            // Return to current line
            self.reset_navigation();
            return self.current_line.as_deref();
        }

        self.position = Some(pos + 1);
        Some(&self.entries[pos + 1].command)
    }

    /// Reset navigation state
    pub fn reset_navigation(&mut self) {
        self.position = None;
        self.current_line = None;
    }

    /// Get current navigation position
    pub fn navigation_position(&self) -> Option<usize> {
        self.position
    }

    /// Serialize to JSON
    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        let entries: Vec<_> = self.entries.iter().collect();
        serde_json::to_string(&entries)
    }

    /// Deserialize from JSON
    pub fn from_json(json: &str, max_entries: usize) -> Result<Self, serde_json::Error> {
        let entries: Vec<HistoryEntry> = serde_json::from_str(json)?;
        Ok(Self {
            entries: entries.into_iter().collect(),
            max_entries,
            position: None,
            current_line: None,
        })
    }
}

impl Default for History {
    fn default() -> Self {
        Self::new(1000)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_history_add() {
        let mut history = History::new(100);
        history.add("command1".to_string());
        history.add("command2".to_string());

        assert_eq!(history.len(), 2);
    }

    #[test]
    fn test_history_skip_empty() {
        let mut history = History::new(100);
        history.add("".to_string());
        history.add("   ".to_string());

        assert!(history.is_empty());
    }

    #[test]
    fn test_history_skip_duplicates() {
        let mut history = History::new(100);
        history.add("same".to_string());
        history.add("same".to_string());
        history.add("same".to_string());

        assert_eq!(history.len(), 1);
    }

    #[test]
    fn test_history_max_size() {
        let mut history = History::new(3);
        for i in 0..5 {
            history.add(format!("cmd{}", i));
        }

        assert_eq!(history.len(), 3);
        assert_eq!(history.get(0).unwrap().command(), "cmd2");
    }

    #[test]
    fn test_history_navigation() {
        let mut history = History::new(100);
        history.add("first".to_string());
        history.add("second".to_string());
        history.add("third".to_string());

        // Navigate up
        assert_eq!(history.up("current"), Some("third"));
        assert_eq!(history.up("current"), Some("second"));
        assert_eq!(history.up("current"), Some("first"));

        // Navigate down
        assert_eq!(history.down(), Some("second"));
        assert_eq!(history.down(), Some("third"));
    }

    #[test]
    fn test_history_search() {
        let mut history = History::new(100);
        history.add("git status".to_string());
        history.add("git commit".to_string());
        history.add("ls -la".to_string());

        let results = history.search("git");
        assert_eq!(results.len(), 2);
    }

    #[test]
    fn test_history_recent() {
        let mut history = History::new(100);
        for i in 0..10 {
            history.add(format!("cmd{}", i));
        }

        let recent = history.recent(3);
        assert_eq!(recent.len(), 3);
        assert_eq!(recent[0].command(), "cmd7");
        assert_eq!(recent[2].command(), "cmd9");
    }

    #[test]
    fn test_history_serialization() {
        let mut history = History::new(100);
        history.add("cmd1".to_string());
        history.add("cmd2".to_string());

        let json = history.to_json().unwrap();
        let restored = History::from_json(&json, 100).unwrap();

        assert_eq!(restored.len(), 2);
        assert_eq!(restored.get(0).unwrap().command(), "cmd1");
    }
}
