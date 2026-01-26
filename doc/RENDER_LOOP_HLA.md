# Void Engine - Render Loop High-Level Architecture

This document provides a focused, high-level architecture view of the render loop cycle, covering all modules that participate in each frame.

---

## Render Loop Overview

The render loop executes continuously, processing one frame per iteration. Each frame follows a strict phase order to ensure deterministic behavior and proper data flow.

```
┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│   ┌──────────────┐                                                │
│   │ FRAME START  │                                                │
│   └──────┬───────┘                                                │
│          │                                                         │
│          ▼                                                         │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐      │
│   │    TIMING    │────►│    INPUT     │────►│    EVENTS    │      │
│   └──────────────┘     └──────────────┘     └──────────────┘      │
│          │                                                         │
│          ▼                                                         │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐      │
│   │    ASSETS    │────►│   PHYSICS    │────►│   SCENE/ECS  │      │
│   └──────────────┘     └──────────────┘     └──────────────┘      │
│          │                                                         │
│          ▼                                                         │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐      │
│   │   RENDERER   │────►│ COMPOSITOR   │────►│  PRESENTER   │      │
│   └──────────────┘     └──────────────┘     └──────────────┘      │
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

### Phase 1: Frame Timing

**Module:** `void_presenter::FrameTiming`

```
┌─────────────────────────────────────────┐
│            FRAME TIMING                  │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Previous frame timestamp       │
│                                          │
│  PROCESS: • Calculate delta_time         │
│           • Update frame history         │
│           • Compute rolling statistics   │
│                                          │
│  OUTPUT:  delta_time (float, seconds)    │
│                                          │
└─────────────────────────────────────────┘
```

**Key Data:**
- `delta_time`: Time elapsed since last frame
- `frame_count`: Total frames rendered
- `fps`: Frames per second (rolling average)

---

### Phase 2: Input Processing

**Module:** Platform layer (GLFW)

```
┌─────────────────────────────────────────┐
│           INPUT PROCESSING               │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   OS event queue                 │
│                                          │
│  PROCESS: • Poll window events           │
│           • Update keyboard state        │
│           • Update mouse state           │
│           • Handle resize/close          │
│                                          │
│  OUTPUT:  Input state buffers            │
│           Window state changes           │
│                                          │
└─────────────────────────────────────────┘
```

**Key Data:**
- Keyboard key states (pressed, held, released)
- Mouse position, button states, scroll
- Window resize events, close requests

---

### Phase 3: Event Bus Processing

**Module:** `void_services::EventBus`

```
┌─────────────────────────────────────────┐
│          EVENT BUS PROCESSING            │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   delta_time                     │
│           Queued events from prev frame  │
│                                          │
│  PROCESS: • Publish FrameStartEvent      │
│           • Process queued events        │
│           • Dispatch to subscribers      │
│                                          │
│  OUTPUT:  Subscriber callbacks invoked   │
│                                          │
└─────────────────────────────────────────┘
```

**Key Events:**
- `FrameStartEvent { delta_time }`
- `FrameEndEvent { frame_count }`
- Custom game events

---

### Phase 4: Asset Server Processing

**Module:** `void_asset::AssetServer`

```
┌─────────────────────────────────────────┐
│         ASSET SERVER PROCESSING          │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Pending load requests          │
│           File system notifications      │
│                                          │
│  PROCESS: • Check async load completions │
│           • Process hot-reload changes   │
│           • Update cache (promote/evict) │
│           • Fire load callbacks          │
│                                          │
│  OUTPUT:  Loaded assets available        │
│           Asset events (Loaded, Failed)  │
│                                          │
└─────────────────────────────────────────┘
```

**Key Data Flow:**
```
Pending → Loading → Loaded
                 └→ Failed

Hot Cache ←→ Resident Cache ←→ Streaming Cache
```

---

### Phase 5: Physics Simulation

**Module:** `void_physics::PhysicsWorld`

```
┌─────────────────────────────────────────┐
│          PHYSICS SIMULATION              │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   delta_time                     │
│           Rigidbody states               │
│           Forces/impulses                │
│                                          │
│  PROCESS: • Accumulate time              │
│           • Fixed timestep loop:         │
│             - Integrate velocities       │
│             - Broad phase collision      │
│             - Narrow phase collision     │
│             - Constraint solving         │
│             - Fire collision callbacks   │
│           • Interpolate visual state     │
│                                          │
│  OUTPUT:  Updated transforms             │
│           CollisionEvents                │
│           TriggerEvents                  │
│                                          │
└─────────────────────────────────────────┘
```

**Timing Model:**
```
Variable render rate
        │
        ▼
┌───────────────────────────────────────┐
│  Physics uses FIXED timestep (1/60s)  │
│                                        │
│  while (accumulated >= FIXED_DT):     │
│      simulate(FIXED_DT)               │
│      accumulated -= FIXED_DT          │
│                                        │
│  alpha = accumulated / FIXED_DT       │
│  visual_state = lerp(prev, curr, α)   │
└───────────────────────────────────────┘
```

---

### Phase 6: Scene/ECS Update

**Module:** `void_scene`, `void_ecs`

```
┌─────────────────────────────────────────┐
│           SCENE/ECS UPDATE               │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   delta_time                     │
│           ECS World state                │
│                                          │
│  PROCESS: • Hot-reload check (0.5s)      │
│             - Watch scene files          │
│             - Reload changed scenes      │
│           • AnimationSystem::update()    │
│             - Query AnimationComponents  │
│             - Update elapsed time        │
│             - Interpolate keyframes      │
│             - Write to TransformComponent│
│                                          │
│  OUTPUT:  Updated TransformComponents    │
│           Reloaded scene entities        │
│                                          │
└─────────────────────────────────────────┘
```

**ECS Query Pattern:**
```
World.query<AnimationComponent, TransformComponent>()
    │
    ├─► For each entity:
    │     animation.elapsed += delta_time
    │     transform = interpolate(animation.keyframes)
    │
    └─► Transform components updated in-place
```

---

### Phase 7: Renderer Update

**Module:** `void_render::SceneRenderer`

```
┌─────────────────────────────────────────┐
│           RENDERER UPDATE                │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   delta_time                     │
│           ECS World (current state)      │
│                                          │
│  PROCESS: • Shader hot-reload check      │
│           • Sync ECS → RenderEntities    │
│             - Read TransformComponent    │
│             - Read MeshComponent         │
│             - Read MaterialComponent     │
│             - Build model matrices       │
│                                          │
│  OUTPUT:  RenderEntity list ready        │
│           Shaders recompiled if needed   │
│                                          │
└─────────────────────────────────────────┘
```

---

### Phase 8: Render Pass

**Module:** `void_render::SceneRenderer`

```
┌─────────────────────────────────────────┐
│             RENDER PASS                  │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   RenderEntity list              │
│           Camera state                   │
│           Light array                    │
│                                          │
│  PROCESS:                                │
│                                          │
│   ┌─────────────────────────────────┐   │
│   │ 1. CLEAR PASS                   │   │
│   │    glClear(COLOR | DEPTH)       │   │
│   └─────────────────────────────────┘   │
│               │                          │
│               ▼                          │
│   ┌─────────────────────────────────┐   │
│   │ 2. SHADOW PASS (optional)       │   │
│   │    • Bind shadow FBO            │   │
│   │    • Render depth from light    │   │
│   │    • Output: shadow map         │   │
│   └─────────────────────────────────┘   │
│               │                          │
│               ▼                          │
│   ┌─────────────────────────────────┐   │
│   │ 3. GEOMETRY PASS                │   │
│   │    • Bind PBR shader            │   │
│   │    • Set camera uniforms        │   │
│   │    • Upload light array         │   │
│   │    • For each RenderEntity:     │   │
│   │      - Set model matrix         │   │
│   │      - Set material uniforms    │   │
│   │      - Bind VAO                 │   │
│   │      - glDrawElements()         │   │
│   └─────────────────────────────────┘   │
│               │                          │
│               ▼                          │
│   ┌─────────────────────────────────┐   │
│   │ 4. POST-PROCESSING (optional)   │   │
│   │    • Bloom extraction           │   │
│   │    • Gaussian blur              │   │
│   │    • Tone mapping               │   │
│   │    • Color grading              │   │
│   └─────────────────────────────────┘   │
│               │                          │
│               ▼                          │
│   ┌─────────────────────────────────┐   │
│   │ 5. DEBUG OVERLAYS               │   │
│   │    • Grid rendering             │   │
│   │    • Gizmos                     │   │
│   └─────────────────────────────────┘   │
│                                          │
│  OUTPUT:  Rendered frame in back buffer  │
│                                          │
└─────────────────────────────────────────┘
```

**GPU Data Flow:**
```
RenderEntity
    │
    ├── model_matrix (from TransformComponent)
    ├── mesh_id → GpuMesh (VAO, VBO, EBO)
    └── material → PBR uniforms
            │
            ▼
        GPU Pipeline
            │
            ├── Vertex Shader
            │     • Transform vertices
            │     • Pass normals, UVs
            │
            └── Fragment Shader
                  • PBR lighting
                  • Shadow sampling
                  • Output color
```

---

### Phase 9: Compositor Processing

**Module:** `void_compositor::ICompositor`

```
┌─────────────────────────────────────────┐
│         COMPOSITOR PROCESSING            │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Rendered frame                 │
│           Display configuration          │
│                                          │
│  PROCESS: • dispatch()                   │
│             - Poll display events        │
│             - Update VRR state           │
│           • should_render() check        │
│           • begin_frame()                │
│             - Acquire render target      │
│           • end_frame()                  │
│             - Submit to display          │
│           • update_content_velocity()    │
│             - Hint for VRR adaptation    │
│                                          │
│  OUTPUT:  Frame queued for display       │
│           VRR timing adjusted            │
│                                          │
└─────────────────────────────────────────┘
```

**VRR (Variable Refresh Rate) Flow:**
```
Content Velocity → Frame Scheduler → Display Timing
      │                  │                 │
      │                  ▼                 │
      │           Adaptive sync           │
      │           (FreeSync/G-Sync)       │
      │                  │                 │
      └──────────────────┴─────────────────┘
```

---

### Phase 10: Presentation

**Module:** `void_presenter::IPresenter` (via GLFW)

```
┌─────────────────────────────────────────┐
│            PRESENTATION                  │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   Back buffer with rendered frame│
│                                          │
│  PROCESS: • glfwSwapBuffers()            │
│             - Swap front/back buffers    │
│             - VSync synchronization      │
│                                          │
│  OUTPUT:  Frame visible on display       │
│                                          │
└─────────────────────────────────────────┘
```

**Double Buffering:**
```
┌──────────────┐     ┌──────────────┐
│ Back Buffer  │     │ Front Buffer │
│ (rendering)  │────►│ (displayed)  │
└──────────────┘     └──────────────┘
       │ swap              │
       └──────────────────-┘
```

---

### Phase 11: Frame End

**Module:** `void_services::EventBus`, Statistics

```
┌─────────────────────────────────────────┐
│             FRAME END                    │
├─────────────────────────────────────────┤
│                                          │
│  INPUT:   frame_count                    │
│                                          │
│  PROCESS: • Publish FrameEndEvent        │
│           • Statistics (per second):     │
│             - FPS calculation            │
│             - Frame time stats           │
│             - Draw call count            │
│             - Entity counts              │
│             - Physics stats              │
│             - Service health             │
│                                          │
│  OUTPUT:  Logged statistics              │
│           Frame counter incremented      │
│                                          │
└─────────────────────────────────────────┘
```

---

## Module Integration Diagram

```
                    ┌─────────────────┐
                    │   FRAME START   │
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              ▼                             ▼
    ┌─────────────────┐           ┌─────────────────┐
    │  FrameTiming    │           │  GLFW Input     │
    │  (presenter)    │           │  (platform)     │
    └────────┬────────┘           └────────┬────────┘
             │ delta_time                   │ input state
             └──────────────┬───────────────┘
                            ▼
                  ┌─────────────────┐
                  │   EventBus      │
                  │   (services)    │
                  └────────┬────────┘
                           │ FrameStartEvent
                           ▼
                  ┌─────────────────┐
                  │  AssetServer    │
                  │   (asset)       │
                  └────────┬────────┘
                           │ loaded assets
                           ▼
                  ┌─────────────────┐
                  │  PhysicsWorld   │
                  │   (physics)     │
                  └────────┬────────┘
                           │ collision events, transforms
                           ▼
         ┌─────────────────┴─────────────────┐
         ▼                                   ▼
┌─────────────────┐               ┌─────────────────┐
│ LiveSceneManager│               │ AnimationSystem │
│    (scene)      │               │    (scene)      │
└────────┬────────┘               └────────┬────────┘
         │ reloaded scenes                  │ animated transforms
         └─────────────────┬────────────────┘
                           ▼
                  ┌─────────────────┐
                  │   ECS World     │
                  │    (ecs)        │
                  └────────┬────────┘
                           │ component data
                           ▼
                  ┌─────────────────┐
                  │ SceneRenderer   │
                  │   (render)      │
                  └────────┬────────┘
                           │ GPU commands
                           ▼
                  ┌─────────────────┐
                  │  Compositor     │
                  │  (compositor)   │
                  └────────┬────────┘
                           │ display frame
                           ▼
                  ┌─────────────────┐
                  │   Presenter     │
                  │   (presenter)   │
                  └────────┬────────┘
                           │ buffer swap
                           ▼
                  ┌─────────────────┐
                  │   FRAME END     │
                  └─────────────────┘
```

---

## Data Dependencies

### Per-Frame Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                     DATA DEPENDENCIES                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  delta_time                                                         │
│      │                                                               │
│      ├──► Physics step (time accumulation)                          │
│      ├──► Animation update (elapsed time)                           │
│      ├──► Shader hot-reload timer                                   │
│      └──► Scene hot-reload timer                                    │
│                                                                      │
│  ECS World                                                          │
│      │                                                               │
│      ├──► TransformComponent ──► Model matrices ──► Renderer        │
│      ├──► MeshComponent ──► GPU mesh lookup ──► Renderer            │
│      ├──► MaterialComponent ──► PBR uniforms ──► Renderer           │
│      ├──► AnimationComponent ──► AnimationSystem ──► Transform      │
│      ├──► CameraComponent ──► View/Projection matrices              │
│      └──► LightComponent ──► Light array ──► Renderer               │
│                                                                      │
│  Asset Server                                                        │
│      │                                                               │
│      ├──► Textures ──► Material bindings ──► Renderer               │
│      ├──► Models ──► GpuMesh creation ──► Renderer                  │
│      └──► Shaders ──► ShaderProgram ──► Renderer                    │
│                                                                      │
│  Physics World                                                       │
│      │                                                               │
│      └──► Rigidbody transforms ──► [sync] ──► TransformComponent    │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Module Read/Write Access

| Module | Reads | Writes |
|--------|-------|--------|
| FrameTiming | Clock | delta_time, stats |
| Input (GLFW) | OS events | Input buffers |
| EventBus | Events | Subscriber callbacks |
| AssetServer | File system | Asset cache, handles |
| PhysicsWorld | Forces, delta_time | Transforms, collision events |
| AnimationSystem | AnimationComponent | TransformComponent |
| SceneRenderer | All ECS components | GPU buffers, framebuffer |
| Compositor | Framebuffer | Display output |
| Presenter | Back buffer | Front buffer (display) |

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
│  Frame Timing                 < 0.01 ms                             │
│  Input Processing             < 0.1 ms                              │
│  Event Bus                    < 0.1 ms                              │
│  Asset Server                 0.1 - 1.0 ms (varies with loads)     │
│  Physics Simulation           1.0 - 3.0 ms (depends on complexity) │
│  Scene/ECS Update             0.5 - 2.0 ms                          │
│  Renderer Update              0.1 - 0.5 ms                          │
│  Render Pass                  5.0 - 12.0 ms (GPU bound)            │
│  Compositor                   0.1 - 0.5 ms                          │
│  Presentation (VSync)         0 - 16.67 ms (waiting)               │
│  Frame End                    < 0.1 ms                              │
│                                                                     │
│  Total (excluding VSync wait): ~8-19 ms                            │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

### Fixed vs Variable Timestep

```
┌────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  PHYSICS: Fixed timestep (1/60s = 16.67ms)                         │
│  ─────────────────────────────────────────                          │
│  • Deterministic simulation                                         │
│  • Accumulator pattern for variable frame rates                     │
│  • Interpolation for smooth visuals                                 │
│                                                                     │
│  RENDERING: Variable timestep (as fast as possible or VSync)       │
│  ────────────────────────────────────────────────────               │
│  • Uses delta_time for time-based animations                        │
│  • VSync limits to display refresh rate                             │
│  • VRR allows variable refresh rates                                │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## Error Handling

### Per-Phase Recovery

| Phase | Failure Mode | Recovery |
|-------|--------------|----------|
| Input | Window close | Request quit |
| Asset Loading | Load failed | Use placeholder, log error |
| Physics | Constraint divergence | Reset body, log warning |
| Animation | Missing keyframes | Skip animation |
| Render | Shader compile fail | Use fallback shader |
| Compositor | Display lost | Recreate surface |
| Presenter | Swap fail | Log, continue |

---

## Hot-Reload Integration

```
┌────────────────────────────────────────────────────────────────────┐
│                     HOT-RELOAD POINTS                               │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ASSET PHASE:                                                       │
│    • Textures: File watcher → reload texture → rebind              │
│    • Models: File watcher → reload mesh → update GPU buffers       │
│                                                                     │
│  SCENE PHASE (every 0.5s):                                         │
│    • Scene files: File watcher → reparse → reinstantiate entities  │
│    • Preserves entity IDs where possible                           │
│                                                                     │
│  RENDER UPDATE PHASE:                                               │
│    • Shaders: File watcher → recompile → relink program            │
│    • Automatic uniform rebinding                                    │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference

### Loop Pseudocode

```cpp
while (!quit_requested) {
    // Phase 1: Timing
    float dt = frame_timing.begin_frame();

    // Phase 2: Input
    glfwPollEvents();

    // Phase 3: Events
    event_bus.publish(FrameStartEvent{dt});
    event_bus.process_queue();

    // Phase 4: Assets
    asset_server.process();

    // Phase 5: Physics
    physics_world->step(dt);

    // Phase 6: Scene/ECS
    live_scene_mgr.update(dt);
    AnimationSystem::update(ecs_world, dt);

    // Phase 7: Renderer Update
    renderer.update(dt);

    // Phase 8: Render
    renderer.render();

    // Phase 9: Compositor
    compositor->dispatch();
    if (compositor->should_render()) {
        auto target = compositor->begin_frame();
        compositor->end_frame(std::move(target));
    }

    // Phase 10: Present
    glfwSwapBuffers(window);

    // Phase 11: Frame End
    event_bus.publish(FrameEndEvent{frame_count++});
}
```

### Module Namespace Reference

| Phase | Namespace | Key Class |
|-------|-----------|-----------|
| Timing | `void_presenter` | `FrameTiming` |
| Input | GLFW | `glfwPollEvents` |
| Events | `void_services` | `EventBus` |
| Assets | `void_asset` | `AssetServer` |
| Physics | `void_physics` | `PhysicsWorld` |
| Scene | `void_scene` | `LiveSceneManager`, `AnimationSystem` |
| ECS | `void_ecs` | `World` |
| Render | `void_render` | `SceneRenderer` |
| Compositor | `void_compositor` | `ICompositor` |
| Presenter | `void_presenter` | `IPresenter` |

---

*Void Engine Render Loop HLA - All modules covered*
