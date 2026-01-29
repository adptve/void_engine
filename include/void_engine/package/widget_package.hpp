#pragma once

/// @file widget_package.hpp
/// @brief Widget package manifest definitions
///
/// Widget packages enable external UI/tooling/overlays to be loaded:
/// - Debug HUDs from mods
/// - Custom gameplay HUDs
/// - Profiling tools
/// - Entity inspectors
///
/// ECS bindings are specified by component NAME and resolved at runtime,
/// enabling widgets from external sources that don't know component IDs.

#include "fwd.hpp"
#include "manifest.hpp"
#include "widget.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace void_package {

// =============================================================================
// BuildType
// =============================================================================

/// Build type for filtering widgets
enum class BuildType : std::uint8_t {
    Debug,       ///< Debug builds only
    Development, ///< Development builds
    Profile,     ///< Profile/instrumented builds
    Release      ///< Release/shipping builds
};

/// Convert BuildType to string
[[nodiscard]] const char* build_type_to_string(BuildType type) noexcept;

/// Parse BuildType from string (case-insensitive)
[[nodiscard]] bool build_type_from_string(const std::string& str, BuildType& out) noexcept;

/// Get current build type (compile-time constant)
[[nodiscard]] constexpr BuildType current_build_type() noexcept {
#ifdef NDEBUG
#ifdef VOID_PROFILE_BUILD
    return BuildType::Profile;
#else
    return BuildType::Release;
#endif
#else
#ifdef VOID_DEVELOPMENT_BUILD
    return BuildType::Development;
#else
    return BuildType::Debug;
#endif
#endif
}

// =============================================================================
// WidgetDeclaration
// =============================================================================

/// Declaration of a widget from manifest
///
/// Example JSON:
/// ```json
/// {
///   "id": "debug_fps",
///   "type": "debug_hud",
///   "enabled_in_builds": ["debug", "development"],
///   "toggle_key": "F3",
///   "config": {
///     "show_fps": true,
///     "show_memory": false
///   }
/// }
/// ```
struct WidgetDeclaration {
    std::string id;                              ///< Unique widget ID
    std::string type;                            ///< Widget type (e.g., "debug_hud", "console")
    std::vector<BuildType> enabled_in_builds;   ///< Build types where widget is enabled
    std::optional<std::string> toggle_key;      ///< Key to toggle visibility (optional)
    nlohmann::json config;                       ///< Widget-specific configuration
    bool initially_visible = true;              ///< Whether widget starts visible
    std::string description;                    ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WidgetDeclaration> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Check if widget should be enabled in current build
    [[nodiscard]] bool is_enabled_in_current_build() const noexcept;

    /// Check if widget should be enabled for a specific build type
    [[nodiscard]] bool is_enabled_for_build(BuildType build) const noexcept;
};

// =============================================================================
// BindingType
// =============================================================================

/// Type of data binding for a widget
enum class BindingType : std::uint8_t {
    Query,    ///< Bind to an ECS query (component-based)
    Resource, ///< Bind to an ECS resource by name
    Event     ///< Subscribe to an event stream
};

/// Convert BindingType to string
[[nodiscard]] const char* binding_type_to_string(BindingType type) noexcept;

/// Parse BindingType from string
[[nodiscard]] bool binding_type_from_string(const std::string& str, BindingType& out) noexcept;

// =============================================================================
// WidgetBinding
// =============================================================================

/// Data binding for a widget
///
/// Bindings connect widgets to ECS data by name, resolved at load time.
///
/// Example JSON for query binding:
/// ```json
/// {
///   "widget_id": "health_bar",
///   "data_source": "query:Player,Health",
///   "binding_type": "query"
/// }
/// ```
///
/// Example JSON for resource binding:
/// ```json
/// {
///   "widget_id": "match_timer",
///   "data_source": "resource:MatchTimer",
///   "binding_type": "resource"
/// }
/// ```
struct WidgetBinding {
    std::string widget_id;      ///< Widget to bind
    std::string data_source;    ///< Data source specification
    BindingType binding_type;   ///< Type of binding
    bool read_only = true;      ///< Whether binding is read-only
    std::string alias;          ///< Optional alias for accessing in widget

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WidgetBinding> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Parse query binding components
    ///
    /// For data_source = "query:Player,Health", returns ["Player", "Health"]
    [[nodiscard]] std::vector<std::string> parse_query_components() const;

    /// Get resource name for resource bindings
    ///
    /// For data_source = "resource:MatchTimer", returns "MatchTimer"
    [[nodiscard]] std::string parse_resource_name() const;

    /// Get event name for event bindings
    ///
    /// For data_source = "event:PlayerDamaged", returns "PlayerDamaged"
    [[nodiscard]] std::string parse_event_name() const;
};

// =============================================================================
// WidgetLibraryDeclaration
// =============================================================================

/// Declaration of a widget type from a dynamic library
///
/// Allows external packages to provide new widget types.
///
/// Example JSON:
/// ```json
/// {
///   "type_name": "custom_minimap",
///   "library": "plugins/minimap.dll",
///   "factory": "create_minimap_widget"
/// }
/// ```
struct WidgetLibraryDeclaration {
    std::string type_name;    ///< Widget type name to register
    std::string library;      ///< Path to dynamic library
    std::string factory;      ///< Factory function name

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WidgetLibraryDeclaration> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// WidgetPackageManifest
// =============================================================================

/// Complete manifest for a widget package
///
/// Extends PackageManifest with widget-specific declarations:
/// - Widget instances to create
/// - Data bindings (queries, resources, events)
/// - Custom widget types from libraries
///
/// Example manifest:
/// ```json
/// {
///   "package": {
///     "name": "ui.debug_tools",
///     "type": "widget",
///     "version": "1.0.0"
///   },
///   "dependencies": {
///     "plugins": [
///       { "name": "core.ecs", "version": ">=1.0.0" }
///     ]
///   },
///   "widgets": [
///     {
///       "id": "fps_counter",
///       "type": "debug_hud",
///       "enabled_in_builds": ["debug", "development"],
///       "toggle_key": "F3"
///     }
///   ],
///   "bindings": [
///     {
///       "widget_id": "health_display",
///       "data_source": "query:Player,Health",
///       "binding_type": "query"
///     }
///   ],
///   "widget_types": [
///     {
///       "type_name": "custom_radar",
///       "library": "widgets/radar.dll",
///       "factory": "create_radar_widget"
///     }
///   ]
/// }
/// ```
struct WidgetPackageManifest {
    // Base manifest (identity, dependencies, etc.)
    PackageManifest base;

    // Widget declarations
    std::vector<WidgetDeclaration> widgets;
    std::vector<WidgetBinding> bindings;
    std::vector<WidgetLibraryDeclaration> widget_types;

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Load widget manifest from JSON file
    [[nodiscard]] static void_core::Result<WidgetPackageManifest> load(
        const std::filesystem::path& path);

    /// Parse from JSON string
    [[nodiscard]] static void_core::Result<WidgetPackageManifest> from_json_string(
        const std::string& json_str,
        const std::filesystem::path& source_path = {});

    /// Parse from JSON object (after base manifest is parsed)
    [[nodiscard]] static void_core::Result<WidgetPackageManifest> from_json(
        const nlohmann::json& j,
        PackageManifest base_manifest);

    // =========================================================================
    // Serialization
    // =========================================================================

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validate widget-specific rules
    [[nodiscard]] void_core::Result<void> validate() const;

    /// Check if this manifest declares a widget
    [[nodiscard]] bool has_widget(const std::string& widget_id) const;

    /// Get widget declaration by ID
    [[nodiscard]] const WidgetDeclaration* get_widget(const std::string& widget_id) const;

    /// Get bindings for a widget
    [[nodiscard]] std::vector<const WidgetBinding*> get_bindings_for(const std::string& widget_id) const;

    // =========================================================================
    // Build Filtering
    // =========================================================================

    /// Get widgets enabled for current build
    [[nodiscard]] std::vector<const WidgetDeclaration*> widgets_for_current_build() const;

    /// Get widgets enabled for specific build type
    [[nodiscard]] std::vector<const WidgetDeclaration*> widgets_for_build(BuildType build) const;

    // =========================================================================
    // Library Resolution
    // =========================================================================

    /// Get all unique library paths for custom widget types
    [[nodiscard]] std::vector<std::filesystem::path> collect_library_paths() const;

    /// Resolve a library path relative to the package base path
    [[nodiscard]] std::filesystem::path resolve_library_path(
        const std::string& lib_path) const;
};

} // namespace void_package
