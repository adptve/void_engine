#pragma once

/// @file resolver.hpp
/// @brief Package dependency resolution
///
/// The PackageResolver performs:
/// - Topological sorting of packages for correct load order
/// - Cycle detection with clear error messages
/// - Plugin layer validation (core < engine < gameplay < feature < mod)
/// - Version constraint satisfaction

#include "fwd.hpp"
#include "manifest.hpp"
#include <void_engine/core/error.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <optional>

namespace void_package {

// =============================================================================
// ResolvedPackage
// =============================================================================

/// A package with resolved dependencies and load path
struct ResolvedPackage {
    PackageManifest manifest;                    ///< The package manifest
    std::filesystem::path path;                  ///< Path to package directory
    std::vector<std::string> resolved_deps;      ///< Dependency names in load order
    std::vector<std::string> missing_optional;   ///< Optional deps that weren't found

    /// Get full path to a file within the package
    [[nodiscard]] std::filesystem::path resolve_path(
        const std::filesystem::path& relative) const {
        return path / relative;
    }
};

// =============================================================================
// Dependency Resolution Errors
// =============================================================================

/// Detailed information about a dependency cycle
struct DependencyCycle {
    std::vector<std::string> cycle_path;  ///< Package names forming the cycle

    /// Format cycle as readable string
    [[nodiscard]] std::string format() const;
};

/// Detailed information about a missing dependency
struct MissingDependency {
    std::string package_name;       ///< Package that has the dependency
    std::string dependency_name;    ///< Name of missing dependency
    VersionConstraint constraint;   ///< Required version
    bool is_optional;               ///< Whether dependency was optional

    /// Format as readable string
    [[nodiscard]] std::string format() const;
};

/// Detailed information about a version conflict
struct VersionConflict {
    std::string dependency_name;                    ///< Package with conflicting requirements
    std::vector<std::string> requiring_packages;    ///< Packages that require it
    std::vector<VersionConstraint> constraints;     ///< Their version constraints
    std::optional<SemanticVersion> available;       ///< Available version (if any)

    /// Format as readable string
    [[nodiscard]] std::string format() const;
};

/// Detailed information about a plugin layer violation
struct LayerViolation {
    std::string package_name;     ///< Plugin that violates hierarchy
    int package_layer;            ///< Its layer level
    std::string dependency_name;  ///< Dependency it's trying to use
    int dependency_layer;         ///< Dependency's layer level

    /// Format as readable string
    [[nodiscard]] std::string format() const;
};

// =============================================================================
// PackageResolver
// =============================================================================

/// Resolves package dependencies and produces load order
///
/// The resolver maintains a registry of available packages and can resolve
/// a package name into a complete list of packages to load in order.
///
/// Thread-safety: The resolver is NOT thread-safe. External synchronization
/// is required for concurrent access.
class PackageResolver {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    PackageResolver() = default;

    // Non-copyable, movable
    PackageResolver(const PackageResolver&) = delete;
    PackageResolver& operator=(const PackageResolver&) = delete;
    PackageResolver(PackageResolver&&) = default;
    PackageResolver& operator=(PackageResolver&&) = default;

    // =========================================================================
    // Package Registration
    // =========================================================================

    /// Add an available package to the resolver
    ///
    /// @param manifest The package manifest
    /// @param path Path to package directory
    void add_available(PackageManifest manifest, std::filesystem::path path);

    /// Remove a package from the resolver
    ///
    /// @param name Package name to remove
    /// @return true if package was found and removed
    bool remove_available(const std::string& name);

    /// Clear all available packages
    void clear();

    // =========================================================================
    // Resolution
    // =========================================================================

    /// Resolve a package and all its dependencies
    ///
    /// Returns packages in load order (dependencies first).
    ///
    /// @param package_name Name of the root package to resolve
    /// @return Resolved packages in load order, or error
    [[nodiscard]] void_core::Result<std::vector<ResolvedPackage>> resolve(
        const std::string& package_name) const;

    /// Resolve multiple packages together
    ///
    /// @param package_names Names of packages to resolve
    /// @return Resolved packages in load order, or error
    [[nodiscard]] void_core::Result<std::vector<ResolvedPackage>> resolve_all(
        const std::vector<std::string>& package_names) const;

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validate that the dependency graph is acyclic
    ///
    /// @return Ok if acyclic, Error with cycle information if not
    [[nodiscard]] void_core::Result<void> validate_acyclic() const;

    /// Validate plugin layer hierarchy
    ///
    /// Ensures no plugin depends on a higher-layer plugin.
    ///
    /// @return Ok if valid, Error with violation information if not
    [[nodiscard]] void_core::Result<void> validate_plugin_layers() const;

    /// Run all validations
    [[nodiscard]] void_core::Result<void> validate_all() const;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if a package is available
    [[nodiscard]] bool has_package(const std::string& name) const;

    /// Get a package manifest by name
    [[nodiscard]] const PackageManifest* get_manifest(const std::string& name) const;

    /// Get all available package names
    [[nodiscard]] std::vector<std::string> available_packages() const;

    /// Get all packages of a specific type
    [[nodiscard]] std::vector<std::string> packages_of_type(PackageType type) const;

    /// Get packages that depend on a given package
    [[nodiscard]] std::vector<std::string> get_dependents(const std::string& package_name) const;

    /// Get direct dependencies of a package
    [[nodiscard]] std::vector<std::string> get_dependencies(const std::string& package_name) const;

    /// Check if adding a dependency would create a cycle
    [[nodiscard]] bool would_create_cycle(
        const std::string& from_package,
        const std::string& to_package) const;

    /// Get number of available packages
    [[nodiscard]] std::size_t size() const noexcept { return m_available.size(); }

    /// Check if resolver is empty
    [[nodiscard]] bool empty() const noexcept { return m_available.empty(); }

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Generate GraphViz DOT format of dependency graph
    [[nodiscard]] std::string to_dot_graph() const;

    /// Format dependency tree as string
    [[nodiscard]] std::string format_dependency_tree(const std::string& root) const;

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct AvailablePackage {
        PackageManifest manifest;
        std::filesystem::path path;
    };

    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// Topological sort helper (DFS)
    [[nodiscard]] void_core::Result<void> topological_visit(
        const std::string& name,
        std::vector<std::string>& order,
        std::set<std::string>& visited,
        std::set<std::string>& in_stack,
        std::vector<std::string>& current_path) const;

    /// Check if a dependency is satisfied
    [[nodiscard]] bool is_dependency_satisfied(
        const PackageDependency& dep,
        std::string* out_error = nullptr) const;

    /// Format dependency tree recursively
    void format_tree_recursive(
        const std::string& name,
        std::string& output,
        const std::string& prefix,
        std::set<std::string>& visited) const;

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, AvailablePackage> m_available;
};

} // namespace void_package
