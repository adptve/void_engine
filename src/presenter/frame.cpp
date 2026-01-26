/// @file frame.cpp
/// @brief Frame management implementation for void_presenter

#include <void_engine/presenter/frame.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <vector>

namespace void_presenter {

// =============================================================================
// Frame Utilities
// =============================================================================

namespace {

/// Global frame number counter
std::atomic<std::uint64_t> g_frame_counter{0};

} // anonymous namespace

std::uint64_t generate_frame_number() {
    return g_frame_counter.fetch_add(1, std::memory_order_relaxed);
}

void reset_frame_counter() {
    g_frame_counter.store(0, std::memory_order_relaxed);
}

std::uint64_t current_frame_number() {
    return g_frame_counter.load(std::memory_order_relaxed);
}

// =============================================================================
// FrameState Utilities
// =============================================================================

bool is_active_frame_state(FrameState state) {
    switch (state) {
        case FrameState::Acquired:
        case FrameState::Recording:
        case FrameState::Submitted:
            return true;

        case FrameState::Idle:
        case FrameState::Presented:
        case FrameState::Dropped:
            return false;
    }
    return false;
}

bool is_terminal_frame_state(FrameState state) {
    return state == FrameState::Presented || state == FrameState::Dropped;
}

const char* frame_state_name(FrameState state) {
    switch (state) {
        case FrameState::Idle: return "Idle";
        case FrameState::Acquired: return "Acquired";
        case FrameState::Recording: return "Recording";
        case FrameState::Submitted: return "Submitted";
        case FrameState::Presented: return "Presented";
        case FrameState::Dropped: return "Dropped";
    }
    return "Unknown";
}

// =============================================================================
// FrameStats Utilities
// =============================================================================

std::string format_frame_stats(const FrameStats& stats) {
    std::string result = "FrameStats {\n";

    result += "  frame_number: " + std::to_string(stats.frame_number) + "\n";
    result += "  state: " + std::string(frame_state_name(stats.state)) + "\n";

    result += "  cpu_time_us: " + std::to_string(stats.cpu_time_us) + "\n";
    result += "  gpu_time_us: " + std::to_string(stats.gpu_time_us) + "\n";
    result += "  present_latency_us: " + std::to_string(stats.present_latency_us) + "\n";
    result += "  total_frame_time_us: " + std::to_string(stats.total_frame_time_us) + "\n";

    if (stats.total_frame_time_us > 0) {
        double fps = 1'000'000.0 / static_cast<double>(stats.total_frame_time_us);
        result += "  fps: " + std::to_string(fps) + "\n";
    }

    result += "}";
    return result;
}

FrameStats merge_frame_stats(const std::vector<FrameStats>& stats) {
    if (stats.empty()) {
        return FrameStats{};
    }

    FrameStats merged;
    merged.frame_number = stats.back().frame_number;
    merged.state = stats.back().state;

    // Calculate averages
    double total_cpu = 0.0;
    double total_gpu = 0.0;
    double total_present = 0.0;
    double total_frame = 0.0;

    for (const auto& s : stats) {
        total_cpu += static_cast<double>(s.cpu_time_us);
        total_gpu += static_cast<double>(s.gpu_time_us);
        total_present += static_cast<double>(s.present_latency_us);
        total_frame += static_cast<double>(s.total_frame_time_us);
    }

    double n = static_cast<double>(stats.size());
    merged.cpu_time_us = static_cast<std::int64_t>(total_cpu / n);
    merged.gpu_time_us = static_cast<std::int64_t>(total_gpu / n);
    merged.present_latency_us = static_cast<std::int64_t>(total_present / n);
    merged.total_frame_time_us = static_cast<std::int64_t>(total_frame / n);

    return merged;
}

// =============================================================================
// FrameOutput Utilities
// =============================================================================

std::string format_frame_output(const FrameOutput& output) {
    std::string result = "FrameOutput {\n";

    result += "  target_id: " + std::to_string(output.target_id) + "\n";
    result += "  size: " + std::to_string(output.width) + "x" + std::to_string(output.height) + "\n";
    result += "  format: " + std::string(to_string(output.format)) + "\n";
    result += "  image_index: " + std::to_string(output.image_index) + "\n";
    result += "  suboptimal: " + std::string(output.suboptimal ? "true" : "false") + "\n";
    result += "  cleared: " + std::string(output.cleared ? "true" : "false") + "\n";

    result += "}";
    return result;
}

bool is_output_valid(const FrameOutput& output) {
    return output.width > 0 && output.height > 0;
}

bool output_needs_clear(const FrameOutput& output) {
    return !output.cleared;
}

// =============================================================================
// Frame Timing Analysis
// =============================================================================

namespace {

/// Calculate percentile from sorted values
template<typename T>
T percentile(const std::vector<T>& sorted_values, double p) {
    if (sorted_values.empty()) return T{};
    if (p <= 0.0) return sorted_values.front();
    if (p >= 1.0) return sorted_values.back();

    double index = p * static_cast<double>(sorted_values.size() - 1);
    std::size_t lower = static_cast<std::size_t>(index);
    std::size_t upper = lower + 1;

    if (upper >= sorted_values.size()) {
        return sorted_values.back();
    }

    double fraction = index - static_cast<double>(lower);
    return static_cast<T>(
        static_cast<double>(sorted_values[lower]) * (1.0 - fraction) +
        static_cast<double>(sorted_values[upper]) * fraction
    );
}

} // anonymous namespace

/// Frame timing analysis result
struct FrameTimingAnalysis {
    double avg_frame_time_us = 0.0;
    double min_frame_time_us = 0.0;
    double max_frame_time_us = 0.0;
    double p50_frame_time_us = 0.0;
    double p95_frame_time_us = 0.0;
    double p99_frame_time_us = 0.0;
    double stddev_us = 0.0;
    double avg_fps = 0.0;
    double stutter_ratio = 0.0; // Frames that took > 2x average
};

FrameTimingAnalysis analyze_frame_timings(const std::vector<std::int64_t>& frame_times_us) {
    FrameTimingAnalysis analysis;

    if (frame_times_us.empty()) {
        return analysis;
    }

    // Calculate sum and average
    double sum = 0.0;
    double min_val = static_cast<double>(frame_times_us[0]);
    double max_val = static_cast<double>(frame_times_us[0]);

    for (const auto& t : frame_times_us) {
        double v = static_cast<double>(t);
        sum += v;
        min_val = std::min(min_val, v);
        max_val = std::max(max_val, v);
    }

    double n = static_cast<double>(frame_times_us.size());
    analysis.avg_frame_time_us = sum / n;
    analysis.min_frame_time_us = min_val;
    analysis.max_frame_time_us = max_val;

    // Calculate average FPS
    if (analysis.avg_frame_time_us > 0.0) {
        analysis.avg_fps = 1'000'000.0 / analysis.avg_frame_time_us;
    }

    // Calculate standard deviation
    double variance_sum = 0.0;
    for (const auto& t : frame_times_us) {
        double diff = static_cast<double>(t) - analysis.avg_frame_time_us;
        variance_sum += diff * diff;
    }
    analysis.stddev_us = std::sqrt(variance_sum / n);

    // Sort for percentiles
    std::vector<std::int64_t> sorted = frame_times_us;
    std::sort(sorted.begin(), sorted.end());

    analysis.p50_frame_time_us = static_cast<double>(percentile(sorted, 0.50));
    analysis.p95_frame_time_us = static_cast<double>(percentile(sorted, 0.95));
    analysis.p99_frame_time_us = static_cast<double>(percentile(sorted, 0.99));

    // Count stutters (frames > 2x average)
    double stutter_threshold = analysis.avg_frame_time_us * 2.0;
    std::size_t stutter_count = 0;
    for (const auto& t : frame_times_us) {
        if (static_cast<double>(t) > stutter_threshold) {
            ++stutter_count;
        }
    }
    analysis.stutter_ratio = static_cast<double>(stutter_count) / n;

    return analysis;
}

std::string format_frame_timing_analysis(const FrameTimingAnalysis& analysis) {
    std::string result = "FrameTimingAnalysis {\n";

    result += "  avg_frame_time: " + std::to_string(analysis.avg_frame_time_us) + " us\n";
    result += "  min_frame_time: " + std::to_string(analysis.min_frame_time_us) + " us\n";
    result += "  max_frame_time: " + std::to_string(analysis.max_frame_time_us) + " us\n";
    result += "  stddev: " + std::to_string(analysis.stddev_us) + " us\n";

    result += "  p50: " + std::to_string(analysis.p50_frame_time_us) + " us\n";
    result += "  p95: " + std::to_string(analysis.p95_frame_time_us) + " us\n";
    result += "  p99: " + std::to_string(analysis.p99_frame_time_us) + " us\n";

    result += "  avg_fps: " + std::to_string(analysis.avg_fps) + "\n";
    result += "  stutter_ratio: " + std::to_string(analysis.stutter_ratio * 100.0) + "%\n";

    result += "}";
    return result;
}

// =============================================================================
// Frame Ring Buffer
// =============================================================================

/// Ring buffer for tracking recent frame statistics
class FrameStatsRingBuffer {
public:
    explicit FrameStatsRingBuffer(std::size_t capacity = 300)
        : m_capacity(capacity)
        , m_write_index(0)
        , m_count(0)
    {
        m_buffer.resize(capacity);
    }

    void push(const FrameStats& stats) {
        m_buffer[m_write_index] = stats;
        m_write_index = (m_write_index + 1) % m_capacity;
        if (m_count < m_capacity) {
            ++m_count;
        }
    }

    [[nodiscard]] std::size_t count() const { return m_count; }
    [[nodiscard]] std::size_t capacity() const { return m_capacity; }
    [[nodiscard]] bool empty() const { return m_count == 0; }
    [[nodiscard]] bool full() const { return m_count == m_capacity; }

    [[nodiscard]] const FrameStats& at(std::size_t index) const {
        if (index >= m_count) {
            static FrameStats empty{};
            return empty;
        }

        // Calculate actual index (oldest first)
        std::size_t start = (m_write_index + m_capacity - m_count) % m_capacity;
        std::size_t actual = (start + index) % m_capacity;
        return m_buffer[actual];
    }

    [[nodiscard]] const FrameStats& latest() const {
        if (m_count == 0) {
            static FrameStats empty{};
            return empty;
        }
        std::size_t idx = (m_write_index + m_capacity - 1) % m_capacity;
        return m_buffer[idx];
    }

    [[nodiscard]] std::vector<std::int64_t> get_frame_times() const {
        std::vector<std::int64_t> times;
        times.reserve(m_count);
        for (std::size_t i = 0; i < m_count; ++i) {
            times.push_back(at(i).total_frame_time_us);
        }
        return times;
    }

    void clear() {
        m_write_index = 0;
        m_count = 0;
    }

private:
    std::vector<FrameStats> m_buffer;
    std::size_t m_capacity;
    std::size_t m_write_index;
    std::size_t m_count;
};

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_frame(const Frame& frame) {
    std::string result = "Frame {\n";

    result += "  number: " + std::to_string(frame.number()) + "\n";
    result += "  state: " + std::string(frame_state_name(frame.state())) + "\n";

    if (auto dur = frame.render_duration()) {
        auto cpu_us = std::chrono::duration_cast<std::chrono::microseconds>(*dur).count();
        result += "  cpu_time: " + std::to_string(cpu_us) + " us\n";
    }

    result += "  width: " + std::to_string(frame.width()) + "\n";
    result += "  height: " + std::to_string(frame.height()) + "\n";

    result += "}";
    return result;
}

} // namespace debug

} // namespace void_presenter
