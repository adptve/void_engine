/// @file config.cpp
/// @brief Configuration system implementation for void_engine

#include <void_engine/engine/config.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace void_engine {

// =============================================================================
// ConfigLayer
// =============================================================================

bool ConfigLayer::contains(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

std::optional<ConfigValue> ConfigLayer::get(const std::string& key) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ConfigLayer::set(const std::string& key, ConfigValue value) {
    m_values[key] = std::move(value);
    m_modified = true;
}

bool ConfigLayer::remove(const std::string& key) {
    bool removed = m_values.erase(key) > 0;
    if (removed) {
        m_modified = true;
    }
    return removed;
}

void ConfigLayer::clear() {
    m_values.clear();
    m_modified = true;
}

std::vector<std::string> ConfigLayer::keys() const {
    std::vector<std::string> result;
    result.reserve(m_values.size());
    for (const auto& [key, _] : m_values) {
        result.push_back(key);
    }
    return result;
}

// =============================================================================
// ConfigManager
// =============================================================================

void ConfigManager::add_layer(std::unique_ptr<ConfigLayer> layer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_layers.push_back(std::move(layer));
}

ConfigLayer* ConfigManager::get_layer(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& layer : m_layers) {
        if (layer->name() == name) {
            return layer.get();
        }
    }
    return nullptr;
}

const ConfigLayer* ConfigManager::get_layer(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& layer : m_layers) {
        if (layer->name() == name) {
            return layer.get();
        }
    }
    return nullptr;
}

bool ConfigManager::remove_layer(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_layers.begin(), m_layers.end(),
        [&name](const std::unique_ptr<ConfigLayer>& layer) {
            return layer->name() == name;
        });
    if (it != m_layers.end()) {
        m_layers.erase(it, m_layers.end());
        return true;
    }
    return false;
}

bool ConfigManager::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& layer : m_layers) {
        if (layer->contains(key)) {
            return true;
        }
    }
    return false;
}

std::optional<ConfigValue> ConfigManager::get(const std::string& key) const {
    auto layers = sorted_layers();
    for (auto* layer : layers) {
        auto value = layer->get(key);
        if (value) {
            return value;
        }
    }
    return std::nullopt;
}

bool ConfigManager::get_bool(const std::string& key, bool default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    if (auto* v = std::get_if<bool>(&*value)) {
        return *v;
    }
    // Try to parse from string
    if (auto* v = std::get_if<std::string>(&*value)) {
        return *v == "true" || *v == "1" || *v == "yes";
    }
    // Try from int
    if (auto* v = std::get_if<std::int64_t>(&*value)) {
        return *v != 0;
    }

    return default_value;
}

std::int64_t ConfigManager::get_int(const std::string& key, std::int64_t default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    if (auto* v = std::get_if<std::int64_t>(&*value)) {
        return *v;
    }
    // Try to parse from string
    if (auto* v = std::get_if<std::string>(&*value)) {
        try {
            return std::stoll(*v);
        } catch (...) {
            return default_value;
        }
    }
    // Try from double
    if (auto* v = std::get_if<double>(&*value)) {
        return static_cast<std::int64_t>(*v);
    }
    // Try from bool
    if (auto* v = std::get_if<bool>(&*value)) {
        return *v ? 1 : 0;
    }

    return default_value;
}

double ConfigManager::get_float(const std::string& key, double default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    if (auto* v = std::get_if<double>(&*value)) {
        return *v;
    }
    // Try from int
    if (auto* v = std::get_if<std::int64_t>(&*value)) {
        return static_cast<double>(*v);
    }
    // Try to parse from string
    if (auto* v = std::get_if<std::string>(&*value)) {
        try {
            return std::stod(*v);
        } catch (...) {
            return default_value;
        }
    }

    return default_value;
}

std::string ConfigManager::get_string(const std::string& key, const std::string& default_value) const {
    auto value = get(key);
    if (!value) return default_value;

    if (auto* v = std::get_if<std::string>(&*value)) {
        return *v;
    }
    // Convert from other types
    if (auto* v = std::get_if<bool>(&*value)) {
        return *v ? "true" : "false";
    }
    if (auto* v = std::get_if<std::int64_t>(&*value)) {
        return std::to_string(*v);
    }
    if (auto* v = std::get_if<double>(&*value)) {
        return std::to_string(*v);
    }

    return default_value;
}

std::vector<std::string> ConfigManager::get_string_array(
    const std::string& key,
    const std::vector<std::string>& default_value) const
{
    auto value = get(key);
    if (!value) return default_value;

    if (auto* v = std::get_if<std::vector<std::string>>(&*value)) {
        return *v;
    }

    return default_value;
}

void ConfigManager::set(const std::string& key, ConfigValue value, const std::string& layer_name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& layer : m_layers) {
        if (layer->name() == layer_name) {
            layer->set(key, std::move(value));
            notify_change(key, layer->get(key).value_or(ConfigValue{}));
            return;
        }
    }

    // Layer doesn't exist, create it
    auto layer = std::make_unique<ConfigLayer>(layer_name);
    layer->set(key, std::move(value));
    notify_change(key, layer->get(key).value_or(ConfigValue{}));
    m_layers.push_back(std::move(layer));
}

void ConfigManager::set_bool(const std::string& key, bool value, const std::string& layer_name) {
    set(key, ConfigValue{value}, layer_name);
}

void ConfigManager::set_int(const std::string& key, std::int64_t value, const std::string& layer_name) {
    set(key, ConfigValue{value}, layer_name);
}

void ConfigManager::set_float(const std::string& key, double value, const std::string& layer_name) {
    set(key, ConfigValue{value}, layer_name);
}

void ConfigManager::set_string(const std::string& key, const std::string& value, const std::string& layer_name) {
    set(key, ConfigValue{value}, layer_name);
}

void_core::Result<void> ConfigManager::load_json(
    const std::filesystem::path& path,
    const std::string& layer_name)
{
    // Simple JSON parser - in production, use a real JSON library
    std::ifstream file(path);
    if (!file) {
        return void_core::Error{"Failed to open file: " + path.string()};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Find or create layer
    ConfigLayer* layer = get_layer(layer_name);
    if (!layer) {
        auto new_layer = std::make_unique<ConfigLayer>(layer_name);
        layer = new_layer.get();
        add_layer(std::move(new_layer));
    }

    // Basic JSON parsing (simplified - would use nlohmann/json in production)
    // For now, just mark as successful
    // In a real implementation, this would parse the JSON and populate the layer

    return void_core::Ok();
}

void_core::Result<void> ConfigManager::save_json(
    const std::filesystem::path& path,
    const std::string& layer_name) const
{
    const ConfigLayer* layer = get_layer(layer_name);
    if (!layer) {
        return void_core::Error{"Layer not found: " + layer_name};
    }

    std::ofstream file(path);
    if (!file) {
        return void_core::Error{"Failed to create file: " + path.string()};
    }

    // Write simple JSON
    file << "{\n";
    auto keys = layer->keys();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto& key = keys[i];
        auto value = layer->get(key);
        if (!value) continue;

        file << "  \"" << key << "\": ";

        std::visit([&file](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                file << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                file << arg;
            } else if constexpr (std::is_same_v<T, double>) {
                file << arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                file << "\"" << arg << "\"";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                file << "[";
                for (std::size_t j = 0; j < arg.size(); ++j) {
                    file << "\"" << arg[j] << "\"";
                    if (j < arg.size() - 1) file << ", ";
                }
                file << "]";
            }
        }, *value);

        if (i < keys.size() - 1) file << ",";
        file << "\n";
    }
    file << "}\n";

    return void_core::Ok();
}

void_core::Result<void> ConfigManager::load_toml(
    const std::filesystem::path& path,
    const std::string& layer_name)
{
    // TOML parsing would require a TOML library
    // For now, just return success (stub)
    (void)path;
    (void)layer_name;
    return void_core::Ok();
}

void_core::Result<void> ConfigManager::save_toml(
    const std::filesystem::path& path,
    const std::string& layer_name) const
{
    // TOML writing would require a TOML library
    // For now, just return success (stub)
    (void)path;
    (void)layer_name;
    return void_core::Ok();
}

void_core::Result<void> ConfigManager::parse_args(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return parse_args(args);
}

void_core::Result<void> ConfigManager::parse_args(const std::vector<std::string>& args) {
    // Find or create command-line layer
    ConfigLayer* layer = get_layer("cmdline");
    if (!layer) {
        auto new_layer = std::make_unique<ConfigLayer>("cmdline", ConfigLayerPriority::CommandLine);
        layer = new_layer.get();
        add_layer(std::move(new_layer));
    }

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        // Handle --key=value or --key value
        if (arg.starts_with("--")) {
            std::string key_value = arg.substr(2);
            auto eq_pos = key_value.find('=');

            std::string key;
            std::string value;

            if (eq_pos != std::string::npos) {
                // --key=value format
                key = key_value.substr(0, eq_pos);
                value = key_value.substr(eq_pos + 1);
            } else if (i + 1 < args.size() && !args[i + 1].starts_with("-")) {
                // --key value format
                key = key_value;
                value = args[++i];
            } else {
                // --flag format (boolean true)
                key = key_value;
                value = "true";
            }

            // Convert - to . for config keys (e.g., --window-width -> window.width)
            std::replace(key.begin(), key.end(), '-', '.');

            // Try to determine type
            if (value == "true" || value == "false") {
                layer->set(key, ConfigValue{value == "true"});
            } else {
                // Try int
                try {
                    std::size_t pos;
                    std::int64_t int_val = std::stoll(value, &pos);
                    if (pos == value.size()) {
                        layer->set(key, ConfigValue{int_val});
                        continue;
                    }
                } catch (...) {}

                // Try float
                try {
                    std::size_t pos;
                    double float_val = std::stod(value, &pos);
                    if (pos == value.size()) {
                        layer->set(key, ConfigValue{float_val});
                        continue;
                    }
                } catch (...) {}

                // Default to string
                layer->set(key, ConfigValue{value});
            }
        }
        // Handle -k value (short options)
        else if (arg.starts_with("-") && arg.size() == 2) {
            // Short options not implemented for simplicity
        }
    }

    return void_core::Ok();
}

void ConfigManager::load_environment(const std::string& prefix) {
    // Find or create environment layer
    ConfigLayer* layer = get_layer("environment");
    if (!layer) {
        auto new_layer = std::make_unique<ConfigLayer>("environment", ConfigLayerPriority::Environment);
        layer = new_layer.get();
        add_layer(std::move(new_layer));
    }

    // Note: Getting all environment variables is platform-specific
    // This is a simplified version that would need to be extended
    // In production, use platform-specific APIs or a library

    // Common environment variables to check
    std::vector<std::pair<std::string, std::string>> env_mappings = {
        {"VOID_DEBUG", "engine.debug"},
        {"VOID_PROFILING", "engine.profiling"},
        {"VOID_TARGET_FPS", "engine.target_fps"},
        {"VOID_ASSET_PATH", "paths.assets"},
        {"VOID_LOG_PATH", "paths.logs"},
    };

    for (const auto& [env_name, config_key] : env_mappings) {
        std::string full_env_name = env_name;
        if (!prefix.empty() && env_name.find(prefix) != 0) {
            continue;  // Doesn't match prefix
        }

#ifdef _WIN32
        char* value = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&value, &size, env_name.c_str()) == 0 && value != nullptr) {
            layer->set(config_key, ConfigValue{std::string(value)});
            free(value);
        }
#else
        const char* value = std::getenv(env_name.c_str());
        if (value) {
            layer->set(config_key, ConfigValue{std::string(value)});
        }
#endif
    }
}

void ConfigManager::on_change(std::function<void(const std::string& key, const ConfigValue& value)> callback) {
    m_change_callbacks.push_back(std::move(callback));
}

void ConfigManager::setup_defaults() {
    create_default_layers();

    auto* defaults = get_layer("defaults");
    if (!defaults) return;

    // Window defaults
    defaults->set(config_keys::WINDOW_TITLE, ConfigValue{std::string("void_engine")});
    defaults->set(config_keys::WINDOW_WIDTH, ConfigValue{std::int64_t(1280)});
    defaults->set(config_keys::WINDOW_HEIGHT, ConfigValue{std::int64_t(720)});
    defaults->set(config_keys::WINDOW_VSYNC, ConfigValue{true});
    defaults->set(config_keys::WINDOW_RESIZABLE, ConfigValue{true});

    // Render defaults
    defaults->set(config_keys::RENDER_MAX_FPS, ConfigValue{std::int64_t(0)});
    defaults->set(config_keys::RENDER_SCALE, ConfigValue{1.0});
    defaults->set(config_keys::RENDER_HDR, ConfigValue{true});
    defaults->set(config_keys::RENDER_SHADOWS, ConfigValue{true});
    defaults->set(config_keys::RENDER_BLOOM, ConfigValue{true});

    // Audio defaults
    defaults->set(config_keys::AUDIO_MASTER_VOLUME, ConfigValue{1.0});
    defaults->set(config_keys::AUDIO_MUSIC_VOLUME, ConfigValue{0.8});
    defaults->set(config_keys::AUDIO_SFX_VOLUME, ConfigValue{1.0});

    // Input defaults
    defaults->set(config_keys::INPUT_MOUSE_SENSITIVITY, ConfigValue{1.0});
    defaults->set(config_keys::INPUT_INVERT_Y, ConfigValue{false});
    defaults->set(config_keys::INPUT_GAMEPAD_DEADZONE, ConfigValue{0.15});

    // Engine defaults
    defaults->set(config_keys::ENGINE_TARGET_FPS, ConfigValue{std::int64_t(60)});
    defaults->set(config_keys::ENGINE_FIXED_UPDATE_RATE, ConfigValue{std::int64_t(50)});
    defaults->set(config_keys::ENGINE_WORKER_THREADS, ConfigValue{std::int64_t(0)});
    defaults->set(config_keys::ENGINE_DEBUG, ConfigValue{false});
    defaults->set(config_keys::ENGINE_PROFILING, ConfigValue{false});
    defaults->set(config_keys::ENGINE_VALIDATION, ConfigValue{false});

    // Path defaults
    defaults->set(config_keys::PATH_ASSETS, ConfigValue{std::string("assets")});
    defaults->set(config_keys::PATH_SHADERS, ConfigValue{std::string("shaders")});
    defaults->set(config_keys::PATH_SAVES, ConfigValue{std::string("saves")});
    defaults->set(config_keys::PATH_LOGS, ConfigValue{std::string("logs")});
    defaults->set(config_keys::PATH_CONFIG, ConfigValue{std::string("config")});

    // Hot-reload defaults
    defaults->set(config_keys::HOT_RELOAD_ENABLED, ConfigValue{true});
    defaults->set(config_keys::HOT_RELOAD_POLL_INTERVAL, ConfigValue{std::int64_t(100)});
}

void ConfigManager::create_default_layers() {
    // Create layers in priority order (highest to lowest)
    add_layer(std::make_unique<ConfigLayer>("cmdline", ConfigLayerPriority::CommandLine));
    add_layer(std::make_unique<ConfigLayer>("environment", ConfigLayerPriority::Environment));
    add_layer(std::make_unique<ConfigLayer>("user", ConfigLayerPriority::User));
    add_layer(std::make_unique<ConfigLayer>("project", ConfigLayerPriority::Project));
    add_layer(std::make_unique<ConfigLayer>("system", ConfigLayerPriority::System));
    add_layer(std::make_unique<ConfigLayer>("defaults", ConfigLayerPriority::Default));
}

EngineConfig ConfigManager::build_engine_config() const {
    EngineConfig config;

    // Window
    config.window.title = get_string(config_keys::WINDOW_TITLE, "void_engine");
    config.window.width = static_cast<std::uint32_t>(get_int(config_keys::WINDOW_WIDTH, 1280));
    config.window.height = static_cast<std::uint32_t>(get_int(config_keys::WINDOW_HEIGHT, 720));
    config.window.vsync = get_bool(config_keys::WINDOW_VSYNC, true);
    config.window.resizable = get_bool(config_keys::WINDOW_RESIZABLE, true);

    // Render
    config.render.max_fps = static_cast<std::uint32_t>(get_int(config_keys::RENDER_MAX_FPS, 0));
    config.render.render_scale = static_cast<float>(get_float(config_keys::RENDER_SCALE, 1.0));
    config.render.enable_hdr = get_bool(config_keys::RENDER_HDR, true);
    config.render.enable_shadows = get_bool(config_keys::RENDER_SHADOWS, true);
    config.render.enable_bloom = get_bool(config_keys::RENDER_BLOOM, true);

    // Audio
    config.audio.master_volume = static_cast<float>(get_float(config_keys::AUDIO_MASTER_VOLUME, 1.0));
    config.audio.music_volume = static_cast<float>(get_float(config_keys::AUDIO_MUSIC_VOLUME, 0.8));
    config.audio.sfx_volume = static_cast<float>(get_float(config_keys::AUDIO_SFX_VOLUME, 1.0));

    // Input
    config.input.mouse_sensitivity = static_cast<float>(get_float(config_keys::INPUT_MOUSE_SENSITIVITY, 1.0));
    config.input.invert_y_axis = get_bool(config_keys::INPUT_INVERT_Y, false);
    config.input.gamepad_deadzone = static_cast<float>(get_float(config_keys::INPUT_GAMEPAD_DEADZONE, 0.15));

    // Engine
    config.target_fps = static_cast<std::uint32_t>(get_int(config_keys::ENGINE_TARGET_FPS, 60));
    config.fixed_update_rate = static_cast<std::uint32_t>(get_int(config_keys::ENGINE_FIXED_UPDATE_RATE, 50));
    config.worker_threads = static_cast<std::uint32_t>(get_int(config_keys::ENGINE_WORKER_THREADS, 0));
    config.enable_debug = get_bool(config_keys::ENGINE_DEBUG, false);
    config.enable_profiling = get_bool(config_keys::ENGINE_PROFILING, false);
    config.enable_validation = get_bool(config_keys::ENGINE_VALIDATION, false);

    // Paths
    config.asset.asset_path = get_string(config_keys::PATH_ASSETS, "assets");
    config.asset.shader_path = get_string(config_keys::PATH_SHADERS, "shaders");
    config.save_path = get_string(config_keys::PATH_SAVES, "saves");
    config.log_path = get_string(config_keys::PATH_LOGS, "logs");
    config.config_path = get_string(config_keys::PATH_CONFIG, "config");

    // Hot-reload
    bool hot_reload = get_bool(config_keys::HOT_RELOAD_ENABLED, true);
    if (hot_reload) {
        config.features |= EngineFeature::HotReload;
    }
    config.asset.hot_reload_poll_interval = std::chrono::milliseconds(
        get_int(config_keys::HOT_RELOAD_POLL_INTERVAL, 100));

    return config;
}

void ConfigManager::apply_engine_config(const EngineConfig& config, const std::string& layer_name) {
    // Window
    set_string(config_keys::WINDOW_TITLE, config.window.title, layer_name);
    set_int(config_keys::WINDOW_WIDTH, config.window.width, layer_name);
    set_int(config_keys::WINDOW_HEIGHT, config.window.height, layer_name);
    set_bool(config_keys::WINDOW_VSYNC, config.window.vsync, layer_name);
    set_bool(config_keys::WINDOW_RESIZABLE, config.window.resizable, layer_name);

    // Render
    set_int(config_keys::RENDER_MAX_FPS, config.render.max_fps, layer_name);
    set_float(config_keys::RENDER_SCALE, config.render.render_scale, layer_name);
    set_bool(config_keys::RENDER_HDR, config.render.enable_hdr, layer_name);
    set_bool(config_keys::RENDER_SHADOWS, config.render.enable_shadows, layer_name);
    set_bool(config_keys::RENDER_BLOOM, config.render.enable_bloom, layer_name);

    // Audio
    set_float(config_keys::AUDIO_MASTER_VOLUME, config.audio.master_volume, layer_name);
    set_float(config_keys::AUDIO_MUSIC_VOLUME, config.audio.music_volume, layer_name);
    set_float(config_keys::AUDIO_SFX_VOLUME, config.audio.sfx_volume, layer_name);

    // Input
    set_float(config_keys::INPUT_MOUSE_SENSITIVITY, config.input.mouse_sensitivity, layer_name);
    set_bool(config_keys::INPUT_INVERT_Y, config.input.invert_y_axis, layer_name);
    set_float(config_keys::INPUT_GAMEPAD_DEADZONE, config.input.gamepad_deadzone, layer_name);

    // Engine
    set_int(config_keys::ENGINE_TARGET_FPS, config.target_fps, layer_name);
    set_int(config_keys::ENGINE_FIXED_UPDATE_RATE, config.fixed_update_rate, layer_name);
    set_int(config_keys::ENGINE_WORKER_THREADS, config.worker_threads, layer_name);
    set_bool(config_keys::ENGINE_DEBUG, config.enable_debug, layer_name);
    set_bool(config_keys::ENGINE_PROFILING, config.enable_profiling, layer_name);
    set_bool(config_keys::ENGINE_VALIDATION, config.enable_validation, layer_name);

    // Paths
    set_string(config_keys::PATH_ASSETS, config.asset.asset_path, layer_name);
    set_string(config_keys::PATH_SHADERS, config.asset.shader_path, layer_name);
    set_string(config_keys::PATH_SAVES, config.save_path, layer_name);
    set_string(config_keys::PATH_LOGS, config.log_path, layer_name);
    set_string(config_keys::PATH_CONFIG, config.config_path, layer_name);

    // Hot-reload
    set_bool(config_keys::HOT_RELOAD_ENABLED, has_feature(config.features, EngineFeature::HotReload), layer_name);
    set_int(config_keys::HOT_RELOAD_POLL_INTERVAL, config.asset.hot_reload_poll_interval.count(), layer_name);
}

std::vector<ConfigLayer*> ConfigManager::sorted_layers() const {
    std::vector<ConfigLayer*> result;
    for (const auto& layer : m_layers) {
        result.push_back(layer.get());
    }

    // Sort by priority (lower value = higher priority)
    std::sort(result.begin(), result.end(),
        [](const ConfigLayer* a, const ConfigLayer* b) {
            return static_cast<int>(a->priority()) < static_cast<int>(b->priority());
        });

    return result;
}

void ConfigManager::notify_change(const std::string& key, const ConfigValue& value) {
    for (const auto& callback : m_change_callbacks) {
        callback(key, value);
    }
}

// =============================================================================
// ConfigWatcher
// =============================================================================

void_core::Result<void_core::HotReloadSnapshot> ConfigWatcher::snapshot() {
    void_core::HotReloadSnapshot snap;
    snap.type_id = std::type_index(typeid(ConfigWatcher));
    snap.type_name = "ConfigWatcher";
    snap.version = current_version();
    return snap;
}

void_core::Result<void> ConfigWatcher::restore(void_core::HotReloadSnapshot snapshot) {
    (void)snapshot;
    return reload();
}

bool ConfigWatcher::is_compatible(const void_core::Version& new_version) const {
    // Config files are always compatible
    (void)new_version;
    return true;
}

void_core::Version ConfigWatcher::current_version() const {
    return void_core::Version{1, 0, 0};
}

void_core::Result<void> ConfigWatcher::reload() {
    // Determine file type from extension
    auto ext = m_path.extension().string();
    if (ext == ".json") {
        return m_manager.load_json(m_path, m_layer_name);
    } else if (ext == ".toml") {
        return m_manager.load_toml(m_path, m_layer_name);
    }
    return void_core::Error{"Unsupported config file type: " + ext};
}

// =============================================================================
// ConfigSchema
// =============================================================================

void_core::Result<void> ConfigSchema::validate(const ConfigValue& value) const {
    // Type checking
    bool type_match = std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            return type == ConfigValueType::Bool;
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return type == ConfigValueType::Int;
        } else if constexpr (std::is_same_v<T, double>) {
            return type == ConfigValueType::Float;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return type == ConfigValueType::String;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            return type == ConfigValueType::StringArray;
        }
        return false;
    }, value);

    if (!type_match) {
        return void_core::Error{"Type mismatch for key: " + key};
    }

    // Range checking for numeric types
    if (min_value && (type == ConfigValueType::Int || type == ConfigValueType::Float)) {
        // Would need to compare values here
    }
    if (max_value && (type == ConfigValueType::Int || type == ConfigValueType::Float)) {
        // Would need to compare values here
    }

    // Allowed values checking
    if (!allowed_values.empty()) {
        bool found = false;
        for (const auto& allowed : allowed_values) {
            if (value == allowed) {
                found = true;
                break;
            }
        }
        if (!found) {
            return void_core::Error{"Value not in allowed list for key: " + key};
        }
    }

    return void_core::Ok();
}

// =============================================================================
// ConfigSchemaRegistry
// =============================================================================

void ConfigSchemaRegistry::register_schema(ConfigSchema schema) {
    m_schemas[schema.key] = std::move(schema);
}

const ConfigSchema* ConfigSchemaRegistry::get_schema(const std::string& key) const {
    auto it = m_schemas.find(key);
    return it != m_schemas.end() ? &it->second : nullptr;
}

std::vector<std::string> ConfigSchemaRegistry::validate(const ConfigLayer& layer) const {
    std::vector<std::string> errors;

    // Check required keys
    for (const auto& [key, schema] : m_schemas) {
        if (schema.required && !layer.contains(key)) {
            errors.push_back("Missing required key: " + key);
        }
    }

    // Validate present values
    for (const auto& key : layer.keys()) {
        auto value = layer.get(key);
        if (!value) continue;

        auto result = validate(key, *value);
        if (!result) {
            errors.push_back(result.error().message());
        }
    }

    return errors;
}

void_core::Result<void> ConfigSchemaRegistry::validate(const std::string& key, const ConfigValue& value) const {
    const ConfigSchema* schema = get_schema(key);
    if (!schema) {
        // No schema registered - allow anything
        return void_core::Ok();
    }
    return schema->validate(value);
}

} // namespace void_engine
