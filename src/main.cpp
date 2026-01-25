/// @file main.cpp
/// @brief void_runtime entry point - loads and runs void_engine projects
///
/// This is the main runtime that loads manifest.toml, parses scene.toml,
/// and renders using the SceneRenderer with full ECS, asset, and hot-reload support.
///
/// Architecture:
/// - ECS World: Authoritative source of scene entities
/// - AssetServer: Loads textures, models, shaders with 3-tier cache
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
#include <void_engine/core/hot_reload.hpp>

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
            // Albedo texture
            if (entity.material.albedo.has_texture()) {
                m_assets->load<void_asset::TextureAsset>(*entity.material.albedo.texture_path);
                queued++;
            }
            // Normal map
            if (entity.material.normal_map.has_value()) {
                m_assets->load<void_asset::TextureAsset>(*entity.material.normal_map);
                queued++;
            }
            // Metallic texture
            if (entity.material.metallic.has_texture()) {
                m_assets->load<void_asset::TextureAsset>(*entity.material.metallic.texture_path);
                queued++;
            }
            // Roughness texture
            if (entity.material.roughness.has_texture()) {
                m_assets->load<void_asset::TextureAsset>(*entity.material.roughness.texture_path);
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
    // Load Initial Scene
    // ==========================================================================
    if (!config.scene_file.empty()) {
        fs::path scene_path = config.project_dir / config.scene_file;
        spdlog::info("Scene file: {}", scene_path.string());

        auto load_result = live_scene_mgr.load_scene(scene_path);
        if (!load_result) {
            spdlog::error("Failed to load scene: {}", load_result.error().message);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }

        spdlog::info("Scene loaded into ECS - {} entities active", ecs_world.entity_count());
    } else {
        spdlog::error("No scene file specified in manifest");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Enable hot-reload
    renderer.set_shader_hot_reload(true);
    live_scene_mgr.set_hot_reload_enabled(true);

    spdlog::info("=== void_engine Runtime Started ===");
    spdlog::info("Systems active:");
    spdlog::info("  - ECS World: {} entity capacity", 1024);
    spdlog::info("  - Asset Server: hot-reload {}", asset_config.hot_reload ? "ON" : "OFF");
    spdlog::info("  - Scene Manager: {}", config.scene_file);
    spdlog::info("  - Renderer: shader hot-reload ON");
    spdlog::info("Controls: Left-drag=orbit, Middle-drag=pan, Scroll=zoom, R=reload shaders, ESC=quit");

    // Main loop
    int frame_count = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    auto last_frame_time = last_fps_time;
    float hot_reload_timer = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        auto now = std::chrono::steady_clock::now();
        float delta_time = std::chrono::duration<float>(now - last_frame_time).count();
        last_frame_time = now;

        glfwPollEvents();

        // =======================================================================
        // Asset Server Update Phase
        // =======================================================================

        // Process pending asset loads (async loading with callbacks)
        asset_server.process();

        // Handle asset events (loaded, failed, reloaded)
        for (const auto& event : asset_server.drain_events()) {
            switch (event.type) {
                case void_asset::AssetEventType::Loaded:
                    spdlog::debug("Asset loaded: {}", event.path.str());
                    break;
                case void_asset::AssetEventType::Failed:
                    spdlog::warn("Asset failed: {} - {}", event.path.str(), event.error);
                    break;
                case void_asset::AssetEventType::Reloaded:
                    spdlog::info("Asset hot-reloaded: {}", event.path.str());
                    break;
                case void_asset::AssetEventType::Unloaded:
                    spdlog::debug("Asset unloaded: {}", event.path.str());
                    break;
                case void_asset::AssetEventType::FileChanged:
                    spdlog::debug("Asset file changed: {}", event.path.str());
                    break;
            }
        }

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

        glfwSwapBuffers(window);

        // FPS counter with ECS and Asset stats
        frame_count++;
        auto fps_elapsed = std::chrono::duration<double>(now - last_fps_time).count();
        if (fps_elapsed >= 1.0) {
            auto& stats = renderer.stats();
            spdlog::info("FPS: {} | Draws: {} | Tris: {} | ECS: {} | Assets: {}/{}",
                         frame_count, stats.draw_calls, stats.triangles,
                         ecs_world.entity_count(),
                         asset_server.loaded_count(), asset_server.total_count());
            frame_count = 0;
            last_fps_time = now;

            // Periodic garbage collection
            auto gc_count = asset_server.collect_garbage();
            if (gc_count > 0) {
                spdlog::debug("Asset GC: {} unreferenced assets cleaned", gc_count);
            }
        }
    }

    spdlog::info("Shutting down...");

    // Shutdown in reverse order of initialization
    live_scene_mgr.shutdown();       // Unload all scenes, destroy ECS entities
    ecs_world.clear();               // Clear any remaining ECS state
    asset_server.collect_garbage();  // Clean up unreferenced assets

    // Log final asset statistics
    spdlog::info("Asset Server final stats: {} loaded, {} pending",
                 asset_server.loaded_count(), asset_server.pending_count());

    g_renderer = nullptr;
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("Shutdown complete.");
    return 0;
}
