/// @file types.hpp
/// @brief Core types for void_engine

#pragma once

#include "fwd.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>
#include <map>

namespace void_engine {

// =============================================================================
// Engine State
// =============================================================================

/// Engine runtime state
enum class EngineState : std::uint8_t {
    Created,        ///< Engine instance created
    Initializing,   ///< Subsystems initializing
    Ready,          ///< Initialization complete
    Running,        ///< Main loop active
    Paused,         ///< Paused (not updating)
    Stopping,       ///< Shutdown in progress
    Terminated,     ///< Fully shutdown
    Error,          ///< Fatal error state
};

/// Get state name
[[nodiscard]] const char* to_string(EngineState state);

// =============================================================================
// Engine Features
// =============================================================================

/// Engine feature flags (bitfield)
enum class EngineFeature : std::uint32_t {
    None            = 0,
    Rendering       = 1 << 0,   ///< Graphics rendering
    Audio           = 1 << 1,   ///< Audio system
    Physics         = 1 << 2,   ///< Physics simulation
    Input           = 1 << 3,   ///< Input handling
    Networking      = 1 << 4,   ///< Network support
    Scripting       = 1 << 5,   ///< Scripting engine
    ECS             = 1 << 6,   ///< Entity component system
    UI              = 1 << 7,   ///< UI system
    HotReload       = 1 << 8,   ///< Hot-reload support
    Profiling       = 1 << 9,   ///< Performance profiling
    Debug           = 1 << 10,  ///< Debug features
    Editor          = 1 << 11,  ///< Editor mode
    AssetHotReload  = 1 << 12,  ///< Asset hot-reload
    VR              = 1 << 13,  ///< Virtual reality
    AR              = 1 << 14,  ///< Augmented reality

    // Common combinations
    Minimal         = Rendering | Input,
    Game            = Rendering | Audio | Physics | Input | ECS | UI,
    Full            = 0xFFFFFFFF,
};

/// Bitwise operations for EngineFeature
inline EngineFeature operator|(EngineFeature a, EngineFeature b) {
    return static_cast<EngineFeature>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline EngineFeature operator&(EngineFeature a, EngineFeature b) {
    return static_cast<EngineFeature>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline EngineFeature& operator|=(EngineFeature& a, EngineFeature b) {
    a = a | b;
    return a;
}

inline EngineFeature& operator&=(EngineFeature& a, EngineFeature b) {
    a = a & b;
    return a;
}

inline bool has_feature(EngineFeature features, EngineFeature check) {
    return (features & check) == check;
}

// =============================================================================
// Lifecycle Phase
// =============================================================================

/// Engine lifecycle phase
enum class LifecyclePhase : std::uint8_t {
    PreInit,        ///< Before any initialization
    CoreInit,       ///< Core systems initializing
    SubsystemInit,  ///< Subsystems initializing
    AppInit,        ///< Application initializing
    Ready,          ///< Ready to run
    Running,        ///< Main loop active
    AppShutdown,    ///< Application shutting down
    SubsystemShutdown, ///< Subsystems shutting down
    CoreShutdown,   ///< Core systems shutting down
    Terminated,     ///< Fully terminated
};

/// Get phase name
[[nodiscard]] const char* to_string(LifecyclePhase phase);

// =============================================================================
// Window Configuration
// =============================================================================

/// Window mode
enum class WindowMode : std::uint8_t {
    Windowed,       ///< Normal window
    Borderless,     ///< Borderless window
    Fullscreen,     ///< Exclusive fullscreen
    FullscreenBorderless, ///< Borderless fullscreen (desktop resolution)
};

/// Window configuration
struct WindowConfig {
    std::string title = "void_engine";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    WindowMode mode = WindowMode::Windowed;
    bool resizable = true;
    bool vsync = true;
    bool allow_high_dpi = true;
    std::int32_t monitor_index = -1;  // -1 = primary
    std::int32_t position_x = -1;     // -1 = centered
    std::int32_t position_y = -1;

    /// Create windowed config
    [[nodiscard]] static WindowConfig windowed(
        std::uint32_t w, std::uint32_t h, const std::string& title = "void_engine");

    /// Create fullscreen config
    [[nodiscard]] static WindowConfig fullscreen(const std::string& title = "void_engine");

    /// Create borderless config
    [[nodiscard]] static WindowConfig borderless(const std::string& title = "void_engine");
};

// =============================================================================
// Render Configuration
// =============================================================================

/// Graphics backend
enum class GraphicsBackend : std::uint8_t {
    Auto,           ///< Auto-select best available
    Vulkan,         ///< Vulkan
    D3D12,          ///< DirectX 12
    D3D11,          ///< DirectX 11
    Metal,          ///< Metal (macOS/iOS)
    OpenGL,         ///< OpenGL
    WebGPU,         ///< WebGPU
};

/// Anti-aliasing mode
enum class AntiAliasing : std::uint8_t {
    None,
    FXAA,           ///< Fast approximate AA
    MSAA2x,         ///< 2x multisample
    MSAA4x,         ///< 4x multisample
    MSAA8x,         ///< 8x multisample
    TAA,            ///< Temporal AA
    SMAA,           ///< Subpixel morphological AA
};

/// Render configuration
struct RenderConfig {
    GraphicsBackend backend = GraphicsBackend::Auto;
    AntiAliasing anti_aliasing = AntiAliasing::FXAA;
    bool triple_buffering = true;
    std::uint32_t max_fps = 0;        // 0 = unlimited
    float render_scale = 1.0f;        // Resolution scale
    bool enable_shadows = true;
    bool enable_bloom = true;
    bool enable_hdr = true;
    bool enable_raytracing = false;
    std::uint32_t shadow_resolution = 2048;
    std::uint32_t max_draw_calls = 10000;
    std::uint32_t max_vertices = 10000000;
    std::uint32_t max_textures = 4096;
};

// =============================================================================
// Audio Configuration
// =============================================================================

/// Audio backend
enum class AudioBackend : std::uint8_t {
    Auto,           ///< Auto-select
    WASAPI,         ///< Windows Audio Session API
    CoreAudio,      ///< macOS Core Audio
    ALSA,           ///< Linux ALSA
    PulseAudio,     ///< Linux PulseAudio
    OpenAL,         ///< OpenAL (cross-platform)
};

/// Audio configuration
struct AudioConfig {
    AudioBackend backend = AudioBackend::Auto;
    std::uint32_t sample_rate = 48000;
    std::uint32_t buffer_size = 1024;
    std::uint32_t channels = 2;
    float master_volume = 1.0f;
    float music_volume = 0.8f;
    float sfx_volume = 1.0f;
    std::uint32_t max_sources = 64;
    bool enable_3d_audio = true;
    bool enable_reverb = true;
};

// =============================================================================
// Input Configuration
// =============================================================================

/// Input configuration
struct InputConfig {
    float mouse_sensitivity = 1.0f;
    float gamepad_deadzone = 0.15f;
    bool invert_y_axis = false;
    bool raw_mouse_input = false;
    bool enable_gamepad = true;
    bool enable_touch = false;
    std::uint32_t max_gamepads = 4;
    std::chrono::milliseconds double_click_time{500};
    std::chrono::milliseconds key_repeat_delay{300};
    std::chrono::milliseconds key_repeat_interval{50};
};

// =============================================================================
// Asset Configuration
// =============================================================================

/// Asset configuration
struct AssetConfig {
    std::string asset_path = "assets";
    std::string cache_path = "cache";
    std::string shader_path = "shaders";
    bool enable_hot_reload = true;
    bool enable_compression = true;
    bool enable_streaming = true;
    bool async_loading = true;
    std::uint32_t max_concurrent_loads = 4;
    std::size_t texture_budget_mb = 512;
    std::size_t mesh_budget_mb = 256;
    std::size_t audio_budget_mb = 128;
    std::chrono::milliseconds hot_reload_poll_interval{100};
};

// =============================================================================
// Engine Configuration
// =============================================================================

/// Complete engine configuration
struct EngineConfig {
    // Application
    std::string app_name = "void_engine_app";
    std::string app_version = "0.1.0";
    std::string organization = "void_engine";

    // Features
    EngineFeature features = EngineFeature::Game;

    // Subsystem configs
    WindowConfig window;
    RenderConfig render;
    AudioConfig audio;
    InputConfig input;
    AssetConfig asset;

    // Paths
    std::string config_path = "config";
    std::string data_path = "data";
    std::string log_path = "logs";
    std::string save_path = "saves";

    // Performance
    std::uint32_t target_fps = 60;
    std::uint32_t fixed_update_rate = 50;  // Physics rate
    std::uint32_t worker_threads = 0;      // 0 = auto

    // Debug
    bool enable_debug = false;
    bool enable_profiling = false;
    bool enable_validation = false;
    bool enable_console = false;

    /// Get fixed timestep in seconds
    [[nodiscard]] float fixed_timestep() const {
        return 1.0f / static_cast<float>(fixed_update_rate);
    }

    /// Get target frame time in seconds
    [[nodiscard]] float target_frame_time() const {
        return target_fps > 0 ? 1.0f / static_cast<float>(target_fps) : 0.0f;
    }

    /// Create default game configuration
    [[nodiscard]] static EngineConfig game(const std::string& name);

    /// Create minimal configuration (rendering + input only)
    [[nodiscard]] static EngineConfig minimal(const std::string& name);

    /// Create editor configuration
    [[nodiscard]] static EngineConfig editor(const std::string& name);
};

// =============================================================================
// Time State
// =============================================================================

/// Time state for the engine
struct TimeState {
    float delta_time = 0.0f;           ///< Time since last frame (seconds)
    float unscaled_delta_time = 0.0f;  ///< Unscaled time since last frame
    float time_scale = 1.0f;           ///< Time scale multiplier
    double total_time = 0.0;           ///< Total elapsed time (seconds)
    double unscaled_total_time = 0.0;  ///< Unscaled total time
    double fixed_time = 0.0;           ///< Fixed update time accumulator
    std::uint64_t frame_count = 0;     ///< Total frame count
    std::uint64_t fixed_frame_count = 0; ///< Fixed update count

    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_frame_time;
    std::chrono::steady_clock::time_point current_time;

    /// Reset time state
    void reset();

    /// Update time state (called each frame)
    void update();

    /// Get time since start
    [[nodiscard]] std::chrono::duration<double> elapsed() const;

    /// Check if fixed update needed
    [[nodiscard]] bool needs_fixed_update(float fixed_timestep) const;

    /// Consume fixed time step
    void consume_fixed_step(float fixed_timestep);
};

// =============================================================================
// Frame Statistics
// =============================================================================

/// Statistics for a single frame
struct FrameStats {
    float frame_time_ms = 0.0f;        ///< Total frame time
    float update_time_ms = 0.0f;       ///< Update time
    float render_time_ms = 0.0f;       ///< Render time
    float present_time_ms = 0.0f;      ///< Present/swap time
    float wait_time_ms = 0.0f;         ///< Time spent waiting for vsync/frame cap

    std::uint32_t draw_calls = 0;
    std::uint32_t triangles = 0;
    std::uint32_t vertices = 0;
    std::uint32_t state_changes = 0;
    std::uint32_t texture_binds = 0;

    std::size_t memory_usage = 0;      ///< Current memory usage (bytes)
    std::size_t gpu_memory_usage = 0;  ///< Current GPU memory usage
};

/// Engine statistics
struct EngineStats {
    // Frame timing
    float fps = 0.0f;
    float avg_frame_time_ms = 0.0f;
    float min_frame_time_ms = 0.0f;
    float max_frame_time_ms = 0.0f;
    float frame_time_variance = 0.0f;

    // Current frame
    FrameStats current_frame;

    // Counters
    std::uint64_t total_frames = 0;
    std::uint64_t fixed_updates = 0;
    std::uint64_t hot_reloads = 0;

    // Memory
    std::size_t total_memory_mb = 0;
    std::size_t gpu_memory_mb = 0;
    std::size_t asset_memory_mb = 0;

    // Uptime
    std::chrono::duration<double> uptime{0};

    // Subsystem health
    std::map<std::string, float> subsystem_health;
};

// =============================================================================
// Lifecycle Event
// =============================================================================

/// Lifecycle event data
struct LifecycleEvent {
    LifecyclePhase old_phase;
    LifecyclePhase new_phase;
    std::chrono::system_clock::time_point timestamp;
    std::string details;
};

// =============================================================================
// Config Value
// =============================================================================

/// Configuration value variant
using ConfigValue = std::variant<
    bool,
    std::int64_t,
    double,
    std::string,
    std::vector<std::string>
>;

/// Configuration value type
enum class ConfigValueType : std::uint8_t {
    Bool,
    Int,
    Float,
    String,
    StringArray,
};

} // namespace void_engine
