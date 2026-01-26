# Module: kernel

## Overview
- **Location**: `include/void_engine/kernel/` and `src/kernel/`
- **Status**: Perfect
- **Grade**: A

## File Inventory

### Headers
| File | Purpose |
|------|---------|
| `kernel.hpp` | Kernel orchestrator |
| `supervisor.hpp` | Erlang-style supervision trees |
| `task.hpp` | Task scheduling |
| `fiber.hpp` | Fiber/coroutine support |

### Implementations
| File | Purpose |
|------|---------|
| `kernel.cpp` | Kernel implementation |
| `supervisor.cpp` | Supervisor implementation |
| `task.cpp` | Task scheduler |
| `fiber.cpp` | Fiber runtime |

## Issues Found

### Style Note (Non-Critical)
`supervisor.cpp` is missing `[[nodiscard]]` attributes on some return values that have them in the header. This doesn't affect compilation but is a style inconsistency.

## Consistency Matrix

| Component | Header | Implementation | Status |
|-----------|--------|----------------|--------|
| Kernel | kernel.hpp | kernel.cpp | OK |
| Supervisor | supervisor.hpp | supervisor.cpp | OK |
| RestartStrategy | supervisor.hpp | supervisor.cpp | OK |
| ChildSpec | supervisor.hpp | supervisor.cpp | OK |
| SupervisorFlags | supervisor.hpp | supervisor.cpp | OK |
| TaskScheduler | task.hpp | task.cpp | OK |
| Fiber | fiber.hpp | fiber.cpp | OK |

## Supervisor Strategies

All restart strategies implemented:
- [x] OneForOne - restart only failed child
- [x] OneForAll - restart all children on failure
- [x] RestForOne - restart failed child and all after it
- [x] SimpleOneForOne - dynamic children, one-for-one

## Action Items

1. [ ] Optional: Add `[[nodiscard]]` to implementation functions for consistency
