/// @file items.hpp
/// @brief Item definitions, instances, and registry for void_inventory module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <any>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_inventory {

// =============================================================================
// ItemDef - Item Definition (Template)
// =============================================================================

/// @brief Definition/template for an item type
struct ItemDef {
    ItemDefId id;
    std::string internal_name;              ///< Unique internal identifier
    std::string display_name;               ///< Localized display name
    std::string description;                ///< Localized description
    std::string icon_path;                  ///< Path to icon asset
    std::string mesh_path;                  ///< Path to 3D mesh

    ItemCategory category{ItemCategory::Misc};
    ItemRarity base_rarity{ItemRarity::Common};
    ItemFlags flags{ItemFlags::None};

    // Stacking
    std::uint32_t max_stack{1};
    bool stackable() const { return max_stack > 1; }

    // Value
    std::uint64_t base_value{0};            ///< Base currency value
    float weight{0.0f};                     ///< Weight per unit

    // Grid size (for grid inventories)
    GridSize grid_size{1, 1};

    // Equipment
    EquipmentSlotType equip_slot{EquipmentSlotType::None};
    std::vector<EquipmentSlotType> alternate_slots;
    std::vector<StatModifier> base_stats;
    std::vector<ItemRequirement> requirements;

    // Consumable
    float cooldown{0};                      ///< Use cooldown in seconds
    std::string cooldown_group;             ///< Shared cooldown group
    ItemUseCallback on_use;

    // Custom properties
    std::unordered_map<std::string, ItemProperty> properties;

    // Tags for filtering/searching
    std::vector<std::string> tags;

    // Set bonuses
    std::string equipment_set;              ///< Name of equipment set

    bool has_tag(std::string_view tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }

    template<typename T>
    T get_property(const std::string& name, const T& default_value = T{}) const {
        auto it = properties.find(name);
        if (it != properties.end()) {
            return it->second.get<T>(default_value);
        }
        return default_value;
    }

    bool is_equippable() const {
        return has_flag(flags, ItemFlags::Equippable) && equip_slot != EquipmentSlotType::None;
    }

    bool is_consumable() const {
        return has_flag(flags, ItemFlags::Consumable);
    }
};

// =============================================================================
// ItemInstance - Individual Item Instance
// =============================================================================

/// @brief Individual instance of an item
struct ItemInstance {
    ItemInstanceId id;
    ItemDefId def_id;
    const ItemDef* def{nullptr};            ///< Cached pointer to definition

    std::uint32_t quantity{1};
    float durability{1.0f};                 ///< 0-1, 1 = full
    float max_durability{1.0f};
    float quality{1.0f};                    ///< Quality multiplier

    ItemRarity rarity{ItemRarity::Common};  ///< Instance rarity (may differ from base)

    // Modifiers applied to this instance
    std::vector<ItemModifier> modifiers;
    std::vector<StatModifier> bonus_stats;

    // Instance-specific properties
    std::unordered_map<std::string, ItemProperty> instance_properties;

    // Ownership/binding
    EntityId owner;
    EntityId bound_to;                      ///< Soulbound target
    bool soulbound{false};

    // Tracking
    double created_time{0};
    double last_used_time{0};
    std::uint32_t use_count{0};
    std::string source;                     ///< Where item came from

    // Cooldown tracking
    double cooldown_end{0};

    // Custom data
    std::any custom_data;

    bool is_on_cooldown(double current_time) const {
        return cooldown_end > current_time;
    }

    float remaining_cooldown(double current_time) const {
        return cooldown_end > current_time ? static_cast<float>(cooldown_end - current_time) : 0;
    }

    template<typename T>
    T get_instance_property(const std::string& name, const T& default_value = T{}) const {
        auto it = instance_properties.find(name);
        if (it != instance_properties.end()) {
            return it->second.get<T>(default_value);
        }
        return default_value;
    }

    template<typename T>
    void set_instance_property(const std::string& name, const T& value) {
        instance_properties[name].set<T>(value);
    }

    /// @brief Calculate total stat modifiers including base and bonuses
    std::vector<StatModifier> get_all_stats() const {
        std::vector<StatModifier> result;
        if (def) {
            result = def->base_stats;
        }
        result.insert(result.end(), bonus_stats.begin(), bonus_stats.end());
        for (const auto& mod : modifiers) {
            for (const auto& [stat, value] : mod.stat_bonuses) {
                StatModifier sm;
                sm.stat = stat;
                sm.type = ModifierType::Flat;
                sm.value = value;
                sm.source = mod.name;
                result.push_back(sm);
            }
        }
        return result;
    }

    /// @brief Calculate total value including quality and modifiers
    std::uint64_t calculate_value() const {
        if (!def) return 0;
        float value = static_cast<float>(def->base_value) * quality;
        for (const auto& mod : modifiers) {
            value *= mod.value_multiplier;
        }
        // Rarity multiplier
        switch (rarity) {
            case ItemRarity::Uncommon: value *= 2.0f; break;
            case ItemRarity::Rare: value *= 5.0f; break;
            case ItemRarity::Epic: value *= 15.0f; break;
            case ItemRarity::Legendary: value *= 50.0f; break;
            case ItemRarity::Mythic: value *= 200.0f; break;
            case ItemRarity::Unique: value *= 500.0f; break;
            default: break;
        }
        return static_cast<std::uint64_t>(value);
    }

    /// @brief Get display name including modifiers
    std::string get_display_name() const {
        if (!def) return "Unknown Item";
        std::string name;
        for (const auto& mod : modifiers) {
            if (!mod.name.empty()) {
                name += mod.name + " ";
            }
        }
        name += def->display_name;
        return name;
    }
};

// =============================================================================
// ItemStack - Stack of identical items
// =============================================================================

/// @brief Represents a stack of items in a slot
struct ItemStack {
    ItemInstanceId item;
    std::uint32_t count{0};
    ItemDefId def_id;

    bool empty() const { return count == 0 || !item; }
    bool full(std::uint32_t max_stack) const { return count >= max_stack; }
    std::uint32_t space(std::uint32_t max_stack) const {
        return count < max_stack ? max_stack - count : 0;
    }
};

// =============================================================================
// IItemRegistry Interface
// =============================================================================

/// @brief Interface for item registry
class IItemRegistry {
public:
    virtual ~IItemRegistry() = default;

    virtual ItemDefId register_item(const ItemDef& def) = 0;
    virtual bool unregister_item(ItemDefId id) = 0;
    virtual const ItemDef* get_definition(ItemDefId id) const = 0;
    virtual ItemDefId find_by_name(std::string_view name) const = 0;
    virtual std::vector<ItemDefId> find_by_category(ItemCategory category) const = 0;
    virtual std::vector<ItemDefId> find_by_tag(std::string_view tag) const = 0;
    virtual std::size_t item_count() const = 0;
};

// =============================================================================
// ItemRegistry Implementation
// =============================================================================

/// @brief Registry for item definitions
class ItemRegistry : public IItemRegistry {
public:
    ItemRegistry();
    ~ItemRegistry() override;

    /// @brief Register a new item definition
    ItemDefId register_item(const ItemDef& def) override;

    /// @brief Unregister an item definition
    bool unregister_item(ItemDefId id) override;

    /// @brief Get item definition by ID
    const ItemDef* get_definition(ItemDefId id) const override;

    /// @brief Find item by internal name
    ItemDefId find_by_name(std::string_view name) const override;

    /// @brief Find all items in a category
    std::vector<ItemDefId> find_by_category(ItemCategory category) const override;

    /// @brief Find all items with a specific tag
    std::vector<ItemDefId> find_by_tag(std::string_view tag) const override;

    /// @brief Get total number of registered items
    std::size_t item_count() const override;

    /// @brief Get all registered item IDs
    std::vector<ItemDefId> all_items() const;

    /// @brief Clear all definitions
    void clear();

    // Preset item definitions
    static ItemDef preset_health_potion();
    static ItemDef preset_mana_potion();
    static ItemDef preset_gold_coin();
    static ItemDef preset_iron_sword();
    static ItemDef preset_leather_armor();
    static ItemDef preset_iron_ore();
    static ItemDef preset_wood_plank();

private:
    std::unordered_map<ItemDefId, ItemDef> m_definitions;
    std::unordered_map<std::string, ItemDefId> m_name_lookup;
    std::uint64_t m_next_id{1};
};

// =============================================================================
// ItemFactory
// =============================================================================

/// @brief Factory for creating item instances
class ItemFactory {
public:
    ItemFactory();
    explicit ItemFactory(ItemRegistry* registry);
    ~ItemFactory();

    /// @brief Set the item registry
    void set_registry(ItemRegistry* registry) { m_registry = registry; }

    /// @brief Create a new item instance
    ItemInstance create(ItemDefId def_id, std::uint32_t quantity = 1);

    /// @brief Create from item name
    ItemInstance create(std::string_view name, std::uint32_t quantity = 1);

    /// @brief Create with specific quality
    ItemInstance create_with_quality(ItemDefId def_id, float quality, std::uint32_t quantity = 1);

    /// @brief Create with random modifiers
    ItemInstance create_with_modifiers(ItemDefId def_id, std::uint32_t modifier_count);

    /// @brief Clone an existing item
    ItemInstance clone(const ItemInstance& source);

    /// @brief Split a stack
    std::pair<ItemInstance, ItemInstance> split(ItemInstance& source, std::uint32_t amount);

    /// @brief Merge two stacks (returns leftover)
    std::uint32_t merge(ItemInstance& dest, ItemInstance& source);

    /// @brief Get next instance ID without creating item
    ItemInstanceId peek_next_id() const { return ItemInstanceId{m_next_id}; }

    // Modifier pools
    void add_modifier_pool(ItemRarity rarity, const std::vector<ItemModifier>& modifiers);
    const std::vector<ItemModifier>& get_modifier_pool(ItemRarity rarity) const;

private:
    ItemInstanceId generate_id();
    void apply_random_modifiers(ItemInstance& item, std::uint32_t count);

    ItemRegistry* m_registry{nullptr};
    std::uint64_t m_next_id{1};
    std::unordered_map<ItemRarity, std::vector<ItemModifier>> m_modifier_pools;
    static const std::vector<ItemModifier> s_empty_modifiers;
};

// =============================================================================
// ItemDatabase (for persistence)
// =============================================================================

/// @brief Manages item instance persistence
class ItemDatabase {
public:
    ItemDatabase();
    explicit ItemDatabase(ItemRegistry* registry, ItemFactory* factory);
    ~ItemDatabase();

    /// @brief Store an item instance
    void store(const ItemInstance& item);

    /// @brief Retrieve an item instance
    std::optional<ItemInstance> retrieve(ItemInstanceId id) const;

    /// @brief Check if item exists
    bool exists(ItemInstanceId id) const;

    /// @brief Remove item from database
    bool remove(ItemInstanceId id);

    /// @brief Get all stored item IDs
    std::vector<ItemInstanceId> all_items() const;

    /// @brief Find items by owner
    std::vector<ItemInstanceId> find_by_owner(EntityId owner) const;

    /// @brief Find items by definition
    std::vector<ItemInstanceId> find_by_definition(ItemDefId def) const;

    /// @brief Clear all stored items
    void clear();

    /// @brief Get total stored item count
    std::size_t count() const { return m_items.size(); }

    // Serialization
    struct SerializedItem {
        std::uint64_t instance_id;
        std::uint64_t def_id;
        std::uint32_t quantity;
        float durability;
        float quality;
        std::uint8_t rarity;
        std::uint64_t owner;
        std::uint64_t bound_to;
        bool soulbound;
        double created_time;
        std::vector<std::uint8_t> custom_data;
    };

    std::vector<SerializedItem> serialize() const;
    void deserialize(const std::vector<SerializedItem>& data);

private:
    ItemRegistry* m_registry{nullptr};
    ItemFactory* m_factory{nullptr};
    std::unordered_map<ItemInstanceId, ItemInstance> m_items;
};

} // namespace void_inventory
