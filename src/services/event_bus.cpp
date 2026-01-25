/// @file event_bus.cpp
/// @brief Event bus implementation for inter-service communication
///
/// This file provides:
/// - Event bus statistics tracking
/// - Event priority comparison utilities
/// - Event debugging and logging helpers

#include <void_engine/services/event_bus.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_services {

// =============================================================================
// Event Priority Utilities
// =============================================================================

/// Convert EventPriority to string representation
[[nodiscard]] const char* to_string(EventPriority priority) {
    switch (priority) {
        case EventPriority::Low: return "Low";
        case EventPriority::Normal: return "Normal";
        case EventPriority::High: return "High";
        case EventPriority::Critical: return "Critical";
    }
    return "Unknown";
}

/// Convert string to EventPriority
/// @param str String representation
/// @return Parsed priority, defaults to Normal if unknown
[[nodiscard]] EventPriority priority_from_string(const std::string& str) {
    if (str == "Low" || str == "low") return EventPriority::Low;
    if (str == "Normal" || str == "normal") return EventPriority::Normal;
    if (str == "High" || str == "high") return EventPriority::High;
    if (str == "Critical" || str == "critical") return EventPriority::Critical;
    return EventPriority::Normal;
}

/// Compare two priorities
/// @return negative if a < b, 0 if equal, positive if a > b
[[nodiscard]] int compare_priority(EventPriority a, EventPriority b) {
    return static_cast<int>(a) - static_cast<int>(b);
}

/// Check if priority a is higher than priority b
[[nodiscard]] bool is_higher_priority(EventPriority a, EventPriority b) {
    return static_cast<int>(a) > static_cast<int>(b);
}

// =============================================================================
// Global Event Statistics
// =============================================================================

namespace {

/// Global event bus statistics for debugging
struct GlobalEventStats {
    std::atomic<std::uint64_t> total_events_published{0};
    std::atomic<std::uint64_t> total_events_processed{0};
    std::atomic<std::uint64_t> total_events_dropped{0};
    std::atomic<std::uint64_t> total_subscriptions_created{0};
    std::atomic<std::uint64_t> total_subscriptions_removed{0};
    std::atomic<std::uint64_t> peak_queue_size{0};
    std::atomic<std::uint64_t> total_buses_created{0};

    std::mutex category_mutex;
    std::unordered_map<std::string, std::uint64_t> events_by_category;
};

GlobalEventStats s_global_stats;

} // anonymous namespace

// =============================================================================
// Global Statistics API
// =============================================================================

/// Record an event publication
void record_event_published(const std::string& category) {
    ++s_global_stats.total_events_published;

    if (!category.empty()) {
        std::lock_guard lock(s_global_stats.category_mutex);
        ++s_global_stats.events_by_category[category];
    }
}

/// Record an event being processed
void record_event_processed() {
    ++s_global_stats.total_events_processed;
}

/// Record an event being dropped
void record_event_dropped() {
    ++s_global_stats.total_events_dropped;
}

/// Record a subscription creation
void record_subscription_created() {
    ++s_global_stats.total_subscriptions_created;
}

/// Record a subscription removal
void record_subscription_removed() {
    ++s_global_stats.total_subscriptions_removed;
}

/// Update peak queue size if current is higher
void record_queue_size(std::size_t size) {
    std::uint64_t current_peak = s_global_stats.peak_queue_size.load();
    while (size > current_peak) {
        if (s_global_stats.peak_queue_size.compare_exchange_weak(current_peak, size)) {
            break;
        }
    }
}

/// Record an event bus creation
void record_bus_created() {
    ++s_global_stats.total_buses_created;
}

/// Get total events published across all buses
[[nodiscard]] std::uint64_t get_global_events_published() {
    return s_global_stats.total_events_published.load();
}

/// Get total events processed across all buses
[[nodiscard]] std::uint64_t get_global_events_processed() {
    return s_global_stats.total_events_processed.load();
}

/// Get total events dropped across all buses
[[nodiscard]] std::uint64_t get_global_events_dropped() {
    return s_global_stats.total_events_dropped.load();
}

/// Get total subscriptions ever created
[[nodiscard]] std::uint64_t get_global_subscriptions_created() {
    return s_global_stats.total_subscriptions_created.load();
}

/// Get total subscriptions ever removed
[[nodiscard]] std::uint64_t get_global_subscriptions_removed() {
    return s_global_stats.total_subscriptions_removed.load();
}

/// Get peak queue size ever observed
[[nodiscard]] std::uint64_t get_global_peak_queue_size() {
    return s_global_stats.peak_queue_size.load();
}

/// Get total event buses ever created
[[nodiscard]] std::uint64_t get_global_buses_created() {
    return s_global_stats.total_buses_created.load();
}

/// Get event counts by category
[[nodiscard]] std::unordered_map<std::string, std::uint64_t> get_events_by_category() {
    std::lock_guard lock(s_global_stats.category_mutex);
    return s_global_stats.events_by_category;
}

/// Reset all global statistics
void reset_global_event_stats() {
    s_global_stats.total_events_published.store(0);
    s_global_stats.total_events_processed.store(0);
    s_global_stats.total_events_dropped.store(0);
    s_global_stats.total_subscriptions_created.store(0);
    s_global_stats.total_subscriptions_removed.store(0);
    s_global_stats.peak_queue_size.store(0);
    s_global_stats.total_buses_created.store(0);

    std::lock_guard lock(s_global_stats.category_mutex);
    s_global_stats.events_by_category.clear();
}

// =============================================================================
// Event Bus Statistics Formatting
// =============================================================================

/// Format EventBusStats as a human-readable string
[[nodiscard]] std::string format_stats(const EventBusStats& stats) {
    std::ostringstream oss;
    oss << "EventBus Statistics:\n";
    oss << "  Published:     " << stats.events_published << "\n";
    oss << "  Queued:        " << stats.events_queued << "\n";
    oss << "  Processed:     " << stats.events_processed << "\n";
    oss << "  Dropped:       " << stats.events_dropped << "\n";
    oss << "  Subscriptions: " << stats.active_subscriptions << "\n";
    oss << "  Queue Size:    " << stats.queue_size << "\n";
    oss << "  Max Queue:     " << stats.max_queue_size;
    return oss.str();
}

/// Format global statistics as a human-readable string
[[nodiscard]] std::string format_global_stats() {
    std::ostringstream oss;
    oss << "Global Event Statistics:\n";
    oss << "  Total Published:  " << get_global_events_published() << "\n";
    oss << "  Total Processed:  " << get_global_events_processed() << "\n";
    oss << "  Total Dropped:    " << get_global_events_dropped() << "\n";
    oss << "  Subscriptions:    " << get_global_subscriptions_created()
        << " created, " << get_global_subscriptions_removed() << " removed\n";
    oss << "  Peak Queue Size:  " << get_global_peak_queue_size() << "\n";
    oss << "  Buses Created:    " << get_global_buses_created();
    return oss.str();
}

// =============================================================================
// Category Matching Utilities
// =============================================================================

/// Check if a category pattern matches an event category
/// @param pattern Pattern to match (supports trailing * wildcard)
/// @param category Event category to check
/// @return true if pattern matches category
[[nodiscard]] bool category_matches(const std::string& pattern, const std::string& category) {
    // Empty pattern matches everything
    if (pattern.empty()) {
        return true;
    }

    // Exact match
    if (pattern == category) {
        return true;
    }

    // Wildcard matching
    if (!pattern.empty() && pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        return category.compare(0, prefix.size(), prefix) == 0;
    }

    return false;
}

/// Split a category into hierarchical components
/// @param category Category string (e.g., "audio.sfx.explosion")
/// @return Vector of components (e.g., ["audio", "sfx", "explosion"])
[[nodiscard]] std::vector<std::string> split_category(const std::string& category) {
    std::vector<std::string> components;
    std::string current;

    for (char c : category) {
        if (c == '.') {
            if (!current.empty()) {
                components.push_back(std::move(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        components.push_back(std::move(current));
    }

    return components;
}

/// Build a category from hierarchical components
/// @param components Vector of components
/// @return Category string joined with dots
[[nodiscard]] std::string build_category(const std::vector<std::string>& components) {
    std::string result;
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (i > 0) {
            result += '.';
        }
        result += components[i];
    }
    return result;
}

/// Get parent category
/// @param category Category string
/// @return Parent category (empty if already root)
[[nodiscard]] std::string parent_category(const std::string& category) {
    auto pos = category.rfind('.');
    if (pos == std::string::npos) {
        return "";
    }
    return category.substr(0, pos);
}

// =============================================================================
// Event Debugging
// =============================================================================

namespace debug {

/// Event trace entry for debugging
struct EventTraceEntry {
    std::chrono::steady_clock::time_point timestamp;
    std::string event_type;
    std::string category;
    EventPriority priority;
    std::size_t handler_count;
    std::chrono::microseconds processing_time;
};

namespace {

std::mutex s_trace_mutex;
std::vector<EventTraceEntry> s_event_trace;
std::size_t s_max_trace_entries = 1000;
bool s_tracing_enabled = false;

} // anonymous namespace

/// Enable event tracing
void enable_tracing(std::size_t max_entries) {
    std::lock_guard lock(s_trace_mutex);
    s_tracing_enabled = true;
    s_max_trace_entries = max_entries;
}

/// Disable event tracing
void disable_tracing() {
    std::lock_guard lock(s_trace_mutex);
    s_tracing_enabled = false;
}

/// Check if tracing is enabled
[[nodiscard]] bool is_tracing_enabled() {
    std::lock_guard lock(s_trace_mutex);
    return s_tracing_enabled;
}

/// Record an event trace entry
void trace_event(
    const std::string& event_type,
    const std::string& category,
    EventPriority priority,
    std::size_t handler_count,
    std::chrono::microseconds processing_time) {

    std::lock_guard lock(s_trace_mutex);
    if (!s_tracing_enabled) {
        return;
    }

    // Remove oldest entries if at capacity
    while (s_event_trace.size() >= s_max_trace_entries) {
        s_event_trace.erase(s_event_trace.begin());
    }

    EventTraceEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.event_type = event_type;
    entry.category = category;
    entry.priority = priority;
    entry.handler_count = handler_count;
    entry.processing_time = processing_time;

    s_event_trace.push_back(std::move(entry));
}

/// Get all trace entries
[[nodiscard]] std::vector<EventTraceEntry> get_trace() {
    std::lock_guard lock(s_trace_mutex);
    return s_event_trace;
}

/// Clear trace entries
void clear_trace() {
    std::lock_guard lock(s_trace_mutex);
    s_event_trace.clear();
}

/// Format trace as string
[[nodiscard]] std::string format_trace() {
    std::lock_guard lock(s_trace_mutex);

    std::ostringstream oss;
    oss << "Event Trace (" << s_event_trace.size() << " entries):\n";

    for (const auto& entry : s_event_trace) {
        oss << "  [" << to_string(entry.priority) << "] "
            << entry.event_type;
        if (!entry.category.empty()) {
            oss << " (" << entry.category << ")";
        }
        oss << " - " << entry.handler_count << " handlers, "
            << entry.processing_time.count() << "us\n";
    }

    return oss.str();
}

} // namespace debug

} // namespace void_services
