/// @file example_ai_plugin.cpp
/// @brief Implementation of the example AI plugin

#include "example_ai_plugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace example_plugins {

// =============================================================================
// Construction
// =============================================================================

ExampleAIPlugin::ExampleAIPlugin() = default;

// =============================================================================
// Lifecycle
// =============================================================================

void ExampleAIPlugin::on_plugin_load(void_plugin_api::IPluginAPI* api) {
    // Store API reference (done by base class, but we can do additional setup)

    // Scan for entities that need AI management
    // In a real game, you'd have a way to tag entities as AI-controlled
    m_managed_entities.clear();

    // For demonstration, we'll manage any entity that has AI state
    const auto& ai_state = api->ai_state();
    for (const auto& [entity_id, blackboard] : ai_state.entity_blackboards) {
        // Check if this entity should be AI-controlled
        auto is_player_it = blackboard.bool_values.find("is_player");
        if (is_player_it == blackboard.bool_values.end() || !is_player_it->second) {
            // Not a player, so AI controls this entity
            m_managed_entities.push_back(entity_id);

            // Initialize decision timer
            m_decision_timers[entity_id] = 0;
            m_current_actions[entity_id] = "idle";
        }
    }
}

void ExampleAIPlugin::on_tick(float dt) {
    m_total_runtime += dt;

    // Process AI for each managed entity
    for (auto entity : m_managed_entities) {
        process_entity_ai(entity, dt);
    }
}

void ExampleAIPlugin::on_fixed_tick(float dt) {
    // Physics-rate updates (e.g., pathfinding, collision queries)
    // For this example, we don't need fixed-rate updates
    (void)dt;
}

void ExampleAIPlugin::on_plugin_unload() {
    // Cleanup - the runtime state will be serialized by serialize_runtime_state()
    // before this is called if hot-reloading
}

// =============================================================================
// Hot-Reload State Preservation
// =============================================================================

std::vector<std::uint8_t> ExampleAIPlugin::serialize_runtime_state() const {
    std::vector<std::uint8_t> data;

    // Serialize managed entities count
    std::uint32_t entity_count = static_cast<std::uint32_t>(m_managed_entities.size());
    data.resize(sizeof(entity_count));
    std::memcpy(data.data(), &entity_count, sizeof(entity_count));

    // Serialize each entity ID
    for (auto entity : m_managed_entities) {
        std::size_t offset = data.size();
        data.resize(offset + sizeof(entity.value));
        std::memcpy(data.data() + offset, &entity.value, sizeof(entity.value));
    }

    // Serialize runtime stats
    std::size_t offset = data.size();
    data.resize(offset + sizeof(m_total_runtime) + sizeof(m_decisions_made));
    std::memcpy(data.data() + offset, &m_total_runtime, sizeof(m_total_runtime));
    offset += sizeof(m_total_runtime);
    std::memcpy(data.data() + offset, &m_decisions_made, sizeof(m_decisions_made));

    return data;
}

void ExampleAIPlugin::deserialize_runtime_state(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t)) {
        return;
    }

    std::size_t offset = 0;

    // Deserialize entity count
    std::uint32_t entity_count;
    std::memcpy(&entity_count, data.data() + offset, sizeof(entity_count));
    offset += sizeof(entity_count);

    // Deserialize entity IDs
    m_managed_entities.clear();
    m_managed_entities.reserve(entity_count);
    for (std::uint32_t i = 0; i < entity_count && offset + sizeof(std::uint64_t) <= data.size(); ++i) {
        std::uint64_t value;
        std::memcpy(&value, data.data() + offset, sizeof(value));
        offset += sizeof(value);
        m_managed_entities.push_back(void_plugin_api::EntityId{value});
        m_decision_timers[void_plugin_api::EntityId{value}] = 0;
        m_current_actions[void_plugin_api::EntityId{value}] = "idle";
    }

    // Deserialize runtime stats
    if (offset + sizeof(m_total_runtime) + sizeof(m_decisions_made) <= data.size()) {
        std::memcpy(&m_total_runtime, data.data() + offset, sizeof(m_total_runtime));
        offset += sizeof(m_total_runtime);
        std::memcpy(&m_decisions_made, data.data() + offset, sizeof(m_decisions_made));
    }
}

// =============================================================================
// AI Logic
// =============================================================================

void ExampleAIPlugin::process_entity_ai(void_plugin_api::EntityId entity, float dt) {
    // Update decision timer
    m_decision_timers[entity] += dt;

    // Check if it's time to make a new decision
    if (m_decision_timers[entity] >= m_decision_interval) {
        m_decision_timers[entity] = 0;

        // Update what this entity can perceive
        update_perception(entity);

        // Evaluate behavior tree / state machine
        evaluate_behavior(entity);

        ++m_decisions_made;
    }

    // Execute current action
    execute_actions(entity, dt);
}

void ExampleAIPlugin::update_perception(void_plugin_api::EntityId entity) {
    auto* plugin_api = api();
    if (!plugin_api) return;

    // Get entity position
    auto position = plugin_api->get_entity_position(entity);

    // Find nearby entities
    auto nearby = plugin_api->get_entities_in_radius(position, m_perception_range);

    // Update perception targets via command
    for (auto other : nearby) {
        if (other.value != entity.value) {
            // Check if this is an enemy (simplified - just check if not same entity)
            const auto& combat_state = plugin_api->combat_state();
            auto vitals_it = combat_state.entity_vitals.find(other);
            if (vitals_it != combat_state.entity_vitals.end() && vitals_it->second.alive) {
                // Found a potential target
                plugin_api->set_perception_target(entity, other);
                break;  // For now, just track one target
            }
        }
    }
}

void ExampleAIPlugin::evaluate_behavior(void_plugin_api::EntityId entity) {
    auto* plugin_api = api();
    if (!plugin_api) return;

    const auto& ai_state = plugin_api->ai_state();
    const auto& combat_state = plugin_api->combat_state();

    // Get our vitals
    auto our_vitals = combat_state.entity_vitals.find(entity);
    if (our_vitals == combat_state.entity_vitals.end() || !our_vitals->second.alive) {
        m_current_actions[entity] = "dead";
        return;
    }

    // Check if we have a perception target
    auto perception_it = ai_state.perception_states.find(entity);
    if (perception_it != ai_state.perception_states.end() &&
        perception_it->second.primary_target.value != 0) {

        auto target = perception_it->second.primary_target;

        // Check target vitals
        auto target_vitals = combat_state.entity_vitals.find(target);
        if (target_vitals != combat_state.entity_vitals.end() && target_vitals->second.alive) {
            // Target is alive - decide whether to attack or flee
            float our_health_pct = our_vitals->second.current_health / our_vitals->second.max_health;

            if (our_health_pct < 0.2f) {
                // Low health - flee!
                m_current_actions[entity] = "flee";
                plugin_api->set_blackboard_string(entity, "behavior", "fleeing");
            } else {
                // Attack!
                m_current_actions[entity] = "attack";
                plugin_api->set_blackboard_string(entity, "behavior", "attacking");
            }
            return;
        }
    }

    // No target or target is dead - patrol or idle
    m_current_actions[entity] = "patrol";
    plugin_api->set_blackboard_string(entity, "behavior", "patrolling");
}

void ExampleAIPlugin::execute_actions(void_plugin_api::EntityId entity, float dt) {
    auto* plugin_api = api();
    if (!plugin_api) return;

    const std::string& action = m_current_actions[entity];

    if (action == "attack") {
        // Get our target
        const auto& ai_state = plugin_api->ai_state();
        auto perception_it = ai_state.perception_states.find(entity);
        if (perception_it != ai_state.perception_states.end()) {
            auto target = perception_it->second.primary_target;
            if (target.value != 0) {
                // Check distance to target
                auto our_pos = plugin_api->get_entity_position(entity);
                auto target_pos = plugin_api->get_entity_position(target);

                float dx = target_pos.x - our_pos.x;
                float dy = target_pos.y - our_pos.y;
                float dz = target_pos.z - our_pos.z;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

                if (dist <= m_attack_range) {
                    // In range - attack!
                    // Get our combat stats
                    const auto& combat_state = plugin_api->combat_state();
                    auto stats_it = combat_state.combat_stats.find(entity);
                    float damage = 10.0f;  // Default
                    if (stats_it != combat_state.combat_stats.end()) {
                        damage = stats_it->second.base_damage;
                    }

                    // Apply damage (scaled by dt to prevent damage spam)
                    // In a real game, you'd have attack cooldowns
                    plugin_api->apply_damage(target, damage * dt, entity,
                                     void_plugin_api::DamageType::Physical);
                } else {
                    // Move towards target
                    plugin_api->request_path(entity, target_pos);
                }
            }
        }
    } else if (action == "flee") {
        // Run away from target
        const auto& ai_state = plugin_api->ai_state();
        auto perception_it = ai_state.perception_states.find(entity);
        if (perception_it != ai_state.perception_states.end()) {
            auto target = perception_it->second.primary_target;
            if (target.value != 0) {
                auto our_pos = plugin_api->get_entity_position(entity);
                auto target_pos = plugin_api->get_entity_position(target);

                // Calculate flee direction (opposite of target)
                float dx = our_pos.x - target_pos.x;
                float dy = our_pos.y - target_pos.y;
                float dz = our_pos.z - target_pos.z;
                float len = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (len > 0.001f) {
                    dx /= len;
                    dy /= len;
                    dz /= len;
                }

                // Move away
                void_plugin_api::Vec3 flee_target{
                    our_pos.x + dx * 20.0f,
                    our_pos.y + dy * 20.0f,
                    our_pos.z + dz * 20.0f
                };
                plugin_api->request_path(entity, flee_target);
            }
        }
    } else if (action == "patrol") {
        // Simple patrol logic - in a real game you'd have patrol points
        (void)dt;
    }
    // "idle" and "dead" do nothing
}

// =============================================================================
// Plugin Factory
// =============================================================================

extern "C" {

void_plugin_api::GameplayPlugin* create_plugin() {
    return new ExampleAIPlugin();
}

void destroy_plugin(void_plugin_api::GameplayPlugin* plugin) {
    delete plugin;
}

}

} // namespace example_plugins
