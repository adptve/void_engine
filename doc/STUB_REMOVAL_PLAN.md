# Stub Removal and Implementation Plan

## Executive Summary

**Total stub.cpp files found:** 13
**Files to DELETE:** 1 (render/stub.cpp)
**Files to KEEP:** 9 (legitimate module stubs)
**Files needing IMPLEMENTATION:** 3 (shell, editor, runtime)

**Additional unimplemented functions:** 50+ across codebase

---

## CRITICAL: Render Pipeline Integration Status

**The render pipeline is architecturally complete but functionally disconnected.**

| Component | Architecture | Integration | Actual Rendering |
|-----------|--------------|-------------|------------------|
| **Compositor** | ✅ Complete | ✅ Integrated | ✅ Working (software) |
| **Presenter** | ✅ Complete | 🚫 BYPASSED | 🚫 Direct GLFW used |
| **Pass System** | ✅ Complete | ✅ Integrated | 🚫 ALL CALLBACKS EMPTY |
| **SceneRenderer** | ✅ Complete | ⚠️ CONFLICTING | ⚠️ Stub vs Real unclear |

### What This Means

1. **Compositor works** - `LayerIntegratedCompositor` and `SoftwareLayerCompositor` are active in main.cpp
2. **Presenter is bypassed** - `main.cpp:735` calls `glfwSwapBuffers()` directly, not through `IPresenter`
3. **All render passes are empty** - Every built-in pass callback is `[](const PassContext&) {}`
4. **gl_renderer.cpp has real code** but may not be linked due to stub.cpp conflict

---

## Part 0: Render Pipeline Integration (CRITICAL)

### Current State: Direct GLFW Rendering

The main loop in `src/main.cpp` bypasses the presenter abstraction:

```cpp
// Line 713 - Rendering
renderer.render();

// Line 720-728 - Compositor (WORKING)
compositor->dispatch();
if (compositor->should_render()) {
    auto render_target = compositor->begin_frame();
    if (render_target) {
        compositor->end_frame(std::move(render_target));
    }
}

// Line 735 - BYPASSES PRESENTER
glfwSwapBuffers(window);  // Direct GLFW call!
```

### Problem 1: Presenter Not Used

The `void_presenter` module has:
- Complete `IPresenter` interface
- `BackendFactory` that can create backends
- `PresenterManager` for multiple outputs
- `NullPresenter`, OpenGL, wgpu backend support

**But none of it is called.** Frame presentation goes directly through GLFW.

### Problem 2: All Pass Callbacks Empty

In `include/void_engine/render/compositor.hpp`, every built-in pass has an empty callback:

```cpp
// Lines 608-670 - ALL of these are empty:
case PassType::DepthPrePass:
    return m_passes.add_callback(builtin_passes::depth_prepass(),
        [](const PassContext&) {});  // EMPTY

case PassType::ShadowMap:
    return m_passes.add_callback(builtin_passes::shadow_map(),
        [](const PassContext&) {});  // EMPTY

case PassType::Forward:
    return m_passes.add_callback(builtin_passes::forward(),
        [](const PassContext&) {});  // EMPTY

// ... ALL 12+ passes are empty
```

### Problem 3: SceneRenderer Confusion

Two implementations exist:
- `src/render/stub.cpp` - All methods empty/stubbed
- `src/render/gl_renderer.cpp` - Real OpenGL implementation

**ODR violation means linker picks one arbitrarily.**

### Required Integration Work

#### Step 1: Delete render/stub.cpp (enables real renderer)
```bash
rm src/render/stub.cpp
# Update CMakeLists.txt
```

#### Step 2: Implement Pass Callbacks

Each pass needs a real implementation. Example for Forward pass:

```cpp
case PassType::Forward:
    return m_passes.add_callback(builtin_passes::forward(),
        [this](const PassContext& ctx) {
            // Bind framebuffer
            // Set viewport
            // For each renderable in scene:
            //   Bind material/shader
            //   Set uniforms (MVP, lighting)
            //   Draw mesh
        });
```

#### Step 3: Integrate Presenter

Replace direct GLFW call:
```cpp
// BEFORE:
glfwSwapBuffers(window);

// AFTER:
presenter->present();
```

And initialize presenter during setup:
```cpp
void_presenter::init();  // Register backends
auto presenter = void_presenter::create_best_backend(window_handle, config);
```

#### Step 4: Connect SceneRenderer to Pass System

The SceneRenderer in gl_renderer.cpp should be called from pass callbacks:
```cpp
case PassType::Forward:
    return m_passes.add_callback(builtin_passes::forward(),
        [&renderer](const PassContext& ctx) {
            renderer.render();  // Use the real GL renderer
        });
```

---

## Part 1: Stub Files Analysis

### DELETE - Duplicate Implementation

#### `src/render/stub.cpp` (218 lines)

**Status:** CRITICAL - Contains 45+ duplicate implementations that conflict with `gl_renderer.cpp`

**Why delete:**
- ALL real implementations exist in `gl_renderer.cpp` (1,587 lines)
- `stub.cpp` contains only empty `{}` bodies or dummy returns
- Having both files causes ODR (One Definition Rule) violations
- Linker will pick one arbitrarily, causing unpredictable behavior

**Duplicated functions (all implemented better in gl_renderer.cpp):**

| Function | stub.cpp | gl_renderer.cpp |
|----------|----------|-----------------|
| GlCamera::view_matrix() | Basic | Same |
| GlCamera::projection_matrix() | Basic | Better ortho scaling |
| GlCamera::orbit() | Works | Better clamping |
| GlCamera::zoom() | No limits | Proper 1.0-100.0 limits |
| GlCamera::pan() | Works | Cleaner code |
| GpuMesh::destroy() | Sets to 0 | Real GL cleanup |
| ShaderProgram::~ShaderProgram() | Empty | glDeleteProgram |
| ShaderProgram::load_from_source() | Returns true | Real compilation |
| ShaderProgram::load_from_files() | Returns true | File loading + hot-reload |
| ShaderProgram::reload() | Returns true | Full hot-reload |
| ShaderProgram::use() | Empty | glUseProgram |
| ShaderProgram::set_* (8 methods) | All empty | All real GL calls |
| ShaderProgram::compile_shader() | Returns 0 | Real GL compile |
| ShaderProgram::link_program() | Returns true | Real GL link |
| ShaderProgram::get_uniform_location() | Returns -1 | Caching + GL call |
| SceneRenderer (18+ methods) | All stubs | All real implementations |
| Mesh generation (8 methods) | Return empty | Full geometry generation |

**Action:**
```bash
# Remove from CMakeLists.txt first, then delete:
rm src/render/stub.cpp
```

---

### KEEP - Legitimate Module Stubs

These stubs provide module metadata, factory functions, or compilation units for header-only libraries.

#### 1. `src/compositor/stub.cpp` (200 lines) - KEEP

**Purpose:** Module registration, backend queries, factory functions
**Contains:**
- `version()`, `module_name()` - Module metadata
- `query_backends()`, `recommended_backend()` - Backend discovery
- `create_layer_manager()`, `create_layer_compositor()` - Factories
- `init()`, `shutdown()` - Lifecycle hooks

**Real implementations:** compositor.cpp, layer_compositor.cpp, frame.cpp, etc.

---

#### 2. `src/presenter/stub.cpp` (59 lines) - KEEP

**Purpose:** Backend registration and discovery
**Contains:**
- `version()` - Module metadata
- `init()` - Registers OpenGL/wgpu backends
- `query_backends()`, `recommended_backend()` - Backend discovery
- `create_best_backend()` - Factory

**Real implementations:** presenter.cpp, backend_factory.cpp, opengl_backend.cpp, wgpu_backend.cpp

---

#### 3. `src/core/stub.cpp` (69 lines) - KEEP

**Purpose:** Module metadata for header-only core library
**Contains:** `version()`, `module_name()`, `capabilities()`, `init()`, `shutdown()`
**Real implementations:** error.cpp, handle.cpp, id.cpp, log.cpp, etc.

---

#### 4. `src/event/stub.cpp` (65 lines) - KEEP

**Purpose:** Module metadata for header-only event system
**Contains:** `version()`, `module_name()`, `capabilities()`, `init()`, `shutdown()`
**Note:** Event system is template-based, header-only

---

#### 5. `src/asset/stub.cpp` (15 lines) - KEEP

**Purpose:** Compilation unit for header-only portions
**Contains:** Single `init()` no-op
**Real implementations:** asset.cpp, loader.cpp, cache.cpp, etc.

---

#### 6. `src/memory/stub.cpp` (15 lines) - KEEP

**Purpose:** Compilation unit for header-only memory allocators
**Contains:** Single `init()` no-op
**Note:** Memory module is entirely header-only

---

#### 7. `src/ecs/stub.cpp` (22 lines) - KEEP

**Purpose:** Compilation unit for header-only ECS
**Contains:** `version()`, `init()`
**Note:** ECS is template-based, header-only

---

#### 8. `src/shader/stub.cpp` (25 lines) - KEEP

**Purpose:** Compilation unit for header-only shader utilities
**Contains:** `version()`, `init()`
**Note:** Shader module is header-only

---

#### 9. `src/graph/stub.cpp` (10 lines) - KEEP (with note)

**Purpose:** Placeholder due to type conflicts
**Contains:** Empty namespace
**Note:** Comment says "implementation files excluded due to internal/public type definition conflicts that need to be resolved"

**TODO:** Resolve type conflicts and re-enable graph implementations

---

### INCOMPLETE - Need Implementation

These stubs are marked as migration work-in-progress.

#### 1. `src/shell/stub.cpp` (6 lines)

**Current:** Single `init()` with TODO comment
**Real implementations exist:** shell.cpp, command.cpp, parser.cpp, session.cpp

**Required work:**
- Integrate init() to call real shell initialization
- Connect to ShellSystem startup

---

#### 2. `src/editor/stub.cpp` (6 lines)

**Current:** Single `init()` with TODO comment
**Real implementations:** None yet

**Required work:**
- Implement editor module from scratch
- Create EditorSystem, EditorWindow, etc.

---

#### 3. `src/runtime/stub.cpp` (6 lines)

**Current:** Single `init()` with TODO comment
**Real implementations exist:** runtime.cpp, input.cpp, layer.cpp, window.cpp

**Required work:**
- Integrate init() to call real runtime initialization
- Connect to RuntimeSystem startup

---

## Part 2: Conditional Stubs (Keep)

These stubs gracefully degrade when dependencies are unavailable.

#### `src/asset/http_client.cpp` - StubHttpClient class (lines 252-286)

**When:** libcurl not available
**Behavior:** Returns 501 "HTTP client not available"
**Keep:** Yes - proper fallback

#### `src/asset/websocket_client.cpp` - StubWebSocketClient class (lines 388-422)

**When:** Beast (Boost) not available
**Behavior:** Returns false for all operations
**Keep:** Yes - proper fallback

#### `src/presenter/backends/wgpu_backend.cpp` - Stub functions (lines 552-567)

**When:** WGPU not available
**Behavior:** `is_wgpu_available()` returns false, functions are no-ops
**Keep:** Yes - proper fallback

#### `src/xr/xr_system.cpp` - StubXrSession class (lines 13-240+)

**When:** VR hardware not available
**Behavior:** Desktop simulation of XR for development
**Keep:** Yes - enables development without VR hardware

---

## Part 3: Unimplemented Functions (TODO/FIXME)

### Critical Priority

| File | Function | Issue |
|------|----------|-------|
| `src/graph/graph.cpp:613` | `Graph::clone()` | "TODO: Deep clone nodes and rebuild connections" |
| `src/graph/registry.cpp:1600` | `GraphLibrary::generate_cpp_code()` | "TODO: Generate C++ code" - returns stub |
| `src/graph/execution.cpp:464` | `GraphCompiler::fold_constants()` | "Placeholder for actual implementation" |
| `src/graph/execution.cpp:469` | `GraphCompiler::eliminate_dead_code()` | "Placeholder for actual implementation" |

### High Priority

| File | Function | Issue |
|------|----------|-------|
| `src/physics/shape.cpp:716` | `CompoundShape::contains_point()` | "TODO: Apply rotation" - transforms ignored |
| `src/physics/world.cpp:592` | `CharacterController::slide_move()` | "TODO: Implement proper collision response with slide" |
| `src/render/animation.cpp:753` | `AnimationStateMachine::play()` | "TODO: Handle blend time for smooth transitions" |
| `include/void_engine/core/version.hpp` | `VersionRange::contains()` | Declared but not implemented |

### Medium Priority

| File | Function | Issue |
|------|----------|-------|
| `src/shell/session.cpp:964` | Completion handling | "TODO: Complete flags from command info" |
| `src/shell/session.cpp:434` | Tab completion | "TODO: Handle tab completion" |
| `src/shell/session.cpp:440` | Command cancel | "TODO: Cancel current command" |
| `src/asset/remote.cpp:557` | `list_assets_async()` | "TODO: Implement asset listing API" |
| `src/asset/remote.cpp:564` | `list_scenes_async()` | "TODO: Implement scene listing API" |
| `src/combat/weapons.cpp:238` | Firing spread | "TODO: Get aiming state" |

### Low Priority

| File | Function | Issue |
|------|----------|-------|
| `src/hud/binding.cpp:100` | `StringFormatConverter::convert_back()` | "String to original type conversion not implemented" |
| `src/hud/binding.cpp:210` | `ColorInterpolateConverter::convert_back()` | "Color to float conversion not implemented" |
| `src/engine/config.cpp:435` | CLI parsing | "Short options not implemented for simplicity" |
| `src/ui/widgets.cpp:610` | Paste handling | "Handle Ctrl+V paste (platform-specific, not implemented)" |

### Placeholders (Need Real Implementation)

| File | Line | Issue |
|------|------|-------|
| `src/hud/elements.cpp` | 198 | "This is a placeholder for the render call" |
| `src/scene/asset_loader.cpp` | 421 | "Create a placeholder - actual glTF loading is in void_render" |
| `src/runtime/scene_loader.cpp` | 405 | "Generate placeholder entity IDs" |
| `src/xr/openxr_backend.cpp` | 615 | "Placeholder - actual implementation needs extension support" |
| `src/xr/openxr_backend.cpp` | 851 | "Query swapchain formats (placeholder)" |
| `src/compositor/rehydration.cpp` | 281 | "Count (placeholder for now)" |
| `src/physics/world.cpp` | 164 | "Slider uses fixed constraint as placeholder" |
| `src/presenter/backends/wgpu_backend.cpp` | 378-386 | `create_instance()` returns true but is stub |

---

## Part 4: Action Plan

### Phase 1: Immediate Cleanup (1 day)

1. **Delete render/stub.cpp**
   ```bash
   # Edit src/render/CMakeLists.txt - remove stub.cpp from SOURCES
   # Delete the file
   rm src/render/stub.cpp
   # Rebuild and verify
   cmake --build build
   ```

2. **Fix compositor bug**
   - File: `include/void_engine/compositor/layer_compositor.hpp:247`
   - Issue: `NullLayerCompositor::end_frame()` references undefined `layers`
   - Fix: Either add member variable or remove the loop

3. **Fix xr namespace mismatch**
   - File: `src/xr/snapshot.cpp`
   - Issue: Uses `void_engine::xr` but header declares `void_xr`
   - Fix: Change to `namespace void_xr {`

### Phase 2: Render Pipeline Integration (CRITICAL - 1-2 weeks)

**This is the most important work - connecting the render pipeline.**

1. **Integrate Presenter into main loop**

   File: `src/main.cpp`

   ```cpp
   // During initialization (after window creation):
   void_presenter::init();
   auto presenter = void_presenter::create_best_backend(window, config);

   // In render loop, replace:
   glfwSwapBuffers(window);
   // With:
   presenter->present();
   ```

2. **Implement Pass Callbacks**

   File: `include/void_engine/render/compositor.hpp` (lines 608-670)

   Each of these needs real GPU work:
   - [ ] `DepthPrePass` - Render depth-only pass
   - [ ] `ShadowMap` - Render shadow maps
   - [ ] `Forward` - Main forward rendering
   - [ ] `ForwardTransparent` - Alpha-blended objects
   - [ ] `Sky` - Skybox/atmosphere
   - [ ] `SSAO` - Screen-space ambient occlusion
   - [ ] `Bloom` - Bloom post-process
   - [ ] `Tonemapping` - HDR to SDR conversion
   - [ ] `FXAA` - Anti-aliasing
   - [ ] `Debug` - Debug visualization
   - [ ] `UI` - User interface overlay

3. **Connect SceneRenderer to Passes**

   The `gl_renderer.cpp` SceneRenderer should be invoked from pass callbacks:
   ```cpp
   // In compositor setup:
   case PassType::Forward:
       return m_passes.add_callback(builtin_passes::forward(),
           [&scene_renderer](const PassContext& ctx) {
               scene_renderer.render();
           });
   ```

4. **Wire up render targets**

   Compositor provides render targets, passes need to use them:
   ```cpp
   auto target = compositor->begin_frame();
   // Pass callbacks render to target
   compositor->end_frame(std::move(target));
   presenter->present();
   ```

### Phase 3: Critical Implementations (1 week)

1. **Implement VersionRange::contains()**
   - File: `src/core/version.cpp`
   - Add implementation for declared method

2. **Implement Graph::clone()**
   - File: `src/graph/graph.cpp:613`
   - Deep clone nodes and rebuild connections

3. **Implement graph compiler optimizations**
   - `fold_constants()` - Evaluate constant expressions at compile time
   - `eliminate_dead_code()` - Remove unreachable nodes

### Phase 4: Module Integration (2 weeks)

1. **Shell module integration**
   - Connect `shell/stub.cpp::init()` to real ShellSystem
   - Implement tab completion
   - Implement command cancellation

2. **Runtime module integration**
   - Connect `runtime/stub.cpp::init()` to RuntimeSystem

3. **Resolve graph type conflicts**
   - Investigate why graph implementations were excluded
   - Fix public/internal type definition conflicts
   - Re-enable real implementations

### Phase 5: Feature Completion (ongoing)

1. **Physics improvements**
   - Implement rotation in CompoundShape::contains_point()
   - Implement proper slide collision response

2. **Animation system**
   - Implement blend time transitions

3. **Editor module**
   - Design and implement from scratch

---

## Summary Checklist

### Files to Delete
- [ ] `src/render/stub.cpp`

### Files to Keep (No Action)
- [x] `src/compositor/stub.cpp`
- [x] `src/presenter/stub.cpp`
- [x] `src/core/stub.cpp`
- [x] `src/event/stub.cpp`
- [x] `src/asset/stub.cpp`
- [x] `src/memory/stub.cpp`
- [x] `src/ecs/stub.cpp`
- [x] `src/shader/stub.cpp`
- [x] `src/graph/stub.cpp` (needs type conflict resolution)

### Files to Integrate
- [ ] `src/shell/stub.cpp` - connect to real shell
- [ ] `src/runtime/stub.cpp` - connect to real runtime
- [ ] `src/editor/stub.cpp` - implement editor module

### Critical Bugs to Fix
- [ ] `compositor/layer_compositor.hpp:247` - undefined `layers` variable
- [ ] `xr/snapshot.cpp` - namespace mismatch
- [ ] `core/version.hpp` - missing `VersionRange::contains()` implementation

### Render Pipeline Integration (CRITICAL)
- [ ] Integrate presenter into main loop (replace `glfwSwapBuffers`)
- [ ] Implement DepthPrePass callback
- [ ] Implement ShadowMap callback
- [ ] Implement Forward pass callback
- [ ] Implement ForwardTransparent callback
- [ ] Implement Sky pass callback
- [ ] Implement SSAO callback
- [ ] Implement Bloom callback
- [ ] Implement Tonemapping callback
- [ ] Implement FXAA callback
- [ ] Implement Debug pass callback
- [ ] Implement UI pass callback
- [ ] Connect SceneRenderer to pass system
- [ ] Wire up render targets from compositor to passes

### Functions to Implement
- [ ] `Graph::clone()` - deep clone
- [ ] `GraphCompiler::fold_constants()` - optimization pass
- [ ] `GraphCompiler::eliminate_dead_code()` - optimization pass
- [ ] `CharacterController::slide_move()` - proper collision
- [ ] `AnimationStateMachine::play()` - blend transitions
- [ ] `RemoteAssetSource::list_assets_async()` - API implementation
- [ ] `RemoteAssetSource::list_scenes_async()` - API implementation
