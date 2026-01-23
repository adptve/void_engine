/// @file types.cpp
/// @brief Core type implementations for void_engine

#include <void_engine/engine/types.hpp>

#include <chrono>

namespace void_engine {

// =============================================================================
// EngineState
// =============================================================================

const char* to_string(EngineState state) {
    switch (state) {
        case EngineState::Created: return "Created";
        case EngineState::Initializing: return "Initializing";
        case EngineState::Ready: return "Ready";
        case EngineState::Running: return "Running";
        case EngineState::Paused: return "Paused";
        case EngineState::Stopping: return "Stopping";
        case EngineState::Terminated: return "Terminated";
        case EngineState::Error: return "Error";
    }
    return "Unknown";
}

// =============================================================================
// LifecyclePhase
// =============================================================================

const char* to_string(LifecyclePhase phase) {
    switch (phase) {
        case LifecyclePhase::PreInit: return "PreInit";
        case LifecyclePhase::CoreInit: return "CoreInit";
        case LifecyclePhase::SubsystemInit: return "SubsystemInit";
        case LifecyclePhase::AppInit: return "AppInit";
        case LifecyclePhase::Ready: return "Ready";
        case LifecyclePhase::Running: return "Running";
        case LifecyclePhase::AppShutdown: return "AppShutdown";
        case LifecyclePhase::SubsystemShutdown: return "SubsystemShutdown";
        case LifecyclePhase::CoreShutdown: return "CoreShutdown";
        case LifecyclePhase::Terminated: return "Terminated";
    }
    return "Unknown";
}

// =============================================================================
// WindowConfig
// =============================================================================

WindowConfig WindowConfig::windowed(std::uint32_t w, std::uint32_t h, const std::string& title) {
    WindowConfig config;
    config.title = title;
    config.width = w;
    config.height = h;
    config.mode = WindowMode::Windowed;
    config.resizable = true;
    return config;
}

WindowConfig WindowConfig::fullscreen(const std::string& title) {
    WindowConfig config;
    config.title = title;
    config.mode = WindowMode::Fullscreen;
    config.resizable = false;
    return config;
}

WindowConfig WindowConfig::borderless(const std::string& title) {
    WindowConfig config;
    config.title = title;
    config.mode = WindowMode::FullscreenBorderless;
    config.resizable = false;
    return config;
}

// =============================================================================
// EngineConfig
// =============================================================================

EngineConfig EngineConfig::game(const std::string& name) {
    EngineConfig config;
    config.app_name = name;
    config.features = EngineFeature::Game;
    config.window.title = name;
    config.window.width = 1280;
    config.window.height = 720;
    config.window.vsync = true;
    config.target_fps = 60;
    config.fixed_update_rate = 50;
    return config;
}

EngineConfig EngineConfig::minimal(const std::string& name) {
    EngineConfig config;
    config.app_name = name;
    config.features = EngineFeature::Minimal;
    config.window.title = name;
    config.window.width = 800;
    config.window.height = 600;
    config.target_fps = 60;
    return config;
}

EngineConfig EngineConfig::editor(const std::string& name) {
    EngineConfig config;
    config.app_name = name;
    config.features = EngineFeature::Full;
    config.window.title = name + " - Editor";
    config.window.width = 1920;
    config.window.height = 1080;
    config.window.vsync = true;
    config.enable_debug = true;
    config.enable_profiling = true;
    config.target_fps = 0;  // Unlimited
    return config;
}

// =============================================================================
// TimeState
// =============================================================================

void TimeState::reset() {
    delta_time = 0.0f;
    unscaled_delta_time = 0.0f;
    time_scale = 1.0f;
    total_time = 0.0;
    unscaled_total_time = 0.0;
    fixed_time = 0.0;
    frame_count = 0;
    fixed_frame_count = 0;

    start_time = std::chrono::steady_clock::now();
    last_frame_time = start_time;
    current_time = start_time;
}

void TimeState::update() {
    current_time = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration<float>(current_time - last_frame_time);
    unscaled_delta_time = duration.count();

    // Clamp delta time to prevent huge jumps (e.g., after breakpoint)
    if (unscaled_delta_time > 0.25f) {
        unscaled_delta_time = 0.25f;
    }

    delta_time = unscaled_delta_time * time_scale;

    total_time += static_cast<double>(delta_time);
    unscaled_total_time += static_cast<double>(unscaled_delta_time);
    fixed_time += static_cast<double>(delta_time);

    frame_count++;
    last_frame_time = current_time;
}

std::chrono::duration<double> TimeState::elapsed() const {
    return current_time - start_time;
}

bool TimeState::needs_fixed_update(float fixed_timestep) const {
    return fixed_time >= static_cast<double>(fixed_timestep);
}

void TimeState::consume_fixed_step(float fixed_timestep) {
    fixed_time -= static_cast<double>(fixed_timestep);
    fixed_frame_count++;
}

} // namespace void_engine
