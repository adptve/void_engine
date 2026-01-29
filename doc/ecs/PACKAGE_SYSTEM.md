# void_engine Package System

> **Document Version**: 1.1
> **Last Updated**: 2026-01-29
> **Scope**: Complete documentation of the package system architecture for worlds, layers, plugins, widgets, and asset bundles

---

## Table of Contents

1. [Overview](#1-overview)
2. [world.package](#2-worldpackage)
3. [layer.package](#3-layerpackage)
4. [plugin.package](#4-pluginpackage)
5. [widget.package](#5-widgetpackage)
6. [asset.bundle](#6-assetbundle)
7. [Package Dependencies](#7-package-dependencies)
8. [Components and Contracts](#8-components-and-contracts)
9. [Creator / Mod Package Patterns](#9-creator--mod-package-patterns)
10. [Loading Order](#10-loading-order)

---

## 1. Overview

The void_engine package system organizes game content and functionality into five distinct package types, each with clear responsibilities:

| Package Type | Purpose | Contains |
|--------------|---------|----------|
| `world.package` | Whole game mode / level configuration | Scene, plugins, layers, spawn rules |
| `layer.package` | Patch / variant on top of a world | Additive content, overrides, variants |
| `plugin.package` | Systems, components, logic | ECS systems, event handlers, game logic |
| `widget.package` | UI / tooling / overlays | Debug HUDs, inspectors, profilers |
| `asset.bundle` | Pure content: data, no behaviour | Models, textures, audio, prefabs |

**Key Principle:** Packages are composable. A world references layers, plugins, widgets, and asset bundles. Layers can reference additional assets. Plugins define behaviour that operates on data from asset bundles.

```
                    ┌─────────────────┐
                    │  world.package  │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐   ┌────────────────┐   ┌────────────────┐
│ layer.package │   │ plugin.package │   │ widget.package │
└───────┬───────┘   └────────┬───────┘   └────────────────┘
        │                    │
        └──────────┬─────────┘
                   ▼
           ┌──────────────┐
           │ asset.bundle │
           └──────────────┘
```

---

## 2. world.package

**Purpose:** "Whole game mode / level configuration"

A world.package represents a complete, playable game state configuration. It defines what scene loads, which systems run, and how gameplay is configured.

### 2.1 Root Scene Definition

The main scene or map that gets instantiated into the ECS world:

- Spawn points
- Static geometry
- Initial entities
- World bounds

```json
{
  "root_scene": {
    "path": "scenes/arena_01.scene.json",
    "spawn_points": ["spawn_alpha", "spawn_beta", "spawn_gamma"],
    "world_bounds": {
      "min": [-500, -50, -500],
      "max": [500, 200, 500]
    }
  }
}
```

### 2.2 Active Layers

References to `layer.package` entries that should be applied:

- Lighting variants
- Fog configurations
- Event-specific dressing
- Seasonal content

```json
{
  "layers": [
    "layers/night_lighting.layer.json",
    "layers/winter_event.layer.json",
    "layers/competitive_mode.layer.json"
  ]
}
```

### 2.3 Core Gameplay Plugins

Which `plugin.package` entries are required for this world:

- Movement systems
- Combat systems
- AI systems
- UI systems

```json
{
  "plugins": [
    "plugins/core_movement.plugin.json",
    "plugins/fps_combat.plugin.json",
    "plugins/enemy_ai.plugin.json",
    "plugins/game_hud.plugin.json"
  ]
}
```

### 2.4 Widget/Debug Overlays

Which `widget.package` entries should be active:

- Debug HUD
- Profiler
- Entity inspector
- Console

```json
{
  "widgets": [
    "widgets/debug_hud.widget.json",
    "widgets/profiler.widget.json"
  ],
  "widgets_dev_only": [
    "widgets/entity_inspector.widget.json",
    "widgets/console.widget.json"
  ]
}
```

### 2.5 Required Asset Bundles

Asset packages that must be loaded for this world to function:

- Maps
- Characters
- Common props
- Shared effects

```json
{
  "asset_bundles": [
    "assets/core_characters.bundle.json",
    "assets/arena_01_props.bundle.json",
    "assets/weapons_tier1.bundle.json",
    "assets/common_vfx.bundle.json"
  ]
}
```

### 2.6 Spawn Rules / Player Entry Configuration

Configuration for player entity instantiation:

- Prefab to use for player entities
- Spawn location selection
- Initial inventory
- Starting stats

```json
{
  "player_spawn": {
    "prefab": "prefabs/player_fps.prefab.json",
    "spawn_selection": "round_robin",
    "initial_inventory": {
      "weapon_slot_1": "weapons/pistol_default",
      "armor": "armor/light_vest"
    },
    "initial_stats": {
      "health": 100,
      "armor": 25,
      "stamina": 100
    }
  }
}
```

### 2.7 World-Level Environment Settings

Global environmental configuration:

- Time-of-day
- Skybox choice
- Weather profile
- Global post-process profile

```json
{
  "environment": {
    "time_of_day": 14.5,
    "skybox": "skyboxes/overcast_day",
    "weather": {
      "profile": "weather/light_rain",
      "intensity": 0.6
    },
    "post_process": {
      "profile": "post/cinematic_default",
      "exposure": 1.2,
      "bloom_intensity": 0.4
    }
  }
}
```

### 2.8 Global Gameplay Settings

Match and ruleset configuration:

- Difficulty
- Ruleset switches
- Match length
- Score limits
- Friendly fire flags

```json
{
  "gameplay": {
    "difficulty": "normal",
    "match_length_seconds": 600,
    "score_limit": 50,
    "friendly_fire": false,
    "respawn_delay_seconds": 5,
    "ruleset_flags": {
      "allow_vehicles": true,
      "enable_killstreaks": false,
      "hardcore_mode": false
    }
  }
}
```

### 2.9 Global ECS Resources Initialization

Initial values for shared ECS resources:

- Gravity
- Game mode state
- Match timer
- Global modifiers

```json
{
  "ecs_resources": {
    "Gravity": { "value": [0, -9.81, 0] },
    "GameModeState": { "phase": "warmup", "round": 1 },
    "MatchTimer": { "remaining_seconds": 600 },
    "GlobalModifiers": {
      "damage_multiplier": 1.0,
      "speed_multiplier": 1.0
    }
  }
}
```

### 2.10 World-Specific Scripting / Logic Binding

Data or scripts that drive world-level behaviour:

- Win/lose conditions
- Round flow
- Match state machines
- References to systems provided by plugins

```json
{
  "world_logic": {
    "win_conditions": [
      { "type": "score_limit", "target": 50 },
      { "type": "time_limit", "seconds": 600 }
    ],
    "lose_conditions": [
      { "type": "team_eliminated" }
    ],
    "round_flow": {
      "warmup_duration": 30,
      "round_duration": 180,
      "intermission_duration": 15
    },
    "state_machine": "scripts/arena_match_flow.state.json"
  }
}
```

---

## 3. layer.package

**Purpose:** "Patch / variant on top of a world"

A layer.package provides modifications, additions, or variants that can be applied on top of a base world. Layers are composable and can be toggled at runtime.

### 3.1 Scene Patches / Additive Scenes

Extra entities to add to the world:

- Props
- Cover objects
- Pickups
- Seasonal decorations

```json
{
  "additive_scenes": [
    {
      "path": "scenes/arena_01_night_props.scene.json",
      "spawn_mode": "immediate"
    },
    {
      "path": "scenes/winter_decorations.scene.json",
      "spawn_mode": "immediate"
    }
  ]
}
```

### 3.2 Spawner Volumes / Encounter Definitions

Additional spawn configurations:

- Enemy spawn zones
- Loot spawners
- Event triggers
- Encounter setups

```json
{
  "spawners": [
    {
      "id": "elite_spawn_zone_a",
      "volume": { "center": [100, 0, 50], "radius": 20 },
      "prefab": "prefabs/enemy_elite.prefab.json",
      "spawn_rate": 0.1,
      "max_active": 2
    }
  ],
  "loot_spawners": [
    {
      "id": "rare_loot_a",
      "position": [50, 1, 75],
      "loot_table": "loot/rare_weapons",
      "respawn_time": 120
    }
  ]
}
```

### 3.3 Lighting Variants

Alternative light setups:

- Night version
- Storm lighting
- Indoor/outdoor transitions
- Dramatic scenes

```json
{
  "lighting": {
    "override_sun": {
      "direction": [-0.5, -0.8, -0.3],
      "color": [0.2, 0.2, 0.4],
      "intensity": 0.3
    },
    "additional_lights": [
      {
        "type": "point",
        "position": [0, 15, 0],
        "color": [1.0, 0.9, 0.7],
        "intensity": 50,
        "radius": 30
      }
    ],
    "ambient_override": {
      "color": [0.05, 0.05, 0.1],
      "intensity": 0.2
    }
  }
}
```

### 3.4 Weather / Environment Overrides

Environmental effect modifications:

- Fog settings
- Rain/snow effects
- Wind zones
- Atmospheric changes

```json
{
  "weather_override": {
    "fog": {
      "enabled": true,
      "color": [0.3, 0.3, 0.4],
      "density": 0.02,
      "height_falloff": 0.5
    },
    "precipitation": {
      "type": "snow",
      "intensity": 0.8,
      "wind_influence": 0.6
    },
    "wind_zones": [
      {
        "volume": { "min": [-100, 0, -100], "max": [100, 50, 100] },
        "direction": [1, 0, 0.3],
        "strength": 5.0
      }
    ]
  }
}
```

### 3.5 Mode-Specific Pickups or Objectives

Game mode specific content:

- Capture points
- Flags
- Bombs
- Payload carts
- Special pickups

```json
{
  "objectives": [
    {
      "type": "capture_point",
      "id": "point_alpha",
      "position": [0, 0, 100],
      "radius": 10,
      "capture_time": 30
    },
    {
      "type": "flag",
      "id": "flag_red",
      "position": [-100, 0, 0],
      "team": "red"
    }
  ],
  "special_pickups": [
    {
      "prefab": "prefabs/powerup_speed.prefab.json",
      "positions": [[25, 1, 25], [-25, 1, -25]],
      "respawn_time": 60
    }
  ]
}
```

### 3.6 Audio Environment Overrides

Sound configuration changes:

- Reverb zones
- Ambient sound sets
- Music playlist changes
- Audio mix adjustments

```json
{
  "audio_override": {
    "reverb_zones": [
      {
        "volume": { "min": [-50, -10, -50], "max": [50, 30, 50] },
        "preset": "large_hall",
        "wet_mix": 0.4
      }
    ],
    "ambient": {
      "soundscape": "audio/night_forest_ambience",
      "volume": 0.6
    },
    "music": {
      "playlist": "music/tense_combat",
      "crossfade_time": 3.0
    }
  }
}
```

### 3.7 Collision / Navigation Patches

Navigation and collision modifications:

- Additional nav mesh links
- Blocked areas
- Dynamic blockers
- Temporary barriers

```json
{
  "navigation_patches": {
    "nav_links": [
      {
        "from": [10, 0, 20],
        "to": [10, 5, 25],
        "type": "jump",
        "bidirectional": false
      }
    ],
    "blocked_areas": [
      {
        "volume": { "min": [50, 0, 50], "max": [60, 10, 60] },
        "reason": "construction_zone"
      }
    ],
    "dynamic_blockers": [
      {
        "id": "gate_north",
        "position": [0, 0, 100],
        "initially_blocked": true
      }
    ]
  }
}
```

### 3.8 Event / Seasonal Content

Temporary themed content:

- Holiday skins for buildings
- Banners
- Event-specific props
- Limited-time entities

```json
{
  "seasonal_content": {
    "theme": "winter_holiday",
    "entity_skins": [
      {
        "target_prefab": "prefabs/building_shop.prefab.json",
        "skin": "skins/building_shop_holiday.skin.json"
      }
    ],
    "decorations": [
      {
        "prefab": "prefabs/holiday_banner.prefab.json",
        "positions": [[0, 10, 0], [50, 10, 0], [-50, 10, 0]]
      }
    ]
  }
}
```

### 3.9 Difficulty or Rule Modifiers

Gameplay balance adjustments:

- HP multipliers
- Spawn rates
- Loot table adjustments
- Damage modifiers

```json
{
  "modifiers": {
    "health_multiplier": 1.5,
    "damage_multiplier": 0.8,
    "spawn_rate_multiplier": 2.0,
    "loot_table_override": "loot/hardcore_tables",
    "xp_multiplier": 2.0
  }
}
```

### 3.10 Debug / Test Instrumentation

Development and testing tools:

- Temporary entities
- Markers for testing
- Visualization aids
- Test scenarios

```json
{
  "debug_instrumentation": {
    "enabled_in_builds": ["debug", "development"],
    "markers": [
      {
        "id": "test_spawn_marker",
        "position": [0, 0, 0],
        "color": [1, 0, 0, 1]
      }
    ],
    "test_entities": [
      {
        "prefab": "prefabs/debug_target_dummy.prefab.json",
        "position": [10, 0, 10]
      }
    ]
  }
}
```

---

## 4. plugin.package

**Purpose:** "Systems, components, logic"

A plugin.package contains behaviour and code. This is where ECS systems, components, and game logic are defined.

### 4.1 New ECS Components and Tags

Component definitions for the ECS:

- Health
- Inventory
- Weapon
- Projectile
- AIState
- AuraState

```json
{
  "components": [
    {
      "name": "Health",
      "fields": {
        "current": { "type": "f32", "default": 100 },
        "max": { "type": "f32", "default": 100 },
        "regeneration_rate": { "type": "f32", "default": 0 }
      }
    },
    {
      "name": "Inventory",
      "fields": {
        "slots": { "type": "array<ItemSlot>", "capacity": 20 },
        "gold": { "type": "u32", "default": 0 }
      }
    },
    {
      "name": "Projectile",
      "fields": {
        "damage": { "type": "f32" },
        "speed": { "type": "f32" },
        "owner": { "type": "Entity" },
        "lifetime": { "type": "f32" }
      }
    }
  ],
  "tags": [
    "Player",
    "Enemy",
    "Interactable",
    "Damageable",
    "Pickupable"
  ]
}
```

### 4.2 ECS Systems

System definitions for game logic:

- Movement
- Combat resolution
- Damage application
- AI brain ticks
- Status effects

```json
{
  "systems": [
    {
      "name": "MovementSystem",
      "stage": "update",
      "query": ["Transform", "Velocity", "?CharacterController"],
      "priority": 100,
      "library": "plugins/movement.dll"
    },
    {
      "name": "CombatResolutionSystem",
      "stage": "update",
      "query": ["Health", "DamageQueue"],
      "priority": 200,
      "library": "plugins/combat.dll"
    },
    {
      "name": "AIBrainSystem",
      "stage": "update",
      "query": ["AIState", "Transform", "?Target"],
      "priority": 150,
      "run_condition": "has_enemies",
      "library": "plugins/ai.dll"
    }
  ]
}
```

### 4.3 Event Handlers and Subscriptions

Event-driven logic:

- PickupEvent
- DamageEvent
- OnDeath
- OnMatchStart
- OnRoundEnd

```json
{
  "event_handlers": [
    {
      "event": "DamageEvent",
      "handler": "on_damage_received",
      "library": "plugins/combat.dll"
    },
    {
      "event": "OnDeath",
      "handler": "on_entity_death",
      "library": "plugins/combat.dll"
    },
    {
      "event": "PickupEvent",
      "handler": "on_item_pickup",
      "library": "plugins/inventory.dll"
    },
    {
      "event": "OnMatchStart",
      "handler": "initialize_match_state",
      "library": "plugins/gamemode.dll"
    }
  ]
}
```

### 4.4 Feature Definitions / Registries

Data registries for game content:

- Weapons
- Abilities
- Buffs
- Quests
- Auras

```json
{
  "registries": {
    "weapons": {
      "type": "WeaponDefinition",
      "entries_path": "data/weapons/",
      "lookup_by": "id"
    },
    "abilities": {
      "type": "AbilityDefinition",
      "entries_path": "data/abilities/",
      "lookup_by": "id"
    },
    "buffs": {
      "type": "BuffDefinition",
      "entries_path": "data/buffs/",
      "lookup_by": "id"
    }
  }
}
```

### 4.5 Integration with External Systems

External system hooks:

- Networking replication
- Persistence
- Analytics
- Matchmaking

```json
{
  "integrations": {
    "networking": {
      "replicated_components": ["Transform", "Health", "Inventory"],
      "rpc_handlers": "plugins/networking.dll"
    },
    "persistence": {
      "saveable_components": ["Inventory", "Progress", "Stats"],
      "save_handler": "plugins/persistence.dll"
    },
    "analytics": {
      "tracked_events": ["PlayerKill", "ItemPickup", "MatchEnd"],
      "analytics_handler": "plugins/analytics.dll"
    }
  }
}
```

### 4.6 Game Mode Logic

Match and round management:

- Round start/stop logic
- Scoring rules
- Victory conditions
- Respawn policies

```json
{
  "game_mode": {
    "name": "TeamDeathmatch",
    "scoring": {
      "kill_points": 1,
      "assist_points": 0.5,
      "objective_points": 5
    },
    "victory_conditions": [
      { "type": "score_limit", "value": 75 },
      { "type": "time_limit", "seconds": 600 }
    ],
    "respawn": {
      "policy": "wave",
      "wave_interval": 10,
      "spawn_protection_duration": 3
    },
    "library": "plugins/gamemode_tdm.dll"
  }
}
```

### 4.7 Ability / Skill Systems

Player ability configuration:

- Cooldowns
- Casting
- Targeting
- Effect application
- Synergy rules

```json
{
  "ability_system": {
    "cooldown_system": {
      "reduction_stat": "cooldown_reduction",
      "min_cooldown_percent": 0.4
    },
    "casting_system": {
      "interrupt_on_damage": true,
      "interrupt_on_movement": false
    },
    "targeting_modes": ["self", "single_enemy", "single_ally", "aoe_ground", "aoe_self", "skillshot"],
    "synergy_rules_path": "data/ability_synergies.json",
    "library": "plugins/abilities.dll"
  }
}
```

### 4.8 AI Utilities and Behaviours

AI system configuration:

- Pathfinding hooks
- Decision trees
- Behaviour trees
- Perception systems

```json
{
  "ai_system": {
    "pathfinding": {
      "algorithm": "a_star",
      "nav_mesh_agent_radius": 0.5,
      "max_path_length": 100
    },
    "perception": {
      "vision_cone_angle": 120,
      "vision_range": 50,
      "hearing_range": 30,
      "update_interval": 0.2
    },
    "behaviour_trees_path": "data/ai/behaviours/",
    "library": "plugins/ai.dll"
  }
}
```

### 4.9 Animation / State Machine Drivers

Animation system integration:

- State translation
- Animation parameters
- Blend control
- Event triggers

```json
{
  "animation_drivers": {
    "state_mappings": [
      {
        "component": "MovementState",
        "field": "is_running",
        "anim_param": "Speed",
        "mapping": "bool_to_float"
      },
      {
        "component": "CombatState",
        "field": "is_attacking",
        "anim_trigger": "Attack"
      }
    ],
    "library": "plugins/animation_driver.dll"
  }
}
```

### 4.10 Content Creators' Custom Behaviours

Mod and creator extensibility:

- Custom systems
- New pickup logic
- Special effects
- Unique mechanics

```json
{
  "custom_behaviours": {
    "enabled": true,
    "sandbox_mode": "restricted",
    "allowed_apis": ["ecs_query", "ecs_spawn", "event_emit"],
    "max_execution_time_ms": 5,
    "examples": [
      {
        "name": "PlasmaAuraOnLowHP",
        "trigger": "health_below_percent",
        "threshold": 25,
        "effect": "apply_aura",
        "aura_id": "plasma_shield"
      }
    ]
  }
}
```

---

## 5. widget.package

**Purpose:** "UI / tooling / overlays"

A widget.package contains everything related to UI rendering, debug tools, and editor interfaces.

### 5.1 Debug HUD Overlays

Performance and debug displays:

- FPS counter
- Ping display
- Memory usage
- CPU/GPU timings

```json
{
  "debug_hud": {
    "enabled_in_builds": ["debug", "development", "profile"],
    "panels": [
      {
        "id": "fps_counter",
        "position": "top_right",
        "metrics": ["fps", "frame_time_ms"]
      },
      {
        "id": "memory_stats",
        "position": "top_left",
        "metrics": ["heap_used_mb", "gpu_memory_mb"]
      },
      {
        "id": "network_stats",
        "position": "bottom_right",
        "metrics": ["ping_ms", "packet_loss_percent"]
      }
    ]
  }
}
```

### 5.2 In-Game Console

Command and debug console:

- Command input
- Log output
- Filtering
- Hot-reload controls

```json
{
  "console": {
    "toggle_key": "tilde",
    "history_size": 100,
    "log_levels": ["error", "warning", "info", "debug"],
    "default_filter": "warning",
    "commands": {
      "reload": "trigger hot-reload",
      "spawn": "spawn entity by prefab",
      "teleport": "teleport player to coordinates",
      "god": "toggle god mode",
      "timescale": "set game time scale"
    },
    "autocomplete": true
  }
}
```

### 5.3 Entity Inspector

Development entity viewer:

- Component display
- Value editing
- Real-time updates
- Component add/remove

```json
{
  "entity_inspector": {
    "enabled_in_builds": ["debug", "development"],
    "selection_modes": ["click", "hierarchy", "search"],
    "editable_components": ["Transform", "Health", "AIState"],
    "read_only_components": ["EntityId", "Archetype"],
    "features": {
      "live_update": true,
      "undo_redo": true,
      "copy_paste": true
    }
  }
}
```

### 5.4 Entity Hierarchy / Outliner

World entity tree view:

- Parent/child relationships
- Search and filter
- Selection sync
- Visibility toggles

```json
{
  "hierarchy_panel": {
    "enabled_in_builds": ["debug", "development"],
    "features": {
      "search": true,
      "filter_by_component": true,
      "filter_by_tag": true,
      "drag_drop_reparent": true,
      "multi_select": true
    },
    "display_options": {
      "show_entity_id": false,
      "show_component_count": true,
      "show_icons": true
    }
  }
}
```

### 5.5 Profiling / Timeline Visualizers

Performance analysis tools:

- Frame time breakdown
- ECS stage timings
- System execution times
- GPU profiling

```json
{
  "profiler": {
    "enabled_in_builds": ["debug", "development", "profile"],
    "capture_key": "F5",
    "panels": [
      {
        "id": "frame_timeline",
        "type": "timeline",
        "tracks": ["cpu_main", "cpu_render", "gpu"]
      },
      {
        "id": "ecs_systems",
        "type": "bar_chart",
        "data_source": "ecs_system_times"
      },
      {
        "id": "memory_timeline",
        "type": "line_graph",
        "data_source": "memory_usage"
      }
    ],
    "export_format": "chrome_trace"
  }
}
```

### 5.6 World / Layer Management Panels

Runtime world control:

- World switching
- Layer toggling
- Package reloading
- Hot-reload triggers

```json
{
  "world_manager": {
    "enabled_in_builds": ["debug", "development"],
    "features": {
      "world_list": true,
      "layer_toggles": true,
      "hot_reload_button": true,
      "package_browser": true,
      "save_world_state": true,
      "load_world_state": true
    }
  }
}
```

### 5.7 Gameplay Debugging Tools

In-game debug visualizations:

- AI state display
- NavMesh overlay
- Vision cones
- Hitbox visualization
- Projectile trails

```json
{
  "gameplay_debug": {
    "enabled_in_builds": ["debug", "development"],
    "visualizers": [
      {
        "id": "ai_state",
        "toggle_key": "F1",
        "display": ["current_state", "target", "path"]
      },
      {
        "id": "navmesh",
        "toggle_key": "F2",
        "display": ["walkable", "obstacles", "links"]
      },
      {
        "id": "hitboxes",
        "toggle_key": "F3",
        "display": ["collision", "hurtbox", "hitbox"]
      },
      {
        "id": "vision_cones",
        "toggle_key": "F4",
        "display": ["fov_cone", "detected_entities"]
      }
    ]
  }
}
```

### 5.8 Networking Debug UI

Network diagnostics:

- Connection status
- Replication stats
- Packet loss
- Bandwidth graphs

```json
{
  "network_debug": {
    "enabled_in_builds": ["debug", "development"],
    "panels": [
      {
        "id": "connection_status",
        "metrics": ["state", "latency", "jitter"]
      },
      {
        "id": "replication_stats",
        "metrics": ["entities_replicated", "bandwidth_in", "bandwidth_out"]
      },
      {
        "id": "packet_graph",
        "type": "line_graph",
        "metrics": ["packets_sent", "packets_received", "packets_lost"]
      }
    ]
  }
}
```

### 5.9 Live Tuning Panels

Runtime parameter adjustment:

- Gameplay sliders
- Rendering settings
- Audio levels
- Physics parameters

```json
{
  "live_tuning": {
    "enabled_in_builds": ["debug", "development"],
    "panels": [
      {
        "id": "gameplay_tuning",
        "parameters": [
          { "path": "player.move_speed", "type": "slider", "range": [1, 20] },
          { "path": "player.jump_height", "type": "slider", "range": [1, 10] },
          { "path": "combat.damage_multiplier", "type": "slider", "range": [0.1, 5] }
        ]
      },
      {
        "id": "render_settings",
        "parameters": [
          { "path": "render.exposure", "type": "slider", "range": [0.1, 5] },
          { "path": "render.bloom_intensity", "type": "slider", "range": [0, 2] }
        ]
      }
    ],
    "features": {
      "save_preset": true,
      "load_preset": true,
      "export_to_config": true
    }
  }
}
```

### 5.10 Creator-Provided UI Widgets

Custom gameplay UI:

- HUDs
- Scoreboards
- Minimaps
- Mod configuration menus

```json
{
  "custom_widgets": {
    "hud": {
      "layout": "ui/layouts/game_hud.layout.json",
      "elements": [
        { "id": "health_bar", "binding": "player.health" },
        { "id": "ammo_counter", "binding": "player.weapon.ammo" },
        { "id": "crosshair", "type": "dynamic_crosshair" }
      ]
    },
    "scoreboard": {
      "layout": "ui/layouts/scoreboard.layout.json",
      "toggle_key": "tab",
      "data_source": "match.player_scores"
    },
    "minimap": {
      "layout": "ui/layouts/minimap.layout.json",
      "position": "top_right",
      "zoom_levels": [1, 2, 4],
      "show_entities": ["player", "teammate", "objective"]
    }
  }
}
```

---

## 6. asset.bundle

**Purpose:** "Pure content: data, no behaviour"

An asset.bundle contains only content data with no executable logic. This is your content-only package type.

### 6.1 Models / Meshes

3D geometry assets:

- Characters
- Weapons
- Environment pieces
- Props
- Vehicles

```json
{
  "meshes": [
    {
      "id": "character_soldier",
      "path": "models/characters/soldier.gltf",
      "lod_levels": ["models/characters/soldier_lod1.gltf", "models/characters/soldier_lod2.gltf"]
    },
    {
      "id": "weapon_rifle",
      "path": "models/weapons/rifle.gltf"
    },
    {
      "id": "prop_barrel",
      "path": "models/props/barrel.gltf",
      "collision": "models/props/barrel_collision.gltf"
    }
  ]
}
```

### 6.2 Textures / Materials

Surface appearance assets:

- Albedo maps
- Normal maps
- Metallic/roughness maps
- Material instances

```json
{
  "textures": [
    {
      "id": "soldier_albedo",
      "path": "textures/characters/soldier_albedo.png",
      "format": "bc7",
      "mipmaps": true
    },
    {
      "id": "soldier_normal",
      "path": "textures/characters/soldier_normal.png",
      "format": "bc5"
    }
  ],
  "materials": [
    {
      "id": "soldier_material",
      "shader": "shaders/pbr_standard",
      "textures": {
        "albedo": "soldier_albedo",
        "normal": "soldier_normal",
        "metallic_roughness": "soldier_orm"
      },
      "parameters": {
        "roughness_scale": 1.0,
        "metallic_scale": 0.0
      }
    }
  ]
}
```

### 6.3 Animations

Motion and skeletal data:

- Skeletal animations
- Blend spaces
- Animation clips
- Additive animations

```json
{
  "animations": [
    {
      "id": "soldier_idle",
      "path": "animations/characters/soldier_idle.gltf",
      "loop": true
    },
    {
      "id": "soldier_run",
      "path": "animations/characters/soldier_run.gltf",
      "loop": true,
      "root_motion": true
    },
    {
      "id": "soldier_attack",
      "path": "animations/characters/soldier_attack.gltf",
      "loop": false,
      "events": [
        { "time": 0.3, "event": "attack_hit" },
        { "time": 0.8, "event": "attack_end" }
      ]
    }
  ],
  "blend_spaces": [
    {
      "id": "soldier_locomotion",
      "type": "2d",
      "axis_x": "velocity_x",
      "axis_y": "velocity_z",
      "samples": [
        { "position": [0, 0], "animation": "soldier_idle" },
        { "position": [0, 5], "animation": "soldier_run" }
      ]
    }
  ]
}
```

### 6.4 Shaders / Shader Graphs

Rendering programs:

- Surface shaders
- Post-process effects
- Aura effects
- Outline shaders

```json
{
  "shaders": [
    {
      "id": "pbr_standard",
      "vertex": "shaders/pbr.vert.spv",
      "fragment": "shaders/pbr.frag.spv",
      "variants": ["NORMAL_MAP", "EMISSIVE", "ALPHA_CUTOUT"]
    },
    {
      "id": "post_bloom",
      "compute": "shaders/bloom.comp.spv"
    },
    {
      "id": "effect_outline",
      "vertex": "shaders/outline.vert.spv",
      "fragment": "shaders/outline.frag.spv"
    }
  ]
}
```

### 6.5 Audio Assets

Sound content:

- Sound effects
- Weapon sounds
- Ambience
- Music tracks
- UI sounds

```json
{
  "audio": [
    {
      "id": "weapon_rifle_fire",
      "path": "audio/weapons/rifle_fire.ogg",
      "type": "sfx",
      "volume": 0.8,
      "variations": [
        "audio/weapons/rifle_fire_01.ogg",
        "audio/weapons/rifle_fire_02.ogg",
        "audio/weapons/rifle_fire_03.ogg"
      ]
    },
    {
      "id": "ambient_forest",
      "path": "audio/ambience/forest_day.ogg",
      "type": "ambient",
      "loop": true
    },
    {
      "id": "music_combat",
      "path": "audio/music/combat_theme.ogg",
      "type": "music",
      "loop": true,
      "bpm": 120
    }
  ]
}
```

### 6.6 VFX Assets

Visual effects content:

- Particle systems
- VFX graphs
- Flipbooks
- Decals

```json
{
  "vfx": [
    {
      "id": "explosion_medium",
      "path": "vfx/explosions/medium.vfx.json",
      "type": "particle_system"
    },
    {
      "id": "muzzle_flash",
      "path": "vfx/weapons/muzzle_flash.vfx.json",
      "type": "particle_system"
    },
    {
      "id": "blood_splatter",
      "path": "vfx/decals/blood_splatter.decal.json",
      "type": "decal",
      "lifetime": 30
    },
    {
      "id": "fire_flipbook",
      "path": "textures/vfx/fire_flipbook.png",
      "type": "flipbook",
      "columns": 8,
      "rows": 8,
      "fps": 30
    }
  ]
}
```

### 6.7 Prefabs / Archetype Definitions

Entity templates:

- Preconfigured entities
- Component bundles
- Spawn configurations

```json
{
  "prefabs": [
    {
      "id": "rifle_pickup",
      "components": {
        "Transform": {},
        "MeshRenderer": { "mesh": "weapon_rifle", "material": "rifle_material" },
        "Collider": { "type": "box", "size": [0.3, 0.2, 1.0] },
        "Pickup": { "item_id": "weapon_rifle", "respawn_time": 30 },
        "Interactable": { "prompt": "Pick up Rifle", "range": 2.0 }
      },
      "tags": ["Pickupable", "Weapon"]
    },
    {
      "id": "explosive_barrel",
      "components": {
        "Transform": {},
        "MeshRenderer": { "mesh": "prop_barrel", "material": "barrel_material" },
        "Health": { "max": 50, "current": 50 },
        "Collider": { "type": "cylinder", "radius": 0.4, "height": 1.2 },
        "Explosive": { "damage": 100, "radius": 5.0, "force": 1000 }
      },
      "tags": ["Damageable", "Explosive"]
    }
  ]
}
```

### 6.8 UI Layouts and Skins

User interface assets:

- HUD layouts
- Icons
- Fonts
- Stylesheets
- Theme definitions

```json
{
  "ui_assets": {
    "layouts": [
      {
        "id": "main_hud",
        "path": "ui/layouts/main_hud.layout.json"
      },
      {
        "id": "inventory_screen",
        "path": "ui/layouts/inventory.layout.json"
      }
    ],
    "icons": [
      {
        "id": "icon_health",
        "path": "ui/icons/health.png"
      },
      {
        "id": "icon_ammo",
        "path": "ui/icons/ammo.png"
      }
    ],
    "fonts": [
      {
        "id": "font_main",
        "path": "ui/fonts/roboto.ttf",
        "sizes": [12, 16, 24, 32]
      }
    ],
    "themes": [
      {
        "id": "theme_default",
        "path": "ui/themes/default.theme.json"
      }
    ]
  }
}
```

### 6.9 Configuration Data / Curves / Tables

Data-driven content:

- Weapon stat tables
- Loot tables
- Tuning curves
- Localization strings

```json
{
  "data_tables": {
    "weapon_stats": {
      "path": "data/weapons/stats.json",
      "schema": "schemas/weapon_stats.schema.json"
    },
    "loot_tables": {
      "path": "data/loot/",
      "schema": "schemas/loot_table.schema.json"
    },
    "damage_curves": {
      "path": "data/curves/damage_falloff.json"
    },
    "localization": {
      "path": "data/localization/",
      "languages": ["en", "es", "fr", "de", "ja"]
    }
  }
}
```

### 6.10 Resource Definitions for Registries

Registry entries:

- WeaponDefinition
- AuraDefinition
- AbilityDefinition
- Referenced by ID from systems in plugins

```json
{
  "definitions": {
    "weapons": [
      {
        "id": "weapon_rifle",
        "display_name": "Assault Rifle",
        "category": "primary",
        "damage": 25,
        "fire_rate": 10,
        "magazine_size": 30,
        "reload_time": 2.5,
        "range": 100,
        "spread": 2.0,
        "mesh": "weapon_rifle",
        "fire_sound": "weapon_rifle_fire",
        "reload_sound": "weapon_rifle_reload"
      }
    ],
    "auras": [
      {
        "id": "aura_regeneration",
        "display_name": "Regeneration",
        "effect": "heal_over_time",
        "value": 5,
        "tick_rate": 1.0,
        "duration": 10,
        "vfx": "vfx_heal_aura",
        "icon": "icon_regen"
      }
    ],
    "abilities": [
      {
        "id": "ability_dash",
        "display_name": "Dash",
        "cooldown": 5,
        "targeting": "self",
        "effect": "apply_velocity",
        "velocity_multiplier": 3.0,
        "duration": 0.3,
        "animation": "soldier_dash",
        "vfx": "vfx_dash_trail"
      }
    ]
  }
}
```

---

## 7. Package Dependencies

Packages can depend on other packages. **The dependency graph must be acyclic; the resolver rejects cycles.**

### 7.1 Dependency Rules

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                             DEPENDENCY RULES                                  │
├──────────────────────────────────────────────────────────────────────────────┤
│ world.package    → may depend on: layer, plugin, widget, asset               │
│ layer.package    → may depend on: plugin, widget, asset                      │
│ plugin.package   → may depend on: plugin (lower layer only), asset           │
│ widget.package   → may depend on: plugin, asset                              │
│ asset.bundle     → may depend on: asset (no cycles, prefer leaf-like)        │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Key points:**

- **Layers can depend on plugins/widgets** — A `competitive_mode.layer` can require a specific gamemode plugin to function.
- **Widgets can depend on plugins** — A HUD widget that displays `Health`/`Inventory` needs the plugin that defines those components.
- **Plugins can depend on other plugins** — A creator weapon plugin can build on top of `gameplay.weapons_core`.
- **Asset bundles are preferably leaf nodes** — Composition happens in world/layer/plugin; deep asset chains are discouraged.

### 7.2 Plugin Layering

Plugin-to-plugin dependencies must follow a strict downward direction to prevent cycles. The resolver enforces this via naming conventions:

```
LAYER HIERARCHY (dependencies flow downward only):

    core.*          ← Foundation plugins (ECS primitives, math, physics)
       ↓
    engine.*        ← Engine-level plugins (rendering, audio, input)
       ↓
    gameplay.*      ← Gameplay plugins (combat, inventory, AI)
       ↓
    feature.*       ← Feature plugins (specific game mechanics)
       ↓
    mod.*           ← Mod/creator plugins (custom content)
```

**Rules:**
- `mod.plasma_weapon` may depend on `gameplay.weapons_core` ✓
- `gameplay.weapons_core` may NOT depend on `mod.plasma_weapon` ✗
- Same-layer dependencies are allowed if acyclic (e.g., `gameplay.combat` → `gameplay.health`)

### 7.3 Asset Bundle Dependencies

Asset bundles may depend on other bundles for content reuse, but with restrictions:

- **Cycles are forbidden** — The resolver rejects circular asset dependencies.
- **Deep chains are discouraged** — Prefer flat composition over deep hierarchies.
- **Leaf-like is preferred** — Most bundles should have zero dependencies; composition happens at the world/layer level.

```json
{
  "package": {
    "name": "weapons_plasma",
    "type": "asset",
    "version": "1.0.0"
  },
  "dependencies": {
    "assets": [
      { "name": "vfx_common", "version": ">=1.0.0", "reason": "shared particle textures" }
    ]
  }
}
```

### 7.4 Example Dependency Declaration

Every package file should begin with a standard metadata block:

```json
{
  "package": {
    "name": "arena_deathmatch",
    "type": "world",
    "version": "1.0.0"
  },
  "dependencies": {
    "plugins": [
      { "name": "core.ecs", "version": ">=1.0.0" },
      { "name": "gameplay.movement", "version": ">=1.0.0" },
      { "name": "gameplay.combat", "version": ">=2.0.0" }
    ],
    "widgets": [
      { "name": "ui.game_hud", "version": ">=1.0.0" }
    ],
    "layers": [
      { "name": "night_variant", "version": "1.0.0", "optional": true }
    ],
    "assets": [
      { "name": "core_characters", "version": ">=1.0.0" },
      { "name": "arena_maps", "version": ">=1.0.0" }
    ]
  }
}
```

---

## 8. Components and Contracts

This section clarifies how component types (declared by plugins) and component instances (in world/layer/asset data) interact.

### 8.1 Component Types vs Instances

| Concept | Declared By | Contains | Example |
|---------|-------------|----------|---------|
| **Component Type** | `plugin.package` | Schema, fields, defaults | `Health { current: f32, max: f32 }` |
| **Component Instance** | `asset.bundle` / `layer.package` / `world.package` | Actual values | `Health { current: 100, max: 100 }` |

**Key principle:** Plugins define the *shape* of data; assets/layers/worlds provide the *values*.

### 8.2 Component Registration Flow

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│  plugin.json    │         │  ECS World      │         │  asset.bundle   │
│                 │         │                 │         │                 │
│  components: [  │ ──────► │  Registers      │ ◄────── │  prefabs: [     │
│    "Health",    │         │  component      │         │    Health: {    │
│    "Inventory"  │         │  types by name  │         │      max: 100   │
│  ]              │         │                 │         │    }            │
└─────────────────┘         └─────────────────┘         └─────────────────┘
```

### 8.3 Asset-to-Behaviour Binding

Asset bundles can only refer to component types **by name/ID** in prefab data. The actual binding happens at runtime:

1. **Plugin declares component type:**
   ```json
   // plugins/gameplay_combat.plugin.json
   {
     "components": [
       {
         "name": "WeaponPickup",
         "fields": {
           "weapon_id": { "type": "string" },
           "ammo_count": { "type": "u32" }
         }
       }
     ]
   }
   ```

2. **Asset bundle references by name:**
   ```json
   // assets/weapons.bundle.json
   {
     "prefabs": [
       {
         "id": "rifle_pickup",
         "components": {
           "WeaponPickup": { "weapon_id": "rifle_01", "ammo_count": 30 }
         }
       }
     ]
   }
   ```

3. **Runtime resolution:**
   - World loads and instantiates prefab
   - ECS looks up "WeaponPickup" in registered component types
   - Creates component instance with provided values

### 8.4 Registries Bridge Definitions and Systems

Plugins provide **registries** that consume **definitions** from asset bundles:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        REGISTRY PATTERN                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  asset.bundle                      plugin.package                        │
│  ─────────────                     ──────────────                        │
│  definitions.weapons: [            registries.weapons: {                 │
│    {                                 type: "WeaponDefinition",           │
│      id: "plasma_rifle",             lookup_by: "id"                     │
│      damage: 45,                   }                                     │
│      fire_rate: 8                                                        │
│    }                               WeaponSystem reads from registry:     │
│  ]                                   let def = weapons.get("plasma_rifle");
│                     ──────────►      apply_damage(def.damage);           │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

**This pattern enables:**
- Creator auras/weapons ship as asset bundles with definitions
- Generic systems in plugins read those definitions from registries
- No code changes needed to add new content

### 8.5 Shared Component Schemas

For interoperability, common component schemas are standardized:

| Schema | Plugin | Purpose |
|--------|--------|---------|
| `Transform` | `core.ecs` | Position, rotation, scale |
| `Health` | `gameplay.combat` | Damageable entities |
| `Inventory` | `gameplay.inventory` | Item containers |
| `AuraState` | `gameplay.effects` | Active buffs/debuffs |
| `WeaponPickup` | `gameplay.weapons` | Weapon pickup interaction |

Any asset bundle or layer can reference these by name, provided the corresponding plugin is loaded.

---

## 9. Creator / Mod Package Patterns

This section provides concrete patterns for creator-authored content that gets uploaded dynamically to the server.

### 9.1 What Constitutes a Creator Feature

A creator-authored feature typically consists of:

| Component | Required? | Purpose |
|-----------|-----------|---------|
| `plugin.package` | Yes (if new behaviour) | Systems, components, event handlers |
| `asset.bundle` | Yes (usually) | Models, VFX, sounds, definitions |
| `widget.package` | Optional | Custom HUD elements |

The feature is referenced either:
- Directly by a `world.package`, or
- Via a separate **modlist** file that the world references

### 9.2 Pattern: Creator Weapon Mod

A new weapon with custom behaviour (damage model, special effects):

```
plasma_rifle_mod/
├── assets/
│   └── weapon_plasma.bundle.json      # Models, VFX, sounds, definitions
├── plugins/
│   └── weapon_plasma.plugin.json      # Custom systems & components
└── widgets/
    └── weapon_plasma_hud.widget.json  # Custom ammo display (optional)
```

**asset bundle (`weapon_plasma.bundle.json`):**
```json
{
  "package": {
    "name": "mod.weapon_plasma",
    "type": "asset",
    "version": "1.0.0"
  },
  "meshes": [
    { "id": "plasma_rifle_mesh", "path": "models/plasma_rifle.gltf" }
  ],
  "vfx": [
    { "id": "plasma_muzzle_flash", "path": "vfx/plasma_flash.vfx.json" },
    { "id": "plasma_projectile_trail", "path": "vfx/plasma_trail.vfx.json" }
  ],
  "audio": [
    { "id": "plasma_fire_sound", "path": "audio/plasma_fire.ogg" }
  ],
  "definitions": {
    "weapons": [
      {
        "id": "weapon_plasma_rifle",
        "display_name": "Plasma Rifle",
        "damage": 45,
        "fire_rate": 8,
        "projectile_type": "plasma_bolt",
        "mesh": "plasma_rifle_mesh",
        "fire_sound": "plasma_fire_sound",
        "muzzle_vfx": "plasma_muzzle_flash"
      }
    ]
  }
}
```

**plugin (`weapon_plasma.plugin.json`):**
```json
{
  "package": {
    "name": "mod.weapon_plasma",
    "type": "plugin",
    "version": "1.0.0"
  },
  "dependencies": {
    "plugins": [
      { "name": "gameplay.weapons_core", "version": ">=1.0.0" },
      { "name": "gameplay.projectiles", "version": ">=1.0.0" }
    ],
    "assets": [
      { "name": "mod.weapon_plasma", "version": ">=1.0.0" }
    ]
  },
  "components": [
    {
      "name": "PlasmaChargeState",
      "fields": {
        "charge_level": { "type": "f32", "default": 0 },
        "is_charging": { "type": "bool", "default": false }
      }
    }
  ],
  "systems": [
    {
      "name": "PlasmaChargeSystem",
      "stage": "update",
      "query": ["PlasmaChargeState", "WeaponState"],
      "library": "mod_weapon_plasma.dll"
    }
  ],
  "event_handlers": [
    {
      "event": "WeaponFireEvent",
      "handler": "on_plasma_fire",
      "filter": { "weapon_id": "weapon_plasma_rifle" },
      "library": "mod_weapon_plasma.dll"
    }
  ]
}
```

### 9.3 Pattern: Creator Visual FX Mod (No Gameplay)

A new aura effect with no new behaviour — just visuals consumed by existing systems:

```
cool_blue_aura_mod/
└── assets/
    └── fx_aura_cool_blue.bundle.json
```

**asset bundle (`fx_aura_cool_blue.bundle.json`):**
```json
{
  "package": {
    "name": "mod.fx_aura_cool_blue",
    "type": "asset",
    "version": "1.0.0"
  },
  "shaders": [
    { "id": "aura_cool_blue_shader", "fragment": "shaders/cool_blue.frag.spv" }
  ],
  "vfx": [
    { "id": "aura_cool_blue_particles", "path": "vfx/cool_blue_swirl.vfx.json" }
  ],
  "ui_assets": {
    "icons": [
      { "id": "icon_cool_blue_aura", "path": "icons/cool_blue.png" }
    ]
  },
  "definitions": {
    "auras": [
      {
        "id": "fx.aura.cool_blue",
        "display_name": "Cool Blue Aura",
        "effect": "visual_only",
        "vfx": "aura_cool_blue_particles",
        "shader_override": "aura_cool_blue_shader",
        "icon": "icon_cool_blue_aura"
      }
    ]
  }
}
```

**Usage:** The existing `gameplay.auras` plugin reads this definition via its `AuraRegistry`. A world or layer assigns the aura to entities via `AuraState` component:

```json
// In a layer or scene file
{
  "entities": [
    {
      "prefab": "player_character",
      "components": {
        "AuraState": { "active_auras": ["fx.aura.cool_blue"] }
      }
    }
  ]
}
```

### 9.4 Pattern: Creator Game Mode

A complete game mode with custom rules, UI, and content:

```
capture_the_flag_mod/
├── assets/
│   └── ctf_content.bundle.json        # Flag models, capture zone VFX
├── plugins/
│   └── ctf_gamemode.plugin.json       # CTF systems, scoring, flag logic
├── widgets/
│   └── ctf_hud.widget.json            # Flag status, team scores
└── layers/
    └── ctf_objectives.layer.json      # Flag spawn points, capture zones
```

This mod would be activated by a world:

```json
// arena_ctf.world.json
{
  "package": { "name": "arena_ctf", "type": "world", "version": "1.0.0" },
  "dependencies": {
    "plugins": [
      { "name": "gameplay.movement", "version": ">=1.0.0" },
      { "name": "mod.ctf_gamemode", "version": ">=1.0.0" }
    ],
    "widgets": [
      { "name": "mod.ctf_hud", "version": ">=1.0.0" }
    ],
    "layers": [
      { "name": "mod.ctf_objectives", "version": ">=1.0.0" }
    ]
  },
  "root_scene": { "path": "scenes/arena_01.scene.json" }
}
```

### 9.5 Modlist Files

For servers that allow multiple mods, a **modlist** file aggregates mod references:

```json
// modlist.json
{
  "name": "community_server_mods",
  "version": "1.0.0",
  "mods": [
    { "name": "mod.weapon_plasma", "version": ">=1.0.0", "enabled": true },
    { "name": "mod.fx_aura_cool_blue", "version": ">=1.0.0", "enabled": true },
    { "name": "mod.ctf_gamemode", "version": ">=1.0.0", "enabled": false }
  ]
}
```

Worlds can reference the modlist instead of individual mods:

```json
{
  "modlist": "modlists/community_server_mods.json"
}
```

---

## 10. Loading Order

Packages are loaded in a specific order to ensure dependencies are satisfied.

### 10.1 Load Order Summary

```
1. asset.bundle      (pure data, no dependencies on behaviour)
2. plugin.package    (registers components, systems, handlers)
3. widget.package    (registers UI, binds to ECS queries)
4. layer.package     (prepared but not applied yet)
5. world.package     (instantiates scene, applies layers, starts systems)
```

**Why this order:**
- Assets must be loaded before plugins can populate registries from definitions
- Plugins must register component types before widgets can bind to ECS queries
- Layers are parsed but deferred until the world applies them
- World is the composition root that brings everything together

### 10.2 Load Sequence Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                         PACKAGE LOAD SEQUENCE                         │
├──────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  1. ASSET BUNDLES                                                     │
│     ├─ Load mesh data into GPU buffers                               │
│     ├─ Load textures, create samplers                                │
│     ├─ Compile shaders, create pipelines                             │
│     ├─ Load audio into memory/streaming                              │
│     └─ Parse prefab definitions into registry                        │
│                                                                       │
│  2. PLUGINS                                                           │
│     ├─ Register ECS components with World                            │
│     ├─ Register ECS systems with Scheduler                           │
│     ├─ Subscribe event handlers                                       │
│     ├─ Initialize registries (weapons, abilities, etc.)              │
│     └─ Connect to external systems (network, persistence)            │
│                                                                       │
│  3. WIDGETS                                                           │
│     ├─ Create UI render targets                                       │
│     ├─ Load UI layouts and themes                                     │
│     ├─ Bind widgets to ECS queries                                   │
│     └─ Register debug commands                                        │
│                                                                       │
│  4. LAYERS (staged)                                                   │
│     ├─ Parse layer definitions                                        │
│     ├─ Validate against base world                                   │
│     └─ Store for deferred application                                │
│                                                                       │
│  5. WORLD                                                             │
│     ├─ Instantiate root scene into ECS                               │
│     ├─ Apply active layers (additive spawning, overrides)            │
│     ├─ Initialize ECS resources                                       │
│     ├─ Start systems execution                                        │
│     └─ Emit OnWorldLoaded event                                       │
│                                                                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference

### Package Types

| Package Type | Extension | Contains | Behaviour? | May Depend On |
|--------------|-----------|----------|------------|---------------|
| `world.package` | `.world.json` | Scene + config | No | layer, plugin, widget, asset |
| `layer.package` | `.layer.json` | Patches + overrides | No | plugin, widget, asset |
| `plugin.package` | `.plugin.json` | Systems + components | Yes | plugin (lower layer), asset |
| `widget.package` | `.widget.json` | UI + tools | Yes | plugin, asset |
| `asset.bundle` | `.bundle.json` | Content only | No | asset (prefer none) |

### Package Metadata Block

Every package file should begin with:

```json
{
  "package": {
    "name": "namespace.package_name",
    "type": "world|layer|plugin|widget|asset",
    "version": "1.0.0"
  },
  "dependencies": { ... }
}
```

### Plugin Layer Hierarchy

```
core.*       → Foundation (ECS, math, physics)
engine.*     → Engine-level (rendering, audio, input)
gameplay.*   → Gameplay (combat, inventory, AI)
feature.*    → Features (specific mechanics)
mod.*        → Mods/creator content
```

Dependencies flow downward only. The resolver rejects cycles.

### Types vs Instances

| Declared By | What It Provides |
|-------------|------------------|
| `plugin.package` | Component types, system behaviour, registries |
| `asset.bundle` | Component instances (prefab data), definitions (registry entries) |
| `layer.package` | Entity instances, overrides, patches |
| `world.package` | Composition root, resource initialization |

---

*Document generated for void_engine package system architecture. Version 1.1*
