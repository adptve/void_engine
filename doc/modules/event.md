# Module: event

## Overview
- **Location**: `include/void_engine/event/`
- **Status**: Perfect (Header-Only)
- **Grade**: A+

## File Inventory

### Headers
| File | Purpose |
|------|---------|
| `event.hpp` | Event system, dispatcher |
| `types.hpp` | Common event types |
| `queue.hpp` | Event queue |

### Implementations
This is a **header-only** module. Template-based event system.

## Issues Found

None. Header-only design is appropriate for template event system.

## Consistency Matrix

| Component | Header | Implementation | Status |
|-----------|--------|----------------|--------|
| EventDispatcher | event.hpp | Header-only | OK |
| Event<T> | event.hpp | Header-only | OK |
| EventQueue | queue.hpp | Header-only | OK |
| EventListener | event.hpp | Header-only | OK |

## Event System Features

| Feature | Status |
|---------|--------|
| Type-safe events | Implemented |
| Multiple listeners | Implemented |
| Priority ordering | Implemented |
| Event cancellation | Implemented |
| Deferred dispatch | Implemented |
| Immediate dispatch | Implemented |

## Built-in Event Types

| Event | Purpose |
|-------|---------|
| WindowResizeEvent | Window size changed |
| KeyEvent | Keyboard input |
| MouseEvent | Mouse input |
| GamepadEvent | Controller input |
| CollisionEvent | Physics collision |
| EntityCreatedEvent | ECS entity created |
| EntityDestroyedEvent | ECS entity destroyed |

## Action Items

None required - header-only design is appropriate.
