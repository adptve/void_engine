# Animations

Built-in animation types for entity movement and transformation.

## Animation Types

| Type | Description |
|------|-------------|
| `rotate` | Continuous rotation around axis |
| `oscillate` | Back-and-forth movement |
| `orbit` | Circular path around point |
| `pulse` | Scale breathing effect |
| `path` | Follow waypoint path |

## Rotate Animation

Continuous spinning around an axis.

```toml
[entities.animation]
type = "rotate"
axis = [0, 1, 0]              # Y-axis rotation
speed = 1.0                    # Radians per second
```

### Examples

```toml
# Slow turntable
[entities.animation]
type = "rotate"
axis = [0, 1, 0]
speed = 0.5

# Fast spin on X-axis
[entities.animation]
type = "rotate"
axis = [1, 0, 0]
speed = 3.14                   # ~180 deg/sec

# Tumbling (diagonal axis)
[entities.animation]
type = "rotate"
axis = [1, 1, 1]
speed = 2.0
```

## Oscillate Animation

Sinusoidal back-and-forth motion.

```toml
[entities.animation]
type = "oscillate"
axis = [0, 1, 0]              # Direction of movement
amplitude = 0.5                # Distance from center
frequency = 1.0                # Cycles per second
phase = 0.0                    # Start offset (0-1)
rotate = false                 # Move position (false) or rotation (true)
```

### Examples

```toml
# Floating up/down
[entities.animation]
type = "oscillate"
axis = [0, 1, 0]
amplitude = 0.3
frequency = 0.5

# Swaying side-to-side
[entities.animation]
type = "oscillate"
axis = [1, 0, 0]
amplitude = 0.2
frequency = 0.8

# Rocking rotation
[entities.animation]
type = "oscillate"
axis = [0, 0, 1]
amplitude = 15.0               # Degrees when rotate=true
frequency = 0.5
rotate = true
```

## Orbit Animation

Circular motion around a center point.

```toml
[entities.animation]
type = "orbit"
center = [0, 0, 0]            # Point to orbit around
radius = 2.0                   # Distance from center
speed = 1.0                    # Radians per second
axis = [0, 1, 0]              # Orbit plane normal
start_angle = 0.0             # Starting position (radians)
face_center = false           # Always look at center
```

### Examples

```toml
# Planet orbiting origin
[entities.animation]
type = "orbit"
center = [0, 0, 0]
radius = 5.0
speed = 0.5
axis = [0, 1, 0]

# Moon orbiting planet at [3, 0, 0]
[entities.animation]
type = "orbit"
center = [3, 0, 0]
radius = 1.0
speed = 2.0
face_center = true

# Vertical orbit (like ferris wheel)
[entities.animation]
type = "orbit"
center = [0, 2, 0]
radius = 2.0
speed = 0.3
axis = [1, 0, 0]
```

## Pulse Animation

Breathing/pulsing scale effect.

```toml
[entities.animation]
type = "pulse"
min_scale = 0.9               # Minimum scale factor
max_scale = 1.1               # Maximum scale factor
frequency = 1.0               # Pulses per second
phase = 0.0                   # Start offset (0-1)
```

### Examples

```toml
# Gentle breathing
[entities.animation]
type = "pulse"
min_scale = 0.95
max_scale = 1.05
frequency = 0.5

# Heartbeat (faster)
[entities.animation]
type = "pulse"
min_scale = 0.9
max_scale = 1.2
frequency = 1.5

# Dramatic pulse
[entities.animation]
type = "pulse"
min_scale = 0.5
max_scale = 1.5
frequency = 0.3
```

## Path Animation

Follow a series of waypoints.

```toml
[entities.animation]
type = "path"
points = [
    [0, 0, 0],
    [2, 1, 0],
    [4, 0, 2],
    [0, 0, 0]
]
duration = 5.0                # Total time in seconds
loop_animation = true         # Repeat when done
ping_pong = false             # Reverse at end instead of jump
interpolation = "linear"      # Curve type
orient_to_path = false        # Face movement direction
easing = "linear"             # Timing function
```

### Interpolation Modes

| Mode | Description |
|------|-------------|
| `linear` | Straight lines between points |
| `catmull_rom` | Smooth curves through points |
| `bezier` | Bezier curve interpolation |
| `step` | Jump to next point |

### Easing Functions

| Easing | Description |
|--------|-------------|
| `linear` | Constant speed |
| `smooth_step` | Smooth acceleration |
| `ease_in` | Start slow, end fast |
| `ease_out` | Start fast, end slow |
| `ease_in_out` | Slow start and end |
| `bounce` | Bouncing effect |
| `elastic` | Springy overshoot |

### Examples

```toml
# Smooth patrol path
[entities.animation]
type = "path"
points = [[0,1,0], [5,1,0], [5,1,5], [0,1,5]]
duration = 10.0
loop_animation = true
interpolation = "catmull_rom"
easing = "smooth_step"

# Flying creature with look-at
[entities.animation]
type = "path"
points = [[0,3,0], [4,5,2], [8,3,4], [4,4,6], [0,3,0]]
duration = 15.0
loop_animation = true
interpolation = "catmull_rom"
orient_to_path = true

# Bouncing ball
[entities.animation]
type = "path"
points = [[0,2,0], [2,0,0], [4,2,0]]
duration = 2.0
loop_animation = true
ping_pong = true
easing = "bounce"
```

## Phased Animations

Use `phase` to offset multiple objects.

```toml
# Object 1 - starts at beginning
[[entities]]
name = "light1"
[entities.animation]
type = "oscillate"
amplitude = 1.0
frequency = 1.0
phase = 0.0

# Object 2 - offset by 1/3 cycle
[[entities]]
name = "light2"
[entities.animation]
type = "oscillate"
amplitude = 1.0
frequency = 1.0
phase = 0.333

# Object 3 - offset by 2/3 cycle
[[entities]]
name = "light3"
[entities.animation]
type = "oscillate"
amplitude = 1.0
frequency = 1.0
phase = 0.666
```
