#pragma once

/// @file asset_bundle.hpp
/// @brief Asset bundle manifest definitions for void_package Phase 2
///
/// An asset.bundle contains pure content data with no executable logic:
/// - Meshes (models, LODs, collision)
/// - Textures and materials
/// - Animations and blend spaces
/// - Audio assets
/// - VFX and shaders
/// - Prefabs (entity templates with components as JSON)
/// - Definitions (data for registries like weapons, auras, abilities)
///
/// CRITICAL: Asset bundles must be loadable from EXTERNAL SOURCES the engine
/// has never seen. Prefabs store components by NAME, resolved at runtime.

#include "fwd.hpp"
#include "manifest.hpp"
#include <void_engine/core/error.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <filesystem>

namespace void_package {

// =============================================================================
// Mesh Entry
// =============================================================================

/// Entry for a mesh/model asset
struct MeshEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string path;                            ///< Path relative to bundle root
    std::vector<std::string> lod_paths;          ///< Optional LOD level paths
    std::optional<std::string> collision_path;   ///< Optional collision mesh path

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<MeshEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Texture Entry
// =============================================================================

/// Entry for a texture asset
struct TextureEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string path;                            ///< Path relative to bundle root
    std::string format;                          ///< Texture format (bc7, bc5, rgba, etc.)
    bool mipmaps = true;                         ///< Whether to generate mipmaps
    bool srgb = true;                            ///< Whether texture is in sRGB color space

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<TextureEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Material Entry
// =============================================================================

/// Entry for a material definition
struct MaterialEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string shader;                          ///< Shader ID or path
    std::map<std::string, std::string> textures; ///< Texture slot -> texture ID mapping
    nlohmann::json parameters;                   ///< Shader parameters (arbitrary JSON)

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<MaterialEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Animation Entry
// =============================================================================

/// Animation event marker
struct AnimationEvent {
    float time;                                  ///< Time in seconds
    std::string event;                           ///< Event name to emit

    [[nodiscard]] static void_core::Result<AnimationEvent> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Entry for an animation clip
struct AnimationEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string path;                            ///< Path relative to bundle root
    bool loop = false;                           ///< Whether animation loops
    bool root_motion = false;                    ///< Whether to extract root motion
    std::vector<AnimationEvent> events;          ///< Animation events

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<AnimationEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Blend Space Entry
// =============================================================================

/// Sample point in a blend space
struct BlendSpaceSample {
    std::vector<float> position;                 ///< Position in blend space (1D or 2D)
    std::string animation;                       ///< Animation ID to use at this position

    [[nodiscard]] static void_core::Result<BlendSpaceSample> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Entry for an animation blend space
struct BlendSpaceEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string type;                            ///< "1d" or "2d"
    std::string axis_x;                          ///< Parameter name for X axis
    std::optional<std::string> axis_y;           ///< Parameter name for Y axis (2D only)
    std::vector<BlendSpaceSample> samples;       ///< Sample points

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<BlendSpaceEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Audio Entry
// =============================================================================

/// Entry for an audio asset
struct AudioEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string path;                            ///< Path relative to bundle root
    std::string type;                            ///< "sfx", "ambient", "music", "voice"
    float volume = 1.0f;                         ///< Default volume (0.0 - 1.0)
    bool loop = false;                           ///< Whether audio loops
    std::vector<std::string> variations;         ///< Optional variation paths

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<AudioEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// VFX Entry
// =============================================================================

/// Entry for a visual effect asset
struct VfxEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::string path;                            ///< Path to VFX definition file
    std::string type;                            ///< "particle_system", "decal", "flipbook"
    std::optional<float> lifetime;               ///< Optional lifetime for decals

    // Flipbook-specific
    std::optional<int> columns;                  ///< Columns for flipbook
    std::optional<int> rows;                     ///< Rows for flipbook
    std::optional<int> fps;                      ///< Frames per second for flipbook

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<VfxEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Shader Entry
// =============================================================================

/// Entry for a shader asset
struct ShaderEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::optional<std::string> vertex;           ///< Vertex shader path
    std::optional<std::string> fragment;         ///< Fragment shader path
    std::optional<std::string> compute;          ///< Compute shader path
    std::vector<std::string> variants;           ///< Shader variants (defines)

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<ShaderEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Prefab Entry
// =============================================================================

/// Entry for a prefab (entity template)
///
/// CRITICAL: Components are stored as a map of component NAME -> JSON data.
/// The engine has no compile-time knowledge of what components exist.
/// Resolution happens at runtime via ComponentRegistry::get_id_by_name().
///
/// Example:
/// ```json
/// {
///   "id": "enemy_soldier",
///   "components": {
///     "Transform": {},
///     "Health": { "max": 100, "current": 100 },
///     "CustomModComponent": { "power_level": 9000 }
///   },
///   "tags": ["Enemy", "Damageable"]
/// }
/// ```
struct PrefabEntry {
    std::string id;                              ///< Unique identifier within bundle
    std::map<std::string, nlohmann::json> components;  ///< Component name -> component data
    std::vector<std::string> tags;               ///< Entity tags

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<PrefabEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Definition Entry
// =============================================================================

/// Entry for a registry definition (weapons, auras, abilities, etc.)
///
/// Definitions provide data for generic registry systems defined by plugins.
/// The engine doesn't know what fields exist - it just stores JSON data
/// keyed by registry_type and id.
///
/// Example:
/// ```json
/// {
///   "registry_type": "weapons",
///   "id": "plasma_rifle",
///   "data": {
///     "display_name": "Plasma Rifle",
///     "damage": 45,
///     "fire_rate": 8,
///     "mesh": "plasma_rifle_mesh"
///   }
/// }
/// ```
struct DefinitionEntry {
    std::string registry_type;                   ///< Registry type (e.g., "weapons", "auras")
    std::string id;                              ///< Unique identifier within registry
    nlohmann::json data;                         ///< Arbitrary definition data

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<DefinitionEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// UI Asset Entries
// =============================================================================

/// Entry for a UI layout
struct UILayoutEntry {
    std::string id;
    std::string path;

    [[nodiscard]] static void_core::Result<UILayoutEntry> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Entry for a UI icon
struct UIIconEntry {
    std::string id;
    std::string path;

    [[nodiscard]] static void_core::Result<UIIconEntry> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Entry for a font
struct UIFontEntry {
    std::string id;
    std::string path;
    std::vector<int> sizes;                      ///< Font sizes to pre-render

    [[nodiscard]] static void_core::Result<UIFontEntry> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Entry for a UI theme
struct UIThemeEntry {
    std::string id;
    std::string path;

    [[nodiscard]] static void_core::Result<UIThemeEntry> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Container for all UI assets
struct UIAssets {
    std::vector<UILayoutEntry> layouts;
    std::vector<UIIconEntry> icons;
    std::vector<UIFontEntry> fonts;
    std::vector<UIThemeEntry> themes;

    [[nodiscard]] static void_core::Result<UIAssets> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// Data Tables
// =============================================================================

/// Entry for a data table reference
struct DataTableEntry {
    std::string id;                              ///< Unique identifier
    std::string path;                            ///< Path to data file or directory
    std::optional<std::string> schema;           ///< Optional JSON schema for validation

    [[nodiscard]] static void_core::Result<DataTableEntry> from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// AssetBundleManifest
// =============================================================================

/// Complete manifest for an asset.bundle package
///
/// Contains all the content assets with no executable logic.
/// Paths are relative to the bundle root directory.
struct AssetBundleManifest {
    PackageManifest base;                        ///< Base package manifest (name, version, deps)

    // Asset collections
    std::vector<MeshEntry> meshes;
    std::vector<TextureEntry> textures;
    std::vector<MaterialEntry> materials;
    std::vector<AnimationEntry> animations;
    std::vector<BlendSpaceEntry> blend_spaces;
    std::vector<AudioEntry> audio;
    std::vector<VfxEntry> vfx;
    std::vector<ShaderEntry> shaders;
    std::vector<PrefabEntry> prefabs;

    // Definitions grouped by registry type
    // Key: registry_type (e.g., "weapons", "auras")
    // Value: list of definitions for that registry
    std::map<std::string, std::vector<DefinitionEntry>> definitions;

    // UI assets
    std::optional<UIAssets> ui_assets;

    // Data tables
    std::map<std::string, DataTableEntry> data_tables;

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Load from a JSON file
    [[nodiscard]] static void_core::Result<AssetBundleManifest> load(
        const std::filesystem::path& path);

    /// Parse from JSON string
    [[nodiscard]] static void_core::Result<AssetBundleManifest> from_json_string(
        const std::string& json_str,
        const std::filesystem::path& source_path = {});

    /// Parse from JSON object
    [[nodiscard]] static void_core::Result<AssetBundleManifest> from_json(
        const nlohmann::json& j,
        const std::filesystem::path& source_path = {});

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validate manifest completeness
    [[nodiscard]] void_core::Result<void> validate() const;

    /// Check for duplicate IDs across all asset types
    [[nodiscard]] void_core::Result<void> validate_unique_ids() const;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Find mesh by ID
    [[nodiscard]] const MeshEntry* find_mesh(const std::string& id) const;

    /// Find texture by ID
    [[nodiscard]] const TextureEntry* find_texture(const std::string& id) const;

    /// Find material by ID
    [[nodiscard]] const MaterialEntry* find_material(const std::string& id) const;

    /// Find animation by ID
    [[nodiscard]] const AnimationEntry* find_animation(const std::string& id) const;

    /// Find audio by ID
    [[nodiscard]] const AudioEntry* find_audio(const std::string& id) const;

    /// Find prefab by ID
    [[nodiscard]] const PrefabEntry* find_prefab(const std::string& id) const;

    /// Find definition by registry type and ID
    [[nodiscard]] const DefinitionEntry* find_definition(
        const std::string& registry_type,
        const std::string& id) const;

    /// Get all definition registry types
    [[nodiscard]] std::vector<std::string> definition_registry_types() const;

    /// Get total asset count
    [[nodiscard]] std::size_t total_asset_count() const;
};

// =============================================================================
// Utilities
// =============================================================================

/// Check if a file extension indicates an asset bundle
[[nodiscard]] bool is_asset_bundle_extension(const std::filesystem::path& path);

} // namespace void_package
