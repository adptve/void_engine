/// @file manifest_parser.cpp
/// @brief Package manifest (manifest.toml) parsing implementation

#include <void_engine/scene/manifest_parser.hpp>

#include <toml++/toml.hpp>

#include <fstream>
#include <sstream>

namespace void_scene {

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
        toml::table tbl = toml::parse(content, source_name);

        // Parse [package] section
        if (auto pkg = tbl["package"].as_table()) {
            parse_package(pkg, manifest.package);
        }

        // Parse [app] section
        if (auto app = tbl["app"].as_table()) {
            parse_app(app, manifest.app);
        }

        // Parse [assets] section
        if (auto assets = tbl["assets"].as_table()) {
            parse_assets(assets, manifest.assets);
        }

        // Parse [platform] section
        if (auto platform = tbl["platform"].as_table()) {
            parse_platform(platform, manifest.platform);
        }

    } catch (const toml::parse_error& err) {
        m_last_error = "TOML parse error: " + std::string(err.what());
        return void_core::Err<ManifestData>(void_core::Error(m_last_error));
    }

    m_last_error.clear();
    return manifest;
}

void ManifestParser::parse_package(const void* tbl_ptr, PackageInfo& pkg) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    if (auto name = tbl["name"].value<std::string>()) {
        pkg.name = *name;
    }
    if (auto display = tbl["display_name"].value<std::string>()) {
        pkg.display_name = *display;
    }
    if (auto ver = tbl["version"].value<std::string>()) {
        pkg.version = *ver;
    }
    if (auto desc = tbl["description"].value<std::string>()) {
        pkg.description = *desc;
    }
    if (auto author = tbl["author"].value<std::string>()) {
        pkg.author = *author;
    }
    if (auto license = tbl["license"].value<std::string>()) {
        pkg.license = *license;
    }

    if (auto keywords = tbl["keywords"].as_array()) {
        for (const auto& kw : *keywords) {
            if (auto str = kw.value<std::string>()) {
                pkg.keywords.push_back(*str);
            }
        }
    }

    if (auto categories = tbl["categories"].as_array()) {
        for (const auto& cat : *categories) {
            if (auto str = cat.value<std::string>()) {
                pkg.categories.push_back(*str);
            }
        }
    }
}

void ManifestParser::parse_app(const void* tbl_ptr, AppConfig& app) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    if (auto app_type = tbl["app_type"].value<std::string>()) {
        app.app_type = *app_type;
    }
    if (auto scene = tbl["scene"].value<std::string>()) {
        app.scene = *scene;
    }

    // Parse [[app.layers]]
    if (auto layers = tbl["layers"].as_array()) {
        parse_layers(layers, app.layers);
    }

    // Parse [app.permissions]
    if (auto perms = tbl["permissions"].as_table()) {
        parse_permissions(perms, app.permissions);
    }

    // Parse [app.resources]
    if (auto resources = tbl["resources"].as_table()) {
        parse_resources(resources, app.resources);
    }

    // Parse [app.lod]
    if (auto lod = tbl["lod"].as_table()) {
        parse_lod(lod, app.lod);
    }

    // Parse [app.streaming]
    if (auto streaming = tbl["streaming"].as_table()) {
        parse_streaming(streaming, app.streaming);
    }
}

void ManifestParser::parse_layers(const void* arr_ptr, std::vector<LayerConfig>& layers) {
    const auto& arr = *static_cast<const toml::array*>(arr_ptr);

    for (const auto& layer_node : arr) {
        if (auto layer_tbl = layer_node.as_table()) {
            LayerConfig layer;

            if (auto name = (*layer_tbl)["name"].value<std::string>()) {
                layer.name = *name;
            }
            if (auto type = (*layer_tbl)["type"].value<std::string>()) {
                layer.type = *type;
            }
            layer.priority = (*layer_tbl)["priority"].value_or(0);
            if (auto blend = (*layer_tbl)["blend"].value<std::string>()) {
                layer.blend = *blend;
            }
            layer.enabled = (*layer_tbl)["enabled"].value_or(true);

            layers.push_back(layer);
        }
    }
}

void ManifestParser::parse_permissions(const void* tbl_ptr, Permissions& perms) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    perms.scripts = tbl["scripts"].value_or(true);
    perms.network = tbl["network"].value_or(false);
    perms.file_system = tbl["file_system"].value_or(false);
    perms.audio = tbl["audio"].value_or(true);
    perms.input = tbl["input"].value_or(true);
    perms.clipboard = tbl["clipboard"].value_or(false);
}

void ManifestParser::parse_resources(const void* tbl_ptr, ResourceLimits& limits) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    limits.max_entities = static_cast<std::uint32_t>(tbl["max_entities"].value_or(10000));
    limits.max_memory = static_cast<std::uint64_t>(tbl["max_memory"].value_or(536870912));
    limits.max_layers = static_cast<std::uint32_t>(tbl["max_layers"].value_or(16));
    limits.max_textures = static_cast<std::uint32_t>(tbl["max_textures"].value_or(1000));
    limits.max_meshes = static_cast<std::uint32_t>(tbl["max_meshes"].value_or(1000));
}

void ManifestParser::parse_lod(const void* tbl_ptr, LodConfig& lod) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    lod.enabled = tbl["enabled"].value_or(true);
    lod.bias = tbl["bias"].value_or(0.0f);

    if (auto distances = tbl["distances"].as_array()) {
        lod.distances.clear();
        for (const auto& d : *distances) {
            if (auto val = d.value<double>()) {
                lod.distances.push_back(static_cast<float>(*val));
            }
        }
    }
}

void ManifestParser::parse_streaming(const void* tbl_ptr, StreamingConfig& streaming) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    streaming.enabled = tbl["enabled"].value_or(false);
    streaming.load_distance = tbl["load_distance"].value_or(100.0f);
    streaming.unload_distance = tbl["unload_distance"].value_or(150.0f);
    streaming.max_concurrent_loads = static_cast<std::uint32_t>(tbl["max_concurrent_loads"].value_or(4));
}

void ManifestParser::parse_assets(const void* tbl_ptr, AssetConfig& assets) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    if (auto include = tbl["include"].as_array()) {
        for (const auto& inc : *include) {
            if (auto str = inc.value<std::string>()) {
                assets.include.push_back(*str);
            }
        }
    }

    if (auto exclude = tbl["exclude"].as_array()) {
        for (const auto& exc : *exclude) {
            if (auto str = exc.value<std::string>()) {
                assets.exclude.push_back(*str);
            }
        }
    }

    if (auto base = tbl["base_path"].value<std::string>()) {
        assets.base_path = *base;
    }

    assets.hot_reload = tbl["hot_reload"].value_or(true);
}

void ManifestParser::parse_platform(const void* tbl_ptr, PlatformRequirements& platform) {
    const auto& tbl = *static_cast<const toml::table*>(tbl_ptr);

    if (auto ver = tbl["min_version"].value<std::string>()) {
        platform.min_version = *ver;
    }

    if (auto required = tbl["required_features"].as_array()) {
        for (const auto& feat : *required) {
            if (auto str = feat.value<std::string>()) {
                platform.required_features.push_back(*str);
            }
        }
    }

    if (auto optional = tbl["optional_features"].as_array()) {
        for (const auto& feat : *optional) {
            if (auto str = feat.value<std::string>()) {
                platform.optional_features.push_back(*str);
            }
        }
    }
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
