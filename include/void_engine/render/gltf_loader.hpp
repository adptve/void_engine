/// @file gltf_loader.hpp
/// @brief glTF 2.0 model loading with hot-reload support

#pragma once

#include <void_engine/render/mesh.hpp>
#include <void_engine/render/material.hpp>
#include <void_engine/render/texture.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace void_render {

// =============================================================================
// Transform
// =============================================================================

/// @brief TRS transform with matrix computation
struct GltfTransform {
    std::array<float, 3> translation = {0, 0, 0};
    std::array<float, 4> rotation = {0, 0, 0, 1};  // quaternion (x, y, z, w)
    std::array<float, 3> scale = {1, 1, 1};

    /// Get 4x4 transformation matrix (column-major)
    [[nodiscard]] std::array<float, 16> to_matrix() const noexcept;

    /// Multiply two 4x4 matrices (column-major)
    [[nodiscard]] static std::array<float, 16> multiply(
        const std::array<float, 16>& a,
        const std::array<float, 16>& b) noexcept;
};

// =============================================================================
// GltfPrimitive - Single drawable within a mesh
// =============================================================================

/// @brief Single primitive (draw call) within a mesh
struct GltfPrimitive {
    MeshData mesh_data;
    std::int32_t material_index = -1;  // -1 = default material

    // Bounding box
    std::array<float, 3> min_bounds = {0, 0, 0};
    std::array<float, 3> max_bounds = {0, 0, 0};
};

// =============================================================================
// GltfMesh - Collection of primitives
// =============================================================================

/// @brief Mesh containing one or more primitives
struct GltfMesh {
    std::string name;
    std::vector<GltfPrimitive> primitives;
};

// =============================================================================
// GltfNode - Scene graph node
// =============================================================================

/// @brief Scene graph node with transform and optional mesh/camera/skin
struct GltfNode {
    std::string name;
    GltfTransform local_transform;
    std::array<float, 16> world_matrix = {{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    }};

    std::int32_t mesh_index = -1;
    std::int32_t skin_index = -1;
    std::int32_t camera_index = -1;
    std::int32_t light_index = -1;

    std::vector<std::int32_t> children;
    std::int32_t parent = -1;
};

// =============================================================================
// GltfMaterial - Loaded PBR material
// =============================================================================

/// @brief PBR material loaded from glTF
struct GltfMaterial {
    GpuMaterial gpu_material;
    std::string name;

    // Texture paths for hot-reload
    std::string base_color_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
    std::string occlusion_texture;
    std::string emissive_texture;
};

// =============================================================================
// GltfTexture - Loaded texture data
// =============================================================================

/// @brief Texture loaded from glTF with sampler settings
struct GltfTexture {
    std::string name;
    std::string uri;
    TextureData data;

    // Sampler settings
    std::uint32_t min_filter = 0x2601;  // GL_LINEAR
    std::uint32_t mag_filter = 0x2601;
    std::uint32_t wrap_s = 0x2901;      // GL_REPEAT
    std::uint32_t wrap_t = 0x2901;
};

// =============================================================================
// GltfScene - Complete loaded scene
// =============================================================================

/// @brief Complete scene loaded from glTF file
struct GltfScene {
    std::string name;
    std::string source_path;

    std::vector<GltfNode> nodes;
    std::vector<GltfMesh> meshes;
    std::vector<GltfMaterial> materials;
    std::vector<GltfTexture> textures;

    std::vector<std::int32_t> root_nodes;

    // Bounding box of entire scene
    std::array<float, 3> min_bounds = {0, 0, 0};
    std::array<float, 3> max_bounds = {0, 0, 0};

    // Statistics
    std::size_t total_vertices = 0;
    std::size_t total_triangles = 0;
    std::size_t total_draw_calls = 0;
};

// =============================================================================
// GltfLoader - Main loader class
// =============================================================================

/// @brief glTF 2.0 loader with support for glb and gltf formats
class GltfLoader {
public:
    /// @brief Load options
    struct LoadOptions {
        bool load_textures = true;
        bool generate_tangents = true;
        bool flip_uvs = false;
        bool merge_primitives = false;
        float scale = 1.0f;

        LoadOptions() = default;
    };

    GltfLoader();
    ~GltfLoader();

    // Move-only
    GltfLoader(GltfLoader&&) noexcept;
    GltfLoader& operator=(GltfLoader&&) noexcept;
    GltfLoader(const GltfLoader&) = delete;
    GltfLoader& operator=(const GltfLoader&) = delete;

    /// @brief Default load options
    [[nodiscard]] static LoadOptions default_options();

    /// @brief Load glTF file with default options
    [[nodiscard]] std::optional<GltfScene> load(const std::string& path);

    /// @brief Load glTF file with custom options
    [[nodiscard]] std::optional<GltfScene> load(
        const std::string& path,
        const LoadOptions& options);

    /// @brief Get last error message
    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// GltfSceneManager - Hot-reloadable scene manager
// =============================================================================

/// @brief Manages multiple loaded glTF scenes with hot-reload support
class GltfSceneManager {
public:
    /// @brief Scene entry with metadata
    struct SceneEntry {
        std::string path;
        GltfScene scene;
        std::filesystem::file_time_type last_modified;
        bool dirty = false;
    };

    GltfSceneManager();
    ~GltfSceneManager();

    // Move-only
    GltfSceneManager(GltfSceneManager&&) noexcept;
    GltfSceneManager& operator=(GltfSceneManager&&) noexcept;
    GltfSceneManager(const GltfSceneManager&) = delete;
    GltfSceneManager& operator=(const GltfSceneManager&) = delete;

    /// @brief Load scene from file, returns index
    [[nodiscard]] std::optional<std::size_t> load(
        const std::string& path,
        const GltfLoader::LoadOptions& options = {});

    /// @brief Get scene by index
    [[nodiscard]] GltfScene* get(std::size_t index);
    [[nodiscard]] const GltfScene* get(std::size_t index) const;

    /// @brief Check for file changes and reload if needed
    void check_hot_reload(const GltfLoader::LoadOptions& options = {});

    /// @brief Check if scene was recently reloaded
    [[nodiscard]] bool is_dirty(std::size_t index) const;

    /// @brief Clear dirty flag
    void clear_dirty(std::size_t index);

    /// @brief Get scene count
    [[nodiscard]] std::size_t count() const noexcept;

    /// @brief Remove scene by index
    void remove(std::size_t index);

    /// @brief Clear all scenes
    void clear();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if a mesh name looks like a file path
[[nodiscard]] bool is_model_path(const std::string& mesh_name);

} // namespace void_render
