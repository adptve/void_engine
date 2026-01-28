/// @file state_stores.hpp
/// @brief Centralized state stores for gameplay systems
///
/// These stores are OWNED by GameStateCore and persist across plugin hot-reloads.
/// Plugins read from these stores and submit commands to modify them.

#pragma once

#include "fwd.hpp"

#include <any>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_plugin_api {

// =============================================================================
// Common Types
// =============================================================================

/// @brief 3D vector for positions/directions
struct Vec3 {
    float x{0}, y{0}, z{0};

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

/// @brief Unique identifier for items
struct ItemDefId {
    std::uint64_t value{0};
    bool operator==(const ItemDefId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for item instances
struct ItemInstanceId {
    std::uint64_t value{0};
    bool operator==(const ItemInstanceId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for status effects
struct StatusEffectId {
    std::uint64_t value{0};
    bool operator==(const StatusEffectId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for projectiles
struct ProjectileId {
    std::uint64_t value{0};
    bool operator==(const ProjectileId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for behavior tree instances
struct BehaviorTreeId {
    std::uint64_t value{0};
    bool operator==(const BehaviorTreeId&) const = default;
    explicit operator bool() const { return value != 0; }
};

} // namespace void_plugin_api

// =============================================================================
// Hash specializations - MUST appear before any unordered_map usage
// =============================================================================
namespace std {

template<>
struct hash<void_plugin_api::ItemDefId> {
    std::size_t operator()(const void_plugin_api::ItemDefId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_plugin_api::ItemInstanceId> {
    std::size_t operator()(const void_plugin_api::ItemInstanceId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_plugin_api::StatusEffectId> {
    std::size_t operator()(const void_plugin_api::StatusEffectId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_plugin_api::ProjectileId> {
    std::size_t operator()(const void_plugin_api::ProjectileId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_plugin_api::BehaviorTreeId> {
    std::size_t operator()(const void_plugin_api::BehaviorTreeId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std

namespace void_plugin_api {

/// @brief Damage type enumeration
enum class DamageType : std::uint8_t {
    Physical,
    Fire,
    Ice,
    Lightning,
    Poison,
    Magic,
    Pure,  // Ignores armor
    Custom
};

// =============================================================================
// AI STATE STORE
// =============================================================================

/// @brief Blackboard data for a single entity
struct BlackboardData {
    std::unordered_map<std::string, bool> bool_values;
    std::unordered_map<std::string, int> int_values;
    std::unordered_map<std::string, float> float_values;
    std::unordered_map<std::string, std::string> string_values;
    std::unordered_map<std::string, Vec3> vec3_values;
    std::unordered_map<std::string, EntityId> entity_values;
    std::unordered_map<std::string, std::any> custom_values;

    double last_modified{0};
};

/// @brief Behavior tree execution state
struct BehaviorTreeState {
    BehaviorTreeId tree_id;
    std::string tree_name;
    std::uint32_t current_node{0};
    std::vector<std::uint32_t> active_stack;  // Stack of running composite nodes
    std::unordered_map<std::uint32_t, std::uint8_t> node_states;  // Node ID -> state
    double last_tick{0};
    bool paused{false};
};

/// @brief Navigation agent state
struct NavAgentState {
    Vec3 current_position;
    Vec3 target_position;
    std::vector<Vec3> path_points;
    std::uint32_t current_waypoint{0};
    float speed{5.0f};
    float radius{0.5f};
    float height{2.0f};
    bool has_path{false};
    bool path_pending{false};
    double path_request_time{0};
};

/// @brief Perception target data
struct PerceptionTarget {
    EntityId entity;
    Vec3 last_known_position;
    float threat_level{0};
    double first_seen{0};
    double last_seen{0};
    bool visible{false};
    bool heard{false};
};

/// @brief Perception state for an entity
struct PerceptionState {
    std::vector<PerceptionTarget> targets;
    EntityId primary_target;
    float sight_range{50.0f};
    float hearing_range{30.0f};
    float fov_degrees{120.0f};
    bool perception_enabled{true};
};

/// @brief Centralized AI state owned by GameStateCore
struct AIStateStore {
    /// Blackboard data per entity
    std::unordered_map<EntityId, BlackboardData> entity_blackboards;

    /// Behavior tree states per entity
    std::unordered_map<EntityId, BehaviorTreeState> tree_states;

    /// Navigation states per entity
    std::unordered_map<EntityId, NavAgentState> nav_states;

    /// Perception states per entity
    std::unordered_map<EntityId, PerceptionState> perception_states;

    /// Global blackboard for shared AI data
    BlackboardData global_blackboard;

    // -------------------------------------------------------------------------
    // Serialization support
    // -------------------------------------------------------------------------

    /// @brief Serialize to binary for save/load
    std::vector<std::uint8_t> serialize() const;

    /// @brief Deserialize from binary
    static AIStateStore deserialize(const std::vector<std::uint8_t>& data);

    /// @brief Clear all state (for level transitions)
    void clear();

    /// @brief Get count of active AI entities
    std::size_t active_count() const {
        return entity_blackboards.size();
    }
};

// =============================================================================
// COMBAT STATE STORE
// =============================================================================

/// @brief Health/vitals state for an entity
struct VitalsState {
    float current_health{100.0f};
    float max_health{100.0f};
    float current_shield{0};
    float max_shield{0};
    float armor{0};
    float health_regen{0};
    float shield_regen{0};
    bool alive{true};
    bool invulnerable{false};
    double last_damage_time{0};
    double death_time{0};
};

/// @brief Active status effect on an entity
struct ActiveEffect {
    StatusEffectId effect_id;
    std::string effect_name;
    EntityId source;
    float duration{0};
    float remaining{0};
    float tick_interval{1.0f};
    float next_tick{0};
    std::uint32_t stacks{1};
    std::uint32_t max_stacks{1};
    std::unordered_map<std::string, float> modifiers;  // Stat -> modifier value
    bool permanent{false};
    bool dispellable{true};
};

/// @brief Combat statistics for an entity
struct CombatStats {
    float base_damage{10.0f};
    float attack_speed{1.0f};
    float crit_chance{0.05f};
    float crit_multiplier{2.0f};
    float armor_penetration{0};
    std::unordered_map<DamageType, float> damage_bonuses;
    std::unordered_map<DamageType, float> resistances;
};

/// @brief Active projectile state
struct ProjectileState {
    ProjectileId id;
    EntityId source;
    EntityId target;  // Optional homing target
    Vec3 position;
    Vec3 velocity;
    Vec3 direction;
    float speed{20.0f};
    float damage{10.0f};
    DamageType damage_type{DamageType::Physical};
    float lifetime{10.0f};
    float elapsed{0};
    float radius{0.1f};
    bool homing{false};
    bool penetrating{false};
    std::uint32_t hits_remaining{1};
};

/// @brief Damage history entry
struct DamageHistoryEntry {
    EntityId source;
    float amount{0};
    DamageType type{DamageType::Physical};
    double timestamp{0};
    bool was_crit{false};
    bool was_blocked{false};
};

/// @brief Damage history for an entity
struct DamageHistory {
    std::vector<DamageHistoryEntry> entries;
    float total_damage_taken{0};
    float total_damage_dealt{0};
    std::uint32_t kills{0};
    std::uint32_t deaths{0};
    static constexpr std::size_t MAX_HISTORY = 100;

    void add_entry(const DamageHistoryEntry& entry) {
        entries.push_back(entry);
        if (entries.size() > MAX_HISTORY) {
            entries.erase(entries.begin());
        }
    }
};

/// @brief Centralized combat state owned by GameStateCore
struct CombatStateStore {
    /// Vitals per entity
    std::unordered_map<EntityId, VitalsState> entity_vitals;

    /// Active status effects per entity
    std::unordered_map<EntityId, std::vector<ActiveEffect>> status_effects;

    /// Combat stats per entity
    std::unordered_map<EntityId, CombatStats> combat_stats;

    /// Active projectiles in the world
    std::vector<ProjectileState> active_projectiles;

    /// Damage history per entity
    std::unordered_map<EntityId, DamageHistory> damage_history;

    /// Global combat modifiers (difficulty scaling, etc.)
    float global_damage_multiplier{1.0f};
    float global_health_multiplier{1.0f};

    // -------------------------------------------------------------------------
    // ID generation
    // -------------------------------------------------------------------------

    ProjectileId next_projectile_id() {
        return ProjectileId{++m_next_projectile_id};
    }

    StatusEffectId next_effect_id() {
        return StatusEffectId{++m_next_effect_id};
    }

    // -------------------------------------------------------------------------
    // Serialization support
    // -------------------------------------------------------------------------

    std::vector<std::uint8_t> serialize() const;
    static CombatStateStore deserialize(const std::vector<std::uint8_t>& data);
    void clear();

    std::size_t active_count() const {
        return entity_vitals.size();
    }

private:
    std::uint64_t m_next_projectile_id{0};
    std::uint64_t m_next_effect_id{0};
};

// =============================================================================
// INVENTORY STATE STORE
// =============================================================================

/// @brief Item instance data
struct ItemInstanceData {
    ItemInstanceId id;
    ItemDefId def_id;
    std::uint32_t quantity{1};
    std::uint32_t max_stack{99};
    float durability{1.0f};
    float quality{1.0f};
    std::vector<std::string> modifiers;  // Applied modifiers
    std::unordered_map<std::string, float> stats;  // Stat bonuses
    double acquired_time{0};
    bool bound{false};  // Soulbound
};

/// @brief Container slot
struct ContainerSlot {
    std::uint32_t index{0};
    ItemInstanceId item;
    std::uint32_t quantity{0};
    bool locked{false};
};

/// @brief Inventory container data
struct InventoryData {
    std::vector<ContainerSlot> slots;
    std::uint32_t capacity{20};
    float max_weight{100.0f};
    float current_weight{0};
    std::uint64_t currency{0};
};

/// @brief Equipment slot data
struct EquipmentSlotData {
    std::string slot_name;
    ItemInstanceId equipped_item;
    bool locked{false};
};

/// @brief Equipment data for an entity
struct EquipmentData {
    std::unordered_map<std::string, EquipmentSlotData> slots;
    std::vector<std::pair<std::string, float>> total_stats;  // Aggregated stats
};

/// @brief Crafting queue entry
struct CraftingQueueEntry {
    std::uint64_t recipe_id{0};
    float progress{0};
    float total_time{0};
    bool paused{false};
};

/// @brief Crafting queue for an entity
struct CraftingQueueData {
    std::vector<CraftingQueueEntry> queue;
    std::uint32_t max_queue_size{3};
    float craft_speed_multiplier{1.0f};
};

/// @brief World item (dropped item in world)
struct WorldItemData {
    ItemInstanceId item;
    ItemDefId def_id;
    std::uint32_t quantity{1};
    Vec3 position;
    double spawn_time{0};
    double despawn_time{0};  // 0 = never
    EntityId owner;  // Optional owner for loot rights
    bool physics_enabled{true};
};

/// @brief Shop state
struct ShopState {
    std::string shop_id;
    std::string name;
    std::vector<ItemDefId> inventory;
    std::unordered_map<ItemDefId, std::uint32_t> stock;  // Item -> count (0 = unlimited)
    std::unordered_map<ItemDefId, float> price_multipliers;
    float buy_multiplier{1.0f};   // Price when buying from shop
    float sell_multiplier{0.5f};  // Price when selling to shop
    double last_restock{0};
    float restock_interval{3600.0f};  // 1 hour
};

/// @brief Centralized inventory state owned by GameStateCore
struct InventoryStateStore {
    /// Inventory per entity
    std::unordered_map<EntityId, InventoryData> entity_inventories;

    /// Equipment per entity
    std::unordered_map<EntityId, EquipmentData> equipment;

    /// Crafting queues per entity
    std::unordered_map<EntityId, CraftingQueueData> crafting_queues;

    /// Items dropped in world
    std::vector<WorldItemData> world_items;

    /// Shop states
    std::unordered_map<std::string, ShopState> shops;

    /// All item instances (master registry)
    std::unordered_map<ItemInstanceId, ItemInstanceData> item_instances;

    // -------------------------------------------------------------------------
    // ID generation
    // -------------------------------------------------------------------------

    ItemInstanceId next_item_instance_id() {
        return ItemInstanceId{++m_next_instance_id};
    }

    // -------------------------------------------------------------------------
    // Serialization support
    // -------------------------------------------------------------------------

    std::vector<std::uint8_t> serialize() const;
    static InventoryStateStore deserialize(const std::vector<std::uint8_t>& data);
    void clear();

    std::size_t total_items() const {
        return item_instances.size();
    }

private:
    std::uint64_t m_next_instance_id{0};
};

} // namespace void_plugin_api
