/// @file hot_reload.cpp
/// @brief Hot-reload system implementation for void_core
///
/// This file provides:
/// - Hot-reload snapshot serialization with binary format
/// - HotReloadManager utilities and debugging
/// - File watcher integration support
/// - Hot-reload validation and verification

#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/log.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace void_core {

// =============================================================================
// Snapshot Binary Serialization
// =============================================================================

namespace serialization {

/// Binary serialization constants for HotReloadSnapshot
namespace snapshot_binary {
    constexpr std::uint32_t MAGIC = 0x484F5453;  // "HOTS"
    constexpr std::uint32_t VERSION = 1;
}

/// Serialize a HotReloadSnapshot to binary
std::vector<std::uint8_t> serialize_snapshot(const HotReloadSnapshot& snapshot) {
    std::vector<std::uint8_t> result;
    result.reserve(256 + snapshot.data.size() + snapshot.type_name.size());

    // Helper to append data
    auto append = [&result](const void* ptr, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(ptr);
        result.insert(result.end(), bytes, bytes + size);
    };

    // Magic
    append(&snapshot_binary::MAGIC, sizeof(std::uint32_t));

    // Version
    append(&snapshot_binary::VERSION, sizeof(std::uint32_t));

    // Snapshot version (as u64)
    std::uint64_t ver_bits = snapshot.version.to_u64();
    append(&ver_bits, sizeof(std::uint64_t));

    // Type name length and data
    std::uint32_t type_name_len = static_cast<std::uint32_t>(snapshot.type_name.size());
    append(&type_name_len, sizeof(std::uint32_t));
    append(snapshot.type_name.data(), type_name_len);

    // Data length and payload
    std::uint64_t data_len = static_cast<std::uint64_t>(snapshot.data.size());
    append(&data_len, sizeof(std::uint64_t));
    append(snapshot.data.data(), snapshot.data.size());

    // Metadata count
    std::uint32_t meta_count = static_cast<std::uint32_t>(snapshot.metadata.size());
    append(&meta_count, sizeof(std::uint32_t));

    // Metadata entries
    for (const auto& [key, value] : snapshot.metadata) {
        std::uint32_t key_len = static_cast<std::uint32_t>(key.size());
        std::uint32_t val_len = static_cast<std::uint32_t>(value.size());
        append(&key_len, sizeof(std::uint32_t));
        append(key.data(), key_len);
        append(&val_len, sizeof(std::uint32_t));
        append(value.data(), val_len);
    }

    return result;
}

/// Deserialize a HotReloadSnapshot from binary
Result<HotReloadSnapshot> deserialize_snapshot(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 2) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Snapshot data too short"));
    }

    const auto* ptr = data.data();
    const auto* end = data.data() + data.size();

    // Helper to read and advance
    auto read = [&ptr, end](void* dest, std::size_t size) -> bool {
        if (ptr + size > end) return false;
        std::memcpy(dest, ptr, size);
        ptr += size;
        return true;
    };

    // Verify magic
    std::uint32_t magic;
    if (!read(&magic, sizeof(std::uint32_t))) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read magic"));
    }
    if (magic != snapshot_binary::MAGIC) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Invalid snapshot magic"));
    }

    // Verify version
    std::uint32_t version;
    if (!read(&version, sizeof(std::uint32_t))) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read version"));
    }
    if (version != snapshot_binary::VERSION) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::IncompatibleVersion, "Unsupported snapshot version"));
    }

    // Read snapshot version
    std::uint64_t ver_bits;
    if (!read(&ver_bits, sizeof(std::uint64_t))) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read snapshot version"));
    }

    // Read type name
    std::uint32_t type_name_len;
    if (!read(&type_name_len, sizeof(std::uint32_t))) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read type name length"));
    }
    if (ptr + type_name_len > end) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Type name truncated"));
    }
    std::string type_name(reinterpret_cast<const char*>(ptr), type_name_len);
    ptr += type_name_len;

    // Read data
    std::uint64_t data_len;
    if (!read(&data_len, sizeof(std::uint64_t))) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read data length"));
    }
    if (ptr + data_len > end) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Snapshot data truncated"));
    }
    std::vector<std::uint8_t> snapshot_data(ptr, ptr + data_len);
    ptr += data_len;

    // Read metadata
    std::uint32_t meta_count;
    if (!read(&meta_count, sizeof(std::uint32_t))) {
        return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read metadata count"));
    }

    std::map<std::string, std::string> metadata;
    for (std::uint32_t i = 0; i < meta_count; ++i) {
        std::uint32_t key_len, val_len;
        if (!read(&key_len, sizeof(std::uint32_t))) {
            return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read metadata key length"));
        }
        if (ptr + key_len > end) {
            return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Metadata key truncated"));
        }
        std::string key(reinterpret_cast<const char*>(ptr), key_len);
        ptr += key_len;

        if (!read(&val_len, sizeof(std::uint32_t))) {
            return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Failed to read metadata value length"));
        }
        if (ptr + val_len > end) {
            return Err<HotReloadSnapshot>(Error(ErrorCode::ParseError, "Metadata value truncated"));
        }
        std::string value(reinterpret_cast<const char*>(ptr), val_len);
        ptr += val_len;

        metadata[std::move(key)] = std::move(value);
    }

    // Construct result
    HotReloadSnapshot result;
    result.data = std::move(snapshot_data);
    result.type_id = std::type_index(typeid(void));  // Placeholder - actual type unknown
    result.type_name = std::move(type_name);
    result.version = Version::from_u64(ver_bits);
    result.metadata = std::move(metadata);

    return Ok(std::move(result));
}

} // namespace serialization

// =============================================================================
// Hot-Reload Debugging Utilities
// =============================================================================

namespace debug {

/// Format a HotReloadSnapshot for debugging
std::string format_snapshot(const HotReloadSnapshot& snapshot) {
    std::ostringstream oss;
    oss << "HotReloadSnapshot {\n"
        << "  type_name: \"" << snapshot.type_name << "\",\n"
        << "  version: " << snapshot.version.to_string() << ",\n"
        << "  data_size: " << snapshot.data.size() << " bytes,\n"
        << "  metadata: {\n";

    for (const auto& [key, value] : snapshot.metadata) {
        oss << "    \"" << key << "\": \"" << value << "\",\n";
    }
    oss << "  }\n}";

    return oss.str();
}

/// Format a ReloadEvent for debugging
std::string format_reload_event(const ReloadEvent& event) {
    std::ostringstream oss;
    oss << "ReloadEvent {\n"
        << "  type: " << reload_event_type_name(event.type) << ",\n"
        << "  path: \"" << event.path << "\",\n";

    if (!event.old_path.empty()) {
        oss << "  old_path: \"" << event.old_path << "\",\n";
    }
    oss << "}";

    return oss.str();
}

/// Format HotReloadManager state for debugging
std::string format_manager_state(const HotReloadManager& manager) {
    std::ostringstream oss;
    oss << "HotReloadManager {\n"
        << "  registered_objects: " << manager.len() << ",\n"
        << "  pending_events: " << manager.pending_count() << ",\n"
        << "  objects: [\n";

    manager.for_each([&oss](const std::string& name, const HotReloadable& obj) {
        oss << "    \"" << name << "\" (v" << obj.current_version().to_string() << "),\n";
    });

    oss << "  ]\n}";

    return oss.str();
}

} // namespace debug

// =============================================================================
// Hot-Reload Validation
// =============================================================================

/// Validate a snapshot before restore
Result<void> validate_snapshot_for_restore(
    const HotReloadSnapshot& snapshot,
    const HotReloadable& target)
{
    // Check type compatibility
    if (snapshot.type_name != target.type_name()) {
        return Err(HotReloadError::restore_failed(
            "Type mismatch: snapshot is " + snapshot.type_name +
            ", target is " + target.type_name()));
    }

    // Check version compatibility
    if (!target.is_compatible(snapshot.version)) {
        return Err(HotReloadError::incompatible_version(
            snapshot.version.to_string(),
            target.current_version().to_string()));
    }

    // Validate data is not empty (unless snapshot was explicitly empty)
    if (snapshot.data.empty() && !snapshot.is_empty()) {
        return Err(HotReloadError::restore_failed("Snapshot data is empty"));
    }

    return Ok();
}

/// Compute a checksum for snapshot data
std::uint32_t compute_snapshot_checksum(const HotReloadSnapshot& snapshot) {
    // Simple FNV-1a checksum
    std::uint32_t hash = 0x811c9dc5;
    for (std::uint8_t byte : snapshot.data) {
        hash ^= byte;
        hash *= 0x01000193;
    }
    return hash;
}

// =============================================================================
// Hot-Reload Statistics
// =============================================================================

namespace {

/// Global hot-reload statistics
struct HotReloadStats {
    std::atomic<std::uint64_t> total_reloads{0};
    std::atomic<std::uint64_t> successful_reloads{0};
    std::atomic<std::uint64_t> failed_reloads{0};
    std::atomic<std::uint64_t> total_snapshot_bytes{0};
    std::atomic<std::uint64_t> total_restore_bytes{0};
};

HotReloadStats s_stats;

} // anonymous namespace

/// Record a successful reload
void record_reload_success(std::size_t snapshot_bytes) {
    s_stats.total_reloads.fetch_add(1, std::memory_order_relaxed);
    s_stats.successful_reloads.fetch_add(1, std::memory_order_relaxed);
    s_stats.total_snapshot_bytes.fetch_add(snapshot_bytes, std::memory_order_relaxed);
}

/// Record a failed reload
void record_reload_failure() {
    s_stats.total_reloads.fetch_add(1, std::memory_order_relaxed);
    s_stats.failed_reloads.fetch_add(1, std::memory_order_relaxed);
}

/// Get hot-reload statistics
HotReloadStatistics get_hot_reload_statistics() {
    return HotReloadStatistics{
        s_stats.total_reloads.load(std::memory_order_relaxed),
        s_stats.successful_reloads.load(std::memory_order_relaxed),
        s_stats.failed_reloads.load(std::memory_order_relaxed),
        s_stats.total_snapshot_bytes.load(std::memory_order_relaxed),
        s_stats.total_restore_bytes.load(std::memory_order_relaxed)
    };
}

/// Reset hot-reload statistics
void reset_hot_reload_statistics() {
    s_stats.total_reloads.store(0, std::memory_order_relaxed);
    s_stats.successful_reloads.store(0, std::memory_order_relaxed);
    s_stats.failed_reloads.store(0, std::memory_order_relaxed);
    s_stats.total_snapshot_bytes.store(0, std::memory_order_relaxed);
    s_stats.total_restore_bytes.store(0, std::memory_order_relaxed);
}

/// Format hot-reload statistics
std::string format_hot_reload_statistics() {
    auto stats = get_hot_reload_statistics();
    std::ostringstream oss;
    oss << "Hot-Reload Statistics:\n"
        << "  Total Reloads: " << stats.total_reloads << "\n"
        << "  Successful: " << stats.successful_reloads << "\n"
        << "  Failed: " << stats.failed_reloads << "\n"
        << "  Success Rate: ";

    if (stats.total_reloads > 0) {
        double rate = 100.0 * stats.successful_reloads / stats.total_reloads;
        oss << std::fixed << std::setprecision(1) << rate << "%\n";
    } else {
        oss << "N/A\n";
    }

    oss << "  Total Snapshot Data: " << stats.total_snapshot_bytes << " bytes\n"
        << "  Total Restore Data: " << stats.total_restore_bytes << " bytes\n";

    return oss.str();
}

// =============================================================================
// File Watcher Utilities
// =============================================================================

/// Filter reload events by file extension
std::vector<ReloadEvent> filter_events_by_extension(
    const std::vector<ReloadEvent>& events,
    const std::vector<std::string>& extensions)
{
    std::vector<ReloadEvent> filtered;

    for (const auto& event : events) {
        for (const auto& ext : extensions) {
            if (event.path.size() >= ext.size() &&
                event.path.compare(event.path.size() - ext.size(), ext.size(), ext) == 0) {
                filtered.push_back(event);
                break;
            }
        }
    }

    return filtered;
}

/// Debounce reload events (remove duplicates within time window)
std::vector<ReloadEvent> debounce_events(
    const std::vector<ReloadEvent>& events,
    std::chrono::milliseconds window)
{
    if (events.empty()) {
        return events;
    }

    std::vector<ReloadEvent> debounced;
    std::map<std::string, ReloadEvent> latest_by_path;

    for (const auto& event : events) {
        auto it = latest_by_path.find(event.path);
        if (it == latest_by_path.end()) {
            latest_by_path[event.path] = event;
        } else {
            auto delta = event.timestamp - it->second.timestamp;
            if (delta > window) {
                // Outside window, keep previous and add new
                debounced.push_back(it->second);
                it->second = event;
            } else {
                // Within window, update to latest
                it->second = event;
            }
        }
    }

    // Add remaining events
    for (auto& [path, event] : latest_by_path) {
        debounced.push_back(std::move(event));
    }

    return debounced;
}

// =============================================================================
// Global Hot-Reload System Instance
// =============================================================================

namespace {
    std::unique_ptr<HotReloadSystem> s_global_hot_reload_system;
}

/// Get or create the global hot-reload system
HotReloadSystem& global_hot_reload_system() {
    if (!s_global_hot_reload_system) {
        s_global_hot_reload_system = std::make_unique<HotReloadSystem>();
    }
    return *s_global_hot_reload_system;
}

/// Shutdown the global hot-reload system
void shutdown_hot_reload_system() {
    if (s_global_hot_reload_system) {
        s_global_hot_reload_system->stop_watching();
        s_global_hot_reload_system.reset();
    }
}

} // namespace void_core
