/// @file action.hpp
/// @brief Input action mapping system for void_input
///
/// STATUS: PRODUCTION (2026-01-28)
/// - Input actions with multiple bindings
/// - Context-based input switching (gameplay, menu, vehicle)
/// - Composite actions (WASD -> 2D axis)
/// - Callbacks and polling
/// - Hot-reloadable bindings

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/hot_reload.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace void_input {

// =============================================================================
// Input Binding
// =============================================================================

/// Source for an input binding
struct BindingSource {
    InputDevice device = InputDevice::Keyboard;

    // The actual input source (only one is valid based on device)
    std::variant<
        KeyCode,
        MouseButton,
        GamepadButton,
        GamepadAxis,
        std::pair<GamepadAxis, GamepadAxis>  // For 2D axis
    > source;

    ModifierFlags modifiers = ModifierFlags::None;

    // For axis inputs
    float scale = 1.0f;         ///< Multiplier for axis value
    bool invert = false;        ///< Invert axis

    // Constructors
    static BindingSource key(KeyCode k, ModifierFlags mods = ModifierFlags::None) {
        BindingSource b;
        b.device = InputDevice::Keyboard;
        b.source = k;
        b.modifiers = mods;
        return b;
    }

    static BindingSource mouse_button(MouseButton btn) {
        BindingSource b;
        b.device = InputDevice::Mouse;
        b.source = btn;
        return b;
    }

    static BindingSource gamepad_button(GamepadButton btn) {
        BindingSource b;
        b.device = InputDevice::Gamepad;
        b.source = btn;
        return b;
    }

    static BindingSource gamepad_axis(GamepadAxis axis, float scale = 1.0f, bool invert = false) {
        BindingSource b;
        b.device = InputDevice::Gamepad;
        b.source = axis;
        b.scale = scale;
        b.invert = invert;
        return b;
    }

    static BindingSource gamepad_stick(GamepadAxis x, GamepadAxis y) {
        BindingSource b;
        b.device = InputDevice::Gamepad;
        b.source = std::make_pair(x, y);
        return b;
    }
};

/// Input binding connects a source to an action
struct InputBinding {
    BindingId id{0};
    std::string name;
    BindingSource source;
    bool enabled = true;
};

// =============================================================================
// Input Action
// =============================================================================

/// Callback types
using ButtonCallback = std::function<void(bool pressed)>;
using Axis1DCallback = std::function<void(float value)>;
using Axis2DCallback = std::function<void(void_math::Vec2 value)>;

/// Input action definition
class InputAction {
public:
    InputAction(ActionId id, std::string name, ActionType type);

    [[nodiscard]] ActionId id() const { return m_id; }
    [[nodiscard]] const std::string& name() const { return m_name; }
    [[nodiscard]] ActionType type() const { return m_type; }
    [[nodiscard]] bool is_enabled() const { return m_enabled; }

    void set_enabled(bool enabled) { m_enabled = enabled; }

    // Bindings
    void add_binding(const InputBinding& binding);
    void remove_binding(BindingId id);
    void clear_bindings();
    [[nodiscard]] const std::vector<InputBinding>& bindings() const { return m_bindings; }

    // Current value
    [[nodiscard]] const InputValue& value() const { return m_value; }
    [[nodiscard]] bool is_pressed() const { return m_type == ActionType::Button && m_value.button.pressed; }
    [[nodiscard]] bool is_just_pressed() const { return m_type == ActionType::Button && m_value.button.just_pressed; }
    [[nodiscard]] bool is_just_released() const { return m_type == ActionType::Button && m_value.button.just_released; }
    [[nodiscard]] float axis1d() const { return m_type == ActionType::Axis1D ? m_value.axis1d.value : 0.0f; }
    [[nodiscard]] void_math::Vec2 axis2d() const { return m_type == ActionType::Axis2D ? m_value.axis2d.value : void_math::Vec2{0, 0}; }

    // Callbacks
    void on_triggered(ButtonCallback callback);
    void on_axis(Axis1DCallback callback);
    void on_axis2d(Axis2DCallback callback);

    // Internal - called by InputSystem
    void update_value(const InputValue& value);
    void fire_callbacks();

private:
    ActionId m_id;
    std::string m_name;
    ActionType m_type;
    bool m_enabled = true;

    std::vector<InputBinding> m_bindings;
    InputValue m_value;
    InputValue m_previous_value;

    std::vector<ButtonCallback> m_button_callbacks;
    std::vector<Axis1DCallback> m_axis1d_callbacks;
    std::vector<Axis2DCallback> m_axis2d_callbacks;
};

// =============================================================================
// Input Context
// =============================================================================

/// Input context groups related actions (e.g., "gameplay", "menu", "vehicle")
class InputContext {
public:
    InputContext(ContextId id, std::string name, int priority = 0);

    [[nodiscard]] ContextId id() const { return m_id; }
    [[nodiscard]] const std::string& name() const { return m_name; }
    [[nodiscard]] int priority() const { return m_priority; }
    [[nodiscard]] bool is_active() const { return m_active; }

    void set_active(bool active) { m_active = active; }
    void set_priority(int priority) { m_priority = priority; }

    // Actions
    InputAction* create_action(const std::string& name, ActionType type);
    InputAction* get_action(const std::string& name);
    InputAction* get_action(ActionId id);
    void remove_action(const std::string& name);
    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<InputAction>>& actions() const { return m_actions; }

    // Consume input (prevents lower priority contexts from receiving it)
    [[nodiscard]] bool consumes_input() const { return m_consumes_input; }
    void set_consumes_input(bool consume) { m_consumes_input = consume; }

    // Update all actions
    void update();

private:
    ContextId m_id;
    std::string m_name;
    int m_priority;
    bool m_active = true;
    bool m_consumes_input = false;

    std::unordered_map<std::string, std::unique_ptr<InputAction>> m_actions;
    std::uint32_t m_next_action_id = 1;
};

// =============================================================================
// Action Map Builder
// =============================================================================

/// Fluent builder for creating input actions
class ActionBuilder {
public:
    explicit ActionBuilder(InputContext* context, const std::string& name);

    ActionBuilder& type(ActionType t);
    ActionBuilder& key(KeyCode k, ModifierFlags mods = ModifierFlags::None);
    ActionBuilder& mouse_button(MouseButton btn);
    ActionBuilder& gamepad_button(GamepadButton btn);
    ActionBuilder& gamepad_axis(GamepadAxis axis, float scale = 1.0f, bool invert = false);
    ActionBuilder& gamepad_stick(GamepadAxis x_axis, GamepadAxis y_axis);
    ActionBuilder& on_pressed(ButtonCallback callback);
    ActionBuilder& on_axis(Axis1DCallback callback);
    ActionBuilder& on_axis2d(Axis2DCallback callback);

    InputAction* build();

private:
    InputContext* m_context;
    std::string m_name;
    ActionType m_type = ActionType::Button;
    std::vector<BindingSource> m_bindings;
    std::vector<ButtonCallback> m_button_callbacks;
    std::vector<Axis1DCallback> m_axis1d_callbacks;
    std::vector<Axis2DCallback> m_axis2d_callbacks;
};

// =============================================================================
// Composite Action Helpers
// =============================================================================

/// Create a 2D axis from 4 keys (WASD style)
struct KeyAxis2D {
    KeyCode up = KeyCode::W;
    KeyCode down = KeyCode::S;
    KeyCode left = KeyCode::A;
    KeyCode right = KeyCode::D;

    static KeyAxis2D wasd() { return KeyAxis2D{}; }
    static KeyAxis2D arrows() { return {KeyCode::Up, KeyCode::Down, KeyCode::Left, KeyCode::Right}; }
};

/// Create a 1D axis from 2 keys
struct KeyAxis1D {
    KeyCode positive;
    KeyCode negative;
};

} // namespace void_input
