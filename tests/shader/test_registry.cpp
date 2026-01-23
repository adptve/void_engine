/// @file test_registry.cpp
/// @brief Tests for void_shader registry

#include <catch2/catch_test_macros.hpp>
#include <void_engine/shader/registry.hpp>
#include <string>

using namespace void_shader;

// =============================================================================
// ShaderEntry Tests
// =============================================================================

TEST_CASE("ShaderEntry: default", "[shader][registry]") {
    ShaderEntry entry;
    REQUIRE(entry.source.is_empty());
    REQUIRE(entry.compiled.empty());
    REQUIRE(entry.version.is_valid());  // Starts at initial version
}

TEST_CASE("ShaderEntry: construct with source", "[shader][registry]") {
    auto source = ShaderSource::glsl_vertex("test", "void main() {}");
    ShaderId id("test");
    ShaderEntry entry(id, source);

    REQUIRE(entry.source.code == "void main() {}");
    REQUIRE(entry.source.stage == ShaderStage::Vertex);
    REQUIRE(entry.name == "test");
}

TEST_CASE("ShaderEntry: has_target", "[shader][registry]") {
    ShaderEntry entry;

    REQUIRE_FALSE(entry.has_target(CompileTarget::SpirV));

    CompiledShader compiled;
    compiled.target = CompileTarget::SpirV;
    compiled.stage = ShaderStage::Vertex;
    compiled.binary = {0x01, 0x02};

    entry.compiled[CompileTarget::SpirV] = compiled;

    REQUIRE(entry.has_target(CompileTarget::SpirV));
    REQUIRE_FALSE(entry.has_target(CompileTarget::HLSL));
}

TEST_CASE("ShaderEntry: get_compiled", "[shader][registry]") {
    ShaderEntry entry;

    CompiledShader spirv;
    spirv.target = CompileTarget::SpirV;
    spirv.stage = ShaderStage::Vertex;
    spirv.binary = {0x01, 0x02};
    entry.compiled[CompileTarget::SpirV] = spirv;

    CompiledShader glsl;
    glsl.target = CompileTarget::Glsl450;
    glsl.stage = ShaderStage::Vertex;
    glsl.source = "#version 450\nvoid main() {}";
    entry.compiled[CompileTarget::Glsl450] = glsl;

    auto* found_spirv = entry.get_compiled(CompileTarget::SpirV);
    REQUIRE(found_spirv != nullptr);
    REQUIRE(found_spirv->binary.size() == 2);

    auto* found_glsl = entry.get_compiled(CompileTarget::Glsl450);
    REQUIRE(found_glsl != nullptr);
    REQUIRE_FALSE(found_glsl->source.empty());

    auto* missing = entry.get_compiled(CompileTarget::HLSL);
    REQUIRE(missing == nullptr);
}

TEST_CASE("ShaderEntry: update_from_result", "[shader][registry]") {
    ShaderEntry entry;
    auto initial_version = entry.version;

    CompileResult result;
    CompiledShader compiled;
    compiled.target = CompileTarget::SpirV;
    compiled.stage = ShaderStage::Vertex;
    compiled.binary = {0x01};
    result.compiled[CompileTarget::SpirV] = compiled;

    entry.update_from_result(result);

    REQUIRE(entry.compiled.size() == 1);
    REQUIRE(entry.version.value > initial_version.value);
}

// =============================================================================
// ShaderRegistry Tests
// =============================================================================

TEST_CASE("ShaderRegistry: default empty", "[shader][registry]") {
    ShaderRegistry registry;
    REQUIRE(registry.len() == 0);
    REQUIRE(registry.is_empty());
}

TEST_CASE("ShaderRegistry: register_shader", "[shader][registry]") {
    ShaderRegistry registry;

    auto source = ShaderSource::glsl_vertex("test_shader", "void main() {}");
    auto id = registry.register_shader(std::move(source));

    REQUIRE(id.is_ok());
    REQUIRE(registry.len() == 1);
}

TEST_CASE("ShaderRegistry: register_shader fails for duplicate", "[shader][registry]") {
    ShaderRegistry registry;

    auto source1 = ShaderSource::glsl_vertex("shader", "void main() {}");
    auto id1 = registry.register_shader(std::move(source1));
    REQUIRE(id1.is_ok());

    auto source2 = ShaderSource::glsl_fragment("shader", "void main() {}");
    auto id2 = registry.register_shader(std::move(source2));

    REQUIRE_FALSE(id2.is_ok());  // Should fail - duplicate name
    REQUIRE(registry.len() == 1);
}

TEST_CASE("ShaderRegistry: get by id", "[shader][registry]") {
    ShaderRegistry registry;

    auto source = ShaderSource::glsl_vertex("my_shader", "test code");
    auto id = registry.register_shader(std::move(source));
    REQUIRE(id.is_ok());

    const auto* found = registry.get(id.value());
    REQUIRE(found != nullptr);
    REQUIRE(found->source.code == "test code");

    ShaderId missing_id("nonexistent");
    const auto* missing = registry.get(missing_id);
    REQUIRE(missing == nullptr);
}

TEST_CASE("ShaderRegistry: contains", "[shader][registry]") {
    ShaderRegistry registry;

    auto source = ShaderSource::glsl_vertex("shader", "code");
    auto id = registry.register_shader(std::move(source));
    REQUIRE(id.is_ok());

    REQUIRE(registry.contains(id.value()));

    ShaderId missing_id("nonexistent");
    REQUIRE_FALSE(registry.contains(missing_id));
}

TEST_CASE("ShaderRegistry: unregister", "[shader][registry]") {
    ShaderRegistry registry;

    auto source = ShaderSource::glsl_vertex("shader", "code");
    auto id = registry.register_shader(std::move(source));
    REQUIRE(id.is_ok());

    REQUIRE(registry.len() == 1);
    REQUIRE(registry.unregister(id.value()));
    REQUIRE(registry.len() == 0);
    REQUIRE_FALSE(registry.contains(id.value()));
    REQUIRE_FALSE(registry.unregister(id.value()));  // Already removed
}

TEST_CASE("ShaderRegistry: clear", "[shader][registry]") {
    ShaderRegistry registry;

    auto s1 = ShaderSource::glsl_vertex("shader1", "code1");
    auto s2 = ShaderSource::glsl_vertex("shader2", "code2");

    registry.register_shader(std::move(s1));
    registry.register_shader(std::move(s2));

    REQUIRE(registry.len() == 2);

    registry.clear();

    REQUIRE(registry.len() == 0);
    REQUIRE(registry.is_empty());
}

TEST_CASE("ShaderRegistry: get_all_ids", "[shader][registry]") {
    ShaderRegistry registry;

    auto s1 = ShaderSource::glsl_vertex("alpha", "code");
    auto s2 = ShaderSource::glsl_vertex("beta", "code");

    registry.register_shader(std::move(s1));
    registry.register_shader(std::move(s2));

    auto ids = registry.get_all_ids();

    REQUIRE(ids.size() == 2);
}

TEST_CASE("ShaderRegistry: for_each", "[shader][registry]") {
    ShaderRegistry registry;

    auto s1 = ShaderSource::glsl_vertex("a", "code");
    auto s2 = ShaderSource::glsl_vertex("b", "code");

    registry.register_shader(std::move(s1));
    registry.register_shader(std::move(s2));

    std::vector<std::string> visited;
    registry.for_each([&visited](const ShaderId& id, const ShaderEntry&) {
        visited.push_back(id.name());
    });

    REQUIRE(visited.size() == 2);
}

TEST_CASE("ShaderRegistry: get_version", "[shader][registry]") {
    ShaderRegistry registry;

    auto source = ShaderSource::glsl_vertex("shader", "code");
    auto id = registry.register_shader(std::move(source));
    REQUIRE(id.is_ok());

    auto version = registry.get_version(id.value());
    REQUIRE(version.is_valid());
    REQUIRE(version.value == ShaderVersion::INITIAL);
}

TEST_CASE("ShaderRegistry: find_by_path", "[shader][registry]") {
    ShaderRegistry registry;

    auto source = ShaderSource::glsl_vertex("shader", "code");
    source.source_path = "/path/to/shader.vert";

    auto id = registry.register_shader(std::move(source));
    REQUIRE(id.is_ok());

    // Update path mapping
    registry.update_path_mapping(id.value(), "/path/to/shader.vert");

    auto found = registry.find_by_path("/path/to/shader.vert");
    REQUIRE(found.has_value());
    REQUIRE(found->name() == "shader");

    auto missing = registry.find_by_path("/nonexistent");
    REQUIRE_FALSE(missing.has_value());
}

// =============================================================================
// ShaderVariantCollection Tests
// =============================================================================

TEST_CASE("ShaderVariantCollection: basic", "[shader][registry]") {
    auto source = ShaderSource::glsl_vertex("base", "void main() {}");
    ShaderVariantCollection collection(std::move(source));

    REQUIRE(collection.variant_count() == 0);
    REQUIRE(collection.compiled_count() == 0);
}

TEST_CASE("ShaderVariantCollection: add_variant", "[shader][registry]") {
    auto source = ShaderSource::glsl_vertex("base", "void main() {}");
    ShaderVariantCollection collection(std::move(source));

    ShaderVariant variant("lit");
    variant.with_define("ENABLE_LIGHTING");

    collection.add_variant(variant);

    REQUIRE(collection.variant_count() == 1);
}

TEST_CASE("ShaderVariantCollection: build_variants", "[shader][registry]") {
    auto source = ShaderSource::glsl_vertex("base", "void main() {}");
    ShaderVariantCollection collection(std::move(source));

    VariantBuilder builder("base");
    builder.with_feature("FEATURE_A")
           .with_feature("FEATURE_B");

    collection.build_variants(builder);

    // 2^2 = 4 variants
    REQUIRE(collection.variant_count() == 4);
}

TEST_CASE("ShaderVariantCollection: variant_names", "[shader][registry]") {
    auto source = ShaderSource::glsl_vertex("base", "void main() {}");
    ShaderVariantCollection collection(std::move(source));

    ShaderVariant v1("normal");
    ShaderVariant v2("lit");
    v2.with_define("LIGHTING");

    collection.add_variant(v1);
    collection.add_variant(v2);

    auto names = collection.variant_names();

    REQUIRE(names.size() == 2);
    REQUIRE(std::find(names.begin(), names.end(), "normal") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "lit") != names.end());
}
