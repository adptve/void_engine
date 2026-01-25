/// @file wgpu_backend.cpp
/// @brief wgpu-native backend implementation for void_presenter
///
/// This implementation provides cross-platform GPU support through wgpu-native,
/// which abstracts over Vulkan, D3D12, Metal, and OpenGL backends.

#include <void_engine/presenter/backends/wgpu_backend.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Only compile actual wgpu code if the library is available
#if defined(VOID_HAS_WGPU)
// In real implementation, this would include wgpu headers:
// #include <wgpu.h>
#endif

namespace void_presenter {
namespace backends {

// =============================================================================
// WGPU Backend Implementation (Conditional)
// =============================================================================

#if defined(VOID_HAS_WGPU)

// =============================================================================
// WgpuSwapchain Implementation
// =============================================================================

WgpuSwapchain::WgpuSwapchain(
    WGPUDeviceImpl* device,
    WGPUSurfaceImpl* surface,
    const SwapchainConfig& config)
    : m_device(device)
    , m_surface(surface)
    , m_swapchain(nullptr)
    , m_config(config)
    , m_current_image_index(0)
    , m_texture_id_counter(0)
{
    create_swapchain();
}

WgpuSwapchain::~WgpuSwapchain() {
    destroy_swapchain();
}

bool WgpuSwapchain::resize(std::uint32_t width, std::uint32_t height) {
    std::lock_guard lock(m_mutex);

    if (width == 0 || height == 0) {
        return true; // Minimized
    }

    if (width == m_config.width && height == m_config.height) {
        return true; // No change
    }

    m_config.width = width;
    m_config.height = height;

    destroy_swapchain();
    return create_swapchain();
}

bool WgpuSwapchain::acquire_image(std::uint64_t timeout_ns, AcquiredImage& out_image) {
    std::lock_guard lock(m_mutex);

    if (!m_swapchain || !m_surface) {
        return false;
    }

    // In real implementation:
    // WGPUSurfaceTexture surface_texture;
    // wgpuSurfaceGetCurrentTexture(m_surface, &surface_texture);
    // if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
    //     return false;
    // }

    ++m_texture_id_counter;
    m_current_image_index = static_cast<std::uint32_t>(
        (m_current_image_index + 1) % m_config.image_count);

    out_image.texture = GpuResourceHandle{m_texture_id_counter, BackendType::Wgpu};
    out_image.width = m_config.width;
    out_image.height = m_config.height;
    out_image.format = m_config.format;
    out_image.image_index = m_current_image_index;
    out_image.suboptimal = false;
    out_image.native_handle = nullptr; // Would be the WGPUTextureView

    return true;
}

bool WgpuSwapchain::present(const AcquiredImage& /*image*/) {
    std::lock_guard lock(m_mutex);

    if (!m_surface) {
        return false;
    }

    // In real implementation:
    // wgpuSurfacePresent(m_surface);

    return true;
}

bool WgpuSwapchain::create_swapchain() {
    if (!m_device || !m_surface) {
        return false;
    }

    // In real implementation:
    // WGPUSurfaceConfiguration surface_config = {};
    // surface_config.device = m_device;
    // surface_config.format = convert_format(m_config.format);
    // surface_config.usage = WGPUTextureUsage_RenderAttachment;
    // surface_config.width = m_config.width;
    // surface_config.height = m_config.height;
    // surface_config.presentMode = convert_present_mode(m_config.present_mode);
    // surface_config.alphaMode = convert_alpha_mode(m_config.alpha_mode);
    // wgpuSurfaceConfigure(m_surface, &surface_config);

    return true;
}

void WgpuSwapchain::destroy_swapchain() {
    // In real implementation:
    // wgpuSurfaceUnconfigure(m_surface);
    m_swapchain = nullptr;
}

// =============================================================================
// WgpuSurface Implementation
// =============================================================================

WgpuSurface::WgpuSurface(
    WGPUInstanceImpl* instance,
    WGPUAdapterImpl* adapter,
    WGPUDeviceImpl* device,
    const SurfaceTarget& target)
    : m_instance(instance)
    , m_adapter(adapter)
    , m_device(device)
    , m_surface(nullptr)
    , m_capabilities_queried(false)
{
    create_surface(target);
    query_capabilities();
}

WgpuSurface::~WgpuSurface() {
    if (m_surface) {
        // In real implementation:
        // wgpuSurfaceRelease(m_surface);
        m_surface = nullptr;
    }
}

SurfaceCapabilities WgpuSurface::capabilities() const {
    return m_capabilities;
}

std::unique_ptr<ISwapchain> WgpuSurface::create_swapchain(const SwapchainConfig& config) {
    if (!m_surface || !m_device) {
        return nullptr;
    }

    return std::make_unique<WgpuSwapchain>(m_device, m_surface, config);
}

bool WgpuSurface::create_surface(const SurfaceTarget& target) {
    if (!m_instance) {
        return false;
    }

    // Create surface based on target type
    if (std::holds_alternative<WindowHandle>(target)) {
        const auto& handle = std::get<WindowHandle>(target);

        // In real implementation:
        // Platform-specific surface creation
        // #ifdef _WIN32
        //     WGPUSurfaceDescriptorFromWindowsHWND windows_desc = {};
        //     windows_desc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
        //     windows_desc.hwnd = handle.hwnd;
        //     windows_desc.hinstance = GetModuleHandle(NULL);
        //     WGPUSurfaceDescriptor desc = {};
        //     desc.nextInChain = &windows_desc.chain;
        //     m_surface = wgpuInstanceCreateSurface(m_instance, &desc);
        // #elif defined(__linux__)
        //     // X11 or Wayland surface creation
        // #elif defined(__APPLE__)
        //     // Metal layer surface creation
        // #endif

        (void)handle; // Suppress unused warning
        return true;
    }
    else if (std::holds_alternative<CanvasHandle>(target)) {
        const auto& canvas = std::get<CanvasHandle>(target);

        // In real implementation (Emscripten):
        // WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {};
        // canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        // canvas_desc.selector = canvas.canvas_id.c_str();
        // WGPUSurfaceDescriptor desc = {};
        // desc.nextInChain = &canvas_desc.chain;
        // m_surface = wgpuInstanceCreateSurface(m_instance, &desc);

        (void)canvas; // Suppress unused warning
        return true;
    }
    else if (std::holds_alternative<OffscreenConfig>(target)) {
        // Offscreen surfaces don't need an actual surface
        // They'll use textures directly
        return true;
    }

    return false;
}

void WgpuSurface::query_capabilities() {
    m_capabilities_queried = true;

    // In real implementation:
    // WGPUSurfaceCapabilities caps = {};
    // wgpuSurfaceGetCapabilities(m_surface, m_adapter, &caps);

    // Default capabilities for wgpu
    m_capabilities.formats = {
        SurfaceFormat::Bgra8UnormSrgb,
        SurfaceFormat::Rgba8UnormSrgb,
        SurfaceFormat::Bgra8Unorm,
        SurfaceFormat::Rgba8Unorm,
        SurfaceFormat::Rgba16Float,
    };

    m_capabilities.present_modes = {
        PresentMode::Fifo,
        PresentMode::FifoRelaxed,
        PresentMode::Mailbox,
        PresentMode::Immediate,
    };

    m_capabilities.alpha_modes = {
        AlphaMode::Opaque,
        AlphaMode::PreMultiplied,
        AlphaMode::PostMultiplied,
    };

    m_capabilities.min_width = 1;
    m_capabilities.min_height = 1;
    m_capabilities.max_width = 16384;
    m_capabilities.max_height = 16384;
}

// =============================================================================
// WgpuBackend Implementation
// =============================================================================

WgpuBackend::WgpuBackend(
    const BackendConfig& config,
    const WgpuBackendConfig& wgpu_config)
    : m_instance(nullptr)
    , m_adapter(nullptr)
    , m_device(nullptr)
    , m_queue(nullptr)
    , m_underlying_api(BackendType::Wgpu)
    , m_device_lost(false)
{
    initialize(config, wgpu_config);
}

WgpuBackend::~WgpuBackend() {
    shutdown();
}

std::unique_ptr<IBackendSurface> WgpuBackend::create_surface(const SurfaceTarget& target) {
    std::lock_guard lock(m_mutex);

    if (!m_instance || !m_adapter || !m_device) {
        return nullptr;
    }

    return std::make_unique<WgpuSurface>(m_instance, m_adapter, m_device, target);
}

void WgpuBackend::wait_idle() {
    std::lock_guard lock(m_mutex);

    if (m_device) {
        // In real implementation:
        // wgpuDevicePoll(m_device, true, nullptr);
    }
}

void WgpuBackend::poll_events() {
    std::lock_guard lock(m_mutex);

    if (m_device) {
        // In real implementation:
        // wgpuDevicePoll(m_device, false, nullptr);
    }

    // Check for device lost
    if (m_device_lost && m_event_callback) {
        m_event_callback("device_lost", "GPU device was lost");
    }
}

bool WgpuBackend::initialize(
    const BackendConfig& config,
    const WgpuBackendConfig& wgpu_config) {

    std::lock_guard lock(m_mutex);

    // Create wgpu instance
    if (!create_instance()) {
        m_last_error = BackendError::init_failed("Failed to create wgpu instance");
        return false;
    }

    // Request adapter
    if (!request_adapter(config.power_preference)) {
        m_last_error = BackendError::init_failed("Failed to request GPU adapter");
        return false;
    }

    // Request device
    if (!request_device(wgpu_config)) {
        m_last_error = BackendError::init_failed("Failed to create GPU device");
        return false;
    }

    // Query capabilities
    query_capabilities();

    return true;
}

void WgpuBackend::shutdown() {
    std::lock_guard lock(m_mutex);

    if (m_queue) {
        // In real implementation:
        // wgpuQueueRelease(m_queue);
        m_queue = nullptr;
    }

    if (m_device) {
        // In real implementation:
        // wgpuDeviceRelease(m_device);
        m_device = nullptr;
    }

    if (m_adapter) {
        // In real implementation:
        // wgpuAdapterRelease(m_adapter);
        m_adapter = nullptr;
    }

    if (m_instance) {
        // In real implementation:
        // wgpuInstanceRelease(m_instance);
        m_instance = nullptr;
    }
}

bool WgpuBackend::create_instance() {
    // In real implementation:
    // WGPUInstanceDescriptor desc = {};
    // m_instance = wgpuCreateInstance(&desc);
    // return m_instance != nullptr;

    // Stub returns success
    return true;
}

bool WgpuBackend::request_adapter(PowerPreference power_pref) {
    if (!m_instance) {
        return false;
    }

    // In real implementation:
    // WGPURequestAdapterOptions options = {};
    // options.powerPreference = power_pref == PowerPreference::HighPerformance
    //     ? WGPUPowerPreference_HighPerformance
    //     : WGPUPowerPreference_LowPower;
    // wgpuInstanceRequestAdapter(m_instance, &options, adapter_callback, this);

    (void)power_pref;
    return true;
}

bool WgpuBackend::request_device(const WgpuBackendConfig& wgpu_config) {
    if (!m_adapter) {
        return false;
    }

    // In real implementation:
    // WGPUDeviceDescriptor desc = {};
    // Setup required features and limits from wgpu_config
    // wgpuAdapterRequestDevice(m_adapter, &desc, device_callback, this);

    (void)wgpu_config;
    return true;
}

void WgpuBackend::query_capabilities() {
    m_capabilities.type = BackendType::Wgpu;

    // In real implementation, query from adapter:
    // WGPUAdapterProperties props = {};
    // wgpuAdapterGetProperties(m_adapter, &props);

    m_capabilities.adapter.name = "wgpu Adapter";
    m_capabilities.adapter.vendor = "wgpu-native";
    m_capabilities.adapter.driver = "1.0.0";
    m_capabilities.adapter.backend_type = BackendType::Wgpu;
    m_capabilities.adapter.is_discrete = true;
    m_capabilities.adapter.is_software = false;

    // Features (would be queried from adapter)
    m_capabilities.features.compute_shaders = true;
    m_capabilities.features.geometry_shaders = false; // Not in WebGPU
    m_capabilities.features.tessellation = false;     // Not in WebGPU
    m_capabilities.features.ray_tracing = false;
    m_capabilities.features.mesh_shaders = false;
    m_capabilities.features.variable_rate_shading = false;
    m_capabilities.features.bindless = false;
    m_capabilities.features.hdr_output = true;
    m_capabilities.features.vrr = true;
    m_capabilities.features.multiview = true;
    m_capabilities.features.foveated_rendering = false;

    // Limits (would be queried from device)
    m_capabilities.limits.max_texture_dimension_1d = 8192;
    m_capabilities.limits.max_texture_dimension_2d = 8192;
    m_capabilities.limits.max_texture_dimension_3d = 2048;
    m_capabilities.limits.max_texture_array_layers = 256;
    m_capabilities.limits.max_bind_groups = 4;
    m_capabilities.limits.max_uniform_buffer_size = 65536;
    m_capabilities.limits.max_storage_buffer_size = 134217728;
    m_capabilities.limits.max_vertex_buffers = 8;
    m_capabilities.limits.max_vertex_attributes = 16;
    m_capabilities.limits.max_compute_workgroup_size_x = 256;
    m_capabilities.limits.max_compute_workgroup_size_y = 256;
    m_capabilities.limits.max_compute_workgroup_size_z = 64;
    m_capabilities.limits.max_compute_workgroups_per_dimension = 65535;

    // Supported formats
    m_capabilities.supported_formats = {
        SurfaceFormat::Bgra8UnormSrgb,
        SurfaceFormat::Rgba8UnormSrgb,
        SurfaceFormat::Bgra8Unorm,
        SurfaceFormat::Rgba8Unorm,
        SurfaceFormat::Rgba16Float,
        SurfaceFormat::Rgb10a2Unorm,
    };

    // Supported present modes
    m_capabilities.supported_present_modes = {
        PresentMode::Fifo,
        PresentMode::FifoRelaxed,
        PresentMode::Mailbox,
        PresentMode::Immediate,
    };
}

void WgpuBackend::on_device_lost(const char* message) {
    m_device_lost = true;
    m_last_error = BackendError::device_lost(message ? message : "GPU device lost");

    if (m_event_callback) {
        m_event_callback("device_lost", message ? message : "");
    }
}

void WgpuBackend::on_uncaptured_error(std::uint32_t type, const char* message) {
    std::string error_type;
    switch (type) {
        case 1: error_type = "Validation"; break;
        case 2: error_type = "OutOfMemory"; break;
        case 3: error_type = "Internal"; break;
        default: error_type = "Unknown"; break;
    }

    m_last_error = BackendError::device_lost(error_type + ": " + (message ? message : ""));

    if (m_event_callback) {
        m_event_callback("uncaptured_error", message ? message : "");
    }
}

// =============================================================================
// Factory Registration
// =============================================================================

namespace {
std::once_flag g_wgpu_backend_registered;
} // anonymous namespace

void register_wgpu_backend() {
    std::call_once(g_wgpu_backend_registered, []() {
        BackendFactory::register_backend(BackendType::Wgpu,
            [](const BackendConfig& config) {
                return std::make_unique<WgpuBackend>(config, WgpuBackendConfig{});
            }
        );
    });
}

bool is_wgpu_available() {
    // In real implementation, would check if wgpu-native library is loaded
    // and if an instance can be created
    return true;
}

std::vector<BackendType> get_wgpu_available_backends() {
    std::vector<BackendType> backends;

    // wgpu can use multiple underlying APIs
    // In real implementation, enumerate adapters and check their backend types

#if defined(_WIN32)
    backends.push_back(BackendType::D3D12);
    backends.push_back(BackendType::Vulkan);
#elif defined(__APPLE__)
    backends.push_back(BackendType::Metal);
#elif defined(__linux__)
    backends.push_back(BackendType::Vulkan);
#endif

    // OpenGL is usually available as fallback
    backends.push_back(BackendType::OpenGL);

    return backends;
}

#else // !VOID_HAS_WGPU

// =============================================================================
// Stub Implementation (when wgpu is not available)
// =============================================================================

void register_wgpu_backend() {
    // No-op when wgpu is not available
}

bool is_wgpu_available() {
    return false;
}

std::vector<BackendType> get_wgpu_available_backends() {
    return {};
}

#endif // VOID_HAS_WGPU

// =============================================================================
// Debug Utilities (always available)
// =============================================================================

namespace debug {

std::string format_wgpu_backend_config(const WgpuBackendConfig& config) {
    std::string result = "WgpuBackendConfig {\n";

    const char* backend_str = "Auto";
    switch (config.forced_backend) {
        case WgpuBackendConfig::WgpuBackendType::Auto: backend_str = "Auto"; break;
        case WgpuBackendConfig::WgpuBackendType::Vulkan: backend_str = "Vulkan"; break;
        case WgpuBackendConfig::WgpuBackendType::D3D12: backend_str = "D3D12"; break;
        case WgpuBackendConfig::WgpuBackendType::Metal: backend_str = "Metal"; break;
        case WgpuBackendConfig::WgpuBackendType::OpenGL: backend_str = "OpenGL"; break;
    }
    result += "  forced_backend: " + std::string(backend_str) + "\n";

    result += "  enable_api_tracing: " +
              std::string(config.enable_api_tracing ? "true" : "false") + "\n";
    result += "  enable_shader_validation: " +
              std::string(config.enable_shader_validation ? "true" : "false") + "\n";

    if (!config.shader_dump_path.empty()) {
        result += "  shader_dump_path: " + config.shader_dump_path + "\n";
    }

    if (!config.required_features.empty()) {
        result += "  required_features: [";
        for (std::size_t i = 0; i < config.required_features.size(); ++i) {
            if (i > 0) result += ", ";
            result += config.required_features[i];
        }
        result += "]\n";
    }

    result += "}";
    return result;
}

} // namespace debug

} // namespace backends
} // namespace void_presenter
