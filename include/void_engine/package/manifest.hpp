#pragma once

/// @file manifest.hpp
/// @brief Package manifest definitions and JSON parsing
///
/// A PackageManifest describes a package's identity, dependencies, and metadata.
/// All package types share this base structure; type-specific data is in separate
/// manifest extensions (e.g., PluginPackageManifest, AssetBundleManifest).

#include "fwd.hpp"
#include "version.hpp"
#include <void_engine/core/error.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <chrono>

namespace void_package {

// =============================================================================
// PackageDependency
// =============================================================================

/// A dependency on another package
struct PackageDependency {
    std::string name;               ///< Package name (e.g., "core.ecs", "mod.plasma_rifle")
    VersionConstraint constraint;   ///< Version constraint (e.g., ">=1.0.0")
    bool optional = false;          ///< If true, loading continues if dependency missing
    std::string reason;             ///< Optional explanation of why dependency is needed

    /// Check if this dependency is satisfied by a given version
    [[nodiscard]] bool is_satisfied_by(const SemanticVersion& version) const noexcept {
        return constraint.satisfies(version);
    }
};

// =============================================================================
// PackageManifest
// =============================================================================

/// Base manifest for all package types
///
/// Every package file begins with:
/// ```json
/// {
///   "package": {
///     "name": "namespace.package_name",
///     "type": "world|layer|plugin|widget|asset",
///     "version": "1.0.0"
///   },
///   "dependencies": { ... }
/// }
/// ```
struct PackageManifest {
    // =========================================================================
    // Identity
    // =========================================================================

    std::string name;              ///< Package name (e.g., "gameplay.combat")
    PackageType type;              ///< Package type
    SemanticVersion version;       ///< Package version

    // =========================================================================
    // Metadata
    // =========================================================================

    std::string display_name;      ///< Human-readable name (optional)
    std::string description;       ///< Package description (optional)
    std::string author;            ///< Package author (optional)
    std::string license;           ///< License identifier (optional)
    std::string homepage;          ///< URL to package homepage (optional)
    std::string repository;        ///< URL to source repository (optional)

    // =========================================================================
    // Engine Compatibility
    // =========================================================================

    std::optional<VersionConstraint> engine_version;  ///< Required engine version

    // =========================================================================
    // Dependencies by Type
    // =========================================================================

    std::vector<PackageDependency> plugin_deps;   ///< Plugin dependencies
    std::vector<PackageDependency> widget_deps;   ///< Widget dependencies
    std::vector<PackageDependency> layer_deps;    ///< Layer dependencies
    std::vector<PackageDependency> asset_deps;    ///< Asset bundle dependencies

    // =========================================================================
    // File Information (set after loading)
    // =========================================================================

    std::filesystem::path source_path;  ///< Path to manifest file
    std::filesystem::path base_path;    ///< Base directory for relative paths

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Load manifest from a JSON file
    ///
    /// @param path Path to the manifest file (*.world.json, *.plugin.json, etc.)
    /// @return Parsed manifest or error
    [[nodiscard]] static void_core::Result<PackageManifest> load(
        const std::filesystem::path& path);

    /// Parse manifest from JSON string
    ///
    /// @param json_str JSON string content
    /// @param source_path Optional source path for error messages
    /// @return Parsed manifest or error
    [[nodiscard]] static void_core::Result<PackageManifest> from_json_string(
        const std::string& json_str,
        const std::filesystem::path& source_path = {});

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validate manifest completeness and rules
    ///
    /// Checks:
    /// - Required fields present
    /// - Package name follows naming conventions
    /// - Dependencies follow may_depend_on rules
    /// - No self-dependency
    [[nodiscard]] void_core::Result<void> validate() const;

    /// Check if this package type may depend on another type
    ///
    /// Dependency rules:
    /// - world → layer, plugin, widget, asset
    /// - layer → plugin, widget, asset
    /// - plugin → plugin (lower layer only), asset
    /// - widget → plugin, asset
    /// - asset → asset (prefer none)
    [[nodiscard]] bool may_depend_on(PackageType other_type) const noexcept;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Get all dependencies (all types combined)
    [[nodiscard]] std::vector<PackageDependency> all_dependencies() const;

    /// Get all required (non-optional) dependencies
    [[nodiscard]] std::vector<PackageDependency> required_dependencies() const;

    /// Check if package has any dependencies
    [[nodiscard]] bool has_dependencies() const noexcept {
        return !plugin_deps.empty() || !widget_deps.empty() ||
               !layer_deps.empty() || !asset_deps.empty();
    }

    /// Get namespace prefix from package name (e.g., "gameplay" from "gameplay.combat")
    [[nodiscard]] std::string namespace_prefix() const;

    /// Get short name without namespace (e.g., "combat" from "gameplay.combat")
    [[nodiscard]] std::string short_name() const;

    // =========================================================================
    // Plugin Layer Validation
    // =========================================================================

    /// Get plugin layer level from package name
    ///
    /// Layer hierarchy (dependencies flow downward only):
    /// - core.*      → 0 (foundation)
    /// - engine.*    → 1 (engine-level)
    /// - gameplay.*  → 2 (gameplay systems)
    /// - feature.*   → 3 (specific features)
    /// - mod.*       → 4 (mods/creator content)
    ///
    /// @return Layer level (0-4), or -1 if not a recognized namespace
    [[nodiscard]] int plugin_layer_level() const noexcept;

    /// Check if a plugin dependency respects layer hierarchy
    ///
    /// Returns false if this plugin depends on a higher-layer plugin
    [[nodiscard]] bool respects_plugin_layers(
        const std::string& dep_name, int dep_layer) const noexcept;
};

// =============================================================================
// Package Name Utilities
// =============================================================================

/// Check if a package name is valid
///
/// Rules:
/// - Must contain at least one dot (namespace.name)
/// - Only lowercase alphanumeric and underscores, separated by dots
/// - Cannot start or end with dot
/// - Cannot have consecutive dots
[[nodiscard]] bool is_valid_package_name(std::string_view name) noexcept;

/// Get plugin layer level from package name
///
/// @param name Package name (e.g., "core.ecs", "gameplay.combat")
/// @return Layer level (0-4), or -1 if not a recognized namespace
[[nodiscard]] int get_plugin_layer_level(std::string_view name) noexcept;

/// Extract namespace prefix from package name
[[nodiscard]] std::string_view get_namespace_prefix(std::string_view name) noexcept;

/// Check if package name matches a namespace prefix
[[nodiscard]] bool has_namespace_prefix(std::string_view name, std::string_view prefix) noexcept;

} // namespace void_package
