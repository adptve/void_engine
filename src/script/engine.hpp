#pragma once

/// @file engine.hpp
/// @brief Main script engine system

#include "interpreter.hpp"

#include <void_engine/ecs/ecs.hpp>
#include <void_engine/event/event.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace void_script {

// =============================================================================
// Script Component
// =============================================================================

/// @brief ECS component for scripted entities
struct ScriptComponent {
    ScriptId script_id;
    std::unique_ptr<ScriptContext> context;
    bool enabled = true;
    bool auto_tick = true;

    std::unordered_map<std::string, Value> local_variables;
};

// =============================================================================
// Script Asset
// =============================================================================

/// @brief A loaded script asset
struct ScriptAsset {
    ScriptId id;
    std::string name;
    std::string source;
    std::filesystem::path path;
    std::unique_ptr<Program> ast;

    bool is_module = false;
    std::vector<ScriptId> dependencies;

    std::filesystem::file_time_type last_modified;
};

// =============================================================================
// Script Events
// =============================================================================

/// @brief Event: Script execution started
struct ScriptStartedEvent {
    ScriptId script_id;
    std::uint64_t entity_id;
};

/// @brief Event: Script execution completed
struct ScriptCompletedEvent {
    ScriptId script_id;
    std::uint64_t entity_id;
    Value result;
};

/// @brief Event: Script error occurred
struct ScriptErrorEvent {
    ScriptId script_id;
    std::uint64_t entity_id;
    ScriptException exception;
};

// =============================================================================
// Native Binding
// =============================================================================

/// @brief Binding for native C++ types and functions
class NativeBinding {
public:
    NativeBinding() = default;

    /// @brief Register a native function
    template <typename... Args>
    NativeBinding& function(const std::string& name, Value (*func)(Args...));

    /// @brief Register a native method
    template <typename T, typename R, typename... Args>
    NativeBinding& method(const std::string& name, R (T::*method)(Args...));

    /// @brief Register a native property getter
    template <typename T, typename R>
    NativeBinding& property(const std::string& name, R (T::*getter)() const);

    /// @brief Register a native property getter/setter
    template <typename T, typename R>
    NativeBinding& property(const std::string& name, R (T::*getter)() const, void (T::*setter)(R));

    /// @brief Register a constant
    NativeBinding& constant(const std::string& name, Value value);

    /// @brief Apply to an interpreter
    void apply(Interpreter& interp) const;

private:
    std::vector<std::pair<std::string, Value>> constants_;
    std::vector<std::tuple<std::string, std::size_t, NativeFunction::Func>> functions_;
};

// =============================================================================
// Script Engine
// =============================================================================

/// @brief Main script engine system
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Singleton access
    [[nodiscard]] static ScriptEngine& instance();
    [[nodiscard]] static ScriptEngine* instance_ptr();

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize the script engine
    void initialize();

    /// @brief Shutdown the script engine
    void shutdown();

    /// @brief Check if initialized
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // ==========================================================================
    // Script Management
    // ==========================================================================

    /// @brief Load a script from source
    ScriptId load_script(const std::string& source, const std::string& name = "");

    /// @brief Load a script from file
    ScriptId load_file(const std::filesystem::path& path);

    /// @brief Unload a script
    bool unload_script(ScriptId id);

    /// @brief Get a script
    [[nodiscard]] ScriptAsset* get_script(ScriptId id);
    [[nodiscard]] const ScriptAsset* get_script(ScriptId id) const;

    /// @brief Find script by name
    [[nodiscard]] ScriptAsset* find_script(const std::string& name);

    /// @brief Get all loaded scripts
    [[nodiscard]] std::vector<ScriptAsset*> all_scripts();

    // ==========================================================================
    // Execution
    // ==========================================================================

    /// @brief Execute a script
    Value execute(ScriptId id);

    /// @brief Execute source code
    Value execute(const std::string& source);

    /// @brief Execute a function in a script
    Value call_function(ScriptId id, const std::string& function_name,
                        const std::vector<Value>& args = {});

    // ==========================================================================
    // Entity Integration
    // ==========================================================================

    /// @brief Attach a script to an entity
    ScriptComponent* attach_script(std::uint64_t entity_id, ScriptId script_id);

    /// @brief Detach a script from an entity
    void detach_script(std::uint64_t entity_id);

    /// @brief Get the script component for an entity
    [[nodiscard]] ScriptComponent* get_component(std::uint64_t entity_id);

    /// @brief Call a method on an entity's script
    Value call_method(std::uint64_t entity_id, const std::string& method_name,
                      const std::vector<Value>& args = {});

    // ==========================================================================
    // Update
    // ==========================================================================

    /// @brief Update all scripts
    void update(float delta_time);

    // ==========================================================================
    // Native Bindings
    // ==========================================================================

    /// @brief Register a native binding
    void register_binding(const std::string& name, const NativeBinding& binding);

    /// @brief Register a native function globally
    void register_function(const std::string& name, std::size_t arity, NativeFunction::Func func);

    /// @brief Register a native constant globally
    void register_constant(const std::string& name, Value value);

    /// @brief Register engine API
    void register_engine_api();

    // ==========================================================================
    // Hot Reload
    // ==========================================================================

    /// @brief Enable hot reload
    void enable_hot_reload(bool enabled);

    /// @brief Check for file changes
    void check_hot_reload();

    /// @brief Hot reload a script
    bool hot_reload(ScriptId id);

    // ==========================================================================
    // Debugging
    // ==========================================================================

    /// @brief Enable debug mode
    void set_debug_mode(bool enabled);
    [[nodiscard]] bool debug_mode() const { return debug_mode_; }

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
        std::size_t loaded_scripts = 0;
        std::size_t active_contexts = 0;
        std::size_t total_executions = 0;
        float average_execution_time_ms = 0.0f;
    };

    [[nodiscard]] Stats stats() const;

private:
    std::unordered_map<ScriptId, std::unique_ptr<ScriptAsset>> scripts_;
    std::unordered_map<std::string, ScriptId> script_names_;
    std::unordered_map<std::uint64_t, ScriptComponent> entity_components_;

    std::unique_ptr<Interpreter> global_interpreter_;
    std::unordered_map<std::string, NativeBinding> bindings_;

    void_event::EventBus* event_bus_ = nullptr;

    bool initialized_ = false;
    bool debug_mode_ = false;
    bool hot_reload_enabled_ = false;

    inline static std::uint32_t next_script_id_ = 1;
};

// =============================================================================
// Prelude Namespace
// =============================================================================

/// @brief Convenient imports for common usage
namespace prelude {

using void_script::Interpreter;
using void_script::ScriptContext;
using void_script::ScriptEngine;
using void_script::Value;
using void_script::NativeFunction;
using void_script::NativeBinding;
using void_script::ScriptId;

} // namespace prelude

} // namespace void_script
