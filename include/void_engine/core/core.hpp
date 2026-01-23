#pragma once

/// @file core.hpp
/// @brief Main include file for void_core module
///
/// This header includes all void_core components in dependency order.

// Forward declarations
#include "fwd.hpp"

// Core types (no dependencies on other void_core headers)
#include "error.hpp"
#include "version.hpp"
#include "id.hpp"
#include "handle.hpp"

// Type system
#include "type_registry.hpp"

// Plugin system
#include "plugin.hpp"

// Hot-reload infrastructure
#include "hot_reload.hpp"

/// @namespace void_core
/// @brief Core engine infrastructure module
///
/// void_core provides foundational types and systems used throughout
/// the void_engine. Key components include:
///
/// - **Error Handling**: Result<T> monadic error handling
/// - **Versioning**: Semantic versioning with compatibility checks
/// - **Identifiers**: Generational IDs and named identifiers
/// - **Handles**: Type-safe generational handles with allocation
/// - **Type Registry**: Runtime type information and dynamic types
/// - **Plugin System**: Plugin lifecycle management
/// - **Hot-Reload**: State preservation across code reloads
///
/// Example usage:
/// @code
/// #include <void_engine/core/core.hpp>
///
/// using namespace void_core;
///
/// // Create a result
/// Result<int> divide(int a, int b) {
///     if (b == 0) {
///         return Err<int>("Division by zero");
///     }
///     return Ok(a / b);
/// }
///
/// // Use handles
/// HandleMap<MyEntity> entities;
/// Handle<MyEntity> h = entities.insert(MyEntity{});
/// if (auto* entity = entities.get_mut(h)) {
///     entity->update();
/// }
///
/// // Register types
/// TypeRegistry registry;
/// registry.register_with_name<MyComponent>("MyComponent");
///
/// // Create plugins
/// class MyPlugin : public Plugin {
/// public:
///     VOID_DEFINE_PLUGIN(MyPlugin, "my_plugin", 1, 0, 0)
///
///     Result<void> on_load(PluginContext& ctx) override {
///         // Initialize plugin
///         return Ok();
///     }
///
///     Result<PluginState> on_unload(PluginContext& ctx) override {
///         // Cleanup plugin
///         return Ok(PluginState::empty());
///     }
/// };
/// @endcode

namespace void_core {

/// Library version
inline constexpr Version VOID_CORE_VERSION{0, 1, 0};

/// Get library version string
[[nodiscard]] inline std::string void_core_version_string() {
    return "void_core " + VOID_CORE_VERSION.to_string();
}

} // namespace void_core
