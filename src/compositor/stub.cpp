/// @file stub.cpp
/// @brief void_compositor module initialization and version information
///
/// void_compositor provides layer-based composition with VRR and HDR support.
/// This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook for backend registration
/// - Version information
/// - Backend availability queries

#include <void_engine/compositor/compositor_module.hpp>
#include <void_engine/compositor/compositor.hpp>
#include <void_engine/compositor/layer.hpp>
#include <void_engine/compositor/layer_compositor.hpp>
#include <void_engine/compositor/rehydration.hpp>
#include <void_engine/compositor/snapshot.hpp>

#include <string>
#include <vector>

namespace void_compositor {

/// Module version
static constexpr const char* k_version = "1.0.0";

/// Module name
static constexpr const char* k_module_name = "void_compositor";

/// Get module version string
const char* version() noexcept {
    return k_version;
}

/// Get module name
const char* module_name() noexcept {
    return k_module_name;
}

/// Backend availability info
struct BackendInfo {
    const char* name;
    const char* description;
    bool available;
    bool is_default;
};

/// Query available backends
std::vector<BackendInfo> query_backends() {
    std::vector<BackendInfo> backends;

    // Null backend is always available
    backends.push_back({
        "null",
        "Null compositor for testing",
        true,
        false
    });

    // Software layer compositor
    backends.push_back({
        "software",
        "CPU-based software layer compositor",
        true,
        true  // Default when no GPU available
    });

#if defined(VOID_HAS_DCOMP)
    backends.push_back({
        "directcomposition",
        "Windows DirectComposition backend",
        true,
        true  // Preferred on Windows
    });
#endif

#if defined(VOID_HAS_SMITHAY) || defined(__linux__)
    backends.push_back({
        "smithay",
        "Smithay Wayland compositor (DRM/KMS)",
#if defined(VOID_HAS_SMITHAY)
        true,
#else
        false,  // Not compiled in
#endif
        true  // Preferred on Linux with Smithay
    });

    backends.push_back({
        "drm",
        "Linux DRM/KMS backend",
        true,
        false
    });
#endif

#if defined(__APPLE__)
    backends.push_back({
        "coregraphics",
        "macOS Core Graphics compositor",
        true,
        true  // Preferred on macOS
    });
#endif

#if defined(VOID_HAS_WEBCANVAS) || defined(__EMSCRIPTEN__)
    backends.push_back({
        "webcanvas",
        "HTML5 Canvas/WebGL compositor",
        true,
        true  // Only option on web
    });
#endif

    return backends;
}

/// Get the recommended backend for this platform
const char* recommended_backend() {
#if defined(_WIN32)
    return "directcomposition";
#elif defined(__APPLE__)
    return "coregraphics";
#elif defined(__EMSCRIPTEN__)
    return "webcanvas";
#elif defined(__linux__)
#if defined(VOID_HAS_SMITHAY)
    return "smithay";
#else
    return "drm";
#endif
#else
    return "software";
#endif
}

/// Check if a specific backend is available
bool is_backend_available(const char* backend_name) {
    auto backends = query_backends();
    for (const auto& backend : backends) {
        if (std::string(backend.name) == backend_name) {
            return backend.available;
        }
    }
    return false;
}

/// Initialize the compositor module
/// Registers all available backends with the factory
void init() {
    // Module initialization
    // In production, this would register platform-specific backends

    // The null backend and software compositor are always available
    // via LayerCompositorFactory and CompositorFactory
}

/// Shutdown the compositor module
void shutdown() {
    // Cleanup any global resources
}

/// Get module capabilities
struct ModuleCapabilities {
    bool supports_vrr;
    bool supports_hdr;
    bool supports_layer_composition;
    bool supports_hot_reload;
    bool supports_multi_output;
    std::uint32_t max_layers;
};

ModuleCapabilities capabilities() {
    return ModuleCapabilities{
        .supports_vrr = true,
        .supports_hdr = true,
        .supports_layer_composition = true,
        .supports_hot_reload = true,
        .supports_multi_output = true,
        .max_layers = 1024,
    };
}

/// Create a layer manager with default configuration
std::unique_ptr<LayerManager> create_layer_manager() {
    return std::make_unique<LayerManager>();
}

/// Create a layer compositor with default configuration
std::unique_ptr<ILayerCompositor> create_layer_compositor(
    const LayerCompositorConfig& config) {
    return LayerCompositorFactory::create(config);
}

/// Create a compositor with default configuration
std::unique_ptr<ICompositor> create_compositor(const CompositorConfig& config) {
    return CompositorFactory::create(config);
}

} // namespace void_compositor
