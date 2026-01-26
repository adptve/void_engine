# Void Engine - Comprehensive Codebase Overview

This document traces the complete flow of the void_engine from initialization through the render loop to shutdown, covering all modules and their integration points.

---

## Table of Contents

1. [Architecture Summary](#1-architecture-summary)
2. [Module Organization](#2-module-organization)
3. [Engine Initialization](#3-engine-initialization)
4. [The Render Loop](#4-the-render-loop)
5. [Module Deep Dives](#5-module-deep-dives)
6. [Data Flow](#6-data-flow)
7. [Shutdown Sequence](#7-shutdown-sequence)

---

## 1. Architecture Summary

void_engine is a C++20 game/render engine built around these core principles:

- **Entity-Component-System (ECS)** as the central data model
- **Hot-reload support** across assets, shaders, and scenes
- **Modular architecture** with 31 distinct modules
- **Service-oriented** with health monitoring and event-driven communication

### Core Stack

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  (User game code, IApp implementation)                       │
├─────────────────────────────────────────────────────────────┤
│                      Engine Layer                            │
│  Engine, LifecycleManager, ConfigManager, Subsystems         │
├─────────────────────────────────────────────────────────────┤
│                      Runtime Layer                           │
│  ECS World, Scene Management, Asset Server, Event Bus        │
├─────────────────────────────────────────────────────────────┤
│                      Simulation Layer                        │
│  Physics, Animation, AI, Combat, Triggers                    │
├─────────────────────────────────────────────────────────────┤
│                      Rendering Layer                         │
│  SceneRenderer, Shaders, Materials, Lighting, Cameras        │
├─────────────────────────────────────────────────────────────┤
│                      Presentation Layer                      │
│  Compositor, Presenter, Frame Scheduling, VRR, HDR           │
├─────────────────────────────────────────────────────────────┤
│                      Platform Layer                          │
│  GLFW, OpenGL, Window Management, Input                      │
└─────────────────────────────────────────────────────────────┘
```

### Namespace Structure

All code resides under the `void_engine` namespace with module-specific sub-namespaces:

```cpp
void_engine::              // Root namespace
├── void_core::            // Logging, errors, hot-reload
├── void_kernel::          // Module loading, supervision
├── void_ecs::             // Entity-Component-System
├── void_scene::           // Scene parsing, instantiation
├── void_render::          // Rendering pipeline
├── void_asset::           // Asset management
├── void_physics::         // Physics simulation
├── void_presenter::       // Display output
├── void_compositor::      // Frame composition
├── void_services::        // Service registry
├── void_audio::           // Sound system
├── void_ui::              // UI framework
└── ...                    // Additional modules
```

---

## 2. Module Organization

### Directory Structure

```
void_engine/
├── include/void_engine/         # Public headers (31 modules)
│   ├── core/                    # Logging, errors, hot-reload
│   ├── kernel/                  # Module loading, supervision
│   ├── engine/                  # Main engine facade
│   ├── ecs/                     # Entity-Component-System
│   ├── scene/                   # Scene management
│   ├── render/                  # Rendering
│   ├── asset/                   # Asset loading
│   ├── physics/                 # Physics simulation
│   ├── presenter/               # Display output
│   ├── compositor/              # Frame composition
│   ├── services/                # Service lifecycle
│   ├── math/                    # Vectors, matrices
│   ├── memory/                  # Custom allocators
│   ├── structures/              # Data structures
│   ├── shader/                  # Shader management
│   ├── ui/                      # ImGui UI
│   ├── hud/                     # HUD elements
│   ├── audio/                   # Sound
│   ├── ai/                      # AI systems
│   ├── event/                   # Event bus
│   ├── ir/                      # Intermediate representation
│   ├── graph/                   # Execution graphs
│   ├── xr/                      # XR/VR support
│   ├── inventory/               # Item management
│   ├── gamestate/               # Game state
│   ├── triggers/                # Trigger volumes
│   └── combat/                  # Combat system
├── src/                         # Implementation files
├── examples/                    # Sample projects
├── tests/                       # Test suites
└── data/                        # Asset data
```

### Module Summary Table

| Module | Purpose | Key Classes |
|--------|---------|-------------|
| **core** | Logging, error handling, hot-reload infrastructure | `Error`, `HotReloadSystem`, `TypeRegistry` |
| **kernel** | Module loading, supervision, sandboxing | `Kernel`, `ModuleLoader`, `Supervisor` |
| **engine** | Main facade, lifecycle, subsystem management | `Engine`, `EngineBuilder`, `LifecycleManager` |
| **ecs** | Entity-Component-System | `World`, `Entity`, `Archetype`, `Query` |
| **scene** | Scene parsing, instantiation, hot-reload | `SceneParser`, `SceneInstantiator`, `LiveSceneManager` |
| **render** | OpenGL rendering, materials, lighting | `SceneRenderer`, `GpuMesh`, `Light`, `Material` |
| **asset** | 3-tier caching, async loading, hot-reload | `AssetServer`, `Handle<T>`, `AssetLoader<T>` |
| **physics** | Rigidbody dynamics, collision, raycasting | `PhysicsWorld`, `IRigidbody`, `CollisionEvent` |
| **presenter** | Frame output, display management | `IPresenter`, `PresenterManager`, `FrameTiming` |
| **compositor** | Display composition, VRR, HDR | `ICompositor`, `FrameScheduler` |
| **services** | Service lifecycle, health, events | `ServiceRegistry`, `ServiceBase`, `EventBus` |
| **math** | Vectors, matrices, quaternions, transforms | `Vec3`, `Mat4`, `Quat`, `Transform` |
| **memory** | Arena, pool, stack, free-list allocators | `Allocator`, `Arena`, `ObjectPool` |
| **structures** | Bitset, slot-map, sparse-set, lock-free | `SlotMap`, `SparseSet`, `LockFreeQueue` |
| **shader** | Shader compilation, hot-reload | `ShaderProgram`, `ShaderRegistry` |
| **ui** | ImGui-based widgets, theming | `UIContext`, `Renderer`, `Theme` |
| **audio** | Sound playback, 3D positioning | `AudioSource`, `Mixer`, `Buffer` |
| **ai** | Behavior trees, navmesh, steering | `BehaviorTree`, `StateMachine`, `NavMesh` |
| **physics** | Rigidbody simulation | `PhysicsWorld`, `RigidBody` |

---

## 3. Engine Initialization

### Entry Point

The engine starts in `src/main.cpp`. The initialization sequence is:

```
main()
  │
  ├─► Load manifest.toml (project configuration)
  │     ├── Application name, version
  │     ├── Scene file path
  │     └── Window configuration
  │
  ├─► Initialize GLFW
  │     ├── glfwInit()
  │     ├── Set OpenGL 3.3 Core profile
  │     └── glfwCreateWindow()
  │
  ├─► Initialize Renderer
  │     └── SceneRenderer::initialize(window)
  │           ├── Load OpenGL functions (glad)
  │           ├── Create built-in meshes (sphere, cube, torus, etc.)
  │           ├── Load PBR shader
  │           └── Load grid shader
  │
  ├─► Initialize Services
  │     ├── Create ServiceRegistry
  │     ├── Create EventBus
  │     └── Setup lifecycle callbacks
  │
  ├─► Initialize Asset Server
  │     ├── Register TextureLoader
  │     ├── Register ModelLoader
  │     └── Enable hot-reload monitoring
  │
  ├─► Initialize ECS World
  │     ├── Pre-allocate entity capacity (1024)
  │     ├── Create EcsSceneBridge
  │     └── Create LiveSceneManager
  │
  ├─► Initialize Physics World
  │     ├── Set gravity (0, -9.81, 0)
  │     ├── Fixed timestep (1/60s)
  │     ├── Max bodies (10000)
  │     ├── Enable CCD
  │     └── Setup collision callbacks
  │
  ├─► Load Initial Scene
  │     ├── Parse scene.toml
  │     ├── Instantiate entities into ECS
  │     └── Sync scene to renderer
  │
  ├─► Initialize Frame Timing
  │     └── Target FPS: 60, history: 120 frames
  │
  ├─► Initialize Compositor
  │     ├── Create platform compositor (or null fallback)
  │     ├── Configure VRR/HDR
  │     └── Setup display outputs
  │
  └─► Enable Hot-Reload Systems
        ├── Shader hot-reload
        └── Scene file watching
```

### Key Initialization Code Paths

| Step | File | Function/Section |
|------|------|------------------|
| Manifest loading | `src/main.cpp` | `main()` lines 80-120 |
| GLFW setup | `src/main.cpp` | `main()` lines 130-180 |
| Renderer init | `src/render/gl_renderer.cpp` | `SceneRenderer::initialize()` |
| ECS setup | `src/main.cpp` | `main()` lines 200-250 |
| Physics init | `src/main.cpp` | `PhysicsWorldBuilder` usage |
| Scene loading | `src/scene/scene_instantiator.cpp` | `SceneInstantiator::instantiate()` |

---

## 4. The Render Loop

### Main Loop Structure

The render loop in `src/main.cpp` follows this pattern:

```cpp
while (!glfwWindowShouldClose(window)) {
    run_once();  // Single frame iteration
}
```

### Per-Frame Execution (run_once)

Each frame executes these phases in order:

```
┌─────────────────────────────────────────────────────────────────┐
│                         FRAME START                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. FRAME TIMING PHASE                                          │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ frame_timing.begin_frame()                          │     │
│     │   → Record frame start time                         │     │
│     │   → Calculate delta_time from previous frame        │     │
│     │   → Update frame history for statistics             │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  2. INPUT PROCESSING PHASE                                      │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ glfwPollEvents()                                    │     │
│     │   → Process window events (resize, close)           │     │
│     │   → Process keyboard/mouse input                    │     │
│     │   → Update input state buffers                      │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  3. EVENT BUS PHASE                                             │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ event_bus.publish(FrameStartEvent{delta_time})      │     │
│     │ event_bus.process_queue()                           │     │
│     │   → Notify all subscribers of frame start           │     │
│     │   → Process queued events from previous frame       │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  4. ASSET SERVER PHASE                                          │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ asset_server.process()                              │     │
│     │   → Check async load completions                    │     │
│     │   → Process hot-reload file changes                 │     │
│     │   → Update asset cache (promote/evict)              │     │
│     │ drain_events()                                      │     │
│     │   → Handle Loaded, Failed, Reloaded events          │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  5. PHYSICS SIMULATION PHASE                                    │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ physics_world->step(delta_time)                     │     │
│     │   → Accumulate time                                 │     │
│     │   → While accumulated >= fixed_timestep:            │     │
│     │       - Integrate velocities                        │     │
│     │       - Detect collisions (broad/narrow phase)      │     │
│     │       - Resolve constraints                         │     │
│     │       - Fire collision/trigger callbacks            │     │
│     │   → Interpolate visual state                        │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  6. SCENE/ECS UPDATE PHASE                                      │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ live_scene_mgr.update(delta_time)                   │     │
│     │   → Check for scene file modifications              │     │
│     │   → Hot-reload changed scenes                       │     │
│     │                                                     │     │
│     │ AnimationSystem::update(ecs_world, delta_time)      │     │
│     │   → Query entities with AnimationComponent          │     │
│     │   → Update animation elapsed time                   │     │
│     │   → Compute keyframe interpolation                  │     │
│     │   → Update TransformComponent                       │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  7. RENDER UPDATE PHASE                                         │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ renderer.update(delta_time)                         │     │
│     │   → Check for shader file modifications             │     │
│     │   → Hot-reload changed shaders                      │     │
│     │   → Sync ECS data to render entities                │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  8. RENDER PHASE                                                │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ renderer.render()                                   │     │
│     │   → Clear framebuffer (color + depth)               │     │
│     │   → Shadow pass (if shadows enabled):               │     │
│     │       - Render depth from light perspective         │     │
│     │   → Geometry pass:                                  │     │
│     │       - Upload light data                           │     │
│     │       - For each render entity:                     │     │
│     │           * Bind PBR shader                         │     │
│     │           * Set uniforms (MVP, material, lights)    │     │
│     │           * Draw GPU mesh (VAO → VBO/EBO)           │     │
│     │   → Post-processing (if enabled):                   │     │
│     │       - Bloom, tone-mapping, color grading          │     │
│     │   → Debug overlays (grid, gizmos)                   │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  9. COMPOSITOR PHASE                                            │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ compositor->dispatch()                              │     │
│     │   → Process display events                          │     │
│     │   → Update VRR state                                │     │
│     │                                                     │     │
│     │ if (compositor->should_render()):                   │     │
│     │   render_target = compositor->begin_frame()         │     │
│     │   compositor->end_frame(render_target)              │     │
│     │                                                     │     │
│     │ compositor->update_content_velocity()               │     │
│     │   → Adjust frame pacing for VRR                     │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  10. PRESENTATION PHASE                                         │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ glfwSwapBuffers(window)                             │     │
│     │   → Present back buffer to display                  │     │
│     │   → VSync synchronization (if enabled)              │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  11. FRAME END PHASE                                            │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ event_bus.publish(FrameEndEvent{frame_count})       │     │
│     │                                                     │     │
│     │ Statistics (every 1 second):                        │     │
│     │   → Log FPS, frame time                             │     │
│     │   → Log draw calls, entity counts                   │     │
│     │   → Log physics stats                               │     │
│     │   → Log service health                              │     │
│     └─────────────────────────────────────────────────────┘     │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│                          FRAME END                               │
└─────────────────────────────────────────────────────────────────┘
```

### Frame Timing Details

The engine uses fixed-timestep physics with variable rendering:

```cpp
// Fixed timestep for physics (deterministic)
const float FIXED_TIMESTEP = 1.0f / 60.0f;  // 60 Hz

// Physics accumulator pattern
while (accumulated_time >= FIXED_TIMESTEP) {
    physics_world->step(FIXED_TIMESTEP);
    accumulated_time -= FIXED_TIMESTEP;
}

// Visual interpolation factor for smooth rendering
float alpha = accumulated_time / FIXED_TIMESTEP;
```

---

## 5. Module Deep Dives

### 5.1 ECS Module (`void_ecs`)

**Location:** `include/void_engine/ecs/`

The ECS is the central data model. All game objects are entities with components.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `World` | Main container for entities, components, and systems |
| `Entity` | 64-bit handle with generation counter |
| `Archetype` | Storage for entities with identical component sets |
| `Query<Ts...>` | Type-safe component query interface |
| `Resources` | Global singleton data (not per-entity) |

**Component Types:**

```cpp
// Core transform (position, rotation, scale)
struct TransformComponent {
    Vec3 position{0.0f};
    Quat rotation{identity};
    Vec3 scale{1.0f};
};

// Mesh reference
struct MeshComponent {
    std::string mesh_id;
    bool visible{true};
    uint32_t layer{0};
};

// PBR material
struct MaterialComponent {
    Vec3 albedo{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float ao{1.0f};
};

// Animation state
struct AnimationComponent {
    std::string name;
    float elapsed{0.0f};
    float duration{1.0f};
    bool looping{true};
    std::vector<Keyframe> keyframes;
};

// Scene ownership tracking
struct SceneTagComponent {
    std::string scene_id;
    size_t instance_id;
};
```

**Query Example:**

```cpp
// Query all entities with Transform and Mesh components
world.query<TransformComponent, MeshComponent>([](Entity e,
    TransformComponent& transform,
    MeshComponent& mesh) {
    // Process entity
});
```

### 5.2 Scene Module (`void_scene`)

**Location:** `include/void_engine/scene/`

Handles scene file parsing and ECS instantiation.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `SceneData` | Parsed scene structure |
| `SceneParser` | TOML/JSON scene file parser |
| `SceneInstantiator` | Creates ECS entities from SceneData |
| `LiveSceneManager` | Hot-reload with file watching |
| `AnimationSystem` | Updates AnimationComponent each frame |
| `EcsSceneBridge` | Connects SceneInstantiator to ECS World |

**Scene File Format (TOML):**

```toml
[scene]
name = "Main Level"

[[cameras]]
name = "main_camera"
position = [0.0, 5.0, 10.0]
target = [0.0, 0.0, 0.0]
fov = 60.0

[[lights]]
type = "directional"
direction = [-0.5, -1.0, -0.5]
color = [1.0, 1.0, 0.9]
intensity = 1.0

[[entities]]
name = "player"
mesh = "sphere"
position = [0.0, 1.0, 0.0]

[entities.material]
albedo = [0.8, 0.2, 0.2]
metallic = 0.0
roughness = 0.5
```

**Scene Loading Flow:**

```
scene.toml
    ↓
SceneParser::parse()
    ↓
SceneData (cameras, lights, entities, materials)
    ↓
SceneInstantiator::instantiate(world, scene_data)
    ↓
ECS Entities with Components
    ↓
SceneRenderer sync
    ↓
GPU Resources (meshes, textures, shaders)
```

### 5.3 Render Module (`void_render`)

**Location:** `include/void_engine/render/`

OpenGL-based rendering with PBR materials.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `SceneRenderer` | Main renderer coordinating all render passes |
| `GpuMesh` | VAO/VBO/EBO wrapper |
| `ShaderProgram` | GLSL program with hot-reload |
| `Material` | PBR material parameters |
| `Light` | Light source data (directional, point, spot) |
| `GlCamera` | Camera with orbit/pan/zoom controls |
| `RenderEntity` | Per-entity render state |

**Render Pipeline:**

```
┌─────────────────────────────────────────┐
│             RENDER PIPELINE             │
├─────────────────────────────────────────┤
│                                         │
│  1. Clear Pass                          │
│     └─ Clear color and depth buffers    │
│                                         │
│  2. Shadow Pass (optional)              │
│     └─ Render depth from light view     │
│     └─ Output: shadow map texture       │
│                                         │
│  3. Geometry Pass                       │
│     ├─ Bind PBR shader                  │
│     ├─ Upload camera matrices           │
│     ├─ Upload light array               │
│     └─ For each RenderEntity:           │
│         ├─ Compute model matrix         │
│         ├─ Set material uniforms        │
│         └─ Draw call (glDrawElements)   │
│                                         │
│  4. Post-Processing (optional)          │
│     ├─ Bloom extraction                 │
│     ├─ Gaussian blur                    │
│     ├─ Tone mapping (HDR → LDR)         │
│     └─ Color grading                    │
│                                         │
│  5. Debug Overlays                      │
│     ├─ Grid rendering                   │
│     ├─ Gizmos (axes, bounds)            │
│     └─ Debug text                       │
│                                         │
└─────────────────────────────────────────┘
```

**PBR Shader Uniforms:**

```glsl
// Vertex uniforms
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normal_matrix;

// Fragment uniforms (material)
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ao;

// Fragment uniforms (lighting)
uniform vec3 u_camera_pos;
uniform int u_light_count;
uniform Light u_lights[MAX_LIGHTS];
```

### 5.4 Asset Module (`void_asset`)

**Location:** `include/void_engine/asset/`

Manages asset loading with 3-tier caching.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `AssetServer` | Central coordinator for all asset operations |
| `Handle<T>` | Type-safe reference-counted asset handle |
| `AssetLoader<T>` | Base class for type-specific loaders |
| `AssetCache` | 3-tier cache (streaming, resident, hot) |

**Cache Tiers:**

```
┌─────────────────────────────────────────────────────────────┐
│                      ASSET CACHE TIERS                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  HOT TIER (Frequently accessed)                             │
│  ├─ In-memory, instant access                               │
│  ├─ Recently used assets                                    │
│  └─ Highest priority for retention                          │
│                        ↑↓                                    │
│  RESIDENT TIER (Loaded but less frequent)                   │
│  ├─ In-memory, fast access                                  │
│  ├─ May be evicted under memory pressure                    │
│  └─ Medium priority                                         │
│                        ↑↓                                    │
│  STREAMING TIER (On-demand)                                 │
│  ├─ Loaded from disk asynchronously                         │
│  ├─ Placeholder returned until ready                        │
│  └─ Lowest priority, first to evict                         │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

**Asset Loading Flow:**

```cpp
// Request an asset (async)
Handle<Texture> texture = asset_server.load<Texture>("textures/brick.png");

// Asset states: Pending → Loading → Loaded (or Failed)

// Check if ready
if (texture.is_loaded()) {
    // Use texture
    Texture* ptr = texture.get();
}

// Or use callback
asset_server.load<Texture>("textures/brick.png", [](Handle<Texture> h) {
    // Called when loaded
});
```

### 5.5 Physics Module (`void_physics`)

**Location:** `include/void_engine/physics/`

Rigidbody dynamics with collision detection.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `PhysicsWorld` | Simulation container |
| `PhysicsWorldBuilder` | Fluent configuration |
| `IRigidbody` | Rigidbody interface |
| `CollisionEvent` | Collision callback data |
| `TriggerEvent` | Trigger volume events |
| `CharacterController` | Player-like capsule collider |

**Physics Configuration:**

```cpp
auto physics_world = PhysicsWorldBuilder()
    .with_gravity({0.0f, -9.81f, 0.0f})
    .with_fixed_timestep(1.0f / 60.0f)
    .with_max_bodies(10000)
    .with_ccd(true)  // Continuous collision detection
    .with_collision_callback([](const CollisionEvent& e) {
        // Handle collision
    })
    .with_hot_reload(true)
    .build();
```

**Physics Step:**

```
physics_world->step(delta_time)
    │
    ├─► Accumulate time
    │
    ├─► While accumulated >= fixed_timestep:
    │       │
    │       ├─► Integrate velocities
    │       │     └─ Apply gravity, forces
    │       │
    │       ├─► Broad phase collision detection
    │       │     └─ AABB overlap tests
    │       │
    │       ├─► Narrow phase collision detection
    │       │     └─ Precise shape intersection
    │       │
    │       ├─► Constraint solving
    │       │     └─ Contact resolution, joints
    │       │
    │       └─► Fire callbacks
    │             └─ CollisionEvent, TriggerEvent
    │
    └─► Interpolate visual state
          └─ Smooth rendering between physics steps
```

### 5.6 Presenter Module (`void_presenter`)

**Location:** `include/void_engine/presenter/`

Manages frame output and display surfaces.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `IPresenter` | Abstract presenter interface |
| `PresenterManager` | Coordinates multiple presenters |
| `FrameTiming` | Frame time tracking and statistics |
| `GlfwPresenter` | GLFW-based implementation |

**Frame Timing:**

```cpp
class FrameTiming {
    std::chrono::steady_clock::time_point m_last_frame;
    std::array<float, 120> m_frame_history;  // Rolling history
    size_t m_frame_count{0};

public:
    float begin_frame();      // Start frame, return delta_time
    float delta_time() const; // Current frame delta
    float fps() const;        // Frames per second
    float avg_frame_time() const; // Average from history
};
```

### 5.7 Compositor Module (`void_compositor`)

**Location:** `include/void_engine/compositor/`

Display composition with VRR and HDR support.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `ICompositor` | Abstract compositor interface |
| `CompositorFactory` | Creates platform-specific compositor |
| `FrameScheduler` | Frame pacing and VRR control |
| `VrrController` | Variable refresh rate management |
| `HdrController` | HDR tone mapping and color spaces |

**Compositor Flow:**

```cpp
// Per-frame compositor usage
compositor->dispatch();  // Process events

if (compositor->should_render()) {
    auto render_target = compositor->begin_frame();
    if (render_target) {
        // Render to target...
        compositor->end_frame(std::move(render_target));
    }
}

compositor->update_content_velocity(velocity);  // VRR hint
```

### 5.8 Services Module (`void_services`)

**Location:** `include/void_engine/services/`

Service lifecycle and event-driven communication.

**Key Classes:**

| Class | Purpose |
|-------|---------|
| `ServiceRegistry` | Central service management |
| `ServiceBase` | Base class for services |
| `EventBus` | Publish/subscribe event system |
| `Channel<T>` | Type-safe event channel |

**Event Bus Usage:**

```cpp
// Define event type
struct FrameStartEvent {
    float delta_time;
};

// Subscribe
event_bus.subscribe<FrameStartEvent>([](const FrameStartEvent& e) {
    // Handle event
});

// Publish
event_bus.publish(FrameStartEvent{delta_time});

// Process queued events
event_bus.process_queue();
```

**Service Lifecycle:**

```
Created → Starting → Running → Stopping → Stopped
              │          │
              └── Degraded ──► (auto-restart)
```

---

## 6. Data Flow

### Complete Frame Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                        ECS WORLD                                 │
│  (Central source of truth for all game state)                    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ Entities with Components:                                │    │
│  │  • TransformComponent (position, rotation, scale)        │    │
│  │  • MeshComponent (mesh ID, visibility, layer)            │    │
│  │  • MaterialComponent (PBR parameters)                    │    │
│  │  • AnimationComponent (keyframes, state)                 │    │
│  │  • CameraComponent, LightComponent                       │    │
│  │  • PhysicsComponent (rigidbody reference)                │    │
│  └─────────────────────────────────────────────────────────┘    │
└───────────────────────────────┬─────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
        ▼                       ▼                       ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│ Physics Sync  │     │ Animation     │     │ Render Sync   │
│               │     │ System        │     │               │
│ • Read physics│     │ • Query anim  │     │ • Query mesh  │
│   body state  │     │   components  │     │   components  │
│ • Write to    │     │ • Interpolate │     │ • Build render│
│   Transform   │     │   keyframes   │     │   entities    │
└───────────────┘     │ • Update      │     │ • Upload GPU  │
                      │   Transform   │     │   data        │
                      └───────────────┘     └───────────────┘
                                │                   │
                                ▼                   ▼
                      ┌─────────────────────────────────────┐
                      │          SCENE RENDERER              │
                      │                                      │
                      │  • Collect render entities           │
                      │  • Sort by material/shader           │
                      │  • Batch draw calls                  │
                      │  • Issue GPU commands                │
                      └──────────────────┬──────────────────┘
                                         │
                                         ▼
                      ┌─────────────────────────────────────┐
                      │           COMPOSITOR                 │
                      │                                      │
                      │  • Frame scheduling                  │
                      │  • VRR/HDR processing                │
                      │  • Display output selection          │
                      └──────────────────┬──────────────────┘
                                         │
                                         ▼
                      ┌─────────────────────────────────────┐
                      │           PRESENTER                  │
                      │                                      │
                      │  • Buffer swap                       │
                      │  • VSync                             │
                      │  • Display                           │
                      └─────────────────────────────────────┘
```

### Asset Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     ASSET LOADING FLOW                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  File System                                                     │
│       │                                                          │
│       ▼                                                          │
│  ┌─────────────────┐                                            │
│  │ AssetServer     │                                            │
│  │                 │                                            │
│  │ • load<T>()     │──────► Returns Handle<T> (pending)         │
│  │ • process()     │        immediately                         │
│  └────────┬────────┘                                            │
│           │                                                      │
│           ▼                                                      │
│  ┌─────────────────┐     ┌─────────────────┐                    │
│  │ AssetLoader<T>  │────►│ Background      │                    │
│  │                 │     │ Loading Thread  │                    │
│  │ • TextureLoader │     │                 │                    │
│  │ • ModelLoader   │     │ • Read file     │                    │
│  │ • ShaderLoader  │     │ • Parse data    │                    │
│  └─────────────────┘     │ • Create GPU    │                    │
│                          │   resource      │                    │
│                          └────────┬────────┘                    │
│                                   │                              │
│                                   ▼                              │
│                          ┌─────────────────┐                    │
│                          │ AssetCache      │                    │
│                          │                 │                    │
│                          │ • Hot tier      │                    │
│                          │ • Resident tier │                    │
│                          │ • Streaming tier│                    │
│                          └────────┬────────┘                    │
│                                   │                              │
│                                   ▼                              │
│                          Handle<T> → Loaded state               │
│                          asset.get() → T* pointer               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. Shutdown Sequence

The engine shuts down in reverse initialization order:

```
request_quit()
    │
    ├─► Stop health monitoring
    │
    ├─► Stop all services
    │     └─ ServiceRegistry::stop_all()
    │
    ├─► Publish final event bus statistics
    │
    ├─► Unload all scenes
    │     ├─ LiveSceneManager::unload_all()
    │     └─ Clear ECS World
    │
    ├─► Clear physics world
    │     └─ PhysicsWorld destructor
    │
    ├─► Garbage collect assets
    │     └─ AssetServer::garbage_collect()
    │
    ├─► Log compositor statistics
    │
    ├─► Shutdown compositor
    │     └─ ICompositor destructor
    │
    ├─► Shutdown renderer
    │     └─ SceneRenderer destructor
    │           ├─ Delete GPU meshes
    │           ├─ Delete shaders
    │           └─ Delete textures
    │
    ├─► Destroy GLFW window
    │     └─ glfwDestroyWindow()
    │
    └─► Terminate GLFW
          └─ glfwTerminate()
```

---

## Appendix: Key File Locations

| Component | Header | Implementation |
|-----------|--------|----------------|
| Main entry | - | `src/main.cpp` |
| Engine | `include/void_engine/engine/engine.hpp` | `src/engine/engine.cpp` |
| ECS World | `include/void_engine/ecs/world.hpp` | `src/ecs/world.cpp` |
| Scene Parser | `include/void_engine/scene/scene_parser.hpp` | `src/scene/scene_parser.cpp` |
| Scene Instantiator | `include/void_engine/scene/scene_instantiator.hpp` | `src/scene/scene_instantiator.cpp` |
| Renderer | `include/void_engine/render/gl_renderer.hpp` | `src/render/gl_renderer.cpp` |
| Asset Server | `include/void_engine/asset/server.hpp` | `src/asset/server.cpp` |
| Physics | `include/void_engine/physics/physics.hpp` | `src/physics/physics.cpp` |
| Presenter | `include/void_engine/presenter/presenter.hpp` | `src/presenter/presenter.cpp` |
| Compositor | `include/void_engine/compositor/compositor.hpp` | `src/compositor/compositor.cpp` |
| Services | `include/void_engine/services/registry.hpp` | `src/services/registry.cpp` |
| Event Bus | `include/void_engine/services/event_bus.hpp` | `src/services/event_bus.cpp` |

---

## Design Patterns

| Pattern | Usage |
|---------|-------|
| **Builder** | `EngineBuilder`, `PhysicsWorldBuilder`, `EngineConfig` |
| **Factory** | `CompositorFactory`, `AssetLoader` registry |
| **Observer** | Event bus publish/subscribe |
| **Component** | ECS archetype-based storage |
| **Template Method** | `AnimationSystem`, update phases |
| **Strategy** | `IPresenter`, `ICompositor` backends |
| **RAII** | Smart pointers, `GlobalEngineGuard` |
| **Handle/Body** | `Handle<T>` for asset references |

---

*Document generated for void_engine codebase analysis*
