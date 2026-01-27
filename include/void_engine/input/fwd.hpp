/// @file fwd.hpp
/// @brief Forward declarations for void_input
///
/// STATUS: PRODUCTION (2026-01-28)

#pragma once

#include <cstdint>
#include <memory>

namespace void_input {

// =============================================================================
// Handle Types
// =============================================================================

/// Unique identifier for input actions
struct ActionId {
    std::uint32_t value = 0;
    bool operator==(const ActionId& other) const { return value == other.value; }
    bool operator!=(const ActionId& other) const { return value != other.value; }
    bool operator<(const ActionId& other) const { return value < other.value; }
    explicit operator bool() const { return value != 0; }
};

/// Unique identifier for input bindings
struct BindingId {
    std::uint32_t value = 0;
    bool operator==(const BindingId& other) const { return value == other.value; }
    bool operator!=(const BindingId& other) const { return value != other.value; }
    explicit operator bool() const { return value != 0; }
};

/// Unique identifier for input contexts
struct ContextId {
    std::uint32_t value = 0;
    bool operator==(const ContextId& other) const { return value == other.value; }
    bool operator!=(const ContextId& other) const { return value != other.value; }
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Enumerations
// =============================================================================

enum class InputDevice : std::uint8_t;
enum class KeyCode : std::uint16_t;
enum class MouseButton : std::uint8_t;
enum class GamepadButton : std::uint8_t;
enum class GamepadAxis : std::uint8_t;
enum class ActionType : std::uint8_t;
enum class InputState : std::uint8_t;

// =============================================================================
// Classes
// =============================================================================

class InputSystem;
class InputContext;
class InputAction;
class InputBinding;
class KeyboardState;
class MouseState;
class GamepadState;

// =============================================================================
// Smart Pointers
// =============================================================================

using InputSystemPtr = std::unique_ptr<InputSystem>;
using InputContextPtr = std::shared_ptr<InputContext>;

} // namespace void_input

// Hash specializations
namespace std {
    template<> struct hash<void_input::ActionId> {
        std::size_t operator()(const void_input::ActionId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_input::BindingId> {
        std::size_t operator()(const void_input::BindingId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_input::ContextId> {
        std::size_t operator()(const void_input::ContextId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
}
