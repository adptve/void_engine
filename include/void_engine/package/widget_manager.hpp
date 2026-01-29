#pragma once

/// @file widget_manager.hpp
/// @brief Widget lifecycle management
///
/// The WidgetManager is responsible for:
/// - Creating widgets from declarations
/// - Setting up ECS bindings (queries, resources)
/// - Updating widgets each frame
/// - Clean destruction and unloading
///
/// Widgets are loadable from external sources, with ECS bindings
/// resolved at runtime by component name.

#include "widget.hpp"
#include "widget_package.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace void_ecs { class World; class QueryState; class QueryDescriptor; }

namespace void_package {

// Forward declarations
class ComponentSchemaRegistry;
class DynamicLibraryCache;

// =============================================================================
// WidgetInstance
// =============================================================================

/// Internal representation of a managed widget
struct WidgetInstance {
    WidgetHandle handle;
    std::string id;
    std::string type;
    std::string source_package;
    std::unique_ptr<Widget> widget;
    WidgetContext context;
    bool initialized = false;
    std::vector<std::unique_ptr<void_ecs::QueryState>> owned_queries;
};

// =============================================================================
// WidgetManager
// =============================================================================

/// Manager for widget lifecycle and ECS integration
///
/// Key responsibilities:
/// - Widget creation based on type (built-in or from library)
/// - ECS query binding from component names
/// - Resource binding
/// - Frame update coordination
/// - Clean unloading
///
/// Thread-safety: NOT thread-safe. Must be accessed from main thread.
class WidgetManager {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    WidgetManager();
    ~WidgetManager();

    // Non-copyable
    WidgetManager(const WidgetManager&) = delete;
    WidgetManager& operator=(const WidgetManager&) = delete;

    // Movable
    WidgetManager(WidgetManager&&) noexcept;
    WidgetManager& operator=(WidgetManager&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Set the ECS world for queries and resources
    void set_ecs_world(void_ecs::World* world) noexcept { m_ecs_world = world; }

    /// Get the ECS world
    [[nodiscard]] void_ecs::World* ecs_world() const noexcept { return m_ecs_world; }

    /// Set the component schema registry for name-to-ID resolution
    void set_schema_registry(ComponentSchemaRegistry* registry) noexcept {
        m_schema_registry = registry;
    }

    /// Get the widget type registry
    [[nodiscard]] WidgetTypeRegistry& type_registry() noexcept { return m_type_registry; }
    [[nodiscard]] const WidgetTypeRegistry& type_registry() const noexcept { return m_type_registry; }

    /// Set the library cache for loading widget types from DLLs
    void set_library_cache(DynamicLibraryCache* cache) noexcept { m_library_cache = cache; }

    // =========================================================================
    // Widget Registration
    // =========================================================================

    /// Register a widget from a declaration
    ///
    /// Creates the widget but does not initialize it.
    /// Call init_widget() or init_all() to initialize.
    ///
    /// @param decl Widget declaration from manifest
    /// @param source_package Name of the package that declared this widget
    /// @return Handle to the registered widget, or Error
    [[nodiscard]] void_core::Result<WidgetHandle> register_widget(
        const WidgetDeclaration& decl,
        const std::string& source_package = {});

    /// Register and immediately initialize a widget
    [[nodiscard]] void_core::Result<WidgetHandle> register_and_init_widget(
        const WidgetDeclaration& decl,
        const std::string& source_package = {});

    // =========================================================================
    // Widget Creation
    // =========================================================================

    /// Create a widget by ID from a registered declaration
    ///
    /// @param id Widget ID
    /// @return Handle to created widget, or Error
    [[nodiscard]] void_core::Result<WidgetHandle> create_widget(const std::string& id);

    /// Create a widget directly by type and config
    ///
    /// @param type Widget type name
    /// @param config Widget configuration
    /// @return Handle to created widget, or Error
    [[nodiscard]] void_core::Result<WidgetHandle> create_widget(
        const std::string& type,
        const nlohmann::json& config);

    // =========================================================================
    // Widget Destruction
    // =========================================================================

    /// Destroy a widget by handle
    ///
    /// Calls shutdown() on the widget and releases all resources.
    ///
    /// @param handle Widget handle
    /// @return Ok on success, Error if handle invalid
    [[nodiscard]] void_core::Result<void> destroy_widget(WidgetHandle handle);

    /// Destroy a widget by ID
    [[nodiscard]] void_core::Result<void> destroy_widget(const std::string& id);

    /// Destroy all widgets from a specific package
    void destroy_widgets_from_package(const std::string& package_name);

    /// Destroy all widgets
    void destroy_all_widgets();

    // =========================================================================
    // ECS Binding
    // =========================================================================

    /// Bind a widget to an ECS query
    ///
    /// The query is built from component names, resolved at runtime.
    ///
    /// @param widget_id Widget to bind
    /// @param query_descriptor Pre-built query descriptor
    /// @param binding_name Name for this binding (for lookup in widget)
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> bind_to_query(
        const std::string& widget_id,
        void_ecs::QueryDescriptor query_descriptor,
        const std::string& binding_name = "default");

    /// Bind a widget to an ECS query by component names
    ///
    /// Component names are resolved to IDs via ComponentSchemaRegistry.
    ///
    /// @param widget_id Widget to bind
    /// @param component_names List of component names
    /// @param binding_name Name for this binding
    /// @return Ok on success, Error if any component unknown
    [[nodiscard]] void_core::Result<void> bind_to_query_by_names(
        const std::string& widget_id,
        const std::vector<std::string>& component_names,
        const std::string& binding_name = "default");

    /// Bind a widget to an ECS resource
    ///
    /// @param widget_id Widget to bind
    /// @param resource_name Name of the resource
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> bind_to_resource(
        const std::string& widget_id,
        const std::string& resource_name);

    /// Apply all bindings from a WidgetBinding specification
    [[nodiscard]] void_core::Result<void> apply_binding(const WidgetBinding& binding);

    // =========================================================================
    // Widget Lifecycle
    // =========================================================================

    /// Initialize a widget
    [[nodiscard]] void_core::Result<void> init_widget(WidgetHandle handle);

    /// Initialize a widget by ID
    [[nodiscard]] void_core::Result<void> init_widget(const std::string& id);

    /// Initialize all registered widgets
    [[nodiscard]] void_core::Result<void> init_all();

    /// Shutdown a widget
    [[nodiscard]] void_core::Result<void> shutdown_widget(WidgetHandle handle);

    /// Shutdown all widgets
    void shutdown_all();

    // =========================================================================
    // Frame Update
    // =========================================================================

    /// Update all active widgets
    ///
    /// Calls update() on all enabled widgets.
    ///
    /// @param dt Delta time in seconds
    void update_all(float dt);

    /// Render all visible widgets
    ///
    /// Calls render() on all visible widgets.
    void render_all();

    // =========================================================================
    // Widget Access
    // =========================================================================

    /// Get a widget by handle
    [[nodiscard]] Widget* get_widget(WidgetHandle handle);
    [[nodiscard]] const Widget* get_widget(WidgetHandle handle) const;

    /// Get a widget by ID
    [[nodiscard]] Widget* get_widget(const std::string& id);
    [[nodiscard]] const Widget* get_widget(const std::string& id) const;

    /// Get widget handle by ID
    [[nodiscard]] std::optional<WidgetHandle> get_handle(const std::string& id) const;

    /// Check if a widget exists
    [[nodiscard]] bool has_widget(const std::string& id) const;

    /// Check if a handle is valid
    [[nodiscard]] bool is_valid_handle(WidgetHandle handle) const;

    /// Get all widget IDs
    [[nodiscard]] std::vector<std::string> all_widget_ids() const;

    /// Get widgets from a specific package
    [[nodiscard]] std::vector<std::string> widgets_from_package(const std::string& package_name) const;

    /// Get widget count
    [[nodiscard]] std::size_t widget_count() const noexcept { return m_widgets.size(); }

    // =========================================================================
    // Widget Type Registration
    // =========================================================================

    /// Register a widget type from a library
    ///
    /// @param decl Library declaration
    /// @return Ok on success, Error if library/factory not found
    [[nodiscard]] void_core::Result<void> register_widget_type_from_library(
        const WidgetLibraryDeclaration& decl);

    // =========================================================================
    // Visibility Control
    // =========================================================================

    /// Toggle widget visibility
    void toggle_widget(const std::string& id);

    /// Set widget visibility
    void set_widget_visible(const std::string& id, bool visible);

    /// Toggle widget by toggle key
    ///
    /// @param key_name Key name (e.g., "F3", "tilde")
    void handle_toggle_key(const std::string& key_name);

    /// Register a toggle key mapping
    void register_toggle_key(const std::string& key_name, const std::string& widget_id);

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Get manager state as formatted string
    [[nodiscard]] std::string format_state() const;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// Allocate a new widget handle
    [[nodiscard]] WidgetHandle allocate_handle();

    /// Get widget instance by handle
    [[nodiscard]] WidgetInstance* get_instance(WidgetHandle handle);
    [[nodiscard]] const WidgetInstance* get_instance(WidgetHandle handle) const;

    /// Get widget instance by ID
    [[nodiscard]] WidgetInstance* get_instance_by_id(const std::string& id);
    [[nodiscard]] const WidgetInstance* get_instance_by_id(const std::string& id) const;

    // =========================================================================
    // Data Members
    // =========================================================================

    // Widget storage
    std::vector<std::unique_ptr<WidgetInstance>> m_widgets;
    std::vector<std::uint32_t> m_free_indices;
    std::uint32_t m_next_generation = 1;

    // Lookup tables
    std::map<std::string, WidgetHandle> m_id_to_handle;
    std::map<std::string, std::string> m_toggle_key_to_widget;

    // External dependencies
    void_ecs::World* m_ecs_world = nullptr;
    ComponentSchemaRegistry* m_schema_registry = nullptr;
    DynamicLibraryCache* m_library_cache = nullptr;

    // Widget type registry
    WidgetTypeRegistry m_type_registry;
};

// =============================================================================
// Factory Function
// =============================================================================

/// Create a widget manager
[[nodiscard]] std::unique_ptr<WidgetManager> create_widget_manager();

} // namespace void_package
