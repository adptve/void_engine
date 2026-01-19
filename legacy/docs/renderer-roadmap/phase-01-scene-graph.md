# Phase 1: Scene Graph (Hierarchical Entities)

## Status: Not Started

## User Story

> As a scene author, I want entities to be parented to other entities so that transforms, visibility, and motion propagate hierarchically.

## Requirements Checklist

- [ ] Entities may optionally reference a `parent` entity
- [ ] Local transforms are evaluated relative to the parent
- [ ] World transforms are derived automatically
- [ ] Visibility propagates from parent to children
- [ ] Support arbitrary depth (not limited to one level)
- [ ] Cycles must be prevented at validation time

## Current State Analysis

### Existing Components (void_ecs/src/render_components.rs)

```rust
// Already exists - needs enhancement
pub struct Parent {
    pub entity: Entity,  // Currently just stores reference
}

pub struct Children {
    pub entities: Vec<Entity>,  // Currently unmanaged
}

pub struct Transform {
    pub position: [f32; 3],
    pub rotation: [f32; 3],  // Euler angles
    pub scale: [f32; 3],
}

pub struct LocalTransform {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
}
```

### Gaps
1. No automatic world transform computation
2. No visibility propagation
3. No cycle detection
4. No hierarchy maintenance (children not auto-updated)
5. Transform uses Euler angles (should use quaternions for proper composition)

## Implementation Specification

### 1. Enhanced Components

```rust
// crates/void_ecs/src/hierarchy.rs (NEW FILE)

use crate::{Entity, Component};

/// Parent-child relationship. Adding this makes the entity a child.
#[derive(Clone, Debug)]
pub struct Parent {
    pub entity: Entity,
}

/// Automatically managed list of children.
/// DO NOT modify directly - use hierarchy APIs.
#[derive(Clone, Debug, Default)]
pub struct Children {
    entities: Vec<Entity>,
}

impl Children {
    pub fn iter(&self) -> impl Iterator<Item = &Entity> {
        self.entities.iter()
    }

    pub fn len(&self) -> usize {
        self.entities.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entities.is_empty()
    }
}

/// Local transform relative to parent (or world if no parent)
#[derive(Clone, Debug)]
pub struct LocalTransform {
    pub translation: [f32; 3],
    pub rotation: [f32; 4],  // Quaternion [x, y, z, w]
    pub scale: [f32; 3],
}

impl Default for LocalTransform {
    fn default() -> Self {
        Self {
            translation: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0, 1.0],  // Identity quaternion
            scale: [1.0, 1.0, 1.0],
        }
    }
}

/// Computed world-space transform. Read-only, updated by system.
#[derive(Clone, Debug)]
pub struct GlobalTransform {
    /// 4x4 column-major matrix
    pub matrix: [[f32; 4]; 4],
}

impl Default for GlobalTransform {
    fn default() -> Self {
        Self {
            matrix: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        }
    }
}

impl GlobalTransform {
    pub fn translation(&self) -> [f32; 3] {
        [self.matrix[3][0], self.matrix[3][1], self.matrix[3][2]]
    }

    pub fn from_local(local: &LocalTransform) -> Self {
        // Compose TRS matrix from local transform
        todo!("Implement TRS composition")
    }

    pub fn multiply(&self, child: &LocalTransform) -> Self {
        // Parent * Child composition
        todo!("Implement matrix multiplication")
    }
}

/// Marks entity for visibility propagation
#[derive(Clone, Debug, Default)]
pub struct InheritedVisibility {
    pub visible: bool,
}

/// Hierarchy depth for sorting (root = 0)
#[derive(Clone, Debug, Default)]
pub struct HierarchyDepth {
    pub depth: u32,
}
```

### 2. Hierarchy System

```rust
// crates/void_ecs/src/systems/hierarchy_system.rs (NEW FILE)

use crate::{World, Entity, Query};
use crate::hierarchy::*;

/// Validates hierarchy, detects cycles, updates depths
pub struct HierarchyValidationSystem;

impl HierarchyValidationSystem {
    pub fn run(world: &mut World) -> Result<(), HierarchyError> {
        // 1. Build parent->children map
        // 2. Detect cycles using DFS with visited set
        // 3. Compute depths via BFS from roots
        // 4. Update Children components
        todo!()
    }
}

/// Propagates transforms down the hierarchy
pub struct TransformPropagationSystem;

impl TransformPropagationSystem {
    pub fn run(world: &mut World) {
        // Process entities in depth order (roots first)
        // For each entity:
        //   1. Get parent's GlobalTransform (or identity)
        //   2. Compose with LocalTransform
        //   3. Store in GlobalTransform
        todo!()
    }
}

/// Propagates visibility down the hierarchy
pub struct VisibilityPropagationSystem;

impl VisibilityPropagationSystem {
    pub fn run(world: &mut World) {
        // For each entity in depth order:
        //   1. Get parent's InheritedVisibility
        //   2. AND with own Visible component
        //   3. Store in InheritedVisibility
        todo!()
    }
}

#[derive(Debug)]
pub enum HierarchyError {
    CycleDetected { entities: Vec<Entity> },
    InvalidParent { child: Entity, parent: Entity },
}
```

### 3. Hierarchy API

```rust
// crates/void_ecs/src/hierarchy_commands.rs (NEW FILE)

use crate::{World, Entity};
use crate::hierarchy::*;

pub struct HierarchyCommands<'w> {
    world: &'w mut World,
}

impl<'w> HierarchyCommands<'w> {
    /// Set entity's parent. Updates both Parent and Children components.
    pub fn set_parent(&mut self, child: Entity, parent: Entity) -> Result<(), HierarchyError> {
        // 1. Validate parent exists
        // 2. Check for cycles (child is not ancestor of parent)
        // 3. Remove from old parent's Children
        // 4. Add Parent component to child
        // 5. Add to new parent's Children
        // 6. Mark hierarchy dirty
        todo!()
    }

    /// Remove entity from hierarchy (becomes root)
    pub fn remove_parent(&mut self, entity: Entity) {
        // 1. Get current parent
        // 2. Remove from parent's Children
        // 3. Remove Parent component
        // 4. Mark hierarchy dirty
        todo!()
    }

    /// Despawn entity and all descendants
    pub fn despawn_recursive(&mut self, entity: Entity) {
        // 1. Collect all descendants (DFS)
        // 2. Despawn in reverse order (leaves first)
        todo!()
    }

    /// Get root ancestor of entity
    pub fn get_root(&self, entity: Entity) -> Entity {
        todo!()
    }

    /// Check if `ancestor` is an ancestor of `entity`
    pub fn is_ancestor(&self, entity: Entity, ancestor: Entity) -> bool {
        todo!()
    }
}
```

### 4. IR Patch Integration

```rust
// crates/void_ir/src/patch.rs - additions

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum HierarchyPatch {
    SetParent { child: EntityId, parent: EntityId },
    RemoveParent { entity: EntityId },
    DespawnRecursive { entity: EntityId },
}
```

### 5. Math Utilities (void_math)

```rust
// crates/void_math/src/transform.rs - additions

impl Transform {
    /// Compose parent * child transforms
    pub fn multiply(&self, child: &Transform) -> Transform {
        // Full TRS composition with quaternion multiplication
        todo!()
    }

    /// Convert to 4x4 matrix
    pub fn to_matrix(&self) -> Mat4 {
        // T * R * S composition
        todo!()
    }

    /// Inverse transform
    pub fn inverse(&self) -> Transform {
        todo!()
    }
}

impl Quat {
    /// Multiply quaternions
    pub fn multiply(&self, other: &Quat) -> Quat {
        todo!()
    }

    /// Rotate vector by quaternion
    pub fn rotate_vec3(&self, v: Vec3) -> Vec3 {
        todo!()
    }

    /// Convert to rotation matrix
    pub fn to_mat3(&self) -> Mat3 {
        todo!()
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/hierarchy.rs` | CREATE | New hierarchy components |
| `void_ecs/src/systems/hierarchy_system.rs` | CREATE | Hierarchy systems |
| `void_ecs/src/hierarchy_commands.rs` | CREATE | Hierarchy manipulation API |
| `void_ecs/src/lib.rs` | MODIFY | Export new modules |
| `void_ecs/src/render_components.rs` | MODIFY | Deprecate old Transform, add GlobalTransform |
| `void_math/src/transform.rs` | MODIFY | Add matrix composition |
| `void_math/src/quaternion.rs` | MODIFY | Add quaternion operations |
| `void_ir/src/patch.rs` | MODIFY | Add HierarchyPatch |
| `void_editor/src/panels/hierarchy.rs` | MODIFY | Use new hierarchy API |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_cycle_detection() {
    let mut world = World::new();
    let a = world.spawn(());
    let b = world.spawn((Parent { entity: a },));
    let c = world.spawn((Parent { entity: b },));

    // This should fail - c is ancestor of a
    let result = HierarchyCommands::new(&mut world).set_parent(a, c);
    assert!(matches!(result, Err(HierarchyError::CycleDetected { .. })));
}

#[test]
fn test_transform_propagation() {
    let mut world = World::new();
    let parent = world.spawn((
        LocalTransform { translation: [10.0, 0.0, 0.0], ..default() },
    ));
    let child = world.spawn((
        Parent { entity: parent },
        LocalTransform { translation: [5.0, 0.0, 0.0], ..default() },
    ));

    TransformPropagationSystem::run(&mut world);

    let child_global = world.get::<GlobalTransform>(child).unwrap();
    assert_eq!(child_global.translation(), [15.0, 0.0, 0.0]);
}

#[test]
fn test_visibility_propagation() {
    let mut world = World::new();
    let parent = world.spawn((Visible { visible: false },));
    let child = world.spawn((
        Parent { entity: parent },
        Visible { visible: true },  // Visible, but parent is not
    ));

    VisibilityPropagationSystem::run(&mut world);

    let inherited = world.get::<InheritedVisibility>(child).unwrap();
    assert!(!inherited.visible);  // Should inherit parent's invisibility
}

#[test]
fn test_deep_hierarchy() {
    let mut world = World::new();
    let mut entities = vec![world.spawn(())];

    // Create 100-deep hierarchy
    for i in 0..100 {
        let parent = entities[i];
        let child = world.spawn((Parent { entity: parent },));
        entities.push(child);
    }

    HierarchyValidationSystem::run(&mut world).unwrap();

    let depth = world.get::<HierarchyDepth>(entities[100]).unwrap();
    assert_eq!(depth.depth, 100);
}
```

### Integration Tests
```rust
#[test]
fn test_hierarchy_with_rendering() {
    // Full integration with extraction and render
}

#[test]
fn test_hierarchy_hot_reload() {
    // Verify hierarchy survives scene reload
}
```

## Performance Considerations

1. **Dirty Flags**: Only recompute transforms for dirty branches
2. **Parallel Processing**: Process independent subtrees concurrently
3. **Cache Locality**: Store hierarchy data contiguously
4. **Depth Sorting**: Pre-sort by depth to avoid random access

## Hot-Swap Support

All hierarchy components must support runtime hot-reload per the project's core philosophy.

### Serialization

```rust
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Parent {
    pub entity: Entity,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct Children {
    entities: Vec<Entity>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LocalTransform {
    pub translation: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct GlobalTransform {
    pub matrix: [[f32; 4]; 4],

    #[serde(skip)]
    pub dirty: bool,  // Transient - recomputed after reload
}
```

### HotReloadable Implementation

```rust
impl HotReloadable for GlobalTransform {
    fn snapshot(&self) -> Vec<u8> {
        bincode::serialize(self).unwrap()
    }

    fn restore(bytes: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(bytes).map_err(|e| HotReloadError::Deserialize(e.to_string()))
    }

    fn on_reload(&mut self) {
        // Mark dirty to force recomputation from LocalTransform
        self.dirty = true;
    }
}

impl HotReloadable for HierarchyDepth {
    fn on_reload(&mut self) {
        // Depths will be recomputed by HierarchyValidationSystem
        self.depth = 0;
    }
}
```

### Frame-Boundary Updates

Hierarchy changes are queued and applied atomically:

```rust
pub struct HierarchyUpdateQueue {
    pending_parents: Vec<(Entity, Option<Entity>)>,
    pending_despawns: Vec<Entity>,
}

impl HierarchyUpdateQueue {
    pub fn apply_at_frame_boundary(&mut self, world: &mut World) {
        // Apply all parent changes
        for (child, new_parent) in self.pending_parents.drain(..) {
            if let Some(parent) = new_parent {
                HierarchyCommands::new(world).set_parent(child, parent).ok();
            } else {
                HierarchyCommands::new(world).remove_parent(child);
            }
        }

        // Apply despawns
        for entity in self.pending_despawns.drain(..) {
            HierarchyCommands::new(world).despawn_recursive(entity);
        }

        // Revalidate and propagate
        HierarchyValidationSystem::run(world).ok();
        TransformPropagationSystem::run(world);
        VisibilityPropagationSystem::run(world);
    }
}
```

### Hot-Swap Tests

```rust
#[test]
fn test_hierarchy_hot_reload() {
    let mut world = setup_hierarchy();

    // Snapshot state
    let snapshot = world.snapshot_components::<(Parent, Children, LocalTransform, GlobalTransform)>();

    // Simulate module hot-reload
    world.clear_transient_state();
    world.restore_from(&snapshot).unwrap();

    // Trigger recomputation
    for (_, global) in world.query::<&mut GlobalTransform>() {
        global.on_reload();
    }

    TransformPropagationSystem::run(&mut world);

    // Verify hierarchy is intact
    assert!(hierarchy_matches_snapshot(&world, &snapshot));
}

#[test]
fn test_hierarchy_rollback() {
    let mut world = setup_hierarchy();
    let snapshot = RollbackSnapshot::create(&world);

    // Make invalid change that fails
    let result = world.apply_patch(invalid_hierarchy_patch);
    assert!(result.is_err());

    // Rollback
    snapshot.rollback(&mut world).unwrap();

    // Verify original state
    assert!(hierarchy_valid(&world));
}
```

## Capability & Security

Hierarchy operations require appropriate capabilities per the seL4-inspired security model.

```rust
pub fn set_parent(&mut self, child: Entity, parent: Entity) -> Result<(), HierarchyError> {
    // Check capability
    let cap = self.world.get_capability();
    if !cap.can_modify_entity(child) {
        return Err(HierarchyError::PermissionDenied { entity: child });
    }
    if !cap.can_read_entity(parent) {
        return Err(HierarchyError::PermissionDenied { entity: parent });
    }

    // Namespace check - can only parent within same namespace or with cross-namespace capability
    let child_ns = self.world.get_namespace(child);
    let parent_ns = self.world.get_namespace(parent);
    if child_ns != parent_ns && !cap.has_cross_namespace_access() {
        return Err(HierarchyError::CrossNamespaceViolation { child_ns, parent_ns });
    }

    // Proceed with parent operation...
}
```

## Fault Tolerance

Hierarchy operations must be fault-tolerant per the kernel supervision model.

```rust
impl TransformPropagationSystem {
    pub fn run(world: &mut World) {
        for (entity, (local, global, parent)) in
            world.query::<(&LocalTransform, &mut GlobalTransform, Option<&Parent>)>()
        {
            let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
                // Compute transform
                if let Some(parent) = parent {
                    let parent_global = world.get::<GlobalTransform>(parent.entity)
                        .unwrap_or(&GlobalTransform::default());
                    *global = parent_global.multiply(local);
                } else {
                    *global = GlobalTransform::from_local(local);
                }
            }));

            if result.is_err() {
                // Log error, use identity transform as fallback
                log::error!("Transform computation failed for {:?}, using identity", entity);
                *global = GlobalTransform::default();
            }
        }
    }
}
```

## Acceptance Criteria

### Functional
- [ ] Entities can be parented via `set_parent(child, parent)`
- [ ] LocalTransform composes with parent's GlobalTransform
- [ ] GlobalTransform is automatically computed each frame
- [ ] Visibility propagates (invisible parent = invisible children)
- [ ] Cycle detection prevents invalid hierarchies
- [ ] 100+ depth hierarchies work correctly
- [ ] `despawn_recursive` removes all descendants
- [ ] Editor hierarchy panel uses new system
- [ ] IR patches support hierarchy operations
- [ ] Performance: <1ms for 10,000 entities

### Hot-Swap Compliance
- [ ] All components implement `Serialize`/`Deserialize`
- [ ] All components implement `HotReloadable`
- [ ] Hierarchy survives module hot-reload
- [ ] GlobalTransform recomputes after reload
- [ ] Changes apply at frame boundary
- [ ] Failed updates rollback cleanly
- [ ] Capability checks enforced on hierarchy operations
- [ ] Cross-namespace parenting requires explicit capability

## Dependencies

- None (foundational phase)

## Dependents

- Phase 2: Camera System (cameras parented to entities)
- Phase 5: Lighting (lights parented to entities)
- Phase 10: Picking (hierarchy-aware picking)
- Phase 14: Spatial Queries (hierarchy bounds)
- Phase 16: Scene Streaming (sub-graph loading)

---

**Estimated Complexity**: Medium
**Primary Crates**: void_ecs, void_math
**Reviewer Notes**: Focus on cycle detection correctness and transform composition accuracy
