/// @file test_compiler.cpp
/// @brief Tests for void_shader compiler system

#include <catch2/catch_test_macros.hpp>
#include <void_engine/shader/compiler.hpp>
#include <string>

using namespace void_shader;

// =============================================================================
// CompilerConfig Tests
// =============================================================================

TEST_CASE("CompilerConfig: defaults", "[shader][compiler]") {
    CompilerConfig config;
    REQUIRE(config.targets.size() == 1);
    REQUIRE(config.targets[0] == CompileTarget::SpirV);
    REQUIRE(config.optimize == true);
    REQUIRE(config.generate_debug_info == false);
    REQUIRE(config.validate == true);
}

TEST_CASE("CompilerConfig: builder pattern", "[shader][compiler]") {
    auto config = CompilerConfig()
        .with_target(CompileTarget::Glsl450)
        .with_optimization(false)
        .with_debug_info(true)
        .with_validation(false);

    REQUIRE(config.targets.size() == 2);  // SpirV + Glsl450
    REQUIRE(config.optimize == false);
    REQUIRE(config.generate_debug_info == true);
    REQUIRE(config.validate == false);
}

TEST_CASE("CompilerConfig: with_include_path", "[shader][compiler]") {
    auto config = CompilerConfig()
        .with_include_path("shaders/common")
        .with_include_path("shaders/lib");

    REQUIRE(config.include_paths.size() == 2);
}

TEST_CASE("CompilerConfig: with_define", "[shader][compiler]") {
    auto config = CompilerConfig()
        .with_define("DEBUG")
        .with_define("MAX_LIGHTS", "16");

    REQUIRE(config.defines.size() == 2);
    REQUIRE(config.defines["DEBUG"].empty());
    REQUIRE(config.defines["MAX_LIGHTS"] == "16");
}

// =============================================================================
// CompileResult Tests
// =============================================================================

TEST_CASE("CompileResult: is_success", "[shader][compiler]") {
    CompileResult result;
    REQUIRE_FALSE(result.is_success());  // No compiled output

    CompiledShader shader;
    shader.binary = {0x01, 0x02, 0x03};
    result.compiled[CompileTarget::SpirV] = shader;

    REQUIRE(result.is_success());
}

TEST_CASE("CompileResult: is_success with errors", "[shader][compiler]") {
    CompileResult result;
    CompiledShader shader;
    shader.binary = {0x01};
    result.compiled[CompileTarget::SpirV] = shader;
    result.errors.push_back("Error");

    REQUIRE_FALSE(result.is_success());  // Has errors
}

TEST_CASE("CompileResult: get target", "[shader][compiler]") {
    CompileResult result;

    CompiledShader spirv;
    spirv.target = CompileTarget::SpirV;
    spirv.binary = {0x01, 0x02};
    result.compiled[CompileTarget::SpirV] = spirv;

    auto* found = result.get(CompileTarget::SpirV);
    REQUIRE(found != nullptr);
    REQUIRE(found->binary.size() == 2);

    auto* missing = result.get(CompileTarget::Glsl450);
    REQUIRE(missing == nullptr);
}

TEST_CASE("CompileResult: has_target", "[shader][compiler]") {
    CompileResult result;
    result.compiled[CompileTarget::SpirV] = CompiledShader{};

    REQUIRE(result.has_target(CompileTarget::SpirV));
    REQUIRE_FALSE(result.has_target(CompileTarget::Glsl450));
}

TEST_CASE("CompileResult: error_message", "[shader][compiler]") {
    CompileResult result;
    REQUIRE(result.error_message().empty());

    result.errors.push_back("Error 1");
    result.errors.push_back("Error 2");

    auto msg = result.error_message();
    REQUIRE(msg.find("Error 1") != std::string::npos);
    REQUIRE(msg.find("Error 2") != std::string::npos);
}

TEST_CASE("CompileResult: warning_message", "[shader][compiler]") {
    CompileResult result;
    REQUIRE(result.warning_message().empty());

    result.warnings.push_back("Warning 1");

    auto msg = result.warning_message();
    REQUIRE(msg.find("Warning 1") != std::string::npos);
}

// =============================================================================
// ValidationRule Tests
// =============================================================================

TEST_CASE("MaxBindingsRule: passes within limit", "[shader][compiler]") {
    MaxBindingsRule rule(16);
    ShaderReflection reflection;
    ShaderSource source;

    auto result = rule.validate(reflection, source);
    REQUIRE(result.is_ok());
}

TEST_CASE("RequiredEntryPointsRule: passes with required entry point", "[shader][compiler]") {
    RequiredEntryPointsRule rule({"main"});
    ShaderReflection reflection;
    reflection.entry_points.push_back("main");
    ShaderSource source;

    auto result = rule.validate(reflection, source);
    REQUIRE(result.is_ok());
}

TEST_CASE("RequiredEntryPointsRule: fails with missing entry point", "[shader][compiler]") {
    RequiredEntryPointsRule rule({"main", "compute"});
    ShaderReflection reflection;
    reflection.entry_points.push_back("main");
    ShaderSource source;

    auto result = rule.validate(reflection, source);
    REQUIRE_FALSE(result.is_ok());
}

// =============================================================================
// NullCompiler Tests
// =============================================================================

TEST_CASE("NullCompiler: name", "[shader][compiler]") {
    NullCompiler compiler;
    REQUIRE(compiler.name() == "NullCompiler");
}

TEST_CASE("NullCompiler: supports_language", "[shader][compiler]") {
    NullCompiler compiler;
    REQUIRE(compiler.supports_language(SourceLanguage::SpirV));
    REQUIRE_FALSE(compiler.supports_language(SourceLanguage::Glsl));
}

TEST_CASE("NullCompiler: supports_target", "[shader][compiler]") {
    NullCompiler compiler;
    REQUIRE(compiler.supports_target(CompileTarget::SpirV));
    REQUIRE_FALSE(compiler.supports_target(CompileTarget::Glsl450));
}

TEST_CASE("NullCompiler: compile SPIRV passthrough", "[shader][compiler]") {
    NullCompiler compiler;

    ShaderSource source;
    source.language = SourceLanguage::SpirV;
    source.code = "spirv binary data";

    CompilerConfig config;
    auto result = compiler.compile(source, config);

    REQUIRE(result.is_ok());
    // NullCompiler passes through SPIR-V as-is
}

TEST_CASE("NullCompiler: compile non-SPIRV fails", "[shader][compiler]") {
    NullCompiler compiler;

    ShaderSource source;
    source.language = SourceLanguage::Glsl;
    source.code = "void main() {}";

    CompilerConfig config;
    auto result = compiler.compile(source, config);

    REQUIRE(result.is_ok());
    REQUIRE_FALSE(result.value().is_success());  // Should have error
}

// =============================================================================
// CachingCompiler Tests
// =============================================================================

TEST_CASE("CachingCompiler: wraps inner compiler", "[shader][compiler]") {
    auto inner = std::make_unique<NullCompiler>();
    CachingCompiler compiler(std::move(inner));

    REQUIRE(compiler.name().find("CachingCompiler") != std::string::npos);
    REQUIRE(compiler.name().find("NullCompiler") != std::string::npos);
}

TEST_CASE("CachingCompiler: cache_size starts at zero", "[shader][compiler]") {
    auto inner = std::make_unique<NullCompiler>();
    CachingCompiler compiler(std::move(inner));

    REQUIRE(compiler.cache_size() == 0);
}

TEST_CASE("CachingCompiler: compile caches result", "[shader][compiler]") {
    auto inner = std::make_unique<NullCompiler>();
    CachingCompiler compiler(std::move(inner));

    ShaderSource source;
    source.language = SourceLanguage::SpirV;
    source.code = "spirv";
    source.name = "test";

    CompilerConfig config;

    // First compile
    auto result1 = compiler.compile(source, config);
    REQUIRE(result1.is_ok());
    REQUIRE(compiler.cache_size() == 1);

    // Second compile (should hit cache)
    auto result2 = compiler.compile(source, config);
    REQUIRE(result2.is_ok());
    REQUIRE(compiler.cache_size() == 1);  // Still 1
}

TEST_CASE("CachingCompiler: different sources have different cache entries", "[shader][compiler]") {
    auto inner = std::make_unique<NullCompiler>();
    CachingCompiler compiler(std::move(inner));

    ShaderSource source1;
    source1.language = SourceLanguage::SpirV;
    source1.code = "spirv1";
    source1.name = "shader1";

    ShaderSource source2;
    source2.language = SourceLanguage::SpirV;
    source2.code = "spirv2";
    source2.name = "shader2";

    CompilerConfig config;

    compiler.compile(source1, config);
    compiler.compile(source2, config);

    REQUIRE(compiler.cache_size() == 2);
}

TEST_CASE("CachingCompiler: clear_cache", "[shader][compiler]") {
    auto inner = std::make_unique<NullCompiler>();
    CachingCompiler compiler(std::move(inner));

    ShaderSource source;
    source.language = SourceLanguage::SpirV;
    source.code = "spirv";
    source.name = "test";

    compiler.compile(source, CompilerConfig{});
    REQUIRE(compiler.cache_size() == 1);

    compiler.clear_cache();
    REQUIRE(compiler.cache_size() == 0);
}

// =============================================================================
// CompilerFactory Tests
// =============================================================================

TEST_CASE("CompilerFactory: create_default", "[shader][compiler]") {
    auto compiler = CompilerFactory::create_default();
    REQUIRE(compiler != nullptr);
}

TEST_CASE("CompilerFactory: register and create", "[shader][compiler]") {
    // Register a test compiler
    CompilerFactory::register_compiler("test_compiler", []() {
        return std::make_unique<NullCompiler>();
    });

    auto compiler = CompilerFactory::create("test_compiler");
    REQUIRE(compiler != nullptr);
    REQUIRE(compiler->name() == "NullCompiler");
}

TEST_CASE("CompilerFactory: create unknown returns nullptr", "[shader][compiler]") {
    auto compiler = CompilerFactory::create("nonexistent_compiler");
    REQUIRE(compiler == nullptr);
}

TEST_CASE("CompilerFactory: available_compilers", "[shader][compiler]") {
    // Register a compiler to ensure at least one is available
    CompilerFactory::register_compiler("available_test", []() {
        return std::make_unique<NullCompiler>();
    });

    auto compilers = CompilerFactory::available_compilers();
    REQUIRE_FALSE(compilers.empty());
}
