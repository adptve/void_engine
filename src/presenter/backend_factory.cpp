/// @file backend_factory.cpp
/// @brief Backend factory implementation

#include <void_engine/presenter/backend.hpp>
#include <void_engine/presenter/backends/null_backend.hpp>

#if defined(VOID_HAS_WGPU)
#include <void_engine/presenter/backends/wgpu_backend.hpp>
#endif

#if defined(VOID_HAS_OPENGL) || (!defined(VOID_HAS_WGPU) && !defined(__EMSCRIPTEN__))
#include <void_engine/presenter/backends/opengl_backend.hpp>
#define VOID_PRESENTER_USE_OPENGL 1
#endif

#include <unordered_map>
#include <mutex>

namespace void_presenter {

namespace {

// Registry of backend creators
std::mutex g_registry_mutex;
std::unordered_map<BackendType, BackendFactory::BackendCreator> g_backend_creators;
bool g_initialized = false;

void ensure_initialized() {
    if (g_initialized) return;
    g_initialized = true;

    // Register null backend (always available)
    g_backend_creators[BackendType::Null] = [](const BackendConfig&) {
        return std::make_unique<backends::NullBackend>();
    };

#if defined(VOID_HAS_WGPU)
    // Register wgpu backend
    g_backend_creators[BackendType::Wgpu] = [](const BackendConfig& config) {
        return std::make_unique<backends::WgpuBackend>(config, backends::WgpuBackendConfig{});
    };
#endif

#if defined(VOID_PRESENTER_USE_OPENGL)
    // Register OpenGL backend
    g_backend_creators[BackendType::OpenGL] = [](const BackendConfig& config) {
        return std::make_unique<backends::OpenGLBackend>(config);
    };
#endif

    // Note: OpenXR, WebGPU, WebXR backends registered separately
    // when their libraries are linked
}

} // anonymous namespace

// =============================================================================
// BackendFactory Implementation
// =============================================================================

std::vector<BackendAvailability> BackendFactory::query_available() {
    std::lock_guard lock(g_registry_mutex);
    ensure_initialized();

    std::vector<BackendAvailability> result;

    // Null backend - always available
    result.push_back({BackendType::Null, true, ""});

    // wgpu backend
#if defined(VOID_HAS_WGPU)
    if (backends::is_wgpu_available()) {
        result.push_back({BackendType::Wgpu, true, ""});
    } else {
        result.push_back({BackendType::Wgpu, false, "wgpu-native not initialized"});
    }
#else
    result.push_back({BackendType::Wgpu, false, "wgpu-native not compiled"});
#endif

    // WebGPU - only on web
#if defined(__EMSCRIPTEN__)
    result.push_back({BackendType::WebGPU, true, ""});
#else
    result.push_back({BackendType::WebGPU, false, "WebGPU only available on web platform"});
#endif

    // Vulkan
#if defined(VOID_HAS_VULKAN)
    result.push_back({BackendType::Vulkan, true, ""});
#else
    result.push_back({BackendType::Vulkan, false, "Vulkan SDK not available"});
#endif

    // D3D12
#if defined(_WIN32) && defined(VOID_HAS_D3D12)
    result.push_back({BackendType::D3D12, true, ""});
#else
    result.push_back({BackendType::D3D12, false, "D3D12 only available on Windows"});
#endif

    // Metal
#if defined(__APPLE__) && defined(VOID_HAS_METAL)
    result.push_back({BackendType::Metal, true, ""});
#else
    result.push_back({BackendType::Metal, false, "Metal only available on Apple platforms"});
#endif

    // OpenGL
#if defined(VOID_PRESENTER_USE_OPENGL)
    if (backends::is_opengl_available()) {
        result.push_back({BackendType::OpenGL, true, ""});
    } else {
        result.push_back({BackendType::OpenGL, false, "OpenGL context creation failed"});
    }
#else
    result.push_back({BackendType::OpenGL, false, "OpenGL not compiled"});
#endif

    // OpenXR
#if defined(VOID_HAS_OPENXR)
    result.push_back({BackendType::OpenXR, true, ""});
#else
    result.push_back({BackendType::OpenXR, false, "OpenXR not available"});
#endif

    // WebXR
#if defined(__EMSCRIPTEN__) && defined(VOID_HAS_WEBXR)
    result.push_back({BackendType::WebXR, true, ""});
#else
    result.push_back({BackendType::WebXR, false, "WebXR only available on web platform"});
#endif

    return result;
}

bool BackendFactory::is_available(BackendType type) {
    std::lock_guard lock(g_registry_mutex);
    ensure_initialized();

    // Check registered creators
    if (g_backend_creators.count(type)) {
        return true;
    }

    // Platform checks
    switch (type) {
        case BackendType::Null:
            return true;

        case BackendType::Wgpu:
#if defined(VOID_HAS_WGPU)
            return backends::is_wgpu_available();
#else
            return false;
#endif

        case BackendType::WebGPU:
#if defined(__EMSCRIPTEN__)
            return true;
#else
            return false;
#endif

        case BackendType::Vulkan:
#if defined(VOID_HAS_VULKAN)
            return true;
#else
            return false;
#endif

        case BackendType::D3D12:
#if defined(_WIN32)
            return true;
#else
            return false;
#endif

        case BackendType::Metal:
#if defined(__APPLE__)
            return true;
#else
            return false;
#endif

        case BackendType::OpenGL:
#if defined(VOID_PRESENTER_USE_OPENGL)
            return backends::is_opengl_available();
#else
            return false;
#endif

        case BackendType::OpenXR:
#if defined(VOID_HAS_OPENXR)
            return true;
#else
            return false;
#endif

        case BackendType::WebXR:
#if defined(__EMSCRIPTEN__)
            return true;
#else
            return false;
#endif
    }

    return false;
}

BackendType BackendFactory::recommended() {
    // Platform-specific recommendations
#if defined(__EMSCRIPTEN__)
    return BackendType::WebGPU;
#elif defined(VOID_HAS_WGPU)
    return BackendType::Wgpu;  // wgpu auto-selects best backend
#elif defined(VOID_PRESENTER_USE_OPENGL)
    return BackendType::OpenGL;  // OpenGL as primary fallback
#elif defined(_WIN32)
    return BackendType::D3D12;
#elif defined(__APPLE__)
    return BackendType::Metal;
#elif defined(VOID_HAS_VULKAN)
    return BackendType::Vulkan;
#else
    return BackendType::Null;
#endif
}

std::unique_ptr<IBackend> BackendFactory::create(const BackendConfig& config) {
    std::lock_guard lock(g_registry_mutex);
    ensure_initialized();

    BackendType type = config.preferred_type;

    // Check if preferred type is available
    auto it = g_backend_creators.find(type);
    if (it != g_backend_creators.end()) {
        auto backend = it->second(config);
        if (backend && backend->is_healthy()) {
            return backend;
        }
    }

    // Try fallbacks
    for (const auto& fallback : config.fallback_types) {
        it = g_backend_creators.find(fallback);
        if (it != g_backend_creators.end()) {
            auto backend = it->second(config);
            if (backend && backend->is_healthy()) {
                return backend;
            }
        }
    }

    // Last resort: null backend
    if (type != BackendType::Null) {
        return std::make_unique<backends::NullBackend>();
    }

    return nullptr;
}

std::unique_ptr<IBackend> BackendFactory::create_with_fallback(
    BackendType preferred,
    const std::vector<BackendType>& fallbacks) {

    BackendConfig config;
    config.preferred_type = preferred;
    config.fallback_types = fallbacks;
    return create(config);
}

std::unique_ptr<IBackend> BackendFactory::create_best_available(PowerPreference power_pref) {
    BackendConfig config;
    config.preferred_type = recommended();
    config.power_preference = power_pref;

    // Setup fallback chain
    config.fallback_types = {
        BackendType::Wgpu,
        BackendType::Vulkan,
        BackendType::D3D12,
        BackendType::Metal,
        BackendType::OpenGL,
        BackendType::Null,
    };

    return create(config);
}

void BackendFactory::register_backend(BackendType type, BackendCreator creator) {
    std::lock_guard lock(g_registry_mutex);
    ensure_initialized();
    g_backend_creators[type] = std::move(creator);
}

} // namespace void_presenter
