# Textures

Texture loading, formats, and configuration.

## Supported Formats

| Format | Extension | Use Case |
|--------|-----------|----------|
| PNG | `.png` | General textures with transparency |
| JPEG | `.jpg`, `.jpeg` | Photos, no transparency needed |
| HDR | `.hdr` | Environment maps, high dynamic range |

## Texture Preloading

Preload textures at scene load for better performance.

```toml
[[textures]]
name = "wood_albedo"           # Reference name
path = "assets/textures/wood_albedo.png"
srgb = true                    # Color space (true for colors)

[[textures]]
name = "wood_normal"
path = "assets/textures/wood_normal.png"
srgb = false                   # Linear for data textures

[[textures]]
name = "environment"
path = "assets/hdri/studio.hdr"
hdr = true                     # HDR format
```

## Texture Options

```toml
[[textures]]
name = "my_texture"
path = "assets/texture.png"
hdr = false                    # HDR format (default: false)
srgb = true                    # sRGB color space (default: true)
mipmap = true                  # Generate mipmaps (optional)
```

### sRGB vs Linear

| Texture Type | sRGB |
|--------------|------|
| Albedo/Color | `true` |
| Normal Map | `false` |
| Metallic | `false` |
| Roughness | `false` |
| AO | `false` |
| Emissive | `true` |
| Height Map | `false` |

## Using Textures in Materials

### Direct Path Reference

```toml
[entities.material]
albedo = { texture = "assets/textures/brick.png" }
normal_map = "assets/textures/brick_normal.png"
```

### Using Preloaded Texture

Reference by the `name` defined in `[[textures]]`:

```toml
[[textures]]
name = "brick_color"
path = "assets/textures/brick.png"
srgb = true

[[entities]]
name = "wall"
[entities.material]
albedo = { texture = "brick_color" }
```

## Texture Types

### Color/Albedo Textures

Base color of the material.

```toml
[[textures]]
name = "metal_albedo"
path = "assets/metal_color.png"
srgb = true                    # Important: color textures need sRGB
```

### Normal Maps

Surface detail without geometry.

```toml
[[textures]]
name = "brick_normal"
path = "assets/brick_normal.png"
srgb = false                   # Linear for accurate normals
```

### Metallic/Roughness

PBR workflow textures.

```toml
[[textures]]
name = "metal_roughness"
path = "assets/metal_roughness.png"
srgb = false                   # Linear for data

[[textures]]
name = "metal_metallic"
path = "assets/metal_metallic.png"
srgb = false
```

### Ambient Occlusion

Pre-baked shadow information.

```toml
[[textures]]
name = "model_ao"
path = "assets/model_ao.png"
srgb = false
```

### HDR Environment Maps

High dynamic range for reflections and IBL.

```toml
[[textures]]
name = "outdoor_hdri"
path = "assets/hdri/outdoor.hdr"
hdr = true
srgb = false
```

## Texture Resolution Guidelines

| Use Case | Recommended Size |
|----------|------------------|
| UI elements | 256-512 |
| Small props | 512-1024 |
| Large objects | 1024-2048 |
| Hero assets | 2048-4096 |
| Environment maps | 1024-4096 |

## Mipmaps

Mipmaps improve quality at distance and performance.

```toml
[[textures]]
name = "floor"
path = "assets/floor.png"
mipmap = true                  # Generate mipmap chain
```

### When to Use Mipmaps

- **Enable**: Most textures viewed at varying distances
- **Disable**: UI textures, pixel-art, textures always viewed close-up

## Complete Texture Setup Example

```toml
# Preload all textures
[[textures]]
name = "brick_albedo"
path = "assets/textures/brick_color.png"
srgb = true
mipmap = true

[[textures]]
name = "brick_normal"
path = "assets/textures/brick_normal.png"
srgb = false
mipmap = true

[[textures]]
name = "brick_roughness"
path = "assets/textures/brick_roughness.png"
srgb = false
mipmap = true

[[textures]]
name = "brick_ao"
path = "assets/textures/brick_ao.png"
srgb = false
mipmap = true

# Use in entity
[[entities]]
name = "brick_wall"
mesh = "cube"

[entities.transform]
position = [0, 2, -5]
scale = [10, 4, 0.5]

[entities.material]
albedo = { texture = "brick_albedo" }
normal_map = "brick_normal"
roughness = { texture = "brick_roughness" }
ao_map = "brick_ao"
metallic = 0.0
```

## Directory Structure

Recommended texture organization:

```
assets/
├── textures/
│   ├── materials/
│   │   ├── brick_albedo.png
│   │   ├── brick_normal.png
│   │   └── brick_roughness.png
│   ├── decals/
│   └── ui/
├── hdri/
│   ├── studio.hdr
│   └── outdoor.hdr
└── models/
```

## Texture Compression

Configure in manifest.toml for build optimization:

```toml
[assets]
compression = 6                # 0 (none) to 9 (max)
```
