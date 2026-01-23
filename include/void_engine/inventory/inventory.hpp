/// @file inventory.hpp
/// @brief Main inventory system header for void_inventory module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "items.hpp"
#include "containers.hpp"
#include "equipment.hpp"
#include "crafting.hpp"

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_inventory {

// =============================================================================
// InventoryComponent - Per-Entity Inventory
// =============================================================================

/// @brief Component managing an entity's inventory
class InventoryComponent {
public:
    InventoryComponent();
    explicit InventoryComponent(EntityId owner);
    ~InventoryComponent();

    // Containers
    /// @brief Get main inventory container
    Container& main_inventory() { return m_main_inventory; }
    const Container& main_inventory() const { return m_main_inventory; }

    /// @brief Get hotbar container
    Container& hotbar() { return m_hotbar; }
    const Container& hotbar() const { return m_hotbar; }

    /// @brief Get equipment component
    EquipmentComponent& equipment() { return m_equipment; }
    const EquipmentComponent& equipment() const { return m_equipment; }

    /// @brief Get crafting component
    CraftingComponent& crafting() { return m_crafting; }
    const CraftingComponent& crafting() const { return m_crafting; }

    // Quick operations
    /// @brief Pick up an item
    TransactionResult pickup(ItemInstanceId item, std::uint32_t quantity = 1);

    /// @brief Drop an item
    TransactionResult drop(std::uint32_t slot, std::uint32_t quantity = 1);

    /// @brief Use an item
    bool use_item(std::uint32_t slot, EntityId target = EntityId{});

    /// @brief Use hotbar slot
    bool use_hotbar(std::uint32_t slot, EntityId target = EntityId{});

    /// @brief Equip from inventory
    TransactionResult equip_from_slot(std::uint32_t inv_slot, EquipmentSlotId equip_slot);

    /// @brief Unequip to inventory
    TransactionResult unequip_to_slot(EquipmentSlotId equip_slot, std::uint32_t inv_slot);

    // Queries
    /// @brief Count items by definition
    std::uint32_t count_items(ItemDefId def) const;

    /// @brief Check if has item
    bool has_item(ItemDefId def, std::uint32_t quantity = 1) const;

    /// @brief Get total weight
    float total_weight() const;

    /// @brief Get total value
    std::uint64_t total_value() const;

    // Currency
    /// @brief Get currency amount
    std::uint64_t get_currency(ItemDefId currency_def) const;

    /// @brief Add currency
    bool add_currency(ItemDefId currency_def, std::uint64_t amount);

    /// @brief Remove currency
    bool remove_currency(ItemDefId currency_def, std::uint64_t amount);

    /// @brief Check if can afford
    bool can_afford(ItemDefId currency_def, std::uint64_t amount) const;

    // Configuration
    void set_main_inventory_size(std::size_t size);
    void set_hotbar_size(std::size_t size);
    void set_weight_limit(float limit);

    // Dependencies
    void set_item_database(ItemDatabase* db);
    void set_item_factory(ItemFactory* factory) { m_factory = factory; }
    void set_recipe_registry(RecipeRegistry* registry);
    void set_set_registry(EquipmentSetRegistry* registry);

    // Events
    void set_on_pickup(ItemPickupCallback callback) { m_on_pickup = std::move(callback); }
    void set_on_drop(ItemDropCallback callback) { m_on_drop = std::move(callback); }
    void set_on_use(std::function<void(const ItemUseEvent&)> callback) { m_on_use = std::move(callback); }

    // Owner
    EntityId owner() const { return m_owner; }

    // Serialization
    struct SerializedInventory {
        std::vector<std::pair<std::uint32_t, std::uint64_t>> main_slots;      // slot -> item id
        std::vector<std::pair<std::uint32_t, std::uint64_t>> hotbar_slots;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> equipped_items;  // slot id -> item id
        std::vector<std::uint64_t> known_recipes;
    };

    SerializedInventory serialize() const;
    void deserialize(const SerializedInventory& data);

private:
    EntityId m_owner;
    Container m_main_inventory;
    Container m_hotbar;
    EquipmentComponent m_equipment;
    CraftingComponent m_crafting;

    ItemDatabase* m_item_db{nullptr};
    ItemFactory* m_factory{nullptr};
    float m_weight_limit{100.0f};

    ItemPickupCallback m_on_pickup;
    ItemDropCallback m_on_drop;
    std::function<void(const ItemUseEvent&)> m_on_use;
};

// =============================================================================
// LootTable - Random Loot Generation
// =============================================================================

/// @brief Entry in a loot table
struct LootEntry {
    ItemDefId item;
    std::uint32_t min_quantity{1};
    std::uint32_t max_quantity{1};
    float weight{1.0f};                     ///< Relative weight for selection
    float chance{1.0f};                     ///< Independent chance (0-1)
    ItemRarity min_rarity{ItemRarity::Common};
    ItemRarity max_rarity{ItemRarity::Common};
    float quality_min{1.0f};
    float quality_max{1.0f};
    std::uint32_t modifier_count_min{0};
    std::uint32_t modifier_count_max{0};
    std::vector<std::string> conditions;    ///< Required conditions
};

/// @brief Loot table for generating random items
struct LootTable {
    std::string name;
    std::vector<LootEntry> entries;
    std::uint32_t min_drops{1};
    std::uint32_t max_drops{1};
    bool guaranteed_drop{true};             ///< At least one item guaranteed

    /// @brief Add an entry
    void add_entry(const LootEntry& entry);

    /// @brief Calculate total weight
    float total_weight() const;
};

/// @brief Generates loot from tables
class LootGenerator {
public:
    LootGenerator();
    ~LootGenerator();

    /// @brief Register a loot table
    void register_table(const std::string& name, const LootTable& table);

    /// @brief Get loot table
    const LootTable* get_table(const std::string& name) const;

    /// @brief Generate loot from table
    std::vector<ItemInstance> generate(const std::string& table_name);

    /// @brief Generate with luck modifier
    std::vector<ItemInstance> generate(const std::string& table_name, float luck);

    /// @brief Generate specific number of items
    std::vector<ItemInstance> generate_count(const std::string& table_name, std::uint32_t count);

    void set_item_factory(ItemFactory* factory) { m_factory = factory; }
    void set_item_registry(ItemRegistry* registry) { m_registry = registry; }

    // Random seed
    void set_seed(std::uint64_t seed);

private:
    ItemInstance generate_item(const LootEntry& entry, float luck);
    float random_float();
    std::uint32_t random_uint(std::uint32_t min, std::uint32_t max);

    std::unordered_map<std::string, LootTable> m_tables;
    ItemFactory* m_factory{nullptr};
    ItemRegistry* m_registry{nullptr};
    std::uint64_t m_seed{12345};
    std::uint64_t m_state{12345};
};

// =============================================================================
// Shop - Trading System
// =============================================================================

/// @brief Item for sale in shop
struct ShopItem {
    ItemDefId item;
    std::uint32_t stock{0};                 ///< 0 = unlimited
    std::uint32_t max_stock{0};
    std::uint64_t buy_price{0};
    std::uint64_t sell_price{0};
    float price_multiplier{1.0f};
    bool restocks{true};
    float restock_time{3600.0f};            ///< Seconds to restock
    float restock_timer{0};
    std::vector<std::string> requirements;
};

/// @brief Shop instance
class Shop {
public:
    Shop();
    explicit Shop(const std::string& name);
    ~Shop();

    // Identity
    const std::string& name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }

    // Inventory
    /// @brief Add item to shop
    void add_item(const ShopItem& item);

    /// @brief Remove item from shop
    bool remove_item(ItemDefId item);

    /// @brief Get shop items
    const std::vector<ShopItem>& items() const { return m_items; }

    /// @brief Get specific item
    const ShopItem* get_item(ItemDefId item) const;
    ShopItem* get_item_mut(ItemDefId item);

    // Trading
    /// @brief Buy item from shop
    TransactionResult buy(ItemDefId item, std::uint32_t quantity,
                          InventoryComponent& buyer, ItemDefId currency);

    /// @brief Sell item to shop
    TransactionResult sell(ItemInstanceId item, std::uint32_t quantity,
                           InventoryComponent& seller, ItemDefId currency);

    /// @brief Get buy price
    std::uint64_t get_buy_price(ItemDefId item, std::uint32_t quantity) const;

    /// @brief Get sell price
    std::uint64_t get_sell_price(const ItemInstance& item, std::uint32_t quantity) const;

    // Price modifiers
    void set_buy_multiplier(float mult) { m_buy_multiplier = mult; }
    void set_sell_multiplier(float mult) { m_sell_multiplier = mult; }
    float buy_multiplier() const { return m_buy_multiplier; }
    float sell_multiplier() const { return m_sell_multiplier; }

    // Reputation
    void set_reputation_discount(float reputation, float discount);
    float get_discount(float reputation) const;

    // Update
    void update(float dt);

    // Dependencies
    void set_item_factory(ItemFactory* factory) { m_factory = factory; }
    void set_item_database(ItemDatabase* db) { m_item_db = db; }

private:
    std::string m_name;
    std::vector<ShopItem> m_items;
    float m_buy_multiplier{1.0f};
    float m_sell_multiplier{0.5f};
    std::vector<std::pair<float, float>> m_reputation_discounts;

    ItemFactory* m_factory{nullptr};
    ItemDatabase* m_item_db{nullptr};
};

// =============================================================================
// InventorySystem - Global System
// =============================================================================

/// @brief Main inventory system
class InventorySystem {
public:
    InventorySystem();
    explicit InventorySystem(const InventoryConfig& config);
    ~InventorySystem();

    // Configuration
    const InventoryConfig& config() const { return m_config; }
    void set_config(const InventoryConfig& config) { m_config = config; }

    // Registries
    ItemRegistry& item_registry() { return m_item_registry; }
    const ItemRegistry& item_registry() const { return m_item_registry; }

    RecipeRegistry& recipe_registry() { return m_recipe_registry; }
    const RecipeRegistry& recipe_registry() const { return m_recipe_registry; }

    EquipmentSetRegistry& set_registry() { return m_set_registry; }
    const EquipmentSetRegistry& set_registry() const { return m_set_registry; }

    // Factory and database
    ItemFactory& item_factory() { return m_item_factory; }
    const ItemFactory& item_factory() const { return m_item_factory; }

    ItemDatabase& item_database() { return m_item_database; }
    const ItemDatabase& item_database() const { return m_item_database; }

    // Container management
    ContainerManager& containers() { return m_containers; }
    const ContainerManager& containers() const { return m_containers; }

    // Loot generation
    LootGenerator& loot_generator() { return m_loot_generator; }
    const LootGenerator& loot_generator() const { return m_loot_generator; }

    // Crafting previewer
    CraftingPreviewer& crafting_previewer() { return m_crafting_previewer; }

    // Entity inventories
    /// @brief Create inventory for entity
    InventoryComponent* create_inventory(EntityId entity);

    /// @brief Get entity inventory
    InventoryComponent* get_inventory(EntityId entity);
    const InventoryComponent* get_inventory(EntityId entity) const;

    /// @brief Remove entity inventory
    bool remove_inventory(EntityId entity);

    // Shops
    /// @brief Create a shop
    Shop* create_shop(const std::string& name);

    /// @brief Get shop by name
    Shop* get_shop(const std::string& name);

    /// @brief Remove shop
    bool remove_shop(const std::string& name);

    // Crafting stations
    /// @brief Create a crafting station
    CraftingStation* create_station(const CraftingStationDef& def);

    /// @brief Get station by ID
    CraftingStation* get_station(CraftingStationId id);

    /// @brief Remove station
    bool remove_station(CraftingStationId id);

    // Global operations
    /// @brief Transfer items between entities
    TransactionResult transfer(EntityId from, EntityId to,
                               std::uint32_t from_slot, std::uint32_t to_slot,
                               std::uint32_t quantity);

    /// @brief Trade items between entities
    TransactionResult trade(EntityId entity_a, const std::vector<std::uint32_t>& slots_a,
                            EntityId entity_b, const std::vector<std::uint32_t>& slots_b);

    // World items (dropped)
    /// @brief Spawn item in world
    ItemInstanceId spawn_world_item(ItemDefId def, std::uint32_t quantity,
                                    float x, float y, float z);

    /// @brief Despawn world item
    bool despawn_world_item(ItemInstanceId item);

    /// @brief Get world items in radius
    std::vector<ItemInstanceId> get_world_items_in_radius(float x, float y, float z, float radius) const;

    // Update
    void update(float dt);

    // Transaction log
    /// @brief Get recent transactions
    const std::deque<InventoryTransaction>& transaction_log() const { return m_transaction_log; }

    /// @brief Clear old transactions
    void clear_old_transactions(double max_age);

    // Statistics
    struct Stats {
        std::uint64_t total_items_created{0};
        std::uint64_t total_items_destroyed{0};
        std::uint64_t total_transactions{0};
        std::uint64_t total_crafts{0};
        std::uint64_t total_trades{0};
        std::size_t active_containers{0};
        std::size_t world_items{0};
    };

    const Stats& stats() const { return m_stats; }

    // Serialization
    struct Snapshot {
        std::vector<ItemDatabase::SerializedItem> items;
        std::vector<std::pair<std::uint64_t, InventoryComponent::SerializedInventory>> entity_inventories;
    };

    Snapshot take_snapshot() const;
    void apply_snapshot(const Snapshot& snapshot);

    // Setup presets
    void setup_preset_items();
    void setup_preset_recipes();
    void setup_preset_equipment_sets();

private:
    void log_transaction(const InventoryTransaction& transaction);

    InventoryConfig m_config;

    ItemRegistry m_item_registry;
    RecipeRegistry m_recipe_registry;
    EquipmentSetRegistry m_set_registry;

    ItemFactory m_item_factory;
    ItemDatabase m_item_database;
    ContainerManager m_containers;
    LootGenerator m_loot_generator;
    CraftingPreviewer m_crafting_previewer;

    std::unordered_map<EntityId, std::unique_ptr<InventoryComponent>> m_inventories;
    std::unordered_map<std::string, std::unique_ptr<Shop>> m_shops;
    std::unordered_map<CraftingStationId, std::unique_ptr<CraftingStation>> m_stations;

    // World items
    struct WorldItem {
        ItemInstanceId item;
        float x, y, z;
        double spawn_time;
        float despawn_time;
    };
    std::unordered_map<ItemInstanceId, WorldItem> m_world_items;

    std::deque<InventoryTransaction> m_transaction_log;
    Stats m_stats;
    double m_current_time{0};
    std::uint64_t m_next_station_id{1};
};

// =============================================================================
// Prelude - Convenient Namespace
// =============================================================================

namespace prelude {
    using void_inventory::ItemCategory;
    using void_inventory::ItemRarity;
    using void_inventory::ItemFlags;
    using void_inventory::EquipmentSlotType;
    using void_inventory::StatType;
    using void_inventory::ModifierType;
    using void_inventory::ContainerType;
    using void_inventory::StationType;
    using void_inventory::RecipeDifficulty;
    using void_inventory::CraftingResult;
    using void_inventory::TransactionType;
    using void_inventory::TransactionResult;

    using void_inventory::ItemDefId;
    using void_inventory::ItemInstanceId;
    using void_inventory::ContainerId;
    using void_inventory::EquipmentSlotId;
    using void_inventory::RecipeId;
    using void_inventory::CraftingStationId;
    using void_inventory::EntityId;

    using void_inventory::ItemDef;
    using void_inventory::ItemInstance;
    using void_inventory::ItemStack;
    using void_inventory::ItemModifier;
    using void_inventory::StatModifier;
    using void_inventory::Recipe;
    using void_inventory::RecipeIngredient;
    using void_inventory::RecipeOutput;
    using void_inventory::LootTable;
    using void_inventory::LootEntry;

    using void_inventory::ItemRegistry;
    using void_inventory::ItemFactory;
    using void_inventory::ItemDatabase;
    using void_inventory::RecipeRegistry;
    using void_inventory::EquipmentSetRegistry;

    using void_inventory::IContainer;
    using void_inventory::Container;
    using void_inventory::GridContainer;
    using void_inventory::WeightedContainer;
    using void_inventory::FilteredContainer;
    using void_inventory::SortedContainer;
    using void_inventory::ContainerManager;

    using void_inventory::EquipmentComponent;
    using void_inventory::LoadoutManager;
    using void_inventory::CharacterStats;

    using void_inventory::CraftingStation;
    using void_inventory::CraftingComponent;
    using void_inventory::CraftingQueue;
    using void_inventory::CraftingPreviewer;

    using void_inventory::InventoryComponent;
    using void_inventory::LootGenerator;
    using void_inventory::Shop;
    using void_inventory::InventorySystem;
}

} // namespace void_inventory
