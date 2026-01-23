/// @file items.cpp
/// @brief Item system implementation for void_inventory module

#include <void_engine/inventory/items.hpp>

#include <algorithm>
#include <chrono>
#include <random>

namespace void_inventory {

// =============================================================================
// ItemRegistry Implementation
// =============================================================================

ItemRegistry::ItemRegistry() = default;
ItemRegistry::~ItemRegistry() = default;

ItemDefId ItemRegistry::register_item(const ItemDef& def) {
    ItemDefId id{m_next_id++};
    ItemDef registered_def = def;
    registered_def.id = id;

    m_definitions[id] = std::move(registered_def);

    if (!def.internal_name.empty()) {
        m_name_lookup[def.internal_name] = id;
    }

    return id;
}

bool ItemRegistry::unregister_item(ItemDefId id) {
    auto it = m_definitions.find(id);
    if (it == m_definitions.end()) {
        return false;
    }

    // Remove from name lookup
    if (!it->second.internal_name.empty()) {
        m_name_lookup.erase(it->second.internal_name);
    }

    m_definitions.erase(it);
    return true;
}

const ItemDef* ItemRegistry::get_definition(ItemDefId id) const {
    auto it = m_definitions.find(id);
    return it != m_definitions.end() ? &it->second : nullptr;
}

ItemDefId ItemRegistry::find_by_name(std::string_view name) const {
    auto it = m_name_lookup.find(std::string(name));
    return it != m_name_lookup.end() ? it->second : ItemDefId{};
}

std::vector<ItemDefId> ItemRegistry::find_by_category(ItemCategory category) const {
    std::vector<ItemDefId> result;
    for (const auto& [id, def] : m_definitions) {
        if (def.category == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<ItemDefId> ItemRegistry::find_by_tag(std::string_view tag) const {
    std::vector<ItemDefId> result;
    for (const auto& [id, def] : m_definitions) {
        if (def.has_tag(tag)) {
            result.push_back(id);
        }
    }
    return result;
}

std::size_t ItemRegistry::item_count() const {
    return m_definitions.size();
}

std::vector<ItemDefId> ItemRegistry::all_items() const {
    std::vector<ItemDefId> result;
    result.reserve(m_definitions.size());
    for (const auto& [id, def] : m_definitions) {
        result.push_back(id);
    }
    return result;
}

void ItemRegistry::clear() {
    m_definitions.clear();
    m_name_lookup.clear();
}

ItemDef ItemRegistry::preset_health_potion() {
    ItemDef def;
    def.internal_name = "health_potion";
    def.display_name = "Health Potion";
    def.description = "Restores 50 health points when consumed.";
    def.icon_path = "items/potions/health.png";
    def.category = ItemCategory::Consumable;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Stackable | ItemFlags::Consumable | ItemFlags::Tradeable |
                ItemFlags::Sellable | ItemFlags::Droppable;
    def.max_stack = 99;
    def.base_value = 50;
    def.weight = 0.5f;
    def.cooldown = 10.0f;
    def.cooldown_group = "potion";
    def.tags = {"potion", "healing", "consumable"};
    return def;
}

ItemDef ItemRegistry::preset_mana_potion() {
    ItemDef def;
    def.internal_name = "mana_potion";
    def.display_name = "Mana Potion";
    def.description = "Restores 50 mana points when consumed.";
    def.icon_path = "items/potions/mana.png";
    def.category = ItemCategory::Consumable;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Stackable | ItemFlags::Consumable | ItemFlags::Tradeable |
                ItemFlags::Sellable | ItemFlags::Droppable;
    def.max_stack = 99;
    def.base_value = 50;
    def.weight = 0.5f;
    def.cooldown = 10.0f;
    def.cooldown_group = "potion";
    def.tags = {"potion", "mana", "consumable"};
    return def;
}

ItemDef ItemRegistry::preset_gold_coin() {
    ItemDef def;
    def.internal_name = "gold_coin";
    def.display_name = "Gold Coin";
    def.description = "Standard currency used throughout the realm.";
    def.icon_path = "items/currency/gold.png";
    def.category = ItemCategory::Currency;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Stackable | ItemFlags::Tradeable;
    def.max_stack = 9999999;
    def.base_value = 1;
    def.weight = 0.01f;
    def.tags = {"currency", "gold"};
    return def;
}

ItemDef ItemRegistry::preset_iron_sword() {
    ItemDef def;
    def.internal_name = "iron_sword";
    def.display_name = "Iron Sword";
    def.description = "A sturdy sword forged from iron.";
    def.icon_path = "items/weapons/iron_sword.png";
    def.mesh_path = "items/weapons/iron_sword.fbx";
    def.category = ItemCategory::Weapon;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Equippable | ItemFlags::Tradeable | ItemFlags::Sellable |
                ItemFlags::Droppable | ItemFlags::Upgradeable | ItemFlags::Enchantable;
    def.max_stack = 1;
    def.base_value = 100;
    def.weight = 5.0f;
    def.grid_size = {2, 1};
    def.equip_slot = EquipmentSlotType::MainHand;
    def.alternate_slots = {EquipmentSlotType::OffHand};
    def.base_stats = {
        {StatType::Attack, ModifierType::Flat, 15.0f, "Iron Sword"},
        {StatType::AttackSpeed, ModifierType::Flat, 1.0f, "Iron Sword"}
    };
    def.requirements = {
        {StatType::Strength, 10.0f, ""}
    };
    def.tags = {"weapon", "sword", "melee", "iron"};
    def.equipment_set = "Iron Set";
    return def;
}

ItemDef ItemRegistry::preset_leather_armor() {
    ItemDef def;
    def.internal_name = "leather_armor";
    def.display_name = "Leather Armor";
    def.description = "Light armor made from tanned leather.";
    def.icon_path = "items/armor/leather_chest.png";
    def.mesh_path = "items/armor/leather_chest.fbx";
    def.category = ItemCategory::Armor;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Equippable | ItemFlags::Tradeable | ItemFlags::Sellable |
                ItemFlags::Droppable | ItemFlags::Upgradeable | ItemFlags::Enchantable;
    def.max_stack = 1;
    def.base_value = 80;
    def.weight = 8.0f;
    def.grid_size = {2, 2};
    def.equip_slot = EquipmentSlotType::Chest;
    def.base_stats = {
        {StatType::Defense, ModifierType::Flat, 10.0f, "Leather Armor"},
        {StatType::MoveSpeed, ModifierType::Percent, -0.05f, "Leather Armor"}
    };
    def.tags = {"armor", "chest", "leather", "light"};
    def.equipment_set = "Leather Set";
    return def;
}

ItemDef ItemRegistry::preset_iron_ore() {
    ItemDef def;
    def.internal_name = "iron_ore";
    def.display_name = "Iron Ore";
    def.description = "Raw iron ore that can be smelted into ingots.";
    def.icon_path = "items/materials/iron_ore.png";
    def.category = ItemCategory::Material;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Stackable | ItemFlags::Tradeable | ItemFlags::Sellable |
                ItemFlags::Droppable;
    def.max_stack = 99;
    def.base_value = 5;
    def.weight = 2.0f;
    def.tags = {"material", "ore", "iron", "metal"};
    return def;
}

ItemDef ItemRegistry::preset_wood_plank() {
    ItemDef def;
    def.internal_name = "wood_plank";
    def.display_name = "Wood Plank";
    def.description = "A plank of processed wood.";
    def.icon_path = "items/materials/wood_plank.png";
    def.category = ItemCategory::Material;
    def.base_rarity = ItemRarity::Common;
    def.flags = ItemFlags::Stackable | ItemFlags::Tradeable | ItemFlags::Sellable |
                ItemFlags::Droppable;
    def.max_stack = 99;
    def.base_value = 2;
    def.weight = 1.0f;
    def.tags = {"material", "wood", "crafting"};
    return def;
}

// =============================================================================
// ItemFactory Implementation
// =============================================================================

const std::vector<ItemModifier> ItemFactory::s_empty_modifiers;

ItemFactory::ItemFactory() = default;

ItemFactory::ItemFactory(ItemRegistry* registry)
    : m_registry(registry) {
}

ItemFactory::~ItemFactory() = default;

ItemInstanceId ItemFactory::generate_id() {
    return ItemInstanceId{m_next_id++};
}

ItemInstance ItemFactory::create(ItemDefId def_id, std::uint32_t quantity) {
    ItemInstance instance;
    instance.id = generate_id();
    instance.def_id = def_id;
    instance.quantity = quantity;

    if (m_registry) {
        instance.def = m_registry->get_definition(def_id);
        if (instance.def) {
            instance.rarity = instance.def->base_rarity;
        }
    }

    auto now = std::chrono::steady_clock::now();
    instance.created_time = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) / 1000.0;

    return instance;
}

ItemInstance ItemFactory::create(std::string_view name, std::uint32_t quantity) {
    if (!m_registry) {
        return ItemInstance{};
    }

    ItemDefId def_id = m_registry->find_by_name(name);
    if (!def_id) {
        return ItemInstance{};
    }

    return create(def_id, quantity);
}

ItemInstance ItemFactory::create_with_quality(ItemDefId def_id, float quality, std::uint32_t quantity) {
    ItemInstance instance = create(def_id, quantity);
    instance.quality = quality;
    return instance;
}

ItemInstance ItemFactory::create_with_modifiers(ItemDefId def_id, std::uint32_t modifier_count) {
    ItemInstance instance = create(def_id, 1);
    apply_random_modifiers(instance, modifier_count);
    return instance;
}

ItemInstance ItemFactory::clone(const ItemInstance& source) {
    ItemInstance instance = source;
    instance.id = generate_id();

    auto now = std::chrono::steady_clock::now();
    instance.created_time = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) / 1000.0;

    return instance;
}

std::pair<ItemInstance, ItemInstance> ItemFactory::split(ItemInstance& source, std::uint32_t amount) {
    if (amount >= source.quantity) {
        ItemInstance empty;
        return {source, empty};
    }

    ItemInstance split_off = clone(source);
    split_off.quantity = amount;
    source.quantity -= amount;

    return {source, split_off};
}

std::uint32_t ItemFactory::merge(ItemInstance& dest, ItemInstance& source) {
    if (dest.def_id != source.def_id) {
        return source.quantity;
    }

    if (!dest.def || dest.def->max_stack <= 1) {
        return source.quantity;
    }

    std::uint32_t space = dest.def->max_stack - dest.quantity;
    std::uint32_t to_merge = std::min(space, source.quantity);

    dest.quantity += to_merge;
    source.quantity -= to_merge;

    return source.quantity;
}

void ItemFactory::add_modifier_pool(ItemRarity rarity, const std::vector<ItemModifier>& modifiers) {
    m_modifier_pools[rarity] = modifiers;
}

const std::vector<ItemModifier>& ItemFactory::get_modifier_pool(ItemRarity rarity) const {
    auto it = m_modifier_pools.find(rarity);
    return it != m_modifier_pools.end() ? it->second : s_empty_modifiers;
}

void ItemFactory::apply_random_modifiers(ItemInstance& item, std::uint32_t count) {
    const auto& pool = get_modifier_pool(item.rarity);
    if (pool.empty() || count == 0) {
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dist(0, pool.size() - 1);

    for (std::uint32_t i = 0; i < count && i < pool.size(); ++i) {
        std::size_t idx = dist(gen);
        item.modifiers.push_back(pool[idx]);
    }
}

// =============================================================================
// ItemDatabase Implementation
// =============================================================================

ItemDatabase::ItemDatabase() = default;

ItemDatabase::ItemDatabase(ItemRegistry* registry, ItemFactory* factory)
    : m_registry(registry)
    , m_factory(factory) {
}

ItemDatabase::~ItemDatabase() = default;

void ItemDatabase::store(const ItemInstance& item) {
    m_items[item.id] = item;
}

std::optional<ItemInstance> ItemDatabase::retrieve(ItemInstanceId id) const {
    auto it = m_items.find(id);
    if (it != m_items.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ItemDatabase::exists(ItemInstanceId id) const {
    return m_items.find(id) != m_items.end();
}

bool ItemDatabase::remove(ItemInstanceId id) {
    return m_items.erase(id) > 0;
}

std::vector<ItemInstanceId> ItemDatabase::all_items() const {
    std::vector<ItemInstanceId> result;
    result.reserve(m_items.size());
    for (const auto& [id, item] : m_items) {
        result.push_back(id);
    }
    return result;
}

std::vector<ItemInstanceId> ItemDatabase::find_by_owner(EntityId owner) const {
    std::vector<ItemInstanceId> result;
    for (const auto& [id, item] : m_items) {
        if (item.owner == owner) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<ItemInstanceId> ItemDatabase::find_by_definition(ItemDefId def) const {
    std::vector<ItemInstanceId> result;
    for (const auto& [id, item] : m_items) {
        if (item.def_id == def) {
            result.push_back(id);
        }
    }
    return result;
}

void ItemDatabase::clear() {
    m_items.clear();
}

std::vector<ItemDatabase::SerializedItem> ItemDatabase::serialize() const {
    std::vector<SerializedItem> result;
    result.reserve(m_items.size());

    for (const auto& [id, item] : m_items) {
        SerializedItem s;
        s.instance_id = id.value;
        s.def_id = item.def_id.value;
        s.quantity = item.quantity;
        s.durability = item.durability;
        s.quality = item.quality;
        s.rarity = static_cast<std::uint8_t>(item.rarity);
        s.owner = item.owner.value;
        s.bound_to = item.bound_to.value;
        s.soulbound = item.soulbound;
        s.created_time = item.created_time;
        result.push_back(std::move(s));
    }

    return result;
}

void ItemDatabase::deserialize(const std::vector<SerializedItem>& data) {
    m_items.clear();

    for (const auto& s : data) {
        ItemInstance item;
        item.id = ItemInstanceId{s.instance_id};
        item.def_id = ItemDefId{s.def_id};
        item.quantity = s.quantity;
        item.durability = s.durability;
        item.quality = s.quality;
        item.rarity = static_cast<ItemRarity>(s.rarity);
        item.owner = EntityId{s.owner};
        item.bound_to = EntityId{s.bound_to};
        item.soulbound = s.soulbound;
        item.created_time = s.created_time;

        if (m_registry) {
            item.def = m_registry->get_definition(item.def_id);
        }

        m_items[item.id] = std::move(item);
    }
}

} // namespace void_inventory
