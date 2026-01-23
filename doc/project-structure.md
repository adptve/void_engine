# void_engine C++ Project Structure

## Design Principles

### 1. Modular Architecture
- Each module is self-contained with its own headers, sources, and tests
- Modules can be built independently or as part of the whole
- Clear dependency graph between modules
- CMake targets per module for granular builds

### 2. Header/Source Separation
- **Public API**: `include/void_engine/<module>/` - Headers users include
- **Private Implementation**: `src/<module>/` - Implementation details
- **Internal Headers**: `src/<module>/internal/` - Module-private headers

### 3. Hot-Reload First
- Core types designed for serialization/deserialization
- Plugin interfaces use stable ABI (C linkage where needed)
- Handle-based resource management (no raw pointers to hot-reloadable data)
- State snapshots for reload preservation

### 4. Build Configuration
- Modular CMake with `CMakeLists.txt` per module
- Presets for common configurations (Debug, Release, Test)
- Export compile commands for IDE integration
- CPM/FetchContent for dependency management

### 5. Testing Strategy
- Unit tests co-located with modules (`tests/<module>/`)
- Integration tests in `tests/integration/`
- Benchmarks in `benchmarks/`
- Catch2 as test framework

### 6. Platform Abstraction
- Platform-specific code in `src/<module>/platform/`
- Compile-time platform detection
- Runtime capability queries

---

## Directory Structure

```
void_engine/
│
├── CMakeLists.txt                 # Root CMake configuration
├── CMakePresets.json              # Build presets (Debug, Release, etc.)
├── CLAUDE.md                      # Claude Code guidance
├── .gitignore
│
├── cmake/                         # CMake modules and utilities
│   ├── VoidEngineConfig.cmake.in  # Package config template
│   ├── CompilerWarnings.cmake     # Warning flags per compiler
│   ├── Dependencies.cmake         # Third-party dependency fetching
│   ├── Testing.cmake              # Test configuration
│   └── Modules.cmake              # Module registration macros
│
├── include/                       # PUBLIC HEADERS (API)
│   └── void_engine/
│       ├── void_engine.hpp        # Master include (includes all)
│       │
│       ├── math/                  # void_math - Linear algebra
│       │   ├── math.hpp           # Module header
│       │   ├── vec.hpp            # Vector types
│       │   ├── mat.hpp            # Matrix types
│       │   ├── quat.hpp           # Quaternion
│       │   ├── transform.hpp      # Transform component
│       │   └── bounds.hpp         # AABB, OBB, Sphere
│       │
│       ├── memory/                # void_memory - Allocators
│       │   ├── memory.hpp
│       │   ├── arena.hpp          # Arena allocator
│       │   ├── pool.hpp           # Pool allocator
│       │   ├── stack.hpp          # Stack allocator
│       │   └── free_list.hpp      # Free list allocator
│       │
│       ├── structures/            # void_structures - Data structures
│       │   ├── structures.hpp
│       │   ├── sparse_set.hpp     # Sparse set
│       │   ├── slot_map.hpp       # Generational slot map
│       │   ├── ring_buffer.hpp    # Lock-free ring buffer
│       │   ├── lock_free_queue.hpp
│       │   └── bitset.hpp         # Dynamic bitset
│       │
│       ├── core/                  # void_core - Foundation
│       │   ├── core.hpp
│       │   ├── types.hpp          # Basic type aliases
│       │   ├── result.hpp         # Result<T, E> type
│       │   ├── handle.hpp         # Generational handles
│       │   ├── id.hpp             # Type-safe IDs
│       │   ├── type_info.hpp      # Runtime type information
│       │   ├── plugin.hpp         # Plugin interface
│       │   └── hot_reload.hpp     # Hot-reload support
│       │
│       ├── event/                 # void_event - Event system
│       │   ├── event.hpp
│       │   ├── bus.hpp            # Event bus
│       │   └── delegate.hpp       # Type-erased callbacks
│       │
│       ├── ecs/                   # void_ecs - Entity Component System
│       │   ├── ecs.hpp
│       │   ├── entity.hpp         # Entity type
│       │   ├── component.hpp      # Component concepts
│       │   ├── world.hpp          # World container
│       │   ├── query.hpp          # Component queries
│       │   ├── system.hpp         # System interface
│       │   └── archetype.hpp      # Archetype storage
│       │
│       ├── ir/                    # void_ir - Intermediate Representation
│       │   ├── ir.hpp
│       │   ├── value.hpp          # Dynamic value type
│       │   ├── patch.hpp          # Patch types
│       │   ├── transaction.hpp    # Transaction builder
│       │   ├── patch_bus.hpp      # Patch queue/dispatch
│       │   └── snapshot.hpp       # State snapshots
│       │
│       ├── asset/                 # void_asset - Asset management
│       │   ├── asset.hpp
│       │   ├── handle.hpp         # Asset handles
│       │   ├── loader.hpp         # Asset loader interface
│       │   ├── cache.hpp          # Asset cache
│       │   └── server.hpp         # Asset server (hot-reload)
│       │
│       ├── shader/                # void_shader - Shader system
│       │   ├── shader.hpp
│       │   ├── compiler.hpp       # WGSL/SPIRV compiler
│       │   ├── reflection.hpp     # Shader reflection
│       │   └── hot_reload.hpp     # Shader hot-reload
│       │
│       ├── render/                # void_render - Rendering
│       │   ├── render.hpp
│       │   ├── device.hpp         # GPU device abstraction
│       │   ├── buffer.hpp         # GPU buffers
│       │   ├── texture.hpp        # Textures
│       │   ├── pipeline.hpp       # Render pipelines
│       │   ├── render_graph.hpp   # Render graph
│       │   ├── pass.hpp           # Render passes
│       │   ├── layer.hpp          # Render layers
│       │   ├── material.hpp       # Materials
│       │   ├── mesh.hpp           # Mesh data
│       │   ├── camera.hpp         # Camera
│       │   └── command.hpp        # Command buffer
│       │
│       ├── ui/                    # void_ui - User interface
│       │   ├── ui.hpp
│       │   ├── widget.hpp         # Widget base
│       │   ├── layout.hpp         # Layout system
│       │   ├── style.hpp          # Styling
│       │   └── font.hpp           # Font rendering
│       │
│       ├── presenter/             # void_presenter - Window/surface
│       │   ├── presenter.hpp
│       │   ├── window.hpp         # Window management
│       │   ├── surface.hpp        # Render surface
│       │   └── swapchain.hpp      # Swapchain
│       │
│       ├── compositor/            # void_compositor - Display compositor
│       │   ├── compositor.hpp
│       │   ├── frame.hpp          # Frame scheduling
│       │   ├── vrr.hpp            # Variable refresh rate
│       │   └── hdr.hpp            # HDR support
│       │
│       ├── xr/                    # void_xr - XR/VR/AR
│       │   ├── xr.hpp
│       │   ├── system.hpp         # XR system
│       │   ├── view.hpp           # Stereo views
│       │   ├── input.hpp          # XR input
│       │   └── hand.hpp           # Hand tracking
│       │
│       ├── kernel/                # void_kernel - Core runtime
│       │   ├── kernel.hpp
│       │   ├── supervisor.hpp     # Supervision tree
│       │   ├── namespace.hpp      # Namespace isolation
│       │   └── sandbox.hpp        # Capability sandbox
│       │
│       ├── engine/                # void_engine - Application framework
│       │   ├── engine.hpp
│       │   ├── app.hpp            # Application interface
│       │   ├── config.hpp         # Engine configuration
│       │   └── lifecycle.hpp      # Lifecycle management
│       │
│       ├── physics/               # void_physics - Physics simulation
│       │   ├── physics.hpp
│       │   ├── world.hpp          # Physics world
│       │   ├── rigidbody.hpp      # Rigid bodies
│       │   ├── collider.hpp       # Colliders
│       │   └── raycast.hpp        # Raycasting
│       │
│       ├── triggers/              # void_triggers - Trigger volumes
│       │   ├── triggers.hpp
│       │   ├── volume.hpp         # Trigger volume
│       │   └── event.hpp          # Trigger events
│       │
│       ├── combat/                # void_combat - Combat system
│       │   ├── combat.hpp
│       │   ├── health.hpp         # Health component
│       │   ├── damage.hpp         # Damage system
│       │   └── weapon.hpp         # Weapons
│       │
│       ├── inventory/             # void_inventory - Inventory system
│       │   ├── inventory.hpp
│       │   ├── item.hpp           # Item definition
│       │   ├── container.hpp      # Containers
│       │   └── equipment.hpp      # Equipment slots
│       │
│       ├── audio/                 # void_audio - Audio system
│       │   ├── audio.hpp
│       │   ├── source.hpp         # Audio source
│       │   ├── listener.hpp       # Audio listener
│       │   └── mixer.hpp          # Audio mixer
│       │
│       ├── ai/                    # void_ai - AI system
│       │   ├── ai.hpp
│       │   ├── behavior_tree.hpp  # Behavior trees
│       │   ├── navmesh.hpp        # Navigation mesh
│       │   ├── pathfinding.hpp    # Pathfinding
│       │   └── senses.hpp         # AI perception
│       │
│       ├── gamestate/             # void_gamestate - Game state
│       │   ├── gamestate.hpp
│       │   ├── state.hpp          # State machine
│       │   ├── variable.hpp       # Game variables
│       │   └── save.hpp           # Save/load
│       │
│       ├── hud/                   # void_hud - HUD system
│       │   ├── hud.hpp
│       │   ├── element.hpp        # HUD elements
│       │   └── binding.hpp        # Data binding
│       │
│       ├── graph/                 # void_graph - Visual scripting
│       │   ├── graph.hpp
│       │   ├── node.hpp           # Graph nodes
│       │   ├── pin.hpp            # Node pins
│       │   ├── connection.hpp     # Connections
│       │   └── executor.hpp       # Graph execution
│       │
│       ├── script/                # void_script - VoidScript
│       │   ├── script.hpp
│       │   ├── lexer.hpp          # Tokenizer
│       │   ├── parser.hpp         # Parser
│       │   ├── ast.hpp            # AST types
│       │   └── interpreter.hpp    # Interpreter
│       │
│       ├── scripting/             # void_scripting - WASM plugins
│       │   ├── scripting.hpp
│       │   ├── host.hpp           # Plugin host
│       │   ├── module.hpp         # WASM module
│       │   └── api.hpp            # Host API
│       │
│       ├── cpp/                   # void_cpp - Native C++ classes
│       │   ├── cpp.hpp
│       │   ├── library.hpp        # DLL loading
│       │   ├── registry.hpp       # Class registry
│       │   └── instance.hpp       # Class instances
│       │
│       ├── shell/                 # void_shell - Debug console
│       │   ├── shell.hpp
│       │   ├── command.hpp        # Commands
│       │   └── parser.hpp         # Command parser
│       │
│       ├── editor/                # void_editor - Editor
│       │   ├── editor.hpp
│       │   ├── inspector.hpp      # Property inspector
│       │   ├── scene_view.hpp     # Scene view
│       │   └── asset_browser.hpp  # Asset browser
│       │
│       ├── runtime/               # void_runtime - Runtime bootstrap
│       │   ├── runtime.hpp
│       │   ├── bootstrap.hpp      # Bootstrap
│       │   └── scene_loader.hpp   # Scene loading
│       │
│       └── services/              # void_services - System services
│           ├── services.hpp
│           ├── registry.hpp       # Service registry
│           └── service.hpp        # Service interface
│
├── src/                           # IMPLEMENTATION
│   ├── math/
│   │   ├── CMakeLists.txt         # Module build config
│   │   ├── transform.cpp
│   │   └── bounds.cpp
│   │
│   ├── memory/
│   │   ├── CMakeLists.txt
│   │   ├── arena.cpp
│   │   ├── pool.cpp
│   │   └── platform/              # Platform-specific
│   │       ├── windows.cpp
│   │       └── posix.cpp
│   │
│   ├── structures/
│   │   └── CMakeLists.txt         # Header-only, tests only
│   │
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   ├── plugin.cpp
│   │   ├── hot_reload.cpp
│   │   ├── type_info.cpp
│   │   └── internal/              # Module-private headers
│   │       └── plugin_impl.hpp
│   │
│   ├── event/
│   │   ├── CMakeLists.txt
│   │   └── bus.cpp
│   │
│   ├── ecs/
│   │   ├── CMakeLists.txt
│   │   ├── world.cpp
│   │   ├── archetype.cpp
│   │   └── query.cpp
│   │
│   ├── ir/
│   │   ├── CMakeLists.txt
│   │   ├── value.cpp
│   │   ├── patch.cpp
│   │   ├── transaction.cpp
│   │   └── patch_bus.cpp
│   │
│   ├── asset/
│   │   ├── CMakeLists.txt
│   │   ├── cache.cpp
│   │   ├── server.cpp
│   │   └── loaders/               # Asset loaders
│   │       ├── texture_loader.cpp
│   │       ├── mesh_loader.cpp
│   │       └── scene_loader.cpp
│   │
│   ├── shader/
│   │   ├── CMakeLists.txt
│   │   ├── compiler.cpp
│   │   └── reflection.cpp
│   │
│   ├── render/
│   │   ├── CMakeLists.txt
│   │   ├── device.cpp
│   │   ├── render_graph.cpp
│   │   ├── layer.cpp
│   │   ├── material.cpp
│   │   └── backends/              # GPU backends
│   │       ├── vulkan/
│   │       │   ├── vk_device.cpp
│   │       │   └── vk_swapchain.cpp
│   │       └── webgpu/
│   │           └── wgpu_device.cpp
│   │
│   ├── ui/
│   │   ├── CMakeLists.txt
│   │   ├── widget.cpp
│   │   └── layout.cpp
│   │
│   ├── presenter/
│   │   ├── CMakeLists.txt
│   │   ├── window.cpp
│   │   └── platform/
│   │       ├── windows.cpp
│   │       ├── linux.cpp
│   │       └── macos.cpp
│   │
│   ├── compositor/
│   │   ├── CMakeLists.txt
│   │   ├── compositor.cpp
│   │   └── frame.cpp
│   │
│   ├── xr/
│   │   ├── CMakeLists.txt
│   │   ├── system.cpp
│   │   └── backends/
│   │       └── openxr/
│   │           └── openxr_backend.cpp
│   │
│   ├── kernel/
│   │   ├── CMakeLists.txt
│   │   ├── kernel.cpp
│   │   └── supervisor.cpp
│   │
│   ├── engine/
│   │   ├── CMakeLists.txt
│   │   ├── engine.cpp
│   │   └── app.cpp
│   │
│   ├── physics/
│   │   ├── CMakeLists.txt
│   │   ├── world.cpp
│   │   └── collider.cpp
│   │
│   ├── triggers/
│   │   ├── CMakeLists.txt
│   │   └── volume.cpp
│   │
│   ├── combat/
│   │   ├── CMakeLists.txt
│   │   ├── health.cpp
│   │   └── damage.cpp
│   │
│   ├── inventory/
│   │   ├── CMakeLists.txt
│   │   └── container.cpp
│   │
│   ├── audio/
│   │   ├── CMakeLists.txt
│   │   ├── mixer.cpp
│   │   └── backends/
│   │       └── miniaudio/
│   │           └── ma_backend.cpp
│   │
│   ├── ai/
│   │   ├── CMakeLists.txt
│   │   ├── behavior_tree.cpp
│   │   ├── navmesh.cpp
│   │   └── pathfinding.cpp
│   │
│   ├── gamestate/
│   │   ├── CMakeLists.txt
│   │   ├── state.cpp
│   │   └── save.cpp
│   │
│   ├── hud/
│   │   ├── CMakeLists.txt
│   │   └── element.cpp
│   │
│   ├── graph/
│   │   ├── CMakeLists.txt
│   │   ├── graph.cpp
│   │   ├── node.cpp
│   │   └── executor.cpp
│   │
│   ├── script/
│   │   ├── CMakeLists.txt
│   │   ├── lexer.cpp
│   │   ├── parser.cpp
│   │   └── interpreter.cpp
│   │
│   ├── scripting/
│   │   ├── CMakeLists.txt
│   │   ├── host.cpp
│   │   └── module.cpp
│   │
│   ├── cpp/
│   │   ├── CMakeLists.txt
│   │   ├── library.cpp
│   │   └── registry.cpp
│   │
│   ├── shell/
│   │   ├── CMakeLists.txt
│   │   ├── shell.cpp
│   │   └── command.cpp
│   │
│   ├── editor/
│   │   ├── CMakeLists.txt
│   │   ├── editor.cpp
│   │   └── inspector.cpp
│   │
│   ├── runtime/
│   │   ├── CMakeLists.txt
│   │   ├── bootstrap.cpp
│   │   └── scene_loader.cpp
│   │
│   ├── services/
│   │   ├── CMakeLists.txt
│   │   └── registry.cpp
│   │
│   └── main.cpp                   # Main entry point
│
├── tests/                         # TESTS
│   ├── CMakeLists.txt
│   ├── test_main.cpp              # Catch2 main
│   │
│   ├── math/
│   │   ├── test_vec.cpp
│   │   ├── test_mat.cpp
│   │   ├── test_quat.cpp
│   │   └── test_transform.cpp
│   │
│   ├── structures/
│   │   ├── test_sparse_set.cpp
│   │   ├── test_slot_map.cpp
│   │   └── test_ring_buffer.cpp
│   │
│   ├── ecs/
│   │   ├── test_entity.cpp
│   │   ├── test_world.cpp
│   │   └── test_query.cpp
│   │
│   ├── ir/
│   │   ├── test_value.cpp
│   │   ├── test_patch.cpp
│   │   └── test_transaction.cpp
│   │
│   ├── render/
│   │   ├── test_render_graph.cpp
│   │   └── test_material.cpp
│   │
│   └── integration/               # Integration tests
│       ├── test_hot_reload.cpp
│       ├── test_ecs_render.cpp
│       └── test_scene_load.cpp
│
├── benchmarks/                    # BENCHMARKS
│   ├── CMakeLists.txt
│   ├── bench_main.cpp
│   ├── bench_ecs.cpp
│   ├── bench_render_graph.cpp
│   └── bench_allocators.cpp
│
├── examples/                      # EXAMPLES
│   ├── CMakeLists.txt
│   ├── hello_triangle/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── ecs_demo/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── hot_reload_demo/
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── xr_demo/
│       ├── CMakeLists.txt
│       └── main.cpp
│
├── third_party/                   # EXTERNAL DEPENDENCIES
│   ├── CMakeLists.txt
│   ├── glm/                       # GLM (header-only)
│   ├── catch2/                    # Catch2 testing
│   ├── spdlog/                    # Logging
│   ├── toml++/                    # TOML parsing
│   ├── stb/                       # stb_image, etc.
│   ├── vma/                       # Vulkan Memory Allocator
│   └── wgpu/                      # WebGPU headers
│
├── assets/                        # DEFAULT ASSETS
│   ├── shaders/
│   │   ├── common/
│   │   │   ├── types.wgsl
│   │   │   └── math.wgsl
│   │   ├── pbr.wgsl
│   │   ├── skybox.wgsl
│   │   └── ui.wgsl
│   ├── textures/
│   │   ├── default_white.png
│   │   ├── default_normal.png
│   │   └── error.png
│   └── meshes/
│       ├── cube.obj
│       ├── sphere.obj
│       └── plane.obj
│
├── tools/                         # BUILD TOOLS
│   ├── shader_compiler/           # Offline shader compilation
│   │   └── main.cpp
│   └── asset_packer/              # Asset packaging
│       └── main.cpp
│
├── doc/                           # DOCUMENTATION (existing)
│   ├── architecture.md
│   ├── migration-plan.md
│   ├── project-structure.md       # This file
│   ├── modules/
│   ├── guides/
│   └── formats/
│
├── legacy/                        # LEGACY RUST (reference)
│   └── crates/
│
└── .claude/                       # CLAUDE CODE (existing)
    ├── skills/
    └── agents/
```

---

## Module Dependency Graph

```
                    ┌─────────────────────────────────────────────────────┐
                    │                     runtime                          │
                    └─────────────────────────────────────────────────────┘
                                              │
                    ┌─────────────────────────┼─────────────────────────┐
                    ▼                         ▼                         ▼
              ┌──────────┐             ┌──────────┐             ┌──────────┐
              │  editor  │             │  engine  │             │   shell  │
              └──────────┘             └──────────┘             └──────────┘
                    │                         │                         │
        ┌───────────┼───────────┬────────────┼────────────┬────────────┘
        ▼           ▼           ▼            ▼            ▼
   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌──────────┐ ┌──────────┐
   │  graph  │ │ script  │ │scripting│ │   cpp    │ │  kernel  │
   └─────────┘ └─────────┘ └─────────┘ └──────────┘ └──────────┘
        │           │           │            │            │
        └───────────┴───────────┴────────────┴────────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    ▼              ▼              ▼
              ┌──────────┐  ┌──────────┐  ┌──────────┐
              │ services │  │  render  │  │    xr    │
              └──────────┘  └──────────┘  └──────────┘
                    │              │              │
        ┌───────────┼──────────────┼──────────────┤
        ▼           ▼              ▼              ▼
   ┌─────────┐ ┌─────────┐  ┌──────────┐  ┌───────────┐
   │  asset  │ │ shader  │  │compositor│  │ presenter │
   └─────────┘ └─────────┘  └──────────┘  └───────────┘
        │           │              │              │
        └───────────┴──────────────┴──────────────┘
                           │
                    ┌──────┴──────┐
                    ▼             ▼
              ┌──────────┐ ┌──────────┐
              │    ui    │ │    ir    │
              └──────────┘ └──────────┘
                    │             │
                    └──────┬──────┘
                           ▼
    ┌────────────────────────────────────────────────────────────┐
    │                         GAMEPLAY                            │
    │  physics │ triggers │ combat │ inventory │ audio │ ai │ hud │
    │  gamestate                                                  │
    └────────────────────────────────────────────────────────────┘
                           │
                    ┌──────┴──────┐
                    ▼             ▼
              ┌──────────┐ ┌──────────┐
              │   ecs    │ │  event   │
              └──────────┘ └──────────┘
                    │             │
                    └──────┬──────┘
                           ▼
                    ┌──────────────┐
                    │     core     │
                    └──────────────┘
                           │
            ┌──────────────┼──────────────┐
            ▼              ▼              ▼
      ┌──────────┐  ┌──────────┐  ┌────────────┐
      │   math   │  │  memory  │  │ structures │
      └──────────┘  └──────────┘  └────────────┘
```

---

## Build Targets

| Target | Type | Description |
|--------|------|-------------|
| `void_math` | INTERFACE | Header-only math library |
| `void_memory` | STATIC | Memory allocators |
| `void_structures` | INTERFACE | Header-only data structures |
| `void_core` | STATIC | Core utilities |
| `void_ecs` | STATIC | Entity Component System |
| `void_render` | STATIC | Rendering system |
| `void_engine` | STATIC | Engine framework |
| `void_runtime` | EXECUTABLE | Main runtime executable |
| `void_tests` | EXECUTABLE | Test runner |
| `void_benchmarks` | EXECUTABLE | Benchmark runner |

---

## CMake Usage

```bash
# Configure (Debug)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all
cmake --build build --parallel

# Build specific module
cmake --build build --target void_ecs

# Run tests
ctest --test-dir build --output-on-failure

# Run benchmarks
./build/void_benchmarks
```

---

## Adding a New Module

1. Create header directory: `include/void_engine/<module>/`
2. Create source directory: `src/<module>/`
3. Create `src/<module>/CMakeLists.txt`
4. Add to root `CMakeLists.txt`
5. Create test directory: `tests/<module>/`
6. Add tests to `tests/CMakeLists.txt`
