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

WasmResult<void> Plugin::on_spawn(std::uint64_t entity_id) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    if (auto* exp = module_->find_export("on_spawn")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {WasmValue{static_cast<std::int64_t>(entity_id)}};
            auto result = instance_->call("on_spawn", args);
            if (!result) {
                return result.error();
            }
        }
    }
    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::on_destroy(std::uint64_t entity_id) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    if (auto* exp = module_->find_export("on_destroy")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {WasmValue{static_cast<std::int64_t>(entity_id)}};
            auto result = instance_->call("on_destroy", args);
            if (!result) {
                return result.error();
            }
        }
    }
    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::on_collision(std::uint64_t entity_a, std::uint64_t entity_b) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    if (auto* exp = module_->find_export("on_collision")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {
                WasmValue{static_cast<std::int64_t>(entity_a)},
                WasmValue{static_cast<std::int64_t>(entity_b)}
            };
            auto result = instance_->call("on_collision", args);
            if (!result) {
                return result.error();
            }
        }
    }
    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::on_input(std::int32_t key_code, bool pressed) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    if (auto* exp = module_->find_export("on_input")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {
                WasmValue{key_code},
                WasmValue{pressed ? 1 : 0}
            };
            auto result = instance_->call("on_input", args);
            if (!result) {
                return result.error();
            }
        }
    }
    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::on_message(std::uint64_t from_entity, std::int32_t msg_type, std::span<const WasmValue> data) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    if (auto* exp = module_->find_export("on_message")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {
                WasmValue{static_cast<std::int64_t>(from_entity)},
                WasmValue{msg_type}
            };
            auto result = instance_->call("on_message", args);
            if (!result) {
                return result.error();
            }
        }
    }
    return WasmResult<void>::ok();
}

WasmResult<void> Plugin::on_interact(std::uint64_t entity_a, std::uint64_t entity_b) {
    if (state_ != PluginState::Active) {
        return WasmResult<void>::ok();
    }

    if (auto* exp = module_->find_export("on_interact")) {
        if (exp->kind == WasmExternKind::Func) {
            std::vector<WasmValue> args = {
                WasmValue{static_cast<std::int64_t>(entity_a)},
                WasmValue{static_cast<std::int64_t>(entity_b)}
            };
            auto result = instance_->call("on_interact", args);
            if (!result) {
                return result.error();
            }
        }
    }
    return WasmResult<void>::ok();
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
    // Logging functions - args: (ptr, len) for string
    runtime.register_host_function("host", "log_info",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            // String will be read by the runtime when memory is available
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

    // Component functions
    runtime.register_host_function("host", "set_component_i32",
        WasmFunctionType{{WasmValType::I64, WasmValType::I32, WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            // args: entity, name_ptr, name_len, value
            WasmValue val;
            val.type = WasmValType::I32;
            val.i32 = args[3].i32;
            api->set_component(static_cast<std::uint64_t>(args[0].i64), "component", val);
            return std::vector<WasmValue>{};
        }, this);

    runtime.register_host_function("host", "set_component_f64",
        WasmFunctionType{{WasmValType::I64, WasmValType::I32, WasmValType::I32, WasmValType::F64}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            WasmValue val;
            val.type = WasmValType::F64;
            val.f64 = args[3].f64;
            api->set_component(static_cast<std::uint64_t>(args[0].i64), "component", val);
            return std::vector<WasmValue>{};
        }, this);

    runtime.register_host_function("host", "get_component_i32",
        WasmFunctionType{{WasmValType::I64, WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            WasmValue val = api->get_component(static_cast<std::uint64_t>(args[0].i64), "component");
            return std::vector<WasmValue>{WasmValue{val.i32}};
        }, this);

    runtime.register_host_function("host", "get_component_f64",
        WasmFunctionType{{WasmValType::I64, WasmValType::I32, WasmValType::I32}, {WasmValType::F64}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            WasmValue val = api->get_component(static_cast<std::uint64_t>(args[0].i64), "component");
            return std::vector<WasmValue>{WasmValue{val.f64}};
        }, this);

    runtime.register_host_function("host", "has_component",
        WasmFunctionType{{WasmValType::I64, WasmValType::I32, WasmValType::I32}, {WasmValType::I32}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            bool has = api->has_component(static_cast<std::uint64_t>(args[0].i64), "component");
            return std::vector<WasmValue>{WasmValue{has ? 1 : 0}};
        }, this);

    runtime.register_host_function("host", "remove_component",
        WasmFunctionType{{WasmValType::I64, WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            api->remove_component(static_cast<std::uint64_t>(args[0].i64), "component");
            return std::vector<WasmValue>{};
        }, this);

    // Event functions
    runtime.register_host_function("host", "emit_event",
        WasmFunctionType{{WasmValType::I32, WasmValType::I32}, {}},
        [](std::span<const WasmValue> args, void* user) -> WasmResult<std::vector<WasmValue>> {
            auto* api = static_cast<HostApi*>(user);
            api->emit_event("event", {});
            return std::vector<WasmValue>{};
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
    if (destroy_entity_callback_) {
        destroy_entity_callback_(entity);
    }
}

bool HostApi::entity_exists(std::uint64_t entity) {
    if (entity_exists_callback_) {
        return entity_exists_callback_(entity);
    }
    return entity > 0;
}

void HostApi::set_component(std::uint64_t entity, const std::string& component, WasmValue value) {
    if (set_component_callback_) {
        set_component_callback_(entity, component, value);
    }
}

WasmValue HostApi::get_component(std::uint64_t entity, const std::string& component) {
    if (get_component_callback_) {
        return get_component_callback_(entity, component);
    }
    return WasmValue{};
}

bool HostApi::has_component(std::uint64_t entity, const std::string& component) {
    if (has_component_callback_) {
        return has_component_callback_(entity, component);
    }
    return false;
}

void HostApi::remove_component(std::uint64_t entity, const std::string& component) {
    if (remove_component_callback_) {
        remove_component_callback_(entity, component);
    }
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

void PluginRegistry::broadcast_spawn(std::uint64_t entity_id) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_spawn(entity_id);
        }
    }
}

void PluginRegistry::broadcast_destroy(std::uint64_t entity_id) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_destroy(entity_id);
        }
    }
}

void PluginRegistry::broadcast_collision(std::uint64_t entity_a, std::uint64_t entity_b) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_collision(entity_a, entity_b);
        }
    }
}

void PluginRegistry::broadcast_input(std::int32_t key_code, bool pressed) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_input(key_code, pressed);
        }
    }
}

void PluginRegistry::broadcast_message(std::uint64_t from_entity, std::int32_t msg_type, std::span<const WasmValue> data) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_message(from_entity, msg_type, data);
        }
    }
}

void PluginRegistry::broadcast_interact(std::uint64_t entity_a, std::uint64_t entity_b) {
    for (auto& [id, plugin] : plugins_) {
        if (plugin->state() == PluginState::Active) {
            plugin->on_interact(entity_a, entity_b);
        }
    }
}

void PluginRegistry::enable_hot_reload(bool enabled) {
    hot_reload_enabled_ = enabled;

    if (enabled) {
        // Capture current timestamps for all plugins with source paths
        for (auto& [id, plugin] : plugins_) {
            const auto& source_path = plugin->source_path();
            if (!source_path.empty() && std::filesystem::exists(source_path)) {
                try {
                    file_timestamps_[id] = std::filesystem::last_write_time(source_path);
                } catch (const std::filesystem::filesystem_error&) {
                    // Ignore errors
                }
            }
        }
        VOID_LOG_INFO("[PluginRegistry] Hot reload enabled, tracking {} plugins",
                      file_timestamps_.size());
    } else {
        file_timestamps_.clear();
        VOID_LOG_INFO("[PluginRegistry] Hot reload disabled");
    }
}

void PluginRegistry::check_hot_reload() {
    if (!hot_reload_enabled_) return;

    std::vector<PluginId> plugins_to_reload;

    for (auto& [id, timestamp] : file_timestamps_) {
        auto* plugin = get_plugin(id);
        if (!plugin) continue;

        const auto& source_path = plugin->source_path();
        if (source_path.empty()) continue;

        try {
            if (!std::filesystem::exists(source_path)) continue;

            auto current_time = std::filesystem::last_write_time(source_path);
            if (current_time > timestamp) {
                plugins_to_reload.push_back(id);
                timestamp = current_time;
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Ignore errors during file checks
        }
    }

    // Reload modified plugins
    for (auto id : plugins_to_reload) {
        auto result = hot_reload(id);
        if (!result) {
            VOID_LOG_ERROR("[PluginRegistry] Hot reload failed for plugin: {}",
                          result.error().message());
        }
    }
}

WasmResult<void> PluginRegistry::hot_reload(PluginId id) {
    auto* plugin = get_plugin(id);
    if (!plugin) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Plugin not found"};
    }

    // Store current state
    bool was_active = (plugin->state() == PluginState::Active);
    std::string name = plugin->name();
    std::filesystem::path source_path = plugin->source_path();

    if (source_path.empty()) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Plugin has no source path for hot reload"};
    }

    // Capture plugin state before unloading (for state preservation)
    std::vector<std::uint8_t> memory_snapshot;
    std::vector<WasmValue> global_snapshot;
    if (plugin->memory()) {
        // Save memory state
        std::size_t mem_size = plugin->memory()->size();
        if (mem_size > 0) {
            memory_snapshot.resize(mem_size);
            plugin->memory()->read_bytes(0, memory_snapshot);
        }
    }

    // Shutdown and unload
    if (was_active) {
        plugin->shutdown();
    }
    plugin->unload();

    // Reload from file
    auto load_result = plugin->load(source_path);
    if (!load_result) {
        VOID_LOG_ERROR("[PluginRegistry] Hot reload failed to load '{}': {}",
                      name, load_result.error().message());
        return load_result.error();
    }

    // Restore memory state if possible
    if (!memory_snapshot.empty() && plugin->memory()) {
        std::size_t restore_size = std::min(memory_snapshot.size(), plugin->memory()->size());
        if (restore_size > 0) {
            plugin->memory()->write_bytes(0, std::span<const std::uint8_t>(
                memory_snapshot.data(), restore_size));
        }
    }

    // Re-initialize if was active
    if (was_active) {
        auto result = plugin->initialize();
        if (!result) {
            VOID_LOG_ERROR("[PluginRegistry] Hot reload failed to initialize '{}': {}",
                          name, result.error().message());
            return result.error();
        }

        // Call hot_reloaded hook if exported
        if (plugin->module()) {
            if (auto* exp = plugin->module()->find_export("on_hot_reload")) {
                if (exp->kind == WasmExternKind::Func) {
                    plugin->instance()->call("on_hot_reload");
                }
            }
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
