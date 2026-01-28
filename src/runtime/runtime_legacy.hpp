/// @file runtime.hpp
/// @brief Main runtime system for void_runtime

#pragma once

#include "fwd.hpp"
#include "window.hpp"
#include "input.hpp"
#include "scene_loader.hpp"
#include "crash_handler.hpp"

#include <void_engine/event/event_bus.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_runtime {

// =============================================================================
// Application Configuration
// =============================================================================

/// @brief Application configuration
struct ApplicationConfig {
    // Identity
    std::string app_name = "Void Application";
    std::string app_version = "1.0.0";
    std::string organization = "Void Engine";

    // Window settings
    WindowConfig main_window;

    // Paths
    std::filesystem::path data_path;
    std::filesystem::path cache_path;
    std::filesystem::path log_path;
    std::filesystem::path config_path;

    // Runtime settings
    double target_fps = 60.0;
    double fixed_timestep = 1.0 / 60.0;
    std::size_t max_fixed_steps_per_frame = 5;
    bool vsync = true;
    bool unlimited_fps = false;

    // Features
    bool enable_debug_console = true;
    bool enable_crash_handler = true;
    bool enable_hot_reload = true;
    bool enable_profiling = false;

    // Startup
    std::string startup_scene;
    std::vector<std::string> startup_modules;
    std::vector<std::string> command_line_args;

    // Callbacks
    std::function<void()> on_init;
    std::function<void()> on_shutdown;
};

// =============================================================================
// Runtime Events
// =============================================================================

/// @brief Application started event
struct ApplicationStartedEvent {
    std::chrono::system_clock::time_point timestamp;
};

/// @brief Application stopping event
struct ApplicationStoppingEvent {
    int exit_code;
};

/// @brief Frame started event
struct FrameStartedEvent {
    std::uint64_t frame_number;
    double delta_time;
};

/// @brief Frame ended event
struct FrameEndedEvent {
    std::uint64_t frame_number;
    double frame_time;
};

/// @brief Scene loaded event
struct SceneLoadedEvent {
    std::string scene_name;
    std::filesystem::path scene_path;
};

/// @brief Module loaded event
struct ModuleLoadedEvent {
    std::string module_name;
    bool hot_reload;
};

// =============================================================================
// Runtime Statistics
// =============================================================================

/// @brief Runtime performance statistics
struct RuntimeStats {
    // Frame stats
    std::uint64_t frame_count = 0;
    double fps = 0.0;
    double frame_time_ms = 0.0;
    double min_frame_time_ms = 0.0;
    double max_frame_time_ms = 0.0;
    double avg_frame_time_ms = 0.0;

    // Update stats
    double update_time_ms = 0.0;
    double fixed_update_time_ms = 0.0;
    double render_time_ms = 0.0;
    std::uint32_t fixed_updates_this_frame = 0;

    // Memory stats
    std::size_t memory_used_bytes = 0;
    std::size_t memory_peak_bytes = 0;
    std::size_t allocations_per_frame = 0;

    // Timing
    std::chrono::steady_clock::time_point start_time;
    double uptime_seconds = 0.0;
};

// =============================================================================
// Application Class
// =============================================================================

/// @brief Main application class
class Application {
public:
    Application();
    virtual ~Application();

    // Singleton access
    static Application& instance();
    static Application* instance_ptr();

    // ==========================================================================
    // Lifecycle
    // ==========================================================================

    /// @brief Initialize the application
    bool initialize(const ApplicationConfig& config);

    /// @brief Run the main loop
    int run();

    /// @brief Request application exit
    void quit(int exit_code = 0);

    /// @brief Check if running
    bool is_running() const { return running_.load(); }

    /// @brief Check if initialized
    bool is_initialized() const { return initialized_; }

    // ==========================================================================
    // Configuration
    // ==========================================================================

    const ApplicationConfig& config() const { return config_; }
    const std::string& app_name() const { return config_.app_name; }
    const std::string& app_version() const { return config_.app_version; }

    // ==========================================================================
    // Subsystems
    // ==========================================================================

    Window* main_window() { return main_window_.get(); }
    const Window* main_window() const { return main_window_.get(); }

    InputManager& input() { return *input_manager_; }
    const InputManager& input() const { return *input_manager_; }

    SceneLoader& scene_loader() { return *scene_loader_; }
    const SceneLoader& scene_loader() const { return *scene_loader_; }

    CrashHandler& crash_handler() { return *crash_handler_; }

    // ==========================================================================
    // Frame Callbacks
    // ==========================================================================

    void set_update_callback(UpdateCallback cb) { update_callback_ = std::move(cb); }
    void set_fixed_update_callback(FixedUpdateCallback cb) { fixed_update_callback_ = std::move(cb); }
    void set_render_callback(RenderCallback cb) { render_callback_ = std::move(cb); }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    const RuntimeStats& stats() const { return stats_; }
    double delta_time() const { return delta_time_; }
    std::uint64_t frame_count() const { return frame_count_; }
    double time_since_start() const { return time_since_start_; }

    // ==========================================================================
    // Event Bus
    // ==========================================================================

    void set_event_bus(void_event::EventBus* bus) { event_bus_ = bus; }
    void_event::EventBus* event_bus() { return event_bus_; }

    // ==========================================================================
    // Paths
    // ==========================================================================

    const std::filesystem::path& data_path() const { return config_.data_path; }
    const std::filesystem::path& cache_path() const { return config_.cache_path; }
    const std::filesystem::path& log_path() const { return config_.log_path; }
    const std::filesystem::path& config_path() const { return config_.config_path; }

protected:
    // Override points for derived applications
    virtual bool on_init() { return true; }
    virtual void on_shutdown() {}
    virtual void on_update(double delta_time) { (void)delta_time; }
    virtual void on_fixed_update(double fixed_time) { (void)fixed_time; }
    virtual void on_render() {}

private:
    bool initialized_ = false;
    std::atomic<bool> running_{false};
    int exit_code_ = 0;

    ApplicationConfig config_;

    std::unique_ptr<Window> main_window_;
    std::unique_ptr<InputManager> input_manager_;
    std::unique_ptr<SceneLoader> scene_loader_;
    std::unique_ptr<CrashHandler> crash_handler_;

    void_event::EventBus* event_bus_ = nullptr;

    // Frame callbacks
    UpdateCallback update_callback_;
    FixedUpdateCallback fixed_update_callback_;
    RenderCallback render_callback_;

    // Timing
    double delta_time_ = 0.0;
    double accumulator_ = 0.0;
    double time_since_start_ = 0.0;
    std::uint64_t frame_count_ = 0;
    std::chrono::steady_clock::time_point last_frame_time_;
    std::chrono::steady_clock::time_point start_time_;

    RuntimeStats stats_;

    // Internal methods
    void main_loop();
    void process_frame();
    void update_stats(double frame_time);
    void setup_paths();
    void load_startup_content();
    void emit_event(const auto& event);
};

// =============================================================================
// Bootstrap System
// =============================================================================

/// @brief Bootstrap configuration and entry point
class Bootstrap {
public:
    Bootstrap();
    ~Bootstrap();

    // ==========================================================================
    // Configuration
    // ==========================================================================

    Bootstrap& app_name(const std::string& name);
    Bootstrap& app_version(const std::string& version);
    Bootstrap& organization(const std::string& org);

    Bootstrap& window_title(const std::string& title);
    Bootstrap& window_size(int width, int height);
    Bootstrap& window_resizable(bool resizable);
    Bootstrap& fullscreen(bool fs);

    Bootstrap& target_fps(double fps);
    Bootstrap& fixed_timestep(double dt);
    Bootstrap& vsync(bool enabled);

    Bootstrap& data_path(const std::filesystem::path& path);
    Bootstrap& startup_scene(const std::string& scene);
    Bootstrap& startup_module(const std::string& module);

    Bootstrap& enable_debug_console(bool enable);
    Bootstrap& enable_crash_handler(bool enable);
    Bootstrap& enable_hot_reload(bool enable);

    Bootstrap& on_init(std::function<void()> callback);
    Bootstrap& on_shutdown(std::function<void()> callback);

    Bootstrap& command_line(int argc, char** argv);

    // ==========================================================================
    // Run
    // ==========================================================================

    /// @brief Build and run the application
    int run();

    /// @brief Get the configuration
    const ApplicationConfig& config() const { return config_; }

private:
    ApplicationConfig config_;
    bool parsed_args_ = false;

    void parse_command_line();
    void setup_default_paths();
};

// =============================================================================
// Global Access Functions
// =============================================================================

/// @brief Get global application instance
inline Application& app() {
    return Application::instance();
}

/// @brief Get delta time
inline double delta_time() {
    return Application::instance().delta_time();
}

/// @brief Get frame count
inline std::uint64_t frame_count() {
    return Application::instance().frame_count();
}

/// @brief Get time since start
inline double time_since_start() {
    return Application::instance().time_since_start();
}

/// @brief Request application quit
inline void quit(int exit_code = 0) {
    Application::instance().quit(exit_code);
}

} // namespace void_runtime
