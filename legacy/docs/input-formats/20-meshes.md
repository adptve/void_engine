# Meshes and 3D Models

Built-in primitives and external model loading.

## Built-in Primitives

Use primitive names directly in the `mesh` field.

```toml
[[entities]]
name = "my_cube"
mesh = "cube"
```

### Available Primitives

| Primitive | Description |
|-----------|-------------|
| `cube` | Unit cube (1x1x1) |
| `sphere` | UV sphere |
| `plane` | Flat quad (horizontal) |
| `cylinder` | Vertical cylinder |
| `cone` | Vertical cone |
| `torus` | Donut shape |
| `diamond` | Diamond/octahedron |

## Primitive Examples

### Cube

```toml
[[entities]]
name = "box"
mesh = "cube"

[entities.transform]
scale = [2, 1, 3]              # 2m wide, 1m tall, 3m deep
```

### Sphere

```toml
[[entities]]
name = "ball"
mesh = "sphere"

[entities.transform]
scale = 0.5                    # 0.5m radius
```

### Plane (Floor/Wall)

```toml
[[entities]]
name = "floor"
mesh = "plane"

[entities.transform]
scale = 10                     # 10x10 floor

[[entities]]
name = "wall"
mesh = "plane"

[entities.transform]
position = [0, 2.5, -5]
rotation = [90, 0, 0]          # Rotate to vertical
scale = [10, 5, 1]             # 10m wide, 5m tall
```

### Cylinder

```toml
[[entities]]
name = "pillar"
mesh = "cylinder"

[entities.transform]
scale = [0.5, 3, 0.5]          # Thin, tall cylinder
```

### Cone

```toml
[[entities]]
name = "tree_top"
mesh = "cone"

[entities.transform]
position = [0, 2, 0]
scale = [1, 2, 1]              # 1m radius, 2m tall
```

### Torus

```toml
[[entities]]
name = "ring"
mesh = "torus"

[entities.transform]
rotation = [90, 0, 0]          # Lay flat
```

### Diamond

```toml
[[entities]]
name = "gem"
mesh = "diamond"

[entities.transform]
scale = 0.3
```

## External Models

Load 3D models from files.

```toml
[[entities]]
name = "character"
mesh = "assets/models/character.glb"
```

### Supported Formats

| Format | Extension | Notes |
|--------|-----------|-------|
| glTF 2.0 | `.gltf` | Text format with external files |
| glTF Binary | `.glb` | Single binary file (recommended) |

### GLB vs GLTF

- **GLB**: Single file, easier to manage, recommended
- **GLTF**: Text format + separate files (textures, buffers)

## Model Paths

Paths are relative to the app directory (where manifest.toml is).

```
my-app/
├── manifest.toml
├── scene.toml
└── assets/
    └── models/
        └── character.glb
```

```toml
mesh = "assets/models/character.glb"
```

## Model with LOD

```toml
[[entities]]
name = "complex_model"
mesh = "assets/models/building_high.glb"

[[entities.lod.levels]]
distance = 50.0
mesh = "assets/models/building_medium.glb"

[[entities.lod.levels]]
distance = 150.0
mesh = "assets/models/building_low.glb"

[[entities.lod.levels]]
distance = 500.0
mesh = "hide"
```

## Model Materials

### Use Embedded Materials

GLTF models include materials. To use them:

```toml
[[entities]]
name = "textured_model"
mesh = "assets/models/house.glb"
# No material section = use model's materials
```

### Override Materials

Replace model materials with custom ones:

```toml
[[entities]]
name = "recolored_model"
mesh = "assets/models/house.glb"

[entities.material]
albedo = [1.0, 0.0, 0.0, 1.0]  # Override to red
metallic = 0.5
roughness = 0.3
```

## Primitive Composition

Build complex objects from primitives.

### Simple Tree

```toml
# Trunk
[[entities]]
name = "tree_trunk"
mesh = "cylinder"

[entities.transform]
position = [0, 0.75, 0]
scale = [0.2, 1.5, 0.2]

[entities.material]
albedo = [0.4, 0.25, 0.1, 1.0]

# Foliage
[[entities]]
name = "tree_foliage"
mesh = "sphere"

[entities.transform]
position = [0, 2.5, 0]
scale = 1.5

[entities.material]
albedo = [0.2, 0.6, 0.2, 1.0]
```

### Simple House

```toml
# Base
[[entities]]
name = "house_base"
mesh = "cube"

[entities.transform]
position = [0, 1, 0]
scale = [4, 2, 3]

[entities.material]
albedo = [0.9, 0.9, 0.85, 1.0]

# Roof
[[entities]]
name = "house_roof"
mesh = "cube"

[entities.transform]
position = [0, 2.5, 0]
rotation = [0, 0, 45]
scale = [3.5, 0.5, 3.2]

[entities.material]
albedo = [0.6, 0.3, 0.2, 1.0]
```

## Multiple Instances

Same mesh, different transforms:

```toml
[[entities]]
name = "pillar_1"
mesh = "cylinder"
[entities.transform]
position = [-3, 1.5, 0]
scale = [0.3, 3, 0.3]

[[entities]]
name = "pillar_2"
mesh = "cylinder"
[entities.transform]
position = [3, 1.5, 0]
scale = [0.3, 3, 0.3]

[[entities]]
name = "pillar_3"
mesh = "cylinder"
[entities.transform]
position = [0, 1.5, -3]
scale = [0.3, 3, 0.3]
```

## Model Transform

Adjust imported model positioning:

```toml
[[entities]]
name = "imported_model"
mesh = "assets/models/chair.glb"

[entities.transform]
position = [2, 0, 0]           # Move to position
rotation = [0, 90, 0]          # Rotate 90 degrees
scale = 0.01                   # Scale down (if model is in cm)
```

### Common Scale Conversions

| Model Units | Scale Factor |
|-------------|--------------|
| Centimeters | 0.01 |
| Inches | 0.0254 |
| Feet | 0.3048 |
| Meters | 1.0 |
