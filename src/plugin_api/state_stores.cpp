/// @file state_stores.cpp
/// @brief Implementation of state store serialization

#include <void_engine/plugin_api/state_stores.hpp>

#include <cstring>

namespace void_plugin_api {

// =============================================================================
// AIStateStore Implementation
// =============================================================================

std::vector<std::uint8_t> AIStateStore::serialize() const {
    // Simple binary serialization
    std::vector<std::uint8_t> data;

    // Serialize entity count
    auto entity_count = static_cast<std::uint32_t>(entity_blackboards.size());
    data.resize(sizeof(entity_count));
    std::memcpy(data.data(), &entity_count, sizeof(entity_count));

    // In production, would serialize all blackboard data, tree states, etc.
    // For now, just the count for validation

    return data;
}

AIStateStore AIStateStore::deserialize(const std::vector<std::uint8_t>& data) {
    AIStateStore store;

    if (data.size() >= sizeof(std::uint32_t)) {
        std::uint32_t entity_count;
        std::memcpy(&entity_count, data.data(), sizeof(entity_count));
        // Would deserialize all data in production
    }

    return store;
}

void AIStateStore::clear() {
    entity_blackboards.clear();
    tree_states.clear();
    nav_states.clear();
    perception_states.clear();
    global_blackboard = BlackboardData{};
}

// =============================================================================
// CombatStateStore Implementation
// =============================================================================

std::vector<std::uint8_t> CombatStateStore::serialize() const {
    std::vector<std::uint8_t> data;

    // Serialize entity count
    auto entity_count = static_cast<std::uint32_t>(entity_vitals.size());
    auto projectile_count = static_cast<std::uint32_t>(active_projectiles.size());

    data.resize(sizeof(entity_count) + sizeof(projectile_count));
    std::memcpy(data.data(), &entity_count, sizeof(entity_count));
    std::memcpy(data.data() + sizeof(entity_count), &projectile_count, sizeof(projectile_count));

    return data;
}

CombatStateStore CombatStateStore::deserialize(const std::vector<std::uint8_t>& data) {
    CombatStateStore store;

    if (data.size() >= sizeof(std::uint32_t) * 2) {
        // Would deserialize all data in production
    }

    return store;
}

void CombatStateStore::clear() {
    entity_vitals.clear();
    status_effects.clear();
    combat_stats.clear();
    active_projectiles.clear();
    damage_history.clear();
    global_damage_multiplier = 1.0f;
    global_health_multiplier = 1.0f;
}

// =============================================================================
// InventoryStateStore Implementation
// =============================================================================

std::vector<std::uint8_t> InventoryStateStore::serialize() const {
    std::vector<std::uint8_t> data;

    auto entity_count = static_cast<std::uint32_t>(entity_inventories.size());
    auto item_count = static_cast<std::uint32_t>(item_instances.size());
    auto world_item_count = static_cast<std::uint32_t>(world_items.size());

    data.resize(sizeof(entity_count) * 3);
    std::memcpy(data.data(), &entity_count, sizeof(entity_count));
    std::memcpy(data.data() + sizeof(entity_count), &item_count, sizeof(item_count));
    std::memcpy(data.data() + sizeof(entity_count) * 2, &world_item_count, sizeof(world_item_count));

    return data;
}

InventoryStateStore InventoryStateStore::deserialize(const std::vector<std::uint8_t>& data) {
    InventoryStateStore store;

    if (data.size() >= sizeof(std::uint32_t) * 3) {
        // Would deserialize all data in production
    }

    return store;
}

void InventoryStateStore::clear() {
    entity_inventories.clear();
    equipment.clear();
    crafting_queues.clear();
    world_items.clear();
    shops.clear();
    item_instances.clear();
}

} // namespace void_plugin_api
