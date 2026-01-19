# PBR Materials

Physically-based rendering materials using the metallic-roughness workflow.

## Material Properties

```toml
[entities.material]
# Base color (required for visible objects)
albedo = [1.0, 0.0, 0.0, 1.0]     # RGBA color
# OR
albedo = [1.0, 0.0, 0.0]          # RGB color (alpha = 1.0)
# OR
albedo = { texture = "path.png" } # Texture

# Surface properties
metallic = 0.0                     # 0.0 = dielectric, 1.0 = metal
roughness = 0.5                    # 0.0 = mirror, 1.0 = rough

# Optional maps
normal_map = "normal.png"          # Normal map texture
ao_map = "ao.png"                  # Ambient occlusion
emissive = [0, 0, 0]              # Self-illumination RGB
```

## Color Values

Colors use 0.0-1.0 range, not 0-255.

```toml
# Pure red
albedo = [1.0, 0.0, 0.0, 1.0]

# 50% gray
albedo = [0.5, 0.5, 0.5, 1.0]

# Orange with transparency
albedo = [1.0, 0.5, 0.0, 0.8]
```

## Metallic-Roughness Values

| Material | Metallic | Roughness |
|----------|----------|-----------|
| Polished metal | 1.0 | 0.1 |
| Brushed metal | 1.0 | 0.4 |
| Plastic | 0.0 | 0.3 |
| Rubber | 0.0 | 0.9 |
| Wood | 0.0 | 0.7 |
| Marble | 0.0 | 0.2 |
| Skin | 0.0 | 0.5 |

## Texture-Based Properties

Any property can use a texture instead of a value.

```toml
[entities.material]
# Color from texture
albedo = { texture = "assets/textures/brick_color.png" }

# Metallic from texture (grayscale: black=0, white=1)
metallic = { texture = "assets/textures/brick_metallic.png" }

# Roughness from texture
roughness = { texture = "assets/textures/brick_roughness.png" }
```

## Normal Maps

Add surface detail without geometry.

```toml
[entities.material]
albedo = [0.8, 0.8, 0.8, 1.0]
normal_map = "assets/textures/stone_normal.png"
roughness = 0.7
```

## Ambient Occlusion

Pre-baked shadows in crevices.

```toml
[entities.material]
albedo = { texture = "wood_color.png" }
ao_map = "wood_ao.png"
```

## Emissive Materials

Self-illuminating surfaces.

```toml
[entities.material]
albedo = [0.1, 0.1, 0.1, 1.0]      # Dark base
emissive = [0.0, 1.0, 0.5]         # Green glow

# Neon sign effect
[entities.material]
albedo = [1.0, 0.0, 0.5, 1.0]
emissive = [5.0, 0.0, 2.5]         # Values > 1 for bloom
```

## Common Material Presets

### Chrome

```toml
[entities.material]
albedo = [0.95, 0.95, 0.95, 1.0]
metallic = 1.0
roughness = 0.05
```

### Gold

```toml
[entities.material]
albedo = [1.0, 0.766, 0.336, 1.0]
metallic = 1.0
roughness = 0.3
```

### Copper

```toml
[entities.material]
albedo = [0.955, 0.637, 0.538, 1.0]
metallic = 1.0
roughness = 0.4
```

### Plastic (Red)

```toml
[entities.material]
albedo = [0.8, 0.1, 0.1, 1.0]
metallic = 0.0
roughness = 0.3
```

### Concrete

```toml
[entities.material]
albedo = [0.5, 0.5, 0.5, 1.0]
metallic = 0.0
roughness = 0.9
```

### Glass (Opaque tinted)

```toml
[entities.material]
albedo = [0.1, 0.3, 0.4, 0.3]
metallic = 0.0
roughness = 0.0
```

## Full Example

```toml
[[entities]]
name = "detailed_floor"
mesh = "plane"

[entities.transform]
scale = 10.0

[entities.material]
albedo = { texture = "assets/textures/floor_albedo.png" }
normal_map = "assets/textures/floor_normal.png"
metallic = 0.0
roughness = { texture = "assets/textures/floor_roughness.png" }
ao_map = "assets/textures/floor_ao.png"
emissive = [0, 0, 0]
```
