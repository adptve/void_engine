/// @file backend.cpp
/// @brief Multi-backend GPU abstraction - Factory functions and BackendManager
///
/// This file provides the factory functions and BackendManager that coordinate
/// all GPU backends. Individual backend implementations are in backends/*

#include "void_engine/render/backend.hpp"

// Include all backend implementations
#include "backends/null/null_backend.hpp"
#include "backends/opengl/opengl_backend.hpp"

#ifdef _WIN32
#include "backends/vulkan/vulkan_backend.hpp"
#include "backends/d3d12/d3d12_backend.hpp"
#elif defined(__linux__)
#include "backends/vulkan/vulkan_backend.hpp"
#elif defined(__APPLE__)
#include "backends/metal/metal_backend.hpp"
#endif

#include "backends/webgpu/webgpu_backend.hpp"

#include <algorithm>
#include <filesystem>

namespace void_render {

// =============================================================================
// GPU Namespace Factory Functions
// =============================================================================

namespace gpu {

std::vector<BackendAvailability> detect_available_backends() {
    std::vector<BackendAvailability> result;

#ifdef _WIN32
    // Windows: Check Vulkan, D3D12, OpenGL
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Vulkan;
        ba.available = backends::check_vulkan_available();
        ba.reason = ba.available ? "" : "vulkan-1.dll not found";
        result.push_back(ba);
    }
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Direct3D12;
        ba.available = backends::check_d3d12_available();
        ba.reason = ba.available ? "" : "d3d12.dll not found";
        result.push_back(ba);
    }
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::OpenGL;
        ba.available = backends::check_opengl_available();
        ba.reason = ba.available ? "" : "OpenGL not available";
        result.push_back(ba);
    }
#elif defined(__linux__)
    // Linux: Check Vulkan, OpenGL
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Vulkan;
        ba.available = backends::check_vulkan_available();
        ba.reason = ba.available ? "" : "libvulkan.so.1 not found";
        result.push_back(ba);
    }
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::OpenGL;
        ba.available = backends::check_opengl_available();
        ba.reason = ba.available ? "" : "libGL.so.1 not found";
        result.push_back(ba);
    }
#elif defined(__APPLE__)
    // macOS: Check Metal
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Metal;
        ba.available = backends::check_metal_available();
        ba.reason = ba.available ? "" : "Metal not available";
        result.push_back(ba);
    }
#endif

#ifdef __EMSCRIPTEN__
    // Web: Check WebGPU
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::WebGPU;
        ba.available = backends::check_webgpu_available();
        ba.reason = ba.available ? "" : "WebGPU not available";
        result.push_back(ba);
    }
#endif

    // Null backend is always available
    {
        BackendAvailability ba;
        ba.gpu_backend = GpuBackend::Null;
        ba.available = true;
        result.push_back(ba);
    }

    return result;
}

GpuBackend select_gpu_backend(const BackendConfig& config,
                               const std::vector<BackendAvailability>& available) {
    // If specific backend requested
    if (config.preferred_gpu_backend != GpuBackend::Auto) {
        for (const auto& ba : available) {
            if (ba.gpu_backend == config.preferred_gpu_backend) {
                if (ba.available) {
                    return ba.gpu_backend;
                } else if (config.gpu_selector == BackendSelector::Require) {
                    return GpuBackend::Null;  // Required but not available
                }
                break;
            }
        }
    }

    // Auto-select best available backend
    // See BackendManager::init() for full status comments on each backend
    static const GpuBackend priority[] = {
        GpuBackend::OpenGL,     // PRODUCTION
        GpuBackend::Vulkan,     // PARTIAL
        GpuBackend::Direct3D12, // STUB
        GpuBackend::Metal,      // STUB
        GpuBackend::WebGPU,     // STUB
        GpuBackend::Null        // FALLBACK
    };

    for (auto backend : priority) {
        for (const auto& ba : available) {
            if (ba.gpu_backend == backend && ba.available) {
                return backend;
            }
        }
    }

    return GpuBackend::Null;
}

std::unique_ptr<IGpuBackend> create_backend(GpuBackend backend) {
    switch (backend) {
        case GpuBackend::Null:
            return backends::create_null_backend();

        case GpuBackend::OpenGL:
            return backends::create_opengl_backend();

#ifdef _WIN32
        case GpuBackend::Vulkan:
            return backends::create_vulkan_backend();

        case GpuBackend::Direct3D12:
            return backends::create_d3d12_backend();
#elif defined(__linux__)
        case GpuBackend::Vulkan:
            return backends::create_vulkan_backend();
#elif defined(__APPLE__)
        case GpuBackend::Metal:
            return backends::create_metal_backend();
#endif

        case GpuBackend::WebGPU:
            return backends::create_webgpu_backend();

        default:
            return nullptr;
    }
}

std::unique_ptr<IPresenter> create_presenter(DisplayBackend backend,
                                              IGpuBackend* gpu_backend,
                                              const BackendConfig& config) {
    // Presenter creation - currently returns null as presenters are handled by window system
    // Full implementation would create platform-specific presenters
    (void)backend;
    (void)gpu_backend;
    (void)config;
    return nullptr;
}

} // namespace gpu

// =============================================================================
// BackendManager Implementation
// =============================================================================

BackendManager::~BackendManager() {
    shutdown();
}

gpu::BackendError BackendManager::init(const gpu::BackendConfig& config) {
    if (m_gpu_backend) return gpu::BackendError::AlreadyInitialized;

    m_config = config;

    // Detect available backends
    auto available = gpu::detect_available_backends();

    // =========================================================================
    // BACKEND PRIORITY - Adjust as implementations mature
    // =========================================================================
    // Status as of 2026-01-28:
    //   - OpenGL:   PRODUCTION - Full implementation, function loading, resource management
    //   - Vulkan:   PARTIAL    - Instance/device creation works, needs surface/swapchain
    //                            integration with GLFW (vkCreateWin32SurfaceKHR, VkSwapchainKHR)
    //   - D3D12:    STUB       - Structure ready, needs ID3D12Device creation, command queues,
    //                            descriptor heaps, real resource allocation
    //   - Metal:    STUB       - Needs Objective-C++ integration, MTLCreateSystemDefaultDevice()
    //   - WebGPU:   STUB       - Needs Dawn or wgpu-native library linking
    //
    // TODO: When promoting a backend:
    //   1. Implement full resource creation (buffers, textures, pipelines)
    //   2. Implement surface/swapchain for window presentation
    //   3. Test hot-swap with SACRED patterns (snapshot/restore/rehydrate)
    //   4. Move it higher in priority list
    // =========================================================================
    static const GpuBackend priority[] = {
        GpuBackend::OpenGL,     // PRODUCTION - Use this until others are complete
        GpuBackend::Vulkan,     // PARTIAL - Needs surface/swapchain integration
        GpuBackend::Direct3D12, // STUB - Needs real D3D12 resource creation
        GpuBackend::Metal,      // STUB - macOS only, needs Obj-C integration
        GpuBackend::WebGPU,     // STUB - Needs Dawn/wgpu-native linking
        GpuBackend::Null        // FALLBACK - Headless/testing only
    };

    // If specific backend requested, try it first
    if (config.preferred_gpu_backend != GpuBackend::Auto) {
        m_gpu_backend = gpu::create_backend(config.preferred_gpu_backend);
        if (m_gpu_backend) {
            gpu::BackendError err = m_gpu_backend->init(config);
            if (err == gpu::BackendError::None) {
                return gpu::BackendError::None;  // Success with preferred backend
            }
            m_gpu_backend.reset();  // Failed, will try fallbacks
        }

        if (config.gpu_selector == BackendSelector::Require) {
            return gpu::BackendError::UnsupportedBackend;  // Required backend failed
        }
    }

    // Try backends in priority order with fallback
    for (auto backend : priority) {
        // Check if this backend is available
        bool is_available = false;
        for (const auto& ba : available) {
            if (ba.gpu_backend == backend && ba.available) {
                is_available = true;
                break;
            }
        }

        if (!is_available) continue;

        // Try to create and initialize
        m_gpu_backend = gpu::create_backend(backend);
        if (!m_gpu_backend) continue;

        gpu::BackendError err = m_gpu_backend->init(config);
        if (err == gpu::BackendError::None) {
            return gpu::BackendError::None;  // Success!
        }

        // Init failed, try next backend
        m_gpu_backend.reset();
    }

    // All backends failed
    return gpu::BackendError::UnsupportedBackend;

    // Create primary presenter
    DisplayBackend display_backend = config.preferred_display_backend;
    if (display_backend == DisplayBackend::Auto) {
#ifdef _WIN32
        display_backend = DisplayBackend::Win32;
#elif defined(__APPLE__)
        display_backend = DisplayBackend::Cocoa;
#elif defined(__EMSCRIPTEN__)
        display_backend = DisplayBackend::Web;
#else
        // Linux: prefer Wayland, fallback to X11
        if (std::getenv("WAYLAND_DISPLAY")) {
            display_backend = DisplayBackend::Wayland;
        } else {
            display_backend = DisplayBackend::X11;
        }
#endif
    }

    auto presenter = gpu::create_presenter(display_backend, m_gpu_backend.get(), config);
    if (presenter) {
        m_presenters.push_back(std::move(presenter));
    }

    return gpu::BackendError::None;
}

void BackendManager::shutdown() {
    if (!m_gpu_backend) return;

    // Destroy presenters
    m_presenters.clear();

    // Shutdown GPU backend
    m_gpu_backend->shutdown();
    m_gpu_backend.reset();
}

gpu::IPresenter* BackendManager::get_presenter(gpu::PresenterId id) const {
    for (const auto& presenter : m_presenters) {
        if (presenter && presenter->id() == id) {
            return presenter.get();
        }
    }
    return nullptr;
}

gpu::PresenterId BackendManager::add_presenter(DisplayBackend backend) {
    auto presenter = gpu::create_presenter(backend, m_gpu_backend.get(), m_config);
    if (presenter) {
        gpu::PresenterId id = m_next_presenter_id++;
        m_presenters.push_back(std::move(presenter));
        return id;
    }
    return gpu::PresenterId{0};
}

void BackendManager::remove_presenter(gpu::PresenterId id) {
    m_presenters.erase(
        std::remove_if(m_presenters.begin(), m_presenters.end(),
            [id](const auto& p) { return p && p->id() == id; }),
        m_presenters.end());
}

const gpu::BackendCapabilities& BackendManager::capabilities() const {
    static gpu::BackendCapabilities empty;
    return m_gpu_backend ? m_gpu_backend->capabilities() : empty;
}

gpu::BackendError BackendManager::begin_frame() {
    if (!m_gpu_backend) return gpu::BackendError::NotInitialized;
    return m_gpu_backend->begin_frame();
}

gpu::BackendError BackendManager::end_frame() {
    if (!m_gpu_backend) return gpu::BackendError::NotInitialized;
    return m_gpu_backend->end_frame();
}

gpu::BackendError BackendManager::hot_swap_backend(GpuBackend new_backend) {
    if (!m_gpu_backend) return gpu::BackendError::NotInitialized;

    // Capture rehydration state (SACRED pattern)
    gpu::RehydrationState state = m_gpu_backend->get_rehydration_state();

    // Shutdown old backend
    m_gpu_backend->shutdown();

    // Create new backend
    m_gpu_backend = gpu::create_backend(new_backend);
    if (!m_gpu_backend) {
        return gpu::BackendError::UnsupportedBackend;
    }

    // Initialize new backend
    gpu::BackendError err = m_gpu_backend->init(m_config);
    if (err != gpu::BackendError::None) {
        return err;
    }

    // Restore state (SACRED pattern)
    return m_gpu_backend->rehydrate(state);
}

gpu::BackendError BackendManager::hot_swap_presenter(gpu::PresenterId id, DisplayBackend new_backend) {
    // Find existing presenter
    for (std::size_t i = 0; i < m_presenters.size(); ++i) {
        if (m_presenters[i] && m_presenters[i]->id() == id) {
            // Capture state (SACRED pattern)
            gpu::RehydrationState state = m_presenters[i]->get_rehydration_state();

            // Create new presenter
            auto new_presenter = gpu::create_presenter(new_backend, m_gpu_backend.get(), m_config);
            if (!new_presenter) {
                return gpu::BackendError::UnsupportedBackend;
            }

            // Restore state (SACRED pattern)
            gpu::BackendError err = new_presenter->rehydrate(state);
            if (err != gpu::BackendError::None) {
                return err;
            }

            // Swap
            m_presenters[i] = std::move(new_presenter);
            return gpu::BackendError::None;
        }
    }

    return gpu::BackendError::InvalidHandle;
}

gpu::RehydrationState BackendManager::snapshot() const {
    if (m_gpu_backend) {
        return m_gpu_backend->get_rehydration_state();
    }
    return gpu::RehydrationState{};
}

gpu::BackendError BackendManager::restore(const gpu::RehydrationState& state) {
    if (!m_gpu_backend) return gpu::BackendError::NotInitialized;
    return m_gpu_backend->rehydrate(state);
}

} // namespace void_render
