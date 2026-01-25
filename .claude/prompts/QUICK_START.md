# Quick Start - Copy & Paste Instructions

## Phase 1: Open 2 Terminals

---

### Terminal 1 - Core Module

**COPY EVERYTHING BELOW AND PASTE INTO NEW CLAUDE SESSION:**

```
Think deeply and thoroughly about this task before implementing. Take your time to understand the codebase patterns first.

You are implementing the void_core module for Void Engine. This is PRODUCTION code that will be reviewed by John Carmack-level engineers.

## CRITICAL REQUIREMENTS - READ CAREFULLY

### 1. HOT-RELOAD IS MANDATORY
Every stateful class MUST implement void_core::HotReloadable:
- snapshot() - Capture ALL state as binary
- restore() - Restore state from binary
- prepare_reload() - Release transient resources (GPU handles, file handles)
- finish_reload() - Rebuild transient resources

### 2. NO FAKING
- NO stub implementations
- NO TODO comments
- NO placeholder code
- Every method must be FULLY implemented
- If you can't implement something, explain why and what's needed

### 3. BINARY SERIALIZATION
All snapshots use binary format:
- 4-byte MAGIC number (unique per class)
- 4-byte VERSION number
- Binary payload (no JSON, no text)

### 4. PERFORMANCE
- NO allocations in update/tick/render paths
- Pre-allocate all buffers
- Use RAII for all resources
- No raw new/delete

### 5. ERROR HANDLING
- Use Result<T> for all fallible operations
- Never throw exceptions in hot paths

---

## YOUR TASK

**Your files:** `src/core/` and `include/void_engine/core/` ONLY.
**Do NOT modify any other directories.**

**Implement these headers:**
1. Read `include/void_engine/core/hot_reload.hpp` - understand the HotReloadable interface
2. Read `include/void_engine/core/error.hpp` → create `src/core/error.cpp`
3. Read `include/void_engine/core/handle.hpp` → create `src/core/handle.cpp`
4. Read `include/void_engine/core/id.hpp` → create `src/core/id.cpp`
5. Read `include/void_engine/core/log.hpp` → create `src/core/log.cpp`
6. Read `include/void_engine/core/type_registry.hpp` → create `src/core/type_registry.cpp`
7. Read `include/void_engine/core/hot_reload.hpp` → create `src/core/hot_reload.cpp`
8. Read `include/void_engine/core/plugin.hpp` → create `src/core/plugin.cpp`
9. Read `include/void_engine/core/version.hpp` → create `src/core/version.cpp`

---

## PROCESS

1. **READ FIRST**: Read each header completely before implementing
2. **CHECK LEGACY**: Look at `legacy/crates/void_core/src/` for behavior reference
3. **IMPLEMENT FULLY**: Every method, no stubs
4. **HOT-RELOAD**: Ensure snapshot/restore works for all stateful classes

---

## WHEN COMPLETE - CREATE THESE DELIVERABLES

### 1. List all files created
```
src/core/error.cpp
src/core/handle.cpp
...
```

### 2. CMakeLists.txt additions
```cmake
target_sources(void_core PRIVATE
    error.cpp
    handle.cpp
    ...
)
```

### 3. Create Mermaid Diagram: `doc/diagrams/core_integration.md`
Include:
- Class diagram showing inheritance and composition
- Sequence diagram showing hot-reload flow (snapshot → serialize → restore)
- Dependency graph showing what core provides to other modules

### 4. Create Validation Report: `doc/validation/core_validation.md`
Include:
- [ ] All headers have .cpp implementations
- [ ] All stateful classes implement HotReloadable
- [ ] Snapshot/restore cycle tested for each class
- [ ] No raw pointers in snapshots (handles only)
- [ ] Binary serialization verified (no JSON)
- [ ] No allocations in hot paths
- [ ] Compiles without warnings

---

## START NOW

Begin by reading:
include/void_engine/core/hot_reload.hpp

Then implement each header systematically. Take your time and do it right.
```

---

### Terminal 2 - IR Module

**COPY EVERYTHING BELOW AND PASTE INTO NEW CLAUDE SESSION:**

```
Think deeply and thoroughly about this task before implementing. Take your time to understand the codebase patterns first.

You are implementing the void_ir module for Void Engine. This is PRODUCTION code that will be reviewed by John Carmack-level engineers.

## CRITICAL REQUIREMENTS - READ CAREFULLY

### 1. HOT-RELOAD IS MANDATORY
Every stateful class MUST implement void_core::HotReloadable:
- snapshot() - Capture ALL state as binary
- restore() - Restore state from binary
- prepare_reload() - Release transient resources
- finish_reload() - Rebuild transient resources

### 2. NO FAKING
- NO stub implementations
- NO TODO comments
- NO placeholder code
- Every method must be FULLY implemented

### 3. BINARY SERIALIZATION
All snapshots use binary format:
- 4-byte MAGIC number (unique per class, e.g., 0x50425553 for "PBUS")
- 4-byte VERSION number
- Binary payload

### 4. PERFORMANCE
- NO allocations in patch dispatch paths
- Lock-free queues where applicable
- Pre-allocate all buffers

### 5. ERROR HANDLING
- Use Result<T> for all fallible operations
- Transaction rollback on failure

---

## YOUR TASK

**Your files:** `src/ir/` and `include/void_engine/ir/` ONLY.
**Do NOT modify any other directories.**

**Implement these headers:**
1. Read `include/void_engine/ir/patch.hpp` → create `src/ir/patch.cpp`
2. Read `include/void_engine/ir/value.hpp` → create `src/ir/value.cpp`
3. Read `include/void_engine/ir/bus.hpp` → create `src/ir/bus.cpp`
4. Read `include/void_engine/ir/batch.hpp` → create `src/ir/batch.cpp`
5. Read `include/void_engine/ir/namespace.hpp` → create `src/ir/namespace.cpp`
6. Read `include/void_engine/ir/transaction.hpp` → create `src/ir/transaction.cpp`
7. Read `include/void_engine/ir/validation.hpp` → create `src/ir/validation.cpp`
8. Read `include/void_engine/ir/ir.hpp` → create `src/ir/ir.cpp`

---

## PROCESS

1. **READ FIRST**: Read each header completely before implementing
2. **CHECK LEGACY**: Look at `legacy/crates/void_ir/src/` for behavior reference
3. **IMPLEMENT FULLY**: Every method, no stubs
4. **HOT-RELOAD**: PatchBus, BatchOptimizer, Transaction must be hot-reloadable

---

## WHEN COMPLETE - CREATE THESE DELIVERABLES

### 1. List all files created

### 2. CMakeLists.txt additions

### 3. Create Mermaid Diagram: `doc/diagrams/ir_integration.md`
Include:
- Class diagram showing Patch, PatchBus, Transaction relationships
- Sequence diagram showing patch flow (create → validate → dispatch)
- Hot-reload sequence diagram

### 4. Create Validation Report: `doc/validation/ir_validation.md`
Include all validation checks as checkboxes

---

## START NOW

Begin by reading:
include/void_engine/ir/patch.hpp

Then implement each header systematically.
```

---

## After Phase 1: Build & Verify

```bash
cmake -B build && cmake --build build
```

Fix any errors before proceeding to Phase 2.

---

## Phase 2: Open 5 Terminals

### Terminal 3 - ECS Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_ecs module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- World, ArchetypeStorage must implement HotReloadable
- Entity IDs are generational (index + generation) - must survive reload
- Component data serialized as binary

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### PERFORMANCE
- Archetype-based storage (Structure of Arrays)
- Cache-friendly iteration
- Sparse sets for entity lookup
- NO allocations during iteration

---

## YOUR TASK

**Your files:** `src/ecs/` and `include/void_engine/ecs/` ONLY.

**Implement:**
1. `archetype.hpp` → `src/ecs/archetype.cpp`
2. `bundle.hpp` → `src/ecs/bundle.cpp`
3. `component.hpp` → `src/ecs/component.cpp`
4. `entity.hpp` → `src/ecs/entity.cpp`
5. `hierarchy.hpp` → `src/ecs/hierarchy.cpp`
6. `query.hpp` → `src/ecs/query.cpp`

**Read first:** `include/void_engine/ecs/entity.hpp`
**Check legacy:** `legacy/crates/void_ecs/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions
3. `doc/diagrams/ecs_integration.md` - Class diagram, hot-reload flow, archetype storage diagram
4. `doc/validation/ecs_validation.md` - All validation checks
```

---

### Terminal 4 - Asset Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_asset module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- AssetServer must implement HotReloadable
- Serialize asset MANIFEST (paths, types, ref counts), NOT asset data
- Asset handles survive reload

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### 3-TIER CACHE
- Hot cache: in-memory, recently used
- Warm cache: compressed, quick decompress
- Cold: disk, async load

### PERFORMANCE
- Async loading with callbacks
- Reference counting
- Memory budget enforcement

---

## YOUR TASK

**Your files:** `src/asset/` and `include/void_engine/asset/` ONLY.

**Implement:**
1. `asset.hpp` → `src/asset/asset.cpp`
2. `cache.hpp` → `src/asset/cache.cpp`
3. `handle.hpp` → `src/asset/handle.cpp`
4. `loader.hpp` → `src/asset/loader.cpp`
5. `server.hpp` → `src/asset/server.cpp`
6. `storage.hpp` → `src/asset/storage.cpp`

**Read first:** `include/void_engine/asset/asset.hpp`
**Check legacy:** `legacy/crates/void_asset/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions
3. `doc/diagrams/asset_integration.md` - 3-tier cache diagram, async loading flow, hot-reload sequence
4. `doc/validation/asset_validation.md`
```

---

### Terminal 5 - Physics Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_physics module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- PhysicsWorld must implement HotReloadable
- Serialize: body transforms, velocities, constraint state
- Broadphase structure rebuilt on restore (not serialized)

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### PERFORMANCE
- Spatial partitioning (BVH or grid) for broadphase
- SIMD for collision math where possible
- NO allocations in simulation step

---

## YOUR TASK

**Your files:** `src/physics/` and `include/void_engine/physics/` ONLY.

**Implement:**
1. `broadphase.hpp` → `src/physics/broadphase.cpp`
2. `collision.hpp` → `src/physics/collision.cpp`
3. `solver.hpp` → `src/physics/solver.cpp`
4. `physics.hpp` → `src/physics/physics.cpp`

**Read first:** `include/void_engine/physics/body.hpp`
**Check legacy:** `legacy/crates/void_physics/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions
3. `doc/diagrams/physics_integration.md` - Broadphase diagram, collision pipeline, hot-reload sequence
4. `doc/validation/physics_validation.md`
```

---

### Terminal 6 - Services Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_services module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- ServiceManager must implement HotReloadable
- Serialize: registered services, service states, dependencies

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### SERVICE LIFECYCLE
- start() / stop() / update()
- Dependency ordering (services start in dependency order)
- Health monitoring

---

## YOUR TASK

**Your files:** `src/services/` and `include/void_engine/services/` ONLY.

**Implement:**
1. `event_bus.hpp` → `src/services/event_bus.cpp`
2. `service.hpp` → `src/services/service.cpp`
3. `services.hpp` → `src/services/services.cpp`

**Read first:** `include/void_engine/services/service.hpp`
**Check legacy:** `legacy/crates/void_services/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions
3. `doc/diagrams/services_integration.md` - Service lifecycle, dependency graph, hot-reload sequence
4. `doc/validation/services_validation.md`
```

---

### Terminal 7 - Presenter Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_presenter module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- Presenter must implement HotReloadable
- Serialize: presentation config, swapchain settings
- GPU resources HANDLES only (recreated on restore)
- prepare_reload() releases GPU resources
- finish_reload() recreates swapchain

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### PLATFORM SUPPORT
- Null backend: always available (for testing)
- WGPU backend: #if defined(VOID_HAS_WGPU)
- DRM backend: #if defined(__linux__) - Linux direct rendering

### PERFORMANCE
- Frame timing with high-resolution clock
- Triple buffering support
- No allocations in present path

---

## YOUR TASK

**Your files:** `src/presenter/` and `include/void_engine/presenter/` ONLY.

**Implement:**
1. `presenter.hpp` → `src/presenter/presenter.cpp`
2. `multi_backend_presenter.hpp` → `src/presenter/multi_backend_presenter.cpp`
3. `swapchain.hpp` → `src/presenter/swapchain.cpp`
4. `surface.hpp` → `src/presenter/surface.cpp`
5. `frame.hpp` → `src/presenter/frame.cpp`
6. `timing.hpp` → `src/presenter/timing.cpp`
7. `rehydration.hpp` → `src/presenter/rehydration.cpp`
8. `presenter_module.hpp` → `src/presenter/presenter_module.cpp`
9. `backends/null_backend.hpp` → `src/presenter/backends/null_backend.cpp`
10. `backends/wgpu_backend.hpp` → `src/presenter/backends/wgpu_backend.cpp`
11. `drm.hpp` → `src/presenter/drm_presenter.cpp` (Linux only)

**Read first:** `include/void_engine/presenter/backend.hpp`
**Check legacy:** `legacy/crates/void_presenter/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions (with platform conditionals)
3. `doc/diagrams/presenter_integration.md` - Backend selection, frame presentation flow, hot-reload sequence
4. `doc/validation/presenter_validation.md`
```

---

## After Phase 2: Build & Verify

```bash
cmake -B build && cmake --build build
```

---

## Phase 3: Open 2 Terminals

### Terminal 8 - Render Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_render module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- RenderSystem must implement HotReloadable
- GPU resources are HANDLES (not raw pointers)
- prepare_reload() releases all GPU resources (textures, buffers, shaders)
- finish_reload() recreates from cached CPU data
- Serialize: render config, camera state, active lights (NOT GPU data)

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### PERFORMANCE
- Batch similar draw calls
- Frustum culling
- Material sorting to minimize state changes
- Pre-allocated command buffers
- NO allocations in render loop

---

## YOUR TASK

**Your files:** `src/render/` and `include/void_engine/render/` ONLY.

**Implement:**
1. `camera.hpp` → `src/render/camera.cpp`
2. `light.hpp` → `src/render/light.cpp`
3. `material.hpp` → `src/render/material.cpp`
4. `mesh.hpp` → `src/render/mesh.cpp`
5. `pass.hpp` → `src/render/pass.cpp`
6. `resource.hpp` → `src/render/resource.cpp`
7. `shadow.hpp` → `src/render/shadow.cpp`
8. `debug.hpp` → `src/render/debug.cpp`
9. `texture.hpp` → `src/render/texture.cpp`
10. `render.hpp` → `src/render/render.cpp`

**Read first:** `include/void_engine/render/render.hpp`
**Check legacy:** `legacy/crates/void_render/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions
3. `doc/diagrams/render_integration.md` - Render pipeline, material system, hot-reload (GPU resource handling)
4. `doc/validation/render_validation.md`
```

---

### Terminal 9 - Compositor Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing.

You are implementing the void_compositor module for Void Engine. PRODUCTION code, Carmack-level review.

## CRITICAL REQUIREMENTS

### HOT-RELOAD MANDATORY
- Compositor must implement HotReloadable
- Serialize: layer stack, HDR config, VRR state, output config

### NO FAKING
- NO stubs, NO TODOs, FULL implementations only

### FEATURES
- Layer-based composition
- HDR tone mapping (PQ, HLG)
- Variable Refresh Rate (VRR/FreeSync/G-Sync)
- Multi-output management

### PERFORMANCE
- No allocations in composition path
- Efficient layer blending

---

## YOUR TASK

**Your files:** `src/compositor/` and `include/void_engine/compositor/` ONLY.

**Implement:**
1. `frame.hpp` → `src/compositor/frame.cpp`
2. `hdr.hpp` → `src/compositor/hdr.cpp`
3. `output.hpp` → `src/compositor/output.cpp`
4. `vrr.hpp` → `src/compositor/vrr.cpp`
5. `rehydration.hpp` → `src/compositor/rehydration.cpp`
6. `input.hpp` → `src/compositor/input.cpp`
7. `compositor_module.hpp` → `src/compositor/compositor_module.cpp`

**Read first:** `include/void_engine/compositor/compositor.hpp`
**Check legacy:** `legacy/crates/void_compositor/src/`

## DELIVERABLES
1. All .cpp files
2. CMakeLists.txt additions
3. `doc/diagrams/compositor_integration.md` - Layer composition, HDR pipeline, VRR flow, hot-reload sequence
4. `doc/validation/compositor_validation.md`
```

---

## After Phase 3: Final Build & Test

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/void_engine
```

---

## What Each Prompt Guarantees

| Requirement | How It's Enforced |
|-------------|-------------------|
| Ultrathink | "Think deeply and thoroughly" at start |
| Hot-reload | Explicit HotReloadable requirements |
| No faking | "NO stubs, NO TODOs, FULL implementations" |
| Full integration | Must implement every header listed |
| Mermaid diagrams | Explicit deliverable with diagram types |
| Validation | Explicit deliverable with checkboxes |
| Performance | Listed requirements, "NO allocations" |
| Quality | "Carmack-level review" framing |
