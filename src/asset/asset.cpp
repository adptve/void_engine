/// @file asset.cpp
/// @brief void_asset module compilation unit
///
/// Primary compilation unit for the void_asset module.
/// Most functionality is header-only (templates), but this file
/// provides explicit instantiations and any non-template utilities.

#include <void_engine/asset/asset.hpp>

#include <algorithm>
#include <sstream>

namespace void_asset {

// =============================================================================
// Module Initialization
// =============================================================================

namespace {
    std::atomic<bool> s_initialized{false};
}

bool init_asset_system() {
    bool expected = false;
    return s_initialized.compare_exchange_strong(expected, true);
}

void shutdown_asset_system() {
    s_initialized.store(false);
}

bool is_asset_system_initialized() {
    return s_initialized.load();
}

// =============================================================================
// Path Utilities
// =============================================================================

std::string normalize_asset_path(const std::string& path) {
    std::string result = path;

    // Convert backslashes to forward slashes
    for (char& c : result) {
        if (c == '\\') c = '/';
    }

    // Remove duplicate slashes
    std::string cleaned;
    cleaned.reserve(result.size());
    bool last_was_slash = false;
    for (char c : result) {
        if (c == '/') {
            if (!last_was_slash) {
                cleaned.push_back(c);
            }
            last_was_slash = true;
        } else {
            cleaned.push_back(c);
            last_was_slash = false;
        }
    }

    // Remove trailing slash
    while (!cleaned.empty() && cleaned.back() == '/') {
        cleaned.pop_back();
    }

    return cleaned;
}

std::string get_asset_extension(const std::string& path) {
    auto pos = path.rfind('.');
    if (pos == std::string::npos || pos == 0) {
        return "";
    }
    // Check there's no slash after the dot
    auto slash_pos = path.rfind('/');
    if (slash_pos != std::string::npos && slash_pos > pos) {
        return "";
    }
    return path.substr(pos + 1);
}

std::string get_asset_filename(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string get_asset_directory(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

std::string join_asset_paths(const std::string& base, const std::string& relative) {
    if (base.empty()) {
        return normalize_asset_path(relative);
    }
    if (relative.empty()) {
        return normalize_asset_path(base);
    }

    std::string result = base;
    if (result.back() != '/') {
        result += '/';
    }

    // Skip leading slash on relative path
    std::size_t start = 0;
    while (start < relative.size() && relative[start] == '/') {
        ++start;
    }
    result += relative.substr(start);

    return normalize_asset_path(result);
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_asset_id(AssetId id) {
    if (!id.is_valid()) {
        return "AssetId(invalid)";
    }
    std::ostringstream oss;
    oss << "AssetId(" << id.raw() << ")";
    return oss.str();
}

std::string format_asset_path(const AssetPath& path) {
    return "AssetPath(\"" + path.str() + "\")";
}

std::string format_asset_metadata(const AssetMetadata& meta) {
    std::ostringstream oss;
    oss << "AssetMetadata {\n";
    oss << "  id: " << meta.id.raw() << "\n";
    oss << "  path: \"" << meta.path.str() << "\"\n";
    oss << "  state: " << load_state_name(meta.state) << "\n";
    oss << "  generation: " << meta.generation << "\n";
    oss << "  size_bytes: " << meta.size_bytes << "\n";
    oss << "  dependencies: " << meta.dependencies.size() << "\n";
    oss << "  dependents: " << meta.dependents.size() << "\n";
    if (!meta.error_message.empty()) {
        oss << "  error: \"" << meta.error_message << "\"\n";
    }
    oss << "}";
    return oss.str();
}

std::string format_asset_event(const AssetEvent& event) {
    std::ostringstream oss;
    oss << "AssetEvent {\n";
    oss << "  type: " << asset_event_type_name(event.type) << "\n";
    oss << "  id: " << event.id.raw() << "\n";
    oss << "  path: \"" << event.path.str() << "\"\n";
    if (!event.error.empty()) {
        oss << "  error: \"" << event.error << "\"\n";
    }
    if (event.generation > 0) {
        oss << "  generation: " << event.generation << "\n";
    }
    oss << "}";
    return oss.str();
}

std::string format_load_state(LoadState state) {
    return load_state_name(state);
}

} // namespace debug

// =============================================================================
// Statistics
// =============================================================================

namespace {
    struct AssetStatistics {
        std::atomic<std::uint64_t> total_loads{0};
        std::atomic<std::uint64_t> successful_loads{0};
        std::atomic<std::uint64_t> failed_loads{0};
        std::atomic<std::uint64_t> total_unloads{0};
        std::atomic<std::uint64_t> total_reloads{0};
        std::atomic<std::uint64_t> total_bytes_loaded{0};
    };

    AssetStatistics s_stats;
}

void record_asset_load(bool success, std::size_t bytes) {
    s_stats.total_loads.fetch_add(1, std::memory_order_relaxed);
    if (success) {
        s_stats.successful_loads.fetch_add(1, std::memory_order_relaxed);
        s_stats.total_bytes_loaded.fetch_add(bytes, std::memory_order_relaxed);
    } else {
        s_stats.failed_loads.fetch_add(1, std::memory_order_relaxed);
    }
}

void record_asset_unload() {
    s_stats.total_unloads.fetch_add(1, std::memory_order_relaxed);
}

void record_asset_reload() {
    s_stats.total_reloads.fetch_add(1, std::memory_order_relaxed);
}

std::string format_asset_statistics() {
    std::ostringstream oss;
    oss << "Asset Statistics:\n";
    oss << "  Total loads: " << s_stats.total_loads.load() << "\n";
    oss << "  Successful: " << s_stats.successful_loads.load() << "\n";
    oss << "  Failed: " << s_stats.failed_loads.load() << "\n";
    oss << "  Unloads: " << s_stats.total_unloads.load() << "\n";
    oss << "  Reloads: " << s_stats.total_reloads.load() << "\n";
    oss << "  Bytes loaded: " << s_stats.total_bytes_loaded.load() << "\n";
    return oss.str();
}

void reset_asset_statistics() {
    s_stats.total_loads.store(0);
    s_stats.successful_loads.store(0);
    s_stats.failed_loads.store(0);
    s_stats.total_unloads.store(0);
    s_stats.total_reloads.store(0);
    s_stats.total_bytes_loaded.store(0);
}

} // namespace void_asset
