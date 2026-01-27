# Proposed Module Initialization Order

**Generated**: 2026-01-27
**Based on**: Unity, Godot, Unreal Engine, Frostbite, Jason Gregory's "Game Engine Architecture"

## Overview

This document proposes a canonical initialization order for ALL void_engine modules, following industry best practices from major game engines.

**Key Principles:**
1. Memory must initialize first - everything allocates
2. Logging second - need diagnostics during init
3. Platform abstraction before windowing
4. Rendering before physics (debug visualization)
5. ECS before scene management
6. Shutdown in reverse order

---

## Proposed Initialization Sequence

### Phase 1: Foundation (No Dependencies)

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 1 | **memory** | `void_memory` | Custom allocators, pools, stacks | None |
| 2 | **core** | `void_core` | Logging, assertions, platform detection | memory |
| 3 | **math** | `void_math` | Vec3, Quat, Mat4, SIMD | None |
| 4 | **structures** | `void_structures` | Containers, hash maps, trees | memory |

### Phase 2: Infrastructure

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 5 | **event** | `void_event` | Lock-free event bus, signals | memory, structures |
| 6 | **services** | `void_services` | ServiceRegistry, EventBus | core, event |
| 7 | **ir** | `void_ir` | Intermediate representation, patches | core, structures |
| 8 | **kernel** | `void_kernel` | Module orchestration, lifecycle | core, services, ir |

### Phase 3: Resources

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 9 | **asset** | `void_asset` | Asset handles, caching, hot-reload | core, services, structures |
| 10 | **shader** | `void_shader` | Shader compilation, pipeline states | core, asset |

### Phase 4: Platform Services

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 11 | **presenter** | `void_presenter` | Window creation, display, vsync | core, services |
| 12 | **render** | `void_render` | GPU context, render graph, frame graph | presenter, shader, asset |
| 13 | **compositor** | `void_compositor` | Post-processing, layer compositing | render |

### Phase 5: Input/Output

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 14 | **audio** | `void_audio` | Sound device, 3D audio, mixer | core, services, math |
| 15 | *(input)* | - | Input devices, mapping | presenter |

### Phase 6: Simulation

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 16 | **ecs** | `void_ecs` | Entity storage, archetypes, queries | memory, structures |
| 17 | **physics** | `void_physics` | Rigidbody, collision, Rapier | ecs, math, event |
| 18 | **triggers** | `void_triggers` | Trigger volumes, callbacks | physics, ecs, event |

### Phase 7: Scene Management

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 19 | **scene** | `void_scene` | Scene graph, transforms, hierarchy | ecs, asset, render |
| 20 | **graph** | `void_graph` | Visual scripting, node graph | scene, ir |

### Phase 8: Scripting

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 21 | **script** | `void_script` | Lexer, parser, AST | core, ir |
| 22 | **scripting** | `void_scripting` | WASM interpreter, VM | script, ecs |
| 23 | **cpp** | `void_cpp` | Runtime C++ compilation, FFI | core, kernel |
| 24 | **shell** | `void_shell` | Debug REPL, commands | scripting, kernel |

### Phase 9: Gameplay Systems

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 25 | **ai** | `void_ai` | FSM, behavior trees, NavMesh | ecs, scene, physics |
| 26 | **combat** | `void_combat` | Damage, hitboxes, weapons | ecs, physics, event |
| 27 | **inventory** | `void_inventory` | Items, equipment, containers | ecs, event |
| 28 | **gamestate** | `void_gamestate` | Save/load, state machine | ecs, services |

### Phase 10: User Interface

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 29 | **ui** | `void_ui` | Immediate-mode UI, widgets | render, input |
| 30 | **hud** | `void_hud` | HUD elements, health bars | ui, ecs |

### Phase 11: Platform Extensions

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 31 | **xr** | `void_xr` | VR/AR headsets, controllers | render, presenter |
| 32 | **editor** | `void_editor` | Scene editor, inspector | ui, scene, all |

### Phase 12: Application Layer

| Order | Module | Namespace | Purpose | Dependencies |
|-------|--------|-----------|---------|--------------|
| 33 | **runtime** | `void_runtime` | Application lifecycle | kernel, all |
| 34 | **engine** | `void_engine` | Engine facade, main loop | runtime |

---

## Dependency Graph

```
Phase 1: Foundation
┌─────────────────────────────────────────────────────────┐
│  memory ──► core ──► structures                         │
│    │                     │                              │
│    └──────► math ◄───────┘                              │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 2: Infrastructure
┌─────────────────────────────────────────────────────────┐
│  event ──► services ──► ir ──► kernel                   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 3: Resources
┌─────────────────────────────────────────────────────────┐
│  asset ──► shader                                       │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 4: Platform
┌─────────────────────────────────────────────────────────┐
│  presenter ──► render ──► compositor                    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 5-6: I/O & Simulation
┌─────────────────────────────────────────────────────────┐
│  audio    ecs ──► physics ──► triggers                  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 7-8: Scene & Scripting
┌─────────────────────────────────────────────────────────┐
│  scene ──► graph                                        │
│  script ──► scripting ──► cpp ──► shell                 │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 9-10: Gameplay & UI
┌─────────────────────────────────────────────────────────┐
│  ai ──► combat ──► inventory ──► gamestate              │
│  ui ──► hud                                             │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
Phase 11-12: Extensions & Application
┌─────────────────────────────────────────────────────────┐
│  xr    editor    runtime ──► engine                     │
└─────────────────────────────────────────────────────────┘
```

---

## Comparison: Current vs Proposed

| Current Order | Proposed Order | Issue |
|---------------|----------------|-------|
| 1. render | 1. memory | **Wrong** - render before memory |
| 2. services | 2. core | **Wrong** - services before core |
| 3. asset | 3. math | OK (reordered) |
| 4. ecs | 4. structures | OK (reordered) |
| 5. physics | 5. event | Missing from current |
| 6. scene | 6. services | OK |
| 7. presenter | 7. ir | Missing from current |
| 8. compositor | 8. kernel | Missing from current |
| - | 9-34 ... | 26 modules missing |

**Critical gaps in current implementation:**
- `memory` not explicitly initialized (relies on default allocators)
- `kernel` not used (inline init instead)
- `ir` not used (patch system bypassed)
- 17 gameplay/scripting modules orphaned

---

## Shutdown Sequence

Shutdown proceeds in **reverse order** (34 → 1):

```
engine → runtime → editor → xr → hud → ui → gamestate → inventory →
combat → ai → shell → cpp → scripting → script → graph → scene →
triggers → physics → ecs → audio → compositor → render → presenter →
shader → asset → kernel → ir → services → event → structures →
math → core → memory
```

**Key rule**: Dependencies must remain available during cleanup.

---

## Hot-Reload Boundaries

Based on industry best practices, these modules should support hot-reload:

| Module | Hot-Reload Type | State Handling |
|--------|-----------------|----------------|
| **script** | Full reload | Serialize AST |
| **scripting** | Full reload | Serialize VM state |
| **cpp** | DLL/SO swap | snapshot/restore |
| **shader** | Pipeline reload | Invalidate caches |
| **asset** | Handle-based | Bump versions |
| **scene** | Partial reload | dehydrate/rehydrate |
| **graph** | Full reload | Serialize nodes |
| **ui** | Stateless | N/A |
| **hud** | Partial | Preserve bindings |

**Critical hot-reload functions to preserve:**
```cpp
snapshot()           // Capture state before reload
restore()            // Restore state after reload
dehydrate()          // Serialize to portable format
rehydrate()          // Deserialize from portable format
on_reloaded()        // Post-reload callback
current_version()    // Version for compatibility
is_compatible()      // Migration validation
```

---

## Frame Loop Integration

Once initialized, the frame loop follows this order (per Unity/Unreal patterns):

```
┌─────────────────────────────────────────────────────────┐
│ Frame Start                                             │
├─────────────────────────────────────────────────────────┤
│ 1. Input polling          (presenter)                   │
│ 2. Event dispatch         (services/event)              │
│ 3. Script execution       (scripting)                   │
│ 4. AI update              (ai)                          │
│ 5. Physics step           (physics) [fixed timestep]    │
│ 6. Trigger callbacks      (triggers)                    │
│ 7. Gameplay update        (combat, inventory, etc.)     │
│ 8. Scene graph update     (scene)                       │
│ 9. Animation update       (ecs)                         │
│ 10. Late update           (gamestate)                   │
├─────────────────────────────────────────────────────────┤
│ Render Phase                                            │
├─────────────────────────────────────────────────────────┤
│ 11. Visibility culling    (scene)                       │
│ 12. Render graph build    (render)                      │
│ 13. GPU submission        (render)                      │
│ 14. UI render             (ui/hud)                      │
│ 15. Post-processing       (compositor)                  │
│ 16. Present               (presenter)                   │
├─────────────────────────────────────────────────────────┤
│ Frame End                                               │
│ 17. Hot-reload check      (kernel)                      │
│ 18. Asset streaming       (asset)                       │
│ 19. Audio update          (audio)                       │
└─────────────────────────────────────────────────────────┘
```

---

## Implementation Recommendations

### Priority 1: Fix Foundation Order
```cpp
// main.cpp should become:
void_memory::init();        // 1. Memory first
void_core::init();          // 2. Logging/platform
void_math::init();          // 3. Math (likely no-op)
void_structures::init();    // 4. Containers
```

### Priority 2: Integrate Kernel
```cpp
// Replace inline init with kernel orchestration
void_kernel::Kernel kernel;
kernel.register_module<void_services::Module>();
kernel.register_module<void_asset::Module>();
kernel.register_module<void_ecs::Module>();
// ... etc
kernel.init_all();  // Respects dependency order
```

### Priority 3: Enable Orphaned Modules
```cpp
// Gameplay modules should be registered
kernel.register_module<void_ai::Module>();
kernel.register_module<void_combat::Module>();
kernel.register_module<void_inventory::Module>();
kernel.register_module<void_gamestate::Module>();
```

### Priority 4: Document Undocumented Modules
- `cpp` - Runtime C++ compilation
- `runtime` - Application lifecycle
- `shell` - Debug REPL
- `script` - Language parser
- `scripting` - WASM VM

---

## Module Count Summary

| Category | Count | Modules |
|----------|-------|---------|
| Foundation | 4 | memory, core, math, structures |
| Infrastructure | 4 | event, services, ir, kernel |
| Resources | 2 | asset, shader |
| Platform | 3 | presenter, render, compositor |
| I/O | 1 | audio |
| Simulation | 3 | ecs, physics, triggers |
| Scene | 2 | scene, graph |
| Scripting | 4 | script, scripting, cpp, shell |
| Gameplay | 4 | ai, combat, inventory, gamestate |
| UI | 2 | ui, hud |
| Extensions | 2 | xr, editor |
| Application | 2 | runtime, engine |
| **Total** | **33** | |

---

## References

- Jason Gregory, *Game Engine Architecture* (3rd Edition)
- [GDC 2017: Frostbite FrameGraph](https://www.gdcvault.com/play/1024612/)
- [Unity Execution Order](https://docs.unity3d.com/Manual/ExecutionOrder.html)
- [Unreal Engine Modules](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-modules)
- [Godot Servers Architecture](https://docs.godotengine.org/en/stable/tutorials/performance/using_servers.html)
- [Hot Reloading in Exile](https://thenumb.at/Hot-Reloading-in-Exile/)
- [ECS FAQ](https://github.com/SanderMertens/ecs-faq)
