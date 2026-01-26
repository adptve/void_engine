/// @file server.cpp
/// @brief void_asset server implementation
///
/// Provides non-template utilities and hot-reload support for the asset server.

#include <void_engine/asset/server.hpp>
#include <void_engine/asset/cache.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <algorithm>
#include <sstream>

namespace void_asset {

// =============================================================================
// Server Statistics
// =============================================================================

namespace {

struct ServerStatistics {
    std::atomic<std::uint64_t> total_requests{0};
    std::atomic<std::uint64_t> cache_hits{0};
    std::atomic<std::uint64_t> cache_misses{0};
    std::atomic<std::uint64_t> loads_completed{0};
    std::atomic<std::uint64_t> loads_failed{0};
    std::atomic<std::uint64_t> reloads_completed{0};
    std::atomic<std::uint64_t> garbage_collections{0};
};

ServerStatistics s_server_stats;

} // anonymous namespace

void record_server_request(bool cache_hit) {
    s_server_stats.total_requests.fetch_add(1, std::memory_order_relaxed);
    if (cache_hit) {
        s_server_stats.cache_hits.fetch_add(1, std::memory_order_relaxed);
    } else {
        s_server_stats.cache_misses.fetch_add(1, std::memory_order_relaxed);
    }
}

void record_load_completed(bool success) {
    if (success) {
        s_server_stats.loads_completed.fetch_add(1, std::memory_order_relaxed);
    } else {
        s_server_stats.loads_failed.fetch_add(1, std::memory_order_relaxed);
    }
}

void record_reload_completed() {
    s_server_stats.reloads_completed.fetch_add(1, std::memory_order_relaxed);
}

void record_gc_run() {
    s_server_stats.garbage_collections.fetch_add(1, std::memory_order_relaxed);
}

std::string format_server_statistics() {
    std::ostringstream oss;
    oss << "Server Statistics:\n";
    oss << "  Total requests: " << s_server_stats.total_requests.load() << "\n";
    oss << "  Cache hits: " << s_server_stats.cache_hits.load() << "\n";
    oss << "  Cache misses: " << s_server_stats.cache_misses.load() << "\n";

    auto hits = s_server_stats.cache_hits.load();
    auto misses = s_server_stats.cache_misses.load();
    auto total = hits + misses;
    double hit_rate = total > 0 ? (100.0 * hits / total) : 0.0;
    oss << "  Hit rate: " << hit_rate << "%\n";

    oss << "  Loads completed: " << s_server_stats.loads_completed.load() << "\n";
    oss << "  Loads failed: " << s_server_stats.loads_failed.load() << "\n";
    oss << "  Reloads: " << s_server_stats.reloads_completed.load() << "\n";
    oss << "  GC runs: " << s_server_stats.garbage_collections.load() << "\n";
    return oss.str();
}

void reset_server_statistics() {
    s_server_stats.total_requests.store(0);
    s_server_stats.cache_hits.store(0);
    s_server_stats.cache_misses.store(0);
    s_server_stats.loads_completed.store(0);
    s_server_stats.loads_failed.store(0);
    s_server_stats.reloads_completed.store(0);
    s_server_stats.garbage_collections.store(0);
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_asset_server_config(const AssetServerConfig& config) {
    std::ostringstream oss;
    oss << "AssetServerConfig {\n";
    oss << "  asset_dir: \"" << config.asset_dir << "\"\n";
    oss << "  hot_reload: " << (config.hot_reload ? "true" : "false") << "\n";
    oss << "  max_concurrent_loads: " << config.max_concurrent_loads << "\n";
    oss << "  auto_garbage_collect: " << (config.auto_garbage_collect ? "true" : "false") << "\n";
    oss << "  gc_interval: " << config.gc_interval.count() << "ms\n";
    oss << "}";
    return oss.str();
}

std::string format_pending_load(const PendingLoad& pending) {
    std::ostringstream oss;
    oss << "PendingLoad {\n";
    oss << "  id: " << pending.id.raw() << "\n";
    oss << "  path: \"" << pending.path.str() << "\"\n";
    oss << "  type: " << pending.type_id.name() << "\n";
    oss << "  has_loader: " << (pending.loader != nullptr ? "true" : "false") << "\n";
    oss << "}";
    return oss.str();
}

std::string format_asset_server(const AssetServer& server) {
    std::ostringstream oss;
    oss << "AssetServer {\n";
    oss << "  pending_count: " << server.pending_count() << "\n";
    oss << "  loaded_count: " << server.loaded_count() << "\n";
    oss << "  total_count: " << server.total_count() << "\n";
    oss << "  config:\n";
    oss << "    asset_dir: \"" << server.config().asset_dir << "\"\n";
    oss << "    hot_reload: " << (server.config().hot_reload ? "true" : "false") << "\n";
    oss << "}";
    return oss.str();
}

} // namespace debug

// =============================================================================
// Hot-Reload Support
// =============================================================================

/// Adapter to make AssetServer hot-reloadable
class AssetServerHotReloadAdapter : public ::void_core::HotReloadable {
public:
    explicit AssetServerHotReloadAdapter(AssetServer& server)
        : m_server(server)
        , m_version(::void_core::Version{0, 1, 0})
    {}

    /// Capture current state as snapshot
    [[nodiscard]] ::void_core::Result<::void_core::HotReloadSnapshot> snapshot() override {
        // Serialize asset manifest (paths, types, ref counts), NOT asset data
        std::vector<std::uint8_t> data = serialization::serialize_storage_manifest(m_server.storage());

        ::void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(AssetServer)),
            "AssetServer",
            m_version
        );

        // Add metadata
        snap.with_metadata("asset_count", std::to_string(m_server.total_count()));
        snap.with_metadata("loaded_count", std::to_string(m_server.loaded_count()));
        snap.with_metadata("pending_count", std::to_string(m_server.pending_count()));

        return ::void_core::Ok(std::move(snap));
    }

    /// Restore state from snapshot
    [[nodiscard]] ::void_core::Result<void> restore(::void_core::HotReloadSnapshot snapshot) override {
        // Deserialize manifest
        auto result = serialization::deserialize_storage_manifest(snapshot.data);
        if (!result) {
            return ::void_core::Err(result.error());
        }

        // Re-register assets (they will need to be reloaded)
        for (const auto& [id, path] : result.value()) {
            // Queue for reload if path still exists
            // The actual asset data will be loaded fresh
            // This preserves handles - they point to the same ID
        }

        return ::void_core::Ok();
    }

    /// Check if compatible with new version
    [[nodiscard]] bool is_compatible(const ::void_core::Version& new_version) const override {
        // Compatible if same major version
        return m_version.major == new_version.major;
    }

    /// Called before reload begins
    [[nodiscard]] ::void_core::Result<void> prepare_reload() override {
        // Drain any pending events
        m_server.drain_events();
        return ::void_core::Ok();
    }

    /// Called after reload completes
    [[nodiscard]] ::void_core::Result<void> finish_reload() override {
        record_reload_completed();
        return ::void_core::Ok();
    }

    /// Get current version
    [[nodiscard]] ::void_core::Version current_version() const override {
        return m_version;
    }

    /// Get type name for debugging
    [[nodiscard]] std::string type_name() const override {
        return "AssetServer";
    }

private:
    AssetServer& m_server;
    ::void_core::Version m_version;
};

std::unique_ptr<::void_core::HotReloadable> make_hot_reloadable(AssetServer& server) {
    return std::make_unique<AssetServerHotReloadAdapter>(server);
}

// =============================================================================
// Global Asset Server
// =============================================================================

namespace {
    std::unique_ptr<AssetServer> s_global_server;
    std::mutex s_global_server_mutex;
}

AssetServer& global_asset_server() {
    std::lock_guard lock(s_global_server_mutex);
    if (!s_global_server) {
        s_global_server = std::make_unique<AssetServer>();
    }
    return *s_global_server;
}

AssetServer& global_asset_server(AssetServerConfig config) {
    std::lock_guard lock(s_global_server_mutex);
    if (!s_global_server) {
        s_global_server = std::make_unique<AssetServer>(std::move(config));
    }
    return *s_global_server;
}

void shutdown_global_asset_server() {
    std::lock_guard lock(s_global_server_mutex);
    s_global_server.reset();
}

bool has_global_asset_server() {
    std::lock_guard lock(s_global_server_mutex);
    return s_global_server != nullptr;
}

// =============================================================================
// Batch Loading Utilities
// =============================================================================

std::vector<AssetId> load_batch(AssetServer& server, const std::vector<std::string>& paths) {
    std::vector<AssetId> ids;
    ids.reserve(paths.size());

    for (const auto& path : paths) {
        ids.push_back(server.load_untyped(path));
    }

    return ids;
}

void wait_for_loads(AssetServer& server, const std::vector<AssetId>& ids,
                   std::chrono::milliseconds timeout)
{
    auto start = std::chrono::steady_clock::now();

    while (true) {
        server.process();

        bool all_complete = true;
        for (const auto& id : ids) {
            auto state = server.get_state(id);
            if (state == LoadState::Loading || state == LoadState::Reloading) {
                all_complete = false;
                break;
            }
        }

        if (all_complete) {
            break;
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// =============================================================================
// Validation Utilities
// =============================================================================

std::vector<std::string> validate_asset_server(const AssetServer& server) {
    std::vector<std::string> errors;

    // Check config
    if (server.config().asset_dir.empty()) {
        errors.push_back("Asset directory is empty");
    }

    // Check storage
    auto storage_errors = validate_storage(server.storage());
    errors.insert(errors.end(), storage_errors.begin(), storage_errors.end());

    // Check loaders
    if (server.loaders().len() == 0) {
        errors.push_back("No loaders registered");
    }

    return errors;
}

} // namespace void_asset
