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

    // ==========================================================================
    // Engine Integration Callbacks
    // ==========================================================================

    // Entity callbacks
    using EntitySpawnCallback = std::function<void(std::uint64_t id, const std::string& name, const ValueMap& components)>;
    using EntityDestroyCallback = std::function<void(std::uint64_t id)>;
    using EntityExistsCallback = std::function<bool(std::uint64_t id)>;
    using EntityCloneCallback = std::function<void(std::uint64_t src, std::uint64_t dst)>;
    using GetComponentCallback = std::function<Value(std::uint64_t id, const std::string& type)>;
    using SetComponentCallback = std::function<void(std::uint64_t id, const std::string& name, const Value& data)>;
    using HasComponentCallback = std::function<bool(std::uint64_t id, const std::string& type)>;
    using RemoveComponentCallback = std::function<bool(std::uint64_t id, const std::string& type)>;

    // Transform callbacks
    using GetPositionCallback = std::function<Value(std::uint64_t id)>;
    using SetPositionCallback = std::function<void(std::uint64_t id, double x, double y, double z)>;
    using GetRotationCallback = std::function<Value(std::uint64_t id)>;
    using SetRotationCallback = std::function<void(std::uint64_t id, double x, double y, double z)>;
    using GetScaleCallback = std::function<Value(std::uint64_t id)>;
    using SetScaleCallback = std::function<void(std::uint64_t id, double x, double y, double z)>;

    // Hierarchy callbacks
    using GetParentCallback = std::function<std::uint64_t(std::uint64_t id)>;
    using SetParentCallback = std::function<void(std::uint64_t id, std::uint64_t parent)>;
    using GetChildrenCallback = std::function<Value(std::uint64_t id)>;

    // Query callbacks
    using FindEntityCallback = std::function<std::uint64_t(const std::string& name)>;
    using FindEntitiesCallback = std::function<Value(const Value& filter)>;

    // Layer callbacks
    using CreateLayerCallback = std::function<void(std::uint64_t id, const std::string& name, const std::string& type)>;
    using DestroyLayerCallback = std::function<void(std::uint64_t id)>;
    using SetLayerVisibleCallback = std::function<void(std::uint64_t id, bool visible)>;
    using GetLayerVisibleCallback = std::function<bool(std::uint64_t id)>;
    using SetLayerOrderCallback = std::function<void(std::uint64_t id, std::int64_t order)>;

    // Input callbacks
    using GetKeyboardStateCallback = std::function<Value()>;
    using GetMouseStateCallback = std::function<Value()>;

    // Viewport callbacks
    using GetViewportSizeCallback = std::function<Value()>;
    using GetViewportAspectCallback = std::function<double()>;

    // Patch callback
    using EmitPatchCallback = std::function<void(const Value& patch)>;

    // Setters for callbacks
    void set_entity_spawn_callback(EntitySpawnCallback cb) { entity_spawn_callback_ = std::move(cb); }
    void set_entity_destroy_callback(EntityDestroyCallback cb) { entity_destroy_callback_ = std::move(cb); }
    void set_entity_exists_callback(EntityExistsCallback cb) { entity_exists_callback_ = std::move(cb); }
    void set_entity_clone_callback(EntityCloneCallback cb) { entity_clone_callback_ = std::move(cb); }
    void set_get_component_callback(GetComponentCallback cb) { get_component_callback_ = std::move(cb); }
    void set_set_component_callback(SetComponentCallback cb) { set_component_callback_ = std::move(cb); }
    void set_has_component_callback(HasComponentCallback cb) { has_component_callback_ = std::move(cb); }
    void set_remove_component_callback(RemoveComponentCallback cb) { remove_component_callback_ = std::move(cb); }
    void set_get_position_callback(GetPositionCallback cb) { get_position_callback_ = std::move(cb); }
    void set_set_position_callback(SetPositionCallback cb) { set_position_callback_ = std::move(cb); }
    void set_get_rotation_callback(GetRotationCallback cb) { get_rotation_callback_ = std::move(cb); }
    void set_set_rotation_callback(SetRotationCallback cb) { set_rotation_callback_ = std::move(cb); }
    void set_get_scale_callback(GetScaleCallback cb) { get_scale_callback_ = std::move(cb); }
    void set_set_scale_callback(SetScaleCallback cb) { set_scale_callback_ = std::move(cb); }
    void set_get_parent_callback(GetParentCallback cb) { get_parent_callback_ = std::move(cb); }
    void set_set_parent_callback(SetParentCallback cb) { set_parent_callback_ = std::move(cb); }
    void set_get_children_callback(GetChildrenCallback cb) { get_children_callback_ = std::move(cb); }
    void set_find_entity_callback(FindEntityCallback cb) { find_entity_callback_ = std::move(cb); }
    void set_find_entities_callback(FindEntitiesCallback cb) { find_entities_callback_ = std::move(cb); }
    void set_create_layer_callback(CreateLayerCallback cb) { create_layer_callback_ = std::move(cb); }
    void set_destroy_layer_callback(DestroyLayerCallback cb) { destroy_layer_callback_ = std::move(cb); }
    void set_layer_visible_callback(SetLayerVisibleCallback set_cb, GetLayerVisibleCallback get_cb) {
        set_layer_visible_callback_ = std::move(set_cb);
        get_layer_visible_callback_ = std::move(get_cb);
    }
    void set_layer_order_callback(SetLayerOrderCallback cb) { set_layer_order_callback_ = std::move(cb); }
    void set_keyboard_state_callback(GetKeyboardStateCallback cb) { get_keyboard_state_callback_ = std::move(cb); }
    void set_mouse_state_callback(GetMouseStateCallback cb) { get_mouse_state_callback_ = std::move(cb); }
    void set_viewport_callbacks(GetViewportSizeCallback size_cb, GetViewportAspectCallback aspect_cb) {
        get_viewport_size_callback_ = std::move(size_cb);
        get_viewport_aspect_callback_ = std::move(aspect_cb);
    }
    void set_emit_patch_callback(EmitPatchCallback cb) { emit_patch_callback_ = std::move(cb); }

    // Frame data (set by engine before update)
    void set_frame_data(float fps, float delta_time) {
        current_fps_ = fps;
        current_delta_time_ = delta_time;
    }

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
    std::uint64_t next_entity_id_ = 1;
    std::uint64_t next_layer_id_ = 1;
    std::uint64_t next_listener_id_ = 1;

    float current_fps_ = 60.0f;
    float current_delta_time_ = 1.0f / 60.0f;

    // Event listeners
    std::unordered_map<std::string, std::vector<std::pair<std::uint64_t, Value>>> event_listeners_;
    std::unordered_map<std::string, std::vector<std::pair<std::uint64_t, Value>>> once_listeners_;

    // Callbacks
    EntitySpawnCallback entity_spawn_callback_;
    EntityDestroyCallback entity_destroy_callback_;
    EntityExistsCallback entity_exists_callback_;
    EntityCloneCallback entity_clone_callback_;
    GetComponentCallback get_component_callback_;
    SetComponentCallback set_component_callback_;
    HasComponentCallback has_component_callback_;
    RemoveComponentCallback remove_component_callback_;
    GetPositionCallback get_position_callback_;
    SetPositionCallback set_position_callback_;
    GetRotationCallback get_rotation_callback_;
    SetRotationCallback set_rotation_callback_;
    GetScaleCallback get_scale_callback_;
    SetScaleCallback set_scale_callback_;
    GetParentCallback get_parent_callback_;
    SetParentCallback set_parent_callback_;
    GetChildrenCallback get_children_callback_;
    FindEntityCallback find_entity_callback_;
    FindEntitiesCallback find_entities_callback_;
    CreateLayerCallback create_layer_callback_;
    DestroyLayerCallback destroy_layer_callback_;
    SetLayerVisibleCallback set_layer_visible_callback_;
    GetLayerVisibleCallback get_layer_visible_callback_;
    SetLayerOrderCallback set_layer_order_callback_;
    GetKeyboardStateCallback get_keyboard_state_callback_;
    GetMouseStateCallback get_mouse_state_callback_;
    GetViewportSizeCallback get_viewport_size_callback_;
    GetViewportAspectCallback get_viewport_aspect_callback_;
    EmitPatchCallback emit_patch_callback_;
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
