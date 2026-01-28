/// @file platform.hpp
/// @brief Platform abstraction layer for void_runtime
///
/// Provides a unified interface for platform-specific initialization:
/// - Window creation and management
/// - GPU backend initialization
/// - Input system setup
/// - Platform event handling
///
/// Architecture:
/// - IPlatform: Abstract interface for platform operations
/// - WindowedPlatform: Desktop windowed mode (Win32/X11/Wayland)
/// - HeadlessPlatform: No display output (server/compute)
/// - XRPlatform: OpenXR-based immersive mode (future)
/// - EditorPlatform: Editor mode with tooling (future)
///
/// The Runtime uses IPlatform to abstract platform-specific details.

#pragma once

#include <void_engine/core/error.hpp>
#include <void_engine/render/backend.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <optional>
#include <chrono>

namespace void_runtime {

// Forward declarations
struct RuntimeConfig;
class Runtime;

// =============================================================================
// Platform Events
// =============================================================================

/// Platform event types
enum class PlatformEventType : std::uint8_t {
    None = 0,
    WindowClose,
    WindowResize,
    WindowMove,
    WindowFocus,
    WindowBlur,
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    WindowRefresh,
    WindowDrop,
    ContentScaleChange,
    MonitorChange,
    KeyDown,
    KeyUp,
    KeyRepeat,
    CharInput,
    MouseMove,
    MouseButton,
    MouseScroll,
    MouseEnter,
    MouseLeave,
    GamepadConnect,
    GamepadDisconnect,
    GamepadButton,
    GamepadAxis,
    TouchBegin,
    TouchMove,
    TouchEnd,
    Quit
};

/// Platform event data
struct PlatformEvent {
    PlatformEventType type{PlatformEventType::None};
    double timestamp{0};

    union {
        struct { int width, height; } resize;
        struct { int x, y; } position;
        struct { float x_scale, y_scale; } content_scale;
        struct { int key, scancode, mods; bool repeat; } key;
        struct { std::uint32_t codepoint; } char_input;
        struct { double x, y, dx, dy; } mouse_move;
        struct { int button, action, mods; } mouse_button;
        struct { double x_offset, y_offset; } scroll;
        struct { int gamepad_id, button, action; } gamepad_button;
        struct { int gamepad_id, axis; float value; } gamepad_axis;
        struct { int touch_id; double x, y; } touch;
    } data{};

    // For drop events, file paths stored separately
    std::vector<std::string> dropped_files;

    PlatformEvent() = default;
    explicit PlatformEvent(PlatformEventType t) : type(t) {}
};

/// Platform event callback
using PlatformEventCallback = std::function<void(const PlatformEvent&)>;

// =============================================================================
// Platform Capabilities
// =============================================================================

/// Platform capability flags
struct PlatformCapabilities {
    bool has_window{false};              ///< Can create windows
    bool has_input{false};               ///< Can receive input
    bool has_gpu{false};                 ///< Has GPU for rendering
    bool has_audio{false};               ///< Has audio output
    bool has_clipboard{false};           ///< Has clipboard access
    bool has_file_dialogs{false};        ///< Can show file dialogs
    bool has_cursor_control{false};      ///< Can control cursor
    bool has_fullscreen{false};          ///< Can go fullscreen
    bool has_multi_monitor{false};       ///< Multi-monitor support
    bool has_dpi_awareness{false};       ///< DPI-aware rendering
    bool has_touch{false};               ///< Touch input support
    bool has_gamepad{false};             ///< Gamepad input support
    bool has_xr{false};                  ///< XR support available
    void_render::GpuBackend gpu_backend{void_render::GpuBackend::Null};
    void_render::DisplayBackend display_backend{void_render::DisplayBackend::Headless};
};

/// Platform information
struct PlatformInfo {
    std::string name;                    ///< Platform name (e.g., "Windows 10")
    std::string version;                 ///< Platform version
    std::string gpu_vendor;              ///< GPU vendor name
    std::string gpu_device;              ///< GPU device name
    std::string gpu_driver;              ///< GPU driver version
    std::uint32_t gpu_memory_mb{0};      ///< GPU memory in MB
    std::uint32_t cpu_cores{0};          ///< CPU core count
    std::uint64_t system_memory_mb{0};   ///< System memory in MB
    PlatformCapabilities capabilities;
};

// =============================================================================
// Window Configuration
// =============================================================================

/// Window configuration for platform creation
struct PlatformWindowConfig {
    std::string title{"void_engine"};
    std::uint32_t width{1920};
    std::uint32_t height{1080};
    std::int32_t x{-1};                  ///< -1 = centered
    std::int32_t y{-1};                  ///< -1 = centered
    bool fullscreen{false};
    bool borderless{false};
    bool resizable{true};
    bool vsync{true};
    bool visible{true};
    bool focused{true};
    bool floating{false};
    std::uint32_t min_width{0};
    std::uint32_t min_height{0};
    std::uint32_t max_width{0};
    std::uint32_t max_height{0};
    std::uint32_t samples{0};            ///< MSAA samples
    std::uint32_t monitor{0};            ///< Target monitor (0 = primary)
};

/// GPU configuration for platform creation
struct PlatformGpuConfig {
    void_render::GpuBackend preferred_backend{void_render::GpuBackend::Auto};
    void_render::BackendSelector selector{void_render::BackendSelector::Auto};
    bool enable_validation{false};       ///< GPU validation layers
    bool enable_debug_markers{false};    ///< Debug markers for profilers
    bool prefer_discrete_gpu{true};      ///< Prefer discrete over integrated
    bool require_compute{false};         ///< Require compute shader support
    bool require_ray_tracing{false};     ///< Require ray tracing support
};

// =============================================================================
// IPlatform Interface
// =============================================================================

/// Abstract platform interface
///
/// Implementations handle platform-specific initialization, window management,
/// input processing, and GPU backend setup.
class IPlatform {
public:
    virtual ~IPlatform() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Initialize the platform
    /// @param window_config Window configuration
    /// @param gpu_config GPU configuration
    /// @return Ok() on success
    [[nodiscard]] virtual void_core::Result<void> initialize(
        const PlatformWindowConfig& window_config,
        const PlatformGpuConfig& gpu_config) = 0;

    /// Shutdown the platform
    virtual void shutdown() = 0;

    /// Check if platform is initialized
    [[nodiscard]] virtual bool is_initialized() const = 0;

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    /// Poll platform events
    /// @param callback Called for each pending event
    virtual void poll_events(const PlatformEventCallback& callback) = 0;

    /// Wait for platform events
    /// @param timeout Maximum wait time (0 = indefinite)
    virtual void wait_events(std::chrono::milliseconds timeout = std::chrono::milliseconds{0}) = 0;

    /// Post a quit event to the event queue
    virtual void request_quit() = 0;

    /// Check if quit was requested
    [[nodiscard]] virtual bool should_quit() const = 0;

    // -------------------------------------------------------------------------
    // Window
    // -------------------------------------------------------------------------

    /// Get window size
    virtual void get_window_size(std::uint32_t& width, std::uint32_t& height) const = 0;

    /// Set window size
    virtual void set_window_size(std::uint32_t width, std::uint32_t height) = 0;

    /// Get framebuffer size (may differ from window due to DPI)
    virtual void get_framebuffer_size(std::uint32_t& width, std::uint32_t& height) const = 0;

    /// Get window position
    virtual void get_window_position(std::int32_t& x, std::int32_t& y) const = 0;

    /// Set window position
    virtual void set_window_position(std::int32_t x, std::int32_t y) = 0;

    /// Set window title
    virtual void set_window_title(const std::string& title) = 0;

    /// Set fullscreen mode
    virtual void set_fullscreen(bool fullscreen, std::uint32_t monitor = 0) = 0;

    /// Check if fullscreen
    [[nodiscard]] virtual bool is_fullscreen() const = 0;

    /// Minimize window
    virtual void minimize_window() = 0;

    /// Maximize window
    virtual void maximize_window() = 0;

    /// Restore window
    virtual void restore_window() = 0;

    /// Focus window
    virtual void focus_window() = 0;

    /// Check if window is focused
    [[nodiscard]] virtual bool is_window_focused() const = 0;

    /// Get content scale (DPI)
    virtual void get_content_scale(float& x_scale, float& y_scale) const = 0;

    /// Get native window handle
    [[nodiscard]] virtual void* native_window_handle() const = 0;

    // -------------------------------------------------------------------------
    // Cursor
    // -------------------------------------------------------------------------

    /// Set cursor visibility
    virtual void set_cursor_visible(bool visible) = 0;

    /// Set cursor captured (locked to window)
    virtual void set_cursor_captured(bool captured) = 0;

    /// Get cursor position
    virtual void get_cursor_position(double& x, double& y) const = 0;

    /// Set cursor position
    virtual void set_cursor_position(double x, double y) = 0;

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    /// Begin frame rendering
    virtual void begin_frame() = 0;

    /// End frame and present
    virtual void end_frame() = 0;

    /// Get current GPU backend
    [[nodiscard]] virtual void_render::GpuBackend gpu_backend() const = 0;

    /// Get native GPU context (backend-specific)
    [[nodiscard]] virtual void* native_gpu_context() const = 0;

    // -------------------------------------------------------------------------
    // Information
    // -------------------------------------------------------------------------

    /// Get platform information
    [[nodiscard]] virtual const PlatformInfo& info() const = 0;

    /// Get platform capabilities
    [[nodiscard]] virtual const PlatformCapabilities& capabilities() const = 0;

    // -------------------------------------------------------------------------
    // Clipboard (optional)
    // -------------------------------------------------------------------------

    /// Get clipboard text
    [[nodiscard]] virtual std::optional<std::string> get_clipboard_text() const { return std::nullopt; }

    /// Set clipboard text
    virtual void set_clipboard_text(const std::string& /*text*/) {}

    // -------------------------------------------------------------------------
    // Time
    // -------------------------------------------------------------------------

    /// Get high-resolution time in seconds
    [[nodiscard]] virtual double get_time() const = 0;

    /// Set time origin
    virtual void set_time(double time) = 0;
};

// =============================================================================
// Platform Factory
// =============================================================================

/// Create platform for the given runtime mode
/// @param config Runtime configuration
/// @return Platform instance or error
[[nodiscard]] std::unique_ptr<IPlatform> create_platform(const RuntimeConfig& config);

/// Get available GPU backends
[[nodiscard]] std::vector<void_render::GpuBackend> enumerate_gpu_backends();

/// Get available display backends
[[nodiscard]] std::vector<void_render::DisplayBackend> enumerate_display_backends();

/// Query platform support without creating
[[nodiscard]] PlatformCapabilities query_platform_capabilities();

} // namespace void_runtime
