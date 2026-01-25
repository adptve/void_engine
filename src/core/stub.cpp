/// @file stub.cpp
/// @brief void_core module initialization and version information
///
/// void_core is primarily header-only with template implementations.
/// This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook
/// - Version information

#include <void_engine/core/core.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/plugin.hpp>
#include <void_engine/core/type_registry.hpp>
#include <void_engine/core/error.hpp>
#include <void_engine/core/handle.hpp>

namespace void_core {

/// Module version
static constexpr const char* k_version = "1.0.0";

/// Module name
static constexpr const char* k_module_name = "void_core";

/// Get module version string
const char* version() noexcept {
    return k_version;
}

/// Get module name
const char* module_name() noexcept {
    return k_module_name;
}

/// Module capabilities
struct ModuleCapabilities {
    bool supports_hot_reload;
    bool supports_plugins;
    bool supports_type_registry;
    bool supports_handle_system;
    bool supports_error_handling;
};

/// Get module capabilities
ModuleCapabilities capabilities() noexcept {
    return ModuleCapabilities{
        .supports_hot_reload = true,
        .supports_plugins = true,
        .supports_type_registry = true,
        .supports_handle_system = true,
        .supports_error_handling = true,
    };
}

/// Initialize the core module
/// This is called at engine startup
void init() {
    // Core module initialization
    // All implementations are header-only templates
    // This function provides a hook for any runtime initialization
}

/// Shutdown the core module
void shutdown() {
    // Cleanup any global resources
}

} // namespace void_core
