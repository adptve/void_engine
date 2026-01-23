#pragma once

/// @file service.hpp
/// @brief Service base interface and lifecycle management
///
/// Services are long-running components with managed lifecycles.
/// They can be started, stopped, restarted, and monitored for health.

#include "fwd.hpp"
#include <void_engine/core/id.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace void_services {

// =============================================================================
// Service ID
// =============================================================================

/// Unique identifier for a service
struct ServiceId {
    std::string name;
    std::uint64_t id = 0;

    ServiceId() = default;
    explicit ServiceId(std::string n) : name(std::move(n)), id(hash_name(name)) {}
    ServiceId(std::string n, std::uint64_t i) : name(std::move(n)), id(i) {}

    [[nodiscard]] bool is_valid() const { return !name.empty(); }

    bool operator==(const ServiceId& other) const {
        return id == other.id && name == other.name;
    }

    bool operator<(const ServiceId& other) const {
        return id < other.id;
    }

private:
    static std::uint64_t hash_name(const std::string& n) {
        // FNV-1a hash
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        for (char c : n) {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }
};

// =============================================================================
// Service State
// =============================================================================

/// Service lifecycle state
enum class ServiceState {
    Stopped,        ///< Service is not running
    Starting,       ///< Service is starting up
    Running,        ///< Service is fully operational
    Stopping,       ///< Service is shutting down
    Failed,         ///< Service failed to start or crashed
    Degraded,       ///< Service is running but with reduced functionality
};

/// Convert state to string
[[nodiscard]] inline const char* to_string(ServiceState state) {
    switch (state) {
        case ServiceState::Stopped: return "Stopped";
        case ServiceState::Starting: return "Starting";
        case ServiceState::Running: return "Running";
        case ServiceState::Stopping: return "Stopping";
        case ServiceState::Failed: return "Failed";
        case ServiceState::Degraded: return "Degraded";
    }
    return "Unknown";
}

// =============================================================================
// Service Health
// =============================================================================

/// Service health information
struct ServiceHealth {
    /// Health score (0.0 = dead, 1.0 = fully healthy)
    float score = 1.0f;

    /// Current state
    ServiceState state = ServiceState::Stopped;

    /// Optional status message
    std::string message;

    /// Last health check time
    std::chrono::steady_clock::time_point last_check;

    /// Number of restarts
    std::uint32_t restart_count = 0;

    /// Time of last failure
    std::optional<std::chrono::steady_clock::time_point> last_failure;

    /// Uptime since last start
    [[nodiscard]] std::chrono::steady_clock::duration uptime(
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const {
        return now - started_at;
    }

    /// When the service started
    std::chrono::steady_clock::time_point started_at;

    [[nodiscard]] bool is_healthy() const {
        return score >= 0.5f && state == ServiceState::Running;
    }

    [[nodiscard]] bool is_critical() const {
        return score < 0.25f || state == ServiceState::Failed;
    }
};

// =============================================================================
// Service Configuration
// =============================================================================

/// Service configuration options
struct ServiceConfig {
    /// Enable automatic restart on failure
    bool auto_restart = true;

    /// Maximum number of restart attempts
    std::uint32_t max_restart_attempts = 3;

    /// Delay between restart attempts
    std::chrono::milliseconds restart_delay{1000};

    /// Health check interval
    std::chrono::milliseconds health_check_interval{5000};

    /// Startup timeout
    std::chrono::milliseconds startup_timeout{30000};

    /// Shutdown timeout
    std::chrono::milliseconds shutdown_timeout{10000};

    /// Service priority (higher = started first, stopped last)
    int priority = 0;

    /// Dependencies (service names that must be running first)
    std::vector<std::string> dependencies;
};

// =============================================================================
// Service Interface
// =============================================================================

/// Base interface for services
class IService {
public:
    virtual ~IService() = default;

    // =========================================================================
    // Identification
    // =========================================================================

    /// Get the service ID
    [[nodiscard]] virtual ServiceId id() const = 0;

    /// Get the service name
    [[nodiscard]] virtual const std::string& name() const = 0;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Start the service
    /// @return true if started successfully
    virtual bool start() = 0;

    /// Stop the service
    virtual void stop() = 0;

    /// Get current state
    [[nodiscard]] virtual ServiceState state() const = 0;

    // =========================================================================
    // Health
    // =========================================================================

    /// Get health information
    [[nodiscard]] virtual ServiceHealth health() const = 0;

    /// Perform health check
    /// @return Health score (0.0 - 1.0)
    [[nodiscard]] virtual float check_health() = 0;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get configuration
    [[nodiscard]] virtual const ServiceConfig& config() const = 0;

    /// Update configuration
    virtual void configure(const ServiceConfig& config) = 0;
};

// =============================================================================
// Service Base Implementation
// =============================================================================

/// Base class for implementing services
class ServiceBase : public IService {
public:
    explicit ServiceBase(std::string name, ServiceConfig config = {})
        : m_id(std::move(name))
        , m_config(std::move(config))
        , m_state(ServiceState::Stopped)
    {
    }

    ~ServiceBase() override {
        if (m_state == ServiceState::Running) {
            stop();
        }
    }

    // =========================================================================
    // IService Implementation
    // =========================================================================

    [[nodiscard]] ServiceId id() const override { return m_id; }
    [[nodiscard]] const std::string& name() const override { return m_id.name; }
    [[nodiscard]] ServiceState state() const override { return m_state.load(); }
    [[nodiscard]] const ServiceConfig& config() const override { return m_config; }

    void configure(const ServiceConfig& config) override {
        m_config = config;
    }

    bool start() override {
        ServiceState expected = ServiceState::Stopped;
        if (!m_state.compare_exchange_strong(expected, ServiceState::Starting)) {
            return false; // Already running or starting
        }

        m_health.started_at = std::chrono::steady_clock::now();

        if (on_start()) {
            m_state = ServiceState::Running;
            m_health.state = ServiceState::Running;
            m_health.score = 1.0f;
            return true;
        } else {
            m_state = ServiceState::Failed;
            m_health.state = ServiceState::Failed;
            m_health.score = 0.0f;
            m_health.last_failure = std::chrono::steady_clock::now();
            return false;
        }
    }

    void stop() override {
        ServiceState expected = ServiceState::Running;
        if (!m_state.compare_exchange_strong(expected, ServiceState::Stopping)) {
            expected = ServiceState::Degraded;
            if (!m_state.compare_exchange_strong(expected, ServiceState::Stopping)) {
                return; // Not running
            }
        }

        on_stop();

        m_state = ServiceState::Stopped;
        m_health.state = ServiceState::Stopped;
    }

    [[nodiscard]] ServiceHealth health() const override {
        ServiceHealth h = m_health;
        h.state = m_state.load();
        return h;
    }

    [[nodiscard]] float check_health() override {
        if (m_state != ServiceState::Running && m_state != ServiceState::Degraded) {
            return 0.0f;
        }

        float score = on_check_health();
        m_health.score = score;
        m_health.last_check = std::chrono::steady_clock::now();

        if (score < 0.5f && m_state == ServiceState::Running) {
            m_state = ServiceState::Degraded;
            m_health.state = ServiceState::Degraded;
        } else if (score >= 0.5f && m_state == ServiceState::Degraded) {
            m_state = ServiceState::Running;
            m_health.state = ServiceState::Running;
        }

        return score;
    }

protected:
    // =========================================================================
    // Override Points
    // =========================================================================

    /// Called when service starts
    /// @return true if started successfully
    virtual bool on_start() = 0;

    /// Called when service stops
    virtual void on_stop() = 0;

    /// Called for health check
    /// @return Health score (0.0 - 1.0)
    virtual float on_check_health() { return 1.0f; }

    /// Mark service as degraded
    void set_degraded(const std::string& reason) {
        m_state = ServiceState::Degraded;
        m_health.state = ServiceState::Degraded;
        m_health.message = reason;
    }

    /// Mark service as failed
    void set_failed(const std::string& reason) {
        m_state = ServiceState::Failed;
        m_health.state = ServiceState::Failed;
        m_health.score = 0.0f;
        m_health.message = reason;
        m_health.last_failure = std::chrono::steady_clock::now();
    }

    /// Increment restart counter
    void increment_restart_count() {
        ++m_health.restart_count;
    }

private:
    ServiceId m_id;
    ServiceConfig m_config;
    std::atomic<ServiceState> m_state;
    ServiceHealth m_health;
};

} // namespace void_services

// Hash specialization
template<>
struct std::hash<void_services::ServiceId> {
    std::size_t operator()(const void_services::ServiceId& id) const noexcept {
        return static_cast<std::size_t>(id.id);
    }
};
