/// @file render_assets.hpp
/// @brief Hot-reloadable render assets (models, textures, shaders)
///
/// Assets are managed by RenderAssetManager which integrates with the
/// kernel's hot-reload system. Assets can be loaded at runtime via API
/// and will automatically reload when source files change.
///
/// This file uses existing types from:
/// - gl_renderer.hpp: GpuMesh
/// - texture.hpp: TextureLoadOptions, TextureHandle
/// - render_handles.hpp: ModelHandle, AssetTextureHandle, AssetShaderHandle

#pragma once

#include <void_engine/render/render_handles.hpp>
#include <void_engine/render/gl_renderer.hpp>  // GpuMesh
#include <void_engine/render/mesh.hpp>
#include <void_engine/render/material.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/core/error.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_render {

// Forward declarations
class RenderAssetManager;

// GpuMesh is already defined in gl_renderer.hpp - use that definition

// =============================================================================
// GpuTexture
// =============================================================================

/// @brief GPU-side texture
struct GpuTexture {
    std::uint32_t id = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t channels = 0;
    bool is_srgb = true;
    bool has_mipmaps = false;

    void destroy();
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

// =============================================================================
// GpuShader
// =============================================================================

/// @brief GPU-side compiled shader program
struct GpuShader {
    std::uint32_t program = 0;
    std::string name;

    // Cached uniform locations
    mutable std::unordered_map<std::string, std::int32_t> uniform_cache;

    void destroy();
    [[nodiscard]] bool is_valid() const noexcept { return program != 0; }
    void use() const;

    // Uniform setters
    void set_int(const std::string& name, int value) const;
    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, float x, float y) const;
    void set_vec3(const std::string& name, float x, float y, float z) const;
    void set_vec4(const std::string& name, float x, float y, float z, float w) const;
    void set_mat3(const std::string& name, const float* data) const;
    void set_mat4(const std::string& name, const float* data) const;

private:
    [[nodiscard]] std::int32_t get_location(const std::string& name) const;
};

// =============================================================================
// LoadedModel - Complete model with meshes and materials
// =============================================================================

/// @brief A loaded 3D model with all its meshes and materials
struct LoadedModel {
    std::string source_path;
    std::uint32_t generation = 0;  // Incremented on reload

    // Geometry
    std::vector<GpuMesh> meshes;
    std::vector<std::string> mesh_names;

    // Materials (indices match glTF material indices)
    std::vector<GpuMaterial> materials;

    // Textures owned by this model
    std::vector<GpuTexture> textures;

    // Scene hierarchy
    struct Node {
        std::string name;
        std::int32_t mesh_index = -1;
        std::int32_t material_index = -1;
        std::array<float, 16> local_transform;
        std::array<float, 16> world_transform;
        std::vector<std::int32_t> children;
        std::int32_t parent = -1;
    };
    std::vector<Node> nodes;
    std::vector<std::int32_t> root_nodes;

    // Bounds
    std::array<float, 3> min_bounds = {0, 0, 0};
    std::array<float, 3> max_bounds = {0, 0, 0};

    // Statistics
    std::size_t total_vertices = 0;
    std::size_t total_triangles = 0;

    void destroy();
    [[nodiscard]] bool is_valid() const noexcept { return !meshes.empty(); }
};

// =============================================================================
// LoadedTexture - Standalone texture asset
// =============================================================================

/// @brief A standalone loaded texture (not embedded in model)
struct LoadedTexture {
    std::string source_path;
    std::uint32_t generation = 0;

    GpuTexture gpu_texture;

    void destroy();
    [[nodiscard]] bool is_valid() const noexcept { return gpu_texture.is_valid(); }
};

// =============================================================================
// LoadedShader - Compiled shader program
// =============================================================================

/// @brief A loaded shader program
struct LoadedShader {
    std::string name;
    std::string vertex_path;
    std::string fragment_path;
    std::uint32_t generation = 0;

    GpuShader gpu_shader;

    // File modification times for hot-reload detection
    std::filesystem::file_time_type vertex_mtime;
    std::filesystem::file_time_type fragment_mtime;

    void destroy();
    [[nodiscard]] bool is_valid() const noexcept { return gpu_shader.is_valid(); }
};

// =============================================================================
// Asset Loading Options
// =============================================================================

/// @brief Options for loading models
struct ModelLoadOptions {
    bool generate_tangents = true;
    bool flip_uvs = false;
    float scale = 1.0f;
    bool load_textures = true;
    bool async = false;  // Async loading (returns immediately, loads in background)

    [[nodiscard]] static ModelLoadOptions defaults() { return {}; }
};

/// @brief Options for loading textures via RenderAssetManager
/// Note: This is separate from void_render::TextureLoadOptions in texture.hpp
/// which is used for lower-level texture creation.
struct AssetTextureLoadOptions {
    bool srgb = true;
    bool generate_mipmaps = true;
    bool flip_vertically = false;
    bool async = false;

    [[nodiscard]] static AssetTextureLoadOptions defaults() { return {}; }
};

/// @brief Options for loading shaders
struct ShaderLoadOptions {
    std::vector<std::pair<std::string, std::string>> defines;  // Preprocessor defines
    bool async = false;

    [[nodiscard]] static ShaderLoadOptions defaults() { return {}; }
};

// =============================================================================
// Asset Callbacks
// =============================================================================

/// @brief Callback when asset is loaded
using OnModelLoaded = std::function<void(ModelHandle handle, LoadedModel* model)>;
using OnTextureLoaded = std::function<void(AssetTextureHandle handle, LoadedTexture* texture)>;
using OnShaderLoaded = std::function<void(AssetShaderHandle handle, LoadedShader* shader)>;

/// @brief Callback when asset is reloaded
using OnModelReloaded = std::function<void(ModelHandle handle, LoadedModel* model)>;
using OnTextureReloaded = std::function<void(AssetTextureHandle handle, LoadedTexture* texture)>;
using OnShaderReloaded = std::function<void(AssetShaderHandle handle, LoadedShader* shader)>;

/// @brief Callback when asset fails to load
using OnAssetError = std::function<void(const std::string& path, const std::string& error)>;

// =============================================================================
// RenderAssetManager
// =============================================================================

/// @brief Central manager for render assets with hot-reload support
///
/// All asset loading goes through this manager. It handles:
/// - Loading from disk (sync or async)
/// - GPU resource management
/// - Hot-reload detection and execution
/// - Reference counting and cleanup
///
/// Designed for runtime use - engine never stops, assets load on demand.
class RenderAssetManager : public void_core::HotReloadable {
public:
    RenderAssetManager();
    ~RenderAssetManager();

    // Non-copyable, movable
    RenderAssetManager(const RenderAssetManager&) = delete;
    RenderAssetManager& operator=(const RenderAssetManager&) = delete;
    RenderAssetManager(RenderAssetManager&&) noexcept;
    RenderAssetManager& operator=(RenderAssetManager&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /// @brief Initialize the asset manager
    /// @param asset_root_path Base path for asset resolution
    void_core::Result<void> initialize(const std::filesystem::path& asset_root_path);

    /// @brief Shutdown and release all resources
    void shutdown();

    // =========================================================================
    // Model Loading
    // =========================================================================

    /// @brief Load a model from path
    /// @param path Path to model file (glTF, GLB)
    /// @param options Loading options
    /// @return Handle to loaded model (may be loading if async)
    [[nodiscard]] ModelHandle load_model(
        const std::string& path,
        const ModelLoadOptions& options = ModelLoadOptions::defaults());

    /// @brief Get loaded model by handle
    /// @return Pointer to model or nullptr if not loaded
    [[nodiscard]] LoadedModel* get_model(ModelHandle handle);
    [[nodiscard]] const LoadedModel* get_model(ModelHandle handle) const;

    /// @brief Check if model is loaded
    [[nodiscard]] bool is_model_loaded(ModelHandle handle) const;

    /// @brief Unload a model (decrements ref count)
    void unload_model(ModelHandle handle);

    /// @brief Force reload a model from disk
    void reload_model(ModelHandle handle);

    // =========================================================================
    // Texture Loading
    // =========================================================================

    /// @brief Load a texture from path
    [[nodiscard]] AssetTextureHandle load_texture(
        const std::string& path,
        const AssetTextureLoadOptions& options = AssetTextureLoadOptions::defaults());

    /// @brief Get loaded texture by handle
    [[nodiscard]] LoadedTexture* get_texture(AssetTextureHandle handle);
    [[nodiscard]] const LoadedTexture* get_texture(AssetTextureHandle handle) const;

    /// @brief Check if texture is loaded
    [[nodiscard]] bool is_texture_loaded(AssetTextureHandle handle) const;

    /// @brief Unload a texture
    void unload_texture(AssetTextureHandle handle);

    /// @brief Force reload a texture from disk
    void reload_texture(AssetTextureHandle handle);

    // =========================================================================
    // Shader Loading
    // =========================================================================

    /// @brief Load a shader from vertex/fragment paths
    [[nodiscard]] AssetShaderHandle load_shader(
        const std::string& name,
        const std::string& vertex_path,
        const std::string& fragment_path,
        const ShaderLoadOptions& options = ShaderLoadOptions::defaults());

    /// @brief Load shader from source strings
    [[nodiscard]] AssetShaderHandle load_shader_from_source(
        const std::string& name,
        const std::string& vertex_source,
        const std::string& fragment_source);

    /// @brief Get loaded shader by handle
    [[nodiscard]] LoadedShader* get_shader(AssetShaderHandle handle);
    [[nodiscard]] const LoadedShader* get_shader(AssetShaderHandle handle) const;

    /// @brief Get shader by name
    [[nodiscard]] LoadedShader* get_shader_by_name(const std::string& name);

    /// @brief Check if shader is loaded
    [[nodiscard]] bool is_shader_loaded(AssetShaderHandle handle) const;

    /// @brief Unload a shader
    void unload_shader(AssetShaderHandle handle);

    /// @brief Force reload a shader from disk
    void reload_shader(AssetShaderHandle handle);

    // =========================================================================
    // Built-in Assets
    // =========================================================================

    /// @brief Get built-in mesh by name ("sphere", "cube", etc.)
    [[nodiscard]] GpuMesh* get_builtin_mesh(const std::string& name);

    /// @brief Get default PBR shader
    [[nodiscard]] LoadedShader* get_default_shader();

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// @brief Poll for file changes and trigger reloads
    /// Call this each frame from HotReloadPoll stage
    void poll_hot_reload();

    /// @brief Enable/disable hot-reload
    void set_hot_reload_enabled(bool enabled);

    /// @brief Check if hot-reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// @brief Set callback for model loaded
    void on_model_loaded(OnModelLoaded callback);

    /// @brief Set callback for model reloaded
    void on_model_reloaded(OnModelReloaded callback);

    /// @brief Set callback for texture loaded
    void on_texture_loaded(OnTextureLoaded callback);

    /// @brief Set callback for texture reloaded
    void on_texture_reloaded(OnTextureReloaded callback);

    /// @brief Set callback for shader loaded
    void on_shader_loaded(OnShaderLoaded callback);

    /// @brief Set callback for shader reloaded
    void on_shader_reloaded(OnShaderReloaded callback);

    /// @brief Set callback for asset errors
    void on_error(OnAssetError callback);

    // =========================================================================
    // Statistics
    // =========================================================================

    /// @brief Get total GPU memory used by assets
    [[nodiscard]] std::size_t gpu_memory_usage() const;

    /// @brief Get number of loaded models
    [[nodiscard]] std::size_t model_count() const;

    /// @brief Get number of loaded textures
    [[nodiscard]] std::size_t texture_count() const;

    /// @brief Get number of loaded shaders
    [[nodiscard]] std::size_t shader_count() const;

    // =========================================================================
    // HotReloadable Interface
    // =========================================================================

    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "RenderAssetManager"; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Check if path looks like a model file
[[nodiscard]] bool is_model_file(const std::string& path);

/// @brief Check if path looks like a texture file
[[nodiscard]] bool is_texture_file(const std::string& path);

/// @brief Check if path looks like a shader file
[[nodiscard]] bool is_shader_file(const std::string& path);

/// @brief Get file extension (lowercase, without dot)
[[nodiscard]] std::string get_extension(const std::string& path);

} // namespace void_render
