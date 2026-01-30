/// @file runtime.hpp
/// @brief Runtime - Application lifecycle owner
///
/// Runtime is the top-level application owner. It handles:
/// - Process lifecycle (startup, run, shutdown)
/// - Kernel initialization
/// - World creation/destruction/switching
/// - Frame loop execution
/// - Mode selection (headless/windowed/XR/editor)
/// - API connectivity and deployment updates
///
/// Runtime does NOT:
/// - Contain gameplay logic (that's in plugins)
/// - Schedule systems directly (that's Kernel's job)
/// - Manage hot-reload details (that's Kernel's job)
///
/// Architecture (from doc/review):
/// ```
/// main() -> Runtime::initialize() -> Runtime::run() -> Runtime::shutdown()
///              |
///              +-> Kernel init (stages, hot-reload orchestration)
///              +-> Foundation boot (memory, core)
///              +-> Infrastructure boot (event bus, services)
///              +-> API connectivity
///              +-> Platform init (presenter, render, compositor)
///              +-> I/O init (input, audio)
///              +-> Simulation base (ECS, physics, triggers)
///              +-> World loading
///              +-> Plugin activation
///              +-> Widget activation
/// ```

#pragma once

#include "runtime_config.hpp"
#include "platform.hpp"

#include <void_engine/core/error.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

// Forward declarations
namespace void_kernel { class Kernel; }
namespace void_event { class EventBus; }
namespace void_ecs { class World; }
namespace void_scene { class World; struct SceneData; }
namespace void_render { class SceneRenderer; }
namespace void_package {
    class PackageRegistry;
    class LoadContext;
    class WorldComposer;
    class PrefabRegistry;
    class ComponentSchemaRegistry;
    class DefinitionRegistry;
    class WidgetManager;
    class LayerApplier;
}

namespace void_runtime {

// =============================================================================
// Runtime State
// =============================================================================

/// @brief Current state of the runtime
enum class RuntimeState : std::uint8_t {
    Uninitialized,  ///< Not yet initialized
    Initializing,   ///< In initialization sequence
    Ready,          ///< Initialized, ready to run
    Running,        ///< Main loop executing
    Paused,         ///< Paused (background on mobile, etc.)
    ShuttingDown,   ///< In shutdown sequence
    Terminated      ///< Shutdown complete
};

/// @brief Convert RuntimeState to string
inline const char* to_string(RuntimeState state) {
    switch (state) {
        case RuntimeState::Uninitialized: return "Uninitialized";
        case RuntimeState::Initializing:  return "Initializing";
        case RuntimeState::Ready:         return "Ready";
        case RuntimeState::Running:       return "Running";
        case RuntimeState::Paused:        return "Paused";
        case RuntimeState::ShuttingDown:  return "ShuttingDown";
        case RuntimeState::Terminated:    return "Terminated";
    }
    return "Unknown";
}

// =============================================================================
// Runtime
// =============================================================================

/// @brief Application lifecycle owner
///
/// Usage:
/// ```cpp
/// void_runtime::RuntimeConfig config;
/// config.mode = void_runtime::RuntimeMode::Windowed;
/// config.initial_world = "main_menu";
///
/// void_runtime::Runtime runtime(config);
///
/// if (auto result = runtime.initialize(); !result) {
///     spdlog::error("Init failed: {}", result.error());
///     return EXIT_FAILURE;
/// }
///
/// int exit_code = runtime.run();  // Blocks until exit
/// runtime.shutdown();
/// return exit_code;
/// ```
class Runtime {
public:
    explicit Runtime(const RuntimeConfig& config);
    ~Runtime();

    // Non-copyable, non-movable (singleton-like lifecycle)
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// @brief Initialize the runtime
    ///
    /// Performs full boot sequence:
    /// 1. Kernel init (stages, hot-reload orchestration)
    /// 2. Foundation (memory, core structures)
    /// 3. Infrastructure (event bus, services)
    /// 4. API connectivity (if configured)
    /// 5. Platform (presenter, render, compositor) - skipped in headless
    /// 6. I/O (input, audio)
    /// 7. Simulation base (ECS, physics, triggers)
    /// 8. Load initial world (if configured)
    /// 9. Activate plugins
    /// 10. Activate widgets
    ///
    /// @return Ok() on success, Error with message on failure
    [[nodiscard]] void_core::Result<void> initialize();

    /// @brief Run the main loop
    ///
    /// Blocks until exit is requested. Returns exit code.
    /// The main loop executes frame stages via Kernel:
    /// - Input
    /// - HotReloadPoll
    /// - EventDispatch
    /// - Update
    /// - FixedUpdate
    /// - PostFixed
    /// - RenderPrepare
    /// - Render
    /// - UI
    /// - Audio
    /// - Streaming/API sync
    ///
    /// @return Exit code (0 = success)
    [[nodiscard]] int run();

    /// @brief Shutdown the runtime
    ///
    /// Performs graceful shutdown in reverse order:
    /// - Deactivate widgets
    /// - Deactivate plugins
    /// - Unload world
    /// - Shutdown simulation
    /// - Shutdown I/O
    /// - Shutdown platform
    /// - Shutdown infrastructure
    /// - Shutdown kernel
    void shutdown();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    /// @brief Get current runtime state
    [[nodiscard]] RuntimeState state() const { return m_state; }

    /// @brief Check if running
    [[nodiscard]] bool is_running() const { return m_state == RuntimeState::Running; }

    /// @brief Request exit
    void request_exit(int exit_code = 0);

    /// @brief Check if exit requested
    [[nodiscard]] bool exit_requested() const { return m_exit_requested; }

    // -------------------------------------------------------------------------
    // World Management
    // -------------------------------------------------------------------------

    /// @brief Load a world
    ///
    /// Worlds are loaded via API or from local paths.
    /// Loading a world:
    /// 1. Streams required assets
    /// 2. Instantiates ECS entities
    /// 3. Activates layers
    /// 4. Activates world-specific plugins
    /// 5. Activates world-specific widgets
    ///
    /// @param world_id World identifier (name or path)
    /// @return Ok() on success
    [[nodiscard]] void_core::Result<void> load_world(const std::string& world_id);

    /// @brief Unload current world
    ///
    /// Unloading a world:
    /// 1. Snapshots state (if requested)
    /// 2. Deactivates world widgets
    /// 3. Deactivates world plugins
    /// 4. Deactivates layers
    /// 5. Destroys ECS entities
    ///
    /// @param snapshot If true, snapshot state for potential restore
    void unload_world(bool snapshot = false);

    /// @brief Switch to a different world
    ///
    /// Equivalent to unload_world() + load_world() with optional state transfer.
    ///
    /// @param world_id Target world identifier
    /// @param transfer_state If true, attempt to migrate compatible state
    /// @return Ok() on success
    [[nodiscard]] void_core::Result<void> switch_world(const std::string& world_id,
                                                        bool transfer_state = false);

    /// @brief Get current world name
    [[nodiscard]] const std::string& current_world() const { return m_current_world; }

    /// @brief Check if a world is loaded
    [[nodiscard]] bool has_world() const { return !m_current_world.empty(); }

    // -------------------------------------------------------------------------
    // Subsystem Access
    // -------------------------------------------------------------------------

    /// @brief Get the Kernel
    [[nodiscard]] void_kernel::Kernel* kernel() const { return m_kernel.get(); }

    /// @brief Get the event bus
    [[nodiscard]] void_event::EventBus* event_bus() const { return m_event_bus.get(); }

    /// @brief Get the ECS world (current world's ECS)
    /// Returns the ECS world from the active WorldComposer
    [[nodiscard]] void_ecs::World* ecs_world() const;

    /// @brief Get the WorldComposer
    /// The WorldComposer manages world lifecycle through the package system
    [[nodiscard]] void_package::WorldComposer* world_composer() const;

    /// @brief Get the PackageRegistry
    [[nodiscard]] void_package::PackageRegistry* package_registry() const;

    /// @brief Get the PrefabRegistry
    [[nodiscard]] void_package::PrefabRegistry* prefab_registry() const;

    /// @brief Get the platform interface
    [[nodiscard]] IPlatform* platform() const;

    /// @brief Get the scene renderer
    [[nodiscard]] void_render::SceneRenderer* renderer() const;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /// @brief Get the runtime configuration
    [[nodiscard]] const RuntimeConfig& config() const { return m_config; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using FrameCallback = std::function<void(float dt)>;
    using WorldCallback = std::function<void(const std::string& world_id)>;

    /// @brief Register callback for each frame (after all stages)
    void on_frame(FrameCallback callback) { m_on_frame = std::move(callback); }

    /// @brief Register callback for world load
    void on_world_loaded(WorldCallback callback) { m_on_world_loaded = std::move(callback); }

    /// @brief Register callback for world unload
    void on_world_unloaded(WorldCallback callback) { m_on_world_unloaded = std::move(callback); }

private:
    // Boot sequence phases
    void_core::Result<void> init_kernel();
    void_core::Result<void> init_foundation();
    void_core::Result<void> init_infrastructure();
    void_core::Result<void> init_packages();
    void_core::Result<void> init_api_connectivity();
    void_core::Result<void> init_platform();
    void_core::Result<void> init_render();
    void_core::Result<void> init_io();
    void_core::Result<void> init_simulation();

    // Frame execution
    void execute_frame(float dt);
    void poll_events();
    void process_platform_events();
    void handle_platform_event(const PlatformEvent& evt);

    // Shutdown phases
    void shutdown_simulation();
    void shutdown_io();
    void shutdown_render();
    void shutdown_platform();
    void shutdown_packages();
    void shutdown_infrastructure();
    void shutdown_kernel();

    // Engine core registration (Phase 2)
    void register_engine_core_components();
    void register_engine_render_systems();
    void init_render_context();

    RuntimeConfig m_config;
    RuntimeState m_state{RuntimeState::Uninitialized};

    bool m_exit_requested{false};
    int m_exit_code{0};

    std::string m_current_world;

    // Core subsystems (owned)
    std::unique_ptr<void_kernel::Kernel> m_kernel;
    std::unique_ptr<void_event::EventBus> m_event_bus;

    // Package system (owns world lifecycle)
    struct PackageContext;
    std::unique_ptr<PackageContext> m_packages;

    // Platform handle (opaque, mode-dependent)
    struct PlatformContext;
    std::unique_ptr<PlatformContext> m_platform;

    // Render context (scene renderer, loaded scene data)
    struct RenderContext;
    std::unique_ptr<RenderContext> m_render;

    // Callbacks
    FrameCallback m_on_frame;
    WorldCallback m_on_world_loaded;
    WorldCallback m_on_world_unloaded;

    // Timing
    double m_time{0};
    double m_last_frame_time{0};
    float m_delta_time{0};
    float m_accumulator{0};  // For fixed timestep
    std::uint64_t m_frame_count{0};
};

} // namespace void_runtime
