/// @file main.cpp
/// @brief void_runtime entry point - loads and runs void_engine projects
///
/// This is the main runtime that loads manifest.toml, parses scene.toml,
/// and renders using the SceneRenderer with full ECS, asset, physics, services, presenter, and hot-reload support.
///
/// Architecture:
/// - ServiceRegistry: Manages engine service lifecycles with health monitoring
/// - EventBus: Inter-system communication via publish/subscribe
/// - FrameTiming: Frame pacing, delta time tracking, and performance statistics
/// - ECS World: Authoritative source of scene entities
/// - AssetServer: Loads textures, models, shaders with 3-tier cache
/// - PhysicsWorld: Simulates rigidbody dynamics, collision detection, raycasting
/// - LiveSceneManager: Loads scenes into ECS with hot-reload
/// - SceneRenderer: Renders entities (synced from ECS via callbacks)
/// - AnimationSystem: Updates ECS entity transforms each frame

#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/scene/scene_parser.hpp>
#include <void_engine/scene/scene_data.hpp>
#include <void_engine/scene/scene_instantiator.hpp>
#include <void_engine/ecs/world.hpp>
#include <void_engine/asset/server.hpp>
#include <void_engine/asset/loaders/texture_loader.hpp>
#include <void_engine/asset/loaders/model_loader.hpp>
#include <void_engine/physics/physics.hpp>
#include <void_engine/services/services.hpp>
#include <void_engine/presenter/timing.hpp>
#include <void_engine/presenter/frame.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/compositor/compositor_module.hpp>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

// =============================================================================
// Input State
// =============================================================================

struct InputState {
    bool left_mouse_down = false;
    bool right_mouse_down = false;
    bool middle_mouse_down = false;
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;
};

static InputState g_input;
static void_render::SceneRenderer* g_renderer = nullptr;

// =============================================================================
// GLFW Callbacks
// =============================================================================

static void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    if (g_renderer) {
        g_renderer->on_resize(width, height);
    }
}

static void mouse_button_callback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_input.left_mouse_down = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_input.right_mouse_down = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        g_input.middle_mouse_down = (action == GLFW_PRESS);
    }
}

static void cursor_position_callback(GLFWwindow*, double xpos, double ypos) {
    double dx = xpos - g_input.last_mouse_x;
    double dy = ypos - g_input.last_mouse_y;

    if (g_renderer) {
        if (g_input.left_mouse_down) {
            g_renderer->camera().orbit(static_cast<float>(dx), static_cast<float>(dy));
        }
        if (g_input.middle_mouse_down) {
            g_renderer->camera().pan(static_cast<float>(-dx), static_cast<float>(dy));
        }
    }

    g_input.last_mouse_x = xpos;
    g_input.last_mouse_y = ypos;
}

static void scroll_callback(GLFWwindow*, double, double yoffset) {
    if (g_renderer) {
        g_renderer->camera().zoom(static_cast<float>(yoffset));
    }
}

static void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        spdlog::info("Manual shader reload requested");
        if (g_renderer) {
            g_renderer->reload_shaders();
        }
    }
}

// =============================================================================
// ECS-Integrated Scene Manager
// =============================================================================

/// Bridge between ECS, Assets, and Renderer
/// When LiveSceneManager loads/reloads a scene, this:
/// 1. Queues external assets for loading (textures, models)
/// 2. Syncs scene data to renderer for GPU resources
class EcsSceneBridge {
public:
    EcsSceneBridge(void_ecs::World* world, void_render::SceneRenderer* renderer,
                   void_asset::AssetServer* assets = nullptr)
        : m_world(world), m_renderer(renderer), m_assets(assets) {}

    /// Called when a scene is loaded or hot-reloaded
    void on_scene_changed(const fs::path& path, const void_scene::SceneData& scene) {
        spdlog::info("ECS scene synced: {}", path.filename().string());
        spdlog::info("  - ECS Entities: {}", m_world->entity_count());
        spdlog::info("  - Cameras: {}", scene.cameras.size());
        spdlog::info("  - Lights: {}", scene.lights.size());
        spdlog::info("  - Mesh Entities: {}", scene.entities.size());

        // Queue external assets for loading if asset server is available
        if (m_assets) {
            queue_scene_assets(scene);
        }

        // Feed scene data to renderer for GPU resources
        m_renderer->load_scene(scene);
    }

    /// Get the ECS world
    void_ecs::World* world() { return m_world; }

    /// Get the asset server
    void_asset::AssetServer* assets() { return m_assets; }

private:
    /// Queue any external assets referenced by the scene
    void queue_scene_assets(const void_scene::SceneData& scene) {
        std::size_t queued = 0;

        // Check for texture references in materials
        for (const auto& entity : scene.entities) {
            // Skip entities without materials
            if (!entity.material) continue;

            const auto& mat = *entity.material;

            // Albedo texture
            if (mat.albedo.has_texture()) {
                m_assets->load<void_asset::TextureAsset>(*mat.albedo.texture_path);
                queued++;
            }
            // Normal map
            if (mat.normal_map.has_value()) {
                m_assets->load<void_asset::TextureAsset>(*mat.normal_map);
                queued++;
            }
            // Metallic texture
            if (mat.metallic.has_texture()) {
                m_assets->load<void_asset::TextureAsset>(*mat.metallic.texture_path);
                queued++;
            }
            // Roughness texture
            if (mat.roughness.has_texture()) {
                m_assets->load<void_asset::TextureAsset>(*mat.roughness.texture_path);
                queued++;
            }
        }

        // Check for model references (external meshes vs built-in shapes)
        for (const auto& entity : scene.entities) {
            // If mesh is a file path (contains '.' for extension), queue it
            if (entity.mesh.find('.') != std::string::npos) {
                m_assets->load<void_asset::ModelAsset>(entity.mesh);
                queued++;
            }
        }

        if (queued > 0) {
            spdlog::info("  - Queued {} assets for loading", queued);
        }
    }

    void_ecs::World* m_world;
    void_render::SceneRenderer* m_renderer;
    void_asset::AssetServer* m_assets;
};

// =============================================================================
// Project Configuration
// =============================================================================

struct ProjectConfig {
    std::string name;
    std::string display_name;
    std::string version;
    std::string scene_file;
    fs::path project_dir;
    int window_width = 1280;
    int window_height = 720;
    bool valid = false;
    std::string error;
};

ProjectConfig load_manifest(const fs::path& manifest_path) {
    ProjectConfig config;

    if (!fs::exists(manifest_path)) {
        config.error = "Manifest file not found: " + manifest_path.string();
        return config;
    }

    config.project_dir = manifest_path.parent_path();

    try {
        auto tbl = toml::parse_file(manifest_path.string());

        // Parse [package] section
        if (auto pkg = tbl["package"].as_table()) {
            config.name = (*pkg)["name"].value_or<std::string>("unnamed");
            config.display_name = (*pkg)["display_name"].value_or(config.name);
            config.version = (*pkg)["version"].value_or<std::string>("0.0.0");
        } else {
            config.error = "Missing [package] section in manifest";
            return config;
        }

        // Parse [app] section
        if (auto app = tbl["app"].as_table()) {
            config.scene_file = (*app)["scene"].value_or<std::string>("");
        } else {
            config.error = "Missing [app] section in manifest";
            return config;
        }

        // Parse [window] section (optional)
        if (auto win = tbl["window"].as_table()) {
            config.window_width = (*win)["width"].value_or(1280);
            config.window_height = (*win)["height"].value_or(720);
        }

        config.valid = true;

    } catch (const toml::parse_error& err) {
        config.error = "Failed to parse manifest: " + std::string(err.what());
    }

    return config;
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS] [PROJECT_PATH]\n"
              << "\n"
              << "Arguments:\n"
              << "  PROJECT_PATH    Path to project directory or manifest.toml\n"
              << "\n"
              << "Options:\n"
              << "  --help, -h      Show this help message\n"
              << "  --version, -v   Show version information\n"
              << "\n"
              << "Controls:\n"
              << "  Left Mouse + Drag   Orbit camera\n"
              << "  Middle Mouse + Drag Pan camera\n"
              << "  Scroll              Zoom\n"
              << "  R                   Reload shaders\n"
              << "  ESC                 Quit\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " examples/model-viewer\n"
              << "  " << program_name << " examples/model-viewer/manifest.toml\n";
}

void print_version() {
    std::cout << "void_runtime 0.1.0\n"
              << "void_engine C++ Runtime\n";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    fs::path project_path;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            print_version();
            return 0;
        } else if (arg[0] != '-') {
            project_path = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (project_path.empty()) {
        std::cerr << "Error: No project specified.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    // Resolve manifest path
    fs::path manifest_path;
    if (fs::is_directory(project_path)) {
        manifest_path = project_path / "manifest.toml";
    } else if (fs::is_regular_file(project_path)) {
        manifest_path = project_path;
    } else {
        std::cerr << "Project path does not exist: " << project_path << "\n";
        return 1;
    }

    // Load manifest
    spdlog::info("Loading project: {}", manifest_path.string());
    auto config = load_manifest(manifest_path);

    if (!config.valid) {
        spdlog::error("Failed to load project: {}", config.error);
        return 1;
    }

    spdlog::info("Project: {} v{}", config.display_name, config.version);

    // Initialize GLFW
    if (!glfwInit()) {
        spdlog::error("Failed to initialize GLFW");
        return 1;
    }

    // Request OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    std::string window_title = config.display_name + " - void_engine";
    GLFWwindow* window = glfwCreateWindow(
        config.window_width, config.window_height,
        window_title.c_str(), nullptr, nullptr);

    if (!window) {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync

    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    // Initialize renderer
    void_render::SceneRenderer renderer;
    g_renderer = &renderer;

    if (!renderer.initialize(window)) {
        spdlog::error("Failed to initialize renderer");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // ==========================================================================
    // Service Registry & Event Bus - Engine lifecycle management
    // ==========================================================================
    spdlog::info("Initializing Service Registry and Event Bus...");

    // Create the central event bus for inter-system communication
    void_services::EventBus event_bus;

    // Create service registry for lifecycle management
    void_services::ServiceRegistry service_registry;

    // Subscribe to service lifecycle events for logging
    service_registry.set_event_callback([](const void_services::ServiceEvent& event) {
        switch (event.type) {
            case void_services::ServiceEventType::Started:
                spdlog::info("Service started: {}", event.service_id.name);
                break;
            case void_services::ServiceEventType::Stopped:
                spdlog::info("Service stopped: {}", event.service_id.name);
                break;
            case void_services::ServiceEventType::Failed:
                spdlog::error("Service failed: {} - {}", event.service_id.name, event.message);
                break;
            case void_services::ServiceEventType::HealthChanged:
                spdlog::debug("Service health changed: {}", event.service_id.name);
                break;
            default:
                break;
        }
    });

    // Define engine events for the event bus
    struct FrameStartEvent { float delta_time; };
    struct FrameEndEvent { int frame_number; };
    struct SceneLoadedEvent { std::string scene_path; std::size_t entity_count; };
    struct AssetLoadedEvent { std::string asset_path; };

    // Subscribe to engine events for debugging/extension
    event_bus.subscribe<SceneLoadedEvent>([](const SceneLoadedEvent& e) {
        spdlog::debug("EventBus: Scene loaded - {} ({} entities)", e.scene_path, e.entity_count);
    });

    event_bus.subscribe<AssetLoadedEvent>([](const AssetLoadedEvent& e) {
        spdlog::debug("EventBus: Asset loaded - {}", e.asset_path);
    });

    spdlog::info("Service Registry initialized");
    spdlog::info("Event Bus initialized with engine event types");

    // ==========================================================================
    // Asset Server - 3-tier cache with hot-reload
    // ==========================================================================
    spdlog::info("Initializing Asset Server...");

    void_asset::AssetServerConfig asset_config;
    asset_config.asset_dir = (config.project_dir / "assets").string();
    asset_config.hot_reload = true;
    asset_config.max_concurrent_loads = 4;

    void_asset::AssetServer asset_server(asset_config);

    // Register asset loaders for textures and models
    asset_server.register_loader<void_asset::TextureAsset>(
        std::make_unique<void_asset::TextureLoader>());
    asset_server.register_loader<void_asset::ModelAsset>(
        std::make_unique<void_asset::ModelLoader>());

    // Create hot-reload adapter for asset server
    auto asset_hot_reload = void_asset::make_hot_reloadable(asset_server);

    spdlog::info("Asset Server initialized:");
    spdlog::info("  - Asset directory: {}", asset_config.asset_dir);
    spdlog::info("  - Hot-reload: {}", asset_config.hot_reload ? "enabled" : "disabled");
    spdlog::info("  - Registered loaders: textures, models");

    // ==========================================================================
    // ECS World - Authoritative source of scene entities
    // ==========================================================================
    spdlog::info("Initializing ECS World...");
    void_ecs::World ecs_world(1024);  // Pre-allocate capacity for 1024 entities

    // ECS-Asset-Renderer bridge
    EcsSceneBridge ecs_bridge(&ecs_world, &renderer, &asset_server);

    // LiveSceneManager: Loads scenes into ECS with hot-reload support
    void_scene::LiveSceneManager live_scene_mgr(&ecs_world);

    // Initialize the scene manager (sets up file watching, registers components)
    auto init_result = live_scene_mgr.initialize();
    if (!init_result) {
        spdlog::error("Failed to initialize LiveSceneManager");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Set up callback: when scene changes, sync to renderer
    live_scene_mgr.on_scene_changed([&ecs_bridge](const fs::path& path, const void_scene::SceneData& scene) {
        ecs_bridge.on_scene_changed(path, scene);
    });

    // ==========================================================================
    // Physics World - Rigidbody dynamics and collision detection
    // ==========================================================================
    spdlog::info("Initializing Physics World...");

    // Build physics world with sensible defaults and hot-reload enabled
    auto physics_world = void_physics::PhysicsWorldBuilder()
        .gravity(0.0f, -9.81f, 0.0f)
        .fixed_timestep(1.0f / 60.0f)
        .max_substeps(4)
        .max_bodies(10000)
        .enable_ccd(true)
        .hot_reload(true)
        .debug_rendering(false)
        .build();

    // Set up collision callbacks for debugging/game logic
    physics_world->on_collision_begin([](const void_physics::CollisionEvent& event) {
        spdlog::debug("Collision begin: body {} <-> body {}",
                      event.body_a.value, event.body_b.value);
    });

    physics_world->on_trigger_enter([](const void_physics::TriggerEvent& event) {
        spdlog::debug("Trigger enter: {} entered trigger {}",
                      event.other_body.value, event.trigger_body.value);
    });

    spdlog::info("Physics World initialized:");
    spdlog::info("  - Gravity: (0, -9.81, 0)");
    spdlog::info("  - Fixed timestep: 60 Hz");
    spdlog::info("  - Max bodies: 10000");
    spdlog::info("  - CCD: enabled");
    spdlog::info("  - Hot-reload: enabled");

    // ==========================================================================
    // Load Initial Scene
    // ==========================================================================
    if (!config.scene_file.empty()) {
        fs::path scene_path = config.project_dir / config.scene_file;
        spdlog::info("Scene file: {}", scene_path.string());

        auto load_result = live_scene_mgr.load_scene(scene_path);
        if (!load_result) {
            spdlog::error("Failed to load scene: {}", load_result.error().message());
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }

        spdlog::info("Scene loaded into ECS - {} entities active", ecs_world.entity_count());

        // Publish scene loaded event through event bus
        event_bus.publish(SceneLoadedEvent{scene_path.string(), ecs_world.entity_count()});
    } else {
        spdlog::error("No scene file specified in manifest");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Enable hot-reload
    renderer.set_shader_hot_reload(true);
    live_scene_mgr.set_hot_reload_enabled(true);

    // Start health monitoring for registered services
    service_registry.start_health_monitor();

    // ==========================================================================
    // Frame Timing - Presenter's frame pacing and statistics
    // ==========================================================================
    spdlog::info("Initializing Frame Timing...");

    // Create frame timing with 60 FPS target (VSync handles actual pacing via glfwSwapInterval)
    void_presenter::FrameTiming frame_timing(60);

    spdlog::info("Frame Timing initialized:");
    spdlog::info("  - Target FPS: {}", 60);
    spdlog::info("  - History size: 120 frames");

    // ==========================================================================
    // Compositor - Post-processing and final output
    // ==========================================================================
    spdlog::info("Initializing Compositor...");

    void_compositor::CompositorConfig compositor_config;
    compositor_config.target_fps = 60;
    compositor_config.vsync = true;
    compositor_config.enable_vrr = false;
    compositor_config.enable_hdr = false;  // Start with SDR
    compositor_config.preferred_format = void_compositor::RenderFormat::Bgra8UnormSrgb;

    auto compositor = void_compositor::CompositorFactory::create(compositor_config);
    if (!compositor) {
        spdlog::warn("Compositor creation failed, using null compositor");
        compositor = void_compositor::CompositorFactory::create_null(compositor_config);
    }

    spdlog::info("Compositor initialized:");
    spdlog::info("  - Backend: {}", void_compositor::CompositorFactory::backend_name());
    spdlog::info("  - HDR: {}", compositor_config.enable_hdr ? "ON" : "OFF");
    spdlog::info("  - VRR: {}", compositor_config.enable_vrr ? "ON" : "OFF");
    spdlog::info("  - Target FPS: {}", compositor_config.target_fps);

    spdlog::info("=== void_engine Runtime Started ===");
    spdlog::info("Systems active:");
    spdlog::info("  - Service Registry: health monitoring ON");
    spdlog::info("  - Event Bus: inter-system messaging ON");
    spdlog::info("  - Frame Timing: 60 FPS target, statistics ON");
    spdlog::info("  - ECS World: {} entity capacity", 1024);
    spdlog::info("  - Physics World: {} body capacity", 10000);
    spdlog::info("  - Asset Server: hot-reload {}", asset_config.hot_reload ? "ON" : "OFF");
    spdlog::info("  - Scene Manager: {}", config.scene_file);
    spdlog::info("  - Renderer: shader hot-reload ON");
    spdlog::info("  - Compositor: {}, HDR={}, VRR={}",
                 void_compositor::CompositorFactory::backend_name(),
                 compositor_config.enable_hdr ? "ON" : "OFF",
                 compositor_config.enable_vrr ? "ON" : "OFF");
    spdlog::info("Controls: Left-drag=orbit, Middle-drag=pan, Scroll=zoom, R=reload shaders, ESC=quit");

    // Main loop
    int frame_count = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    float hot_reload_timer = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        // Use FrameTiming for accurate delta time tracking and statistics
        auto now = frame_timing.begin_frame();
        float delta_time = frame_timing.delta_time();

        glfwPollEvents();

        // =======================================================================
        // Service & Event Bus Update Phase
        // =======================================================================

        // Publish frame start event for subscribers
        event_bus.publish(FrameStartEvent{delta_time});

        // Process any queued events
        event_bus.process_queue();

        // =======================================================================
        // Asset Server Update Phase
        // =======================================================================

        // Process pending asset loads (async loading with callbacks)
        asset_server.process();

        // Handle asset events (loaded, failed, reloaded)
        for (const auto& asset_event : asset_server.drain_events()) {
            switch (asset_event.type) {
                case void_asset::AssetEventType::Loaded:
                    spdlog::debug("Asset loaded: {}", asset_event.path.str());
                    // Publish through event bus for any subscribers
                    event_bus.publish(AssetLoadedEvent{asset_event.path.str()});
                    break;
                case void_asset::AssetEventType::Failed:
                    spdlog::warn("Asset failed: {} - {}", asset_event.path.str(), asset_event.error);
                    break;
                case void_asset::AssetEventType::Reloaded:
                    spdlog::info("Asset hot-reloaded: {}", asset_event.path.str());
                    event_bus.publish(AssetLoadedEvent{asset_event.path.str()});
                    break;
                case void_asset::AssetEventType::Unloaded:
                    spdlog::debug("Asset unloaded: {}", asset_event.path.str());
                    break;
                case void_asset::AssetEventType::FileChanged:
                    spdlog::debug("Asset file changed: {}", asset_event.path.str());
                    break;
            }
        }

        // =======================================================================
        // Physics Update Phase
        // =======================================================================

        // Step physics simulation (uses fixed timestep internally with accumulator)
        physics_world->step(delta_time);

        // TODO: Sync physics transforms back to ECS entities
        // When we add RigidbodyComponent to ECS, we'll iterate physics bodies
        // and update their corresponding TransformComponents here.
        // Example:
        // physics_world->for_each_body([&ecs_world](const void_physics::IRigidbody& body) {
        //     auto entity_id = body.user_id();
        //     if (auto* transform = ecs_world.get_component<TransformComponent>(entity_id)) {
        //         transform->position = body.position();
        //         transform->rotation = body.rotation();
        //     }
        // });

        // =======================================================================
        // ECS Update Phase
        // =======================================================================

        // Check for scene file changes and hot-reload into ECS
        hot_reload_timer += delta_time;
        if (hot_reload_timer >= 0.5f) {
            hot_reload_timer = 0.0f;
            live_scene_mgr.update(delta_time);
        }

        // Update ECS animation system (transforms updated in ECS)
        void_scene::AnimationSystem::update(ecs_world, delta_time);

        // =======================================================================
        // Render Phase
        // =======================================================================

        // Update renderer (shader hot-reload, animation sync)
        renderer.update(delta_time);

        // Render the scene
        renderer.render();

        // =======================================================================
        // Compositor Post-Processing Phase
        // =======================================================================

        // Dispatch compositor events (VRR timing, input, etc.)
        compositor->dispatch();

        // Begin compositor frame if we should render
        if (compositor->should_render()) {
            auto render_target = compositor->begin_frame();
            if (render_target) {
                // In a full GPU pipeline, post-processing would happen here
                // For now, we just end the frame
                compositor->end_frame(std::move(render_target));
            }
        }

        // Update content velocity for VRR adaptation
        compositor->update_content_velocity(0.5f);  // Mid-velocity for typical 3D scene

        glfwSwapBuffers(window);

        // Publish frame end event
        event_bus.publish(FrameEndEvent{frame_count});

        // FPS counter with ECS, Physics, Services, Presenter, and Asset stats
        frame_count++;
        auto fps_elapsed = std::chrono::duration<double>(now - last_fps_time).count();
        if (fps_elapsed >= 1.0) {
            auto& render_stats = renderer.stats();
            auto physics_stats = physics_world->stats();
            auto service_stats = service_registry.stats();
            auto event_stats = event_bus.stats();

            // Use frame_timing for accurate FPS reporting
            double avg_fps = frame_timing.average_fps();
            float frame_ms = std::chrono::duration<float, std::milli>(
                frame_timing.average_frame_duration()).count();

            auto& compositor_scheduler = compositor->frame_scheduler();
            spdlog::info("FPS: {:.1f} ({:.2f}ms) | Draws: {} | Tris: {} | ECS: {} | Physics: {}/{} | Assets: {} | Comp: {:.1f}fps",
                         avg_fps, frame_ms,
                         render_stats.draw_calls, render_stats.triangles,
                         ecs_world.entity_count(),
                         physics_stats.active_bodies, physics_world->body_count(),
                         asset_server.loaded_count(),
                         compositor_scheduler.current_fps());
            frame_count = 0;
            last_fps_time = now;

            // Periodic garbage collection
            auto gc_count = asset_server.collect_garbage();
            if (gc_count > 0) {
                spdlog::debug("Asset GC: {} unreferenced assets cleaned", gc_count);
            }

            // Log service health if any services are degraded
            if (service_stats.degraded_services > 0 || service_stats.failed_services > 0) {
                spdlog::warn("Services: {} running, {} degraded, {} failed",
                             service_stats.running_services,
                             service_stats.degraded_services,
                             service_stats.failed_services);
            }

            // Log event bus throughput
            if (event_stats.events_processed > 0) {
                spdlog::debug("Events: {} processed, {} subscriptions",
                              event_stats.events_processed, event_stats.active_subscriptions);
            }
        }
    }

    spdlog::info("Shutting down...");

    // Log final frame timing statistics
    spdlog::info("Frame Timing final stats: {} total frames, {:.1f} avg FPS, {:.2f}ms avg frame time",
                 frame_timing.frame_count(),
                 frame_timing.average_fps(),
                 std::chrono::duration<float, std::milli>(frame_timing.average_frame_duration()).count());

    // Shutdown in reverse order of initialization

    // Stop service health monitoring first
    service_registry.stop_health_monitor();
    spdlog::info("Service health monitor stopped");

    // Stop all registered services
    service_registry.stop_all();
    auto final_service_stats = service_registry.stats();
    spdlog::info("Services stopped: {} total, {} restarts during session",
                 final_service_stats.total_services, final_service_stats.total_restarts);

    // Log final event bus statistics
    auto final_event_stats = event_bus.stats();
    spdlog::info("Event Bus final stats: {} published, {} processed, {} dropped",
                 final_event_stats.events_published,
                 final_event_stats.events_processed,
                 final_event_stats.events_dropped);

    live_scene_mgr.shutdown();       // Unload all scenes, destroy ECS entities
    ecs_world.clear();               // Clear any remaining ECS state

    // Log final physics statistics
    auto final_physics_stats = physics_world->stats();
    spdlog::info("Physics World final stats: {} bodies, {} active, {} sleeping",
                 physics_world->body_count(),
                 final_physics_stats.active_bodies,
                 final_physics_stats.sleeping_bodies);
    physics_world->clear();          // Destroy all physics bodies and joints

    asset_server.collect_garbage();  // Clean up unreferenced assets

    // Log final asset statistics
    spdlog::info("Asset Server final stats: {} loaded, {} pending",
                 asset_server.loaded_count(), asset_server.pending_count());

    // Shutdown compositor
    spdlog::info("Compositor final stats: {} frames, {:.1f} avg FPS",
                 compositor->frame_number(),
                 compositor->frame_scheduler().current_fps());
    compositor->shutdown();

    g_renderer = nullptr;
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("Shutdown complete.");
    return 0;
}
