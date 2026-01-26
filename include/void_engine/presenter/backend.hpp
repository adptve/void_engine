#pragma once

/// @file backend.hpp
/// @brief Graphics backend abstraction for void_presenter
///
/// Provides the abstraction layer for multiple graphics backends:
/// - wgpu-native (wraps Vulkan, D3D12, Metal, OpenGL)
/// - WebGPU (native web API)
/// - OpenXR (VR/XR on native platforms)
/// - WebXR (VR/XR on web)
///
/// ## Architecture (inspired by Unity RHI and Unreal RHI)
///
/// ```
/// Application
///     │
///     ▼
/// ┌─────────────────────────────────────────┐
/// │           IBackend Interface            │
/// │  (create_surface, create_swapchain)     │
/// └─────────────────────────────────────────┘
///     │           │           │           │
///     ▼           ▼           ▼           ▼
/// ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐
/// │ wgpu  │  │WebGPU │  │OpenXR │  │ WebXR │
/// │native │  │ (web) │  │(VR/XR)│  │(webVR)│
/// └───────┘  └───────┘  └───────┘  └───────┘
///     │           │           │           │
///     ▼           ▼           ▼           ▼
/// ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐
/// │Vulkan │  │Browser│  │OpenXR │  │Browser│
/// │D3D12  │  │WebGPU │  │Runtime│  │WebXR  │
/// │Metal  │  │  API  │  │       │  │ API   │
/// │OpenGL │  │       │  │       │  │       │
/// └───────┘  └───────┘  └───────┘  └───────┘
/// ```

#include "fwd.hpp"
#include "types.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace void_presenter {

// =============================================================================
// Backend Type
// =============================================================================

/// Graphics backend type
enum class BackendType {
    Null,       ///< Null backend (testing)
    Wgpu,       ///< wgpu-native (Vulkan, D3D12, Metal, OpenGL)
    WebGPU,     ///< WebGPU (web browsers)
    Vulkan,     ///< Direct Vulkan (advanced use)
    D3D12,      ///< Direct3D 12 (Windows)
    Metal,      ///< Metal (macOS/iOS)
    OpenGL,     ///< OpenGL/ES (fallback)
    OpenXR,     ///< OpenXR (native VR/XR)
    WebXR,      ///< WebXR (web VR/XR)
};

/// Get backend name
[[nodiscard]] constexpr const char* to_string(BackendType type) {
    switch (type) {
        case BackendType::Null: return "Null";
        case BackendType::Wgpu: return "wgpu-native";
        case BackendType::WebGPU: return "WebGPU";
        case BackendType::Vulkan: return "Vulkan";
        case BackendType::D3D12: return "D3D12";
        case BackendType::Metal: return "Metal";
        case BackendType::OpenGL: return "OpenGL";
        case BackendType::OpenXR: return "OpenXR";
        case BackendType::WebXR: return "WebXR";
    }
    return "Unknown";
}

/// Check if backend is XR-capable
[[nodiscard]] constexpr bool is_xr_backend(BackendType type) {
    return type == BackendType::OpenXR || type == BackendType::WebXR;
}

/// Check if backend is web-only
[[nodiscard]] constexpr bool is_web_backend(BackendType type) {
    return type == BackendType::WebGPU || type == BackendType::WebXR;
}

// =============================================================================
// Power Preference
// =============================================================================

/// GPU power preference
enum class PowerPreference {
    DontCare,       ///< No preference
    LowPower,       ///< Prefer integrated GPU
    HighPerformance,///< Prefer discrete GPU
};

// =============================================================================
// Backend Features
// =============================================================================

/// Backend feature flags
struct BackendFeatures {
    bool compute_shaders = false;       ///< Compute shader support
    bool geometry_shaders = false;      ///< Geometry shader support
    bool tessellation = false;          ///< Tessellation support
    bool ray_tracing = false;           ///< Ray tracing support
    bool mesh_shaders = false;          ///< Mesh shader support
    bool variable_rate_shading = false; ///< VRS support
    bool bindless = false;              ///< Bindless resources
    bool hdr_output = false;            ///< HDR display output
    bool vrr = false;                   ///< Variable refresh rate
    bool multiview = false;             ///< Multi-view rendering (XR)
    bool foveated_rendering = false;    ///< Foveated rendering (XR)
};

// =============================================================================
// Backend Limits
// =============================================================================

/// Backend resource limits
struct BackendLimits {
    std::uint32_t max_texture_dimension_1d = 8192;
    std::uint32_t max_texture_dimension_2d = 8192;
    std::uint32_t max_texture_dimension_3d = 2048;
    std::uint32_t max_texture_array_layers = 256;
    std::uint32_t max_bind_groups = 4;
    std::uint32_t max_bindings_per_group = 1000;
    std::uint32_t max_uniform_buffer_size = 65536;
    std::uint32_t max_storage_buffer_size = 134217728;
    std::uint32_t max_vertex_buffers = 8;
    std::uint32_t max_vertex_attributes = 16;
    std::uint32_t max_compute_workgroup_size_x = 256;
    std::uint32_t max_compute_workgroup_size_y = 256;
    std::uint32_t max_compute_workgroup_size_z = 64;
    std::uint32_t max_compute_workgroups_per_dimension = 65535;
    std::size_t max_buffer_size = 268435456;  // 256 MB
};

// =============================================================================
// Adapter Info
// =============================================================================

/// GPU adapter information
struct AdapterInfo {
    std::string name;               ///< GPU name
    std::string vendor;             ///< Vendor name
    std::string driver;             ///< Driver info
    std::uint32_t vendor_id = 0;    ///< Vendor ID
    std::uint32_t device_id = 0;    ///< Device ID
    BackendType backend_type = BackendType::Null;
    bool is_discrete = false;       ///< Discrete vs integrated
    bool is_software = false;       ///< Software renderer
    std::size_t dedicated_video_memory = 0;
    std::size_t shared_system_memory = 0;
};

// =============================================================================
// Backend Capabilities
// =============================================================================

/// Complete backend capabilities
struct BackendCapabilities {
    BackendType type = BackendType::Null;
    AdapterInfo adapter;
    BackendFeatures features;
    BackendLimits limits;
    std::vector<SurfaceFormat> supported_formats;
    std::vector<PresentMode> supported_present_modes;

    /// Check if a format is supported
    [[nodiscard]] bool supports_format(SurfaceFormat format) const {
        for (const auto& f : supported_formats) {
            if (f == format) return true;
        }
        return false;
    }

    /// Check if a present mode is supported
    [[nodiscard]] bool supports_present_mode(PresentMode mode) const {
        for (const auto& m : supported_present_modes) {
            if (m == mode) return true;
        }
        return false;
    }

    /// Get best format for HDR
    [[nodiscard]] std::optional<SurfaceFormat> best_hdr_format() const {
        for (const auto& f : supported_formats) {
            if (is_hdr_capable(f)) return f;
        }
        return std::nullopt;
    }
};

// =============================================================================
// Surface Target
// =============================================================================

/// Native window handle (platform-specific)
struct WindowHandle {
#if defined(_WIN32)
    void* hwnd = nullptr;       ///< HWND on Windows
    void* hinstance = nullptr;  ///< HINSTANCE on Windows
#elif defined(__APPLE__)
    void* ns_view = nullptr;    ///< NSView on macOS
    void* ns_window = nullptr;  ///< NSWindow on macOS
#elif defined(__linux__)
    void* display = nullptr;    ///< Display* (X11) or wl_display* (Wayland)
    std::uint64_t window = 0;   ///< Window (X11) or wl_surface* (Wayland)
    bool is_wayland = false;    ///< True if Wayland, false if X11
#endif
};

/// Web canvas handle
struct CanvasHandle {
    std::string canvas_id;      ///< HTML canvas element ID
    std::uint32_t width = 0;    ///< Canvas width
    std::uint32_t height = 0;   ///< Canvas height
    float device_pixel_ratio = 1.0f;
};

/// XR session handle
struct XrSessionHandle {
    void* session = nullptr;    ///< XrSession or WebXR session
    void* system = nullptr;     ///< XrSystemId or system info
    bool is_immersive = true;   ///< Immersive vs inline
};

/// Offscreen rendering config
struct OffscreenConfig {
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    SurfaceFormat format = SurfaceFormat::Rgba8UnormSrgb;
    bool is_headless = true;
};

/// Surface target - what we're rendering to
using SurfaceTarget = std::variant<
    WindowHandle,
    CanvasHandle,
    XrSessionHandle,
    OffscreenConfig
>;

// =============================================================================
// Backend Configuration
// =============================================================================

/// Backend initialization configuration
struct BackendConfig {
    BackendType preferred_type = BackendType::Wgpu;
    PowerPreference power_preference = PowerPreference::HighPerformance;
    bool enable_validation = false;     ///< Enable API validation layers
    bool enable_debug_markers = false;  ///< Enable debug markers/labels
    bool prefer_low_latency = false;    ///< Prefer low latency over throughput
    bool enable_gpu_timing = false;     ///< Enable GPU timestamp queries
    bool allow_software_fallback = true;  ///< Allow software rendering fallback
    std::vector<BackendType> fallback_types;  ///< Fallback if preferred unavailable

    /// Builder pattern
    [[nodiscard]] BackendConfig with_validation(bool enable) const {
        BackendConfig copy = *this;
        copy.enable_validation = enable;
        return copy;
    }

    [[nodiscard]] BackendConfig with_power_preference(PowerPreference pref) const {
        BackendConfig copy = *this;
        copy.power_preference = pref;
        return copy;
    }

    [[nodiscard]] BackendConfig with_fallbacks(std::vector<BackendType> types) const {
        BackendConfig copy = *this;
        copy.fallback_types = std::move(types);
        return copy;
    }
};

// =============================================================================
// Backend Error
// =============================================================================

/// Backend error types
enum class BackendErrorKind {
    NotSupported,       ///< Backend not supported on platform
    InitFailed,         ///< Initialization failed
    DeviceLost,         ///< GPU device lost
    OutOfMemory,        ///< Out of GPU memory
    ValidationFailed,   ///< Validation layer error
    SurfaceError,       ///< Surface creation/operation error
    SwapchainError,     ///< Swapchain error
    Timeout,            ///< Operation timed out
    Internal,           ///< Internal error
};

/// Backend error
struct BackendError {
    BackendErrorKind kind;
    std::string message;
    std::optional<std::int32_t> native_error_code;

    [[nodiscard]] static BackendError not_supported(std::string msg) {
        return {BackendErrorKind::NotSupported, std::move(msg), std::nullopt};
    }

    [[nodiscard]] static BackendError init_failed(std::string msg) {
        return {BackendErrorKind::InitFailed, std::move(msg), std::nullopt};
    }

    [[nodiscard]] static BackendError device_lost(std::string msg = "Device lost") {
        return {BackendErrorKind::DeviceLost, std::move(msg), std::nullopt};
    }

    [[nodiscard]] static BackendError out_of_memory() {
        return {BackendErrorKind::OutOfMemory, "Out of GPU memory", std::nullopt};
    }

    [[nodiscard]] static BackendError surface_failed(std::string msg) {
        return {BackendErrorKind::SurfaceError, std::move(msg), std::nullopt};
    }
};

// =============================================================================
// GPU Resource Handle
// =============================================================================

/// Opaque handle to a GPU resource
struct GpuResourceHandle {
    std::uint64_t id = 0;
    BackendType backend = BackendType::Null;

    [[nodiscard]] bool is_valid() const { return id != 0; }

    bool operator==(const GpuResourceHandle& other) const {
        return id == other.id && backend == other.backend;
    }
};

// =============================================================================
// Acquired Image
// =============================================================================

/// Acquired swapchain image for rendering
struct AcquiredImage {
    GpuResourceHandle texture;          ///< Texture handle
    std::uint32_t width = 0;            ///< Image width
    std::uint32_t height = 0;           ///< Image height
    SurfaceFormat format = SurfaceFormat::Bgra8UnormSrgb;
    std::uint32_t image_index = 0;      ///< Swapchain image index
    bool suboptimal = false;            ///< Should resize soon
    void* native_handle = nullptr;      ///< Backend-specific handle
};

// =============================================================================
// Swapchain Interface
// =============================================================================

/// Swapchain configuration
struct SwapchainConfig {
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    SurfaceFormat format = SurfaceFormat::Bgra8UnormSrgb;
    PresentMode present_mode = PresentMode::Fifo;
    AlphaMode alpha_mode = AlphaMode::Opaque;
    std::uint32_t image_count = 3;      ///< Triple buffering by default
    bool enable_hdr = false;

    [[nodiscard]] SwapchainConfig with_size(std::uint32_t w, std::uint32_t h) const {
        SwapchainConfig copy = *this;
        copy.width = w;
        copy.height = h;
        return copy;
    }

    [[nodiscard]] SwapchainConfig with_format(SurfaceFormat f) const {
        SwapchainConfig copy = *this;
        copy.format = f;
        return copy;
    }

    [[nodiscard]] SwapchainConfig with_present_mode(PresentMode mode) const {
        SwapchainConfig copy = *this;
        copy.present_mode = mode;
        return copy;
    }
};

/// Swapchain interface
class ISwapchain {
public:
    virtual ~ISwapchain() = default;

    /// Get current configuration
    [[nodiscard]] virtual const SwapchainConfig& config() const = 0;

    /// Resize the swapchain
    /// @return true on success
    virtual bool resize(std::uint32_t width, std::uint32_t height) = 0;

    /// Acquire the next image for rendering
    /// @param timeout_ns Timeout in nanoseconds (0 = non-blocking, UINT64_MAX = infinite)
    /// @param out_image Output acquired image
    /// @return true on success
    virtual bool acquire_image(std::uint64_t timeout_ns, AcquiredImage& out_image) = 0;

    /// Present the acquired image
    /// @return true on success
    virtual bool present(const AcquiredImage& image) = 0;

    /// Get current size
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const {
        return {config().width, config().height};
    }
};

// =============================================================================
// Backend Surface Interface
// =============================================================================

/// Backend surface interface
class IBackendSurface {
public:
    virtual ~IBackendSurface() = default;

    /// Get surface capabilities for this backend
    [[nodiscard]] virtual SurfaceCapabilities capabilities() const = 0;

    /// Create a swapchain for this surface
    [[nodiscard]] virtual std::unique_ptr<ISwapchain> create_swapchain(
        const SwapchainConfig& config) = 0;

    /// Get native surface handle (backend-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;

    /// Check if surface is still valid
    [[nodiscard]] virtual bool is_valid() const = 0;
};

// =============================================================================
// Backend Interface
// =============================================================================

/// Backend event callback
using BackendEventCallback = std::function<void(const std::string& event, const std::string& details)>;

/// Graphics backend interface
class IBackend {
public:
    virtual ~IBackend() = default;

    /// Get backend type
    [[nodiscard]] virtual BackendType type() const = 0;

    /// Get backend capabilities
    [[nodiscard]] virtual const BackendCapabilities& capabilities() const = 0;

    /// Create a surface for the given target
    [[nodiscard]] virtual std::unique_ptr<IBackendSurface> create_surface(
        const SurfaceTarget& target) = 0;

    /// Wait for all GPU operations to complete
    virtual void wait_idle() = 0;

    /// Check if backend is healthy
    [[nodiscard]] virtual bool is_healthy() const = 0;

    /// Get last error (if any)
    [[nodiscard]] virtual std::optional<BackendError> last_error() const = 0;

    /// Set event callback for backend events
    virtual void set_event_callback(BackendEventCallback callback) = 0;

    /// Poll for device events (device lost, etc.)
    virtual void poll_events() = 0;

    /// Get native device handle (backend-specific)
    [[nodiscard]] virtual void* native_device() const = 0;

    /// Get native queue handle (backend-specific)
    [[nodiscard]] virtual void* native_queue() const = 0;
};

// =============================================================================
// Backend Factory
// =============================================================================

/// Backend availability info
struct BackendAvailability {
    BackendType type;
    bool available = false;
    std::string reason;  ///< Why not available (if !available)
};

/// Backend factory for creating backends
class BackendFactory {
public:
    /// Check which backends are available on this platform
    [[nodiscard]] static std::vector<BackendAvailability> query_available();

    /// Check if a specific backend is available
    [[nodiscard]] static bool is_available(BackendType type);

    /// Get recommended backend for this platform
    [[nodiscard]] static BackendType recommended();

    /// Create a backend
    /// @param config Backend configuration
    /// @return Created backend or nullptr on failure
    [[nodiscard]] static std::unique_ptr<IBackend> create(const BackendConfig& config);

    /// Create a backend with automatic fallback
    /// @param preferred Preferred backend type
    /// @param fallbacks Fallback backends to try
    /// @return Created backend or nullptr if all failed
    [[nodiscard]] static std::unique_ptr<IBackend> create_with_fallback(
        BackendType preferred,
        const std::vector<BackendType>& fallbacks = {});

    /// Create best available backend
    [[nodiscard]] static std::unique_ptr<IBackend> create_best_available(
        PowerPreference power_pref = PowerPreference::HighPerformance);

    /// Register a custom backend factory
    using BackendCreator = std::function<std::unique_ptr<IBackend>(const BackendConfig&)>;
    static void register_backend(BackendType type, BackendCreator creator);
};

} // namespace void_presenter
