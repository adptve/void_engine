# Level of Detail (LOD)

Automatic mesh switching based on distance for performance.

## App-Level LOD Configuration

Configure in `manifest.toml`:

```toml
[app.lod]
enabled = true                 # Enable LOD system
mode = "distance"              # LOD calculation mode
default_mode = "instant"       # Transition mode
cross_fade_duration = 0.2      # Fade time in seconds
dither_pattern = "bayer4"      # Dither pattern for fading
hysteresis = 0.1               # Prevents LOD flickering
update_frequency = 30.0        # LOD update rate (Hz)
```

### LOD Modes

| Mode | Description |
|------|-------------|
| `distance` | Based on camera distance |
| `screen_coverage` | Based on screen pixel coverage |

### Transition Modes

| Mode | Description |
|------|-------------|
| `instant` | Immediate switch |
| `cross_fade` | Smooth blend between levels |

### Dither Patterns

| Pattern | Quality |
|---------|---------|
| `bayer4` | 4x4 dither (fastest) |
| `bayer8` | 8x8 dither (balanced) |
| `bayer16` | 16x16 dither (smoothest) |

## Entity LOD Configuration

Define LOD levels per entity in `scene.toml`:

```toml
[[entities]]
name = "detailed_tree"
mesh = "assets/models/tree_lod0.glb"  # Highest detail

[entities.lod]
bias = 0.0                     # LOD bias (-1 to +1)
fade_transition = true         # Enable fade for this entity
fade_duration = 0.2            # Override global fade time

[[entities.lod.levels]]
distance = 20.0                # Switch at 20 meters
mesh = "assets/models/tree_lod1.glb"

[[entities.lod.levels]]
distance = 50.0
mesh = "assets/models/tree_lod2.glb"

[[entities.lod.levels]]
distance = 100.0
mesh = "hide"                  # Cull at this distance
```

### LOD Bias

Adjust LOD switching threshold:
- `-1.0` = Use higher detail longer
- `0.0` = Normal (default)
- `+1.0` = Switch to lower detail sooner

```toml
# Hero character - keep high detail longer
[entities.lod]
bias = -0.5

# Background prop - aggressive LOD
[entities.lod]
bias = 0.5
```

## Screen Coverage LOD

Use pixel coverage instead of distance:

```toml
[[entities.lod.levels]]
screen_coverage = 0.1          # Switch when <10% of screen
mesh = "low_detail.glb"

[[entities.lod.levels]]
screen_coverage = 0.01         # Switch when <1% of screen
mesh = "hide"
```

## LOD Strategies

### Simple Two-Level

```toml
[[entities]]
name = "rock"
mesh = "rock_high.glb"

[[entities.lod.levels]]
distance = 30.0
mesh = "rock_low.glb"
```

### Three-Level with Culling

```toml
[[entities]]
name = "vegetation"
mesh = "bush_high.glb"

[[entities.lod.levels]]
distance = 15.0
mesh = "bush_medium.glb"

[[entities.lod.levels]]
distance = 40.0
mesh = "bush_low.glb"

[[entities.lod.levels]]
distance = 80.0
mesh = "hide"
```

### Billboard LOD

Switch to 2D at distance:

```toml
[[entities]]
name = "distant_tree"
mesh = "tree_3d.glb"

[[entities.lod.levels]]
distance = 100.0
mesh = "tree_billboard.glb"    # Flat quad with texture
```

## Complete LOD Example

### manifest.toml

```toml
[app.lod]
enabled = true
mode = "distance"
default_mode = "cross_fade"
cross_fade_duration = 0.3
dither_pattern = "bayer8"
hysteresis = 0.15
update_frequency = 20.0
```

### scene.toml

```toml
# High-detail character with smooth transitions
[[entities]]
name = "character"
mesh = "assets/characters/hero_lod0.glb"

[entities.transform]
position = [0, 0, 0]

[entities.lod]
bias = -0.2
fade_transition = true
fade_duration = 0.4

[[entities.lod.levels]]
distance = 10.0
mesh = "assets/characters/hero_lod1.glb"

[[entities.lod.levels]]
distance = 25.0
mesh = "assets/characters/hero_lod2.glb"

# Environment prop with aggressive LOD
[[entities]]
name = "rock_cluster"
mesh = "assets/props/rocks_high.glb"

[entities.lod]
bias = 0.3
fade_transition = false

[[entities.lod.levels]]
distance = 20.0
mesh = "assets/props/rocks_low.glb"

[[entities.lod.levels]]
distance = 60.0
mesh = "hide"

# Static building - no fade needed
[[entities]]
name = "building"
mesh = "assets/buildings/house_lod0.glb"

[entities.lod]
fade_transition = false

[[entities.lod.levels]]
distance = 50.0
mesh = "assets/buildings/house_lod1.glb"

[[entities.lod.levels]]
distance = 150.0
mesh = "assets/buildings/house_lod2.glb"

[[entities.lod.levels]]
distance = 300.0
mesh = "hide"
```

## Performance Guidelines

| Scene Type | LOD Levels | Cull Distance |
|------------|------------|---------------|
| Interior | 1-2 | 50-100m |
| Urban | 3-4 | 200-500m |
| Open world | 4-5 | 500-2000m |

## Disabling LOD

Globally in manifest:
```toml
[app.lod]
enabled = false
```

Per-entity (no `[entities.lod]` section):
```toml
[[entities]]
name = "always_high_detail"
mesh = "model.glb"
# No lod section = always use this mesh
```
