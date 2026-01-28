/// @file gamestate_core.hpp
/// @brief Core game state system that owns all persistent state
///
/// GameStateCore is the authoritative owner of all gameplay state.
/// It persists across plugin hot-reloads, ensuring state is never lost.
/// Plugins read state through IPluginAPI and submit commands to modify it.

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "variables.hpp"
#include "objectives.hpp"
#include "saveload.hpp"
#include "gamestate.hpp"

#include <void_engine/plugin_api/fwd.hpp>
#include <void_engine/plugin_api/state_stores.hpp>
#include <void_engine/plugin_api/commands.hpp>
#include <void_engine/plugin_api/plugin_api.hpp>
#include <void_engine/plugin_api/plugin_watcher.hpp>
#include <void_engine/plugin_api/dynamic_library.hpp>
#include <void_engine/core/plugin.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace void_gamestate {

// =============================================================================
// GameStateCore Configuration
// =============================================================================

/// @brief Configuration for GameStateCore
struct GameStateCoreConfig {
    // State store limits
    std::uint32_t max_ai_entities{10000};
    std::uint32_t max_combat_entities{10000};
    std::uint32_t max_inventory_entities{10000};
    std::uint32_t max_world_items{50000};
    std::uint32_t max_projectiles{5000};

    // Command processing
    std::uint32_t max_queued_commands{1000};
    bool validate_commands{true};
    bool log_commands{false};

    // Hot-reload
    bool enable_hot_reload{true};
    std::string plugin_directory{"plugins"};

    // Save/load integration
    bool auto_save_state{true};
    bool persist_across_levels{true};

    // Inherit from base config
    GameStateConfig base_config;
};

// =============================================================================
// GameStateCore
// =============================================================================

/// @brief Central game state management owning all persistent state
///
/// GameStateCore extends GameStateSystem with:
/// - AI, Combat, Inventory state stores
/// - Command processing system
/// - Plugin API for gameplay plugins
/// - State persistence across hot-reloads
/// - Automatic plugin discovery and hot-reload via PluginWatcher
class GameStateCore : public GameStateSystem, public void_plugin_api::IPluginLoader {
public:
    GameStateCore();
    explicit GameStateCore(const GameStateCoreConfig& config);
    ~GameStateCore();

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /// @brief Initialize the core system
    void initialize();

    /// @brief Shutdown and cleanup
    void shutdown();

    /// @brief Check if initialized
    [[nodiscard]] bool is_initialized() const { return m_core_initialized; }

    // -------------------------------------------------------------------------
    // State Store Access (for internal use and PluginAPIImpl)
    // -------------------------------------------------------------------------

    /// @brief Get AI state store (mutable - for command execution)
    [[nodiscard]] void_plugin_api::AIStateStore& ai_state() { return m_ai_state; }
    [[nodiscard]] const void_plugin_api::AIStateStore& ai_state() const { return m_ai_state; }

    /// @brief Get combat state store
    [[nodiscard]] void_plugin_api::CombatStateStore& combat_state() { return m_combat_state; }
    [[nodiscard]] const void_plugin_api::CombatStateStore& combat_state() const { return m_combat_state; }

    /// @brief Get inventory state store
    [[nodiscard]] void_plugin_api::InventoryStateStore& inventory_state() { return m_inventory_state; }
    [[nodiscard]] const void_plugin_api::InventoryStateStore& inventory_state() const { return m_inventory_state; }

    // -------------------------------------------------------------------------
    // Command Processing
    // -------------------------------------------------------------------------

    /// @brief Get command processor
    [[nodiscard]] void_plugin_api::CommandProcessor& command_processor() { return *m_command_processor; }

    /// @brief Execute a command immediately
    void_plugin_api::CommandResult execute_command(void_plugin_api::CommandPtr command);

    /// @brief Queue a command for deferred execution
    void queue_command(void_plugin_api::CommandPtr command);

    /// @brief Process all queued commands
    void process_commands();

    // -------------------------------------------------------------------------
    // Plugin API
    // -------------------------------------------------------------------------

    /// @brief Get plugin API for gameplay plugins
    [[nodiscard]] void_plugin_api::IPluginAPI* plugin_api() { return m_plugin_api.get(); }

    // -------------------------------------------------------------------------
    // Plugin Management
    // -------------------------------------------------------------------------

    /// @brief Register a gameplay plugin
    void_core::Result<void> register_plugin(std::unique_ptr<void_plugin_api::GameplayPlugin> plugin);

    /// @brief Load and activate a plugin
    void_core::Result<void> load_plugin(const std::string& name);

    /// @brief Unload a plugin
    void_core::Result<void> unload_plugin(const std::string& name);

    /// @brief Hot-reload a plugin
    void_core::Result<void> hot_reload_plugin(const std::string& name,
                                               std::unique_ptr<void_plugin_api::GameplayPlugin> new_plugin);

    /// @brief Get active plugin count
    [[nodiscard]] std::size_t active_plugin_count() const;

    /// @brief Update all active plugins
    void update_plugins(float dt);

    /// @brief Fixed update all active plugins
    void fixed_update_plugins(float fixed_dt);

    // -------------------------------------------------------------------------
    // IPluginLoader Interface (for PluginWatcher integration)
    // -------------------------------------------------------------------------

    /// @brief Load a plugin by path (for watcher)
    bool watcher_load_plugin(const std::filesystem::path& path) override;

    /// @brief Unload a plugin by name (for watcher)
    bool watcher_unload_plugin(const std::string& name) override;

    /// @brief Hot-reload a plugin (preserves state, for watcher)
    bool watcher_hot_reload_plugin(const std::string& name, const std::filesystem::path& new_path) override;

    /// @brief Check if a plugin is loaded (for watcher)
    bool watcher_is_plugin_loaded(const std::string& name) const override;

    /// @brief Get list of loaded plugin names (for watcher)
    std::vector<std::string> watcher_loaded_plugins() const override;

    // -------------------------------------------------------------------------
    // Plugin Watcher (Automatic Hot-Reload)
    // -------------------------------------------------------------------------

    /// @brief Get the plugin watcher (creates on first call if enabled)
    void_plugin_api::PluginWatcher* watcher();

    /// @brief Start watching for plugin changes
    void start_watching(const std::vector<std::filesystem::path>& paths = {});

    /// @brief Stop watching for plugin changes
    void stop_watching();

    /// @brief Check if watching is active
    bool is_watching() const;

    /// @brief Configure the watcher
    void configure_watcher(const void_plugin_api::PluginWatcherConfig& config);

    /// @brief Get the custom plugin state registry
    void_plugin_api::PluginStateRegistry& state_registry() { return m_state_registry; }

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    /// @brief Main update (call from game loop)
    void update(float dt);

    /// @brief Fixed update (call at fixed timestep)
    void fixed_update(float fixed_dt);

    // -------------------------------------------------------------------------
    // State Management
    // -------------------------------------------------------------------------

    /// @brief Clear all gameplay state (for new game)
    void clear_gameplay_state();

    /// @brief Clear state for specific entity
    void clear_entity_state(void_plugin_api::EntityId entity);

    /// @brief Register entity with gameplay systems
    void register_entity(void_plugin_api::EntityId entity);

    /// @brief Unregister entity from gameplay systems
    void unregister_entity(void_plugin_api::EntityId entity);

    // -------------------------------------------------------------------------
    // Serialization (extends base)
    // -------------------------------------------------------------------------

    /// @brief Serialize all state for save
    std::vector<std::uint8_t> serialize_state() const;

    /// @brief Deserialize state from save
    void deserialize_state(const std::vector<std::uint8_t>& data);

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    struct Stats {
        std::uint64_t commands_executed{0};
        std::uint64_t commands_failed{0};
        std::uint64_t ai_entities{0};
        std::uint64_t combat_entities{0};
        std::uint64_t inventory_entities{0};
        std::uint64_t active_projectiles{0};
        std::uint64_t world_items{0};
        std::uint32_t active_plugins{0};
    };

    [[nodiscard]] Stats stats() const;

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    using DamageCallback = std::function<void(void_plugin_api::EntityId target,
                                               void_plugin_api::EntityId source,
                                               float damage, bool killed)>;
    using DeathCallback = std::function<void(void_plugin_api::EntityId entity,
                                              void_plugin_api::EntityId killer)>;
    using ItemCallback = std::function<void(void_plugin_api::EntityId entity,
                                             void_plugin_api::ItemInstanceId item)>;

    void on_damage(DamageCallback callback) { m_on_damage = std::move(callback); }
    void on_death(DeathCallback callback) { m_on_death = std::move(callback); }
    void on_item_acquired(ItemCallback callback) { m_on_item_acquired = std::move(callback); }
    void on_item_lost(ItemCallback callback) { m_on_item_lost = std::move(callback); }

    // -------------------------------------------------------------------------
    // Internal callbacks for command execution
    // -------------------------------------------------------------------------

    void notify_damage(void_plugin_api::EntityId target, void_plugin_api::EntityId source,
                       float damage, bool killed);
    void notify_death(void_plugin_api::EntityId entity, void_plugin_api::EntityId killer);
    void notify_item_acquired(void_plugin_api::EntityId entity, void_plugin_api::ItemInstanceId item);
    void notify_item_lost(void_plugin_api::EntityId entity, void_plugin_api::ItemInstanceId item);

private:
    void setup_command_processor();
    void setup_plugin_api();

    GameStateCoreConfig m_core_config;
    bool m_core_initialized{false};

    // State stores (OWNED - persist across hot-reloads)
    void_plugin_api::AIStateStore m_ai_state;
    void_plugin_api::CombatStateStore m_combat_state;
    void_plugin_api::InventoryStateStore m_inventory_state;

    // Command processor
    std::unique_ptr<void_plugin_api::CommandProcessor> m_command_processor;

    // Plugin API
    std::unique_ptr<void_plugin_api::PluginAPIImpl> m_plugin_api;

    // Plugin registry
    void_core::PluginRegistry m_plugin_registry;
    void_core::TypeRegistry m_type_registry;

    // Plugin watcher for automatic hot-reload
    std::unique_ptr<void_plugin_api::PluginWatcher> m_watcher;
    void_plugin_api::PluginWatcherConfig m_watcher_config;

    // Custom plugin state registry
    void_plugin_api::PluginStateRegistry m_state_registry;

    // Plugin path to name mapping
    std::unordered_map<std::filesystem::path, std::string> m_path_to_plugin;
    std::unordered_map<std::string, std::filesystem::path> m_plugin_to_path;

    // Dynamically loaded plugins (owns the DLL handles and plugin instances)
    std::unordered_map<std::string, std::unique_ptr<void_plugin_api::LoadedPlugin>> m_loaded_plugins;

    // Time tracking
    float m_delta_time{0};
    std::uint32_t m_frame_number{0};

    // Callbacks
    DamageCallback m_on_damage;
    DeathCallback m_on_death;
    ItemCallback m_on_item_acquired;
    ItemCallback m_on_item_lost;
};

} // namespace void_gamestate
