# Package-Driven Architecture Vision

## The Core Principle

**Everything is loadable via packages** - not just entity data, but components, systems, and behavior.

JSON manifests describe **what** exists. Native code (DLLs/SOs) provides **how** it works.

---

## Table of Contents

1. [Architectural Vision](#1-architectural-vision)
2. [The Two-Layer Model](#2-the-two-layer-model)
3. [Component Loading Architecture](#3-component-loading-architecture)
4. [System Loading Architecture](#4-system-loading-architecture)
5. [Current State Analysis](#5-current-state-analysis)
6. [Gap Analysis](#6-gap-analysis)
7. [Integration Requirements](#7-integration-requirements)
8. [Example Package Structure](#8-example-package-structure)

---

## 1. Architectural Vision

### The Promise

External creators can define **complete game functionality** through packages:

```
Package = Manifest (JSON) + Native Code (DLL/SO) + Assets
```

| What | Defined By | Loaded Via |
|------|-----------|------------|
| Component Schema | JSON manifest | ComponentSchemaRegistry |
| Component Factory | Native code (DLL) | Plugin loader |
| System Logic | Native code (DLL) | Plugin loader → Kernel |
| Entity Templates | JSON (prefabs) | PrefabRegistry |
| Assets | Binary files | AssetBundleLoader |

### The Reality Check

JSON can describe **data structures** but cannot describe **behavior**.

A component like `Health { current: 100, max: 100 }` is just data.
The **system** that decrements health when damage is received is **code**.

Therefore:

```
┌─────────────────────────────────────────────────────────────┐
│  PLUGIN PACKAGE = JSON Manifest + Native Library (DLL/SO)  │
│                                                             │
│  manifest.plugin.json:                                      │
│    - Component schemas (what fields exist)                  │
│    - System declarations (what systems to register)         │
│    - Entry point symbol name                                │
│                                                             │
│  plugin.dll / plugin.so:                                    │
│    - Component factories (JSON → Component)                 │
│    - System implementations (actual game logic)             │
│    - Event handlers                                         │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. The Two-Layer Model

### Layer 1: Engine Foundation

The engine provides **core capabilities** that all games need:

| Category | Engine Provides | Always Available |
|----------|----------------|------------------|
| **Core Components** | Transform, Hierarchy, Name, Tag | Yes |
| **Render Components** | Mesh, Material, Light, Camera, Renderable | Yes |
| **Core Systems** | TransformSystem, RenderPrepareSystem, RenderSystem | Yes |
| **Infrastructure** | ECS World, Kernel, EventBus, AssetServer | Yes |

These are **hardcoded in the engine** because:
- They're needed by virtually every game
- They require deep integration with GPU/platform
- They define the contract that plugins build upon

### Layer 2: Plugin Extensions

Plugins provide **game-specific functionality**:

| Category | Plugin Provides | Loaded Dynamically |
|----------|----------------|-------------------|
| **Game Components** | Health, Weapon, Inventory, AI | Via DLL |
| **Game Systems** | CombatSystem, AISystem, InventorySystem | Via DLL |
| **Custom Render** | CustomShaderComponent, ParticleEmitter | Via DLL |
| **Behavior** | Event handlers, game rules | Via DLL |

### The Contract

```
┌─────────────────────────────────────────────────────────────┐
│                     ENGINE (Foundation)                      │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Core Components: Transform, Mesh, Material, Light...   │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Core Systems: TransformSystem, RenderSystem...         │ │
│  └────────────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Plugin API: register_component(), register_system()    │ │
│  └────────────────────────────────────────────────────────┘ │
│                              │                               │
└──────────────────────────────┼───────────────────────────────┘
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
         ▼                     ▼                     ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ core.gameplay   │  │ gameplay.combat │  │ mod.custom      │
│ (Plugin DLL)    │  │ (Plugin DLL)    │  │ (Plugin DLL)    │
│                 │  │                 │  │                 │
│ - Health        │  │ - Weapon        │  │ - CustomComp    │
│ - Player        │  │ - DamageReceiver│  │ - CustomSystem  │
│ - Enemy         │  │ - CombatSystem  │  │                 │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

---

## 3. Component Loading Architecture

### How Components Should Work

**Step 1: Engine registers core components**
```cpp
// In engine initialization (always available)
schema_registry->register_builtin<TransformComponent>("Transform");
schema_registry->register_builtin<MeshComponent>("Mesh");
schema_registry->register_builtin<MaterialComponent>("Material");
schema_registry->register_builtin<LightComponent>("Light");
schema_registry->register_builtin<CameraComponent>("Camera");
schema_registry->register_builtin<RenderableTag>("Renderable");
```

**Step 2: Plugin DLL exports component registration**
```cpp
// In plugin DLL (core.gameplay.dll)
extern "C" PLUGIN_API void register_components(ComponentSchemaRegistry* registry) {
    registry->register_component<Health>("Health", {
        {"current", FieldType::Float},
        {"max", FieldType::Float}
    });
    registry->register_component<Player>("Player", {});
    registry->register_component<Enemy>("Enemy", {});
}
```

**Step 3: Plugin loader calls registration**
```cpp
// In PluginPackageLoader::load()
auto register_fn = dll.get_symbol<RegisterComponentsFn>("register_components");
register_fn(schema_registry);
```

### The Component Factory Pattern

JSON defines data, but someone must construct the actual C++ component:

```cpp
// Component factory registered by plugin
registry->register_factory("Health", [](const json& data, World& world, Entity e) {
    Health h;
    h.current = data.value("current", 100.0f);
    h.max = data.value("max", 100.0f);
    world.add_component(e, h);
    return Ok();
});
```

### Current Implementation

Looking at `PluginPackageLoader`:

```cpp
// Current: Reads component schemas from JSON manifest
for (const auto& comp : manifest.components) {
    auto schema = ComponentSchema::from_declaration(comp);
    m_schema_registry->register_schema(comp.name, schema);
}
```

**Problem**: This only registers the **schema** (field names/types).
It does NOT register a **factory** that can construct real components.

The schema registry can describe that "Health has current:float and max:float"
but cannot actually CREATE a Health component because Health is a C++ struct
defined in the plugin DLL, not in the engine.

---

## 4. System Loading Architecture

### How Systems Should Work

**Step 1: Engine registers core systems**
```cpp
// In engine initialization (always runs)
kernel->register_system(Stage::Update, "TransformSystem", TransformSystem::run, -100);
kernel->register_system(Stage::RenderPrepare, "CameraSystem", CameraSystem::run, 0);
kernel->register_system(Stage::RenderPrepare, "LightSystem", LightSystem::run, 0);
kernel->register_system(Stage::RenderPrepare, "RenderPrepareSystem", RenderPrepareSystem::run, 100);
kernel->register_system(Stage::Render, "RenderSystem", RenderSystem::run, 0);
```

**Step 2: Plugin DLL exports system registration**
```cpp
// In plugin DLL (gameplay.combat.dll)
extern "C" PLUGIN_API void register_systems(Kernel* kernel, World* world) {
    kernel->register_system(Stage::Update, "CombatSystem",
        [world](float dt) { CombatSystem::run(*world, dt); },
        50);
    kernel->register_system(Stage::Update, "HealthRegenSystem",
        [world](float dt) { HealthRegenSystem::run(*world, dt); },
        60);
}
```

**Step 3: Plugin loader calls registration**
```cpp
// In PluginPackageLoader::load()
auto register_fn = dll.get_symbol<RegisterSystemsFn>("register_systems");
register_fn(kernel, ecs_world);
```

### Current Implementation

Looking at `PluginPackageManifest`:

```cpp
struct SystemDeclaration {
    std::string name;
    std::string stage;       // "Update", "FixedUpdate", etc.
    int priority = 0;
    std::string entry_point; // Symbol name in DLL
    // ...
};
```

The manifest describes systems, but...

**Question**: Is the DLL actually being loaded and symbols resolved?

Looking at `PluginPackageLoader::load()`:
- It reads the manifest
- It registers component schemas
- But it does NOT load a DLL
- The "systems" are just metadata, not actual code

---

## 5. Current State Analysis

### What EXISTS in the Engine

| Component | Location | Available To Packages? |
|-----------|----------|----------------------|
| `TransformComponent` | void_render/components.hpp | NO - not in schema registry |
| `MeshComponent` | void_render/components.hpp | NO - not in schema registry |
| `MaterialComponent` | void_render/components.hpp | NO - not in schema registry |
| `LightComponent` | void_render/components.hpp | NO - not in schema registry |
| `CameraComponent` | void_render/components.hpp | NO - not in schema registry |
| `RenderableTag` | void_render/components.hpp | NO - not in schema registry |

| System | Location | Registered with Kernel? |
|--------|----------|------------------------|
| `TransformSystem` | void_render/render_systems.hpp | NO |
| `CameraSystem` | void_render/render_systems.hpp | NO |
| `LightSystem` | void_render/render_systems.hpp | NO |
| `RenderPrepareSystem` | void_render/render_systems.hpp | NO |
| `RenderSystem` | void_render/render_systems.hpp | NO |

### What EXISTS in the Example

| Component | Defined In | Has Native Code? |
|-----------|-----------|-----------------|
| `Transform` (gameplay) | core.gameplay manifest | NO - JSON schema only |
| `Health` | core.gameplay manifest | NO - JSON schema only |
| `Player` | core.gameplay manifest | NO - JSON schema only |
| `Enemy` | core.gameplay manifest | NO - JSON schema only |
| `Weapon` | gameplay.combat manifest | NO - JSON schema only |
| `DamageReceiver` | gameplay.combat manifest | NO - JSON schema only |

| System | Defined In | Has Native Code? |
|--------|-----------|-----------------|
| (none) | - | - |

### What's Actually Happening

1. **PluginPackageLoader** reads JSON manifests
2. **ComponentSchemaRegistry** stores field definitions
3. **PrefabRegistry** can instantiate entities with components
4. But components are **generic blobs** - the engine doesn't know what they mean
5. No systems process these components because no DLLs are loaded
6. No rendering because render components/systems aren't connected

---

## 6. Gap Analysis

### Gap 1: Engine Core Components Not Exposed

**Status**: Engine has production-ready render components but they're not available to packages.

**What's Missing**:
- `void_render` components not registered in `ComponentSchemaRegistry`
- No way for prefabs to include `Mesh`, `Material`, `Renderable`
- Engine-defined components and plugin-defined components live in separate worlds

**Required**:
```cpp
// During Runtime::init_packages() or init_render()
register_engine_core_components(schema_registry);
```

### Gap 2: Engine Core Systems Not Registered

**Status**: Engine has production-ready render systems but they don't run.

**What's Missing**:
- `TransformSystem`, `RenderPrepareSystem`, `RenderSystem` not registered with kernel
- No connection between kernel stages and void_render systems
- Frame loop runs but render pipeline is disconnected

**Required**:
```cpp
// During Runtime::init_render()
register_engine_core_systems(kernel, ecs_world);
```

### Gap 3: RenderContext Not Available

**Status**: RenderContext exists but isn't an ECS resource.

**What's Missing**:
- Render systems query `world.resource<RenderContext>()` but it's not there
- Legacy `SceneRenderer` holds GPU state separately
- No bridge between package ECS world and GPU resources

**Required**:
```cpp
// During Runtime::init_render()
ecs_world->insert_resource(RenderContext{...});
```

### Gap 4: No Plugin DLL Loading

**Status**: Plugin packages are JSON-only, no native code loaded.

**What's Missing**:
- `PluginPackageLoader` doesn't load DLLs
- Component factories don't exist (only schemas)
- System code isn't executed (only declared)
- The `DynamicLibrary` class exists but isn't used

**Required**:
```cpp
// In PluginPackageLoader::load()
if (!manifest.library_path.empty()) {
    auto dll = DynamicLibrary::load(manifest.library_path);
    auto init = dll.get_symbol<PluginInitFn>("plugin_init");
    init(schema_registry, kernel, ecs_world);
}
```

### Gap 5: Example Has No Native Code

**Status**: Example defines component schemas in JSON but provides no behavior.

**What's Missing**:
- No `.dll` or `.so` files in example packages
- No `library_path` in plugin manifests
- No `plugin_init` entry points
- Systems declared but code doesn't exist

**Required**:
- Example needs to either:
  - A) Provide actual plugin DLLs with component/system implementations
  - B) Use only engine-provided core components/systems

### Gap 6: No Camera or Lights

**Status**: World loads but has no viewpoint.

**What's Missing**:
- World manifest doesn't spawn camera entity
- World manifest doesn't spawn light entities
- No default camera/light creation
- Render systems have nothing to render to/with

**Required**:
- World manifest should define camera/lights, OR
- WorldComposer should spawn defaults, OR
- Engine should have fallback rendering

---

## 7. Integration Requirements

### Option A: Engine-First (Recommended for MVP)

The engine provides all core components and systems. Plugins extend with game-specific logic.

```
┌─────────────────────────────────────────────────────────────┐
│  ENGINE (Always Available)                                  │
│                                                             │
│  Core Components:                                           │
│    Transform, Mesh, Material, Light, Camera, Renderable,    │
│    Hierarchy, Name, Tag                                     │
│                                                             │
│  Core Systems:                                              │
│    TransformSystem, CameraSystem, LightSystem,              │
│    RenderPrepareSystem, RenderSystem, AnimationSystem       │
│                                                             │
│  RenderContext as ECS Resource                              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  PLUGIN (JSON-only, uses engine components)                 │
│                                                             │
│  manifest.plugin.json:                                      │
│    components: []  // No custom components                  │
│    systems: []     // No custom systems                     │
│                                                             │
│  Uses prefabs with engine components:                       │
│    { "Transform": {...}, "Mesh": {...}, "Material": {...} } │
└─────────────────────────────────────────────────────────────┘
```

**Implementation Steps**:
1. Engine registers core components in schema registry at startup
2. Engine registers core systems with kernel at startup
3. Engine creates RenderContext as ECS resource
4. Example prefabs use engine component names
5. No DLLs required for basic rendering

### Option B: Plugin-First (Full Architecture)

Plugins provide all game-specific components and systems via native code.

```
┌─────────────────────────────────────────────────────────────┐
│  ENGINE (Minimal)                                           │
│                                                             │
│  Only provides:                                             │
│    - ECS World infrastructure                               │
│    - Kernel stage scheduler                                 │
│    - Plugin loading (DLL/SO)                                │
│    - Asset loading                                          │
│    - GPU abstraction                                        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  CORE.RENDER PLUGIN (Engine-provided DLL)                   │
│                                                             │
│  manifest.plugin.json:                                      │
│    library: "core.render.dll"                               │
│    components: [Transform, Mesh, Material, Light, Camera]   │
│    systems: [TransformSystem, RenderPrepareSystem, ...]     │
│                                                             │
│  core.render.dll:                                           │
│    plugin_init() registers everything                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  GAMEPLAY PLUGIN (Game-specific DLL)                        │
│                                                             │
│  manifest.plugin.json:                                      │
│    library: "gameplay.dll"                                  │
│    components: [Health, Weapon, Enemy]                      │
│    systems: [CombatSystem, AISystem]                        │
│                                                             │
│  gameplay.dll:                                              │
│    plugin_init() registers game components/systems          │
└─────────────────────────────────────────────────────────────┘
```

**Implementation Steps**:
1. Implement DLL loading in PluginPackageLoader
2. Define plugin API (void_plugin_api module)
3. Package engine render code as core.render plugin
4. Example creates gameplay.dll with custom components
5. World manifest depends on both plugins

### Hybrid Approach (Practical)

Engine provides core rendering. Plugins can extend OR override.

```
┌─────────────────────────────────────────────────────────────┐
│  ENGINE CORE (Always Available, Can't Override)             │
│    - Transform, Mesh, Material, Light, Camera, Renderable   │
│    - TransformSystem, RenderPrepareSystem, RenderSystem     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  PLUGIN (Extends Engine)                                    │
│                                                             │
│  JSON-only plugin (no DLL):                                 │
│    - Defines prefabs using engine components                │
│    - No custom components or systems                        │
│                                                             │
│  Native plugin (with DLL):                                  │
│    - Adds NEW components (Health, Weapon)                   │
│    - Adds NEW systems (CombatSystem)                        │
│    - Cannot override engine components/systems              │
└─────────────────────────────────────────────────────────────┘
```

---

## 8. Example Package Structure

### Current Structure (Broken)

```
examples/package_demo/packages/
├── core.gameplay/
│   └── manifest.plugin.json      # Component schemas (JSON only)
├── gameplay.combat/
│   └── manifest.plugin.json      # Component schemas (JSON only)
├── demo.characters/
│   └── manifest.bundle.json      # Prefabs using above components
├── demo.environment/
│   └── manifest.bundle.json      # Prefabs using above components
├── layer.night_mode/
│   └── manifest.layer.json
├── layer.hard_mode/
│   └── manifest.layer.json
├── widget.game_hud/
│   └── manifest.widget.json
└── world.demo_arena/
    └── manifest.world.json
```

**Problem**: No DLLs, no render components, no systems to run.

### Required Structure (Option A - Engine Core)

```
examples/package_demo/packages/
├── demo.characters/
│   └── manifest.bundle.json      # Prefabs using ENGINE components
│       {
│         "prefabs": {
│           "enemy_prefab": {
│             "components": {
│               "Transform": { "position": [0,0,0] },
│               "Mesh": { "builtin": "sphere" },        // ENGINE component
│               "Material": { "albedo": [1,0,0,1] },    // ENGINE component
│               "Renderable": { "visible": true }       // ENGINE component
│             }
│           }
│         }
│       }
├── demo.environment/
│   └── manifest.bundle.json
├── layer.night_mode/
│   └── manifest.layer.json
└── world.demo_arena/
    └── manifest.world.json
        {
          "camera": {                                   // Spawns camera
            "position": [0, 5, -10],
            "fov": 60
          },
          "lights": [{                                  // Spawns lights
            "type": "directional",
            "direction": [0, -1, 0.5],
            "intensity": 1.0
          }]
        }
```

### Required Structure (Option B - Full Plugin)

```
examples/package_demo/packages/
├── core.render/                                        # Engine-provided
│   ├── manifest.plugin.json
│   └── bin/
│       ├── core.render.dll (Windows)
│       └── libcore.render.so (Linux)
├── core.gameplay/
│   ├── manifest.plugin.json
│   │   {
│   │     "library": "bin/core.gameplay.dll",
│   │     "components": ["Health", "Player", "Enemy"],
│   │     "systems": [
│   │       { "name": "HealthSystem", "stage": "Update", "entry": "health_system_run" }
│   │     ]
│   │   }
│   └── bin/
│       ├── core.gameplay.dll
│       └── libcore.gameplay.so
├── gameplay.combat/
│   ├── manifest.plugin.json
│   └── bin/
│       └── gameplay.combat.dll
├── demo.characters/
│   └── manifest.bundle.json
└── world.demo_arena/
    └── manifest.world.json
```

---

## Summary

### What Works
- Package discovery and scanning
- Dependency resolution
- JSON manifest parsing
- Component schema registration (metadata only)
- Prefab instantiation (creates entities)
- World loading orchestration

### What's Disconnected

| System | Has Code | Registered | Connected |
|--------|----------|------------|-----------|
| Engine Render Components | ✅ Yes | ❌ No | ❌ No |
| Engine Render Systems | ✅ Yes | ❌ No | ❌ No |
| RenderContext Resource | ✅ Yes | ❌ No | ❌ No |
| Plugin DLL Loading | ✅ Yes (DynamicLibrary) | ❌ No | ❌ No |
| Plugin Component Factories | ❌ No | ❌ No | ❌ No |
| Plugin System Registration | ❌ No | ❌ No | ❌ No |
| Default Camera/Lights | ❌ No | ❌ No | ❌ No |

### The Path Forward

**Immediate (Make it render)**:
1. Register engine render components in schema registry
2. Register engine render systems with kernel
3. Add RenderContext as ECS resource
4. Update example prefabs to use engine component names
5. Add camera and lights to world manifest

**Full Architecture (Complete plugin system)**:
1. Implement DLL loading in PluginPackageLoader
2. Define plugin API entry points
3. Create plugin build system (CMake targets)
4. Example provides gameplay.dll with custom components/systems
5. Hot-reload support for plugin DLLs

The engine has all the pieces. They just need to be wired together.
