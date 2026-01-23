/// @file lifecycle.hpp
/// @brief Lifecycle management for void_engine
///
/// Provides hooks into engine lifecycle phases for initialization
/// and shutdown coordination.

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace void_engine {

// =============================================================================
// Lifecycle Hook
// =============================================================================

/// Priority for lifecycle hooks (lower = earlier)
enum class HookPriority : std::int32_t {
    Critical = -1000,    ///< Critical system hooks (first)
    System = -100,       ///< Core system hooks
    Default = 0,         ///< Normal hooks
    User = 100,          ///< User hooks
    Late = 1000,         ///< Late hooks (last)
};

/// Hook callback signature
using LifecycleCallback = std::function<void_core::Result<void>(Engine&)>;

/// A lifecycle hook entry
struct LifecycleHook {
    std::string name;
    LifecyclePhase phase;
    LifecycleCallback callback;
    HookPriority priority = HookPriority::Default;
    bool enabled = true;
    bool once = false;      ///< Remove after first execution

    /// Create a hook
    [[nodiscard]] static LifecycleHook create(
        const std::string& name,
        LifecyclePhase phase,
        LifecycleCallback callback,
        HookPriority priority = HookPriority::Default)
    {
        return LifecycleHook{name, phase, std::move(callback), priority, true, false};
    }

    /// Create a one-shot hook
    [[nodiscard]] static LifecycleHook create_once(
        const std::string& name,
        LifecyclePhase phase,
        LifecycleCallback callback,
        HookPriority priority = HookPriority::Default)
    {
        return LifecycleHook{name, phase, std::move(callback), priority, true, true};
    }
};

// =============================================================================
// Lifecycle Manager
// =============================================================================

/// Manages engine lifecycle phases and hooks
class LifecycleManager {
public:
    LifecycleManager() = default;
    ~LifecycleManager() = default;

    // Non-copyable
    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;

    // =========================================================================
    // Hook Registration
    // =========================================================================

    /// Register a lifecycle hook
    void register_hook(LifecycleHook hook);

    /// Register multiple hooks
    void register_hooks(std::vector<LifecycleHook> hooks);

    /// Unregister a hook by name
    bool unregister_hook(const std::string& name);

    /// Enable/disable a hook
    void set_hook_enabled(const std::string& name, bool enabled);

    /// Check if hook exists
    [[nodiscard]] bool has_hook(const std::string& name) const;

    // =========================================================================
    // Convenience Registration
    // =========================================================================

    /// Register init hook (CoreInit phase)
    void on_init(const std::string& name, LifecycleCallback callback,
                 HookPriority priority = HookPriority::Default);

    /// Register ready hook (Ready phase)
    void on_ready(const std::string& name, LifecycleCallback callback,
                  HookPriority priority = HookPriority::Default);

    /// Register shutdown hook (CoreShutdown phase)
    void on_shutdown(const std::string& name, LifecycleCallback callback,
                     HookPriority priority = HookPriority::Default);

    /// Register pre-update hook (called before each frame)
    void on_pre_update(const std::string& name, LifecycleCallback callback,
                       HookPriority priority = HookPriority::Default);

    /// Register post-update hook (called after each frame)
    void on_post_update(const std::string& name, LifecycleCallback callback,
                        HookPriority priority = HookPriority::Default);

    // =========================================================================
    // Phase Management
    // =========================================================================

    /// Get current phase
    [[nodiscard]] LifecyclePhase current_phase() const { return m_current_phase; }

    /// Transition to a new phase
    void_core::Result<void> transition_to(LifecyclePhase phase, Engine& engine);

    /// Execute hooks for current phase
    void_core::Result<void> execute_current_phase(Engine& engine);

    /// Execute hooks for specific phase
    void_core::Result<void> execute_phase(LifecyclePhase phase, Engine& engine);

    // =========================================================================
    // Frame Hooks
    // =========================================================================

    /// Execute pre-update hooks
    void_core::Result<void> pre_update(Engine& engine);

    /// Execute post-update hooks
    void_core::Result<void> post_update(Engine& engine);

    // =========================================================================
    // Events
    // =========================================================================

    /// Set callback for phase changes
    void set_on_phase_change(std::function<void(const LifecycleEvent&)> callback);

    /// Get lifecycle events history
    [[nodiscard]] const std::vector<LifecycleEvent>& events() const { return m_events; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get time spent in phase
    [[nodiscard]] std::chrono::nanoseconds phase_duration(LifecyclePhase phase) const;

    /// Get total time in all phases
    [[nodiscard]] std::chrono::nanoseconds total_init_time() const;

    /// Get hook count
    [[nodiscard]] std::size_t hook_count() const;

    /// Get hook count for phase
    [[nodiscard]] std::size_t hook_count(LifecyclePhase phase) const;

private:
    /// Get hooks for phase (sorted by priority)
    [[nodiscard]] std::vector<LifecycleHook*> get_hooks_for_phase(LifecyclePhase phase);

    /// Record phase transition
    void record_transition(LifecyclePhase old_phase, LifecyclePhase new_phase);

private:
    LifecyclePhase m_current_phase = LifecyclePhase::PreInit;

    // Hooks stored by name for easy lookup
    std::map<std::string, LifecycleHook> m_hooks;
    mutable std::mutex m_hooks_mutex;

    // Frame hooks (executed every frame)
    std::vector<std::string> m_pre_update_hooks;
    std::vector<std::string> m_post_update_hooks;

    // Events history
    std::vector<LifecycleEvent> m_events;

    // Phase timing
    std::map<LifecyclePhase, std::chrono::nanoseconds> m_phase_durations;
    std::chrono::steady_clock::time_point m_phase_start_time;

    // Callbacks
    std::function<void(const LifecycleEvent&)> m_on_phase_change;
};

// =============================================================================
// Lifecycle Guard
// =============================================================================

/// RAII guard for automatic shutdown
class LifecycleGuard {
public:
    /// Create guard with shutdown callback
    explicit LifecycleGuard(std::function<void()> shutdown)
        : m_shutdown(std::move(shutdown)), m_active(true) {}

    ~LifecycleGuard() {
        release();
    }

    // Non-copyable
    LifecycleGuard(const LifecycleGuard&) = delete;
    LifecycleGuard& operator=(const LifecycleGuard&) = delete;

    // Movable
    LifecycleGuard(LifecycleGuard&& other) noexcept
        : m_shutdown(std::move(other.m_shutdown))
        , m_active(other.m_active)
    {
        other.m_active = false;
    }

    LifecycleGuard& operator=(LifecycleGuard&& other) noexcept {
        if (this != &other) {
            release();
            m_shutdown = std::move(other.m_shutdown);
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

    /// Release without calling shutdown
    void dismiss() { m_active = false; }

    /// Trigger shutdown early
    void release() {
        if (m_active && m_shutdown) {
            m_active = false;
            m_shutdown();
        }
    }

private:
    std::function<void()> m_shutdown;
    bool m_active;
};

// =============================================================================
// Scoped Phase
// =============================================================================

/// RAII helper for phase transitions
class ScopedPhase {
public:
    ScopedPhase(LifecycleManager& manager, LifecyclePhase phase, Engine& engine)
        : m_manager(manager)
        , m_engine(engine)
        , m_previous_phase(manager.current_phase())
    {
        m_manager.transition_to(phase, m_engine);
    }

    ~ScopedPhase() {
        // Don't auto-restore - phase transitions are explicit
    }

    // Non-copyable, non-movable
    ScopedPhase(const ScopedPhase&) = delete;
    ScopedPhase& operator=(const ScopedPhase&) = delete;

    /// Get previous phase
    [[nodiscard]] LifecyclePhase previous_phase() const { return m_previous_phase; }

private:
    LifecycleManager& m_manager;
    Engine& m_engine;
    LifecyclePhase m_previous_phase;
};

} // namespace void_engine
