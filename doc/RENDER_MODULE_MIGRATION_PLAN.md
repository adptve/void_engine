# Void Engine Render Module Migration Plan

## Overview

This document provides comprehensive UML diagrams and a migration plan for aligning the render module's header declarations with their cpp implementations.

**Status**: Discovery Complete
**Date**: 2026-01-26
**Files Affected**: 8 cpp files, 6+ header files

---

## Table of Contents

1. [Current Architecture Overview](#1-current-architecture-overview)
2. [Texture Subsystem](#2-texture-subsystem)
3. [Shadow Subsystem](#3-shadow-subsystem)
4. [Debug Subsystem](#4-debug-subsystem)
5. [Instancing Subsystem](#5-instancing-subsystem)
6. [Spatial Subsystem](#6-spatial-subsystem)
7. [Mesh Subsystem](#7-mesh-subsystem)
8. [Post-Processing Subsystem](#8-post-processing-subsystem)
9. [GLTF Loading Subsystem](#9-gltf-loading-subsystem)
10. [LOD Subsystem](#10-lod-subsystem)
11. [Temporal Effects Subsystem](#11-temporal-effects-subsystem)
12. [Header Dependency Graph](#12-header-dependency-graph)
13. [Identified Mismatches](#13-identified-mismatches)
14. [Migration Tasks](#14-migration-tasks)

---

## 1. Current Architecture Overview

```mermaid
graph TB
    subgraph "Public Headers (include/void_engine/render/)"
        fwd[fwd.hpp<br/>Forward Declarations]
        resource[resource.hpp<br/>Resource Types]
        texture_h[texture.hpp]
        mesh_h[mesh.hpp]
        shadow_h[shadow.hpp]
        debug_h[debug.hpp]
        instancing_h[instancing.hpp]
        spatial_h[spatial.hpp]
        camera_h[camera.hpp]
        material_h[material.hpp]
        gl_renderer_h[gl_renderer.hpp]
        render_h[render.hpp<br/>Master Include]
    end

    subgraph "Implementation (src/render/)"
        texture_cpp[texture.cpp]
        shadow_cpp[shadow_renderer.cpp]
        debug_cpp[debug_renderer.cpp]
        instancing_cpp[instancing.cpp]
        spatial_cpp[spatial.cpp]
        gltf_cpp[gltf_loader.cpp]
        lod_cpp[lod.cpp]
        temporal_cpp[temporal_effects.cpp]
        post_cpp[post_process.cpp]
        gl_cpp[gl_renderer.cpp]
    end

    subgraph "Missing Headers"
        gltf_h[gltf.hpp ❌]
        lod_h[lod.hpp ❌]
        temporal_h[temporal.hpp ❌]
        post_h[post_process.hpp ❌]
    end

    texture_cpp --> texture_h
    shadow_cpp --> shadow_h
    debug_cpp --> debug_h
    instancing_cpp --> instancing_h
    spatial_cpp --> spatial_h
    gltf_cpp -.-> gltf_h
    lod_cpp -.-> lod_h
    temporal_cpp -.-> temporal_h
    post_cpp -.-> post_h

    render_h --> texture_h
    render_h --> mesh_h
    render_h --> shadow_h
    render_h --> debug_h
    render_h --> instancing_h
    render_h --> spatial_h
```

---

## 2. Texture Subsystem

### Header Declaration (texture.hpp)

```mermaid
classDiagram
    class TextureHandle {
        -uint64_t m_id
        +id() uint64_t
        +is_valid() bool
        +operator bool()
    }

    class TextureData {
        +vector~uint8_t~ pixels
        +uint32_t width
        +uint32_t height
        +uint32_t depth
        +uint32_t channels
        +uint32_t mip_levels
        +TextureFormat format
        +bool is_hdr
        +bool is_srgb
        +from_rgba()$ TextureData
        +solid_color()$ TextureData
        +checkerboard()$ TextureData
        +default_normal()$ TextureData
        +default_white()$ TextureData
        +default_black()$ TextureData
        +generate_mipmaps()$ vector
        +size_bytes() size_t
        +is_valid() bool
        +get_pixel() array
    }

    class HdrTextureData {
        +vector~float~ pixels
        +uint32_t width
        +uint32_t height
        +uint32_t channels
        +is_valid() bool
        +get_pixel() array
        +to_ldr() TextureData
    }

    class CubemapData {
        +array~TextureData,6~ faces
        +bool is_hdr
        +from_equirectangular()$ CubemapData
        +from_faces()$ CubemapData
        +is_valid() bool
        +face_size() uint32_t
    }

    class Texture {
        -uint32_t m_id
        -uint32_t m_width
        -uint32_t m_height
        -uint32_t m_mip_levels
        -TextureFormat m_format
        -bool m_is_hdr
        +create(TextureData) bool
        +create_hdr(HdrTextureData) bool
        +create_render_target() bool
        +create_depth() bool
        +destroy() void
        +bind(unit) void
        +unbind() void
        +update(data) void
        +generate_mipmaps() void
        +id() uint32_t
        +width() uint32_t
        +height() uint32_t
        +format() TextureFormat
        +is_valid() bool
        +is_hdr() bool
        +mip_levels() uint32_t
        +gpu_memory_bytes() size_t
    }

    class Cubemap {
        -uint32_t m_id
        -uint32_t m_face_size
        -bool m_is_hdr
        +create(CubemapData) bool
        +create_from_equirectangular() bool
        +destroy() void
        +bind(unit) void
        +id() uint32_t
        +face_size() uint32_t
        +is_valid() bool
    }

    class Sampler {
        -uint32_t m_id
        +create(SamplerDesc) bool
        +destroy() void
        +bind(unit) void
        +id() uint32_t
        +is_valid() bool
    }

    class TextureLoader {
        +load()$ optional~TextureData~
        +load_hdr()$ optional~HdrTextureData~
        +load_cubemap()$ optional~CubemapData~
        +load_cubemap_directory()$ optional~CubemapData~
        +load_cubemap_equirectangular()$ optional~CubemapData~
        +save()$ bool
        +save_hdr()$ bool
        +is_supported_format()$ bool
        +is_hdr_format()$ bool
    }

    class TextureManager {
        -mutex m_mutex
        -map m_textures
        -map m_cubemaps
        -map m_path_to_handle
        -atomic m_next_handle
        -TextureHandle m_default_white
        -TextureHandle m_default_black
        -TextureHandle m_default_normal
        -TextureHandle m_default_checker
        +initialize() bool
        +shutdown() void
        +load(path) TextureHandle
        +load_from_memory() TextureHandle
        +load_cubemap() TextureHandle
        +get(handle) Texture*
        +get_cubemap(handle) Cubemap*
        +is_valid(handle) bool
        +release(handle) void
        +reload(handle) bool
        +update(dt) void
        +set_reload_interval() void
        +default_white() TextureHandle
        +default_black() TextureHandle
        +default_normal() TextureHandle
        +default_checkerboard() TextureHandle
        +stats() TextureManagerStats
        +on_texture_reloaded callback
    }

    class IBLProcessor {
        +generate_irradiance_map()$ Cubemap
        +generate_prefiltered_map()$ Cubemap
        +generate_brdf_lut()$ Texture
        +create_from_hdr()$ IBLMaps
    }

    class IBLMaps {
        +Cubemap environment
        +Cubemap irradiance
        +Cubemap prefiltered
        +Texture brdf_lut
    }

    TextureManager --> Texture
    TextureManager --> Cubemap
    TextureManager --> TextureHandle
    TextureLoader --> TextureData
    TextureLoader --> HdrTextureData
    TextureLoader --> CubemapData
    IBLProcessor --> IBLMaps
    IBLProcessor --> Cubemap
    IBLProcessor --> Texture
```

### Implementation Status

| Class | Header Declared | CPP Implemented | Match Status |
|-------|-----------------|-----------------|--------------|
| TextureHandle | ✅ Full | ✅ Full | ✅ Match |
| TextureData | ✅ Full | ✅ Full | ✅ Match |
| HdrTextureData | ✅ Full | ✅ Full | ✅ Match |
| CubemapData | ✅ Full | ✅ Full | ✅ Match |
| Texture | ✅ Full | ✅ Full | ✅ Match |
| Cubemap | ✅ Full | ✅ Full | ✅ Match |
| Sampler | ✅ Full | ⚠️ Skeleton | ⚠️ Incomplete |
| TextureLoader | ✅ Full | ⚠️ Skeleton | ⚠️ Incomplete |
| TextureManager | ✅ Full | ⚠️ Skeleton | ⚠️ Incomplete |
| IBLProcessor | ✅ Full | ⚠️ Skeleton | ⚠️ Incomplete |

---

## 3. Shadow Subsystem

### Header Declaration (shadow.hpp)

```mermaid
classDiagram
    class ShadowConfig {
        +bool enabled
        +ShadowQuality quality
        +ShadowFilterMode filter_mode
        +uint32_t cascade_count
        +uint32_t resolution
        +float cascade_split_lambda
        +float cascade_blend_distance
        +float shadow_distance
        +float depth_bias
        +float normal_bias
        +float slope_bias
        +uint32_t pcf_samples
        +float pcf_radius
        +float pcss_light_size
        +uint32_t pcss_blocker_search_samples
        +uint32_t atlas_size
        +uint32_t max_point_shadows
        +uint32_t max_spot_shadows
        +bool stabilize_cascades
        +bool cull_front_faces
        +bool blend_cascade_regions
        +bool visualize_cascades
        +default_config()$ ShadowConfig
        +high_quality()$ ShadowConfig
        +performance()$ ShadowConfig
    }

    class CascadeData {
        +mat4 view_projection
        +float split_depth
        +float texel_size
        +uint32_t cascade_index
    }

    class GpuCascadeData {
        <<aligned 128 bytes>>
        +float view_proj_matrix[16]
        +float split_depths[4]
        +float atlas_viewport[4]
        +float shadow_params[4]
    }

    class GpuShadowData {
        <<aligned 1024 bytes>>
        +GpuCascadeData cascades[4]
        +float global_params[4]
        +float light_direction[3]
        +float shadow_color[3]
        +float shadow_strength
    }

    class CascadedShadowMap {
        -ShadowConfig m_config
        -Texture m_shadow_map
        -vector~uint32_t~ m_framebuffers
        -vector~CascadeData~ m_cascade_data
        -array~float~ m_cascade_splits
        +initialize(config) bool
        +destroy() void
        +update(view, proj, near, far, light_dir) void
        +begin_shadow_pass(cascade) void
        +end_shadow_pass() void
        +bind_shadow_map(unit) void
        +cascade_count() uint32_t
        +cascade_data(idx) CascadeData
        +shadow_map_texture() Texture*
        +config() ShadowConfig
        -calculate_cascade_splits() void
        -get_frustum_corners_world_space() array
    }

    class ShadowAtlasAllocation {
        +bool allocated
        +uint32_t light_id
        +uint32_t x, y, width, height
        +vec4 uv_rect
    }

    class ShadowAtlas {
        -uint32_t m_atlas_size
        -uint32_t m_max_lights
        -uint32_t m_tile_size
        -Texture m_atlas_texture
        -uint32_t m_framebuffer
        -vector~Allocation~ m_allocations
        +initialize(size, max_lights) bool
        +destroy() void
        +allocate(light_id, size) Allocation*
        +release(light_id) void
        +begin_render(allocation) void
        +end_render() void
        +bind(unit) void
        +size() uint32_t
        +tile_size() uint32_t
    }

    class ShadowManager {
        -ShadowConfig m_config
        -CascadedShadowMap m_cascaded_shadows
        -ShadowAtlas m_shadow_atlas
        -unique_ptr~ShaderProgram~ m_depth_shader
        +initialize(config) bool
        +shutdown() void
        +update(view, proj, near, far, light_dir) void
        +begin_directional_shadow_pass(cascade) void
        +end_directional_shadow_pass() void
        +get_cascade_view_projection(idx) mat4
        +bind_shadow_maps(tex_unit, sampler) void
        +get_cascade_data_packed() GpuShadowData
        +config() ShadowConfig
        +cascaded_shadows() CascadedShadowMap
        +atlas() ShadowAtlas
        -create_depth_shader() bool
    }

    class RayTracedShadowConfig {
        +bool enabled
        +uint32_t rays_per_pixel
        +float max_ray_distance
        +float shadow_bias
        +float soft_shadow_radius
        +bool use_blue_noise
        +bool temporal_accumulation
        +uint32_t denoiser_iterations
    }

    class BlasHandle {
        +uint64_t id
        +is_valid() bool
    }

    class TlasHandle {
        +uint64_t id
        +is_valid() bool
    }

    class AccelerationStructureGeometry {
        +float* vertices
        +uint32_t vertex_count
        +uint32_t vertex_stride
        +uint32_t* indices
        +uint32_t index_count
        +bool opaque
    }

    class AccelerationStructureInstance {
        +BlasHandle blas
        +mat4 transform
        +uint32_t instance_id
        +uint32_t mask
        +bool visible
    }

    class RayTracedShadowRenderer {
        -RayTracedShadowConfig m_config
        -uint32_t m_width, m_height
        -bool m_rt_supported
        -bool m_tlas_dirty
        -uint32_t m_shadow_texture
        -uint32_t m_history_texture
        -uint32_t m_blue_noise_texture
        -map m_blas_map
        -vector m_instances
        -uint64_t m_next_blas_id
        -uint64_t m_frame_count
        +initialize(config, w, h) bool
        +shutdown() void
        +is_supported()$ bool
        +build_blas(geometry) BlasHandle
        +destroy_blas(handle) void
        +build_tlas(instances) bool
        +update_tlas() void
        +trace_directional_shadows(dir, depth) void
        +trace_point_shadows(pos, range, depth) void
        +shadow_texture() uint32_t
        +config() RayTracedShadowConfig
        -check_raytracing_support() bool
        -create_shadow_texture() bool
        -destroy_shadow_texture() void
        -create_rt_pipeline() bool
        -destroy_rt_pipeline() void
        -create_blue_noise_texture() void
        -create_temporal_resources() void
        -destroy_temporal_resources() void
        -destroy_acceleration_structures() void
    }

    ShadowManager --> CascadedShadowMap
    ShadowManager --> ShadowAtlas
    ShadowManager --> ShadowConfig
    CascadedShadowMap --> CascadeData
    CascadedShadowMap --> ShadowConfig
    ShadowAtlas --> ShadowAtlasAllocation
    RayTracedShadowRenderer --> RayTracedShadowConfig
    RayTracedShadowRenderer --> BlasHandle
    RayTracedShadowRenderer --> TlasHandle
```

### Implementation Status

| Class | Header Declared | CPP Implemented | Match Status |
|-------|-----------------|-----------------|--------------|
| ShadowConfig | ✅ Full | ✅ Full | ✅ Match |
| CascadeData | ✅ Full | ✅ Full | ✅ Match |
| GpuCascadeData | ✅ Full | ✅ Full | ✅ Match |
| GpuShadowData | ✅ Full | ✅ Full | ✅ Match |
| CascadedShadowMap | ✅ Full | ✅ Full | ✅ Match |
| ShadowAtlas | ✅ Full | ✅ Full | ✅ Match |
| ShadowManager | ✅ Full | ✅ Full | ✅ Match |
| RayTracedShadowConfig | ✅ Full | ✅ Full | ✅ Match |
| RayTracedShadowRenderer | ✅ Full | ✅ Full | ✅ Match |

---

## 4. Debug Subsystem

### Header Declaration (debug.hpp)

```mermaid
classDiagram
    class DebugVertex {
        +vec3 position
        +vec4 color
        +DebugVertex()
        +DebugVertex(pos, color)
    }

    class TextRequest {
        +vec3 position
        +string text
        +vec4 color
    }

    class FrameStats {
        +float frame_time_ms
        +float cpu_time_ms
        +float gpu_time_ms
        +float fps
        +float min_frame_time_ms
        +float max_frame_time_ms
        +uint32_t draw_calls
        +uint32_t triangles
        +uint32_t vertices
        +uint64_t gpu_memory_used
        +uint64_t texture_memory
        +uint64_t buffer_memory
        +time_point frame_start
        +begin_frame() void
        +end_frame() void
    }

    class DebugRenderer {
        -uint32_t m_vao
        -uint32_t m_vbo
        -unique_ptr~ShaderProgram~ m_shader
        -size_t m_max_vertices
        -vector~DebugVertex~ m_line_vertices
        -vector~DebugVertex~ m_triangle_vertices
        -vector~TextRequest~ m_text_requests
        +DebugRenderer()
        +~DebugRenderer()
        +initialize(max_vertices) bool
        +shutdown() void
        +begin_frame() void
        +end_frame() void
        +draw_line(start, end, color) void
        +draw_box(min, max, color) void
        +draw_box(aabb, color) void
        +draw_sphere(center, radius, color, segments) void
        +draw_sphere(sphere, color, segments) void
        +draw_frustum(frustum, color) void
        +draw_ray(ray, length, color) void
        +draw_axis(position, size) void
        +draw_grid(center, size, divisions, color) void
        +draw_transform(transform, size) void
        +draw_text_3d(position, text, color) void
        +render(view_projection) void
        -create_shader() bool
    }

    class StatsCollector {
        -FrameStats m_current_stats
        -time_point m_frame_start
        -vector~float~ m_frame_times
        -size_t m_history_size
        -uint64_t m_frame_count
        +StatsCollector()
        +begin_frame() void
        +end_frame() void
        +record_draw_call() void
        +record_triangles(count) void
        +record_vertices(count) void
        +record_gpu_time(ms) void
        +get_stats() FrameStats
        +frame_time_history() span~float~
        +reset() void
    }

    class DebugOverlayConfig {
        +bool show_fps
        +bool show_frame_time
        +bool show_draw_calls
        +bool show_triangle_count
        +bool show_gpu_memory
        +vec2 position
        +float scale
        +vec4 text_color
        +vec4 background_color
    }

    class DebugOverlay {
        -bool m_visible
        -bool m_show_fps
        -FrameStats m_stats
        -map~string,string~ m_text_entries
        +DebugOverlay()
        +~DebugOverlay()
        +initialize() bool
        +shutdown() void
        +set_stats(stats) void
        +add_text(key, value) void
        +remove_text(key) void
        +clear_text() void
        +render(width, height) void
        +set_visible(visible) void
        +is_visible() bool
        +set_show_fps(show) void
    }

    DebugRenderer --> DebugVertex
    DebugRenderer --> TextRequest
    StatsCollector --> FrameStats
    DebugOverlay --> FrameStats
    DebugOverlay --> DebugOverlayConfig
```

### Global Functions

```cpp
bool init_debug_rendering(size_t max_vertices = 65536);
void shutdown_debug_rendering();
DebugRenderer* get_debug_renderer();
StatsCollector* get_stats_collector();
DebugOverlay* get_debug_overlay();
```

### Implementation Status

| Class | Header Declared | CPP Implemented | Match Status |
|-------|-----------------|-----------------|--------------|
| DebugVertex | ✅ Full | ✅ Full | ✅ Match |
| TextRequest | ✅ Full | ✅ Full | ✅ Match |
| FrameStats | ✅ Full | ✅ Full | ✅ Match |
| DebugRenderer | ✅ Full | ✅ Full | ✅ Match |
| StatsCollector | ✅ Full | ✅ Full | ✅ Match |
| DebugOverlay | ✅ Full | ⚠️ Partial | ⚠️ Incomplete |
| Global Functions | ✅ Declared | ✅ Implemented | ✅ Match |

---

## 5. Instancing Subsystem

### Header Declaration (instancing.hpp)

```mermaid
classDiagram
    class InstanceData {
        <<aligned 16, 144 bytes>>
        +array~array~float,4~,4~ model_matrix
        +array~array~float,4~,3~ normal_matrix
        +array~float,4~ color_tint
        +array~float,4~ custom
        +SIZE$ size_t
        +InstanceData()
        +InstanceData(model, tint)
        +InstanceData(model, tint, custom)
        +from_position()$ InstanceData
        +from_position_scale()$ InstanceData
        +from_transform()$ InstanceData
        +from_glm()$ InstanceData
        +compute_normal_matrix() void
        +set_position(x, y, z) void
        +set_scale(sx, sy, sz) void
    }

    class BatchKey {
        +uint64_t mesh_id
        +uint64_t material_id
        +uint32_t layer_mask
        +BatchKey()
        +BatchKey(mesh, mat, layer)
        +operator<() bool
        +operator==() bool
    }

    class InstanceBuffer {
        -uint32_t m_buffer
        -size_t m_capacity
        -size_t m_stride
        -size_t m_count
        +InstanceBuffer()
        +~InstanceBuffer()
        +InstanceBuffer(InstanceBuffer&&)
        +operator=(InstanceBuffer&&)
        +initialize(capacity, stride) bool
        +destroy() void
        +resize(new_capacity) void
        +update(data, count) void
        +update_range(data, offset, count) void
        +bind() void
        +unbind()$ void
        +clear() void
        +buffer() uint32_t
        +count() size_t
        +capacity() size_t
    }

    class IndirectDrawBuffer {
        -uint32_t m_buffer
        -size_t m_capacity
        +IndirectDrawBuffer()
        +~IndirectDrawBuffer()
        +IndirectDrawBuffer(IndirectDrawBuffer&&)
        +operator=(IndirectDrawBuffer&&)
        +initialize(max_commands) bool
        +destroy() void
        +buffer() uint32_t
    }

    class InstanceBatch {
        -vector~InstanceData~ m_instances
        -InstanceBuffer m_buffer
        -bool m_dirty
        +InstanceBatch()
        +~InstanceBatch()
        +InstanceBatch(InstanceBatch&&)
        +operator=(InstanceBatch&&)
        +initialize(capacity) bool
        +add(instance) void
        +add_bulk(instances) void
        +clear() void
        +upload() void
        +bind() void
        +size() size_t
        +count() size_t
        +empty() bool
        +instances() vector~InstanceData~
    }

    class InstanceBatcherConfig {
        +size_t max_batch_size
        +size_t max_batches
        +bool auto_upload
    }

    class InstanceBatcher {
        -Config m_config
        -map~BatchKey,InstanceBatch~ m_batches
        -map~BatchKey,size_t~ m_batch_lookup
        -uint32_t m_draw_calls
        -uint64_t m_instances_rendered
        +InstanceBatcher()
        +~InstanceBatcher()
        +initialize(config) bool
        +begin_frame() void
        +submit(key, instance) void
        +submit_bulk(key, instances) void
        +end_frame() void
        +for_each_batch(callback) void
        +batch_count() size_t
        +draw_calls() uint32_t
        +instances_rendered() uint64_t
    }

    class InstanceRendererStats {
        +uint32_t draw_calls
        +uint64_t instances_rendered
        +uint64_t triangles_rendered
    }

    class InstanceRenderer {
        -size_t m_max_instances
        -InstanceBuffer m_instance_buffer
        -IndirectDrawBuffer m_indirect_buffer
        -InstanceBatcher m_batcher
        -vector~InstanceData~ m_staging_instances
        -Stats m_draw_stats
        +InstanceRenderer()
        +~InstanceRenderer()
        +initialize(max_instances) bool
        +shutdown() void
        +begin_frame() void
        +submit(mesh, material, instance) void
        +submit_batch(mesh, material, instances) void
        +end_frame() void
        +render_batch(key, batch, vao, index_count) void
        +render_all(setup_callback, get_mesh) void
        +stats() Stats
    }

    class MaterialHandle {
        +uint64_t id
        +is_valid() bool
    }

    InstanceBatcher --> InstanceBatcherConfig
    InstanceBatcher --> InstanceBatch
    InstanceBatcher --> BatchKey
    InstanceBatch --> InstanceData
    InstanceBatch --> InstanceBuffer
    InstanceRenderer --> InstanceRendererStats
    InstanceRenderer --> InstanceBuffer
    InstanceRenderer --> IndirectDrawBuffer
    InstanceRenderer --> InstanceBatcher
```

### Implementation Status

| Class | Header Declared | CPP Implemented | Match Status |
|-------|-----------------|-----------------|--------------|
| InstanceData | ✅ Full | ✅ Full | ✅ Match |
| BatchKey | ✅ Full | ✅ Full | ✅ Match |
| InstanceBuffer | ✅ Full | ✅ Full | ✅ Match |
| IndirectDrawBuffer | ✅ Full | ✅ Full | ✅ Match |
| InstanceBatch | ✅ Full | ✅ Full | ✅ Match |
| InstanceBatcher | ✅ Full | ✅ Full | ✅ Match |
| InstanceRenderer | ✅ Full | ✅ Full | ✅ Match |
| MaterialHandle | ✅ Full | ✅ Full | ✅ Match |

---

## 6. Spatial Subsystem

### Header Declaration (spatial.hpp)

```mermaid
classDiagram
    class Ray {
        +vec3 origin
        +vec3 direction
        +from_points()$ Ray
        +from_screen()$ Ray
        +from_pixel()$ Ray
        +point_at(t) vec3
        +at(t) vec3
        +at_array(t) array~float,3~
    }

    class AABB {
        +array~float,3~ min
        +array~float,3~ max
        +from_center_extents()$ AABB
        +from_points()$ AABB
        +unit()$ AABB
        +merge()$ AABB
        +is_valid() bool
        +center() array
        +extents() array
        +size() array
        +surface_area() float
        +volume() float
        +longest_axis() int
        +expand(point) void
        +expand(aabb) void
        +contains(point) bool
        +intersects(aabb) bool
        +intersect_ray(ray) optional
        +transformed(matrix) AABB
        +ray_intersect(ray) bool
    }

    class BoundingSphere {
        +vec3 center
        +float radius
        +from_points()$ BoundingSphere
        +from_aabb()$ BoundingSphere
        +contains(point) bool
        +intersects(sphere) bool
        +intersect_ray(ray) optional
        +transformed(matrix) BoundingSphere
        +ray_intersect(ray) bool
    }

    class RayHit {
        +bool hit
        +float distance
        +vec3 point
        +vec3 normal
        +uint64_t entity_id
        +uint32_t mesh_index
        +uint32_t triangle_index
        +float u, v
        +miss()$ RayHit
        +is_closer_than(other) bool
    }

    class BVHNode {
        +AABB bounds
        +uint32_t left_child
        +uint32_t right_child
        +uint32_t first_primitive
        +uint32_t primitive_count
        +is_leaf() bool
        +primitive_range() pair
    }

    class BVHPrimitive {
        +AABB bounds
        +vec3 centroid
        +uint64_t entity_id
        +uint32_t original_index
    }

    class BVHHitResult {
        +bool hit
        +uint32_t primitive_index
        +uint64_t entity_id
        +float distance
        +vec3 point
    }

    class BVH {
        -vector~BVHNode~ m_nodes
        -vector~uint32_t~ m_primitive_indices
        +build(primitives) void
        +intersect(ray) HitResult
        +ray_intersect(ray) HitResult
        +query_aabb(aabb) vector
        +query_frustum(frustum) vector
        +query_sphere(sphere) vector
        +clear() void
        +node_count() size_t
        +primitive_count() size_t
        +nodes() span
        +primitive_indices() span
        -build_recursive() uint32_t
    }

    class PickResult {
        +uint64_t id
        +vec3 position
        +vec3 normal
        +float distance
    }

    class PickingResult {
        +bool hit
        +uint64_t entity_id
        +vec3 world_position
        +vec3 world_normal
        +float distance
        +vec2 screen_position
        +float depth
        +miss()$ PickingResult
    }

    class PickingManager {
        -map m_objects
        -BVH* m_bvh
        +set_bvh(bvh) void
        +get_bvh() BVH*
        +pick_ray(ray) RayHit
        +register_object(id, bounds) void
        +unregister_object(id) void
        +update_object(id, bounds) void
        +pick(ray) PickResult
        +pick_all(ray) vector~PickResult~
        +query_frustum(frustum) vector
        +clear() void
    }

    class SpatialHashCellKey {
        +int x, y, z
        +operator==() bool
    }

    class SpatialHash {
        -float m_cell_size
        -float m_inv_cell_size
        -map m_cells
        -map m_object_bounds
        +insert(id, aabb) void
        +insert(id, point) void
        +remove(id) void
        +update(id, aabb) void
        +query(aabb) vector
        +query_point(point) vector
        +clear() void
        +cell_size() float
        -get_cell_key(point) CellKey
        -get_cell_range(aabb) pair
    }

    BVH --> BVHNode
    BVH --> BVHPrimitive
    BVH --> BVHHitResult
    PickingManager --> BVH
    PickingManager --> PickResult
    PickingManager --> PickingResult
    SpatialHash --> SpatialHashCellKey
```

### Implementation Status

| Class | Header Declared | CPP Implemented | Match Status |
|-------|-----------------|-----------------|--------------|
| Ray | ✅ Full | ✅ Full | ✅ Match |
| AABB | ✅ Full | ✅ Full | ✅ Match |
| BoundingSphere | ✅ Full | ✅ Full | ✅ Match |
| RayHit | ✅ Full | ✅ Full | ✅ Match |
| BVHNode | ✅ Full | ✅ Full | ✅ Match |
| BVH | ✅ Full | ✅ Full | ✅ Match |
| PickingManager | ✅ Full | ✅ Full | ✅ Match |
| SpatialHash | ✅ Full | ✅ Full | ✅ Match |

---

## 7. Mesh Subsystem

### Header Declaration (mesh.hpp)

```mermaid
classDiagram
    class Vertex {
        <<aligned 16, 80 bytes>>
        +array~float,3~ position
        +float _pad0
        +array~float,3~ normal
        +float _pad1
        +array~float,4~ tangent
        +array~float,2~ uv0
        +array~float,2~ uv1
        +array~float,4~ color
        +Vertex()
        +Vertex(x, y, z)
        +Vertex(pos, normal)
        +Vertex(full params)
    }

    class MeshData {
        -vector~Vertex~ m_vertices
        -vector~uint32_t~ m_indices
        -PrimitiveTopology m_topology
        +quad()$ MeshData
        +plane()$ MeshData
        +cube()$ MeshData
        +sphere()$ MeshData
        +cylinder()$ MeshData
        +vertices() vector~Vertex~
        +indices() vector~uint32_t~
        +topology() PrimitiveTopology
        +set_topology(topo) void
        +vertex_count() size_t
        +index_count() size_t
        +is_indexed() bool
        +triangle_count() size_t
        +clear() void
        +reserve_vertices(count) void
        +reserve_indices(count) void
        +add_vertex(vertex) void
        +add_index(index) void
        +add_triangle(i0, i1, i2) void
    }

    class MeshHandle {
        +uint64_t asset_id
        +uint64_t generation
        +is_valid() bool
        +operator==() bool
    }

    class GpuVertexBuffer {
        +uint32_t id
        +uint32_t vertex_count
        +uint32_t stride
        +size_t size_bytes
        +is_valid() bool
    }

    class GpuIndexBuffer {
        +uint32_t id
        +uint32_t index_count
        +IndexFormat format
        +size_t size_bytes
        +is_valid() bool
    }

    class CachedPrimitive {
        +uint32_t index
        +GpuVertexBuffer vertex_buffer
        +GpuIndexBuffer index_buffer
        +uint32_t triangle_count
        +uint32_t material_index
    }

    class CachedMesh {
        +uint64_t asset_id
        +string path
        +vector~CachedPrimitive~ primitives
        +size_t gpu_memory
        +uint32_t ref_count
        +uint64_t last_access_frame
    }

    class MeshCacheStats {
        +uint32_t mesh_count
        +uint32_t primitive_count
        +size_t memory_used
        +size_t memory_budget
        +uint64_t cache_hits
        +uint64_t cache_misses
        +uint64_t evictions
    }

    class MeshCache {
        -map m_meshes
        -map m_path_to_id
        -atomic m_next_buffer_id
        -atomic m_next_asset_id
        -uint64_t m_current_frame
        -size_t m_memory_budget
        -atomic m_memory_usage
        -atomic m_generation
        -MeshCacheStats m_stats
        +get_or_load(path) MeshHandle
        +get(handle) CachedMesh*
        +add(mesh_data) MeshHandle
        +release(handle) void
        +begin_frame() void
        +stats() MeshCacheStats
        +memory_budget() size_t
        +memory_usage() size_t
        +clear() void
        -evict_lru() void
    }

    MeshData --> Vertex
    MeshCache --> CachedMesh
    MeshCache --> MeshHandle
    MeshCache --> MeshCacheStats
    CachedMesh --> CachedPrimitive
    CachedPrimitive --> GpuVertexBuffer
    CachedPrimitive --> GpuIndexBuffer
```

---

## 8. Post-Processing Subsystem

### ❌ MISSING HEADER - Needs Creation

**File needed**: `include/void_engine/render/post_process.hpp`

```mermaid
classDiagram
    class TonemapOperator {
        <<enumeration>>
        Reinhard
        ACES
        Uncharted2
        Linear
    }

    class PostProcessConfig {
        +bool bloom_enabled
        +float bloom_threshold
        +float bloom_intensity
        +float bloom_radius
        +uint32_t bloom_mip_count
        +bool ssao_enabled
        +float ssao_radius
        +float ssao_bias
        +float ssao_intensity
        +uint32_t ssao_kernel_size
        +TonemapOperator tonemap_operator
        +float exposure
        +float gamma
        +bool fxaa_enabled
        +float fxaa_subpixel
        +float fxaa_edge_threshold
        +float fxaa_edge_threshold_min
        +bool vignette_enabled
        +float vignette_intensity
        +float vignette_smoothness
        +bool grain_enabled
        +float grain_intensity
        +bool chromatic_aberration_enabled
        +float chromatic_aberration_intensity
    }

    class Framebuffer {
        +uint32_t fbo
        +uint32_t color_texture
        +uint32_t depth_texture
        +uint32_t width, height
        +create(w, h, depth) bool
        +destroy() void
        +bind() void
        +unbind()$ void
    }

    class PostProcessPipeline {
        -PostProcessConfig m_config
        -uint32_t m_width, m_height
        -vector~Framebuffer~ m_bloom_mips
        -Framebuffer m_ssao_buffer
        -Framebuffer m_ssao_blur
        -Framebuffer m_temp_buffer
        -unique_ptr m_bloom_downsample_shader
        -unique_ptr m_bloom_upsample_shader
        -unique_ptr m_ssao_shader
        -unique_ptr m_ssao_blur_shader
        -unique_ptr m_tonemap_shader
        -unique_ptr m_fxaa_shader
        -unique_ptr m_composite_shader
        -vector~vec3~ m_ssao_kernel
        -uint32_t m_ssao_noise_texture
        -uint32_t m_quad_vao
        +initialize(w, h, config) bool
        +shutdown() void
        +resize(w, h) void
        +process(input, output) void
        +config() PostProcessConfig
        -create_shaders() bool
        -create_ssao_data() void
        -create_fullscreen_quad() void
        -render_fullscreen_quad() void
        -apply_bloom() void
        -apply_ssao() void
        -apply_tonemapping() void
        -apply_fxaa() void
    }

    PostProcessPipeline --> PostProcessConfig
    PostProcessPipeline --> Framebuffer
    PostProcessConfig --> TonemapOperator
```

### Global Functions to Declare

```cpp
bool init_post_processing(uint32_t width, uint32_t height, const PostProcessConfig& config);
void shutdown_post_processing();
void resize_post_processing(uint32_t width, uint32_t height);
void apply_post_processing(uint32_t input_texture, uint32_t output_fbo);
```

---

## 9. GLTF Loading Subsystem

### ❌ MISSING HEADER - Needs Creation

**File needed**: `include/void_engine/render/gltf.hpp`

```mermaid
classDiagram
    class Transform {
        +array~float,3~ translation
        +array~float,4~ rotation
        +array~float,3~ scale
        +multiply()$ Transform
        +to_matrix() mat4
    }

    class GltfPrimitive {
        +MeshData mesh_data
        +int material_index
        +array~float,3~ min_bounds
        +array~float,3~ max_bounds
    }

    class GltfMesh {
        +string name
        +vector~GltfPrimitive~ primitives
    }

    class GltfNode {
        +string name
        +Transform local_transform
        +mat4 world_matrix
        +optional~size_t~ mesh_index
        +optional~size_t~ skin_index
        +optional~size_t~ camera_index
        +optional~size_t~ light_index
        +vector~size_t~ children
        +optional~size_t~ parent
    }

    class GltfMaterial {
        +Material gpu_material
        +string name
        +optional base_color_texture
        +optional normal_texture
        +optional metallic_roughness_texture
        +optional occlusion_texture
        +optional emissive_texture
    }

    class GltfTexture {
        +string name
        +string uri
        +TextureData data
        +FilterMode min_filter
        +FilterMode mag_filter
        +AddressMode wrap_s
        +AddressMode wrap_t
    }

    class GltfScene {
        +string name
        +string source_path
        +vector~GltfNode~ nodes
        +vector~GltfMesh~ meshes
        +vector~GltfMaterial~ materials
        +vector~GltfTexture~ textures
        +vector~size_t~ root_nodes
        +vec3 min_bounds
        +vec3 max_bounds
        +size_t total_vertices
        +size_t total_triangles
        +size_t total_draw_calls
    }

    class GltfLoadOptions {
        +bool load_textures
        +bool generate_tangents
        +bool flip_uvs
        +bool merge_primitives
        +float scale
    }

    class GltfLoader {
        -GltfLoadOptions m_options
        -string m_base_path
        -string m_last_error
        +default_options()$ GltfLoadOptions
        +load(path) optional~GltfScene~
        +load(path, options) optional~GltfScene~
        +last_error() string
        -load_textures() void
        -load_materials() void
        -load_material_extensions() void
        -load_meshes() void
        -load_primitive_vertices() void
        -load_primitive_indices() void
        -generate_tangents() void
        -load_nodes() void
        -compute_world_transforms() void
        -compute_scene_bounds() void
    }

    class GltfSceneManagerEntry {
        +GltfScene scene
        +time_point last_modified
        +bool dirty
    }

    class GltfSceneManager {
        -map~string,SceneEntry~ m_scenes
        +load(path) GltfScene*
        +get(path) GltfScene*
        +get(path) const GltfScene*
        +check_hot_reload() void
        +is_dirty(path) bool
        +clear_dirty(path) void
        +count() size_t
        +remove(path) void
        +clear() void
    }

    GltfScene --> GltfNode
    GltfScene --> GltfMesh
    GltfScene --> GltfMaterial
    GltfScene --> GltfTexture
    GltfMesh --> GltfPrimitive
    GltfNode --> Transform
    GltfLoader --> GltfLoadOptions
    GltfLoader --> GltfScene
    GltfSceneManager --> GltfSceneManagerEntry
    GltfSceneManagerEntry --> GltfScene
```

---

## 10. LOD Subsystem

### ❌ MISSING HEADER - Needs Creation

**File needed**: `include/void_engine/render/lod.hpp`

```mermaid
classDiagram
    class LodLevel {
        +float screen_size_threshold
        +float distance_threshold
        +MeshHandle mesh
        +uint32_t triangle_count
    }

    class LodGroup {
        -vector~LodLevel~ m_levels
        -uint32_t m_current_level
        +add_level(level) void
        +select_by_distance(distance) uint32_t
        +select_by_screen_size(screen_size) uint32_t
        +current_level() uint32_t
        +level_count() size_t
        +get_level(idx) LodLevel
    }

    class SimplificationOptions {
        +float target_ratio
        +uint32_t target_triangles
        +bool preserve_borders
        +bool preserve_seams
        +float error_tolerance
    }

    class MeshSimplifier {
        +simplify(mesh, options) MeshData
        +simplify_to_ratio(mesh, ratio) MeshData
        +simplify_to_triangles(mesh, count) MeshData
    }

    class LodGeneratorConfig {
        +uint32_t level_count
        +array~float~ reduction_ratios
        +bool auto_generate_thresholds
    }

    class LodGenerator {
        +generate(mesh, config) LodGroup
        +generate_default(mesh) LodGroup
    }

    class LodManagerStats {
        +uint32_t total_objects
        +uint32_t visible_objects
        +uint64_t triangles_drawn
        +uint64_t triangles_culled
    }

    class LodManager {
        -map m_lod_groups
        -LodManagerStats m_stats
        +register_object(id, group) void
        +unregister_object(id) void
        +update(camera) void
        +get_mesh(id) MeshHandle
        +stats() LodManagerStats
    }

    class HlodNode {
        +AABB bounds
        +MeshHandle proxy_mesh
        +vector~uint64_t~ children
        +bool is_leaf
    }

    class HlodTree {
        -vector~HlodNode~ m_nodes
        -uint32_t m_root
        +build(objects) void
        +query(frustum, distance) vector
        +clear() void
    }

    LodGroup --> LodLevel
    LodGenerator --> LodGroup
    LodGenerator --> LodGeneratorConfig
    MeshSimplifier --> SimplificationOptions
    LodManager --> LodGroup
    LodManager --> LodManagerStats
    HlodTree --> HlodNode
```

---

## 11. Temporal Effects Subsystem

### ❌ MISSING HEADER - Needs Creation

**File needed**: `include/void_engine/render/temporal.hpp`

```mermaid
classDiagram
    class TaaConfig {
        +bool enabled
        +uint32_t sample_count
        +float feedback_min
        +float feedback_max
        +bool use_motion_vectors
        +bool use_velocity_rejection
        +float velocity_rejection_scale
    }

    class TaaPass {
        -TaaConfig m_config
        -uint32_t m_width, m_height
        -Texture m_history
        -Texture m_velocity
        -unique_ptr m_shader
        -uint32_t m_frame_index
        +initialize(w, h, config) bool
        +shutdown() void
        +resize(w, h) void
        +render(input, depth, motion, output) void
        +jitter_offset() vec2
        +config() TaaConfig
    }

    class MotionBlurConfig {
        +bool enabled
        +float intensity
        +uint32_t sample_count
        +float max_velocity
    }

    class MotionBlurPass {
        -MotionBlurConfig m_config
        -unique_ptr m_shader
        +initialize(config) bool
        +shutdown() void
        +render(input, velocity, output) void
        +config() MotionBlurConfig
    }

    class DepthOfFieldConfig {
        +bool enabled
        +float focus_distance
        +float focus_range
        +float bokeh_radius
        +uint32_t sample_count
    }

    class DepthOfFieldPass {
        -DepthOfFieldConfig m_config
        -Framebuffer m_coc_buffer
        -Framebuffer m_blur_buffer
        -unique_ptr m_coc_shader
        -unique_ptr m_blur_shader
        -unique_ptr m_composite_shader
        +initialize(w, h, config) bool
        +shutdown() void
        +resize(w, h) void
        +render(input, depth, output) void
        +config() DepthOfFieldConfig
    }

    TaaPass --> TaaConfig
    MotionBlurPass --> MotionBlurConfig
    DepthOfFieldPass --> DepthOfFieldConfig
```

---

## 12. Header Dependency Graph

```mermaid
graph TB
    subgraph "Core Layer"
        fwd[fwd.hpp]
        resource[resource.hpp]
    end

    subgraph "Foundation Layer"
        texture[texture.hpp]
        mesh[mesh.hpp]
        camera[camera.hpp]
        spatial[spatial.hpp]
    end

    subgraph "Rendering Layer"
        material[material.hpp]
        light[light.hpp]
        shadow[shadow.hpp]
        instancing[instancing.hpp]
        debug[debug.hpp]
    end

    subgraph "High-Level Layer"
        pass[pass.hpp]
        compositor[compositor.hpp]
        render_graph[render_graph.hpp]
        gl_renderer[gl_renderer.hpp]
    end

    subgraph "Missing Headers (to create)"
        gltf[gltf.hpp ❌]
        lod[lod.hpp ❌]
        temporal[temporal.hpp ❌]
        post_process[post_process.hpp ❌]
    end

    subgraph "Master Include"
        render[render.hpp]
    end

    fwd --> resource
    resource --> texture
    resource --> mesh
    fwd --> camera
    fwd --> spatial

    texture --> material
    mesh --> instancing
    camera --> spatial
    spatial --> debug

    material --> shadow
    light --> shadow
    instancing --> gl_renderer

    shadow --> pass
    pass --> compositor
    compositor --> render_graph

    mesh --> gltf
    mesh --> lod
    texture --> temporal
    texture --> post_process

    render --> texture
    render --> mesh
    render --> shadow
    render --> debug
    render --> instancing
    render --> spatial
    render --> gl_renderer
```

---

## 13. Identified Mismatches

### Critical Issues (Must Fix)

| Issue | File | Description | Priority |
|-------|------|-------------|----------|
| Missing Header | gltf_loader.cpp | No public header for GLTF types | HIGH |
| Missing Header | lod.cpp | No public header for LOD types | HIGH |
| Missing Header | temporal_effects.cpp | No public header for temporal effects | HIGH |
| Missing Header | post_process.cpp | No public header for post-processing | HIGH |

### Incomplete Implementations

| Class | Header | CPP | Issue |
|-------|--------|-----|-------|
| Sampler | texture.hpp | texture.cpp | Skeleton only |
| TextureLoader | texture.hpp | texture.cpp | Skeleton only |
| TextureManager | texture.hpp | texture.cpp | Skeleton only |
| IBLProcessor | texture.hpp | texture.cpp | Not implemented |
| DebugOverlay | debug.hpp | debug_renderer.cpp | Partial implementation |

### API Mismatches (Already Fixed)

| Class | Issue | Status |
|-------|-------|--------|
| DebugRenderer | Header had inline impl, cpp had GPU impl | ✅ Fixed |
| InstanceBatch | Missing count(), bind() methods | ✅ Fixed |
| InstanceBatcher | Different API (Config struct) | ✅ Fixed |
| InstanceRenderer | Not declared in original header | ✅ Fixed |

---

## 14. Migration Tasks

### Phase 1: Create Missing Headers (Priority: HIGH)

1. **Create `gltf.hpp`**
   - Declare: Transform, GltfPrimitive, GltfMesh, GltfNode, GltfMaterial, GltfTexture, GltfScene, GltfLoadOptions, GltfLoader, GltfSceneManager
   - Include in: render.hpp
   - Dependencies: mesh.hpp, texture.hpp, material.hpp

2. **Create `lod.hpp`**
   - Declare: LodLevel, LodGroup, SimplificationOptions, MeshSimplifier, LodGeneratorConfig, LodGenerator, LodManagerStats, LodManager, HlodNode, HlodTree
   - Include in: render.hpp
   - Dependencies: mesh.hpp, spatial.hpp

3. **Create `temporal.hpp`**
   - Declare: TaaConfig, TaaPass, MotionBlurConfig, MotionBlurPass, DepthOfFieldConfig, DepthOfFieldPass
   - Include in: render.hpp
   - Dependencies: texture.hpp, gl_renderer.hpp

4. **Create `post_process.hpp`**
   - Declare: TonemapOperator, PostProcessConfig, Framebuffer, PostProcessPipeline, global functions
   - Include in: render.hpp
   - Dependencies: texture.hpp, gl_renderer.hpp

### Phase 2: Complete Skeleton Implementations (Priority: MEDIUM)

1. **texture.cpp** - Complete:
   - Sampler full implementation
   - TextureLoader file I/O
   - TextureManager cache logic
   - IBLProcessor IBL generation

2. **debug_renderer.cpp** - Complete:
   - DebugOverlay text rendering

### Phase 3: Update render.hpp (Priority: LOW)

1. Add includes for new headers:
   ```cpp
   #include "gltf.hpp"
   #include "lod.hpp"
   #include "temporal.hpp"
   #include "post_process.hpp"
   ```

### Phase 4: Testing

1. Build each module individually
2. Run existing tests
3. Add unit tests for new APIs

---

## Files to Modify Summary

| Action | File | Estimated Changes |
|--------|------|-------------------|
| CREATE | include/void_engine/render/gltf.hpp | ~300 lines |
| CREATE | include/void_engine/render/lod.hpp | ~250 lines |
| CREATE | include/void_engine/render/temporal.hpp | ~200 lines |
| CREATE | include/void_engine/render/post_process.hpp | ~200 lines |
| MODIFY | include/void_engine/render/render.hpp | ~10 lines |
| COMPLETE | src/render/texture.cpp | Implement skeletons |
| COMPLETE | src/render/debug_renderer.cpp | Implement DebugOverlay |

---

## Notes for Implementation

1. **All classes in `void_render` namespace**
2. **Use `#pragma once` for header guards**
3. **Follow existing patterns**: Non-copyable with deleted copy ops, move semantics where appropriate
4. **GPU-aligned structs**: Use `alignas(16)` for GPU data
5. **Use glm types**: `glm::vec3`, `glm::vec4`, `glm::mat4` for math
6. **Include order**: fwd.hpp first, then dependencies, then STL headers

---

*Document generated: 2026-01-26*
*For use in fresh Claude Code session*
