# Cameras

Camera definitions for scene viewpoints.

## Basic Camera

```toml
[[cameras]]
name = "main"                 # Required identifier
active = true                 # Is this the active camera
type = "perspective"          # perspective or orthographic
```

## Camera Transform

Position and orientation in world space.

```toml
[cameras.transform]
position = [0, 2, 5]          # Camera position [x, y, z]
target = [0, 0, 0]            # Look-at point [x, y, z]
up = [0, 1, 0]                # Up direction [x, y, z]
```

## Perspective Camera

Standard 3D camera with depth perception.

```toml
[[cameras]]
name = "main"
active = true
type = "perspective"

[cameras.transform]
position = [0, 5, 10]
target = [0, 0, 0]
up = [0, 1, 0]

[cameras.perspective]
fov = 60.0                    # Field of view in degrees
near = 0.1                    # Near clip plane (meters)
far = 1000.0                  # Far clip plane (meters)
aspect = "auto"               # "auto" or fixed ratio (e.g., 1.777)
```

### FOV Guidelines

| FOV | Use Case |
|-----|----------|
| 30-45 | Telephoto, cinematic |
| 60-75 | Standard view |
| 90-110 | Wide angle, FPS games |
| 120+ | Fisheye effect |

### Near/Far Clip Guidelines

- **Near**: Keep as large as possible (0.1-1.0) for depth precision
- **Far**: Keep as small as practical for your scene
- Ratio `far/near` affects depth buffer precision

## Orthographic Camera

No perspective distortion, good for 2D/isometric views.

```toml
[[cameras]]
name = "ortho_cam"
active = false
type = "orthographic"

[cameras.transform]
position = [10, 10, 10]
target = [0, 0, 0]

[cameras.orthographic]
left = -10.0                  # Left boundary
right = 10.0                  # Right boundary
bottom = -10.0                # Bottom boundary
top = 10.0                    # Top boundary
near = 0.1                    # Near clip
far = 100.0                   # Far clip
```

### Orthographic Size Calculation

For centered view:
```toml
# View width = 20 units, height = 15 units
left = -10.0
right = 10.0
bottom = -7.5
top = 7.5
```

## Multiple Cameras

Define multiple cameras and switch between them.

```toml
[[cameras]]
name = "overview"
active = true                 # Default camera
type = "perspective"

[cameras.transform]
position = [0, 20, 20]
target = [0, 0, 0]

[cameras.perspective]
fov = 45.0
near = 1.0
far = 500.0

[[cameras]]
name = "closeup"
active = false
type = "perspective"

[cameras.transform]
position = [0, 2, 3]
target = [0, 1, 0]

[cameras.perspective]
fov = 35.0
near = 0.1
far = 50.0

[[cameras]]
name = "top_down"
active = false
type = "orthographic"

[cameras.transform]
position = [0, 50, 0]
target = [0, 0, 0]
up = [0, 0, -1]              # Look straight down

[cameras.orthographic]
left = -25.0
right = 25.0
bottom = -25.0
top = 25.0
near = 0.1
far = 100.0
```

## Camera Presets

### First-Person View

```toml
[[cameras]]
name = "first_person"
type = "perspective"
active = true

[cameras.transform]
position = [0, 1.7, 0]        # Eye height
target = [0, 1.7, -1]
up = [0, 1, 0]

[cameras.perspective]
fov = 90.0
near = 0.01
far = 500.0
```

### Cinematic Camera

```toml
[[cameras]]
name = "cinematic"
type = "perspective"
active = true

[cameras.transform]
position = [5, 2, 8]
target = [0, 1, 0]

[cameras.perspective]
fov = 35.0                    # Narrow for telephoto look
near = 0.5
far = 200.0
```

### Isometric Camera

```toml
[[cameras]]
name = "isometric"
type = "orthographic"
active = true

[cameras.transform]
position = [10, 10, 10]       # 45-degree angle
target = [0, 0, 0]

[cameras.orthographic]
left = -15.0
right = 15.0
bottom = -10.0
top = 10.0
near = 0.1
far = 50.0
```

### Security Camera (Fixed)

```toml
[[cameras]]
name = "security_cam_1"
type = "perspective"
active = false

[cameras.transform]
position = [8, 4, 8]
target = [0, 0, 0]

[cameras.perspective]
fov = 75.0
near = 0.5
far = 30.0
```

## Aspect Ratio

```toml
# Auto-detect from window size (default)
aspect = "auto"

# Fixed 16:9
aspect = 1.777

# Fixed 4:3
aspect = 1.333

# Fixed 21:9 ultrawide
aspect = 2.333
```
