#pragma once

/// @file plugin.hpp
/// @brief WASM-based plugin system

#include "wasm.hpp"

#include <void_engine/event/event.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_scripting {

// =============================================================================
// Plugin Metadata
// =============================================================================

/// @brief Plugin metadata
struct PluginMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string license;

    std::vector<std::string> dependencies;
    std::vector<std::string> tags;

    // API version requirements
    std::uint32_t api_version_min = 1;
    std::uint32_t api_version_max = 1;

    // Capabilities
    bool requires_network = false;
    bool requires_filesystem = false;
    bool requires_threads = false;
};

/// @brief Plugin state
enum class PluginState : std::uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Initializing,
    Active,
    Paused,
    Error,
    Unloading
};

// =============================================================================
// Plugin Interface
// =============================================================================

/// @brief A loaded WASM plugin
class Plugin {
public:
    Plugin(PluginId id, std::string name);
    ~Plugin();

    // Non-copyable, movable
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&&) noexcept;
    Plugin& operator=(Plugin&&) noexcept;

    // Identity
    [[nodiscard]] PluginId id() const { return id_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const PluginMetadata& metadata() const { return metadata_; }
    [[nodiscard]] PluginState state() const { return state_; }

    // Module access
    [[nodiscard]] WasmModule* module() const { return module_; }
    [[nodiscard]] WasmInstance* instance() const { return instance_; }

    // Lifecycle
    WasmResult<void> load(const std::filesystem::path& path);
    WasmResult<void> load_binary(std::span<const std::uint8_t> binary);
    WasmResult<void> initialize();
    WasmResult<void> shutdown();
    void unload();

    // Execution
    WasmResult<void> update(float delta_time);

    // Event handling
    WasmResult<void> on_event(const std::string& event_name, std::span<const WasmValue> args);

    // Function calls
    template <typename R, typename... Args>
    WasmResult<R> call(const std::string& function_name, Args... args);

    // Memory access
    [[nodiscard]] WasmMemory* memory() const;

    // Error info
    [[nodiscard]] const std::string& error_message() const { return error_message_; }

private:
    /// Parse metadata from custom section
    void parse_metadata(std::span<const std::uint8_t> data);

    PluginId id_;
    std::string name_;
    PluginMetadata metadata_;
    PluginState state_ = PluginState::Unloaded;

    WasmModule* module_ = nullptr;
    WasmInstance* instance_ = nullptr;

    std::string error_message_;
    std::filesystem::path source_path_;
};

// =============================================================================
// Plugin Host API
// =============================================================================

/// @brief Host API exposed to plugins
class HostApi {
public:
    HostApi();
    ~HostApi();

    // Singleton access
    [[nodiscard]] static HostApi& instance();

    // ==========================================================================
    // Registration
    // ==========================================================================

    /// @brief Register all host functions with the runtime
    void register_with(WasmRuntime& runtime);

    // ==========================================================================
    // Logging
    // ==========================================================================

    void log_info(const std::string& message);
    void log_warn(const std::string& message);
    void log_error(const std::string& message);
    void log_debug(const std::string& message);

    // ==========================================================================
    // Time
    // ==========================================================================

    [[nodiscard]] double get_time() const;
    [[nodiscard]] double get_delta_time() const;
    [[nodiscard]] std::uint64_t get_frame_count() const;

    void set_delta_time(double dt) { delta_time_ = dt; }
    void set_frame_count(std::uint64_t frame) { frame_count_ = frame; }

    // ==========================================================================
    // Random
    // ==========================================================================

    [[nodiscard]] std::uint32_t random_u32();
    [[nodiscard]] double random_f64();
    [[nodiscard]] double random_range(double min, double max);

    // ==========================================================================
    // Entity API
    // ==========================================================================

    std::uint64_t create_entity();
    void destroy_entity(std::uint64_t entity);
    bool entity_exists(std::uint64_t entity);

    // ==========================================================================
    // Component API
    // ==========================================================================

    void set_component(std::uint64_t entity, const std::string& component, WasmValue value);
    WasmValue get_component(std::uint64_t entity, const std::string& component);
    bool has_component(std::uint64_t entity, const std::string& component);
    void remove_component(std::uint64_t entity, const std::string& component);

    // ==========================================================================
    // Event API
    // ==========================================================================

    void emit_event(const std::string& event_name, std::span<const WasmValue> args);

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    using LogCallback = std::function<void(int level, const std::string& message)>;
    using EntityCallback = std::function<std::uint64_t()>;
    using EventCallback = std::function<void(const std::string&, std::span<const WasmValue>)>;

    void set_log_callback(LogCallback cb) { log_callback_ = std::move(cb); }
    void set_create_entity_callback(EntityCallback cb) { create_entity_callback_ = std::move(cb); }
    void set_event_callback(EventCallback cb) { event_callback_ = std::move(cb); }

private:
    double delta_time_ = 0.0;
    std::uint64_t frame_count_ = 0;
    std::chrono::steady_clock::time_point start_time_;

    LogCallback log_callback_;
    EntityCallback create_entity_callback_;
    EventCallback event_callback_;
};

// =============================================================================
// Plugin Registry
// =============================================================================

/// @brief Registry of loaded plugins
class PluginRegistry {
public:
    PluginRegistry();
    ~PluginRegistry();

    // Singleton access
    [[nodiscard]] static PluginRegistry& instance();

    // ==========================================================================
    // Plugin Management
    // ==========================================================================

    /// @brief Load a plugin from file
    WasmResult<Plugin*> load_plugin(const std::filesystem::path& path);

    /// @brief Load a plugin from binary
    WasmResult<Plugin*> load_plugin(const std::string& name, std::span<const std::uint8_t> binary);

    /// @brief Unload a plugin
    bool unload_plugin(PluginId id);

    /// @brief Get a plugin
    [[nodiscard]] Plugin* get_plugin(PluginId id);
    [[nodiscard]] const Plugin* get_plugin(PluginId id) const;

    /// @brief Find a plugin by name
    [[nodiscard]] Plugin* find_plugin(const std::string& name);

    /// @brief Get all plugins
    [[nodiscard]] std::vector<Plugin*> plugins();

    /// @brief Get plugins by state
    [[nodiscard]] std::vector<Plugin*> plugins_by_state(PluginState state);

    // ==========================================================================
    // Lifecycle
    // ==========================================================================

    /// @brief Initialize all loaded plugins
    void initialize_all();

    /// @brief Shutdown all plugins
    void shutdown_all();

    /// @brief Update all active plugins
    void update_all(float delta_time);

    // ==========================================================================
    // Events
    // ==========================================================================

    /// @brief Broadcast an event to all plugins
    void broadcast_event(const std::string& event_name, std::span<const WasmValue> args = {});

    // ==========================================================================
    // Hot Reload
    // ==========================================================================

    /// @brief Enable hot reload watching
    void enable_hot_reload(bool enabled);

    /// @brief Check for changes and reload
    void check_hot_reload();

    /// @brief Hot reload a specific plugin
    WasmResult<void> hot_reload(PluginId id);

    // ==========================================================================
    // Dependencies
    // ==========================================================================

    /// @brief Resolve dependencies for a plugin
    WasmResult<void> resolve_dependencies(PluginId id);

    /// @brief Get load order based on dependencies
    [[nodiscard]] std::vector<PluginId> get_load_order() const;

private:
    std::unordered_map<PluginId, std::unique_ptr<Plugin>> plugins_;
    std::unordered_map<std::string, PluginId> plugin_names_;

    bool hot_reload_enabled_ = false;
    std::unordered_map<PluginId, std::filesystem::file_time_type> file_timestamps_;

    inline static std::uint32_t next_plugin_id_ = 1;
};

// =============================================================================
// Template Implementations
// =============================================================================

template <typename R, typename... Args>
WasmResult<R> Plugin::call(const std::string& function_name, Args... args) {
    if (!instance_) {
        return WasmResult<R>::error(WasmError::InvalidModule);
    }
    return instance_->call_typed<R>(function_name, args...);
}

} // namespace void_scripting
