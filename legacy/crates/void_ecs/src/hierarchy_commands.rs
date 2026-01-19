//! Hierarchy Commands API
//!
//! This module provides a safe, validated API for manipulating the entity hierarchy.
//! All operations validate inputs and prevent invalid states like cycles.
//!
//! # Usage
//!
//! ```ignore
//! use void_ecs::hierarchy_commands::HierarchyCommands;
//!
//! let mut commands = HierarchyCommands::new(&mut world);
//!
//! // Set parent-child relationship
//! commands.set_parent(child, parent)?;
//!
//! // Remove from hierarchy
//! commands.remove_parent(child);
//!
//! // Despawn entity and all descendants
//! commands.despawn_recursive(entity);
//! ```
//!
//! # Hot-Swap Support
//!
//! The commands API is stateless and works correctly after hot-reload.
//! Component state is preserved; the API operates on the reloaded data.

use alloc::vec::Vec;

use crate::hierarchy::{Children, GlobalTransform, HierarchyDepth, HierarchyError, LocalTransform, Parent};
use crate::world::World;
use crate::Entity;

/// Commands for manipulating the entity hierarchy.
///
/// This provides a safe API that validates all operations and prevents
/// invalid hierarchy states like cycles.
pub struct HierarchyCommands<'w> {
    world: &'w mut World,
}

impl<'w> HierarchyCommands<'w> {
    /// Create a new HierarchyCommands with access to the world
    pub fn new(world: &'w mut World) -> Self {
        Self { world }
    }

    /// Set an entity's parent.
    ///
    /// This operation:
    /// 1. Validates both entities exist
    /// 2. Checks for cycles (child cannot be ancestor of parent)
    /// 3. Removes child from old parent's Children (if any)
    /// 4. Sets Parent component on child
    /// 5. Adds child to new parent's Children
    ///
    /// # Errors
    ///
    /// Returns an error if:
    /// - Either entity doesn't exist
    /// - Setting the parent would create a cycle
    ///
    /// # Example
    ///
    /// ```ignore
    /// let mut commands = HierarchyCommands::new(&mut world);
    /// commands.set_parent(child, parent)?;
    /// ```
    pub fn set_parent(&mut self, child: Entity, parent: Entity) -> Result<(), HierarchyError> {
        // Validate entities exist
        if !self.world.is_alive(child) {
            return Err(HierarchyError::EntityNotFound { entity: child });
        }
        if !self.world.is_alive(parent) {
            return Err(HierarchyError::InvalidParent { child, parent });
        }

        // Cannot parent to self
        if child == parent {
            return Err(HierarchyError::CycleDetected {
                entities: alloc::vec![child],
            });
        }

        // Check for cycles: child cannot be an ancestor of parent
        if self.is_ancestor(parent, child) {
            return Err(HierarchyError::CycleDetected {
                entities: alloc::vec![child, parent],
            });
        }

        // Remove from old parent's children (if any)
        if let Some(old_parent) = self.world.get_component::<Parent>(child) {
            let old_parent_entity = old_parent.entity;
            if old_parent_entity != parent {
                if let Some(children) = self.world.get_component_mut::<Children>(old_parent_entity) {
                    children.remove(child);
                }
            }
        }

        // Set Parent component on child
        if self.world.has_component::<Parent>(child) {
            if let Some(p) = self.world.get_component_mut::<Parent>(child) {
                p.entity = parent;
            }
        } else {
            self.world.add_component(child, Parent::new(parent));
        }

        // Add to new parent's Children
        if self.world.has_component::<Children>(parent) {
            if let Some(children) = self.world.get_component_mut::<Children>(parent) {
                children.push(child);
            }
        } else {
            self.world
                .add_component(parent, Children::with_children(alloc::vec![child]));
        }

        // Update child's depth
        let parent_depth = self
            .world
            .get_component::<HierarchyDepth>(parent)
            .map(|hd| hd.depth)
            .unwrap_or(0);

        if self.world.has_component::<HierarchyDepth>(child) {
            if let Some(hd) = self.world.get_component_mut::<HierarchyDepth>(child) {
                hd.depth = parent_depth + 1;
            }
        } else {
            self.world
                .add_component(child, HierarchyDepth::new(parent_depth + 1));
        }

        // Mark child's GlobalTransform as dirty
        if let Some(gt) = self.world.get_component_mut::<GlobalTransform>(child) {
            gt.mark_dirty();
        }

        Ok(())
    }

    /// Remove an entity from the hierarchy (make it a root entity).
    ///
    /// This removes the Parent component and updates the old parent's Children.
    ///
    /// # Example
    ///
    /// ```ignore
    /// let mut commands = HierarchyCommands::new(&mut world);
    /// commands.remove_parent(child);
    /// ```
    pub fn remove_parent(&mut self, entity: Entity) {
        if !self.world.is_alive(entity) {
            return;
        }

        // Get old parent before removing
        let old_parent = self
            .world
            .get_component::<Parent>(entity)
            .map(|p| p.entity);

        // Remove from old parent's children
        if let Some(parent_entity) = old_parent {
            if let Some(children) = self.world.get_component_mut::<Children>(parent_entity) {
                children.remove(entity);
            }
        }

        // Remove Parent component
        self.world.remove_component::<Parent>(entity);

        // Update depth to 0 (now a root)
        if let Some(hd) = self.world.get_component_mut::<HierarchyDepth>(entity) {
            hd.depth = 0;
        }

        // Mark GlobalTransform as dirty
        if let Some(gt) = self.world.get_component_mut::<GlobalTransform>(entity) {
            gt.mark_dirty();
        }
    }

    /// Despawn an entity and all its descendants.
    ///
    /// Entities are despawned from deepest to shallowest to ensure
    /// parent entities still exist when children are removed.
    ///
    /// # Example
    ///
    /// ```ignore
    /// let mut commands = HierarchyCommands::new(&mut world);
    /// commands.despawn_recursive(entity);
    /// ```
    pub fn despawn_recursive(&mut self, entity: Entity) {
        if !self.world.is_alive(entity) {
            return;
        }

        // Collect all descendants (depth-first)
        let mut to_despawn = Vec::new();
        self.collect_descendants(entity, &mut to_despawn);

        // Despawn in reverse order (deepest first)
        to_despawn.reverse();
        for e in to_despawn {
            self.world.despawn(e);
        }

        // Remove entity from parent's children
        if let Some(parent) = self.world.get_component::<Parent>(entity) {
            let parent_entity = parent.entity;
            if let Some(children) = self.world.get_component_mut::<Children>(parent_entity) {
                children.remove(entity);
            }
        }

        // Despawn the entity itself
        self.world.despawn(entity);
    }

    /// Get the root ancestor of an entity.
    ///
    /// Follows the parent chain to find the topmost entity.
    /// Returns the entity itself if it has no parent.
    ///
    /// # Example
    ///
    /// ```ignore
    /// let root = commands.get_root(deeply_nested_entity);
    /// ```
    pub fn get_root(&self, entity: Entity) -> Entity {
        let mut current = entity;
        let mut visited = 0;
        const MAX_DEPTH: usize = 1000; // Safety limit

        while visited < MAX_DEPTH {
            if let Some(parent) = self.world.get_component::<Parent>(current) {
                current = parent.entity;
                visited += 1;
            } else {
                break;
            }
        }

        current
    }

    /// Check if `ancestor` is an ancestor of `entity`.
    ///
    /// Returns true if `ancestor` appears anywhere in `entity`'s parent chain.
    ///
    /// # Example
    ///
    /// ```ignore
    /// if commands.is_ancestor(child, potential_parent) {
    ///     // potential_parent is somewhere above child in the hierarchy
    /// }
    /// ```
    pub fn is_ancestor(&self, entity: Entity, ancestor: Entity) -> bool {
        let mut current = entity;
        let mut visited = 0;
        const MAX_DEPTH: usize = 1000; // Safety limit

        while visited < MAX_DEPTH {
            if let Some(parent) = self.world.get_component::<Parent>(current) {
                if parent.entity == ancestor {
                    return true;
                }
                current = parent.entity;
                visited += 1;
            } else {
                break;
            }
        }

        false
    }

    /// Check if `descendant` is a descendant of `entity`.
    ///
    /// Returns true if `entity` appears anywhere in `descendant`'s parent chain.
    ///
    /// # Example
    ///
    /// ```ignore
    /// if commands.is_descendant(potential_child, parent) {
    ///     // parent is somewhere above potential_child in the hierarchy
    /// }
    /// ```
    pub fn is_descendant(&self, descendant: Entity, entity: Entity) -> bool {
        self.is_ancestor(descendant, entity)
    }

    /// Get the parent of an entity.
    ///
    /// # Example
    ///
    /// ```ignore
    /// if let Some(parent) = commands.get_parent(entity) {
    ///     // entity has a parent
    /// }
    /// ```
    pub fn get_parent(&self, entity: Entity) -> Option<Entity> {
        self.world
            .get_component::<Parent>(entity)
            .map(|p| p.entity)
    }

    /// Get the children of an entity.
    ///
    /// Returns an empty iterator if the entity has no children.
    ///
    /// # Example
    ///
    /// ```ignore
    /// for child in commands.children(parent) {
    ///     // process child
    /// }
    /// ```
    pub fn children(&self, entity: Entity) -> impl Iterator<Item = Entity> + '_ {
        self.world
            .get_component::<Children>(entity)
            .into_iter()
            .flat_map(|c| c.iter().copied())
    }

    /// Get the depth of an entity in the hierarchy.
    ///
    /// Root entities have depth 0, their children have depth 1, etc.
    ///
    /// # Example
    ///
    /// ```ignore
    /// let depth = commands.get_depth(entity);
    /// ```
    pub fn get_depth(&self, entity: Entity) -> u32 {
        self.world
            .get_component::<HierarchyDepth>(entity)
            .map(|hd| hd.depth)
            .unwrap_or(0)
    }

    /// Get all ancestors of an entity (from parent to root).
    ///
    /// # Example
    ///
    /// ```ignore
    /// let ancestors = commands.ancestors(entity);
    /// // ancestors[0] is direct parent, last is root
    /// ```
    pub fn ancestors(&self, entity: Entity) -> Vec<Entity> {
        let mut ancestors = Vec::new();
        let mut current = entity;
        let mut visited = 0;
        const MAX_DEPTH: usize = 1000;

        while visited < MAX_DEPTH {
            if let Some(parent) = self.world.get_component::<Parent>(current) {
                ancestors.push(parent.entity);
                current = parent.entity;
                visited += 1;
            } else {
                break;
            }
        }

        ancestors
    }

    /// Get all descendants of an entity (breadth-first).
    ///
    /// # Example
    ///
    /// ```ignore
    /// let descendants = commands.descendants(entity);
    /// ```
    pub fn descendants(&self, entity: Entity) -> Vec<Entity> {
        let mut descendants = Vec::new();
        self.collect_descendants(entity, &mut descendants);
        descendants
    }

    /// Get the number of direct children.
    ///
    /// # Example
    ///
    /// ```ignore
    /// let count = commands.child_count(entity);
    /// ```
    pub fn child_count(&self, entity: Entity) -> usize {
        self.world
            .get_component::<Children>(entity)
            .map(|c| c.len())
            .unwrap_or(0)
    }

    /// Check if an entity is a root (has no parent).
    ///
    /// # Example
    ///
    /// ```ignore
    /// if commands.is_root(entity) {
    ///     // entity is a root entity
    /// }
    /// ```
    pub fn is_root(&self, entity: Entity) -> bool {
        !self.world.has_component::<Parent>(entity)
    }

    /// Check if an entity is a leaf (has no children).
    ///
    /// # Example
    ///
    /// ```ignore
    /// if commands.is_leaf(entity) {
    ///     // entity has no children
    /// }
    /// ```
    pub fn is_leaf(&self, entity: Entity) -> bool {
        self.world
            .get_component::<Children>(entity)
            .map(|c| c.is_empty())
            .unwrap_or(true)
    }

    /// Add a child entity to a parent with a local transform.
    ///
    /// This is a convenience method that:
    /// 1. Sets the parent relationship
    /// 2. Ensures LocalTransform exists
    /// 3. Ensures GlobalTransform exists
    ///
    /// # Example
    ///
    /// ```ignore
    /// commands.add_child(parent, child, LocalTransform::from_translation([0.0, 1.0, 0.0]))?;
    /// ```
    pub fn add_child(
        &mut self,
        parent: Entity,
        child: Entity,
        local_transform: LocalTransform,
    ) -> Result<(), HierarchyError> {
        // Set parent relationship
        self.set_parent(child, parent)?;

        // Ensure LocalTransform
        if self.world.has_component::<LocalTransform>(child) {
            if let Some(lt) = self.world.get_component_mut::<LocalTransform>(child) {
                *lt = local_transform;
            }
        } else {
            self.world.add_component(child, local_transform);
        }

        // Ensure GlobalTransform (will be computed by TransformPropagationSystem)
        if !self.world.has_component::<GlobalTransform>(child) {
            self.world
                .add_component(child, GlobalTransform::from_local(&local_transform));
        }

        Ok(())
    }

    /// Collect all descendants of an entity (internal helper).
    fn collect_descendants(&self, entity: Entity, descendants: &mut Vec<Entity>) {
        if let Some(children) = self.world.get_component::<Children>(entity) {
            for &child in children.iter() {
                descendants.push(child);
                self.collect_descendants(child, descendants);
            }
        }
    }
}

// ============================================================================
// Builder Pattern for Creating Hierarchies
// ============================================================================

/// Builder for creating entity hierarchies.
///
/// Provides a fluent API for building complex hierarchies.
///
/// # Example
///
/// ```ignore
/// let root = HierarchyBuilder::new(&mut world)
///     .spawn_with(LocalTransform::from_translation([0.0, 0.0, 0.0]))
///     .with_child(|builder| {
///         builder.spawn_with(LocalTransform::from_translation([1.0, 0.0, 0.0]))
///     })
///     .with_child(|builder| {
///         builder.spawn_with(LocalTransform::from_translation([-1.0, 0.0, 0.0]))
///             .with_child(|builder| {
///                 builder.spawn_with(LocalTransform::from_translation([0.0, 1.0, 0.0]))
///             })
///     })
///     .build();
/// ```
pub struct HierarchyBuilder<'w> {
    world: &'w mut World,
    entity: Option<Entity>,
    children: Vec<Entity>,
}

impl<'w> HierarchyBuilder<'w> {
    /// Create a new hierarchy builder
    pub fn new(world: &'w mut World) -> Self {
        Self {
            world,
            entity: None,
            children: Vec::new(),
        }
    }

    /// Spawn the root entity with a local transform
    pub fn spawn_with(mut self, transform: LocalTransform) -> Self {
        let entity = self.world.spawn();
        self.world.add_component(entity, transform);
        self.world
            .add_component(entity, GlobalTransform::from_local(&transform));
        self.world.add_component(entity, HierarchyDepth::root());
        self.entity = Some(entity);
        self
    }

    /// Spawn the root entity with an existing entity
    pub fn with_entity(mut self, entity: Entity) -> Self {
        self.entity = Some(entity);
        self
    }

    /// Add a child using a builder closure
    pub fn with_child<F>(mut self, f: F) -> Self
    where
        F: FnOnce(ChildBuilder<'_>) -> ChildBuilder<'_>,
    {
        if let Some(parent) = self.entity {
            let child_builder = ChildBuilder::new(self.world, parent);
            let completed = f(child_builder);
            if let Some(child) = completed.entity {
                self.children.push(child);
            }
        }
        self
    }

    /// Build and return the root entity
    pub fn build(self) -> Option<Entity> {
        self.entity
    }
}

/// Builder for child entities within a hierarchy.
pub struct ChildBuilder<'w> {
    world: &'w mut World,
    parent: Entity,
    entity: Option<Entity>,
}

impl<'w> ChildBuilder<'w> {
    /// Create a new child builder
    fn new(world: &'w mut World, parent: Entity) -> Self {
        Self {
            world,
            parent,
            entity: None,
        }
    }

    /// Spawn this child with a local transform
    pub fn spawn_with(mut self, transform: LocalTransform) -> Self {
        let entity = self.world.spawn();
        self.world.add_component(entity, transform);
        self.world.add_component(entity, Parent::new(self.parent));

        // Compute global transform
        let parent_global = self
            .world
            .get_component::<GlobalTransform>(self.parent)
            .copied()
            .unwrap_or_default();
        self.world
            .add_component(entity, parent_global.multiply(&transform));

        // Update depth
        let parent_depth = self
            .world
            .get_component::<HierarchyDepth>(self.parent)
            .map(|hd| hd.depth)
            .unwrap_or(0);
        self.world
            .add_component(entity, HierarchyDepth::new(parent_depth + 1));

        // Add to parent's children
        if self.world.has_component::<Children>(self.parent) {
            if let Some(children) = self.world.get_component_mut::<Children>(self.parent) {
                children.push(entity);
            }
        } else {
            self.world
                .add_component(self.parent, Children::with_children(alloc::vec![entity]));
        }

        self.entity = Some(entity);
        self
    }

    /// Add a child to this child
    pub fn with_child<F>(mut self, f: F) -> Self
    where
        F: FnOnce(ChildBuilder<'_>) -> ChildBuilder<'_>,
    {
        if let Some(entity) = self.entity {
            let child_builder = ChildBuilder::new(self.world, entity);
            f(child_builder);
        }
        self
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_set_parent() {
        let mut world = World::new();

        let parent = world.spawn();
        let child = world.spawn();

        world.add_component(parent, LocalTransform::default());
        world.add_component(parent, HierarchyDepth::root());
        world.add_component(child, LocalTransform::default());

        let mut commands = HierarchyCommands::new(&mut world);
        let result = commands.set_parent(child, parent);

        assert!(result.is_ok());
        assert!(world.has_component::<Parent>(child));
        assert_eq!(world.get_component::<Parent>(child).unwrap().entity, parent);
    }

    #[test]
    fn test_cycle_detection() {
        let mut world = World::new();

        let a = world.spawn();
        let b = world.spawn();
        let c = world.spawn();

        // Create chain: a -> b -> c
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(b, a).unwrap();
        }
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(c, b).unwrap();
        }

        // Try to set a's parent to c (would create cycle)
        let result = {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(a, c)
        };

        assert!(matches!(result, Err(HierarchyError::CycleDetected { .. })));
    }

    #[test]
    fn test_is_ancestor() {
        let mut world = World::new();

        let root = world.spawn();
        let child = world.spawn();
        let grandchild = world.spawn();

        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(child, root).unwrap();
        }
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(grandchild, child).unwrap();
        }

        let commands = HierarchyCommands::new(&mut world);
        assert!(commands.is_ancestor(grandchild, root));
        assert!(commands.is_ancestor(grandchild, child));
        assert!(!commands.is_ancestor(root, grandchild));
    }

    #[test]
    fn test_despawn_recursive() {
        let mut world = World::new();

        let root = world.spawn();
        let child1 = world.spawn();
        let child2 = world.spawn();
        let grandchild = world.spawn();

        // Build hierarchy
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(child1, root).unwrap();
        }
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(child2, root).unwrap();
        }
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(grandchild, child1).unwrap();
        }

        assert_eq!(world.entity_count(), 4);

        // Despawn root recursively
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.despawn_recursive(root);
        }

        assert_eq!(world.entity_count(), 0);
    }

    #[test]
    fn test_get_root() {
        let mut world = World::new();

        let root = world.spawn();
        let child = world.spawn();
        let grandchild = world.spawn();

        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(child, root).unwrap();
        }
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(grandchild, child).unwrap();
        }

        let commands = HierarchyCommands::new(&mut world);
        assert_eq!(commands.get_root(grandchild), root);
        assert_eq!(commands.get_root(child), root);
        assert_eq!(commands.get_root(root), root);
    }

    #[test]
    fn test_ancestors() {
        let mut world = World::new();

        let root = world.spawn();
        let child = world.spawn();
        let grandchild = world.spawn();

        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(child, root).unwrap();
        }
        {
            let mut commands = HierarchyCommands::new(&mut world);
            commands.set_parent(grandchild, child).unwrap();
        }

        let commands = HierarchyCommands::new(&mut world);
        let ancestors = commands.ancestors(grandchild);

        assert_eq!(ancestors.len(), 2);
        assert_eq!(ancestors[0], child);
        assert_eq!(ancestors[1], root);
    }
}
