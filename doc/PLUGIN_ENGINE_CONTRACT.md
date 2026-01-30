# Plugin-Engine Contract & State Management

## Core Philosophy

This engine exists for **one reason**: to enable external creators to build complete game experiences through remotely-loaded plugins. Everything flows from this principle.

The engine provides the **renderer** and **infrastructure**. Plugins provide **everything else**.

---

## CRITICAL: Module Architecture (READ FIRST)

> **This section is NON-NEGOTIABLE. Misunderstanding this architecture causes bugs.**

### The Three Layers

```
┌─────────────────────────────────────────────────────────────────────────┐
│  void_ecs (include/void_engine/ecs/)                                     │
│                                                                          │
│  The CORE ECS implementation:                                            │
│    - World, Entity, Component, Archetype, Query                          │
│    - ComponentRegistry (low-level type registration)                     │
│    - Resources, Systems, Snapshots                                       │
│                                                                          │
│  This is the LOW-LEVEL ECS engine. It knows nothing about JSON,          │
│  packages, or plugins. It just stores and queries component data.        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ USES
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  void_package (include/void_engine/package/)                             │
│                                                                          │
│  The PACKAGE SYSTEM that uses void_ecs:                                  │
│    - ComponentSchemaRegistry (JSON → component conversion)               │
│    - PrefabRegistry (prefab definitions)                                 │
│    - WorldComposer (world loading and entity spawning)                   │
│    - LayerApplier (layer patches)                                        │
│    - Package loaders (plugin, world, layer, widget, asset)               │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  ComponentSchemaRegistry IS THE CANONICAL WAY TO REGISTER        │    │
│  │  COMPONENTS FOR JSON→COMPONENT CONVERSION.                       │    │
│  │                                                                  │    │
│  │  ALL component registration MUST go through this registry.       │    │
│  │  DO NOT create parallel factory systems.                         │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ USES
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  void_plugin_api (include/void_engine/plugin_api/)                       │
│                                                                          │
│  The PLUGIN API that uses void_package and void_ecs:                     │
│    - IPlugin interface (what plugins implement)                          │
│    - PluginContext (what engine provides to plugins)                     │
│    - RenderableDesc (render contract)                                    │
│                                                                          │
│  PluginContext.register_component<T>() delegates to                      │
│  void_package::ComponentSchemaRegistry, NOT a parallel system.           │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Component Registration Flow

```
Plugin calls:
  ctx.register_component<Health>("Health", applier)
        │
        │ delegates to
        ▼
  void_package::ComponentSchemaRegistry::register_schema_with_factory()
        │
        │ which uses
        ▼
  void_ecs::World::register_component<T>()
        │
        │ returns
        ▼
  void_ecs::ComponentId
```

### NEVER DO THIS

```cpp
// WRONG: Creating parallel factory systems
class PluginContext {
    std::unordered_map<std::string, ComponentFactory> m_factories;  // NO!
};

// WRONG: Bypassing ComponentSchemaRegistry
void some_loader() {
    world.register_component<MyComp>();  // Direct registration without schema
}
```

### ALWAYS DO THIS

```cpp
// CORRECT: Use ComponentSchemaRegistry
class PluginContext {
    void_package::ComponentSchemaRegistry* m_schema_registry;  // YES!

    template<typename T>
    ComponentId register_component(const std::string& name, ComponentApplier applier) {
        // Register with schema registry (the single source of truth)
        m_schema_registry->register_schema_with_factory(schema, nullptr, applier);
    }
};
```

### Why This Matters

1. **Single Source of Truth**: ComponentSchemaRegistry is where ALL component schemas live
2. **Prefab Loading**: WorldComposer uses ComponentSchemaRegistry to instantiate prefabs
3. **Hot-Reload**: Schema registry tracks what components exist for migration
4. **Plugin Isolation**: Registry tracks which plugin registered which component
5. **JSON Validation**: Schema registry validates JSON against component schemas

**If you create parallel systems, prefabs won't load, hot-reload breaks, and plugins collide.**

---

## Table of Contents

1. [The Contract Layer](#1-the-contract-layer)
2. [Plugin State Management (True ECS)](#2-plugin-state-management-true-ecs)
3. [Engine-Plugin Render Contract](#3-engine-plugin-render-contract)
4. [Base Plugin Library](#4-base-plugin-library)
5. [Plugin Lifecycle & Hot-Reload](#5-plugin-lifecycle--hot-reload)
6. [Current Gaps & Required Wiring](#6-current-gaps--required-wiring)

---

## 1. The Contract Layer

### The Separation

```
┌─────────────────────────────────────────────────────────────────┐
│                         ENGINE                                   │
│                                                                  │
│  Owns:                                                           │
│    - ECS World (the container)                                   │
│    - Component Registry (type registration)                      │
│    - Kernel (stage scheduler)                                    │
│    - Renderer (GPU, shaders, draw calls)                        │
│    - Asset System (loading, caching)                             │
│    - Hot-Reload Orchestrator                                     │
│                                                                  │
│  Provides Contract:                                              │
│    - IPlugin interface                                           │
│    - Component registration API                                  │
│    - System registration API                                     │
│    - Render component schemas (Transform, Mesh, Material, etc.) │
│    - Render system execution (TransformSystem, RenderSystem)    │
│                                                                  │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                    ═══════════╪═══════════  CONTRACT BOUNDARY
                               │
┌──────────────────────────────┴──────────────────────────────────┐
│                         PLUGINS                                  │
│                                                                  │
│  Owns:                                                           │
│    - Custom component definitions (structs)                      │
│    - Custom system implementations (logic)                       │
│    - Game-specific state                                         │
│    - Event handlers                                              │
│                                                                  │
│  Must Implement:                                                 │
│    - IPlugin interface                                           │
│    - Component factories (JSON → Component)                      │
│    - System functions (World&, float dt)                        │
│    - State snapshot/restore (for hot-reload)                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### IPlugin Interface (The Contract)

```cpp
/// The contract every plugin must implement
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /// Unique plugin identifier (e.g., "core.gameplay")
    virtual const char* id() const = 0;

    /// Semantic version
    virtual Version version() const = 0;

    /// Dependencies (other plugins this requires)
    virtual std::span<const Dependency> dependencies() const = 0;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Called when plugin is loaded
    /// Register components, systems, event handlers here
    virtual Result<void> on_load(PluginContext& ctx) = 0;

    /// Called when plugin is about to be unloaded
    /// Cleanup resources, unregister handlers
    virtual Result<void> on_unload(PluginContext& ctx) = 0;

    /// Called every frame if plugin requested tick
    virtual void on_tick(float dt) {}

    // =========================================================================
    // State Management (Hot-Reload)
    // =========================================================================

    /// Capture plugin state before reload
    virtual PluginSnapshot snapshot() = 0;

    /// Restore plugin state after reload
    virtual Result<void> restore(const PluginSnapshot& snapshot) = 0;

    /// Called after successful reload
    virtual void on_reloaded() {}

    // =========================================================================
    // Introspection
    // =========================================================================

    /// Get registered component names
    virtual std::span<const char*> component_names() const = 0;

    /// Get registered system names
    virtual std::span<const char*> system_names() const = 0;
};
```

### PluginContext (What Engine Provides)

```cpp
/// Context passed to plugins during lifecycle
class PluginContext {
public:
    // =========================================================================
    // Registration APIs
    // =========================================================================

    /// Register a component type with factory
    template<typename T>
    ComponentId register_component(
        const char* name,
        ComponentFactory<T> factory);

    /// Register a system to run at a specific stage
    SystemId register_system(
        Stage stage,
        const char* name,
        SystemFunc func,
        int priority = 0);

    /// Subscribe to an event type
    template<typename E>
    SubscriptionId subscribe(EventHandler<E> handler);

    // =========================================================================
    // ECS Access
    // =========================================================================

    /// Get the ECS world (read-only queries)
    const World& world() const;

    /// Get the ECS world (mutations)
    World& world_mut();

    /// Get a resource
    template<typename R>
    const R* resource() const;

    /// Get a mutable resource
    template<typename R>
    R* resource_mut();

    // =========================================================================
    // Engine Services
    // =========================================================================

    /// Get the event bus
    EventBus& events();

    /// Get the asset server
    AssetServer& assets();

    /// Get the kernel (for advanced system management)
    Kernel& kernel();

    // =========================================================================
    // Render Contract
    // =========================================================================

    /// Access render component IDs (engine-owned)
    const RenderComponentIds& render_components() const;

    /// Check if an entity is renderable
    bool is_renderable(Entity e) const;

    /// Make an entity renderable (adds Mesh, Material, Renderable)
    Result<void> make_renderable(Entity e, const RenderableDesc& desc);
};
```

---

## 2. Plugin State Management (True ECS)

### The Problem

In true ECS, **everything is data**. Plugins are not exceptions.

A plugin has:
- **Registration state**: What components/systems it registered
- **Runtime state**: Any internal data structures, caches, counters
- **Entity ownership**: Entities the plugin created/manages

All of this must survive hot-reload.

### Plugin as ECS Citizen

```cpp
/// Plugin state stored as ECS resource
struct PluginState {
    std::string id;
    Version version;
    PluginStatus status;  // Loading, Active, Unloading, Failed

    // What this plugin registered
    std::vector<ComponentId> registered_components;
    std::vector<SystemId> registered_systems;
    std::vector<SubscriptionId> subscriptions;

    // Entities this plugin owns
    std::vector<Entity> owned_entities;

    // Plugin's custom state (opaque blob for hot-reload)
    std::vector<std::uint8_t> custom_state;

    // Timestamps
    std::chrono::steady_clock::time_point loaded_at;
    std::chrono::steady_clock::time_point last_reloaded_at;
};

/// All plugins tracked as ECS resource
struct PluginRegistry {
    std::unordered_map<std::string, PluginState> plugins;
    std::vector<std::string> load_order;  // Dependency-sorted
};
```

### State Flow During Hot-Reload

```
┌─────────────────────────────────────────────────────────────────┐
│  HOT-RELOAD TRIGGERED (plugin.dll modified)                      │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  1. SNAPSHOT PHASE                                               │
│                                                                  │
│  For each affected plugin (in dependency order):                 │
│    - Call plugin.snapshot()                                      │
│    - Store PluginState.custom_state                              │
│    - Store PluginState.owned_entities                            │
│    - Store component data for owned entities                     │
│                                                                  │
│  ECS World state captured via world.snapshot()                   │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  2. UNLOAD PHASE                                                 │
│                                                                  │
│  For each affected plugin (reverse dependency order):            │
│    - Call plugin.on_unload()                                     │
│    - Unregister systems from kernel                              │
│    - Unregister event subscriptions                              │
│    - Keep component registrations (data survives)                │
│    - Unload DLL                                                  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  3. RELOAD PHASE                                                 │
│                                                                  │
│  For each affected plugin (dependency order):                    │
│    - Load new DLL                                                │
│    - Call plugin.on_load()                                       │
│    - Re-register systems (may have new logic)                    │
│    - Re-register event handlers                                  │
│    - Component registrations updated if schema changed           │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  4. RESTORE PHASE                                                │
│                                                                  │
│  For each affected plugin (dependency order):                    │
│    - Call plugin.restore(snapshot)                               │
│    - Plugin restores internal state                              │
│    - Component data already in ECS (survived reload)             │
│    - Call plugin.on_reloaded()                                   │
│                                                                  │
│  ECS World restored via world.restore()                          │
└─────────────────────────────────────────────────────────────────┘
```

### Entity Ownership

Plugins must track what they create:

```cpp
class MyPlugin : public IPlugin {
    std::vector<Entity> m_spawned_enemies;

    Result<void> on_load(PluginContext& ctx) override {
        // Spawn some enemies
        for (int i = 0; i < 10; i++) {
            Entity e = ctx.world_mut().spawn();
            ctx.world_mut().add_component(e, Transform{...});
            ctx.world_mut().add_component(e, Health{100, 100});
            ctx.world_mut().add_component(e, Enemy{});

            // Make it renderable (uses engine contract)
            ctx.make_renderable(e, {
                .mesh = "sphere",
                .material = { .albedo = {1, 0, 0, 1} }
            });

            m_spawned_enemies.push_back(e);
        }
        return Ok();
    }

    PluginSnapshot snapshot() override {
        PluginSnapshot snap;
        // Serialize enemy entity IDs
        snap.data = serialize(m_spawned_enemies);
        return snap;
    }

    Result<void> restore(const PluginSnapshot& snap) override {
        // Restore enemy entity references
        m_spawned_enemies = deserialize<std::vector<Entity>>(snap.data);
        return Ok();
    }
};
```

---

## 3. Engine-Plugin Render Contract

### The Problem

Plugins define game components (Health, Weapon, Enemy).
Engine owns render components (Mesh, Material, Transform).

How do they connect?

### The Contract: RenderableDesc

```cpp
/// Description of how an entity should render
/// Plugins use this to request rendering without knowing GPU details
struct RenderableDesc {
    // Mesh specification
    std::string mesh_builtin;        // "sphere", "cube", "plane", etc.
    std::string mesh_asset;          // Path to glTF/GLB model

    // Material specification
    struct MaterialSpec {
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

    // Animation (optional)
    std::optional<AnimationSpec> animation;
};
```

### How Plugins Use It

```cpp
// Plugin wants to make an entity visible
Entity enemy = world.spawn();

// Add game components (plugin-defined)
world.add_component(enemy, Transform{.position = {0, 1, 0}});
world.add_component(enemy, Health{100, 100});
world.add_component(enemy, Enemy{.type = EnemyType::Grunt});

// Request rendering (engine contract)
ctx.make_renderable(enemy, {
    .mesh_builtin = "sphere",
    .material = {
        .albedo = {1.0f, 0.2f, 0.2f, 1.0f},  // Red
        .metallic = 0.0f,
        .roughness = 0.7f
    },
    .visible = true
});
```

### What Engine Does

```cpp
Result<void> PluginContext::make_renderable(Entity e, const RenderableDesc& desc) {
    auto& world = world_mut();

    // Add engine render components
    if (!desc.mesh_builtin.empty()) {
        world.add_component(e, MeshComponent::builtin(desc.mesh_builtin));
    } else if (!desc.mesh_asset.empty()) {
        world.add_component(e, ModelComponent::from_path(desc.mesh_asset));
    }

    world.add_component(e, MaterialComponent{
        .albedo = desc.material.albedo,
        .metallic_value = desc.material.metallic,
        .roughness_value = desc.material.roughness
    });

    world.add_component(e, RenderableTag{
        .visible = desc.visible,
        .layer_mask = desc.layer_mask,
        .render_order = desc.render_order
    });

    return Ok();
}
```

### The Component Bridge

```
┌─────────────────────────────────────────────────────────────────┐
│                        ENTITY                                    │
│                                                                  │
│  Plugin Components (plugin owns):                                │
│    ├── Transform { position, rotation, scale }                   │
│    ├── Health { current, max }                                   │
│    └── Enemy { type, ai_state }                                  │
│                                                                  │
│  Render Components (engine owns, plugin requested):              │
│    ├── MeshComponent { builtin: "sphere" }                       │
│    ├── MaterialComponent { albedo, metallic, roughness }         │
│    └── RenderableTag { visible, layer_mask }                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               │ Engine queries entities with
                               │ RenderableTag + Transform + Mesh + Material
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ENGINE RENDER SYSTEMS                         │
│                                                                  │
│  TransformSystem → Updates world matrices                        │
│  RenderPrepareSystem → Builds draw commands                      │
│  RenderSystem → Executes GPU draws                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Transform: Shared or Separate?

**Question**: Should plugins use the same Transform as the render system?

**Option A: Shared Transform (Recommended)**
- Engine provides `TransformComponent`
- Plugins use the same component
- One source of truth for position/rotation/scale
- Render systems automatically see changes

```cpp
// Engine registers Transform
schema_registry.register_builtin<TransformComponent>("Transform");

// Plugin uses same Transform
world.add_component(e, TransformComponent{...});

// Render system queries it directly
for (auto [e, transform, mesh] : world.query<TransformComponent, MeshComponent>()) {
    draw(transform.world_matrix, mesh);
}
```

**Option B: Separate Transforms**
- Plugin has GameTransform
- Engine has RenderTransform
- Sync system copies GameTransform → RenderTransform
- More complexity, allows different update rates

**Recommendation**: Shared Transform. It's simpler and ECS-idiomatic.

---

## 4. Base Plugin Library

### Purpose

Engine ships with **base plugins** that:
1. Demonstrate how to write plugins
2. Provide common functionality creators can use
3. Establish patterns and conventions
4. Can be overridden or extended

### Proposed Base Plugins

```
engine/plugins/
├── core.transform/           # Shared transform component & system
│   ├── manifest.plugin.json
│   └── src/
│       └── transform_plugin.cpp
│
├── core.hierarchy/           # Parent-child relationships
│   ├── manifest.plugin.json
│   └── src/
│       └── hierarchy_plugin.cpp
│
├── base.health/              # Health, damage, death
│   ├── manifest.plugin.json
│   └── src/
│       └── health_plugin.cpp
│
├── base.physics/             # Rigidbody, collision
│   ├── manifest.plugin.json
│   └── src/
│       └── physics_plugin.cpp
│
├── base.ai/                  # Basic AI, state machines
│   ├── manifest.plugin.json
│   └── src/
│       └── ai_plugin.cpp
│
├── base.inventory/           # Item management
│   ├── manifest.plugin.json
│   └── src/
│       └── inventory_plugin.cpp
│
└── base.combat/              # Weapons, damage dealing
    ├── manifest.plugin.json
    └── src/
        └── combat_plugin.cpp
```

### Example: base.health Plugin

**manifest.plugin.json:**
```json
{
  "name": "base.health",
  "version": "1.0.0",
  "type": "plugin",
  "tier": "engine",
  "description": "Health, damage, and death handling",

  "library": {
    "windows": "bin/base.health.dll",
    "linux": "bin/libbase.health.so",
    "macos": "bin/libbase.health.dylib"
  },

  "entry_point": "plugin_create",

  "components": [
    {
      "name": "Health",
      "fields": [
        { "name": "current", "type": "f32", "default": 100.0 },
        { "name": "max", "type": "f32", "default": 100.0 },
        { "name": "regeneration", "type": "f32", "default": 0.0 },
        { "name": "invulnerable", "type": "bool", "default": false }
      ]
    },
    {
      "name": "DamageReceiver",
      "fields": [
        { "name": "armor", "type": "f32", "default": 0.0 },
        { "name": "damage_multiplier", "type": "f32", "default": 1.0 }
      ]
    },
    {
      "name": "Dead",
      "fields": [],
      "description": "Tag component for dead entities"
    }
  ],

  "systems": [
    {
      "name": "HealthRegenSystem",
      "stage": "Update",
      "priority": 100,
      "description": "Regenerates health over time"
    },
    {
      "name": "DeathSystem",
      "stage": "PostFixed",
      "priority": 200,
      "description": "Handles entity death when health <= 0"
    }
  ],

  "events": [
    { "name": "DamageEvent", "fields": ["entity", "amount", "source"] },
    { "name": "DeathEvent", "fields": ["entity", "killer"] },
    { "name": "HealEvent", "fields": ["entity", "amount"] }
  ]
}
```

**health_plugin.cpp:**
```cpp
#include <void_engine/plugin_api/plugin.hpp>

using namespace void_plugin;

// Component definitions
struct Health {
    float current = 100.0f;
    float max = 100.0f;
    float regeneration = 0.0f;
    bool invulnerable = false;
};

struct DamageReceiver {
    float armor = 0.0f;
    float damage_multiplier = 1.0f;
};

struct Dead {};

// Events
struct DamageEvent {
    Entity entity;
    float amount;
    Entity source;
};

struct DeathEvent {
    Entity entity;
    Entity killer;
};

// Systems
void health_regen_system(World& world, float dt) {
    for (auto [e, health] : world.query<Health>()) {
        if (health.current < health.max && health.regeneration > 0) {
            health.current = std::min(health.current + health.regeneration * dt, health.max);
        }
    }
}

void death_system(World& world, EventBus& events) {
    for (auto [e, health] : world.query<Health>()) {
        if (health.current <= 0 && !world.has_component<Dead>(e)) {
            world.add_component(e, Dead{});
            events.publish(DeathEvent{e, Entity{}});
        }
    }
}

// Plugin implementation
class HealthPlugin : public IPlugin {
public:
    const char* id() const override { return "base.health"; }
    Version version() const override { return {1, 0, 0}; }

    Result<void> on_load(PluginContext& ctx) override {
        // Register components with factories
        ctx.register_component<Health>("Health", [](const json& j) {
            return Health{
                .current = j.value("current", 100.0f),
                .max = j.value("max", 100.0f),
                .regeneration = j.value("regeneration", 0.0f),
                .invulnerable = j.value("invulnerable", false)
            };
        });

        ctx.register_component<DamageReceiver>("DamageReceiver", [](const json& j) {
            return DamageReceiver{
                .armor = j.value("armor", 0.0f),
                .damage_multiplier = j.value("damage_multiplier", 1.0f)
            };
        });

        ctx.register_component<Dead>("Dead", [](const json&) {
            return Dead{};
        });

        // Register systems
        m_regen_system = ctx.register_system(Stage::Update, "HealthRegenSystem",
            [&ctx](float dt) { health_regen_system(ctx.world_mut(), dt); },
            100);

        m_death_system = ctx.register_system(Stage::PostFixed, "DeathSystem",
            [&ctx](float) { death_system(ctx.world_mut(), ctx.events()); },
            200);

        // Subscribe to damage events
        m_damage_sub = ctx.subscribe<DamageEvent>([this, &ctx](const DamageEvent& e) {
            on_damage(ctx.world_mut(), e);
        });

        return Ok();
    }

    Result<void> on_unload(PluginContext& ctx) override {
        ctx.unsubscribe(m_damage_sub);
        return Ok();
    }

    PluginSnapshot snapshot() override {
        // No internal state to snapshot
        return {};
    }

    Result<void> restore(const PluginSnapshot&) override {
        return Ok();
    }

private:
    void on_damage(World& world, const DamageEvent& e) {
        if (auto* health = world.get_component<Health>(e.entity)) {
            if (health->invulnerable) return;

            float actual_damage = e.amount;
            if (auto* receiver = world.get_component<DamageReceiver>(e.entity)) {
                actual_damage = (e.amount - receiver->armor) * receiver->damage_multiplier;
            }

            health->current = std::max(0.0f, health->current - actual_damage);
        }
    }

    SystemId m_regen_system;
    SystemId m_death_system;
    SubscriptionId m_damage_sub;
};

// Entry point
extern "C" PLUGIN_API IPlugin* plugin_create() {
    return new HealthPlugin();
}

extern "C" PLUGIN_API void plugin_destroy(IPlugin* plugin) {
    delete plugin;
}
```

---

## 5. Plugin Lifecycle & Hot-Reload

### Plugin States

```
┌────────────┐
│ Discovered │  Package found on disk
└─────┬──────┘
      │ resolve dependencies
      ▼
┌────────────┐
│  Resolved  │  Dependencies satisfied
└─────┬──────┘
      │ load DLL
      ▼
┌────────────┐
│  Loading   │  DLL loaded, calling on_load()
└─────┬──────┘
      │ on_load() succeeds
      ▼
┌────────────┐
│   Active   │  Running, systems executing
└─────┬──────┘
      │ hot-reload or unload requested
      ▼
┌────────────┐
│ Reloading  │  snapshot() called, DLL swapping
└─────┬──────┘
      │ restore() succeeds
      ▼
┌────────────┐
│   Active   │  Back to running
└────────────┘

      │ unload requested
      ▼
┌────────────┐
│ Unloading  │  on_unload() called
└─────┬──────┘
      │ cleanup complete
      ▼
┌────────────┐
│  Unloaded  │  DLL released
└────────────┘
```

### Hot-Reload Guarantees

1. **Component data survives**: ECS keeps component storage, only code reloads
2. **Entity IDs stable**: No entity ID changes during reload
3. **System re-registration**: New code, same system names, same stages
4. **Event handlers refreshed**: Old handlers removed, new handlers added
5. **Plugin state restored**: Custom state via snapshot/restore

### What Can Change During Hot-Reload

| Can Change | Cannot Change |
|-----------|--------------|
| System logic | Component struct layout (causes migration) |
| Event handler behavior | Plugin ID |
| Internal algorithms | Registered component names |
| Constants, tuning values | |

### Component Migration (Schema Change)

If component struct changes:

```cpp
// Old: Health { current, max }
// New: Health { current, max, shield }

// Engine detects schema mismatch and migrates:
for (auto [e, old_data] : query_raw("Health")) {
    Health new_health;
    new_health.current = old_data["current"];
    new_health.max = old_data["max"];
    new_health.shield = 0.0f;  // New field gets default
    world.replace_component(e, new_health);
}
```

---

## 6. Current Gaps & Required Wiring

### Gap 1: IPlugin Interface Not Defined

**Status**: No formal plugin interface exists.

**Location needed**: `include/void_engine/plugin_api/plugin.hpp`

**Required**:
```cpp
class IPlugin {
    virtual const char* id() const = 0;
    virtual Result<void> on_load(PluginContext& ctx) = 0;
    virtual Result<void> on_unload(PluginContext& ctx) = 0;
    virtual PluginSnapshot snapshot() = 0;
    virtual Result<void> restore(const PluginSnapshot&) = 0;
};
```

### Gap 2: PluginContext Not Implemented

**Status**: Plugins have no way to access engine services.

**Location needed**: `include/void_engine/plugin_api/context.hpp`

**Required**:
- Component registration API
- System registration API
- ECS World access
- Event subscription
- Asset loading
- Render contract (make_renderable)

### Gap 3: DLL Loading Not Connected

**Status**: `DynamicLibrary` class exists but `PluginPackageLoader` doesn't use it.

**Location**: `src/package/plugin_package_loader.cpp`

**Required**:
```cpp
Result<void> PluginPackageLoader::load(const ResolvedPackage& pkg, LoadContext& ctx) {
    // Load manifest
    auto manifest = PluginPackageManifest::load(pkg.manifest.source_path);

    // Load DLL if specified
    if (!manifest.library_path.empty()) {
        auto dll = DynamicLibrary::load(resolve_library_path(manifest));
        auto create_fn = dll.get_symbol<PluginCreateFn>("plugin_create");

        auto* plugin = create_fn();

        PluginContext plugin_ctx{...};
        plugin->on_load(plugin_ctx);

        m_loaded_plugins[manifest.name] = {plugin, std::move(dll)};
    }

    return Ok();
}
```

### Gap 4: Engine Render Components Not Exposed

**Status**: Render components exist but not in schema registry.

**Location**: `src/runtime/runtime.cpp` init_packages()

**Required**:
```cpp
// Register engine core components for plugins to use
void register_engine_core_components(ComponentSchemaRegistry& registry) {
    registry.register_builtin<TransformComponent>("Transform");
    registry.register_builtin<MeshComponent>("Mesh");
    registry.register_builtin<MaterialComponent>("Material");
    registry.register_builtin<LightComponent>("Light");
    registry.register_builtin<CameraComponent>("Camera");
    registry.register_builtin<RenderableTag>("Renderable");
    registry.register_builtin<HierarchyComponent>("Hierarchy");
}
```

### Gap 5: Render Systems Not Registered

**Status**: Render systems exist but don't run.

**Location**: `src/runtime/runtime.cpp` init_render()

**Required**:
```cpp
void register_engine_core_systems(Kernel& kernel, World& world) {
    kernel.register_system(Stage::Update, "TransformSystem",
        [&world](float dt) { TransformSystem::run(world, dt); }, -100);

    kernel.register_system(Stage::RenderPrepare, "CameraSystem",
        [&world](float dt) { CameraSystem::run(world, dt); }, 0);

    kernel.register_system(Stage::RenderPrepare, "LightSystem",
        [&world](float dt) { LightSystem::run(world, dt); }, 10);

    kernel.register_system(Stage::RenderPrepare, "RenderPrepareSystem",
        [&world](float dt) { RenderPrepareSystem::run(world, dt); }, 100);

    kernel.register_system(Stage::Render, "RenderSystem",
        [&world](float dt) { RenderSystem::run(world, dt); }, 0);
}
```

### Gap 6: RenderContext Not in ECS

**Status**: RenderContext is standalone, not an ECS resource.

**Required**:
```cpp
// In init_render()
auto render_ctx = RenderContext{};
render_ctx.initialize(width, height);
m_packages->ecs_world->insert_resource(std::move(render_ctx));
```

### Gap 7: make_renderable Contract Not Implemented

**Status**: No API for plugins to request entity rendering.

**Required in PluginContext**:
```cpp
Result<void> PluginContext::make_renderable(Entity e, const RenderableDesc& desc) {
    // Add engine render components to entity
}
```

### Gap 8: Plugin State Not Tracked in ECS

**Status**: No ECS resource tracking plugin states.

**Required**:
```cpp
// In WorldComposer or PluginPackageLoader
world.insert_resource(PluginRegistry{});

// During plugin load
auto& registry = world.resource_mut<PluginRegistry>();
registry.plugins[plugin_id] = PluginState{...};
```

### Gap 9: Example Has No Native Code

**Status**: Example plugins are JSON-only.

**Required**:
- Build system for plugin DLLs
- Example plugin implementations (or use base plugins)
- Library paths in manifests

---

## Summary

### The Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         ENGINE                                   │
│                                                                  │
│  Core (always available):                                        │
│    - ECS World, Kernel, EventBus, AssetServer                   │
│    - Transform, Mesh, Material, Light, Camera, Renderable       │
│    - TransformSystem, RenderPrepareSystem, RenderSystem         │
│    - IPlugin interface, PluginContext                           │
│                                                                  │
│  Contract: make_renderable(entity, desc)                        │
│                                                                  │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                    ═══════════╪═══════════
                               │
┌──────────────────────────────┴──────────────────────────────────┐
│                     BASE PLUGINS (engine-provided)               │
│                                                                  │
│  core.transform, core.hierarchy                                  │
│  base.health, base.physics, base.ai, base.combat                │
│                                                                  │
│  Provides common components & systems creators can use           │
│                                                                  │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                    ═══════════╪═══════════
                               │
┌──────────────────────────────┴──────────────────────────────────┐
│                     CREATOR PLUGINS (remote loaded)              │
│                                                                  │
│  game.rpg, game.shooter, mod.custom_enemies                     │
│                                                                  │
│  Uses base plugins + adds game-specific functionality           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### What Needs Wiring

1. **Define IPlugin interface** - The contract plugins implement
2. **Implement PluginContext** - What engine provides to plugins
3. **Connect DLL loading** - Load native code from plugins
4. **Expose engine render components** - Register in schema registry
5. **Register engine render systems** - Connect to kernel stages
6. **Add RenderContext to ECS** - Make it a resource
7. **Implement make_renderable** - The render contract API
8. **Track plugin state in ECS** - PluginRegistry resource
9. **Create base plugins** - Health, combat, etc. as templates
10. **Update example** - Use base plugins or provide DLLs
