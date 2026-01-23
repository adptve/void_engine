#pragma once

/// @file timing.hpp
/// @brief Frame timing and synchronization for void_presenter
///
/// Provides frame timing, VSync control, and pacing.

#include "fwd.hpp"
#include "types.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>

namespace void_presenter {

// =============================================================================
// Frame Timing
// =============================================================================

/// Frame timing tracker
class FrameTiming {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using TimePoint = Clock::time_point;

    /// Create new frame timing with target FPS
    explicit FrameTiming(std::uint32_t target_fps = 60)
        : m_history_size(120) {
        set_target_fps(target_fps);
        m_frame_times.reserve(m_history_size);
    }

    /// Create with unlimited FPS
    [[nodiscard]] static FrameTiming unlimited() {
        FrameTiming timing(0);
        return timing;
    }

    // =========================================================================
    // Target FPS
    // =========================================================================

    /// Set target FPS (0 = unlimited)
    void set_target_fps(std::uint32_t fps) {
        if (fps == 0) {
            m_target_frame_time = Duration::zero();
        } else {
            m_target_frame_time = std::chrono::duration_cast<Duration>(
                std::chrono::duration<double>(1.0 / static_cast<double>(fps))
            );
        }
    }

    /// Get target frame time
    [[nodiscard]] Duration target_frame_time() const {
        return m_target_frame_time;
    }

    /// Get target FPS
    [[nodiscard]] double target_fps() const {
        if (m_target_frame_time == Duration::zero()) {
            return std::numeric_limits<double>::infinity();
        }
        return 1.0 / std::chrono::duration<double>(m_target_frame_time).count();
    }

    // =========================================================================
    // Frame Tracking
    // =========================================================================

    /// Mark frame start
    /// @return Current time
    TimePoint begin_frame() {
        auto now = Clock::now();

        if (m_last_frame_start) {
            m_last_frame_duration = now - *m_last_frame_start;
            m_total_elapsed += m_last_frame_duration;

            // Update history
            if (m_frame_times.size() >= m_history_size) {
                m_frame_times.erase(m_frame_times.begin());
            }
            m_frame_times.push_back(m_last_frame_duration);
        }

        m_last_frame_start = now;
        ++m_frame_count;
        return now;
    }

    /// Get time to wait before next frame (for frame pacing)
    [[nodiscard]] Duration time_to_wait() const {
        if (m_target_frame_time == Duration::zero()) {
            return Duration::zero();
        }

        if (!m_last_frame_start) {
            return Duration::zero();
        }

        auto elapsed = Clock::now() - *m_last_frame_start;
        if (elapsed < m_target_frame_time) {
            return m_target_frame_time - elapsed;
        }
        return Duration::zero();
    }

    /// Wait until next frame target (blocking)
    void wait_for_next_frame() const {
        auto wait = time_to_wait();
        if (wait > Duration::zero()) {
            std::this_thread::sleep_for(wait);
        }
    }

    // =========================================================================
    // Timing Queries
    // =========================================================================

    /// Get last frame duration
    [[nodiscard]] Duration last_frame_duration() const {
        return m_last_frame_duration;
    }

    /// Get last frame duration as delta time (seconds)
    [[nodiscard]] float delta_time() const {
        return std::chrono::duration<float>(m_last_frame_duration).count();
    }

    /// Get average frame duration
    [[nodiscard]] Duration average_frame_duration() const {
        if (m_frame_times.empty()) {
            return m_target_frame_time;
        }

        Duration sum = Duration::zero();
        for (const auto& d : m_frame_times) {
            sum += d;
        }
        return sum / static_cast<int>(m_frame_times.size());
    }

    /// Get average FPS
    [[nodiscard]] double average_fps() const {
        auto avg = average_frame_duration();
        if (avg == Duration::zero()) return 0.0;
        return 1.0 / std::chrono::duration<double>(avg).count();
    }

    /// Get instant FPS (from last frame)
    [[nodiscard]] double instant_fps() const {
        if (m_last_frame_duration == Duration::zero()) return 0.0;
        return 1.0 / std::chrono::duration<double>(m_last_frame_duration).count();
    }

    /// Get frame time percentile (0-100)
    [[nodiscard]] Duration frame_time_percentile(std::uint32_t percentile) const {
        if (m_frame_times.empty()) {
            return Duration::zero();
        }

        auto sorted = m_frame_times;
        std::sort(sorted.begin(), sorted.end());

        std::size_t idx = static_cast<std::size_t>(
            (std::min(percentile, 100u) / 100.0) * (sorted.size() - 1)
        );
        return sorted[idx];
    }

    /// Get total elapsed time
    [[nodiscard]] Duration total_elapsed() const {
        return m_total_elapsed;
    }

    /// Get total frame count
    [[nodiscard]] std::uint64_t frame_count() const {
        return m_frame_count;
    }

    /// Reset timing
    void reset() {
        m_last_frame_start.reset();
        m_last_frame_duration = Duration::zero();
        m_frame_times.clear();
        m_total_elapsed = Duration::zero();
        m_frame_count = 0;
    }

private:
    Duration m_target_frame_time = Duration::zero();
    std::optional<TimePoint> m_last_frame_start;
    Duration m_last_frame_duration = Duration::zero();
    std::vector<Duration> m_frame_times;
    std::size_t m_history_size;
    Duration m_total_elapsed = Duration::zero();
    std::uint64_t m_frame_count = 0;
};

// =============================================================================
// Frame Limiter
// =============================================================================

/// Frame limiter for CPU-side frame pacing
class FrameLimiter {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using TimePoint = Clock::time_point;

    /// Create new frame limiter with target FPS
    explicit FrameLimiter(std::uint32_t target_fps = 60)
        : m_last_frame(Clock::now()) {
        set_target_fps(target_fps);
    }

    /// Create unlimited (no limiting)
    [[nodiscard]] static FrameLimiter unlimited() {
        return FrameLimiter(0);
    }

    /// Set target FPS (0 = unlimited)
    void set_target_fps(std::uint32_t fps) {
        if (fps == 0) {
            m_target_frame_time = Duration::zero();
        } else {
            m_target_frame_time = std::chrono::duration_cast<Duration>(
                std::chrono::duration<double>(1.0 / static_cast<double>(fps))
            );
        }
    }

    /// Get target FPS
    [[nodiscard]] double target_fps() const {
        if (m_target_frame_time == Duration::zero()) {
            return std::numeric_limits<double>::infinity();
        }
        return 1.0 / std::chrono::duration<double>(m_target_frame_time).count();
    }

    /// Wait for next frame (blocking with busy-wait for accuracy)
    void wait() {
        if (m_target_frame_time == Duration::zero()) {
            m_last_frame = Clock::now();
            return;
        }

        auto elapsed = Clock::now() - m_last_frame;
        auto target = m_target_frame_time;

        // Compensate for previous oversleep
        if (m_oversleep_compensation > Duration::zero()) {
            if (target > m_oversleep_compensation) {
                target -= m_oversleep_compensation;
            } else {
                target = Duration::zero();
            }
        }

        if (elapsed < target) {
            auto sleep_time = target - elapsed;

            // Sleep most of the time, then busy-wait for accuracy
            constexpr auto busy_wait_threshold = std::chrono::milliseconds(2);
            if (sleep_time > busy_wait_threshold) {
                std::this_thread::sleep_for(sleep_time - std::chrono::milliseconds(1));
            }

            // Busy-wait for the rest
            while (Clock::now() - m_last_frame < m_target_frame_time) {
                // Spin
            }
        }

        // Track oversleep for compensation
        auto actual_elapsed = Clock::now() - m_last_frame;
        if (actual_elapsed > m_target_frame_time) {
            m_oversleep_compensation = std::min(
                actual_elapsed - m_target_frame_time,
                std::chrono::milliseconds(5)
            );
        } else {
            m_oversleep_compensation = Duration::zero();
        }

        m_last_frame = Clock::now();
    }

    /// Mark frame start (for when you don't want to wait)
    void mark_frame() {
        m_last_frame = Clock::now();
    }

    /// Get time since last frame
    [[nodiscard]] Duration elapsed() const {
        return Clock::now() - m_last_frame;
    }

private:
    Duration m_target_frame_time = Duration::zero();
    TimePoint m_last_frame;
    Duration m_oversleep_compensation = Duration::zero();
};

} // namespace void_presenter
