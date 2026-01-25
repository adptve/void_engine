# void_ecs Integration

This document provides integration diagrams for the void_ecs module, an archetype-based Entity Component System designed for cache-efficient iteration and hot-reload support.

## Architecture Overview

The void_ecs module is **header-only by design**, maximizing inlining opportunities for performance-critical iteration patterns. All types are templated or inline to enable compiler optimizations.

## Class Diagram

```mermaid
classDiagram
    direction TB

    %% Core Entity Types
    class Entity {
        +EntityIndex index
        +Generation generation
        +is_null() bool
        +is_valid() bool
        +to_bits() uint64_t
        +from_bits(uint64_t) Entity$
    }

    class EntityAllocator {
        -vector~Generation~ generations_
        -vector~EntityIndex~ free_list_
        -size_t alive_count_
        +allocate() Entity
        +deallocate(Entity) bool
        +is_alive(Entity) bool
    }

    class EntityLocation {
        +ArchetypeId archetype_id
        +size_t row
        +is_valid() bool
    }

    %% Component Types
    class ComponentId {
        +uint32_t id
        +is_valid() bool
        +value() uint32_t
    }

    class ComponentInfo {
        +ComponentId id
        +string name
        +size_t size
        +size_t align
        +type_index type_id
        +function drop_fn
        +function move_fn
        +function clone_fn
        +of~T~() ComponentInfo$
    }

    class ComponentRegistry {
        -vector~ComponentInfo~ components_
        -unordered_map type_map_
        +register_component~T~() ComponentId
        +get_id~T~() optional
        +get_info(ComponentId) ComponentInfo*
    }

    class ComponentStorage {
        -ComponentInfo info_
        -vector~byte~ data_
        -size_t len_
        +push~T~(T) void
        +get~T~(size_t) T&
        +get_raw(size_t) void*
        +swap_remove(size_t) bool
    }

    %% Archetype Types
    class ArchetypeId {
        +uint32_t id
        +is_valid() bool
    }

    class Archetype {
        -ArchetypeId id_
        -vector~ComponentId~ components_
        -BitSet component_mask_
        -vector~ComponentStorage~ storages_
        -vector~Entity~ entities_
        +has_component(ComponentId) bool
        +add_entity(Entity, data) size_t
        +remove_entity(size_t) optional~Entity~
        +get_component~T~(ComponentId, size_t) T*
    }

    class Archetypes {
        -vector~unique_ptr~Archetype~~ archetypes_
        -map signature_map_
        +get(ArchetypeId) Archetype*
        +get_or_create(ComponentInfo[]) ArchetypeId
        +find(ComponentId[]) optional~ArchetypeId~
    }

    %% Query Types
    class QueryDescriptor {
        -vector~ComponentAccess~ components_
        -BitSet required_mask_
        -BitSet excluded_mask_
        +read(ComponentId) QueryDescriptor&
        +write(ComponentId) QueryDescriptor&
        +without(ComponentId) QueryDescriptor&
        +matches_archetype(Archetype) bool
    }

    class QueryState {
        -QueryDescriptor descriptor_
        -vector~ArchetypeId~ matched_archetypes_
        +update(Archetypes) void
        +matched_archetypes() vector~ArchetypeId~
    }

    class QueryIter {
        -Archetypes* archetypes_
        -vector~ArchetypeId~* matched_
        -size_t archetype_index_
        -size_t row_
        +entity() Entity
        +next() bool
        +empty() bool
    }

    %% System Types
    class SystemDescriptor {
        +string name
        +SystemStage stage
        +vector~QueryDescriptor~ queries
        +vector~ResourceAccess~ resources
        +bool exclusive
        +conflicts_with(SystemDescriptor) bool
    }

    class System {
        <<interface>>
        +descriptor() SystemDescriptor
        +run(World) void
    }

    class SystemScheduler {
        -array~vector~System~~ stages_
        +add_system(System) void
        +run(World) void
        +run_stage(World, SystemStage) void
    }

    %% World
    class World {
        -EntityAllocator entities_
        -vector~EntityLocation~ locations_
        -ComponentRegistry components_
        -Archetypes archetypes_
        -Resources resources_
        +spawn() Entity
        +despawn(Entity) bool
        +add_component~T~(Entity, T) bool
        +get_component~T~(Entity) T*
        +query(QueryDescriptor) QueryState
    }

    %% Hierarchy
    class Parent {
        +Entity entity
    }

    class Children {
        +vector~Entity~ entities
        +add(Entity) void
        +remove(Entity) void
    }

    class LocalTransform {
        +Vec3 position
        +Quat rotation
        +Vec3 scale
        +to_matrix() Mat4
    }

    class GlobalTransform {
        +Mat4 matrix
        +position() Vec3
    }

    %% Snapshot Types
    class WorldSnapshot {
        +uint32_t version
        +vector~EntitySnapshot~ entities
        +vector~ComponentMeta~ component_registry
    }

    class EntitySnapshot {
        +uint64_t entity_bits
        +vector~ComponentSnapshot~ components
    }

    %% Relationships
    World *-- EntityAllocator
    World *-- ComponentRegistry
    World *-- Archetypes
    World *-- Resources
    World o-- EntityLocation

    Archetypes *-- Archetype
    Archetype *-- ComponentStorage
    Archetype o-- Entity
    ComponentStorage o-- ComponentInfo

    ComponentRegistry *-- ComponentInfo

    QueryState o-- QueryDescriptor
    QueryIter o-- Archetypes
    QueryIter o-- QueryState

    SystemScheduler *-- System
    System o-- SystemDescriptor

    WorldSnapshot *-- EntitySnapshot
    EntitySnapshot *-- ComponentSnapshot
```

## Hot-Reload Flow

```mermaid
sequenceDiagram
    participant App as Application
    participant World as World
    participant Snap as WorldSnapshot
    participant Ser as Serializer
    participant Storage as File/Memory

    Note over App,Storage: === CAPTURE SNAPSHOT ===

    App->>World: take_world_snapshot()
    activate World

    World->>World: Iterate archetypes
    loop For each archetype
        World->>World: Capture entities[]
        loop For each entity
            World->>World: Capture component data
            Note right of World: Uses clone_fn if available<br/>Falls back to memcpy for POD
        end
    end

    World->>World: Capture component_registry metadata
    World-->>Snap: Return WorldSnapshot
    deactivate World

    App->>Ser: serialize_snapshot(snapshot)
    Ser->>Ser: Write version header
    Ser->>Ser: Write component registry
    Ser->>Ser: Write entity data
    Ser-->>Storage: Binary data

    Note over App,Storage: === CODE RELOAD OCCURS ===

    Note over App,Storage: === RESTORE SNAPSHOT ===

    Storage-->>Ser: Binary data
    App->>Ser: deserialize_snapshot(data)
    Ser->>Ser: Verify version
    Ser->>Ser: Parse component registry
    Ser->>Ser: Parse entities
    Ser-->>Snap: Return WorldSnapshot

    App->>World: apply_world_snapshot(snapshot)
    activate World

    World->>World: clear() existing state
    World->>World: Build component ID mapping<br/>(old ID -> new ID by name)

    loop For each entity snapshot
        World->>World: spawn() new entity
        loop For each component
            World->>World: Map old component ID to new
            World->>World: Verify size matches
            World->>World: add_component_raw()
        end
    end

    World-->>App: Return success
    deactivate World
```

## Entity Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Spawned: spawn()

    Spawned --> InArchetype: Added to empty archetype

    InArchetype --> MovedArchetype: add_component()
    MovedArchetype --> InArchetype: Component added

    InArchetype --> MovedArchetype: remove_component()
    MovedArchetype --> InArchetype: Component removed

    InArchetype --> Despawned: despawn()

    Despawned --> [*]: Generation incremented<br/>Index added to free_list

    note right of Spawned
        Entity allocated from free_list
        or new index created
    end note

    note right of MovedArchetype
        Entity moved between archetypes
        using swap-remove pattern
    end note

    note right of Despawned
        Old Entity handles become
        invalid via generation check
    end note
```

## Archetype Graph

```mermaid
graph TD
    subgraph "Archetype Graph (Component Transitions)"
        Empty["Empty Archetype<br/>{}"]
        A["Archetype A<br/>{Position}"]
        B["Archetype B<br/>{Velocity}"]
        AB["Archetype AB<br/>{Position, Velocity}"]
        ABC["Archetype ABC<br/>{Position, Velocity, Renderable}"]

        Empty -->|"+Position"| A
        Empty -->|"+Velocity"| B
        A -->|"+Velocity"| AB
        B -->|"+Position"| AB
        AB -->|"+Renderable"| ABC

        A -->|"-Position"| Empty
        AB -->|"-Velocity"| A
        AB -->|"-Position"| B
        ABC -->|"-Renderable"| AB
    end
```

## Data Flow (Query Iteration)

```mermaid
flowchart TB
    subgraph Query["Query Creation"]
        QD[QueryDescriptor]
        QD -->|"read(pos_id)"| QD1[Add required mask]
        QD1 -->|"write(vel_id)"| QD2[Add write access]
        QD2 -->|"without(static_id)"| QD3[Add excluded mask]
        QD3 -->|"build()"| QS[QueryState]
    end

    subgraph Match["Archetype Matching"]
        QS -->|"update()"| M1{For each archetype}
        M1 -->|"Check masks"| M2{Required ⊆ Archetype?}
        M2 -->|Yes| M3{Excluded ∩ Archetype = ∅?}
        M3 -->|Yes| M4[Add to matched_archetypes]
        M2 -->|No| M1
        M3 -->|No| M1
    end

    subgraph Iter["Iteration"]
        M4 --> I1[QueryIter created]
        I1 --> I2{More archetypes?}
        I2 -->|Yes| I3[Get archetype]
        I3 --> I4{More rows?}
        I4 -->|Yes| I5[Return entity + components]
        I5 -->|"next()"| I4
        I4 -->|No| I2
        I2 -->|No| I6[Done]
    end
```

## Module Dependencies

```mermaid
graph TB
    subgraph "void_ecs Module"
        ECS[void_ecs/ecs.hpp]
        ENT[entity.hpp]
        COMP[component.hpp]
        ARCH[archetype.hpp]
        QUERY[query.hpp]
        WORLD[world.hpp]
        SYS[system.hpp]
        SNAP[snapshot.hpp]
        HIER[hierarchy.hpp]
        BUND[bundle.hpp]
    end

    subgraph "Dependencies"
        CORE[void_core]
        STRUCT[void_structures]
        EVENT[void_event]
    end

    ECS --> ENT
    ECS --> COMP
    ECS --> ARCH
    ECS --> QUERY
    ECS --> WORLD
    ECS --> SYS
    ECS --> SNAP
    ECS --> HIER
    ECS --> BUND

    ARCH --> ENT
    ARCH --> COMP
    ARCH --> STRUCT

    QUERY --> ARCH

    WORLD --> ARCH
    WORLD --> QUERY
    WORLD --> COMP

    SYS --> QUERY

    SNAP --> WORLD

    HIER --> WORLD
    HIER --> QUERY

    BUND --> WORLD
    BUND --> HIER
```

## Memory Layout

```mermaid
graph LR
    subgraph "Archetype Storage (SoA)"
        E["entities[]<br/>[E0, E1, E2, E3]"]
        P["Position[]<br/>[P0, P1, P2, P3]"]
        V["Velocity[]<br/>[V0, V1, V2, V3]"]
        R["Renderable[]<br/>[R0, R1, R2, R3]"]
    end

    subgraph "Cache-Friendly Iteration"
        direction TB
        IT1["for row in 0..4:"]
        IT2["  pos = Position[row]"]
        IT3["  vel = Velocity[row]"]
        IT4["  // Sequential memory access"]
    end

    E --> IT1
    P --> IT2
    V --> IT3
```

## Key Design Decisions

### Header-Only Architecture

The entire ECS module is implemented as header-only code because:

1. **Inlining**: ECS iteration is performance-critical; inline implementations allow the compiler to optimize away function call overhead
2. **Templates**: Heavy use of templates for type safety requires header definitions
3. **Zero-Cost Abstractions**: Queries, iterators, and component access compile to direct memory operations
4. **Link-Time Optimization**: All code visible to compiler enables aggressive cross-module optimization

### Generational Entities

```cpp
struct Entity {
    EntityIndex index;      // 32-bit slot in allocator
    Generation generation;  // 32-bit version counter
};
```

- Prevents use-after-free bugs
- O(1) validity checks
- No dangling reference issues across hot-reload

### Archetype-Based Storage

- Entities with same components stored contiguously
- Cache-friendly iteration patterns
- Swap-remove for O(1) entity removal
- Graph edges for fast archetype transitions

### Hot-Reload Support

- Component ID mapping by name (survives recompilation)
- Binary snapshot format with version checking
- Size validation prevents data corruption
- Clone functions for non-POD components
