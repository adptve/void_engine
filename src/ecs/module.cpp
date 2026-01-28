/// @file module.cpp
/// @brief Module linkage for void_ecs (header-only ECS)
///
/// void_ecs is a high-performance, archetype-based ECS implemented entirely
/// in C++20 templates for maximum performance through compile-time optimization.
///
/// This file provides:
/// - Module version information
/// - Exported symbols for library linkage
/// - Any non-template utilities
///
/// The header-only design is intentional for ECS because:
/// - All component storage is templated on component types
/// - Query iteration is templated for type-safe access
/// - Compiler can fully inline and optimize for specific archetypes
///
/// Similar to industry-standard ECS libraries (EnTT, flecs headers).

#include <void_engine/ecs/ecs.hpp>
#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/snapshot.hpp>

namespace void_ecs {

// =============================================================================
// Module Information
// =============================================================================

/// Module version string
const char* version() noexcept {
    return "1.0.0";
}

/// Module name
const char* module_name() noexcept {
    return "void_ecs";
}

/// Module description
const char* module_description() noexcept {
    return "High-performance archetype-based Entity Component System";
}

// =============================================================================
// Module Initialization
// =============================================================================

/// Initialize the ECS module
/// @note Header-only module - no runtime initialization needed
void init() {
    // No runtime initialization required for header-only ECS
    // All storage and iteration is compile-time templated
}

/// Shutdown the ECS module
void shutdown() {
    // No cleanup required for header-only ECS
}

// =============================================================================
// Utility Functions (non-template helpers)
// =============================================================================

/// Get recommended initial entity capacity based on game type
std::size_t recommended_capacity(const char* game_type) noexcept {
    // Small indie game
    if (game_type && std::string_view(game_type) == "small") {
        return 1000;
    }
    // Medium game
    if (game_type && std::string_view(game_type) == "medium") {
        return 10000;
    }
    // Large open world
    if (game_type && std::string_view(game_type) == "large") {
        return 100000;
    }
    // AAA scale
    if (game_type && std::string_view(game_type) == "aaa") {
        return 1000000;
    }
    // Default
    return 10000;
}

} // namespace void_ecs
