# Quick Start - Copy & Paste Instructions

## CRITICAL LESSONS FROM PHASE 1

**The following mistakes were made in Phase 1. DO NOT REPEAT THEM:**

1. **DO NOT redefine structs/classes** - If a struct is in a header, use it. Never copy struct definitions into .cpp files.
2. **Include ALL necessary headers** - Always add `#include <cstring>`, `<algorithm>`, `<set>`, `<unordered_set>`, `<atomic>`, `<vector>` as needed.
3. **Check actual APIs before using them** - Read the header to see if it's `Version.major` (member) or `Version.major()` (method).
4. **Result/Err pattern** - Use `return void_core::Err<ReturnType>("message");` for errors.
5. **Forward declare cross-file functions** - If function A in file1.cpp uses function B from file2.cpp, add a forward declaration.

---

## Phase 1: Foundation (COMPLETED)

Phase 1 (void_core, void_ir) is complete. Proceed to Phase 2.

---

## Phase 2: Open 5 Terminals

**IMPORTANT LESSON: Phase 2 modules were initially implemented as "ghost code" - they existed but weren't integrated into the engine. Each module SHOULD modify src/main.cpp to wire into the render loop. If they don't, you'll need to integrate them manually afterward.**

**Already integrated in src/main.cpp:**
- void_ecs: World, LiveSceneManager, AnimationSystem::update()
- void_asset: AssetServer with TextureLoader, ModelLoader, process(), drain_events()
- void_physics: PhysicsWorld with step(delta_time), collision callbacks
- void_services: ServiceRegistry, EventBus with publish/subscribe
- void_presenter: FrameTiming for delta_time() and average_fps()

### Terminal 3 - ECS Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_ecs module for Void Engine. This code MUST compile without errors on first attempt.

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- If a struct/class/enum exists in a header, USE IT - do NOT copy it to .cpp
- Your .cpp files only implement methods, not redefine types
- If you need a type, #include the header that defines it

### RULE 2: CHECK APIS BEFORE USING
- ALWAYS read the header file first to understand the actual interface
- Check if members are accessed as `.name` or `.name()`
- Check the exact function signatures before implementing
- Check what includes are needed

### RULE 3: INCLUDE ALL DEPENDENCIES
Standard includes you likely need:
```cpp
#include <algorithm>    // std::remove, std::sort
#include <atomic>       // std::atomic, std::memory_order_relaxed
#include <cstring>      // std::memcpy, std::strlen
#include <set>          // std::set
#include <unordered_set> // std::unordered_set
#include <vector>       // std::vector
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
// Correct usage:
return void_core::Ok(value);           // Success with value
return void_core::Ok();                 // Success void
return void_core::Err<ReturnType>("error message");  // Error

// Check void_core::Result<T> and void_core::Error in error.hpp
```

### RULE 5: HOT-RELOAD PATTERN
```cpp
// Check void_core::HotReloadable interface in hot_reload.hpp
// Check void_core::Version - it has PUBLIC MEMBERS: major, minor, patch (NOT methods!)
bool is_compatible(const void_core::Version& v) const override {
    return v.major == MAJOR_VERSION;  // .major NOT .major()
}
```

---

## HOT-RELOAD IS MANDATORY
- World, ArchetypeStorage must implement HotReloadable
- Entity IDs are generational (index + generation) - must survive reload
- Component data serialized as binary with MAGIC + VERSION header

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only
- If something can't be implemented, explain why

## PERFORMANCE
- Archetype-based storage (Structure of Arrays)
- Cache-friendly iteration
- Sparse sets for entity lookup
- NO allocations during iteration

---

## YOUR TASK

**Your files:** `src/ecs/` and `include/void_engine/ecs/` ONLY.

**BEFORE implementing each file:**
1. Read the corresponding header file COMPLETELY
2. Note the exact types, method signatures, includes needed
3. Check what types come from other headers (void_core, void_structures)

**Implement:**
1. `archetype.hpp` → `src/ecs/archetype.cpp`
2. `bundle.hpp` → `src/ecs/bundle.cpp`
3. `component.hpp` → `src/ecs/component.cpp`
4. `entity.hpp` → `src/ecs/entity.cpp`
5. `hierarchy.hpp` → `src/ecs/hierarchy.cpp`
6. `query.hpp` → `src/ecs/query.cpp`

**Check legacy:** `legacy/crates/void_ecs/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. `doc/diagrams/ecs_integration.md` - Class diagram, hot-reload flow
4. `doc/validation/ecs_validation.md` - Validation checklist

## START NOW
Read include/void_engine/ecs/entity.hpp first.
```

---

### Terminal 4 - Asset Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_asset module for Void Engine. This code MUST compile without errors on first attempt.

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- If a struct/class/enum exists in a header, USE IT - do NOT copy it to .cpp
- Your .cpp files only implement methods, not redefine types

### RULE 2: CHECK APIS BEFORE USING
- ALWAYS read the header file first
- Check exact function signatures and member access patterns
- Verify includes needed from void_core, void_structures

### RULE 3: INCLUDE ALL DEPENDENCIES
```cpp
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>  // std::filesystem::path
#include <fstream>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
return void_core::Ok(value);
return void_core::Err<ReturnType>("error message");
```

### RULE 5: VERSION ACCESS
```cpp
// void_core::Version has PUBLIC MEMBERS, not methods
v.major  // Correct
v.major() // WRONG - will not compile
```

---

## HOT-RELOAD MANDATORY
- AssetServer must implement HotReloadable
- Serialize asset MANIFEST (paths, types, ref counts), NOT asset data
- Asset handles survive reload

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only

## 3-TIER CACHE
- Hot cache: in-memory, recently used
- Warm cache: compressed, quick decompress
- Cold: disk, async load

## PERFORMANCE
- Async loading with callbacks
- Reference counting
- Memory budget enforcement
- NO allocations in hot paths

---

## YOUR TASK

**Your files:** `src/asset/` and `include/void_engine/asset/` ONLY.

**BEFORE implementing each file:**
1. Read the header file COMPLETELY
2. Note exact types and signatures
3. Check dependencies from void_core

**Implement:**
1. `asset.hpp` → `src/asset/asset.cpp`
2. `cache.hpp` → `src/asset/cache.cpp`
3. `handle.hpp` → `src/asset/handle.cpp`
4. `loader.hpp` → `src/asset/loader.cpp`
5. `server.hpp` → `src/asset/server.cpp`
6. `storage.hpp` → `src/asset/storage.cpp`

**Check legacy:** `legacy/crates/void_asset/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. `doc/diagrams/asset_integration.md`
4. `doc/validation/asset_validation.md`

## START NOW
Read include/void_engine/asset/asset.hpp first.
```

---

### Terminal 5 - Physics Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_physics module for Void Engine. This code MUST compile without errors on first attempt.

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- If a struct/class exists in a header, USE IT from the header
- .cpp files implement methods only

### RULE 2: CHECK APIS BEFORE USING
- Read headers first, check exact signatures
- void_core::Version has .major .minor .patch MEMBERS (not methods)

### RULE 3: INCLUDE ALL DEPENDENCIES
```cpp
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
return void_core::Ok(value);
return void_core::Err<ReturnType>("message");
```

---

## HOT-RELOAD MANDATORY
- PhysicsWorld must implement HotReloadable
- Serialize: body transforms, velocities, constraint state
- Broadphase structure rebuilt on restore (transient)

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only

## PERFORMANCE
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

**Check legacy:** `legacy/crates/void_physics/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. `doc/diagrams/physics_integration.md`
4. `doc/validation/physics_validation.md`

## START NOW
Read include/void_engine/physics/body.hpp first.
```

---

### Terminal 6 - Services Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_services module for Void Engine. This code MUST compile without errors on first attempt.

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- Use types from headers, never copy definitions to .cpp

### RULE 2: CHECK APIS BEFORE USING
- Read void_core headers for Result, Error, Version patterns
- Version.major is a MEMBER, not a method

### RULE 3: INCLUDE ALL DEPENDENCIES
```cpp
#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
return void_core::Ok(value);
return void_core::Err<ReturnType>("message");
```

---

## HOT-RELOAD MANDATORY
- ServiceManager must implement HotReloadable
- Serialize: registered services, states, dependencies

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only

## SERVICE LIFECYCLE
- start() / stop() / update()
- Dependency ordering
- Health monitoring

---

## YOUR TASK

**Your files:** `src/services/` and `include/void_engine/services/` ONLY.

**Implement:**
1. `event_bus.hpp` → `src/services/event_bus.cpp`
2. `service.hpp` → `src/services/service.cpp`
3. `services.hpp` → `src/services/services.cpp`

**Check legacy:** `legacy/crates/void_services/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. `doc/diagrams/services_integration.md`
4. `doc/validation/services_validation.md`

## START NOW
Read include/void_engine/services/service.hpp first.
```

---

### Terminal 7 - Presenter Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_presenter module for Void Engine. This code MUST compile without errors on first attempt.

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- Use types from headers, never copy definitions to .cpp

### RULE 2: CHECK APIS BEFORE USING
- Read void_core headers for Result, Error, HotReloadable, Version
- Version.major is a MEMBER (.major), not a method (.major())

### RULE 3: INCLUDE ALL DEPENDENCIES
```cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
return void_core::Ok(value);
return void_core::Err<ReturnType>("message");
```

---

## HOT-RELOAD MANDATORY
- Presenter must implement HotReloadable
- Serialize: presentation config, swapchain settings
- GPU resources are HANDLES (recreated on restore)
- prepare_reload() releases GPU resources
- finish_reload() recreates swapchain

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only

## PLATFORM SUPPORT
- Null backend: always available
- WGPU backend: #if defined(VOID_HAS_WGPU)
- DRM backend: #if defined(__linux__)

## PERFORMANCE
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
8. `backends/null_backend.hpp` → `src/presenter/backends/null_backend.cpp`
9. `backends/wgpu_backend.hpp` → `src/presenter/backends/wgpu_backend.cpp`

**Check legacy:** `legacy/crates/void_presenter/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. `doc/diagrams/presenter_integration.md`
4. `doc/validation/presenter_validation.md`

## START NOW
Read include/void_engine/presenter/backend.hpp first.
```

---

## After Phase 2: Build & Verify

```bash
cmake -B build && cmake --build build
```

---

## Phase 3: Open 2 Terminals

**CRITICAL: Phase 3 modules MUST be integrated into src/main.cpp, NOT ghost code!**

The following modules from Phase 2 are already integrated into main.cpp:
- void_ecs (World, LiveSceneManager, AnimationSystem)
- void_asset (AssetServer, TextureLoader, ModelLoader)
- void_physics (PhysicsWorld with step() in main loop)
- void_services (ServiceRegistry, EventBus)
- void_presenter (FrameTiming for delta time and statistics)

Phase 3 modules MUST follow the same pattern.

### Terminal 8 - Render Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_render module for Void Engine. This code MUST compile without errors on first attempt.

## CRITICAL: ENGINE INTEGRATION REQUIRED

**This is NOT a standalone module. It MUST integrate into src/main.cpp.**

The current main.cpp already has a basic SceneRenderer. Your task is to:
1. Fix any compilation errors in the render module headers
2. Ensure the render module integrates cleanly with the existing SceneRenderer
3. Add any missing implementations that the SceneRenderer depends on

**Look at src/main.cpp to see how SceneRenderer is already used:**
- `renderer.initialize(window)` - Initialize with GLFW window
- `renderer.load_scene(scene)` - Load scene data
- `renderer.update(delta_time)` - Update each frame
- `renderer.render()` - Render the scene
- `renderer.stats()` - Get render statistics
- `renderer.camera().orbit/pan/zoom` - Camera controls
- `renderer.reload_shaders()` - Hot-reload shaders

**Your implementations must support this existing API.**

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- Types in headers are used, not copied to .cpp files

### RULE 2: CHECK APIS BEFORE USING
- Read void_core headers for exact interfaces
- void_core::Version has MEMBERS: .major .minor .patch (NOT methods)

### RULE 3: INCLUDE ALL DEPENDENCIES
```cpp
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <vector>
#include <unordered_map>
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
return void_core::Ok(value);
return void_core::Err<ReturnType>("message");
```

---

## HOT-RELOAD MANDATORY
- RenderSystem must implement HotReloadable
- GPU resources are HANDLES (not raw pointers)
- prepare_reload() releases GPU resources
- finish_reload() recreates from cached CPU data
- Serialize: render config, camera state, lights (NOT GPU data)

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only

## PERFORMANCE
- Batch similar draw calls
- Frustum culling
- Material sorting
- Pre-allocated command buffers
- NO allocations in render loop

---

## KNOWN ISSUES TO FIX

The render module has compilation errors in:
- `pass.hpp` - constexpr operator| issues, ClearValue static member issues, missing std::sort include
- `compositor.hpp` - constexpr LayerFlags issues, const correctness
- `render_graph.cpp` - API mismatches with headers

**Fix these compilation errors as part of your implementation.**

---

## YOUR TASK

**Your files:** `src/render/` and `include/void_engine/render/`

**ALSO modify:** `src/main.cpp` to verify integration works

**Implement:**
1. `camera.hpp` → `src/render/camera.cpp`
2. `light.hpp` → `src/render/light.cpp`
3. `material.hpp` → `src/render/material.cpp`
4. `mesh.hpp` → `src/render/mesh.cpp`
5. `pass.hpp` → `src/render/pass.cpp` (FIX header issues first!)
6. `resource.hpp` → `src/render/resource.cpp`
7. `shadow.hpp` → `src/render/shadow.cpp`
8. `debug.hpp` → `src/render/debug.cpp`
9. `texture.hpp` → `src/render/texture.cpp`
10. `render.hpp` → `src/render/render.cpp`

**Check legacy:** `legacy/crates/void_render/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. **Fixed header files** (pass.hpp, compositor.hpp, etc.)
4. Verified that `src/main.cpp` still compiles and runs
5. `doc/diagrams/render_integration.md`
6. `doc/validation/render_validation.md`

## START NOW
1. First run `cmake --build build 2>&1 | head -100` to see current errors
2. Read include/void_engine/render/pass.hpp and fix compilation errors
3. Then implement the .cpp files
```

---

### Terminal 9 - Compositor Module

**COPY EVERYTHING BELOW AND PASTE:**

```
Think deeply and thoroughly about this task before implementing. You are a master senior engineer implementing production code.

You are implementing the void_compositor module for Void Engine. This code MUST compile without errors on first attempt.

## CRITICAL: ENGINE INTEGRATION REQUIRED

**This is NOT a standalone module. It MUST integrate into src/main.cpp.**

The compositor manages post-processing, HDR, and final output composition. After implementing, you MUST:

1. Add compositor include to main.cpp:
```cpp
#include <void_engine/compositor/compositor_module.hpp>
```

2. Initialize compositor after renderer in main.cpp:
```cpp
// ==========================================================================
// Compositor - Post-processing and final output
// ==========================================================================
spdlog::info("Initializing Compositor...");

void_compositor::CompositorConfig compositor_config;
compositor_config.enable_hdr = false;  // Start with SDR
compositor_config.enable_vrr = false;
compositor_config.output_width = config.window_width;
compositor_config.output_height = config.window_height;

void_compositor::Compositor compositor(compositor_config);
compositor.initialize();

spdlog::info("Compositor initialized:");
spdlog::info("  - HDR: {}", compositor_config.enable_hdr ? "ON" : "OFF");
spdlog::info("  - VRR: {}", compositor_config.enable_vrr ? "ON" : "OFF");
```

3. Use compositor in main loop (after renderer.render()):
```cpp
// Compositor post-processing
compositor.begin_frame();
compositor.apply_post_processing(renderer.output_texture());
compositor.end_frame();
```

4. Add compositor stats to FPS logging
5. Add compositor shutdown

## ABSOLUTE RULES - VIOLATION = FAILURE

### RULE 1: NEVER REDEFINE TYPES
- Use types from headers, never copy to .cpp

### RULE 2: CHECK APIS BEFORE USING
- void_core::Version has MEMBERS (.major), not methods (.major())

### RULE 3: INCLUDE ALL DEPENDENCIES
```cpp
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <vector>
```

### RULE 4: RESULT/ERROR PATTERN
```cpp
return void_core::Ok(value);
return void_core::Err<ReturnType>("message");
```

---

## HOT-RELOAD MANDATORY
- Compositor must implement HotReloadable
- Serialize: layer stack, HDR config, VRR state, output config

## NO FAKING
- NO stubs, NO TODOs, FULL implementations only

## FEATURES
- Layer-based composition
- HDR tone mapping (PQ, HLG)
- Variable Refresh Rate (VRR/FreeSync/G-Sync)
- Multi-output management

## PERFORMANCE
- No allocations in composition path
- Efficient layer blending

---

## YOUR TASK

**Your files:** `src/compositor/` and `include/void_engine/compositor/`

**ALSO modify:** `src/main.cpp` to integrate the compositor

**Implement:**
1. `frame.hpp` → `src/compositor/frame.cpp`
2. `hdr.hpp` → `src/compositor/hdr.cpp`
3. `output.hpp` → `src/compositor/output.cpp`
4. `vrr.hpp` → `src/compositor/vrr.cpp`
5. `rehydration.hpp` → `src/compositor/rehydration.cpp`
6. `input.hpp` → `src/compositor/input.cpp`
7. `compositor_module.hpp` → `src/compositor/compositor_module.cpp`

**Check legacy:** `legacy/crates/void_compositor/src/`

## DELIVERABLES
1. All .cpp files (that compile without errors)
2. CMakeLists.txt additions
3. **Updated src/main.cpp** with compositor integration
4. `doc/diagrams/compositor_integration.md`
5. `doc/validation/compositor_validation.md`

## INTEGRATION VERIFICATION

After implementation, main.cpp should have this structure in the main loop:
```
while (!glfwWindowShouldClose(window)) {
    frame_timing.begin_frame();
    event_bus.process();
    asset_server.process();
    physics_world->step(delta_time);
    live_scene_mgr.update(delta_time);
    AnimationSystem::update(ecs_world, delta_time);
    renderer.update(delta_time);
    renderer.render();
    compositor.begin_frame();           // NEW
    compositor.apply_post_processing(); // NEW
    compositor.end_frame();             // NEW
    glfwSwapBuffers(window);
}
```

## START NOW
1. Read include/void_engine/compositor/compositor.hpp first
2. Implement the .cpp files
3. THEN modify src/main.cpp to integrate
4. Verify the build compiles and runs
```

---

## After Phase 3: Final Build & Test

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/void_engine examples/model-viewer
```

**VERIFY:** The engine should start with all systems logging:
- Service Registry: health monitoring ON
- Event Bus: inter-system messaging ON
- Frame Timing: statistics ON
- ECS World: entity capacity
- Physics World: body capacity
- Asset Server: hot-reload ON
- Renderer: shader hot-reload ON
- Compositor: HDR/VRR status (NEW)

---

## What Each Prompt Guarantees

| Requirement | How It's Enforced |
|-------------|-------------------|
| No duplicate types | RULE 1: NEVER REDEFINE TYPES |
| Correct APIs | RULE 2: CHECK APIS BEFORE USING |
| Proper includes | RULE 3: INCLUDE ALL DEPENDENCIES |
| Error handling | RULE 4: RESULT/ERROR PATTERN |
| Version access | Explicit: .major is MEMBER not method |
| Hot-reload | Explicit HotReloadable requirements |
| No faking | "NO stubs, NO TODOs" |
| Senior engineer | "master senior engineer" framing |
| Must compile | "MUST compile without errors on first attempt" |
| **Engine integration** | **Phase 3: "MUST integrate into src/main.cpp"** |
| **Not ghost code** | **Phase 3: explicit main.cpp modifications required** |
| **Verification** | **Phase 3: "verify the build compiles and runs"** |

---

## Integration Checklist (Phase 3)

Before marking a Phase 3 module complete, verify:

- [ ] Include added to src/main.cpp
- [ ] Initialization code added after existing systems
- [ ] Main loop integration (update/process calls)
- [ ] Statistics added to FPS logging
- [ ] Shutdown code added in reverse order
- [ ] Build compiles without errors
- [ ] Engine runs with `./build/void_engine examples/model-viewer`
- [ ] System logs appear at startup
