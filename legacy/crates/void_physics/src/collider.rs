//! Collider types and components

use crate::body::RigidBodyHandle;
use crate::layers::CollisionGroups;
use crate::material::PhysicsMaterial;
use rapier3d::prelude as rapier;
use serde::{Deserialize, Serialize};

/// Handle to a collider in the physics world
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ColliderHandle(pub(crate) rapier::ColliderHandle);

impl ColliderHandle {
    /// Create from raw Rapier handle
    pub fn from_raw(handle: rapier::ColliderHandle) -> Self {
        Self(handle)
    }

    /// Get the raw Rapier handle
    pub fn raw(&self) -> rapier::ColliderHandle {
        self.0
    }
}

/// Collision shape type
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ColliderShape {
    /// Sphere with radius
    Sphere {
        radius: f32,
    },
    /// Box with half-extents
    Box {
        half_extents: [f32; 3],
    },
    /// Capsule aligned along Y axis
    CapsuleY {
        half_height: f32,
        radius: f32,
    },
    /// Capsule aligned along X axis
    CapsuleX {
        half_height: f32,
        radius: f32,
    },
    /// Capsule aligned along Z axis
    CapsuleZ {
        half_height: f32,
        radius: f32,
    },
    /// Cylinder aligned along Y axis
    CylinderY {
        half_height: f32,
        radius: f32,
    },
    /// Cone aligned along Y axis
    ConeY {
        half_height: f32,
        radius: f32,
    },
    /// Convex hull from points
    ConvexHull {
        points: Vec<[f32; 3]>,
    },
    /// Triangle mesh (static only)
    TriMesh {
        vertices: Vec<[f32; 3]>,
        indices: Vec<[u32; 3]>,
    },
    /// Heightfield terrain
    HeightField {
        heights: Vec<f32>,
        rows: usize,
        cols: usize,
        scale: [f32; 3],
    },
}

impl Default for ColliderShape {
    fn default() -> Self {
        Self::Box {
            half_extents: [0.5, 0.5, 0.5],
        }
    }
}

impl ColliderShape {
    /// Create a sphere shape
    pub fn sphere(radius: f32) -> Self {
        Self::Sphere { radius }
    }

    /// Create a box shape from half-extents
    pub fn cuboid(hx: f32, hy: f32, hz: f32) -> Self {
        Self::Box {
            half_extents: [hx, hy, hz],
        }
    }

    /// Create a box shape from full size
    pub fn from_size(width: f32, height: f32, depth: f32) -> Self {
        Self::Box {
            half_extents: [width * 0.5, height * 0.5, depth * 0.5],
        }
    }

    /// Create a capsule shape (Y-aligned)
    pub fn capsule(half_height: f32, radius: f32) -> Self {
        Self::CapsuleY { half_height, radius }
    }

    /// Create a cylinder shape (Y-aligned)
    pub fn cylinder(half_height: f32, radius: f32) -> Self {
        Self::CylinderY { half_height, radius }
    }

    /// Build a Rapier shared shape
    pub(crate) fn to_rapier(&self) -> rapier::SharedShape {
        match self {
            Self::Sphere { radius } => rapier::SharedShape::ball(*radius),
            Self::Box { half_extents } => {
                rapier::SharedShape::cuboid(half_extents[0], half_extents[1], half_extents[2])
            }
            Self::CapsuleY { half_height, radius } => {
                rapier::SharedShape::capsule_y(*half_height, *radius)
            }
            Self::CapsuleX { half_height, radius } => {
                rapier::SharedShape::capsule_x(*half_height, *radius)
            }
            Self::CapsuleZ { half_height, radius } => {
                rapier::SharedShape::capsule_z(*half_height, *radius)
            }
            Self::CylinderY { half_height, radius } => {
                rapier::SharedShape::cylinder(*half_height, *radius)
            }
            Self::ConeY { half_height, radius } => {
                rapier::SharedShape::cone(*half_height, *radius)
            }
            Self::ConvexHull { points } => {
                let rapier_points: Vec<_> = points
                    .iter()
                    .map(|p| rapier::Point::new(p[0], p[1], p[2]))
                    .collect();
                rapier::SharedShape::convex_hull(&rapier_points)
                    .unwrap_or_else(|| rapier::SharedShape::ball(0.5))
            }
            Self::TriMesh { vertices, indices } => {
                let rapier_verts: Vec<_> = vertices
                    .iter()
                    .map(|v| rapier::Point::new(v[0], v[1], v[2]))
                    .collect();
                rapier::SharedShape::trimesh(rapier_verts, indices.clone())
            }
            Self::HeightField {
                heights,
                rows,
                cols,
                scale,
            } => {
                let matrix = rapier::nalgebra::DMatrix::from_row_slice(*rows, *cols, heights);
                rapier::SharedShape::heightfield(
                    matrix,
                    rapier::Vector::new(scale[0], scale[1], scale[2]),
                )
            }
        }
    }
}

/// Description for creating a collider
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ColliderDesc {
    /// Collision shape
    pub shape: ColliderShape,
    /// Position offset from parent body
    pub position_offset: [f32; 3],
    /// Rotation offset (quaternion)
    pub rotation_offset: [f32; 4],
    /// Is this a sensor/trigger (no physical response)
    pub is_sensor: bool,
    /// Physics material
    pub material: PhysicsMaterial,
    /// Collision groups
    pub collision_groups: CollisionGroups,
    /// Solver groups (which bodies affect each other)
    pub solver_groups: CollisionGroups,
    /// Contact force event threshold (0 = disabled)
    pub contact_force_event_threshold: f32,
    /// User data (entity ID, etc.)
    pub user_data: u128,
}

impl Default for ColliderDesc {
    fn default() -> Self {
        Self {
            shape: ColliderShape::default(),
            position_offset: [0.0, 0.0, 0.0],
            rotation_offset: [0.0, 0.0, 0.0, 1.0],
            is_sensor: false,
            material: PhysicsMaterial::default(),
            collision_groups: CollisionGroups::ALL,
            solver_groups: CollisionGroups::ALL,
            contact_force_event_threshold: 0.0,
            user_data: 0,
        }
    }
}

impl ColliderDesc {
    /// Create a new collider description with a shape
    pub fn new(shape: ColliderShape) -> Self {
        Self {
            shape,
            ..Default::default()
        }
    }

    /// Create a sensor (trigger volume)
    pub fn sensor(shape: ColliderShape) -> Self {
        Self {
            shape,
            is_sensor: true,
            ..Default::default()
        }
    }

    /// Set position offset
    pub fn with_offset(mut self, x: f32, y: f32, z: f32) -> Self {
        self.position_offset = [x, y, z];
        self
    }

    /// Set as sensor
    pub fn with_sensor(mut self, is_sensor: bool) -> Self {
        self.is_sensor = is_sensor;
        self
    }

    /// Set material
    pub fn with_material(mut self, material: PhysicsMaterial) -> Self {
        self.material = material;
        self
    }

    /// Set collision groups
    pub fn with_collision_groups(mut self, groups: CollisionGroups) -> Self {
        self.collision_groups = groups;
        self
    }

    /// Set user data
    pub fn with_user_data(mut self, data: u128) -> Self {
        self.user_data = data;
        self
    }

    /// Build a Rapier collider builder
    pub(crate) fn to_rapier_builder(&self) -> rapier::ColliderBuilder {
        let mut builder = rapier::ColliderBuilder::new(self.shape.to_rapier())
            .translation(rapier::Vector::new(
                self.position_offset[0],
                self.position_offset[1],
                self.position_offset[2],
            ))
            .sensor(self.is_sensor)
            .friction(self.material.friction)
            .restitution(self.material.restitution)
            .density(self.material.density)
            .collision_groups(rapier::InteractionGroups::new(
                rapier::Group::from_bits_truncate(self.collision_groups.memberships),
                rapier::Group::from_bits_truncate(self.collision_groups.filter),
            ))
            .solver_groups(rapier::InteractionGroups::new(
                rapier::Group::from_bits_truncate(self.solver_groups.memberships),
                rapier::Group::from_bits_truncate(self.solver_groups.filter),
            ))
            .user_data(self.user_data);

        if self.contact_force_event_threshold > 0.0 {
            builder = builder.contact_force_event_threshold(self.contact_force_event_threshold);
        }

        builder
    }
}

/// ECS Component for colliders
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ColliderComponent {
    /// Collider description
    pub desc: ColliderDesc,
    /// Handle to physics collider (set at runtime)
    #[serde(skip)]
    pub handle: Option<ColliderHandle>,
    /// Associated rigid body handle
    #[serde(skip)]
    pub body_handle: Option<RigidBodyHandle>,
    /// Whether the component is initialized
    #[serde(skip)]
    pub initialized: bool,
}

impl ColliderComponent {
    /// Create a new collider component
    pub fn new(desc: ColliderDesc) -> Self {
        Self {
            desc,
            handle: None,
            body_handle: None,
            initialized: false,
        }
    }

    /// Create a box collider
    pub fn cuboid(hx: f32, hy: f32, hz: f32) -> Self {
        Self::new(ColliderDesc::new(ColliderShape::cuboid(hx, hy, hz)))
    }

    /// Create a sphere collider
    pub fn sphere(radius: f32) -> Self {
        Self::new(ColliderDesc::new(ColliderShape::sphere(radius)))
    }

    /// Create a capsule collider
    pub fn capsule(half_height: f32, radius: f32) -> Self {
        Self::new(ColliderDesc::new(ColliderShape::capsule(half_height, radius)))
    }

    /// Create a sensor/trigger collider
    pub fn sensor(shape: ColliderShape) -> Self {
        Self::new(ColliderDesc::sensor(shape))
    }
}

impl Default for ColliderComponent {
    fn default() -> Self {
        Self::new(ColliderDesc::default())
    }
}
