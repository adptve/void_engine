/// @file plugin_watcher.hpp
/// @brief File system watcher for automatic plugin hot-reload
///
/// Monitors plugin directories for changes and triggers hot-reload automatically.
/// This is what makes plugins TRULY hot-swappable at runtime.

#pragma once

#include "fwd.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_plugin_api {

// =============================================================================
// Plugin File Info
// =============================================================================

/// @brief Information about a plugin file
struct PluginFileInfo {
    std::filesystem::path path;
    std::filesystem::path source_path;  // For recompilation
    std::string name;
    std::filesystem::file_time_type last_modified;
    std::size_t file_size{0};
    bool loaded{false};
    bool pending_reload{false};
    std::chrono::steady_clock::time_point change_detected;
};

// =============================================================================
// Plugin Watcher Configuration
// =============================================================================

// =============================================================================
// Platform Detection
// =============================================================================

/// @brief Runtime platform detection
enum class Platform : std::uint8_t {
    Windows,
    Linux,
    MacOS,
    Unknown
};

/// @brief Get current runtime platform
constexpr Platform current_platform() {
#if defined(_WIN32) || defined(_WIN64)
    return Platform::Windows;
#elif defined(__APPLE__) && defined(__MACH__)
    return Platform::MacOS;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

/// @brief Get native shared library extension for current platform
constexpr const char* native_plugin_extension() {
    switch (current_platform()) {
        case Platform::Windows: return ".dll";
        case Platform::MacOS:   return ".dylib";
        case Platform::Linux:   return ".so";
        default:                return ".so";
    }
}

/// @brief Get native shared library prefix for current platform
constexpr const char* native_plugin_prefix() {
    switch (current_platform()) {
        case Platform::Windows: return "";
        case Platform::MacOS:   return "lib";
        case Platform::Linux:   return "lib";
        default:                return "lib";
    }
}

/// @brief Get all valid plugin extensions (for cross-compiled plugins)
inline std::vector<std::string> all_plugin_extensions() {
    return {".dll", ".so", ".dylib"};
}

/// @brief Configuration for the plugin watcher
struct PluginWatcherConfig {
    /// Directories to watch for plugins
    std::vector<std::filesystem::path> watch_paths;

    /// File extensions to consider as plugins (defaults to current platform)
    std::vector<std::string> plugin_extensions{native_plugin_extension()};

    /// Whether to accept plugins from other platforms (for development/testing)
    bool accept_cross_platform{false};

    /// File extensions for source files (triggers recompile)
    std::vector<std::string> source_extensions{".cpp", ".hpp", ".h", ".cc", ".cxx", ".hxx"};

    /// How often to poll for changes (milliseconds)
    std::chrono::milliseconds poll_interval{100};

    /// Debounce time before triggering reload (prevents multiple reloads)
    std::chrono::milliseconds debounce_time{500};

    /// Whether to automatically load new plugins found
    bool auto_load_new{true};

    /// Whether to automatically reload changed plugins
    bool auto_reload_changed{true};

    /// Whether to watch source files and trigger recompile
    bool watch_sources{true};

    /// Build command template (use {plugin} for plugin name, {source} for source path)
    std::string build_command;

    /// Maximum concurrent recompilations
    std::uint32_t max_concurrent_builds{2};
};

// =============================================================================
// Plugin Watcher Events
// =============================================================================

/// @brief Type of plugin event
enum class PluginEventType : std::uint8_t {
    Discovered,      // New plugin file found
    Modified,        // Plugin file changed
    Removed,         // Plugin file deleted
    LoadStarted,     // Plugin load beginning
    LoadSucceeded,   // Plugin loaded successfully
    LoadFailed,      // Plugin load failed
    UnloadStarted,   // Plugin unload beginning
    UnloadSucceeded, // Plugin unloaded
    ReloadStarted,   // Hot-reload beginning
    ReloadSucceeded, // Hot-reload completed
    ReloadFailed,    // Hot-reload failed
    BuildStarted,    // Recompilation starting
    BuildSucceeded,  // Recompilation completed
    BuildFailed      // Recompilation failed
};

/// @brief Plugin event data
struct PluginEvent {
    PluginEventType type;
    std::string plugin_name;
    std::filesystem::path plugin_path;
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
};

/// @brief Callback for plugin events
using PluginEventCallback = std::function<void(const PluginEvent&)>;

// =============================================================================
// Plugin Watcher Interface
// =============================================================================

/// @brief Interface for plugin loading operations (used by PluginWatcher)
class IPluginLoader {
public:
    virtual ~IPluginLoader() = default;

    /// @brief Load a plugin by path (for watcher)
    virtual bool watcher_load_plugin(const std::filesystem::path& path) = 0;

    /// @brief Unload a plugin by name (for watcher)
    virtual bool watcher_unload_plugin(const std::string& name) = 0;

    /// @brief Hot-reload a plugin (preserves state, for watcher)
    virtual bool watcher_hot_reload_plugin(const std::string& name, const std::filesystem::path& new_path) = 0;

    /// @brief Check if a plugin is loaded (for watcher)
    virtual bool watcher_is_plugin_loaded(const std::string& name) const = 0;

    /// @brief Get list of loaded plugin names (for watcher)
    virtual std::vector<std::string> watcher_loaded_plugins() const = 0;
};

// =============================================================================
// Plugin Watcher
// =============================================================================

/// @brief Watches filesystem for plugin changes and triggers hot-reload
class PluginWatcher {
public:
    explicit PluginWatcher(IPluginLoader& loader);
    PluginWatcher(IPluginLoader& loader, const PluginWatcherConfig& config);
    ~PluginWatcher();

    // Non-copyable, non-movable (owns thread)
    PluginWatcher(const PluginWatcher&) = delete;
    PluginWatcher& operator=(const PluginWatcher&) = delete;
    PluginWatcher(PluginWatcher&&) = delete;
    PluginWatcher& operator=(PluginWatcher&&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// @brief Start watching for plugin changes
    void start();

    /// @brief Stop watching
    void stop();

    /// @brief Check if watcher is running
    bool is_running() const { return m_running.load(); }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /// @brief Add a directory to watch
    void add_watch_path(const std::filesystem::path& path);

    /// @brief Remove a directory from watch
    void remove_watch_path(const std::filesystem::path& path);

    /// @brief Set the build command template
    void set_build_command(const std::string& command);

    /// @brief Update configuration (takes effect on next poll)
    void set_config(const PluginWatcherConfig& config);

    /// @brief Get current configuration
    const PluginWatcherConfig& config() const { return m_config; }

    // -------------------------------------------------------------------------
    // Manual Operations
    // -------------------------------------------------------------------------

    /// @brief Force scan of all watch directories
    void scan_now();

    /// @brief Force reload of a specific plugin
    bool reload_plugin(const std::string& name);

    /// @brief Force rebuild and reload of a plugin
    bool rebuild_plugin(const std::string& name);

    /// @brief Get info about a tracked plugin
    std::optional<PluginFileInfo> get_plugin_info(const std::string& name) const;

    /// @brief Get all tracked plugins
    std::vector<PluginFileInfo> all_plugins() const;

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    /// @brief Subscribe to plugin events
    void on_event(PluginEventCallback callback);

    /// @brief Get recent events (last N)
    std::vector<PluginEvent> recent_events(std::size_t count = 50) const;

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    struct Stats {
        std::uint64_t plugins_discovered{0};
        std::uint64_t plugins_loaded{0};
        std::uint64_t plugins_unloaded{0};
        std::uint64_t hot_reloads{0};
        std::uint64_t hot_reload_failures{0};
        std::uint64_t builds_triggered{0};
        std::uint64_t build_failures{0};
        std::chrono::steady_clock::time_point last_scan;
        std::chrono::milliseconds average_reload_time{0};
    };

    Stats stats() const;

private:
    // -------------------------------------------------------------------------
    // Internal Methods
    // -------------------------------------------------------------------------

    void watch_thread();
    void scan_directories();
    void process_pending_reloads();
    void check_file_changes();
    bool trigger_build(const std::string& plugin_name);
    void emit_event(PluginEventType type, const std::string& name,
                    const std::filesystem::path& path, const std::string& message = "");
    std::string extract_plugin_name(const std::filesystem::path& path) const;
    bool is_plugin_file(const std::filesystem::path& path) const;
    bool is_source_file(const std::filesystem::path& path) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    IPluginLoader& m_loader;
    PluginWatcherConfig m_config;

    std::thread m_watch_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_scan_requested{false};

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, PluginFileInfo> m_plugins;
    std::unordered_map<std::filesystem::path, std::string> m_source_to_plugin;

    mutable std::mutex m_event_mutex;
    std::vector<PluginEventCallback> m_event_callbacks;
    std::vector<PluginEvent> m_event_history;
    static constexpr std::size_t MAX_EVENT_HISTORY = 1000;

    Stats m_stats;
    mutable std::mutex m_stats_mutex;
};

// =============================================================================
// Plugin State Registry (for custom plugin state)
// =============================================================================

/// @brief Interface for plugin-defined state that survives hot-reload
class IPluginState {
public:
    virtual ~IPluginState() = default;

    /// @brief Unique type identifier for this state
    virtual std::string type_id() const = 0;

    /// @brief Serialize state to binary
    virtual std::vector<std::uint8_t> serialize() const = 0;

    /// @brief Deserialize state from binary
    virtual void deserialize(const std::vector<std::uint8_t>& data) = 0;

    /// @brief Clear all state
    virtual void clear() = 0;

    /// @brief Clone this state (for snapshots)
    virtual std::unique_ptr<IPluginState> clone() const = 0;
};

/// @brief Registry for plugin-defined state types
class PluginStateRegistry {
public:
    /// @brief Register a custom state type
    template<typename T>
    void register_state(const std::string& plugin_name) {
        static_assert(std::is_base_of_v<IPluginState, T>,
                      "State must derive from IPluginState");
        std::lock_guard lock(m_mutex);
        auto& states = m_plugin_states[plugin_name];
        auto state = std::make_unique<T>();
        states[state->type_id()] = std::move(state);
    }

    /// @brief Get a custom state by type
    template<typename T>
    T* get_state(const std::string& plugin_name) {
        std::lock_guard lock(m_mutex);
        auto plugin_it = m_plugin_states.find(plugin_name);
        if (plugin_it == m_plugin_states.end()) return nullptr;

        for (auto& [id, state] : plugin_it->second) {
            if (auto* typed = dynamic_cast<T*>(state.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    /// @brief Get state by type ID
    IPluginState* get_state(const std::string& plugin_name, const std::string& type_id);

    /// @brief Snapshot all state for a plugin (before hot-reload)
    std::unordered_map<std::string, std::vector<std::uint8_t>>
    snapshot_plugin(const std::string& plugin_name);

    /// @brief Restore all state for a plugin (after hot-reload)
    void restore_plugin(const std::string& plugin_name,
                       const std::unordered_map<std::string, std::vector<std::uint8_t>>& data);

    /// @brief Clear all state for a plugin
    void clear_plugin(const std::string& plugin_name);

    /// @brief Unregister all state for a plugin
    void unregister_plugin(const std::string& plugin_name);

    /// @brief Get all registered plugins
    std::vector<std::string> registered_plugins() const;

    /// @brief Get state types for a plugin
    std::vector<std::string> state_types(const std::string& plugin_name) const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string,
                       std::unordered_map<std::string, std::unique_ptr<IPluginState>>> m_plugin_states;
};

} // namespace void_plugin_api
