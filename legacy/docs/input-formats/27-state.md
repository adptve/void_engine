# Game State & Variables

The state system provides persistent game variables, save/load functionality, and entity state management.

## Scene State Variables

### Basic Variables

```toml
[state]
[state.variables]
score = { type = "int", default = 0 }
player_name = { type = "string", default = "Player" }
difficulty = { type = "float", default = 1.0 }
has_key = { type = "bool", default = false }
checkpoint = { type = "vec3", default = [0, 0, 0] }
```

### Variable Configuration

```toml
[state.variables]
# Integer variable
score = {
    type = "int",
    default = 0,
    min = 0,
    max = 999999,
    persistent = true            # Saved to disk
}

# Float variable
health_multiplier = {
    type = "float",
    default = 1.0,
    min = 0.5,
    max = 2.0,
    persistent = true
}

# String variable
current_level = {
    type = "string",
    default = "level_01",
    persistent = true
}

# Boolean variable
tutorial_complete = {
    type = "bool",
    default = false,
    persistent = true
}

# Vector variable
last_checkpoint = {
    type = "vec3",
    default = [0, 0, 0],
    persistent = true
}

# Array variable
collected_items = {
    type = "array",
    element_type = "string",
    default = [],
    persistent = true
}

# Dictionary/map variable
quest_progress = {
    type = "map",
    key_type = "string",
    value_type = "int",
    default = {},
    persistent = true
}
```

## Variable Types

| Type | TOML Syntax | Description |
|------|-------------|-------------|
| `int` | `{ type = "int", default = 0 }` | Integer number |
| `float` | `{ type = "float", default = 0.0 }` | Decimal number |
| `bool` | `{ type = "bool", default = false }` | True/false |
| `string` | `{ type = "string", default = "" }` | Text |
| `vec2` | `{ type = "vec2", default = [0, 0] }` | 2D vector |
| `vec3` | `{ type = "vec3", default = [0, 0, 0] }` | 3D vector |
| `vec4` | `{ type = "vec4", default = [0, 0, 0, 0] }` | 4D vector |
| `color` | `{ type = "color", default = [1, 1, 1, 1] }` | RGBA color |
| `entity` | `{ type = "entity", default = "" }` | Entity reference |
| `array` | `{ type = "array", element_type = "int" }` | List |
| `map` | `{ type = "map", key_type = "string" }` | Dictionary |

## Entity State

### Basic Entity State

```toml
[[entities]]
name = "door"
mesh = "door.glb"

[entities.state]
is_open = false
is_locked = true
times_opened = 0
```

### State with Events

```toml
[[entities]]
name = "treasure_chest"

[entities.state]
is_opened = false
loot_collected = false
required_key = "gold_key"

[entities.state.events]
on_state_change = "OnChestStateChange"
```

### State Variables

```toml
[[entities]]
name = "enemy"

[entities.state]
health = 100
alert_level = 0.0
current_state = "idle"
target_entity = ""
last_known_position = [0, 0, 0]

[entities.state.config]
sync_to_network = true           # For multiplayer
replicate_to_clients = true
```

## Save/Load System

### Auto-Save Configuration

```toml
[state.save]
enabled = true
auto_save = true
auto_save_interval = 300         # 5 minutes
auto_save_slot = 0               # Slot for auto-saves
max_auto_saves = 3               # Rotating auto-saves
save_on_checkpoint = true
save_on_level_change = true
```

### Manual Save Configuration

```toml
[state.save]
slot_count = 10                  # Number of save slots
save_format = "binary"           # binary, json, toml
compress = true
encrypt = false                  # Optional save encryption

# What to save
include_scene_state = true
include_entity_states = true
include_inventory = true
include_quest_progress = true
include_statistics = true
```

### Save Slot Data

```toml
# Define what appears in save slot UI
[state.save.slot_info]
show_timestamp = true
show_play_time = true
show_level_name = true
show_screenshot = true
screenshot_resolution = [320, 180]
custom_fields = ["player_level", "location_name", "completion_percent"]
```

### Persistent Variables

Mark which variables persist across sessions:

```toml
[state.variables]
# These are saved
high_score = { type = "int", default = 0, persistent = true }
unlocked_levels = { type = "array", element_type = "string", default = [], persistent = true }

# These reset each session
current_combo = { type = "int", default = 0, persistent = false }
temp_buff_active = { type = "bool", default = false, persistent = false }
```

## Checkpoints

### Checkpoint Definition

```toml
[[checkpoints]]
id = "checkpoint_01"
position = [50, 0, 0]
rotation = [0, 90, 0]
trigger = "checkpoint_trigger_01"  # Associated trigger volume

[checkpoints.on_activate]
save_variables = ["health", "ammo", "inventory"]
heal_player = true
refill_ammo = false
```

### Checkpoint Trigger

```toml
[[triggers]]
name = "checkpoint_trigger_01"
shape = "box"
size = [5, 4, 5]
position = [50, 2, 0]
layer_mask = ["player"]
one_shot = true
on_enter = "ActivateCheckpoint"

[triggers.data]
checkpoint_id = "checkpoint_01"
```

## Quest/Progress System

### Quest Definition

```toml
[quests]
[[quests.definitions]]
id = "main_quest_01"
name = "The Beginning"
description = "Find the ancient artifact"
type = "main"                    # main, side, daily, event

[[quests.definitions.objectives]]
id = "find_key"
description = "Find the dungeon key"
type = "collect"
target = "dungeon_key"
required = 1
optional = false

[[quests.definitions.objectives]]
id = "defeat_guardian"
description = "Defeat the Guardian"
type = "kill"
target = "boss_guardian"
required = 1

[[quests.definitions.objectives]]
id = "collect_gems"
description = "Collect optional gems"
type = "collect"
target = "gem"
required = 5
optional = true                  # Bonus objective

[quests.definitions.rewards]
experience = 500
gold = 100
items = [{ id = "rare_sword", quantity = 1 }]
```

### Quest Progress State

```toml
[state.variables]
quest_main_01_status = { type = "string", default = "not_started" }
quest_main_01_objectives = {
    type = "map",
    key_type = "string",
    value_type = "int",
    default = {},
    persistent = true
}
```

## Statistics Tracking

```toml
[state.statistics]
# Gameplay stats
play_time_seconds = { type = "int", default = 0, persistent = true }
enemies_killed = { type = "int", default = 0, persistent = true }
deaths = { type = "int", default = 0, persistent = true }
damage_dealt = { type = "float", default = 0.0, persistent = true }
damage_taken = { type = "float", default = 0.0, persistent = true }
items_collected = { type = "int", default = 0, persistent = true }
distance_traveled = { type = "float", default = 0.0, persistent = true }
highest_combo = { type = "int", default = 0, persistent = true }

# Per-level stats (tracked separately)
level_completion_times = {
    type = "map",
    key_type = "string",
    value_type = "float",
    default = {},
    persistent = true
}
```

## Achievements

```toml
[achievements]
[[achievements.definitions]]
id = "first_blood"
name = "First Blood"
description = "Defeat your first enemy"
icon = "ui/achievements/first_blood.png"
hidden = false
condition = "enemies_killed >= 1"

[[achievements.definitions]]
id = "sharpshooter"
name = "Sharpshooter"
description = "Achieve 100 headshots"
icon = "ui/achievements/sharpshooter.png"
hidden = false
condition = "headshots >= 100"
progress_stat = "headshots"
progress_max = 100

[[achievements.definitions]]
id = "secret_area"
name = "Explorer"
description = "Find the hidden area"
hidden = true                    # Hidden until unlocked
condition = "found_secret_area == true"
```

## State Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_state_change` | `key: string, old_value: any, new_value: any` | Variable changed |
| `on_save` | `slot: int` | Game saved |
| `on_load` | `slot: int` | Game loaded |
| `on_checkpoint` | `checkpoint_id: string` | Checkpoint reached |
| `on_quest_update` | `quest_id: string, objective_id: string` | Quest progress |
| `on_achievement` | `achievement_id: string` | Achievement unlocked |

## State Scripting API

### Variable Access

```cpp
// Get values
int Score = State->GetInt("score");
float Difficulty = State->GetFloat("difficulty");
FString Name = State->GetString("player_name");
bool HasKey = State->GetBool("has_key");
FVector Checkpoint = State->GetVec3("checkpoint");

// Set values
State->SetInt("score", Score + 100);
State->SetFloat("difficulty", 1.5f);
State->SetString("player_name", "Hero");
State->SetBool("has_key", true);
State->SetVec3("checkpoint", NewPosition);

// Increment/decrement
State->Increment("score", 50);
State->Decrement("lives", 1);
```

### Entity State

```cpp
// Get entity state
bool IsOpen = Entity->GetState<bool>("is_open");
int TimesUsed = Entity->GetState<int>("times_used");

// Set entity state
Entity->SetState("is_open", true);
Entity->SetState("times_used", TimesUsed + 1);

// Watch for changes
Entity->OnStateChange("is_open", [](bool OldValue, bool NewValue) {
    // React to change
});
```

### Save/Load

```cpp
// Manual save
State->SaveGame(SlotIndex);
State->SaveGameAsync(SlotIndex, [](bool Success) {
    // Callback when done
});

// Load game
State->LoadGame(SlotIndex);

// Check slots
bool HasSave = State->HasSaveInSlot(SlotIndex);
FSaveSlotInfo Info = State->GetSlotInfo(SlotIndex);

// Delete save
State->DeleteSave(SlotIndex);
```

### Checkpoints

```cpp
// Activate checkpoint
State->SetCheckpoint("checkpoint_02");

// Respawn at checkpoint
State->RespawnAtCheckpoint();

// Get current checkpoint
FString CheckpointId = State->GetCurrentCheckpoint();
```

### Quests

```cpp
// Start quest
Quests->StartQuest("main_quest_01");

// Update objective
Quests->UpdateObjective("main_quest_01", "find_key", 1);

// Complete quest
Quests->CompleteQuest("main_quest_01");

// Check status
EQuestStatus Status = Quests->GetQuestStatus("main_quest_01");
int Progress = Quests->GetObjectiveProgress("main_quest_01", "collect_gems");
```

### Statistics

```cpp
// Update stats
Stats->Increment("enemies_killed");
Stats->Add("damage_dealt", DamageAmount);
Stats->SetMax("highest_combo", CurrentCombo);  // Only updates if higher

// Get stats
int Kills = Stats->GetInt("enemies_killed");
float PlayTime = Stats->GetFloat("play_time_seconds");
```

## Complete Example

```toml
# Game state configuration

[state]
# Global game variables
[state.variables]
# Player progress
player_level = { type = "int", default = 1, min = 1, max = 100, persistent = true }
player_experience = { type = "int", default = 0, persistent = true }
gold = { type = "int", default = 0, min = 0, persistent = true }

# Game progress
current_chapter = { type = "int", default = 1, persistent = true }
unlocked_areas = { type = "array", element_type = "string", default = ["starting_village"], persistent = true }
defeated_bosses = { type = "array", element_type = "string", default = [], persistent = true }

# Settings (non-gameplay)
difficulty = { type = "string", default = "normal", persistent = true }
show_damage_numbers = { type = "bool", default = true, persistent = true }

# Temporary state (not saved)
current_checkpoint = { type = "string", default = "", persistent = false }
in_combat = { type = "bool", default = false, persistent = false }
combo_counter = { type = "int", default = 0, persistent = false }

# Save system
[state.save]
enabled = true
slot_count = 5
auto_save = true
auto_save_interval = 300
auto_save_slot = 0
save_on_checkpoint = true
compress = true

[state.save.slot_info]
show_timestamp = true
show_play_time = true
show_level_name = true
show_screenshot = true
custom_fields = ["player_level", "current_chapter", "gold"]

# Statistics
[state.statistics]
play_time = { type = "float", default = 0.0, persistent = true }
enemies_defeated = { type = "int", default = 0, persistent = true }
deaths = { type = "int", default = 0, persistent = true }
damage_dealt = { type = "float", default = 0.0, persistent = true }
distance_walked = { type = "float", default = 0.0, persistent = true }
items_collected = { type = "int", default = 0, persistent = true }
secrets_found = { type = "int", default = 0, persistent = true }
max_combo = { type = "int", default = 0, persistent = true }

# Checkpoints
[[checkpoints]]
id = "village_entrance"
position = [0, 0, 5]
rotation = [0, 0, 0]

[[checkpoints]]
id = "dungeon_start"
position = [100, 0, 0]
rotation = [0, 90, 0]

[[checkpoints]]
id = "boss_arena"
position = [200, 0, 50]
rotation = [0, 180, 0]

# Quests
[[quests.definitions]]
id = "tutorial"
name = "Learning the Ropes"
description = "Complete the tutorial"
type = "main"

[[quests.definitions.objectives]]
id = "move_around"
description = "Move using WASD"
type = "action"
target = "moved"
required = 1

[[quests.definitions.objectives]]
id = "attack_dummy"
description = "Attack the training dummy"
type = "action"
target = "attacked_dummy"
required = 1

[[quests.definitions.objectives]]
id = "open_menu"
description = "Open the inventory"
type = "action"
target = "opened_inventory"
required = 1

[quests.definitions.rewards]
experience = 50

[[quests.definitions]]
id = "first_dungeon"
name = "Into the Darkness"
description = "Clear the abandoned mine"
type = "main"

[[quests.definitions.objectives]]
id = "enter_mine"
description = "Enter the abandoned mine"
type = "reach"
target = "mine_entrance"
required = 1

[[quests.definitions.objectives]]
id = "kill_spiders"
description = "Defeat the giant spiders"
type = "kill"
target = "giant_spider"
required = 5

[[quests.definitions.objectives]]
id = "find_artifact"
description = "Find the ancient artifact"
type = "collect"
target = "ancient_artifact"
required = 1

[[quests.definitions.objectives]]
id = "defeat_boss"
description = "Defeat the Spider Queen"
type = "kill"
target = "spider_queen"
required = 1

[quests.definitions.rewards]
experience = 500
gold = 200
items = [{ id = "spider_silk_armor", quantity = 1 }]

# Achievements
[[achievements.definitions]]
id = "first_steps"
name = "First Steps"
description = "Complete the tutorial"
condition = "quest_tutorial_complete == true"

[[achievements.definitions]]
id = "monster_slayer"
name = "Monster Slayer"
description = "Defeat 100 enemies"
condition = "enemies_defeated >= 100"
progress_stat = "enemies_defeated"
progress_max = 100

[[achievements.definitions]]
id = "treasure_hunter"
name = "Treasure Hunter"
description = "Find all secret areas"
condition = "secrets_found >= 10"
progress_stat = "secrets_found"
progress_max = 10

[[achievements.definitions]]
id = "speed_runner"
name = "Speed Runner"
description = "Complete the game in under 2 hours"
hidden = true
condition = "game_complete == true AND play_time < 7200"

# Entities with state

[[entities]]
name = "locked_door"
mesh = "props/door_ornate.glb"

[entities.transform]
position = [50, 1.5, 0]

[entities.state]
is_locked = true
required_key = "golden_key"
is_open = false

[[entities]]
name = "puzzle_switch_1"
mesh = "props/floor_switch.glb"

[entities.transform]
position = [45, 0, 5]

[entities.state]
is_activated = false
linked_door = "locked_door"

[[entities]]
name = "collectable_gem"
mesh = "items/gem_red.glb"

[entities.transform]
position = [30, 1, 10]

[entities.pickup]
item_id = "red_gem"
quantity = 1
one_shot = true

[entities.state]
collected = false
```
