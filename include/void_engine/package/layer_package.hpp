#pragma once

/// @file layer_package.hpp
/// @brief Layer package manifest definitions
///
/// Layer packages enable runtime content patches and variants:
/// - Mod layers that add content to base game
/// - Seasonal/event layers toggled at runtime
/// - Additive scenes, spawners, lighting overrides
/// - Gameplay modifiers (damage multipliers, spawn rates)
///
/// CRITICAL: Layers are loadable from EXTERNAL SOURCES.
/// Layers can depend on plugins (for custom components) and use
/// prefabs from ANY loaded asset bundle.

#include "fwd.hpp"
#include "manifest.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/entity.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <map>
#include <array>

namespace void_package {

// =============================================================================
// SpawnMode
// =============================================================================

/// When to spawn additive scene entities
enum class SpawnMode : std::uint8_t {
    Immediate,  ///< Spawn when layer is applied
    Deferred    ///< Spawn when explicitly triggered
};

/// Convert SpawnMode to string
[[nodiscard]] const char* spawn_mode_to_string(SpawnMode mode) noexcept;

/// Parse SpawnMode from string (case-insensitive)
[[nodiscard]] bool spawn_mode_from_string(const std::string& str, SpawnMode& out) noexcept;

// =============================================================================
// AdditiveSceneEntry
// =============================================================================

/// An additive scene to spawn when the layer is applied
///
/// Example JSON:
/// ```json
/// {
///   "path": "scenes/winter_decorations.scene.json",
///   "spawn_mode": "immediate"
/// }
/// ```
struct AdditiveSceneEntry {
    std::string path;         ///< Path to scene file (relative to package)
    SpawnMode spawn_mode = SpawnMode::Immediate;  ///< When to spawn

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<AdditiveSceneEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// SpawnerVolume
// =============================================================================

/// Volume definition for a spawner
///
/// Supports spherical and box volumes.
struct SpawnerVolume {
    enum class Type : std::uint8_t {
        Sphere,  ///< Spherical volume (center + radius)
        Box      ///< Axis-aligned box (min + max)
    };

    Type type = Type::Sphere;
    std::array<float, 3> center = {0.0f, 0.0f, 0.0f};
    float radius = 10.0f;
    std::array<float, 3> min = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> max = {0.0f, 0.0f, 0.0f};

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<SpawnerVolume> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// SpawnerEntry
// =============================================================================

/// A spawner definition for spawning prefabs over time
///
/// Example JSON:
/// ```json
/// {
///   "id": "elite_spawn_zone_a",
///   "volume": { "center": [100, 0, 50], "radius": 20 },
///   "prefab": "prefabs/enemy_elite.prefab.json",
///   "spawn_rate": 0.1,
///   "max_active": 2
/// }
/// ```
struct SpawnerEntry {
    std::string id;                    ///< Unique spawner identifier
    SpawnerVolume volume;              ///< Spawn volume
    std::string prefab;                ///< Prefab to spawn
    float spawn_rate = 1.0f;           ///< Spawns per second
    int max_active = 10;               ///< Maximum active spawned entities
    float initial_delay = 0.0f;        ///< Delay before first spawn
    bool spawn_on_apply = false;       ///< Spawn one immediately when layer applied

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<SpawnerEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// LightEntry
// =============================================================================

/// A light definition for additional or override lights
struct LightEntry {
    enum class Type : std::uint8_t {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Point;
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> direction = {0.0f, -1.0f, 0.0f};
    std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;           ///< For point/spot lights
    float inner_cone_angle = 30.0f; ///< For spot lights (degrees)
    float outer_cone_angle = 45.0f; ///< For spot lights (degrees)

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<LightEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// SunOverride
// =============================================================================

/// Override for the sun/directional light
struct SunOverride {
    std::array<float, 3> direction = {-0.5f, -0.8f, -0.3f};
    std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<SunOverride> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// AmbientOverride
// =============================================================================

/// Override for ambient lighting
struct AmbientOverride {
    std::array<float, 3> color = {0.1f, 0.1f, 0.1f};
    float intensity = 0.2f;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<AmbientOverride> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// LightingOverride
// =============================================================================

/// Complete lighting override for a layer
///
/// Example JSON:
/// ```json
/// {
///   "sun": {
///     "direction": [-0.5, -0.8, -0.3],
///     "color": [0.2, 0.2, 0.4],
///     "intensity": 0.3
///   },
///   "additional_lights": [
///     {
///       "type": "point",
///       "position": [0, 15, 0],
///       "color": [1.0, 0.9, 0.7],
///       "intensity": 50,
///       "radius": 30
///     }
///   ],
///   "ambient": {
///     "color": [0.05, 0.05, 0.1],
///     "intensity": 0.2
///   }
/// }
/// ```
struct LightingOverride {
    std::optional<SunOverride> sun;
    std::vector<LightEntry> additional_lights;
    std::optional<AmbientOverride> ambient;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<LightingOverride> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Check if any overrides are present
    [[nodiscard]] bool has_overrides() const noexcept {
        return sun.has_value() || !additional_lights.empty() || ambient.has_value();
    }
};

// =============================================================================
// FogConfig
// =============================================================================

/// Fog configuration for weather override
struct FogConfig {
    bool enabled = false;
    std::array<float, 3> color = {0.5f, 0.5f, 0.5f};
    float density = 0.01f;
    float height_falloff = 0.5f;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<FogConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// PrecipitationConfig
// =============================================================================

/// Precipitation configuration
struct PrecipitationConfig {
    enum class Type : std::uint8_t {
        None,
        Rain,
        Snow,
        Hail
    };

    Type type = Type::None;
    float intensity = 0.5f;
    float wind_influence = 0.5f;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<PrecipitationConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// WindZone
// =============================================================================

/// A wind zone definition
struct WindZone {
    std::array<float, 3> min = {-100.0f, 0.0f, -100.0f};
    std::array<float, 3> max = {100.0f, 50.0f, 100.0f};
    std::array<float, 3> direction = {1.0f, 0.0f, 0.0f};
    float strength = 5.0f;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WindZone> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// WeatherOverride
// =============================================================================

/// Weather override for a layer
///
/// Example JSON:
/// ```json
/// {
///   "fog": {
///     "enabled": true,
///     "color": [0.3, 0.3, 0.4],
///     "density": 0.02,
///     "height_falloff": 0.5
///   },
///   "precipitation": {
///     "type": "snow",
///     "intensity": 0.8,
///     "wind_influence": 0.6
///   },
///   "wind_zones": [
///     {
///       "min": [-100, 0, -100],
///       "max": [100, 50, 100],
///       "direction": [1, 0, 0.3],
///       "strength": 5.0
///     }
///   ]
/// }
/// ```
struct WeatherOverride {
    std::optional<FogConfig> fog;
    std::optional<PrecipitationConfig> precipitation;
    std::vector<WindZone> wind_zones;

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WeatherOverride> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Check if any overrides are present
    [[nodiscard]] bool has_overrides() const noexcept {
        return fog.has_value() || precipitation.has_value() || !wind_zones.empty();
    }
};

// =============================================================================
// ObjectiveEntry
// =============================================================================

/// A gameplay objective definition
///
/// Example JSON:
/// ```json
/// {
///   "type": "capture_point",
///   "id": "point_alpha",
///   "position": [0, 0, 100],
///   "config": {
///     "radius": 10,
///     "capture_time": 30
///   }
/// }
/// ```
struct ObjectiveEntry {
    std::string type;                ///< Objective type (capture_point, flag, etc.)
    std::string id;                  ///< Unique objective ID
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
    nlohmann::json config;           ///< Type-specific configuration

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<ObjectiveEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// ModifierEntry
// =============================================================================

/// A gameplay modifier (runtime resource patch)
///
/// Modifiers update ECS resources at runtime. When the layer is unapplied,
/// the original values are restored.
///
/// Example JSON:
/// ```json
/// {
///   "path": "gameplay.damage_multiplier",
///   "value": 1.5
/// }
/// ```
struct ModifierEntry {
    std::string path;        ///< Resource path (e.g., "gameplay.damage_multiplier")
    nlohmann::json value;    ///< New value to set

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<ModifierEntry> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Parse the path into segments
    [[nodiscard]] std::vector<std::string> parse_path_segments() const;
};

// =============================================================================
// LayerPriority
// =============================================================================

/// Layer priority for ordering when multiple layers modify the same thing
///
/// Lower priority layers are applied first.
/// Higher priority layers override lower priority.
constexpr int DEFAULT_LAYER_PRIORITY = 100;

// =============================================================================
// LayerPackageManifest
// =============================================================================

/// Complete manifest for a layer package
///
/// Extends PackageManifest with layer-specific declarations:
/// - Additive scenes to spawn
/// - Spawners for dynamic entity creation
/// - Lighting and weather overrides
/// - Gameplay objectives
/// - Modifiers for ECS resources
///
/// Example manifest:
/// ```json
/// {
///   "package": {
///     "name": "layer.night_mode",
///     "type": "layer",
///     "version": "1.0.0"
///   },
///   "dependencies": {
///     "plugins": [
///       { "name": "gameplay.combat", "version": ">=1.0.0" }
///     ],
///     "assets": [
///       { "name": "assets.night_props", "version": ">=1.0.0" }
///     ]
///   },
///   "priority": 100,
///   "additive_scenes": [
///     {
///       "path": "scenes/night_props.scene.json",
///       "spawn_mode": "immediate"
///     }
///   ],
///   "spawners": [...],
///   "lighting": {...},
///   "weather": {...},
///   "objectives": [...],
///   "modifiers": [
///     { "path": "gameplay.damage_multiplier", "value": 1.5 }
///   ]
/// }
/// ```
struct LayerPackageManifest {
    // Base manifest (identity, dependencies, etc.)
    PackageManifest base;

    // Layer priority for ordering
    int priority = DEFAULT_LAYER_PRIORITY;

    // Content declarations
    std::vector<AdditiveSceneEntry> additive_scenes;
    std::vector<SpawnerEntry> spawners;
    std::optional<LightingOverride> lighting;
    std::optional<WeatherOverride> weather;
    std::vector<ObjectiveEntry> objectives;
    std::vector<ModifierEntry> modifiers;

    // Audio overrides (stored as raw JSON for extensibility)
    std::optional<nlohmann::json> audio_override;

    // Navigation patches (stored as raw JSON for extensibility)
    std::optional<nlohmann::json> navigation_patches;

    // Debug instrumentation
    std::optional<nlohmann::json> debug_instrumentation;

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Load layer manifest from JSON file
    [[nodiscard]] static void_core::Result<LayerPackageManifest> load(
        const std::filesystem::path& path);

    /// Parse from JSON string
    [[nodiscard]] static void_core::Result<LayerPackageManifest> from_json_string(
        const std::string& json_str,
        const std::filesystem::path& source_path = {});

    /// Parse from JSON object (after base manifest is parsed)
    [[nodiscard]] static void_core::Result<LayerPackageManifest> from_json(
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

    /// Validate layer-specific rules
    [[nodiscard]] void_core::Result<void> validate() const;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if layer has any content
    [[nodiscard]] bool has_content() const noexcept {
        return !additive_scenes.empty() || !spawners.empty() ||
               (lighting.has_value() && lighting->has_overrides()) ||
               (weather.has_value() && weather->has_overrides()) ||
               !objectives.empty() || !modifiers.empty();
    }

    /// Get spawner by ID
    [[nodiscard]] const SpawnerEntry* get_spawner(const std::string& id) const;

    /// Get objective by ID
    [[nodiscard]] const ObjectiveEntry* get_objective(const std::string& id) const;

    // =========================================================================
    // Path Resolution
    // =========================================================================

    /// Resolve a scene path relative to the package base path
    [[nodiscard]] std::filesystem::path resolve_scene_path(const std::string& scene_path) const;

    /// Resolve a prefab path relative to the package base path
    [[nodiscard]] std::filesystem::path resolve_prefab_path(const std::string& prefab_path) const;
};

} // namespace void_package
