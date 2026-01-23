#pragma once

/// @file vrr.hpp
/// @brief Variable Refresh Rate (VRR) support
///
/// This module handles VRR (VSync-off, FreeSync, G-Sync) detection and configuration.
/// VRR allows dynamic refresh rate adjustment for lower latency and smoother experience.

#include "fwd.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace void_compositor {

// =============================================================================
// VRR Mode
// =============================================================================

/// VRR operating mode
enum class VrrMode : std::uint8_t {
    /// VRR disabled (fixed refresh rate)
    Disabled,
    /// Automatic VRR (adapt based on content)
    Auto,
    /// Always run at maximum refresh rate
    MaximumPerformance,
    /// Prefer lower refresh rates for power saving
    PowerSaving,
};

/// Get mode name
[[nodiscard]] inline const char* to_string(VrrMode mode) {
    switch (mode) {
        case VrrMode::Disabled: return "Disabled";
        case VrrMode::Auto: return "Auto";
        case VrrMode::MaximumPerformance: return "MaximumPerformance";
        case VrrMode::PowerSaving: return "PowerSaving";
    }
    return "Unknown";
}

// =============================================================================
// VRR Configuration
// =============================================================================

/// VRR configuration
struct VrrConfig {
    /// Is VRR enabled?
    bool enabled = false;
    /// Minimum refresh rate (Hz)
    std::uint32_t min_refresh_rate = 48;
    /// Maximum refresh rate (Hz)
    std::uint32_t max_refresh_rate = 144;
    /// Current dynamic refresh rate (Hz)
    std::uint32_t current_refresh_rate = 60;
    /// VRR mode
    VrrMode mode = VrrMode::Disabled;

    /// Create a new VRR configuration
    static VrrConfig create(std::uint32_t min_refresh, std::uint32_t max_refresh) {
        return VrrConfig{
            .enabled = false,
            .min_refresh_rate = min_refresh,
            .max_refresh_rate = max_refresh,
            .current_refresh_rate = max_refresh,
            .mode = VrrMode::Disabled,
        };
    }

    /// Enable VRR with the given mode
    void enable(VrrMode new_mode) {
        enabled = true;
        mode = new_mode;
        switch (new_mode) {
            case VrrMode::Disabled:
                current_refresh_rate = max_refresh_rate;
                break;
            case VrrMode::Auto:
            case VrrMode::MaximumPerformance:
                current_refresh_rate = max_refresh_rate;
                break;
            case VrrMode::PowerSaving:
                current_refresh_rate = min_refresh_rate;
                break;
        }
    }

    /// Disable VRR
    void disable() {
        enabled = false;
        mode = VrrMode::Disabled;
    }

    /// Check if VRR is active
    [[nodiscard]] bool is_active() const {
        return enabled && mode != VrrMode::Disabled;
    }

    /// Get the current frame time target
    [[nodiscard]] std::chrono::nanoseconds frame_time() const {
        if (current_refresh_rate > 0) {
            return std::chrono::nanoseconds(1'000'000'000 / current_refresh_rate);
        }
        return std::chrono::milliseconds(16); // Fallback to 60Hz
    }

    /// Get frame time as duration for compatibility
    [[nodiscard]] std::chrono::duration<double> frame_time_seconds() const {
        return std::chrono::duration<double>(1.0 / static_cast<double>(current_refresh_rate));
    }

    /// Adapt refresh rate based on content velocity
    ///
    /// Content velocity is a normalized value (0.0-1.0) that represents
    /// how much the scene is changing. Higher values indicate more motion.
    ///
    /// The algorithm:
    /// - Uses maximum refresh for fast-moving content (velocity > 0.5)
    /// - Uses minimum refresh for static content (velocity < 0.1)
    /// - Interpolates for medium content
    /// - Applies hysteresis to avoid rapid changes
    void adapt_refresh_rate(float content_velocity) {
        if (!is_active() || mode != VrrMode::Auto) {
            return;
        }

        std::uint32_t target_refresh;
        if (content_velocity > 0.5f) {
            // Fast-moving content: use max refresh
            target_refresh = max_refresh_rate;
        } else if (content_velocity < 0.1f) {
            // Static content: use min refresh
            target_refresh = min_refresh_rate;
        } else {
            // Medium content: interpolate
            float t = (content_velocity - 0.1f) / 0.4f; // Normalize to 0-1
            float range = static_cast<float>(max_refresh_rate - min_refresh_rate);
            target_refresh = min_refresh_rate + static_cast<std::uint32_t>(range * t);
        }

        // Apply hysteresis: only change if difference is significant (>5 Hz)
        std::int32_t diff = static_cast<std::int32_t>(target_refresh) -
                           static_cast<std::int32_t>(current_refresh_rate);
        if (std::abs(diff) > 5) {
            current_refresh_rate = std::clamp(target_refresh, min_refresh_rate, max_refresh_rate);
        }
    }

    /// Check if a refresh rate is within VRR range
    [[nodiscard]] bool supports_refresh_rate(std::uint32_t rate) const {
        return rate >= min_refresh_rate && rate <= max_refresh_rate;
    }

    /// Get the VRR range as a string
    [[nodiscard]] std::string range_string() const {
        return std::to_string(min_refresh_rate) + "-" +
               std::to_string(max_refresh_rate) + "Hz";
    }
};

// =============================================================================
// VRR Capability
// =============================================================================

/// VRR capability detection result
struct VrrCapability {
    /// Is VRR supported by the display?
    bool supported = false;
    /// Minimum refresh rate (if supported)
    std::optional<std::uint32_t> min_refresh_rate;
    /// Maximum refresh rate (if supported)
    std::optional<std::uint32_t> max_refresh_rate;
    /// VRR technology name (FreeSync, G-Sync, VESA AdaptiveSync, etc.)
    std::optional<std::string> technology;

    /// Create a VRR capability for a non-VRR display
    static VrrCapability not_supported() {
        return VrrCapability{};
    }

    /// Create a VRR capability for a VRR-capable display
    static VrrCapability create_supported(
        std::uint32_t min_refresh,
        std::uint32_t max_refresh,
        std::optional<std::string> tech = std::nullopt) {
        return VrrCapability{
            .supported = true,
            .min_refresh_rate = min_refresh,
            .max_refresh_rate = max_refresh,
            .technology = std::move(tech),
        };
    }

    /// Convert to VrrConfig (if supported)
    [[nodiscard]] std::optional<VrrConfig> to_config() const {
        if (!supported) {
            return std::nullopt;
        }
        return VrrConfig::create(
            min_refresh_rate.value_or(48),
            max_refresh_rate.value_or(144)
        );
    }

    /// Get VRR range as string
    [[nodiscard]] std::string range_string() const {
        if (!supported) {
            return "Not supported";
        }
        return std::to_string(min_refresh_rate.value_or(0)) + "-" +
               std::to_string(max_refresh_rate.value_or(0)) + "Hz";
    }
};

} // namespace void_compositor
