//! Physics configuration

use serde::{Deserialize, Serialize};

/// Physics world configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PhysicsConfig {
    /// Gravity vector (default: -9.81 in Y)
    pub gravity: [f32; 3],

    /// Fixed timestep for physics simulation
    pub timestep: f32,

    /// Maximum number of substeps per frame
    pub max_substeps: u32,

    /// Enable continuous collision detection
    pub ccd_enabled: bool,

    /// Default friction coefficient
    pub default_friction: f32,

    /// Default restitution (bounciness)
    pub default_restitution: f32,

    /// Solver iterations for velocity
    pub velocity_iterations: usize,

    /// Solver iterations for position
    pub position_iterations: usize,

    /// Enable sleeping for inactive bodies
    pub sleeping_enabled: bool,

    /// Linear velocity threshold for sleeping
    pub sleep_linear_threshold: f32,

    /// Angular velocity threshold for sleeping
    pub sleep_angular_threshold: f32,
}

impl Default for PhysicsConfig {
    fn default() -> Self {
        Self {
            gravity: [0.0, -9.81, 0.0],
            timestep: 1.0 / 60.0,
            max_substeps: 4,
            ccd_enabled: true,
            default_friction: 0.5,
            default_restitution: 0.0,
            velocity_iterations: 4,
            position_iterations: 1,
            sleeping_enabled: true,
            sleep_linear_threshold: 0.1,
            sleep_angular_threshold: 0.1,
        }
    }
}

impl PhysicsConfig {
    /// Create a configuration for high-precision simulation
    pub fn high_precision() -> Self {
        Self {
            velocity_iterations: 8,
            position_iterations: 4,
            max_substeps: 8,
            ..Default::default()
        }
    }

    /// Create a configuration for fast simulation (lower quality)
    pub fn fast() -> Self {
        Self {
            velocity_iterations: 2,
            position_iterations: 1,
            max_substeps: 2,
            ccd_enabled: false,
            ..Default::default()
        }
    }

    /// Set gravity
    pub fn with_gravity(mut self, x: f32, y: f32, z: f32) -> Self {
        self.gravity = [x, y, z];
        self
    }

    /// Set timestep
    pub fn with_timestep(mut self, timestep: f32) -> Self {
        self.timestep = timestep;
        self
    }
}
