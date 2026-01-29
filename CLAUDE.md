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
- `void_render` - Rendering
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

**When implementing package system features:**
- Follow the phase structure in PACKAGE_SYSTEM_MIGRATION.md
- Use the exact JSON schemas defined in PACKAGE_SYSTEM.md
- Respect dependency rules (core → engine → gameplay → feature → mod)
- Implement complete loaders, not stubs
