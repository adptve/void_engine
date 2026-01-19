# AI & Navigation System

The AI system provides pathfinding, behavior trees, state machines, and sensory systems for NPCs and enemies.

## Navigation Mesh

### NavMesh Configuration

```toml
[navigation]
navmesh = "navmesh/level1.nav"   # Pre-baked navmesh
agent_radius = 0.5               # Agent collision radius
agent_height = 2.0               # Agent height
step_height = 0.3                # Max step climb
max_slope = 45                   # Max walkable slope (degrees)
```

### NavMesh Generation Settings

```toml
[navigation.generation]
cell_size = 0.3                  # XZ resolution
cell_height = 0.2                # Y resolution
walkable_climb = 0.3
walkable_slope = 45
min_region_area = 8
merge_region_area = 20
edge_max_length = 12
edge_max_error = 1.3
verts_per_poly = 6
detail_sample_distance = 6
detail_max_error = 1
```

### NavMesh Areas

```toml
[navigation.areas]
default = { cost = 1.0 }
grass = { cost = 1.2 }
water = { cost = 3.0 }
road = { cost = 0.8 }
danger = { cost = 10.0 }
blocked = { cost = -1 }          # Impassable
```

## AI Component

### Basic AI

```toml
[[entities]]
name = "enemy_grunt"
mesh = "characters/grunt.glb"

[entities.ai]
enabled = true
type = "state_machine"           # state_machine, behavior_tree, utility
```

### State Machine AI

```toml
[[entities]]
name = "patrol_guard"

[entities.ai]
enabled = true
type = "state_machine"
initial_state = "idle"

[entities.ai.states]
[entities.ai.states.idle]
duration = 2.0
animation = "idle"
next_state = "patrol"

[entities.ai.states.patrol]
type = "patrol"
waypoints = "patrol_path_01"
speed = 3.0
animation = "walk"

[entities.ai.states.chase]
type = "chase"
target = "player"
speed = 5.0
animation = "run"
give_up_distance = 25.0
give_up_time = 5.0

[entities.ai.states.attack]
type = "attack"
range = 2.0
cooldown = 1.5
animation = "attack"

[entities.ai.states.search]
type = "search"
duration = 10.0
animation = "look_around"
search_radius = 5.0

# State transitions
[[entities.ai.transitions]]
from = "idle"
to = "chase"
condition = "can_see_player"

[[entities.ai.transitions]]
from = "patrol"
to = "chase"
condition = "can_see_player"

[[entities.ai.transitions]]
from = "chase"
to = "attack"
condition = "in_attack_range"

[[entities.ai.transitions]]
from = "chase"
to = "search"
condition = "lost_target"

[[entities.ai.transitions]]
from = "attack"
to = "chase"
condition = "target_out_of_range"

[[entities.ai.transitions]]
from = "search"
to = "patrol"
condition = "search_timeout"

[[entities.ai.transitions]]
from = "search"
to = "chase"
condition = "can_see_player"
```

### Behavior Tree AI

```toml
[[entities]]
name = "smart_enemy"

[entities.ai]
enabled = true
type = "behavior_tree"
tree = "ai/trees/smart_enemy.bt"
```

Behavior tree file:

```toml
# ai/trees/smart_enemy.bt
[behavior_tree]
name = "SmartEnemy"

# Root selector - tries children until one succeeds
[root]
type = "selector"

[[root.children]]
type = "sequence"
name = "combat_sequence"

[[root.children.children]]
type = "condition"
name = "has_target"
check = "target != null"

[[root.children.children]]
type = "selector"
name = "attack_or_pursue"

[[root.children.children.children]]
type = "sequence"
name = "attack_sequence"

[[root.children.children.children.children]]
type = "condition"
check = "distance_to_target < attack_range"

[[root.children.children.children.children]]
type = "action"
name = "attack"
action = "AttackTarget"

[[root.children.children.children]]
type = "action"
name = "pursue"
action = "MoveToTarget"

[[root.children]]
type = "sequence"
name = "patrol_sequence"

[[root.children.children]]
type = "condition"
check = "has_patrol_path"

[[root.children.children]]
type = "action"
action = "PatrolPath"

[[root.children]]
type = "action"
name = "idle"
action = "Idle"
```

### Utility AI

```toml
[[entities]]
name = "utility_npc"

[entities.ai]
enabled = true
type = "utility"

# Actions with utility scores
[[entities.ai.actions]]
name = "attack"
base_score = 0.5

[[entities.ai.actions.considerations]]
type = "curve"
input = "target_distance"
curve = "inverse_linear"         # Closer = higher score
min = 0
max = 20

[[entities.ai.actions.considerations]]
type = "curve"
input = "own_health_percent"
curve = "linear"                 # Higher health = more aggressive
min = 0.2
max = 1.0

[[entities.ai.actions]]
name = "flee"
base_score = 0.3

[[entities.ai.actions.considerations]]
type = "curve"
input = "own_health_percent"
curve = "inverse_quadratic"      # Lower health = flee more
min = 0
max = 0.5

[[entities.ai.actions]]
name = "heal"
base_score = 0.4

[[entities.ai.actions.considerations]]
type = "threshold"
input = "own_health_percent"
threshold = 0.5
below_value = 1.0
above_value = 0.0

[[entities.ai.actions.considerations]]
type = "boolean"
input = "has_health_item"
true_value = 1.0
false_value = 0.0
```

## Sensory System

### Vision

```toml
[entities.ai.senses]
[entities.ai.senses.sight]
enabled = true
range = 20.0
angle = 120                      # Field of view (degrees)
height_offset = 1.7              # Eye height
raycast_count = 5                # Rays for detection
detection_time = 0.5             # Time to fully detect
peripheral_range = 8.0           # Reduced peripheral vision
peripheral_angle = 180

# What can be seen
detect_layers = ["player", "allies"]
blocked_by = ["terrain", "walls", "cover"]
```

### Hearing

```toml
[entities.ai.senses.hearing]
enabled = true
range = 15.0
through_walls = true
through_walls_reduction = 0.5    # 50% range through walls

# Sound detection thresholds
[[entities.ai.senses.hearing.sounds]]
type = "footstep"
detection_range = 8.0

[[entities.ai.senses.hearing.sounds]]
type = "gunshot"
detection_range = 50.0

[[entities.ai.senses.hearing.sounds]]
type = "explosion"
detection_range = 100.0
```

### Other Senses

```toml
[entities.ai.senses.damage]
enabled = true                   # Know when damaged
reveal_attacker = true

[entities.ai.senses.touch]
enabled = true
radius = 1.0                     # Proximity detection
```

### Awareness Levels

```toml
[entities.ai.awareness]
levels = ["unaware", "suspicious", "alert", "combat"]
initial_level = "unaware"

[entities.ai.awareness.decay]
suspicious_decay_time = 10.0     # Return to unaware
alert_decay_time = 30.0          # Return to suspicious
combat_decay_time = 60.0         # Return to alert

[entities.ai.awareness.thresholds]
suspicious = 0.3                 # Detection amount
alert = 0.6
combat = 1.0
```

## Waypoint Paths

### Path Definition

```toml
[[waypoint_paths]]
name = "patrol_path_01"
loop = true                      # Loop when reaching end
reverse_on_end = false           # Ping-pong instead of loop

[[waypoint_paths.points]]
position = [0, 0, 0]
wait_time = 2.0                  # Pause duration
look_direction = [1, 0, 0]       # Optional look direction
animation = "look_around"        # Optional animation at point

[[waypoint_paths.points]]
position = [10, 0, 0]
wait_time = 0.0

[[waypoint_paths.points]]
position = [10, 0, 10]
wait_time = 3.0
look_direction = [0, 0, 1]

[[waypoint_paths.points]]
position = [0, 0, 10]
wait_time = 2.0
```

### Dynamic Waypoints

```toml
[[waypoint_paths]]
name = "dynamic_patrol"
type = "dynamic"
center = [50, 0, 50]
radius = 15.0
point_count = 6
random_order = true
min_wait = 1.0
max_wait = 5.0
```

## Cover System

### Cover Points

```toml
[[cover_points]]
position = [10, 0, 5]
normal = [-1, 0, 0]              # Direction of cover
height = "low"                   # low, medium, high
width = 2.0
can_shoot_over = true
can_shoot_around_left = true
can_shoot_around_right = false
```

### Cover Volumes

```toml
[[cover_volumes]]
name = "building_cover"
mesh = "collision/building_cover.glb"
auto_generate_points = true
point_spacing = 2.0
detect_height = true
```

## Squad/Group AI

### AI Group

```toml
[[ai_groups]]
name = "enemy_squad_01"
formation = "line"               # line, wedge, circle, custom
spacing = 2.0
leader = "squad_leader_01"
members = ["grunt_01", "grunt_02", "grunt_03"]

[ai_groups.behavior]
follow_leader = true
max_spread = 10.0
regroup_distance = 15.0
share_targets = true
coordinate_attacks = true
```

### Formation Types

| Formation | Description |
|-----------|-------------|
| `line` | Side by side |
| `column` | Single file |
| `wedge` | V-shape |
| `circle` | Surround leader |
| `custom` | Custom positions |

## AI Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_target_acquired` | `target: Entity` | New target detected |
| `on_target_lost` | `target: Entity` | Target no longer visible |
| `on_damaged` | `damage: float, source: Entity` | Received damage |
| `on_ally_killed` | `ally: Entity, killer: Entity` | Ally died |
| `on_state_change` | `old_state: string, new_state: string` | State changed |
| `on_path_complete` | `path_name: string` | Reached destination |
| `on_waypoint_reached` | `waypoint_index: int` | Reached waypoint |

## AI Scripting API

### Movement

```cpp
// Basic movement
AI->MoveTo(TargetPosition);
AI->MoveToEntity(TargetEntity);
AI->StopMovement();

// With options
FMoveRequest Request;
Request.Target = TargetPosition;
Request.Speed = 5.0f;
Request.AcceptanceRadius = 1.0f;
Request.UsePathfinding = true;
Request.AvoidDynamic = true;
AI->MoveTo(Request);

// Patrol
AI->StartPatrol("patrol_path_01");
AI->StopPatrol();
AI->SetPatrolPaused(true);
```

### State Control

```cpp
// State machine
AI->SetState("chase");
FString CurrentState = AI->GetState();

// Behavior tree
AI->SetBlackboardValue("target", PlayerEntity);
auto Target = AI->GetBlackboardValue<Entity*>("target");

// Enable/disable
AI->SetEnabled(false);
```

### Sensing

```cpp
// Check senses
bool CanSee = AI->CanSeeEntity(Target);
bool CanHear = AI->CanHearEntity(Target);
float Awareness = AI->GetAwarenessOf(Target);

// Get perceived entities
TArray<Entity*> VisibleEnemies = AI->GetVisibleEntities("enemy");
Entity* NearestThreat = AI->GetNearestThreat();

// Set target
AI->SetTarget(Target);
AI->ClearTarget();
Entity* CurrentTarget = AI->GetTarget();
```

### Pathfinding

```cpp
// Find path
TArray<FVector> Path = Navigation->FindPath(Start, End);

// Check reachability
bool Reachable = Navigation->IsReachable(Target);
float Distance = Navigation->GetPathLength(Start, End);

// Raycast on navmesh
FNavHitResult Hit;
bool Blocked = Navigation->Raycast(Start, End, Hit);
```

### Cover

```cpp
// Find cover
FCoverPoint* Cover = AI->FindCoverFrom(ThreatPosition);
FCoverPoint* Cover = AI->FindCoverNear(Position, Radius);

// Use cover
AI->MoveToCover(Cover);
bool InCover = AI->IsInCover();
AI->LeaveCover();
```

### Group Commands

```cpp
// Squad control
Squad->SetFormation(EFormation::Wedge);
Squad->MoveToPosition(TargetPosition);
Squad->AttackTarget(Target);
Squad->Regroup();

// Member coordination
Squad->AssignTarget(Member, Target);
Squad->RequestSupport(Position);
```

## Complete Example

```toml
# Level with AI enemies

[navigation]
navmesh = "navmesh/dungeon.nav"
agent_radius = 0.4
agent_height = 1.8
step_height = 0.3
max_slope = 45

[navigation.areas]
default = { cost = 1.0 }
water = { cost = 2.0 }
danger = { cost = 5.0 }

# Patrol paths
[[waypoint_paths]]
name = "guard_patrol_01"
loop = true

[[waypoint_paths.points]]
position = [10, 0, 10]
wait_time = 3.0
look_direction = [1, 0, 0]

[[waypoint_paths.points]]
position = [30, 0, 10]
wait_time = 0.0

[[waypoint_paths.points]]
position = [30, 0, 30]
wait_time = 3.0
look_direction = [0, 0, 1]

[[waypoint_paths.points]]
position = [10, 0, 30]
wait_time = 0.0

[[waypoint_paths]]
name = "guard_patrol_02"
loop = true

[[waypoint_paths.points]]
position = [50, 0, 50]
wait_time = 2.0

[[waypoint_paths.points]]
position = [70, 0, 50]
wait_time = 2.0

# Cover points
[[cover_points]]
position = [20, 0, 20]
normal = [0, 0, -1]
height = "low"
can_shoot_over = true

[[cover_points]]
position = [25, 0, 15]
normal = [1, 0, 0]
height = "medium"
can_shoot_around_left = true

# Enemy entities
[[entities]]
name = "guard_01"
mesh = "characters/guard.glb"

[entities.transform]
position = [10, 0, 10]

[entities.ai]
enabled = true
type = "state_machine"
initial_state = "patrol"

[entities.ai.states.patrol]
type = "patrol"
waypoints = "guard_patrol_01"
speed = 2.5
animation = "walk"

[entities.ai.states.alert]
type = "investigate"
speed = 3.0
animation = "walk_cautious"

[entities.ai.states.chase]
type = "chase"
target = "player"
speed = 5.0
animation = "run"
give_up_distance = 30.0

[entities.ai.states.attack]
type = "attack"
range = 15.0
cooldown = 0.5
animation = "shoot"
use_cover = true

[entities.ai.states.search]
type = "search"
duration = 15.0
search_radius = 10.0
animation = "look_around"

[[entities.ai.transitions]]
from = "patrol"
to = "alert"
condition = "heard_sound"

[[entities.ai.transitions]]
from = "patrol"
to = "chase"
condition = "can_see_player"

[[entities.ai.transitions]]
from = "alert"
to = "chase"
condition = "can_see_player"

[[entities.ai.transitions]]
from = "alert"
to = "search"
condition = "investigate_complete"

[[entities.ai.transitions]]
from = "chase"
to = "attack"
condition = "in_attack_range AND has_line_of_sight"

[[entities.ai.transitions]]
from = "attack"
to = "chase"
condition = "NOT in_attack_range OR NOT has_line_of_sight"

[[entities.ai.transitions]]
from = "chase"
to = "search"
condition = "lost_target"

[[entities.ai.transitions]]
from = "search"
to = "chase"
condition = "can_see_player"

[[entities.ai.transitions]]
from = "search"
to = "patrol"
condition = "search_timeout"

[entities.ai.senses.sight]
enabled = true
range = 25.0
angle = 90
detection_time = 0.3

[entities.ai.senses.hearing]
enabled = true
range = 20.0

[[entities.ai.senses.hearing.sounds]]
type = "footstep"
detection_range = 10.0

[[entities.ai.senses.hearing.sounds]]
type = "gunshot"
detection_range = 40.0

[entities.health]
max_health = 100

[entities.weapon]
equipped = "guard_rifle"

# Heavy enemy with behavior tree
[[entities]]
name = "heavy_01"
mesh = "characters/heavy.glb"

[entities.transform]
position = [50, 0, 50]

[entities.ai]
enabled = true
type = "behavior_tree"
tree = "ai/trees/heavy_enemy.bt"

[entities.ai.senses.sight]
enabled = true
range = 30.0
angle = 120
detection_time = 0.5

[entities.ai.senses.hearing]
enabled = true
range = 25.0

[entities.health]
max_health = 300

[entities.health.resistances]
physical = 0.7
fire = 0.5

[entities.weapon]
equipped = "minigun"

# Squad group
[[ai_groups]]
name = "patrol_squad"
formation = "line"
spacing = 3.0
leader = "guard_01"
members = ["guard_02", "guard_03"]

[ai_groups.behavior]
follow_leader = true
share_targets = true
coordinate_attacks = true
```
