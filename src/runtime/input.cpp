/// @file input.cpp
/// @brief Input handling implementation for void_runtime

#include "input.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#endif

namespace void_runtime {

// =============================================================================
// InputManager Implementation
// =============================================================================

InputManager::InputManager() {
    // Initialize all states to released
    key_states_.fill(KeyState::Released);
    prev_key_states_.fill(KeyState::Released);
    mouse_button_states_.fill(KeyState::Released);
    prev_mouse_button_states_.fill(KeyState::Released);

    for (auto& gamepad : gamepad_states_) {
        gamepad.button_states.fill(KeyState::Released);
        gamepad.prev_button_states.fill(KeyState::Released);
        gamepad.axis_values.fill(0.0f);
    }
}

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::initialize() {
    return true;
}

void InputManager::shutdown() {
    clear();
}

void InputManager::update() {
    // Store previous states
    prev_key_states_ = key_states_;
    prev_mouse_button_states_ = mouse_button_states_;

    for (auto& gamepad : gamepad_states_) {
        gamepad.prev_button_states = gamepad.button_states;
    }

    // Reset per-frame values
    scroll_x_ = 0.0;
    scroll_y_ = 0.0;
    text_input_buffer_.clear();

    // Update held states (pressed -> held)
    for (auto& state : key_states_) {
        if (state == KeyState::Pressed) {
            state = KeyState::Held;
        }
    }

    for (auto& state : mouse_button_states_) {
        if (state == KeyState::Pressed) {
            state = KeyState::Held;
        }
    }

    for (auto& gamepad : gamepad_states_) {
        for (auto& state : gamepad.button_states) {
            if (state == KeyState::Pressed) {
                state = KeyState::Held;
            }
        }
    }

    // Poll gamepad state
#ifdef _WIN32
    for (DWORD i = 0; i < XUSER_MAX_COUNT && i < 8; ++i) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(state));

        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            auto& gamepad = gamepad_states_[i];

            if (!gamepad.connected) {
                gamepad.connected = true;
                gamepad.name = "Xbox Controller " + std::to_string(i);

                // Fire connection event
                if (event_callback_) {
                    InputEvent event;
                    event.type = InputEventType::GamepadConnected;
                    event.timestamp = 0;
                    event.data.gamepad_connection.gamepad_id = i;
                    event.data.gamepad_connection.name = gamepad.name.c_str();
                    event_callback_(event);
                }
            }

            // Update buttons
            auto update_button = [&](GamepadButton btn, WORD mask) {
                bool pressed = (state.Gamepad.wButtons & mask) != 0;
                int idx = static_cast<int>(btn);
                if (pressed && gamepad.button_states[idx] == KeyState::Released) {
                    gamepad.button_states[idx] = KeyState::Pressed;
                } else if (!pressed && gamepad.button_states[idx] != KeyState::Released) {
                    gamepad.button_states[idx] = KeyState::Released;
                }
            };

            update_button(GamepadButton::A, XINPUT_GAMEPAD_A);
            update_button(GamepadButton::B, XINPUT_GAMEPAD_B);
            update_button(GamepadButton::X, XINPUT_GAMEPAD_X);
            update_button(GamepadButton::Y, XINPUT_GAMEPAD_Y);
            update_button(GamepadButton::LeftBumper, XINPUT_GAMEPAD_LEFT_SHOULDER);
            update_button(GamepadButton::RightBumper, XINPUT_GAMEPAD_RIGHT_SHOULDER);
            update_button(GamepadButton::Back, XINPUT_GAMEPAD_BACK);
            update_button(GamepadButton::Start, XINPUT_GAMEPAD_START);
            update_button(GamepadButton::LeftThumb, XINPUT_GAMEPAD_LEFT_THUMB);
            update_button(GamepadButton::RightThumb, XINPUT_GAMEPAD_RIGHT_THUMB);
            update_button(GamepadButton::DPadUp, XINPUT_GAMEPAD_DPAD_UP);
            update_button(GamepadButton::DPadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
            update_button(GamepadButton::DPadDown, XINPUT_GAMEPAD_DPAD_DOWN);
            update_button(GamepadButton::DPadLeft, XINPUT_GAMEPAD_DPAD_LEFT);

            // Update axes
            auto normalize_axis = [](SHORT value, SHORT deadzone) -> float {
                if (std::abs(value) < deadzone) return 0.0f;
                float normalized = value / 32767.0f;
                return std::clamp(normalized, -1.0f, 1.0f);
            };

            gamepad.axis_values[static_cast<int>(GamepadAxis::LeftX)] =
                normalize_axis(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            gamepad.axis_values[static_cast<int>(GamepadAxis::LeftY)] =
                normalize_axis(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            gamepad.axis_values[static_cast<int>(GamepadAxis::RightX)] =
                normalize_axis(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
            gamepad.axis_values[static_cast<int>(GamepadAxis::RightY)] =
                normalize_axis(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
            gamepad.axis_values[static_cast<int>(GamepadAxis::LeftTrigger)] =
                state.Gamepad.bLeftTrigger / 255.0f;
            gamepad.axis_values[static_cast<int>(GamepadAxis::RightTrigger)] =
                state.Gamepad.bRightTrigger / 255.0f;

        } else if (gamepad_states_[i].connected) {
            gamepad_states_[i].connected = false;

            if (event_callback_) {
                InputEvent event;
                event.type = InputEventType::GamepadDisconnected;
                event.timestamp = 0;
                event.data.gamepad_connection.gamepad_id = i;
                event_callback_(event);
            }
        }
    }
#endif

    // Store mouse delta
    prev_mouse_x_ = mouse_x_;
    prev_mouse_y_ = mouse_y_;
}

void InputManager::clear() {
    key_states_.fill(KeyState::Released);
    prev_key_states_.fill(KeyState::Released);
    mouse_button_states_.fill(KeyState::Released);
    prev_mouse_button_states_.fill(KeyState::Released);
    current_modifiers_ = Modifier::None;
    scroll_x_ = scroll_y_ = 0.0;
    text_input_buffer_.clear();
}

// =============================================================================
// Keyboard Input
// =============================================================================

bool InputManager::is_key_down(Key key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= static_cast<int>(key_states_.size())) return false;
    return key_states_[idx] == KeyState::Pressed || key_states_[idx] == KeyState::Held;
}

bool InputManager::is_key_pressed(Key key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= static_cast<int>(key_states_.size())) return false;
    return key_states_[idx] == KeyState::Pressed ||
           (key_states_[idx] == KeyState::Held && prev_key_states_[idx] == KeyState::Released);
}

bool InputManager::is_key_released(Key key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= static_cast<int>(key_states_.size())) return false;
    return key_states_[idx] == KeyState::Released && prev_key_states_[idx] != KeyState::Released;
}

bool InputManager::is_key_held(Key key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= static_cast<int>(key_states_.size())) return false;
    return key_states_[idx] == KeyState::Held;
}

bool InputManager::is_modifier_active(Modifier mod) const {
    return (static_cast<std::uint8_t>(current_modifiers_) &
            static_cast<std::uint8_t>(mod)) != 0;
}

std::string InputManager::key_name(Key key) {
    switch (key) {
        case Key::Space: return "Space";
        case Key::Apostrophe: return "Apostrophe";
        case Key::Comma: return "Comma";
        case Key::Minus: return "Minus";
        case Key::Period: return "Period";
        case Key::Slash: return "Slash";
        case Key::Num0: return "0";
        case Key::Num1: return "1";
        case Key::Num2: return "2";
        case Key::Num3: return "3";
        case Key::Num4: return "4";
        case Key::Num5: return "5";
        case Key::Num6: return "6";
        case Key::Num7: return "7";
        case Key::Num8: return "8";
        case Key::Num9: return "9";
        case Key::A: return "A";
        case Key::B: return "B";
        case Key::C: return "C";
        case Key::D: return "D";
        case Key::E: return "E";
        case Key::F: return "F";
        case Key::G: return "G";
        case Key::H: return "H";
        case Key::I: return "I";
        case Key::J: return "J";
        case Key::K: return "K";
        case Key::L: return "L";
        case Key::M: return "M";
        case Key::N: return "N";
        case Key::O: return "O";
        case Key::P: return "P";
        case Key::Q: return "Q";
        case Key::R: return "R";
        case Key::S: return "S";
        case Key::T: return "T";
        case Key::U: return "U";
        case Key::V: return "V";
        case Key::W: return "W";
        case Key::X: return "X";
        case Key::Y: return "Y";
        case Key::Z: return "Z";
        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Insert: return "Insert";
        case Key::Delete: return "Delete";
        case Key::Right: return "Right";
        case Key::Left: return "Left";
        case Key::Down: return "Down";
        case Key::Up: return "Up";
        case Key::PageUp: return "PageUp";
        case Key::PageDown: return "PageDown";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::CapsLock: return "CapsLock";
        case Key::F1: return "F1";
        case Key::F2: return "F2";
        case Key::F3: return "F3";
        case Key::F4: return "F4";
        case Key::F5: return "F5";
        case Key::F6: return "F6";
        case Key::F7: return "F7";
        case Key::F8: return "F8";
        case Key::F9: return "F9";
        case Key::F10: return "F10";
        case Key::F11: return "F11";
        case Key::F12: return "F12";
        case Key::LeftShift: return "LeftShift";
        case Key::LeftControl: return "LeftControl";
        case Key::LeftAlt: return "LeftAlt";
        case Key::RightShift: return "RightShift";
        case Key::RightControl: return "RightControl";
        case Key::RightAlt: return "RightAlt";
        default: return "Unknown";
    }
}

Key InputManager::key_from_name(const std::string& name) {
    // Simple lookup - could use a map for better performance
    if (name == "Space") return Key::Space;
    if (name == "Escape") return Key::Escape;
    if (name == "Enter") return Key::Enter;
    if (name == "Tab") return Key::Tab;
    if (name == "Backspace") return Key::Backspace;
    // ... add more as needed
    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') {
        return static_cast<Key>(static_cast<int>(Key::A) + (name[0] - 'A'));
    }
    return Key::Unknown;
}

// =============================================================================
// Mouse Input
// =============================================================================

bool InputManager::is_mouse_button_down(MouseButton button) const {
    int idx = static_cast<int>(button);
    if (idx < 0 || idx >= static_cast<int>(mouse_button_states_.size())) return false;
    return mouse_button_states_[idx] == KeyState::Pressed ||
           mouse_button_states_[idx] == KeyState::Held;
}

bool InputManager::is_mouse_button_pressed(MouseButton button) const {
    int idx = static_cast<int>(button);
    if (idx < 0 || idx >= static_cast<int>(mouse_button_states_.size())) return false;
    return mouse_button_states_[idx] == KeyState::Pressed ||
           (mouse_button_states_[idx] == KeyState::Held &&
            prev_mouse_button_states_[idx] == KeyState::Released);
}

bool InputManager::is_mouse_button_released(MouseButton button) const {
    int idx = static_cast<int>(button);
    if (idx < 0 || idx >= static_cast<int>(mouse_button_states_.size())) return false;
    return mouse_button_states_[idx] == KeyState::Released &&
           prev_mouse_button_states_[idx] != KeyState::Released;
}

void InputManager::get_mouse_position(double& x, double& y) const {
    x = mouse_x_;
    y = mouse_y_;
}

double InputManager::mouse_x() const { return mouse_x_; }
double InputManager::mouse_y() const { return mouse_y_; }

void InputManager::get_mouse_delta(double& dx, double& dy) const {
    dx = mouse_x_ - prev_mouse_x_;
    dy = mouse_y_ - prev_mouse_y_;
}

double InputManager::mouse_dx() const { return mouse_x_ - prev_mouse_x_; }
double InputManager::mouse_dy() const { return mouse_y_ - prev_mouse_y_; }

void InputManager::get_scroll_delta(double& x, double& y) const {
    x = scroll_x_;
    y = scroll_y_;
}

double InputManager::scroll_x() const { return scroll_x_; }
double InputManager::scroll_y() const { return scroll_y_; }

// =============================================================================
// Gamepad Input
// =============================================================================

std::size_t InputManager::gamepad_count() const {
    std::size_t count = 0;
    for (const auto& gamepad : gamepad_states_) {
        if (gamepad.connected) count++;
    }
    return count;
}

bool InputManager::is_gamepad_connected(InputDeviceId id) const {
    return id < 8 && gamepad_states_[id].connected;
}

std::string InputManager::gamepad_name(InputDeviceId id) const {
    if (id >= 8 || !gamepad_states_[id].connected) return "";
    return gamepad_states_[id].name;
}

bool InputManager::is_gamepad_button_down(InputDeviceId id, GamepadButton button) const {
    if (id >= 8 || !gamepad_states_[id].connected) return false;
    int idx = static_cast<int>(button);
    if (idx < 0 || idx >= 16) return false;
    return gamepad_states_[id].button_states[idx] != KeyState::Released;
}

bool InputManager::is_gamepad_button_pressed(InputDeviceId id, GamepadButton button) const {
    if (id >= 8 || !gamepad_states_[id].connected) return false;
    int idx = static_cast<int>(button);
    if (idx < 0 || idx >= 16) return false;
    return gamepad_states_[id].button_states[idx] == KeyState::Pressed ||
           (gamepad_states_[id].button_states[idx] == KeyState::Held &&
            gamepad_states_[id].prev_button_states[idx] == KeyState::Released);
}

bool InputManager::is_gamepad_button_released(InputDeviceId id, GamepadButton button) const {
    if (id >= 8 || !gamepad_states_[id].connected) return false;
    int idx = static_cast<int>(button);
    if (idx < 0 || idx >= 16) return false;
    return gamepad_states_[id].button_states[idx] == KeyState::Released &&
           gamepad_states_[id].prev_button_states[idx] != KeyState::Released;
}

float InputManager::gamepad_axis(InputDeviceId id, GamepadAxis axis) const {
    if (id >= 8 || !gamepad_states_[id].connected) return 0.0f;
    int idx = static_cast<int>(axis);
    if (idx < 0 || idx >= 8) return 0.0f;
    return gamepad_states_[id].axis_values[idx];
}

void InputManager::vibrate_gamepad(InputDeviceId id, float left_motor, float right_motor,
                                    float duration_seconds) {
#ifdef _WIN32
    if (id >= XUSER_MAX_COUNT) return;

    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = static_cast<WORD>(left_motor * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(right_motor * 65535.0f);
    XInputSetState(id, &vibration);
#endif
}

// =============================================================================
// Action Binding System
// =============================================================================

void InputManager::register_action(const InputAction& action) {
    actions_[action.name] = action;
}

void InputManager::unregister_action(const std::string& name) {
    actions_.erase(name);
}

bool InputManager::is_action_pressed(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return false;

    for (const auto& binding : it->second.bindings) {
        float value = evaluate_binding(binding);
        if (std::abs(value) > binding.deadzone) {
            return true;
        }
    }
    return false;
}

bool InputManager::is_action_just_pressed(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return false;

    for (const auto& binding : it->second.bindings) {
        if (binding.type == InputBindingType::Key) {
            if (is_key_pressed(binding.source.key)) {
                if ((binding.required_modifiers == Modifier::None) ||
                    is_modifier_active(binding.required_modifiers)) {
                    return true;
                }
            }
        } else if (binding.type == InputBindingType::MouseButton) {
            if (is_mouse_button_pressed(binding.source.mouse_button)) {
                return true;
            }
        } else if (binding.type == InputBindingType::GamepadButton) {
            for (InputDeviceId id = 0; id < 8; ++id) {
                if (is_gamepad_button_pressed(id, binding.source.gamepad_button)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool InputManager::is_action_just_released(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return false;

    for (const auto& binding : it->second.bindings) {
        if (binding.type == InputBindingType::Key) {
            if (is_key_released(binding.source.key)) {
                return true;
            }
        } else if (binding.type == InputBindingType::MouseButton) {
            if (is_mouse_button_released(binding.source.mouse_button)) {
                return true;
            }
        } else if (binding.type == InputBindingType::GamepadButton) {
            for (InputDeviceId id = 0; id < 8; ++id) {
                if (is_gamepad_button_released(id, binding.source.gamepad_button)) {
                    return true;
                }
            }
        }
    }
    return false;
}

float InputManager::get_action_value(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return 0.0f;

    float result = 0.0f;
    for (const auto& binding : it->second.bindings) {
        float value = evaluate_binding(binding);

        // Apply deadzone
        if (std::abs(value) < binding.deadzone) {
            value = 0.0f;
        } else {
            // Rescale to 0-1 range after deadzone
            float sign = value > 0 ? 1.0f : -1.0f;
            value = sign * (std::abs(value) - binding.deadzone) / (1.0f - binding.deadzone);
        }

        // Apply scale and inversion
        value *= binding.scale;
        if (binding.inverted) value = -value;

        // Take maximum magnitude
        if (std::abs(value) > std::abs(result)) {
            result = value;
        }
    }

    return result;
}

float InputManager::get_action_raw_value(const std::string& name) const {
    auto it = actions_.find(name);
    if (it == actions_.end()) return 0.0f;

    float result = 0.0f;
    for (const auto& binding : it->second.bindings) {
        float value = evaluate_binding(binding);
        if (std::abs(value) > std::abs(result)) {
            result = value;
        }
    }
    return result;
}

float InputManager::evaluate_binding(const InputBinding& binding) const {
    switch (binding.type) {
        case InputBindingType::Key:
            if ((binding.required_modifiers == Modifier::None) ||
                is_modifier_active(binding.required_modifiers)) {
                return is_key_down(binding.source.key) ? 1.0f : 0.0f;
            }
            return 0.0f;

        case InputBindingType::MouseButton:
            return is_mouse_button_down(binding.source.mouse_button) ? 1.0f : 0.0f;

        case InputBindingType::MouseAxis:
            switch (binding.source.mouse_axis) {
                case 0: return static_cast<float>(mouse_dx());
                case 1: return static_cast<float>(mouse_dy());
                case 2: return static_cast<float>(scroll_x());
                case 3: return static_cast<float>(scroll_y());
                default: return 0.0f;
            }

        case InputBindingType::GamepadButton:
            for (InputDeviceId id = 0; id < 8; ++id) {
                if (is_gamepad_button_down(id, binding.source.gamepad_button)) {
                    return 1.0f;
                }
            }
            return 0.0f;

        case InputBindingType::GamepadAxis:
            for (InputDeviceId id = 0; id < 8; ++id) {
                if (is_gamepad_connected(id)) {
                    float value = gamepad_axis(id, binding.source.gamepad_axis);
                    if (std::abs(value) > 0.0f) {
                        return value;
                    }
                }
            }
            return 0.0f;
    }

    return 0.0f;
}

bool InputManager::load_bindings(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;

    // Simple format: action_name key:A,gamepad_button:A
    // Real implementation would use JSON or similar
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string action_name;
        iss >> action_name;

        InputAction action;
        action.name = action_name;

        std::string binding_str;
        while (iss >> binding_str) {
            InputBinding binding;

            if (binding_str.find("key:") == 0) {
                binding.type = InputBindingType::Key;
                binding.source.key = key_from_name(binding_str.substr(4));
                action.bindings.push_back(binding);
            }
            // Add more binding types...
        }

        if (!action.bindings.empty()) {
            register_action(action);
        }
    }

    return true;
}

bool InputManager::save_bindings(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file) return false;

    file << "# Input bindings\n";
    for (const auto& [name, action] : actions_) {
        file << name;
        for (const auto& binding : action.bindings) {
            if (binding.type == InputBindingType::Key) {
                file << " key:" << key_name(binding.source.key);
            }
            // Add more binding types...
        }
        file << "\n";
    }

    return true;
}

// =============================================================================
// Event Handling
// =============================================================================

void InputManager::set_event_callback(InputEventCallback callback) {
    event_callback_ = std::move(callback);
}

void InputManager::process_event(const InputEvent& event) {
    switch (event.type) {
        case InputEventType::KeyPressed:
        case InputEventType::KeyHeld: {
            int idx = static_cast<int>(event.data.keyboard.key);
            if (idx >= 0 && idx < static_cast<int>(key_states_.size())) {
                if (key_states_[idx] == KeyState::Released) {
                    key_states_[idx] = KeyState::Pressed;
                }
            }
            current_modifiers_ = event.data.keyboard.modifiers;
            break;
        }

        case InputEventType::KeyReleased: {
            int idx = static_cast<int>(event.data.keyboard.key);
            if (idx >= 0 && idx < static_cast<int>(key_states_.size())) {
                key_states_[idx] = KeyState::Released;
            }
            current_modifiers_ = event.data.keyboard.modifiers;
            break;
        }

        case InputEventType::CharInput:
            if (text_input_active_) {
                // Convert codepoint to UTF-8
                unsigned int cp = event.data.character.codepoint;
                if (cp < 0x80) {
                    text_input_buffer_ += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    text_input_buffer_ += static_cast<char>(0xC0 | (cp >> 6));
                    text_input_buffer_ += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    text_input_buffer_ += static_cast<char>(0xE0 | (cp >> 12));
                    text_input_buffer_ += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    text_input_buffer_ += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    text_input_buffer_ += static_cast<char>(0xF0 | (cp >> 18));
                    text_input_buffer_ += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    text_input_buffer_ += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    text_input_buffer_ += static_cast<char>(0x80 | (cp & 0x3F));
                }
            }
            break;

        case InputEventType::MouseButtonPressed: {
            int idx = static_cast<int>(event.data.mouse_button.button);
            if (idx >= 0 && idx < static_cast<int>(mouse_button_states_.size())) {
                mouse_button_states_[idx] = KeyState::Pressed;
            }
            break;
        }

        case InputEventType::MouseButtonReleased: {
            int idx = static_cast<int>(event.data.mouse_button.button);
            if (idx >= 0 && idx < static_cast<int>(mouse_button_states_.size())) {
                mouse_button_states_[idx] = KeyState::Released;
            }
            break;
        }

        case InputEventType::MouseMoved:
            mouse_x_ = event.data.mouse_move.x;
            mouse_y_ = event.data.mouse_move.y;
            break;

        case InputEventType::MouseScrolled:
            scroll_x_ += event.data.mouse_scroll.x_offset;
            scroll_y_ += event.data.mouse_scroll.y_offset;
            break;

        default:
            break;
    }

    // Forward to callback
    if (event_callback_) {
        event_callback_(event);
    }
}

// =============================================================================
// Text Input
// =============================================================================

void InputManager::start_text_input() {
    text_input_active_ = true;
    text_input_buffer_.clear();
}

void InputManager::stop_text_input() {
    text_input_active_ = false;
}

bool InputManager::is_text_input_active() const {
    return text_input_active_;
}

// =============================================================================
// Clipboard
// =============================================================================

std::string InputManager::get_clipboard() const {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return "";

    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return "";
    }

    wchar_t* text = static_cast<wchar_t*>(GlobalLock(data));
    if (!text) {
        CloseClipboard();
        return "";
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);

    GlobalUnlock(data);
    CloseClipboard();

    return result;
#else
    return "";
#endif
}

void InputManager::set_clipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;

    EmptyClipboard();

    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size * sizeof(wchar_t));
    if (!data) {
        CloseClipboard();
        return;
    }

    wchar_t* dest = static_cast<wchar_t*>(GlobalLock(data));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, dest, size);
    GlobalUnlock(data);

    SetClipboardData(CF_UNICODETEXT, data);
    CloseClipboard();
#endif
}

} // namespace void_runtime
