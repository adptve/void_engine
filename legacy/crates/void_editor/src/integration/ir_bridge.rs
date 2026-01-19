//! Bridge between editor commands and void_ir patches.
//!
//! Converts editor operations to declarative patches for hot-swappable
//! scene synchronization.

use void_ir::{
    Patch, PatchKind, EntityPatch, ComponentPatch, EntityRef, NamespaceId, Value,
};
use void_ir::patch::{EntityOp, ComponentOp};
use std::collections::HashMap;

use crate::core::{EntityId, Transform, MeshType, SceneEntity};

/// Get the editor namespace ID for void_ir patches.
pub fn editor_namespace() -> NamespaceId {
    NamespaceId::from_raw(1)
}

/// Bridge for converting editor operations to void_ir patches.
pub struct IrBridge {
    namespace: NamespaceId,
    pending_patches: Vec<Patch>,
}

impl Default for IrBridge {
    fn default() -> Self {
        Self::new(editor_namespace())
    }
}

impl IrBridge {
    /// Create a new IR bridge for the given namespace.
    pub fn new(namespace: NamespaceId) -> Self {
        Self {
            namespace,
            pending_patches: Vec::new(),
        }
    }

    /// Get the namespace ID.
    pub fn namespace(&self) -> NamespaceId {
        self.namespace
    }

    /// Take all pending patches.
    pub fn take_patches(&mut self) -> Vec<Patch> {
        std::mem::take(&mut self.pending_patches)
    }

    /// Check if there are pending patches.
    pub fn has_pending_patches(&self) -> bool {
        !self.pending_patches.is_empty()
    }

    // ========================================================================
    // Entity Patches
    // ========================================================================

    /// Create a patch for entity creation.
    pub fn create_entity_patch(&mut self, entity: &SceneEntity) {
        let entity_ref = self.entity_ref(entity.id);

        let mut components = HashMap::new();

        // Add name component
        components.insert(
            "Name".to_string(),
            Value::String(entity.name.clone()),
        );

        // Add transform component
        components.insert(
            "Transform".to_string(),
            self.transform_to_value(&entity.transform),
        );

        // Add mesh component
        components.insert(
            "Mesh".to_string(),
            self.mesh_to_value(entity.mesh_type, entity.color),
        );

        let patch = Patch::new(
            self.namespace,
            PatchKind::Entity(EntityPatch {
                entity: entity_ref,
                op: EntityOp::Create {
                    archetype: Some(entity.mesh_type.name().to_string()),
                    components,
                },
            }),
        );

        self.pending_patches.push(patch);
    }

    /// Create a patch for entity destruction.
    pub fn destroy_entity_patch(&mut self, id: EntityId) {
        let entity_ref = self.entity_ref(id);

        let patch = Patch::new(
            self.namespace,
            PatchKind::Entity(EntityPatch {
                entity: entity_ref,
                op: EntityOp::Destroy,
            }),
        );

        self.pending_patches.push(patch);
    }

    /// Create a patch for reparenting.
    pub fn reparent_patch(&mut self, id: EntityId, new_parent: Option<EntityId>) {
        let entity_ref = self.entity_ref(id);
        let parent_ref = new_parent.map(|pid| self.entity_ref(pid));

        let patch = Patch::new(
            self.namespace,
            PatchKind::Entity(EntityPatch {
                entity: entity_ref,
                op: EntityOp::SetParent { parent: parent_ref },
            }),
        );

        self.pending_patches.push(patch);
    }

    // ========================================================================
    // Component Patches
    // ========================================================================

    /// Create a patch for transform update.
    pub fn transform_patch(&mut self, id: EntityId, transform: &Transform) {
        let entity_ref = self.entity_ref(id);

        let patch = Patch::new(
            self.namespace,
            PatchKind::Component(ComponentPatch {
                entity: entity_ref,
                component: "Transform".to_string(),
                op: ComponentOp::Set {
                    data: self.transform_to_value(transform),
                },
            }),
        );

        self.pending_patches.push(patch);
    }

    /// Create a patch for position-only update.
    pub fn position_patch(&mut self, id: EntityId, position: [f32; 3]) {
        let entity_ref = self.entity_ref(id);

        let mut fields = HashMap::new();
        fields.insert("position".to_string(), self.vec3_to_value(position));

        let patch = Patch::new(
            self.namespace,
            PatchKind::Component(ComponentPatch {
                entity: entity_ref,
                component: "Transform".to_string(),
                op: ComponentOp::Update { fields },
            }),
        );

        self.pending_patches.push(patch);
    }

    /// Create a patch for rotation-only update.
    pub fn rotation_patch(&mut self, id: EntityId, rotation: [f32; 3]) {
        let entity_ref = self.entity_ref(id);

        let mut fields = HashMap::new();
        fields.insert("rotation".to_string(), self.vec3_to_value(rotation));

        let patch = Patch::new(
            self.namespace,
            PatchKind::Component(ComponentPatch {
                entity: entity_ref,
                component: "Transform".to_string(),
                op: ComponentOp::Update { fields },
            }),
        );

        self.pending_patches.push(patch);
    }

    /// Create a patch for scale-only update.
    pub fn scale_patch(&mut self, id: EntityId, scale: [f32; 3]) {
        let entity_ref = self.entity_ref(id);

        let mut fields = HashMap::new();
        fields.insert("scale".to_string(), self.vec3_to_value(scale));

        let patch = Patch::new(
            self.namespace,
            PatchKind::Component(ComponentPatch {
                entity: entity_ref,
                component: "Transform".to_string(),
                op: ComponentOp::Update { fields },
            }),
        );

        self.pending_patches.push(patch);
    }

    /// Create a patch for color update.
    pub fn color_patch(&mut self, id: EntityId, color: [f32; 3]) {
        let entity_ref = self.entity_ref(id);

        let mut fields = HashMap::new();
        fields.insert("color".to_string(), self.vec3_to_value(color));

        let patch = Patch::new(
            self.namespace,
            PatchKind::Component(ComponentPatch {
                entity: entity_ref,
                component: "Mesh".to_string(),
                op: ComponentOp::Update { fields },
            }),
        );

        self.pending_patches.push(patch);
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    fn entity_ref(&self, id: EntityId) -> EntityRef {
        EntityRef::new(self.namespace, id.0 as u64)
    }

    fn transform_to_value(&self, transform: &Transform) -> Value {
        Value::Object({
            let mut map = HashMap::new();
            map.insert("position".to_string(), self.vec3_to_value(transform.position));
            map.insert("rotation".to_string(), self.vec3_to_value(transform.rotation));
            map.insert("scale".to_string(), self.vec3_to_value(transform.scale));
            map
        })
    }

    fn mesh_to_value(&self, mesh_type: MeshType, color: [f32; 3]) -> Value {
        Value::Object({
            let mut map = HashMap::new();
            map.insert("type".to_string(), Value::String(mesh_type.name().to_string()));
            map.insert("color".to_string(), self.vec3_to_value(color));
            map
        })
    }

    fn vec3_to_value(&self, v: [f32; 3]) -> Value {
        Value::Array(vec![
            Value::Float(v[0] as f64),
            Value::Float(v[1] as f64),
            Value::Float(v[2] as f64),
        ])
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_create_entity_patch() {
        let mut bridge = IrBridge::default();

        let entity = SceneEntity {
            id: EntityId(42),
            name: "Test Entity".to_string(),
            transform: Transform::new(),
            mesh_type: MeshType::Cube,
            color: [1.0, 0.5, 0.0],
            ..Default::default()
        };

        bridge.create_entity_patch(&entity);

        let patches = bridge.take_patches();
        assert_eq!(patches.len(), 1);

        match &patches[0].kind {
            PatchKind::Entity(ep) => {
                assert_eq!(ep.entity.local_id, 42);
                match &ep.op {
                    EntityOp::Create { archetype, components } => {
                        assert_eq!(archetype.as_deref(), Some("Cube"));
                        assert!(components.contains_key("Name"));
                        assert!(components.contains_key("Transform"));
                        assert!(components.contains_key("Mesh"));
                    }
                    _ => panic!("Expected Create op"),
                }
            }
            _ => panic!("Expected Entity patch"),
        }
    }

    #[test]
    fn test_transform_patch() {
        let mut bridge = IrBridge::default();

        let transform = Transform {
            position: [1.0, 2.0, 3.0],
            rotation: [0.1, 0.2, 0.3],
            scale: [1.0, 1.0, 1.0],
        };

        bridge.transform_patch(EntityId(1), &transform);

        let patches = bridge.take_patches();
        assert_eq!(patches.len(), 1);

        match &patches[0].kind {
            PatchKind::Component(cp) => {
                assert_eq!(cp.component, "Transform");
                match &cp.op {
                    ComponentOp::Set { data } => {
                        if let Value::Object(map) = data {
                            assert!(map.contains_key("position"));
                            assert!(map.contains_key("rotation"));
                            assert!(map.contains_key("scale"));
                        } else {
                            panic!("Expected Map value");
                        }
                    }
                    _ => panic!("Expected Set op"),
                }
            }
            _ => panic!("Expected Component patch"),
        }
    }
}
