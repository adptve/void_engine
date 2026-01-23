/// @file plugin.cpp
/// @brief WASM-based plugin system implementation

#include "plugin.hpp"

#include <void_engine/core/log.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>

namespace void_scripting {

// =============================================================================
// Plugin Implementation
// =============================================================================

Plugin::Plugin(PluginId id, std::string name)
    : id_(id)
    , name_(std::move(name)) {}

Plugin::~Plugin() {
    if (state_ == PluginState::Active || state_ == PluginState::Paused) {
        unload();
    }
}

Plugin::Plugin(Plugin&& other) noexcept
    : id_(other.id_)
    , name_(std::move(other.name_))
    , metadata_(std::move(other.metadata_))
    , state_(other.state_)
    , module_(other.module_)
    , instance_(other.instance_)
    , error_message_(std::move(other.error_message_))
    , source_path_(std::move(other.source_path_)) {
    other.state_ = PluginState::Unloaded;
    other.module_ = nullptr;
    other.instance_ = nullptr;
}

Plugin& Plugin::operator=(Plugin&& other) noexcept {
    if (this != &other) {
        if (state_ == PluginState::Active || state_ == PluginState::Paused) {
            unload();
        }

        id_ = other.id_;
        name_ = std::move(other.name_);
        metadata_ = std::move(other.metadata_);
        state_ = other.state_;
        module_ = other.module_;
        instance_ = other.instance_;
        error_message_ = std::move(other.error_message_);
        source_path_ = std::move(other.source_path_);

        other.state_ = PluginState::Unloaded;
        other.module_ = nullptr;
        other.instance_ = nullptr;
    }
    return *this;
}

WasmResult<void> Plugin::load(const std::filesystem::path& path) {
    if (state_ != PluginState::Unloaded) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Plugin already loaded"};
    }

    state_ = PluginState::Loading;
    source_path_ = path;

    // Read binary
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        state_ = PluginState::Error;
        error_message_ = "Failed to open file: " + path.string();
        return void_core::Error{void_core::ErrorCode::IOError, error_message_};
    }

    std::size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> binary(size);
    if (!file.read(reinterpret_cast<char*>(binary.data()), size)) {
        state_ = PluginState::Error;
        error_message_ = "Failed to read file: " + path.string();
        return void_core::Error{void_core::ErrorCode::IOError, error_message_};
    }

    return load_binary(binary);
}

WasmResult<void> Plugin::load_binary(std::span<const std::uint8_t> binary) {
    if (state_ != PluginState::Unloaded && state_ != PluginState::Loading) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Plugin already loaded"};
    }

    state_ = PluginState::Loading;

    auto& runtime = WasmRuntime::instance();

    // Compile module
    auto compile_result = runtime.compile_module(name_, binary);
    if (!compile_result) {
        state_ = PluginState::Error;
        error_message_ = "Compilation failed";
        return compile_result.error();
    }

    module_ = compile_result.value();

    // Parse metadata from custom section
    if (auto section = module_->get_custom_section("plugin_metadata")) {
        parse_metadata(*section);
    }

    // Instantiate
    auto inst_result = runtime.instantiate(module_->id());
    if (!inst_result) {
        state_ = PluginState::Error;
        error_message_ = "Instantiation failed";
        return inst_result.error();
    }

    instance_ = inst_result.value();
    state_ = PluginState::Loaded;

    VOID_LOG_INFO("[Plugin] Loaded '{}' v{}", name_, metadata_.version);

    return WasmResult<void>::ok();
}

void Plugin::parse_metadata(std::span<const std::uint8_t> data) {
    // Simple key-value parsing from custom section
    // Format: key=value\n...

    std::string content(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(key);
        trim(value);

        if (key == "name") metadata_.name = value;
        else if (key == "version") metadata_.version = value;
        else if (key == "author") metadata_.author = value;
        else if (key == "description") metadata_.description = value;
        else if (key == "license") metadata_.license = value;
        else if (key == "api_version_min") metadata_.api_version_min = std::stoul(value);
        else if (key == "api_version_max") metadata_.api_version_max = std::stoul(value);
        else if (key == "requires_network") metadata_.requires_network = (value == "true");
        else if (key == "requires_filesystem") metadata_.requires_filesystem = (value == "true");
        else if (key == "requires_threads") metadata_.requires_threads = (value == "true");
        else if (key == "dependencies") {
            // Comma-separated list
            std::istringstream deps(value);
            std::string dep;
            while (std::getline(deps, dep, ',')) {
                trim(dep);
                if (!dep.empty()) {
                    metadata_.dependencies.push_back(dep);
                }
            }
        }
        else if (key == "tags") {
            std::istringstream tags(value);
            std::string tag;
            while (std::getline(tags, tag, ',')) {
                trim(tag);
                if (!tag.empty()) {
                    metadata_.tags.push_back(tag);
                }
            }
        }
    }

    // Use plugin name if metadata name is empty
    if (metadata_.name.empty()) {
        metadata_.name = name_;
    }
}

WasmResult<void> Plugin::initialize() {
    if (state_ != PluginState::Loaded) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Invalid plugin state"};
    }

    state_ = PluginState::Initializing;

    // Call plugin_init if exported
    if (auto* exp = module_->find_export("plugin_init")) {
        if (exp->kind == WasmExternKind::Func) {
            auto result = instance_->call("plugin_init");
            if (!result) {
                state_ = PluginState::Error;
                error_message_ = "plugin_init failed";
                return result.error();
            }
        }
    }

    state_ = PluginState::Active;
    VOID_LOG_INFO("[Plugin] Initialized '{}'", name_);

    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::shutdown() {
    if (state_ != PluginState::Active && state_ != PluginState::Paused) {
        return WasmResult<void>::ok();
    }

    // Call plugin_shutdown if exported
    if (module_ && instance_) {
        if (auto* exp = module_->find_export("plugin_shutdown")) {
            if (exp->kind == WasmExternKind::Func) {
                auto result = instance_->call("plugin_shutdown");
                if (!result) {
                    VOID_LOG_WARN("[Plugin] plugin_shutdown failed for '{}'", name_);
                }
            }
        }
    }

    state_ = PluginState::Unloading;
    VOID_LOG_INFO("[Plugin] Shutdown '{}'", name_);

    return WasmResult<void>::ok();
}

void Plugin::unload() {
    if (state_ == PluginState::Active || state_ == PluginState::Paused) {
        shutdown();
    }

    if (instance_) {
        WasmRuntime::instance().destroy_instance(instance_->id());
        instance_ = nullptr;
    }

    if (module_) {
        WasmRuntime::instance().unload_module(module_->id());
        module_ = nullptr;
    }

    state_ = PluginState::Unloaded;
    VOID_LOG_INFO("[Plugin] Unloaded '{}'", name_);
}

WasmResult<void> Plugin::update(float delta_time) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    // Call plugin_update if exported
    if (auto* exp = module_->find_export("plugin_update")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {WasmValue{delta_time}};
            auto result = instance_->call("plugin_update", args);
            if (!result) {
                if (result.error().code() == void_core::ErrorCode::Timeout) {
                    VOID_LOG_WARN("[Plugin] '{}' exceeded execution limit", name_);
                    state_ = PluginState::Paused;
                }
                return result.error();
            }
        }
    }

    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::on_event(const std::string& event_name, std::span<const WasmValue> args) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    // Check for event handler: on_<event_name>
    std::string handler_name = "on_" + event_name;
    if (auto* exp = module_->find_export(handler_name)) {
        if (exp->kind == WasmExternKind::Func) {
            auto result = instance_->call(handler_name, args);
            if (!result) {
                return result.error();
            }
        }
    }

    return WasmResult<void>::ok();
}

WasmMemory* Plugin::memory() const {
    return instance_ ? instance_->memory() : nullptr;
}

// =============================================================================
// HostApi Implementation
// =============================================================================

namespace {
    HostApi* g_host_api_instance = nullptr;
}

HostApi::HostApi()
    : start_time_(std::chrono::steady_clock::now()) {
    g_host_api_instance = this;
}

HostApi::~HostApi() {
    if (g_host_api_instance == this) {
        g_host_api_instance = nullptr;
    }
}

HostApi& HostApi::instance() {
    if (!g_host_api_instance) {
        static HostApi default_instance;
        g_host_api_instance = &default_instance;
    }
    return *g_host_api_instance;
}

void HostApi::register_with(WasmRuntime& runtime) {
    // Logging functions
    runtime.register_host_function("host", "log_info",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            // TODO: Read string from memory
            auto* api = static_cast<HostApi*>(user);
            api->log_info("Plugin message");
            return std::vector<WasmValue>{};
        }, this);

    runtime.register_host_function("host", "log_warn",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            api->log_warn("Plugin warning");
            return std::vector<WasmValue>{};
        }, this);

    runtime.register_host_function("host", "log_error",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            api->log_error("Plugin error");
            return std::vector<WasmValue>{};
        }, this);

    // Time functions
    runtime.register_host_function("host", "get_time",
        WasmFunctionType{{}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            return std::vector<WasmValue>{WasmValue{api->get_time()}};
        }, this);

    runtime.register_host_function("host", "get_delta_time",
        WasmFunctionType{{}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            return std::vector<WasmValue>{WasmValue{api->get_delta_time()}};
        }, this);

    runtime.register_host_function("host", "get_frame_count",
        WasmFunctionType{{}, {WasmValType::I64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            return std::vector<WasmValue>{WasmValue{static_cast<std::int64_t>(api->get_frame_count())}};
        }, this);

    // Random functions
    runtime.register_host_function("host", "random_u32",
        WasmFunctionType{{}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            return std::vector<WasmValue>{WasmValue{static_cast<std::int32_t>(api->random_u32())}};
        }, this);

    runtime.register_host_function("host", "random_f64",
        WasmFunctionType{{}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            return std::vector<WasmValue>{WasmValue{api->random_f64()}};
        }, this);

    runtime.register_host_function("host", "random_range",
        WasmFunctionType{{WasmValType::F64, WasmValType::F64}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            double min = args[0].f64;
            double max = args[1].f64;
            return std::vector<WasmValue>{WasmValue{api->random_range(min, max)}};
        }, this);

    // Entity functions
    runtime.register_host_function("host", "create_entity",
        WasmFunctionType{{}, {WasmValType::I64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            return std::vector<WasmValue>{WasmValue{static_cast<std::int64_t>(api->create_entity())}};
        }, this);

    runtime.register_host_function("host", "destroy_entity",
        WasmFunctionType{{WasmValType::I64}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            api->destroy_entity(static_cast<std::uint64_t>(args[0].i64));
            return std::vector<WasmValue>{};
        }, this);

    runtime.register_host_function("host", "entity_exists",
        WasmFunctionType{{WasmValType::I64}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            bool exists = api->entity_exists(static_cast<std::uint64_t>(args[0].i64));
            return std::vector<WasmValue>{WasmValue{exists ? 1 : 0}};
        }, this);

    VOID_LOG_INFO("[HostApi] Registered host functions");
}

void HostApi::log_info(const std::string& message) {
    if (log_callback_) {
        log_callback_(0, message);
    } else {
        VOID_LOG_INFO("[Plugin] {}", message);
    }
}

void HostApi::log_warn(const std::string& message) {
    if (log_callback_) {
        log_callback_(1, message);
    } else {
        VOID_LOG_WARN("[Plugin] {}", message);
    }
}

void HostApi::log_error(const std::string& message) {
    if (log_callback_) {
        log_callback_(2, message);
    } else {
        VOID_LOG_ERROR("[Plugin] {}", message);
    }
}

void HostApi::log_debug(const std::string& message) {
    if (log_callback_) {
        log_callback_(3, message);
    } else {
        VOID_LOG_DEBUG("[Plugin] {}", message);
    }
}

double HostApi::get_time() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_time_).count();
}

double HostApi::get_delta_time() const {
    return delta_time_;
}

std::uint64_t HostApi::get_frame_count() const {
    return frame_count_;
}

std::uint32_t HostApi::random_u32() {
    static std::mt19937 gen(std::random_device{}());
    return gen();
}

double HostApi::random_f64() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen);
}

double HostApi::random_range(double min, double max) {
    return min + random_f64() * (max - min);
}

std::uint64_t HostApi::create_entity() {
    if (create_entity_callback_) {
        return create_entity_callback_();
    }
    // Fallback: return incrementing ID
    static std::uint64_t next_id = 1;
    return next_id++;
}

void HostApi::destroy_entity(std::uint64_t entity) {
    // TODO: Integrate with ECS
}

bool HostApi::entity_exists(std::uint64_t entity) {
    // TODO: Integrate with ECS
    return entity > 0;
}

void HostApi::set_component(std::uint64_t entity, const std::string& component, WasmValue value) {
    // TODO: Integrate with ECS
}

WasmValue HostApi::get_component(std::uint64_t entity, const std::string& component) {
    // TODO: Integrate with ECS
    return WasmValue{};
}

bool HostApi::has_component(std::uint64_t entity, const std::string& component) {
    // TODO: Integrate with ECS
    return false;
}

void HostApi::remove_component(std::uint64_t entity, const std::string& component) {
    // TODO: Integrate with ECS
}

void HostApi::emit_event(const std::string& event_name, std::span<const WasmValue> args) {
    if (event_callback_) {
        event_callback_(event_name, args);
    }
}

// =============================================================================
// PluginRegistry Implementation
// =============================================================================

namespace {
    PluginRegistry* g_registry_instance = nullptr;
}

PluginRegistry::PluginRegistry() {
    g_registry_instance = this;
}

PluginRegistry::~PluginRegistry() {
    shutdown_all();
    if (g_registry_instance == this) {
        g_registry_instance = nullptr;
    }
}

PluginRegistry& PluginRegistry::instance() {
    if (!g_registry_instance) {
        static PluginRegistry default_instance;
        g_registry_instance = &default_instance;
    }
    return *g_registry_instance;
}

WasmResult<Plugin*> PluginRegistry::load_plugin(const std::filesystem::path& path) {
    std::string name = path.stem().string();

    // Check if already loaded
    if (auto* existing = find_plugin(name)) {
        return existing;
    }

    PluginId id = PluginId::create(next_plugin_id_++, 0);
    auto plugin = std::make_unique<Plugin>(id, name);

    auto result = plugin->load(path);
    if (!result) {
        return result.error();
    }

    auto* plugin_ptr = plugin.get();
    plugins_[id] = std::move(plugin);
    plugin_names_[name] = id;

    // Track file timestamp for hot reload
    if (hot_reload_enabled_) {
        file_timestamps_[id] = std::filesystem::last_write_time(path);
    }

    VOID_LOG_INFO("[PluginRegistry] Loaded plugin '{}' from {}", name, path.string());

    return plugin_ptr;
}

WasmResult<Plugin*> PluginRegistry::load_plugin(const std::string& name, std::span<const std::uint8_t> binary) {
    // Check if already loaded
    if (auto* existing = find_plugin(name)) {
        return existing;
    }

    PluginId id = PluginId::create(next_plugin_id_++, 0);
    auto plugin = std::make_unique<Plugin>(id, name);

    auto result = plugin->load_binary(binary);
    if (!result) {
        return result.error();
    }

    auto* plugin_ptr = plugin.get();
    plugins_[id] = std::move(plugin);
    plugin_names_[name] = id;

    VOID_LOG_INFO("[PluginRegistry] Loaded plugin '{}' from binary", name);

    return plugin_ptr;
}

bool PluginRegistry::unload_plugin(PluginId id) {
    auto it = plugins_.find(id);
    if (it == plugins_.end()) return false;

    std::string name = it->second->name();

    it->second->unload();
    plugin_names_.erase(name);
    file_timestamps_.erase(id);
    plugins_.erase(it);

    VOID_LOG_INFO("[PluginRegistry] Unloaded plugin '{}'", name);

    return true;
}

Plugin* PluginRegistry::get_plugin(PluginId id) {
    auto it = plugins_.find(id);
    return (it != plugins_.end()) ? it->second.get() : nullptr;
}

const Plugin* PluginRegistry::get_plugin(PluginId id) const {
    auto it = plugins_.find(id);
    return (it != plugins_.end()) ? it->second.get() : nullptr;
}

Plugin* PluginRegistry::find_plugin(const std::string& name) {
    auto it = plugin_names_.find(name);
    if (it == plugin_names_.end()) return nullptr;
    return get_plugin(it->second);
}

std::vector<Plugin*> PluginRegistry::plugins() {
    std::vector<Plugin*> result;
    result.reserve(plugins_.size());
    for (auto& [id, plugin] : plugins_) {
        result.push_back(plugin.get());
    }
    return result;
}

std::vector<Plugin*> PluginRegistry::plugins_by_state(PluginState state) {
    std::vector<Plugin*> result;
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == state) {
            result.push_back(plugin.get());
        }
    }
    return result;
}

void PluginRegistry::initialize_all() {
    // Get load order based on dependencies
    auto order = get_load_order();

    for (auto id : order) {
        if (auto* plugin = get_plugin(id)) {
            if (plugin->state() == PluginState::Loaded) {
                auto result = plugin->initialize();
                if (!result) {
                    VOID_LOG_ERROR("[PluginRegistry] Failed to initialize plugin '{}'",
                                  plugin->name());
                }
            }
        }
    }
}

void PluginRegistry::shutdown_all() {
    // Shutdown in reverse load order
    auto order = get_load_order();
    std::reverse(order.begin(), order.end());

    for (auto id : order) {
        if (auto* plugin = get_plugin(id)) {
            if (plugin->state() == PluginState::Active ||
                plugin->state() == PluginState::Paused) {
                plugin->shutdown();
            }
        }
    }
}

void PluginRegistry::update_all(float delta_time) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->update(delta_time);
        }
    }
}

void PluginRegistry::broadcast_event(const std::string& event_name, std::span<const WasmValue> args) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_event(event_name, args);
        }
    }
}

void PluginRegistry::enable_hot_reload(bool enabled) {
    hot_reload_enabled_ = enabled;

    if (enabled) {
        // Capture current timestamps
        for (auto& [id, plugin] : plugins_) {
            // Would need source path tracking
        }
    }
}

void PluginRegistry::check_hot_reload() {
    if (!hot_reload_enabled_) return;

    for (auto& [id, timestamp] : file_timestamps_) {
        auto* plugin = get_plugin(id);
        if (!plugin) continue;

        // Check if file was modified
        // Note: Would need to track source path in plugin
        // This is a placeholder for the hot reload check
    }
}

WasmResult<void> PluginRegistry::hot_reload(PluginId id) {
    auto* plugin = get_plugin(id);
    if (!plugin) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Invalid plugin state"};
    }

    // Store current state
    bool was_active = (plugin->state() == PluginState::Active);
    std::string name = plugin->name();

    // Get source path (would need to be tracked)
    // For now, this is a stub

    // Unload
    plugin->unload();

    // Reload would happen here with stored path

    // Re-initialize if was active
    if (was_active) {
        auto result = plugin->initialize();
        if (!result) {
            return result.error();
        }
    }

    VOID_LOG_INFO("[PluginRegistry] Hot reloaded plugin '{}'", name);

    return WasmResult<void>::ok();
}

WasmResult<void> PluginRegistry::resolve_dependencies(PluginId id) {
    auto* plugin = get_plugin(id);
    if (!plugin) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Invalid plugin state"};
    }

    for (const auto& dep_name : plugin->metadata().dependencies) {
        auto* dep = find_plugin(dep_name);
        if (!dep) {
            VOID_LOG_ERROR("[PluginRegistry] Missing dependency '{}' for plugin '{}'",
                          dep_name, plugin->name());
            return void_core::Error{void_core::ErrorCode::NotFound, "Import not found"};
        }

        // Ensure dependency is initialized
        if (dep->state() == PluginState::Loaded) {
            auto result = dep->initialize();
            if (!result) {
                return result.error();
            }
        }
    }

    return WasmResult<void>::ok();
}

std::vector<PluginId> PluginRegistry::get_load_order() const {
    // Topological sort based on dependencies
    std::vector<PluginId> result;
    std::unordered_map<PluginId, bool> visited;
    std::unordered_map<PluginId, bool> in_stack;

    std::function<bool(PluginId)> visit = [&](PluginId id) -> bool {
        if (in_stack[id]) {
            VOID_LOG_ERROR("[PluginRegistry] Circular dependency detected");
            return false;
        }

        if (visited[id]) {
            return true;
        }

        visited[id] = true;
        in_stack[id] = true;

        auto it = plugins_.find(id);
        if (it != plugins_.end()) {
            for (const auto& dep_name : it->second->metadata().dependencies) {
                auto name_it = plugin_names_.find(dep_name);
                if (name_it != plugin_names_.end()) {
                    if (!visit(name_it->second)) {
                        return false;
                    }
                }
            }
        }

        in_stack[id] = false;
        result.push_back(id);
        return true;
    };

    for (const auto& [id, plugin] : plugins_) {
        if (!visited[id]) {
            visit(id);
        }
    }

    return result;
}

} // namespace void_scripting
