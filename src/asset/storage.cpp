/// @file storage.cpp
/// @brief void_asset storage implementation
///
/// Provides non-template utilities for the asset storage system.
/// Core storage functionality is template-based in the header.

#include <void_engine/asset/storage.hpp>

#include <algorithm>
#include <sstream>

namespace void_asset {

// =============================================================================
// Storage Statistics
// =============================================================================

namespace {

struct StorageStatistics {
    std::atomic<std::uint64_t> total_assets_stored{0};
    std::atomic<std::uint64_t> total_assets_removed{0};
    std::atomic<std::uint64_t> garbage_collections{0};
    std::atomic<std::uint64_t> assets_collected{0};
    std::atomic<std::uint64_t> total_bytes_stored{0};
};

StorageStatistics s_storage_stats;

} // anonymous namespace

void record_asset_stored(std::size_t bytes) {
    s_storage_stats.total_assets_stored.fetch_add(1, std::memory_order_relaxed);
    s_storage_stats.total_bytes_stored.fetch_add(bytes, std::memory_order_relaxed);
}

void record_asset_removed(std::size_t bytes) {
    s_storage_stats.total_assets_removed.fetch_add(1, std::memory_order_relaxed);
    if (bytes <= s_storage_stats.total_bytes_stored.load()) {
        s_storage_stats.total_bytes_stored.fetch_sub(bytes, std::memory_order_relaxed);
    }
}

void record_garbage_collection(std::size_t count) {
    s_storage_stats.garbage_collections.fetch_add(1, std::memory_order_relaxed);
    s_storage_stats.assets_collected.fetch_add(count, std::memory_order_relaxed);
}

std::string format_storage_statistics() {
    std::ostringstream oss;
    oss << "Storage Statistics:\n";
    oss << "  Total stored: " << s_storage_stats.total_assets_stored.load() << "\n";
    oss << "  Total removed: " << s_storage_stats.total_assets_removed.load() << "\n";
    oss << "  Active: " << (s_storage_stats.total_assets_stored.load() -
                           s_storage_stats.total_assets_removed.load()) << "\n";
    oss << "  GC runs: " << s_storage_stats.garbage_collections.load() << "\n";
    oss << "  Assets collected: " << s_storage_stats.assets_collected.load() << "\n";
    oss << "  Bytes stored: " << s_storage_stats.total_bytes_stored.load() << "\n";
    return oss.str();
}

void reset_storage_statistics() {
    s_storage_stats.total_assets_stored.store(0);
    s_storage_stats.total_assets_removed.store(0);
    s_storage_stats.garbage_collections.store(0);
    s_storage_stats.assets_collected.store(0);
    s_storage_stats.total_bytes_stored.store(0);
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_asset_entry(const AssetEntry& entry) {
    std::ostringstream oss;
    oss << "AssetEntry {\n";
    oss << "  has_asset: " << (entry.asset != nullptr ? "true" : "false") << "\n";
    oss << "  type: " << entry.type_id.name() << "\n";
    oss << "  has_deleter: " << (entry.deleter ? "true" : "false") << "\n";
    if (entry.handle_data) {
        oss << "  handle_data:\n";
        oss << "    strong_count: " << entry.handle_data->use_count() << "\n";
        oss << "    generation: " << entry.handle_data->get_generation() << "\n";
        oss << "    state: " << load_state_name(entry.handle_data->get_state()) << "\n";
    }
    oss << "  metadata:\n";
    oss << "    id: " << entry.metadata.id.raw() << "\n";
    oss << "    path: \"" << entry.metadata.path.str() << "\"\n";
    oss << "    state: " << load_state_name(entry.metadata.state) << "\n";
    oss << "}";
    return oss.str();
}

std::string format_asset_storage(const AssetStorage& storage) {
    std::ostringstream oss;
    oss << "AssetStorage {\n";
    oss << "  total_count: " << storage.len() << "\n";
    oss << "  loaded_count: " << storage.loaded_count() << "\n";
    oss << "}\n";
    oss << "Assets:\n";

    storage.for_each([&oss](AssetId id, const AssetMetadata& meta) {
        oss << "  [" << id.raw() << "] \"" << meta.path.str() << "\" - "
            << load_state_name(meta.state) << "\n";
    });

    return oss.str();
}

} // namespace debug

// =============================================================================
// Storage Validation
// =============================================================================

std::vector<std::string> validate_storage(const AssetStorage& storage) {
    std::vector<std::string> errors;

    storage.for_each([&errors](AssetId id, const AssetMetadata& meta) {
        // Check for orphaned metadata
        if (!id.is_valid()) {
            errors.push_back("Invalid asset ID found in storage");
        }

        // Check for empty paths
        if (meta.path.str().empty()) {
            std::ostringstream oss;
            oss << "Asset " << id.raw() << " has empty path";
            errors.push_back(oss.str());
        }

        // Check for loaded assets with zero size
        if (meta.is_loaded() && meta.size_bytes == 0) {
            std::ostringstream oss;
            oss << "Asset " << id.raw() << " (" << meta.path.str()
                << ") is loaded but has zero size";
            errors.push_back(oss.str());
        }

        // Check for failed assets without error message
        if (meta.is_failed() && meta.error_message.empty()) {
            std::ostringstream oss;
            oss << "Asset " << id.raw() << " (" << meta.path.str()
                << ") failed without error message";
            errors.push_back(oss.str());
        }
    });

    return errors;
}

// =============================================================================
// Dependency Graph Utilities
// =============================================================================

std::vector<AssetId> get_all_dependents(const AssetStorage& storage, AssetId id) {
    std::vector<AssetId> result;
    std::vector<AssetId> to_process;
    std::set<std::uint64_t> visited;

    to_process.push_back(id);
    visited.insert(id.raw());

    while (!to_process.empty()) {
        AssetId current = to_process.back();
        to_process.pop_back();

        const auto* meta = storage.get_metadata(current);
        if (!meta) continue;

        for (const auto& dependent_id : meta->dependents) {
            if (visited.find(dependent_id.raw()) == visited.end()) {
                visited.insert(dependent_id.raw());
                result.push_back(dependent_id);
                to_process.push_back(dependent_id);
            }
        }
    }

    return result;
}

std::vector<AssetId> get_all_dependencies(const AssetStorage& storage, AssetId id) {
    std::vector<AssetId> result;
    std::vector<AssetId> to_process;
    std::set<std::uint64_t> visited;

    to_process.push_back(id);
    visited.insert(id.raw());

    while (!to_process.empty()) {
        AssetId current = to_process.back();
        to_process.pop_back();

        const auto* meta = storage.get_metadata(current);
        if (!meta) continue;

        for (const auto& dep_id : meta->dependencies) {
            if (visited.find(dep_id.raw()) == visited.end()) {
                visited.insert(dep_id.raw());
                result.push_back(dep_id);
                to_process.push_back(dep_id);
            }
        }
    }

    return result;
}

bool has_circular_dependency(const AssetStorage& storage, AssetId id) {
    std::vector<AssetId> to_process;
    std::set<std::uint64_t> visited;
    std::set<std::uint64_t> in_stack;

    to_process.push_back(id);

    while (!to_process.empty()) {
        AssetId current = to_process.back();

        if (in_stack.find(current.raw()) != in_stack.end()) {
            return true; // Circular dependency detected
        }

        if (visited.find(current.raw()) != visited.end()) {
            to_process.pop_back();
            continue;
        }

        visited.insert(current.raw());
        in_stack.insert(current.raw());

        const auto* meta = storage.get_metadata(current);
        if (meta) {
            for (const auto& dep_id : meta->dependencies) {
                to_process.push_back(dep_id);
            }
        }

        in_stack.erase(current.raw());
        to_process.pop_back();
    }

    return false;
}

// =============================================================================
// Serialization for Hot-Reload Manifest
// =============================================================================

namespace serialization {

std::vector<std::uint8_t> serialize_storage_manifest(const AssetStorage& storage) {
    std::vector<std::uint8_t> data;

    // Format: count (8 bytes), then for each asset:
    // - id (8 bytes)
    // - path_len (4 bytes)
    // - path (path_len bytes)
    // - state (1 byte)
    // - generation (4 bytes)

    std::size_t count = storage.len();

    // Write count
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<std::uint8_t>((count >> (i * 8)) & 0xFF));
    }

    storage.for_each([&data](AssetId id, const AssetMetadata& meta) {
        // Write id
        std::uint64_t raw_id = id.raw();
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<std::uint8_t>((raw_id >> (i * 8)) & 0xFF));
        }

        // Write path length
        std::uint32_t path_len = static_cast<std::uint32_t>(meta.path.str().size());
        for (int i = 0; i < 4; ++i) {
            data.push_back(static_cast<std::uint8_t>((path_len >> (i * 8)) & 0xFF));
        }

        // Write path
        for (char c : meta.path.str()) {
            data.push_back(static_cast<std::uint8_t>(c));
        }

        // Write state
        data.push_back(static_cast<std::uint8_t>(meta.state));

        // Write generation
        for (int i = 0; i < 4; ++i) {
            data.push_back(static_cast<std::uint8_t>((meta.generation >> (i * 8)) & 0xFF));
        }
    });

    return data;
}

void_core::Result<std::vector<std::pair<AssetId, AssetPath>>> deserialize_storage_manifest(
    const std::vector<std::uint8_t>& data)
{
    std::vector<std::pair<AssetId, AssetPath>> result;

    if (data.size() < 8) {
        return void_core::Err<std::vector<std::pair<AssetId, AssetPath>>>("Invalid manifest data");
    }

    // Read count
    std::size_t count = 0;
    for (int i = 0; i < 8; ++i) {
        count |= static_cast<std::size_t>(data[i]) << (i * 8);
    }

    std::size_t offset = 8;

    for (std::size_t i = 0; i < count; ++i) {
        if (offset + 8 + 4 > data.size()) {
            return void_core::Err<std::vector<std::pair<AssetId, AssetPath>>>(
                "Truncated manifest data");
        }

        // Read id
        std::uint64_t raw_id = 0;
        for (int j = 0; j < 8; ++j) {
            raw_id |= static_cast<std::uint64_t>(data[offset + j]) << (j * 8);
        }
        offset += 8;

        // Read path length
        std::uint32_t path_len = 0;
        for (int j = 0; j < 4; ++j) {
            path_len |= static_cast<std::uint32_t>(data[offset + j]) << (j * 8);
        }
        offset += 4;

        if (offset + path_len + 5 > data.size()) {
            return void_core::Err<std::vector<std::pair<AssetId, AssetPath>>>(
                "Truncated manifest path data");
        }

        // Read path
        std::string path_str(data.begin() + offset, data.begin() + offset + path_len);
        offset += path_len;

        // Skip state (1 byte) and generation (4 bytes)
        offset += 5;

        result.emplace_back(AssetId{raw_id}, AssetPath{path_str});
    }

    return void_core::Ok(std::move(result));
}

} // namespace serialization

} // namespace void_asset
