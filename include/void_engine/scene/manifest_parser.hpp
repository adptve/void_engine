#pragma once

/// @file manifest_parser.hpp
/// @brief Package manifest (manifest.json) parsing

#include <void_engine/core/error.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace void_scene {

// =============================================================================
// Manifest Data Structures
// =============================================================================

/// Package metadata
struct PackageInfo {
    std::string name;
    std::string display_name;
    std::string version = "1.0.0";
    std::string description;
    std::string author;
    std::string license;
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
};

/// Render layer configuration
struct LayerConfig {
    std::string name;
    std::string type = "content";  // "content", "overlay", "background"
    int priority = 0;
    std::string blend = "normal";  // "normal", "replace", "additive", "multiply"
    bool enabled = true;
};

/// Resource limits
struct ResourceLimits {
    std::uint32_t max_entities = 10000;
    std::uint64_t max_memory = 536870912;  // 512 MB
    std::uint32_t max_layers = 16;
    std::uint32_t max_textures = 1000;
    std::uint32_t max_meshes = 1000;
};

/// Permissions configuration
struct Permissions {
    bool scripts = true;
    bool network = false;
    bool file_system = false;
    bool audio = true;
    bool input = true;
    bool clipboard = false;
};

/// LOD configuration
struct LodConfig {
    bool enabled = true;
    float bias = 0.0f;
    std::vector<float> distances = {10.0f, 25.0f, 50.0f, 100.0f};
};

/// Streaming configuration
struct StreamingConfig {
    bool enabled = false;
    float load_distance = 100.0f;
    float unload_distance = 150.0f;
    std::uint32_t max_concurrent_loads = 4;
};

/// Application configuration
struct AppConfig {
    std::string app_type = "game";  // "game", "demo", "tool", "editor"
    std::string scene;              // Default scene file
    std::vector<LayerConfig> layers;
    Permissions permissions;
    ResourceLimits resources;
    LodConfig lod;
    StreamingConfig streaming;
};

/// Asset configuration
struct AssetConfig {
    std::vector<std::string> include;   // Directories to include
    std::vector<std::string> exclude;   // Patterns to exclude
    std::string base_path;              // Base path for assets
    bool hot_reload = true;             // Enable hot-reload for assets
};

/// Platform requirements
struct PlatformRequirements {
    std::string min_version = "1.0.0";
    std::vector<std::string> required_features;
    std::vector<std::string> optional_features;
};

/// Complete manifest data
struct ManifestData {
    PackageInfo package;
    AppConfig app;
    AssetConfig assets;
    PlatformRequirements platform;

    /// Check if manifest is valid
    [[nodiscard]] bool is_valid() const {
        return !package.name.empty() && !app.scene.empty();
    }
};

// =============================================================================
// Manifest Parser
// =============================================================================

/// Parses manifest.json files
class ManifestParser {
public:
    ManifestParser() = default;

    /// Parse manifest from file
    [[nodiscard]] void_core::Result<ManifestData> parse(const std::filesystem::path& path);

    /// Parse manifest from string
    [[nodiscard]] void_core::Result<ManifestData> parse_string(
        const std::string& content,
        const std::string& source_name = "manifest.json");

    /// Get last error
    [[nodiscard]] const std::string& last_error() const noexcept { return m_last_error; }

private:
    std::string m_last_error;
};

// =============================================================================
// Manifest Manager
// =============================================================================

/// Manages application manifest with hot-reload support
class ManifestManager {
public:
    ManifestManager() = default;
    ~ManifestManager() = default;

    /// Initialize with manifest path
    [[nodiscard]] void_core::Result<void> initialize(const std::filesystem::path& manifest_path);

    /// Shutdown
    void shutdown();

    /// Get current manifest
    [[nodiscard]] const ManifestData* manifest() const;

    /// Get scene path (resolved relative to manifest)
    [[nodiscard]] std::filesystem::path scene_path() const;

    /// Get asset base path
    [[nodiscard]] std::filesystem::path asset_base_path() const;

    /// Check for manifest changes and reload
    void update();

    /// Force reload
    [[nodiscard]] void_core::Result<void> reload();

    /// Set callback for manifest changes
    void on_manifest_changed(std::function<void(const ManifestData&)> callback);

private:
    ManifestParser m_parser;
    std::optional<ManifestData> m_manifest;
    std::filesystem::path m_manifest_path;
    std::filesystem::file_time_type m_last_modified;
    std::function<void(const ManifestData&)> m_on_changed;
    bool m_hot_reload_enabled = true;
};

} // namespace void_scene
