/// @file test_source.cpp
/// @brief Tests for void_shader source handling

#include <catch2/catch_test_macros.hpp>
#include <void_engine/shader/source.hpp>
#include <string>

using namespace void_shader;

// =============================================================================
// SourceLanguage Tests
// =============================================================================

TEST_CASE("SourceLanguage: names are correct", "[shader][source]") {
    REQUIRE(std::string(source_language_name(SourceLanguage::Glsl)) == "GLSL");
    REQUIRE(std::string(source_language_name(SourceLanguage::Hlsl)) == "HLSL");
    REQUIRE(std::string(source_language_name(SourceLanguage::Wgsl)) == "WGSL");
    REQUIRE(std::string(source_language_name(SourceLanguage::SpirV)) == "SPIR-V");
}

TEST_CASE("SourceLanguage: detect_language", "[shader][source]") {
    // GLSL extensions
    REQUIRE(detect_language("test.vert") == SourceLanguage::Glsl);
    REQUIRE(detect_language("test.frag") == SourceLanguage::Glsl);
    REQUIRE(detect_language("test.comp") == SourceLanguage::Glsl);
    REQUIRE(detect_language("test.glsl") == SourceLanguage::Glsl);
    REQUIRE(detect_language("test.geom") == SourceLanguage::Glsl);
    REQUIRE(detect_language("test.tesc") == SourceLanguage::Glsl);
    REQUIRE(detect_language("test.tese") == SourceLanguage::Glsl);

    // HLSL extensions
    REQUIRE(detect_language("test.hlsl") == SourceLanguage::Hlsl);
    REQUIRE(detect_language("test.fx") == SourceLanguage::Hlsl);

    // WGSL extension
    REQUIRE(detect_language("test.wgsl") == SourceLanguage::Wgsl);

    // SPIRV extension
    REQUIRE(detect_language("test.spv") == SourceLanguage::SpirV);
    REQUIRE(detect_language("test.spirv") == SourceLanguage::SpirV);
}

TEST_CASE("SourceLanguage: detect_stage", "[shader][source]") {
    REQUIRE(detect_stage("test.vert") == ShaderStage::Vertex);
    REQUIRE(detect_stage("test.frag") == ShaderStage::Fragment);
    REQUIRE(detect_stage("test.comp") == ShaderStage::Compute);
    REQUIRE(detect_stage("test.geom") == ShaderStage::Geometry);
    REQUIRE(detect_stage("test.tesc") == ShaderStage::TessControl);
    REQUIRE(detect_stage("test.tese") == ShaderStage::TessEvaluation);

    // Stem-based detection
    REQUIRE(detect_stage("shader_vs.glsl") == ShaderStage::Vertex);
    REQUIRE(detect_stage("shader_fs.glsl") == ShaderStage::Fragment);
    REQUIRE(detect_stage("shader_ps.glsl") == ShaderStage::Fragment);
    REQUIRE(detect_stage("shader_cs.glsl") == ShaderStage::Compute);
    REQUIRE(detect_stage("shader_gs.glsl") == ShaderStage::Geometry);

    // Unknown should return nullopt
    REQUIRE_FALSE(detect_stage("test.wgsl").has_value());
}

// =============================================================================
// SourceDefine Tests
// =============================================================================

TEST_CASE("SourceDefine: default", "[shader][source]") {
    SourceDefine def;
    REQUIRE(def.name.empty());
    REQUIRE(def.value.empty());
}

TEST_CASE("SourceDefine: name only", "[shader][source]") {
    SourceDefine def("FEATURE_FLAG");
    REQUIRE(def.name == "FEATURE_FLAG");
    REQUIRE(def.value.empty());
}

TEST_CASE("SourceDefine: with value", "[shader][source]") {
    SourceDefine def("MAX_COUNT", "16");
    REQUIRE(def.name == "MAX_COUNT");
    REQUIRE(def.value == "16");
}

// =============================================================================
// ShaderSource Tests
// =============================================================================

TEST_CASE("ShaderSource: default", "[shader][source]") {
    ShaderSource source;
    REQUIRE(source.code.empty());
    REQUIRE(source.language == SourceLanguage::Glsl);
    REQUIRE(source.entry_point == "main");
    REQUIRE(source.is_empty());
}

TEST_CASE("ShaderSource: construct with code", "[shader][source]") {
    ShaderSource source("test", "void main() {}", SourceLanguage::Glsl);
    REQUIRE(source.name == "test");
    REQUIRE(source.code == "void main() {}");
    REQUIRE(source.language == SourceLanguage::Glsl);
}

TEST_CASE("ShaderSource: glsl_vertex factory", "[shader][source]") {
    auto source = ShaderSource::glsl_vertex("test", "void main() {}");
    REQUIRE(source.code == "void main() {}");
    REQUIRE(source.language == SourceLanguage::Glsl);
    REQUIRE(source.stage == ShaderStage::Vertex);
}

TEST_CASE("ShaderSource: glsl_fragment factory", "[shader][source]") {
    auto source = ShaderSource::glsl_fragment("test", "void main() {}");
    REQUIRE(source.language == SourceLanguage::Glsl);
    REQUIRE(source.stage == ShaderStage::Fragment);
}

TEST_CASE("ShaderSource: glsl_compute factory", "[shader][source]") {
    auto source = ShaderSource::glsl_compute("test", "void main() {}");
    REQUIRE(source.language == SourceLanguage::Glsl);
    REQUIRE(source.stage == ShaderStage::Compute);
}

TEST_CASE("ShaderSource: wgsl factory", "[shader][source]") {
    auto source = ShaderSource::wgsl("test", "@vertex fn main() {}");
    REQUIRE(source.language == SourceLanguage::Wgsl);
}

TEST_CASE("ShaderSource: from_string", "[shader][source]") {
    auto source = ShaderSource::from_string(
        "my_shader",
        "void main() {}",
        SourceLanguage::Glsl,
        ShaderStage::Vertex
    );

    REQUIRE(source.name == "my_shader");
    REQUIRE(source.code == "void main() {}");
    REQUIRE(source.stage == ShaderStage::Vertex);
}

TEST_CASE("ShaderSource: is_empty", "[shader][source]") {
    ShaderSource empty;
    REQUIRE(empty.is_empty());

    ShaderSource with_code;
    with_code.code = "void main() {}";
    REQUIRE_FALSE(with_code.is_empty());
}

TEST_CASE("ShaderSource: with_variant", "[shader][source]") {
    ShaderSource source;
    source.code = "void main() {}";

    ShaderVariant variant;
    variant.with_define("FEATURE_A");

    auto modified = source.with_variant(variant);
    REQUIRE(modified.find("#define FEATURE_A") != std::string::npos);
    REQUIRE(modified.find("void main() {}") != std::string::npos);
}

TEST_CASE("ShaderSource: defines", "[shader][source]") {
    ShaderSource source;
    source.defines.push_back(SourceDefine("SHADOWS"));
    source.defines.push_back(SourceDefine("MAX_LIGHTS", "8"));

    REQUIRE(source.defines.size() == 2);
    REQUIRE(source.defines[0].name == "SHADOWS");
    REQUIRE(source.defines[1].value == "8");
}

// =============================================================================
// ShaderIncludeResolver Tests
// =============================================================================

TEST_CASE("ShaderIncludeResolver: default", "[shader][source]") {
    ShaderIncludeResolver resolver;
    // By default, resolve fails for any path (no include paths configured)
    auto result = resolver.resolve("any/path.glsl");
    REQUIRE(result.is_ok());  // Returns the source as-is when no includes
}

TEST_CASE("ShaderIncludeResolver: add_include_path", "[shader][source]") {
    ShaderIncludeResolver resolver;
    resolver.add_include_path("shaders/common");

    // This test would need actual files to work properly
    // Just verify the method doesn't crash
}

TEST_CASE("ShaderIncludeResolver: resolve with no includes", "[shader][source]") {
    ShaderIncludeResolver resolver;
    std::string source = "void main() {}";

    auto result = resolver.resolve(source);
    REQUIRE(result.is_ok());
    REQUIRE(result.value() == source + "\n");
}

// =============================================================================
// VariantBuilder Tests
// =============================================================================

TEST_CASE("VariantBuilder: basic construction", "[shader][source]") {
    VariantBuilder builder("base");
    REQUIRE(builder.variant_count() == 1);  // Always at least 1 (base variant)
}

TEST_CASE("VariantBuilder: with_feature", "[shader][source]") {
    VariantBuilder builder("base");
    builder.with_feature("FEATURE_A")
           .with_feature("FEATURE_B");

    // 2^2 = 4 variants
    REQUIRE(builder.variant_count() == 4);
}

TEST_CASE("VariantBuilder: with_define", "[shader][source]") {
    VariantBuilder builder("base");
    builder.with_define("COUNT", "10");

    auto variants = builder.build();
    REQUIRE(variants.size() == 1);
}

TEST_CASE("VariantBuilder: build generates all permutations", "[shader][source]") {
    VariantBuilder builder("shader");
    builder.with_feature("A")
           .with_feature("B");

    auto variants = builder.build();

    REQUIRE(variants.size() == 4);
    // Variants should be: base, A, B, A+B
}

TEST_CASE("VariantBuilder: variant names include features", "[shader][source]") {
    VariantBuilder builder("shader");
    builder.with_feature("FEATURE_A");

    auto variants = builder.build();

    REQUIRE(variants.size() == 2);
    // Check that variant names are generated
    REQUIRE_FALSE(variants[0].name.empty());
}
