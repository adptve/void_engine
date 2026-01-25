# Implement void_presenter Module

> **You own**: `src/presenter/` and `include/void_engine/presenter/`
> **Do NOT modify** any other directories
> **Depends on**: void_core (assume it exists)

---

## YOUR TASK

Implement these 12 headers:

| Header | Create |
|--------|--------|
| `presenter.hpp` | `src/presenter/presenter.cpp` |
| `multi_backend_presenter.hpp` | `src/presenter/multi_backend_presenter.cpp` |
| `swapchain.hpp` | `src/presenter/swapchain.cpp` |
| `surface.hpp` | `src/presenter/surface.cpp` |
| `frame.hpp` | `src/presenter/frame.cpp` |
| `timing.hpp` | `src/presenter/timing.cpp` |
| `rehydration.hpp` | `src/presenter/rehydration.cpp` |
| `presenter_module.hpp` | `src/presenter/presenter_module.cpp` |
| `backends/null_backend.hpp` | `src/presenter/backends/null_backend.cpp` |
| `backends/wgpu_backend.hpp` | `src/presenter/backends/wgpu_backend.cpp` |
| `xr/xr_types.hpp` | `src/presenter/xr/xr_types.cpp` |
| `drm.hpp` | `src/presenter/drm_presenter.cpp` |

---

## PROCESS

1. **Read** each header in `include/void_engine/presenter/`
2. **Check** legacy code at `legacy/crates/void_presenter/src/` for behavior reference
3. **Implement** each .cpp file
4. **Create** `doc/diagrams/presenter_integration.md`
5. **Create** `doc/validation/presenter_validation.md`

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct PresenterSnapshot {
    static constexpr uint32_t MAGIC = 0x50525354;  // "PRST"
    static constexpr uint32_t VERSION = 1;
    uint32_t width, height;
    PresentMode present_mode;
    // Swapchain config (recreated on restore)
};
```

### Platform-Specific
- DRM backend: `#if defined(__linux__)` guards
- WGPU backend: conditional on VOID_HAS_WGPU
- Null backend: always available for testing

### Performance
- Frame timing uses high-resolution clock
- Swapchain operations don't allocate
- Triple buffering support

---

## OUTPUT WHEN COMPLETE

```
## void_presenter Implementation Complete

### Files Created
- src/presenter/presenter.cpp
- src/presenter/multi_backend_presenter.cpp
- src/presenter/swapchain.cpp
- src/presenter/surface.cpp
- src/presenter/frame.cpp
- src/presenter/timing.cpp
- src/presenter/rehydration.cpp
- src/presenter/presenter_module.cpp
- src/presenter/backends/null_backend.cpp
- src/presenter/backends/wgpu_backend.cpp
- src/presenter/xr/xr_types.cpp
- src/presenter/drm_presenter.cpp

### Add to src/presenter/CMakeLists.txt
target_sources(void_presenter PRIVATE
    presenter.cpp
    multi_backend_presenter.cpp
    swapchain.cpp
    surface.cpp
    frame.cpp
    timing.cpp
    rehydration.cpp
    presenter_module.cpp
    backends/null_backend.cpp
)

if(VOID_HAS_WGPU)
    target_sources(void_presenter PRIVATE backends/wgpu_backend.cpp)
endif()

if(UNIX AND NOT APPLE)
    target_sources(void_presenter PRIVATE drm_presenter.cpp)
    target_link_libraries(void_presenter PRIVATE drm gbm)
endif()

### Dependencies
- void_core::HotReloadable
- void_core::Result
```

---

## START

Begin by reading:
```
Read include/void_engine/presenter/backend.hpp
```
(Backend interface is the foundation)
