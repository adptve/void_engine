/// @file types.hpp
/// @brief Core type definitions for void_input
///
/// STATUS: PRODUCTION (2026-01-28)
/// - Complete key code mapping (GLFW compatible)
/// - Mouse and gamepad button definitions
/// - Gamepad axis definitions
/// - Action types for input mapping

#pragma once

#include "fwd.hpp"
#include <void_engine/math/math.hpp>
#include <array>
#include <string>
#include <functional>
#include <vector>

namespace void_input {

// =============================================================================
// Input Device Types
// =============================================================================

/// Input device types
enum class InputDevice : std::uint8_t {
    Keyboard,
    Mouse,
    Gamepad,
    Touch,
    Custom
};

/// Input state
enum class InputState : std::uint8_t {
    Released = 0,
    Pressed = 1,
    Held = 2,
    JustPressed = 3,
    JustReleased = 4
};

// =============================================================================
// Key Codes (GLFW Compatible)
// =============================================================================

enum class KeyCode : std::uint16_t {
    Unknown = 0,

    // Printable keys
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,

    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Semicolon = 59,
    Equal = 61,

    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,

    World1 = 161,
    World2 = 162,

    // Function keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,

    // Keypad
    Kp0 = 320, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
    KpDecimal = 330,
    KpDivide = 331,
    KpMultiply = 332,
    KpSubtract = 333,
    KpAdd = 334,
    KpEnter = 335,
    KpEqual = 336,

    // Modifiers
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,

    Last = Menu
};

// =============================================================================
// Mouse Buttons
// =============================================================================

enum class MouseButton : std::uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,
    Last = Button8
};

// =============================================================================
// Gamepad Buttons (Xbox Layout)
// =============================================================================

enum class GamepadButton : std::uint8_t {
    A = 0,              // Cross (PS)
    B = 1,              // Circle (PS)
    X = 2,              // Square (PS)
    Y = 3,              // Triangle (PS)
    LeftBumper = 4,     // L1
    RightBumper = 5,    // R1
    Back = 6,           // Select/Share
    Start = 7,          // Options
    Guide = 8,          // PS/Xbox button
    LeftThumb = 9,      // L3
    RightThumb = 10,    // R3
    DpadUp = 11,
    DpadRight = 12,
    DpadDown = 13,
    DpadLeft = 14,
    Last = DpadLeft
};

// =============================================================================
// Gamepad Axes
// =============================================================================

enum class GamepadAxis : std::uint8_t {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5,
    Last = RightTrigger
};

// =============================================================================
// Action Types
// =============================================================================

enum class ActionType : std::uint8_t {
    Button,     ///< Digital on/off (key press, button press)
    Axis1D,     ///< Single axis value (-1 to 1)
    Axis2D,     ///< Two axis values (stick, mouse delta)
    Axis3D      ///< Three axis values (6DOF controller)
};

// =============================================================================
// Modifier Flags
// =============================================================================

enum class ModifierFlags : std::uint8_t {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,
    CapsLock = 1 << 4,
    NumLock = 1 << 5
};

inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) {
    return static_cast<ModifierFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline ModifierFlags operator&(ModifierFlags a, ModifierFlags b) {
    return static_cast<ModifierFlags>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline bool has_modifier(ModifierFlags flags, ModifierFlags check) {
    return (flags & check) == check;
}

// =============================================================================
// Input Values
// =============================================================================

/// Value for a button input
struct ButtonValue {
    bool pressed = false;
    bool just_pressed = false;
    bool just_released = false;
    float pressure = 0.0f;      ///< For analog buttons (triggers)
};

/// Value for 1D axis
struct Axis1DValue {
    float value = 0.0f;
    float delta = 0.0f;
    float raw = 0.0f;           ///< Before deadzone/curve
};

/// Value for 2D axis (stick, mouse)
struct Axis2DValue {
    void_math::Vec2 value{0, 0};
    void_math::Vec2 delta{0, 0};
    void_math::Vec2 raw{0, 0};
};

/// Combined input value
struct InputValue {
    ActionType type = ActionType::Button;

    union {
        ButtonValue button;
        Axis1DValue axis1d;
        Axis2DValue axis2d;
    };

    InputValue() : button{} {}

    static InputValue from_button(bool pressed, bool just_pressed = false, bool just_released = false) {
        InputValue v;
        v.type = ActionType::Button;
        v.button.pressed = pressed;
        v.button.just_pressed = just_pressed;
        v.button.just_released = just_released;
        return v;
    }

    static InputValue from_axis1d(float value, float delta = 0.0f) {
        InputValue v;
        v.type = ActionType::Axis1D;
        v.axis1d.value = value;
        v.axis1d.delta = delta;
        return v;
    }

    static InputValue from_axis2d(void_math::Vec2 value, void_math::Vec2 delta = {0, 0}) {
        InputValue v;
        v.type = ActionType::Axis2D;
        v.axis2d.value = value;
        v.axis2d.delta = delta;
        return v;
    }
};

// =============================================================================
// Keyboard State
// =============================================================================

struct KeyboardState {
    static constexpr std::size_t KEY_COUNT = 512;

    std::array<bool, KEY_COUNT> keys{};
    std::array<bool, KEY_COUNT> previous_keys{};
    ModifierFlags modifiers = ModifierFlags::None;

    [[nodiscard]] bool is_pressed(KeyCode key) const {
        auto k = static_cast<std::size_t>(key);
        return k < KEY_COUNT && keys[k];
    }

    [[nodiscard]] bool is_just_pressed(KeyCode key) const {
        auto k = static_cast<std::size_t>(key);
        return k < KEY_COUNT && keys[k] && !previous_keys[k];
    }

    [[nodiscard]] bool is_just_released(KeyCode key) const {
        auto k = static_cast<std::size_t>(key);
        return k < KEY_COUNT && !keys[k] && previous_keys[k];
    }

    void update() {
        previous_keys = keys;
    }
};

// =============================================================================
// Mouse State
// =============================================================================

struct MouseState {
    void_math::Vec2 position{0, 0};
    void_math::Vec2 previous_position{0, 0};
    void_math::Vec2 delta{0, 0};
    void_math::Vec2 scroll{0, 0};

    std::array<bool, 8> buttons{};
    std::array<bool, 8> previous_buttons{};

    bool captured = false;
    bool visible = true;

    [[nodiscard]] bool is_pressed(MouseButton button) const {
        auto b = static_cast<std::size_t>(button);
        return b < 8 && buttons[b];
    }

    [[nodiscard]] bool is_just_pressed(MouseButton button) const {
        auto b = static_cast<std::size_t>(button);
        return b < 8 && buttons[b] && !previous_buttons[b];
    }

    [[nodiscard]] bool is_just_released(MouseButton button) const {
        auto b = static_cast<std::size_t>(button);
        return b < 8 && !buttons[b] && previous_buttons[b];
    }

    void update() {
        previous_position = position;
        previous_buttons = buttons;
        delta = {0, 0};
        scroll = {0, 0};
    }
};

// =============================================================================
// Gamepad State
// =============================================================================

struct GamepadState {
    bool connected = false;
    std::string name;

    std::array<bool, 15> buttons{};
    std::array<bool, 15> previous_buttons{};
    std::array<float, 6> axes{};
    std::array<float, 6> previous_axes{};

    float deadzone_inner = 0.1f;
    float deadzone_outer = 0.9f;

    [[nodiscard]] bool is_pressed(GamepadButton button) const {
        auto b = static_cast<std::size_t>(button);
        return b < 15 && buttons[b];
    }

    [[nodiscard]] bool is_just_pressed(GamepadButton button) const {
        auto b = static_cast<std::size_t>(button);
        return b < 15 && buttons[b] && !previous_buttons[b];
    }

    [[nodiscard]] bool is_just_released(GamepadButton button) const {
        auto b = static_cast<std::size_t>(button);
        return b < 15 && !buttons[b] && previous_buttons[b];
    }

    [[nodiscard]] float get_axis(GamepadAxis axis) const {
        auto a = static_cast<std::size_t>(axis);
        if (a >= 6) return 0.0f;

        float raw = axes[a];

        // Apply deadzone
        float abs_val = std::abs(raw);
        if (abs_val < deadzone_inner) return 0.0f;
        if (abs_val > deadzone_outer) return raw > 0 ? 1.0f : -1.0f;

        // Remap to 0-1 range within deadzone
        float sign = raw > 0 ? 1.0f : -1.0f;
        return sign * (abs_val - deadzone_inner) / (deadzone_outer - deadzone_inner);
    }

    [[nodiscard]] void_math::Vec2 get_stick(bool right = false) const {
        if (right) {
            return {get_axis(GamepadAxis::RightX), get_axis(GamepadAxis::RightY)};
        }
        return {get_axis(GamepadAxis::LeftX), get_axis(GamepadAxis::LeftY)};
    }

    void update() {
        previous_buttons = buttons;
        previous_axes = axes;
    }
};

// =============================================================================
// Input Configuration
// =============================================================================

struct InputConfig {
    float mouse_sensitivity = 1.0f;
    float scroll_sensitivity = 1.0f;
    bool invert_y = false;
    float gamepad_deadzone_inner = 0.1f;
    float gamepad_deadzone_outer = 0.9f;
    bool raw_mouse_input = true;

    static InputConfig defaults() { return InputConfig{}; }
};

// =============================================================================
// Input Statistics
// =============================================================================

struct InputStats {
    std::uint32_t key_events = 0;
    std::uint32_t mouse_events = 0;
    std::uint32_t gamepad_events = 0;
    std::uint32_t actions_triggered = 0;
    std::uint32_t connected_gamepads = 0;
};

// =============================================================================
// String Conversions
// =============================================================================

const char* to_string(KeyCode key);
const char* to_string(MouseButton button);
const char* to_string(GamepadButton button);
const char* to_string(GamepadAxis axis);
const char* to_string(ActionType type);

KeyCode key_from_string(const std::string& name);
MouseButton mouse_button_from_string(const std::string& name);
GamepadButton gamepad_button_from_string(const std::string& name);

} // namespace void_input
