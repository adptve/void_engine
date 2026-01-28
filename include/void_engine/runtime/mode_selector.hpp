/// @file mode_selector.hpp
/// @brief Runtime mode selection and validation
///
/// Provides comprehensive mode selection logic:
/// - Command-line argument parsing
/// - Environment variable support
/// - Manifest-based configuration
/// - Mode validation against platform capabilities
/// - Automatic fallback when requested mode unavailable
/// - Mode-specific initialization paths
///
/// Priority order for mode selection:
/// 1. Explicit CLI argument (--headless, --windowed, etc.)
/// 2. Environment variable (VOID_ENGINE_MODE)
/// 3. Manifest file specification
/// 4. Auto-detection based on system capabilities
///
/// Architecture invariant: Mode selection happens before Runtime initialization.

#pragma once

#include "runtime_config.hpp"
#include "platform.hpp"

#include <void_engine/core/error.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <functional>

namespace void_runtime {

// =============================================================================
// Mode Selection Configuration
// =============================================================================

/// Environment variable names for mode selection
struct ModeEnvironmentVars {
    static constexpr const char* MODE = "VOID_ENGINE_MODE";
    static constexpr const char* DEBUG = "VOID_ENGINE_DEBUG";
    static constexpr const char* VERBOSE = "VOID_ENGINE_VERBOSE";
    static constexpr const char* GPU_VALIDATION = "VOID_ENGINE_GPU_VALIDATION";
    static constexpr const char* HOT_RELOAD = "VOID_ENGINE_HOT_RELOAD";
    static constexpr const char* MANIFEST = "VOID_ENGINE_MANIFEST";
    static constexpr const char* API_ENDPOINT = "VOID_ENGINE_API_ENDPOINT";
    static constexpr const char* WORLD = "VOID_ENGINE_WORLD";
    static constexpr const char* WIDTH = "VOID_ENGINE_WIDTH";
    static constexpr const char* HEIGHT = "VOID_ENGINE_HEIGHT";
    static constexpr const char* FULLSCREEN = "VOID_ENGINE_FULLSCREEN";
    static constexpr const char* VSYNC = "VOID_ENGINE_VSYNC";
    static constexpr const char* TARGET_FPS = "VOID_ENGINE_TARGET_FPS";
};

/// Mode selection result
struct ModeSelectionResult {
    RuntimeMode selected_mode{RuntimeMode::Windowed};
    RuntimeMode requested_mode{RuntimeMode::Windowed};
    bool fallback_used{false};
    std::string fallback_reason;
    PlatformCapabilities capabilities;
};

/// Mode requirement flags
struct ModeRequirements {
    bool requires_window{false};
    bool requires_gpu{false};
    bool requires_input{false};
    bool requires_audio{false};
    bool requires_xr{false};
    bool requires_compute{false};
};

// =============================================================================
// Mode Selection Functions
// =============================================================================

/// Get requirements for a specific mode
[[nodiscard]] constexpr ModeRequirements get_mode_requirements(RuntimeMode mode) noexcept {
    ModeRequirements req;
    switch (mode) {
        case RuntimeMode::Headless:
            // Headless requires nothing - can run anywhere
            break;
        case RuntimeMode::Windowed:
            req.requires_window = true;
            req.requires_gpu = true;
            req.requires_input = true;
            break;
        case RuntimeMode::XR:
            req.requires_window = false;  // XR uses its own compositor
            req.requires_gpu = true;
            req.requires_xr = true;
            req.requires_input = true;
            break;
        case RuntimeMode::Editor:
            req.requires_window = true;
            req.requires_gpu = true;
            req.requires_input = true;
            break;
    }
    return req;
}

/// Check if capabilities satisfy mode requirements
[[nodiscard]] constexpr bool can_satisfy_mode(const PlatformCapabilities& caps,
                                               const ModeRequirements& req) noexcept {
    if (req.requires_window && !caps.has_window) return false;
    if (req.requires_gpu && !caps.has_gpu) return false;
    if (req.requires_input && !caps.has_input) return false;
    if (req.requires_audio && !caps.has_audio) return false;
    if (req.requires_xr && !caps.has_xr) return false;
    return true;
}

/// Parse RuntimeMode from string
[[nodiscard]] std::optional<RuntimeMode> parse_mode(std::string_view mode_str);

/// Get string name for RuntimeMode
[[nodiscard]] constexpr const char* mode_to_string(RuntimeMode mode) noexcept {
    switch (mode) {
        case RuntimeMode::Headless: return "headless";
        case RuntimeMode::Windowed: return "windowed";
        case RuntimeMode::XR: return "xr";
        case RuntimeMode::Editor: return "editor";
        default: return "unknown";
    }
}

/// Get environment variable value
[[nodiscard]] std::optional<std::string> get_env_var(const char* name);

/// Get environment variable as bool (supports: 1/0, true/false, yes/no)
[[nodiscard]] std::optional<bool> get_env_bool(const char* name);

/// Get environment variable as integer
[[nodiscard]] std::optional<int> get_env_int(const char* name);

// =============================================================================
// ModeSelector Class
// =============================================================================

/// Comprehensive mode selection and configuration
///
/// Usage:
/// ```cpp
/// ModeSelector selector;
///
/// // Set sources (in priority order - later overrides earlier)
/// selector.apply_defaults();
/// selector.apply_environment();
/// selector.apply_manifest("path/to/manifest.json");
/// selector.apply_cli(argc, argv);
///
/// // Validate and get final config
/// auto result = selector.select_mode();
/// RuntimeConfig config = selector.build_config();
/// ```
class ModeSelector {
public:
    ModeSelector() = default;

    // -------------------------------------------------------------------------
    // Configuration Sources
    // -------------------------------------------------------------------------

    /// Apply default values
    void apply_defaults();

    /// Apply values from environment variables
    void apply_environment();

    /// Apply values from manifest file
    /// @param manifest_path Path to manifest file (JSON/YAML)
    /// @return Ok() on success, Error if manifest cannot be loaded
    [[nodiscard]] void_core::Result<void> apply_manifest(const std::filesystem::path& manifest_path);

    /// Apply values from command-line arguments
    /// @param argc Argument count
    /// @param argv Argument values
    /// @return Ok() on success, Error if invalid arguments
    [[nodiscard]] void_core::Result<void> apply_cli(int argc, char* argv[]);

    // -------------------------------------------------------------------------
    // Mode Selection
    // -------------------------------------------------------------------------

    /// Select the final runtime mode with validation and fallback
    /// @return Mode selection result including any fallback info
    [[nodiscard]] ModeSelectionResult select_mode() const;

    /// Check if a specific mode is available on this platform
    [[nodiscard]] bool is_mode_available(RuntimeMode mode) const;

    /// Get all available modes on this platform
    [[nodiscard]] std::vector<RuntimeMode> available_modes() const;

    /// Get the recommended mode for this platform
    [[nodiscard]] RuntimeMode recommended_mode() const;

    // -------------------------------------------------------------------------
    // Configuration Building
    // -------------------------------------------------------------------------

    /// Build the final RuntimeConfig
    /// Uses select_mode() internally to determine mode
    [[nodiscard]] RuntimeConfig build_config() const;

    /// Get the current config state (before finalization)
    [[nodiscard]] const RuntimeConfig& current_config() const { return m_config; }

    // -------------------------------------------------------------------------
    // Help and Information
    // -------------------------------------------------------------------------

    /// Check if help was requested
    [[nodiscard]] bool help_requested() const { return m_config.show_help; }

    /// Check if version was requested
    [[nodiscard]] bool version_requested() const { return m_config.show_version; }

    /// Print usage information to stdout
    static void print_usage(const char* program_name);

    /// Print version information to stdout
    static void print_version();

    /// Print mode information to stdout
    static void print_mode_info();

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ModeChangeCallback = std::function<void(RuntimeMode from, RuntimeMode to, const std::string& reason)>;

    /// Set callback for mode fallback notification
    void on_mode_fallback(ModeChangeCallback callback) { m_on_fallback = std::move(callback); }

private:
    RuntimeConfig m_config;
    std::optional<RuntimeMode> m_explicit_mode;  // Mode set explicitly via CLI
    mutable PlatformCapabilities m_cached_caps;
    mutable bool m_caps_cached{false};
    ModeChangeCallback m_on_fallback;

    [[nodiscard]] const PlatformCapabilities& get_capabilities() const;
    [[nodiscard]] RuntimeMode find_fallback_mode(RuntimeMode requested) const;
};

// =============================================================================
// Configuration Builder (Fluent API)
// =============================================================================

/// Fluent configuration builder for programmatic setup
///
/// Usage:
/// ```cpp
/// auto config = ConfigBuilder()
///     .mode(RuntimeMode::Windowed)
///     .window_size(1920, 1080)
///     .fullscreen(false)
///     .debug(true)
///     .build();
/// ```
class ConfigBuilder {
public:
    ConfigBuilder() = default;

    /// Set runtime mode
    ConfigBuilder& mode(RuntimeMode m) { m_config.mode = m; return *this; }

    /// Set window size
    ConfigBuilder& window_size(std::uint32_t w, std::uint32_t h) {
        m_config.window_width = w;
        m_config.window_height = h;
        return *this;
    }

    /// Set window title
    ConfigBuilder& title(std::string t) { m_config.window_title = std::move(t); return *this; }

    /// Set fullscreen mode
    ConfigBuilder& fullscreen(bool fs) { m_config.fullscreen = fs; return *this; }

    /// Set vsync
    ConfigBuilder& vsync(bool v) { m_config.vsync = v; return *this; }

    /// Set target FPS (0 = unlimited)
    ConfigBuilder& target_fps(std::uint32_t fps) { m_config.target_fps = fps; return *this; }

    /// Set initial world
    ConfigBuilder& world(std::string w) { m_config.initial_world = std::move(w); return *this; }

    /// Set manifest path
    ConfigBuilder& manifest(std::string m) { m_config.manifest_path = std::move(m); return *this; }

    /// Set API endpoint
    ConfigBuilder& api_endpoint(std::string e) { m_config.api_endpoint = std::move(e); return *this; }

    /// Enable/disable debug mode
    ConfigBuilder& debug(bool d) { m_config.debug_mode = d; return *this; }

    /// Enable/disable GPU validation
    ConfigBuilder& gpu_validation(bool v) { m_config.gpu_validation = v; return *this; }

    /// Enable/disable hot-reload
    ConfigBuilder& hot_reload(bool h) { m_config.enable_hot_reload = h; return *this; }

    /// Enable/disable verbose logging
    ConfigBuilder& verbose(bool v) { m_config.verbose = v; return *this; }

    /// Set fixed timestep
    ConfigBuilder& fixed_timestep(float dt) { m_config.fixed_timestep = dt; return *this; }

    /// Set render scale
    ConfigBuilder& render_scale(float s) { m_config.render_scale = s; return *this; }

    /// Add asset search path
    ConfigBuilder& add_asset_path(std::filesystem::path p) {
        m_config.asset_paths.push_back(std::move(p));
        return *this;
    }

    /// Add plugin search path
    ConfigBuilder& add_plugin_path(std::filesystem::path p) {
        m_config.plugin_paths.push_back(std::move(p));
        return *this;
    }

    /// Build the final configuration
    [[nodiscard]] RuntimeConfig build() const { return m_config; }

private:
    RuntimeConfig m_config;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/// Quick configuration from CLI (combines all sources)
/// Priority: defaults < env < manifest (if specified) < cli
[[nodiscard]] RuntimeConfig configure_from_cli(int argc, char* argv[]);

/// Quick configuration with mode override
[[nodiscard]] RuntimeConfig configure_with_mode(RuntimeMode mode);

/// Check if running in CI/headless environment
[[nodiscard]] bool is_headless_environment();

/// Check if running in development environment
[[nodiscard]] bool is_development_environment();

} // namespace void_runtime
