/// @file mode_selector.cpp
/// @brief Runtime mode selection and validation implementation

#include <void_engine/runtime/mode_selector.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace void_runtime {

// =============================================================================
// Environment Variable Helpers
// =============================================================================

std::optional<std::string> get_env_var(const char* name) {
#ifdef _WIN32
    char buffer[32768];
    DWORD size = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    if (size == 0 || size >= sizeof(buffer)) {
        return std::nullopt;
    }
    return std::string(buffer, size);
#else
    const char* value = std::getenv(name);
    if (!value) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

std::optional<bool> get_env_bool(const char* name) {
    auto value = get_env_var(name);
    if (!value) {
        return std::nullopt;
    }

    std::string lower = *value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "0" || lower == "false" || lower == "no" || lower == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<int> get_env_int(const char* name) {
    auto value = get_env_var(name);
    if (!value) {
        return std::nullopt;
    }

    try {
        return std::stoi(*value);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<RuntimeMode> parse_mode(std::string_view mode_str) {
    std::string lower(mode_str);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "headless" || lower == "server" || lower == "compute") {
        return RuntimeMode::Headless;
    }
    if (lower == "windowed" || lower == "window" || lower == "desktop") {
        return RuntimeMode::Windowed;
    }
    if (lower == "xr" || lower == "vr" || lower == "ar" || lower == "mr") {
        return RuntimeMode::XR;
    }
    if (lower == "editor" || lower == "edit" || lower == "dev") {
        return RuntimeMode::Editor;
    }

    return std::nullopt;
}

// =============================================================================
// ModeSelector Implementation
// =============================================================================

void ModeSelector::apply_defaults() {
    // Default configuration
    m_config.mode = RuntimeMode::Windowed;
    m_config.window_width = 1920;
    m_config.window_height = 1080;
    m_config.window_title = "void_engine";
    m_config.fullscreen = false;
    m_config.vsync = true;
    m_config.target_fps = 0;  // Unlimited
    m_config.fixed_timestep = 1.0f / 60.0f;
    m_config.max_frame_time = 0.25f;
    m_config.render_scale = 1.0f;
    m_config.enable_hot_reload = true;
    m_config.hot_reload_poll_ms = 100;
    m_config.hot_reload_debounce_ms = 500;
    m_config.debug_mode = false;
    m_config.validation_mode = false;
    m_config.verbose = false;
    m_config.gpu_validation = false;
    m_config.frame_capture = false;
    m_config.show_help = false;
    m_config.show_version = false;
}

void ModeSelector::apply_environment() {
    // Mode
    if (auto mode_str = get_env_var(ModeEnvironmentVars::MODE)) {
        if (auto mode = parse_mode(*mode_str)) {
            m_config.mode = *mode;
            spdlog::debug("Mode from environment: {}", mode_to_string(*mode));
        }
    }

    // Debug flags
    if (auto debug = get_env_bool(ModeEnvironmentVars::DEBUG)) {
        m_config.debug_mode = *debug;
    }
    if (auto verbose = get_env_bool(ModeEnvironmentVars::VERBOSE)) {
        m_config.verbose = *verbose;
    }
    if (auto gpu_val = get_env_bool(ModeEnvironmentVars::GPU_VALIDATION)) {
        m_config.gpu_validation = *gpu_val;
    }
    if (auto hot_reload = get_env_bool(ModeEnvironmentVars::HOT_RELOAD)) {
        m_config.enable_hot_reload = *hot_reload;
    }

    // Content paths
    if (auto manifest = get_env_var(ModeEnvironmentVars::MANIFEST)) {
        m_config.manifest_path = *manifest;
    }
    if (auto api = get_env_var(ModeEnvironmentVars::API_ENDPOINT)) {
        m_config.api_endpoint = *api;
    }
    if (auto world = get_env_var(ModeEnvironmentVars::WORLD)) {
        m_config.initial_world = *world;
    }

    // Window settings
    if (auto width = get_env_int(ModeEnvironmentVars::WIDTH)) {
        m_config.window_width = static_cast<std::uint32_t>(*width);
    }
    if (auto height = get_env_int(ModeEnvironmentVars::HEIGHT)) {
        m_config.window_height = static_cast<std::uint32_t>(*height);
    }
    if (auto fullscreen = get_env_bool(ModeEnvironmentVars::FULLSCREEN)) {
        m_config.fullscreen = *fullscreen;
    }
    if (auto vsync = get_env_bool(ModeEnvironmentVars::VSYNC)) {
        m_config.vsync = *vsync;
    }
    if (auto fps = get_env_int(ModeEnvironmentVars::TARGET_FPS)) {
        m_config.target_fps = static_cast<std::uint32_t>(*fps);
    }
}

void_core::Result<void> ModeSelector::apply_manifest(const std::filesystem::path& manifest_path) {
    if (!std::filesystem::exists(manifest_path)) {
        return void_core::Error("Manifest file not found: " + manifest_path.string());
    }

    // Load and parse manifest
    // For now, just set the manifest path - full parsing is done by Runtime
    m_config.manifest_path = manifest_path.string();

    spdlog::debug("Manifest path set: {}", manifest_path.string());

    // TODO: Parse manifest JSON/YAML and extract mode-related settings
    // This would use nlohmann::json or similar to parse:
    // - mode
    // - initial_world
    // - plugins
    // - window settings

    return void_core::Ok();
}

void_core::Result<void> ModeSelector::apply_cli(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        // Mode selection (explicit mode takes highest priority)
        if (arg == "--headless") {
            m_config.mode = RuntimeMode::Headless;
            m_explicit_mode = RuntimeMode::Headless;
        } else if (arg == "--windowed") {
            m_config.mode = RuntimeMode::Windowed;
            m_explicit_mode = RuntimeMode::Windowed;
        } else if (arg == "--xr" || arg == "--vr") {
            m_config.mode = RuntimeMode::XR;
            m_explicit_mode = RuntimeMode::XR;
        } else if (arg == "--editor") {
            m_config.mode = RuntimeMode::Editor;
            m_explicit_mode = RuntimeMode::Editor;
        }

        // Content paths
        else if ((arg == "--manifest" || arg == "-m") && i + 1 < args.size()) {
            m_config.manifest_path = args[++i];
        } else if ((arg == "--world" || arg == "-w") && i + 1 < args.size()) {
            m_config.initial_world = args[++i];
        } else if (arg == "--api-endpoint" && i + 1 < args.size()) {
            m_config.api_endpoint = args[++i];
        }

        // Debug / development flags
        else if (arg == "--debug" || arg == "-d") {
            m_config.debug_mode = true;
        } else if (arg == "--validate") {
            m_config.validation_mode = true;
        } else if (arg == "--gpu-validation") {
            m_config.gpu_validation = true;
        } else if (arg == "--frame-capture") {
            m_config.frame_capture = true;
        } else if (arg == "--no-hot-reload") {
            m_config.enable_hot_reload = false;
        } else if (arg == "--verbose" || arg == "-v") {
            m_config.verbose = true;
        }

        // Window settings
        else if (arg == "--width" && i + 1 < args.size()) {
            try {
                m_config.window_width = static_cast<std::uint32_t>(std::stoi(args[++i]));
            } catch (...) {
                return void_core::Error("Invalid width value");
            }
        } else if (arg == "--height" && i + 1 < args.size()) {
            try {
                m_config.window_height = static_cast<std::uint32_t>(std::stoi(args[++i]));
            } catch (...) {
                return void_core::Error("Invalid height value");
            }
        } else if (arg == "--fullscreen" || arg == "-f") {
            m_config.fullscreen = true;
        } else if (arg == "--no-vsync") {
            m_config.vsync = false;
        } else if (arg == "--title" && i + 1 < args.size()) {
            m_config.window_title = args[++i];
        } else if (arg == "--fps" && i + 1 < args.size()) {
            try {
                m_config.target_fps = static_cast<std::uint32_t>(std::stoi(args[++i]));
            } catch (...) {
                return void_core::Error("Invalid fps value");
            }
        }

        // Paths
        else if (arg == "--asset-path" && i + 1 < args.size()) {
            m_config.asset_paths.push_back(std::filesystem::path(args[++i]));
        } else if (arg == "--plugin-path" && i + 1 < args.size()) {
            m_config.plugin_paths.push_back(std::filesystem::path(args[++i]));
        }

        // Help / version
        else if (arg == "--help" || arg == "-h") {
            m_config.show_help = true;
        } else if (arg == "--version" || arg == "-V") {
            m_config.show_version = true;
        }

        // Unknown argument
        else if (arg[0] == '-') {
            spdlog::warn("Unknown argument: {}", arg);
        }
    }

    return void_core::Ok();
}

ModeSelectionResult ModeSelector::select_mode() const {
    ModeSelectionResult result;
    result.requested_mode = m_config.mode;
    result.capabilities = get_capabilities();

    // Check if requested mode is available
    auto requirements = get_mode_requirements(m_config.mode);

    if (can_satisfy_mode(result.capabilities, requirements)) {
        result.selected_mode = m_config.mode;
        result.fallback_used = false;
    } else {
        // Need to fall back
        result.selected_mode = find_fallback_mode(m_config.mode);
        result.fallback_used = true;

        // Build reason string
        std::string reason;
        if (requirements.requires_window && !result.capabilities.has_window) {
            reason = "window not available";
        } else if (requirements.requires_gpu && !result.capabilities.has_gpu) {
            reason = "GPU not available";
        } else if (requirements.requires_xr && !result.capabilities.has_xr) {
            reason = "XR runtime not available";
        } else {
            reason = "platform does not support requested mode";
        }

        result.fallback_reason = reason;

        spdlog::warn("Mode '{}' not available ({}), falling back to '{}'",
                     mode_to_string(result.requested_mode),
                     reason,
                     mode_to_string(result.selected_mode));

        // Invoke callback if set
        if (m_on_fallback) {
            m_on_fallback(result.requested_mode, result.selected_mode, reason);
        }
    }

    return result;
}

bool ModeSelector::is_mode_available(RuntimeMode mode) const {
    auto requirements = get_mode_requirements(mode);
    return can_satisfy_mode(get_capabilities(), requirements);
}

std::vector<RuntimeMode> ModeSelector::available_modes() const {
    std::vector<RuntimeMode> modes;
    const auto& caps = get_capabilities();

    // Check each mode
    for (auto mode : {RuntimeMode::Headless, RuntimeMode::Windowed,
                      RuntimeMode::XR, RuntimeMode::Editor}) {
        if (can_satisfy_mode(caps, get_mode_requirements(mode))) {
            modes.push_back(mode);
        }
    }

    return modes;
}

RuntimeMode ModeSelector::recommended_mode() const {
    const auto& caps = get_capabilities();

    // Priority order for recommendation:
    // 1. Windowed (standard desktop)
    // 2. Editor (if windowed available)
    // 3. XR (if available)
    // 4. Headless (always available)

    if (can_satisfy_mode(caps, get_mode_requirements(RuntimeMode::Windowed))) {
        return RuntimeMode::Windowed;
    }
    if (can_satisfy_mode(caps, get_mode_requirements(RuntimeMode::XR))) {
        return RuntimeMode::XR;
    }

    return RuntimeMode::Headless;
}

RuntimeConfig ModeSelector::build_config() const {
    RuntimeConfig config = m_config;

    // Apply mode selection with validation
    auto result = select_mode();
    config.mode = result.selected_mode;

    return config;
}

const PlatformCapabilities& ModeSelector::get_capabilities() const {
    if (!m_caps_cached) {
        m_cached_caps = query_platform_capabilities();
        m_caps_cached = true;
    }
    return m_cached_caps;
}

RuntimeMode ModeSelector::find_fallback_mode(RuntimeMode requested) const {
    const auto& caps = get_capabilities();

    // Fallback order depends on requested mode
    switch (requested) {
        case RuntimeMode::XR:
            // XR -> Windowed -> Headless
            if (can_satisfy_mode(caps, get_mode_requirements(RuntimeMode::Windowed))) {
                return RuntimeMode::Windowed;
            }
            return RuntimeMode::Headless;

        case RuntimeMode::Editor:
            // Editor -> Windowed -> Headless
            if (can_satisfy_mode(caps, get_mode_requirements(RuntimeMode::Windowed))) {
                return RuntimeMode::Windowed;
            }
            return RuntimeMode::Headless;

        case RuntimeMode::Windowed:
            // Windowed -> Headless
            return RuntimeMode::Headless;

        case RuntimeMode::Headless:
            // Headless is always available
            return RuntimeMode::Headless;
    }

    return RuntimeMode::Headless;
}

void ModeSelector::print_usage(const char* program_name) {
    spdlog::info("void_engine - ECS-first, hot-reloadable, XR-native game engine");
    spdlog::info("");
    spdlog::info("Usage: {} [options]", program_name);
    spdlog::info("");
    spdlog::info("Mode selection:");
    spdlog::info("  --headless          Run without graphics (server/compute mode)");
    spdlog::info("  --windowed          Run in windowed mode (default)");
    spdlog::info("  --xr, --vr          Run in XR/VR mode");
    spdlog::info("  --editor            Run in editor mode");
    spdlog::info("");
    spdlog::info("Content loading:");
    spdlog::info("  -m, --manifest <path>    Load manifest file");
    spdlog::info("  -w, --world <name>       Initial world to load");
    spdlog::info("  --api-endpoint <url>     API endpoint for content delivery");
    spdlog::info("  --asset-path <path>      Add asset search path");
    spdlog::info("  --plugin-path <path>     Add plugin search path");
    spdlog::info("");
    spdlog::info("Window settings:");
    spdlog::info("  --width <n>         Window width (default: 1920)");
    spdlog::info("  --height <n>        Window height (default: 1080)");
    spdlog::info("  -f, --fullscreen    Run in fullscreen mode");
    spdlog::info("  --no-vsync          Disable vertical sync");
    spdlog::info("  --title <text>      Window title");
    spdlog::info("  --fps <n>           Target frame rate (0 = unlimited)");
    spdlog::info("");
    spdlog::info("Development:");
    spdlog::info("  -d, --debug         Enable debug mode");
    spdlog::info("  --validate          Run validation harness");
    spdlog::info("  --gpu-validation    Enable GPU validation layers");
    spdlog::info("  --frame-capture     Enable frame capture (RenderDoc)");
    spdlog::info("  --no-hot-reload     Disable hot-reload");
    spdlog::info("  -v, --verbose       Verbose logging");
    spdlog::info("");
    spdlog::info("Other:");
    spdlog::info("  -h, --help          Show this help");
    spdlog::info("  -V, --version       Show version");
    spdlog::info("");
    spdlog::info("Environment variables:");
    spdlog::info("  VOID_ENGINE_MODE          Runtime mode (headless/windowed/xr/editor)");
    spdlog::info("  VOID_ENGINE_DEBUG         Enable debug mode (1/0)");
    spdlog::info("  VOID_ENGINE_VERBOSE       Enable verbose logging (1/0)");
    spdlog::info("  VOID_ENGINE_GPU_VALIDATION Enable GPU validation (1/0)");
    spdlog::info("  VOID_ENGINE_MANIFEST      Path to manifest file");
    spdlog::info("  VOID_ENGINE_WORLD         Initial world name");
}

void ModeSelector::print_version() {
    spdlog::info("void_engine version 0.12.0");
    spdlog::info("ECS-first, hot-reloadable, XR-native game engine");
    spdlog::info("");
    spdlog::info("Build configuration:");
#ifdef _DEBUG
    spdlog::info("  Configuration: Debug");
#else
    spdlog::info("  Configuration: Release");
#endif
#ifdef _WIN32
    spdlog::info("  Platform: Windows");
#elif defined(__linux__)
    spdlog::info("  Platform: Linux");
#elif defined(__APPLE__)
    spdlog::info("  Platform: macOS");
#endif
    spdlog::info("");
    spdlog::info("Features:");
    spdlog::info("  - ECS authoritative transforms");
    spdlog::info("  - Kernel-orchestrated hot-reload");
    spdlog::info("  - Plugin-based gameplay");
    spdlog::info("  - Reactive widget system");
    spdlog::info("  - Multi-backend rendering");
}

void ModeSelector::print_mode_info() {
    auto caps = query_platform_capabilities();

    spdlog::info("Platform capabilities:");
    spdlog::info("  Window:      {}", caps.has_window ? "yes" : "no");
    spdlog::info("  GPU:         {}", caps.has_gpu ? "yes" : "no");
    spdlog::info("  Input:       {}", caps.has_input ? "yes" : "no");
    spdlog::info("  Audio:       {}", caps.has_audio ? "yes" : "no");
    spdlog::info("  Gamepad:     {}", caps.has_gamepad ? "yes" : "no");
    spdlog::info("  XR:          {}", caps.has_xr ? "yes" : "no");
    spdlog::info("  Clipboard:   {}", caps.has_clipboard ? "yes" : "no");
    spdlog::info("  DPI-aware:   {}", caps.has_dpi_awareness ? "yes" : "no");
    spdlog::info("");

    spdlog::info("Available modes:");
    ModeSelector selector;
    for (auto mode : selector.available_modes()) {
        bool recommended = (mode == selector.recommended_mode());
        spdlog::info("  {} {}", mode_to_string(mode), recommended ? "(recommended)" : "");
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

RuntimeConfig configure_from_cli(int argc, char* argv[]) {
    ModeSelector selector;
    selector.apply_defaults();
    selector.apply_environment();

    if (auto result = selector.apply_cli(argc, argv); !result) {
        spdlog::error("Failed to parse command line: {}", result.error().message());
    }

    // Load manifest if specified
    if (!selector.current_config().manifest_path.empty()) {
        if (auto result = selector.apply_manifest(selector.current_config().manifest_path); !result) {
            spdlog::warn("Failed to load manifest: {}", result.error().message());
        }
    }

    return selector.build_config();
}

RuntimeConfig configure_with_mode(RuntimeMode mode) {
    return ConfigBuilder()
        .mode(mode)
        .build();
}

bool is_headless_environment() {
    // Check common CI environment variables
    if (get_env_var("CI")) return true;
    if (get_env_var("CONTINUOUS_INTEGRATION")) return true;
    if (get_env_var("GITHUB_ACTIONS")) return true;
    if (get_env_var("GITLAB_CI")) return true;
    if (get_env_var("JENKINS_URL")) return true;
    if (get_env_var("TRAVIS")) return true;

    // Check if DISPLAY is unset on Linux
#ifndef _WIN32
    if (!get_env_var("DISPLAY") && !get_env_var("WAYLAND_DISPLAY")) {
        return true;
    }
#endif

    // Check explicit headless mode
    if (auto mode = get_env_var(ModeEnvironmentVars::MODE)) {
        if (auto parsed = parse_mode(*mode)) {
            return *parsed == RuntimeMode::Headless;
        }
    }

    return false;
}

bool is_development_environment() {
    // Check for development indicators
    if (get_env_bool(ModeEnvironmentVars::DEBUG).value_or(false)) return true;
    if (get_env_var("VOID_ENGINE_DEV")) return true;

    // Check for common IDE environment
    if (get_env_var("VSCODE_PID")) return true;
    if (get_env_var("CLION_IDE")) return true;
    if (get_env_var("VISUAL_STUDIO_VERSION")) return true;

#ifdef _DEBUG
    return true;
#endif

    return false;
}

} // namespace void_runtime
