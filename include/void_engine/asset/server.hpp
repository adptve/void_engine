#pragma once

/// @file server.hpp
/// @brief Asset server for void_asset

#include "fwd.hpp"
#include "types.hpp"
#include "handle.hpp"
#include "loader.hpp"
#include "storage.hpp"
#include <void_engine/core/error.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace void_asset {

// =============================================================================
// AssetServerConfig
// =============================================================================

/// Configuration for asset server
struct AssetServerConfig {
    std::string asset_dir = "assets";
    bool hot_reload = true;
    std::size_t max_concurrent_loads = 4;
    bool auto_garbage_collect = true;
    std::chrono::milliseconds gc_interval{5000};

    /// Default constructor
    AssetServerConfig() = default;

    /// Builder pattern
    AssetServerConfig& with_asset_dir(const std::string& dir) {
        asset_dir = dir;
        return *this;
    }

    AssetServerConfig& with_hot_reload(bool enable) {
        hot_reload = enable;
        return *this;
    }

    AssetServerConfig& with_max_concurrent_loads(std::size_t max) {
        max_concurrent_loads = max;
        return *this;
    }
};

// =============================================================================
// PendingLoad
// =============================================================================

/// Pending asset load request
struct PendingLoad {
    AssetId id;
    AssetPath path;
    std::type_index type_id{typeid(void)};
    ErasedLoader* loader = nullptr;
};

// =============================================================================
// AssetServer
// =============================================================================

/// Main asset management system
class AssetServer {
public:
    using FileReader = std::function<std::optional<std::vector<std::uint8_t>>(const std::string&)>;

    /// Constructor with config
    explicit AssetServer(AssetServerConfig config = {})
        : m_config(std::move(config))
    {
        // Register built-in loaders
        register_loader<BytesAsset>(std::make_unique<BytesLoader>());
        register_loader<TextAsset>(std::make_unique<TextLoader>());
    }

    /// Register typed loader (base type)
    template<typename T>
    void register_loader(std::unique_ptr<AssetLoader<T>> loader) {
        m_loaders.register_loader<T>(std::move(loader));
    }

    /// Register derived loader type (automatically extracts asset type from loader)
    template<typename Derived,
             typename T = typename Derived::asset_type,
             typename = std::enable_if_t<std::is_base_of_v<AssetLoader<T>, Derived>>>
    void register_loader(std::unique_ptr<Derived> loader) {
        m_loaders.register_loader(std::move(loader));
    }

    /// Load asset by path
    template<typename T>
    [[nodiscard]] Handle<T> load(const std::string& path) {
        AssetPath asset_path(path);

        // Check if already loaded or loading
        if (auto existing_id = m_storage.get_id(asset_path)) {
            return m_storage.get_handle<T>(*existing_id);
        }

        // Find loader for extension
        std::string ext = asset_path.extension();
        auto loaders = m_loaders.find_by_extension(ext);
        if (loaders.empty()) {
            return Handle<T>{};
        }

        // Allocate ID and register
        AssetId id = m_storage.allocate_id();
        auto handle = m_storage.register_asset<T>(id, asset_path);

        // Queue for loading
        PendingLoad pending;
        pending.id = id;
        pending.path = asset_path;
        pending.type_id = std::type_index(typeid(T));
        pending.loader = loaders.front();

        {
            std::lock_guard lock(m_pending_mutex);
            m_pending.push(std::move(pending));
        }

        return handle;
    }

    /// Load untyped asset
    [[nodiscard]] AssetId load_untyped(const std::string& path) {
        AssetPath asset_path(path);

        // Check if already loaded or loading
        if (auto existing_id = m_storage.get_id(asset_path)) {
            return *existing_id;
        }

        // Find loader for extension
        std::string ext = asset_path.extension();
        auto* loader = m_loaders.find_first(ext);
        if (!loader) {
            return AssetId::invalid();
        }

        // Allocate ID and register
        AssetId id = m_storage.allocate_id();

        // Create handle data manually
        auto handle_data = std::make_shared<HandleData>();
        handle_data->id = id;
        handle_data->set_state(LoadState::Loading);

        // Queue for loading
        PendingLoad pending;
        pending.id = id;
        pending.path = asset_path;
        pending.type_id = loader->type_id();
        pending.loader = loader;

        {
            std::lock_guard lock(m_pending_mutex);
            m_pending.push(std::move(pending));
        }

        return id;
    }

    /// Process pending loads
    void process() {
        process([this](const std::string& path) -> std::optional<std::vector<std::uint8_t>> {
            return read_file(path);
        });
    }

    /// Process pending loads with custom file reader
    void process(FileReader read_file) {
        std::vector<PendingLoad> to_load;

        {
            std::lock_guard lock(m_pending_mutex);
            while (!m_pending.empty() && to_load.size() < m_config.max_concurrent_loads) {
                to_load.push_back(std::move(m_pending.front()));
                m_pending.pop();
            }
        }

        for (auto& pending : to_load) {
            process_load(pending, read_file);
        }
    }

    /// Reload asset by ID
    void_core::Result<void> reload(AssetId id) {
        return reload(id, [this](const std::string& path) {
            return read_file(path);
        });
    }

    /// Reload asset with custom file reader
    void_core::Result<void> reload(AssetId id, FileReader read_file) {
        const auto* meta = m_storage.get_metadata(id);
        if (!meta) {
            return void_core::Err(AssetError::not_found("Asset ID not found"));
        }

        m_storage.mark_reloading(id);

        // Find loader
        std::string ext = meta->path.extension();
        auto* loader = m_loaders.find_first(ext);
        if (!loader) {
            m_storage.mark_failed(id, "No loader for extension");
            return void_core::Err(AssetError::no_loader(ext));
        }

        // Read file
        std::string full_path = m_config.asset_dir + "/" + meta->path.str();
        auto data = read_file(full_path);
        if (!data) {
            m_storage.mark_failed(id, "Failed to read file");
            return void_core::Err(AssetError::load_failed(meta->path.str(), "Failed to read file"));
        }

        // Load
        LoadContext ctx(*data, meta->path, id);
        auto result = loader->load_erased(ctx);

        if (!result) {
            m_storage.mark_failed(id, result.error().message());
            queue_event(AssetEvent::failed(id, meta->path, result.error().message()));
            return void_core::Err(result.error());
        }

        // Store
        m_storage.store_erased(id, result.value(), loader->type_id(),
            [loader](void* ptr) { loader->delete_asset(ptr); });

        queue_event(AssetEvent::reloaded(id, meta->path, meta->generation + 1));

        return void_core::Ok();
    }

    /// Unload asset
    bool unload(AssetId id) {
        const auto* meta = m_storage.get_metadata(id);
        if (meta) {
            queue_event(AssetEvent::unloaded(id, meta->path));
        }
        return m_storage.remove(id);
    }

    /// Get handle for existing asset
    template<typename T>
    [[nodiscard]] Handle<T> get_handle(const std::string& path) {
        AssetPath asset_path(path);
        if (auto id = m_storage.get_id(asset_path)) {
            return m_storage.get_handle<T>(*id);
        }
        return Handle<T>{};
    }

    /// Get asset ID by path
    [[nodiscard]] std::optional<AssetId> get_id(const std::string& path) const {
        return m_storage.get_id(AssetPath(path));
    }

    /// Get asset path by ID
    [[nodiscard]] std::optional<AssetPath> get_path(AssetId id) const {
        const auto* meta = m_storage.get_metadata(id);
        return meta ? std::optional{meta->path} : std::nullopt;
    }

    /// Check if asset is loaded
    [[nodiscard]] bool is_loaded(AssetId id) const {
        return m_storage.is_loaded(id);
    }

    /// Get load state
    [[nodiscard]] LoadState get_state(AssetId id) const {
        return m_storage.get_state(id);
    }

    /// Get asset metadata
    [[nodiscard]] const AssetMetadata* get_metadata(AssetId id) const {
        return m_storage.get_metadata(id);
    }

    /// Drain events
    std::vector<AssetEvent> drain_events() {
        std::lock_guard lock(m_events_mutex);
        std::vector<AssetEvent> events;
        std::swap(events, m_events);
        return events;
    }

    /// Collect garbage
    std::size_t collect_garbage() {
        return m_storage.remove_unreferenced();
    }

    /// Get pending load count
    [[nodiscard]] std::size_t pending_count() const {
        std::lock_guard lock(m_pending_mutex);
        return m_pending.size();
    }

    /// Get loaded asset count
    [[nodiscard]] std::size_t loaded_count() const {
        return m_storage.loaded_count();
    }

    /// Get total asset count
    [[nodiscard]] std::size_t total_count() const {
        return m_storage.len();
    }

    /// Get storage
    [[nodiscard]] AssetStorage& storage() { return m_storage; }
    [[nodiscard]] const AssetStorage& storage() const { return m_storage; }

    /// Get loader registry
    [[nodiscard]] LoaderRegistry& loaders() { return m_loaders; }
    [[nodiscard]] const LoaderRegistry& loaders() const { return m_loaders; }

    /// Get config
    [[nodiscard]] const AssetServerConfig& config() const { return m_config; }

private:
    void process_load(PendingLoad& pending, FileReader& read_file) {
        std::string full_path = m_config.asset_dir + "/" + pending.path.str();

        auto data = read_file(full_path);
        if (!data) {
            m_storage.mark_failed(pending.id, "Failed to read file");
            queue_event(AssetEvent::failed(pending.id, pending.path, "Failed to read file"));
            return;
        }

        LoadContext ctx(*data, pending.path, pending.id);
        auto result = pending.loader->load_erased(ctx);

        if (!result) {
            m_storage.mark_failed(pending.id, result.error().message());
            queue_event(AssetEvent::failed(pending.id, pending.path, result.error().message()));
            return;
        }

        m_storage.store_erased(pending.id, result.value(), pending.loader->type_id(),
            [loader = pending.loader](void* ptr) { loader->delete_asset(ptr); });

        queue_event(AssetEvent::loaded(pending.id, pending.path));
    }

    void queue_event(AssetEvent event) {
        std::lock_guard lock(m_events_mutex);
        m_events.push_back(std::move(event));
    }

    std::optional<std::vector<std::uint8_t>> read_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return std::nullopt;
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> data(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            return std::nullopt;
        }

        return data;
    }

    AssetServerConfig m_config;
    AssetStorage m_storage;
    LoaderRegistry m_loaders;

    std::queue<PendingLoad> m_pending;
    mutable std::mutex m_pending_mutex;

    std::vector<AssetEvent> m_events;
    mutable std::mutex m_events_mutex;
};

// =============================================================================
// Server Statistics (Implemented in server.cpp)
// =============================================================================

/// Record a server request
void record_server_request(bool cache_hit);

/// Record a load completed
void record_load_completed(bool success);

/// Record a reload completed
void record_reload_completed();

/// Record a garbage collection run
void record_gc_run();

/// Format server statistics
std::string format_server_statistics();

/// Reset server statistics
void reset_server_statistics();

// =============================================================================
// Hot-Reload Support (Implemented in server.cpp)
// =============================================================================

// Forward declaration
namespace void_core {
    class HotReloadable;
}

/// Create a hot-reloadable adapter for an asset server
std::unique_ptr<void_core::HotReloadable> make_hot_reloadable(AssetServer& server);

// =============================================================================
// Global Asset Server (Implemented in server.cpp)
// =============================================================================

/// Get or create global asset server
AssetServer& global_asset_server();

/// Get or create global asset server with config
AssetServer& global_asset_server(AssetServerConfig config);

/// Shutdown global asset server
void shutdown_global_asset_server();

/// Check if global asset server exists
bool has_global_asset_server();

// =============================================================================
// Batch Loading Utilities (Implemented in server.cpp)
// =============================================================================

/// Load multiple assets at once
std::vector<AssetId> load_batch(AssetServer& server, const std::vector<std::string>& paths);

/// Wait for loads to complete with timeout
void wait_for_loads(AssetServer& server, const std::vector<AssetId>& ids,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

// =============================================================================
// Validation Utilities (Implemented in server.cpp)
// =============================================================================

/// Validate asset server configuration and state
std::vector<std::string> validate_asset_server(const AssetServer& server);

// =============================================================================
// Debug Utilities (Implemented in server.cpp)
// =============================================================================

namespace debug {

/// Format AssetServerConfig for debugging
std::string format_asset_server_config(const AssetServerConfig& config);

/// Format PendingLoad for debugging
std::string format_pending_load(const PendingLoad& pending);

/// Format AssetServer for debugging
std::string format_asset_server(const AssetServer& server);

} // namespace debug

} // namespace void_asset
