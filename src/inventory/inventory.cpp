/// @file inventory.cpp
/// @brief Main inventory system implementation for void_inventory module

#include <void_engine/inventory/inventory.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace void_inventory {

// =============================================================================
// InventoryComponent Implementation
// =============================================================================

InventoryComponent::InventoryComponent()
    : m_main_inventory(ContainerId{1}, "Main Inventory", 20)
    , m_hotbar(ContainerId{2}, "Hotbar", 10) {
}

InventoryComponent::InventoryComponent(EntityId owner)
    : m_owner(owner)
    , m_main_inventory(ContainerId{1}, "Main Inventory", 20)
    , m_hotbar(ContainerId{2}, "Hotbar", 10)
    , m_equipment(owner)
    , m_crafting(owner) {
}

InventoryComponent::~InventoryComponent() = default;

TransactionResult InventoryComponent::pickup(ItemInstanceId item, std::uint32_t quantity) {
    // Try hotbar first
    std::uint32_t slot;
    auto result = m_hotbar.add(item, quantity, &slot);

    if (result != TransactionResult::Success) {
        // Try main inventory
        result = m_main_inventory.add(item, quantity, &slot);
    }

    if (result == TransactionResult::Success && m_on_pickup) {
        const ItemInstance* inst = nullptr;
        if (m_item_db) {
            auto opt = m_item_db->retrieve(item);
            if (opt) {
                ItemPickupEvent event;
                event.entity = m_owner;
                event.item = item;
                event.def = opt->def_id;
                event.quantity = quantity;
                m_on_pickup(event);
            }
        }
    }

    return result;
}

TransactionResult InventoryComponent::drop(std::uint32_t slot, std::uint32_t quantity) {
    ItemInstanceId item = m_main_inventory.get_item(slot);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    auto result = m_main_inventory.remove(slot, quantity);

    if (result == TransactionResult::Success && m_on_drop) {
        ItemDropEvent event;
        event.entity = m_owner;
        event.item = item;
        event.quantity = quantity;
        m_on_drop(event);
    }

    return result;
}

bool InventoryComponent::use_item(std::uint32_t slot, EntityId target) {
    ItemInstanceId item_id = m_main_inventory.get_item(slot);
    if (!item_id || !m_item_db) {
        return false;
    }

    auto opt = m_item_db->retrieve(item_id);
    if (!opt || !opt->def) {
        return false;
    }

    ItemInstance& item = *opt;

    // Check cooldown
    auto now = std::chrono::steady_clock::now();
    double current_time = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) / 1000.0;

    if (item.is_on_cooldown(current_time)) {
        return false;
    }

    // Check if consumable
    if (!has_flag(item.def->flags, ItemFlags::Consumable)) {
        return false;
    }

    // Call use callback
    bool consumed = false;
    if (item.def->on_use) {
        consumed = item.def->on_use(m_owner, item);
    }

    // Start cooldown
    if (item.def->cooldown > 0) {
        item.cooldown_end = current_time + item.def->cooldown;
        m_item_db->store(item);
    }

    // Fire event
    if (m_on_use) {
        ItemUseEvent event;
        event.entity = m_owner;
        event.item = item_id;
        event.def = item.def_id;
        event.target = target;
        event.consumed = consumed;
        m_on_use(event);
    }

    // Remove if consumed
    if (consumed) {
        m_main_inventory.remove(slot, 1);
    }

    return true;
}

bool InventoryComponent::use_hotbar(std::uint32_t slot, EntityId target) {
    ItemInstanceId item_id = m_hotbar.get_item(slot);
    if (!item_id) {
        return false;
    }

    // Find in main inventory and use
    auto main_slot = m_main_inventory.find_item(item_id);
    if (main_slot) {
        return use_item(*main_slot, target);
    }

    return false;
}

TransactionResult InventoryComponent::equip_from_slot(std::uint32_t inv_slot, EquipmentSlotId equip_slot) {
    ItemInstanceId item = m_main_inventory.get_item(inv_slot);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    // Unequip current if occupied
    ItemInstanceId current;
    if (m_equipment.is_slot_occupied(equip_slot)) {
        m_equipment.unequip(equip_slot, &current);
        if (current) {
            m_main_inventory.add_to_slot(inv_slot, current, 1);
        }
    }

    // Remove from inventory
    m_main_inventory.remove(inv_slot, 1);

    // Equip
    return m_equipment.equip(item, equip_slot);
}

TransactionResult InventoryComponent::unequip_to_slot(EquipmentSlotId equip_slot, std::uint32_t inv_slot) {
    ItemInstanceId item;
    auto result = m_equipment.unequip(equip_slot, &item);

    if (result == TransactionResult::Success && item) {
        result = m_main_inventory.add_to_slot(inv_slot, item, 1);
        if (result != TransactionResult::Success) {
            // Re-equip if can't store
            m_equipment.equip(item, equip_slot);
        }
    }

    return result;
}

std::uint32_t InventoryComponent::count_items(ItemDefId def) const {
    return m_main_inventory.count_item(def) + m_hotbar.count_item(def);
}

bool InventoryComponent::has_item(ItemDefId def, std::uint32_t quantity) const {
    return count_items(def) >= quantity;
}

float InventoryComponent::total_weight() const {
    float weight = 0;

    // Calculate from main inventory
    for (std::uint32_t i = 0; i < m_main_inventory.capacity(); ++i) {
        const SlotState* slot = m_main_inventory.get_slot(i);
        if (slot && !slot->empty() && m_item_db) {
            auto opt = m_item_db->retrieve(slot->item);
            if (opt && opt->def) {
                weight += opt->def->weight * slot->quantity;
            }
        }
    }

    return weight;
}

std::uint64_t InventoryComponent::total_value() const {
    std::uint64_t value = 0;

    for (std::uint32_t i = 0; i < m_main_inventory.capacity(); ++i) {
        const SlotState* slot = m_main_inventory.get_slot(i);
        if (slot && !slot->empty() && m_item_db) {
            auto opt = m_item_db->retrieve(slot->item);
            if (opt) {
                value += opt->calculate_value() * slot->quantity;
            }
        }
    }

    return value;
}

std::uint64_t InventoryComponent::get_currency(ItemDefId currency_def) const {
    return count_items(currency_def);
}

bool InventoryComponent::add_currency(ItemDefId currency_def, std::uint64_t amount) {
    if (!m_factory) {
        return false;
    }

    ItemInstance currency = m_factory->create(currency_def, static_cast<std::uint32_t>(amount));
    if (m_item_db) {
        m_item_db->store(currency);
    }

    return m_main_inventory.add(currency.id, static_cast<std::uint32_t>(amount), nullptr) == TransactionResult::Success;
}

bool InventoryComponent::remove_currency(ItemDefId currency_def, std::uint64_t amount) {
    if (count_items(currency_def) < amount) {
        return false;
    }

    auto slots = m_main_inventory.find_all(currency_def);
    std::uint64_t remaining = amount;

    for (auto slot : slots) {
        std::uint32_t in_slot = m_main_inventory.get_quantity(slot);
        std::uint32_t to_remove = static_cast<std::uint32_t>(std::min(static_cast<std::uint64_t>(in_slot), remaining));
        m_main_inventory.remove(slot, to_remove);
        remaining -= to_remove;
        if (remaining == 0) break;
    }

    return true;
}

bool InventoryComponent::can_afford(ItemDefId currency_def, std::uint64_t amount) const {
    return get_currency(currency_def) >= amount;
}

void InventoryComponent::set_main_inventory_size(std::size_t size) {
    m_main_inventory.resize(size);
}

void InventoryComponent::set_hotbar_size(std::size_t size) {
    m_hotbar.resize(size);
}

void InventoryComponent::set_weight_limit(float limit) {
    m_weight_limit = limit;
}

void InventoryComponent::set_item_database(ItemDatabase* db) {
    m_item_db = db;
    m_main_inventory.set_item_database(db);
    m_hotbar.set_item_database(db);
    m_equipment.set_item_database(db);
    m_crafting.set_item_database(db);
}

void InventoryComponent::set_recipe_registry(RecipeRegistry* registry) {
    m_crafting.set_recipe_registry(registry);
}

void InventoryComponent::set_set_registry(EquipmentSetRegistry* registry) {
    m_equipment.set_set_registry(registry);
}

InventoryComponent::SerializedInventory InventoryComponent::serialize() const {
    SerializedInventory data;

    // Main inventory
    for (std::uint32_t i = 0; i < m_main_inventory.capacity(); ++i) {
        const SlotState* slot = m_main_inventory.get_slot(i);
        if (slot && !slot->empty()) {
            data.main_slots.emplace_back(i, slot->item.value);
        }
    }

    // Hotbar
    for (std::uint32_t i = 0; i < m_hotbar.capacity(); ++i) {
        const SlotState* slot = m_hotbar.get_slot(i);
        if (slot && !slot->empty()) {
            data.hotbar_slots.emplace_back(i, slot->item.value);
        }
    }

    // Equipment
    for (const auto& equipped : m_equipment.all_equipped()) {
        data.equipped_items.emplace_back(equipped.slot.value, equipped.item.value);
    }

    // Known recipes
    for (const auto& recipe : m_crafting.known_recipes()) {
        data.known_recipes.push_back(recipe.value);
    }

    return data;
}

void InventoryComponent::deserialize(const SerializedInventory& data) {
    m_main_inventory.clear();
    m_hotbar.clear();

    for (const auto& [slot, item_id] : data.main_slots) {
        m_main_inventory.add_to_slot(slot, ItemInstanceId{item_id}, 1);
    }

    for (const auto& [slot, item_id] : data.hotbar_slots) {
        m_hotbar.add_to_slot(slot, ItemInstanceId{item_id}, 1);
    }

    for (const auto& [slot_id, item_id] : data.equipped_items) {
        m_equipment.equip(ItemInstanceId{item_id}, EquipmentSlotId{slot_id});
    }

    for (auto recipe_id : data.known_recipes) {
        m_crafting.learn_recipe(RecipeId{recipe_id});
    }
}

// =============================================================================
// LootTable Implementation
// =============================================================================

void LootTable::add_entry(const LootEntry& entry) {
    entries.push_back(entry);
}

float LootTable::total_weight() const {
    float total = 0;
    for (const auto& entry : entries) {
        total += entry.weight;
    }
    return total;
}

// =============================================================================
// LootGenerator Implementation
// =============================================================================

LootGenerator::LootGenerator() = default;
LootGenerator::~LootGenerator() = default;

void LootGenerator::register_table(const std::string& name, const LootTable& table) {
    m_tables[name] = table;
}

const LootTable* LootGenerator::get_table(const std::string& name) const {
    auto it = m_tables.find(name);
    return it != m_tables.end() ? &it->second : nullptr;
}

std::vector<ItemInstance> LootGenerator::generate(const std::string& table_name) {
    return generate(table_name, 0);
}

std::vector<ItemInstance> LootGenerator::generate(const std::string& table_name, float luck) {
    std::vector<ItemInstance> result;

    const LootTable* table = get_table(table_name);
    if (!table || table->entries.empty()) {
        return result;
    }

    // Determine number of drops
    std::uint32_t num_drops = random_uint(table->min_drops, table->max_drops);

    // Apply luck to drop count
    if (luck > 0) {
        float bonus = luck * 0.1f;  // 10% extra drops per luck point
        if (random_float() < bonus) {
            num_drops++;
        }
    }

    // Generate drops
    for (std::uint32_t i = 0; i < num_drops; ++i) {
        // Calculate total weight
        float total_weight = table->total_weight();
        float roll = random_float() * total_weight;

        float cumulative = 0;
        for (const auto& entry : table->entries) {
            cumulative += entry.weight;
            if (roll <= cumulative) {
                // Check independent chance
                if (random_float() <= entry.chance) {
                    ItemInstance item = generate_item(entry, luck);
                    if (item.id) {
                        result.push_back(std::move(item));
                    }
                }
                break;
            }
        }
    }

    // Ensure at least one drop if guaranteed
    if (result.empty() && table->guaranteed_drop && !table->entries.empty()) {
        const auto& entry = table->entries[random_uint(0, static_cast<std::uint32_t>(table->entries.size()) - 1)];
        ItemInstance item = generate_item(entry, luck);
        if (item.id) {
            result.push_back(std::move(item));
        }
    }

    return result;
}

std::vector<ItemInstance> LootGenerator::generate_count(const std::string& table_name, std::uint32_t count) {
    std::vector<ItemInstance> result;

    const LootTable* table = get_table(table_name);
    if (!table || table->entries.empty()) {
        return result;
    }

    for (std::uint32_t i = 0; i < count; ++i) {
        float total_weight = table->total_weight();
        float roll = random_float() * total_weight;

        float cumulative = 0;
        for (const auto& entry : table->entries) {
            cumulative += entry.weight;
            if (roll <= cumulative) {
                ItemInstance item = generate_item(entry, 0);
                if (item.id) {
                    result.push_back(std::move(item));
                }
                break;
            }
        }
    }

    return result;
}

ItemInstance LootGenerator::generate_item(const LootEntry& entry, float luck) {
    if (!m_factory) {
        return ItemInstance{};
    }

    // Determine quantity
    std::uint32_t quantity = random_uint(entry.min_quantity, entry.max_quantity);

    // Create item
    ItemInstance item = m_factory->create(entry.item, quantity);

    // Determine rarity
    ItemRarity rarity = entry.min_rarity;
    float rarity_roll = random_float();
    rarity_roll -= luck * 0.05f;  // Luck improves rarity

    int rarity_diff = static_cast<int>(entry.max_rarity) - static_cast<int>(entry.min_rarity);
    if (rarity_diff > 0) {
        float rarity_step = 1.0f / (rarity_diff + 1);
        for (int i = 0; i <= rarity_diff; ++i) {
            if (rarity_roll < rarity_step * (i + 1)) {
                rarity = static_cast<ItemRarity>(static_cast<int>(entry.min_rarity) + i);
                break;
            }
        }
    }
    item.rarity = rarity;

    // Determine quality
    item.quality = entry.quality_min + random_float() * (entry.quality_max - entry.quality_min);

    // Apply modifiers from pool
    std::uint32_t modifier_count = random_uint(entry.modifier_count_min, entry.modifier_count_max);
    if (modifier_count > 0 && m_factory) {
        m_factory->apply_random_modifiers(item, modifier_count);
    }

    return item;
}

void LootGenerator::set_seed(std::uint64_t seed) {
    m_seed = seed;
    m_state = seed;
}

float LootGenerator::random_float() {
    // Simple xorshift
    m_state ^= m_state << 13;
    m_state ^= m_state >> 17;
    m_state ^= m_state << 5;
    return static_cast<float>(m_state & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

std::uint32_t LootGenerator::random_uint(std::uint32_t min, std::uint32_t max) {
    if (min >= max) return min;
    return min + static_cast<std::uint32_t>(random_float() * (max - min + 1));
}

// =============================================================================
// Shop Implementation
// =============================================================================

Shop::Shop() = default;

Shop::Shop(const std::string& name)
    : m_name(name) {
}

Shop::~Shop() = default;

void Shop::add_item(const ShopItem& item) {
    m_items.push_back(item);
}

bool Shop::remove_item(ItemDefId item) {
    auto it = std::remove_if(m_items.begin(), m_items.end(),
        [item](const ShopItem& si) { return si.item == item; });
    if (it != m_items.end()) {
        m_items.erase(it, m_items.end());
        return true;
    }
    return false;
}

const ShopItem* Shop::get_item(ItemDefId item) const {
    for (const auto& si : m_items) {
        if (si.item == item) {
            return &si;
        }
    }
    return nullptr;
}

ShopItem* Shop::get_item_mut(ItemDefId item) {
    for (auto& si : m_items) {
        if (si.item == item) {
            return &si;
        }
    }
    return nullptr;
}

TransactionResult Shop::buy(ItemDefId item, std::uint32_t quantity,
                            InventoryComponent& buyer, ItemDefId currency) {
    const ShopItem* shop_item = get_item(item);
    if (!shop_item) {
        return TransactionResult::InvalidItem;
    }

    // Check stock
    if (shop_item->stock > 0 && shop_item->stock < quantity) {
        return TransactionResult::InvalidQuantity;
    }

    // Calculate price
    std::uint64_t total_price = get_buy_price(item, quantity);

    // Check if buyer can afford
    if (!buyer.can_afford(currency, total_price)) {
        return TransactionResult::PermissionDenied;  // Can't afford
    }

    // Create item
    if (!m_factory) {
        return TransactionResult::Failed;
    }

    ItemInstance purchased = m_factory->create(item, quantity);
    if (m_item_db) {
        m_item_db->store(purchased);
    }

    // Deduct currency
    buyer.remove_currency(currency, total_price);

    // Add to buyer inventory
    auto result = buyer.pickup(purchased.id, quantity);

    // Update stock
    if (result == TransactionResult::Success) {
        ShopItem* si = get_item_mut(item);
        if (si && si->stock > 0) {
            si->stock -= quantity;
        }
    }

    return result;
}

TransactionResult Shop::sell(ItemInstanceId item, std::uint32_t quantity,
                             InventoryComponent& seller, ItemDefId currency) {
    if (!m_item_db) {
        return TransactionResult::Failed;
    }

    auto opt = m_item_db->retrieve(item);
    if (!opt) {
        return TransactionResult::InvalidItem;
    }

    // Calculate sell price
    std::uint64_t total_price = get_sell_price(*opt, quantity);

    // Remove from seller
    auto slot = seller.main_inventory().find_item(item);
    if (!slot) {
        return TransactionResult::InvalidItem;
    }

    seller.main_inventory().remove(*slot, quantity);

    // Add currency to seller
    seller.add_currency(currency, total_price);

    return TransactionResult::Success;
}

std::uint64_t Shop::get_buy_price(ItemDefId item, std::uint32_t quantity) const {
    const ShopItem* shop_item = get_item(item);
    if (!shop_item) {
        return 0;
    }

    return static_cast<std::uint64_t>(shop_item->buy_price * shop_item->price_multiplier * m_buy_multiplier * quantity);
}

std::uint64_t Shop::get_sell_price(const ItemInstance& item, std::uint32_t quantity) const {
    const ShopItem* shop_item = get_item(item.def_id);
    if (shop_item) {
        return static_cast<std::uint64_t>(shop_item->sell_price * m_sell_multiplier * quantity);
    }

    // Default to item value * sell multiplier
    return static_cast<std::uint64_t>(item.calculate_value() * m_sell_multiplier * quantity);
}

void Shop::set_reputation_discount(float reputation, float discount) {
    m_reputation_discounts.emplace_back(reputation, discount);
    std::sort(m_reputation_discounts.begin(), m_reputation_discounts.end());
}

float Shop::get_discount(float reputation) const {
    float discount = 0;
    for (const auto& [rep, disc] : m_reputation_discounts) {
        if (reputation >= rep) {
            discount = disc;
        } else {
            break;
        }
    }
    return discount;
}

void Shop::update(float dt) {
    for (auto& item : m_items) {
        if (item.restocks && item.stock < item.max_stock) {
            item.restock_timer += dt;
            if (item.restock_timer >= item.restock_time) {
                item.stock = item.max_stock;
                item.restock_timer = 0;
            }
        }
    }
}

// =============================================================================
// InventorySystem Implementation
// =============================================================================

InventorySystem::InventorySystem()
    : InventorySystem(InventoryConfig{}) {
}

InventorySystem::InventorySystem(const InventoryConfig& config)
    : m_config(config)
    , m_item_factory(&m_item_registry)
    , m_item_database(&m_item_registry, &m_item_factory) {

    m_containers.set_item_database(&m_item_database);
    m_loot_generator.set_item_factory(&m_item_factory);
    m_loot_generator.set_item_registry(&m_item_registry);
    m_crafting_previewer.set_recipe_registry(&m_recipe_registry);
    m_crafting_previewer.set_item_database(&m_item_database);
}

InventorySystem::~InventorySystem() = default;

InventoryComponent* InventorySystem::create_inventory(EntityId entity) {
    auto inventory = std::make_unique<InventoryComponent>(entity);
    inventory->set_item_database(&m_item_database);
    inventory->set_item_factory(&m_item_factory);
    inventory->set_recipe_registry(&m_recipe_registry);
    inventory->set_set_registry(&m_set_registry);

    auto* ptr = inventory.get();
    m_inventories[entity] = std::move(inventory);
    return ptr;
}

InventoryComponent* InventorySystem::get_inventory(EntityId entity) {
    auto it = m_inventories.find(entity);
    return it != m_inventories.end() ? it->second.get() : nullptr;
}

const InventoryComponent* InventorySystem::get_inventory(EntityId entity) const {
    auto it = m_inventories.find(entity);
    return it != m_inventories.end() ? it->second.get() : nullptr;
}

bool InventorySystem::remove_inventory(EntityId entity) {
    return m_inventories.erase(entity) > 0;
}

Shop* InventorySystem::create_shop(const std::string& name) {
    auto shop = std::make_unique<Shop>(name);
    shop->set_item_factory(&m_item_factory);
    shop->set_item_database(&m_item_database);

    auto* ptr = shop.get();
    m_shops[name] = std::move(shop);
    return ptr;
}

Shop* InventorySystem::get_shop(const std::string& name) {
    auto it = m_shops.find(name);
    return it != m_shops.end() ? it->second.get() : nullptr;
}

bool InventorySystem::remove_shop(const std::string& name) {
    return m_shops.erase(name) > 0;
}

CraftingStation* InventorySystem::create_station(const CraftingStationDef& def) {
    CraftingStationDef registered = def;
    registered.id = CraftingStationId{m_next_station_id++};

    auto station = std::make_unique<CraftingStation>(registered);
    station->set_recipe_registry(&m_recipe_registry);
    station->set_item_factory(&m_item_factory);

    auto* ptr = station.get();
    m_stations[registered.id] = std::move(station);
    return ptr;
}

CraftingStation* InventorySystem::get_station(CraftingStationId id) {
    auto it = m_stations.find(id);
    return it != m_stations.end() ? it->second.get() : nullptr;
}

bool InventorySystem::remove_station(CraftingStationId id) {
    return m_stations.erase(id) > 0;
}

TransactionResult InventorySystem::transfer(EntityId from, EntityId to,
                                            std::uint32_t from_slot, std::uint32_t to_slot,
                                            std::uint32_t quantity) {
    auto* from_inv = get_inventory(from);
    auto* to_inv = get_inventory(to);

    if (!from_inv || !to_inv) {
        return TransactionResult::InvalidSlot;
    }

    ItemInstanceId item = from_inv->main_inventory().get_item(from_slot);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    auto result = to_inv->main_inventory().add_to_slot(to_slot, item, quantity);
    if (result == TransactionResult::Success) {
        from_inv->main_inventory().remove(from_slot, quantity);

        InventoryTransaction txn;
        txn.type = TransactionType::Move;
        txn.result = TransactionResult::Success;
        txn.item = item;
        txn.quantity = quantity;
        txn.actual_quantity = quantity;
        txn.timestamp = m_current_time;
        log_transaction(txn);
    }

    return result;
}

TransactionResult InventorySystem::trade(EntityId entity_a, const std::vector<std::uint32_t>& slots_a,
                                         EntityId entity_b, const std::vector<std::uint32_t>& slots_b) {
    auto* inv_a = get_inventory(entity_a);
    auto* inv_b = get_inventory(entity_b);

    if (!inv_a || !inv_b) {
        return TransactionResult::InvalidSlot;
    }

    // Collect items to trade
    std::vector<std::pair<ItemInstanceId, std::uint32_t>> items_a;
    std::vector<std::pair<ItemInstanceId, std::uint32_t>> items_b;

    for (auto slot : slots_a) {
        ItemInstanceId item = inv_a->main_inventory().get_item(slot);
        std::uint32_t qty = inv_a->main_inventory().get_quantity(slot);
        if (item) {
            items_a.emplace_back(item, qty);
        }
    }

    for (auto slot : slots_b) {
        ItemInstanceId item = inv_b->main_inventory().get_item(slot);
        std::uint32_t qty = inv_b->main_inventory().get_quantity(slot);
        if (item) {
            items_b.emplace_back(item, qty);
        }
    }

    // Remove items from both
    for (auto slot : slots_a) {
        inv_a->main_inventory().remove(slot, inv_a->main_inventory().get_quantity(slot));
    }
    for (auto slot : slots_b) {
        inv_b->main_inventory().remove(slot, inv_b->main_inventory().get_quantity(slot));
    }

    // Add items to opposite inventories
    for (const auto& [item, qty] : items_a) {
        inv_b->pickup(item, qty);
    }
    for (const auto& [item, qty] : items_b) {
        inv_a->pickup(item, qty);
    }

    m_stats.total_trades++;
    return TransactionResult::Success;
}

ItemInstanceId InventorySystem::spawn_world_item(ItemDefId def, std::uint32_t quantity,
                                                  float x, float y, float z) {
    ItemInstance item = m_item_factory.create(def, quantity);
    m_item_database.store(item);

    WorldItem wi;
    wi.item = item.id;
    wi.x = x;
    wi.y = y;
    wi.z = z;
    wi.spawn_time = m_current_time;
    wi.despawn_time = 300.0f;  // 5 minutes

    m_world_items[item.id] = wi;
    m_stats.world_items = m_world_items.size();
    m_stats.total_items_created++;

    return item.id;
}

bool InventorySystem::despawn_world_item(ItemInstanceId item) {
    auto it = m_world_items.find(item);
    if (it == m_world_items.end()) {
        return false;
    }

    m_world_items.erase(it);
    m_item_database.remove(item);
    m_stats.world_items = m_world_items.size();
    m_stats.total_items_destroyed++;

    return true;
}

std::vector<ItemInstanceId> InventorySystem::get_world_items_in_radius(float x, float y, float z, float radius) const {
    std::vector<ItemInstanceId> result;
    float radius_sq = radius * radius;

    for (const auto& [id, wi] : m_world_items) {
        float dx = wi.x - x;
        float dy = wi.y - y;
        float dz = wi.z - z;
        float dist_sq = dx * dx + dy * dy + dz * dz;

        if (dist_sq <= radius_sq) {
            result.push_back(id);
        }
    }

    return result;
}

void InventorySystem::update(float dt) {
    m_current_time += dt;

    // Update shops
    for (auto& [name, shop] : m_shops) {
        shop->update(dt);
    }

    // Update crafting stations
    for (auto& [id, station] : m_stations) {
        station->update(dt);
    }

    // Despawn old world items
    std::vector<ItemInstanceId> to_despawn;
    for (const auto& [id, wi] : m_world_items) {
        if (m_current_time - wi.spawn_time > wi.despawn_time) {
            to_despawn.push_back(id);
        }
    }
    for (auto id : to_despawn) {
        despawn_world_item(id);
    }

    // Update stats
    m_stats.active_containers = m_containers.all_containers().size();
}

void InventorySystem::clear_old_transactions(double max_age) {
    while (!m_transaction_log.empty()) {
        if (m_current_time - m_transaction_log.front().timestamp > max_age) {
            m_transaction_log.pop_front();
        } else {
            break;
        }
    }
}

InventorySystem::Snapshot InventorySystem::take_snapshot() const {
    Snapshot snapshot;

    // Items
    snapshot.items = m_item_database.serialize();

    // Entity inventories
    for (const auto& [entity, inv] : m_inventories) {
        snapshot.entity_inventories.emplace_back(entity.value, inv->serialize());
    }

    return snapshot;
}

void InventorySystem::apply_snapshot(const Snapshot& snapshot) {
    // Restore items
    m_item_database.deserialize(snapshot.items);

    // Restore inventories
    for (const auto& [entity_id, inv_data] : snapshot.entity_inventories) {
        EntityId entity{entity_id};
        auto* inv = get_inventory(entity);
        if (!inv) {
            inv = create_inventory(entity);
        }
        inv->deserialize(inv_data);
    }
}

void InventorySystem::setup_preset_items() {
    m_item_registry.register_item(ItemRegistry::preset_health_potion());
    m_item_registry.register_item(ItemRegistry::preset_mana_potion());
    m_item_registry.register_item(ItemRegistry::preset_gold_coin());
    m_item_registry.register_item(ItemRegistry::preset_iron_sword());
    m_item_registry.register_item(ItemRegistry::preset_leather_armor());
    m_item_registry.register_item(ItemRegistry::preset_iron_ore());
    m_item_registry.register_item(ItemRegistry::preset_wood_plank());
}

void InventorySystem::setup_preset_recipes() {
    m_recipe_registry.register_recipe(RecipeRegistry::preset_iron_sword());
    m_recipe_registry.register_recipe(RecipeRegistry::preset_leather_armor());
    m_recipe_registry.register_recipe(RecipeRegistry::preset_health_potion());
    m_recipe_registry.register_recipe(RecipeRegistry::preset_iron_ingot());
}

void InventorySystem::setup_preset_equipment_sets() {
    m_set_registry.register_set("Iron Set", EquipmentSetRegistry::preset_iron_set());
    m_set_registry.register_set("Leather Set", EquipmentSetRegistry::preset_leather_set());
}

void InventorySystem::log_transaction(const InventoryTransaction& transaction) {
    m_transaction_log.push_back(transaction);
    m_stats.total_transactions++;

    // Trim old entries
    if (m_config.transaction_log_retention > 0) {
        clear_old_transactions(m_config.transaction_log_retention);
    }
}

} // namespace void_inventory
