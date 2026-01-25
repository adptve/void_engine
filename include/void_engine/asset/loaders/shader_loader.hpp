#pragma once

/// @file shader_loader.hpp
/// @brief Shader asset loader for GLSL, WGSL, HLSL, and SPIR-V

#include <void_engine/asset/loader.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace void_asset {

// =============================================================================
// Shader Asset Types
// =============================================================================

/// Shader language
enum class ShaderLanguage : std::uint8_t {
    GLSL,
    HLSL,
    WGSL,
    SPIRV,
    Metal,
};

/// Shader stage
enum class ShaderStage : std::uint8_t {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation,
    RayGeneration,
    RayAnyHit,
    RayClosestHit,
    RayMiss,
    RayIntersection,
    Mesh,
    Task,
};

/// Get shader stage name
[[nodiscard]] inline const char* shader_stage_name(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return "vertex";
        case ShaderStage::Fragment: return "fragment";
        case ShaderStage::Compute: return "compute";
        case ShaderStage::Geometry: return "geometry";
        case ShaderStage::TessControl: return "tess_control";
        case ShaderStage::TessEvaluation: return "tess_evaluation";
        case ShaderStage::RayGeneration: return "raygen";
        case ShaderStage::RayAnyHit: return "anyhit";
        case ShaderStage::RayClosestHit: return "closesthit";
        case ShaderStage::RayMiss: return "miss";
        case ShaderStage::RayIntersection: return "intersection";
        case ShaderStage::Mesh: return "mesh";
        case ShaderStage::Task: return "task";
        default: return "unknown";
    }
}

/// Shader uniform/constant type
enum class ShaderDataType : std::uint8_t {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UInt,
    UInt2,
    UInt3,
    UInt4,
    Mat2,
    Mat3,
    Mat4,
    Sampler2D,
    SamplerCube,
    Sampler2DArray,
    StorageBuffer,
    UniformBuffer,
};

/// Reflection info for a shader input/output
struct ShaderVariable {
    std::string name;
    ShaderDataType type = ShaderDataType::Float;
    std::uint32_t location = 0;
    std::uint32_t binding = 0;
    std::uint32_t set = 0;
    std::uint32_t array_size = 1;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

/// Shader module (single stage)
struct ShaderModule {
    std::string name;
    ShaderStage stage = ShaderStage::Vertex;
    ShaderLanguage language = ShaderLanguage::GLSL;
    std::string source;
    std::vector<std::uint32_t> spirv;  // Compiled SPIR-V (if available)
    std::string entry_point = "main";

    // Reflection data
    std::vector<ShaderVariable> inputs;
    std::vector<ShaderVariable> outputs;
    std::vector<ShaderVariable> uniforms;
    std::vector<ShaderVariable> samplers;
    std::vector<ShaderVariable> storage_buffers;

    // Workgroup size for compute shaders
    std::array<std::uint32_t, 3> workgroup_size = {1, 1, 1};

    /// Check if has SPIR-V binary
    [[nodiscard]] bool has_spirv() const { return !spirv.empty(); }
};

/// Complete shader program (multiple stages linked)
struct ShaderAsset {
    std::string name;
    std::string source_path;
    ShaderLanguage language = ShaderLanguage::GLSL;

    std::optional<ShaderModule> vertex;
    std::optional<ShaderModule> fragment;
    std::optional<ShaderModule> compute;
    std::optional<ShaderModule> geometry;
    std::optional<ShaderModule> tess_control;
    std::optional<ShaderModule> tess_evaluation;

    // Combined reflection data
    std::vector<ShaderVariable> uniforms;
    std::vector<ShaderVariable> samplers;
    std::map<std::string, std::uint32_t> uniform_locations;

    // Defines/macros used during compilation
    std::map<std::string, std::string> defines;

    // Include dependencies
    std::vector<std::string> includes;

    /// Check if shader has stage
    [[nodiscard]] bool has_stage(ShaderStage stage) const {
        switch (stage) {
            case ShaderStage::Vertex: return vertex.has_value();
            case ShaderStage::Fragment: return fragment.has_value();
            case ShaderStage::Compute: return compute.has_value();
            case ShaderStage::Geometry: return geometry.has_value();
            case ShaderStage::TessControl: return tess_control.has_value();
            case ShaderStage::TessEvaluation: return tess_evaluation.has_value();
            default: return false;
        }
    }

    /// Check if is graphics shader (has vertex + fragment)
    [[nodiscard]] bool is_graphics() const {
        return vertex.has_value() && fragment.has_value();
    }

    /// Check if is compute shader
    [[nodiscard]] bool is_compute() const {
        return compute.has_value();
    }
};

// =============================================================================
// Shader Loader
// =============================================================================

/// Configuration for shader loading
struct ShaderLoadConfig {
    bool compile_to_spirv = false;
    bool reflect = true;
    std::map<std::string, std::string> defines;
    std::vector<std::string> include_paths;
    ShaderLanguage target_language = ShaderLanguage::GLSL;
};

/// Loads shader assets
class ShaderLoader : public AssetLoader<ShaderAsset> {
public:
    ShaderLoader() = default;
    explicit ShaderLoader(ShaderLoadConfig config) : m_config(std::move(config)) {}

    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"glsl", "vert", "frag", "comp", "geom", "tesc", "tese",
                "wgsl", "hlsl", "spv", "metal"};
    }

    [[nodiscard]] LoadResult<ShaderAsset> load(LoadContext& ctx) override;

    [[nodiscard]] std::string type_name() const override {
        return "ShaderAsset";
    }

    /// Set load config
    void set_config(ShaderLoadConfig config) { m_config = std::move(config); }

    /// Add define
    void add_define(const std::string& name, const std::string& value = "1") {
        m_config.defines[name] = value;
    }

    /// Add include path
    void add_include_path(const std::string& path) {
        m_config.include_paths.push_back(path);
    }

private:
    LoadResult<ShaderAsset> load_glsl(LoadContext& ctx);
    LoadResult<ShaderAsset> load_wgsl(LoadContext& ctx);
    LoadResult<ShaderAsset> load_hlsl(LoadContext& ctx);
    LoadResult<ShaderAsset> load_spirv(LoadContext& ctx);

    ShaderStage detect_stage(const std::string& path) const;
    ShaderLanguage detect_language(const std::string& ext) const;
    std::string preprocess(const std::string& source, const std::string& base_path);
    void reflect_module(ShaderModule& module);

    ShaderLoadConfig m_config;
};

// =============================================================================
// Shader Include Handler
// =============================================================================

/// Handles #include directives in shaders
class ShaderIncludeHandler {
public:
    using IncludeCallback = std::function<std::optional<std::string>(const std::string&)>;

    ShaderIncludeHandler() = default;

    /// Add include path
    void add_include_path(const std::string& path) {
        m_include_paths.push_back(path);
    }

    /// Set custom include callback
    void set_include_callback(IncludeCallback callback) {
        m_callback = std::move(callback);
    }

    /// Process includes in source
    std::string process(const std::string& source, const std::string& base_path);

    /// Get list of included files
    [[nodiscard]] const std::vector<std::string>& included_files() const {
        return m_included;
    }

private:
    std::string process_includes(const std::string& source,
                                  const std::string& base_path,
                                  int depth);
    std::optional<std::string> resolve_include(const std::string& include_path,
                                                const std::string& base_path);
    std::optional<std::string> read_file(const std::string& path);

    std::vector<std::string> m_include_paths;
    std::vector<std::string> m_included;
    IncludeCallback m_callback;
};

} // namespace void_asset
