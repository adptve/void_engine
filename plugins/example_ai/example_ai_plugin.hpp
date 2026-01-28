/// @file example_ai_plugin.hpp
/// @brief Example AI plugin demonstrating the plugin system
///
/// This is a complete, working example of a hot-swappable gameplay plugin.
/// Developers can use this as a template for their own plugins.

#pragma once

#include <void_engine/plugin_api/plugin_api.hpp>
#include <void_engine/plugin_api/state_stores.hpp>
#include <void_engine/plugin_api/commands.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace example_plugins {

/// @brief Example AI plugin that demonstrates behavior tree-style logic
///
/// This plugin shows how to:
/// - Read state from GameStateCore (AI, Combat, Inventory)
/// - Submit commands to modify state
/// - Handle hot-reload with runtime state preservation
/// - Register custom plugin state
class ExampleAIPlugin : public void_plugin_api::GameplayPlugin {
public:
    ExampleAIPlugin();
    ~ExampleAIPlugin() override = default;

    // -------------------------------------------------------------------------
    // Plugin Identity (required)
    // -------------------------------------------------------------------------

    [[nodiscard]] void_core::PluginId id() const override {
        return void_core::PluginId("example_ai");
    }

    [[nodiscard]] void_core::Version version() const override {
        return void_core::Version{1, 0, 0};
    }

    [[nodiscard]] std::string type_name() const override {
        return "ExampleAIPlugin";
    }

    [[nodiscard]] bool supports_hot_reload() const override {
        return true;  // This plugin supports hot-reload!
    }

    // -------------------------------------------------------------------------
    // Lifecycle Callbacks
    // -------------------------------------------------------------------------

    /// Called when plugin is loaded (or reloaded)
    void on_plugin_load(void_plugin_api::IPluginAPI* api) override;

    /// Called every frame
    void on_tick(float dt) override;

    /// Called at fixed timestep (physics rate)
    void on_fixed_tick(float dt) override;

    /// Called when plugin is about to be unloaded
    void on_plugin_unload() override;

    // -------------------------------------------------------------------------
    // Hot-Reload State Preservation
    // -------------------------------------------------------------------------

    /// Serialize runtime state for hot-reload
    [[nodiscard]] std::vector<std::uint8_t> serialize_runtime_state() const override;

    /// Restore runtime state after hot-reload
    void deserialize_runtime_state(const std::vector<std::uint8_t>& data) override;

private:
    // -------------------------------------------------------------------------
    // AI Logic
    // -------------------------------------------------------------------------

    void process_entity_ai(void_plugin_api::EntityId entity, float dt);
    void update_perception(void_plugin_api::EntityId entity);
    void evaluate_behavior(void_plugin_api::EntityId entity);
    void execute_actions(void_plugin_api::EntityId entity, float dt);

    // -------------------------------------------------------------------------
    // Runtime State (preserved across hot-reload)
    // -------------------------------------------------------------------------

    /// Entities this plugin is managing
    std::vector<void_plugin_api::EntityId> m_managed_entities;

    /// Per-entity decision timers
    std::unordered_map<void_plugin_api::EntityId, float> m_decision_timers;

    /// Per-entity current action
    std::unordered_map<void_plugin_api::EntityId, std::string> m_current_actions;

    /// Total time plugin has been running
    float m_total_runtime{0};

    /// Number of AI decisions made
    std::uint32_t m_decisions_made{0};

    // -------------------------------------------------------------------------
    // Configuration (could be loaded from file)
    // -------------------------------------------------------------------------

    float m_decision_interval{0.5f};  // How often to re-evaluate behavior
    float m_perception_range{50.0f};  // How far entities can see
    float m_attack_range{5.0f};       // Melee attack range
};

// =============================================================================
// Plugin Factory (required for dynamic loading)
// =============================================================================

/// @brief Creates the plugin instance - called by the engine when loading the DLL/SO
extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #else
    __attribute__((visibility("default")))
    #endif
    void_plugin_api::GameplayPlugin* create_plugin();

    #ifdef _WIN32
    __declspec(dllexport)
    #else
    __attribute__((visibility("default")))
    #endif
    void destroy_plugin(void_plugin_api::GameplayPlugin* plugin);
}

} // namespace example_plugins
