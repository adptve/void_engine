/// @file types.hpp
/// @brief Core types for void_kernel
///
/// Provides fundamental types used throughout the kernel system:
/// - Module identification and metadata
/// - Supervisor restart strategies
/// - Resource limits and permissions
/// - Kernel configuration

#pragma once

#include "fwd.hpp"

#include <void_engine/core/version.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace void_kernel {

// =============================================================================
// Module Types
// =============================================================================

/// Unique identifier for a module
struct ModuleId {
    std::uint64_t value = 0;

    constexpr ModuleId() = default;
    constexpr explicit ModuleId(std::uint64_t v) : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const { return value != 0; }
    constexpr bool operator==(const ModuleId& other) const { return value == other.value; }
    constexpr bool operator!=(const ModuleId& other) const { return value != other.value; }
    constexpr bool operator<(const ModuleId& other) const { return value < other.value; }

    /// Create from name hash
    static ModuleId from_name(const std::string& name);
};

/// Module state enumeration
enum class ModuleState : std::uint8_t {
    Unloaded,       ///< Not loaded
    Loading,        ///< Currently loading
    Loaded,         ///< Loaded but not initialized
    Initializing,   ///< Running initialization
    Ready,          ///< Fully initialized and ready
    Running,        ///< Actively running
    Stopping,       ///< Shutting down
    Unloading,      ///< Being unloaded
    Failed,         ///< Failed to load or initialize
    Reloading,      ///< Hot-reloading in progress
};

/// Convert module state to string
[[nodiscard]] const char* to_string(ModuleState state);

/// Module information
struct ModuleInfo {
    std::string name;
    std::string description;
    void_core::Version version;
    std::vector<std::string> dependencies;
    std::vector<std::string> optional_dependencies;
    std::string author;
    std::string license;
    bool supports_hot_reload = false;

    /// Check if this module depends on another
    [[nodiscard]] bool depends_on(const std::string& module_name) const;
};

/// Module load result
struct ModuleLoadResult {
    bool success = false;
    ModuleId id;
    std::string error_message;
    std::chrono::nanoseconds load_time{0};
};

// =============================================================================
// Supervisor Types
// =============================================================================

/// Restart strategy for supervised tasks
enum class RestartStrategy : std::uint8_t {
    /// Restart only the failed child
    OneForOne,

    /// Restart all children when one fails
    OneForAll,

    /// Restart the failed child and all children started after it
    RestForOne,

    /// Never restart (transient tasks)
    Temporary,

    /// Restart only on abnormal termination
    Transient,
};

/// Convert restart strategy to string
[[nodiscard]] const char* to_string(RestartStrategy strategy);

/// Restart intensity limits (max restarts in time window)
struct RestartLimits {
    std::uint32_t max_restarts = 3;
    std::chrono::seconds time_window{60};

    /// Check if restart is allowed given current count
    [[nodiscard]] bool allows_restart(std::uint32_t current_count) const {
        return current_count < max_restarts;
    }
};

/// Child process state
enum class ChildState : std::uint8_t {
    Stopped,        ///< Not running
    Starting,       ///< Being started
    Running,        ///< Actively running
    Stopping,       ///< Being stopped
    Restarting,     ///< Being restarted
    Failed,         ///< Crashed or failed
    Terminated,     ///< Cleanly terminated
};

/// Convert child state to string
[[nodiscard]] const char* to_string(ChildState state);

/// Child specification for supervisor
struct ChildSpec {
    std::string name;
    std::function<void()> start_fn;
    std::function<void()> stop_fn;
    RestartStrategy restart = RestartStrategy::Transient;
    std::chrono::milliseconds shutdown_timeout{5000};
    std::vector<std::string> dependencies;
    std::uint32_t priority = 100;  ///< Lower = starts first

    /// Builder pattern for fluent configuration
    ChildSpec& with_restart(RestartStrategy s) { restart = s; return *this; }
    ChildSpec& with_timeout(std::chrono::milliseconds t) { shutdown_timeout = t; return *this; }
    ChildSpec& with_dependency(const std::string& dep) { dependencies.push_back(dep); return *this; }
    ChildSpec& with_priority(std::uint32_t p) { priority = p; return *this; }
};

/// Supervisor configuration
struct SupervisorConfig {
    std::string name = "supervisor";
    RestartStrategy strategy = RestartStrategy::OneForOne;
    RestartLimits limits;
    std::chrono::milliseconds max_restart_delay{30000};
    std::chrono::milliseconds base_restart_delay{100};
    float restart_delay_multiplier = 2.0f;
    bool auto_start = true;

    /// Builder pattern
    SupervisorConfig& with_strategy(RestartStrategy s) { strategy = s; return *this; }
    SupervisorConfig& with_limits(std::uint32_t max, std::chrono::seconds window) {
        limits = {max, window};
        return *this;
    }
};

/// Supervisor state
enum class SupervisorState : std::uint8_t {
    Stopped,
    Starting,
    Running,
    Stopping,
    Failed,
};

/// Convert supervisor state to string
[[nodiscard]] const char* to_string(SupervisorState state);

// =============================================================================
// Sandbox Types
// =============================================================================

/// Permission flags (can be combined with bitwise OR)
enum class Permission : std::uint32_t {
    None            = 0,

    // File system
    FileRead        = 1 << 0,
    FileWrite       = 1 << 1,
    FileCreate      = 1 << 2,
    FileDelete      = 1 << 3,
    FileExecute     = 1 << 4,

    // Network
    NetworkConnect  = 1 << 5,
    NetworkListen   = 1 << 6,
    NetworkUdp      = 1 << 7,

    // Process
    ProcessSpawn    = 1 << 8,
    ProcessKill     = 1 << 9,
    ThreadCreate    = 1 << 10,

    // System
    SystemInfo      = 1 << 11,
    EnvironmentRead = 1 << 12,
    TimeAccess      = 1 << 13,
    RandomAccess    = 1 << 14,

    // Engine specific
    AssetRead       = 1 << 15,
    AssetWrite      = 1 << 16,
    SceneModify     = 1 << 17,
    EntityCreate    = 1 << 18,
    EntityDestroy   = 1 << 19,
    ComponentAccess = 1 << 20,
    ServiceCall     = 1 << 21,
    EventPublish    = 1 << 22,
    EventSubscribe  = 1 << 23,

    // Convenience combinations
    FileAll         = FileRead | FileWrite | FileCreate | FileDelete | FileExecute,
    NetworkAll      = NetworkConnect | NetworkListen | NetworkUdp,
    ProcessAll      = ProcessSpawn | ProcessKill | ThreadCreate,
    AssetAll        = AssetRead | AssetWrite,
    EntityAll       = EntityCreate | EntityDestroy | ComponentAccess,
    EventAll        = EventPublish | EventSubscribe,

    /// Full access (dangerous - for trusted code only)
    All             = 0xFFFFFFFF,
};

/// Bitwise operators for Permission
constexpr Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
constexpr Permission operator&(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
constexpr Permission operator~(Permission a) {
    return static_cast<Permission>(~static_cast<std::uint32_t>(a));
}
constexpr Permission& operator|=(Permission& a, Permission b) {
    a = a | b;
    return a;
}
constexpr Permission& operator&=(Permission& a, Permission b) {
    a = a & b;
    return a;
}

/// Check if permission set contains a specific permission
[[nodiscard]] constexpr bool has_permission(Permission set, Permission check) {
    return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(check)) ==
           static_cast<std::uint32_t>(check);
}

/// Resource limits for sandbox
struct ResourceLimits {
    std::size_t max_memory_bytes = 256 * 1024 * 1024;  ///< 256 MB default
    std::size_t max_stack_bytes = 1 * 1024 * 1024;     ///< 1 MB stack
    std::uint64_t max_cpu_time_us = 0;                  ///< 0 = unlimited
    std::uint64_t max_instructions = 0;                 ///< 0 = unlimited
    std::uint32_t max_file_handles = 64;
    std::uint32_t max_threads = 4;
    std::uint32_t max_allocations = 100000;

    /// Create default limits
    static ResourceLimits defaults() { return ResourceLimits{}; }

    /// Create unlimited limits (for trusted code)
    static ResourceLimits unlimited() {
        return ResourceLimits{
            .max_memory_bytes = std::numeric_limits<std::size_t>::max(),
            .max_stack_bytes = std::numeric_limits<std::size_t>::max(),
            .max_cpu_time_us = 0,
            .max_instructions = 0,
            .max_file_handles = std::numeric_limits<std::uint32_t>::max(),
            .max_threads = std::numeric_limits<std::uint32_t>::max(),
            .max_allocations = std::numeric_limits<std::uint32_t>::max(),
        };
    }

    /// Create strict limits (for untrusted code)
    static ResourceLimits strict() {
        return ResourceLimits{
            .max_memory_bytes = 64 * 1024 * 1024,  // 64 MB
            .max_stack_bytes = 512 * 1024,          // 512 KB
            .max_cpu_time_us = 1000000,             // 1 second
            .max_instructions = 10000000,
            .max_file_handles = 8,
            .max_threads = 1,
            .max_allocations = 10000,
        };
    }
};

/// Sandbox configuration
struct SandboxConfig {
    std::string name = "sandbox";
    Permission permissions = Permission::None;
    ResourceLimits limits;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> allowed_hosts;
    bool inherit_environment = false;
    bool allow_debugging = false;

    /// Builder pattern
    SandboxConfig& with_permission(Permission p) { permissions |= p; return *this; }
    SandboxConfig& with_limits(const ResourceLimits& l) { limits = l; return *this; }
    SandboxConfig& allow_path(const std::string& path) { allowed_paths.push_back(path); return *this; }
    SandboxConfig& allow_host(const std::string& host) { allowed_hosts.push_back(host); return *this; }

    /// Create config for trusted code
    static SandboxConfig trusted(const std::string& name = "trusted") {
        return SandboxConfig{
            .name = name,
            .permissions = Permission::All,
            .limits = ResourceLimits::unlimited(),
            .inherit_environment = true,
            .allow_debugging = true,
        };
    }

    /// Create config for untrusted code
    static SandboxConfig untrusted(const std::string& name = "untrusted") {
        return SandboxConfig{
            .name = name,
            .permissions = Permission::AssetRead | Permission::EntityAll | Permission::EventAll,
            .limits = ResourceLimits::strict(),
            .inherit_environment = false,
            .allow_debugging = false,
        };
    }
};

/// Sandbox state
enum class SandboxState : std::uint8_t {
    Created,
    Running,
    Suspended,
    Terminated,
    Violated,  ///< Security violation detected
};

/// Convert sandbox state to string
[[nodiscard]] const char* to_string(SandboxState state);

// =============================================================================
// Frame Stage Types (for Runtime frame loop)
// =============================================================================

/// @brief Frame execution stages
///
/// These stages define the order of execution within each frame.
/// Systems register into stages; Kernel executes stages in order.
/// This separates frame execution order from initialization order.
enum class Stage : std::uint8_t {
    Input,          ///< Poll and process input events
    HotReloadPoll,  ///< Check for hot-reload (plugins, widgets, assets)
    EventDispatch,  ///< Dispatch queued events
    Update,         ///< Variable timestep update (gameplay, AI, etc.)
    FixedUpdate,    ///< Fixed timestep update (physics)
    PostFixed,      ///< Post-physics (trigger events, collision response)
    RenderPrepare,  ///< Prepare render state (culling, batching)
    Render,         ///< Submit render commands
    UI,             ///< UI update and render
    Audio,          ///< Audio update
    Streaming,      ///< Asset streaming, API sync

    _Count          ///< Number of stages (internal use)
};

/// Convert Stage to string
[[nodiscard]] inline const char* to_string(Stage stage) {
    switch (stage) {
        case Stage::Input:         return "Input";
        case Stage::HotReloadPoll: return "HotReloadPoll";
        case Stage::EventDispatch: return "EventDispatch";
        case Stage::Update:        return "Update";
        case Stage::FixedUpdate:   return "FixedUpdate";
        case Stage::PostFixed:     return "PostFixed";
        case Stage::RenderPrepare: return "RenderPrepare";
        case Stage::Render:        return "Render";
        case Stage::UI:            return "UI";
        case Stage::Audio:         return "Audio";
        case Stage::Streaming:     return "Streaming";
        case Stage::_Count:        return "_Count";
    }
    return "Unknown";
}

/// @brief System function signature
/// @param dt Delta time in seconds (for Update), or fixed timestep (for FixedUpdate)
using SystemFunc = std::function<void(float dt)>;

/// @brief System registration info
struct SystemInfo {
    std::string name;
    SystemFunc func;
    std::int32_t priority{0};  ///< Lower = runs first within stage
    bool enabled{true};
};

/// @brief Stage configuration
struct StageConfig {
    bool enabled{true};
    bool profile{false};  ///< Collect timing stats
};

// =============================================================================
// Kernel Types
// =============================================================================

/// Kernel phase enumeration
enum class KernelPhase : std::uint8_t {
    PreInit,        ///< Before any initialization
    CoreInit,       ///< Core systems initializing
    ServiceInit,    ///< Services starting
    ModuleInit,     ///< Modules loading
    PluginInit,     ///< Plugins loading
    Ready,          ///< Fully initialized
    Running,        ///< Main loop active
    Shutdown,       ///< Shutting down
    Terminated,     ///< Fully terminated
};

/// Convert kernel phase to string
[[nodiscard]] const char* to_string(KernelPhase phase);

/// Kernel configuration
struct KernelConfig {
    std::string name = "void_engine";
    std::string config_path = "config/";
    std::string module_path = "modules/";
    std::string plugin_path = "plugins/";
    std::string asset_path = "assets/";
    std::uint32_t target_fps = 60;
    bool enable_hot_reload = true;
    bool enable_profiling = false;
    bool enable_validation = true;
    std::chrono::milliseconds hot_reload_poll_interval{100};
    std::chrono::milliseconds health_check_interval{1000};
    std::uint32_t worker_thread_count = 0;  ///< 0 = auto-detect

    /// Builder pattern
    KernelConfig& with_name(const std::string& n) { name = n; return *this; }
    KernelConfig& with_fps(std::uint32_t fps) { target_fps = fps; return *this; }
    KernelConfig& with_hot_reload(bool enable) { enable_hot_reload = enable; return *this; }
    KernelConfig& with_workers(std::uint32_t count) { worker_thread_count = count; return *this; }
};

/// Kernel statistics
struct KernelStats {
    std::uint64_t frame_count = 0;
    std::uint64_t total_modules = 0;
    std::uint64_t active_modules = 0;
    std::uint64_t total_services = 0;
    std::uint64_t active_services = 0;
    std::uint64_t total_plugins = 0;
    std::uint64_t active_plugins = 0;
    std::uint64_t hot_reloads = 0;
    std::uint64_t supervisor_restarts = 0;
    std::uint64_t sandbox_violations = 0;
    std::chrono::nanoseconds uptime{0};
    std::chrono::nanoseconds last_frame_time{0};
    std::chrono::nanoseconds avg_frame_time{0};
    float cpu_usage = 0.0f;
    std::size_t memory_used = 0;
    std::size_t memory_peak = 0;
};

// =============================================================================
// Event Types
// =============================================================================

/// Module loaded event
struct ModuleLoadedEvent {
    ModuleId id;
    std::string name;
    void_core::Version version;
    std::chrono::nanoseconds load_time;
};

/// Module unloaded event
struct ModuleUnloadedEvent {
    ModuleId id;
    std::string name;
    bool was_reloading = false;
};

/// Module reload event
struct ModuleReloadEvent {
    ModuleId id;
    std::string name;
    void_core::Version old_version;
    void_core::Version new_version;
    bool success = false;
    std::string error;
};

/// Child event types
enum class ChildEventType : std::uint8_t {
    Started,
    Stopped,
    Crashed,
    Restarted,
    TerminatedNormally,
    TerminatedAbnormally,
};

/// Supervisor child event
struct ChildEvent {
    std::string supervisor_name;
    std::string child_name;
    ChildEventType type;
    std::optional<std::string> error_message;
    std::uint32_t restart_count = 0;
};

/// Sandbox violation event
struct SandboxViolationEvent {
    std::string sandbox_name;
    Permission attempted_permission;
    std::string details;
    std::chrono::system_clock::time_point timestamp;
};

/// Kernel phase change event
struct KernelPhaseEvent {
    KernelPhase old_phase;
    KernelPhase new_phase;
    std::chrono::system_clock::time_point timestamp;
};

} // namespace void_kernel

// =============================================================================
// Hash specializations for std::unordered_map
// =============================================================================

namespace std {

template<>
struct hash<void_kernel::ModuleId> {
    std::size_t operator()(const void_kernel::ModuleId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
