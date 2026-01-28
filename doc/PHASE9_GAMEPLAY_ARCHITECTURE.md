# Phase 9: Gameplay - Plugin Architecture Design

**Status**: IMPLEMENTED (Core infrastructure complete)
**Date**: 2026-01-28
**Depends on**: Phase 8 (Scripting - void_cpp hot-reload infrastructure)

## Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| AIStateStore | COMPLETE | Blackboard, behavior trees, navigation, perception |
| CombatStateStore | COMPLETE | Vitals, status effects, projectiles, damage history |
| InventoryStateStore | COMPLETE | Containers, equipment, crafting, world items |
| IStateCommand | COMPLETE | 12 command types implemented |
| CommandProcessor | COMPLETE | Validates and executes commands |
| IPluginAPI | COMPLETE | Read state, submit commands, engine services |
| GameplayPlugin | COMPLETE | Hot-reloadable base class |
| GameStateCore | COMPLETE | Owns all state, coordinates plugins |

---

## Alignment with Phased Build System

This design follows the void_engine phased initialization pattern established in `CMakeLists.txt` and `src/main.cpp`:

```
Phase 0:  Skeleton      - CLI, manifest
Phase 1:  Foundation    - memory, core, math, structures
Phase 2:  Infrastructure - event, services, ir, kernel
Phase 3:  Resources     - asset, shader
Phase 4:  Platform      - presenter, render, compositor
Phase 5:  I/O           - audio, input
Phase 6:  Simulation    - ecs, physics, triggers
Phase 7:  Scene         - scene, graph
Phase 8:  Scripting     - script, scripting, cpp, shell  ← Hot-reload infrastructure
Phase 9:  Gameplay      - gamestate + plugins (ai, combat, inventory)  ← THIS PHASE
Phase 10: UI            - ui, hud
Phase 11: Extensions    - xr, editor
Phase 12: Application   - runtime, engine
```

### Key Architectural Decision

**Phase 9 differs from other phases**: Instead of adding more core modules, it establishes a **plugin consumption pattern** where:

1. **GameStateCore** (core module) - Maintains ALL persistent game state
2. **Gameplay Plugins** (hot-swappable) - Implement game logic, read/write state through API

This enables live game development with instant code iteration.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           void_engine Core                               │
│  (Phases 0-8: Always loaded, foundation for everything)                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   Phase 8: void_cpp provides hot-reload infrastructure                  │
│   ├── DynamicModule     - Load/unload DLLs at runtime                   │
│   ├── ModuleRegistry    - Track loaded modules                          │
│   ├── HotReloader       - Watch files, trigger recompile                │
│   ├── CppClassRegistry  - Instantiate plugin classes                    │
│   └── FfiWorldContext   - Engine API for plugins                        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    Phase 9: GameStateCore (Core Module)                  │
│                                                                          │
│   OWNS ALL PERSISTENT STATE - Never hot-reloaded                        │
│   ├── AIStateStore        - Blackboards, nav state, perception          │
│   ├── CombatStateStore    - Vitals, effects, projectiles, damage        │
│   ├── InventoryStateStore - Items, equipment, crafting, shops           │
│   ├── VariableStore       - Game flags, counters, entity variables      │
│   ├── ObjectiveTracker    - Quest progress, objectives                  │
│   ├── SaveManager         - Save/load, checkpoints, autosave            │
│   └── GameStateMachine    - Phase transitions (menu, gameplay, etc.)    │
│                                                                          │
│   PROVIDES: IPluginAPI for plugins to read state and submit commands    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
        │                           │                           │
        │ IPluginAPI                │ IPluginAPI                │ IPluginAPI
        ▼                           ▼                           ▼
┌───────────────────┐   ┌───────────────────┐   ┌───────────────────┐
│     AIPlugin      │   │   CombatPlugin    │   │  InventoryPlugin  │
│   (Hot-Swap DLL)  │   │   (Hot-Swap DLL)  │   │   (Hot-Swap DLL)  │
├───────────────────┤   ├───────────────────┤   ├───────────────────┤
│ • Behavior Trees  │   │ • Damage Calc     │   │ • Item Registry   │
│ • State Machines  │   │ • Projectiles     │   │ • Equipment       │
│ • Navigation      │   │ • Status Effects  │   │ • Crafting        │
│ • Perception      │   │ • Weapons         │   │ • Loot Gen        │
│ • Steering        │   │ • Hit Detection   │   │ • Trading         │
└───────────────────┘   └───────────────────┘   └───────────────────┘
        │                           │                           │
        └───────────────────────────┴───────────────────────────┘
                                    │
                                    ▼
                        ┌───────────────────────┐
                        │   Command Processor   │
                        │  Validates & Executes │
                        │  State Modifications  │
                        └───────────────────────┘
```

---

## Hot-Reload Guarantees

### What Survives Plugin Hot-Reload

| Data | Location | Survives? |
|------|----------|-----------|
| Entity health/shields | CombatStateStore | ✓ YES |
| Inventory contents | InventoryStateStore | ✓ YES |
| AI blackboard values | AIStateStore | ✓ YES |
| Quest progress | ObjectiveTracker | ✓ YES |
| Game variables | VariableStore | ✓ YES |
| Save checkpoints | SaveManager | ✓ YES |
| Active projectiles | CombatStateStore | ✓ YES |
| Navigation paths | AIStateStore | ✓ YES |

### What Gets Rebuilt After Reload

| Data | Location | Rebuilt From |
|------|----------|--------------|
| Behavior tree instances | AIPlugin cache | Tree definitions + state |
| Item definitions | InventoryPlugin | Data files |
| Weapon templates | CombatPlugin | Data files |
| Navigation queries | AIPlugin | NavMesh + agent state |

### Hot-Reload Flow

```
1. Developer saves plugin source file
          │
          ▼
2. HotReloader detects file change
          │
          ▼
3. Plugin.prepare_reload() called
   └── Flush pending commands to GameStateCore
          │
          ▼
4. GameStateCore state is UNCHANGED (it's a core module)
          │
          ▼
5. Old plugin DLL unloaded
          │
          ▼
6. Compiler builds new DLL (C++20, incremental)
          │
          ▼
7. New plugin DLL loaded
          │
          ▼
8. Plugin.on_plugin_load() called
   └── Rebuild caches from GameStateCore state
          │
          ▼
9. Game continues - no state lost, new code active
```

---

## Directory Structure

```
void_engine/
├── include/void_engine/
│   ├── gamestate/
│   │   ├── gamestate_core.hpp      # Core state owner
│   │   └── stores/
│   │       ├── ai_state_store.hpp
│   │       ├── combat_state_store.hpp
│   │       └── inventory_state_store.hpp
│   │
│   └── plugin_api/
│       ├── plugin_api.hpp          # IPluginAPI interface
│       ├── gameplay_plugin.hpp     # GameplayPlugin base class
│       ├── commands.hpp            # State modification commands
│       └── state_change.hpp        # Change notification events
│
├── src/
│   ├── gamestate/
│   │   ├── gamestate_core.cpp
│   │   └── stores/
│   │       ├── ai_state_store.cpp
│   │       ├── combat_state_store.cpp
│   │       └── inventory_state_store.cpp
│   │
│   └── plugin_api/
│       └── plugin_api.cpp
│
└── plugins/                         # Hot-swappable plugin DLLs
    ├── ai/
    │   ├── CMakeLists.txt
    │   ├── ai_plugin.hpp
    │   ├── ai_plugin.cpp
    │   ├── behavior_tree.cpp
    │   ├── navigation.cpp
    │   └── perception.cpp
    │
    ├── combat/
    │   ├── CMakeLists.txt
    │   ├── combat_plugin.hpp
    │   ├── combat_plugin.cpp
    │   ├── damage_system.cpp
    │   ├── projectiles.cpp
    │   └── status_effects.cpp
    │
    └── inventory/
        ├── CMakeLists.txt
        ├── inventory_plugin.hpp
        ├── inventory_plugin.cpp
        ├── item_system.cpp
        ├── equipment.cpp
        └── crafting.cpp
```

---

## CMakeLists.txt Changes

```cmake
# ============================================================================
# PHASE 9: GAMEPLAY
# Core Module: gamestate (always loaded, owns all state)
# Plugins: ai, combat, inventory (hot-swappable)
# ============================================================================

# Core state management (NOT hot-reloaded)
add_subdirectory(src/gamestate)

# Plugin API library (linked by plugins)
add_subdirectory(src/plugin_api)

# Gameplay plugins (hot-swappable DLLs)
add_subdirectory(plugins/ai)
add_subdirectory(plugins/combat)
add_subdirectory(plugins/inventory)

# Link core module to main executable
target_link_libraries(void_engine PRIVATE void_gamestate_core)
target_link_libraries(void_engine PRIVATE void_plugin_api)

# Plugins are loaded at runtime via void_cpp, not linked
```

---

## IPluginAPI Interface

```cpp
/// Interface provided to gameplay plugins for state access
class IPluginAPI {
public:
    virtual ~IPluginAPI() = default;

    // =========================================================================
    // Read-Only State Access
    // =========================================================================
    virtual const AIStateStore& ai_state() const = 0;
    virtual const CombatStateStore& combat_state() const = 0;
    virtual const InventoryStateStore& inventory_state() const = 0;
    virtual const VariableStore& variables() const = 0;
    virtual const ObjectiveTracker& objectives() const = 0;

    // =========================================================================
    // State Modification (Command Pattern)
    // =========================================================================
    virtual void submit_command(std::unique_ptr<IStateCommand> cmd) = 0;

    // =========================================================================
    // Convenience Methods (Submit pre-built commands)
    // =========================================================================

    // AI
    virtual void set_blackboard_value(EntityId entity, std::string_view key,
                                       const BlackboardValue& value) = 0;
    virtual void request_navigation(EntityId entity, const Vec3& dest) = 0;

    // Combat
    virtual void apply_damage(const DamageRequest& request) = 0;
    virtual void heal_entity(EntityId target, float amount, EntityId source) = 0;
    virtual void spawn_projectile(const ProjectileSpawnRequest& request) = 0;
    virtual void apply_status_effect(EntityId target, StatusEffectId effect) = 0;

    // Inventory
    virtual bool add_item(EntityId entity, ItemDefId item, uint32_t qty) = 0;
    virtual bool remove_item(EntityId entity, ItemDefId item, uint32_t qty) = 0;
    virtual bool equip_item(EntityId entity, ItemInstanceId item, EquipSlot slot) = 0;

    // =========================================================================
    // Engine Services
    // =========================================================================
    virtual const void_ecs::World& ecs_world() const = 0;
    virtual float delta_time() const = 0;
    virtual double game_time() const = 0;
    virtual void log(LogLevel level, std::string_view message) = 0;

    // =========================================================================
    // Event Subscription
    // =========================================================================
    virtual SubscriptionId subscribe(StateCategory cat, StateChangeCallback cb) = 0;
    virtual void unsubscribe(SubscriptionId id) = 0;
};
```

---

## GameplayPlugin Base Class

```cpp
/// Base class for hot-swappable gameplay plugins
class GameplayPlugin : public void_core::Plugin,
                       public void_core::HotReloadable {
public:
    // =========================================================================
    // Plugin Lifecycle
    // =========================================================================

    /// Called when plugin loads - receive API, initialize
    void_core::Result<void> on_load(PluginContext& ctx) final {
        m_api = ctx.get<IPluginAPI*>("plugin_api");
        return on_plugin_load();
    }

    /// Called before unload - cleanup, flush commands
    void_core::Result<PluginState> on_unload(PluginContext& ctx) final {
        on_plugin_unload();
        return capture_plugin_state();
    }

    /// Called after hot-reload - restore from state
    void_core::Result<void> on_reload(PluginContext& ctx, PluginState state) final {
        m_api = ctx.get<IPluginAPI*>("plugin_api");
        return restore_plugin_state(state);
    }

    // =========================================================================
    // Override These
    // =========================================================================

    virtual void_core::Result<void> on_plugin_load() = 0;
    virtual void on_plugin_unload() {}
    virtual void on_tick(float dt) = 0;
    virtual void on_fixed_tick(float dt) {}

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    bool supports_hot_reload() const override { return true; }

    // Plugins don't own persistent state - GameStateCore does
    // Only capture runtime caches/config for faster reload
    void_core::Result<HotReloadSnapshot> snapshot() override {
        return capture_runtime_caches();
    }

    void_core::Result<void> restore(HotReloadSnapshot snap) override {
        return restore_runtime_caches(snap);
    }

protected:
    IPluginAPI* api() { return m_api; }

    virtual HotReloadSnapshot capture_runtime_caches() { return {}; }
    virtual void_core::Result<void> restore_runtime_caches(HotReloadSnapshot) {
        return void_core::Ok();
    }

private:
    IPluginAPI* m_api = nullptr;
};
```

---

## State Stores

### AIStateStore

```cpp
struct AIStateStore {
    // Per-entity blackboards (shared decision-making data)
    std::unordered_map<EntityId, BlackboardData> entity_blackboards;

    // Global blackboards (world-level AI knowledge)
    std::unordered_map<std::string, BlackboardData> global_blackboards;

    // Behavior tree execution state
    std::unordered_map<EntityId, BehaviorTreeState> tree_states;

    // Navigation agent state (current path, waypoint index)
    std::unordered_map<EntityId, NavAgentState> nav_states;

    // Perception state (known targets, last seen positions)
    std::unordered_map<EntityId, PerceptionState> perception_states;

    // Serialization
    void serialize(BinaryWriter& writer) const;
    void deserialize(BinaryReader& reader);
};
```

### CombatStateStore

```cpp
struct CombatStateStore {
    // Entity vitals (health, shields, armor)
    std::unordered_map<EntityId, VitalsState> entity_vitals;

    // Active status effects per entity
    std::unordered_map<EntityId, std::vector<ActiveEffect>> status_effects;

    // Combat statistics (kills, deaths, damage dealt)
    std::unordered_map<EntityId, CombatStats> combat_stats;

    // In-flight projectiles
    std::vector<ProjectileState> active_projectiles;

    // Recent damage for assist tracking
    std::unordered_map<EntityId, DamageHistory> damage_history;

    // Serialization
    void serialize(BinaryWriter& writer) const;
    void deserialize(BinaryReader& reader);
};
```

### InventoryStateStore

```cpp
struct InventoryStateStore {
    // Per-entity inventories
    std::unordered_map<EntityId, InventoryData> entity_inventories;

    // Equipped items per entity
    std::unordered_map<EntityId, EquipmentData> equipment;

    // Active crafting jobs
    std::unordered_map<EntityId, CraftingQueueData> crafting_queues;

    // Dropped items in world
    std::vector<WorldItemData> world_items;

    // Shop inventories
    std::unordered_map<std::string, ShopState> shops;

    // Serialization
    void serialize(BinaryWriter& writer) const;
    void deserialize(BinaryReader& reader);
};
```

---

## main.cpp Integration

```cpp
// =========================================================================
// PHASE 9: GAMEPLAY
// =========================================================================
spdlog::info("Phase 9: Gameplay");

// Initialize GameStateCore (owns all persistent state)
spdlog::info("  [gamestate_core]");
void_gamestate::GameStateCore game_state_core;
auto gs_result = game_state_core.initialize();
if (!gs_result) {
    spdlog::error("    Failed to initialize GameStateCore");
    return 1;
}
spdlog::info("    State stores: AI, Combat, Inventory");
spdlog::info("    Command processor: ready");
spdlog::info("    Save/load: integrated");

// Create Plugin API implementation
spdlog::info("  [plugin_api]");
void_plugin_api::PluginAPIImpl plugin_api(&game_state_core, &ecs_world, &event_bus);
spdlog::info("    API ready for plugins");

// Load gameplay plugins
spdlog::info("  [plugins]");
void_cpp::CppSystem& cpp_system = void_cpp::CppSystem::instance();

// Set plugin API in context
cpp_system.set_plugin_context("plugin_api", &plugin_api);

// Load plugins from plugins/ directory
auto ai_result = cpp_system.load_plugin("plugins/ai");
if (ai_result) {
    spdlog::info("    AIPlugin: loaded (behavior trees, navigation, perception)");
}

auto combat_result = cpp_system.load_plugin("plugins/combat");
if (combat_result) {
    spdlog::info("    CombatPlugin: loaded (damage, projectiles, effects)");
}

auto inv_result = cpp_system.load_plugin("plugins/inventory");
if (inv_result) {
    spdlog::info("    InventoryPlugin: loaded (items, equipment, crafting)");
}

// Enable hot-reload for all plugins
cpp_system.watch_directory("plugins/", true);
spdlog::info("    Hot-reload: ENABLED for plugins/");

spdlog::info("Phase 9 complete");
```

---

## Expected Output

```
Phase 9: Gameplay
  [gamestate_core]
    State stores: AI, Combat, Inventory
    Command processor: ready
    Save/load: integrated
  [plugin_api]
    API ready for plugins
  [plugins]
    AIPlugin: loaded (behavior trees, navigation, perception)
    CombatPlugin: loaded (damage, projectiles, effects)
    InventoryPlugin: loaded (items, equipment, crafting)
    Hot-reload: ENABLED for plugins/
Phase 9 complete
```

---

## Verification Checklist

- [ ] GameStateCore initializes with all state stores
- [ ] PluginAPI provides read access to all state stores
- [ ] AIPlugin loads and ticks correctly
- [ ] CombatPlugin loads and ticks correctly
- [ ] InventoryPlugin loads and ticks correctly
- [ ] Hot-reload: modify plugin source, verify game continues with state intact
- [ ] Save/load: save game, modify plugin, load game - state matches
- [ ] Command validation: invalid commands rejected gracefully
- [ ] Entity lifecycle: state cleanup when entities destroyed
