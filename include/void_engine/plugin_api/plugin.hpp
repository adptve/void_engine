/// @file plugin.hpp
/// @brief Plugin interface for package-based plugins
///
/// This file defines the IPlugin interface for plugins loaded from packages.
/// Unlike void_core::Plugin (which is for in-process plugins managed by PluginRegistry),
/// this interface is specifically for DLL-based plugins that:
/// - Register components with JSON factories
/// - Register systems with kernel stages
/// - Integrate with the ECS world
/// - Support hot-reload with state preservation
///
/// The existing void_core::Plugin infrastructure handles registration and lifecycle;
/// this interface adds the package-specific capabilities needed for component/system
/// registration and the render contract.

#pragma once

#include <void_engine/core/version.hpp>
#include <void_engine/core/error.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_plugin_api {

// Forward declarations
class PluginContext;

// =============================================================================
// PluginSnapshot
// =============================================================================

/// @brief State snapshot for hot-reload preservation
///
/// Plugins serialize their runtime state into this structure before unloading.
/// After reload, the state is passed back to restore(). This enables plugins
/// to preserve entity relationships, cached data, and runtime configuration
/// across hot-reloads.
struct PluginSnapshot {
    /// Serialized binary state data
    std::vector<std::uint8_t> data;

    /// Type identifier for validation
    std::string type_name;

    /// Version of the plugin that created this snapshot
    void_core::Version version;

    /// Optional metadata (JSON-compatible strings)
    std::vector<std::pair<std::string, std::string>> metadata;

    /// Create empty snapshot
    [[nodiscard]] static PluginSnapshot empty() {
        return PluginSnapshot{};
    }

    /// Check if snapshot is empty
    [[nodiscard]] bool is_empty() const noexcept {
        return data.empty();
    }

    /// Get metadata value
    [[nodiscard]] const std::string* get_metadata(const std::string& key) const {
        for (const auto& [k, v] : metadata) {
            if (k == key) return &v;
        }
        return nullptr;
    }

    /// Set metadata value
    void set_metadata(const std::string& key, const std::string& value) {
        for (auto& [k, v] : metadata) {
            if (k == key) {
                v = value;
                return;
            }
        }
        metadata.emplace_back(key, value);
    }
};

// =============================================================================
// Dependency
// =============================================================================

/// @brief Plugin dependency specification
struct Dependency {
    /// Name of the required plugin (e.g., "base.health", "core.physics")
    std::string name;

    /// Minimum required version (0.0.0 = any version)
    void_core::Version min_version{0, 0, 0};

    /// Maximum compatible version (0.0.0 = no maximum)
    void_core::Version max_version{0, 0, 0};

    /// Whether this dependency is optional
    bool optional = false;

    /// Create required dependency
    [[nodiscard]] static Dependency required(const std::string& n,
                                              void_core::Version min = {0, 0, 0}) {
        return Dependency{n, min, {0, 0, 0}, false};
    }

    /// Create optional dependency
    [[nodiscard]] static Dependency opt(const std::string& n,
                                         void_core::Version min = {0, 0, 0}) {
        return Dependency{n, min, {0, 0, 0}, true};
    }

    /// Check if a version satisfies this dependency
    [[nodiscard]] bool is_satisfied_by(const void_core::Version& v) const {
        if (min_version.major > 0 || min_version.minor > 0 || min_version.patch > 0) {
            if (v < min_version) return false;
        }
        if (max_version.major > 0 || max_version.minor > 0 || max_version.patch > 0) {
            if (v > max_version) return false;
        }
        return true;
    }
};

// =============================================================================
// IPlugin Interface
// =============================================================================

/// @brief Interface for package-based plugins
///
/// Plugins implement this interface to integrate with the engine. The lifecycle is:
///
/// 1. DLL loaded, plugin_create() called to instantiate IPlugin
/// 2. on_load(context) called - plugin registers components, systems, subscriptions
/// 3. Frame loop runs, plugin's systems execute
/// 4. Hot-reload triggered:
///    a. snapshot() called - plugin saves runtime state
///    b. on_unload(context) called - plugin cleans up
///    c. DLL unloaded, new DLL loaded, new plugin_create()
///    d. on_load(context) called
///    e. restore(snapshot) called - plugin restores state
///    f. on_reloaded() called - plugin notified
/// 5. Shutdown: on_unload(context), plugin_destroy()
///
/// @note The existing void_core::Plugin handles basic plugin registration with
/// PluginRegistry. This interface extends that concept for package-based plugins
/// that need ECS integration, component factories, and system scheduling.
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // =========================================================================
    // Identification
    // =========================================================================

    /// @brief Get unique plugin identifier
    /// Convention: "vendor.name" (e.g., "base.health", "mygame.combat")
    [[nodiscard]] virtual std::string id() const = 0;

    /// @brief Get plugin version
    [[nodiscard]] virtual void_core::Version version() const = 0;

    /// @brief Get list of dependencies
    [[nodiscard]] virtual std::vector<Dependency> dependencies() const {
        return {};
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// @brief Called when plugin is loaded
    ///
    /// This is where the plugin should:
    /// - Register components with ctx.register_component<T>()
    /// - Register systems with ctx.register_system()
    /// - Subscribe to events with ctx.subscribe<E>()
    /// - Initialize resources
    ///
    /// @param ctx Plugin context providing engine access
    /// @return Ok() on success, error on failure
    [[nodiscard]] virtual void_core::Result<void> on_load(PluginContext& ctx) = 0;

    /// @brief Called when plugin is unloaded
    ///
    /// This is where the plugin should:
    /// - Clean up any resources it owns
    /// - Note: Systems and subscriptions are automatically unregistered
    ///
    /// @param ctx Plugin context
    /// @return Ok() on success, error on failure (cleanup continues regardless)
    [[nodiscard]] virtual void_core::Result<void> on_unload(PluginContext& ctx) = 0;

    // =========================================================================
    // Hot-Reload Support
    // =========================================================================

    /// @brief Capture current state for hot-reload
    ///
    /// Called before unloading during hot-reload. The plugin should serialize
    /// any runtime state that needs to survive the reload (cached lookups,
    /// entity references by external ID, configuration, etc.).
    ///
    /// @return Snapshot containing serialized state, or empty if stateless
    [[nodiscard]] virtual PluginSnapshot snapshot() {
        return PluginSnapshot::empty();
    }

    /// @brief Restore state after hot-reload
    ///
    /// Called after on_load() during hot-reload. The snapshot contains
    /// state captured before the reload. The plugin should deserialize
    /// and apply this state.
    ///
    /// @param snap Snapshot from previous instance's snapshot()
    /// @return Ok() on success, error if state is incompatible
    [[nodiscard]] virtual void_core::Result<void> restore(const PluginSnapshot& snap) {
        (void)snap;
        return void_core::Ok();
    }

    /// @brief Called after reload completes
    ///
    /// Notification that hot-reload has completed and the plugin is
    /// fully operational with restored state. Use this to trigger
    /// any post-reload initialization that depends on restored state.
    virtual void on_reloaded() {}

    /// @brief Check if plugin supports hot-reload
    [[nodiscard]] virtual bool supports_hot_reload() const {
        return false;
    }

    // =========================================================================
    // Introspection
    // =========================================================================

    /// @brief Get names of components registered by this plugin
    ///
    /// Used for debugging and dependency tracking.
    [[nodiscard]] virtual std::vector<std::string> component_names() const {
        return {};
    }

    /// @brief Get names of systems registered by this plugin
    ///
    /// Used for debugging and dependency tracking.
    [[nodiscard]] virtual std::vector<std::string> system_names() const {
        return {};
    }

    /// @brief Get human-readable description
    [[nodiscard]] virtual std::string description() const {
        return "";
    }

    /// @brief Get author information
    [[nodiscard]] virtual std::string author() const {
        return "";
    }
};

// =============================================================================
// Plugin Export Macros
// =============================================================================

/// @brief Platform-specific export macro for plugin symbols
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef VOID_PLUGIN_EXPORT
        #define PLUGIN_API __declspec(dllexport)
    #else
        #define PLUGIN_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define PLUGIN_API __attribute__((visibility("default")))
    #else
        #define PLUGIN_API
    #endif
#endif

/// @brief Declare plugin entry points
///
/// Use in your plugin implementation file to export the required symbols:
/// @code
/// class MyPlugin : public IPlugin { ... };
/// VOID_DECLARE_PLUGIN(MyPlugin)
/// @endcode
#define VOID_DECLARE_PLUGIN(PluginClass) \
    extern "C" PLUGIN_API void_plugin_api::IPlugin* plugin_create() { \
        return new PluginClass(); \
    } \
    extern "C" PLUGIN_API void plugin_destroy(void_plugin_api::IPlugin* plugin) { \
        delete plugin; \
    } \
    extern "C" PLUGIN_API const char* plugin_api_version() { \
        return "1.0.0"; \
    }

/// @brief Plugin entry point function types
using PluginCreateFunc = IPlugin* (*)();
using PluginDestroyFunc = void (*)(IPlugin*);
using PluginApiVersionFunc = const char* (*)();

} // namespace void_plugin_api
