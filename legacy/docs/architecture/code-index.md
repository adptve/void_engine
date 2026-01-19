# Void GUI v2 - Queryable Code Index

This document provides a queryable, exportable index of the codebase organized by various dimensions.

---

## Crate Index

| ID | Crate | Layer | LOC* | Purpose |
|----|-------|-------|------|---------|
| C01 | void_core | Foundation | - | Plugin system, type registry, hot-reload |
| C02 | void_math | Foundation | - | Math primitives (vectors, matrices) |
| C03 | void_memory | Foundation | - | Custom allocators |
| C04 | void_structures | Foundation | - | Lock-free data structures |
| C05 | void_event | Foundation | - | Event bus system |
| C06 | void_ecs | Storage | - | Archetype-based ECS |
| C07 | void_ir | Communication | - | IR patches & transactions |
| C08 | void_asset | Resources | - | Hot-reloadable assets |
| C09 | void_render | Graphics | - | Render graph abstraction |
| C10 | void_shader | Graphics | - | Naga shader compilation |
| C11 | void_ui | Graphics | - | Immediate-mode UI |
| C12 | void_presenter | Graphics | - | Platform output adapters |
| C13 | void_compositor | Graphics | - | Wayland compositor (Smithay) |
| C14 | void_engine | Framework | - | Application orchestration |
| C15 | void_kernel | Core | - | Never-dying kernel |
| C16 | void_graph | Scripting | - | Visual scripting |
| C17 | void_xr | Platform | - | XR/VR/AR abstraction |
| C18 | void_runtime | Entry | - | Main process entry |
| C19 | void_services | Services | - | Async service registry |
| C20 | void_asset_server | Services | - | Hot-reload file server |
| C21 | void_scripting | Scripting | - | WASM plugin runtime |
| C22 | void_script | Scripting | - | VoidScript language |
| C23 | void_shell | Interface | - | vsh REPL |
| C24 | void_editor | Interface | - | Visual editor (egui) |

---

## Trait Index

| ID | Trait | Crate | File | Purpose |
|----|-------|-------|------|---------|
| T01 | `Plugin` | void_core | plugin.rs | Base trait for all plugins |
| T02 | `HotReloadable` | void_core | hot_reload.rs | Hot-swap state management |
| T03 | `Component` | void_ecs | component.rs | ECS component marker |
| T04 | `Bundle` | void_ecs | bundle.rs | Component grouping |
| T05 | `QueryFilter` | void_ecs | query.rs | Query filtering |
| T06 | `Service` | void_services | service.rs | Async service lifecycle |
| T07 | `Presenter` | void_presenter | lib.rs | Output adapter interface |
| T08 | `AssetLoader` | void_asset | loader.rs | Asset deserialization |
| T09 | `Node` | void_graph | node.rs | Visual script node |

---

## Struct Index

| ID | Struct | Crate | File | Purpose |
|----|--------|-------|------|---------|
| S01 | `PluginRegistry` | void_core | plugin.rs | Plugin container |
| S02 | `TypeRegistry` | void_core | registry.rs | Runtime type info |
| S03 | `PluginContext` | void_core | plugin.rs | Plugin initialization context |
| S04 | `World` | void_ecs | world.rs | Central ECS storage |
| S05 | `Archetype` | void_ecs | archetype.rs | Component storage group |
| S06 | `EntityAllocator` | void_ecs | entity.rs | Generational ID allocator |
| S07 | `ComponentRegistry` | void_ecs | component.rs | Component type mapping |
| S08 | `Patch` | void_ir | patch.rs | Single state change |
| S09 | `Transaction` | void_ir | transaction.rs | Atomic patch group |
| S10 | `PatchBus` | void_ir | bus.rs | Patch queue manager |
| S11 | `TransactionBuilder` | void_ir | transaction.rs | Transaction constructor |
| S12 | `Kernel` | void_kernel | lib.rs | Main kernel struct |
| S13 | `KernelConfig` | void_kernel | config.rs | Kernel settings |
| S14 | `SupervisorTree` | void_kernel | supervision.rs | Fault tolerance tree |
| S15 | `Supervisor` | void_kernel | supervision.rs | Child supervisor |
| S16 | `Capability` | void_kernel | capability.rs | Security token |
| S17 | `Namespace` | void_kernel | namespace.rs | App isolation boundary |
| S18 | `ResourceBudget` | void_kernel | sandbox.rs | App resource limits |
| S19 | `Layer` | void_kernel | layer.rs | Render layer |
| S20 | `RenderGraph` | void_render | graph.rs | Declarative render passes |
| S21 | `ShaderPipeline` | void_shader | pipeline.rs | Shader compilation |
| S22 | `AssetServer` | void_asset_server | lib.rs | File watching server |
| S23 | `ServiceRegistry` | void_services | registry.rs | Service container |

---

## Enum Index

| ID | Enum | Crate | File | Purpose |
|----|------|-------|------|---------|
| E01 | `PluginStatus` | void_core | plugin.rs | Plugin lifecycle states |
| E02 | `PatchKind` | void_ir | patch.rs | Patch type discriminator |
| E03 | `EntityPatch` | void_ir | patch.rs | Entity operations |
| E04 | `ComponentPatch` | void_ir | patch.rs | Component operations |
| E05 | `LayerPatch` | void_ir | patch.rs | Layer operations |
| E06 | `AssetPatch` | void_ir | patch.rs | Asset operations |
| E07 | `RestartStrategy` | void_kernel | supervision.rs | Supervisor restart modes |
| E08 | `CapabilityKind` | void_kernel | capability.rs | Permission types |
| E09 | `RecoveryResult` | void_kernel | recovery.rs | Recovery outcomes |
| E10 | `LayerType` | void_kernel | layer.rs | Layer categories |
| E11 | `BlendMode` | void_kernel | layer.rs | Layer blending |
| E12 | `ServiceState` | void_services | service.rs | Service lifecycle |
| E13 | `Backend` | void_runtime | config.rs | Output backends |
| E14 | `HotSwapKind` | void_kernel | hot_swap.rs | Hot-swap targets |

---

## Pattern Index

| ID | Pattern | Primary Crate | Key Types | Description |
|----|---------|---------------|-----------|-------------|
| P01 | Plugin System | void_core | `Plugin`, `PluginRegistry` | Dynamic component loading |
| P02 | Type Registry | void_core | `TypeRegistry`, `TypeInfo` | Runtime type introspection |
| P03 | Hot Reload | void_core | `HotReloadable`, `PluginState` | State-preserving reload |
| P04 | Archetype ECS | void_ecs | `World`, `Archetype` | Cache-efficient storage |
| P05 | Generational IDs | void_ecs | `EntityId`, `Generation` | Use-after-free prevention |
| P06 | IR Patches | void_ir | `Patch`, `PatchKind` | Declarative state changes |
| P07 | Transactions | void_ir | `Transaction`, `TransactionBuilder` | Atomic patch groups |
| P08 | Patch Bus | void_ir | `PatchBus`, `Namespace` | Namespaced patch queuing |
| P09 | Supervision Tree | void_kernel | `SupervisorTree`, `Supervisor` | Erlang OTP fault tolerance |
| P10 | Capability Security | void_kernel | `Capability`, `CapabilityKind` | seL4-style permissions |
| P11 | Namespace Isolation | void_kernel | `Namespace`, `EntityRef` | App isolation |
| P12 | Resource Budgets | void_kernel | `ResourceBudget`, `Sandbox` | Resource limiting |
| P13 | Layer Composition | void_kernel | `Layer`, `LayerType` | Multi-app rendering |
| P14 | Service Lifecycle | void_services | `Service`, `ServiceState` | Async service management |
| P15 | Presenter Adapter | void_presenter | `Presenter` | Output abstraction |

---

## Hot-Swappable Components

| ID | Component | Crate | Mechanism | State Preserved |
|----|-----------|-------|-----------|-----------------|
| H01 | Plugins | void_core | `hot_reload` feature | ✅ Via `PluginState` |
| H02 | Assets | void_asset | File watching | ✅ Handle stable |
| H03 | Shaders | void_shader | Naga recompile | ✅ Frame-boundary |
| H04 | Services | void_services | Lifecycle API | ✅ Graceful restart |
| H05 | Modules | void_kernel | `HotSwapManager` | ✅ Snapshot/restore |
| H06 | Layers | void_kernel | Layer composition | ✅ Instant swap |
| H07 | Renderers | void_presenter | Backend switch | ✅ Runtime switch |

---

## Security Boundaries

| ID | Boundary | Enforcement | Crate |
|----|----------|-------------|-------|
| B01 | Namespace | Entity ID includes namespace | void_ir |
| B02 | Capabilities | Unforgeable tokens | void_kernel |
| B03 | Resource Budget | Per-app limits | void_kernel |
| B04 | Panic Containment | `catch_unwind` | void_kernel |
| B05 | Supervision | Auto-restart on crash | void_kernel |
| B06 | Audit Logging | All capability checks | void_kernel |

---

## Dependency Graph (JSON Export)

```json
{
  "crates": {
    "void_core": {
      "depends_on": ["void_structures", "void_memory"],
      "depended_by": ["void_ecs", "void_asset", "void_event"]
    },
    "void_ecs": {
      "depends_on": ["void_core"],
      "depended_by": ["void_ir", "void_engine"]
    },
    "void_ir": {
      "depends_on": ["void_ecs"],
      "depended_by": ["void_render", "void_engine", "void_kernel"]
    },
    "void_kernel": {
      "depends_on": ["void_engine", "void_ir", "void_render"],
      "depended_by": ["void_runtime"]
    },
    "void_runtime": {
      "depends_on": ["void_kernel", "void_shell", "void_editor"],
      "depended_by": []
    }
  }
}
```

---

## Feature Flag Matrix

| Crate | hot-reload | smithay | xr | audio | drm |
|-------|:----------:|:-------:|:--:|:-----:|:---:|
| void_core | ✅ | - | - | - | - |
| void_kernel | ✅ | - | - | - | - |
| void_runtime | ✅ | ✅ | ✅ | - | ✅ |
| void_compositor | - | ✅ | - | - | ✅ |
| void_presenter | - | ✅ | ✅ | - | ✅ |
| void_services | - | - | - | ✅ | - |

---

## Query Examples

### Find all hot-swappable components
```
Pattern: H*
Result: H01-H07 (7 components)
```

### Find all security mechanisms
```
Pattern: B*
Result: B01-B06 (6 boundaries)
```

### Find all kernel patterns
```
Crate filter: void_kernel
Result: P09, P10, P11, P12, P13 (5 patterns)
```

### Find ECS-related types
```
Crate filter: void_ecs
Result: T03-T05, S04-S07, P04-P05
```

---

## Export Formats

This index can be exported to:
- **JSON**: Machine-readable for tooling
- **CSV**: Spreadsheet analysis
- **SQLite**: Complex queries
- **GraphViz**: Dependency visualization

See `/docs/exports/` for generated exports.
