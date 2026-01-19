# Input Configuration

Camera controls and key bindings.

## Camera Input

Configure mouse and keyboard camera controls.

```toml
[input.camera]
orbit_button = "left"          # Mouse button for orbit
pan_button = "middle"          # Mouse button for pan
zoom_scroll = true             # Enable scroll wheel zoom
```

### Mouse Buttons

| Value | Button |
|-------|--------|
| `"left"` | Left mouse button |
| `"right"` | Right mouse button |
| `"middle"` | Middle mouse button / wheel click |

## Sensitivity Settings

```toml
[input.camera]
orbit_sensitivity = 0.005      # Rotation speed
pan_sensitivity = 0.01         # Pan speed
zoom_sensitivity = 0.1         # Zoom speed
```

### Sensitivity Guidelines

| Setting | Low | Default | High |
|---------|-----|---------|------|
| orbit | 0.002 | 0.005 | 0.01 |
| pan | 0.005 | 0.01 | 0.02 |
| zoom | 0.05 | 0.1 | 0.2 |

## Inversion

```toml
[input.camera]
invert_y = false               # Invert vertical look
invert_x = false               # Invert horizontal look
```

## Zoom Limits

```toml
[input.camera]
min_distance = 0.5             # Closest zoom distance
max_distance = 50.0            # Farthest zoom distance
```

## Complete Camera Config

```toml
[input.camera]
orbit_button = "left"
pan_button = "middle"
zoom_scroll = true
orbit_sensitivity = 0.005
pan_sensitivity = 0.01
zoom_sensitivity = 0.1
invert_y = false
invert_x = false
min_distance = 1.0
max_distance = 100.0
```

## Key Bindings

Map keys to actions.

```toml
[input.bindings]
next_material = "Tab"
prev_material = "Shift+Tab"
toggle_wireframe = "W"
reset_camera = "R"
take_screenshot = "F12"
toggle_fullscreen = "F11"
```

### Key Format

Single key:
```toml
action = "Space"
action = "Enter"
action = "Escape"
action = "Tab"
action = "A"
action = "F1"
```

Key with modifier:
```toml
action = "Shift+S"
action = "Ctrl+Z"
action = "Alt+Enter"
action = "Ctrl+Shift+P"
```

### Available Keys

**Letters**: `A` through `Z`

**Numbers**: `0` through `9`

**Function Keys**: `F1` through `F12`

**Special Keys**:
| Key | Name |
|-----|------|
| Space | `Space` |
| Enter | `Enter` |
| Escape | `Escape` |
| Tab | `Tab` |
| Backspace | `Backspace` |
| Delete | `Delete` |
| Insert | `Insert` |
| Home | `Home` |
| End | `End` |
| PageUp | `PageUp` |
| PageDown | `PageDown` |

**Arrow Keys**: `Up`, `Down`, `Left`, `Right`

**Modifiers**: `Shift`, `Ctrl`, `Alt`

## Input Presets

### Model Viewer

```toml
[input.camera]
orbit_button = "left"
pan_button = "middle"
zoom_scroll = true
orbit_sensitivity = 0.005
pan_sensitivity = 0.01
zoom_sensitivity = 0.1
min_distance = 0.5
max_distance = 20.0

[input.bindings]
reset_view = "R"
toggle_wireframe = "W"
toggle_grid = "G"
next_material = "Tab"
screenshot = "F12"
```

### First Person

```toml
[input.camera]
orbit_button = "right"         # Hold right to look
pan_button = "middle"
zoom_scroll = false
orbit_sensitivity = 0.003
invert_y = false

[input.bindings]
forward = "W"
backward = "S"
left = "A"
right = "D"
jump = "Space"
crouch = "Ctrl"
sprint = "Shift"
```

### Editor Style

```toml
[input.camera]
orbit_button = "middle"        # Alt+middle to orbit
pan_button = "middle"          # Shift+middle to pan
zoom_scroll = true
orbit_sensitivity = 0.004
pan_sensitivity = 0.008
zoom_sensitivity = 0.15
min_distance = 0.1
max_distance = 1000.0

[input.bindings]
frame_selected = "F"
toggle_orthographic = "Numpad5"
view_front = "Numpad1"
view_right = "Numpad3"
view_top = "Numpad7"
undo = "Ctrl+Z"
redo = "Ctrl+Shift+Z"
delete = "Delete"
duplicate = "Ctrl+D"
```

## Full Example

```toml
[input]
[input.camera]
orbit_button = "left"
pan_button = "right"
zoom_scroll = true
orbit_sensitivity = 0.005
pan_sensitivity = 0.01
zoom_sensitivity = 0.1
invert_y = false
invert_x = false
min_distance = 1.0
max_distance = 50.0

[input.bindings]
# Camera
reset_camera = "Home"
toggle_orbit = "O"

# Materials
next_material = "Tab"
prev_material = "Shift+Tab"

# Debug
toggle_wireframe = "1"
toggle_normals = "2"
toggle_bounds = "3"
toggle_stats = "F3"

# General
screenshot = "F12"
fullscreen = "F11"
quit = "Escape"
```
