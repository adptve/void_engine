# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

**Namespace:** All code lives under `void_engine`

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
