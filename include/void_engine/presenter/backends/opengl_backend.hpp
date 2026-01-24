#pragma once

/// @file opengl_backend.hpp
/// @brief OpenGL backend for void_presenter
///
/// Provides OpenGL/GLFW-based presentation that integrates with the
/// existing gl_renderer. Supports:
/// - Multi-window rendering
/// - VSync control
/// - Resize handling
/// - Hot-reload state preservation

#include "../backend.hpp"
#include "../surface.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations for GLFW (avoid including glfw3.h in header)
struct GLFWwindow;

namespace void_presenter {
namespace backends {

// =============================================================================
// OpenGL Swapchain
// =============================================================================

/// OpenGL swapchain using GLFW double-buffering
class OpenGLSwapchain : public ISwapchain {
public:
    /// Create swapchain for a GLFW window
    OpenGLSwapchain(GLFWwindow* window, const SwapchainConfig& config);

    ~OpenGLSwapchain();

    // Non-copyable
    OpenGLSwapchain(const OpenGLSwapchain&) = delete;
    OpenGLSwapchain& operator=(const OpenGLSwapchain&) = delete;

    [[nodiscard]] const SwapchainConfig& config() const override {
        return m_config;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override;

    bool acquire_image(std::uint64_t timeout_ns, AcquiredImage& out_image) override;

    bool present(const AcquiredImage& image) override;

    /// Get underlying GLFW window
    [[nodiscard]] GLFWwindow* glfw_window() const { return m_window; }

    /// Get frame count
    [[nodiscard]] std::uint64_t frame_count() const { return m_frame_count; }

private:
    GLFWwindow* m_window;
    SwapchainConfig m_config;
    std::uint64_t m_frame_count;
    std::uint64_t m_texture_id_counter;
    std::uint32_t m_current_image_index;
    bool m_vsync_enabled;

    mutable std::mutex m_mutex;
};

// =============================================================================
// OpenGL Surface
// =============================================================================

/// OpenGL surface wrapping a GLFW window
class OpenGLSurface : public IBackendSurface {
public:
    /// Create surface from an existing GLFW window
    /// @param window GLFW window with OpenGL context
    /// @param owns_window If true, surface will destroy window on destruction
    explicit OpenGLSurface(GLFWwindow* window, bool owns_window = false);

    /// Create surface by creating a new GLFW window
    /// @param width Window width
    /// @param height Window height
    /// @param title Window title
    /// @param share_context GLFW window to share context with (optional)
    OpenGLSurface(
        std::uint32_t width,
        std::uint32_t height,
        const std::string& title,
        GLFWwindow* share_context = nullptr);

    ~OpenGLSurface();

    // Non-copyable
    OpenGLSurface(const OpenGLSurface&) = delete;
    OpenGLSurface& operator=(const OpenGLSurface&) = delete;

    [[nodiscard]] SurfaceCapabilities capabilities() const override;

    [[nodiscard]] std::unique_ptr<ISwapchain> create_swapchain(
        const SwapchainConfig& config) override;

    [[nodiscard]] void* native_handle() const override {
        return m_window;
    }

    [[nodiscard]] bool is_valid() const override;

    /// Get underlying GLFW window
    [[nodiscard]] GLFWwindow* glfw_window() const { return m_window; }

    /// Check if this surface owns the window
    [[nodiscard]] bool owns_window() const { return m_owns_window; }

    /// Get current window size
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> get_size() const;

    /// Get framebuffer size (may differ from window size on HiDPI)
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> get_framebuffer_size() const;

private:
    void query_capabilities();

    GLFWwindow* m_window;
    bool m_owns_window;
    SurfaceCapabilities m_capabilities;
    mutable std::mutex m_mutex;
};

// =============================================================================
// OpenGL Backend
// =============================================================================

/// OpenGL backend using GLFW for window management
class OpenGLBackend : public IBackend {
public:
    /// Create OpenGL backend
    /// @param config Backend configuration
    explicit OpenGLBackend(const BackendConfig& config);

    ~OpenGLBackend();

    // Non-copyable
    OpenGLBackend(const OpenGLBackend&) = delete;
    OpenGLBackend& operator=(const OpenGLBackend&) = delete;

    [[nodiscard]] BackendType type() const override {
        return BackendType::OpenGL;
    }

    [[nodiscard]] const BackendCapabilities& capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<IBackendSurface> create_surface(
        const SurfaceTarget& target) override;

    void wait_idle() override;

    [[nodiscard]] bool is_healthy() const override {
        return m_initialized && !m_device_lost;
    }

    [[nodiscard]] std::optional<BackendError> last_error() const override {
        return m_last_error;
    }

    void set_event_callback(BackendEventCallback callback) override {
        m_event_callback = std::move(callback);
    }

    void poll_events() override;

    [[nodiscard]] void* native_device() const override {
        return nullptr;  // OpenGL doesn't have a device object
    }

    [[nodiscard]] void* native_queue() const override {
        return nullptr;  // OpenGL doesn't have a queue object
    }

    // =========================================================================
    // OpenGL-specific methods
    // =========================================================================

    /// Get OpenGL version string
    [[nodiscard]] const std::string& gl_version() const { return m_gl_version; }

    /// Get GLSL version string
    [[nodiscard]] const std::string& glsl_version() const { return m_glsl_version; }

    /// Get OpenGL vendor string
    [[nodiscard]] const std::string& gl_vendor() const { return m_gl_vendor; }

    /// Get OpenGL renderer string
    [[nodiscard]] const std::string& gl_renderer() const { return m_gl_renderer; }

    /// Check if a GLFW window has an active OpenGL context
    [[nodiscard]] static bool has_current_context();

    /// Get major OpenGL version
    [[nodiscard]] int gl_major_version() const { return m_gl_major; }

    /// Get minor OpenGL version
    [[nodiscard]] int gl_minor_version() const { return m_gl_minor; }

private:
    bool initialize(const BackendConfig& config);
    void shutdown();
    void query_capabilities();
    void query_gl_info();

    BackendCapabilities m_capabilities;
    BackendConfig m_config;

    std::string m_gl_version;
    std::string m_glsl_version;
    std::string m_gl_vendor;
    std::string m_gl_renderer;
    int m_gl_major;
    int m_gl_minor;

    std::atomic<bool> m_initialized;
    std::atomic<bool> m_device_lost;
    std::optional<BackendError> m_last_error;
    BackendEventCallback m_event_callback;

    // Hidden window for context creation when no window is provided
    GLFWwindow* m_hidden_window;

    mutable std::mutex m_mutex;
};

// =============================================================================
// OpenGL Backend Factory Registration
// =============================================================================

/// Register OpenGL backend with the factory
void register_opengl_backend();

/// Check if OpenGL is available on this platform
[[nodiscard]] bool is_opengl_available();

/// Get OpenGL version info without creating a full backend
struct OpenGLVersionInfo {
    int major = 0;
    int minor = 0;
    std::string version_string;
    std::string glsl_version;
    std::string vendor;
    std::string renderer;
    bool core_profile = false;
    bool compatibility_profile = false;
};

/// Query OpenGL version info
[[nodiscard]] std::optional<OpenGLVersionInfo> query_opengl_version();

} // namespace backends
} // namespace void_presenter
