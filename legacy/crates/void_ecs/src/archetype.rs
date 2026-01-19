//! Archetype - Groups of entities with the same component types
//!
//! Entities with identical component sets are stored together for
//! cache-efficient iteration.

use crate::component::{ComponentId, ComponentInfo, ComponentStorage};
use crate::entity::Entity;
use alloc::vec::Vec;
use alloc::collections::BTreeMap;
use core::ptr::NonNull;
use void_structures::BitSet;

/// Unique identifier for an archetype
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct ArchetypeId(pub u32);

impl ArchetypeId {
    /// Invalid archetype ID
    pub const INVALID: Self = Self(u32::MAX);

    /// Create a new archetype ID
    #[inline]
    pub const fn new(id: u32) -> Self {
        Self(id)
    }

    /// Get the raw ID
    #[inline]
    pub const fn id(&self) -> u32 {
        self.0
    }

    /// Check if valid
    #[inline]
    pub const fn is_valid(&self) -> bool {
        self.0 != u32::MAX
    }
}

/// Archetype edge for graph navigation
#[derive(Clone, Copy, Debug)]
pub struct ArchetypeEdge {
    /// Archetype reached by adding a component
    pub add: ArchetypeId,
    /// Archetype reached by removing a component
    pub remove: ArchetypeId,
}

impl Default for ArchetypeEdge {
    fn default() -> Self {
        Self {
            add: ArchetypeId::INVALID,
            remove: ArchetypeId::INVALID,
        }
    }
}

/// An archetype stores entities with the same component set
pub struct Archetype {
    /// Unique identifier
    id: ArchetypeId,
    /// Component IDs in this archetype (sorted)
    components: Vec<ComponentId>,
    /// Component mask for fast matching
    component_mask: BitSet,
    /// Storage for each component type
    storages: Vec<ComponentStorage>,
    /// ComponentId -> storage index mapping
    component_indices: BTreeMap<ComponentId, usize>,
    /// Entities in this archetype
    entities: Vec<Entity>,
    /// Graph edges for archetype transitions
    edges: BTreeMap<ComponentId, ArchetypeEdge>,
}

impl Archetype {
    /// Create a new archetype
    pub fn new(id: ArchetypeId, component_infos: Vec<ComponentInfo>) -> Self {
        let mut components: Vec<ComponentId> = component_infos.iter().map(|c| c.id).collect();
        components.sort();

        let max_component_id = components.iter().map(|c| c.0).max().unwrap_or(0) as usize;
        let mut component_mask = BitSet::new(max_component_id + 1);
        for &comp_id in &components {
            component_mask.set(comp_id.0 as usize);
        }

        let mut component_indices = BTreeMap::new();
        let mut storages = Vec::with_capacity(component_infos.len());

        for (idx, info) in component_infos.into_iter().enumerate() {
            component_indices.insert(info.id, idx);
            storages.push(ComponentStorage::new(info));
        }

        Self {
            id,
            components,
            component_mask,
            storages,
            component_indices,
            entities: Vec::new(),
            edges: BTreeMap::new(),
        }
    }

    /// Get archetype ID
    #[inline]
    pub fn id(&self) -> ArchetypeId {
        self.id
    }

    /// Get component IDs
    #[inline]
    pub fn components(&self) -> &[ComponentId] {
        &self.components
    }

    /// Get component mask
    #[inline]
    pub fn component_mask(&self) -> &BitSet {
        &self.component_mask
    }

    /// Check if archetype has a component
    #[inline]
    pub fn has_component(&self, component_id: ComponentId) -> bool {
        self.component_indices.contains_key(&component_id)
    }

    /// Get number of entities
    #[inline]
    pub fn len(&self) -> usize {
        self.entities.len()
    }

    /// Check if empty
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.entities.is_empty()
    }

    /// Get entities
    #[inline]
    pub fn entities(&self) -> &[Entity] {
        &self.entities
    }

    /// Get storage for a component
    pub fn storage(&self, component_id: ComponentId) -> Option<&ComponentStorage> {
        self.component_indices.get(&component_id).map(|&idx| &self.storages[idx])
    }

    /// Get mutable storage for a component
    pub fn storage_mut(&mut self, component_id: ComponentId) -> Option<&mut ComponentStorage> {
        if let Some(&idx) = self.component_indices.get(&component_id) {
            Some(&mut self.storages[idx])
        } else {
            None
        }
    }

    /// Get all storages
    #[inline]
    pub fn storages(&self) -> &[ComponentStorage] {
        &self.storages
    }

    /// Get all storages mutably
    #[inline]
    pub fn storages_mut(&mut self) -> &mut [ComponentStorage] {
        &mut self.storages
    }

    /// Reserve capacity for entities
    pub fn reserve(&mut self, additional: usize) {
        self.entities.reserve(additional);
        for storage in &mut self.storages {
            storage.reserve(additional);
        }
    }

    /// Add an entity with its components
    ///
    /// # Safety
    /// The component data must match the archetype's component types in order
    pub unsafe fn add_entity_raw(&mut self, entity: Entity, component_data: &[NonNull<u8>]) -> usize {
        debug_assert_eq!(component_data.len(), self.storages.len());

        let row = self.entities.len();
        self.entities.push(entity);

        for (storage, &data) in self.storages.iter_mut().zip(component_data.iter()) {
            storage.push_raw(data);
        }

        row
    }

    /// Remove an entity by row index (swap-remove)
    ///
    /// Returns the entity that was swapped into this position (if any)
    pub fn remove_entity(&mut self, row: usize) -> Option<Entity> {
        if row >= self.entities.len() {
            return None;
        }

        let last_row = self.entities.len() - 1;
        let swapped_entity = if row != last_row {
            Some(self.entities[last_row])
        } else {
            None
        };

        self.entities.swap_remove(row);
        for storage in &mut self.storages {
            storage.swap_remove(row);
        }

        swapped_entity
    }

    /// Move an entity's data to another archetype
    ///
    /// Returns the row in the new archetype
    pub fn move_entity_to(
        &mut self,
        row: usize,
        target: &mut Archetype,
    ) -> Option<(usize, Option<Entity>)> {
        if row >= self.entities.len() {
            return None;
        }

        let entity = self.entities[row];
        let target_row = target.entities.len();
        target.entities.push(entity);

        // Copy components that exist in both archetypes
        for (comp_id, &storage_idx) in &self.component_indices {
            if let Some(&target_idx) = target.component_indices.get(comp_id) {
                let src_storage = &self.storages[storage_idx];
                let dst_storage = &mut target.storages[target_idx];

                if let Some(ptr) = src_storage.get_raw(row) {
                    unsafe {
                        dst_storage.push_raw(ptr);
                    }
                }
            }
        }

        // Remove from source archetype WITHOUT dropping components
        // (they've been moved to the target, dropping would cause double-free
        // for types with heap allocations like Vec)
        let swapped = self.remove_entity_no_drop(row);

        Some((target_row, swapped))
    }

    /// Remove an entity by row index WITHOUT dropping components (for moves)
    ///
    /// Returns the entity that was swapped into this position (if any)
    fn remove_entity_no_drop(&mut self, row: usize) -> Option<Entity> {
        if row >= self.entities.len() {
            return None;
        }

        let last_row = self.entities.len() - 1;
        let swapped_entity = if row != last_row {
            Some(self.entities[last_row])
        } else {
            None
        };

        self.entities.swap_remove(row);
        for storage in &mut self.storages {
            storage.swap_remove_no_drop(row);
        }

        swapped_entity
    }

    /// Get an edge for a component
    pub fn edge(&self, component_id: ComponentId) -> Option<&ArchetypeEdge> {
        self.edges.get(&component_id)
    }

    /// Set an edge for a component
    pub fn set_edge(&mut self, component_id: ComponentId, edge: ArchetypeEdge) {
        self.edges.insert(component_id, edge);
    }

    /// Get or create an edge for a component
    pub fn edge_mut(&mut self, component_id: ComponentId) -> &mut ArchetypeEdge {
        self.edges.entry(component_id).or_default()
    }

    /// Get a component value at a row
    ///
    /// # Safety
    /// The type T must match the component type stored at the given component ID.
    pub fn get_component<T: 'static>(&self, component_id: ComponentId, row: usize) -> Option<&T> {
        let storage = self.storage(component_id)?;
        let ptr = storage.get_raw(row)?;
        // SAFETY: Caller guarantees T matches the stored type
        Some(unsafe { &*(ptr.as_ptr() as *const T) })
    }

    /// Get a mutable component value at a row
    ///
    /// # Safety
    /// The type T must match the component type stored at the given component ID.
    pub fn get_component_mut<T: 'static>(&mut self, component_id: ComponentId, row: usize) -> Option<&mut T> {
        let storage = self.storage_mut(component_id)?;
        let ptr = storage.get_raw(row)?;
        // SAFETY: Caller guarantees T matches the stored type
        Some(unsafe { &mut *(ptr.as_ptr() as *mut T) })
    }
}

/// Archetype storage and management
pub struct Archetypes {
    /// All archetypes
    archetypes: Vec<Archetype>,
    /// Component signature -> archetype ID mapping
    signature_map: BTreeMap<Vec<ComponentId>, ArchetypeId>,
    /// Empty archetype (for entities with no components)
    empty_archetype: ArchetypeId,
}

impl Archetypes {
    /// Create a new archetype storage
    pub fn new() -> Self {
        let mut archetypes = Self {
            archetypes: Vec::new(),
            signature_map: BTreeMap::new(),
            empty_archetype: ArchetypeId::INVALID,
        };

        // Create the empty archetype
        archetypes.empty_archetype = archetypes.get_or_create(Vec::new());

        archetypes
    }

    /// Get the empty archetype ID
    #[inline]
    pub fn empty(&self) -> ArchetypeId {
        self.empty_archetype
    }

    /// Get an archetype by ID
    #[inline]
    pub fn get(&self, id: ArchetypeId) -> Option<&Archetype> {
        self.archetypes.get(id.0 as usize)
    }

    /// Get an archetype mutably by ID
    #[inline]
    pub fn get_mut(&mut self, id: ArchetypeId) -> Option<&mut Archetype> {
        self.archetypes.get_mut(id.0 as usize)
    }

    /// Get two archetypes mutably
    pub fn get_mut2(
        &mut self,
        id1: ArchetypeId,
        id2: ArchetypeId,
    ) -> Option<(&mut Archetype, &mut Archetype)> {
        if id1 == id2 {
            return None;
        }

        let (a, b) = if id1.0 < id2.0 {
            let (left, right) = self.archetypes.split_at_mut(id2.0 as usize);
            (&mut left[id1.0 as usize], &mut right[0])
        } else {
            let (left, right) = self.archetypes.split_at_mut(id1.0 as usize);
            (&mut right[0], &mut left[id2.0 as usize])
        };

        Some((a, b))
    }

    /// Get or create an archetype with the given components
    pub fn get_or_create(&mut self, component_infos: Vec<ComponentInfo>) -> ArchetypeId {
        let mut signature: Vec<ComponentId> = component_infos.iter().map(|c| c.id).collect();
        signature.sort();

        if let Some(&id) = self.signature_map.get(&signature) {
            return id;
        }

        let id = ArchetypeId::new(self.archetypes.len() as u32);
        let archetype = Archetype::new(id, component_infos);

        self.signature_map.insert(signature, id);
        self.archetypes.push(archetype);

        id
    }

    /// Get archetype ID for a component signature
    pub fn find(&self, components: &[ComponentId]) -> Option<ArchetypeId> {
        let mut signature = components.to_vec();
        signature.sort();
        self.signature_map.get(&signature).copied()
    }

    /// Get number of archetypes
    #[inline]
    pub fn len(&self) -> usize {
        self.archetypes.len()
    }

    /// Check if empty
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.archetypes.is_empty()
    }

    /// Iterate over all archetypes
    pub fn iter(&self) -> impl Iterator<Item = &Archetype> {
        self.archetypes.iter()
    }

    /// Iterate over all archetypes mutably
    pub fn iter_mut(&mut self) -> impl Iterator<Item = &mut Archetype> {
        self.archetypes.iter_mut()
    }

    /// Get all archetypes as a slice (for iteration)
    pub fn values(&self) -> impl Iterator<Item = &Archetype> {
        self.archetypes.iter()
    }

    /// Get all archetypes mutably (for iteration)
    pub fn values_mut(&mut self) -> impl Iterator<Item = &mut Archetype> {
        self.archetypes.iter_mut()
    }
}

impl Default for Archetypes {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::component::ComponentRegistry;

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct Position {
        x: f32,
        y: f32,
    }

    #[derive(Clone, Copy, Debug, PartialEq)]
    struct Velocity {
        x: f32,
        y: f32,
    }

    #[test]
    fn test_archetype_creation() {
        let mut registry = ComponentRegistry::new();
        let pos_id = registry.register::<Position>();
        let vel_id = registry.register::<Velocity>();

        let pos_info = registry.get_info(pos_id).unwrap().clone();
        let vel_info = registry.get_info(vel_id).unwrap().clone();

        let archetype = Archetype::new(
            ArchetypeId::new(0),
            vec![pos_info, vel_info],
        );

        assert!(archetype.has_component(pos_id));
        assert!(archetype.has_component(vel_id));
        assert!(!archetype.has_component(ComponentId::new(999)));
    }

    #[test]
    fn test_archetypes_get_or_create() {
        let mut registry = ComponentRegistry::new();
        let pos_id = registry.register::<Position>();
        let vel_id = registry.register::<Velocity>();

        let pos_info = registry.get_info(pos_id).unwrap().clone();
        let vel_info = registry.get_info(vel_id).unwrap().clone();

        let mut archetypes = Archetypes::new();

        // Create archetype with Position + Velocity
        let arch1 = archetypes.get_or_create(vec![pos_info.clone(), vel_info.clone()]);

        // Should return the same archetype
        let arch2 = archetypes.get_or_create(vec![pos_info.clone(), vel_info.clone()]);

        assert_eq!(arch1, arch2);

        // Different order should still return the same archetype
        let arch3 = archetypes.get_or_create(vec![vel_info, pos_info]);
        assert_eq!(arch1, arch3);
    }
}
