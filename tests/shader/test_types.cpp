/// @file test_types.cpp
/// @brief Tests for void_shader types

#include <catch2/catch_test_macros.hpp>
#include <void_engine/shader/types.hpp>
#include <string>

using namespace void_shader;

// =============================================================================
// ShaderStage Tests
// =============================================================================

TEST_CASE("ShaderStage: names are correct", "[shader][types]") {
    REQUIRE(std::string(shader_stage_name(ShaderStage::Vertex)) == "Vertex");
    REQUIRE(std::string(shader_stage_name(ShaderStage::Fragment)) == "Fragment");
    REQUIRE(std::string(shader_stage_name(ShaderStage::Compute)) == "Compute");
    REQUIRE(std::string(shader_stage_name(ShaderStage::Geometry)) == "Geometry");
    REQUIRE(std::string(shader_stage_name(ShaderStage::TessControl)) == "TessControl");
    REQUIRE(std::string(shader_stage_name(ShaderStage::TessEvaluation)) == "TessEvaluation");
}

TEST_CASE("ShaderStage: extensions are correct", "[shader][types]") {
    REQUIRE(std::string(shader_stage_extension(ShaderStage::Vertex)) == ".vert");
    REQUIRE(std::string(shader_stage_extension(ShaderStage::Fragment)) == ".frag");
    REQUIRE(std::string(shader_stage_extension(ShaderStage::Compute)) == ".comp");
    REQUIRE(std::string(shader_stage_extension(ShaderStage::Geometry)) == ".geom");
    REQUIRE(std::string(shader_stage_extension(ShaderStage::TessControl)) == ".tesc");
    REQUIRE(std::string(shader_stage_extension(ShaderStage::TessEvaluation)) == ".tese");
}

// =============================================================================
// CompileTarget Tests
// =============================================================================

TEST_CASE("CompileTarget: names are correct", "[shader][types]") {
    REQUIRE(std::string(compile_target_name(CompileTarget::SpirV)) == "SPIR-V");
    REQUIRE(std::string(compile_target_name(CompileTarget::Glsl450)) == "GLSL 450");
    REQUIRE(std::string(compile_target_name(CompileTarget::HLSL)) == "HLSL");
    REQUIRE(std::string(compile_target_name(CompileTarget::MSL)) == "MSL");
    REQUIRE(std::string(compile_target_name(CompileTarget::WGSL)) == "WGSL");
}

TEST_CASE("CompileTarget: is_binary_target", "[shader][types]") {
    REQUIRE(is_binary_target(CompileTarget::SpirV));
    REQUIRE_FALSE(is_binary_target(CompileTarget::Glsl450));
    REQUIRE_FALSE(is_binary_target(CompileTarget::HLSL));
    REQUIRE_FALSE(is_binary_target(CompileTarget::MSL));
    REQUIRE_FALSE(is_binary_target(CompileTarget::WGSL));
}

// =============================================================================
// ShaderId Tests
// =============================================================================

TEST_CASE("ShaderId: default", "[shader][types]") {
    ShaderId id;
    REQUIRE(id.name().empty());
}

TEST_CASE("ShaderId: construction from name", "[shader][types]") {
    ShaderId id("my_shader");
    REQUIRE(id.name() == "my_shader");
    REQUIRE(id.hash() != 0);  // Hash should be computed
}

TEST_CASE("ShaderId: comparison", "[shader][types]") {
    ShaderId a("shader_a");
    ShaderId b("shader_b");
    ShaderId c("shader_a");

    REQUIRE(a == c);
    REQUIRE(a != b);
}

TEST_CASE("ShaderId: hash works for unordered_map", "[shader][types]") {
    ShaderId id("test");
    std::hash<ShaderId> hasher;
    REQUIRE(hasher(id) == static_cast<std::size_t>(id.hash()));
}

// =============================================================================
// ShaderVersion Tests
// =============================================================================

TEST_CASE("ShaderVersion: default", "[shader][types]") {
    ShaderVersion v;
    REQUIRE(v.value == 0);
    REQUIRE_FALSE(v.is_valid());
}

TEST_CASE("ShaderVersion: initial", "[shader][types]") {
    auto v = ShaderVersion::initial();
    REQUIRE(v.value == ShaderVersion::INITIAL);
    REQUIRE(v.is_valid());
}

TEST_CASE("ShaderVersion: next increments", "[shader][types]") {
    auto v = ShaderVersion::initial();
    auto next = v.next();
    REQUIRE(next.value == v.value + 1);
}

TEST_CASE("ShaderVersion: comparison", "[shader][types]") {
    ShaderVersion v1{1};
    ShaderVersion v2{2};
    ShaderVersion v3{1};

    REQUIRE(v1 == v3);
    REQUIRE(v1 != v2);
    REQUIRE(v1 < v2);
}

// =============================================================================
// ShaderDefine Tests
// =============================================================================

TEST_CASE("ShaderDefine: name only", "[shader][types]") {
    ShaderDefine def("ENABLE_SHADOWS");
    REQUIRE(def.name == "ENABLE_SHADOWS");
    REQUIRE_FALSE(def.value.has_value());
}

TEST_CASE("ShaderDefine: with value", "[shader][types]") {
    ShaderDefine def("MAX_LIGHTS", "16");
    REQUIRE(def.name == "MAX_LIGHTS");
    REQUIRE(def.value.has_value());
    REQUIRE(*def.value == "16");
}

TEST_CASE("ShaderDefine: to_directive", "[shader][types]") {
    ShaderDefine flag_def("FLAG");
    REQUIRE(flag_def.to_directive() == "#define FLAG");

    ShaderDefine valued_def("COUNT", "10");
    REQUIRE(valued_def.to_directive() == "#define COUNT 10");
}

TEST_CASE("ShaderDefine: comparison by name", "[shader][types]") {
    ShaderDefine a("A");
    ShaderDefine b("B");
    ShaderDefine a2("A", "value");

    REQUIRE(a == a2);  // Same name, even with different value
    REQUIRE(a != b);
}

// =============================================================================
// ShaderVariant Tests
// =============================================================================

TEST_CASE("ShaderVariant: default", "[shader][types]") {
    ShaderVariant v;
    REQUIRE(v.name.empty());
    REQUIRE(v.defines.empty());
}

TEST_CASE("ShaderVariant: construction with name", "[shader][types]") {
    ShaderVariant v("lit_variant");
    REQUIRE(v.name == "lit_variant");
}

TEST_CASE("ShaderVariant: builder pattern", "[shader][types]") {
    ShaderVariant v("pbr");
    v.with_define("ENABLE_PBR")
     .with_define("MAX_LIGHTS", "8");

    REQUIRE(v.defines.size() == 2);
}

TEST_CASE("ShaderVariant: to_header", "[shader][types]") {
    ShaderVariant v;
    v.with_define("A")
     .with_define("B", "2");

    auto header = v.to_header();
    REQUIRE(header.find("#define A") != std::string::npos);
    REQUIRE(header.find("#define B 2") != std::string::npos);
}

TEST_CASE("ShaderVariant: has_define", "[shader][types]") {
    ShaderVariant v;
    v.with_define("SHADOWS");

    REQUIRE(v.has_define("SHADOWS"));
    REQUIRE_FALSE(v.has_define("BLOOM"));
}

// =============================================================================
// CompiledShader Tests
// =============================================================================

TEST_CASE("CompiledShader: default", "[shader][types]") {
    CompiledShader shader;
    REQUIRE(shader.binary.empty());
    REQUIRE(shader.source.empty());
    REQUIRE(shader.is_empty());
}

TEST_CASE("CompiledShader: binary shader", "[shader][types]") {
    CompiledShader shader(
        CompileTarget::SpirV,
        ShaderStage::Vertex,
        std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04},
        "main"
    );

    REQUIRE(shader.is_binary());
    REQUIRE(shader.size() == 4);
    REQUIRE_FALSE(shader.is_empty());
    REQUIRE(shader.spirv_word_count() == 1);
}

TEST_CASE("CompiledShader: source shader", "[shader][types]") {
    CompiledShader shader(
        CompileTarget::Glsl450,
        ShaderStage::Fragment,
        std::string("#version 450\nvoid main() {}"),
        "main"
    );

    REQUIRE_FALSE(shader.is_binary());
    REQUIRE(shader.size() > 0);
    REQUIRE_FALSE(shader.is_empty());
}

// =============================================================================
// ShaderMetadata Tests
// =============================================================================

TEST_CASE("ShaderMetadata: default", "[shader][types]") {
    ShaderMetadata meta;
    REQUIRE(meta.reload_count == 0);
    REQUIRE(meta.tags.empty());
    REQUIRE(meta.source_path.empty());
}

TEST_CASE("ShaderMetadata: mark_updated", "[shader][types]") {
    ShaderMetadata meta;
    auto initial_time = meta.updated_at;

    meta.mark_updated();

    REQUIRE(meta.reload_count == 1);
    REQUIRE(meta.updated_at >= initial_time);
}

TEST_CASE("ShaderMetadata: add_tag", "[shader][types]") {
    ShaderMetadata meta;
    meta.add_tag("pbr").add_tag("deferred");

    REQUIRE(meta.tags.size() == 2);
    REQUIRE(meta.has_tag("pbr"));
    REQUIRE(meta.has_tag("deferred"));
    REQUIRE_FALSE(meta.has_tag("forward"));
}

// =============================================================================
// ShaderError Tests
// =============================================================================

TEST_CASE("ShaderError: file_read", "[shader][types]") {
    auto error = ShaderError::file_read("test.glsl", "File not found");
    REQUIRE(error.code() == void_core::ErrorCode::IOError);
    REQUIRE(error.message().find("test.glsl") != std::string::npos);
}

TEST_CASE("ShaderError: parse_error", "[shader][types]") {
    auto error = ShaderError::parse_error("shader", "Syntax error");
    REQUIRE(error.code() == void_core::ErrorCode::ParseError);
}

TEST_CASE("ShaderError: compile_error", "[shader][types]") {
    auto error = ShaderError::compile_error("test.glsl", "Undefined symbol");
    REQUIRE(error.code() == void_core::ErrorCode::CompileError);
}

TEST_CASE("ShaderError: validation_error", "[shader][types]") {
    auto error = ShaderError::validation_error("shader", "Missing uniform");
    REQUIRE(error.code() == void_core::ErrorCode::ValidationError);
}

TEST_CASE("ShaderError: not_found", "[shader][types]") {
    auto error = ShaderError::not_found("missing.glsl");
    REQUIRE(error.code() == void_core::ErrorCode::NotFound);
}

TEST_CASE("ShaderError: no_rollback", "[shader][types]") {
    auto error = ShaderError::no_rollback("shader");
    REQUIRE(error.code() == void_core::ErrorCode::InvalidState);
}

TEST_CASE("ShaderError: unsupported_target", "[shader][types]") {
    auto error = ShaderError::unsupported_target("WebGPU");
    REQUIRE(error.code() == void_core::ErrorCode::NotSupported);
}

TEST_CASE("ShaderError: include_failed", "[shader][types]") {
    auto error = ShaderError::include_failed("common.glsl", "File not found");
    REQUIRE(error.code() == void_core::ErrorCode::DependencyMissing);
}
