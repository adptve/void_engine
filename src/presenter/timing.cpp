/// @file timing.cpp
/// @brief Frame timing implementation for void_presenter

#include <void_engine/presenter/timing.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace void_presenter {

// =============================================================================
// FrameTiming Utilities
// =============================================================================

std::string format_frame_timing(const FrameTiming& timing) {
    std::string result = "FrameTiming {\n";

    result += "  frame_number: " + std::to_string(timing.frame_number) + "\n";

    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(
        timing.present_end - timing.frame_begin).count();
    result += "  total_time: " + std::to_string(total_us) + " us\n";

    auto cpu_us = std::chrono::duration_cast<std::chrono::microseconds>(
        timing.cpu_end - timing.cpu_begin).count();
    result += "  cpu_time: " + std::to_string(cpu_us) + " us\n";

    if (timing.gpu_timestamp_valid) {
        result += "  gpu_time: " + std::to_string(timing.gpu_time_ns / 1000) + " us\n";
    }

    if (timing.fps() > 0.0) {
        result += "  fps: " + std::to_string(timing.fps()) + "\n";
    }

    result += "}";
    return result;
}

FrameTiming create_frame_timing(std::uint64_t frame_number) {
    FrameTiming timing;
    timing.frame_number = frame_number;
    timing.frame_begin = std::chrono::steady_clock::now();
    timing.cpu_begin = timing.frame_begin;
    timing.cpu_end = timing.frame_begin;
    timing.gpu_begin = timing.frame_begin;
    timing.gpu_end = timing.frame_begin;
    timing.present_begin = timing.frame_begin;
    timing.present_end = timing.frame_begin;
    timing.gpu_timestamp_valid = false;
    timing.gpu_time_ns = 0;
    return timing;
}

void begin_cpu_timing(FrameTiming& timing) {
    timing.cpu_begin = std::chrono::steady_clock::now();
}

void end_cpu_timing(FrameTiming& timing) {
    timing.cpu_end = std::chrono::steady_clock::now();
}

void begin_gpu_timing(FrameTiming& timing) {
    timing.gpu_begin = std::chrono::steady_clock::now();
}

void end_gpu_timing(FrameTiming& timing) {
    timing.gpu_end = std::chrono::steady_clock::now();
}

void begin_present_timing(FrameTiming& timing) {
    timing.present_begin = std::chrono::steady_clock::now();
}

void end_present_timing(FrameTiming& timing) {
    timing.present_end = std::chrono::steady_clock::now();
}

void set_gpu_timestamp(FrameTiming& timing, std::int64_t ns) {
    timing.gpu_time_ns = ns;
    timing.gpu_timestamp_valid = true;
}

// =============================================================================
// FrameLimiter Extended Implementation
// =============================================================================

/// High-precision frame limiter with adaptive sleep
class AdaptiveFrameLimiter {
public:
    explicit AdaptiveFrameLimiter(float target_fps = 60.0f)
        : m_target_fps(target_fps)
        , m_frame_count(0)
        , m_overshoot_us(0.0)
        , m_sleep_granularity_us(1000.0) // 1ms default
    {
        update_target_frame_time();
        calibrate_sleep_granularity();
    }

    /// Set target FPS
    void set_target_fps(float fps) {
        m_target_fps = fps;
        update_target_frame_time();
    }

    [[nodiscard]] float target_fps() const { return m_target_fps; }

    /// Wait until next frame should start
    /// Returns actual time waited
    std::chrono::nanoseconds wait() {
        auto now = std::chrono::steady_clock::now();

        if (m_frame_count == 0) {
            m_last_frame_time = now;
            ++m_frame_count;
            return std::chrono::nanoseconds(0);
        }

        // Calculate target time for this frame
        auto elapsed = now - m_last_frame_time;
        auto target = m_target_frame_time;

        // Adjust for previous overshoot
        auto adjusted_target = target - std::chrono::nanoseconds(
            static_cast<std::int64_t>(m_overshoot_us * 1000.0));

        if (elapsed >= adjusted_target) {
            // Already past target - track overshoot
            auto overshoot = elapsed - adjusted_target;
            m_overshoot_us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(overshoot).count());
            // Decay overshoot tracking
            m_overshoot_us *= 0.5;

            m_last_frame_time = now;
            ++m_frame_count;
            return std::chrono::nanoseconds(0);
        }

        auto remaining = adjusted_target - elapsed;

        // Two-phase wait: sleep for most of the time, then spin for precision
        auto sleep_threshold = std::chrono::nanoseconds(
            static_cast<std::int64_t>(m_sleep_granularity_us * 1500.0)); // 1.5x granularity

        if (remaining > sleep_threshold) {
            // Sleep for the bulk of the wait
            auto sleep_time = remaining - sleep_threshold;
            std::this_thread::sleep_for(sleep_time);
        }

        // Spin-wait for remaining time (higher precision)
        auto spin_start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() < m_last_frame_time + adjusted_target) {
            // Yield to prevent 100% CPU
            std::this_thread::yield();
        }

        auto actual_end = std::chrono::steady_clock::now();
        auto total_wait = actual_end - now;

        // Track overshoot for next frame
        auto actual_elapsed = actual_end - m_last_frame_time;
        if (actual_elapsed > target) {
            auto overshoot = actual_elapsed - target;
            m_overshoot_us = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(overshoot).count());
        } else {
            m_overshoot_us *= 0.9; // Decay
        }

        m_last_frame_time = actual_end;
        ++m_frame_count;

        return total_wait;
    }

    /// Reset timing
    void reset() {
        m_frame_count = 0;
        m_overshoot_us = 0.0;
    }

    [[nodiscard]] std::uint64_t frame_count() const { return m_frame_count; }

private:
    void update_target_frame_time() {
        if (m_target_fps <= 0.0f) {
            m_target_frame_time = std::chrono::nanoseconds(0);
        } else {
            double ns = 1'000'000'000.0 / static_cast<double>(m_target_fps);
            m_target_frame_time = std::chrono::nanoseconds(static_cast<std::int64_t>(ns));
        }
    }

    void calibrate_sleep_granularity() {
        // Measure actual sleep granularity
        constexpr int samples = 5;
        double total_us = 0.0;

        for (int i = 0; i < samples; ++i) {
            auto start = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            auto end = std::chrono::steady_clock::now();

            auto actual = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            total_us += static_cast<double>(actual.count());
        }

        m_sleep_granularity_us = std::max(total_us / static_cast<double>(samples), 500.0);
    }

    float m_target_fps;
    std::chrono::nanoseconds m_target_frame_time;
    std::chrono::steady_clock::time_point m_last_frame_time;
    std::uint64_t m_frame_count;
    double m_overshoot_us;
    double m_sleep_granularity_us;
};

// =============================================================================
// Frame Time History
// =============================================================================

/// Maintains history of frame times for analysis
class FrameTimeHistory {
public:
    explicit FrameTimeHistory(std::size_t capacity = 300)
        : m_capacity(capacity)
    {
        m_times.reserve(capacity);
    }

    void record(std::chrono::nanoseconds frame_time) {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(frame_time).count();

        if (m_times.size() >= m_capacity) {
            m_times.erase(m_times.begin());
        }
        m_times.push_back(us);
    }

    [[nodiscard]] double average_ms() const {
        if (m_times.empty()) return 0.0;
        double sum = std::accumulate(m_times.begin(), m_times.end(), 0.0);
        return (sum / static_cast<double>(m_times.size())) / 1000.0;
    }

    [[nodiscard]] double average_fps() const {
        double avg_ms = average_ms();
        return avg_ms > 0.0 ? 1000.0 / avg_ms : 0.0;
    }

    [[nodiscard]] double percentile_ms(double p) const {
        if (m_times.empty()) return 0.0;

        std::vector<std::int64_t> sorted = m_times;
        std::sort(sorted.begin(), sorted.end());

        double index = p * static_cast<double>(sorted.size() - 1);
        std::size_t lower = static_cast<std::size_t>(index);
        std::size_t upper = lower + 1;

        if (upper >= sorted.size()) {
            return static_cast<double>(sorted.back()) / 1000.0;
        }

        double fraction = index - static_cast<double>(lower);
        double value = static_cast<double>(sorted[lower]) * (1.0 - fraction) +
                       static_cast<double>(sorted[upper]) * fraction;
        return value / 1000.0;
    }

    [[nodiscard]] double min_ms() const {
        if (m_times.empty()) return 0.0;
        return static_cast<double>(*std::min_element(m_times.begin(), m_times.end())) / 1000.0;
    }

    [[nodiscard]] double max_ms() const {
        if (m_times.empty()) return 0.0;
        return static_cast<double>(*std::max_element(m_times.begin(), m_times.end())) / 1000.0;
    }

    [[nodiscard]] double stddev_ms() const {
        if (m_times.size() < 2) return 0.0;

        double avg = average_ms() * 1000.0; // Convert back to us
        double variance_sum = 0.0;
        for (const auto& t : m_times) {
            double diff = static_cast<double>(t) - avg;
            variance_sum += diff * diff;
        }
        return std::sqrt(variance_sum / static_cast<double>(m_times.size())) / 1000.0;
    }

    [[nodiscard]] std::size_t count() const { return m_times.size(); }
    [[nodiscard]] std::size_t capacity() const { return m_capacity; }

    void clear() { m_times.clear(); }

    [[nodiscard]] const std::vector<std::int64_t>& raw_times_us() const { return m_times; }

private:
    std::vector<std::int64_t> m_times;
    std::size_t m_capacity;
};

// =============================================================================
// Display Refresh Rate Detection
// =============================================================================

/// Detect display refresh rate
struct DisplayRefreshInfo {
    double refresh_rate_hz = 60.0;
    double frame_time_ms = 16.667;
    bool vrr_supported = false;
    double vrr_min_hz = 0.0;
    double vrr_max_hz = 0.0;
};

DisplayRefreshInfo detect_display_refresh() {
    DisplayRefreshInfo info;

    // Default to 60Hz - actual detection would require platform-specific APIs
    info.refresh_rate_hz = 60.0;
    info.frame_time_ms = 1000.0 / info.refresh_rate_hz;
    info.vrr_supported = false;

    return info;
}

/// Calculate ideal frame pacing for a target refresh rate
std::chrono::nanoseconds calculate_frame_budget(double refresh_rate_hz) {
    if (refresh_rate_hz <= 0.0) {
        return std::chrono::nanoseconds(0);
    }
    double ns = 1'000'000'000.0 / refresh_rate_hz;
    return std::chrono::nanoseconds(static_cast<std::int64_t>(ns));
}

// =============================================================================
// Timing Statistics
// =============================================================================

/// Comprehensive timing statistics
struct TimingStatistics {
    // Frame time stats (in microseconds)
    double avg_frame_time_us = 0.0;
    double min_frame_time_us = 0.0;
    double max_frame_time_us = 0.0;
    double p50_frame_time_us = 0.0;
    double p95_frame_time_us = 0.0;
    double p99_frame_time_us = 0.0;
    double stddev_us = 0.0;

    // FPS stats
    double avg_fps = 0.0;
    double p01_fps = 0.0; // 1% low FPS
    double p001_fps = 0.0; // 0.1% low FPS

    // Consistency metrics
    double frame_time_variance = 0.0;
    double jitter_ratio = 0.0; // % of frames outside ±10% of average
    std::size_t stutter_count = 0; // Frames > 2x average

    // Totals
    std::size_t frame_count = 0;
    double total_time_seconds = 0.0;
};

TimingStatistics calculate_timing_statistics(const FrameTimeHistory& history) {
    TimingStatistics stats;

    const auto& times = history.raw_times_us();
    if (times.empty()) {
        return stats;
    }

    stats.frame_count = times.size();

    // Calculate sum for average
    double sum = 0.0;
    double min_val = static_cast<double>(times[0]);
    double max_val = static_cast<double>(times[0]);

    for (const auto& t : times) {
        double v = static_cast<double>(t);
        sum += v;
        min_val = std::min(min_val, v);
        max_val = std::max(max_val, v);
    }

    double n = static_cast<double>(times.size());
    stats.avg_frame_time_us = sum / n;
    stats.min_frame_time_us = min_val;
    stats.max_frame_time_us = max_val;
    stats.total_time_seconds = sum / 1'000'000.0;

    // FPS from average
    if (stats.avg_frame_time_us > 0.0) {
        stats.avg_fps = 1'000'000.0 / stats.avg_frame_time_us;
    }

    // Variance and stddev
    double variance_sum = 0.0;
    for (const auto& t : times) {
        double diff = static_cast<double>(t) - stats.avg_frame_time_us;
        variance_sum += diff * diff;
    }
    stats.frame_time_variance = variance_sum / n;
    stats.stddev_us = std::sqrt(stats.frame_time_variance);

    // Sort for percentiles
    std::vector<std::int64_t> sorted = times;
    std::sort(sorted.begin(), sorted.end());

    auto get_percentile = [&sorted](double p) -> double {
        double index = p * static_cast<double>(sorted.size() - 1);
        std::size_t lower = static_cast<std::size_t>(index);
        std::size_t upper = lower + 1;
        if (upper >= sorted.size()) return static_cast<double>(sorted.back());
        double fraction = index - static_cast<double>(lower);
        return static_cast<double>(sorted[lower]) * (1.0 - fraction) +
               static_cast<double>(sorted[upper]) * fraction;
    };

    stats.p50_frame_time_us = get_percentile(0.50);
    stats.p95_frame_time_us = get_percentile(0.95);
    stats.p99_frame_time_us = get_percentile(0.99);

    // 1% and 0.1% low FPS (based on 99th and 99.9th percentile frame times)
    double p99_time = get_percentile(0.99);
    double p999_time = get_percentile(0.999);
    stats.p01_fps = p99_time > 0.0 ? 1'000'000.0 / p99_time : 0.0;
    stats.p001_fps = p999_time > 0.0 ? 1'000'000.0 / p999_time : 0.0;

    // Jitter and stutter analysis
    double jitter_threshold = stats.avg_frame_time_us * 0.1; // 10%
    double stutter_threshold = stats.avg_frame_time_us * 2.0; // 200%
    std::size_t jitter_count = 0;

    for (const auto& t : times) {
        double diff = std::abs(static_cast<double>(t) - stats.avg_frame_time_us);
        if (diff > jitter_threshold) {
            ++jitter_count;
        }
        if (static_cast<double>(t) > stutter_threshold) {
            ++stats.stutter_count;
        }
    }

    stats.jitter_ratio = static_cast<double>(jitter_count) / n;

    return stats;
}

std::string format_timing_statistics(const TimingStatistics& stats) {
    std::string result = "TimingStatistics {\n";

    result += "  frame_count: " + std::to_string(stats.frame_count) + "\n";
    result += "  total_time: " + std::to_string(stats.total_time_seconds) + " s\n";

    result += "  avg_fps: " + std::to_string(stats.avg_fps) + "\n";
    result += "  1% low fps: " + std::to_string(stats.p01_fps) + "\n";
    result += "  0.1% low fps: " + std::to_string(stats.p001_fps) + "\n";

    result += "  avg_frame_time: " + std::to_string(stats.avg_frame_time_us / 1000.0) + " ms\n";
    result += "  min_frame_time: " + std::to_string(stats.min_frame_time_us / 1000.0) + " ms\n";
    result += "  max_frame_time: " + std::to_string(stats.max_frame_time_us / 1000.0) + " ms\n";
    result += "  stddev: " + std::to_string(stats.stddev_us / 1000.0) + " ms\n";

    result += "  p50: " + std::to_string(stats.p50_frame_time_us / 1000.0) + " ms\n";
    result += "  p95: " + std::to_string(stats.p95_frame_time_us / 1000.0) + " ms\n";
    result += "  p99: " + std::to_string(stats.p99_frame_time_us / 1000.0) + " ms\n";

    result += "  jitter_ratio: " + std::to_string(stats.jitter_ratio * 100.0) + "%\n";
    result += "  stutter_count: " + std::to_string(stats.stutter_count) + "\n";

    result += "}";
    return result;
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_frame_limiter(const FrameLimiter& limiter) {
    std::string result = "FrameLimiter {\n";

    result += "  target_fps: " + std::to_string(limiter.target_fps()) + "\n";
    result += "  enabled: " + std::string(limiter.enabled() ? "true" : "false") + "\n";
    result += "  frame_count: " + std::to_string(limiter.frame_count()) + "\n";

    result += "}";
    return result;
}

} // namespace debug

} // namespace void_presenter
