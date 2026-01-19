# Picking (Object Selection)

Configure mouse/pointer object selection.

## Scene Picking Configuration

```toml
[picking]
enabled = true                 # Enable picking system
method = "gpu"                 # Picking method
max_distance = 100.0           # Maximum pick distance
layer_mask = ["world"]         # Layers to pick from
```

### Picking Methods

| Method | Description | Performance |
|--------|-------------|-------------|
| `cpu` | Ray-mesh intersection on CPU | Accurate, slower |
| `gpu` | Color ID render pass | Fast, requires readback |
| `hybrid` | GPU cull, CPU refine | Balanced |

## GPU Picking Options

```toml
[picking.gpu]
buffer_size = [256, 256]       # Pick buffer resolution
readback_delay = 1             # Frames before reading result
```

### Buffer Size Guidelines

| Size | Use Case |
|------|----------|
| `[128, 128]` | Performance priority |
| `[256, 256]` | Balanced (default) |
| `[512, 512]` | Precision priority |

## Entity Pickable Component

Make specific entities selectable.

```toml
[[entities]]
name = "button"
mesh = "cube"

[entities.pickable]
enabled = true                 # Can be picked
priority = 0                   # Pick priority (higher wins overlap)
bounds = "mesh"                # Bounds type for picking
highlight_on_hover = true      # Visual feedback on hover
group = "interactive"          # Logical grouping
```

### Bounds Types

| Type | Description |
|------|-------------|
| `mesh` | Exact mesh geometry (most accurate) |
| `aabb` | Axis-aligned bounding box (faster) |
| `sphere` | Bounding sphere (fastest) |
| `custom` | Custom collision shape |

### Priority

When objects overlap, higher priority wins:

```toml
# UI elements get priority
[[entities]]
name = "ui_button"
[entities.pickable]
priority = 100

# World objects lower priority
[[entities]]
name = "floor"
[entities.pickable]
priority = 0
```

## Input Events

Respond to pointer interactions.

```toml
[[entities]]
name = "interactive_object"
mesh = "cube"

[entities.pickable]
enabled = true

[entities.input_events]
on_click = "handle_click"
on_double_click = "handle_double_click"
on_pointer_enter = "handle_hover_start"
on_pointer_exit = "handle_hover_end"
on_drag_start = "handle_drag_start"
on_drag = "handle_dragging"
on_drag_end = "handle_drag_end"
on_context_menu = "handle_right_click"
```

### Event Reference

| Event | Trigger |
|-------|---------|
| `on_click` | Single left click |
| `on_double_click` | Double left click |
| `on_pointer_enter` | Cursor enters bounds |
| `on_pointer_exit` | Cursor leaves bounds |
| `on_drag_start` | Begin drag operation |
| `on_drag` | During drag (each frame) |
| `on_drag_end` | Release after drag |
| `on_context_menu` | Right click |

## Layer Filtering

Pick only from specific layers.

```toml
[picking]
enabled = true
layer_mask = ["world", "interactive"]  # Only these layers

[[entities]]
name = "pickable_cube"
layer = "interactive"          # In pickable layer

[entities.pickable]
enabled = true

[[entities]]
name = "background"
layer = "background"           # Not in layer_mask, won't be picked
```

## Complete Picking Setup

```toml
# Scene-level config
[picking]
enabled = true
method = "gpu"
max_distance = 50.0
layer_mask = ["world", "ui"]

[picking.gpu]
buffer_size = [256, 256]
readback_delay = 1

# Non-pickable floor
[[entities]]
name = "floor"
mesh = "plane"
layer = "world"

[entities.pickable]
enabled = false

# Pickable interactive object
[[entities]]
name = "button_3d"
mesh = "cube"
layer = "ui"

[entities.transform]
position = [0, 1, 0]

[entities.pickable]
enabled = true
priority = 10
bounds = "aabb"
highlight_on_hover = true
group = "buttons"

[entities.input_events]
on_click = "activate_button"
on_pointer_enter = "highlight_button"
on_pointer_exit = "unhighlight_button"

# Draggable object
[[entities]]
name = "draggable_sphere"
mesh = "sphere"
layer = "world"

[entities.transform]
position = [2, 1, 0]

[entities.pickable]
enabled = true
priority = 5
bounds = "sphere"

[entities.input_events]
on_drag_start = "grab_object"
on_drag = "move_object"
on_drag_end = "release_object"
```

## Picking Groups

Organize pickable objects logically.

```toml
[[entities]]
name = "chest"
[entities.pickable]
group = "containers"

[[entities]]
name = "door"
[entities.pickable]
group = "interactables"

[[entities]]
name = "npc"
[entities.pickable]
group = "characters"
```

## Disabling Picking

Globally:
```toml
[picking]
enabled = false
```

Per-entity:
```toml
[entities.pickable]
enabled = false
```
