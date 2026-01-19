# Package Manifest Format

The manifest file (`manifest.toml`) defines application metadata, configuration, resource limits, and render pipeline settings.

## File Format

- **Extension**: `.toml`
- **Encoding**: UTF-8
- **Location**: Application root directory

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

### Name Requirements

- Lowercase alphanumeric with hyphens
- Must start with letter
- Maximum 64 characters
- Must be unique within application registry

## App Section (Required)

```toml
[app]
app_type = "demo"            # tool, demo, game
scene = "scene.toml"         # Path to initial scene file
entry = "scripts/main.cpp"   # Entry script (optional)
```

### App Types

| Type | Description | Resource Defaults |
|------|-------------|-------------------|
| `tool` | Development/utility tool | Minimal resources |
| `demo` | Demonstration/showcase | Standard resources |
| `game` | Full game application | Maximum resources |

## Render Layers

Define compositing layers for multi-pass rendering:

```toml
[[app.layers]]
name = "shadow"
type = "shadow"              # Layer type
priority = 0                 # Lower = render first
blend = "replace"            # Blend mode

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

### Layer Types

| Type | Description | Use Case |
|------|-------------|----------|
| `shadow` | Shadow map generation | Shadow casting |
| `content` | Main 3D content | Scene objects |
| `overlay` | UI/HUD elements | 2D interface |
| `effect` | Post-processing | Bloom, DOF, color grading |
| `portal` | Render-to-texture | Mirrors, portals, security cameras |

### Blend Modes

| Mode | Description |
|------|-------------|
| `replace` | Overwrite previous layer |
| `normal` | Standard alpha blending |
| `additive` | Add colors together (glow effects) |
| `multiply` | Darken by multiplying (shadows) |
| `screen` | Lighten using screen blend |

### Layer Priority

Lower priority renders first, higher renders on top:

```toml
# Render order: shadow (0) → world (10) → effects (50) → ui (100)
[[app.layers]]
name = "shadow"
priority = 0

[[app.layers]]
name = "world"
priority = 10

[[app.layers]]
name = "effects"
priority = 50

[[app.layers]]
name = "ui"
priority = 100
```

## Render Passes

Custom render pass configuration for advanced effects:

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

### Pass Types

| Type | Description |
|------|-------------|
| `shadow` | Shadow map generation |
| `gbuffer` | Deferred geometry buffer |
| `lighting` | Lighting calculations |
| `forward` | Forward rendering pass |
| `post` | Post-processing effects |
| `composite` | Final compositing |

## LOD System

Level of Detail configuration:

```toml
[app.lod]
enabled = true
mode = "distance"            # distance, screen_coverage
default_mode = "instant"     # instant, cross_fade
cross_fade_duration = 0.2    # Seconds
dither_pattern = "bayer4"    # bayer4, bayer8, bayer16
hysteresis = 0.1             # Prevents LOD popping
update_frequency = 30.0      # Updates per second
```

### LOD Modes

| Mode | Description |
|------|-------------|
| `distance` | Switch based on camera distance |
| `screen_coverage` | Switch based on screen percentage |

### Transition Modes

| Mode | Description |
|------|-------------|
| `instant` | Immediate switch (may pop) |
| `cross_fade` | Alpha blend between levels |
| `dither` | Dithered transition |

## Scene Streaming

For large worlds with chunked loading:

```toml
[app.streaming]
enabled = true
chunk_size = 100.0           # Meters
load_distance = 500.0        # Start loading at this distance
unload_distance = 600.0      # Unload beyond this distance
max_concurrent_loads = 4     # Parallel load operations
max_memory_mb = 512          # Memory budget
preload_neighbors = true     # Predictive loading
priority_bias = "camera_direction"  # camera_direction, distance

[app.streaming.recycling]
enabled = true
pool_size = 1000             # Object pool size
cooldown_frames = 60         # Frames before recycling
```

## Precision Management

For large-scale worlds:

```toml
[app.precision]
enabled = true
origin_rebase_threshold = 5000.0  # Meters from origin
camera_relative_rendering = true
double_precision_physics = true
coordinate_system = "right_handed"  # right_handed, left_handed
```

## Permissions

Application capability permissions:

```toml
[app.permissions]
scripts = true               # Allow script execution
network = false              # Network access
filesystem = false           # File system access (beyond assets)
audio = true                 # Audio playback
input = true                 # Input device access
```

## Resource Limits

Budget constraints for the application:

```toml
[app.resources]
max_entities = 10000
max_memory = 536870912       # Bytes (512MB)
max_layers = 16
max_cpu_ms = 16.0            # Per frame (60fps target)
max_draw_calls = 1000
max_triangles = 10000000
max_lights = 128
max_shadow_casters = 16
```

### Resource Guidelines

| Resource | Minimum | Recommended | Maximum |
|----------|---------|-------------|---------|
| Entities | 100 | 5,000 | 100,000 |
| Memory | 64MB | 512MB | 4GB |
| Draw Calls | 50 | 500 | 5,000 |
| Triangles | 10K | 1M | 50M |
| Lights | 8 | 64 | 512 |

## Assets

Asset loading configuration:

```toml
[assets]
include = ["assets/", "shared/"]
exclude = ["*.psd", "*.blend"]
compression = 6              # 0-9 (0=none, 9=max)
hot_reload = true            # Enable hot-reloading
```

## Platform Requirements

Specify platform compatibility:

```toml
[platform]
min_version = "0.1.0"        # Minimum engine version
required_features = ["hot-reload", "pbr", "shadows"]
optional_features = ["raytracing", "dlss"]
```

### Available Features

| Feature | Description |
|---------|-------------|
| `hot-reload` | Asset hot-reloading |
| `pbr` | Physically-based rendering |
| `shadows` | Shadow mapping |
| `raytracing` | Hardware ray tracing |
| `dlss` | NVIDIA DLSS support |
| `fsr` | AMD FSR support |

## Complete Example

```toml
[package]
name = "space-game"
display_name = "Space Game"
version = "1.0.0"
description = "A space exploration game"
author = "Developer"
license = "MIT"
keywords = ["space", "3d", "exploration"]
categories = ["game"]

[app]
app_type = "game"
scene = "scenes/main.toml"

[[app.layers]]
name = "shadow"
type = "shadow"
priority = 0
blend = "replace"

[[app.layers]]
name = "world"
type = "content"
priority = 10
blend = "normal"

[[app.layers]]
name = "transparent"
type = "content"
priority = 15
blend = "normal"

[[app.layers]]
name = "effects"
type = "effect"
priority = 50
blend = "additive"

[[app.layers]]
name = "ui"
type = "overlay"
priority = 100
blend = "normal"

[app.lod]
enabled = true
mode = "distance"
default_mode = "cross_fade"
cross_fade_duration = 0.3

[app.streaming]
enabled = true
chunk_size = 200.0
load_distance = 1000.0
unload_distance = 1200.0
max_concurrent_loads = 8
max_memory_mb = 1024

[app.precision]
enabled = true
origin_rebase_threshold = 10000.0
camera_relative_rendering = true

[app.permissions]
scripts = true
audio = true
input = true

[app.resources]
max_entities = 50000
max_memory = 2147483648      # 2GB
max_layers = 8
max_cpu_ms = 16.0
max_draw_calls = 2000
max_triangles = 20000000
max_lights = 256
max_shadow_casters = 32

[assets]
include = ["assets/"]
compression = 6
hot_reload = true

[platform]
min_version = "1.0.0"
required_features = ["hot-reload", "pbr", "shadows"]
```

## C++ Loading API

```cpp
#include <void_engine/app/manifest.hpp>

using namespace void_engine::app;

// Load manifest from file
auto result = Manifest::load("manifest.toml");
if (result.is_ok()) {
    Manifest manifest = result.unwrap();

    // Access configuration
    std::string name = manifest.package().name;
    uint32_t max_entities = manifest.resources().max_entities;

    // Initialize engine with manifest
    engine.configure(manifest);
}
```

## Validation

The manifest loader validates:

- Required fields present
- Package name format
- Semantic version format
- Layer names unique
- Layer priorities unique
- Resource limits within platform capabilities
- Referenced files exist
