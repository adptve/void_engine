/// @file vrr.cpp
/// @brief Variable Refresh Rate (VRR) implementation
///
/// Provides VRR configuration, adaptation algorithms, and display sync utilities.

#include <void_engine/compositor/vrr.hpp>
#include <void_engine/compositor/types.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace void_compositor {

// =============================================================================
// VRR Mode Utilities
// =============================================================================

/// Get the default VRR mode for a use case
[[nodiscard]] VrrMode get_default_vrr_mode(bool prefer_performance, bool battery_saver) {
    if (battery_saver) {
        return VrrMode::PowerSaving;
    }
    if (prefer_performance) {
        return VrrMode::MaximumPerformance;
    }
    return VrrMode::Auto;
}

/// Check if VRR mode is power-efficient
[[nodiscard]] bool is_power_efficient_mode(VrrMode mode) {
    return mode == VrrMode::PowerSaving;
}

/// Check if VRR mode is performance-oriented
[[nodiscard]] bool is_performance_mode(VrrMode mode) {
    return mode == VrrMode::MaximumPerformance;
}

// =============================================================================
// VRR Configuration Utilities
// =============================================================================

/// Create VRR config for gaming
[[nodiscard]] VrrConfig create_gaming_vrr_config(std::uint32_t max_refresh) {
    VrrConfig config;
    config.enabled = true;
    config.min_refresh_rate = 48;
    config.max_refresh_rate = max_refresh;
    config.current_refresh_rate = max_refresh;
    config.mode = VrrMode::MaximumPerformance;
    return config;
}

/// Create VRR config for video playback
[[nodiscard]] VrrConfig create_video_vrr_config(std::uint32_t video_fps) {
    VrrConfig config;
    config.enabled = true;

    // Set range around video fps for smooth playback
    config.min_refresh_rate = video_fps;
    config.max_refresh_rate = video_fps * 2;  // Allow 2x for judder-free

    // Common video frame rates
    if (video_fps == 24 || video_fps == 25 || video_fps == 30) {
        config.min_refresh_rate = 24;
        config.max_refresh_rate = 60;
    } else if (video_fps == 50 || video_fps == 60) {
        config.min_refresh_rate = 48;
        config.max_refresh_rate = 120;
    }

    config.current_refresh_rate = video_fps;
    config.mode = VrrMode::Auto;
    return config;
}

/// Create VRR config for desktop/productivity
[[nodiscard]] VrrConfig create_desktop_vrr_config(std::uint32_t max_refresh) {
    VrrConfig config;
    config.enabled = true;
    config.min_refresh_rate = 30;  // Allow low refresh for static content
    config.max_refresh_rate = max_refresh;
    config.current_refresh_rate = 60;  // Start at reasonable rate
    config.mode = VrrMode::Auto;
    return config;
}

/// Calculate frame time for a refresh rate
[[nodiscard]] std::chrono::nanoseconds frame_time_for_rate(std::uint32_t refresh_hz) {
    if (refresh_hz == 0) {
        return std::chrono::milliseconds(16);  // Default to 60Hz
    }
    return std::chrono::nanoseconds(1'000'000'000 / refresh_hz);
}

/// Calculate refresh rate from frame time
[[nodiscard]] std::uint32_t rate_for_frame_time(std::chrono::nanoseconds frame_time) {
    if (frame_time.count() == 0) {
        return 60;  // Default
    }
    return static_cast<std::uint32_t>(1'000'000'000 / frame_time.count());
}

// =============================================================================
// VRR Adaptation Algorithms
// =============================================================================

/// Content velocity analyzer
class ContentVelocityAnalyzer {
public:
    /// Update with new frame data
    void update(float motion_vectors_magnitude, float scene_change_metric) {
        // Combine metrics into velocity
        float raw_velocity = motion_vectors_magnitude * 0.7f + scene_change_metric * 0.3f;

        // Apply smoothing
        constexpr float alpha = 0.1f;
        m_smoothed_velocity = m_smoothed_velocity * (1.0f - alpha) +
                              std::clamp(raw_velocity, 0.0f, 1.0f) * alpha;

        // Track history for trend analysis
        m_history[m_history_index] = m_smoothed_velocity;
        m_history_index = (m_history_index + 1) % k_history_size;
        if (m_history_count < k_history_size) {
            ++m_history_count;
        }
    }

    /// Get current smoothed velocity
    [[nodiscard]] float velocity() const { return m_smoothed_velocity; }

    /// Get velocity trend (-1 to 1, negative = decreasing)
    [[nodiscard]] float trend() const {
        if (m_history_count < 2) return 0.0f;

        // Compare recent average to older average
        float recent_sum = 0.0f;
        float older_sum = 0.0f;
        std::size_t half = m_history_count / 2;

        for (std::size_t i = 0; i < half; ++i) {
            std::size_t recent_idx = (m_history_index + k_history_size - 1 - i) % k_history_size;
            std::size_t older_idx = (m_history_index + k_history_size - 1 - half - i) % k_history_size;
            recent_sum += m_history[recent_idx];
            older_sum += m_history[older_idx];
        }

        float recent_avg = recent_sum / static_cast<float>(half);
        float older_avg = older_sum / static_cast<float>(half);

        return recent_avg - older_avg;
    }

    /// Predict future velocity
    [[nodiscard]] float predict(float time_ahead_seconds) const {
        float current = m_smoothed_velocity;
        float delta = trend() * time_ahead_seconds;
        return std::clamp(current + delta, 0.0f, 1.0f);
    }

    /// Reset analyzer
    void reset() {
        m_smoothed_velocity = 0.0f;
        m_history_index = 0;
        m_history_count = 0;
        for (auto& h : m_history) h = 0.0f;
    }

private:
    static constexpr std::size_t k_history_size = 30;

    float m_smoothed_velocity = 0.0f;
    std::array<float, k_history_size> m_history{};
    std::size_t m_history_index = 0;
    std::size_t m_history_count = 0;
};

/// Calculate optimal refresh rate with prediction
[[nodiscard]] std::uint32_t calculate_predictive_refresh_rate(
    const VrrConfig& config,
    float current_velocity,
    float velocity_trend,
    float prediction_horizon_seconds) {

    if (!config.is_active()) {
        return config.max_refresh_rate;
    }

    // Predict future velocity
    float predicted_velocity = std::clamp(
        current_velocity + velocity_trend * prediction_horizon_seconds,
        0.0f, 1.0f);

    // Use higher of current and predicted
    float effective_velocity = std::max(current_velocity, predicted_velocity);

    // Map velocity to refresh rate
    float t = effective_velocity;
    float range = static_cast<float>(config.max_refresh_rate - config.min_refresh_rate);
    std::uint32_t target = config.min_refresh_rate +
                           static_cast<std::uint32_t>(range * t);

    return std::clamp(target, config.min_refresh_rate, config.max_refresh_rate);
}

// =============================================================================
// VRR Capability Detection
// =============================================================================

/// Merge VRR capabilities from multiple outputs
[[nodiscard]] VrrCapability merge_vrr_capabilities(
    const std::vector<VrrCapability>& capabilities) {

    VrrCapability merged;

    for (const auto& cap : capabilities) {
        if (!cap.supported) continue;

        if (!merged.supported) {
            merged = cap;
        } else {
            // Take the intersection of ranges
            merged.min_refresh_rate = std::max(
                merged.min_refresh_rate.value_or(0),
                cap.min_refresh_rate.value_or(0));
            merged.max_refresh_rate = std::min(
                merged.max_refresh_rate.value_or(std::numeric_limits<std::uint32_t>::max()),
                cap.max_refresh_rate.value_or(std::numeric_limits<std::uint32_t>::max()));
        }
    }

    // Validate merged range
    if (merged.supported &&
        merged.min_refresh_rate.value_or(0) >= merged.max_refresh_rate.value_or(0)) {
        // Invalid range after merge - disable VRR
        merged.supported = false;
    }

    return merged;
}

/// Check if refresh rate is within VRR range
[[nodiscard]] bool is_rate_in_vrr_range(
    std::uint32_t rate,
    const VrrCapability& capability) {

    if (!capability.supported) return false;

    return rate >= capability.min_refresh_rate.value_or(0) &&
           rate <= capability.max_refresh_rate.value_or(0);
}

/// Get the closest supported refresh rate
[[nodiscard]] std::uint32_t clamp_to_vrr_range(
    std::uint32_t rate,
    const VrrCapability& capability) {

    if (!capability.supported) return rate;

    return std::clamp(
        rate,
        capability.min_refresh_rate.value_or(rate),
        capability.max_refresh_rate.value_or(rate));
}

// =============================================================================
// VRR Diagnostics
// =============================================================================

/// VRR diagnostic information
struct VrrDiagnostics {
    bool vrr_active = false;
    std::uint32_t current_refresh_rate = 0;
    std::uint32_t target_refresh_rate = 0;
    float content_velocity = 0.0f;
    std::chrono::nanoseconds frame_time{0};
    std::chrono::nanoseconds frame_time_budget{0};
    double headroom_percentage = 0.0;
    std::string mode_string;
    std::string range_string;
};

/// Get VRR diagnostics
[[nodiscard]] VrrDiagnostics get_vrr_diagnostics(const VrrConfig& config) {
    VrrDiagnostics diag;

    diag.vrr_active = config.is_active();
    diag.current_refresh_rate = config.current_refresh_rate;
    diag.frame_time = config.frame_time();
    diag.frame_time_budget = diag.frame_time;
    diag.mode_string = to_string(config.mode);
    diag.range_string = config.range_string();

    return diag;
}

/// Format VRR diagnostics as string
[[nodiscard]] std::string format_vrr_diagnostics(const VrrDiagnostics& diag) {
    std::ostringstream ss;

    ss << "VRR Status: " << (diag.vrr_active ? "Active" : "Inactive") << "\n";
    ss << "  Mode: " << diag.mode_string << "\n";
    ss << "  Range: " << diag.range_string << "\n";
    ss << "  Current: " << diag.current_refresh_rate << " Hz\n";

    if (diag.frame_time.count() > 0) {
        double ms = static_cast<double>(diag.frame_time.count()) / 1'000'000.0;
        ss << "  Frame time: " << ms << " ms\n";
    }

    return ss.str();
}

// =============================================================================
// LFC (Low Framerate Compensation)
// =============================================================================

/// Check if LFC should be enabled
[[nodiscard]] bool should_enable_lfc(
    std::uint32_t actual_fps,
    const VrrCapability& capability) {

    if (!capability.supported) return false;

    // LFC is needed when frame rate drops below VRR minimum
    return actual_fps < capability.min_refresh_rate.value_or(48);
}

/// Calculate LFC multiplier for smooth presentation
[[nodiscard]] std::uint32_t calculate_lfc_multiplier(
    std::uint32_t actual_fps,
    std::uint32_t min_vrr_rate) {

    if (actual_fps == 0 || actual_fps >= min_vrr_rate) {
        return 1;
    }

    // Find smallest multiplier that puts us in VRR range
    std::uint32_t multiplier = (min_vrr_rate + actual_fps - 1) / actual_fps;
    return std::max(multiplier, 1u);
}

/// Get effective refresh rate with LFC
[[nodiscard]] std::uint32_t get_lfc_refresh_rate(
    std::uint32_t actual_fps,
    const VrrCapability& capability) {

    if (!should_enable_lfc(actual_fps, capability)) {
        return actual_fps;
    }

    std::uint32_t multiplier = calculate_lfc_multiplier(
        actual_fps, capability.min_refresh_rate.value_or(48));

    return actual_fps * multiplier;
}

} // namespace void_compositor
