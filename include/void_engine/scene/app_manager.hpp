#pragma once

/// @file app_manager.hpp
/// @brief Unified application manager - combines manifest, scene, and assets

#include <void_engine/scene/manifest_parser.hpp>
#include <void_engine/scene/scene_parser.hpp>
#include <void_engine/scene/scene_instantiator.hpp>
#include <void_engine/scene/asset_loader.hpp>
#include <void_engine/scene/scene_serializer.hpp>
#include <void_engine/core/error.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace void_scene {

// =============================================================================
// App Load State
// =============================================================================

/// Application loading state
enum class AppLoadState : std::uint8_t {
    Unloaded,           ///< No app loaded
    LoadingManifest,    ///< Parsing manifest.toml
    LoadingAssets,      ///< Loading textures, models, etc.
    LoadingScene,       ///< Parsing and instantiating scene.toml
    Ready,              ///< App is fully loaded and ready
    Error,              ///< Loading failed
};

/// Application loading progress
struct AppLoadProgress {
    AppLoadState state = AppLoadState::Unloaded;
    std::string current_stage;
    std::string current_file;
    std::uint32_t assets_loaded = 0;
    std::uint32_t assets_total = 0;
    float percent = 0.0f;
    std::string error_message;

    [[nodiscard]] bool is_loading() const {
        return state == AppLoadState::LoadingManifest ||
               state == AppLoadState::LoadingAssets ||
               state == AppLoadState::LoadingScene;
    }

    [[nodiscard]] bool is_ready() const {
        return state == AppLoadState::Ready;
    }

    [[nodiscard]] bool has_error() const {
        return state == AppLoadState::Error;
    }
};

// =============================================================================
// App Configuration
// =============================================================================

/// Configuration for app loading
struct AppLoadConfig {
    bool load_assets_async = false;     ///< Load assets in background thread
    bool hot_reload_enabled = true;     ///< Enable hot-reload for scenes/assets
    bool preload_all_textures = true;   ///< Preload all referenced textures
    bool preload_all_models = true;     ///< Preload all referenced models
    std::uint32_t asset_load_threads = 4; ///< Number of threads for async loading
};

// =============================================================================
// App Manager
// =============================================================================

/// Unified manager for loading complete applications (manifest + scene + assets)
///
/// This is the primary entry point for loading a void_engine application.
/// It coordinates manifest parsing, asset loading, scene parsing, and ECS instantiation.
///
/// Usage:
/// ```cpp
/// AppManager app;
/// app.initialize(ecs_world);
///
/// auto result = app.load_app("examples/avatar-demo/manifest.toml");
/// if (!result) {
///     // Handle error
/// }
///
/// // In game loop:
/// app.update(delta_time);
/// ```
class AppManager {
public:
    using ProgressCallback = std::function<void(const AppLoadProgress&)>;
    using SceneChangedCallback = std::function<void(const SceneData&)>;

    AppManager();
    ~AppManager();

    // =========================================================================
    // Initialization
    // =========================================================================

    /// Initialize with ECS world
    [[nodiscard]] void_core::Result<void> initialize(void_ecs::World* world);

    /// Shutdown and release all resources
    void shutdown();

    // =========================================================================
    // App Loading
    // =========================================================================

    /// Load application from manifest.toml
    [[nodiscard]] void_core::Result<void> load_app(
        const std::filesystem::path& manifest_path,
        const AppLoadConfig& config = {});

    /// Load application asynchronously
    void load_app_async(
        const std::filesystem::path& manifest_path,
        const AppLoadConfig& config = {},
        ProgressCallback progress = nullptr);

    /// Unload current application
    void unload_app();

    /// Check if app is loaded
    [[nodiscard]] bool is_loaded() const { return m_load_state == AppLoadState::Ready; }

    /// Get current load state
    [[nodiscard]] AppLoadState load_state() const { return m_load_state; }

    /// Get load progress
    [[nodiscard]] const AppLoadProgress& load_progress() const { return m_progress; }

    // =========================================================================
    // Scene Operations
    // =========================================================================

    /// Load a different scene (from same app)
    [[nodiscard]] void_core::Result<void> load_scene(const std::filesystem::path& scene_path);

    /// Load scene additively (keep existing entities)
    [[nodiscard]] void_core::Result<void> load_scene_additive(const std::filesystem::path& scene_path);

    /// Unload a specific scene
    void unload_scene(const std::filesystem::path& scene_path);

    /// Save current scene to file
    [[nodiscard]] void_core::Result<void> save_scene(const std::filesystem::path& path);

    /// Get current scene data
    [[nodiscard]] const SceneData* current_scene() const;

    /// Get scene instance
    [[nodiscard]] const SceneInstance* current_scene_instance() const;

    // =========================================================================
    // Manifest Access
    // =========================================================================

    /// Get current manifest
    [[nodiscard]] const ManifestData* manifest() const;

    /// Get package info
    [[nodiscard]] const PackageInfo* package_info() const;

    /// Get app config
    [[nodiscard]] const AppConfig* app_config() const;

    // =========================================================================
    // Asset Access
    // =========================================================================

    /// Get asset loader
    [[nodiscard]] SceneAssetLoader& assets() { return m_asset_loader; }
    [[nodiscard]] const SceneAssetLoader& assets() const { return m_asset_loader; }

    /// Get texture by path
    [[nodiscard]] const LoadedTexture* get_texture(const std::string& path) const;

    /// Get model by path
    [[nodiscard]] const LoadedModel* get_model(const std::string& path) const;

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Enable/disable hot-reload
    void set_hot_reload_enabled(bool enabled);

    /// Check if hot-reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const { return m_hot_reload_enabled; }

    /// Force reload of current scene
    [[nodiscard]] void_core::Result<void> reload_scene();

    /// Force reload of all modified assets
    void reload_modified_assets();

    // =========================================================================
    // Update
    // =========================================================================

    /// Update - call once per frame
    /// Handles hot-reload checking, async loading progress, animation updates
    void update(float delta_time);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Set callback for scene changes (load/reload)
    void on_scene_changed(SceneChangedCallback callback);

    /// Set callback for load progress
    void on_load_progress(ProgressCallback callback);

    // =========================================================================
    // Sub-manager Access
    // =========================================================================

    /// Get manifest manager
    [[nodiscard]] ManifestManager& manifest_manager() { return m_manifest_manager; }

    /// Get live scene manager
    [[nodiscard]] LiveSceneManager& scene_manager() { return m_live_scene_manager; }

    /// Get scene instantiator
    [[nodiscard]] SceneInstantiator& instantiator() { return m_live_scene_manager.instantiator(); }

    /// Get scene serializer
    [[nodiscard]] SceneSerializer& serializer() { return m_serializer; }

private:
    // Internal loading steps
    void_core::Result<void> load_manifest(const std::filesystem::path& path);
    void_core::Result<void> load_assets();
    void_core::Result<void> load_scene_internal();

    void set_error(const std::string& message);
    void update_progress(AppLoadState state, const std::string& stage, float percent);

    // State
    void_ecs::World* m_world = nullptr;
    AppLoadState m_load_state = AppLoadState::Unloaded;
    AppLoadProgress m_progress;
    bool m_hot_reload_enabled = true;

    // Paths
    std::filesystem::path m_app_root;
    std::filesystem::path m_manifest_path;

    // Sub-managers
    ManifestManager m_manifest_manager;
    SceneAssetLoader m_asset_loader;
    LiveSceneManager m_live_scene_manager;
    SceneSerializer m_serializer;

    // Config
    AppLoadConfig m_config;

    // Callbacks
    ProgressCallback m_on_progress;
    SceneChangedCallback m_on_scene_changed;
};

} // namespace void_scene
