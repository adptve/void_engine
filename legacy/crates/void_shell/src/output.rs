//! Shell output handling
//!
//! Structured output for shell commands.

use serde::{Serialize, Deserialize};

/// Output level/severity
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum OutputLevel {
    /// Debug information
    Debug,
    /// Normal information
    Info,
    /// Warning message
    Warning,
    /// Error message
    Error,
    /// Success message
    Success,
}

impl OutputLevel {
    /// Get ANSI color code for this level
    pub fn color_code(&self) -> &'static str {
        match self {
            Self::Debug => "\x1b[90m",    // Gray
            Self::Info => "\x1b[0m",      // Default
            Self::Warning => "\x1b[33m",  // Yellow
            Self::Error => "\x1b[31m",    // Red
            Self::Success => "\x1b[32m",  // Green
        }
    }

    /// Get level prefix
    pub fn prefix(&self) -> &'static str {
        match self {
            Self::Debug => "[DEBUG]",
            Self::Info => "",
            Self::Warning => "[WARN]",
            Self::Error => "[ERROR]",
            Self::Success => "[OK]",
        }
    }
}

/// A single line of output
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OutputLine {
    /// Output level
    level: OutputLevel,
    /// Text content
    text: String,
    /// Optional timestamp
    timestamp: Option<u64>,
}

impl OutputLine {
    /// Create a new output line
    pub fn new(level: OutputLevel, text: String) -> Self {
        Self {
            level,
            text,
            timestamp: None,
        }
    }

    /// Create with timestamp
    pub fn with_timestamp(level: OutputLevel, text: String) -> Self {
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_secs())
            .ok();

        Self {
            level,
            text,
            timestamp,
        }
    }

    /// Create info line
    pub fn info(text: impl Into<String>) -> Self {
        Self::new(OutputLevel::Info, text.into())
    }

    /// Create error line
    pub fn error(text: impl Into<String>) -> Self {
        Self::new(OutputLevel::Error, text.into())
    }

    /// Create warning line
    pub fn warning(text: impl Into<String>) -> Self {
        Self::new(OutputLevel::Warning, text.into())
    }

    /// Create success line
    pub fn success(text: impl Into<String>) -> Self {
        Self::new(OutputLevel::Success, text.into())
    }

    /// Create debug line
    pub fn debug(text: impl Into<String>) -> Self {
        Self::new(OutputLevel::Debug, text.into())
    }

    /// Get level
    pub fn level(&self) -> OutputLevel {
        self.level
    }

    /// Get text
    pub fn text(&self) -> &str {
        &self.text
    }

    /// Get timestamp
    pub fn timestamp(&self) -> Option<u64> {
        self.timestamp
    }

    /// Format for terminal (with colors)
    pub fn format_colored(&self) -> String {
        let prefix = self.level.prefix();
        let color = self.level.color_code();
        let reset = "\x1b[0m";

        if prefix.is_empty() {
            format!("{}{}{}", color, self.text, reset)
        } else {
            format!("{}{} {}{}", color, prefix, self.text, reset)
        }
    }

    /// Format plain text (no colors)
    pub fn format_plain(&self) -> String {
        let prefix = self.level.prefix();
        if prefix.is_empty() {
            self.text.clone()
        } else {
            format!("{} {}", prefix, self.text)
        }
    }
}

impl std::fmt::Display for OutputLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.format_plain())
    }
}

/// Collection of output lines
#[derive(Debug, Clone, Default)]
pub struct Output {
    lines: Vec<OutputLine>,
}

impl Output {
    /// Create empty output
    pub fn new() -> Self {
        Self { lines: Vec::new() }
    }

    /// Create with single message
    pub fn message(text: impl Into<String>) -> Self {
        Self {
            lines: vec![OutputLine::info(text)],
        }
    }

    /// Add a line
    pub fn add_line(&mut self, line: OutputLine) {
        self.lines.push(line);
    }

    /// Add info line
    pub fn add_info(&mut self, text: impl Into<String>) {
        self.lines.push(OutputLine::info(text));
    }

    /// Add error line
    pub fn add_error(&mut self, text: impl Into<String>) {
        self.lines.push(OutputLine::error(text));
    }

    /// Add warning line
    pub fn add_warning(&mut self, text: impl Into<String>) {
        self.lines.push(OutputLine::warning(text));
    }

    /// Add success line
    pub fn add_success(&mut self, text: impl Into<String>) {
        self.lines.push(OutputLine::success(text));
    }

    /// Get all lines
    pub fn lines(&self) -> &[OutputLine] {
        &self.lines
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.lines.is_empty()
    }

    /// Get line count
    pub fn len(&self) -> usize {
        self.lines.len()
    }

    /// Append another output
    pub fn append(&mut self, other: Output) {
        self.lines.extend(other.lines);
    }

    /// Format all lines (colored)
    pub fn format_colored(&self) -> String {
        self.lines.iter()
            .map(|l| l.format_colored())
            .collect::<Vec<_>>()
            .join("\n")
    }

    /// Format all lines (plain)
    pub fn format_plain(&self) -> String {
        self.lines.iter()
            .map(|l| l.format_plain())
            .collect::<Vec<_>>()
            .join("\n")
    }

    /// Filter by level
    pub fn filter_level(&self, level: OutputLevel) -> Vec<&OutputLine> {
        self.lines.iter().filter(|l| l.level == level).collect()
    }

    /// Check if any errors
    pub fn has_errors(&self) -> bool {
        self.lines.iter().any(|l| l.level == OutputLevel::Error)
    }

    /// Check if any warnings
    pub fn has_warnings(&self) -> bool {
        self.lines.iter().any(|l| l.level == OutputLevel::Warning)
    }
}

impl std::fmt::Display for Output {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.format_plain())
    }
}

impl IntoIterator for Output {
    type Item = OutputLine;
    type IntoIter = std::vec::IntoIter<OutputLine>;

    fn into_iter(self) -> Self::IntoIter {
        self.lines.into_iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_output_line() {
        let line = OutputLine::info("Hello, world!");
        assert_eq!(line.level(), OutputLevel::Info);
        assert_eq!(line.text(), "Hello, world!");
    }

    #[test]
    fn test_output_line_formatting() {
        let line = OutputLine::error("Something went wrong");
        assert!(line.format_plain().contains("[ERROR]"));
        assert!(line.format_colored().contains("\x1b[31m")); // Red
    }

    #[test]
    fn test_output() {
        let mut output = Output::new();
        output.add_info("Line 1");
        output.add_warning("Line 2");
        output.add_error("Line 3");

        assert_eq!(output.len(), 3);
        assert!(output.has_errors());
        assert!(output.has_warnings());
    }

    #[test]
    fn test_output_filter() {
        let mut output = Output::new();
        output.add_info("Info 1");
        output.add_info("Info 2");
        output.add_error("Error 1");

        let errors = output.filter_level(OutputLevel::Error);
        assert_eq!(errors.len(), 1);
    }
}
