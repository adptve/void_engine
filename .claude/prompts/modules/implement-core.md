# Implement void_core Module

> **RUN THIS FIRST** - Other modules depend on void_core
> **You own**: `src/core/` and `include/void_engine/core/`
> **Do NOT modify** any other directories

---

## YOUR TASK

Implement these 8 headers:

| Header | Create |
|--------|--------|
| `error.hpp` | `src/core/error.cpp` |
| `handle.hpp` | `src/core/handle.cpp` |
| `id.hpp` | `src/core/id.cpp` |
| `log.hpp` | `src/core/log.cpp` |
| `type_registry.hpp` | `src/core/type_registry.cpp` |
| `hot_reload.hpp` | `src/core/hot_reload.cpp` |
| `plugin.hpp` | `src/core/plugin.cpp` |
| `version.hpp` | `src/core/version.cpp` |

---

## PROCESS

1. **Read** `include/void_engine/core/hot_reload.hpp` first - this defines the HotReloadable interface ALL modules use
2. **Read** each header to understand what methods need implementation
3. **Check** legacy code at `legacy/crates/void_core/src/` for behavior reference
4. **Implement** each .cpp file
5. **Create** `doc/diagrams/core_integration.md` with Mermaid diagrams
6. **Create** `doc/validation/core_validation.md` with validation checklist

---

## REQUIREMENTS

### Hot-Reload
Every stateful class implements HotReloadable:
```cpp
Result<HotReloadSnapshot> snapshot() override;
Result<void> restore(HotReloadSnapshot snapshot) override;
bool is_compatible(const Version& new_version) const override;
```

### Snapshots
```cpp
struct MySnapshot {
    static constexpr uint32_t MAGIC = 0x????????;  // Unique 4 chars
    static constexpr uint32_t VERSION = 1;
    // Binary serializable data only
};
```

### Performance
- No allocations in hot paths
- Pre-allocated buffers
- RAII for all resources

### Error Handling
- Use Result<T> for fallible operations
- Never throw in hot paths

---

## OUTPUT WHEN COMPLETE

```
## void_core Implementation Complete

### Files Created
- src/core/error.cpp
- src/core/handle.cpp
- src/core/id.cpp
- src/core/log.cpp
- src/core/type_registry.cpp
- src/core/hot_reload.cpp
- src/core/plugin.cpp
- src/core/version.cpp

### Add to src/core/CMakeLists.txt
target_sources(void_core PRIVATE
    error.cpp
    handle.cpp
    id.cpp
    log.cpp
    type_registry.cpp
    hot_reload.cpp
    plugin.cpp
    version.cpp
)

### Diagrams Created
- doc/diagrams/core_integration.md

### Validation Report
- doc/validation/core_validation.md

### Dependencies
- None (this is the base module)
```

---

## START

Begin by reading:
```
Read include/void_engine/core/hot_reload.hpp
```

Then implement each header systematically.
