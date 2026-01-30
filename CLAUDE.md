# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## CRITICAL: Code Preservation Rules

**These rules are NON-NEGOTIABLE. Violating them destroys the codebase.**

### 1. NEVER Delete to Fix Builds
- Do NOT comment out code to make things compile
- Do NOT delete complex logic to fix errors
- Do NOT add stubs/placeholders instead of real implementations
- If you don't understand code, ASK - don't delete

### 2. ALWAYS Keep Most Advanced Implementation
When duplicate code exists (stub vs real):
- Compare line counts, features, error handling
- Keep whichever has MORE functionality
- More lines + more features + better error handling = KEEP IT
- MERGE unique features if both have something valuable

### 3. Hot-Reload is SACRED
These patterns MUST be preserved - never remove or stub out:
```cpp
snapshot()           // Captures state for reload
restore()            // Restores state after reload
dehydrate()          // Serializes for persistence
rehydrate()          // Deserializes from persistence
on_reloaded()        // Callback after hot-reload
current_version()    // Version tracking
is_compatible()      // Compatibility checking
```

### 4. No Build-Driven Development
- Do NOT run builds to "see what happens" then chase errors
- UNDERSTAND the code first, then make surgical fixes
- Build errors are INFORMATION, not a to-do list
- Most errors share a root cause - fix that, not symptoms

### 5. Stubs Are Temporary, Integration is Required
- Stub files exist for migration - they should be REPLACED not kept
- Factory functions must return REAL implementations
- Module init() must connect to REAL systems
- If something is bypassed (direct GLFW calls etc), that's a bug to fix

### 6. NEVER GUESS - READ THE CODE
- This is a PRODUCTION-CRITICAL application
- NEVER guess at namespaces, types, function signatures, or APIs
- ALWAYS read the actual headers/source files BEFORE writing code that uses them
- Check `include/void_engine/<module>/<file>.hpp` for the real definitions
- Module namespaces: `void_math`, `void_structures`, `void_core`, etc. (NOT `void_engine::math`)
- If unsure, READ the header first - never assume

### 7. JSON for All Configuration (NOT TOML)
- All config files MUST use JSON format (`.json` extension)
- `manifest.json` for project configuration (NOT manifest.toml)
- `scene.json` for scene definitions (NOT scene.toml)
- Use `nlohmann/json` library for parsing
- JSON enables: schema validation, API consumption, better tooling
- NEVER use TOML, YAML, or other formats for configuration

## MANDATORY: Production-Ready Code Standards

**This is a PRODUCTION ENGINE. Every line of code must be production-ready.**

### Code Quality Requirements

1. **ALWAYS use advanced, complete implementations**
   - Use modern C++20/23 features appropriately (concepts, ranges, coroutines where beneficial)
   - Implement proper RAII for all resource management
   - Use strong typing (no raw pointers without ownership semantics, prefer `std::unique_ptr`/`std::shared_ptr`)
   - Include complete error handling with `Result<T>` types or exceptions where appropriate

2. **NEVER take shortcuts**
   - No `// TODO: implement later` without immediately implementing
   - No placeholder return values (`return {};` or `return nullptr;` as a cop-out)
   - No skipping edge cases - handle ALL cases
   - No "happy path only" code - handle errors, nulls, empty collections, boundary conditions

3. **NEVER use bandaid fixes**
   - Don't add workarounds for symptoms - fix root causes
   - Don't add special-case code that masks underlying issues
   - Don't disable features to avoid bugs - fix the bugs
   - Don't add defensive code that hides logic errors - fix the logic

4. **ALWAYS implement complete functionality**
   - If a function is declared, implement it fully
   - If an interface is defined, implement all methods with real logic
   - If a class is created, make it production-complete (constructors, destructors, move semantics, etc.)
   - If error handling is needed, implement proper recovery or propagation

### Implementation Patterns Required

```cpp
// WRONG: Stub/placeholder
Result<void> load_package(const std::string& path) {
    return Ok(); // TODO: implement
}

// CORRECT: Full implementation
Result<void> load_package(const std::string& path) {
    if (path.empty()) {
        return Error("Package path cannot be empty");
    }

    auto manifest_result = PackageManifest::load(path);
    if (!manifest_result) {
        return Error("Failed to load manifest: " + manifest_result.error());
    }

    auto& manifest = *manifest_result;

    // Validate dependencies
    for (const auto& dep : manifest.dependencies()) {
        if (!is_available(dep.name)) {
            return Error("Missing dependency: " + dep.name);
        }
    }

    // Actually load the package content
    // ... complete implementation ...

    m_loaded_packages[manifest.name()] = std::move(manifest);
    return Ok();
}
```

### Before Writing ANY Code

1. **Read existing code** - Understand the patterns already in use
2. **Check the headers** - Know the actual types and APIs
3. **Plan the implementation** - Think through edge cases BEFORE coding
4. **Consider thread safety** - Is this code accessed from multiple threads?
5. **Consider hot-reload** - Will this survive a reload? Does state need preservation?

### Code Review Checklist (Self-Apply)

Before considering any implementation complete:
- [ ] All code paths return meaningful values (no empty returns)
- [ ] All errors are handled or propagated (no silent failures)
- [ ] All resources are properly managed (RAII, no leaks)
- [ ] All edge cases are handled (empty, null, boundary, overflow)
- [ ] All public APIs have clear contracts (preconditions documented)
- [ ] Thread safety is considered (locks, atomics, or documented as single-threaded)
- [ ] Hot-reload compatibility is preserved (state can snapshot/restore)
- [ ] No magic numbers (use named constants)
- [ ] No copy-paste code (factor into functions)
- [ ] Naming is clear and consistent with codebase conventions

## Build Commands

```bash
# Configure (from project root)
cmake -B build

# Build
cmake --build build

# Run
./build/void_engine      # Linux/macOS
.\build\Debug\void_engine.exe  # Windows
```

## Architecture

void_engine is a C++20 game/render engine using CMake.

**Namespaces:** Each module has its own namespace:
- `void_math` - Math types (Vec3, Mat4, Quat, etc.)
- `void_structures` - Data structures (SlotMap, SparseSet, etc.)
- `void_core` - Core utilities (Handle, Result, Plugin, etc.)
- `void_ecs` - Entity Component System
- `void_package` - Package system (loaders, component schemas, prefabs)
- `void_plugin_api` - Plugin API (IPlugin, PluginContext)
- `void_render` - Rendering
- `void_kernel` - Kernel (stages, system scheduling)
- etc.

**Directory structure:**
- `include/void_engine/` - Public headers (included as `<void_engine/...>`)
- `src/` - Implementation files

**Core class:** `Engine` (`include/void_engine/core/engine.hpp`) - Main engine class with lifecycle methods: `init()`, `run()`, `shutdown()`

**Conventions:**
- Member variables prefixed with `m_`
- Static variables prefixed with `s_`
- Constants prefixed with `k_`
- Non-copyable classes use deleted copy constructor/assignment
- Headers use `#pragma once`

## CRITICAL: ECS and Package Module Architecture

> **This section is NON-NEGOTIABLE. Misunderstanding this causes cascading bugs.**

### The Module Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────┐
│  void_ecs (include/void_engine/ecs/)                                     │
│                                                                          │
│  CORE ECS IMPLEMENTATION (low-level, no JSON knowledge):                 │
│    - World, Entity, Component, Archetype, Query                          │
│    - ComponentRegistry (type registration by C++ type)                   │
│    - Resources, Systems, Snapshots                                       │
│                                                                          │
│  This is JUST data storage. It knows nothing about JSON, packages,       │
│  or plugins. Use void_package for JSON→component conversion.             │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ USES
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  void_package (include/void_engine/package/)                             │
│                                                                          │
│  PACKAGE SYSTEM (uses void_ecs, adds JSON support):                      │
│    - ComponentSchemaRegistry (JSON schemas, factories)                   │
│    - PrefabRegistry (prefab definitions)                                 │
│    - WorldComposer (world loading, entity spawning)                      │
│    - Package loaders (plugin, world, layer, widget, asset)               │
│                                                                          │
│  ╔══════════════════════════════════════════════════════════════════╗   │
│  ║  void_package::ComponentSchemaRegistry IS THE SINGLE SOURCE OF   ║   │
│  ║  TRUTH FOR COMPONENT REGISTRATION WITH JSON FACTORIES.           ║   │
│  ║                                                                  ║   │
│  ║  ALL component factories MUST be registered here.                ║   │
│  ║  DO NOT create parallel factory systems in other modules.        ║   │
│  ╚══════════════════════════════════════════════════════════════════╝   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ USES
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  void_plugin_api (include/void_engine/plugin_api/)                       │
│                                                                          │
│  PLUGIN API (uses void_package for component registration):              │
│    - IPlugin interface (what plugins implement)                          │
│    - PluginContext (what engine provides to plugins)                     │
│    - RenderableDesc (render contract)                                    │
│                                                                          │
│  PluginContext::register_component<T>() DELEGATES TO                     │
│  void_package::ComponentSchemaRegistry - NOT a parallel system.          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Critical Types and Their Modules

| Type | Module | Purpose |
|------|--------|---------|
| `void_ecs::World` | void_ecs | Entity/component storage |
| `void_ecs::Entity` | void_ecs | Entity handle |
| `void_ecs::ComponentId` | void_ecs | Component type identifier |
| `void_package::ComponentSchemaRegistry` | **void_package** | **JSON→component factories (USE THIS)** |
| `void_package::ComponentApplier` | void_package | Function: `(World&, Entity, json) → Result<void>` |
| `void_package::WorldComposer` | void_package | World loading, uses ComponentSchemaRegistry |
| `void_plugin_api::PluginContext` | void_plugin_api | Delegates to ComponentSchemaRegistry |

### NEVER DO THIS

```cpp
// WRONG: Creating parallel factory systems
class SomeModule {
    std::unordered_map<std::string, ComponentFactory> m_factories;  // NO!
};

// WRONG: Bypassing ComponentSchemaRegistry for JSON loading
world.register_component<MyComp>();  // Only ECS registration, no JSON support!
```

### ALWAYS DO THIS

```cpp
// CORRECT: Use ComponentSchemaRegistry for JSON→component
void_package::ComponentSchemaRegistry& registry = ...;
registry.register_schema_with_factory(schema, nullptr, applier);

// CORRECT: PluginContext delegates to schema registry
class PluginContext {
    void_package::ComponentSchemaRegistry* m_schema_registry;

    template<typename T>
    ComponentId register_component(const std::string& name, ComponentApplier applier) {
        m_schema_registry->register_schema_with_factory(schema, nullptr, applier);
    }
};
```

### Why This Matters

- **WorldComposer** uses ComponentSchemaRegistry to spawn prefabs
- **Hot-reload** uses schema registry for component migration
- **Plugin isolation** - registry tracks which plugin registered which component
- **JSON validation** - schemas validate data before applying

**If you bypass ComponentSchemaRegistry, prefabs won't load correctly.**

## Specialized Skills

Reference these skill guides in `.claude/skills/` for domain expertise:

| Skill | File | Use For |
|-------|------|---------|
| C++ Expert | `cpp-expert.md` | Modern C++20/23, memory, templates |
| Rust Expert | `rust-expert.md` | Rust code, FFI with C++ |
| Game Engine | `game-engine.md` | ECS, rendering, game loops |
| Hot-Reload | `hot-reload.md` | DLL/SO hot swapping, state preservation |
| Architecture | `architecture.md` | Module design, dependencies, SOLID |
| Functional | `functional.md` | Pure functions, ranges, pipelines |
| Refactoring | `refactor.md` | Code restructuring, legacy code |
| Migration | `migrate.md` | API transitions, upgrades |
| Best Practices | `best-practices.md` | Code quality, naming, RAII |
| Unit Testing | `unit-test.md` | Catch2, mocking, TDD |

**Orchestrator:** `.claude/agents/orchestrator.md` - Coordinates skills for complex tasks

## Key Documentation

Before implementing major features, READ these documents:

| Document | Path | Purpose |
|----------|------|---------|
| ECS Architecture | `doc/ecs/ECS_COMPREHENSIVE_ARCHITECTURE.md` | Complete ECS system reference |
| Package System | `doc/ecs/PACKAGE_SYSTEM.md` | Package types, dependencies, contracts |
| Package Migration | `doc/ecs/PACKAGE_SYSTEM_MIGRATION.md` | Implementation plan, phases, checklist |

### Plugin System Documentation (CRITICAL)

**Read these IN ORDER when working on the plugin/package system:**

| Document | Path | Purpose |
|----------|------|---------|
| **Progress Tracker** | `doc/PLUGIN_SYSTEM_PROGRESS.md` | **CHECK FIRST** - Living checklist, current status |
| Plugin Implementation Plan | `doc/IMPLEMENTATION_PLAN_PLUGIN_SYSTEM.md` | Tasks, acceptance criteria, verification |
| Plugin-Engine Contract | `doc/PLUGIN_ENGINE_CONTRACT.md` | IPlugin interface, PluginContext, render contract |
| Architecture Vision | `doc/PACKAGE_DRIVEN_ARCHITECTURE_VISION.md` | Why plugin-first, two-layer model |
| Architecture Deep Dive | `doc/VOID_ENGINE_ARCHITECTURE_DEEP_DIVE.md` | All 37 modules, frame loop, gaps |

### Progress Tracking Protocol

1. **Before starting work**: Read `PLUGIN_SYSTEM_PROGRESS.md` to see current phase/task
2. **When starting a task**: Mark it 🟡 In Progress, add session date to Session Log
3. **When completing items**: Check off [ ] → [x] for each acceptance criteria
4. **When completing a task**: Mark it ✅ Complete, update notes
5. **If blocked**: Mark ❌ Blocked, add to Blockers table
6. **Track file changes**: Add modified files to File Change Log

## CORE IDEOLOGY (READ THIS FIRST AFTER COMPACTION)

### Why This Engine Exists

**This engine exists for ONE purpose: external creators load plugins that define complete game experiences remotely at runtime.**

This is NOT a traditional game engine where you compile game code into the engine.
This IS a platform where:
- Games are defined entirely by plugins (DLLs loaded at runtime)
- Plugins can be hot-reloaded without restarting
- External creators build content without engine source access
- Everything is data-driven and remotely deployable

**If you find yourself writing game logic in the engine, STOP. That belongs in a plugin.**

### The Two-Layer Architecture

```
LAYER 1: ENGINE (Infrastructure Only)
├── Renderer (OpenGL/Vulkan, shaders, draw calls)
├── ECS World (entity storage, component registry)
├── Kernel (stage scheduler, system execution)
├── Asset System (loading, caching, hot-reload)
├── Core Components (Transform, Mesh, Material, Light, Camera)
├── Core Systems (TransformSystem, RenderPrepareSystem, RenderSystem)
└── Plugin API (IPlugin interface, PluginContext)

LAYER 2: PLUGINS (All Game Logic)
├── Game Components (Health, Weapon, Enemy, Inventory - via DLLs)
├── Game Systems (CombatSystem, AISystem - via DLLs)
├── Event Handlers (game-specific behavior - via DLLs)
└── State (tracked as ECS resources, survives hot-reload)
```

### The Contract

Plugins and engine communicate through a strict contract:

1. **IPlugin Interface** - Every plugin implements this (on_load, on_unload, snapshot, restore)
2. **PluginContext** - Engine provides this to plugins (registration APIs, ECS access)
3. **make_renderable()** - Plugins request rendering, engine adds render components
4. **Component Factories** - Plugins provide JSON→Component conversion functions
5. **System Registration** - Plugins register systems that run in kernel stages

### Plugin State Management (True ECS)

Plugins are first-class ECS citizens:
- Plugin metadata stored as ECS resources (PluginRegistry)
- Registered components/systems tracked for cleanup
- Entity ownership tracked (what each plugin spawned)
- State survives hot-reload via snapshot/restore
- No global state in plugins - everything in ECS

### Non-Negotiable Plugin Rules

1. **Plugins are DLLs** - JSON manifests describe metadata, DLLs provide code
2. **Plugins define components** - C++ structs with JSON factories for instantiation
3. **Plugins register systems** - Functions registered with kernel stages
4. **Plugin state is ECS** - All state as resources, survives hot-reload
5. **Engine provides render contract** - Plugins call make_renderable(), engine handles GPU
6. **No game logic in engine** - Engine is INFRASTRUCTURE ONLY
7. **Shared Transform** - Plugins use engine's TransformComponent (one source of truth)
8. **Hot-reload mandatory** - Every plugin MUST implement snapshot/restore
9. **No direct GPU access** - Plugins use render contract, not GL/Vulkan calls
10. **No bypassing contract** - Plugins use PluginContext, not internal engine APIs

### What To Do When Implementing

1. **Check PLUGIN_SYSTEM_PROGRESS.md** - See current task and status
2. **Read the task's acceptance criteria** - Every checkbox must be checked
3. **Implement completely** - No stubs, no TODOs, no placeholders
4. **Update progress file** - Mark checkboxes, add notes, log files changed
5. **Consider hot-reload** - Will this survive a plugin DLL swap?
6. **Consider the contract** - Is this plugin-side or engine-side?

### What NOT To Do

- ❌ Write game-specific code in engine (put it in a plugin)
- ❌ Create stubs "to be implemented later"
- ❌ Bypass PluginContext with direct engine access
- ❌ Skip snapshot/restore in plugin implementations
- ❌ Hardcode component types that should be plugin-defined
- ❌ Put render logic in plugins (use make_renderable contract)
- ❌ Create global state outside ECS
- ❌ Forget to track progress in PLUGIN_SYSTEM_PROGRESS.md

**When implementing package/plugin system features:**
- Follow the phase structure in IMPLEMENTATION_PLAN_PLUGIN_SYSTEM.md
- Check off acceptance criteria as you complete them
- Update PLUGIN_SYSTEM_PROGRESS.md with status and notes
- Respect dependency rules (core → engine → gameplay → feature → mod)
- Implement complete loaders with DLL support, not stubs
- All plugins MUST implement IPlugin interface with snapshot/restore
