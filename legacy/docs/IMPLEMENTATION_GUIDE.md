# Game Logic Implementation Guide

This guide provides a roadmap for implementing the game logic systems documented in `docs/input-formats/21-29`.

## Overview

The Void GUI v2 game logic system uses:

- **C++ Native Scripts**: High-performance game logic via FFI bindings
- **Blueprints**: Visual scripting built on `void_graph`
- **ECS Components**: Game state stored in archetype-based ECS
- **Event System**: Decoupled communication via `void_event`

## Architecture Decision: C++ + Blueprints

Instead of the custom VoidScript language, we follow Unreal Engine's proven model:

| Aspect | C++ | Blueprints |
|--------|-----|------------|
| Performance | Maximum | Good (interpreted) |
| Use Case | Core systems, AI, physics | Game logic, events, prototyping |
| Hot-Reload | Recompile DLL | Instant (asset reload) |
| Learning Curve | High | Low (visual) |

## Implementation Phases

### Phase 1: Foundation (Week 1-2)

#### 1.1 Create void_cpp Crate

```bash
cargo new --lib crates/void_cpp
```

Key components:
- `CppClass` trait for FFI interface
- `CppLibrary` for dynamic loading
- `CppClassRegistry` for instance management
- Hot-reload state preservation

#### 1.2 Enhance void_graph for Blueprints

Extend existing node system with:
- Event entry point nodes (BeginPlay, Tick, etc.)
- Flow control nodes (Branch, Sequence, Loop, etc.)
- Game logic nodes (SpawnActor, ApplyDamage, etc.)
- Variable get/set nodes

Create `BlueprintComponent` for entity attachment.

### Phase 2: Core Systems (Week 2-4)

#### 2.1 Physics System

Files to create:
- `crates/void_ecs/src/physics/mod.rs`
- `crates/void_ecs/src/physics/collider.rs`
- `crates/void_ecs/src/physics/rigidbody.rs`
- `crates/void_ecs/src/physics/joints.rs`
- `crates/void_ecs/src/physics/layers.rs`

Integration:
- Use Rapier 3D for physics simulation
- Sync transforms between ECS and physics world
- Emit collision events through `void_event`

#### 2.2 Trigger System

Files to create:
- `crates/void_ecs/src/triggers/mod.rs`
- `crates/void_ecs/src/triggers/volume.rs`
- `crates/void_ecs/src/triggers/events.rs`

Features:
- Shape types (box, sphere, capsule, mesh)
- Layer mask and tag filtering
- One-shot and cooldown behavior

#### 2.3 State System

Files to create:
- `crates/void_ecs/src/state/mod.rs`
- `crates/void_ecs/src/state/variables.rs`
- `crates/void_ecs/src/state/entity_state.rs`
- `crates/void_services/src/save_system.rs`

Features:
- Typed variable system
- Save/load serialization
- Checkpoint management

### Phase 3: Gameplay Systems (Week 4-6)

#### 3.1 Combat System

Files to create:
- `crates/void_ecs/src/combat/mod.rs`
- `crates/void_ecs/src/combat/health.rs`
- `crates/void_ecs/src/combat/damage.rs`
- `crates/void_ecs/src/combat/weapons.rs`
- `crates/void_ecs/src/combat/projectiles.rs`
- `crates/void_ecs/src/combat/status_effects.rs`

Features:
- Health with resistances and armor
- Damage types enum
- Weapon types (hitscan, projectile, melee)
- Status effects (DOT, CC, buffs)

#### 3.2 Inventory System

Files to create:
- `crates/void_ecs/src/inventory/mod.rs`
- `crates/void_ecs/src/inventory/items.rs`
- `crates/void_ecs/src/inventory/container.rs`
- `crates/void_ecs/src/inventory/equipment.rs`
- `crates/void_ecs/src/inventory/pickups.rs`

Features:
- Item definitions with categories
- Inventory slots and weight
- Equipment system
- World pickups and loot tables

#### 3.3 Audio System

Files to modify/create:
- `crates/void_services/src/audio/mod.rs`
- `crates/void_ecs/src/audio/source.rs`
- `crates/void_ecs/src/audio/listener.rs`
- `crates/void_ecs/src/audio/zones.rs`

Features:
- Spatial audio with rolloff
- Music system with crossfade
- Sound groups and variations
- Reverb zones

### Phase 4: AI & UI (Week 6-8)

#### 4.1 AI System

Files to create:
- `crates/void_ecs/src/ai/mod.rs`
- `crates/void_ecs/src/ai/controller.rs`
- `crates/void_ecs/src/ai/state_machine.rs`
- `crates/void_ecs/src/ai/behavior_tree.rs`
- `crates/void_ecs/src/ai/senses.rs`
- `crates/void_services/src/navigation/mod.rs`
- `crates/void_services/src/navigation/navmesh.rs`
- `crates/void_services/src/navigation/pathfinding.rs`

Features:
- NavMesh integration
- State machine AI
- Behavior tree executor
- Sensory systems
- Squad/group AI

#### 4.2 UI System

Files to modify/create:
- `crates/void_ui/src/hud/mod.rs`
- `crates/void_ui/src/hud/elements.rs`
- `crates/void_ui/src/hud/binding.rs`
- `crates/void_ui/src/widgets/damage_numbers.rs`
- `crates/void_ui/src/widgets/nameplates.rs`

Features:
- HUD element types
- Data binding system
- Damage numbers
- Interaction prompts
- Nameplate system

## Scene Loader Integration

Update `crates/void_runtime/src/scene_loader.rs` to parse new TOML sections:

```rust
// Add parsing for new component types
fn parse_entity_physics(table: &toml::Table) -> Result<PhysicsComponent>;
fn parse_entity_health(table: &toml::Table) -> Result<HealthComponent>;
fn parse_entity_inventory(table: &toml::Table) -> Result<InventoryComponent>;
fn parse_entity_ai(table: &toml::Table) -> Result<AIComponent>;
fn parse_entity_audio(table: &toml::Table) -> Result<AudioComponent>;
fn parse_entity_state(table: &toml::Table) -> Result<StateComponent>;
fn parse_entity_cpp_class(table: &toml::Table) -> Result<CppClassComponent>;
fn parse_entity_blueprint(table: &toml::Table) -> Result<BlueprintComponent>;

// Add parsing for scene-level configs
fn parse_triggers(array: &toml::Array) -> Result<Vec<TriggerVolume>>;
fn parse_waypoint_paths(array: &toml::Array) -> Result<Vec<WaypointPath>>;
fn parse_weapons(array: &toml::Array) -> Result<Vec<WeaponDefinition>>;
fn parse_navigation(table: &toml::Table) -> Result<NavigationConfig>;
fn parse_audio_config(table: &toml::Table) -> Result<AudioConfig>;
fn parse_ui_config(table: &toml::Table) -> Result<UIConfig>;
fn parse_state_config(table: &toml::Table) -> Result<StateConfig>;
```

## Testing Strategy

### Unit Tests

Each system should have unit tests:

```rust
#[cfg(test)]
mod tests {
    #[test]
    fn test_damage_calculation() {
        let health = HealthComponent::new(100.0);
        let damage = DamageInfo::new(50.0, DamageType::Physical);
        let result = health.apply_damage(&damage);
        assert_eq!(result.final_damage, 50.0);
    }
}
```

### Integration Tests

Test system interactions:

```rust
#[test]
fn test_combat_flow() {
    let mut world = World::new();

    // Spawn attacker with weapon
    let attacker = world.spawn_with_weapon("laser_pistol");

    // Spawn target with health
    let target = world.spawn_with_health(100.0);

    // Fire weapon
    world.run_systems();

    // Verify damage applied
    assert!(world.get::<HealthComponent>(target).current < 100.0);
}
```

### Editor Integration Tests

Test TOML parsing:

```rust
#[test]
fn test_parse_combat_entity() {
    let toml = r#"
        [[entities]]
        name = "enemy"

        [entities.health]
        max_health = 100

        [entities.health.resistances]
        fire = 0.5
    "#;

    let scene = parse_scene(toml).unwrap();
    let enemy = &scene.entities[0];
    assert_eq!(enemy.health.as_ref().unwrap().max_health, 100.0);
}
```

## Documentation

Each system should have:

1. **Input format doc** (done: `docs/input-formats/21-29`)
2. **API reference** (generated via `cargo doc`)
3. **Example scenes** in `examples/game_logic/`

## Example Project Structure

```
examples/game_logic/
├── basic_combat/
│   ├── manifest.toml
│   ├── scene.toml
│   └── blueprints/
│       └── player_combat.bp
│
├── inventory_demo/
│   ├── manifest.toml
│   ├── scene.toml
│   └── items/
│       ├── health_potion.toml
│       └── sword.toml
│
├── ai_showcase/
│   ├── manifest.toml
│   ├── scene.toml
│   ├── navmesh/
│   │   └── level.nav
│   └── ai/
│       └── patrol_guard.bt
│
└── full_game/
    ├── manifest.toml
    ├── scene.toml
    ├── Source/
    │   └── PlayerController.cpp
    ├── Blueprints/
    │   └── BP_Enemy.bp
    └── Content/
        ├── items/
        ├── weapons/
        └── audio/
```

## Claude Skills Usage

Use the defined skills to implement each system:

```bash
# Implement physics
/implement-physics

# Implement combat
/implement-combat

# Implement inventory
/implement-inventory

# Implement AI
/implement-ai
```

See `.claude/skills/game-logic-implementation.md` for full skill definitions.

## Answers to Editor Team Questions

From `docs/GAME_LOGIC_REQUIREMENTS.md`:

1. **Scripting Language**: C++ native code via FFI + Blueprint visual scripting (replacing `.vs`)
2. **Execution Model**: Scripts attached per-entity via components; global scripts via GameMode
3. **Hot Reload**: C++ via DLL reload; Blueprints via asset hot-reload
4. **Performance**: C++ for hot paths; Blueprints batch-executed per-archetype
5. **Debugging**: Console logging, visual debugger, Blueprint breakpoints
6. **Physics Engine**: Rapier 3D
7. **Audio Engine**: rodio
8. **Networking**: Planned; client-server model with entity replication
