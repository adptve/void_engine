# Module Analysis Index

## Overview

This directory contains comprehensive header-to-implementation analysis for all modules in the void_engine C++ codebase.

**Total Modules Analyzed**: 33
**Analysis Date**: 2026-01-26

## Summary Statistics

| Grade | Count | Modules |
|-------|-------|---------|
| A+ | 18 | engine, ecs, ir, asset, scene, graph, math, memory, structures, ai, combat, gamestate, triggers, shader, event, presenter, kernel, audio |
| A | 2 | kernel, presenter |
| B+ | 4 | core, audio, hud, inventory |
| B | 2 | physics, services |
| C | 1 | compositor |
| D | 2 | render, xr |

## Duplicate Implementation Analysis

A thorough analysis was performed to identify cases where code is duplicated between:
1. Header files (.hpp) and implementation files (.cpp)
2. Multiple .cpp files implementing the same functions

### Findings

| Module | Duplicate Type | Details |
|--------|---------------|---------|
| **render** | cpp-to-cpp | **45+ functions** duplicated between `stub.cpp` and `gl_renderer.cpp`. ALL real implementations are in `gl_renderer.cpp`; stub.cpp should be deleted. |
| All others | None | No header-to-cpp or cpp-to-cpp duplicates found. Codebase correctly uses inline/template in headers only. |

### Header-Only Modules (Intentional Design)

These modules are header-only by design (templates, inline, constexpr):
- ecs, math, memory, structures, event, shader, ai (state_machine.hpp)

## Critical Issues Requiring Immediate Attention

### 1. render (Grade: D)
- **ODR Violation**: **45+ functions** duplicated between stub.cpp and gl_renderer.cpp
- `stub.cpp` contains only empty stubs; `gl_renderer.cpp` has ALL real implementations
- **Recommendation**: Delete `stub.cpp` entirely
- **Missing**: 10 InstanceData methods not implemented
- [View Details](render.md)

### 2. xr (Grade: D)
- **Namespace Mismatch**: Header uses `void_xr`, implementation uses `void_engine::xr`
- **Missing**: 4 snapshot functions not implemented
- [View Details](xr.md)

### 3. compositor (Grade: C)
- **Compile Error**: `NullLayerCompositor::end_frame()` references undeclared variable
- [View Details](compositor.md)

## Modules by Status

### Perfect Consistency
- [engine](engine.md) - Core engine lifecycle
- [ecs](ecs.md) - Entity Component System (header-only)
- [ir](ir.md) - Intermediate Representation & Patches
- [asset](asset.md) - Asset management
- [scene](scene.md) - Scene graph
- [graph](graph.md) - Visual scripting
- [math](math.md) - Math utilities (header-only)
- [memory](memory.md) - Memory allocators (header-only)
- [structures](structures.md) - Data structures (header-only)
- [ai](ai.md) - AI systems
- [combat](combat.md) - Combat system
- [gamestate](gamestate.md) - Game state management
- [triggers](triggers.md) - Trigger volumes
- [shader](shader.md) - Shader types (header-only)
- [event](event.md) - Event system (header-only)

### Minor Issues
- [core](core.md) - Missing `VersionRange::contains()`
- [physics](physics.md) - Abstract `PhysicsDebugRenderer` without concrete impl
- [audio](audio.md) - Orphaned `AudioSource3D` forward declaration
- [services](services.md) - 71 internal functions (acceptable)
- [ui](ui.md) - Missing `create_opengl_renderer()`
- [hud](hud.md) - Easing functions need verification
- [inventory](inventory.md) - Partial review needed

### Critical Issues
- [render](render.md) - ODR violation, missing implementations
- [compositor](compositor.md) - Undeclared variable reference
- [xr](xr.md) - Namespace mismatch, missing implementations

## Module Categories

### Core Systems
| Module | Grade | Notes |
|--------|-------|-------|
| [core](core.md) | B+ | Missing one method |
| [engine](engine.md) | A+ | Perfect |
| [kernel](kernel.md) | A | Minor style note |
| [services](services.md) | B | Internal functions |

### Entity/Data
| Module | Grade | Notes |
|--------|-------|-------|
| [ecs](ecs.md) | A+ | Header-only |
| [ir](ir.md) | A+ | Perfect |
| [scene](scene.md) | A+ | Perfect |

### Rendering
| Module | Grade | Notes |
|--------|-------|-------|
| [render](render.md) | D | Critical issues |
| [compositor](compositor.md) | C | Compile error |
| [presenter](presenter.md) | A- | Internal functions OK |
| [shader](shader.md) | A+ | Header-only |

### Gameplay
| Module | Grade | Notes |
|--------|-------|-------|
| [graph](graph.md) | A+ | Visual scripting |
| [ai](ai.md) | A+ | Header-only FSM |
| [combat](combat.md) | A+ | Perfect |
| [gamestate](gamestate.md) | A+ | Perfect |
| [triggers](triggers.md) | A+ | Perfect |
| [inventory](inventory.md) | B+ | Partial review |

### UI/HUD
| Module | Grade | Notes |
|--------|-------|-------|
| [ui](ui.md) | B | Missing factory |
| [hud](hud.md) | B+ | Verify easing |

### Utilities
| Module | Grade | Notes |
|--------|-------|-------|
| [math](math.md) | A+ | Header-only |
| [memory](memory.md) | A+ | Header-only |
| [structures](structures.md) | A+ | Header-only |
| [event](event.md) | A+ | Header-only |

### Platform/Hardware
| Module | Grade | Notes |
|--------|-------|-------|
| [physics](physics.md) | B | Abstract interface |
| [audio](audio.md) | B+ | Orphaned declaration |
| [asset](asset.md) | A+ | Perfect |
| [xr](xr.md) | D | Critical issues |

## Recommended Priority

1. **Immediate**: Fix `render` and `xr` modules - these have blocking issues
2. **High**: Fix `compositor` compile error
3. **Medium**: Implement missing methods in `core`, `ui`
4. **Low**: Clean up orphaned declarations in `audio`, verify `hud` easing functions

---

## Related Documentation

- **[STUB_REMOVAL_PLAN.md](../STUB_REMOVAL_PLAN.md)** - Comprehensive plan for removing stubs and completing implementations
  - 13 stub files analyzed
  - 1 file to delete (render/stub.cpp)
  - 50+ unimplemented functions catalogued
  - Phased action plan included
