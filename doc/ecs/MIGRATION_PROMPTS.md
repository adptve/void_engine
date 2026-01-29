# Package System Migration Prompts

These prompts are designed to be used sequentially with Claude Code. After each phase:
1. Build the project (`cmake -B build && cmake --build build`)
2. If errors, paste the build output to Claude to fix
3. Once building, move to the next phase

**Critical Requirements (included in each prompt):**
- All packages must be loadable from external sources the engine has never seen
- No hardcoded paths, types, or assumptions about content
- Dynamic discovery and registration at runtime
- Production-ready code with complete error handling

---

## Phase 1: Package Infrastructure

```
TASK: Implement Phase 1 of the Package System Migration - Core Infrastructure

READ THESE DOCUMENTS FIRST:
- doc/ecs/PACKAGE_SYSTEM.md (full spec)
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 6: Phase 1)
- CLAUDE.md (code standards)

CRITICAL REQUIREMENT:
This system must load packages from EXTERNAL SOURCES the engine has never seen before.
No hardcoded types, paths, or assumptions. Everything discovered and registered at runtime.

CREATE THE FOLLOWING in include/void_engine/package/ and src/package/:

1. **fwd.hpp** - Forward declarations

2. **version.hpp / version.cpp** - SemVer implementation
   - Parse "1.2.3", "1.2.3-beta", "1.2.3+build"
   - Compare versions (==, <, >, <=, >=)
   - Match constraints: ">=1.0.0", "^1.2", "~1.2.3", ranges
   - Full error handling for malformed versions

3. **manifest.hpp / manifest.cpp** - Package manifests
   - PackageType enum (World, Layer, Plugin, Widget, Asset)
   - PackageDependency struct with version constraints
   - PackageManifest with all fields from PACKAGE_SYSTEM.md
   - JSON parsing via nlohmann/json
   - Validation (may_depend_on rules, required fields)
   - Static factory: PackageManifest::load(path) -> Result<PackageManifest>

4. **resolver.hpp / resolver.cpp** - Dependency resolution
   - Topological sort for load order
   - Cycle detection with clear error messages
   - Plugin layer validation (core < engine < gameplay < feature < mod)
   - Version constraint satisfaction
   - Result<std::vector<ResolvedPackage>> resolve(package_name)

5. **loader.hpp** - Loader interface
   - Abstract PackageLoader base class
   - LoadContext providing access to ECS World, AssetServer, EventBus
   - Virtual load/unload methods

6. **registry.hpp / registry.cpp** - Package registry
   - scan_directory(path) - discover available packages
   - load_package(name, ctx) -> Result<void>
   - unload_package(name, ctx) -> Result<void>
   - reload_package(name, ctx) -> Result<void>
   - Query methods: status(), loaded_packages(), available_packages()
   - PackageStatus enum (Available, Loading, Loaded, Failed)

7. **package.hpp** - Main include that exports all

8. **CMakeLists.txt** - Build configuration for void_package module

IMPLEMENTATION REQUIREMENTS:
- Use void_core::Result<T> for all fallible operations
- Proper RAII for all resources
- Thread-safe where appropriate (registry access)
- Complete error messages with context (which package, which dependency, why failed)
- No stubs, no TODOs, no placeholder returns
- Follow existing codebase conventions (m_ prefix, namespaces, etc.)

NAMESPACE: void_package

After implementation, I will build and provide any errors for you to fix.
```

---

## Phase 2: Asset Bundle Migration

```
TASK: Implement Phase 2 of the Package System Migration - Asset Bundles

READ THESE DOCUMENTS FIRST:
- doc/ecs/PACKAGE_SYSTEM.md (section 6: asset.bundle)
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 7: Phase 2)
- CLAUDE.md (code standards)

PREREQUISITE: Phase 1 must be complete and building.

CRITICAL REQUIREMENT:
Asset bundles must be loadable from EXTERNAL SOURCES the engine has never seen.
- Unknown mesh formats registered dynamically
- Unknown texture types handled
- Prefabs with arbitrary component combinations (components by NAME, not compile-time type)
- Definitions for registries the engine doesn't know about yet

CREATE/MODIFY THE FOLLOWING:

1. **include/void_engine/package/asset_bundle.hpp** - Asset bundle manifest
   - MeshEntry, TextureEntry, MaterialEntry, AnimationEntry
   - AudioEntry, VfxEntry, ShaderEntry
   - PrefabEntry (components as map<string, json>)
   - DefinitionEntry (registry_type + arbitrary json data)
   - AssetBundleManifest extending PackageManifest
   - JSON parsing for all entry types

2. **include/void_engine/package/prefab_registry.hpp** - Prefab system
   - PrefabDefinition storing component data as JSON (not typed!)
   - register_prefab(definition)
   - get(prefab_id) -> const PrefabDefinition*
   - instantiate(prefab_id, world, optional_transform) -> Result<Entity>
   - Instantiation resolves component names to ComponentIds at RUNTIME

3. **include/void_engine/package/definition_registry.hpp** - Definition system
   - Generic registry for weapon definitions, aura definitions, etc.
   - register_definition(registry_type, id, json_data)
   - get_definition(registry_type, id) -> optional<json>
   - list_definitions(registry_type) -> vector<string>
   - Collision policy (error, first_wins, last_wins)

4. **src/package/asset_bundle_loader.cpp** - Loader implementation
   - Implements PackageLoader interface
   - Loads meshes into AssetServer
   - Loads textures, materials, shaders
   - Registers prefabs with PrefabRegistry
   - Registers definitions with DefinitionRegistry
   - All paths are bundle-relative

5. **Modify src/asset/** as needed to support:
   - Bundle-relative path resolution
   - Dynamic asset type registration if not already supported

RUNTIME COMPONENT INSTANTIATION (critical for external packages):
```cpp
// Prefab JSON (from external package):
{
  "components": {
    "Health": { "max": 100, "current": 100 },
    "CustomModComponent": { "power_level": 9000 }
  }
}

// At instantiate time:
for (const auto& [comp_name, comp_data] : prefab.components) {
    // Look up component ID by NAME (registered by some plugin)
    auto comp_id = component_registry.get_id_by_name(comp_name);
    if (!comp_id) {
        return Error("Unknown component: " + comp_name);
    }
    // Create component from JSON using schema
    auto bytes = schema_registry.create_instance(comp_name, comp_data);
    world.add_component_raw(entity, *comp_id, bytes);
}
```

IMPLEMENTATION REQUIREMENTS:
- No compile-time component types in prefab instantiation
- All component resolution by string name at runtime
- Support for components from plugins not yet loaded (defer or error clearly)
- Full error handling with context
- No hardcoded asset paths or types

After implementation, I will build and provide any errors for you to fix.
```

---

## Phase 3: Plugin System Migration

```
TASK: Implement Phase 3 of the Package System Migration - Plugin Packages

READ THESE DOCUMENTS FIRST:
- doc/ecs/PACKAGE_SYSTEM.md (section 4: plugin.package)
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 8: Phase 3)
- doc/ecs/ECS_COMPREHENSIVE_ARCHITECTURE.md (component/system registration)
- CLAUDE.md (code standards)

PREREQUISITE: Phases 1-2 must be complete and building.

CRITICAL REQUIREMENT:
Plugins must be loadable from EXTERNAL SOURCES the engine has never seen.
- Components declared in JSON, not C++ headers
- Systems loaded from dynamic libraries (.dll/.so)
- Event handlers registered by name
- The engine should run mods it was never compiled against

CREATE/MODIFY THE FOLLOWING:

1. **include/void_engine/package/plugin_package.hpp** - Plugin manifest
   - ComponentDeclaration with fields as map<string, FieldDeclaration>
   - FieldDeclaration (type string, optional default as json)
   - SystemDeclaration (name, stage, query, priority, library path)
   - EventHandlerDeclaration (event name, handler name, library, filter)
   - RegistryDeclaration (name, type, entries_path)
   - PluginPackageManifest extending PackageManifest

2. **include/void_engine/package/component_schema.hpp** - Schema registry
   - Stores component schemas (field names, types, defaults)
   - register_from_declaration(decl) -> Result<ComponentId>
   - get_schema(name) -> const ComponentDeclaration*
   - create_instance(name, json_data) -> Result<vector<byte>>
   - Supports types: f32, f64, i32, u32, bool, string, vec2, vec3, vec4, quat, mat4, Entity, array<T>

3. **include/void_engine/package/dynamic_library.hpp** - DLL/SO loading
   - Cross-platform dynamic library loading (LoadLibrary/dlopen)
   - get_symbol<T>(name) -> T*
   - Proper unloading and error handling
   - RAII wrapper

4. **src/package/plugin_package_loader.cpp** - Loader implementation
   - Register component types from declarations (JSON schema -> ECS ComponentId)
   - Load dynamic libraries for systems
   - Register systems with scheduler using manifest info
   - Subscribe event handlers
   - Initialize registries from DefinitionRegistry

5. **Modify include/void_engine/ecs/component.hpp** if needed:
   - Support registering components by schema (not just template)
   - register_component_dynamic(name, size, align, ...) -> ComponentId
   - Lookup by name: get_id_by_name(string) -> optional<ComponentId>

6. **Modify include/void_engine/ecs/system.hpp** if needed:
   - Support registering systems from function pointers loaded at runtime
   - System queries built from component names, resolved to IDs at registration

DYNAMIC SYSTEM REGISTRATION (critical for external plugins):
```cpp
// Plugin manifest declares:
{
  "systems": [{
    "name": "PlasmaOverheatSystem",
    "stage": "update",
    "query": ["PlasmaWeapon", "Heat"],
    "library": "plasma_weapon.dll"
  }]
}

// At load time:
auto lib = DynamicLibrary::load(manifest_dir / system.library);
auto run_fn = lib.get_symbol<void(*)(World&)>(system.name + "_run");

// Build query from component NAMES
QueryDescriptor query;
for (const auto& comp_name : system.query) {
    auto id = component_registry.get_id_by_name(comp_name);
    if (!id) return Error("Unknown component in query: " + comp_name);
    query.write(*id);
}

scheduler.add_system(SystemDescriptor(system.name)
    .set_stage(parse_stage(system.stage))
    .set_query(query),
    run_fn);
```

IMPLEMENTATION REQUIREMENTS:
- Zero compile-time knowledge of external plugin components
- All type information from JSON schemas
- Dynamic library loading must be robust (handle missing symbols, version mismatch)
- Systems and handlers registered by name lookup, not template instantiation
- Full error handling: missing libraries, missing symbols, schema mismatches

After implementation, I will build and provide any errors for you to fix.
```

---

## Phase 4: Widget System Migration

```
TASK: Implement Phase 4 of the Package System Migration - Widget Packages

READ THESE DOCUMENTS FIRST:
- doc/ecs/PACKAGE_SYSTEM.md (section 5: widget.package)
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 9: Phase 4)
- CLAUDE.md (code standards)

PREREQUISITE: Phases 1-3 must be complete and building.

CRITICAL REQUIREMENT:
Widgets must be loadable from EXTERNAL SOURCES.
- Custom HUDs from mods
- Debug tools added without recompilation
- ECS bindings specified by component NAME, resolved at runtime

CREATE/MODIFY THE FOLLOWING:

1. **include/void_engine/package/widget_package.hpp** - Widget manifest
   - WidgetDeclaration (id, type, enabled_in_builds, toggle_key, config)
   - WidgetBinding (widget_id, data_source, binding_type)
   - WidgetPackageManifest extending PackageManifest

2. **include/void_engine/package/widget_manager.hpp** - Widget lifecycle
   - register_widget(declaration)
   - create_widget(id) -> Result<WidgetHandle>
   - destroy_widget(handle)
   - bind_to_query(widget_id, query_descriptor)
   - bind_to_resource(widget_id, resource_name)
   - update_all() - called each frame
   - get_widget(id) -> Widget*

3. **include/void_engine/package/widget.hpp** - Widget interface
   - Abstract Widget base class
   - Virtual methods: init(), update(dt), render(), shutdown()
   - Access to bound ECS queries and resources
   - Configuration from JSON

4. **src/package/widget_package_loader.cpp** - Loader implementation
   - Parse widget declarations
   - Create widgets based on type (builtin or from library)
   - Setup ECS bindings (queries built from component names)
   - Filter by build type (debug/development/release)

5. **Built-in widget types** (can be minimal for now):
   - "debug_hud" - FPS, frame time
   - "console" - Command input
   - "inspector" - Entity component viewer

   These serve as examples; external widgets extend this.

ECS BINDING EXAMPLE:
```cpp
// Widget manifest:
{
  "bindings": [{
    "widget_id": "health_bar",
    "data_source": "query:Player,Health",
    "binding_type": "query"
  }]
}

// At load time:
auto parts = split(binding.data_source, ':');  // ["query", "Player,Health"]
auto comp_names = split(parts[1], ',');        // ["Player", "Health"]

QueryDescriptor query;
for (const auto& name : comp_names) {
    auto id = component_registry.get_id_by_name(name);
    if (!id) return Error("Unknown component for widget binding: " + name);
    query.read(*id);  // Widgets typically read-only
}

widget_manager.bind_to_query(binding.widget_id, query);
```

IMPLEMENTATION REQUIREMENTS:
- Widget types discoverable at runtime (not hardcoded enum)
- ECS bindings by component name, resolved to IDs
- Support for custom widgets from dynamic libraries
- Build-type filtering actually implemented
- Clean lifecycle management (no leaks on unload)

After implementation, I will build and provide any errors for you to fix.
```

---

## Phase 5: Layer System Migration

```
TASK: Implement Phase 5 of the Package System Migration - Layer Packages

READ THESE DOCUMENTS FIRST:
- doc/ecs/PACKAGE_SYSTEM.md (section 3: layer.package)
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 10: Phase 5)
- CLAUDE.md (code standards)

PREREQUISITE: Phases 1-4 must be complete and building.

CRITICAL REQUIREMENT:
Layers must be loadable from EXTERNAL SOURCES.
- Mod layers that add content to base game
- Seasonal/event layers toggled at runtime
- Layers can depend on plugins (for custom components)

CREATE/MODIFY THE FOLLOWING:

1. **include/void_engine/package/layer_package.hpp** - Layer manifest
   - AdditiveSceneEntry (path, spawn_mode)
   - SpawnerEntry (id, volume, prefab, spawn_rate, max_active)
   - LightingOverride (sun, additional_lights, ambient)
   - WeatherOverride (fog, precipitation, wind_zones)
   - ObjectiveEntry (type, id, position, config)
   - ModifierEntry (path, value as json)
   - LayerPackageManifest extending PackageManifest

2. **include/void_engine/package/layer_applier.hpp** - Layer application
   - StagedLayer struct (manifest + tracking data)
   - stage(package) -> Result<StagedLayer> (parse but don't apply)
   - apply(staged_layer, world) -> Result<void>
   - unapply(layer_name, world) -> Result<void>
   - is_applied(layer_name) -> bool
   - Track all entities/resources modified by layer for clean rollback

3. **src/package/layer_package_loader.cpp** - Loader implementation
   - Implements PackageLoader
   - Stages layer on load (doesn't apply yet)
   - World decides when to apply/unapply

4. **Layer application logic:**
   - Additive scenes: instantiate entities from scene file using PrefabRegistry
   - Spawners: create spawner entities that spawn prefabs over time
   - Lighting: modify or create light entities
   - Modifiers: update ECS resources (e.g., damage_multiplier)
   - Track everything for unapply

5. **Layer unapplication logic:**
   - Despawn all entities created by this layer
   - Revert modified resources to pre-layer values
   - Remove spawners
   - Restore lighting to base state

MODIFIER SYSTEM:
```cpp
// Layer manifest:
{
  "modifiers": [
    { "path": "gameplay.damage_multiplier", "value": 1.5 },
    { "path": "gameplay.spawn_rate", "value": 2.0 }
  ]
}

// Apply:
for (const auto& mod : manifest.modifiers) {
    // Store original value for rollback
    auto original = get_resource_value(world, mod.path);
    m_original_values[layer_name][mod.path] = original;

    // Apply new value
    set_resource_value(world, mod.path, mod.value);
}

// Unapply:
for (const auto& [path, original] : m_original_values[layer_name]) {
    set_resource_value(world, path, original);
}
```

IMPLEMENTATION REQUIREMENTS:
- Layers can use prefabs from ANY loaded asset bundle
- Layers can use components from ANY loaded plugin
- Clean rollback with no leaked entities or state
- Runtime toggle (apply/unapply while game running)
- Layer ordering/priority if multiple layers modify same thing

After implementation, I will build and provide any errors for you to fix.
```

---

## Phase 6: World Package Migration

```
TASK: Implement Phase 6 of the Package System Migration - World Packages

READ THESE DOCUMENTS FIRST:
- doc/ecs/PACKAGE_SYSTEM.md (section 2: world.package)
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 11: Phase 6)
- doc/ecs/ECS_COMPREHENSIVE_ARCHITECTURE.md (Runtime, boot sequence)
- CLAUDE.md (code standards)

PREREQUISITE: Phases 1-5 must be complete and building.

CRITICAL REQUIREMENT:
Worlds must be loadable from EXTERNAL SOURCES.
- Complete game modes from mods
- Self-contained world definitions
- World specifies ALL its dependencies (plugins, layers, widgets, assets)

CREATE/MODIFY THE FOLLOWING:

1. **include/void_engine/package/world_package.hpp** - World manifest
   - PlayerSpawnConfig (prefab, spawn_selection, initial_inventory, initial_stats)
   - EnvironmentConfig (time_of_day, skybox, weather, post_process)
   - GameplayConfig (difficulty, match_length, score_limit, ruleset_flags)
   - WorldLogicConfig (win_conditions, lose_conditions, round_flow)
   - WorldPackageManifest extending PackageManifest

2. **include/void_engine/package/world_composer.hpp** - World composition
   - load_world(world_package_name) -> Result<void>
   - Full orchestration:
     1. Resolve all dependencies
     2. Load asset bundles
     3. Load plugins (registers components/systems)
     4. Load widgets
     5. Stage layers
     6. Load root scene
     7. Apply default layers
     8. Initialize ECS resources
     9. Start systems
     10. Emit OnWorldLoaded event
   - unload_world() -> Result<void>
   - switch_world(new_world_name) -> Result<void>

3. **src/package/world_package_loader.cpp** - Loader implementation
   - Implements PackageLoader
   - Delegates to WorldComposer for actual loading

4. **Modify src/runtime/** or equivalent:
   - Runtime uses WorldComposer instead of direct scene loading
   - Legacy compatibility: if given .json without package metadata, treat as legacy scene

5. **ECS Resource initialization from world manifest:**
```cpp
// World manifest:
{
  "ecs_resources": {
    "Gravity": { "value": [0, -9.81, 0] },
    "GameModeState": { "phase": "warmup", "round": 1 },
    "GlobalModifiers": { "damage_multiplier": 1.0 }
  }
}

// At load time:
for (const auto& [resource_name, data] : manifest.ecs_resources) {
    // Resources are also registered by plugins via schema
    auto schema = resource_schema_registry.get(resource_name);
    if (!schema) {
        return Error("Unknown resource type: " + resource_name);
    }
    auto resource = schema->create_from_json(data);
    world.insert_resource_dynamic(resource_name, std::move(resource));
}
```

6. **Player spawning:**
```cpp
// Use PrefabRegistry to spawn player
auto player = prefab_registry.instantiate(
    manifest.player_spawn.prefab,
    world,
    spawn_point_transform
);

// Apply initial inventory (if inventory system loaded)
if (manifest.player_spawn.initial_inventory) {
    apply_inventory(player, *manifest.player_spawn.initial_inventory);
}
```

7. **Legacy compatibility layer:**
```cpp
Result<void> Runtime::load_world(const std::string& path) {
    if (path.ends_with(".world.json")) {
        // New package system
        return m_world_composer.load_world(path);
    } else if (path.ends_with(".json")) {
        // Legacy scene.json - use old SceneInstantiator
        return load_legacy_scene(path);
    }
    return Error("Unknown world format: " + path);
}
```

IMPLEMENTATION REQUIREMENTS:
- World packages are completely self-describing
- Engine needs ZERO prior knowledge of world content
- All dependencies resolved and loaded automatically
- Clean world transitions (unload old, load new)
- Hot-reload support: can reload world package and restore state
- Legacy scenes still work during migration

FULL BOOT SEQUENCE (must implement):
1. PackageRegistry::scan_directory(content_path)
2. WorldComposer::load_world("arena_deathmatch")
   a. Resolver determines load order
   b. Load asset.bundles (PrefabRegistry, DefinitionRegistry populated)
   c. Load plugin.packages (components, systems registered)
   d. Load widget.packages (UI created)
   e. Stage layer.packages
   f. Parse world manifest
   g. Instantiate root scene (using PrefabRegistry)
   h. Apply active layers
   i. Initialize ECS resources from manifest
   j. Configure environment
   k. Start scheduler
   l. Emit WorldLoadedEvent
3. Game loop runs

After implementation, I will build and provide any errors for you to fix.
```

---

## Post-Implementation: Integration Test Prompt

```
TASK: Create integration tests for the Package System

All phases are complete. Now create comprehensive tests.

READ:
- doc/ecs/PACKAGE_SYSTEM_MIGRATION.md (section 21: Testing Strategy)

CREATE:

1. **test/package/test_packages/** - Test package fixtures
   - A minimal world package
   - A test asset bundle with prefabs
   - A test plugin with custom components
   - A test layer
   - A test widget

2. **test/package/package_tests.cpp** - Unit tests
   - Version parsing and comparison
   - Manifest parsing for all types
   - Resolver cycle detection
   - Resolver layer validation
   - Registry load/unload

3. **test/package/integration_tests.cpp** - Integration tests
   - Full world load from packages
   - Prefab instantiation with dynamic components
   - Layer apply/unapply
   - Hot-reload of packages
   - Error cases (missing deps, cycles, invalid manifests)

Use Catch2 or your existing test framework.

Each test should be self-contained and clean up after itself.
```

---

## Error Resolution Prompt Template

When build fails, use this:

```
BUILD FAILED. Here is the compiler output:

<paste build errors here>

Fix all errors. Remember:
- Read CLAUDE.md for code standards
- Check actual header files for correct types/namespaces
- No stubs or placeholder fixes
- Fix root causes, not symptoms
```

---

## Summary

| Phase | Est. Time | Key Deliverables |
|-------|-----------|------------------|
| 1 | 30-60 min | PackageManifest, Resolver, Registry |
| 2 | 30-60 min | AssetBundleLoader, PrefabRegistry, DefinitionRegistry |
| 3 | 45-90 min | PluginPackageLoader, ComponentSchema, DynamicLibrary |
| 4 | 20-40 min | WidgetPackageLoader, WidgetManager |
| 5 | 30-60 min | LayerPackageLoader, LayerApplier |
| 6 | 45-90 min | WorldPackageLoader, WorldComposer, Runtime integration |

**Total: ~4-7 hours of Claude time**

After each phase:
1. `cmake -B build && cmake --build build`
2. If errors, paste to Claude with error resolution prompt
3. Repeat until clean build
4. Move to next phase
