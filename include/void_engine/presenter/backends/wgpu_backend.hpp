#pragma once

/// @file wgpu_backend.hpp
/// @brief wgpu-native backend for cross-platform GPU support
///
/// This backend uses wgpu-native to provide:
/// - Vulkan (Linux, Windows, Android)
/// - Direct3D 12 (Windows)
/// - Metal (macOS, iOS)
/// - OpenGL/ES (fallback)
///
/// wgpu-native is the native implementation of the WebGPU standard,
/// providing a modern, safe GPU API across all platforms.

#include "../backend.hpp"
#include "../surface.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations for wgpu-native types
// In real implementation, include <wgpu.h>
struct WGPUInstanceImpl;
struct WGPUAdapterImpl;
struct WGPUDeviceImpl;
struct WGPUQueueImpl;
struct WGPUSurfaceImpl;
struct WGPUSwapChainImpl;

namespace void_presenter {
namespace backends {

// =============================================================================
// wgpu Swapchain
// =============================================================================

/// wgpu swapchain implementation
class WgpuSwapchain : public ISwapchain {
public:
    WgpuSwapchain(
        WGPUDeviceImpl* device,
        WGPUSurfaceImpl* surface,
        const SwapchainConfig& config);

    ~WgpuSwapchain();

    // Non-copyable
    WgpuSwapchain(const WgpuSwapchain&) = delete;
    WgpuSwapchain& operator=(const WgpuSwapchain&) = delete;

    [[nodiscard]] const SwapchainConfig& config() const override {
        return m_config;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override;

    bool acquire_image(std::uint64_t timeout_ns, AcquiredImage& out_image) override;

    bool present(const AcquiredImage& image) override;

private:
    bool create_swapchain();
    void destroy_swapchain();

    WGPUDeviceImpl* m_device;
    WGPUSurfaceImpl* m_surface;
    WGPUSwapChainImpl* m_swapchain;
    SwapchainConfig m_config;

    std::uint32_t m_current_image_index;
    std::uint64_t m_texture_id_counter;

    mutable std::mutex m_mutex;
};

// =============================================================================
// wgpu Surface
// =============================================================================

/// wgpu surface implementation
class WgpuSurface : public IBackendSurface {
public:
    WgpuSurface(
        WGPUInstanceImpl* instance,
        WGPUAdapterImpl* adapter,
        WGPUDeviceImpl* device,
        const SurfaceTarget& target);

    ~WgpuSurface();

    // Non-copyable
    WgpuSurface(const WgpuSurface&) = delete;
    WgpuSurface& operator=(const WgpuSurface&) = delete;

    [[nodiscard]] SurfaceCapabilities capabilities() const override;

    [[nodiscard]] std::unique_ptr<ISwapchain> create_swapchain(const SwapchainConfig& config) override;

    [[nodiscard]] void* native_handle() const override {
        return m_surface;
    }

    [[nodiscard]] bool is_valid() const override {
        return m_surface != nullptr;
    }

private:
    bool create_surface(const SurfaceTarget& target);
    void query_capabilities();

    WGPUInstanceImpl* m_instance;
    WGPUAdapterImpl* m_adapter;
    WGPUDeviceImpl* m_device;
    WGPUSurfaceImpl* m_surface;

    SurfaceCapabilities m_capabilities;
    bool m_capabilities_queried;
};

// =============================================================================
// wgpu Backend Configuration
// =============================================================================

/// wgpu-specific backend configuration
struct WgpuBackendConfig {
    /// Force specific wgpu backend
    enum class WgpuBackendType {
        Auto,       ///< Automatic selection
        Vulkan,     ///< Force Vulkan
        D3D12,      ///< Force D3D12
        Metal,      ///< Force Metal
        OpenGL,     ///< Force OpenGL
    };

    WgpuBackendType forced_backend = WgpuBackendType::Auto;
    bool enable_api_tracing = false;
    bool enable_shader_validation = false;
    std::string shader_dump_path;  ///< Path to dump compiled shaders

    /// Required features
    std::vector<std::string> required_features;

    /// Required limits (0 = default)
    std::uint32_t min_uniform_buffer_size = 0;
    std::uint32_t min_storage_buffer_size = 0;
    std::uint32_t min_texture_dimension = 0;
};

// =============================================================================
// wgpu Backend
// =============================================================================

/// wgpu-native backend implementation
class WgpuBackend : public IBackend {
public:
    /// Create wgpu backend
    /// @param config Backend configuration
    /// @param wgpu_config wgpu-specific configuration
    explicit WgpuBackend(
        const BackendConfig& config,
        const WgpuBackendConfig& wgpu_config = {});

    ~WgpuBackend();

    // Non-copyable
    WgpuBackend(const WgpuBackend&) = delete;
    WgpuBackend& operator=(const WgpuBackend&) = delete;

    [[nodiscard]] BackendType type() const override {
        return BackendType::Wgpu;
    }

    [[nodiscard]] const BackendCapabilities& capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<IBackendSurface> create_surface(const SurfaceTarget& target) override;

    void wait_idle() override;

    [[nodiscard]] bool is_healthy() const override {
        return m_device != nullptr && !m_device_lost;
    }

    [[nodiscard]] std::optional<BackendError> last_error() const override {
        return m_last_error;
    }

    void set_event_callback(BackendEventCallback callback) override {
        m_event_callback = std::move(callback);
    }

    void poll_events() override;

    [[nodiscard]] void* native_device() const override {
        return m_device;
    }

    [[nodiscard]] void* native_queue() const override {
        return m_queue;
    }

    // =========================================================================
    // wgpu-specific methods
    // =========================================================================

    /// Get wgpu instance
    [[nodiscard]] WGPUInstanceImpl* instance() const { return m_instance; }

    /// Get wgpu adapter
    [[nodiscard]] WGPUAdapterImpl* adapter() const { return m_adapter; }

    /// Get wgpu device
    [[nodiscard]] WGPUDeviceImpl* device() const { return m_device; }

    /// Get wgpu queue
    [[nodiscard]] WGPUQueueImpl* queue() const { return m_queue; }

    /// Check which underlying API is being used
    [[nodiscard]] BackendType underlying_api() const { return m_underlying_api; }

private:
    bool initialize(const BackendConfig& config, const WgpuBackendConfig& wgpu_config);
    void shutdown();

    bool create_instance();
    bool request_adapter(PowerPreference power_pref);
    bool request_device(const WgpuBackendConfig& wgpu_config);
    void query_capabilities();

    void on_device_lost(const char* message);
    void on_uncaptured_error(std::uint32_t type, const char* message);

    WGPUInstanceImpl* m_instance;
    WGPUAdapterImpl* m_adapter;
    WGPUDeviceImpl* m_device;
    WGPUQueueImpl* m_queue;

    BackendCapabilities m_capabilities;
    BackendType m_underlying_api;  // Actual API being used (Vulkan, D3D12, etc.)

    std::atomic<bool> m_device_lost;
    std::optional<BackendError> m_last_error;
    BackendEventCallback m_event_callback;

    mutable std::mutex m_mutex;
};

// =============================================================================
// wgpu Backend Factory Registration
// =============================================================================

/// Register wgpu backend with the factory
void register_wgpu_backend();

/// Check if wgpu is available on this platform
[[nodiscard]] bool is_wgpu_available();

/// Get available wgpu underlying backends
[[nodiscard]] std::vector<BackendType> get_wgpu_available_backends();

} // namespace backends
} // namespace void_presenter
