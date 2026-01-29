# Void Engine - Orphaned Code Analysis

**Date:** 2026-01-29
**Traced From:** `src/main.cpp` through all modules
**Analysis Method:** Systematic tracing of includes, instantiations, and call chains

---

## Executive Summary

A comprehensive trace of `src/main.cpp` through all headers and modules reveals **significant orphaned code** across the codebase. While the runtime and core rendering pipeline are well-integrated, many sophisticated subsystems exist in complete isolation with zero integration points.

### Key Statistics

| Category | Lines of Code | Orphaned % |
|----------|--------------|------------|
| Render Module (orphaned features) | ~8,500 | 40% |
| Kernel Module (supervisor/sandbox) | ~1,450 | 35% |
| Graph Module (visual programming) | ~1,662 | 100% |
| C++ Hot Reload System | ~1,050 | 95% |
| Shell Module | ~600+ | 100% |
| Asset Remote Loading | ~300 | 100% |
| Scripting (WASM) | ~200 | 80% |
| **Total Orphaned** | **~13,762+** | - |

---

## Application Entry Point Trace

### main.cpp → Runtime Flow

```
main()
├── ModeSelector (CLI parsing, config loading)
├── Runtime::initialize()
│   ├── init_kernel() → Kernel (stages, system registration)
│   ├── init_foundation() → (header-only, no calls)
│   ├── init_infrastructure() → EventBus
│   ├── init_api_connectivity() → (stub, logging only)
│   ├── init_platform() → IPlatform (GLFW windowing)
│   ├── init_render() → SceneRenderer, SceneParser
│   ├── init_io() → Input polling, Audio update
│   └── init_simulation() → ECS progress system
├── Runtime::run()
│   └── execute_frame() → 11 kernel stages per frame
└── Runtime::shutdown()
```

### What IS Connected (Active Code)

| Module | Status | Evidence |
|--------|--------|----------|
| Runtime | ✅ ACTIVE | Entry point, lifecycle management |
| Kernel (stages) | ✅ ACTIVE | System scheduling, 11 stages executed |
| Event Bus | ✅ ACTIVE | World events published |
| ECS World | ✅ ACTIVE | Entities spawned, components attached |
| Scene Renderer | ✅ ACTIVE | OpenGL rendering each frame |
| Platform (GLFW) | ✅ ACTIVE | Window creation, input polling |
| Asset Loading | ✅ PARTIAL | Models/textures loaded, remote unused |
| Math/Structures | ✅ ACTIVE | Header-only, used throughout |

---

## Module-by-Module Orphaned Code Analysis

### 1. KERNEL MODULE

**Location:** `src/kernel/`, `include/void_engine/kernel/`

#### ACTIVE (Used)
- **Stage System** - All 11 stages executed each frame
- **System Registration** - 6+ systems registered in runtime
- **Kernel Lifecycle** - initialize/start/shutdown called
- **Module Loader** - Created but minimal usage

#### ORPHANED (~1,450 lines)

| Component | File | Lines | Issue |
|-----------|------|-------|-------|
| **Supervisor Tree** | `supervisor.hpp/cpp` | ~800 | Created but never accessed, no add_child() calls |
| **Sandbox System** | `sandbox.hpp/cpp` | ~650 | Never instantiated, permissions system unused |
| **Module Registry** | `module_loader.hpp` | ~80 | Interface exists but never called |
| **Hot-Reload Callbacks** | `kernel.cpp:74-87` | ~50 | Callbacks defined but bodies empty |

**Supervisor Tree Details:**
```cpp
// DEFINED (supervisor.hpp):
class Supervisor { ... };
class SupervisorTree { ... };
class ChildSpec { ... };
enum class RestartStrategy { ... };

// NEVER CALLED:
kernel->supervisors()  // Zero references in codebase
add_child(), start(), stop(), check_children()  // Zero calls
```

**Sandbox System Details:**
```cpp
// DEFINED (sandbox.hpp):
class Sandbox { ... };
class SandboxFactory { ... };
class PermissionSet { ... };
class ResourceUsageTracker { ... };

// NEVER CALLED:
kernel->create_sandbox()  // Zero references in codebase
```

---

### 2. ECS MODULE

**Location:** `src/ecs/`, `include/void_engine/ecs/`

#### ACTIVE (Used)
- **World** - Created, updated each frame
- **Entity/Component** - Full lifecycle management
- **Query System** - Systems iterate via queries
- **Archetype** - Internal storage optimization

#### ORPHANED (~350 lines)

| Component | File | Lines | Issue |
|-----------|------|-------|-------|
| **Snapshot System** | `snapshot.hpp` | ~200 | Zero callsites for snapshot/restore |
| **System Scheduler** | `system.hpp` | ~150 | Defined but not kernel-integrated |

**Snapshot System Details:**
```cpp
// DEFINED but never called:
WorldSnapshot::snapshot()   // For hot-reload state preservation
WorldSnapshot::restore()    // Never invoked
World::add_component_raw()  // Snapshot restore helper - unused
World::set_entity_location() // Snapshot restore helper - unused
```

---

### 3. RENDER MODULE

**Location:** `src/render/`, `include/void_engine/render/`

#### ACTIVE Pipeline

```
Engine Frame
├── ModelLoaderSystem (Update) - loads assets
├── TransformSystem (Update) - builds matrices
├── AnimationSystem (Update) - simple euler animation
├── CameraSystem (PreUpdate) - extracts camera data
├── LightSystem (PreUpdate) - collects up to 4 lights
├── RenderPrepareSystem (PostUpdate) - builds draw queue
└── RenderSystem (exclusive) - OpenGL draw calls
```

#### ORPHANED (~8,500 lines)

| Component | File | Lines | Issue |
|-----------|------|-------|-------|
| **Skeletal Animation** | `animation.cpp` | ~1,700 | Complete system, zero references |
| **Post-Processing** | `post_process.cpp` | ~850 | Bloom/SSAO/FXAA defined, never called |
| **Temporal Effects** | `temporal_effects.cpp` | ~1,900 | TAA/DOF/MotionBlur, never instantiated |
| **LOD System** | `lod.cpp` | ~1,000 | Full LOD selection, never queried |
| **Shadow Renderer** | `shadow_renderer.cpp` | ~1,150 | CSM/atlas defined, never executed |
| **Debug Renderer** | `debug_renderer.cpp` | ~600 | Debug primitives, never called |
| **Instancing** | `instancing.cpp` | ~600 | Batching system, never used |
| **Render Graph** | `render_graph.cpp` | ~400 | Pass dependencies defined, never compiled |
| **Spatial System** | `spatial.cpp` | ~2,600 | BVH culling, never used |

**Animation System Details:**
```cpp
// FULLY IMPLEMENTED (animation.cpp ~1,700 lines):
class Skeleton { ... };           // Bone hierarchy
class AnimationChannel { ... };   // Per-joint tracks
class AnimationClip { ... };      // Complete sequences
class AnimationLayer { ... };     // Layered blending
class AnimationMixer { ... };     // State management
class MorphTarget { ... };        // Blend shapes
class SkinnedMesh { ... };        // GPU skinning
class AnimationManager { ... };   // Central orchestration

// ACTIVE SYSTEM uses DIFFERENT approach:
// AnimationSystem in render_systems.cpp uses simple
// AnimationComponent with euler angles - NOT skeletal
```

**Post-Processing Details:**
```cpp
// DEFINED (post_process.cpp ~850 lines):
PostProcessPipeline::apply()      // Never called
- Bloom (brightpass + blur)
- SSAO (screen-space AO)
- FXAA (fast approximate AA)
- Tonemapping (multiple operators)
- Vignette, Chromatic Aberration

// ACTUAL RENDERING:
RenderSystem does glDrawElements directly to framebuffer
// No post-process hooks exist in render_systems.cpp
```

**Missing Connections (Defined but Not Wired):**
1. glTF animations loaded but never converted to AnimationComponent
2. Textures loaded but not bound in draw loop
3. Shadow pass defined but never executed
4. LOD manager exists but always uses full detail
5. Instancing batcher exists but draws one-by-one
6. BVH exists but no frustum culling performed

---

### 4. GRAPH MODULE (Visual Programming)

**Location:** `src/graph/`, `include/void_engine/graph/`

**Status:** 100% ORPHANED (~1,662 lines)

| File | Lines | Content |
|------|-------|---------|
| `types.cpp` | 99 | Pin validation, type conversion |
| `node.cpp` | 897 | EventNode, FunctionNode, FlowControl, Math, Conversion |
| `execution.cpp` | 666 | GraphExecutor, GraphCompiler, VM execution |

**Complete Feature Set (Never Used):**
```cpp
// Node Types Implemented:
EventNode, FunctionNode, VariableNode
BranchNode, SequenceNode, ForLoopNode, WhileLoopNode
DelayNode, GateNode, FlipFlopNode
MathNode (50+ operations), ConversionNode
CommentNode, RerouteNode, SubgraphNode

// Execution Modes:
GraphExecutor     // Direct interpretation with stack
GraphCompiler     // Bytecode compilation
CompiledGraphExecutor  // VM-based execution

// Debugging Features:
Breakpoints, step-over, step-into

// INTEGRATION: NONE
// Zero graph instances created anywhere in codebase
```

---

### 5. C++ HOT RELOAD SYSTEM

**Location:** `src/cpp/`

**Status:** 95% ORPHANED (~1,050 lines)

| File | Lines | Content |
|------|-------|---------|
| `hot_reload.cpp` | 743 | FileWatcher, StatePreserver, HotReloader |
| `system.cpp` | 307 | CppSystem, compiler integration |

**Complete Feature Set (Never Called):**
```cpp
// FileWatcher (lines 18-260):
- Watches for .cpp/.hpp/.h changes
- Debounce logic for multiple rapid changes
- Directory traversal

// StatePreserver (lines 266-379):
- Serializes state before reload
- Restores state after reload
- Component registration

// HotReloader (lines 385-741):
- Full compilation pipeline
- Module loading/unloading
- State migration

// CppSystem (lines 1-307):
- Compiler configuration
- Module source registration
- Hot reload management

// INTEGRATION: NONE
CppSystem::initialize() never called from engine
```

---

### 6. SHELL MODULE

**Location:** `src/shell/`

**Status:** 100% ORPHANED (~600+ lines)

| File | Content |
|------|---------|
| `shell.cpp` | Singleton ShellSystem, REPL initialization |
| `parser.cpp` | Command parsing |
| `command.cpp` | Command registry |
| `session.cpp` | Session management |
| `remote.cpp` | Network server for remote connections |
| `builtins.cpp` | Built-in commands |

**Features (Never Activated):**
- Interactive REPL with history
- Command registration/execution
- Multiple shell sessions
- Remote network access
- Built-in command library

**Integration:** ShellSystem never initialized from Engine

---

### 7. SCRIPTING MODULE (WASM)

**Location:** `src/scripting/`

**Status:** 80% ORPHANED (~200 lines)

| File | Lines | Content |
|------|-------|---------|
| `system.cpp` | 204 | WasmRuntime, HostApi, PluginRegistry |

**Features (Never Connected):**
```cpp
ScriptingSystem::instance()  // Singleton - never retrieved
WasmRuntime creation         // Never triggered
HostApi registration         // Never called
PluginRegistry               // Never populated
Event bus integration        // No subscribers
```

---

### 8. ASSET REMOTE LOADING

**Location:** `src/asset/remote.cpp`, `http_client.cpp`, `websocket_client.cpp`

**Status:** 100% ORPHANED (~300 lines)

**Complete Features (Never Used):**
```cpp
// AsyncTaskPool - Full async infrastructure
// RemoteAssetSource - HTTP/WebSocket loading
// HttpClient - Connection pooling
// WebSocketClient - Bidirectional communication

// INTEGRATION: NONE
// Local asset loading works, remote never instantiated
```

---

### 9. SERVICES MODULE ISSUES

**Location:** `src/services/`, `include/void_engine/services/`

#### ACTIVE
- ServiceRegistry - Production-ready
- SessionManager - Fully implemented

#### ISSUES

| Issue | Location | Description |
|-------|----------|-------------|
| **Duplicate EventBus** | `services/event_bus.hpp` | Parallel implementation to void_event::EventBus |
| **Unused Global Instances** | `services.cpp` | Created at init but never retrieved |

**Duplicate EventBus:**
```cpp
// ACTIVE: void_event::EventBus (lock-free, templated)
// ORPHANED: void_services::EventBus (priority queues, type-erased)
// The void_services version is never instantiated
```

---

### 10. EMPTY STUB FILES

**Location:** Various `stub.cpp` files

| File | Status | Action |
|------|--------|--------|
| `src/shell/stub.cpp` | Empty placeholder | DELETE |
| `src/editor/stub.cpp` | Empty placeholder | DELETE |
| `src/runtime/stub.cpp` | Empty placeholder | DELETE |
| `src/asset/stub.cpp` | Valid (header-only pattern) | KEEP |
| `src/memory/stub.cpp` | Valid (header-only pattern) | KEEP |
| `src/compositor/stub.cpp` | Valid (200 lines real code) | KEEP |
| `src/event/stub.cpp` | Valid (65 lines real code) | KEEP |
| `src/presenter/stub.cpp` | Valid (59 lines real code) | KEEP |

---

## Game-Specific Modules Status

These modules were analyzed and found to be **FULLY INTEGRATED** (not orphaned):

| Module | Status | Integration Point |
|--------|--------|-------------------|
| AI | ✅ INTEGRATED | GameStateCore, plugin API |
| Combat | ✅ INTEGRATED | GameStateCore, state stores |
| Inventory | ✅ INTEGRATED | GameStateCore, state stores |
| Triggers | ✅ INTEGRATED | Scene parser, physics queries |
| GameState | ✅ CENTRAL HUB | Owns all state stores |
| HUD | ✅ INTEGRATED | WidgetStateCore, bindings |

---

## Architecture Observations

### Two-Tier Scheduler Mismatch
1. **Kernel** has Stage-based scheduling (Input, Update, Render, etc.)
2. **ECS** has SystemScheduler with parallel batching
3. **Current:** ECS systems execute via kernel's "ecs_progress" stage
4. **Issue:** ECS's own scheduler infrastructure is pre-planned but unused

### Premature Abstraction Pattern
- Systems designed independently before integration requirements clear
- Feature flags disabled (hot_reload = false), blocking C++ system use
- Render graph/passes defined but pipeline uses direct GL calls

### Quality vs Integration
- All orphaned code is **production-grade** with error handling
- Code is **well-structured** with clear separation of concerns
- Issue is **disconnection**, not quality

---

## Recommendations

### Immediate Cleanup (Safe to Delete)

| Category | Files | Lines |
|----------|-------|-------|
| Empty stubs | shell/editor/runtime stub.cpp | ~30 |
| Duplicate EventBus | services/event_bus.hpp | ~200 |
| **Total Safe Delete** | | **~230** |

### Consider Removing (If Features Not Planned)

| Category | Files | Lines |
|----------|-------|-------|
| Graph system | graph/*.cpp | 1,662 |
| Shell system | shell/*.cpp | 600+ |
| C++ hot reload | cpp/*.cpp | 1,050 |
| Remote assets | asset/remote.cpp, http_client.cpp, websocket_client.cpp | 300 |
| Render orphans | animation.cpp, post_process.cpp, temporal_effects.cpp, lod.cpp, etc. | 8,500+ |
| Kernel orphans | supervisor, sandbox | 1,450 |
| **Total Consider** | | **~13,562** |

### Keep But Document (Intentional Future Features)

| Category | Files | Reason |
|----------|-------|--------|
| ECS Snapshot | snapshot.hpp | Hot-reload state preservation (needs wiring) |
| Render passes | Various | Deferred/advanced rendering (needs integration) |

---

## Conclusion

The void_engine codebase contains approximately **13,000+ lines of high-quality, production-ready code that is 100% orphaned**. These systems were built independently with sophisticated implementations but no integration into the main Engine lifecycle.

The active rendering pipeline is a simple forward renderer using ~2,700 lines of `render_systems.cpp`, while ~8,500 lines of advanced rendering features (skeletal animation, post-processing, temporal effects, LOD, shadows, instancing, deferred rendering) sit completely disconnected.

**Strategic Decision Required:**
1. **Integrate:** Wire orphaned systems into initialization and feature flags
2. **Archive:** Move to legacy branch for potential future use
3. **Delete:** Remove if features definitively not needed

The code quality is excellent - the issue is purely architectural disconnection.
