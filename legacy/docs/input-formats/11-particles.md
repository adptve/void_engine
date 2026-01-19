# Particle Emitters

Particle systems for fire, smoke, sparks, and effects.

## Basic Emitter

```toml
[[particle_emitters]]
name = "sparks"                # Required identifier
position = [0, 0, 0]           # Emitter position
enabled = true                 # Active state
```

## Emission Properties

```toml
[[particle_emitters]]
name = "fire"
position = [0, 0, 0]
emit_rate = 50.0               # Particles per second
max_particles = 1000           # Maximum alive particles
```

## Particle Lifetime

```toml
[[particle_emitters]]
name = "smoke"
lifetime = [2.0, 4.0]          # [min, max] seconds
```

## Movement

```toml
[[particle_emitters]]
name = "fountain"
direction = [0, 1, 0]          # Emission direction (normalized)
spread = 0.3                   # Cone angle (0 = laser, 1 = hemisphere)
speed = [2.0, 4.0]             # [min, max] initial speed
gravity = [0, -9.8, 0]         # Gravity acceleration
```

### Spread Values

| Value | Effect |
|-------|--------|
| 0.0 | Parallel beam |
| 0.1 | Tight cone |
| 0.5 | Wide cone |
| 1.0 | Hemisphere |

## Size

```toml
[[particle_emitters]]
name = "dust"
size = [0.05, 0.15]            # [min, max] particle size
```

## Color

Colors transition from start to end over particle lifetime.

```toml
[[particle_emitters]]
name = "fire"
color_start = [1.0, 0.8, 0.2, 1.0]   # RGBA at birth
color_end = [1.0, 0.2, 0.0, 0.0]     # RGBA at death
```

## Complete Emitter Example

```toml
[[particle_emitters]]
name = "campfire"
position = [0, 0.5, 0]
emit_rate = 80.0
max_particles = 500
lifetime = [0.5, 1.5]
speed = [1.0, 3.0]
size = [0.1, 0.3]
color_start = [1.0, 0.9, 0.3, 1.0]
color_end = [1.0, 0.2, 0.0, 0.0]
gravity = [0, 2.0, 0]          # Rises (negative gravity)
spread = 0.2
direction = [0, 1, 0]
enabled = true
```

## Effect Presets

### Fire

```toml
[[particle_emitters]]
name = "fire"
position = [0, 0, 0]
emit_rate = 100.0
max_particles = 500
lifetime = [0.3, 0.8]
speed = [1.5, 3.0]
size = [0.15, 0.4]
color_start = [1.0, 0.9, 0.2, 1.0]
color_end = [1.0, 0.1, 0.0, 0.0]
gravity = [0, 3.0, 0]
spread = 0.15
direction = [0, 1, 0]
```

### Smoke

```toml
[[particle_emitters]]
name = "smoke"
position = [0, 1.5, 0]
emit_rate = 20.0
max_particles = 200
lifetime = [3.0, 6.0]
speed = [0.3, 0.8]
size = [0.3, 1.0]
color_start = [0.3, 0.3, 0.3, 0.6]
color_end = [0.5, 0.5, 0.5, 0.0]
gravity = [0, 0.5, 0]
spread = 0.3
direction = [0, 1, 0]
```

### Sparks

```toml
[[particle_emitters]]
name = "sparks"
position = [0, 0, 0]
emit_rate = 30.0
max_particles = 100
lifetime = [0.5, 1.5]
speed = [3.0, 8.0]
size = [0.02, 0.05]
color_start = [1.0, 0.8, 0.3, 1.0]
color_end = [1.0, 0.3, 0.0, 0.0]
gravity = [0, -5.0, 0]
spread = 0.8
direction = [0, 1, 0]
```

### Snow

```toml
[[particle_emitters]]
name = "snow"
position = [0, 10, 0]
emit_rate = 50.0
max_particles = 2000
lifetime = [5.0, 10.0]
speed = [0.1, 0.3]
size = [0.03, 0.08]
color_start = [1.0, 1.0, 1.0, 0.8]
color_end = [1.0, 1.0, 1.0, 0.0]
gravity = [0, -0.5, 0]
spread = 1.0
direction = [0, -1, 0]
```

### Rain

```toml
[[particle_emitters]]
name = "rain"
position = [0, 15, 0]
emit_rate = 200.0
max_particles = 3000
lifetime = [1.0, 2.0]
speed = [8.0, 12.0]
size = [0.01, 0.02]
color_start = [0.6, 0.7, 0.8, 0.7]
color_end = [0.6, 0.7, 0.8, 0.0]
gravity = [0, -2.0, 0]
spread = 0.05
direction = [0, -1, 0]
```

### Magic Sparkles

```toml
[[particle_emitters]]
name = "magic"
position = [0, 1, 0]
emit_rate = 40.0
max_particles = 200
lifetime = [1.0, 2.5]
speed = [0.5, 1.5]
size = [0.02, 0.08]
color_start = [0.5, 0.8, 1.0, 1.0]
color_end = [1.0, 0.5, 1.0, 0.0]
gravity = [0, 0.3, 0]
spread = 1.0
direction = [0, 1, 0]
```

### Dust Cloud

```toml
[[particle_emitters]]
name = "dust"
position = [0, 0.1, 0]
emit_rate = 15.0
max_particles = 100
lifetime = [2.0, 4.0]
speed = [0.2, 0.5]
size = [0.1, 0.4]
color_start = [0.6, 0.5, 0.4, 0.4]
color_end = [0.6, 0.5, 0.4, 0.0]
gravity = [0, 0.2, 0]
spread = 0.8
direction = [0, 1, 0]
```

## Multiple Emitters

Combine emitters for complex effects.

```toml
# Fire base
[[particle_emitters]]
name = "torch_fire"
position = [0, 1.5, 0]
emit_rate = 60.0
lifetime = [0.2, 0.5]
color_start = [1.0, 0.8, 0.2, 1.0]
color_end = [1.0, 0.2, 0.0, 0.0]

# Smoke trail
[[particle_emitters]]
name = "torch_smoke"
position = [0, 2.0, 0]
emit_rate = 15.0
lifetime = [2.0, 4.0]
color_start = [0.2, 0.2, 0.2, 0.4]
color_end = [0.3, 0.3, 0.3, 0.0]

# Occasional sparks
[[particle_emitters]]
name = "torch_sparks"
position = [0, 1.5, 0]
emit_rate = 5.0
lifetime = [0.5, 1.0]
speed = [2.0, 5.0]
```
