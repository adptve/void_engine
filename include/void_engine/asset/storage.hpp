#pragma once

/// @file storage.hpp
/// @brief Asset storage system for void_asset

#include "fwd.hpp"
#include "types.hpp"
#include "handle.hpp"
#include <cstdint>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <atomic>
#include <utility>

namespace void_asset {

// =============================================================================
// AssetEntry
// =============================================================================

/// Internal entry for storing an asset
struct AssetEntry {
    std::shared_ptr<HandleData> handle_data;
    void* asset = nullptr;
    std::type_index type_id{typeid(void)};
    std::function<void(void*)> deleter;
    AssetMetadata metadata;

    /// Default constructor
    AssetEntry() = default;

    /// Construct with data
    template<typename T>
    AssetEntry(std::shared_ptr<HandleData> hd, std::unique_ptr<T> asset_ptr, const AssetMetadata& meta)
        : handle_data(std::move(hd))
        , asset(asset_ptr.release())
        , type_id(std::type_index(typeid(T)))
        , metadata(meta)
    {
        deleter = [](void* ptr) { delete static_cast<T*>(ptr); };
    }

    /// Destructor
    ~AssetEntry() {
        if (asset && deleter) {
            deleter(asset);
        }
    }

    /// Move constructor
    AssetEntry(AssetEntry&& other) noexcept
        : handle_data(std::move(other.handle_data))
        , asset(other.asset)
        , type_id(other.type_id)
        , deleter(std::move(other.deleter))
        , metadata(std::move(other.metadata))
    {
        other.asset = nullptr;
    }

    /// Move assignment
    AssetEntry& operator=(AssetEntry&& other) noexcept {
        if (this != &other) {
            if (asset && deleter) {
                deleter(asset);
            }
            handle_data = std::move(other.handle_data);
            asset = other.asset;
            type_id = other.type_id;
            deleter = std::move(other.deleter);
            metadata = std::move(other.metadata);
            other.asset = nullptr;
        }
        return *this;
    }

    /// Non-copyable
    AssetEntry(const AssetEntry&) = delete;
    AssetEntry& operator=(const AssetEntry&) = delete;

    /// Get asset as typed pointer
    template<typename T>
    [[nodiscard]] T* get() const {
        if (type_id == std::type_index(typeid(T))) {
            return static_cast<T*>(asset);
        }
        return nullptr;
    }
};

// =============================================================================
// AssetStorage
// =============================================================================

/// Central storage for all loaded assets
class AssetStorage {
public:
    /// Constructor
    AssetStorage() = default;

    /// Allocate new asset ID
    [[nodiscard]] AssetId allocate_id() {
        return AssetId{m_next_id.fetch_add(1, std::memory_order_relaxed)};
    }

    /// Register asset for loading (creates handle data, sets to Loading state)
    template<typename T>
    [[nodiscard]] Handle<T> register_asset(AssetId id, const AssetPath& path) {
        std::unique_lock lock(m_mutex);

        auto handle_data = std::make_shared<HandleData>();
        handle_data->id = id;
        handle_data->set_state(LoadState::Loading);

        AssetEntry entry;
        entry.handle_data = handle_data;
        entry.type_id = std::type_index(typeid(T));
        entry.metadata.id = id;
        entry.metadata.path = path;
        entry.metadata.type_id = AssetTypeId::of<T>();
        entry.metadata.state = LoadState::Loading;

        m_entries[id] = std::move(entry);
        m_path_to_id[path.str()] = id;

        return Handle<T>(handle_data, nullptr);
    }

    /// Store loaded asset
    template<typename T>
    void store(AssetId id, std::unique_ptr<T> asset) {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return;
        }

        auto& entry = it->second;
        T* asset_ptr = asset.release();

        // Clean up old asset if any
        if (entry.asset && entry.deleter) {
            entry.deleter(entry.asset);
        }

        entry.asset = asset_ptr;
        entry.type_id = std::type_index(typeid(T));
        entry.deleter = [](void* ptr) { delete static_cast<T*>(ptr); };
        entry.metadata.mark_loaded(sizeof(T));
        entry.handle_data->set_state(LoadState::Loaded);
    }

    /// Store type-erased asset
    void store_erased(AssetId id, void* asset, std::type_index type_id, std::function<void(void*)> deleter) {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return;
        }

        auto& entry = it->second;

        // Clean up old asset if any
        if (entry.asset && entry.deleter) {
            entry.deleter(entry.asset);
        }

        entry.asset = asset;
        entry.type_id = type_id;
        entry.deleter = std::move(deleter);
        entry.metadata.mark_loaded();
        entry.handle_data->set_state(LoadState::Loaded);
    }

    /// Mark asset as failed
    void mark_failed(AssetId id, const std::string& error) {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it != m_entries.end()) {
            it->second.metadata.mark_failed(error);
            it->second.handle_data->set_state(LoadState::Failed);
        }
    }

    /// Mark asset as reloading
    void mark_reloading(AssetId id) {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it != m_entries.end()) {
            it->second.metadata.mark_reloading();
            it->second.handle_data->set_state(LoadState::Reloading);
        }
    }

    /// Get handle for existing asset
    template<typename T>
    [[nodiscard]] Handle<T> get_handle(AssetId id) {
        std::shared_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return Handle<T>{};
        }

        auto& entry = it->second;
        if (entry.type_id != std::type_index(typeid(T))) {
            return Handle<T>{};
        }

        return Handle<T>(entry.handle_data, static_cast<T*>(entry.asset));
    }

    /// Get asset by ID
    template<typename T>
    [[nodiscard]] T* get(AssetId id) {
        std::shared_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return nullptr;
        }

        return it->second.get<T>();
    }

    /// Get metadata
    [[nodiscard]] const AssetMetadata* get_metadata(AssetId id) const {
        std::shared_lock lock(m_mutex);

        auto it = m_entries.find(id);
        return it != m_entries.end() ? &it->second.metadata : nullptr;
    }

    /// Get ID by path
    [[nodiscard]] std::optional<AssetId> get_id(const AssetPath& path) const {
        std::shared_lock lock(m_mutex);

        auto it = m_path_to_id.find(path.str());
        if (it != m_path_to_id.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Check if asset exists
    [[nodiscard]] bool contains(AssetId id) const {
        std::shared_lock lock(m_mutex);
        return m_entries.find(id) != m_entries.end();
    }

    /// Check if asset is loaded
    [[nodiscard]] bool is_loaded(AssetId id) const {
        std::shared_lock lock(m_mutex);

        auto it = m_entries.find(id);
        return it != m_entries.end() && it->second.metadata.is_loaded();
    }

    /// Get load state
    [[nodiscard]] LoadState get_state(AssetId id) const {
        std::shared_lock lock(m_mutex);

        auto it = m_entries.find(id);
        return it != m_entries.end() ? it->second.metadata.state : LoadState::NotLoaded;
    }

    /// Remove asset
    bool remove(AssetId id) {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return false;
        }

        // Remove path mapping
        m_path_to_id.erase(it->second.metadata.path.str());

        m_entries.erase(it);
        return true;
    }

    /// Collect garbage (find unreferenced assets)
    [[nodiscard]] std::vector<AssetId> collect_garbage() const {
        std::shared_lock lock(m_mutex);

        std::vector<AssetId> unreferenced;
        for (const auto& [id, entry] : m_entries) {
            if (entry.handle_data && entry.handle_data->use_count() <= 1) {
                // Only the storage holds a reference
                unreferenced.push_back(id);
            }
        }
        return unreferenced;
    }

    /// Remove unreferenced assets
    std::size_t remove_unreferenced() {
        auto unreferenced = collect_garbage();
        for (const auto& id : unreferenced) {
            remove(id);
        }
        return unreferenced.size();
    }

    /// Get total count
    [[nodiscard]] std::size_t len() const {
        std::shared_lock lock(m_mutex);
        return m_entries.size();
    }

    /// Get loaded count
    [[nodiscard]] std::size_t loaded_count() const {
        std::shared_lock lock(m_mutex);
        std::size_t count = 0;
        for (const auto& [id, entry] : m_entries) {
            if (entry.metadata.is_loaded()) {
                count++;
            }
        }
        return count;
    }

    /// Clear all assets
    void clear() {
        std::unique_lock lock(m_mutex);
        m_entries.clear();
        m_path_to_id.clear();
    }

    /// Iterate over all assets
    template<typename F>
    void for_each(F&& func) const {
        std::shared_lock lock(m_mutex);
        for (const auto& [id, entry] : m_entries) {
            func(id, entry.metadata);
        }
    }

private:
    std::map<AssetId, AssetEntry> m_entries;
    std::map<std::string, AssetId> m_path_to_id;
    std::atomic<std::uint64_t> m_next_id{1};
    mutable std::shared_mutex m_mutex;
};

// =============================================================================
// Storage Statistics (Implemented in storage.cpp)
// =============================================================================

/// Record an asset stored
void record_asset_stored(std::size_t bytes = 0);

/// Record an asset removed
void record_asset_removed(std::size_t bytes = 0);

/// Record a garbage collection
void record_garbage_collection(std::size_t count);

/// Format storage statistics
std::string format_storage_statistics();

/// Reset storage statistics
void reset_storage_statistics();

// =============================================================================
// Storage Validation (Implemented in storage.cpp)
// =============================================================================

/// Validate storage contents
std::vector<std::string> validate_storage(const AssetStorage& storage);

// =============================================================================
// Dependency Graph Utilities (Implemented in storage.cpp)
// =============================================================================

/// Get all dependents (transitive)
std::vector<AssetId> get_all_dependents(const AssetStorage& storage, AssetId id);

/// Get all dependencies (transitive)
std::vector<AssetId> get_all_dependencies(const AssetStorage& storage, AssetId id);

/// Check for circular dependencies
bool has_circular_dependency(const AssetStorage& storage, AssetId id);

// =============================================================================
// Storage Serialization (Implemented in storage.cpp)
// =============================================================================

namespace serialization {

/// Serialize storage manifest (for hot-reload)
std::vector<std::uint8_t> serialize_storage_manifest(const AssetStorage& storage);

/// Deserialize storage manifest
void_core::Result<std::vector<std::pair<AssetId, AssetPath>>> deserialize_storage_manifest(
    const std::vector<std::uint8_t>& data);

} // namespace serialization

// =============================================================================
// Debug Utilities (Implemented in storage.cpp)
// =============================================================================

namespace debug {

/// Format asset entry for debugging
std::string format_asset_entry(const AssetEntry& entry);

/// Format asset storage for debugging
std::string format_asset_storage(const AssetStorage& storage);

} // namespace debug

} // namespace void_asset
