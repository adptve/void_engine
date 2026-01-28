/// @file hot_reload_orchestrator.cpp
/// @brief Implementation of centralized hot-reload orchestration

#include <void_engine/kernel/hot_reload_orchestrator.hpp>
#include <void_engine/event/event_bus.hpp>

#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace void_kernel {

// =============================================================================
// HotReloadOrchestrator Implementation
// =============================================================================

HotReloadOrchestrator::HotReloadOrchestrator()
    : m_file_watcher(std::make_unique<void_core::PollingFileWatcher>())
    , m_start_time(std::chrono::steady_clock::now()) {
}

HotReloadOrchestrator::~HotReloadOrchestrator() {
    shutdown();
}

HotReloadOrchestrator::HotReloadOrchestrator(HotReloadOrchestrator&& other) noexcept
    : m_config(std::move(other.m_config))
    , m_event_bus(other.m_event_bus)
    , m_units(std::move(other.m_units))
    , m_path_to_unit(std::move(other.m_path_to_unit))
    , m_current_phase(other.m_current_phase)
    , m_current_reload_id(std::move(other.m_current_reload_id))
    , m_pending_reloads(std::move(other.m_pending_reloads))
    , m_cycle_start_time(other.m_cycle_start_time)
    , m_debounce_queue(std::move(other.m_debounce_queue))
    , m_file_watcher(std::move(other.m_file_watcher))
    , m_total_reloads(other.m_total_reloads)
    , m_successful_reloads(other.m_successful_reloads)
    , m_failed_reloads(other.m_failed_reloads)
    , m_rollback_count(other.m_rollback_count)
    , m_total_reload_time(other.m_total_reload_time)
    , m_reload_cycle_count(other.m_reload_cycle_count)
    , m_pre_reload_callback(std::move(other.m_pre_reload_callback))
    , m_post_reload_callback(std::move(other.m_post_reload_callback))
    , m_initialized(other.m_initialized)
    , m_start_time(other.m_start_time) {
    other.m_event_bus = nullptr;
    other.m_initialized = false;
}

HotReloadOrchestrator& HotReloadOrchestrator::operator=(HotReloadOrchestrator&& other) noexcept {
    if (this != &other) {
        m_config = std::move(other.m_config);
        m_event_bus = other.m_event_bus;
        m_units = std::move(other.m_units);
        m_path_to_unit = std::move(other.m_path_to_unit);
        m_current_phase = other.m_current_phase;
        m_current_reload_id = std::move(other.m_current_reload_id);
        m_pending_reloads = std::move(other.m_pending_reloads);
        m_cycle_start_time = other.m_cycle_start_time;
        m_debounce_queue = std::move(other.m_debounce_queue);
        m_file_watcher = std::move(other.m_file_watcher);
        m_total_reloads = other.m_total_reloads;
        m_successful_reloads = other.m_successful_reloads;
        m_failed_reloads = other.m_failed_reloads;
        m_rollback_count = other.m_rollback_count;
        m_total_reload_time = other.m_total_reload_time;
        m_reload_cycle_count = other.m_reload_cycle_count;
        m_pre_reload_callback = std::move(other.m_pre_reload_callback);
        m_post_reload_callback = std::move(other.m_post_reload_callback);
        m_initialized = other.m_initialized;
        m_start_time = other.m_start_time;
        other.m_event_bus = nullptr;
        other.m_initialized = false;
    }
    return *this;
}

void_core::Result<void> HotReloadOrchestrator::initialize() {
    if (m_initialized) {
        return void_core::Ok();
    }

    // Configure file watcher with poll interval
    if (auto* polling = dynamic_cast<void_core::PollingFileWatcher*>(m_file_watcher.get())) {
        polling->set_interval(m_config.poll_interval);
    }

    // Watch configured directories
    for (const auto& dir : m_config.watched_directories) {
        auto result = m_file_watcher->watch(dir);
        if (!result) {
            // Log but don't fail - directory might not exist yet
        }
    }

    m_initialized = true;
    return void_core::Ok();
}

void HotReloadOrchestrator::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Cancel any pending reloads
    cancel_all_pending();

    // Stop watching all paths
    unwatch_all();

    // Clear registered units
    {
        std::lock_guard<std::mutex> lock(m_units_mutex);
        m_units.clear();
        m_path_to_unit.clear();
    }

    m_initialized = false;
}

void HotReloadOrchestrator::configure(const ReloadOrchestratorConfig& config) {
    m_config = config;

    // Update file watcher poll interval
    if (auto* polling = dynamic_cast<void_core::PollingFileWatcher*>(m_file_watcher.get())) {
        polling->set_interval(m_config.poll_interval);
    }
}

// =============================================================================
// Unit Registration
// =============================================================================

void_core::Result<void> HotReloadOrchestrator::register_unit(ReloadUnit unit) {
    if (unit.name.empty()) {
        return void_core::Err("Unit name cannot be empty");
    }

    std::lock_guard<std::mutex> lock(m_units_mutex);

    if (m_units.find(unit.name) != m_units.end()) {
        return void_core::Err("Unit already registered: " + unit.name);
    }

    // Set initial version from object if available
    if (unit.object) {
        unit.version = unit.object->current_version();
    }

    // Track path -> unit mapping
    if (!unit.source_path.empty()) {
        m_path_to_unit[unit.source_path] = unit.name;

        // Start watching the path
        if (m_config.enabled) {
            // Don't fail if watch fails - file might not exist yet
            (void)m_file_watcher->watch(unit.source_path);
        }
    }

    m_units[unit.name] = std::move(unit);

    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::register_object(
    const std::string& name,
    void_core::HotReloadable* object,
    ReloadCategory category,
    ReloadPriority priority,
    const std::string& source_path) {

    if (!object) {
        return void_core::Err("Cannot register null object");
    }

    ReloadUnit unit;
    unit.name = name;
    unit.category = category;
    unit.priority = priority;
    unit.source_path = source_path;
    unit.object = object;
    unit.version = object->current_version();

    return register_unit(std::move(unit));
}

bool HotReloadOrchestrator::unregister_unit(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_units.find(name);
    if (it == m_units.end()) {
        return false;
    }

    // Remove path mapping
    if (!it->second.source_path.empty()) {
        m_path_to_unit.erase(it->second.source_path);
        m_file_watcher->unwatch(it->second.source_path);
    }

    // Remove from pending reloads
    m_pending_reloads.erase(name);

    m_units.erase(it);
    return true;
}

bool HotReloadOrchestrator::is_registered(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_units_mutex);
    return m_units.find(name) != m_units.end();
}

const ReloadUnit* HotReloadOrchestrator::get_unit(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_units_mutex);
    auto it = m_units.find(name);
    return it != m_units.end() ? &it->second : nullptr;
}

std::vector<std::string> HotReloadOrchestrator::registered_units() const {
    std::lock_guard<std::mutex> lock(m_units_mutex);
    std::vector<std::string> names;
    names.reserve(m_units.size());
    for (const auto& [name, _] : m_units) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> HotReloadOrchestrator::units_by_category(ReloadCategory category) const {
    std::lock_guard<std::mutex> lock(m_units_mutex);
    std::vector<std::string> names;
    for (const auto& [name, unit] : m_units) {
        if (unit.category == category) {
            names.push_back(name);
        }
    }
    return names;
}

// =============================================================================
// Reload Operations
// =============================================================================

std::vector<std::string> HotReloadOrchestrator::poll_and_process(float dt) {
    if (!m_config.enabled || !m_initialized) {
        return {};
    }

    // Don't process if reload is already in progress
    if (is_reload_in_progress()) {
        return {};
    }

    // Poll file watcher for changes
    auto events = m_file_watcher->poll();

    // Queue events for debouncing
    {
        std::lock_guard<std::mutex> lock(m_debounce_mutex);
        auto now = std::chrono::steady_clock::now();

        for (const auto& event : events) {
            // Check if this file extension is watched
            bool should_watch = m_config.watched_extensions.empty();
            if (!should_watch) {
                for (const auto& ext : m_config.watched_extensions) {
                    if (event.path.size() >= ext.size() &&
                        event.path.compare(event.path.size() - ext.size(), ext.size(), ext) == 0) {
                        should_watch = true;
                        break;
                    }
                }
            }

            if (should_watch) {
                m_debounce_queue.push_back(PendingChange{
                    event.path,
                    event.type,
                    now
                });
            }
        }
    }

    // Process debounced events
    process_debounced_events();

    // If we have pending reloads and enough time has passed, execute
    if (!m_pending_reloads.empty()) {
        auto result = execute_reload_cycle();
        if (result) {
            return std::move(result).value();
        }
    }

    return {};
}

void HotReloadOrchestrator::process_debounced_events() {
    std::lock_guard<std::mutex> lock(m_debounce_mutex);

    auto now = std::chrono::steady_clock::now();
    std::vector<PendingChange> ready;

    // Find events that have been debounced long enough
    auto it = m_debounce_queue.begin();
    while (it != m_debounce_queue.end()) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->timestamp);
        if (age >= m_config.debounce_time) {
            ready.push_back(*it);
            it = m_debounce_queue.erase(it);
        } else {
            ++it;
        }
    }

    // Map paths to units and queue reloads
    for (const auto& change : ready) {
        const std::string* unit_name = find_unit_by_path(change.path);
        if (unit_name) {
            m_pending_reloads.insert(*unit_name);
        }
    }
}

void_core::Result<void> HotReloadOrchestrator::request_reload(const std::string& unit_name) {
    std::lock_guard<std::mutex> lock(m_units_mutex);

    if (m_units.find(unit_name) == m_units.end()) {
        return void_core::Err("Unit not registered: " + unit_name);
    }

    m_pending_reloads.insert(unit_name);
    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::request_reload_batch(
    const std::vector<std::string>& unit_names) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    for (const auto& name : unit_names) {
        if (m_units.find(name) == m_units.end()) {
            return void_core::Err("Unit not registered: " + name);
        }
    }

    for (const auto& name : unit_names) {
        m_pending_reloads.insert(name);
    }

    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::request_reload_category(ReloadCategory category) {
    auto units = units_by_category(category);
    for (const auto& name : units) {
        m_pending_reloads.insert(name);
    }
    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::force_reload(const std::string& unit_name) {
    auto request_result = request_reload(unit_name);
    if (!request_result) {
        return request_result;
    }

    auto result = execute_reload_cycle();
    if (!result) {
        return void_core::Err(result.error().message());
    }

    return void_core::Ok();
}

bool HotReloadOrchestrator::cancel_reload(const std::string& unit_name) {
    auto it = m_pending_reloads.find(unit_name);
    if (it != m_pending_reloads.end()) {
        m_pending_reloads.erase(it);
        return true;
    }
    return false;
}

void HotReloadOrchestrator::cancel_all_pending() {
    m_pending_reloads.clear();

    std::lock_guard<std::mutex> lock(m_debounce_mutex);
    m_debounce_queue.clear();
}

// =============================================================================
// Reload Cycle Execution
// =============================================================================

void_core::Result<std::vector<std::string>> HotReloadOrchestrator::execute_reload_cycle() {
    if (m_pending_reloads.empty()) {
        return std::vector<std::string>{};
    }

    if (is_reload_in_progress()) {
        return void_core::Result<std::vector<std::string>>{
            void_core::Error{"Reload cycle already in progress"}};
    }

    // Collect units to reload
    std::vector<std::string> units_to_reload(m_pending_reloads.begin(), m_pending_reloads.end());
    m_pending_reloads.clear();

    // Compute reload order (respects dependencies)
    units_to_reload = compute_reload_order(units_to_reload);

    // Start reload cycle
    m_current_reload_id = generate_reload_id();
    m_cycle_start_time = std::chrono::steady_clock::now();
    ++m_reload_cycle_count;

    // Publish cycle started event
    publish_event(ReloadCycleStartedEvent{
        m_current_reload_id,
        units_to_reload,
        current_time()
    });

    // Invoke pre-reload callback
    if (m_pre_reload_callback) {
        m_pre_reload_callback(units_to_reload);
    }

    std::vector<std::string> successfully_reloaded;
    std::vector<std::string> failed_units;
    std::string first_failure_reason;

    // Phase 1: Snapshot
    set_phase(ReloadPhase::Snapshotting);
    auto snapshot_result = execute_snapshot_phase(units_to_reload);
    if (!snapshot_result) {
        first_failure_reason = snapshot_result.error().message();
        // Continue to try other units
    }

    // Phase 2: Prepare reload (call prepare_reload on all units)
    set_phase(ReloadPhase::Unloading);
    for (const auto& unit_name : units_to_reload) {
        std::lock_guard<std::mutex> lock(m_units_mutex);
        auto it = m_units.find(unit_name);
        if (it == m_units.end() || !it->second.object) continue;

        auto& unit = it->second;
        unit.state = ReloadUnitState::Unloading;

        auto prepare_result = unit.object->prepare_reload();
        if (!prepare_result) {
            unit.state = ReloadUnitState::Failed;
            failed_units.push_back(unit_name);
            if (first_failure_reason.empty()) {
                first_failure_reason = prepare_result.error().message();
            }
        }
    }

    // Phase 3: Load new implementations (via factory)
    set_phase(ReloadPhase::Loading);
    auto load_result = execute_load_phase(units_to_reload);

    // Phase 4: Restore state
    set_phase(ReloadPhase::Restoring);
    auto restore_result = execute_restore_phase(units_to_reload);

    // Phase 5: Finalize
    set_phase(ReloadPhase::Finalizing);
    auto finalize_result = execute_finalize_phase(units_to_reload);

    // Determine success/failure for each unit
    {
        std::lock_guard<std::mutex> lock(m_units_mutex);
        for (const auto& unit_name : units_to_reload) {
            auto it = m_units.find(unit_name);
            if (it == m_units.end()) continue;

            auto& unit = it->second;
            auto reload_end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                reload_end - m_cycle_start_time);

            if (unit.state == ReloadUnitState::Failed) {
                failed_units.push_back(unit_name);
                ++m_failed_reloads;

                publish_event(UnitReloadedEvent{
                    m_current_reload_id,
                    unit_name,
                    unit.version,
                    unit.version,
                    false,
                    "Reload failed",
                    duration,
                    current_time()
                });
            } else {
                unit.state = ReloadUnitState::Ready;
                unit.record_reload(duration);
                successfully_reloaded.push_back(unit_name);
                ++m_successful_reloads;
                ++m_total_reloads;
                m_total_reload_time += duration;

                auto old_version = unit.version;
                if (unit.object) {
                    unit.version = unit.object->current_version();
                }

                publish_event(UnitReloadedEvent{
                    m_current_reload_id,
                    unit_name,
                    old_version,
                    unit.version,
                    true,
                    "",
                    duration,
                    current_time()
                });
            }
        }
    }

    // Handle rollback if needed
    if (!failed_units.empty() && m_config.auto_rollback) {
        set_phase(ReloadPhase::Failed);
        execute_rollback(successfully_reloaded, failed_units.front());
        set_phase(ReloadPhase::RolledBack);
    } else if (!failed_units.empty()) {
        set_phase(ReloadPhase::Failed);
    } else {
        set_phase(ReloadPhase::Complete);
    }

    // Calculate total duration
    auto cycle_end = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        cycle_end - m_cycle_start_time);

    // Publish cycle completed event
    publish_event(ReloadCycleCompletedEvent{
        m_current_reload_id,
        failed_units.empty(),
        successfully_reloaded.size(),
        failed_units.size(),
        failed_units,
        total_duration,
        current_time()
    });

    // Invoke post-reload callback
    if (m_post_reload_callback) {
        m_post_reload_callback(successfully_reloaded, failed_units.empty());
    }

    // Reset to idle
    set_phase(ReloadPhase::Idle);
    m_current_reload_id.clear();

    if (!failed_units.empty()) {
        return void_core::Result<std::vector<std::string>>{
            void_core::Error{"Some units failed to reload: " + first_failure_reason}};
    }

    return successfully_reloaded;
}

void_core::Result<void> HotReloadOrchestrator::execute_snapshot_phase(
    const std::vector<std::string>& units) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    for (const auto& unit_name : units) {
        auto it = m_units.find(unit_name);
        if (it == m_units.end()) continue;

        auto& unit = it->second;
        if (!unit.object) continue;

        set_phase(ReloadPhase::Snapshotting, unit_name);
        unit.state = ReloadUnitState::Snapshotting;

        auto snapshot_result = unit.object->snapshot();
        if (!snapshot_result) {
            unit.state = ReloadUnitState::Failed;
            return void_core::Err("Failed to snapshot unit " + unit_name + ": " +
                                  snapshot_result.error().message());
        }

        unit.pending_snapshot = std::move(snapshot_result).value();

        publish_event(UnitSnapshotTakenEvent{
            m_current_reload_id,
            unit_name,
            unit.pending_snapshot.data.size(),
            unit.version,
            current_time()
        });
    }

    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::execute_unload_phase(
    const std::vector<std::string>& units) {

    // Unloading is handled in the main cycle
    // This is a placeholder for more complex unload logic if needed
    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::execute_load_phase(
    const std::vector<std::string>& units) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    for (const auto& unit_name : units) {
        auto it = m_units.find(unit_name);
        if (it == m_units.end()) continue;

        auto& unit = it->second;
        if (unit.state == ReloadUnitState::Failed) continue;

        set_phase(ReloadPhase::Loading, unit_name);
        unit.state = ReloadUnitState::Loading;

        // If there's a factory, use it to create new instance
        if (unit.factory) {
            auto new_object = unit.factory();
            if (!new_object) {
                unit.state = ReloadUnitState::Failed;
                continue;
            }

            // Check version compatibility
            if (!new_object->is_compatible(unit.pending_snapshot.version)) {
                unit.state = ReloadUnitState::Failed;
                continue;
            }

            // The new object becomes the active one
            // Note: Caller is responsible for managing object lifetime
            unit.object = new_object.release();
        }
        // If no factory, we assume the existing object will be reused
        // (common for assets that reload in-place)
    }

    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::execute_restore_phase(
    const std::vector<std::string>& units) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    for (const auto& unit_name : units) {
        auto it = m_units.find(unit_name);
        if (it == m_units.end()) continue;

        auto& unit = it->second;
        if (unit.state == ReloadUnitState::Failed) continue;
        if (!unit.object) continue;

        set_phase(ReloadPhase::Restoring, unit_name);
        unit.state = ReloadUnitState::Restoring;

        auto restore_result = unit.object->restore(std::move(unit.pending_snapshot));
        if (!restore_result) {
            unit.state = ReloadUnitState::Failed;
            return void_core::Err("Failed to restore unit " + unit_name + ": " +
                                  restore_result.error().message());
        }
    }

    return void_core::Ok();
}

void_core::Result<void> HotReloadOrchestrator::execute_finalize_phase(
    const std::vector<std::string>& units) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    for (const auto& unit_name : units) {
        auto it = m_units.find(unit_name);
        if (it == m_units.end()) continue;

        auto& unit = it->second;
        if (unit.state == ReloadUnitState::Failed) continue;
        if (!unit.object) continue;

        set_phase(ReloadPhase::Finalizing, unit_name);
        unit.state = ReloadUnitState::Finalizing;

        auto finish_result = unit.object->finish_reload();
        if (!finish_result) {
            unit.state = ReloadUnitState::Failed;
            // Don't fail the whole cycle, just mark this unit as failed
        }
    }

    return void_core::Ok();
}

void HotReloadOrchestrator::execute_rollback(
    const std::vector<std::string>& units,
    const std::string& failed_unit) {

    ++m_rollback_count;

    // Publish rollback event
    publish_event(ReloadRollbackEvent{
        m_current_reload_id,
        failed_unit,
        "Rollback initiated due to reload failure",
        units.size(),
        current_time()
    });

    // Rollback is complex - in production you'd restore from snapshots
    // For now, we mark units as rolled back
    std::lock_guard<std::mutex> lock(m_units_mutex);
    for (const auto& unit_name : units) {
        auto it = m_units.find(unit_name);
        if (it != m_units.end()) {
            it->second.state = ReloadUnitState::RolledBack;
        }
    }
}

std::vector<std::string> HotReloadOrchestrator::pending_units() const {
    return std::vector<std::string>(m_pending_reloads.begin(), m_pending_reloads.end());
}

// =============================================================================
// Snapshot/Restore Manual Control
// =============================================================================

void_core::Result<void_core::HotReloadSnapshot> HotReloadOrchestrator::snapshot_unit(
    const std::string& name) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_units.find(name);
    if (it == m_units.end()) {
        return void_core::Result<void_core::HotReloadSnapshot>{
            void_core::Error{"Unit not registered: " + name}};
    }

    if (!it->second.object) {
        return void_core::Result<void_core::HotReloadSnapshot>{
            void_core::Error{"Unit has no object: " + name}};
    }

    return it->second.object->snapshot();
}

void_core::Result<void> HotReloadOrchestrator::restore_unit(
    const std::string& name,
    void_core::HotReloadSnapshot snapshot) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_units.find(name);
    if (it == m_units.end()) {
        return void_core::Err("Unit not registered: " + name);
    }

    if (!it->second.object) {
        return void_core::Err("Unit has no object: " + name);
    }

    return it->second.object->restore(std::move(snapshot));
}

void_core::Result<std::map<std::string, void_core::HotReloadSnapshot>>
HotReloadOrchestrator::snapshot_all() {

    std::lock_guard<std::mutex> lock(m_units_mutex);
    std::map<std::string, void_core::HotReloadSnapshot> snapshots;

    for (const auto& [name, unit] : m_units) {
        if (!unit.object) continue;

        auto result = unit.object->snapshot();
        if (!result) {
            return void_core::Result<std::map<std::string, void_core::HotReloadSnapshot>>{
                void_core::Error{"Failed to snapshot " + name + ": " +
                                  result.error().message()}};
        }

        snapshots[name] = std::move(result).value();
    }

    return snapshots;
}

void_core::Result<void> HotReloadOrchestrator::restore_all(
    const std::map<std::string, void_core::HotReloadSnapshot>& snapshots) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    for (const auto& [name, snapshot] : snapshots) {
        auto it = m_units.find(name);
        if (it == m_units.end()) continue;
        if (!it->second.object) continue;

        auto result = it->second.object->restore(void_core::HotReloadSnapshot{snapshot});
        if (!result) {
            return void_core::Err("Failed to restore " + name + ": " +
                                  result.error().message());
        }
    }

    return void_core::Ok();
}

// =============================================================================
// File Watching
// =============================================================================

void_core::Result<void> HotReloadOrchestrator::watch_path(const std::string& path) {
    return m_file_watcher->watch(path);
}

void HotReloadOrchestrator::unwatch_path(const std::string& path) {
    m_file_watcher->unwatch(path);
}

void HotReloadOrchestrator::unwatch_all() {
    m_file_watcher->clear();
}

std::vector<std::string> HotReloadOrchestrator::watched_paths() const {
    // The file watcher doesn't expose watched paths directly
    // Return paths from registered units
    std::lock_guard<std::mutex> lock(m_units_mutex);
    std::vector<std::string> paths;
    for (const auto& [path, _] : m_path_to_unit) {
        paths.push_back(path);
    }
    return paths;
}

// =============================================================================
// Dependency Management
// =============================================================================

void HotReloadOrchestrator::add_dependency(
    const std::string& unit,
    const std::string& depends_on) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_units.find(unit);
    if (it == m_units.end()) return;

    auto& deps = it->second.dependencies;
    if (std::find(deps.begin(), deps.end(), depends_on) == deps.end()) {
        deps.push_back(depends_on);
    }
}

void HotReloadOrchestrator::remove_dependency(
    const std::string& unit,
    const std::string& depends_on) {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_units.find(unit);
    if (it == m_units.end()) return;

    auto& deps = it->second.dependencies;
    deps.erase(std::remove(deps.begin(), deps.end(), depends_on), deps.end());
}

std::vector<std::string> HotReloadOrchestrator::get_dependents(
    const std::string& unit) const {

    std::lock_guard<std::mutex> lock(m_units_mutex);
    std::vector<std::string> dependents;

    for (const auto& [name, u] : m_units) {
        if (u.depends_on(unit)) {
            dependents.push_back(name);
        }
    }

    return dependents;
}

std::vector<std::string> HotReloadOrchestrator::get_dependencies(
    const std::string& unit) const {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_units.find(unit);
    if (it == m_units.end()) {
        return {};
    }

    return it->second.dependencies;
}

std::vector<std::string> HotReloadOrchestrator::compute_reload_order(
    const std::vector<std::string>& units) const {

    std::lock_guard<std::mutex> lock(m_units_mutex);

    // Kahn's algorithm for topological sort
    std::map<std::string, std::set<std::string>> graph;      // unit -> dependents
    std::map<std::string, std::size_t> in_degree;            // unit -> dependency count

    // Build graph for requested units only
    std::set<std::string> unit_set(units.begin(), units.end());

    for (const auto& unit_name : units) {
        in_degree[unit_name] = 0;
        graph[unit_name] = {};
    }

    // Calculate in-degrees
    for (const auto& unit_name : units) {
        auto it = m_units.find(unit_name);
        if (it == m_units.end()) continue;

        for (const auto& dep : it->second.dependencies) {
            if (unit_set.count(dep)) {
                graph[dep].insert(unit_name);
                ++in_degree[unit_name];
            }
        }
    }

    // Process nodes with in-degree 0
    std::vector<std::string> result;
    std::vector<std::string> queue;

    // Sort by priority first, then by name for determinism
    std::vector<std::pair<std::string, ReloadPriority>> priority_queue;
    for (const auto& [name, deg] : in_degree) {
        if (deg == 0) {
            auto it = m_units.find(name);
            ReloadPriority prio = it != m_units.end() ? it->second.priority : ReloadPriority::Normal;
            priority_queue.push_back({name, prio});
        }
    }

    std::sort(priority_queue.begin(), priority_queue.end(),
              [](const auto& a, const auto& b) {
                  if (a.second != b.second) {
                      return static_cast<int>(a.second) < static_cast<int>(b.second);
                  }
                  return a.first < b.first;
              });

    for (const auto& [name, _] : priority_queue) {
        queue.push_back(name);
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.erase(queue.begin());
        result.push_back(current);

        for (const auto& dependent : graph[current]) {
            if (--in_degree[dependent] == 0) {
                // Insert maintaining priority order
                auto it = m_units.find(dependent);
                ReloadPriority prio = it != m_units.end() ? it->second.priority : ReloadPriority::Normal;

                auto insert_pos = std::lower_bound(queue.begin(), queue.end(), dependent,
                    [this, prio](const std::string& a, const std::string& b) {
                        auto it_a = m_units.find(a);
                        auto it_b = m_units.find(b);
                        ReloadPriority prio_a = it_a != m_units.end() ? it_a->second.priority : ReloadPriority::Normal;
                        ReloadPriority prio_b = it_b != m_units.end() ? it_b->second.priority : ReloadPriority::Normal;
                        if (prio_a != prio_b) {
                            return static_cast<int>(prio_a) < static_cast<int>(prio_b);
                        }
                        return a < b;
                    });
                queue.insert(insert_pos, dependent);
            }
        }
    }

    // If result size doesn't match input, there's a cycle - return original order
    if (result.size() != units.size()) {
        return units;
    }

    return result;
}

// =============================================================================
// Statistics
// =============================================================================

std::chrono::nanoseconds HotReloadOrchestrator::average_reload_time() const {
    if (m_total_reloads == 0) {
        return std::chrono::nanoseconds{0};
    }
    return m_total_reload_time / m_total_reloads;
}

void HotReloadOrchestrator::reset_statistics() {
    m_total_reloads = 0;
    m_successful_reloads = 0;
    m_failed_reloads = 0;
    m_rollback_count = 0;
    m_total_reload_time = std::chrono::nanoseconds{0};

    std::lock_guard<std::mutex> lock(m_units_mutex);
    for (auto& [_, unit] : m_units) {
        unit.reload_count = 0;
        unit.total_reload_time = std::chrono::nanoseconds{0};
        unit.avg_reload_time = std::chrono::nanoseconds{0};
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

void HotReloadOrchestrator::set_phase(ReloadPhase phase, const std::string& current_unit) {
    ReloadPhase old_phase = m_current_phase;
    m_current_phase = phase;

    if (old_phase != phase) {
        publish_event(ReloadPhaseChangedEvent{
            m_current_reload_id,
            old_phase,
            phase,
            current_unit,
            current_time()
        });
    }
}

std::string HotReloadOrchestrator::generate_reload_id() const {
    // Generate a unique ID: timestamp + random suffix
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);

    std::ostringstream oss;
    oss << "reload_" << std::hex << ms << "_" << std::setw(4) << std::setfill('0') << dis(gen);
    return oss.str();
}

double HotReloadOrchestrator::current_time() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(now - m_start_time);
    return duration.count();
}

template<typename E>
void HotReloadOrchestrator::publish_event(E&& event) {
    if (m_event_bus) {
        m_event_bus->publish(std::forward<E>(event));
    }
}

const std::string* HotReloadOrchestrator::find_unit_by_path(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_units_mutex);

    auto it = m_path_to_unit.find(path);
    if (it != m_path_to_unit.end()) {
        return &it->second;
    }

    // Try partial match (for paths that might differ slightly)
    for (const auto& [registered_path, unit_name] : m_path_to_unit) {
        // Check if path ends with registered path or vice versa
        if (path.size() >= registered_path.size() &&
            path.compare(path.size() - registered_path.size(),
                        registered_path.size(), registered_path) == 0) {
            return &unit_name;
        }
        if (registered_path.size() >= path.size() &&
            registered_path.compare(registered_path.size() - path.size(),
                                   path.size(), path) == 0) {
            // Store in mutable location - in real code you'd cache this
            static thread_local std::string found_name;
            found_name = unit_name;
            return &found_name;
        }
    }

    return nullptr;
}

// Explicit template instantiations for event publishing
template void HotReloadOrchestrator::publish_event<ReloadCycleStartedEvent>(ReloadCycleStartedEvent&&);
template void HotReloadOrchestrator::publish_event<ReloadPhaseChangedEvent>(ReloadPhaseChangedEvent&&);
template void HotReloadOrchestrator::publish_event<UnitSnapshotTakenEvent>(UnitSnapshotTakenEvent&&);
template void HotReloadOrchestrator::publish_event<UnitReloadedEvent>(UnitReloadedEvent&&);
template void HotReloadOrchestrator::publish_event<ReloadCycleCompletedEvent>(ReloadCycleCompletedEvent&&);
template void HotReloadOrchestrator::publish_event<ReloadRollbackEvent>(ReloadRollbackEvent&&);

} // namespace void_kernel
