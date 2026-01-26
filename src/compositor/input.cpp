/// @file input.cpp
/// @brief Input handling implementation
///
/// Provides input event processing, gesture recognition, and input state management.

#include <void_engine/compositor/input.hpp>
#include <void_engine/compositor/rehydration.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace void_compositor {

// =============================================================================
// Input State Queries
// =============================================================================

/// Get all currently pressed keys
[[nodiscard]] std::vector<std::uint32_t> get_pressed_keys(const InputState& state) {
    std::vector<std::uint32_t> keys;

    for (std::uint32_t code = 1; code < 512; ++code) {
        if (state.is_key_pressed(code)) {
            keys.push_back(code);
        }
    }

    return keys;
}

/// Get all currently pressed buttons
[[nodiscard]] std::vector<std::uint32_t> get_pressed_buttons(const InputState& state) {
    std::vector<std::uint32_t> buttons;

    for (std::uint32_t code = 0; code < 16; ++code) {
        if (state.is_button_pressed(code)) {
            buttons.push_back(code);
        }
    }

    return buttons;
}

/// Check if any key from a set is pressed
[[nodiscard]] bool is_any_key_pressed(
    const InputState& state,
    const std::vector<std::uint32_t>& keycodes) {

    for (std::uint32_t code : keycodes) {
        if (state.is_key_pressed(code)) {
            return true;
        }
    }
    return false;
}

/// Check if all keys from a set are pressed
[[nodiscard]] bool are_all_keys_pressed(
    const InputState& state,
    const std::vector<std::uint32_t>& keycodes) {

    for (std::uint32_t code : keycodes) {
        if (!state.is_key_pressed(code)) {
            return false;
        }
    }
    return !keycodes.empty();
}

// =============================================================================
// Pointer/Mouse Utilities
// =============================================================================

/// Calculate pointer velocity from motion history
struct PointerVelocity {
    double x_per_second = 0.0;
    double y_per_second = 0.0;

    [[nodiscard]] double magnitude() const {
        return std::sqrt(x_per_second * x_per_second + y_per_second * y_per_second);
    }

    [[nodiscard]] double angle() const {
        return std::atan2(y_per_second, x_per_second);
    }
};

/// Motion history entry
struct MotionEntry {
    double x = 0.0;
    double y = 0.0;
    std::chrono::steady_clock::time_point timestamp;
};

/// Pointer velocity tracker
class PointerVelocityTracker {
public:
    static constexpr std::size_t k_history_size = 8;
    static constexpr auto k_max_age = std::chrono::milliseconds(100);

    /// Record a motion event
    void record(double x, double y) {
        MotionEntry entry;
        entry.x = x;
        entry.y = y;
        entry.timestamp = std::chrono::steady_clock::now();

        m_history[m_index] = entry;
        m_index = (m_index + 1) % k_history_size;
        if (m_count < k_history_size) {
            ++m_count;
        }
    }

    /// Calculate current velocity
    [[nodiscard]] PointerVelocity velocity() const {
        if (m_count < 2) {
            return {};
        }

        auto now = std::chrono::steady_clock::now();

        // Find oldest valid entry
        double total_dx = 0.0;
        double total_dy = 0.0;
        double total_dt = 0.0;
        std::size_t valid_samples = 0;

        for (std::size_t i = 1; i < m_count; ++i) {
            std::size_t curr = (m_index + k_history_size - i) % k_history_size;
            std::size_t prev = (m_index + k_history_size - i - 1) % k_history_size;

            auto age = now - m_history[curr].timestamp;
            if (age > k_max_age) {
                break;
            }

            auto dt = m_history[curr].timestamp - m_history[prev].timestamp;
            double dt_seconds = std::chrono::duration<double>(dt).count();

            if (dt_seconds > 0.0) {
                total_dx += m_history[curr].x - m_history[prev].x;
                total_dy += m_history[curr].y - m_history[prev].y;
                total_dt += dt_seconds;
                ++valid_samples;
            }
        }

        if (total_dt <= 0.0 || valid_samples == 0) {
            return {};
        }

        return {total_dx / total_dt, total_dy / total_dt};
    }

    /// Reset tracker
    void reset() {
        m_count = 0;
        m_index = 0;
    }

private:
    std::array<MotionEntry, k_history_size> m_history;
    std::size_t m_index = 0;
    std::size_t m_count = 0;
};

// =============================================================================
// Gesture Recognition
// =============================================================================

/// Gesture types
enum class GestureType : std::uint8_t {
    None,
    Tap,
    DoubleTap,
    LongPress,
    SwipeLeft,
    SwipeRight,
    SwipeUp,
    SwipeDown,
    PinchIn,
    PinchOut,
    Rotate,
};

/// Recognized gesture
struct Gesture {
    GestureType type = GestureType::None;
    double x = 0.0;
    double y = 0.0;
    double magnitude = 0.0;  // For swipes/pinch
    double angle = 0.0;      // For rotation
    std::chrono::steady_clock::time_point timestamp;
};

/// Simple gesture recognizer for touch input
class GestureRecognizer {
public:
    static constexpr double k_tap_max_distance = 10.0;
    static constexpr auto k_tap_max_duration = std::chrono::milliseconds(200);
    static constexpr auto k_double_tap_max_interval = std::chrono::milliseconds(300);
    static constexpr auto k_long_press_duration = std::chrono::milliseconds(500);
    static constexpr double k_swipe_min_distance = 50.0;

    /// Process touch down
    void touch_down(double x, double y) {
        m_start_x = x;
        m_start_y = y;
        m_start_time = std::chrono::steady_clock::now();
        m_is_tracking = true;
        m_long_press_triggered = false;
    }

    /// Process touch move
    void touch_move(double x, double y) {
        if (!m_is_tracking) return;

        m_current_x = x;
        m_current_y = y;

        // Check for long press cancellation
        double dist = distance_from_start(x, y);
        if (dist > k_tap_max_distance) {
            m_long_press_triggered = false;
        }
    }

    /// Process touch up and return recognized gesture
    [[nodiscard]] Gesture touch_up(double x, double y) {
        if (!m_is_tracking) {
            return {};
        }

        m_is_tracking = false;
        auto end_time = std::chrono::steady_clock::now();
        auto duration = end_time - m_start_time;

        double dist = distance_from_start(x, y);
        double dx = x - m_start_x;
        double dy = y - m_start_y;

        Gesture gesture;
        gesture.x = x;
        gesture.y = y;
        gesture.timestamp = end_time;

        // Check for swipe
        if (dist >= k_swipe_min_distance) {
            gesture.magnitude = dist;
            gesture.angle = std::atan2(dy, dx);

            // Determine direction
            if (std::abs(dx) > std::abs(dy)) {
                gesture.type = dx > 0 ? GestureType::SwipeRight : GestureType::SwipeLeft;
            } else {
                gesture.type = dy > 0 ? GestureType::SwipeDown : GestureType::SwipeUp;
            }
            return gesture;
        }

        // Check for tap
        if (dist < k_tap_max_distance && duration < k_tap_max_duration) {
            // Check for double tap
            auto interval = end_time - m_last_tap_time;
            if (interval < k_double_tap_max_interval) {
                gesture.type = GestureType::DoubleTap;
                m_last_tap_time = {};  // Reset to prevent triple-tap
            } else {
                gesture.type = GestureType::Tap;
                m_last_tap_time = end_time;
            }
            return gesture;
        }

        // Check for long press
        if (dist < k_tap_max_distance && duration >= k_long_press_duration) {
            gesture.type = GestureType::LongPress;
            return gesture;
        }

        return {};
    }

    /// Update for long press detection (call periodically)
    [[nodiscard]] std::optional<Gesture> update() {
        if (!m_is_tracking || m_long_press_triggered) {
            return std::nullopt;
        }

        auto now = std::chrono::steady_clock::now();
        auto duration = now - m_start_time;

        double dist = distance_from_start(m_current_x, m_current_y);

        if (duration >= k_long_press_duration && dist < k_tap_max_distance) {
            m_long_press_triggered = true;

            Gesture gesture;
            gesture.type = GestureType::LongPress;
            gesture.x = m_current_x;
            gesture.y = m_current_y;
            gesture.timestamp = now;
            return gesture;
        }

        return std::nullopt;
    }

    /// Reset recognizer
    void reset() {
        m_is_tracking = false;
        m_long_press_triggered = false;
    }

private:
    [[nodiscard]] double distance_from_start(double x, double y) const {
        double dx = x - m_start_x;
        double dy = y - m_start_y;
        return std::sqrt(dx * dx + dy * dy);
    }

    double m_start_x = 0.0;
    double m_start_y = 0.0;
    double m_current_x = 0.0;
    double m_current_y = 0.0;
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_tap_time;
    bool m_is_tracking = false;
    bool m_long_press_triggered = false;
};

// =============================================================================
// Input Event Formatting
// =============================================================================

/// Format keyboard event for debugging
[[nodiscard]] std::string format_keyboard_event(const KeyboardEvent& event) {
    std::ostringstream ss;

    ss << "KeyboardEvent { keycode: " << event.keycode;
    ss << ", state: " << (event.state == KeyState::Pressed ? "Pressed" : "Released");

    if (!event.utf8.empty()) {
        ss << ", char: \"" << event.utf8 << "\"";
    }

    if (event.modifiers.any()) {
        ss << ", mods: [";
        if (event.modifiers.shift) ss << "Shift ";
        if (event.modifiers.ctrl) ss << "Ctrl ";
        if (event.modifiers.alt) ss << "Alt ";
        if (event.modifiers.super) ss << "Super ";
        ss << "]";
    }

    ss << " }";
    return ss.str();
}

/// Format pointer event for debugging
[[nodiscard]] std::string format_pointer_event(const PointerEvent& event) {
    std::ostringstream ss;

    std::visit([&ss](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, PointerMotionEvent>) {
            ss << "PointerMotion { x: " << e.surface_x << ", y: " << e.surface_y << " }";
        } else if constexpr (std::is_same_v<T, PointerButtonEvent>) {
            ss << "PointerButton { button: " << e.button;
            ss << ", state: " << (e.state == ButtonState::Pressed ? "Pressed" : "Released");
            ss << " }";
        } else if constexpr (std::is_same_v<T, PointerAxisEvent>) {
            ss << "PointerAxis { horizontal: " << e.horizontal;
            ss << ", vertical: " << e.vertical << " }";
        } else if constexpr (std::is_same_v<T, PointerEnterEvent>) {
            ss << "PointerEnter { x: " << e.surface_x << ", y: " << e.surface_y << " }";
        } else if constexpr (std::is_same_v<T, PointerLeaveEvent>) {
            ss << "PointerLeave {}";
        }
    }, event);

    return ss.str();
}

/// Format touch event for debugging
[[nodiscard]] std::string format_touch_event(const TouchEvent& event) {
    std::ostringstream ss;

    std::visit([&ss](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, TouchDownEvent>) {
            ss << "TouchDown { id: " << e.id << ", x: " << e.x << ", y: " << e.y << " }";
        } else if constexpr (std::is_same_v<T, TouchUpEvent>) {
            ss << "TouchUp { id: " << e.id << " }";
        } else if constexpr (std::is_same_v<T, TouchMotionEvent>) {
            ss << "TouchMotion { id: " << e.id << ", x: " << e.x << ", y: " << e.y << " }";
        } else if constexpr (std::is_same_v<T, TouchCancelEvent>) {
            ss << "TouchCancel {}";
        }
    }, event);

    return ss.str();
}

/// Format InputEvent variant for debugging
[[nodiscard]] std::string format_input_event(const InputEvent& event) {
    return std::visit([](const auto& e) -> std::string {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, KeyboardEvent>) {
            return format_keyboard_event(e);
        } else if constexpr (std::is_same_v<T, PointerEvent>) {
            return format_pointer_event(e);
        } else if constexpr (std::is_same_v<T, TouchEvent>) {
            return format_touch_event(e);
        } else if constexpr (std::is_same_v<T, DeviceEvent>) {
            return "DeviceEvent { ... }";
        } else {
            return "UnknownEvent";
        }
    }, event.data);
}

// =============================================================================
// Input State Serialization for Hot-Reload
// =============================================================================

/// Create rehydration state from InputState
[[nodiscard]] RehydrationState serialize_input_state(const InputState& state) {
    RehydrationState rstate;

    // Pointer position
    auto [px, py] = state.pointer_position();
    rstate.set_float("pointer_x", px);
    rstate.set_float("pointer_y", py);

    // Modifiers
    const auto& mods = state.modifiers();
    rstate.set_bool("mod_shift", mods.shift);
    rstate.set_bool("mod_ctrl", mods.ctrl);
    rstate.set_bool("mod_alt", mods.alt);
    rstate.set_bool("mod_super", mods.super);
    rstate.set_bool("mod_caps", mods.caps_lock);
    rstate.set_bool("mod_num", mods.num_lock);

    // Note: Key/button states are transient and not serialized

    return rstate;
}

/// Restore InputState from rehydration state
void deserialize_input_state(InputState& state, const RehydrationState& rstate) {
    // We can't directly restore InputState private members, but we can
    // provide initial values for the next frame

    // For hot-reload, the input state will be rebuilt from new events
    // This is intentionally a no-op as input state is transient
    (void)state;
    (void)rstate;
}

} // namespace void_compositor
