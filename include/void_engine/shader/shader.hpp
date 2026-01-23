#pragma once

/// @file shader.hpp
/// @brief Main include file for void_shader module
///
/// This header includes all void_shader components.

// Forward declarations
#include "fwd.hpp"

// Type definitions
#include "types.hpp"

// Binding and reflection
#include "binding.hpp"

// Source handling
#include "source.hpp"

// Compiler interface
#include "compiler.hpp"

// Shaderc compiler (optional, requires VOID_HAS_SHADERC)
#include "shaderc_compiler.hpp"

// Registry and versioning
#include "registry.hpp"

// Hot-reload
#include "hot_reload.hpp"

/// @namespace void_shader
/// @brief Shader compilation and management module
///
/// void_shader provides comprehensive shader handling including:
///
/// - **Shader Types**: Stage definitions, compile targets, variants
/// - **Binding Reflection**: Automatic extraction of bind group layouts
/// - **Compilation**: Pluggable compiler backends
/// - **Registry**: Central shader management with version tracking
/// - **Hot-Reload**: File watching and automatic recompilation
///
/// Example usage:
/// @code
/// #include <void_engine/shader/shader.hpp>
///
/// using namespace void_shader;
///
/// // Create registry and compiler
/// ShaderRegistry registry;
/// auto compiler = CompilerFactory::create_default();
/// CompilerConfig config;
/// config.targets = {CompileTarget::SpirV};
///
/// // Register shader from file
/// auto id = registry.register_from_file("shaders/pbr.wgsl");
///
/// // Compile
/// registry.compile(*id, *compiler, config);
///
/// // Get compiled output
/// const auto* spirv = registry.get_compiled(*id, CompileTarget::SpirV);
///
/// // Hot-reload setup
/// ShaderHotReloadManager hot_reload(registry, *compiler, config);
/// hot_reload.start_watching("shaders/");
///
/// // In render loop
/// auto changes = hot_reload.poll_changes();
/// for (const auto& change : changes) {
///     if (change.success) {
///         log("Reloaded: " + change.path);
///     } else {
///         log("Failed: " + change.error_message);
///     }
/// }
/// @endcode

namespace void_shader {

// =============================================================================
// ShaderPipelineConfig
// =============================================================================

/// Configuration for shader pipeline
struct ShaderPipelineConfig {
    std::string shader_base_path = "shaders";
    std::vector<CompileTarget> default_targets = {CompileTarget::SpirV};
    bool validate = true;
    bool hot_reload = true;
    std::size_t max_cached_shaders = 256;
    std::size_t max_history_depth = 3;
    std::chrono::milliseconds debounce_interval{100};
    std::vector<std::string> include_paths;

    /// Default constructor
    ShaderPipelineConfig() = default;

    /// Builder pattern
    ShaderPipelineConfig& with_base_path(const std::string& path) {
        shader_base_path = path;
        return *this;
    }

    ShaderPipelineConfig& with_target(CompileTarget target) {
        default_targets.push_back(target);
        return *this;
    }

    ShaderPipelineConfig& with_validation(bool enable) {
        validate = enable;
        return *this;
    }

    ShaderPipelineConfig& with_hot_reload(bool enable) {
        hot_reload = enable;
        return *this;
    }

    ShaderPipelineConfig& with_cache_size(std::size_t size) {
        max_cached_shaders = size;
        return *this;
    }

    ShaderPipelineConfig& with_include_path(const std::string& path) {
        include_paths.push_back(path);
        return *this;
    }
};

// =============================================================================
// ShaderPipeline
// =============================================================================

/// Main shader pipeline combining all components
class ShaderPipeline {
public:
    /// Constructor with configuration
    explicit ShaderPipeline(ShaderPipelineConfig config = {})
        : m_config(std::move(config))
    {
        // Create registry
        ShaderRegistry::Config reg_config;
        reg_config.max_cached_shaders = m_config.max_cached_shaders;
        reg_config.max_history_depth = m_config.max_history_depth;
        m_registry = std::make_unique<ShaderRegistry>(reg_config);

        // Create compiler
        m_compiler = CompilerFactory::create_default();
        if (m_config.max_cached_shaders > 0) {
            m_compiler = std::make_unique<CachingCompiler>(
                std::move(m_compiler), m_config.max_cached_shaders);
        }

        // Create compiler config
        m_compiler_config.targets = m_config.default_targets;
        m_compiler_config.validate = m_config.validate;
        m_compiler_config.include_paths = m_config.include_paths;

        // Create hot-reload manager if enabled
        if (m_config.hot_reload) {
            ShaderHotReloadManager::Config hr_config;
            hr_config.debounce_interval = m_config.debounce_interval;
            m_hot_reload = std::make_unique<ShaderHotReloadManager>(
                *m_registry, *m_compiler, m_compiler_config, hr_config);
        }
    }

    /// Load and compile shader from file
    void_core::Result<ShaderId> load(const std::string& path) {
        std::string full_path = m_config.shader_base_path + "/" + path;

        auto id_result = m_registry->register_from_file(full_path);
        if (!id_result) {
            return id_result;
        }

        auto compile_result = m_registry->compile(id_result.value(), *m_compiler, m_compiler_config);
        if (!compile_result) {
            m_registry->unregister(id_result.value());
            return void_core::Err<ShaderId>(compile_result.error());
        }

        return id_result;
    }

    /// Load and compile shader from source
    void_core::Result<ShaderId> load_source(ShaderSource source) {
        auto id_result = m_registry->register_shader(std::move(source));
        if (!id_result) {
            return id_result;
        }

        auto compile_result = m_registry->compile(id_result.value(), *m_compiler, m_compiler_config);
        if (!compile_result) {
            m_registry->unregister(id_result.value());
            return void_core::Err<ShaderId>(compile_result.error());
        }

        return id_result;
    }

    /// Unload shader
    bool unload(const ShaderId& id) {
        return m_registry->unregister(id);
    }

    /// Get shader
    [[nodiscard]] const ShaderEntry* get(const ShaderId& id) const {
        return m_registry->get(id);
    }

    /// Get compiled shader
    [[nodiscard]] const CompiledShader* get_compiled(
        const ShaderId& id,
        CompileTarget target) const
    {
        return m_registry->get_compiled(id, target);
    }

    /// Get SPIR-V compiled shader
    [[nodiscard]] const CompiledShader* get_spirv(const ShaderId& id) const {
        return get_compiled(id, CompileTarget::SpirV);
    }

    /// Get shader reflection
    [[nodiscard]] const ShaderReflection* get_reflection(const ShaderId& id) const {
        return m_registry->get_reflection(id);
    }

    /// Check if shader exists
    [[nodiscard]] bool contains(const ShaderId& id) const {
        return m_registry->contains(id);
    }

    /// Start watching for hot-reload
    void_core::Result<void> start_watching() {
        if (!m_hot_reload) {
            return void_core::Err(void_core::Error("Hot-reload not enabled"));
        }
        return m_hot_reload->start_watching(m_config.shader_base_path);
    }

    /// Stop watching
    void stop_watching() {
        if (m_hot_reload) {
            m_hot_reload->stop_watching();
        }
    }

    /// Poll for shader changes
    std::vector<ShaderReloadResult> poll_changes() {
        if (!m_hot_reload) {
            return {};
        }
        return m_hot_reload->poll_changes();
    }

    /// Add reload callback
    void on_reload(ShaderHotReloadManager::ReloadCallback callback) {
        if (m_hot_reload) {
            m_hot_reload->on_reload(std::move(callback));
        }
    }

    /// Add shader change listener
    void add_listener(ShaderListener listener) {
        m_registry->add_listener(std::move(listener));
    }

    /// Get shader count
    [[nodiscard]] std::size_t shader_count() const {
        return m_registry->len();
    }

    /// Get registry
    [[nodiscard]] ShaderRegistry& registry() { return *m_registry; }
    [[nodiscard]] const ShaderRegistry& registry() const { return *m_registry; }

    /// Get compiler
    [[nodiscard]] ShaderCompiler& compiler() { return *m_compiler; }
    [[nodiscard]] const ShaderCompiler& compiler() const { return *m_compiler; }

    /// Get config
    [[nodiscard]] const ShaderPipelineConfig& config() const { return m_config; }

private:
    ShaderPipelineConfig m_config;
    std::unique_ptr<ShaderRegistry> m_registry;
    std::unique_ptr<ShaderCompiler> m_compiler;
    CompilerConfig m_compiler_config;
    std::unique_ptr<ShaderHotReloadManager> m_hot_reload;
};

/// Library version
inline constexpr void_core::Version VOID_SHADER_VERSION{0, 1, 0};

/// Get library version string
[[nodiscard]] inline std::string void_shader_version_string() {
    return "void_shader " + VOID_SHADER_VERSION.to_string();
}

} // namespace void_shader
