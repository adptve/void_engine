# Validation Reports

This directory contains validation reports for each implemented module.

## Required Reports

| File | Module | Status |
|------|--------|--------|
| `ir_validation.md` | void_ir | ⬜ TODO |
| `presenter_validation.md` | void_presenter | ⬜ TODO |
| `render_validation.md` | void_render | ⬜ TODO |
| `asset_validation.md` | void_asset | ⬜ TODO |
| `ecs_validation.md` | void_ecs | ⬜ TODO |
| `compositor_validation.md` | void_compositor | ⬜ TODO |
| `core_validation.md` | void_core | ⬜ TODO |
| `physics_validation.md` | void_physics | ⬜ TODO |
| `services_validation.md` | void_services | ⬜ TODO |

## Validation Categories

Each report must cover:

### 1. Hot-Reload Validation
- Every HotReloadable class passes snapshot→restore→verify cycle
- Snapshots are binary (no JSON/text)
- Magic numbers are unique
- Version numbers set
- No raw pointers in serialized data

### 2. Performance Validation
- No allocations in update()/tick()/render() paths
- Pre-allocated buffers verified
- No virtual calls in inner loops
- Cache-friendly data layout

### 3. Integration Validation
- Module registers with engine
- Module participates in hot-reload cycle
- Module handles init/shutdown cleanly
- No circular dependencies

### 4. Build Validation
- Compiles without warnings
- Links without undefined symbols
- CMakeLists.txt updated

## Report Template

```markdown
# [Module Name] Validation Report

## Implementation Status
- Total headers: X
- Implemented: X
- Remaining: 0

## Hot-Reload Validation
| Class | snapshot() | restore() | Cycle Test | Status |
|-------|------------|-----------|------------|--------|

## Performance Validation
| Test | Result | Notes |
|------|--------|-------|

## Integration Validation
| Test | Result |
|------|--------|

## Build Validation
| Check | Result |
|-------|--------|
```
