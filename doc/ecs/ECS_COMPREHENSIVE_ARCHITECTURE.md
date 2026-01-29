# void_engine ECS Architecture - Comprehensive Documentation

> **Document Version**: 1.0
> **Last Updated**: 2026-01-29
> **Scope**: Complete documentation of the ECS system and its integration with all engine modules

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architectural Principles](#2-architectural-principles)
3. [ECS Core Module](#3-ecs-core-module)
4. [Component System](#4-component-system)
5. [Entity Management](#5-entity-management)
6. [Archetype System](#6-archetype-system)
7. [Query System](#7-query-system)
8. [System Scheduling](#8-system-scheduling)
9. [Hierarchy and Transforms](#9-hierarchy-and-transforms)
10. [Resources](#10-resources)
11. [Asset Integration](#11-asset-integration)
12. [Render Integration](#12-render-integration)
13. [Scene Integration](#13-scene-integration)
14. [Plugin Integration](#14-plugin-integration)
15. [Event System](#15-event-system)
16. [Hot-Reload Support](#16-hot-reload-support)
17. [Runtime Integration](#17-runtime-integration)
18. [Data Flow Diagrams](#18-data-flow-diagrams)
19. [Performance Characteristics](#19-performance-characteristics)
20. [Quick Reference](#20-quick-reference)

---

## 1. Executive Summary

The void_engine ECS (Entity Component System) is the **authoritative source of truth** for all runtime state. This is a production-grade, archetype-based ECS implementation in C++20 with the following characteristics:

| Feature | Description |
|---------|-------------|
| **Storage Model** | Archetype-based (Structure of Arrays) |
| **Entity Handles** | 64-bit generational indices |
| **Component Storage** | Type-erased byte arrays per archetype |
| **Query System** | Bitmask-based filtering with caching |
| **System Scheduling** | Stage-based with parallelization support |
| **Hot-Reload** | Snapshot/restore via binary serialization |
| **Header-Only** | Most implementation in `.hpp` for optimization |

**Key Files:**
- `include/void_engine/ecs/*.hpp` - All ECS headers
- `src/ecs/module.cpp` - Module linkage and utilities

**Namespace:** `void_ecs`

---

## 2. Architectural Principles

### 2.1 ECS is Authoritative

```
Architecture Invariant: ECS is the source of truth for ALL runtime state

Scene JSON → SceneInstantiator → ECS World (AUTHORITATIVE) → Derived Views
                                        ↑
                                   Runtime reads/writes here
```

**What this means:**
- Scene files are declarative *input*, not runtime state
- The `void_ecs::World` holds all live entity data
- Render data is *derived* from ECS each frame
- Hot-reload operates on ECS snapshots

### 2.2 Scene == World

The `void_scene::World` wraps `void_ecs::World` with additional context:
- Active layers (additive patches)
- Active plugins (systems)
- Active widgets (reactive UI)
- Spatial context (XR anchoring)

### 2.3 Plugins Contain Systems

Gameplay logic lives in plugins, not in the engine core:
- Plugins register systems with the ECS scheduler
- Systems query the ECS world
- Kernel orchestrates system execution

### 2.4 Everything is Loadable via API

The engine supports runtime loading of:
- Worlds (via API or local paths)
- Plugins (via dynamic loading)
- Assets (via asset server)
- Layers (as scene patches)

---

## 3. ECS Core Module

### 3.1 File Structure

```
include/void_engine/ecs/
├── fwd.hpp           # Forward declarations
├── entity_id.hpp     # EntityId conversion utilities
├── entity.hpp        # Entity + EntityAllocator
├── component.hpp     # ComponentId, Registry, Storage
├── archetype.hpp     # Archetype + Archetypes manager
├── query.hpp         # Query system + iterators
├── system.hpp        # System + Scheduler
├── world.hpp         # World + Resources + EntityBuilder
├── bundle.hpp        # Bundles (component groups)
├── hierarchy.hpp     # Hierarchy + Transforms
├── snapshot.hpp      # Hot-reload snapshot/restore
└── ecs.hpp           # Main include (exports all)

src/ecs/
├── module.cpp        # Module linkage + utilities
└── CMakeLists.txt    # Build configuration
```

### 3.2 Module Dependencies

```
void_ecs
    ├── void_core     # EntityId, Id types, Error handling
    ├── void_structures  # BitSet for component masks
    └── void_event    # Event integration
```

### 3.3 Header-Only Design

The ECS is header-only because:
- Heavy template usage (components, queries, systems)
- Compiler needs full definitions for optimization
- Similar to industry libraries (EnTT, flecs)
- `module.cpp` provides linkage symbols only

---

## 4. Component System

### 4.1 ComponentId

```cpp
// Unique 32-bit identifier for component types
struct ComponentId {
    std::uint32_t id;

    bool is_valid() const;
    bool operator==(const ComponentId&) const;
    std::size_t hash() const;
};
```

ComponentIds are assigned sequentially during registration.

### 4.2 ComponentInfo

```cpp
struct ComponentInfo {
    ComponentId id;
    std::string name;
    std::size_t size;
    std::size_t align;
    std::type_index type_index;

    // Function pointers for lifecycle
    void (*drop_fn)(void*);                          // Destructor
    void (*move_fn)(void* dst, void* src);           // Move constructor
    std::unique_ptr<void, void(*)(void*)> (*clone_fn)(const void*);  // Clone (optional)
};
```

### 4.3 ComponentRegistry

Global registry mapping types to metadata:

```cpp
class ComponentRegistry {
    // Registration
    ComponentId register_component<T>();
    ComponentId register_cloneable<T>();  // With clone support
    ComponentId register_dynamic(ComponentInfo info);

    // Lookup
    std::optional<ComponentId> get_id<T>() const;
    std::optional<ComponentId> get_id_by_name(const std::string&) const;
    const ComponentInfo* get_info(ComponentId) const;

    // Iteration
    template<typename F> void for_each(F&& func) const;
};
```

### 4.4 ComponentStorage

Type-erased byte-level storage for one component type:

```cpp
class ComponentStorage {
    std::vector<std::byte> data_;
    std::size_t size_;
    std::size_t alignment_;

    // Typed operations
    template<typename T> void push(T&& value);
    template<typename T> T& get(std::size_t index);
    template<typename T> std::span<T> as_mut_slice();

    // Generic operations
    void push_raw_bytes(const std::byte* data, std::size_t size);
    void swap_remove(std::size_t index);  // O(1) removal
    std::size_t size() const;
};
```

### 4.5 Component Design Guidelines

**POD-Safe Components:**
```cpp
// GOOD: Fixed-size arrays
struct TransformComponent {
    float position[3];
    float rotation[4];  // Quaternion
    float scale[3];
};

// BAD: Dynamic allocations
struct TransformComponent {
    std::string name;           // Heap allocation
    std::vector<float> values;  // Dynamic size
};
```

**Why POD?**
- Cache-efficient archetype storage
- Safe for binary serialization
- Deterministic memory layout
- Fast memcpy for archetype moves

---

## 5. Entity Management

### 5.1 Entity Handle

```cpp
struct Entity {
    EntityIndex index;      // 32-bit slot
    Generation generation;  // 32-bit version

    // Encoding for hashing/storage
    static constexpr Entity null();
    std::uint64_t to_bits() const;
    static Entity from_bits(std::uint64_t);

    bool is_null() const;
    bool operator==(const Entity&) const;
};
```

**Generational Safety:**
```cpp
Entity e1(5, 0);   // Index 5, generation 0
despawn(e1);       // Index 5 freed, generation → 1
Entity e2(5, 0);   // STALE - generation mismatch detected
```

### 5.2 EntityAllocator

```cpp
class EntityAllocator {
    std::vector<Generation> generations_;
    std::vector<EntityIndex> free_list_;
    std::uint32_t alive_count_;

    Entity allocate();                   // Returns new or reused slot
    void deallocate(Entity e);           // Marks dead, increments generation
    bool is_alive(Entity e) const;       // Checks generation match
    Generation current_generation(EntityIndex) const;
};
```

### 5.3 EntityLocation

Maps entity to storage location:

```cpp
struct EntityLocation {
    ArchetypeId archetype_id;  // Which archetype
    std::size_t row;           // Row in archetype's arrays
};
```

### 5.4 Entity Lifecycle

**Spawning:**
```cpp
Entity entity = world.spawn();  // Empty archetype
// OR
Entity entity = world.spawn();
world.add_component(entity, Position{0, 0, 0});
world.add_component(entity, Velocity{1, 0, 0});
```

**Despawning:**
```cpp
world.despawn(entity);
// Entity becomes invalid (generation incremented)
```

**Validity Check:**
```cpp
if (world.is_alive(entity)) {
    // Safe to access
}
```

---

## 6. Archetype System

### 6.1 What is an Archetype?

An archetype represents a unique combination of component types:
- All entities with the same components share an archetype
- Components are stored in Structure of Arrays (SoA) layout
- Adding/removing components moves entity to different archetype

```
Archetype {Position, Velocity}
├── Entity Array:    [E1, E2, E3]
├── Position Array:  [P1, P2, P3]
└── Velocity Array:  [V1, V2, V3]
```

### 6.2 Archetype Structure

```cpp
class Archetype {
    ArchetypeId id_;
    std::vector<ComponentId> components_;          // Sorted IDs
    void_structures::BitSet component_mask_;       // Fast matching
    std::vector<ComponentStorage> storages_;       // One per component
    std::map<ComponentId, std::size_t> component_indices_;
    std::vector<Entity> entities_;
    std::map<ComponentId, ArchetypeEdge> edges_;   // Graph connections

    // Core operations
    std::size_t add_entity(Entity e, /* component data */);
    std::optional<Entity> remove_entity(std::size_t row);
    bool has_component(ComponentId) const;
    ComponentStorage* storage(ComponentId);
    Entity entity_at(std::size_t row) const;
};
```

### 6.3 Archetype Graph

Edges enable O(1) archetype transition lookup:

```cpp
struct ArchetypeEdge {
    ArchetypeId add;     // Archetype if adding this component
    ArchetypeId remove;  // Archetype if removing this component
};

// Example: Archetype{A,B} + C → looks up edge[C].add → Archetype{A,B,C}
```

### 6.4 Archetypes Manager

```cpp
class Archetypes {
    std::vector<std::unique_ptr<Archetype>> archetypes_;
    Archetype* empty_archetype_;

    Archetype* find(const std::vector<ComponentId>& components);
    Archetype* get_or_create(const std::vector<ComponentId>& components);
    Archetype* get(ArchetypeId id);
};
```

### 6.5 SoA Memory Layout Benefits

```
Traditional OOP (AoS):           ECS Archetype (SoA):
[Pos1|Vel1] [Pos2|Vel2] ...     [Pos1|Pos2|Pos3...] [Vel1|Vel2|Vel3...]
    ↓                               ↓
Cache misses on iteration        Cache-friendly sequential access
```

---

## 7. Query System

### 7.1 Access Modes

```cpp
enum class Access : uint8_t {
    Read,           // Required, immutable
    Write,          // Required, mutable
    OptionalRead,   // Optional, immutable
    OptionalWrite,  // Optional, mutable
    Without,        // Must NOT have
};
```

### 7.2 QueryDescriptor

Builder pattern for query construction:

```cpp
auto query = QueryDescriptor()
    .read(position_id)
    .write(velocity_id)
    .optional_read(health_id)
    .without(static_id)
    .build();
```

After `build()`:
- `required_mask_` - Components that must exist
- `excluded_mask_` - Components that must not exist
- `optional_mask_` - Components that may exist

### 7.3 Archetype Matching

```cpp
bool matches(const Archetype& arch) const {
    const auto& mask = arch.component_mask();
    return (mask & required_mask_) == required_mask_
        && (mask & excluded_mask_).none();
}
```

### 7.4 QueryState (Cached Query)

```cpp
class QueryState {
    QueryDescriptor descriptor_;
    std::vector<ArchetypeId> matched_archetypes_;
    std::size_t last_archetype_count_;

    void update(const Archetypes& archetypes);  // Incremental update
};
```

### 7.5 Query Iterators

**ArchetypeQueryIter** - Single archetype iteration:
```cpp
class ArchetypeQueryIter {
    Entity entity() const;
    std::size_t row() const;
    template<typename T> T* get_component(ComponentId);
    void next();
    bool empty() const;
};
```

**QueryIter** - Multi-archetype iteration:
```cpp
class QueryIter {
    Entity entity() const;
    const Archetype* archetype() const;
    std::size_t row() const;
    template<typename T> T* get();
    void next();
    bool empty() const;
};
```

### 7.6 Type-Safe Query API

```cpp
// Write access
auto query = world.query_with<Position, Velocity>();

// Read-only access
auto query = world.query_read<Position, Rotation>();

// Iteration
for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
    Entity e = iter.entity();
    Position* pos = iter.get<Position>();
    Velocity* vel = iter.get<Velocity>();

    pos->x += vel->x * dt;
}
```

---

## 8. System Scheduling

### 8.1 System Stages

```cpp
enum class SystemStage : uint8_t {
    First = 0,      // Initialization
    PreUpdate,      // Before main logic
    Update,         // Main game logic (DEFAULT)
    PostUpdate,     // After main logic
    PreRender,      // Before rendering
    Render,         // Actual rendering
    PostRender,     // After rendering
    Last,           // Cleanup
    COUNT           // Total: 8 stages
};
```

### 8.2 SystemDescriptor

```cpp
class SystemDescriptor {
    std::string name_;
    SystemStage stage_ = SystemStage::Update;
    std::vector<QueryDescriptor> queries_;
    std::set<std::type_index> read_resources_;
    std::set<std::type_index> write_resources_;
    std::set<SystemId> run_after_;
    std::set<SystemId> run_before_;
    bool exclusive_ = false;

    // Builder methods
    SystemDescriptor& set_stage(SystemStage);
    SystemDescriptor& add_query(QueryDescriptor);
    template<typename R> SystemDescriptor& read_resource();
    template<typename R> SystemDescriptor& write_resource();
    SystemDescriptor& after(SystemId);
    SystemDescriptor& before(SystemId);
    SystemDescriptor& set_exclusive();

    // Conflict detection
    bool conflicts_with(const SystemDescriptor& other) const;
};
```

### 8.3 System Interface

```cpp
class System {
public:
    virtual const SystemDescriptor& descriptor() const = 0;
    virtual void run(World& world) = 0;

    virtual void on_add(World& world) {}
    virtual void on_remove(World& world) {}
};
```

### 8.4 FunctionSystem

Lambda-based system creation:

```cpp
auto system = make_system(
    SystemDescriptor("MovementSystem")
        .set_stage(SystemStage::Update)
        .read_resource<DeltaTime>(),
    [](World& world) {
        auto query = world.query_with<Position, Velocity>();
        auto dt = world.resource<DeltaTime>();

        for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
            Position* pos = iter.get<Position>();
            Velocity* vel = iter.get<Velocity>();
            pos->x += vel->x * dt->value;
        }
    }
);
```

### 8.5 SystemScheduler

```cpp
class SystemScheduler {
    std::vector<std::unique_ptr<System>> systems_;
    std::array<std::vector<std::size_t>, SystemStage::COUNT> stage_systems_;

    void add_system(std::unique_ptr<System> system);

    template<typename F>
    void add_system(SystemDescriptor desc, F&& func);

    void run(World& world);                       // All stages
    void run_stage(World& world, SystemStage);    // Single stage

    std::vector<SystemBatch> create_parallel_batches(SystemStage);
};
```

### 8.6 Parallel Batching

Systems without conflicts can run in parallel:

```cpp
auto batches = scheduler.create_parallel_batches(SystemStage::Update);

for (const auto& batch : batches) {
    // Systems in this batch can run concurrently
    std::for_each(std::execution::par,
                  batch.systems().begin(),
                  batch.systems().end(),
                  [&](std::size_t idx) {
                      systems_[idx]->run(world);
                  });
}
```

**Conflict Detection:**
- Same component accessed, at least one writes → conflict
- Same resource accessed, at least one writes → conflict
- Explicit ordering constraints → dependency

---

## 9. Hierarchy and Transforms

### 9.1 Hierarchy Components

```cpp
struct Parent {
    Entity entity;  // Parent entity reference
};

struct Children {
    std::vector<Entity> entities;
    void add(Entity child);
    void remove(Entity child);
};

struct HierarchyDepth {
    uint32_t depth;  // 0 = root
};
```

### 9.2 Transform Types

```cpp
struct Vec3 {
    float x, y, z;
    // Operations: +, -, *, dot, cross, normalize, length
};

struct Quat {
    float x, y, z, w;
    // From: axis_angle, euler, rotation_arc
    // Operations: rotate(vec), multiply, inverse, slerp
};

struct Mat4 {
    float m[16];  // Column-major
    // From: trs, translation, scale, rotation
    // Operations: multiply, inverse, transform_point
};

struct LocalTransform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};

struct GlobalTransform {
    Mat4 matrix;  // World-space (computed)
};
```

### 9.3 Visibility Components

```cpp
struct Visible {
    bool visible;
};

struct InheritedVisibility {
    bool visible;  // Computed from parent chain
};
```

### 9.4 Hierarchy Operations

```cpp
// Set parent-child relationship
set_parent(world, child, parent);

// Remove from parent
remove_parent(world, child);

// Despawn with all descendants
despawn_recursive(world, entity);

// Cycle detection
bool has_cycle = has_hierarchy_cycle(world, child, new_parent);
```

### 9.5 Transform Propagation

```cpp
void propagate_transforms(World& world) {
    // 1. Calculate hierarchy depths
    // 2. Find roots (depth == 0)
    // 3. Level-order traversal:
    //    GlobalTransform = parent.GlobalTransform * LocalTransform.to_matrix()
}

void propagate_visibility(World& world) {
    // InheritedVisibility = parent.InheritedVisibility && Visible
}
```

### 9.6 Transform Flow

```
TransformComponent (scene-level)
        ↓
sync_scene_to_ecs()
        ↓
LocalTransform (ECS, relative to parent)
        ↓
propagate_transforms()
        ↓
GlobalTransform (ECS, world-space - AUTHORITATIVE)
        ↓
RenderSceneGraph.rebuild()
        ↓
Render submission
```

---

## 10. Resources

### 10.1 ResourceEntry

Type-erased storage for any resource:

```cpp
class ResourceEntry {
    void* data_;
    std::function<void(void*)> deleter_;
    std::function<void*(void*)> mover_;

    template<typename T> static ResourceEntry create(T&& value);
    template<typename T> T* get();
};
```

### 10.2 Resources Container

```cpp
class Resources {
    std::unordered_map<std::type_index, ResourceEntry> resources_;

    template<typename R> void insert(R&& resource);
    template<typename R> std::optional<R> remove();
    template<typename R> R* get();
    template<typename R> bool has() const;
};
```

### 10.3 Resource Usage

```cpp
// Insert resource (move-only types supported)
world.insert_resource(DeltaTime{0.016f});
world.insert_resource(std::make_unique<RenderContext>());

// Access resource
auto* dt = world.resource<DeltaTime>();

// Check existence
if (world.has_resource<AudioEngine>()) {
    // ...
}

// Remove resource
auto dt = world.remove_resource<DeltaTime>();
```

### 10.4 Resource Characteristics

| Property | Value |
|----------|-------|
| Scope | Global singleton (one per World) |
| Lifetime | Tied to World lifetime |
| Thread-safety | Not thread-safe by default |
| Move-only support | Yes |

---

## 11. Asset Integration

### 11.1 Asset System Overview

```
AssetServer (coordinates loading)
    ├── LoaderRegistry (extension → loader mapping)
    ├── AssetStorage (loaded assets + metadata)
    └── AssetCache (three-tier: hot/warm/cold)
```

### 11.2 Asset Handles

```cpp
template<typename Tag>
struct RenderHandle {
    std::uint64_t id;
    std::uint32_t generation;  // Hot-reload tracking

    bool is_valid() const;
    bool is_stale(std::uint32_t current_gen) const;
};

using ModelHandle = RenderHandle<handle_tags::Model>;
using AssetTextureHandle = RenderHandle<handle_tags::Texture>;
using AssetShaderHandle = RenderHandle<handle_tags::Shader>;
```

### 11.3 ECS + Assets Integration

Components reference assets via handles:

```cpp
struct MeshComponent {
    char builtin_mesh[32];        // Built-in mesh name ("cube", "sphere")
    AssetMeshHandle mesh_handle;  // OR loaded mesh handle
    std::uint32_t submesh_index;

    static MeshComponent builtin(const char* name);
    static MeshComponent from_handle(AssetMeshHandle h, uint32_t submesh);
};

struct MaterialComponent {
    std::array<float, 4> albedo;
    float metallic, roughness, ao;
    AssetTextureHandle albedo_texture;
    AssetTextureHandle normal_texture;
    // ...
};
```

### 11.4 Asset Loading Flow

```
1. load<Texture>("path.png") called
2. Check if already loaded/loading
3. Find extension → find loader
4. Allocate AssetId
5. Register in storage (Loading state)
6. Queue PendingLoad
7. Return Handle (ready immediately)

Per-frame:
8. process() pops pending loads
9. Read file, call loader
10. Store result, update Handle
11. Queue AssetEvent (Loaded/Failed)
```

### 11.5 Hot-Reload Integration

```cpp
class AssetRegistry : public void_core::HotReloadable {
    // HotReloadable interface
    Result<HotReloadSnapshot> snapshot() override;
    Result<void> restore(HotReloadSnapshot) override;

    // Hot-reload polling
    void poll_hot_reload();  // Check file changes
};
```

---

## 12. Render Integration

### 12.1 Render Components

| Component | Purpose |
|-----------|---------|
| `TransformComponent` | Position, rotation, scale |
| `MeshComponent` | Mesh reference (builtin or handle) |
| `MaterialComponent` | PBR material + textures |
| `CameraComponent` | Perspective/orthographic camera |
| `LightComponent` | Directional/point/spot light |
| `RenderableTag` | Visibility and layer mask |
| `HierarchyComponent` | Parent-child for transforms |
| `AnimationComponent` | Animation state |

### 12.2 Render Systems Pipeline

```
Stage: Update
├── ModelLoaderSystem    # Load models, populate handles
├── TransformSystem      # Sync and propagate transforms
└── AnimationSystem      # Update animation state

Stage: PreRender
├── CameraSystem         # Extract camera data
└── LightSystem          # Extract lights

Stage: Render
├── RenderPrepareSystem  # Build draw list from ECS
└── RenderSystem         # Execute draw commands
```

### 12.3 RenderContext Resource

```cpp
class RenderContext {
    RenderAssetManager* m_assets;
    RenderQueue m_render_queue;
    std::vector<LightData> m_lights;
    CameraData m_camera_data;
    Stats m_stats;
};

// Registered as ECS resource
world.insert_resource(std::move(render_context));
```

### 12.4 Draw Command Generation

```cpp
struct DrawCommand {
    GpuMesh* mesh;
    GpuShader* shader;
    std::array<float, 16> model_matrix;
    std::array<float, 9> normal_matrix;
    // Material data...
    std::uint64_t sort_key;
};

void RenderPrepareSystem::run(World& world) {
    auto query = world.query_with<RenderableTag, TransformComponent,
                                   MeshComponent, MaterialComponent>();

    auto& ctx = *world.resource<RenderContext>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* transform = iter.get<TransformComponent>();
        auto* mesh = iter.get<MeshComponent>();
        auto* material = iter.get<MaterialComponent>();

        DrawCommand cmd = build_draw_command(transform, mesh, material);
        ctx.m_render_queue.push(cmd);
    }
}
```

### 12.5 GPU-Ready Structures

All aligned to 16 bytes for GPU buffers:

| Structure | Size | Purpose |
|-----------|------|---------|
| `Vertex` | 80 bytes | Per-vertex data |
| `GpuMaterial` | 256 bytes | PBR material |
| `GpuDirectionalLight` | 112 bytes | Sun light |
| `GpuPointLight` | 48 bytes | Point light |
| `GpuSpotLight` | 144 bytes | Spot light |
| `CameraData` | 256 bytes | Camera matrices |

---

## 13. Scene Integration

### 13.1 Scene Data Structures

```cpp
struct SceneData {
    CameraData camera;
    EnvironmentData environment;
    std::vector<LightData> lights;
    std::vector<EntityData> entities;
};

struct EntityData {
    std::string name;
    TransformData transform;
    std::optional<MeshData> mesh;
    std::optional<MaterialData> material;
    std::optional<AnimationData> animation;
    std::vector<std::string> children;
};
```

### 13.2 SceneInstantiator

Converts `SceneData` → ECS entities:

```cpp
class SceneInstantiator {
    World* m_world;

    // Register all render components with ECS
    void register_components();

    // Instantiate scene data into ECS
    Result<SceneInstance> instantiate(const SceneData& data,
                                       const std::string& path);

    // Individual entity creation
    Entity create_camera_entity(const CameraData&);
    Entity create_light_entity(const LightData&);
    Entity create_game_entity(const EntityData&);
};

struct SceneInstance {
    std::string path;
    std::vector<Entity> entities;  // Tracks created entities
};
```

### 13.3 Scene Loading Flow

```
1. Parse scene.json → SceneData
2. SceneInstantiator.register_components()
3. For each EntityData:
   a. Create ECS entity
   b. Add TransformComponent
   c. Add MeshComponent (if mesh data)
   d. Add MaterialComponent (if material data)
   e. Add AnimationComponent (if animation data)
   f. Track in SceneInstance
4. Build hierarchy from parent-child relationships
5. Return SceneInstance
```

### 13.4 LiveSceneManager

Hot-reload aware scene management:

```cpp
class LiveSceneManager {
    std::vector<SceneInstance> instances_;

    Result<std::size_t> load_scene(const std::string& path);
    void unload_scene(std::size_t index);

    void poll_hot_reload();  // Check for file changes
    void handle_scene_change(const std::string& path);
};
```

---

## 14. Plugin Integration

### 14.1 Plugin Structure

```cpp
class Plugin {
public:
    virtual PluginId id() const = 0;
    virtual Version version() const;
    virtual std::vector<PluginId> dependencies() const;

    // Lifecycle
    virtual Result<void> on_load(PluginContext& ctx) = 0;
    virtual void on_update(float dt);
    virtual Result<PluginState> on_unload(PluginContext& ctx) = 0;
    virtual Result<void> on_reload(PluginContext& ctx, PluginState state);

    // Type registration
    virtual void register_types(TypeRegistry& registry);
    virtual bool supports_hot_reload() const;
};
```

### 14.2 Plugins Contain Systems

Plugins register systems with the ECS:

```cpp
class CombatPlugin : public Plugin {
public:
    Result<void> on_load(PluginContext& ctx) override {
        auto* world = ctx.get<void_ecs::World*>("ecs_world");

        // Register components
        world->register_component<Health>();
        world->register_component<Damage>();

        // Register systems
        world->scheduler().add_system(
            SystemDescriptor("DamageSystem")
                .set_stage(SystemStage::Update),
            [](World& w) {
                // Damage logic
            }
        );

        return Ok();
    }
};
```

### 14.3 Plugin Lifecycle

```
1. PluginRegistry::register_plugin(plugin)
2. Dependency resolution (topological sort)
3. PluginRegistry::load(plugin_id, type_registry)
   - Plugin::on_load(ctx)
   - Status → Active
4. Per-frame: Plugin::on_update(dt)
5. PluginRegistry::unload(plugin_id)
   - Plugin::on_unload(ctx) → PluginState
   - Status → Unloaded
6. Hot-reload:
   - on_unload() → state snapshot
   - Load new code
   - on_reload(ctx, state)
```

---

## 15. Event System

### 15.1 EventBus

```cpp
class EventBus {
    LockFreeQueue<EventEnvelope> m_queue;
    std::map<std::type_index, std::vector<HandlerEntry>> m_handlers;

    // Publish
    template<typename E>
    void publish(E&& event);

    template<typename E>
    void publish_with_priority(E&& event, Priority priority);

    // Subscribe
    template<typename E, typename F>
    SubscriberId subscribe(F&& handler);

    void unsubscribe(SubscriberId id);

    // Process
    void process();            // Process all pending
    void process_batch(std::size_t max_events);
};
```

### 15.2 Event Priority

```cpp
enum class Priority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};
```

Higher priority events are processed first.

### 15.3 ECS + Events Integration

```cpp
// Define event types
struct EntitySpawned { Entity entity; };
struct EntityDespawned { Entity entity; };
struct ComponentAdded { Entity entity; ComponentId component; };

// System publishes events
void SpawnSystem::run(World& world) {
    auto& bus = *world.resource<EventBus>();

    // ... spawn entity ...

    bus.publish(EntitySpawned{entity});
}

// Widget subscribes to events
class EntityList {
    SubscriberId m_sub_id;

    void init(EventBus& bus) {
        m_sub_id = bus.subscribe<EntitySpawned>([this](const auto& e) {
            add_to_list(e.entity);
        });
    }
};
```

---

## 16. Hot-Reload Support

### 16.1 Snapshot Types

```cpp
struct ComponentSnapshot {
    ComponentId id;
    std::string name;
    std::size_t size;
    std::vector<uint8_t> data;
};

struct EntitySnapshot {
    uint64_t entity_bits;
    std::vector<ComponentSnapshot> components;
};

struct WorldSnapshot {
    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t version;
    std::vector<EntitySnapshot> entities;
    std::vector<ComponentMeta> component_registry;
};
```

### 16.2 Snapshot Operations

```cpp
// Take snapshot before code reload
WorldSnapshot snapshot = take_world_snapshot(world);

// Serialize for storage
std::vector<uint8_t> bytes = serialize_snapshot(snapshot);

// After code reload, deserialize and restore
auto snapshot_opt = deserialize_snapshot(bytes);
bool success = apply_world_snapshot(new_world, *snapshot_opt);
```

### 16.3 Hot-Reload Workflow

```
1. File change detected (Kernel watches files)
2. take_world_snapshot(world) → WorldSnapshot
3. Unload old plugin/module code
4. Load new plugin/module code
5. apply_world_snapshot(world, snapshot)
6. Component IDs remapped by name (handles version changes)
7. Systems re-registered
8. Resume execution
```

### 16.4 HotReloadable Interface

```cpp
class HotReloadable {
public:
    virtual Result<HotReloadSnapshot> snapshot() = 0;
    virtual Result<void> restore(HotReloadSnapshot snapshot) = 0;
    virtual bool is_compatible(const Version& new_version) const = 0;
    virtual Version current_version() const = 0;
};
```

---

## 17. Runtime Integration

### 17.1 Runtime Class

The top-level application owner:

```cpp
class Runtime {
    RuntimeConfig m_config;
    RuntimeState m_state;

    std::unique_ptr<Kernel> m_kernel;
    std::unique_ptr<EventBus> m_event_bus;
    std::unique_ptr<World> m_world;

    // Lifecycle
    Result<void> initialize();
    int run();
    void shutdown();

    // World management
    Result<void> load_world(const std::string& world_id);
    void unload_world(bool snapshot = false);
    Result<void> switch_world(const std::string& world_id, bool transfer_state);

    // Subsystem access
    Kernel* kernel() const;
    EventBus* event_bus() const;
    void_ecs::World* ecs_world() const;
};
```

### 17.2 Boot Sequence

```
main()
    ↓
Runtime::initialize()
    ├── init_kernel()           # Stages, hot-reload orchestration
    ├── init_foundation()       # Memory, core structures
    ├── init_infrastructure()   # Event bus, services
    ├── init_api_connectivity() # External API (if configured)
    ├── init_platform()         # Window, input
    ├── init_render()           # GL context, renderer
    ├── init_io()               # Audio, file systems
    └── init_simulation()       # ECS World, physics
    ↓
load_world(initial_world)
    ├── Stream assets
    ├── Instantiate ECS entities
    ├── Activate layers
    ├── Activate plugins
    └── Activate widgets
    ↓
Runtime::run()  # Main loop (blocks)
    ↓
Runtime::shutdown()
    ├── Deactivate widgets
    ├── Deactivate plugins
    ├── Unload world
    └── Shutdown in reverse order
```

### 17.3 Frame Execution

```
Per Frame:
    ├── Input
    ├── HotReloadPoll
    ├── EventDispatch (EventBus::process())
    ├── Update (SystemStage::Update)
    ├── FixedUpdate (physics, fixed timestep)
    ├── PostFixed
    ├── RenderPrepare (build draw list)
    ├── Render (execute draws)
    ├── UI (widgets)
    ├── Audio
    └── Streaming/API sync
```

---

## 18. Data Flow Diagrams

### 18.1 Complete System Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              RUNTIME                                         │
│                                                                              │
│  ┌──────────┐    ┌──────────┐    ┌────────────────┐    ┌──────────────────┐ │
│  │  Kernel  │    │  Event   │    │   void_scene   │    │   Platform       │ │
│  │ (stages) │    │   Bus    │    │     World      │    │ (window/input)   │ │
│  └────┬─────┘    └────┬─────┘    └───────┬────────┘    └────────┬─────────┘ │
│       │               │                  │                      │           │
│       │               │         ┌────────┴────────┐             │           │
│       │               │         │                 │             │           │
│       │               │    void_ecs::World   Layers            │           │
│       │               │    (AUTHORITATIVE)   Plugins           │           │
│       │               │         │            Widgets           │           │
└───────┼───────────────┼─────────┼──────────────────────────────┼───────────┘
        │               │         │                               │
        ▼               ▼         ▼                               ▼
   ┌─────────┐    ┌──────────┐   ┌──────────────────┐      ┌────────────┐
   │ Systems │◄───│  Events  │◄──│    Components    │◄─────│   Input    │
   │(plugins)│    │(typed)   │   │   - Transform    │      │  Events    │
   └────┬────┘    └──────────┘   │   - Mesh         │      └────────────┘
        │                        │   - Material     │
        │                        │   - Light/Camera │
        │                        │   - Renderable   │
        ▼                        └────────┬─────────┘
   ┌─────────┐                            │
   │ Queries │────────────────────────────┘
   │(cached) │
   └────┬────┘
        │
        ▼
┌───────────────────────────────────────────────────────────────────┐
│                         RENDER PIPELINE                            │
│                                                                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐           │
│  │ CameraSystem│───►│LightSystem  │───►│RenderPrepare│           │
│  │ (extract)   │    │ (extract)   │    │ (draw list) │           │
│  └─────────────┘    └─────────────┘    └──────┬──────┘           │
│                                               │                   │
│                                               ▼                   │
│                                        ┌─────────────┐           │
│                                        │RenderSystem │           │
│                                        │ (GL calls)  │           │
│                                        └─────────────┘           │
└───────────────────────────────────────────────────────────────────┘
```

### 18.2 Asset Loading Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                        ASSET SYSTEM                               │
│                                                                   │
│   Application                                                     │
│       │                                                          │
│       ▼                                                          │
│   ┌─────────────┐    ┌──────────────┐    ┌───────────────────┐  │
│   │AssetServer  │───►│LoaderRegistry│───►│ Loader<T>         │  │
│   │load<T>(path)│    │(extension map)│    │(TextureLoader,    │  │
│   └──────┬──────┘    └──────────────┘    │ ModelLoader, etc.)│  │
│          │                               └─────────┬─────────┘  │
│          ▼                                         │             │
│   ┌─────────────┐                                  │             │
│   │AssetStorage │◄─────────────────────────────────┘             │
│   │(loaded data)│                                                │
│   └──────┬──────┘                                                │
│          │                                                       │
│          ▼                                                       │
│   ┌─────────────┐    ┌──────────────────────────────────────┐   │
│   │ Handle<T>   │───►│      ECS Component                    │   │
│   │(generational)│   │  MeshComponent { mesh_handle }        │   │
│   └─────────────┘    │  MaterialComponent { texture_handle } │   │
│                      └──────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

### 18.3 Scene Instantiation Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    SCENE LOADING                                 │
│                                                                  │
│   scene.json                                                     │
│       │                                                         │
│       ▼                                                         │
│   ┌─────────────┐                                               │
│   │SceneParser  │ ─► SceneData (pure data)                      │
│   └──────┬──────┘                                               │
│          │                                                      │
│          ▼                                                      │
│   ┌──────────────────┐                                          │
│   │SceneInstantiator │                                          │
│   └────────┬─────────┘                                          │
│            │                                                    │
│   ┌────────┼────────────────────────────────────┐               │
│   │        │                                    │               │
│   │        ▼                                    ▼               │
│   │  ┌──────────────┐                   ┌──────────────┐        │
│   │  │create_entity │                   │ Build        │        │
│   │  │  + Transform │                   │ Hierarchy    │        │
│   │  │  + Mesh      │                   │ (Parent/     │        │
│   │  │  + Material  │                   │  Children)   │        │
│   │  │  + Animation │                   └──────────────┘        │
│   │  └──────┬───────┘                                           │
│   │         │                                                   │
│   │         ▼                                                   │
│   │  ┌────────────────────────────────────────────────────┐     │
│   │  │              void_ecs::World (AUTHORITATIVE)        │     │
│   │  │                                                     │     │
│   │  │  Archetype{Transform,Mesh,Material}  → [E1,E2,E3]  │     │
│   │  │  Archetype{Transform,Light}          → [E4,E5]     │     │
│   │  │  Archetype{Transform,Camera}         → [E6]        │     │
│   │  │                                                     │     │
│   │  └────────────────────────────────────────────────────┘     │
│   │                                                              │
│   └──────────────────────────────────────────────────────────────┘
│                            │
│                            ▼
│                    ┌──────────────┐
│                    │SceneInstance │
│                    │(tracks all   │
│                    │ entities)    │
│                    └──────────────┘
└──────────────────────────────────────────────────────────────────┘
```

---

## 19. Performance Characteristics

### 19.1 Complexity Analysis

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `world.spawn()` | O(1) amortized | Free list reuse |
| `world.despawn(e)` | O(1) | Swap-remove from archetype |
| `world.add_component<T>(e, c)` | O(C) | C = components in new archetype |
| `world.remove_component<T>(e)` | O(C) | Copy C-1 components |
| `world.get_component<T>(e)` | O(1) | Direct lookup via location |
| Query archetype match | O(A) | A = archetypes (cached) |
| Query iteration | O(E) | E = matching entities |
| System scheduling | O(S²) | S = systems (precomputed) |

### 19.2 Memory Layout

**Entity Storage:**
```
EntityAllocator:
├── generations_: std::vector<Generation>  [G0, G1, G2, ...]
├── free_list_: std::vector<EntityIndex>   [5, 12, 3, ...]
└── alive_count_: uint32_t
```

**Archetype Storage (SoA):**
```
Archetype {Position, Velocity}:
├── entities_: [E0, E1, E2, E3]
├── storages_[0] (Position): [P0, P1, P2, P3]  # Contiguous
└── storages_[1] (Velocity): [V0, V1, V2, V3]  # Contiguous
```

### 19.3 Cache Efficiency

**Good (ECS iteration):**
```cpp
// Sequential memory access
for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
    Position* pos = iter.get<Position>();  // Cache line hit
    Velocity* vel = iter.get<Velocity>();  // Adjacent cache line
    pos->x += vel->x;
}
```

**Bad (OOP iteration):**
```cpp
// Scattered memory access
for (GameObject* obj : objects) {
    obj->position.x += obj->velocity.x;  // Cache miss per object
}
```

### 19.4 Memory Overhead Per Entity

| Component | Overhead |
|-----------|----------|
| Entity slot | 4 bytes (generation) |
| Location | 12 bytes (archetype_id + row) |
| Per-archetype | ~8 bytes (entity array entry) |
| **Total baseline** | **~24 bytes** |

---

## 20. Quick Reference

### 20.1 Common Operations

```cpp
// === ENTITY LIFECYCLE ===
Entity e = world.spawn();
world.despawn(e);
bool alive = world.is_alive(e);

// === COMPONENTS ===
world.register_component<Position>();
world.add_component(e, Position{0, 0, 0});
Position* pos = world.get_component<Position>(e);
bool has = world.has_component<Position>(e);
auto removed = world.remove_component<Position>(e);

// === QUERIES ===
auto query = world.query_with<Position, Velocity>();
for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
    Entity e = iter.entity();
    Position* pos = iter.get<Position>();
}

// === RESOURCES ===
world.insert_resource(DeltaTime{0.016f});
auto* dt = world.resource<DeltaTime>();

// === HIERARCHY ===
set_parent(world, child, parent);
despawn_recursive(world, entity);
propagate_transforms(world);

// === SYSTEMS ===
scheduler.add_system(descriptor, [](World& w) { /* ... */ });
scheduler.run(world);
scheduler.run_stage(world, SystemStage::Update);
```

### 20.2 Namespaces

| Namespace | Purpose |
|-----------|---------|
| `void_ecs` | Entity Component System |
| `void_core` | Core utilities, errors, IDs |
| `void_render` | Rendering |
| `void_asset` | Asset management |
| `void_scene` | Scene loading, world |
| `void_event` | Event bus |
| `void_math` | Math types |
| `void_structures` | Data structures |
| `void_runtime` | Application lifecycle |
| `void_kernel` | Orchestration |

### 20.3 Key Files

| File | Purpose |
|------|---------|
| `ecs/world.hpp` | Main ECS container |
| `ecs/entity.hpp` | Entity handles |
| `ecs/component.hpp` | Component system |
| `ecs/archetype.hpp` | Archetype storage |
| `ecs/query.hpp` | Query system |
| `ecs/system.hpp` | System scheduling |
| `ecs/hierarchy.hpp` | Parent-child transforms |
| `ecs/snapshot.hpp` | Hot-reload support |
| `render/components.hpp` | Render components |
| `render/render_systems.hpp` | Render systems |
| `scene/scene_instantiator.hpp` | Scene → ECS bridge |

### 20.4 System Stages (Execution Order)

```
First       → Initialization
PreUpdate   → Before main logic
Update      → Main game logic (DEFAULT)
PostUpdate  → After main logic
PreRender   → Before rendering
Render      → Actual rendering
PostRender  → After rendering
Last        → Cleanup
```

---

## Appendix A: Component Design Examples

### POD Component (Recommended)

```cpp
struct TransformComponent {
    float position[3] = {0.f, 0.f, 0.f};
    float rotation[4] = {0.f, 0.f, 0.f, 1.f};  // Quaternion
    float scale[3] = {1.f, 1.f, 1.f};

    // Fixed-size arrays only
    // No std::string, std::vector, std::unique_ptr
};
```

### Component with Asset Handle

```cpp
struct MeshComponent {
    char builtin_mesh[32] = {};
    AssetMeshHandle mesh_handle{};
    std::uint32_t submesh_index = 0;

    static MeshComponent builtin(const char* name) {
        MeshComponent c;
        std::strncpy(c.builtin_mesh, name, 31);
        return c;
    }
};
```

### Tag Component (Zero-Size)

```cpp
struct RenderableTag {
    bool visible = true;
    std::uint32_t layer_mask = 0xFFFFFFFF;
    std::int32_t render_order = 0;
};
```

---

## Appendix B: System Implementation Examples

### Query-Based System

```cpp
void MovementSystem(World& world) {
    auto dt = world.resource<DeltaTime>()->value;
    auto query = world.query_with<TransformComponent, VelocityComponent>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* transform = iter.get<TransformComponent>();
        auto* velocity = iter.get<VelocityComponent>();

        transform->position[0] += velocity->x * dt;
        transform->position[1] += velocity->y * dt;
        transform->position[2] += velocity->z * dt;
    }
}
```

### Class-Based System

```cpp
class PhysicsSystem : public System {
    SystemDescriptor m_descriptor;

public:
    PhysicsSystem() {
        m_descriptor = SystemDescriptor("PhysicsSystem")
            .set_stage(SystemStage::FixedUpdate)
            .write_resource<PhysicsWorld>();
    }

    const SystemDescriptor& descriptor() const override {
        return m_descriptor;
    }

    void run(World& world) override {
        auto* physics = world.resource<PhysicsWorld>();
        physics->step(FIXED_TIMESTEP);
    }
};
```

---

*This document provides a complete reference for the void_engine ECS system and its integration with all engine modules. For specific implementation details, refer to the source code in the paths listed above.*
