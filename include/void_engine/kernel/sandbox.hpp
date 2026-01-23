/// @file sandbox.hpp
/// @brief Sandbox for isolated code execution
///
/// Provides resource isolation and permission control with:
/// - Fine-grained permission system
/// - Resource usage tracking and limits
/// - Path and network access control
/// - Violation detection and handling

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace void_kernel {

// =============================================================================
// Permission Set
// =============================================================================

/// Manages a set of permissions with path/host allowlists
class PermissionSet {
public:
    PermissionSet() = default;
    explicit PermissionSet(Permission base_permissions);

    // =========================================================================
    // Permission Management
    // =========================================================================

    /// Grant a permission
    void grant(Permission perm);

    /// Revoke a permission
    void revoke(Permission perm);

    /// Check if a permission is granted
    [[nodiscard]] bool has(Permission perm) const;

    /// Check multiple permissions (all must be granted)
    [[nodiscard]] bool has_all(Permission perms) const;

    /// Check multiple permissions (at least one must be granted)
    [[nodiscard]] bool has_any(Permission perms) const;

    /// Get raw permission flags
    [[nodiscard]] Permission raw() const { return m_permissions; }

    // =========================================================================
    // Path Access
    // =========================================================================

    /// Allow access to a specific path
    void allow_path(const std::filesystem::path& path);

    /// Allow access to paths matching a pattern (glob-style)
    void allow_path_pattern(const std::string& pattern);

    /// Check if path access is allowed
    [[nodiscard]] bool is_path_allowed(const std::filesystem::path& path) const;

    /// Get all allowed paths
    [[nodiscard]] const std::vector<std::filesystem::path>& allowed_paths() const;

    // =========================================================================
    // Network Access
    // =========================================================================

    /// Allow access to a specific host
    void allow_host(const std::string& host);

    /// Allow access to a host pattern (e.g., "*.example.com")
    void allow_host_pattern(const std::string& pattern);

    /// Check if host access is allowed
    [[nodiscard]] bool is_host_allowed(const std::string& host) const;

    /// Get all allowed hosts
    [[nodiscard]] const std::vector<std::string>& allowed_hosts() const;

    // =========================================================================
    // Presets
    // =========================================================================

    /// Create minimal permission set (almost nothing allowed)
    static PermissionSet minimal();

    /// Create read-only permission set
    static PermissionSet read_only();

    /// Create full permission set (everything allowed)
    static PermissionSet full();

    /// Create permission set for game scripts
    static PermissionSet game_script();

    /// Create permission set for editor plugins
    static PermissionSet editor_plugin();

private:
    Permission m_permissions = Permission::None;
    std::vector<std::filesystem::path> m_allowed_paths;
    std::vector<std::string> m_path_patterns;
    std::vector<std::string> m_allowed_hosts;
    std::vector<std::string> m_host_patterns;
};

// =============================================================================
// Resource Usage Tracker
// =============================================================================

/// Tracks resource usage within a sandbox
class ResourceUsageTracker {
public:
    ResourceUsageTracker() = default;
    explicit ResourceUsageTracker(const ResourceLimits& limits);

    // =========================================================================
    // Memory Tracking
    // =========================================================================

    /// Record memory allocation
    [[nodiscard]] bool allocate(std::size_t bytes);

    /// Record memory deallocation
    void deallocate(std::size_t bytes);

    /// Get current memory usage
    [[nodiscard]] std::size_t memory_used() const { return m_memory_used.load(); }

    /// Get peak memory usage
    [[nodiscard]] std::size_t memory_peak() const { return m_memory_peak.load(); }

    /// Get allocation count
    [[nodiscard]] std::uint32_t allocation_count() const { return m_allocation_count.load(); }

    // =========================================================================
    // CPU Time Tracking
    // =========================================================================

    /// Record CPU time used (microseconds)
    [[nodiscard]] bool use_cpu_time(std::uint64_t microseconds);

    /// Get CPU time used
    [[nodiscard]] std::uint64_t cpu_time_used() const { return m_cpu_time_used.load(); }

    // =========================================================================
    // Instruction Counting
    // =========================================================================

    /// Record instructions executed
    [[nodiscard]] bool execute_instructions(std::uint64_t count);

    /// Get instruction count
    [[nodiscard]] std::uint64_t instructions_executed() const { return m_instructions.load(); }

    // =========================================================================
    // Handle Tracking
    // =========================================================================

    /// Record file handle opened
    [[nodiscard]] bool open_handle();

    /// Record file handle closed
    void close_handle();

    /// Get open handle count
    [[nodiscard]] std::uint32_t open_handles() const { return m_open_handles.load(); }

    // =========================================================================
    // Thread Tracking
    // =========================================================================

    /// Record thread created
    [[nodiscard]] bool create_thread();

    /// Record thread terminated
    void terminate_thread();

    /// Get active thread count
    [[nodiscard]] std::uint32_t active_threads() const { return m_active_threads.load(); }

    // =========================================================================
    // Limits
    // =========================================================================

    /// Get limits
    [[nodiscard]] const ResourceLimits& limits() const { return m_limits; }

    /// Set limits
    void set_limits(const ResourceLimits& limits) { m_limits = limits; }

    /// Check if any limit is exceeded
    [[nodiscard]] bool any_limit_exceeded() const;

    /// Get which limits are exceeded
    [[nodiscard]] std::vector<std::string> exceeded_limits() const;

    /// Reset all counters
    void reset();

private:
    ResourceLimits m_limits;

    std::atomic<std::size_t> m_memory_used{0};
    std::atomic<std::size_t> m_memory_peak{0};
    std::atomic<std::uint32_t> m_allocation_count{0};
    std::atomic<std::uint64_t> m_cpu_time_used{0};
    std::atomic<std::uint64_t> m_instructions{0};
    std::atomic<std::uint32_t> m_open_handles{0};
    std::atomic<std::uint32_t> m_active_threads{0};
};

// =============================================================================
// Sandbox
// =============================================================================

/// Sandbox for isolated code execution
class Sandbox {
public:
    /// Callback for violations
    using ViolationCallback = std::function<void(const SandboxViolationEvent&)>;

    /// Create sandbox with configuration
    explicit Sandbox(SandboxConfig config);
    ~Sandbox();

    // Non-copyable
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get sandbox name
    [[nodiscard]] const std::string& name() const { return m_config.name; }

    /// Get configuration
    [[nodiscard]] const SandboxConfig& config() const { return m_config; }

    /// Get permission set
    [[nodiscard]] const PermissionSet& permissions() const { return m_permissions; }

    /// Get mutable permission set
    [[nodiscard]] PermissionSet& permissions() { return m_permissions; }

    /// Get resource tracker
    [[nodiscard]] const ResourceUsageTracker& resources() const { return m_resources; }

    /// Get mutable resource tracker
    [[nodiscard]] ResourceUsageTracker& resources() { return m_resources; }

    // =========================================================================
    // State
    // =========================================================================

    /// Get current state
    [[nodiscard]] SandboxState state() const { return m_state.load(); }

    /// Check if sandbox is active
    [[nodiscard]] bool is_active() const {
        auto s = m_state.load();
        return s == SandboxState::Running || s == SandboxState::Suspended;
    }

    /// Check if sandbox has been violated
    [[nodiscard]] bool is_violated() const { return m_state.load() == SandboxState::Violated; }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Enter sandbox (makes it active for current thread)
    void_core::Result<void> enter();

    /// Exit sandbox
    void exit();

    /// Suspend sandbox (pause execution)
    void suspend();

    /// Resume sandbox
    void resume();

    /// Terminate sandbox
    void terminate();

    // =========================================================================
    // Permission Checking
    // =========================================================================

    /// Check if an operation is allowed
    [[nodiscard]] bool check_permission(Permission perm) const;

    /// Check if file access is allowed
    [[nodiscard]] bool check_file_access(
        const std::filesystem::path& path,
        Permission access_type) const;

    /// Check if network access is allowed
    [[nodiscard]] bool check_network_access(
        const std::string& host,
        Permission access_type) const;

    /// Request permission (may trigger UI or callback)
    [[nodiscard]] void_core::Result<void> request_permission(Permission perm);

    // =========================================================================
    // Resource Allocation
    // =========================================================================

    /// Allocate memory (checks limits)
    [[nodiscard]] void_core::Result<void> allocate_memory(std::size_t bytes);

    /// Deallocate memory
    void deallocate_memory(std::size_t bytes);

    /// Use CPU time (checks limits)
    [[nodiscard]] void_core::Result<void> use_cpu_time(std::uint64_t microseconds);

    /// Execute instructions (checks limits)
    [[nodiscard]] void_core::Result<void> execute_instructions(std::uint64_t count);

    /// Open handle (checks limits)
    [[nodiscard]] void_core::Result<void> open_handle();

    /// Close handle
    void close_handle();

    /// Create thread (checks limits)
    [[nodiscard]] void_core::Result<void> create_thread();

    /// Terminate thread
    void terminate_thread();

    // =========================================================================
    // Violation Handling
    // =========================================================================

    /// Report a violation
    void report_violation(Permission attempted, const std::string& details);

    /// Get violation count
    [[nodiscard]] std::uint32_t violation_count() const { return m_violation_count.load(); }

    /// Get last violation
    [[nodiscard]] std::optional<SandboxViolationEvent> last_violation() const;

    /// Set violation callback
    void set_violation_callback(ViolationCallback callback);

    /// Set maximum violations before termination (0 = unlimited)
    void set_max_violations(std::uint32_t max) { m_max_violations = max; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get creation time
    [[nodiscard]] std::chrono::steady_clock::time_point creation_time() const {
        return m_creation_time;
    }

    /// Get uptime
    [[nodiscard]] std::chrono::nanoseconds uptime() const;

    /// Get total execution time
    [[nodiscard]] std::chrono::nanoseconds execution_time() const;

private:
    void handle_violation(Permission attempted, const std::string& details);

private:
    SandboxConfig m_config;
    PermissionSet m_permissions;
    ResourceUsageTracker m_resources;

    std::atomic<SandboxState> m_state{SandboxState::Created};
    std::chrono::steady_clock::time_point m_creation_time;
    std::chrono::steady_clock::time_point m_enter_time;
    std::chrono::nanoseconds m_total_execution_time{0};

    std::atomic<std::uint32_t> m_violation_count{0};
    std::uint32_t m_max_violations = 0;  // 0 = unlimited
    mutable std::mutex m_violation_mutex;
    std::optional<SandboxViolationEvent> m_last_violation;
    ViolationCallback m_violation_callback;
};

// =============================================================================
// Sandbox Factory
// =============================================================================

/// Factory for creating pre-configured sandboxes
class SandboxFactory {
public:
    /// Create a sandbox for trusted code (full access)
    [[nodiscard]] static std::unique_ptr<Sandbox> create_trusted(
        const std::string& name = "trusted");

    /// Create a sandbox for untrusted code (minimal access)
    [[nodiscard]] static std::unique_ptr<Sandbox> create_untrusted(
        const std::string& name = "untrusted");

    /// Create a sandbox for game scripts
    [[nodiscard]] static std::unique_ptr<Sandbox> create_for_script(
        const std::string& name = "script");

    /// Create a sandbox for editor plugins
    [[nodiscard]] static std::unique_ptr<Sandbox> create_for_plugin(
        const std::string& name = "plugin");

    /// Create a sandbox with custom configuration
    [[nodiscard]] static std::unique_ptr<Sandbox> create_custom(
        const SandboxConfig& config);
};

// =============================================================================
// Thread-Local Sandbox Context
// =============================================================================

/// Get the current sandbox for this thread (nullptr if none)
[[nodiscard]] Sandbox* current_sandbox();

/// Set the current sandbox for this thread
void set_current_sandbox(Sandbox* sandbox);

/// RAII guard for sandbox context
class SandboxGuard {
public:
    explicit SandboxGuard(Sandbox& sandbox);
    ~SandboxGuard();

    // Non-copyable, non-movable
    SandboxGuard(const SandboxGuard&) = delete;
    SandboxGuard& operator=(const SandboxGuard&) = delete;
    SandboxGuard(SandboxGuard&&) = delete;
    SandboxGuard& operator=(SandboxGuard&&) = delete;

private:
    Sandbox* m_previous;
    Sandbox* m_current;
};

// =============================================================================
// Permission Check Macros
// =============================================================================

/// Check permission in current sandbox (returns error if denied)
#define VOID_SANDBOX_CHECK(permission) \
    do { \
        if (auto* sb = ::void_kernel::current_sandbox()) { \
            if (!sb->check_permission(permission)) { \
                sb->report_violation(permission, "Permission denied: " #permission); \
                return ::void_core::Error{"Permission denied: " #permission}; \
            } \
        } \
    } while (0)

/// Check permission in current sandbox (returns false if denied)
#define VOID_SANDBOX_CHECK_BOOL(permission) \
    ([&]() -> bool { \
        if (auto* sb = ::void_kernel::current_sandbox()) { \
            if (!sb->check_permission(permission)) { \
                sb->report_violation(permission, "Permission denied: " #permission); \
                return false; \
            } \
        } \
        return true; \
    })()

} // namespace void_kernel
