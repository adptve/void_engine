# Module Initialization Order (C++ Port)

**Generated**: 2026-01-27
**Entry Point**: `src/main.cpp:301-844`

## Initialization Sequence

| Order | Module | Namespace | Init Location | Description |
|-------|--------|-----------|---------------|-------------|
| 1 | **render** | `void_render` | `main.cpp:388-397` | SceneRenderer (requires window context) |
| 2 | **services** | `void_services` | `main.cpp:400-446` | EventBus + ServiceRegistry |
| 3 | **asset** | `void_asset` | `main.cpp:450-472` | AssetServer with loaders |
| 4 | **ecs** | `void_ecs` | `main.cpp:475-498` | ECS World + SceneBridge |
| 5 | **physics** | `void_physics` | `main.cpp:501-532` | PhysicsWorld (gravity, collision) |
| 6 | **scene** | `void_scene` | `main.cpp:535-558` | Initial scene loading |
| 7 | **presenter** | `void_presenter` | `main.cpp:569-577` | Frame timing (60 FPS target) |
| 8 | **compositor** | `void_compositor` | `main.cpp:581-601` | Post-processing & final output |

**Note**: Window/OpenGL context created inline at `main.cpp:351-386` using GLFW (external library, not a module).

### Dependency Chain

```
render (SceneRenderer)
└── services (EventBus, ServiceRegistry)
      ├── asset (AssetServer, loaders)
      │     └── ecs (World, SceneBridge)
      │           └── physics (PhysicsWorld)
      │                 └── scene (LiveSceneManager)
      └── presenter (FrameTiming)
            └── compositor (post-processing)
```

---

## Legacy Comparison

| Order | C++ Module | Legacy Crate | Status |
|-------|------------|--------------|--------|
| 1 | render | void_render | Both active |
| 2 | services | void_services | Both active |
| 3 | asset | void_asset + void_asset_server | Merged in C++ |
| 4 | ecs | void_ecs | Both active |
| 5 | physics | void_physics | Both active |
| 6 | scene | void_scene (via void_kernel) | Both active |
| 7 | presenter | void_presenter | Both active |
| 8 | compositor | void_compositor | Both active |
| - | kernel | void_kernel | **C++ orphaned** |
| - | shell | void_shell | **C++ undocumented** |
| - | ir | void_ir | **C++ orphaned** |

**Key difference**: Legacy uses `void_kernel` to orchestrate initialization. C++ port has inline initialization in `main.cpp` - the `kernel` module exists but is not used.

---

## Module Status Matrix

### Legend
- **ACTIVE**: Initialized and used in main render loop
- **DEPENDENCY**: Used indirectly by active modules
- **ORPHANED**: Has code but NOT used in main.cpp
- **UNDOCUMENTED**: Has code but NO documentation

| Module | Status | Documented | Has Code | main.cpp | Legacy Equivalent |
|--------|--------|------------|----------|----------|-------------------|
| **render** | ACTIVE | Y | Y | L388 | void_render |
| **services** | ACTIVE | Y | Y | L400 | void_services |
| **asset** | ACTIVE | Y | Y | L450 | void_asset |
| **ecs** | ACTIVE | Y | Y | L475 | void_ecs |
| **physics** | ACTIVE | Y | Y | L501 | void_physics |
| **scene** | ACTIVE | Y | Y | L535 | void_scene |
| **presenter** | ACTIVE | Y | Y | L569 | void_presenter |
| **compositor** | ACTIVE | Y | Y | L581 | void_compositor |
| **core** | DEPENDENCY | Y | Y | - | void_core |
| **math** | DEPENDENCY | Y | Y | - | void_math |
| **memory** | DEPENDENCY | Y | Y | - | void_memory |
| **structures** | DEPENDENCY | Y | Y | - | void_structures |
| **shader** | DEPENDENCY | Y | Y | - | void_shader |
| **event** | DEPENDENCY | Y | Y | - | void_event |
| **engine** | ORPHANED | Y | Y | - | - |
| **kernel** | ORPHANED | Y | Y | - | void_kernel |
| **ir** | ORPHANED | Y | Y | - | void_ir |
| **graph** | ORPHANED | Y | Y | - | void_graph |
| **ai** | ORPHANED | Y | Y | - | void_ai |
| **combat** | ORPHANED | Y | Y | - | void_combat |
| **gamestate** | ORPHANED | Y | Y | - | void_gamestate |
| **triggers** | ORPHANED | Y | Y | - | void_triggers |
| **inventory** | ORPHANED | Y | Y | - | void_inventory |
| **hud** | ORPHANED | Y | Y | - | void_hud |
| **ui** | ORPHANED | Y | Y | - | void_ui |
| **audio** | ORPHANED | Y | Y | - | void_audio |
| **xr** | ORPHANED | Y | Y | - | void_xr |
| **cpp** | UNDOCUMENTED | N | Y | - | void_cpp |
| **runtime** | UNDOCUMENTED | N | Y | - | void_runtime |
| **shell** | UNDOCUMENTED | N | Y | - | void_shell |
| **script** | UNDOCUMENTED | N | Y | - | void_script |
| **scripting** | UNDOCUMENTED | N | Y | - | void_scripting |
| **editor** | UNDOCUMENTED | N | stub | - | void_editor |

---

## Orphaned Modules Detail

### Documented But Not Initialized (17 modules)

**Core/Infrastructure:**
| Module | Legacy | Notes |
|--------|--------|-------|
| engine | - | Engine class exists but main.cpp uses inline init |
| kernel | void_kernel | Module loader not used (legacy uses this!) |
| ir | void_ir | IR patches not used (legacy uses this!) |

**Gameplay Systems:**
| Module | Legacy | Notes |
|--------|--------|-------|
| ai | void_ai | FSM, behavior trees, pathfinding |
| combat | void_combat | Damage calculation, hitboxes |
| gamestate | void_gamestate | Game state machine |
| triggers | void_triggers | Trigger volumes and callbacks |
| inventory | void_inventory | Item and equipment management |

**UI/Presentation:**
| Module | Legacy | Notes |
|--------|--------|-------|
| graph | void_graph | Visual scripting nodes |
| hud | void_hud | HUD elements and animations |
| ui | void_ui | Full UI framework |

**Platform:**
| Module | Legacy | Notes |
|--------|--------|-------|
| audio | void_audio | Sound system |
| xr | void_xr | VR/XR support (Grade D issues) |

### Undocumented Modules (5 modules)

| Module | Legacy | Files | Largest File | Purpose |
|--------|--------|-------|--------------|---------|
| **cpp** | void_cpp | 7 | compiler.cpp (48KB) | Runtime C++ compilation |
| **runtime** | void_runtime | 11 | scene_parser.cpp (96KB) | Scene loading, window, input |
| **shell** | void_shell | 11 | builtins.cpp (75KB) | Interactive debug shell |
| **script** | void_script | 4 | parser.hpp | Scripting language parser |
| **scripting** | void_scripting | 4 | wasm_interpreter.hpp | WASM execution |

---

## Stub Files (To Be Deleted)

| File | Module | Action |
|------|--------|--------|
| `src/render/stub.cpp` | render | **DELETE** - causes ODR violation |
| `src/compositor/stub.cpp` | compositor | DELETE |
| `src/presenter/stub.cpp` | presenter | DELETE |
| `src/core/stub.cpp` | core | DELETE |
| `src/asset/stub.cpp` | asset | DELETE |
| `src/ecs/stub.cpp` | ecs | DELETE (header-only) |
| `src/event/stub.cpp` | event | DELETE (header-only) |
| `src/memory/stub.cpp` | memory | DELETE (header-only) |
| `src/graph/stub.cpp` | graph | DELETE |
| `src/shader/stub.cpp` | shader | DELETE |
| `src/editor/stub.cpp` | editor | DELETE |
| `src/shell/stub.cpp` | shell | DELETE |
| `src/runtime/stub.cpp` | runtime | DELETE |

---

## Critical Issues

### 1. render (Grade D)
- **Issue**: 45+ functions duplicated between `stub.cpp` and `gl_renderer.cpp`
- **Fix**: Delete `src/render/stub.cpp`

### 2. presenter Bypassed
- **Location**: `main.cpp:735`
- **Issue**: Direct `glfwSwapBuffers()` call bypasses presenter abstraction
- **Fix**: Route through presenter module

### 3. compositor (Grade C)
- **Issue**: `NullLayerCompositor::end_frame()` references undeclared variable
- **Fix**: Implement missing variable or remove dead code

### 4. kernel Not Used
- **Issue**: Legacy uses `void_kernel` for orchestration; C++ port ignores it
- **Fix**: Either use kernel module or document why inline init is preferred

### 5. Five Modules Need Documentation
- cpp, runtime, shell, script, scripting

---

## Recommended Actions

| Priority | Action | Modules |
|----------|--------|---------|
| **P0** | Delete render/stub.cpp | render |
| **P0** | Fix compositor compile error | compositor |
| **P1** | Integrate presenter properly | presenter |
| **P1** | Document undocumented modules | cpp, runtime, shell, script, scripting |
| **P2** | Delete all remaining stubs | 12 files |
| **P3** | Decide on kernel usage | kernel |
| **P3** | Integrate orphaned gameplay modules | ai, combat, triggers, etc. |
