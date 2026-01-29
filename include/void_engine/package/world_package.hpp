#pragma once

/// @file world_package.hpp
/// @brief World package manifest definitions
///
/// World packages are the composition root for complete game modes/levels.
/// They define:
/// - Root scene to load
/// - All dependencies (plugins, layers, widgets, assets)
/// - Player spawn configuration
/// - Environment settings (time, skybox, weather, post-process)
/// - Gameplay settings (difficulty, match rules)
/// - ECS resource initialization
/// - World logic (win/lose conditions, round flow)
///
/// CRITICAL: World packages are completely self-describing.
/// The engine needs ZERO prior knowledge of world content.
/// All dependencies are resolved and loaded automatically.
///
/// CRITICAL: Worlds must be loadable from EXTERNAL SOURCES.
/// - Complete game modes from mods
/// - Self-contained world definitions
/// - World specifies ALL its dependencies

#include "fwd.hpp"
#include "manifest.hpp"
#include "prefab_registry.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <map>
#include <array>

namespace void_package {

// TransformData is declared in prefab_registry.hpp

// =============================================================================
// SpawnSelection
// =============================================================================

/// How to select spawn points for players
enum class SpawnSelection : std::uint8_t {
    RoundRobin,   ///< Cycle through spawn points in order
    Random,       ///< Random selection from available points
    Fixed,        ///< Always use first spawn point
    Weighted      ///< Use weighted random selection
};

/// Convert SpawnSelection to string
[[nodiscard]] const char* spawn_selection_to_string(SpawnSelection selection) noexcept;

/// Parse SpawnSelection from string (case-insensitive)
[[nodiscard]] bool spawn_selection_from_string(const std::string& str, SpawnSelection& out) noexcept;

// =============================================================================
// RootSceneConfig
// =============================================================================

/// Root scene configuration for the world
///
/// Defines the main scene to load and world boundaries.
///
/// Example JSON:
/// ```json
/// {
///   "path": "scenes/arena_01.scene.json",
///   "spawn_points": ["spawn_alpha", "spawn_beta", "spawn_gamma"],
///   "world_bounds": {
///     "min": [-500, -50, -500],
///     "max": [500, 200, 500]
///   }
/// }
/// ```
struct RootSceneConfig {
    std::string path;                              ///< Path to scene file
    std::vector<std::string> spawn_points;         ///< Named spawn point entities
    std::array<float, 3> world_bounds_min = {-1000.0f, -100.0f, -1000.0f};
    std::array<float, 3> world_bounds_max = {1000.0f, 500.0f, 1000.0f};

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<RootSceneConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// PlayerSpawnConfig
// =============================================================================

/// Player entity spawning configuration
///
/// Defines how players enter the world:
/// - Which prefab to instantiate
/// - How spawn points are selected
/// - Initial inventory (if inventory system loaded)
/// - Initial stats (health, armor, etc.)
///
/// Example JSON:
/// ```json
/// {
///   "prefab": "prefabs/player_fps.prefab.json",
///   "spawn_selection": "round_robin",
///   "initial_inventory": {
///     "weapon_slot_1": "weapons/pistol_default",
///     "armor": "armor/light_vest"
///   },
///   "initial_stats": {
///     "health": 100,
///     "armor": 25,
///     "stamina": 100
///   }
/// }
/// ```
struct PlayerSpawnConfig {
    std::string prefab;                            ///< Player prefab to instantiate
    SpawnSelection spawn_selection = SpawnSelection::RoundRobin;
    std::optional<nlohmann::json> initial_inventory; ///< Initial inventory slots
    std::optional<nlohmann::json> initial_stats;     ///< Initial stat values

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<PlayerSpawnConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Check if inventory configuration is present
    [[nodiscard]] bool has_initial_inventory() const noexcept {
        return initial_inventory.has_value() && !initial_inventory->is_null();
    }

    /// Check if stats configuration is present
    [[nodiscard]] bool has_initial_stats() const noexcept {
        return initial_stats.has_value() && !initial_stats->is_null();
    }
};

// =============================================================================
// WeatherConfig
// =============================================================================

/// Weather configuration for the world environment
struct WeatherConfig {
    std::string profile;                           ///< Weather profile path/ID
    float intensity = 1.0f;                        ///< Weather intensity (0-1)

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WeatherConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// PostProcessConfig
// =============================================================================

/// Post-processing configuration
struct PostProcessConfig {
    std::string profile;                           ///< Post-process profile path/ID
    float exposure = 1.0f;                         ///< Exposure value
    float bloom_intensity = 0.0f;                  ///< Bloom intensity

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<PostProcessConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// EnvironmentConfig
// =============================================================================

/// World environment settings
///
/// Global environmental configuration:
/// - Time of day (affects lighting)
/// - Skybox selection
/// - Weather profile
/// - Post-processing settings
///
/// Example JSON:
/// ```json
/// {
///   "time_of_day": 14.5,
///   "skybox": "skyboxes/overcast_day",
///   "weather": {
///     "profile": "weather/light_rain",
///     "intensity": 0.6
///   },
///   "post_process": {
///     "profile": "post/cinematic_default",
///     "exposure": 1.2,
///     "bloom_intensity": 0.4
///   }
/// }
/// ```
struct EnvironmentConfig {
    float time_of_day = 12.0f;                     ///< Time of day (0-24)
    std::string skybox;                            ///< Skybox asset path
    std::optional<WeatherConfig> weather;          ///< Weather configuration
    std::optional<PostProcessConfig> post_process; ///< Post-process settings
    std::array<float, 3> ambient_color = {0.1f, 0.1f, 0.1f};  ///< Ambient light color
    float ambient_intensity = 0.2f;                ///< Ambient light intensity

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<EnvironmentConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// GameplayConfig
// =============================================================================

/// Gameplay/match settings
///
/// Match and ruleset configuration:
/// - Difficulty level
/// - Match time/score limits
/// - Ruleset flags (friendly fire, vehicles, etc.)
///
/// Example JSON:
/// ```json
/// {
///   "difficulty": "normal",
///   "match_length_seconds": 600,
///   "score_limit": 50,
///   "friendly_fire": false,
///   "respawn_delay_seconds": 5,
///   "ruleset_flags": {
///     "allow_vehicles": true,
///     "enable_killstreaks": false,
///     "hardcore_mode": false
///   }
/// }
/// ```
struct GameplayConfig {
    std::string difficulty = "normal";             ///< Difficulty level
    int match_length_seconds = 0;                  ///< Match time limit (0 = unlimited)
    int score_limit = 0;                           ///< Score limit (0 = unlimited)
    bool friendly_fire = false;                    ///< Allow friendly fire
    int respawn_delay_seconds = 5;                 ///< Respawn delay
    std::map<std::string, nlohmann::json> ruleset_flags;  ///< Additional ruleset flags

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<GameplayConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Get a ruleset flag value
    template<typename T>
    [[nodiscard]] T get_flag(const std::string& name, T default_value) const {
        auto it = ruleset_flags.find(name);
        if (it == ruleset_flags.end()) {
            return default_value;
        }
        try {
            return it->second.get<T>();
        } catch (...) {
            return default_value;
        }
    }
};

// =============================================================================
// WinCondition
// =============================================================================

/// A win condition definition
struct WinCondition {
    std::string type;                              ///< Condition type (score_limit, time_limit, etc.)
    nlohmann::json config;                         ///< Type-specific configuration

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WinCondition> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// LoseCondition
// =============================================================================

/// A lose condition definition
struct LoseCondition {
    std::string type;                              ///< Condition type (team_eliminated, etc.)
    nlohmann::json config;                         ///< Type-specific configuration

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<LoseCondition> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// RoundFlowConfig
// =============================================================================

/// Round flow timing configuration
struct RoundFlowConfig {
    int warmup_duration = 30;                      ///< Warmup phase duration (seconds)
    int round_duration = 180;                      ///< Round duration (seconds)
    int intermission_duration = 15;                ///< Intermission duration (seconds)

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<RoundFlowConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

// =============================================================================
// WorldLogicConfig
// =============================================================================

/// World-level logic and state machine configuration
///
/// Defines win/lose conditions and round flow:
/// - Victory conditions (score limit, time limit)
/// - Defeat conditions (team elimination)
/// - Round flow (warmup, rounds, intermission)
/// - Optional state machine reference
///
/// Example JSON:
/// ```json
/// {
///   "win_conditions": [
///     { "type": "score_limit", "target": 50 },
///     { "type": "time_limit", "seconds": 600 }
///   ],
///   "lose_conditions": [
///     { "type": "team_eliminated" }
///   ],
///   "round_flow": {
///     "warmup_duration": 30,
///     "round_duration": 180,
///     "intermission_duration": 15
///   },
///   "state_machine": "scripts/arena_match_flow.state.json"
/// }
/// ```
struct WorldLogicConfig {
    std::vector<WinCondition> win_conditions;
    std::vector<LoseCondition> lose_conditions;
    std::optional<RoundFlowConfig> round_flow;
    std::optional<std::string> state_machine;      ///< Path to state machine definition

    /// Parse from JSON
    [[nodiscard]] static void_core::Result<WorldLogicConfig> from_json(const nlohmann::json& j);

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Check if any logic is defined
    [[nodiscard]] bool has_logic() const noexcept {
        return !win_conditions.empty() || !lose_conditions.empty() ||
               round_flow.has_value() || state_machine.has_value();
    }
};

// =============================================================================
// WorldPackageManifest
// =============================================================================

/// Complete manifest for a world package
///
/// A world.package is the composition root that brings together:
/// - Root scene definition
/// - Plugin dependencies (for components/systems)
/// - Widget dependencies (for UI)
/// - Layer references (patches to apply)
/// - Asset bundle dependencies (for content)
/// - Player spawn configuration
/// - Environment settings
/// - Gameplay settings
/// - ECS resource initialization
/// - World logic (conditions, flow)
///
/// The engine loads a world by:
/// 1. Resolving all dependencies
/// 2. Loading asset bundles (PrefabRegistry, DefinitionRegistry populated)
/// 3. Loading plugins (components, systems registered)
/// 4. Loading widgets (UI created)
/// 5. Staging layers
/// 6. Loading root scene
/// 7. Applying default layers
/// 8. Initializing ECS resources from manifest
/// 9. Configuring environment
/// 10. Starting scheduler
/// 11. Emitting WorldLoadedEvent
///
/// Example manifest:
/// ```json
/// {
///   "package": {
///     "name": "world.arena_deathmatch",
///     "type": "world",
///     "version": "1.0.0"
///   },
///   "dependencies": {
///     "plugins": [
///       { "name": "gameplay.combat", "version": ">=1.0.0" }
///     ],
///     "widgets": [
///       { "name": "ui.game_hud", "version": ">=1.0.0" }
///     ],
///     "layers": [
///       { "name": "layer.night_mode", "version": ">=1.0.0", "optional": true }
///     ],
///     "assets": [
///       { "name": "assets.core_characters", "version": ">=1.0.0" }
///     ]
///   },
///   "root_scene": { ... },
///   "player_spawn": { ... },
///   "environment": { ... },
///   "gameplay": { ... },
///   "ecs_resources": { ... },
///   "world_logic": { ... },
///   "layers": ["layer.competitive_mode", "layer.night_lighting"],
///   "widgets": ["widget.debug_hud"],
///   "widgets_dev_only": ["widget.entity_inspector"]
/// }
/// ```
struct WorldPackageManifest {
    // Base manifest (identity, dependencies, etc.)
    PackageManifest base;

    // =========================================================================
    // Scene Configuration
    // =========================================================================

    /// Root scene definition
    RootSceneConfig root_scene;

    // =========================================================================
    // Player Configuration
    // =========================================================================

    /// Player spawn configuration
    std::optional<PlayerSpawnConfig> player_spawn;

    // =========================================================================
    // Environment Configuration
    // =========================================================================

    /// Environment settings
    EnvironmentConfig environment;

    // =========================================================================
    // Gameplay Configuration
    // =========================================================================

    /// Gameplay/match settings
    GameplayConfig gameplay;

    // =========================================================================
    // ECS Resources
    // =========================================================================

    /// Initial ECS resource values (resource_name -> initial_data)
    std::map<std::string, nlohmann::json> ecs_resources;

    // =========================================================================
    // World Logic
    // =========================================================================

    /// World logic configuration
    std::optional<WorldLogicConfig> world_logic;

    // =========================================================================
    // Layer References
    // =========================================================================

    /// Layers to apply automatically when world loads
    std::vector<std::string> layers;

    // =========================================================================
    // Widget References
    // =========================================================================

    /// Widgets to activate for all builds
    std::vector<std::string> widgets;

    /// Widgets to activate only in dev builds
    std::vector<std::string> widgets_dev_only;

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Load world manifest from JSON file
    [[nodiscard]] static void_core::Result<WorldPackageManifest> load(
        const std::filesystem::path& path);

    /// Parse from JSON string
    [[nodiscard]] static void_core::Result<WorldPackageManifest> from_json_string(
        const std::string& json_str,
        const std::filesystem::path& source_path = {});

    /// Parse from JSON object (after base manifest is parsed)
    [[nodiscard]] static void_core::Result<WorldPackageManifest> from_json(
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

    /// Validate world-specific rules
    [[nodiscard]] void_core::Result<void> validate() const;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if world has player spawn configuration
    [[nodiscard]] bool has_player_spawn() const noexcept {
        return player_spawn.has_value();
    }

    /// Check if world has any layers
    [[nodiscard]] bool has_layers() const noexcept {
        return !layers.empty();
    }

    /// Check if world has ECS resources
    [[nodiscard]] bool has_ecs_resources() const noexcept {
        return !ecs_resources.empty();
    }

    /// Check if world has logic configuration
    [[nodiscard]] bool has_world_logic() const noexcept {
        return world_logic.has_value() && world_logic->has_logic();
    }

    /// Get all widget names (including dev-only based on flag)
    [[nodiscard]] std::vector<std::string> all_widgets(bool include_dev = false) const;

    // =========================================================================
    // Path Resolution
    // =========================================================================

    /// Resolve a scene path relative to the package base path
    [[nodiscard]] std::filesystem::path resolve_scene_path(const std::string& scene_path) const;

    /// Resolve a prefab path relative to the package base path
    [[nodiscard]] std::filesystem::path resolve_prefab_path(const std::string& prefab_path) const;
};

// =============================================================================
// World Events
// =============================================================================

/// Event emitted when a world is fully loaded
struct WorldLoadedEvent {
    std::string world_name;                        ///< Name of the loaded world
    std::string world_package_path;                ///< Path to world package
};

/// Event emitted when a world is about to unload
struct WorldUnloadingEvent {
    std::string world_name;
};

/// Event emitted after world has unloaded
struct WorldUnloadedEvent {
    std::string world_name;
};

/// Event emitted when switching worlds
struct WorldSwitchEvent {
    std::string from_world;
    std::string to_world;
};

} // namespace void_package
