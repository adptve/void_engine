#pragma once

/// @file hot_reload.hpp
/// @brief Asset hot-reload system for void_asset

#include "fwd.hpp"
#include "types.hpp"
#include "server.hpp"
#include <void_engine/core/hot_reload.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <mutex>
#include <functional>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <thread>

namespace void_asset {

// =============================================================================
// AssetChangeEvent
// =============================================================================

/// Type of file change
enum class FileChangeType : std::uint8_t {
    Created,
    Modified,
    Deleted,
    Renamed,
};

/// Get file change type name
[[nodiscard]] inline const char* file_change_type_name(FileChangeType type) {
    switch (type) {
        case FileChangeType::Created: return "Created";
        case FileChangeType::Modified: return "Modified";
        case FileChangeType::Deleted: return "Deleted";
        case FileChangeType::Renamed: return "Renamed";
        default: return "Unknown";
    }
}

/// Asset file change event
struct AssetChangeEvent {
    FileChangeType type;
    AssetPath path;
    AssetPath old_path;  // For rename events
    std::chrono::steady_clock::time_point timestamp;

    /// Default constructor
    AssetChangeEvent()
        : type(FileChangeType::Modified)
        , timestamp(std::chrono::steady_clock::now()) {}

    /// Construct with values
    AssetChangeEvent(FileChangeType t, AssetPath p)
        : type(t)
        , path(std::move(p))
        , timestamp(std::chrono::steady_clock::now()) {}

    /// Factory methods
    [[nodiscard]] static AssetChangeEvent created(const AssetPath& path) {
        return AssetChangeEvent{FileChangeType::Created, path};
    }

    [[nodiscard]] static AssetChangeEvent modified(const AssetPath& path) {
        return AssetChangeEvent{FileChangeType::Modified, path};
    }

    [[nodiscard]] static AssetChangeEvent deleted(const AssetPath& path) {
        return AssetChangeEvent{FileChangeType::Deleted, path};
    }

    [[nodiscard]] static AssetChangeEvent renamed(const AssetPath& old_path, const AssetPath& new_path) {
        AssetChangeEvent event{FileChangeType::Renamed, new_path};
        event.old_path = old_path;
        return event;
    }
};

// =============================================================================
// AssetReloadResult
// =============================================================================

/// Result of reloading an asset
struct AssetReloadResult {
    AssetId id;
    AssetPath path;
    bool success = false;
    std::string error;
    std::chrono::milliseconds duration{0};
    std::uint32_t new_generation = 0;

    /// Default constructor
    AssetReloadResult() = default;

    /// Construct success result
    [[nodiscard]] static AssetReloadResult ok(AssetId id, const AssetPath& path,
                                               std::uint32_t gen, std::chrono::milliseconds dur) {
        AssetReloadResult result;
        result.id = id;
        result.path = path;
        result.success = true;
        result.new_generation = gen;
        result.duration = dur;
        return result;
    }

    /// Construct failure result
    [[nodiscard]] static AssetReloadResult failed(AssetId id, const AssetPath& path,
                                                   const std::string& err) {
        AssetReloadResult result;
        result.id = id;
        result.path = path;
        result.success = false;
        result.error = err;
        return result;
    }
};

// =============================================================================
// FileModificationTracker
// =============================================================================

/// Tracks file modification times for change detection
class FileModificationTracker {
public:
    /// Constructor
    FileModificationTracker() = default;

    /// Update tracked file
    bool update(const std::string& path) {
        std::error_code ec;
        auto write_time = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return false;
        }

        std::lock_guard lock(m_mutex);
        auto it = m_modification_times.find(path);
        if (it == m_modification_times.end()) {
            m_modification_times[path] = write_time;
            return true;  // New file
        }

        if (it->second != write_time) {
            it->second = write_time;
            return true;  // Modified
        }

        return false;  // Unchanged
    }

    /// Check if file was modified
    [[nodiscard]] bool is_modified(const std::string& path) const {
        std::error_code ec;
        auto write_time = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return false;
        }

        std::lock_guard lock(m_mutex);
        auto it = m_modification_times.find(path);
        if (it == m_modification_times.end()) {
            return true;  // New file
        }

        return it->second != write_time;
    }

    /// Remove tracked file
    void remove(const std::string& path) {
        std::lock_guard lock(m_mutex);
        m_modification_times.erase(path);
    }

    /// Clear all tracked files
    void clear() {
        std::lock_guard lock(m_mutex);
        m_modification_times.clear();
    }

    /// Get tracked file count
    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_modification_times.size();
    }

private:
    std::map<std::string, std::filesystem::file_time_type> m_modification_times;
    mutable std::mutex m_mutex;
};

// =============================================================================
// AssetWatcher
// =============================================================================

/// Callback for asset changes
using AssetChangeCallback = std::function<void(const AssetChangeEvent&)>;

/// Interface for watching asset files
class AssetWatcher {
public:
    virtual ~AssetWatcher() = default;

    /// Start watching
    virtual void start() = 0;

    /// Stop watching
    virtual void stop() = 0;

    /// Check if watching
    [[nodiscard]] virtual bool is_watching() const = 0;

    /// Poll for changes
    virtual std::vector<AssetChangeEvent> poll() = 0;

    /// Add watch path
    virtual void add_path(const std::string& path) = 0;

    /// Remove watch path
    virtual void remove_path(const std::string& path) = 0;

    /// Add extension filter
    virtual void add_extension(const std::string& ext) = 0;

    /// Set change callback
    virtual void set_callback(AssetChangeCallback callback) = 0;
};

// =============================================================================
// PollingAssetWatcher
// =============================================================================

/// Polling-based asset watcher
class PollingAssetWatcher : public AssetWatcher {
public:
    /// Constructor
    explicit PollingAssetWatcher(std::chrono::milliseconds interval = std::chrono::milliseconds{100})
        : m_poll_interval(interval) {}

    /// Destructor
    ~PollingAssetWatcher() override {
        stop();
    }

    /// Start watching
    void start() override {
        if (m_watching.exchange(true)) {
            return;
        }

        m_thread = std::thread([this]() {
            while (m_watching.load()) {
                check_changes();
                std::this_thread::sleep_for(m_poll_interval);
            }
        });
    }

    /// Stop watching
    void stop() override {
        m_watching.store(false);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    /// Check if watching
    [[nodiscard]] bool is_watching() const override {
        return m_watching.load();
    }

    /// Poll for changes
    std::vector<AssetChangeEvent> poll() override {
        std::lock_guard lock(m_events_mutex);
        std::vector<AssetChangeEvent> events;
        std::swap(events, m_events);
        return events;
    }

    /// Add watch path
    void add_path(const std::string& path) override {
        std::lock_guard lock(m_paths_mutex);
        m_watch_paths.insert(path);

        // Scan directory for initial state
        scan_directory(path);
    }

    /// Remove watch path
    void remove_path(const std::string& path) override {
        std::lock_guard lock(m_paths_mutex);
        m_watch_paths.erase(path);
    }

    /// Set change callback
    void set_callback(AssetChangeCallback callback) override {
        std::lock_guard lock(m_callback_mutex);
        m_callback = std::move(callback);
    }

    /// Set poll interval
    void set_poll_interval(std::chrono::milliseconds interval) {
        m_poll_interval = interval;
    }

    /// Get extensions filter
    [[nodiscard]] const std::set<std::string>& extensions() const {
        return m_extensions;
    }

    /// Add extension filter
    void add_extension(const std::string& ext) override {
        m_extensions.insert(ext);
    }

    /// Clear extension filter (watch all files)
    void clear_extensions() {
        m_extensions.clear();
    }

private:
    void scan_directory(const std::string& dir) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string path = entry.path().string();
            if (!should_watch(path)) {
                continue;
            }

            m_tracker.update(path);
            m_known_files.insert(path);
        }
    }

    void check_changes() {
        std::set<std::string> current_files;
        std::vector<AssetChangeEvent> new_events;

        std::unique_lock paths_lock(m_paths_mutex);
        auto paths = m_watch_paths;
        paths_lock.unlock();

        for (const auto& dir : paths) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                std::string path = entry.path().string();
                if (!should_watch(path)) {
                    continue;
                }

                current_files.insert(path);

                // Check for new or modified files
                if (m_known_files.find(path) == m_known_files.end()) {
                    // New file
                    new_events.push_back(AssetChangeEvent::created(AssetPath(path)));
                    m_tracker.update(path);
                } else if (m_tracker.is_modified(path)) {
                    // Modified file
                    new_events.push_back(AssetChangeEvent::modified(AssetPath(path)));
                    m_tracker.update(path);
                }
            }
        }

        // Check for deleted files
        for (const auto& path : m_known_files) {
            if (current_files.find(path) == current_files.end()) {
                new_events.push_back(AssetChangeEvent::deleted(AssetPath(path)));
                m_tracker.remove(path);
            }
        }

        m_known_files = std::move(current_files);

        // Queue events and call callback
        if (!new_events.empty()) {
            AssetChangeCallback callback;
            {
                std::lock_guard lock(m_callback_mutex);
                callback = m_callback;
            }

            {
                std::lock_guard lock(m_events_mutex);
                for (auto& event : new_events) {
                    m_events.push_back(event);
                    if (callback) {
                        callback(event);
                    }
                }
            }
        }
    }

    [[nodiscard]] bool should_watch(const std::string& path) const {
        if (m_extensions.empty()) {
            return true;
        }

        auto pos = path.rfind('.');
        if (pos == std::string::npos) {
            return false;
        }

        std::string ext = path.substr(pos + 1);
        return m_extensions.find(ext) != m_extensions.end();
    }

    std::chrono::milliseconds m_poll_interval;
    std::atomic<bool> m_watching{false};
    std::thread m_thread;

    std::set<std::string> m_watch_paths;
    mutable std::mutex m_paths_mutex;

    std::set<std::string> m_extensions;
    std::set<std::string> m_known_files;
    FileModificationTracker m_tracker;

    std::vector<AssetChangeEvent> m_events;
    mutable std::mutex m_events_mutex;

    AssetChangeCallback m_callback;
    mutable std::mutex m_callback_mutex;
};

// =============================================================================
// AssetHotReloadConfig
// =============================================================================

/// Configuration for asset hot-reload
struct AssetHotReloadConfig {
    bool enabled = true;
    std::chrono::milliseconds poll_interval{100};
    std::chrono::milliseconds debounce_time{50};
    bool reload_dependencies = true;
    bool notify_on_failure = true;
    std::size_t max_concurrent_reloads = 4;

    /// Default constructor
    AssetHotReloadConfig() = default;

    /// Builder pattern
    AssetHotReloadConfig& with_enabled(bool enable) {
        enabled = enable;
        return *this;
    }

    AssetHotReloadConfig& with_poll_interval(std::chrono::milliseconds interval) {
        poll_interval = interval;
        return *this;
    }

    AssetHotReloadConfig& with_debounce_time(std::chrono::milliseconds time) {
        debounce_time = time;
        return *this;
    }

    AssetHotReloadConfig& with_reload_dependencies(bool reload) {
        reload_dependencies = reload;
        return *this;
    }
};

// =============================================================================
// AssetHotReloadManager
// =============================================================================

/// Callback for reload events
using ReloadCallback = std::function<void(const AssetReloadResult&)>;

/// Manages hot-reloading of assets
class AssetHotReloadManager {
public:
    /// Constructor
    explicit AssetHotReloadManager(AssetServer& server, AssetHotReloadConfig config = {})
        : m_server(server)
        , m_config(std::move(config))
        , m_watcher(std::make_unique<PollingAssetWatcher>(m_config.poll_interval))
    {
        // Add asset directory to watch
        m_watcher->add_path(m_server.config().asset_dir);

        // Get supported extensions from loaders
        for (const auto& ext : m_server.loaders().supported_extensions()) {
            m_watcher->add_extension(ext);
        }
    }

    /// Destructor
    ~AssetHotReloadManager() {
        stop();
    }

    /// Start hot-reload monitoring
    void start() {
        if (!m_config.enabled || m_running.exchange(true)) {
            return;
        }

        m_watcher->start();
    }

    /// Stop hot-reload monitoring
    void stop() {
        m_running.store(false);
        m_watcher->stop();
    }

    /// Check if running
    [[nodiscard]] bool is_running() const {
        return m_running.load();
    }

    /// Process pending changes
    void process() {
        if (!m_running.load()) {
            return;
        }

        auto changes = m_watcher->poll();
        auto now = std::chrono::steady_clock::now();

        for (const auto& change : changes) {
            // Debounce: skip if too recent
            auto it = m_last_change.find(change.path.str());
            if (it != m_last_change.end()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - it->second);
                if (elapsed < m_config.debounce_time) {
                    continue;
                }
            }
            m_last_change[change.path.str()] = now;

            handle_change(change);
        }
    }

    /// Manually trigger reload for path
    AssetReloadResult reload(const std::string& path) {
        auto id = m_server.get_id(path);
        if (!id) {
            return AssetReloadResult::failed(AssetId::invalid(), AssetPath(path),
                                              "Asset not found");
        }

        return reload_asset(*id);
    }

    /// Manually trigger reload for ID
    AssetReloadResult reload(AssetId id) {
        return reload_asset(id);
    }

    /// Set reload callback
    void set_callback(ReloadCallback callback) {
        std::lock_guard lock(m_callback_mutex);
        m_callback = std::move(callback);
    }

    /// Get reload history
    [[nodiscard]] std::vector<AssetReloadResult> drain_results() {
        std::lock_guard lock(m_results_mutex);
        std::vector<AssetReloadResult> results;
        std::swap(results, m_results);
        return results;
    }

    /// Get pending reload count
    [[nodiscard]] std::size_t pending_count() const {
        std::lock_guard lock(m_pending_mutex);
        return m_pending_reloads.size();
    }

    /// Get config
    [[nodiscard]] const AssetHotReloadConfig& config() const {
        return m_config;
    }

    /// Get watcher
    [[nodiscard]] AssetWatcher& watcher() {
        return *m_watcher;
    }

private:
    void handle_change(const AssetChangeEvent& change) {
        switch (change.type) {
            case FileChangeType::Modified:
            case FileChangeType::Created: {
                // Find asset by path and reload
                auto id = m_server.get_id(change.path.str());
                if (id) {
                    queue_reload(*id);
                }
                break;
            }
            case FileChangeType::Deleted: {
                // Optionally handle deleted files
                auto id = m_server.get_id(change.path.str());
                if (id) {
                    // Mark as failed or unload
                    m_server.unload(*id);
                }
                break;
            }
            case FileChangeType::Renamed: {
                // Handle rename as delete old + create new
                auto old_id = m_server.get_id(change.old_path.str());
                if (old_id) {
                    m_server.unload(*old_id);
                }
                break;
            }
        }
    }

    void queue_reload(AssetId id) {
        std::lock_guard lock(m_pending_mutex);
        m_pending_reloads.insert(id);
    }

    AssetReloadResult reload_asset(AssetId id) {
        auto start_time = std::chrono::steady_clock::now();

        auto path = m_server.get_path(id);
        if (!path) {
            return AssetReloadResult::failed(id, AssetPath(), "Asset not found");
        }

        auto result = m_server.reload(id);

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        AssetReloadResult reload_result;
        if (result) {
            const auto* meta = m_server.get_metadata(id);
            reload_result = AssetReloadResult::ok(id, *path,
                meta ? meta->generation : 0, duration);
        } else {
            reload_result = AssetReloadResult::failed(id, *path, result.error().message());
        }

        // Store result
        {
            std::lock_guard lock(m_results_mutex);
            m_results.push_back(reload_result);
        }

        // Call callback
        {
            std::lock_guard lock(m_callback_mutex);
            if (m_callback) {
                m_callback(reload_result);
            }
        }

        // Reload dependents if enabled
        if (m_config.reload_dependencies && reload_result.success) {
            const auto* meta = m_server.get_metadata(id);
            if (meta) {
                for (const auto& dep_id : meta->dependents) {
                    queue_reload(dep_id);
                }
            }
        }

        return reload_result;
    }

    AssetServer& m_server;
    AssetHotReloadConfig m_config;
    std::unique_ptr<AssetWatcher> m_watcher;

    std::atomic<bool> m_running{false};

    std::set<AssetId> m_pending_reloads;
    mutable std::mutex m_pending_mutex;

    std::map<std::string, std::chrono::steady_clock::time_point> m_last_change;

    std::vector<AssetReloadResult> m_results;
    mutable std::mutex m_results_mutex;

    ReloadCallback m_callback;
    mutable std::mutex m_callback_mutex;
};

// =============================================================================
// AssetHotReloadSystem
// =============================================================================

/// High-level system combining server and hot-reload
class AssetHotReloadSystem {
public:
    /// Constructor
    explicit AssetHotReloadSystem(
        AssetServerConfig server_config = {},
        AssetHotReloadConfig reload_config = {})
        : m_server(std::move(server_config))
        , m_reload_manager(m_server, std::move(reload_config))
    {}

    /// Start the system
    void start() {
        m_reload_manager.start();
    }

    /// Stop the system
    void stop() {
        m_reload_manager.stop();
    }

    /// Process all pending work
    void process() {
        m_server.process();
        m_reload_manager.process();
    }

    /// Load asset
    template<typename T>
    [[nodiscard]] Handle<T> load(const std::string& path) {
        return m_server.load<T>(path);
    }

    /// Unload asset
    bool unload(AssetId id) {
        return m_server.unload(id);
    }

    /// Reload asset
    AssetReloadResult reload(AssetId id) {
        return m_reload_manager.reload(id);
    }

    /// Get server
    [[nodiscard]] AssetServer& server() { return m_server; }
    [[nodiscard]] const AssetServer& server() const { return m_server; }

    /// Get reload manager
    [[nodiscard]] AssetHotReloadManager& reload_manager() { return m_reload_manager; }
    [[nodiscard]] const AssetHotReloadManager& reload_manager() const { return m_reload_manager; }

    /// Register loader (base type)
    template<typename T>
    void register_loader(std::unique_ptr<AssetLoader<T>> loader) {
        // Add extension to watcher
        for (const auto& ext : loader->extensions()) {
            dynamic_cast<PollingAssetWatcher&>(m_reload_manager.watcher()).add_extension(ext);
        }
        m_server.register_loader(std::move(loader));
    }

    /// Register derived loader type (automatically extracts asset type from loader)
    template<typename Derived,
             typename T = typename Derived::asset_type,
             typename = std::enable_if_t<std::is_base_of_v<AssetLoader<T>, Derived>>>
    void register_loader(std::unique_ptr<Derived> loader) {
        register_loader<T>(std::unique_ptr<AssetLoader<T>>(std::move(loader)));
    }

    /// Drain all events
    std::vector<AssetEvent> drain_events() {
        return m_server.drain_events();
    }

    /// Drain reload results
    std::vector<AssetReloadResult> drain_reload_results() {
        return m_reload_manager.drain_results();
    }

private:
    AssetServer m_server;
    AssetHotReloadManager m_reload_manager;
};

} // namespace void_asset
