#pragma once

/// @file asset_bundle_loader.hpp
/// @brief PackageLoader implementation for asset bundles
///
/// The AssetBundleLoader handles loading/unloading of asset.bundle packages.
/// It coordinates:
/// - Parsing the AssetBundleManifest
/// - Registering prefabs with PrefabRegistry
/// - Registering definitions with DefinitionRegistry
/// - Loading assets into engine systems (meshes, textures, etc.)
///
/// CRITICAL: This loader handles EXTERNAL content. It makes no assumptions
/// about what components, registries, or asset types will be present.

#include "fwd.hpp"
#include "loader.hpp"
#include "asset_bundle.hpp"
#include "prefab_registry.hpp"
#include <void_engine/core/error.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <memory>

namespace void_package {

// Forward declarations
class DefinitionRegistry;
class ComponentSchemaRegistry;

// =============================================================================
// AssetBundleLoadResult
// =============================================================================

/// Result of loading an asset bundle
struct AssetBundleLoadResult {
    std::string bundle_name;                     ///< Bundle name
    std::size_t prefabs_loaded = 0;              ///< Number of prefabs registered
    std::size_t definitions_loaded = 0;          ///< Number of definitions registered
    std::size_t meshes_loaded = 0;               ///< Number of meshes loaded
    std::size_t textures_loaded = 0;             ///< Number of textures loaded
    std::size_t materials_loaded = 0;            ///< Number of materials loaded
    std::size_t animations_loaded = 0;           ///< Number of animations loaded
    std::size_t audio_loaded = 0;                ///< Number of audio assets loaded
    std::size_t shaders_loaded = 0;              ///< Number of shaders loaded
    std::vector<std::string> warnings;           ///< Non-fatal warnings during load

    /// Get total asset count
    [[nodiscard]] std::size_t total_assets() const {
        return prefabs_loaded + definitions_loaded + meshes_loaded +
               textures_loaded + materials_loaded + animations_loaded +
               audio_loaded + shaders_loaded;
    }

    /// Check if there were any warnings
    [[nodiscard]] bool has_warnings() const { return !warnings.empty(); }
};

// =============================================================================
// AssetBundleLoader
// =============================================================================

/// PackageLoader implementation for asset.bundle packages
///
/// Handles loading pure content packages containing:
/// - Meshes, textures, materials
/// - Animations and blend spaces
/// - Audio and VFX
/// - Prefabs (entity templates with components as JSON)
/// - Definitions (data for generic registries)
///
/// Thread-safety: NOT thread-safe. Load operations must be externally synchronized.
class AssetBundleLoader : public PackageLoader {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    AssetBundleLoader() = default;

    /// Construct with registries
    ///
    /// @param prefab_registry Registry for prefab storage
    /// @param definition_registry Registry for definition storage
    /// @param schema_registry Optional schema registry for component conversion
    explicit AssetBundleLoader(
        PrefabRegistry* prefab_registry,
        DefinitionRegistry* definition_registry,
        ComponentSchemaRegistry* schema_registry = nullptr);

    // Non-copyable, movable
    AssetBundleLoader(const AssetBundleLoader&) = delete;
    AssetBundleLoader& operator=(const AssetBundleLoader&) = delete;
    AssetBundleLoader(AssetBundleLoader&&) = default;
    AssetBundleLoader& operator=(AssetBundleLoader&&) = default;

    // =========================================================================
    // PackageLoader Interface
    // =========================================================================

    /// Get the package type this loader handles
    [[nodiscard]] PackageType supported_type() const override {
        return PackageType::Asset;
    }

    /// Get loader name for debugging
    [[nodiscard]] const char* name() const override {
        return "AssetBundleLoader";
    }

    /// Load an asset bundle package
    ///
    /// @param package Resolved package information
    /// @param ctx Load context with access to engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) override;

    /// Unload an asset bundle package
    ///
    /// @param package_name Name of package to unload
    /// @param ctx Load context with access to engine systems
    /// @return Ok on success, Error on failure
    [[nodiscard]] void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) override;

    /// Check if this loader supports hot-reload
    [[nodiscard]] bool supports_hot_reload() const override { return true; }

    /// Check if a package is currently loaded
    [[nodiscard]] bool is_loaded(const std::string& package_name) const override;

    /// Get names of all loaded packages
    [[nodiscard]] std::vector<std::string> loaded_packages() const override;

    // =========================================================================
    // Extended API
    // =========================================================================

    /// Load and get detailed result
    [[nodiscard]] void_core::Result<AssetBundleLoadResult> load_with_result(
        const ResolvedPackage& package,
        LoadContext& ctx);

    /// Get load result for a loaded bundle
    [[nodiscard]] const AssetBundleLoadResult* get_load_result(
        const std::string& package_name) const;

    /// Get the manifest of a loaded bundle
    [[nodiscard]] const AssetBundleManifest* get_manifest(
        const std::string& package_name) const;

    // =========================================================================
    // Registry Configuration
    // =========================================================================

    /// Set the prefab registry
    void set_prefab_registry(PrefabRegistry* registry) {
        m_prefab_registry = registry;
    }

    /// Get the prefab registry
    [[nodiscard]] PrefabRegistry* prefab_registry() const {
        return m_prefab_registry;
    }

    /// Set the definition registry
    void set_definition_registry(DefinitionRegistry* registry) {
        m_definition_registry = registry;
    }

    /// Get the definition registry
    [[nodiscard]] DefinitionRegistry* definition_registry() const {
        return m_definition_registry;
    }

    /// Set the component schema registry
    void set_schema_registry(ComponentSchemaRegistry* registry) {
        m_schema_registry = registry;
    }

    /// Get the component schema registry
    [[nodiscard]] ComponentSchemaRegistry* schema_registry() const {
        return m_schema_registry;
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Policy for handling missing asset files
    enum class MissingAssetPolicy {
        Error,      ///< Fail loading if any asset file is missing
        Warn,       ///< Log warning and continue
        Skip        ///< Silently skip missing assets
    };

    /// Set policy for missing asset files
    void set_missing_asset_policy(MissingAssetPolicy policy) {
        m_missing_policy = policy;
    }

    /// Get current missing asset policy
    [[nodiscard]] MissingAssetPolicy missing_asset_policy() const {
        return m_missing_policy;
    }

    /// Enable/disable strict validation of manifests
    void set_strict_validation(bool strict) {
        m_strict_validation = strict;
    }

    /// Check if strict validation is enabled
    [[nodiscard]] bool strict_validation() const {
        return m_strict_validation;
    }

    // =========================================================================
    // Debugging
    // =========================================================================

    /// Format loader state for debugging
    [[nodiscard]] std::string format_state() const;

    /// Get statistics
    struct Stats {
        std::size_t bundles_loaded = 0;
        std::size_t total_prefabs = 0;
        std::size_t total_definitions = 0;
        std::size_t total_assets = 0;
    };
    [[nodiscard]] Stats get_stats() const;

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    /// Information about a loaded bundle
    struct LoadedBundle {
        AssetBundleManifest manifest;
        std::filesystem::path root_path;
        AssetBundleLoadResult result;
        std::set<std::string> loaded_asset_ids;  ///< Track loaded assets for cleanup
    };

    // =========================================================================
    // Internal Methods
    // =========================================================================

    /// Load prefabs from manifest
    [[nodiscard]] void_core::Result<std::size_t> load_prefabs(
        const AssetBundleManifest& manifest,
        const std::string& bundle_name,
        AssetBundleLoadResult& result);

    /// Load definitions from manifest
    [[nodiscard]] void_core::Result<std::size_t> load_definitions(
        const AssetBundleManifest& manifest,
        const std::string& bundle_name,
        AssetBundleLoadResult& result);

    /// Load mesh assets (placeholder for asset server integration)
    [[nodiscard]] void_core::Result<std::size_t> load_meshes(
        const AssetBundleManifest& manifest,
        const std::filesystem::path& root_path,
        LoadContext& ctx,
        AssetBundleLoadResult& result);

    /// Load texture assets (placeholder for asset server integration)
    [[nodiscard]] void_core::Result<std::size_t> load_textures(
        const AssetBundleManifest& manifest,
        const std::filesystem::path& root_path,
        LoadContext& ctx,
        AssetBundleLoadResult& result);

    /// Load material assets (placeholder for asset server integration)
    [[nodiscard]] void_core::Result<std::size_t> load_materials(
        const AssetBundleManifest& manifest,
        const std::filesystem::path& root_path,
        LoadContext& ctx,
        AssetBundleLoadResult& result);

    /// Load animation assets (placeholder for asset server integration)
    [[nodiscard]] void_core::Result<std::size_t> load_animations(
        const AssetBundleManifest& manifest,
        const std::filesystem::path& root_path,
        LoadContext& ctx,
        AssetBundleLoadResult& result);

    /// Load audio assets (placeholder for asset server integration)
    [[nodiscard]] void_core::Result<std::size_t> load_audio(
        const AssetBundleManifest& manifest,
        const std::filesystem::path& root_path,
        LoadContext& ctx,
        AssetBundleLoadResult& result);

    /// Load shader assets (placeholder for asset server integration)
    [[nodiscard]] void_core::Result<std::size_t> load_shaders(
        const AssetBundleManifest& manifest,
        const std::filesystem::path& root_path,
        LoadContext& ctx,
        AssetBundleLoadResult& result);

    /// Unload all assets from a bundle
    void unload_bundle_assets(const LoadedBundle& bundle, LoadContext& ctx);

    /// Convert PrefabEntry to PrefabDefinition
    [[nodiscard]] PrefabDefinition entry_to_definition(
        const PrefabEntry& entry,
        const std::string& bundle_name) const;

    /// Handle missing asset based on policy
    [[nodiscard]] void_core::Result<void> handle_missing_asset(
        const std::string& asset_id,
        const std::string& asset_path,
        AssetBundleLoadResult& result);

    // =========================================================================
    // Data Members
    // =========================================================================

    std::map<std::string, LoadedBundle> m_loaded_bundles;
    PrefabRegistry* m_prefab_registry = nullptr;
    DefinitionRegistry* m_definition_registry = nullptr;
    ComponentSchemaRegistry* m_schema_registry = nullptr;
    MissingAssetPolicy m_missing_policy = MissingAssetPolicy::Warn;
    bool m_strict_validation = false;
};

} // namespace void_package
