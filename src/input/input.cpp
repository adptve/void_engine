/// @file input.cpp
/// @brief Input system implementation with GLFW integration
///
/// STATUS: PRODUCTION (2026-01-28)

#include <void_engine/input/input.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>

namespace void_input {

// =============================================================================
// Static callback pointers (GLFW uses C callbacks)
// =============================================================================

static InputSystem* s_input_system = nullptr;

static void glfw_key_callback(GLFWwindow*, int key, int scancode, int action, int mods) {
    if (s_input_system) s_input_system->on_key(key, scancode, action, mods);
}

static void glfw_char_callback(GLFWwindow*, unsigned int codepoint) {
    if (s_input_system) s_input_system->on_char(codepoint);
}

static void glfw_mouse_button_callback(GLFWwindow*, int button, int action, int mods) {
    if (s_input_system) s_input_system->on_mouse_button(button, action, mods);
}

static void glfw_cursor_pos_callback(GLFWwindow*, double x, double y) {
    if (s_input_system) s_input_system->on_cursor_pos(x, y);
}

static void glfw_scroll_callback(GLFWwindow*, double x, double y) {
    if (s_input_system) s_input_system->on_scroll(x, y);
}

static void glfw_joystick_callback(int jid, int event) {
    if (s_input_system) s_input_system->on_joystick(jid, event);
}

// =============================================================================
// InputSystem Implementation
// =============================================================================

InputSystem::InputSystem() {
    // Initialize gamepads
    for (auto& pad : m_gamepads) {
        pad.connected = false;
    }
}

InputSystem::~InputSystem() {
    shutdown();
}

void InputSystem::initialize(GLFWwindow* window) {
    if (m_initialized) return;

    m_window = window;
    s_input_system = this;

    // Set GLFW callbacks
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetCharCallback(window, glfw_char_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetJoystickCallback(glfw_joystick_callback);

    // Get initial mouse position
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    m_mouse.position = {static_cast<float>(mx), static_cast<float>(my)};
    m_mouse.previous_position = m_mouse.position;

    // Check for connected gamepads
    for (int i = 0; i < MAX_GAMEPADS; ++i) {
        if (glfwJoystickPresent(i)) {
            m_gamepads[i].connected = true;
            const char* name = glfwGetJoystickName(i);
            m_gamepads[i].name = name ? name : "Unknown Gamepad";
        }
    }

    m_initialized = true;
}

void InputSystem::shutdown() {
    if (!m_initialized) return;

    if (m_window && s_input_system == this) {
        // Remove callbacks
        glfwSetKeyCallback(m_window, nullptr);
        glfwSetCharCallback(m_window, nullptr);
        glfwSetMouseButtonCallback(m_window, nullptr);
        glfwSetCursorPosCallback(m_window, nullptr);
        glfwSetScrollCallback(m_window, nullptr);
        glfwSetJoystickCallback(nullptr);

        s_input_system = nullptr;
    }

    m_contexts.clear();
    m_window = nullptr;
    m_initialized = false;
}

void InputSystem::update() {
    if (!m_initialized) return;

    // Update previous states
    m_keyboard.update();
    m_mouse.update();
    for (auto& pad : m_gamepads) {
        pad.update();
    }

    // Poll GLFW events
    glfwPollEvents();

    // Update gamepads
    update_gamepads();

    // Update actions
    update_actions();

    // Reset per-frame stats
    m_stats.key_events = 0;
    m_stats.mouse_events = 0;
    m_stats.gamepad_events = 0;
    m_stats.actions_triggered = 0;
}

void InputSystem::process_events() {
    glfwPollEvents();
}

const GamepadState& InputSystem::gamepad(int index) const {
    static GamepadState dummy;
    if (index < 0 || index >= MAX_GAMEPADS) return dummy;
    return m_gamepads[index];
}

GamepadState& InputSystem::gamepad(int index) {
    static GamepadState dummy;
    if (index < 0 || index >= MAX_GAMEPADS) return dummy;
    return m_gamepads[index];
}

int InputSystem::connected_gamepad_count() const {
    int count = 0;
    for (const auto& pad : m_gamepads) {
        if (pad.connected) ++count;
    }
    return count;
}

void InputSystem::capture_mouse(bool capture) {
    if (!m_window) return;

    m_mouse.captured = capture;
    glfwSetInputMode(m_window, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    // Enable raw mouse motion if available
    if (capture && m_config.raw_mouse_input && glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    } else {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }
}

void InputSystem::set_mouse_visible(bool visible) {
    if (!m_window) return;

    m_mouse.visible = visible;
    if (!m_mouse.captured) {
        glfwSetInputMode(m_window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

void InputSystem::set_mouse_position(void_math::Vec2 pos) {
    if (!m_window) return;

    glfwSetCursorPos(m_window, static_cast<double>(pos.x), static_cast<double>(pos.y));
    m_mouse.position = pos;
}

InputContext* InputSystem::create_context(const std::string& name, int priority) {
    ContextId id{m_next_context_id++};
    auto context = std::make_unique<InputContext>(id, name, priority);
    auto* ptr = context.get();
    m_contexts[name] = std::move(context);
    return ptr;
}

InputContext* InputSystem::get_context(const std::string& name) {
    auto it = m_contexts.find(name);
    return it != m_contexts.end() ? it->second.get() : nullptr;
}

InputContext* InputSystem::get_context(ContextId id) {
    for (auto& [name, context] : m_contexts) {
        if (context->id() == id) {
            return context.get();
        }
    }
    return nullptr;
}

void InputSystem::remove_context(const std::string& name) {
    m_contexts.erase(name);
}

void InputSystem::activate_context(const std::string& name) {
    if (auto* ctx = get_context(name)) {
        ctx->set_active(true);
    }
}

void InputSystem::deactivate_context(const std::string& name) {
    if (auto* ctx = get_context(name)) {
        ctx->set_active(false);
    }
}

std::vector<InputContext*> InputSystem::active_contexts() const {
    std::vector<InputContext*> result;
    for (const auto& [name, context] : m_contexts) {
        if (context->is_active()) {
            result.push_back(context.get());
        }
    }

    // Sort by priority (higher priority first)
    std::sort(result.begin(), result.end(), [](InputContext* a, InputContext* b) {
        return a->priority() > b->priority();
    });

    return result;
}

InputAction* InputSystem::find_action(const std::string& name) {
    for (auto* ctx : active_contexts()) {
        if (auto* action = ctx->get_action(name)) {
            return action;
        }
    }
    return nullptr;
}

bool InputSystem::is_action_pressed(const std::string& name) {
    if (auto* action = find_action(name)) {
        return action->is_pressed();
    }
    return false;
}

bool InputSystem::is_action_just_pressed(const std::string& name) {
    if (auto* action = find_action(name)) {
        return action->is_just_pressed();
    }
    return false;
}

float InputSystem::get_action_axis(const std::string& name) {
    if (auto* action = find_action(name)) {
        return action->axis1d();
    }
    return 0.0f;
}

void_math::Vec2 InputSystem::get_action_axis2d(const std::string& name) {
    if (auto* action = find_action(name)) {
        return action->axis2d();
    }
    return {0, 0};
}

void InputSystem::set_config(const InputConfig& config) {
    m_config = config;

    // Apply gamepad deadzone to all gamepads
    for (auto& pad : m_gamepads) {
        pad.deadzone_inner = config.gamepad_deadzone_inner;
        pad.deadzone_outer = config.gamepad_deadzone_outer;
    }
}

// =============================================================================
// Hot Reload (SACRED)
// =============================================================================

std::vector<std::uint8_t> InputSystem::snapshot() const {
    std::vector<std::uint8_t> data;

    // Version
    std::uint32_t version = 1;
    data.insert(data.end(), reinterpret_cast<const std::uint8_t*>(&version),
                            reinterpret_cast<const std::uint8_t*>(&version) + sizeof(version));

    // Config
    data.insert(data.end(), reinterpret_cast<const std::uint8_t*>(&m_config),
                            reinterpret_cast<const std::uint8_t*>(&m_config) + sizeof(m_config));

    // Mouse state (captured/visible)
    std::uint8_t mouse_flags = (m_mouse.captured ? 1 : 0) | (m_mouse.visible ? 2 : 0);
    data.push_back(mouse_flags);

    return data;
}

void InputSystem::restore(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t)) return;

    const std::uint8_t* ptr = data.data();

    // Version
    std::uint32_t version;
    std::memcpy(&version, ptr, sizeof(version));
    ptr += sizeof(version);

    if (version != 1) return;

    // Config
    if (ptr + sizeof(InputConfig) <= data.data() + data.size()) {
        std::memcpy(&m_config, ptr, sizeof(m_config));
        ptr += sizeof(m_config);
        set_config(m_config);
    }

    // Mouse state
    if (ptr < data.data() + data.size()) {
        std::uint8_t mouse_flags = *ptr++;
        capture_mouse((mouse_flags & 1) != 0);
        set_mouse_visible((mouse_flags & 2) != 0);
    }
}

// =============================================================================
// GLFW Callbacks
// =============================================================================

void InputSystem::on_key(int key, int /*scancode*/, int action, int mods) {
    if (key < 0 || key >= static_cast<int>(KeyboardState::KEY_COUNT)) return;

    m_keyboard.keys[key] = (action != GLFW_RELEASE);

    // Update modifiers
    m_keyboard.modifiers = ModifierFlags::None;
    if (mods & GLFW_MOD_SHIFT) m_keyboard.modifiers = m_keyboard.modifiers | ModifierFlags::Shift;
    if (mods & GLFW_MOD_CONTROL) m_keyboard.modifiers = m_keyboard.modifiers | ModifierFlags::Control;
    if (mods & GLFW_MOD_ALT) m_keyboard.modifiers = m_keyboard.modifiers | ModifierFlags::Alt;
    if (mods & GLFW_MOD_SUPER) m_keyboard.modifiers = m_keyboard.modifiers | ModifierFlags::Super;
    if (mods & GLFW_MOD_CAPS_LOCK) m_keyboard.modifiers = m_keyboard.modifiers | ModifierFlags::CapsLock;
    if (mods & GLFW_MOD_NUM_LOCK) m_keyboard.modifiers = m_keyboard.modifiers | ModifierFlags::NumLock;

    m_stats.key_events++;
}

void InputSystem::on_char(unsigned int codepoint) {
    m_text_input.push_back(codepoint);
}

void InputSystem::on_mouse_button(int button, int action, int /*mods*/) {
    if (button >= 0 && button < 8) {
        m_mouse.buttons[button] = (action != GLFW_RELEASE);
    }
    m_stats.mouse_events++;
}

void InputSystem::on_cursor_pos(double x, double y) {
    void_math::Vec2 new_pos{static_cast<float>(x), static_cast<float>(y)};

    m_mouse.delta = new_pos - m_mouse.position;
    m_mouse.delta.x *= m_config.mouse_sensitivity;
    m_mouse.delta.y *= m_config.mouse_sensitivity * (m_config.invert_y ? -1.0f : 1.0f);

    m_mouse.position = new_pos;
}

void InputSystem::on_scroll(double x, double y) {
    m_mouse.scroll = {
        static_cast<float>(x) * m_config.scroll_sensitivity,
        static_cast<float>(y) * m_config.scroll_sensitivity
    };
}

void InputSystem::on_joystick(int jid, int event) {
    if (jid < 0 || jid >= MAX_GAMEPADS) return;

    if (event == GLFW_CONNECTED) {
        m_gamepads[jid].connected = true;
        const char* name = glfwGetJoystickName(jid);
        m_gamepads[jid].name = name ? name : "Unknown Gamepad";
    } else if (event == GLFW_DISCONNECTED) {
        m_gamepads[jid].connected = false;
        m_gamepads[jid].name.clear();
    }
}

// =============================================================================
// Internal Update Functions
// =============================================================================

void InputSystem::update_gamepads() {
    for (int i = 0; i < MAX_GAMEPADS; ++i) {
        auto& pad = m_gamepads[i];

        if (!glfwJoystickPresent(i)) {
            if (pad.connected) {
                pad.connected = false;
                pad.name.clear();
            }
            continue;
        }

        if (!pad.connected) {
            pad.connected = true;
            const char* name = glfwGetJoystickName(i);
            pad.name = name ? name : "Unknown Gamepad";
        }

        // Try to get gamepad state (mapped)
        GLFWgamepadstate state;
        if (glfwGetGamepadState(i, &state)) {
            // Buttons
            for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST && b < 15; ++b) {
                pad.buttons[b] = (state.buttons[b] == GLFW_PRESS);
            }

            // Axes
            for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST && a < 6; ++a) {
                pad.axes[a] = state.axes[a];
            }
        } else {
            // Fallback to raw joystick input
            int axis_count;
            const float* axes = glfwGetJoystickAxes(i, &axis_count);
            if (axes) {
                for (int a = 0; a < axis_count && a < 6; ++a) {
                    pad.axes[a] = axes[a];
                }
            }

            int button_count;
            const unsigned char* buttons = glfwGetJoystickButtons(i, &button_count);
            if (buttons) {
                for (int b = 0; b < button_count && b < 15; ++b) {
                    pad.buttons[b] = (buttons[b] == GLFW_PRESS);
                }
            }
        }
    }

    m_stats.connected_gamepads = connected_gamepad_count();
}

void InputSystem::update_actions() {
    for (auto* context : active_contexts()) {
        for (auto& [name, action] : context->actions()) {
            if (!action->is_enabled()) continue;

            InputValue combined_value;
            combined_value.type = action->type();
            bool any_active = false;

            // Evaluate all bindings and combine
            for (const auto& binding : action->bindings()) {
                if (!binding.enabled) continue;

                InputValue binding_value = evaluate_binding(binding.source);

                // Combine values based on type
                switch (action->type()) {
                    case ActionType::Button:
                        if (binding_value.button.pressed) {
                            combined_value.button.pressed = true;
                            any_active = true;
                        }
                        break;

                    case ActionType::Axis1D:
                        if (std::abs(binding_value.axis1d.value) > std::abs(combined_value.axis1d.value)) {
                            combined_value.axis1d.value = binding_value.axis1d.value;
                            any_active = true;
                        }
                        break;

                    case ActionType::Axis2D: {
                        float new_mag = binding_value.axis2d.value.x * binding_value.axis2d.value.x +
                                        binding_value.axis2d.value.y * binding_value.axis2d.value.y;
                        float old_mag = combined_value.axis2d.value.x * combined_value.axis2d.value.x +
                                        combined_value.axis2d.value.y * combined_value.axis2d.value.y;
                        if (new_mag > old_mag) {
                            combined_value.axis2d.value = binding_value.axis2d.value;
                            any_active = true;
                        }
                        break;
                    }

                    default:
                        break;
                }
            }

            action->update_value(combined_value);
            if (any_active) m_stats.actions_triggered++;
        }

        context->update();
    }
}

InputValue InputSystem::evaluate_binding(const BindingSource& binding) {
    InputValue result;

    switch (binding.device) {
        case InputDevice::Keyboard: {
            if (auto* key = std::get_if<KeyCode>(&binding.source)) {
                // Check modifiers
                if (binding.modifiers != ModifierFlags::None) {
                    if (!has_modifier(m_keyboard.modifiers, binding.modifiers)) {
                        return result;
                    }
                }

                result = InputValue::from_button(m_keyboard.is_pressed(*key));
            }
            break;
        }

        case InputDevice::Mouse: {
            if (auto* btn = std::get_if<MouseButton>(&binding.source)) {
                result = InputValue::from_button(m_mouse.is_pressed(*btn));
            }
            break;
        }

        case InputDevice::Gamepad: {
            // Try button first
            if (auto* btn = std::get_if<GamepadButton>(&binding.source)) {
                result = InputValue::from_button(m_gamepads[0].is_pressed(*btn));
            }
            // Single axis
            else if (auto* axis = std::get_if<GamepadAxis>(&binding.source)) {
                float value = m_gamepads[0].get_axis(*axis);
                if (binding.invert) value = -value;
                value *= binding.scale;
                result = InputValue::from_axis1d(value);
            }
            // Dual axis (stick)
            else if (auto* stick = std::get_if<std::pair<GamepadAxis, GamepadAxis>>(&binding.source)) {
                float x = m_gamepads[0].get_axis(stick->first);
                float y = m_gamepads[0].get_axis(stick->second);
                if (binding.invert) y = -y;
                result = InputValue::from_axis2d({x * binding.scale, y * binding.scale});
            }
            break;
        }

        default:
            break;
    }

    return result;
}

} // namespace void_input
