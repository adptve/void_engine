# Implement void_services Module

> **You own**: `src/services/` and `include/void_engine/services/`
> **Do NOT modify** any other directories
> **Depends on**: void_core, void_event (assume they exist)

---

## YOUR TASK

Implement these 4 headers:

| Header | Create |
|--------|--------|
| `event_bus.hpp` | `src/services/event_bus.cpp` |
| `service.hpp` | `src/services/service.cpp` |
| `services.hpp` | `src/services/services.cpp` |

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct ServiceManagerSnapshot {
    static constexpr uint32_t MAGIC = 0x53565353;  // "SVCS"
    static constexpr uint32_t VERSION = 1;
    // Registered services
    // Service states
    // Dependencies
};
```

### Service Lifecycle
- start() / stop() / update()
- Dependency ordering
- Health monitoring

---

## START

```
Read include/void_engine/services/service.hpp
```
