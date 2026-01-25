# Header Integration Gaps - Complete Discovery

> **Created**: 2026-01-25
> **Status**: DISCOVERY COMPLETE
> **Total Headers Needing Implementation**: 68

---

## Summary

| Module | Total Headers | Has .cpp | Missing .cpp | Status |
|--------|---------------|----------|--------------|--------|
| IR | 10 | 2 | 8 | 🔴 CRITICAL |
| Presenter | 17 | 5 | 12 | 🔴 CRITICAL |
| Render | 16 | 6 | 10 | 🔴 CRITICAL |
| Asset | 14 | 7 | 7 | 🔴 CRITICAL |
| ECS | 11 | 4 | 7 | 🔴 CRITICAL |
| Compositor | 13 | 6 | 7 | 🟡 HIGH |
| Core | 11 | 3 | 8 | 🟡 HIGH |
| Physics | 14 | 9 | 5 | 🟡 HIGH |
| Services | 7 | 3 | 4 | 🟠 MEDIUM |
| **TOTAL** | **113** | **45** | **68** | - |

---

## Header-Only Modules (Verified OK)

These modules are **intentionally header-only** with template/inline implementations:

| Module | Headers | Status |
|--------|---------|--------|
| Math | 14 | ✅ Templates/inline |
| Memory | 7 | ✅ Allocator templates |
| Structures | 7 | ✅ Container templates |
| Event | 4 | ✅ Lock-free templates |

---

## Priority 1: IR Module (8 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `batch.hpp` | BatchOptimizer, PatchDeduplicator, PatchSplitter | `batch.cpp` |
| `bus.hpp` | PatchBus, AsyncPatchBus | `bus.cpp` |
| `ir.hpp` | Main dispatcher | `ir.cpp` |
| `namespace.hpp` | Namespace management | `namespace.cpp` |
| `patch.hpp` | Patch system, serialization | `patch.cpp` |
| `transaction.hpp` | Transaction management | `transaction.cpp` |
| `validation.hpp` | Validation logic | `validation.cpp` |
| `value.hpp` | Value type system | `value.cpp` |

---

## Priority 2: Presenter Module (12 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `presenter.hpp` | Presenter class | `presenter.cpp` |
| `multi_backend_presenter.hpp` | Multi-backend factory | `multi_backend_presenter.cpp` |
| `swapchain.hpp` | Swapchain management | `swapchain.cpp` |
| `surface.hpp` | Window surface | `surface.cpp` |
| `frame.hpp` | Frame structure | `frame.cpp` |
| `timing.hpp` | Frame timing | `timing.cpp` |
| `rehydration.hpp` | State preservation | `rehydration.cpp` |
| `presenter_module.hpp` | Module integration | `presenter_module.cpp` |
| `backends/null_backend.hpp` | Null backend | `backends/null_backend.cpp` |
| `backends/wgpu_backend.hpp` | WebGPU backend | `backends/wgpu_backend.cpp` |
| `xr/xr_types.hpp` | XR types | `xr/xr_types.cpp` |
| `drm.hpp` | Linux DRM | `drm_presenter.cpp` |

---

## Priority 3: Render Module (10 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `camera.hpp` | Camera system | `camera.cpp` |
| `light.hpp` | Lighting system | `light.cpp` |
| `material.hpp` | Material binding | `material.cpp` |
| `mesh.hpp` | Mesh structures | `mesh.cpp` |
| `pass.hpp` | Render pass | `pass.cpp` |
| `resource.hpp` | GPU resources | `resource.cpp` |
| `shadow.hpp` | Shadow mapping | `shadow.cpp` |
| `debug.hpp` | Debug visualization | `debug.cpp` |
| `texture.hpp` | Texture management | `texture.cpp` |
| `render.hpp` | Main render | `render.cpp` |

---

## Priority 4: Asset Module (7 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `asset.hpp` | Main asset system | `asset.cpp` |
| `cache.hpp` | Asset caching | `cache.cpp` |
| `handle.hpp` | Asset handles | `handle.cpp` |
| `loader.hpp` | Loader interface | `loader.cpp` |
| `server.hpp` | Asset server | `server.cpp` |
| `storage.hpp` | Asset storage | `storage.cpp` |

---

## Priority 5: ECS Module (7 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `archetype.hpp` | Entity grouping | `archetype.cpp` |
| `bundle.hpp` | Component bundles | `bundle.cpp` |
| `component.hpp` | Component storage | `component.cpp` |
| `entity.hpp` | Entity type | `entity.cpp` |
| `hierarchy.hpp` | Parent-child | `hierarchy.cpp` |
| `query.hpp` | Query system | `query.cpp` |

---

## Priority 6: Compositor Module (7 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `frame.hpp` | Frame composition | `frame.cpp` |
| `hdr.hpp` | HDR tone mapping | `hdr.cpp` |
| `output.hpp` | Output management | `output.cpp` |
| `vrr.hpp` | Variable refresh rate | `vrr.cpp` |
| `rehydration.hpp` | State preservation | `rehydration.cpp` |
| `input.hpp` | Input handling | `input.cpp` |
| `compositor_module.hpp` | Module integration | `compositor_module.cpp` |

---

## Priority 7: Core Module (8 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `error.hpp` | Result<T> type | `error.cpp` |
| `handle.hpp` | Handle allocation | `handle.cpp` |
| `id.hpp` | ID with generation | `id.cpp` |
| `log.hpp` | Logging system | `log.cpp` |
| `type_registry.hpp` | RTTI/dynamic types | `type_registry.cpp` |
| `hot_reload.hpp` | Hot-reload manager | `hot_reload.cpp` |
| `plugin.hpp` | Plugin system | `plugin.cpp` |
| `version.hpp` | Version comparison | `version.cpp` |

---

## Priority 8: Physics Module (5 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `broadphase.hpp` | Broad-phase collision | `broadphase.cpp` |
| `collision.hpp` | Collision response | `collision.cpp` |
| `solver.hpp` | Constraint solver | `solver.cpp` |
| `physics.hpp` | Main physics | `physics.cpp` |

---

## Priority 9: Services Module (4 missing)

| Header | Class/Interface | Needs |
|--------|-----------------|-------|
| `event_bus.hpp` | Service event bus | `event_bus.cpp` |
| `service.hpp` | Service base class | `service.cpp` |
| `services.hpp` | Main services | `services.cpp` |

---

## Implementation Prompt

Use `.claude/prompts/implement-headers.md` to implement all 68 headers.

---

## Requirements for Each Implementation

1. **Hot-Reload**: Implement `void_core::HotReloadable` for stateful classes
2. **Snapshots**: Binary serialization with MAGIC and VERSION
3. **Performance**: No allocations in frame loop
4. **Error Handling**: Use `Result<T>` for fallible operations
5. **Interfaces**: Factory functions, dependency injection
