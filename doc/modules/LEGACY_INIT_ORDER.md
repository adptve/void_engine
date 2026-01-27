# Legacy Crate Initialization Order

**Generated**: 2026-01-27
**Entry Point**: `legacy/crates/void_runtime/src/main.rs`

## Initialization Sequence

The legacy Rust system initializes modules in this order:

| Order | Module | Crate | Init Location | Description |
|-------|--------|-------|---------------|-------------|
| 1 | runtime | internal | `main.rs:13` | Runtime orchestration |
| 2 | presenter | internal | `main.rs:14` | GPU presentation layer |
| 3 | compositor | internal | `main.rs:15` | Wayland compositor |
| 4 | scene_renderer | internal | `main.rs:16` | 3D content rendering |
| 5 | scene_loader | internal | `main.rs:17` | Scene file loading |
| 6 | texture_manager | internal | `main.rs:18` | Texture management |
| 7 | input | internal | `main.rs:19` | Input handling |
| 8 | boot_config | internal | `main.rs:20` | Boot configuration |
| 9 | app_loader | internal | `main.rs:21` | Application loading |
| 10 | game_systems | internal | `main.rs:22` | Physics, triggers, AI |
| 11 | drm_main | internal | `main.rs:25` | DRM backend (Linux) |
| 12 | smithay_main | internal | `main.rs:28` | Smithay backend (Linux) |

### Main Function Flow

| Step | Location | Action |
|------|----------|--------|
| 1 | `main.rs:44-48` | Initialize logging |
| 2 | `main.rs:59-62` | Install panic handler |
| 3 | `main.rs:64-66` | Load boot configuration |
| 4 | `main.rs:69-103` | Select backend (Smithay/Winit/XR/CLI) |

### Windowed Backend Initialization (`main.rs:205-672`)

| Order | Location | Component | Crate Used |
|-------|----------|-----------|------------|
| 1 | `main.rs:269-270` | DesktopPresenter | `void_presenter` |
| 2 | `main.rs:274-279` | SceneRenderer | `void_render` |
| 3 | `main.rs:297-299` | GameWorld + GameSystemsManager | `void_physics`, `void_triggers` |
| 4 | `main.rs:305-326` | Scene loading + physics setup | `void_asset_server` |
| 5 | `main.rs:346-350` | Compositor (2D overlay) | `void_compositor` |
| 6 | `main.rs:352-361` | Runtime (kernel + shell) | `void_kernel`, `void_shell` |

### Dependency Chain

```
void_runtime (entry point)
├── void_kernel (orchestration)
│   ├── void_core (foundation)
│   ├── void_ecs (entities)
│   │   ├── void_math
│   │   ├── void_memory
│   │   └── void_structures
│   ├── void_ir (patches)
│   ├── void_render (render graph)
│   │   ├── void_asset
│   │   └── void_ecs
│   └── void_asset
├── void_shell (REPL)
│   ├── void_ir
│   └── void_kernel
├── void_presenter (GPU output)
├── void_compositor (Wayland)
├── void_render (camera, 3D)
├── void_asset_server (GLTF loading)
├── void_physics (Rapier 3D)
│   ├── void_ecs
│   ├── void_math
│   └── void_event
├── void_triggers (trigger volumes)
│   ├── void_math
│   └── void_event
├── void_script (scripting)
└── void_shader (pipeline)
```

---

## All Legacy Crates (34 total)

### Crate Status Matrix

| Crate | Status | Used By | Purpose |
|-------|--------|---------|---------|
| **void_runtime** | ENTRY | - | Metaverse OS Runtime |
| **void_kernel** | ACTIVE | runtime.rs:7 | Core kernel orchestration |
| **void_ir** | ACTIVE | runtime.rs:10-14 | IR and Patch Bus |
| **void_ecs** | ACTIVE | runtime.rs:8 | Entity Component System |
| **void_shell** | ACTIVE | runtime.rs:9 | Shell REPL interface |
| **void_script** | ACTIVE | app_loader.rs | Script execution |
| **void_presenter** | ACTIVE | presenter.rs:37 | GPU presentation |
| **void_compositor** | ACTIVE | compositor.rs:15 | Wayland compositor |
| **void_render** | ACTIVE | scene_renderer.rs:41 | Render graph, camera |
| **void_math** | ACTIVE | game_systems.rs:9 | Vec3, Quat, Mat4 |
| **void_asset** | ACTIVE | Cargo.toml | Hot-reload assets |
| **void_asset_server** | ACTIVE | scene_renderer.rs:71 | GLTF loading |
| **void_physics** | ACTIVE | game_systems.rs:6 | Rapier 3D physics |
| **void_triggers** | ACTIVE | game_systems.rs:7 | Trigger volumes |
| **void_shader** | ACTIVE | app_loader.rs | Shader pipeline |
| **void_services** | ACTIVE | Cargo.toml | System services |
| **void_xr** | ACTIVE | Cargo.toml (optional) | VR/XR support |
| **void_core** | DEPENDENCY | all crates | Foundation types |
| **void_memory** | DEPENDENCY | void_ecs | Memory utilities |
| **void_structures** | DEPENDENCY | void_ecs | Data structures |
| **void_event** | DEPENDENCY | void_physics | Lock-free events |
| **void_ai** | ORPHANED | - | AI/navigation |
| **void_audio** | ORPHANED | - | Sound (rodio) |
| **void_combat** | ORPHANED | - | Combat system |
| **void_cpp** | ORPHANED | - | C++ FFI |
| **void_editor** | SEPARATE | - | Visual editor binary |
| **void_engine** | UNCLEAR | - | Legacy wrapper? |
| **void_gamestate** | ORPHANED | - | Save/load system |
| **void_graph** | ORPHANED | - | Visual scripting |
| **void_hud** | ORPHANED | - | HUD elements |
| **void_inventory** | ORPHANED | - | Item management |
| **void_scripting** | ORPHANED | - | WASM (wasmtime) |
| **void_ui** | ORPHANED | - | Immediate-mode UI |

---

## Orphaned Crates Detail

### Not Used in Initialization (11 crates)

These crates exist but are NOT loaded by `void_runtime`:

**Gameplay Systems:**
| Crate | Location | Purpose | Dependencies |
|-------|----------|---------|--------------|
| `void_ai` | `legacy/crates/void_ai/` | AI, pathfinding, FSM | void_core, serde |
| `void_combat` | `legacy/crates/void_combat/` | Health, weapons, damage | void_core |
| `void_gamestate` | `legacy/crates/void_gamestate/` | Save/load, state machine | void_core |
| `void_inventory` | `legacy/crates/void_inventory/` | Items, equipment | void_core |

**UI/Presentation:**
| Crate | Location | Purpose | Dependencies |
|-------|----------|---------|--------------|
| `void_hud` | `legacy/crates/void_hud/` | HUD elements | void_core, void_math |
| `void_ui` | `legacy/crates/void_ui/` | Immediate-mode UI | void_core |
| `void_graph` | `legacy/crates/void_graph/` | Visual scripting | void_core, void_ir |

**Platform/Integration:**
| Crate | Location | Purpose | Dependencies |
|-------|----------|---------|--------------|
| `void_audio` | `legacy/crates/void_audio/` | Sound system | void_core, rodio |
| `void_cpp` | `legacy/crates/void_cpp/` | C++ FFI bridge | void_core |
| `void_scripting` | `legacy/crates/void_scripting/` | WASM interpreter | void_core, wasmtime |

**Separate Binary:**
| Crate | Location | Purpose | Notes |
|-------|----------|---------|-------|
| `void_editor` | `legacy/crates/void_editor/` | Visual scene editor | Independent `[[bin]]` |

---

## Active vs C++ Port Comparison

| Legacy Crate | C++ Module | Status |
|--------------|------------|--------|
| void_kernel | kernel | Both exist |
| void_ecs | ecs | Both exist |
| void_ir | ir | Both exist |
| void_render | render | Both exist (C++ has issues) |
| void_presenter | presenter | Both exist |
| void_compositor | compositor | Both exist (C++ has issues) |
| void_physics | physics | Both exist |
| void_asset | asset | Both exist |
| void_asset_server | asset (server) | Merged in C++ |
| void_shell | shell | C++ undocumented |
| void_script | script | C++ undocumented |
| void_math | math | Both exist |
| void_core | core | Both exist |
| void_services | services | Both exist |
| void_triggers | triggers | Both exist |
| void_ai | ai | Both exist (orphaned) |
| void_combat | combat | Both exist (orphaned) |
| void_gamestate | gamestate | Both exist (orphaned) |
| void_inventory | inventory | Both exist (orphaned) |
| void_hud | hud | Both exist (orphaned) |
| void_ui | ui | Both exist (orphaned) |
| void_audio | audio | Both exist (orphaned) |
| void_graph | graph | Both exist (orphaned) |
| void_xr | xr | Both exist (C++ has issues) |
| void_cpp | cpp | C++ undocumented |
| void_scripting | scripting | C++ undocumented |
| void_editor | editor | C++ stub only |
| void_event | event | Both exist |
| void_memory | memory | Both exist |
| void_structures | structures | Both exist |
| void_shader | shader | Both exist |

---

## Backend Support

| Backend | Feature Flag | Platform | Location |
|---------|--------------|----------|----------|
| Winit | default | All | `main.rs:205-672` |
| Smithay | `smithay` | Linux | `smithay_main.rs` |
| DRM | `drm-backend` | Linux | `drm_main.rs` |
| XR | `xr` | All | `main.rs:89-95` |
| CLI | fallback | All | `main.rs:97-102` |

---

## Summary

| Category | Count |
|----------|-------|
| Total crates | 34 |
| Actively used | 17 |
| Dependency-only | 4 |
| Orphaned | 11 |
| Separate binary | 1 |
| Unclear status | 1 |

**First crate hit**: `void_kernel` (via `runtime.rs:7`) - the kernel orchestrates all other systems.

**Core initialization chain**:
`void_kernel` → `void_ecs` → `void_shell` → `void_presenter` → `void_render` → `void_physics` → `void_triggers`
