/// @file system.cpp
/// @brief Main scripting system implementation

#include "system.hpp"

#include <void_engine/core/log.hpp>

namespace void_scripting {

// =============================================================================
// ScriptingSystem Implementation
// =============================================================================

namespace {
    ScriptingSystem* g_system_instance = nullptr;
}

ScriptingSystem::ScriptingSystem() {
    g_system_instance = this;
}

ScriptingSystem::~ScriptingSystem() {
    if (initialized_) {
        shutdown();
    }
    if (g_system_instance == this) {
        g_system_instance = nullptr;
    }
}

ScriptingSystem& ScriptingSystem::instance() {
    if (!g_system_instance) {
        static ScriptingSystem default_instance;
        g_system_instance = &default_instance;
    }
    return *g_system_instance;
}

ScriptingSystem* ScriptingSystem::instance_ptr() {
    return g_system_instance;
}

void ScriptingSystem::initialize(const WasmConfig& config) {
    if (initialized_) {
        VOID_LOG_WARN("[ScriptingSystem] Already initialized");
        return;
    }

    VOID_LOG_INFO("[ScriptingSystem] Initializing with backend: {}",
                  static_cast<int>(config.backend));

    // Create runtime
    runtime_ = std::make_unique<WasmRuntime>(config);

    // Create host API
    host_api_ = std::make_unique<HostApi>();
    host_api_->register_with(*runtime_);

    // Register standard imports
    runtime_->register_wasi_imports();
    runtime_->register_engine_imports();

    // Create plugin registry
    plugins_ = std::make_unique<PluginRegistry>();

    initialized_ = true;

    VOID_LOG_INFO("[ScriptingSystem] Initialization complete");
}

void ScriptingSystem::shutdown() {
    if (!initialized_) {
        return;
    }

    VOID_LOG_INFO("[ScriptingSystem] Shutting down...");

    // Shutdown plugins first
    if (plugins_) {
        plugins_->shutdown_all();
        plugins_.reset();
    }

    // Clear host API
    host_api_.reset();

    // Clear runtime last
    runtime_.reset();

    initialized_ = false;

    VOID_LOG_INFO("[ScriptingSystem] Shutdown complete");
}

WasmResult<Plugin*> ScriptingSystem::load_plugin(const std::filesystem::path& path) {
    if (!initialized_) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Scripting system not initialized"};
    }

    auto result = plugins_->load_plugin(path);
    if (!result) {
        // Emit error event
        if (event_bus_) {
            PluginErrorEvent event;
            event.error = WasmError::InvalidModule;
            event.message = "Failed to load plugin: " + path.string();
            event_bus_->publish(event);
        }
        return result.error();
    }

    auto* plugin = result.value();

    // Resolve dependencies
    auto dep_result = plugins_->resolve_dependencies(plugin->id());
    if (!dep_result) {
        plugins_->unload_plugin(plugin->id());
        return dep_result.error();
    }

    // Initialize plugin
    auto init_result = plugin->initialize();
    if (!init_result) {
        if (event_bus_) {
            PluginErrorEvent event;
            event.plugin_id = plugin->id();
            event.error = WasmError::InvalidModule;
            event.message = "Failed to initialize plugin: " + plugin->name();
            event_bus_->publish(event);
        }
        plugins_->unload_plugin(plugin->id());
        return init_result.error();
    }

    // Emit loaded event
    if (event_bus_) {
        PluginLoadedEvent event;
        event.plugin_id = plugin->id();
        event.plugin_name = plugin->name();
        event_bus_->publish(event);
    }

    return plugin;
}

WasmResult<WasmModule*> ScriptingSystem::load_module(const std::string& name,
                                                      const std::filesystem::path& path) {
    if (!initialized_) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Scripting system not initialized"};
    }

    return runtime_->compile_module(name, path);
}

WasmResult<WasmInstance*> ScriptingSystem::create_instance(const std::string& module_name) {
    if (!initialized_) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "Scripting system not initialized"};
    }

    auto* module = runtime_->find_module(module_name);
    if (!module) {
        return void_core::Error{void_core::ErrorCode::NotFound, "Module not found: " + module_name};
    }

    return runtime_->instantiate(module->id());
}

void ScriptingSystem::update(float delta_time) {
    if (!initialized_) {
        return;
    }

    // Update host API time info
    host_api_->set_delta_time(static_cast<double>(delta_time));
    host_api_->set_frame_count(host_api_->get_frame_count() + 1);

    // Check for hot reload
    plugins_->check_hot_reload();

    // Update all plugins
    plugins_->update_all(delta_time);
}

ScriptingSystem::Stats ScriptingSystem::stats() const {
    Stats s;

    if (runtime_) {
        auto runtime_stats = runtime_->stats();
        s.modules_loaded = runtime_stats.modules_loaded;
        s.instances_active = runtime_stats.instances_active;
        s.total_memory_bytes = runtime_stats.total_memory_bytes;
    }

    if (plugins_) {
        auto all_plugins = plugins_->plugins();
        s.plugins_loaded = all_plugins.size();
        s.plugins_active = plugins_->plugins_by_state(PluginState::Active).size();
    }

    return s;
}

} // namespace void_scripting
