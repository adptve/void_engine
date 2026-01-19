# Shadows

Shadow mapping configuration for realistic lighting.

## Basic Shadow Setup

```toml
[shadows]
enabled = true                 # Master shadow toggle
atlas_size = 4096              # Shadow atlas resolution
max_shadow_distance = 50.0     # Max distance for shadows
shadow_fade_distance = 5.0     # Fade-out distance
```

## Cascade Shadow Maps (CSM)

Split shadow resolution across distance ranges for better quality.

```toml
[shadows.cascades]
count = 3                      # Number of cascades (1-4)
split_scheme = "practical"     # How to divide distance
lambda = 0.5                   # Blend factor for practical scheme
```

### Split Schemes

| Scheme | Description |
|--------|-------------|
| `linear` | Even distance splits |
| `logarithmic` | More detail near camera |
| `practical` | Blend of linear and log (recommended) |

### Lambda Values

- `0.0` = fully linear
- `1.0` = fully logarithmic
- `0.5` = balanced (default)

## Cascade Level Configuration

```toml
[[shadows.cascades.levels]]
resolution = 2048              # Texture resolution
distance = 10.0                # End distance for this cascade
bias = 0.001                   # Depth bias to prevent acne

[[shadows.cascades.levels]]
resolution = 1024
distance = 30.0
bias = 0.002

[[shadows.cascades.levels]]
resolution = 512
distance = 50.0
bias = 0.005
```

### Bias Guidelines

- Lower = less peter-panning, more shadow acne
- Higher = less acne, more peter-panning
- Start with 0.001, adjust per cascade

## Shadow Filtering

Control shadow edge quality.

```toml
[shadows.filtering]
method = "pcf"                 # Filtering algorithm
pcf_samples = 16               # Sample count (4, 8, 16, 32)
pcf_radius = 1.5               # Sample spread
soft_shadows = true            # Enable softness
contact_hardening = false      # PCSS (expensive)
```

### Filter Methods

| Method | Quality | Performance |
|--------|---------|-------------|
| `hard` | Sharp edges | Fastest |
| `pcf` | Soft edges | Good balance |
| `vsm` | Very soft | Medium |
| `esm` | Smooth | Medium |

### PCF Samples

- `4` - Fastest, blocky
- `8` - Good balance
- `16` - Smooth (recommended)
- `32` - Very smooth, expensive

## Complete Shadow Configuration

### High Quality

```toml
[shadows]
enabled = true
atlas_size = 8192
max_shadow_distance = 100.0
shadow_fade_distance = 10.0

[shadows.cascades]
count = 4
split_scheme = "practical"
lambda = 0.7

[[shadows.cascades.levels]]
resolution = 4096
distance = 10.0
bias = 0.0005

[[shadows.cascades.levels]]
resolution = 2048
distance = 25.0
bias = 0.001

[[shadows.cascades.levels]]
resolution = 1024
distance = 50.0
bias = 0.002

[[shadows.cascades.levels]]
resolution = 512
distance = 100.0
bias = 0.005

[shadows.filtering]
method = "pcf"
pcf_samples = 32
pcf_radius = 2.0
soft_shadows = true
contact_hardening = true
```

### Performance Preset

```toml
[shadows]
enabled = true
atlas_size = 2048
max_shadow_distance = 30.0
shadow_fade_distance = 5.0

[shadows.cascades]
count = 2
split_scheme = "practical"
lambda = 0.5

[[shadows.cascades.levels]]
resolution = 1024
distance = 15.0
bias = 0.002

[[shadows.cascades.levels]]
resolution = 512
distance = 30.0
bias = 0.005

[shadows.filtering]
method = "pcf"
pcf_samples = 8
pcf_radius = 1.0
soft_shadows = true
contact_hardening = false
```

### Minimal Shadows

```toml
[shadows]
enabled = true
atlas_size = 1024
max_shadow_distance = 20.0
shadow_fade_distance = 3.0

[shadows.cascades]
count = 1
split_scheme = "linear"

[[shadows.cascades.levels]]
resolution = 1024
distance = 20.0
bias = 0.003

[shadows.filtering]
method = "hard"
```

## Per-Entity Shadow Control

Control which entities cast/receive shadows.

```toml
[[entities]]
name = "floor"
mesh = "plane"

[entities.render_pass]
cast_shadows = false           # Floor doesn't cast
receive_shadows = true         # But receives shadows

[[entities]]
name = "character"
mesh = "model.glb"

[entities.render_pass]
cast_shadows = true            # Character casts shadows
receive_shadows = true

[[entities]]
name = "ghost"
mesh = "sphere"

[entities.render_pass]
cast_shadows = false           # No shadow
receive_shadows = false        # Not affected by shadows
```

## Disabling Shadows

```toml
[shadows]
enabled = false
```

Or per-light:

```toml
[[lights]]
name = "sun"
type = "directional"
[lights.directional]
cast_shadows = false
```
