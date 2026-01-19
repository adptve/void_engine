//! Query - Efficient access to entities with specific components
//!
//! Queries allow iterating over entities that have specific component sets.

use crate::archetype::{Archetype, ArchetypeId, Archetypes};
use crate::component::{ComponentId, ComponentRegistry};
use crate::entity::Entity;
use alloc::vec::Vec;
use core::marker::PhantomData;
use core::ptr::NonNull;
use void_structures::BitSet;

/// Access mode for a component in a query
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Access {
    /// Read-only access
    Read,
    /// Read-write access
    Write,
    /// Optional read access
    OptionalRead,
    /// Optional write access
    OptionalWrite,
    /// Component must not be present
    Without,
}

/// Component requirement for a query
#[derive(Clone, Debug)]
pub struct ComponentAccess {
    /// Component ID
    pub id: ComponentId,
    /// Access mode
    pub access: Access,
}

/// Describes a query's requirements
#[derive(Clone, Debug, Default)]
pub struct QueryDescriptor {
    /// Required/optional component accesses
    components: Vec<ComponentAccess>,
    /// Component IDs that must be present (Read/Write)
    required_mask: Option<BitSet>,
    /// Component IDs that must not be present
    excluded_mask: Option<BitSet>,
}

impl QueryDescriptor {
    /// Create a new query descriptor
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a read component requirement
    pub fn read(mut self, id: ComponentId) -> Self {
        self.components.push(ComponentAccess { id, access: Access::Read });
        self
    }

    /// Add a write component requirement
    pub fn write(mut self, id: ComponentId) -> Self {
        self.components.push(ComponentAccess { id, access: Access::Write });
        self
    }

    /// Add an optional read component
    pub fn optional_read(mut self, id: ComponentId) -> Self {
        self.components.push(ComponentAccess { id, access: Access::OptionalRead });
        self
    }

    /// Add an optional write component
    pub fn optional_write(mut self, id: ComponentId) -> Self {
        self.components.push(ComponentAccess { id, access: Access::OptionalWrite });
        self
    }

    /// Add an excluded component
    pub fn without(mut self, id: ComponentId) -> Self {
        self.components.push(ComponentAccess { id, access: Access::Without });
        self
    }

    /// Build the component masks
    pub fn build(mut self) -> Self {
        let max_id = self.components.iter()
            .map(|c| c.id.0 as usize)
            .max()
            .unwrap_or(0);

        let mut required = BitSet::new(max_id + 1);
        let mut excluded = BitSet::new(max_id + 1);

        for access in &self.components {
            match access.access {
                Access::Read | Access::Write => {
                    required.set(access.id.0 as usize);
                }
                Access::Without => {
                    excluded.set(access.id.0 as usize);
                }
                _ => {}
            }
        }

        self.required_mask = Some(required);
        self.excluded_mask = Some(excluded);
        self
    }

    /// Check if an archetype matches this query
    pub fn matches_archetype(&self, archetype: &Archetype) -> bool {
        let arch_mask = archetype.component_mask();

        // Check required components
        if let Some(required) = &self.required_mask {
            for bit in required.iter_ones() {
                if bit >= arch_mask.len() || !arch_mask.get(bit) {
                    return false;
                }
            }
        }

        // Check excluded components
        if let Some(excluded) = &self.excluded_mask {
            for bit in excluded.iter_ones() {
                if bit < arch_mask.len() && arch_mask.get(bit) {
                    return false;
                }
            }
        }

        true
    }

    /// Get component accesses
    pub fn accesses(&self) -> &[ComponentAccess] {
        &self.components
    }
}

/// State for a query including matched archetypes
pub struct QueryState {
    /// Query descriptor
    descriptor: QueryDescriptor,
    /// Matched archetype IDs
    matched_archetypes: Vec<ArchetypeId>,
    /// Last archetype count when matched
    last_archetype_count: usize,
}

impl QueryState {
    /// Create a new query state
    pub fn new(descriptor: QueryDescriptor) -> Self {
        Self {
            descriptor,
            matched_archetypes: Vec::new(),
            last_archetype_count: 0,
        }
    }

    /// Update matched archetypes if needed
    pub fn update(&mut self, archetypes: &Archetypes) {
        if archetypes.len() == self.last_archetype_count {
            return;
        }

        // Check new archetypes
        for arch in archetypes.iter().skip(self.last_archetype_count) {
            if self.descriptor.matches_archetype(arch) {
                self.matched_archetypes.push(arch.id());
            }
        }

        self.last_archetype_count = archetypes.len();
    }

    /// Get matched archetype IDs
    #[inline]
    pub fn matched_archetypes(&self) -> &[ArchetypeId] {
        &self.matched_archetypes
    }

    /// Get the query descriptor
    #[inline]
    pub fn descriptor(&self) -> &QueryDescriptor {
        &self.descriptor
    }
}

/// Iterator over query results for a single archetype
pub struct ArchetypeQueryIter<'a> {
    /// The archetype being iterated
    archetype: &'a Archetype,
    /// Current row index
    current: usize,
    /// Total rows
    len: usize,
}

impl<'a> ArchetypeQueryIter<'a> {
    /// Create a new archetype query iterator
    pub fn new(archetype: &'a Archetype) -> Self {
        Self {
            archetype,
            current: 0,
            len: archetype.len(),
        }
    }

    /// Get component data at the current position
    ///
    /// # Safety
    /// Component ID must be valid for this archetype
    #[inline]
    pub unsafe fn get_component<T: Send + Sync + 'static>(&self, component_id: ComponentId, index: usize) -> &T {
        let storage = self.archetype.storage(component_id).unwrap();
        storage.get::<T>(index)
    }

    /// Get mutable component data at the current position
    ///
    /// # Safety
    /// Component ID must be valid for this archetype, must have exclusive access
    #[inline]
    pub unsafe fn get_component_mut<T: Send + Sync + 'static>(
        &self,
        component_id: ComponentId,
        index: usize,
    ) -> &mut T {
        let storage = self.archetype.storage(component_id).unwrap();
        let ptr = storage.get_raw(index).unwrap().as_ptr() as *mut T;
        &mut *ptr
    }

    /// Get the entity at an index
    #[inline]
    pub fn entity(&self, index: usize) -> Entity {
        self.archetype.entities()[index]
    }

    /// Get remaining count
    #[inline]
    pub fn remaining(&self) -> usize {
        self.len - self.current
    }
}

impl<'a> Iterator for ArchetypeQueryIter<'a> {
    type Item = (usize, Entity);

    fn next(&mut self) -> Option<Self::Item> {
        if self.current >= self.len {
            return None;
        }

        let index = self.current;
        let entity = self.archetype.entities()[index];
        self.current += 1;

        Some((index, entity))
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.len - self.current;
        (remaining, Some(remaining))
    }
}

impl<'a> ExactSizeIterator for ArchetypeQueryIter<'a> {}

/// Full query iterator across all matching archetypes
pub struct QueryIter<'a> {
    /// Query state
    archetypes: &'a Archetypes,
    /// Matched archetype IDs
    matched: &'a [ArchetypeId],
    /// Current archetype index
    archetype_index: usize,
    /// Current row in current archetype
    row: usize,
}

impl<'a> QueryIter<'a> {
    /// Create a new query iterator
    pub fn new(archetypes: &'a Archetypes, state: &'a QueryState) -> Self {
        Self {
            archetypes,
            matched: &state.matched_archetypes,
            archetype_index: 0,
            row: 0,
        }
    }

    /// Get the current archetype (if any)
    fn current_archetype(&self) -> Option<&'a Archetype> {
        self.matched.get(self.archetype_index)
            .and_then(|&id| self.archetypes.get(id))
    }
}

impl<'a> Iterator for QueryIter<'a> {
    type Item = (ArchetypeId, usize, Entity);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let archetype = self.current_archetype()?;

            if self.row < archetype.len() {
                let entity = archetype.entities()[self.row];
                let arch_id = archetype.id();
                let row = self.row;
                self.row += 1;
                return Some((arch_id, row, entity));
            }

            // Move to next archetype
            self.archetype_index += 1;
            self.row = 0;

            if self.archetype_index >= self.matched.len() {
                return None;
            }
        }
    }
}

/// Builder for typed queries
pub struct Query<'w, T> {
    archetypes: &'w Archetypes,
    state: QueryState,
    _marker: PhantomData<T>,
}

impl<'w, T> Query<'w, T> {
    /// Create a new query
    pub fn new(archetypes: &'w Archetypes, descriptor: QueryDescriptor) -> Self {
        let mut state = QueryState::new(descriptor);
        state.update(archetypes);

        Self {
            archetypes,
            state,
            _marker: PhantomData,
        }
    }

    /// Iterate over matched entities
    pub fn iter(&self) -> QueryIter<'_> {
        QueryIter::new(self.archetypes, &self.state)
    }

    /// Get number of matched entities
    pub fn count(&self) -> usize {
        self.state.matched_archetypes.iter()
            .filter_map(|&id| self.archetypes.get(id))
            .map(|arch| arch.len())
            .sum()
    }

    /// Check if query has any matches
    pub fn is_empty(&self) -> bool {
        self.state.matched_archetypes.iter()
            .filter_map(|&id| self.archetypes.get(id))
            .all(|arch| arch.is_empty())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::component::ComponentRegistry;

    #[derive(Clone, Copy)]
    struct Position { x: f32, y: f32 }

    #[derive(Clone, Copy)]
    struct Velocity { x: f32, y: f32 }

    #[derive(Clone, Copy)]
    struct Health(f32);

    #[test]
    fn test_query_descriptor() {
        let mut registry = ComponentRegistry::new();
        let pos_id = registry.register::<Position>();
        let vel_id = registry.register::<Velocity>();
        let health_id = registry.register::<Health>();

        let query = QueryDescriptor::new()
            .read(pos_id)
            .write(vel_id)
            .without(health_id)
            .build();

        // Create test archetype with Position + Velocity (should match)
        let pos_info = registry.get_info(pos_id).unwrap().clone();
        let vel_info = registry.get_info(vel_id).unwrap().clone();
        let health_info = registry.get_info(health_id).unwrap().clone();

        let matching_arch = crate::archetype::Archetype::new(
            ArchetypeId::new(0),
            vec![pos_info.clone(), vel_info.clone()],
        );

        assert!(query.matches_archetype(&matching_arch));

        // Create archetype with Health (should not match due to exclusion)
        let non_matching_arch = crate::archetype::Archetype::new(
            ArchetypeId::new(1),
            vec![pos_info, vel_info, health_info],
        );

        assert!(!query.matches_archetype(&non_matching_arch));
    }
}
