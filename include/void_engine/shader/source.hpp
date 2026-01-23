#pragma once

/// @file source.hpp
/// @brief Shader source handling for void_shader

#include "fwd.hpp"
#include "types.hpp"
#include <void_engine/core/error.hpp>
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

namespace void_shader {

// =============================================================================
// SourceLanguage
// =============================================================================

/// Shader source language
enum class SourceLanguage : std::uint8_t {
    Wgsl,    // WebGPU Shading Language
    Glsl,    // OpenGL Shading Language
    Hlsl,    // High Level Shading Language
    SpirV,   // Pre-compiled SPIR-V
};

/// Get source language name
[[nodiscard]] inline const char* source_language_name(SourceLanguage lang) {
    switch (lang) {
        case SourceLanguage::Wgsl: return "WGSL";
        case SourceLanguage::Glsl: return "GLSL";
        case SourceLanguage::Hlsl: return "HLSL";
        case SourceLanguage::SpirV: return "SPIR-V";
        default: return "Unknown";
    }
}

/// Detect language from file extension
[[nodiscard]] inline SourceLanguage detect_language(const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".wgsl") return SourceLanguage::Wgsl;
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag" ||
        ext == ".comp" || ext == ".geom" || ext == ".tesc" || ext == ".tese") {
        return SourceLanguage::Glsl;
    }
    if (ext == ".hlsl" || ext == ".fx") return SourceLanguage::Hlsl;
    if (ext == ".spv" || ext == ".spirv") return SourceLanguage::SpirV;

    return SourceLanguage::Glsl;  // Default
}

/// Detect shader stage from file extension
[[nodiscard]] inline std::optional<ShaderStage> detect_stage(const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    std::string stem = p.stem().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);

    // Check extension
    if (ext == ".vert") return ShaderStage::Vertex;
    if (ext == ".frag") return ShaderStage::Fragment;
    if (ext == ".comp") return ShaderStage::Compute;
    if (ext == ".geom") return ShaderStage::Geometry;
    if (ext == ".tesc") return ShaderStage::TessControl;
    if (ext == ".tese") return ShaderStage::TessEvaluation;

    // Check stem suffix
    if (stem.ends_with("_vs") || stem.ends_with(".vs")) return ShaderStage::Vertex;
    if (stem.ends_with("_fs") || stem.ends_with(".fs")) return ShaderStage::Fragment;
    if (stem.ends_with("_ps") || stem.ends_with(".ps")) return ShaderStage::Fragment;
    if (stem.ends_with("_cs") || stem.ends_with(".cs")) return ShaderStage::Compute;
    if (stem.ends_with("_gs") || stem.ends_with(".gs")) return ShaderStage::Geometry;

    return std::nullopt;
}

// =============================================================================
// ShaderSource
// =============================================================================

/// Shader source define
struct SourceDefine {
    std::string name;
    std::string value;

    SourceDefine() = default;
    explicit SourceDefine(std::string n) : name(std::move(n)) {}
    SourceDefine(std::string n, std::string v) : name(std::move(n)), value(std::move(v)) {}
};

/// Shader source code container
struct ShaderSource {
    std::string name;
    std::string code;
    SourceLanguage language = SourceLanguage::Glsl;
    std::optional<ShaderStage> stage;
    std::string source_path;
    std::string entry_point = "main";
    std::vector<SourceDefine> defines;

    /// Default constructor
    ShaderSource() = default;

    /// Construct with code
    ShaderSource(std::string n, std::string c, SourceLanguage lang = SourceLanguage::Glsl)
        : name(std::move(n)), code(std::move(c)), language(lang) {}

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return code.empty();
    }

    /// Get code with variant defines prepended
    [[nodiscard]] std::string with_variant(const ShaderVariant& variant) const {
        return variant.to_header() + code;
    }

    /// Load from file
    [[nodiscard]] static void_core::Result<ShaderSource> from_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return void_core::Err<ShaderSource>(ShaderError::file_read(path, "Cannot open file"));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        std::filesystem::path fs_path(path);
        ShaderSource source;
        source.name = fs_path.stem().string();
        source.code = buffer.str();
        source.language = detect_language(path);
        source.stage = detect_stage(path);
        source.source_path = path;

        return void_core::Ok(std::move(source));
    }

    /// Create from string
    [[nodiscard]] static ShaderSource from_string(
        const std::string& name,
        const std::string& code,
        SourceLanguage lang = SourceLanguage::Glsl,
        std::optional<ShaderStage> stg = std::nullopt)
    {
        ShaderSource source;
        source.name = name;
        source.code = code;
        source.language = lang;
        source.stage = stg;
        return source;
    }

    /// Create WGSL source
    [[nodiscard]] static ShaderSource wgsl(const std::string& name, const std::string& code) {
        return from_string(name, code, SourceLanguage::Wgsl);
    }

    /// Create GLSL vertex shader
    [[nodiscard]] static ShaderSource glsl_vertex(const std::string& name, const std::string& code) {
        return from_string(name, code, SourceLanguage::Glsl, ShaderStage::Vertex);
    }

    /// Create GLSL fragment shader
    [[nodiscard]] static ShaderSource glsl_fragment(const std::string& name, const std::string& code) {
        return from_string(name, code, SourceLanguage::Glsl, ShaderStage::Fragment);
    }

    /// Create GLSL compute shader
    [[nodiscard]] static ShaderSource glsl_compute(const std::string& name, const std::string& code) {
        return from_string(name, code, SourceLanguage::Glsl, ShaderStage::Compute);
    }
};

// =============================================================================
// ShaderIncludeResolver
// =============================================================================

/// Resolves #include directives in shader source
class ShaderIncludeResolver {
public:
    using IncludeCallback = std::function<void_core::Result<std::string>(const std::string&)>;

    /// Constructor with include paths
    explicit ShaderIncludeResolver(std::vector<std::string> include_paths = {})
        : m_include_paths(std::move(include_paths)) {}

    /// Add include path
    void add_include_path(const std::string& path) {
        m_include_paths.push_back(path);
    }

    /// Set custom include callback
    void set_callback(IncludeCallback cb) {
        m_callback = std::move(cb);
    }

    /// Resolve includes in source
    [[nodiscard]] void_core::Result<std::string> resolve(
        const std::string& source,
        const std::string& source_path = "") const
    {
        std::string result;
        std::istringstream stream(source);
        std::string line;
        std::set<std::string> included;  // Prevent recursive includes

        // Get source directory for relative includes
        std::string source_dir;
        if (!source_path.empty()) {
            std::filesystem::path p(source_path);
            source_dir = p.parent_path().string();
        }

        while (std::getline(stream, line)) {
            // Check for #include directive
            std::smatch match;
            std::regex include_regex(R"(^\s*#include\s+[<"]([^>"]+)[>"])");

            if (std::regex_search(line, match, include_regex)) {
                std::string include_file = match[1].str();

                if (included.count(include_file) > 0) {
                    // Already included, skip
                    continue;
                }
                included.insert(include_file);

                // Try to resolve include
                auto include_result = resolve_include(include_file, source_dir);
                if (!include_result) {
                    return void_core::Err<std::string>(include_result.error());
                }

                // Recursively resolve includes in the included file
                auto recursive_result = resolve(include_result.value(), include_file);
                if (!recursive_result) {
                    return recursive_result;
                }

                result += recursive_result.value() + "\n";
            } else {
                result += line + "\n";
            }
        }

        return void_core::Ok(std::move(result));
    }

private:
    [[nodiscard]] void_core::Result<std::string> resolve_include(
        const std::string& include_file,
        const std::string& source_dir) const
    {
        // Try custom callback first
        if (m_callback) {
            return m_callback(include_file);
        }

        // Try relative to source
        if (!source_dir.empty()) {
            std::filesystem::path p = std::filesystem::path(source_dir) / include_file;
            if (std::filesystem::exists(p)) {
                return read_file(p.string());
            }
        }

        // Try include paths
        for (const auto& include_path : m_include_paths) {
            std::filesystem::path p = std::filesystem::path(include_path) / include_file;
            if (std::filesystem::exists(p)) {
                return read_file(p.string());
            }
        }

        return void_core::Err<std::string>(
            ShaderError::file_read(include_file, "Include file not found"));
    }

    [[nodiscard]] static void_core::Result<std::string> read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return void_core::Err<std::string>(ShaderError::file_read(path, "Cannot open file"));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return void_core::Ok(buffer.str());
    }

    std::vector<std::string> m_include_paths;
    IncludeCallback m_callback;
};

// =============================================================================
// VariantBuilder
// =============================================================================

/// Builder for generating shader variant permutations
class VariantBuilder {
public:
    /// Constructor with base name
    explicit VariantBuilder(std::string base_name) : m_base_name(std::move(base_name)) {}

    /// Add feature flag
    VariantBuilder& with_feature(std::string feature) {
        m_features.push_back(std::move(feature));
        return *this;
    }

    /// Add valued define
    VariantBuilder& with_define(std::string name, std::string value) {
        m_defines.emplace_back(std::move(name), std::move(value));
        return *this;
    }

    /// Build all permutations
    [[nodiscard]] std::vector<ShaderVariant> build() const {
        std::vector<ShaderVariant> variants;

        // Generate 2^n combinations
        std::size_t count = 1u << m_features.size();

        for (std::size_t i = 0; i < count; ++i) {
            ShaderVariant variant(generate_name(i));

            // Add common defines
            for (const auto& def : m_defines) {
                variant.with_define(def.name, *def.value);
            }

            // Add feature defines based on bits
            for (std::size_t j = 0; j < m_features.size(); ++j) {
                if (i & (1u << j)) {
                    variant.with_define(m_features[j]);
                }
            }

            variants.push_back(std::move(variant));
        }

        return variants;
    }

    /// Get variant count
    [[nodiscard]] std::size_t variant_count() const noexcept {
        return 1u << m_features.size();
    }

private:
    [[nodiscard]] std::string generate_name(std::size_t bits) const {
        if (bits == 0) {
            return m_base_name;
        }

        std::string name = m_base_name;
        for (std::size_t j = 0; j < m_features.size(); ++j) {
            if (bits & (1u << j)) {
                // Convert feature name to lowercase suffix
                std::string suffix = m_features[j];
                std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
                name += "_" + suffix;
            }
        }
        return name;
    }

    std::string m_base_name;
    std::vector<std::string> m_features;
    std::vector<ShaderDefine> m_defines;
};

} // namespace void_shader
