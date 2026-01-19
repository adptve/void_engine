//! Console/log panel.
//!
//! Displays log messages with filtering by level.

use std::collections::VecDeque;
use std::time::Instant;

/// Log level for console messages.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LogLevel {
    Info,
    Warning,
    Error,
    Debug,
}

impl LogLevel {
    pub fn color(&self) -> egui::Color32 {
        match self {
            LogLevel::Info => egui::Color32::WHITE,
            LogLevel::Warning => egui::Color32::YELLOW,
            LogLevel::Error => egui::Color32::from_rgb(255, 100, 100),
            LogLevel::Debug => egui::Color32::from_rgb(150, 150, 255),
        }
    }

    pub fn prefix(&self) -> &'static str {
        match self {
            LogLevel::Info => "[INFO]",
            LogLevel::Warning => "[WARN]",
            LogLevel::Error => "[ERROR]",
            LogLevel::Debug => "[DEBUG]",
        }
    }
}

/// A single log entry.
#[derive(Clone, Debug)]
pub struct LogEntry {
    pub level: LogLevel,
    pub message: String,
    pub timestamp: Instant,
    pub count: u32, // For collapsed duplicate messages
}

impl LogEntry {
    pub fn new(level: LogLevel, message: impl Into<String>) -> Self {
        Self {
            level,
            message: message.into(),
            timestamp: Instant::now(),
            count: 1,
        }
    }
}

/// Console panel state.
#[derive(Debug)]
pub struct Console {
    entries: VecDeque<LogEntry>,
    max_entries: usize,

    // Filters
    pub show_info: bool,
    pub show_warnings: bool,
    pub show_errors: bool,
    pub show_debug: bool,
    pub filter_text: String,

    // Options
    pub auto_scroll: bool,
    pub collapse_duplicates: bool,
    pub show_timestamps: bool,
}

impl Default for Console {
    fn default() -> Self {
        Self {
            entries: VecDeque::new(),
            max_entries: 1000,
            show_info: true,
            show_warnings: true,
            show_errors: true,
            show_debug: true,
            filter_text: String::new(),
            auto_scroll: true,
            collapse_duplicates: true,
            show_timestamps: false,
        }
    }
}

impl Console {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_capacity(max_entries: usize) -> Self {
        Self {
            max_entries,
            ..Default::default()
        }
    }

    /// Log a message with the given level.
    pub fn log(&mut self, level: LogLevel, message: impl Into<String>) {
        let message = message.into();

        // Collapse duplicates if enabled
        if self.collapse_duplicates {
            if let Some(last) = self.entries.back_mut() {
                if last.level == level && last.message == message {
                    last.count += 1;
                    last.timestamp = Instant::now();
                    return;
                }
            }
        }

        let entry = LogEntry::new(level, message);
        self.entries.push_back(entry);

        // Trim if over limit
        while self.entries.len() > self.max_entries {
            self.entries.pop_front();
        }
    }

    /// Log an info message.
    pub fn info(&mut self, message: impl Into<String>) {
        self.log(LogLevel::Info, message);
    }

    /// Log a warning message.
    pub fn warn(&mut self, message: impl Into<String>) {
        self.log(LogLevel::Warning, message);
    }

    /// Log an error message.
    pub fn error(&mut self, message: impl Into<String>) {
        self.log(LogLevel::Error, message);
    }

    /// Log a debug message.
    pub fn debug(&mut self, message: impl Into<String>) {
        self.log(LogLevel::Debug, message);
    }

    /// Clear all entries.
    pub fn clear(&mut self) {
        self.entries.clear();
    }

    /// Check if an entry should be shown based on current filters.
    pub fn should_show(&self, entry: &LogEntry) -> bool {
        let level_ok = match entry.level {
            LogLevel::Info => self.show_info,
            LogLevel::Warning => self.show_warnings,
            LogLevel::Error => self.show_errors,
            LogLevel::Debug => self.show_debug,
        };

        let filter_ok = self.filter_text.is_empty()
            || entry.message.to_lowercase().contains(&self.filter_text.to_lowercase());

        level_ok && filter_ok
    }

    /// Get all entries.
    pub fn entries(&self) -> &VecDeque<LogEntry> {
        &self.entries
    }

    /// Get filtered entries.
    pub fn filtered_entries(&self) -> impl Iterator<Item = &LogEntry> {
        self.entries.iter().filter(|e| self.should_show(e))
    }

    /// Get entry count.
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// Check if console is empty.
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    /// Count entries by level.
    pub fn count_by_level(&self) -> (usize, usize, usize, usize) {
        let mut info = 0;
        let mut warn = 0;
        let mut error = 0;
        let mut debug = 0;

        for entry in &self.entries {
            match entry.level {
                LogLevel::Info => info += entry.count as usize,
                LogLevel::Warning => warn += entry.count as usize,
                LogLevel::Error => error += entry.count as usize,
                LogLevel::Debug => debug += entry.count as usize,
            }
        }

        (info, warn, error, debug)
    }
}
