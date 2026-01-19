# Scene File Format

Scene files define the complete state of a 3D scene including entities, cameras, lights, environment, and materials.

## File Format

- **Extension**: `.toml`
- **Encoding**: UTF-8
- **Format**: TOML 1.0

## Scene Structure Overview

```toml
[scene]                      # Scene metadata (required)
[[cameras]]                  # Camera definitions (array)
[[lights]]                   # Light definitions (array)
[shadows]                    # Shadow configuration
[environment]                # Sky and ambient settings
[[entities]]                 # Scene objects (array)
[[particle_emitters]]        # Particle systems (array)
[input]                      # Input bindings
[debug]                      # Debug visualization
```

## Scene Metadata

```toml
[scene]
name = "My Scene"            # Required: scene identifier
description = "Description"  # Optional: human-readable description
version = "1.0.0"            # Optional: scene version
```

## Entities

Entities are the primary objects in a scene. Each entity can have transform, mesh, material, and behavior components.

### Basic Entity

```toml
[[entities]]
name = "my_object"           # Required: unique identifier
mesh = "cube"                # Mesh type or asset path
layer = "world"              # Render layer (default: "world")
visible = true               # Visibility flag (default: true)
```

### Transform Component

```toml
[entities.transform]
position = [0, 0, 0]         # [x, y, z] world position
rotation = [0, 45, 0]        # [x, y, z] Euler angles in degrees
scale = 1.0                  # Uniform scale
# OR
scale = [1, 2, 1]            # Non-uniform [x, y, z] scale
```

### Built-in Mesh Types

```toml
mesh = "cube"
mesh = "sphere"
mesh = "plane"
mesh = "cylinder"
mesh = "cone"
mesh = "torus"
mesh = "diamond"
```

### External Models

```toml
mesh = "assets/models/character.glb"
mesh = "assets/models/building.gltf"
```

### Material Component

```toml
[entities.material]
# Base color (RGBA)
albedo = [1.0, 0.0, 0.0, 1.0]
# OR texture
albedo = { texture = "assets/textures/wood.png" }

# PBR properties
metallic = 0.0               # 0.0 = dielectric, 1.0 = metal
roughness = 0.5              # 0.0 = mirror, 1.0 = rough

# Optional maps
normal_map = "normal.png"    # Normal map texture
ao_map = "ao.png"            # Ambient occlusion
emissive = [0, 0, 0]         # Self-illumination RGB
```

### Material Presets

| Material | Albedo | Metallic | Roughness |
|----------|--------|----------|-----------|
| Chrome | [0.95, 0.95, 0.95, 1.0] | 1.0 | 0.05 |
| Gold | [1.0, 0.766, 0.336, 1.0] | 1.0 | 0.3 |
| Copper | [0.955, 0.637, 0.538, 1.0] | 1.0 | 0.4 |
| Plastic | [0.8, 0.1, 0.1, 1.0] | 0.0 | 0.3 |
| Concrete | [0.5, 0.5, 0.5, 1.0] | 0.0 | 0.9 |
| Wood | [0.6, 0.4, 0.2, 1.0] | 0.0 | 0.7 |

### Pickable Component

Enable entity selection via mouse/pointer:

```toml
[entities.pickable]
enabled = true
priority = 0
bounds = "mesh"              # mesh, aabb, sphere
highlight_on_hover = true
group = "interactive"
```

### Input Events

```toml
[entities.input_events]
on_click = "handle_click"
on_pointer_enter = "show_hover"
on_pointer_exit = "hide_hover"
on_drag_start = "begin_drag"
on_drag = "update_drag"
on_drag_end = "end_drag"
```

### LOD (Level of Detail)

```toml
[entities.lod]
bias = 0.0
fade_transition = true
fade_duration = 0.2

[[entities.lod.levels]]
distance = 10.0
mesh = "assets/models/medium_detail.glb"

[[entities.lod.levels]]
distance = 30.0
mesh = "assets/models/low_detail.glb"

[[entities.lod.levels]]
distance = 100.0
mesh = "hide"                # Cull at this distance
```

### Animation Component

```toml
[entities.animation]
type = "rotate"
axis = [0, 1, 0]
speed = 1.0
```

### Render Pass Control

```toml
[entities.render_pass]
cast_shadows = true
receive_shadows = true
reflect = false
refract = false
custom_passes = ["transparent"]
order = 0                    # Render order (higher = later)
```

## Cameras

### Perspective Camera

```toml
[[cameras]]
name = "main"
active = true
type = "perspective"

[cameras.transform]
position = [0, 5, 10]
target = [0, 0, 0]
up = [0, 1, 0]

[cameras.perspective]
fov = 60.0                   # Field of view in degrees
near = 0.1                   # Near clip plane
far = 1000.0                 # Far clip plane
aspect = "auto"              # "auto" or fixed ratio
```

### FOV Guidelines

| FOV | Use Case |
|-----|----------|
| 30-45 | Telephoto, cinematic |
| 60-75 | Standard view |
| 90-110 | Wide angle, FPS games |
| 120+ | Fisheye effect |

### Orthographic Camera

```toml
[[cameras]]
name = "ortho"
active = false
type = "orthographic"

[cameras.transform]
position = [10, 10, 10]
target = [0, 0, 0]

[cameras.orthographic]
left = -10.0
right = 10.0
bottom = -10.0
top = 10.0
near = 0.1
far = 100.0
```

### Camera Presets

```toml
# First-Person View
[[cameras]]
name = "first_person"
type = "perspective"
[cameras.transform]
position = [0, 1.7, 0]       # Eye height
target = [0, 1.7, -1]
[cameras.perspective]
fov = 90.0
near = 0.01
far = 500.0

# Isometric Camera
[[cameras]]
name = "isometric"
type = "orthographic"
[cameras.transform]
position = [10, 10, 10]
target = [0, 0, 0]
[cameras.orthographic]
left = -15.0
right = 15.0
bottom = -10.0
top = 10.0
```

## Lights

### Directional Light

Simulates distant light source like the sun:

```toml
[[lights]]
name = "sun"
type = "directional"
enabled = true

[lights.directional]
direction = [0.5, -0.7, 0.5] # Vector toward light source
color = [1.0, 0.95, 0.9]     # RGB color
intensity = 3.0               # Brightness multiplier
cast_shadows = true
```

### Sunlight Presets

```toml
# Midday
direction = [0.2, -0.9, 0.1]
color = [1.0, 1.0, 0.98]
intensity = 5.0

# Golden Hour
direction = [0.7, -0.2, 0.3]
color = [1.0, 0.8, 0.5]
intensity = 3.0

# Moonlight
direction = [0.3, -0.5, 0.4]
color = [0.7, 0.8, 1.0]
intensity = 0.3
```

### Point Light

Omnidirectional light from a single point:

```toml
[[lights]]
name = "lamp"
type = "point"
enabled = true

[lights.point]
position = [0, 3, 0]
color = [1.0, 0.9, 0.7]
intensity = 100.0
range = 10.0
cast_shadows = false

[lights.point.attenuation]
constant = 1.0
linear = 0.09
quadratic = 0.032
```

### Spot Light

Cone-shaped beam:

```toml
[[lights]]
name = "spotlight"
type = "spot"
enabled = true

[lights.spot]
position = [0, 5, 0]
direction = [0, -1, 0]
color = [1.0, 1.0, 1.0]
intensity = 200.0
range = 15.0
inner_angle = 20.0           # Full intensity cone (degrees)
outer_angle = 35.0           # Falloff cone (degrees)
cast_shadows = true
```

## Environment

### Basic Environment

```toml
[environment]
ambient_intensity = 0.25
light_direction = [0.5, -0.7, 0.5]
light_color = [1.0, 0.95, 0.85]
light_intensity = 3.0
```

### Environment Map (HDR)

```toml
[environment]
environment_map = "assets/hdri/studio.hdr"
ambient_intensity = 0.3
```

### Procedural Sky

```toml
[environment.sky]
zenith_color = [0.1, 0.3, 0.6]
horizon_color = [0.5, 0.7, 0.9]
ground_color = [0.15, 0.12, 0.1]
```

### Sky Presets

```toml
# Clear Day
zenith_color = [0.1, 0.4, 0.8]
horizon_color = [0.6, 0.8, 1.0]
ground_color = [0.2, 0.18, 0.15]

# Sunset
zenith_color = [0.1, 0.15, 0.4]
horizon_color = [1.0, 0.5, 0.2]
ground_color = [0.15, 0.1, 0.08]

# Night
zenith_color = [0.01, 0.01, 0.03]
horizon_color = [0.05, 0.05, 0.1]
ground_color = [0.02, 0.02, 0.02]
```

### Sun Disc

```toml
[environment.sky]
sun_size = 0.03              # Angular size
sun_intensity = 50.0         # Brightness
sun_falloff = 3.0            # Glow falloff
```

### Fog

```toml
[environment.sky]
fog_density = 0.02
fog_color = [0.7, 0.75, 0.8]
```

## Shadows

```toml
[shadows]
enabled = true
resolution = 2048            # Shadow map resolution
cascade_count = 4            # CSM cascades
soft_shadows = true
bias = 0.005
```

## Complete Scene Example

```toml
[scene]
name = "Demo Scene"
description = "A complete demo scene"
version = "1.0.0"

# Camera
[[cameras]]
name = "main"
active = true
type = "perspective"

[cameras.transform]
position = [0, 5, 10]
target = [0, 0, 0]
up = [0, 1, 0]

[cameras.perspective]
fov = 60.0
near = 0.1
far = 1000.0

# Sun light
[[lights]]
name = "sun"
type = "directional"
enabled = true

[lights.directional]
direction = [0.5, -0.7, 0.5]
color = [1.0, 0.95, 0.9]
intensity = 3.0
cast_shadows = true

# Environment
[environment]
ambient_intensity = 0.2

[environment.sky]
zenith_color = [0.1, 0.3, 0.6]
horizon_color = [0.5, 0.7, 0.9]
sun_size = 0.025
sun_intensity = 80.0

# Floor entity
[[entities]]
name = "floor"
mesh = "plane"

[entities.transform]
position = [0, 0, 0]
scale = 10.0

[entities.material]
albedo = [0.5, 0.5, 0.5, 1.0]
roughness = 0.8

# Metallic sphere
[[entities]]
name = "sphere"
mesh = "sphere"

[entities.transform]
position = [0, 1, 0]

[entities.material]
albedo = [0.8, 0.2, 0.2, 1.0]
metallic = 0.9
roughness = 0.1
```

## Data Types Reference

| Type | Format | Example |
|------|--------|---------|
| Position | `[f32; 3]` | `[1.0, 2.0, 3.0]` |
| Color RGB | `[f32; 3]` | `[1.0, 0.5, 0.0]` |
| Color RGBA | `[f32; 4]` | `[1.0, 0.5, 0.0, 1.0]` |
| Rotation | `[f32; 3]` | `[0, 45, 0]` (degrees) |
| Scale | `f32` or `[f32; 3]` | `2.0` or `[1, 2, 1]` |
| Bool | `bool` | `true`, `false` |
| String | `"string"` | `"my_name"` |
| Path | `"string"` | `"assets/textures/wood.png"` |

## Path Resolution

Paths are relative to the application directory containing `manifest.toml`:

```
my_app/
├── manifest.toml
├── scene.toml
└── assets/
    ├── textures/
    │   └── wood.png      # Referenced as "assets/textures/wood.png"
    └── models/
        └── char.glb      # Referenced as "assets/models/char.glb"
```

## C++ Loading API

```cpp
#include <void_engine/scene/loader.hpp>

using namespace void_engine::scene;

// Load scene from file
auto result = SceneLoader::load("scenes/main.toml");
if (result.is_ok()) {
    Scene scene = result.unwrap();
    engine.set_scene(std::move(scene));
}

// Parse scene from string
auto result = SceneLoader::parse(toml_string);

// Hot-reload support
scene_watcher.watch("scenes/main.toml", [&](auto const& path) {
    auto result = SceneLoader::load(path);
    if (result.is_ok()) {
        engine.reload_scene(result.unwrap());
    }
});
```

## Validation

The scene loader validates:

- Required fields (scene.name, entity names)
- Unique entity names within scene
- Valid mesh references (built-in or asset paths)
- Valid texture paths
- Camera type matches configuration (perspective/orthographic)
- Light type matches configuration
- Value ranges (0-1 for colors, positive for distances)
