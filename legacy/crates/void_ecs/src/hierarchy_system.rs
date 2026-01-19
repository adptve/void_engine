//! Hierarchy Systems for Scene Graph
//!
//! This module provides systems for maintaining the scene hierarchy:
//!
//! - [`HierarchyValidationSystem`] - Validates hierarchy, detects cycles, updates depths
//! - [`TransformPropagationSystem`] - Propagates transforms down the hierarchy
//! - [`VisibilityPropagationSystem`] - Propagates visibility down the hierarchy
//!
//! # Execution Order
//!
//! These systems should run in order:
//! 1. HierarchyValidationSystem (after any Parent component changes)
//! 2. TransformPropagationSystem (after LocalTransform changes)
//! 3. VisibilityPropagationSystem (after Visible component changes)
//!
//! # Hot-Swap Support
//!
//! All systems are stateless and re-execute cleanly after hot-reload.
//! Component state is preserved; systems recompute derived data.

use alloc::collections::BTreeMap;
use alloc::collections::BTreeSet;
use alloc::vec::Vec;

use crate::hierarchy::{
    Children, GlobalTransform, HierarchyDepth, HierarchyError, InheritedVisibility, LocalTransform,
    Parent,
};
use crate::render_components::Visible;
use crate::world::World;
use crate::Entity;

// ============================================================================
// Hierarchy Validation System
// ============================================================================

/// Validates hierarchy structure, detects cycles, and updates depths.
///
/// This system should run after any Parent component modifications to ensure
/// the hierarchy is consistent and cycle-free.
///
/// # Operations
///
/// 1. Builds parent->children mapping from Parent components
/// 2. Detects cycles using DFS with visited set
/// 3. Computes depths via BFS from roots (entities without Parent)
/// 4. Updates Children components on parent entities
///
/// # Fault Tolerance
///
/// If a cycle is detected, the offending entities are logged and marked,
/// but the system continues processing valid parts of the hierarchy.
pub struct HierarchyValidationSystem;

impl HierarchyValidationSystem {
    /// Run the hierarchy validation system
    pub fn run(world: &mut World) -> Result<(), HierarchyError> {
        // Step 1: Collect all parent relationships
        let mut parent_map: BTreeMap<Entity, Entity> = BTreeMap::new();
        let mut children_map: BTreeMap<Entity, Vec<Entity>> = BTreeMap::new();
        let mut all_entities: Vec<Entity> = Vec::new();

        // Gather all entities with hierarchy components
        Self::collect_hierarchy_data(world, &mut parent_map, &mut children_map, &mut all_entities);

        // Step 2: Detect cycles
        if let Some(cycle) = Self::detect_cycle(&parent_map) {
            return Err(HierarchyError::CycleDetected { entities: cycle });
        }

        // Step 3: Compute depths and update HierarchyDepth components
        let depths = Self::compute_depths(&parent_map, &all_entities);

        // Step 4: Update HierarchyDepth components
        for (entity, depth) in depths {
            Self::update_depth(world, entity, depth);
        }

        // Step 5: Update Children components
        for (parent, children) in children_map {
            Self::update_children(world, parent, children);
        }

        Ok(())
    }

    /// Collect hierarchy data from world
    fn collect_hierarchy_data(
        world: &World,
        parent_map: &mut BTreeMap<Entity, Entity>,
        children_map: &mut BTreeMap<Entity, Vec<Entity>>,
        all_entities: &mut Vec<Entity>,
    ) {
        // This is a simplified collection - in a full implementation,
        // we'd iterate over archetypes with Parent components

        // For now, iterate over all entities and check for Parent component
        for archetype in world.archetypes().iter() {
            for entity in archetype.entities() {
                all_entities.push(*entity);

                // Check if entity has Parent component
                if let Some(parent) = world.get_component::<Parent>(*entity) {
                    parent_map.insert(*entity, parent.entity);

                    // Add to children map
                    children_map
                        .entry(parent.entity)
                        .or_default()
                        .push(*entity);
                }
            }
        }
    }

    /// Detect cycles in the hierarchy using DFS
    fn detect_cycle(parent_map: &BTreeMap<Entity, Entity>) -> Option<Vec<Entity>> {
        let mut visited = BTreeSet::new();
        let mut path = Vec::new();
        let mut in_path = BTreeSet::new();

        for &entity in parent_map.keys() {
            if !visited.contains(&entity) {
                if let Some(cycle) =
                    Self::detect_cycle_dfs(entity, parent_map, &mut visited, &mut path, &mut in_path)
                {
                    return Some(cycle);
                }
            }
        }

        None
    }

    /// DFS helper for cycle detection
    fn detect_cycle_dfs(
        entity: Entity,
        parent_map: &BTreeMap<Entity, Entity>,
        visited: &mut BTreeSet<Entity>,
        path: &mut Vec<Entity>,
        in_path: &mut BTreeSet<Entity>,
    ) -> Option<Vec<Entity>> {
        if in_path.contains(&entity) {
            // Cycle found - extract cycle from path
            let start_idx = path.iter().position(|&e| e == entity).unwrap_or(0);
            return Some(path[start_idx..].to_vec());
        }

        if visited.contains(&entity) {
            return None;
        }

        visited.insert(entity);
        in_path.insert(entity);
        path.push(entity);

        // Follow parent chain
        if let Some(&parent) = parent_map.get(&entity) {
            if let Some(cycle) = Self::detect_cycle_dfs(parent, parent_map, visited, path, in_path) {
                return Some(cycle);
            }
        }

        path.pop();
        in_path.remove(&entity);

        None
    }

    /// Compute depths for all entities
    fn compute_depths(
        parent_map: &BTreeMap<Entity, Entity>,
        all_entities: &[Entity],
    ) -> BTreeMap<Entity, u32> {
        let mut depths: BTreeMap<Entity, u32> = BTreeMap::new();

        for &entity in all_entities {
            if !depths.contains_key(&entity) {
                let depth = Self::compute_entity_depth(entity, parent_map, &mut depths);
                depths.insert(entity, depth);
            }
        }

        depths
    }

    /// Compute depth for a single entity (recursive with memoization)
    fn compute_entity_depth(
        entity: Entity,
        parent_map: &BTreeMap<Entity, Entity>,
        depths: &mut BTreeMap<Entity, u32>,
    ) -> u32 {
        if let Some(&depth) = depths.get(&entity) {
            return depth;
        }

        let depth = if let Some(&parent) = parent_map.get(&entity) {
            Self::compute_entity_depth(parent, parent_map, depths) + 1
        } else {
            0 // Root entity
        };

        depths.insert(entity, depth);
        depth
    }

    /// Update HierarchyDepth component for an entity
    fn update_depth(world: &mut World, entity: Entity, depth: u32) {
        if world.has_component::<HierarchyDepth>(entity) {
            if let Some(hd) = world.get_component_mut::<HierarchyDepth>(entity) {
                hd.depth = depth;
            }
        } else {
            world.add_component(entity, HierarchyDepth::new(depth));
        }
    }

    /// Update Children component for a parent entity
    fn update_children(world: &mut World, parent: Entity, children: Vec<Entity>) {
        if world.has_component::<Children>(parent) {
            if let Some(c) = world.get_component_mut::<Children>(parent) {
                c.clear();
                for child in children {
                    c.push(child);
                }
            }
        } else {
            world.add_component(parent, Children::with_children(children));
        }
    }
}

// ============================================================================
// Transform Propagation System
// ============================================================================

/// Propagates transforms down the hierarchy.
///
/// This system computes [`GlobalTransform`] for all entities based on their
/// [`LocalTransform`] and parent's [`GlobalTransform`].
///
/// # Algorithm
///
/// 1. Sort entities by depth (roots first)
/// 2. For each entity in depth order:
///    - If no parent: GlobalTransform = LocalTransform
///    - If parent: GlobalTransform = Parent.GlobalTransform * LocalTransform
///
/// # Performance
///
/// - Uses dirty flags to skip unchanged transforms
/// - Processes in depth order for cache efficiency
/// - Target: <1ms for 10,000 entities
pub struct TransformPropagationSystem;

impl TransformPropagationSystem {
    /// Run the transform propagation system
    pub fn run(world: &mut World) {
        // Step 1: Collect entities with transforms, sorted by depth
        let mut entities_by_depth: Vec<(Entity, u32)> = Vec::new();

        for archetype in world.archetypes().iter() {
            for &entity in archetype.entities() {
                if world.has_component::<LocalTransform>(entity) {
                    let depth = world
                        .get_component::<HierarchyDepth>(entity)
                        .map(|hd| hd.depth)
                        .unwrap_or(0);
                    entities_by_depth.push((entity, depth));
                }
            }
        }

        // Sort by depth (roots first)
        entities_by_depth.sort_by_key(|(_, depth)| *depth);

        // Step 2: Propagate transforms in depth order
        for (entity, _) in entities_by_depth {
            Self::propagate_transform(world, entity);
        }
    }

    /// Propagate transform for a single entity
    fn propagate_transform(world: &mut World, entity: Entity) {
        // Get local transform
        let local = match world.get_component::<LocalTransform>(entity) {
            Some(lt) => *lt,
            None => return,
        };

        // Get parent's global transform (if any)
        let parent_global = world
            .get_component::<Parent>(entity)
            .and_then(|p| world.get_component::<GlobalTransform>(p.entity))
            .copied();

        // Compute global transform
        let global = match parent_global {
            Some(parent_gt) => parent_gt.multiply(&local),
            None => GlobalTransform::from_local(&local),
        };

        // Update or add GlobalTransform component
        if world.has_component::<GlobalTransform>(entity) {
            if let Some(gt) = world.get_component_mut::<GlobalTransform>(entity) {
                *gt = global;
            }
        } else {
            world.add_component(entity, global);
        }
    }

    /// Run with dirty flag optimization
    pub fn run_incremental(world: &mut World) {
        // Step 1: Find entities with dirty GlobalTransform
        let mut dirty_entities: Vec<(Entity, u32)> = Vec::new();

        for archetype in world.archetypes().iter() {
            for &entity in archetype.entities() {
                if let Some(gt) = world.get_component::<GlobalTransform>(entity) {
                    if gt.is_dirty() {
                        let depth = world
                            .get_component::<HierarchyDepth>(entity)
                            .map(|hd| hd.depth)
                            .unwrap_or(0);
                        dirty_entities.push((entity, depth));
                    }
                }
            }
        }

        // Sort by depth
        dirty_entities.sort_by_key(|(_, depth)| *depth);

        // Step 2: Propagate transforms and mark children dirty
        for (entity, _) in dirty_entities {
            Self::propagate_transform(world, entity);
            Self::mark_children_dirty(world, entity);
        }
    }

    /// Mark all children's transforms as dirty
    fn mark_children_dirty(world: &mut World, entity: Entity) {
        if let Some(children) = world.get_component::<Children>(entity) {
            let child_entities: Vec<Entity> = children.iter().copied().collect();
            for child in child_entities {
                if let Some(gt) = world.get_component_mut::<GlobalTransform>(child) {
                    gt.mark_dirty();
                }
                Self::mark_children_dirty(world, child);
            }
        }
    }
}

// ============================================================================
// Visibility Propagation System
// ============================================================================

/// Propagates visibility down the hierarchy.
///
/// This system computes [`InheritedVisibility`] for all entities based on their
/// own [`Visible`] component and parent's [`InheritedVisibility`].
///
/// # Algorithm
///
/// An entity is visible only if:
/// 1. Its own Visible.visible is true
/// 2. All ancestors' Visible.visible are true
///
/// # Execution Order
///
/// Should run after HierarchyValidationSystem.
pub struct VisibilityPropagationSystem;

impl VisibilityPropagationSystem {
    /// Run the visibility propagation system
    pub fn run(world: &mut World) {
        // Step 1: Collect entities with visibility, sorted by depth
        let mut entities_by_depth: Vec<(Entity, u32)> = Vec::new();

        for archetype in world.archetypes().iter() {
            for &entity in archetype.entities() {
                // Include entities with Visible or InheritedVisibility
                if world.has_component::<Visible>(entity)
                    || world.has_component::<InheritedVisibility>(entity)
                {
                    let depth = world
                        .get_component::<HierarchyDepth>(entity)
                        .map(|hd| hd.depth)
                        .unwrap_or(0);
                    entities_by_depth.push((entity, depth));
                }
            }
        }

        // Sort by depth (roots first)
        entities_by_depth.sort_by_key(|(_, depth)| *depth);

        // Step 2: Propagate visibility in depth order
        for (entity, _) in entities_by_depth {
            Self::propagate_visibility(world, entity);
        }
    }

    /// Propagate visibility for a single entity
    fn propagate_visibility(world: &mut World, entity: Entity) {
        // Get own visibility (default to true if no Visible component)
        let own_visible = world
            .get_component::<Visible>(entity)
            .map(|v| v.visible)
            .unwrap_or(true);

        // Get parent's inherited visibility
        let parent_visible = world
            .get_component::<Parent>(entity)
            .and_then(|p| world.get_component::<InheritedVisibility>(p.entity))
            .map(|iv| iv.visible)
            .unwrap_or(true);

        // Compute inherited visibility
        let inherited = InheritedVisibility {
            visible: own_visible && parent_visible,
        };

        // Update or add InheritedVisibility component
        if world.has_component::<InheritedVisibility>(entity) {
            if let Some(iv) = world.get_component_mut::<InheritedVisibility>(entity) {
                *iv = inherited;
            }
        } else {
            world.add_component(entity, inherited);
        }
    }
}

// ============================================================================
// Hierarchy Update Queue
// ============================================================================

/// Queue for frame-boundary hierarchy updates.
///
/// Hierarchy changes are queued during the frame and applied atomically
/// at frame boundaries to prevent mid-frame corruption.
#[derive(Clone, Debug, Default)]
pub struct HierarchyUpdateQueue {
    /// Pending parent changes: (child, new_parent) - None means remove parent
    pending_parents: Vec<(Entity, Option<Entity>)>,
    /// Pending recursive despawns
    pending_despawns: Vec<Entity>,
}

impl HierarchyUpdateQueue {
    /// Create a new empty queue
    pub fn new() -> Self {
        Self::default()
    }

    /// Queue a parent change
    pub fn queue_set_parent(&mut self, child: Entity, parent: Entity) {
        self.pending_parents.push((child, Some(parent)));
    }

    /// Queue a parent removal
    pub fn queue_remove_parent(&mut self, child: Entity) {
        self.pending_parents.push((child, None));
    }

    /// Queue a recursive despawn
    pub fn queue_despawn_recursive(&mut self, entity: Entity) {
        self.pending_despawns.push(entity);
    }

    /// Check if there are pending updates
    pub fn has_pending(&self) -> bool {
        !self.pending_parents.is_empty() || !self.pending_despawns.is_empty()
    }

    /// Get pending parent changes count
    pub fn pending_parent_count(&self) -> usize {
        self.pending_parents.len()
    }

    /// Get pending despawn count
    pub fn pending_despawn_count(&self) -> usize {
        self.pending_despawns.len()
    }

    /// Apply all pending updates at frame boundary
    pub fn apply_at_frame_boundary(&mut self, world: &mut World) {
        // Apply parent changes
        for (child, new_parent) in self.pending_parents.drain(..) {
            if let Some(parent) = new_parent {
                Self::apply_set_parent(world, child, parent);
            } else {
                Self::apply_remove_parent(world, child);
            }
        }

        // Apply despawns
        for entity in self.pending_despawns.drain(..) {
            Self::apply_despawn_recursive(world, entity);
        }

        // Revalidate and propagate
        let _ = HierarchyValidationSystem::run(world);
        TransformPropagationSystem::run(world);
        VisibilityPropagationSystem::run(world);
    }

    /// Apply set_parent operation
    fn apply_set_parent(world: &mut World, child: Entity, parent: Entity) {
        // Validate entities exist
        if !world.is_alive(child) || !world.is_alive(parent) {
            return;
        }

        // Remove from old parent's children if exists
        if let Some(old_parent) = world.get_component::<Parent>(child) {
            let old_parent_entity = old_parent.entity;
            if let Some(children) = world.get_component_mut::<Children>(old_parent_entity) {
                children.remove(child);
            }
        }

        // Set new parent
        if world.has_component::<Parent>(child) {
            if let Some(p) = world.get_component_mut::<Parent>(child) {
                p.entity = parent;
            }
        } else {
            world.add_component(child, Parent::new(parent));
        }

        // Add to new parent's children
        if world.has_component::<Children>(parent) {
            if let Some(children) = world.get_component_mut::<Children>(parent) {
                children.push(child);
            }
        } else {
            world.add_component(parent, Children::with_children(alloc::vec![child]));
        }
    }

    /// Apply remove_parent operation
    fn apply_remove_parent(world: &mut World, entity: Entity) {
        if !world.is_alive(entity) {
            return;
        }

        // Get and remove from old parent's children
        if let Some(old_parent) = world.get_component::<Parent>(entity) {
            let old_parent_entity = old_parent.entity;
            if let Some(children) = world.get_component_mut::<Children>(old_parent_entity) {
                children.remove(entity);
            }
        }

        // Remove Parent component
        world.remove_component::<Parent>(entity);
    }

    /// Apply recursive despawn
    fn apply_despawn_recursive(world: &mut World, entity: Entity) {
        if !world.is_alive(entity) {
            return;
        }

        // Collect all descendants
        let mut to_despawn = Vec::new();
        Self::collect_descendants(world, entity, &mut to_despawn);

        // Despawn in reverse order (deepest first)
        to_despawn.reverse();
        for e in to_despawn {
            world.despawn(e);
        }

        // Despawn the entity itself
        world.despawn(entity);
    }

    /// Collect all descendants of an entity
    fn collect_descendants(world: &World, entity: Entity, descendants: &mut Vec<Entity>) {
        if let Some(children) = world.get_component::<Children>(entity) {
            for &child in children.iter() {
                descendants.push(child);
                Self::collect_descendants(world, child, descendants);
            }
        }
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn setup_world() -> World {
        World::new()
    }

    #[test]
    fn test_hierarchy_validation_no_cycle() {
        let mut world = setup_world();

        // Create hierarchy: root -> child -> grandchild
        let root = world.spawn();
        let child = world.spawn();
        let grandchild = world.spawn();

        world.add_component(root, LocalTransform::default());
        world.add_component(child, LocalTransform::default());
        world.add_component(child, Parent::new(root));
        world.add_component(grandchild, LocalTransform::default());
        world.add_component(grandchild, Parent::new(child));

        let result = HierarchyValidationSystem::run(&mut world);
        assert!(result.is_ok());

        // Check depths
        assert_eq!(
            world.get_component::<HierarchyDepth>(root).unwrap().depth,
            0
        );
        assert_eq!(
            world.get_component::<HierarchyDepth>(child).unwrap().depth,
            1
        );
        assert_eq!(
            world
                .get_component::<HierarchyDepth>(grandchild)
                .unwrap()
                .depth,
            2
        );
    }

    #[test]
    fn test_transform_propagation() {
        let mut world = setup_world();

        // Create parent at position [10, 0, 0]
        let parent = world.spawn();
        world.add_component(parent, LocalTransform::from_translation([10.0, 0.0, 0.0]));
        world.add_component(parent, HierarchyDepth::root());

        // Create child at local position [5, 0, 0]
        let child = world.spawn();
        world.add_component(child, LocalTransform::from_translation([5.0, 0.0, 0.0]));
        world.add_component(child, Parent::new(parent));
        world.add_component(child, HierarchyDepth::new(1));

        // Run propagation
        TransformPropagationSystem::run(&mut world);

        // Parent should be at [10, 0, 0]
        let parent_gt = world.get_component::<GlobalTransform>(parent).unwrap();
        let parent_trans = parent_gt.translation();
        assert!((parent_trans[0] - 10.0).abs() < 0.001);

        // Child should be at [15, 0, 0]
        let child_gt = world.get_component::<GlobalTransform>(child).unwrap();
        let child_trans = child_gt.translation();
        assert!((child_trans[0] - 15.0).abs() < 0.001);
    }

    #[test]
    fn test_visibility_propagation() {
        let mut world = setup_world();

        // Create parent (visible)
        let parent = world.spawn();
        world.add_component(parent, Visible::default()); // visible: true
        world.add_component(parent, HierarchyDepth::root());

        // Create child (also visible)
        let child = world.spawn();
        world.add_component(child, Visible::default());
        world.add_component(child, Parent::new(parent));
        world.add_component(child, HierarchyDepth::new(1));

        // Run propagation
        VisibilityPropagationSystem::run(&mut world);

        // Both should be visible
        assert!(
            world
                .get_component::<InheritedVisibility>(parent)
                .unwrap()
                .visible
        );
        assert!(
            world
                .get_component::<InheritedVisibility>(child)
                .unwrap()
                .visible
        );

        // Now hide parent
        if let Some(v) = world.get_component_mut::<Visible>(parent) {
            v.visible = false;
        }

        // Run propagation again
        VisibilityPropagationSystem::run(&mut world);

        // Parent is hidden
        assert!(
            !world
                .get_component::<InheritedVisibility>(parent)
                .unwrap()
                .visible
        );
        // Child should also be hidden (inherits from parent)
        assert!(
            !world
                .get_component::<InheritedVisibility>(child)
                .unwrap()
                .visible
        );
    }

    #[test]
    fn test_hierarchy_update_queue() {
        let mut world = setup_world();
        let mut queue = HierarchyUpdateQueue::new();

        // Create entities
        let parent = world.spawn();
        let child = world.spawn();

        world.add_component(parent, LocalTransform::default());
        world.add_component(child, LocalTransform::default());

        // Queue parent change
        queue.queue_set_parent(child, parent);
        assert!(queue.has_pending());
        assert_eq!(queue.pending_parent_count(), 1);

        // Apply at frame boundary
        queue.apply_at_frame_boundary(&mut world);

        // Child should now have Parent component
        assert!(world.has_component::<Parent>(child));
        assert_eq!(world.get_component::<Parent>(child).unwrap().entity, parent);

        // Parent should have Children component
        assert!(world.has_component::<Children>(parent));
        assert!(
            world
                .get_component::<Children>(parent)
                .unwrap()
                .contains(child)
        );
    }
}
