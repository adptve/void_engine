# Lights

Lighting system for scene illumination.

## Light Types

| Type | Description |
|------|-------------|
| `directional` | Sun/moon - parallel rays from infinity |
| `point` | Light bulb - omnidirectional from point |
| `spot` | Flashlight - cone-shaped beam |
| `area` | Soft rectangular light source |

## Directional Light

Simulates distant light source like the sun.

```toml
[[lights]]
name = "sun"
type = "directional"
enabled = true

[lights.directional]
direction = [0.5, -0.7, 0.5]  # Vector pointing toward light source
color = [1.0, 0.95, 0.9]      # Warm white
intensity = 3.0                # Brightness multiplier
cast_shadows = true
```

### Direction Guidelines

- Points FROM the light toward origin
- `[0, -1, 0]` = straight down (noon)
- `[0.5, -0.3, 0]` = low angle (sunrise/sunset)
- Doesn't need to be normalized

### Sunlight Presets

```toml
# Midday sun
[lights.directional]
direction = [0.2, -0.9, 0.1]
color = [1.0, 1.0, 0.98]
intensity = 5.0

# Golden hour
[lights.directional]
direction = [0.7, -0.2, 0.3]
color = [1.0, 0.8, 0.5]
intensity = 3.0

# Moonlight
[lights.directional]
direction = [0.3, -0.5, 0.4]
color = [0.7, 0.8, 1.0]
intensity = 0.3
```

## Point Light

Omnidirectional light from a single point.

```toml
[[lights]]
name = "lamp"
type = "point"
enabled = true

[lights.point]
position = [0, 3, 0]          # World position
color = [1.0, 0.9, 0.7]       # Warm white
intensity = 100.0              # Brightness (in lumens-like units)
range = 10.0                   # Maximum reach
cast_shadows = false           # Expensive for points

[lights.point.attenuation]
constant = 1.0                 # Base attenuation
linear = 0.09                  # Distance falloff
quadratic = 0.032              # Distance^2 falloff
```

### Attenuation Formula

```
attenuation = 1.0 / (constant + linear*d + quadratic*d²)
```

### Point Light Presets

```toml
# Candle
[lights.point]
position = [0, 1, 0]
color = [1.0, 0.6, 0.2]
intensity = 20.0
range = 3.0

# Light bulb (60W equivalent)
[lights.point]
position = [0, 2.5, 0]
color = [1.0, 0.95, 0.85]
intensity = 100.0
range = 8.0

# Neon accent
[lights.point]
position = [2, 1, 0]
color = [0.0, 1.0, 0.8]
intensity = 50.0
range = 5.0
```

## Spot Light

Cone-shaped beam like a flashlight or stage light.

```toml
[[lights]]
name = "spotlight"
type = "spot"
enabled = true

[lights.spot]
position = [0, 5, 0]          # World position
direction = [0, -1, 0]        # Beam direction
color = [1.0, 1.0, 1.0]       # White
intensity = 200.0              # Brightness
range = 15.0                   # Maximum reach
inner_angle = 20.0            # Full intensity cone (degrees)
outer_angle = 35.0            # Falloff cone (degrees)
cast_shadows = true
```

### Angle Guidelines

- **inner_angle**: Full brightness cone
- **outer_angle**: Edge of light (soft falloff between inner and outer)
- Larger difference = softer edge

### Spot Light Presets

```toml
# Stage spotlight
[lights.spot]
position = [0, 8, 5]
direction = [0, -0.8, -0.5]
color = [1.0, 1.0, 1.0]
intensity = 500.0
range = 20.0
inner_angle = 10.0
outer_angle = 15.0
cast_shadows = true

# Flashlight
[lights.spot]
position = [0, 1.5, 0]
direction = [0, 0, -1]
color = [1.0, 0.98, 0.95]
intensity = 150.0
range = 25.0
inner_angle = 15.0
outer_angle = 30.0

# Car headlight
[lights.spot]
position = [0, 0.5, 2]
direction = [0, -0.1, 1]
color = [1.0, 1.0, 1.0]
intensity = 300.0
range = 50.0
inner_angle = 25.0
outer_angle = 45.0
```

## Multiple Lights

```toml
# Main sun
[[lights]]
name = "sun"
type = "directional"
[lights.directional]
direction = [0.5, -0.7, 0.3]
color = [1.0, 0.95, 0.85]
intensity = 3.0
cast_shadows = true

# Fill light (softer, from opposite side)
[[lights]]
name = "fill"
type = "directional"
[lights.directional]
direction = [-0.5, -0.3, -0.3]
color = [0.7, 0.8, 1.0]
intensity = 0.5
cast_shadows = false

# Accent point light
[[lights]]
name = "accent"
type = "point"
[lights.point]
position = [3, 2, 0]
color = [1.0, 0.5, 0.0]
intensity = 50.0
range = 5.0
```

## Disabling Lights

```toml
[[lights]]
name = "optional_light"
type = "point"
enabled = false               # Defined but not active

[lights.point]
position = [0, 2, 0]
intensity = 100.0
```
