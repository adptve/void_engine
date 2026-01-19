# Physics System

The physics system provides rigidbody dynamics, collision detection, and physical materials using the Rapier physics engine.

## Basic Physics Entity

```toml
[[entities]]
name = "crate"
mesh = "crate.glb"

[entities.physics]
enabled = true
body_type = "dynamic"
mass = 10.0
```

## Physics Configuration

```toml
[[entities]]
name = "physics_object"
mesh = "object.glb"

[entities.physics]
enabled = true
body_type = "dynamic"            # static, dynamic, kinematic
mass = 10.0                      # Mass in kg (dynamic only)
gravity_scale = 1.0              # Multiplier for gravity
linear_damping = 0.0             # Air resistance
angular_damping = 0.05           # Rotational damping
continuous_detection = false     # CCD for fast objects

# Collider shape
[entities.physics.collider]
shape = "box"
size = [1, 1, 1]
offset = [0, 0, 0]
rotation = [0, 0, 0]
is_trigger = false               # Trigger vs solid collider

# Physical material
[entities.physics.material]
friction = 0.5                   # 0 = ice, 1 = rubber
restitution = 0.3                # Bounciness (0-1)
density = 1.0                    # Affects auto-computed mass

# Movement constraints
[entities.physics.constraints]
freeze_position = [false, false, false]  # X, Y, Z
freeze_rotation = [false, true, false]   # X, Y, Z

# Collision events
[entities.physics.events]
on_collision_enter = "HandleCollisionStart"
on_collision_stay = "HandleCollisionStay"
on_collision_exit = "HandleCollisionEnd"
```

## Body Types

| Type | Description | Use Case |
|------|-------------|----------|
| `static` | Immovable, infinite mass | Floors, walls, terrain |
| `dynamic` | Fully simulated | Crates, projectiles, ragdolls |
| `kinematic` | Script-controlled, affects dynamics | Moving platforms, elevators |

### Static Bodies

```toml
[[entities]]
name = "floor"
mesh = "plane"

[entities.physics]
body_type = "static"

[entities.physics.collider]
shape = "box"
size = [100, 1, 100]
offset = [0, -0.5, 0]
```

### Dynamic Bodies

```toml
[[entities]]
name = "ball"
mesh = "sphere"

[entities.physics]
body_type = "dynamic"
mass = 1.0
gravity_scale = 1.0

[entities.physics.collider]
shape = "sphere"
radius = 0.5

[entities.physics.material]
restitution = 0.9               # Very bouncy
friction = 0.1                  # Low friction
```

### Kinematic Bodies

```toml
[[entities]]
name = "moving_platform"
mesh = "platform.glb"

[entities.physics]
body_type = "kinematic"

[entities.physics.collider]
shape = "box"
size = [5, 0.5, 5]
```

## Collider Shapes

### Box

```toml
[entities.physics.collider]
shape = "box"
size = [2, 1, 3]                # Width, height, depth
offset = [0, 0.5, 0]            # Center offset
rotation = [0, 45, 0]           # Local rotation
```

### Sphere

```toml
[entities.physics.collider]
shape = "sphere"
radius = 1.0
offset = [0, 1, 0]
```

### Capsule

```toml
[entities.physics.collider]
shape = "capsule"
radius = 0.4
height = 1.8                    # Total height including caps
axis = "y"                      # Capsule orientation: x, y, z
offset = [0, 0.9, 0]
```

### Cylinder

```toml
[entities.physics.collider]
shape = "cylinder"
radius = 0.5
height = 2.0
axis = "y"
```

### Convex Hull

Auto-computed from mesh vertices:

```toml
[entities.physics.collider]
shape = "convex"
mesh = "assets/collision/object_convex.glb"
# OR compute from visual mesh:
compute_from_mesh = true
```

### Triangle Mesh

Exact mesh collision (static only):

```toml
[[entities]]
name = "terrain"
mesh = "terrain.glb"

[entities.physics]
body_type = "static"

[entities.physics.collider]
shape = "trimesh"
collision_mesh = "assets/collision/terrain_collision.glb"
# OR use visual mesh:
use_render_mesh = true
```

### Compound Collider

Multiple shapes for complex objects:

```toml
[[entities]]
name = "character"
mesh = "character.glb"

[entities.physics]
body_type = "dynamic"
mass = 70.0

[[entities.physics.colliders]]
shape = "capsule"
radius = 0.3
height = 1.6
offset = [0, 0.8, 0]
name = "body"

[[entities.physics.colliders]]
shape = "sphere"
radius = 0.15
offset = [0, 1.7, 0]
name = "head"
```

## Collider Shape Reference

| Shape | Parameters |
|-------|------------|
| `box` | `size = [x, y, z]` |
| `sphere` | `radius = float` |
| `capsule` | `radius`, `height`, `axis` |
| `cylinder` | `radius`, `height`, `axis` |
| `convex` | `mesh` or `compute_from_mesh` |
| `trimesh` | `collision_mesh` or `use_render_mesh` |

## Physical Materials

```toml
[entities.physics.material]
friction = 0.5                   # Coefficient of friction
restitution = 0.0                # Bounciness (0 = no bounce, 1 = perfect bounce)
density = 1.0                    # kg/m³, used for auto mass calculation
friction_combine = "average"     # average, min, max, multiply
restitution_combine = "average"  # average, min, max, multiply
```

### Material Presets

```toml
# Ice
[entities.physics.material]
friction = 0.02
restitution = 0.1

# Rubber
[entities.physics.material]
friction = 0.9
restitution = 0.8

# Metal
[entities.physics.material]
friction = 0.4
restitution = 0.3
density = 7800.0

# Wood
[entities.physics.material]
friction = 0.5
restitution = 0.2
density = 600.0

# Concrete
[entities.physics.material]
friction = 0.6
restitution = 0.0
density = 2400.0
```

## Collision Layers

Define which objects can collide with each other:

```toml
[physics]
gravity = [0, -9.81, 0]         # World gravity

# Define layer names
layers = ["default", "player", "enemies", "projectiles", "triggers", "terrain"]

# Collision matrix - which layers interact
[physics.collision_matrix]
default = ["default", "player", "enemies", "projectiles", "terrain"]
player = ["default", "enemies", "projectiles", "triggers", "terrain"]
enemies = ["default", "player", "projectiles", "terrain"]
projectiles = ["default", "enemies", "terrain"]
triggers = ["player", "enemies"]
terrain = ["default", "player", "enemies", "projectiles"]
```

### Assigning Layers

```toml
[[entities]]
name = "player"

[entities.physics]
layer = "player"

[[entities]]
name = "bullet"

[entities.physics]
layer = "projectiles"
```

## Collision Events

```toml
[entities.physics.events]
on_collision_enter = "OnCollisionStart"
on_collision_stay = "OnCollisionOngoing"
on_collision_exit = "OnCollisionEnd"
```

Event parameters:

| Event | Parameters |
|-------|------------|
| `on_collision_enter` | `self: Entity, other: Entity, point: Vec3, normal: Vec3, impulse: float` |
| `on_collision_stay` | `self: Entity, other: Entity, point: Vec3, normal: Vec3` |
| `on_collision_exit` | `self: Entity, other: Entity` |

## Constraints

### Position Constraints

Lock movement on specific axes:

```toml
[entities.physics.constraints]
freeze_position = [false, false, false]  # [X, Y, Z]
```

### Rotation Constraints

Lock rotation on specific axes:

```toml
[entities.physics.constraints]
freeze_rotation = [true, false, true]   # Lock X and Z rotation
```

### 2D Mode

Constrain to XY plane:

```toml
[entities.physics.constraints]
freeze_position = [false, false, true]  # Lock Z
freeze_rotation = [true, true, false]   # Only Z rotation
```

## Joints

Connect physics bodies together:

### Fixed Joint

Rigid connection between bodies:

```toml
[[joints]]
name = "weld"
type = "fixed"
body_a = "object_a"
body_b = "object_b"
anchor_a = [0, 0, 0]
anchor_b = [0, 0, 0]
break_force = 1000.0             # Optional: break threshold
```

### Hinge Joint

Single-axis rotation:

```toml
[[joints]]
name = "door_hinge"
type = "hinge"
body_a = "door_frame"
body_b = "door"
anchor_a = [0.5, 1, 0]
anchor_b = [-0.5, 1, 0]
axis = [0, 1, 0]                 # Rotation axis

[joints.limits]
enabled = true
min_angle = 0                    # Degrees
max_angle = 90

[joints.motor]
enabled = false
target_velocity = 0
max_force = 100
```

### Ball Joint

Spherical connection:

```toml
[[joints]]
name = "ragdoll_shoulder"
type = "ball"
body_a = "torso"
body_b = "upper_arm"
anchor_a = [0.3, 0.4, 0]
anchor_b = [0, 0.2, 0]

[joints.limits]
cone_angle = 60                  # Max angle from axis
twist_min = -30
twist_max = 30
```

### Slider Joint

Linear motion along axis:

```toml
[[joints]]
name = "piston"
type = "slider"
body_a = "cylinder"
body_b = "piston_head"
axis = [0, 1, 0]

[joints.limits]
enabled = true
min_distance = 0
max_distance = 2.0

[joints.motor]
enabled = true
target_position = 1.0
max_force = 500
```

### Spring Joint

Elastic connection:

```toml
[[joints]]
name = "bungee"
type = "spring"
body_a = "anchor_point"
body_b = "hanging_object"
anchor_a = [0, 0, 0]
anchor_b = [0, 2, 0]
rest_length = 3.0
stiffness = 100.0               # Spring constant
damping = 10.0                  # Oscillation damping
```

### Distance Joint

Fixed distance constraint:

```toml
[[joints]]
name = "rope"
type = "distance"
body_a = "hook"
body_b = "weight"
distance = 5.0
compliance = 0.0                # 0 = rigid, higher = elastic
```

## Raycasting

Configure raycast queries:

```toml
[physics.raycast]
max_distance = 1000.0
layer_mask = ["default", "terrain", "enemies"]
ignore_triggers = true
```

Raycast in scripts:

```cpp
FHitResult Hit;
if (Physics->Raycast(Origin, Direction, MaxDistance, Hit, LayerMask)) {
    Entity* HitEntity = Hit.Entity;
    FVector HitPoint = Hit.Point;
    FVector HitNormal = Hit.Normal;
}
```

Blueprint: Use `Raycast` node.

## Overlap Queries

Test for overlapping bodies:

```cpp
TArray<Entity*> Hits = Physics->OverlapSphere(Center, Radius, LayerMask);
TArray<Entity*> Hits = Physics->OverlapBox(Center, HalfExtents, Rotation, LayerMask);
TArray<Entity*> Hits = Physics->OverlapCapsule(Center, Radius, Height, LayerMask);
```

## Physics Scripting API

### Apply Forces

```cpp
// Force applied over time (acceleration)
Entity->AddForce(FVector(0, 100, 0));
Entity->AddForceAtPosition(Force, WorldPosition);

// Impulse (instant velocity change)
Entity->AddImpulse(FVector(0, 500, 0));
Entity->AddImpulseAtPosition(Impulse, WorldPosition);

// Torque
Entity->AddTorque(FVector(0, 10, 0));
Entity->AddTorqueImpulse(FVector(0, 50, 0));
```

### Velocity Control

```cpp
// Linear velocity
FVector Velocity = Entity->GetLinearVelocity();
Entity->SetLinearVelocity(FVector(5, 0, 0));

// Angular velocity
FVector AngularVel = Entity->GetAngularVelocity();
Entity->SetAngularVelocity(FVector(0, 3.14, 0));
```

### Transform

```cpp
FVector Position = Entity->GetPosition();
Entity->SetPosition(NewPosition);

// Teleport (no interpolation)
Entity->TeleportTo(NewPosition, NewRotation);
```

### Kinematic Movement

```cpp
// For kinematic bodies
Entity->MoveKinematic(TargetPosition, TargetRotation, DeltaTime);
```

## Character Controller

Special physics for player characters:

```toml
[[entities]]
name = "player"

[entities.character_controller]
enabled = true
height = 1.8
radius = 0.4
step_height = 0.3               # Max step climb
slope_limit = 45                # Max walkable slope (degrees)
skin_width = 0.08               # Collision skin
gravity_scale = 1.0

[entities.character_controller.ground_check]
distance = 0.1
layer_mask = ["terrain", "default"]
```

Character controller scripting:

```cpp
// Movement
Character->Move(Direction * Speed * DeltaTime);

// Jump
if (Character->IsGrounded()) {
    Character->AddImpulse(FVector(0, JumpForce, 0));
}

// Ground check
bool OnGround = Character->IsGrounded();
FVector GroundNormal = Character->GetGroundNormal();
```

## Physics World Settings

```toml
[physics]
gravity = [0, -9.81, 0]
fixed_timestep = 0.016666       # 60 Hz physics
max_substeps = 4                # Max physics steps per frame
solver_iterations = 4           # Constraint solver iterations
enable_ccd = true               # Continuous collision detection
```

## Complete Example

```toml
# Scene with various physics objects

[physics]
gravity = [0, -9.81, 0]
layers = ["default", "player", "enemies", "projectiles", "triggers", "terrain"]

[physics.collision_matrix]
default = ["default", "terrain"]
player = ["default", "enemies", "terrain", "triggers"]
enemies = ["default", "player", "terrain"]
projectiles = ["enemies", "terrain"]
terrain = ["default", "player", "enemies", "projectiles"]

# Static floor
[[entities]]
name = "floor"
mesh = "plane"
[entities.transform]
scale = 50.0
[entities.physics]
body_type = "static"
layer = "terrain"
[entities.physics.collider]
shape = "box"
size = [100, 1, 100]
offset = [0, -0.5, 0]

# Dynamic crate
[[entities]]
name = "crate"
mesh = "crate.glb"
[entities.transform]
position = [0, 5, 0]
[entities.physics]
body_type = "dynamic"
mass = 20.0
layer = "default"
[entities.physics.collider]
shape = "box"
size = [1, 1, 1]
[entities.physics.material]
friction = 0.5
restitution = 0.1
[entities.physics.events]
on_collision_enter = "OnCrateHit"

# Bouncy ball
[[entities]]
name = "ball"
mesh = "sphere"
[entities.transform]
position = [3, 8, 0]
[entities.physics]
body_type = "dynamic"
mass = 1.0
[entities.physics.collider]
shape = "sphere"
radius = 0.5
[entities.physics.material]
friction = 0.1
restitution = 0.9

# Player character
[[entities]]
name = "player"
mesh = "character.glb"
[entities.transform]
position = [-5, 1, 0]
[entities.character_controller]
enabled = true
height = 1.8
radius = 0.4
step_height = 0.3
slope_limit = 45
[entities.physics]
layer = "player"

# Moving platform
[[entities]]
name = "platform"
mesh = "platform.glb"
[entities.transform]
position = [10, 2, 0]
[entities.physics]
body_type = "kinematic"
layer = "terrain"
[entities.physics.collider]
shape = "box"
size = [4, 0.5, 4]
[entities.animation]
type = "path"
points = [[10, 2, 0], [10, 8, 0]]
duration = 4.0
loop = "ping_pong"

# Swinging pendulum
[[entities]]
name = "pendulum_anchor"
[entities.transform]
position = [15, 10, 0]
[entities.physics]
body_type = "static"

[[entities]]
name = "pendulum_weight"
mesh = "sphere"
[entities.transform]
position = [15, 5, 0]
scale = 1.5
[entities.physics]
body_type = "dynamic"
mass = 50.0
[entities.physics.collider]
shape = "sphere"
radius = 0.75
[entities.physics.material]
restitution = 0.5

[[joints]]
name = "pendulum_joint"
type = "distance"
body_a = "pendulum_anchor"
body_b = "pendulum_weight"
distance = 5.0

# Door with hinge
[[entities]]
name = "door_frame"
[entities.transform]
position = [20, 0, 0]
[entities.physics]
body_type = "static"

[[entities]]
name = "door"
mesh = "door.glb"
[entities.transform]
position = [20.5, 1.5, 0]
[entities.physics]
body_type = "dynamic"
mass = 30.0
[entities.physics.collider]
shape = "box"
size = [1, 3, 0.1]

[[joints]]
name = "door_hinge"
type = "hinge"
body_a = "door_frame"
body_b = "door"
anchor_a = [0, 1.5, 0]
anchor_b = [-0.5, 0, 0]
axis = [0, 1, 0]
[joints.limits]
enabled = true
min_angle = 0
max_angle = 120
```
