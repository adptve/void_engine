/// @file frame.cpp
/// @brief Frame scheduler implementation
///
/// Provides out-of-line implementations for FrameScheduler functionality.

#include <void_engine/compositor/frame.hpp>
#include <void_engine/compositor/vrr.hpp>
#include <void_engine/compositor/rehydration.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace void_compositor {

// =============================================================================
// Frame Scheduler Non-Inline Implementations
// =============================================================================

// Most of FrameScheduler is header-implemented. This file provides
// a compilation unit and any future non-inline implementations.

// =============================================================================
// Frame Statistics Utilities
// =============================================================================

/// Calculate jitter (standard deviation of frame times)
[[nodiscard]] double calculate_frame_jitter(const FrameScheduler& scheduler) {
    // Get frame times for analysis
    auto avg = scheduler.average_frame_time();
    auto p50 = scheduler.frame_time_p50();
    auto p95 = scheduler.frame_time_p95();

    // Simple jitter estimate based on percentile spread
    double avg_ns = static_cast<double>(avg.count());
    double p50_ns = static_cast<double>(p50.count());
    double p95_ns = static_cast<double>(p95.count());

    if (avg_ns == 0.0) {
        return 0.0;
    }

    // Jitter as coefficient of variation
    double spread = p95_ns - p50_ns;
    return spread / avg_ns;
}

/// Check if frame timing is stable
[[nodiscard]] bool is_frame_timing_stable(const FrameScheduler& scheduler) {
    double jitter = calculate_frame_jitter(scheduler);
    return jitter < 0.15;  // 15% variation threshold
}

/// Get recommended target FPS based on current performance
[[nodiscard]] std::uint32_t recommend_target_fps(const FrameScheduler& scheduler) {
    double current_fps = scheduler.current_fps();

    // Round down to nearest common refresh rate
    if (current_fps >= 120.0) return 120;
    if (current_fps >= 90.0) return 90;
    if (current_fps >= 60.0) return 60;
    if (current_fps >= 45.0) return 45;
    if (current_fps >= 30.0) return 30;
    return 30;  // Minimum
}

// =============================================================================
// Frame Budget Utilities
// =============================================================================

/// Calculate frame budget utilization (0.0 - 1.0)
[[nodiscard]] double frame_budget_utilization(const FrameScheduler& scheduler) {
    auto budget = scheduler.frame_budget();
    auto actual = scheduler.average_frame_time();

    if (budget.count() == 0) {
        return 1.0;
    }

    return static_cast<double>(actual.count()) / static_cast<double>(budget.count());
}

/// Calculate headroom (remaining budget percentage)
[[nodiscard]] double frame_headroom(const FrameScheduler& scheduler) {
    double utilization = frame_budget_utilization(scheduler);
    return std::max(0.0, 1.0 - utilization);
}

// =============================================================================
// Frame Timing Analysis
// =============================================================================

/// Analyze frame pacing quality
struct FramePacingAnalysis {
    double average_fps;
    double target_fps;
    double jitter;
    double utilization;
    double headroom;
    bool hitting_target;
    bool stable;
    std::uint32_t dropped_frames;
    std::uint32_t recommended_fps;
};

[[nodiscard]] FramePacingAnalysis analyze_frame_pacing(const FrameScheduler& scheduler) {
    FramePacingAnalysis analysis;

    analysis.average_fps = scheduler.current_fps();
    analysis.target_fps = static_cast<double>(scheduler.target_fps());
    analysis.jitter = calculate_frame_jitter(scheduler);
    analysis.utilization = frame_budget_utilization(scheduler);
    analysis.headroom = frame_headroom(scheduler);
    analysis.hitting_target = scheduler.hitting_target();
    analysis.stable = is_frame_timing_stable(scheduler);
    analysis.dropped_frames = static_cast<std::uint32_t>(scheduler.dropped_frame_count());
    analysis.recommended_fps = recommend_target_fps(scheduler);

    return analysis;
}

// =============================================================================
// Presentation Timing
// =============================================================================

/// Calculate presentation latency statistics
struct PresentationLatencyStats {
    std::chrono::nanoseconds min_latency{0};
    std::chrono::nanoseconds max_latency{0};
    std::chrono::nanoseconds avg_latency{0};
};

[[nodiscard]] PresentationLatencyStats calculate_presentation_latency(
    const FrameScheduler& scheduler) {

    PresentationLatencyStats stats;

    const auto* feedback = scheduler.latest_feedback();
    if (!feedback) {
        return stats;
    }

    // Use latest feedback as representative
    stats.min_latency = feedback->latency;
    stats.max_latency = feedback->latency;
    stats.avg_latency = feedback->latency;

    return stats;
}

// =============================================================================
// VRR Adaptation Helpers
// =============================================================================

/// Calculate optimal VRR refresh rate for current frame times
[[nodiscard]] std::uint32_t calculate_optimal_vrr_refresh(
    const FrameScheduler& scheduler,
    std::uint32_t min_refresh,
    std::uint32_t max_refresh) {

    double avg_fps = scheduler.current_fps();

    // Clamp to VRR range
    auto target = static_cast<std::uint32_t>(std::round(avg_fps));
    return std::clamp(target, min_refresh, max_refresh);
}

/// Check if VRR would benefit current workload
[[nodiscard]] bool vrr_would_help(const FrameScheduler& scheduler) {
    double jitter = calculate_frame_jitter(scheduler);

    // VRR helps when frame times are variable
    return jitter > 0.1;  // More than 10% variation
}

// =============================================================================
// Frame State Machine
// =============================================================================

/// Get next expected frame state
[[nodiscard]] FrameState expected_next_state(FrameState current) {
    switch (current) {
        case FrameState::WaitingForCallback:
            return FrameState::ReadyToRender;
        case FrameState::ReadyToRender:
            return FrameState::Rendering;
        case FrameState::Rendering:
            return FrameState::WaitingForPresent;
        case FrameState::WaitingForPresent:
            return FrameState::Presented;
        case FrameState::Presented:
            return FrameState::WaitingForCallback;
        case FrameState::Dropped:
            return FrameState::WaitingForCallback;
    }
    return FrameState::WaitingForCallback;
}

/// Check if state transition is valid
[[nodiscard]] bool is_valid_transition(FrameState from, FrameState to) {
    // Dropped state can transition to any state
    if (from == FrameState::Dropped) {
        return true;
    }

    // Normal state machine
    return expected_next_state(from) == to;
}

} // namespace void_compositor
