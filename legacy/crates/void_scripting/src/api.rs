//! Host API that plugins can call

use void_ecs::prelude::Entity;
use void_math::prelude::Vec3;
use crate::context::{PluginValue, SpawnRequest};
use std::collections::HashMap;

/// Commands that plugins can issue to the host
#[derive(Debug, Clone)]
pub enum HostCommand {
    /// Spawn a new entity
    Spawn(SpawnRequest),
    /// Destroy an entity
    Despawn(Entity),
    /// Set entity position
    SetPosition { entity: Entity, position: Vec3 },
    /// Set entity rotation
    SetRotation { entity: Entity, rotation: Vec3 },
    /// Set entity scale
    SetScale { entity: Entity, scale: Vec3 },
    /// Apply force to entity (physics)
    ApplyForce { entity: Entity, force: Vec3 },
    /// Apply impulse to entity (physics)
    ApplyImpulse { entity: Entity, impulse: Vec3 },
    /// Set entity velocity
    SetVelocity { entity: Entity, velocity: Vec3 },
    /// Play a sound effect
    PlaySound { name: String, position: Option<Vec3> },
    /// Play animation on entity
    PlayAnimation { entity: Entity, name: String, looping: bool },
    /// Send message to entity
    SendMessage { target: Option<Entity>, message_type: String, data: PluginValue },
    /// Log a message
    Log { level: LogLevel, message: String },
    /// Store persistent data
    SetData { key: String, value: PluginValue },
    /// Raycast query
    Raycast { origin: Vec3, direction: Vec3, max_distance: f32 },
}

/// Log levels for plugin messages
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LogLevel {
    Debug,
    Info,
    Warn,
    Error,
}

/// Host API interface that plugins call into
///
/// This trait defines all functions available to plugins.
/// The implementation translates these to actual engine operations.
pub trait HostApi {
    // =========================================================================
    // Entity queries
    // =========================================================================

    /// Get entity position
    fn get_position(&self, entity: Entity) -> Option<Vec3>;

    /// Get entity rotation (euler angles)
    fn get_rotation(&self, entity: Entity) -> Option<Vec3>;

    /// Get entity scale
    fn get_scale(&self, entity: Entity) -> Option<Vec3>;

    /// Check if entity exists and is active
    fn entity_exists(&self, entity: Entity) -> bool;

    /// Get entities within radius of a point
    fn get_entities_in_radius(&self, center: Vec3, radius: f32) -> Vec<Entity>;

    // =========================================================================
    // Input queries
    // =========================================================================

    /// Check if a key is currently pressed
    fn is_key_pressed(&self, key: &str) -> bool;

    /// Check if a mouse button is pressed
    fn is_mouse_pressed(&self, button: u32) -> bool;

    /// Get mouse position in screen coordinates
    fn get_mouse_position(&self) -> (f32, f32);

    /// Get mouse delta since last frame
    fn get_mouse_delta(&self) -> (f32, f32);

    // =========================================================================
    // Time queries
    // =========================================================================

    /// Get current time in seconds since start
    fn get_time(&self) -> f64;

    /// Get delta time for this frame
    fn get_delta_time(&self) -> f32;

    // =========================================================================
    // Data storage
    // =========================================================================

    /// Get stored data value
    fn get_data(&self, key: &str) -> Option<PluginValue>;

    /// Get entity-specific data
    fn get_entity_data(&self, entity: Entity, key: &str) -> Option<PluginValue>;

    // =========================================================================
    // Random
    // =========================================================================

    /// Get a random float between 0 and 1
    fn random(&self) -> f32;

    /// Get a random float in range
    fn random_range(&self, min: f32, max: f32) -> f32;
}

/// Default implementation that provides basic functionality
pub struct DefaultHostApi {
    time: f64,
    delta_time: f32,
    pressed_keys: std::collections::HashSet<String>,
    mouse_position: (f32, f32),
    mouse_delta: (f32, f32),
    mouse_buttons: u32,
    data: HashMap<String, PluginValue>,
}

impl Default for DefaultHostApi {
    fn default() -> Self {
        Self {
            time: 0.0,
            delta_time: 0.016,
            pressed_keys: std::collections::HashSet::new(),
            mouse_position: (0.0, 0.0),
            mouse_delta: (0.0, 0.0),
            mouse_buttons: 0,
            data: HashMap::new(),
        }
    }
}

impl DefaultHostApi {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn update(&mut self, dt: f32) {
        self.delta_time = dt;
        self.time += dt as f64;
    }

    pub fn set_key_pressed(&mut self, key: &str, pressed: bool) {
        if pressed {
            self.pressed_keys.insert(key.to_string());
        } else {
            self.pressed_keys.remove(key);
        }
    }

    pub fn set_mouse(&mut self, x: f32, y: f32, dx: f32, dy: f32, buttons: u32) {
        self.mouse_position = (x, y);
        self.mouse_delta = (dx, dy);
        self.mouse_buttons = buttons;
    }

    pub fn set_data(&mut self, key: String, value: PluginValue) {
        self.data.insert(key, value);
    }
}

impl HostApi for DefaultHostApi {
    fn get_position(&self, _entity: Entity) -> Option<Vec3> {
        // Would query ECS world
        None
    }

    fn get_rotation(&self, _entity: Entity) -> Option<Vec3> {
        None
    }

    fn get_scale(&self, _entity: Entity) -> Option<Vec3> {
        None
    }

    fn entity_exists(&self, _entity: Entity) -> bool {
        false
    }

    fn get_entities_in_radius(&self, _center: Vec3, _radius: f32) -> Vec<Entity> {
        Vec::new()
    }

    fn is_key_pressed(&self, key: &str) -> bool {
        self.pressed_keys.contains(key)
    }

    fn is_mouse_pressed(&self, button: u32) -> bool {
        (self.mouse_buttons & (1 << button)) != 0
    }

    fn get_mouse_position(&self) -> (f32, f32) {
        self.mouse_position
    }

    fn get_mouse_delta(&self) -> (f32, f32) {
        self.mouse_delta
    }

    fn get_time(&self) -> f64 {
        self.time
    }

    fn get_delta_time(&self) -> f32 {
        self.delta_time
    }

    fn get_data(&self, key: &str) -> Option<PluginValue> {
        self.data.get(key).cloned()
    }

    fn get_entity_data(&self, _entity: Entity, _key: &str) -> Option<PluginValue> {
        None
    }

    fn random(&self) -> f32 {
        // Simple pseudo-random (would use proper RNG in production)
        let t = self.time * 12.9898;
        let s = (t.sin() * 43758.5453).fract();
        s as f32
    }

    fn random_range(&self, min: f32, max: f32) -> f32 {
        min + self.random() * (max - min)
    }
}
