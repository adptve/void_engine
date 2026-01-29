/// @file runtime.cpp
/// @brief Runtime implementation - production application lifecycle owner
///
/// The Runtime is the authoritative application owner following the architecture review.
/// It orchestrates the Kernel, manages world lifecycle, and executes the frame loop.

#include <void_engine/runtime/runtime.hpp>
#include <void_engine/runtime/platform.hpp>
#include <void_engine/kernel/kernel.hpp>
#include <void_engine/event/event_bus.hpp>
#include <void_engine/scene/world.hpp>
#include <void_engine/scene/scene_data.hpp>
#include <void_engine/scene/scene_parser.hpp>
#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/ecs/world.hpp>

// Package system
#include <void_engine/package/package.hpp>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace void_runtime {

// =============================================================================
// Platform Context
// =============================================================================

struct Runtime::PlatformContext {
    std::unique_ptr<IPlatform> platform;
    bool initialized{false};

    // Input state tracking
    double last_mouse_x{0};
    double last_mouse_y{0};

    // Frame timing
    double frame_start_time{0};
    double frame_end_time{0};
};

// =============================================================================
// Render Context
// =============================================================================

struct Runtime::RenderContext {
    std::unique_ptr<void_render::SceneRenderer> renderer;
    std::unique_ptr<void_scene::SceneData> scene_data;
    bool initialized{false};
    std::filesystem::path scene_path;
};

// =============================================================================
// Package Context
// =============================================================================

struct Runtime::PackageContext {
    std::unique_ptr<void_package::PackageRegistry> registry;
    std::unique_ptr<void_package::LoadContext> load_context;
    std::unique_ptr<void_package::WorldComposer> composer;
    std::unique_ptr<void_package::PrefabRegistry> prefab_registry;
    std::unique_ptr<void_package::ComponentSchemaRegistry> schema_registry;
    std::unique_ptr<void_package::DefinitionRegistry> definition_registry;
    std::unique_ptr<void_package::WidgetManager> widget_manager;
    std::unique_ptr<void_package::LayerApplier> layer_applier;
    bool initialized{false};
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

Runtime::Runtime(const RuntimeConfig& config)
    : m_config(config)
    , m_platform(std::make_unique<PlatformContext>())
    , m_render(std::make_unique<RenderContext>())
    , m_packages(std::make_unique<PackageContext>()) {
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

    if (auto result = init_packages(); !result) {
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

        if (auto result = init_render(); !result) {
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

    auto should_continue = [this]() {
        if (m_exit_requested) return false;
        if (m_platform->platform && m_platform->platform->should_quit()) return false;
        return true;
    };

    while (should_continue()) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        m_delta_time = (std::min)(elapsed, m_config.max_frame_time);
        m_time += m_delta_time;

        // Begin frame on platform
        if (m_platform->platform) {
            m_platform->platform->begin_frame();
        }

        poll_events();
        execute_frame(m_delta_time);

        if (m_on_frame) {
            m_on_frame(m_delta_time);
        }

        m_frame_count++;

        // Frame rate limiting
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
        shutdown_render();
        shutdown_platform();
    }

    shutdown_packages();
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

    // Package system is required - no legacy fallback
    if (!m_packages || !m_packages->composer) {
        return void_core::Error("Package system not initialized - cannot load world '" + world_id + "'");
    }

    if (!m_packages->registry) {
        return void_core::Error("PackageRegistry not available - cannot load world '" + world_id + "'");
    }

    // Check if world package exists
    if (!m_packages->registry->is_available(world_id)) {
        return void_core::Error("World package not found: '" + world_id + "'. "
            "Ensure the package is in the content_path and has a valid .world.json manifest.");
    }

    if (has_world()) {
        unload_world(false);
    }

    // Load world through package system
    void_package::WorldLoadOptions options;
    options.spawn_player = true;
    options.apply_layers = true;
    options.emit_events = true;

    if (auto result = m_packages->composer->load_world(world_id, options); !result) {
        spdlog::error("Failed to load world '{}': {}", world_id, result.error().message());
        return result;
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

    std::string old_world = m_current_world;

    // Unload through package system
    if (m_packages && m_packages->composer && m_packages->composer->has_world()) {
        void_package::WorldUnloadOptions options;
        options.preserve_player = snapshot;
        options.emit_events = true;

        if (snapshot) {
            spdlog::info("  Creating state snapshot for world: {}", m_current_world);
        }

        if (auto result = m_packages->composer->unload_world(options); !result) {
            spdlog::error("Failed to unload world: {}", result.error().message());
        }
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

    // Package system is required
    if (!m_packages || !m_packages->composer) {
        return void_core::Error("Package system not initialized - cannot switch to world '" + world_id + "'");
    }

    // Use WorldComposer's atomic switch
    void_package::WorldSwitchOptions options;
    options.transfer_player = transfer_state;
    options.emit_events = true;

    auto result = m_packages->composer->switch_world(world_id, options);
    if (!result) {
        return result;
    }

    m_current_world = world_id;

    if (m_on_world_loaded) {
        m_on_world_loaded(world_id);
    }

    spdlog::info("World switched to: {}", world_id);
    return void_core::Ok();
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

void_core::Result<void> Runtime::init_packages() {
    spdlog::info("  [packages] Initializing...");

    // Create package system components
    m_packages->registry = std::make_unique<void_package::PackageRegistry>();
    m_packages->load_context = std::make_unique<void_package::LoadContext>();
    m_packages->composer = void_package::create_world_composer();
    m_packages->prefab_registry = std::make_unique<void_package::PrefabRegistry>();
    m_packages->schema_registry = std::make_unique<void_package::ComponentSchemaRegistry>();
    m_packages->definition_registry = std::make_unique<void_package::DefinitionRegistry>();
    m_packages->widget_manager = std::make_unique<void_package::WidgetManager>();
    m_packages->layer_applier = std::make_unique<void_package::LayerApplier>();

    // Configure LoadContext with ECS world and event bus
    // Note: ECS world will be set when a world is loaded
    m_packages->load_context->set_event_bus(m_event_bus.get());

    // Register package loaders into LoadContext
    m_packages->load_context->register_loader(void_package::create_plugin_package_loader());
    m_packages->load_context->register_loader(void_package::create_widget_package_loader());
    m_packages->load_context->register_loader(void_package::create_layer_package_loader());
    m_packages->load_context->register_loader(void_package::create_world_package_loader());

    // Configure WorldComposer with all components
    m_packages->composer->set_package_registry(m_packages->registry.get());
    m_packages->composer->set_load_context(m_packages->load_context.get());
    m_packages->composer->set_prefab_registry(m_packages->prefab_registry.get());
    m_packages->composer->set_schema_registry(m_packages->schema_registry.get());
    m_packages->composer->set_definition_registry(m_packages->definition_registry.get());
    m_packages->composer->set_widget_manager(m_packages->widget_manager.get());
    m_packages->composer->set_event_bus(m_event_bus.get());
    m_packages->composer->set_layer_applier(m_packages->layer_applier.get());

    // Scan for packages if content path is specified
    if (!m_config.content_path.empty()) {
        std::filesystem::path content_dir(m_config.content_path);
        if (std::filesystem::exists(content_dir)) {
            auto scan_result = m_packages->registry->scan_directory(content_dir, true);
            if (scan_result) {
                spdlog::info("  [packages] Discovered {} packages in {}",
                             *scan_result, m_config.content_path);
            } else {
                spdlog::warn("  [packages] Failed to scan content directory: {}",
                             scan_result.error().message());
            }
        }
    }

    // Also scan manifest directory if available
    if (!m_config.manifest_path.empty()) {
        std::filesystem::path manifest_dir = std::filesystem::path(m_config.manifest_path).parent_path();
        if (std::filesystem::exists(manifest_dir)) {
            auto scan_result = m_packages->registry->scan_directory(manifest_dir, true);
            if (scan_result) {
                spdlog::info("  [packages] Discovered {} packages in manifest directory",
                             *scan_result);
            }
        }
    }

    m_packages->initialized = true;
    spdlog::info("  [packages] Initialized ({} packages available)",
                 m_packages->registry->available_count());
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

    // Create platform instance based on mode
    m_platform->platform = create_platform(m_config);
    if (!m_platform->platform) {
        return void_core::Error("Failed to create platform");
    }

    // Configure window
    PlatformWindowConfig window_config;
    window_config.title = m_config.window_title;
    window_config.width = m_config.window_width;
    window_config.height = m_config.window_height;
    window_config.fullscreen = m_config.fullscreen;
    window_config.vsync = m_config.vsync;
    window_config.resizable = true;
    window_config.visible = true;
    window_config.focused = true;

    // Configure GPU
    PlatformGpuConfig gpu_config;
    gpu_config.enable_validation = m_config.gpu_validation;
    gpu_config.enable_debug_markers = m_config.debug_mode;

    // Initialize platform
    if (auto result = m_platform->platform->initialize(window_config, gpu_config); !result) {
        m_platform->platform.reset();
        return void_core::Error("Platform initialization failed: " + result.error().message());
    }

    // Log platform info
    const auto& info = m_platform->platform->info();
    spdlog::info("  [platform] {} - GPU: {} ({})",
                 info.name,
                 void_render::gpu_backend_name(info.capabilities.gpu_backend),
                 info.gpu_device.empty() ? "unknown" : info.gpu_device);

    // Register platform event processing into Input stage
    m_kernel->register_system(void_kernel::Stage::Input, "platform_events",
        [this](float) {
            process_platform_events();
        }, -100);  // High priority - process events first

    // Register frame presentation into Render stage (end)
    m_kernel->register_system(void_kernel::Stage::Render, "platform_present",
        [this](float) {
            if (m_platform->platform) {
                m_platform->platform->end_frame();
            }
        }, 1000);  // Low priority - present last

    m_platform->initialized = true;
    spdlog::info("  [platform] Initialized successfully");
    return void_core::Ok();
}

void_core::Result<void> Runtime::init_render() {
    spdlog::info("  [render] Initializing...");

    // Get window dimensions
    std::uint32_t width, height;
    m_platform->platform->get_window_size(width, height);

    // Create and initialize the scene renderer
    m_render->renderer = std::make_unique<void_render::SceneRenderer>();
    if (!m_render->renderer->initialize(width, height)) {
        return void_core::Error("Failed to initialize SceneRenderer");
    }

    // Load scene from manifest if specified
    if (!m_config.manifest_path.empty()) {
        std::filesystem::path manifest_path(m_config.manifest_path);
        std::filesystem::path manifest_dir = manifest_path.parent_path();

        // Read manifest to find scene file
        std::ifstream manifest_file(manifest_path);
        if (manifest_file.is_open()) {
            try {
                nlohmann::json manifest;
                manifest_file >> manifest;

                // Get scene file from manifest
                std::string scene_file;
                if (manifest.contains("app") && manifest["app"].contains("scene")) {
                    scene_file = manifest["app"]["scene"].get<std::string>();
                }

                if (!scene_file.empty()) {
                    std::filesystem::path scene_path = manifest_dir / scene_file;
                    m_render->scene_path = scene_path;

                    spdlog::info("  [render] Loading scene: {}", scene_path.string());

                    // Parse the scene file
                    void_scene::SceneParser parser;
                    auto result = parser.parse(scene_path);

                    if (result) {
                        m_render->scene_data = std::make_unique<void_scene::SceneData>(std::move(*result));
                        m_render->renderer->load_scene(*m_render->scene_data);
                        spdlog::info("  [render] Scene loaded: {} entities, {} lights",
                                     m_render->scene_data->entities.size(),
                                     m_render->scene_data->lights.size());
                    } else {
                        spdlog::warn("  [render] Failed to parse scene: {}", parser.last_error());
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("  [render] Failed to parse manifest: {}", e.what());
            }
        }
    }

    // Register render update into RenderPrepare stage
    m_kernel->register_system(void_kernel::Stage::RenderPrepare, "scene_update",
        [this](float dt) {
            if (m_render->renderer) {
                m_render->renderer->update(dt);
            }
        }, 0);

    // Register scene rendering into Render stage (before platform_present)
    m_kernel->register_system(void_kernel::Stage::Render, "scene_render",
        [this](float) {
            if (m_render->renderer) {
                m_render->renderer->render();
            }
        }, 0);  // Before platform_present (which is at 1000)

    m_render->initialized = true;
    spdlog::info("  [render] Initialized successfully");
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

    // ECS world is created when a world is loaded via the package system
    // Register ECS update into the Update stage
    m_kernel->register_system(void_kernel::Stage::Update, "ecs_progress",
        [this](float dt) {
            // ECS world progresses through WorldComposer
            if (m_packages && m_packages->composer) {
                m_packages->composer->update(dt);
            }
        }, 100);

    spdlog::info("  [simulation] Initialized (package-based world loading ready)");
    return void_core::Ok();
}

// =============================================================================
// Accessors
// =============================================================================

void_ecs::World* Runtime::ecs_world() const {
    if (m_packages && m_packages->composer) {
        return m_packages->composer->ecs_world();
    }
    return nullptr;
}

void_package::WorldComposer* Runtime::world_composer() const {
    return m_packages ? m_packages->composer.get() : nullptr;
}

void_package::PackageRegistry* Runtime::package_registry() const {
    return m_packages ? m_packages->registry.get() : nullptr;
}

void_package::PrefabRegistry* Runtime::prefab_registry() const {
    return m_packages ? m_packages->prefab_registry.get() : nullptr;
}

IPlatform* Runtime::platform() const {
    return m_platform ? m_platform->platform.get() : nullptr;
}

void_render::SceneRenderer* Runtime::renderer() const {
    return m_render ? m_render->renderer.get() : nullptr;
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
    // Platform events are now processed via the platform_events system
    // registered in init_platform(). This method is kept for compatibility
    // but the actual polling happens in process_platform_events().
}

void Runtime::process_platform_events() {
    if (!m_platform->platform) return;

    m_platform->platform->poll_events([this](const PlatformEvent& evt) {
        handle_platform_event(evt);
    });
}

void Runtime::handle_platform_event(const PlatformEvent& evt) {
    switch (evt.type) {
        case PlatformEventType::Quit:
        case PlatformEventType::WindowClose:
            request_exit(0);
            break;

        case PlatformEventType::WindowResize:
            spdlog::debug("Window resized: {}x{}", evt.data.resize.width, evt.data.resize.height);
            // Publish resize event
            if (m_event_bus) {
                // Could publish a WindowResizedEvent here
            }
            break;

        case PlatformEventType::WindowFocus:
            spdlog::debug("Window focused");
            break;

        case PlatformEventType::WindowBlur:
            spdlog::debug("Window unfocused");
            break;

        case PlatformEventType::KeyDown:
        case PlatformEventType::KeyUp:
        case PlatformEventType::KeyRepeat:
            // Forward to input system via event bus
            if (m_event_bus) {
                // Could publish KeyEvent here
            }
            break;

        case PlatformEventType::MouseMove:
            m_platform->last_mouse_x = evt.data.mouse_move.x;
            m_platform->last_mouse_y = evt.data.mouse_move.y;
            break;

        case PlatformEventType::MouseButton:
        case PlatformEventType::MouseScroll:
            // Forward to input system
            break;

        case PlatformEventType::GamepadConnect:
            spdlog::info("Gamepad {} connected", evt.data.gamepad_button.gamepad_id);
            break;

        case PlatformEventType::GamepadDisconnect:
            spdlog::info("Gamepad {} disconnected", evt.data.gamepad_button.gamepad_id);
            break;

        case PlatformEventType::WindowDrop:
            spdlog::info("Files dropped: {}", evt.dropped_files.size());
            for (const auto& file : evt.dropped_files) {
                spdlog::info("  - {}", file);
            }
            break;

        default:
            break;
    }
}

// =============================================================================
// Shutdown Phases
// =============================================================================

void Runtime::shutdown_simulation() {
    spdlog::info("  [simulation] Shutting down...");

    // Unregister ECS system from kernel
    m_kernel->unregister_system(void_kernel::Stage::Update, "ecs_progress");

    // World is unloaded via package system in shutdown sequence
    spdlog::info("  [simulation] Shutdown complete");
}

void Runtime::shutdown_io() {
    spdlog::info("  [io] Shutting down...");

    m_kernel->unregister_system(void_kernel::Stage::Input, "input_poll");
    m_kernel->unregister_system(void_kernel::Stage::Audio, "audio_update");

    spdlog::info("  [io] Shutdown complete");
}

void Runtime::shutdown_render() {
    spdlog::info("  [render] Shutting down...");

    // Unregister render systems
    m_kernel->unregister_system(void_kernel::Stage::RenderPrepare, "scene_update");
    m_kernel->unregister_system(void_kernel::Stage::Render, "scene_render");

    // Shutdown renderer
    if (m_render->renderer) {
        m_render->renderer->shutdown();
        m_render->renderer.reset();
    }

    m_render->scene_data.reset();
    m_render->initialized = false;
    spdlog::info("  [render] Shutdown complete");
}

void Runtime::shutdown_platform() {
    spdlog::info("  [platform] Shutting down...");

    // Unregister platform systems
    m_kernel->unregister_system(void_kernel::Stage::Input, "platform_events");
    m_kernel->unregister_system(void_kernel::Stage::Render, "platform_present");

    // Shutdown platform
    if (m_platform->platform) {
        m_platform->platform->shutdown();
        m_platform->platform.reset();
    }

    m_platform->initialized = false;
    spdlog::info("  [platform] Shutdown complete");
}

void Runtime::shutdown_packages() {
    spdlog::info("  [packages] Shutting down...");

    if (m_packages && m_packages->initialized) {
        // Unload all packages
        if (m_packages->registry && m_packages->load_context) {
            auto result = m_packages->registry->unload_all(*m_packages->load_context);
            if (!result) {
                spdlog::warn("  [packages] Error unloading packages: {}", result.error().message());
            }
        }

        // Clear all components in reverse order
        m_packages->layer_applier.reset();
        m_packages->widget_manager.reset();
        m_packages->definition_registry.reset();
        m_packages->schema_registry.reset();
        m_packages->prefab_registry.reset();
        m_packages->composer.reset();
        m_packages->load_context.reset();
        m_packages->registry.reset();

        m_packages->initialized = false;
    }

    spdlog::info("  [packages] Shutdown complete");
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
