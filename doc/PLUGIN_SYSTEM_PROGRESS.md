# Plugin System Implementation Progress

> **This is the LIVING checklist. Update this file as tasks complete.**
>
> Status: ⬜ Not Started | 🟡 In Progress | ✅ Complete | ❌ Blocked

---

## Current Phase: Verification & Testing

## Overall Progress: 17 / 45 tasks (Phases 1-6 Complete, Build Passing)

---

## Phase 1: Plugin API Foundation
**Goal**: Define the contract between engine and plugins
**Status**: ✅ Complete

### Task 1.1: Create IPlugin Interface
**File**: `include/void_engine/plugin_api/plugin.hpp`
**Status**: ✅ Complete

- [x] IPlugin base class defined
- [x] id() method - returns plugin identifier
- [x] version() method - returns semantic version
- [x] dependencies() method - returns dependency list
- [x] on_load(PluginContext&) method - called when loaded
- [x] on_unload(PluginContext&) method - called when unloading
- [x] snapshot() method - captures state for hot-reload
- [x] restore(PluginSnapshot&) method - restores state after reload
- [x] on_reloaded() method - notification after reload
- [x] component_names() method - introspection
- [x] system_names() method - introspection
- [x] PluginSnapshot struct defined
- [x] Dependency struct defined
- [x] PLUGIN_API export macros defined

**Notes**:
```
2026-01-30: Created plugin.hpp with complete IPlugin interface.
- IPlugin is separate from void_core::Plugin (which is for in-process plugins)
- This interface is for DLL-based package plugins
- Includes VOID_DECLARE_PLUGIN macro for easy export
- PluginSnapshot supports metadata for versioned state restoration
- Dependency struct supports version ranges and optional dependencies
```

---

### Task 1.2: Create PluginContext
**File**: `include/void_engine/plugin_api/context.hpp`
**Status**: ✅ Complete

- [x] PluginContext class defined
- [x] register_component<T>() template method
- [x] register_system() method with stage, name, func, priority
- [x] subscribe<E>() template for events
- [x] unsubscribe() method
- [x] world() const - read-only ECS access
- [x] world_mut() - mutable ECS access
- [x] resource<R>() const template
- [x] resource_mut<R>() template
- [x] events() - EventBus access
- [ ] assets() - AssetServer access (deferred - needs AssetServer integration)
- [x] kernel() - Kernel access
- [x] render_components() - engine component IDs
- [x] make_renderable() - render contract implementation

**Notes**:
```
2026-01-30: Created context.hpp and context.cpp with full implementation.
- Component registration uses void_package::ComponentSchemaRegistry (the canonical way)
- System registration integrates with void_kernel::IKernel stages
- Event subscription tracking (full EventBus integration TODO)
- make_renderable() implements the render contract
- assets() deferred until AssetServer is wired in
- Implementation in src/plugin_api/context.cpp
- Updated CMakeLists.txt with void_ecs, void_kernel, void_render, void_package deps
- Uses ComponentApplier type from void_package for JSON→component conversion
```

---

### Task 1.3: Create RenderableDesc
**File**: `include/void_engine/plugin_api/renderable.hpp`
**Status**: ✅ Complete

- [x] RenderableDesc struct defined
- [x] mesh_builtin field (string)
- [x] mesh_asset field (string path)
- [x] material.albedo field
- [x] material.metallic field
- [x] material.roughness field
- [x] material.albedo_texture field
- [x] material.normal_texture field
- [x] visible field
- [x] layer_mask field
- [x] render_order field
- [x] Sensible default values

**Notes**:
```
2026-01-30: Created renderable.hpp with RenderableDesc and MaterialDesc.
- MaterialDesc supports full PBR workflow (albedo, metallic, roughness, AO, emissive)
- MaterialDesc has factory methods (from_color, metallic, emissive, transparent)
- RenderableDesc has factory methods (builtin, from_asset, colored_cube/sphere)
- Builder pattern for fluent API (with_mesh, with_color, at_position, etc.)
- Transform override support for plugins that need to set position
- Advanced options (shadows, culling, LOD bias) with sensible defaults
```

---

## Phase 2: Engine Core Registration
**Goal**: Expose engine render components and systems to plugin system
**Status**: ✅ Complete

### Task 2.1: Register Engine Render Components
**File**: `src/runtime/runtime.cpp`
**Status**: ✅ Complete

- [x] TransformComponent registered with JSON factory
- [x] MeshComponent registered with JSON factory
- [x] MaterialComponent registered with JSON factory
- [x] LightComponent registered with JSON factory
- [x] CameraComponent registered with JSON factory
- [x] RenderableTag registered with JSON factory
- [x] HierarchyComponent registered with JSON factory
- [x] All factories handle missing fields with defaults
- [x] All factories handle malformed JSON gracefully
- [x] Components available before any plugin loads

**Notes**:
```
2026-01-30: Implemented register_engine_core_components() in runtime.cpp.
- Each component has a dedicated registration function in anonymous namespace
- JSON factories handle all field types with sensible defaults
- ComponentSchema includes field definitions for documentation/validation
- Uses void_package::ComponentSchemaRegistry (canonical way)
- Called at end of init_packages() before package scanning
- All 7 components registered: Transform, Mesh, Material, Light, Camera, Renderable, Hierarchy
```

---

### Task 2.2: Register Engine Render Systems
**File**: `src/runtime/runtime.cpp`
**Status**: ✅ Complete

- [x] TransformSystem registered (Stage::Update, priority -100)
- [x] CameraSystem registered (Stage::RenderPrepare, priority 0)
- [x] LightSystem registered (Stage::RenderPrepare, priority 10)
- [x] RenderPrepareSystem registered (Stage::RenderPrepare, priority 100)
- [x] RenderSystem registered (Stage::Render, priority -10)
- [x] All systems use package ECS world
- [x] Systems named with "engine." prefix

**Notes**:
```
2026-01-30: Implemented register_engine_render_systems() in runtime.cpp.
- 5 systems registered with appropriate stages and priorities
- All systems operate on m_packages->ecs_world (package ECS world)
- System names prefixed with "engine." for clarity
- RenderSystem at priority -10 to run before legacy scene_render (0)
- Called in init_render() after init_render_context()
```

---

### Task 2.3: Add RenderContext as ECS Resource
**File**: `src/runtime/runtime.cpp`
**Status**: ✅ Complete

- [x] RenderContext created with window dimensions
- [x] RenderContext initialized successfully
- [x] RenderContext moved into ECS world as resource
- [x] Render systems registered AFTER resource available
- [ ] Window resize updates the resource (TODO: wire up resize event)

**Notes**:
```
2026-01-30: Implemented init_render_context() in runtime.cpp.
- Gets window dimensions from platform or config
- Initializes RenderContext with proper dimensions
- Moves RenderContext into ECS world via insert_resource()
- Called in init_render() before register_engine_render_systems()
- Window resize handler TODO (needs event wiring)
```

---

## Phase 3: Plugin Loading Infrastructure
**Goal**: Load plugin DLLs and call their entry points
**Status**: ✅ Complete

### Task 3.1: Update PluginPackageLoader to Load DLLs
**File**: `src/package/plugin_package_loader.cpp`
**Status**: ✅ Complete

- [x] Check manifest for library path
- [x] Resolve platform-specific path (dll/so/dylib)
- [x] Load DynamicLibrary
- [x] Get plugin_create symbol
- [x] Call plugin_create to get IPlugin*
- [x] Create PluginContext
- [x] Call plugin->on_load(context)
- [x] Track in m_loaded_plugins map
- [x] Error handling: missing library
- [x] Error handling: missing symbol
- [x] Error handling: failed on_load
- [x] JSON-only plugins still work (backward compatible)

**Notes**:
```
2026-01-30: Extended PluginPackageLoader for IPlugin interface.
- try_create_iplugin() checks if library exports plugin_create
- create_plugin_context() creates PluginContext with all engine references
- populate_render_component_ids() looks up engine render components
- load() detects IPlugin libraries and uses new lifecycle
- Legacy manifest-based loading preserved for backward compatibility
- LoadedPluginState extended with iplugin, context, main_library_path
```

---

### Task 3.2: Implement Plugin Unloading
**File**: `src/package/plugin_package_loader.cpp`
**Status**: ✅ Complete

- [x] Find plugin in m_loaded_plugins
- [x] Call plugin->on_unload(context)
- [x] Unregister systems from kernel
- [x] Unregister event subscriptions
- [x] Get plugin_destroy symbol
- [x] Call plugin_destroy(plugin)
- [x] Unload DynamicLibrary
- [x] Remove from m_loaded_plugins
- [x] Continue cleanup even if on_unload fails

**Notes**:
```
2026-01-30: Implemented proper IPlugin unloading.
- unload() detects IPlugin vs legacy mode
- Calls context->unregister_all_systems() and unsubscribe_all()
- Calls plugin->on_unload(context)
- destroy_iplugin() calls plugin_destroy from library
- Library unloaded AFTER plugin_destroy (critical order)
- Cleanup continues even if on_unload fails (logged as warning)
```

---

### Task 3.3: Implement Plugin Hot-Reload
**File**: `src/package/plugin_package_loader.cpp`
**Status**: ✅ Complete

- [x] Call plugin->snapshot()
- [x] Call plugin->on_unload()
- [x] Unregister systems/subscriptions
- [x] Call plugin_destroy
- [x] Unload old DLL
- [x] Load new DLL
- [x] Call plugin_create
- [x] Create new PluginContext
- [x] Call plugin->on_load()
- [x] Call plugin->restore(snapshot)
- [x] Call plugin->on_reloaded()
- [x] Update tracking
- [ ] Rollback on failure (TODO: complex, deferred)

**Notes**:
```
2026-01-30: Implemented hot_reload_plugin() for full hot-reload cycle.
- Captures snapshot before unloading
- Full unload sequence (unregister, on_unload, destroy, unload DLL)
- Full load sequence (load DLL, create, on_load)
- Restores state from snapshot
- Calls on_reloaded() notification
- reload() override delegates to hot_reload_plugin()
- Rollback on failure not implemented (complex state restoration)
```

---

## Phase 4: Plugin State in ECS
**Goal**: Track plugin state as ECS resources
**Status**: ✅ Complete

### Task 4.1: Create PluginState Resource
**File**: `include/void_engine/plugin_api/state.hpp`
**Status**: ✅ Complete

- [x] PluginState struct defined
- [x] id, version, status fields
- [x] registered_components vector
- [x] registered_systems vector
- [x] subscriptions vector
- [x] owned_entities vector
- [x] loaded_at timestamp
- [x] last_reloaded_at timestamp
- [x] PluginRegistry struct defined
- [x] plugins map
- [x] load_order vector
- [x] get() method
- [x] is_loaded() method
- [x] dependents_of() method

**Notes**:
```
2026-01-30: Created state.hpp with PluginState and PluginRegistry.
- PluginStatus enum with Loading, Active, Reloading, Unloading, Failed, Unloaded
- PluginState tracks identity, registrations, dependencies, timing, errors
- PluginRegistry provides queries, modification, and component/system lookup
- Helper methods: is_active(), has_reloaded(), uptime(), has_component(), has_system()
- rebuild_dependents() for dependency graph maintenance
- find_component_owner() and find_system_owner() for debugging
```

---

### Task 4.2: Initialize PluginRegistry Resource
**File**: `src/runtime/runtime.cpp`
**Status**: ✅ Complete

- [x] PluginRegistry inserted in init_packages()
- [x] Available before any plugins load

**Notes**:
```
2026-01-30: Added PluginRegistry as ECS resource in init_packages().
- Inserted before register_engine_core_components()
- Available for all plugin loading operations
- Uses void_plugin_api::PluginRegistry{}
```

---

### Task 4.3: Update Plugin Loading to Track State
**File**: `src/package/plugin_package_loader.cpp`
**Status**: ✅ Complete

- [x] Get PluginRegistry from ECS
- [x] Add PluginState on load
- [x] Update load_order on load
- [x] Remove PluginState on unload
- [x] Remove from load_order on unload
- [x] Update status during lifecycle

**Notes**:
```
2026-01-30: Added tracking methods and integrated with load/unload/reload.
- get_plugin_registry() retrieves registry from ECS world
- track_plugin_loading() creates initial PluginState
- track_plugin_loaded() updates with registration info
- track_plugin_failed() sets error status
- track_plugin_unloaded() removes from registry
- track_plugin_reloading/reloaded() for hot-reload status
- All error paths now update PluginRegistry
```

---

## Phase 5: Base Plugin Library
**Goal**: Create engine-provided base plugins
**Status**: ✅ Complete

### Task 5.1: Create base.health Plugin
**Directory**: `plugins/base.health/`
**Status**: ✅ Complete

- [x] manifest.plugin.json created
- [x] CMakeLists.txt created
- [x] health_plugin.cpp created
- [x] Health component defined
- [x] DamageReceiver component defined
- [x] Dead tag component defined
- [x] Health JSON factory implemented
- [x] DamageReceiver JSON factory implemented
- [x] HealthRegenSystem implemented
- [x] DeathSystem implemented
- [ ] DamageEvent handler implemented (deferred - needs EventBus integration)
- [x] snapshot() captures internal state
- [x] restore() restores internal state
- [x] Plugin compiles as DLL/SO (CMake configured)
- [ ] Plugin loads successfully in engine (needs runtime test)

**Notes**:
```
2026-01-30: Created base.health plugin with full IPlugin implementation.
- Health component with current, max, regen_rate, regen_delay, time_since_damage
- DamageReceiver component with armor and damage_multiplier
- Dead tag component
- JSON factories for all components with validation
- HealthRegenSystem runs in FixedUpdate (priority 10)
- DeathSystem runs in PostFixed (priority 0)
- Hot-reload support with statistics preservation
- DamageEvent handler deferred until EventBus fully integrated
```

---

### Task 5.2: Create CMake Build System for Plugins
**File**: `plugins/CMakeLists.txt`
**Status**: ✅ Complete

- [x] add_void_plugin() function defined
- [x] Builds as shared library
- [x] No "lib" prefix on Linux
- [x] Links against void_plugin_api
- [x] Copies to output plugins/ directory
- [x] base.health added as subdirectory

**Notes**:
```
2026-01-30: Created plugins/CMakeLists.txt with add_void_plugin() function.
- Converts dots to underscores for valid target names
- Links void_plugin_api, void_core, void_ecs, void_package, void_kernel
- Defines VOID_PLUGIN_EXPORT for DLL export
- Sets output to ${CMAKE_BINARY_DIR}/plugins
- Copies to plugins/bin/ for development
- MSVC hot-reload settings (ZI, INCREMENTAL)
- Updated root CMakeLists.txt to use add_subdirectory(plugins)
```

---

## Phase 6: Example Integration
**Goal**: Update example to use the plugin system correctly
**Status**: ✅ Complete

### Task 6.1: Update Example Prefabs
**File**: `examples/package_demo/packages/assets/demo.characters.bundle.json`
**Status**: ✅ Complete

- [x] enemy_prefab has Transform component
- [x] enemy_prefab has Mesh component (builtin: sphere)
- [x] enemy_prefab has Material component (red color)
- [x] enemy_prefab has Renderable component
- [x] enemy_prefab has Health component (from base.health)
- [x] player_prefab updated similarly (blue capsule)
- [x] All component names match engine/plugin definitions
- [x] Added floor_tile, wall_segment, cover_crate prefabs

**Notes**:
```
2026-01-30: Updated demo.characters.bundle.json with full render components.
- player_prefab: blue capsule, Health 100, regen_rate 5, regen_delay 3, armor 10
- enemy_prefab: red sphere, Health 50, no regen, damage_multiplier 1.5
- floor_tile: gray cube for arena floor
- wall_segment: gray cube for arena walls
- cover_crate: brown cube for cover objects
- All prefabs have Transform, Mesh, Material, Renderable components
```

---

### Task 6.2: Add Camera and Lights to World
**File**: `examples/package_demo/packages/worlds/world.demo_arena.world.json`
**Status**: ✅ Complete

- [x] camera section added to manifest
- [x] camera.position defined [0, 15, 30]
- [x] camera.target defined [0, 0, 0]
- [x] camera.fov defined (60.0)
- [x] base.health plugin added as dependency
- [x] Lights defined in arena.scene.json
- [x] Directional sun light with shadows
- [x] Two point lights for arena illumination

**Notes**:
```
2026-01-30: Added camera section to world manifest and enhanced scene file.
- World manifest now includes camera settings (position, target, fov, near/far planes)
- Added base.health as plugin dependency
- Scene file updated with proper camera entity with Transform and Camera components
- Scene file lights updated with proper Transform and Light components
- Added 3 enemy entities to scene for testing
```

---

### Task 6.3: Update WorldComposer to Spawn Camera/Lights
**File**: `src/package/world_composer.cpp`
**Status**: ✅ Complete

- [x] Parse camera from scene file
- [x] Spawn entity with CameraComponent
- [x] Spawn entity with TransformComponent for camera
- [x] Set camera position from scene data
- [x] Parse lights array from scene file
- [x] Spawn entity per light with LightComponent
- [x] Spawn entity per light with TransformComponent
- [x] Set light properties from scene data
- [x] Camera marked as active via Camera component

**Notes**:
```
2026-01-30: Implemented scene instantiation in WorldComposer.
- instantiate_root_scene() now parses scene JSON file
- spawn_camera_from_scene() creates camera entity with Transform and Camera components
- spawn_light_from_scene() creates light entities with Transform and Light components
- spawn_entity_from_scene() handles both prefab-based and direct component entities
- Uses ComponentSchemaRegistry appliers for all component application
- Uses PrefabRegistry for prefab instantiation with transform overrides
```

---

## Verification Checklist

### Build Status
- [x] All modules compile without errors
- [x] All plugins build as DLLs
- [x] package_demo.exe links successfully
- [x] void_engine.exe links successfully

### Plugin API
- [x] IPlugin interface compiles
- [ ] PluginContext provides all documented APIs (needs runtime verification)
- [ ] make_renderable creates correct engine components (needs runtime verification)
- [ ] Plugins can register components with factories (needs runtime verification)
- [ ] Plugins can register systems with kernel (needs runtime verification)
- [ ] Plugins can subscribe to events (needs runtime verification)

### Engine Integration
- [x] Engine render components in schema registry
- [x] Engine render systems run in correct stages
- [x] RenderContext available as ECS resource
- [ ] Prefabs can instantiate entities with render components (needs verification)

### DLL Loading
- [x] Plugin DLLs load on Windows (.dll) - implemented, needs runtime test
- [x] Plugin SOs load on Linux (.so) - implemented, needs runtime test
- [x] plugin_create/plugin_destroy called correctly - implemented
- [x] on_load/on_unload lifecycle works - implemented
- [x] Hot-reload preserves state - implemented (snapshot/restore cycle)

### Example Works
- [ ] package_demo.exe opens window
- [ ] Entities are visible (not just grid)
- [ ] Enemies appear as colored shapes
- [ ] Camera positioned correctly
- [ ] Lighting affects scene
- [ ] Frame loop runs at target FPS

### Hot-Reload
- [ ] Modify plugin source → recompile → engine reloads
- [ ] Plugin state preserved across reload
- [ ] Systems re-registered with new code
- [ ] No crashes or memory leaks
- [ ] No orphaned entities or resources

---

## Session Log

### Session: 2026-01-30 (Build Fixes)
**Started**: Build verification and error fixes
**Completed**: Full build passing
**Notes**:
- Fixed context.hpp: `ComponentId::value` → `value()` method call syntax (lines 115-118)
- Fixed plugin_package_loader.cpp: Added explicit `SemanticVersion` → `void_core::Version` conversion (line 323)
- Fixed world_composer.cpp: Replaced invalid `get_applier()` with correct `apply_to_entity()` API
- Fixed health_plugin.cpp: All `m_*_id.value` → `m_*_id.value()` method calls (6 occurrences)
- Fixed package_demo/CMakeLists.txt: Added missing `void_plugin_api` link dependency
- All targets now compile and link successfully
- base.health.dll plugin builds correctly
- package_demo.exe builds and links correctly
- Ready for runtime verification

### Session: 2026-01-30 (Phase 6)
**Started**: Phase 6, Task 6.1 (Update Example Prefabs)
**Completed**: Phase 6 (Tasks 6.1, 6.2, 6.3)
**Notes**:
- Updated demo.characters.bundle.json with complete render components
- All prefabs now have Transform, Mesh, Material, Renderable, Health, DamageReceiver
- Added floor_tile, wall_segment, cover_crate prefabs for arena environment
- Added camera section to world.demo_arena.world.json
- Added base.health as plugin dependency in world manifest
- Updated arena.scene.json with proper camera and light entities
- Added 3 enemy entities to scene
- Implemented WorldComposer scene spawning methods
- spawn_camera_from_scene(), spawn_light_from_scene(), spawn_entity_from_scene()
- instantiate_root_scene() now parses and spawns full scene content
- Ready for verification and runtime testing

### Session: 2026-01-30 (Phase 5)
**Started**: Phase 5, Task 5.1 (Create base.health Plugin)
**Completed**: Phase 5 (Tasks 5.1, 5.2)
**Notes**:
- Created plugins/CMakeLists.txt with add_void_plugin() function
- Function handles dots-to-underscores conversion for valid target names
- Links all required modules (plugin_api, core, ecs, package, kernel)
- Created plugins/base.health/ directory with full IPlugin implementation
- Health, DamageReceiver, Dead components with JSON factories
- HealthRegenSystem and DeathSystem for gameplay logic
- Snapshot/restore for hot-reload state preservation
- Updated root CMakeLists.txt to use centralized plugins build
- Ready to proceed to Phase 6 (Example Integration)

### Session: 2026-01-30 (Phase 4)
**Started**: Phase 4, Task 4.1 (Create PluginState Resource)
**Completed**: Phase 4 (Tasks 4.1, 4.2, 4.3)
**Notes**:
- Created state.hpp with PluginState and PluginRegistry structs
- PluginStatus enum for lifecycle tracking
- PluginState tracks identity, registrations, dependencies, timing
- PluginRegistry provides queries and dependency graph
- Inserted PluginRegistry as ECS resource in runtime init_packages()
- Added tracking methods to PluginPackageLoader
- All load/unload/reload operations now update PluginRegistry
- Ready to proceed to Phase 5 (Base Plugin Library)

### Session: 2026-01-30 (Phase 3)
**Started**: Phase 3, Task 3.1 (Update PluginPackageLoader to Load DLLs)
**Completed**: Phase 3 (Tasks 3.1, 3.2, 3.3)
**Notes**:
- Extended PluginPackageLoader for IPlugin interface support
- Added try_create_iplugin(), destroy_iplugin() for DLL lifecycle
- Added create_plugin_context() with all engine references
- Added populate_render_component_ids() for render contract
- load() auto-detects IPlugin vs legacy manifest mode
- unload() properly calls on_unload() and plugin_destroy()
- hot_reload_plugin() implements full snapshot/restore cycle
- Added factory function with kernel parameter
- Ready to proceed to Phase 4 (Plugin State in ECS)

### Session: 2026-01-30 (continued)
**Started**: Phase 2, Task 2.1 (Register Engine Render Components)
**Completed**: Phase 2 (Tasks 2.1, 2.2, 2.3)
**Notes**:
- Registered 7 engine core components with JSON factories
- Registered 5 engine render systems with kernel
- Added RenderContext as ECS resource
- All component registration uses void_package::ComponentSchemaRegistry
- Systems operate on package ECS world
- Ready to proceed to Phase 3 (Plugin Loading Infrastructure)

### Session: 2026-01-30
**Started**: Phase 1, Task 1.1 (Create IPlugin Interface)
**Completed**: Phase 1 (Tasks 1.1, 1.2, 1.3)
**Notes**:
- Created complete plugin API foundation
- IPlugin interface with lifecycle methods and hot-reload support
- PluginContext with ECS/Kernel/Render integration
- RenderableDesc for the plugin-engine render contract
- Implementation compiles against void_ecs, void_kernel, void_render
- Ready to proceed to Phase 2 (Engine Core Registration)

---

## Blockers & Issues

| Issue | Status | Resolution |
|-------|--------|------------|
| assets() not implemented | Deferred | Needs AssetServer wiring in Phase 2+ |
| Event subscription basic | TODO | Full EventBus integration needed |

---

## File Change Log

| File | Change | Date |
|------|--------|------|
| `include/void_engine/plugin_api/plugin.hpp` | Created - IPlugin interface, PluginSnapshot, Dependency, VOID_DECLARE_PLUGIN | 2026-01-30 |
| `include/void_engine/plugin_api/context.hpp` | Created - PluginContext using void_package::ComponentSchemaRegistry | 2026-01-30 |
| `include/void_engine/plugin_api/renderable.hpp` | Created - RenderableDesc, MaterialDesc | 2026-01-30 |
| `src/plugin_api/context.cpp` | Created - PluginContext implementation with make_renderable | 2026-01-30 |
| `include/void_engine/plugin_api/fwd.hpp` | Updated - Added forward declarations for new types | 2026-01-30 |
| `src/plugin_api/CMakeLists.txt` | Updated - Added context.cpp and void_package dependency | 2026-01-30 |
| `doc/PLUGIN_ENGINE_CONTRACT.md` | Updated - Added CRITICAL Module Architecture section | 2026-01-30 |
| `doc/IMPLEMENTATION_PLAN_PLUGIN_SYSTEM.md` | Updated - Added CRITICAL Module Architecture section | 2026-01-30 |
| `CLAUDE.md` | Updated - Added ECS/Package Module Architecture section | 2026-01-30 |
| `src/runtime/runtime.cpp` | Added Phase 2 - 7 component registrations, 5 system registrations, RenderContext init | 2026-01-30 |
| `include/void_engine/runtime/runtime.hpp` | Added method declarations for Phase 2 | 2026-01-30 |
| `src/package/plugin_package_loader.cpp` | Phase 3 - IPlugin lifecycle, DLL loading, hot-reload support | 2026-01-30 |
| `include/void_engine/package/loader.hpp` | Added Kernel forward decl and 3-param factory function | 2026-01-30 |
| `include/void_engine/plugin_api/state.hpp` | Phase 4 - PluginState, PluginStatus, PluginRegistry | 2026-01-30 |
| `include/void_engine/plugin_api/fwd.hpp` | Added forward declarations for Phase 4 types | 2026-01-30 |
| `src/runtime/runtime.cpp` | Added PluginRegistry resource insertion | 2026-01-30 |
| `src/package/plugin_package_loader.cpp` | Phase 4 - Added tracking methods and integration | 2026-01-30 |
| `plugins/CMakeLists.txt` | Phase 5 - Created with add_void_plugin() function | 2026-01-30 |
| `plugins/base.health/CMakeLists.txt` | Phase 5 - Build config using add_void_plugin() | 2026-01-30 |
| `plugins/base.health/manifest.plugin.json` | Phase 5 - Plugin manifest with components/systems/events | 2026-01-30 |
| `plugins/base.health/health_plugin.hpp` | Phase 5 - HealthPlugin header with Health/DamageReceiver/Dead | 2026-01-30 |
| `plugins/base.health/health_plugin.cpp` | Phase 5 - Full IPlugin implementation with hot-reload | 2026-01-30 |
| `plugins/example_ai/CMakeLists.txt` | Updated - Output to plugins/bin/, marked as legacy | 2026-01-30 |
| `CMakeLists.txt` | Updated - Use add_subdirectory(plugins) instead of individual plugins | 2026-01-30 |
| `examples/package_demo/packages/assets/demo.characters.bundle.json` | Phase 6 - Added Mesh, Material, Renderable, Health, DamageReceiver to prefabs | 2026-01-30 |
| `examples/package_demo/packages/worlds/world.demo_arena.world.json` | Phase 6 - Added camera section and base.health dependency | 2026-01-30 |
| `examples/package_demo/packages/worlds/scenes/arena.scene.json` | Phase 6 - Added camera entity, updated lights with components, added enemies | 2026-01-30 |
| `src/package/world_composer.cpp` | Phase 6 - Scene spawning (camera, lights, entities via prefabs) | 2026-01-30 |
| `include/void_engine/package/world_composer.hpp` | Phase 6 - Added spawn_camera/light/entity_from_scene methods | 2026-01-30 |
| `include/void_engine/plugin_api/context.hpp` | Build fix - `value` → `value()` method calls | 2026-01-30 |
| `src/package/plugin_package_loader.cpp` | Build fix - SemanticVersion to void_core::Version conversion | 2026-01-30 |
| `src/package/world_composer.cpp` | Build fix - `get_applier()` → `apply_to_entity()` | 2026-01-30 |
| `plugins/base.health/health_plugin.cpp` | Build fix - `value` → `value()` method calls | 2026-01-30 |
| `examples/package_demo/CMakeLists.txt` | Build fix - Added void_plugin_api link dependency | 2026-01-30 |
