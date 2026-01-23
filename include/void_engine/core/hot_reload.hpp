#pragma once

/// @file hot_reload.hpp
/// @brief Hot-reload infrastructure for void_core

#include "fwd.hpp"
#include "error.hpp"
#include "version.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <typeindex>
#include <functional>
#include <queue>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <set>

namespace void_core {

// =============================================================================
// ReloadEvent
// =============================================================================

/// Types of reload events
enum class ReloadEventType : std::uint8_t {
    FileModified,    // File content changed
    FileCreated,     // New file detected
    FileDeleted,     // File removed
    FileRenamed,     // File renamed
    ForceReload,     // Manual reload request
};

/// Get event type name
[[nodiscard]] inline const char* reload_event_type_name(ReloadEventType type) {
    switch (type) {
        case ReloadEventType::FileModified: return "FileModified";
        case ReloadEventType::FileCreated: return "FileCreated";
        case ReloadEventType::FileDeleted: return "FileDeleted";
        case ReloadEventType::FileRenamed: return "FileRenamed";
        case ReloadEventType::ForceReload: return "ForceReload";
        default: return "Unknown";
    }
}

/// A reload event from the file watcher
struct ReloadEvent {
    ReloadEventType type = ReloadEventType::FileModified;
    std::string path;
    std::string old_path;  // For rename events
    std::chrono::steady_clock::time_point timestamp;

    /// Default constructor
    ReloadEvent() : timestamp(std::chrono::steady_clock::now()) {}

    /// Construct with type and path
    ReloadEvent(ReloadEventType t, std::string p)
        : type(t), path(std::move(p)), timestamp(std::chrono::steady_clock::now()) {}

    /// Construct rename event
    ReloadEvent(std::string old_p, std::string new_p)
        : type(ReloadEventType::FileRenamed)
        , path(std::move(new_p))
        , old_path(std::move(old_p))
        , timestamp(std::chrono::steady_clock::now()) {}

    /// Factory methods
    [[nodiscard]] static ReloadEvent modified(const std::string& path) {
        return ReloadEvent{ReloadEventType::FileModified, path};
    }

    [[nodiscard]] static ReloadEvent created(const std::string& path) {
        return ReloadEvent{ReloadEventType::FileCreated, path};
    }

    [[nodiscard]] static ReloadEvent deleted(const std::string& path) {
        return ReloadEvent{ReloadEventType::FileDeleted, path};
    }

    [[nodiscard]] static ReloadEvent renamed(const std::string& old_path, const std::string& new_path) {
        return ReloadEvent{old_path, new_path};
    }

    [[nodiscard]] static ReloadEvent force_reload(const std::string& path) {
        return ReloadEvent{ReloadEventType::ForceReload, path};
    }
};

// =============================================================================
// HotReloadSnapshot
// =============================================================================

/// Captured state for hot-reload restoration
struct HotReloadSnapshot {
    std::vector<std::uint8_t> data;           // Serialized state
    std::type_index type_id;                   // Original type
    std::string type_name;                     // Readable type name
    Version version;                           // Version at snapshot time
    std::map<std::string, std::string> metadata;  // Additional metadata

    /// Default constructor
    HotReloadSnapshot()
        : type_id(typeid(void))
        , version(Version::zero()) {}

    /// Construct with data
    HotReloadSnapshot(
        std::vector<std::uint8_t> d,
        std::type_index tid,
        std::string tname,
        Version v)
        : data(std::move(d))
        , type_id(tid)
        , type_name(std::move(tname))
        , version(v) {}

    /// Create empty snapshot
    [[nodiscard]] static HotReloadSnapshot empty() {
        return HotReloadSnapshot{};
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return data.empty();
    }

    /// Add metadata
    HotReloadSnapshot& with_metadata(const std::string& key, const std::string& value) {
        metadata[key] = value;
        return *this;
    }

    /// Get metadata
    [[nodiscard]] const std::string* get_metadata(const std::string& key) const {
        auto it = metadata.find(key);
        return it != metadata.end() ? &it->second : nullptr;
    }

    /// Check type compatibility
    template<typename T>
    [[nodiscard]] bool is_type() const {
        return type_id == std::type_index(typeid(T));
    }
};

// =============================================================================
// HotReloadable (Interface)
// =============================================================================

/// Interface for objects that support hot-reload
class HotReloadable {
public:
    virtual ~HotReloadable() = default;

    /// Capture current state as snapshot
    [[nodiscard]] virtual Result<HotReloadSnapshot> snapshot() = 0;

    /// Restore state from snapshot
    [[nodiscard]] virtual Result<void> restore(HotReloadSnapshot snapshot) = 0;

    /// Check if compatible with new version
    [[nodiscard]] virtual bool is_compatible(const Version& new_version) const = 0;

    /// Called before reload begins (optional cleanup)
    [[nodiscard]] virtual Result<void> prepare_reload() {
        return Ok();
    }

    /// Called after reload completes (optional finalization)
    [[nodiscard]] virtual Result<void> finish_reload() {
        return Ok();
    }

    /// Get current version
    [[nodiscard]] virtual Version current_version() const = 0;

    /// Get type name for debugging
    [[nodiscard]] virtual std::string type_name() const = 0;
};

// =============================================================================
// HotReloadEntry
// =============================================================================

/// Entry in the hot-reload registry
struct HotReloadEntry {
    std::unique_ptr<HotReloadable> object;
    std::string source_path;
    Version version;
    bool pending_reload = false;
    std::chrono::steady_clock::time_point last_reload;

    HotReloadEntry() = default;

    HotReloadEntry(std::unique_ptr<HotReloadable> obj, std::string path)
        : object(std::move(obj))
        , source_path(std::move(path))
        , last_reload(std::chrono::steady_clock::now())
    {
        if (object) {
            version = object->current_version();
        }
    }
};

// =============================================================================
// HotReloadManager
// =============================================================================

/// Central manager for hot-reload operations
class HotReloadManager {
public:
    using ReloadCallback = std::function<void(const std::string&, bool)>;

    /// Constructor
    HotReloadManager() = default;

    /// Register a hot-reloadable object
    Result<void> register_object(
        const std::string& name,
        std::unique_ptr<HotReloadable> object,
        const std::string& source_path = "")
    {
        if (!object) {
            return Err(Error("Cannot register null object"));
        }

        if (m_entries.find(name) != m_entries.end()) {
            return Err(HotReloadError::already_registered(name));
        }

        m_entries[name] = HotReloadEntry{std::move(object), source_path};

        // Track path -> name mapping
        if (!source_path.empty()) {
            m_path_to_name[source_path] = name;
        }

        return Ok();
    }

    /// Unregister object
    bool unregister_object(const std::string& name) {
        auto it = m_entries.find(name);
        if (it == m_entries.end()) {
            return false;
        }

        // Remove path mapping
        if (!it->second.source_path.empty()) {
            m_path_to_name.erase(it->second.source_path);
        }

        m_entries.erase(it);
        return true;
    }

    /// Queue a reload event
    void queue_event(ReloadEvent event) {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_pending_events.push(std::move(event));
    }

    /// Process all pending events
    std::vector<Result<void>> process_pending() {
        std::vector<Result<void>> results;

        std::queue<ReloadEvent> events;
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            std::swap(events, m_pending_events);
        }

        while (!events.empty()) {
            ReloadEvent event = std::move(events.front());
            events.pop();

            auto result = process_event(event);
            results.push_back(std::move(result));
        }

        return results;
    }

    /// Reload specific object by name
    Result<void> reload(const std::string& name) {
        auto it = m_entries.find(name);
        if (it == m_entries.end()) {
            return Err(HotReloadError::not_found(name));
        }

        auto& entry = it->second;
        if (!entry.object) {
            return Err(HotReloadError::invalid_state(name, "Object is null"));
        }

        // Prepare for reload
        auto prepare_result = entry.object->prepare_reload();
        if (!prepare_result) {
            return prepare_result;
        }

        // Take snapshot
        auto snapshot_result = entry.object->snapshot();
        if (!snapshot_result) {
            return Err(snapshot_result.error());
        }

        // Store snapshot for restoration
        m_pending_snapshots[name] = std::move(snapshot_result).value();
        entry.pending_reload = true;

        return Ok();
    }

    /// Complete reload with new object implementation
    Result<void> complete_reload(
        const std::string& name,
        std::unique_ptr<HotReloadable> new_object)
    {
        auto entry_it = m_entries.find(name);
        if (entry_it == m_entries.end()) {
            return Err(HotReloadError::not_found(name));
        }

        auto snapshot_it = m_pending_snapshots.find(name);
        if (snapshot_it == m_pending_snapshots.end()) {
            return Err(HotReloadError::invalid_state(name, "No pending snapshot"));
        }

        auto& entry = entry_it->second;
        HotReloadSnapshot snapshot = std::move(snapshot_it->second);
        m_pending_snapshots.erase(snapshot_it);

        if (!new_object) {
            entry.pending_reload = false;
            return Err(Error("Cannot complete reload with null object"));
        }

        // Check version compatibility
        if (!new_object->is_compatible(snapshot.version)) {
            entry.pending_reload = false;
            return Err(HotReloadError::incompatible_version(
                snapshot.version.to_string(),
                new_object->current_version().to_string()));
        }

        // Restore state
        auto restore_result = new_object->restore(std::move(snapshot));
        if (!restore_result) {
            entry.pending_reload = false;
            return restore_result;
        }

        // Finish reload
        auto finish_result = new_object->finish_reload();
        if (!finish_result) {
            entry.pending_reload = false;
            return finish_result;
        }

        // Replace object
        entry.object = std::move(new_object);
        entry.version = entry.object->current_version();
        entry.pending_reload = false;
        entry.last_reload = std::chrono::steady_clock::now();

        // Notify callbacks
        for (auto& callback : m_callbacks) {
            callback(name, true);
        }

        return Ok();
    }

    /// Cancel pending reload
    void cancel_reload(const std::string& name) {
        m_pending_snapshots.erase(name);

        auto it = m_entries.find(name);
        if (it != m_entries.end()) {
            it->second.pending_reload = false;
        }
    }

    /// Get object by name
    [[nodiscard]] HotReloadable* get(const std::string& name) {
        auto it = m_entries.find(name);
        return it != m_entries.end() ? it->second.object.get() : nullptr;
    }

    [[nodiscard]] const HotReloadable* get(const std::string& name) const {
        auto it = m_entries.find(name);
        return it != m_entries.end() ? it->second.object.get() : nullptr;
    }

    /// Get object by name with type cast
    template<typename T>
    [[nodiscard]] T* get_as(const std::string& name) {
        auto* obj = get(name);
        return obj ? dynamic_cast<T*>(obj) : nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* get_as(const std::string& name) const {
        const auto* obj = get(name);
        return obj ? dynamic_cast<const T*>(obj) : nullptr;
    }

    /// Check if object is registered
    [[nodiscard]] bool contains(const std::string& name) const {
        return m_entries.find(name) != m_entries.end();
    }

    /// Check if reload is pending
    [[nodiscard]] bool is_pending(const std::string& name) const {
        auto it = m_entries.find(name);
        return it != m_entries.end() && it->second.pending_reload;
    }

    /// Get count
    [[nodiscard]] std::size_t len() const noexcept {
        return m_entries.size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return m_entries.empty();
    }

    /// Get pending event count
    [[nodiscard]] std::size_t pending_count() const {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return m_pending_events.size();
    }

    /// Register reload callback
    void on_reload(ReloadCallback callback) {
        m_callbacks.push_back(std::move(callback));
    }

    /// Find name by path
    [[nodiscard]] const std::string* find_by_path(const std::string& path) const {
        auto it = m_path_to_name.find(path);
        return it != m_path_to_name.end() ? &it->second : nullptr;
    }

    /// Iterate over all entries
    template<typename F>
    void for_each(F&& func) const {
        for (const auto& [name, entry] : m_entries) {
            if (entry.object) {
                func(name, *entry.object);
            }
        }
    }

private:
    /// Process single event
    Result<void> process_event(const ReloadEvent& event) {
        // Find object by path
        const std::string* name = find_by_path(event.path);
        if (!name) {
            // No registered object for this path
            return Ok();
        }

        switch (event.type) {
            case ReloadEventType::FileModified:
            case ReloadEventType::ForceReload:
                return reload(*name);

            case ReloadEventType::FileDeleted:
                // Mark as pending but don't unregister
                if (auto it = m_entries.find(*name); it != m_entries.end()) {
                    it->second.pending_reload = true;
                }
                return Ok();

            case ReloadEventType::FileCreated:
            case ReloadEventType::FileRenamed:
                // Update path mapping if needed
                if (event.type == ReloadEventType::FileRenamed) {
                    auto it = m_path_to_name.find(event.old_path);
                    if (it != m_path_to_name.end()) {
                        std::string obj_name = it->second;
                        m_path_to_name.erase(it);
                        m_path_to_name[event.path] = obj_name;

                        if (auto entry_it = m_entries.find(obj_name); entry_it != m_entries.end()) {
                            entry_it->second.source_path = event.path;
                        }
                    }
                }
                return Ok();

            default:
                return Ok();
        }
    }

    std::map<std::string, HotReloadEntry> m_entries;
    std::map<std::string, std::string> m_path_to_name;
    std::map<std::string, HotReloadSnapshot> m_pending_snapshots;
    std::queue<ReloadEvent> m_pending_events;
    mutable std::mutex m_queue_mutex;
    std::vector<ReloadCallback> m_callbacks;
};

// =============================================================================
// FileWatcher (Interface)
// =============================================================================

/// Abstract interface for file system watching
class FileWatcher {
public:
    virtual ~FileWatcher() = default;

    /// Start watching a path (file or directory)
    [[nodiscard]] virtual Result<void> watch(const std::string& path) = 0;

    /// Stop watching a path
    [[nodiscard]] virtual Result<void> unwatch(const std::string& path) = 0;

    /// Poll for pending events
    [[nodiscard]] virtual std::vector<ReloadEvent> poll() = 0;

    /// Check if path is being watched
    [[nodiscard]] virtual bool is_watching(const std::string& path) const = 0;

    /// Get watched path count
    [[nodiscard]] virtual std::size_t watched_count() const = 0;

    /// Clear all watches
    virtual void clear() = 0;
};

// =============================================================================
// MemoryFileWatcher (Test Implementation)
// =============================================================================

/// In-memory file watcher for testing
class MemoryFileWatcher : public FileWatcher {
public:
    MemoryFileWatcher() = default;

    Result<void> watch(const std::string& path) override {
        if (m_watched.find(path) != m_watched.end()) {
            return Err("Path already being watched: " + path);
        }
        m_watched.insert(path);
        return Ok();
    }

    Result<void> unwatch(const std::string& path) override {
        auto it = m_watched.find(path);
        if (it == m_watched.end()) {
            return Err("Path not being watched: " + path);
        }
        m_watched.erase(it);
        return Ok();
    }

    std::vector<ReloadEvent> poll() override {
        std::vector<ReloadEvent> events;
        std::swap(events, m_pending);
        return events;
    }

    bool is_watching(const std::string& path) const override {
        return m_watched.find(path) != m_watched.end();
    }

    std::size_t watched_count() const override {
        return m_watched.size();
    }

    void clear() override {
        m_watched.clear();
        m_pending.clear();
    }

    /// Simulate file modification
    void simulate_modify(const std::string& path) {
        if (m_watched.find(path) != m_watched.end()) {
            m_pending.push_back(ReloadEvent::modified(path));
        }
    }

    /// Simulate file creation
    void simulate_create(const std::string& path) {
        m_pending.push_back(ReloadEvent::created(path));
    }

    /// Simulate file deletion
    void simulate_delete(const std::string& path) {
        if (m_watched.find(path) != m_watched.end()) {
            m_pending.push_back(ReloadEvent::deleted(path));
        }
    }

    /// Simulate file rename
    void simulate_rename(const std::string& old_path, const std::string& new_path) {
        m_pending.push_back(ReloadEvent::renamed(old_path, new_path));
    }

private:
    std::set<std::string> m_watched;
    std::vector<ReloadEvent> m_pending;
};

// =============================================================================
// PollingFileWatcher
// =============================================================================

/// File watcher using polling (cross-platform)
class PollingFileWatcher : public FileWatcher {
public:
    /// Constructor with poll interval
    explicit PollingFileWatcher(
        std::chrono::milliseconds interval = std::chrono::milliseconds(100))
        : m_interval(interval) {}

    Result<void> watch(const std::string& path) override {
        std::filesystem::path fs_path(path);

        std::error_code ec;
        if (!std::filesystem::exists(fs_path, ec)) {
            // Still watch non-existent files (they might be created)
            m_watched[path] = WatchedFile{path, std::filesystem::file_time_type{}, false};
            return Ok();
        }

        auto mtime = std::filesystem::last_write_time(fs_path, ec);
        if (ec) {
            return Err("Failed to get modification time: " + ec.message());
        }

        m_watched[path] = WatchedFile{path, mtime, true};
        return Ok();
    }

    Result<void> unwatch(const std::string& path) override {
        auto it = m_watched.find(path);
        if (it == m_watched.end()) {
            return Err("Path not being watched: " + path);
        }
        m_watched.erase(it);
        return Ok();
    }

    std::vector<ReloadEvent> poll() override {
        std::vector<ReloadEvent> events;

        auto now = std::chrono::steady_clock::now();
        if (now - m_last_poll < m_interval) {
            return events;
        }
        m_last_poll = now;

        for (auto& [path, file] : m_watched) {
            std::filesystem::path fs_path(path);
            std::error_code ec;

            bool exists = std::filesystem::exists(fs_path, ec);

            if (!exists && file.exists) {
                // File was deleted
                file.exists = false;
                events.push_back(ReloadEvent::deleted(path));
            }
            else if (exists && !file.exists) {
                // File was created
                file.exists = true;
                file.last_modified = std::filesystem::last_write_time(fs_path, ec);
                events.push_back(ReloadEvent::created(path));
            }
            else if (exists) {
                // Check modification time
                auto mtime = std::filesystem::last_write_time(fs_path, ec);
                if (!ec && mtime != file.last_modified) {
                    file.last_modified = mtime;
                    events.push_back(ReloadEvent::modified(path));
                }
            }
        }

        return events;
    }

    bool is_watching(const std::string& path) const override {
        return m_watched.find(path) != m_watched.end();
    }

    std::size_t watched_count() const override {
        return m_watched.size();
    }

    void clear() override {
        m_watched.clear();
    }

    /// Set poll interval
    void set_interval(std::chrono::milliseconds interval) {
        m_interval = interval;
    }

private:
    struct WatchedFile {
        std::string path;
        std::filesystem::file_time_type last_modified;
        bool exists = false;
    };

    std::map<std::string, WatchedFile> m_watched;
    std::chrono::milliseconds m_interval;
    std::chrono::steady_clock::time_point m_last_poll;
};

// =============================================================================
// HotReloadSystem
// =============================================================================

/// Complete hot-reload system combining manager and watcher
class HotReloadSystem {
public:
    /// Constructor with custom watcher
    explicit HotReloadSystem(std::unique_ptr<FileWatcher> watcher)
        : m_watcher(std::move(watcher)) {}

    /// Constructor with default polling watcher
    HotReloadSystem()
        : m_watcher(std::make_unique<PollingFileWatcher>()) {}

    /// Register object with file watching
    Result<void> register_watched(
        const std::string& name,
        std::unique_ptr<HotReloadable> object,
        const std::string& source_path)
    {
        auto result = m_manager.register_object(name, std::move(object), source_path);
        if (!result) {
            return result;
        }

        return m_watcher->watch(source_path);
    }

    /// Poll and process file changes
    std::vector<Result<void>> update() {
        auto events = m_watcher->poll();
        for (auto& event : events) {
            m_manager.queue_event(std::move(event));
        }
        return m_manager.process_pending();
    }

    /// Get manager
    [[nodiscard]] HotReloadManager& manager() { return m_manager; }
    [[nodiscard]] const HotReloadManager& manager() const { return m_manager; }

    /// Get watcher
    [[nodiscard]] FileWatcher& watcher() { return *m_watcher; }
    [[nodiscard]] const FileWatcher& watcher() const { return *m_watcher; }

    /// Set poll interval (for polling watchers)
    void set_poll_interval(std::chrono::milliseconds interval) {
        m_poll_interval = interval;
    }

    /// Watch a directory
    Result<void> watch_directory(const std::string& path) {
        return m_watcher->watch(path);
    }

    /// Stop watching all paths
    void stop_watching() {
        m_watcher->clear();
    }

private:
    HotReloadManager m_manager;
    std::unique_ptr<FileWatcher> m_watcher;
    std::chrono::milliseconds m_poll_interval{100};
};

} // namespace void_core
