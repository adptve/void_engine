# Void Engine Legacy Crates - Render Loop High-Level Architecture

This document provides a focused, high-level architecture view of the render loop cycle in the Rust-based legacy crates, covering all crates that participate in each frame.

---

## Render Loop Overview

The render loop executes continuously via `Engine::run()`. Each frame has two major phases: **Update** (logic) and **Render** (graphics). The IR patch system ensures atomic state transitions.

```
┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│   ┌──────────────┐                                                │
│   │ FRAME START  │                                                │
│   └──────┬───────┘                                                │
│          │                                                         │
│          ▼                                                         │
│   ┌──────────────────────────────────────────────────────┐        │
│   │              UPDATE PHASE (Logic)                     │        │
│   │                                                       │        │
│   │  TIMING → PRE_UPDATE → FIXED_UPDATE → UPDATE         │        │
│   │     ↓                                                 │        │
│   │  IR_TRANSACTIONS → POST_UPDATE → EVENTS → SYSTEMS    │        │
│   └──────────────────────────────────────────────────────┘        │
│          │                                                         │
│          ▼                                                         │
│   ┌──────────────────────────────────────────────────────┐        │
│   │              RENDER PHASE (Graphics)                  │        │
│   │                                                       │        │
│   │  BUILD_GRAPH → PRE_RENDER → RENDER → POST_RENDER     │        │
│   │     ↓                                                 │        │
│   │  COMPOSITOR → PRESENTER                               │        │
│   └──────────────────────────────────────────────────────┘        │
│          │                                                         │
│          ▼                                                         │
│   ┌──────────────┐                                                │
│   │  FRAME END   │                                                │
│   └──────────────┘                                                │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## Phase Breakdown

### Phase 1: Timing Update

**Crate:** `void_engine`

```
┌─────────────────────────────────────────┐
│            TIMING UPDATE                 │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Previous frame timestamp       │
│           Target FPS configuration       │
│                                          │
│  PROCESS: • Calculate delta_time         │
│           • Update total_time            │
│           • Increment frame_count        │
│           • Calculate actual_fps         │
│                                          │
│  OUTPUT:  FrameTime {                    │
│             delta: f32,                  │
│             total: f64,                  │
│             frame: u64,                  │
│             actual_fps: f32             │
│           }                              │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 2: Kernel Begin Frame

**Crate:** `void_kernel`

```
┌─────────────────────────────────────────┐
│          KERNEL BEGIN FRAME              │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   delta_time                     │
│                                          │
│  PROCESS: • Create FrameContext          │
│           • Initialize transaction batch │
│           • Reset per-frame state        │
│           • Start watchdog timer         │
│                                          │
│  OUTPUT:  FrameContext {                 │
│             frame: u64,                  │
│             delta_time: f32,             │
│             total_time: f64,             │
│             state: FrameState::Update   │
│           }                              │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 3: Pre-Update

**Crate:** `void_engine` (plugin callbacks)

```
┌─────────────────────────────────────────┐
│             PRE-UPDATE                   │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   EngineContext (full access)    │
│                                          │
│  PROCESS: for plugin in plugins:         │
│             plugin.on_pre_update(ctx)    │
│                                          │
│           Typical operations:            │
│           • Process input events         │
│           • Update input state           │
│           • Prepare for simulation       │
│           • Check hot-reload triggers    │
│                                          │
│  OUTPUT:  Input state ready              │
│           Pre-simulation setup done      │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 4: Fixed Update (Accumulator Loop)

**Crates:** `void_engine`, `void_physics`

```
┌─────────────────────────────────────────┐
│           FIXED UPDATE                   │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   delta_time                     │
│           fixed_timestep (e.g., 1/60s)   │
│           accumulated time               │
│                                          │
│  PROCESS: accumulated += delta_time      │
│                                          │
│           while accumulated >= fixed_dt: │
│               for plugin in plugins:     │
│                   plugin.on_fixed_update │
│                                          │
│               Physics:                   │
│               • physics_world.step(dt)   │
│               • Integrate velocities     │
│               • Collision detection      │
│               • Constraint solving       │
│               • Fire collision events    │
│                                          │
│               accumulated -= fixed_dt    │
│                                          │
│  OUTPUT:  Physics state updated          │
│           Collision events queued        │
│           Deterministic simulation       │
│                                          │
└─────────────────────────────────────────┘
```

**Fixed Timestep Pattern:**
```
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   Variable Frame Rate          Fixed Physics Rate        │
│   ─────────────────────        ─────────────────         │
│                                                          │
│   Frame 1: dt=0.018s  ───►  1 physics step (0.0167s)    │
│   Frame 2: dt=0.015s  ───►  0 physics steps (accum)     │
│   Frame 3: dt=0.020s  ───►  2 physics steps             │
│   Frame 4: dt=0.016s  ───►  1 physics step              │
│                                                          │
│   Accumulated time carries over between frames           │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

### Phase 5: Update

**Crates:** `void_engine`, `void_ir`

```
┌─────────────────────────────────────────┐
│              UPDATE                      │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   EngineContext                  │
│           PatchBus (for IR emission)     │
│                                          │
│  PROCESS: for plugin in plugins:         │
│             plugin.on_update(ctx)        │
│                                          │
│           Typical operations:            │
│           • Game logic                   │
│           • AI decisions                 │
│           • Animation updates            │
│           • State machine transitions    │
│           • Emit IR patches              │
│                                          │
│           Patches emitted via:           │
│           ctx.kernel.patch_bus.emit(     │
│               Patch {                    │
│                   namespace: Namespace,  │
│                   op: PatchOp,           │
│                   target: PatchTarget,   │
│                   data: PatchData,       │
│               }                          │
│           )                              │
│                                          │
│  OUTPUT:  Patches queued in PatchBus     │
│           Game state changes pending     │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 6: IR Transaction Processing

**Crates:** `void_kernel`, `void_ir`, `void_ecs`

```
┌─────────────────────────────────────────┐
│       IR TRANSACTION PROCESSING          │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   PatchBus with pending patches  │
│           ECS World                      │
│                                          │
│  PROCESS: kernel.process_transactions()  │
│                                          │
│           1. Collect patches by namespace│
│              ┌─────────────────────┐     │
│              │ ecs: [p1, p2, p3]   │     │
│              │ render: [p4, p5]    │     │
│              │ asset: [p6]         │     │
│              │ physics: [p7, p8]   │     │
│              └─────────────────────┘     │
│                                          │
│           2. Validate transactions       │
│              • Check conflicts           │
│              • Verify targets exist      │
│                                          │
│           3. Apply atomically            │
│              • Spawn/despawn entities    │
│              • Add/remove components     │
│              • Update component data     │
│              • Modify render layers      │
│                                          │
│           4. Notify change subscribers   │
│              • Entity created events     │
│              • Component changed events  │
│                                          │
│  OUTPUT:  World state updated atomically │
│           Change events published        │
│                                          │
└─────────────────────────────────────────┘
```

**Patch Operations:**
```
┌────────────────────────────────────────────────────────────┐
│                                                            │
│  PatchOp::Create    │  Spawn entity, add component        │
│  PatchOp::Update    │  Modify component data              │
│  PatchOp::Delete    │  Despawn entity, remove component   │
│  PatchOp::Transform │  Batch transform update             │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

### Phase 7: Post-Update

**Crate:** `void_engine`

```
┌─────────────────────────────────────────┐
│            POST-UPDATE                   │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   EngineContext (state updated)  │
│                                          │
│  PROCESS: for plugin in plugins:         │
│             plugin.on_post_update(ctx)   │
│                                          │
│           Typical operations:            │
│           • Camera follow/smooth         │
│           • Late constraint resolution   │
│           • Cleanup temporary state      │
│           • Prepare for rendering        │
│                                          │
│  OUTPUT:  Final update state ready       │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 8: Event Processing

**Crate:** `void_event`

```
┌─────────────────────────────────────────┐
│          EVENT PROCESSING                │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   EventBus with pending events   │
│                                          │
│  PROCESS: events.process()               │
│                                          │
│           1. Sort by priority            │
│              ┌─────────────────────┐     │
│              │ Immediate  (first)  │     │
│              │ High                │     │
│              │ Normal              │     │
│              │ Low                 │     │
│              │ Deferred   (last)   │     │
│              └─────────────────────┘     │
│                                          │
│           2. Dispatch to subscribers     │
│              • Type-safe callbacks       │
│              • Lock-free delivery        │
│                                          │
│           3. Clear processed events      │
│                                          │
│  OUTPUT:  All handlers invoked           │
│           Event queue cleared            │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 9: ECS Systems

**Crate:** `void_ecs`

```
┌─────────────────────────────────────────┐
│            ECS SYSTEMS                   │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   World with updated state       │
│                                          │
│  PROCESS: world.run_systems()            │
│                                          │
│           For each registered System:    │
│           1. Build query from descriptor │
│              QueryDescriptor {           │
│                includes: [A, B],         │
│                excludes: [C],            │
│              }                           │
│                                          │
│           2. Iterate matching entities   │
│              for (entity, a, b) in query │
│                                          │
│           3. Execute system logic        │
│                                          │
│  OUTPUT:  Components processed           │
│           System side-effects applied    │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 10: Build Render Graph

**Crate:** `void_kernel`, `void_render`

```
┌─────────────────────────────────────────┐
│         BUILD RENDER GRAPH               │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Layer configuration            │
│           Backend selection              │
│                                          │
│  PROCESS: kernel.build_render_graph()    │
│                                          │
│           1. Collect visible layers      │
│              • Filter by visibility      │
│              • Check render conditions   │
│                                          │
│           2. Sort by priority            │
│              ┌─────────────────────┐     │
│              │ Layer 0: Background │     │
│              │ Layer 1: World      │     │
│              │ Layer 2: Effects    │     │
│              │ Layer 3: UI         │     │
│              │ Layer 4: Debug      │     │
│              └─────────────────────┘     │
│                                          │
│           3. Build pass dependencies     │
│              • Shadow passes first       │
│              • Geometry passes           │
│              • Post-processing last      │
│                                          │
│           4. Select backend              │
│              • GPU (wgpu/Vulkan/GL)      │
│              • DRM (direct rendering)    │
│              • Web (WebGPU/Canvas)       │
│                                          │
│  OUTPUT:  RenderGraph {                  │
│             frame: u64,                  │
│             layers: Vec<LayerId>,        │
│             backend: Backend,            │
│           }                              │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 11: Pre-Render

**Crate:** `void_engine`

```
┌─────────────────────────────────────────┐
│             PRE-RENDER                   │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   EngineContext                  │
│           RenderGraph                    │
│                                          │
│  PROCESS: for plugin in plugins:         │
│             plugin.on_pre_render(ctx)    │
│                                          │
│           Typical operations:            │
│           • Upload GPU buffers           │
│           • Update uniform data          │
│           • Prepare render resources     │
│           • Cull invisible objects       │
│                                          │
│  OUTPUT:  GPU resources ready            │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 12: Render

**Crates:** `void_engine`, `void_render`

```
┌─────────────────────────────────────────┐
│              RENDER                      │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   RenderGraph                    │
│           Compositor                     │
│           Frame (from presenter)         │
│                                          │
│  PROCESS:                                │
│                                          │
│   ┌─────────────────────────────────┐   │
│   │ 1. PLUGIN RENDER CALLBACKS      │   │
│   │    for plugin in plugins:       │   │
│   │        plugin.on_render(ctx)    │   │
│   │      → Fill render commands     │   │
│   │      → Submit draw calls        │   │
│   └─────────────────────────────────┘   │
│               │                          │
│               ▼                          │
│   ┌─────────────────────────────────┐   │
│   │ 2. COMPOSITOR EXECUTION         │   │
│   │    compositor.execute(graph)    │   │
│   │                                 │   │
│   │    For each layer:              │   │
│   │    ├─ Setup layer state         │   │
│   │    ├─ renderer.execute(pass)    │   │
│   │    └─ Apply blend mode          │   │
│   │                                 │   │
│   │    Post-processing:             │   │
│   │    ├─ Bloom                     │   │
│   │    ├─ Tone mapping              │   │
│   │    ├─ Color grading             │   │
│   │    └─ FXAA/TAA                  │   │
│   └─────────────────────────────────┘   │
│               │                          │
│               ▼                          │
│   ┌─────────────────────────────────┐   │
│   │ 3. LAYER COMPOSITING            │   │
│   │    Blend all layers together    │   │
│   │    Apply final output transform │   │
│   └─────────────────────────────────┘   │
│                                          │
│  OUTPUT:  Composed frame ready           │
│                                          │
└─────────────────────────────────────────┘
```

**Layer Blending:**
```
┌────────────────────────────────────────────────────────────┐
│                                                            │
│  Background (priority 0) ──┐                               │
│                            ├──► Blend ──┐                  │
│  World      (priority 1) ──┘            │                  │
│                                         ├──► Blend ──┐     │
│  Effects    (priority 2) ───────────────┘            │     │
│                                                      ├──►  │
│  UI         (priority 3) ────────────────────────────┘     │
│                                                            │
│                                            Final Frame     │
└────────────────────────────────────────────────────────────┘
```

---

### Phase 13: Post-Render

**Crate:** `void_engine`

```
┌─────────────────────────────────────────┐
│            POST-RENDER                   │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   EngineContext                  │
│           Composed frame                 │
│                                          │
│  PROCESS: for plugin in plugins:         │
│             plugin.on_post_render(ctx)   │
│                                          │
│           Typical operations:            │
│           • UI overlay rendering         │
│           • Debug visualization          │
│           • Screenshot capture           │
│           • Performance HUD              │
│                                          │
│  OUTPUT:  Final frame with overlays      │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 14: Presentation

**Crates:** `void_presenter`, `void_xr`

```
┌─────────────────────────────────────────┐
│            PRESENTATION                  │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Frame (texture, encoder)       │
│                                          │
│  PROCESS: presenter.present(frame)       │
│                                          │
│           Platform-specific:             │
│                                          │
│           Desktop (DesktopPresenter):    │
│           • Submit command buffer        │
│           • Swap buffers (wgpu)          │
│           • VSync synchronization        │
│                                          │
│           Web (WebPresenter):            │
│           • Submit to WebGPU queue       │
│           • requestAnimationFrame sync   │
│                                          │
│           XR (XrPresenter):              │
│           • Submit per-eye views         │
│           • OpenXR frame submission      │
│           • Compositor blending (HMD)    │
│                                          │
│  OUTPUT:  Frame visible on display       │
│                                          │
└─────────────────────────────────────────┘
```

**XR Stereo Rendering:**
```
┌────────────────────────────────────────────────────────────┐
│                                                            │
│   Left Eye View                    Right Eye View          │
│   ┌─────────────┐                  ┌─────────────┐         │
│   │             │                  │             │         │
│   │  Render     │                  │  Render     │         │
│   │  Scene      │                  │  Scene      │         │
│   │             │                  │             │         │
│   └──────┬──────┘                  └──────┬──────┘         │
│          │                                │                │
│          └────────────┬───────────────────┘                │
│                       │                                    │
│                       ▼                                    │
│            ┌─────────────────────┐                         │
│            │  XR Compositor      │                         │
│            │  (OpenXR runtime)   │                         │
│            └─────────────────────┘                         │
│                       │                                    │
│                       ▼                                    │
│                    HMD Display                             │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

### Phase 15: Kernel End Frame

**Crate:** `void_kernel`

```
┌─────────────────────────────────────────┐
│          KERNEL END FRAME                │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   FrameContext                   │
│           Transaction history            │
│                                          │
│  PROCESS: kernel.end_frame()             │
│                                          │
│           • Garbage collect old txns     │
│           • Reset frame-local state      │
│           • Update frame statistics      │
│           • Check watchdog health        │
│           • Log performance metrics      │
│                                          │
│  OUTPUT:  Ready for next frame           │
│                                          │
└─────────────────────────────────────────┘
```

---

## Crate Integration Diagram

```
                    ┌─────────────────┐
                    │   FRAME START   │
                    └────────┬────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │   void_engine       │
                  │   (FrameTime)       │
                  └──────────┬──────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │   void_kernel       │
                  │   (begin_frame)     │
                  └──────────┬──────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│ void_physics  │  │ void_engine   │  │ void_ai       │
│ (fixed_update)│  │ (update)      │  │ (decisions)   │
└───────┬───────┘  └───────┬───────┘  └───────┬───────┘
        │                  │                  │
        └──────────────────┼──────────────────┘
                           │
                           ▼
                  ┌─────────────────────┐
                  │     void_ir         │
                  │  (patch emission)   │
                  └──────────┬──────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │   void_kernel       │
                  │ (process_txns)      │
                  └──────────┬──────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │    void_ecs         │
                  │  (World updated)    │
                  └──────────┬──────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
   ┌─────────────────────┐       ┌─────────────────────┐
   │   void_event        │       │    void_ecs         │
   │   (process)         │       │   (run_systems)     │
   └─────────────────────┘       └──────────┬──────────┘
                                            │
                                            ▼
                                 ┌─────────────────────┐
                                 │   void_kernel       │
                                 │ (build_render_graph)│
                                 └──────────┬──────────┘
                                            │
                                            ▼
                                 ┌─────────────────────┐
                                 │   void_render       │
                                 │   (Compositor)      │
                                 └──────────┬──────────┘
                                            │
                             ┌──────────────┴──────────────┐
                             │                             │
                             ▼                             ▼
                  ┌─────────────────────┐       ┌─────────────────────┐
                  │  void_presenter     │       │    void_xr          │
                  │ (Desktop/Web)       │       │  (XR presenter)     │
                  └──────────┬──────────┘       └──────────┬──────────┘
                             │                             │
                             └──────────────┬──────────────┘
                                            │
                                            ▼
                                 ┌─────────────────────┐
                                 │   void_kernel       │
                                 │   (end_frame)       │
                                 └──────────┬──────────┘
                                            │
                                            ▼
                                 ┌─────────────────────┐
                                 │    FRAME END        │
                                 └─────────────────────┘
```

---

## Data Dependencies

### Per-Frame Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                     DATA DEPENDENCIES                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  FrameTime                                                          │
│      │                                                               │
│      ├──► Physics step (accumulated time)                           │
│      ├──► Animation interpolation                                   │
│      ├──► Timer/scheduler updates                                   │
│      └──► Watchdog timeout checking                                 │
│                                                                      │
│  PatchBus                                                           │
│      │                                                               │
│      ├──► ecs namespace ──► Entity/Component changes                │
│      ├──► render namespace ──► Layer/Material changes               │
│      ├──► asset namespace ──► Asset state changes                   │
│      └──► physics namespace ──► Physics body changes                │
│                                                                      │
│  ECS World                                                          │
│      │                                                               │
│      ├──► Transform components ──► Model matrices ──► Renderer      │
│      ├──► Mesh components ──► GPU mesh lookup                       │
│      ├──► Material components ──► Shader bindings                   │
│      └──► Camera components ──► View/Projection                     │
│                                                                      │
│  RenderGraph                                                        │
│      │                                                               │
│      ├──► layers ──► Compositor execution order                     │
│      └──► backend ──► Platform-specific rendering                   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Crate Read/Write Access

| Crate | Reads | Writes |
|-------|-------|--------|
| void_engine | Config, state | FrameTime, plugin calls |
| void_kernel | Patches, layers | FrameContext, RenderGraph |
| void_ir | - | Patches, Transactions |
| void_ecs | Queries | Entities, Components |
| void_event | Subscriptions | Dispatched callbacks |
| void_physics | Forces, dt | Transforms, collision events |
| void_render | RenderGraph | GPU commands, framebuffer |
| void_presenter | Frame | Display output |
| void_xr | Poses, views | HMD submission |
| void_asset | Paths | Loaded assets |

---

## Timing Characteristics

### Frame Budget (60 FPS = 16.67ms)

```
┌────────────────────────────────────────────────────────────────────┐
│                    TYPICAL FRAME BUDGET                             │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Phase                        Typical Time                          │
│  ─────────────────────────────────────────                          │
│  Timing Update                < 0.01 ms                             │
│  Kernel Begin Frame           < 0.05 ms                             │
│  Pre-Update                   0.1 - 0.5 ms                          │
│  Fixed Update (physics)       1.0 - 3.0 ms (per step)              │
│  Update (game logic)          1.0 - 3.0 ms                          │
│  IR Transaction Processing    0.1 - 0.5 ms                          │
│  Post-Update                  0.1 - 0.3 ms                          │
│  Event Processing             0.1 - 0.5 ms                          │
│  ECS Systems                  0.5 - 2.0 ms                          │
│  Build Render Graph           0.1 - 0.3 ms                          │
│  Pre-Render                   0.2 - 1.0 ms                          │
│  Render                       5.0 - 10.0 ms (GPU bound)            │
│  Post-Render                  0.1 - 0.5 ms                          │
│  Presentation                 0 - 8.0 ms (VSync wait)              │
│  Kernel End Frame             < 0.05 ms                             │
│                                                                     │
│  Total (excluding VSync): ~9-21 ms                                 │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## Error Handling

### Per-Phase Recovery

| Phase | Failure Mode | Recovery |
|-------|--------------|----------|
| Physics | Constraint divergence | Reset body, log warning |
| IR Processing | Invalid patch | Skip patch, log error |
| ECS Systems | Panic in system | Supervisor restarts app |
| Render | Shader error | Fallback shader |
| Presenter | Surface lost | Recreate surface |
| XR | Session lost | Fall back to desktop |

### Supervisor Recovery

```
┌────────────────────────────────────────────────────────────────────┐
│                     FAULT TOLERANCE                                 │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Watchdog monitors each frame:                                      │
│  • Frame timeout (configurable, e.g., 100ms)                       │
│  • Memory limits                                                    │
│  • CPU usage                                                        │
│                                                                     │
│  On fault:                                                          │
│  1. Watchdog detects timeout/crash                                  │
│  2. Supervisor isolates faulting app                                │
│  3. RecoveryManager restores last good state                        │
│  4. App restarted in sandbox                                        │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference

### Loop Pseudocode

```rust
fn run(&mut self) {
    self.call_plugins(|p| p.on_start(ctx));

    while self.state == Running {
        // UPDATE PHASE
        self.time.update();
        self.kernel.begin_frame(self.time.delta);

        self.call_plugins(|p| p.on_pre_update(ctx));

        while self.accumulated >= FIXED_DT {
            self.call_plugins(|p| p.on_fixed_update(ctx));
            self.accumulated -= FIXED_DT;
        }

        self.call_plugins(|p| p.on_update(ctx));
        self.kernel.process_transactions(&mut self.world);
        self.call_plugins(|p| p.on_post_update(ctx));
        self.events.process();
        self.world.run_systems();

        // RENDER PHASE
        let graph = self.kernel.build_render_graph();
        self.call_plugins(|p| p.on_pre_render(ctx));
        self.call_plugins(|p| p.on_render(ctx));
        self.compositor.execute(graph, &mut frame);
        self.call_plugins(|p| p.on_post_render(ctx));
        self.presenter.present(frame);

        self.kernel.end_frame();
    }

    self.call_plugins(|p| p.on_stop(ctx));
    self.call_plugins(|p| p.on_shutdown(ctx));
}
```

### Crate Reference

| Phase | Crate | Key Function |
|-------|-------|--------------|
| Timing | void_engine | `FrameTime::update()` |
| Begin Frame | void_kernel | `Kernel::begin_frame()` |
| Pre-Update | void_engine | `EnginePlugin::on_pre_update()` |
| Fixed Update | void_physics | `PhysicsWorld::step()` |
| Update | void_engine | `EnginePlugin::on_update()` |
| Patches | void_ir | `PatchBus::emit()` |
| Transactions | void_kernel | `Kernel::process_transactions()` |
| Events | void_event | `EventBus::process()` |
| Systems | void_ecs | `World::run_systems()` |
| Render Graph | void_kernel | `Kernel::build_render_graph()` |
| Render | void_render | `Compositor::execute()` |
| Present | void_presenter | `Presenter::present()` |
| End Frame | void_kernel | `Kernel::end_frame()` |

---

*Void Engine Legacy Crates - Render Loop HLA - All crates covered*
