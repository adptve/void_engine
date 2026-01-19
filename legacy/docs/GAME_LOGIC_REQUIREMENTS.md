# Game Logic & Scripting System Requirements

This document outlines the features required for the Void GUI renderer to support full game development, including triggers, combat, inventory, physics, and scripting.

---

## Table of Contents

1. [Documentation Format Preference](#documentation-format-preference)
2. [Scripting System](#scripting-system)
3. [Trigger Volumes & Collision Events](#trigger-volumes--collision-events)
4. [Physics System](#physics-system)
5. [Combat & Damage System](#combat--damage-system)
6. [Inventory & Pickup System](#inventory--pickup-system)
7. [Audio System](#audio-system)
8. [Game State & Variables](#game-state--variables)
9. [UI/HUD System](#uihud-system)
10. [AI & Navigation](#ai--navigation)
11. [Networking (Optional)](#networking-optional)
12. [Editor Integration Notes](#editor-integration-notes)

---

## Documentation Format Preference

For each feature, please provide documentation in the same format as existing `docs/input-formats/*.md` files:

1. **Markdown file** per feature (e.g., `21-scripting.md`, `22-triggers.md`, etc.)
2. **TOML examples** showing configuration syntax
3. **Tables** for enums/options with descriptions
4. **Complete examples** at the end of each document

For the scripting language itself, please provide:
- **Language reference** (syntax, data types, operators)
- **Built-in functions** reference
- **Event handler signatures** (parameters passed to callbacks)
- **API reference** for engine access (spawn entities, play sounds, etc.)

---

## Scripting System

### Requirements

The editor needs to know:

1. **What scripting language is `.vs`?** (custom? Lua-based? WASM?)
2. **How are scripts attached to entities?**
3. **What is the lifecycle?** (init, update, destroy)
4. **How do TOML event names map to script functions?**

### Suggested TOML Structure

```toml
# Per-entity script attachment
[[entities]]
name = "enemy"
mesh = "robot.glb"

[entities.script]
source = "scripts/enemy_ai.vs"      # Script file
class = "EnemyController"            # Class/module name (if applicable)
enabled = true

# Or inline for simple cases
[entities.script]
inline = """
on_hit(damage, source) {
  health -= damage
  if health <= 0 { destroy_self() }
}
"""
```

### Required Documentation

| Document | Contents |
|----------|----------|
| `scripting-overview.md` | Language basics, file structure, execution model |
| `scripting-api.md` | Engine API (spawn, destroy, get_entity, etc.) |
| `scripting-events.md` | All event signatures with parameters |
| `scripting-examples.md` | Common patterns (player controller, enemy AI, pickups) |

### Event Handler Signatures Needed

```
// We need to know what parameters are passed to each event

on_click(entity_id, hit_point, hit_normal)
on_collision_enter(self, other, contact_point, contact_normal)
on_trigger_enter(self, other)
on_damage(amount, damage_type, source_entity)
on_death(killer_entity)
on_pickup(picker_entity, item_data)
on_key_pressed(key, modifiers)
on_update(delta_time)
on_timer(timer_id)
```

---

## Trigger Volumes & Collision Events

### Requirements

Invisible volumes that fire events when entities enter/exit/stay.

### Suggested TOML Structure

```toml
[[triggers]]
name = "death_zone"
shape = "box"                        # box, sphere, capsule, mesh
size = [10, 5, 10]                   # Dimensions for box
position = [0, -10, 0]
rotation = [0, 0, 0]

# Filtering
layer_mask = ["player", "enemies"]   # Only these layers trigger
tag_filter = ["damageable"]          # Only entities with these tags

# Events
on_enter = "handle_death_zone_enter"
on_exit = "handle_death_zone_exit"
on_stay = "handle_death_zone_stay"   # Called each frame while inside

# Optional: one-shot vs repeating
one_shot = false                     # If true, only fires once ever
cooldown = 0.0                       # Seconds before can fire again
```

### Trigger Shapes Needed

| Shape | Parameters |
|-------|------------|
| `box` | `size = [x, y, z]` |
| `sphere` | `radius = float` |
| `capsule` | `radius = float, height = float` |
| `cylinder` | `radius = float, height = float` |
| `mesh` | `collision_mesh = "path.glb"` |

### Use Cases to Support

- Death zones (fall off map)
- Checkpoints (save progress)
- Loading zones (stream next area)
- Damage zones (lava, poison)
- Pickup areas (coins in a region)
- Cutscene triggers
- Enemy spawn triggers
- Door/portal activation

---

## Physics System

### Requirements

Rigidbody physics for dynamic objects.

### Suggested TOML Structure

```toml
[[entities]]
name = "crate"
mesh = "crate.glb"

[entities.physics]
enabled = true
body_type = "dynamic"                # static, dynamic, kinematic
mass = 10.0                          # kg
gravity_scale = 1.0                  # Multiplier for gravity

# Collision shape (can differ from visual mesh)
[entities.physics.collider]
shape = "box"
size = [1, 1, 1]
offset = [0, 0, 0]                   # Offset from entity origin
is_trigger = false                   # If true, no physical response

# Material properties
[entities.physics.material]
friction = 0.5
restitution = 0.3                    # Bounciness (0-1)
density = 1.0

# Constraints
[entities.physics.constraints]
freeze_position = [false, false, false]  # X, Y, Z
freeze_rotation = [false, true, false]   # X, Y, Z

# Collision events
[entities.physics.events]
on_collision_enter = "handle_collision"
on_collision_exit = "handle_collision_end"
```

### Body Types

| Type | Description |
|------|-------------|
| `static` | Never moves, infinite mass (walls, floors) |
| `dynamic` | Fully simulated (crates, ragdolls) |
| `kinematic` | Moved by code, affects dynamic bodies (moving platforms) |

### Collision Layers

```toml
[physics]
layers = ["default", "player", "enemies", "projectiles", "triggers"]

# Collision matrix - which layers collide with which
[physics.collision_matrix]
player = ["default", "enemies", "projectiles", "triggers"]
enemies = ["default", "player", "projectiles"]
projectiles = ["default", "enemies"]
triggers = ["player", "enemies"]
```

---

## Combat & Damage System

### Requirements

Health, damage, weapons, projectiles.

### Suggested TOML Structure

```toml
# Health component
[[entities]]
name = "enemy_soldier"

[entities.health]
max_health = 100
current_health = 100
invulnerable = false
invulnerability_time = 0.5          # Seconds of i-frames after hit

# Damage type resistances (multipliers)
[entities.health.resistances]
physical = 1.0
fire = 0.5                           # Takes half fire damage
ice = 1.5                            # Takes 50% more ice damage
poison = 0.0                         # Immune to poison

# Events
[entities.health.events]
on_damage = "handle_damage"          # (amount, type, source)
on_heal = "handle_heal"
on_death = "handle_death"

# Optional: damage numbers, hit effects
[entities.health.feedback]
show_damage_numbers = true
hit_flash_color = [1, 0, 0, 0.5]
hit_flash_duration = 0.1
```

### Weapon/Projectile System

```toml
# Weapon definition
[[weapons]]
name = "laser_pistol"
type = "hitscan"                     # hitscan, projectile, melee

[weapons.hitscan]
damage = 25
damage_type = "energy"
range = 100
fire_rate = 5.0                      # Shots per second
spread = 2.0                         # Degrees of random spread
penetration = 0                      # How many targets to pierce

[weapons.effects]
muzzle_flash = "effects/muzzle_flash.toml"
hit_effect = "effects/laser_hit.toml"
tracer = "effects/laser_tracer.toml"
sound_fire = "sounds/laser_fire.wav"
sound_hit = "sounds/laser_hit.wav"

# Projectile weapon example
[[weapons]]
name = "rocket_launcher"
type = "projectile"

[weapons.projectile]
prefab = "prefabs/rocket.toml"       # Projectile entity to spawn
speed = 50.0
damage = 100
damage_type = "explosive"
splash_radius = 5.0
splash_damage_falloff = "linear"     # linear, none, exponential
gravity_scale = 0.1
lifetime = 10.0                      # Seconds before auto-destroy

# Melee weapon example
[[weapons]]
name = "sword"
type = "melee"

[weapons.melee]
damage = 40
damage_type = "physical"
range = 2.0
arc = 90                             # Degrees of swing arc
swing_time = 0.3                     # Seconds
combo_window = 0.5                   # Seconds to chain next attack
```

### Damage Types Enum (Suggested)

| Type | Typical Use |
|------|-------------|
| `physical` | Bullets, melee, falls |
| `fire` | Flames, explosions |
| `ice` | Freeze effects |
| `electric` | Shock, stun |
| `poison` | DOT effects |
| `energy` | Lasers, plasma |
| `true` | Ignores all resistances |

---

## Inventory & Pickup System

### Requirements

Collectible items, inventory management, equipment.

### Suggested TOML Structure

```toml
# Pickup item in world
[[entities]]
name = "health_pack"
mesh = "health_pack.glb"

[entities.pickup]
item_id = "health_small"             # Reference to item definition
quantity = 1
auto_collect = true                  # Collect on touch vs require interact
collect_radius = 1.0                 # For auto_collect
respawn = true
respawn_time = 30.0                  # Seconds

[entities.pickup.events]
on_collect = "handle_health_pickup"  # (collector, item_id, quantity)
on_fail_collect = "handle_inventory_full"

# Visual feedback
[entities.pickup.feedback]
bob_animation = true                 # Float up and down
bob_height = 0.2
bob_speed = 2.0
rotate = true
rotate_speed = 90                    # Degrees per second
glow_color = [0, 1, 0, 1]
collect_effect = "effects/pickup_sparkle.toml"
collect_sound = "sounds/pickup.wav"
```

### Item Definitions

```toml
# Separate file: items/health_small.toml
[item]
id = "health_small"
name = "Small Health Pack"
description = "Restores 25 health"
icon = "ui/icons/health_small.png"
category = "consumable"
stackable = true
max_stack = 99
rarity = "common"

[item.use_effect]
type = "heal"
amount = 25

# Weapon item
[item]
id = "laser_pistol"
name = "Laser Pistol"
category = "weapon"
stackable = false
weapon_ref = "weapons/laser_pistol"  # Links to weapon definition
equip_slot = "primary"

# Armor item
[item]
id = "steel_helmet"
name = "Steel Helmet"
category = "armor"
equip_slot = "head"

[item.stats]
defense = 10
fire_resistance = 0.1
```

### Inventory Component

```toml
[[entities]]
name = "player"

[entities.inventory]
slots = 20                           # Total inventory size
equipment_slots = ["head", "chest", "legs", "feet", "primary", "secondary"]
drop_on_death = false

[entities.inventory.starting_items]
items = [
  { id = "health_small", quantity = 3 },
  { id = "laser_pistol", quantity = 1 }
]
```

---

## Audio System

### Requirements

Sound effects, music, spatial audio.

### Suggested TOML Structure

```toml
# Sound effect on entity
[[entities]]
name = "torch"

[entities.audio]
# Ambient/looping sound
[entities.audio.ambient]
clip = "sounds/fire_crackle.wav"
volume = 0.8
loop = true
spatial = true                       # 3D positioned audio
min_distance = 1.0                   # Full volume within this
max_distance = 20.0                  # Silent beyond this
rolloff = "logarithmic"              # linear, logarithmic, custom

# One-shot sounds (triggered by events)
[entities.audio.sounds]
ignite = "sounds/fire_ignite.wav"
extinguish = "sounds/fire_extinguish.wav"
```

### Scene-Level Audio

```toml
[audio]
master_volume = 1.0
music_volume = 0.7
sfx_volume = 1.0
ambient_volume = 0.8

[audio.music]
track = "music/exploration.ogg"
loop = true
fade_in = 2.0                        # Seconds
crossfade = 3.0                      # When switching tracks

[audio.reverb_zones]
# Define areas with different reverb
[[audio.reverb_zones]]
name = "cave_reverb"
bounds = { min = [-10, 0, -10], max = [10, 5, 10] }
preset = "cave"                      # Or custom parameters
```

### Audio Scripting API Needed

```
// Functions we'd need to call from scripts
audio.play_sound(clip_path, position?, volume?)
audio.play_music(track_path, fade_time?)
audio.stop_music(fade_time?)
audio.set_volume(channel, volume)
audio.play_at_entity(entity_id, clip_path)
```

---

## Game State & Variables

### Requirements

Persistent state, save/load, global variables.

### Suggested TOML Structure

```toml
# Scene-level state
[state]
# Typed variables accessible from scripts
[state.variables]
score = { type = "int", default = 0 }
player_name = { type = "string", default = "Player" }
difficulty = { type = "float", default = 1.0 }
has_key = { type = "bool", default = false }
checkpoint = { type = "vector3", default = [0, 0, 0] }

# Persistence
[state.save]
auto_save = true
auto_save_interval = 300             # Seconds
save_slot_count = 3
save_variables = ["score", "has_key", "checkpoint"]  # Which to persist
```

### Entity State

```toml
[[entities]]
name = "door"

[entities.state]
is_open = false
is_locked = true
required_key = "gold_key"
```

### Scripting API Needed

```
// State access from scripts
state.get("score")
state.set("score", 100)
state.increment("score", 10)
state.save(slot_number)
state.load(slot_number)

// Entity state
entity.get_state("is_open")
entity.set_state("is_open", true)
```

---

## UI/HUD System

### Requirements

Health bars, score display, menus, damage numbers.

### Suggested TOML Structure

```toml
# HUD definition
[ui.hud]
enabled = true
layout = "ui/layouts/game_hud.toml"

# Health bar
[[ui.hud.elements]]
type = "progress_bar"
id = "health_bar"
anchor = "top_left"
offset = [20, 20]
size = [200, 20]
bind_to = "player.health.current"
bind_max = "player.health.max"
fill_color = [0.8, 0.2, 0.2, 1]
background_color = [0.2, 0.2, 0.2, 0.8]

# Score display
[[ui.hud.elements]]
type = "text"
id = "score_display"
anchor = "top_right"
offset = [-20, 20]
font = "fonts/game_font.ttf"
font_size = 24
color = [1, 1, 1, 1]
bind_to = "state.score"
format = "Score: {value}"

# Crosshair
[[ui.hud.elements]]
type = "image"
id = "crosshair"
anchor = "center"
offset = [0, 0]
size = [32, 32]
image = "ui/crosshair.png"

# Damage numbers (floating combat text)
[ui.damage_numbers]
enabled = true
font = "fonts/damage_font.ttf"
font_size = 20
float_speed = 50                     # Pixels per second upward
fade_time = 1.0
colors = {
  physical = [1, 1, 1, 1],
  fire = [1, 0.5, 0, 1],
  ice = [0, 0.8, 1, 1],
  critical = [1, 1, 0, 1]
}
```

---

## AI & Navigation

### Requirements

Pathfinding, enemy behavior, waypoints.

### Suggested TOML Structure

```toml
# Navigation mesh (baked separately)
[navigation]
navmesh = "navmesh/level1.nav"
agent_radius = 0.5
agent_height = 2.0
step_height = 0.3
max_slope = 45                       # Degrees

# AI Entity
[[entities]]
name = "enemy_patrol"

[entities.ai]
enabled = true
type = "state_machine"               # state_machine, behavior_tree, utility

# Simple state machine
[entities.ai.states]
idle = { duration = 2.0, next = "patrol" }
patrol = { waypoints = "patrol_path_1", speed = 3.0 }
chase = { target = "player", speed = 5.0, give_up_distance = 20.0 }
attack = { range = 2.0, cooldown = 1.0 }

[entities.ai.transitions]
# state -> condition -> new_state
idle_to_chase = { from = "idle", condition = "player_visible", to = "chase" }
patrol_to_chase = { from = "patrol", condition = "player_visible", to = "chase" }
chase_to_attack = { from = "chase", condition = "in_attack_range", to = "attack" }
chase_to_patrol = { from = "chase", condition = "lost_player", to = "patrol" }

[entities.ai.senses]
sight_range = 15.0
sight_angle = 120                    # Degrees
hearing_range = 10.0

# Waypoint path
[[waypoint_paths]]
name = "patrol_path_1"
loop = true
points = [
  [0, 0, 0],
  [10, 0, 0],
  [10, 0, 10],
  [0, 0, 10]
]
wait_times = [2.0, 0, 2.0, 0]        # Seconds to wait at each point
```

---

## Networking (Optional)

If multiplayer support is planned:

```toml
[networking]
mode = "client_server"               # client_server, peer_to_peer
max_players = 16
tick_rate = 60

[networking.replication]
# Which components sync over network
sync_transform = true
sync_health = true
sync_animation = true

[[entities]]
name = "networked_player"

[entities.network]
replicated = true
owner_authority = true               # Owner controls this object
interpolation = true
interpolation_delay = 0.1            # Seconds
```

---

## Editor Integration Notes

For the Void Editor to support these features, please document:

### 1. File References
How the editor should handle references between files:
- Entity -> Script file
- Entity -> Weapon definition
- Pickup -> Item definition
- AI -> Waypoint path

### 2. Prefab System
If there's a prefab/template system for reusable entity configurations.

### 3. Event Autocompletion
List of all valid event names so the editor can provide autocomplete.

### 4. Variable Types
All supported variable types for state system.

### 5. Enum Values
All enum options for each field (damage types, body types, AI states, etc.)

---

## Summary Checklist

Please provide documentation for:

- [ ] Scripting language reference (`.vs` files)
- [ ] Scripting API (engine functions callable from scripts)
- [ ] Event handler signatures (parameters for each event)
- [ ] Trigger volumes (`[[triggers]]`)
- [ ] Physics system (`[entities.physics]`)
- [ ] Collision layers and matrix
- [ ] Health/damage system (`[entities.health]`)
- [ ] Weapon definitions (`[[weapons]]`)
- [ ] Projectile system
- [ ] Pickup system (`[entities.pickup]`)
- [ ] Item definitions
- [ ] Inventory system (`[entities.inventory]`)
- [ ] Audio system (`[entities.audio]`)
- [ ] Music and ambient audio
- [ ] Game state/variables (`[state]`)
- [ ] Save/load system
- [ ] UI/HUD system (`[ui.hud]`)
- [ ] Navigation/pathfinding (`[navigation]`)
- [ ] AI system (`[entities.ai]`)
- [ ] Waypoint paths (`[[waypoint_paths]]`)

---

## Questions for the Team

1. **Scripting Language**: Is `.vs` a custom language or based on something (Lua, WASM)?

2. **Execution Model**: Are scripts run per-entity, per-scene, or globally?

3. **Hot Reload**: Can scripts be reloaded without restarting the scene?

4. **Performance**: Any limits on script complexity or API call frequency?

5. **Debugging**: Is there a debugger, console, or logging system?

6. **Physics Engine**: Which physics engine? (custom, Rapier, PhysX?)

7. **Audio Engine**: Which audio backend? (custom, FMOD, Wwise?)

8. **Networking**: Is multiplayer planned? What netcode model?

---

*Document created for Void GUI renderer team - please provide corresponding documentation in the same markdown + TOML format as existing `docs/input-formats/*.md` files.*
