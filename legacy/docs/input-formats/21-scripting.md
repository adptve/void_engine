# Scripting System

Void GUI v2 uses a dual-scripting approach: **C++ native code** for high-performance game logic and **Blueprints** for visual scripting. This follows the Unreal Engine model.

## Overview

| Approach | Use Case | Performance | Hot-Reload |
|----------|----------|-------------|------------|
| C++ | Complex AI, physics, core systems | Maximum | Yes (recompile) |
| Blueprint | Game logic, events, prototyping | Good | Yes (instant) |

## C++ Scripting

### Attaching C++ Classes to Entities

```toml
[[entities]]
name = "player"
mesh = "character.glb"

[entities.cpp_class]
library = "game_logic.dll"        # Windows
# library = "libgame_logic.so"    # Linux
# library = "libgame_logic.dylib" # macOS
class = "PlayerController"        # Class name in library
hot_reload = true                 # Enable hot-reload
```

### C++ Class Properties

Expose properties to the editor via TOML:

```toml
[[entities]]
name = "enemy"

[entities.cpp_class]
library = "game_logic.dll"
class = "EnemyAI"

# Properties passed to C++ class
[entities.cpp_class.properties]
max_health = 100.0
patrol_speed = 3.0
chase_speed = 6.0
attack_damage = 25
attack_cooldown = 1.5
detection_range = 15.0
```

### Property Types

| Type | TOML Syntax | C++ Type |
|------|-------------|----------|
| Integer | `value = 42` | `int32_t` |
| Float | `value = 3.14` | `float` |
| Boolean | `value = true` | `bool` |
| String | `value = "text"` | `std::string` |
| Vector3 | `value = [1, 2, 3]` | `FVector` |
| Vector4 | `value = [1, 2, 3, 4]` | `FVector4` |
| Color | `value = [1, 0, 0, 1]` | `FColor` |
| Asset | `value = "path/to/asset"` | `AssetRef` |
| Entity | `value = "entity_name"` | `EntityId` |

### C++ Base Classes

All game classes inherit from these base classes:

| Base Class | Purpose |
|------------|---------|
| `VoidActor` | Game entities with transform |
| `VoidComponent` | Attachable components |
| `VoidGameMode` | Game rules and state |
| `VoidPlayerController` | Player input handling |
| `VoidAIController` | AI behavior |
| `VoidWidget` | UI elements |

### C++ Lifecycle Methods

```cpp
class MyActor : public VoidActor {
public:
    // Called when entity spawns
    virtual void BeginPlay() override;

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Called when entity is destroyed
    virtual void EndPlay() override;

    // Called at fixed timestep (physics)
    virtual void FixedTick(float FixedDeltaTime) override;
};
```

### C++ Event Methods

```cpp
class MyActor : public VoidActor {
public:
    // Collision events
    virtual void OnCollisionEnter(VoidActor* Other, const FHitResult& Hit) override;
    virtual void OnCollisionStay(VoidActor* Other, const FHitResult& Hit) override;
    virtual void OnCollisionExit(VoidActor* Other) override;

    // Trigger events
    virtual void OnTriggerEnter(VoidActor* Other) override;
    virtual void OnTriggerStay(VoidActor* Other) override;
    virtual void OnTriggerExit(VoidActor* Other) override;

    // Combat events
    virtual void OnDamage(float Amount, EDamageType Type, VoidActor* Source) override;
    virtual void OnDeath(VoidActor* Killer) override;
    virtual void OnHeal(float Amount, VoidActor* Healer) override;

    // Input events
    virtual void OnInputAction(const FInputAction& Action) override;

    // Interaction events
    virtual void OnInteract(VoidActor* Interactor) override;
    virtual void OnFocus(VoidActor* Focuser) override;
    virtual void OnUnfocus(VoidActor* Focuser) override;
};
```

## Blueprint Visual Scripting

### Attaching Blueprints to Entities

```toml
[[entities]]
name = "door"
mesh = "door.glb"

[entities.blueprint]
graph = "blueprints/interactive_door.bp"
enabled = true
```

### Blueprint Variables

```toml
[[entities]]
name = "treasure_chest"

[entities.blueprint]
graph = "blueprints/chest.bp"

# Variables exposed to editor
[entities.blueprint.variables]
is_locked = { type = "bool", default = true }
required_key = { type = "string", default = "gold_key" }
gold_amount = { type = "int", default = 100, min = 0, max = 10000 }
open_sound = { type = "asset", default = "sounds/chest_open.wav" }
```

### Variable Types

| Type | TOML Syntax | Description |
|------|-------------|-------------|
| `bool` | `{ type = "bool", default = false }` | True/false |
| `int` | `{ type = "int", default = 0 }` | Integer number |
| `float` | `{ type = "float", default = 0.0 }` | Decimal number |
| `string` | `{ type = "string", default = "" }` | Text |
| `vec2` | `{ type = "vec2", default = [0, 0] }` | 2D vector |
| `vec3` | `{ type = "vec3", default = [0, 0, 0] }` | 3D vector |
| `vec4` | `{ type = "vec4", default = [0, 0, 0, 0] }` | 4D vector |
| `color` | `{ type = "color", default = [1, 1, 1, 1] }` | RGBA color |
| `entity` | `{ type = "entity", default = "" }` | Entity reference |
| `asset` | `{ type = "asset", default = "" }` | Asset path |

### Blueprint Event Bindings

Map entity events to Blueprint graph entry points:

```toml
[entities.blueprint.events]
on_begin_play = "BeginPlay"
on_tick = "Tick"
on_collision_enter = "OnCollisionEnter"
on_trigger_enter = "OnTriggerEnter"
on_damage = "OnDamage"
on_death = "OnDeath"
on_interact = "OnInteract"
```

## Event Handler Signatures

All event handlers receive specific parameters:

### Lifecycle Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_begin_play` | (none) | Entity spawned |
| `on_tick` | `delta_time: float` | Every frame |
| `on_end_play` | (none) | Entity destroyed |

### Collision Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_collision_enter` | `other: Entity, hit_point: Vec3, hit_normal: Vec3, impulse: float` | Collision started |
| `on_collision_stay` | `other: Entity, hit_point: Vec3, hit_normal: Vec3` | Collision ongoing |
| `on_collision_exit` | `other: Entity` | Collision ended |

### Trigger Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_trigger_enter` | `other: Entity` | Entered trigger |
| `on_trigger_stay` | `other: Entity` | Inside trigger |
| `on_trigger_exit` | `other: Entity` | Exited trigger |

### Combat Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_damage` | `amount: float, type: string, source: Entity, hit_point: Vec3` | Received damage |
| `on_death` | `killer: Entity, damage_type: string` | Health reached zero |
| `on_heal` | `amount: float, healer: Entity` | Received healing |

### Input Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_input_action` | `action: string, value: float, pressed: bool` | Input action |
| `on_key_pressed` | `key: string, modifiers: int` | Key down |
| `on_key_released` | `key: string, modifiers: int` | Key up |
| `on_mouse_button` | `button: int, pressed: bool, position: Vec2` | Mouse button |

### Interaction Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_interact` | `interactor: Entity` | Player interacted |
| `on_focus` | `focuser: Entity` | Player looking at |
| `on_unfocus` | `focuser: Entity` | Player looked away |

## Blueprint Node Categories

### Event Nodes (Entry Points)

| Node | Description |
|------|-------------|
| `Event BeginPlay` | Called when entity spawns |
| `Event Tick` | Called every frame |
| `Event OnCollision` | Collision occurred |
| `Event OnTrigger` | Trigger volume event |
| `Event OnDamage` | Received damage |
| `Event OnInteract` | Player interacted |
| `Event Custom` | Custom named event |

### Flow Control Nodes

| Node | Description |
|------|-------------|
| `Branch` | If/else conditional |
| `Sequence` | Execute in order |
| `For Loop` | Repeat N times |
| `For Each` | Iterate collection |
| `While Loop` | Loop with condition |
| `Do Once` | Execute only once |
| `Gate` | Enable/disable flow |
| `Delay` | Wait before continuing |

### Entity Nodes

| Node | Description |
|------|-------------|
| `Get Self` | Current entity |
| `Spawn Actor` | Create new entity |
| `Destroy Actor` | Remove entity |
| `Get Location` | Get position |
| `Set Location` | Set position |
| `Get Rotation` | Get orientation |
| `Set Rotation` | Set orientation |
| `Get Component` | Get component by type |
| `Add Component` | Add new component |

### Math Nodes

| Node | Description |
|------|-------------|
| `Add`, `Subtract`, `Multiply`, `Divide` | Arithmetic |
| `Abs`, `Floor`, `Ceil`, `Round` | Rounding |
| `Min`, `Max`, `Clamp` | Range |
| `Lerp`, `Slerp` | Interpolation |
| `Sin`, `Cos`, `Tan` | Trigonometry |
| `Distance`, `Normalize`, `Dot`, `Cross` | Vector math |

### Logic Nodes

| Node | Description |
|------|-------------|
| `And`, `Or`, `Not`, `Xor` | Boolean logic |
| `Equals`, `Not Equals` | Equality |
| `Greater`, `Less`, `Greater or Equal`, `Less or Equal` | Comparison |

### Variable Nodes

| Node | Description |
|------|-------------|
| `Get Variable` | Read variable value |
| `Set Variable` | Write variable value |
| `Get Game State` | Read global state |
| `Set Game State` | Write global state |

## Script Execution Model

### Execution Order per Frame

```
1. Physics pre-solve
2. C++ FixedTick (fixed timestep loop)
3. Blueprint EventTick execution
4. C++ Tick
5. Physics solve
6. Transform sync
7. Render
```

### Script Isolation

Each entity's script runs in isolation:

- **Namespace**: `entity_name::script`
- **Variables**: Private unless exposed
- **Communication**: Via events and state system
- **Capabilities**: Based on kernel permissions

## Hot-Reload

### C++ Hot-Reload

1. Modify C++ source file
2. Recompile to DLL/SO
3. Engine detects change
4. Instances serialized
5. Old library unloaded
6. New library loaded
7. Instances restored

```toml
[entities.cpp_class]
hot_reload = true
hot_reload_preserve = ["health", "position", "custom_data"]
```

### Blueprint Hot-Reload

1. Modify `.bp` file
2. Asset server detects change
3. Graph reloaded
4. Variables preserved
5. Execution continues

## Combining C++ and Blueprints

Entities can use both:

```toml
[[entities]]
name = "boss"
mesh = "boss.glb"

# C++ for performance-critical logic
[entities.cpp_class]
library = "game_logic.dll"
class = "BossAI"

# Blueprint for event responses and tuning
[entities.blueprint]
graph = "blueprints/boss_events.bp"
```

Blueprint can call into C++:

```toml
# In blueprint, use "Call C++ Function" node
[[nodes]]
id = 10
type = "CallCppFunction"
position = [500, 200]
[nodes.pins]
function = "ActivateSpecialAttack"
params = { attack_type = "flame_breath" }
```

## Complete Example

### Player Character

```toml
[[entities]]
name = "player"
mesh = "assets/models/player.glb"

[entities.transform]
position = [0, 0, 0]

[entities.cpp_class]
library = "game_logic.dll"
class = "PlayerController"
hot_reload = true

[entities.cpp_class.properties]
max_health = 100.0
move_speed = 5.0
sprint_speed = 8.0
jump_force = 10.0
look_sensitivity = 2.0

[entities.blueprint]
graph = "blueprints/player/BP_PlayerEvents.bp"

[entities.blueprint.variables]
is_sprinting = { type = "bool", default = false }
stamina = { type = "float", default = 100.0 }
current_weapon = { type = "string", default = "pistol" }

[entities.blueprint.events]
on_damage = "OnPlayerDamage"
on_death = "OnPlayerDeath"
on_pickup = "OnItemPickup"

[entities.physics]
body_type = "dynamic"
mass = 80.0

[entities.physics.collider]
shape = "capsule"
radius = 0.4
height = 1.8

[entities.health]
max_health = 100
regeneration = 5.0
regeneration_delay = 3.0

[entities.inventory]
slots = 20
equipment_slots = ["primary", "secondary", "head", "chest", "legs"]
```

### Enemy AI

```toml
[[entities]]
name = "enemy_soldier"
mesh = "assets/models/soldier.glb"

[entities.cpp_class]
library = "game_logic.dll"
class = "EnemyAI"

[entities.cpp_class.properties]
patrol_speed = 2.5
chase_speed = 5.0
attack_range = 2.0
sight_range = 20.0
sight_angle = 120.0
hearing_range = 15.0

[entities.blueprint]
graph = "blueprints/enemies/BP_Soldier.bp"

[entities.blueprint.variables]
alert_level = { type = "float", default = 0.0 }
current_state = { type = "string", default = "idle" }
target_entity = { type = "entity", default = "" }

[entities.ai]
enabled = true
navmesh = "navmesh/level1.nav"

[entities.health]
max_health = 100

[entities.health.events]
on_damage = "OnSoldierDamage"
on_death = "OnSoldierDeath"
```
