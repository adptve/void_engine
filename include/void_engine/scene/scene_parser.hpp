#pragma once

/// @file scene_parser.hpp
/// @brief Scene parser for TOML scene files

#include "scene_data.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace void_scene {

// =============================================================================
// SceneParser
// =============================================================================

/// Parser for scene.toml files
class SceneParser {
public:
    SceneParser() = default;
    ~SceneParser() = default;

    /// Parse a scene file
    [[nodiscard]] void_core::Result<SceneData> parse(const std::filesystem::path& path);

    /// Parse a scene from string content
    [[nodiscard]] void_core::Result<SceneData> parse_string(
        const std::string& content,
        const std::string& source_name = "<string>");

    /// Get last parse error details
    [[nodiscard]] const std::string& last_error() const { return m_last_error; }

private:
    std::string m_last_error;
};

// =============================================================================
// HotReloadableScene
// =============================================================================

/// Scene that supports hot-reload
class HotReloadableScene : public void_core::HotReloadable {
public:
    using ReloadCallback = std::function<void(const SceneData&)>;

    /// Constructor
    explicit HotReloadableScene(std::filesystem::path path);

    /// Get scene data
    [[nodiscard]] const SceneData& data() const { return m_data; }

    /// Get scene path
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    /// Set callback for when scene is reloaded
    void on_reload(ReloadCallback callback) { m_on_reload = std::move(callback); }

    /// Reload from disk
    [[nodiscard]] void_core::Result<void> reload();

    // HotReloadable interface
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "HotReloadableScene"; }

private:
    std::filesystem::path m_path;
    SceneData m_data;
    void_core::Version m_version{1, 0, 0};
    ReloadCallback m_on_reload;
    SceneParser m_parser;
};

// =============================================================================
// SceneManager
// =============================================================================

/// Manages scene loading and hot-reload
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    /// Initialize the scene manager
    [[nodiscard]] void_core::Result<void> initialize();

    /// Shutdown the scene manager
    void shutdown();

    /// Load a scene from file
    [[nodiscard]] void_core::Result<void> load_scene(const std::filesystem::path& path);

    /// Get current scene data
    [[nodiscard]] const SceneData* current_scene() const;

    /// Get scene by path
    [[nodiscard]] const SceneData* get_scene(const std::filesystem::path& path) const;

    /// Enable/disable hot-reload watching
    void set_hot_reload_enabled(bool enabled);

    /// Update (polls for file changes if hot-reload enabled)
    void update();

    /// Set callback for scene load/reload
    void on_scene_loaded(std::function<void(const std::filesystem::path&, const SceneData&)> callback);

    /// Get hot-reload system
    [[nodiscard]] void_core::HotReloadSystem& hot_reload_system() { return m_hot_reload; }

private:
    void_core::HotReloadSystem m_hot_reload;
    std::map<std::filesystem::path, std::unique_ptr<HotReloadableScene>> m_scenes;
    std::map<std::filesystem::path, std::filesystem::file_time_type> m_file_timestamps;
    std::filesystem::path m_current_scene_path;
    bool m_hot_reload_enabled = true;
    std::function<void(const std::filesystem::path&, const SceneData&)> m_on_scene_loaded;
};

} // namespace void_scene
