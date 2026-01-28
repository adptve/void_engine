/// @file manifest_parser.cpp
/// @brief Package manifest (manifest.json) parsing implementation

#include <void_engine/scene/manifest_parser.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace void_scene {

using json = nlohmann::json;

// =============================================================================
// Helper Functions (internal)
// =============================================================================

namespace {

void parse_package(const json& obj, PackageInfo& pkg) {
    if (obj.contains("name") && obj["name"].is_string()) {
        pkg.name = obj["name"].get<std::string>();
    }
    if (obj.contains("display_name") && obj["display_name"].is_string()) {
        pkg.display_name = obj["display_name"].get<std::string>();
    }
    if (obj.contains("version") && obj["version"].is_string()) {
        pkg.version = obj["version"].get<std::string>();
    }
    if (obj.contains("description") && obj["description"].is_string()) {
        pkg.description = obj["description"].get<std::string>();
    }
    if (obj.contains("author") && obj["author"].is_string()) {
        pkg.author = obj["author"].get<std::string>();
    }
    if (obj.contains("license") && obj["license"].is_string()) {
        pkg.license = obj["license"].get<std::string>();
    }

    if (obj.contains("keywords") && obj["keywords"].is_array()) {
        for (const auto& kw : obj["keywords"]) {
            if (kw.is_string()) {
                pkg.keywords.push_back(kw.get<std::string>());
            }
        }
    }

    if (obj.contains("categories") && obj["categories"].is_array()) {
        for (const auto& cat : obj["categories"]) {
            if (cat.is_string()) {
                pkg.categories.push_back(cat.get<std::string>());
            }
        }
    }
}

void parse_layers(const json& arr, std::vector<LayerConfig>& layers) {
    for (const auto& layer_obj : arr) {
        if (!layer_obj.is_object()) continue;

        LayerConfig layer;

        if (layer_obj.contains("name") && layer_obj["name"].is_string()) {
            layer.name = layer_obj["name"].get<std::string>();
        }
        if (layer_obj.contains("type") && layer_obj["type"].is_string()) {
            layer.type = layer_obj["type"].get<std::string>();
        }
        layer.priority = layer_obj.value("priority", 0);
        if (layer_obj.contains("blend") && layer_obj["blend"].is_string()) {
            layer.blend = layer_obj["blend"].get<std::string>();
        }
        layer.enabled = layer_obj.value("enabled", true);

        layers.push_back(layer);
    }
}

void parse_permissions(const json& obj, Permissions& perms) {
    perms.scripts = obj.value("scripts", true);
    perms.network = obj.value("network", false);
    perms.file_system = obj.value("file_system", false);
    perms.audio = obj.value("audio", true);
    perms.input = obj.value("input", true);
    perms.clipboard = obj.value("clipboard", false);
}

void parse_resources(const json& obj, ResourceLimits& limits) {
    limits.max_entities = obj.value("max_entities", 10000u);
    limits.max_memory = obj.value("max_memory", 536870912ull);
    limits.max_layers = obj.value("max_layers", 16u);
    limits.max_textures = obj.value("max_textures", 1000u);
    limits.max_meshes = obj.value("max_meshes", 1000u);
}

void parse_lod(const json& obj, LodConfig& lod) {
    lod.enabled = obj.value("enabled", true);
    lod.bias = obj.value("bias", 0.0f);

    if (obj.contains("distances") && obj["distances"].is_array()) {
        lod.distances.clear();
        for (const auto& d : obj["distances"]) {
            if (d.is_number()) {
                lod.distances.push_back(d.get<float>());
            }
        }
    }
}

void parse_streaming(const json& obj, StreamingConfig& streaming) {
    streaming.enabled = obj.value("enabled", false);
    streaming.load_distance = obj.value("load_distance", 100.0f);
    streaming.unload_distance = obj.value("unload_distance", 150.0f);
    streaming.max_concurrent_loads = obj.value("max_concurrent_loads", 4u);
}

void parse_app(const json& obj, AppConfig& app) {
    if (obj.contains("app_type") && obj["app_type"].is_string()) {
        app.app_type = obj["app_type"].get<std::string>();
    }
    if (obj.contains("scene") && obj["scene"].is_string()) {
        app.scene = obj["scene"].get<std::string>();
    }

    // Parse "layers" array
    if (obj.contains("layers") && obj["layers"].is_array()) {
        parse_layers(obj["layers"], app.layers);
    }

    // Parse "permissions" object
    if (obj.contains("permissions") && obj["permissions"].is_object()) {
        parse_permissions(obj["permissions"], app.permissions);
    }

    // Parse "resources" object
    if (obj.contains("resources") && obj["resources"].is_object()) {
        parse_resources(obj["resources"], app.resources);
    }

    // Parse "lod" object
    if (obj.contains("lod") && obj["lod"].is_object()) {
        parse_lod(obj["lod"], app.lod);
    }

    // Parse "streaming" object
    if (obj.contains("streaming") && obj["streaming"].is_object()) {
        parse_streaming(obj["streaming"], app.streaming);
    }
}

void parse_assets(const json& obj, AssetConfig& assets) {
    if (obj.contains("include") && obj["include"].is_array()) {
        for (const auto& inc : obj["include"]) {
            if (inc.is_string()) {
                assets.include.push_back(inc.get<std::string>());
            }
        }
    }

    if (obj.contains("exclude") && obj["exclude"].is_array()) {
        for (const auto& exc : obj["exclude"]) {
            if (exc.is_string()) {
                assets.exclude.push_back(exc.get<std::string>());
            }
        }
    }

    if (obj.contains("base_path") && obj["base_path"].is_string()) {
        assets.base_path = obj["base_path"].get<std::string>();
    }

    assets.hot_reload = obj.value("hot_reload", true);
}

void parse_platform(const json& obj, PlatformRequirements& platform) {
    if (obj.contains("min_version") && obj["min_version"].is_string()) {
        platform.min_version = obj["min_version"].get<std::string>();
    }

    if (obj.contains("required_features") && obj["required_features"].is_array()) {
        for (const auto& feat : obj["required_features"]) {
            if (feat.is_string()) {
                platform.required_features.push_back(feat.get<std::string>());
            }
        }
    }

    if (obj.contains("optional_features") && obj["optional_features"].is_array()) {
        for (const auto& feat : obj["optional_features"]) {
            if (feat.is_string()) {
                platform.optional_features.push_back(feat.get<std::string>());
            }
        }
    }
}

} // anonymous namespace

// =============================================================================
// ManifestParser Implementation
// =============================================================================

void_core::Result<ManifestData> ManifestParser::parse(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_last_error = "Failed to open manifest file: " + path.string();
        return void_core::Err<ManifestData>(void_core::Error(m_last_error));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parse_string(buffer.str(), path.string());
}

void_core::Result<ManifestData> ManifestParser::parse_string(
    const std::string& content,
    const std::string& source_name)
{
    ManifestData manifest;

    try {
        json root = json::parse(content);

        // Parse "package" section
        if (root.contains("package") && root["package"].is_object()) {
            parse_package(root["package"], manifest.package);
        }

        // Parse "app" section
        if (root.contains("app") && root["app"].is_object()) {
            parse_app(root["app"], manifest.app);
        }

        // Parse "assets" section
        if (root.contains("assets") && root["assets"].is_object()) {
            parse_assets(root["assets"], manifest.assets);
        }

        // Parse "platform" section
        if (root.contains("platform") && root["platform"].is_object()) {
            parse_platform(root["platform"], manifest.platform);
        }

    } catch (const json::parse_error& err) {
        m_last_error = "JSON parse error: " + std::string(err.what());
        return void_core::Err<ManifestData>(void_core::Error(m_last_error));
    } catch (const json::type_error& err) {
        m_last_error = "JSON type error: " + std::string(err.what());
        return void_core::Err<ManifestData>(void_core::Error(m_last_error));
    }

    m_last_error.clear();
    return manifest;
}

// =============================================================================
// ManifestManager Implementation
// =============================================================================

void_core::Result<void> ManifestManager::initialize(const std::filesystem::path& manifest_path) {
    m_manifest_path = manifest_path;

    // Parse manifest
    auto result = m_parser.parse(manifest_path);
    if (!result) {
        return void_core::Err(result.error());
    }

    m_manifest = std::move(*result);

    // Get initial file modification time
    std::error_code ec;
    m_last_modified = std::filesystem::last_write_time(manifest_path, ec);

    return void_core::Ok();
}

void ManifestManager::shutdown() {
    m_manifest.reset();
    m_manifest_path.clear();
}

const ManifestData* ManifestManager::manifest() const {
    return m_manifest ? &*m_manifest : nullptr;
}

std::filesystem::path ManifestManager::scene_path() const {
    if (!m_manifest) return {};

    auto parent = m_manifest_path.parent_path();
    return parent / m_manifest->app.scene;
}

std::filesystem::path ManifestManager::asset_base_path() const {
    if (!m_manifest) return {};

    auto parent = m_manifest_path.parent_path();
    if (m_manifest->assets.base_path.empty()) {
        return parent;
    }
    return parent / m_manifest->assets.base_path;
}

void ManifestManager::update() {
    if (!m_hot_reload_enabled || m_manifest_path.empty()) return;

    std::error_code ec;
    auto current_mtime = std::filesystem::last_write_time(m_manifest_path, ec);
    if (ec) return;

    if (current_mtime != m_last_modified) {
        m_last_modified = current_mtime;
        auto result = reload();
        // Silently ignore reload errors (file might be mid-write)
        (void)result;
    }
}

void_core::Result<void> ManifestManager::reload() {
    auto result = m_parser.parse(m_manifest_path);
    if (!result) {
        return void_core::Err(result.error());
    }

    m_manifest = std::move(*result);

    // Notify callback
    if (m_on_changed && m_manifest) {
        m_on_changed(*m_manifest);
    }

    return void_core::Ok();
}

void ManifestManager::on_manifest_changed(std::function<void(const ManifestData&)> callback) {
    m_on_changed = std::move(callback);
}

} // namespace void_scene
