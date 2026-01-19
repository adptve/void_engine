# void_engine Migration Plan: Rust → C++

## Overview

This document outlines the strategy for porting the legacy Rust void_engine to C++20. The migration follows a bottom-up approach, porting foundation crates first before higher-level systems.

## Migration Principles

1. **Mirror public APIs** - Translate Rust traits to C++ concepts/interfaces
2. **Preserve hot-reload semantics** - All components remain hot-swappable
3. **Tests first** - Port tests before implementation, verify identical behavior
4. **RAII over lifetimes** - Replace Rust ownership with C++ smart pointers and RAII
5. **Use Rust as oracle** - Generate test data from Rust, verify C++ produces same results

## Crate Porting Order

### Phase 1: Foundation (Weeks 1-2)

Zero-dependency crates that everything else builds on.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 1 | `void_math` | `math/` | Low | Vec3, Mat4, Quat, Transform |
| 2 | `void_memory` | `memory/` | Medium | Arena, Pool, StackAllocator, FreeList |
| 3 | `void_structures` | `structures/` | Medium | SparseSet, SlotMap, LockFreeQueue, BitSet |
| 4 | `void_core` | `core/` | High | Plugin, TypeRegistry, Handle, HotReload |

### Phase 2: Data Layer (Weeks 3-4)

Core data management systems.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 5 | `void_event` | `event/` | Low | EventBus, Event, Subscription |
| 6 | `void_ecs` | `ecs/` | High | World, Entity, Component, Query, System |
| 7 | `void_ir` | `ir/` | Medium | Patch, Transaction, PatchBus, Value |
| 8 | `void_asset` | `asset/` | Medium | Asset, Handle, Loader, Cache |
| 9 | `void_asset_server` | `asset_server/` | Medium | AssetInjector, FileWatcher, HotReload |
| 10 | `void_services` | `services/` | Medium | ServiceRegistry, EventBus, Session |

### Phase 3: Rendering (Weeks 5-8)

Graphics and presentation.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 11 | `void_shader` | `shader/` | High | ShaderCompiler, Reflection, HotReload |
| 12 | `void_render` | `render/` | Very High | RenderGraph, Layer, Material, Mesh |
| 13 | `void_ui` | `ui/` | Medium | Widget, Theme, Font |
| 14 | `void_presenter` | `presenter/` | High | Surface, Swapchain, Presenter |
| 15 | `void_compositor` | `compositor/` | Very High | Compositor, FrameScheduler, VRR, HDR |
| 16 | `void_xr` | `xr/` | High | XrSystem, View, Pose, HandTracking |

### Phase 4: Engine Core (Weeks 9-10)

Application framework and runtime.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 17 | `void_engine` | `engine/` | High | App, Lifecycle, Config |
| 18 | `void_kernel` | `kernel/` | Very High | Kernel, Supervisor, Sandbox |

### Phase 5: Gameplay Systems (Weeks 11-14)

Game-specific systems.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 19 | `void_physics` | `physics/` | High | Rigidbody, Collider, PhysicsWorld |
| 20 | `void_triggers` | `triggers/` | Low | TriggerVolume, TriggerEvent |
| 21 | `void_combat` | `combat/` | Medium | Health, Damage, Weapon |
| 22 | `void_inventory` | `inventory/` | Medium | Item, Container, Equipment |
| 23 | `void_audio` | `audio/` | Medium | AudioSource, Listener, Mixer |
| 24 | `void_ai` | `ai/` | High | BehaviorTree, NavMesh, Senses |
| 25 | `void_gamestate` | `gamestate/` | Medium | State, Variable, SaveLoad |
| 26 | `void_hud` | `hud/` | Medium | HudElement, Binding |

### Phase 6: Scripting (Weeks 15-16)

Scripting and visual programming systems.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 27 | `void_graph` | `graph/` | High | Graph, Node, Connection, GraphExecutor |
| 28 | `void_script` | `script/` | Medium | VoidScript, Lexer, Parser, Interpreter |
| 29 | `void_scripting` | `scripting/` | High | PluginHost, WasmModule, HostApi |
| 30 | `void_cpp` | `cpp/` | High | CppClassRegistry, CppLibrary, HotReload |

### Phase 7: Tools (Weeks 17-18)

Development tools and interfaces.

| Priority | Crate | C++ Module | Complexity | Key Types |
|----------|-------|------------|------------|-----------|
| 31 | `void_shell` | `shell/` | Low | Command, Parser, Session |
| 32 | `void_editor` | `editor/` | Very High | Editor, Inspector, SceneView |
| 33 | `void_runtime` | `runtime/` | High | Runtime, Bootstrap, SceneLoader |

---

## Per-Crate Migration Details

### void_math

**Source files:**
- `legacy/crates/void_math/src/transform.rs`

**C++ approach:**
- Use GLM as backend or implement custom
- Match API exactly for test compatibility

```cpp
// Rust
pub struct Transform {
    pub position: Vec3,
    pub rotation: Quat,
    pub scale: Vec3,
}

// C++
struct Transform {
    glm::vec3 position{0.f};
    glm::quat rotation{glm::identity<glm::quat>()};
    glm::vec3 scale{1.f};

    glm::mat4 to_matrix() const;
    Transform inverse() const;
    Transform operator*(Transform const& rhs) const;
};
```

### void_memory

**Source files:**
- `legacy/crates/void_memory/src/arena.rs`
- `legacy/crates/void_memory/src/pool.rs`
- `legacy/crates/void_memory/src/stack.rs`
- `legacy/crates/void_memory/src/free_list.rs`

**C++ approach:**
- Implement as pmr-compatible allocators
- Use `std::pmr::memory_resource` as base

```cpp
// Arena allocator
class ArenaResource : public std::pmr::memory_resource {
    void* do_allocate(size_t bytes, size_t alignment) override;
    void do_deallocate(void* p, size_t bytes, size_t alignment) override;
    bool do_is_equal(memory_resource const& other) const noexcept override;

    void reset();  // Bulk deallocation
};

// Pool allocator
template<size_t BlockSize>
class PoolResource : public std::pmr::memory_resource { /* ... */ };
```

### void_structures

**Source files:**
- `legacy/crates/void_structures/src/sparse_set.rs`
- `legacy/crates/void_structures/src/slot_map.rs`
- `legacy/crates/void_structures/src/lock_free_queue.rs`
- `legacy/crates/void_structures/src/bitset.rs`

**C++ approach:**
- Template-based containers
- Lock-free queue uses `std::atomic`

```cpp
template<typename T>
class SparseSet {
    std::vector<T> m_dense;
    std::vector<uint32_t> m_sparse;
public:
    bool contains(uint32_t id) const;
    T* get(uint32_t id);
    void insert(uint32_t id, T value);
    void remove(uint32_t id);
};

template<typename T, typename Key = uint32_t>
class SlotMap {
    struct Slot { T value; uint32_t generation; };
    std::vector<Slot> m_slots;
    std::queue<uint32_t> m_free_list;
};
```

### void_core

**Source files:**
- `legacy/crates/void_core/src/plugin.rs`
- `legacy/crates/void_core/src/hot_reload.rs`
- `legacy/crates/void_core/src/id.rs`
- `legacy/crates/void_core/src/handle.rs`
- `legacy/crates/void_core/src/type_registry.rs`
- `legacy/crates/void_core/src/error.rs`

**C++ approach:**
- Plugins via DLL loading (dlopen/LoadLibrary)
- Type registry using RTTI or custom type IDs
- Hot-reload via binary serialization

```cpp
// Plugin interface (exported from DLL)
extern "C" {
    IPlugin* create_plugin();
    void destroy_plugin(IPlugin* p);
}

// Dynamic loading
class PluginLoader {
    void* m_handle = nullptr;
public:
    bool load(std::filesystem::path path);
    void unload();
    IPlugin* create_instance();
};
```

### void_ecs

**Source files:**
- `legacy/crates/void_ecs/src/world.rs`
- `legacy/crates/void_ecs/src/query.rs`
- `legacy/crates/void_ecs/src/system.rs`

**C++ approach:**
- Archetype-based storage (like EnTT, flecs)
- Type-safe queries with variadic templates

```cpp
// Rust
for (entity, pos, vel) in world.query::<(Entity, &Position, &mut Velocity)>() {
    pos.x += vel.x * dt;
}

// C++
for (auto [entity, pos, vel] : world.query<Position, Velocity>()) {
    pos.x += vel.x * dt;
}
```

### void_ir

**Source files:**
- `legacy/crates/void_ir/src/patch.rs`
- `legacy/crates/void_ir/src/transaction.rs`

**C++ approach:**
- Variant-based patch types
- Transaction builder pattern

```cpp
auto tx = TransactionBuilder(namespace_id)
    .create_entity(entity_ref, "Player")
    .set_component(entity_ref, "Transform", transform_data)
    .build();
patch_bus.submit(std::move(tx));
```

### void_render

**Source files:**
- `legacy/crates/void_render/src/graph.rs`
- `legacy/crates/void_render/src/resource.rs`

**C++ approach:**
- Frame graph for render passes
- Vulkan/wgpu backend

```cpp
// Add render pass
graph.add_pass("geometry", [](PassBuilder& builder) {
    builder.write_color("gbuffer_albedo");
    builder.write_color("gbuffer_normal");
    builder.write_depth("depth");
}, [](CommandBuffer& cmd, PassData const& data) {
    // Render geometry
});
```

### void_shader

**Source files:**
- `legacy/crates/void_shader/src/hot_reload.rs`
- `legacy/crates/void_shader/src/reflect.rs`
- `legacy/crates/void_shader/src/validator.rs`

**C++ approach:**
- SPIRV-Cross for reflection
- File watcher for hot-reload
- Shader compilation pipeline

```cpp
class ShaderCompiler {
public:
    Result<ShaderModule> compile(std::filesystem::path wgsl_path);
    void watch(std::filesystem::path path, std::function<void()> on_change);
};
```

### void_graph

**Source files:**
- `legacy/crates/void_graph/src/graph.rs`
- `legacy/crates/void_graph/src/node.rs`
- `legacy/crates/void_graph/src/value.rs`

**C++ approach:**
- Blueprint-compatible visual scripting
- Node-based execution with data/exec pins
- Hot-reloadable graphs

```cpp
class Graph {
public:
    NodeId add_node(std::string_view type);
    void connect(NodeId from, PinId from_pin, NodeId to, PinId to_pin);
    void add_variable(Variable var);
};

class GraphExecutor {
public:
    void execute(NodeId entry_point);
};
```

### void_xr

**Source files:**
- `legacy/crates/void_xr/src/lib.rs`
- `legacy/crates/void_xr/src/openxr_backend.rs`

**C++ approach:**
- OpenXR backend for VR/AR
- Runtime mode switching (VR, AR, Desktop)
- Hand tracking and spatial anchors

```cpp
class XrSystem {
public:
    void set_mode(XrMode mode);
    std::vector<View> views() const;
    Pose head_pose() const;
    ControllerState controller_state(Hand hand) const;
    std::optional<HandState> hand_state(Hand hand) const;
};
```

### void_asset_server

**Source files:**
- `legacy/crates/void_asset_server/src/injector.rs`
- `legacy/crates/void_asset_server/src/watcher.rs`
- `legacy/crates/void_asset_server/src/loaders/`

**C++ approach:**
- File watching for hot-reload
- Multiple asset loaders (textures, meshes, scenes)
- Remote asset source support

```cpp
class AssetInjector {
public:
    void start();
    std::vector<InjectionEvent> update();
};

class FileWatcher {
public:
    void watch(std::filesystem::path path);
    std::vector<FileChange> poll_events();
};
```

### void_services

**Source files:**
- `legacy/crates/void_services/src/registry.rs`
- `legacy/crates/void_services/src/service.rs`

**C++ approach:**
- Service lifecycle management
- Inter-service communication via channels
- Health monitoring

```cpp
class ServiceRegistry {
public:
    void register_service(std::string_view name, std::unique_ptr<IService> service);
    template<typename T> T* get(std::string_view name);
    void start_all();
    void stop_all();
};
```

### void_scripting

**Source files:**
- `legacy/crates/void_scripting/src/host.rs`
- `legacy/crates/void_scripting/src/plugin.rs`
- `legacy/crates/void_scripting/src/api.rs`

**C++ approach:**
- WASM module loading
- Host function bindings
- Plugin lifecycle management

```cpp
class PluginHost {
public:
    PluginId load_plugin(std::filesystem::path wasm_path);
    void call_on_spawn(PluginId id, EntityId entity);
    void call_on_update(PluginId id, EntityId entity, float dt);
};
```

### void_script

**Source files:**
- `legacy/crates/void_script/src/lexer.rs`
- `legacy/crates/void_script/src/parser.rs`
- `legacy/crates/void_script/src/interpreter.rs`

**C++ approach:**
- Simple scripting language (VoidScript)
- REPL integration
- Kernel integration via ScriptContext

```cpp
class VoidScript {
public:
    Value execute(std::string_view source);
    Value eval(std::string_view expression);
    void set_var(std::string_view name, Value value);
    void register_fn(std::string_view name, NativeFunction fn);
};
```

### void_compositor

**Source files:**
- `legacy/crates/void_compositor/src/compositor.rs`
- `legacy/crates/void_compositor/src/frame.rs`
- `legacy/crates/void_compositor/src/vrr.rs`
- `legacy/crates/void_compositor/src/hdr.rs`

**C++ approach:**
- Wayland compositor (Smithay-like)
- DRM/KMS display management
- VRR and HDR support

```cpp
class Compositor {
public:
    void initialize(CompositorConfig const& config);
    CompositorCapabilities capabilities() const;
    void begin_frame();
    void end_frame();
};
```

### void_cpp

**Source files:**
- `legacy/crates/void_cpp/src/library.rs`
- `legacy/crates/void_cpp/src/registry.rs`
- `legacy/crates/void_cpp/src/instance.rs`

**C++ approach:**
- Native C++ class loading from DLLs
- Hot-reload with state preservation
- Unreal-like actor lifecycle

```cpp
class CppClassRegistry {
public:
    void load_library(std::filesystem::path dll_path);
    CppClassInstance* create_instance(std::string_view class_name, EntityId entity);
    void reload_library(std::string_view name);  // Hot-reload
};
```

---

## Rust → C++ Translation Patterns

### Ownership

```rust
// Rust: ownership transfer
fn take(value: MyStruct) { }

// C++: move semantics
void take(MyStruct value);  // Copy or move
void take(MyStruct&& value);  // Move only
```

### Borrowing

```rust
// Rust: immutable borrow
fn read(value: &MyStruct) { }

// C++: const reference
void read(MyStruct const& value);
```

```rust
// Rust: mutable borrow
fn modify(value: &mut MyStruct) { }

// C++: non-const reference
void modify(MyStruct& value);
```

### Option/Result

```rust
// Rust
fn find(id: u32) -> Option<&Entity> { }
fn load(path: &str) -> Result<Asset, Error> { }

// C++
std::optional<Entity*> find(uint32_t id);
std::expected<Asset, Error> load(std::string_view path);
// Or custom Result<T, E> type
```

### Traits

```rust
// Rust trait
trait Component: Clone + Send + Sync + 'static { }

// C++ concept
template<typename T>
concept Component = std::copyable<T> && std::is_trivially_copyable_v<T>;

// Or interface
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual std::unique_ptr<IComponent> clone() const = 0;
};
```

### Enums with Data

```rust
// Rust enum
enum PatchKind {
    Entity(EntityPatch),
    Component(ComponentPatch),
    Layer(LayerPatch),
}

// C++ variant
using PatchKind = std::variant<EntityPatch, ComponentPatch, LayerPatch>;

// Pattern matching
std::visit(overloaded{
    [](EntityPatch const& p) { /* ... */ },
    [](ComponentPatch const& p) { /* ... */ },
    [](LayerPatch const& p) { /* ... */ }
}, patch);
```

### Error Handling

```rust
// Rust: ? operator
fn load() -> Result<Data, Error> {
    let bytes = read_file(path)?;
    let data = parse(bytes)?;
    Ok(data)
}

// C++: expected + monadic operations (C++23)
std::expected<Data, Error> load() {
    return read_file(path)
        .and_then([](auto bytes) { return parse(bytes); });
}

// Or exceptions at boundaries
Data load() {
    auto bytes = read_file(path).value_or_throw();
    return parse(bytes).value_or_throw();
}
```

---

## Testing Strategy

### Unit Test Translation

```rust
// Rust test
#[test]
fn test_sparse_set_insert() {
    let mut set = SparseSet::new();
    set.insert(5, "hello");
    assert_eq!(set.get(5), Some(&"hello"));
}

// C++ test (Catch2)
TEST_CASE("SparseSet insert", "[structures]") {
    SparseSet<std::string> set;
    set.insert(5, "hello");
    REQUIRE(set.get(5) != nullptr);
    REQUIRE(*set.get(5) == "hello");
}
```

### Oracle Testing

1. Generate test inputs in Rust
2. Run through Rust implementation, capture outputs
3. Run same inputs through C++
4. Compare outputs

```bash
# Generate test data
cargo run --bin generate_test_data -- math > test_data/math.json

# Run C++ tests against same data
./void_engine_tests --test-data=test_data/math.json
```

### Hot-Reload Testing

```cpp
TEST_CASE("Shader hot-reload preserves state") {
    ShaderManager manager;
    auto handle = manager.load("test.wgsl");

    // Simulate file change
    modify_shader_file("test.wgsl");
    manager.check_for_changes();

    // Verify handle still valid, shader recompiled
    REQUIRE(manager.is_valid(handle));
    REQUIRE(manager.is_current(handle));
}
```

---

## Directory Structure

```
void_engine/
├── CMakeLists.txt
├── include/
│   └── void_engine/
│       ├── core/
│       │   ├── plugin.hpp
│       │   ├── handle.hpp
│       │   ├── type_registry.hpp
│       │   └── hot_reload.hpp
│       ├── math/
│       │   ├── vec.hpp
│       │   ├── mat.hpp
│       │   ├── quat.hpp
│       │   └── transform.hpp
│       ├── memory/
│       │   ├── arena.hpp
│       │   ├── pool.hpp
│       │   └── allocator.hpp
│       ├── structures/
│       │   ├── sparse_set.hpp
│       │   ├── slot_map.hpp
│       │   └── lock_free_queue.hpp
│       ├── ecs/
│       │   ├── world.hpp
│       │   ├── entity.hpp
│       │   ├── component.hpp
│       │   └── query.hpp
│       ├── ir/
│       │   ├── patch.hpp
│       │   ├── transaction.hpp
│       │   └── value.hpp
│       ├── render/
│       │   ├── graph.hpp
│       │   ├── layer.hpp
│       │   ├── material.hpp
│       │   └── mesh.hpp
│       └── engine/
│           ├── engine.hpp
│           └── app.hpp
├── src/
│   ├── core/
│   ├── math/
│   ├── memory/
│   ├── structures/
│   ├── ecs/
│   ├── ir/
│   ├── render/
│   ├── engine/
│   └── main.cpp
├── tests/
│   ├── core/
│   ├── math/
│   ├── ecs/
│   └── render/
├── doc/
├── legacy/           # Original Rust codebase (reference)
└── third_party/
    ├── glm/
    ├── vulkan/
    └── catch2/
```

---

## Build System

```cmake
cmake_minimum_required(VERSION 3.20)
project(void_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(VOID_BUILD_TESTS "Build unit tests" ON)
option(VOID_BUILD_EXAMPLES "Build examples" ON)
option(VOID_HOT_RELOAD "Enable hot-reload support" ON)

# Core modules (header-only or static)
add_library(void_math INTERFACE)
add_library(void_memory STATIC)
add_library(void_structures INTERFACE)
add_library(void_core STATIC)
add_library(void_ecs STATIC)
add_library(void_ir STATIC)
add_library(void_render STATIC)
add_library(void_engine STATIC)

# Main executable
add_executable(void_runtime src/main.cpp)
target_link_libraries(void_runtime PRIVATE void_engine)

# Tests
if(VOID_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

---

## Milestones

### M1: Foundation Complete (Week 2)
- [ ] void_math with full test coverage
- [ ] void_memory allocators working
- [ ] void_structures containers implemented
- [ ] void_core plugin system loading DLLs

### M2: Data Layer Working (Week 4)
- [ ] void_ecs: Entity creation/destruction, queries
- [ ] void_ir: Patch bus, transactions
- [ ] void_asset: Asset loading and caching
- [ ] void_asset_server: File watching, hot-reload
- [ ] void_services: Service registry operational

### M3: Basic Rendering (Week 8)
- [ ] void_render: Render graph compiling
- [ ] void_shader: Shader compilation and hot-reload
- [ ] void_presenter: Surface/swapchain management
- [ ] void_compositor: Basic frame scheduling
- [ ] Layer composition working

### M4: Hot-Reload Demo (Week 10)
- [ ] Full hot-reload cycle working
- [ ] Shader live editing
- [ ] Component hot-swap
- [ ] State preservation verified
- [ ] void_kernel: Basic supervisor tree

### M5: Gameplay Systems (Week 14)
- [ ] void_physics: Physics integration
- [ ] void_triggers: Trigger volumes
- [ ] void_ai: Basic pathfinding
- [ ] void_audio: Audio playback
- [ ] void_combat, void_inventory, void_gamestate, void_hud

### M6: Scripting Systems (Week 16)
- [ ] void_graph: Visual scripting (Blueprint-like)
- [ ] void_script: VoidScript interpreter
- [ ] void_scripting: WASM plugin host
- [ ] void_cpp: C++ class loading with hot-reload

### M7: XR Support (Week 17)
- [ ] void_xr: OpenXR backend integration
- [ ] VR rendering pipeline
- [ ] Hand tracking
- [ ] Controller input

### M8: Production Ready (Week 18)
- [ ] void_editor: Editor integration
- [ ] void_shell: Debug console
- [ ] void_runtime: Full bootstrap
- [ ] Full test coverage (all 33 modules)
- [ ] Performance benchmarks
- [ ] Documentation complete
