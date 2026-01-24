/// @file stub.cpp
/// @brief void_presenter module initialization and version information
///
/// void_presenter is primarily header-only with template implementations.
/// This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook for backend registration
/// - Version information

#include <void_engine/presenter/presenter_module.hpp>
#include <void_engine/presenter/backend.hpp>
#include <void_engine/presenter/backends/null_backend.hpp>

#if defined(VOID_HAS_OPENGL) || (!defined(VOID_HAS_WGPU) && !defined(__EMSCRIPTEN__))
#include <void_engine/presenter/backends/opengl_backend.hpp>
#define VOID_PRESENTER_USE_OPENGL 1
#endif

namespace void_presenter {

/// Get module version string
const char* version() noexcept {
    return "1.0.0";
}

/// Initialize the presenter module
/// Registers all available backends with the factory
void init() {
    // Null backend is always registered in ensure_initialized()
    // Additional backends are registered here for explicit initialization

#if defined(VOID_PRESENTER_USE_OPENGL)
    // Register OpenGL backend if available
    backends::register_opengl_backend();
#endif

#if defined(VOID_HAS_WGPU)
    // Register wgpu backend if available
    backends::register_wgpu_backend();
#endif
}

/// Query available backends
std::vector<BackendAvailability> query_backends() {
    return BackendFactory::query_available();
}

/// Get recommended backend for this platform
BackendType recommended_backend() {
    return BackendFactory::recommended();
}

/// Create the best available backend
std::unique_ptr<IBackend> create_best_backend(PowerPreference power_pref) {
    return BackendFactory::create_best_available(power_pref);
}

} // namespace void_presenter
