/// @file services.cpp
/// @brief Main void_services module implementation
///
/// This file provides:
/// - Module version information
/// - Module initialization and shutdown
/// - Hot-reload support integration
/// - Global registry access

#include <void_engine/services/services.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_services {

// =============================================================================
// Module Version
// =============================================================================

namespace {

/// Module version components
constexpr std::uint16_t VERSION_MAJOR = 1;
constexpr std::uint16_t VERSION_MINOR = 0;
constexpr std::uint16_t VERSION_PATCH = 0;

/// Module initialization state
std::atomic<bool> s_initialized{false};

/// Global shared registry (optional singleton pattern)
std::shared_mutex s_global_registry_mutex;
std::shared_ptr<ServiceRegistry> s_global_registry;

/// Global shared event bus (optional singleton pattern)
std::shared_mutex s_global_bus_mutex;
std::shared_ptr<EventBus> s_global_event_bus;

/// Global shared session manager (optional singleton pattern)
std::shared_mutex s_global_session_mutex;
std::shared_ptr<SessionManager> s_global_session_manager;

} // anonymous namespace

/// Get module version string
[[nodiscard]] const char* version() noexcept {
    return "1.0.0";
}

/// Get major version
[[nodiscard]] std::uint16_t version_major() noexcept {
    return VERSION_MAJOR;
}

/// Get minor version
[[nodiscard]] std::uint16_t version_minor() noexcept {
    return VERSION_MINOR;
}

/// Get patch version
[[nodiscard]] std::uint16_t version_patch() noexcept {
    return VERSION_PATCH;
}

/// Check if a version is compatible with current module
/// @param major Major version to check
/// @param minor Minor version to check
/// @return true if versions are compatible
[[nodiscard]] bool is_version_compatible(std::uint16_t major, std::uint16_t minor) noexcept {
    // Major version must match exactly
    if (major != VERSION_MAJOR) {
        return false;
    }
    // Minor version must be <= current (backwards compatible)
    return minor <= VERSION_MINOR;
}

// =============================================================================
// Module Lifecycle
// =============================================================================

/// Initialize the services module
/// @return true if initialization succeeded
bool init() {
    bool expected = false;
    if (!s_initialized.compare_exchange_strong(expected, true)) {
        return true; // Already initialized
    }

    // Create default global instances
    {
        std::unique_lock lock(s_global_registry_mutex);
        s_global_registry = std::make_shared<ServiceRegistry>();
    }

    {
        std::unique_lock lock(s_global_bus_mutex);
        s_global_event_bus = std::make_shared<EventBus>();
    }

    {
        std::unique_lock lock(s_global_session_mutex);
        s_global_session_manager = std::make_shared<SessionManager>();
    }

    return true;
}

/// Shutdown the services module
void shutdown() {
    bool expected = true;
    if (!s_initialized.compare_exchange_strong(expected, false)) {
        return; // Already shutdown
    }

    // Stop all services and cleanup
    {
        std::unique_lock lock(s_global_registry_mutex);
        if (s_global_registry) {
            s_global_registry->stop_all();
            s_global_registry.reset();
        }
    }

    {
        std::unique_lock lock(s_global_bus_mutex);
        if (s_global_event_bus) {
            s_global_event_bus->clear_queue();
            s_global_event_bus->clear_subscriptions();
            s_global_event_bus.reset();
        }
    }

    {
        std::unique_lock lock(s_global_session_mutex);
        if (s_global_session_manager) {
            s_global_session_manager->stop_cleanup();
            s_global_session_manager.reset();
        }
    }
}

/// Check if module is initialized
[[nodiscard]] bool is_initialized() noexcept {
    return s_initialized.load();
}

// =============================================================================
// Global Instance Access
// =============================================================================

/// Get the global service registry
/// @note Module must be initialized first
/// @return Shared pointer to global registry, or nullptr if not initialized
[[nodiscard]] std::shared_ptr<ServiceRegistry> get_global_registry() {
    std::shared_lock lock(s_global_registry_mutex);
    return s_global_registry;
}

/// Set the global service registry
/// @param registry New registry to use as global
void set_global_registry(std::shared_ptr<ServiceRegistry> registry) {
    std::unique_lock lock(s_global_registry_mutex);
    s_global_registry = std::move(registry);
}

/// Get the global event bus
/// @note Module must be initialized first
/// @return Shared pointer to global event bus, or nullptr if not initialized
[[nodiscard]] std::shared_ptr<EventBus> get_global_event_bus() {
    std::shared_lock lock(s_global_bus_mutex);
    return s_global_event_bus;
}

/// Set the global event bus
/// @param bus New event bus to use as global
void set_global_event_bus(std::shared_ptr<EventBus> bus) {
    std::unique_lock lock(s_global_bus_mutex);
    s_global_event_bus = std::move(bus);
}

/// Get the global session manager
/// @note Module must be initialized first
/// @return Shared pointer to global session manager, or nullptr if not initialized
[[nodiscard]] std::shared_ptr<SessionManager> get_global_session_manager() {
    std::shared_lock lock(s_global_session_mutex);
    return s_global_session_manager;
}

/// Set the global session manager
/// @param manager New session manager to use as global
void set_global_session_manager(std::shared_ptr<SessionManager> manager) {
    std::unique_lock lock(s_global_session_mutex);
    s_global_session_manager = std::move(manager);
}

// =============================================================================
// Hot-Reload Support
// =============================================================================

/// Snapshot of entire services module state for hot-reload
struct ServicesModuleSnapshot {
    static constexpr std::uint32_t MAGIC = 0x56534D53; // "VSMS"
    static constexpr std::uint32_t VERSION = 1;

    std::uint32_t magic = MAGIC;
    std::uint32_t version = VERSION;
    std::vector<std::uint8_t> registry_data;
    std::vector<std::uint8_t> session_data;
    std::vector<std::uint8_t> event_bus_data;

    [[nodiscard]] bool is_valid() const {
        return magic == MAGIC && version == VERSION;
    }
};

/// Take a complete module snapshot for hot-reload
/// @return Binary snapshot data
[[nodiscard]] std::vector<std::uint8_t> take_module_snapshot() {
    ServicesModuleSnapshot snapshot;

    // Capture registry state
    {
        std::shared_lock lock(s_global_registry_mutex);
        if (s_global_registry) {
            snapshot.registry_data = take_and_serialize_registry(*s_global_registry);
        }
    }

    // Capture session state
    {
        std::shared_lock lock(s_global_session_mutex);
        if (s_global_session_manager) {
            snapshot.session_data = take_and_serialize_sessions(*s_global_session_manager);
        }
    }

    // Capture event bus state
    {
        std::shared_lock lock(s_global_bus_mutex);
        if (s_global_event_bus) {
            snapshot.event_bus_data = take_and_serialize_event_bus(*s_global_event_bus);
        }
    }

    // Serialize module snapshot
    BinaryWriter writer;
    writer.write_u32(snapshot.magic);
    writer.write_u32(snapshot.version);

    // Registry data
    writer.write_u32(static_cast<std::uint32_t>(snapshot.registry_data.size()));
    for (auto byte : snapshot.registry_data) {
        writer.write_u8(byte);
    }

    // Session data
    writer.write_u32(static_cast<std::uint32_t>(snapshot.session_data.size()));
    for (auto byte : snapshot.session_data) {
        writer.write_u8(byte);
    }

    // Event bus data
    writer.write_u32(static_cast<std::uint32_t>(snapshot.event_bus_data.size()));
    for (auto byte : snapshot.event_bus_data) {
        writer.write_u8(byte);
    }

    return writer.take();
}

/// Restore module state from a snapshot
/// @param data Binary snapshot data
/// @return true if restoration succeeded
bool restore_module_snapshot(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return false;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    std::uint32_t version = reader.read_u32();

    if (magic != ServicesModuleSnapshot::MAGIC ||
        version != ServicesModuleSnapshot::VERSION) {
        return false;
    }

    // Read registry data
    std::uint32_t registry_size = reader.read_u32();
    std::vector<std::uint8_t> registry_data;
    registry_data.reserve(registry_size);
    for (std::uint32_t i = 0; i < registry_size; ++i) {
        registry_data.push_back(reader.read_u8());
    }

    // Read session data
    std::uint32_t session_size = reader.read_u32();
    std::vector<std::uint8_t> session_data;
    session_data.reserve(session_size);
    for (std::uint32_t i = 0; i < session_size; ++i) {
        session_data.push_back(reader.read_u8());
    }

    // Read event bus data
    std::uint32_t bus_size = reader.read_u32();
    std::vector<std::uint8_t> bus_data;
    bus_data.reserve(bus_size);
    for (std::uint32_t i = 0; i < bus_size; ++i) {
        bus_data.push_back(reader.read_u8());
    }

    if (!reader.valid()) {
        return false;
    }

    // Restore registry
    {
        std::unique_lock lock(s_global_registry_mutex);
        if (s_global_registry && !registry_data.empty()) {
            deserialize_and_restore_registry(*s_global_registry, registry_data);
        }
    }

    // Restore sessions
    {
        std::unique_lock lock(s_global_session_mutex);
        if (s_global_session_manager && !session_data.empty()) {
            deserialize_and_restore_sessions(*s_global_session_manager, session_data);
        }
    }

    // Restore event bus
    {
        std::unique_lock lock(s_global_bus_mutex);
        if (s_global_event_bus && !bus_data.empty()) {
            deserialize_and_restore_event_bus(*s_global_event_bus, bus_data);
        }
    }

    return true;
}

// =============================================================================
// Module Statistics
// =============================================================================

/// Module-wide statistics summary
struct ModuleStats {
    // Registry stats
    std::size_t total_services = 0;
    std::size_t running_services = 0;
    float average_health = 0.0f;

    // Session stats
    std::size_t active_sessions = 0;
    std::size_t total_sessions_created = 0;

    // Event bus stats
    std::uint64_t events_published = 0;
    std::uint64_t events_processed = 0;
    std::size_t active_subscriptions = 0;
};

/// Get module-wide statistics
[[nodiscard]] ModuleStats get_module_stats() {
    ModuleStats stats;

    // Registry stats
    {
        std::shared_lock lock(s_global_registry_mutex);
        if (s_global_registry) {
            auto reg_stats = s_global_registry->stats();
            stats.total_services = reg_stats.total_services;
            stats.running_services = reg_stats.running_services;
            stats.average_health = reg_stats.average_health;
        }
    }

    // Session stats
    {
        std::shared_lock lock(s_global_session_mutex);
        if (s_global_session_manager) {
            auto sess_stats = s_global_session_manager->stats();
            stats.active_sessions = sess_stats.active_sessions;
            stats.total_sessions_created = sess_stats.total_created;
        }
    }

    // Event bus stats
    {
        std::shared_lock lock(s_global_bus_mutex);
        if (s_global_event_bus) {
            auto bus_stats = s_global_event_bus->stats();
            stats.events_published = bus_stats.events_published;
            stats.events_processed = bus_stats.events_processed;
            stats.active_subscriptions = bus_stats.active_subscriptions;
        }
    }

    return stats;
}

/// Format module statistics as a human-readable string
[[nodiscard]] std::string format_module_stats() {
    auto stats = get_module_stats();

    std::ostringstream oss;
    oss << "void_services " << version() << "\n";
    oss << "============================\n";
    oss << "Services:\n";
    oss << "  Total:    " << stats.total_services << "\n";
    oss << "  Running:  " << stats.running_services << "\n";
    oss << "  Health:   " << (stats.average_health * 100.0f) << "%\n";
    oss << "\nSessions:\n";
    oss << "  Active:   " << stats.active_sessions << "\n";
    oss << "  Created:  " << stats.total_sessions_created << "\n";
    oss << "\nEvents:\n";
    oss << "  Published:     " << stats.events_published << "\n";
    oss << "  Processed:     " << stats.events_processed << "\n";
    oss << "  Subscriptions: " << stats.active_subscriptions;

    return oss.str();
}

// =============================================================================
// Service Factory Registration
// =============================================================================

namespace {

/// Service factory function type
using ServiceFactory = std::function<std::shared_ptr<IService>()>;

/// Registered service factories
std::shared_mutex s_factory_mutex;
std::unordered_map<std::string, ServiceFactory> s_service_factories;

} // anonymous namespace

/// Register a service factory
/// @param name Service type name
/// @param factory Factory function to create service instances
/// @return true if registered successfully
bool register_service_factory(const std::string& name, ServiceFactory factory) {
    std::unique_lock lock(s_factory_mutex);
    if (s_service_factories.count(name) > 0) {
        return false; // Already registered
    }
    s_service_factories[name] = std::move(factory);
    return true;
}

/// Unregister a service factory
/// @param name Service type name
/// @return true if unregistered successfully
bool unregister_service_factory(const std::string& name) {
    std::unique_lock lock(s_factory_mutex);
    return s_service_factories.erase(name) > 0;
}

/// Create a service instance from a registered factory
/// @param name Service type name
/// @return New service instance, or nullptr if factory not found
[[nodiscard]] std::shared_ptr<IService> create_service(const std::string& name) {
    std::shared_lock lock(s_factory_mutex);
    auto it = s_service_factories.find(name);
    if (it == s_service_factories.end()) {
        return nullptr;
    }
    return it->second();
}

/// Get all registered factory names
[[nodiscard]] std::vector<std::string> get_registered_factories() {
    std::shared_lock lock(s_factory_mutex);
    std::vector<std::string> names;
    names.reserve(s_service_factories.size());
    for (const auto& [name, _] : s_service_factories) {
        names.push_back(name);
    }
    return names;
}

/// Check if a factory is registered
[[nodiscard]] bool has_factory(const std::string& name) {
    std::shared_lock lock(s_factory_mutex);
    return s_service_factories.count(name) > 0;
}

} // namespace void_services
