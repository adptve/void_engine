/// @file cache.cpp
/// @brief void_asset cache implementation
///
/// Provides non-template implementations for the tiered cache system.
/// Core cache functionality is header-only, this file adds utilities.

#include <void_engine/asset/cache.hpp>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace void_asset {

// =============================================================================
// Cache Statistics Formatting
// =============================================================================

std::string format_cache_stats(const CacheStats& stats) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Cache Statistics:\n";
    oss << "  Hot Cache:\n";
    oss << "    Entries: " << stats.hot_entries << "\n";
    oss << "    Size: " << (stats.hot_size_bytes / 1024.0 / 1024.0) << " MB\n";
    oss << "    Capacity: " << (stats.hot_capacity_bytes / 1024.0 / 1024.0) << " MB\n";
    oss << "    Utilization: " << (stats.hot_capacity_bytes > 0 ?
        (100.0 * stats.hot_size_bytes / stats.hot_capacity_bytes) : 0.0) << "%\n";
    oss << "  Warm Cache:\n";
    oss << "    Entries: " << stats.warm_entries << "\n";
    oss << "    Size: " << (stats.warm_size_bytes / 1024.0 / 1024.0) << " MB\n";
    oss << "  Performance:\n";
    oss << "    Hits: " << stats.hits << "\n";
    oss << "    Misses: " << stats.misses << "\n";
    oss << "    Hit Rate: " << (stats.hit_rate() * 100.0) << "%\n";
    oss << "    Evictions: " << stats.evictions << "\n";
    oss << "  I/O:\n";
    oss << "    Disk Reads: " << stats.disk_reads << "\n";
    oss << "    Disk Writes: " << stats.disk_writes << "\n";
    return oss.str();
}

// =============================================================================
// Content Hashing
// =============================================================================

namespace {

// Simple FNV-1a hash for content
std::uint64_t fnv1a_hash(const std::uint8_t* data, std::size_t len) {
    constexpr std::uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
    constexpr std::uint64_t FNV_PRIME = 0x100000001b3ULL;

    std::uint64_t hash = FNV_OFFSET_BASIS;
    for (std::size_t i = 0; i < len; ++i) {
        hash ^= static_cast<std::uint64_t>(data[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

} // anonymous namespace

std::string compute_content_hash(const std::vector<std::uint8_t>& data) {
    std::uint64_t hash = fnv1a_hash(data.data(), data.size());

    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

std::string compute_content_hash(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = fnv1a_hash(data, size);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

// =============================================================================
// Cache Entry Utilities
// =============================================================================

CacheEntry create_cache_entry(
    std::vector<std::uint8_t> data,
    CachePriority priority,
    const std::string& asset_type,
    const std::string& source_url)
{
    CacheEntry entry;
    entry.data = std::move(data);
    entry.meta.size_bytes = entry.data.size();
    entry.meta.content_hash = compute_content_hash(entry.data);
    entry.meta.priority = priority;
    entry.meta.asset_type = asset_type;
    entry.meta.source_url = source_url;
    entry.meta.cached_at = std::chrono::steady_clock::now();
    entry.meta.last_access = entry.meta.cached_at;
    entry.meta.access_count = 1;
    return entry;
}

// =============================================================================
// Validation Utilities
// =============================================================================

std::string validation_result_name(ValidationResult result) {
    switch (result) {
        case ValidationResult::Valid: return "Valid";
        case ValidationResult::Stale: return "Stale";
        case ValidationResult::Invalid: return "Invalid";
        case ValidationResult::NotFound: return "NotFound";
        default: return "Unknown";
    }
}

std::string cache_priority_name(CachePriority priority) {
    switch (priority) {
        case CachePriority::Low: return "Low";
        case CachePriority::Normal: return "Normal";
        case CachePriority::High: return "High";
        case CachePriority::Essential: return "Essential";
        default: return "Unknown";
    }
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_cache_entry_meta(const CacheEntryMeta& meta) {
    std::ostringstream oss;
    oss << "CacheEntryMeta {\n";
    oss << "  content_hash: \"" << meta.content_hash << "\"\n";
    oss << "  size_bytes: " << meta.size_bytes << "\n";
    oss << "  priority: " << cache_priority_name(meta.priority) << "\n";
    oss << "  asset_type: \"" << meta.asset_type << "\"\n";
    if (!meta.source_url.empty()) {
        oss << "  source_url: \"" << meta.source_url << "\"\n";
    }
    if (!meta.etag.empty()) {
        oss << "  etag: \"" << meta.etag << "\"\n";
    }
    oss << "  access_count: " << meta.access_count << "\n";
    oss << "}";
    return oss.str();
}

std::string format_tiered_cache_config(const TieredCacheConfig& config) {
    std::ostringstream oss;
    oss << "TieredCacheConfig {\n";
    oss << "  hot_cache_bytes: " << (config.hot_cache_bytes / 1024.0 / 1024.0) << " MB\n";
    oss << "  disk_cache_dir: \"" << config.disk_cache_dir.string() << "\"\n";
    oss << "  enable_disk_cache: " << (config.enable_disk_cache ? "true" : "false") << "\n";
    oss << "  auto_promote: " << (config.auto_promote ? "true" : "false") << "\n";
    oss << "}";
    return oss.str();
}

} // namespace debug

// =============================================================================
// Compression Utilities (for Warm Cache)
// =============================================================================

namespace compression {

// Simple RLE compression for demonstration
// In production, use LZ4 or similar
std::vector<std::uint8_t> compress_rle(const std::vector<std::uint8_t>& data) {
    if (data.empty()) {
        return {};
    }

    std::vector<std::uint8_t> compressed;
    compressed.reserve(data.size());

    std::size_t i = 0;
    while (i < data.size()) {
        std::uint8_t current = data[i];
        std::uint8_t run_length = 1;

        while (i + run_length < data.size() &&
               data[i + run_length] == current &&
               run_length < 255) {
            ++run_length;
        }

        if (run_length >= 4 || current == 0xFF) {
            // Use escape sequence: 0xFF, count, byte
            compressed.push_back(0xFF);
            compressed.push_back(run_length);
            compressed.push_back(current);
        } else {
            // Output literals
            for (std::uint8_t j = 0; j < run_length; ++j) {
                compressed.push_back(current);
            }
        }

        i += run_length;
    }

    // Only use compressed if smaller
    if (compressed.size() < data.size()) {
        return compressed;
    }
    return data;
}

std::vector<std::uint8_t> decompress_rle(const std::vector<std::uint8_t>& compressed) {
    if (compressed.empty()) {
        return {};
    }

    std::vector<std::uint8_t> decompressed;
    decompressed.reserve(compressed.size() * 2);

    std::size_t i = 0;
    while (i < compressed.size()) {
        if (compressed[i] == 0xFF && i + 2 < compressed.size()) {
            std::uint8_t run_length = compressed[i + 1];
            std::uint8_t value = compressed[i + 2];
            for (std::uint8_t j = 0; j < run_length; ++j) {
                decompressed.push_back(value);
            }
            i += 3;
        } else {
            decompressed.push_back(compressed[i]);
            ++i;
        }
    }

    return decompressed;
}

} // namespace compression

// =============================================================================
// Cache Key Utilities
// =============================================================================

std::string make_cache_key(const std::string& path, const std::string& variant) {
    if (variant.empty()) {
        return path;
    }
    return path + "#" + variant;
}

std::pair<std::string, std::string> parse_cache_key(const std::string& key) {
    auto pos = key.rfind('#');
    if (pos == std::string::npos) {
        return {key, ""};
    }
    return {key.substr(0, pos), key.substr(pos + 1)};
}

} // namespace void_asset
