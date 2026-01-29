/// @file world_package.cpp
/// @brief World package manifest implementation

#include <void_engine/package/world_package.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace void_package {

// =============================================================================
// SpawnSelection Utilities
// =============================================================================

const char* spawn_selection_to_string(SpawnSelection selection) noexcept {
    switch (selection) {
        case SpawnSelection::RoundRobin: return "round_robin";
        case SpawnSelection::Random:     return "random";
        case SpawnSelection::Fixed:      return "fixed";
        case SpawnSelection::Weighted:   return "weighted";
    }
    return "unknown";
}

bool spawn_selection_from_string(const std::string& str, SpawnSelection& out) noexcept {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "round_robin" || lower == "roundrobin") {
        out = SpawnSelection::RoundRobin;
        return true;
    }
    if (lower == "random") {
        out = SpawnSelection::Random;
        return true;
    }
    if (lower == "fixed") {
        out = SpawnSelection::Fixed;
        return true;
    }
    if (lower == "weighted") {
        out = SpawnSelection::Weighted;
        return true;
    }
    return false;
}

// =============================================================================
// RootSceneConfig
// =============================================================================

void_core::Result<RootSceneConfig> RootSceneConfig::from_json(const nlohmann::json& j) {
    RootSceneConfig config;

    // Path is required
    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<RootSceneConfig>("root_scene.path is required and must be a string");
    }
    config.path = j["path"].get<std::string>();

    // Spawn points
    if (j.contains("spawn_points") && j["spawn_points"].is_array()) {
        for (const auto& sp : j["spawn_points"]) {
            if (sp.is_string()) {
                config.spawn_points.push_back(sp.get<std::string>());
            }
        }
    }

    // World bounds
    if (j.contains("world_bounds")) {
        const auto& bounds = j["world_bounds"];
        if (bounds.contains("min") && bounds["min"].is_array() && bounds["min"].size() >= 3) {
            config.world_bounds_min[0] = bounds["min"][0].get<float>();
            config.world_bounds_min[1] = bounds["min"][1].get<float>();
            config.world_bounds_min[2] = bounds["min"][2].get<float>();
        }
        if (bounds.contains("max") && bounds["max"].is_array() && bounds["max"].size() >= 3) {
            config.world_bounds_max[0] = bounds["max"][0].get<float>();
            config.world_bounds_max[1] = bounds["max"][1].get<float>();
            config.world_bounds_max[2] = bounds["max"][2].get<float>();
        }
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json RootSceneConfig::to_json() const {
    nlohmann::json j;
    j["path"] = path;

    if (!spawn_points.empty()) {
        j["spawn_points"] = spawn_points;
    }

    j["world_bounds"] = {
        {"min", {world_bounds_min[0], world_bounds_min[1], world_bounds_min[2]}},
        {"max", {world_bounds_max[0], world_bounds_max[1], world_bounds_max[2]}}
    };

    return j;
}

// =============================================================================
// PlayerSpawnConfig
// =============================================================================

void_core::Result<PlayerSpawnConfig> PlayerSpawnConfig::from_json(const nlohmann::json& j) {
    PlayerSpawnConfig config;

    // Prefab is required
    if (!j.contains("prefab") || !j["prefab"].is_string()) {
        return void_core::Err<PlayerSpawnConfig>("player_spawn.prefab is required and must be a string");
    }
    config.prefab = j["prefab"].get<std::string>();

    // Spawn selection
    if (j.contains("spawn_selection") && j["spawn_selection"].is_string()) {
        if (!spawn_selection_from_string(j["spawn_selection"].get<std::string>(),
                                          config.spawn_selection)) {
            return void_core::Err<PlayerSpawnConfig>("Invalid spawn_selection value: " +
                                  j["spawn_selection"].get<std::string>());
        }
    }

    // Initial inventory
    if (j.contains("initial_inventory") && !j["initial_inventory"].is_null()) {
        config.initial_inventory = j["initial_inventory"];
    }

    // Initial stats
    if (j.contains("initial_stats") && !j["initial_stats"].is_null()) {
        config.initial_stats = j["initial_stats"];
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json PlayerSpawnConfig::to_json() const {
    nlohmann::json j;
    j["prefab"] = prefab;
    j["spawn_selection"] = spawn_selection_to_string(spawn_selection);

    if (initial_inventory.has_value()) {
        j["initial_inventory"] = *initial_inventory;
    }
    if (initial_stats.has_value()) {
        j["initial_stats"] = *initial_stats;
    }

    return j;
}

// =============================================================================
// WeatherConfig
// =============================================================================

void_core::Result<WeatherConfig> WeatherConfig::from_json(const nlohmann::json& j) {
    WeatherConfig config;

    if (j.contains("profile") && j["profile"].is_string()) {
        config.profile = j["profile"].get<std::string>();
    }
    if (j.contains("intensity") && j["intensity"].is_number()) {
        config.intensity = j["intensity"].get<float>();
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json WeatherConfig::to_json() const {
    nlohmann::json j;
    j["profile"] = profile;
    j["intensity"] = intensity;
    return j;
}

// =============================================================================
// PostProcessConfig
// =============================================================================

void_core::Result<PostProcessConfig> PostProcessConfig::from_json(const nlohmann::json& j) {
    PostProcessConfig config;

    if (j.contains("profile") && j["profile"].is_string()) {
        config.profile = j["profile"].get<std::string>();
    }
    if (j.contains("exposure") && j["exposure"].is_number()) {
        config.exposure = j["exposure"].get<float>();
    }
    if (j.contains("bloom_intensity") && j["bloom_intensity"].is_number()) {
        config.bloom_intensity = j["bloom_intensity"].get<float>();
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json PostProcessConfig::to_json() const {
    nlohmann::json j;
    j["profile"] = profile;
    j["exposure"] = exposure;
    j["bloom_intensity"] = bloom_intensity;
    return j;
}

// =============================================================================
// EnvironmentConfig
// =============================================================================

void_core::Result<EnvironmentConfig> EnvironmentConfig::from_json(const nlohmann::json& j) {
    EnvironmentConfig config;

    if (j.contains("time_of_day") && j["time_of_day"].is_number()) {
        config.time_of_day = j["time_of_day"].get<float>();
    }
    if (j.contains("skybox") && j["skybox"].is_string()) {
        config.skybox = j["skybox"].get<std::string>();
    }

    // Weather
    if (j.contains("weather") && j["weather"].is_object()) {
        auto weather_result = WeatherConfig::from_json(j["weather"]);
        if (!weather_result) {
            return void_core::Err<EnvironmentConfig>("Failed to parse weather: " + weather_result.error().message());
        }
        config.weather = std::move(*weather_result);
    }

    // Post-process
    if (j.contains("post_process") && j["post_process"].is_object()) {
        auto pp_result = PostProcessConfig::from_json(j["post_process"]);
        if (!pp_result) {
            return void_core::Err<EnvironmentConfig>("Failed to parse post_process: " + pp_result.error().message());
        }
        config.post_process = std::move(*pp_result);
    }

    // Ambient
    if (j.contains("ambient_color") && j["ambient_color"].is_array() && j["ambient_color"].size() >= 3) {
        config.ambient_color[0] = j["ambient_color"][0].get<float>();
        config.ambient_color[1] = j["ambient_color"][1].get<float>();
        config.ambient_color[2] = j["ambient_color"][2].get<float>();
    }
    if (j.contains("ambient_intensity") && j["ambient_intensity"].is_number()) {
        config.ambient_intensity = j["ambient_intensity"].get<float>();
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json EnvironmentConfig::to_json() const {
    nlohmann::json j;
    j["time_of_day"] = time_of_day;
    j["skybox"] = skybox;

    if (weather.has_value()) {
        j["weather"] = weather->to_json();
    }
    if (post_process.has_value()) {
        j["post_process"] = post_process->to_json();
    }

    j["ambient_color"] = {ambient_color[0], ambient_color[1], ambient_color[2]};
    j["ambient_intensity"] = ambient_intensity;

    return j;
}

// =============================================================================
// GameplayConfig
// =============================================================================

void_core::Result<GameplayConfig> GameplayConfig::from_json(const nlohmann::json& j) {
    GameplayConfig config;

    if (j.contains("difficulty") && j["difficulty"].is_string()) {
        config.difficulty = j["difficulty"].get<std::string>();
    }
    if (j.contains("match_length_seconds") && j["match_length_seconds"].is_number_integer()) {
        config.match_length_seconds = j["match_length_seconds"].get<int>();
    }
    if (j.contains("score_limit") && j["score_limit"].is_number_integer()) {
        config.score_limit = j["score_limit"].get<int>();
    }
    if (j.contains("friendly_fire") && j["friendly_fire"].is_boolean()) {
        config.friendly_fire = j["friendly_fire"].get<bool>();
    }
    if (j.contains("respawn_delay_seconds") && j["respawn_delay_seconds"].is_number_integer()) {
        config.respawn_delay_seconds = j["respawn_delay_seconds"].get<int>();
    }

    // Ruleset flags
    if (j.contains("ruleset_flags") && j["ruleset_flags"].is_object()) {
        for (auto& [key, value] : j["ruleset_flags"].items()) {
            config.ruleset_flags[key] = value;
        }
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json GameplayConfig::to_json() const {
    nlohmann::json j;
    j["difficulty"] = difficulty;
    j["match_length_seconds"] = match_length_seconds;
    j["score_limit"] = score_limit;
    j["friendly_fire"] = friendly_fire;
    j["respawn_delay_seconds"] = respawn_delay_seconds;

    if (!ruleset_flags.empty()) {
        j["ruleset_flags"] = nlohmann::json::object();
        for (const auto& [key, value] : ruleset_flags) {
            j["ruleset_flags"][key] = value;
        }
    }

    return j;
}

// =============================================================================
// WinCondition
// =============================================================================

void_core::Result<WinCondition> WinCondition::from_json(const nlohmann::json& j) {
    WinCondition cond;

    if (!j.contains("type") || !j["type"].is_string()) {
        return void_core::Err<WinCondition>("win_condition.type is required");
    }
    cond.type = j["type"].get<std::string>();

    // Store all other fields as config
    cond.config = j;
    cond.config.erase("type");

    return void_core::Ok(std::move(cond));
}

nlohmann::json WinCondition::to_json() const {
    nlohmann::json j = config;
    j["type"] = type;
    return j;
}

// =============================================================================
// LoseCondition
// =============================================================================

void_core::Result<LoseCondition> LoseCondition::from_json(const nlohmann::json& j) {
    LoseCondition cond;

    if (!j.contains("type") || !j["type"].is_string()) {
        return void_core::Err<LoseCondition>("lose_condition.type is required");
    }
    cond.type = j["type"].get<std::string>();

    // Store all other fields as config
    cond.config = j;
    cond.config.erase("type");

    return void_core::Ok(std::move(cond));
}

nlohmann::json LoseCondition::to_json() const {
    nlohmann::json j = config;
    j["type"] = type;
    return j;
}

// =============================================================================
// RoundFlowConfig
// =============================================================================

void_core::Result<RoundFlowConfig> RoundFlowConfig::from_json(const nlohmann::json& j) {
    RoundFlowConfig config;

    if (j.contains("warmup_duration") && j["warmup_duration"].is_number_integer()) {
        config.warmup_duration = j["warmup_duration"].get<int>();
    }
    if (j.contains("round_duration") && j["round_duration"].is_number_integer()) {
        config.round_duration = j["round_duration"].get<int>();
    }
    if (j.contains("intermission_duration") && j["intermission_duration"].is_number_integer()) {
        config.intermission_duration = j["intermission_duration"].get<int>();
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json RoundFlowConfig::to_json() const {
    nlohmann::json j;
    j["warmup_duration"] = warmup_duration;
    j["round_duration"] = round_duration;
    j["intermission_duration"] = intermission_duration;
    return j;
}

// =============================================================================
// WorldLogicConfig
// =============================================================================

void_core::Result<WorldLogicConfig> WorldLogicConfig::from_json(const nlohmann::json& j) {
    WorldLogicConfig config;

    // Win conditions
    if (j.contains("win_conditions") && j["win_conditions"].is_array()) {
        for (const auto& cond : j["win_conditions"]) {
            auto result = WinCondition::from_json(cond);
            if (!result) {
                return void_core::Err<WorldLogicConfig>("Failed to parse win_condition: " + result.error().message());
            }
            config.win_conditions.push_back(std::move(*result));
        }
    }

    // Lose conditions
    if (j.contains("lose_conditions") && j["lose_conditions"].is_array()) {
        for (const auto& cond : j["lose_conditions"]) {
            auto result = LoseCondition::from_json(cond);
            if (!result) {
                return void_core::Err<WorldLogicConfig>("Failed to parse lose_condition: " + result.error().message());
            }
            config.lose_conditions.push_back(std::move(*result));
        }
    }

    // Round flow
    if (j.contains("round_flow") && j["round_flow"].is_object()) {
        auto result = RoundFlowConfig::from_json(j["round_flow"]);
        if (!result) {
            return void_core::Err<WorldLogicConfig>("Failed to parse round_flow: " + result.error().message());
        }
        config.round_flow = std::move(*result);
    }

    // State machine
    if (j.contains("state_machine") && j["state_machine"].is_string()) {
        config.state_machine = j["state_machine"].get<std::string>();
    }

    return void_core::Ok(std::move(config));
}

nlohmann::json WorldLogicConfig::to_json() const {
    nlohmann::json j;

    if (!win_conditions.empty()) {
        j["win_conditions"] = nlohmann::json::array();
        for (const auto& cond : win_conditions) {
            j["win_conditions"].push_back(cond.to_json());
        }
    }

    if (!lose_conditions.empty()) {
        j["lose_conditions"] = nlohmann::json::array();
        for (const auto& cond : lose_conditions) {
            j["lose_conditions"].push_back(cond.to_json());
        }
    }

    if (round_flow.has_value()) {
        j["round_flow"] = round_flow->to_json();
    }

    if (state_machine.has_value()) {
        j["state_machine"] = *state_machine;
    }

    return j;
}

// =============================================================================
// WorldPackageManifest
// =============================================================================

void_core::Result<WorldPackageManifest> WorldPackageManifest::load(
    const std::filesystem::path& path) {

    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        return void_core::Err<WorldPackageManifest>("Failed to open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return from_json_string(buffer.str(), path);
}

void_core::Result<WorldPackageManifest> WorldPackageManifest::from_json_string(
    const std::string& json_str,
    const std::filesystem::path& source_path) {

    try {
        auto j = nlohmann::json::parse(json_str);

        // Parse base manifest first
        auto base_result = PackageManifest::from_json_string(json_str, source_path);
        if (!base_result) {
            return void_core::Err<WorldPackageManifest>("Failed to parse base manifest: " + base_result.error().message());
        }

        // Validate type
        if (base_result->type != PackageType::World) {
            return void_core::Err<WorldPackageManifest>("Package type must be 'world', got '" +
                                  std::string(package_type_to_string(base_result->type)) + "'");
        }

        return from_json(j, std::move(*base_result));
    } catch (const nlohmann::json::exception& e) {
        return void_core::Err<WorldPackageManifest>(std::string("JSON parse error: ") + e.what());
    }
}

void_core::Result<WorldPackageManifest> WorldPackageManifest::from_json(
    const nlohmann::json& j,
    PackageManifest base_manifest) {

    WorldPackageManifest manifest;
    manifest.base = std::move(base_manifest);

    // Root scene (required)
    if (!j.contains("root_scene")) {
        return void_core::Err<WorldPackageManifest>("root_scene is required for world packages");
    }
    auto root_scene_result = RootSceneConfig::from_json(j["root_scene"]);
    if (!root_scene_result) {
        return void_core::Err<WorldPackageManifest>("Failed to parse root_scene: " + root_scene_result.error().message());
    }
    manifest.root_scene = std::move(*root_scene_result);

    // Player spawn (optional)
    if (j.contains("player_spawn") && j["player_spawn"].is_object()) {
        auto spawn_result = PlayerSpawnConfig::from_json(j["player_spawn"]);
        if (!spawn_result) {
            return void_core::Err<WorldPackageManifest>("Failed to parse player_spawn: " + spawn_result.error().message());
        }
        manifest.player_spawn = std::move(*spawn_result);
    }

    // Environment (optional, has defaults)
    if (j.contains("environment") && j["environment"].is_object()) {
        auto env_result = EnvironmentConfig::from_json(j["environment"]);
        if (!env_result) {
            return void_core::Err<WorldPackageManifest>("Failed to parse environment: " + env_result.error().message());
        }
        manifest.environment = std::move(*env_result);
    }

    // Gameplay (optional, has defaults)
    if (j.contains("gameplay") && j["gameplay"].is_object()) {
        auto gameplay_result = GameplayConfig::from_json(j["gameplay"]);
        if (!gameplay_result) {
            return void_core::Err<WorldPackageManifest>("Failed to parse gameplay: " + gameplay_result.error().message());
        }
        manifest.gameplay = std::move(*gameplay_result);
    }

    // ECS resources
    if (j.contains("ecs_resources") && j["ecs_resources"].is_object()) {
        for (auto& [key, value] : j["ecs_resources"].items()) {
            manifest.ecs_resources[key] = value;
        }
    }

    // World logic (optional)
    if (j.contains("world_logic") && j["world_logic"].is_object()) {
        auto logic_result = WorldLogicConfig::from_json(j["world_logic"]);
        if (!logic_result) {
            return void_core::Err<WorldPackageManifest>("Failed to parse world_logic: " + logic_result.error().message());
        }
        manifest.world_logic = std::move(*logic_result);
    }

    // Layers
    if (j.contains("layers") && j["layers"].is_array()) {
        for (const auto& layer : j["layers"]) {
            if (layer.is_string()) {
                manifest.layers.push_back(layer.get<std::string>());
            }
        }
    }

    // Widgets
    if (j.contains("widgets") && j["widgets"].is_array()) {
        for (const auto& widget : j["widgets"]) {
            if (widget.is_string()) {
                manifest.widgets.push_back(widget.get<std::string>());
            }
        }
    }

    // Dev-only widgets
    if (j.contains("widgets_dev_only") && j["widgets_dev_only"].is_array()) {
        for (const auto& widget : j["widgets_dev_only"]) {
            if (widget.is_string()) {
                manifest.widgets_dev_only.push_back(widget.get<std::string>());
            }
        }
    }

    // Validate
    auto validate_result = manifest.validate();
    if (!validate_result) {
        return void_core::Err<WorldPackageManifest>("Validation failed: " + validate_result.error().message());
    }

    return void_core::Ok(std::move(manifest));
}

nlohmann::json WorldPackageManifest::to_json() const {
    // Start with base manifest JSON
    nlohmann::json j;

    // Package block
    j["package"] = {
        {"name", base.name},
        {"type", package_type_to_string(base.type)},
        {"version", base.version.to_string()}
    };

    // Dependencies
    nlohmann::json deps = nlohmann::json::object();
    if (!base.plugin_deps.empty()) {
        deps["plugins"] = nlohmann::json::array();
        for (const auto& dep : base.plugin_deps) {
            nlohmann::json d;
            d["name"] = dep.name;
            d["version"] = dep.constraint.to_string();
            if (dep.optional) d["optional"] = true;
            deps["plugins"].push_back(d);
        }
    }
    if (!base.widget_deps.empty()) {
        deps["widgets"] = nlohmann::json::array();
        for (const auto& dep : base.widget_deps) {
            nlohmann::json d;
            d["name"] = dep.name;
            d["version"] = dep.constraint.to_string();
            if (dep.optional) d["optional"] = true;
            deps["widgets"].push_back(d);
        }
    }
    if (!base.layer_deps.empty()) {
        deps["layers"] = nlohmann::json::array();
        for (const auto& dep : base.layer_deps) {
            nlohmann::json d;
            d["name"] = dep.name;
            d["version"] = dep.constraint.to_string();
            if (dep.optional) d["optional"] = true;
            deps["layers"].push_back(d);
        }
    }
    if (!base.asset_deps.empty()) {
        deps["assets"] = nlohmann::json::array();
        for (const auto& dep : base.asset_deps) {
            nlohmann::json d;
            d["name"] = dep.name;
            d["version"] = dep.constraint.to_string();
            if (dep.optional) d["optional"] = true;
            deps["assets"].push_back(d);
        }
    }
    if (!deps.empty()) {
        j["dependencies"] = deps;
    }

    // World-specific sections
    j["root_scene"] = root_scene.to_json();

    if (player_spawn.has_value()) {
        j["player_spawn"] = player_spawn->to_json();
    }

    j["environment"] = environment.to_json();
    j["gameplay"] = gameplay.to_json();

    if (!ecs_resources.empty()) {
        j["ecs_resources"] = nlohmann::json::object();
        for (const auto& [key, value] : ecs_resources) {
            j["ecs_resources"][key] = value;
        }
    }

    if (world_logic.has_value()) {
        j["world_logic"] = world_logic->to_json();
    }

    if (!layers.empty()) {
        j["layers"] = layers;
    }

    if (!widgets.empty()) {
        j["widgets"] = widgets;
    }

    if (!widgets_dev_only.empty()) {
        j["widgets_dev_only"] = widgets_dev_only;
    }

    return j;
}

void_core::Result<void> WorldPackageManifest::validate() const {
    // Base manifest validation
    auto base_result = base.validate();
    if (!base_result) {
        return base_result;
    }

    // Root scene path required
    if (root_scene.path.empty()) {
        return void_core::Err("root_scene.path cannot be empty");
    }

    // Player spawn prefab if configured
    if (player_spawn.has_value() && player_spawn->prefab.empty()) {
        return void_core::Err("player_spawn.prefab cannot be empty when player_spawn is configured");
    }

    return void_core::Ok();
}

std::vector<std::string> WorldPackageManifest::all_widgets(bool include_dev) const {
    std::vector<std::string> result = widgets;
    if (include_dev) {
        result.insert(result.end(), widgets_dev_only.begin(), widgets_dev_only.end());
    }
    return result;
}

std::filesystem::path WorldPackageManifest::resolve_scene_path(const std::string& scene_path) const {
    if (base.base_path.empty()) {
        return scene_path;
    }
    return base.base_path / scene_path;
}

std::filesystem::path WorldPackageManifest::resolve_prefab_path(const std::string& prefab_path) const {
    if (base.base_path.empty()) {
        return prefab_path;
    }
    return base.base_path / prefab_path;
}

} // namespace void_package
