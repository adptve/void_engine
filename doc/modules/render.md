# Module: render

## Overview
- **Location**: `include/void_engine/render/` and `src/render/`
- **Status**: CRITICAL ISSUES
- **Grade**: D

## File Inventory

### Headers
| File | Purpose |
|------|---------|
| `fwd.hpp` | Forward declarations |
| `pass.hpp` | Render pass definitions |
| `render_graph.hpp` | RenderGraph, LayerManager, Compositor, View, RenderQueue |
| `renderer.hpp` | IRenderer interface |
| `resource.hpp` | ResourceId, texture/buffer resources |

### Implementations
| File | Purpose |
|------|---------|
| `gl_renderer.cpp` | OpenGL renderer implementation **(REAL)** |
| `pass.cpp` | Render pass implementations |
| `render_graph.cpp` | RenderGraph implementation |
| `resource.cpp` | Resource management |
| `stub.cpp` | Stub/fallback implementations **(SHOULD BE REMOVED)** |

## Issues Found

### CRITICAL: Duplicate Implementations (ODR Violation)
**Severity**: Critical
**Files**: `stub.cpp` and `gl_renderer.cpp`

Both files define implementations for the same functions. **45+ functions are duplicated**.

## Detailed Duplicate Comparison

### GlCamera Methods (5 functions)

| Function | stub.cpp | gl_renderer.cpp | Real Implementation |
|----------|----------|-----------------|---------------------|
| `view_matrix()` | Functional | Identical | Both work |
| `projection_matrix()` | Works | Better ortho scaling (`*0.5f`) | **gl_renderer.cpp** |
| `view_projection()` | Functional | Identical | Both work |
| `orbit()` | Works | Better pitch clamping | **gl_renderer.cpp** |
| `zoom()` | No max clamp | Clamps 1.0-100.0 | **gl_renderer.cpp** |
| `pan()` | Works | Cleaner, calls orbit() | **gl_renderer.cpp** |

### GpuMesh Methods

| Function | stub.cpp | gl_renderer.cpp | Real Implementation |
|----------|----------|-----------------|---------------------|
| `destroy()` | Sets vars to 0 (no GL calls) | Calls `glDeleteVertexArrays`, `glDeleteBuffers` | **gl_renderer.cpp** |

### ShaderProgram Methods (20 functions)

| Function | stub.cpp | gl_renderer.cpp | Real Implementation |
|----------|----------|-----------------|---------------------|
| `~ShaderProgram()` | Empty comment | `glDeleteProgram` | **gl_renderer.cpp** |
| `load_from_source()` | Returns true, logs warning | Compiles shaders, error handling | **gl_renderer.cpp** |
| `load_from_files()` | Returns true, logs warning | Reads files, tracks mod times | **gl_renderer.cpp** |
| `reload()` | Returns true | Full hot-reload with version bump | **gl_renderer.cpp** |
| `use()` | Empty | `glUseProgram` | **gl_renderer.cpp** |
| `set_bool()` | Empty | `glUniform1i` | **gl_renderer.cpp** |
| `set_int()` | Empty | `glUniform1i` | **gl_renderer.cpp** |
| `set_float()` | Empty | `glUniform1f` | **gl_renderer.cpp** |
| `set_vec2()` | Empty | `glUniform2fv` | **gl_renderer.cpp** |
| `set_vec3()` | Empty | `glUniform3fv` | **gl_renderer.cpp** |
| `set_vec4()` | Empty | `glUniform4fv` | **gl_renderer.cpp** |
| `set_mat3()` | Empty | `glUniformMatrix3fv` | **gl_renderer.cpp** |
| `set_mat4()` | Empty | `glUniformMatrix4fv` | **gl_renderer.cpp** |
| `get_uniform_location()` | Returns -1 | Caching + GL call | **gl_renderer.cpp** |
| `compile_shader()` | Returns 0 | Full GL compile | **gl_renderer.cpp** |
| `link_program()` | Returns true | Full GL link | **gl_renderer.cpp** |
| `snapshot()` | Empty snapshot | Stores shader paths | **gl_renderer.cpp** |
| `restore()` | Returns Ok | Calls reload() | **gl_renderer.cpp** |
| `is_compatible()` | Returns true | Returns true | Both |
| `current_version()` | Returns m_version | Returns m_version | Both |

### SceneRenderer Methods (10 functions)

| Function | stub.cpp | gl_renderer.cpp | Real Implementation |
|----------|----------|-----------------|---------------------|
| Constructor | Logs only | `= default` | **gl_renderer.cpp** |
| Destructor | Logs only | Calls shutdown() | **gl_renderer.cpp** |
| `initialize()` | Logs, returns true | Full GL setup | **gl_renderer.cpp** |
| `shutdown()` | Logs only | Destroys meshes, clears | **gl_renderer.cpp** |
| `load_scene()` | Camera only | Full scene conversion | **gl_renderer.cpp** |
| `render()` | Sets stats to 0 | Full render pipeline | **gl_renderer.cpp** |
| `update()` | Empty | Animation + hot-reload | **gl_renderer.cpp** |
| `on_resize()` | Updates vars | Updates vars + glViewport | **gl_renderer.cpp** |
| `reload_shaders()` | Logs only | Reloads both shaders | **gl_renderer.cpp** |

### Mesh Generation (8 functions)

| Function | stub.cpp | gl_renderer.cpp | Real Implementation |
|----------|----------|-----------------|---------------------|
| `create_builtin_meshes()` | `{}` empty | Creates all meshes | **gl_renderer.cpp** |
| `create_sphere_mesh()` | `{}` empty | 44 lines geometry | **gl_renderer.cpp** |
| `create_cube_mesh()` | `{}` empty | 54 lines geometry | **gl_renderer.cpp** |
| `create_torus_mesh()` | `{}` empty | 53 lines geometry | **gl_renderer.cpp** |
| `create_plane_mesh()` | `{}` empty | 14 lines geometry | **gl_renderer.cpp** |
| `create_cylinder_mesh()` | `{}` empty | 74 lines geometry | **gl_renderer.cpp** |
| `create_diamond_mesh()` | `{}` empty | 45 lines geometry | **gl_renderer.cpp** |
| `create_quad_mesh()` | `{}` empty | 12 lines geometry | **gl_renderer.cpp** |

### Helper & Conversion Functions (8 functions)

| Function | stub.cpp | gl_renderer.cpp | Real Implementation |
|----------|----------|-----------------|---------------------|
| `create_shaders()` | `{}` empty | Creates both shaders | **gl_renderer.cpp** |
| `check_shader_reload()` | `{}` empty | Timer-based reload | **gl_renderer.cpp** |
| `render_entity()` | `{}` empty | Full entity rendering | **gl_renderer.cpp** |
| `render_grid()` | `{}` empty | Grid rendering | **gl_renderer.cpp** |
| `upload_lights()` | `{}` empty | Light uniform upload | **gl_renderer.cpp** |
| `convert_camera()` | `{}` empty | Scene to GL camera | **gl_renderer.cpp** |
| `convert_light()` | `{}` empty | Scene to GL light | **gl_renderer.cpp** |
| `convert_entity()` | `{}` empty | Scene to GL entity | **gl_renderer.cpp** |

## Verdict

**`stub.cpp` is a migration artifact that should be DELETED.**

- 100% of real implementations are in `gl_renderer.cpp`
- `stub.cpp` contains only empty stubs or minimal logging
- Both files being compiled causes ODR violations

**Recommendation**: Remove `stub.cpp` from CMakeLists.txt and delete the file.

### CRITICAL: Missing InstanceData Implementations
**Severity**: Critical
**Location**: `renderer.hpp` declares `InstanceData` struct with methods

**Missing Implementations** (10 functions):
1. `InstanceData::InstanceData()` - constructor
2. `InstanceData::~InstanceData()` - destructor
3. `InstanceData::reserve()`
4. `InstanceData::clear()`
5. `InstanceData::add_instance()`
6. `InstanceData::update_instance()`
7. `InstanceData::remove_instance()`
8. `InstanceData::instance_count()`
9. `InstanceData::data()`
10. `InstanceData::stride()`

**Recommendation**: Either implement these methods in a .cpp file or mark them as inline/header-only.

## Consistency Matrix

| Component | Header | Implementation | Status |
|-----------|--------|----------------|--------|
| RenderGraph | render_graph.hpp | render_graph.cpp | OK |
| RenderPass | pass.hpp | pass.cpp | OK |
| RenderLayer | render_graph.hpp | render_graph.cpp | OK |
| LayerManager | render_graph.hpp | render_graph.cpp | OK |
| View | render_graph.hpp | render_graph.cpp | OK |
| Compositor | render_graph.hpp | render_graph.cpp | OK |
| RenderQueue | render_graph.hpp | render_graph.cpp | OK |
| IRenderer | renderer.hpp | gl_renderer.cpp + stub.cpp | DUPLICATE |
| InstanceData | renderer.hpp | MISSING | MISSING |
| ResourceId | resource.hpp | resource.cpp | OK |

## Action Items

1. [ ] **URGENT**: Resolve ODR violation between stub.cpp and gl_renderer.cpp
2. [ ] **URGENT**: Implement InstanceData methods or convert to header-only
3. [ ] Consider using factory pattern for renderer selection instead of dual implementations
