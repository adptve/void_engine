/// @file window.hpp
/// @brief Window management for void_runtime

#pragma once

#include "fwd.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_runtime {

// =============================================================================
// Window Configuration
// =============================================================================

/// @brief Window configuration
struct WindowConfig {
    std::string title = "Void Engine";
    int width = 1280;
    int height = 720;
    int x = -1;  // -1 = centered
    int y = -1;
    int min_width = 0;
    int min_height = 0;
    int max_width = 0;
    int max_height = 0;
    bool resizable = true;
    bool decorated = true;
    bool visible = true;
    bool focused = true;
    bool floating = false;
    WindowState initial_state = WindowState::Normal;
    int monitor = 0;  // 0 = primary
    bool vsync = true;
    int swap_interval = 1;
    int samples = 0;  // MSAA samples, 0 = disabled
};

// =============================================================================
// Window Events
// =============================================================================

/// @brief Window event types
enum class WindowEventType {
    Close,
    Resize,
    Move,
    Focus,
    Blur,
    Minimize,
    Maximize,
    Restore,
    Refresh,
    ContentScale,
    Drop
};

/// @brief Window event data
struct WindowEvent {
    WindowEventType type;
    WindowId window_id;

    union {
        struct { int width, height; } resize;
        struct { int x, y; } move;
        struct { float x_scale, y_scale; } content_scale;
    } data;

    std::vector<std::string> dropped_files;  // For drop events
};

// =============================================================================
// Monitor Information
// =============================================================================

/// @brief Monitor video mode
struct VideoMode {
    int width = 0;
    int height = 0;
    int red_bits = 8;
    int green_bits = 8;
    int blue_bits = 8;
    int refresh_rate = 60;
};

/// @brief Monitor information
struct MonitorInfo {
    std::string name;
    int physical_width_mm = 0;
    int physical_height_mm = 0;
    int x = 0;
    int y = 0;
    float content_scale_x = 1.0f;
    float content_scale_y = 1.0f;
    VideoMode current_mode;
    std::vector<VideoMode> available_modes;
    bool primary = false;
};

// =============================================================================
// Window Class
// =============================================================================

/// @brief Window management class
class Window {
public:
    Window();
    ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Movable
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    // ==========================================================================
    // Creation/Destruction
    // ==========================================================================

    /// @brief Create the window
    bool create(const WindowConfig& config);

    /// @brief Destroy the window
    void destroy();

    /// @brief Check if window is valid
    bool is_valid() const;

    /// @brief Get window ID
    WindowId id() const { return id_; }

    // ==========================================================================
    // Properties
    // ==========================================================================

    /// @brief Get/Set title
    std::string title() const;
    void set_title(const std::string& title);

    /// @brief Get/Set size
    int width() const;
    int height() const;
    void get_size(int& width, int& height) const;
    void set_size(int width, int height);

    /// @brief Get framebuffer size (may differ due to DPI scaling)
    void get_framebuffer_size(int& width, int& height) const;

    /// @brief Get/Set position
    int x() const;
    int y() const;
    void get_position(int& x, int& y) const;
    void set_position(int x, int y);

    /// @brief Get/Set size limits
    void set_size_limits(int min_w, int min_h, int max_w, int max_h);

    /// @brief Get aspect ratio
    float aspect_ratio() const;

    /// @brief Set aspect ratio constraint
    void set_aspect_ratio(int numerator, int denominator);

    /// @brief Get content scale (DPI)
    void get_content_scale(float& x_scale, float& y_scale) const;

    // ==========================================================================
    // State
    // ==========================================================================

    /// @brief Get window state
    WindowState state() const { return state_; }

    /// @brief Minimize window
    void minimize();

    /// @brief Maximize window
    void maximize();

    /// @brief Restore window
    void restore();

    /// @brief Show/Hide window
    void show();
    void hide();
    bool is_visible() const;

    /// @brief Focus window
    void focus();
    bool is_focused() const;

    /// @brief Fullscreen mode
    void set_fullscreen(bool fullscreen, int monitor = 0);
    void set_fullscreen_borderless(bool borderless, int monitor = 0);
    bool is_fullscreen() const;

    /// @brief Check if should close
    bool should_close() const;
    void set_should_close(bool close);

    // ==========================================================================
    // Cursor
    // ==========================================================================

    /// @brief Set cursor mode
    void set_cursor_mode(CursorMode mode);
    CursorMode cursor_mode() const;

    /// @brief Get/Set cursor position
    void get_cursor_position(double& x, double& y) const;
    void set_cursor_position(double x, double y);

    /// @brief Set custom cursor
    void set_cursor(void* cursor_data, int width, int height, int hot_x, int hot_y);
    void set_standard_cursor(int cursor_type);
    void reset_cursor();

    // ==========================================================================
    // Platform-Specific
    // ==========================================================================

    /// @brief Get native window handle
    void* native_handle() const;

    /// @brief Make OpenGL context current (if applicable)
    void make_context_current();

    /// @brief Swap buffers
    void swap_buffers();

    // ==========================================================================
    // Events
    // ==========================================================================

    /// @brief Process pending events
    void poll_events();

    /// @brief Wait for events
    void wait_events();

    /// @brief Wait for events with timeout
    void wait_events(double timeout);

    /// @brief Set event callback
    void set_event_callback(WindowEventCallback callback);

    // ==========================================================================
    // Monitor
    // ==========================================================================

    /// @brief Get current monitor
    MonitorInfo current_monitor() const;

    /// @brief Get all monitors
    static std::vector<MonitorInfo> all_monitors();

    /// @brief Get primary monitor
    static MonitorInfo primary_monitor();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    WindowId id_ = 0;
    WindowState state_ = WindowState::Normal;
    WindowEventCallback event_callback_;

    static std::uint32_t next_id_;
};

// =============================================================================
// Window Manager
// =============================================================================

/// @brief Manages multiple windows
class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    static WindowManager& instance();

    /// @brief Create a new window
    Window* create_window(const WindowConfig& config);

    /// @brief Get window by ID
    Window* get_window(WindowId id);

    /// @brief Destroy a window
    void destroy_window(WindowId id);

    /// @brief Get all windows
    std::vector<Window*> all_windows();

    /// @brief Get window count
    std::size_t window_count() const;

    /// @brief Poll all window events
    void poll_all_events();

    /// @brief Check if any window should close
    bool any_should_close() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace void_runtime
