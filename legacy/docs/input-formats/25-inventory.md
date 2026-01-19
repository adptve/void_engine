# Inventory & Pickup System

The inventory system manages collectible items, equipment, and world pickups.

## Item Definitions

Items are defined in separate files and referenced by ID:

```toml
# items/health_potion.toml
[item]
id = "health_potion"
name = "Health Potion"
description = "Restores 50 health points"
icon = "ui/icons/health_potion.png"
category = "consumable"
rarity = "common"
stackable = true
max_stack = 99
value = 25                       # Gold/currency value

[item.use_effect]
type = "heal"
amount = 50
sound = "sounds/drink_potion.wav"
effect = "effects/heal_sparkle.toml"
```

## Item Categories

| Category | Description | Stackable |
|----------|-------------|-----------|
| `consumable` | Single-use items | Yes |
| `weapon` | Equippable weapons | No |
| `armor` | Equippable protection | No |
| `accessory` | Rings, amulets, etc. | No |
| `key` | Quest/unlock items | Sometimes |
| `material` | Crafting resources | Yes |
| `ammo` | Weapon ammunition | Yes |
| `quest` | Quest-related items | No |
| `misc` | Miscellaneous | Yes |

## Item Rarity

| Rarity | Typical Color | Drop Rate |
|--------|---------------|-----------|
| `common` | White | High |
| `uncommon` | Green | Medium |
| `rare` | Blue | Low |
| `epic` | Purple | Very Low |
| `legendary` | Orange | Extremely Rare |
| `unique` | Gold | One per game |

## Consumable Items

### Health Restoration

```toml
[item]
id = "health_small"
name = "Small Health Pack"
category = "consumable"
stackable = true
max_stack = 20

[item.use_effect]
type = "heal"
amount = 25
```

### Status Effect

```toml
[item]
id = "fire_resistance_potion"
name = "Fire Resistance Potion"
category = "consumable"

[item.use_effect]
type = "buff"
status = "fire_resistance"
duration = 60.0
resistance_value = 0.5           # 50% fire resistance
```

### Multi-Effect

```toml
[item]
id = "super_potion"
name = "Super Potion"
category = "consumable"

[[item.use_effects]]
type = "heal"
amount = 100

[[item.use_effects]]
type = "buff"
status = "damage_boost"
duration = 30.0
multiplier = 1.25

[[item.use_effects]]
type = "buff"
status = "speed_boost"
duration = 30.0
multiplier = 1.2
```

## Weapon Items

```toml
[item]
id = "plasma_rifle"
name = "Plasma Rifle"
category = "weapon"
stackable = false
rarity = "rare"
equip_slot = "primary"
weapon_ref = "weapons/plasma_rifle"  # Links to weapon definition
icon = "ui/icons/plasma_rifle.png"
mesh_preview = "weapons/plasma_rifle.glb"

[item.requirements]
level = 10
strength = 15

[item.stats]
damage_bonus = 15
fire_rate_bonus = 0.1
```

## Armor Items

```toml
[item]
id = "steel_helmet"
name = "Steel Helmet"
category = "armor"
stackable = false
rarity = "uncommon"
equip_slot = "head"
icon = "ui/icons/steel_helmet.png"
mesh = "armor/steel_helmet.glb"

[item.stats]
defense = 10
fire_resistance = 0.1
weight = 5.0

[item.requirements]
level = 5
```

## Accessory Items

```toml
[item]
id = "ring_of_haste"
name = "Ring of Haste"
category = "accessory"
equip_slot = "ring"
rarity = "epic"

[item.stats]
move_speed_bonus = 0.15
attack_speed_bonus = 0.1

[item.passive_effect]
trigger = "on_dodge"
effect = "speed_burst"
duration = 2.0
cooldown = 30.0
```

## Key Items

```toml
[item]
id = "gold_key"
name = "Golden Key"
category = "key"
stackable = false
quest_item = true                # Can't be dropped/sold

[item]
id = "dungeon_map"
name = "Dungeon Map"
category = "key"

[item.use_effect]
type = "reveal_map"
area = "dungeon_level_1"
```

## Material Items

```toml
[item]
id = "iron_ore"
name = "Iron Ore"
category = "material"
stackable = true
max_stack = 999
value = 5

[item]
id = "dragon_scale"
name = "Dragon Scale"
category = "material"
rarity = "legendary"
stackable = true
max_stack = 10
value = 500
```

## Ammunition

```toml
[item]
id = "bullet_9mm"
name = "9mm Rounds"
category = "ammo"
stackable = true
max_stack = 500
ammo_type = "9mm"
value = 1

[item]
id = "energy_cell"
name = "Energy Cell"
category = "ammo"
stackable = true
max_stack = 100
ammo_type = "energy"
value = 10
```

## Inventory Component

Attach inventory to entities:

```toml
[[entities]]
name = "player"

[entities.inventory]
slots = 30                       # Total inventory capacity
weight_limit = 100.0             # Max carry weight (optional)

# Equipment slots
equipment_slots = [
    "head",
    "chest",
    "legs",
    "feet",
    "hands",
    "primary",
    "secondary",
    "ring_1",
    "ring_2",
    "amulet"
]

# Behavior
drop_on_death = false
destroy_on_death = false
transfer_on_death = "storage_chest"

# Starting items
[[entities.inventory.starting_items]]
id = "health_potion"
quantity = 5

[[entities.inventory.starting_items]]
id = "laser_pistol"
quantity = 1
equip = true                     # Auto-equip on start

[[entities.inventory.starting_items]]
id = "bullet_energy"
quantity = 100
```

## Pickup Entities

World items that can be collected:

```toml
[[entities]]
name = "health_pickup_01"
mesh = "pickups/health_pack.glb"

[entities.transform]
position = [10, 0.5, 5]

[entities.pickup]
item_id = "health_small"
quantity = 1
auto_collect = true              # Collect on touch
collect_radius = 1.5             # For auto-collect
respawn = true
respawn_time = 30.0              # Seconds

[entities.pickup.events]
on_collect = "OnHealthCollected"
on_fail_collect = "OnInventoryFull"

[entities.pickup.feedback]
bob_animation = true
bob_height = 0.2
bob_speed = 2.0
rotate = true
rotate_speed = 90                # Degrees/second
glow_color = [0, 1, 0, 1]
collect_effect = "effects/pickup_sparkle.toml"
collect_sound = "sounds/pickup_health.wav"
```

### Manual Pickup (Interact)

```toml
[[entities]]
name = "weapon_pickup"
mesh = "weapons/shotgun.glb"

[entities.transform]
position = [20, 0.5, 0]

[entities.pickup]
item_id = "shotgun"
quantity = 1
auto_collect = false             # Requires interaction
interact_prompt = "Press E to pick up Shotgun"

[entities.pickup.feedback]
highlight_on_focus = true
highlight_color = [1, 1, 0, 1]
```

### Loot Container

```toml
[[entities]]
name = "treasure_chest"
mesh = "props/chest.glb"

[entities.transform]
position = [30, 0, 10]

[entities.loot_container]
requires_interaction = true
interact_prompt = "Press E to open"
open_animation = "chest_open"
open_sound = "sounds/chest_open.wav"
one_time_only = true

# Fixed loot
[[entities.loot_container.items]]
item_id = "gold_key"
quantity = 1
guaranteed = true

# Random loot from table
[entities.loot_container.loot_table]
table = "loot_tables/dungeon_chest.toml"
rolls = 3
```

## Loot Tables

Define random loot generation:

```toml
# loot_tables/dungeon_chest.toml
[loot_table]
name = "dungeon_chest"

[[entries]]
item_id = "health_potion"
weight = 30                      # Relative probability
quantity_min = 1
quantity_max = 3

[[entries]]
item_id = "mana_potion"
weight = 25
quantity_min = 1
quantity_max = 2

[[entries]]
item_id = "gold_coin"
weight = 40
quantity_min = 10
quantity_max = 50

[[entries]]
item_id = "rare_gem"
weight = 5
quantity_min = 1
quantity_max = 1

# Guaranteed items
[[guaranteed]]
item_id = "dungeon_map"
condition = "first_chest_in_level"
```

## Equipment Slots

| Slot | Description |
|------|-------------|
| `head` | Helmets, hats |
| `chest` | Body armor, robes |
| `legs` | Pants, greaves |
| `feet` | Boots, shoes |
| `hands` | Gloves, gauntlets |
| `primary` | Main weapon |
| `secondary` | Off-hand/backup weapon |
| `ring_1`, `ring_2` | Rings |
| `amulet` | Necklaces |
| `belt` | Belts, pouches |
| `back` | Capes, backpacks |

## Inventory Events

| Event | Parameters | Description |
|-------|------------|-------------|
| `on_item_added` | `item_id: string, quantity: int, slot: int` | Item added |
| `on_item_removed` | `item_id: string, quantity: int` | Item removed |
| `on_item_used` | `item_id: string, target: Entity` | Item consumed |
| `on_item_equipped` | `item_id: string, slot: string` | Item equipped |
| `on_item_unequipped` | `item_id: string, slot: string` | Item unequipped |
| `on_inventory_full` | `attempted_item: string` | Can't add item |

## Inventory Scripting API

### Item Management

```cpp
// Add items
bool Added = Inventory->AddItem("health_potion", 5);
bool AddedAt = Inventory->AddItemToSlot("sword", 0);

// Remove items
bool Removed = Inventory->RemoveItem("health_potion", 1);
Inventory->RemoveItemAt(5);

// Check items
bool Has = Inventory->HasItem("gold_key");
int Count = Inventory->GetItemCount("health_potion");
TArray<FItemStack> AllItems = Inventory->GetAllItems();
```

### Equipment

```cpp
// Equip/unequip
Inventory->EquipItem("steel_helmet", EEquipSlot::Head);
Inventory->UnequipSlot(EEquipSlot::Head);

// Check equipment
FItemData* Helmet = Inventory->GetEquippedItem(EEquipSlot::Head);
bool HasWeapon = Inventory->HasEquipped(EEquipSlot::Primary);
```

### Item Use

```cpp
// Use consumable
Inventory->UseItem("health_potion");
Inventory->UseItemOn("antidote", TargetEntity);

// Drop item
Inventory->DropItem("iron_ore", 10, DropLocation);
```

### Queries

```cpp
// Inventory state
int FreeSlots = Inventory->GetFreeSlots();
float CurrentWeight = Inventory->GetCurrentWeight();
bool IsFull = Inventory->IsFull();

// Find items
TArray<int> HealthPotions = Inventory->FindItemSlots("health_potion");
FItemStack* FirstWeapon = Inventory->FindFirstByCategory(EItemCategory::Weapon);
```

## Complete Example

```toml
# Game with full inventory system

# Item definitions
# items/weapons/plasma_pistol.toml
[item]
id = "plasma_pistol"
name = "Plasma Pistol"
description = "Standard issue energy weapon"
category = "weapon"
rarity = "common"
equip_slot = "primary"
weapon_ref = "weapons/plasma_pistol"
icon = "ui/icons/plasma_pistol.png"
value = 100

[item.stats]
damage_bonus = 0

# items/armor/combat_vest.toml
[item]
id = "combat_vest"
name = "Combat Vest"
description = "Light ballistic protection"
category = "armor"
rarity = "common"
equip_slot = "chest"
icon = "ui/icons/combat_vest.png"
mesh = "armor/combat_vest.glb"
value = 150

[item.stats]
defense = 15
physical_resistance = 0.1
weight = 8.0

# items/consumables/stim_pack.toml
[item]
id = "stim_pack"
name = "Stim Pack"
description = "Emergency medical injection"
category = "consumable"
stackable = true
max_stack = 10
icon = "ui/icons/stim_pack.png"
value = 50

[item.use_effect]
type = "heal"
amount = 40
sound = "sounds/inject.wav"

# Player entity
[[entities]]
name = "player"
mesh = "characters/player.glb"

[entities.inventory]
slots = 24
weight_limit = 80.0
equipment_slots = ["head", "chest", "legs", "feet", "primary", "secondary", "accessory"]
drop_on_death = false

[[entities.inventory.starting_items]]
id = "plasma_pistol"
quantity = 1
equip = true

[[entities.inventory.starting_items]]
id = "stim_pack"
quantity = 3

[[entities.inventory.starting_items]]
id = "energy_cell"
quantity = 50

# World pickups
[[entities]]
name = "stim_pickup_01"
mesh = "pickups/stim_pack.glb"

[entities.transform]
position = [5, 0.3, 10]

[entities.pickup]
item_id = "stim_pack"
quantity = 1
auto_collect = true
collect_radius = 1.0
respawn = true
respawn_time = 60.0

[entities.pickup.feedback]
bob_animation = true
bob_height = 0.15
bob_speed = 1.5
glow_color = [0.2, 0.8, 0.2, 1]
collect_effect = "effects/item_collect.toml"
collect_sound = "sounds/pickup_item.wav"

# Weapon on ground
[[entities]]
name = "shotgun_pickup"
mesh = "weapons/shotgun_world.glb"

[entities.transform]
position = [15, 0.4, 5]
rotation = [0, 45, 0]

[entities.pickup]
item_id = "combat_shotgun"
quantity = 1
auto_collect = false
interact_prompt = "Pick up Combat Shotgun"

[entities.pickup.feedback]
highlight_on_focus = true
highlight_color = [1, 0.8, 0, 0.5]

# Loot crate
[[entities]]
name = "supply_crate"
mesh = "props/supply_crate.glb"

[entities.transform]
position = [25, 0, 0]

[entities.loot_container]
requires_interaction = true
interact_prompt = "Open Supply Crate"
open_animation = "crate_open"
open_sound = "sounds/crate_open.wav"
one_time_only = true

[[entities.loot_container.items]]
item_id = "energy_cell"
quantity = 25
guaranteed = true

[entities.loot_container.loot_table]
table = "loot_tables/supply_crate"
rolls = 2

# Vendor NPC
[[entities]]
name = "merchant"
mesh = "characters/merchant.glb"

[entities.transform]
position = [30, 0, 20]

[entities.vendor]
name = "Arms Dealer"
interact_prompt = "Trade"

[[entities.vendor.inventory]]
item_id = "plasma_pistol"
quantity = 3
price = 150

[[entities.vendor.inventory]]
item_id = "stim_pack"
quantity = 10
price = 75

[[entities.vendor.inventory]]
item_id = "combat_vest"
quantity = 1
price = 200

[entities.vendor.buy_rates]
default = 0.5                    # Buy at 50% value
weapon = 0.6                     # Buy weapons at 60%
junk = 0.2                       # Buy junk at 20%
```
