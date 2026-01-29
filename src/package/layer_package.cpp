/// @file layer_package.cpp
/// @brief Layer package manifest parsing implementation

#include <void_engine/package/layer_package.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>

namespace void_package {

// =============================================================================
// SpawnMode
// =============================================================================

const char* spawn_mode_to_string(SpawnMode mode) noexcept {
    switch (mode) {
        case SpawnMode::Immediate: return "immediate";
        case SpawnMode::Deferred: return "deferred";
        default: return "unknown";
    }
}

bool spawn_mode_from_string(const std::string& str, SpawnMode& out) noexcept {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "immediate") {
        out = SpawnMode::Immediate;
        return true;
    }
    if (lower == "deferred") {
        out = SpawnMode::Deferred;
        return true;
    }
    return false;
}

// =============================================================================
// AdditiveSceneEntry
// =============================================================================

void_core::Result<AdditiveSceneEntry> AdditiveSceneEntry::from_json(const nlohmann::json& j) {
    AdditiveSceneEntry entry;

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<AdditiveSceneEntry>("AdditiveSceneEntry: missing or invalid 'path'");
    }
    entry.path = j["path"].get<std::string>();

    if (j.contains("spawn_mode")) {
        if (!j["spawn_mode"].is_string()) {
            return void_core::Err<AdditiveSceneEntry>("AdditiveSceneEntry: 'spawn_mode' must be a string");
        }
        if (!spawn_mode_from_string(j["spawn_mode"].get<std::string>(), entry.spawn_mode)) {
            return void_core::Err<AdditiveSceneEntry>("AdditiveSceneEntry: invalid spawn_mode value");
        }
    }

    return entry;
}

nlohmann::json AdditiveSceneEntry::to_json() const {
    return {
        {"path", path},
        {"spawn_mode", spawn_mode_to_string(spawn_mode)}
    };
}

// =============================================================================
// SpawnerVolume
// =============================================================================

void_core::Result<SpawnerVolume> SpawnerVolume::from_json(const nlohmann::json& j) {
    SpawnerVolume vol;

    // Check for sphere volume (center + radius)
    if (j.contains("center") && j.contains("radius")) {
        vol.type = SpawnerVolume::Type::Sphere;

        if (!j["center"].is_array() || j["center"].size() != 3) {
            return void_core::Err<SpawnerVolume>("SpawnerVolume: 'center' must be [x, y, z] array");
        }
        vol.center = {
            j["center"][0].get<float>(),
            j["center"][1].get<float>(),
            j["center"][2].get<float>()
        };
        vol.radius = j["radius"].get<float>();
    }
    // Check for box volume (min + max)
    else if (j.contains("min") && j.contains("max")) {
        vol.type = SpawnerVolume::Type::Box;

        if (!j["min"].is_array() || j["min"].size() != 3) {
            return void_core::Err<SpawnerVolume>("SpawnerVolume: 'min' must be [x, y, z] array");
        }
        if (!j["max"].is_array() || j["max"].size() != 3) {
            return void_core::Err<SpawnerVolume>("SpawnerVolume: 'max' must be [x, y, z] array");
        }
        vol.min = {
            j["min"][0].get<float>(),
            j["min"][1].get<float>(),
            j["min"][2].get<float>()
        };
        vol.max = {
            j["max"][0].get<float>(),
            j["max"][1].get<float>(),
            j["max"][2].get<float>()
        };
    }
    else {
        return void_core::Err<SpawnerVolume>("SpawnerVolume: must have either (center, radius) or (min, max)");
    }

    return vol;
}

nlohmann::json SpawnerVolume::to_json() const {
    nlohmann::json j;
    if (type == Type::Sphere) {
        j["center"] = {center[0], center[1], center[2]};
        j["radius"] = radius;
    } else {
        j["min"] = {min[0], min[1], min[2]};
        j["max"] = {max[0], max[1], max[2]};
    }
    return j;
}

// =============================================================================
// SpawnerEntry
// =============================================================================

void_core::Result<SpawnerEntry> SpawnerEntry::from_json(const nlohmann::json& j) {
    SpawnerEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<SpawnerEntry>("SpawnerEntry: missing or invalid 'id'");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("volume") || !j["volume"].is_object()) {
        return void_core::Err<SpawnerEntry>("SpawnerEntry: missing or invalid 'volume'");
    }
    auto vol_result = SpawnerVolume::from_json(j["volume"]);
    if (!vol_result) {
        return void_core::Err<SpawnerEntry>("SpawnerEntry: " + vol_result.error().message());
    }
    entry.volume = *vol_result;

    if (!j.contains("prefab") || !j["prefab"].is_string()) {
        return void_core::Err<SpawnerEntry>("SpawnerEntry: missing or invalid 'prefab'");
    }
    entry.prefab = j["prefab"].get<std::string>();

    if (j.contains("spawn_rate")) {
        entry.spawn_rate = j["spawn_rate"].get<float>();
    }
    if (j.contains("max_active")) {
        entry.max_active = j["max_active"].get<int>();
    }
    if (j.contains("initial_delay")) {
        entry.initial_delay = j["initial_delay"].get<float>();
    }
    if (j.contains("spawn_on_apply")) {
        entry.spawn_on_apply = j["spawn_on_apply"].get<bool>();
    }

    return entry;
}

nlohmann::json SpawnerEntry::to_json() const {
    return {
        {"id", id},
        {"volume", volume.to_json()},
        {"prefab", prefab},
        {"spawn_rate", spawn_rate},
        {"max_active", max_active},
        {"initial_delay", initial_delay},
        {"spawn_on_apply", spawn_on_apply}
    };
}

// =============================================================================
// LightEntry
// =============================================================================

void_core::Result<LightEntry> LightEntry::from_json(const nlohmann::json& j) {
    LightEntry light;

    // Parse type
    if (j.contains("type")) {
        std::string type_str = j["type"].get<std::string>();
        std::transform(type_str.begin(), type_str.end(), type_str.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (type_str == "directional") {
            light.type = LightEntry::Type::Directional;
        } else if (type_str == "point") {
            light.type = LightEntry::Type::Point;
        } else if (type_str == "spot") {
            light.type = LightEntry::Type::Spot;
        } else {
            return void_core::Err<LightEntry>("LightEntry: invalid type '" + type_str + "'");
        }
    }

    // Parse position
    if (j.contains("position")) {
        if (!j["position"].is_array() || j["position"].size() != 3) {
            return void_core::Err<LightEntry>("LightEntry: 'position' must be [x, y, z] array");
        }
        light.position = {
            j["position"][0].get<float>(),
            j["position"][1].get<float>(),
            j["position"][2].get<float>()
        };
    }

    // Parse direction
    if (j.contains("direction")) {
        if (!j["direction"].is_array() || j["direction"].size() != 3) {
            return void_core::Err<LightEntry>("LightEntry: 'direction' must be [x, y, z] array");
        }
        light.direction = {
            j["direction"][0].get<float>(),
            j["direction"][1].get<float>(),
            j["direction"][2].get<float>()
        };
    }

    // Parse color
    if (j.contains("color")) {
        if (!j["color"].is_array() || j["color"].size() != 3) {
            return void_core::Err<LightEntry>("LightEntry: 'color' must be [r, g, b] array");
        }
        light.color = {
            j["color"][0].get<float>(),
            j["color"][1].get<float>(),
            j["color"][2].get<float>()
        };
    }

    if (j.contains("intensity")) {
        light.intensity = j["intensity"].get<float>();
    }
    if (j.contains("radius")) {
        light.radius = j["radius"].get<float>();
    }
    if (j.contains("inner_cone_angle")) {
        light.inner_cone_angle = j["inner_cone_angle"].get<float>();
    }
    if (j.contains("outer_cone_angle")) {
        light.outer_cone_angle = j["outer_cone_angle"].get<float>();
    }

    return light;
}

nlohmann::json LightEntry::to_json() const {
    nlohmann::json j;

    switch (type) {
        case Type::Directional: j["type"] = "directional"; break;
        case Type::Point: j["type"] = "point"; break;
        case Type::Spot: j["type"] = "spot"; break;
    }

    j["position"] = {position[0], position[1], position[2]};
    j["direction"] = {direction[0], direction[1], direction[2]};
    j["color"] = {color[0], color[1], color[2]};
    j["intensity"] = intensity;
    j["radius"] = radius;
    j["inner_cone_angle"] = inner_cone_angle;
    j["outer_cone_angle"] = outer_cone_angle;

    return j;
}

// =============================================================================
// SunOverride
// =============================================================================

void_core::Result<SunOverride> SunOverride::from_json(const nlohmann::json& j) {
    SunOverride sun;

    if (j.contains("direction")) {
        if (!j["direction"].is_array() || j["direction"].size() != 3) {
            return void_core::Err<SunOverride>("SunOverride: 'direction' must be [x, y, z] array");
        }
        sun.direction = {
            j["direction"][0].get<float>(),
            j["direction"][1].get<float>(),
            j["direction"][2].get<float>()
        };
    }

    if (j.contains("color")) {
        if (!j["color"].is_array() || j["color"].size() != 3) {
            return void_core::Err<SunOverride>("SunOverride: 'color' must be [r, g, b] array");
        }
        sun.color = {
            j["color"][0].get<float>(),
            j["color"][1].get<float>(),
            j["color"][2].get<float>()
        };
    }

    if (j.contains("intensity")) {
        sun.intensity = j["intensity"].get<float>();
    }

    return sun;
}

nlohmann::json SunOverride::to_json() const {
    return {
        {"direction", {direction[0], direction[1], direction[2]}},
        {"color", {color[0], color[1], color[2]}},
        {"intensity", intensity}
    };
}

// =============================================================================
// AmbientOverride
// =============================================================================

void_core::Result<AmbientOverride> AmbientOverride::from_json(const nlohmann::json& j) {
    AmbientOverride ambient;

    if (j.contains("color")) {
        if (!j["color"].is_array() || j["color"].size() != 3) {
            return void_core::Err<AmbientOverride>("AmbientOverride: 'color' must be [r, g, b] array");
        }
        ambient.color = {
            j["color"][0].get<float>(),
            j["color"][1].get<float>(),
            j["color"][2].get<float>()
        };
    }

    if (j.contains("intensity")) {
        ambient.intensity = j["intensity"].get<float>();
    }

    return ambient;
}

nlohmann::json AmbientOverride::to_json() const {
    return {
        {"color", {color[0], color[1], color[2]}},
        {"intensity", intensity}
    };
}

// =============================================================================
// LightingOverride
// =============================================================================

void_core::Result<LightingOverride> LightingOverride::from_json(const nlohmann::json& j) {
    LightingOverride lighting;

    // Parse sun override
    if (j.contains("sun") || j.contains("override_sun")) {
        const auto& sun_j = j.contains("sun") ? j["sun"] : j["override_sun"];
        auto sun_result = SunOverride::from_json(sun_j);
        if (!sun_result) {
            return void_core::Err<LightingOverride>("LightingOverride: " + sun_result.error().message());
        }
        lighting.sun = *sun_result;
    }

    // Parse additional lights
    if (j.contains("additional_lights")) {
        if (!j["additional_lights"].is_array()) {
            return void_core::Err<LightingOverride>("LightingOverride: 'additional_lights' must be an array");
        }
        for (const auto& light_j : j["additional_lights"]) {
            auto light_result = LightEntry::from_json(light_j);
            if (!light_result) {
                return void_core::Err<LightingOverride>("LightingOverride: " + light_result.error().message());
            }
            lighting.additional_lights.push_back(*light_result);
        }
    }

    // Parse ambient override
    if (j.contains("ambient") || j.contains("ambient_override")) {
        const auto& amb_j = j.contains("ambient") ? j["ambient"] : j["ambient_override"];
        auto amb_result = AmbientOverride::from_json(amb_j);
        if (!amb_result) {
            return void_core::Err<LightingOverride>("LightingOverride: " + amb_result.error().message());
        }
        lighting.ambient = *amb_result;
    }

    return lighting;
}

nlohmann::json LightingOverride::to_json() const {
    nlohmann::json j;

    if (sun.has_value()) {
        j["sun"] = sun->to_json();
    }

    if (!additional_lights.empty()) {
        j["additional_lights"] = nlohmann::json::array();
        for (const auto& light : additional_lights) {
            j["additional_lights"].push_back(light.to_json());
        }
    }

    if (ambient.has_value()) {
        j["ambient"] = ambient->to_json();
    }

    return j;
}

// =============================================================================
// FogConfig
// =============================================================================

void_core::Result<FogConfig> FogConfig::from_json(const nlohmann::json& j) {
    FogConfig fog;

    if (j.contains("enabled")) {
        fog.enabled = j["enabled"].get<bool>();
    }

    if (j.contains("color")) {
        if (!j["color"].is_array() || j["color"].size() != 3) {
            return void_core::Err<FogConfig>("FogConfig: 'color' must be [r, g, b] array");
        }
        fog.color = {
            j["color"][0].get<float>(),
            j["color"][1].get<float>(),
            j["color"][2].get<float>()
        };
    }

    if (j.contains("density")) {
        fog.density = j["density"].get<float>();
    }

    if (j.contains("height_falloff")) {
        fog.height_falloff = j["height_falloff"].get<float>();
    }

    return fog;
}

nlohmann::json FogConfig::to_json() const {
    return {
        {"enabled", enabled},
        {"color", {color[0], color[1], color[2]}},
        {"density", density},
        {"height_falloff", height_falloff}
    };
}

// =============================================================================
// PrecipitationConfig
// =============================================================================

void_core::Result<PrecipitationConfig> PrecipitationConfig::from_json(const nlohmann::json& j) {
    PrecipitationConfig precip;

    if (j.contains("type")) {
        std::string type_str = j["type"].get<std::string>();
        std::transform(type_str.begin(), type_str.end(), type_str.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (type_str == "none") {
            precip.type = PrecipitationConfig::Type::None;
        } else if (type_str == "rain") {
            precip.type = PrecipitationConfig::Type::Rain;
        } else if (type_str == "snow") {
            precip.type = PrecipitationConfig::Type::Snow;
        } else if (type_str == "hail") {
            precip.type = PrecipitationConfig::Type::Hail;
        } else {
            return void_core::Err<PrecipitationConfig>("PrecipitationConfig: invalid type '" + type_str + "'");
        }
    }

    if (j.contains("intensity")) {
        precip.intensity = j["intensity"].get<float>();
    }

    if (j.contains("wind_influence")) {
        precip.wind_influence = j["wind_influence"].get<float>();
    }

    return precip;
}

nlohmann::json PrecipitationConfig::to_json() const {
    std::string type_str;
    switch (type) {
        case Type::None: type_str = "none"; break;
        case Type::Rain: type_str = "rain"; break;
        case Type::Snow: type_str = "snow"; break;
        case Type::Hail: type_str = "hail"; break;
    }

    return {
        {"type", type_str},
        {"intensity", intensity},
        {"wind_influence", wind_influence}
    };
}

// =============================================================================
// WindZone
// =============================================================================

void_core::Result<WindZone> WindZone::from_json(const nlohmann::json& j) {
    WindZone zone;

    // Parse volume bounds
    if (j.contains("volume")) {
        const auto& vol = j["volume"];
        if (vol.contains("min") && vol["min"].is_array() && vol["min"].size() == 3) {
            zone.min = {
                vol["min"][0].get<float>(),
                vol["min"][1].get<float>(),
                vol["min"][2].get<float>()
            };
        }
        if (vol.contains("max") && vol["max"].is_array() && vol["max"].size() == 3) {
            zone.max = {
                vol["max"][0].get<float>(),
                vol["max"][1].get<float>(),
                vol["max"][2].get<float>()
            };
        }
    } else {
        // Direct min/max
        if (j.contains("min") && j["min"].is_array() && j["min"].size() == 3) {
            zone.min = {
                j["min"][0].get<float>(),
                j["min"][1].get<float>(),
                j["min"][2].get<float>()
            };
        }
        if (j.contains("max") && j["max"].is_array() && j["max"].size() == 3) {
            zone.max = {
                j["max"][0].get<float>(),
                j["max"][1].get<float>(),
                j["max"][2].get<float>()
            };
        }
    }

    if (j.contains("direction")) {
        if (!j["direction"].is_array() || j["direction"].size() != 3) {
            return void_core::Err<WindZone>("WindZone: 'direction' must be [x, y, z] array");
        }
        zone.direction = {
            j["direction"][0].get<float>(),
            j["direction"][1].get<float>(),
            j["direction"][2].get<float>()
        };
    }

    if (j.contains("strength")) {
        zone.strength = j["strength"].get<float>();
    }

    return zone;
}

nlohmann::json WindZone::to_json() const {
    return {
        {"min", {min[0], min[1], min[2]}},
        {"max", {max[0], max[1], max[2]}},
        {"direction", {direction[0], direction[1], direction[2]}},
        {"strength", strength}
    };
}

// =============================================================================
// WeatherOverride
// =============================================================================

void_core::Result<WeatherOverride> WeatherOverride::from_json(const nlohmann::json& j) {
    WeatherOverride weather;

    if (j.contains("fog")) {
        auto fog_result = FogConfig::from_json(j["fog"]);
        if (!fog_result) {
            return void_core::Err<WeatherOverride>("WeatherOverride: " + fog_result.error().message());
        }
        weather.fog = *fog_result;
    }

    if (j.contains("precipitation")) {
        auto precip_result = PrecipitationConfig::from_json(j["precipitation"]);
        if (!precip_result) {
            return void_core::Err<WeatherOverride>("WeatherOverride: " + precip_result.error().message());
        }
        weather.precipitation = *precip_result;
    }

    if (j.contains("wind_zones")) {
        if (!j["wind_zones"].is_array()) {
            return void_core::Err<WeatherOverride>("WeatherOverride: 'wind_zones' must be an array");
        }
        for (const auto& zone_j : j["wind_zones"]) {
            auto zone_result = WindZone::from_json(zone_j);
            if (!zone_result) {
                return void_core::Err<WeatherOverride>("WeatherOverride: " + zone_result.error().message());
            }
            weather.wind_zones.push_back(*zone_result);
        }
    }

    return weather;
}

nlohmann::json WeatherOverride::to_json() const {
    nlohmann::json j;

    if (fog.has_value()) {
        j["fog"] = fog->to_json();
    }

    if (precipitation.has_value()) {
        j["precipitation"] = precipitation->to_json();
    }

    if (!wind_zones.empty()) {
        j["wind_zones"] = nlohmann::json::array();
        for (const auto& zone : wind_zones) {
            j["wind_zones"].push_back(zone.to_json());
        }
    }

    return j;
}

// =============================================================================
// ObjectiveEntry
// =============================================================================

void_core::Result<ObjectiveEntry> ObjectiveEntry::from_json(const nlohmann::json& j) {
    ObjectiveEntry obj;

    if (!j.contains("type") || !j["type"].is_string()) {
        return void_core::Err<ObjectiveEntry>("ObjectiveEntry: missing or invalid 'type'");
    }
    obj.type = j["type"].get<std::string>();

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<ObjectiveEntry>("ObjectiveEntry: missing or invalid 'id'");
    }
    obj.id = j["id"].get<std::string>();

    if (j.contains("position")) {
        if (!j["position"].is_array() || j["position"].size() != 3) {
            return void_core::Err<ObjectiveEntry>("ObjectiveEntry: 'position' must be [x, y, z] array");
        }
        obj.position = {
            j["position"][0].get<float>(),
            j["position"][1].get<float>(),
            j["position"][2].get<float>()
        };
    }

    // Store all other fields as config
    obj.config = j;
    obj.config.erase("type");
    obj.config.erase("id");
    obj.config.erase("position");

    return obj;
}

nlohmann::json ObjectiveEntry::to_json() const {
    nlohmann::json j = config;
    j["type"] = type;
    j["id"] = id;
    j["position"] = {position[0], position[1], position[2]};
    return j;
}

// =============================================================================
// ModifierEntry
// =============================================================================

void_core::Result<ModifierEntry> ModifierEntry::from_json(const nlohmann::json& j) {
    ModifierEntry mod;

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<ModifierEntry>("ModifierEntry: missing or invalid 'path'");
    }
    mod.path = j["path"].get<std::string>();

    if (!j.contains("value")) {
        return void_core::Err<ModifierEntry>("ModifierEntry: missing 'value'");
    }
    mod.value = j["value"];

    return mod;
}

nlohmann::json ModifierEntry::to_json() const {
    return {
        {"path", path},
        {"value", value}
    };
}

std::vector<std::string> ModifierEntry::parse_path_segments() const {
    std::vector<std::string> segments;
    std::stringstream ss(path);
    std::string segment;

    while (std::getline(ss, segment, '.')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    return segments;
}

// =============================================================================
// LayerPackageManifest
// =============================================================================

void_core::Result<LayerPackageManifest> LayerPackageManifest::load(
    const std::filesystem::path& path)
{
    // Read file
    std::ifstream file(path);
    if (!file) {
        return void_core::Err<LayerPackageManifest>("Failed to open layer manifest: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return from_json_string(buffer.str(), path);
}

void_core::Result<LayerPackageManifest> LayerPackageManifest::from_json_string(
    const std::string& json_str,
    const std::filesystem::path& source_path)
{
    // Parse JSON
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error& e) {
        return void_core::Err<LayerPackageManifest>("JSON parse error: " + std::string(e.what()));
    }

    // Parse base manifest first
    auto base_result = PackageManifest::from_json_string(json_str, source_path);
    if (!base_result) {
        return void_core::Err<LayerPackageManifest>("Failed to parse base manifest: " + base_result.error().message());
    }

    // Verify it's a layer package
    if (base_result->type != PackageType::Layer) {
        return void_core::Err<LayerPackageManifest>("Package type is not 'layer'");
    }

    return from_json(j, std::move(*base_result));
}

void_core::Result<LayerPackageManifest> LayerPackageManifest::from_json(
    const nlohmann::json& j,
    PackageManifest base_manifest)
{
    LayerPackageManifest manifest;
    manifest.base = std::move(base_manifest);

    // Parse priority
    if (j.contains("priority")) {
        manifest.priority = j["priority"].get<int>();
    }

    // Parse additive scenes
    if (j.contains("additive_scenes")) {
        if (!j["additive_scenes"].is_array()) {
            return void_core::Err<LayerPackageManifest>("'additive_scenes' must be an array");
        }
        for (const auto& scene_j : j["additive_scenes"]) {
            auto scene_result = AdditiveSceneEntry::from_json(scene_j);
            if (!scene_result) {
                return void_core::Err<LayerPackageManifest>("additive_scenes: " + scene_result.error().message());
            }
            manifest.additive_scenes.push_back(*scene_result);
        }
    }

    // Parse spawners
    if (j.contains("spawners")) {
        if (!j["spawners"].is_array()) {
            return void_core::Err<LayerPackageManifest>("'spawners' must be an array");
        }
        for (const auto& spawner_j : j["spawners"]) {
            auto spawner_result = SpawnerEntry::from_json(spawner_j);
            if (!spawner_result) {
                return void_core::Err<LayerPackageManifest>("spawners: " + spawner_result.error().message());
            }
            manifest.spawners.push_back(*spawner_result);
        }
    }

    // Parse lighting
    if (j.contains("lighting")) {
        auto lighting_result = LightingOverride::from_json(j["lighting"]);
        if (!lighting_result) {
            return void_core::Err<LayerPackageManifest>("lighting: " + lighting_result.error().message());
        }
        manifest.lighting = *lighting_result;
    }

    // Parse weather
    if (j.contains("weather") || j.contains("weather_override")) {
        const auto& weather_j = j.contains("weather") ? j["weather"] : j["weather_override"];
        auto weather_result = WeatherOverride::from_json(weather_j);
        if (!weather_result) {
            return void_core::Err<LayerPackageManifest>("weather: " + weather_result.error().message());
        }
        manifest.weather = *weather_result;
    }

    // Parse objectives
    if (j.contains("objectives")) {
        if (!j["objectives"].is_array()) {
            return void_core::Err<LayerPackageManifest>("'objectives' must be an array");
        }
        for (const auto& obj_j : j["objectives"]) {
            auto obj_result = ObjectiveEntry::from_json(obj_j);
            if (!obj_result) {
                return void_core::Err<LayerPackageManifest>("objectives: " + obj_result.error().message());
            }
            manifest.objectives.push_back(*obj_result);
        }
    }

    // Parse modifiers
    if (j.contains("modifiers")) {
        if (!j["modifiers"].is_array()) {
            return void_core::Err<LayerPackageManifest>("'modifiers' must be an array");
        }
        for (const auto& mod_j : j["modifiers"]) {
            auto mod_result = ModifierEntry::from_json(mod_j);
            if (!mod_result) {
                return void_core::Err<LayerPackageManifest>("modifiers: " + mod_result.error().message());
            }
            manifest.modifiers.push_back(*mod_result);
        }
    }

    // Store optional raw JSON sections
    if (j.contains("audio_override")) {
        manifest.audio_override = j["audio_override"];
    }
    if (j.contains("navigation_patches")) {
        manifest.navigation_patches = j["navigation_patches"];
    }
    if (j.contains("debug_instrumentation")) {
        manifest.debug_instrumentation = j["debug_instrumentation"];
    }

    return manifest;
}

nlohmann::json LayerPackageManifest::to_json() const {
    // Start with base manifest serialization
    nlohmann::json j;

    // Package info
    j["package"] = {
        {"name", base.name},
        {"type", "layer"},
        {"version", base.version.to_string()}
    };

    // Dependencies
    if (base.has_dependencies()) {
        j["dependencies"] = nlohmann::json::object();
        if (!base.plugin_deps.empty()) {
            j["dependencies"]["plugins"] = nlohmann::json::array();
            for (const auto& dep : base.plugin_deps) {
                j["dependencies"]["plugins"].push_back({
                    {"name", dep.name},
                    {"version", dep.constraint.to_string()}
                });
            }
        }
        if (!base.asset_deps.empty()) {
            j["dependencies"]["assets"] = nlohmann::json::array();
            for (const auto& dep : base.asset_deps) {
                j["dependencies"]["assets"].push_back({
                    {"name", dep.name},
                    {"version", dep.constraint.to_string()}
                });
            }
        }
    }

    // Priority
    if (priority != DEFAULT_LAYER_PRIORITY) {
        j["priority"] = priority;
    }

    // Additive scenes
    if (!additive_scenes.empty()) {
        j["additive_scenes"] = nlohmann::json::array();
        for (const auto& scene : additive_scenes) {
            j["additive_scenes"].push_back(scene.to_json());
        }
    }

    // Spawners
    if (!spawners.empty()) {
        j["spawners"] = nlohmann::json::array();
        for (const auto& spawner : spawners) {
            j["spawners"].push_back(spawner.to_json());
        }
    }

    // Lighting
    if (lighting.has_value() && lighting->has_overrides()) {
        j["lighting"] = lighting->to_json();
    }

    // Weather
    if (weather.has_value() && weather->has_overrides()) {
        j["weather"] = weather->to_json();
    }

    // Objectives
    if (!objectives.empty()) {
        j["objectives"] = nlohmann::json::array();
        for (const auto& obj : objectives) {
            j["objectives"].push_back(obj.to_json());
        }
    }

    // Modifiers
    if (!modifiers.empty()) {
        j["modifiers"] = nlohmann::json::array();
        for (const auto& mod : modifiers) {
            j["modifiers"].push_back(mod.to_json());
        }
    }

    // Optional raw sections
    if (audio_override.has_value()) {
        j["audio_override"] = *audio_override;
    }
    if (navigation_patches.has_value()) {
        j["navigation_patches"] = *navigation_patches;
    }
    if (debug_instrumentation.has_value()) {
        j["debug_instrumentation"] = *debug_instrumentation;
    }

    return j;
}

void_core::Result<void> LayerPackageManifest::validate() const {
    // Validate base manifest
    auto base_result = base.validate();
    if (!base_result) {
        return base_result;
    }

    // Check for duplicate spawner IDs
    std::set<std::string> spawner_ids;
    for (const auto& spawner : spawners) {
        if (spawner_ids.count(spawner.id) > 0) {
            return void_core::Err("Duplicate spawner ID: " + spawner.id);
        }
        spawner_ids.insert(spawner.id);
    }

    // Check for duplicate objective IDs
    std::set<std::string> objective_ids;
    for (const auto& obj : objectives) {
        if (objective_ids.count(obj.id) > 0) {
            return void_core::Err("Duplicate objective ID: " + obj.id);
        }
        objective_ids.insert(obj.id);
    }

    // Validate modifier paths
    for (const auto& mod : modifiers) {
        if (mod.path.empty()) {
            return void_core::Err("Modifier path cannot be empty");
        }
        if (mod.path.front() == '.' || mod.path.back() == '.') {
            return void_core::Err("Modifier path cannot start or end with '.'");
        }
    }

    return void_core::Ok();
}

const SpawnerEntry* LayerPackageManifest::get_spawner(const std::string& id) const {
    for (const auto& spawner : spawners) {
        if (spawner.id == id) {
            return &spawner;
        }
    }
    return nullptr;
}

const ObjectiveEntry* LayerPackageManifest::get_objective(const std::string& id) const {
    for (const auto& obj : objectives) {
        if (obj.id == id) {
            return &obj;
        }
    }
    return nullptr;
}

std::filesystem::path LayerPackageManifest::resolve_scene_path(
    const std::string& scene_path) const
{
    if (std::filesystem::path(scene_path).is_absolute()) {
        return scene_path;
    }
    return base.base_path / scene_path;
}

std::filesystem::path LayerPackageManifest::resolve_prefab_path(
    const std::string& prefab_path) const
{
    if (std::filesystem::path(prefab_path).is_absolute()) {
        return prefab_path;
    }
    return base.base_path / prefab_path;
}

} // namespace void_package
