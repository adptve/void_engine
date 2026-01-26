#pragma once

/// @file asset_loader.hpp
/// @brief Scene asset loading - integrates scene data with void_render

#include <void_engine/scene/scene_data.hpp>
#include <void_engine/scene/manifest_parser.hpp>
#include <void_engine/core/error.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_scene {

// =============================================================================
// Asset Types
// =============================================================================

/// Asset loading state
enum class AssetState : std::uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Failed,
    Unloading,
};

/// Asset type enum
enum class AssetType : std::uint8_t {
    Texture,
    Model,
    Material,
    Animation,
    Audio,
    Script,
    Shader,
};

/// Asset handle
struct AssetHandle {
    std::uint64_t id = 0;
    AssetType type = AssetType::Texture;

    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
    [[nodiscard]] static AssetHandle invalid() { return AssetHandle{}; }

    bool operator==(const AssetHandle& other) const {
        return id == other.id && type == other.type;
    }
};

/// Asset metadata
struct AssetMetadata {
    std::string name;
    std::string path;
    AssetType type = AssetType::Texture;
    AssetState state = AssetState::Unloaded;
    std::uint64_t size_bytes = 0;
    std::filesystem::file_time_type last_modified;
    std::string error_message;
};

// =============================================================================
// Loaded Asset Data
// =============================================================================

/// Loaded texture data (ready for upload to GPU)
struct LoadedTexture {
    std::string name;
    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = 4;
    bool is_hdr = false;
    bool is_cubemap = false;
    bool generate_mipmaps = true;
    bool srgb = true;
};

/// Loaded model data (meshes + materials)
struct LoadedMesh {
    std::string name;

    // Vertex data
    std::vector<float> positions;   // vec3
    std::vector<float> normals;     // vec3
    std::vector<float> texcoords;   // vec2
    std::vector<float> tangents;    // vec4
    std::vector<std::uint32_t> indices;

    // Skinning data (optional)
    std::vector<std::uint8_t> joint_indices;  // uvec4 packed as bytes
    std::vector<float> joint_weights;         // vec4

    // Material index
    std::int32_t material_index = -1;
};

struct LoadedMaterial {
    std::string name;

    // Base PBR
    std::array<float, 4> base_color = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    std::array<float, 3> emissive = {0.0f, 0.0f, 0.0f};

    // Texture paths
    std::string albedo_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    std::string emissive_texture;
    std::string occlusion_texture;

    // Advanced properties
    float transmission = 0.0f;
    float ior = 1.5f;
    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    float sheen = 0.0f;
    std::array<float, 3> sheen_color = {0.0f, 0.0f, 0.0f};
    float anisotropy = 0.0f;
};

struct LoadedModel {
    std::string name;
    std::string source_path;
    std::vector<LoadedMesh> meshes;
    std::vector<LoadedMaterial> materials;

    // Scene hierarchy (for glTF scenes)
    struct Node {
        std::string name;
        std::array<float, 3> translation = {0.0f, 0.0f, 0.0f};
        std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};  // quaternion
        std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};
        std::int32_t mesh_index = -1;
        std::vector<std::uint32_t> children;
    };
    std::vector<Node> nodes;
    std::vector<std::uint32_t> root_nodes;

    // Animation data
    struct AnimationClip {
        std::string name;
        float duration = 0.0f;
        // Channel data would go here
    };
    std::vector<AnimationClip> animations;
};

// =============================================================================
// Asset Loading Progress
// =============================================================================

/// Progress callback for asset loading
struct LoadProgress {
    std::uint32_t loaded = 0;
    std::uint32_t total = 0;
    std::string current_asset;
    float percent = 0.0f;

    [[nodiscard]] bool is_complete() const { return loaded >= total; }
};

using ProgressCallback = std::function<void(const LoadProgress&)>;

// =============================================================================
// Scene Asset Loader
// =============================================================================

/// Loads all assets referenced in a scene
class SceneAssetLoader {
public:
    SceneAssetLoader();
    ~SceneAssetLoader();

    /// Initialize with asset base path
    [[nodiscard]] void_core::Result<void> initialize(
        const std::filesystem::path& base_path);

    /// Shutdown and release all assets
    void shutdown();

    /// Load all assets referenced in scene (synchronous)
    [[nodiscard]] void_core::Result<void> load_scene_assets(
        const SceneData& scene,
        ProgressCallback progress = nullptr);

    /// Load all assets referenced in scene (asynchronous)
    [[nodiscard]] std::future<void_core::Result<void>> load_scene_assets_async(
        const SceneData& scene,
        ProgressCallback progress = nullptr);

    /// Load a single texture
    [[nodiscard]] AssetHandle load_texture(const std::string& path);

    /// Load a single model
    [[nodiscard]] AssetHandle load_model(const std::string& path);

    /// Unload a specific asset
    void unload(AssetHandle handle);

    /// Unload all assets for a scene
    void unload_scene_assets(const std::filesystem::path& scene_path);

    /// Get loaded texture data
    [[nodiscard]] const LoadedTexture* get_texture(AssetHandle handle) const;

    /// Get loaded model data
    [[nodiscard]] const LoadedModel* get_model(AssetHandle handle) const;

    /// Get asset metadata
    [[nodiscard]] const AssetMetadata* get_metadata(AssetHandle handle) const;

    /// Check if asset is loaded
    [[nodiscard]] bool is_loaded(AssetHandle handle) const;

    /// Get asset state
    [[nodiscard]] AssetState get_state(AssetHandle handle) const;

    /// Find asset by path
    [[nodiscard]] AssetHandle find_by_path(const std::string& path) const;

    /// Enable/disable hot-reload
    void set_hot_reload_enabled(bool enabled) { m_hot_reload_enabled = enabled; }

    /// Update - check for file changes and reload
    void update();

    /// Force reload of modified assets
    void reload_modified();

    /// Set callback for when asset is loaded
    void on_asset_loaded(std::function<void(AssetHandle, const AssetMetadata&)> callback);

    /// Set callback for when asset fails to load
    void on_asset_failed(std::function<void(AssetHandle, const std::string&)> callback);

    /// Get total memory usage
    [[nodiscard]] std::uint64_t total_memory_usage() const;

    /// Get list of all loaded assets
    [[nodiscard]] std::vector<AssetHandle> loaded_assets() const;

private:
    struct AssetEntry {
        AssetMetadata metadata;
        std::unique_ptr<LoadedTexture> texture;
        std::unique_ptr<LoadedModel> model;
        std::filesystem::path scene_owner;  // Scene that owns this asset
    };

    std::filesystem::path m_base_path;
    std::unordered_map<std::uint64_t, AssetEntry> m_assets;
    std::unordered_map<std::string, std::uint64_t> m_path_to_handle;
    std::uint64_t m_next_handle = 1;
    mutable std::mutex m_mutex;

    bool m_hot_reload_enabled = true;
    std::function<void(AssetHandle, const AssetMetadata&)> m_on_loaded;
    std::function<void(AssetHandle, const std::string&)> m_on_failed;

    // Internal loading functions
    void_core::Result<void> load_texture_internal(AssetEntry& entry);
    void_core::Result<void> load_model_internal(AssetEntry& entry);
    void collect_scene_assets(const SceneData& scene, std::vector<std::pair<std::string, AssetType>>& assets);
    std::filesystem::path resolve_path(const std::string& path) const;
};

// =============================================================================
// Asset Cache
// =============================================================================

/// LRU cache for loaded assets
class AssetCache {
public:
    AssetCache(std::uint64_t max_memory_bytes = 512 * 1024 * 1024);  // 512 MB default

    /// Add asset to cache
    void add(AssetHandle handle, std::uint64_t size_bytes);

    /// Mark asset as recently used
    void touch(AssetHandle handle);

    /// Remove asset from cache
    void remove(AssetHandle handle);

    /// Get assets to evict to make room for new asset
    [[nodiscard]] std::vector<AssetHandle> get_eviction_candidates(std::uint64_t required_bytes);

    /// Get current cache size
    [[nodiscard]] std::uint64_t current_size() const { return m_current_size; }

    /// Get max cache size
    [[nodiscard]] std::uint64_t max_size() const { return m_max_size; }

    /// Clear entire cache
    void clear();

private:
    struct CacheEntry {
        AssetHandle handle;
        std::uint64_t size_bytes = 0;
        std::uint64_t last_access = 0;
    };

    std::uint64_t m_max_size;
    std::uint64_t m_current_size = 0;
    std::uint64_t m_access_counter = 0;
    std::unordered_map<std::uint64_t, CacheEntry> m_entries;
};

} // namespace void_scene

// Hash specialization for AssetHandle
template<>
struct std::hash<void_scene::AssetHandle> {
    std::size_t operator()(const void_scene::AssetHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id) ^
               (std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(h.type)) << 1);
    }
};
