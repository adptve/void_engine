#pragma once

/// @file input.hpp
/// @brief Input event handling
///
/// Provides a unified input API for keyboard, mouse, touch, and device events.

#include "fwd.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>

namespace void_compositor {

// =============================================================================
// Basic Types
// =============================================================================

/// 2D vector for input positions
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }

    [[nodiscard]] float length_squared() const { return x * x + y * y; }
    [[nodiscard]] float length() const;
};

// =============================================================================
// Key State
// =============================================================================

/// Key state
enum class KeyState : std::uint8_t {
    Pressed,
    Released,
};

/// Button state
enum class ButtonState : std::uint8_t {
    Pressed,
    Released,
};

// =============================================================================
// Keyboard Modifiers
// =============================================================================

/// Keyboard modifiers
struct Modifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool logo = false;  // Windows/Super/Command key
    bool caps_lock = false;
    bool num_lock = false;

    [[nodiscard]] bool none() const {
        return !shift && !ctrl && !alt && !logo;
    }

    [[nodiscard]] bool any() const {
        return shift || ctrl || alt || logo;
    }
};

// =============================================================================
// Keyboard Events
// =============================================================================

/// Keyboard event
struct KeyboardEvent {
    /// Key code (hardware-specific scancode)
    std::uint32_t keycode = 0;
    /// Key state
    KeyState state = KeyState::Released;
    /// Timestamp in milliseconds
    std::uint32_t time_ms = 0;
    /// Modifier state at time of event
    Modifiers modifiers;
};

// =============================================================================
// Pointer Events
// =============================================================================

/// Mouse button
enum class PointerButton : std::uint32_t {
    Left = 0x110,    // BTN_LEFT
    Right = 0x111,   // BTN_RIGHT
    Middle = 0x112,  // BTN_MIDDLE
    Back = 0x113,
    Forward = 0x114,
};

/// Convert raw button code to PointerButton
[[nodiscard]] inline PointerButton pointer_button_from_code(std::uint32_t code) {
    switch (code) {
        case 0x110: return PointerButton::Left;
        case 0x111: return PointerButton::Right;
        case 0x112: return PointerButton::Middle;
        case 0x113: return PointerButton::Back;
        case 0x114: return PointerButton::Forward;
        default: return static_cast<PointerButton>(code);
    }
}

/// Axis event source
enum class AxisSource : std::uint8_t {
    /// Mouse wheel
    Wheel,
    /// Touchpad finger
    Finger,
    /// Continuous (trackball, etc.)
    Continuous,
    /// Unknown
    Unknown,
};

/// Pointer motion event
struct PointerMotionEvent {
    /// Absolute position (if known)
    std::optional<Vec2> position;
    /// Delta movement
    Vec2 delta;
    /// Timestamp in milliseconds
    std::uint32_t time_ms = 0;
};

/// Pointer button event
struct PointerButtonEvent {
    PointerButton button = PointerButton::Left;
    ButtonState state = ButtonState::Released;
    std::uint32_t time_ms = 0;
};

/// Pointer axis (scroll) event
struct PointerAxisEvent {
    /// Horizontal scroll
    double horizontal = 0.0;
    /// Vertical scroll
    double vertical = 0.0;
    /// Source (wheel, finger, etc.)
    AxisSource source = AxisSource::Unknown;
    std::uint32_t time_ms = 0;
};

/// Pointer event variant
using PointerEvent = std::variant<PointerMotionEvent, PointerButtonEvent, PointerAxisEvent>;

// =============================================================================
// Touch Events
// =============================================================================

/// Touch down event
struct TouchDownEvent {
    std::int32_t slot = 0;
    Vec2 position;
    std::uint32_t time_ms = 0;
};

/// Touch motion event
struct TouchMotionEvent {
    std::int32_t slot = 0;
    Vec2 position;
    std::uint32_t time_ms = 0;
};

/// Touch up event
struct TouchUpEvent {
    std::int32_t slot = 0;
    std::uint32_t time_ms = 0;
};

/// Touch cancel event
struct TouchCancelEvent {
    std::int32_t slot = 0;
    std::uint32_t time_ms = 0;
};

/// Touch event variant
using TouchEvent = std::variant<TouchDownEvent, TouchMotionEvent, TouchUpEvent, TouchCancelEvent>;

// =============================================================================
// Device Events
// =============================================================================

/// Input device type
enum class DeviceType : std::uint8_t {
    Keyboard,
    Pointer,
    Touch,
    Tablet,
    Other,
};

/// Device added event
struct DeviceAddedEvent {
    std::uint64_t device_id = 0;
    std::string name;
    DeviceType device_type = DeviceType::Other;
};

/// Device removed event
struct DeviceRemovedEvent {
    std::uint64_t device_id = 0;
};

/// Device event variant
using DeviceEvent = std::variant<DeviceAddedEvent, DeviceRemovedEvent>;

// =============================================================================
// Input Event
// =============================================================================

/// Input event from any device
struct InputEvent {
    enum class Type : std::uint8_t {
        Keyboard,
        Pointer,
        Touch,
        Device,
    };

    Type type;
    std::variant<KeyboardEvent, PointerEvent, TouchEvent, DeviceEvent> data;

    /// Create keyboard event
    static InputEvent keyboard(const KeyboardEvent& e) {
        return {Type::Keyboard, e};
    }

    /// Create pointer event
    static InputEvent pointer(const PointerEvent& e) {
        return {Type::Pointer, e};
    }

    /// Create touch event
    static InputEvent touch(const TouchEvent& e) {
        return {Type::Touch, e};
    }

    /// Create device event
    static InputEvent device(const DeviceEvent& e) {
        return {Type::Device, e};
    }

    /// Check event type
    [[nodiscard]] bool is_keyboard() const { return type == Type::Keyboard; }
    [[nodiscard]] bool is_pointer() const { return type == Type::Pointer; }
    [[nodiscard]] bool is_touch() const { return type == Type::Touch; }
    [[nodiscard]] bool is_device() const { return type == Type::Device; }

    /// Get keyboard event (if applicable)
    [[nodiscard]] const KeyboardEvent* as_keyboard() const {
        return is_keyboard() ? &std::get<KeyboardEvent>(data) : nullptr;
    }

    /// Get pointer event (if applicable)
    [[nodiscard]] const PointerEvent* as_pointer() const {
        return is_pointer() ? &std::get<PointerEvent>(data) : nullptr;
    }

    /// Get touch event (if applicable)
    [[nodiscard]] const TouchEvent* as_touch() const {
        return is_touch() ? &std::get<TouchEvent>(data) : nullptr;
    }

    /// Get device event (if applicable)
    [[nodiscard]] const DeviceEvent* as_device() const {
        return is_device() ? &std::get<DeviceEvent>(data) : nullptr;
    }
};

// =============================================================================
// Input State Tracker
// =============================================================================

/// Input state tracker - tracks currently pressed keys and buttons
class InputState {
public:
    InputState() = default;

    /// Process an input event
    void handle_event(const InputEvent& event) {
        if (event.is_keyboard()) {
            handle_keyboard(*event.as_keyboard());
        } else if (event.is_pointer()) {
            handle_pointer(*event.as_pointer());
        }
    }

    /// Check if a key is pressed
    [[nodiscard]] bool is_key_pressed(std::uint32_t keycode) const {
        return m_pressed_keys.count(keycode) > 0;
    }

    /// Check if a button is pressed
    [[nodiscard]] bool is_button_pressed(PointerButton button) const {
        return m_pressed_buttons.count(static_cast<std::uint32_t>(button)) > 0;
    }

    /// Get pointer position
    [[nodiscard]] Vec2 pointer_position() const { return m_pointer_position; }

    /// Get current modifiers
    [[nodiscard]] const Modifiers& modifiers() const { return m_modifiers; }

    /// Get all pressed keys
    [[nodiscard]] const std::unordered_set<std::uint32_t>& pressed_keys() const {
        return m_pressed_keys;
    }

    /// Reset all state
    void reset() {
        m_pressed_keys.clear();
        m_pressed_buttons.clear();
        m_pointer_position = Vec2{};
        m_modifiers = Modifiers{};
    }

private:
    void handle_keyboard(const KeyboardEvent& e) {
        if (e.state == KeyState::Pressed) {
            m_pressed_keys.insert(e.keycode);
        } else {
            m_pressed_keys.erase(e.keycode);
        }
        m_modifiers = e.modifiers;
    }

    void handle_pointer(const PointerEvent& e) {
        std::visit([this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, PointerMotionEvent>) {
                if (arg.position) {
                    m_pointer_position = *arg.position;
                }
            } else if constexpr (std::is_same_v<T, PointerButtonEvent>) {
                auto btn = static_cast<std::uint32_t>(arg.button);
                if (arg.state == ButtonState::Pressed) {
                    m_pressed_buttons.insert(btn);
                } else {
                    m_pressed_buttons.erase(btn);
                }
            }
        }, e);
    }

private:
    std::unordered_set<std::uint32_t> m_pressed_keys;
    std::unordered_set<std::uint32_t> m_pressed_buttons;
    Vec2 m_pointer_position;
    Modifiers m_modifiers;
};

// =============================================================================
// Vec2 Implementation
// =============================================================================

inline float Vec2::length() const {
    return std::sqrt(length_squared());
}

} // namespace void_compositor
