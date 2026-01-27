# Phased Initialization Build Plan

**Purpose**: Refactor `src/main.cpp` to align with proposed industry-standard initialization order.
**Method**: Comment out all code, then uncomment and validate phase by phase.

## Quick Reference

```
Phase 0: Skeleton      - Entry point, CLI, logging only
Phase 1: Foundation    - memory, core, math, structures
Phase 2: Infrastructure - event, services, ir, kernel
Phase 3: Resources     - asset, shader
Phase 4: Platform      - presenter, render, compositor
Phase 5: Simulation    - ecs, physics, triggers
Phase 6: Scene         - scene, graph
Phase 7: Application   - Initial scene load, main loop
```

---

## Pre-Work: Create Backup

```bash
# Create backup before modifying
cp src/main.cpp src/main.cpp.backup

# Create branch for this work
git checkout -b refactor/phased-init
```

---

## Phase 0: Skeleton (Entry Point Only)

**Goal**: Minimal executable that parses CLI and logs startup.

### What to Keep Uncommented

```cpp
// Keep these includes:
#include <spdlog/spdlog.h>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// Keep: print_usage(), print_version()

// Keep: main() CLI parsing (lines 301-327)
// Keep: Manifest path resolution (lines 329-338)
// Keep: Load manifest (lines 340-349)

int main(int argc, char** argv) {
    // CLI parsing - KEEP
    // Manifest loading - KEEP

    spdlog::info("Phase 0: Skeleton - CLI and manifest working");
    spdlog::info("Project: {} v{}", config.display_name, config.version);

    return 0;  // Exit here for Phase 0
}
```

### What to Comment Out

```cpp
// Comment out ALL of these sections:

// -- Includes (comment out) --
// #include <void_engine/render/gl_renderer.hpp>
// #include <void_engine/scene/scene_parser.hpp>
// ... all void_engine includes
// #include <GLFW/glfw3.h>

// -- InputState struct (comment out) --
// -- GLFW callbacks (comment out) --
// -- EcsSceneBridge class (comment out) --

// -- In main(): comment out everything after manifest loading --
// GLFW init (351-386)
// Renderer init (388-397)
// Services (399-446)
// Assets (448-472)
// ECS (474-498)
// Physics (500-532)
// Scene loading (534-558)
// Frame timing (567-577)
// Compositor (579-601)
// Main loop (619-785)
// Shutdown (787-843)
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected output:
# [info] Phase 0: Skeleton - CLI and manifest working
# [info] Project: Model Viewer v1.0.0
```

### Acceptance Criteria
- [ ] Compiles without errors
- [ ] Runs and exits cleanly
- [ ] Logs project name and version

---

## Phase 1: Foundation (memory, core, math, structures)

**Goal**: Initialize foundation modules. These are mostly header-only but establish patterns.

### New Includes to Uncomment

```cpp
#include <void_engine/memory/allocator.hpp>    // If exists
#include <void_engine/core/types.hpp>          // If exists
#include <void_engine/math/vec3.hpp>           // If exists
#include <void_engine/structures/handle.hpp>   // If exists
```

### New Code to Add/Uncomment

```cpp
int main(int argc, char** argv) {
    // Phase 0: CLI parsing (already working)

    // =========================================
    // Phase 1: Foundation
    // =========================================
    spdlog::info("Phase 1: Initializing Foundation...");

    // Memory system (if module has init)
    // void_memory::init();
    spdlog::info("  - memory: initialized (header-only)");

    // Core utilities (if module has init)
    // void_core::init();
    spdlog::info("  - core: initialized (header-only)");

    // Math library (header-only, no init needed)
    spdlog::info("  - math: initialized (header-only)");

    // Structures (header-only, no init needed)
    spdlog::info("  - structures: initialized (header-only)");

    spdlog::info("Phase 1: Foundation complete");

    return 0;
}
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected output:
# [info] Phase 1: Initializing Foundation...
# [info]   - memory: initialized (header-only)
# [info]   - core: initialized (header-only)
# [info]   - math: initialized (header-only)
# [info]   - structures: initialized (header-only)
# [info] Phase 1: Foundation complete
```

### Acceptance Criteria
- [ ] All foundation headers include without error
- [ ] Basic types (Vec3, Handle, etc.) can be instantiated
- [ ] No link errors

---

## Phase 2: Infrastructure (event, services, ir, kernel)

**Goal**: Event bus, service registry, IR patches, kernel orchestration.

### New Includes to Uncomment

```cpp
#include <void_engine/event/event_bus.hpp>     // If separate from services
#include <void_engine/services/services.hpp>
#include <void_engine/ir/patch.hpp>            // If exists
#include <void_engine/kernel/kernel.hpp>       // If exists
```

### New Code to Add/Uncomment

```cpp
    // =========================================
    // Phase 2: Infrastructure
    // =========================================
    spdlog::info("Phase 2: Initializing Infrastructure...");

    // Event system
    void_services::EventBus event_bus;
    spdlog::info("  - event: EventBus created");

    // Service registry
    void_services::ServiceRegistry service_registry;
    service_registry.set_event_callback([](const void_services::ServiceEvent& event) {
        switch (event.type) {
            case void_services::ServiceEventType::Started:
                spdlog::info("  Service started: {}", event.service_id.name);
                break;
            case void_services::ServiceEventType::Stopped:
                spdlog::info("  Service stopped: {}", event.service_id.name);
                break;
            case void_services::ServiceEventType::Failed:
                spdlog::error("  Service failed: {}", event.service_id.name);
                break;
            default:
                break;
        }
    });
    spdlog::info("  - services: ServiceRegistry created");

    // IR system (if available)
    // void_ir::PatchBus patch_bus;
    spdlog::info("  - ir: (not yet integrated)");

    // Kernel (if available)
    // void_kernel::Kernel kernel;
    spdlog::info("  - kernel: (not yet integrated)");

    spdlog::info("Phase 2: Infrastructure complete");

    // Shutdown (reverse order)
    service_registry.stop_all();

    return 0;
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected output includes:
# [info] Phase 2: Initializing Infrastructure...
# [info]   - event: EventBus created
# [info]   - services: ServiceRegistry created
# [info] Phase 2: Infrastructure complete
```

### Acceptance Criteria
- [ ] EventBus compiles and instantiates
- [ ] ServiceRegistry compiles and instantiates
- [ ] Event callbacks work
- [ ] Clean shutdown

---

## Phase 3: Resources (asset, shader)

**Goal**: Asset server with hot-reload, shader system ready.

### New Includes to Uncomment

```cpp
#include <void_engine/asset/server.hpp>
#include <void_engine/asset/loaders/texture_loader.hpp>
#include <void_engine/asset/loaders/model_loader.hpp>
#include <void_engine/shader/pipeline.hpp>     // If exists
```

### New Code to Add/Uncomment

```cpp
    // =========================================
    // Phase 3: Resources
    // =========================================
    spdlog::info("Phase 3: Initializing Resources...");

    // Asset server
    void_asset::AssetServerConfig asset_config;
    asset_config.asset_dir = (config.project_dir / "assets").string();
    asset_config.hot_reload = true;
    asset_config.max_concurrent_loads = 4;

    void_asset::AssetServer asset_server(asset_config);

    // Register loaders
    asset_server.register_loader<void_asset::TextureAsset>(
        std::make_unique<void_asset::TextureLoader>());
    asset_server.register_loader<void_asset::ModelAsset>(
        std::make_unique<void_asset::ModelLoader>());

    spdlog::info("  - asset: AssetServer created");
    spdlog::info("    - Directory: {}", asset_config.asset_dir);
    spdlog::info("    - Hot-reload: {}", asset_config.hot_reload ? "ON" : "OFF");
    spdlog::info("    - Loaders: textures, models");

    // Shader system (if available)
    // void_shader::Pipeline shader_pipeline;
    spdlog::info("  - shader: (initialized with render in Phase 4)");

    spdlog::info("Phase 3: Resources complete");

    // Shutdown
    asset_server.collect_garbage();
    service_registry.stop_all();

    return 0;
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected output includes:
# [info] Phase 3: Initializing Resources...
# [info]   - asset: AssetServer created
# [info]   - shader: (initialized with render in Phase 4)
# [info] Phase 3: Resources complete
```

### Acceptance Criteria
- [ ] AssetServer compiles and instantiates
- [ ] Loaders register without error
- [ ] Hot-reload config accepted
- [ ] Clean shutdown with garbage collection

---

## Phase 4: Platform (presenter, render, compositor)

**Goal**: Window creation, GPU context, rendering pipeline.

### New Includes to Uncomment

```cpp
#include <void_engine/presenter/timing.hpp>
#include <void_engine/presenter/frame.hpp>
#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/compositor/compositor_module.hpp>
#include <GLFW/glfw3.h>
```

### New Code to Add/Uncomment

```cpp
    // =========================================
    // Phase 4: Platform
    // =========================================
    spdlog::info("Phase 4: Initializing Platform...");

    // Presenter: Window creation
    if (!glfwInit()) {
        spdlog::error("Failed to initialize GLFW");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    std::string window_title = config.display_name + " - void_engine";
    GLFWwindow* window = glfwCreateWindow(
        config.window_width, config.window_height,
        window_title.c_str(), nullptr, nullptr);

    if (!window) {
        spdlog::error("Failed to create window");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    spdlog::info("  - presenter: Window created ({}x{})",
                 config.window_width, config.window_height);

    // Render: GPU context and renderer
    void_render::SceneRenderer renderer;
    if (!renderer.initialize(window)) {
        spdlog::error("Failed to initialize renderer");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    spdlog::info("  - render: SceneRenderer initialized");

    // Compositor: Post-processing
    void_compositor::CompositorConfig compositor_config;
    compositor_config.target_fps = 60;
    compositor_config.vsync = true;
    compositor_config.enable_hdr = false;

    auto compositor = void_compositor::CompositorFactory::create(compositor_config);
    if (!compositor) {
        compositor = void_compositor::CompositorFactory::create_null(compositor_config);
    }
    spdlog::info("  - compositor: {} backend",
                 void_compositor::CompositorFactory::backend_name());

    // Frame timing
    void_presenter::FrameTiming frame_timing(60);
    spdlog::info("  - timing: 60 FPS target");

    spdlog::info("Phase 4: Platform complete");

    // Quick render test - show window briefly
    for (int i = 0; i < 60; ++i) {  // ~1 second at 60fps
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) break;

        renderer.render();
        glfwSwapBuffers(window);
    }

    // Shutdown
    compositor->shutdown();
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    asset_server.collect_garbage();
    service_registry.stop_all();

    spdlog::info("Phase 4: Shutdown complete");
    return 0;
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected:
# - Window opens briefly (~1 second)
# - May show blank or default color
# - Closes cleanly
```

### Acceptance Criteria
- [ ] Window opens at correct size
- [ ] OpenGL context created
- [ ] Renderer initializes
- [ ] Compositor creates (even if null)
- [ ] Window closes cleanly

---

## Phase 5: Simulation (ecs, physics, triggers)

**Goal**: ECS world, physics simulation, trigger volumes.

### New Includes to Uncomment

```cpp
#include <void_engine/ecs/world.hpp>
#include <void_engine/physics/physics.hpp>
// #include <void_engine/triggers/triggers.hpp>  // If exists
```

### New Code to Add/Uncomment

```cpp
    // =========================================
    // Phase 5: Simulation
    // =========================================
    spdlog::info("Phase 5: Initializing Simulation...");

    // ECS World
    void_ecs::World ecs_world(1024);
    spdlog::info("  - ecs: World created (capacity: 1024)");

    // Physics World
    auto physics_world = void_physics::PhysicsWorldBuilder()
        .gravity(0.0f, -9.81f, 0.0f)
        .fixed_timestep(1.0f / 60.0f)
        .max_substeps(4)
        .max_bodies(10000)
        .enable_ccd(true)
        .hot_reload(true)
        .build();

    physics_world->on_collision_begin([](const void_physics::CollisionEvent& event) {
        spdlog::debug("Collision: {} <-> {}", event.body_a.value, event.body_b.value);
    });

    spdlog::info("  - physics: PhysicsWorld created");
    spdlog::info("    - Gravity: (0, -9.81, 0)");
    spdlog::info("    - Max bodies: 10000");

    // Triggers (if available)
    spdlog::info("  - triggers: (not yet integrated)");

    spdlog::info("Phase 5: Simulation complete");

    // Test loop with physics stepping
    for (int i = 0; i < 60; ++i) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) break;

        float dt = 1.0f / 60.0f;
        physics_world->step(dt);

        renderer.render();
        glfwSwapBuffers(window);
    }

    // Shutdown (reverse order)
    physics_world->clear();
    ecs_world.clear();
    // ... rest of shutdown
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected:
# - ECS and Physics initialize
# - Physics steps without error
# - Clean shutdown
```

### Acceptance Criteria
- [ ] ECS World creates with capacity
- [ ] Physics World builds with config
- [ ] Physics stepping works (even empty)
- [ ] Collision callbacks register
- [ ] Clean shutdown

---

## Phase 6: Scene (scene, graph)

**Goal**: Scene loading, scene graph, ECS integration.

### New Includes to Uncomment

```cpp
#include <void_engine/scene/scene_parser.hpp>
#include <void_engine/scene/scene_data.hpp>
#include <void_engine/scene/scene_instantiator.hpp>
// #include <void_engine/graph/node.hpp>  // If exists
```

### New Code to Add/Uncomment

```cpp
    // =========================================
    // Phase 6: Scene
    // =========================================
    spdlog::info("Phase 6: Initializing Scene...");

    // ECS-Renderer bridge (uncomment EcsSceneBridge class)
    EcsSceneBridge ecs_bridge(&ecs_world, &renderer, &asset_server);

    // LiveSceneManager
    void_scene::LiveSceneManager live_scene_mgr(&ecs_world);

    auto init_result = live_scene_mgr.initialize();
    if (!init_result) {
        spdlog::error("Failed to initialize LiveSceneManager");
        // cleanup and return
    }

    live_scene_mgr.on_scene_changed([&ecs_bridge](const fs::path& path,
                                                   const void_scene::SceneData& scene) {
        ecs_bridge.on_scene_changed(path, scene);
    });

    spdlog::info("  - scene: LiveSceneManager initialized");

    // Graph (visual scripting) - if available
    spdlog::info("  - graph: (not yet integrated)");

    spdlog::info("Phase 6: Scene complete");

    // Load initial scene
    if (!config.scene_file.empty()) {
        fs::path scene_path = config.project_dir / config.scene_file;
        auto load_result = live_scene_mgr.load_scene(scene_path);
        if (!load_result) {
            spdlog::error("Failed to load scene: {}", load_result.error().message());
        } else {
            spdlog::info("Scene loaded: {} entities", ecs_world.entity_count());
        }
    }

    // Shutdown
    live_scene_mgr.shutdown();
    // ... rest
```

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected:
# - Scene file loads
# - Entities created in ECS
# - Renderer receives scene data
```

### Acceptance Criteria
- [ ] LiveSceneManager initializes
- [ ] Scene file parses
- [ ] Entities populate ECS
- [ ] Renderer receives scene
- [ ] Hot-reload callback registered

---

## Phase 7: Application (Full Loop)

**Goal**: Complete main loop with all systems integrated.

### Code to Uncomment

- All GLFW callbacks (InputState, mouse, keyboard)
- Full main loop (lines 619-785)
- Full shutdown sequence (lines 787-843)
- Hot-reload timers
- FPS counter with full stats

### Build & Validate

```bash
cmake --build build
./build/void_engine examples/model-viewer

# Expected:
# - Full application runs
# - Camera controls work
# - Scene renders
# - Hot-reload works (modify scene.toml)
# - ESC quits cleanly
```

### Acceptance Criteria
- [ ] All systems initialized in correct order
- [ ] Main loop runs at target FPS
- [ ] Input controls work (orbit, pan, zoom)
- [ ] Hot-reload for scene and shaders
- [ ] Clean shutdown in reverse order
- [ ] All statistics logged

---

## Future Phases (Post-MVP)

### Phase 8: Scripting
- script, scripting, cpp, shell modules
- WASM interpreter
- Debug REPL

### Phase 9: Gameplay
- ai, combat, inventory, gamestate modules

### Phase 10: UI
- ui, hud modules

### Phase 11: Extensions
- xr, editor modules

---

## Troubleshooting

### Link Errors
```bash
# Check for stub.cpp conflicts
find src -name "stub.cpp" -exec echo "Found stub: {}" \;

# Remove conflicting stubs (especially src/render/stub.cpp)
```

### Missing Symbols
```bash
# Check CMakeLists.txt for missing source files
grep -r "add_library\|add_executable" CMakeLists.txt
```

### Include Errors
```bash
# Verify include paths
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# Check compile_commands.json for -I flags
```

### Runtime Crashes
```bash
# Run with debugger
gdb ./build/void_engine
run examples/model-viewer
bt  # backtrace on crash
```

---

## Checklist Summary

```
[ ] Phase 0: Skeleton compiles and runs
[ ] Phase 1: Foundation modules include
[ ] Phase 2: EventBus and ServiceRegistry work
[ ] Phase 3: AssetServer initializes
[ ] Phase 4: Window opens, renderer works
[ ] Phase 5: ECS and Physics initialize
[ ] Phase 6: Scene loads into ECS
[ ] Phase 7: Full application runs
```

---

## References

- [PROPOSED_INIT_ORDER.md](PROPOSED_INIT_ORDER.md) - Target initialization order
- [INIT_ORDER.md](INIT_ORDER.md) - Current initialization order
- [LEGACY_INIT_ORDER.md](LEGACY_INIT_ORDER.md) - Legacy Rust reference
- [INDEX.md](INDEX.md) - Module status and grades
