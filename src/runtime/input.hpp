/// @file input.hpp
/// @brief Input handling for void_runtime

#pragma once

#include "fwd.hpp"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_runtime {

// =============================================================================
// Key Codes
// =============================================================================

/// @brief Keyboard key codes
enum class Key {
    Unknown = -1,

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
    KP0 = 320, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,

    MaxKey = 512
};

/// @brief Modifier keys
enum class Modifier : std::uint8_t {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,
    CapsLock = 1 << 4,
    NumLock = 1 << 5
};

inline Modifier operator|(Modifier a, Modifier b) {
    return static_cast<Modifier>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline Modifier operator&(Modifier a, Modifier b) {
    return static_cast<Modifier>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

// =============================================================================
// Input Events
// =============================================================================

/// @brief Input event types
enum class InputEventType {
    KeyPressed,
    KeyReleased,
    KeyHeld,
    CharInput,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled,
    GamepadButtonPressed,
    GamepadButtonReleased,
    GamepadAxisMoved,
    GamepadConnected,
    GamepadDisconnected,
    TouchBegan,
    TouchMoved,
    TouchEnded,
    TouchCancelled
};

/// @brief Input event data
struct InputEvent {
    InputEventType type;
    double timestamp;

    union {
        struct {
            Key key;
            int scancode;
            Modifier modifiers;
            bool repeat;
        } keyboard;

        struct {
            unsigned int codepoint;
        } character;

        struct {
            MouseButton button;
            Modifier modifiers;
            double x, y;
        } mouse_button;

        struct {
            double x, y;
            double dx, dy;
        } mouse_move;

        struct {
            double x_offset;
            double y_offset;
        } mouse_scroll;

        struct {
            InputDeviceId gamepad_id;
            GamepadButton button;
        } gamepad_button;

        struct {
            InputDeviceId gamepad_id;
            GamepadAxis axis;
            float value;
        } gamepad_axis;

        struct {
            InputDeviceId gamepad_id;
            const char* name;
        } gamepad_connection;

        struct {
            int touch_id;
            double x, y;
            double pressure;
        } touch;
    } data;
};

// =============================================================================
// Input Action Binding
// =============================================================================

/// @brief Input binding type
enum class InputBindingType {
    Key,
    MouseButton,
    MouseAxis,
    GamepadButton,
    GamepadAxis
};

/// @brief Input binding specification
struct InputBinding {
    InputBindingType type;

    union {
        Key key;
        MouseButton mouse_button;
        int mouse_axis;  // 0=X, 1=Y, 2=ScrollX, 3=ScrollY
        GamepadButton gamepad_button;
        GamepadAxis gamepad_axis;
    } source;

    Modifier required_modifiers = Modifier::None;
    float scale = 1.0f;
    float deadzone = 0.1f;
    bool inverted = false;
};

/// @brief Input action configuration
struct InputAction {
    std::string name;
    std::vector<InputBinding> bindings;
    bool is_axis = false;  // Continuous value vs. binary
};

// =============================================================================
// Input Manager
// =============================================================================

/// @brief Input management system
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Non-copyable
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /// @brief Initialize input system
    bool initialize();

    /// @brief Shutdown input system
    void shutdown();

    /// @brief Update input state (call once per frame)
    void update();

    /// @brief Clear input state
    void clear();

    // ==========================================================================
    // Keyboard Input
    // ==========================================================================

    /// @brief Check if key is currently pressed
    bool is_key_down(Key key) const;

    /// @brief Check if key was pressed this frame
    bool is_key_pressed(Key key) const;

    /// @brief Check if key was released this frame
    bool is_key_released(Key key) const;

    /// @brief Check if key is being held
    bool is_key_held(Key key) const;

    /// @brief Get current modifier state
    Modifier modifiers() const { return current_modifiers_; }

    /// @brief Check if modifier is active
    bool is_modifier_active(Modifier mod) const;

    /// @brief Get key name
    static std::string key_name(Key key);

    /// @brief Get key from name
    static Key key_from_name(const std::string& name);

    // ==========================================================================
    // Mouse Input
    // ==========================================================================

    /// @brief Check if mouse button is down
    bool is_mouse_button_down(MouseButton button) const;

    /// @brief Check if mouse button was pressed this frame
    bool is_mouse_button_pressed(MouseButton button) const;

    /// @brief Check if mouse button was released this frame
    bool is_mouse_button_released(MouseButton button) const;

    /// @brief Get mouse position
    void get_mouse_position(double& x, double& y) const;
    double mouse_x() const;
    double mouse_y() const;

    /// @brief Get mouse delta this frame
    void get_mouse_delta(double& dx, double& dy) const;
    double mouse_dx() const;
    double mouse_dy() const;

    /// @brief Get scroll delta this frame
    void get_scroll_delta(double& x, double& y) const;
    double scroll_x() const;
    double scroll_y() const;

    // ==========================================================================
    // Gamepad Input
    // ==========================================================================

    /// @brief Get number of connected gamepads
    std::size_t gamepad_count() const;

    /// @brief Check if gamepad is connected
    bool is_gamepad_connected(InputDeviceId id) const;

    /// @brief Get gamepad name
    std::string gamepad_name(InputDeviceId id) const;

    /// @brief Check if gamepad button is down
    bool is_gamepad_button_down(InputDeviceId id, GamepadButton button) const;

    /// @brief Check if gamepad button was pressed this frame
    bool is_gamepad_button_pressed(InputDeviceId id, GamepadButton button) const;

    /// @brief Check if gamepad button was released this frame
    bool is_gamepad_button_released(InputDeviceId id, GamepadButton button) const;

    /// @brief Get gamepad axis value (-1 to 1)
    float gamepad_axis(InputDeviceId id, GamepadAxis axis) const;

    /// @brief Vibrate gamepad (0-1 intensity)
    void vibrate_gamepad(InputDeviceId id, float left_motor, float right_motor,
                         float duration_seconds = 0.0f);

    // ==========================================================================
    // Action Binding System
    // ==========================================================================

    /// @brief Register an input action
    void register_action(const InputAction& action);

    /// @brief Unregister an input action
    void unregister_action(const std::string& name);

    /// @brief Check if action is active (button-like)
    bool is_action_pressed(const std::string& name) const;
    bool is_action_just_pressed(const std::string& name) const;
    bool is_action_just_released(const std::string& name) const;

    /// @brief Get action value (axis-like, -1 to 1)
    float get_action_value(const std::string& name) const;

    /// @brief Get action raw value (before deadzone/scaling)
    float get_action_raw_value(const std::string& name) const;

    /// @brief Load action bindings from file
    bool load_bindings(const std::string& filepath);

    /// @brief Save action bindings to file
    bool save_bindings(const std::string& filepath) const;

    // ==========================================================================
    // Event Handling
    // ==========================================================================

    /// @brief Set input event callback
    void set_event_callback(InputEventCallback callback);

    /// @brief Process an input event (called by window system)
    void process_event(const InputEvent& event);

    // ==========================================================================
    // Text Input
    // ==========================================================================

    /// @brief Enable text input mode
    void start_text_input();

    /// @brief Disable text input mode
    void stop_text_input();

    /// @brief Check if text input is active
    bool is_text_input_active() const;

    /// @brief Get text input this frame
    const std::string& text_input() const { return text_input_buffer_; }

    // ==========================================================================
    // Clipboard
    // ==========================================================================

    /// @brief Get clipboard text
    std::string get_clipboard() const;

    /// @brief Set clipboard text
    void set_clipboard(const std::string& text);

private:
    // Keyboard state
    std::array<KeyState, static_cast<int>(Key::MaxKey)> key_states_;
    std::array<KeyState, static_cast<int>(Key::MaxKey)> prev_key_states_;
    Modifier current_modifiers_ = Modifier::None;

    // Mouse state
    std::array<KeyState, 8> mouse_button_states_;
    std::array<KeyState, 8> prev_mouse_button_states_;
    double mouse_x_ = 0.0, mouse_y_ = 0.0;
    double prev_mouse_x_ = 0.0, prev_mouse_y_ = 0.0;
    double scroll_x_ = 0.0, scroll_y_ = 0.0;

    // Gamepad state
    struct GamepadState {
        bool connected = false;
        std::string name;
        std::array<KeyState, 16> button_states;
        std::array<KeyState, 16> prev_button_states;
        std::array<float, 8> axis_values;
    };
    std::array<GamepadState, 8> gamepad_states_;

    // Action bindings
    std::unordered_map<std::string, InputAction> actions_;

    // Text input
    bool text_input_active_ = false;
    std::string text_input_buffer_;

    // Event callback
    InputEventCallback event_callback_;

    // Helper methods
    float evaluate_binding(const InputBinding& binding) const;
};

} // namespace void_runtime
