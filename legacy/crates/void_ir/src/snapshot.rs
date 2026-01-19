//! State snapshots for rollback support
//!
//! This module provides efficient state snapshots that enable atomic
//! transaction rollback. Snapshots use copy-on-write semantics where
//! possible to minimize memory overhead.

use crate::patch::{EntityRef, Patch, PatchKind};
use crate::value::Value;
use std::collections::HashMap;

/// A snapshot of world state for rollback
#[derive(Debug, Clone)]
pub struct StateSnapshot {
    /// Snapshot ID
    pub id: SnapshotId,
    /// Frame number when snapshot was taken
    pub frame: u64,
    /// Entity existence state
    pub entities: EntitySnapshot,
    /// Component state
    pub components: ComponentSnapshot,
    /// Layer state
    pub layers: LayerSnapshot,
    /// Asset state
    pub assets: AssetSnapshot,
}

impl StateSnapshot {
    /// Create a new empty snapshot
    pub fn new(frame: u64) -> Self {
        Self {
            id: SnapshotId::new(),
            frame,
            entities: EntitySnapshot::new(),
            components: ComponentSnapshot::new(),
            layers: LayerSnapshot::new(),
            assets: AssetSnapshot::new(),
        }
    }

    /// Compute the difference between this and another snapshot
    pub fn diff(&self, other: &StateSnapshot) -> Vec<Patch> {
        let mut patches = Vec::new();

        // Entity diffs
        patches.extend(self.entities.diff(&other.entities));

        // Component diffs
        patches.extend(self.components.diff(&other.components));

        // Layer diffs
        patches.extend(self.layers.diff(&other.layers));

        // Asset diffs
        patches.extend(self.assets.diff(&other.assets));

        patches
    }

    /// Get the memory size of this snapshot
    pub fn memory_size(&self) -> usize {
        self.entities.memory_size()
            + self.components.memory_size()
            + self.layers.memory_size()
            + self.assets.memory_size()
    }
}

/// Unique identifier for a snapshot
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SnapshotId(u64);

impl SnapshotId {
    /// Create a new unique snapshot ID
    pub fn new() -> Self {
        use std::sync::atomic::{AtomicU64, Ordering};
        static COUNTER: AtomicU64 = AtomicU64::new(1);
        Self(COUNTER.fetch_add(1, Ordering::Relaxed))
    }

    /// Get the raw ID
    pub fn raw(&self) -> u64 {
        self.0
    }
}

impl Default for SnapshotId {
    fn default() -> Self {
        Self::new()
    }
}

/// Snapshot of entity state
#[derive(Debug, Clone)]
pub struct EntitySnapshot {
    /// Set of entities that exist
    entities: HashMap<EntityRef, EntityState>,
}

#[derive(Debug, Clone)]
struct EntityState {
    /// Whether entity is enabled
    enabled: bool,
    /// Parent entity (if any)
    parent: Option<EntityRef>,
    /// Tags on this entity
    tags: Vec<String>,
}

impl EntitySnapshot {
    /// Create a new empty entity snapshot
    pub fn new() -> Self {
        Self {
            entities: HashMap::new(),
        }
    }

    /// Record that an entity exists
    pub fn insert(&mut self, entity: EntityRef, enabled: bool, parent: Option<EntityRef>, tags: Vec<String>) {
        self.entities.insert(entity, EntityState { enabled, parent, tags });
    }

    /// Record that an entity was destroyed
    pub fn remove(&mut self, entity: &EntityRef) {
        self.entities.remove(entity);
    }

    /// Check if entity exists
    pub fn contains(&self, entity: &EntityRef) -> bool {
        self.entities.contains_key(entity)
    }

    /// Get entity state
    pub fn get(&self, entity: &EntityRef) -> Option<&EntityState> {
        self.entities.get(entity)
    }

    /// Compute patches to go from other to self
    pub fn diff(&self, other: &EntitySnapshot) -> Vec<Patch> {
        let mut patches = Vec::new();

        // Find entities that exist in self but not other (created)
        for (entity, state) in &self.entities {
            if !other.contains(entity) {
                use crate::patch::{EntityOp, EntityPatch};
                patches.push(Patch::new(
                    entity.namespace,
                    PatchKind::Entity(EntityPatch {
                        entity: *entity,
                        op: EntityOp::Create {
                            archetype: None,
                            components: HashMap::new(),
                        },
                    }),
                ));
            }
        }

        // Find entities that exist in other but not self (destroyed)
        for entity in other.entities.keys() {
            if !self.contains(entity) {
                use crate::patch::{EntityOp, EntityPatch};
                patches.push(Patch::new(
                    entity.namespace,
                    PatchKind::Entity(EntityPatch {
                        entity: *entity,
                        op: EntityOp::Destroy,
                    }),
                ));
            }
        }

        patches
    }

    /// Get memory size
    pub fn memory_size(&self) -> usize {
        self.entities.len() * (std::mem::size_of::<EntityRef>() + std::mem::size_of::<EntityState>())
    }
}

impl Default for EntitySnapshot {
    fn default() -> Self {
        Self::new()
    }
}

/// Snapshot of component state
#[derive(Debug, Clone)]
pub struct ComponentSnapshot {
    /// Components per entity
    components: HashMap<EntityRef, HashMap<String, Value>>,
}

impl ComponentSnapshot {
    /// Create a new empty component snapshot
    pub fn new() -> Self {
        Self {
            components: HashMap::new(),
        }
    }

    /// Set a component value
    pub fn set(&mut self, entity: EntityRef, component: String, data: Value) {
        self.components
            .entry(entity)
            .or_insert_with(HashMap::new)
            .insert(component, data);
    }

    /// Remove a component
    pub fn remove(&mut self, entity: &EntityRef, component: &str) {
        if let Some(comps) = self.components.get_mut(entity) {
            comps.remove(component);
        }
    }

    /// Remove all components for an entity
    pub fn remove_entity(&mut self, entity: &EntityRef) {
        self.components.remove(entity);
    }

    /// Get a component value
    pub fn get(&self, entity: &EntityRef, component: &str) -> Option<&Value> {
        self.components.get(entity)?.get(component)
    }

    /// Check if entity has component
    pub fn has_component(&self, entity: &EntityRef, component: &str) -> bool {
        self.components
            .get(entity)
            .map_or(false, |comps| comps.contains_key(component))
    }

    /// Compute patches to go from other to self
    pub fn diff(&self, other: &ComponentSnapshot) -> Vec<Patch> {
        let mut patches = Vec::new();

        use crate::patch::{ComponentOp, ComponentPatch};

        // Find components that exist in self but not other (added/updated)
        for (entity, components) in &self.components {
            for (comp_name, comp_data) in components {
                let other_data = other.get(entity, comp_name);

                if other_data != Some(comp_data) {
                    patches.push(Patch::new(
                        entity.namespace,
                        PatchKind::Component(ComponentPatch {
                            entity: *entity,
                            component: comp_name.clone(),
                            op: ComponentOp::Set {
                                data: comp_data.clone(),
                            },
                        }),
                    ));
                }
            }
        }

        // Find components that exist in other but not self (removed)
        for (entity, components) in &other.components {
            for comp_name in components.keys() {
                if !self.has_component(entity, comp_name) {
                    patches.push(Patch::new(
                        entity.namespace,
                        PatchKind::Component(ComponentPatch {
                            entity: *entity,
                            component: comp_name.clone(),
                            op: ComponentOp::Remove,
                        }),
                    ));
                }
            }
        }

        patches
    }

    /// Get memory size
    pub fn memory_size(&self) -> usize {
        let mut size = 0;
        for (_, components) in &self.components {
            size += std::mem::size_of::<EntityRef>();
            size += components.len() * (std::mem::size_of::<String>() + std::mem::size_of::<Value>());
        }
        size
    }
}

impl Default for ComponentSnapshot {
    fn default() -> Self {
        Self::new()
    }
}

/// Snapshot of layer state
#[derive(Debug, Clone)]
pub struct LayerSnapshot {
    /// Layers that exist
    layers: HashMap<String, LayerState>,
}

#[derive(Debug, Clone)]
struct LayerState {
    /// Layer type
    layer_type: crate::patch::LayerType,
    /// Priority
    priority: i32,
    /// Visible
    visible: bool,
    /// Blend mode
    blend_mode: crate::patch::BlendMode,
}

impl LayerSnapshot {
    /// Create a new empty layer snapshot
    pub fn new() -> Self {
        Self {
            layers: HashMap::new(),
        }
    }

    /// Add a layer
    pub fn insert(
        &mut self,
        layer_id: String,
        layer_type: crate::patch::LayerType,
        priority: i32,
        visible: bool,
        blend_mode: crate::patch::BlendMode,
    ) {
        self.layers.insert(
            layer_id,
            LayerState {
                layer_type,
                priority,
                visible,
                blend_mode,
            },
        );
    }

    /// Remove a layer
    pub fn remove(&mut self, layer_id: &str) {
        self.layers.remove(layer_id);
    }

    /// Check if layer exists
    pub fn contains(&self, layer_id: &str) -> bool {
        self.layers.contains_key(layer_id)
    }

    /// Compute patches to go from other to self
    pub fn diff(&self, other: &LayerSnapshot) -> Vec<Patch> {
        let mut patches = Vec::new();

        use crate::namespace::NamespaceId;
        use crate::patch::{LayerOp, LayerPatch};

        // Find layers that exist in self but not other (created)
        for (layer_id, state) in &self.layers {
            if !other.contains(layer_id) {
                patches.push(Patch::new(
                    NamespaceId::KERNEL, // Layers are kernel-managed
                    PatchKind::Layer(LayerPatch {
                        layer_id: layer_id.clone(),
                        op: LayerOp::Create {
                            layer_type: state.layer_type,
                            priority: state.priority,
                        },
                    }),
                ));
            }
        }

        // Find layers that exist in other but not self (destroyed)
        for layer_id in other.layers.keys() {
            if !self.contains(layer_id) {
                patches.push(Patch::new(
                    NamespaceId::KERNEL,
                    PatchKind::Layer(LayerPatch {
                        layer_id: layer_id.clone(),
                        op: LayerOp::Destroy,
                    }),
                ));
            }
        }

        patches
    }

    /// Get memory size
    pub fn memory_size(&self) -> usize {
        self.layers.len() * (std::mem::size_of::<String>() + std::mem::size_of::<LayerState>())
    }
}

impl Default for LayerSnapshot {
    fn default() -> Self {
        Self::new()
    }
}

/// Snapshot of asset state
#[derive(Debug, Clone)]
pub struct AssetSnapshot {
    /// Assets that are loaded
    assets: HashMap<String, AssetState>,
}

#[derive(Debug, Clone)]
struct AssetState {
    /// Asset path
    path: String,
    /// Asset type
    asset_type: Option<String>,
}

impl AssetSnapshot {
    /// Create a new empty asset snapshot
    pub fn new() -> Self {
        Self {
            assets: HashMap::new(),
        }
    }

    /// Add an asset
    pub fn insert(&mut self, asset_id: String, path: String, asset_type: Option<String>) {
        self.assets.insert(asset_id, AssetState { path, asset_type });
    }

    /// Remove an asset
    pub fn remove(&mut self, asset_id: &str) {
        self.assets.remove(asset_id);
    }

    /// Check if asset exists
    pub fn contains(&self, asset_id: &str) -> bool {
        self.assets.contains_key(asset_id)
    }

    /// Compute patches to go from other to self
    pub fn diff(&self, other: &AssetSnapshot) -> Vec<Patch> {
        let mut patches = Vec::new();

        use crate::namespace::NamespaceId;
        use crate::patch::{AssetOp, AssetPatch};

        // Find assets that exist in self but not other (loaded)
        for (asset_id, state) in &self.assets {
            if !other.contains(asset_id) {
                patches.push(Patch::new(
                    NamespaceId::KERNEL,
                    PatchKind::Asset(AssetPatch {
                        asset_id: asset_id.clone(),
                        op: AssetOp::Load {
                            path: state.path.clone(),
                            asset_type: state.asset_type.clone(),
                        },
                    }),
                ));
            }
        }

        // Find assets that exist in other but not self (unloaded)
        for asset_id in other.assets.keys() {
            if !self.contains(asset_id) {
                patches.push(Patch::new(
                    NamespaceId::KERNEL,
                    PatchKind::Asset(AssetPatch {
                        asset_id: asset_id.clone(),
                        op: AssetOp::Unload,
                    }),
                ));
            }
        }

        patches
    }

    /// Get memory size
    pub fn memory_size(&self) -> usize {
        self.assets.len() * (std::mem::size_of::<String>() + std::mem::size_of::<AssetState>())
    }
}

impl Default for AssetSnapshot {
    fn default() -> Self {
        Self::new()
    }
}

/// Manages a collection of snapshots with garbage collection
pub struct SnapshotManager {
    /// Stored snapshots
    snapshots: HashMap<SnapshotId, StateSnapshot>,
    /// Maximum number of snapshots to keep
    max_snapshots: usize,
    /// Total memory used by snapshots
    total_memory: usize,
    /// Maximum memory to use
    max_memory: usize,
}

impl SnapshotManager {
    /// Create a new snapshot manager
    pub fn new(max_snapshots: usize, max_memory: usize) -> Self {
        Self {
            snapshots: HashMap::new(),
            max_snapshots,
            total_memory: 0,
            max_memory,
        }
    }

    /// Store a snapshot
    pub fn store(&mut self, snapshot: StateSnapshot) -> SnapshotId {
        let id = snapshot.id;
        let size = snapshot.memory_size();

        self.snapshots.insert(id, snapshot);
        self.total_memory += size;

        // Garbage collect if needed
        self.gc_if_needed();

        id
    }

    /// Get a snapshot by ID
    pub fn get(&self, id: SnapshotId) -> Option<&StateSnapshot> {
        self.snapshots.get(&id)
    }

    /// Remove a snapshot
    pub fn remove(&mut self, id: SnapshotId) -> Option<StateSnapshot> {
        if let Some(snapshot) = self.snapshots.remove(&id) {
            self.total_memory -= snapshot.memory_size();
            Some(snapshot)
        } else {
            None
        }
    }

    /// Get the number of stored snapshots
    pub fn len(&self) -> usize {
        self.snapshots.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.snapshots.is_empty()
    }

    /// Get total memory used
    pub fn memory_used(&self) -> usize {
        self.total_memory
    }

    /// Garbage collect old snapshots if needed
    fn gc_if_needed(&mut self) {
        // Remove old snapshots if we exceed limits
        while self.snapshots.len() > self.max_snapshots || self.total_memory > self.max_memory {
            // Find oldest snapshot (lowest frame number)
            if let Some((&oldest_id, _)) = self
                .snapshots
                .iter()
                .min_by_key(|(_, snapshot)| snapshot.frame)
            {
                self.remove(oldest_id);
            } else {
                break;
            }
        }
    }

    /// Clear all snapshots
    pub fn clear(&mut self) {
        self.snapshots.clear();
        self.total_memory = 0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::namespace::NamespaceId;

    #[test]
    fn test_entity_snapshot() {
        let mut snapshot = EntitySnapshot::new();
        let entity = EntityRef::new(NamespaceId::new(), 1);

        snapshot.insert(entity, true, None, vec![]);
        assert!(snapshot.contains(&entity));

        snapshot.remove(&entity);
        assert!(!snapshot.contains(&entity));
    }

    #[test]
    fn test_component_snapshot() {
        let mut snapshot = ComponentSnapshot::new();
        let entity = EntityRef::new(NamespaceId::new(), 1);

        snapshot.set(entity, "Transform".to_string(), Value::from([0.0, 0.0, 0.0]));
        assert!(snapshot.has_component(&entity, "Transform"));

        snapshot.remove(&entity, "Transform");
        assert!(!snapshot.has_component(&entity, "Transform"));
    }

    #[test]
    fn test_snapshot_diff() {
        let ns = NamespaceId::new();
        let entity1 = EntityRef::new(ns, 1);
        let entity2 = EntityRef::new(ns, 2);

        let mut snapshot1 = StateSnapshot::new(1);
        snapshot1.entities.insert(entity1, true, None, vec![]);

        let mut snapshot2 = StateSnapshot::new(2);
        snapshot2.entities.insert(entity1, true, None, vec![]);
        snapshot2.entities.insert(entity2, true, None, vec![]);

        let diff = snapshot2.diff(&snapshot1);
        assert_eq!(diff.len(), 1); // One entity created
    }

    #[test]
    fn test_snapshot_manager() {
        let mut manager = SnapshotManager::new(2, 1024 * 1024);

        let snapshot1 = StateSnapshot::new(1);
        let id1 = manager.store(snapshot1);

        let snapshot2 = StateSnapshot::new(2);
        let id2 = manager.store(snapshot2);

        assert_eq!(manager.len(), 2);

        let snapshot3 = StateSnapshot::new(3);
        manager.store(snapshot3);

        // Should have GC'd the oldest snapshot
        assert_eq!(manager.len(), 2);
        assert!(manager.get(id1).is_none()); // Oldest removed
        assert!(manager.get(id2).is_some());
    }
}
