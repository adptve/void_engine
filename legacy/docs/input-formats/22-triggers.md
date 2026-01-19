# Trigger Volumes

Trigger volumes are invisible collision shapes that fire events when entities enter, stay in, or exit the volume.

## Basic Trigger

```toml
[[triggers]]
name = "death_zone"
shape = "box"
size = [10, 5, 10]
position = [0, -10, 0]
on_enter = "HandleDeathZone"
```

## Trigger Configuration

```toml
[[triggers]]
name = "checkpoint"
shape = "box"                    # box, sphere, capsule, cylinder, mesh
size = [5, 3, 5]                 # Dimensions based on shape
position = [100, 0, 50]
rotation = [0, 0, 0]             # Euler angles in degrees
enabled = true                   # Can be toggled at runtime

# Filtering
layer_mask = ["player"]          # Only these layers trigger
tag_filter = ["saveable"]        # Only entities with these tags

# Events
on_enter = "OnCheckpointEnter"
on_stay = "OnCheckpointStay"     # Called each frame while inside
on_exit = "OnCheckpointExit"

# Behavior
one_shot = false                 # If true, only fires once ever
cooldown = 0.0                   # Seconds before can fire again
```

## Trigger Shapes

### Box

Axis-aligned or oriented bounding box.

```toml
[[triggers]]
name = "room_trigger"
shape = "box"
size = [10, 5, 8]                # Width, height, depth
position = [0, 2.5, 0]
rotation = [0, 45, 0]            # 45 degrees rotated
```

### Sphere

Simple radius-based trigger.

```toml
[[triggers]]
name = "explosion_radius"
shape = "sphere"
radius = 5.0
position = [0, 0, 0]
```

### Capsule

Cylinder with hemispherical caps, good for character-shaped areas.

```toml
[[triggers]]
name = "character_trigger"
shape = "capsule"
radius = 0.5
height = 2.0                     # Total height including caps
position = [0, 1, 0]
```

### Cylinder

Flat-capped cylinder.

```toml
[[triggers]]
name = "pillar_zone"
shape = "cylinder"
radius = 2.0
height = 4.0
position = [0, 2, 0]
```

### Mesh

Custom collision mesh for complex shapes.

```toml
[[triggers]]
name = "complex_zone"
shape = "mesh"
collision_mesh = "assets/collision/cave_trigger.glb"
position = [0, 0, 0]
```

## Shape Parameters

| Shape | Required Parameters |
|-------|---------------------|
| `box` | `size = [x, y, z]` |
| `sphere` | `radius = float` |
| `capsule` | `radius = float`, `height = float` |
| `cylinder` | `radius = float`, `height = float` |
| `mesh` | `collision_mesh = "path.glb"` |

## Filtering

### Layer Mask

Only specific physics layers trigger events:

```toml
[[triggers]]
name = "player_only"
shape = "box"
size = [5, 3, 5]

# Only player layer triggers this
layer_mask = ["player"]
```

### Tag Filter

Only entities with matching tags trigger events:

```toml
[[triggers]]
name = "vehicle_checkpoint"
shape = "box"
size = [10, 5, 10]

# Only entities tagged as vehicles
tag_filter = ["vehicle", "rideable"]
```

### Combined Filtering

Both conditions must match:

```toml
[[triggers]]
name = "player_vehicle_zone"
shape = "box"
size = [20, 5, 20]

# Must be in player layer AND tagged as vehicle
layer_mask = ["player"]
tag_filter = ["vehicle"]
```

## Event Configuration

### Event Names

Map to script functions:

| Event | When Called | Parameters |
|-------|-------------|------------|
| `on_enter` | Entity enters volume | `self: Trigger, other: Entity` |
| `on_stay` | Each frame while inside | `self: Trigger, other: Entity, delta: float` |
| `on_exit` | Entity exits volume | `self: Trigger, other: Entity` |

### One-Shot Triggers

Fire only once, then disable:

```toml
[[triggers]]
name = "first_time_cutscene"
shape = "box"
size = [5, 3, 2]
one_shot = true
on_enter = "PlayIntroCutscene"
```

### Cooldown

Minimum time between activations:

```toml
[[triggers]]
name = "damage_zone"
shape = "box"
size = [5, 1, 5]
cooldown = 1.0                   # 1 second between damage ticks
on_enter = "ApplyDamageOverTime"
on_stay = "ApplyDamageOverTime"
```

## Common Use Cases

### Death Zone

```toml
[[triggers]]
name = "kill_zone"
shape = "box"
size = [100, 10, 100]
position = [0, -50, 0]
layer_mask = ["player", "enemies", "objects"]
on_enter = "InstantDeath"
```

### Checkpoint

```toml
[[triggers]]
name = "checkpoint_01"
shape = "box"
size = [3, 4, 3]
position = [50, 2, 0]
layer_mask = ["player"]
one_shot = true
on_enter = "SaveCheckpoint"

[triggers.data]
checkpoint_id = 1
spawn_position = [50, 0, 2]
spawn_rotation = [0, 180, 0]
```

### Loading Zone

```toml
[[triggers]]
name = "level2_loader"
shape = "box"
size = [5, 4, 1]
position = [100, 2, 0]
layer_mask = ["player"]
on_enter = "BeginLoadLevel"
on_stay = "ShowLoadingProgress"

[triggers.data]
target_level = "level2"
loading_screen = "ui/loading_level2.toml"
```

### Damage Zone (Lava/Poison)

```toml
[[triggers]]
name = "lava_pit"
shape = "box"
size = [10, 2, 10]
position = [0, -1, 0]
layer_mask = ["player", "enemies"]
tag_filter = ["damageable"]
cooldown = 0.5
on_enter = "ApplyFireDamage"
on_stay = "ApplyFireDamage"
on_exit = "StopBurning"

[triggers.data]
damage_per_tick = 10
damage_type = "fire"
apply_dot = true
dot_duration = 3.0
```

### Pickup Area

```toml
[[triggers]]
name = "coin_area"
shape = "sphere"
radius = 5.0
position = [20, 1, 30]
layer_mask = ["player"]
one_shot = true
on_enter = "CollectCoinsInArea"

[triggers.data]
coin_value = 50
effect = "effects/coin_burst.toml"
sound = "sounds/coins_collect.wav"
```

### Cutscene Trigger

```toml
[[triggers]]
name = "boss_intro"
shape = "box"
size = [10, 5, 2]
position = [0, 2.5, -50]
layer_mask = ["player"]
one_shot = true
on_enter = "StartBossCutscene"

[triggers.data]
cutscene = "cutscenes/boss_intro.toml"
disable_player_control = true
skip_allowed = true
```

### Enemy Spawn Trigger

```toml
[[triggers]]
name = "ambush_zone"
shape = "box"
size = [15, 5, 15]
position = [75, 2.5, 25]
layer_mask = ["player"]
one_shot = true
on_enter = "TriggerAmbush"

[triggers.data]
spawn_points = ["spawn_01", "spawn_02", "spawn_03"]
enemy_type = "enemy_soldier"
enemy_count = 5
spawn_delay = 0.5
close_doors = ["door_front", "door_back"]
```

### Door Activation

```toml
[[triggers]]
name = "door_proximity"
shape = "sphere"
radius = 3.0
position = [10, 1.5, 0]
layer_mask = ["player"]
on_enter = "ShowInteractPrompt"
on_exit = "HideInteractPrompt"

[triggers.data]
target_door = "main_door"
prompt_text = "Press E to open"
```

### Portal/Teleporter

```toml
[[triggers]]
name = "portal_a"
shape = "cylinder"
radius = 1.5
height = 3.0
position = [0, 1.5, 0]
layer_mask = ["player", "objects"]
on_enter = "TeleportToDestination"

[triggers.data]
destination = "portal_b"
preserve_velocity = true
effect = "effects/teleport.toml"
sound = "sounds/teleport.wav"
```

## Trigger Data

Custom data passed to event handlers:

```toml
[[triggers]]
name = "custom_trigger"
shape = "box"
size = [5, 3, 5]
on_enter = "HandleCustomTrigger"

# Custom key-value data
[triggers.data]
message = "Welcome to the dungeon"
difficulty_multiplier = 1.5
spawn_enemies = true
required_items = ["torch", "key"]
```

Access in scripts:

```cpp
void HandleCustomTrigger(Trigger* Self, Entity* Other) {
    FString Message = Self->GetData<FString>("message");
    float Difficulty = Self->GetData<float>("difficulty_multiplier");
    TArray<FString> Items = Self->GetData<TArray<FString>>("required_items");
}
```

Blueprint: Use `Get Trigger Data` node with data key.

## Trigger Groups

Manage multiple triggers together:

```toml
# Define trigger group
[trigger_groups.combat_arena]
triggers = ["arena_enter", "arena_wall_n", "arena_wall_s", "arena_wall_e", "arena_wall_w"]
enabled = true

# Triggers in group
[[triggers]]
name = "arena_enter"
group = "combat_arena"
shape = "box"
size = [5, 4, 2]
one_shot = true
on_enter = "StartArenaFight"

[[triggers]]
name = "arena_wall_n"
group = "combat_arena"
shape = "box"
size = [20, 10, 1]
position = [0, 5, 10]
on_enter = "BlockEscape"
```

Disable group in script:

```cpp
TriggerSystem->SetGroupEnabled("combat_arena", false);
```

## Visualization

Debug visualization in editor:

```toml
[[triggers]]
name = "debug_trigger"
shape = "box"
size = [5, 3, 5]

[triggers.debug]
visible = true                   # Show in editor
color = [0, 1, 0, 0.3]          # Green, 30% opacity
show_label = true
show_bounds = true
```

## Runtime Control

### Enable/Disable

```toml
# Initially disabled
[[triggers]]
name = "secret_door_trigger"
enabled = false
```

Enable via script:

```cpp
TriggerSystem->SetTriggerEnabled("secret_door_trigger", true);
```

### Reset One-Shot

```cpp
TriggerSystem->ResetTrigger("checkpoint_01");
```

### Move Trigger

```cpp
TriggerSystem->SetTriggerPosition("moving_hazard", NewPosition);
```

## Complete Example

```toml
# Game level with various triggers

[[triggers]]
name = "level_start"
shape = "box"
size = [10, 5, 10]
position = [0, 2.5, 5]
layer_mask = ["player"]
one_shot = true
on_enter = "OnLevelStart"
[triggers.data]
show_tutorial = true
music_track = "music/level1_ambient.ogg"

[[triggers]]
name = "first_enemy_spawn"
shape = "sphere"
radius = 8.0
position = [30, 0, 0]
layer_mask = ["player"]
one_shot = true
on_enter = "SpawnFirstEnemy"
[triggers.data]
enemy_prefab = "prefabs/enemy_basic.toml"
spawn_point = [35, 0, 5]

[[triggers]]
name = "health_pickup_area"
shape = "sphere"
radius = 2.0
position = [45, 1, 10]
layer_mask = ["player"]
on_enter = "ShowPickupHint"
on_exit = "HidePickupHint"

[[triggers]]
name = "lava_warning"
shape = "box"
size = [20, 3, 2]
position = [60, 1.5, 0]
layer_mask = ["player"]
one_shot = true
on_enter = "ShowLavaWarning"
[triggers.data]
warning_text = "Caution: Lava ahead!"
duration = 3.0

[[triggers]]
name = "lava_damage"
shape = "box"
size = [20, 2, 20]
position = [60, -1, 20]
layer_mask = ["player", "enemies"]
tag_filter = ["damageable"]
cooldown = 0.25
on_stay = "ApplyLavaDamage"
[triggers.data]
damage = 15
damage_type = "fire"

[[triggers]]
name = "boss_arena"
shape = "box"
size = [30, 10, 30]
position = [100, 5, 50]
layer_mask = ["player"]
one_shot = true
on_enter = "StartBossFight"
[triggers.data]
boss_entity = "boss_dragon"
close_exits = true
boss_music = "music/boss_battle.ogg"

[[triggers]]
name = "level_exit"
shape = "box"
size = [3, 4, 3]
position = [120, 2, 80]
layer_mask = ["player"]
enabled = false                  # Enabled after boss defeat
on_enter = "LoadNextLevel"
[triggers.data]
next_level = "level2"
transition = "fade_black"
```
