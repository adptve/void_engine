/// @file types.hpp
/// @brief Core types and enumerations for void_inventory module

#pragma once

#include "fwd.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_inventory {

// =============================================================================
// Item Enumerations
// =============================================================================

/// @brief Category of item
enum class ItemCategory : std::uint8_t {
    None = 0,
    Consumable,     ///< Can be used/consumed
    Equipment,      ///< Can be equipped
    Material,       ///< Crafting material
    Quest,          ///< Quest item
    Currency,       ///< Currency/money
    Key,            ///< Key item
    Weapon,         ///< Weapon equipment
    Armor,          ///< Armor equipment
    Accessory,      ///< Accessory equipment
    Tool,           ///< Usable tool
    Ammunition,     ///< Ammo for weapons
    Container,      ///< Contains other items
    Misc            ///< Miscellaneous
};

/// @brief Rarity/quality tier of item
enum class ItemRarity : std::uint8_t {
    Common = 0,
    Uncommon,
    Rare,
    Epic,
    Legendary,
    Mythic,
    Unique      ///< One-of-a-kind
};

/// @brief Flags for item behavior
enum class ItemFlags : std::uint32_t {
    None            = 0,
    Stackable       = 1 << 0,   ///< Can stack multiple
    Unique          = 1 << 1,   ///< Only one can exist
    Soulbound       = 1 << 2,   ///< Cannot be traded
    QuestItem       = 1 << 3,   ///< Related to quest
    Consumable      = 1 << 4,   ///< Can be consumed
    Equippable      = 1 << 5,   ///< Can be equipped
    Tradeable       = 1 << 6,   ///< Can be traded
    Droppable       = 1 << 7,   ///< Can be dropped
    Sellable        = 1 << 8,   ///< Can be sold
    Destroyable     = 1 << 9,   ///< Can be destroyed
    Craftable       = 1 << 10,  ///< Can be crafted
    Upgradeable     = 1 << 11,  ///< Can be upgraded
    Enchantable     = 1 << 12,  ///< Can be enchanted
    Hidden          = 1 << 13,  ///< Hidden from normal view
    NoStorage       = 1 << 14,  ///< Cannot be stored
    AutoPickup      = 1 << 15,  ///< Auto pickup when near
    CooldownShared  = 1 << 16,  ///< Shares cooldown with similar items
};

inline ItemFlags operator|(ItemFlags a, ItemFlags b) {
    return static_cast<ItemFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline ItemFlags operator&(ItemFlags a, ItemFlags b) {
    return static_cast<ItemFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_flag(ItemFlags flags, ItemFlags flag) {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

// =============================================================================
// Equipment Enumerations
// =============================================================================

/// @brief Standard equipment slot types
enum class EquipmentSlotType : std::uint8_t {
    None = 0,
    Head,
    Chest,
    Legs,
    Feet,
    Hands,
    MainHand,
    OffHand,
    TwoHand,
    Ring1,
    Ring2,
    Amulet,
    Belt,
    Cape,
    Shoulders,
    Bracers,
    Trinket1,
    Trinket2,
    Ranged,
    Ammo,
    Custom
};

/// @brief Stat types that can be modified
enum class StatType : std::uint8_t {
    None = 0,
    // Primary stats
    Strength,
    Dexterity,
    Intelligence,
    Vitality,
    Wisdom,
    Charisma,
    Luck,
    // Secondary stats
    MaxHealth,
    MaxMana,
    MaxStamina,
    HealthRegen,
    ManaRegen,
    StaminaRegen,
    // Combat stats
    Attack,
    Defense,
    MagicAttack,
    MagicDefense,
    CritChance,
    CritDamage,
    AttackSpeed,
    CastSpeed,
    // Resistances
    FireResist,
    IceResist,
    LightningResist,
    PoisonResist,
    PhysicalResist,
    MagicResist,
    // Movement
    MoveSpeed,
    JumpHeight,
    // Misc
    Experience,
    GoldFind,
    ItemFind,
    Custom
};

/// @brief How stat modifier is applied
enum class ModifierType : std::uint8_t {
    Flat,           ///< Add flat value
    Percent,        ///< Add percentage of base
    Multiplier      ///< Multiply final value
};

// =============================================================================
// Container Enumerations
// =============================================================================

/// @brief Container type
enum class ContainerType : std::uint8_t {
    Basic,          ///< Simple slot-based
    Grid,           ///< 2D grid-based (like Diablo)
    Weighted,       ///< Weight-limited
    Sorted,         ///< Auto-sorting
    Stash,          ///< Permanent storage
    Hotbar,         ///< Quick access bar
    Equipment,      ///< Equipment slots
    Loot,           ///< Loot container
    Shop,           ///< Shop inventory
    Mail,           ///< Mail/delivery
    Custom
};

/// @brief Slot flags
enum class SlotFlags : std::uint8_t {
    None        = 0,
    Locked      = 1 << 0,   ///< Cannot be modified
    Reserved    = 1 << 1,   ///< Reserved for specific item
    Filtered    = 1 << 2,   ///< Has filter applied
    Hidden      = 1 << 3,   ///< Hidden from view
    Empty       = 1 << 4,   ///< Guaranteed empty
};

inline SlotFlags operator|(SlotFlags a, SlotFlags b) {
    return static_cast<SlotFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline bool has_flag(SlotFlags flags, SlotFlags flag) {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
}

// =============================================================================
// Crafting Enumerations
// =============================================================================

/// @brief Crafting station type
enum class StationType : std::uint8_t {
    None = 0,
    Basic,          ///< Basic workbench
    Forge,          ///< Metalworking
    Anvil,          ///< Smithing
    Alchemy,        ///< Potion making
    Enchanting,     ///< Enchanting
    Cooking,        ///< Food preparation
    Sewing,         ///< Cloth/leather work
    Woodworking,    ///< Wood crafting
    Jeweling,       ///< Jewelry making
    Inscription,    ///< Writing/scrolls
    Custom
};

/// @brief Recipe difficulty
enum class RecipeDifficulty : std::uint8_t {
    Trivial,
    Easy,
    Normal,
    Hard,
    Expert,
    Master
};

/// @brief Crafting result
enum class CraftingResult : std::uint8_t {
    Success,
    CriticalSuccess,    ///< Bonus output
    Failure,
    CriticalFailure,    ///< Lost materials
    Cancelled,
    InsufficientMaterials,
    InsufficientSkill,
    InvalidStation,
    InvalidRecipe
};

// =============================================================================
// Item Structures
// =============================================================================

/// @brief Property value that can be on items
struct ItemProperty {
    std::string name;
    std::any value;

    template<typename T>
    T get(const T& default_value = T{}) const {
        if (value.has_value()) {
            try {
                return std::any_cast<T>(value);
            } catch (...) {}
        }
        return default_value;
    }

    template<typename T>
    void set(const T& v) {
        value = v;
    }
};

/// @brief Modifier applied to an item
struct ItemModifier {
    std::string name;
    std::string description;
    std::vector<std::pair<StatType, float>> stat_bonuses;
    ItemRarity rarity_boost{ItemRarity::Common};
    float value_multiplier{1.0f};
    std::uint32_t color{0xFFFFFFFF};
};

/// @brief Stat modifier from equipment
struct StatModifier {
    StatType stat{StatType::None};
    ModifierType type{ModifierType::Flat};
    float value{0};
    std::string source;
};

/// @brief Requirement to use/equip item
struct ItemRequirement {
    StatType stat{StatType::None};
    float min_value{0};
    std::string custom_check;
};

// =============================================================================
// Container Structures
// =============================================================================

/// @brief Configuration for a container slot
struct SlotConfig {
    std::uint32_t index{0};
    SlotFlags flags{SlotFlags::None};
    ItemCategory allowed_category{ItemCategory::None};  ///< None = any
    std::vector<ItemDefId> allowed_items;               ///< Empty = any
    std::uint32_t max_stack_override{0};                ///< 0 = use item default
};

/// @brief Current state of a slot
struct SlotState {
    std::uint32_t index{0};
    ItemInstanceId item;
    std::uint32_t quantity{0};
    SlotFlags flags{SlotFlags::None};
    bool empty() const { return !item; }
};

/// @brief Grid position for grid containers
struct GridPosition {
    std::uint32_t x{0};
    std::uint32_t y{0};
    bool operator==(const GridPosition&) const = default;
};

/// @brief Size in grid cells
struct GridSize {
    std::uint32_t width{1};
    std::uint32_t height{1};
};

// =============================================================================
// Crafting Structures
// =============================================================================

/// @brief Ingredient required for recipe
struct RecipeIngredient {
    ItemDefId item;
    std::uint32_t quantity{1};
    bool consumed{true};        ///< False for catalysts
    float quality_min{0};       ///< Minimum quality
};

/// @brief Output from recipe
struct RecipeOutput {
    ItemDefId item;
    std::uint32_t quantity{1};
    float base_quality{1.0f};
    float quality_variance{0};
    std::vector<ItemModifier> possible_modifiers;
    std::vector<float> modifier_chances;
};

/// @brief Progress of active crafting
struct CraftingProgress {
    RecipeId recipe;
    float progress{0};          ///< 0-1
    float total_time{0};
    float elapsed_time{0};
    EntityId crafter;
    CraftingStationId station;
    bool paused{false};
    std::vector<ItemInstanceId> consumed_items;
};

// =============================================================================
// Transaction Structures
// =============================================================================

/// @brief Type of inventory operation
enum class TransactionType : std::uint8_t {
    Add,
    Remove,
    Move,
    Split,
    Merge,
    Equip,
    Unequip,
    Use,
    Drop,
    Destroy,
    Trade,
    Craft
};

/// @brief Result of inventory operation
enum class TransactionResult : std::uint8_t {
    Success,
    Failed,
    PartialSuccess,     ///< Some items affected
    InvalidItem,
    InvalidSlot,
    InvalidQuantity,
    ContainerFull,
    ItemNotStackable,
    ItemNotEquippable,
    RequirementsNotMet,
    ItemLocked,
    PermissionDenied,
    Cancelled
};

/// @brief Record of inventory transaction
struct InventoryTransaction {
    TransactionType type{TransactionType::Add};
    TransactionResult result{TransactionResult::Success};
    ItemInstanceId item;
    ItemDefId item_def;
    ContainerId source_container;
    ContainerId dest_container;
    std::uint32_t source_slot{0};
    std::uint32_t dest_slot{0};
    std::uint32_t quantity{0};
    std::uint32_t actual_quantity{0};   ///< Actual amount affected
    double timestamp{0};
    std::string error_message;
};

// =============================================================================
// Event Structures
// =============================================================================

/// @brief Item pickup event
struct ItemPickupEvent {
    EntityId entity;
    ItemInstanceId item;
    ItemDefId def;
    std::uint32_t quantity{1};
    ContainerId container;
    std::uint32_t slot{0};
};

/// @brief Item drop event
struct ItemDropEvent {
    EntityId entity;
    ItemInstanceId item;
    ItemDefId def;
    std::uint32_t quantity{1};
    float x{0}, y{0}, z{0};
};

/// @brief Item use event
struct ItemUseEvent {
    EntityId entity;
    ItemInstanceId item;
    ItemDefId def;
    EntityId target;
    bool consumed{false};
};

/// @brief Equipment change event
struct EquipmentChangeEvent {
    EntityId entity;
    EquipmentSlotId slot;
    ItemInstanceId old_item;
    ItemInstanceId new_item;
    std::vector<StatModifier> old_modifiers;
    std::vector<StatModifier> new_modifiers;
};

/// @brief Crafting complete event
struct CraftingCompleteEvent {
    EntityId crafter;
    RecipeId recipe;
    CraftingResult result;
    std::vector<ItemInstanceId> outputs;
    float quality{1.0f};
};

// =============================================================================
// Configuration Structures
// =============================================================================

/// @brief Configuration for inventory system
struct InventoryConfig {
    // Stacking
    std::uint32_t default_stack_size{99};
    std::uint32_t currency_stack_size{9999999};
    bool auto_stack{true};

    // Containers
    std::uint32_t default_container_size{20};
    float default_weight_limit{100.0f};
    bool allow_container_nesting{false};

    // Equipment
    bool require_equip_requirements{true};
    bool allow_two_hand_plus_offhand{false};

    // Crafting
    float base_craft_speed{1.0f};
    float crit_craft_chance{0.05f};
    float fail_material_loss{0.5f};

    // Misc
    bool track_item_history{true};
    double transaction_log_retention{3600.0 * 24};  ///< 24 hours
};

/// @brief Rarity color configuration
struct RarityColors {
    std::uint32_t common{0xFFFFFFFF};
    std::uint32_t uncommon{0xFF00FF00};
    std::uint32_t rare{0xFF0000FF};
    std::uint32_t epic{0xFFFF00FF};
    std::uint32_t legendary{0xFFFF8800};
    std::uint32_t mythic{0xFFFF0000};
    std::uint32_t unique{0xFF00FFFF};

    std::uint32_t get(ItemRarity rarity) const {
        switch (rarity) {
            case ItemRarity::Common: return common;
            case ItemRarity::Uncommon: return uncommon;
            case ItemRarity::Rare: return rare;
            case ItemRarity::Epic: return epic;
            case ItemRarity::Legendary: return legendary;
            case ItemRarity::Mythic: return mythic;
            case ItemRarity::Unique: return unique;
            default: return common;
        }
    }
};

// =============================================================================
// Callback Types
// =============================================================================

using ItemUseCallback = std::function<bool(EntityId user, ItemInstance& item)>;
using ItemPickupCallback = std::function<void(const ItemPickupEvent&)>;
using ItemDropCallback = std::function<void(const ItemDropEvent&)>;
using EquipmentChangeCallback = std::function<void(const EquipmentChangeEvent&)>;
using CraftingCompleteCallback = std::function<void(const CraftingCompleteEvent&)>;
using ContainerChangeCallback = std::function<void(ContainerId, std::uint32_t slot)>;
using ItemFilterCallback = std::function<bool(const ItemInstance&)>;
using SlotFilterCallback = std::function<bool(const ItemInstance&, std::uint32_t slot)>;

} // namespace void_inventory
