# Package Manifest (manifest.toml)

The manifest file defines app metadata, configuration, and resource limits.

## Package Section (Required)

```toml
[package]
name = "my-app"              # Slug format, required
display_name = "My App"      # Human-readable name
version = "1.0.0"            # Semantic versioning
description = "Description"
author = "Your Name"
license = "MIT"
keywords = ["demo", "3d"]
categories = ["demo"]        # demo, tool, game
```

## App Section (Required)

```toml
[app]
app_type = "demo"            # tool, demo, game
scene = "scene.toml"         # Path to scene file
entry = "scripts/main.vs"    # Entry script (optional)
```

## Render Layers

Define compositing layers for multi-pass rendering.

```toml
[[app.layers]]
name = "shadow"
type = "shadow"              # shadow, content, overlay, effect, portal
priority = 0                 # Lower = render first
blend = "replace"            # replace, normal, additive, multiply

[[app.layers]]
name = "world"
type = "content"
priority = 10
blend = "normal"

[[app.layers]]
name = "ui"
type = "overlay"
priority = 100
blend = "normal"
```

## Render Passes

Custom render pass configuration for advanced effects.

```toml
[[app.render_passes]]
name = "shadow_pass"
type = "shadow"
priority = 0
enabled = true

[app.render_passes.config]
resolution = [2048, 2048]
format = "depth32float"
clear_depth = 1.0
entity_filter = "shadow_casters"

[app.render_passes.dependencies]
requires = []
provides = ["shadow_map"]
```

## LOD System

```toml
[app.lod]
enabled = true
mode = "distance"            # distance, screen_coverage
default_mode = "instant"     # instant, cross_fade
cross_fade_duration = 0.2
dither_pattern = "bayer4"    # bayer4, bayer8, bayer16
hysteresis = 0.1
update_frequency = 30.0      # Hz
```

## Scene Streaming

For large worlds with chunked loading.

```toml
[app.streaming]
enabled = true
chunk_size = 100.0           # Meters
load_distance = 500.0
unload_distance = 600.0
max_concurrent_loads = 4
max_memory_mb = 512
preload_neighbors = true
priority_bias = "camera_direction"  # camera_direction, distance

[app.streaming.recycling]
enabled = true
pool_size = 1000
cooldown_frames = 60
```

## Precision Management

For large-scale worlds.

```toml
[app.precision]
enabled = true
origin_rebase_threshold = 5000.0  # Meters
camera_relative_rendering = true
double_precision_physics = true
coordinate_system = "right_handed"  # right_handed, left_handed
```

## Permissions

```toml
[app.permissions]
scripts = true               # Allow script execution
```

## Resource Limits

```toml
[app.resources]
max_entities = 10000
max_memory = 536870912       # Bytes (512MB)
max_layers = 16
max_cpu_ms = 16.0            # Per frame
max_draw_calls = 1000
max_triangles = 10000000
max_lights = 128
max_shadow_casters = 16
```

## Assets

```toml
[assets]
include = ["assets/", "shared/"]
compression = 6              # 0-9
```

## Platform Requirements

```toml
[platform]
min_version = "0.1.0"
required_features = ["hot-reload", "pbr"]
```
