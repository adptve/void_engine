//! World - Container for all ECS data
//!
//! The World is the central storage for all entities, components, and resources.
//! It provides the main API for creating entities and manipulating components.

use crate::archetype::{ArchetypeId, Archetypes};
use crate::component::{ComponentId, ComponentInfo, ComponentRegistry, ComponentStorage};
use crate::entity::{Entity, EntityAllocator};
use crate::query::{QueryDescriptor, QueryState};
use crate::system::{System, SystemScheduler, SystemStage, SystemWorld};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use core::any::{Any, TypeId};
use core::ptr::NonNull;
use parking_lot::RwLock;

/// Location of an entity in archetype storage
#[derive(Clone, Copy, Debug)]
pub struct EntityLocation {
    /// Archetype containing the entity
    pub archetype_id: ArchetypeId,
    /// Row index within the archetype
    pub row: usize,
}

impl EntityLocation {
    /// Create a new entity location
    pub const fn new(archetype_id: ArchetypeId, row: usize) -> Self {
        Self { archetype_id, row }
    }

    /// Invalid location
    pub const fn invalid() -> Self {
        Self {
            archetype_id: ArchetypeId::INVALID,
            row: usize::MAX,
        }
    }

    /// Check if valid
    pub const fn is_valid(&self) -> bool {
        self.archetype_id.is_valid()
    }
}

/// Resource storage
struct Resources {
    /// Resources by type ID
    data: BTreeMap<TypeId, Box<dyn Any + Send + Sync>>,
}

impl Resources {
    fn new() -> Self {
        Self {
            data: BTreeMap::new(),
        }
    }

    fn insert<R: Any + Send + Sync>(&mut self, resource: R) {
        self.data.insert(TypeId::of::<R>(), Box::new(resource));
    }

    fn remove<R: Any + Send + Sync>(&mut self) -> Option<R> {
        self.data
            .remove(&TypeId::of::<R>())
            .and_then(|boxed| boxed.downcast::<R>().ok())
            .map(|b| *b)
    }

    fn get<R: Any + Send + Sync>(&self) -> Option<&R> {
        self.data
            .get(&TypeId::of::<R>())
            .and_then(|boxed| boxed.downcast_ref::<R>())
    }

    fn get_mut<R: Any + Send + Sync>(&mut self) -> Option<&mut R> {
        self.data
            .get_mut(&TypeId::of::<R>())
            .and_then(|boxed| boxed.downcast_mut::<R>())
    }

    fn contains<R: Any + Send + Sync>(&self) -> bool {
        self.data.contains_key(&TypeId::of::<R>())
    }

    fn get_ptr(&self, type_id: TypeId) -> Option<*const u8> {
        self.data.get(&type_id).map(|b| {
            let ptr: *const dyn Any = b.as_ref();
            ptr as *const u8
        })
    }

    fn get_ptr_mut(&mut self, type_id: TypeId) -> Option<*mut u8> {
        self.data.get_mut(&type_id).map(|b| {
            let ptr: *mut dyn Any = b.as_mut();
            ptr as *mut u8
        })
    }
}

/// The ECS World - container for all game data
pub struct World {
    /// Entity allocator
    entities: EntityAllocator,
    /// Entity locations
    locations: Vec<EntityLocation>,
    /// Component registry
    components: ComponentRegistry,
    /// Archetype storage
    archetypes: Archetypes,
    /// Resource storage
    resources: Resources,
    /// System scheduler
    scheduler: SystemScheduler,
    /// Pending spawn commands
    spawn_queue: Vec<SpawnCommand>,
    /// Pending despawn commands
    despawn_queue: Vec<Entity>,
}

/// Command to spawn an entity with components
struct SpawnCommand {
    /// Entity ID (pre-allocated)
    entity: Entity,
    /// Component data
    components: Vec<(ComponentId, Box<[u8]>)>,
}

impl World {
    /// Create a new empty world
    pub fn new() -> Self {
        Self {
            entities: EntityAllocator::new(),
            locations: Vec::new(),
            components: ComponentRegistry::new(),
            archetypes: Archetypes::new(),
            resources: Resources::new(),
            scheduler: SystemScheduler::new(),
            spawn_queue: Vec::new(),
            despawn_queue: Vec::new(),
        }
    }

    /// Create with initial capacity
    pub fn with_capacity(entity_capacity: usize) -> Self {
        Self {
            entities: EntityAllocator::with_capacity(entity_capacity),
            locations: Vec::with_capacity(entity_capacity),
            components: ComponentRegistry::new(),
            archetypes: Archetypes::new(),
            resources: Resources::new(),
            scheduler: SystemScheduler::new(),
            spawn_queue: Vec::new(),
            despawn_queue: Vec::new(),
        }
    }

    // ========== Component Registration ==========

    /// Register a component type
    pub fn register_component<T: Send + Sync + 'static>(&mut self) -> ComponentId {
        self.components.register::<T>()
    }

    /// Register a cloneable component type
    pub fn register_cloneable_component<T: Send + Sync + Clone + 'static>(&mut self) -> ComponentId {
        self.components.register_cloneable::<T>()
    }

    /// Get component ID by type
    pub fn component_id<T: Send + Sync + 'static>(&self) -> Option<ComponentId> {
        self.components.get_id::<T>()
    }

    /// Get component ID by name
    pub fn component_id_by_name(&self, name: &str) -> Option<ComponentId> {
        self.components.get_id_by_name(name)
    }

    /// Get component info
    pub fn component_info(&self, id: ComponentId) -> Option<&ComponentInfo> {
        self.components.get_info(id)
    }

    // ========== Entity Management ==========

    /// Spawn a new entity (without components initially)
    pub fn spawn(&mut self) -> Entity {
        let entity = self.entities.allocate();

        // Ensure location array is large enough
        if entity.index() as usize >= self.locations.len() {
            self.locations.resize(
                entity.index() as usize + 1,
                EntityLocation::invalid(),
            );
        }

        // Put in empty archetype
        let empty_archetype = self.archetypes.empty();
        if let Some(archetype) = self.archetypes.get_mut(empty_archetype) {
            let row = archetype.len();
            unsafe {
                archetype.add_entity_raw(entity, &[]);
            }
            self.locations[entity.index() as usize] = EntityLocation::new(empty_archetype, row);
        }

        entity
    }

    /// Despawn an entity
    pub fn despawn(&mut self, entity: Entity) -> bool {
        if !self.entities.is_alive(entity) {
            return false;
        }

        let location = self.locations[entity.index() as usize];
        if !location.is_valid() {
            return false;
        }

        // Remove from archetype
        if let Some(archetype) = self.archetypes.get_mut(location.archetype_id) {
            if let Some(swapped_entity) = archetype.remove_entity(location.row) {
                // Update the swapped entity's location
                self.locations[swapped_entity.index() as usize].row = location.row;
            }
        }

        self.locations[entity.index() as usize] = EntityLocation::invalid();
        self.entities.deallocate(entity);

        true
    }

    /// Check if an entity is alive
    pub fn is_alive(&self, entity: Entity) -> bool {
        self.entities.is_alive(entity)
    }

    /// Get the number of alive entities
    pub fn entity_count(&self) -> usize {
        self.entities.alive_count()
    }

    /// Get entity location
    pub fn entity_location(&self, entity: Entity) -> Option<EntityLocation> {
        if !self.entities.is_alive(entity) {
            return None;
        }
        let loc = self.locations.get(entity.index() as usize)?;
        if loc.is_valid() {
            Some(*loc)
        } else {
            None
        }
    }

    // ========== Component Access ==========

    /// Add a component to an entity
    pub fn add_component<T: Send + Sync + 'static>(&mut self, entity: Entity, component: T) -> bool {
        if !self.entities.is_alive(entity) {
            return false;
        }

        // Ensure component is registered
        let component_id = self.register_component::<T>();
        let component_info = self.components.get_info(component_id).unwrap().clone();

        let location = self.locations[entity.index() as usize];
        if !location.is_valid() {
            return false;
        }

        let current_archetype = self.archetypes.get(location.archetype_id).unwrap();

        // Already has this component?
        if current_archetype.has_component(component_id) {
            // Update existing component
            if let Some(archetype) = self.archetypes.get_mut(location.archetype_id) {
                if let Some(storage) = archetype.storage_mut(component_id) {
                    unsafe {
                        *storage.get_mut::<T>(location.row) = component;
                    }
                }
            }
            return true;
        }

        // Need to move to a new archetype
        let mut new_components: Vec<ComponentInfo> = current_archetype
            .components()
            .iter()
            .filter_map(|&id| self.components.get_info(id).cloned())
            .collect();
        new_components.push(component_info);

        let new_archetype_id = self.archetypes.get_or_create(new_components);

        // Get both archetypes
        if let Some((src, dst)) = self.archetypes.get_mut2(location.archetype_id, new_archetype_id) {
            // Move entity data
            if let Some((new_row, swapped)) = src.move_entity_to(location.row, dst) {
                // Update swapped entity's location
                if let Some(swapped_entity) = swapped {
                    self.locations[swapped_entity.index() as usize].row = location.row;
                }

                // Add the new component
                if let Some(storage) = dst.storage_mut(component_id) {
                    storage.push(component);
                }

                self.locations[entity.index() as usize] = EntityLocation::new(new_archetype_id, new_row);
            }
        }

        true
    }

    /// Remove a component from an entity
    pub fn remove_component<T: Clone + Send + Sync + 'static>(&mut self, entity: Entity) -> Option<T> {
        if !self.entities.is_alive(entity) {
            return None;
        }

        let component_id = self.components.get_id::<T>()?;
        let location = self.locations[entity.index() as usize];
        if !location.is_valid() {
            return None;
        }

        let current_archetype = self.archetypes.get(location.archetype_id)?;

        // Doesn't have this component?
        if !current_archetype.has_component(component_id) {
            return None;
        }

        // Get the component value before moving
        let value = unsafe {
            current_archetype
                .storage(component_id)?
                .get::<T>(location.row)
                .clone()
        };

        // Create new archetype without this component
        let new_components: Vec<ComponentInfo> = current_archetype
            .components()
            .iter()
            .filter(|&&id| id != component_id)
            .filter_map(|&id| self.components.get_info(id).cloned())
            .collect();

        let new_archetype_id = self.archetypes.get_or_create(new_components);

        // Move entity to new archetype
        if let Some((src, dst)) = self.archetypes.get_mut2(location.archetype_id, new_archetype_id) {
            if let Some((new_row, swapped)) = src.move_entity_to(location.row, dst) {
                if let Some(swapped_entity) = swapped {
                    self.locations[swapped_entity.index() as usize].row = location.row;
                }
                self.locations[entity.index() as usize] = EntityLocation::new(new_archetype_id, new_row);
            }
        }

        Some(value)
    }

    /// Get a component reference
    pub fn get_component<T: Send + Sync + 'static>(&self, entity: Entity) -> Option<&T> {
        if !self.entities.is_alive(entity) {
            return None;
        }

        let component_id = self.components.get_id::<T>()?;
        let location = self.locations.get(entity.index() as usize)?;
        if !location.is_valid() {
            return None;
        }

        let archetype = self.archetypes.get(location.archetype_id)?;
        let storage = archetype.storage(component_id)?;

        Some(unsafe { storage.get::<T>(location.row) })
    }

    /// Get a mutable component reference
    pub fn get_component_mut<T: Send + Sync + 'static>(&mut self, entity: Entity) -> Option<&mut T> {
        if !self.entities.is_alive(entity) {
            return None;
        }

        let component_id = self.components.get_id::<T>()?;
        let location = *self.locations.get(entity.index() as usize)?;
        if !location.is_valid() {
            return None;
        }

        let archetype = self.archetypes.get_mut(location.archetype_id)?;
        let storage = archetype.storage_mut(component_id)?;

        Some(unsafe { storage.get_mut::<T>(location.row) })
    }

    /// Check if entity has a component
    pub fn has_component<T: Send + Sync + 'static>(&self, entity: Entity) -> bool {
        if !self.entities.is_alive(entity) {
            return false;
        }

        let component_id = match self.components.get_id::<T>() {
            Some(id) => id,
            None => return false,
        };

        let location = match self.locations.get(entity.index() as usize) {
            Some(loc) if loc.is_valid() => loc,
            _ => return false,
        };

        self.archetypes
            .get(location.archetype_id)
            .map(|arch| arch.has_component(component_id))
            .unwrap_or(false)
    }

    // ========== Resources ==========

    /// Insert a resource
    pub fn insert_resource<R: Any + Send + Sync>(&mut self, resource: R) {
        self.resources.insert(resource);
    }

    /// Remove a resource
    pub fn remove_resource<R: Any + Send + Sync>(&mut self) -> Option<R> {
        self.resources.remove::<R>()
    }

    /// Get a resource reference
    pub fn resource<R: Any + Send + Sync>(&self) -> Option<&R> {
        self.resources.get::<R>()
    }

    /// Get a mutable resource reference
    pub fn resource_mut<R: Any + Send + Sync>(&mut self) -> Option<&mut R> {
        self.resources.get_mut::<R>()
    }

    /// Check if a resource exists
    pub fn has_resource<R: Any + Send + Sync>(&self) -> bool {
        self.resources.contains::<R>()
    }

    // ========== Queries ==========

    /// Create a query
    pub fn query(&self, descriptor: QueryDescriptor) -> QueryState {
        let mut state = QueryState::new(descriptor);
        state.update(&self.archetypes);
        state
    }

    /// Get archetypes (for queries)
    pub fn archetypes(&self) -> &Archetypes {
        &self.archetypes
    }

    /// Get archetypes mutably
    pub fn archetypes_mut(&mut self) -> &mut Archetypes {
        &mut self.archetypes
    }

    // ========== Systems ==========

    /// Add a system
    pub fn add_system(&mut self, system: Box<dyn System>) {
        self.scheduler.add_system(system);
    }

    /// Run all systems
    pub fn run_systems(&mut self) {
        // Use a temporary wrapper to satisfy SystemWorld trait
        let mut world_ref = WorldRef { world: self as *mut World };
        self.scheduler.run(&mut world_ref);
    }

    /// Get the scheduler
    pub fn scheduler(&self) -> &SystemScheduler {
        &self.scheduler
    }

    /// Get the scheduler mutably
    pub fn scheduler_mut(&mut self) -> &mut SystemScheduler {
        &mut self.scheduler
    }

    // ========== Maintenance ==========

    /// Clear all entities and components
    pub fn clear(&mut self) {
        self.spawn_queue.clear();
        self.despawn_queue.clear();

        for archetype in self.archetypes.iter_mut() {
            for entity in archetype.entities().to_vec() {
                self.entities.deallocate(entity);
            }
        }

        self.archetypes = Archetypes::new();
        self.locations.clear();
    }
}

impl Default for World {
    fn default() -> Self {
        Self::new()
    }
}

/// Reference wrapper for SystemWorld implementation
struct WorldRef {
    world: *mut World,
}

impl SystemWorld for WorldRef {
    fn get_component_ptr(&self, entity_id: u64, component_id: ComponentId) -> Option<*const u8> {
        let world = unsafe { &*self.world };
        let entity = Entity::from_bits(entity_id);

        if !world.is_alive(entity) {
            return None;
        }

        let location = world.locations.get(entity.index() as usize)?;
        if !location.is_valid() {
            return None;
        }

        let archetype = world.archetypes.get(location.archetype_id)?;
        let storage = archetype.storage(component_id)?;
        storage.get_raw(location.row).map(|p| p.as_ptr() as *const u8)
    }

    fn get_component_ptr_mut(&self, entity_id: u64, component_id: ComponentId) -> Option<*mut u8> {
        let world = unsafe { &mut *self.world };
        let entity = Entity::from_bits(entity_id);

        if !world.is_alive(entity) {
            return None;
        }

        let location = *world.locations.get(entity.index() as usize)?;
        if !location.is_valid() {
            return None;
        }

        let archetype = world.archetypes.get_mut(location.archetype_id)?;
        let storage = archetype.storage_mut(component_id)?;
        storage.get_raw(location.row).map(|p| p.as_ptr())
    }

    fn has_component(&self, entity_id: u64, component_id: ComponentId) -> bool {
        let world = unsafe { &*self.world };
        let entity = Entity::from_bits(entity_id);

        if !world.is_alive(entity) {
            return false;
        }

        let location = match world.locations.get(entity.index() as usize) {
            Some(loc) if loc.is_valid() => loc,
            _ => return false,
        };

        world
            .archetypes
            .get(location.archetype_id)
            .map(|arch| arch.has_component(component_id))
            .unwrap_or(false)
    }

    fn get_resource_ptr(&self, type_id: TypeId) -> Option<*const u8> {
        let world = unsafe { &*self.world };
        world.resources.get_ptr(type_id)
    }

    fn get_resource_ptr_mut(&self, type_id: TypeId) -> Option<*mut u8> {
        let world = unsafe { &mut *self.world };
        world.resources.get_ptr_mut(type_id)
    }
}

// Safety: World uses internal synchronization for concurrent access
unsafe impl Send for WorldRef {}
unsafe impl Sync for WorldRef {}

#[cfg(test)]
mod tests {
    use super::*;

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

    #[derive(Debug, PartialEq)]
    struct Name(String);

    #[test]
    fn test_world_spawn_despawn() {
        let mut world = World::new();

        let e1 = world.spawn();
        let e2 = world.spawn();

        assert!(world.is_alive(e1));
        assert!(world.is_alive(e2));
        assert_eq!(world.entity_count(), 2);

        world.despawn(e1);
        assert!(!world.is_alive(e1));
        assert!(world.is_alive(e2));
        assert_eq!(world.entity_count(), 1);
    }

    #[test]
    fn test_world_components() {
        let mut world = World::new();

        let entity = world.spawn();
        world.add_component(entity, Position { x: 1.0, y: 2.0 });
        world.add_component(entity, Velocity { x: 0.5, y: -0.5 });

        assert!(world.has_component::<Position>(entity));
        assert!(world.has_component::<Velocity>(entity));

        let pos = world.get_component::<Position>(entity).unwrap();
        assert_eq!(pos.x, 1.0);
        assert_eq!(pos.y, 2.0);

        // Modify component
        if let Some(vel) = world.get_component_mut::<Velocity>(entity) {
            vel.x = 1.0;
        }

        let vel = world.get_component::<Velocity>(entity).unwrap();
        assert_eq!(vel.x, 1.0);
    }

    #[test]
    fn test_world_remove_component() {
        let mut world = World::new();

        let entity = world.spawn();
        world.add_component(entity, Position { x: 1.0, y: 2.0 });
        world.add_component(entity, Velocity { x: 0.5, y: -0.5 });

        let removed = world.remove_component::<Position>(entity);
        assert_eq!(removed, Some(Position { x: 1.0, y: 2.0 }));
        assert!(!world.has_component::<Position>(entity));
        assert!(world.has_component::<Velocity>(entity));
    }

    #[test]
    fn test_world_resources() {
        let mut world = World::new();

        #[derive(Debug, PartialEq)]
        struct Time(f32);

        world.insert_resource(Time(0.0));

        assert!(world.has_resource::<Time>());
        assert_eq!(world.resource::<Time>().unwrap().0, 0.0);

        if let Some(time) = world.resource_mut::<Time>() {
            time.0 = 1.5;
        }

        assert_eq!(world.resource::<Time>().unwrap().0, 1.5);
    }
}
