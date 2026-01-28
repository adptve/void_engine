/// @file hot_reload_orchestrator.hpp
/// @brief Centralized hot-reload orchestration for the Kernel
///
/// Architecture (from doc/review):
/// - Kernel is the sole authority for hot-reload coordination
/// - All reloadable units register with the orchestrator
/// - Reload lifecycle: snapshot → pre-event → unload → reload → restore → post-event
/// - Events are published to EventBus for plugin notification
/// - Plugins subscribe to events; they don't own reload logic

#pragma once

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/version.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace void_event { class EventBus; }

namespace void_kernel {

// =============================================================================
// Hot-Reload Lifecycle Events (published to EventBus)
// =============================================================================

/// @brief Phase of the hot-reload lifecycle
enum class ReloadPhase : std::uint8_t {
    Idle,               ///< No reload in progress
    Detected,           ///< Change detected, queued for reload
    Snapshotting,       ///< Taking snapshots of reloadable units
    Unloading,          ///< Unloading old implementation
    Loading,            ///< Loading new implementation
    Restoring,          ///< Restoring state from snapshots
    Finalizing,         ///< Running finish_reload() callbacks
    Complete,           ///< Reload completed successfully
    Failed,             ///< Reload failed (rollback may be in progress)
    RolledBack,         ///< Rollback completed after failure
};

/// @brief Convert ReloadPhase to string
[[nodiscard]] inline const char* to_string(ReloadPhase phase) {
    switch (phase) {
        case ReloadPhase::Idle:         return "Idle";
        case ReloadPhase::Detected:     return "Detected";
        case ReloadPhase::Snapshotting: return "Snapshotting";
        case ReloadPhase::Unloading:    return "Unloading";
        case ReloadPhase::Loading:      return "Loading";
        case ReloadPhase::Restoring:    return "Restoring";
        case ReloadPhase::Finalizing:   return "Finalizing";
        case ReloadPhase::Complete:     return "Complete";
        case ReloadPhase::Failed:       return "Failed";
        case ReloadPhase::RolledBack:   return "RolledBack";
    }
    return "Unknown";
}

/// @brief Event published when a reload cycle begins
/// Plugins can use this to pause operations, flush caches, etc.
struct ReloadCycleStartedEvent {
    std::string reload_id;              ///< Unique ID for this reload cycle
    std::vector<std::string> units;     ///< Names of units being reloaded
    double timestamp;                   ///< When reload started
};

/// @brief Event published during reload phase transitions
struct ReloadPhaseChangedEvent {
    std::string reload_id;
    ReloadPhase old_phase;
    ReloadPhase new_phase;
    std::string current_unit;           ///< Unit being processed (empty if N/A)
    double timestamp;
};

/// @brief Event published when a specific unit snapshot is taken
struct UnitSnapshotTakenEvent {
    std::string reload_id;
    std::string unit_name;
    std::size_t snapshot_bytes;         ///< Size of snapshot data
    void_core::Version version;         ///< Version at snapshot time
    double timestamp;
};

/// @brief Event published when a specific unit is reloaded
struct UnitReloadedEvent {
    std::string reload_id;
    std::string unit_name;
    void_core::Version old_version;
    void_core::Version new_version;
    bool success;
    std::string error;                  ///< Error message if !success
    std::chrono::nanoseconds duration;  ///< How long reload took
    double timestamp;
};

/// @brief Event published when a reload cycle completes
struct ReloadCycleCompletedEvent {
    std::string reload_id;
    bool success;                       ///< Overall success
    std::size_t units_reloaded;         ///< Number of units reloaded
    std::size_t units_failed;           ///< Number of units that failed
    std::vector<std::string> failed_units; ///< Names of failed units
    std::chrono::nanoseconds total_duration;
    double timestamp;
};

/// @brief Event published when reload is rolled back due to failure
struct ReloadRollbackEvent {
    std::string reload_id;
    std::string failed_unit;            ///< Unit that caused rollback
    std::string failure_reason;
    std::size_t units_rolled_back;
    double timestamp;
};

// =============================================================================
// Reload Unit (Registration Entry)
// =============================================================================

/// @brief Category of reloadable unit
enum class ReloadCategory : std::uint8_t {
    Module,         ///< Core engine module
    Plugin,         ///< Gameplay plugin
    Shader,         ///< Shader program
    Asset,          ///< General asset
    Config,         ///< Configuration file
    Script,         ///< Script file (if scripting enabled)
};

/// @brief Convert ReloadCategory to string
[[nodiscard]] inline const char* to_string(ReloadCategory cat) {
    switch (cat) {
        case ReloadCategory::Module:    return "Module";
        case ReloadCategory::Plugin:    return "Plugin";
        case ReloadCategory::Shader:    return "Shader";
        case ReloadCategory::Asset:     return "Asset";
        case ReloadCategory::Config:    return "Config";
        case ReloadCategory::Script:    return "Script";
    }
    return "Unknown";
}

/// @brief Reload priority (determines order within a reload cycle)
enum class ReloadPriority : std::uint8_t {
    Critical = 0,   ///< Core systems (reload first)
    High = 1,       ///< Important modules
    Normal = 2,     ///< Standard plugins/assets
    Low = 3,        ///< Optional, can fail without breaking
    Background = 4, ///< Background tasks (reload last)
};

/// @brief State of a reload unit
enum class ReloadUnitState : std::uint8_t {
    Registered,         ///< Registered but not currently reloading
    PendingReload,      ///< Queued for reload
    Snapshotting,       ///< Snapshot being taken
    Unloading,          ///< Being unloaded
    Loading,            ///< New implementation loading
    Restoring,          ///< State being restored
    Finalizing,         ///< finish_reload() running
    Ready,              ///< Reload complete, fully operational
    Failed,             ///< Reload failed
    RolledBack,         ///< Rolled back to previous state
};

/// @brief Callback for creating new instances after reload
/// @return New HotReloadable instance, or nullptr if creation fails
using ReloadFactory = std::function<std::unique_ptr<void_core::HotReloadable>()>;

/// @brief A registered unit that can be hot-reloaded
struct ReloadUnit {
    std::string name;                   ///< Unique identifier
    ReloadCategory category;            ///< Unit category
    ReloadPriority priority;            ///< Reload order priority
    std::string source_path;            ///< Path to source file (for file watching)
    std::vector<std::string> dependencies; ///< Units this depends on

    // Runtime state (managed by orchestrator)
    void_core::HotReloadable* object{nullptr}; ///< Currently active object
    ReloadUnitState state{ReloadUnitState::Registered};
    void_core::HotReloadSnapshot pending_snapshot;
    void_core::Version version;
    ReloadFactory factory;              ///< Factory for creating new instances

    // Statistics
    std::uint32_t reload_count{0};
    std::chrono::steady_clock::time_point last_reload;
    std::chrono::nanoseconds total_reload_time{0};
    std::chrono::nanoseconds avg_reload_time{0};

    /// @brief Check if this unit depends on another
    [[nodiscard]] bool depends_on(const std::string& other) const {
        return std::find(dependencies.begin(), dependencies.end(), other) != dependencies.end();
    }

    /// @brief Update statistics after a reload
    void record_reload(std::chrono::nanoseconds duration) {
        ++reload_count;
        last_reload = std::chrono::steady_clock::now();
        total_reload_time += duration;
        avg_reload_time = total_reload_time / reload_count;
    }
};

// =============================================================================
// Reload Configuration
// =============================================================================

/// @brief Configuration for the hot-reload orchestrator
struct ReloadOrchestratorConfig {
    /// Enable/disable hot-reload globally
    bool enabled{true};

    /// Poll interval for file watcher
    std::chrono::milliseconds poll_interval{100};

    /// Debounce time (ignore rapid successive changes)
    std::chrono::milliseconds debounce_time{500};

    /// Maximum concurrent reloads (0 = sequential only)
    std::uint32_t max_concurrent_reloads{0};

    /// Timeout for individual unit reload
    std::chrono::milliseconds unit_reload_timeout{5000};

    /// Timeout for entire reload cycle
    std::chrono::milliseconds cycle_timeout{30000};

    /// Auto-rollback on failure
    bool auto_rollback{true};

    /// Pause game simulation during reload
    bool pause_during_reload{false};

    /// File extensions to watch (empty = watch all)
    std::vector<std::string> watched_extensions{".dll", ".so", ".dylib", ".spv", ".glsl", ".hlsl"};

    /// Directories to watch (relative to asset/module paths)
    std::vector<std::string> watched_directories;
};

// =============================================================================
// Hot-Reload Orchestrator
// =============================================================================

/// @brief Centralized hot-reload coordinator
///
/// The orchestrator is the Kernel's authority for all hot-reload operations.
/// It manages the complete lifecycle:
///
/// 1. **Detection**: File watcher detects changes
/// 2. **Queuing**: Changes are debounced and queued
/// 3. **Dependency Resolution**: Determines reload order
/// 4. **Snapshot**: Captures state from all affected units
/// 5. **Unload**: Old implementations are unloaded
/// 6. **Load**: New implementations are loaded
/// 7. **Restore**: State is restored from snapshots
/// 8. **Finalize**: finish_reload() is called on all units
/// 9. **Notification**: Events are published to EventBus
///
/// Usage:
/// ```cpp
/// HotReloadOrchestrator orchestrator;
/// orchestrator.set_event_bus(&event_bus);
/// orchestrator.configure(config);
///
/// // Register units
/// orchestrator.register_unit(ReloadUnit{
///     .name = "combat_plugin",
///     .category = ReloadCategory::Plugin,
///     .priority = ReloadPriority::Normal,
///     .source_path = "plugins/combat.dll",
///     .object = &combat_plugin,
///     .factory = []() { return load_combat_plugin(); }
/// });
///
/// // Each frame in HotReloadPoll stage:
/// orchestrator.poll_and_process();
/// ```
class HotReloadOrchestrator {
public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    HotReloadOrchestrator();
    ~HotReloadOrchestrator();

    // Non-copyable
    HotReloadOrchestrator(const HotReloadOrchestrator&) = delete;
    HotReloadOrchestrator& operator=(const HotReloadOrchestrator&) = delete;

    // Movable
    HotReloadOrchestrator(HotReloadOrchestrator&&) noexcept;
    HotReloadOrchestrator& operator=(HotReloadOrchestrator&&) noexcept;

    /// @brief Initialize the orchestrator
    [[nodiscard]] void_core::Result<void> initialize();

    /// @brief Shutdown the orchestrator
    void shutdown();

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Apply configuration
    void configure(const ReloadOrchestratorConfig& config);

    /// @brief Get current configuration
    [[nodiscard]] const ReloadOrchestratorConfig& config() const noexcept { return m_config; }

    /// @brief Set the event bus for publishing events
    void set_event_bus(void_event::EventBus* bus) { m_event_bus = bus; }

    /// @brief Get the event bus
    [[nodiscard]] void_event::EventBus* event_bus() const noexcept { return m_event_bus; }

    // =========================================================================
    // Unit Registration
    // =========================================================================

    /// @brief Register a reload unit
    [[nodiscard]] void_core::Result<void> register_unit(ReloadUnit unit);

    /// @brief Register a HotReloadable object directly
    [[nodiscard]] void_core::Result<void> register_object(
        const std::string& name,
        void_core::HotReloadable* object,
        ReloadCategory category = ReloadCategory::Asset,
        ReloadPriority priority = ReloadPriority::Normal,
        const std::string& source_path = "");

    /// @brief Unregister a unit by name
    bool unregister_unit(const std::string& name);

    /// @brief Check if a unit is registered
    [[nodiscard]] bool is_registered(const std::string& name) const;

    /// @brief Get a registered unit by name
    [[nodiscard]] const ReloadUnit* get_unit(const std::string& name) const;

    /// @brief Get all registered unit names
    [[nodiscard]] std::vector<std::string> registered_units() const;

    /// @brief Get units by category
    [[nodiscard]] std::vector<std::string> units_by_category(ReloadCategory category) const;

    // =========================================================================
    // Reload Operations
    // =========================================================================

    /// @brief Poll for file changes and process pending reloads
    /// @param dt Delta time since last poll (for debouncing)
    /// @return List of units that were reloaded (empty if none)
    std::vector<std::string> poll_and_process(float dt = 0.0f);

    /// @brief Request reload of a specific unit
    [[nodiscard]] void_core::Result<void> request_reload(const std::string& unit_name);

    /// @brief Request reload of multiple units
    [[nodiscard]] void_core::Result<void> request_reload_batch(const std::vector<std::string>& unit_names);

    /// @brief Request reload of all units in a category
    [[nodiscard]] void_core::Result<void> request_reload_category(ReloadCategory category);

    /// @brief Force immediate reload (bypasses debounce and queue)
    [[nodiscard]] void_core::Result<void> force_reload(const std::string& unit_name);

    /// @brief Cancel a pending reload request
    bool cancel_reload(const std::string& unit_name);

    /// @brief Cancel all pending reload requests
    void cancel_all_pending();

    // =========================================================================
    // Reload Cycle Control
    // =========================================================================

    /// @brief Execute a full reload cycle for pending units
    /// @return Result with list of successfully reloaded units, or error
    [[nodiscard]] void_core::Result<std::vector<std::string>> execute_reload_cycle();

    /// @brief Check if a reload cycle is currently in progress
    [[nodiscard]] bool is_reload_in_progress() const noexcept { return m_current_phase != ReloadPhase::Idle; }

    /// @brief Get current reload phase
    [[nodiscard]] ReloadPhase current_phase() const noexcept { return m_current_phase; }

    /// @brief Get ID of current reload cycle (empty if none)
    [[nodiscard]] const std::string& current_reload_id() const noexcept { return m_current_reload_id; }

    /// @brief Get units pending reload
    [[nodiscard]] std::vector<std::string> pending_units() const;

    // =========================================================================
    // State Snapshot/Restore (Manual Control)
    // =========================================================================

    /// @brief Take a snapshot of a unit's state
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot_unit(const std::string& name);

    /// @brief Restore a unit from a snapshot
    [[nodiscard]] void_core::Result<void> restore_unit(
        const std::string& name,
        void_core::HotReloadSnapshot snapshot);

    /// @brief Take snapshots of all units (for save/load)
    [[nodiscard]] void_core::Result<std::map<std::string, void_core::HotReloadSnapshot>> snapshot_all();

    /// @brief Restore all units from snapshots
    [[nodiscard]] void_core::Result<void> restore_all(
        const std::map<std::string, void_core::HotReloadSnapshot>& snapshots);

    // =========================================================================
    // File Watching
    // =========================================================================

    /// @brief Watch a file or directory for changes
    [[nodiscard]] void_core::Result<void> watch_path(const std::string& path);

    /// @brief Stop watching a path
    void unwatch_path(const std::string& path);

    /// @brief Stop watching all paths
    void unwatch_all();

    /// @brief Get watched paths
    [[nodiscard]] std::vector<std::string> watched_paths() const;

    // =========================================================================
    // Dependency Management
    // =========================================================================

    /// @brief Add a dependency between units
    void add_dependency(const std::string& unit, const std::string& depends_on);

    /// @brief Remove a dependency
    void remove_dependency(const std::string& unit, const std::string& depends_on);

    /// @brief Get units that depend on a given unit
    [[nodiscard]] std::vector<std::string> get_dependents(const std::string& unit) const;

    /// @brief Get units that a given unit depends on
    [[nodiscard]] std::vector<std::string> get_dependencies(const std::string& unit) const;

    /// @brief Compute reload order (topological sort respecting dependencies)
    [[nodiscard]] std::vector<std::string> compute_reload_order(
        const std::vector<std::string>& units) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /// @brief Get total reload count
    [[nodiscard]] std::uint64_t total_reload_count() const noexcept { return m_total_reloads; }

    /// @brief Get successful reload count
    [[nodiscard]] std::uint64_t successful_reload_count() const noexcept { return m_successful_reloads; }

    /// @brief Get failed reload count
    [[nodiscard]] std::uint64_t failed_reload_count() const noexcept { return m_failed_reloads; }

    /// @brief Get rollback count
    [[nodiscard]] std::uint64_t rollback_count() const noexcept { return m_rollback_count; }

    /// @brief Get average reload time
    [[nodiscard]] std::chrono::nanoseconds average_reload_time() const;

    /// @brief Reset statistics
    void reset_statistics();

    // =========================================================================
    // Callbacks (for Kernel integration)
    // =========================================================================

    using PreReloadCallback = std::function<void(const std::vector<std::string>&)>;
    using PostReloadCallback = std::function<void(const std::vector<std::string>&, bool)>;

    /// @brief Set callback invoked before reload cycle starts
    void set_pre_reload_callback(PreReloadCallback callback) {
        m_pre_reload_callback = std::move(callback);
    }

    /// @brief Set callback invoked after reload cycle completes
    void set_post_reload_callback(PostReloadCallback callback) {
        m_post_reload_callback = std::move(callback);
    }

private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================

    /// @brief Set reload phase and publish event
    void set_phase(ReloadPhase phase, const std::string& current_unit = "");

    /// @brief Generate unique reload cycle ID
    [[nodiscard]] std::string generate_reload_id() const;

    /// @brief Get current time as double (for events)
    [[nodiscard]] double current_time() const;

    /// @brief Publish an event to the event bus
    template<typename E>
    void publish_event(E&& event);

    /// @brief Execute snapshot phase for a list of units
    [[nodiscard]] void_core::Result<void> execute_snapshot_phase(const std::vector<std::string>& units);

    /// @brief Execute unload phase for a list of units
    [[nodiscard]] void_core::Result<void> execute_unload_phase(const std::vector<std::string>& units);

    /// @brief Execute load phase for a list of units
    [[nodiscard]] void_core::Result<void> execute_load_phase(const std::vector<std::string>& units);

    /// @brief Execute restore phase for a list of units
    [[nodiscard]] void_core::Result<void> execute_restore_phase(const std::vector<std::string>& units);

    /// @brief Execute finalize phase for a list of units
    [[nodiscard]] void_core::Result<void> execute_finalize_phase(const std::vector<std::string>& units);

    /// @brief Rollback to previous state after failure
    void execute_rollback(const std::vector<std::string>& units, const std::string& failed_unit);

    /// @brief Process debounced file events
    void process_debounced_events();

    /// @brief Map path to unit name
    [[nodiscard]] const std::string* find_unit_by_path(const std::string& path) const;

private:
    // Configuration
    ReloadOrchestratorConfig m_config;

    // Event bus for publishing events
    void_event::EventBus* m_event_bus{nullptr};

    // Registered units
    mutable std::mutex m_units_mutex;
    std::map<std::string, ReloadUnit> m_units;
    std::map<std::string, std::string> m_path_to_unit; // path -> unit name

    // Reload state
    ReloadPhase m_current_phase{ReloadPhase::Idle};
    std::string m_current_reload_id;
    std::set<std::string> m_pending_reloads;
    std::chrono::steady_clock::time_point m_cycle_start_time;

    // Debouncing
    struct PendingChange {
        std::string path;
        void_core::ReloadEventType type;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<PendingChange> m_debounce_queue;
    mutable std::mutex m_debounce_mutex;

    // File watcher
    std::unique_ptr<void_core::FileWatcher> m_file_watcher;

    // Statistics
    std::uint64_t m_total_reloads{0};
    std::uint64_t m_successful_reloads{0};
    std::uint64_t m_failed_reloads{0};
    std::uint64_t m_rollback_count{0};
    std::chrono::nanoseconds m_total_reload_time{0};
    std::uint64_t m_reload_cycle_count{0};

    // Callbacks
    PreReloadCallback m_pre_reload_callback;
    PostReloadCallback m_post_reload_callback;

    // Initialization state
    bool m_initialized{false};
    std::chrono::steady_clock::time_point m_start_time;
};

// =============================================================================
// Orchestrator Builder
// =============================================================================

/// @brief Fluent builder for HotReloadOrchestrator configuration
class HotReloadOrchestratorBuilder {
public:
    HotReloadOrchestratorBuilder() = default;

    /// @brief Enable/disable hot-reload
    HotReloadOrchestratorBuilder& enabled(bool e) {
        m_config.enabled = e;
        return *this;
    }

    /// @brief Set poll interval
    HotReloadOrchestratorBuilder& poll_interval(std::chrono::milliseconds interval) {
        m_config.poll_interval = interval;
        return *this;
    }

    /// @brief Set debounce time
    HotReloadOrchestratorBuilder& debounce(std::chrono::milliseconds time) {
        m_config.debounce_time = time;
        return *this;
    }

    /// @brief Set unit reload timeout
    HotReloadOrchestratorBuilder& unit_timeout(std::chrono::milliseconds timeout) {
        m_config.unit_reload_timeout = timeout;
        return *this;
    }

    /// @brief Set cycle timeout
    HotReloadOrchestratorBuilder& cycle_timeout(std::chrono::milliseconds timeout) {
        m_config.cycle_timeout = timeout;
        return *this;
    }

    /// @brief Enable/disable auto-rollback
    HotReloadOrchestratorBuilder& auto_rollback(bool enable) {
        m_config.auto_rollback = enable;
        return *this;
    }

    /// @brief Enable/disable pause during reload
    HotReloadOrchestratorBuilder& pause_during_reload(bool pause) {
        m_config.pause_during_reload = pause;
        return *this;
    }

    /// @brief Add watched file extension
    HotReloadOrchestratorBuilder& watch_extension(const std::string& ext) {
        m_config.watched_extensions.push_back(ext);
        return *this;
    }

    /// @brief Add watched directory
    HotReloadOrchestratorBuilder& watch_directory(const std::string& dir) {
        m_config.watched_directories.push_back(dir);
        return *this;
    }

    /// @brief Build the configuration
    [[nodiscard]] ReloadOrchestratorConfig build() const { return m_config; }

private:
    ReloadOrchestratorConfig m_config;
};

} // namespace void_kernel
