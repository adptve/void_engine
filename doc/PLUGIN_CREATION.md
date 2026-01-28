# Plugin Creation Guide

This guide explains how to create hot-swappable gameplay plugins for void_engine.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    GameStateCore (Engine)                    │
│  Owns ALL persistent state - survives plugin hot-reloads    │
├─────────────────────────────────────────────────────────────┤
│  AIStateStore   │ CombatStateStore │ InventoryStateStore    │
│  - Blackboards  │ - Entity Vitals  │ - Inventories          │
│  - Nav States   │ - Status Effects │ - Equipment            │
│  - Perception   │ - Projectiles    │ - Crafting Queues      │
└───────────────┬─┴────────┬─────────┴──────────┬─────────────┘
                │ Read     │ Read               │ Read
                ▼          ▼                    ▼
        ┌───────────────────────────────────────────────┐
        │              Your Plugin (.dll/.so)           │
        │  - Reads state through IPluginAPI (const)     │
        │  - Submits commands to modify state           │
        │  - Never directly modifies state stores       │
        └───────────────────────────────────────────────┘
                               │
                               │ Commands
                               ▼
                ┌─────────────────────────┐
                │    Command Processor    │
                │ Validates & Executes    │
                └─────────────────────────┘
```

**Key Principle:** Plugins NEVER own persistent game state. They read state and submit commands. This allows plugins to be unloaded, recompiled, and reloaded without losing any game state.

---

## Quick Start

### 1. Create Plugin Directory

```
plugins/
└── my_plugin/
    ├── CMakeLists.txt
    ├── my_plugin.hpp
    └── my_plugin.cpp
```

### 2. Create Plugin Header

```cpp
// my_plugin.hpp
#pragma once

#include <void_engine/plugin_api/plugin_api.hpp>

class MyPlugin : public void_plugin_api::GameplayPlugin {
public:
    // Required: Plugin identity
    void_core::PluginId id() const override {
        return void_core::PluginId("my_plugin");
    }

    void_core::Version version() const override {
        return void_core::Version{1, 0, 0};
    }

    std::string type_name() const override {
        return "MyPlugin";
    }

    bool supports_hot_reload() const override {
        return true;  // Enable hot-reload!
    }

    // Lifecycle callbacks
    void on_plugin_load(void_plugin_api::IPluginAPI* api) override;
    void on_tick(float dt) override;
    void on_fixed_tick(float dt) override;
    void on_plugin_unload() override;

    // Hot-reload state preservation
    std::vector<std::uint8_t> serialize_runtime_state() const override;
    void deserialize_runtime_state(const std::vector<std::uint8_t>& data) override;

private:
    // Your plugin's runtime state goes here
    // This will be serialized during hot-reload
};

// Required: Plugin factory functions
extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #else
    __attribute__((visibility("default")))
    #endif
    void_plugin_api::GameplayPlugin* create_plugin();

    #ifdef _WIN32
    __declspec(dllexport)
    #else
    __attribute__((visibility("default")))
    #endif
    void destroy_plugin(void_plugin_api::GameplayPlugin* plugin);
}
```

### 3. Create Plugin Implementation

```cpp
// my_plugin.cpp
#include "my_plugin.hpp"

void MyPlugin::on_plugin_load(void_plugin_api::IPluginAPI* api) {
    // Called when plugin loads or after hot-reload
    // 'api' provides read access to all game state
}

void MyPlugin::on_tick(float dt) {
    auto* api = api();

    // Read state (always const)
    const auto& ai_state = api->ai_state();
    const auto& combat_state = api->combat_state();
    const auto& inventory_state = api->inventory_state();

    // Modify state via commands
    api->apply_damage(target, 10.0f, source, void_plugin_api::DamageType::Physical);
    api->set_blackboard_bool(entity, "alert", true);
    api->add_item(entity, item_def, 1);
}

void MyPlugin::on_fixed_tick(float dt) {
    // Called at fixed timestep (physics rate)
}

void MyPlugin::on_plugin_unload() {
    // Cleanup before unload
}

std::vector<std::uint8_t> MyPlugin::serialize_runtime_state() const {
    // Serialize any runtime state you need to preserve during hot-reload
    std::vector<std::uint8_t> data;
    // ... serialize your state ...
    return data;
}

void MyPlugin::deserialize_runtime_state(const std::vector<std::uint8_t>& data) {
    // Restore runtime state after hot-reload
    // ... deserialize your state ...
}

// Plugin factory
extern "C" {
    void_plugin_api::GameplayPlugin* create_plugin() {
        return new MyPlugin();
    }

    void destroy_plugin(void_plugin_api::GameplayPlugin* plugin) {
        delete plugin;
    }
}
```

### 4. Create CMakeLists.txt

```cmake
add_library(my_plugin SHARED
    my_plugin.cpp
    my_plugin.hpp
)

target_link_libraries(my_plugin PRIVATE
    void_plugin_api
    void_core
)

target_include_directories(my_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(my_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""  # Remove 'lib' prefix
)
```

### 5. Add to Root CMakeLists.txt

```cmake
# In CMakeLists.txt, add:
add_subdirectory(plugins/my_plugin)
```

---

## IPluginAPI Reference

### State Access (Read-Only)

```cpp
// Get state stores (const references)
const AIStateStore& api->ai_state();
const CombatStateStore& api->combat_state();
const InventoryStateStore& api->inventory_state();

// Time and frame info
double api->current_time();
float api->delta_time();
uint32_t api->frame_number();
bool api->is_paused();

// Entity queries
bool api->entity_exists(EntityId entity);
Vec3 api->get_entity_position(EntityId entity);
std::vector<EntityId> api->get_entities_in_radius(Vec3 center, float radius);
```

### Commands (State Modification)

All state changes go through commands. Commands are validated before execution.

#### AI Commands
```cpp
// Blackboard manipulation
api->set_blackboard_bool(entity, "key", true);
api->set_blackboard_int(entity, "key", 42);
api->set_blackboard_float(entity, "key", 3.14f);
api->set_blackboard_string(entity, "key", "value");
api->set_blackboard_vec3(entity, "key", Vec3{1, 2, 3});
api->set_blackboard_entity(entity, "target", other_entity);

// Navigation
api->request_path(entity, destination);

// Perception
api->set_perception_target(entity, target);
```

#### Combat Commands
```cpp
// Damage
CommandResult result = api->apply_damage(target, amount, source, DamageType::Physical);

// Healing
api->heal_entity(entity, amount, source);

// Status effects
api->apply_status_effect(target, effect_name, duration, source);

// Projectiles
api->spawn_projectile(source, position, direction, speed, damage, DamageType::Fire);
```

#### Inventory Commands
```cpp
// Items
ItemInstanceId item = api->add_item(entity, item_def_id, quantity);
api->remove_item(entity, item_instance_id, quantity);
api->transfer_item(from_entity, to_entity, item_instance_id, quantity);

// Equipment
api->equip_item(entity, item_instance_id, slot_name);

// Crafting
api->start_crafting(entity, recipe_id);
```

---

## Hot-Reload Best Practices

### What Gets Preserved Automatically

**GameStateCore owns all of this - you don't need to save it:**
- All entity blackboards, behavior trees, navigation states
- All entity vitals, status effects, combat stats
- All inventories, equipment, crafting queues
- All world items, projectiles, shops

### What You Need to Preserve

**Your plugin's runtime state:**
- Entity tracking lists (which entities your plugin manages)
- Timers and cooldowns
- Cached calculations
- Configuration loaded at runtime

### Serialization Example

```cpp
std::vector<std::uint8_t> MyPlugin::serialize_runtime_state() const {
    std::vector<std::uint8_t> data;

    // Serialize a list of managed entities
    uint32_t count = static_cast<uint32_t>(m_entities.size());
    data.resize(sizeof(count));
    std::memcpy(data.data(), &count, sizeof(count));

    for (auto entity : m_entities) {
        size_t offset = data.size();
        data.resize(offset + sizeof(entity.value));
        std::memcpy(data.data() + offset, &entity.value, sizeof(entity.value));
    }

    // Serialize runtime statistics
    size_t offset = data.size();
    data.resize(offset + sizeof(m_total_time));
    std::memcpy(data.data() + offset, &m_total_time, sizeof(m_total_time));

    return data;
}

void MyPlugin::deserialize_runtime_state(const std::vector<std::uint8_t>& data) {
    if (data.empty()) return;

    size_t offset = 0;

    // Deserialize entity count
    uint32_t count;
    std::memcpy(&count, data.data() + offset, sizeof(count));
    offset += sizeof(count);

    // Deserialize entities
    m_entities.clear();
    for (uint32_t i = 0; i < count && offset < data.size(); ++i) {
        uint64_t value;
        std::memcpy(&value, data.data() + offset, sizeof(value));
        offset += sizeof(value);
        m_entities.push_back(EntityId{value});
    }

    // Deserialize statistics
    if (offset + sizeof(m_total_time) <= data.size()) {
        std::memcpy(&m_total_time, data.data() + offset, sizeof(m_total_time));
    }
}
```

---

## State Store Reference

### AIStateStore

```cpp
struct AIStateStore {
    // Per-entity blackboards
    std::unordered_map<EntityId, BlackboardData> entity_blackboards;

    // Behavior tree execution state
    std::unordered_map<EntityId, BehaviorTreeState> tree_states;

    // Navigation agent state
    std::unordered_map<EntityId, NavAgentState> nav_states;

    // Perception (what entities can see/hear)
    std::unordered_map<EntityId, PerceptionState> perception_states;

    // Global blackboard for shared data
    BlackboardData global_blackboard;
};

// BlackboardData supports these types:
struct BlackboardData {
    std::unordered_map<std::string, bool> bool_values;
    std::unordered_map<std::string, int> int_values;
    std::unordered_map<std::string, float> float_values;
    std::unordered_map<std::string, std::string> string_values;
    std::unordered_map<std::string, Vec3> vec3_values;
    std::unordered_map<std::string, EntityId> entity_values;
};
```

### CombatStateStore

```cpp
struct CombatStateStore {
    // Health, shields, armor
    std::unordered_map<EntityId, VitalsState> entity_vitals;

    // Active buffs/debuffs
    std::unordered_map<EntityId, std::vector<ActiveEffect>> status_effects;

    // Damage, attack speed, crit, etc.
    std::unordered_map<EntityId, CombatStats> combat_stats;

    // Bullets, arrows, spells in flight
    std::vector<ProjectileState> active_projectiles;

    // Who damaged who, for aggro/kill attribution
    std::unordered_map<EntityId, DamageHistory> damage_history;
};
```

### InventoryStateStore

```cpp
struct InventoryStateStore {
    // Entity inventories
    std::unordered_map<EntityId, InventoryData> entity_inventories;

    // What's equipped in each slot
    std::unordered_map<EntityId, EquipmentData> equipment;

    // Active crafting jobs
    std::unordered_map<EntityId, CraftingQueueData> crafting_queues;

    // Items on the ground
    std::vector<WorldItemData> world_items;

    // Shop inventories and prices
    std::unordered_map<std::string, ShopState> shops;

    // Master item instance registry
    std::unordered_map<ItemInstanceId, ItemInstanceData> item_instances;
};
```

---

## Events

### Subscribing to Events

```cpp
void MyPlugin::on_plugin_load(void_plugin_api::IPluginAPI* api) {
    api->subscribe_event("entity_spawned", [this](const std::any& data) {
        auto entity = std::any_cast<EntityId>(data);
        // Handle entity spawn
    });

    api->subscribe_event("entity_died", [this](const std::any& data) {
        // Handle death
    });
}
```

### Emitting Events

```cpp
api->emit_event("custom_event", some_data);
```

---

## Custom Plugin State Registry

For complex plugins that need additional state types:

```cpp
// Define your custom state
class MyCustomState : public void_plugin_api::IPluginState {
public:
    std::string type_id() const override { return "MyCustomState"; }
    std::vector<uint8_t> serialize() const override { /* ... */ }
    void deserialize(const std::vector<uint8_t>& data) override { /* ... */ }
    void clear() override { /* ... */ }
    std::unique_ptr<IPluginState> clone() const override { /* ... */ }

    // Your custom data
    std::unordered_map<EntityId, MyData> entity_data;
};

// Register in on_plugin_load
void MyPlugin::on_plugin_load(void_plugin_api::IPluginAPI* api) {
    auto* registry = /* get from context */;
    registry->register_state<MyCustomState>("my_plugin");
}
```

---

## Platform Notes

### Windows (.dll)
- Use `__declspec(dllexport)` for factory functions
- Build with `/ZI` for Edit and Continue debugging
- Plugin extension: `.dll`

### Linux (.so)
- Use `__attribute__((visibility("default")))` for factory functions
- Build with `-fPIC`
- Plugin prefix: `lib` (automatically removed by watcher)
- Plugin extension: `.so`

### macOS (.dylib)
- Same visibility attribute as Linux
- Plugin prefix: `lib` (automatically removed by watcher)
- Plugin extension: `.dylib`

---

## Debugging Hot-Reload

1. **Check plugin events:**
   ```cpp
   watcher->on_event([](const PluginEvent& event) {
       spdlog::info("Plugin event: {} - {}", event.plugin_name, event.message);
   });
   ```

2. **Verify state preservation:**
   - Log your serialized state size before/after reload
   - Compare critical values before/after

3. **Common issues:**
   - **State lost:** Check `serialize_runtime_state()` is complete
   - **Crash on reload:** Verify deserialization handles empty/partial data
   - **Plugin not detected:** Check file extension matches platform

---

## Example Plugins

See `plugins/example_ai/` for a complete working example demonstrating:
- AI behavior logic
- Reading perception/combat state
- Submitting damage/navigation commands
- Full hot-reload state serialization
