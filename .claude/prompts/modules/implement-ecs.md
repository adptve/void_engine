# Implement void_ecs Module

> **You own**: `src/ecs/` and `include/void_engine/ecs/`
> **Do NOT modify** any other directories
> **Depends on**: void_core, void_structures (assume they exist)

---

## YOUR TASK

Implement these 7 headers:

| Header | Create |
|--------|--------|
| `archetype.hpp` | `src/ecs/archetype.cpp` |
| `bundle.hpp` | `src/ecs/bundle.cpp` |
| `component.hpp` | `src/ecs/component.cpp` |
| `entity.hpp` | `src/ecs/entity.cpp` |
| `hierarchy.hpp` | `src/ecs/hierarchy.cpp` |
| `query.hpp` | `src/ecs/query.cpp` |

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct WorldSnapshot {
    static constexpr uint32_t MAGIC = 0x45435357;  // "ECSW"
    static constexpr uint32_t VERSION = 1;
    // Entity generations
    // Component data (binary serialized)
    // Archetype layouts
    // Hierarchy relationships
};
```

### Performance
- Archetype-based storage (SoA)
- Cache-friendly iteration
- Sparse sets for fast entity lookup
- Query caching

### Entity IDs
- Generational IDs (index + generation)
- Handles survive hot-reload
- Recycling with generation increment

---

## START

```
Read include/void_engine/ecs/entity.hpp
```
