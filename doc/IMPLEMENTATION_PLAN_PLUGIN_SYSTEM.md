# Plugin System Implementation Plan

> **CRITICAL**: This document is the source of truth for implementing the plugin system.
> Read this ENTIRELY before making ANY code changes.

---

## Core Ideology (NON-NEGOTIABLE)

### Why This Engine Exists

This engine exists for **one purpose**: to enable external creators to build complete game experiences through **remotely-loaded plugins**. Everything else is infrastructure to support this goal.

### The Fundamental Architecture

```
PLUGINS DEFINE EVERYTHING → ENGINE RENDERS IT
```

- **Engine owns**: Renderer, ECS infrastructure, Kernel, Asset loading
- **Plugins own**: All game logic, components, systems, behavior
- **Contract connects them**: IPlugin interface, PluginContext, make_renderable()

### What Makes This Different

This is NOT a traditional game engine where you compile game code into the engine.
This IS a platform where:
- Plugins are DLLs loaded at runtime
- Plugins can be hot-reloaded without restart
- Plugins define components as C++ structs with JSON factories
- Plugins register systems that run in kernel stages
- Plugin state survives hot-reload via snapshot/restore
- Everything is tracked in ECS (including plugin state itself)

---

## Absolute Rules (VIOLATIONS ARE BUGS)

### DO NOT

1. **DO NOT stub implementations** - Every function must do real work or not exist
2. **DO NOT use placeholders** - No `return {};` or `// TODO: implement`
3. **DO NOT bypass the contract** - Plugins MUST use PluginContext, not direct access
4. **DO NOT hardcode game logic in engine** - Engine provides infrastructure only
5. **DO NOT create render components without the contract** - Use make_renderable()
6. **DO NOT skip hot-reload support** - Every plugin MUST implement snapshot/restore
7. **DO NOT ignore state management** - Plugin state MUST be ECS resources
8. **DO NOT create orphan code** - Everything must be wired and callable
9. **DO NOT duplicate functionality** - Use existing void_render components
10. **DO NOT break the stage system** - Systems run in kernel stages, period

### DO

1. **DO implement complete features** - Full error handling, edge cases, cleanup
2. **DO use the existing code** - void_render has production components/systems
3. **DO maintain hot-reload** - snapshot/restore patterns everywhere
4. **DO track ownership** - Plugins track entities they create
5. **DO use strong typing** - No void*, use proper interfaces
6. **DO write production code** - This ships to users, not a prototype
7. **DO preserve existing functionality** - Render components already work
8. **DO follow the contract** - IPlugin, PluginContext, make_renderable
9. **DO wire everything** - No dead code, all paths connected
10. **DO test hot-reload** - State must survive DLL swap

---

## CRITICAL: Module Architecture

> **This section is NON-NEGOTIABLE. Misunderstanding causes cascading bugs.**

### The Module Hierarchy

```
┌────────────────────────────────────────────────────────────────────────┐
│  void_ecs (include/void_engine/ecs/)                                    │
│                                                                         │
│  CORE ECS IMPLEMENTATION:                                               │
│    World, Entity, Component, Archetype, Query, Resources, Systems       │
│                                                                         │
│  This is JUST the data storage layer. Knows nothing about JSON/plugins. │
└────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ USES
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│  void_package (include/void_engine/package/)                            │
│                                                                         │
│  PACKAGE SYSTEM USING void_ecs:                                         │
│    ComponentSchemaRegistry, PrefabRegistry, WorldComposer, Loaders      │
│                                                                         │
│  ╔══════════════════════════════════════════════════════════════════╗  │
│  ║  ComponentSchemaRegistry IS THE SINGLE SOURCE OF TRUTH FOR       ║  │
│  ║  COMPONENT REGISTRATION AND JSON→COMPONENT CONVERSION.           ║  │
│  ║                                                                  ║  │
│  ║  ALL component factories MUST be registered here.                ║  │
│  ║  DO NOT create parallel factory systems.                         ║  │
│  ╚══════════════════════════════════════════════════════════════════╝  │
└────────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ USES
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│  void_plugin_api (include/void_engine/plugin_api/)                      │
│                                                                         │
│  PLUGIN API USING void_package:                                         │
│    IPlugin, PluginContext, RenderableDesc                               │
│                                                                         │
│  PluginContext::register_component() DELEGATES TO                       │
│  void_package::ComponentSchemaRegistry, not a parallel system!          │
└────────────────────────────────────────────────────────────────────────┘
```

### Key Types and Their Homes

| Type | Module | Purpose |
|------|--------|---------|
| `void_ecs::World` | void_ecs | ECS container (entities, components) |
| `void_ecs::Entity` | void_ecs | Entity handle |
| `void_ecs::ComponentId` | void_ecs | Component type identifier |
| `void_ecs::ComponentRegistry` | void_ecs | Low-level type registration |
| `void_package::ComponentSchemaRegistry` | void_package | **JSON→component factories** |
| `void_package::ComponentApplier` | void_package | Function type for JSON→entity |
| `void_package::WorldComposer` | void_package | World loading, prefab spawning |
| `void_plugin_api::PluginContext` | void_plugin_api | Engine access for plugins |
| `void_plugin_api::IPlugin` | void_plugin_api | Plugin interface |

### Component Registration (CORRECT WAY)

```cpp
// In PluginContext (void_plugin_api)
template<typename T>
ComponentId register_component(const std::string& name, ComponentApplier applier) {
    // 1. Register with void_ecs (low-level)
    ComponentId id = m_world->register_component<T>();

    // 2. Register schema with void_package (JSON factory)
    m_schema_registry->register_schema_with_factory(schema, nullptr, applier);

    return id;
}
```

### WHY ComponentSchemaRegistry Matters

1. **WorldComposer uses it** - Prefab loading calls `schema_registry.apply_to_entity()`
2. **Hot-reload uses it** - Schema migration requires knowing all schemas
3. **Plugin isolation** - Registry tracks source_plugin for each schema
4. **Validation** - JSON data validated against schema before applying
5. **Introspection** - Tools can enumerate all registered components

**If you bypass ComponentSchemaRegistry, prefabs won't spawn entities correctly.**

---

## Implementation Phases

### Phase 1: Plugin API Foundation
**Goal**: Define the contract between engine and plugins

#### Task 1.1: Create IPlugin Interface
**File**: `include/void_engine/plugin_api/plugin.hpp`

```cpp
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Identity
    virtual const char* id() const = 0;
    virtual Version version() const = 0;
    virtual std::span<const Dependency> dependencies() const = 0;

    // Lifecycle
    virtual Result<void> on_load(PluginContext& ctx) = 0;
    virtual Result<void> on_unload(PluginContext& ctx) = 0;

    // State (hot-reload)
    virtual PluginSnapshot snapshot() = 0;
    virtual Result<void> restore(const PluginSnapshot& snapshot) = 0;
    virtual void on_reloaded() {}

    // Introspection
    virtual std::span<const char*> component_names() const = 0;
    virtual std::span<const char*> system_names() const = 0;
};
```

**Acceptance Criteria**:
- [ ] Interface defined with all required methods
- [ ] PluginSnapshot struct defined for state serialization
- [ ] Dependency struct defined for plugin dependencies
- [ ] Version struct reused from void_core
- [ ] Export macros defined (PLUGIN_API)

#### Task 1.2: Create PluginContext
**File**: `include/void_engine/plugin_api/context.hpp`

```cpp
class PluginContext {
public:
    // Component registration
    template<typename T>
    ComponentId register_component(const char* name, ComponentFactory<T> factory);

    // System registration
    SystemId register_system(Stage stage, const char* name, SystemFunc func, int priority);

    // Event subscription
    template<typename E>
    SubscriptionId subscribe(EventHandler<E> handler);
    void unsubscribe(SubscriptionId id);

    // ECS access
    const World& world() const;
    World& world_mut();

    template<typename R> const R* resource() const;
    template<typename R> R* resource_mut();

    // Services
    EventBus& events();
    AssetServer& assets();
    Kernel& kernel();

    // Render contract
    const RenderComponentIds& render_components() const;
    Result<void> make_renderable(Entity e, const RenderableDesc& desc);
};
```

**Acceptance Criteria**:
- [ ] All registration APIs implemented
- [ ] ECS World access (const and mutable)
- [ ] Resource access (const and mutable)
- [ ] Event subscription with proper cleanup
- [ ] make_renderable implemented using engine render components
- [ ] RenderableDesc struct defined

#### Task 1.3: Create RenderableDesc (Render Contract)
**File**: `include/void_engine/plugin_api/renderable.hpp`

```cpp
struct RenderableDesc {
    // Mesh
    std::string mesh_builtin;  // "sphere", "cube", etc.
    std::string mesh_asset;    // Path to model file

    // Material
    struct {
        std::array<float, 4> albedo = {0.8f, 0.8f, 0.8f, 1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        std::string albedo_texture;
        std::string normal_texture;
    } material;

    // Visibility
    bool visible = true;
    std::uint32_t layer_mask = 0xFFFFFFFF;
    int render_order = 0;
};
```

**Acceptance Criteria**:
- [ ] Struct defined with all render options
- [ ] Default values are sensible
- [ ] Supports both builtin meshes and asset paths
- [ ] Material properties match MaterialComponent

---

### Phase 2: Engine Core Registration
**Goal**: Expose engine render components and systems to plugin system

#### Task 2.1: Register Engine Render Components
**File**: `src/runtime/runtime.cpp` (modify init_packages)

**Implementation**:
```cpp
void Runtime::register_engine_core_components() {
    auto* registry = m_packages->schema_registry.get();

    // Register with factories that create actual components
    registry->register_builtin<void_render::TransformComponent>("Transform",
        [](const json& j, World& w, Entity e) {
            TransformComponent t;
            if (j.contains("position")) {
                auto& p = j["position"];
                t.position = {p[0], p[1], p[2]};
            }
            // ... full implementation
            w.add_component(e, t);
            return Ok();
        });

    // MeshComponent, MaterialComponent, LightComponent, CameraComponent, RenderableTag
    // Each with complete JSON→Component factory
}
```

**Acceptance Criteria**:
- [ ] TransformComponent registered with full factory
- [ ] MeshComponent registered (builtin + asset handle)
- [ ] MaterialComponent registered (all PBR properties)
- [ ] LightComponent registered (all light types)
- [ ] CameraComponent registered (perspective + ortho)
- [ ] RenderableTag registered
- [ ] HierarchyComponent registered
- [ ] All factories handle missing fields with defaults
- [ ] All factories handle malformed JSON gracefully

#### Task 2.2: Register Engine Render Systems
**File**: `src/runtime/runtime.cpp` (modify init_render or init_simulation)

**Implementation**:
```cpp
void Runtime::register_engine_core_systems() {
    auto* kernel = m_kernel.get();
    auto* world = m_packages->ecs_world.get();

    // Transform must run first to update world matrices
    kernel->register_system(Stage::Update, "engine.TransformSystem",
        [world](float dt) { void_render::TransformSystem::run(*world, dt); },
        -100);  // High priority (runs early)

    kernel->register_system(Stage::RenderPrepare, "engine.CameraSystem",
        [world](float dt) { void_render::CameraSystem::run(*world, dt); },
        0);

    kernel->register_system(Stage::RenderPrepare, "engine.LightSystem",
        [world](float dt) { void_render::LightSystem::run(*world, dt); },
        10);

    kernel->register_system(Stage::RenderPrepare, "engine.RenderPrepareSystem",
        [world](float dt) { void_render::RenderPrepareSystem::run(*world, dt); },
        100);

    kernel->register_system(Stage::Render, "engine.RenderSystem",
        [world](float dt) { void_render::RenderSystem::run(*world, dt); },
        0);
}
```

**Acceptance Criteria**:
- [ ] TransformSystem runs in Update stage (priority -100)
- [ ] CameraSystem runs in RenderPrepare stage
- [ ] LightSystem runs in RenderPrepare stage
- [ ] RenderPrepareSystem runs in RenderPrepare stage (after camera/light)
- [ ] RenderSystem runs in Render stage
- [ ] All systems use the package ECS world
- [ ] Systems are named with "engine." prefix

#### Task 2.3: Add RenderContext as ECS Resource
**File**: `src/runtime/runtime.cpp` (modify init_render)

**Implementation**:
```cpp
Result<void> Runtime::init_render() {
    // ... existing window/platform setup ...

    // Create RenderContext and add to ECS
    void_render::RenderContext render_ctx;
    auto init_result = render_ctx.initialize(width, height);
    if (!init_result) {
        return init_result;
    }

    // Move into ECS as resource (render systems will query this)
    m_packages->ecs_world->insert_resource(std::move(render_ctx));

    // Register render systems AFTER resource is available
    register_engine_core_systems();

    return Ok();
}
```

**Acceptance Criteria**:
- [ ] RenderContext created with correct dimensions
- [ ] RenderContext moved into ECS world as resource
- [ ] Render systems registered AFTER resource available
- [ ] Resize handling updates the resource

---

### Phase 3: Plugin Loading Infrastructure
**Goal**: Load plugin DLLs and call their entry points

#### Task 3.1: Update PluginPackageLoader to Load DLLs
**File**: `src/package/plugin_package_loader.cpp`

**Implementation**:
```cpp
Result<void> PluginPackageLoader::load(const ResolvedPackage& pkg, LoadContext& ctx) {
    auto manifest = PluginPackageManifest::load(pkg.manifest.source_path);
    if (!manifest) return manifest.error();

    // If manifest specifies a library, load it
    if (!manifest->library.empty()) {
        auto lib_path = resolve_library_path(*manifest, pkg.manifest.source_path);

        auto dll = DynamicLibrary::load(lib_path);
        if (!dll) {
            return Error("Failed to load plugin library: " + lib_path.string());
        }

        // Get entry point
        auto create_fn = dll->get_symbol<PluginCreateFn>("plugin_create");
        if (!create_fn) {
            return Error("Plugin missing plugin_create entry point");
        }

        // Create plugin instance
        IPlugin* plugin = create_fn();
        if (!plugin) {
            return Error("plugin_create returned null");
        }

        // Create context and call on_load
        PluginContext plugin_ctx = create_plugin_context(ctx);
        auto load_result = plugin->on_load(plugin_ctx);
        if (!load_result) {
            plugin_destroy(plugin);
            return load_result;
        }

        // Track loaded plugin
        m_loaded_plugins[manifest->base.name] = LoadedPlugin{
            .plugin = plugin,
            .library = std::move(*dll),
            .manifest = std::move(*manifest),
            .context = std::move(plugin_ctx)
        };
    } else {
        // JSON-only plugin (uses engine components, no custom code)
        // Register component schemas from manifest
        register_manifest_schemas(*manifest, ctx);
    }

    return Ok();
}
```

**Acceptance Criteria**:
- [ ] DynamicLibrary loaded from manifest library path
- [ ] Platform-specific path resolution (dll/so/dylib)
- [ ] plugin_create symbol resolved
- [ ] IPlugin instance created
- [ ] PluginContext created and passed to on_load
- [ ] Error handling for missing library, missing symbol, failed load
- [ ] Plugin tracked in m_loaded_plugins
- [ ] JSON-only plugins still work (backward compatible)

#### Task 3.2: Implement Plugin Unloading
**File**: `src/package/plugin_package_loader.cpp`

**Implementation**:
```cpp
Result<void> PluginPackageLoader::unload(const std::string& name, LoadContext& ctx) {
    auto it = m_loaded_plugins.find(name);
    if (it == m_loaded_plugins.end()) {
        return Error("Plugin not loaded: " + name);
    }

    auto& loaded = it->second;

    // Call on_unload
    auto unload_result = loaded.plugin->on_unload(loaded.context);
    if (!unload_result) {
        spdlog::warn("Plugin {} on_unload failed: {}", name, unload_result.error().message());
        // Continue with cleanup anyway
    }

    // Unregister systems from kernel
    for (const auto& sys_id : loaded.context.registered_systems()) {
        ctx.kernel()->unregister_system(sys_id);
    }

    // Unregister event subscriptions
    for (const auto& sub_id : loaded.context.registered_subscriptions()) {
        ctx.event_bus()->unsubscribe(sub_id);
    }

    // Destroy plugin
    auto destroy_fn = loaded.library.get_symbol<PluginDestroyFn>("plugin_destroy");
    if (destroy_fn) {
        destroy_fn(loaded.plugin);
    }

    // DynamicLibrary destructor unloads the DLL
    m_loaded_plugins.erase(it);

    return Ok();
}
```

**Acceptance Criteria**:
- [ ] on_unload called before destruction
- [ ] Systems unregistered from kernel
- [ ] Event subscriptions removed
- [ ] plugin_destroy called
- [ ] DLL unloaded
- [ ] Plugin removed from tracking

#### Task 3.3: Implement Plugin Hot-Reload
**File**: `src/package/plugin_package_loader.cpp`

**Implementation**:
```cpp
Result<void> PluginPackageLoader::reload(const ResolvedPackage& pkg, LoadContext& ctx) {
    auto& name = pkg.manifest.name;
    auto it = m_loaded_plugins.find(name);
    if (it == m_loaded_plugins.end()) {
        return Error("Plugin not loaded: " + name);
    }

    auto& loaded = it->second;

    // 1. Snapshot
    PluginSnapshot snapshot = loaded.plugin->snapshot();

    // 2. Unload (but keep snapshot)
    auto unload_result = loaded.plugin->on_unload(loaded.context);

    // ... unregister systems, subscriptions ...

    auto destroy_fn = loaded.library.get_symbol<PluginDestroyFn>("plugin_destroy");
    if (destroy_fn) destroy_fn(loaded.plugin);
    loaded.library.unload();

    // 3. Load new DLL
    auto lib_path = resolve_library_path(loaded.manifest, pkg.manifest.source_path);
    auto dll = DynamicLibrary::load(lib_path);
    if (!dll) {
        return Error("Failed to reload plugin library");
    }

    auto create_fn = dll->get_symbol<PluginCreateFn>("plugin_create");
    IPlugin* new_plugin = create_fn();

    // 4. Call on_load
    PluginContext new_ctx = create_plugin_context(ctx);
    auto load_result = new_plugin->on_load(new_ctx);
    if (!load_result) {
        // Rollback: try to reload old version
        return load_result;
    }

    // 5. Restore state
    auto restore_result = new_plugin->restore(snapshot);
    if (!restore_result) {
        spdlog::warn("Plugin {} restore failed: {}", name, restore_result.error().message());
    }

    // 6. Notify
    new_plugin->on_reloaded();

    // Update tracking
    loaded.plugin = new_plugin;
    loaded.library = std::move(*dll);
    loaded.context = std::move(new_ctx);

    return Ok();
}
```

**Acceptance Criteria**:
- [ ] Snapshot captured before unload
- [ ] Old plugin fully cleaned up
- [ ] New DLL loaded
- [ ] New plugin initialized via on_load
- [ ] State restored via restore()
- [ ] on_reloaded() called
- [ ] Rollback on failure

---

### Phase 4: Plugin State in ECS
**Goal**: Track plugin state as ECS resources

#### Task 4.1: Create PluginState Resource
**File**: `include/void_engine/plugin_api/state.hpp`

```cpp
struct PluginState {
    std::string id;
    Version version;
    PluginStatus status;

    std::vector<ComponentId> registered_components;
    std::vector<SystemId> registered_systems;
    std::vector<SubscriptionId> subscriptions;
    std::vector<Entity> owned_entities;

    std::chrono::steady_clock::time_point loaded_at;
    std::chrono::steady_clock::time_point last_reloaded_at;
};

struct PluginRegistry {
    std::unordered_map<std::string, PluginState> plugins;
    std::vector<std::string> load_order;

    const PluginState* get(const std::string& id) const;
    bool is_loaded(const std::string& id) const;
    std::vector<std::string> dependents_of(const std::string& id) const;
};
```

**Acceptance Criteria**:
- [ ] PluginState captures all plugin metadata
- [ ] PluginRegistry tracks all plugins
- [ ] Load order maintained for dependency handling
- [ ] Query methods for introspection

#### Task 4.2: Initialize PluginRegistry Resource
**File**: `src/runtime/runtime.cpp`

```cpp
Result<void> Runtime::init_packages() {
    // ... existing setup ...

    // Add PluginRegistry as ECS resource
    m_packages->ecs_world->insert_resource(PluginRegistry{});

    // ... rest of init ...
}
```

**Acceptance Criteria**:
- [ ] PluginRegistry inserted early in init
- [ ] Available before any plugins load

#### Task 4.3: Update Plugin Loading to Track State
**File**: `src/package/plugin_package_loader.cpp`

```cpp
// In load():
auto& registry = ctx.ecs_world()->resource_mut<PluginRegistry>();
registry.plugins[name] = PluginState{
    .id = name,
    .version = plugin->version(),
    .status = PluginStatus::Active,
    .loaded_at = std::chrono::steady_clock::now()
};
registry.load_order.push_back(name);

// In unload():
registry.plugins.erase(name);
std::erase(registry.load_order, name);
```

**Acceptance Criteria**:
- [ ] State added on load
- [ ] State removed on unload
- [ ] Load order maintained
- [ ] Status transitions tracked

---

### Phase 5: Base Plugin Library
**Goal**: Create engine-provided base plugins

#### Task 5.1: Create base.health Plugin
**Directory**: `plugins/base.health/`

**Files**:
- `manifest.plugin.json` - Plugin manifest
- `src/health_plugin.cpp` - Implementation
- `CMakeLists.txt` - Build config

**Components**:
- Health { current, max, regeneration, invulnerable }
- DamageReceiver { armor, damage_multiplier }
- Dead {} (tag)

**Systems**:
- HealthRegenSystem (Stage::Update)
- DeathSystem (Stage::PostFixed)

**Events**:
- DamageEvent { entity, amount, source }
- DeathEvent { entity, killer }
- HealEvent { entity, amount }

**Acceptance Criteria**:
- [ ] Complete plugin implementation (not a stub)
- [ ] All components with JSON factories
- [ ] All systems fully implemented
- [ ] Event handlers working
- [ ] Hot-reload support (snapshot/restore)
- [ ] Can be used by example

#### Task 5.2: Create CMake Build System for Plugins
**File**: `plugins/CMakeLists.txt`

```cmake
# Function to create a plugin target
function(add_void_plugin NAME)
    add_library(${NAME} SHARED ${ARGN})
    target_link_libraries(${NAME} PRIVATE void_plugin_api)
    set_target_properties(${NAME} PROPERTIES
        PREFIX ""  # No "lib" prefix on Linux
        OUTPUT_NAME ${NAME}
    )
    # Copy to plugins output directory
    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${NAME}>
                ${CMAKE_BINARY_DIR}/plugins/${NAME}$<TARGET_FILE_SUFFIX:${NAME}>
    )
endfunction()

add_subdirectory(base.health)
```

**Acceptance Criteria**:
- [ ] Plugin builds as shared library
- [ ] Correct naming (no lib prefix)
- [ ] Copied to output directory
- [ ] Links against plugin API

---

### Phase 6: Example Integration
**Goal**: Update example to use the plugin system correctly

#### Task 6.1: Update Example Prefabs
**File**: `examples/package_demo/packages/demo.characters/manifest.bundle.json`

Prefabs must use engine render components:
```json
{
  "prefabs": {
    "enemy_prefab": {
      "components": {
        "Transform": { "position": [0, 1, 0] },
        "Mesh": { "builtin": "sphere" },
        "Material": {
          "albedo": [0.8, 0.2, 0.2, 1.0],
          "metallic": 0.0,
          "roughness": 0.5
        },
        "Renderable": { "visible": true },
        "Health": { "current": 100, "max": 100 },
        "Enemy": {}
      }
    }
  }
}
```

**Acceptance Criteria**:
- [ ] All prefabs include Transform, Mesh, Material, Renderable
- [ ] Engine component names used correctly
- [ ] Game components from plugins (Health, Enemy)

#### Task 6.2: Add Camera and Lights to World
**File**: `examples/package_demo/packages/world.demo_arena/manifest.world.json`

```json
{
  "name": "world.demo_arena",
  "scene": {
    "camera": {
      "position": [0, 5, -10],
      "target": [0, 0, 0],
      "fov": 60
    },
    "lights": [
      {
        "type": "directional",
        "direction": [0.5, -1, 0.5],
        "color": [1, 1, 1],
        "intensity": 1.0
      }
    ]
  }
}
```

**Acceptance Criteria**:
- [ ] World defines camera
- [ ] World defines at least one light
- [ ] WorldComposer spawns camera entity
- [ ] WorldComposer spawns light entities

#### Task 6.3: Update WorldComposer to Spawn Camera/Lights
**File**: `src/package/world_composer.cpp`

**Acceptance Criteria**:
- [ ] Parse camera from world manifest
- [ ] Spawn camera entity with CameraComponent + TransformComponent
- [ ] Parse lights from world manifest
- [ ] Spawn light entities with LightComponent + TransformComponent
- [ ] Set camera as active

---

## Verification Checklist

After implementation, verify:

### Plugin API
- [ ] IPlugin interface compiles
- [ ] PluginContext provides all documented APIs
- [ ] make_renderable creates correct engine components
- [ ] Plugins can register components with factories
- [ ] Plugins can register systems with kernel
- [ ] Plugins can subscribe to events

### Engine Integration
- [ ] Engine render components in schema registry
- [ ] Engine render systems run in correct stages
- [ ] RenderContext available as ECS resource
- [ ] Prefabs can instantiate entities with render components

### DLL Loading
- [ ] Plugin DLLs load on Windows
- [ ] Plugin SOs load on Linux
- [ ] plugin_create/plugin_destroy called
- [ ] on_load/on_unload lifecycle works
- [ ] Hot-reload preserves state

### Example Works
- [ ] package_demo.exe shows rendered entities
- [ ] Enemies visible as red spheres
- [ ] Camera positioned correctly
- [ ] Lighting affects scene
- [ ] Frame loop runs smoothly

### Hot-Reload
- [ ] Modify plugin DLL → reloads
- [ ] Plugin state preserved
- [ ] Systems re-registered
- [ ] No crashes or memory leaks

---

## File Reference

### New Files to Create
```
include/void_engine/plugin_api/
├── plugin.hpp           # IPlugin interface
├── context.hpp          # PluginContext
├── renderable.hpp       # RenderableDesc
├── state.hpp            # PluginState, PluginRegistry
├── types.hpp            # Common types (Dependency, etc.)
└── export.hpp           # PLUGIN_API macros

plugins/
├── CMakeLists.txt       # Plugin build system
└── base.health/
    ├── manifest.plugin.json
    ├── CMakeLists.txt
    └── src/
        └── health_plugin.cpp
```

### Files to Modify
```
src/runtime/runtime.cpp
  - register_engine_core_components()
  - register_engine_core_systems()
  - Add RenderContext as ECS resource

src/package/plugin_package_loader.cpp
  - Load DLLs from manifest
  - Create PluginContext
  - Call plugin lifecycle methods
  - Implement hot-reload

src/package/world_composer.cpp
  - Spawn camera from world manifest
  - Spawn lights from world manifest

examples/package_demo/packages/
  - Update prefabs with render components
  - Add camera/lights to world manifest
```

---

## Success Criteria

The implementation is complete when:

1. **Example renders**: package_demo.exe shows a window with visible entities
2. **Plugins work**: base.health plugin loads and provides Health component
3. **Hot-reload works**: Changing plugin code reloads without restart
4. **No stubs**: Every declared function has full implementation
5. **No hardcoding**: All game logic in plugins, not engine
6. **Contract enforced**: Plugins use PluginContext, not direct access
7. **State managed**: All plugin state in ECS resources
8. **Production ready**: Error handling, cleanup, logging complete

---

## Notes for Future Sessions

If conversation compacts, read these documents in order:

1. `CLAUDE.md` - Core rules (never delete code, no stubs, etc.)
2. `doc/PLUGIN_ENGINE_CONTRACT.md` - The architectural contract
3. `doc/IMPLEMENTATION_PLAN_PLUGIN_SYSTEM.md` - This file (implementation details)
4. `doc/VOID_ENGINE_ARCHITECTURE_DEEP_DIVE.md` - Module reference
5. `doc/PACKAGE_DRIVEN_ARCHITECTURE_VISION.md` - Why this architecture

The core principle: **Plugins define everything, engine renders it.**
