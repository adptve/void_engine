# Parallel Implementation Strategy

> **Run sessions in PHASES** based on dependencies
> **Each phase can run in parallel** - modules in same phase don't depend on each other
> **Wait for phase to complete** before starting next phase

---

## DEPENDENCY GRAPH

```
TIER 0 (Foundation - No dependencies):
├── core ─────────────────────────┐
├── ir                            │
├── math (header-only ✓)          │
└── structures (header-only ✓)    │
                                  │
TIER 1 (Depends on TIER 0): ◄─────┘
├── ecs (needs: core, structures)
├── asset (needs: core)
├── physics (needs: core, math)
├── services (needs: core)
└── presenter (minimal deps)
                                  │
TIER 2 (Depends on TIER 0-1): ◄───┘
├── render (needs: core, asset)
└── compositor (needs: presenter)
```

---

## EXECUTION PHASES

### Phase 1: Foundation (2 parallel sessions)

| Terminal | Module | Headers | Dependencies |
|----------|--------|---------|--------------|
| 1 | **core** | 8 | None |
| 2 | **ir** | 8 | None |

**Wait for Phase 1 to complete before starting Phase 2!**

```bash
# After Phase 1, verify:
cmake -B build && cmake --build build
# Fix any errors before proceeding
```

---

### Phase 2: Core Systems (5 parallel sessions)

| Terminal | Module | Headers | Dependencies |
|----------|--------|---------|--------------|
| 3 | **ecs** | 7 | core, structures |
| 4 | **asset** | 7 | core |
| 5 | **physics** | 5 | core, math |
| 6 | **services** | 4 | core |
| 7 | **presenter** | 12 | minimal |

**Wait for Phase 2 to complete before starting Phase 3!**

```bash
# After Phase 2, verify:
cmake -B build && cmake --build build
# Fix any errors before proceeding
```

---

### Phase 3: Rendering Pipeline (2 parallel sessions)

| Terminal | Module | Headers | Dependencies |
|----------|--------|---------|--------------|
| 8 | **render** | 10 | core, asset |
| 9 | **compositor** | 7 | presenter |

```bash
# After Phase 3, final build:
cmake -B build && cmake --build build
```

---

## COMPLETE EXECUTION TIMELINE

```
TIME ──────────────────────────────────────────────────────►

Phase 1:  [core]────────────►
          [ir]──────────────►
                              │
                              ▼ Build & Verify
Phase 2:                      [ecs]─────────►
                              [asset]───────►
                              [physics]─────►
                              [services]────►
                              [presenter]───►
                                             │
                                             ▼ Build & Verify
Phase 3:                                     [render]─────►
                                             [compositor]─►
                                                           │
                                                           ▼ Final Build
```

---

## HOW TO RUN

### Phase 1: Open 2 Terminals

**Terminal 1:**
```bash
cd /path/to/void_engine
claude
# Paste: .claude/prompts/modules/implement-core.md
```

**Terminal 2:**
```bash
cd /path/to/void_engine
claude
# Paste: .claude/prompts/modules/implement-ir.md
```

**Wait for both to complete, then build:**
```bash
cmake -B build && cmake --build build
```

---

### Phase 2: Open 5 Terminals

**Terminal 3:** `implement-ecs.md`
**Terminal 4:** `implement-asset.md`
**Terminal 5:** `implement-physics.md`
**Terminal 6:** `implement-services.md`
**Terminal 7:** `implement-presenter.md`

**Wait for all 5 to complete, then build:**
```bash
cmake -B build && cmake --build build
```

---

### Phase 3: Open 2 Terminals

**Terminal 8:** `implement-render.md`
**Terminal 9:** `implement-compositor.md`

**Final build:**
```bash
cmake -B build && cmake --build build
```

---

## INTEGRATION POINTS

### How Modules Connect

Each module uses **headers** from its dependencies:

```cpp
// In src/ecs/archetype.cpp
#include <void_engine/core/hot_reload.hpp>  // Uses core's interface
#include <void_engine/structures/bitset.hpp>  // Uses structures

// The IMPLEMENTATIONS of core and structures must exist
// before ECS can LINK (not just compile)
```

### Why Phased Execution Works

1. **Phase 1 completes** → core.cpp, ir.cpp exist and compile
2. **Phase 2 starts** → ecs.cpp can now link against core
3. **Phase 2 completes** → asset.cpp exists
4. **Phase 3 starts** → render.cpp can now link against asset

### What Could Go Wrong

| Problem | Solution |
|---------|----------|
| Phase 2 starts before Phase 1 done | ECS won't link - missing core symbols |
| render starts before asset done | render won't link - missing asset symbols |
| Two sessions edit same file | Only happens if you assign wrong module |

---

## RULES FOR EACH SESSION

### Rule 1: Only Touch Your Module
```
Session for ECS:
✓ Can edit: src/ecs/*, include/void_engine/ecs/*
✗ Cannot edit: anything else
```

### Rule 2: Use Other Modules' Headers (Don't Implement)
```cpp
// In your module's .cpp:
#include <void_engine/core/hot_reload.hpp>  // ✓ OK to include
// Do NOT implement HotReloadable - that's core's job
```

### Rule 3: List CMake Changes, Don't Apply
```
At end of session, output:
"Add to src/ecs/CMakeLists.txt:
target_sources(void_ecs PRIVATE
    archetype.cpp
    ...
)"
```

### Rule 4: Document Cross-Module Calls
```
At end of session, output:
"Uses from other modules:
- void_core::HotReloadable::snapshot()
- void_structures::Bitset
- void_asset::AssetHandle
"
```

---

## TIME ESTIMATES

| Phase | Sessions | Est. Time |
|-------|----------|-----------|
| Phase 1 | 2 parallel | ~2 hours |
| Build & Verify | - | ~10 min |
| Phase 2 | 5 parallel | ~3 hours |
| Build & Verify | - | ~10 min |
| Phase 3 | 2 parallel | ~2 hours |
| Final Build | - | ~10 min |
| **TOTAL** | 9 sessions | **~7-8 hours** |

(Assumes sessions run simultaneously within each phase)

---

## TRACKING PROGRESS

Create a local file to track:

```markdown
# Implementation Progress

## Phase 1 (Foundation)
- [ ] core - Terminal 1 - Started: ___ Completed: ___
- [ ] ir - Terminal 2 - Started: ___ Completed: ___
- [ ] Phase 1 Build: ___

## Phase 2 (Core Systems)
- [ ] ecs - Terminal 3 - Started: ___ Completed: ___
- [ ] asset - Terminal 4 - Started: ___ Completed: ___
- [ ] physics - Terminal 5 - Started: ___ Completed: ___
- [ ] services - Terminal 6 - Started: ___ Completed: ___
- [ ] presenter - Terminal 7 - Started: ___ Completed: ___
- [ ] Phase 2 Build: ___

## Phase 3 (Rendering)
- [ ] render - Terminal 8 - Started: ___ Completed: ___
- [ ] compositor - Terminal 9 - Started: ___ Completed: ___
- [ ] Final Build: ___

## Validation
- [ ] All diagrams created
- [ ] All validation reports created
- [ ] Hot-reload tested
- [ ] Performance validated
```

---

## AFTER ALL PHASES COMPLETE

### 1. Merge CMakeLists.txt Changes
Each session outputs CMake additions. Merge them all.

### 2. Full Rebuild
```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### 3. Create Full Integration Diagram
```bash
# Create doc/diagrams/full_integration.md showing all modules
```

### 4. Run Engine
```bash
./build/void_engine
```

### 5. Test Hot-Reload
Trigger hot-reload while running, verify state survives.
