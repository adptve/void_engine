#pragma once

/// @file shaderc_compiler.hpp
/// @brief Shaderc-based shader compiler for void_shader
///
/// This compiler uses Google's shaderc library to compile GLSL/HLSL to SPIR-V,
/// and SPIRV-Cross for transpilation to other backends (GLSL, HLSL, MSL, WGSL).
///
/// Supports:
/// - GLSL → SPIR-V compilation
/// - HLSL → SPIR-V compilation
/// - SPIR-V → GLSL/HLSL/MSL transpilation via SPIRV-Cross
/// - Automatic reflection extraction
/// - Include file resolution
/// - Shader variants with defines
/// - Hot-reload compatible

#include "fwd.hpp"
#include "types.hpp"
#include "binding.hpp"
#include "source.hpp"
#include "compiler.hpp"
#include <void_engine/core/error.hpp>

#ifdef VOID_HAS_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#ifdef VOID_HAS_SPIRV_CROSS
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_hlsl.hpp>
#include <spirv_cross/spirv_msl.hpp>
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace void_shader {

// =============================================================================
// ShadercIncluder
// =============================================================================

#ifdef VOID_HAS_SHADERC

/// Custom includer for shaderc that resolves #include directives
class ShadercIncluder : public shaderc::CompileOptions::IncluderInterface {
public:
    /// Constructor with include paths
    explicit ShadercIncluder(std::vector<std::string> include_paths)
        : m_include_paths(std::move(include_paths)) {}

    /// Add include resolver
    void set_resolver(std::shared_ptr<ShaderIncludeResolver> resolver) {
        m_resolver = std::move(resolver);
    }

    /// Resolve include
    shaderc_include_result* GetInclude(
        const char* requested_source,
        shaderc_include_type type,
        const char* requesting_source,
        size_t /*include_depth*/) override
    {
        std::string resolved_path;
        std::string content;

        // Try custom resolver first
        if (m_resolver) {
            auto result = m_resolver->resolve(requested_source);
            if (result.has_value()) {
                content = *result;
                resolved_path = requested_source;
            }
        }

        // Try include paths
        if (content.empty()) {
            std::string base_dir;
            if (type == shaderc_include_type_relative) {
                std::filesystem::path req_path(requesting_source);
                base_dir = req_path.parent_path().string();
            }

            for (const auto& include_path : m_include_paths) {
                std::filesystem::path full_path;
                if (!base_dir.empty() && type == shaderc_include_type_relative) {
                    full_path = std::filesystem::path(base_dir) / requested_source;
                } else {
                    full_path = std::filesystem::path(include_path) / requested_source;
                }

                if (std::filesystem::exists(full_path)) {
                    std::ifstream file(full_path);
                    if (file.is_open()) {
                        std::stringstream ss;
                        ss << file.rdbuf();
                        content = ss.str();
                        resolved_path = full_path.string();
                        break;
                    }
                }
            }
        }

        // Create result
        auto* result = new shaderc_include_result;
        if (!content.empty()) {
            // Store content (must persist until ReleaseInclude)
            auto* data = new IncludeData{resolved_path, content};
            m_include_data.push_back(data);

            result->source_name = data->path.c_str();
            result->source_name_length = data->path.length();
            result->content = data->content.c_str();
            result->content_length = data->content.length();
            result->user_data = data;
        } else {
            // Error: include not found
            std::string error = "Include not found: " + std::string(requested_source);
            auto* data = new IncludeData{"", error};
            m_include_data.push_back(data);

            result->source_name = "";
            result->source_name_length = 0;
            result->content = data->content.c_str();
            result->content_length = data->content.length();
            result->user_data = data;
        }

        return result;
    }

    /// Release include result
    void ReleaseInclude(shaderc_include_result* result) override {
        if (result && result->user_data) {
            auto* data = static_cast<IncludeData*>(result->user_data);
            auto it = std::find(m_include_data.begin(), m_include_data.end(), data);
            if (it != m_include_data.end()) {
                m_include_data.erase(it);
                delete data;
            }
        }
        delete result;
    }

private:
    struct IncludeData {
        std::string path;
        std::string content;
    };

    std::vector<std::string> m_include_paths;
    std::shared_ptr<ShaderIncludeResolver> m_resolver;
    std::vector<IncludeData*> m_include_data;
};

#endif // VOID_HAS_SHADERC

// =============================================================================
// ShadercCompiler
// =============================================================================

/// Shader compiler using shaderc for GLSL/HLSL → SPIR-V compilation
/// and SPIRV-Cross for transpilation to other backends
class ShadercCompiler : public ShaderCompiler {
public:
    /// Configuration
    struct Config {
        bool generate_debug_info = false;
        bool optimize = true;
        bool auto_bind_uniforms = false;
        bool hlsl_offsets = false;
        std::uint32_t vulkan_version = 12;  // 10, 11, 12, 13
        std::uint32_t glsl_version = 450;
        bool invert_y = false;  // For Vulkan coordinate system

        Config() = default;
    };

    /// Constructor
    ShadercCompiler() : m_config(Config{}) {}
    explicit ShadercCompiler(const Config& config) : m_config(config) {}

    /// Compile shader source to multiple targets
    [[nodiscard]] void_core::Result<CompileResult> compile(
        const ShaderSource& source,
        const CompilerConfig& config) override
    {
#ifndef VOID_HAS_SHADERC
        CompileResult result;
        result.errors.push_back("Shaderc not available. Build with VOID_HAS_SHADERC=1");
        return void_core::Ok(std::move(result));
#else
        CompileResult result;

        // First compile to SPIR-V
        auto spirv_result = compile_to_spirv(source, config);
        if (!spirv_result.is_ok) {
            result.errors = std::move(spirv_result.errors);
            result.warnings = std::move(spirv_result.warnings);
            return void_core::Ok(std::move(result));
        }

        result.warnings = std::move(spirv_result.warnings);

        // Extract reflection from SPIR-V
        auto reflection_result = extract_reflection(spirv_result.spirv);
        if (reflection_result) {
            result.reflection = std::move(*reflection_result);
        }

        // Store SPIR-V if requested
        for (const auto& target : config.targets) {
            if (target == CompileTarget::SpirV) {
                CompiledShader compiled;
                compiled.target = CompileTarget::SpirV;
                compiled.stage = source.stage.value_or(ShaderStage::Vertex);
                compiled.entry_point = source.entry_point.empty() ? "main" : source.entry_point;

                // Convert uint32 SPIR-V to bytes
                compiled.binary.resize(spirv_result.spirv.size() * sizeof(std::uint32_t));
                std::memcpy(compiled.binary.data(), spirv_result.spirv.data(),
                           spirv_result.spirv.size() * sizeof(std::uint32_t));

                result.compiled[CompileTarget::SpirV] = std::move(compiled);
            }
        }

        // Transpile to other targets using SPIRV-Cross
        for (const auto& target : config.targets) {
            if (target == CompileTarget::SpirV) continue;

            auto transpile_result = transpile(spirv_result.spirv, target, source.stage.value_or(ShaderStage::Vertex));
            if (transpile_result) {
                result.compiled[target] = std::move(*transpile_result);
            } else {
                result.warnings.push_back("Failed to transpile to " +
                    std::string(compile_target_name(target)) + ": " + transpile_result.error().message());
            }
        }

        // Run custom validation rules
        auto validation = run_validation(result.reflection, source);
        if (!validation) {
            result.errors.push_back(validation.error().message());
        }

        return void_core::Ok(std::move(result));
#endif
    }

    [[nodiscard]] std::string name() const override {
        return "ShadercCompiler";
    }

    [[nodiscard]] bool supports_language(SourceLanguage lang) const override {
        switch (lang) {
            case SourceLanguage::Glsl:
            case SourceLanguage::Hlsl:
            case SourceLanguage::SpirV:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] bool supports_target(CompileTarget target) const override {
        switch (target) {
            case CompileTarget::SpirV:
                return true;
#ifdef VOID_HAS_SPIRV_CROSS
            case CompileTarget::Glsl330:
            case CompileTarget::Glsl450:
            case CompileTarget::GlslEs300:
            case CompileTarget::GlslEs310:
            case CompileTarget::HLSL:
            case CompileTarget::MSL:
                return true;
#endif
            default:
                return false;
        }
    }

    /// Set include resolver
    void set_include_resolver(std::shared_ptr<ShaderIncludeResolver> resolver) {
        m_resolver = std::move(resolver);
    }

    /// Get config
    [[nodiscard]] const Config& shader_config() const { return m_config; }

private:
#ifdef VOID_HAS_SHADERC

    struct SpirvCompileResult {
        std::vector<std::uint32_t> spirv;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        bool is_ok = false;
    };

    /// Compile to SPIR-V using shaderc
    SpirvCompileResult compile_to_spirv(const ShaderSource& source, const CompilerConfig& config) {
        SpirvCompileResult result;

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        // Set up options
        if (m_config.optimize) {
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        } else {
            options.SetOptimizationLevel(shaderc_optimization_level_zero);
        }

        if (m_config.generate_debug_info || config.generate_debug_info) {
            options.SetGenerateDebugInfo();
        }

        // Set Vulkan version
        switch (m_config.vulkan_version) {
            case 10: options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0); break;
            case 11: options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1); break;
            case 12: options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2); break;
            case 13: options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3); break;
            default: options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2); break;
        }

        // Add defines from config
        for (const auto& [name, value] : config.defines) {
            if (value.empty()) {
                options.AddMacroDefinition(name);
            } else {
                options.AddMacroDefinition(name, value);
            }
        }

        // Add defines from source
        for (const auto& define : source.defines) {
            if (define.value.empty()) {
                options.AddMacroDefinition(define.name);
            } else {
                options.AddMacroDefinition(define.name, define.value);
            }
        }

        // Set up includer
        auto includer = std::make_unique<ShadercIncluder>(config.include_paths);
        if (m_resolver) {
            includer->set_resolver(m_resolver);
        }
        options.SetIncluder(std::move(includer));

        // Determine shader kind
        shaderc_shader_kind kind = get_shader_kind(source.stage.value_or(ShaderStage::Vertex));

        // Set source language
        if (source.language == SourceLanguage::Hlsl) {
            options.SetSourceLanguage(shaderc_source_language_hlsl);
            if (m_config.hlsl_offsets) {
                options.SetHlslOffsets(true);
            }
        } else {
            options.SetSourceLanguage(shaderc_source_language_glsl);
        }

        // Compile
        std::string filename = source.name.empty() ? "shader" : source.name;
        std::string entry = source.entry_point.empty() ? "main" : source.entry_point;

        auto compile_result = compiler.CompileGlslToSpv(
            source.code, kind, filename.c_str(), entry.c_str(), options);

        // Check result
        if (compile_result.GetCompilationStatus() != shaderc_compilation_status_success) {
            result.errors.push_back(compile_result.GetErrorMessage());
            result.is_ok = false;
            return result;
        }

        // Get warnings
        if (compile_result.GetNumWarnings() > 0) {
            result.warnings.push_back(compile_result.GetErrorMessage());
        }

        // Copy SPIR-V
        result.spirv.assign(compile_result.cbegin(), compile_result.cend());
        result.is_ok = true;

        return result;
    }

    /// Get shaderc shader kind from stage
    static shaderc_shader_kind get_shader_kind(ShaderStage stage) {
        switch (stage) {
            case ShaderStage::Vertex: return shaderc_vertex_shader;
            case ShaderStage::Fragment: return shaderc_fragment_shader;
            case ShaderStage::Compute: return shaderc_compute_shader;
            case ShaderStage::Geometry: return shaderc_geometry_shader;
            case ShaderStage::TessControl: return shaderc_tess_control_shader;
            case ShaderStage::TessEvaluation: return shaderc_tess_evaluation_shader;
            default: return shaderc_vertex_shader;
        }
    }

#endif // VOID_HAS_SHADERC

#ifdef VOID_HAS_SPIRV_CROSS

    /// Extract reflection from SPIR-V
    std::optional<ShaderReflection> extract_reflection(const std::vector<std::uint32_t>& spirv) {
        try {
            spirv_cross::Compiler compiler(spirv);
            ShaderReflection reflection;

            // Get shader resources
            auto resources = compiler.get_shader_resources();

            // Extract uniform buffers
            for (const auto& ub : resources.uniform_buffers) {
                BindingInfo info;
                info.name = ub.name;
                info.group = compiler.get_decoration(ub.id, spv::DecorationDescriptorSet);
                info.binding = compiler.get_decoration(ub.id, spv::DecorationBinding);
                info.type = BindingType::UniformBuffer;

                // Get buffer size
                const auto& type = compiler.get_type(ub.base_type_id);
                info.min_binding_size = compiler.get_declared_struct_size(type);

                add_to_bind_group(reflection, info);
            }

            // Extract storage buffers
            for (const auto& sb : resources.storage_buffers) {
                BindingInfo info;
                info.name = sb.name;
                info.group = compiler.get_decoration(sb.id, spv::DecorationDescriptorSet);
                info.binding = compiler.get_decoration(sb.id, spv::DecorationBinding);
                info.type = BindingType::StorageBuffer;

                // Check if read-only
                auto flags = compiler.get_buffer_block_flags(sb.id);
                if (flags.get(spv::DecorationNonWritable)) {
                    info.type = BindingType::ReadOnlyStorageBuffer;
                }

                add_to_bind_group(reflection, info);
            }

            // Extract sampled images (textures)
            for (const auto& img : resources.sampled_images) {
                BindingInfo info;
                info.name = img.name;
                info.group = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
                info.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
                info.type = BindingType::SampledTexture;

                add_to_bind_group(reflection, info);
            }

            // Extract separate samplers
            for (const auto& sampler : resources.separate_samplers) {
                BindingInfo info;
                info.name = sampler.name;
                info.group = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
                info.binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
                info.type = BindingType::Sampler;

                add_to_bind_group(reflection, info);
            }

            // Extract separate images
            for (const auto& img : resources.separate_images) {
                BindingInfo info;
                info.name = img.name;
                info.group = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
                info.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
                info.type = BindingType::SampledTexture;

                add_to_bind_group(reflection, info);
            }

            // Extract storage images
            for (const auto& img : resources.storage_images) {
                BindingInfo info;
                info.name = img.name;
                info.group = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
                info.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
                info.type = BindingType::StorageTexture;

                add_to_bind_group(reflection, info);
            }

            // Extract push constants
            if (!resources.push_constant_buffers.empty()) {
                const auto& pc = resources.push_constant_buffers[0];
                const auto& type = compiler.get_type(pc.base_type_id);

                PushConstantRange range;
                range.offset = 0;
                range.size = static_cast<std::uint32_t>(compiler.get_declared_struct_size(type));
                range.stages = ShaderStage::Vertex;  // Will be determined by actual shader stage

                reflection.push_constants = range;
            }

            // Extract stage inputs (vertex attributes)
            for (const auto& input : resources.stage_inputs) {
                VertexInput vi;
                vi.name = input.name;
                vi.location = compiler.get_decoration(input.id, spv::DecorationLocation);

                const auto& type = compiler.get_type(input.type_id);
                vi.format = spirv_type_to_vertex_format(type);

                reflection.vertex_inputs.push_back(vi);
            }

            // Extract stage outputs
            for (const auto& output : resources.stage_outputs) {
                FragmentOutput fo;
                fo.name = output.name;
                fo.location = compiler.get_decoration(output.id, spv::DecorationLocation);

                const auto& type = compiler.get_type(output.type_id);
                fo.format = spirv_type_to_vertex_format(type);

                reflection.fragment_outputs.push_back(fo);
            }

            // Extract entry points
            auto entry_points = compiler.get_entry_points_and_stages();
            for (const auto& ep : entry_points) {
                reflection.entry_points.push_back(ep.name);
            }

            return reflection;
        } catch (const std::exception& e) {
            // Reflection failed, return empty
            return std::nullopt;
        }
    }

    /// Add binding info to appropriate bind group
    static void add_to_bind_group(ShaderReflection& reflection, const BindingInfo& info) {
        auto& group = reflection.bind_groups[info.group];
        group.group = info.group;
        group.bindings.push_back(info);
    }

    /// Convert SPIRV-Cross type to VertexFormat
    static VertexFormat spirv_type_to_vertex_format(const spirv_cross::SPIRType& type) {
        switch (type.basetype) {
            case spirv_cross::SPIRType::Float:
                switch (type.vecsize) {
                    case 1: return VertexFormat::Float32;
                    case 2: return VertexFormat::Float32x2;
                    case 3: return VertexFormat::Float32x3;
                    case 4: return VertexFormat::Float32x4;
                    default: return VertexFormat::Float32x4;
                }
            case spirv_cross::SPIRType::Int:
                switch (type.vecsize) {
                    case 1: return VertexFormat::Sint32;
                    case 2: return VertexFormat::Sint32x2;
                    case 3: return VertexFormat::Sint32x3;
                    case 4: return VertexFormat::Sint32x4;
                    default: return VertexFormat::Sint32x4;
                }
            case spirv_cross::SPIRType::UInt:
                switch (type.vecsize) {
                    case 1: return VertexFormat::Uint32;
                    case 2: return VertexFormat::Uint32x2;
                    case 3: return VertexFormat::Uint32x3;
                    case 4: return VertexFormat::Uint32x4;
                    default: return VertexFormat::Uint32x4;
                }
            default:
                return VertexFormat::Float32x4;
        }
    }

    /// Transpile SPIR-V to other backends
    void_core::Result<CompiledShader> transpile(
        const std::vector<std::uint32_t>& spirv,
        CompileTarget target,
        ShaderStage stage)
    {
        CompiledShader compiled;
        compiled.target = target;
        compiled.stage = stage;
        compiled.entry_point = "main";

        try {
            switch (target) {
                case CompileTarget::Glsl330:
                case CompileTarget::Glsl450:
                case CompileTarget::GlslEs300:
                case CompileTarget::GlslEs310: {
                    spirv_cross::CompilerGLSL glsl_compiler(spirv);
                    spirv_cross::CompilerGLSL::Options options;

                    switch (target) {
                        case CompileTarget::Glsl330:
                            options.version = 330;
                            options.es = false;
                            break;
                        case CompileTarget::Glsl450:
                            options.version = 450;
                            options.es = false;
                            break;
                        case CompileTarget::GlslEs300:
                            options.version = 300;
                            options.es = true;
                            break;
                        case CompileTarget::GlslEs310:
                            options.version = 310;
                            options.es = true;
                            break;
                        default:
                            break;
                    }

                    options.vulkan_semantics = false;
                    options.enable_420pack_extension = (options.version >= 420);

                    glsl_compiler.set_common_options(options);

                    std::string glsl = glsl_compiler.compile();
                    compiled.source = std::move(glsl);
                    break;
                }

                case CompileTarget::HLSL: {
                    spirv_cross::CompilerHLSL hlsl_compiler(spirv);
                    spirv_cross::CompilerHLSL::Options options;
                    options.shader_model = 50;  // SM 5.0

                    hlsl_compiler.set_hlsl_options(options);

                    std::string hlsl = hlsl_compiler.compile();
                    compiled.source = std::move(hlsl);
                    break;
                }

                case CompileTarget::MSL: {
                    spirv_cross::CompilerMSL msl_compiler(spirv);
                    spirv_cross::CompilerMSL::Options options;
                    options.platform = spirv_cross::CompilerMSL::Options::macOS;
                    options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);

                    msl_compiler.set_msl_options(options);

                    std::string msl = msl_compiler.compile();
                    compiled.source = std::move(msl);
                    break;
                }

                default:
                    return void_core::Err<CompiledShader>(
                        ShaderError::unsupported_target(compile_target_name(target)));
            }

            return void_core::Ok(std::move(compiled));
        } catch (const spirv_cross::CompilerError& e) {
            return void_core::Err<CompiledShader>(
                ShaderError::compile_failed("SPIRV-Cross", e.what()));
        }
    }

#else // !VOID_HAS_SPIRV_CROSS

    std::optional<ShaderReflection> extract_reflection(const std::vector<std::uint32_t>& /*spirv*/) {
        return std::nullopt;
    }

    void_core::Result<CompiledShader> transpile(
        const std::vector<std::uint32_t>& /*spirv*/,
        CompileTarget target,
        ShaderStage /*stage*/)
    {
        return void_core::Err<CompiledShader>(
            ShaderError::unsupported_target(
                std::string(compile_target_name(target)) + " (SPIRV-Cross not available)"));
    }

#endif // VOID_HAS_SPIRV_CROSS

    Config m_config;
    std::shared_ptr<ShaderIncludeResolver> m_resolver;
};

// =============================================================================
// Factory Registration
// =============================================================================

/// Register shaderc compiler with factory
inline void register_shaderc_compiler() {
    CompilerFactory::register_compiler("shaderc", []() {
        return std::make_unique<ShadercCompiler>();
    });
}

/// Auto-register on include (optional, can be disabled)
#ifndef VOID_SHADER_NO_AUTO_REGISTER
namespace detail {
    struct ShadercAutoRegister {
        ShadercAutoRegister() {
            register_shaderc_compiler();
        }
    };
    inline ShadercAutoRegister shaderc_auto_register;
}
#endif

} // namespace void_shader
