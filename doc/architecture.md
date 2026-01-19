# void_engine Architecture

## Overview

void_engine is a production-grade metaverse operating system kernel. The architecture implements:

- **Never-dying kernel**: Crash containment prevents failures from affecting the system
- **Hot-swappable components**: All modules can be replaced at runtime without restart
- **Declarative state via IR patches**: Applications emit patches rather than mutating state directly
- **Capability-based security**: seL4-inspired unforgeable permission tokens
- **Erlang-style supervision**: Automatic restart with backoff for failed components

## System Layers

```
┌─────────────────────────────────────────────────────────┐
│                     Entry Layer                          │
│                    (void_runtime)                        │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────┐
│                  Interface Layer                         │
│              (void_shell, void_editor)                   │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────┐
│                Orchestration Layer                       │
│              (void_kernel, void_engine)                  │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────┐
│                Communication Layer                       │
│               (void_ir, void_event)                      │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────┐
│                   Storage Layer                          │
│               (void_ecs, void_asset)                     │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────┐
│                  Rendering Layer                         │
│    (void_render, void_shader, void_ui, void_presenter)   │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────┐
│                  Foundation Layer                        │
│   (void_core, void_math, void_memory, void_structures)   │
└─────────────────────────────────────────────────────────┘
```

## Module Index

### Foundation Layer (Zero Dependencies)

| Module | Purpose | C++ Equivalent |
|--------|---------|----------------|
| `void_math` | Math primitives (vectors, matrices, quaternions) | GLM integration |
| `void_memory` | Custom allocators (arena, pool, stack, free list) | pmr allocators |
| `void_structures` | Lock-free data structures (sparse set, slot map, queue) | Custom containers |
| `void_core` | Plugin system, type registry, hot-reload | RTTI + DLL loading |
| `void_event` | Event bus for decoupled communication | Signal/slot pattern |

### Storage Layer

| Module | Purpose | C++ Approach |
|--------|---------|--------------|
| `void_ecs` | Archetype-based Entity Component System | EnTT-style or custom |
| `void_asset` | Hot-reloadable asset management | Handle-based resources |
| `void_asset_server` | File watching and hot-reload server | notify + async loading |

### Communication Layer

| Module | Purpose | C++ Approach |
|--------|---------|--------------|
| `void_ir` | IR patches and transactions | Command pattern |
| `void_event` | Event bus system | Observer pattern |

### Rendering Layer

| Module | Purpose | C++ Approach |
|--------|---------|--------------|
| `void_render` | Render graph abstraction | Frame graph pattern |
| `void_shader` | Shader compilation (Naga → SPIRV/GLSL) | Shader reflection |
| `void_ui` | Immediate-mode UI | ImGui or custom |
| `void_presenter` | Platform output adapters | Vulkan/DX12/Metal |
| `void_compositor` | Layer composition | Window compositor |

### Orchestration Layer

| Module | Purpose | C++ Approach |
|--------|---------|--------------|
| `void_kernel` | Never-dying kernel, supervision tree | Exception handling + restart |
| `void_engine` | Application orchestration | Application framework |

### Gameplay Systems

| Module | Purpose | C++ Approach |
|--------|---------|--------------|
| `void_physics` | Physics simulation | Rapier/Jolt integration |
| `void_combat` | Health, damage, weapons | Component systems |
| `void_inventory` | Items and equipment | Data-driven items |
| `void_ai` | AI and pathfinding | Behavior trees, navmesh |
| `void_audio` | Spatial audio | OpenAL/FMOD |
| `void_triggers` | Trigger volumes and events | Collision callbacks |
| `void_gamestate` | Save/load, game variables | Serialization |
| `void_hud` | HUD and UI elements | UI layer |

## Core Patterns

### Plugin System

All engine components are plugins with lifecycle management:

```cpp
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual bool on_load(PluginContext& ctx) = 0;
    virtual void on_unload() = 0;
    virtual void on_config(PluginConfig const& cfg) = 0;
    virtual char const* name() const = 0;
    virtual Version version() const = 0;
};

enum class PluginStatus {
    Registered,
    Loading,
    Active,
    Unloading,
    Disabled,
    Failed
};
```

### Hot-Reload State Flow

```
Snapshot → Serialize → Unload Old → Load New → Deserialize → Restore
                                                      │
                                               ┌──────┴──────┐
                                               ▼             ▼
                                          [Success]     [Failure]
                                               │             │
                                               │      ┌──────┴──────┐
                                               │      │  Rollback   │
                                               │      │ to Snapshot │
                                               ▼      └─────────────┘
                                        [Frame Continues]
```

### IR Patch System

Applications never mutate state directly. They emit declarative patches:

```cpp
enum class PatchKind {
    Entity,     // Create/destroy/enable/disable entities
    Component,  // Set/update/remove components
    Layer,      // Create/update/destroy render layers
    Asset       // Load/unload/update assets
};

struct Patch {
    PatchKind kind;
    NamespaceId namespace_id;
    std::variant<EntityPatch, ComponentPatch, LayerPatch, AssetPatch> data;
};

struct Transaction {
    std::vector<Patch> patches;
    NamespaceId source;
    std::string description;
};
```

### Entity-Component System

Archetype-based storage for cache-efficient iteration:

```cpp
struct EntityId {
    uint32_t index;
    uint32_t generation;  // Use-after-free prevention
};

class World {
public:
    EntityId create();
    void destroy(EntityId id);
    bool is_valid(EntityId id) const;

    template<typename T>
    T* get_component(EntityId id);

    template<typename T>
    T& add_component(EntityId id, T&& component);

    template<typename... Components>
    auto query() -> Query<Components...>;
};
```

### Supervision Tree

Erlang-style fault tolerance:

```cpp
enum class RestartStrategy {
    OneForOne,   // Restart only failed child
    OneForAll,   // Restart all children when one fails
    RestForOne   // Restart failed + all started after
};

enum class ChildType {
    Permanent,   // Always restart
    Temporary,   // Never restart
    Transient    // Restart only on abnormal exit
};

struct RestartIntensity {
    uint32_t max_restarts = 5;
    uint32_t window_secs = 60;
    uint64_t initial_delay_ms = 100;
    uint64_t max_delay_ms = 30000;
    float multiplier = 2.0f;
};
```

### Capability Security

seL4-inspired permission system:

```cpp
enum class CapabilityKind {
    CreateEntities,
    DestroyEntities,
    ModifyComponents,
    CreateLayers,
    LoadAssets,
    AccessNetwork,
    AccessFilesystem,
    CrossNamespaceRead,
    ExecuteScripts,
    HotSwap,           // Admin only
    ManageCapabilities, // Admin only
    KernelAdmin        // Admin only
};

struct Capability {
    CapabilityId id;          // Unforgeable token
    CapabilityKind kind;
    NamespaceId holder;
    NamespaceId grantor;
    std::optional<TimePoint> expires_at;
    bool delegable;
};
```

## Namespace Isolation

Each application operates in complete isolation:

```
┌─────────────────────────────────────────┐
│ Kernel Namespace (0)                     │
│ - Full access to all resources          │
└─────────────────────────────────────────┘
         │ full access    │ full access
         ▼                ▼
┌─────────────────┐    ┌─────────────────┐
│ App A (ns:1)    │    │ App B (ns:2)    │
│ - Own entities  │    │ - Own entities  │
│ - Own layers    │    │ - Own layers    │
│ - Own resources │    │ - Own resources │
└─────────────────┘    └─────────────────┘
         │                │
         └───── X ────────┘
            Cannot access each other
```

## Resource Budgets

Apps are sandboxed with strict resource limits:

```cpp
struct ResourceBudget {
    uint64_t max_memory_bytes = 256 * 1024 * 1024;    // 256 MB
    uint64_t max_gpu_memory_bytes = 512 * 1024 * 1024; // 512 MB
    uint32_t max_entities = 10000;
    uint32_t max_layers = 8;
    uint32_t max_assets = 1000;
    uint64_t max_frame_time_us = 16000;  // ~16ms
    uint32_t max_patches_per_frame = 1000;
    uint32_t max_draw_calls = 1000;
};
```

## Game Loop

Fixed timestep with interpolation:

```cpp
void Engine::run() {
    constexpr double fixed_dt = 1.0 / 60.0;
    double accumulator = 0.0;
    auto previous = Clock::now();

    while (m_running) {
        auto current = Clock::now();
        double frame_time = duration_cast<seconds>(current - previous).count();
        previous = current;

        frame_time = std::min(frame_time, 0.25);  // Prevent spiral of death
        accumulator += frame_time;

        process_input();

        while (accumulator >= fixed_dt) {
            fixed_update(fixed_dt);  // Physics, gameplay
            accumulator -= fixed_dt;
        }

        double alpha = accumulator / fixed_dt;
        render(alpha);  // Interpolate for smooth visuals
    }
}
```

## Rendering Architecture

Multi-layer composition with crash isolation:

```
App A                    App B
  │                        │
  ├─ Layer: content        ├─ Layer: world
  ├─ Layer: effects        ├─ Layer: effects
  └─ Layer: ui             └─ Layer: ui
        │                        │
        └────────┬───────────────┘
                 ▼
         Layer Compositor
                 │
    ┌────────────┼────────────┐
    ▼            ▼            ▼
 Desktop       DRM        OpenXR
 (wgpu)    (Direct GPU)   (VR/AR)
```

## Build Configuration

Target C++20 with CMake:

```cmake
cmake_minimum_required(VERSION 3.20)
project(void_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Modules structure
add_subdirectory(src/core)
add_subdirectory(src/math)
add_subdirectory(src/memory)
add_subdirectory(src/structures)
add_subdirectory(src/ecs)
add_subdirectory(src/event)
add_subdirectory(src/ir)
add_subdirectory(src/asset)
add_subdirectory(src/render)
add_subdirectory(src/shader)
add_subdirectory(src/kernel)
add_subdirectory(src/engine)
add_subdirectory(src/runtime)
```

## External Dependencies (Planned)

| Category | Library | Purpose |
|----------|---------|---------|
| Math | GLM | Vector/matrix math |
| Graphics | Vulkan/wgpu | GPU abstraction |
| Windowing | SDL3/GLFW | Cross-platform windows |
| Serialization | nlohmann/json, toml++ | Config/asset parsing |
| Physics | Jolt/Rapier | Physics simulation |
| Audio | OpenAL/miniaudio | Spatial audio |
| UI | Dear ImGui | Debug/editor UI |
| Testing | Catch2 | Unit testing |
