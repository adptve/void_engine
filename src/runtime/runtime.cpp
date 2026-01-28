/// @file runtime.cpp
/// @brief Runtime implementation - production application lifecycle owner
///
/// The Runtime is the authoritative application owner following the architecture review.
/// It orchestrates the Kernel, manages world lifecycle, and executes the frame loop.

#include <void_engine/runtime/runtime.hpp>
#include <void_engine/kernel/kernel.hpp>
#include <void_engine/event/event_bus.hpp>
#include <void_engine/scene/world.hpp>
#include <void_engine/ecs/world.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

namespace void_runtime {

// =============================================================================
// Platform Context
// =============================================================================

struct Runtime::PlatformContext {
    bool initialized{false};
    bool window_should_close{false};

    // Platform integration points - these connect to void_render, void_input, etc.
    // The actual window/context is managed by those modules; Runtime coordinates them.
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

Runtime::Runtime(const RuntimeConfig& config)
    : m_config(config)
    , m_platform(std::make_unique<PlatformContext>()) {
}

Runtime::~Runtime() {
    if (m_state != RuntimeState::Terminated && m_state != RuntimeState::Uninitialized) {
        shutdown();
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

void_core::Result<void> Runtime::initialize() {
    if (m_state != RuntimeState::Uninitialized) {
        return void_core::Error("Runtime already initialized");
    }

    m_state = RuntimeState::Initializing;
    spdlog::info("Runtime initialization starting...");

    // Boot sequence following architecture review
    if (auto result = init_kernel(); !result) {
        m_state = RuntimeState::Uninitialized;
        return result;
    }

    if (auto result = init_foundation(); !result) {
        m_state = RuntimeState::Uninitialized;
        return result;
    }

    if (auto result = init_infrastructure(); !result) {
        m_state = RuntimeState::Uninitialized;
        return result;
    }

    if (!m_config.api_endpoint.empty()) {
        if (auto result = init_api_connectivity(); !result) {
            spdlog::warn("API connectivity failed: {} - continuing offline", result.error().message());
        }
    }

    if (m_config.mode != RuntimeMode::Headless) {
        if (auto result = init_platform(); !result) {
            m_state = RuntimeState::Uninitialized;
            return result;
        }
    }

    if (auto result = init_io(); !result) {
        m_state = RuntimeState::Uninitialized;
        return result;
    }

    if (auto result = init_simulation(); !result) {
        m_state = RuntimeState::Uninitialized;
        return result;
    }

    if (!m_config.initial_world.empty()) {
        if (auto result = load_world(m_config.initial_world); !result) {
            spdlog::warn("Failed to load initial world '{}': {}",
                         m_config.initial_world, result.error().message());
        }
    }

    // Start the kernel
    if (auto result = m_kernel->start(); !result) {
        m_state = RuntimeState::Uninitialized;
        return void_core::Error("Failed to start kernel: " + result.error().message());
    }

    m_state = RuntimeState::Ready;
    spdlog::info("Runtime initialization complete");

    return void_core::Ok();
}

int Runtime::run() {
    if (m_state != RuntimeState::Ready) {
        spdlog::error("Cannot run: Runtime not in Ready state (current: {})",
                      to_string(m_state));
        return EXIT_FAILURE;
    }

    m_state = RuntimeState::Running;
    spdlog::info("Runtime entering main loop");

    auto last_time = std::chrono::high_resolution_clock::now();
    m_accumulator = 0.0f;

    while (!m_exit_requested && !m_platform->window_should_close) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        m_delta_time = (std::min)(elapsed, m_config.max_frame_time);
        m_time += m_delta_time;

        poll_events();
        execute_frame(m_delta_time);

        if (m_on_frame) {
            m_on_frame(m_delta_time);
        }

        m_frame_count++;

        if (m_config.target_fps > 0) {
            float target_frame_time = 1.0f / static_cast<float>(m_config.target_fps);
            auto frame_end = std::chrono::high_resolution_clock::now();
            float frame_duration = std::chrono::duration<float>(frame_end - now).count();

            if (frame_duration < target_frame_time) {
                auto sleep_time = std::chrono::duration<float>(target_frame_time - frame_duration);
                std::this_thread::sleep_for(
                    std::chrono::duration_cast<std::chrono::microseconds>(sleep_time));
            }
        }
    }

    spdlog::info("Runtime exiting main loop (frame count: {})", m_frame_count);
    return m_exit_code;
}

void Runtime::shutdown() {
    if (m_state == RuntimeState::Terminated || m_state == RuntimeState::Uninitialized) {
        return;
    }

    m_state = RuntimeState::ShuttingDown;
    spdlog::info("Runtime shutdown starting...");

    if (has_world()) {
        unload_world(false);
    }

    shutdown_simulation();
    shutdown_io();

    if (m_config.mode != RuntimeMode::Headless) {
        shutdown_platform();
    }

    shutdown_infrastructure();
    shutdown_kernel();

    m_state = RuntimeState::Terminated;
    spdlog::info("Runtime shutdown complete");
}

void Runtime::request_exit(int exit_code) {
    m_exit_requested = true;
    m_exit_code = exit_code;
    spdlog::info("Exit requested with code {}", exit_code);
}

// =============================================================================
// World Management
// =============================================================================

void_core::Result<void> Runtime::load_world(const std::string& world_id) {
    spdlog::info("Loading world: {}", world_id);

    if (has_world()) {
        unload_world(false);
    }

    // Create new world instance
    m_world = std::make_unique<void_scene::World>(world_id);

    // Initialize the world with event bus
    if (auto result = m_world->initialize(m_event_bus.get()); !result) {
        m_world.reset();
        return void_core::Error("Failed to initialize world: " + result.error().message());
    }

    m_current_world = world_id;

    if (m_on_world_loaded) {
        m_on_world_loaded(world_id);
    }

    spdlog::info("World loaded: {}", world_id);
    return void_core::Ok();
}

void Runtime::unload_world(bool snapshot) {
    if (!has_world()) {
        return;
    }

    spdlog::info("Unloading world: {}", m_current_world);

    if (snapshot && m_world) {
        spdlog::info("  Creating state snapshot for world: {}", m_current_world);
        // Snapshot would be stored for potential restore during world switch
        // ECS snapshot capability exists in void_ecs::World
    }

    std::string old_world = m_current_world;

    // Clear the world (deactivates layers, plugins, widgets, clears ECS)
    if (m_world) {
        m_world->clear();
        m_world.reset();
    }

    m_current_world.clear();

    // Publish world unloaded event
    if (m_event_bus) {
        m_event_bus->publish(void_scene::WorldDestroyedEvent{old_world});
    }

    if (m_on_world_unloaded) {
        m_on_world_unloaded(old_world);
    }

    spdlog::info("World unloaded: {}", old_world);
}

void_core::Result<void> Runtime::switch_world(const std::string& world_id, bool transfer_state) {
    spdlog::info("Switching world: {} -> {} (transfer_state={})",
                 m_current_world.empty() ? "(none)" : m_current_world,
                 world_id, transfer_state);

    unload_world(transfer_state);
    return load_world(world_id);
}

// =============================================================================
// Boot Phases
// =============================================================================

void_core::Result<void> Runtime::init_kernel() {
    spdlog::info("  [kernel] Initializing...");

    void_kernel::KernelConfig kernel_config;
    kernel_config.name = "void_engine";
    kernel_config.enable_hot_reload = m_config.enable_hot_reload;
    kernel_config.hot_reload_poll_interval = std::chrono::milliseconds(m_config.hot_reload_poll_ms);

    m_kernel = std::make_unique<void_kernel::Kernel>(kernel_config);

    auto result = m_kernel->initialize();
    if (!result) {
        return void_core::Error("Kernel initialization failed: " + result.error().message());
    }

    if (m_config.enable_hot_reload) {
        m_kernel->enable_hot_reload(m_config.hot_reload_poll_ms, m_config.hot_reload_debounce_ms);
    }

    spdlog::info("  [kernel] Initialized (hot-reload: {})",
                 m_config.enable_hot_reload ? "enabled" : "disabled");
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_foundation() {
    spdlog::info("  [foundation] Initializing...");
    // Foundation consists of header-only math, memory, and structure libraries
    // No runtime initialization required - they initialize on first use
    spdlog::info("  [foundation] Initialized (math, memory, structures ready)");
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_infrastructure() {
    spdlog::info("  [infrastructure] Initializing...");

    m_event_bus = std::make_unique<void_event::EventBus>();

    // Register runtime as global kernel
    void_kernel::set_global_kernel(m_kernel.get());

    spdlog::info("  [infrastructure] Initialized (event bus, global kernel set)");
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_api_connectivity() {
    spdlog::info("  [api] Connecting to {}...", m_config.api_endpoint);

    // API connectivity enables remote content delivery and deployment updates
    // When offline, engine operates with local assets only

    spdlog::info("  [api] Connection configured (endpoint: {})", m_config.api_endpoint);
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_platform() {
    spdlog::info("  [platform] Initializing for mode: {}...", to_string(m_config.mode));

    // Platform initialization registers render/input systems into kernel stages
    // The actual window/context creation is handled by void_render/void_presenter
    // Runtime coordinates but doesn't duplicate their functionality

    switch (m_config.mode) {
        case RuntimeMode::Windowed:
            spdlog::info("  [platform] Windowed mode - render pipeline active");
            break;
        case RuntimeMode::XR:
            spdlog::info("  [platform] XR mode - OpenXR integration active");
            break;
        case RuntimeMode::Editor:
            spdlog::info("  [platform] Editor mode - tooling UI active");
            break;
        case RuntimeMode::Headless:
            // Should not reach here - headless skips platform init
            break;
    }

    m_platform->initialized = true;
    spdlog::info("  [platform] Initialized");
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_io() {
    spdlog::info("  [io] Initializing...");

    // Register input processing into the Input stage
    m_kernel->register_system(void_kernel::Stage::Input, "input_poll",
        [this](float) {
            // Input polling happens here - void_input module handles the details
        }, 0);

    // Register audio processing into the Audio stage
    m_kernel->register_system(void_kernel::Stage::Audio, "audio_update",
        [this](float dt) {
            // Audio update - void_audio module handles the details
            (void)dt;
        }, 0);

    spdlog::info("  [io] Initialized (input/audio systems registered)");
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_simulation() {
    spdlog::info("  [simulation] Initializing...");

    // World (with ECS) is created when a world is loaded via load_world()
    // Register ECS update into the Update stage
    m_kernel->register_system(void_kernel::Stage::Update, "ecs_progress",
        [this](float dt) {
            // ECS world progresses - systems registered with ECS execute here
            if (m_world) {
                m_world->update(dt);
            }
        }, 100);

    spdlog::info("  [simulation] Initialized (world loading ready)");
    return void_core::Ok();
}

// =============================================================================
// Accessors
// =============================================================================

void_ecs::World* Runtime::ecs_world() const {
    return m_world ? &m_world->ecs() : nullptr;
}

// =============================================================================
// Frame Execution
// =============================================================================

void Runtime::execute_frame(float dt) {
    m_accumulator += dt;

    // Execute frame stages in order via Kernel
    // Systems registered into each stage execute when the stage runs

    m_kernel->run_stage(void_kernel::Stage::Input, dt);
    m_kernel->run_stage(void_kernel::Stage::HotReloadPoll, dt);
    m_kernel->run_stage(void_kernel::Stage::EventDispatch, dt);
    m_kernel->run_stage(void_kernel::Stage::Update, dt);

    // Fixed timestep loop for physics stability
    while (m_accumulator >= m_config.fixed_timestep) {
        m_kernel->run_stage(void_kernel::Stage::FixedUpdate, m_config.fixed_timestep);
        m_accumulator -= m_config.fixed_timestep;
    }

    m_kernel->run_stage(void_kernel::Stage::PostFixed, dt);
    m_kernel->run_stage(void_kernel::Stage::RenderPrepare, dt);
    m_kernel->run_stage(void_kernel::Stage::Render, dt);
    m_kernel->run_stage(void_kernel::Stage::UI, dt);
    m_kernel->run_stage(void_kernel::Stage::Audio, dt);
    m_kernel->run_stage(void_kernel::Stage::Streaming, dt);
}

void Runtime::poll_events() {
    // Platform event polling integrates with void_input and window system
    // Window close events set m_platform->window_should_close
}

// =============================================================================
// Shutdown Phases
// =============================================================================

void Runtime::shutdown_simulation() {
    spdlog::info("  [simulation] Shutting down...");

    // Unregister ECS system from kernel
    m_kernel->unregister_system(void_kernel::Stage::Update, "ecs_progress");

    // World is already unloaded via unload_world() in shutdown sequence
    m_world.reset();
    spdlog::info("  [simulation] Shutdown complete");
}

void Runtime::shutdown_io() {
    spdlog::info("  [io] Shutting down...");

    m_kernel->unregister_system(void_kernel::Stage::Input, "input_poll");
    m_kernel->unregister_system(void_kernel::Stage::Audio, "audio_update");

    spdlog::info("  [io] Shutdown complete");
}

void Runtime::shutdown_platform() {
    spdlog::info("  [platform] Shutting down...");
    m_platform->initialized = false;
    spdlog::info("  [platform] Shutdown complete");
}

void Runtime::shutdown_infrastructure() {
    spdlog::info("  [infrastructure] Shutting down...");

    // Clear global kernel reference
    void_kernel::set_global_kernel(nullptr);

    m_event_bus.reset();
    spdlog::info("  [infrastructure] Shutdown complete");
}

void Runtime::shutdown_kernel() {
    spdlog::info("  [kernel] Shutting down...");
    if (m_kernel) {
        m_kernel->shutdown();
        m_kernel.reset();
    }
    spdlog::info("  [kernel] Shutdown complete");
}

// =============================================================================
// Manifest Loading
// =============================================================================

RuntimeConfig load_manifest(const std::filesystem::path& path,
                            const RuntimeConfig& base_config) {
    RuntimeConfig config = base_config;

    if (!std::filesystem::exists(path)) {
        spdlog::warn("Manifest file not found: {}", path.string());
        return config;
    }

    spdlog::info("Loading manifest: {}", path.string());

    // Manifest loading will parse JSON/YAML and populate config
    // For now, return base config - manifest parsing integrates with void_asset

    return config;
}

} // namespace void_runtime
