/// @file state.hpp
/// @brief Plugin state tracking as ECS resources
///
/// PluginState and PluginRegistry are ECS resources that track plugin lifecycle:
/// - Which plugins are loaded
/// - What components/systems each plugin registered
/// - Load order for dependency management
/// - Status for debugging and hot-reload coordination
///
/// These resources are inserted into the ECS world before any plugins load,
/// allowing systems to query plugin state and plugins to discover each other.

#pragma once

#include <void_engine/core/version.hpp>
#include <void_engine/ecs/entity.hpp>
#include <void_engine/ecs/component.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace void_plugin_api {

// =============================================================================
// PluginStatus
// =============================================================================

/// @brief Current status of a plugin in its lifecycle
enum class PluginStatus : std::uint8_t {
    Loading,        ///< Plugin is being loaded (on_load in progress)
    Active,         ///< Plugin is loaded and running
    Reloading,      ///< Plugin is being hot-reloaded
    Unloading,      ///< Plugin is being unloaded (on_unload in progress)
    Failed,         ///< Plugin load/reload failed
    Unloaded        ///< Plugin has been unloaded
};

/// @brief Convert PluginStatus to string
[[nodiscard]] inline const char* to_string(PluginStatus status) noexcept {
    switch (status) {
        case PluginStatus::Loading:   return "Loading";
        case PluginStatus::Active:    return "Active";
        case PluginStatus::Reloading: return "Reloading";
        case PluginStatus::Unloading: return "Unloading";
        case PluginStatus::Failed:    return "Failed";
        case PluginStatus::Unloaded:  return "Unloaded";
    }
    return "Unknown";
}

// =============================================================================
// PluginState
// =============================================================================

/// @brief State of a single loaded plugin
///
/// Tracks everything a plugin has registered with the engine, enabling:
/// - Clean unloading (know what to remove)
/// - Hot-reload (know what to re-register)
/// - Debugging (inspect plugin contributions)
/// - Dependency analysis (what depends on what)
struct PluginState {
    // =========================================================================
    // Identity
    // =========================================================================

    /// Plugin identifier (e.g., "base.health", "mygame.combat")
    std::string id;

    /// Plugin version
    void_core::Version version{0, 0, 0};

    /// Current status in lifecycle
    PluginStatus status{PluginStatus::Loading};

    /// Path to the plugin's main library (DLL/SO)
    std::string library_path;

    /// Human-readable description
    std::string description;

    /// Author information
    std::string author;

    // =========================================================================
    // Registrations
    // =========================================================================

    /// Component names registered by this plugin
    std::vector<std::string> registered_components;

    /// System names registered by this plugin
    std::vector<std::string> registered_systems;

    /// Event subscriptions held by this plugin
    std::vector<std::string> subscriptions;

    /// Entities owned/created by this plugin (for cleanup)
    std::vector<void_ecs::Entity> owned_entities;

    // =========================================================================
    // Dependencies
    // =========================================================================

    /// Plugin IDs this plugin depends on
    std::vector<std::string> dependencies;

    /// Plugin IDs that depend on this plugin (computed)
    std::vector<std::string> dependents;

    // =========================================================================
    // Timing
    // =========================================================================

    /// When the plugin was loaded (steady_clock for duration calculations)
    std::chrono::steady_clock::time_point loaded_at;

    /// When the plugin was last hot-reloaded (or loaded_at if never reloaded)
    std::chrono::steady_clock::time_point last_reloaded_at;

    /// Number of times this plugin has been hot-reloaded
    std::uint32_t reload_count{0};

    // =========================================================================
    // Error State
    // =========================================================================

    /// Error message if status is Failed
    std::string error_message;

    // =========================================================================
    // Helpers
    // =========================================================================

    /// Check if plugin is currently active
    [[nodiscard]] bool is_active() const noexcept {
        return status == PluginStatus::Active;
    }

    /// Check if plugin has been reloaded
    [[nodiscard]] bool has_reloaded() const noexcept {
        return reload_count > 0;
    }

    /// Get time since load (or last reload)
    [[nodiscard]] std::chrono::steady_clock::duration uptime() const noexcept {
        return std::chrono::steady_clock::now() - last_reloaded_at;
    }

    /// Check if plugin registered a specific component
    [[nodiscard]] bool has_component(const std::string& name) const {
        for (const auto& c : registered_components) {
            if (c == name) return true;
        }
        return false;
    }

    /// Check if plugin registered a specific system
    [[nodiscard]] bool has_system(const std::string& name) const {
        for (const auto& s : registered_systems) {
            if (s == name) return true;
        }
        return false;
    }

    /// Check if plugin depends on another
    [[nodiscard]] bool depends_on(const std::string& plugin_id) const {
        for (const auto& d : dependencies) {
            if (d == plugin_id) return true;
        }
        return false;
    }

    /// Create initial state for a loading plugin
    [[nodiscard]] static PluginState loading(const std::string& plugin_id,
                                              const void_core::Version& ver) {
        PluginState state;
        state.id = plugin_id;
        state.version = ver;
        state.status = PluginStatus::Loading;
        state.loaded_at = std::chrono::steady_clock::now();
        state.last_reloaded_at = state.loaded_at;
        return state;
    }
};

// =============================================================================
// PluginRegistry
// =============================================================================

/// @brief Registry of all loaded plugins (ECS Resource)
///
/// This is an ECS resource inserted into the world before any plugins load.
/// It provides a central place to:
/// - Query which plugins are loaded
/// - Look up plugin state by ID
/// - Iterate plugins in load order
/// - Find dependents for safe unloading
///
/// Thread Safety:
/// - Read operations are safe from any thread
/// - Write operations must happen on the main thread
/// - The registry itself is stored as an ECS resource (single instance)
struct PluginRegistry {
    // =========================================================================
    // Plugin Storage
    // =========================================================================

    /// All plugin states, keyed by plugin ID
    std::map<std::string, PluginState> plugins;

    /// Plugin IDs in load order (first loaded = first in vector)
    std::vector<std::string> load_order;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Get plugin state by ID
    [[nodiscard]] PluginState* get(const std::string& plugin_id) {
        auto it = plugins.find(plugin_id);
        return it != plugins.end() ? &it->second : nullptr;
    }

    /// Get plugin state by ID (const)
    [[nodiscard]] const PluginState* get(const std::string& plugin_id) const {
        auto it = plugins.find(plugin_id);
        return it != plugins.end() ? &it->second : nullptr;
    }

    /// Check if a plugin is loaded (any status except Unloaded)
    [[nodiscard]] bool is_loaded(const std::string& plugin_id) const {
        auto it = plugins.find(plugin_id);
        return it != plugins.end() && it->second.status != PluginStatus::Unloaded;
    }

    /// Check if a plugin is active
    [[nodiscard]] bool is_active(const std::string& plugin_id) const {
        auto it = plugins.find(plugin_id);
        return it != plugins.end() && it->second.is_active();
    }

    /// Get all plugin IDs that depend on a given plugin
    [[nodiscard]] std::vector<std::string> dependents_of(const std::string& plugin_id) const {
        std::vector<std::string> result;
        for (const auto& [id, state] : plugins) {
            if (state.depends_on(plugin_id)) {
                result.push_back(id);
            }
        }
        return result;
    }

    /// Get all active plugin IDs
    [[nodiscard]] std::vector<std::string> active_plugins() const {
        std::vector<std::string> result;
        for (const auto& id : load_order) {
            auto it = plugins.find(id);
            if (it != plugins.end() && it->second.is_active()) {
                result.push_back(id);
            }
        }
        return result;
    }

    /// Get number of loaded plugins
    [[nodiscard]] std::size_t count() const noexcept {
        return plugins.size();
    }

    /// Check if registry is empty
    [[nodiscard]] bool empty() const noexcept {
        return plugins.empty();
    }

    // =========================================================================
    // Modification (main thread only)
    // =========================================================================

    /// Add a new plugin (called when loading starts)
    void add(PluginState state) {
        const std::string id = state.id;
        plugins[id] = std::move(state);
        load_order.push_back(id);
    }

    /// Update plugin status
    void set_status(const std::string& plugin_id, PluginStatus status) {
        if (auto* state = get(plugin_id)) {
            state->status = status;
        }
    }

    /// Update plugin status with error
    void set_failed(const std::string& plugin_id, const std::string& error) {
        if (auto* state = get(plugin_id)) {
            state->status = PluginStatus::Failed;
            state->error_message = error;
        }
    }

    /// Mark plugin as reloaded
    void mark_reloaded(const std::string& plugin_id) {
        if (auto* state = get(plugin_id)) {
            state->reload_count++;
            state->last_reloaded_at = std::chrono::steady_clock::now();
            state->status = PluginStatus::Active;
        }
    }

    /// Remove a plugin (called after unload completes)
    void remove(const std::string& plugin_id) {
        plugins.erase(plugin_id);
        load_order.erase(
            std::remove(load_order.begin(), load_order.end(), plugin_id),
            load_order.end()
        );
    }

    /// Update dependents for all plugins (call after dependency graph changes)
    void rebuild_dependents() {
        // Clear all dependents
        for (auto& [id, state] : plugins) {
            state.dependents.clear();
        }

        // Rebuild from dependencies
        for (const auto& [id, state] : plugins) {
            for (const auto& dep : state.dependencies) {
                if (auto* dep_state = get(dep)) {
                    dep_state->dependents.push_back(id);
                }
            }
        }
    }

    // =========================================================================
    // Component/System Lookup
    // =========================================================================

    /// Find which plugin registered a component
    [[nodiscard]] std::optional<std::string> find_component_owner(
        const std::string& component_name) const
    {
        for (const auto& [id, state] : plugins) {
            if (state.has_component(component_name)) {
                return id;
            }
        }
        return std::nullopt;
    }

    /// Find which plugin registered a system
    [[nodiscard]] std::optional<std::string> find_system_owner(
        const std::string& system_name) const
    {
        for (const auto& [id, state] : plugins) {
            if (state.has_system(system_name)) {
                return id;
            }
        }
        return std::nullopt;
    }
};

} // namespace void_plugin_api
