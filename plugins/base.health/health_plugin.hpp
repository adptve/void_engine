/// @file health_plugin.hpp
/// @brief base.health plugin - Core health and damage system
///
/// Provides fundamental health/damage gameplay components:
/// - Health: Current/max health with optional regeneration
/// - DamageReceiver: Armor and damage modifiers
/// - Dead: Tag marking dead entities
///
/// Systems:
/// - HealthRegenSystem: Regenerates health over time
/// - DeathSystem: Marks zero-health entities as dead

#pragma once

#include <void_engine/plugin_api/plugin.hpp>
#include <void_engine/plugin_api/context.hpp>
#include <void_engine/core/id.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace base_health {

// =============================================================================
// Components
// =============================================================================

/// @brief Health component - current and max health with regeneration support
struct Health {
    float current = 100.0f;         ///< Current health value
    float max = 100.0f;             ///< Maximum health value
    float regen_rate = 0.0f;        ///< Health regenerated per second (0 = no regen)
    float regen_delay = 0.0f;       ///< Seconds after damage before regen starts
    float time_since_damage = 0.0f; ///< Time since last damage taken (for regen delay)

    /// @brief Clamp current health to [0, max]
    void clamp() {
        if (current < 0.0f) current = 0.0f;
        if (current > max) current = max;
    }

    /// @brief Check if entity is alive
    [[nodiscard]] bool is_alive() const { return current > 0.0f; }

    /// @brief Check if health is full
    [[nodiscard]] bool is_full() const { return current >= max; }

    /// @brief Get health as percentage [0, 1]
    [[nodiscard]] float percent() const {
        return max > 0.0f ? current / max : 0.0f;
    }
};

/// @brief DamageReceiver component - armor and damage modifiers
struct DamageReceiver {
    float armor = 0.0f;             ///< Flat damage reduction
    float damage_multiplier = 1.0f; ///< Multiplier applied after armor (e.g., 0.5 = 50% damage taken)

    /// @brief Calculate final damage after armor and multiplier
    [[nodiscard]] float calculate_damage(float raw_damage) const {
        float after_armor = raw_damage - armor;
        if (after_armor < 0.0f) after_armor = 0.0f;
        return after_armor * damage_multiplier;
    }
};

/// @brief Dead tag component - marks entity as dead
struct Dead {
    std::uint8_t padding = 0; // Non-empty for ECS compatibility
};

// =============================================================================
// Events
// =============================================================================

/// @brief Event fired when an entity takes damage
struct DamageEvent {
    void_core::EntityId target{0};  ///< Entity that took damage
    void_core::EntityId source{0};  ///< Entity that caused damage (0 = environmental)
    float amount = 0.0f;            ///< Amount of damage dealt (after modifiers)
    float raw_amount = 0.0f;        ///< Original damage before modifiers
    std::string damage_type;        ///< Type of damage (e.g., "physical", "fire", "poison")
};

/// @brief Event fired when an entity dies
struct DeathEvent {
    void_core::EntityId entity{0};  ///< Entity that died
    void_core::EntityId killer{0};  ///< Entity that dealt killing blow (0 = environmental/self)
};

/// @brief Event fired when an entity is healed
struct HealEvent {
    void_core::EntityId target{0};  ///< Entity that was healed
    void_core::EntityId source{0};  ///< Entity that caused healing (0 = self/regen)
    float amount = 0.0f;            ///< Amount healed
};

// =============================================================================
// Plugin Implementation
// =============================================================================

/// @brief base.health plugin implementation
///
/// Registers Health, DamageReceiver, and Dead components with JSON factories.
/// Runs HealthRegenSystem (FixedUpdate) and DeathSystem (PostFixed).
class HealthPlugin : public void_plugin_api::IPlugin {
public:
    HealthPlugin() = default;
    ~HealthPlugin() override = default;

    // =========================================================================
    // IPlugin Interface
    // =========================================================================

    [[nodiscard]] std::string id() const override {
        return "base.health";
    }

    [[nodiscard]] void_core::Version version() const override {
        return {1, 0, 0};
    }

    [[nodiscard]] std::vector<void_plugin_api::Dependency> dependencies() const override {
        return {}; // No dependencies - this is a base plugin
    }

    [[nodiscard]] void_core::Result<void> on_load(void_plugin_api::PluginContext& ctx) override;
    [[nodiscard]] void_core::Result<void> on_unload(void_plugin_api::PluginContext& ctx) override;

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    [[nodiscard]] void_plugin_api::PluginSnapshot snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(const void_plugin_api::PluginSnapshot& snap) override;
    void on_reloaded() override;

    [[nodiscard]] bool supports_hot_reload() const override {
        return true;
    }

    // =========================================================================
    // Introspection
    // =========================================================================

    [[nodiscard]] std::vector<std::string> component_names() const override {
        return {"Health", "DamageReceiver", "Dead"};
    }

    [[nodiscard]] std::vector<std::string> system_names() const override {
        return {"base.health.HealthRegenSystem", "base.health.DeathSystem"};
    }

    [[nodiscard]] std::string description() const override {
        return "Core health and damage system with regeneration and death handling";
    }

    [[nodiscard]] std::string author() const override {
        return "void_engine";
    }

private:
    // =========================================================================
    // System Functions
    // =========================================================================

    /// @brief Run health regeneration for all entities with Health component
    void run_health_regen(float dt);

    /// @brief Check for dead entities and mark them with Dead component
    void run_death_check(float dt);

    // =========================================================================
    // State
    // =========================================================================

    void_plugin_api::PluginContext* m_ctx = nullptr;

    // Component IDs for fast lookup
    void_ecs::ComponentId m_health_id{0};
    void_ecs::ComponentId m_damage_receiver_id{0};
    void_ecs::ComponentId m_dead_id{0};

    // Statistics for debugging
    std::uint32_t m_entities_regenerated = 0;
    std::uint32_t m_entities_killed = 0;
};

} // namespace base_health
