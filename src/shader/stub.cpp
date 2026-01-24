/// @file stub.cpp
/// @brief Minimal compilation unit for void_shader header-only module
///
/// void_shader is primarily header-only. This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook
/// - Version information

#include <void_engine/shader/shader.hpp>

namespace void_shader {

/// Get module version string
const char* version() noexcept {
    return "1.0.0";
}

/// Initialize the shader module
/// Header-only module - no runtime initialization needed
void init() {
    // Shader module is header-only with no global state requiring initialization
}

} // namespace void_shader
