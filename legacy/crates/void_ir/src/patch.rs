//! Patches - declarative operations on the world
//!
//! A patch represents a single operation that an app wants to perform.
//! Patches are collected into transactions and applied atomically.

use crate::namespace::NamespaceId;
use crate::value::Value;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Unique identifier for an entity within a namespace
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct EntityRef {
    /// The namespace this entity belongs to
    pub namespace: NamespaceId,
    /// Local entity ID within the namespace
    pub local_id: u64,
}

impl EntityRef {
    /// Create a new entity reference
    pub fn new(namespace: NamespaceId, local_id: u64) -> Self {
        Self { namespace, local_id }
    }
}

/// A single patch operation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Patch {
    /// The namespace this patch originates from
    pub source: NamespaceId,
    /// The kind of patch
    pub kind: PatchKind,
    /// Optional priority (higher = applied first)
    pub priority: i32,
    /// Timestamp when patch was created
    pub timestamp: u64,
}

impl Patch {
    /// Create a new patch
    pub fn new(source: NamespaceId, kind: PatchKind) -> Self {
        Self {
            source,
            kind,
            priority: 0,
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_millis() as u64)
                .unwrap_or(0),
        }
    }

    /// Set priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Check if this patch targets a specific entity
    pub fn targets_entity(&self, entity: EntityRef) -> bool {
        match &self.kind {
            PatchKind::Entity(ep) => ep.entity == entity,
            PatchKind::Component(cp) => cp.entity == entity,
            PatchKind::Layer(_) => false,
            PatchKind::Asset(_) => false,
            PatchKind::Hierarchy(hp) => hp.entity == entity,
            PatchKind::Camera(cp) => cp.entity == entity,
        }
    }
}

/// The kind of patch operation
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum PatchKind {
    /// Entity-level operations
    Entity(EntityPatch),
    /// Component-level operations
    Component(ComponentPatch),
    /// Layer operations
    Layer(LayerPatch),
    /// Asset operations
    Asset(AssetPatch),
    /// Hierarchy operations (Phase 1: Scene Graph)
    Hierarchy(HierarchyPatch),
    /// Camera operations (Phase 2: Camera System)
    Camera(CameraPatch),
}

/// Entity-level patch operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EntityPatch {
    /// The entity being operated on
    pub entity: EntityRef,
    /// The operation
    pub op: EntityOp,
}

/// Entity operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum EntityOp {
    /// Create a new entity with optional initial components
    Create {
        /// Optional archetype/template name
        archetype: Option<String>,
        /// Initial component data
        components: HashMap<String, Value>,
    },
    /// Destroy an entity
    Destroy,
    /// Enable an entity
    Enable,
    /// Disable an entity (hidden from queries but not destroyed)
    Disable,
    /// Set entity's parent (for hierarchy)
    SetParent { parent: Option<EntityRef> },
    /// Add a tag to the entity
    AddTag { tag: String },
    /// Remove a tag from the entity
    RemoveTag { tag: String },
}

/// Component-level patch operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentPatch {
    /// The entity being operated on
    pub entity: EntityRef,
    /// The component type name
    pub component: String,
    /// The operation
    pub op: ComponentOp,
}

/// Component operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ComponentOp {
    /// Add or replace a component
    Set { data: Value },
    /// Update specific fields of a component
    Update { fields: HashMap<String, Value> },
    /// Remove a component
    Remove,
}

/// Layer-level patch operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LayerPatch {
    /// Layer identifier
    pub layer_id: String,
    /// The operation
    pub op: LayerOp,
}

/// Layer operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum LayerOp {
    /// Request a new layer
    Create {
        /// Layer type (Content, Effect, Overlay, Portal)
        layer_type: LayerType,
        /// Render priority (higher = rendered later / on top)
        priority: i32,
    },
    /// Update layer properties
    Update {
        /// New priority
        priority: Option<i32>,
        /// Visibility
        visible: Option<bool>,
        /// Blend mode
        blend_mode: Option<BlendMode>,
    },
    /// Release a layer
    Destroy,
}

/// Types of layers
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum LayerType {
    /// Standard content layer (geometry, materials)
    Content,
    /// Effect/post-processing layer
    Effect,
    /// Overlay/UI layer
    Overlay,
    /// Portal layer (render another view)
    Portal,
}

/// Blend modes for layer composition
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum BlendMode {
    /// Normal alpha blending
    Normal,
    /// Additive blending
    Additive,
    /// Multiplicative blending
    Multiply,
    /// Screen blending
    Screen,
    /// Replace (no blending)
    Replace,
}

impl Default for BlendMode {
    fn default() -> Self {
        Self::Normal
    }
}

/// Asset-level patch operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AssetPatch {
    /// Asset path/identifier
    pub asset_id: String,
    /// The operation
    pub op: AssetOp,
}

/// Asset operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum AssetOp {
    /// Request an asset to be loaded
    Load {
        /// Asset path
        path: String,
        /// Asset type hint
        asset_type: Option<String>,
    },
    /// Unload an asset
    Unload,
    /// Update asset data (for hot-reload)
    Update { data: Value },
}

// ============================================================================
// Hierarchy Patches (Phase 1: Scene Graph)
// ============================================================================

/// Hierarchy-level patch operations for scene graph management.
///
/// These patches provide declarative operations for manipulating the
/// entity hierarchy. They are validated and applied atomically.
///
/// # Hot-Swap Support
///
/// Hierarchy patches are fully serializable and can be replayed during
/// hot-reload to restore hierarchy state.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HierarchyPatch {
    /// The primary entity being operated on
    pub entity: EntityRef,
    /// The operation
    pub op: HierarchyOp,
}

/// Hierarchy operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum HierarchyOp {
    /// Set an entity's parent
    ///
    /// Validates:
    /// - Parent entity exists
    /// - No cycle would be created
    /// - Namespace permissions allow cross-namespace parenting (if applicable)
    SetParent {
        /// The new parent entity
        parent: EntityRef,
    },

    /// Remove an entity's parent (make it a root)
    RemoveParent,

    /// Despawn an entity and all its descendants recursively
    ///
    /// Descendants are despawned deepest-first to maintain hierarchy
    /// integrity during the operation.
    DespawnRecursive,

    /// Reparent all children to a new parent
    ///
    /// Useful for reorganizing hierarchies without despawning.
    ReparentChildren {
        /// The new parent for all children (None = make children roots)
        new_parent: Option<EntityRef>,
    },

    /// Detach all children (make them roots)
    DetachChildren,

    /// Set the local transform of an entity
    ///
    /// The global transform will be recomputed automatically.
    SetLocalTransform {
        /// Translation [x, y, z]
        translation: [f32; 3],
        /// Rotation quaternion [x, y, z, w]
        rotation: [f32; 4],
        /// Scale [x, y, z]
        scale: [f32; 3],
    },

    /// Update visibility
    SetVisible {
        /// Whether the entity is visible
        visible: bool,
    },

    /// Insert entity at specific index among siblings
    ///
    /// Useful for controlling draw order among siblings.
    SetSiblingIndex {
        /// Index among siblings (0 = first)
        index: usize,
    },
}

impl HierarchyPatch {
    /// Create a set parent patch
    pub fn set_parent(child: EntityRef, parent: EntityRef) -> Self {
        Self {
            entity: child,
            op: HierarchyOp::SetParent { parent },
        }
    }

    /// Create a remove parent patch
    pub fn remove_parent(entity: EntityRef) -> Self {
        Self {
            entity,
            op: HierarchyOp::RemoveParent,
        }
    }

    /// Create a recursive despawn patch
    pub fn despawn_recursive(entity: EntityRef) -> Self {
        Self {
            entity,
            op: HierarchyOp::DespawnRecursive,
        }
    }

    /// Create a reparent children patch
    pub fn reparent_children(entity: EntityRef, new_parent: Option<EntityRef>) -> Self {
        Self {
            entity,
            op: HierarchyOp::ReparentChildren { new_parent },
        }
    }

    /// Create a detach children patch
    pub fn detach_children(entity: EntityRef) -> Self {
        Self {
            entity,
            op: HierarchyOp::DetachChildren,
        }
    }

    /// Create a set local transform patch
    pub fn set_local_transform(
        entity: EntityRef,
        translation: [f32; 3],
        rotation: [f32; 4],
        scale: [f32; 3],
    ) -> Self {
        Self {
            entity,
            op: HierarchyOp::SetLocalTransform {
                translation,
                rotation,
                scale,
            },
        }
    }

    /// Create a set visible patch
    pub fn set_visible(entity: EntityRef, visible: bool) -> Self {
        Self {
            entity,
            op: HierarchyOp::SetVisible { visible },
        }
    }

    /// Create a set sibling index patch
    pub fn set_sibling_index(entity: EntityRef, index: usize) -> Self {
        Self {
            entity,
            op: HierarchyOp::SetSiblingIndex { index },
        }
    }
}

// ============================================================================
// Phase 2: Camera System
// ============================================================================

/// Camera patch for camera operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CameraPatch {
    /// The camera entity being operated on
    pub entity: EntityRef,
    /// The operation
    pub op: CameraOp,
}

/// Camera operations
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CameraOp {
    /// Set this camera as the main camera
    SetMain,

    /// Clear main camera status
    ClearMain,

    /// Set camera active state
    SetActive {
        /// Whether the camera is active
        active: bool,
    },

    /// Set projection mode
    SetProjection {
        /// The projection settings
        projection: ProjectionData,
    },

    /// Set clip planes
    SetClipPlanes {
        /// Near clip distance
        near: f32,
        /// Far clip distance
        far: f32,
    },

    /// Set clear color
    SetClearColor {
        /// Clear color (RGBA)
        color: [f32; 4],
    },

    /// Set viewport
    SetViewport {
        /// Viewport rectangle (normalized 0-1)
        viewport: Option<ViewportData>,
    },

    /// Set render priority
    SetPriority {
        /// Render priority (higher = rendered later)
        priority: i32,
    },

    /// Set layer mask
    SetLayerMask {
        /// Layer mask (None = render all)
        mask: Option<u32>,
    },

    /// Animate FOV (perspective cameras only)
    AnimateFov {
        /// Target FOV in degrees
        target_fov_degrees: f32,
        /// Animation duration in seconds
        duration: f32,
    },

    /// Set aspect ratio override
    SetAspectRatio {
        /// Aspect ratio (None = use viewport)
        aspect: Option<f32>,
    },
}

/// Projection data for camera patches
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ProjectionData {
    /// Perspective projection
    Perspective {
        /// Field of view in degrees
        fov_degrees: f32,
    },
    /// Orthographic projection
    Orthographic {
        /// View height
        height: f32,
    },
}

/// Viewport data for camera patches
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ViewportData {
    /// X position (0-1)
    pub x: f32,
    /// Y position (0-1)
    pub y: f32,
    /// Width (0-1)
    pub width: f32,
    /// Height (0-1)
    pub height: f32,
}

impl ViewportData {
    /// Full screen viewport
    pub fn full() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: 1.0,
            height: 1.0,
        }
    }

    /// Left half of screen
    pub fn left_half() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: 0.5,
            height: 1.0,
        }
    }

    /// Right half of screen
    pub fn right_half() -> Self {
        Self {
            x: 0.5,
            y: 0.0,
            width: 0.5,
            height: 1.0,
        }
    }
}

impl CameraPatch {
    /// Create a set main camera patch
    pub fn set_main(entity: EntityRef) -> Self {
        Self {
            entity,
            op: CameraOp::SetMain,
        }
    }

    /// Create a set active patch
    pub fn set_active(entity: EntityRef, active: bool) -> Self {
        Self {
            entity,
            op: CameraOp::SetActive { active },
        }
    }

    /// Create a set perspective projection patch
    pub fn set_perspective(entity: EntityRef, fov_degrees: f32) -> Self {
        Self {
            entity,
            op: CameraOp::SetProjection {
                projection: ProjectionData::Perspective { fov_degrees },
            },
        }
    }

    /// Create a set orthographic projection patch
    pub fn set_orthographic(entity: EntityRef, height: f32) -> Self {
        Self {
            entity,
            op: CameraOp::SetProjection {
                projection: ProjectionData::Orthographic { height },
            },
        }
    }

    /// Create a set clip planes patch
    pub fn set_clip_planes(entity: EntityRef, near: f32, far: f32) -> Self {
        Self {
            entity,
            op: CameraOp::SetClipPlanes { near, far },
        }
    }

    /// Create a set clear color patch
    pub fn set_clear_color(entity: EntityRef, color: [f32; 4]) -> Self {
        Self {
            entity,
            op: CameraOp::SetClearColor { color },
        }
    }

    /// Create a set priority patch
    pub fn set_priority(entity: EntityRef, priority: i32) -> Self {
        Self {
            entity,
            op: CameraOp::SetPriority { priority },
        }
    }

    /// Create an animate FOV patch
    pub fn animate_fov(entity: EntityRef, target_fov_degrees: f32, duration: f32) -> Self {
        Self {
            entity,
            op: CameraOp::AnimateFov {
                target_fov_degrees,
                duration,
            },
        }
    }
}

// Builder pattern for creating patches

impl EntityPatch {
    /// Create a new entity creation patch
    pub fn create(namespace: NamespaceId, local_id: u64) -> Self {
        Self {
            entity: EntityRef::new(namespace, local_id),
            op: EntityOp::Create {
                archetype: None,
                components: HashMap::new(),
            },
        }
    }

    /// Create a destroy patch
    pub fn destroy(entity: EntityRef) -> Self {
        Self {
            entity,
            op: EntityOp::Destroy,
        }
    }

    /// Add initial components (only valid for Create op)
    pub fn with_component(mut self, name: impl Into<String>, data: Value) -> Self {
        if let EntityOp::Create { components, .. } = &mut self.op {
            components.insert(name.into(), data);
        }
        self
    }

    /// Set archetype (only valid for Create op)
    pub fn with_archetype(mut self, archetype: impl Into<String>) -> Self {
        if let EntityOp::Create {
            archetype: arch, ..
        } = &mut self.op
        {
            *arch = Some(archetype.into());
        }
        self
    }
}

impl ComponentPatch {
    /// Create a set component patch
    pub fn set(entity: EntityRef, component: impl Into<String>, data: Value) -> Self {
        Self {
            entity,
            component: component.into(),
            op: ComponentOp::Set { data },
        }
    }

    /// Create an update component patch
    pub fn update(
        entity: EntityRef,
        component: impl Into<String>,
        fields: HashMap<String, Value>,
    ) -> Self {
        Self {
            entity,
            component: component.into(),
            op: ComponentOp::Update { fields },
        }
    }

    /// Create a remove component patch
    pub fn remove(entity: EntityRef, component: impl Into<String>) -> Self {
        Self {
            entity,
            component: component.into(),
            op: ComponentOp::Remove,
        }
    }
}

impl LayerPatch {
    /// Create a new layer request
    pub fn create(
        layer_id: impl Into<String>,
        layer_type: LayerType,
        priority: i32,
    ) -> Self {
        Self {
            layer_id: layer_id.into(),
            op: LayerOp::Create {
                layer_type,
                priority,
            },
        }
    }

    /// Destroy a layer
    pub fn destroy(layer_id: impl Into<String>) -> Self {
        Self {
            layer_id: layer_id.into(),
            op: LayerOp::Destroy,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_entity_patch_builder() {
        let ns = NamespaceId::new();
        let patch = EntityPatch::create(ns, 1)
            .with_archetype("Player")
            .with_component("Transform", Value::from([0.0, 0.0, 0.0]));

        if let EntityOp::Create {
            archetype,
            components,
        } = &patch.op
        {
            assert_eq!(archetype.as_deref(), Some("Player"));
            assert!(components.contains_key("Transform"));
        } else {
            panic!("Expected Create op");
        }
    }

    #[test]
    fn test_component_patch() {
        let entity = EntityRef::new(NamespaceId::new(), 1);
        let patch = ComponentPatch::set(entity, "Position", Value::from([1.0, 2.0, 3.0]));

        assert_eq!(patch.component, "Position");
        assert!(matches!(patch.op, ComponentOp::Set { .. }));
    }
}
