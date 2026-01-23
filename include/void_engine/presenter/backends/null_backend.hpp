#pragma once

/// @file null_backend.hpp
/// @brief Null backend for testing

#include "../backend.hpp"
#include "../surface.hpp"

#include <atomic>

namespace void_presenter {
namespace backends {

// =============================================================================
// Null Swapchain
// =============================================================================

/// Null swapchain for testing
class NullSwapchain : public ISwapchain {
public:
    explicit NullSwapchain(const SwapchainConfig& config)
        : m_config(config)
        , m_image_index(0)
        , m_texture_id(0)
    {}

    [[nodiscard]] const SwapchainConfig& config() const override {
        return m_config;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override {
        m_config.width = width;
        m_config.height = height;
        return true;
    }

    bool acquire_image(std::uint64_t /*timeout_ns*/, AcquiredImage& out_image) override {
        ++m_texture_id;
        m_image_index = (m_image_index + 1) % m_config.image_count;

        out_image.texture = GpuResourceHandle{m_texture_id, BackendType::Null};
        out_image.width = m_config.width;
        out_image.height = m_config.height;
        out_image.format = m_config.format;
        out_image.image_index = m_image_index;
        out_image.suboptimal = false;
        out_image.native_handle = nullptr;

        return true;
    }

    bool present(const AcquiredImage& /*image*/) override {
        return true;
    }

private:
    SwapchainConfig m_config;
    std::uint32_t m_image_index;
    std::uint64_t m_texture_id;
};

// =============================================================================
// Null Surface
// =============================================================================

/// Null surface for testing
class NullBackendSurface : public IBackendSurface {
public:
    NullBackendSurface() {
        m_capabilities.formats = {
            SurfaceFormat::Bgra8UnormSrgb,
            SurfaceFormat::Rgba8UnormSrgb,
            SurfaceFormat::Rgba16Float,
        };
        m_capabilities.present_modes = {
            PresentMode::Immediate,
            PresentMode::Mailbox,
            PresentMode::Fifo,
            PresentMode::FifoRelaxed,
        };
        m_capabilities.alpha_modes = {AlphaMode::Opaque, AlphaMode::PreMultiplied};
        m_capabilities.min_width = 1;
        m_capabilities.min_height = 1;
        m_capabilities.max_width = 16384;
        m_capabilities.max_height = 16384;
    }

    [[nodiscard]] SurfaceCapabilities capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<ISwapchain> create_swapchain(const SwapchainConfig& config) override {
        return std::make_unique<NullSwapchain>(config);
    }

    [[nodiscard]] void* native_handle() const override {
        return nullptr;
    }

    [[nodiscard]] bool is_valid() const override {
        return true;
    }

private:
    SurfaceCapabilities m_capabilities;
};

// =============================================================================
// Null Backend
// =============================================================================

/// Null backend for testing
class NullBackend : public IBackend {
public:
    NullBackend() {
        // Setup capabilities
        m_capabilities.type = BackendType::Null;
        m_capabilities.adapter.name = "Null Adapter";
        m_capabilities.adapter.vendor = "Void Engine";
        m_capabilities.adapter.driver = "1.0.0";
        m_capabilities.adapter.backend_type = BackendType::Null;
        m_capabilities.adapter.is_software = true;

        m_capabilities.features.compute_shaders = true;
        m_capabilities.features.hdr_output = true;
        m_capabilities.features.vrr = true;
        m_capabilities.features.multiview = true;

        m_capabilities.supported_formats = {
            SurfaceFormat::Bgra8UnormSrgb,
            SurfaceFormat::Rgba8UnormSrgb,
            SurfaceFormat::Rgba16Float,
            SurfaceFormat::Rgb10a2Unorm,
        };

        m_capabilities.supported_present_modes = {
            PresentMode::Immediate,
            PresentMode::Mailbox,
            PresentMode::Fifo,
            PresentMode::FifoRelaxed,
        };
    }

    [[nodiscard]] BackendType type() const override {
        return BackendType::Null;
    }

    [[nodiscard]] const BackendCapabilities& capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] std::unique_ptr<IBackendSurface> create_surface(const SurfaceTarget& /*target*/) override {
        return std::make_unique<NullBackendSurface>();
    }

    void wait_idle() override {
        // No-op
    }

    [[nodiscard]] bool is_healthy() const override {
        return true;
    }

    [[nodiscard]] std::optional<BackendError> last_error() const override {
        return m_last_error;
    }

    void set_event_callback(BackendEventCallback callback) override {
        m_event_callback = std::move(callback);
    }

    void poll_events() override {
        // No-op
    }

    [[nodiscard]] void* native_device() const override {
        return nullptr;
    }

    [[nodiscard]] void* native_queue() const override {
        return nullptr;
    }

private:
    BackendCapabilities m_capabilities;
    std::optional<BackendError> m_last_error;
    BackendEventCallback m_event_callback;
};

} // namespace backends
} // namespace void_presenter
