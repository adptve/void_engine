# Void Engine Legacy Crates - Comprehensive Codebase Overview

This document traces the complete flow of the Rust-based legacy crates from initialization through the render loop to shutdown, covering all crates and their integration points.

---

## Table of Contents

1. [Architecture Summary](#1-architecture-summary)
2. [Crate Organization](#2-crate-organization)
3. [Engine Initialization](#3-engine-initialization)
4. [The Render Loop](#4-the-render-loop)
5. [Crate Deep Dives](#5-crate-deep-dives)
6. [Data Flow](#6-data-flow)
7. [Shutdown Sequence](#7-shutdown-sequence)

---

## 1. Architecture Summary

The legacy crates form a Rust-based game engine with these core principles:

- **Plugin Architecture** - Everything is a plugin, extensible via traits
- **IR-Based State Updates** - Patches batched into atomic transactions
- **Archetype ECS** - Cache-friendly entity storage
- **Multi-Platform Rendering** - Desktop, Web, XR backends
- **Hot-Reload Support** - Assets and configurations

### Core Stack

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  (User game code, EnginePlugin implementations)              │
├─────────────────────────────────────────────────────────────┤
│                      Engine Layer                            │
│  void_engine: Engine, EnginePlugin, EngineContext           │
├─────────────────────────────────────────────────────────────┤
│                      Orchestration Layer                     │
│  void_kernel: Kernel, Supervisor, Watchdog, AppManager       │
├─────────────────────────────────────────────────────────────┤
│                      State Management Layer                  │
│  void_ir: Patch, Transaction, PatchBus, Namespace            │
│  void_ecs: World, Entity, Component, System, Archetype       │
│  void_event: EventBus, Event, Priority                       │
├─────────────────────────────────────────────────────────────┤
│                      Simulation Layer                        │
│  void_physics: PhysicsWorld, RigidBody, Collider             │
│  void_ai, void_combat, void_triggers, void_gamestate         │
├─────────────────────────────────────────────────────────────┤
│                      Rendering Layer                         │
│  void_render: RenderGraph, Layer, Compositor                 │
│  void_shader, void_ui, void_hud                              │
├─────────────────────────────────────────────────────────────┤
│                      Presentation Layer                      │
│  void_presenter: Presenter, DesktopPresenter, WebPresenter   │
│  void_compositor: Wayland/DRM compositor                     │
│  void_xr: XrSystem, OpenXR/WebXR backends                    │
├─────────────────────────────────────────────────────────────┤
│                      Foundation Layer                        │
│  void_core, void_math, void_memory, void_structures          │
└─────────────────────────────────────────────────────────────┘
```

### Technology Stack

| Aspect | Technology |
|--------|------------|
| Language | Rust 2021 Edition |
| Build | Cargo workspace |
| Physics | Rapier 3D |
| Wayland | Smithay |
| Locking | parking_lot |
| Serialization | serde |
| Error Handling | thiserror |

---

## 2. Crate Organization

### Directory Structure

```
legacy/crates/ (33 crates)
│
├── Core Infrastructure (7 crates)
│   ├── void_core/          # Zero-dependency primitives
│   ├── void_math/          # SIMD math (Vec, Mat, Quat)
│   ├── void_memory/        # Allocators (Arena, Pool, FreeList)
│   ├── void_structures/    # Lock-free data structures
│   ├── void_event/         # Lock-free event bus
│   ├── void_kernel/        # Frame orchestration
│   └── void_ir/            # Intermediate representation
│
├── Engine Core (5 crates)
│   ├── void_engine/        # Main engine orchestration
│   ├── void_ecs/           # Entity-Component-System
│   ├── void_asset/         # Asset management
│   ├── void_render/        # Render graph & compositor
│   └── void_presenter/     # Platform output adapters
│
├── XR & Display (2 crates)
│   ├── void_xr/            # XR/AR/VR abstraction
│   └── void_compositor/    # Wayland compositor (Linux)
│
├── Gameplay Systems (7 crates)
│   ├── void_physics/       # Rapier 3D physics
│   ├── void_audio/         # Audio system
│   ├── void_combat/        # Combat mechanics
│   ├── void_ai/            # Behavior trees, AI
│   ├── void_gamestate/     # Game state management
│   ├── void_triggers/      # Trigger volumes
│   └── void_inventory/     # Inventory system
│
├── Tools & UI (8 crates)
│   ├── void_shell/         # Command shell (vsh)
│   ├── void_ui/            # UI system
│   ├── void_hud/           # HUD rendering
│   ├── void_editor/        # Built-in editor
│   ├── void_graph/         # Visual scripting
│   ├── void_shader/        # Shader system
│   ├── void_script/        # Scripting integration
│   └── void_scripting/     # Script runtime
│
└── Services & Integration (4 crates)
    ├── void_services/      # Service registry
    ├── void_runtime/       # Runtime stub
    ├── void_asset_server/  # Asset loaders
    └── void_cpp/           # C++ FFI
```

### Crate Summary Table

| Crate | Purpose | Key Types |
|-------|---------|-----------|
| **void_core** | Zero-dep primitives | `Plugin`, `Handle`, `Id`, `TypeRegistry` |
| **void_math** | SIMD math library | `Vec2/3/4`, `Mat3/4`, `Quat`, `Transform` |
| **void_memory** | Custom allocators | `Arena`, `Pool`, `FreeList`, `Stack` |
| **void_structures** | Lock-free structures | `SlotMap`, `SparseSet`, `LockFreeQueue` |
| **void_event** | Event bus | `EventBus`, `Event`, `Priority`, `SubscriberId` |
| **void_kernel** | Frame orchestration | `Kernel`, `FrameContext`, `Supervisor`, `Watchdog` |
| **void_ir** | Patch system | `Patch`, `Transaction`, `PatchBus`, `Namespace` |
| **void_engine** | Main orchestration | `Engine`, `EnginePlugin`, `EngineContext`, `FrameTime` |
| **void_ecs** | Entity-Component-System | `World`, `Entity`, `Component`, `System`, `Archetype` |
| **void_asset** | Asset loading | `AssetServer`, `Handle<T>`, `AssetLoader`, `LoadState` |
| **void_render** | Render graph | `RenderGraph`, `Layer`, `Compositor`, `PassId` |
| **void_presenter** | Platform output | `Presenter`, `DesktopPresenter`, `WebPresenter` |
| **void_compositor** | Display server | `Compositor` (Wayland/DRM) |
| **void_xr** | XR abstraction | `XrSystem`, `XrMode`, `View`, `Pose`, `HandState` |
| **void_physics** | Physics simulation | `PhysicsWorld`, `RigidBodyComponent`, `ColliderComponent` |
| **void_graph** | Visual scripting | `Graph`, `Node`, `NodeExecutor`, `Value` |
| **void_services** | Service registry | `ServiceRegistry`, `AssetService`, `SessionService` |
| **void_ai** | AI systems | `BehaviorTree`, `StateMachine`, `NavMesh` |
| **void_audio** | Sound | `AudioSource`, `Mixer`, `Buffer` |

---

## 3. Engine Initialization

### Entry Point

The engine starts via the builder pattern:

```rust
let engine = Engine::builder()
    .with_config(EngineConfig {
        app_name: "MyGame".into(),
        target_fps: 60,
        fixed_timestep: 1.0 / 60.0,
        hot_reload: true,
        ..Default::default()
    })
    .with_plugin(RenderPlugin::new())
    .with_plugin(PhysicsPlugin::new())
    .with_plugin(GamePlugin::new())
    .build();

engine.run();  // Blocking main loop
```

### Initialization Sequence

```
Engine::builder().build()
    │
    ├─► Create EngineConfig
    │     ├── app_name, version
    │     ├── target_fps, fixed_timestep
    │     └── hot_reload flags
    │
    ├─► Initialize void_core
    │     ├── TypeRegistry
    │     └── Handle system
    │
    ├─► Initialize void_memory
    │     └── Default allocators
    │
    ├─► Initialize void_event::EventBus
    │     └── Lock-free event queue
    │
    ├─► Initialize void_ecs::World
    │     ├── Archetype storage
    │     └── System scheduler
    │
    ├─► Initialize void_ir::PatchBus
    │     └── Transaction batching
    │
    ├─► Initialize void_kernel::Kernel
    │     ├── LayerManager
    │     ├── AppManager
    │     ├── Supervisor
    │     └── Watchdog
    │
    ├─► Initialize void_asset::AssetServer
    │     ├── Register loaders (gltf, texture, shader, scene)
    │     └── Enable hot-reload
    │
    ├─► Initialize void_render::Compositor
    │     ├── Layer management
    │     └── Renderer assignment
    │
    ├─► Initialize void_presenter::Presenter
    │     └── Platform-specific (Desktop/Web/XR)
    │
    ├─► Initialize void_xr::XrSystem (if XR enabled)
    │     ├── Backend selection (OpenXR/WebXR)
    │     └── Mode configuration
    │
    ├─► Register plugins (user-provided)
    │     └── Vec<Box<dyn EnginePlugin>>
    │
    └─► Call plugin.on_init() for each plugin
          └─ One-time setup

Engine::run()
    │
    └─► Call plugin.on_start() for each plugin
          └─ Runtime initialization
```

### Engine Lifecycle States

```
Created → Initializing → Running ⟷ Paused → ShuttingDown → Stopped
                            │
                            └─► Frame loop executing
```

---

## 4. The Render Loop

### Main Loop Structure

```rust
// Engine::run() - blocking main loop
pub fn run(&mut self) {
    self.start();  // on_start() callbacks

    while self.state == EngineState::Running {
        self.update();   // Logic phase
        self.render();   // Render phase
    }

    self.stop();     // on_stop() callbacks
    self.shutdown(); // on_shutdown() callbacks
}
```

### Per-Frame Execution

```
┌─────────────────────────────────────────────────────────────────┐
│                         FRAME START                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. TIMING UPDATE                                               │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ Update FrameTime                                    │     │
│     │   → delta_time (since last frame)                   │     │
│     │   → total_time (since start)                        │     │
│     │   → frame_count++                                   │     │
│     │   → Calculate actual FPS                            │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  2. KERNEL BEGIN FRAME                                          │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ kernel.begin_frame(delta_time)                      │     │
│     │   → Create FrameContext                             │     │
│     │   → Initialize IR transaction batch                 │     │
│     │   → Reset per-frame state                           │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  3. PRE-UPDATE PHASE                                            │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ for plugin in plugins:                              │     │
│     │     plugin.on_pre_update(&mut context)              │     │
│     │   → Setup for this frame                            │     │
│     │   → Input processing                                │     │
│     │   → Pre-simulation preparation                      │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  4. FIXED UPDATE PHASE (accumulator loop)                       │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ accumulated += delta_time                           │     │
│     │ while accumulated >= fixed_timestep:                │     │
│     │     for plugin in plugins:                          │     │
│     │         plugin.on_fixed_update(&mut context)        │     │
│     │       → Physics simulation step                     │     │
│     │       → Deterministic game logic                    │     │
│     │     accumulated -= fixed_timestep                   │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  5. UPDATE PHASE                                                │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ for plugin in plugins:                              │     │
│     │     plugin.on_update(&mut context)                  │     │
│     │   → Game logic updates                              │     │
│     │   → Emit IR patches via kernel.patch_bus()          │     │
│     │   → Animation updates                               │     │
│     │   → AI decisions                                    │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  6. IR TRANSACTION PROCESSING                                   │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ kernel.process_transactions(&mut world)             │     │
│     │   → Collect all patches from this frame             │     │
│     │   → Validate transactions                           │     │
│     │   → Apply atomically to ECS World                   │     │
│     │   → Update render layers                            │     │
│     │   → Notify subscribers of changes                   │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  7. POST-UPDATE PHASE                                           │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ for plugin in plugins:                              │     │
│     │     plugin.on_post_update(&mut context)             │     │
│     │   → Cleanup                                         │     │
│     │   → Late updates (camera follow, etc.)              │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  8. EVENT PROCESSING                                            │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ events.process()                                    │     │
│     │   → Sort by priority                                │     │
│     │   → Dispatch to subscribers                         │     │
│     │   → Clear processed events                          │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  9. ECS SYSTEMS                                                 │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ world.run_systems()                                 │     │
│     │   → Execute registered systems                      │     │
│     │   → Query-based component iteration                 │     │
│     └─────────────────────────────────────────────────────┘     │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│                      RENDER PHASE                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  10. BUILD RENDER GRAPH                                         │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ kernel.build_render_graph()                         │     │
│     │   → Collect visible layers                          │     │
│     │   → Sort by priority                                │     │
│     │   → Select backend (GPU/DRM/Web)                    │     │
│     │   → Build dependency graph                          │     │
│     │   → Return RenderGraph { layers, backend }          │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  11. PRE-RENDER PHASE                                           │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ for plugin in plugins:                              │     │
│     │     plugin.on_pre_render(&mut context)              │     │
│     │   → Prepare render resources                        │     │
│     │   → Upload GPU data                                 │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  12. RENDER PHASE                                               │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ for plugin in plugins:                              │     │
│     │     plugin.on_render(&mut context)                  │     │
│     │   → Fill render passes                              │     │
│     │   → Submit draw commands                            │     │
│     │                                                     │     │
│     │ compositor.execute(render_graph)                    │     │
│     │   → Execute passes per layer                        │     │
│     │   → Apply post-processing                           │     │
│     │   → Blend layers                                    │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  13. POST-RENDER PHASE                                          │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ for plugin in plugins:                              │     │
│     │     plugin.on_post_render(&mut context)             │     │
│     │   → UI overlay                                      │     │
│     │   → Debug rendering                                 │     │
│     │   → Screenshot capture                              │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  14. PRESENTATION                                               │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ presenter.present(frame)                            │     │
│     │   → Swap buffers / submit to display                │     │
│     │   → VSync synchronization                           │     │
│     └─────────────────────────────────────────────────────┘     │
│                              ↓                                   │
│  15. KERNEL END FRAME                                           │
│     ┌─────────────────────────────────────────────────────┐     │
│     │ kernel.end_frame()                                  │     │
│     │   → Garbage collect old transactions                │     │
│     │   → Reset frame-local state                         │     │
│     │   → Update statistics                               │     │
│     └─────────────────────────────────────────────────────┘     │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│                          FRAME END                               │
└─────────────────────────────────────────────────────────────────┘
```

### Plugin Callback Order

```
on_init()           # Once at engine creation
on_start()          # Once at run() start
│
├─► on_pre_update()     # Every frame
├─► on_fixed_update()   # Fixed timestep (may repeat)
├─► on_update()         # Every frame
├─► on_post_update()    # Every frame
├─► on_pre_render()     # Every frame
├─► on_render()         # Every frame
└─► on_post_render()    # Every frame
│
on_stop()           # Once at run() end
on_shutdown()       # Once at engine destruction
```

---

## 5. Crate Deep Dives

### 5.1 void_engine

**Location:** `legacy/crates/void_engine/`

The main orchestration crate.

**Key Types:**

```rust
pub struct Engine {
    config: EngineConfig,
    state: EngineState,
    time: FrameTime,
    plugins: Vec<Box<dyn EnginePlugin>>,
    // Sub-systems accessed via EngineContext
}

pub struct EngineConfig {
    pub app_name: String,
    pub app_version: Version,
    pub target_fps: u32,
    pub fixed_timestep: f32,
    pub hot_reload: bool,
    pub xr_enabled: bool,
}

pub struct FrameTime {
    pub delta: f32,       // Seconds since last frame
    pub total: f64,       // Total seconds since start
    pub frame: u64,       // Frame counter
    pub target_fps: u32,
    pub actual_fps: f32,
}

pub trait EnginePlugin: Send + Sync {
    fn on_init(&mut self, ctx: &mut EngineContext);
    fn on_start(&mut self, ctx: &mut EngineContext);
    fn on_pre_update(&mut self, ctx: &mut EngineContext);
    fn on_fixed_update(&mut self, ctx: &mut EngineContext);
    fn on_update(&mut self, ctx: &mut EngineContext);
    fn on_post_update(&mut self, ctx: &mut EngineContext);
    fn on_pre_render(&mut self, ctx: &mut EngineContext);
    fn on_render(&mut self, ctx: &mut EngineContext);
    fn on_post_render(&mut self, ctx: &mut EngineContext);
    fn on_stop(&mut self, ctx: &mut EngineContext);
    fn on_shutdown(&mut self, ctx: &mut EngineContext);
}

pub struct EngineContext<'a> {
    pub config: &'a EngineConfig,
    pub time: &'a FrameTime,
    pub world: &'a mut World,
    pub events: &'a mut EventBus,
    pub assets: &'a mut AssetServer,
    pub compositor: &'a mut Compositor,
    pub xr: Option<&'a mut XrSystem>,
    pub kernel: &'a mut Kernel,
    // ... more sub-systems
}
```

### 5.2 void_kernel

**Location:** `legacy/crates/void_kernel/`

Frame orchestration and app supervision.

**Key Types:**

```rust
pub struct Kernel {
    patch_bus: PatchBus,
    layer_manager: LayerManager,
    app_manager: AppManager,
    supervisor: Supervisor,
    watchdog: Watchdog,
    recovery_manager: RecoveryManager,
}

pub struct FrameContext {
    pub frame: u64,
    pub delta_time: f32,
    pub total_time: f64,
    pub state: FrameState,
}

pub struct RenderGraph {
    pub frame: u64,
    pub layers: Vec<LayerId>,
    pub backend: Backend,
}

impl Kernel {
    pub fn begin_frame(&mut self, delta: f32) -> FrameContext;
    pub fn process_transactions(&mut self, world: &mut World);
    pub fn build_render_graph(&self) -> RenderGraph;
    pub fn end_frame(&mut self);
}
```

**Supervisor Features:**
- App isolation (sandboxed execution)
- Fault tolerance (crash recovery)
- Watchdog (health monitoring, timeouts)
- Recovery manager (state snapshots)

### 5.3 void_ir

**Location:** `legacy/crates/void_ir/`

Intermediate representation for state updates.

**Key Types:**

```rust
pub struct Patch {
    pub namespace: Namespace,
    pub op: PatchOp,
    pub target: PatchTarget,
    pub data: PatchData,
}

pub enum PatchOp {
    Create,
    Update,
    Delete,
    Transform,
}

pub struct Transaction {
    pub id: TransactionId,
    pub patches: Vec<Patch>,
    pub timestamp: Instant,
}

pub struct PatchBus {
    pending: Vec<Patch>,
    transactions: Vec<Transaction>,
}

impl PatchBus {
    pub fn emit(&mut self, patch: Patch);
    pub fn commit(&mut self) -> Transaction;
    pub fn apply(&mut self, world: &mut World, transaction: Transaction);
}
```

**Namespaces:**
- `ecs` - Entity/component changes
- `render` - Layer/material changes
- `asset` - Asset state changes
- `physics` - Physics body changes

### 5.4 void_ecs

**Location:** `legacy/crates/void_ecs/`

Archetype-based Entity-Component-System.

**Key Types:**

```rust
pub struct World {
    entities: EntityStorage,
    archetypes: ArchetypeStorage,
    systems: SystemScheduler,
    resources: Resources,
}

pub struct Entity {
    id: u32,
    generation: u32,
}

pub trait Component: Send + Sync + 'static {}

pub struct QueryDescriptor {
    pub includes: Vec<TypeId>,
    pub excludes: Vec<TypeId>,
}

pub trait System: Send + Sync {
    fn run(&mut self, query: QueryState, world: &World);
}

impl World {
    pub fn spawn(&mut self) -> Entity;
    pub fn despawn(&mut self, entity: Entity);
    pub fn add_component<T: Component>(&mut self, entity: Entity, component: T);
    pub fn remove_component<T: Component>(&mut self, entity: Entity);
    pub fn get<T: Component>(&self, entity: Entity) -> Option<&T>;
    pub fn get_mut<T: Component>(&mut self, entity: Entity) -> Option<&mut T>;
    pub fn query(&self, descriptor: QueryDescriptor) -> QueryIter;
    pub fn run_systems(&mut self);
}
```

### 5.5 void_render

**Location:** `legacy/crates/void_render/`

Render graph and compositor.

**Key Types:**

```rust
pub struct Compositor {
    layers: Vec<Layer>,
    renderers: HashMap<LayerId, Box<dyn CompositorRenderer>>,
}

pub struct Layer {
    pub id: LayerId,
    pub priority: i32,
    pub visible: bool,
    pub blend_mode: BlendMode,
    pub post_processing: Vec<PostProcess>,
}

pub struct RenderGraph {
    pub passes: Vec<RenderPass>,
    pub dependencies: Vec<(PassId, PassId)>,
}

pub trait CompositorRenderer: Send + Sync {
    fn setup(&mut self, layer: &Layer);
    fn execute(&mut self, pass: &RenderPass, frame: &mut Frame);
    fn cleanup(&mut self);
}

impl Compositor {
    pub fn create_layer(&mut self, config: LayerConfig) -> LayerId;
    pub fn assign_renderer(&mut self, layer: LayerId, renderer: Box<dyn CompositorRenderer>);
    pub fn begin_frame(&mut self) -> Frame;
    pub fn build_graph(&self) -> RenderGraph;
    pub fn execute(&mut self, graph: RenderGraph, frame: &mut Frame);
    pub fn end_frame(&mut self, frame: Frame);
}
```

### 5.6 void_presenter

**Location:** `legacy/crates/void_presenter/`

Platform-specific output adapters.

**Key Types:**

```rust
pub trait Presenter: Send + Sync {
    fn begin_frame(&mut self) -> Frame;
    fn present(&mut self, frame: Frame);
    fn reconfigure(&mut self, config: SurfaceConfig);
    fn resize(&mut self, width: u32, height: u32);
}

pub struct DesktopPresenter {
    // winit + wgpu
}

pub struct WebPresenter {
    // WebGPU / Canvas
}

pub struct XrPresenter {
    // OpenXR HMD output
}

pub struct Frame {
    pub texture: SurfaceTexture,
    pub view: TextureView,
    pub encoder: CommandEncoder,
}
```

### 5.7 void_xr

**Location:** `legacy/crates/void_xr/`

XR/VR/AR abstraction layer.

**Key Types:**

```rust
pub struct XrSystem {
    backend: Box<dyn XrBackend>,
    mode: XrMode,
    session: Option<XrSession>,
}

pub enum XrMode {
    Desktop,
    Vr,
    Ar,
    Mr,
    Spectator,
}

pub struct View {
    pub eye: Eye,
    pub pose: Pose,
    pub fov: Fov,
    pub resolution: (u32, u32),
}

pub struct Pose {
    pub position: Vec3,
    pub orientation: Quat,
}

pub struct HandState {
    pub tracked: bool,
    pub joints: [JointPose; 26],
    pub pinch_strength: f32,
    pub grip_strength: f32,
}

pub trait XrBackend: Send + Sync {
    fn initialize(&mut self) -> Result<()>;
    fn begin_frame(&mut self) -> Vec<View>;
    fn end_frame(&mut self, layers: Vec<CompositionLayer>);
    fn get_hand_state(&self, hand: Hand) -> Option<HandState>;
}
```

### 5.8 void_physics

**Location:** `legacy/crates/void_physics/`

Rapier 3D physics integration.

**Key Types:**

```rust
pub struct PhysicsWorld {
    pipeline: PhysicsPipeline,
    rigid_bodies: RigidBodySet,
    colliders: ColliderSet,
    joints: JointSet,
    gravity: Vec3,
}

pub struct RigidBodyComponent {
    pub body_type: RigidBodyType,
    pub mass: f32,
    pub linear_damping: f32,
    pub angular_damping: f32,
}

pub struct ColliderComponent {
    pub shape: ColliderShape,
    pub material: PhysicsMaterial,
    pub sensor: bool,
}

pub struct PhysicsMaterial {
    pub friction: f32,
    pub restitution: f32,
}

impl PhysicsWorld {
    pub fn step(&mut self, dt: f32);
    pub fn raycast(&self, ray: Ray, max_dist: f32) -> Option<RaycastHit>;
    pub fn add_body(&mut self, entity: Entity, body: RigidBodyComponent);
    pub fn add_collider(&mut self, entity: Entity, collider: ColliderComponent);
}
```

### 5.9 void_graph

**Location:** `legacy/crates/void_graph/`

Blueprint-compatible visual scripting.

**Key Types:**

```rust
pub struct Graph {
    pub nodes: Vec<Node>,
    pub edges: Vec<Edge>,
}

pub struct Node {
    pub id: NodeId,
    pub node_type: NodeType,
    pub inputs: Vec<Pin>,
    pub outputs: Vec<Pin>,
    pub position: Vec2,
}

pub enum NodeType {
    // Flow control
    Branch, Sequence, ForLoop, WhileLoop,
    // Math
    Add, Subtract, Multiply, Divide,
    // Logic
    And, Or, Not, Compare,
    // Events
    OnTick, OnInput, OnCollision,
    // Custom
    Custom(String),
}

pub struct GraphExecutor {
    graph: Graph,
    context: ExecutionContext,
}

impl GraphExecutor {
    pub fn execute(&mut self, entry: NodeId) -> Value;
}
```

### 5.10 void_event

**Location:** `legacy/crates/void_event/`

Lock-free event bus.

**Key Types:**

```rust
pub struct EventBus {
    channels: HashMap<TypeId, Channel>,
    subscribers: HashMap<TypeId, Vec<Subscriber>>,
}

pub struct Event {
    pub type_id: TypeId,
    pub priority: Priority,
    pub data: Box<dyn Any + Send>,
}

pub enum Priority {
    Immediate,  // Process this frame
    High,
    Normal,
    Low,
    Deferred,   // Process next frame
}

impl EventBus {
    pub fn publish<E: 'static + Send>(&mut self, event: E);
    pub fn subscribe<E: 'static>(&mut self, handler: impl Fn(&E) + Send + Sync);
    pub fn process(&mut self);  // Dispatch all pending events
}
```

### 5.11 void_asset

**Location:** `legacy/crates/void_asset/`

Hot-reloadable asset management.

**Key Types:**

```rust
pub struct AssetServer {
    loaders: HashMap<TypeId, Box<dyn AssetLoader>>,
    assets: HashMap<AssetId, AssetEntry>,
    pending: Vec<LoadRequest>,
}

pub struct Handle<T> {
    id: AssetId,
    _marker: PhantomData<T>,
}

pub enum LoadState {
    NotLoaded,
    Loading,
    Loaded,
    Failed(String),
}

pub trait AssetLoader: Send + Sync {
    type Asset: Send + Sync + 'static;
    fn load(&self, path: &Path) -> Result<Self::Asset>;
    fn extensions(&self) -> &[&str];
}

impl AssetServer {
    pub fn load<T: 'static>(&mut self, path: &str) -> Handle<T>;
    pub fn get<T: 'static>(&self, handle: &Handle<T>) -> Option<&T>;
    pub fn state<T: 'static>(&self, handle: &Handle<T>) -> LoadState;
    pub fn reload(&mut self, path: &str);
}
```

---

## 6. Data Flow

### Complete Frame Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     PLUGIN UPDATES                               │
│  (on_pre_update, on_fixed_update, on_update, on_post_update)    │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      IR PATCH BUS                                │
│  Plugins emit patches: Create, Update, Delete, Transform        │
│  Patches batched by namespace (ecs, render, asset, physics)     │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│              KERNEL TRANSACTION PROCESSING                       │
│  kernel.process_transactions(&mut world)                         │
│  - Validate all patches                                          │
│  - Apply atomically to World                                     │
│  - Notify change subscribers                                     │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                    ┌───────────┴───────────┐
                    │                       │
                    ▼                       ▼
┌───────────────────────────┐   ┌───────────────────────────┐
│       ECS WORLD           │   │      EVENT BUS            │
│                           │   │                           │
│ - Entities updated        │   │ - Change events fired     │
│ - Components modified     │   │ - Subscribers notified    │
│ - Archetypes reorganized  │   │ - Priority-sorted dispatch│
└───────────────┬───────────┘   └───────────────────────────┘
                │
                ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ECS SYSTEMS                                   │
│  world.run_systems()                                             │
│  - Query entities by component signature                         │
│  - Transform updates, AI, etc.                                   │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                   RENDER GRAPH BUILD                             │
│  kernel.build_render_graph()                                     │
│  - Collect visible layers                                        │
│  - Sort by priority                                              │
│  - Build pass dependencies                                       │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                     COMPOSITOR                                   │
│  compositor.execute(render_graph, frame)                         │
│  - Execute passes per layer                                      │
│  - Apply post-processing                                         │
│  - Blend layers together                                         │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      PRESENTER                                   │
│  presenter.present(frame)                                        │
│  - Platform-specific output                                      │
│  - VSync / display timing                                        │
└─────────────────────────────────────────────────────────────────┘
```

### Asset Loading Flow

```
AssetServer::load<T>(path)
         │
         ▼
┌─────────────────────┐
│ Create Handle<T>    │
│ (immediate return)  │
│ state = Loading     │
└─────────┬───────────┘
          │
          ▼ (async)
┌─────────────────────┐
│ Find AssetLoader<T> │
│ by file extension   │
└─────────┬───────────┘
          │
          ▼
┌─────────────────────┐
│ loader.load(path)   │
│ - Read file         │
│ - Parse/decode      │
│ - Create asset      │
└─────────┬───────────┘
          │
          ├─► Success: state = Loaded, asset stored
          │
          └─► Failure: state = Failed(error)

Handle<T>::get() → Option<&T>
          │
          └─► Returns Some(&asset) if Loaded
              Returns None if Loading/Failed
```

---

## 7. Shutdown Sequence

```
Engine::stop()
    │
    └─► for plugin in plugins.iter().rev():
            plugin.on_stop(&mut context)

Engine::shutdown()
    │
    ├─► for plugin in plugins.iter().rev():
    │       plugin.on_shutdown(&mut context)
    │
    ├─► Drop XrSystem (if present)
    │       └─ End XR session
    │
    ├─► Drop Compositor
    │       └─ Release render resources
    │
    ├─► Drop Presenter
    │       └─ Close window/display
    │
    ├─► Drop AssetServer
    │       └─ Unload all assets
    │
    ├─► Drop World
    │       └─ Despawn all entities
    │
    ├─► Drop EventBus
    │       └─ Clear all subscribers
    │
    ├─► Drop Kernel
    │       └─ Stop supervisor/watchdog
    │
    └─► state = EngineState::Stopped
```

---

## Appendix: Key File Locations

| Crate | Main File |
|-------|-----------|
| void_engine | `legacy/crates/void_engine/src/lib.rs` |
| void_kernel | `legacy/crates/void_kernel/src/lib.rs` |
| void_ir | `legacy/crates/void_ir/src/lib.rs` |
| void_ecs | `legacy/crates/void_ecs/src/lib.rs` |
| void_render | `legacy/crates/void_render/src/lib.rs` |
| void_presenter | `legacy/crates/void_presenter/src/lib.rs` |
| void_compositor | `legacy/crates/void_compositor/src/lib.rs` |
| void_xr | `legacy/crates/void_xr/src/lib.rs` |
| void_physics | `legacy/crates/void_physics/src/lib.rs` |
| void_asset | `legacy/crates/void_asset/src/lib.rs` |
| void_event | `legacy/crates/void_event/src/lib.rs` |
| void_graph | `legacy/crates/void_graph/src/lib.rs` |
| void_math | `legacy/crates/void_math/src/lib.rs` |
| void_memory | `legacy/crates/void_memory/src/lib.rs` |
| void_structures | `legacy/crates/void_structures/src/lib.rs` |
| void_core | `legacy/crates/void_core/src/lib.rs` |

---

## Design Patterns

| Pattern | Usage |
|---------|-------|
| **Builder** | `Engine::builder()`, configuration |
| **Plugin** | `EnginePlugin` trait for extensibility |
| **Observer** | Event bus publish/subscribe |
| **ECS** | Archetype-based entity storage |
| **Command** | IR patches as commands |
| **Transaction** | Atomic state updates |
| **Handle/Body** | `Handle<T>` for asset references |
| **Strategy** | `Presenter`, `XrBackend` traits |
| **Supervisor** | Fault-tolerant app management |

---

*Document generated for void_engine legacy crates analysis*
