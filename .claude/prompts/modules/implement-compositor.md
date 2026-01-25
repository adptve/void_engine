# Implement void_compositor Module

> **You own**: `src/compositor/` and `include/void_engine/compositor/`
> **Do NOT modify** any other directories
> **Depends on**: void_core, void_presenter (assume they exist)

---

## YOUR TASK

Implement these 7 headers:

| Header | Create |
|--------|--------|
| `frame.hpp` | `src/compositor/frame.cpp` |
| `hdr.hpp` | `src/compositor/hdr.cpp` |
| `output.hpp` | `src/compositor/output.cpp` |
| `vrr.hpp` | `src/compositor/vrr.cpp` |
| `rehydration.hpp` | `src/compositor/rehydration.cpp` |
| `input.hpp` | `src/compositor/input.cpp` |
| `compositor_module.hpp` | `src/compositor/compositor_module.cpp` |

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct CompositorSnapshot {
    static constexpr uint32_t MAGIC = 0x434F4D50;  // "COMP"
    static constexpr uint32_t VERSION = 1;
    // Layer stack
    // HDR configuration
    // VRR state
    // Output configuration
};
```

### Features
- Layer-based composition
- HDR tone mapping (PQ, HLG)
- Variable Refresh Rate support
- Multi-output management

---

## START

```
Read include/void_engine/compositor/compositor.hpp
```
