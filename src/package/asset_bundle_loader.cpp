/// @file asset_bundle_loader.cpp
/// @brief Implementation of AssetBundleLoader for asset.bundle packages

#include <void_engine/package/asset_bundle_loader.hpp>
#include <void_engine/package/prefab_registry.hpp>
#include <void_engine/package/definition_registry.hpp>
#include <void_engine/package/component_schema.hpp>

#include <sstream>
#include <fstream>

namespace void_package {

// =============================================================================
// Construction
// =============================================================================

AssetBundleLoader::AssetBundleLoader(
    PrefabRegistry* prefab_registry,
    DefinitionRegistry* definition_registry,
    ComponentSchemaRegistry* schema_registry)
    : m_prefab_registry(prefab_registry)
    , m_definition_registry(definition_registry)
    , m_schema_registry(schema_registry)
{
}

// =============================================================================
// PackageLoader Interface
// =============================================================================

void_core::Result<void> AssetBundleLoader::load(
    const ResolvedPackage& package,
    LoadContext& ctx)
{
    auto result = load_with_result(package, ctx);
    if (!result) {
        return void_core::Err(result.error());
    }
    return void_core::Ok();
}

void_core::Result<void> AssetBundleLoader::unload(
    const std::string& package_name,
    LoadContext& ctx)
{
    auto it = m_loaded_bundles.find(package_name);
    if (it == m_loaded_bundles.end()) {
        return void_core::Err("Bundle not loaded: " + package_name);
    }

    // Unload in reverse order of loading

    // 1. Unload engine assets (meshes, textures, etc.)
    unload_bundle_assets(it->second, ctx);

    // 2. Unregister definitions
    if (m_definition_registry) {
        m_definition_registry->unregister_bundle(package_name);
    }

    // 3. Unregister prefabs
    if (m_prefab_registry) {
        m_prefab_registry->unregister_bundle(package_name);
    }

    // Remove from loaded bundles
    m_loaded_bundles.erase(it);

    return void_core::Ok();
}

bool AssetBundleLoader::is_loaded(const std::string& package_name) const {
    return m_loaded_bundles.count(package_name) > 0;
}

std::vector<std::string> AssetBundleLoader::loaded_packages() const {
    std::vector<std::string> names;
    names.reserve(m_loaded_bundles.size());
    for (const auto& [name, _] : m_loaded_bundles) {
        names.push_back(name);
    }
    return names;
}

// =============================================================================
// Extended API
// =============================================================================

void_core::Result<AssetBundleLoadResult> AssetBundleLoader::load_with_result(
    const ResolvedPackage& package,
    LoadContext& ctx)
{
    const std::string& bundle_name = package.manifest.name;

    // Check if already loaded
    if (is_loaded(bundle_name)) {
        return void_core::Err<AssetBundleLoadResult>("Bundle already loaded: " + bundle_name);
    }

    // Validate package type
    if (package.manifest.type != PackageType::Asset) {
        return void_core::Err<AssetBundleLoadResult>("Package '" + bundle_name + "' is not an asset bundle (type: " +
                               std::string(package_type_to_string(package.manifest.type)) + ")");
    }

    // Find manifest file
    std::filesystem::path manifest_path = package.path / "manifest.json";
    if (!std::filesystem::exists(manifest_path)) {
        // Try alternate names
        manifest_path = package.path / "bundle.json";
        if (!std::filesystem::exists(manifest_path)) {
            return void_core::Err<AssetBundleLoadResult>("No manifest found in bundle: " + bundle_name +
                                   " (tried manifest.json, bundle.json)");
        }
    }

    // Load the asset bundle manifest
    auto manifest_result = AssetBundleManifest::load(manifest_path);
    if (!manifest_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load manifest for bundle '" + bundle_name +
                               "': " + manifest_result.error().message());
    }

    AssetBundleManifest& manifest = *manifest_result;

    // Strict validation if enabled
    if (m_strict_validation) {
        auto validate_result = manifest.validate();
        if (!validate_result) {
            return void_core::Err<AssetBundleLoadResult>("Manifest validation failed for bundle '" + bundle_name +
                                   "': " + validate_result.error().message());
        }

        auto unique_result = manifest.validate_unique_ids();
        if (!unique_result) {
            return void_core::Err<AssetBundleLoadResult>("Duplicate IDs in bundle '" + bundle_name +
                                   "': " + unique_result.error().message());
        }
    }

    // Initialize result
    AssetBundleLoadResult result;
    result.bundle_name = bundle_name;

    // Load content in dependency order:
    // 1. Shaders (needed by materials)
    // 2. Textures (needed by materials)
    // 3. Materials (needed by meshes/prefabs)
    // 4. Meshes
    // 5. Animations
    // 6. Audio
    // 7. Definitions (may reference assets)
    // 8. Prefabs (may reference all of the above)

    // Load shaders
    auto shaders_result = load_shaders(manifest, package.path, ctx, result);
    if (!shaders_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load shaders: " + shaders_result.error().message());
    }
    result.shaders_loaded = *shaders_result;

    // Load textures
    auto textures_result = load_textures(manifest, package.path, ctx, result);
    if (!textures_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load textures: " + textures_result.error().message());
    }
    result.textures_loaded = *textures_result;

    // Load materials
    auto materials_result = load_materials(manifest, package.path, ctx, result);
    if (!materials_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load materials: " + materials_result.error().message());
    }
    result.materials_loaded = *materials_result;

    // Load meshes
    auto meshes_result = load_meshes(manifest, package.path, ctx, result);
    if (!meshes_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load meshes: " + meshes_result.error().message());
    }
    result.meshes_loaded = *meshes_result;

    // Load animations
    auto animations_result = load_animations(manifest, package.path, ctx, result);
    if (!animations_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load animations: " + animations_result.error().message());
    }
    result.animations_loaded = *animations_result;

    // Load audio
    auto audio_result = load_audio(manifest, package.path, ctx, result);
    if (!audio_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load audio: " + audio_result.error().message());
    }
    result.audio_loaded = *audio_result;

    // Load definitions
    auto defs_result = load_definitions(manifest, bundle_name, result);
    if (!defs_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load definitions: " + defs_result.error().message());
    }
    result.definitions_loaded = *defs_result;

    // Load prefabs (last, as they may reference everything else)
    auto prefabs_result = load_prefabs(manifest, bundle_name, result);
    if (!prefabs_result) {
        return void_core::Err<AssetBundleLoadResult>("Failed to load prefabs: " + prefabs_result.error().message());
    }
    result.prefabs_loaded = *prefabs_result;

    // Store loaded bundle info
    LoadedBundle loaded;
    loaded.manifest = std::move(manifest);
    loaded.root_path = package.path;
    loaded.result = result;
    m_loaded_bundles[bundle_name] = std::move(loaded);

    return void_core::Ok(std::move(result));
}

const AssetBundleLoadResult* AssetBundleLoader::get_load_result(
    const std::string& package_name) const
{
    auto it = m_loaded_bundles.find(package_name);
    if (it == m_loaded_bundles.end()) {
        return nullptr;
    }
    return &it->second.result;
}

const AssetBundleManifest* AssetBundleLoader::get_manifest(
    const std::string& package_name) const
{
    auto it = m_loaded_bundles.find(package_name);
    if (it == m_loaded_bundles.end()) {
        return nullptr;
    }
    return &it->second.manifest;
}

// =============================================================================
// Internal Methods - Prefabs
// =============================================================================

void_core::Result<std::size_t> AssetBundleLoader::load_prefabs(
    const AssetBundleManifest& manifest,
    const std::string& bundle_name,
    AssetBundleLoadResult& result)
{
    if (!m_prefab_registry) {
        // No registry configured - skip prefabs but warn
        if (!manifest.prefabs.empty()) {
            result.warnings.push_back("No PrefabRegistry configured, skipping " +
                                       std::to_string(manifest.prefabs.size()) + " prefabs");
        }
        return void_core::Ok(std::size_t{0});
    }

    std::size_t loaded = 0;

    for (const auto& entry : manifest.prefabs) {
        PrefabDefinition def = entry_to_definition(entry, bundle_name);

        auto reg_result = m_prefab_registry->register_prefab(std::move(def));
        if (!reg_result) {
            // Check if we should fail or warn
            result.warnings.push_back("Failed to register prefab '" + entry.id +
                                       "': " + reg_result.error().message());
            continue;
        }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

PrefabDefinition AssetBundleLoader::entry_to_definition(
    const PrefabEntry& entry,
    const std::string& bundle_name) const
{
    PrefabDefinition def;
    def.id = entry.id;
    def.source_bundle = bundle_name;
    def.components = entry.components;  // Direct copy - components stored by name
    def.tags = entry.tags;
    return def;
}

// =============================================================================
// Internal Methods - Definitions
// =============================================================================

void_core::Result<std::size_t> AssetBundleLoader::load_definitions(
    const AssetBundleManifest& manifest,
    const std::string& bundle_name,
    AssetBundleLoadResult& result)
{
    if (!m_definition_registry) {
        // No registry configured - skip definitions but warn
        std::size_t total_defs = 0;
        for (const auto& [_, defs] : manifest.definitions) {
            total_defs += defs.size();
        }
        if (total_defs > 0) {
            result.warnings.push_back("No DefinitionRegistry configured, skipping " +
                                       std::to_string(total_defs) + " definitions");
        }
        return void_core::Ok(std::size_t{0});
    }

    std::size_t loaded = 0;

    for (const auto& [registry_type, definitions] : manifest.definitions) {
        for (const auto& def_entry : definitions) {
            DefinitionSource source;
            source.bundle_name = bundle_name;

            auto reg_result = m_definition_registry->register_definition(
                def_entry.registry_type.empty() ? registry_type : def_entry.registry_type,
                def_entry.id,
                def_entry.data,
                std::move(source));

            if (!reg_result) {
                result.warnings.push_back("Failed to register definition '" + def_entry.id +
                                           "' in registry '" + registry_type +
                                           "': " + reg_result.error().message());
                continue;
            }

            ++loaded;
        }
    }

    return void_core::Ok(loaded);
}

// =============================================================================
// Internal Methods - Asset Loading
// =============================================================================

void_core::Result<std::size_t> AssetBundleLoader::load_meshes(
    const AssetBundleManifest& manifest,
    const std::filesystem::path& root_path,
    [[maybe_unused]] LoadContext& ctx,
    AssetBundleLoadResult& result)
{
    // TODO: Integrate with AssetServer once available
    // For now, just validate paths exist if strict validation is enabled

    std::size_t loaded = 0;

    for (const auto& mesh : manifest.meshes) {
        std::filesystem::path full_path = root_path / mesh.path;

        if (m_strict_validation && !std::filesystem::exists(full_path)) {
            auto handle_result = handle_missing_asset(mesh.id, mesh.path, result);
            if (!handle_result) {
                return void_core::Err<std::size_t>(handle_result.error());
            }
            continue;
        }

        // When AssetServer is available:
        // auto* asset_server = ctx.get_service<AssetServer>();
        // if (asset_server) {
        //     asset_server->load_mesh(mesh.id, full_path, mesh.lod_paths, mesh.collision_path);
        // }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

void_core::Result<std::size_t> AssetBundleLoader::load_textures(
    const AssetBundleManifest& manifest,
    const std::filesystem::path& root_path,
    [[maybe_unused]] LoadContext& ctx,
    AssetBundleLoadResult& result)
{
    std::size_t loaded = 0;

    for (const auto& texture : manifest.textures) {
        std::filesystem::path full_path = root_path / texture.path;

        if (m_strict_validation && !std::filesystem::exists(full_path)) {
            auto handle_result = handle_missing_asset(texture.id, texture.path, result);
            if (!handle_result) {
                return void_core::Err<std::size_t>(handle_result.error());
            }
            continue;
        }

        // When AssetServer is available:
        // auto* asset_server = ctx.get_service<AssetServer>();
        // if (asset_server) {
        //     TextureLoadOptions opts;
        //     opts.format = texture.format;
        //     opts.mipmaps = texture.mipmaps;
        //     opts.srgb = texture.srgb;
        //     asset_server->load_texture(texture.id, full_path, opts);
        // }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

void_core::Result<std::size_t> AssetBundleLoader::load_materials(
    const AssetBundleManifest& manifest,
    [[maybe_unused]] const std::filesystem::path& root_path,
    [[maybe_unused]] LoadContext& ctx,
    [[maybe_unused]] AssetBundleLoadResult& result)
{
    std::size_t loaded = 0;

    for ([[maybe_unused]] const auto& material : manifest.materials) {
        // Materials are definitions, not files - always loadable
        // When MaterialSystem is available:
        // auto* material_system = ctx.get_service<MaterialSystem>();
        // if (material_system) {
        //     material_system->register_material(material.id, material.shader,
        //                                        material.textures, material.parameters);
        // }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

void_core::Result<std::size_t> AssetBundleLoader::load_animations(
    const AssetBundleManifest& manifest,
    const std::filesystem::path& root_path,
    [[maybe_unused]] LoadContext& ctx,
    AssetBundleLoadResult& result)
{
    std::size_t loaded = 0;

    for (const auto& anim : manifest.animations) {
        std::filesystem::path full_path = root_path / anim.path;

        if (m_strict_validation && !std::filesystem::exists(full_path)) {
            auto handle_result = handle_missing_asset(anim.id, anim.path, result);
            if (!handle_result) {
                return void_core::Err<std::size_t>(handle_result.error());
            }
            continue;
        }

        // When AnimationSystem is available:
        // auto* anim_system = ctx.get_service<AnimationSystem>();
        // if (anim_system) {
        //     AnimationLoadOptions opts;
        //     opts.loop = anim.loop;
        //     opts.root_motion = anim.root_motion;
        //     opts.events = anim.events;
        //     anim_system->load_animation(anim.id, full_path, opts);
        // }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

void_core::Result<std::size_t> AssetBundleLoader::load_audio(
    const AssetBundleManifest& manifest,
    const std::filesystem::path& root_path,
    [[maybe_unused]] LoadContext& ctx,
    AssetBundleLoadResult& result)
{
    std::size_t loaded = 0;

    for (const auto& audio : manifest.audio) {
        std::filesystem::path full_path = root_path / audio.path;

        if (m_strict_validation && !std::filesystem::exists(full_path)) {
            auto handle_result = handle_missing_asset(audio.id, audio.path, result);
            if (!handle_result) {
                return void_core::Err<std::size_t>(handle_result.error());
            }
            continue;
        }

        // When AudioSystem is available:
        // auto* audio_system = ctx.get_service<AudioSystem>();
        // if (audio_system) {
        //     AudioLoadOptions opts;
        //     opts.type = audio.type;
        //     opts.volume = audio.volume;
        //     opts.loop = audio.loop;
        //     opts.variations = audio.variations;
        //     audio_system->load_audio(audio.id, full_path, opts);
        // }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

void_core::Result<std::size_t> AssetBundleLoader::load_shaders(
    const AssetBundleManifest& manifest,
    const std::filesystem::path& root_path,
    [[maybe_unused]] LoadContext& ctx,
    AssetBundleLoadResult& result)
{
    std::size_t loaded = 0;

    for (const auto& shader : manifest.shaders) {
        // Validate shader files exist
        if (m_strict_validation) {
            if (shader.vertex) {
                std::filesystem::path vs_path = root_path / *shader.vertex;
                if (!std::filesystem::exists(vs_path)) {
                    auto handle_result = handle_missing_asset(shader.id + ".vs", *shader.vertex, result);
                    if (!handle_result) {
                        return void_core::Err<std::size_t>(handle_result.error());
                    }
                    continue;
                }
            }
            if (shader.fragment) {
                std::filesystem::path fs_path = root_path / *shader.fragment;
                if (!std::filesystem::exists(fs_path)) {
                    auto handle_result = handle_missing_asset(shader.id + ".fs", *shader.fragment, result);
                    if (!handle_result) {
                        return void_core::Err<std::size_t>(handle_result.error());
                    }
                    continue;
                }
            }
            if (shader.compute) {
                std::filesystem::path cs_path = root_path / *shader.compute;
                if (!std::filesystem::exists(cs_path)) {
                    auto handle_result = handle_missing_asset(shader.id + ".cs", *shader.compute, result);
                    if (!handle_result) {
                        return void_core::Err<std::size_t>(handle_result.error());
                    }
                    continue;
                }
            }
        }

        // When ShaderSystem is available:
        // auto* shader_system = ctx.get_service<ShaderSystem>();
        // if (shader_system) {
        //     ShaderLoadOptions opts;
        //     opts.variants = shader.variants;
        //     shader_system->load_shader(shader.id,
        //                                shader.vertex ? (root_path / *shader.vertex) : "",
        //                                shader.fragment ? (root_path / *shader.fragment) : "",
        //                                shader.compute ? (root_path / *shader.compute) : "",
        //                                opts);
        // }

        ++loaded;
    }

    return void_core::Ok(loaded);
}

void AssetBundleLoader::unload_bundle_assets(const LoadedBundle& bundle, LoadContext& ctx) {
    // When asset systems are available, unload assets here:
    // auto* asset_server = ctx.get_service<AssetServer>();
    // if (asset_server) {
    //     for (const auto& mesh : bundle.manifest.meshes) {
    //         asset_server->unload_mesh(mesh.id);
    //     }
    //     // ... etc for other asset types
    // }

    // For now, nothing to do since assets aren't actually loaded
    (void)bundle;
    (void)ctx;
}

void_core::Result<void> AssetBundleLoader::handle_missing_asset(
    const std::string& asset_id,
    const std::string& asset_path,
    AssetBundleLoadResult& result)
{
    std::string message = "Missing asset '" + asset_id + "' at path: " + asset_path;

    switch (m_missing_policy) {
        case MissingAssetPolicy::Error:
            return void_core::Err(message);

        case MissingAssetPolicy::Warn:
            result.warnings.push_back(message);
            return void_core::Ok();

        case MissingAssetPolicy::Skip:
            return void_core::Ok();
    }

    return void_core::Ok();
}

// =============================================================================
// Debugging
// =============================================================================

std::string AssetBundleLoader::format_state() const {
    std::ostringstream ss;
    ss << "AssetBundleLoader:\n";
    ss << "  Loaded bundles: " << m_loaded_bundles.size() << "\n";
    ss << "  PrefabRegistry: " << (m_prefab_registry ? "configured" : "not configured") << "\n";
    ss << "  DefinitionRegistry: " << (m_definition_registry ? "configured" : "not configured") << "\n";
    ss << "  SchemaRegistry: " << (m_schema_registry ? "configured" : "not configured") << "\n";
    ss << "  Missing asset policy: ";
    switch (m_missing_policy) {
        case MissingAssetPolicy::Error: ss << "Error"; break;
        case MissingAssetPolicy::Warn: ss << "Warn"; break;
        case MissingAssetPolicy::Skip: ss << "Skip"; break;
    }
    ss << "\n";
    ss << "  Strict validation: " << (m_strict_validation ? "enabled" : "disabled") << "\n";

    if (!m_loaded_bundles.empty()) {
        ss << "\n  Loaded bundles:\n";
        for (const auto& [name, bundle] : m_loaded_bundles) {
            ss << "    " << name << ":\n";
            ss << "      Prefabs: " << bundle.result.prefabs_loaded << "\n";
            ss << "      Definitions: " << bundle.result.definitions_loaded << "\n";
            ss << "      Total assets: " << bundle.result.total_assets() << "\n";
            if (!bundle.result.warnings.empty()) {
                ss << "      Warnings: " << bundle.result.warnings.size() << "\n";
            }
        }
    }

    return ss.str();
}

AssetBundleLoader::Stats AssetBundleLoader::get_stats() const {
    Stats stats;
    stats.bundles_loaded = m_loaded_bundles.size();

    for (const auto& [_, bundle] : m_loaded_bundles) {
        stats.total_prefabs += bundle.result.prefabs_loaded;
        stats.total_definitions += bundle.result.definitions_loaded;
        stats.total_assets += bundle.result.total_assets();
    }

    return stats;
}

} // namespace void_package
