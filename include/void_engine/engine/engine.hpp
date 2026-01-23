/// @file engine.hpp
/// @brief Main engine facade for void_engine
///
/// The Engine class is the central orchestrator that ties together:
/// - Kernel (modules, supervisors, sandboxing)
/// - Application lifecycle
/// - Configuration management
/// - Subsystem coordination
/// - Hot-reload orchestration

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "app.hpp"
#include "lifecycle.hpp"
#include "config.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/kernel/kernel.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_engine {

// =============================================================================
// Engine Subsystem Interface
// =============================================================================

/// Interface for engine subsystems
class IEngineSubsystem {
public:
    virtual ~IEngineSubsystem() = default;

    /// Get subsystem name
    [[nodiscard]] virtual std::string name() const = 0;

    /// Initialize the subsystem
    [[nodiscard]] virtual void_core::Result<void> initialize(Engine& engine) = 0;

    /// Shutdown the subsystem
    virtual void shutdown(Engine& engine) = 0;

    /// Update the subsystem
    virtual void update(Engine& engine, float dt) {
        (void)engine;
        (void)dt;
    }

    /// Get health score (0.0 = dead, 1.0 = healthy)
    [[nodiscard]] virtual float health() const { return 1.0f; }

    /// Check if subsystem supports hot-reload
    [[nodiscard]] virtual bool supports_hot_reload() const { return false; }
};

// =============================================================================
// Engine
// =============================================================================

/// Main engine class - the central orchestrator
class Engine {
public:
    /// Create engine with configuration
    explicit Engine(EngineConfig config = {});
    ~Engine();

    // Non-copyable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /// Initialize the engine
    [[nodiscard]] void_core::Result<void> initialize();

    /// Initialize with an application
    [[nodiscard]] void_core::Result<void> initialize(std::unique_ptr<IApp> app);

    // =========================================================================
    // Main Loop
    // =========================================================================

    /// Run the main loop (blocking)
    void run();

    /// Run a single frame
    void run_once();

    /// Request engine shutdown
    void request_quit();

    /// Check if quit was requested
    [[nodiscard]] bool quit_requested() const { return m_quit_requested.load(); }

    // =========================================================================
    // State
    // =========================================================================

    /// Get current state
    [[nodiscard]] EngineState state() const { return m_state.load(); }

    /// Check if running
    [[nodiscard]] bool is_running() const { return m_state.load() == EngineState::Running; }

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const {
        auto s = m_state.load();
        return s >= EngineState::Ready && s < EngineState::Terminated;
    }

    /// Pause the engine
    void pause();

    /// Resume the engine
    void resume();

    /// Check if paused
    [[nodiscard]] bool is_paused() const { return m_state.load() == EngineState::Paused; }

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get engine configuration
    [[nodiscard]] const EngineConfig& config() const { return m_config; }

    /// Get mutable engine configuration (use with caution)
    [[nodiscard]] EngineConfig& config_mut() { return m_config; }

    /// Get config manager
    [[nodiscard]] ConfigManager& config_manager() { return *m_config_manager; }
    [[nodiscard]] const ConfigManager& config_manager() const { return *m_config_manager; }

    // =========================================================================
    // Subsystems
    // =========================================================================

    /// Get kernel
    [[nodiscard]] void_kernel::Kernel& kernel() { return *m_kernel; }
    [[nodiscard]] const void_kernel::Kernel& kernel() const { return *m_kernel; }

    /// Get lifecycle manager
    [[nodiscard]] LifecycleManager& lifecycle() { return *m_lifecycle; }
    [[nodiscard]] const LifecycleManager& lifecycle() const { return *m_lifecycle; }

    /// Register a subsystem
    void_core::Result<void> register_subsystem(std::unique_ptr<IEngineSubsystem> subsystem);

    /// Get subsystem by name
    [[nodiscard]] IEngineSubsystem* get_subsystem(const std::string& name);
    [[nodiscard]] const IEngineSubsystem* get_subsystem(const std::string& name) const;

    /// Get subsystem by type
    template<typename T>
    [[nodiscard]] T* get_subsystem() {
        for (auto& [name, subsystem] : m_subsystems) {
            if (auto* typed = dynamic_cast<T*>(subsystem.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* get_subsystem() const {
        for (const auto& [name, subsystem] : m_subsystems) {
            if (const auto* typed = dynamic_cast<const T*>(subsystem.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    // =========================================================================
    // Application
    // =========================================================================

    /// Get application (may be nullptr)
    [[nodiscard]] IApp* app() { return m_app.get(); }
    [[nodiscard]] const IApp* app() const { return m_app.get(); }

    /// Set application
    void set_app(std::unique_ptr<IApp> app);

    // =========================================================================
    // Time
    // =========================================================================

    /// Get time state
    [[nodiscard]] const TimeState& time() const { return m_time; }

    /// Get delta time
    [[nodiscard]] float delta_time() const { return m_time.delta_time; }

    /// Get unscaled delta time
    [[nodiscard]] float unscaled_delta_time() const { return m_time.unscaled_delta_time; }

    /// Get time scale
    [[nodiscard]] float time_scale() const { return m_time.time_scale; }

    /// Set time scale
    void set_time_scale(float scale) { m_time.time_scale = scale; }

    /// Get total time
    [[nodiscard]] double total_time() const { return m_time.total_time; }

    /// Get frame count
    [[nodiscard]] std::uint64_t frame_count() const { return m_time.frame_count; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get engine statistics
    [[nodiscard]] EngineStats stats() const;

    /// Get current frame statistics
    [[nodiscard]] const FrameStats& frame_stats() const { return m_frame_stats; }

    // =========================================================================
    // Features
    // =========================================================================

    /// Check if feature is enabled
    [[nodiscard]] bool has_feature(EngineFeature feature) const {
        return void_engine::has_feature(m_config.features, feature);
    }

    /// Get enabled features
    [[nodiscard]] EngineFeature features() const { return m_config.features; }

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Trigger hot-reload for registered objects
    void_core::Result<void> hot_reload();

    /// Check if hot-reload is enabled
    [[nodiscard]] bool hot_reload_enabled() const {
        return has_feature(EngineFeature::HotReload);
    }

    // =========================================================================
    // Events
    // =========================================================================

    /// Set callback for state changes
    void on_state_change(std::function<void(EngineState, EngineState)> callback);

    /// Set callback for focus changes
    void on_focus_change(std::function<void(bool)> callback);

    /// Set callback for resize
    void on_resize(std::function<void(std::uint32_t, std::uint32_t)> callback);

private:
    // State management
    void set_state(EngineState new_state);

    // Initialization phases
    void_core::Result<void> init_core();
    void_core::Result<void> init_subsystems();
    void_core::Result<void> init_app();

    // Main loop phases
    void begin_frame();
    void process_input();
    void update(float dt);
    void fixed_update(float dt);
    void late_update(float dt);
    void render();
    void end_frame();

    // Shutdown phases
    void shutdown_app();
    void shutdown_subsystems();
    void shutdown_core();

    // Frame limiting
    void limit_frame_rate();

private:
    // Configuration
    EngineConfig m_config;

    // State
    std::atomic<EngineState> m_state{EngineState::Created};
    std::atomic<bool> m_quit_requested{false};
    std::atomic<bool> m_has_focus{true};

    // Core systems
    std::unique_ptr<void_kernel::Kernel> m_kernel;
    std::unique_ptr<LifecycleManager> m_lifecycle;
    std::unique_ptr<ConfigManager> m_config_manager;

    // Application
    std::unique_ptr<IApp> m_app;

    // Subsystems
    std::unordered_map<std::string, std::unique_ptr<IEngineSubsystem>> m_subsystems;
    std::vector<std::string> m_subsystem_order;  // Initialization order

    // Time
    TimeState m_time;

    // Statistics
    FrameStats m_frame_stats;
    std::array<float, 120> m_frame_time_history{};
    std::size_t m_frame_time_index = 0;

    // Callbacks
    std::function<void(EngineState, EngineState)> m_on_state_change;
    std::function<void(bool)> m_on_focus_change;
    std::function<void(std::uint32_t, std::uint32_t)> m_on_resize;
};

// =============================================================================
// Engine Builder
// =============================================================================

/// Fluent builder for engine configuration
class EngineBuilder {
public:
    EngineBuilder() = default;

    // =========================================================================
    // Application Info
    // =========================================================================

    /// Set application name
    EngineBuilder& name(const std::string& n) {
        m_config.app_name = n;
        return *this;
    }

    /// Set application version
    EngineBuilder& version(const std::string& v) {
        m_config.app_version = v;
        return *this;
    }

    /// Set organization
    EngineBuilder& organization(const std::string& org) {
        m_config.organization = org;
        return *this;
    }

    // =========================================================================
    // Features
    // =========================================================================

    /// Set features
    EngineBuilder& features(EngineFeature f) {
        m_config.features = f;
        return *this;
    }

    /// Add feature
    EngineBuilder& with_feature(EngineFeature f) {
        m_config.features |= f;
        return *this;
    }

    /// Remove feature
    EngineBuilder& without_feature(EngineFeature f) {
        m_config.features = static_cast<EngineFeature>(
            static_cast<std::uint32_t>(m_config.features) &
            ~static_cast<std::uint32_t>(f));
        return *this;
    }

    // =========================================================================
    // Window
    // =========================================================================

    /// Set window configuration
    EngineBuilder& window(const WindowConfig& config) {
        m_config.window = config;
        return *this;
    }

    /// Set window size
    EngineBuilder& window_size(std::uint32_t width, std::uint32_t height) {
        m_config.window.width = width;
        m_config.window.height = height;
        return *this;
    }

    /// Set window title
    EngineBuilder& window_title(const std::string& title) {
        m_config.window.title = title;
        return *this;
    }

    /// Set window mode
    EngineBuilder& window_mode(WindowMode mode) {
        m_config.window.mode = mode;
        return *this;
    }

    /// Enable/disable vsync
    EngineBuilder& vsync(bool enable) {
        m_config.window.vsync = enable;
        return *this;
    }

    // =========================================================================
    // Render
    // =========================================================================

    /// Set render configuration
    EngineBuilder& render(const RenderConfig& config) {
        m_config.render = config;
        return *this;
    }

    /// Set graphics backend
    EngineBuilder& graphics_backend(GraphicsBackend backend) {
        m_config.render.backend = backend;
        return *this;
    }

    /// Set anti-aliasing
    EngineBuilder& anti_aliasing(AntiAliasing aa) {
        m_config.render.anti_aliasing = aa;
        return *this;
    }

    // =========================================================================
    // Audio
    // =========================================================================

    /// Set audio configuration
    EngineBuilder& audio(const AudioConfig& config) {
        m_config.audio = config;
        return *this;
    }

    // =========================================================================
    // Input
    // =========================================================================

    /// Set input configuration
    EngineBuilder& input(const InputConfig& config) {
        m_config.input = config;
        return *this;
    }

    // =========================================================================
    // Assets
    // =========================================================================

    /// Set asset configuration
    EngineBuilder& assets(const AssetConfig& config) {
        m_config.asset = config;
        return *this;
    }

    /// Set asset path
    EngineBuilder& asset_path(const std::string& path) {
        m_config.asset.asset_path = path;
        return *this;
    }

    // =========================================================================
    // Performance
    // =========================================================================

    /// Set target FPS
    EngineBuilder& target_fps(std::uint32_t fps) {
        m_config.target_fps = fps;
        return *this;
    }

    /// Set fixed update rate
    EngineBuilder& fixed_update_rate(std::uint32_t rate) {
        m_config.fixed_update_rate = rate;
        return *this;
    }

    /// Set worker thread count
    EngineBuilder& workers(std::uint32_t count) {
        m_config.worker_threads = count;
        return *this;
    }

    // =========================================================================
    // Debug
    // =========================================================================

    /// Enable debug mode
    EngineBuilder& debug(bool enable = true) {
        m_config.enable_debug = enable;
        return *this;
    }

    /// Enable profiling
    EngineBuilder& profiling(bool enable = true) {
        m_config.enable_profiling = enable;
        return *this;
    }

    /// Enable validation
    EngineBuilder& validation(bool enable = true) {
        m_config.enable_validation = enable;
        return *this;
    }

    // =========================================================================
    // Build
    // =========================================================================

    /// Build the engine
    [[nodiscard]] std::unique_ptr<Engine> build() {
        return std::make_unique<Engine>(m_config);
    }

    /// Build and initialize the engine
    [[nodiscard]] void_core::Result<std::unique_ptr<Engine>> build_and_init() {
        auto engine = build();
        auto result = engine->initialize();
        if (!result) {
            return void_core::Error{result.error().message()};
        }
        return engine;
    }

    /// Build, initialize, and set app
    [[nodiscard]] void_core::Result<std::unique_ptr<Engine>> build_with_app(std::unique_ptr<IApp> app) {
        auto engine = build();
        auto result = engine->initialize(std::move(app));
        if (!result) {
            return void_core::Error{result.error().message()};
        }
        return engine;
    }

    /// Get config for inspection
    [[nodiscard]] const EngineConfig& get_config() const { return m_config; }

private:
    EngineConfig m_config;
};

// =============================================================================
// Global Engine Access
// =============================================================================

/// Get the global engine instance (nullptr if not set)
[[nodiscard]] Engine* global_engine();

/// Set the global engine instance
void set_global_engine(Engine* engine);

/// RAII guard for global engine
class GlobalEngineGuard {
public:
    explicit GlobalEngineGuard(Engine& engine);
    ~GlobalEngineGuard();

    // Non-copyable, non-movable
    GlobalEngineGuard(const GlobalEngineGuard&) = delete;
    GlobalEngineGuard& operator=(const GlobalEngineGuard&) = delete;

private:
    Engine* m_previous;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/// Create and run a simple application
inline int run_app(
    const std::string& name,
    std::function<void_core::Result<void>(Engine&)> init,
    std::function<void(Engine&, float)> update,
    std::function<void(Engine&)> render,
    std::function<void(Engine&)> shutdown)
{
    auto app = make_app(name, std::move(init), std::move(update), std::move(render), std::move(shutdown));

    auto engine_result = EngineBuilder()
        .name(name)
        .build_with_app(std::move(app));

    if (!engine_result) {
        return 1;
    }

    auto engine = std::move(engine_result).value();
    GlobalEngineGuard guard(*engine);

    engine->run();
    return 0;
}

} // namespace void_engine
