# Combat & Damage System

The combat system provides health management, damage dealing, weapons, and projectiles for game entities.

## Health Component

### Basic Health

```toml
[[entities]]
name = "enemy"
mesh = "enemy.glb"

[entities.health]
max_health = 100
current_health = 100             # Optional, defaults to max
```

### Full Health Configuration

```toml
[[entities]]
name = "player"

[entities.health]
max_health = 100
current_health = 100
invulnerable = false
invulnerability_time = 0.5       # I-frames after taking damage
regeneration = 0.0               # Health per second
regeneration_delay = 3.0         # Seconds after damage before regen starts

# Damage type resistances (multipliers)
[entities.health.resistances]
physical = 1.0                   # Normal damage
fire = 0.5                       # Takes half fire damage
ice = 1.5                        # Takes 50% more ice damage
electric = 1.0
poison = 0.0                     # Immune to poison
energy = 1.0
true = 1.0                       # True damage ignores resistances

# Armor
[entities.health.armor]
value = 10                       # Flat damage reduction
penetration_threshold = 50       # Damage above this ignores armor

# Events
[entities.health.events]
on_damage = "HandleDamage"
on_heal = "HandleHeal"
on_death = "HandleDeath"
on_revive = "HandleRevive"

# Visual feedback
[entities.health.feedback]
show_damage_numbers = true
hit_flash_color = [1, 0, 0, 0.5]
hit_flash_duration = 0.1
death_effect = "effects/death_burst.toml"
```

## Damage Types

| Type | Description | Typical Use |
|------|-------------|-------------|
| `physical` | Default damage type | Bullets, melee, fall damage |
| `fire` | Burning damage | Flames, explosions, lava |
| `ice` | Cold damage | Freeze effects |
| `electric` | Shock damage | Tesla, stun effects |
| `poison` | Damage over time | Toxic zones, venomous attacks |
| `energy` | Energy-based | Lasers, plasma, magic |
| `true` | Ignores all resistances | Instakill zones, scripted damage |

## Weapons

### Hitscan Weapon

Instant raycast-based weapons:

```toml
[[weapons]]
name = "laser_pistol"
type = "hitscan"

[weapons.hitscan]
damage = 25
damage_type = "energy"
range = 100.0                    # Max distance
fire_rate = 5.0                  # Shots per second
spread = 2.0                     # Degrees of random spread
penetration = 0                  # Targets to pierce (0 = none)
headshot_multiplier = 2.0        # Bonus for head hits

[weapons.ammo]
magazine_size = 12
reserve_max = 120
reload_time = 1.5
auto_reload = true

[weapons.effects]
muzzle_flash = "effects/muzzle_flash.toml"
hit_effect = "effects/laser_hit.toml"
tracer = "effects/laser_tracer.toml"
sound_fire = "sounds/laser_fire.wav"
sound_reload = "sounds/laser_reload.wav"
sound_empty = "sounds/click.wav"
```

### Projectile Weapon

Physical projectiles:

```toml
[[weapons]]
name = "rocket_launcher"
type = "projectile"

[weapons.projectile]
prefab = "prefabs/rocket.toml"   # Projectile entity to spawn
speed = 50.0
damage = 100
damage_type = "fire"
splash_radius = 5.0
splash_damage_falloff = "linear" # linear, none, exponential
gravity_scale = 0.1              # Projectile gravity
lifetime = 10.0                  # Seconds before auto-destroy
homing = false

[weapons.ammo]
magazine_size = 1
reserve_max = 20
reload_time = 3.0

[weapons.effects]
muzzle_flash = "effects/rocket_muzzle.toml"
sound_fire = "sounds/rocket_fire.wav"
```

### Melee Weapon

Close-range attacks:

```toml
[[weapons]]
name = "sword"
type = "melee"

[weapons.melee]
damage = 40
damage_type = "physical"
range = 2.0                      # Attack reach
arc = 90                         # Degrees of swing arc
swing_time = 0.3                 # Attack duration
recovery_time = 0.2              # Time before next attack
combo_window = 0.5               # Seconds to chain combo
max_combo = 3                    # Combo length
combo_damage_mult = [1.0, 1.2, 1.5]  # Damage per combo hit

[weapons.melee.hitbox]
shape = "box"
size = [1.5, 1.0, 2.0]
offset = [0.75, 0, 1.0]

[weapons.effects]
swing_effect = "effects/sword_trail.toml"
hit_effect = "effects/slash_hit.toml"
sound_swing = "sounds/sword_swing.wav"
sound_hit = "sounds/sword_hit.wav"
```

### Shotgun (Multi-pellet)

```toml
[[weapons]]
name = "shotgun"
type = "hitscan"

[weapons.hitscan]
damage = 15                      # Per pellet
damage_type = "physical"
range = 30.0
fire_rate = 1.0
spread = 8.0
pellet_count = 8                 # Multiple rays per shot
falloff_start = 10.0
falloff_end = 30.0
falloff_multiplier = 0.3

[weapons.ammo]
magazine_size = 6
reserve_max = 48
reload_time = 0.5
reload_type = "single"           # Reload one shell at a time
```

### Beam Weapon (Continuous)

```toml
[[weapons]]
name = "plasma_beam"
type = "beam"

[weapons.beam]
damage_per_second = 80
damage_type = "energy"
range = 50.0
width = 0.2
heat_per_second = 20.0
max_heat = 100.0
cooldown_rate = 30.0

[weapons.effects]
beam_effect = "effects/plasma_beam.toml"
hit_effect = "effects/plasma_burn.toml"
overheat_effect = "effects/overheat_steam.toml"
sound_fire = "sounds/beam_loop.wav"
sound_overheat = "sounds/overheat.wav"
```

## Projectile Entities

Define projectile behavior:

```toml
# prefabs/rocket.toml
[prefab]
name = "rocket"

[[entities]]
name = "rocket"
mesh = "projectile_rocket.glb"

[entities.projectile]
owner_immune = true              # Can't damage shooter
team_immune = true               # Can't damage allies
damage = 100
damage_type = "fire"
splash_radius = 5.0
splash_falloff = "linear"

[entities.physics]
body_type = "dynamic"
gravity_scale = 0.1
continuous_detection = true

[entities.physics.collider]
shape = "capsule"
radius = 0.1
height = 0.4

# Homing behavior
[entities.projectile.homing]
enabled = false
turn_rate = 180                  # Degrees per second
acquire_range = 20.0
lock_angle = 30.0

# Effects
[entities.projectile.effects]
trail = "effects/smoke_trail.toml"
explosion = "effects/explosion.toml"
sound_flight = "sounds/rocket_fly.wav"
sound_explode = "sounds/explosion.wav"

# Events
[entities.projectile.events]
on_hit = "OnRocketHit"
on_expire = "OnRocketExpire"
```

## Status Effects

### Damage Over Time

```toml
# Apply via script or weapon
[status_effects.burning]
type = "dot"                     # Damage over time
damage = 5
damage_type = "fire"
tick_rate = 0.5                  # Damage every 0.5 seconds
duration = 3.0
stackable = false
max_stacks = 1
effect = "effects/burning.toml"
sound = "sounds/burning.wav"

[status_effects.poison]
type = "dot"
damage = 3
damage_type = "poison"
tick_rate = 1.0
duration = 10.0
stackable = true
max_stacks = 5

[status_effects.bleeding]
type = "dot"
damage = 2
damage_type = "physical"
tick_rate = 0.25
duration = 5.0
stackable = true
max_stacks = 3
```

### Crowd Control

```toml
[status_effects.stun]
type = "cc"
cc_type = "stun"                 # Prevents all actions
duration = 2.0
effect = "effects/stunned.toml"

[status_effects.slow]
type = "cc"
cc_type = "slow"
move_speed_mult = 0.5            # 50% speed
duration = 3.0
effect = "effects/frozen_slow.toml"

[status_effects.root]
type = "cc"
cc_type = "root"                 # Can't move, can attack
duration = 2.0

[status_effects.knockback]
type = "cc"
cc_type = "knockback"
force = 500.0
direction = "away_from_source"
```

### Buffs/Debuffs

```toml
[status_effects.damage_boost]
type = "buff"
damage_mult = 1.5
duration = 10.0
effect = "effects/damage_buff.toml"

[status_effects.armor_break]
type = "debuff"
armor_reduction = 20
resistance_mult = 1.3            # Take 30% more damage
duration = 5.0
```

## Weapon Component

Attach weapons to entities:

```toml
[[entities]]
name = "player"

[entities.weapon]
equipped = "laser_pistol"        # Reference to weapon definition
socket = "hand_r"                # Attachment bone/point

[entities.weapon.state]
current_ammo = 12
reserve_ammo = 60
heat = 0.0

# Input bindings
[entities.weapon.input]
fire = "attack"                  # Input action name
aim = "aim"
reload = "reload"
switch_next = "weapon_next"
switch_prev = "weapon_prev"
```

## Combat Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_damage` | `amount: float, type: string, source: Entity, point: Vec3` | Took damage |
| `on_death` | `killer: Entity, damage_type: string` | Health reached 0 |
| `on_heal` | `amount: float, healer: Entity` | Received healing |
| `on_revive` | `reviver: Entity` | Returned from death |
| `on_kill` | `victim: Entity, damage_type: string` | Killed another entity |

## Combat Scripting API

### Dealing Damage

```cpp
// Basic damage
Target->ApplyDamage(25.0f, EDamageType::Physical, Attacker);

// Damage with hit info
FDamageInfo Info;
Info.Amount = 50.0f;
Info.Type = EDamageType::Fire;
Info.Source = Attacker;
Info.HitPoint = ImpactPoint;
Info.HitNormal = SurfaceNormal;
Info.IsCritical = bHeadshot;
Target->ApplyDamage(Info);

// Area damage
Combat->ApplyRadialDamage(Center, Radius, MaxDamage, DamageType, Instigator, FalloffType);
```

### Health Management

```cpp
// Get health
float Current = Entity->GetHealth();
float Max = Entity->GetMaxHealth();
float Percent = Entity->GetHealthPercent();

// Modify health
Entity->Heal(25.0f);
Entity->SetHealth(50.0f);
Entity->SetMaxHealth(150.0f);

// Invulnerability
Entity->SetInvulnerable(true);
bool bInvuln = Entity->IsInvulnerable();
```

### Weapon Control

```cpp
// Fire weapon
Weapon->Fire();
Weapon->StartFire();  // Continuous
Weapon->StopFire();

// Reload
Weapon->Reload();
bool bReloading = Weapon->IsReloading();

// Ammo
int Ammo = Weapon->GetCurrentAmmo();
int Reserve = Weapon->GetReserveAmmo();
Weapon->AddAmmo(30);
```

### Status Effects

```cpp
// Apply status
Entity->ApplyStatus("burning", Duration, Instigator);

// Check status
bool bBurning = Entity->HasStatus("burning");
float TimeLeft = Entity->GetStatusDuration("burning");

// Remove status
Entity->RemoveStatus("burning");
Entity->ClearAllStatuses();
```

## Complete Example

```toml
# Combat-focused game scene

# Player with full combat setup
[[entities]]
name = "player"
mesh = "characters/player.glb"

[entities.transform]
position = [0, 0, 0]

[entities.health]
max_health = 100
regeneration = 5.0
regeneration_delay = 5.0

[entities.health.resistances]
fire = 0.8                       # Some fire resistance

[entities.health.events]
on_damage = "OnPlayerDamage"
on_death = "OnPlayerDeath"

[entities.health.feedback]
show_damage_numbers = true
hit_flash_color = [1, 0, 0, 0.3]
hit_flash_duration = 0.15

[entities.weapon]
equipped = "assault_rifle"
socket = "hand_r"

# Enemy soldier
[[entities]]
name = "enemy_soldier"
mesh = "characters/soldier.glb"

[entities.transform]
position = [20, 0, 0]

[entities.health]
max_health = 80
invulnerability_time = 0.2

[entities.health.armor]
value = 5

[entities.health.events]
on_damage = "OnEnemyDamage"
on_death = "OnEnemyDeath"

[entities.weapon]
equipped = "enemy_rifle"
socket = "hand_r"

# Heavy enemy with resistances
[[entities]]
name = "enemy_heavy"
mesh = "characters/heavy.glb"

[entities.transform]
position = [30, 0, 10]

[entities.health]
max_health = 250

[entities.health.resistances]
physical = 0.7                   # Armored
fire = 0.5
electric = 2.0                   # Weak to electric

[entities.health.armor]
value = 20
penetration_threshold = 80

# Weapon definitions
[[weapons]]
name = "assault_rifle"
type = "hitscan"

[weapons.hitscan]
damage = 20
damage_type = "physical"
range = 80.0
fire_rate = 10.0
spread = 3.0
headshot_multiplier = 2.5

[weapons.ammo]
magazine_size = 30
reserve_max = 180
reload_time = 2.0

[weapons.effects]
muzzle_flash = "effects/muzzle_ar.toml"
hit_effect = "effects/bullet_impact.toml"
tracer = "effects/tracer.toml"
sound_fire = "sounds/rifle_fire.wav"
sound_reload = "sounds/rifle_reload.wav"

[[weapons]]
name = "enemy_rifle"
type = "hitscan"

[weapons.hitscan]
damage = 12
damage_type = "physical"
range = 60.0
fire_rate = 6.0
spread = 5.0

[weapons.ammo]
magazine_size = 25
reserve_max = 9999               # Infinite for AI

[[weapons]]
name = "grenade_launcher"
type = "projectile"

[weapons.projectile]
prefab = "prefabs/grenade.toml"
speed = 25.0
damage = 80
damage_type = "fire"
splash_radius = 4.0
splash_damage_falloff = "linear"
gravity_scale = 1.0
lifetime = 5.0

[weapons.ammo]
magazine_size = 1
reserve_max = 10
reload_time = 2.5

[[weapons]]
name = "plasma_sword"
type = "melee"

[weapons.melee]
damage = 60
damage_type = "energy"
range = 2.5
arc = 120
swing_time = 0.4
recovery_time = 0.3
combo_window = 0.6
max_combo = 4
combo_damage_mult = [1.0, 1.1, 1.3, 2.0]

[weapons.effects]
swing_effect = "effects/plasma_trail.toml"
hit_effect = "effects/plasma_cut.toml"
sound_swing = "sounds/plasma_swing.wav"
sound_hit = "sounds/plasma_hit.wav"

# Explosive barrel
[[entities]]
name = "explosive_barrel"
mesh = "props/barrel_red.glb"

[entities.transform]
position = [15, 0, 5]

[entities.health]
max_health = 30

[entities.health.events]
on_death = "BarrelExplode"

[entities.physics]
body_type = "dynamic"
mass = 50.0

# Damage zone (lava)
[[triggers]]
name = "lava_damage"
shape = "box"
size = [20, 2, 20]
position = [50, -1, 0]
layer_mask = ["player", "enemies"]
cooldown = 0.5
on_stay = "ApplyLavaDamage"

[triggers.data]
damage = 25
damage_type = "fire"
apply_burning = true
```
