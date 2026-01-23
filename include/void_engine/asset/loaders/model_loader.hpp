#pragma once

/// @file model_loader.hpp
/// @brief 3D model asset loader for glTF, OBJ, and other formats

#include <void_engine/asset/loader.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace void_asset {

// =============================================================================
// Model Asset Types
// =============================================================================

/// Vertex attribute semantic
enum class VertexAttribute : std::uint8_t {
    Position,
    Normal,
    Tangent,
    TexCoord0,
    TexCoord1,
    Color0,
    Joints0,
    Weights0,
};

/// Mesh primitive topology
enum class PrimitiveTopology : std::uint8_t {
    Points,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan,
};

/// Mesh primitive data
struct MeshPrimitive {
    std::vector<float> positions;       // vec3
    std::vector<float> normals;         // vec3
    std::vector<float> tangents;        // vec4
    std::vector<float> texcoords0;      // vec2
    std::vector<float> texcoords1;      // vec2
    std::vector<float> colors0;         // vec4
    std::vector<std::uint8_t> joints0;  // uvec4 as bytes
    std::vector<float> weights0;        // vec4
    std::vector<std::uint32_t> indices;

    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    std::int32_t material_index = -1;

    /// Get vertex count
    [[nodiscard]] std::uint32_t vertex_count() const {
        return positions.empty() ? 0 : static_cast<std::uint32_t>(positions.size() / 3);
    }

    /// Get index count
    [[nodiscard]] std::uint32_t index_count() const {
        return static_cast<std::uint32_t>(indices.size());
    }

    /// Has attribute
    [[nodiscard]] bool has_attribute(VertexAttribute attr) const {
        switch (attr) {
            case VertexAttribute::Position: return !positions.empty();
            case VertexAttribute::Normal: return !normals.empty();
            case VertexAttribute::Tangent: return !tangents.empty();
            case VertexAttribute::TexCoord0: return !texcoords0.empty();
            case VertexAttribute::TexCoord1: return !texcoords1.empty();
            case VertexAttribute::Color0: return !colors0.empty();
            case VertexAttribute::Joints0: return !joints0.empty();
            case VertexAttribute::Weights0: return !weights0.empty();
        }
        return false;
    }
};

/// Material definition from model
struct ModelMaterial {
    std::string name;

    // Base PBR
    std::array<float, 4> base_color_factor = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic_factor = 0.0f;
    float roughness_factor = 0.5f;
    std::array<float, 3> emissive_factor = {0.0f, 0.0f, 0.0f};

    // Texture indices (-1 = none)
    std::int32_t base_color_texture = -1;
    std::int32_t metallic_roughness_texture = -1;
    std::int32_t normal_texture = -1;
    std::int32_t occlusion_texture = -1;
    std::int32_t emissive_texture = -1;

    // Advanced properties
    float normal_scale = 1.0f;
    float occlusion_strength = 1.0f;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;

    enum class AlphaMode : std::uint8_t {
        Opaque,
        Mask,
        Blend,
    };
    AlphaMode alpha_mode = AlphaMode::Opaque;

    // Extensions
    float transmission = 0.0f;
    float ior = 1.5f;
    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    float sheen = 0.0f;
    std::array<float, 3> sheen_color = {0.0f, 0.0f, 0.0f};
};

/// Texture info from model
struct ModelTexture {
    std::string name;
    std::string uri;
    std::int32_t sampler_index = -1;
    std::vector<std::uint8_t> embedded_data;  // For embedded textures
};

/// Sampler info from model
struct ModelSampler {
    enum class Filter : std::uint8_t {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear,
    };

    enum class Wrap : std::uint8_t {
        Repeat,
        ClampToEdge,
        MirroredRepeat,
    };

    Filter mag_filter = Filter::Linear;
    Filter min_filter = Filter::LinearMipmapLinear;
    Wrap wrap_s = Wrap::Repeat;
    Wrap wrap_t = Wrap::Repeat;
};

/// Mesh data (collection of primitives)
struct ModelMesh {
    std::string name;
    std::vector<MeshPrimitive> primitives;
};

/// Node in scene hierarchy
struct ModelNode {
    std::string name;
    std::array<float, 3> translation = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};  // quaternion
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};
    std::int32_t mesh_index = -1;
    std::int32_t skin_index = -1;
    std::vector<std::uint32_t> children;

    /// Check if node has transform
    [[nodiscard]] bool has_transform() const {
        return translation[0] != 0 || translation[1] != 0 || translation[2] != 0 ||
               rotation[0] != 0 || rotation[1] != 0 || rotation[2] != 0 || rotation[3] != 1 ||
               scale[0] != 1 || scale[1] != 1 || scale[2] != 1;
    }
};

/// Skin for skeletal animation
struct ModelSkin {
    std::string name;
    std::vector<std::uint32_t> joints;
    std::vector<std::array<float, 16>> inverse_bind_matrices;
    std::int32_t skeleton_root = -1;
};

/// Animation channel target
struct AnimationTarget {
    std::uint32_t node_index = 0;
    enum class Path : std::uint8_t {
        Translation,
        Rotation,
        Scale,
        Weights,
    };
    Path path = Path::Translation;
};

/// Animation sampler
struct AnimationSampler {
    std::vector<float> input;   // Keyframe times
    std::vector<float> output;  // Keyframe values
    enum class Interpolation : std::uint8_t {
        Linear,
        Step,
        CubicSpline,
    };
    Interpolation interpolation = Interpolation::Linear;
};

/// Animation channel
struct AnimationChannel {
    std::uint32_t sampler_index = 0;
    AnimationTarget target;
};

/// Animation clip
struct ModelAnimation {
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    float duration = 0.0f;
};

/// Scene in model
struct ModelScene {
    std::string name;
    std::vector<std::uint32_t> root_nodes;
};

/// Complete model asset
struct ModelAsset {
    std::string name;
    std::string source_path;

    std::vector<ModelMesh> meshes;
    std::vector<ModelMaterial> materials;
    std::vector<ModelTexture> textures;
    std::vector<ModelSampler> samplers;
    std::vector<ModelNode> nodes;
    std::vector<ModelSkin> skins;
    std::vector<ModelAnimation> animations;
    std::vector<ModelScene> scenes;
    std::int32_t default_scene = 0;

    // Statistics
    [[nodiscard]] std::uint32_t total_vertices() const {
        std::uint32_t count = 0;
        for (const auto& mesh : meshes) {
            for (const auto& prim : mesh.primitives) {
                count += prim.vertex_count();
            }
        }
        return count;
    }

    [[nodiscard]] std::uint32_t total_indices() const {
        std::uint32_t count = 0;
        for (const auto& mesh : meshes) {
            for (const auto& prim : mesh.primitives) {
                count += prim.index_count();
            }
        }
        return count;
    }

    [[nodiscard]] std::uint32_t total_triangles() const {
        return total_indices() / 3;
    }

    [[nodiscard]] bool has_animations() const {
        return !animations.empty();
    }

    [[nodiscard]] bool has_skins() const {
        return !skins.empty();
    }
};

// =============================================================================
// Model Loader
// =============================================================================

/// Configuration for model loading
struct ModelLoadConfig {
    bool load_textures = true;
    bool generate_tangents = true;
    bool merge_primitives = false;
    bool flip_uvs = false;
    float scale = 1.0f;
};

/// Loads 3D model assets
class ModelLoader : public AssetLoader<ModelAsset> {
public:
    ModelLoader() = default;
    explicit ModelLoader(ModelLoadConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"gltf", "glb", "obj", "fbx"};
    }

    [[nodiscard]] LoadResult<ModelAsset> load(LoadContext& ctx) override;

    [[nodiscard]] std::string type_name() const override {
        return "ModelAsset";
    }

    /// Set load config
    void set_config(ModelLoadConfig config) { m_config = std::move(config); }

    /// Get load config
    [[nodiscard]] const ModelLoadConfig& config() const { return m_config; }

private:
    LoadResult<ModelAsset> load_gltf(LoadContext& ctx, bool is_binary);
    LoadResult<ModelAsset> load_obj(LoadContext& ctx);

    void generate_tangents(MeshPrimitive& prim);
    void apply_scale(ModelAsset& model, float scale);

    ModelLoadConfig m_config;
};

} // namespace void_asset
