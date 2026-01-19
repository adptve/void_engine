# Render Layers

Layer-based rendering for compositing and multi-pass effects.

## Layer Basics

Layers organize rendering into composable passes.

```toml
[[app.layers]]
name = "world"                 # Layer identifier
type = "content"               # Layer type
priority = 10                  # Render order (lower = first)
blend = "normal"               # Blend mode
```

## Layer Types

| Type | Description | Use Case |
|------|-------------|----------|
| `shadow` | Shadow map generation | Shadow casting |
| `content` | Main 3D content | Scene objects |
| `overlay` | UI/HUD elements | 2D interface |
| `effect` | Post-processing | Bloom, DOF |
| `portal` | Render-to-texture | Mirrors, portals |

## Blend Modes

| Mode | Description |
|------|-------------|
| `replace` | Overwrite previous |
| `normal` | Standard alpha blend |
| `additive` | Add colors together |
| `multiply` | Darken by multiplying |

## Priority

Lower priority renders first, higher renders on top.

```toml
# Render order: shadow → world → effects → ui
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

## Assigning Entities to Layers

```toml
[[entities]]
name = "floor"
mesh = "plane"
layer = "world"                # Renders in world layer

[[entities]]
name = "ui_panel"
mesh = "quad"
layer = "ui"                   # Renders in UI layer
```

## Common Layer Configurations

### Basic 3D Scene

```toml
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
name = "ui"
type = "overlay"
priority = 100
blend = "normal"
```

### With Post-Processing

```toml
[[app.layers]]
name = "shadow"
type = "shadow"
priority = 0

[[app.layers]]
name = "world"
type = "content"
priority = 10

[[app.layers]]
name = "bloom"
type = "effect"
priority = 50
blend = "additive"

[[app.layers]]
name = "ui"
type = "overlay"
priority = 100
```

### With Portals/Mirrors

```toml
[[app.layers]]
name = "portal_render"
type = "portal"
priority = 5
blend = "replace"

[[app.layers]]
name = "world"
type = "content"
priority = 10

[[app.layers]]
name = "portal_composite"
type = "content"
priority = 15
blend = "normal"
```

### Multi-World

```toml
[[app.layers]]
name = "background_world"
type = "content"
priority = 0
blend = "replace"

[[app.layers]]
name = "main_world"
type = "content"
priority = 10
blend = "normal"

[[app.layers]]
name = "foreground_effects"
type = "effect"
priority = 20
blend = "additive"
```

## Layer Properties in Rust

From `void_render/src/layer.rs`:

```toml
# Full layer specification
[[app.layers]]
name = "custom"
type = "content"
priority = 25

# Additional properties (if supported)
z_order = 25                   # Alternative to priority
opacity = 1.0                  # Layer opacity (0-1)
visible = true                 # Layer visibility
clear_color = [0, 0, 0, 0]     # Clear color RGBA
render_scale = 1.0             # Resolution scale
use_depth = true               # Has depth buffer
```

## Entity Layer Assignment

### Default Layer

Entities without explicit layer go to "world":

```toml
[[entities]]
name = "object"
mesh = "cube"
# layer defaults to "world"
```

### Explicit Layer

```toml
[[entities]]
name = "hud_element"
mesh = "quad"
layer = "ui"
```

### Layer Filtering for Picking

```toml
[picking]
layer_mask = ["world", "interactive"]  # Only pick from these layers
```

## Complete Layer Example

### manifest.toml

```toml
[package]
name = "layered-app"
version = "1.0.0"

[app]
scene = "scene.toml"

[[app.layers]]
name = "shadow"
type = "shadow"
priority = 0
blend = "replace"

[[app.layers]]
name = "background"
type = "content"
priority = 5
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
```

### scene.toml

```toml
[scene]
name = "Layered Scene"

# Sky in background layer
[[entities]]
name = "sky_dome"
mesh = "sphere"
layer = "background"
[entities.material]
emissive = [0.5, 0.7, 1.0]

# Opaque objects in world layer
[[entities]]
name = "floor"
mesh = "plane"
layer = "world"

[[entities]]
name = "building"
mesh = "cube"
layer = "world"

# Glass in transparent layer (renders after world)
[[entities]]
name = "window"
mesh = "plane"
layer = "transparent"
[entities.material]
albedo = [1, 1, 1, 0.3]

# UI elements
[[entities]]
name = "health_bar"
mesh = "quad"
layer = "ui"
```

## Layer Best Practices

1. **Shadow layer first** (priority 0) - Generate shadow maps
2. **Background layer** (priority 5) - Skybox, distant objects
3. **World layer** (priority 10) - Main opaque content
4. **Transparent layer** (priority 15) - Glass, particles
5. **Effects layer** (priority 50) - Post-processing
6. **UI layer** (priority 100) - Interface elements
