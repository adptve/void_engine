/// @file input.hpp
/// @brief Main include header for void_input
///
/// STATUS: PRODUCTION (2026-01-28)
/// - Keyboard, mouse, gamepad input handling
/// - Action-based input mapping
/// - Context switching for different game states
/// - GLFW integration for native input
/// - SACRED hot-reload patterns (snapshot/restore)
///
/// ## Quick Start
///
/// ### Creating Input System
/// ```cpp
/// #include <void_engine/input/input.hpp>
///
/// // Create input system (attach to GLFW window)
/// void_input::InputSystem input;
/// input.initialize(glfw_window);
///
/// // Create gameplay context
/// auto* gameplay = input.create_context("gameplay", 0);
///
/// // Create actions with bindings
/// gameplay->create_action("move", void_input::ActionType::Axis2D)
///     ->add_binding({KeyAxis2D::wasd()})
///     ->add_binding({GamepadAxis::LeftX, GamepadAxis::LeftY});
///
/// gameplay->create_action("jump", void_input::ActionType::Button)
///     ->add_binding({KeyCode::Space})
///     ->add_binding({GamepadButton::A});
/// ```
///
/// ### Polling Input
/// ```cpp
/// void game_update(float dt) {
///     input.update();
///
///     auto* move = gameplay->get_action("move");
///     auto* jump = gameplay->get_action("jump");
///
///     if (move) {
///         auto dir = move->axis2d();
///         player.move(dir * speed * dt);
///     }
///
///     if (jump && jump->is_just_pressed()) {
///         player.jump();
///     }
/// }
/// ```
///
/// ### Direct State Access
/// ```cpp
/// // Keyboard
/// if (input.keyboard().is_pressed(KeyCode::Escape)) {
///     pause_game();
/// }
///
/// // Mouse
/// auto mouse_delta = input.mouse().delta;
/// camera.rotate(mouse_delta * sensitivity);
///
/// // Gamepad
/// if (input.gamepad(0).connected) {
///     auto stick = input.gamepad(0).get_stick(false);
///     // ...
/// }
/// ```

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "action.hpp"

#include <void_engine/core/hot_reload.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declare GLFW types
struct GLFWwindow;

namespace void_input {

// =============================================================================
// Input System
// =============================================================================

/// Central input system - handles all input devices and action mapping
class InputSystem {
public:
    static constexpr int MAX_GAMEPADS = 4;

    InputSystem();
    ~InputSystem();

    // Non-copyable
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /// Initialize with GLFW window
    void initialize(GLFWwindow* window);

    /// Shutdown
    void shutdown();

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const { return m_initialized; }

    // =========================================================================
    // Update
    // =========================================================================

    /// Update input state (call once per frame, before game logic)
    void update();

    /// Process pending events
    void process_events();

    // =========================================================================
    // Device State Access
    // =========================================================================

    /// Get keyboard state
    [[nodiscard]] const KeyboardState& keyboard() const { return m_keyboard; }
    [[nodiscard]] KeyboardState& keyboard() { return m_keyboard; }

    /// Get mouse state
    [[nodiscard]] const MouseState& mouse() const { return m_mouse; }
    [[nodiscard]] MouseState& mouse() { return m_mouse; }

    /// Get gamepad state (0-3)
    [[nodiscard]] const GamepadState& gamepad(int index) const;
    [[nodiscard]] GamepadState& gamepad(int index);

    /// Get number of connected gamepads
    [[nodiscard]] int connected_gamepad_count() const;

    // =========================================================================
    // Quick Checks
    // =========================================================================

    /// Check if key is pressed
    [[nodiscard]] bool is_key_pressed(KeyCode key) const { return m_keyboard.is_pressed(key); }
    [[nodiscard]] bool is_key_just_pressed(KeyCode key) const { return m_keyboard.is_just_pressed(key); }
    [[nodiscard]] bool is_key_just_released(KeyCode key) const { return m_keyboard.is_just_released(key); }

    /// Check if mouse button is pressed
    [[nodiscard]] bool is_mouse_pressed(MouseButton btn) const { return m_mouse.is_pressed(btn); }
    [[nodiscard]] bool is_mouse_just_pressed(MouseButton btn) const { return m_mouse.is_just_pressed(btn); }

    /// Get mouse position
    [[nodiscard]] void_math::Vec2 mouse_position() const { return m_mouse.position; }
    [[nodiscard]] void_math::Vec2 mouse_delta() const { return m_mouse.delta; }
    [[nodiscard]] void_math::Vec2 mouse_scroll() const { return m_mouse.scroll; }

    // =========================================================================
    // Mouse Control
    // =========================================================================

    /// Capture mouse (hide cursor, lock to window)
    void capture_mouse(bool capture);
    [[nodiscard]] bool is_mouse_captured() const { return m_mouse.captured; }

    /// Set mouse visibility
    void set_mouse_visible(bool visible);
    [[nodiscard]] bool is_mouse_visible() const { return m_mouse.visible; }

    /// Set mouse position
    void set_mouse_position(void_math::Vec2 pos);

    // =========================================================================
    // Contexts
    // =========================================================================

    /// Create an input context
    InputContext* create_context(const std::string& name, int priority = 0);

    /// Get context by name
    InputContext* get_context(const std::string& name);

    /// Get context by ID
    InputContext* get_context(ContextId id);

    /// Remove context
    void remove_context(const std::string& name);

    /// Activate context
    void activate_context(const std::string& name);

    /// Deactivate context
    void deactivate_context(const std::string& name);

    /// Get all contexts sorted by priority
    [[nodiscard]] std::vector<InputContext*> active_contexts() const;

    // =========================================================================
    // Action Queries
    // =========================================================================

    /// Find action by name (searches all active contexts)
    InputAction* find_action(const std::string& name);

    /// Check if action is triggered (searches all active contexts)
    [[nodiscard]] bool is_action_pressed(const std::string& name);
    [[nodiscard]] bool is_action_just_pressed(const std::string& name);
    [[nodiscard]] float get_action_axis(const std::string& name);
    [[nodiscard]] void_math::Vec2 get_action_axis2d(const std::string& name);

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get configuration
    [[nodiscard]] const InputConfig& config() const { return m_config; }

    /// Set configuration
    void set_config(const InputConfig& config);

    /// Get statistics
    [[nodiscard]] InputStats stats() const { return m_stats; }

    // =========================================================================
    // Hot Reload (SACRED)
    // =========================================================================

    /// Take snapshot for hot-reload
    [[nodiscard]] std::vector<std::uint8_t> snapshot() const;

    /// Restore from snapshot
    void restore(const std::vector<std::uint8_t>& data);

    // =========================================================================
    // GLFW Callbacks (internal use)
    // =========================================================================

    void on_key(int key, int scancode, int action, int mods);
    void on_char(unsigned int codepoint);
    void on_mouse_button(int button, int action, int mods);
    void on_cursor_pos(double x, double y);
    void on_scroll(double x, double y);
    void on_joystick(int jid, int event);

private:
    bool m_initialized = false;
    GLFWwindow* m_window = nullptr;

    InputConfig m_config;
    InputStats m_stats;

    KeyboardState m_keyboard;
    MouseState m_mouse;
    std::array<GamepadState, MAX_GAMEPADS> m_gamepads;

    std::unordered_map<std::string, std::unique_ptr<InputContext>> m_contexts;
    std::uint32_t m_next_context_id = 1;

    // Text input buffer
    std::vector<std::uint32_t> m_text_input;

    void update_gamepads();
    void update_actions();
    InputValue evaluate_binding(const BindingSource& binding);
};

// =============================================================================
// Prelude
// =============================================================================

namespace prelude {
    using void_input::InputSystem;
    using void_input::InputContext;
    using void_input::InputAction;
    using void_input::InputBinding;
    using void_input::ActionBuilder;

    using void_input::InputDevice;
    using void_input::InputState;
    using void_input::KeyCode;
    using void_input::MouseButton;
    using void_input::GamepadButton;
    using void_input::GamepadAxis;
    using void_input::ActionType;
    using void_input::ModifierFlags;

    using void_input::KeyboardState;
    using void_input::MouseState;
    using void_input::GamepadState;

    using void_input::InputValue;
    using void_input::InputConfig;
    using void_input::InputStats;

    using void_input::ActionId;
    using void_input::BindingId;
    using void_input::ContextId;

    using void_input::KeyAxis2D;
    using void_input::KeyAxis1D;
}

} // namespace void_input
