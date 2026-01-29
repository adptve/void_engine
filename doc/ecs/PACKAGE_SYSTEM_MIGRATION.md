# Package System Migration Guide

> **Document Version**: 1.1
> **Last Updated**: 2026-01-29
> **Scope**: Migration plan from current ECS architecture to the new Package System

---

## Table of Contents

### Part I: Architecture & Migration
1. [Executive Summary](#1-executive-summary)
2. [Current State Analysis](#2-current-state-analysis)
3. [Target State Overview](#3-target-state-overview)
4. [Architecture Mapping](#4-architecture-mapping)
5. [Migration Phases](#5-migration-phases)
6. [Phase 1: Package Infrastructure](#6-phase-1-package-infrastructure)
7. [Phase 2: Asset Bundle Migration](#7-phase-2-asset-bundle-migration)
8. [Phase 3: Plugin System Migration](#8-phase-3-plugin-system-migration)
9. [Phase 4: Widget System Migration](#9-phase-4-widget-system-migration)
10. [Phase 5: Layer System Migration](#10-phase-5-layer-system-migration)
11. [Phase 6: World Package Migration](#11-phase-6-world-package-migration)
12. [Component Contract Migration](#12-component-contract-migration)
13. [Hot-Reload Adaptation](#13-hot-reload-adaptation)
14. [Dependency Resolution Implementation](#14-dependency-resolution-implementation)

### Part II: Operational & Integration
15. [Runtime Integration & Lifecycle](#15-runtime-integration--lifecycle)
16. [Tooling & Authoring Workflow](#16-tooling--authoring-workflow)
17. [File Formats, Paths, and Identifiers](#17-file-formats-paths-and-identifiers)
18. [Versioning & Compatibility](#18-versioning--compatibility)
19. [Security & Sandboxing](#19-security--sandboxing)
20. [Distribution & Deployment](#20-distribution--deployment)

### Part III: Validation & Rollout
21. [Testing Strategy](#21-testing-strategy)
22. [Rollback Plan](#22-rollback-plan)
23. [Implementation Checklist](#23-implementation-checklist)

---

## 1. Executive Summary

This document outlines the migration from void_engine's current monolithic scene/plugin architecture to the new modular Package System. The migration preserves the ECS-authoritative principle while enabling:

- **Dynamic content loading** (creator uploads)
- **Proper dependency management** (acyclic graph with layering)
- **Clear separation** between behaviour (plugins), content (assets), and composition (worlds)
- **Runtime extensibility** (mods, layers, widgets)

### Migration Goals

| Goal | Description |
|------|-------------|
| **Zero data loss** | All existing scenes, assets, and plugins migrate cleanly |
| **Incremental adoption** | Each phase is independently deployable |
| **Backward compatibility** | Legacy scene.json files work during transition |
| **Preserve hot-reload** | snapshot/restore patterns remain intact |

### Estimated Scope

| Phase | Components Affected | Risk Level |
|-------|---------------------|------------|
| 1. Package Infrastructure | New code only | Low |
| 2. Asset Bundle Migration | AssetServer, loaders | Medium |
| 3. Plugin System Migration | Plugin, PluginRegistry | Medium |
| 4. Widget System Migration | UI, debug tools | Low |
| 5. Layer System Migration | Scene, SceneInstantiator | Medium |
| 6. World Package Migration | Runtime, Kernel | High |

---

## 2. Current State Analysis

### 2.1 Current Architecture (from ECS_COMPREHENSIVE_ARCHITECTURE.md)

```
┌─────────────────────────────────────────────────────────────────┐
│                         CURRENT STATE                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Runtime                                                         │
│    ├── Kernel (stages, hot-reload orchestration)                │
│    ├── EventBus (typed events)                                  │
│    ├── void_scene::World                                         │
│    │     ├── void_ecs::World (AUTHORITATIVE)                    │
│    │     ├── Layers (additive patches) ←── loosely defined      │
│    │     ├── Plugins (systems) ←── tightly coupled              │
│    │     └── Widgets (reactive UI) ←── loosely defined          │
│    └── Platform (window/input)                                   │
│                                                                  │
│  Asset System                                                    │
│    ├── AssetServer (load coordination)                          │
│    ├── LoaderRegistry (extension mapping)                       │
│    └── AssetStorage (loaded data)                               │
│                                                                  │
│  Scene Loading                                                   │
│    ├── scene.json → SceneData                                   │
│    ├── SceneInstantiator → ECS entities                         │
│    └── LiveSceneManager (hot-reload)                            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Current Pain Points

| Issue | Description | Impact |
|-------|-------------|--------|
| **Monolithic scenes** | Scene files contain everything (entities, config, settings) | Hard to variant/patch |
| **Plugin coupling** | Plugins register directly with World | No dependency management |
| **No asset bundling** | Assets loaded individually, no grouping | No bundle versioning |
| **Implicit layers** | Layers mentioned but not formalized | Inconsistent patching |
| **Widget tangle** | UI mixed with debug tools | Hard to toggle/configure |
| **No component contracts** | Components defined inline in plugins | No shared schemas |

### 2.3 Current Data Flow

```
scene.json
    │
    ├── Parse → SceneData
    │     ├── camera
    │     ├── environment
    │     ├── lights[]
    │     └── entities[]
    │           ├── name
    │           ├── transform
    │           ├── mesh (optional)
    │           ├── material (optional)
    │           └── children[]
    │
    └── Instantiate → void_ecs::World
          ├── Entity E1 [Transform, Mesh, Material]
          ├── Entity E2 [Transform, Light]
          └── Entity E3 [Transform, Camera]
```

### 2.4 Current Plugin Lifecycle

```cpp
// Current: Plugin directly modifies World
class CombatPlugin : public Plugin {
    Result<void> on_load(PluginContext& ctx) override {
        auto* world = ctx.get<void_ecs::World*>("ecs_world");

        // Direct component registration
        world->register_component<Health>();
        world->register_component<Damage>();

        // Direct system registration
        world->scheduler().add_system(...);

        return Ok();
    }
};
```

---

## 3. Target State Overview

### 3.1 Target Architecture (from PACKAGE_SYSTEM.md)

```
┌─────────────────────────────────────────────────────────────────┐
│                         TARGET STATE                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  world.package (composition root)                                │
│    ├── Root scene definition                                     │
│    ├── Plugin references → plugin.package[]                      │
│    ├── Layer references → layer.package[]                        │
│    ├── Widget references → widget.package[]                      │
│    ├── Asset references → asset.bundle[]                         │
│    ├── Player spawn rules                                        │
│    ├── Environment settings                                      │
│    ├── Gameplay settings                                         │
│    └── ECS resource initialization                               │
│                                                                  │
│  layer.package (patches)                                         │
│    ├── Additive scenes                                           │
│    ├── Spawners / encounters                                     │
│    ├── Lighting overrides                                        │
│    └── Modifier values                                           │
│                                                                  │
│  plugin.package (behaviour)                                      │
│    ├── Component type declarations                               │
│    ├── System definitions                                        │
│    ├── Event handlers                                            │
│    └── Registry definitions                                      │
│                                                                  │
│  widget.package (UI)                                             │
│    ├── Debug HUD                                                 │
│    ├── Console                                                   │
│    ├── Inspectors                                                │
│    └── Custom HUD                                                │
│                                                                  │
│  asset.bundle (content)                                          │
│    ├── Meshes                                                    │
│    ├── Textures / Materials                                      │
│    ├── Animations                                                │
│    ├── Audio                                                     │
│    ├── Prefabs                                                   │
│    └── Definitions (for registries)                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Target Data Flow

```
world.package
    │
    ├── Resolve dependencies (PackageResolver)
    │     ├── Load asset.bundle[] → AssetRegistry
    │     ├── Load plugin.package[] → Register components/systems
    │     ├── Load widget.package[] → Bind UI
    │     └── Stage layer.package[] → Deferred
    │
    └── Instantiate
          ├── Root scene → void_ecs::World
          ├── Apply layers → Additive spawning
          ├── Initialize resources
          └── Start systems
```

### 3.3 Dependency Hierarchy

```
world.package    → may depend on: layer, plugin, widget, asset
layer.package    → may depend on: plugin, widget, asset
plugin.package   → may depend on: plugin (lower layer), asset
widget.package   → may depend on: plugin, asset
asset.bundle     → may depend on: asset (prefer none)

Plugin layers (downward only):
    core.*       → Foundation
    engine.*     → Engine-level
    gameplay.*   → Gameplay
    feature.*    → Features
    mod.*        → Mods/creator
```

---

## 4. Architecture Mapping

### 4.1 Current → Target Mapping

| Current Component | Target Package Type | Migration Strategy |
|-------------------|---------------------|-------------------|
| `scene.json` | Split: `world.package` + `asset.bundle` | Decompose scene into composition (world) and content (assets) |
| `SceneData.entities[]` | `asset.bundle` prefabs | Extract entity templates as prefabs |
| `SceneData.camera` | `world.package` environment | Move to world-level config |
| `SceneData.lights[]` | `asset.bundle` or `layer.package` | Static → assets, dynamic → layers |
| `Plugin` classes | `plugin.package` | Add manifest, declare components |
| `Widget` classes | `widget.package` | Add manifest, declare bindings |
| Asset files (loose) | `asset.bundle` | Group into versioned bundles |

### 4.2 Component Registration Mapping

**Current:**
```cpp
// Plugin directly registers with World
world->register_component<Health>();
```

**Target:**
```json
// plugin.package declares component type
{
  "components": [
    {
      "name": "Health",
      "fields": {
        "current": { "type": "f32", "default": 100 },
        "max": { "type": "f32", "default": 100 }
      }
    }
  ]
}
```

### 4.3 System Registration Mapping

**Current:**
```cpp
// Plugin directly adds system
world->scheduler().add_system(
    SystemDescriptor("DamageSystem").set_stage(SystemStage::Update),
    [](World& w) { /* ... */ }
);
```

**Target:**
```json
// plugin.package declares system
{
  "systems": [
    {
      "name": "DamageSystem",
      "stage": "update",
      "query": ["Health", "DamageQueue"],
      "library": "plugins/combat.dll"
    }
  ]
}
```

### 4.4 Scene → World + Assets Mapping

**Current scene.json:**
```json
{
  "camera": { "position": [0, 5, 10], "fov": 60 },
  "environment": { "ambient": [0.1, 0.1, 0.1] },
  "entities": [
    {
      "name": "Player",
      "transform": { "position": [0, 0, 0] },
      "mesh": { "path": "models/player.gltf" },
      "material": { "albedo": [1, 1, 1, 1] }
    }
  ]
}
```

**Target world.package + asset.bundle:**

```json
// arena.world.json
{
  "package": { "name": "arena_dm", "type": "world", "version": "1.0.0" },
  "dependencies": {
    "plugins": [{ "name": "gameplay.combat", "version": ">=1.0.0" }],
    "assets": [{ "name": "characters.player", "version": ">=1.0.0" }]
  },
  "root_scene": { "path": "scenes/arena.scene.json" },
  "environment": {
    "camera": { "position": [0, 5, 10], "fov": 60 },
    "ambient": [0.1, 0.1, 0.1]
  },
  "player_spawn": {
    "prefab": "prefabs/player.prefab.json"
  }
}
```

```json
// characters_player.bundle.json
{
  "package": { "name": "characters.player", "type": "asset", "version": "1.0.0" },
  "meshes": [
    { "id": "player_mesh", "path": "models/player.gltf" }
  ],
  "prefabs": [
    {
      "id": "player",
      "components": {
        "Transform": {},
        "MeshRenderer": { "mesh": "player_mesh" },
        "Health": { "max": 100, "current": 100 }
      }
    }
  ]
}
```

---

## 5. Migration Phases

### 5.1 Phase Overview

```
┌────────────────────────────────────────────────────────────────────────────┐
│                          MIGRATION PHASES                                   │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Phase 1: Package Infrastructure                                            │
│  ════════════════════════════════                                           │
│  • PackageManifest parser (JSON)                                           │
│  • PackageResolver (dependency graph)                                       │
│  • PackageLoader interface                                                  │
│  • PackageRegistry (loaded packages)                                        │
│                                                                             │
│  Phase 2: Asset Bundle Migration                                            │
│  ═══════════════════════════════                                            │
│  • AssetBundle manifest format                                              │
│  • AssetBundleLoader                                                        │
│  • Migrate AssetServer to use bundles                                       │
│  • Prefab definition support                                                │
│                                                                             │
│  Phase 3: Plugin System Migration                                           │
│  ════════════════════════════════                                           │
│  • PluginPackage manifest format                                            │
│  • Component type declarations (JSON schema)                                │
│  • System registration via manifest                                         │
│  • Plugin dependency resolution                                             │
│                                                                             │
│  Phase 4: Widget System Migration                                           │
│  ════════════════════════════════                                           │
│  • WidgetPackage manifest format                                            │
│  • Widget binding declarations                                              │
│  • Debug/release widget separation                                          │
│                                                                             │
│  Phase 5: Layer System Migration                                            │
│  ═══════════════════════════════                                            │
│  • LayerPackage manifest format                                             │
│  • Additive scene spawning                                                  │
│  • Override/modifier application                                            │
│  • Layer toggle at runtime                                                  │
│                                                                             │
│  Phase 6: World Package Migration                                           │
│  ════════════════════════════════                                           │
│  • WorldPackage manifest format                                             │
│  • Composition orchestration                                                │
│  • Full load sequence implementation                                        │
│  • Legacy scene.json compatibility layer                                    │
│                                                                             │
└────────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Phase Dependencies

```
Phase 1 ──► Phase 2 ──► Phase 3 ──┐
                                  │
                                  ├──► Phase 6
                                  │
            Phase 4 ──────────────┤
                                  │
            Phase 5 ──────────────┘
```

---

## 6. Phase 1: Package Infrastructure

### 6.1 New Files to Create

```
include/void_engine/package/
├── fwd.hpp                    # Forward declarations
├── manifest.hpp               # PackageManifest, PackageType
├── resolver.hpp               # PackageResolver, DependencyGraph
├── loader.hpp                 # PackageLoader interface
├── registry.hpp               # PackageRegistry
├── version.hpp                # SemVer parsing
└── package.hpp                # Main include

src/package/
├── manifest.cpp
├── resolver.cpp
├── loader.cpp
├── registry.cpp
├── version.cpp
└── CMakeLists.txt
```

### 6.2 PackageManifest

```cpp
// include/void_engine/package/manifest.hpp

namespace void_package {

enum class PackageType : uint8_t {
    World,
    Layer,
    Plugin,
    Widget,
    Asset
};

struct PackageDependency {
    std::string name;
    std::string version_constraint;  // ">=1.0.0", "^2.0", etc.
    bool optional = false;
};

struct PackageManifest {
    // Identity
    std::string name;
    PackageType type;
    Version version;

    // Dependencies by type
    std::vector<PackageDependency> plugin_deps;
    std::vector<PackageDependency> widget_deps;
    std::vector<PackageDependency> layer_deps;
    std::vector<PackageDependency> asset_deps;

    // Parsing
    static Result<PackageManifest> from_json(const nlohmann::json& j);
    static Result<PackageManifest> load(const std::string& path);

    // Validation
    Result<void> validate() const;
    bool may_depend_on(PackageType other) const;
};

} // namespace void_package
```

### 6.3 PackageResolver

```cpp
// include/void_engine/package/resolver.hpp

namespace void_package {

struct ResolvedPackage {
    PackageManifest manifest;
    std::string path;
    std::vector<std::string> resolved_deps;  // In load order
};

class PackageResolver {
public:
    // Add available packages
    void add_available(const PackageManifest& manifest, const std::string& path);

    // Resolve a package and all dependencies
    Result<std::vector<ResolvedPackage>> resolve(const std::string& package_name);

    // Check for cycles
    Result<void> validate_acyclic() const;

    // Validate plugin layering (core < engine < gameplay < feature < mod)
    Result<void> validate_plugin_layers() const;

private:
    std::map<std::string, std::pair<PackageManifest, std::string>> m_available;

    Result<void> topological_sort(
        const std::string& root,
        std::vector<std::string>& order,
        std::set<std::string>& visited,
        std::set<std::string>& in_stack
    ) const;
};

} // namespace void_package
```

### 6.4 PackageLoader Interface

```cpp
// include/void_engine/package/loader.hpp

namespace void_package {

class PackageLoader {
public:
    virtual ~PackageLoader() = default;

    virtual PackageType supported_type() const = 0;
    virtual Result<void> load(const ResolvedPackage& package, LoadContext& ctx) = 0;
    virtual Result<void> unload(const std::string& package_name, LoadContext& ctx) = 0;
};

class LoadContext {
public:
    void_ecs::World* ecs_world() const;
    void_asset::AssetServer* asset_server() const;
    void_event::EventBus* event_bus() const;

    // Register loaders
    void register_loader(std::unique_ptr<PackageLoader> loader);
    PackageLoader* get_loader(PackageType type) const;
};

} // namespace void_package
```

### 6.5 PackageRegistry

```cpp
// include/void_engine/package/registry.hpp

namespace void_package {

enum class PackageStatus {
    Available,    // Known but not loaded
    Loading,      // Currently loading
    Loaded,       // Fully loaded
    Failed        // Failed to load
};

struct LoadedPackage {
    ResolvedPackage resolved;
    PackageStatus status;
    std::chrono::steady_clock::time_point load_time;
};

class PackageRegistry {
public:
    // Discovery
    void scan_directory(const std::string& path);

    // Loading
    Result<void> load_package(const std::string& name, LoadContext& ctx);
    Result<void> unload_package(const std::string& name, LoadContext& ctx);

    // Query
    PackageStatus status(const std::string& name) const;
    std::vector<std::string> loaded_packages() const;
    std::vector<std::string> available_packages() const;

    // Hot-reload
    Result<void> reload_package(const std::string& name, LoadContext& ctx);

private:
    PackageResolver m_resolver;
    std::map<std::string, LoadedPackage> m_packages;
};

} // namespace void_package
```

### 6.6 Phase 1 Tasks

| Task | Priority | Description |
|------|----------|-------------|
| P1.1 | High | Create `void_package` module structure |
| P1.2 | High | Implement `PackageManifest` JSON parsing |
| P1.3 | High | Implement `Version` (SemVer) parsing and comparison |
| P1.4 | High | Implement `PackageResolver` with cycle detection |
| P1.5 | Medium | Implement plugin layer validation |
| P1.6 | Medium | Implement `PackageRegistry` |
| P1.7 | Medium | Implement `LoadContext` |
| P1.8 | Low | Add unit tests for resolver |

---

## 7. Phase 2: Asset Bundle Migration

### 7.1 AssetBundleManifest

```cpp
// Extension to manifest.hpp

namespace void_package {

struct MeshEntry {
    std::string id;
    std::string path;
    std::vector<std::string> lod_paths;
    std::optional<std::string> collision_path;
};

struct TextureEntry {
    std::string id;
    std::string path;
    std::string format;  // "bc7", "bc5", etc.
    bool mipmaps = true;
};

struct MaterialEntry {
    std::string id;
    std::string shader;
    std::map<std::string, std::string> textures;
    std::map<std::string, nlohmann::json> parameters;
};

struct PrefabEntry {
    std::string id;
    std::map<std::string, nlohmann::json> components;
    std::vector<std::string> tags;
};

struct DefinitionEntry {
    std::string registry_type;  // "weapons", "auras", "abilities"
    nlohmann::json data;
};

struct AssetBundleManifest {
    PackageManifest base;

    std::vector<MeshEntry> meshes;
    std::vector<TextureEntry> textures;
    std::vector<MaterialEntry> materials;
    std::vector<PrefabEntry> prefabs;
    std::map<std::string, std::vector<DefinitionEntry>> definitions;

    static Result<AssetBundleManifest> from_json(const nlohmann::json& j);
};

} // namespace void_package
```

### 7.2 AssetBundleLoader

```cpp
// include/void_engine/package/asset_bundle_loader.hpp

namespace void_package {

class AssetBundleLoader : public PackageLoader {
public:
    PackageType supported_type() const override { return PackageType::Asset; }

    Result<void> load(const ResolvedPackage& package, LoadContext& ctx) override;
    Result<void> unload(const std::string& package_name, LoadContext& ctx) override;

private:
    // Load individual asset types
    Result<void> load_meshes(const AssetBundleManifest& manifest,
                              void_asset::AssetServer& server);
    Result<void> load_textures(const AssetBundleManifest& manifest,
                                void_asset::AssetServer& server);
    Result<void> load_materials(const AssetBundleManifest& manifest,
                                 void_asset::AssetServer& server);
    Result<void> register_prefabs(const AssetBundleManifest& manifest,
                                   PrefabRegistry& registry);
    Result<void> register_definitions(const AssetBundleManifest& manifest,
                                       DefinitionRegistry& registry);
};

} // namespace void_package
```

### 7.3 PrefabRegistry

New concept to bridge asset prefabs and ECS instantiation:

```cpp
// include/void_engine/package/prefab_registry.hpp

namespace void_package {

struct PrefabDefinition {
    std::string id;
    std::string source_bundle;
    std::map<std::string, nlohmann::json> components;
    std::vector<std::string> tags;
};

class PrefabRegistry {
public:
    void register_prefab(PrefabDefinition def);
    const PrefabDefinition* get(const std::string& id) const;

    // Instantiate prefab into ECS
    Result<void_ecs::Entity> instantiate(
        const std::string& prefab_id,
        void_ecs::World& world,
        const std::optional<TransformData>& override_transform = std::nullopt
    );

private:
    std::map<std::string, PrefabDefinition> m_prefabs;
};

} // namespace void_package
```

### 7.4 Migration from Loose Assets

```
BEFORE (loose files):
assets/
├── models/
│   ├── player.gltf
│   └── enemy.gltf
├── textures/
│   ├── player_albedo.png
│   └── player_normal.png
└── audio/
    └── footstep.ogg

AFTER (bundled):
packages/
└── characters/
    ├── characters.bundle.json    # Manifest
    ├── models/
    │   ├── player.gltf
    │   └── enemy.gltf
    ├── textures/
    │   ├── player_albedo.png
    │   └── player_normal.png
    └── audio/
        └── footstep.ogg
```

### 7.5 Phase 2 Tasks

| Task | Priority | Description |
|------|----------|-------------|
| P2.1 | High | Define `AssetBundleManifest` JSON schema |
| P2.2 | High | Implement `AssetBundleLoader` |
| P2.3 | High | Implement `PrefabRegistry` |
| P2.4 | High | Implement `DefinitionRegistry` |
| P2.5 | Medium | Migrate `AssetServer` to support bundle-relative paths |
| P2.6 | Medium | Create asset bundling tool/script |
| P2.7 | Medium | Add prefab instantiation to `SceneInstantiator` |
| P2.8 | Low | Create migration script for existing assets |

---

## 8. Phase 3: Plugin System Migration

### 8.1 PluginPackageManifest

```cpp
namespace void_package {

struct ComponentDeclaration {
    std::string name;
    std::map<std::string, FieldDeclaration> fields;
};

struct FieldDeclaration {
    std::string type;  // "f32", "u32", "bool", "Entity", "array<T>"
    std::optional<nlohmann::json> default_value;
};

struct SystemDeclaration {
    std::string name;
    std::string stage;  // "update", "pre_render", etc.
    std::vector<std::string> query;  // Component names
    int priority = 0;
    std::string library;  // DLL/SO path
    std::optional<std::string> run_condition;
};

struct EventHandlerDeclaration {
    std::string event;
    std::string handler;
    std::string library;
    std::optional<nlohmann::json> filter;
};

struct RegistryDeclaration {
    std::string name;
    std::string type;
    std::string entries_path;
};

struct PluginPackageManifest {
    PackageManifest base;

    std::vector<ComponentDeclaration> components;
    std::vector<std::string> tags;
    std::vector<SystemDeclaration> systems;
    std::vector<EventHandlerDeclaration> event_handlers;
    std::vector<RegistryDeclaration> registries;

    static Result<PluginPackageManifest> from_json(const nlohmann::json& j);
};

} // namespace void_package
```

### 8.2 Component Schema Registry

Bridge between JSON declarations and C++ component registration:

```cpp
// include/void_engine/package/component_schema.hpp

namespace void_package {

class ComponentSchemaRegistry {
public:
    // Register from plugin manifest
    Result<void_ecs::ComponentId> register_from_declaration(
        const ComponentDeclaration& decl,
        void_ecs::ComponentRegistry& ecs_registry
    );

    // Lookup schema by name
    const ComponentDeclaration* get_schema(const std::string& name) const;

    // Create component instance from JSON
    Result<std::vector<std::byte>> create_instance(
        const std::string& component_name,
        const nlohmann::json& data
    ) const;

private:
    std::map<std::string, ComponentDeclaration> m_schemas;
    std::map<std::string, void_ecs::ComponentId> m_ids;
};

} // namespace void_package
```

### 8.3 PluginPackageLoader

```cpp
namespace void_package {

class PluginPackageLoader : public PackageLoader {
public:
    PackageType supported_type() const override { return PackageType::Plugin; }

    Result<void> load(const ResolvedPackage& package, LoadContext& ctx) override;
    Result<void> unload(const std::string& package_name, LoadContext& ctx) override;

private:
    // Register component types with ECS
    Result<void> register_components(
        const PluginPackageManifest& manifest,
        void_ecs::World& world,
        ComponentSchemaRegistry& schemas
    );

    // Load and register systems
    Result<void> register_systems(
        const PluginPackageManifest& manifest,
        void_ecs::SystemScheduler& scheduler
    );

    // Subscribe event handlers
    Result<void> register_event_handlers(
        const PluginPackageManifest& manifest,
        void_event::EventBus& bus
    );

    // Initialize registries with definitions from asset bundles
    Result<void> populate_registries(
        const PluginPackageManifest& manifest,
        DefinitionRegistry& definitions
    );

    // Dynamic library management
    std::map<std::string, DynamicLibrary> m_libraries;
};

} // namespace void_package
```

### 8.4 Plugin Migration Strategy

**Step 1: Create manifest alongside existing plugin:**
```
plugins/
├── combat/
│   ├── combat.plugin.json   # NEW: manifest
│   ├── combat.cpp           # EXISTING: code
│   └── combat.hpp           # EXISTING: header
```

**Step 2: Refactor plugin to use manifest-driven registration:**
```cpp
// BEFORE: Direct registration
class CombatPlugin : public Plugin {
    Result<void> on_load(PluginContext& ctx) override {
        world->register_component<Health>();
        world->scheduler().add_system(...);
    }
};

// AFTER: Manifest-driven with entry points
extern "C" {
    // System entry points (called by PluginPackageLoader)
    void damage_system_run(void_ecs::World& world);
    void on_damage_event(const DamageEvent& event);
}
```

### 8.5 Phase 3 Tasks

| Task | Priority | Description |
|------|----------|-------------|
| P3.1 | High | Define `PluginPackageManifest` JSON schema |
| P3.2 | High | Implement `ComponentSchemaRegistry` |
| P3.3 | High | Implement `PluginPackageLoader` |
| P3.4 | High | Create component-to-JSON serialization |
| P3.5 | Medium | Implement dynamic library loading for systems |
| P3.6 | Medium | Implement event handler registration |
| P3.7 | Medium | Migrate existing plugins to new format |
| P3.8 | Low | Create plugin migration tool |

---

## 9. Phase 4: Widget System Migration

### 9.1 WidgetPackageManifest

```cpp
namespace void_package {

struct WidgetDeclaration {
    std::string id;
    std::string type;  // "debug_hud", "console", "inspector", "custom"
    std::vector<std::string> enabled_in_builds;  // "debug", "development", "release"
    std::optional<std::string> toggle_key;
    nlohmann::json config;
};

struct WidgetBinding {
    std::string widget_id;
    std::string data_source;  // ECS query or resource path
    std::string binding_type;  // "query", "resource", "event"
};

struct WidgetPackageManifest {
    PackageManifest base;

    std::vector<WidgetDeclaration> widgets;
    std::vector<WidgetBinding> bindings;

    static Result<WidgetPackageManifest> from_json(const nlohmann::json& j);
};

} // namespace void_package
```

### 9.2 WidgetPackageLoader

```cpp
namespace void_package {

class WidgetPackageLoader : public PackageLoader {
public:
    PackageType supported_type() const override { return PackageType::Widget; }

    Result<void> load(const ResolvedPackage& package, LoadContext& ctx) override;
    Result<void> unload(const std::string& package_name, LoadContext& ctx) override;

private:
    Result<void> create_widgets(
        const WidgetPackageManifest& manifest,
        WidgetManager& widgets
    );

    Result<void> setup_bindings(
        const WidgetPackageManifest& manifest,
        void_ecs::World& world,
        WidgetManager& widgets
    );
};

} // namespace void_package
```

### 9.3 Phase 4 Tasks

| Task | Priority | Description |
|------|----------|-------------|
| P4.1 | Medium | Define `WidgetPackageManifest` JSON schema |
| P4.2 | Medium | Implement `WidgetPackageLoader` |
| P4.3 | Medium | Create `WidgetManager` for widget lifecycle |
| P4.4 | Medium | Implement ECS query bindings for widgets |
| P4.5 | Low | Extract existing debug tools as widget packages |
| P4.6 | Low | Add build-type filtering |

---

## 10. Phase 5: Layer System Migration

### 10.1 LayerPackageManifest

```cpp
namespace void_package {

struct AdditiveSceneEntry {
    std::string path;
    std::string spawn_mode;  // "immediate", "deferred"
};

struct SpawnerEntry {
    std::string id;
    nlohmann::json volume;
    std::string prefab;
    float spawn_rate;
    int max_active;
};

struct LightingOverride {
    std::optional<nlohmann::json> sun_override;
    std::vector<nlohmann::json> additional_lights;
    std::optional<nlohmann::json> ambient_override;
};

struct ModifierEntry {
    std::string path;  // "health_multiplier", "damage_multiplier"
    nlohmann::json value;
};

struct LayerPackageManifest {
    PackageManifest base;

    std::vector<AdditiveSceneEntry> additive_scenes;
    std::vector<SpawnerEntry> spawners;
    std::optional<LightingOverride> lighting;
    std::optional<nlohmann::json> weather_override;
    std::vector<nlohmann::json> objectives;
    std::vector<ModifierEntry> modifiers;

    static Result<LayerPackageManifest> from_json(const nlohmann::json& j);
};

} // namespace void_package
```

### 10.2 LayerPackageLoader

```cpp
namespace void_package {

class LayerPackageLoader : public PackageLoader {
public:
    PackageType supported_type() const override { return PackageType::Layer; }

    Result<void> load(const ResolvedPackage& package, LoadContext& ctx) override;
    Result<void> unload(const std::string& package_name, LoadContext& ctx) override;

    // Layers are "staged" first, then "applied" by world
    Result<StagedLayer> stage(const ResolvedPackage& package, LoadContext& ctx);
    Result<void> apply(const StagedLayer& layer, void_ecs::World& world);
    Result<void> unapply(const std::string& layer_name, void_ecs::World& world);

private:
    struct StagedLayer {
        std::string name;
        LayerPackageManifest manifest;
        std::vector<void_ecs::Entity> spawned_entities;  // For unapply
    };

    std::map<std::string, StagedLayer> m_staged;
};

} // namespace void_package
```

### 10.3 Layer Application Flow

```
1. Layer is loaded (staged but not applied)
2. World activates layer
   a. Spawn additive scene entities
   b. Create spawner entities
   c. Apply lighting overrides (modify light entities)
   d. Apply modifiers (update ECS resources)
   e. Track all created/modified entities
3. Layer is deactivated
   a. Despawn tracked entities
   b. Revert lighting to base
   c. Revert modifiers
```

### 10.4 Phase 5 Tasks

| Task | Priority | Description |
|------|----------|-------------|
| P5.1 | Medium | Define `LayerPackageManifest` JSON schema |
| P5.2 | Medium | Implement `LayerPackageLoader` with staging |
| P5.3 | Medium | Implement layer application (additive spawning) |
| P5.4 | Medium | Implement layer unapplication (despawning) |
| P5.5 | Medium | Implement modifier system |
| P5.6 | Low | Runtime layer toggle support |
| P5.7 | Low | Layer ordering/priority |

---

## 11. Phase 6: World Package Migration

### 11.1 WorldPackageManifest

```cpp
namespace void_package {

struct PlayerSpawnConfig {
    std::string prefab;
    std::string spawn_selection;  // "round_robin", "random", "fixed"
    std::optional<nlohmann::json> initial_inventory;
    std::optional<nlohmann::json> initial_stats;
};

struct EnvironmentConfig {
    float time_of_day = 12.0f;
    std::string skybox;
    std::optional<nlohmann::json> weather;
    std::optional<nlohmann::json> post_process;
};

struct GameplayConfig {
    std::string difficulty = "normal";
    int match_length_seconds = 0;
    int score_limit = 0;
    bool friendly_fire = false;
    std::map<std::string, nlohmann::json> ruleset_flags;
};

struct WorldPackageManifest {
    PackageManifest base;

    // Scene definition
    std::string root_scene_path;
    nlohmann::json world_bounds;

    // Player
    PlayerSpawnConfig player_spawn;

    // Environment
    EnvironmentConfig environment;

    // Gameplay
    GameplayConfig gameplay;

    // ECS resources
    std::map<std::string, nlohmann::json> ecs_resources;

    // World logic
    std::optional<nlohmann::json> world_logic;

    static Result<WorldPackageManifest> from_json(const nlohmann::json& j);
};

} // namespace void_package
```

### 11.2 WorldPackageLoader

```cpp
namespace void_package {

class WorldPackageLoader : public PackageLoader {
public:
    PackageType supported_type() const override { return PackageType::World; }

    Result<void> load(const ResolvedPackage& package, LoadContext& ctx) override;
    Result<void> unload(const std::string& package_name, LoadContext& ctx) override;

private:
    // Full load sequence
    Result<void> execute_load_sequence(
        const WorldPackageManifest& manifest,
        LoadContext& ctx
    );

    // Individual steps
    Result<void> load_root_scene(const WorldPackageManifest& manifest,
                                  void_ecs::World& world);
    Result<void> apply_active_layers(const ResolvedPackage& package,
                                      LayerPackageLoader& layer_loader,
                                      void_ecs::World& world);
    Result<void> initialize_ecs_resources(const WorldPackageManifest& manifest,
                                           void_ecs::World& world);
    Result<void> configure_environment(const WorldPackageManifest& manifest,
                                        void_ecs::World& world);
    Result<void> start_systems(void_ecs::SystemScheduler& scheduler);
    Result<void> emit_world_loaded(void_event::EventBus& bus);
};

} // namespace void_package
```

### 11.3 Runtime Integration

Modify `Runtime` to use package system:

```cpp
// Modified Runtime class
class Runtime {
public:
    // NEW: Load world via package system
    Result<void> load_world_package(const std::string& world_package_name);

    // DEPRECATED: Legacy support during migration
    Result<void> load_world_legacy(const std::string& scene_json_path);

private:
    PackageRegistry m_package_registry;
    LoadContext m_load_context;
};
```

### 11.4 Legacy Compatibility

During migration, support both old and new formats:

```cpp
Result<void> Runtime::load_world(const std::string& path) {
    // Check if it's a package or legacy scene
    if (path.ends_with(".world.json")) {
        return load_world_package(path);
    } else if (path.ends_with(".json")) {
        // Legacy scene.json - convert on the fly
        return load_world_legacy(path);
    }
    return Error("Unknown world format");
}
```

### 11.5 Phase 6 Tasks

| Task | Priority | Description |
|------|----------|-------------|
| P6.1 | High | Define `WorldPackageManifest` JSON schema |
| P6.2 | High | Implement `WorldPackageLoader` |
| P6.3 | High | Implement full load sequence |
| P6.4 | High | Integrate with `Runtime` |
| P6.5 | Medium | Legacy scene.json compatibility |
| P6.6 | Medium | World switching support |
| P6.7 | Low | World state snapshot for hot-reload |
| P6.8 | Low | Migration tool for existing scenes |

---

## 12. Component Contract Migration

### 12.1 Shared Component Schemas

Create a `core.ecs` plugin package with foundational components:

```json
// packages/core/ecs/core_ecs.plugin.json
{
  "package": {
    "name": "core.ecs",
    "type": "plugin",
    "version": "1.0.0"
  },
  "components": [
    {
      "name": "Transform",
      "fields": {
        "position": { "type": "vec3", "default": [0, 0, 0] },
        "rotation": { "type": "quat", "default": [0, 0, 0, 1] },
        "scale": { "type": "vec3", "default": [1, 1, 1] }
      }
    },
    {
      "name": "LocalTransform",
      "fields": {
        "position": { "type": "vec3", "default": [0, 0, 0] },
        "rotation": { "type": "quat", "default": [0, 0, 0, 1] },
        "scale": { "type": "vec3", "default": [1, 1, 1] }
      }
    },
    {
      "name": "GlobalTransform",
      "fields": {
        "matrix": { "type": "mat4" }
      }
    },
    {
      "name": "Parent",
      "fields": {
        "entity": { "type": "Entity" }
      }
    },
    {
      "name": "Children",
      "fields": {
        "entities": { "type": "array<Entity>", "capacity": 64 }
      }
    }
  ],
  "tags": ["Root", "Static", "Dynamic"],
  "systems": [
    {
      "name": "TransformPropagationSystem",
      "stage": "post_update",
      "query": ["LocalTransform", "?Parent", "GlobalTransform"],
      "library": "core_ecs.dll"
    }
  ]
}
```

### 12.2 Gameplay Component Schemas

```json
// packages/gameplay/combat/gameplay_combat.plugin.json
{
  "package": {
    "name": "gameplay.combat",
    "type": "plugin",
    "version": "1.0.0"
  },
  "dependencies": {
    "plugins": [
      { "name": "core.ecs", "version": ">=1.0.0" }
    ]
  },
  "components": [
    {
      "name": "Health",
      "fields": {
        "current": { "type": "f32", "default": 100 },
        "max": { "type": "f32", "default": 100 },
        "regeneration_rate": { "type": "f32", "default": 0 }
      }
    },
    {
      "name": "DamageQueue",
      "fields": {
        "pending": { "type": "array<DamageInstance>", "capacity": 16 }
      }
    }
  ],
  "tags": ["Damageable", "Invulnerable"]
}
```

### 12.3 Migration from Inline Components

**Current (inline in C++):**
```cpp
struct Health {
    float current = 100.0f;
    float max = 100.0f;
    float regeneration_rate = 0.0f;
};
```

**Migration steps:**
1. Add JSON schema to plugin manifest (keeps C++ struct)
2. Generate C++ from JSON schema (optional, for type safety)
3. Runtime validates instances against schema

---

## 13. Hot-Reload Adaptation

### 13.1 Package-Aware Snapshots

Extend `WorldSnapshot` to include package state:

```cpp
struct PackageSnapshot {
    std::string package_name;
    std::string package_version;
    std::vector<uint8_t> package_state;  // Package-specific data
};

struct WorldSnapshot {
    static constexpr uint32_t CURRENT_VERSION = 2;  // Bump version

    uint32_t version;
    std::vector<EntitySnapshot> entities;
    std::vector<ComponentMeta> component_registry;

    // NEW: Package state
    std::vector<PackageSnapshot> loaded_packages;
    std::string active_world_package;
    std::vector<std::string> active_layers;
};
```

### 13.2 Hot-Reload Flow with Packages

```
1. File change detected
2. Determine affected packages
3. For each affected package:
   a. Take package-specific snapshot
   b. Unload package (if plugin/widget)
   c. Reload package code
   d. Re-register components/systems
   e. Restore package snapshot
4. If world-level change:
   a. Take full WorldSnapshot
   b. Reload world package
   c. Restore WorldSnapshot
```

### 13.3 Package HotReloadable Interface

```cpp
class PackageLoader {
public:
    // Hot-reload support
    virtual bool supports_hot_reload() const { return false; }
    virtual Result<PackageSnapshot> take_snapshot(const std::string& name) {
        return Error("Not supported");
    }
    virtual Result<void> restore_snapshot(const PackageSnapshot& snapshot) {
        return Error("Not supported");
    }
};
```

---

## 14. Dependency Resolution Implementation

### 14.1 Topological Sort Algorithm

```cpp
Result<std::vector<std::string>> PackageResolver::resolve(
    const std::string& root_package
) {
    std::vector<std::string> order;
    std::set<std::string> visited;
    std::set<std::string> in_stack;

    auto result = visit(root_package, order, visited, in_stack);
    if (!result) {
        return result.error();
    }

    return order;  // Load order (dependencies first)
}

Result<void> PackageResolver::visit(
    const std::string& name,
    std::vector<std::string>& order,
    std::set<std::string>& visited,
    std::set<std::string>& in_stack
) {
    if (in_stack.contains(name)) {
        return Error("Cycle detected: " + name);
    }
    if (visited.contains(name)) {
        return Ok();
    }

    in_stack.insert(name);

    auto* pkg = m_available.find(name);
    if (!pkg) {
        return Error("Package not found: " + name);
    }

    // Visit all dependencies first
    for (const auto& dep : pkg->manifest.all_dependencies()) {
        auto result = visit(dep.name, order, visited, in_stack);
        if (!result) return result;
    }

    in_stack.erase(name);
    visited.insert(name);
    order.push_back(name);

    return Ok();
}
```

### 14.2 Plugin Layer Validation

```cpp
Result<void> PackageResolver::validate_plugin_layers() const {
    // Extract layer from plugin name (e.g., "gameplay.combat" → "gameplay")
    auto get_layer = [](const std::string& name) -> int {
        if (name.starts_with("core.")) return 0;
        if (name.starts_with("engine.")) return 1;
        if (name.starts_with("gameplay.")) return 2;
        if (name.starts_with("feature.")) return 3;
        if (name.starts_with("mod.")) return 4;
        return -1;  // Unknown
    };

    for (const auto& [name, entry] : m_available) {
        if (entry.manifest.type != PackageType::Plugin) continue;

        int my_layer = get_layer(name);
        if (my_layer < 0) continue;  // Skip unknown layers

        for (const auto& dep : entry.manifest.plugin_deps) {
            int dep_layer = get_layer(dep.name);
            if (dep_layer < 0) continue;

            if (dep_layer > my_layer) {
                return Error(
                    "Plugin layer violation: " + name +
                    " (layer " + std::to_string(my_layer) +
                    ") depends on " + dep.name +
                    " (layer " + std::to_string(dep_layer) + ")"
                );
            }
        }
    }

    return Ok();
}
```

---

## 15. Runtime Integration & Lifecycle

This section describes how the package system integrates with the existing runtime (kernel, ECS world, scene instantiation, hot-reload). It complements the structural migration phases.

### 15.1 World Boot Sequence

At runtime, the engine should follow this sequence to boot a world from a `world.package`:

**Step 1: Discover packages**
```cpp
PackageRegistry::scan_directory(<content_root>);
// Optionally, register additional packages from network / platform SDK
```

**Step 2: Resolve world + dependencies**
```cpp
PackageRegistry::load_package("world_id", ctx);
// Internally calls PackageResolver::resolve("world_id") to produce ordered list:
// asset.bundle → plugin.package → widget.package → layer.package → world.package
// Validates acyclic graph and plugin layering rules
```

**Step 3: Load packages by type**
```cpp
for (const auto& resolved : load_order) {
    LoadContext::get_loader(resolved.manifest.type)
        ->load(resolved, ctx);
}
```

Recommended high-level order:
1. Asset bundles (populate AssetServer, PrefabRegistry, DefinitionRegistry)
2. Plugins (register components/systems, registries)
3. Widgets (UI + debug)
4. Layers (staged only, not applied)
5. World (composition + scene instantiation)

**Step 4: Instantiate world**

From `WorldPackageManifest`:
- Load root scene into ECS (`SceneInstantiator` using `PrefabRegistry`)
- Apply environment and gameplay config (resources)
- Apply default/auto layers via `LayerPackageLoader::apply`
- Initialize ECS resources required by plugins

**Step 5: Start simulation**

Kernel stages run as today, but:
- System graph is now defined by loaded `plugin.package`s
- Widgets are added to the UI pass based on `widget.package`s
- Active layers are tracked by layer name, not hard-coded

### 15.2 Runtime Operations API

The following operations must be supported over the package system:

| Operation | API | Description |
|-----------|-----|-------------|
| Hot-reload package | `PackageRegistry::reload_package(name, ctx)` | Reload single package |
| Toggle layer | `LayerPackageLoader::apply/unapply(layer, world)` | Runtime layer control |
| Load optional mod | `PackageRegistry::load_package(mod_name, ctx)` | Dynamic mod loading |
| Unload mod | `PackageRegistry::unload_package(mod_name, ctx)` | Clean mod removal |
| Query loaded | `PackageRegistry::loaded_packages()` | List active packages |

### 15.3 Hot-Reload Semantics by Package Type

| Package Type | Hot-Reload Support | Scope |
|--------------|-------------------|-------|
| `asset.bundle` | Yes | Reload assets, update registries, optionally re-instantiate affected prefabs |
| `plugin.package` | Partial | Stop systems, unload library, reload, re-register (may require world reset) |
| `widget.package` | Yes | Rebuild UI tree/bindings |
| `layer.package` | Yes | Unapply → reload → re-apply |
| `world.package` | No (full reload) | Requires full world transition |

### 15.4 Error Handling & Failure Modes

**Manifest parse/validation failure:**
- Mark package as `PackageStatus::Failed`
- Exclude from dependency resolution
- Log with path + reason
- Surface error to caller

**Missing dependency:**
```cpp
// PackageResolver::resolve returns error describing:
struct DependencyError {
    std::string missing_package;
    std::string required_by;
    std::string version_constraint;
};
```
- World load should fail early with clear message

**Runtime load failure:**
- If **optional** dependency fails: Log warning, continue
- If **required** dependency fails: Fail world load, offer rollback to last known good package set

**Partial load recovery:**
```cpp
Result<void> PackageRegistry::load_package_with_fallback(
    const std::string& name,
    const std::string& fallback_name,
    LoadContext& ctx
);
```

### 15.5 Graph Representation at Runtime

The dependency graph should be queryable at runtime:

```cpp
class PackageResolver {
public:
    // Query operations
    std::vector<std::string> get_dependents(const std::string& package_name) const;
    std::vector<std::string> get_dependencies(const std::string& package_name) const;
    bool would_create_cycle(const std::string& from, const std::string& to) const;

    // Visualization
    std::string to_dot_graph() const;  // GraphViz format for debugging
};
```

---

## 16. Tooling & Authoring Workflow

This section defines the minimal tooling required to work with the package system for both engine developers and creators.

### 16.1 Command-Line Tools (`voidpkg`)

Provide a `voidpkg` CLI to operate on packages:

| Command | Description |
|---------|-------------|
| `voidpkg validate <path>` | Parse manifest, run JSON schema validation, check deps |
| `voidpkg pack <source_dir> -o <package_file>` | Build package from source layout |
| `voidpkg list-deps <world_name>` | Print dependency tree |
| `voidpkg migrate-scene <scene.json> -o <dir>` | Convert legacy scene to world.package + asset.bundle |
| `voidpkg check-layers <plugin_name>` | Validate plugin layer rules |

**Validation steps performed by `voidpkg validate`:**
1. JSON schema validation per package type
2. Dependency resolution checks (can deps be satisfied?)
3. Plugin layer validation (no upward dependencies)
4. Content sanity checks:
   - Missing asset files referenced in manifest
   - Unknown component names in prefabs
   - Invalid version constraints

### 16.2 Editor Integration

The editor should treat **packages** as the primary artifact, not loose files:

**Package-first operations:**
- "Create new world" → generates `.world.json` skeleton manifest
- "Create layer from scene diff" → builds `.layer.json` comparing against base world
- "Convert selection to prefab" → updates corresponding `asset.bundle`
- "Add system to plugin" → updates `.plugin.json` manifest

**Runtime integration:**
- Editor can launch test world via `PackageRegistry` with given `world.package`
- Visualize loaded packages and dependency graph
- Show which package each entity/component originated from

**Round-trip guarantees:**
- Edit entity in viewport → updates prefab in asset.bundle
- Change component default in inspector → updates plugin.package schema
- Add prop to scene → editor decides: base world vs layer

### 16.3 Creator SDK Layout

For external/creator workflows, define a minimal repository layout:

```
my_mod/
├── manifest.json              # Optional: high-level mod metadata
├── assets/
│   └── my_weapon.bundle.json
├── plugins/
│   └── my_weapon.plugin.json
├── widgets/
│   └── my_hud.widget.json
├── scripts/                   # If scripted plugins enabled
│   └── my_weapon_logic.lua
└── out/                       # Build output (gitignored)
```

**Build command:**
```bash
voidpkg build --config release
# Validates all manifests
# Compiles scripts (if any) to VM bytecode
# Emits deployable packages to out/
```

### 16.4 Minimum Creator Environment

| Requirement | Description |
|-------------|-------------|
| `voidpkg` CLI | Validation, packing, testing |
| Text editor | JSON manifest editing |
| Asset tools | Blender/etc for models, Audacity for audio |
| Local runtime | Test world loading locally |
| Documentation | This spec + API reference |

**Local testing workflow:**
1. Create package structure
2. `voidpkg validate .` - Check for errors
3. `voidpkg pack . -o test.pak` - Build package
4. Launch engine with `--load-package test.pak --world my_test_world`
5. Iterate

---

## 17. File Formats, Paths, and Identifiers

### 17.1 File Format Rules

| Rule | Specification |
|------|---------------|
| Encoding | UTF-8 (no BOM) |
| Format | Strict JSON (no comments, no trailing commas) |
| Line endings | LF preferred, CRLF accepted |
| Parser | `nlohmann::json` on engine side |

**Manifest file extensions:**
- `.world.json` - World packages
- `.layer.json` - Layer packages
- `.plugin.json` - Plugin packages
- `.widget.json` - Widget packages
- `.bundle.json` - Asset bundles

**Content files** (models, textures, audio, etc.) may use any engine-supported format.

### 17.2 Path Semantics

**Package-relative paths:**
- Manifest fields like `"path"` are resolved relative to the package root directory
- The engine's `AssetServer` maps these to real filesystem/VFS locations

**Example:**
```json
// In packages/weapons/plasma.bundle.json
{
  "meshes": [
    { "id": "plasma_rifle", "path": "models/plasma.gltf" }
  ]
}
// Resolves to: packages/weapons/models/plasma.gltf
```

**Case sensitivity:**
- Paths are treated as **case-sensitive** internally
- Avoids cross-platform issues (Windows vs Linux)

**Collision rules:**
- Two packages may reference the same underlying asset file (shared assets)
- Within a single package, `"id"` fields must be unique per collection

### 17.3 ID and Namespace Conventions

**Package names** follow dot-separated namespace convention:

```
core.*       → ECS, math, physics (foundation)
engine.*     → Render, audio, input, platform
gameplay.*   → Combat, movement, AI (game systems)
feature.*    → Optional high-level features
mod.*        → User/creator-authored content
```

**ID uniqueness:**
- IDs must be stable across versions (if semantics preserved)
- Globally unique when combined as `package.name:id`
- Systems reference definitions by `(package_name, id)` or resolved registry key

**Collision policy:**
```cpp
enum class CollisionPolicy {
    Error,        // Reject load if duplicate ID
    FirstWins,    // Keep first loaded
    LastWins,     // Override with later load (higher layer wins)
    Merge         // Attempt to merge (for compatible types)
};

// Configurable per registry type
registry.set_collision_policy(CollisionPolicy::LastWins);
```

### 17.4 Reserved Names and Patterns

| Pattern | Reserved For |
|---------|--------------|
| `core.*` | Engine-provided packages only |
| `engine.*` | Engine-provided packages only |
| `_internal.*` | Internal use, not for creators |
| `test.*` | Testing only, stripped in release |

---

## 18. Versioning & Compatibility

### 18.1 Semantic Versioning

All packages use **SemVer**: `MAJOR.MINOR.PATCH`

| Component | Meaning |
|-----------|---------|
| MAJOR | Breaking changes to schema or behaviour |
| MINOR | Backwards-compatible additions |
| PATCH | Bugfixes, no new features or fields |

**Examples:**
- `1.0.0` → `1.0.1`: Bugfix (safe to upgrade)
- `1.0.0` → `1.1.0`: New optional field added (safe)
- `1.0.0` → `2.0.0`: Required field removed (breaking)

### 18.2 Version Constraints

`PackageDependency.version_constraint` supports:

| Constraint | Meaning |
|------------|---------|
| `"1.2.3"` | Exact version |
| `">=1.0.0"` | Minimum version |
| `"<=2.0.0"` | Maximum version |
| `">=1.0.0,<2.0.0"` | Range |
| `"^1.2"` | Compatible with 1.2.x (>=1.2.0, <2.0.0) |
| `"~1.2"` | Approximately 1.2.x (>=1.2.0, <1.3.0) |

**Resolution algorithm:**
- `PackageResolver` selects the **highest** version satisfying all constraints
- Fails if no version satisfies all dependents

### 18.3 Engine Compatibility

Each manifest may declare engine version requirements:

```json
{
  "package": {
    "name": "mod.my_weapon",
    "type": "plugin",
    "version": "1.0.0"
  },
  "engine": {
    "min": "1.0.0",
    "max": "1.x"
  }
}
```

**On load, `PackageRegistry` checks:**
- If current engine version is outside range → `PackageStatus::Failed`
- Clear error message: "Package requires engine 1.0.0-1.x, current is 2.0.0"

### 18.4 Data Migration Strategy

When breaking changes to data schemas are required:

**Preferred approach: Additive changes**
- Add new fields with defaults
- Keep old fields as deprecated (optional)
- Systems handle both old and new formats

**When removal is unavoidable:**

1. **Offline migration tools:**
   ```bash
   voidpkg migrate --from 1.x --to 2.x <package_dir>
   ```

2. **Plugin-provided migration hooks:**
   ```cpp
   // In plugin library
   extern "C" void migrate_v1_to_v2(DefinitionRegistry& registry) {
       // Transform old weapon definitions to new format
   }
   ```

3. **Versioned schemas:**
   ```json
   {
     "schema_version": 2,
     "components": [...]
   }
   ```
   Loader detects version and applies appropriate parser.

---

## 19. Security & Sandboxing

This section defines trust levels and runtime restrictions for packages, especially for creator-authored content.

### 19.1 Trust Levels

| Level | Namespaces | Privileges |
|-------|------------|------------|
| **Trusted** | `core.*`, `engine.*` | Native code, full engine API |
| **Curated** | `gameplay.*`, `feature.*` | Native or scripted, limited external access |
| **Untrusted** | `mod.*` | Scripted only, strict sandbox |

### 19.2 Native vs Scripted Plugins

**Native plugins (`library` field points to DLL/SO):**
- Only allowed for Trusted and Curated levels
- Full performance, full API access
- Must be code-reviewed before distribution

**Scripted plugins (Lua/WASM/other VM):**
- Required for Untrusted mods
- Sandboxed execution environment
- Limited API surface

### 19.3 Script Runtime Constraints

For script-based `plugin.package`:

**Allowed APIs (whitelist):**
```cpp
// Exposed to scripts
ecs_query(...)        // Query ECS world
ecs_get(entity, component)
ecs_set(entity, component, value)
ecs_spawn(prefab_id)
ecs_despawn(entity)
event_emit(event_type, data)
log_info/warn/error(message)
```

**Blocked APIs:**
- File system access
- Network access
- OS/process operations
- Raw memory access

**Resource limits:**
| Limit | Default | Configurable |
|-------|---------|--------------|
| Max execution time per tick | 5 ms | Yes |
| Max memory per script | 16 MB | Yes |
| Max instructions per tick | 100,000 | Yes |
| Max spawned entities per tick | 100 | Yes |

### 19.4 Multiplayer / Authority Rules

**Server authority:**
- Server is authoritative for which packages are loaded
- Server broadcasts required package list + versions to clients
- Clients must load matching packages or disconnect

**Client restrictions:**
- Clients cannot load gameplay `plugin.package` unknown to server
- Clients may load client-only `widget.package` (UI mods)
- All gameplay state from untrusted packages validated server-side

**Package negotiation flow:**
```
1. Client connects
2. Server sends: { required_packages: [...], optional_packages: [...] }
3. Client checks local availability
4. Client downloads missing packages (if allowed)
5. Client loads packages
6. Client confirms readiness
7. Server allows full join
```

### 19.5 Content Signing (Future)

For workshop/distribution:
- Packages can be signed with publisher key
- Engine verifies signature before loading
- Trust levels can be based on signature:
  - Official signature → Trusted
  - Verified publisher → Curated
  - Unsigned → Untrusted

---

## 20. Distribution & Deployment

This section defines how packages are stored, discovered, and distributed.

### 20.1 On-Disk Layout

Recommended structure:

```
<game_root>/
├── engine/                    # Engine binaries
├── content/                   # Content root
│   └── packages/
│       ├── worlds/
│       │   └── arena_dm.world.json
│       ├── layers/
│       │   └── night_mode.layer.json
│       ├── plugins/
│       │   ├── core/
│       │   │   └── core_ecs.plugin.json
│       │   └── gameplay/
│       │       └── combat.plugin.json
│       ├── widgets/
│       │   └── debug_hud.widget.json
│       └── assets/
│           └── characters.bundle.json
├── mods/                      # User mods (separate from base content)
│   └── packages/
│       └── ...
└── cache/                     # Downloaded/cached packages
```

**Scan order:**
1. `content/packages/` (base game)
2. `mods/packages/` (user mods)
3. `cache/` (downloaded from servers)

### 20.2 Packaging Formats

**Loose directory + manifest (development):**
- Good for development builds and hot-reload
- Manifest at: `packages/<type>/<name>.<type>.json`
- Assets alongside in subdirectories

**Packed archive (distribution):**
- Single file (`.vpk` = void package, or `.zip`)
- Contains manifest + all referenced assets
- Used for DLC, workshop uploads, server distribution

```bash
# Create packed archive
voidpkg pack packages/weapons/plasma/ -o plasma_weapon.vpk

# Extract for inspection
voidpkg unpack plasma_weapon.vpk -o extracted/
```

**`PackageRegistry` supports both:**
```cpp
registry.scan_directory("content/packages/");  // Loose
registry.load_archive("mods/weapon.vpk");      // Packed
```

### 20.3 Content Hashing & Caching

**Content hash:**
- Each package instance has SHA-256 hash of (manifest + all files)
- Used for cache invalidation and integrity verification

**Client caching:**
- Cache key: `(package_name, version, content_hash)`
- Cached packages stored in `cache/` directory
- Engine checks cache before downloading

**Server announcement:**
```json
{
  "required_packages": [
    {
      "name": "gameplay.combat",
      "version": "1.2.0",
      "hash": "sha256:abc123...",
      "download_url": "https://..."  // Optional
    }
  ]
}
```

### 20.4 Download Sources

Packages can be obtained from (in priority order):
1. Local install (`content/packages/`)
2. DLC folders (platform SDK)
3. Workshop (Steam, etc.)
4. Direct from game server (if enabled)
5. CDN/HTTP (for large mods)

**Security:**
- Downloads must verify content hash
- Untrusted sources require signature verification
- Server can reject clients with tampered packages

### 20.5 Dedicated Server Mod Management

**Server-side modlist:**
```json
// server_mods.json
{
  "mods": [
    { "name": "mod.custom_weapons", "version": ">=1.0.0", "required": true },
    { "name": "mod.cosmetics", "version": ">=1.0.0", "required": false }
  ],
  "allow_client_widgets": true,
  "allow_unsigned_mods": false
}
```

**Admin operations:**
- Hot-add mod: `server.load_mod("mod.new_content")`
- Remove mod: `server.unload_mod("mod.old_content")` (may require map change)
- List mods: `server.list_loaded_mods()`

---

## 21. Testing Strategy

### 21.1 Unit Tests

| Test Suite | Coverage |
|------------|----------|
| `PackageManifestTests` | JSON parsing, validation |
| `PackageResolverTests` | Dependency resolution, cycle detection |
| `VersionTests` | SemVer parsing, comparison |
| `AssetBundleLoaderTests` | Asset loading, prefab registration |
| `PluginPackageLoaderTests` | Component/system registration |
| `LayerPackageLoaderTests` | Layer staging/application |
| `WorldPackageLoaderTests` | Full load sequence |

### 21.2 Integration Tests

```cpp
TEST_CASE("Full package load sequence") {
    PackageRegistry registry;
    LoadContext ctx;

    // Scan test packages
    registry.scan_directory("test_packages/");

    // Load world with all dependencies
    auto result = registry.load_package("test_world", ctx);
    REQUIRE(result.is_ok());

    // Verify components registered
    auto* world = ctx.ecs_world();
    REQUIRE(world->has_component_type<Health>());

    // Verify systems running
    world->scheduler().run_stage(*world, SystemStage::Update);

    // Verify prefabs instantiable
    auto entity = ctx.prefab_registry()->instantiate("test_prefab", *world);
    REQUIRE(entity.is_ok());
}
```

### 21.3 Migration Regression Tests

For each existing scene/plugin:
1. Load with legacy system → capture state
2. Migrate to package format
3. Load with new system → capture state
4. Compare states (should be identical)

---

## 22. Rollback Plan

### 22.1 Phase Rollback Points

Each phase has a clean rollback:

| Phase | Rollback Strategy |
|-------|-------------------|
| 1 | Delete `void_package` module (no other code affected) |
| 2 | Revert `AssetServer` changes, delete bundle support |
| 3 | Revert plugin registration, keep legacy `Plugin` class |
| 4 | Delete widget package support (widgets still work directly) |
| 5 | Delete layer system (scenes work without layers) |
| 6 | Revert `Runtime` changes, use legacy scene loading |

### 22.2 Feature Flags

```cpp
// config.hpp
namespace void_config {
    // Set to false to use legacy loading
    constexpr bool USE_PACKAGE_SYSTEM = true;

    // Individual feature flags
    constexpr bool USE_ASSET_BUNDLES = true;
    constexpr bool USE_PLUGIN_PACKAGES = true;
    constexpr bool USE_LAYER_PACKAGES = true;
    constexpr bool USE_WIDGET_PACKAGES = true;
}
```

---

## 23. Implementation Checklist

This is the condensed "are we done yet?" checklist covering all aspects of the package system implementation.

### 23.1 Core Package Infrastructure

- [ ] `void_package` module created (headers + src + CMake target)
- [ ] `PackageManifest` parsing and validation implemented
- [ ] `Version` (SemVer) parsing/comparison implemented
- [ ] `PackageResolver` implemented:
  - [ ] Acyclic dependency check
  - [ ] Plugin layer validation (core/engine/gameplay/feature/mod)
  - [ ] Error messages with dependency chain info
- [ ] `PackageLoader` interface + `LoadContext`
- [ ] `PackageRegistry` implemented (scan/load/unload/reload/query)
- [ ] Unit tests for all core components

### 23.2 Asset Bundles

- [ ] `AssetBundleManifest` implemented (meshes/textures/materials/prefabs/definitions)
- [ ] `AssetBundleLoader` implemented and registered
- [ ] `PrefabRegistry` implemented and integrated with `SceneInstantiator`
- [ ] `DefinitionRegistry` implemented and plumbed into gameplay plugins
- [ ] `AssetServer` understands bundle-relative paths
- [ ] Migration path from loose assets to bundles (script or tool)
- [ ] Integration tests for asset loading

### 23.3 Plugins

- [ ] `PluginPackageManifest` implemented (components/systems/handlers/registries)
- [ ] `ComponentSchemaRegistry` implemented
- [ ] `PluginPackageLoader` implemented:
  - [ ] Component registration via ECS registry
  - [ ] System registration into scheduler using manifest info
  - [ ] Event handler subscription
  - [ ] Registry initialization from `DefinitionRegistry`
- [ ] Dynamic library loading (or script runtime) wired into entry points
- [ ] At least one existing plugin migrated to manifest-driven registration
- [ ] Integration tests for plugin loading

### 23.4 Widgets

- [ ] `WidgetPackageManifest` implemented (widgets + bindings)
- [ ] `WidgetManager` (or equivalent) for UI lifecycle
- [ ] `WidgetPackageLoader` implemented and registered
- [ ] ECS query/resource bindings for widgets
- [ ] A few existing debug tools migrated to widget packages
- [ ] Build-type filtering (debug/development/release)

### 23.5 Layers

- [ ] `LayerPackageManifest` implemented (additive scenes, spawners, lighting, modifiers)
- [ ] `LayerPackageLoader` implemented (stage/apply/unapply)
- [ ] World runtime supports:
  - [ ] Activating/deactivating layers at runtime
  - [ ] Tracking entities/resources modified by a layer for clean rollback
- [ ] Integration tests for layer application/removal

### 23.6 World Packages

- [ ] `WorldPackageManifest` implemented (root scene, plugins, layers, widgets, asset bundles, spawn config, environment, gameplay, resources)
- [ ] `WorldPackageLoader` implements full load sequence
- [ ] `Runtime`/kernel uses `PackageRegistry` to load a world instead of legacy `scene.json`
- [ ] Compatibility layer for legacy scenes implemented
- [ ] World switching support implemented

### 23.7 Runtime Integration & Operations

- [ ] World boot sequence implemented end-to-end using package system
- [ ] Hot-reload via `PackageRegistry::reload_package` implemented for:
  - [ ] asset.bundle
  - [ ] plugin.package (where feasible)
  - [ ] widget.package
  - [ ] layer.package
- [ ] Layer toggling is stable in long-running sessions
- [ ] Error handling surfaces clear messages for:
  - [ ] Manifest parse/validation issues
  - [ ] Missing dependencies
  - [ ] Plugin layer violations
  - [ ] Version constraint failures

### 23.8 Tooling

- [ ] `voidpkg validate` implemented
- [ ] `voidpkg pack` implemented (or equivalent)
- [ ] `voidpkg list-deps` implemented
- [ ] `voidpkg migrate-scene` implemented (legacy conversion)
- [ ] Editor integration:
  - [ ] Create/edit all package types
  - [ ] Launch worlds via `world.package`
  - [ ] Show loaded package/dependency info
  - [ ] Visualize dependency graph

### 23.9 Security & Networking

- [ ] Trust levels defined for package namespaces (core/engine/gameplay/feature/mod)
- [ ] If script runtime is used:
  - [ ] Sandbox & allowed APIs defined and enforced
  - [ ] Resource limits (time/memory) enforced
- [ ] Server authoritative mod list implemented
- [ ] Client correctly refuses to load unknown gameplay plugins in multiplayer
- [ ] Package negotiation flow implemented

### 23.10 Distribution

- [ ] Loose directory loading supported
- [ ] Packed archive (.vpk) loading supported
- [ ] Content hashing implemented
- [ ] Client caching implemented
- [ ] Server mod announcement to clients implemented

### 23.11 Documentation & Quality

- [ ] JSON schema files for all package types
- [ ] API documentation for package system
- [ ] Creator guide for mod authoring
- [ ] Migration guide for existing content
- [ ] Performance benchmarks (load times, memory)
- [ ] All example projects updated to use packages

### 23.12 Post-Migration Cleanup

- [ ] Legacy scene loading code removed (after stabilization period)
- [ ] Deprecated APIs marked and documented
- [ ] Migration tools tested with real-world content
- [ ] Community feedback incorporated

---

## Appendix A: JSON Schema Examples

### A.1 Minimal World Package

```json
{
  "package": {
    "name": "example.minimal_world",
    "type": "world",
    "version": "1.0.0"
  },
  "dependencies": {
    "plugins": [
      { "name": "core.ecs", "version": ">=1.0.0" }
    ],
    "assets": [
      { "name": "example.basic_assets", "version": ">=1.0.0" }
    ]
  },
  "root_scene": {
    "path": "scenes/main.scene.json"
  },
  "environment": {
    "time_of_day": 12.0,
    "skybox": "skyboxes/default"
  }
}
```

### A.2 Minimal Plugin Package

```json
{
  "package": {
    "name": "gameplay.health",
    "type": "plugin",
    "version": "1.0.0"
  },
  "dependencies": {
    "plugins": [
      { "name": "core.ecs", "version": ">=1.0.0" }
    ]
  },
  "components": [
    {
      "name": "Health",
      "fields": {
        "current": { "type": "f32", "default": 100 },
        "max": { "type": "f32", "default": 100 }
      }
    }
  ],
  "tags": ["Damageable"],
  "systems": [
    {
      "name": "HealthRegenSystem",
      "stage": "update",
      "query": ["Health"],
      "library": "gameplay_health.dll"
    }
  ]
}
```

### A.3 Minimal Asset Bundle

```json
{
  "package": {
    "name": "example.basic_assets",
    "type": "asset",
    "version": "1.0.0"
  },
  "meshes": [
    { "id": "cube", "path": "models/cube.gltf" }
  ],
  "materials": [
    {
      "id": "default_material",
      "shader": "shaders/pbr_standard",
      "parameters": {
        "albedo": [1, 1, 1, 1],
        "metallic": 0.0,
        "roughness": 0.5
      }
    }
  ],
  "prefabs": [
    {
      "id": "basic_cube",
      "components": {
        "Transform": {},
        "MeshRenderer": { "mesh": "cube", "material": "default_material" }
      }
    }
  ]
}
```

---

*This migration guide (v1.1) provides a complete roadmap for transitioning void_engine to the new Package System. It covers architecture, implementation phases, operational integration, tooling, security, and distribution - everything needed for both implementation and onboarding other teams or external creators.*
