#pragma once

/// @file frame.hpp
/// @brief Frame representation for void_presenter
///
/// Represents a single presentable frame with timing and state information.

#include "fwd.hpp"
#include "types.hpp"

#include <any>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace void_presenter {

// =============================================================================
// Frame
// =============================================================================

/// A single presentable frame
class Frame {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    /// Create a new frame
    Frame(std::uint64_t frame_number, std::uint32_t width, std::uint32_t height)
        : m_number(frame_number)
        , m_width(width)
        , m_height(height)
        , m_state(FrameState::Preparing)
        , m_created_at(Clock::now())
    {}

    // =========================================================================
    // Basic Properties
    // =========================================================================

    /// Get frame number
    [[nodiscard]] std::uint64_t number() const { return m_number; }

    /// Get frame state
    [[nodiscard]] FrameState state() const { return m_state; }

    /// Get frame width
    [[nodiscard]] std::uint32_t width() const { return m_width; }

    /// Get frame height
    [[nodiscard]] std::uint32_t height() const { return m_height; }

    /// Get frame size as pair
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const {
        return {m_width, m_height};
    }

    /// Get creation time
    [[nodiscard]] TimePoint created_at() const { return m_created_at; }

    // =========================================================================
    // Deadline Management
    // =========================================================================

    /// Set deadline
    void set_deadline(TimePoint deadline) {
        m_deadline = deadline;
    }

    /// Set deadline from target FPS
    void set_target_fps(std::uint32_t fps) {
        if (fps == 0) {
            m_deadline.reset();
            return;
        }
        auto frame_time = std::chrono::duration_cast<Duration>(
            std::chrono::duration<double>(1.0 / static_cast<double>(fps))
        );
        m_deadline = m_created_at + frame_time;
    }

    /// Get deadline
    [[nodiscard]] std::optional<TimePoint> deadline() const {
        return m_deadline;
    }

    /// Check if frame missed its deadline
    [[nodiscard]] bool missed_deadline() const {
        if (!m_deadline) return false;
        return Clock::now() > *m_deadline;
    }

    /// Get time until deadline
    [[nodiscard]] std::optional<Duration> time_until_deadline() const {
        if (!m_deadline) return std::nullopt;
        auto now = Clock::now();
        if (now >= *m_deadline) return Duration::zero();
        return *m_deadline - now;
    }

    // =========================================================================
    // Lifecycle Methods
    // =========================================================================

    /// Mark render start
    void begin_render() {
        m_state = FrameState::Rendering;
        m_render_start = Clock::now();
    }

    /// Mark render end
    void end_render() {
        m_state = FrameState::Ready;
        m_render_end = Clock::now();
    }

    /// Mark as presented
    void mark_presented() {
        m_state = FrameState::Presented;
        m_presented_at = Clock::now();
    }

    /// Mark as dropped
    void mark_dropped() {
        m_state = FrameState::Dropped;
    }

    // =========================================================================
    // Timing Queries
    // =========================================================================

    /// Get render duration
    [[nodiscard]] std::optional<Duration> render_duration() const {
        if (m_render_start && m_render_end) {
            return *m_render_end - *m_render_start;
        }
        return std::nullopt;
    }

    /// Get total frame time (creation to presentation)
    [[nodiscard]] std::optional<Duration> total_duration() const {
        if (m_presented_at) {
            return *m_presented_at - m_created_at;
        }
        return std::nullopt;
    }

    /// Get current latency (creation to now)
    [[nodiscard]] Duration current_latency() const {
        return Clock::now() - m_created_at;
    }

    /// Get render start time
    [[nodiscard]] std::optional<TimePoint> render_start() const {
        return m_render_start;
    }

    /// Get render end time
    [[nodiscard]] std::optional<TimePoint> render_end() const {
        return m_render_end;
    }

    /// Get presentation time
    [[nodiscard]] std::optional<TimePoint> presented_at() const {
        return m_presented_at;
    }

    // =========================================================================
    // User Data
    // =========================================================================

    /// Set user data
    template<typename T>
    void set_user_data(T&& data) {
        m_user_data = std::forward<T>(data);
    }

    /// Get user data
    template<typename T>
    [[nodiscard]] const T* user_data() const {
        if (!m_user_data.has_value()) return nullptr;
        return std::any_cast<T>(&m_user_data);
    }

    /// Get user data (mutable)
    template<typename T>
    [[nodiscard]] T* user_data() {
        if (!m_user_data.has_value()) return nullptr;
        return std::any_cast<T>(&m_user_data);
    }

    /// Take user data
    template<typename T>
    [[nodiscard]] std::optional<T> take_user_data() {
        if (!m_user_data.has_value()) return std::nullopt;
        try {
            T value = std::any_cast<T>(std::move(m_user_data));
            m_user_data.reset();
            return value;
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /// Check if has user data
    [[nodiscard]] bool has_user_data() const {
        return m_user_data.has_value();
    }

private:
    std::uint64_t m_number;
    std::uint32_t m_width;
    std::uint32_t m_height;
    FrameState m_state;
    TimePoint m_created_at;
    std::optional<TimePoint> m_deadline;
    std::optional<TimePoint> m_render_start;
    std::optional<TimePoint> m_render_end;
    std::optional<TimePoint> m_presented_at;
    std::any m_user_data;
};

// =============================================================================
// Frame Output (render target output info)
// =============================================================================

/// Frame output descriptor (render target information)
struct FrameOutput {
    std::uint64_t target_id = 0;            ///< Target identifier
    std::uint32_t width = 0;                ///< Output width
    std::uint32_t height = 0;               ///< Output height
    SurfaceFormat format = SurfaceFormat::Bgra8UnormSrgb;  ///< Output format
    std::uint32_t image_index = 0;          ///< Swapchain image index
    bool suboptimal = false;                ///< Whether surface is suboptimal
    bool cleared = false;                   ///< Whether output was cleared
};

// =============================================================================
// Frame Timing Output (presentation result)
// =============================================================================

/// Frame timing output descriptor (presentation timing result)
struct FrameTimingOutput {
    std::uint64_t frame_number = 0;     ///< Frame number
    std::uint32_t width = 0;            ///< Frame width
    std::uint32_t height = 0;           ///< Frame height
    std::uint64_t render_time_us = 0;   ///< Render time in microseconds
    std::uint64_t total_time_us = 0;    ///< Total time in microseconds
    bool missed_deadline = false;       ///< Whether deadline was missed
    bool dropped = false;               ///< Whether frame was dropped

    /// Create from frame
    [[nodiscard]] static FrameTimingOutput from_frame(const Frame& frame) {
        FrameTimingOutput output;
        output.frame_number = frame.number();
        output.width = frame.width();
        output.height = frame.height();

        if (auto dur = frame.render_duration()) {
            output.render_time_us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(*dur).count()
            );
        }

        if (auto dur = frame.total_duration()) {
            output.total_time_us = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(*dur).count()
            );
        }

        output.missed_deadline = frame.missed_deadline();
        output.dropped = frame.state() == FrameState::Dropped;

        return output;
    }
};

// =============================================================================
// Per-Frame Statistics (timing data for individual frames)
// =============================================================================

/// Per-frame timing statistics
struct FrameStats {
    std::uint64_t frame_number = 0;         ///< Frame number
    FrameState state = FrameState::Idle;    ///< Current frame state
    std::int64_t cpu_time_us = 0;           ///< CPU time in microseconds
    std::int64_t gpu_time_us = 0;           ///< GPU time in microseconds
    std::int64_t present_latency_us = 0;    ///< Presentation latency in microseconds
    std::int64_t total_frame_time_us = 0;   ///< Total frame time in microseconds
};

// =============================================================================
// Aggregate Frame Statistics
// =============================================================================

/// Aggregate frame statistics tracker (over many frames)
struct AggregateFrameStats {
    std::uint64_t total_frames = 0;         ///< Total frames
    std::uint64_t presented_frames = 0;     ///< Presented frames
    std::uint64_t dropped_frames = 0;       ///< Dropped frames
    double avg_render_time_us = 0.0;        ///< Average render time (us)
    double avg_total_time_us = 0.0;         ///< Average total time (us)
    std::uint64_t min_render_time_us = 0;   ///< Min render time (us)
    std::uint64_t max_render_time_us = 0;   ///< Max render time (us)
    std::uint64_t deadline_misses = 0;      ///< Deadline miss count

    /// Update stats with new frame output
    void update(const FrameTimingOutput& output) {
        ++total_frames;

        if (output.dropped) {
            ++dropped_frames;
        } else {
            ++presented_frames;
        }

        if (output.missed_deadline) {
            ++deadline_misses;
        }

        // Update render time stats
        if (output.render_time_us > 0) {
            double n = static_cast<double>(presented_frames);
            avg_render_time_us =
                (avg_render_time_us * (n - 1.0) + static_cast<double>(output.render_time_us)) / n;
            avg_total_time_us =
                (avg_total_time_us * (n - 1.0) + static_cast<double>(output.total_time_us)) / n;

            if (min_render_time_us == 0 || output.render_time_us < min_render_time_us) {
                min_render_time_us = output.render_time_us;
            }
            if (output.render_time_us > max_render_time_us) {
                max_render_time_us = output.render_time_us;
            }
        }
    }

    /// Get frame drop rate (0.0 - 1.0)
    [[nodiscard]] double drop_rate() const {
        if (total_frames == 0) return 0.0;
        return static_cast<double>(dropped_frames) / static_cast<double>(total_frames);
    }

    /// Get deadline miss rate (0.0 - 1.0)
    [[nodiscard]] double deadline_miss_rate() const {
        if (total_frames == 0) return 0.0;
        return static_cast<double>(deadline_misses) / static_cast<double>(total_frames);
    }

    /// Get average FPS based on total time
    [[nodiscard]] double average_fps() const {
        if (avg_total_time_us <= 0.0) return 0.0;
        return 1'000'000.0 / avg_total_time_us;
    }

    /// Reset statistics
    void reset() {
        *this = AggregateFrameStats{};
    }
};

// =============================================================================
// GPU Frame (for low-level frame tracking)
// =============================================================================

/// Low-level GPU frame for command recording (used by debug utilities)
struct GpuFrame {
    std::uint64_t number = 0;                               ///< Frame number
    FrameState state = FrameState::Idle;                    ///< Frame state
    std::chrono::steady_clock::time_point cpu_begin;        ///< CPU work begin time
    std::chrono::steady_clock::time_point cpu_end;          ///< CPU work end time
    std::vector<FrameOutput> outputs;                       ///< Frame outputs
};

} // namespace void_presenter
