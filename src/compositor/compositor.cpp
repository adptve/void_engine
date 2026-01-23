/// @file compositor.cpp
/// @brief Compositor factory implementation

#include <void_engine/compositor/compositor.hpp>

namespace void_compositor {

std::unique_ptr<ICompositor> CompositorFactory::create(const CompositorConfig& config) {
    // Platform-specific compositor creation
    // For now, return null compositor as fallback
    // Real implementations would be:
    // - Linux: Smithay-based DRM/libinput compositor
    // - Windows: Win32 compositor with DWM integration
    // - macOS: Core Graphics compositor
    // - Web: HTML5 Canvas/WebGL compositor

#if defined(__linux__) && defined(VOID_HAS_SMITHAY)
    // Linux with Smithay: Create DRM compositor
    // return std::make_unique<SmithayCompositor>(config);
#endif

#if defined(_WIN32)
    // Windows: Could use DirectComposition or custom DWM integration
    // return std::make_unique<WindowsCompositor>(config);
#endif

#if defined(__APPLE__)
    // macOS: Could use Core Graphics or Metal compositor
    // return std::make_unique<MacOSCompositor>(config);
#endif

#if defined(__EMSCRIPTEN__)
    // Web: WebGL/Canvas compositor
    // return std::make_unique<WebCompositor>(config);
#endif

    // Fallback to null compositor
    return std::make_unique<NullCompositor>(config);
}

bool CompositorFactory::is_available() {
#if defined(__linux__) && defined(VOID_HAS_SMITHAY)
    return true;
#elif defined(_WIN32)
    return true;
#elif defined(__APPLE__)
    return true;
#elif defined(__EMSCRIPTEN__)
    return true;
#else
    return true; // Null compositor is always available
#endif
}

const char* CompositorFactory::backend_name() {
#if defined(__linux__) && defined(VOID_HAS_SMITHAY)
    return "Smithay (DRM/KMS)";
#elif defined(_WIN32)
    return "Windows (DirectComposition)";
#elif defined(__APPLE__)
    return "macOS (Core Graphics)";
#elif defined(__EMSCRIPTEN__)
    return "Web (Canvas/WebGL)";
#else
    return "Null (Testing)";
#endif
}

} // namespace void_compositor
