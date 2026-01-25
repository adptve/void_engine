# Integration Diagrams

This directory contains Mermaid diagrams documenting the integration of each module.

## Required Diagrams

| File | Module | Status |
|------|--------|--------|
| `ir_integration.md` | void_ir | ⬜ TODO |
| `presenter_integration.md` | void_presenter | ⬜ TODO |
| `render_integration.md` | void_render | ⬜ TODO |
| `asset_integration.md` | void_asset | ⬜ TODO |
| `ecs_integration.md` | void_ecs | ✅ Complete |
| `compositor_integration.md` | void_compositor | ⬜ TODO |
| `core_integration.md` | void_core | ⬜ TODO |
| `physics_integration.md` | void_physics | ⬜ TODO |
| `services_integration.md` | void_services | ⬜ TODO |
| `full_integration.md` | Complete system | ⬜ TODO |

## Diagram Requirements

Each diagram MUST include:

1. **Class Diagram** - Shows inheritance, composition, dependencies
2. **Hot-Reload Flow** - Sequence diagram of snapshot→serialize→restore
3. **Data Flow** - How data moves through the module
4. **Dependencies** - How module connects to other modules

## Format

Use Mermaid markdown format:

```markdown
# Module Integration

## Class Diagram

​```mermaid
classDiagram
    ...
​```

## Hot-Reload Flow

​```mermaid
sequenceDiagram
    ...
​```

## Dependencies

​```mermaid
graph TD
    ...
​```
```
