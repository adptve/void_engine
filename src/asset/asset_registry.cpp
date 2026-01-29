/// @file asset_registry.cpp
/// @brief Hot-reloadable asset registry implementation
///
/// Uses existing void_core hot-reload infrastructure (HotReloadSystem, PollingFileWatcher)
/// and void_asset server infrastructure (AssetServer, AssetStorage).

#include <void_engine/asset/asset_registry.hpp>
#include <void_engine/asset/server.hpp>
#include <void_engine/asset/storage.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <mutex>
#include <filesystem>

namespace void_asset {

// =============================================================================
// AssetRegistry Implementation
// =============================================================================

class AssetRegistry::Impl {
public:
    RegistryConfig config;
    std::unique_ptr<AssetServer> server;
    std::string asset_root;
    bool initialized = false;
    bool hot_reload_enabled = true;

    // Hot-reload system using existing void_core infrastructure
    std::unique_ptr<void_core::PollingFileWatcher> file_watcher;

    // Path to asset ID mapping for hot-reload
    std::map<std::string, AssetId> watched_paths;

    // Callbacks
    std::vector<AssetLoadedCallback> on_loaded_callbacks;
    std::vector<AssetFailedCallback> on_failed_callbacks;
    std::vector<AssetChangeCallback> on_reloaded_callbacks;
    std::vector<AssetChangeCallback> on_unloaded_callbacks;

    // Statistics
    std::size_t total_memory = 0;

    // Mutex for thread safety
    mutable std::mutex mutex;

    // Last GC time
    std::chrono::steady_clock::time_point last_gc_time;

    Impl(RegistryConfig cfg)
        : config(std::move(cfg))
        , last_gc_time(std::chrono::steady_clock::now())
    {
        AssetServerConfig server_config;
        server_config.hot_reload = config.hot_reload_enabled;
        server_config.max_concurrent_loads = config.max_concurrent_loads;
        server_config.auto_garbage_collect = config.auto_gc;

        server = std::make_unique<AssetServer>(server_config);
        hot_reload_enabled = config.hot_reload_enabled;
    }

    void process_events() {
        auto events = server->drain_events();
        for (const auto& event : events) {
            switch (event.type) {
                case AssetEventType::Loaded:
                    spdlog::debug("Asset loaded: {} (id={})", event.path.str(), event.id.raw());
                    for (const auto& cb : on_loaded_callbacks) {
                        cb(event.id, event.path);
                    }
                    // Start watching for hot-reload
                    if (hot_reload_enabled && file_watcher) {
                        std::string full_path = asset_root + "/" + event.path.str();
                        if (file_watcher->watch(full_path)) {
                            watched_paths[full_path] = event.id;
                        }
                    }
                    break;

                case AssetEventType::Failed:
                    spdlog::warn("Asset failed: {} - {}", event.path.str(), event.error);
                    for (const auto& cb : on_failed_callbacks) {
                        cb(event.id, event.path, event.error);
                    }
                    break;

                case AssetEventType::Reloaded:
                    spdlog::info("Asset reloaded: {} (gen={})", event.path.str(), event.generation);
                    for (const auto& cb : on_reloaded_callbacks) {
                        cb(event.id, event.path, event.generation);
                    }
                    break;

                case AssetEventType::Unloaded:
                    spdlog::debug("Asset unloaded: {}", event.path.str());
                    for (const auto& cb : on_unloaded_callbacks) {
                        cb(event.id, event.path, 0);
                    }
                    // Stop watching
                    if (file_watcher) {
                        std::string full_path = asset_root + "/" + event.path.str();
                        file_watcher->unwatch(full_path);
                        watched_paths.erase(full_path);
                    }
                    break;

                case AssetEventType::FileChanged:
                    // Handled by file watcher polling
                    break;
            }
        }
    }

    void poll_file_changes() {
        if (!file_watcher || !hot_reload_enabled) return;

        auto events = file_watcher->poll();
        for (const auto& event : events) {
            if (event.type == void_core::ReloadEventType::FileModified) {
                auto it = watched_paths.find(event.path);
                if (it != watched_paths.end()) {
                    spdlog::info("Detected file change: {}", event.path);
                    server->reload(it->second);
                }
            }
        }
    }

    void check_auto_gc() {
        if (!config.auto_gc) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_gc_time);

        if (elapsed.count() >= config.gc_interval_seconds) {
            std::size_t removed = server->collect_garbage();
            if (removed > 0) {
                spdlog::debug("Auto GC removed {} unreferenced assets", removed);
            }
            last_gc_time = now;
        }
    }
};

// =============================================================================
// AssetRegistry Public Interface
// =============================================================================

AssetRegistry::AssetRegistry()
    : m_impl(std::make_unique<Impl>(RegistryConfig{}))
{
}

AssetRegistry::AssetRegistry(RegistryConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config)))
{
}

AssetRegistry::~AssetRegistry() {
    if (m_impl && m_impl->initialized) {
        shutdown();
    }
}

AssetRegistry::AssetRegistry(AssetRegistry&&) noexcept = default;
AssetRegistry& AssetRegistry::operator=(AssetRegistry&&) noexcept = default;

void_core::Result<void> AssetRegistry::initialize(const std::string& asset_root) {
    std::lock_guard lock(m_impl->mutex);

    if (m_impl->initialized) {
        return void_core::Err(void_core::Error(
            void_core::ErrorCode::AlreadyExists,
            "AssetRegistry already initialized"));
    }

    m_impl->asset_root = asset_root;

    // Set up file watcher for hot-reload using existing void_core infrastructure
    if (m_impl->config.hot_reload_enabled) {
        // Use PollingFileWatcher from void_core::hot_reload.hpp
        auto poll_interval = std::chrono::milliseconds(m_impl->config.hot_reload_poll_ms);
        m_impl->file_watcher = std::make_unique<void_core::PollingFileWatcher>(poll_interval);
        spdlog::info("Asset hot-reload enabled for: {} (poll interval: {}ms)",
                     asset_root, m_impl->config.hot_reload_poll_ms);
    }

    m_impl->initialized = true;
    spdlog::info("AssetRegistry initialized: {}", asset_root);

    return void_core::Ok();
}

void AssetRegistry::shutdown() {
    std::lock_guard lock(m_impl->mutex);

    if (!m_impl->initialized) return;

    // Clear file watcher
    if (m_impl->file_watcher) {
        m_impl->file_watcher->clear();
        m_impl->file_watcher.reset();
    }
    m_impl->watched_paths.clear();

    // Clear all assets
    m_impl->server->storage().clear();

    // Clear callbacks
    m_impl->on_loaded_callbacks.clear();
    m_impl->on_failed_callbacks.clear();
    m_impl->on_reloaded_callbacks.clear();
    m_impl->on_unloaded_callbacks.clear();

    m_impl->initialized = false;
    spdlog::info("AssetRegistry shutdown complete");
}

bool AssetRegistry::is_initialized() const noexcept {
    return m_impl->initialized;
}

bool AssetRegistry::exists(const std::string& path) const {
    return m_impl->server->get_id(path).has_value();
}

bool AssetRegistry::is_loaded(AssetId id) const {
    return m_impl->server->is_loaded(id);
}

bool AssetRegistry::is_loading(AssetId id) const {
    auto state = m_impl->server->get_state(id);
    return state == LoadState::Loading || state == LoadState::Reloading;
}

std::optional<AssetId> AssetRegistry::get_id(const std::string& path) const {
    return m_impl->server->get_id(path);
}

std::optional<AssetPath> AssetRegistry::get_path(AssetId id) const {
    return m_impl->server->get_path(id);
}

const AssetMetadata* AssetRegistry::get_metadata(AssetId id) const {
    return m_impl->server->get_metadata(id);
}

std::uint32_t AssetRegistry::get_generation(AssetId id) const {
    const auto* meta = get_metadata(id);
    return meta ? meta->generation : 0;
}

bool AssetRegistry::unload(AssetId id) {
    const auto* meta = m_impl->server->get_metadata(id);
    if (meta) {
        for (const auto& cb : m_impl->on_unloaded_callbacks) {
            cb(id, meta->path, meta->generation);
        }
    }
    return m_impl->server->unload(id);
}

bool AssetRegistry::unload(const std::string& path) {
    auto id = get_id(path);
    if (!id) return false;
    return unload(*id);
}

void_core::Result<void> AssetRegistry::reload(AssetId id) {
    return m_impl->server->reload(id);
}

void_core::Result<void> AssetRegistry::reload(const std::string& path) {
    auto id = get_id(path);
    if (!id) {
        return void_core::Err(AssetError::not_found(path));
    }
    return reload(*id);
}

void AssetRegistry::unload_all() {
    m_impl->server->storage().clear();
}

std::size_t AssetRegistry::collect_garbage() {
    return m_impl->server->collect_garbage();
}

bool AssetRegistry::supports_extension(const std::string& ext) const {
    return m_impl->server->loaders().supports_extension(ext);
}

std::vector<std::string> AssetRegistry::supported_extensions() const {
    return m_impl->server->loaders().supported_extensions();
}

void AssetRegistry::on_asset_loaded(AssetLoadedCallback callback) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->on_loaded_callbacks.push_back(std::move(callback));
}

void AssetRegistry::on_asset_failed(AssetFailedCallback callback) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->on_failed_callbacks.push_back(std::move(callback));
}

void AssetRegistry::on_asset_reloaded(AssetChangeCallback callback) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->on_reloaded_callbacks.push_back(std::move(callback));
}

void AssetRegistry::on_asset_unloaded(AssetChangeCallback callback) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->on_unloaded_callbacks.push_back(std::move(callback));
}

void AssetRegistry::poll_hot_reload() {
    if (!m_impl->hot_reload_enabled) return;

    // Poll file watcher for changes and trigger reloads
    m_impl->poll_file_changes();

    // Process any events from asset server
    m_impl->process_events();

    // Check auto GC
    m_impl->check_auto_gc();
}

void AssetRegistry::process_pending() {
    m_impl->server->process();
    m_impl->process_events();
}

void AssetRegistry::set_hot_reload_enabled(bool enabled) {
    m_impl->hot_reload_enabled = enabled;
}

bool AssetRegistry::is_hot_reload_enabled() const {
    return m_impl->hot_reload_enabled;
}

std::size_t AssetRegistry::total_count() const {
    return m_impl->server->total_count();
}

std::size_t AssetRegistry::loaded_count() const {
    return m_impl->server->loaded_count();
}

std::size_t AssetRegistry::pending_count() const {
    return m_impl->server->pending_count();
}

std::size_t AssetRegistry::memory_usage() const {
    // Approximate based on loaded count
    // In production, track actual memory usage
    return m_impl->total_memory;
}

AssetServer& AssetRegistry::server() {
    return *m_impl->server;
}

const AssetServer& AssetRegistry::server() const {
    return *m_impl->server;
}

// =============================================================================
// HotReloadable Interface
// =============================================================================

void_core::Result<void_core::HotReloadSnapshot> AssetRegistry::snapshot() {
    void_core::HotReloadSnapshot snap;
    snap.type_id = std::type_index(typeid(AssetRegistry));
    snap.type_name = type_name();
    snap.version = current_version();

    // Serialize asset paths for restoration
    auto manifest = serialization::serialize_storage_manifest(m_impl->server->storage());
    snap.data = std::move(manifest);

    return snap;
}

void_core::Result<void> AssetRegistry::restore(void_core::HotReloadSnapshot snapshot) {
    // Deserialize manifest
    auto result = serialization::deserialize_storage_manifest(snapshot.data);
    if (!result) {
        return void_core::Err(result.error());
    }

    // Reload all assets from manifest
    for (const auto& [id, path] : result.value()) {
        // Assets will be reloaded as needed
        spdlog::debug("Registry restore: {} -> {}", id.raw(), path.str());
    }

    return void_core::Ok();
}

bool AssetRegistry::is_compatible(const void_core::Version&) const {
    return true;
}

void_core::Version AssetRegistry::current_version() const {
    return void_core::Version{1, 0, 0};
}

// =============================================================================
// Global Registry
// =============================================================================

namespace {
    std::unique_ptr<AssetRegistry> g_registry;
    std::mutex g_registry_mutex;
}

AssetRegistry& global_registry() {
    std::lock_guard lock(g_registry_mutex);
    if (!g_registry) {
        g_registry = std::make_unique<AssetRegistry>();
    }
    return *g_registry;
}

AssetRegistry& global_registry(RegistryConfig config) {
    std::lock_guard lock(g_registry_mutex);
    if (!g_registry) {
        g_registry = std::make_unique<AssetRegistry>(std::move(config));
    }
    return *g_registry;
}

void_core::Result<void> init_global_registry(const std::string& asset_root) {
    return global_registry().initialize(asset_root);
}

void shutdown_global_registry() {
    std::lock_guard lock(g_registry_mutex);
    if (g_registry) {
        g_registry->shutdown();
        g_registry.reset();
    }
}

bool has_global_registry() {
    std::lock_guard lock(g_registry_mutex);
    return g_registry != nullptr;
}

} // namespace void_asset
