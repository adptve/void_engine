# Implement void_render Module

> **You own**: `src/render/` and `include/void_engine/render/`
> **Do NOT modify** any other directories
> **Depends on**: void_core, void_shader (assume they exist)

---

## YOUR TASK

Implement these 10 headers:

| Header | Create |
|--------|--------|
| `camera.hpp` | `src/render/camera.cpp` |
| `light.hpp` | `src/render/light.cpp` |
| `material.hpp` | `src/render/material.cpp` |
| `mesh.hpp` | `src/render/mesh.cpp` |
| `pass.hpp` | `src/render/pass.cpp` |
| `resource.hpp` | `src/render/resource.cpp` |
| `shadow.hpp` | `src/render/shadow.cpp` |
| `debug.hpp` | `src/render/debug.cpp` |
| `texture.hpp` | `src/render/texture.cpp` |
| `render.hpp` | `src/render/render.cpp` |

---

## REQUIREMENTS

### Hot-Reload
```cpp
struct RenderSystemSnapshot {
    static constexpr uint32_t MAGIC = 0x524E4452;  // "RNDR"
    static constexpr uint32_t VERSION = 1;
    // Camera state, active lights, render config
    // GPU resources are HANDLES not pointers
};
```

### Performance
- Batch similar draw calls
- Frustum culling
- Material sorting to minimize state changes
- Pre-allocated command buffers

### GPU Resources
- Use handles for textures, buffers, shaders
- Resources recreated on restore (not serialized)
- prepare_reload() releases GPU resources
- finish_reload() recreates from cached data

---

## OUTPUT FORMAT

Same as other modules - list files, CMake additions, dependencies.

---

## START

```
Read include/void_engine/render/render.hpp
```
