/// @file shader_loader.cpp
/// @brief Shader asset loader implementation

#include <void_engine/asset/loaders/shader_loader.hpp>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace void_asset {

// =============================================================================
// Shader Include Handler Implementation
// =============================================================================

std::string ShaderIncludeHandler::process(const std::string& source,
                                          const std::string& base_path) {
    m_included.clear();
    return process_includes(source, base_path, 0);
}

std::string ShaderIncludeHandler::process_includes(const std::string& source,
                                                   const std::string& base_path,
                                                   int depth) {
    // Prevent infinite recursion
    if (depth > 32) {
        return source;
    }

    std::string result;
    result.reserve(source.size());

    std::istringstream stream(source);
    std::string line;

    // Match #include "path" or #include <path>
    std::regex include_regex(R"(^\s*#\s*include\s*[<"]([^>"]+)[>"])");

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, include_regex)) {
            std::string include_path = match[1].str();

            auto content = resolve_include(include_path, base_path);
            if (content) {
                // Track included file
                m_included.push_back(include_path);

                // Process nested includes
                std::filesystem::path inc_path(include_path);
                std::string inc_base = inc_path.parent_path().string();
                if (inc_base.empty()) {
                    inc_base = base_path;
                }

                result += "// Begin include: " + include_path + "\n";
                result += process_includes(*content, inc_base, depth + 1);
                result += "// End include: " + include_path + "\n";
            } else {
                // Keep original line if include not found
                result += "// ERROR: Could not resolve include: " + include_path + "\n";
                result += line + "\n";
            }
        } else {
            result += line + "\n";
        }
    }

    return result;
}

std::optional<std::string> ShaderIncludeHandler::resolve_include(
    const std::string& include_path,
    const std::string& base_path) {

    // Try custom callback first
    if (m_callback) {
        auto result = m_callback(include_path);
        if (result) {
            return result;
        }
    }

    // Try relative to base path
    std::filesystem::path full_path = std::filesystem::path(base_path) / include_path;
    if (auto content = read_file(full_path.string())) {
        return content;
    }

    // Try include paths
    for (const auto& inc_path : m_include_paths) {
        full_path = std::filesystem::path(inc_path) / include_path;
        if (auto content = read_file(full_path.string())) {
            return content;
        }
    }

    return std::nullopt;
}

std::optional<std::string> ShaderIncludeHandler::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// =============================================================================
// Shader Loader Implementation
// =============================================================================

LoadResult<ShaderAsset> ShaderLoader::load(LoadContext& ctx) {
    const auto& ext = ctx.extension();

    // Detect language from extension
    ShaderLanguage lang = detect_language(ext);

    switch (lang) {
        case ShaderLanguage::GLSL:
            return load_glsl(ctx);
        case ShaderLanguage::WGSL:
            return load_wgsl(ctx);
        case ShaderLanguage::HLSL:
            return load_hlsl(ctx);
        case ShaderLanguage::SPIRV:
            return load_spirv(ctx);
        default:
            return load_glsl(ctx);  // Default to GLSL
    }
}

ShaderLanguage ShaderLoader::detect_language(const std::string& ext) const {
    if (ext == "wgsl") {
        return ShaderLanguage::WGSL;
    } else if (ext == "hlsl") {
        return ShaderLanguage::HLSL;
    } else if (ext == "spv") {
        return ShaderLanguage::SPIRV;
    } else if (ext == "metal") {
        return ShaderLanguage::Metal;
    }
    // Default: glsl, vert, frag, comp, geom, tesc, tese
    return ShaderLanguage::GLSL;
}

ShaderStage ShaderLoader::detect_stage(const std::string& path) const {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::string stem = p.stem().string();

    // Check extension
    if (ext == "vert") return ShaderStage::Vertex;
    if (ext == "frag") return ShaderStage::Fragment;
    if (ext == "comp") return ShaderStage::Compute;
    if (ext == "geom") return ShaderStage::Geometry;
    if (ext == "tesc") return ShaderStage::TessControl;
    if (ext == "tese") return ShaderStage::TessEvaluation;

    // Check filename suffix
    std::string lower_stem = stem;
    std::transform(lower_stem.begin(), lower_stem.end(), lower_stem.begin(), ::tolower);

    if (lower_stem.ends_with(".vert") || lower_stem.ends_with("_vert") ||
        lower_stem.ends_with(".vs") || lower_stem.ends_with("_vs")) {
        return ShaderStage::Vertex;
    }
    if (lower_stem.ends_with(".frag") || lower_stem.ends_with("_frag") ||
        lower_stem.ends_with(".fs") || lower_stem.ends_with("_fs") ||
        lower_stem.ends_with(".ps") || lower_stem.ends_with("_ps")) {
        return ShaderStage::Fragment;
    }
    if (lower_stem.ends_with(".comp") || lower_stem.ends_with("_comp") ||
        lower_stem.ends_with(".cs") || lower_stem.ends_with("_cs")) {
        return ShaderStage::Compute;
    }
    if (lower_stem.ends_with(".geom") || lower_stem.ends_with("_geom") ||
        lower_stem.ends_with(".gs") || lower_stem.ends_with("_gs")) {
        return ShaderStage::Geometry;
    }

    // Default to vertex
    return ShaderStage::Vertex;
}

std::string ShaderLoader::preprocess(const std::string& source,
                                     const std::string& base_path) {
    // Process includes
    ShaderIncludeHandler handler;
    for (const auto& path : m_config.include_paths) {
        handler.add_include_path(path);
    }

    std::string processed = handler.process(source, base_path);

    // Inject defines at the beginning (after #version if present)
    if (!m_config.defines.empty()) {
        std::string defines_block;
        for (const auto& [name, value] : m_config.defines) {
            defines_block += "#define " + name + " " + value + "\n";
        }

        // Find #version directive
        std::regex version_regex(R"(^(\s*#\s*version\s+\d+.*\n))");
        std::smatch match;
        if (std::regex_search(processed, match, version_regex)) {
            // Insert after #version
            processed = match[1].str() + defines_block + match.suffix().str();
        } else {
            // No version, prepend
            processed = defines_block + processed;
        }
    }

    return processed;
}

void ShaderLoader::reflect_module(ShaderModule& module) {
    if (!m_config.reflect) {
        return;
    }

    // Parse source to extract uniform/attribute info
    // This is a simplified reflection - full reflection would use SPIRV-Cross

    const std::string& source = module.source;

    // Extract uniform blocks
    std::regex uniform_regex(R"(uniform\s+(\w+)\s+(\w+)\s*;)");
    std::sregex_iterator it(source.begin(), source.end(), uniform_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        ShaderVariable var;
        var.name = (*it)[2].str();
        std::string type_str = (*it)[1].str();

        // Map type string to ShaderDataType
        if (type_str == "float") var.type = ShaderDataType::Float;
        else if (type_str == "vec2") var.type = ShaderDataType::Float2;
        else if (type_str == "vec3") var.type = ShaderDataType::Float3;
        else if (type_str == "vec4") var.type = ShaderDataType::Float4;
        else if (type_str == "int") var.type = ShaderDataType::Int;
        else if (type_str == "ivec2") var.type = ShaderDataType::Int2;
        else if (type_str == "ivec3") var.type = ShaderDataType::Int3;
        else if (type_str == "ivec4") var.type = ShaderDataType::Int4;
        else if (type_str == "uint") var.type = ShaderDataType::UInt;
        else if (type_str == "uvec2") var.type = ShaderDataType::UInt2;
        else if (type_str == "uvec3") var.type = ShaderDataType::UInt3;
        else if (type_str == "uvec4") var.type = ShaderDataType::UInt4;
        else if (type_str == "mat2") var.type = ShaderDataType::Mat2;
        else if (type_str == "mat3") var.type = ShaderDataType::Mat3;
        else if (type_str == "mat4") var.type = ShaderDataType::Mat4;
        else if (type_str == "sampler2D") var.type = ShaderDataType::Sampler2D;
        else if (type_str == "samplerCube") var.type = ShaderDataType::SamplerCube;
        else if (type_str == "sampler2DArray") var.type = ShaderDataType::Sampler2DArray;

        if (type_str.find("sampler") != std::string::npos) {
            module.samplers.push_back(var);
        } else {
            module.uniforms.push_back(var);
        }
    }

    // Extract layout-qualified uniforms with binding
    std::regex layout_uniform_regex(
        R"(layout\s*\(\s*(?:set\s*=\s*(\d+)\s*,\s*)?binding\s*=\s*(\d+)\s*\)\s*uniform\s+(\w+)\s+(\w+))");
    it = std::sregex_iterator(source.begin(), source.end(), layout_uniform_regex);

    for (; it != end; ++it) {
        ShaderVariable var;
        if ((*it)[1].matched) {
            var.set = static_cast<std::uint32_t>(std::stoi((*it)[1].str()));
        }
        var.binding = static_cast<std::uint32_t>(std::stoi((*it)[2].str()));
        std::string type_str = (*it)[3].str();
        var.name = (*it)[4].str();

        if (type_str.find("sampler") != std::string::npos) {
            module.samplers.push_back(var);
        } else {
            module.uniforms.push_back(var);
        }
    }

    // Extract inputs (for vertex shader)
    if (module.stage == ShaderStage::Vertex) {
        std::regex input_regex(R"(layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\s+(\w+)\s+(\w+))");
        it = std::sregex_iterator(source.begin(), source.end(), input_regex);

        for (; it != end; ++it) {
            ShaderVariable var;
            var.location = static_cast<std::uint32_t>(std::stoi((*it)[1].str()));
            var.name = (*it)[3].str();
            module.inputs.push_back(var);
        }
    }

    // Extract outputs (for fragment shader)
    if (module.stage == ShaderStage::Fragment) {
        std::regex output_regex(R"(layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*out\s+(\w+)\s+(\w+))");
        it = std::sregex_iterator(source.begin(), source.end(), output_regex);

        for (; it != end; ++it) {
            ShaderVariable var;
            var.location = static_cast<std::uint32_t>(std::stoi((*it)[1].str()));
            var.name = (*it)[3].str();
            module.outputs.push_back(var);
        }
    }

    // Extract compute shader workgroup size
    if (module.stage == ShaderStage::Compute) {
        std::regex workgroup_regex(
            R"(layout\s*\(\s*local_size_x\s*=\s*(\d+)(?:\s*,\s*local_size_y\s*=\s*(\d+))?(?:\s*,\s*local_size_z\s*=\s*(\d+))?\s*\))");
        std::smatch match;
        if (std::regex_search(source, match, workgroup_regex)) {
            module.workgroup_size[0] = static_cast<std::uint32_t>(std::stoi(match[1].str()));
            if (match[2].matched) {
                module.workgroup_size[1] = static_cast<std::uint32_t>(std::stoi(match[2].str()));
            }
            if (match[3].matched) {
                module.workgroup_size[2] = static_cast<std::uint32_t>(std::stoi(match[3].str()));
            }
        }
    }
}

LoadResult<ShaderAsset> ShaderLoader::load_glsl(LoadContext& ctx) {
    // Read source
    const auto& data = ctx.data();
    std::string source(reinterpret_cast<const char*>(data.data()), data.size());

    ShaderAsset asset;
    asset.name = ctx.name();
    asset.source_path = ctx.path().string();
    asset.language = ShaderLanguage::GLSL;
    asset.defines = m_config.defines;

    // Detect shader stage
    ShaderStage stage = detect_stage(ctx.path().string());

    // Preprocess
    std::string processed = preprocess(source, ctx.path().parent_path().string());

    // Create module
    ShaderModule module;
    module.name = ctx.name();
    module.stage = stage;
    module.language = ShaderLanguage::GLSL;
    module.source = processed;
    module.entry_point = "main";

    // Reflect
    reflect_module(module);

    // Assign to appropriate stage
    switch (stage) {
        case ShaderStage::Vertex:
            asset.vertex = std::move(module);
            break;
        case ShaderStage::Fragment:
            asset.fragment = std::move(module);
            break;
        case ShaderStage::Compute:
            asset.compute = std::move(module);
            break;
        case ShaderStage::Geometry:
            asset.geometry = std::move(module);
            break;
        case ShaderStage::TessControl:
            asset.tess_control = std::move(module);
            break;
        case ShaderStage::TessEvaluation:
            asset.tess_evaluation = std::move(module);
            break;
        default:
            break;
    }

    // Combine reflection data
    auto combine_vars = [&](const std::optional<ShaderModule>& mod) {
        if (!mod) return;
        for (const auto& u : mod->uniforms) {
            asset.uniforms.push_back(u);
        }
        for (const auto& s : mod->samplers) {
            asset.samplers.push_back(s);
        }
    };

    combine_vars(asset.vertex);
    combine_vars(asset.fragment);
    combine_vars(asset.compute);
    combine_vars(asset.geometry);
    combine_vars(asset.tess_control);
    combine_vars(asset.tess_evaluation);

    return asset;
}

LoadResult<ShaderAsset> ShaderLoader::load_wgsl(LoadContext& ctx) {
    const auto& data = ctx.data();
    std::string source(reinterpret_cast<const char*>(data.data()), data.size());

    ShaderAsset asset;
    asset.name = ctx.name();
    asset.source_path = ctx.path().string();
    asset.language = ShaderLanguage::WGSL;

    // WGSL can contain multiple entry points
    // Parse @vertex and @fragment annotations

    ShaderModule vertex_module;
    vertex_module.name = ctx.name() + "_vertex";
    vertex_module.stage = ShaderStage::Vertex;
    vertex_module.language = ShaderLanguage::WGSL;
    vertex_module.source = source;

    ShaderModule fragment_module;
    fragment_module.name = ctx.name() + "_fragment";
    fragment_module.stage = ShaderStage::Fragment;
    fragment_module.language = ShaderLanguage::WGSL;
    fragment_module.source = source;

    // Find entry point names
    std::regex vertex_entry_regex(R"(@vertex\s+fn\s+(\w+))");
    std::regex fragment_entry_regex(R"(@fragment\s+fn\s+(\w+))");
    std::regex compute_entry_regex(R"(@compute\s+fn\s+(\w+))");

    std::smatch match;
    if (std::regex_search(source, match, vertex_entry_regex)) {
        vertex_module.entry_point = match[1].str();
        asset.vertex = std::move(vertex_module);
    }

    if (std::regex_search(source, match, fragment_entry_regex)) {
        fragment_module.entry_point = match[1].str();
        asset.fragment = std::move(fragment_module);
    }

    if (std::regex_search(source, match, compute_entry_regex)) {
        ShaderModule compute_module;
        compute_module.name = ctx.name() + "_compute";
        compute_module.stage = ShaderStage::Compute;
        compute_module.language = ShaderLanguage::WGSL;
        compute_module.source = source;
        compute_module.entry_point = match[1].str();

        // Extract workgroup size
        std::regex workgroup_regex(
            R"(@workgroup_size\s*\(\s*(\d+)(?:\s*,\s*(\d+))?(?:\s*,\s*(\d+))?\s*\))");
        if (std::regex_search(source, match, workgroup_regex)) {
            compute_module.workgroup_size[0] =
                static_cast<std::uint32_t>(std::stoi(match[1].str()));
            if (match[2].matched) {
                compute_module.workgroup_size[1] =
                    static_cast<std::uint32_t>(std::stoi(match[2].str()));
            }
            if (match[3].matched) {
                compute_module.workgroup_size[2] =
                    static_cast<std::uint32_t>(std::stoi(match[3].str()));
            }
        }

        asset.compute = std::move(compute_module);
    }

    // Parse bindings
    std::regex binding_regex(
        R"(@group\s*\(\s*(\d+)\s*\)\s*@binding\s*\(\s*(\d+)\s*\)\s*var(?:<[^>]+>)?\s+(\w+))");
    std::sregex_iterator it(source.begin(), source.end(), binding_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        ShaderVariable var;
        var.set = static_cast<std::uint32_t>(std::stoi((*it)[1].str()));
        var.binding = static_cast<std::uint32_t>(std::stoi((*it)[2].str()));
        var.name = (*it)[3].str();
        asset.uniforms.push_back(var);
    }

    return asset;
}

LoadResult<ShaderAsset> ShaderLoader::load_hlsl(LoadContext& ctx) {
    const auto& data = ctx.data();
    std::string source(reinterpret_cast<const char*>(data.data()), data.size());

    ShaderAsset asset;
    asset.name = ctx.name();
    asset.source_path = ctx.path().string();
    asset.language = ShaderLanguage::HLSL;

    // Detect stage from filename
    ShaderStage stage = detect_stage(ctx.path().string());

    ShaderModule module;
    module.name = ctx.name();
    module.stage = stage;
    module.language = ShaderLanguage::HLSL;
    module.source = source;

    // Find entry point - common HLSL conventions
    std::regex vs_entry_regex(R"((\w+)\s*\([^)]*\)\s*:\s*SV_POSITION)");
    std::regex ps_entry_regex(R"((\w+)\s*\([^)]*\)\s*:\s*SV_TARGET)");
    std::regex cs_entry_regex(R"(\[numthreads\s*\([^)]+\)\]\s*void\s+(\w+))");

    std::smatch match;
    if (stage == ShaderStage::Vertex && std::regex_search(source, match, vs_entry_regex)) {
        module.entry_point = match[1].str();
    } else if (stage == ShaderStage::Fragment && std::regex_search(source, match, ps_entry_regex)) {
        module.entry_point = match[1].str();
    } else if (stage == ShaderStage::Compute && std::regex_search(source, match, cs_entry_regex)) {
        module.entry_point = match[1].str();

        // Extract numthreads
        std::regex numthreads_regex(
            R"(\[numthreads\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\])");
        if (std::regex_search(source, match, numthreads_regex)) {
            module.workgroup_size[0] = static_cast<std::uint32_t>(std::stoi(match[1].str()));
            module.workgroup_size[1] = static_cast<std::uint32_t>(std::stoi(match[2].str()));
            module.workgroup_size[2] = static_cast<std::uint32_t>(std::stoi(match[3].str()));
        }
    } else {
        module.entry_point = "main";
    }

    // Parse register bindings
    std::regex register_regex(R"(:\s*register\s*\(\s*([bstu])(\d+)(?:\s*,\s*space(\d+))?\s*\))");
    std::sregex_iterator it(source.begin(), source.end(), register_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        ShaderVariable var;
        char reg_type = (*it)[1].str()[0];
        var.binding = static_cast<std::uint32_t>(std::stoi((*it)[2].str()));
        if ((*it)[3].matched) {
            var.set = static_cast<std::uint32_t>(std::stoi((*it)[3].str()));
        }

        switch (reg_type) {
            case 'b':
                var.type = ShaderDataType::UniformBuffer;
                asset.uniforms.push_back(var);
                break;
            case 't':
                var.type = ShaderDataType::Sampler2D;
                asset.samplers.push_back(var);
                break;
            case 's':
                // Sampler state
                break;
            case 'u':
                var.type = ShaderDataType::StorageBuffer;
                asset.uniforms.push_back(var);
                break;
        }
    }

    // Assign to stage
    switch (stage) {
        case ShaderStage::Vertex:
            asset.vertex = std::move(module);
            break;
        case ShaderStage::Fragment:
            asset.fragment = std::move(module);
            break;
        case ShaderStage::Compute:
            asset.compute = std::move(module);
            break;
        case ShaderStage::Geometry:
            asset.geometry = std::move(module);
            break;
        default:
            break;
    }

    return asset;
}

LoadResult<ShaderAsset> ShaderLoader::load_spirv(LoadContext& ctx) {
    const auto& data = ctx.data();

    // Validate SPIR-V magic number
    if (data.size() < 4) {
        return LoadError{LoadErrorCode::InvalidFormat, "SPIR-V file too small"};
    }

    std::uint32_t magic = *reinterpret_cast<const std::uint32_t*>(data.data());
    if (magic != 0x07230203) {
        return LoadError{LoadErrorCode::InvalidFormat, "Invalid SPIR-V magic number"};
    }

    ShaderAsset asset;
    asset.name = ctx.name();
    asset.source_path = ctx.path().string();
    asset.language = ShaderLanguage::SPIRV;

    // Detect stage from filename
    ShaderStage stage = detect_stage(ctx.path().string());

    ShaderModule module;
    module.name = ctx.name();
    module.stage = stage;
    module.language = ShaderLanguage::SPIRV;
    module.entry_point = "main";

    // Copy SPIR-V binary
    module.spirv.resize(data.size() / sizeof(std::uint32_t));
    std::memcpy(module.spirv.data(), data.data(), data.size());

    // For full reflection, we would use SPIRV-Cross here
    // This is a minimal implementation that just stores the binary

    // Assign to stage
    switch (stage) {
        case ShaderStage::Vertex:
            asset.vertex = std::move(module);
            break;
        case ShaderStage::Fragment:
            asset.fragment = std::move(module);
            break;
        case ShaderStage::Compute:
            asset.compute = std::move(module);
            break;
        case ShaderStage::Geometry:
            asset.geometry = std::move(module);
            break;
        case ShaderStage::TessControl:
            asset.tess_control = std::move(module);
            break;
        case ShaderStage::TessEvaluation:
            asset.tess_evaluation = std::move(module);
            break;
        default:
            break;
    }

    return asset;
}

} // namespace void_asset
