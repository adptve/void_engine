/// @file app_manager.cpp
/// @brief Unified application manager implementation

#include <void_engine/scene/app_manager.hpp>

#include <spdlog/spdlog.h>

namespace void_scene {

// =============================================================================
// AppManager Implementation
// =============================================================================

AppManager::AppManager() = default;

AppManager::~AppManager() {
    shutdown();
}

void_core::Result<void> AppManager::initialize(void_ecs::World* world) {
    m_world = world;

    // Initialize live scene manager with world
    m_live_scene_manager.set_world(world);
    auto result = m_live_scene_manager.initialize();
    if (!result) {
        return result;
    }

    // Set up scene change callback
    m_live_scene_manager.on_scene_changed([this](const std::filesystem::path& path, const SceneData& data) {
        (void)path;
        if (m_on_scene_changed) {
            m_on_scene_changed(data);
        }
    });

    spdlog::info("AppManager initialized");
    return void_core::Ok();
}

void AppManager::shutdown() {
    unload_app();

    m_live_scene_manager.shutdown();
    m_asset_loader.shutdown();
    m_manifest_manager.shutdown();

    m_world = nullptr;
    m_load_state = AppLoadState::Unloaded;

    spdlog::info("AppManager shut down");
}

void_core::Result<void> AppManager::load_app(
    const std::filesystem::path& manifest_path,
    const AppLoadConfig& config)
{
    // Unload any existing app
    unload_app();

    m_config = config;
    m_manifest_path = manifest_path;
    m_app_root = manifest_path.parent_path();
    m_hot_reload_enabled = config.hot_reload_enabled;

    // Step 1: Load manifest
    update_progress(AppLoadState::LoadingManifest, "Loading manifest", 0.0f);
    auto manifest_result = load_manifest(manifest_path);
    if (!manifest_result) {
        set_error("Failed to load manifest: " + manifest_result.error().message());
        return manifest_result;
    }

    // Step 2: Load assets
    update_progress(AppLoadState::LoadingAssets, "Loading assets", 0.2f);
    auto assets_result = load_assets();
    if (!assets_result) {
        set_error("Failed to load assets: " + assets_result.error().message());
        return assets_result;
    }

    // Step 3: Load scene
    update_progress(AppLoadState::LoadingScene, "Loading scene", 0.8f);
    auto scene_result = load_scene_internal();
    if (!scene_result) {
        set_error("Failed to load scene: " + scene_result.error().message());
        return scene_result;
    }

    // Done
    update_progress(AppLoadState::Ready, "Ready", 1.0f);
    spdlog::info("App loaded successfully: {}", manifest_path.string());

    return void_core::Ok();
}

void AppManager::load_app_async(
    const std::filesystem::path& manifest_path,
    const AppLoadConfig& config,
    ProgressCallback progress)
{
    m_on_progress = std::move(progress);

    // For now, use synchronous loading
    // A proper async implementation would use std::async or a task system
    auto result = load_app(manifest_path, config);
    if (!result) {
        spdlog::error("Async app load failed: {}", result.error().message());
    }
}

void AppManager::unload_app() {
    m_live_scene_manager.unload_all();
    m_asset_loader.shutdown();
    m_manifest_manager.shutdown();

    m_app_root.clear();
    m_manifest_path.clear();
    m_load_state = AppLoadState::Unloaded;
    m_progress = AppLoadProgress{};
}

void_core::Result<void> AppManager::load_scene(const std::filesystem::path& scene_path) {
    // Unload existing scenes first
    m_live_scene_manager.unload_all();

    // Resolve path relative to app root
    auto full_path = scene_path.is_absolute() ? scene_path : (m_app_root / scene_path);

    return m_live_scene_manager.load_scene(full_path);
}

void_core::Result<void> AppManager::load_scene_additive(const std::filesystem::path& scene_path) {
    // Load without unloading existing scenes
    auto full_path = scene_path.is_absolute() ? scene_path : (m_app_root / scene_path);

    return m_live_scene_manager.load_scene(full_path);
}

void AppManager::unload_scene(const std::filesystem::path& scene_path) {
    auto full_path = scene_path.is_absolute() ? scene_path : (m_app_root / scene_path);
    m_live_scene_manager.unload_scene(full_path);
}

void_core::Result<void> AppManager::save_scene(const std::filesystem::path& path) {
    const auto* scene = current_scene();
    if (!scene) {
        return void_core::Err(void_core::Error("No scene loaded"));
    }

    return m_serializer.save(*scene, path);
}

const SceneData* AppManager::current_scene() const {
    return m_live_scene_manager.get_scene_data(m_live_scene_manager.current_scene_path());
}

const SceneInstance* AppManager::current_scene_instance() const {
    return m_live_scene_manager.get_scene_instance(m_live_scene_manager.current_scene_path());
}

const ManifestData* AppManager::manifest() const {
    return m_manifest_manager.manifest();
}

const PackageInfo* AppManager::package_info() const {
    const auto* m = manifest();
    return m ? &m->package : nullptr;
}

const AppConfig* AppManager::app_config() const {
    const auto* m = manifest();
    return m ? &m->app : nullptr;
}

const LoadedTexture* AppManager::get_texture(const std::string& path) const {
    auto handle = m_asset_loader.find_by_path(path);
    return m_asset_loader.get_texture(handle);
}

const LoadedModel* AppManager::get_model(const std::string& path) const {
    auto handle = m_asset_loader.find_by_path(path);
    return m_asset_loader.get_model(handle);
}

void AppManager::set_hot_reload_enabled(bool enabled) {
    m_hot_reload_enabled = enabled;
    m_live_scene_manager.set_hot_reload_enabled(enabled);
    m_asset_loader.set_hot_reload_enabled(enabled);
}

void_core::Result<void> AppManager::reload_scene() {
    return m_live_scene_manager.force_reload(m_live_scene_manager.current_scene_path());
}

void AppManager::reload_modified_assets() {
    m_asset_loader.reload_modified();
}

void AppManager::update(float delta_time) {
    // Check for hot-reload changes
    if (m_hot_reload_enabled) {
        m_manifest_manager.update();
        m_asset_loader.update();
    }

    // Update scene manager (polls for scene file changes)
    m_live_scene_manager.update(delta_time);
}

void AppManager::on_scene_changed(SceneChangedCallback callback) {
    m_on_scene_changed = std::move(callback);
}

void AppManager::on_load_progress(ProgressCallback callback) {
    m_on_progress = std::move(callback);
}

// =============================================================================
// Internal Loading Steps
// =============================================================================

void_core::Result<void> AppManager::load_manifest(const std::filesystem::path& path) {
    auto result = m_manifest_manager.initialize(path);
    if (!result) {
        return result;
    }

    const auto* manifest = m_manifest_manager.manifest();
    if (!manifest) {
        return void_core::Err(void_core::Error("Manifest parsed but no data available"));
    }

    if (!manifest->is_valid()) {
        return void_core::Err(void_core::Error("Invalid manifest: missing required fields"));
    }

    spdlog::info("Loaded manifest: {} v{}",
                 manifest->package.name, manifest->package.version);

    return void_core::Ok();
}

void_core::Result<void> AppManager::load_assets() {
    // Initialize asset loader with app root as base path
    auto result = m_asset_loader.initialize(m_app_root);
    if (!result) {
        return result;
    }

    // Get the scene path from manifest
    auto scene_path = m_manifest_manager.scene_path();
    if (scene_path.empty()) {
        return void_core::Err(void_core::Error("No scene path in manifest"));
    }

    // Parse scene to collect asset references
    SceneParser parser;
    auto scene_result = parser.parse(scene_path);
    if (!scene_result) {
        return void_core::Err(scene_result.error());
    }

    const auto& scene_data = *scene_result;

    // Set up progress callback
    auto asset_progress = [this](const LoadProgress& prog) {
        m_progress.assets_loaded = prog.loaded;
        m_progress.assets_total = prog.total;
        m_progress.current_file = prog.current_asset;
        m_progress.percent = 0.2f + (prog.percent * 0.6f);  // Scale to 20%-80%

        if (m_on_progress) {
            m_on_progress(m_progress);
        }
    };

    // Load all assets referenced in the scene
    return m_asset_loader.load_scene_assets(scene_data, asset_progress);
}

void_core::Result<void> AppManager::load_scene_internal() {
    auto scene_path = m_manifest_manager.scene_path();
    if (scene_path.empty()) {
        return void_core::Err(void_core::Error("No scene path in manifest"));
    }

    return m_live_scene_manager.load_scene(scene_path);
}

void AppManager::set_error(const std::string& message) {
    m_load_state = AppLoadState::Error;
    m_progress.state = AppLoadState::Error;
    m_progress.error_message = message;
    spdlog::error("AppManager error: {}", message);

    if (m_on_progress) {
        m_on_progress(m_progress);
    }
}

void AppManager::update_progress(AppLoadState state, const std::string& stage, float percent) {
    m_load_state = state;
    m_progress.state = state;
    m_progress.current_stage = stage;
    m_progress.percent = percent;

    if (m_on_progress) {
        m_on_progress(m_progress);
    }
}

} // namespace void_scene
