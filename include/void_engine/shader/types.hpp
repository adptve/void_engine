#pragma once

/// @file types.hpp
/// @brief Shader type definitions for void_shader

#include "fwd.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/core/id.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>

namespace void_shader {

// =============================================================================
// ShaderStage
// =============================================================================

/// Shader stage/type
enum class ShaderStage : std::uint8_t {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation,
};

/// Get shader stage name
[[nodiscard]] inline const char* shader_stage_name(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return "Vertex";
        case ShaderStage::Fragment: return "Fragment";
        case ShaderStage::Compute: return "Compute";
        case ShaderStage::Geometry: return "Geometry";
        case ShaderStage::TessControl: return "TessControl";
        case ShaderStage::TessEvaluation: return "TessEvaluation";
        default: return "Unknown";
    }
}

/// Get shader stage file extension
[[nodiscard]] inline const char* shader_stage_extension(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return ".vert";
        case ShaderStage::Fragment: return ".frag";
        case ShaderStage::Compute: return ".comp";
        case ShaderStage::Geometry: return ".geom";
        case ShaderStage::TessControl: return ".tesc";
        case ShaderStage::TessEvaluation: return ".tese";
        default: return "";
    }
}

// =============================================================================
// CompileTarget
// =============================================================================

/// Compilation target backend
enum class CompileTarget : std::uint8_t {
    SpirV,       // SPIR-V (Vulkan)
    WGSL,        // WGSL (WebGPU)
    GlslEs300,   // GLSL ES 300 (WebGL)
    GlslEs310,   // GLSL ES 310
    Glsl330,     // GLSL 330 Desktop
    Glsl450,     // GLSL 450 Desktop
    HLSL,        // HLSL (D3D11/D3D12)
    MSL,         // Metal Shading Language
};

/// Get compile target name
[[nodiscard]] inline const char* compile_target_name(CompileTarget target) {
    switch (target) {
        case CompileTarget::SpirV: return "SPIR-V";
        case CompileTarget::WGSL: return "WGSL";
        case CompileTarget::GlslEs300: return "GLSL ES 300";
        case CompileTarget::GlslEs310: return "GLSL ES 310";
        case CompileTarget::Glsl330: return "GLSL 330";
        case CompileTarget::Glsl450: return "GLSL 450";
        case CompileTarget::HLSL: return "HLSL";
        case CompileTarget::MSL: return "MSL";
        default: return "Unknown";
    }
}

/// Check if target produces binary output
[[nodiscard]] inline bool is_binary_target(CompileTarget target) {
    return target == CompileTarget::SpirV;
}

// =============================================================================
// ShaderId
// =============================================================================

/// Shader identifier
struct ShaderId {
    void_core::NamedId id;

    /// Default constructor
    ShaderId() = default;

    /// Construct from name
    explicit ShaderId(const std::string& name) : id(name) {}
    explicit ShaderId(const char* name) : id(name) {}

    /// Get name
    [[nodiscard]] const std::string& name() const noexcept { return id.name; }

    /// Get hash
    [[nodiscard]] std::uint64_t hash() const noexcept { return id.hash; }

    /// Comparison
    bool operator==(const ShaderId& other) const noexcept { return id == other.id; }
    bool operator!=(const ShaderId& other) const noexcept { return id != other.id; }
    bool operator<(const ShaderId& other) const noexcept { return id < other.id; }

    /// String conversion
    [[nodiscard]] explicit operator std::string() const { return id.name; }
};

// =============================================================================
// ShaderVersion
// =============================================================================

/// Shader version for tracking changes
struct ShaderVersion {
    std::uint32_t value = 0;

    /// Initial version
    static constexpr std::uint32_t INITIAL = 1;

    /// Default constructor (uninitialized)
    constexpr ShaderVersion() noexcept = default;

    /// Construct with value
    explicit constexpr ShaderVersion(std::uint32_t v) noexcept : value(v) {}

    /// Create initial version
    [[nodiscard]] static constexpr ShaderVersion initial() noexcept {
        return ShaderVersion{INITIAL};
    }

    /// Increment version
    [[nodiscard]] constexpr ShaderVersion next() const noexcept {
        return ShaderVersion{value + 1};
    }

    /// Check if valid (non-zero)
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value > 0;
    }

    /// Comparison
    constexpr bool operator==(const ShaderVersion&) const noexcept = default;
    constexpr auto operator<=>(const ShaderVersion&) const noexcept = default;
};

// =============================================================================
// ShaderDefine
// =============================================================================

/// Preprocessor define for shader variants
struct ShaderDefine {
    std::string name;
    std::optional<std::string> value;

    /// Construct name-only define
    explicit ShaderDefine(std::string n) : name(std::move(n)) {}

    /// Construct valued define
    ShaderDefine(std::string n, std::string v)
        : name(std::move(n)), value(std::move(v)) {}

    /// Generate preprocessor directive
    [[nodiscard]] std::string to_directive() const {
        if (value.has_value()) {
            return "#define " + name + " " + *value;
        }
        return "#define " + name;
    }

    /// Comparison by name
    bool operator==(const ShaderDefine& other) const noexcept {
        return name == other.name;
    }

    bool operator<(const ShaderDefine& other) const noexcept {
        return name < other.name;
    }
};

// =============================================================================
// ShaderVariant
// =============================================================================

/// Shader variant with specific defines
struct ShaderVariant {
    std::string name;
    std::vector<ShaderDefine> defines;

    /// Default constructor
    ShaderVariant() = default;

    /// Construct with name
    explicit ShaderVariant(std::string n) : name(std::move(n)) {}

    /// Add define
    ShaderVariant& with_define(ShaderDefine def) {
        defines.push_back(std::move(def));
        return *this;
    }

    /// Add define by name
    ShaderVariant& with_define(const std::string& def_name) {
        defines.emplace_back(def_name);
        return *this;
    }

    /// Add valued define
    ShaderVariant& with_define(const std::string& def_name, const std::string& value) {
        defines.emplace_back(def_name, value);
        return *this;
    }

    /// Generate preprocessor header
    [[nodiscard]] std::string to_header() const {
        std::string result;
        for (const auto& def : defines) {
            result += def.to_directive() + "\n";
        }
        return result;
    }

    /// Check if has define
    [[nodiscard]] bool has_define(const std::string& name) const {
        for (const auto& def : defines) {
            if (def.name == name) return true;
        }
        return false;
    }
};

// =============================================================================
// CompiledShader
// =============================================================================

/// Compiled shader bytecode or source
struct CompiledShader {
    CompileTarget target;
    ShaderStage stage;
    std::vector<std::uint8_t> binary;   // For SPIR-V
    std::string source;                  // For text formats
    std::string entry_point;

    /// Default constructor
    CompiledShader() : target(CompileTarget::SpirV), stage(ShaderStage::Vertex) {}

    /// Construct binary shader
    CompiledShader(CompileTarget t, ShaderStage s, std::vector<std::uint8_t> bin, std::string entry)
        : target(t), stage(s), binary(std::move(bin)), entry_point(std::move(entry)) {}

    /// Construct source shader
    CompiledShader(CompileTarget t, ShaderStage s, std::string src, std::string entry)
        : target(t), stage(s), source(std::move(src)), entry_point(std::move(entry)) {}

    /// Check if binary format
    [[nodiscard]] bool is_binary() const noexcept {
        return is_binary_target(target);
    }

    /// Get size in bytes
    [[nodiscard]] std::size_t size() const noexcept {
        return is_binary() ? binary.size() : source.size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const noexcept {
        return binary.empty() && source.empty();
    }

    /// Get SPIR-V words (assumes SPIR-V binary)
    [[nodiscard]] const std::uint32_t* spirv_data() const {
        return reinterpret_cast<const std::uint32_t*>(binary.data());
    }

    /// Get SPIR-V word count
    [[nodiscard]] std::size_t spirv_word_count() const {
        return binary.size() / sizeof(std::uint32_t);
    }
};

// =============================================================================
// ShaderMetadata
// =============================================================================

/// Metadata about a shader
struct ShaderMetadata {
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    std::uint32_t reload_count = 0;
    std::vector<std::string> tags;
    std::string source_path;

    /// Default constructor
    ShaderMetadata() : created_at(std::chrono::system_clock::now()), updated_at(created_at) {}

    /// Mark as updated
    void mark_updated() {
        updated_at = std::chrono::system_clock::now();
        reload_count++;
    }

    /// Add tag
    ShaderMetadata& add_tag(std::string tag) {
        tags.push_back(std::move(tag));
        return *this;
    }

    /// Check if has tag
    [[nodiscard]] bool has_tag(const std::string& tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }
};

// =============================================================================
// ShaderError
// =============================================================================

/// Shader-specific errors
struct ShaderError {
    /// File read error
    [[nodiscard]] static void_core::Error file_read(const std::string& path, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::IOError,
            "Failed to read shader file '" + path + "': " + reason);
    }

    /// Parse error
    [[nodiscard]] static void_core::Error parse_error(const std::string& shader_name, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::ParseError,
            "Failed to parse shader '" + shader_name + "': " + reason);
    }

    /// Compilation error
    [[nodiscard]] static void_core::Error compile_error(const std::string& shader_name, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::CompileError,
            "Failed to compile shader '" + shader_name + "': " + reason);
    }

    /// Compilation failed (alias)
    [[nodiscard]] static void_core::Error compile_failed(const std::string& shader_name, const std::string& reason) {
        return compile_error(shader_name, reason);
    }

    /// Validation error
    [[nodiscard]] static void_core::Error validation_error(const std::string& shader_name, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::ValidationError,
            "Shader validation failed for '" + shader_name + "': " + reason);
    }

    /// Not found
    [[nodiscard]] static void_core::Error not_found(const std::string& shader_name) {
        return void_core::Error(void_core::ErrorCode::NotFound,
            "Shader not found: " + shader_name);
    }

    /// No rollback available
    [[nodiscard]] static void_core::Error no_rollback(const std::string& shader_name) {
        return void_core::Error(void_core::ErrorCode::InvalidState,
            "No rollback history available for shader: " + shader_name);
    }

    /// Unsupported target
    [[nodiscard]] static void_core::Error unsupported_target(const std::string& target) {
        return void_core::Error(void_core::ErrorCode::NotSupported,
            "Unsupported compile target: " + target);
    }

    /// Include failed
    [[nodiscard]] static void_core::Error include_failed(const std::string& include_path, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::DependencyMissing,
            "Failed to include '" + include_path + "': " + reason);
    }
};

} // namespace void_shader

/// Hash specialization
template<>
struct std::hash<void_shader::ShaderId> {
    std::size_t operator()(const void_shader::ShaderId& id) const noexcept {
        return static_cast<std::size_t>(id.hash());
    }
};
