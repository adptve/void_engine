# C++ and Blueprint Scripting Architecture

This document describes the architecture for Void GUI v2's game logic system using C++ native code and Blueprint visual scripting (inspired by Unreal Engine).

## Overview

Void GUI v2 replaces VoidScript with a dual-scripting approach:

1. **C++ Native Scripts**: High-performance native code for complex game logic
2. **Blueprints**: Visual node-based scripting built on `void_graph`

Both systems integrate with the existing ECS (`void_ecs`), event system (`void_event`), and kernel (`void_kernel`).

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Game Logic Layer                                │
├─────────────────────────────────┬───────────────────────────────────────────┤
│         C++ Scripts             │              Blueprints                    │
│   ┌─────────────────────┐       │       ┌─────────────────────────┐         │
│   │  GameplayActor.cpp  │       │       │  Blueprint Graph (.bp)  │         │
│   │  AIController.cpp   │       │       │  ┌─────┐  ┌─────┐      │         │
│   │  WeaponSystem.cpp   │       │       │  │Event│─▶│Logic│──▶   │         │
│   └──────────┬──────────┘       │       │  └─────┘  └─────┘      │         │
│              │                  │       └───────────┬─────────────┘         │
│              ▼                  │                   ▼                       │
│   ┌─────────────────────┐       │       ┌─────────────────────────┐         │
│   │  void_cpp (FFI)     │       │       │  void_graph (Executor)  │         │
│   │  - CppClass trait   │       │       │  - NodeRegistry         │         │
│   │  - CppLibrary       │       │       │  - GraphExecutor        │         │
│   │  - Hot-reload       │       │       │  - BlueprintComponent   │         │
│   └──────────┬──────────┘       │       └───────────┬─────────────┘         │
│              │                  │                   │                       │
└──────────────┼──────────────────┴───────────────────┼───────────────────────┘
               │                                      │
               ▼                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Engine Services Layer                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  void_ecs    │  │  void_event  │  │  void_ir     │  │  void_asset  │    │
│  │  (World)     │  │  (EventBus)  │  │  (Patches)   │  │  (Resources) │    │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                             void_kernel (Core)                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

## C++ Native Scripts

### Design Philosophy

Like Unreal Engine, C++ provides the performance-critical foundation:

- **Base Classes**: `VoidActor`, `VoidComponent`, `VoidGameMode`
- **Reflection**: Macro-based property system for editor exposure
- **Hot-Reload**: Compile changes without restarting via dynamic libraries
- **Memory Safety**: Rust manages lifetime, C++ handles logic

### Core C++ Classes

```cpp
// VoidActor.h - Base class for all game entities
#pragma once
#include "void_api.h"

class VOID_API VoidActor {
public:
    VoidActor();
    virtual ~VoidActor();

    // Lifecycle
    virtual void BeginPlay();
    virtual void Tick(float DeltaTime);
    virtual void EndPlay();

    // Events
    virtual void OnCollisionEnter(VoidActor* Other, const FHitResult& Hit);
    virtual void OnCollisionExit(VoidActor* Other);
    virtual void OnTriggerEnter(VoidActor* Other);
    virtual void OnTriggerExit(VoidActor* Other);
    virtual void OnDamage(float Amount, EDamageType Type, VoidActor* Source);
    virtual void OnDeath(VoidActor* Killer);

    // Transform
    FVector GetPosition() const;
    void SetPosition(const FVector& Position);
    FQuat GetRotation() const;
    void SetRotation(const FQuat& Rotation);

    // Components
    template<typename T>
    T* AddComponent();
    template<typename T>
    T* GetComponent();

    // Entity ID (links to ECS)
    uint64_t GetEntityId() const { return m_EntityId; }

protected:
    uint64_t m_EntityId;
    class VoidWorld* m_World;
};

// VoidComponent.h - Base class for components
class VOID_API VoidComponent {
public:
    virtual void OnAttach(VoidActor* Owner);
    virtual void OnDetach();
    virtual void Tick(float DeltaTime);

    VoidActor* GetOwner() const { return m_Owner; }

protected:
    VoidActor* m_Owner;
};
```

### FFI Layer (void_cpp)

The Rust crate `void_cpp` provides FFI bindings:

```rust
// crates/void_cpp/src/lib.rs

/// Trait for C++ classes exposed to Rust
pub trait CppClass: Send + Sync {
    fn class_name() -> &'static str;
    fn begin_play(&mut self, entity_id: u64);
    fn tick(&mut self, delta_time: f32);
    fn end_play(&mut self);
}

/// Dynamic library loader for hot-reload
pub struct CppLibrary {
    library: libloading::Library,
    classes: HashMap<String, CppClassFactory>,
    version: u32,
}

impl CppLibrary {
    pub fn load(path: &Path) -> Result<Self, CppError>;
    pub fn reload(&mut self) -> Result<(), CppError>;
    pub fn create_instance(&self, class_name: &str) -> Result<Box<dyn CppClass>, CppError>;
}

/// Registry of all C++ classes
pub struct CppClassRegistry {
    libraries: HashMap<PathBuf, CppLibrary>,
    instances: HashMap<EntityId, Box<dyn CppClass>>,
}
```

### TOML Configuration

```toml
# Entity with C++ class
[[entities]]
name = "player"
mesh = "character.glb"

[entities.cpp_class]
library = "game_logic.dll"       # Compiled C++ library
class = "PlayerController"        # Class name
hot_reload = true                 # Enable hot-reload

# Properties exposed to editor (via reflection)
[entities.cpp_class.properties]
max_health = 100.0
move_speed = 5.0
jump_force = 10.0
```

## Blueprint Visual Scripting

### Built on void_graph

Blueprints extend the existing `void_graph` system:

```rust
// Extension to void_graph for Blueprint functionality

/// Blueprint graph stored per-entity
pub struct BlueprintComponent {
    pub graph_asset: AssetId,
    pub variables: HashMap<String, Value>,
    pub enabled: bool,
}

/// Blueprint-specific node categories
pub enum BlueprintNodeCategory {
    // Events (entry points)
    EventBeginPlay,
    EventTick,
    EventCollision,
    EventTrigger,
    EventInput,
    EventDamage,
    EventCustom(String),

    // Flow Control
    Branch,
    Sequence,
    ForLoop,
    ForEachLoop,
    WhileLoop,
    DoOnce,
    Gate,
    MultiGate,
    Delay,

    // Entity Operations
    SpawnActor,
    DestroyActor,
    GetActorLocation,
    SetActorLocation,
    GetActorRotation,
    SetActorRotation,
    GetComponent,
    AddComponent,

    // Physics
    AddForce,
    AddImpulse,
    SetVelocity,
    Raycast,
    OverlapSphere,

    // Combat
    ApplyDamage,
    GetHealth,
    SetHealth,
    SpawnProjectile,

    // Inventory
    AddItem,
    RemoveItem,
    HasItem,
    GetItemCount,
    EquipItem,

    // Audio
    PlaySound,
    PlaySoundAtLocation,
    StopSound,
    SetMusicTrack,

    // UI
    ShowWidget,
    HideWidget,
    SetText,
    SetProgressBar,

    // State
    GetVariable,
    SetVariable,
    SaveGame,
    LoadGame,

    // AI
    MoveTo,
    SetAIState,
    GetAIState,
    FindPath,

    // Math (inherited from void_graph)
    // Add, Subtract, Multiply, etc.
}
```

### TOML Configuration

```toml
# Entity with Blueprint
[[entities]]
name = "door"
mesh = "door.glb"

[entities.blueprint]
graph = "blueprints/interactive_door.bp"
enabled = true

# Variables exposed to editor
[entities.blueprint.variables]
is_locked = { type = "bool", default = true }
required_key = { type = "string", default = "gold_key" }
open_speed = { type = "float", default = 2.0 }

# Event bindings
[entities.blueprint.events]
on_interact = "OnPlayerInteract"
on_trigger_enter = "OnTriggerEnter"
```

### Blueprint File Format (.bp)

```toml
# blueprints/interactive_door.bp
[blueprint]
name = "InteractiveDoor"
version = "1.0.0"

# Variables
[[variables]]
name = "is_locked"
type = "bool"
default = true
exposed = true
category = "State"

[[variables]]
name = "is_open"
type = "bool"
default = false

[[variables]]
name = "open_speed"
type = "float"
default = 2.0
exposed = true
category = "Config"

# Nodes
[[nodes]]
id = 1
type = "EventOnInteract"
position = [100, 100]

[[nodes]]
id = 2
type = "GetVariable"
position = [300, 100]
[nodes.pins]
variable_name = "is_locked"

[[nodes]]
id = 3
type = "Branch"
position = [500, 100]

[[nodes]]
id = 4
type = "PlaySound"
position = [700, 50]
[nodes.pins]
sound = "sounds/door_locked.wav"

[[nodes]]
id = 5
type = "SetVariable"
position = [700, 150]
[nodes.pins]
variable_name = "is_open"
value = true

[[nodes]]
id = 6
type = "PlayAnimation"
position = [900, 150]
[nodes.pins]
animation = "door_open"
speed = { variable = "open_speed" }

# Connections (exec flow = white, data = colored by type)
[[connections]]
from = { node = 1, pin = "exec_out" }
to = { node = 2, pin = "exec_in" }

[[connections]]
from = { node = 2, pin = "exec_out" }
to = { node = 3, pin = "exec_in" }

[[connections]]
from = { node = 2, pin = "value" }
to = { node = 3, pin = "condition" }

[[connections]]
from = { node = 3, pin = "true" }
to = { node = 4, pin = "exec_in" }

[[connections]]
from = { node = 3, pin = "false" }
to = { node = 5, pin = "exec_in" }

[[connections]]
from = { node = 5, pin = "exec_out" }
to = { node = 6, pin = "exec_in" }
```

## Game Logic Components

### Component Types

All game systems are implemented as ECS components with C++ and Blueprint support:

| System | Component | C++ Class | Blueprint Nodes |
|--------|-----------|-----------|-----------------|
| Physics | `PhysicsComponent` | `VoidPhysicsBody` | AddForce, SetVelocity, Raycast |
| Combat | `HealthComponent`, `WeaponComponent` | `VoidCombatant`, `VoidWeapon` | ApplyDamage, GetHealth |
| Inventory | `InventoryComponent` | `VoidInventory` | AddItem, RemoveItem |
| Audio | `AudioComponent` | `VoidAudioSource` | PlaySound, SetVolume |
| AI | `AIComponent` | `VoidAIController` | MoveTo, SetState |
| Triggers | `TriggerComponent` | `VoidTriggerVolume` | OnEnter, OnExit |
| State | `StateComponent` | `VoidStateMachine` | GetVar, SetVar |

### Execution Order

```
1. Input Processing
   └─ Read input state, dispatch input events

2. Physics Pre-Solve
   └─ Collision detection, trigger events

3. C++ Tick
   └─ All C++ class Tick() methods

4. Blueprint Execution
   └─ EventTick nodes, queued events

5. Physics Solve
   └─ Integrate velocities, resolve constraints

6. Late Update
   └─ Transform synchronization

7. Render
   └─ ECS queries, GPU submission
```

## Hot-Reload Architecture

### C++ Hot-Reload

```rust
pub struct HotReloadManager {
    watcher: FileWatcher,
    pending_reloads: Vec<PathBuf>,
}

impl HotReloadManager {
    pub fn check_for_changes(&mut self) {
        for path in self.watcher.changed_files() {
            if path.extension() == Some("dll") || path.extension() == Some("so") {
                self.pending_reloads.push(path);
            }
        }
    }

    pub fn apply_reloads(&mut self, registry: &mut CppClassRegistry) {
        for path in self.pending_reloads.drain(..) {
            // 1. Serialize instance state
            let states = registry.serialize_instances(&path);

            // 2. Unload old library
            registry.unload_library(&path);

            // 3. Load new library
            registry.load_library(&path)?;

            // 4. Recreate instances with saved state
            registry.restore_instances(states);
        }
    }
}
```

### Blueprint Hot-Reload

Blueprints hot-reload automatically via the asset system:

1. File watcher detects `.bp` file change
2. Asset server reloads graph definition
3. Running `GraphExecutor` picks up changes next frame
4. Variables and execution state preserved

## Security Model

Using `void_kernel` capabilities:

```rust
pub enum ScriptCapability {
    // Entity operations
    SpawnEntities { max_per_frame: u32 },
    DestroyEntities,
    ModifyTransforms,

    // Physics
    ApplyForces,
    CreateColliders,

    // Audio
    PlaySounds { spatial_only: bool },
    ControlMusic,

    // State
    ReadGameState,
    WriteGameState,
    SaveLoad,

    // Networking
    SendMessages { rate_limit: u32 },
    ReceiveMessages,

    // File System (restricted)
    ReadAssets { paths: Vec<PathBuf> },
}
```

## Editor Integration

### Property Exposure

Both C++ and Blueprints expose properties to the editor:

```cpp
// C++ using macros
VOID_CLASS(PlayerController, VoidActor)
    VOID_PROPERTY(float, MaxHealth, Category="Stats", Min=1, Max=1000)
    VOID_PROPERTY(float, MoveSpeed, Category="Movement")
    VOID_PROPERTY(FVector, SpawnOffset, Category="Spawn")
VOID_CLASS_END()
```

```toml
# Blueprint variables with metadata
[[variables]]
name = "max_health"
type = "float"
default = 100.0
exposed = true
category = "Stats"
min = 1.0
max = 1000.0
tooltip = "Maximum health points"
```

### Event Autocomplete

The editor receives a list of all available events:

```rust
pub fn get_available_events() -> Vec<EventSignature> {
    vec![
        EventSignature::new("OnBeginPlay", &[]),
        EventSignature::new("OnTick", &[("DeltaTime", ValueType::Float)]),
        EventSignature::new("OnCollisionEnter", &[
            ("Other", ValueType::Entity),
            ("HitPoint", ValueType::Vec3),
            ("HitNormal", ValueType::Vec3),
        ]),
        EventSignature::new("OnTriggerEnter", &[("Other", ValueType::Entity)]),
        EventSignature::new("OnDamage", &[
            ("Amount", ValueType::Float),
            ("Type", ValueType::String),
            ("Source", ValueType::Entity),
        ]),
        // ... more events
    ]
}
```

## Performance Considerations

1. **C++ for Hot Paths**: AI pathfinding, physics queries, complex math
2. **Blueprints for Logic**: Event handling, game flow, UI
3. **Batched Execution**: Blueprint nodes batched per-archetype
4. **Async Nodes**: Long operations (pathfinding, loading) are async
5. **Profile Markers**: Both C++ and Blueprint emit profile scopes

## Migration from VoidScript

| VoidScript | C++ Equivalent | Blueprint Equivalent |
|------------|----------------|---------------------|
| `spawn()` | `World->SpawnActor<T>()` | SpawnActor node |
| `set_position()` | `SetActorLocation()` | SetActorLocation node |
| `emit_event()` | `BroadcastEvent()` | Custom Event node |
| `on_hit()` | `OnDamage()` override | EventOnDamage node |

## File Structure

```
game/
├── Source/                      # C++ source files
│   ├── Player/
│   │   ├── PlayerController.h
│   │   ├── PlayerController.cpp
│   │   └── PlayerAnimInstance.cpp
│   ├── Enemies/
│   │   ├── EnemyBase.h
│   │   └── EnemyBase.cpp
│   └── Weapons/
│       ├── Weapon.h
│       └── ProjectileBase.cpp
│
├── Blueprints/                  # Blueprint graphs
│   ├── Player/
│   │   └── BP_PlayerController.bp
│   ├── Enemies/
│   │   ├── BP_EnemyPatrol.bp
│   │   └── BP_EnemyChase.bp
│   └── Interactables/
│       ├── BP_Door.bp
│       └── BP_Pickup.bp
│
├── Content/                     # Assets
│   ├── Models/
│   ├── Textures/
│   └── Sounds/
│
└── Config/
    ├── manifest.toml
    └── scene.toml
```
