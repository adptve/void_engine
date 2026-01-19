//! Plugin types and management

use std::path::PathBuf;

/// Unique identifier for a loaded plugin
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct PluginId(pub(crate) u32);

/// Information about a loaded plugin
#[derive(Debug, Clone)]
pub struct PluginInfo {
    /// Plugin identifier
    pub id: PluginId,
    /// Original file path
    pub path: PathBuf,
    /// Plugin name (from metadata or filename)
    pub name: String,
    /// Plugin version (if specified in metadata)
    pub version: Option<String>,
    /// Current state
    pub state: PluginState,
    /// Available exported functions
    pub exports: PluginExports,
}

/// Current state of a plugin
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PluginState {
    /// Plugin is loaded and ready
    Ready,
    /// Plugin is currently executing
    Running,
    /// Plugin encountered an error
    Error,
    /// Plugin has been unloaded
    Unloaded,
}

/// Flags indicating which functions the plugin exports
#[derive(Debug, Clone, Copy, Default)]
pub struct PluginExports {
    /// Has `on_spawn` function
    pub on_spawn: bool,
    /// Has `on_update` function
    pub on_update: bool,
    /// Has `on_interact` function
    pub on_interact: bool,
    /// Has `on_collision` function
    pub on_collision: bool,
    /// Has `on_destroy` function
    pub on_destroy: bool,
    /// Has `on_input` function
    pub on_input: bool,
    /// Has `on_message` function
    pub on_message: bool,
}

impl PluginExports {
    /// Check if any update-related functions are exported
    pub fn needs_update(&self) -> bool {
        self.on_update || self.on_input
    }
}
