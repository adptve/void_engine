# Environment

Sky, ambient lighting, and atmospheric effects.

## Basic Environment

```toml
[environment]
ambient_intensity = 0.25       # Global ambient light level
light_direction = [0.5, -0.7, 0.5]  # Default light direction
light_color = [1.0, 0.95, 0.85]     # Default light color
light_intensity = 3.0               # Default light intensity
```

## Environment Map (HDR)

Use an HDR image for reflections and image-based lighting.

```toml
[environment]
environment_map = "assets/hdri/studio.hdr"
ambient_intensity = 0.3
```

### Environment Map Usage

- Provides realistic reflections on metallic surfaces
- Adds image-based ambient lighting
- Supported format: `.hdr` (Radiance HDR)

## Procedural Sky

Generate sky without textures.

```toml
[environment.sky]
zenith_color = [0.1, 0.3, 0.6]     # Sky color overhead
horizon_color = [0.5, 0.7, 0.9]   # Sky color at horizon
ground_color = [0.15, 0.12, 0.1]  # Color below horizon
```

### Sky Color Presets

#### Clear Day

```toml
[environment.sky]
zenith_color = [0.1, 0.4, 0.8]
horizon_color = [0.6, 0.8, 1.0]
ground_color = [0.2, 0.18, 0.15]
```

#### Sunset

```toml
[environment.sky]
zenith_color = [0.1, 0.15, 0.4]
horizon_color = [1.0, 0.5, 0.2]
ground_color = [0.15, 0.1, 0.08]
```

#### Night

```toml
[environment.sky]
zenith_color = [0.01, 0.01, 0.03]
horizon_color = [0.05, 0.05, 0.1]
ground_color = [0.02, 0.02, 0.02]
```

#### Overcast

```toml
[environment.sky]
zenith_color = [0.5, 0.5, 0.55]
horizon_color = [0.6, 0.6, 0.65]
ground_color = [0.3, 0.28, 0.25]
```

## Sun Disc

Configure visible sun in the sky.

```toml
[environment.sky]
sun_size = 0.03                # Angular size (0.01 - 0.1)
sun_intensity = 50.0           # Brightness multiplier
sun_falloff = 3.0              # Glow falloff rate
```

### Sun Presets

```toml
# Realistic sun
sun_size = 0.02
sun_intensity = 100.0
sun_falloff = 5.0

# Large stylized sun
sun_size = 0.08
sun_intensity = 30.0
sun_falloff = 2.0

# No visible sun disc
sun_intensity = 0.0
```

## Clouds

Basic cloud layer.

```toml
[environment.sky]
cloud_coverage = 0.3           # 0.0 = clear, 1.0 = overcast
```

## Fog

Distance-based atmospheric fog.

```toml
[environment.sky]
fog_density = 0.02             # Fog thickness (0.0 = none)
fog_color = [0.7, 0.75, 0.8]   # Fog color (optional, uses horizon)
```

### Fog Presets

```toml
# Light haze
fog_density = 0.005
fog_color = [0.8, 0.85, 0.9]

# Morning mist
fog_density = 0.03
fog_color = [0.9, 0.9, 0.95]

# Dense fog
fog_density = 0.1
fog_color = [0.6, 0.6, 0.65]
```

## Complete Environment Examples

### Outdoor Day Scene

```toml
[environment]
ambient_intensity = 0.2
light_direction = [0.5, -0.8, 0.3]
light_color = [1.0, 0.98, 0.95]
light_intensity = 4.0

[environment.sky]
zenith_color = [0.15, 0.4, 0.8]
horizon_color = [0.6, 0.8, 1.0]
ground_color = [0.2, 0.18, 0.15]
sun_size = 0.025
sun_intensity = 80.0
sun_falloff = 4.0
cloud_coverage = 0.2
fog_density = 0.002
```

### Studio Environment

```toml
[environment]
environment_map = "assets/hdri/studio_soft.hdr"
ambient_intensity = 0.5
light_direction = [0.3, -0.5, 0.5]
light_color = [1.0, 1.0, 1.0]
light_intensity = 2.0

[environment.sky]
zenith_color = [0.2, 0.2, 0.2]
horizon_color = [0.3, 0.3, 0.3]
ground_color = [0.15, 0.15, 0.15]
sun_intensity = 0.0
```

### Alien World

```toml
[environment]
ambient_intensity = 0.3

[environment.sky]
zenith_color = [0.4, 0.1, 0.5]
horizon_color = [0.8, 0.3, 0.2]
ground_color = [0.1, 0.05, 0.1]
sun_size = 0.1
sun_intensity = 40.0
sun_falloff = 2.0
fog_density = 0.01
fog_color = [0.5, 0.2, 0.3]
```

### Indoor Scene (No Sky)

```toml
[environment]
ambient_intensity = 0.1
light_intensity = 0.0          # Disable default light

[environment.sky]
zenith_color = [0.05, 0.05, 0.05]
horizon_color = [0.05, 0.05, 0.05]
ground_color = [0.03, 0.03, 0.03]
sun_intensity = 0.0
```
