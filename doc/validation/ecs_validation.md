# void_ecs Validation Checklist

This document tracks the validation status of the void_ecs module implementation.

## Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| entity.hpp | Complete | Entity, EntityAllocator, EntityLocation, ArchetypeId |
| component.hpp | Complete | ComponentId, ComponentInfo, ComponentRegistry, ComponentStorage |
| archetype.hpp | Complete | Archetype, Archetypes, ArchetypeEdge |
| query.hpp | Complete | Access, QueryDescriptor, QueryState, QueryIter, ArchetypeQueryIter |
| world.hpp | Complete | World, Resources, EntityBuilder |
| system.hpp | Complete | SystemId, SystemStage, SystemDescriptor, System, SystemScheduler |
| snapshot.hpp | Complete | WorldSnapshot, EntitySnapshot, ComponentSnapshot, serialization |
| hierarchy.hpp | Complete | Vec3, Quat, Mat4, Parent, Children, LocalTransform, GlobalTransform |
| bundle.hpp | Complete | TupleBundle, TransformBundle, SpatialBundle, HierarchyBundle |

**Architecture Note**: void_ecs is header-only by design for maximum performance. All implementations are inline.

---

## Core Entity System

### Entity

- [ ] **Entity construction**: Default constructor creates null entity
- [ ] **Entity validity**: `is_null()` and `is_valid()` work correctly
- [ ] **Entity comparison**: `operator==`, `operator<` work correctly
- [ ] **Bit encoding**: `to_bits()` and `from_bits()` roundtrip correctly
- [ ] **String representation**: `to_string()` produces readable output
- [ ] **std::hash**: Entity can be used as key in unordered containers

### EntityAllocator

- [ ] **Allocation**: `allocate()` returns unique entities
- [ ] **Deallocation**: `deallocate()` marks entity as dead
- [ ] **Generation increment**: Deallocated slots have incremented generation
- [ ] **Free list reuse**: Deallocated slots are reused on next allocation
- [ ] **Alive check**: `is_alive()` correctly distinguishes live/dead entities
- [ ] **Stale reference detection**: Old entity handles fail `is_alive()` after deallocation
- [ ] **Capacity**: `reserve()` pre-allocates without changing alive count
- [ ] **Clear**: `clear()` resets allocator to initial state

### EntityLocation

- [ ] **Construction**: Creates valid location from archetype ID and row
- [ ] **Invalid factory**: `invalid()` creates recognizable invalid location
- [ ] **Validity check**: `is_valid()` returns false for invalid locations

---

## Component System

### ComponentId

- [ ] **Construction**: Default creates invalid ID
- [ ] **Validity**: `is_valid()` returns false for INVALID_ID
- [ ] **Comparison**: All comparison operators work correctly

### ComponentInfo

- [ ] **Type creation**: `of<T>()` captures size, alignment, type_index
- [ ] **Drop function**: `drop_fn` correctly destructs components
- [ ] **Move function**: `move_fn` correctly move-constructs components
- [ ] **Clone function**: `of_cloneable<T>()` captures clone function

### ComponentRegistry

- [ ] **Registration**: `register_component<T>()` returns unique ID per type
- [ ] **Idempotent**: Re-registering same type returns same ID
- [ ] **Lookup by type**: `get_id<T>()` finds registered components
- [ ] **Lookup by name**: `get_id_by_name()` finds by type name
- [ ] **Info retrieval**: `get_info()` returns correct metadata
- [ ] **Dynamic registration**: `register_dynamic()` works for runtime types

### ComponentStorage

- [ ] **Construction**: Creates storage with correct component info
- [ ] **Typed push**: `push<T>()` stores component correctly
- [ ] **Typed get**: `get<T>()` retrieves correct component
- [ ] **Raw access**: `get_raw()` returns correct pointer
- [ ] **Swap remove**: `swap_remove()` removes and swaps with last
- [ ] **Swap remove no drop**: `swap_remove_no_drop()` skips destructor
- [ ] **Clear**: `clear()` calls destructors and resets storage
- [ ] **Move semantics**: Move constructor/assignment work correctly
- [ ] **Non-copyable**: Copy operations are deleted

---

## Archetype System

### ArchetypeId

- [ ] **Construction**: Creates from uint32_t
- [ ] **Invalid factory**: `invalid()` creates recognizable invalid ID
- [ ] **Validity**: `is_valid()` returns false for invalid ID

### Archetype

- [ ] **Construction**: Creates with sorted component set
- [ ] **Component mask**: Correctly tracks component presence
- [ ] **Has component**: `has_component()` works for present/absent
- [ ] **Add entity**: `add_entity()` stores entity and component data
- [ ] **Remove entity**: `remove_entity()` uses swap-remove pattern
- [ ] **Storage access**: `storage()` returns correct ComponentStorage
- [ ] **Component access**: `get_component<T>()` returns correct data
- [ ] **Raw access**: `get_component_raw()` returns correct pointer
- [ ] **Entity list**: `entities()` returns correct entity vector
- [ ] **Graph edges**: `edge()`, `set_edge()` manage transitions

### Archetypes

- [ ] **Construction**: Creates with empty archetype (ID 0)
- [ ] **Empty archetype**: `empty()` returns ID of empty archetype
- [ ] **Get archetype**: `get()` returns correct archetype by ID
- [ ] **Find by signature**: `find()` locates existing archetype
- [ ] **Get or create**: `get_or_create()` creates new or returns existing
- [ ] **Iteration**: Range-based for loop works over archetypes

---

## Query System

### Access Enum

- [ ] **Read**: Represents immutable required access
- [ ] **Write**: Represents mutable required access
- [ ] **OptionalRead**: Represents optional immutable access
- [ ] **OptionalWrite**: Represents optional mutable access
- [ ] **Without**: Represents exclusion filter

### ComponentAccess

- [ ] **Required check**: `is_required()` returns true for Read/Write
- [ ] **Optional check**: `is_optional()` returns true for optional access
- [ ] **Excluded check**: `is_excluded()` returns true for Without
- [ ] **Write check**: `is_write()` returns true for Write/OptionalWrite

### QueryDescriptor

- [ ] **Builder pattern**: Chained methods return self-reference
- [ ] **Read addition**: `read()` adds required read component
- [ ] **Write addition**: `write()` adds required write component
- [ ] **Optional addition**: `optional_read()`/`optional_write()` work
- [ ] **Exclusion**: `without()` adds excluded component
- [ ] **Build**: `build()` computes required/excluded masks
- [ ] **Archetype matching**: `matches_archetype()` correctly filters
- [ ] **Conflict detection**: `conflicts_with()` detects write conflicts

### QueryState

- [ ] **Construction**: Takes QueryDescriptor
- [ ] **Update**: `update()` finds matching archetypes
- [ ] **Incremental update**: Only checks new archetypes on subsequent calls
- [ ] **Invalidation**: `invalidate()` forces full rescan

### QueryIter

- [ ] **Construction**: Creates from Archetypes and QueryState
- [ ] **Empty check**: `empty()` returns true when exhausted
- [ ] **Entity access**: `entity()` returns current entity
- [ ] **Row access**: `row()` returns current row index
- [ ] **Archetype access**: `archetype()` returns current archetype
- [ ] **Advancement**: `next()` moves to next entity/archetype
- [ ] **Skip empty**: Automatically skips empty archetypes

---

## World

### Entity Management

- [ ] **Spawn**: `spawn()` creates entity in empty archetype
- [ ] **Despawn**: `despawn()` removes entity and updates locations
- [ ] **Alive check**: `is_alive()` correctly reports entity status
- [ ] **Entity count**: `entity_count()` returns correct count
- [ ] **Location lookup**: `entity_location()` returns correct location

### Component Management

- [ ] **Registration**: `register_component<T>()` delegates to registry
- [ ] **Add component**: `add_component<T>()` adds to entity
- [ ] **Add to existing**: Adding existing component updates value
- [ ] **Add moves archetype**: Adding new component moves entity
- [ ] **Remove component**: `remove_component<T>()` returns removed value
- [ ] **Get component**: `get_component<T>()` returns pointer or null
- [ ] **Has component**: `has_component<T>()` returns correct status

### Resources

- [ ] **Insert**: `insert_resource<R>()` stores singleton
- [ ] **Get**: `resource<R>()` returns pointer to resource
- [ ] **Remove**: `remove_resource<R>()` returns and removes
- [ ] **Has**: `has_resource<R>()` returns correct status

### Queries

- [ ] **Create query**: `query()` creates QueryState and updates
- [ ] **Update query**: `update_query()` refreshes matched archetypes
- [ ] **Query iterator**: `query_iter()` creates working iterator

### Hot-Reload Support

- [ ] **Raw add**: `add_component_raw()` adds from raw bytes
- [ ] **Location set**: `set_entity_location()` directly sets location
- [ ] **Accessor**: `entity_allocator()` provides mutable access

### Maintenance

- [ ] **Clear**: `clear()` resets all entities and archetypes

---

## System Scheduling

### SystemId

- [ ] **Construction**: Creates from size_t
- [ ] **From name**: `from_name()` creates from string hash

### SystemStage

- [ ] **All stages**: First, PreUpdate, Update, PostUpdate, PreRender, Render, PostRender, Last

### SystemDescriptor

- [ ] **Construction**: Creates with name
- [ ] **Stage setting**: `set_stage()` sets execution stage
- [ ] **Query addition**: `add_query()` adds component requirements
- [ ] **Resource access**: `read_resource<R>()`, `write_resource<R>()` work
- [ ] **Ordering**: `after()`, `before()` set dependencies
- [ ] **Exclusive**: `set_exclusive()` marks system as exclusive
- [ ] **Conflict detection**: `conflicts_with()` detects conflicts

### System Interface

- [ ] **Abstract interface**: `descriptor()` and `run()` are pure virtual
- [ ] **Lifecycle hooks**: `on_add()`, `on_remove()` called at appropriate times

### FunctionSystem

- [ ] **Lambda storage**: Stores callable correctly
- [ ] **Run execution**: `run()` invokes stored function

### SystemScheduler

- [ ] **Add system**: `add_system()` adds to correct stage
- [ ] **Run all**: `run()` executes all stages in order
- [ ] **Run stage**: `run_stage()` executes single stage
- [ ] **Parallel batching**: `create_parallel_batches()` groups non-conflicting

---

## Snapshot System

### ComponentSnapshot

- [ ] **Construction**: Stores component ID, name, size, data

### EntitySnapshot

- [ ] **Construction**: Stores entity bits
- [ ] **Component storage**: Stores vector of ComponentSnapshot

### WorldSnapshot

- [ ] **Version**: Stores format version
- [ ] **Entity storage**: Stores all entity snapshots
- [ ] **Registry metadata**: Stores component registry info
- [ ] **Compatibility check**: `is_compatible()` checks version

### Capture Functions

- [ ] **take_world_snapshot**: Captures all entities and components
- [ ] **Clone handling**: Uses clone_fn when available
- [ ] **POD fallback**: Uses memcpy for POD types
- [ ] **Registry capture**: Captures component metadata

### Restore Functions

- [ ] **apply_world_snapshot**: Restores from snapshot
- [ ] **ID mapping**: Maps old component IDs to new by name
- [ ] **Size validation**: Verifies component sizes match
- [ ] **World clear**: Clears existing state before restore

### Serialization

- [ ] **serialize_snapshot**: Produces binary format
- [ ] **deserialize_snapshot**: Parses binary format
- [ ] **Version check**: Rejects incompatible versions
- [ ] **Roundtrip**: serialize -> deserialize preserves data

---

## Hierarchy System

### Transform Types

- [ ] **Vec3**: Arithmetic operations work correctly
- [ ] **Quat**: Rotation operations work correctly
- [ ] **Mat4**: Matrix multiplication and transform_point work

### Hierarchy Components

- [ ] **Parent**: Stores parent entity reference
- [ ] **Children**: add(), remove(), contains() work correctly
- [ ] **LocalTransform**: to_matrix() produces correct matrix
- [ ] **GlobalTransform**: position() extracts translation
- [ ] **HierarchyDepth**: Stores depth value
- [ ] **Visible**: Stores visibility flag
- [ ] **InheritedVisibility**: Stores inherited flag

### Hierarchy Commands

- [ ] **set_parent**: Sets parent and updates children lists
- [ ] **remove_parent**: Removes parent relationship
- [ ] **despawn_recursive**: Despawns entity and all descendants

### Hierarchy Validation

- [ ] **has_hierarchy_cycle**: Detects cycles correctly

### Transform Propagation

- [ ] **propagate_transforms**: Updates GlobalTransform from LocalTransform chain
- [ ] **propagate_visibility**: Updates InheritedVisibility from parent chain
- [ ] **Level-order traversal**: Processes parent before children

---

## Bundle System

### TupleBundle

- [ ] **Construction**: Stores components in tuple
- [ ] **add_to_entity**: Adds all components to entity

### TransformBundle

- [ ] **Default**: Creates identity transform
- [ ] **Position**: Creates with position only
- [ ] **Full**: Creates with position, rotation, scale

### SpatialBundle

- [ ] **Default**: Creates with visibility

### HierarchyBundle

- [ ] **Default**: Creates Children and HierarchyDepth

### World Extensions

- [ ] **spawn_with_bundle**: Spawns and adds bundle
- [ ] **spawn_with**: Spawns with variadic components

### BundleEntityBuilder

- [ ] **with**: Adds single component
- [ ] **with_bundle**: Adds bundle
- [ ] **with_components**: Adds variadic components
- [ ] **child_of**: Sets parent
- [ ] **build**: Returns entity

---

## Performance Validation

### Memory Layout

- [ ] **SoA storage**: Components stored in contiguous arrays
- [ ] **Cache alignment**: Component arrays are properly aligned
- [ ] **No allocations during iteration**: Iteration doesn't allocate

### Iteration Performance

- [ ] **Linear memory access**: Iteration accesses memory sequentially
- [ ] **Minimal indirection**: Query iteration has minimal pointer chasing
- [ ] **Inline potential**: Hot paths can be fully inlined

### Archetype Operations

- [ ] **O(1) component lookup**: Bitmask-based has_component
- [ ] **O(1) entity removal**: Swap-remove pattern
- [ ] **Efficient transitions**: Graph edges cache archetype lookups

---

## Integration Tests

### Basic Usage

- [ ] **Entity lifecycle**: Create, add components, remove, destroy
- [ ] **Query iteration**: Find and modify entities with specific components
- [ ] **System execution**: Systems process entities correctly

### Hot-Reload

- [ ] **Snapshot capture**: Captures all entity state
- [ ] **Snapshot restore**: Restores entity state correctly
- [ ] **ID remapping**: Handles component ID changes
- [ ] **Size mismatch handling**: Skips incompatible components

### Hierarchy

- [ ] **Parent-child relationships**: set_parent creates correct relationships
- [ ] **Transform propagation**: Children inherit parent transforms
- [ ] **Recursive despawn**: Despawning parent removes children

---

## Compilation Validation

- [ ] **Headers compile standalone**: Each header compiles without others
- [ ] **No ODR violations**: Inline/constexpr used correctly
- [ ] **Template instantiation**: Common types compile without errors
- [ ] **Windows/Linux/macOS**: Compiles on all target platforms
- [ ] **C++20 features**: Uses requires, concepts, <format> correctly
