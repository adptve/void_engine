# void_engine Feature Inventory & Orphaned Code Guidance (Code-Verified)

## Scope

This document is derived from **the current code** (not just architecture docs). It summarizes the active runtime flow, registered systems, and key subsystems visible in the sources, then identifies major orphaned modules by comparing what is implemented to what is actually wired into the runtime lifecycle.

---

## Feature Inventory (Verified in Code)

### Runtime & Lifecycle
- **Runtime-driven entry point**: `main.cpp` delegates lifecycle control to `void_runtime::Runtime`, including initialization, the frame loop, and shutdown.【F:src/main.cpp†L1-L188】
- **Boot sequence and core initialization**: `Runtime::initialize()` wires the kernel, infrastructure, package system, platform, render, I/O, and simulation phases in that order, then starts the kernel if initialization succeeds.【F:src/runtime/runtime.cpp†L79-L214】
- **Frame loop**: The runtime tracks timing, handles frame pacing, and calls `execute_frame()` each iteration of the main loop.【F:src/runtime/runtime.cpp†L216-L305】
- **Stage execution order**: `Runtime::execute_frame()` explicitly runs kernel stages in the order Input → HotReloadPoll → EventDispatch → Update → FixedUpdate → PostFixed → RenderPrepare → Render → UI → Audio → Streaming.【F:src/runtime/runtime.cpp†L886-L914】

### Kernel & Stage Scheduler
- **Kernel responsibilities**: The kernel interface owns module loading, the supervisor tree, sandbox creation, hot-reload orchestration, and stage scheduling APIs (register/run stages, enable hot reload, etc.).【F:include/void_engine/kernel/kernel.hpp†L1-L170】

### Package System & World Lifecycle
- **Package-backed world loading**: `Runtime::load_world()` uses `void_package::WorldComposer` with `WorldLoadOptions` and `PackageRegistry` checks to load worlds and spawn entities via the package system (no legacy fallback).【F:src/runtime/runtime.cpp†L318-L414】
- **Package initialization**: The runtime creates and wires the package registry, load context, world composer, prefab registry, component schema registry, definition registry, widget manager, layer applier, and ECS world, then scans content/plugin directories for packages.【F:src/runtime/runtime.cpp†L436-L604】
- **Component schema integration**: The ECS component registry is explicitly connected to the `ComponentSchemaRegistry`, and the prefab registry is wired to schema registration for JSON-driven instantiation.【F:src/runtime/runtime.cpp†L468-L560】

### Rendering & ECS Render Pipeline
- **Render pipeline registration**: `register_engine_render_systems()` wires the ECS render systems into kernel stages with explicit priorities (TransformSystem, CameraSystem, LightSystem, RenderPrepareSystem, RenderSystem).【F:src/runtime/runtime.cpp†L1464-L1525】
- **RenderContext resource**: `init_render_context()` builds a `RenderContext` and inserts it as an ECS resource before render systems run.【F:src/runtime/runtime.cpp†L1527-L1569】
- **ECS render systems**: `render_systems.cpp` defines core render systems (model loading, transform propagation, camera/light collection, render queue building, and draw execution).【F:src/render/render_systems.cpp†L1-L240】

---

## Orphaned Code Findings (Based on Runtime Wiring)

The following subsystems are implemented but **not initialized or registered** by the runtime lifecycle shown above, indicating they are currently orphaned from the active application flow.

### Kernel Supervisor & Sandbox
- The kernel interface exposes **SupervisorTree** and **Sandbox** management APIs, but `Runtime::initialize()` and related boot phases never create or attach supervisors/sandboxes, so these capabilities are not exercised in the live runtime flow.【F:include/void_engine/kernel/kernel.hpp†L41-L140】【F:src/runtime/runtime.cpp†L79-L214】

### C++ Hot Reload System (void_cpp)
- The C++ hot reload system implements file watching and reload helpers (e.g., `FileWatcher::watch()`, directory scanning, debounce, and change detection), yet it is not invoked by runtime initialization or kernel hot-reload wiring in `Runtime::init_kernel()`/`execute_frame()`.【F:src/cpp/hot_reload.cpp†L1-L186】【F:src/runtime/runtime.cpp†L360-L424】

### Graph / Visual Scripting Module
- The graph executor implements scheduling, latent actions, and execution control (`GraphExecutor::start()`, `update()`, and execution loop) but is never instantiated or registered by the runtime lifecycle phases in `Runtime::initialize()` or `init_simulation()`.【F:src/graph/execution.cpp†L1-L185】【F:src/runtime/runtime.cpp†L79-L826】

### Shell / REPL Module
- `ShellSystem` supports command registration, sessions, history, and a remote server, but it is not initialized anywhere in the runtime boot sequence (`init_io()` only registers placeholder input/audio systems).【F:src/shell/shell.cpp†L1-L191】【F:src/runtime/runtime.cpp†L744-L806】

### Remote Asset Loading
- The remote asset source provides async task pooling, HTTP/WebSocket transport, and caching for remote content, yet the runtime package/asset initialization path does not wire this into `void_asset` or the package registry flow.【F:src/asset/remote.cpp†L1-L200】【F:src/runtime/runtime.cpp†L436-L604】

---

## Guidance: Merge vs. Remove (Based on Code Evidence)

The codebase’s preservation rules prioritize advanced implementations. Based on current wiring, the following guidance aligns with what exists in code today.

### Strong Candidates to Merge (Advanced Features)
1. **C++ Hot Reload System**
   - The C++ hot reload system is fully implemented and should be integrated into the kernel hot-reload stage to honor the engine’s hot-reload goals.【F:src/cpp/hot_reload.cpp†L1-L186】【F:src/runtime/runtime.cpp†L360-L424】
2. **Kernel Supervisor & Sandbox**
   - Supervisor/sandbox APIs exist in the kernel but are not used by runtime; integrate them into runtime configuration if fault tolerance or isolation is on the roadmap.【F:include/void_engine/kernel/kernel.hpp†L41-L140】【F:src/runtime/runtime.cpp†L79-L214】
3. **Remote Asset Loading**
   - Remote asset loading is a concrete implementation (HTTP/WebSocket + async queue) and should be wired into asset/package ingestion if remote content delivery is required.【F:src/asset/remote.cpp†L1-L200】【F:src/runtime/runtime.cpp†L436-L604】

### Conditional Candidates (Merge if Roadmap Requires)
1. **Graph / Visual Scripting**
   - The graph executor is complete but unused in runtime; merge only if creator tooling or visual scripting is in scope, otherwise archive to keep code ready but not blocking the mainline runtime.【F:src/graph/execution.cpp†L1-L185】【F:src/runtime/runtime.cpp†L79-L826】
2. **Shell / REPL**
   - The shell provides a full REPL and remote admin channel, but is not initialized by `Runtime::init_io()`; merge if live ops tooling is needed, otherwise archive or keep behind a build flag.【F:src/shell/shell.cpp†L1-L191】【F:src/runtime/runtime.cpp†L744-L806】

---

## Recommended Next Steps

1. **Integrate hot reload end-to-end** by wiring the C++ reload system into kernel hot reload polling and ensuring state preservation is used for live reloads.【F:src/cpp/hot_reload.cpp†L1-L186】【F:src/runtime/runtime.cpp†L360-L424】
2. **Decide on creator tooling scope** (graph scripting, shell/REPL). If required, add explicit initialization in runtime boot phases; if not, archive to reduce maintenance overhead.【F:src/graph/execution.cpp†L1-L185】【F:src/shell/shell.cpp†L1-L191】
3. **Plan remote asset ingestion** by threading `RemoteAssetSource` into the package/asset pipeline for remote content delivery, or explicitly document that only local assets are supported today.【F:src/asset/remote.cpp†L1-L200】【F:src/runtime/runtime.cpp†L436-L604】
