/// @file engine.cpp
/// @brief Main engine implementation for void_engine

#include <void_engine/engine/engine.hpp>

#include <algorithm>
#include <chrono>
#include <thread>

namespace void_engine {

// =============================================================================
// Global Engine
// =============================================================================

static Engine* g_engine = nullptr;

Engine* global_engine() {
    return g_engine;
}

void set_global_engine(Engine* engine) {
    g_engine = engine;
}

GlobalEngineGuard::GlobalEngineGuard(Engine& engine)
    : m_previous(g_engine)
{
    g_engine = &engine;
}

GlobalEngineGuard::~GlobalEngineGuard() {
    g_engine = m_previous;
}

// =============================================================================
// Engine Implementation
// =============================================================================

Engine::Engine(EngineConfig config)
    : m_config(std::move(config))
    , m_lifecycle(std::make_unique<LifecycleManager>())
    , m_config_manager(std::make_unique<ConfigManager>())
{
    // Initialize time state
    m_time.reset();

    // Initialize config manager
    m_config_manager->setup_defaults();
    m_config_manager->apply_engine_config(m_config, "project");

    // Create kernel configuration
    void_kernel::KernelConfig kernel_config;
    kernel_config.name = m_config.app_name;
    kernel_config.config_path = m_config.config_path;
    kernel_config.module_path = "modules";
    kernel_config.plugin_path = "plugins";
    kernel_config.asset_path = m_config.asset.asset_path;
    kernel_config.target_fps = m_config.target_fps;
    kernel_config.enable_hot_reload = has_feature(EngineFeature::HotReload);
    kernel_config.enable_profiling = m_config.enable_profiling;
    kernel_config.enable_validation = m_config.enable_validation;
    kernel_config.worker_thread_count = m_config.worker_threads;
    kernel_config.hot_reload_poll_interval = m_config.asset.hot_reload_poll_interval;

    m_kernel = std::make_unique<void_kernel::Kernel>(kernel_config);
}

Engine::~Engine() {
    if (m_state.load() != EngineState::Terminated) {
        shutdown_app();
        shutdown_subsystems();
        shutdown_core();
    }
}

void_core::Result<void> Engine::initialize() {
    if (m_state.load() != EngineState::Created) {
        return void_core::Error{"Engine already initialized"};
    }

    set_state(EngineState::Initializing);

    // Initialize lifecycle
    auto core_result = init_core();
    if (!core_result) {
        set_state(EngineState::Error);
        return core_result;
    }

    // Initialize subsystems
    auto subsystem_result = init_subsystems();
    if (!subsystem_result) {
        set_state(EngineState::Error);
        return subsystem_result;
    }

    set_state(EngineState::Ready);
    return void_core::Ok();
}

void_core::Result<void> Engine::initialize(std::unique_ptr<IApp> app) {
    m_app = std::move(app);

    auto result = initialize();
    if (!result) {
        return result;
    }

    // Initialize application
    auto app_result = init_app();
    if (!app_result) {
        set_state(EngineState::Error);
        return app_result;
    }

    return void_core::Ok();
}

void Engine::run() {
    if (m_state.load() != EngineState::Ready) {
        return;
    }

    // Start kernel
    auto start_result = m_kernel->start();
    if (!start_result) {
        set_state(EngineState::Error);
        return;
    }

    set_state(EngineState::Running);

    // Notify app that we're ready
    if (m_app) {
        auto ready_result = m_app->on_ready(*this);
        if (!ready_result) {
            set_state(EngineState::Error);
            return;
        }
    }

    // Transition lifecycle to running
    m_lifecycle->transition_to(LifecyclePhase::Running, *this);

    // Main loop
    while (!m_quit_requested.load() && m_state.load() == EngineState::Running) {
        run_once();
    }

    // Shutdown
    set_state(EngineState::Stopping);
    shutdown_app();
    shutdown_subsystems();
    shutdown_core();
    set_state(EngineState::Terminated);
}

void Engine::run_once() {
    begin_frame();

    // Pre-update hooks
    m_lifecycle->pre_update(*this);

    // Process input
    process_input();

    // Fixed updates (physics, simulation)
    float fixed_timestep = m_config.fixed_timestep();
    while (m_time.needs_fixed_update(fixed_timestep)) {
        fixed_update(fixed_timestep);
        m_time.consume_fixed_step(fixed_timestep);
    }

    // Regular update
    update(m_time.delta_time);

    // Late update
    late_update(m_time.delta_time);

    // Render
    render();

    // Post-update hooks
    m_lifecycle->post_update(*this);

    end_frame();

    // Frame limiting
    limit_frame_rate();
}

void Engine::request_quit() {
    if (m_app) {
        if (!m_app->on_quit_request(*this)) {
            return;  // App prevented quit
        }
    }
    m_quit_requested.store(true);
    m_kernel->request_shutdown();
}

void Engine::pause() {
    if (m_state.load() == EngineState::Running) {
        set_state(EngineState::Paused);
        m_time.time_scale = 0.0f;
    }
}

void Engine::resume() {
    if (m_state.load() == EngineState::Paused) {
        set_state(EngineState::Running);
        m_time.time_scale = 1.0f;
    }
}

void_core::Result<void> Engine::register_subsystem(std::unique_ptr<IEngineSubsystem> subsystem) {
    if (!subsystem) {
        return void_core::Error{"Cannot register null subsystem"};
    }

    std::string name = subsystem->name();
    if (m_subsystems.find(name) != m_subsystems.end()) {
        return void_core::Error{"Subsystem already registered: " + name};
    }

    m_subsystem_order.push_back(name);
    m_subsystems[name] = std::move(subsystem);

    return void_core::Ok();
}

IEngineSubsystem* Engine::get_subsystem(const std::string& name) {
    auto it = m_subsystems.find(name);
    return it != m_subsystems.end() ? it->second.get() : nullptr;
}

const IEngineSubsystem* Engine::get_subsystem(const std::string& name) const {
    auto it = m_subsystems.find(name);
    return it != m_subsystems.end() ? it->second.get() : nullptr;
}

void Engine::set_app(std::unique_ptr<IApp> app) {
    if (m_app) {
        m_app->on_shutdown(*this);
    }
    m_app = std::move(app);
    if (m_app && m_state.load() >= EngineState::Ready) {
        m_app->on_init(*this);
    }
}

EngineStats Engine::stats() const {
    EngineStats stats;

    // Frame timing
    float total_time = 0.0f;
    float min_time = std::numeric_limits<float>::max();
    float max_time = 0.0f;

    for (std::size_t i = 0; i < m_frame_time_history.size(); ++i) {
        float ft = m_frame_time_history[i];
        if (ft > 0.0f) {
            total_time += ft;
            min_time = std::min(min_time, ft);
            max_time = std::max(max_time, ft);
        }
    }

    std::size_t sample_count = std::min(m_time.frame_count, static_cast<std::uint64_t>(m_frame_time_history.size()));
    if (sample_count > 0) {
        stats.avg_frame_time_ms = (total_time / static_cast<float>(sample_count)) * 1000.0f;
        stats.fps = 1000.0f / stats.avg_frame_time_ms;
    }
    stats.min_frame_time_ms = min_time * 1000.0f;
    stats.max_frame_time_ms = max_time * 1000.0f;

    // Current frame
    stats.current_frame = m_frame_stats;

    // Counters
    stats.total_frames = m_time.frame_count;
    stats.fixed_updates = m_time.fixed_frame_count;
    stats.hot_reloads = m_kernel ? m_kernel->stats().hot_reloads : 0;

    // Uptime
    stats.uptime = m_time.elapsed();

    // Subsystem health
    for (const auto& [name, subsystem] : m_subsystems) {
        stats.subsystem_health[name] = subsystem->health();
    }

    return stats;
}

void_core::Result<void> Engine::hot_reload() {
    if (!hot_reload_enabled()) {
        return void_core::Error{"Hot-reload is not enabled"};
    }

    // Trigger kernel hot-reload
    m_kernel->hot_reload().update();

    // Hot-reload app if supported
    if (m_app && m_app->supports_hot_reload()) {
        auto snapshot_result = m_app->prepare_reload(*this);
        if (!snapshot_result) {
            return void_core::Error{snapshot_result.error().message()};
        }

        // Would reload app module here in a real implementation

        auto reload_result = m_app->complete_reload(*this, std::move(snapshot_result).value());
        if (!reload_result) {
            return reload_result;
        }
    }

    return void_core::Ok();
}

void Engine::on_state_change(std::function<void(EngineState, EngineState)> callback) {
    m_on_state_change = std::move(callback);
}

void Engine::on_focus_change(std::function<void(bool)> callback) {
    m_on_focus_change = std::move(callback);
}

void Engine::on_resize(std::function<void(std::uint32_t, std::uint32_t)> callback) {
    m_on_resize = std::move(callback);
}

void Engine::set_state(EngineState new_state) {
    EngineState old_state = m_state.exchange(new_state);
    if (m_on_state_change && old_state != new_state) {
        m_on_state_change(old_state, new_state);
    }
}

void_core::Result<void> Engine::init_core() {
    m_lifecycle->transition_to(LifecyclePhase::CoreInit, *this);

    // Initialize kernel
    auto kernel_result = m_kernel->initialize();
    if (!kernel_result) {
        return kernel_result;
    }

    return void_core::Ok();
}

void_core::Result<void> Engine::init_subsystems() {
    m_lifecycle->transition_to(LifecyclePhase::SubsystemInit, *this);

    // Initialize subsystems in registration order
    for (const auto& name : m_subsystem_order) {
        auto it = m_subsystems.find(name);
        if (it == m_subsystems.end()) continue;

        auto result = it->second->initialize(*this);
        if (!result) {
            return void_core::Error{"Failed to initialize subsystem '" + name + "': " + result.error().message()};
        }
    }

    return void_core::Ok();
}

void_core::Result<void> Engine::init_app() {
    if (!m_app) {
        return void_core::Ok();
    }

    m_lifecycle->transition_to(LifecyclePhase::AppInit, *this);

    auto result = m_app->on_init(*this);
    if (!result) {
        return result;
    }

    m_lifecycle->transition_to(LifecyclePhase::Ready, *this);
    return void_core::Ok();
}

void Engine::begin_frame() {
    m_time.update();
    m_frame_stats = FrameStats{};  // Reset frame stats
}

void Engine::process_input() {
    // Input processing would be done by input subsystem
    // For now, just update kernel
    m_kernel->update(m_time.delta_time);
}

void Engine::update(float dt) {
    auto update_start = std::chrono::steady_clock::now();

    // Update subsystems
    for (const auto& name : m_subsystem_order) {
        auto it = m_subsystems.find(name);
        if (it != m_subsystems.end()) {
            it->second->update(*this, dt);
        }
    }

    // Update application
    if (m_app && !is_paused()) {
        m_app->on_update(*this, dt);
    }

    auto update_end = std::chrono::steady_clock::now();
    m_frame_stats.update_time_ms = std::chrono::duration<float, std::milli>(update_end - update_start).count();
}

void Engine::fixed_update(float dt) {
    if (m_app && !is_paused()) {
        m_app->on_fixed_update(*this, dt);
    }
}

void Engine::late_update(float dt) {
    if (m_app && !is_paused()) {
        m_app->on_late_update(*this, dt);
    }
}

void Engine::render() {
    auto render_start = std::chrono::steady_clock::now();

    if (m_app) {
        m_app->on_render(*this);
    }

    auto render_end = std::chrono::steady_clock::now();
    m_frame_stats.render_time_ms = std::chrono::duration<float, std::milli>(render_end - render_start).count();
}

void Engine::end_frame() {
    // Record frame time
    m_frame_stats.frame_time_ms = m_time.unscaled_delta_time * 1000.0f;
    m_frame_time_history[m_frame_time_index] = m_time.unscaled_delta_time;
    m_frame_time_index = (m_frame_time_index + 1) % m_frame_time_history.size();
}

void Engine::limit_frame_rate() {
    if (m_config.target_fps == 0) {
        return;  // No frame limit
    }

    float target_frame_time = 1.0f / static_cast<float>(m_config.target_fps);
    float elapsed = m_time.unscaled_delta_time;

    if (elapsed < target_frame_time) {
        auto wait_time = std::chrono::duration<float>(target_frame_time - elapsed);
        auto wait_start = std::chrono::steady_clock::now();

        // Sleep for most of the wait time
        if (wait_time.count() > 0.002f) {
            std::this_thread::sleep_for(wait_time - std::chrono::milliseconds(2));
        }

        // Busy wait for the remainder (more accurate)
        while (std::chrono::steady_clock::now() - m_time.current_time <
               std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                   std::chrono::duration<float>(target_frame_time))) {
            // Spin
        }

        auto wait_end = std::chrono::steady_clock::now();
        m_frame_stats.wait_time_ms = std::chrono::duration<float, std::milli>(wait_end - wait_start).count();
    }
}

void Engine::shutdown_app() {
    if (!m_app) return;

    m_lifecycle->transition_to(LifecyclePhase::AppShutdown, *this);
    m_app->on_shutdown(*this);
    m_app.reset();
}

void Engine::shutdown_subsystems() {
    m_lifecycle->transition_to(LifecyclePhase::SubsystemShutdown, *this);

    // Shutdown in reverse order
    for (auto it = m_subsystem_order.rbegin(); it != m_subsystem_order.rend(); ++it) {
        auto subsystem_it = m_subsystems.find(*it);
        if (subsystem_it != m_subsystems.end()) {
            subsystem_it->second->shutdown(*this);
        }
    }

    m_subsystems.clear();
    m_subsystem_order.clear();
}

void Engine::shutdown_core() {
    m_lifecycle->transition_to(LifecyclePhase::CoreShutdown, *this);

    if (m_kernel) {
        m_kernel->shutdown();
    }

    m_lifecycle->transition_to(LifecyclePhase::Terminated, *this);
}

} // namespace void_engine
