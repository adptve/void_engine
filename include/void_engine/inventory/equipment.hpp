/// @file equipment.hpp
/// @brief Equipment system for void_inventory module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "items.hpp"
#include "containers.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_inventory {

// =============================================================================
// EquipmentSlotDef - Slot Definition
// =============================================================================

/// @brief Definition of an equipment slot
struct EquipmentSlotDef {
    EquipmentSlotId id;
    std::string name;
    EquipmentSlotType type{EquipmentSlotType::None};
    std::vector<ItemCategory> allowed_categories;
    std::vector<EquipmentSlotType> compatible_item_slots;
    bool required{false};               ///< Must have item equipped
    bool visible{true};                 ///< Show in UI
    std::uint32_t ui_order{0};          ///< Order in UI

    bool accepts(const ItemDef& def) const {
        // Check compatible slots
        if (!compatible_item_slots.empty()) {
            bool found = false;
            for (auto slot : compatible_item_slots) {
                if (slot == def.equip_slot) {
                    found = true;
                    break;
                }
                for (auto alt : def.alternate_slots) {
                    if (slot == alt) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) return false;
        }

        // Check allowed categories
        if (!allowed_categories.empty()) {
            bool found = false;
            for (auto cat : allowed_categories) {
                if (cat == def.category) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        return true;
    }
};

/// @brief Equipped item data
struct EquippedItem {
    EquipmentSlotId slot;
    ItemInstanceId item;
    double equipped_time{0};
    std::vector<StatModifier> applied_modifiers;
};

// =============================================================================
// EquipmentSet - Set Bonus System
// =============================================================================

/// @brief Bonus granted by equipping set pieces
struct SetBonus {
    std::uint32_t pieces_required{2};
    std::string name;
    std::string description;
    std::vector<StatModifier> stat_bonuses;
    std::function<void(EntityId)> on_activate;
    std::function<void(EntityId)> on_deactivate;
};

/// @brief Equipment set definition
struct EquipmentSetDef {
    std::string name;
    std::string description;
    std::vector<ItemDefId> items;
    std::vector<SetBonus> bonuses;

    std::uint32_t max_pieces() const { return static_cast<std::uint32_t>(items.size()); }

    bool contains_item(ItemDefId id) const {
        for (const auto& item : items) {
            if (item == id) return true;
        }
        return false;
    }

    const SetBonus* get_active_bonus(std::uint32_t equipped_count) const {
        const SetBonus* best = nullptr;
        for (const auto& bonus : bonuses) {
            if (equipped_count >= bonus.pieces_required) {
                if (!best || bonus.pieces_required > best->pieces_required) {
                    best = &bonus;
                }
            }
        }
        return best;
    }
};

// =============================================================================
// EquipmentSetRegistry
// =============================================================================

/// @brief Registry for equipment sets
class EquipmentSetRegistry {
public:
    EquipmentSetRegistry();
    ~EquipmentSetRegistry();

    /// @brief Register a new equipment set
    void register_set(const std::string& name, const EquipmentSetDef& set);

    /// @brief Get set definition
    const EquipmentSetDef* get_set(const std::string& name) const;

    /// @brief Find sets containing an item
    std::vector<const EquipmentSetDef*> find_sets_with_item(ItemDefId item) const;

    /// @brief Get all set names
    std::vector<std::string> all_sets() const;

    /// @brief Clear all sets
    void clear();

    // Preset sets
    static EquipmentSetDef preset_iron_set();
    static EquipmentSetDef preset_leather_set();

private:
    std::unordered_map<std::string, EquipmentSetDef> m_sets;
};

// =============================================================================
// EquipmentComponent
// =============================================================================

/// @brief Component for entity equipment management
class EquipmentComponent {
public:
    EquipmentComponent();
    explicit EquipmentComponent(EntityId owner);
    ~EquipmentComponent();

    // Slot management
    /// @brief Add an equipment slot
    EquipmentSlotId add_slot(const EquipmentSlotDef& def);

    /// @brief Remove a slot
    bool remove_slot(EquipmentSlotId slot);

    /// @brief Get slot definition
    const EquipmentSlotDef* get_slot_def(EquipmentSlotId slot) const;

    /// @brief Get all slot IDs
    std::vector<EquipmentSlotId> all_slots() const;

    /// @brief Get slot by type
    EquipmentSlotId get_slot_by_type(EquipmentSlotType type) const;

    // Equipment operations
    /// @brief Equip an item
    TransactionResult equip(ItemInstanceId item, EquipmentSlotId slot);

    /// @brief Equip to first valid slot
    TransactionResult equip_auto(ItemInstanceId item, EquipmentSlotId* out_slot = nullptr);

    /// @brief Unequip from slot
    TransactionResult unequip(EquipmentSlotId slot, ItemInstanceId* out_item = nullptr);

    /// @brief Unequip specific item
    TransactionResult unequip_item(ItemInstanceId item);

    /// @brief Swap equipped items between slots
    TransactionResult swap_slots(EquipmentSlotId slot_a, EquipmentSlotId slot_b);

    /// @brief Check if item can be equipped in slot
    bool can_equip(ItemInstanceId item, EquipmentSlotId slot) const;

    /// @brief Check if item meets requirements
    bool meets_requirements(const ItemInstance& item) const;

    // Queries
    /// @brief Get equipped item in slot
    ItemInstanceId get_equipped(EquipmentSlotId slot) const;

    /// @brief Find slot containing item
    std::optional<EquipmentSlotId> find_item_slot(ItemInstanceId item) const;

    /// @brief Check if slot is occupied
    bool is_slot_occupied(EquipmentSlotId slot) const;

    /// @brief Get all equipped items
    std::vector<EquippedItem> all_equipped() const;

    /// @brief Count equipped items
    std::size_t equipped_count() const;

    // Stats
    /// @brief Get all stat modifiers from equipment
    std::vector<StatModifier> get_all_modifiers() const;

    /// @brief Get total value for a stat
    float get_stat_total(StatType stat) const;

    /// @brief Calculate stats with base values
    std::unordered_map<StatType, float> calculate_stats(const std::unordered_map<StatType, float>& base_stats) const;

    // Set bonuses
    /// @brief Set the set registry
    void set_set_registry(EquipmentSetRegistry* registry) { m_set_registry = registry; }

    /// @brief Get active set bonuses
    std::vector<std::pair<const EquipmentSetDef*, const SetBonus*>> get_active_set_bonuses() const;

    /// @brief Get equipped count for a set
    std::uint32_t get_set_piece_count(const std::string& set_name) const;

    // Events
    void set_on_equip(std::function<void(const EquipmentChangeEvent&)> callback);
    void set_on_unequip(std::function<void(const EquipmentChangeEvent&)> callback);

    // Requirement checker
    using RequirementChecker = std::function<bool(StatType, float)>;
    void set_requirement_checker(RequirementChecker checker) { m_requirement_checker = std::move(checker); }

    // Item database
    void set_item_database(ItemDatabase* db) { m_item_db = db; }

    // Owner
    EntityId owner() const { return m_owner; }
    void set_owner(EntityId owner) { m_owner = owner; }

    // Preset slot configurations
    static std::vector<EquipmentSlotDef> preset_humanoid_slots();
    static std::vector<EquipmentSlotDef> preset_minimal_slots();

private:
    void update_set_bonuses();
    void apply_set_bonus(const EquipmentSetDef* set, const SetBonus* bonus);
    void remove_set_bonus(const EquipmentSetDef* set, const SetBonus* bonus);
    const ItemInstance* get_item_instance(ItemInstanceId id) const;

    EntityId m_owner;
    std::unordered_map<EquipmentSlotId, EquipmentSlotDef> m_slot_defs;
    std::unordered_map<EquipmentSlotId, EquippedItem> m_equipped;

    EquipmentSetRegistry* m_set_registry{nullptr};
    ItemDatabase* m_item_db{nullptr};
    RequirementChecker m_requirement_checker;

    std::function<void(const EquipmentChangeEvent&)> m_on_equip;
    std::function<void(const EquipmentChangeEvent&)> m_on_unequip;

    // Cache for active set bonuses
    std::unordered_map<std::string, std::uint32_t> m_set_counts;
    std::vector<std::pair<const EquipmentSetDef*, const SetBonus*>> m_active_bonuses;

    std::uint64_t m_next_slot_id{1};
};

// =============================================================================
// EquipmentLoadout - Saved Equipment Configuration
// =============================================================================

/// @brief Saved equipment loadout
struct EquipmentLoadout {
    std::string name;
    std::unordered_map<EquipmentSlotId, ItemInstanceId> items;
    double created_time{0};
    double last_used{0};
};

/// @brief Manages equipment loadouts
class LoadoutManager {
public:
    LoadoutManager();
    explicit LoadoutManager(EquipmentComponent* equipment);
    ~LoadoutManager();

    /// @brief Save current equipment as loadout
    void save_loadout(const std::string& name);

    /// @brief Apply a loadout
    bool apply_loadout(const std::string& name, IContainer* inventory = nullptr);

    /// @brief Delete a loadout
    bool delete_loadout(const std::string& name);

    /// @brief Rename a loadout
    bool rename_loadout(const std::string& old_name, const std::string& new_name);

    /// @brief Get loadout
    const EquipmentLoadout* get_loadout(const std::string& name) const;

    /// @brief Get all loadout names
    std::vector<std::string> all_loadouts() const;

    /// @brief Set equipment component
    void set_equipment(EquipmentComponent* equipment) { m_equipment = equipment; }

    // Serialization
    struct SerializedLoadout {
        std::string name;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> slot_items;
        double created_time;
    };

    std::vector<SerializedLoadout> serialize() const;
    void deserialize(const std::vector<SerializedLoadout>& data);

private:
    EquipmentComponent* m_equipment{nullptr};
    std::unordered_map<std::string, EquipmentLoadout> m_loadouts;
};

// =============================================================================
// CharacterStats - Full stat calculation
// =============================================================================

/// @brief Calculates final character stats from base + equipment
class CharacterStats {
public:
    CharacterStats();
    ~CharacterStats();

    /// @brief Set base stat value
    void set_base_stat(StatType stat, float value);

    /// @brief Get base stat value
    float get_base_stat(StatType stat) const;

    /// @brief Add a modifier
    void add_modifier(const StatModifier& mod);

    /// @brief Remove modifiers by source
    void remove_modifiers_by_source(const std::string& source);

    /// @brief Clear all modifiers
    void clear_modifiers();

    /// @brief Calculate final stat value
    float get_final_stat(StatType stat) const;

    /// @brief Recalculate all stats
    void recalculate();

    /// @brief Get all final stats
    const std::unordered_map<StatType, float>& final_stats() const { return m_final_stats; }

    /// @brief Set equipment component to pull modifiers from
    void set_equipment(EquipmentComponent* equipment) { m_equipment = equipment; }

    /// @brief Trigger recalculation from equipment change
    void on_equipment_change();

private:
    std::unordered_map<StatType, float> m_base_stats;
    std::vector<StatModifier> m_modifiers;
    std::unordered_map<StatType, float> m_final_stats;
    EquipmentComponent* m_equipment{nullptr};
    bool m_dirty{true};
};

} // namespace void_inventory
