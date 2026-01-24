/// @file stub.cpp
/// @brief Minimal compilation unit for void_ir header-only module
///
/// void_ir is primarily header-only. This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook
/// - Version information

#include <void_engine/ir/ir.hpp>

namespace void_ir {

/// Get module version string
const char* version() noexcept {
    return "1.0.0";
}

/// Initialize the IR module
/// Header-only module - no runtime initialization needed
void init() {
    // IR module is header-only with no global state requiring initialization
}

} // namespace void_ir
