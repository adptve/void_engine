#pragma once

/// @file compiler.hpp
/// @brief Shader compiler interface for void_shader

#include "fwd.hpp"
#include "types.hpp"
#include "binding.hpp"
#include "source.hpp"
#include <void_engine/core/error.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace void_shader {

// =============================================================================
// CompilerConfig
// =============================================================================

/// Configuration for shader compiler
struct CompilerConfig {
    std::vector<CompileTarget> targets = {CompileTarget::SpirV};
    bool validate = true;
    bool generate_debug_info = false;
    bool optimize = true;
    std::vector<std::string> include_paths;
    std::map<std::string, std::string> defines;

    /// Default constructor
    CompilerConfig() = default;

    /// Add compile target
    CompilerConfig& with_target(CompileTarget target) {
        targets.push_back(target);
        return *this;
    }

    /// Add include path
    CompilerConfig& with_include_path(const std::string& path) {
        include_paths.push_back(path);
        return *this;
    }

    /// Add define
    CompilerConfig& with_define(const std::string& name, const std::string& value = "") {
        defines[name] = value;
        return *this;
    }

    /// Enable debug info
    CompilerConfig& with_debug_info(bool enable = true) {
        generate_debug_info = enable;
        return *this;
    }

    /// Disable optimization
    CompilerConfig& with_optimization(bool enable = true) {
        optimize = enable;
        return *this;
    }

    /// Disable validation
    CompilerConfig& with_validation(bool enable = true) {
        validate = enable;
        return *this;
    }
};

// =============================================================================
// CompileResult
// =============================================================================

/// Result of shader compilation
struct CompileResult {
    std::map<CompileTarget, CompiledShader> compiled;
    ShaderReflection reflection;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    /// Check if successful
    [[nodiscard]] bool is_success() const noexcept {
        return errors.empty() && !compiled.empty();
    }

    /// Get compiled shader for target
    [[nodiscard]] const CompiledShader* get(CompileTarget target) const {
        auto it = compiled.find(target);
        return it != compiled.end() ? &it->second : nullptr;
    }

    /// Check if has target
    [[nodiscard]] bool has_target(CompileTarget target) const {
        return compiled.find(target) != compiled.end();
    }

    /// Get error message
    [[nodiscard]] std::string error_message() const {
        std::string msg;
        for (const auto& err : errors) {
            if (!msg.empty()) msg += "\n";
            msg += err;
        }
        return msg;
    }

    /// Get warning message
    [[nodiscard]] std::string warning_message() const {
        std::string msg;
        for (const auto& warn : warnings) {
            if (!msg.empty()) msg += "\n";
            msg += warn;
        }
        return msg;
    }
};

// =============================================================================
// ValidationRule (Interface)
// =============================================================================

/// Base class for custom validation rules
class ValidationRule {
public:
    virtual ~ValidationRule() = default;

    /// Rule name
    [[nodiscard]] virtual std::string name() const = 0;

    /// Validate shader reflection
    [[nodiscard]] virtual void_core::Result<void> validate(
        const ShaderReflection& reflection,
        const ShaderSource& source) const = 0;
};

/// Maximum bindings per group rule
class MaxBindingsRule : public ValidationRule {
public:
    explicit MaxBindingsRule(std::uint32_t max_per_group = 16)
        : m_max_per_group(max_per_group) {}

    [[nodiscard]] std::string name() const override {
        return "MaxBindingsRule";
    }

    [[nodiscard]] void_core::Result<void> validate(
        const ShaderReflection& reflection,
        const ShaderSource& /*source*/) const override
    {
        for (const auto& [group, layout] : reflection.bind_groups) {
            if (layout.binding_count() > m_max_per_group) {
                return void_core::Err("Bind group " + std::to_string(group) +
                    " exceeds max bindings (" + std::to_string(layout.binding_count()) +
                    " > " + std::to_string(m_max_per_group) + ")");
            }
        }
        return void_core::Ok();
    }

private:
    std::uint32_t m_max_per_group;
};

/// Required entry points rule
class RequiredEntryPointsRule : public ValidationRule {
public:
    explicit RequiredEntryPointsRule(std::vector<std::string> required)
        : m_required(std::move(required)) {}

    [[nodiscard]] std::string name() const override {
        return "RequiredEntryPointsRule";
    }

    [[nodiscard]] void_core::Result<void> validate(
        const ShaderReflection& reflection,
        const ShaderSource& /*source*/) const override
    {
        for (const auto& ep : m_required) {
            if (!reflection.has_entry_point(ep)) {
                return void_core::Err("Missing required entry point: " + ep);
            }
        }
        return void_core::Ok();
    }

private:
    std::vector<std::string> m_required;
};

// =============================================================================
// ShaderCompiler (Interface)
// =============================================================================

/// Abstract shader compiler interface
class ShaderCompiler {
public:
    virtual ~ShaderCompiler() = default;

    /// Compile shader source
    [[nodiscard]] virtual void_core::Result<CompileResult> compile(
        const ShaderSource& source,
        const CompilerConfig& config) = 0;

    /// Compile with variant
    [[nodiscard]] virtual void_core::Result<CompileResult> compile_variant(
        const ShaderSource& source,
        const ShaderVariant& variant,
        const CompilerConfig& config)
    {
        // Default implementation: prepend variant defines to source
        ShaderSource modified_source = source;
        modified_source.code = source.with_variant(variant);
        modified_source.name = variant.name.empty() ? source.name : variant.name;
        return compile(modified_source, config);
    }

    /// Get compiler name
    [[nodiscard]] virtual std::string name() const = 0;

    /// Check if compiler supports source language
    [[nodiscard]] virtual bool supports_language(SourceLanguage lang) const = 0;

    /// Check if compiler supports target
    [[nodiscard]] virtual bool supports_target(CompileTarget target) const = 0;

    /// Add validation rule
    void add_validation_rule(std::unique_ptr<ValidationRule> rule) {
        m_validation_rules.push_back(std::move(rule));
    }

    /// Clear validation rules
    void clear_validation_rules() {
        m_validation_rules.clear();
    }

protected:
    /// Run custom validation rules
    [[nodiscard]] void_core::Result<void> run_validation(
        const ShaderReflection& reflection,
        const ShaderSource& source) const
    {
        for (const auto& rule : m_validation_rules) {
            auto result = rule->validate(reflection, source);
            if (!result) {
                return result;
            }
        }
        return void_core::Ok();
    }

    std::vector<std::unique_ptr<ValidationRule>> m_validation_rules;
};

// =============================================================================
// NullCompiler (Stub Implementation)
// =============================================================================

/// Null compiler that passes through SPIR-V
class NullCompiler : public ShaderCompiler {
public:
    [[nodiscard]] void_core::Result<CompileResult> compile(
        const ShaderSource& source,
        const CompilerConfig& /*config*/) override
    {
        CompileResult result;

        if (source.language == SourceLanguage::SpirV) {
            // Pass through pre-compiled SPIR-V
            CompiledShader compiled;
            compiled.target = CompileTarget::SpirV;
            compiled.stage = source.stage.value_or(ShaderStage::Vertex);
            compiled.binary = std::vector<std::uint8_t>(source.code.begin(), source.code.end());
            compiled.entry_point = "main";

            result.compiled[CompileTarget::SpirV] = std::move(compiled);
        } else {
            result.errors.push_back("NullCompiler only supports pre-compiled SPIR-V");
        }

        return void_core::Ok(std::move(result));
    }

    [[nodiscard]] std::string name() const override {
        return "NullCompiler";
    }

    [[nodiscard]] bool supports_language(SourceLanguage lang) const override {
        return lang == SourceLanguage::SpirV;
    }

    [[nodiscard]] bool supports_target(CompileTarget target) const override {
        return target == CompileTarget::SpirV;
    }
};

// =============================================================================
// CachingCompiler (Decorator)
// =============================================================================

/// Compiler wrapper that caches compiled results
class CachingCompiler : public ShaderCompiler {
public:
    /// Constructor with underlying compiler
    explicit CachingCompiler(std::unique_ptr<ShaderCompiler> inner, std::size_t max_cache_size = 256)
        : m_inner(std::move(inner)), m_max_cache_size(max_cache_size) {}

    [[nodiscard]] void_core::Result<CompileResult> compile(
        const ShaderSource& source,
        const CompilerConfig& config) override
    {
        // Generate cache key
        std::string key = generate_cache_key(source, config);

        // Check cache
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return void_core::Ok(it->second);
        }

        // Compile
        auto result = m_inner->compile(source, config);
        if (!result) {
            return result;
        }

        // Cache result
        if (m_cache.size() >= m_max_cache_size) {
            // Simple eviction: remove first entry
            m_cache.erase(m_cache.begin());
        }
        m_cache[key] = result.value();

        return result;
    }

    [[nodiscard]] std::string name() const override {
        return "CachingCompiler(" + m_inner->name() + ")";
    }

    [[nodiscard]] bool supports_language(SourceLanguage lang) const override {
        return m_inner->supports_language(lang);
    }

    [[nodiscard]] bool supports_target(CompileTarget target) const override {
        return m_inner->supports_target(target);
    }

    /// Clear cache
    void clear_cache() {
        m_cache.clear();
    }

    /// Get cache size
    [[nodiscard]] std::size_t cache_size() const noexcept {
        return m_cache.size();
    }

private:
    [[nodiscard]] std::string generate_cache_key(
        const ShaderSource& source,
        const CompilerConfig& config) const
    {
        // Simple hash-based key
        std::string key = source.name + "|" + source.code;

        // Add defines
        for (const auto& [name, value] : config.defines) {
            key += "|" + name + "=" + value;
        }

        // Add targets
        for (const auto& target : config.targets) {
            key += "|" + std::to_string(static_cast<int>(target));
        }

        return key;
    }

    std::unique_ptr<ShaderCompiler> m_inner;
    std::size_t m_max_cache_size;
    std::map<std::string, CompileResult> m_cache;
};

// =============================================================================
// CompilerFactory
// =============================================================================

/// Factory for creating shader compilers
class CompilerFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<ShaderCompiler>()>;

    /// Register compiler creator
    static void register_compiler(const std::string& name, CreatorFunc creator) {
        get_creators()[name] = std::move(creator);
    }

    /// Create compiler by name
    [[nodiscard]] static std::unique_ptr<ShaderCompiler> create(const std::string& name) {
        auto& creators = get_creators();
        auto it = creators.find(name);
        if (it != creators.end()) {
            return it->second();
        }
        return nullptr;
    }

    /// Create default compiler
    [[nodiscard]] static std::unique_ptr<ShaderCompiler> create_default() {
        // Try to create in order of preference
        if (auto compiler = create("shaderc")) return compiler;
        if (auto compiler = create("glslang")) return compiler;

        // Fallback to null compiler
        return std::make_unique<NullCompiler>();
    }

    /// Get available compiler names
    [[nodiscard]] static std::vector<std::string> available_compilers() {
        std::vector<std::string> names;
        for (const auto& [name, creator] : get_creators()) {
            names.push_back(name);
        }
        return names;
    }

private:
    static std::map<std::string, CreatorFunc>& get_creators() {
        static std::map<std::string, CreatorFunc> creators;
        return creators;
    }
};

} // namespace void_shader
