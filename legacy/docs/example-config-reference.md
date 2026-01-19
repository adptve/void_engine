# Void GUI Example Configuration Reference

> **Purpose:** This document provides a complete specification for the configuration files used by Void GUI applications. Use this as a reference for building editors that generate valid configuration output.

## Table of Contents

1. [Overview](#overview)
2. [Directory Structure](#directory-structure)
3. [manifest.toml - Package Configuration](#manifesttoml---package-configuration)
4. [scene.toml - Scene Definition](#scenetoml---scene-definition)
5. [Camera System](#camera-system)
6. [Lighting System](#lighting-system)
7. [Shadow Mapping](#shadow-mapping)
8. [Advanced Materials](#advanced-materials)
9. [Animation System](#animation-system)
10. [Animation Blending](#animation-blending)
11. [Picking and Raycasting](#picking-and-raycasting)
12. [Entity Input Events](#entity-input-events)
13. [Multi-Pass Rendering](#multi-pass-rendering)
14. [Custom Render Passes](#custom-render-passes)
15. [Spatial Queries](#spatial-queries)
16. [LOD System](#lod-system)
17. [Scene Streaming](#scene-streaming)
18. [Precision Management](#precision-management)
19. [Debug and Introspection](#debug-and-introspection)
20. [Shader Files (.wgsl)](#shader-files-wgsl)
21. [Asset Organization](#asset-organization)
22. [Complete Schema Reference](#complete-schema-reference)
23. [Editor Generation Requirements](#editor-generation-requirements)

---

## Overview

Void GUI applications consist of two primary approaches:

| Type | App Type | Entry Point | Use Case |
|------|----------|-------------|----------|
| **Declarative** | `tool` | `scene.toml` | Static scenes, model viewers, level editors |
| **Scripted** | `demo` | `scripts/main.vs` | Dynamic simulations, games, interactive demos |

The **declarative approach** (model-viewer) is fully data-driven and requires no code - everything is defined in TOML configuration files. This is the primary format an editor should generate.

---

## Directory Structure

### Declarative Application (model-viewer)
```
app-name/
├── manifest.toml                    # Package configuration (REQUIRED)
├── scene.toml                       # Scene definition (REQUIRED for app_type="tool")
├── assets/
│   └── shaders/
│       ├── pbr.wgsl                 # PBR material shader
│       ├── grid.wgsl                # Ground grid shader
│       └── sky.wgsl                 # Sky/environment shader
├── models/
│   ├── character.gltf               # glTF models
│   └── props/
│       └── crate.glb                # Binary glTF
└── textures/
    ├── env/
    │   └── environment.hdr          # HDR environment map
    └── MaterialName/
        ├── MaterialName_Color.png       # Albedo (sRGB)
        ├── MaterialName_NormalGL.png    # Normal map (Linear)
        ├── MaterialName_Roughness.png   # Roughness (Linear)
        └── MaterialName_Metalness.png   # Metallic (Linear)
```

### Scripted Application (nebula-genesis)
```
app-name/
├── manifest.toml                    # Package configuration (REQUIRED)
├── scripts/
│   ├── main.vs                      # Entry point (REQUIRED for app_type="demo")
│   └── systems/
│       └── *.vs                     # Additional modules
└── assets/
    └── shaders/
        └── *.wgsl                   # Custom shaders
```

---

## manifest.toml - Package Configuration

The manifest defines package metadata, layer composition, resource limits, render passes, streaming configuration, LOD settings, and platform requirements.

### Complete Schema

```toml
# =============================================================================
# PACKAGE METADATA
# =============================================================================
[package]
name = "my-app"                      # string (required) - Unique identifier, lowercase with hyphens
display_name = "My Application"       # string (required) - Human-readable name
version = "1.0.0"                    # string (required) - Semantic version (MAJOR.MINOR.PATCH)
description = "Description here"      # string (required) - App description
author = "Author Name"               # string (required) - Author or team name
license = "MIT"                      # string (required) - License identifier
keywords = ["3d", "viewer"]          # array<string> (optional) - Search keywords
categories = ["demo", "tools"]       # array<string> (optional) - App categories

# =============================================================================
# APPLICATION CONFIGURATION
# =============================================================================
[app]
app_type = "tool"                    # string (required) - "tool" (declarative) or "demo" (scripted)
scene = "scene.toml"                 # string (conditional) - Scene file path (required if app_type="tool")
entry = "scripts/main.vs"            # string (conditional) - Script entry (required if app_type="demo")

# =============================================================================
# RENDER LAYERS
# =============================================================================
# Layers define the rendering pipeline. Each layer is rendered in priority order
# and composited using the specified blend mode.

[[app.layers]]
name = "sky"                         # string (required) - Unique layer identifier
type = "content"                     # string (required) - Layer type (see below)
priority = -100                      # integer (required) - Render order (lower = first)
blend = "replace"                    # string (required) - Blend mode (see below)

# Repeat [[app.layers]] for each layer...

# -----------------------------------------------------------------------------
# Layer Types:
# - "content"  : Main scene content (geometry, objects)
# - "overlay"  : UI elements, HUD, particles
# - "effect"   : Post-processing effects (bloom, color grading)
# - "portal"   : Render-to-texture for portals/mirrors
# - "shadow"   : Shadow map generation layer
#
# Blend Modes:
# - "replace"  : Completely replace previous content
# - "normal"   : Standard alpha blending
# - "additive" : Add colors together (good for glow, particles)
# - "multiply" : Multiply colors (good for shadows, tinting)
#
# Priority Guidelines:
# - Shadow Maps:      -200 to -150
# - Sky/Background:    -100 to -50
# - Ground/Grid:       -50 to -10
# - World Content:     0 (default)
# - Transparent:       10 to 40
# - Particles/Effects: 50 to 100
# - UI/Overlay:        100 to 200
# - Debug:             200 to 300
# -----------------------------------------------------------------------------

# =============================================================================
# CUSTOM RENDER PASSES (Phase 13)
# =============================================================================
# Define custom render passes for specialized rendering effects

[[app.render_passes]]
name = "shadow_cascade_0"            # string (required) - Pass identifier
type = "shadow"                      # string (required) - Pass type (see below)
priority = -200                      # integer (required) - Execution order
enabled = true                       # boolean (optional) - Default: true

# Pass-specific configuration
[app.render_passes.config]
resolution = [2048, 2048]            # array<int>[2] (optional) - Render target size
format = "depth32float"              # string (optional) - Texture format
clear_color = [0.0, 0.0, 0.0, 1.0]  # array<float>[4] (optional) - Clear color
clear_depth = 1.0                    # float (optional) - Clear depth value
entity_filter = "shadow_casters"     # string (optional) - Which entities to render

# Pass dependencies (ensures proper ordering)
[app.render_passes.dependencies]
requires = []                        # array<string> - Passes that must complete first
provides = ["shadow_map_0"]          # array<string> - Resources this pass produces

# -----------------------------------------------------------------------------
# Render Pass Types:
# - "shadow"      : Shadow map generation
# - "depth"       : Depth pre-pass
# - "gbuffer"     : Geometry buffer (deferred rendering)
# - "lighting"    : Lighting calculation pass
# - "forward"     : Forward rendering pass
# - "transparent" : Transparent object pass
# - "post"        : Post-processing effect
# - "custom"      : User-defined pass
# -----------------------------------------------------------------------------

# =============================================================================
# LOD SYSTEM CONFIGURATION (Phase 15)
# =============================================================================
[app.lod]
enabled = true                       # boolean (optional) - Enable LOD system
mode = "distance"                    # string (optional) - "distance" or "screen_height"
default_mode = "cross_fade"          # string (optional) - Default transition mode
cross_fade_duration = 0.5            # float (optional) - Fade duration in seconds
dither_pattern = "bayer8"            # string (optional) - Dither pattern for dither mode
hysteresis = 0.1                     # float (optional) - Prevents LOD popping (0.0-1.0)
update_frequency = 60.0              # float (optional) - LOD updates per second

# -----------------------------------------------------------------------------
# LOD Modes:
# - "instant"    : Instant switch (may cause popping)
# - "cross_fade" : Smooth alpha fade between levels
# - "dither"     : Dithered transition (screen-door effect)
# - "morph"      : Vertex morphing (requires special mesh setup)
#
# Dither Patterns:
# - "bayer4"  : 4x4 Bayer matrix
# - "bayer8"  : 8x8 Bayer matrix
# - "noise"   : Blue noise pattern
# -----------------------------------------------------------------------------

# =============================================================================
# SCENE STREAMING CONFIGURATION (Phase 16)
# =============================================================================
[app.streaming]
enabled = true                       # boolean (optional) - Enable streaming
chunk_size = 64.0                    # float (optional) - Default chunk size (world units)
load_distance = 256.0                # float (optional) - Distance to start loading
unload_distance = 320.0              # float (optional) - Distance to unload chunks
max_concurrent_loads = 4             # integer (optional) - Parallel load limit
max_memory_mb = 512                  # integer (optional) - Memory budget for streamed content
preload_neighbors = true             # boolean (optional) - Preload adjacent chunks
priority_bias = "camera_direction"   # string (optional) - Load priority strategy

# Entity recycling settings
[app.streaming.recycling]
enabled = true                       # boolean (optional) - Enable ID recycling
pool_size = 1000                     # integer (optional) - Recycled ID pool size
cooldown_frames = 60                 # integer (optional) - Frames before ID reuse

# -----------------------------------------------------------------------------
# Priority Bias Strategies:
# - "camera_direction" : Prioritize chunks in view direction
# - "distance"         : Strictly by distance
# - "player_velocity"  : Anticipate movement direction
# -----------------------------------------------------------------------------

# =============================================================================
# PRECISION MANAGEMENT (Phase 17)
# =============================================================================
[app.precision]
enabled = true                       # boolean (optional) - Enable precision management
origin_rebase_threshold = 5000.0     # float (optional) - Distance before origin rebase
camera_relative_rendering = true     # boolean (optional) - Use camera-relative rendering
double_precision_physics = true      # boolean (optional) - Use f64 for physics
coordinate_system = "right_handed"   # string (optional) - Coordinate system convention

# =============================================================================
# DEBUG AND INTROSPECTION (Phase 18)
# =============================================================================
[app.debug]
enabled = false                      # boolean (optional) - Enable debug features
show_stats_overlay = false           # boolean (optional) - Show performance stats
stats_position = "top_left"          # string (optional) - Stats overlay position

# Debug visualization flags
[app.debug.visualization]
bounds = false                       # boolean - Show bounding boxes
normals = false                      # boolean - Show normal vectors
wireframe = false                    # boolean - Show wireframe overlay
light_volumes = false                # boolean - Show light influence volumes
shadow_cascades = false              # boolean - Show cascade boundaries
lod_colors = false                   # boolean - Color by LOD level
chunk_boundaries = false             # boolean - Show streaming chunk bounds
collision_shapes = false             # boolean - Show physics shapes
skeleton = false                     # boolean - Show animation skeletons
velocity_vectors = false             # boolean - Show entity velocities

# Debug appearance settings
[app.debug.appearance]
bounds_color = [0.0, 1.0, 0.0, 0.5]  # array<float>[4] - Bounding box color
normal_color = [0.0, 0.5, 1.0, 1.0]  # array<float>[4] - Normal vector color
normal_length = 0.1                  # float - Normal vector display length
line_width = 1.0                     # float - Debug line width
font_size = 14                       # integer - Stats overlay font size

# =============================================================================
# SPATIAL QUERY SETTINGS (Phase 14)
# =============================================================================
[app.spatial]
structure = "bvh"                    # string (optional) - "bvh", "octree", or "grid"
rebuild_threshold = 0.3              # float (optional) - Rebuild when 30% of entities moved
max_depth = 16                       # integer (optional) - Maximum tree depth
min_objects_per_node = 4             # integer (optional) - Split threshold

# =============================================================================
# PERMISSIONS
# =============================================================================
[app.permissions]
scripts = false                      # boolean (optional) - Allow VoidScript execution
                                     # Default: false for "tool", true for "demo"

# =============================================================================
# RESOURCE LIMITS
# =============================================================================
[app.resources]
max_entities = 1000                  # integer (required) - Maximum entity count
max_memory = 134217728               # integer (required) - Max memory in bytes (e.g., 128 MB)
max_layers = 5                       # integer (required) - Maximum render layers
max_cpu_ms = 16.0                    # float (required) - Max CPU time per frame in milliseconds
max_draw_calls = 5000                # integer (optional) - Draw call budget
max_triangles = 10000000             # integer (optional) - Triangle budget (10M default)
max_lights = 128                     # integer (optional) - Maximum active lights
max_shadow_casters = 8               # integer (optional) - Shadow-casting light limit

# Common memory values:
# 64 MB  = 67108864
# 128 MB = 134217728
# 256 MB = 268435456
# 512 MB = 536870912
# 1 GB   = 1073741824

# =============================================================================
# ASSET CONFIGURATION
# =============================================================================
[assets]
include = ["textures/", "models/"]   # array<string> (required) - Directories to include
compression = 6                      # integer (optional) - Compression level 0-9

# =============================================================================
# SCRIPTING (only for app_type="demo")
# =============================================================================
[scripts]
language = "voidscript"              # string (optional) - Script language identifier

# =============================================================================
# PLATFORM REQUIREMENTS
# =============================================================================
[platform]
min_version = "1.0.0"                # string (required) - Minimum Void GUI version
required_features = ["webgpu"]       # array<string> (required) - Required engine features

# Available features:
# - "webgpu"        : WebGPU rendering backend
# - "compute"       : Compute shader support
# - "xr"            : VR/AR support
# - "ray_tracing"   : Hardware ray tracing
# - "mesh_shaders"  : Mesh shader support
# - "bindless"      : Bindless texture support
```

### Example: Complete manifest.toml

```toml
[package]
name = "model-viewer"
display_name = "3D Model Viewer"
version = "2.0.0"
description = "Interactive 3D model viewer with PBR materials, animations, LOD, and streaming support"
author = "Void Engine Team"
license = "MIT"
keywords = ["3d", "pbr", "viewer", "model", "materials", "animation", "lod", "streaming"]
categories = ["demo", "tools", "visualization"]

[app]
app_type = "tool"
scene = "scene.toml"

[[app.layers]]
name = "shadow"
type = "shadow"
priority = -200
blend = "replace"

[[app.layers]]
name = "sky"
type = "content"
priority = -100
blend = "replace"

[[app.layers]]
name = "grid"
type = "content"
priority = -10
blend = "normal"

[[app.layers]]
name = "world"
type = "content"
priority = 0
blend = "normal"

[[app.layers]]
name = "transparent"
type = "content"
priority = 20
blend = "normal"

[[app.layers]]
name = "particles"
type = "overlay"
priority = 50
blend = "additive"

[[app.layers]]
name = "ui"
type = "overlay"
priority = 100
blend = "normal"

[[app.layers]]
name = "debug"
type = "overlay"
priority = 200
blend = "normal"

# Custom render passes for shadow mapping
[[app.render_passes]]
name = "shadow_cascade_0"
type = "shadow"
priority = -200
enabled = true

[app.render_passes.config]
resolution = [2048, 2048]
format = "depth32float"
clear_depth = 1.0
entity_filter = "shadow_casters"

[[app.render_passes]]
name = "shadow_cascade_1"
type = "shadow"
priority = -199
enabled = true

[app.render_passes.config]
resolution = [1024, 1024]
format = "depth32float"

[[app.render_passes]]
name = "shadow_cascade_2"
type = "shadow"
priority = -198
enabled = true

[app.render_passes.config]
resolution = [512, 512]
format = "depth32float"

# LOD configuration
[app.lod]
enabled = true
mode = "distance"
default_mode = "cross_fade"
cross_fade_duration = 0.3
hysteresis = 0.15

# Streaming configuration
[app.streaming]
enabled = true
chunk_size = 64.0
load_distance = 256.0
unload_distance = 320.0
max_concurrent_loads = 4
max_memory_mb = 512

[app.streaming.recycling]
enabled = true
pool_size = 1000

# Precision management for large worlds
[app.precision]
enabled = true
origin_rebase_threshold = 10000.0
camera_relative_rendering = true

# Debug settings (disabled by default)
[app.debug]
enabled = false
show_stats_overlay = false

[app.debug.visualization]
bounds = false
lod_colors = false
chunk_boundaries = false

# Spatial indexing
[app.spatial]
structure = "bvh"
rebuild_threshold = 0.3

[app.resources]
max_entities = 10000
max_memory = 536870912  # 512 MB
max_layers = 8
max_cpu_ms = 16.0
max_draw_calls = 10000
max_triangles = 50000000
max_lights = 256
max_shadow_casters = 4

[assets]
include = ["textures/", "models/"]

[platform]
min_version = "2.0.0"
required_features = ["webgpu"]
```

---

## scene.toml - Scene Definition

The scene file defines all entities, materials, animations, lights, shadows, cameras, particles, environment settings, LOD groups, streaming chunks, and input configuration.

### Complete Schema

```toml
# =============================================================================
# SCENE METADATA
# =============================================================================
[scene]
name = "My Scene"                    # string (required) - Scene name
description = "Scene description"     # string (required) - Description
version = "1.0.0"                    # string (required) - Scene version

# =============================================================================
# WORLD ORIGIN (Phase 17 - Precision Management)
# =============================================================================
[world_origin]
offset = [0.0, 0.0, 0.0]            # array<f64>[3] (optional) - World origin offset (double precision)
auto_rebase = true                   # boolean (optional) - Automatic origin rebasing

# =============================================================================
# ENVIRONMENT
# =============================================================================
[environment]
environment_map = "textures/env/studio.hdr"  # string (optional) - HDR environment map path
ambient_intensity = 0.25             # float (required) - Ambient light intensity

# Sky settings (used when no environment map or as fallback)
[environment.sky]
zenith_color = [0.1, 0.3, 0.6]       # array<float>[3] (required) - Sky top color RGB
horizon_color = [0.5, 0.7, 0.9]      # array<float>[3] (required) - Sky horizon color RGB
ground_color = [0.15, 0.12, 0.1]     # array<float>[3] (required) - Ground reflection color RGB
sun_size = 0.03                      # float (required) - Sun angular radius (0.0-1.0)
sun_intensity = 50.0                 # float (required) - Sun disc brightness
sun_falloff = 3.0                    # float (required) - Sun glow falloff exponent
fog_density = 0.0                    # float (required) - Volumetric fog (0.0 = none)

# =============================================================================
# INPUT CONFIGURATION
# =============================================================================
[input.camera]
orbit_button = "left"                # string (required) - Mouse button: "left", "right", "middle"
pan_button = "middle"                # string (required) - Mouse button for panning
zoom_scroll = true                   # boolean (required) - Enable scroll wheel zoom
orbit_sensitivity = 0.005            # float (required) - Mouse orbit speed
pan_sensitivity = 0.01               # float (required) - Mouse pan speed
zoom_sensitivity = 0.1               # float (required) - Scroll zoom speed
invert_y = false                     # boolean (required) - Invert Y axis
invert_x = false                     # boolean (required) - Invert X axis
min_distance = 0.5                   # float (required) - Minimum camera distance
max_distance = 50.0                  # float (required) - Maximum camera distance

[input.bindings]
# Key binding format: action_name = "Key" or "Modifier+Key"
#
# Modifiers: "Shift+", "Ctrl+", "Alt+"
# Keys: A-Z, 0-9, F1-F12, Tab, Space, Enter, Escape, etc.
#
# Examples:
next_material = "Tab"
prev_material = "Shift+Tab"
model_sphere = "1"
model_cube = "2"
reset_camera = "R"
toggle_wireframe = "F"
toggle_grid = "G"
toggle_debug = "F3"
screenshot = "F12"
```

---

## Camera System

Cameras define viewpoints in the scene. Multiple cameras can exist, with one marked as active.

```toml
# =============================================================================
# CAMERAS (Phase 2)
# =============================================================================
# Define cameras for the scene. One camera should be marked as active.

[[cameras]]
name = "main_camera"                 # string (required) - Unique camera identifier
active = true                        # boolean (required) - Is this the active camera?
type = "perspective"                 # string (required) - "perspective" or "orthographic"

[cameras.transform]
position = [0.0, 2.0, 5.0]          # array<float>[3] (required) - World position
target = [0.0, 0.0, 0.0]            # array<float>[3] (optional) - Look-at target
up = [0.0, 1.0, 0.0]                # array<float>[3] (optional) - Up vector

# Perspective camera settings
[cameras.perspective]
fov = 60.0                           # float (required) - Field of view in degrees
near = 0.1                           # float (required) - Near clip plane
far = 1000.0                         # float (required) - Far clip plane
aspect = "auto"                      # string or float - "auto" or explicit ratio

# -----------------------------------------------------------------------------

[[cameras]]
name = "ortho_camera"
active = false
type = "orthographic"

[cameras.transform]
position = [0.0, 10.0, 0.0]
target = [0.0, 0.0, 0.0]

# Orthographic camera settings
[cameras.orthographic]
left = -10.0                         # float (required) - Left plane
right = 10.0                         # float (required) - Right plane
bottom = -10.0                       # float (required) - Bottom plane
top = 10.0                           # float (required) - Top plane
near = 0.1                           # float (required) - Near clip plane
far = 100.0                          # float (required) - Far clip plane

# -----------------------------------------------------------------------------

# Top-down camera example
[[cameras]]
name = "top_down"
active = false
type = "orthographic"

[cameras.transform]
position = [0.0, 50.0, 0.0]
target = [0.0, 0.0, 0.0]
up = [0.0, 0.0, -1.0]               # Looking down, -Z is "up"

[cameras.orthographic]
left = -25.0
right = 25.0
bottom = -25.0
top = 25.0
near = 1.0
far = 100.0
```

---

## Lighting System

The lighting system supports multiple light types with physically-based parameters.

```toml
# =============================================================================
# LIGHTS (Phase 5)
# =============================================================================
# Define lights in the scene. Supports directional, point, spot, and area lights.

# --- DIRECTIONAL LIGHT (Sun/Moon) ---
[[lights]]
name = "sun"                         # string (required) - Unique light identifier
type = "directional"                 # string (required) - Light type
enabled = true                       # boolean (optional) - Default: true

[lights.directional]
direction = [0.5, -0.7, 0.5]        # array<float>[3] (required) - Light direction (normalized)
color = [1.0, 0.95, 0.85]           # array<float>[3] (required) - Light color RGB
intensity = 3.0                      # float (required) - Light intensity (lux for directional)
cast_shadows = true                  # boolean (optional) - Enable shadow casting

# -----------------------------------------------------------------------------

# --- POINT LIGHT (Omnidirectional) ---
[[lights]]
name = "lamp_01"
type = "point"
enabled = true

[lights.point]
position = [2.0, 3.0, 0.0]          # array<float>[3] (required) - World position
color = [1.0, 0.9, 0.7]             # array<float>[3] (required) - Light color RGB
intensity = 100.0                    # float (required) - Light intensity (lumens)
range = 10.0                         # float (required) - Maximum influence radius
cast_shadows = false                 # boolean (optional) - Enable shadow casting

# Attenuation model
[lights.point.attenuation]
constant = 1.0                       # float (optional) - Constant attenuation term
linear = 0.09                        # float (optional) - Linear attenuation term
quadratic = 0.032                    # float (optional) - Quadratic attenuation term

# -----------------------------------------------------------------------------

# --- SPOT LIGHT (Cone) ---
[[lights]]
name = "spotlight_01"
type = "spot"
enabled = true

[lights.spot]
position = [0.0, 5.0, 0.0]          # array<float>[3] (required) - World position
direction = [0.0, -1.0, 0.0]        # array<float>[3] (required) - Light direction
color = [1.0, 1.0, 1.0]             # array<float>[3] (required) - Light color RGB
intensity = 500.0                    # float (required) - Light intensity (lumens)
range = 15.0                         # float (required) - Maximum influence radius
inner_angle = 15.0                   # float (required) - Inner cone angle in degrees (full intensity)
outer_angle = 30.0                   # float (required) - Outer cone angle in degrees (falloff edge)
cast_shadows = true                  # boolean (optional) - Enable shadow casting

# -----------------------------------------------------------------------------

# --- AREA LIGHT (Rectangle/Disc) ---
[[lights]]
name = "panel_light"
type = "area"
enabled = true

[lights.area]
position = [0.0, 4.0, 0.0]          # array<float>[3] (required) - Center position
direction = [0.0, -1.0, 0.0]        # array<float>[3] (required) - Normal direction
color = [1.0, 1.0, 0.95]            # array<float>[3] (required) - Light color RGB
intensity = 1000.0                   # float (required) - Light intensity (lumens)
shape = "rectangle"                  # string (required) - "rectangle" or "disc"
width = 2.0                          # float (required for rectangle) - Width
height = 1.0                         # float (required for rectangle) - Height
radius = 0.5                         # float (required for disc) - Radius
two_sided = false                    # boolean (optional) - Emit from both sides

# -----------------------------------------------------------------------------
# Light Intensity Reference:
#
# Directional (lux):
# - Overcast day: 1,000-2,000
# - Cloudy day: 10,000-25,000
# - Direct sunlight: 50,000-100,000
#
# Point/Spot (lumens):
# - Candle: 12
# - 40W incandescent: 450
# - 60W incandescent: 800
# - 100W incandescent: 1,600
# - LED panel: 2,000-5,000
#
# Area (lumens per square meter):
# - Soft box: 1,000-5,000
# - LED panel: 5,000-20,000
# -----------------------------------------------------------------------------
```

---

## Shadow Mapping

Configure shadow rendering with cascaded shadow maps, atlas settings, and filtering.

```toml
# =============================================================================
# SHADOW CONFIGURATION (Phase 6)
# =============================================================================

[shadows]
enabled = true                       # boolean (optional) - Enable shadows globally
atlas_size = 4096                    # integer (optional) - Shadow atlas texture size
max_shadow_distance = 100.0          # float (optional) - Maximum shadow render distance
shadow_fade_distance = 10.0          # float (optional) - Distance over which shadows fade

# Cascaded Shadow Maps (for directional lights)
[shadows.cascades]
count = 4                            # integer (optional) - Number of cascades (1-4)
split_scheme = "practical"           # string (optional) - "uniform", "logarithmic", "practical"
lambda = 0.5                         # float (optional) - Blend factor for practical scheme (0-1)

# Per-cascade configuration
[[shadows.cascades.levels]]
resolution = 2048                    # integer - Cascade resolution
distance = 10.0                      # float - Maximum distance for this cascade
bias = 0.0005                        # float - Depth bias to prevent shadow acne

[[shadows.cascades.levels]]
resolution = 1024
distance = 25.0
bias = 0.001

[[shadows.cascades.levels]]
resolution = 512
distance = 50.0
bias = 0.002

[[shadows.cascades.levels]]
resolution = 512
distance = 100.0
bias = 0.003

# Shadow filtering
[shadows.filtering]
method = "pcf"                       # string (optional) - "none", "pcf", "pcss", "vsm"
pcf_samples = 16                     # integer (optional) - PCF sample count (4, 9, 16, 25)
pcf_radius = 1.5                     # float (optional) - PCF filter radius in texels
soft_shadows = true                  # boolean (optional) - Enable soft shadow edges
contact_hardening = false            # boolean (optional) - Distance-based softness (PCSS)

# -----------------------------------------------------------------------------
# Shadow Filtering Methods:
# - "none"  : Hard shadows, no filtering
# - "pcf"   : Percentage Closer Filtering (soft edges)
# - "pcss"  : Percentage Closer Soft Shadows (contact hardening)
# - "vsm"   : Variance Shadow Maps (very soft, may have light bleeding)
#
# Split Schemes:
# - "uniform"     : Equal distance splits
# - "logarithmic" : Logarithmic distribution (better near)
# - "practical"   : Blend of uniform and logarithmic (recommended)
# -----------------------------------------------------------------------------
```

---

## Advanced Materials

The material system supports advanced PBR features including clearcoat, transmission, subsurface scattering, sheen, anisotropy, and iridescence.

```toml
# =============================================================================
# ENTITIES WITH ADVANCED MATERIALS (Phase 7)
# =============================================================================

[[entities]]
name = "car_paint"
mesh = "sphere"
layer = "world"
visible = true

[entities.transform]
position = [0.0, 1.0, 0.0]
scale = 1.0

# Advanced PBR material with clearcoat
[entities.material]
albedo = [0.8, 0.1, 0.1]            # Base color (red)
metallic = 0.0
roughness = 0.3

# Clearcoat layer (car paint, lacquer)
[entities.material.clearcoat]
intensity = 1.0                      # float (0-1) - Clearcoat strength
roughness = 0.1                      # float (0-1) - Clearcoat roughness
normal_map = "textures/clearcoat_normal.png"  # string (optional) - Clearcoat normal

# -----------------------------------------------------------------------------

[[entities]]
name = "glass_sphere"
mesh = "sphere"
layer = "transparent"                # Note: Use transparent layer
visible = true

[entities.transform]
position = [2.0, 1.0, 0.0]
scale = 0.8

# Glass/transmission material
[entities.material]
albedo = [1.0, 1.0, 1.0]
metallic = 0.0
roughness = 0.0

# Transmission (glass, water, crystals)
[entities.material.transmission]
factor = 0.95                        # float (0-1) - Transmission amount (1 = fully transparent)
ior = 1.5                            # float - Index of refraction (glass ≈ 1.5, water ≈ 1.33)
thickness = 0.5                      # float - Material thickness for absorption
attenuation_color = [0.9, 0.95, 1.0] # array<float>[3] - Absorption tint color
attenuation_distance = 2.0           # float - Distance at which color fully applies
dispersion = 0.0                     # float (optional) - Chromatic dispersion amount

# -----------------------------------------------------------------------------

[[entities]]
name = "skin"
mesh = "sphere"
layer = "world"
visible = true

[entities.transform]
position = [-2.0, 1.0, 0.0]
scale = 0.8

# Subsurface scattering material (skin, wax, leaves)
[entities.material]
albedo = [0.9, 0.7, 0.6]
metallic = 0.0
roughness = 0.5

# Subsurface scattering
[entities.material.subsurface]
factor = 0.5                         # float (0-1) - Subsurface strength
radius = [1.0, 0.5, 0.25]           # array<float>[3] - Scattering radius per RGB channel
color = [1.0, 0.4, 0.3]             # array<float>[3] - Subsurface color tint

# -----------------------------------------------------------------------------

[[entities]]
name = "velvet_cloth"
mesh = "sphere"
layer = "world"
visible = true

[entities.transform]
position = [0.0, 1.0, 2.0]
scale = 0.7

# Fabric with sheen (velvet, satin)
[entities.material]
albedo = [0.2, 0.1, 0.3]            # Deep purple
metallic = 0.0
roughness = 0.8

# Sheen (fabric edge highlights)
[entities.material.sheen]
color = [0.5, 0.3, 0.6]             # array<float>[3] - Sheen color
roughness = 0.3                      # float (0-1) - Sheen roughness

# -----------------------------------------------------------------------------

[[entities]]
name = "brushed_metal"
mesh = "cylinder"
layer = "world"
visible = true

[entities.transform]
position = [0.0, 0.5, -2.0]
scale = [1.0, 0.5, 1.0]

# Anisotropic material (brushed metal, hair, CD surface)
[entities.material]
albedo = [0.8, 0.8, 0.85]
metallic = 1.0
roughness = 0.3

# Anisotropy (directional roughness)
[entities.material.anisotropy]
strength = 0.8                       # float (-1 to 1) - Anisotropy strength and direction
rotation = 0.0                       # float (0-1) - Rotation in UV space (0.5 = 90 degrees)
direction_map = "textures/aniso_direction.png"  # string (optional) - Direction texture

# -----------------------------------------------------------------------------

[[entities]]
name = "soap_bubble"
mesh = "sphere"
layer = "transparent"
visible = true

[entities.transform]
position = [2.0, 1.5, 2.0]
scale = 0.5

# Iridescent material (soap bubbles, beetle shells, oil slicks)
[entities.material]
albedo = [0.95, 0.95, 0.95]
metallic = 0.0
roughness = 0.1

[entities.material.transmission]
factor = 0.8
ior = 1.3

# Iridescence (thin-film interference)
[entities.material.iridescence]
factor = 1.0                         # float (0-1) - Iridescence strength
ior = 1.3                            # float - Thin film IOR
thickness_min = 100.0                # float (nm) - Minimum film thickness
thickness_max = 400.0                # float (nm) - Maximum film thickness
thickness_map = "textures/thickness.png"  # string (optional) - Thickness texture

# -----------------------------------------------------------------------------

# Alpha modes for transparency
[[entities]]
name = "foliage"
mesh = "plane"
layer = "transparent"
visible = true

[entities.transform]
position = [3.0, 1.0, 0.0]
scale = 1.0

[entities.material]
albedo = { texture = "textures/leaf_color.png" }
metallic = 0.0
roughness = 0.8

# Alpha/transparency settings
[entities.material.alpha]
mode = "mask"                        # string - "opaque", "mask", or "blend"
cutoff = 0.5                         # float (for mask mode) - Alpha threshold (0-1)
double_sided = true                  # boolean - Render both faces

# -----------------------------------------------------------------------------
# Alpha Modes:
# - "opaque" : No transparency (default)
# - "mask"   : Binary transparency using alpha cutoff
# - "blend"  : Full alpha blending (requires transparent layer)
#
# Material Property Ranges:
# - IOR: 1.0 (vacuum) to 2.4+ (diamond)
#   - Water: 1.33
#   - Glass: 1.5
#   - Crystal: 2.0
#   - Diamond: 2.4
# - Iridescence thickness: 100-1000 nm typical
# - Anisotropy: -1.0 (perpendicular) to 1.0 (along tangent)
# -----------------------------------------------------------------------------
```

---

## Animation System

The keyframe animation system supports clips, players, and various interpolation modes.

```toml
# =============================================================================
# ANIMATION CLIPS (Phase 8)
# =============================================================================
# Define reusable animation clips with keyframes

[[animation_clips]]
name = "walk_cycle"                  # string (required) - Unique clip identifier
duration = 1.0                       # float (required) - Clip duration in seconds
loop_mode = "repeat"                 # string (optional) - "once", "repeat", "ping_pong"

# Translation keyframes
[[animation_clips.channels]]
target = "character/hips"            # string (required) - Target entity path
property = "translation"             # string (required) - Property to animate

[[animation_clips.channels.keyframes]]
time = 0.0                           # float (required) - Time in seconds
value = [0.0, 0.0, 0.0]             # array<float>[3] - Translation XYZ
interpolation = "linear"             # string (optional) - Interpolation mode

[[animation_clips.channels.keyframes]]
time = 0.5
value = [0.0, 0.1, 0.5]
interpolation = "cubic_spline"

[[animation_clips.channels.keyframes]]
time = 1.0
value = [0.0, 0.0, 1.0]
interpolation = "linear"

# Rotation keyframes (quaternion)
[[animation_clips.channels]]
target = "character/spine"
property = "rotation"

[[animation_clips.channels.keyframes]]
time = 0.0
value = [0.0, 0.0, 0.0, 1.0]        # array<float>[4] - Quaternion XYZW

[[animation_clips.channels.keyframes]]
time = 0.5
value = [0.0, 0.707, 0.0, 0.707]

# Scale keyframes
[[animation_clips.channels]]
target = "character/chest"
property = "scale"

[[animation_clips.channels.keyframes]]
time = 0.0
value = [1.0, 1.0, 1.0]             # array<float>[3] - Scale XYZ

[[animation_clips.channels.keyframes]]
time = 0.5
value = [1.1, 0.9, 1.1]

# Morph target weights
[[animation_clips.channels]]
target = "character/face"
property = "weights"

[[animation_clips.channels.keyframes]]
time = 0.0
value = [0.0, 0.0, 1.0]             # array<float> - Morph target weights

[[animation_clips.channels.keyframes]]
time = 0.5
value = [1.0, 0.0, 0.0]

# -----------------------------------------------------------------------------
# Interpolation Modes:
# - "step"         : No interpolation, instant value change
# - "linear"       : Linear interpolation between keyframes
# - "cubic_spline" : Cubic spline with tangent control
#
# Loop Modes:
# - "once"      : Play once and stop
# - "repeat"    : Loop from start
# - "ping_pong" : Alternate forward/backward
#
# Animatable Properties:
# - "translation" : Position (Vec3)
# - "rotation"    : Rotation (Quaternion)
# - "scale"       : Scale (Vec3 or uniform float)
# - "weights"     : Morph target weights (array<float>)
# -----------------------------------------------------------------------------

# =============================================================================
# ANIMATION PLAYERS
# =============================================================================
# Attach animation players to entities

[[entities]]
name = "animated_character"
mesh = { gltf = "models/character.gltf" }
layer = "world"
visible = true

[entities.transform]
position = [0.0, 0.0, 0.0]
scale = 1.0

# Animation player component
[entities.animation_player]
clip = "walk_cycle"                  # string (required) - Animation clip to play
speed = 1.0                          # float (optional) - Playback speed multiplier
autoplay = true                      # boolean (optional) - Start playing immediately
paused = false                       # boolean (optional) - Initial pause state
time = 0.0                           # float (optional) - Starting time in seconds
```

---

## Animation Blending

Blend trees allow combining multiple animations for smooth transitions and layered animation.

```toml
# =============================================================================
# ANIMATION BLEND TREES (Phase 9)
# =============================================================================

[[blend_trees]]
name = "locomotion"                  # string (required) - Blend tree identifier
root = "movement_blend"              # string (required) - Root node name

# 1D blend node (blend by single parameter)
[[blend_trees.nodes]]
name = "movement_blend"
type = "blend_1d"                    # string (required) - Node type
parameter = "speed"                  # string (required) - Parameter to blend by

[[blend_trees.nodes.children]]
clip = "idle"
threshold = 0.0                      # float - Parameter value for this clip

[[blend_trees.nodes.children]]
clip = "walk"
threshold = 1.0

[[blend_trees.nodes.children]]
clip = "run"
threshold = 2.0

# 2D blend node (blend by two parameters)
[[blend_trees.nodes]]
name = "directional_blend"
type = "blend_2d"
parameter_x = "velocity_x"           # string - X-axis parameter
parameter_y = "velocity_z"           # string - Y-axis parameter

[[blend_trees.nodes.children]]
clip = "strafe_left"
position = [-1.0, 0.0]              # array<float>[2] - Position in blend space

[[blend_trees.nodes.children]]
clip = "forward"
position = [0.0, 1.0]

[[blend_trees.nodes.children]]
clip = "strafe_right"
position = [1.0, 0.0]

[[blend_trees.nodes.children]]
clip = "backward"
position = [0.0, -1.0]

# Additive blend node
[[blend_trees.nodes]]
name = "hit_reaction"
type = "additive"                    # Additive blending on top of base
base = "movement_blend"              # string - Base animation node
additive_clip = "hit_flinch"         # string - Clip to add
weight = "hit_weight"                # string or float - Blend weight parameter

# Layer blend node
[[blend_trees.nodes]]
name = "upper_body_override"
type = "layer"
base = "locomotion"
override_clip = "aim_rifle"
mask = "upper_body"                  # string - Bone mask name
weight = "aim_weight"

# -----------------------------------------------------------------------------
# Blend Node Types:
# - "blend_1d"  : Blend clips along a single parameter axis
# - "blend_2d"  : Blend clips in 2D parameter space
# - "additive"  : Add animation on top of base (for reactions, breathing)
# - "layer"     : Override specific bones (for upper/lower body split)
# - "select"    : Choose one clip based on condition
#
# Blend Modes (for layers):
# - "override"  : Replace base animation
# - "additive"  : Add to base animation
# - "multiply"  : Multiply with base (for scale animations)
# -----------------------------------------------------------------------------

# =============================================================================
# ANIMATION STATE MACHINES
# =============================================================================

[[animation_state_machines]]
name = "character_states"
initial_state = "idle"

[[animation_state_machines.states]]
name = "idle"
blend_tree = "locomotion"            # Reference to blend tree
speed_parameter = "speed"
default_speed = 0.0

[[animation_state_machines.states]]
name = "moving"
blend_tree = "locomotion"
speed_parameter = "speed"

[[animation_state_machines.states]]
name = "jumping"
clip = "jump"
loop = false

# Transitions between states
[[animation_state_machines.transitions]]
from = "idle"
to = "moving"
condition = "speed > 0.1"            # string - Condition expression
duration = 0.2                       # float - Transition duration in seconds
curve = "smooth_step"                # string (optional) - Transition curve

[[animation_state_machines.transitions]]
from = "moving"
to = "idle"
condition = "speed < 0.1"
duration = 0.3

[[animation_state_machines.transitions]]
from = "*"                           # Wildcard: any state
to = "jumping"
condition = "jump_pressed"
duration = 0.1
interrupt = true                     # boolean - Can interrupt current transition

# =============================================================================
# BONE MASKS
# =============================================================================

[[bone_masks]]
name = "upper_body"
include = [                          # array<string> - Bones to include
    "spine",
    "spine_01",
    "spine_02",
    "neck",
    "head",
    "clavicle_l",
    "clavicle_r",
    "upperarm_*",                    # Wildcard support
    "lowerarm_*",
    "hand_*"
]
weights = "gradient"                 # string - "uniform" or "gradient"
falloff = 0.2                        # float - Gradient falloff at boundaries
```

---

## Picking and Raycasting

Configure which entities can be picked and customize ray casting behavior.

```toml
# =============================================================================
# PICKING CONFIGURATION (Phase 10)
# =============================================================================

[picking]
enabled = true                       # boolean (optional) - Enable picking system
method = "gpu"                       # string (optional) - "cpu", "gpu", or "hybrid"
max_distance = 1000.0                # float (optional) - Maximum ray distance
layer_mask = ["world", "ui"]         # array<string> (optional) - Layers to pick from

# GPU picking settings
[picking.gpu]
buffer_size = [256, 256]            # array<int>[2] - Pick buffer resolution
readback_delay = 1                   # integer - Frames to wait for GPU readback

# =============================================================================
# PICKABLE ENTITIES
# =============================================================================

[[entities]]
name = "interactive_button"
mesh = "cube"
layer = "world"
visible = true

[entities.transform]
position = [0.0, 1.0, 0.0]
scale = [0.5, 0.2, 0.5]

[entities.material]
albedo = [0.2, 0.5, 0.8]
metallic = 0.0
roughness = 0.3

# Pickable component
[entities.pickable]
enabled = true                       # boolean (optional) - Can be picked
priority = 0                         # integer (optional) - Pick priority (higher wins)
bounds = "mesh"                      # string (optional) - "mesh", "aabb", "sphere", "custom"
interaction_distance = 5.0           # float (optional) - Maximum interaction distance
highlight_on_hover = true            # boolean (optional) - Visual feedback on hover

# Custom bounds (if bounds = "custom")
[entities.pickable.custom_bounds]
type = "sphere"                      # string - "aabb", "sphere", "capsule"
center = [0.0, 0.0, 0.0]            # array<float>[3] - Local center
radius = 0.5                         # float (for sphere)
# Or for AABB:
# min = [-0.5, -0.5, -0.5]
# max = [0.5, 0.5, 0.5]
```

---

## Entity Input Events

Configure event handlers for entity interactions (click, hover, drag).

```toml
# =============================================================================
# ENTITY INPUT EVENTS (Phase 11)
# =============================================================================

[[entities]]
name = "door"
mesh = { gltf = "models/door.gltf" }
layer = "world"
visible = true

[entities.transform]
position = [5.0, 0.0, 0.0]
scale = 1.0

[entities.pickable]
enabled = true

# Input event handlers
[entities.input_events]
# Click events
on_click = "door_toggle"             # string - Event name to emit
on_click_data = { door_id = 1 }      # table (optional) - Data to include with event

# Pointer events
on_pointer_enter = "door_highlight_on"
on_pointer_exit = "door_highlight_off"
on_pointer_down = "door_press"
on_pointer_up = "door_release"

# Drag events (for draggable objects)
[entities.input_events.drag]
enabled = false                      # boolean - Enable dragging
axis_constraint = "none"             # string - "none", "x", "y", "z", "xy", "xz", "yz"
plane_constraint = "none"            # string - "none", "xy", "xz", "yz"
on_drag_start = "begin_drag"
on_drag = "dragging"
on_drag_end = "end_drag"
snap_to_grid = false                 # boolean - Snap to grid while dragging
grid_size = 0.5                      # float - Grid snap size

# -----------------------------------------------------------------------------

[[entities]]
name = "slider_handle"
mesh = "sphere"
layer = "ui"
visible = true

[entities.transform]
position = [0.0, 0.0, 0.0]
scale = 0.2

[entities.pickable]
enabled = true

# Draggable slider
[entities.input_events]
on_click = "slider_click"

[entities.input_events.drag]
enabled = true
axis_constraint = "x"                # Only move along X axis
on_drag_start = "slider_drag_start"
on_drag = "slider_update"
on_drag_end = "slider_drag_end"

# -----------------------------------------------------------------------------
# Axis Constraints:
# - "none" : Free movement in 3D
# - "x", "y", "z" : Lock to single axis
# - "xy", "xz", "yz" : Lock to plane
#
# Event Data:
# Events include automatic data:
# - world_position: Hit point in world space
# - local_position: Hit point in entity local space
# - normal: Surface normal at hit point
# - distance: Distance from camera
# - button: Mouse button used
# -----------------------------------------------------------------------------
```

---

## Multi-Pass Rendering

Configure entities to render in multiple passes for special effects.

```toml
# =============================================================================
# MULTI-PASS ENTITIES (Phase 12)
# =============================================================================

[[entities]]
name = "outlined_object"
mesh = "cube"
layer = "world"
visible = true

[entities.transform]
position = [0.0, 1.0, 0.0]
scale = 1.0

[entities.material]
albedo = [0.3, 0.6, 0.9]
metallic = 0.0
roughness = 0.4

# Render in multiple passes
[entities.render_passes]
passes = ["shadow", "main", "outline"]  # array<string> - Pass names

# Per-pass overrides
[entities.render_passes.outline]
material_override = "outline_material"  # string - Different material for this pass
scale_offset = 1.02                     # float - Scale multiplier
cull_front = true                       # boolean - Cull front faces (for outline)

[entities.render_passes.shadow]
cast_only = true                        # boolean - Only cast shadow, don't receive

# -----------------------------------------------------------------------------

[[entities]]
name = "reflective_floor"
mesh = "plane"
layer = "world"
visible = true

[entities.transform]
position = [0.0, 0.0, 0.0]
scale = 10.0

[entities.material]
albedo = [0.5, 0.5, 0.5]
metallic = 0.8
roughness = 0.1

# Mirror/reflection pass
[entities.render_passes]
passes = ["reflection", "main"]

[entities.render_passes.reflection]
render_target = "reflection_texture"    # string - Render to texture
camera_flip = "y"                       # string - Flip camera axis for reflection
stencil_mask = 1                        # integer - Stencil value for masking
```

---

## Custom Render Passes

Define custom render passes for specialized effects in the scene file.

```toml
# =============================================================================
# CUSTOM RENDER PASSES (Phase 13)
# =============================================================================
# Define scene-specific render passes

[[render_passes]]
name = "outline_pass"
type = "custom"
priority = 50
enabled = true
shader = "shaders/outline.wgsl"      # string - Custom shader path

[render_passes.config]
resolution = "screen"                # string or array<int>[2] - "screen", "half", or [width, height]
format = "rgba8unorm"
clear_color = [0.0, 0.0, 0.0, 0.0]
entity_filter = "outlined"           # string - Entity tag filter

[render_passes.uniforms]
outline_color = [1.0, 0.5, 0.0, 1.0] # Custom uniform data
outline_width = 2.0

# -----------------------------------------------------------------------------

[[render_passes]]
name = "blur_pass"
type = "post"
priority = 100
enabled = true
shader = "shaders/gaussian_blur.wgsl"

[render_passes.config]
resolution = "half"                  # Half resolution for performance
format = "rgba16float"

[render_passes.inputs]
color = "main_color"                 # string - Input texture from previous pass
depth = "main_depth"

[render_passes.outputs]
blurred = "blur_result"              # string - Output texture name

[render_passes.uniforms]
blur_radius = 5.0
blur_sigma = 2.0
direction = [1.0, 0.0]               # Horizontal pass

# -----------------------------------------------------------------------------

[[render_passes]]
name = "bloom_combine"
type = "post"
priority = 110
enabled = true

[render_passes.dependencies]
requires = ["blur_pass"]             # Wait for blur to complete

[render_passes.inputs]
scene = "main_color"
bloom = "blur_result"

[render_passes.uniforms]
bloom_intensity = 0.3
bloom_threshold = 1.0
```

---

## Spatial Queries

Configure spatial indexing structures for efficient queries.

```toml
# =============================================================================
# SPATIAL QUERY CONFIGURATION (Phase 14)
# =============================================================================

[spatial]
structure = "bvh"                    # string - "bvh", "octree", or "grid"
auto_rebuild = true                  # boolean - Rebuild when entities move
rebuild_threshold = 0.3              # float - Rebuild when 30% of entities moved

# BVH-specific settings
[spatial.bvh]
max_leaf_size = 4                    # integer - Max primitives per leaf
build_quality = "medium"             # string - "fast", "medium", "high"
traversal_cost = 1.0                 # float - Cost ratio for SAH
intersection_cost = 1.0              # float - Cost ratio for SAH

# Octree-specific settings
[spatial.octree]
max_depth = 8                        # integer - Maximum tree depth
min_size = 1.0                       # float - Minimum node size
loose_factor = 1.5                   # float - Loose octree expansion (1.0 = tight)

# Grid-specific settings
[spatial.grid]
cell_size = [10.0, 10.0, 10.0]      # array<float>[3] - Cell dimensions
origin = [0.0, 0.0, 0.0]            # array<float>[3] - Grid origin

# Query settings
[spatial.queries]
frustum_culling = true               # boolean - Enable frustum culling
occlusion_culling = false            # boolean - Enable occlusion culling (experimental)
max_query_results = 1000             # integer - Limit query results
```

---

## LOD System

Configure level-of-detail groups for entities with multiple detail levels.

```toml
# =============================================================================
# LOD GROUPS (Phase 15)
# =============================================================================

[[entities]]
name = "tree_lod"
layer = "world"
visible = true

[entities.transform]
position = [10.0, 0.0, 5.0]
scale = 1.0

# LOD group configuration
[entities.lod]
mode = "distance"                    # string - "distance" or "screen_height"
transition = "cross_fade"            # string - "instant", "cross_fade", "dither", "morph"
fade_duration = 0.3                  # float - Transition duration in seconds
hysteresis = 0.1                     # float - Prevents LOD popping (0.0-1.0)

# LOD levels (ordered by distance/size)
[[entities.lod.levels]]
mesh = { gltf = "models/tree_lod0.gltf" }  # Highest detail
max_distance = 20.0                  # float - Maximum distance for this LOD
# OR for screen_height mode:
# min_screen_height = 0.3            # float - Minimum screen coverage (0-1)

[entities.lod.levels.material]
albedo = { texture = "textures/tree/bark_4k.png" }
normal_map = "textures/tree/bark_normal_4k.png"
roughness = 0.8

[[entities.lod.levels]]
mesh = { gltf = "models/tree_lod1.gltf" }  # Medium detail
max_distance = 50.0

[entities.lod.levels.material]
albedo = { texture = "textures/tree/bark_2k.png" }
normal_map = "textures/tree/bark_normal_2k.png"
roughness = 0.8

[[entities.lod.levels]]
mesh = { gltf = "models/tree_lod2.gltf" }  # Low detail
max_distance = 100.0

[entities.lod.levels.material]
albedo = { texture = "textures/tree/bark_1k.png" }
roughness = 0.8

[[entities.lod.levels]]
mesh = "billboard"                   # Lowest detail: billboard
max_distance = 500.0                 # Beyond this: culled

[entities.lod.levels.material]
albedo = { texture = "textures/tree/billboard.png" }

[entities.lod.levels.billboard]
type = "spherical"                   # string - "spherical", "cylindrical", "axial"
axis = [0.0, 1.0, 0.0]              # array<float>[3] - For cylindrical/axial

# -----------------------------------------------------------------------------
# LOD Transition Modes:
# - "instant"    : Immediate switch (may cause popping)
# - "cross_fade" : Alpha blend between LODs
# - "dither"     : Screen-door dithering pattern
# - "morph"      : Vertex morphing (requires compatible meshes)
#
# Mode Selection:
# - "distance"      : Based on camera distance
# - "screen_height" : Based on projected screen coverage
#
# Billboard Types:
# - "spherical"   : Always faces camera
# - "cylindrical" : Rotates only around specified axis
# - "axial"       : Aligned to specified axis
# -----------------------------------------------------------------------------
```

---

## Scene Streaming

Configure scene chunks for streaming large worlds.

```toml
# =============================================================================
# SCENE STREAMING / CHUNKS (Phase 16)
# =============================================================================

# Define streamable chunks
[[chunks]]
id = "chunk_0_0"                     # string (required) - Unique chunk identifier
file = "chunks/terrain_0_0.toml"     # string (required) - Chunk data file
priority = 0                         # integer (optional) - Load priority

[chunks.bounds]
min = [0.0, -10.0, 0.0]             # array<float>[3] - AABB minimum
max = [64.0, 50.0, 64.0]            # array<float>[3] - AABB maximum

[chunks.streaming]
preload = false                      # boolean - Load immediately at scene start
resident = false                     # boolean - Never unload this chunk
dependencies = []                    # array<string> - Chunks that must load first
load_distance = 128.0                # float (optional) - Override global load distance
unload_distance = 160.0              # float (optional) - Override global unload distance

# -----------------------------------------------------------------------------

[[chunks]]
id = "chunk_1_0"
file = "chunks/terrain_1_0.toml"
priority = 0

[chunks.bounds]
min = [64.0, -10.0, 0.0]
max = [128.0, 50.0, 64.0]

[chunks.streaming]
preload = false
dependencies = ["chunk_0_0"]         # Load after chunk_0_0

# -----------------------------------------------------------------------------

[[chunks]]
id = "spawn_area"
file = "chunks/spawn.toml"
priority = 100                       # High priority - load first

[chunks.bounds]
min = [-32.0, -10.0, -32.0]
max = [32.0, 50.0, 32.0]

[chunks.streaming]
preload = true                       # Load immediately
resident = true                      # Never unload

# =============================================================================
# CHUNK FILE FORMAT (chunks/terrain_0_0.toml)
# =============================================================================
# Each chunk file contains entities local to that chunk

# [chunk]
# id = "chunk_0_0"
# version = "1.0.0"
#
# [[entities]]
# name = "rock_001"
# mesh = { gltf = "models/rock.gltf" }
# layer = "world"
# visible = true
#
# [entities.transform]
# position = [10.0, 0.0, 15.0]      # Position relative to chunk
# scale = 2.0
#
# [entities.material]
# albedo = [0.5, 0.5, 0.5]
# roughness = 0.9
#
# ... more entities ...

# -----------------------------------------------------------------------------
# Chunk States:
# - Unloaded  : Not in memory
# - Loading   : Async load in progress
# - Loaded    : Ready and visible
# - Unloading : Being removed from memory
# - Failed    : Load error occurred
#
# Priority Guidelines:
# - Spawn/start areas: 100+
# - Story-critical: 50-99
# - Normal terrain: 0
# - Background/distant: -50 to -1
# -----------------------------------------------------------------------------
```

---

## Precision Management

Configure large world support with origin rebasing and high-precision coordinates.

```toml
# =============================================================================
# PRECISION MANAGEMENT (Phase 17)
# =============================================================================

# World origin configuration
[world_origin]
# Current origin offset in double precision (f64)
# This is the true world position of the scene's (0,0,0) point
offset = [1000000.0, 0.0, 5000000.0]  # array<f64>[3] - Origin in world coords

# Automatic rebasing
auto_rebase = true                   # boolean - Enable automatic origin rebasing
rebase_threshold = 5000.0            # float - Distance from origin before rebase
rebase_anchor = "camera"             # string - "camera", "player", or entity name

# =============================================================================
# HIGH-PRECISION ENTITIES
# =============================================================================
# Entities that need precise positioning in large worlds

[[entities]]
name = "gps_marker"
mesh = "sphere"
layer = "world"
visible = true

# Standard transform (relative to current origin)
[entities.transform]
position = [0.0, 100.0, 0.0]
scale = 5.0

# High-precision world position (f64)
[entities.precision_position]
# True world coordinates in double precision
world = [1000500.5, 100.0, 5000250.75]  # array<f64>[3]
# The local position is automatically computed from:
# local = world - world_origin.offset

# -----------------------------------------------------------------------------

[[entities]]
name = "space_station"
mesh = { gltf = "models/station.gltf" }
layer = "world"
visible = true

[entities.transform]
position = [0.0, 0.0, 0.0]          # Local position (updated by precision system)
scale = 1.0

[entities.precision_position]
# Position at 400km altitude
world = [0.0, 400000.0, 0.0]

# Precision rendering hints
[entities.precision_position.hints]
camera_relative = true               # boolean - Use camera-relative rendering
depth_offset = 0.0                   # float - Depth buffer offset
logarithmic_depth = true             # boolean - Use logarithmic depth buffer

# -----------------------------------------------------------------------------
# Large World Coordinate System:
#
# World coordinates use f64 (double precision) for positions:
# - Maximum safe integer: ~9 quadrillion (9e15)
# - Millimeter precision at: ~1e12 meters (1 billion km)
# - Micrometer precision at: ~1e9 meters (1 million km)
#
# For planetary scales:
# - Earth radius: ~6.4e6 meters
# - Moon distance: ~3.8e8 meters
# - Sun distance: ~1.5e11 meters
#
# Coordinate Conventions:
# - Y-up, right-handed by default
# - Geographic: X=East, Y=Up, Z=North
# - Can override in [app.precision.coordinate_system]
# -----------------------------------------------------------------------------
```

---

## Debug and Introspection

Configure debug visualization and performance monitoring.

```toml
# =============================================================================
# DEBUG CONFIGURATION (Phase 18)
# =============================================================================

[debug]
enabled = true                       # boolean - Master debug enable

# Stats overlay
[debug.stats]
enabled = true                       # boolean - Show stats overlay
position = "top_left"                # string - "top_left", "top_right", "bottom_left", "bottom_right"
font_size = 14                       # integer - Font size in pixels
background_alpha = 0.7               # float - Background opacity

# Which stats to display
[debug.stats.display]
fps = true                           # boolean - Frames per second
frame_time = true                    # boolean - Frame time in ms
draw_calls = true                    # boolean - Draw call count
triangles = true                     # boolean - Triangle count
entities_total = true                # boolean - Total entity count
entities_visible = true              # boolean - Visible entity count
gpu_memory = true                    # boolean - GPU memory usage
cpu_time = true                      # boolean - CPU frame time
gpu_time = false                     # boolean - GPU frame time (requires query)
texture_memory = true                # boolean - Texture memory usage
buffer_memory = true                 # boolean - Buffer memory usage

# Visualization flags
[debug.visualization]
enabled = true                       # boolean - Enable debug rendering

# Geometry visualization
bounds = false                       # boolean - Show axis-aligned bounding boxes
oriented_bounds = false              # boolean - Show oriented bounding boxes
wireframe = false                    # boolean - Wireframe overlay on all geometry
normals = false                      # boolean - Show vertex normals
tangents = false                     # boolean - Show tangent vectors
uv_checker = false                   # boolean - Replace textures with UV checker

# Lighting visualization
light_volumes = false                # boolean - Show light influence volumes
light_directions = false             # boolean - Show light direction vectors
shadow_cascades = false              # boolean - Color by shadow cascade
shadow_maps = false                  # boolean - Show shadow map textures

# Animation visualization
skeleton = false                     # boolean - Show bone hierarchy
bone_names = false                   # boolean - Show bone names
animation_curves = false             # boolean - Show animation parameter graphs

# Spatial structure visualization
bvh_nodes = false                    # boolean - Show BVH node boundaries
octree_cells = false                 # boolean - Show octree cells
chunk_boundaries = false             # boolean - Show streaming chunk bounds
lod_levels = false                   # boolean - Color entities by LOD level

# Physics visualization (if physics enabled)
collision_shapes = false             # boolean - Show collision geometry
contact_points = false               # boolean - Show contact points
velocity_vectors = false             # boolean - Show velocity arrows
force_vectors = false                # boolean - Show force arrows

# Picking visualization
pick_ray = false                     # boolean - Show pick ray
hit_points = false                   # boolean - Show hit locations

# Appearance settings
[debug.visualization.appearance]
bounds_color = [0.0, 1.0, 0.0, 0.5]  # array<float>[4] - RGBA
wireframe_color = [1.0, 1.0, 1.0, 0.3]
normal_color = [0.0, 0.5, 1.0, 1.0]
tangent_color = [1.0, 0.5, 0.0, 1.0]
bone_color = [1.0, 1.0, 0.0, 1.0]
velocity_color = [0.0, 1.0, 1.0, 1.0]

normal_length = 0.1                  # float - Length of normal vectors
bone_size = 0.05                     # float - Bone visualization size
line_width = 1.0                     # float - Debug line width
point_size = 5.0                     # float - Debug point size

# LOD level colors (index = LOD level)
lod_colors = [
    [0.0, 1.0, 0.0, 0.3],           # LOD 0 - Green (highest detail)
    [1.0, 1.0, 0.0, 0.3],           # LOD 1 - Yellow
    [1.0, 0.5, 0.0, 0.3],           # LOD 2 - Orange
    [1.0, 0.0, 0.0, 0.3],           # LOD 3 - Red (lowest detail)
]

# Debug controls
[debug.controls]
toggle_key = "F3"                    # string - Key to toggle debug overlay
cycle_mode_key = "F4"                # string - Key to cycle visualization modes
reload_shaders_key = "F5"            # string - Key to hot-reload shaders
pause_key = "Pause"                  # string - Key to pause simulation
step_key = "F10"                     # string - Key to step one frame (when paused)

# Performance profiling
[debug.profiling]
enabled = false                      # boolean - Enable CPU profiling
gpu_profiling = false                # boolean - Enable GPU profiling
trace_file = "profile.json"          # string - Output file for trace
frame_capture_key = "F11"            # string - Key to capture frame trace

# -----------------------------------------------------------------------------
# Debug Visualization Quick Reference:
#
# Geometry:
#   bounds, wireframe, normals, tangents - Mesh inspection
#
# Lighting:
#   light_volumes, shadow_cascades - Light debugging
#
# Animation:
#   skeleton, bone_names - Rig inspection
#
# Spatial:
#   bvh_nodes, octree_cells - Spatial structure
#   chunk_boundaries - Streaming visualization
#
# Performance:
#   lod_levels - LOD verification
#   Stats overlay - Frame metrics
# -----------------------------------------------------------------------------
```

---

## Shader Files (.wgsl)

Shaders use WGSL (WebGPU Shading Language) with standardized bind groups.

### Bind Group Convention

| Group | Purpose | Contents |
|-------|---------|----------|
| 0 | Camera | view_proj, view, projection, camera_pos |
| 1 | Model | model matrix, normal_matrix, instance data |
| 2 | Material | uniforms + textures (albedo, normal, roughness, etc.) |
| 3 | Lighting | lights array, shadow maps, environment |
| 4+ | Custom | Application-specific data |

### Camera Uniforms (Group 0)

```wgsl
struct CameraUniforms {
    view_proj: mat4x4<f32>,      // Pre-multiplied view * projection
    view: mat4x4<f32>,           // View matrix only
    projection: mat4x4<f32>,     // Projection matrix only
    camera_pos: vec3<f32>,       // Camera world position
    _padding: f32,               // Alignment padding
    // Large world support (Phase 17)
    camera_pos_high: vec3<f32>,  // High bits of camera position
    camera_pos_low: vec3<f32>,   // Low bits of camera position
}

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
```

### Model Uniforms (Group 1)

```wgsl
struct ModelUniforms {
    model: mat4x4<f32>,          // Object-to-world transform
    normal_matrix: mat4x4<f32>,  // Inverse-transpose for normals
    // Instance data (Phase 4)
    instance_offset: u32,        // Offset into instance buffer
    instance_count: u32,         // Number of instances
}

@group(1) @binding(0) var<uniform> model: ModelUniforms;

// Instance buffer (for instanced rendering)
struct InstanceData {
    model: mat4x4<f32>,
    color: vec4<f32>,
}

@group(1) @binding(1) var<storage, read> instances: array<InstanceData>;
```

### Material Uniforms (Group 2)

```wgsl
// Texture presence flags (bitwise)
const HAS_ALBEDO_TEX: u32 = 1u;
const HAS_NORMAL_TEX: u32 = 2u;
const HAS_ROUGHNESS_TEX: u32 = 4u;
const HAS_METALLIC_TEX: u32 = 8u;
const HAS_AO_TEX: u32 = 16u;
const HAS_EMISSIVE_TEX: u32 = 32u;
const HAS_CLEARCOAT_TEX: u32 = 64u;
const HAS_TRANSMISSION_TEX: u32 = 128u;

struct MaterialUniforms {
    base_color: vec4<f32>,       // RGBA, alpha = transparency
    metallic: f32,               // 0.0-1.0
    roughness: f32,              // 0.0-1.0
    emissive: vec3<f32>,         // Emissive RGB
    normal_scale: f32,           // Normal map intensity
    ao_strength: f32,            // AO strength
    texture_flags: u32,          // Bitflags for texture presence

    // Advanced material properties (Phase 7)
    clearcoat: f32,              // Clearcoat intensity
    clearcoat_roughness: f32,    // Clearcoat roughness
    transmission: f32,           // Transmission factor
    ior: f32,                    // Index of refraction
    thickness: f32,              // Material thickness
    subsurface: f32,             // Subsurface scattering factor
    subsurface_radius: vec3<f32>, // SSS radius per channel
    sheen_color: vec3<f32>,      // Sheen color
    sheen_roughness: f32,        // Sheen roughness
    anisotropy: f32,             // Anisotropy strength
    anisotropy_rotation: f32,    // Anisotropy rotation
    iridescence: f32,            // Iridescence factor
    iridescence_ior: f32,        // Thin film IOR
    iridescence_thickness: vec2<f32>, // Min/max film thickness

    // Alpha (Phase 7)
    alpha_mode: u32,             // 0=opaque, 1=mask, 2=blend
    alpha_cutoff: f32,           // Mask threshold
}

@group(2) @binding(0) var<uniform> material: MaterialUniforms;
@group(2) @binding(1) var t_albedo: texture_2d<f32>;
@group(2) @binding(2) var s_albedo: sampler;
@group(2) @binding(3) var t_normal: texture_2d<f32>;
@group(2) @binding(4) var s_normal: sampler;
@group(2) @binding(5) var t_roughness: texture_2d<f32>;
@group(2) @binding(6) var s_roughness: sampler;
@group(2) @binding(7) var t_metallic: texture_2d<f32>;
@group(2) @binding(8) var s_metallic: sampler;
@group(2) @binding(9) var t_ao: texture_2d<f32>;
@group(2) @binding(10) var s_ao: sampler;
@group(2) @binding(11) var t_emissive: texture_2d<f32>;
@group(2) @binding(12) var s_emissive: sampler;
```

### Light Uniforms (Group 3)

```wgsl
const MAX_LIGHTS: u32 = 128u;

const LIGHT_TYPE_DIRECTIONAL: u32 = 0u;
const LIGHT_TYPE_POINT: u32 = 1u;
const LIGHT_TYPE_SPOT: u32 = 2u;
const LIGHT_TYPE_AREA: u32 = 3u;

struct Light {
    light_type: u32,             // Light type enum
    _pad1: u32,
    _pad2: u32,
    cast_shadows: u32,           // Boolean as u32

    position: vec3<f32>,         // World position (point/spot/area)
    range: f32,                  // Maximum influence radius

    direction: vec3<f32>,        // Light direction (directional/spot/area)
    intensity: f32,              // Light intensity

    color: vec3<f32>,            // Light color RGB
    inner_angle: f32,            // Spot inner cone (cos)

    outer_angle: f32,            // Spot outer cone (cos)
    attenuation_constant: f32,
    attenuation_linear: f32,
    attenuation_quadratic: f32,

    // Area light properties
    area_size: vec2<f32>,        // Width/height or radius
    area_two_sided: u32,         // Boolean
    _pad3: u32,
}

struct LightingUniforms {
    light_count: u32,
    ambient_color: vec3<f32>,
    ambient_intensity: f32,
    environment_intensity: f32,
}

@group(3) @binding(0) var<uniform> lighting: LightingUniforms;
@group(3) @binding(1) var<storage, read> lights: array<Light>;
@group(3) @binding(2) var t_shadow_atlas: texture_depth_2d;
@group(3) @binding(3) var s_shadow: sampler_comparison;
@group(3) @binding(4) var t_environment: texture_cube<f32>;
@group(3) @binding(5) var s_environment: sampler;
@group(3) @binding(6) var t_irradiance: texture_cube<f32>;
@group(3) @binding(7) var t_prefilter: texture_cube<f32>;
@group(3) @binding(8) var t_brdf_lut: texture_2d<f32>;

// Shadow cascade matrices (Phase 6)
@group(3) @binding(9) var<uniform> shadow_cascades: array<mat4x4<f32>, 4>;
@group(3) @binding(10) var<uniform> cascade_splits: vec4<f32>;
```

---

## Asset Organization

### PBR Material Texture Sets

Each PBR material should be organized in a folder with these files:

```
textures/MaterialName/
├── MaterialName_Color.png           # Albedo/base color (sRGB)
├── MaterialName_NormalGL.png        # Normal map - OpenGL convention (Linear)
├── MaterialName_Roughness.png       # Roughness map (Linear, grayscale)
├── MaterialName_Metalness.png       # Metallic map (Linear, grayscale)
├── MaterialName_Displacement.png    # Height map (Linear, optional)
├── MaterialName_AmbientOcclusion.png # AO map (Linear, optional)
├── MaterialName_Emissive.png        # Emissive map (sRGB, optional)
└── MaterialName_Transmission.png    # Transmission map (Linear, optional)
```

### Color Space Requirements

| Texture Type | Color Space | srgb Flag |
|--------------|-------------|-----------|
| Albedo/Color | sRGB | `true` |
| Emissive | sRGB | `true` |
| Normal Map | Linear | `false` |
| Roughness | Linear | `false` |
| Metallic | Linear | `false` |
| AO | Linear | `false` |
| Displacement | Linear | `false` |
| Transmission | Linear | `false` |
| Thickness | Linear | `false` |
| HDR Environment | Linear | N/A (use `hdr = true`) |

### Normal Map Convention

**Always use OpenGL convention (GL) normal maps**, where:
- Red channel = X (right is positive)
- Green channel = Y (up is positive)
- Blue channel = Z (out is positive)

DirectX convention maps need Y channel inversion before use.

### Environment Maps

HDR environment maps should be in Radiance HDR format (`.hdr`):

```
textures/env/
└── environment_name.hdr             # Radiance HDR format
```

### Model Files

glTF models (Phase 3) should be organized by type:

```
models/
├── characters/
│   ├── player.gltf
│   └── npc.glb
├── props/
│   ├── crate.gltf
│   └── barrel.glb
└── environment/
    ├── tree.gltf
    └── rock.glb
```

---

## Complete Schema Reference

### Type Definitions

```typescript
// Core types
type Vec2 = [number, number];
type Vec3 = [number, number, number];
type Vec4 = [number, number, number, number];
type Vec3_f64 = [number, number, number];  // Double precision
type Range = [number, number];  // [min, max]
type Quaternion = [number, number, number, number];  // XYZW

// Enumerations
type AppType = "tool" | "demo";
type LayerType = "content" | "overlay" | "effect" | "portal" | "shadow";
type BlendMode = "replace" | "normal" | "additive" | "multiply";
type MeshType = "sphere" | "cube" | "torus" | "plane" | "cylinder" | "diamond" | "billboard";
type MeshRef = MeshType | { gltf: string };
type AnimationType = "rotate" | "oscillate" | "orbit" | "path" | "pulse";
type MouseButton = "left" | "right" | "middle";
type Interpolation = "linear" | "cubic" | "catmull";
type Easing = "linear" | "smooth_step" | "ease_in" | "ease_out";

// Camera types
type CameraType = "perspective" | "orthographic";

// Light types
type LightType = "directional" | "point" | "spot" | "area";
type AreaLightShape = "rectangle" | "disc";

// Shadow types
type ShadowFilterMethod = "none" | "pcf" | "pcss" | "vsm";
type CascadeSplitScheme = "uniform" | "logarithmic" | "practical";

// Material types
type AlphaMode = "opaque" | "mask" | "blend";
type MaterialValue<T> = T | { texture: string };

// Animation types
type AnimationInterpolation = "step" | "linear" | "cubic_spline";
type LoopMode = "once" | "repeat" | "ping_pong";
type BlendNodeType = "blend_1d" | "blend_2d" | "additive" | "layer" | "select";

// LOD types
type LodMode = "distance" | "screen_height";
type LodTransition = "instant" | "cross_fade" | "dither" | "morph";
type BillboardType = "spherical" | "cylindrical" | "axial";

// Spatial types
type SpatialStructure = "bvh" | "octree" | "grid";

// Render pass types
type RenderPassType = "shadow" | "depth" | "gbuffer" | "lighting" | "forward" | "transparent" | "post" | "custom";

// Debug types
type StatsPosition = "top_left" | "top_right" | "bottom_left" | "bottom_right";
type PickingMethod = "cpu" | "gpu" | "hybrid";
type BoundsType = "mesh" | "aabb" | "sphere" | "custom";
type CustomBoundsType = "aabb" | "sphere" | "capsule";
type AxisConstraint = "none" | "x" | "y" | "z" | "xy" | "xz" | "yz";

// Interfaces
interface Package {
    name: string;
    display_name: string;
    version: string;
    description: string;
    author: string;
    license: string;
    keywords?: string[];
    categories?: string[];
}

interface Layer {
    name: string;
    type: LayerType;
    priority: number;
    blend: BlendMode;
}

interface RenderPass {
    name: string;
    type: RenderPassType;
    priority: number;
    enabled?: boolean;
    shader?: string;
    config?: RenderPassConfig;
    dependencies?: RenderPassDependencies;
    inputs?: Record<string, string>;
    outputs?: Record<string, string>;
    uniforms?: Record<string, any>;
}

interface Resources {
    max_entities: number;
    max_memory: number;
    max_layers: number;
    max_cpu_ms: number;
    max_draw_calls?: number;
    max_triangles?: number;
    max_lights?: number;
    max_shadow_casters?: number;
}

interface Transform {
    position: Vec3;
    scale: number | Vec3;
    rotation?: Vec3 | Quaternion;
}

interface Camera {
    name: string;
    active: boolean;
    type: CameraType;
    transform: CameraTransform;
    perspective?: PerspectiveSettings;
    orthographic?: OrthographicSettings;
}

interface Light {
    name: string;
    type: LightType;
    enabled?: boolean;
    directional?: DirectionalLightSettings;
    point?: PointLightSettings;
    spot?: SpotLightSettings;
    area?: AreaLightSettings;
}

interface Material {
    albedo: Vec3 | { texture: string };
    normal_map?: string;
    metallic: number | { texture: string };
    roughness: number | { texture: string };
    emissive?: Vec3;
    clearcoat?: ClearcoatSettings;
    transmission?: TransmissionSettings;
    subsurface?: SubsurfaceSettings;
    sheen?: SheenSettings;
    anisotropy?: AnisotropySettings;
    iridescence?: IridescenceSettings;
    alpha?: AlphaSettings;
}

interface LodGroup {
    mode?: LodMode;
    transition?: LodTransition;
    fade_duration?: number;
    hysteresis?: number;
    levels: LodLevel[];
}

interface Chunk {
    id: string;
    file: string;
    priority?: number;
    bounds: { min: Vec3; max: Vec3 };
    streaming?: ChunkStreamingSettings;
}

interface Entity {
    name: string;
    parent?: string;  // Scene graph hierarchy
    mesh: MeshRef;
    layer: string;
    visible: boolean;
    transform: Transform;
    material?: Material;
    animation?: Animation;
    animation_player?: AnimationPlayer;
    lod?: LodGroup;
    pickable?: PickableSettings;
    input_events?: InputEventSettings;
    render_passes?: EntityRenderPasses;
    precision_position?: PrecisionPosition;
    instance_data?: InstanceData[];  // For instanced rendering
}
```

---

## Editor Generation Requirements

An editor generating valid Void GUI configurations must support:

### 1. Package Configuration
- [ ] Package metadata fields (name, version, author, etc.)
- [ ] App type selection (tool vs demo)
- [ ] Layer creation with type, priority, and blend mode
- [ ] Custom render pass definitions
- [ ] Resource limit configuration
- [ ] Platform feature selection

### 2. Scene Environment
- [ ] World origin configuration (large world support)
- [ ] Directional light settings (direction, color, intensity)
- [ ] Ambient light configuration
- [ ] Sky gradient colors (zenith, horizon, ground)
- [ ] Sun appearance (size, intensity, falloff)
- [ ] HDR environment map selection
- [ ] Fog density control

### 3. Camera System
- [ ] Multiple camera support
- [ ] Active camera selection
- [ ] Perspective camera settings (FOV, near/far, aspect)
- [ ] Orthographic camera settings (bounds, near/far)
- [ ] Camera transform (position, target, up)

### 4. Input Configuration
- [ ] Camera control binding (orbit, pan, zoom buttons)
- [ ] Sensitivity settings for each control
- [ ] Camera distance limits
- [ ] Axis inversion options
- [ ] Custom key binding editor
- [ ] Modifier key support (Shift, Ctrl, Alt)
- [ ] Debug toggle bindings

### 5. Lighting System
- [ ] Directional light creation and configuration
- [ ] Point light with position, color, intensity, range, attenuation
- [ ] Spot light with cone angles
- [ ] Area light with shape (rectangle/disc)
- [ ] Shadow casting toggle per light
- [ ] Light enable/disable toggle

### 6. Shadow Mapping
- [ ] Shadow atlas size configuration
- [ ] Cascade count and split scheme
- [ ] Per-cascade resolution and bias
- [ ] Filtering method selection (PCF, PCSS, VSM)
- [ ] PCF sample count and radius
- [ ] Shadow distance limits

### 7. Entity System
- [ ] Entity creation with unique names
- [ ] Parent/child hierarchy (scene graph)
- [ ] Mesh type selection (primitives or glTF)
- [ ] Layer assignment (must reference valid layer)
- [ ] Visibility toggle
- [ ] Transform editor (position, scale, rotation)
- [ ] Multiple instances per entity

### 8. Material System
- [ ] Albedo: color picker OR texture file selection
- [ ] Normal map: file selection (GL convention)
- [ ] Metallic: slider (0-1) OR texture file
- [ ] Roughness: slider (0-1) OR texture file
- [ ] Emissive: color picker
- [ ] **Clearcoat**: intensity, roughness, normal map
- [ ] **Transmission**: factor, IOR, thickness, attenuation
- [ ] **Subsurface**: factor, radius per channel, color
- [ ] **Sheen**: color, roughness
- [ ] **Anisotropy**: strength, rotation, direction map
- [ ] **Iridescence**: factor, IOR, thickness range
- [ ] **Alpha**: mode (opaque/mask/blend), cutoff, double-sided
- [ ] Texture preview and validation

### 9. Animation System
- [ ] Animation clip creation with keyframes
- [ ] Animation type dropdown (rotate, oscillate, orbit, path, pulse)
- [ ] Keyframe editor with time and value
- [ ] Interpolation mode selection (step, linear, cubic_spline)
- [ ] Loop mode selection (once, repeat, ping_pong)
- [ ] Animation player assignment
- [ ] Blend tree editor
- [ ] Animation state machine editor
- [ ] Bone mask definition

### 10. Picking and Input Events
- [ ] Pickable component configuration
- [ ] Bounds type selection
- [ ] Custom bounds editor
- [ ] Click event handler assignment
- [ ] Pointer event handlers (enter, exit, down, up)
- [ ] Drag configuration (axis constraints, grid snap)
- [ ] Drag event handlers

### 11. Multi-Pass Rendering
- [ ] Pass list assignment per entity
- [ ] Per-pass material overrides
- [ ] Per-pass transform modifiers
- [ ] Render target assignment

### 12. LOD System
- [ ] LOD group creation per entity
- [ ] LOD mode selection (distance/screen_height)
- [ ] Transition mode selection
- [ ] Per-level mesh and material assignment
- [ ] Distance/screen height thresholds
- [ ] Billboard configuration for lowest LOD

### 13. Scene Streaming
- [ ] Chunk definition with bounds
- [ ] Chunk file path assignment
- [ ] Load/unload distance configuration
- [ ] Priority and dependency settings
- [ ] Preload and resident flags

### 14. Precision Management
- [ ] World origin offset (f64)
- [ ] Auto-rebase configuration
- [ ] Per-entity precision position
- [ ] Camera-relative rendering hints

### 15. Debug Configuration
- [ ] Stats overlay toggle and position
- [ ] Visualization flag toggles
- [ ] Appearance customization
- [ ] Debug key binding

### 16. Particle System
- [ ] Emitter position (3D)
- [ ] Emission rate and max particles
- [ ] Lifetime range (min/max)
- [ ] Speed range (min/max)
- [ ] Size range (min/max)
- [ ] Color gradient (start RGBA to end RGBA)
- [ ] Gravity vector
- [ ] Spread and direction
- [ ] Enable/disable toggle

### 17. Texture Management
- [ ] Texture import with file browser
- [ ] Color space selection (sRGB vs Linear)
- [ ] HDR format detection
- [ ] Mipmap generation option
- [ ] Filter mode selection
- [ ] Texture preview
- [ ] Automatic texture set detection (PBR folder)

### 18. Validation
- [ ] Layer reference validation (entities must reference existing layers)
- [ ] Parent reference validation (valid entity names)
- [ ] Camera existence validation (at least one active camera)
- [ ] Light validation (reasonable intensity values)
- [ ] Shadow cascade validation (increasing distances)
- [ ] LOD level validation (increasing distances)
- [ ] Chunk bounds validation (non-overlapping or intentional)
- [ ] Texture path existence check
- [ ] Normal map convention detection
- [ ] Color space correctness
- [ ] Required field validation
- [ ] Semantic version format
- [ ] Resource limit sanity checks

### 19. Output Generation
- [ ] Valid TOML serialization
- [ ] Proper inline table syntax for material textures
- [ ] Array-of-tables syntax for entities, layers, textures, lights, cameras
- [ ] Comment preservation (optional)
- [ ] File path normalization (forward slashes)
- [ ] Chunk file generation

---

## Output File Relationships

```
manifest.toml
    │
    ├── [app]
    │   ├── scene = "scene.toml"  ────────────────► scene.toml
    │   ├── layers[].name  ◄─────────────────────┐
    │   └── render_passes[].name  ◄──────────────│───┐
    │                                             │   │
scene.toml                                        │   │
    │                                             │   │
    ├── entities[].layer  ────────────────────────┘   │
    ├── entities[].render_passes.passes  ─────────────┘
    ├── entities[].parent  ───────────────────────────► entities[].name (scene graph)
    │
    ├── entities[].mesh.gltf  ─────────────────────► models/**/*.gltf
    ├── entities[].material.albedo.texture  ───────► textures/*/Color.png
    ├── entities[].material.normal_map  ───────────► textures/*/NormalGL.png
    ├── entities[].material.metallic.texture  ─────► textures/*/Metalness.png
    ├── entities[].material.roughness.texture  ────► textures/*/Roughness.png
    │
    ├── animation_clips[].name  ◄──────────────────┐
    ├── entities[].animation_player.clip  ─────────┘
    │
    ├── blend_trees[].name  ◄──────────────────────┐
    ├── animation_state_machines[].states[].blend_tree  ───┘
    │
    ├── environment.environment_map  ──────────────► textures/env/*.hdr
    │
    ├── chunks[].file  ────────────────────────────► chunks/*.toml
    │
    └── textures[].path  ──────────────────────────► textures/**/*.(png|hdr)
```

---

## Quick Reference Card

### Layer Priority Scale
```
-200 ──── Shadow Maps
-100 ──── Sky/Background
 -50 ──── Nebula/Clouds
 -10 ──── Ground/Grid
   0 ──── World Content (default)
  20 ──── Transparent Objects
  50 ──── Particles/Effects
 100 ──── Post-processing/Bloom
 200 ──── UI/Overlay
 300 ──── Debug Visualization
```

### Common Memory Limits
```
 64 MB =  67108864
128 MB = 134217728
256 MB = 268435456
512 MB = 536870912
  1 GB = 1073741824
```

### Animation Speed Reference
```
Slow rotation:   0.1-0.3 rad/s
Normal rotation: 0.5-1.0 rad/s
Fast rotation:   1.5-3.0 rad/s

Slow bob:   0.2-0.5 Hz
Normal bob: 0.5-1.0 Hz
Fast bob:   1.5-2.0 Hz
```

### Material Value Ranges
```
Metallic:  0.0 (dielectric) to 1.0 (metal)
Roughness: 0.0 (mirror) to 1.0 (diffuse)
Emissive:  0.0-0.5 (subtle glow) to 1.0+ (bright)

IOR Values:
- Air:     1.0
- Water:   1.33
- Glass:   1.5
- Crystal: 2.0
- Diamond: 2.4

Clearcoat: 0.0-1.0 (typical car paint: 0.8-1.0)
Transmission: 0.0 (opaque) to 1.0 (fully transparent)
Subsurface: 0.0-1.0 (skin typically 0.3-0.5)
Sheen: 0.0-1.0 (velvet typically 0.8-1.0)
Anisotropy: -1.0 to 1.0 (brushed metal typically 0.5-0.9)
Iridescence: 0.0-1.0 (soap bubble typically 0.8-1.0)
```

### Light Intensity Reference
```
Directional (lux):
- Moonlight:      0.25
- Overcast day:   1,000-2,000
- Cloudy day:     10,000-25,000
- Direct sun:     50,000-100,000

Point/Spot (lumens):
- Candle:         12
- 40W bulb:       450
- 60W bulb:       800
- 100W bulb:      1,600
- LED panel:      2,000-5,000
```

### Shadow Cascade Split Distances
```
Near camera (high detail):    0-10m
Medium distance:              10-25m
Far distance:                 25-50m
Very far (low detail):        50-100m+
```

### LOD Distance Guidelines
```
LOD 0 (highest): 0-20m
LOD 1:           20-50m
LOD 2:           50-100m
LOD 3:           100-200m
Billboard:       200m+
```

---

*Document version: 2.0.0*
*Compatible with Void GUI v2.0.0+*
*Covers all 18 renderer phases*
