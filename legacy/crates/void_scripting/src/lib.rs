//! # Metaverse Scripting
//!
//! WASM-based behavior plugin system for dynamic entity behaviors.
//!
//! ## Overview
//!
//! Plugins are WASM modules that can be loaded at runtime to add custom
//! behaviors to entities. A gun mesh can have a `gun.wasm` plugin that
//! handles shooting, reloading, etc.
//!
//! ## Plugin API
//!
//! Plugins export functions that the engine calls:
//! - `on_spawn(entity_id)` - Called when entity is created
//! - `on_update(entity_id, dt)` - Called every frame
//! - `on_interact(entity_id, other_id)` - Called on interaction
//! - `on_destroy(entity_id)` - Called when entity is destroyed
//!
//! Plugins can call host functions:
//! - `spawn_entity(prefab, x, y, z)` - Spawn a new entity
//! - `get_position(entity_id)` - Get entity position
//! - `set_position(entity_id, x, y, z)` - Set entity position
//! - `apply_force(entity_id, fx, fy, fz)` - Apply physics force
//! - `play_sound(name)` - Play a sound effect
//! - `log(message)` - Log a message
//!
//! ## Example
//!
//! ```ignore
//! use void_scripting::{PluginHost, PluginId};
//!
//! let mut host = PluginHost::new();
//! let plugin_id = host.load_plugin("behaviors/gun.wasm")?;
//!
//! // When entity spawns
//! host.call_on_spawn(plugin_id, entity_id);
//!
//! // Every frame
//! host.call_on_update(plugin_id, entity_id, delta_time);
//! ```

mod host;
mod plugin;
mod api;
mod context;

pub use host::{PluginHost, PluginHostConfig};
pub use plugin::{PluginId, PluginInfo, PluginState};
pub use api::{HostApi, HostCommand};
pub use context::{PluginContext, EntityRef};

/// Result type for scripting operations
pub type Result<T> = std::result::Result<T, ScriptError>;

/// Errors that can occur in the scripting system
#[derive(Debug, Clone)]
pub enum ScriptError {
    /// Failed to load WASM module
    LoadError(String),
    /// Failed to instantiate module
    InstantiationError(String),
    /// Function call failed
    CallError(String),
    /// Plugin not found
    PluginNotFound(PluginId),
    /// Invalid plugin state
    InvalidState(String),
    /// IO error
    IoError(String),
}

impl std::fmt::Display for ScriptError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ScriptError::LoadError(s) => write!(f, "Load error: {}", s),
            ScriptError::InstantiationError(s) => write!(f, "Instantiation error: {}", s),
            ScriptError::CallError(s) => write!(f, "Call error: {}", s),
            ScriptError::PluginNotFound(id) => write!(f, "Plugin not found: {:?}", id),
            ScriptError::InvalidState(s) => write!(f, "Invalid state: {}", s),
            ScriptError::IoError(s) => write!(f, "IO error: {}", s),
        }
    }
}

impl std::error::Error for ScriptError {}
