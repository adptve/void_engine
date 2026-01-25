# Implement void_ir Module

> **You own**: `src/ir/` and `include/void_engine/ir/`
> **Do NOT modify** any other directories
> **Depends on**: void_core (assume it exists)

---

## YOUR TASK

Implement these 8 headers:

| Header | Create |
|--------|--------|
| `batch.hpp` | `src/ir/batch.cpp` |
| `bus.hpp` | `src/ir/bus.cpp` |
| `ir.hpp` | `src/ir/ir.cpp` |
| `namespace.hpp` | `src/ir/namespace.cpp` |
| `patch.hpp` | `src/ir/patch.cpp` |
| `transaction.hpp` | `src/ir/transaction.cpp` |
| `validation.hpp` | `src/ir/validation.cpp` |
| `value.hpp` | `src/ir/value.cpp` |

---

## PROCESS

1. **Read** each header in `include/void_engine/ir/`
2. **Check** legacy code at `legacy/crates/void_ir/src/` for behavior reference
3. **Implement** each .cpp file with full functionality
4. **Create** `doc/diagrams/ir_integration.md`
5. **Create** `doc/validation/ir_validation.md`

---

## REQUIREMENTS

### Hot-Reload
Stateful classes (PatchBus, BatchOptimizer, Transaction) implement HotReloadable:
```cpp
struct PatchBusSnapshot {
    static constexpr uint32_t MAGIC = 0x50425553;  // "PBUS"
    static constexpr uint32_t VERSION = 1;
    // Pending patches, subscriber state
};
```

### Performance
- PatchBus uses lock-free queue from void_structures
- No allocations during patch dispatch
- Batch optimization pre-allocates buffers

### Error Handling
- Validation returns Result<void> with detailed errors
- Transaction rollback on failure

---

## OUTPUT WHEN COMPLETE

```
## void_ir Implementation Complete

### Files Created
- src/ir/batch.cpp
- src/ir/bus.cpp
- src/ir/ir.cpp
- src/ir/namespace.cpp
- src/ir/patch.cpp
- src/ir/transaction.cpp
- src/ir/validation.cpp
- src/ir/value.cpp

### Add to src/ir/CMakeLists.txt
target_sources(void_ir PRIVATE
    batch.cpp
    bus.cpp
    ir.cpp
    namespace.cpp
    patch.cpp
    transaction.cpp
    validation.cpp
    value.cpp
)

### Dependencies on Other Modules
- void_core::HotReloadable
- void_core::Result
- void_structures::LockFreeQueue
```

---

## START

Begin by reading:
```
Read include/void_engine/ir/patch.hpp
```
(Patch is the core type, understand it first)
