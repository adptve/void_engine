#pragma once

/// @file registry.hpp
/// @brief Package registry for discovery, loading, and management
///
/// The PackageRegistry is the main entry point for the package system.
/// It discovers packages on disk, manages their lifecycle, and coordinates
/// loading through type-specific loaders.

#include "fwd.hpp"
#include "manifest.hpp"
#include "resolver.hpp"
#include "loader.hpp"
#include <void_engine/core/error.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <optional>

namespace void_package {

// =============================================================================
// LoadedPackage
// =============================================================================

/// Information about a loaded package
struct LoadedPackage {
    ResolvedPackage resolved;                              ///< Resolution info
    PackageStatus status;                                  ///< Current status
    std::chrono::steady_clock::time_point load_time;       ///< When it was loaded
    std::chrono::steady_clock::time_point last_access;     ///< Last access time
    std::string error_message;                             ///< Error if Failed

    /// Get time since load
    [[nodiscard]] std::chrono::milliseconds time_since_load() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - load_time);
    }
};

// =============================================================================
// PackageRegistry
// =============================================================================

/// Central registry for package discovery, loading, and management
///
/// The PackageRegistry:
/// - Scans directories to discover available packages
/// - Resolves dependencies and determines load order
/// - Coordinates loading through type-specific loaders
/// - Tracks loaded package state
/// - Supports hot-reload of packages
///
/// Thread-safety: The registry uses internal locking for thread-safe access
/// to its state. However, actual package loading is NOT thread-safe and
/// should occur on a single thread.
class PackageRegistry {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    PackageRegistry();
    ~PackageRegistry();

    // Non-copyable, non-movable (contains std::mutex)
    PackageRegistry(const PackageRegistry&) = delete;
    PackageRegistry& operator=(const PackageRegistry&) = delete;
    PackageRegistry(PackageRegistry&&) = delete;
    PackageRegistry& operator=(PackageRegistry&&) = delete;

    // =========================================================================
    // Discovery
    // =========================================================================

    /// Scan a directory for package manifests
    ///
    /// Searches for files matching:
    /// - *.world.json
    /// - *.layer.json
    /// - *.plugin.json
    /// - *.widget.json
    /// - *.bundle.json
    ///
    /// @param path Directory to scan
    /// @param recursive If true, scan subdirectories
    /// @return Number of packages discovered, or error
    [[nodiscard]] void_core::Result<std::size_t> scan_directory(
        const std::filesystem::path& path,
        bool recursive = true);

    /// Register a single package manifest
    ///
    /// @param manifest_path Path to manifest file
    /// @return Ok on success, Error if invalid
    [[nodiscard]] void_core::Result<void> register_manifest(
        const std::filesystem::path& manifest_path);

    /// Remove a package from the registry (must be unloaded first)
    ///
    /// @param name Package name
    /// @return Ok if removed, Error if still loaded or not found
    [[nodiscard]] void_core::Result<void> unregister_package(const std::string& name);

    /// Clear all unloaded packages from registry
    void clear_available();

    // =========================================================================
    // Loading
    // =========================================================================

    /// Load a package and all its dependencies
    ///
    /// @param name Package name
    /// @param ctx Load context with engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> load_package(
        const std::string& name,
        LoadContext& ctx);

    /// Load multiple packages
    ///
    /// @param names Package names
    /// @param ctx Load context with engine systems
    /// @return Ok on success, Error on first failure
    [[nodiscard]] void_core::Result<void> load_packages(
        const std::vector<std::string>& names,
        LoadContext& ctx);

    /// Unload a package (and packages that depend on it)
    ///
    /// @param name Package name
    /// @param ctx Load context with engine systems
    /// @param force If true, unload even if other packages depend on it
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> unload_package(
        const std::string& name,
        LoadContext& ctx,
        bool force = false);

    /// Unload all loaded packages
    ///
    /// @param ctx Load context with engine systems
    /// @return Ok on success, Error on first failure
    [[nodiscard]] void_core::Result<void> unload_all(LoadContext& ctx);

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Reload a package
    ///
    /// If the package's loader supports hot-reload, uses that.
    /// Otherwise, unloads and reloads the package.
    ///
    /// @param name Package name
    /// @param ctx Load context with engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> reload_package(
        const std::string& name,
        LoadContext& ctx);

    /// Check if any packages have changed on disk
    ///
    /// @return Names of packages that have changed
    [[nodiscard]] std::vector<std::string> check_for_changes() const;

    // =========================================================================
    // Status Queries
    // =========================================================================

    /// Get status of a package
    ///
    /// @param name Package name
    /// @return Status, or nullopt if package not known
    [[nodiscard]] std::optional<PackageStatus> status(const std::string& name) const;

    /// Get loaded package info
    ///
    /// @param name Package name
    /// @return Package info, or nullptr if not loaded
    [[nodiscard]] const LoadedPackage* get_loaded(const std::string& name) const;

    /// Get manifest for a package
    ///
    /// @param name Package name
    /// @return Manifest, or nullptr if not known
    [[nodiscard]] const PackageManifest* get_manifest(const std::string& name) const;

    /// Check if a package is loaded
    [[nodiscard]] bool is_loaded(const std::string& name) const;

    /// Check if a package is available (discovered but maybe not loaded)
    [[nodiscard]] bool is_available(const std::string& name) const;

    // =========================================================================
    // Package Listings
    // =========================================================================

    /// Get all loaded package names
    [[nodiscard]] std::vector<std::string> loaded_packages() const;

    /// Get all available package names
    [[nodiscard]] std::vector<std::string> available_packages() const;

    /// Get packages by type
    [[nodiscard]] std::vector<std::string> packages_of_type(PackageType type) const;

    /// Get packages by status
    [[nodiscard]] std::vector<std::string> packages_by_status(PackageStatus status) const;

    /// Get packages that depend on a given package
    [[nodiscard]] std::vector<std::string> get_dependents(const std::string& name) const;

    /// Get dependencies of a package
    [[nodiscard]] std::vector<std::string> get_dependencies(const std::string& name) const;

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validate all available packages
    ///
    /// Checks manifests, dependencies, cycles, etc.
    ///
    /// @return Ok if all valid, Error with details if not
    [[nodiscard]] void_core::Result<void> validate() const;

    // =========================================================================
    // Resolver Access
    // =========================================================================

    /// Get the internal resolver (for advanced queries)
    [[nodiscard]] const PackageResolver& resolver() const noexcept { return m_resolver; }

    /// Get mutable resolver (use with caution)
    [[nodiscard]] PackageResolver& resolver_mut() noexcept { return m_resolver; }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get number of loaded packages
    [[nodiscard]] std::size_t loaded_count() const;

    /// Get number of available packages
    [[nodiscard]] std::size_t available_count() const;

    /// Get total packages discovered
    [[nodiscard]] std::size_t total_count() const;

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Format registry state as string
    [[nodiscard]] std::string format_state() const;

    /// Format dependency graph as DOT
    [[nodiscard]] std::string format_dependency_graph() const;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// Load a single resolved package
    [[nodiscard]] void_core::Result<void> load_resolved(
        const ResolvedPackage& resolved,
        LoadContext& ctx);

    /// Unload a single package
    [[nodiscard]] void_core::Result<void> unload_single(
        const std::string& name,
        LoadContext& ctx);

    /// Find packages that would be affected by unloading
    [[nodiscard]] std::vector<std::string> affected_by_unload(
        const std::string& name) const;

    /// Scan a single file for package manifest
    void scan_file(const std::filesystem::path& path);

    // =========================================================================
    // Data Members
    // =========================================================================

    mutable std::mutex m_mutex;  // Protects m_loaded and m_failed
    PackageResolver m_resolver;
    std::map<std::string, LoadedPackage> m_loaded;
    std::map<std::string, std::string> m_failed;  // name -> error message
    std::map<std::string, std::filesystem::file_time_type> m_file_times;  // For change detection
};

// =============================================================================
// Utility Functions
// =============================================================================

/// Get the canonical package manifest extensions
[[nodiscard]] std::vector<std::string> package_manifest_extensions();

/// Check if a file path looks like a package manifest
[[nodiscard]] bool is_package_manifest_path(const std::filesystem::path& path);

/// Determine package type from file extension
[[nodiscard]] std::optional<PackageType> package_type_from_extension(
    const std::filesystem::path& path);

} // namespace void_package
