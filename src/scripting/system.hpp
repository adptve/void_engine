#pragma once

/// @file system.hpp
/// @brief Main scripting system

#include "plugin.hpp"

#include <void_engine/event/event.hpp>

namespace void_scripting {

// =============================================================================
// Scripting Events
// =============================================================================

/// @brief Event: Plugin loaded
struct PluginLoadedEvent {
    PluginId plugin_id;
    std::string plugin_name;
};

/// @brief Event: Plugin unloaded
struct PluginUnloadedEvent {
    PluginId plugin_id;
    std::string plugin_name;
};

/// @brief Event: Plugin error
struct PluginErrorEvent {
    PluginId plugin_id;
    WasmError error;
    std::string message;
};

// =============================================================================
// Scripting System
// =============================================================================

/// @brief Main scripting system
class ScriptingSystem {
public:
    ScriptingSystem();
    ~ScriptingSystem();

    // Singleton access
    [[nodiscard]] static ScriptingSystem& instance();
    [[nodiscard]] static ScriptingSystem* instance_ptr();

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize the scripting system
    void initialize(const WasmConfig& config = {});

    /// @brief Shutdown the scripting system
    void shutdown();

    /// @brief Check if initialized
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ==========================================================================
    // Subsystems
    // ==========================================================================

    /// @brief Get the WASM runtime
    [[nodiscard]] WasmRuntime& runtime() { return *runtime_; }
    [[nodiscard]] const WasmRuntime& runtime() const { return *runtime_; }

    /// @brief Get the plugin registry
    [[nodiscard]] PluginRegistry& plugins() { return *plugins_; }
    [[nodiscard]] const PluginRegistry& plugins() const { return *plugins_; }

    /// @brief Get the host API
    [[nodiscard]] HostApi& host_api() { return *host_api_; }
    [[nodiscard]] const HostApi& host_api() const { return *host_api_; }

    // ==========================================================================
    // Quick Access
    // ==========================================================================

    /// @brief Load a plugin
    WasmResult<Plugin*> load_plugin(const std::filesystem::path& path);

    /// @brief Load a WASM module
    WasmResult<WasmModule*> load_module(const std::string& name,
                                          const std::filesystem::path& path);

    /// @brief Create an instance of a module
    WasmResult<WasmInstance*> create_instance(const std::string& module_name);

    // ==========================================================================
    // Update
    // ==========================================================================

    /// @brief Update all scripts and plugins
    void update(float delta_time);

    // ==========================================================================
    // Events
    // ==========================================================================

    /// @brief Set event bus
    void set_event_bus(void_event::EventBus* bus) { event_bus_ = bus; }

    /// @brief Get event bus
    [[nodiscard]] void_event::EventBus* event_bus() const { return event_bus_; }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t modules_loaded = 0;
        std::size_t instances_active = 0;
        std::size_t plugins_loaded = 0;
        std::size_t plugins_active = 0;
        std::size_t total_memory_bytes = 0;
    };

    [[nodiscard]] Stats stats() const;

private:
    std::unique_ptr<WasmRuntime> runtime_;
    std::unique_ptr<PluginRegistry> plugins_;
    std::unique_ptr<HostApi> host_api_;

    void_event::EventBus* event_bus_ = nullptr;
    bool initialized_ = false;
};

// =============================================================================
// Prelude Namespace
// =============================================================================

namespace prelude {

using void_scripting::ScriptingSystem;
using void_scripting::WasmRuntime;
using void_scripting::WasmModule;
using void_scripting::WasmInstance;
using void_scripting::WasmValue;
using void_scripting::WasmError;
using void_scripting::Plugin;
using void_scripting::PluginRegistry;
using void_scripting::HostApi;

} // namespace prelude

} // namespace void_scripting
