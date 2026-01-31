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
#include <void_engine/ecs/world.hpp>

// Package system
#include <void_engine/package/package.hpp>
#include <void_engine/package/asset_bundle_loader.hpp>

// Render components and systems for ECS integration
#include <void_engine/render/components.hpp>
#include <void_engine/render/render_systems.hpp>

// Plugin state for ECS (Phase 4)
#include <void_engine/plugin_api/state.hpp>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
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
    bool initialized{false};
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
    std::unique_ptr<void_ecs::World> ecs_world;  // ECS world for package system
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
    spdlog::info("Starting kernel...");
    spdlog::default_logger()->flush();
    if (auto result = m_kernel->start(); !result) {
        spdlog::error("Kernel start failed: {}", result.error().message());
        spdlog::default_logger()->flush();
        m_state = RuntimeState::Uninitialized;
        return void_core::Error("Failed to start kernel: " + result.error().message());
    }
    spdlog::info("Kernel started successfully");

    m_state = RuntimeState::Ready;
    spdlog::info("Runtime initialization complete");
    spdlog::default_logger()->flush();

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

    // Debug: Check initial state
    spdlog::info("  exit_requested: {}, platform valid: {}, should_quit: {}",
                 m_exit_requested,
                 m_platform->platform != nullptr,
                 m_platform->platform ? m_platform->platform->should_quit() : false);
    spdlog::default_logger()->flush();

    auto last_time = std::chrono::high_resolution_clock::now();
    m_accumulator = 0.0f;

    auto should_continue = [this]() {
        if (m_exit_requested) {
            spdlog::debug("Exiting: exit_requested");
            return false;
        }
        if (m_platform->platform && m_platform->platform->should_quit()) {
            spdlog::debug("Exiting: platform should_quit");
            return false;
        }
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
    spdlog::default_logger()->flush();
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

    // Create the ECS world for the package system
    m_packages->ecs_world = std::make_unique<void_ecs::World>(10000);  // Pre-allocate for 10k entities
    spdlog::info("  [packages] Created ECS world");

    // Connect ComponentSchemaRegistry to the ECS component registry.
    // This allows plugins to register component schemas which allocate ECS component IDs.
    // MUST happen before any packages are loaded since plugins register schemas during load.
    m_packages->schema_registry->set_ecs_registry(&m_packages->ecs_world->component_registry_mut());
    spdlog::info("  [packages] Shared schema registry address: {}",
                 static_cast<void*>(m_packages->schema_registry.get()));

    // Configure LoadContext with ECS world and event bus
    m_packages->load_context->set_ecs_world(m_packages->ecs_world.get());
    m_packages->load_context->set_event_bus(m_event_bus.get());

    // Register package loaders into LoadContext
    // The plugin loader uses the shared schema registry so component schemas
    // are available to the prefab registry for instantiation.
    // The kernel is passed so IPlugin-based plugins can register systems.
    spdlog::info("  [packages] Creating plugin loader with shared schema registry and kernel");
    m_packages->load_context->register_loader(
        void_package::create_plugin_package_loader(m_packages->schema_registry.get(), m_kernel.get()));
    m_packages->load_context->register_loader(void_package::create_widget_package_loader());

    // The layer loader uses the shared layer_applier so layers staged during
    // package loading are visible to WorldComposer for application.
    spdlog::info("  [packages] Creating layer loader with shared layer applier");
    m_packages->load_context->register_loader(
        void_package::create_layer_package_loader(m_packages->layer_applier.get()));

    m_packages->load_context->register_loader(void_package::create_world_package_loader());

    // Create and register asset bundle loader with registries
    auto asset_loader = std::make_unique<void_package::AssetBundleLoader>(
        m_packages->prefab_registry.get(),
        m_packages->definition_registry.get(),
        m_packages->schema_registry.get());
    m_packages->load_context->register_loader(std::move(asset_loader));

    // Configure WorldComposer with all components
    m_packages->composer->set_package_registry(m_packages->registry.get());
    m_packages->composer->set_load_context(m_packages->load_context.get());
    m_packages->composer->set_prefab_registry(m_packages->prefab_registry.get());
    m_packages->composer->set_schema_registry(m_packages->schema_registry.get());
    m_packages->composer->set_definition_registry(m_packages->definition_registry.get());
    m_packages->composer->set_widget_manager(m_packages->widget_manager.get());
    m_packages->composer->set_event_bus(m_event_bus.get());
    m_packages->composer->set_layer_applier(m_packages->layer_applier.get());

    // Connect PrefabRegistry to ComponentSchemaRegistry for JSON->component conversion
    // This enables prefabs to instantiate components using schema-based factories.
    // Critical for hot-loaded packages: when a new plugin registers component schemas,
    // the prefab registry can immediately use them to instantiate entities.
    m_packages->prefab_registry->set_schema_registry(m_packages->schema_registry.get());
    spdlog::info("  [packages] PrefabRegistry configured with schema registry: {}",
                 static_cast<void*>(m_packages->schema_registry.get()));

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

    // Scan additional plugin paths (for built plugins, engine plugins, etc.)
    for (const auto& plugin_path : m_config.plugin_paths) {
        if (std::filesystem::exists(plugin_path)) {
            auto scan_result = m_packages->registry->scan_directory(plugin_path, true);
            if (scan_result) {
                spdlog::info("  [packages] Discovered {} packages in plugin path: {}",
                             *scan_result, plugin_path.string());
            } else {
                spdlog::warn("  [packages] Failed to scan plugin path {}: {}",
                             plugin_path.string(), scan_result.error().message());
            }
        } else {
            spdlog::debug("  [packages] Plugin path does not exist: {}", plugin_path.string());
        }
    }

    // Insert PluginRegistry as ECS resource BEFORE any plugins load
    // This allows plugins to discover each other and track state
    m_packages->ecs_world->insert_resource(void_plugin_api::PluginRegistry{});
    spdlog::info("  [packages] PluginRegistry resource inserted");

    // Register engine core components BEFORE any packages load
    // This ensures Transform, Mesh, Material, Light, Camera, Renderable, Hierarchy
    // are available for prefab instantiation
    register_engine_core_components();

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

    // Initialize RenderContext as ECS resource (must happen before render systems)
    init_render_context();

    // Register engine render systems with kernel
    register_engine_render_systems();

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

    m_kernel->unregister_system(void_kernel::Stage::HotReloadPoll, "engine.RenderAssetsHotReload");
    m_kernel->unregister_system(void_kernel::Stage::Render, "engine.RenderSystem");
    m_kernel->unregister_system(void_kernel::Stage::RenderPrepare, "engine.RenderPrepareSystem");
    m_kernel->unregister_system(void_kernel::Stage::RenderPrepare, "engine.LightSystem");
    m_kernel->unregister_system(void_kernel::Stage::RenderPrepare, "engine.CameraSystem");
    m_kernel->unregister_system(void_kernel::Stage::Update, "engine.TransformSystem");

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
// Engine Core Component Registration (Phase 2)
// =============================================================================

namespace {

/// @brief Register TransformComponent with JSON factory
void register_transform_component(void_package::ComponentSchemaRegistry& registry,
                                   void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Transform";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::TransformComponent);
    schema.alignment = alignof(void_render::TransformComponent);

    // Define fields for documentation/validation
    schema.fields = {
        {.name = "position", .type = void_package::FieldType::Vec3, .default_value = nlohmann::json::array({0.0f, 0.0f, 0.0f})},
        {.name = "rotation", .type = void_package::FieldType::Quat, .default_value = nlohmann::json::array({0.0f, 0.0f, 0.0f, 1.0f})},
        {.name = "scale", .type = void_package::FieldType::Vec3, .default_value = nlohmann::json::array({1.0f, 1.0f, 1.0f})}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::TransformComponent transform;

        if (data.contains("position") && data["position"].is_array() && data["position"].size() >= 3) {
            transform.position = {
                data["position"][0].get<float>(),
                data["position"][1].get<float>(),
                data["position"][2].get<float>()
            };
        }

        if (data.contains("rotation") && data["rotation"].is_array() && data["rotation"].size() >= 4) {
            transform.rotation = {
                data["rotation"][0].get<float>(),
                data["rotation"][1].get<float>(),
                data["rotation"][2].get<float>(),
                data["rotation"][3].get<float>()
            };
        }

        if (data.contains("scale")) {
            if (data["scale"].is_array() && data["scale"].size() >= 3) {
                transform.scale = {
                    data["scale"][0].get<float>(),
                    data["scale"][1].get<float>(),
                    data["scale"][2].get<float>()
                };
            } else if (data["scale"].is_number()) {
                float s = data["scale"].get<float>();
                transform.scale = {s, s, s};
            }
        }

        transform.dirty = true;
        w.add_component(e, transform);
        return void_core::Ok();
    };

    // Register component type with ECS first
    world.register_component<void_render::TransformComponent>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Transform schema: {}", result.error().message());
    }
}

/// @brief Register MeshComponent with JSON factory
void register_mesh_component(void_package::ComponentSchemaRegistry& registry,
                              void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Mesh";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::MeshComponent);
    schema.alignment = alignof(void_render::MeshComponent);

    schema.fields = {
        {.name = "builtin", .type = void_package::FieldType::String, .default_value = ""},
        {.name = "asset", .type = void_package::FieldType::String, .default_value = ""},
        {.name = "submesh_index", .type = void_package::FieldType::UInt32, .default_value = 0u}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::MeshComponent mesh;

        if (data.contains("builtin") && data["builtin"].is_string()) {
            mesh.builtin_mesh = data["builtin"].get<std::string>();
        }

        if (data.contains("submesh_index") && data["submesh_index"].is_number_unsigned()) {
            mesh.submesh_index = data["submesh_index"].get<std::uint32_t>();
        }

        w.add_component(e, mesh);
        return void_core::Ok();
    };

    world.register_component<void_render::MeshComponent>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Mesh schema: {}", result.error().message());
    }
}

/// @brief Register MaterialComponent with JSON factory
void register_material_component(void_package::ComponentSchemaRegistry& registry,
                                  void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Material";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::MaterialComponent);
    schema.alignment = alignof(void_render::MaterialComponent);

    schema.fields = {
        {.name = "albedo", .type = void_package::FieldType::Vec4, .default_value = nlohmann::json::array({0.8f, 0.8f, 0.8f, 1.0f})},
        {.name = "metallic", .type = void_package::FieldType::Float32, .default_value = 0.0f},
        {.name = "roughness", .type = void_package::FieldType::Float32, .default_value = 0.5f},
        {.name = "emissive", .type = void_package::FieldType::Vec3, .default_value = nlohmann::json::array({0.0f, 0.0f, 0.0f})},
        {.name = "emissive_strength", .type = void_package::FieldType::Float32, .default_value = 0.0f},
        {.name = "double_sided", .type = void_package::FieldType::Bool, .default_value = false},
        {.name = "alpha_blend", .type = void_package::FieldType::Bool, .default_value = false}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::MaterialComponent mat;

        if (data.contains("albedo") && data["albedo"].is_array() && data["albedo"].size() >= 3) {
            mat.albedo[0] = data["albedo"][0].get<float>();
            mat.albedo[1] = data["albedo"][1].get<float>();
            mat.albedo[2] = data["albedo"][2].get<float>();
            mat.albedo[3] = data["albedo"].size() >= 4 ? data["albedo"][3].get<float>() : 1.0f;
        }

        if (data.contains("metallic") && data["metallic"].is_number()) {
            mat.metallic_value = data["metallic"].get<float>();
        }

        if (data.contains("roughness") && data["roughness"].is_number()) {
            mat.roughness_value = data["roughness"].get<float>();
        }

        if (data.contains("ao") && data["ao"].is_number()) {
            mat.ao_value = data["ao"].get<float>();
        }

        if (data.contains("emissive") && data["emissive"].is_array() && data["emissive"].size() >= 3) {
            mat.emissive = {
                data["emissive"][0].get<float>(),
                data["emissive"][1].get<float>(),
                data["emissive"][2].get<float>()
            };
        }

        if (data.contains("emissive_strength") && data["emissive_strength"].is_number()) {
            mat.emissive_strength = data["emissive_strength"].get<float>();
        }

        if (data.contains("double_sided") && data["double_sided"].is_boolean()) {
            mat.double_sided = data["double_sided"].get<bool>();
        }

        if (data.contains("alpha_blend") && data["alpha_blend"].is_boolean()) {
            mat.alpha_blend = data["alpha_blend"].get<bool>();
        }

        if (data.contains("alpha_cutoff") && data["alpha_cutoff"].is_number()) {
            mat.alpha_cutoff = data["alpha_cutoff"].get<float>();
        }

        w.add_component(e, mat);
        return void_core::Ok();
    };

    world.register_component<void_render::MaterialComponent>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Material schema: {}", result.error().message());
    }
}

/// @brief Register LightComponent with JSON factory
void register_light_component(void_package::ComponentSchemaRegistry& registry,
                               void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Light";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::LightComponent);
    schema.alignment = alignof(void_render::LightComponent);

    schema.fields = {
        {.name = "type", .type = void_package::FieldType::String, .default_value = "point"},
        {.name = "color", .type = void_package::FieldType::Vec3, .default_value = nlohmann::json::array({1.0f, 1.0f, 1.0f})},
        {.name = "intensity", .type = void_package::FieldType::Float32, .default_value = 1.0f},
        {.name = "range", .type = void_package::FieldType::Float32, .default_value = 10.0f},
        {.name = "inner_cone_angle", .type = void_package::FieldType::Float32, .default_value = 30.0f},
        {.name = "outer_cone_angle", .type = void_package::FieldType::Float32, .default_value = 45.0f},
        {.name = "cast_shadows", .type = void_package::FieldType::Bool, .default_value = false}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::LightComponent light;

        if (data.contains("type") && data["type"].is_string()) {
            std::string type_str = data["type"].get<std::string>();
            if (type_str == "directional") {
                light.type = void_render::LightComponent::Type::Directional;
            } else if (type_str == "spot") {
                light.type = void_render::LightComponent::Type::Spot;
            } else {
                light.type = void_render::LightComponent::Type::Point;
            }
        }

        if (data.contains("color") && data["color"].is_array() && data["color"].size() >= 3) {
            light.color = {
                data["color"][0].get<float>(),
                data["color"][1].get<float>(),
                data["color"][2].get<float>()
            };
        }

        if (data.contains("intensity") && data["intensity"].is_number()) {
            light.intensity = data["intensity"].get<float>();
        }

        if (data.contains("range") && data["range"].is_number()) {
            light.range = data["range"].get<float>();
        }

        if (data.contains("inner_cone_angle") && data["inner_cone_angle"].is_number()) {
            light.inner_cone_angle = data["inner_cone_angle"].get<float>();
        }

        if (data.contains("outer_cone_angle") && data["outer_cone_angle"].is_number()) {
            light.outer_cone_angle = data["outer_cone_angle"].get<float>();
        }

        if (data.contains("cast_shadows") && data["cast_shadows"].is_boolean()) {
            light.cast_shadows = data["cast_shadows"].get<bool>();
        }

        if (data.contains("shadow_resolution") && data["shadow_resolution"].is_number_unsigned()) {
            light.shadow_resolution = data["shadow_resolution"].get<std::uint32_t>();
        }

        w.add_component(e, light);
        return void_core::Ok();
    };

    world.register_component<void_render::LightComponent>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Light schema: {}", result.error().message());
    }
}

/// @brief Register CameraComponent with JSON factory
void register_camera_component(void_package::ComponentSchemaRegistry& registry,
                                void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Camera";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::CameraComponent);
    schema.alignment = alignof(void_render::CameraComponent);

    schema.fields = {
        {.name = "projection", .type = void_package::FieldType::String, .default_value = "perspective"},
        {.name = "fov", .type = void_package::FieldType::Float32, .default_value = 60.0f},
        {.name = "near", .type = void_package::FieldType::Float32, .default_value = 0.1f},
        {.name = "far", .type = void_package::FieldType::Float32, .default_value = 1000.0f},
        {.name = "ortho_size", .type = void_package::FieldType::Float32, .default_value = 10.0f},
        {.name = "active", .type = void_package::FieldType::Bool, .default_value = true},
        {.name = "priority", .type = void_package::FieldType::Int32, .default_value = 0}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::CameraComponent camera;

        if (data.contains("projection") && data["projection"].is_string()) {
            std::string proj_str = data["projection"].get<std::string>();
            if (proj_str == "orthographic") {
                camera.projection = void_render::CameraComponent::Projection::Orthographic;
            } else {
                camera.projection = void_render::CameraComponent::Projection::Perspective;
            }
        }

        if (data.contains("fov") && data["fov"].is_number()) {
            camera.fov = data["fov"].get<float>();
        }

        if (data.contains("near") && data["near"].is_number()) {
            camera.near_plane = data["near"].get<float>();
        }

        if (data.contains("far") && data["far"].is_number()) {
            camera.far_plane = data["far"].get<float>();
        }

        if (data.contains("ortho_size") && data["ortho_size"].is_number()) {
            camera.ortho_size = data["ortho_size"].get<float>();
        }

        if (data.contains("active") && data["active"].is_boolean()) {
            camera.active = data["active"].get<bool>();
        }

        if (data.contains("priority") && data["priority"].is_number_integer()) {
            camera.priority = data["priority"].get<std::int32_t>();
        }

        if (data.contains("render_target") && data["render_target"].is_number_unsigned()) {
            camera.render_target = data["render_target"].get<std::uint32_t>();
        }

        w.add_component(e, camera);
        return void_core::Ok();
    };

    world.register_component<void_render::CameraComponent>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Camera schema: {}", result.error().message());
    }
}

/// @brief Register RenderableTag with JSON factory
void register_renderable_component(void_package::ComponentSchemaRegistry& registry,
                                    void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Renderable";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::RenderableTag);
    schema.alignment = alignof(void_render::RenderableTag);

    schema.fields = {
        {.name = "visible", .type = void_package::FieldType::Bool, .default_value = true},
        {.name = "layer_mask", .type = void_package::FieldType::UInt32, .default_value = 0xFFFFFFFF},
        {.name = "render_order", .type = void_package::FieldType::Int32, .default_value = 0}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::RenderableTag tag;

        if (data.contains("visible") && data["visible"].is_boolean()) {
            tag.visible = data["visible"].get<bool>();
        }

        if (data.contains("layer_mask") && data["layer_mask"].is_number_unsigned()) {
            tag.layer_mask = data["layer_mask"].get<std::uint32_t>();
        }

        if (data.contains("render_order") && data["render_order"].is_number_integer()) {
            tag.render_order = data["render_order"].get<std::int32_t>();
        }

        w.add_component(e, tag);
        return void_core::Ok();
    };

    world.register_component<void_render::RenderableTag>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Renderable schema: {}", result.error().message());
    }
}

/// @brief Register HierarchyComponent with JSON factory
void register_hierarchy_component(void_package::ComponentSchemaRegistry& registry,
                                   void_ecs::World& world) {
    void_package::ComponentSchema schema;
    schema.name = "Hierarchy";
    schema.source_plugin = "engine.core";
    schema.size = sizeof(void_render::HierarchyComponent);
    schema.alignment = alignof(void_render::HierarchyComponent);

    schema.fields = {
        {.name = "parent_id", .type = void_package::FieldType::UInt64, .default_value = 0u},
        {.name = "parent_generation", .type = void_package::FieldType::UInt32, .default_value = 0u}
    };

    void_package::ComponentApplier applier = [](void_ecs::World& w, void_ecs::Entity e,
                                                 const nlohmann::json& data) -> void_core::Result<void> {
        void_render::HierarchyComponent hierarchy;

        if (data.contains("parent_id") && data["parent_id"].is_number_unsigned()) {
            hierarchy.parent_id = data["parent_id"].get<std::uint64_t>();
        }

        if (data.contains("parent_generation") && data["parent_generation"].is_number_unsigned()) {
            hierarchy.parent_generation = data["parent_generation"].get<std::uint32_t>();
        }

        w.add_component(e, hierarchy);
        return void_core::Ok();
    };

    world.register_component<void_render::HierarchyComponent>();

    auto result = registry.register_schema_with_factory(std::move(schema), nullptr, std::move(applier));
    if (!result) {
        spdlog::warn("Failed to register Hierarchy schema: {}", result.error().message());
    }
}

} // anonymous namespace

/// @brief Register all engine core render components with the schema registry
///
/// This is called during init_packages() to ensure engine render components
/// are available before any plugins load. Plugins can use these component
/// names in prefabs and the WorldComposer will instantiate them correctly.
void Runtime::register_engine_core_components() {
    if (!m_packages || !m_packages->schema_registry || !m_packages->ecs_world) {
        spdlog::error("Cannot register engine components: package system not initialized");
        return;
    }

    auto& registry = *m_packages->schema_registry;
    auto& world = *m_packages->ecs_world;

    spdlog::info("  [packages] Registering engine core render components...");

    register_transform_component(registry, world);
    register_mesh_component(registry, world);
    register_material_component(registry, world);
    register_light_component(registry, world);
    register_camera_component(registry, world);
    register_renderable_component(registry, world);
    register_hierarchy_component(registry, world);

    spdlog::info("  [packages] Registered 7 engine core render components");
}

/// @brief Register engine render systems with the kernel
///
/// Called during init_render() to set up the render pipeline.
/// Systems are registered with appropriate stages and priorities:
/// - TransformSystem: Update stage, priority -100 (runs early)
/// - CameraSystem: RenderPrepare stage, priority 0
/// - LightSystem: RenderPrepare stage, priority 10
/// - RenderPrepareSystem: RenderPrepare stage, priority 100
/// - RenderSystem: Render stage, priority 0 (before platform_present)
void Runtime::register_engine_render_systems() {
    if (!m_kernel || !m_packages || !m_packages->ecs_world) {
        spdlog::error("Cannot register render systems: kernel or ECS world not initialized");
        return;
    }

    auto& world = *m_packages->ecs_world;

    spdlog::info("  [render] Registering engine render systems...");

    // TransformSystem - Updates world matrices from local transforms
    m_kernel->register_system(void_kernel::Stage::Update, "engine.TransformSystem",
        [&world](float dt) {
            void_render::TransformSystem::run(world, dt);
        }, -100);  // Early in Update stage

    // CameraSystem - Updates RenderContext with camera data
    m_kernel->register_system(void_kernel::Stage::RenderPrepare, "engine.CameraSystem",
        [&world](float dt) {
            void_render::CameraSystem::run(world, dt);
        }, 0);

    // LightSystem - Collects light data for rendering
    m_kernel->register_system(void_kernel::Stage::RenderPrepare, "engine.LightSystem",
        [&world](float dt) {
            void_render::LightSystem::run(world, dt);
        }, 10);

    // RenderPrepareSystem - Builds render queue from entities
    m_kernel->register_system(void_kernel::Stage::RenderPrepare, "engine.RenderPrepareSystem",
        [&world](float dt) {
            void_render::RenderPrepareSystem::run(world, dt);
        }, 100);

    // RenderSystem - Executes draw calls
    m_kernel->register_system(void_kernel::Stage::Render, "engine.RenderSystem",
        [&world](float dt) {
            void_render::RenderSystem::run(world, dt);
        }, -10);  // Before platform_present (1000)

    // RenderAssetManager hot-reload polling
    m_kernel->register_system(void_kernel::Stage::HotReloadPoll, "engine.RenderAssetsHotReload",
        [&world](float) {
            auto* render_ctx = world.resource<void_render::RenderContext>();
            if (render_ctx) {
                render_ctx->assets().poll_hot_reload();
            }
        }, 0);

    spdlog::info("  [render] Registered 6 engine render systems");
}

/// @brief Add RenderContext as ECS resource
///
/// RenderContext must be available as an ECS resource before render systems run.
/// This is called during init_render() after window dimensions are known.
void Runtime::init_render_context() {
    if (!m_packages || !m_packages->ecs_world) {
        spdlog::error("Cannot init RenderContext: ECS world not available");
        return;
    }

    // Get window dimensions
    std::uint32_t width = m_config.window_width;
    std::uint32_t height = m_config.window_height;

    if (m_platform && m_platform->platform) {
        m_platform->platform->get_window_size(width, height);
    }

    // Create and initialize RenderContext
    void_render::RenderContext render_ctx;
    auto init_result = render_ctx.initialize(width, height);
    if (!init_result) {
        spdlog::error("Failed to initialize RenderContext: {}", init_result.error().message());
        return;
    }

    // Insert as ECS resource (move semantics)
    m_packages->ecs_world->insert_resource(std::move(render_ctx));

    spdlog::info("  [render] RenderContext added as ECS resource ({}x{})", width, height);
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
