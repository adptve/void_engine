/// @file swapchain.cpp
/// @brief Swapchain utilities implementation for void_presenter

#include <void_engine/presenter/swapchain.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <numeric>
#include <vector>

namespace void_presenter {

// =============================================================================
// SwapchainConfig Utilities
// =============================================================================

SwapchainConfig create_default_swapchain_config(
    std::uint32_t width,
    std::uint32_t height) {

    SwapchainConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.present_mode = PresentMode::Fifo;
    config.alpha_mode = AlphaMode::Opaque;
    config.image_count = 3; // Triple buffering
    config.enable_hdr = false;
    return config;
}

SwapchainConfig create_triple_buffer_config(
    std::uint32_t width,
    std::uint32_t height,
    SurfaceFormat format) {

    SwapchainConfig config;
    config.width = width;
    config.height = height;
    config.format = format;
    config.present_mode = PresentMode::Mailbox;
    config.alpha_mode = AlphaMode::Opaque;
    config.image_count = 3;
    config.enable_hdr = is_hdr_capable(format);
    return config;
}

SwapchainConfig create_vsync_config(
    std::uint32_t width,
    std::uint32_t height,
    SurfaceFormat format) {

    SwapchainConfig config;
    config.width = width;
    config.height = height;
    config.format = format;
    config.present_mode = PresentMode::Fifo;
    config.alpha_mode = AlphaMode::Opaque;
    config.image_count = 2; // Double buffer is sufficient for VSync
    config.enable_hdr = is_hdr_capable(format);
    return config;
}

SwapchainConfig create_low_latency_config(
    std::uint32_t width,
    std::uint32_t height,
    SurfaceFormat format) {

    SwapchainConfig config;
    config.width = width;
    config.height = height;
    config.format = format;
    config.present_mode = PresentMode::Immediate;
    config.alpha_mode = AlphaMode::Opaque;
    config.image_count = 2;
    config.enable_hdr = is_hdr_capable(format);
    return config;
}

SwapchainConfig create_hdr_config(
    std::uint32_t width,
    std::uint32_t height) {

    SwapchainConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Rgba16Float; // HDR format
    config.present_mode = PresentMode::Fifo;
    config.alpha_mode = AlphaMode::Opaque;
    config.image_count = 3;
    config.enable_hdr = true;
    return config;
}

// =============================================================================
// SwapchainStats Utilities
// =============================================================================

std::string format_swapchain_stats(const SwapchainStats& stats) {
    std::string result = "SwapchainStats {\n";

    result += "  frames_presented: " + std::to_string(stats.frames_presented) + "\n";
    result += "  frames_dropped: " + std::to_string(stats.frames_dropped) + "\n";
    result += "  drop_rate: " + std::to_string(stats.drop_rate() * 100.0) + "%\n";

    result += "  avg_fps: " + std::to_string(stats.average_fps()) + "\n";
    result += "  avg_frame_time: " + std::to_string(stats.avg_frame_time_us) + " us\n";
    result += "  avg_acquire_time: " + std::to_string(stats.avg_acquire_time_us) + " us\n";
    result += "  avg_present_time: " + std::to_string(stats.avg_present_time_us) + " us\n";

    if (stats.min_frame_time_us != UINT64_MAX) {
        result += "  min_frame_time: " + std::to_string(stats.min_frame_time_us) + " us\n";
    }
    result += "  max_frame_time: " + std::to_string(stats.max_frame_time_us) + " us\n";

    result += "  resize_count: " + std::to_string(stats.resize_count) + "\n";
    result += "  recreate_count: " + std::to_string(stats.recreate_count) + "\n";

    result += "}";
    return result;
}

SwapchainStats merge_swapchain_stats(
    const SwapchainStats& a,
    const SwapchainStats& b) {

    SwapchainStats merged;

    merged.frames_presented = a.frames_presented + b.frames_presented;
    merged.frames_dropped = a.frames_dropped + b.frames_dropped;
    merged.resize_count = a.resize_count + b.resize_count;
    merged.recreate_count = a.recreate_count + b.recreate_count;

    // Weighted average for timing
    double total = static_cast<double>(merged.frames_presented);
    if (total > 0.0) {
        double a_weight = static_cast<double>(a.frames_presented) / total;
        double b_weight = static_cast<double>(b.frames_presented) / total;

        merged.avg_acquire_time_us = a.avg_acquire_time_us * a_weight + b.avg_acquire_time_us * b_weight;
        merged.avg_present_time_us = a.avg_present_time_us * a_weight + b.avg_present_time_us * b_weight;
        merged.avg_frame_time_us = a.avg_frame_time_us * a_weight + b.avg_frame_time_us * b_weight;
    }

    merged.min_frame_time_us = std::min(a.min_frame_time_us, b.min_frame_time_us);
    merged.max_frame_time_us = std::max(a.max_frame_time_us, b.max_frame_time_us);

    return merged;
}

// =============================================================================
// SwapchainState Utilities
// =============================================================================

bool is_recoverable_state(SwapchainState state) {
    switch (state) {
        case SwapchainState::Ready:
        case SwapchainState::Suboptimal:
        case SwapchainState::Minimized:
            return true;

        case SwapchainState::OutOfDate:
            return true; // Can be recovered by recreating

        case SwapchainState::Lost:
            return false; // Usually needs full surface recreation
    }
    return false;
}

bool needs_recreation(SwapchainState state) {
    return state == SwapchainState::OutOfDate ||
           state == SwapchainState::Lost ||
           state == SwapchainState::Suboptimal;
}

bool can_render(SwapchainState state) {
    return state == SwapchainState::Ready ||
           state == SwapchainState::Suboptimal;
}

// =============================================================================
// Frame Pacing Utilities
// =============================================================================

namespace {

/// Calculate target frame time based on display refresh rate
std::chrono::nanoseconds calculate_target_frame_time(float target_fps) {
    if (target_fps <= 0.0f) {
        return std::chrono::nanoseconds(0); // No limit
    }
    return std::chrono::nanoseconds(static_cast<std::int64_t>(1'000'000'000.0 / target_fps));
}

} // anonymous namespace

// =============================================================================
// FramePacingController
// =============================================================================

/// Frame pacing controller for smooth frame delivery
class FramePacingController {
public:
    explicit FramePacingController(float target_fps = 60.0f)
        : m_target_frame_time(calculate_target_frame_time(target_fps))
        , m_frame_count(0)
        , m_running_average_us(0.0)
        , m_variance(0.0)
    {}

    /// Set target FPS
    void set_target_fps(float fps) {
        m_target_frame_time = calculate_target_frame_time(fps);
    }

    /// Get target frame time
    [[nodiscard]] std::chrono::nanoseconds target_frame_time() const {
        return m_target_frame_time;
    }

    /// Record frame timing and return sleep time for pacing
    [[nodiscard]] std::chrono::nanoseconds record_frame(std::chrono::nanoseconds actual_time) {
        ++m_frame_count;

        // Update running average using exponential moving average
        double actual_us = static_cast<double>(actual_time.count()) / 1000.0;
        double alpha = std::min(0.1, 1.0 / static_cast<double>(m_frame_count));

        m_running_average_us = m_running_average_us * (1.0 - alpha) + actual_us * alpha;

        // Update variance
        double diff = actual_us - m_running_average_us;
        m_variance = m_variance * (1.0 - alpha) + diff * diff * alpha;

        // Calculate sleep time
        if (m_target_frame_time.count() <= 0) {
            return std::chrono::nanoseconds(0);
        }

        // Target minus actual, with some padding for jitter
        double target_us = static_cast<double>(m_target_frame_time.count()) / 1000.0;
        double jitter_padding = std::sqrt(m_variance) * 0.5; // Half std dev
        double sleep_us = target_us - actual_us - jitter_padding;

        if (sleep_us > 0.0) {
            return std::chrono::nanoseconds(static_cast<std::int64_t>(sleep_us * 1000.0));
        }
        return std::chrono::nanoseconds(0);
    }

    /// Get average frame time
    [[nodiscard]] double average_frame_time_us() const {
        return m_running_average_us;
    }

    /// Get frame time standard deviation
    [[nodiscard]] double frame_time_stddev_us() const {
        return std::sqrt(m_variance);
    }

    /// Reset statistics
    void reset() {
        m_frame_count = 0;
        m_running_average_us = 0.0;
        m_variance = 0.0;
    }

private:
    std::chrono::nanoseconds m_target_frame_time;
    std::uint64_t m_frame_count;
    double m_running_average_us;
    double m_variance;
};

// =============================================================================
// SwapchainConfigValidator
// =============================================================================

/// Validates and adjusts swapchain configuration
class SwapchainConfigValidator {
public:
    /// Validate config against surface capabilities
    [[nodiscard]] static bool validate(
        const SwapchainConfig& config,
        const SurfaceCapabilities& caps,
        std::string* out_error = nullptr) {

        // Check format
        bool format_ok = false;
        for (const auto& fmt : caps.formats) {
            if (fmt == config.format) {
                format_ok = true;
                break;
            }
        }
        if (!format_ok) {
            if (out_error) *out_error = "Surface format not supported";
            return false;
        }

        // Check present mode
        bool mode_ok = false;
        for (const auto& mode : caps.present_modes) {
            if (mode == config.present_mode) {
                mode_ok = true;
                break;
            }
        }
        if (!mode_ok) {
            if (out_error) *out_error = "Present mode not supported";
            return false;
        }

        // Check alpha mode
        bool alpha_ok = false;
        for (const auto& alpha : caps.alpha_modes) {
            if (alpha == config.alpha_mode) {
                alpha_ok = true;
                break;
            }
        }
        if (!alpha_ok) {
            if (out_error) *out_error = "Alpha mode not supported";
            return false;
        }

        // Check dimensions
        if (config.width < caps.min_width || config.width > caps.max_width) {
            if (out_error) *out_error = "Width out of range";
            return false;
        }

        if (config.height < caps.min_height || config.height > caps.max_height) {
            if (out_error) *out_error = "Height out of range";
            return false;
        }

        return true;
    }

    /// Adjust config to match surface capabilities
    [[nodiscard]] static SwapchainConfig adjust(
        const SwapchainConfig& config,
        const SurfaceCapabilities& caps) {

        SwapchainConfig adjusted = config;

        // Adjust format
        bool format_ok = false;
        for (const auto& fmt : caps.formats) {
            if (fmt == config.format) {
                format_ok = true;
                break;
            }
        }
        if (!format_ok && !caps.formats.empty()) {
            adjusted.format = caps.preferred_format();
        }

        // Adjust present mode
        bool mode_ok = false;
        for (const auto& mode : caps.present_modes) {
            if (mode == config.present_mode) {
                mode_ok = true;
                break;
            }
        }
        if (!mode_ok && !caps.present_modes.empty()) {
            adjusted.present_mode = caps.present_modes[0];
        }

        // Adjust alpha mode
        bool alpha_ok = false;
        for (const auto& alpha : caps.alpha_modes) {
            if (alpha == config.alpha_mode) {
                alpha_ok = true;
                break;
            }
        }
        if (!alpha_ok && !caps.alpha_modes.empty()) {
            adjusted.alpha_mode = caps.alpha_modes[0];
        }

        // Clamp dimensions
        auto [w, h] = caps.clamp_extent(config.width, config.height);
        adjusted.width = w;
        adjusted.height = h;

        return adjusted;
    }
};

// =============================================================================
// Swapchain Utilities Export Functions
// =============================================================================

bool validate_swapchain_config(
    const SwapchainConfig& config,
    const SurfaceCapabilities& caps,
    std::string* out_error) {
    return SwapchainConfigValidator::validate(config, caps, out_error);
}

SwapchainConfig adjust_swapchain_config(
    const SwapchainConfig& config,
    const SurfaceCapabilities& caps) {
    return SwapchainConfigValidator::adjust(config, caps);
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_swapchain_config(const SwapchainConfig& config) {
    std::string result = "SwapchainConfig {\n";

    result += "  size: " + std::to_string(config.width) + "x" + std::to_string(config.height) + "\n";
    result += "  format: " + std::string(to_string(config.format)) + "\n";
    result += "  present_mode: " + std::string(to_string(config.present_mode)) + "\n";
    result += "  alpha_mode: " + std::string(to_string(config.alpha_mode)) + "\n";
    result += "  image_count: " + std::to_string(config.image_count) + "\n";
    result += "  enable_hdr: " + std::string(config.enable_hdr ? "true" : "false") + "\n";

    result += "}";
    return result;
}

std::string format_swapchain_state(SwapchainState state) {
    return std::string(to_string(state));
}

std::string format_frame_sync_data(const FrameSyncData& sync) {
    std::string result = "FrameSyncData {\n";

    result += "  frame_number: " + std::to_string(sync.frame_number) + "\n";
    result += "  in_use: " + std::string(sync.in_use ? "true" : "false") + "\n";

    result += "}";
    return result;
}

} // namespace debug

} // namespace void_presenter
