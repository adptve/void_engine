#pragma once

/// @file registry.hpp
/// @brief Service registry for managing service lifecycles
///
/// The ServiceRegistry manages:
/// - Service registration and discovery
/// - Lifecycle management (start/stop/restart)
/// - Health monitoring with auto-restart
/// - Dependency ordering
/// - Thread-safe access

#include "service.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_services {

// =============================================================================
// Service Events
// =============================================================================

/// Events emitted by the service registry
enum class ServiceEventType {
    Registered,     ///< Service was registered
    Unregistered,   ///< Service was unregistered
    Starting,       ///< Service is starting
    Started,        ///< Service started successfully
    Stopping,       ///< Service is stopping
    Stopped,        ///< Service stopped
    Failed,         ///< Service failed
    Restarting,     ///< Service is restarting
    HealthChanged,  ///< Service health changed
};

/// Service event data
struct ServiceEvent {
    ServiceEventType type;
    ServiceId service_id;
    std::string message;
    std::chrono::steady_clock::time_point timestamp;

    [[nodiscard]] static ServiceEvent create(
        ServiceEventType t, const ServiceId& id, std::string msg = {}) {
        return {t, id, std::move(msg), std::chrono::steady_clock::now()};
    }
};

/// Service event callback
using ServiceEventCallback = std::function<void(const ServiceEvent&)>;

// =============================================================================
// Service Registry Statistics
// =============================================================================

/// Registry statistics
struct RegistryStats {
    std::size_t total_services = 0;
    std::size_t running_services = 0;
    std::size_t stopped_services = 0;
    std::size_t failed_services = 0;
    std::size_t degraded_services = 0;
    std::uint64_t total_restarts = 0;
    float average_health = 0.0f;
};

// =============================================================================
// Service Registry
// =============================================================================

/// Central registry for managing services
class ServiceRegistry {
public:
    ServiceRegistry() : m_enabled(true), m_health_check_running(false) {}

    ~ServiceRegistry() {
        stop_health_monitor();
        stop_all();
    }

    // Non-copyable
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    // =========================================================================
    // Registration
    // =========================================================================

    /// Register a service
    /// @param service Service to register
    /// @return true if registered successfully
    bool register_service(std::shared_ptr<IService> service) {
        if (!service) return false;

        std::unique_lock lock(m_mutex);

        const auto& id = service->id();
        if (m_services.count(id)) {
            return false; // Already registered
        }

        ServiceEntry entry;
        entry.service = std::move(service);
        entry.registration_order = m_next_order++;

        m_services[id] = std::move(entry);
        m_order.push_back(id);

        emit_event(ServiceEvent::create(ServiceEventType::Registered, id));
        return true;
    }

    /// Register a service with a specific type
    template<typename T, typename... Args>
    std::shared_ptr<T> register_service(Args&&... args) {
        auto service = std::make_shared<T>(std::forward<Args>(args)...);
        if (register_service(service)) {
            return service;
        }
        return nullptr;
    }

    /// Unregister a service by ID
    bool unregister(const ServiceId& id) {
        std::unique_lock lock(m_mutex);

        auto it = m_services.find(id);
        if (it == m_services.end()) {
            return false;
        }

        // Stop service if running
        if (it->second.service->state() == ServiceState::Running) {
            lock.unlock();
            stop_service(id);
            lock.lock();
        }

        m_services.erase(it);
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id), m_order.end());

        emit_event(ServiceEvent::create(ServiceEventType::Unregistered, id));
        return true;
    }

    /// Unregister a service by name
    bool unregister(const std::string& name) {
        return unregister(ServiceId(name));
    }

    // =========================================================================
    // Service Access
    // =========================================================================

    /// Get a service by ID
    [[nodiscard]] std::shared_ptr<IService> get(const ServiceId& id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_services.find(id);
        return it != m_services.end() ? it->second.service : nullptr;
    }

    /// Get a service by name
    [[nodiscard]] std::shared_ptr<IService> get(const std::string& name) const {
        return get(ServiceId(name));
    }

    /// Get a service with a specific type
    template<typename T>
    [[nodiscard]] std::shared_ptr<T> get_typed(const std::string& name) const {
        auto service = get(name);
        return std::dynamic_pointer_cast<T>(service);
    }

    /// Check if a service is registered
    [[nodiscard]] bool has(const ServiceId& id) const {
        std::shared_lock lock(m_mutex);
        return m_services.count(id) > 0;
    }

    /// Check if a service is registered by name
    [[nodiscard]] bool has(const std::string& name) const {
        return has(ServiceId(name));
    }

    /// Get all registered service IDs
    [[nodiscard]] std::vector<ServiceId> list() const {
        std::shared_lock lock(m_mutex);
        return m_order;
    }

    /// Get service count
    [[nodiscard]] std::size_t count() const {
        std::shared_lock lock(m_mutex);
        return m_services.size();
    }

    // =========================================================================
    // Lifecycle Control
    // =========================================================================

    /// Start a specific service
    bool start_service(const ServiceId& id) {
        auto service = get(id);
        if (!service) return false;

        emit_event(ServiceEvent::create(ServiceEventType::Starting, id));

        // Start dependencies first
        for (const auto& dep_name : service->config().dependencies) {
            ServiceId dep_id(dep_name);
            auto dep = get(dep_id);
            if (dep && dep->state() != ServiceState::Running) {
                if (!start_service(dep_id)) {
                    emit_event(ServiceEvent::create(
                        ServiceEventType::Failed, id,
                        "Dependency failed: " + dep_name));
                    return false;
                }
            }
        }

        bool success = service->start();

        if (success) {
            emit_event(ServiceEvent::create(ServiceEventType::Started, id));
        } else {
            emit_event(ServiceEvent::create(ServiceEventType::Failed, id));
            maybe_restart(id);
        }

        return success;
    }

    /// Start a service by name
    bool start_service(const std::string& name) {
        return start_service(ServiceId(name));
    }

    /// Stop a specific service
    void stop_service(const ServiceId& id) {
        auto service = get(id);
        if (!service) return;

        emit_event(ServiceEvent::create(ServiceEventType::Stopping, id));
        service->stop();
        emit_event(ServiceEvent::create(ServiceEventType::Stopped, id));
    }

    /// Stop a service by name
    void stop_service(const std::string& name) {
        stop_service(ServiceId(name));
    }

    /// Restart a specific service
    bool restart_service(const ServiceId& id) {
        emit_event(ServiceEvent::create(ServiceEventType::Restarting, id));
        stop_service(id);
        return start_service(id);
    }

    /// Restart a service by name
    bool restart_service(const std::string& name) {
        return restart_service(ServiceId(name));
    }

    /// Start all registered services (in priority order)
    void start_all() {
        if (!m_enabled) return;

        auto ids = get_ordered_ids();
        for (const auto& id : ids) {
            auto service = get(id);
            if (service && service->state() == ServiceState::Stopped) {
                start_service(id);
            }
        }
    }

    /// Stop all running services (in reverse priority order)
    void stop_all() {
        auto ids = get_ordered_ids();
        std::reverse(ids.begin(), ids.end());

        for (const auto& id : ids) {
            auto service = get(id);
            if (service && service->state() == ServiceState::Running) {
                stop_service(id);
            }
        }
    }

    /// Restart all services
    void restart_all() {
        stop_all();
        start_all();
    }

    // =========================================================================
    // Health Monitoring
    // =========================================================================

    /// Start the health monitoring thread
    void start_health_monitor(std::chrono::milliseconds interval = std::chrono::milliseconds{5000}) {
        if (m_health_check_running) return;

        m_health_check_running = true;
        m_health_check_interval = interval;
        m_health_thread = std::thread(&ServiceRegistry::health_monitor_loop, this);
    }

    /// Stop the health monitoring thread
    void stop_health_monitor() {
        m_health_check_running = false;
        if (m_health_thread.joinable()) {
            m_health_thread.join();
        }
    }

    /// Perform health check on all services
    void check_all_health() {
        std::shared_lock lock(m_mutex);
        for (auto& [id, entry] : m_services) {
            if (entry.service->state() == ServiceState::Running ||
                entry.service->state() == ServiceState::Degraded) {
                float old_health = entry.last_health;
                float new_health = entry.service->check_health();
                entry.last_health = new_health;

                if (std::abs(new_health - old_health) > 0.1f) {
                    emit_event(ServiceEvent::create(
                        ServiceEventType::HealthChanged, id,
                        "Health: " + std::to_string(new_health)));
                }

                // Check for failures
                if (new_health < 0.25f && entry.service->config().auto_restart) {
                    lock.unlock();
                    maybe_restart(id);
                    lock.lock();
                }
            }
        }
    }

    /// Get health for a specific service
    [[nodiscard]] std::optional<ServiceHealth> get_health(const ServiceId& id) const {
        auto service = get(id);
        if (service) {
            return service->health();
        }
        return std::nullopt;
    }

    /// Get health for all services
    [[nodiscard]] std::unordered_map<ServiceId, ServiceHealth> get_all_health() const {
        std::shared_lock lock(m_mutex);
        std::unordered_map<ServiceId, ServiceHealth> result;
        for (const auto& [id, entry] : m_services) {
            result[id] = entry.service->health();
        }
        return result;
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get registry statistics
    [[nodiscard]] RegistryStats stats() const {
        std::shared_lock lock(m_mutex);

        RegistryStats s;
        s.total_services = m_services.size();

        float total_health = 0.0f;
        for (const auto& [id, entry] : m_services) {
            auto state = entry.service->state();
            switch (state) {
                case ServiceState::Running: ++s.running_services; break;
                case ServiceState::Stopped: ++s.stopped_services; break;
                case ServiceState::Failed: ++s.failed_services; break;
                case ServiceState::Degraded: ++s.degraded_services; break;
                default: break;
            }

            auto health = entry.service->health();
            total_health += health.score;
            s.total_restarts += health.restart_count;
        }

        if (!m_services.empty()) {
            s.average_health = total_health / static_cast<float>(m_services.size());
        }

        return s;
    }

    // =========================================================================
    // Events
    // =========================================================================

    /// Set event callback
    void set_event_callback(ServiceEventCallback callback) {
        std::unique_lock lock(m_mutex);
        m_event_callback = std::move(callback);
    }

    // =========================================================================
    // Control
    // =========================================================================

    /// Enable/disable the registry
    void set_enabled(bool enabled) {
        m_enabled = enabled;
        if (!enabled) {
            stop_all();
        }
    }

    /// Check if enabled
    [[nodiscard]] bool is_enabled() const { return m_enabled; }

private:
    struct ServiceEntry {
        std::shared_ptr<IService> service;
        std::size_t registration_order = 0;
        float last_health = 1.0f;
    };

    [[nodiscard]] std::vector<ServiceId> get_ordered_ids() const {
        std::shared_lock lock(m_mutex);

        std::vector<std::pair<ServiceId, int>> sorted;
        for (const auto& id : m_order) {
            auto it = m_services.find(id);
            if (it != m_services.end()) {
                sorted.push_back({id, it->second.service->config().priority});
            }
        }

        // Sort by priority (higher first), then by registration order
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            });

        std::vector<ServiceId> result;
        result.reserve(sorted.size());
        for (const auto& [id, _] : sorted) {
            result.push_back(id);
        }
        return result;
    }

    void maybe_restart(const ServiceId& id) {
        auto service = get(id);
        if (!service) return;

        const auto& config = service->config();
        if (!config.auto_restart) return;

        auto health = service->health();
        if (health.restart_count >= config.max_restart_attempts) {
            emit_event(ServiceEvent::create(
                ServiceEventType::Failed, id,
                "Max restart attempts exceeded"));
            return;
        }

        // Delay before restart
        std::this_thread::sleep_for(config.restart_delay);

        emit_event(ServiceEvent::create(ServiceEventType::Restarting, id));
        service->stop();
        if (service->start()) {
            emit_event(ServiceEvent::create(ServiceEventType::Started, id));
        } else {
            emit_event(ServiceEvent::create(ServiceEventType::Failed, id));
        }
    }

    void health_monitor_loop() {
        while (m_health_check_running) {
            std::this_thread::sleep_for(m_health_check_interval);
            if (m_health_check_running && m_enabled) {
                check_all_health();
            }
        }
    }

    void emit_event(const ServiceEvent& event) {
        if (m_event_callback) {
            m_event_callback(event);
        }
    }

    mutable std::shared_mutex m_mutex;
    std::unordered_map<ServiceId, ServiceEntry> m_services;
    std::vector<ServiceId> m_order;
    std::size_t m_next_order = 0;

    std::atomic<bool> m_enabled;
    ServiceEventCallback m_event_callback;

    std::atomic<bool> m_health_check_running;
    std::chrono::milliseconds m_health_check_interval{5000};
    std::thread m_health_thread;
};

// =============================================================================
// Shared Service Registry
// =============================================================================

/// Thread-safe shared pointer to ServiceRegistry
class SharedServiceRegistry {
public:
    SharedServiceRegistry() : m_registry(std::make_shared<ServiceRegistry>()) {}

    explicit SharedServiceRegistry(std::shared_ptr<ServiceRegistry> registry)
        : m_registry(std::move(registry)) {}

    [[nodiscard]] ServiceRegistry* operator->() const { return m_registry.get(); }
    [[nodiscard]] ServiceRegistry& operator*() const { return *m_registry; }
    [[nodiscard]] std::shared_ptr<ServiceRegistry> get() const { return m_registry; }

private:
    std::shared_ptr<ServiceRegistry> m_registry;
};

} // namespace void_services
