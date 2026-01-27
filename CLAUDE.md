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
