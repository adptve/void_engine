/// @file asset_bundle.cpp
/// @brief Implementation of asset bundle manifest parsing and validation

#include <void_engine/package/asset_bundle.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace void_package {

// =============================================================================
// MeshEntry
// =============================================================================

void_core::Result<MeshEntry> MeshEntry::from_json(const nlohmann::json& j) {
    MeshEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<MeshEntry>("MeshEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<MeshEntry>("MeshEntry '" + entry.id + "': missing or invalid 'path' field");
    }
    entry.path = j["path"].get<std::string>();

    if (j.contains("lod_levels") && j["lod_levels"].is_array()) {
        for (const auto& lod : j["lod_levels"]) {
            if (lod.is_string()) {
                entry.lod_paths.push_back(lod.get<std::string>());
            }
        }
    }

    if (j.contains("collision") && j["collision"].is_string()) {
        entry.collision_path = j["collision"].get<std::string>();
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json MeshEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["path"] = path;
    if (!lod_paths.empty()) {
        j["lod_levels"] = lod_paths;
    }
    if (collision_path) {
        j["collision"] = *collision_path;
    }
    return j;
}

// =============================================================================
// TextureEntry
// =============================================================================

void_core::Result<TextureEntry> TextureEntry::from_json(const nlohmann::json& j) {
    TextureEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<TextureEntry>("TextureEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<TextureEntry>("TextureEntry '" + entry.id + "': missing or invalid 'path' field");
    }
    entry.path = j["path"].get<std::string>();

    if (j.contains("format") && j["format"].is_string()) {
        entry.format = j["format"].get<std::string>();
    }

    if (j.contains("mipmaps") && j["mipmaps"].is_boolean()) {
        entry.mipmaps = j["mipmaps"].get<bool>();
    }

    if (j.contains("srgb") && j["srgb"].is_boolean()) {
        entry.srgb = j["srgb"].get<bool>();
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json TextureEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["path"] = path;
    if (!format.empty()) {
        j["format"] = format;
    }
    j["mipmaps"] = mipmaps;
    j["srgb"] = srgb;
    return j;
}

// =============================================================================
// MaterialEntry
// =============================================================================

void_core::Result<MaterialEntry> MaterialEntry::from_json(const nlohmann::json& j) {
    MaterialEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<MaterialEntry>("MaterialEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("shader") || !j["shader"].is_string()) {
        return void_core::Err<MaterialEntry>("MaterialEntry '" + entry.id + "': missing or invalid 'shader' field");
    }
    entry.shader = j["shader"].get<std::string>();

    if (j.contains("textures") && j["textures"].is_object()) {
        for (auto& [key, value] : j["textures"].items()) {
            if (value.is_string()) {
                entry.textures[key] = value.get<std::string>();
            }
        }
    }

    if (j.contains("parameters") && j["parameters"].is_object()) {
        entry.parameters = j["parameters"];
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json MaterialEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["shader"] = shader;
    if (!textures.empty()) {
        j["textures"] = textures;
    }
    if (!parameters.empty()) {
        j["parameters"] = parameters;
    }
    return j;
}

// =============================================================================
// AnimationEvent
// =============================================================================

void_core::Result<AnimationEvent> AnimationEvent::from_json(const nlohmann::json& j) {
    AnimationEvent entry;

    if (!j.contains("time") || !j["time"].is_number()) {
        return void_core::Err<AnimationEvent>("AnimationEvent: missing or invalid 'time' field");
    }
    entry.time = j["time"].get<float>();

    if (!j.contains("event") || !j["event"].is_string()) {
        return void_core::Err<AnimationEvent>("AnimationEvent: missing or invalid 'event' field");
    }
    entry.event = j["event"].get<std::string>();

    return void_core::Ok(std::move(entry));
}

nlohmann::json AnimationEvent::to_json() const {
    return nlohmann::json{{"time", time}, {"event", event}};
}

// =============================================================================
// AnimationEntry
// =============================================================================

void_core::Result<AnimationEntry> AnimationEntry::from_json(const nlohmann::json& j) {
    AnimationEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<AnimationEntry>("AnimationEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<AnimationEntry>("AnimationEntry '" + entry.id + "': missing or invalid 'path' field");
    }
    entry.path = j["path"].get<std::string>();

    if (j.contains("loop") && j["loop"].is_boolean()) {
        entry.loop = j["loop"].get<bool>();
    }

    if (j.contains("root_motion") && j["root_motion"].is_boolean()) {
        entry.root_motion = j["root_motion"].get<bool>();
    }

    if (j.contains("events") && j["events"].is_array()) {
        for (const auto& evt : j["events"]) {
            auto result = AnimationEvent::from_json(evt);
            if (!result) {
                return void_core::Err<AnimationEntry>("AnimationEntry '" + entry.id + "': " + result.error().message());
            }
            entry.events.push_back(std::move(*result));
        }
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json AnimationEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["path"] = path;
    j["loop"] = loop;
    if (root_motion) {
        j["root_motion"] = root_motion;
    }
    if (!events.empty()) {
        j["events"] = nlohmann::json::array();
        for (const auto& evt : events) {
            j["events"].push_back(evt.to_json());
        }
    }
    return j;
}

// =============================================================================
// BlendSpaceSample
// =============================================================================

void_core::Result<BlendSpaceSample> BlendSpaceSample::from_json(const nlohmann::json& j) {
    BlendSpaceSample sample;

    if (!j.contains("position") || !j["position"].is_array()) {
        return void_core::Err<BlendSpaceSample>("BlendSpaceSample: missing or invalid 'position' field");
    }
    for (const auto& v : j["position"]) {
        if (v.is_number()) {
            sample.position.push_back(v.get<float>());
        }
    }

    if (!j.contains("animation") || !j["animation"].is_string()) {
        return void_core::Err<BlendSpaceSample>("BlendSpaceSample: missing or invalid 'animation' field");
    }
    sample.animation = j["animation"].get<std::string>();

    return void_core::Ok(std::move(sample));
}

nlohmann::json BlendSpaceSample::to_json() const {
    return nlohmann::json{{"position", position}, {"animation", animation}};
}

// =============================================================================
// BlendSpaceEntry
// =============================================================================

void_core::Result<BlendSpaceEntry> BlendSpaceEntry::from_json(const nlohmann::json& j) {
    BlendSpaceEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<BlendSpaceEntry>("BlendSpaceEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("type") || !j["type"].is_string()) {
        return void_core::Err<BlendSpaceEntry>("BlendSpaceEntry '" + entry.id + "': missing or invalid 'type' field");
    }
    entry.type = j["type"].get<std::string>();

    if (!j.contains("axis_x") || !j["axis_x"].is_string()) {
        return void_core::Err<BlendSpaceEntry>("BlendSpaceEntry '" + entry.id + "': missing or invalid 'axis_x' field");
    }
    entry.axis_x = j["axis_x"].get<std::string>();

    if (j.contains("axis_y") && j["axis_y"].is_string()) {
        entry.axis_y = j["axis_y"].get<std::string>();
    }

    if (j.contains("samples") && j["samples"].is_array()) {
        for (const auto& s : j["samples"]) {
            auto result = BlendSpaceSample::from_json(s);
            if (!result) {
                return void_core::Err<BlendSpaceEntry>("BlendSpaceEntry '" + entry.id + "': " + result.error().message());
            }
            entry.samples.push_back(std::move(*result));
        }
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json BlendSpaceEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["type"] = type;
    j["axis_x"] = axis_x;
    if (axis_y) {
        j["axis_y"] = *axis_y;
    }
    if (!samples.empty()) {
        j["samples"] = nlohmann::json::array();
        for (const auto& s : samples) {
            j["samples"].push_back(s.to_json());
        }
    }
    return j;
}

// =============================================================================
// AudioEntry
// =============================================================================

void_core::Result<AudioEntry> AudioEntry::from_json(const nlohmann::json& j) {
    AudioEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<AudioEntry>("AudioEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<AudioEntry>("AudioEntry '" + entry.id + "': missing or invalid 'path' field");
    }
    entry.path = j["path"].get<std::string>();

    if (j.contains("type") && j["type"].is_string()) {
        entry.type = j["type"].get<std::string>();
    }

    if (j.contains("volume") && j["volume"].is_number()) {
        entry.volume = j["volume"].get<float>();
    }

    if (j.contains("loop") && j["loop"].is_boolean()) {
        entry.loop = j["loop"].get<bool>();
    }

    if (j.contains("variations") && j["variations"].is_array()) {
        for (const auto& v : j["variations"]) {
            if (v.is_string()) {
                entry.variations.push_back(v.get<std::string>());
            }
        }
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json AudioEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["path"] = path;
    if (!type.empty()) {
        j["type"] = type;
    }
    if (volume != 1.0f) {
        j["volume"] = volume;
    }
    if (loop) {
        j["loop"] = loop;
    }
    if (!variations.empty()) {
        j["variations"] = variations;
    }
    return j;
}

// =============================================================================
// VfxEntry
// =============================================================================

void_core::Result<VfxEntry> VfxEntry::from_json(const nlohmann::json& j) {
    VfxEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<VfxEntry>("VfxEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<VfxEntry>("VfxEntry '" + entry.id + "': missing or invalid 'path' field");
    }
    entry.path = j["path"].get<std::string>();

    if (j.contains("type") && j["type"].is_string()) {
        entry.type = j["type"].get<std::string>();
    }

    if (j.contains("lifetime") && j["lifetime"].is_number()) {
        entry.lifetime = j["lifetime"].get<float>();
    }

    if (j.contains("columns") && j["columns"].is_number_integer()) {
        entry.columns = j["columns"].get<int>();
    }

    if (j.contains("rows") && j["rows"].is_number_integer()) {
        entry.rows = j["rows"].get<int>();
    }

    if (j.contains("fps") && j["fps"].is_number_integer()) {
        entry.fps = j["fps"].get<int>();
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json VfxEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["path"] = path;
    if (!type.empty()) {
        j["type"] = type;
    }
    if (lifetime) {
        j["lifetime"] = *lifetime;
    }
    if (columns) {
        j["columns"] = *columns;
    }
    if (rows) {
        j["rows"] = *rows;
    }
    if (fps) {
        j["fps"] = *fps;
    }
    return j;
}

// =============================================================================
// ShaderEntry
// =============================================================================

void_core::Result<ShaderEntry> ShaderEntry::from_json(const nlohmann::json& j) {
    ShaderEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<ShaderEntry>("ShaderEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (j.contains("vertex") && j["vertex"].is_string()) {
        entry.vertex = j["vertex"].get<std::string>();
    }

    if (j.contains("fragment") && j["fragment"].is_string()) {
        entry.fragment = j["fragment"].get<std::string>();
    }

    if (j.contains("compute") && j["compute"].is_string()) {
        entry.compute = j["compute"].get<std::string>();
    }

    if (j.contains("variants") && j["variants"].is_array()) {
        for (const auto& v : j["variants"]) {
            if (v.is_string()) {
                entry.variants.push_back(v.get<std::string>());
            }
        }
    }

    // At least one shader path must be provided
    if (!entry.vertex && !entry.fragment && !entry.compute) {
        return void_core::Err<ShaderEntry>("ShaderEntry '" + entry.id + "': must have at least one of vertex/fragment/compute");
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json ShaderEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    if (vertex) {
        j["vertex"] = *vertex;
    }
    if (fragment) {
        j["fragment"] = *fragment;
    }
    if (compute) {
        j["compute"] = *compute;
    }
    if (!variants.empty()) {
        j["variants"] = variants;
    }
    return j;
}

// =============================================================================
// PrefabEntry
// =============================================================================

void_core::Result<PrefabEntry> PrefabEntry::from_json(const nlohmann::json& j) {
    PrefabEntry entry;

    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<PrefabEntry>("PrefabEntry: missing or invalid 'id' field");
    }
    entry.id = j["id"].get<std::string>();

    if (j.contains("components") && j["components"].is_object()) {
        for (auto& [key, value] : j["components"].items()) {
            entry.components[key] = value;
        }
    }

    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& t : j["tags"]) {
            if (t.is_string()) {
                entry.tags.push_back(t.get<std::string>());
            }
        }
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json PrefabEntry::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    if (!components.empty()) {
        j["components"] = components;
    }
    if (!tags.empty()) {
        j["tags"] = tags;
    }
    return j;
}

// =============================================================================
// DefinitionEntry
// =============================================================================

void_core::Result<DefinitionEntry> DefinitionEntry::from_json(const nlohmann::json& j) {
    DefinitionEntry entry;

    // Check if this is the nested format with explicit registry_type
    if (j.contains("registry_type") && j["registry_type"].is_string()) {
        entry.registry_type = j["registry_type"].get<std::string>();

        if (!j.contains("id") || !j["id"].is_string()) {
            return void_core::Err<DefinitionEntry>("DefinitionEntry: missing or invalid 'id' field");
        }
        entry.id = j["id"].get<std::string>();

        if (j.contains("data")) {
            entry.data = j["data"];
        }
    }
    // Otherwise, assume ID is required and data is the whole object minus id
    else {
        if (!j.contains("id") || !j["id"].is_string()) {
            return void_core::Err<DefinitionEntry>("DefinitionEntry: missing or invalid 'id' field");
        }
        entry.id = j["id"].get<std::string>();

        // Copy all fields except 'id' to data
        entry.data = j;
        entry.data.erase("id");
    }

    return void_core::Ok(std::move(entry));
}

nlohmann::json DefinitionEntry::to_json() const {
    nlohmann::json j;
    if (!registry_type.empty()) {
        j["registry_type"] = registry_type;
    }
    j["id"] = id;
    j["data"] = data;
    return j;
}

// =============================================================================
// UI Asset Entries
// =============================================================================

void_core::Result<UILayoutEntry> UILayoutEntry::from_json(const nlohmann::json& j) {
    UILayoutEntry entry;
    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<UILayoutEntry>("UILayoutEntry: missing 'id'");
    }
    entry.id = j["id"].get<std::string>();
    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<UILayoutEntry>("UILayoutEntry: missing 'path'");
    }
    entry.path = j["path"].get<std::string>();
    return void_core::Ok(std::move(entry));
}

nlohmann::json UILayoutEntry::to_json() const {
    return nlohmann::json{{"id", id}, {"path", path}};
}

void_core::Result<UIIconEntry> UIIconEntry::from_json(const nlohmann::json& j) {
    UIIconEntry entry;
    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<UIIconEntry>("UIIconEntry: missing 'id'");
    }
    entry.id = j["id"].get<std::string>();
    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<UIIconEntry>("UIIconEntry: missing 'path'");
    }
    entry.path = j["path"].get<std::string>();
    return void_core::Ok(std::move(entry));
}

nlohmann::json UIIconEntry::to_json() const {
    return nlohmann::json{{"id", id}, {"path", path}};
}

void_core::Result<UIFontEntry> UIFontEntry::from_json(const nlohmann::json& j) {
    UIFontEntry entry;
    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<UIFontEntry>("UIFontEntry: missing 'id'");
    }
    entry.id = j["id"].get<std::string>();
    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<UIFontEntry>("UIFontEntry: missing 'path'");
    }
    entry.path = j["path"].get<std::string>();
    if (j.contains("sizes") && j["sizes"].is_array()) {
        for (const auto& s : j["sizes"]) {
            if (s.is_number_integer()) {
                entry.sizes.push_back(s.get<int>());
            }
        }
    }
    return void_core::Ok(std::move(entry));
}

nlohmann::json UIFontEntry::to_json() const {
    nlohmann::json j{{"id", id}, {"path", path}};
    if (!sizes.empty()) {
        j["sizes"] = sizes;
    }
    return j;
}

void_core::Result<UIThemeEntry> UIThemeEntry::from_json(const nlohmann::json& j) {
    UIThemeEntry entry;
    if (!j.contains("id") || !j["id"].is_string()) {
        return void_core::Err<UIThemeEntry>("UIThemeEntry: missing 'id'");
    }
    entry.id = j["id"].get<std::string>();
    if (!j.contains("path") || !j["path"].is_string()) {
        return void_core::Err<UIThemeEntry>("UIThemeEntry: missing 'path'");
    }
    entry.path = j["path"].get<std::string>();
    return void_core::Ok(std::move(entry));
}

nlohmann::json UIThemeEntry::to_json() const {
    return nlohmann::json{{"id", id}, {"path", path}};
}

void_core::Result<UIAssets> UIAssets::from_json(const nlohmann::json& j) {
    UIAssets assets;

    if (j.contains("layouts") && j["layouts"].is_array()) {
        for (const auto& item : j["layouts"]) {
            auto result = UILayoutEntry::from_json(item);
            if (!result) return void_core::Err<UIAssets>(result.error());
            assets.layouts.push_back(std::move(*result));
        }
    }

    if (j.contains("icons") && j["icons"].is_array()) {
        for (const auto& item : j["icons"]) {
            auto result = UIIconEntry::from_json(item);
            if (!result) return void_core::Err<UIAssets>(result.error());
            assets.icons.push_back(std::move(*result));
        }
    }

    if (j.contains("fonts") && j["fonts"].is_array()) {
        for (const auto& item : j["fonts"]) {
            auto result = UIFontEntry::from_json(item);
            if (!result) return void_core::Err<UIAssets>(result.error());
            assets.fonts.push_back(std::move(*result));
        }
    }

    if (j.contains("themes") && j["themes"].is_array()) {
        for (const auto& item : j["themes"]) {
            auto result = UIThemeEntry::from_json(item);
            if (!result) return void_core::Err<UIAssets>(result.error());
            assets.themes.push_back(std::move(*result));
        }
    }

    return void_core::Ok(std::move(assets));
}

nlohmann::json UIAssets::to_json() const {
    nlohmann::json j;
    if (!layouts.empty()) {
        j["layouts"] = nlohmann::json::array();
        for (const auto& l : layouts) j["layouts"].push_back(l.to_json());
    }
    if (!icons.empty()) {
        j["icons"] = nlohmann::json::array();
        for (const auto& i : icons) j["icons"].push_back(i.to_json());
    }
    if (!fonts.empty()) {
        j["fonts"] = nlohmann::json::array();
        for (const auto& f : fonts) j["fonts"].push_back(f.to_json());
    }
    if (!themes.empty()) {
        j["themes"] = nlohmann::json::array();
        for (const auto& t : themes) j["themes"].push_back(t.to_json());
    }
    return j;
}

// =============================================================================
// DataTableEntry
// =============================================================================

void_core::Result<DataTableEntry> DataTableEntry::from_json(const nlohmann::json& j) {
    DataTableEntry entry;
    if (j.is_string()) {
        // Simple form: just a path string
        entry.path = j.get<std::string>();
        return void_core::Ok(std::move(entry));
    }
    if (j.contains("path") && j["path"].is_string()) {
        entry.path = j["path"].get<std::string>();
    }
    if (j.contains("schema") && j["schema"].is_string()) {
        entry.schema = j["schema"].get<std::string>();
    }
    return void_core::Ok(std::move(entry));
}

nlohmann::json DataTableEntry::to_json() const {
    nlohmann::json j;
    j["path"] = path;
    if (schema) {
        j["schema"] = *schema;
    }
    return j;
}

// =============================================================================
// AssetBundleManifest
// =============================================================================

void_core::Result<AssetBundleManifest> AssetBundleManifest::load(
    const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return void_core::Err<AssetBundleManifest>("Failed to open file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return from_json_string(buffer.str(), path);
}

void_core::Result<AssetBundleManifest> AssetBundleManifest::from_json_string(
    const std::string& json_str,
    const std::filesystem::path& source_path)
{
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        return from_json(j, source_path);
    } catch (const nlohmann::json::exception& e) {
        return void_core::Err<AssetBundleManifest>(std::string("JSON parse error: ") + e.what());
    }
}

void_core::Result<AssetBundleManifest> AssetBundleManifest::from_json(
    const nlohmann::json& j,
    const std::filesystem::path& source_path)
{
    AssetBundleManifest manifest;

    // Parse base manifest
    auto base_result = PackageManifest::from_json_string(j.dump(), source_path);
    if (!base_result) {
        return void_core::Err<AssetBundleManifest>("Failed to parse base manifest: " + base_result.error().message());
    }
    manifest.base = std::move(*base_result);

    // Verify this is an asset bundle
    if (manifest.base.type != PackageType::Asset) {
        return void_core::Err<AssetBundleManifest>("Package type must be 'asset' for asset bundle, got: " +
                               std::string(package_type_to_string(manifest.base.type)));
    }

    // Parse meshes
    if (j.contains("meshes") && j["meshes"].is_array()) {
        for (const auto& item : j["meshes"]) {
            auto result = MeshEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("meshes: " + result.error().message());
            }
            manifest.meshes.push_back(std::move(*result));
        }
    }

    // Parse textures
    if (j.contains("textures") && j["textures"].is_array()) {
        for (const auto& item : j["textures"]) {
            auto result = TextureEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("textures: " + result.error().message());
            }
            manifest.textures.push_back(std::move(*result));
        }
    }

    // Parse materials
    if (j.contains("materials") && j["materials"].is_array()) {
        for (const auto& item : j["materials"]) {
            auto result = MaterialEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("materials: " + result.error().message());
            }
            manifest.materials.push_back(std::move(*result));
        }
    }

    // Parse animations
    if (j.contains("animations") && j["animations"].is_array()) {
        for (const auto& item : j["animations"]) {
            auto result = AnimationEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("animations: " + result.error().message());
            }
            manifest.animations.push_back(std::move(*result));
        }
    }

    // Parse blend spaces
    if (j.contains("blend_spaces") && j["blend_spaces"].is_array()) {
        for (const auto& item : j["blend_spaces"]) {
            auto result = BlendSpaceEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("blend_spaces: " + result.error().message());
            }
            manifest.blend_spaces.push_back(std::move(*result));
        }
    }

    // Parse audio
    if (j.contains("audio") && j["audio"].is_array()) {
        for (const auto& item : j["audio"]) {
            auto result = AudioEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("audio: " + result.error().message());
            }
            manifest.audio.push_back(std::move(*result));
        }
    }

    // Parse VFX
    if (j.contains("vfx") && j["vfx"].is_array()) {
        for (const auto& item : j["vfx"]) {
            auto result = VfxEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("vfx: " + result.error().message());
            }
            manifest.vfx.push_back(std::move(*result));
        }
    }

    // Parse shaders
    if (j.contains("shaders") && j["shaders"].is_array()) {
        for (const auto& item : j["shaders"]) {
            auto result = ShaderEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("shaders: " + result.error().message());
            }
            manifest.shaders.push_back(std::move(*result));
        }
    }

    // Parse prefabs
    if (j.contains("prefabs") && j["prefabs"].is_array()) {
        for (const auto& item : j["prefabs"]) {
            auto result = PrefabEntry::from_json(item);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("prefabs: " + result.error().message());
            }
            manifest.prefabs.push_back(std::move(*result));
        }
    }

    // Parse definitions - can be nested by type or flat with explicit registry_type
    if (j.contains("definitions")) {
        const auto& defs = j["definitions"];
        if (defs.is_object()) {
            for (auto& [registry_type, entries] : defs.items()) {
                if (entries.is_array()) {
                    for (const auto& item : entries) {
                        auto result = DefinitionEntry::from_json(item);
                        if (!result) {
                            return void_core::Err<AssetBundleManifest>("definitions." + registry_type + ": " +
                                                   result.error().message());
                        }
                        result->registry_type = registry_type;
                        manifest.definitions[registry_type].push_back(std::move(*result));
                    }
                }
            }
        }
    }

    // Parse UI assets
    if (j.contains("ui_assets") && j["ui_assets"].is_object()) {
        auto result = UIAssets::from_json(j["ui_assets"]);
        if (!result) {
            return void_core::Err<AssetBundleManifest>("ui_assets: " + result.error().message());
        }
        manifest.ui_assets = std::move(*result);
    }

    // Parse data tables
    if (j.contains("data_tables") && j["data_tables"].is_object()) {
        for (auto& [key, value] : j["data_tables"].items()) {
            auto result = DataTableEntry::from_json(value);
            if (!result) {
                return void_core::Err<AssetBundleManifest>("data_tables." + key + ": " + result.error().message());
            }
            result->id = key;
            manifest.data_tables[key] = std::move(*result);
        }
    }

    return void_core::Ok(std::move(manifest));
}

void_core::Result<void> AssetBundleManifest::validate() const {
    auto base_valid = base.validate();
    if (!base_valid) {
        return base_valid;
    }

    auto ids_valid = validate_unique_ids();
    if (!ids_valid) {
        return ids_valid;
    }

    return void_core::Ok();
}

void_core::Result<void> AssetBundleManifest::validate_unique_ids() const {
    std::unordered_set<std::string> seen_ids;

    auto check_id = [&seen_ids](const std::string& id, const char* type) -> void_core::Result<void> {
        if (id.empty()) {
            return void_core::Err(std::string(type) + " has empty ID");
        }
        if (seen_ids.count(id) > 0) {
            return void_core::Err("Duplicate ID '" + id + "' in " + type);
        }
        seen_ids.insert(id);
        return void_core::Ok();
    };

    for (const auto& m : meshes) {
        auto r = check_id(m.id, "meshes");
        if (!r) return r;
    }
    for (const auto& t : textures) {
        auto r = check_id(t.id, "textures");
        if (!r) return r;
    }
    for (const auto& m : materials) {
        auto r = check_id(m.id, "materials");
        if (!r) return r;
    }
    for (const auto& a : animations) {
        auto r = check_id(a.id, "animations");
        if (!r) return r;
    }
    for (const auto& b : blend_spaces) {
        auto r = check_id(b.id, "blend_spaces");
        if (!r) return r;
    }
    for (const auto& a : audio) {
        auto r = check_id(a.id, "audio");
        if (!r) return r;
    }
    for (const auto& v : vfx) {
        auto r = check_id(v.id, "vfx");
        if (!r) return r;
    }
    for (const auto& s : shaders) {
        auto r = check_id(s.id, "shaders");
        if (!r) return r;
    }
    for (const auto& p : prefabs) {
        auto r = check_id(p.id, "prefabs");
        if (!r) return r;
    }

    return void_core::Ok();
}

const MeshEntry* AssetBundleManifest::find_mesh(const std::string& id) const {
    for (const auto& m : meshes) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const TextureEntry* AssetBundleManifest::find_texture(const std::string& id) const {
    for (const auto& t : textures) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const MaterialEntry* AssetBundleManifest::find_material(const std::string& id) const {
    for (const auto& m : materials) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const AnimationEntry* AssetBundleManifest::find_animation(const std::string& id) const {
    for (const auto& a : animations) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

const AudioEntry* AssetBundleManifest::find_audio(const std::string& id) const {
    for (const auto& a : audio) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

const PrefabEntry* AssetBundleManifest::find_prefab(const std::string& id) const {
    for (const auto& p : prefabs) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const DefinitionEntry* AssetBundleManifest::find_definition(
    const std::string& registry_type,
    const std::string& id) const
{
    auto it = definitions.find(registry_type);
    if (it == definitions.end()) return nullptr;

    for (const auto& def : it->second) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

std::vector<std::string> AssetBundleManifest::definition_registry_types() const {
    std::vector<std::string> types;
    types.reserve(definitions.size());
    for (const auto& [type, _] : definitions) {
        types.push_back(type);
    }
    return types;
}

std::size_t AssetBundleManifest::total_asset_count() const {
    std::size_t count = 0;
    count += meshes.size();
    count += textures.size();
    count += materials.size();
    count += animations.size();
    count += blend_spaces.size();
    count += audio.size();
    count += vfx.size();
    count += shaders.size();
    count += prefabs.size();
    for (const auto& [_, defs] : definitions) {
        count += defs.size();
    }
    return count;
}

// =============================================================================
// Utilities
// =============================================================================

bool is_asset_bundle_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (ext == ".json") {
        // Check for .bundle.json
        std::string stem = path.stem().string();
        return stem.size() > 7 && stem.substr(stem.size() - 7) == ".bundle";
    }
    return false;
}

} // namespace void_package
