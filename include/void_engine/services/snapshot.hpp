#pragma once

/// @file snapshot.hpp
/// @brief Hot-reload snapshot support for void_services
///
/// Provides serialization/deserialization for:
/// - ServiceRegistry state
/// - SessionManager state
/// - Service health and restart counts
/// - Session state, permissions, and metadata

#include "registry.hpp"
#include "session.hpp"
#include "event_bus.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace void_services {

// =============================================================================
// Binary Serialization Helpers
// =============================================================================

/// Binary writer for snapshot serialization
class BinaryWriter {
public:
    void write_u8(std::uint8_t v) { m_buffer.push_back(v); }

    void write_u32(std::uint32_t v) {
        m_buffer.push_back(static_cast<std::uint8_t>(v & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void write_u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            m_buffer.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void write_bool(bool v) { write_u8(v ? 1 : 0); }

    void write_string(const std::string& s) {
        write_u32(static_cast<std::uint32_t>(s.size()));
        m_buffer.insert(m_buffer.end(), s.begin(), s.end());
    }

    void write_f32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        write_u32(bits);
    }

    [[nodiscard]] std::vector<std::uint8_t> take() {
        return std::move(m_buffer);
    }

private:
    std::vector<std::uint8_t> m_buffer;
};

/// Binary reader for snapshot deserialization
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<std::uint8_t>& data)
        : m_data(data), m_offset(0) {}

    [[nodiscard]] bool has_remaining(std::size_t bytes) const {
        return m_offset + bytes <= m_data.size();
    }

    [[nodiscard]] std::uint8_t read_u8() {
        if (!has_remaining(1)) return 0;
        return m_data[m_offset++];
    }

    [[nodiscard]] std::uint32_t read_u32() {
        if (!has_remaining(4)) return 0;
        std::uint32_t v = m_data[m_offset] |
                          (m_data[m_offset + 1] << 8) |
                          (m_data[m_offset + 2] << 16) |
                          (m_data[m_offset + 3] << 24);
        m_offset += 4;
        return v;
    }

    [[nodiscard]] std::uint64_t read_u64() {
        if (!has_remaining(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(m_data[m_offset + i]) << (i * 8);
        }
        m_offset += 8;
        return v;
    }

    [[nodiscard]] bool read_bool() { return read_u8() != 0; }

    [[nodiscard]] std::string read_string() {
        std::uint32_t len = read_u32();
        if (!has_remaining(len)) return "";
        std::string s(m_data.begin() + m_offset, m_data.begin() + m_offset + len);
        m_offset += len;
        return s;
    }

    [[nodiscard]] float read_f32() {
        std::uint32_t bits = read_u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    [[nodiscard]] bool valid() const { return m_offset <= m_data.size(); }

private:
    const std::vector<std::uint8_t>& m_data;
    std::size_t m_offset;
};

// =============================================================================
// Service Registry Snapshot
// =============================================================================

/// Snapshot of a single service's state
struct ServiceStateSnapshot {
    std::string service_name;
    ServiceState state;
    float health_score;
    std::uint32_t restart_count;
    std::string last_error;
};

/// Snapshot of the entire registry
struct RegistrySnapshot {
    static constexpr std::uint32_t VERSION = 1;

    std::uint32_t version = VERSION;
    bool enabled = true;
    std::vector<ServiceStateSnapshot> services;

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Serialize a service's state for hot-reload
inline void serialize_service_state(BinaryWriter& writer, const ServiceStateSnapshot& snap) {
    writer.write_string(snap.service_name);
    writer.write_u8(static_cast<std::uint8_t>(snap.state));
    writer.write_f32(snap.health_score);
    writer.write_u32(snap.restart_count);
    writer.write_string(snap.last_error);
}

/// Deserialize a service's state
inline ServiceStateSnapshot deserialize_service_state(BinaryReader& reader) {
    ServiceStateSnapshot snap;
    snap.service_name = reader.read_string();
    snap.state = static_cast<ServiceState>(reader.read_u8());
    snap.health_score = reader.read_f32();
    snap.restart_count = reader.read_u32();
    snap.last_error = reader.read_string();
    return snap;
}

/// Take a snapshot of the registry for hot-reload
[[nodiscard]] inline RegistrySnapshot take_registry_snapshot(const ServiceRegistry& registry) {
    RegistrySnapshot snapshot;
    snapshot.version = RegistrySnapshot::VERSION;
    snapshot.enabled = registry.is_enabled();

    for (const auto& id : registry.list()) {
        auto service = registry.get(id);
        if (service) {
            ServiceStateSnapshot sss;
            sss.service_name = service->name();
            sss.state = service->state();

            auto health = service->health();
            sss.health_score = health.score;
            sss.restart_count = health.restart_count;
            sss.last_error = health.message;

            snapshot.services.push_back(std::move(sss));
        }
    }

    return snapshot;
}

/// Serialize registry snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_registry_snapshot(
    const RegistrySnapshot& snapshot) {
    BinaryWriter writer;

    writer.write_u32(snapshot.version);
    writer.write_bool(snapshot.enabled);
    writer.write_u32(static_cast<std::uint32_t>(snapshot.services.size()));

    for (const auto& sss : snapshot.services) {
        serialize_service_state(writer, sss);
    }

    return writer.take();
}

/// Deserialize registry snapshot from binary
[[nodiscard]] inline std::optional<RegistrySnapshot> deserialize_registry_snapshot(
    const std::vector<std::uint8_t>& data) {
    if (data.size() < 4) return std::nullopt;

    BinaryReader reader(data);

    RegistrySnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    snapshot.enabled = reader.read_bool();
    std::uint32_t count = reader.read_u32();

    snapshot.services.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        snapshot.services.push_back(deserialize_service_state(reader));
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

/// Restore a registry from a snapshot
/// @note Services must be re-registered after hot-reload. This function
///       restores the enabled state and matches service states by name.
/// @param registry The registry to restore into
/// @param snapshot The snapshot to restore from
/// @return Number of services whose state was restored
inline std::size_t restore_registry_snapshot(
    ServiceRegistry& registry,
    const RegistrySnapshot& snapshot) {

    if (!snapshot.is_compatible()) {
        return 0;
    }

    // Restore enabled state
    registry.set_enabled(snapshot.enabled);

    // Match and restore service states by name
    std::size_t restored = 0;
    for (const auto& sss : snapshot.services) {
        // Find service by name in the registry
        for (const auto& id : registry.list()) {
            auto service = registry.get(id);
            if (service && service->name() == sss.service_name) {
                // Restore state based on snapshot
                switch (sss.state) {
                    case ServiceState::Running:
                        if (service->state() != ServiceState::Running) {
                            registry.start_service(id);
                        }
                        break;
                    case ServiceState::Stopped:
                        if (service->state() == ServiceState::Running) {
                            registry.stop_service(id);
                        }
                        break;
                    default:
                        // Other states (Starting, Stopping, Failed, Degraded)
                        // are transient and will be handled by the service itself
                        break;
                }
                ++restored;
                break;
            }
        }
    }

    return restored;
}

/// Deserialize and restore registry in one call
/// @return Number of services restored, or 0 if deserialization failed
inline std::size_t deserialize_and_restore_registry(
    ServiceRegistry& registry,
    const std::vector<std::uint8_t>& data) {

    auto snapshot = deserialize_registry_snapshot(data);
    if (!snapshot) {
        return 0;
    }
    return restore_registry_snapshot(registry, *snapshot);
}

// =============================================================================
// Session Manager Snapshot
// =============================================================================

/// Snapshot of a single session
struct SessionSnapshot {
    std::uint64_t session_id;
    SessionState state;
    std::optional<std::string> user_id;
    bool authenticated;
    std::vector<std::string> permissions;
    std::vector<std::pair<std::string, std::string>> metadata;
};

/// Snapshot of the session manager
struct SessionManagerSnapshot {
    static constexpr std::uint32_t VERSION = 1;

    std::uint32_t version = VERSION;
    std::uint64_t next_session_id;
    std::vector<SessionSnapshot> sessions;
    std::uint64_t total_created;
    std::uint64_t total_terminated;
    std::uint64_t total_expired;
    std::size_t peak_concurrent;

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Serialize a session for hot-reload
inline void serialize_session(BinaryWriter& writer, const SessionSnapshot& snap) {
    writer.write_u64(snap.session_id);
    writer.write_u8(static_cast<std::uint8_t>(snap.state));
    writer.write_bool(snap.user_id.has_value());
    if (snap.user_id) {
        writer.write_string(*snap.user_id);
    }
    writer.write_bool(snap.authenticated);

    writer.write_u32(static_cast<std::uint32_t>(snap.permissions.size()));
    for (const auto& perm : snap.permissions) {
        writer.write_string(perm);
    }

    writer.write_u32(static_cast<std::uint32_t>(snap.metadata.size()));
    for (const auto& [key, value] : snap.metadata) {
        writer.write_string(key);
        writer.write_string(value);
    }
}

/// Deserialize a session
inline SessionSnapshot deserialize_session(BinaryReader& reader) {
    SessionSnapshot snap;
    snap.session_id = reader.read_u64();
    snap.state = static_cast<SessionState>(reader.read_u8());

    bool has_user = reader.read_bool();
    if (has_user) {
        snap.user_id = reader.read_string();
    }
    snap.authenticated = reader.read_bool();

    std::uint32_t perm_count = reader.read_u32();
    snap.permissions.reserve(perm_count);
    for (std::uint32_t i = 0; i < perm_count; ++i) {
        snap.permissions.push_back(reader.read_string());
    }

    std::uint32_t meta_count = reader.read_u32();
    snap.metadata.reserve(meta_count);
    for (std::uint32_t i = 0; i < meta_count; ++i) {
        std::string key = reader.read_string();
        std::string value = reader.read_string();
        snap.metadata.push_back({std::move(key), std::move(value)});
    }

    return snap;
}

/// Take a snapshot of the session manager for hot-reload
[[nodiscard]] inline SessionManagerSnapshot take_session_snapshot(const SessionManager& manager) {
    SessionManagerSnapshot snapshot;
    snapshot.version = SessionManagerSnapshot::VERSION;

    auto stats = manager.stats();
    snapshot.total_created = stats.total_created;
    snapshot.total_terminated = stats.total_terminated;
    snapshot.total_expired = stats.total_expired;
    snapshot.peak_concurrent = stats.peak_concurrent;

    // Capture active sessions
    auto active_ids = manager.list_active();
    for (const auto& id : active_ids) {
        auto session = manager.get(id);
        if (session) {
            SessionSnapshot ss;
            ss.session_id = id.id;
            ss.state = session->state();
            ss.user_id = session->user_id();
            ss.authenticated = session->is_authenticated();

            auto perms = session->permissions();
            ss.permissions.reserve(perms.size());
            for (const auto& p : perms) {
                ss.permissions.push_back(p);
            }

            auto meta = session->metadata();
            ss.metadata.reserve(meta.size());
            for (const auto& [k, v] : meta) {
                ss.metadata.push_back({k, v});
            }

            snapshot.sessions.push_back(std::move(ss));
        }
    }

    return snapshot;
}

/// Serialize session manager snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_session_snapshot(
    const SessionManagerSnapshot& snapshot) {
    BinaryWriter writer;

    writer.write_u32(snapshot.version);
    writer.write_u64(snapshot.next_session_id);
    writer.write_u64(snapshot.total_created);
    writer.write_u64(snapshot.total_terminated);
    writer.write_u64(snapshot.total_expired);
    writer.write_u64(snapshot.peak_concurrent);

    writer.write_u32(static_cast<std::uint32_t>(snapshot.sessions.size()));
    for (const auto& ss : snapshot.sessions) {
        serialize_session(writer, ss);
    }

    return writer.take();
}

/// Deserialize session manager snapshot from binary
[[nodiscard]] inline std::optional<SessionManagerSnapshot> deserialize_session_snapshot(
    const std::vector<std::uint8_t>& data) {
    if (data.size() < 4) return std::nullopt;

    BinaryReader reader(data);

    SessionManagerSnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    snapshot.next_session_id = reader.read_u64();
    snapshot.total_created = reader.read_u64();
    snapshot.total_terminated = reader.read_u64();
    snapshot.total_expired = reader.read_u64();
    snapshot.peak_concurrent = reader.read_u64();

    std::uint32_t session_count = reader.read_u32();
    snapshot.sessions.reserve(session_count);
    for (std::uint32_t i = 0; i < session_count; ++i) {
        snapshot.sessions.push_back(deserialize_session(reader));
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

/// Restore a session manager from a snapshot
/// @note Session variables (std::any) cannot be serialized and must be
///       re-established by the application after hot-reload.
/// @param manager The session manager to restore into
/// @param snapshot The snapshot to restore from
/// @return Number of sessions restored
inline std::size_t restore_session_snapshot(
    SessionManager& manager,
    const SessionManagerSnapshot& snapshot) {

    if (!snapshot.is_compatible()) {
        return 0;
    }

    // Restore stats
    manager.restore_stats(
        snapshot.total_created,
        snapshot.total_terminated,
        snapshot.total_expired,
        snapshot.peak_concurrent);

    // Set next session ID to avoid conflicts
    manager.set_next_session_id(snapshot.next_session_id);

    // Restore sessions
    std::size_t restored = 0;
    for (const auto& ss : snapshot.sessions) {
        auto session = manager.restore_session(
            ss.session_id,
            ss.state,
            ss.user_id,
            ss.authenticated,
            ss.permissions,
            ss.metadata);

        if (session) {
            ++restored;
        }
    }

    return restored;
}

/// Deserialize and restore session manager in one call
/// @return Number of sessions restored, or 0 if deserialization failed
inline std::size_t deserialize_and_restore_sessions(
    SessionManager& manager,
    const std::vector<std::uint8_t>& data) {

    auto snapshot = deserialize_session_snapshot(data);
    if (!snapshot) {
        return 0;
    }
    return restore_session_snapshot(manager, *snapshot);
}

// =============================================================================
// Event Bus Snapshot (Forward Declaration)
// =============================================================================

// Note: EventBus snapshot is limited because subscriptions contain function
// pointers and type-erased handlers that cannot be serialized. We capture
// configuration and statistics for diagnostic/logging purposes during hot-reload.
// Subscriptions must be re-established after hot-reload.

struct EventBusSnapshot {
    static constexpr std::uint32_t VERSION = 1;

    std::uint32_t version = VERSION;
    bool enabled = true;

    // Configuration
    std::size_t max_queue_size = 10000;
    bool drop_on_queue_full = true;
    bool process_immediate = false;

    // Statistics (for logging/diagnostics)
    std::uint64_t events_published = 0;
    std::uint64_t events_queued = 0;
    std::uint64_t events_processed = 0;
    std::uint64_t events_dropped = 0;
    std::size_t active_subscriptions = 0;
    std::size_t queue_size = 0;
    std::size_t max_queue_size_reached = 0;

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Serialize event bus snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_event_bus_snapshot(
    const EventBusSnapshot& snapshot) {
    BinaryWriter writer;

    writer.write_u32(snapshot.version);
    writer.write_bool(snapshot.enabled);

    // Configuration
    writer.write_u64(snapshot.max_queue_size);
    writer.write_bool(snapshot.drop_on_queue_full);
    writer.write_bool(snapshot.process_immediate);

    // Statistics
    writer.write_u64(snapshot.events_published);
    writer.write_u64(snapshot.events_queued);
    writer.write_u64(snapshot.events_processed);
    writer.write_u64(snapshot.events_dropped);
    writer.write_u64(snapshot.active_subscriptions);
    writer.write_u64(snapshot.queue_size);
    writer.write_u64(snapshot.max_queue_size_reached);

    return writer.take();
}

/// Deserialize event bus snapshot from binary
[[nodiscard]] inline std::optional<EventBusSnapshot> deserialize_event_bus_snapshot(
    const std::vector<std::uint8_t>& data) {
    if (data.size() < 4) return std::nullopt;

    BinaryReader reader(data);

    EventBusSnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    snapshot.enabled = reader.read_bool();

    // Configuration
    snapshot.max_queue_size = reader.read_u64();
    snapshot.drop_on_queue_full = reader.read_bool();
    snapshot.process_immediate = reader.read_bool();

    // Statistics
    snapshot.events_published = reader.read_u64();
    snapshot.events_queued = reader.read_u64();
    snapshot.events_processed = reader.read_u64();
    snapshot.events_dropped = reader.read_u64();
    snapshot.active_subscriptions = reader.read_u64();
    snapshot.queue_size = reader.read_u64();
    snapshot.max_queue_size_reached = reader.read_u64();

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

/// Take a snapshot of the event bus for hot-reload
/// @note Subscriptions and queued events cannot be serialized. Only config
///       and stats are captured for diagnostic purposes.
[[nodiscard]] inline EventBusSnapshot take_event_bus_snapshot(const EventBus& bus) {
    EventBusSnapshot snapshot;
    snapshot.version = EventBusSnapshot::VERSION;
    snapshot.enabled = bus.is_enabled();

    // Capture config
    const auto& config = bus.config();
    snapshot.max_queue_size = config.max_queue_size;
    snapshot.drop_on_queue_full = config.drop_on_queue_full;
    snapshot.process_immediate = config.process_immediate;

    // Capture stats
    auto stats = bus.stats();
    snapshot.events_published = stats.events_published;
    snapshot.events_queued = stats.events_queued;
    snapshot.events_processed = stats.events_processed;
    snapshot.events_dropped = stats.events_dropped;
    snapshot.active_subscriptions = stats.active_subscriptions;
    snapshot.queue_size = stats.queue_size;
    snapshot.max_queue_size = stats.max_queue_size;

    return snapshot;
}

/// Restore an event bus from a snapshot
/// @note Only the enabled state can be restored. Subscriptions must be
///       re-established by the application after hot-reload.
/// @param bus The event bus to restore into
/// @param snapshot The snapshot to restore from
/// @return true if restoration succeeded
inline bool restore_event_bus_snapshot(EventBus& bus, const EventBusSnapshot& snapshot) {
    if (!snapshot.is_compatible()) {
        return false;
    }

    // Restore enabled state
    bus.set_enabled(snapshot.enabled);

    // Note: Configuration cannot be changed after construction
    // Note: Stats are internal and automatically tracked
    // Note: Subscriptions must be re-registered by application code

    return true;
}

/// Deserialize and restore event bus in one call
/// @return true if successful
inline bool deserialize_and_restore_event_bus(
    EventBus& bus,
    const std::vector<std::uint8_t>& data) {

    auto snapshot = deserialize_event_bus_snapshot(data);
    if (!snapshot) {
        return false;
    }
    return restore_event_bus_snapshot(bus, *snapshot);
}

// =============================================================================
// Convenience Functions
// =============================================================================

/// Take and serialize registry snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_registry(
    const ServiceRegistry& registry) {
    return serialize_registry_snapshot(take_registry_snapshot(registry));
}

/// Take and serialize session manager snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_sessions(
    const SessionManager& manager) {
    return serialize_session_snapshot(take_session_snapshot(manager));
}

/// Take and serialize event bus snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_event_bus(
    const EventBus& bus) {
    return serialize_event_bus_snapshot(take_event_bus_snapshot(bus));
}

} // namespace void_services
