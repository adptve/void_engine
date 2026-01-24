/// @file stub.cpp
/// @brief Minimal compilation unit for void_services header-only module
///
/// void_services is primarily header-only. This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook
/// - Version information

#include <void_engine/services/services.hpp>

namespace void_services {

/// Get module version string
const char* version() noexcept {
    return "1.0.0";
}

/// Initialize the services module
/// Header-only module - no runtime initialization needed
void init() {
    // Services module is header-only with no global state requiring initialization
}

} // namespace void_services
