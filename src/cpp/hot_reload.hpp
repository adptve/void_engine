#pragma once

/// @file hot_reload.hpp
/// @brief C++ hot reload system

#include "compiler.hpp"
#include "module.hpp"

#include <void_engine/event/event.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_cpp {

// =============================================================================
// File Watcher
// =============================================================================

/// @brief Watches files for changes
class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    // Non-copyable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Watch management
    WatcherId watch(const std::filesystem::path& path);
    WatcherId watch_directory(const std::filesystem::path& dir, bool recursive = true);
    void unwatch(WatcherId id);
    void unwatch_all();

    // Filtering
    void add_extension_filter(const std::string& ext);  // e.g., ".cpp", ".hpp"
    void clear_extension_filters();
    void set_ignore_patterns(const std::vector<std::string>& patterns);

    // Polling
    std::vector<FileChangeEvent> poll();

    // Callback-based
    using ChangeCallback = std::function<void(const FileChangeEvent&)>;
    void set_callback(ChangeCallback callback);

    // Start/Stop background watching
    void start(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(100));
    void stop();
    [[nodiscard]] bool is_running() const { return running_; }

    // Debounce
    void set_debounce_time(std::chrono::milliseconds time) { debounce_time_ = time; }

private:
    void watch_thread();
    bool matches_filters(const std::filesystem::path& path) const;
    bool matches_ignore(const std::filesystem::path& path) const;

    struct WatchEntry {
        WatcherId id;
        std::filesystem::path path;
        bool is_directory;
        bool recursive;
        std::filesystem::file_time_type last_time;
    };

    std::unordered_map<WatcherId, WatchEntry> watches_;
    std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> file_times_;

    std::vector<std::string> extension_filters_;
    std::vector<std::string> ignore_patterns_;

    std::mutex mutex_;
    std::vector<FileChangeEvent> pending_events_;
    std::chrono::milliseconds debounce_time_{50};

    // Background thread
    std::thread watch_thread_;
    std::atomic<bool> running_{false};
    std::chrono::milliseconds poll_interval_{100};
    ChangeCallback callback_;

    std::uint32_t next_watcher_id_ = 1;
};

// =============================================================================
// State Preserver
// =============================================================================

/// @brief Preserves and restores state during hot reload
class StatePreserver {
public:
    StatePreserver();
    ~StatePreserver();

    // ==========================================================================
    // State Registration
    // ==========================================================================

    /// @brief Register state to preserve
    template <typename T>
    void register_state(const std::string& name, T* ptr) {
        states_[name] = StateEntry{
            ptr,
            sizeof(T),
            [](void* dst, const void* src, std::size_t size) {
                std::memcpy(dst, src, size);
            },
            [](void* dst, const void* src, std::size_t size) {
                std::memcpy(dst, src, size);
            }
        };
    }

    /// @brief Register state with custom save/restore
    template <typename T>
    void register_state(
        const std::string& name,
        T* ptr,
        std::function<void(void*, const T*)> save_func,
        std::function<void(T*, const void*)> restore_func) {

        states_[name] = StateEntry{
            ptr,
            sizeof(T),
            [save_func](void* dst, const void* src, std::size_t) {
                save_func(dst, static_cast<const T*>(src));
            },
            [restore_func](void* dst, const void* src, std::size_t) {
                restore_func(static_cast<T*>(dst), src);
            }
        };
    }

    /// @brief Unregister state
    void unregister_state(const std::string& name);

    /// @brief Clear all registered states
    void clear();

    // ==========================================================================
    // Save/Restore
    // ==========================================================================

    /// @brief Save all registered state
    void save_all();

    /// @brief Restore all saved state
    void restore_all();

    /// @brief Save specific state
    void save(const std::string& name);

    /// @brief Restore specific state
    void restore(const std::string& name);

    // ==========================================================================
    // Serialization
    // ==========================================================================

    /// @brief Save to file
    CppResult<void> save_to_file(const std::filesystem::path& path);

    /// @brief Restore from file
    CppResult<void> restore_from_file(const std::filesystem::path& path);

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    using SaveCallback = std::function<void(const std::string& name, std::vector<std::uint8_t>& data)>;
    using RestoreCallback = std::function<void(const std::string& name, const std::vector<std::uint8_t>& data)>;

    void set_save_callback(SaveCallback callback) { save_callback_ = std::move(callback); }
    void set_restore_callback(RestoreCallback callback) { restore_callback_ = std::move(callback); }

private:
    struct StateEntry {
        void* ptr;
        std::size_t size;
        std::function<void(void*, const void*, std::size_t)> save_func;
        std::function<void(void*, const void*, std::size_t)> restore_func;
    };

    std::unordered_map<std::string, StateEntry> states_;
    std::unordered_map<std::string, std::vector<std::uint8_t>> saved_data_;

    SaveCallback save_callback_;
    RestoreCallback restore_callback_;
};

// =============================================================================
// Reload Context
// =============================================================================

/// @brief Context for a reload operation
struct ReloadContext {
    ModuleId module_id;
    std::string module_name;
    std::filesystem::path old_path;
    std::filesystem::path new_path;

    std::vector<std::filesystem::path> changed_files;
    std::chrono::system_clock::time_point start_time;
    std::chrono::milliseconds compile_time{0};
    std::chrono::milliseconds load_time{0};

    bool success = false;
    std::string error_message;

    // User data
    void* user_data = nullptr;
};

// =============================================================================
// Hot Reload Events
// =============================================================================

/// @brief Event: File changed
struct FileChangedEvent {
    FileChangeType type;
    std::filesystem::path path;
};

/// @brief Event: Reload started
struct ReloadStartedEvent {
    ModuleId module_id;
    std::string module_name;
};

/// @brief Event: Reload completed
struct ReloadCompletedEvent {
    ModuleId module_id;
    std::string module_name;
    bool success;
    std::string error_message;
    std::chrono::milliseconds total_time;
};

// =============================================================================
// Hot Reloader
// =============================================================================

/// @brief Main hot reload system
class HotReloader {
public:
    HotReloader();
    explicit HotReloader(Compiler& compiler, ModuleRegistry& registry);
    ~HotReloader();

    // Singleton
    [[nodiscard]] static HotReloader& instance();

    // ==========================================================================
    // Configuration
    // ==========================================================================

    /// @brief Enable/disable hot reload
    void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const { return enabled_; }

    /// @brief Set source directories to watch
    void add_source_directory(const std::filesystem::path& dir);
    void clear_source_directories();

    /// @brief Set compiler configuration
    void set_compiler_config(const CompilerConfig& config);

    /// @brief Set debounce time (delay before triggering reload)
    void set_debounce_time(std::chrono::milliseconds time);

    // ==========================================================================
    // Module Registration
    // ==========================================================================

    /// @brief Register a module for hot reload
    void register_module(ModuleId id, const std::vector<std::filesystem::path>& sources);

    /// @brief Unregister a module
    void unregister_module(ModuleId id);

    /// @brief Get registered modules
    [[nodiscard]] std::vector<ModuleId> registered_modules() const;

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    void set_pre_reload_callback(PreReloadCallback callback);
    void set_post_reload_callback(PostReloadCallback callback);
    void set_reload_callback(ReloadCallback callback);

    // ==========================================================================
    // Reload Operations
    // ==========================================================================

    /// @brief Trigger immediate reload for a module
    CppResult<void> reload(ModuleId module_id);

    /// @brief Reload all modules with changed sources
    void reload_changed();

    /// @brief Check for changes and queue reloads
    void poll();

    // ==========================================================================
    // State Preservation
    // ==========================================================================

    [[nodiscard]] StatePreserver& state_preserver() { return state_preserver_; }

    // ==========================================================================
    // Event Bus
    // ==========================================================================

    void set_event_bus(void_event::EventBus* bus) { event_bus_ = bus; }
    [[nodiscard]] void_event::EventBus* event_bus() const { return event_bus_; }

    // ==========================================================================
    // Background Processing
    // ==========================================================================

    /// @brief Start background watching
    void start();

    /// @brief Stop background watching
    void stop();

    /// @brief Update (call from main loop)
    void update();

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t total_reloads = 0;
        std::size_t successful_reloads = 0;
        std::size_t failed_reloads = 0;
        std::chrono::milliseconds total_reload_time{0};
        std::chrono::milliseconds average_reload_time{0};
    };

    [[nodiscard]] Stats stats() const { return stats_; }

private:
    void on_file_changed(const FileChangeEvent& event);
    void process_pending_reloads();
    CppResult<void> do_reload(ModuleId module_id, const ReloadContext& context);

    struct ModuleEntry {
        ModuleId id;
        std::vector<std::filesystem::path> sources;
        std::filesystem::file_time_type last_compile_time;
    };

    Compiler& compiler_;
    ModuleRegistry& registry_;
    FileWatcher file_watcher_;
    StatePreserver state_preserver_;

    std::unordered_map<ModuleId, ModuleEntry> registered_modules_;
    std::unordered_map<std::filesystem::path, ModuleId> source_to_module_;

    CompilerConfig compiler_config_;
    std::vector<std::filesystem::path> source_directories_;

    bool enabled_ = true;
    std::chrono::milliseconds debounce_time_{200};

    // Pending reloads
    std::mutex pending_mutex_;
    std::unordered_set<ModuleId> pending_reloads_;
    std::unordered_map<ModuleId, std::chrono::steady_clock::time_point> reload_timestamps_;

    // Callbacks
    PreReloadCallback pre_reload_callback_;
    PostReloadCallback post_reload_callback_;
    ReloadCallback reload_callback_;

    void_event::EventBus* event_bus_ = nullptr;
    Stats stats_;
};

// =============================================================================
// Convenience Macros
// =============================================================================

#ifdef VOID_HOT_RELOAD

/// @brief Mark a class as hot-reloadable
#define VOID_HOT_RELOADABLE(ClassName) \
    static void* __void_hr_state_; \
    static void __void_hr_save_state(void* state) { \
        std::memcpy(state, __void_hr_state_, sizeof(ClassName)); \
    } \
    static void __void_hr_restore_state(void* state) { \
        std::memcpy(__void_hr_state_, state, sizeof(ClassName)); \
    }

/// @brief Register state for hot reload
#define VOID_REGISTER_STATE(name, ptr) \
    void_cpp::HotReloader::instance().state_preserver().register_state(name, ptr)

/// @brief Unregister state
#define VOID_UNREGISTER_STATE(name) \
    void_cpp::HotReloader::instance().state_preserver().unregister_state(name)

#else

#define VOID_HOT_RELOADABLE(ClassName)
#define VOID_REGISTER_STATE(name, ptr)
#define VOID_UNREGISTER_STATE(name)

#endif

} // namespace void_cpp
