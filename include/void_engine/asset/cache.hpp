#pragma once

/// @file cache.hpp
/// @brief Tiered asset cache with LRU eviction and disk persistence
///
/// Three-tier caching system:
/// - Tier 1 (Hot): In-memory LRU with priority hints
/// - Tier 2 (Warm): Disk-based persistent cache
/// - Tier 3 (Cold): Remote fetch on demand
///
/// Features:
/// - Priority-based eviction (essential assets stay longer)
/// - Content-addressable disk storage (hash-based)
/// - ETag/Last-Modified validation
/// - TTL support for forced revalidation
/// - Prefetch hints for predictive loading

#include <void_engine/core/id.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_asset {

// =============================================================================
// Cache Types
// =============================================================================

/// Asset priority for cache eviction decisions
enum class CachePriority : std::uint8_t {
    Low = 0,        ///< Optional/decorative assets, evict first
    Normal = 1,     ///< Standard assets
    High = 2,       ///< Important assets (materials, common textures)
    Essential = 3,  ///< Critical assets (shaders, core UI), evict last
};

/// Cache entry metadata
struct CacheEntryMeta {
    /// Content hash (SHA-256 or similar)
    std::string content_hash;
    /// ETag from server (for HTTP cache validation)
    std::string etag;
    /// Last-Modified timestamp from server
    std::string last_modified;
    /// Asset size in bytes
    std::size_t size_bytes = 0;
    /// Cache priority
    CachePriority priority = CachePriority::Normal;
    /// Time-to-live (0 = infinite)
    std::chrono::seconds ttl{0};
    /// When this entry was cached
    std::chrono::steady_clock::time_point cached_at;
    /// When this entry was last accessed
    std::chrono::steady_clock::time_point last_access;
    /// Access count for statistics
    std::uint32_t access_count = 0;
    /// Original remote URL
    std::string source_url;
    /// Asset type hint (extension)
    std::string asset_type;
};

/// Cache validation result
enum class ValidationResult {
    Valid,          ///< Cache entry is still valid
    Stale,          ///< Entry needs revalidation with server
    Invalid,        ///< Entry is invalid, must refetch
    NotFound,       ///< Entry not in cache
};

/// Cache statistics
struct CacheStats {
    std::size_t hot_entries = 0;
    std::size_t hot_size_bytes = 0;
    std::size_t hot_capacity_bytes = 0;
    std::size_t warm_entries = 0;
    std::size_t warm_size_bytes = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    std::uint64_t disk_reads = 0;
    std::uint64_t disk_writes = 0;

    [[nodiscard]] double hit_rate() const {
        const auto total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / static_cast<double>(total) : 0.0;
    }
};

// =============================================================================
// Cache Entry
// =============================================================================

/// In-memory cache entry
struct CacheEntry {
    CacheEntryMeta meta;
    std::vector<std::uint8_t> data;

    [[nodiscard]] bool is_expired() const {
        if (meta.ttl.count() == 0) return false;
        auto now = std::chrono::steady_clock::now();
        return (now - meta.cached_at) > meta.ttl;
    }
};

// =============================================================================
// Hot Cache (In-Memory LRU)
// =============================================================================

/// In-memory LRU cache with priority-aware eviction
class HotCache {
public:
    using Key = std::string;

    /// Create hot cache with capacity limit
    explicit HotCache(std::size_t max_bytes)
        : m_max_bytes(max_bytes), m_current_bytes(0) {}

    /// Get entry from cache (updates LRU order)
    [[nodiscard]] std::shared_ptr<CacheEntry> get(const Key& key) {
        std::unique_lock lock(m_mutex);

        auto it = m_lookup.find(key);
        if (it == m_lookup.end()) {
            return nullptr;
        }

        // Move to front (most recently used)
        auto list_it = it->second;
        m_lru_list.splice(m_lru_list.begin(), m_lru_list, list_it);

        // Update access stats
        list_it->entry->meta.last_access = std::chrono::steady_clock::now();
        list_it->entry->meta.access_count++;

        return list_it->entry;
    }

    /// Put entry in cache (may trigger eviction)
    void put(const Key& key, std::shared_ptr<CacheEntry> entry) {
        std::unique_lock lock(m_mutex);

        // Remove existing entry if present
        auto it = m_lookup.find(key);
        if (it != m_lookup.end()) {
            m_current_bytes -= it->second->entry->meta.size_bytes;
            m_lru_list.erase(it->second);
            m_lookup.erase(it);
        }

        // Evict until we have space
        while (m_current_bytes + entry->meta.size_bytes > m_max_bytes && !m_lru_list.empty()) {
            evict_one_unlocked();
        }

        // Insert at front
        m_lru_list.push_front(LruNode{key, entry});
        m_lookup[key] = m_lru_list.begin();
        m_current_bytes += entry->meta.size_bytes;
    }

    /// Remove entry from cache
    bool remove(const Key& key) {
        std::unique_lock lock(m_mutex);

        auto it = m_lookup.find(key);
        if (it == m_lookup.end()) {
            return false;
        }

        m_current_bytes -= it->second->entry->meta.size_bytes;
        m_lru_list.erase(it->second);
        m_lookup.erase(it);
        return true;
    }

    /// Check if key exists
    [[nodiscard]] bool contains(const Key& key) const {
        std::shared_lock lock(m_mutex);
        return m_lookup.find(key) != m_lookup.end();
    }

    /// Clear all entries
    void clear() {
        std::unique_lock lock(m_mutex);
        m_lru_list.clear();
        m_lookup.clear();
        m_current_bytes = 0;
    }

    /// Get current size in bytes
    [[nodiscard]] std::size_t size_bytes() const {
        std::shared_lock lock(m_mutex);
        return m_current_bytes;
    }

    /// Get entry count
    [[nodiscard]] std::size_t count() const {
        std::shared_lock lock(m_mutex);
        return m_lookup.size();
    }

    /// Get capacity
    [[nodiscard]] std::size_t capacity_bytes() const { return m_max_bytes; }

    /// Set eviction callback (called when entry is evicted)
    void set_eviction_callback(std::function<void(const Key&, std::shared_ptr<CacheEntry>)> cb) {
        std::unique_lock lock(m_mutex);
        m_on_evict = std::move(cb);
    }

private:
    struct LruNode {
        Key key;
        std::shared_ptr<CacheEntry> entry;
    };

    void evict_one_unlocked() {
        if (m_lru_list.empty()) return;

        // Find lowest priority item from the back
        auto evict_it = m_lru_list.end();
        CachePriority lowest_priority = CachePriority::Essential;

        // Search from back (least recently used)
        for (auto it = m_lru_list.rbegin(); it != m_lru_list.rend(); ++it) {
            if (it->entry->meta.priority <= lowest_priority) {
                lowest_priority = it->entry->meta.priority;
                evict_it = std::prev(it.base());

                // If we found a Low priority item, evict it immediately
                if (lowest_priority == CachePriority::Low) {
                    break;
                }
            }
        }

        if (evict_it == m_lru_list.end()) {
            // Fallback: evict LRU item
            evict_it = std::prev(m_lru_list.end());
        }

        // Notify callback before eviction
        if (m_on_evict) {
            m_on_evict(evict_it->key, evict_it->entry);
        }

        m_current_bytes -= evict_it->entry->meta.size_bytes;
        m_lookup.erase(evict_it->key);
        m_lru_list.erase(evict_it);
        ++m_eviction_count;
    }

    mutable std::shared_mutex m_mutex;
    std::list<LruNode> m_lru_list;
    std::unordered_map<Key, typename std::list<LruNode>::iterator> m_lookup;
    std::size_t m_max_bytes;
    std::size_t m_current_bytes;
    std::uint64_t m_eviction_count = 0;
    std::function<void(const Key&, std::shared_ptr<CacheEntry>)> m_on_evict;
};

// =============================================================================
// Warm Cache (Disk-Based)
// =============================================================================

/// Disk-based cache with content-addressable storage
class WarmCache {
public:
    /// Create warm cache at the specified directory
    explicit WarmCache(std::filesystem::path cache_dir)
        : m_cache_dir(std::move(cache_dir)) {
        std::filesystem::create_directories(m_cache_dir);
        std::filesystem::create_directories(m_cache_dir / "data");
        std::filesystem::create_directories(m_cache_dir / "meta");
        load_index();
    }

    /// Get entry from disk cache
    [[nodiscard]] std::shared_ptr<CacheEntry> get(const std::string& key) {
        std::shared_lock lock(m_mutex);

        auto it = m_index.find(key);
        if (it == m_index.end()) {
            return nullptr;
        }

        // Load from disk
        auto entry = load_entry(key, it->second);
        if (entry) {
            ++m_read_count;
        }
        return entry;
    }

    /// Put entry to disk cache
    void put(const std::string& key, std::shared_ptr<CacheEntry> entry) {
        std::unique_lock lock(m_mutex);

        if (save_entry(key, entry)) {
            m_index[key] = entry->meta;
            m_total_bytes += entry->meta.size_bytes;
            ++m_write_count;
        }
    }

    /// Remove entry from disk cache
    bool remove(const std::string& key) {
        std::unique_lock lock(m_mutex);

        auto it = m_index.find(key);
        if (it == m_index.end()) {
            return false;
        }

        auto data_path = get_data_path(key);
        auto meta_path = get_meta_path(key);

        std::error_code ec;
        std::filesystem::remove(data_path, ec);
        std::filesystem::remove(meta_path, ec);

        m_total_bytes -= it->second.size_bytes;
        m_index.erase(it);
        return true;
    }

    /// Check if key exists
    [[nodiscard]] bool contains(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        return m_index.find(key) != m_index.end();
    }

    /// Get metadata without loading data
    [[nodiscard]] std::optional<CacheEntryMeta> get_meta(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        auto it = m_index.find(key);
        if (it == m_index.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    /// Clear all entries
    void clear() {
        std::unique_lock lock(m_mutex);

        std::error_code ec;
        std::filesystem::remove_all(m_cache_dir / "data", ec);
        std::filesystem::remove_all(m_cache_dir / "meta", ec);
        std::filesystem::create_directories(m_cache_dir / "data");
        std::filesystem::create_directories(m_cache_dir / "meta");

        m_index.clear();
        m_total_bytes = 0;
    }

    /// Get total size in bytes
    [[nodiscard]] std::size_t size_bytes() const {
        std::shared_lock lock(m_mutex);
        return m_total_bytes;
    }

    /// Get entry count
    [[nodiscard]] std::size_t count() const {
        std::shared_lock lock(m_mutex);
        return m_index.size();
    }

    /// Get read count
    [[nodiscard]] std::uint64_t read_count() const { return m_read_count; }

    /// Get write count
    [[nodiscard]] std::uint64_t write_count() const { return m_write_count; }

private:
    [[nodiscard]] std::filesystem::path get_data_path(const std::string& key) const {
        // Use hash-based subdirectories to avoid too many files in one dir
        auto hash = std::hash<std::string>{}(key);
        auto subdir = std::to_string(hash % 256);
        return m_cache_dir / "data" / subdir / (key + ".bin");
    }

    [[nodiscard]] std::filesystem::path get_meta_path(const std::string& key) const {
        auto hash = std::hash<std::string>{}(key);
        auto subdir = std::to_string(hash % 256);
        return m_cache_dir / "meta" / subdir / (key + ".json");
    }

    void load_index() {
        // Scan meta directory to rebuild index
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_cache_dir / "meta", ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto key = entry.path().stem().string();
                auto meta = load_meta(entry.path());
                if (meta) {
                    m_index[key] = *meta;
                    m_total_bytes += meta->size_bytes;
                }
            }
        }
    }

    [[nodiscard]] std::optional<CacheEntryMeta> load_meta(const std::filesystem::path& path) const {
        // Simplified JSON parsing - in production use a proper JSON library
        std::ifstream file(path);
        if (!file) return std::nullopt;

        CacheEntryMeta meta;
        std::string line;
        while (std::getline(file, line)) {
            // Parse simple key: value format
            auto pos = line.find(':');
            if (pos == std::string::npos) continue;

            auto key = line.substr(0, pos);
            auto value = line.substr(pos + 1);
            // Trim whitespace
            while (!key.empty() && std::isspace(key.back())) key.pop_back();
            while (!value.empty() && std::isspace(value.front())) value.erase(0, 1);

            if (key == "content_hash") meta.content_hash = value;
            else if (key == "etag") meta.etag = value;
            else if (key == "last_modified") meta.last_modified = value;
            else if (key == "size_bytes") meta.size_bytes = std::stoull(value);
            else if (key == "priority") meta.priority = static_cast<CachePriority>(std::stoi(value));
            else if (key == "source_url") meta.source_url = value;
            else if (key == "asset_type") meta.asset_type = value;
        }

        return meta;
    }

    bool save_meta(const std::filesystem::path& path, const CacheEntryMeta& meta) const {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        if (!file) return false;

        file << "content_hash: " << meta.content_hash << "\n";
        file << "etag: " << meta.etag << "\n";
        file << "last_modified: " << meta.last_modified << "\n";
        file << "size_bytes: " << meta.size_bytes << "\n";
        file << "priority: " << static_cast<int>(meta.priority) << "\n";
        file << "source_url: " << meta.source_url << "\n";
        file << "asset_type: " << meta.asset_type << "\n";

        return true;
    }

    [[nodiscard]] std::shared_ptr<CacheEntry> load_entry(
        const std::string& key, const CacheEntryMeta& meta) const {

        auto data_path = get_data_path(key);
        std::ifstream file(data_path, std::ios::binary);
        if (!file) return nullptr;

        auto entry = std::make_shared<CacheEntry>();
        entry->meta = meta;
        entry->meta.last_access = std::chrono::steady_clock::now();

        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        entry->data.resize(static_cast<std::size_t>(size));
        file.read(reinterpret_cast<char*>(entry->data.data()), size);

        return entry;
    }

    bool save_entry(const std::string& key, std::shared_ptr<CacheEntry> entry) {
        auto data_path = get_data_path(key);
        auto meta_path = get_meta_path(key);

        std::filesystem::create_directories(data_path.parent_path());

        // Save data
        std::ofstream data_file(data_path, std::ios::binary);
        if (!data_file) return false;
        data_file.write(reinterpret_cast<const char*>(entry->data.data()),
                        static_cast<std::streamsize>(entry->data.size()));

        // Save metadata
        return save_meta(meta_path, entry->meta);
    }

    mutable std::shared_mutex m_mutex;
    std::filesystem::path m_cache_dir;
    std::unordered_map<std::string, CacheEntryMeta> m_index;
    std::size_t m_total_bytes = 0;
    std::atomic<std::uint64_t> m_read_count{0};
    std::atomic<std::uint64_t> m_write_count{0};
};

// =============================================================================
// Tiered Asset Cache
// =============================================================================

/// Configuration for the tiered cache
struct TieredCacheConfig {
    /// Hot cache memory limit (default 256 MB)
    std::size_t hot_cache_bytes = 256 * 1024 * 1024;
    /// Disk cache directory
    std::filesystem::path disk_cache_dir = "cache/assets";
    /// Enable disk caching
    bool enable_disk_cache = true;
    /// Default TTL for entries (0 = infinite)
    std::chrono::seconds default_ttl{0};
    /// Auto-promote from warm to hot on access
    bool auto_promote = true;
};

/// Three-tier cache: Hot (memory) → Warm (disk) → Cold (remote)
class TieredCache {
public:
    explicit TieredCache(TieredCacheConfig config = {})
        : m_config(std::move(config))
        , m_hot_cache(m_config.hot_cache_bytes)
        , m_warm_cache(m_config.disk_cache_dir)
    {
        // Set up eviction callback to persist to disk
        if (m_config.enable_disk_cache) {
            m_hot_cache.set_eviction_callback(
                [this](const std::string& key, std::shared_ptr<CacheEntry> entry) {
                    m_warm_cache.put(key, entry);
                });
        }
    }

    /// Get from cache (checks hot, then warm)
    [[nodiscard]] std::shared_ptr<CacheEntry> get(const std::string& key) {
        // Check hot cache first
        auto entry = m_hot_cache.get(key);
        if (entry) {
            ++m_stats.hits;
            return entry;
        }

        // Check warm cache
        if (m_config.enable_disk_cache) {
            entry = m_warm_cache.get(key);
            if (entry) {
                ++m_stats.hits;
                ++m_stats.disk_reads;

                // Promote to hot cache
                if (m_config.auto_promote) {
                    m_hot_cache.put(key, entry);
                }

                return entry;
            }
        }

        ++m_stats.misses;
        return nullptr;
    }

    /// Put to hot cache (will cascade to warm on eviction)
    void put(const std::string& key, std::shared_ptr<CacheEntry> entry) {
        m_hot_cache.put(key, entry);
    }

    /// Put directly to warm cache (skip hot)
    void put_warm(const std::string& key, std::shared_ptr<CacheEntry> entry) {
        if (m_config.enable_disk_cache) {
            m_warm_cache.put(key, entry);
            ++m_stats.disk_writes;
        }
    }

    /// Remove from all cache tiers
    void remove(const std::string& key) {
        m_hot_cache.remove(key);
        if (m_config.enable_disk_cache) {
            m_warm_cache.remove(key);
        }
    }

    /// Invalidate entry (remove and mark for refetch)
    void invalidate(const std::string& key) {
        remove(key);
        // Could track invalidated keys if needed
    }

    /// Check if exists in any tier
    [[nodiscard]] bool contains(const std::string& key) const {
        if (m_hot_cache.contains(key)) return true;
        if (m_config.enable_disk_cache && m_warm_cache.contains(key)) return true;
        return false;
    }

    /// Validate cache entry (check TTL, etc.)
    [[nodiscard]] ValidationResult validate(const std::string& key) const {
        // Check hot cache
        auto entry = const_cast<HotCache&>(m_hot_cache).get(key);
        if (entry) {
            if (entry->is_expired()) {
                return ValidationResult::Stale;
            }
            return ValidationResult::Valid;
        }

        // Check warm cache metadata
        if (m_config.enable_disk_cache) {
            auto meta = m_warm_cache.get_meta(key);
            if (meta) {
                // Could check TTL here if stored
                return ValidationResult::Valid;
            }
        }

        return ValidationResult::NotFound;
    }

    /// Get metadata for revalidation (ETag, Last-Modified)
    [[nodiscard]] std::optional<CacheEntryMeta> get_meta(const std::string& key) const {
        // Check hot cache
        auto entry = const_cast<HotCache&>(m_hot_cache).get(key);
        if (entry) {
            return entry->meta;
        }

        // Check warm cache
        if (m_config.enable_disk_cache) {
            return m_warm_cache.get_meta(key);
        }

        return std::nullopt;
    }

    /// Clear all caches
    void clear() {
        m_hot_cache.clear();
        if (m_config.enable_disk_cache) {
            m_warm_cache.clear();
        }
        m_stats = CacheStats{};
    }

    /// Get statistics
    [[nodiscard]] CacheStats stats() const {
        CacheStats s = m_stats;
        s.hot_entries = m_hot_cache.count();
        s.hot_size_bytes = m_hot_cache.size_bytes();
        s.hot_capacity_bytes = m_hot_cache.capacity_bytes();
        if (m_config.enable_disk_cache) {
            s.warm_entries = m_warm_cache.count();
            s.warm_size_bytes = m_warm_cache.size_bytes();
            s.disk_reads = m_warm_cache.read_count();
            s.disk_writes = m_warm_cache.write_count();
        }
        return s;
    }

    /// Get configuration
    [[nodiscard]] const TieredCacheConfig& config() const { return m_config; }

private:
    TieredCacheConfig m_config;
    HotCache m_hot_cache;
    WarmCache m_warm_cache;
    mutable CacheStats m_stats;
};

} // namespace void_asset
