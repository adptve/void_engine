/// @file supervisor.hpp
/// @brief Erlang-style supervision for fault tolerance
///
/// Provides hierarchical supervision with:
/// - Multiple restart strategies (one_for_one, one_for_all, rest_for_one)
/// - Configurable restart limits
/// - Dependency-aware child ordering
/// - Automatic restart with exponential backoff
/// - Health monitoring and failure detection

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_kernel {

// =============================================================================
// Child Handle
// =============================================================================

/// Handle for managing a supervised child
class ChildHandle {
public:
    ChildHandle() = default;

    /// Get child name
    [[nodiscard]] const std::string& name() const { return m_name; }

    /// Get current state
    [[nodiscard]] ChildState state() const { return m_state.load(); }

    /// Get restart count
    [[nodiscard]] std::uint32_t restart_count() const { return m_restart_count; }

    /// Get last start time
    [[nodiscard]] std::chrono::steady_clock::time_point last_start_time() const {
        return m_last_start_time;
    }

    /// Get uptime (time since last start)
    [[nodiscard]] std::chrono::nanoseconds uptime() const;

    /// Check if running
    [[nodiscard]] bool is_running() const { return m_state.load() == ChildState::Running; }

    /// Get specification
    [[nodiscard]] const ChildSpec& spec() const { return m_spec; }

private:
    friend class Supervisor;

    std::string m_name;
    ChildSpec m_spec;
    std::atomic<ChildState> m_state{ChildState::Stopped};
    std::uint32_t m_restart_count = 0;
    std::chrono::steady_clock::time_point m_last_start_time;
    std::chrono::steady_clock::time_point m_last_failure_time;
    std::optional<std::string> m_last_error;
    std::thread m_thread;
    std::atomic<bool> m_should_stop{false};
};

// =============================================================================
// Supervisor
// =============================================================================

/// Supervisor for managing child processes with fault tolerance
class Supervisor {
public:
    /// Callback for child events
    using ChildEventCallback = std::function<void(const ChildEvent&)>;

    /// Create supervisor with configuration
    explicit Supervisor(SupervisorConfig config = {});
    ~Supervisor();

    // Non-copyable
    Supervisor(const Supervisor&) = delete;
    Supervisor& operator=(const Supervisor&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get supervisor name
    [[nodiscard]] const std::string& name() const { return m_config.name; }

    /// Get configuration
    [[nodiscard]] const SupervisorConfig& config() const { return m_config; }

    /// Update configuration (takes effect on next restart)
    void set_config(const SupervisorConfig& config);

    // =========================================================================
    // Child Management
    // =========================================================================

    /// Add a child specification
    void_core::Result<void> add_child(ChildSpec spec);

    /// Add a simple child with function
    void_core::Result<void> add_child(
        const std::string& name,
        std::function<void()> start_fn,
        std::function<void()> stop_fn = nullptr,
        RestartStrategy restart = RestartStrategy::Transient);

    /// Remove a child by name
    void_core::Result<void> remove_child(const std::string& name);

    /// Get child handle by name
    [[nodiscard]] ChildHandle* get_child(const std::string& name);
    [[nodiscard]] const ChildHandle* get_child(const std::string& name) const;

    /// Get all child names
    [[nodiscard]] std::vector<std::string> child_names() const;

    /// Get child count
    [[nodiscard]] std::size_t child_count() const;

    /// Get running child count
    [[nodiscard]] std::size_t running_child_count() const;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Start the supervisor and all children
    void_core::Result<void> start();

    /// Stop the supervisor and all children
    void stop();

    /// Restart the supervisor
    void_core::Result<void> restart();

    /// Get supervisor state
    [[nodiscard]] SupervisorState state() const { return m_state.load(); }

    /// Check if running
    [[nodiscard]] bool is_running() const { return m_state.load() == SupervisorState::Running; }

    // =========================================================================
    // Child Control
    // =========================================================================

    /// Start a specific child
    void_core::Result<void> start_child(const std::string& name);

    /// Stop a specific child
    void_core::Result<void> stop_child(const std::string& name);

    /// Restart a specific child
    void_core::Result<void> restart_child(const std::string& name);

    /// Terminate a child (no restart)
    void_core::Result<void> terminate_child(const std::string& name);

    // =========================================================================
    // Monitoring
    // =========================================================================

    /// Check for crashed children and handle restarts
    void check_children();

    /// Report a child failure (called by child threads)
    void report_failure(const std::string& name, const std::string& error);

    /// Get restart count for a child
    [[nodiscard]] std::uint32_t get_restart_count(const std::string& name) const;

    /// Get total restart count for all children
    [[nodiscard]] std::uint32_t total_restart_count() const;

    /// Check if restart limits exceeded
    [[nodiscard]] bool restart_limits_exceeded() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Set callback for child events
    void set_event_callback(ChildEventCallback callback);

    /// Set callback for all children failed (restart limits exceeded)
    void set_on_max_restarts(std::function<void()> callback);

private:
    // Internal methods
    void start_child_internal(ChildHandle& child);
    void stop_child_internal(ChildHandle& child);
    void handle_child_failure(const std::string& name);
    void apply_restart_strategy(const std::string& failed_child);
    std::chrono::milliseconds calculate_restart_delay(std::uint32_t restart_count) const;
    std::vector<std::string> get_start_order() const;
    std::vector<std::string> get_stop_order() const;
    void emit_event(const ChildEvent& event);

private:
    SupervisorConfig m_config;
    std::atomic<SupervisorState> m_state{SupervisorState::Stopped};

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<ChildHandle>> m_children;
    std::vector<std::string> m_child_order;  // Maintains insertion order

    // Restart tracking
    std::vector<std::chrono::steady_clock::time_point> m_restart_times;
    std::uint32_t m_total_restarts = 0;

    // Callbacks
    ChildEventCallback m_event_callback;
    std::function<void()> m_on_max_restarts;

    // Monitoring thread
    std::thread m_monitor_thread;
    std::atomic<bool> m_monitor_running{false};
    std::condition_variable m_monitor_cv;
};

// =============================================================================
// Supervisor Tree
// =============================================================================

/// Hierarchical supervisor tree for complex applications
class SupervisorTree {
public:
    SupervisorTree();
    ~SupervisorTree();

    // Non-copyable
    SupervisorTree(const SupervisorTree&) = delete;
    SupervisorTree& operator=(const SupervisorTree&) = delete;

    // =========================================================================
    // Tree Structure
    // =========================================================================

    /// Create and add a root supervisor
    void_core::Result<Supervisor*> create_root(SupervisorConfig config = {});

    /// Create and add a child supervisor under a parent
    void_core::Result<Supervisor*> create_supervisor(
        const std::string& parent_name,
        SupervisorConfig config);

    /// Get supervisor by name
    [[nodiscard]] Supervisor* get_supervisor(const std::string& name);
    [[nodiscard]] const Supervisor* get_supervisor(const std::string& name) const;

    /// Get root supervisor
    [[nodiscard]] Supervisor* root();
    [[nodiscard]] const Supervisor* root() const;

    /// Remove a supervisor and its children
    void_core::Result<void> remove_supervisor(const std::string& name);

    /// Get all supervisor names
    [[nodiscard]] std::vector<std::string> supervisor_names() const;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Start the entire tree
    void_core::Result<void> start();

    /// Stop the entire tree
    void stop();

    /// Check all supervisors for failed children
    void check_all();

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get total child count across all supervisors
    [[nodiscard]] std::size_t total_child_count() const;

    /// Get total running child count
    [[nodiscard]] std::size_t total_running_count() const;

    /// Get total restart count
    [[nodiscard]] std::uint32_t total_restart_count() const;

private:
    struct SupervisorNode {
        std::unique_ptr<Supervisor> supervisor;
        std::string parent_name;
        std::vector<std::string> children;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, SupervisorNode> m_supervisors;
    std::string m_root_name;
};

// =============================================================================
// Supervised Task Helpers
// =============================================================================

/// Create a child spec from a simple function
inline ChildSpec make_child(
    const std::string& name,
    std::function<void()> fn,
    RestartStrategy restart = RestartStrategy::Transient) {
    return ChildSpec{
        .name = name,
        .start_fn = std::move(fn),
        .stop_fn = nullptr,
        .restart = restart,
    };
}

/// Create a child spec with start and stop functions
inline ChildSpec make_child(
    const std::string& name,
    std::function<void()> start_fn,
    std::function<void()> stop_fn,
    RestartStrategy restart = RestartStrategy::Transient) {
    return ChildSpec{
        .name = name,
        .start_fn = std::move(start_fn),
        .stop_fn = std::move(stop_fn),
        .restart = restart,
    };
}

/// Create a permanent worker (always restart)
inline ChildSpec make_worker(
    const std::string& name,
    std::function<void()> fn) {
    return make_child(name, std::move(fn), RestartStrategy::OneForOne);
}

/// Create a transient task (restart only on abnormal exit)
inline ChildSpec make_task(
    const std::string& name,
    std::function<void()> fn) {
    return make_child(name, std::move(fn), RestartStrategy::Transient);
}

/// Create a temporary task (never restart)
inline ChildSpec make_temporary(
    const std::string& name,
    std::function<void()> fn) {
    return make_child(name, std::move(fn), RestartStrategy::Temporary);
}

} // namespace void_kernel
