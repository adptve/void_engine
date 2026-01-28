/// @file fwd.hpp
/// @brief Forward declarations for widget system

#pragma once

#include <cstdint>
#include <functional>

namespace void_widget {

// =============================================================================
// Handle Types (Type-safe IDs)
// =============================================================================

/// @brief Widget instance identifier
struct WidgetId {
    std::uint64_t value{0};

    constexpr bool operator==(const WidgetId& other) const { return value == other.value; }
    constexpr bool operator!=(const WidgetId& other) const { return value != other.value; }
    constexpr bool operator<(const WidgetId& other) const { return value < other.value; }
    constexpr explicit operator bool() const { return value != 0; }
};

/// @brief Widget layer identifier
struct LayerId {
    std::uint64_t value{0};

    constexpr bool operator==(const LayerId& other) const { return value == other.value; }
    constexpr bool operator!=(const LayerId& other) const { return value != other.value; }
    constexpr bool operator<(const LayerId& other) const { return value < other.value; }
    constexpr explicit operator bool() const { return value != 0; }
};

/// @brief Animation identifier
struct AnimationId {
    std::uint64_t value{0};

    constexpr bool operator==(const AnimationId& other) const { return value == other.value; }
    constexpr bool operator!=(const AnimationId& other) const { return value != other.value; }
    constexpr explicit operator bool() const { return value != 0; }
};

/// @brief Data binding identifier
struct BindingId {
    std::uint64_t value{0};

    constexpr bool operator==(const BindingId& other) const { return value == other.value; }
    constexpr bool operator!=(const BindingId& other) const { return value != other.value; }
    constexpr explicit operator bool() const { return value != 0; }
};

/// @brief Style class identifier
struct StyleId {
    std::uint64_t value{0};

    constexpr bool operator==(const StyleId& other) const { return value == other.value; }
    constexpr bool operator!=(const StyleId& other) const { return value != other.value; }
    constexpr explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Forward Declarations - State Stores
// =============================================================================

struct WidgetRegistry;
struct LayoutState;
struct StyleState;
struct InteractionState;
struct AnimationState;
struct BindingState;
struct RenderState;

// =============================================================================
// Forward Declarations - Data Types
// =============================================================================

struct WidgetInstance;
struct WidgetTemplate;
struct WidgetLayer;
struct LayoutData;
struct LayoutConstraints;
struct ComputedStyle;
struct StyleOverrides;
struct Theme;
struct FontData;
struct TextureRegion;
struct ActiveAnimation;
struct AnimationDef;
struct Keyframe;
struct DataBinding;
struct DrawCommand;
struct DrawCommandList;

// =============================================================================
// Forward Declarations - Core Classes
// =============================================================================

class WidgetStateCore;
class IWidgetAPI;
class WidgetAPIImpl;
class Widget;
class IWidgetCommand;
class WidgetWatcher;

// =============================================================================
// Forward Declarations - Commands
// =============================================================================

class CreateWidgetCommand;
class DestroyWidgetCommand;
class SetParentCommand;
class SetLayoutCommand;
class SetStyleCommand;
class SetVisibleCommand;
class PlayAnimationCommand;
class StopAnimationCommand;
class BindPropertyCommand;
class UnbindPropertyCommand;

} // namespace void_widget

// =============================================================================
// Hash Specializations (for use in unordered containers)
// =============================================================================

namespace std {

template<>
struct hash<void_widget::WidgetId> {
    std::size_t operator()(const void_widget::WidgetId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_widget::LayerId> {
    std::size_t operator()(const void_widget::LayerId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_widget::AnimationId> {
    std::size_t operator()(const void_widget::AnimationId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_widget::BindingId> {
    std::size_t operator()(const void_widget::BindingId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_widget::StyleId> {
    std::size_t operator()(const void_widget::StyleId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
