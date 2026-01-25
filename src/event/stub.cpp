/// @file stub.cpp
/// @brief void_event module initialization and version information
///
/// void_event is primarily header-only with template implementations.
/// This stub provides:
/// - A compilation unit for CMake library target
/// - Runtime initialization hook
/// - Version information

#include <void_engine/event/event_bus.hpp>
#include <void_engine/event/channel.hpp>
#include <void_engine/event/event.hpp>

namespace void_event {

/// Module version
static constexpr const char* k_version = "1.0.0";

/// Module name
static constexpr const char* k_module_name = "void_event";

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
    bool supports_lock_free_queue;
    bool supports_priority_events;
    bool supports_typed_events;
    bool supports_dynamic_events;
    bool supports_hot_reload;
};

/// Get module capabilities
ModuleCapabilities capabilities() noexcept {
    return ModuleCapabilities{
        .supports_lock_free_queue = true,
        .supports_priority_events = true,
        .supports_typed_events = true,
        .supports_dynamic_events = true,
        .supports_hot_reload = true,
    };
}

/// Initialize the event module
void init() {
    // Event module initialization
    // All implementations are header-only templates
    // This function provides a hook for any runtime initialization
}

/// Shutdown the event module
void shutdown() {
    // Cleanup any global resources
}

} // namespace void_event
