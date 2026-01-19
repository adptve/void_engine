//! Patch applicator - applies IR transactions to the world
//!
//! The applicator is the bridge between the IR system and the actual
//! ECS world. It validates and applies patches atomically.
//!
//! # Security
//!
//! Every patch is checked against the capability system before application:
//! - Entity creation requires `CreateEntities` capability
//! - Entity destruction requires `DestroyEntities` capability
//! - Component modification requires `ModifyComponents` capability
//! - Layer creation requires `CreateLayers` capability
//! - Asset loading requires `LoadAssets` capability

use crate::capability::{CapabilityChecker, enforce_patch_capability};
use crate::layer::{LayerConfig, LayerManager};
use crate::registry::AssetRegistry;
use void_ecs::World;
use void_ir::patch::{
    AssetOp, AssetPatch, BlendMode, ComponentOp, ComponentPatch, EntityOp, EntityPatch,
    EntityRef, LayerOp, LayerPatch, LayerType, Patch, PatchKind,
    CameraPatch, CameraOp, ProjectionData,
};
use void_ir::transaction::{Transaction, TransactionId, TransactionResult, TransactionState};
use void_ir::Value;
use std::collections::HashMap;

/// Result of applying a transaction
#[derive(Debug)]
pub struct ApplyResult {
    /// Transaction ID
    pub transaction_id: TransactionId,
    /// Whether all patches succeeded
    pub success: bool,
    /// Number of patches applied
    pub patches_applied: usize,
    /// Error message if failed
    pub error: Option<String>,
    /// Entity mapping (local ID -> ECS entity)
    pub entity_map: HashMap<u64, void_ecs::Entity>,
    /// Patches that were applied (for undo)
    pub applied_patches: Vec<Patch>,
}

impl ApplyResult {
    /// Convert to transaction result for patch bus
    pub fn to_transaction_result(&self) -> TransactionResult {
        if self.success {
            TransactionResult::success(self.transaction_id, self.patches_applied)
        } else {
            TransactionResult::failure(
                self.transaction_id,
                self.error.clone().unwrap_or_default(),
                self.patches_applied,
            )
        }
    }
}

/// Applies patches to the world
pub struct PatchApplicator {
    /// Entity mapping per namespace (namespace -> local_id -> ecs_entity)
    entity_maps: HashMap<void_ir::NamespaceId, HashMap<u64, void_ecs::Entity>>,
}

impl PatchApplicator {
    /// Create a new applicator
    pub fn new() -> Self {
        Self {
            entity_maps: HashMap::new(),
        }
    }

    /// Apply a transaction to the world (without capability checking)
    ///
    /// WARNING: This bypasses security checks. Use `apply_with_capability_check`
    /// for production code.
    pub fn apply(
        &mut self,
        mut transaction: Transaction,
        world: &mut World,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
    ) -> ApplyResult {
        self.apply_internal(transaction, world, layer_manager, asset_registry, None)
    }

    /// Apply a transaction with capability checking
    ///
    /// Every patch is validated against the capability system before application.
    /// If any patch lacks the required capability, the entire transaction is rejected.
    pub fn apply_with_capability_check(
        &mut self,
        mut transaction: Transaction,
        world: &mut World,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
        capability_checker: &CapabilityChecker,
    ) -> ApplyResult {
        self.apply_internal(
            transaction,
            world,
            layer_manager,
            asset_registry,
            Some(capability_checker),
        )
    }

    /// Internal apply implementation with optional capability checking
    fn apply_internal(
        &mut self,
        mut transaction: Transaction,
        world: &mut World,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
        capability_checker: Option<&CapabilityChecker>,
    ) -> ApplyResult {
        let tx_id = transaction.id;
        let source = transaction.source;

        // Sort patches by priority
        transaction.sort_by_priority();

        let mut applied = Vec::new();
        let mut entity_map = HashMap::new();

        // If capability checking is enabled, validate all patches first
        if let Some(checker) = capability_checker {
            for patch in &transaction.patches {
                if let Err(e) = enforce_patch_capability(checker, source, &patch.kind) {
                    log::warn!("Capability check failed for patch: {}", e);
                    return ApplyResult {
                        transaction_id: tx_id,
                        success: false,
                        patches_applied: 0,
                        error: Some(format!("Capability check failed: {}", e)),
                        entity_map: HashMap::new(),
                        applied_patches: Vec::new(),
                    };
                }
            }
        }

        for patch in &transaction.patches {
            match self.apply_patch(patch, source, world, layer_manager, asset_registry) {
                Ok(entities) => {
                    applied.push(patch.clone());
                    entity_map.extend(entities);
                }
                Err(e) => {
                    log::error!("Failed to apply patch: {}", e);
                    // Rollback applied patches
                    self.rollback(&applied, world, layer_manager);
                    return ApplyResult {
                        transaction_id: tx_id,
                        success: false,
                        patches_applied: applied.len(),
                        error: Some(e),
                        entity_map,
                        applied_patches: applied,
                    };
                }
            }
        }

        ApplyResult {
            transaction_id: tx_id,
            success: true,
            patches_applied: applied.len(),
            error: None,
            entity_map,
            applied_patches: applied,
        }
    }

    /// Apply a single patch
    fn apply_patch(
        &mut self,
        patch: &Patch,
        source: void_ir::NamespaceId,
        world: &mut World,
        layer_manager: &mut LayerManager,
        asset_registry: &mut AssetRegistry,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        match &patch.kind {
            PatchKind::Entity(ep) => self.apply_entity_patch(ep, source, world),
            PatchKind::Component(cp) => self.apply_component_patch(cp, source, world),
            PatchKind::Layer(lp) => self.apply_layer_patch(lp, source, layer_manager),
            PatchKind::Asset(ap) => self.apply_asset_patch(ap, source, asset_registry),
            PatchKind::Hierarchy(hp) => self.apply_hierarchy_patch(hp, source, world),
            PatchKind::Camera(cp) => self.apply_camera_patch(cp, source, world),
        }
    }

    /// Apply a hierarchy patch
    fn apply_hierarchy_patch(
        &mut self,
        patch: &void_ir::HierarchyPatch,
        _source: void_ir::NamespaceId,
        world: &mut World,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        use void_ir::HierarchyOp;
        use void_ecs::hierarchy::{Children, LocalTransform, Parent};
        use void_ecs::hierarchy_commands::HierarchyCommands;

        // Look up the actual entity
        let ns_map = self.entity_maps.entry(patch.entity.namespace).or_default();
        let entity = match ns_map.get(&patch.entity.local_id) {
            Some(&e) => e,
            None => return Err(format!("Entity not found: {:?}", patch.entity)),
        };

        match &patch.op {
            HierarchyOp::SetParent { parent } => {
                let parent_ns_map = self.entity_maps.entry(parent.namespace).or_default();
                if let Some(&parent_entity) = parent_ns_map.get(&parent.local_id) {
                    let mut cmds = HierarchyCommands::new(world);
                    cmds.set_parent(entity, parent_entity)
                        .map_err(|e| format!("{:?}", e))?;
                } else {
                    return Err(format!("Parent entity not found: {:?}", parent));
                }
            }
            HierarchyOp::RemoveParent => {
                let mut cmds = HierarchyCommands::new(world);
                cmds.remove_parent(entity);
            }
            HierarchyOp::DespawnRecursive => {
                let mut cmds = HierarchyCommands::new(world);
                cmds.despawn_recursive(entity);
            }
            HierarchyOp::ReparentChildren { new_parent } => {
                // Reparent all children to a new parent
                if let Some(children) = world.get_component::<Children>(entity) {
                    let child_entities: Vec<_> = children.iter().copied().collect();
                    let mut cmds = HierarchyCommands::new(world);
                    for child in child_entities {
                        if let Some(parent_ref) = new_parent {
                            let parent_ns_map = self.entity_maps.entry(parent_ref.namespace).or_default();
                            if let Some(&new_parent_entity) = parent_ns_map.get(&parent_ref.local_id) {
                                let _ = cmds.set_parent(child, new_parent_entity);
                            }
                        } else {
                            cmds.remove_parent(child);
                        }
                    }
                }
            }
            HierarchyOp::DetachChildren => {
                if let Some(children) = world.get_component::<Children>(entity) {
                    let child_entities: Vec<_> = children.iter().copied().collect();
                    let mut cmds = HierarchyCommands::new(world);
                    for child in child_entities {
                        cmds.remove_parent(child);
                    }
                }
            }
            HierarchyOp::SetLocalTransform { translation, rotation, scale } => {
                let local = LocalTransform::new(*translation, *rotation, *scale);
                world.add_component(entity, local);
            }
            HierarchyOp::SetVisible { visible } => {
                use void_ecs::render_components::Visible;
                world.add_component(entity, Visible { visible: *visible, ..Default::default() });
            }
            HierarchyOp::SetSiblingIndex { index: _ } => {
                // Sibling ordering not yet implemented
                log::warn!("SetSiblingIndex not yet implemented");
            }
        }

        Ok(HashMap::new())
    }

    /// Apply an entity patch
    fn apply_entity_patch(
        &mut self,
        patch: &EntityPatch,
        source: void_ir::NamespaceId,
        world: &mut World,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        let ns_map = self.entity_maps.entry(patch.entity.namespace).or_default();
        let mut result = HashMap::new();

        match &patch.op {
            EntityOp::Create { archetype, components } => {
                // Create a new entity in the ECS
                let entity = world.spawn();
                ns_map.insert(patch.entity.local_id, entity);
                result.insert(patch.entity.local_id, entity);

                log::debug!(
                    "Created entity {:?} (local_id: {}, archetype: {:?})",
                    entity,
                    patch.entity.local_id,
                    archetype
                );

                // Apply initial components
                for (name, value) in components {
                    self.set_component(world, entity, name, value)?;
                }
            }
            EntityOp::Destroy => {
                if let Some(entity) = ns_map.remove(&patch.entity.local_id) {
                    world.despawn(entity);
                    log::debug!("Destroyed entity {:?}", entity);
                } else {
                    return Err(format!(
                        "Entity not found: {}:{}",
                        patch.entity.namespace, patch.entity.local_id
                    ));
                }
            }
            EntityOp::Enable => {
                // Would set an "enabled" flag on the entity
                log::debug!("Enable entity: {:?}", patch.entity);
            }
            EntityOp::Disable => {
                // Would set an "enabled" flag on the entity
                log::debug!("Disable entity: {:?}", patch.entity);
            }
            EntityOp::SetParent { parent } => {
                // Would set up hierarchy relationship
                log::debug!("Set parent: {:?} -> {:?}", patch.entity, parent);
            }
            EntityOp::AddTag { tag } => {
                log::debug!("Add tag '{}' to {:?}", tag, patch.entity);
            }
            EntityOp::RemoveTag { tag } => {
                log::debug!("Remove tag '{}' from {:?}", tag, patch.entity);
            }
        }

        Ok(result)
    }

    /// Apply a component patch
    fn apply_component_patch(
        &mut self,
        patch: &ComponentPatch,
        source: void_ir::NamespaceId,
        world: &mut World,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        let ns_map = self.entity_maps.get(&patch.entity.namespace)
            .ok_or_else(|| format!("Namespace not found: {}", patch.entity.namespace))?;

        let entity = ns_map.get(&patch.entity.local_id)
            .ok_or_else(|| format!("Entity not found: {}", patch.entity.local_id))?;

        match &patch.op {
            ComponentOp::Set { data } => {
                self.set_component(world, *entity, &patch.component, data)?;
            }
            ComponentOp::Update { fields } => {
                // Partial update - would merge fields
                log::debug!(
                    "Update component '{}' on {:?}: {:?}",
                    patch.component,
                    entity,
                    fields
                );
            }
            ComponentOp::Remove => {
                log::debug!("Remove component '{}' from {:?}", patch.component, entity);
            }
        }

        Ok(HashMap::new())
    }

    /// Apply a layer patch
    fn apply_layer_patch(
        &mut self,
        patch: &LayerPatch,
        source: void_ir::NamespaceId,
        layer_manager: &mut LayerManager,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        match &patch.op {
            LayerOp::Create { layer_type, priority } => {
                let config = match layer_type {
                    LayerType::Content => LayerConfig::content(*priority),
                    LayerType::Effect => LayerConfig::effect(*priority),
                    LayerType::Overlay => LayerConfig::overlay(*priority),
                    LayerType::Portal => LayerConfig::portal(*priority),
                };
                layer_manager
                    .create_layer(&patch.layer_id, source, config)
                    .map_err(|e| e.to_string())?;
            }
            LayerOp::Update { priority, visible, blend_mode } => {
                // Find layer by name and update
                // This is simplified - in practice we'd have a layer ID mapping
                log::debug!(
                    "Update layer '{}': priority={:?}, visible={:?}, blend={:?}",
                    patch.layer_id,
                    priority,
                    visible,
                    blend_mode
                );
            }
            LayerOp::Destroy => {
                // Find and destroy layer by name
                log::debug!("Destroy layer '{}'", patch.layer_id);
            }
        }

        Ok(HashMap::new())
    }

    /// Apply an asset patch
    fn apply_asset_patch(
        &mut self,
        patch: &AssetPatch,
        source: void_ir::NamespaceId,
        asset_registry: &mut AssetRegistry,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        match &patch.op {
            AssetOp::Load { path, asset_type } => {
                log::debug!(
                    "Load asset '{}' from '{}' (type: {:?})",
                    patch.asset_id,
                    path,
                    asset_type
                );
                // Would trigger asset loading
            }
            AssetOp::Unload => {
                log::debug!("Unload asset '{}'", patch.asset_id);
            }
            AssetOp::Update { data } => {
                log::debug!("Update asset '{}': {:?}", patch.asset_id, data);
            }
        }

        Ok(HashMap::new())
    }

    /// Apply a camera patch
    fn apply_camera_patch(
        &mut self,
        patch: &CameraPatch,
        _source: void_ir::NamespaceId,
        world: &mut World,
    ) -> Result<HashMap<u64, void_ecs::Entity>, String> {
        use void_ecs::camera::{Camera, Projection, Viewport};

        // Look up the actual entity
        let ns_map = self.entity_maps.entry(patch.entity.namespace).or_default();
        let entity = match ns_map.get(&patch.entity.local_id) {
            Some(&e) => e,
            None => return Err(format!("Camera entity not found: {:?}", patch.entity)),
        };

        // Get or create camera component
        let mut camera = world.get_component::<Camera>(entity)
            .cloned()
            .unwrap_or_else(Camera::default);

        match &patch.op {
            CameraOp::SetMain => {
                camera.is_main = true;
                log::debug!("Set camera {:?} as main", entity);
            }
            CameraOp::ClearMain => {
                camera.is_main = false;
                log::debug!("Cleared main camera status for {:?}", entity);
            }
            CameraOp::SetActive { active } => {
                camera.active = *active;
                log::debug!("Set camera {:?} active={}", entity, active);
            }
            CameraOp::SetProjection { projection } => {
                camera.projection = match projection {
                    ProjectionData::Perspective { fov_degrees } => {
                        Projection::Perspective { fov_y: fov_degrees.to_radians() }
                    }
                    ProjectionData::Orthographic { height } => {
                        Projection::Orthographic { height: *height }
                    }
                };
                log::debug!("Set camera {:?} projection: {:?}", entity, camera.projection);
            }
            CameraOp::SetClipPlanes { near, far } => {
                camera.near = *near;
                camera.far = *far;
                log::debug!("Set camera {:?} clip planes: near={}, far={}", entity, near, far);
            }
            CameraOp::SetClearColor { color } => {
                camera.clear_color = *color;
                log::debug!("Set camera {:?} clear color: {:?}", entity, color);
            }
            CameraOp::SetViewport { viewport } => {
                camera.viewport = viewport.as_ref().map(|v| Viewport {
                    x: v.x,
                    y: v.y,
                    width: v.width,
                    height: v.height,
                });
                log::debug!("Set camera {:?} viewport: {:?}", entity, camera.viewport);
            }
            CameraOp::SetPriority { priority } => {
                camera.priority = *priority;
                log::debug!("Set camera {:?} priority: {}", entity, priority);
            }
            CameraOp::SetLayerMask { mask } => {
                camera.layer_mask = *mask;
                log::debug!("Set camera {:?} layer mask: {:?}", entity, mask);
            }
            CameraOp::AnimateFov { target_fov_degrees, duration } => {
                // Start FOV animation
                use void_ecs::camera::{CameraAnimation, EasingFunction};
                if let Projection::Perspective { fov_y } = camera.projection {
                    let anim = CameraAnimation::fov_transition_degrees(*target_fov_degrees, *duration)
                        .with_easing(EasingFunction::EaseInOut);
                    camera.animation = Some(anim);
                    log::debug!(
                        "Started FOV animation on camera {:?}: {} -> {} over {}s",
                        entity, fov_y.to_degrees(), target_fov_degrees, duration
                    );
                } else {
                    log::warn!("Cannot animate FOV on orthographic camera {:?}", entity);
                }
            }
            CameraOp::SetAspectRatio { aspect } => {
                camera.aspect_ratio = *aspect;
                log::debug!("Set camera {:?} aspect ratio: {:?}", entity, aspect);
            }
        }

        // Update the camera component
        world.add_component(entity, camera);

        Ok(HashMap::new())
    }

    /// Set a component on an entity from Value
    fn set_component(
        &self,
        world: &mut World,
        entity: void_ecs::Entity,
        name: &str,
        value: &Value,
    ) -> Result<(), String> {
        // This is where we'd map Value to actual component types
        // For now, we just log
        log::debug!("Set component '{}' on {:?}: {:?}", name, entity, value);

        // In a full implementation, we'd have a component registry that maps
        // string names to component types and handles serialization/deserialization
        // Example:
        // match name {
        //     "Transform" => {
        //         let transform = Transform::from_value(value)?;
        //         world.add_component(entity, transform);
        //     }
        //     _ => return Err(format!("Unknown component: {}", name)),
        // }

        Ok(())
    }

    /// Rollback applied patches
    fn rollback(
        &mut self,
        patches: &[Patch],
        world: &mut World,
        layer_manager: &mut LayerManager,
    ) {
        // Rollback in reverse order
        for patch in patches.iter().rev() {
            match &patch.kind {
                PatchKind::Entity(ep) => {
                    match &ep.op {
                        EntityOp::Create { .. } => {
                            // Destroy the created entity
                            if let Some(ns_map) = self.entity_maps.get_mut(&ep.entity.namespace) {
                                if let Some(entity) = ns_map.remove(&ep.entity.local_id) {
                                    world.despawn(entity);
                                }
                            }
                        }
                        _ => {} // Other ops would need their inverse
                    }
                }
                PatchKind::Layer(lp) => {
                    match &lp.op {
                        LayerOp::Create { .. } => {
                            // Would destroy the created layer
                        }
                        _ => {}
                    }
                }
                _ => {}
            }
        }
    }

    /// Get the ECS entity for a namespace entity reference
    pub fn resolve_entity(&self, entity_ref: EntityRef) -> Option<void_ecs::Entity> {
        self.entity_maps
            .get(&entity_ref.namespace)?
            .get(&entity_ref.local_id)
            .copied()
    }
}

impl Default for PatchApplicator {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use void_ir::transaction::TransactionBuilder;
    use void_ir::NamespaceId;

    #[test]
    fn test_apply_entity_create() {
        let mut applicator = PatchApplicator::new();
        let mut world = World::new();
        let mut layer_manager = LayerManager::new(10);
        let mut asset_registry = AssetRegistry::new();

        let ns = NamespaceId::new();
        let tx = TransactionBuilder::new(ns)
            .patch(Patch::new(
                ns,
                PatchKind::Entity(EntityPatch::create(ns, 1)),
            ))
            .build();

        let result = applicator.apply(tx, &mut world, &mut layer_manager, &mut asset_registry);

        assert!(result.success);
        assert_eq!(result.patches_applied, 1);
        assert!(result.entity_map.contains_key(&1));
    }
}
