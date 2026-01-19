//! Rigid body types and components

use rapier3d::prelude as rapier;
use rapier3d::na::{UnitQuaternion, Quaternion};
use serde::{Deserialize, Serialize};

/// Handle to a rigid body in the physics world
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct RigidBodyHandle(pub(crate) rapier::RigidBodyHandle);

impl RigidBodyHandle {
    /// Create from raw Rapier handle
    pub fn from_raw(handle: rapier::RigidBodyHandle) -> Self {
        Self(handle)
    }

    /// Get the raw Rapier handle
    pub fn raw(&self) -> rapier::RigidBodyHandle {
        self.0
    }
}

/// Type of rigid body
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
pub enum RigidBodyType {
    /// Static body - never moves, infinite mass
    Static,
    /// Dynamic body - fully simulated
    #[default]
    Dynamic,
    /// Kinematic position-based - moved by user, affects dynamic bodies
    KinematicPositionBased,
    /// Kinematic velocity-based - velocity set by user
    KinematicVelocityBased,
}

impl From<RigidBodyType> for rapier::RigidBodyType {
    fn from(t: RigidBodyType) -> Self {
        match t {
            RigidBodyType::Static => rapier::RigidBodyType::Fixed,
            RigidBodyType::Dynamic => rapier::RigidBodyType::Dynamic,
            RigidBodyType::KinematicPositionBased => rapier::RigidBodyType::KinematicPositionBased,
            RigidBodyType::KinematicVelocityBased => rapier::RigidBodyType::KinematicVelocityBased,
        }
    }
}

/// Constraints on rigid body motion
#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize)]
pub struct RigidBodyConstraints {
    /// Lock translation on X axis
    pub lock_translation_x: bool,
    /// Lock translation on Y axis
    pub lock_translation_y: bool,
    /// Lock translation on Z axis
    pub lock_translation_z: bool,
    /// Lock rotation on X axis
    pub lock_rotation_x: bool,
    /// Lock rotation on Y axis
    pub lock_rotation_y: bool,
    /// Lock rotation on Z axis
    pub lock_rotation_z: bool,
}

impl RigidBodyConstraints {
    /// No constraints
    pub const NONE: Self = Self {
        lock_translation_x: false,
        lock_translation_y: false,
        lock_translation_z: false,
        lock_rotation_x: false,
        lock_rotation_y: false,
        lock_rotation_z: false,
    };

    /// Lock all rotation (2D-like simulation)
    pub const LOCK_ROTATION: Self = Self {
        lock_translation_x: false,
        lock_translation_y: false,
        lock_translation_z: false,
        lock_rotation_x: true,
        lock_rotation_y: true,
        lock_rotation_z: true,
    };

    /// Lock Y rotation only (common for characters)
    pub const LOCK_ROTATION_Y: Self = Self {
        lock_translation_x: false,
        lock_translation_y: false,
        lock_translation_z: false,
        lock_rotation_x: false,
        lock_rotation_y: true,
        lock_rotation_z: false,
    };

    /// Convert to Rapier locked axes
    pub fn to_rapier(&self) -> rapier::LockedAxes {
        let mut axes = rapier::LockedAxes::empty();
        if self.lock_translation_x {
            axes |= rapier::LockedAxes::TRANSLATION_LOCKED_X;
        }
        if self.lock_translation_y {
            axes |= rapier::LockedAxes::TRANSLATION_LOCKED_Y;
        }
        if self.lock_translation_z {
            axes |= rapier::LockedAxes::TRANSLATION_LOCKED_Z;
        }
        if self.lock_rotation_x {
            axes |= rapier::LockedAxes::ROTATION_LOCKED_X;
        }
        if self.lock_rotation_y {
            axes |= rapier::LockedAxes::ROTATION_LOCKED_Y;
        }
        if self.lock_rotation_z {
            axes |= rapier::LockedAxes::ROTATION_LOCKED_Z;
        }
        axes
    }
}

/// Description for creating a rigid body
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RigidBodyDesc {
    /// Type of rigid body
    pub body_type: RigidBodyType,
    /// Initial position
    pub position: [f32; 3],
    /// Initial rotation (quaternion: x, y, z, w)
    pub rotation: [f32; 4],
    /// Initial linear velocity
    pub linear_velocity: [f32; 3],
    /// Initial angular velocity
    pub angular_velocity: [f32; 3],
    /// Gravity scale (0 = no gravity, 1 = normal, 2 = double)
    pub gravity_scale: f32,
    /// Linear damping (air resistance)
    pub linear_damping: f32,
    /// Angular damping (rotational resistance)
    pub angular_damping: f32,
    /// Mass (if 0, calculated from colliders)
    pub mass: f32,
    /// Motion constraints
    pub constraints: RigidBodyConstraints,
    /// Enable continuous collision detection
    pub ccd_enabled: bool,
    /// Can this body sleep when inactive
    pub can_sleep: bool,
    /// Is this body currently sleeping
    pub sleeping: bool,
}

impl Default for RigidBodyDesc {
    fn default() -> Self {
        Self {
            body_type: RigidBodyType::Dynamic,
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0, 1.0],
            linear_velocity: [0.0, 0.0, 0.0],
            angular_velocity: [0.0, 0.0, 0.0],
            gravity_scale: 1.0,
            linear_damping: 0.0,
            angular_damping: 0.0,
            mass: 0.0,
            constraints: RigidBodyConstraints::NONE,
            ccd_enabled: false,
            can_sleep: true,
            sleeping: false,
        }
    }
}

impl RigidBodyDesc {
    /// Create a static body description
    pub fn fixed() -> Self {
        Self {
            body_type: RigidBodyType::Static,
            ..Default::default()
        }
    }

    /// Create a dynamic body description
    pub fn dynamic() -> Self {
        Self {
            body_type: RigidBodyType::Dynamic,
            ..Default::default()
        }
    }

    /// Create a kinematic body description
    pub fn kinematic() -> Self {
        Self {
            body_type: RigidBodyType::KinematicPositionBased,
            ..Default::default()
        }
    }

    /// Set position
    pub fn with_position(mut self, x: f32, y: f32, z: f32) -> Self {
        self.position = [x, y, z];
        self
    }

    /// Set rotation from euler angles (radians)
    pub fn with_rotation_euler(mut self, x: f32, y: f32, z: f32) -> Self {
        let quat = euler_to_quat(x, y, z);
        self.rotation = quat;
        self
    }

    /// Set linear velocity
    pub fn with_linear_velocity(mut self, x: f32, y: f32, z: f32) -> Self {
        self.linear_velocity = [x, y, z];
        self
    }

    /// Set gravity scale
    pub fn with_gravity_scale(mut self, scale: f32) -> Self {
        self.gravity_scale = scale;
        self
    }

    /// Set mass
    pub fn with_mass(mut self, mass: f32) -> Self {
        self.mass = mass;
        self
    }

    /// Set constraints
    pub fn with_constraints(mut self, constraints: RigidBodyConstraints) -> Self {
        self.constraints = constraints;
        self
    }

    /// Enable CCD
    pub fn with_ccd(mut self, enabled: bool) -> Self {
        self.ccd_enabled = enabled;
        self
    }

    /// Build a Rapier rigid body builder
    pub(crate) fn to_rapier_builder(&self) -> rapier::RigidBodyBuilder {
        let mut builder = rapier::RigidBodyBuilder::new(self.body_type.into())
            .translation(rapier::Vector::new(
                self.position[0],
                self.position[1],
                self.position[2],
            ))
            .rotation(rapier::Vector::new(
                self.rotation[0],
                self.rotation[1],
                self.rotation[2],
            ))
            .linvel(rapier::Vector::new(
                self.linear_velocity[0],
                self.linear_velocity[1],
                self.linear_velocity[2],
            ))
            .angvel(rapier::Vector::new(
                self.angular_velocity[0],
                self.angular_velocity[1],
                self.angular_velocity[2],
            ))
            .gravity_scale(self.gravity_scale)
            .linear_damping(self.linear_damping)
            .angular_damping(self.angular_damping)
            .locked_axes(self.constraints.to_rapier())
            .ccd_enabled(self.ccd_enabled)
            .can_sleep(self.can_sleep);

        if self.sleeping {
            builder = builder.sleeping(true);
        }

        builder
    }
}

/// ECS Component for rigid bodies
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RigidBodyComponent {
    /// Body description
    pub desc: RigidBodyDesc,
    /// Handle to physics body (set at runtime)
    #[serde(skip)]
    pub handle: Option<RigidBodyHandle>,
    /// Whether the component is initialized
    #[serde(skip)]
    pub initialized: bool,
}

impl RigidBodyComponent {
    /// Create a new rigid body component
    pub fn new(desc: RigidBodyDesc) -> Self {
        Self {
            desc,
            handle: None,
            initialized: false,
        }
    }

    /// Create a static body component
    pub fn fixed() -> Self {
        Self::new(RigidBodyDesc::fixed())
    }

    /// Create a dynamic body component
    pub fn dynamic() -> Self {
        Self::new(RigidBodyDesc::dynamic())
    }

    /// Create a kinematic body component
    pub fn kinematic() -> Self {
        Self::new(RigidBodyDesc::kinematic())
    }
}

impl Default for RigidBodyComponent {
    fn default() -> Self {
        Self::new(RigidBodyDesc::default())
    }
}

/// Convert euler angles to quaternion
fn euler_to_quat(x: f32, y: f32, z: f32) -> [f32; 4] {
    let (sx, cx) = (x * 0.5).sin_cos();
    let (sy, cy) = (y * 0.5).sin_cos();
    let (sz, cz) = (z * 0.5).sin_cos();

    [
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    ]
}
