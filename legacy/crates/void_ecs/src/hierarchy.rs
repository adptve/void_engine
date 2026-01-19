//! Hierarchical Entity Components for Scene Graph
//!
//! This module provides components for parent-child relationships between entities,
//! with automatic transform propagation, visibility inheritance, and cycle detection.
//!
//! # Core Philosophy
//!
//! All components in this module are designed to be hot-swappable per the project's
//! core philosophy "Everything is Hot-Swappable". They implement:
//! - `Serialize`/`Deserialize` for state persistence
//! - `HotReloadable` for runtime module replacement
//!
//! # Components
//!
//! - [`Parent`] - References the parent entity
//! - [`Children`] - Automatically managed list of child entities
//! - [`LocalTransform`] - Transform relative to parent (or world if root)
//! - [`GlobalTransform`] - Computed world-space transform
//! - [`InheritedVisibility`] - Visibility inherited from parent chain
//! - [`HierarchyDepth`] - Depth in hierarchy tree (root = 0)
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::prelude::*;
//! use void_ecs::hierarchy::*;
//!
//! let mut world = World::new();
//!
//! // Create parent entity
//! let parent = world.build_entity()
//!     .with(LocalTransform::from_translation([10.0, 0.0, 0.0]))
//!     .build();
//!
//! // Create child entity
//! let child = world.build_entity()
//!     .with(Parent { entity: parent })
//!     .with(LocalTransform::from_translation([5.0, 0.0, 0.0]))
//!     .build();
//!
//! // After TransformPropagationSystem runs:
//! // child's GlobalTransform will have translation [15.0, 0.0, 0.0]
//! ```

use alloc::string::String;
use alloc::vec::Vec;

use serde::{Deserialize, Serialize};

use crate::Entity;
use void_core::error::Result;
use void_core::hot_reload::{HotReloadSnapshot, HotReloadable};
use void_core::version::Version;

// ============================================================================
// Entity Serialization Support
// ============================================================================

/// Wrapper for Entity serialization
/// Entity uses to_bits()/from_bits() for efficient u64 representation
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct SerializableEntity(pub Entity);

impl Serialize for SerializableEntity {
    fn serialize<S>(&self, serializer: S) -> core::result::Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.0.to_bits().serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for SerializableEntity {
    fn deserialize<D>(deserializer: D) -> core::result::Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let bits = u64::deserialize(deserializer)?;
        Ok(SerializableEntity(Entity::from_bits(bits)))
    }
}

impl From<Entity> for SerializableEntity {
    fn from(entity: Entity) -> Self {
        SerializableEntity(entity)
    }
}

impl From<SerializableEntity> for Entity {
    fn from(se: SerializableEntity) -> Self {
        se.0
    }
}

// ============================================================================
// Parent Component
// ============================================================================

/// Parent-child relationship component.
///
/// Adding this component to an entity makes it a child of the referenced parent.
/// The parent entity must exist and be valid.
///
/// # Hot-Swap Support
///
/// This component is fully serializable and survives hot-reload. The entity
/// reference is serialized as a u64 bit pattern.
///
/// # Example
///
/// ```ignore
/// let child = world.spawn();
/// world.add_component(child, Parent { entity: parent_entity });
/// ```
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Parent {
    /// The parent entity reference (serialized as u64 bits)
    #[serde(with = "entity_serde")]
    pub entity: Entity,
}

impl Parent {
    /// Create a new Parent component
    pub fn new(entity: Entity) -> Self {
        Self { entity }
    }
}

impl HotReloadable for Parent {
    fn snapshot(&self) -> Result<HotReloadSnapshot> {
        let data = bincode::serialize(self).map_err(|e| {
            void_core::error::HotReloadError::SnapshotFailed(e.to_string().into())
        })?;
        Ok(HotReloadSnapshot::new::<Self>(data, Version::new(1, 0, 0)))
    }

    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
        if !snapshot.is_type::<Self>() {
            return Err(void_core::error::HotReloadError::RestoreFailed(
                "Type mismatch".into(),
            )
            .into());
        }
        *self = bincode::deserialize(&snapshot.data).map_err(|e| {
            void_core::error::HotReloadError::RestoreFailed(e.to_string().into())
        })?;
        Ok(())
    }

    fn is_compatible(&self, new_version: &Version) -> bool {
        new_version.major == 1
    }
}

// ============================================================================
// Children Component
// ============================================================================

/// Automatically managed list of child entities.
///
/// **DO NOT modify directly** - use the hierarchy APIs in [`HierarchyCommands`].
/// This component is automatically updated when [`Parent`] components are added
/// or removed.
///
/// # Hot-Swap Support
///
/// All child entity references are serialized and restored during hot-reload.
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Children {
    /// Child entities (serialized as Vec<u64>)
    #[serde(with = "entity_vec_serde")]
    entities: Vec<Entity>,
}

impl Children {
    /// Create an empty Children component
    pub fn new() -> Self {
        Self::default()
    }

    /// Create with initial children
    pub fn with_children(children: Vec<Entity>) -> Self {
        Self { entities: children }
    }

    /// Iterate over child entities
    pub fn iter(&self) -> impl Iterator<Item = &Entity> {
        self.entities.iter()
    }

    /// Get the number of children
    pub fn len(&self) -> usize {
        self.entities.len()
    }

    /// Check if there are no children
    pub fn is_empty(&self) -> bool {
        self.entities.is_empty()
    }

    /// Check if entity is a child
    pub fn contains(&self, entity: Entity) -> bool {
        self.entities.contains(&entity)
    }

    /// Get child at index
    pub fn get(&self, index: usize) -> Option<Entity> {
        self.entities.get(index).copied()
    }

    /// Get first child
    pub fn first(&self) -> Option<Entity> {
        self.entities.first().copied()
    }

    /// Get last child
    pub fn last(&self) -> Option<Entity> {
        self.entities.last().copied()
    }

    // Internal methods for hierarchy system
    pub(crate) fn push(&mut self, entity: Entity) {
        if !self.entities.contains(&entity) {
            self.entities.push(entity);
        }
    }

    pub(crate) fn remove(&mut self, entity: Entity) {
        self.entities.retain(|&e| e != entity);
    }

    pub(crate) fn clear(&mut self) {
        self.entities.clear();
    }
}

impl HotReloadable for Children {
    fn snapshot(&self) -> Result<HotReloadSnapshot> {
        let data = bincode::serialize(self).map_err(|e| {
            void_core::error::HotReloadError::SnapshotFailed(e.to_string().into())
        })?;
        Ok(HotReloadSnapshot::new::<Self>(data, Version::new(1, 0, 0)))
    }

    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
        if !snapshot.is_type::<Self>() {
            return Err(void_core::error::HotReloadError::RestoreFailed(
                "Type mismatch".into(),
            )
            .into());
        }
        *self = bincode::deserialize(&snapshot.data).map_err(|e| {
            void_core::error::HotReloadError::RestoreFailed(e.to_string().into())
        })?;
        Ok(())
    }

    fn is_compatible(&self, new_version: &Version) -> bool {
        new_version.major == 1
    }
}

// ============================================================================
// LocalTransform Component
// ============================================================================

/// Local transform relative to parent (or world if no parent).
///
/// Uses quaternion rotation for proper composition without gimbal lock.
/// The transform is applied in TRS order: Translation * Rotation * Scale.
///
/// # Hot-Swap Support
///
/// All transform values are serializable. Transient cached matrices are
/// recomputed after hot-reload.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct LocalTransform {
    /// Translation offset in parent space
    pub translation: [f32; 3],
    /// Rotation quaternion [x, y, z, w] (unit quaternion)
    pub rotation: [f32; 4],
    /// Scale factors
    pub scale: [f32; 3],
}

impl Default for LocalTransform {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl LocalTransform {
    /// Identity transform (no translation, no rotation, unit scale)
    pub const IDENTITY: Self = Self {
        translation: [0.0, 0.0, 0.0],
        rotation: [0.0, 0.0, 0.0, 1.0], // Identity quaternion
        scale: [1.0, 1.0, 1.0],
    };

    /// Create from translation only
    pub fn from_translation(translation: [f32; 3]) -> Self {
        Self {
            translation,
            ..Self::IDENTITY
        }
    }

    /// Create from translation and uniform scale
    pub fn from_translation_scale(translation: [f32; 3], scale: f32) -> Self {
        Self {
            translation,
            scale: [scale, scale, scale],
            ..Self::IDENTITY
        }
    }

    /// Create from translation, rotation, and scale
    pub fn new(translation: [f32; 3], rotation: [f32; 4], scale: [f32; 3]) -> Self {
        Self {
            translation,
            rotation,
            scale,
        }
    }

    /// Create from Euler angles (pitch, yaw, roll) in radians
    pub fn from_euler(translation: [f32; 3], euler: [f32; 3], scale: [f32; 3]) -> Self {
        Self {
            translation,
            rotation: euler_to_quaternion(euler),
            scale,
        }
    }

    /// Set translation
    pub fn with_translation(mut self, translation: [f32; 3]) -> Self {
        self.translation = translation;
        self
    }

    /// Set rotation from quaternion
    pub fn with_rotation(mut self, rotation: [f32; 4]) -> Self {
        self.rotation = rotation;
        self
    }

    /// Set rotation from Euler angles (pitch, yaw, roll) in radians
    pub fn with_euler(mut self, euler: [f32; 3]) -> Self {
        self.rotation = euler_to_quaternion(euler);
        self
    }

    /// Set scale
    pub fn with_scale(mut self, scale: [f32; 3]) -> Self {
        self.scale = scale;
        self
    }

    /// Set uniform scale
    pub fn with_uniform_scale(mut self, scale: f32) -> Self {
        self.scale = [scale, scale, scale];
        self
    }

    /// Convert to 4x4 column-major transformation matrix
    pub fn to_matrix(&self) -> [[f32; 4]; 4] {
        let [tx, ty, tz] = self.translation;
        let [qx, qy, qz, qw] = self.rotation;
        let [sx, sy, sz] = self.scale;

        // Rotation matrix from quaternion
        let x2 = qx + qx;
        let y2 = qy + qy;
        let z2 = qz + qz;
        let xx = qx * x2;
        let xy = qx * y2;
        let xz = qx * z2;
        let yy = qy * y2;
        let yz = qy * z2;
        let zz = qz * z2;
        let wx = qw * x2;
        let wy = qw * y2;
        let wz = qw * z2;

        // TRS matrix (column-major)
        [
            [sx * (1.0 - yy - zz), sx * (xy + wz), sx * (xz - wy), 0.0],
            [sy * (xy - wz), sy * (1.0 - xx - zz), sy * (yz + wx), 0.0],
            [sz * (xz + wy), sz * (yz - wx), sz * (1.0 - xx - yy), 0.0],
            [tx, ty, tz, 1.0],
        ]
    }

    /// Interpolate between two transforms
    pub fn lerp(&self, other: &Self, t: f32) -> Self {
        let t = t.clamp(0.0, 1.0);
        let inv_t = 1.0 - t;

        Self {
            translation: [
                self.translation[0] * inv_t + other.translation[0] * t,
                self.translation[1] * inv_t + other.translation[1] * t,
                self.translation[2] * inv_t + other.translation[2] * t,
            ],
            rotation: slerp(self.rotation, other.rotation, t),
            scale: [
                self.scale[0] * inv_t + other.scale[0] * t,
                self.scale[1] * inv_t + other.scale[1] * t,
                self.scale[2] * inv_t + other.scale[2] * t,
            ],
        }
    }
}

impl HotReloadable for LocalTransform {
    fn snapshot(&self) -> Result<HotReloadSnapshot> {
        let data = bincode::serialize(self).map_err(|e| {
            void_core::error::HotReloadError::SnapshotFailed(e.to_string().into())
        })?;
        Ok(HotReloadSnapshot::new::<Self>(data, Version::new(1, 0, 0)))
    }

    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
        if !snapshot.is_type::<Self>() {
            return Err(void_core::error::HotReloadError::RestoreFailed(
                "Type mismatch".into(),
            )
            .into());
        }
        *self = bincode::deserialize(&snapshot.data).map_err(|e| {
            void_core::error::HotReloadError::RestoreFailed(e.to_string().into())
        })?;
        Ok(())
    }

    fn is_compatible(&self, new_version: &Version) -> bool {
        new_version.major == 1
    }
}

// ============================================================================
// GlobalTransform Component
// ============================================================================

/// Computed world-space transform.
///
/// This component is **read-only** from user code perspective. It is automatically
/// updated by the [`TransformPropagationSystem`] based on the entity's
/// [`LocalTransform`] and parent chain.
///
/// # Hot-Swap Support
///
/// The matrix is serialized, but marked dirty on reload to force recomputation.
/// This ensures the transform stays in sync with any LocalTransform changes.
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct GlobalTransform {
    /// 4x4 column-major transformation matrix
    pub matrix: [[f32; 4]; 4],

    /// Transient: marks if recomputation is needed (not serialized)
    #[serde(skip)]
    pub dirty: bool,
}

impl Default for GlobalTransform {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl GlobalTransform {
    /// Identity transform
    pub const IDENTITY: Self = Self {
        matrix: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
        dirty: true,
    };

    /// Create from a local transform (for root entities)
    pub fn from_local(local: &LocalTransform) -> Self {
        Self {
            matrix: local.to_matrix(),
            dirty: false,
        }
    }

    /// Get world-space translation
    pub fn translation(&self) -> [f32; 3] {
        [self.matrix[3][0], self.matrix[3][1], self.matrix[3][2]]
    }

    /// Get world-space scale (approximate - assumes no shear)
    pub fn scale(&self) -> [f32; 3] {
        [
            (self.matrix[0][0] * self.matrix[0][0]
                + self.matrix[0][1] * self.matrix[0][1]
                + self.matrix[0][2] * self.matrix[0][2])
                .sqrt(),
            (self.matrix[1][0] * self.matrix[1][0]
                + self.matrix[1][1] * self.matrix[1][1]
                + self.matrix[1][2] * self.matrix[1][2])
                .sqrt(),
            (self.matrix[2][0] * self.matrix[2][0]
                + self.matrix[2][1] * self.matrix[2][1]
                + self.matrix[2][2] * self.matrix[2][2])
                .sqrt(),
        ]
    }

    /// Compose with a child's local transform: self * child
    pub fn multiply(&self, child: &LocalTransform) -> Self {
        let child_matrix = child.to_matrix();
        Self {
            matrix: mat4_multiply(&self.matrix, &child_matrix),
            dirty: false,
        }
    }

    /// Multiply two matrices
    pub fn multiply_matrix(&self, other: &[[f32; 4]; 4]) -> Self {
        Self {
            matrix: mat4_multiply(&self.matrix, other),
            dirty: false,
        }
    }

    /// Transform a point by this matrix
    pub fn transform_point(&self, point: [f32; 3]) -> [f32; 3] {
        let x = self.matrix[0][0] * point[0]
            + self.matrix[1][0] * point[1]
            + self.matrix[2][0] * point[2]
            + self.matrix[3][0];
        let y = self.matrix[0][1] * point[0]
            + self.matrix[1][1] * point[1]
            + self.matrix[2][1] * point[2]
            + self.matrix[3][1];
        let z = self.matrix[0][2] * point[0]
            + self.matrix[1][2] * point[1]
            + self.matrix[2][2] * point[2]
            + self.matrix[3][2];
        [x, y, z]
    }

    /// Transform a direction vector (ignores translation)
    pub fn transform_vector(&self, vector: [f32; 3]) -> [f32; 3] {
        let x = self.matrix[0][0] * vector[0]
            + self.matrix[1][0] * vector[1]
            + self.matrix[2][0] * vector[2];
        let y = self.matrix[0][1] * vector[0]
            + self.matrix[1][1] * vector[1]
            + self.matrix[2][1] * vector[2];
        let z = self.matrix[0][2] * vector[0]
            + self.matrix[1][2] * vector[1]
            + self.matrix[2][2] * vector[2];
        [x, y, z]
    }

    /// Get the forward direction (negative Z axis in local space)
    pub fn forward(&self) -> [f32; 3] {
        let z = [-self.matrix[2][0], -self.matrix[2][1], -self.matrix[2][2]];
        normalize_vec3(z)
    }

    /// Get the up direction (positive Y axis in local space)
    pub fn up(&self) -> [f32; 3] {
        let y = [self.matrix[1][0], self.matrix[1][1], self.matrix[1][2]];
        normalize_vec3(y)
    }

    /// Get the right direction (positive X axis in local space)
    pub fn right(&self) -> [f32; 3] {
        let x = [self.matrix[0][0], self.matrix[0][1], self.matrix[0][2]];
        normalize_vec3(x)
    }

    /// Mark as needing recomputation
    pub fn mark_dirty(&mut self) {
        self.dirty = true;
    }

    /// Check if recomputation is needed
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }
}

impl HotReloadable for GlobalTransform {
    fn snapshot(&self) -> Result<HotReloadSnapshot> {
        let data = bincode::serialize(self).map_err(|e| {
            void_core::error::HotReloadError::SnapshotFailed(e.to_string().into())
        })?;
        Ok(HotReloadSnapshot::new::<Self>(data, Version::new(1, 0, 0)))
    }

    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
        if !snapshot.is_type::<Self>() {
            return Err(void_core::error::HotReloadError::RestoreFailed(
                "Type mismatch".into(),
            )
            .into());
        }
        *self = bincode::deserialize(&snapshot.data).map_err(|e| {
            void_core::error::HotReloadError::RestoreFailed(e.to_string().into())
        })?;
        Ok(())
    }

    fn is_compatible(&self, new_version: &Version) -> bool {
        new_version.major == 1
    }

    fn finish_reload(&mut self) -> Result<()> {
        // Mark dirty to force recomputation from LocalTransform
        self.dirty = true;
        Ok(())
    }
}

// ============================================================================
// InheritedVisibility Component
// ============================================================================

/// Visibility inherited from parent chain.
///
/// This is computed by [`VisibilityPropagationSystem`] based on the entity's
/// own visibility and all ancestors' visibility. An entity is only visible
/// if all its ancestors are also visible.
///
/// # Hot-Swap Support
///
/// Visibility state is preserved across hot-reload.
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct InheritedVisibility {
    /// Whether the entity is visible (considering entire parent chain)
    pub visible: bool,
}

impl InheritedVisibility {
    /// Create visible
    pub fn visible() -> Self {
        Self { visible: true }
    }

    /// Create hidden
    pub fn hidden() -> Self {
        Self { visible: false }
    }
}

impl HotReloadable for InheritedVisibility {
    fn snapshot(&self) -> Result<HotReloadSnapshot> {
        let data = bincode::serialize(self).map_err(|e| {
            void_core::error::HotReloadError::SnapshotFailed(e.to_string().into())
        })?;
        Ok(HotReloadSnapshot::new::<Self>(data, Version::new(1, 0, 0)))
    }

    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
        if !snapshot.is_type::<Self>() {
            return Err(void_core::error::HotReloadError::RestoreFailed(
                "Type mismatch".into(),
            )
            .into());
        }
        *self = bincode::deserialize(&snapshot.data).map_err(|e| {
            void_core::error::HotReloadError::RestoreFailed(e.to_string().into())
        })?;
        Ok(())
    }

    fn is_compatible(&self, new_version: &Version) -> bool {
        new_version.major == 1
    }
}

// ============================================================================
// HierarchyDepth Component
// ============================================================================

/// Depth in the hierarchy tree.
///
/// Root entities have depth 0, their children have depth 1, and so on.
/// This is used for ordering transform propagation (parents before children).
///
/// # Hot-Swap Support
///
/// Depth is set to 0 on reload and recomputed by [`HierarchyValidationSystem`].
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct HierarchyDepth {
    /// Depth level (root = 0)
    pub depth: u32,
}

impl HierarchyDepth {
    /// Create with specific depth
    pub fn new(depth: u32) -> Self {
        Self { depth }
    }

    /// Create root depth (0)
    pub fn root() -> Self {
        Self { depth: 0 }
    }
}

impl HotReloadable for HierarchyDepth {
    fn snapshot(&self) -> Result<HotReloadSnapshot> {
        let data = bincode::serialize(self).map_err(|e| {
            void_core::error::HotReloadError::SnapshotFailed(e.to_string().into())
        })?;
        Ok(HotReloadSnapshot::new::<Self>(data, Version::new(1, 0, 0)))
    }

    fn restore(&mut self, snapshot: HotReloadSnapshot) -> Result<()> {
        if !snapshot.is_type::<Self>() {
            return Err(void_core::error::HotReloadError::RestoreFailed(
                "Type mismatch".into(),
            )
            .into());
        }
        *self = bincode::deserialize(&snapshot.data).map_err(|e| {
            void_core::error::HotReloadError::RestoreFailed(e.to_string().into())
        })?;
        Ok(())
    }

    fn is_compatible(&self, new_version: &Version) -> bool {
        new_version.major == 1
    }

    fn finish_reload(&mut self) -> Result<()> {
        // Depths will be recomputed by HierarchyValidationSystem
        self.depth = 0;
        Ok(())
    }
}

// ============================================================================
// Hierarchy Error Types
// ============================================================================

/// Errors that can occur during hierarchy operations
#[derive(Clone, Debug)]
pub enum HierarchyError {
    /// Cycle detected in hierarchy
    CycleDetected {
        /// Entities involved in the cycle
        entities: Vec<Entity>,
    },
    /// Invalid parent reference
    InvalidParent {
        /// Child entity
        child: Entity,
        /// Invalid parent entity
        parent: Entity,
    },
    /// Entity not found
    EntityNotFound {
        /// Missing entity
        entity: Entity,
    },
    /// Permission denied for hierarchy operation
    PermissionDenied {
        /// Entity that was denied access
        entity: Entity,
    },
    /// Cross-namespace parenting without capability
    CrossNamespaceViolation {
        /// Child's namespace
        child_ns: String,
        /// Parent's namespace
        parent_ns: String,
    },
}

impl core::fmt::Display for HierarchyError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::CycleDetected { entities } => {
                write!(f, "Cycle detected in hierarchy: {:?}", entities)
            }
            Self::InvalidParent { child, parent } => {
                write!(f, "Invalid parent {:?} for child {:?}", parent, child)
            }
            Self::EntityNotFound { entity } => {
                write!(f, "Entity not found: {:?}", entity)
            }
            Self::PermissionDenied { entity } => {
                write!(f, "Permission denied for entity: {:?}", entity)
            }
            Self::CrossNamespaceViolation { child_ns, parent_ns } => {
                write!(
                    f,
                    "Cross-namespace parenting denied: {} -> {}",
                    child_ns, parent_ns
                )
            }
        }
    }
}

// ============================================================================
// Serde Helpers for Entity
// ============================================================================

/// Serde module for single Entity serialization
mod entity_serde {
    use super::*;
    use serde::{Deserializer, Serializer};

    pub fn serialize<S>(entity: &Entity, serializer: S) -> core::result::Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        entity.to_bits().serialize(serializer)
    }

    pub fn deserialize<'de, D>(deserializer: D) -> core::result::Result<Entity, D::Error>
    where
        D: Deserializer<'de>,
    {
        let bits = u64::deserialize(deserializer)?;
        Ok(Entity::from_bits(bits))
    }
}

/// Serde module for Vec<Entity> serialization
mod entity_vec_serde {
    use super::*;
    use serde::{Deserializer, Serializer};

    pub fn serialize<S>(entities: &[Entity], serializer: S) -> core::result::Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let bits: Vec<u64> = entities.iter().map(|e| e.to_bits()).collect();
        bits.serialize(serializer)
    }

    pub fn deserialize<'de, D>(deserializer: D) -> core::result::Result<Vec<Entity>, D::Error>
    where
        D: Deserializer<'de>,
    {
        let bits: Vec<u64> = Vec::deserialize(deserializer)?;
        Ok(bits.into_iter().map(Entity::from_bits).collect())
    }
}

// ============================================================================
// Math Utilities
// ============================================================================

/// Convert Euler angles (pitch, yaw, roll) to quaternion [x, y, z, w]
fn euler_to_quaternion(euler: [f32; 3]) -> [f32; 4] {
    let [pitch, yaw, roll] = euler;
    let hp = pitch * 0.5;
    let hy = yaw * 0.5;
    let hr = roll * 0.5;

    let (sp, cp) = hp.sin_cos();
    let (sy, cy) = hy.sin_cos();
    let (sr, cr) = hr.sin_cos();

    [
        sr * cp * cy - cr * sp * sy, // x
        cr * sp * cy + sr * cp * sy, // y
        cr * cp * sy - sr * sp * cy, // z
        cr * cp * cy + sr * sp * sy, // w
    ]
}

/// Spherical linear interpolation for quaternions
fn slerp(a: [f32; 4], b: [f32; 4], t: f32) -> [f32; 4] {
    let mut dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];

    // Ensure shortest path
    let mut b = b;
    if dot < 0.0 {
        b = [-b[0], -b[1], -b[2], -b[3]];
        dot = -dot;
    }

    // If quaternions are very close, use linear interpolation
    if dot > 0.9995 {
        let result = [
            a[0] + t * (b[0] - a[0]),
            a[1] + t * (b[1] - a[1]),
            a[2] + t * (b[2] - a[2]),
            a[3] + t * (b[3] - a[3]),
        ];
        return normalize_quat(result);
    }

    let theta_0 = dot.acos();
    let theta = theta_0 * t;
    let sin_theta = theta.sin();
    let sin_theta_0 = theta_0.sin();

    let s0 = (theta_0 - theta).cos() - dot * sin_theta / sin_theta_0;
    let s1 = sin_theta / sin_theta_0;

    [
        s0 * a[0] + s1 * b[0],
        s0 * a[1] + s1 * b[1],
        s0 * a[2] + s1 * b[2],
        s0 * a[3] + s1 * b[3],
    ]
}

/// Normalize a quaternion
fn normalize_quat(q: [f32; 4]) -> [f32; 4] {
    let len = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]).sqrt();
    if len > 0.0 {
        [q[0] / len, q[1] / len, q[2] / len, q[3] / len]
    } else {
        [0.0, 0.0, 0.0, 1.0] // Identity
    }
}

/// Multiply two 4x4 column-major matrices
fn mat4_multiply(a: &[[f32; 4]; 4], b: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let mut result = [[0.0f32; 4]; 4];

    for col in 0..4 {
        for row in 0..4 {
            result[col][row] = a[0][row] * b[col][0]
                + a[1][row] * b[col][1]
                + a[2][row] * b[col][2]
                + a[3][row] * b[col][3];
        }
    }

    result
}

/// Normalize a 3D vector
fn normalize_vec3(v: [f32; 3]) -> [f32; 3] {
    let len_sq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if len_sq > 0.0001 {
        let len = len_sq.sqrt();
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, 0.0, 1.0] // Default to Z-axis if near zero
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_local_transform_identity() {
        let t = LocalTransform::IDENTITY;
        let m = t.to_matrix();

        // Should be identity matrix
        assert_eq!(m[0][0], 1.0);
        assert_eq!(m[1][1], 1.0);
        assert_eq!(m[2][2], 1.0);
        assert_eq!(m[3][3], 1.0);
    }

    #[test]
    fn test_local_transform_translation() {
        let t = LocalTransform::from_translation([10.0, 20.0, 30.0]);
        let m = t.to_matrix();

        // Translation should be in last column
        assert_eq!(m[3][0], 10.0);
        assert_eq!(m[3][1], 20.0);
        assert_eq!(m[3][2], 30.0);
    }

    #[test]
    fn test_global_transform_multiply() {
        let parent = GlobalTransform::from_local(&LocalTransform::from_translation([10.0, 0.0, 0.0]));
        let child_local = LocalTransform::from_translation([5.0, 0.0, 0.0]);

        let child_global = parent.multiply(&child_local);

        // Child should be at parent + local = 15.0
        let translation = child_global.translation();
        assert!((translation[0] - 15.0).abs() < 0.001);
    }

    #[test]
    fn test_parent_serialization() {
        let entity = Entity::new(42, 7);
        let parent = Parent::new(entity);

        let serialized = bincode::serialize(&parent).unwrap();
        let deserialized: Parent = bincode::deserialize(&serialized).unwrap();

        assert_eq!(parent.entity, deserialized.entity);
    }

    #[test]
    fn test_children_operations() {
        let mut children = Children::new();
        let e1 = Entity::new(1, 0);
        let e2 = Entity::new(2, 0);

        children.push(e1);
        children.push(e2);
        assert_eq!(children.len(), 2);
        assert!(children.contains(e1));

        children.remove(e1);
        assert_eq!(children.len(), 1);
        assert!(!children.contains(e1));
    }

    #[test]
    fn test_euler_to_quaternion() {
        // Zero rotation should give identity quaternion
        let q = euler_to_quaternion([0.0, 0.0, 0.0]);
        assert!((q[0]).abs() < 0.001);
        assert!((q[1]).abs() < 0.001);
        assert!((q[2]).abs() < 0.001);
        assert!((q[3] - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_transform_lerp() {
        let a = LocalTransform::from_translation([0.0, 0.0, 0.0]);
        let b = LocalTransform::from_translation([10.0, 10.0, 10.0]);

        let mid = a.lerp(&b, 0.5);
        assert!((mid.translation[0] - 5.0).abs() < 0.001);
        assert!((mid.translation[1] - 5.0).abs() < 0.001);
        assert!((mid.translation[2] - 5.0).abs() < 0.001);
    }
}
