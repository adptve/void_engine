/// @file runtime.cpp
/// @brief Main runtime system implementation for void_runtime

#include "runtime.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace void_runtime {

// =============================================================================
// Application Implementation
// =============================================================================

static Application* g_app_instance = nullptr;

Application::Application() {
    g_app_instance = this;
}

Application::~Application() {
    if (initialized_) {
        // Notify shutdown
        if (config_.on_shutdown) {
            config_.on_shutdown();
        }
        on_shutdown();
    }

    if (g_app_instance == this) {
        g_app_instance = nullptr;
    }
}

Application& Application::instance() {
    if (!g_app_instance) {
        static Application static_instance;
        g_app_instance = &static_instance;
    }
    return *g_app_instance;
}

Application* Application::instance_ptr() {
    return g_app_instance;
}

bool Application::initialize(const ApplicationConfig& config) {
    if (initialized_) {
        return true;
    }

    config_ = config;

    // Setup paths
    setup_paths();

    // Initialize crash handler first
    if (config.enable_crash_handler) {
        crash_handler_ = std::make_unique<CrashHandler>();
        crash_handler_->set_app_name(config.app_name);
        crash_handler_->set_app_version(config.app_version);
        crash_handler_->set_dump_directory(config.log_path);
        crash_handler_->install();
    }

    // Initialize input manager
    input_manager_ = std::make_unique<InputManager>();
    if (!input_manager_->initialize()) {
        std::cerr << "Failed to initialize input manager" << std::endl;
        return false;
    }

    // Create main window
    main_window_ = std::make_unique<Window>();
    if (!main_window_->create(config.main_window)) {
        std::cerr << "Failed to create main window" << std::endl;
        return false;
    }

    // Initialize scene loader
    scene_loader_ = std::make_unique<SceneLoader>();
    if (!scene_loader_->initialize()) {
        std::cerr << "Failed to initialize scene loader" << std::endl;
        return false;
    }

    // Add default scene search paths
    if (!config.data_path.empty()) {
        scene_loader_->add_search_path(config.data_path / "scenes");
    }

    // Enable hot reload if configured
    if (config.enable_hot_reload) {
        scene_loader_->enable_hot_reload(true);
    }

    // Record start time
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;
    stats_.start_time = start_time_;

    // Call user init callback
    if (config.on_init) {
        config.on_init();
    }

    // Virtual init
    if (!on_init()) {
        return false;
    }

    // Load startup content
    load_startup_content();

    initialized_ = true;

    // Emit application started event
    if (event_bus_) {
        ApplicationStartedEvent event;
        event.timestamp = std::chrono::system_clock::now();
        event_bus_->publish(event);
    }

    return true;
}

int Application::run() {
    if (!initialized_) {
        return -1;
    }

    running_.store(true);
    main_loop();

    return exit_code_;
}

void Application::quit(int exit_code) {
    exit_code_ = exit_code;
    running_.store(false);

    if (event_bus_) {
        ApplicationStoppingEvent event;
        event.exit_code = exit_code;
        event_bus_->publish(event);
    }
}

void Application::main_loop() {
    while (running_.load()) {
        // Check if window should close
        if (main_window_ && main_window_->should_close()) {
            quit(0);
            break;
        }

        process_frame();
    }
}

void Application::process_frame() {
    auto frame_start = std::chrono::steady_clock::now();

    // Calculate delta time
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - last_frame_time_;
    delta_time_ = elapsed.count();
    last_frame_time_ = now;

    // Clamp delta time to prevent spiral of death
    if (delta_time_ > 0.25) {
        delta_time_ = 0.25;
    }

    // Update time since start
    std::chrono::duration<double> since_start = now - start_time_;
    time_since_start_ = since_start.count();

    // Emit frame started event
    if (event_bus_) {
        FrameStartedEvent event;
        event.frame_number = frame_count_;
        event.delta_time = delta_time_;
        event_bus_->publish(event);
    }

    // Poll window events
    if (main_window_) {
        main_window_->poll_events();
    }

    // Update input
    if (input_manager_) {
        input_manager_->update();
    }

    // Fixed timestep updates
    auto fixed_update_start = std::chrono::steady_clock::now();
    accumulator_ += delta_time_;
    std::uint32_t fixed_steps = 0;

    while (accumulator_ >= config_.fixed_timestep && fixed_steps < config_.max_fixed_steps_per_frame) {
        // User fixed update callback
        if (fixed_update_callback_) {
            fixed_update_callback_(config_.fixed_timestep);
        }

        // Virtual fixed update
        on_fixed_update(config_.fixed_timestep);

        accumulator_ -= config_.fixed_timestep;
        fixed_steps++;
    }

    auto fixed_update_end = std::chrono::steady_clock::now();
    stats_.fixed_update_time_ms = std::chrono::duration<double, std::milli>(
        fixed_update_end - fixed_update_start).count();
    stats_.fixed_updates_this_frame = fixed_steps;

    // Regular update
    auto update_start = std::chrono::steady_clock::now();

    // Scene loader update (processes async loads)
    if (scene_loader_) {
        scene_loader_->update();
    }

    // User update callback
    if (update_callback_) {
        update_callback_(delta_time_);
    }

    // Virtual update
    on_update(delta_time_);

    auto update_end = std::chrono::steady_clock::now();
    stats_.update_time_ms = std::chrono::duration<double, std::milli>(
        update_end - update_start).count();

    // Render
    auto render_start = std::chrono::steady_clock::now();

    // User render callback
    if (render_callback_) {
        render_callback_();
    }

    // Virtual render
    on_render();

    // Swap buffers
    if (main_window_) {
        main_window_->swap_buffers();
    }

    auto render_end = std::chrono::steady_clock::now();
    stats_.render_time_ms = std::chrono::duration<double, std::milli>(
        render_end - render_start).count();

    // Frame timing
    auto frame_end = std::chrono::steady_clock::now();
    double frame_time = std::chrono::duration<double, std::milli>(
        frame_end - frame_start).count();

    // Frame rate limiting (if not using vsync and not unlimited)
    if (!config_.vsync && !config_.unlimited_fps && config_.target_fps > 0) {
        double target_frame_time = 1000.0 / config_.target_fps;
        if (frame_time < target_frame_time) {
            double sleep_time = target_frame_time - frame_time;
            std::this_thread::sleep_for(std::chrono::microseconds(
                static_cast<long long>(sleep_time * 1000)));
        }
    }

    // Update statistics
    update_stats(frame_time);

    frame_count_++;

    // Emit frame ended event
    if (event_bus_) {
        FrameEndedEvent event;
        event.frame_number = frame_count_;
        event.frame_time = frame_time;
        event_bus_->publish(event);
    }
}

void Application::update_stats(double frame_time) {
    stats_.frame_count = frame_count_;
    stats_.frame_time_ms = frame_time;
    stats_.uptime_seconds = time_since_start_;

    // Calculate FPS
    static double fps_accumulator = 0.0;
    static int fps_frame_count = 0;
    static auto last_fps_update = std::chrono::steady_clock::now();

    fps_accumulator += frame_time;
    fps_frame_count++;

    auto now = std::chrono::steady_clock::now();
    auto fps_elapsed = std::chrono::duration<double>(now - last_fps_update).count();

    if (fps_elapsed >= 0.5) {  // Update FPS twice per second
        stats_.fps = fps_frame_count / fps_elapsed;
        stats_.avg_frame_time_ms = fps_accumulator / fps_frame_count;

        fps_accumulator = 0.0;
        fps_frame_count = 0;
        last_fps_update = now;
    }

    // Track min/max frame time
    if (frame_time < stats_.min_frame_time_ms || stats_.min_frame_time_ms == 0) {
        stats_.min_frame_time_ms = frame_time;
    }
    if (frame_time > stats_.max_frame_time_ms) {
        stats_.max_frame_time_ms = frame_time;
    }
}

void Application::setup_paths() {
    // Set default paths if not specified
    if (config_.data_path.empty()) {
        config_.data_path = std::filesystem::current_path() / "data";
    }

    if (config_.cache_path.empty()) {
#ifdef _WIN32
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
            config_.cache_path = std::filesystem::path(path) / config_.organization / config_.app_name / "cache";
        } else {
            config_.cache_path = std::filesystem::temp_directory_path() / config_.app_name / "cache";
        }
#else
        const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
        if (xdg_cache) {
            config_.cache_path = std::filesystem::path(xdg_cache) / config_.app_name;
        } else {
            config_.cache_path = std::filesystem::path(std::getenv("HOME")) / ".cache" / config_.app_name;
        }
#endif
    }

    if (config_.log_path.empty()) {
        config_.log_path = config_.cache_path / "logs";
    }

    if (config_.config_path.empty()) {
#ifdef _WIN32
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
            config_.config_path = std::filesystem::path(path) / config_.organization / config_.app_name;
        } else {
            config_.config_path = config_.data_path / "config";
        }
#else
        const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
        if (xdg_config) {
            config_.config_path = std::filesystem::path(xdg_config) / config_.app_name;
        } else {
            config_.config_path = std::filesystem::path(std::getenv("HOME")) / ".config" / config_.app_name;
        }
#endif
    }

    // Create directories if they don't exist
    std::filesystem::create_directories(config_.data_path);
    std::filesystem::create_directories(config_.cache_path);
    std::filesystem::create_directories(config_.log_path);
    std::filesystem::create_directories(config_.config_path);
}

void Application::load_startup_content() {
    // Load startup scene if specified
    if (!config_.startup_scene.empty() && scene_loader_) {
        scene_loader_->load_scene_async(config_.startup_scene, SceneLoadMode::Single,
            [this](const std::string& name, bool success) {
                if (success && event_bus_) {
                    SceneLoadedEvent event;
                    event.scene_name = name;
                    event_bus_->publish(event);
                }
            });
    }
}

// =============================================================================
// Bootstrap Implementation
// =============================================================================

Bootstrap::Bootstrap() {
    // Set sensible defaults
    config_.main_window.title = "Void Application";
    config_.main_window.width = 1280;
    config_.main_window.height = 720;
    config_.main_window.resizable = true;
    config_.main_window.vsync = true;
}

Bootstrap::~Bootstrap() = default;

Bootstrap& Bootstrap::app_name(const std::string& name) {
    config_.app_name = name;
    config_.main_window.title = name;
    return *this;
}

Bootstrap& Bootstrap::app_version(const std::string& version) {
    config_.app_version = version;
    return *this;
}

Bootstrap& Bootstrap::organization(const std::string& org) {
    config_.organization = org;
    return *this;
}

Bootstrap& Bootstrap::window_title(const std::string& title) {
    config_.main_window.title = title;
    return *this;
}

Bootstrap& Bootstrap::window_size(int width, int height) {
    config_.main_window.width = width;
    config_.main_window.height = height;
    return *this;
}

Bootstrap& Bootstrap::window_resizable(bool resizable) {
    config_.main_window.resizable = resizable;
    return *this;
}

Bootstrap& Bootstrap::fullscreen(bool fs) {
    if (fs) {
        config_.main_window.initial_state = WindowState::Fullscreen;
    }
    return *this;
}

Bootstrap& Bootstrap::target_fps(double fps) {
    config_.target_fps = fps;
    return *this;
}

Bootstrap& Bootstrap::fixed_timestep(double dt) {
    config_.fixed_timestep = dt;
    return *this;
}

Bootstrap& Bootstrap::vsync(bool enabled) {
    config_.vsync = enabled;
    config_.main_window.vsync = enabled;
    return *this;
}

Bootstrap& Bootstrap::data_path(const std::filesystem::path& path) {
    config_.data_path = path;
    return *this;
}

Bootstrap& Bootstrap::startup_scene(const std::string& scene) {
    config_.startup_scene = scene;
    return *this;
}

Bootstrap& Bootstrap::startup_module(const std::string& module) {
    config_.startup_modules.push_back(module);
    return *this;
}

Bootstrap& Bootstrap::enable_debug_console(bool enable) {
    config_.enable_debug_console = enable;
    return *this;
}

Bootstrap& Bootstrap::enable_crash_handler(bool enable) {
    config_.enable_crash_handler = enable;
    return *this;
}

Bootstrap& Bootstrap::enable_hot_reload(bool enable) {
    config_.enable_hot_reload = enable;
    return *this;
}

Bootstrap& Bootstrap::on_init(std::function<void()> callback) {
    config_.on_init = std::move(callback);
    return *this;
}

Bootstrap& Bootstrap::on_shutdown(std::function<void()> callback) {
    config_.on_shutdown = std::move(callback);
    return *this;
}

Bootstrap& Bootstrap::command_line(int argc, char** argv) {
    config_.command_line_args.clear();
    for (int i = 0; i < argc; ++i) {
        config_.command_line_args.push_back(argv[i]);
    }
    parsed_args_ = false;
    return *this;
}

int Bootstrap::run() {
    // Parse command line arguments
    if (!parsed_args_) {
        parse_command_line();
    }

    // Setup default paths
    setup_default_paths();

    // Initialize and run application
    auto& app = Application::instance();

    if (!app.initialize(config_)) {
        return -1;
    }

    return app.run();
}

void Bootstrap::parse_command_line() {
    for (std::size_t i = 1; i < config_.command_line_args.size(); ++i) {
        const auto& arg = config_.command_line_args[i];

        if (arg == "--fullscreen" || arg == "-f") {
            config_.main_window.initial_state = WindowState::Fullscreen;
        } else if (arg == "--windowed" || arg == "-w") {
            config_.main_window.initial_state = WindowState::Normal;
        } else if (arg == "--vsync") {
            config_.vsync = true;
        } else if (arg == "--no-vsync") {
            config_.vsync = false;
        } else if (arg == "--debug-console") {
            config_.enable_debug_console = true;
        } else if (arg == "--no-crash-handler") {
            config_.enable_crash_handler = false;
        } else if ((arg == "--width" || arg == "-W") && i + 1 < config_.command_line_args.size()) {
            config_.main_window.width = std::stoi(config_.command_line_args[++i]);
        } else if ((arg == "--height" || arg == "-H") && i + 1 < config_.command_line_args.size()) {
            config_.main_window.height = std::stoi(config_.command_line_args[++i]);
        } else if ((arg == "--scene" || arg == "-s") && i + 1 < config_.command_line_args.size()) {
            config_.startup_scene = config_.command_line_args[++i];
        } else if ((arg == "--data" || arg == "-d") && i + 1 < config_.command_line_args.size()) {
            config_.data_path = config_.command_line_args[++i];
        }
    }

    parsed_args_ = true;
}

void Bootstrap::setup_default_paths() {
    // Paths are setup in Application::initialize
}

} // namespace void_runtime
