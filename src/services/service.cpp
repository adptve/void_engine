/// @file service.cpp
/// @brief Service lifecycle management implementation
///
/// This file provides:
/// - Service state machine utilities
/// - Health monitoring helpers
/// - Service dependency resolution

#include <void_engine/services/service.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_services {

// =============================================================================
// Service State Utilities
// =============================================================================

namespace {

/// Global service statistics
struct ServiceStats {
    std::atomic<std::uint64_t> total_services_created{0};
    std::atomic<std::uint64_t> total_services_started{0};
    std::atomic<std::uint64_t> total_services_stopped{0};
    std::atomic<std::uint64_t> total_services_failed{0};
    std::atomic<std::uint64_t> total_restarts{0};
};

ServiceStats s_stats;

} // anonymous namespace

// =============================================================================
// State Transition Validation
// =============================================================================

/// Check if a state transition is valid
/// @param from Current state
/// @param to Target state
/// @return true if transition is allowed
[[nodiscard]] bool is_valid_transition(ServiceState from, ServiceState to) {
    // Define valid state transitions
    switch (from) {
        case ServiceState::Stopped:
            return to == ServiceState::Starting;

        case ServiceState::Starting:
            return to == ServiceState::Running ||
                   to == ServiceState::Failed ||
                   to == ServiceState::Stopped;

        case ServiceState::Running:
            return to == ServiceState::Stopping ||
                   to == ServiceState::Degraded ||
                   to == ServiceState::Failed;

        case ServiceState::Stopping:
            return to == ServiceState::Stopped ||
                   to == ServiceState::Failed;

        case ServiceState::Failed:
            return to == ServiceState::Stopped ||
                   to == ServiceState::Starting;

        case ServiceState::Degraded:
            return to == ServiceState::Running ||
                   to == ServiceState::Stopping ||
                   to == ServiceState::Failed;
    }
    return false;
}

/// Check if a state is operational (service is doing useful work)
[[nodiscard]] bool is_operational(ServiceState state) {
    return state == ServiceState::Running || state == ServiceState::Degraded;
}

/// Check if a state is transitional (service is changing states)
[[nodiscard]] bool is_transitional(ServiceState state) {
    return state == ServiceState::Starting || state == ServiceState::Stopping;
}

/// Check if a state indicates the service is usable
[[nodiscard]] bool is_usable(ServiceState state) {
    return state == ServiceState::Running;
}

// =============================================================================
// Service Statistics API
// =============================================================================

/// Record a service creation
void record_service_created() {
    ++s_stats.total_services_created;
}

/// Record a service start
void record_service_started() {
    ++s_stats.total_services_started;
}

/// Record a service stop
void record_service_stopped() {
    ++s_stats.total_services_stopped;
}

/// Record a service failure
void record_service_failed() {
    ++s_stats.total_services_failed;
}

/// Record a service restart
void record_service_restart() {
    ++s_stats.total_restarts;
}

/// Get total services ever created
[[nodiscard]] std::uint64_t get_total_services_created() {
    return s_stats.total_services_created.load();
}

/// Get total services started
[[nodiscard]] std::uint64_t get_total_services_started() {
    return s_stats.total_services_started.load();
}

/// Get total services stopped
[[nodiscard]] std::uint64_t get_total_services_stopped() {
    return s_stats.total_services_stopped.load();
}

/// Get total services failed
[[nodiscard]] std::uint64_t get_total_services_failed() {
    return s_stats.total_services_failed.load();
}

/// Get total restarts
[[nodiscard]] std::uint64_t get_total_restarts() {
    return s_stats.total_restarts.load();
}

/// Reset all statistics
void reset_service_stats() {
    s_stats.total_services_created.store(0);
    s_stats.total_services_started.store(0);
    s_stats.total_services_stopped.store(0);
    s_stats.total_services_failed.store(0);
    s_stats.total_restarts.store(0);
}

// =============================================================================
// Dependency Resolution
// =============================================================================

namespace {

/// Dependency graph node
struct DepNode {
    std::string name;
    std::vector<std::string> dependencies;
    bool visited = false;
    bool in_stack = false;
};

/// Topological sort helper using DFS
bool topological_sort_visit(
    const std::string& name,
    std::unordered_map<std::string, DepNode>& nodes,
    std::vector<std::string>& result,
    std::string& cycle_node) {

    auto it = nodes.find(name);
    if (it == nodes.end()) {
        // Node not found - dependency is missing (will be checked elsewhere)
        return true;
    }

    DepNode& node = it->second;

    if (node.in_stack) {
        // Cycle detected
        cycle_node = name;
        return false;
    }

    if (node.visited) {
        return true;
    }

    node.visited = true;
    node.in_stack = true;

    for (const auto& dep : node.dependencies) {
        if (!topological_sort_visit(dep, nodes, result, cycle_node)) {
            return false;
        }
    }

    node.in_stack = false;
    result.push_back(name);
    return true;
}

} // anonymous namespace

/// Resolve service startup order based on dependencies
/// @param services List of service names
/// @param get_deps Function to get dependencies for a service name
/// @param[out] cycle_service If cycle detected, contains the cycle service name
/// @return Ordered list of services (dependencies first), empty if cycle detected
[[nodiscard]] std::vector<std::string> resolve_startup_order(
    const std::vector<std::string>& services,
    std::function<std::vector<std::string>(const std::string&)> get_deps,
    std::string& cycle_service) {

    // Build dependency graph
    std::unordered_map<std::string, DepNode> nodes;
    for (const auto& name : services) {
        DepNode node;
        node.name = name;
        node.dependencies = get_deps(name);
        nodes[name] = std::move(node);
    }

    // Topological sort
    std::vector<std::string> result;
    result.reserve(services.size());

    for (const auto& name : services) {
        if (!topological_sort_visit(name, nodes, result, cycle_service)) {
            return {}; // Cycle detected
        }
    }

    return result;
}

/// Get shutdown order (reverse of startup order)
[[nodiscard]] std::vector<std::string> resolve_shutdown_order(
    const std::vector<std::string>& startup_order) {

    std::vector<std::string> shutdown_order = startup_order;
    std::reverse(shutdown_order.begin(), shutdown_order.end());
    return shutdown_order;
}

// =============================================================================
// Health Calculation Utilities
// =============================================================================

/// Calculate aggregate health score from multiple services
/// @param scores Vector of individual health scores
/// @return Aggregate health score (0.0 - 1.0)
[[nodiscard]] float calculate_aggregate_health(const std::vector<float>& scores) {
    if (scores.empty()) {
        return 1.0f; // No services = healthy
    }

    // Use weighted average with emphasis on unhealthy services
    float sum = 0.0f;
    float min_score = 1.0f;

    for (float score : scores) {
        sum += score;
        min_score = std::min(min_score, score);
    }

    float avg = sum / static_cast<float>(scores.size());

    // Weight towards minimum (degraded services matter more)
    return 0.7f * avg + 0.3f * min_score;
}

/// Determine health status category from score
/// @param score Health score (0.0 - 1.0)
/// @return Human-readable status string
[[nodiscard]] const char* health_status_string(float score) {
    if (score >= 0.9f) {
        return "Healthy";
    } else if (score >= 0.75f) {
        return "Good";
    } else if (score >= 0.5f) {
        return "Degraded";
    } else if (score >= 0.25f) {
        return "Critical";
    } else {
        return "Failed";
    }
}

/// Check if health score indicates the service should be restarted
[[nodiscard]] bool should_restart_for_health(float score, const ServiceConfig& config) {
    if (!config.auto_restart) {
        return false;
    }
    return score < 0.25f;
}

// =============================================================================
// Configuration Validation
// =============================================================================

/// Validate a service configuration
/// @param config Configuration to validate
/// @return Error message if invalid, empty string if valid
[[nodiscard]] std::string validate_config(const ServiceConfig& config) {
    if (config.max_restart_attempts > 100) {
        return "max_restart_attempts exceeds reasonable limit (100)";
    }

    if (config.restart_delay.count() < 0) {
        return "restart_delay cannot be negative";
    }

    if (config.health_check_interval.count() < 100) {
        return "health_check_interval too small (minimum 100ms)";
    }

    if (config.startup_timeout.count() < 1000) {
        return "startup_timeout too small (minimum 1000ms)";
    }

    if (config.shutdown_timeout.count() < 500) {
        return "shutdown_timeout too small (minimum 500ms)";
    }

    return ""; // Valid
}

} // namespace void_services
