# ECS Package System Failure Analysis

**Date:** 2026-01-30
**Analyst:** Claude (Automated Analysis)
**Status:** ✅ FIXES APPLIED

## Executive Summary

The ECS package system was failing to load worlds and instantiate prefabs due to **two integration gaps** and **one design bug** introduced during the migration to a modular package-based architecture.

### Fixes Applied:
1. **`src/package/world_composer.cpp`**: Fixed `spawn_player_internal()` to directly implement spawning without calling `spawn_player()` which required `has_world()` to be true.
2. **`src/runtime/runtime.cpp`**: Added missing `prefab_registry->set_schema_registry()` call to connect the registries.
3. **`src/package/component_schema.cpp`**: Replaced stub factory with production-ready JSON parsing that properly converts field values to binary component data.

All fixes are hot-reload safe - they capture state by value and don't rely on registry pointers remaining stable.

---

## Observed Symptoms

```
[error] Failed to load world 'world.demo_arena': Failed to spawn player: No world is loaded
[warning] Failed to spawn enemy 0: Failed to apply component 'DamageReceiver': Component 'DamageReceiver' is registered but has no JSON instantiator
```

**Key observations from logs:**
- 8 packages discovered successfully
- All package types loaded (Plugins, Assets, Layers, Widgets, Worlds)
- WorldComposer shows "No world loaded" despite world package being available
- Prefabs have 0 total entities in ECS

---

## Root Cause Analysis

### Issue #1: Chicken-and-Egg Bug in World Boot Sequence

**Severity:** CRITICAL
**Location:** `src/package/world_composer.cpp:640-657` and `789-794`

The world boot sequence (`execute_boot_sequence`) calls `spawn_player_internal()` at step 10, which in turn calls the public `spawn_player()` function. However, `spawn_player()` checks `has_world()`:

```cpp
// world_composer.cpp:789-794
void_core::Result<void_ecs::Entity> WorldComposer::spawn_player(
    const std::optional<TransformData>& position_override) {

    if (!has_world()) {  // <-- FAILS HERE
        return void_core::Err<void_ecs::Entity>("No world is loaded");
    }
    // ...
}
```

The `has_world()` function checks:
```cpp
// world_composer.hpp:337-339
[[nodiscard]] bool has_world() const noexcept {
    return m_current_world.has_value() && m_current_world->state == WorldState::Ready;
}
```

**The Problem:** `m_current_world` is only assigned AFTER the boot sequence completes (line 122):
```cpp
// world_composer.cpp:117-122
// Mark as ready
info.state = WorldState::Ready;
// ...
// Store as current world
m_current_world = std::move(info);  // <-- Only set AFTER boot sequence
```

**Result:** During the boot sequence, `has_world()` always returns `false` because `m_current_world` doesn't exist yet. Step 10 (spawn player) fails, which fails the entire boot sequence, which means the world never gets loaded.

**Control Flow:**
```
load_world("world.demo_arena")
├─ execute_boot_sequence(info, options)  // info is LOCAL variable
│   ├─ Step 1-9: succeed
│   ├─ Step 10: spawn_player_internal()
│   │   └─ spawn_player()
│   │       └─ has_world() → FALSE (m_current_world not set yet!)
│   │       └─ return Err("No world is loaded")
│   └─ return Err("Failed to spawn player: No world is loaded")
├─ info.state = WorldState::Failed
├─ cleanup_partial_load(info)
└─ return Err(...)  // m_current_world NEVER set!
```

---

### Issue #2: PrefabRegistry Not Connected to ComponentSchemaRegistry

**Severity:** CRITICAL
**Location:** `src/runtime/runtime.cpp:429-504`

During runtime initialization, the package system creates both registries but **never connects them**:

```cpp
// runtime.cpp:433-437
m_packages->registry = std::make_unique<void_package::PackageRegistry>();
m_packages->load_context = std::make_unique<void_package::LoadContext>();
m_packages->composer = void_package::create_world_composer();
m_packages->prefab_registry = std::make_unique<void_package::PrefabRegistry>();
m_packages->schema_registry = std::make_unique<void_package::ComponentSchemaRegistry>();
```

The WorldComposer gets configured with both:
```cpp
// runtime.cpp:466-467
m_packages->composer->set_prefab_registry(m_packages->prefab_registry.get());
m_packages->composer->set_schema_registry(m_packages->schema_registry.get());
```

**But PrefabRegistry never gets its schema registry set:**
```cpp
// MISSING:
// m_packages->prefab_registry->set_schema_registry(m_packages->schema_registry.get());
```

The PrefabRegistry has the method available:
```cpp
// prefab_registry.hpp:293-295
void set_schema_registry(ComponentSchemaRegistry* registry) {
    m_schema_registry = registry;
}
```

**Result:** When `PrefabRegistry::apply_component()` is called, `m_schema_registry` is `nullptr`:

```cpp
// prefab_registry.cpp:284-291
// Second, try schema registry
if (m_schema_registry) {  // <-- m_schema_registry is nullptr, this block skipped!
    auto result = m_schema_registry->apply_to_entity(...);
    if (result) {
        return void_core::Ok(true);
    }
}
```

It falls through to:
```cpp
// prefab_registry.cpp:293-304
auto comp_id = world.component_id_by_name(component_name);
if (!comp_id) {
    return void_core::Err<bool>("Unknown component: " + component_name + "...");
}

// Component IS registered in ECS, but no way to instantiate from JSON
return void_core::Err<bool>("Component '" + component_name +
                       "' is registered but has no JSON instantiator");
```

This produces the exact error message observed: `Component 'DamageReceiver' is registered but has no JSON instantiator`

---

### Issue #3: Component Factory Returns Zero-Initialized Data (Secondary)

**Severity:** MEDIUM (Would cause data corruption even if Issues #1 and #2 are fixed)
**Location:** `src/package/component_schema.cpp:603-624`

The default factory created for components ignores JSON data and returns all-zero bytes:

```cpp
// component_schema.cpp:603-624
ComponentFactory ComponentSchemaRegistry::create_default_factory(const ComponentSchema& schema) {
    // ...
    std::size_t comp_size = schema.size;
    std::vector<FieldSchema> fields = schema.fields;

    return [comp_size, fields]([[maybe_unused]] const nlohmann::json& data)
        -> void_core::Result<std::vector<std::byte>> {

        std::vector<std::byte> bytes(comp_size, std::byte{0});

        // This is a simplified implementation - in production you'd want proper field layout
        // For now, just validate and return zeroed bytes
        // Real implementation would parse each field and place at correct offset
        //        ^^^ EXPLICIT TODO COMMENT ^^^

        return void_core::Ok(std::move(bytes));
    };
}
```

**Example Impact:**
```json
// Prefab definition:
"DamageReceiver": {
    "armor": 10,
    "damage_multiplier": 1.5
}
```

Expected: Component created with `armor=10`, `damage_multiplier=1.5`
Actual: Component created with `armor=0`, `damage_multiplier=0`

**Note:** The file contains a utility function `parse_field_value()` (lines 651-700+) that CAN parse JSON fields to bytes, but it's never called by the factory.

---

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           INITIALIZATION PHASE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Runtime::init_packages()                                                   │
│  ├─ Create PackageRegistry          ──────┐                                │
│  ├─ Create LoadContext                    │                                │
│  ├─ Create WorldComposer                  │                                │
│  ├─ Create PrefabRegistry     ◄───────────┤  All created independently    │
│  ├─ Create ComponentSchemaRegistry ◄──────┘                                │
│  │                                                                          │
│  ├─ WorldComposer.set_prefab_registry(prefab_registry)    ✓                │
│  ├─ WorldComposer.set_schema_registry(schema_registry)    ✓                │
│  │                                                                          │
│  └─ PrefabRegistry.set_schema_registry(???)               ✗ MISSING!      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           WORLD LOAD PHASE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Runtime::initialize()                                                      │
│  └─ load_world("world.demo_arena")                                         │
│      └─ WorldComposer::load_world()                                        │
│          │                                                                  │
│          │  ┌──────────────────────────────────────────┐                   │
│          ├─ │ info = LoadedWorldInfo (LOCAL VARIABLE)  │                   │
│          │  │ info.state = WorldState::Loading         │                   │
│          │  └──────────────────────────────────────────┘                   │
│          │                                                                  │
│          ├─ execute_boot_sequence(info, options)                           │
│          │   ├─ Step 1: resolve_dependencies()      ✓                      │
│          │   ├─ Step 2: load_assets()               ✓                      │
│          │   │   └─ Registers prefabs in PrefabRegistry                    │
│          │   ├─ Step 3: load_plugins()              ✓                      │
│          │   │   └─ Registers schemas in ComponentSchemaRegistry           │
│          │   ├─ Step 4: load_widgets()              ✓                      │
│          │   ├─ Step 5: stage_layers()              ✓                      │
│          │   ├─ Step 6: instantiate_root_scene()    ✓                      │
│          │   ├─ Step 7: apply_layers()              ✓                      │
│          │   ├─ Step 8: initialize_ecs_resources()  ✓                      │
│          │   ├─ Step 9: configure_environment()     ✓                      │
│          │   │                                                              │
│          │   └─ Step 10: spawn_player_internal()    ✗ FAILS                │
│          │       └─ spawn_player()                                          │
│          │           └─ has_world() → FALSE                                 │
│          │               │                                                  │
│          │               │  m_current_world.has_value() → FALSE            │
│          │               │  (not set until AFTER boot sequence)            │
│          │               │                                                  │
│          │               └─ return Err("No world is loaded")               │
│          │                                                                  │
│          ├─ boot_result = Err("Failed to spawn player...")                 │
│          ├─ info.state = WorldState::Failed                                │
│          ├─ cleanup_partial_load(info)                                     │
│          └─ return Err(...)                                                 │
│                                                                             │
│          ┌──────────────────────────────────────────────────────┐          │
│          │ m_current_world = std::move(info)  ◄── NEVER REACHED │          │
│          └──────────────────────────────────────────────────────┘          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                     PREFAB INSTANTIATION (from demo main.cpp)               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  prefabs->instantiate("enemy_prefab", *ecs, transform)                     │
│  └─ PrefabRegistry::apply_component("DamageReceiver", {...})               │
│      │                                                                      │
│      ├─ Try #1: m_instantiators.find(component_name)                       │
│      │   └─ Not found (no instantiators registered)                        │
│      │                                                                      │
│      ├─ Try #2: if (m_schema_registry) {...}                               │
│      │   └─ SKIPPED (m_schema_registry is nullptr!)                        │
│      │                                                                      │
│      └─ Try #3: world.component_id_by_name(component_name)                 │
│          └─ Found! (plugin registered it in ECS)                           │
│          └─ But no way to instantiate from JSON                            │
│          └─ return Err("Component 'DamageReceiver' is registered           │
│                         but has no JSON instantiator")                      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Required Fixes

### Fix #1: Use Internal Spawn During Boot Sequence

The boot sequence should use an internal method that doesn't check `has_world()`:

**Option A:** Create `spawn_player_during_boot(...)` that accepts `LoadedWorldInfo&` directly
**Option B:** Have `spawn_player_internal()` directly implement spawning without calling public `spawn_player()`

**Location to modify:** `src/package/world_composer.cpp:640-657`

### Fix #2: Connect PrefabRegistry to ComponentSchemaRegistry

Add the missing call in runtime initialization:

**Location to modify:** `src/runtime/runtime.cpp` (around line 466)

```cpp
// After creating both registries:
m_packages->prefab_registry->set_schema_registry(m_packages->schema_registry.get());
```

### Fix #3: Implement Actual JSON Parsing in Factory

The factory in `create_default_factory()` needs to actually parse JSON fields.

**Location to modify:** `src/package/component_schema.cpp:603-624`

The file already has `parse_field_value()` function that can parse individual JSON values to bytes (lines 651+). This should be used in the factory to populate the byte buffer with actual field values.

---

## File Reference

| File | Line(s) | Issue |
|------|---------|-------|
| `src/package/world_composer.cpp` | 789-794 | `spawn_player()` checks `has_world()` |
| `src/package/world_composer.cpp` | 337-339 | `has_world()` requires `m_current_world` to be set |
| `src/package/world_composer.cpp` | 117-122 | `m_current_world` only set after boot sequence |
| `src/package/world_composer.cpp` | 640-657 | `spawn_player_internal()` called during boot |
| `src/runtime/runtime.cpp` | 429-504 | Missing `prefab_registry->set_schema_registry()` |
| `src/package/prefab_registry.cpp` | 284-291 | Schema registry check (null = skip) |
| `src/package/prefab_registry.cpp` | 301-304 | "No JSON instantiator" error |
| `src/package/component_schema.cpp` | 603-624 | Stub factory returning zero bytes |
| `src/package/component_schema.cpp` | 651+ | `parse_field_value()` exists but unused |

---

## Architecture Notes

The package system's modular design is sound. The architecture separates concerns properly:

- **PackageRegistry**: Discovery and manifest resolution
- **LoadContext**: Dependency injection container for loaders
- **WorldComposer**: Orchestrates world boot sequence
- **PrefabRegistry**: Template definitions for entity creation
- **ComponentSchemaRegistry**: JSON-to-binary conversion schemas
- **DefinitionRegistry**: Game data definitions

The issues are **integration gaps** where these systems weren't fully connected during runtime initialization, not architectural flaws. The boot sequence chicken-and-egg issue is a design oversight where a public API was called from an internal context.

---

## Test Verification Steps

After fixes are applied, verify:

1. **World loads successfully:**
   ```
   [info] Loading world: world.demo_arena
   [info] Runtime initialization complete
   ```
   (No "Failed to spawn player" error)

2. **WorldComposer shows loaded world:**
   ```
   WorldComposer State:
     Current world: world.demo_arena
     State: Ready
   ```

3. **Prefab instantiation succeeds:**
   ```
   [info] Spawned enemy 0 at (-10.0, 1.0, 0.0)
   [info] Spawned enemy 1 at (0.0, 1.0, 5.0)
   [info] Spawned enemy 2 at (10.0, 1.0, 10.0)
   [info] Total entities in ECS: 3
   ```

4. **Component data is correct (not zeros):**
   Query spawned entities and verify component values match prefab definitions.

---

## Conclusion

The ECS package system migration is architecturally complete but has three integration issues:

1. **Boot sequence calls `spawn_player()` which requires world to already be loaded** (Design bug)
2. **PrefabRegistry never gets ComponentSchemaRegistry connected** (Missing initialization)
3. **Component factory stub never parses JSON** (Incomplete implementation)

Fix priority: #1 and #2 are blocking failures. #3 causes silent data corruption.
