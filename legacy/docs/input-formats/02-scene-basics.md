# Scene Basics (scene.toml)

The scene file defines all objects, lighting, cameras, and behavior in a 3D scene.

## Scene Metadata

```toml
[scene]
name = "My Scene"            # Required
description = "A test scene"
version = "1.0.0"
```

## Top-Level Sections

A complete scene.toml can contain these sections:

```toml
[scene]                      # Metadata
[[cameras]]                  # Camera definitions (array)
[[lights]]                   # Light definitions (array)
[shadows]                    # Shadow configuration
[environment]                # Sky and ambient
[picking]                    # Object selection config
[spatial]                    # Spatial query config
[debug]                      # Debug visualization
[input]                      # Input bindings
[[entities]]                 # Scene objects (array)
[[particle_emitters]]        # Particle systems (array)
[[textures]]                 # Preloaded textures (array)
```

## Minimal Scene Example

```toml
[scene]
name = "Hello Cube"

[[entities]]
name = "cube"
mesh = "cube"

[entities.transform]
position = [0, 0, 0]

[entities.material]
albedo = [1.0, 0.0, 0.0, 1.0]  # Red
```

## Complete Scene Example

```toml
[scene]
name = "Demo Scene"
description = "A complete demo"
version = "1.0.0"

# Camera
[[cameras]]
name = "main"
active = true
type = "perspective"

[cameras.transform]
position = [0, 2, 5]
target = [0, 0, 0]
up = [0, 1, 0]

[cameras.perspective]
fov = 60.0
near = 0.1
far = 1000.0

# Light
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

# Entity
[[entities]]
name = "floor"
mesh = "plane"

[entities.transform]
position = [0, 0, 0]
scale = 10.0

[entities.material]
albedo = [0.5, 0.5, 0.5, 1.0]
roughness = 0.8

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

## Path References

Paths are relative to the app directory containing manifest.toml:

```toml
# Texture path
albedo = { texture = "assets/textures/wood.png" }

# Model path
mesh = "assets/models/character.glb"
```
