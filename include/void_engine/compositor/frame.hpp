#pragma once

/// @file frame.hpp
/// @brief Frame scheduling and timing
///
/// This module provides frame timing control for precise scheduling
/// of frame rendering and presentation.

#include "fwd.hpp"
#include "vrr.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>

namespace void_compositor {

// =============================================================================
// Frame State
// =============================================================================

/// Frame state in the rendering pipeline
enum class FrameState : std::uint8_t {
    /// Waiting for frame callback from compositor
    WaitingForCallback,
    /// Ready to render
    ReadyToRender,
    /// Currently rendering
    Rendering,
    /// Waiting for presentation
    WaitingForPresent,
    /// Frame was presented
    Presented,
    /// Frame was dropped (missed deadline)
    Dropped,
};

/// Get state name
[[nodiscard]] inline const char* to_string(FrameState state) {
    switch (state) {
        case FrameState::WaitingForCallback: return "WaitingForCallback";
        case FrameState::ReadyToRender: return "ReadyToRender";
        case FrameState::Rendering: return "Rendering";
        case FrameState::WaitingForPresent: return "WaitingForPresent";
        case FrameState::Presented: return "Presented";
        case FrameState::Dropped: return "Dropped";
    }
    return "Unknown";
}

// =============================================================================
// Presentation Feedback
// =============================================================================

/// Presentation feedback from the display
struct PresentationFeedback {
    /// When the frame was actually presented (display scanout)
    std::chrono::steady_clock::time_point presented_at;
    /// Sequence number
    std::uint64_t sequence = 0;
    /// Time from commit to presentation
    std::chrono::nanoseconds latency{0};
    /// Was VSync used
    bool vsync = true;
    /// Refresh rate at presentation (Hz)
    std::uint32_t refresh_rate = 60;
};

// =============================================================================
// Frame Scheduler
// =============================================================================

/// Frame scheduler - controls when frames are rendered
///
/// Provides high-level frame scheduling policy with support for:
/// - Target framerate control
/// - VRR (Variable Refresh Rate) integration
/// - Frame timing statistics (P50, P95, P99)
/// - Content velocity-based refresh rate adaptation
class FrameScheduler {
public:
    /// Create a new frame scheduler
    /// @param target_fps Target refresh rate (0 = use display default)
    explicit FrameScheduler(std::uint32_t target_fps = 60)
        : m_target_fps(target_fps)
        , m_frame_budget(target_fps > 0
            ? std::chrono::nanoseconds(1'000'000'000 / target_fps)
            : std::chrono::milliseconds(16))
        , m_last_presentation(std::chrono::steady_clock::now())
    {
        // std::deque doesn't have reserve(), but that's fine for our use case
    }

    // =========================================================================
    // Frame Lifecycle
    // =========================================================================

    /// Called when compositor signals frame callback
    void on_frame_callback() {
        m_callback_ready = true;
        m_state = FrameState::ReadyToRender;
    }

    /// Check if we should render a frame now
    [[nodiscard]] bool should_render() const {
        return m_callback_ready && m_state == FrameState::ReadyToRender;
    }

    /// Begin a new frame
    /// @return Frame number
    [[nodiscard]] std::uint64_t begin_frame() {
        m_state = FrameState::Rendering;
        m_callback_ready = false;
        return ++m_frame_number;
    }

    /// End the current frame (called after commit)
    void end_frame() {
        m_state = FrameState::WaitingForPresent;
    }

    /// Called when presentation feedback is received
    void on_presentation_feedback(const PresentationFeedback& feedback) {
        auto frame_time = feedback.presented_at - m_last_presentation;
        m_last_presentation = feedback.presented_at;

        // Update statistics
        if (m_frame_times.size() >= m_max_history) {
            m_frame_times.pop_front();
        }
        m_frame_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time));

        // Store feedback
        if (m_feedback_history.size() >= 10) {
            m_feedback_history.pop_front();
        }
        m_feedback_history.push_back(feedback);

        m_state = FrameState::Presented;
    }

    /// Mark frame as dropped (missed deadline)
    void drop_frame() {
        m_state = FrameState::Dropped;
        ++m_dropped_frame_count;
    }

    // =========================================================================
    // State Queries
    // =========================================================================

    /// Get current frame number
    [[nodiscard]] std::uint64_t frame_number() const { return m_frame_number; }

    /// Get current frame state
    [[nodiscard]] FrameState state() const { return m_state; }

    /// Get target FPS
    [[nodiscard]] std::uint32_t target_fps() const { return m_target_fps; }

    /// Set target FPS
    void set_target_fps(std::uint32_t fps) {
        m_target_fps = fps;
        m_frame_budget = fps > 0
            ? std::chrono::nanoseconds(1'000'000'000 / fps)
            : std::chrono::milliseconds(16);
    }

    /// Get frame budget
    [[nodiscard]] std::chrono::nanoseconds frame_budget() const { return m_frame_budget; }

    /// Get dropped frame count
    [[nodiscard]] std::uint64_t dropped_frame_count() const { return m_dropped_frame_count; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get average frame time
    [[nodiscard]] std::chrono::nanoseconds average_frame_time() const {
        if (m_frame_times.empty()) {
            return m_frame_budget;
        }
        std::chrono::nanoseconds total{0};
        for (const auto& t : m_frame_times) {
            total += t;
        }
        return total / static_cast<std::int64_t>(m_frame_times.size());
    }

    /// Get current FPS (based on recent frames)
    [[nodiscard]] double current_fps() const {
        auto avg = average_frame_time();
        double avg_seconds = std::chrono::duration<double>(avg).count();
        return avg_seconds > 0.0 ? 1.0 / avg_seconds : 0.0;
    }

    /// Get frame time percentile (for performance analysis)
    [[nodiscard]] std::chrono::nanoseconds frame_time_percentile(double percentile) const {
        if (m_frame_times.empty()) {
            return m_frame_budget;
        }

        std::vector<std::chrono::nanoseconds> sorted(m_frame_times.begin(), m_frame_times.end());
        std::sort(sorted.begin(), sorted.end());

        std::size_t index = static_cast<std::size_t>(
            (percentile / 100.0) * static_cast<double>(sorted.size() - 1)
        );
        return sorted[std::min(index, sorted.size() - 1)];
    }

    /// Get 50th percentile frame time (median)
    [[nodiscard]] std::chrono::nanoseconds frame_time_p50() const {
        return frame_time_percentile(50.0);
    }

    /// Get 95th percentile frame time
    [[nodiscard]] std::chrono::nanoseconds frame_time_p95() const {
        return frame_time_percentile(95.0);
    }

    /// Get 99th percentile frame time (useful for judging smoothness)
    [[nodiscard]] std::chrono::nanoseconds frame_time_p99() const {
        return frame_time_percentile(99.0);
    }

    /// Check if we're hitting target framerate
    [[nodiscard]] bool hitting_target() const {
        if (m_target_fps == 0) {
            return true;
        }
        auto target_time = std::chrono::nanoseconds(1'000'000'000 / m_target_fps);
        auto tolerance = target_time * 11 / 10; // 10% tolerance
        return average_frame_time() <= tolerance;
    }

    /// Get latest presentation feedback
    [[nodiscard]] const PresentationFeedback* latest_feedback() const {
        return m_feedback_history.empty() ? nullptr : &m_feedback_history.back();
    }

    // =========================================================================
    // Time Management
    // =========================================================================

    /// Calculate time remaining in frame budget
    [[nodiscard]] std::chrono::nanoseconds time_remaining() const {
        auto elapsed = std::chrono::steady_clock::now() - m_last_presentation;
        auto budget = effective_frame_budget();
        if (elapsed >= budget) {
            return std::chrono::nanoseconds{0};
        }
        return std::chrono::duration_cast<std::chrono::nanoseconds>(budget - elapsed);
    }

    /// Get time since last presentation
    [[nodiscard]] std::chrono::nanoseconds time_since_present() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - m_last_presentation
        );
    }

    // =========================================================================
    // VRR Integration
    // =========================================================================

    /// Set VRR configuration
    void set_vrr_config(std::optional<VrrConfig> config) {
        m_vrr_config = std::move(config);
        if (m_vrr_config && m_vrr_config->is_active()) {
            m_frame_budget = m_vrr_config->frame_time();
        }
    }

    /// Get VRR configuration
    [[nodiscard]] const std::optional<VrrConfig>& vrr_config() const { return m_vrr_config; }

    /// Get mutable VRR configuration
    [[nodiscard]] std::optional<VrrConfig>& vrr_config() { return m_vrr_config; }

    /// Check if VRR is active
    [[nodiscard]] bool is_vrr_active() const {
        return m_vrr_config && m_vrr_config->is_active();
    }

    /// Update content velocity for VRR adaptation
    ///
    /// Content velocity is a normalized value (0.0-1.0) that represents
    /// how much the scene is changing. Higher values indicate more motion.
    void update_content_velocity(float velocity) {
        // Smooth the velocity to avoid rapid changes
        constexpr float alpha = 0.1f;
        m_content_velocity = m_content_velocity * (1.0f - alpha) +
                            std::clamp(velocity, 0.0f, 1.0f) * alpha;

        // Adapt VRR refresh rate if enabled
        if (m_vrr_config) {
            m_vrr_config->adapt_refresh_rate(m_content_velocity);
            m_frame_budget = m_vrr_config->frame_time();
        }
    }

    /// Get current content velocity
    [[nodiscard]] float content_velocity() const { return m_content_velocity; }

private:
    /// Get effective frame budget (considering VRR)
    [[nodiscard]] std::chrono::nanoseconds effective_frame_budget() const {
        if (m_vrr_config && m_vrr_config->is_active()) {
            return m_vrr_config->frame_time();
        }
        return m_frame_budget;
    }

private:
    // Target configuration
    std::uint32_t m_target_fps;
    std::chrono::nanoseconds m_frame_budget;

    // Frame state
    std::chrono::steady_clock::time_point m_last_presentation;
    std::uint64_t m_frame_number = 0;
    std::uint64_t m_dropped_frame_count = 0;
    FrameState m_state = FrameState::WaitingForCallback;
    bool m_callback_ready = false;

    // Statistics
    std::deque<std::chrono::nanoseconds> m_frame_times;
    static constexpr std::size_t m_max_history = 120;
    std::deque<PresentationFeedback> m_feedback_history;

    // VRR
    std::optional<VrrConfig> m_vrr_config;
    float m_content_velocity = 0.0f;
};

} // namespace void_compositor
