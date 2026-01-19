# Debug Configuration

Debug visualization and development tools.

## Scene-Level Debug Config

```toml
[debug]
enabled = true                 # Master debug toggle
```

## Statistics Overlay

Display performance metrics.

```toml
[debug.stats]
enabled = true                 # Show stats overlay
position = "top_left"          # Screen position
font_size = 14                 # Text size
background_alpha = 0.7         # Background opacity
```

### Position Options

| Value | Location |
|-------|----------|
| `top_left` | Top-left corner |
| `top_right` | Top-right corner |
| `bottom_left` | Bottom-left corner |
| `bottom_right` | Bottom-right corner |

### Stat Display Options

```toml
[debug.stats.display]
fps = true                     # Frames per second
frame_time = true              # Frame time in ms
draw_calls = true              # Number of draw calls
triangles = true               # Triangle count
entities_total = true          # Total entities
entities_visible = true        # Visible entities (after culling)
gpu_memory = false             # GPU memory usage
cpu_time = true                # CPU frame time
```

## Debug Visualization

Visual overlays for debugging.

```toml
[debug.visualization]
enabled = true                 # Enable visualizations
bounds = false                 # Show bounding boxes
wireframe = false              # Wireframe mode
normals = false                # Show surface normals
light_volumes = false          # Show light ranges
shadow_cascades = false        # Show shadow cascade splits
lod_levels = false             # Color by LOD level
skeleton = false               # Show bone structure
```

### Appearance Customization

```toml
[debug.visualization.appearance]
bounds_color = [0, 1, 0, 0.5]      # Green, semi-transparent
normal_color = [0, 0.5, 1, 1]     # Blue
normal_length = 0.1                # Normal line length
line_width = 1.0                   # Line thickness
```

## Debug Controls

Keyboard shortcuts for debug features.

```toml
[debug.controls]
toggle_key = "F3"              # Toggle debug overlay
cycle_mode_key = "F4"          # Cycle visualization modes
reload_shaders_key = "F5"      # Hot-reload shaders
```

## App-Level Debug Config

Configure in `manifest.toml`:

```toml
[app.debug]
enabled = true
show_stats_overlay = true
stats_position = "top_right"

[app.debug.visualization]
bounds = false
normals = false
wireframe = false
light_volumes = false
shadow_cascades = false
lod_colors = false
chunk_boundaries = false
collision_shapes = false
skeleton = false
velocity_vectors = false

[app.debug.appearance]
bounds_color = [0, 1, 0, 0.5]
normal_color = [0, 0.5, 1, 1]
normal_length = 0.1
line_width = 1.0
font_size = 12
```

## Debug Presets

### Performance Analysis

```toml
[debug]
enabled = true

[debug.stats]
enabled = true
position = "top_left"

[debug.stats.display]
fps = true
frame_time = true
draw_calls = true
triangles = true
entities_visible = true
gpu_memory = true
cpu_time = true

[debug.visualization]
enabled = false
```

### Visual Debugging

```toml
[debug]
enabled = true

[debug.stats]
enabled = false

[debug.visualization]
enabled = true
bounds = true
wireframe = false
normals = true
light_volumes = true

[debug.visualization.appearance]
bounds_color = [1, 1, 0, 0.3]
normal_color = [1, 0, 1, 1]
normal_length = 0.2
```

### Lighting Debug

```toml
[debug]
enabled = true

[debug.visualization]
enabled = true
light_volumes = true
shadow_cascades = true
bounds = false
```

### LOD Debug

```toml
[debug]
enabled = true

[debug.stats]
enabled = true

[debug.stats.display]
entities_total = true
entities_visible = true

[debug.visualization]
enabled = true
lod_levels = true              # Color entities by LOD
bounds = true
```

### Animation Debug

```toml
[debug]
enabled = true

[debug.visualization]
enabled = true
skeleton = true                # Show bone hierarchy
bounds = true
velocity_vectors = true
```

## Complete Debug Configuration

```toml
[debug]
enabled = true

[debug.stats]
enabled = true
position = "top_left"
font_size = 14
background_alpha = 0.8

[debug.stats.display]
fps = true
frame_time = true
draw_calls = true
triangles = true
entities_total = true
entities_visible = true
gpu_memory = false
cpu_time = true

[debug.visualization]
enabled = false
bounds = false
wireframe = false
normals = false
light_volumes = false
shadow_cascades = false
lod_levels = false
skeleton = false

[debug.visualization.appearance]
bounds_color = [0, 1, 0, 0.5]
normal_color = [0, 0.5, 1, 1]
normal_length = 0.1
line_width = 1.0

[debug.controls]
toggle_key = "F3"
cycle_mode_key = "F4"
reload_shaders_key = "F5"
```

## Disabling Debug

For release builds:

```toml
[debug]
enabled = false
```

Or remove the entire `[debug]` section.
