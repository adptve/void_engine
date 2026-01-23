/// @file main.cpp
/// @brief void_runtime entry point - loads and runs void_engine projects
///
/// This is the main runtime that loads manifest.toml, parses scene.toml,
/// and renders using the SceneRenderer with full hot-reload support.

#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/scene/scene_parser.hpp>
#include <void_engine/scene/scene_data.hpp>

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
// Scene Hot-Reload
// =============================================================================

class SceneHotReloader {
public:
    SceneHotReloader(const fs::path& scene_path, void_render::SceneRenderer* renderer)
        : m_path(scene_path), m_renderer(renderer) {
        update_mtime();
    }

    void check_reload() {
        std::error_code ec;
        auto current_mtime = fs::last_write_time(m_path, ec);
        if (ec) return;

        if (current_mtime != m_last_mtime) {
            m_last_mtime = current_mtime;
            reload_scene();
        }
    }

    bool load_initial() {
        return reload_scene();
    }

private:
    bool reload_scene() {
        spdlog::info("Loading scene: {}", m_path.filename().string());

        void_scene::SceneParser parser;
        auto result = parser.parse(m_path);

        if (!result) {
            spdlog::error("Failed to parse scene: {}", parser.last_error());
            return false;
        }

        m_renderer->load_scene(result.value());

        const auto& scene = result.value();
        spdlog::info("Scene loaded: {}", scene.metadata.name);
        spdlog::info("  - Cameras: {}", scene.cameras.size());
        spdlog::info("  - Lights: {}", scene.lights.size());
        spdlog::info("  - Entities: {}", scene.entities.size());

        return true;
    }

    void update_mtime() {
        std::error_code ec;
        m_last_mtime = fs::last_write_time(m_path, ec);
    }

    fs::path m_path;
    void_render::SceneRenderer* m_renderer;
    fs::file_time_type m_last_mtime;
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

    // Load scene
    std::unique_ptr<SceneHotReloader> hot_reloader;

    if (!config.scene_file.empty()) {
        fs::path scene_path = config.project_dir / config.scene_file;
        spdlog::info("Scene file: {}", scene_path.string());

        hot_reloader = std::make_unique<SceneHotReloader>(scene_path, &renderer);
        if (!hot_reloader->load_initial()) {
            spdlog::error("Failed to load scene");
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
    } else {
        spdlog::error("No scene file specified in manifest");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Enable hot-reload
    renderer.set_shader_hot_reload(true);

    spdlog::info("Starting render loop...");
    spdlog::info("Hot-reload enabled - modify scene.toml while running!");
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

        // Check for scene hot-reload
        hot_reload_timer += delta_time;
        if (hot_reload_timer >= 0.5f) {
            hot_reload_timer = 0.0f;
            if (hot_reloader) {
                hot_reloader->check_reload();
            }
        }

        // Update (animations, shader hot-reload)
        renderer.update(delta_time);

        // Render
        renderer.render();

        glfwSwapBuffers(window);

        // FPS counter
        frame_count++;
        auto fps_elapsed = std::chrono::duration<double>(now - last_fps_time).count();
        if (fps_elapsed >= 1.0) {
            auto& stats = renderer.stats();
            spdlog::info("FPS: {} | Draw calls: {} | Triangles: {} | Entities: {}",
                         frame_count, stats.draw_calls, stats.triangles, stats.entities);
            frame_count = 0;
            last_fps_time = now;
        }
    }

    spdlog::info("Shutting down...");

    g_renderer = nullptr;
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    spdlog::info("Shutdown complete.");
    return 0;
}
