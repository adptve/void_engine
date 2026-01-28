/// @file runtime_config.hpp
/// @brief Runtime configuration structures
///
/// RuntimeConfig captures all settings needed to initialize and run the engine.
/// Configuration can come from:
/// - Command line arguments
/// - Manifest files
/// - API deployment descriptors
/// - Environment variables

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace void_runtime {

// =============================================================================
// Runtime Modes
// =============================================================================

/// @brief Operating mode for the runtime
enum class RuntimeMode : std::uint8_t {
    Headless,   ///< No graphics, server/compute mode
    Windowed,   ///< Standard windowed/fullscreen graphics
    XR,         ///< XR mode (OpenXR, spatial anchors, layers)
    Editor      ///< Editor mode with tooling UI
};

/// @brief Convert RuntimeMode to string for logging
inline const char* to_string(RuntimeMode mode) {
    switch (mode) {
        case RuntimeMode::Headless: return "Headless";
        case RuntimeMode::Windowed: return "Windowed";
        case RuntimeMode::XR:       return "XR";
        case RuntimeMode::Editor:   return "Editor";
    }
    return "Unknown";
}

// =============================================================================
// Runtime Configuration
// =============================================================================

/// @brief Complete runtime configuration
///
/// This struct captures everything needed to initialize and run the engine.
/// The Runtime uses this to:
/// - Initialize the Kernel with appropriate stages
/// - Configure API connectivity
/// - Set up the appropriate rendering mode
/// - Load the initial world
struct RuntimeConfig {
    // -------------------------------------------------------------------------
    // Mode Selection
    // -------------------------------------------------------------------------

    /// Operating mode
    RuntimeMode mode{RuntimeMode::Windowed};

    // -------------------------------------------------------------------------
    // Content Loading
    // -------------------------------------------------------------------------

    /// Path to manifest file (JSON/YAML describing world, plugins, assets)
    std::string manifest_path;

    /// Initial world to load (name or path)
    std::string initial_world;

    /// API endpoint for content delivery
    std::string api_endpoint;

    /// Authentication token (if required)
    std::string auth_token;

    /// Local asset search paths (fallback when API unavailable)
    std::vector<std::filesystem::path> asset_paths;

    /// Plugin search paths
    std::vector<std::filesystem::path> plugin_paths;

    /// Widget search paths
    std::vector<std::filesystem::path> widget_paths;

    // -------------------------------------------------------------------------
    // Window Settings (Windowed/Editor modes)
    // -------------------------------------------------------------------------

    /// Window width
    std::uint32_t window_width{1920};

    /// Window height
    std::uint32_t window_height{1080};

    /// Fullscreen mode
    bool fullscreen{false};

    /// VSync enabled
    bool vsync{true};

    /// Window title
    std::string window_title{"void_engine"};

    // -------------------------------------------------------------------------
    // XR Settings (XR mode)
    // -------------------------------------------------------------------------

    /// Requested XR form factor (head-mounted, handheld, etc.)
    std::string xr_form_factor{"head_mounted"};

    /// Requested XR view configuration (stereo, mono, etc.)
    std::string xr_view_config{"stereo"};

    /// Requested XR blend mode (opaque, additive, alpha_blend)
    std::string xr_blend_mode{"opaque"};

    // -------------------------------------------------------------------------
    // Performance / Quality
    // -------------------------------------------------------------------------

    /// Target frame rate (0 = unlimited)
    std::uint32_t target_fps{0};

    /// Fixed timestep for physics (seconds)
    float fixed_timestep{1.0f / 60.0f};

    /// Maximum frame time before slowdown (seconds)
    float max_frame_time{0.25f};

    /// Render scale (1.0 = native resolution)
    float render_scale{1.0f};

    // -------------------------------------------------------------------------
    // Hot-Reload
    // -------------------------------------------------------------------------

    /// Enable hot-reload for plugins/widgets/assets
    bool enable_hot_reload{true};

    /// Hot-reload poll interval (milliseconds)
    std::uint32_t hot_reload_poll_ms{100};

    /// Hot-reload debounce time (milliseconds)
    std::uint32_t hot_reload_debounce_ms{500};

    // -------------------------------------------------------------------------
    // Debugging / Development
    // -------------------------------------------------------------------------

    /// Debug mode (extra validation, asserts, overlays)
    bool debug_mode{false};

    /// Validation mode (run validation harness)
    bool validation_mode{false};

    /// Verbose logging
    bool verbose{false};

    /// GPU validation (Vulkan validation layers, D3D12 debug layer)
    bool gpu_validation{false};

    /// Frame capture (RenderDoc integration)
    bool frame_capture{false};

    // -------------------------------------------------------------------------
    // CLI Flags (transient)
    // -------------------------------------------------------------------------

    /// Show help and exit
    bool show_help{false};

    /// Show version and exit
    bool show_version{false};
};

// =============================================================================
// Manifest Loading (future)
// =============================================================================

/// @brief Load RuntimeConfig from manifest file
/// @param path Path to manifest file (JSON/YAML)
/// @param base_config Base config to overlay manifest onto
/// @return Updated configuration
///
/// Manifest format (JSON example):
/// ```json
/// {
///   "mode": "windowed",
///   "initial_world": "worlds/main_menu",
///   "api_endpoint": "https://api.void.engine/v1",
///   "plugins": ["gameplay", "ai", "combat"],
///   "widgets": ["hud", "menu"],
///   "window": { "width": 1920, "height": 1080 }
/// }
/// ```
RuntimeConfig load_manifest(const std::filesystem::path& path,
                            const RuntimeConfig& base_config = {});

} // namespace void_runtime
