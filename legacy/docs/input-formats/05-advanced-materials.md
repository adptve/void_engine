# Advanced Materials

Extended PBR material features for glass, fabric, skin, and special effects.

## Transmission (Glass/Liquids)

For transparent/translucent materials with refraction.

```toml
[entities.material]
albedo = [1.0, 1.0, 1.0, 1.0]
metallic = 0.0
roughness = 0.0

[entities.material.transmission]
factor = 1.0                      # 0 = opaque, 1 = fully transmissive
ior = 1.5                         # Index of refraction (glass = 1.5)
thickness = 0.1                   # Volume thickness in meters
attenuation_color = [1, 1, 1]     # Color absorption
attenuation_distance = 1.0        # Distance for full absorption
```

### Common IOR Values

| Material | IOR |
|----------|-----|
| Air | 1.0 |
| Water | 1.33 |
| Glass | 1.5 |
| Crystal | 2.0 |
| Diamond | 2.42 |

### Tinted Glass Example

```toml
[entities.material.transmission]
factor = 0.95
ior = 1.5
thickness = 0.02
attenuation_color = [0.2, 0.8, 0.3]  # Green tint
attenuation_distance = 0.5
```

## Sheen (Fabric/Velvet)

Soft highlight for cloth-like materials.

```toml
[entities.material]
albedo = [0.3, 0.1, 0.4, 1.0]     # Purple velvet
metallic = 0.0
roughness = 0.8

[entities.material.sheen]
color = [1.0, 1.0, 1.0]           # Highlight color
roughness = 0.5                    # Sheen roughness
```

### Silk Example

```toml
[entities.material]
albedo = [0.8, 0.2, 0.3, 1.0]
roughness = 0.4

[entities.material.sheen]
color = [1.0, 0.9, 0.95]
roughness = 0.3
```

## Clearcoat (Car Paint/Lacquer)

Additional specular layer on top of base material.

```toml
[entities.material]
albedo = [0.8, 0.1, 0.1, 1.0]     # Red car paint
metallic = 0.9
roughness = 0.4

[entities.material.clearcoat]
intensity = 1.0                    # 0 = no coat, 1 = full coat
roughness = 0.1                    # Coat roughness (usually low)
normal_map = "clearcoat_normal.png"  # Optional separate normal
```

### Metallic Car Paint

```toml
[entities.material]
albedo = [0.0, 0.3, 0.8, 1.0]
metallic = 0.8
roughness = 0.35

[entities.material.clearcoat]
intensity = 1.0
roughness = 0.05
```

## Anisotropy (Brushed Metal/Hair)

Directional roughness for brushed surfaces.

```toml
[entities.material]
albedo = [0.9, 0.9, 0.9, 1.0]
metallic = 1.0
roughness = 0.3

[entities.material.anisotropy]
strength = 0.8                     # -1 to 1 (0 = isotropic)
rotation = 0.0                     # Radians
direction_map = "aniso_dir.png"    # Optional direction texture
```

### Brushed Stainless Steel

```toml
[entities.material]
albedo = [0.95, 0.95, 0.95, 1.0]
metallic = 1.0
roughness = 0.25

[entities.material.anisotropy]
strength = 0.7
rotation = 1.5708                  # 90 degrees
```

## Subsurface Scattering (Skin/Wax)

Light penetrating and scattering inside material.

```toml
[entities.material]
albedo = [0.9, 0.7, 0.6, 1.0]     # Skin tone
metallic = 0.0
roughness = 0.5

[entities.material.subsurface]
color = [1.0, 0.4, 0.3]           # Scattered light color
radius = [1.0, 0.2, 0.1]          # Scatter distance per channel
factor = 0.5                       # SSS intensity
```

### Candle Wax

```toml
[entities.material]
albedo = [0.95, 0.9, 0.8, 1.0]
roughness = 0.6

[entities.material.subsurface]
color = [1.0, 0.8, 0.6]
radius = [0.5, 0.3, 0.2]
factor = 0.8
```

## Iridescence (Soap Bubbles/Oil)

Thin-film interference effects.

```toml
[entities.material]
albedo = [0.1, 0.1, 0.1, 1.0]
metallic = 0.0
roughness = 0.0

[entities.material.iridescence]
factor = 1.0                       # Effect intensity
ior = 1.3                          # Thin-film IOR
thickness_range = [100, 400]       # Thickness in nanometers
```

### Soap Bubble

```toml
[entities.material]
albedo = [0.05, 0.05, 0.05, 0.1]
roughness = 0.0

[entities.material.transmission]
factor = 0.9
ior = 1.0

[entities.material.iridescence]
factor = 1.0
ior = 1.33
thickness_range = [100, 500]
```

## Combined Example

### Pearlescent Car Paint

```toml
[[entities]]
name = "car_body"
mesh = "assets/models/car.glb"

[entities.material]
albedo = [0.9, 0.9, 0.95, 1.0]
metallic = 0.7
roughness = 0.3

[entities.material.clearcoat]
intensity = 1.0
roughness = 0.05

[entities.material.iridescence]
factor = 0.3
ior = 1.8
thickness_range = [200, 400]
```
