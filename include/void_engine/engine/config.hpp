/// @file config.hpp
/// @brief Configuration system for void_engine
///
/// Provides layered configuration with:
/// - Default values
/// - Configuration file loading (JSON, TOML)
/// - Command-line argument parsing
/// - Runtime modification
/// - Hot-reload support

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <any>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace void_engine {

// =============================================================================
// Config Layer
// =============================================================================

/// Configuration layer priority (lower = higher priority)
enum class ConfigLayerPriority : std::int32_t {
    CommandLine = -1000,    ///< Command-line arguments (highest)
    Environment = -500,     ///< Environment variables
    User = 0,               ///< User configuration file
    Project = 100,          ///< Project configuration file
    System = 500,           ///< System defaults
    Default = 1000,         ///< Built-in defaults (lowest)
};

/// A configuration layer
class ConfigLayer {
public:
    explicit ConfigLayer(const std::string& name, ConfigLayerPriority priority = ConfigLayerPriority::User)
        : m_name(name), m_priority(priority) {}

    /// Get layer name
    [[nodiscard]] const std::string& name() const { return m_name; }

    /// Get priority
    [[nodiscard]] ConfigLayerPriority priority() const { return m_priority; }

    /// Check if key exists
    [[nodiscard]] bool contains(const std::string& key) const;

    /// Get value
    [[nodiscard]] std::optional<ConfigValue> get(const std::string& key) const;

    /// Set value
    void set(const std::string& key, ConfigValue value);

    /// Remove key
    bool remove(const std::string& key);

    /// Clear all values
    void clear();

    /// Get all keys
    [[nodiscard]] std::vector<std::string> keys() const;

    /// Get value count
    [[nodiscard]] std::size_t size() const { return m_values.size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const { return m_values.empty(); }

    /// Mark as modified
    void mark_modified() { m_modified = true; }

    /// Check if modified
    [[nodiscard]] bool is_modified() const { return m_modified; }

    /// Clear modified flag
    void clear_modified() { m_modified = false; }

private:
    std::string m_name;
    ConfigLayerPriority m_priority;
    std::map<std::string, ConfigValue> m_values;
    bool m_modified = false;
};

// =============================================================================
// Config Manager
// =============================================================================

/// Layered configuration manager
class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    // Non-copyable
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // =========================================================================
    // Layer Management
    // =========================================================================

    /// Add a configuration layer
    void add_layer(std::unique_ptr<ConfigLayer> layer);

    /// Get layer by name
    [[nodiscard]] ConfigLayer* get_layer(const std::string& name);
    [[nodiscard]] const ConfigLayer* get_layer(const std::string& name) const;

    /// Remove layer
    bool remove_layer(const std::string& name);

    /// Get layer count
    [[nodiscard]] std::size_t layer_count() const { return m_layers.size(); }

    // =========================================================================
    // Value Access (Merged View)
    // =========================================================================

    /// Check if key exists in any layer
    [[nodiscard]] bool contains(const std::string& key) const;

    /// Get value (from highest priority layer that contains it)
    [[nodiscard]] std::optional<ConfigValue> get(const std::string& key) const;

    /// Get value with default
    template<typename T>
    [[nodiscard]] T get_or(const std::string& key, T default_value) const {
        auto value = get(key);
        if (!value) return default_value;

        if constexpr (std::is_same_v<T, bool>) {
            if (auto* v = std::get_if<bool>(&*value)) return *v;
        } else if constexpr (std::is_integral_v<T>) {
            if (auto* v = std::get_if<std::int64_t>(&*value)) return static_cast<T>(*v);
        } else if constexpr (std::is_floating_point_v<T>) {
            if (auto* v = std::get_if<double>(&*value)) return static_cast<T>(*v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (auto* v = std::get_if<std::string>(&*value)) return *v;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            if (auto* v = std::get_if<std::vector<std::string>>(&*value)) return *v;
        }

        return default_value;
    }

    /// Get bool value
    [[nodiscard]] bool get_bool(const std::string& key, bool default_value = false) const;

    /// Get int value
    [[nodiscard]] std::int64_t get_int(const std::string& key, std::int64_t default_value = 0) const;

    /// Get float value
    [[nodiscard]] double get_float(const std::string& key, double default_value = 0.0) const;

    /// Get string value
    [[nodiscard]] std::string get_string(const std::string& key, const std::string& default_value = "") const;

    /// Get string array value
    [[nodiscard]] std::vector<std::string> get_string_array(
        const std::string& key,
        const std::vector<std::string>& default_value = {}) const;

    // =========================================================================
    // Value Setting
    // =========================================================================

    /// Set value in specific layer (or user layer by default)
    void set(const std::string& key, ConfigValue value, const std::string& layer_name = "user");

    /// Set bool value
    void set_bool(const std::string& key, bool value, const std::string& layer_name = "user");

    /// Set int value
    void set_int(const std::string& key, std::int64_t value, const std::string& layer_name = "user");

    /// Set float value
    void set_float(const std::string& key, double value, const std::string& layer_name = "user");

    /// Set string value
    void set_string(const std::string& key, const std::string& value, const std::string& layer_name = "user");

    // =========================================================================
    // File Operations
    // =========================================================================

    /// Load configuration from JSON file
    void_core::Result<void> load_json(const std::filesystem::path& path, const std::string& layer_name);

    /// Save layer to JSON file
    void_core::Result<void> save_json(const std::filesystem::path& path, const std::string& layer_name) const;

    /// Load configuration from TOML file
    void_core::Result<void> load_toml(const std::filesystem::path& path, const std::string& layer_name);

    /// Save layer to TOML file
    void_core::Result<void> save_toml(const std::filesystem::path& path, const std::string& layer_name) const;

    // =========================================================================
    // Command Line
    // =========================================================================

    /// Parse command-line arguments
    void_core::Result<void> parse_args(int argc, char** argv);

    /// Parse command-line arguments from vector
    void_core::Result<void> parse_args(const std::vector<std::string>& args);

    // =========================================================================
    // Environment
    // =========================================================================

    /// Load environment variables with prefix
    void load_environment(const std::string& prefix = "VOID_");

    // =========================================================================
    // Events
    // =========================================================================

    /// Set callback for config changes
    void on_change(std::function<void(const std::string& key, const ConfigValue& value)> callback);

    // =========================================================================
    // Defaults
    // =========================================================================

    /// Set up default configuration
    void setup_defaults();

    /// Create default layers (defaults, project, user, env, cmdline)
    void create_default_layers();

    // =========================================================================
    // Engine Config Conversion
    // =========================================================================

    /// Build EngineConfig from current values
    [[nodiscard]] EngineConfig build_engine_config() const;

    /// Apply EngineConfig values to a layer
    void apply_engine_config(const EngineConfig& config, const std::string& layer_name = "user");

private:
    /// Get layers sorted by priority (highest to lowest)
    [[nodiscard]] std::vector<ConfigLayer*> sorted_layers() const;

    /// Notify change callbacks
    void notify_change(const std::string& key, const ConfigValue& value);

private:
    std::vector<std::unique_ptr<ConfigLayer>> m_layers;
    mutable std::mutex m_mutex;
    std::vector<std::function<void(const std::string&, const ConfigValue&)>> m_change_callbacks;
};

// =============================================================================
// Config Watcher (Hot-Reload)
// =============================================================================

/// Watches configuration files for changes
class ConfigWatcher : public void_core::HotReloadable {
public:
    ConfigWatcher(ConfigManager& manager, const std::string& layer_name, std::filesystem::path path)
        : m_manager(manager)
        , m_layer_name(layer_name)
        , m_path(std::move(path)) {}

    // HotReloadable implementation
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "ConfigWatcher"; }

    /// Reload configuration from file
    void_core::Result<void> reload();

    /// Get watched path
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    /// Get layer name
    [[nodiscard]] const std::string& layer_name() const { return m_layer_name; }

private:
    ConfigManager& m_manager;
    std::string m_layer_name;
    std::filesystem::path m_path;
};

// =============================================================================
// Config Schema
// =============================================================================

/// Configuration value schema for validation
struct ConfigSchema {
    std::string key;
    ConfigValueType type;
    std::optional<ConfigValue> default_value;
    std::optional<ConfigValue> min_value;
    std::optional<ConfigValue> max_value;
    std::vector<ConfigValue> allowed_values;
    std::string description;
    bool required = false;

    /// Validate a value against this schema
    [[nodiscard]] void_core::Result<void> validate(const ConfigValue& value) const;
};

/// Schema registry for validation
class ConfigSchemaRegistry {
public:
    /// Register a schema
    void register_schema(ConfigSchema schema);

    /// Get schema for key
    [[nodiscard]] const ConfigSchema* get_schema(const std::string& key) const;

    /// Validate all values in a layer
    [[nodiscard]] std::vector<std::string> validate(const ConfigLayer& layer) const;

    /// Validate a specific value
    [[nodiscard]] void_core::Result<void> validate(const std::string& key, const ConfigValue& value) const;

private:
    std::map<std::string, ConfigSchema> m_schemas;
};

// =============================================================================
// Config Keys (Constants)
// =============================================================================

namespace config_keys {

// Window
constexpr const char* WINDOW_TITLE = "window.title";
constexpr const char* WINDOW_WIDTH = "window.width";
constexpr const char* WINDOW_HEIGHT = "window.height";
constexpr const char* WINDOW_MODE = "window.mode";
constexpr const char* WINDOW_VSYNC = "window.vsync";
constexpr const char* WINDOW_RESIZABLE = "window.resizable";

// Render
constexpr const char* RENDER_BACKEND = "render.backend";
constexpr const char* RENDER_ANTI_ALIASING = "render.anti_aliasing";
constexpr const char* RENDER_MAX_FPS = "render.max_fps";
constexpr const char* RENDER_SCALE = "render.scale";
constexpr const char* RENDER_HDR = "render.hdr";
constexpr const char* RENDER_SHADOWS = "render.shadows";
constexpr const char* RENDER_BLOOM = "render.bloom";

// Audio
constexpr const char* AUDIO_MASTER_VOLUME = "audio.master_volume";
constexpr const char* AUDIO_MUSIC_VOLUME = "audio.music_volume";
constexpr const char* AUDIO_SFX_VOLUME = "audio.sfx_volume";
constexpr const char* AUDIO_SAMPLE_RATE = "audio.sample_rate";

// Input
constexpr const char* INPUT_MOUSE_SENSITIVITY = "input.mouse_sensitivity";
constexpr const char* INPUT_INVERT_Y = "input.invert_y";
constexpr const char* INPUT_GAMEPAD_DEADZONE = "input.gamepad_deadzone";

// Engine
constexpr const char* ENGINE_TARGET_FPS = "engine.target_fps";
constexpr const char* ENGINE_FIXED_UPDATE_RATE = "engine.fixed_update_rate";
constexpr const char* ENGINE_WORKER_THREADS = "engine.worker_threads";
constexpr const char* ENGINE_DEBUG = "engine.debug";
constexpr const char* ENGINE_PROFILING = "engine.profiling";
constexpr const char* ENGINE_VALIDATION = "engine.validation";

// Paths
constexpr const char* PATH_ASSETS = "paths.assets";
constexpr const char* PATH_SHADERS = "paths.shaders";
constexpr const char* PATH_SAVES = "paths.saves";
constexpr const char* PATH_LOGS = "paths.logs";
constexpr const char* PATH_CONFIG = "paths.config";

// Hot-reload
constexpr const char* HOT_RELOAD_ENABLED = "hot_reload.enabled";
constexpr const char* HOT_RELOAD_POLL_INTERVAL = "hot_reload.poll_interval_ms";

} // namespace config_keys

} // namespace void_engine
