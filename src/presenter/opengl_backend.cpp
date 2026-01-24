/// @file opengl_backend.cpp
/// @brief OpenGL backend implementation for void_presenter

#include <void_engine/presenter/backends/opengl_backend.hpp>
#include <void_engine/presenter/backend.hpp>

// GLFW must be included after OpenGL headers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>

namespace void_presenter {
namespace backends {

// =============================================================================
// OpenGL Swapchain Implementation
// =============================================================================

OpenGLSwapchain::OpenGLSwapchain(GLFWwindow* window, const SwapchainConfig& config)
    : m_window(window)
    , m_config(config)
    , m_frame_count(0)
    , m_texture_id_counter(0)
    , m_current_image_index(0)
    , m_vsync_enabled(config.present_mode == PresentMode::Fifo ||
                      config.present_mode == PresentMode::FifoRelaxed)
{
    if (m_window) {
        // Make context current to set swap interval
        GLFWwindow* prev_context = glfwGetCurrentContext();
        glfwMakeContextCurrent(m_window);

        // Set VSync based on present mode
        int swap_interval = 0;
        switch (config.present_mode) {
            case PresentMode::Immediate:
                swap_interval = 0;
                break;
            case PresentMode::Mailbox:
                // Mailbox is like VSync but with frame dropping - use adaptive
                swap_interval = -1;  // Adaptive VSync (if supported)
                break;
            case PresentMode::Fifo:
                swap_interval = 1;
                break;
            case PresentMode::FifoRelaxed:
                swap_interval = -1;  // Adaptive VSync
                break;
        }
        glfwSwapInterval(swap_interval);

        // Get actual framebuffer size
        int fb_width, fb_height;
        glfwGetFramebufferSize(m_window, &fb_width, &fb_height);
        m_config.width = static_cast<std::uint32_t>(fb_width);
        m_config.height = static_cast<std::uint32_t>(fb_height);

        // Restore previous context
        if (prev_context && prev_context != m_window) {
            glfwMakeContextCurrent(prev_context);
        }
    }
}

OpenGLSwapchain::~OpenGLSwapchain() {
    // Nothing to clean up - window is managed by surface
}

bool OpenGLSwapchain::resize(std::uint32_t width, std::uint32_t height) {
    std::lock_guard lock(m_mutex);

    if (width == 0 || height == 0) {
        return true;  // Minimized
    }

    m_config.width = width;
    m_config.height = height;

    // OpenGL automatically handles resize via framebuffer
    return true;
}

bool OpenGLSwapchain::acquire_image(std::uint64_t /*timeout_ns*/, AcquiredImage& out_image) {
    std::lock_guard lock(m_mutex);

    if (!m_window) {
        return false;
    }

    // Check if window should close
    if (glfwWindowShouldClose(m_window)) {
        return false;
    }

    // Make context current
    glfwMakeContextCurrent(m_window);

    // Get current framebuffer size (may have changed)
    int fb_width, fb_height;
    glfwGetFramebufferSize(m_window, &fb_width, &fb_height);

    // Check if resized
    bool resized = (static_cast<std::uint32_t>(fb_width) != m_config.width ||
                    static_cast<std::uint32_t>(fb_height) != m_config.height);

    if (fb_width > 0 && fb_height > 0) {
        m_config.width = static_cast<std::uint32_t>(fb_width);
        m_config.height = static_cast<std::uint32_t>(fb_height);
    }

    // Generate texture ID for this frame
    ++m_texture_id_counter;
    m_current_image_index = static_cast<std::uint32_t>(m_frame_count % m_config.image_count);

    // Fill out acquired image
    out_image.texture = GpuResourceHandle{m_texture_id_counter, BackendType::OpenGL};
    out_image.width = m_config.width;
    out_image.height = m_config.height;
    out_image.format = m_config.format;
    out_image.image_index = m_current_image_index;
    out_image.suboptimal = resized;  // Signal that resize happened
    out_image.native_handle = m_window;

    // Set viewport to match framebuffer
    glViewport(0, 0, fb_width, fb_height);

    return true;
}

bool OpenGLSwapchain::present(const AcquiredImage& /*image*/) {
    std::lock_guard lock(m_mutex);

    if (!m_window) {
        return false;
    }

    // Ensure context is current
    glfwMakeContextCurrent(m_window);

    // Swap buffers
    glfwSwapBuffers(m_window);

    ++m_frame_count;

    return true;
}

// =============================================================================
// OpenGL Surface Implementation
// =============================================================================

OpenGLSurface::OpenGLSurface(GLFWwindow* window, bool owns_window)
    : m_window(window)
    , m_owns_window(owns_window)
{
    query_capabilities();
}

OpenGLSurface::OpenGLSurface(
    std::uint32_t width,
    std::uint32_t height,
    const std::string& title,
    GLFWwindow* share_context)
    : m_window(nullptr)
    , m_owns_window(true)
{
    // Set window hints for OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);  // No MSAA in swapchain (handle in FBO)

    // Create window
    m_window = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        title.c_str(),
        nullptr,
        share_context
    );

    if (m_window) {
        // Make context current to initialize GLAD
        glfwMakeContextCurrent(m_window);

        // Load OpenGL functions
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    query_capabilities();
}

OpenGLSurface::~OpenGLSurface() {
    if (m_window && m_owns_window) {
        glfwDestroyWindow(m_window);
    }
}

void OpenGLSurface::query_capabilities() {
    // Standard formats for OpenGL
    m_capabilities.formats = {
        SurfaceFormat::Rgba8UnormSrgb,
        SurfaceFormat::Bgra8UnormSrgb,
        SurfaceFormat::Rgba8Unorm,
        SurfaceFormat::Bgra8Unorm,
    };

    // Check for HDR support (requires specific extensions)
    // For now, add Rgba16Float if available
    m_capabilities.formats.push_back(SurfaceFormat::Rgba16Float);

    // All present modes are "supported" through swap interval
    m_capabilities.present_modes = {
        PresentMode::Immediate,
        PresentMode::Fifo,
        PresentMode::FifoRelaxed,
        PresentMode::Mailbox,
    };

    m_capabilities.alpha_modes = {
        AlphaMode::Opaque,
        AlphaMode::PreMultiplied,
    };

    m_capabilities.min_width = 1;
    m_capabilities.min_height = 1;
    m_capabilities.max_width = 16384;
    m_capabilities.max_height = 16384;

    // Query actual limits from OpenGL if we have a context
    if (m_window) {
        GLFWwindow* prev_context = glfwGetCurrentContext();
        glfwMakeContextCurrent(m_window);

        GLint max_tex_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
        if (max_tex_size > 0) {
            m_capabilities.max_width = static_cast<std::uint32_t>(max_tex_size);
            m_capabilities.max_height = static_cast<std::uint32_t>(max_tex_size);
        }

        if (prev_context && prev_context != m_window) {
            glfwMakeContextCurrent(prev_context);
        }
    }
}

SurfaceCapabilities OpenGLSurface::capabilities() const {
    std::lock_guard lock(m_mutex);
    return m_capabilities;
}

std::unique_ptr<ISwapchain> OpenGLSurface::create_swapchain(const SwapchainConfig& config) {
    std::lock_guard lock(m_mutex);

    if (!m_window) {
        return nullptr;
    }

    return std::make_unique<OpenGLSwapchain>(m_window, config);
}

bool OpenGLSurface::is_valid() const {
    std::lock_guard lock(m_mutex);
    return m_window != nullptr && !glfwWindowShouldClose(m_window);
}

std::pair<std::uint32_t, std::uint32_t> OpenGLSurface::get_size() const {
    std::lock_guard lock(m_mutex);

    if (!m_window) {
        return {0, 0};
    }

    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

std::pair<std::uint32_t, std::uint32_t> OpenGLSurface::get_framebuffer_size() const {
    std::lock_guard lock(m_mutex);

    if (!m_window) {
        return {0, 0};
    }

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}

// =============================================================================
// OpenGL Backend Implementation
// =============================================================================

OpenGLBackend::OpenGLBackend(const BackendConfig& config)
    : m_config(config)
    , m_gl_major(0)
    , m_gl_minor(0)
    , m_initialized(false)
    , m_device_lost(false)
    , m_hidden_window(nullptr)
{
    initialize(config);
}

OpenGLBackend::~OpenGLBackend() {
    shutdown();
}

bool OpenGLBackend::initialize(const BackendConfig& config) {
    std::lock_guard lock(m_mutex);

    // Initialize GLFW if not already done
    if (!glfwInit()) {
        m_last_error = BackendError::init_failed("Failed to initialize GLFW");
        return false;
    }

    // Create a hidden window for context if needed
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    if (!m_hidden_window) {
        // Try with lower OpenGL version
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        m_hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    }

    if (!m_hidden_window) {
        m_last_error = BackendError::init_failed("Failed to create OpenGL context");
        return false;
    }

    glfwMakeContextCurrent(m_hidden_window);

    // Load OpenGL functions
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(m_hidden_window);
        m_hidden_window = nullptr;
        m_last_error = BackendError::init_failed("Failed to load OpenGL functions");
        return false;
    }

    // Query OpenGL info
    query_gl_info();
    query_capabilities();

    // Enable debug output if validation enabled
    if (config.enable_validation) {
        if (GLAD_GL_KHR_debug) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }
    }

    m_initialized = true;
    return true;
}

void OpenGLBackend::shutdown() {
    std::lock_guard lock(m_mutex);

    if (m_hidden_window) {
        glfwDestroyWindow(m_hidden_window);
        m_hidden_window = nullptr;
    }

    m_initialized = false;
}

void OpenGLBackend::query_gl_info() {
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glsl_ver = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    m_gl_version = version ? version : "Unknown";
    m_glsl_version = glsl_ver ? glsl_ver : "Unknown";
    m_gl_vendor = vendor ? vendor : "Unknown";
    m_gl_renderer = renderer ? renderer : "Unknown";

    glGetIntegerv(GL_MAJOR_VERSION, &m_gl_major);
    glGetIntegerv(GL_MINOR_VERSION, &m_gl_minor);
}

void OpenGLBackend::query_capabilities() {
    m_capabilities.type = BackendType::OpenGL;

    // Adapter info
    m_capabilities.adapter.name = m_gl_renderer;
    m_capabilities.adapter.vendor = m_gl_vendor;
    m_capabilities.adapter.driver = m_gl_version;
    m_capabilities.adapter.backend_type = BackendType::OpenGL;

    // Detect if discrete GPU based on vendor/renderer strings
    std::string renderer_lower = m_gl_renderer;
    std::transform(renderer_lower.begin(), renderer_lower.end(),
                   renderer_lower.begin(), ::tolower);

    m_capabilities.adapter.is_discrete =
        renderer_lower.find("nvidia") != std::string::npos ||
        renderer_lower.find("radeon") != std::string::npos ||
        renderer_lower.find("geforce") != std::string::npos ||
        renderer_lower.find("amd") != std::string::npos;

    m_capabilities.adapter.is_software =
        renderer_lower.find("llvmpipe") != std::string::npos ||
        renderer_lower.find("swrast") != std::string::npos ||
        renderer_lower.find("software") != std::string::npos;

    // Features
    m_capabilities.features.compute_shaders = (m_gl_major >= 4 && m_gl_minor >= 3) ||
                                               GLAD_GL_ARB_compute_shader;
    m_capabilities.features.geometry_shaders = (m_gl_major >= 3 && m_gl_minor >= 2);
    m_capabilities.features.tessellation = (m_gl_major >= 4 && m_gl_minor >= 0);
    m_capabilities.features.ray_tracing = false;  // Not in core OpenGL
    m_capabilities.features.mesh_shaders = GLAD_GL_NV_mesh_shader;
    m_capabilities.features.variable_rate_shading = false;
    m_capabilities.features.bindless = GLAD_GL_ARB_bindless_texture;
    m_capabilities.features.hdr_output = true;  // Via Rgba16Float
    m_capabilities.features.vrr = true;  // VSync control available
    m_capabilities.features.multiview = GLAD_GL_OVR_multiview;
    m_capabilities.features.foveated_rendering = false;

    // Limits
    GLint value = 0;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    m_capabilities.limits.max_texture_dimension_1d = static_cast<std::uint32_t>(value);
    m_capabilities.limits.max_texture_dimension_2d = static_cast<std::uint32_t>(value);

    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &value);
    m_capabilities.limits.max_texture_dimension_3d = static_cast<std::uint32_t>(value);

    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &value);
    m_capabilities.limits.max_texture_array_layers = static_cast<std::uint32_t>(value);

    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &value);
    m_capabilities.limits.max_bind_groups = std::min(static_cast<std::uint32_t>(value), 8u);

    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
    m_capabilities.limits.max_uniform_buffer_size = static_cast<std::uint32_t>(value);

    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value);
    m_capabilities.limits.max_storage_buffer_size = static_cast<std::uint32_t>(value);

    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &value);
    m_capabilities.limits.max_vertex_buffers = static_cast<std::uint32_t>(value);

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
    m_capabilities.limits.max_vertex_attributes = static_cast<std::uint32_t>(value);

    if (m_capabilities.features.compute_shaders) {
        GLint work_group_size[3];
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_group_size[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_group_size[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_group_size[2]);
        m_capabilities.limits.max_compute_workgroup_size_x = static_cast<std::uint32_t>(work_group_size[0]);
        m_capabilities.limits.max_compute_workgroup_size_y = static_cast<std::uint32_t>(work_group_size[1]);
        m_capabilities.limits.max_compute_workgroup_size_z = static_cast<std::uint32_t>(work_group_size[2]);

        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &value);
        m_capabilities.limits.max_compute_workgroups_per_dimension = static_cast<std::uint32_t>(value);
    }

    // Supported formats
    m_capabilities.supported_formats = {
        SurfaceFormat::Rgba8UnormSrgb,
        SurfaceFormat::Bgra8UnormSrgb,
        SurfaceFormat::Rgba8Unorm,
        SurfaceFormat::Bgra8Unorm,
        SurfaceFormat::Rgba16Float,
        SurfaceFormat::Rgb10a2Unorm,
    };

    // Supported present modes
    m_capabilities.supported_present_modes = {
        PresentMode::Immediate,
        PresentMode::Fifo,
        PresentMode::FifoRelaxed,
        PresentMode::Mailbox,
    };
}

std::unique_ptr<IBackendSurface> OpenGLBackend::create_surface(const SurfaceTarget& target) {
    std::lock_guard lock(m_mutex);

    if (!m_initialized) {
        return nullptr;
    }

    // Handle different target types
    if (std::holds_alternative<WindowHandle>(target)) {
        const auto& window_handle = std::get<WindowHandle>(target);

        // For WindowHandle, we expect the caller to provide a GLFW window
        // via the native handle pointer
        // This is a simplification - in production you'd want platform-specific
        // window wrapping

        // Check if we have an existing GLFW window in the handle
        // For now, create a new window since we don't have a GLFW window pointer
        return nullptr;  // Caller should use OpenGLSurface constructor directly
    }
    else if (std::holds_alternative<OffscreenConfig>(target)) {
        const auto& offscreen = std::get<OffscreenConfig>(target);

        // Create hidden window for offscreen rendering
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_gl_major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_gl_minor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* offscreen_window = glfwCreateWindow(
            static_cast<int>(offscreen.width),
            static_cast<int>(offscreen.height),
            "Offscreen",
            nullptr,
            m_hidden_window  // Share context
        );

        if (!offscreen_window) {
            return nullptr;
        }

        return std::make_unique<OpenGLSurface>(offscreen_window, true);
    }

    return nullptr;
}

void OpenGLBackend::wait_idle() {
    std::lock_guard lock(m_mutex);

    if (m_initialized) {
        glFinish();
    }
}

void OpenGLBackend::poll_events() {
    // GLFW event polling
    glfwPollEvents();

    // Check for OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR && m_event_callback) {
        std::string error_msg;
        switch (err) {
            case GL_INVALID_ENUM: error_msg = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: error_msg = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: error_msg = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: error_msg = "GL_OUT_OF_MEMORY"; break;
            default: error_msg = "Unknown GL error"; break;
        }
        m_event_callback("gl_error", error_msg);
    }
}

bool OpenGLBackend::has_current_context() {
    return glfwGetCurrentContext() != nullptr;
}

// =============================================================================
// Factory Registration
// =============================================================================

void register_opengl_backend() {
    BackendFactory::register_backend(BackendType::OpenGL, [](const BackendConfig& config) {
        return std::make_unique<OpenGLBackend>(config);
    });
}

bool is_opengl_available() {
    // Check if GLFW can be initialized
    if (!glfwInit()) {
        return false;
    }

    // Try to create a hidden OpenGL context
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* test_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    if (!test_window) {
        return false;
    }

    glfwMakeContextCurrent(test_window);
    bool has_gl = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
    glfwDestroyWindow(test_window);

    return has_gl;
}

std::optional<OpenGLVersionInfo> query_opengl_version() {
    if (!glfwInit()) {
        return std::nullopt;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* test_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    if (!test_window) {
        // Try lower version
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        test_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
        if (!test_window) {
            return std::nullopt;
        }
    }

    glfwMakeContextCurrent(test_window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(test_window);
        return std::nullopt;
    }

    OpenGLVersionInfo info;
    glGetIntegerv(GL_MAJOR_VERSION, &info.major);
    glGetIntegerv(GL_MINOR_VERSION, &info.minor);

    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    info.version_string = ver ? ver : "";
    info.glsl_version = glsl ? glsl : "";
    info.vendor = vendor ? vendor : "";
    info.renderer = renderer ? renderer : "";

    // Check profile
    GLint profile_mask = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
    info.core_profile = (profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) != 0;
    info.compatibility_profile = (profile_mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0;

    glfwDestroyWindow(test_window);

    return info;
}

} // namespace backends
} // namespace void_presenter
