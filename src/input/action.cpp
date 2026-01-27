/// @file action.cpp
/// @brief Input action system implementation

#include <void_engine/input/action.hpp>
#include <algorithm>

namespace void_input {

// =============================================================================
// InputAction Implementation
// =============================================================================

InputAction::InputAction(ActionId id, std::string name, ActionType type)
    : m_id(id)
    , m_name(std::move(name))
    , m_type(type) {

    // Initialize value based on type
    m_value.type = type;
    m_previous_value.type = type;
}

void InputAction::add_binding(const InputBinding& binding) {
    m_bindings.push_back(binding);
}

void InputAction::remove_binding(BindingId id) {
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
            [id](const InputBinding& b) { return b.id == id; }),
        m_bindings.end()
    );
}

void InputAction::clear_bindings() {
    m_bindings.clear();
}

void InputAction::on_triggered(ButtonCallback callback) {
    m_button_callbacks.push_back(std::move(callback));
}

void InputAction::on_axis(Axis1DCallback callback) {
    m_axis1d_callbacks.push_back(std::move(callback));
}

void InputAction::on_axis2d(Axis2DCallback callback) {
    m_axis2d_callbacks.push_back(std::move(callback));
}

void InputAction::update_value(const InputValue& value) {
    m_previous_value = m_value;
    m_value = value;

    // Compute just_pressed/just_released for buttons
    if (m_type == ActionType::Button) {
        bool was_pressed = m_previous_value.button.pressed;
        bool is_pressed = m_value.button.pressed;

        m_value.button.just_pressed = is_pressed && !was_pressed;
        m_value.button.just_released = !is_pressed && was_pressed;
    }
}

void InputAction::fire_callbacks() {
    if (!m_enabled) return;

    switch (m_type) {
        case ActionType::Button:
            if (m_value.button.just_pressed || m_value.button.just_released) {
                for (auto& cb : m_button_callbacks) {
                    cb(m_value.button.pressed);
                }
            }
            break;

        case ActionType::Axis1D:
            if (std::abs(m_value.axis1d.value - m_previous_value.axis1d.value) > 0.001f) {
                for (auto& cb : m_axis1d_callbacks) {
                    cb(m_value.axis1d.value);
                }
            }
            break;

        case ActionType::Axis2D: {
            auto diff = m_value.axis2d.value - m_previous_value.axis2d.value;
            if (std::abs(diff.x) > 0.001f || std::abs(diff.y) > 0.001f) {
                for (auto& cb : m_axis2d_callbacks) {
                    cb(m_value.axis2d.value);
                }
            }
            break;
        }

        default:
            break;
    }
}

// =============================================================================
// InputContext Implementation
// =============================================================================

InputContext::InputContext(ContextId id, std::string name, int priority)
    : m_id(id)
    , m_name(std::move(name))
    , m_priority(priority) {}

InputAction* InputContext::create_action(const std::string& name, ActionType type) {
    ActionId id{m_next_action_id++};
    auto action = std::make_unique<InputAction>(id, name, type);
    auto* ptr = action.get();
    m_actions[name] = std::move(action);
    return ptr;
}

InputAction* InputContext::get_action(const std::string& name) {
    auto it = m_actions.find(name);
    return it != m_actions.end() ? it->second.get() : nullptr;
}

InputAction* InputContext::get_action(ActionId id) {
    for (auto& [name, action] : m_actions) {
        if (action->id() == id) {
            return action.get();
        }
    }
    return nullptr;
}

void InputContext::remove_action(const std::string& name) {
    m_actions.erase(name);
}

void InputContext::update() {
    for (auto& [name, action] : m_actions) {
        action->fire_callbacks();
    }
}

// =============================================================================
// ActionBuilder Implementation
// =============================================================================

ActionBuilder::ActionBuilder(InputContext* context, const std::string& name)
    : m_context(context)
    , m_name(name) {}

ActionBuilder& ActionBuilder::type(ActionType t) {
    m_type = t;
    return *this;
}

ActionBuilder& ActionBuilder::key(KeyCode k, ModifierFlags mods) {
    m_bindings.push_back(BindingSource::key(k, mods));
    return *this;
}

ActionBuilder& ActionBuilder::mouse_button(MouseButton btn) {
    m_bindings.push_back(BindingSource::mouse_button(btn));
    return *this;
}

ActionBuilder& ActionBuilder::gamepad_button(GamepadButton btn) {
    m_bindings.push_back(BindingSource::gamepad_button(btn));
    return *this;
}

ActionBuilder& ActionBuilder::gamepad_axis(GamepadAxis axis, float scale, bool invert) {
    m_bindings.push_back(BindingSource::gamepad_axis(axis, scale, invert));
    return *this;
}

ActionBuilder& ActionBuilder::gamepad_stick(GamepadAxis x_axis, GamepadAxis y_axis) {
    m_bindings.push_back(BindingSource::gamepad_stick(x_axis, y_axis));
    return *this;
}

ActionBuilder& ActionBuilder::on_pressed(ButtonCallback callback) {
    m_button_callbacks.push_back(std::move(callback));
    return *this;
}

ActionBuilder& ActionBuilder::on_axis(Axis1DCallback callback) {
    m_axis1d_callbacks.push_back(std::move(callback));
    return *this;
}

ActionBuilder& ActionBuilder::on_axis2d(Axis2DCallback callback) {
    m_axis2d_callbacks.push_back(std::move(callback));
    return *this;
}

InputAction* ActionBuilder::build() {
    auto* action = m_context->create_action(m_name, m_type);
    if (!action) return nullptr;

    // Add bindings
    std::uint32_t binding_id = 1;
    for (auto& source : m_bindings) {
        InputBinding binding;
        binding.id = BindingId{binding_id++};
        binding.source = source;
        action->add_binding(binding);
    }

    // Add callbacks
    for (auto& cb : m_button_callbacks) {
        action->on_triggered(std::move(cb));
    }
    for (auto& cb : m_axis1d_callbacks) {
        action->on_axis(std::move(cb));
    }
    for (auto& cb : m_axis2d_callbacks) {
        action->on_axis2d(std::move(cb));
    }

    return action;
}

} // namespace void_input
