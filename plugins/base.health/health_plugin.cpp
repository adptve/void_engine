/// @file health_plugin.cpp
/// @brief Implementation of base.health plugin

#define VOID_PLUGIN_EXPORT

#include "health_plugin.hpp"

#include <void_engine/plugin_api/plugin.hpp>
#include <void_engine/plugin_api/context.hpp>
#include <void_engine/ecs/world.hpp>
#include <void_engine/kernel/types.hpp>
#include <void_engine/package/component_schema.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cstring>

namespace base_health {

// =============================================================================
// JSON Appliers
// =============================================================================

/// Apply Health component from JSON
static void_core::Result<void> apply_health(
    void_ecs::World& world,
    void_ecs::Entity entity,
    const nlohmann::json& data)
{
    Health health;
    health.current = data.value("current", 100.0f);
    health.max = data.value("max", 100.0f);
    health.regen_rate = data.value("regen_rate", 0.0f);
    health.regen_delay = data.value("regen_delay", 0.0f);
    health.time_since_damage = data.value("time_since_damage", 0.0f);

    // Validate
    if (health.max <= 0.0f) {
        return void_core::Error("Health.max must be positive");
    }
    if (health.current < 0.0f) {
        health.current = 0.0f;
    }
    if (health.current > health.max) {
        health.current = health.max;
    }

    world.add_component(entity, health);
    return void_core::Ok();
}

/// Apply DamageReceiver component from JSON
static void_core::Result<void> apply_damage_receiver(
    void_ecs::World& world,
    void_ecs::Entity entity,
    const nlohmann::json& data)
{
    DamageReceiver dr;
    dr.armor = data.value("armor", 0.0f);
    dr.damage_multiplier = data.value("damage_multiplier", 1.0f);

    // Validate
    if (dr.armor < 0.0f) {
        dr.armor = 0.0f;
    }
    if (dr.damage_multiplier < 0.0f) {
        dr.damage_multiplier = 0.0f;
    }

    world.add_component(entity, dr);
    return void_core::Ok();
}

/// Apply Dead tag component from JSON
static void_core::Result<void> apply_dead(
    void_ecs::World& world,
    void_ecs::Entity entity,
    [[maybe_unused]] const nlohmann::json& data)
{
    world.add_component(entity, Dead{});
    return void_core::Ok();
}

// =============================================================================
// Plugin Lifecycle
// =============================================================================

void_core::Result<void> HealthPlugin::on_load(void_plugin_api::PluginContext& ctx) {
    spdlog::info("[base.health] Loading plugin v{}.{}.{}",
                 version().major, version().minor, version().patch);

    m_ctx = &ctx;

    // Register components with JSON factories
    m_health_id = ctx.register_component<Health>("Health", apply_health);
    if (m_health_id.value() == 0) {
        return void_core::Error("Failed to register Health component");
    }
    spdlog::info("[base.health]   Registered Health component (id={})", m_health_id.value());

    m_damage_receiver_id = ctx.register_component<DamageReceiver>("DamageReceiver", apply_damage_receiver);
    if (m_damage_receiver_id.value() == 0) {
        return void_core::Error("Failed to register DamageReceiver component");
    }
    spdlog::info("[base.health]   Registered DamageReceiver component (id={})", m_damage_receiver_id.value());

    m_dead_id = ctx.register_component<Dead>("Dead", apply_dead);
    if (m_dead_id.value() == 0) {
        return void_core::Error("Failed to register Dead component");
    }
    spdlog::info("[base.health]   Registered Dead component (id={})", m_dead_id.value());

    // Register systems
    // HealthRegenSystem runs in FixedUpdate (consistent timestep)
    ctx.register_system(
        void_kernel::Stage::FixedUpdate,
        "base.health.HealthRegenSystem",
        [this](float dt) { run_health_regen(dt); },
        10  // Priority 10 - run after core physics
    );
    spdlog::info("[base.health]   Registered HealthRegenSystem (FixedUpdate, priority=10)");

    // DeathSystem runs in PostFixed (after all damage has been applied)
    ctx.register_system(
        void_kernel::Stage::PostFixed,
        "base.health.DeathSystem",
        [this](float dt) { run_death_check(dt); },
        0   // Priority 0 - run early in PostFixed
    );
    spdlog::info("[base.health]   Registered DeathSystem (PostFixed, priority=0)");

    // TODO: Subscribe to DamageEvent when event system is integrated
    // auto handle = ctx.subscribe<DamageEvent>([this](const DamageEvent& e) {
    //     handle_damage(e);
    // });

    spdlog::info("[base.health] Plugin loaded successfully");
    return void_core::Ok();
}

void_core::Result<void> HealthPlugin::on_unload(void_plugin_api::PluginContext& ctx) {
    spdlog::info("[base.health] Unloading plugin");

    // Systems and subscriptions are automatically cleaned up by PluginContext
    // Just reset our state
    m_ctx = nullptr;
    m_health_id = void_ecs::ComponentId{0};
    m_damage_receiver_id = void_ecs::ComponentId{0};
    m_dead_id = void_ecs::ComponentId{0};

    spdlog::info("[base.health]   Stats: {} entities regenerated, {} entities killed",
                 m_entities_regenerated, m_entities_killed);

    return void_core::Ok();
}

// =============================================================================
// Hot-Reload Support
// =============================================================================

void_plugin_api::PluginSnapshot HealthPlugin::snapshot() {
    spdlog::info("[base.health] Creating snapshot for hot-reload");

    void_plugin_api::PluginSnapshot snap;
    snap.type_name = "base.health::HealthPlugin";
    snap.version = version();

    // Serialize statistics
    snap.data.resize(sizeof(std::uint32_t) * 2);
    std::memcpy(snap.data.data(), &m_entities_regenerated, sizeof(std::uint32_t));
    std::memcpy(snap.data.data() + sizeof(std::uint32_t), &m_entities_killed, sizeof(std::uint32_t));

    // Store component IDs in metadata (they may change after reload)
    snap.set_metadata("health_id", std::to_string(m_health_id.value()));
    snap.set_metadata("damage_receiver_id", std::to_string(m_damage_receiver_id.value()));
    snap.set_metadata("dead_id", std::to_string(m_dead_id.value()));

    spdlog::info("[base.health]   Snapshot size: {} bytes", snap.data.size());
    return snap;
}

void_core::Result<void> HealthPlugin::restore(const void_plugin_api::PluginSnapshot& snap) {
    spdlog::info("[base.health] Restoring from snapshot");

    // Validate snapshot
    if (snap.type_name != "base.health::HealthPlugin") {
        return void_core::Error("Invalid snapshot type: " + snap.type_name);
    }

    // Check version compatibility (allow minor version differences)
    if (snap.version.major != version().major) {
        return void_core::Error("Incompatible snapshot version: " +
                                 std::to_string(snap.version.major) + "." +
                                 std::to_string(snap.version.minor) + "." +
                                 std::to_string(snap.version.patch));
    }

    // Restore statistics
    if (snap.data.size() >= sizeof(std::uint32_t) * 2) {
        std::memcpy(&m_entities_regenerated, snap.data.data(), sizeof(std::uint32_t));
        std::memcpy(&m_entities_killed, snap.data.data() + sizeof(std::uint32_t), sizeof(std::uint32_t));
    }

    spdlog::info("[base.health]   Restored stats: {} regenerated, {} killed",
                 m_entities_regenerated, m_entities_killed);

    return void_core::Ok();
}

void HealthPlugin::on_reloaded() {
    spdlog::info("[base.health] Hot-reload complete");
}

// =============================================================================
// System Functions
// =============================================================================

void HealthPlugin::run_health_regen(float dt) {
    if (!m_ctx) return;

    void_ecs::World& world = m_ctx->world_mut();

    // Query all entities with Health component that don't have Dead tag
    auto query_state = world.query_with<Health>();
    world.update_query(query_state);
    auto iter = world.query_iter(query_state);

    while (!iter.empty()) {
        void_ecs::Entity entity = iter.entity();

        // Skip dead entities
        if (world.has_component<Dead>(entity)) {
            iter.next();
            continue;
        }

        Health* health = iter.get<Health>();
        if (health && health->regen_rate > 0.0f && !health->is_full()) {
            // Update time since damage
            health->time_since_damage += dt;

            // Only regenerate after delay
            if (health->time_since_damage >= health->regen_delay) {
                float old_health = health->current;
                health->current += health->regen_rate * dt;
                health->clamp();

                // Track if we actually regenerated
                if (health->current > old_health) {
                    ++m_entities_regenerated;
                }
            }
        }

        iter.next();
    }
}

void HealthPlugin::run_death_check([[maybe_unused]] float dt) {
    if (!m_ctx) return;

    void_ecs::World& world = m_ctx->world_mut();

    // Query all entities with Health component
    auto query_state = world.query_with<Health>();
    world.update_query(query_state);
    auto iter = world.query_iter(query_state);

    while (!iter.empty()) {
        void_ecs::Entity entity = iter.entity();

        // Skip already dead entities
        if (world.has_component<Dead>(entity)) {
            iter.next();
            continue;
        }

        const Health* health = iter.get<Health>();
        if (health && !health->is_alive()) {
            // Mark as dead
            world.add_component(entity, Dead{});
            ++m_entities_killed;

            // TODO: Dispatch DeathEvent when event system is integrated
            // DeathEvent event;
            // event.entity = void_core::EntityId{entity.index};
            // event.killer = void_core::EntityId{0};  // Unknown killer
            // m_ctx->events()->publish(event);

            spdlog::debug("[base.health] Entity {} died", entity.index);
        }

        iter.next();
    }
}

} // namespace base_health

// =============================================================================
// Plugin Entry Points
// =============================================================================

VOID_DECLARE_PLUGIN(base_health::HealthPlugin)
