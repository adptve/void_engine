/// @file fwd.hpp
/// @brief Forward declarations for void_hud module

#pragma once

#include <cstdint>
#include <functional>

namespace void_hud {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Unique identifier for HUD elements
struct HudElementId {
    std::uint64_t value{0};
    bool operator==(const HudElementId&) const = default;
    bool operator!=(const HudElementId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for HUD layers
struct HudLayerId {
    std::uint64_t value{0};
    bool operator==(const HudLayerId&) const = default;
    bool operator!=(const HudLayerId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for data bindings
struct BindingId {
    std::uint64_t value{0};
    bool operator==(const BindingId&) const = default;
    bool operator!=(const BindingId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for animations
struct HudAnimationId {
    std::uint64_t value{0};
    bool operator==(const HudAnimationId&) const = default;
    bool operator!=(const HudAnimationId&) const = default;
    explicit operator bool() const { return value != 0; }
};

/// @brief Unique identifier for HUD presets
struct HudPresetId {
    std::uint64_t value{0};
    bool operator==(const HudPresetId&) const = default;
    bool operator!=(const HudPresetId&) const = default;
    explicit operator bool() const { return value != 0; }
};

// =============================================================================
// Forward Declarations - Elements
// =============================================================================

class IHudElement;
class HudPanel;
class HudText;
class HudProgressBar;
class HudIcon;
class HudMinimap;
class HudCrosshair;
class HudCompass;
class HudObjectiveMarker;
class HudDamageIndicator;
class HudNotification;
class HudTooltip;

// =============================================================================
// Forward Declarations - Data Binding
// =============================================================================

class IDataSource;
class PropertyBinding;
class DataBindingManager;
class BindingContext;

// =============================================================================
// Forward Declarations - Animation
// =============================================================================

class IHudAnimation;
class HudAnimator;
class HudAnimationSequence;
class HudTransition;

// =============================================================================
// Forward Declarations - System
// =============================================================================

struct HudConfig;
class HudLayer;
class HudManager;
class HudSystem;

} // namespace void_hud

// =============================================================================
// Hash Specializations
// =============================================================================

namespace std {

template<>
struct hash<void_hud::HudElementId> {
    std::size_t operator()(const void_hud::HudElementId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_hud::HudLayerId> {
    std::size_t operator()(const void_hud::HudLayerId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_hud::BindingId> {
    std::size_t operator()(const void_hud::BindingId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct hash<void_hud::HudAnimationId> {
    std::size_t operator()(const void_hud::HudAnimationId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
