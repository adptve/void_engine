/// @file stub.cpp
/// @brief Minimal implementation stub for void_ecs module
///
/// void_ecs is primarily header-only via C++20 templates.
/// This file provides minimal exported symbols for module linkage.

#include <void_engine/ecs/ecs.hpp>

namespace void_ecs {

/// Module version
const char* version() noexcept {
    return "1.0.0";
}

/// Initialize the ECS module (no-op for header-only implementation)
void init() {
    // Header-only module - no runtime initialization needed
}

} // namespace void_ecs
