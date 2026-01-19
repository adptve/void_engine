# Entities

Entities are the building blocks of scenes - objects with transforms, meshes, and materials.

## Basic Entity

```toml
[[entities]]
name = "my_object"           # Required, unique identifier
mesh = "cube"                # Mesh type or path
layer = "world"              # Render layer (default: "world")
visible = true               # Visibility (default: true)
```

## Transform

Position, rotation, and scale in world space.

```toml
[entities.transform]
position = [0, 0, 0]         # [x, y, z] (default: origin)
rotation = [0, 45, 0]        # [x, y, z] in degrees (default: no rotation)
scale = 1.0                  # Uniform scale (default: 1.0)
# OR
scale = [1, 2, 1]            # Non-uniform [x, y, z]
```

## Mesh Types

### Built-in Primitives

```toml
mesh = "cube"
mesh = "sphere"
mesh = "plane"
mesh = "cylinder"
mesh = "cone"
mesh = "torus"
mesh = "diamond"
```

### External Model

```toml
mesh = "assets/models/character.glb"
mesh = "assets/models/building.gltf"
```

## Entity with Material

```toml
[[entities]]
name = "metal_sphere"
mesh = "sphere"

[entities.transform]
position = [0, 1, 0]

[entities.material]
albedo = [0.8, 0.8, 0.8, 1.0]
metallic = 1.0
roughness = 0.2
```

## Entity with Textures

```toml
[[entities]]
name = "textured_cube"
mesh = "cube"

[entities.material]
albedo = { texture = "assets/textures/wood_albedo.png" }
normal_map = "assets/textures/wood_normal.png"
metallic = { texture = "assets/textures/wood_metallic.png" }
roughness = { texture = "assets/textures/wood_roughness.png" }
ao_map = "assets/textures/wood_ao.png"
```

## Entity with Animation

```toml
[[entities]]
name = "spinning_cube"
mesh = "cube"

[entities.transform]
position = [0, 1, 0]

[entities.material]
albedo = [1.0, 0.0, 0.0, 1.0]

[entities.animation]
type = "rotate"
axis = [0, 1, 0]
speed = 1.0
```

## Pickable Entity

Enable selection via mouse/pointer.

```toml
[[entities]]
name = "clickable"
mesh = "cube"

[entities.pickable]
enabled = true
priority = 0
bounds = "mesh"              # mesh, aabb, sphere, custom
highlight_on_hover = true
group = "interactive"
```

## Entity with Input Events

```toml
[[entities]]
name = "button"
mesh = "cube"

[entities.pickable]
enabled = true

[entities.input_events]
on_click = "handle_click"
on_pointer_enter = "show_hover"
on_pointer_exit = "hide_hover"
on_drag_start = "begin_drag"
on_drag = "update_drag"
on_drag_end = "end_drag"
```

## Entity with LOD

```toml
[[entities]]
name = "complex_model"
mesh = "assets/models/high_detail.glb"

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

## Render Pass Control

```toml
[[entities]]
name = "glass_panel"
mesh = "plane"

[entities.render_pass]
cast_shadows = false
receive_shadows = true
reflect = true
refract = true
custom_passes = ["transparent"]
order = 100                  # Render later for transparency
```

## Multiple Entities

```toml
[[entities]]
name = "floor"
mesh = "plane"
[entities.transform]
scale = 10.0
[entities.material]
albedo = [0.3, 0.3, 0.3, 1.0]

[[entities]]
name = "wall_left"
mesh = "cube"
[entities.transform]
position = [-5, 2.5, 0]
scale = [0.1, 5, 10]
[entities.material]
albedo = [0.8, 0.8, 0.8, 1.0]
```
