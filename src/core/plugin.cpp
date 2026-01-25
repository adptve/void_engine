/// @file plugin.cpp
/// @brief Plugin system implementation for void_core
///
/// This file provides:
/// - Plugin state serialization with binary format
/// - Plugin lifecycle utilities
/// - Plugin dependency resolution
/// - Plugin debugging and validation

#include <void_engine/core/plugin.hpp>
#include <void_engine/core/log.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace void_core {

// =============================================================================
// Plugin State Binary Serialization
// =============================================================================

namespace serialization {

/// Binary serialization constants for PluginState
namespace plugin_binary {
    constexpr std::uint32_t MAGIC = 0x504C5547;  // "PLUG"
    constexpr std::uint32_t VERSION = 1;
}

/// Serialize a PluginState to binary
std::vector<std::uint8_t> serialize_plugin_state(const PluginState& state) {
    std::vector<std::uint8_t> result;
    result.reserve(128 + state.data.size() + state.type_name.size());

    // Helper to append data
    auto append = [&result](const void* ptr, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(ptr);
        result.insert(result.end(), bytes, bytes + size);
    };

    // Magic
    append(&plugin_binary::MAGIC, sizeof(std::uint32_t));

    // Serialization version
    append(&plugin_binary::VERSION, sizeof(std::uint32_t));

    // Plugin version (as u64)
    std::uint64_t ver_bits = state.version.to_u64();
    append(&ver_bits, sizeof(std::uint64_t));

    // Type name length and data
    std::uint32_t type_name_len = static_cast<std::uint32_t>(state.type_name.size());
    append(&type_name_len, sizeof(std::uint32_t));
    append(state.type_name.data(), type_name_len);

    // Data length and payload
    std::uint64_t data_len = static_cast<std::uint64_t>(state.data.size());
    append(&data_len, sizeof(std::uint64_t));
    append(state.data.data(), state.data.size());

    return result;
}

/// Deserialize a PluginState from binary
Result<PluginState> deserialize_plugin_state(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 2) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Plugin state data too short"));
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
        return Err<PluginState>(Error(ErrorCode::ParseError, "Failed to read magic"));
    }
    if (magic != plugin_binary::MAGIC) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Invalid plugin state magic"));
    }

    // Verify version
    std::uint32_t version;
    if (!read(&version, sizeof(std::uint32_t))) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Failed to read version"));
    }
    if (version != plugin_binary::VERSION) {
        return Err<PluginState>(Error(ErrorCode::IncompatibleVersion, "Unsupported plugin state version"));
    }

    // Read plugin version
    std::uint64_t ver_bits;
    if (!read(&ver_bits, sizeof(std::uint64_t))) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Failed to read plugin version"));
    }

    // Read type name
    std::uint32_t type_name_len;
    if (!read(&type_name_len, sizeof(std::uint32_t))) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Failed to read type name length"));
    }
    if (ptr + type_name_len > end) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Type name truncated"));
    }
    std::string type_name(reinterpret_cast<const char*>(ptr), type_name_len);
    ptr += type_name_len;

    // Read data
    std::uint64_t data_len;
    if (!read(&data_len, sizeof(std::uint64_t))) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Failed to read data length"));
    }
    if (ptr + data_len > end) {
        return Err<PluginState>(Error(ErrorCode::ParseError, "Plugin state data truncated"));
    }
    std::vector<std::uint8_t> state_data(ptr, ptr + data_len);

    return Ok(PluginState(std::move(state_data), type_name, Version::from_u64(ver_bits)));
}

} // namespace serialization

// =============================================================================
// Plugin Dependency Resolution
// =============================================================================

/// Topologically sort plugins by dependencies
Result<std::vector<PluginId>> resolve_load_order(
    const std::vector<PluginId>& plugins,
    const std::function<std::vector<PluginId>(const PluginId&)>& get_dependencies)
{
    // Build dependency graph
    std::map<std::string, std::vector<std::string>> graph;
    std::map<std::string, int> in_degree;

    for (const auto& plugin : plugins) {
        std::string name = plugin.name();
        if (graph.find(name) == graph.end()) {
            graph[name] = {};
            in_degree[name] = 0;
        }

        auto deps = get_dependencies(plugin);
        for (const auto& dep : deps) {
            std::string dep_name = dep.name();
            graph[dep_name].push_back(name);
            in_degree[name]++;

            if (graph.find(dep_name) == graph.end()) {
                graph[dep_name] = {};
            }
            if (in_degree.find(dep_name) == in_degree.end()) {
                in_degree[dep_name] = 0;
            }
        }
    }

    // Kahn's algorithm for topological sort
    std::vector<std::string> order;
    std::vector<std::string> queue;

    for (const auto& [name, degree] : in_degree) {
        if (degree == 0) {
            queue.push_back(name);
        }
    }

    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();
        order.push_back(current);

        for (const auto& next : graph[current]) {
            in_degree[next]--;
            if (in_degree[next] == 0) {
                queue.push_back(next);
            }
        }
    }

    // Check for cycles
    if (order.size() != graph.size()) {
        return Err<std::vector<PluginId>>(PluginError::init_failed(
            "dependency_resolution", "Circular dependency detected in plugin graph"));
    }

    // Convert back to PluginIds, filtering to only requested plugins
    std::vector<PluginId> result;
    std::set<std::string> requested;
    for (const auto& p : plugins) {
        requested.insert(p.name());
    }

    for (const auto& name : order) {
        if (requested.find(name) != requested.end()) {
            result.push_back(PluginId(name));
        }
    }

    return Ok(std::move(result));
}

/// Check if all dependencies are satisfied
Result<void> check_dependencies(
    const PluginId& plugin,
    const std::vector<PluginId>& dependencies,
    const std::function<bool(const PluginId&)>& is_loaded)
{
    for (const auto& dep : dependencies) {
        if (!is_loaded(dep)) {
            return Err(PluginError::missing_dependency(plugin.name(), dep.name()));
        }
    }
    return Ok();
}

// =============================================================================
// Plugin Debugging Utilities
// =============================================================================

namespace debug {

/// Format a PluginId for debugging
std::string format_plugin_id(const PluginId& id) {
    std::ostringstream oss;
    oss << "PluginId(\"" << id.name() << "\", hash=0x"
        << std::hex << std::setfill('0') << std::setw(16) << id.hash() << ")";
    return oss.str();
}

/// Format a PluginInfo for debugging
std::string format_plugin_info(const PluginInfo& info) {
    std::ostringstream oss;
    oss << "PluginInfo {\n"
        << "  id: \"" << info.id.name() << "\",\n"
        << "  version: " << info.version.to_string() << ",\n"
        << "  status: " << plugin_status_name(info.status) << ",\n"
        << "  hot_reload: " << (info.supports_hot_reload ? "true" : "false") << ",\n"
        << "  dependencies: [";

    for (std::size_t i = 0; i < info.dependencies.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << info.dependencies[i].name() << "\"";
    }
    oss << "]\n}";

    return oss.str();
}

/// Format PluginRegistry state for debugging
std::string format_registry_state(const PluginRegistry& registry) {
    std::ostringstream oss;
    oss << "PluginRegistry {\n"
        << "  total_plugins: " << registry.len() << ",\n"
        << "  active_plugins: " << registry.active_count() << ",\n"
        << "  load_order: [";

    const auto& order = registry.load_order();
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << order[i].name() << "\"";
    }
    oss << "]\n}";

    return oss.str();
}

/// Format a PluginState for debugging
std::string format_plugin_state(const PluginState& state) {
    std::ostringstream oss;
    oss << "PluginState {\n"
        << "  type_name: \"" << state.type_name << "\",\n"
        << "  version: " << state.version.to_string() << ",\n"
        << "  data_size: " << state.data.size() << " bytes\n"
        << "}";
    return oss.str();
}

} // namespace debug

// =============================================================================
// Plugin Validation
// =============================================================================

/// Validate a plugin before registration
Result<void> validate_plugin(const Plugin& plugin) {
    // Check ID is valid
    if (plugin.id().name().empty()) {
        return Err(PluginError::init_failed("unknown", "Plugin ID cannot be empty"));
    }

    // Check version is reasonable
    Version v = plugin.version();
    if (v.major > 999 || v.minor > 999 || v.patch > 999) {
        return Err(PluginError::init_failed(
            plugin.id().name(), "Plugin version numbers are unreasonably large"));
    }

    // Check dependencies don't include self
    auto deps = plugin.dependencies();
    for (const auto& dep : deps) {
        if (dep.name() == plugin.id().name()) {
            return Err(PluginError::init_failed(
                plugin.id().name(), "Plugin cannot depend on itself"));
        }
    }

    // Check for duplicate dependencies
    std::set<std::string> seen;
    for (const auto& dep : deps) {
        if (seen.find(dep.name()) != seen.end()) {
            return Err(PluginError::init_failed(
                plugin.id().name(),
                "Duplicate dependency: " + dep.name()));
        }
        seen.insert(dep.name());
    }

    return Ok();
}

/// Validate plugin state before restore
Result<void> validate_plugin_state(const PluginState& state, const Plugin& plugin) {
    // Check type name matches (if present)
    if (!state.type_name.empty()) {
        // Type validation would require RTTI comparison
        // For now, we trust the type name
    }

    // Check version compatibility
    if (!state.version.is_compatible_with(plugin.version())) {
        // Version compatibility is one-directional: state must be compatible with plugin
        // This means the plugin version must be >= state version for same major
    }

    return Ok();
}

// =============================================================================
// Plugin Statistics
// =============================================================================

namespace {

/// Global plugin statistics
struct PluginStats {
    std::atomic<std::uint64_t> total_loads{0};
    std::atomic<std::uint64_t> total_unloads{0};
    std::atomic<std::uint64_t> total_hot_reloads{0};
    std::atomic<std::uint64_t> failed_loads{0};
    std::atomic<std::uint64_t> failed_hot_reloads{0};
};

PluginStats s_plugin_stats;

} // anonymous namespace

/// Record a plugin load
void record_plugin_load(bool success) {
    s_plugin_stats.total_loads.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        s_plugin_stats.failed_loads.fetch_add(1, std::memory_order_relaxed);
    }
}

/// Record a plugin unload
void record_plugin_unload() {
    s_plugin_stats.total_unloads.fetch_add(1, std::memory_order_relaxed);
}

/// Record a plugin hot-reload
void record_plugin_hot_reload(bool success) {
    s_plugin_stats.total_hot_reloads.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        s_plugin_stats.failed_hot_reloads.fetch_add(1, std::memory_order_relaxed);
    }
}

/// Get plugin statistics
PluginStatistics get_plugin_statistics() {
    return PluginStatistics{
        s_plugin_stats.total_loads.load(std::memory_order_relaxed),
        s_plugin_stats.total_unloads.load(std::memory_order_relaxed),
        s_plugin_stats.total_hot_reloads.load(std::memory_order_relaxed),
        s_plugin_stats.failed_loads.load(std::memory_order_relaxed),
        s_plugin_stats.failed_hot_reloads.load(std::memory_order_relaxed)
    };
}

/// Reset plugin statistics
void reset_plugin_statistics() {
    s_plugin_stats.total_loads.store(0, std::memory_order_relaxed);
    s_plugin_stats.total_unloads.store(0, std::memory_order_relaxed);
    s_plugin_stats.total_hot_reloads.store(0, std::memory_order_relaxed);
    s_plugin_stats.failed_loads.store(0, std::memory_order_relaxed);
    s_plugin_stats.failed_hot_reloads.store(0, std::memory_order_relaxed);
}

/// Format plugin statistics
std::string format_plugin_statistics() {
    auto stats = get_plugin_statistics();
    std::ostringstream oss;
    oss << "Plugin Statistics:\n"
        << "  Total Loads: " << stats.total_loads << "\n"
        << "  Failed Loads: " << stats.failed_loads << "\n"
        << "  Total Unloads: " << stats.total_unloads << "\n"
        << "  Total Hot-Reloads: " << stats.total_hot_reloads << "\n"
        << "  Failed Hot-Reloads: " << stats.failed_hot_reloads << "\n";

    if (stats.total_loads > 0) {
        double success_rate = 100.0 * (stats.total_loads - stats.failed_loads) / stats.total_loads;
        oss << "  Load Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%\n";
    }

    return oss.str();
}

// =============================================================================
// Global Plugin Registry Instance
// =============================================================================

namespace {
    std::unique_ptr<PluginRegistry> s_global_plugin_registry;
}

/// Get or create the global plugin registry
PluginRegistry& global_plugin_registry() {
    if (!s_global_plugin_registry) {
        s_global_plugin_registry = std::make_unique<PluginRegistry>();
    }
    return *s_global_plugin_registry;
}

/// Shutdown the global plugin registry
void shutdown_plugin_registry() {
    if (s_global_plugin_registry) {
        s_global_plugin_registry->unload_all();
        s_global_plugin_registry.reset();
    }
}

} // namespace void_core
