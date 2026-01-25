# Implement void_physics Module

> **You own**: `src/physics/` and `include/void_engine/physics/`
> **Do NOT modify** any other directories
> **Depends on**: void_core, void_math (assume they exist)

---

## YOUR TASK

Implement these 5 headers:

| Header | Create |
|--------|--------|
| `broadphase.hpp` | `src/physics/broadphase.cpp` |
| `collision.hpp` | `src/physics/collision.cpp` |
| `solver.hpp` | `src/physics/solver.cpp` |
| `physics.hpp` | `src/physics/physics.cpp` |

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct PhysicsWorldSnapshot {
    static constexpr uint32_t MAGIC = 0x50485953;  // "PHYS"
    static constexpr uint32_t VERSION = 1;
    // Body transforms, velocities
    // Constraint state
    // Broadphase structure
};
```

### Performance
- Spatial partitioning (BVH or grid)
- SIMD for collision math
- Constraint solver iterations configurable

---

## START

```
Read include/void_engine/physics/body.hpp
```
