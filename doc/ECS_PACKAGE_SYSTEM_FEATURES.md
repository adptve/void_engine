# ECS Package System: Features, Runtime Usage, and Live Content Flow

## Scope

This document describes the ECS package system as implemented **in code**, with an emphasis on runtime (hot) loading of external creator content while the engine continues running. All statements are backed by current headers and runtime wiring.

---

## Core Premise (Runtime-Loadable ECS Extensions)

The package system is designed to let external creators ship new **entities, components, and systems** as packages that are discovered, loaded, and applied **at runtime** (not build time). The runtime initialization path wires up the package registry, loaders, and ECS world so packages can be scanned and loaded while the engine is running.【F:src/runtime/runtime.cpp†L436-L604】

Key runtime behaviors that support this:
- **Package discovery** via `PackageRegistry::scan_directory()` (manifest scanning for world/layer/plugin/widget/asset bundles).【F:include/void_engine/package/registry.hpp†L41-L86】
- **Runtime load/unload/reload** APIs, including hot-reload hooks (`reload_package()` and change detection).【F:include/void_engine/package/registry.hpp†L88-L168】
- **Runtime ECS component registration** via `ComponentSchemaRegistry`, which bridges JSON-defined components to ECS component IDs and instance creation at runtime.【F:include/void_engine/package/component_schema.hpp†L1-L206】

---

## Package System Architecture (Code-Level)

### 1) Package Registry (Discovery + Lifecycle)

`PackageRegistry` is the central discovery and lifecycle manager:
- Scans directories for manifests (`*.world.json`, `*.plugin.json`, `*.widget.json`, `*.layer.json`, `*.bundle.json`).【F:include/void_engine/package/registry.hpp†L41-L86】
- Loads/unloads packages and dependencies through type-specific loaders using a `LoadContext`.【F:include/void_engine/package/registry.hpp†L88-L148】
- Supports reload and change detection for hot-reload workflows.【F:include/void_engine/package/registry.hpp†L150-L170】

### 2) LoadContext + PackageLoader

`LoadContext` provides loader access to engine systems (ECS world, event bus, and other services) during package load/unload operations, and maintains loader registration by package type.【F:include/void_engine/package/loader.hpp†L26-L116】

`PackageLoader` is the interface for package-type loaders, with explicit support for unload and (optional) hot-reload semantics.【F:include/void_engine/package/loader.hpp†L118-L214】

### 3) WorldComposer (World-Orchestrated Load Order)

`WorldComposer` drives world loading via a structured boot sequence:
- Resolve dependencies → load asset bundles → load plugins → load widgets → stage layers → parse world manifest → spawn scene entities → apply layers → initialize resources → start scheduler → emit world events.【F:include/void_engine/package/world_composer.hpp†L13-L82】【F:include/void_engine/package/world_composer.hpp†L260-L356】
- Supports unload and switch semantics, providing the runtime basis for world swapping without shutting down the engine.【F:include/void_engine/package/world_composer.hpp†L359-L441】

### 4) ComponentSchemaRegistry (Runtime Component Registration)

`ComponentSchemaRegistry` is the runtime bridge between JSON component definitions and ECS component IDs:
- Registers components by schema and associates factories/appliers for JSON → component application at runtime.【F:include/void_engine/package/component_schema.hpp†L137-L210】
- Explicitly integrates with `void_ecs::ComponentRegistry` via `set_ecs_registry()` so new components receive valid runtime IDs.【F:include/void_engine/package/component_schema.hpp†L212-L236】

### 5) PrefabRegistry (Runtime Entity Instantiation)

Prefabs store component data by **name** (not by C++ type), enabling entities built from externally registered component schemas:
- Prefab instantiation resolves components at runtime through schema registry and/or instantiators, allowing new component types defined by plugins to be used immediately.【F:include/void_engine/package/prefab_registry.hpp†L1-L160】【F:include/void_engine/package/prefab_registry.hpp†L200-L310】

### 6) DefinitionRegistry (Data-Driven Registries)

The definition registry stores arbitrary JSON definitions by registry type and ID, enabling plugin-defined data registries that are filled by asset bundles at runtime (e.g., weapons, abilities).【F:include/void_engine/package/definition_registry.hpp†L1-L120】

### 7) Plugin Packages (Runtime ECS Extension Surface)

Plugin packages declare ECS surface area **in JSON** (components, systems, event handlers, registries), and can reference dynamic libraries for system execution:
- `ComponentDeclaration`, `SystemDeclaration`, and `EventHandlerDeclaration` define runtime component/system/handler metadata.【F:include/void_engine/package/plugin_package.hpp†L23-L160】
- The manifest can include library paths and entry points to link runtime systems in shared libraries (DLL/SO).【F:include/void_engine/package/plugin_package.hpp†L82-L160】

---

## Runtime Wiring (How the Engine Enables Live Loading)

### Initialization Flow
`Runtime::init_packages()` configures and wires the entire package system at startup:
- Creates the registry, load context, world composer, prefab registry, schema registry, definition registry, widget manager, and layer applier.
- Creates a dedicated ECS world for package-managed entities.
- Connects schema registry to the ECS component registry and prefab registry to the schema registry for runtime JSON → component instantiation.
- Registers package loaders (plugin, widget, layer, world, asset bundle).
- Scans content and plugin paths to discover packages on disk at runtime.

All of these steps are performed without requiring a build step or engine restart.【F:src/runtime/runtime.cpp†L436-L604】

### World Loading
`Runtime::load_world()` uses `WorldComposer::load_world()` with `WorldLoadOptions`, ensuring that plugin components/systems are registered before prefab-based world entities are instantiated.【F:src/runtime/runtime.cpp†L318-L414】

---

## Runtime Usage Flow (External Creator Content)

1. **Creators ship package manifests + content**
   - Packages are authored as JSON manifests (world, plugin, asset bundle, widget, layer).【F:include/void_engine/package/registry.hpp†L41-L86】
2. **Engine scans for packages at runtime**
   - `PackageRegistry::scan_directory()` discovers packages from content paths and plugin paths during runtime initialization or later scanning passes.【F:include/void_engine/package/registry.hpp†L41-L86】【F:src/runtime/runtime.cpp†L532-L604】
3. **Plugin packages register new ECS components/systems**
   - Plugin manifests declare components/systems; schema registration allocates ECS component IDs dynamically via `ComponentSchemaRegistry` and the ECS registry it is connected to.【F:include/void_engine/package/plugin_package.hpp†L23-L160】【F:include/void_engine/package/component_schema.hpp†L137-L236】
4. **Asset bundles provide prefabs/data**
   - Prefabs reference component names as JSON, and instantiation uses the schema registry to apply components to entities at runtime.【F:include/void_engine/package/prefab_registry.hpp†L1-L160】【F:include/void_engine/package/prefab_registry.hpp†L200-L310】
5. **World load composes everything**
   - `WorldComposer` enforces load order: asset bundles → plugins → widgets → layers → world scene instantiation, ensuring components are available before entities are created.【F:include/void_engine/package/world_composer.hpp†L13-L82】【F:include/void_engine/package/world_composer.hpp†L260-L356】

This flow preserves the app’s runtime-on premise: the engine stays up while new content is discovered, loaded, and instantiated.

---

## Feature Summary (ECS Package System)

- **Runtime component registration** (schema-driven, JSON → ECS) via `ComponentSchemaRegistry` + ECS registry integration.【F:include/void_engine/package/component_schema.hpp†L137-L236】
- **Runtime prefab instantiation** from JSON component data via `PrefabRegistry`.【F:include/void_engine/package/prefab_registry.hpp†L1-L310】
- **Runtime data registries** for arbitrary game data via `DefinitionRegistry`.【F:include/void_engine/package/definition_registry.hpp†L1-L120】
- **Package discovery + dependency resolution** via `PackageRegistry` and manifest scanning rules.【F:include/void_engine/package/registry.hpp†L41-L170】
- **Ordered world boot** and runtime world switching via `WorldComposer` (asset bundles → plugins → widgets → layers → world).【F:include/void_engine/package/world_composer.hpp†L13-L82】【F:include/void_engine/package/world_composer.hpp†L260-L441】
- **Plugin-defined ECS extensions** via plugin manifests (components, systems, events, registries, dynamic libraries).【F:include/void_engine/package/plugin_package.hpp†L23-L160】
- **Runtime loader infrastructure** via `LoadContext` and `PackageLoader` for pluggable package types and hot reload support.【F:include/void_engine/package/loader.hpp†L26-L214】

---

## Operational Notes (Always-On Runtime)

- The package system is structured to support **live discovery, load, unload, and reload** of creator content through registry APIs without stopping the engine loop.【F:include/void_engine/package/registry.hpp†L88-L170】
- The runtime already wires this system during initialization, ensuring the engine can stay online while creators deliver new ECS content via packages that are scanned and loaded during runtime operations.【F:src/runtime/runtime.cpp†L436-L604】
