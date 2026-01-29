#pragma once

/// @file plugin_package.hpp
/// @brief Plugin package manifest definitions for runtime component/system loading
///
/// Plugin packages enable external content to extend the engine with:
/// - Components declared in JSON, not C++ headers
/// - Systems loaded from dynamic libraries (.dll/.so)
/// - Event handlers registered by name
/// - Data-driven registries for custom game data
///
/// This supports zero compile-time knowledge of external plugin components.

#include "fwd.hpp"
#include "manifest.hpp"
#include "component_schema.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/system.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <filesystem>

namespace void_package {

// =============================================================================
// FieldDeclaration
// =============================================================================

/// Declaration of a component field from JSON
struct FieldDeclaration {
    std::string name;                            ///< Field name
    std::string type;                            ///< Type string (f32, vec3, Entity, etc.)
    std::optional<nlohmann::json> default_value; ///< Default value if not specified
    bool required = false;                       ///< Whether field is required in instances
    std::string description;                     ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<FieldDeclaration> from_json(
        const std::string& field_name,
        const nlohmann::json& j);

    /// Convert to FieldSchema for ComponentSchemaRegistry
    [[nodiscard]] void_core::Result<FieldSchema> to_field_schema() const;
};

// =============================================================================
// ComponentDeclaration
// =============================================================================

/// Declaration of a component type from plugin manifest
///
/// Example JSON:
/// ```json
/// {
///   "name": "Health",
///   "fields": {
///     "current": { "type": "f32", "default": 100.0 },
///     "max": { "type": "f32", "default": 100.0 },
///     "regeneration": { "type": "f32", "default": 0.0 }
///   }
/// }
/// ```
struct ComponentDeclaration {
    std::string name;                            ///< Component name (e.g., "Health")
    std::map<std::string, FieldDeclaration> fields; ///< Field definitions
    bool is_tag = false;                         ///< Tag component (no data)
    std::string description;                     ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<ComponentDeclaration> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Convert to ComponentSchema for registration
    [[nodiscard]] void_core::Result<ComponentSchema> to_component_schema(
        const std::string& plugin_name) const;
};

// =============================================================================
// SystemDeclaration
// =============================================================================

/// Declaration of a system from plugin manifest
///
/// Example JSON:
/// ```json
/// {
///   "name": "MovementSystem",
///   "stage": "update",
///   "query": ["Transform", "Velocity"],
///   "library": "plugins/movement.dll",
///   "entry_point": "movement_system_run"
/// }
/// ```
struct SystemDeclaration {
    std::string name;                            ///< System name
    std::string stage;                           ///< Execution stage (first, pre_update, update, etc.)
    std::vector<std::string> query;              ///< Component names for query
    std::vector<std::string> exclude;            ///< Components to exclude from query
    std::string library;                         ///< Path to dynamic library (relative to package)
    std::string entry_point;                     ///< Function name in library
    std::vector<std::string> run_after;          ///< Systems to run after
    std::vector<std::string> run_before;         ///< Systems to run before
    bool exclusive = false;                      ///< Cannot run in parallel
    std::string description;                     ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<SystemDeclaration> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Convert stage string to SystemStage enum
    [[nodiscard]] static void_core::Result<void_ecs::SystemStage> parse_stage(const std::string& stage_str);

    /// Get SystemStage from declaration
    [[nodiscard]] void_core::Result<void_ecs::SystemStage> get_stage() const;
};

// =============================================================================
// EventHandlerDeclaration
// =============================================================================

/// Declaration of an event handler from plugin manifest
///
/// Example JSON:
/// ```json
/// {
///   "event": "EntityDamaged",
///   "handler": "on_entity_damaged",
///   "library": "plugins/combat.dll",
///   "priority": 100
/// }
/// ```
struct EventHandlerDeclaration {
    std::string event;                           ///< Event type name
    std::string handler;                         ///< Handler function name
    std::string library;                         ///< Path to dynamic library
    int priority = 0;                            ///< Handler priority (higher runs first)
    std::string description;                     ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<EventHandlerDeclaration> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// RegistryDeclaration
// =============================================================================

/// Declaration of a data registry from plugin manifest
///
/// Example JSON:
/// ```json
/// {
///   "name": "weapons",
///   "collision_policy": "last_wins",
///   "schema": "schemas/weapon.json"
/// }
/// ```
struct RegistryDeclaration {
    std::string name;                            ///< Registry name (e.g., "weapons")
    std::string collision_policy;                ///< How to handle duplicate IDs
    std::optional<std::string> schema_path;      ///< Optional JSON schema for validation
    bool allow_dynamic_fields = true;            ///< Allow fields not in schema
    std::string description;                     ///< Documentation

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<RegistryDeclaration> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Convert to RegistryTypeConfig
    [[nodiscard]] void_core::Result<RegistryTypeConfig> to_registry_config() const;
};

// =============================================================================
// PluginPackageManifest
// =============================================================================

/// Complete manifest for a plugin package
///
/// Extends PackageManifest with plugin-specific declarations:
/// - Components defined in JSON
/// - Systems loaded from dynamic libraries
/// - Event handlers for engine events
/// - Data registries for game content
///
/// Example manifest:
/// ```json
/// {
///   "package": {
///     "name": "gameplay.combat",
///     "type": "plugin",
///     "version": "1.0.0"
///   },
///   "dependencies": {
///     "plugins": ["core.ecs"]
///   },
///   "components": [
///     { "name": "Health", "fields": { ... } }
///   ],
///   "systems": [
///     { "name": "DamageSystem", "stage": "update", ... }
///   ],
///   "event_handlers": [
///     { "event": "EntityDamaged", "handler": "on_damage", ... }
///   ],
///   "registries": [
///     { "name": "weapons", ... }
///   ]
/// }
/// ```
struct PluginPackageManifest {
    // Base manifest (identity, dependencies, etc.)
    PackageManifest base;

    // Plugin-specific declarations
    std::vector<ComponentDeclaration> components;
    std::vector<SystemDeclaration> systems;
    std::vector<EventHandlerDeclaration> event_handlers;
    std::vector<RegistryDeclaration> registries;

    // Library paths (resolved relative to package)
    std::vector<std::filesystem::path> libraries;

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Load plugin manifest from JSON file
    [[nodiscard]] static void_core::Result<PluginPackageManifest> load(
        const std::filesystem::path& path);

    /// Parse from JSON string
    [[nodiscard]] static void_core::Result<PluginPackageManifest> from_json_string(
        const std::string& json_str,
        const std::filesystem::path& source_path = {});

    /// Parse from JSON object (after base manifest is parsed)
    [[nodiscard]] static void_core::Result<PluginPackageManifest> from_json(
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

    /// Validate plugin-specific rules
    [[nodiscard]] void_core::Result<void> validate() const;

    /// Check if this plugin declares a component
    [[nodiscard]] bool has_component(const std::string& name) const;

    /// Check if this plugin declares a system
    [[nodiscard]] bool has_system(const std::string& name) const;

    /// Get component declaration by name
    [[nodiscard]] const ComponentDeclaration* get_component(const std::string& name) const;

    /// Get system declaration by name
    [[nodiscard]] const SystemDeclaration* get_system(const std::string& name) const;

    // =========================================================================
    // Library Resolution
    // =========================================================================

    /// Get all unique library paths used by systems and event handlers
    [[nodiscard]] std::vector<std::filesystem::path> collect_library_paths() const;

    /// Resolve a library path relative to the package base path
    [[nodiscard]] std::filesystem::path resolve_library_path(
        const std::string& lib_path) const;
};

// =============================================================================
// System Stage Utilities
// =============================================================================

/// Convert SystemStage enum to string
[[nodiscard]] const char* system_stage_to_string(void_ecs::SystemStage stage) noexcept;

/// Parse SystemStage from string (case-insensitive)
[[nodiscard]] bool system_stage_from_string(const std::string& str, void_ecs::SystemStage& out) noexcept;

} // namespace void_package
