/// @file asset_registry.hpp
/// @brief Hot-reloadable asset registry with generational handles
///
/// The AssetRegistry provides centralized asset management with:
/// - Type-safe asset access via generational handles
/// - Hot-reload support with automatic generation tracking
/// - Event-driven notifications for asset changes
/// - Integration with void_core hot-reload infrastructure
///
/// This registry builds on top of AssetServer and AssetStorage to provide
/// a higher-level interface suitable for engine-wide asset management.

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "handle.hpp"
#include "server.hpp"
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/error.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace void_asset {

// =============================================================================
// AssetChangeCallback
// =============================================================================

/// Callback type for asset change notifications
using AssetChangeCallback = std::function<void(AssetId id, const AssetPath& path, std::uint32_t generation)>;

/// Callback type for asset load complete
using AssetLoadedCallback = std::function<void(AssetId id, const AssetPath& path)>;

/// Callback type for asset load failure
using AssetFailedCallback = std::function<void(AssetId id, const AssetPath& path, const std::string& error)>;

// =============================================================================
// RegistryConfig
// =============================================================================

/// Configuration for asset registry
struct RegistryConfig {
    /// Enable hot-reload watching
    bool hot_reload_enabled = true;

    /// Hot-reload polling interval in milliseconds
    std::uint32_t hot_reload_poll_ms = 100;

    /// Maximum number of concurrent loads
    std::size_t max_concurrent_loads = 8;

    /// Enable automatic garbage collection
    bool auto_gc = true;

    /// Garbage collection interval in seconds
    std::uint32_t gc_interval_seconds = 60;

    /// Builder pattern
    RegistryConfig& with_hot_reload(bool enabled) {
        hot_reload_enabled = enabled;
        return *this;
    }

    RegistryConfig& with_poll_interval(std::uint32_t ms) {
        hot_reload_poll_ms = ms;
        return *this;
    }

    RegistryConfig& with_max_loads(std::size_t max) {
        max_concurrent_loads = max;
        return *this;
    }
};

// =============================================================================
// AssetRegistry
// =============================================================================

/// @brief Central registry for all assets with hot-reload support
///
/// The registry provides:
/// - Type-safe asset loading and retrieval
/// - Generation tracking for stale handle detection
/// - Event callbacks for asset lifecycle events
/// - Hot-reload integration with void_core::HotReloadManager
/// - Thread-safe access to all operations
class AssetRegistry : public void_core::HotReloadable {
public:
    /// Constructor with default config
    AssetRegistry();

    /// Constructor with custom config
    explicit AssetRegistry(RegistryConfig config);

    /// Destructor
    ~AssetRegistry() override;

    // Non-copyable
    AssetRegistry(const AssetRegistry&) = delete;
    AssetRegistry& operator=(const AssetRegistry&) = delete;

    // Movable
    AssetRegistry(AssetRegistry&&) noexcept;
    AssetRegistry& operator=(AssetRegistry&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /// Initialize the registry with asset root path
    void_core::Result<void> initialize(const std::string& asset_root);

    /// Shutdown the registry and release all assets
    void shutdown();

    /// Check if registry is initialized
    [[nodiscard]] bool is_initialized() const noexcept;

    // =========================================================================
    // Asset Loading
    // =========================================================================

    /// Load asset by path (returns handle immediately, loads async)
    template<typename T>
    [[nodiscard]] Handle<T> load(const std::string& path);

    /// Load asset and wait for completion
    template<typename T>
    [[nodiscard]] Handle<T> load_sync(const std::string& path);

    /// Load asset from raw data
    template<typename T>
    [[nodiscard]] Handle<T> load_from_data(const std::string& name, std::vector<std::uint8_t> data);

    /// Check if asset exists by path
    [[nodiscard]] bool exists(const std::string& path) const;

    /// Check if asset is loaded
    [[nodiscard]] bool is_loaded(AssetId id) const;

    /// Check if asset is loading
    [[nodiscard]] bool is_loading(AssetId id) const;

    // =========================================================================
    // Asset Retrieval
    // =========================================================================

    /// Get asset by ID
    template<typename T>
    [[nodiscard]] T* get(AssetId id);

    /// Get asset by ID (const)
    template<typename T>
    [[nodiscard]] const T* get(AssetId id) const;

    /// Get asset by path
    template<typename T>
    [[nodiscard]] T* get_by_path(const std::string& path);

    /// Get handle by path
    template<typename T>
    [[nodiscard]] Handle<T> get_handle(const std::string& path);

    /// Get asset ID by path
    [[nodiscard]] std::optional<AssetId> get_id(const std::string& path) const;

    /// Get asset path by ID
    [[nodiscard]] std::optional<AssetPath> get_path(AssetId id) const;

    /// Get asset metadata
    [[nodiscard]] const AssetMetadata* get_metadata(AssetId id) const;

    /// Get current generation for asset
    [[nodiscard]] std::uint32_t get_generation(AssetId id) const;

    // =========================================================================
    // Asset Management
    // =========================================================================

    /// Unload asset by ID
    bool unload(AssetId id);

    /// Unload asset by path
    bool unload(const std::string& path);

    /// Force reload asset from disk
    void_core::Result<void> reload(AssetId id);

    /// Force reload asset by path
    void_core::Result<void> reload(const std::string& path);

    /// Unload all assets
    void unload_all();

    /// Collect garbage (remove unreferenced assets)
    std::size_t collect_garbage();

    // =========================================================================
    // Loader Registration
    // =========================================================================

    /// Register asset loader
    template<typename T>
    void register_loader(std::unique_ptr<AssetLoader<T>> loader);

    /// Check if extension is supported
    [[nodiscard]] bool supports_extension(const std::string& ext) const;

    /// Get supported extensions
    [[nodiscard]] std::vector<std::string> supported_extensions() const;

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    /// Register callback for when assets are loaded
    void on_asset_loaded(AssetLoadedCallback callback);

    /// Register callback for when assets fail to load
    void on_asset_failed(AssetFailedCallback callback);

    /// Register callback for when assets are reloaded (hot-reload)
    void on_asset_reloaded(AssetChangeCallback callback);

    /// Register callback for when assets are unloaded
    void on_asset_unloaded(AssetChangeCallback callback);

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Poll for hot-reload changes (call each frame)
    void poll_hot_reload();

    /// Process pending loads
    void process_pending();

    /// Enable/disable hot-reload
    void set_hot_reload_enabled(bool enabled);

    /// Check if hot-reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get total asset count
    [[nodiscard]] std::size_t total_count() const;

    /// Get loaded asset count
    [[nodiscard]] std::size_t loaded_count() const;

    /// Get pending load count
    [[nodiscard]] std::size_t pending_count() const;

    /// Get total memory usage (approximate)
    [[nodiscard]] std::size_t memory_usage() const;

    // =========================================================================
    // HotReloadable Interface
    // =========================================================================

    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "AssetRegistry"; }

    // =========================================================================
    // Internal Access
    // =========================================================================

    /// Get underlying asset server (advanced use)
    [[nodiscard]] AssetServer& server();
    [[nodiscard]] const AssetServer& server() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
Handle<T> AssetRegistry::load(const std::string& path) {
    return server().load<T>(path);
}

template<typename T>
Handle<T> AssetRegistry::load_sync(const std::string& path) {
    auto handle = server().load<T>(path);
    while (handle.is_loading()) {
        server().process();
    }
    return handle;
}

template<typename T>
Handle<T> AssetRegistry::load_from_data(const std::string& name, std::vector<std::uint8_t> data) {
    // Create virtual path
    AssetPath path(name);

    // Register asset
    auto& storage = server().storage();
    AssetId id = storage.allocate_id();
    auto handle = storage.register_asset<T>(id, path);

    // Find loader
    std::string ext = path.extension();
    auto loaders = server().loaders().find_by_extension(ext);
    if (loaders.empty()) {
        storage.mark_failed(id, "No loader for extension: " + ext);
        return handle;
    }

    // Load immediately
    LoadContext ctx(data, path, id);
    auto result = loaders.front()->load_erased(ctx);

    if (!result) {
        storage.mark_failed(id, result.error().message());
    } else {
        storage.store_erased(id, result.value(), loaders.front()->type_id(),
            [loader = loaders.front()](void* ptr) { loader->delete_asset(ptr); });
    }

    return storage.get_handle<T>(id);
}

template<typename T>
T* AssetRegistry::get(AssetId id) {
    return server().storage().get<T>(id);
}

template<typename T>
const T* AssetRegistry::get(AssetId id) const {
    return const_cast<AssetRegistry*>(this)->server().storage().get<T>(id);
}

template<typename T>
T* AssetRegistry::get_by_path(const std::string& path) {
    auto id = get_id(path);
    if (!id) return nullptr;
    return get<T>(*id);
}

template<typename T>
Handle<T> AssetRegistry::get_handle(const std::string& path) {
    return server().get_handle<T>(path);
}

template<typename T>
void AssetRegistry::register_loader(std::unique_ptr<AssetLoader<T>> loader) {
    server().register_loader(std::move(loader));
}

// =============================================================================
// Global Registry Access
// =============================================================================

/// Get global asset registry instance
AssetRegistry& global_registry();

/// Get global asset registry with configuration
AssetRegistry& global_registry(RegistryConfig config);

/// Initialize global registry with asset root
void_core::Result<void> init_global_registry(const std::string& asset_root);

/// Shutdown global registry
void shutdown_global_registry();

/// Check if global registry exists
bool has_global_registry();

} // namespace void_asset
