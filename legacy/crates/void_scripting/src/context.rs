//! Plugin execution context

use void_ecs::prelude::Entity;
use void_math::prelude::Vec3;
use std::collections::HashMap;

/// Context passed to plugins during execution
///
/// This provides a safe, limited view of the world that plugins can query
/// and modify through the host API.
#[derive(Debug, Default)]
pub struct PluginContext {
    /// Entities managed by this plugin
    pub entities: Vec<EntityRef>,
    /// Pending spawn requests
    pub spawn_queue: Vec<SpawnRequest>,
    /// Pending despawn requests
    pub despawn_queue: Vec<Entity>,
    /// Messages to send to other entities
    pub message_queue: Vec<EntityMessage>,
    /// Sound effects to play
    pub sound_queue: Vec<String>,
    /// Custom data storage (string key-value)
    pub data: HashMap<String, PluginValue>,
}

/// Reference to an entity with cached data
#[derive(Debug, Clone)]
pub struct EntityRef {
    /// Entity ID
    pub entity: Entity,
    /// Cached position
    pub position: Vec3,
    /// Cached rotation (euler angles)
    pub rotation: Vec3,
    /// Cached scale
    pub scale: Vec3,
    /// Entity is active
    pub active: bool,
}

/// Request to spawn a new entity
#[derive(Debug, Clone)]
pub struct SpawnRequest {
    /// Prefab name or mesh path
    pub prefab: String,
    /// Initial position
    pub position: Vec3,
    /// Initial rotation
    pub rotation: Vec3,
    /// Initial velocity (for projectiles)
    pub velocity: Option<Vec3>,
    /// Plugin to attach to spawned entity
    pub plugin: Option<String>,
    /// Custom properties
    pub properties: HashMap<String, PluginValue>,
}

/// Message between entities
#[derive(Debug, Clone)]
pub struct EntityMessage {
    /// Target entity (or None for broadcast)
    pub target: Option<Entity>,
    /// Message type
    pub message_type: String,
    /// Message data
    pub data: PluginValue,
}

/// Dynamic value type for plugin data
#[derive(Debug, Clone)]
pub enum PluginValue {
    Null,
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    Vec3([f64; 3]),
    Array(Vec<PluginValue>),
    Object(HashMap<String, PluginValue>),
}

impl PluginValue {
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            PluginValue::Bool(b) => Some(*b),
            _ => None,
        }
    }

    pub fn as_int(&self) -> Option<i64> {
        match self {
            PluginValue::Int(i) => Some(*i),
            PluginValue::Float(f) => Some(*f as i64),
            _ => None,
        }
    }

    pub fn as_float(&self) -> Option<f64> {
        match self {
            PluginValue::Float(f) => Some(*f),
            PluginValue::Int(i) => Some(*i as f64),
            _ => None,
        }
    }

    pub fn as_string(&self) -> Option<&str> {
        match self {
            PluginValue::String(s) => Some(s),
            _ => None,
        }
    }

    pub fn as_vec3(&self) -> Option<Vec3> {
        match self {
            PluginValue::Vec3(v) => Some(Vec3::new(v[0] as f32, v[1] as f32, v[2] as f32)),
            _ => None,
        }
    }
}

impl Default for PluginValue {
    fn default() -> Self {
        PluginValue::Null
    }
}

impl From<bool> for PluginValue {
    fn from(v: bool) -> Self {
        PluginValue::Bool(v)
    }
}

impl From<i64> for PluginValue {
    fn from(v: i64) -> Self {
        PluginValue::Int(v)
    }
}

impl From<f64> for PluginValue {
    fn from(v: f64) -> Self {
        PluginValue::Float(v)
    }
}

impl From<String> for PluginValue {
    fn from(v: String) -> Self {
        PluginValue::String(v)
    }
}

impl From<Vec3> for PluginValue {
    fn from(v: Vec3) -> Self {
        PluginValue::Vec3([v.x as f64, v.y as f64, v.z as f64])
    }
}
