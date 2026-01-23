#pragma once

/// @file registry.hpp
/// @brief Shader registry and version tracking for void_shader

#include "fwd.hpp"
#include "types.hpp"
#include "binding.hpp"
#include "source.hpp"
#include "compiler.hpp"
#include <void_engine/core/error.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace void_shader {

// =============================================================================
// ShaderEntry
// =============================================================================

/// Entry in the shader registry
struct ShaderEntry {
    ShaderId id;
    std::string name;
    ShaderSource source;
    ShaderVersion version;
    ShaderReflection reflection;
    std::map<CompileTarget, CompiledShader> compiled;
    ShaderMetadata metadata;

    /// Default constructor
    ShaderEntry() : version(ShaderVersion::initial()) {}

    /// Construct with source
    ShaderEntry(ShaderId sid, ShaderSource src)
        : id(std::move(sid))
        , name(src.name)
        , source(std::move(src))
        , version(ShaderVersion::initial()) {}

    /// Check if has compiled output for target
    [[nodiscard]] bool has_target(CompileTarget target) const {
        return compiled.find(target) != compiled.end();
    }

    /// Get compiled output for target
    [[nodiscard]] const CompiledShader* get_compiled(CompileTarget target) const {
        auto it = compiled.find(target);
        return it != compiled.end() ? &it->second : nullptr;
    }

    /// Update from compile result
    void update_from_result(const CompileResult& result) {
        reflection = result.reflection;
        compiled = result.compiled;
        version = version.next();
        metadata.mark_updated();
    }
};

// =============================================================================
// ShaderListener
// =============================================================================

/// Callback for shader changes
using ShaderListener = std::function<void(const ShaderId&, ShaderVersion)>;

// =============================================================================
// ShaderRegistry
// =============================================================================

/// Central registry for shader management
class ShaderRegistry {
public:
    /// Configuration
    struct Config {
        std::size_t max_cached_shaders = 256;
        std::size_t max_history_depth = 3;
    };

    /// Constructor
    explicit ShaderRegistry(Config config = {}) : m_config(config) {}

    /// Register shader from source
    void_core::Result<ShaderId> register_shader(ShaderSource source) {
        std::unique_lock lock(m_mutex);

        ShaderId id(source.name);

        if (m_entries.find(id.name()) != m_entries.end()) {
            return void_core::Err<ShaderId>(
                void_core::Error(void_core::ErrorCode::AlreadyExists,
                    "Shader already registered: " + source.name));
        }

        ShaderEntry entry(id, std::move(source));
        m_entries[id.name()] = std::move(entry);

        return void_core::Ok(id);
    }

    /// Register shader from file
    void_core::Result<ShaderId> register_from_file(const std::string& path) {
        auto source_result = ShaderSource::from_file(path);
        if (!source_result) {
            return void_core::Err<ShaderId>(source_result.error());
        }

        auto result = register_shader(std::move(source_result).value());
        if (result.is_ok()) {
            // Track path for hot-reload
            std::unique_lock lock(m_mutex);
            m_path_to_shader[path] = result.value().name();
        }

        return result;
    }

    /// Compile shader with compiler
    void_core::Result<void> compile(
        const ShaderId& id,
        ShaderCompiler& compiler,
        const CompilerConfig& config)
    {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id.name());
        if (it == m_entries.end()) {
            return void_core::Err(ShaderError::not_found(id.name()));
        }

        auto& entry = it->second;

        // Save history for rollback
        save_history(id.name(), entry);

        // Compile
        auto result = compiler.compile(entry.source, config);
        if (!result) {
            return void_core::Err(result.error());
        }

        if (!result.value().is_success()) {
            return void_core::Err(ShaderError::compile_error(
                id.name(), result.value().error_message()));
        }

        // Update entry
        entry.update_from_result(result.value());

        // Notify listeners
        notify_listeners(id, entry.version);

        return void_core::Ok();
    }

    /// Recompile shader with new source
    void_core::Result<void> recompile(
        const ShaderId& id,
        ShaderSource new_source,
        ShaderCompiler& compiler,
        const CompilerConfig& config)
    {
        std::unique_lock lock(m_mutex);

        auto it = m_entries.find(id.name());
        if (it == m_entries.end()) {
            return void_core::Err(ShaderError::not_found(id.name()));
        }

        auto& entry = it->second;

        // Save history
        save_history(id.name(), entry);

        // Compile new source
        auto result = compiler.compile(new_source, config);
        if (!result) {
            return void_core::Err(result.error());
        }

        if (!result.value().is_success()) {
            return void_core::Err(ShaderError::compile_error(
                id.name(), result.value().error_message()));
        }

        // Update entry
        entry.source = std::move(new_source);
        entry.update_from_result(result.value());

        // Notify listeners
        notify_listeners(id, entry.version);

        return void_core::Ok();
    }

    /// Rollback shader to previous version
    void_core::Result<void> rollback(const ShaderId& id) {
        std::unique_lock lock(m_mutex);

        auto history_it = m_history.find(id.name());
        if (history_it == m_history.end() || history_it->second.empty()) {
            return void_core::Err(ShaderError::no_rollback(id.name()));
        }

        auto entry_it = m_entries.find(id.name());
        if (entry_it == m_entries.end()) {
            return void_core::Err(ShaderError::not_found(id.name()));
        }

        // Restore from history
        entry_it->second = std::move(history_it->second.back());
        history_it->second.pop_back();

        // Notify listeners
        notify_listeners(id, entry_it->second.version);

        return void_core::Ok();
    }

    /// Get shader entry
    [[nodiscard]] const ShaderEntry* get(const ShaderId& id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_entries.find(id.name());
        return it != m_entries.end() ? &it->second : nullptr;
    }

    /// Get compiled shader for target
    [[nodiscard]] const CompiledShader* get_compiled(
        const ShaderId& id,
        CompileTarget target) const
    {
        std::shared_lock lock(m_mutex);
        auto it = m_entries.find(id.name());
        if (it == m_entries.end()) {
            return nullptr;
        }
        return it->second.get_compiled(target);
    }

    /// Get shader reflection
    [[nodiscard]] const ShaderReflection* get_reflection(const ShaderId& id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_entries.find(id.name());
        return it != m_entries.end() ? &it->second.reflection : nullptr;
    }

    /// Get shader version
    [[nodiscard]] ShaderVersion get_version(const ShaderId& id) const {
        std::shared_lock lock(m_mutex);
        auto it = m_entries.find(id.name());
        return it != m_entries.end() ? it->second.version : ShaderVersion{};
    }

    /// Check if shader exists
    [[nodiscard]] bool contains(const ShaderId& id) const {
        std::shared_lock lock(m_mutex);
        return m_entries.find(id.name()) != m_entries.end();
    }

    /// Unregister shader
    bool unregister(const ShaderId& id) {
        std::unique_lock lock(m_mutex);
        return m_entries.erase(id.name()) > 0;
    }

    /// Get shader count
    [[nodiscard]] std::size_t len() const {
        std::shared_lock lock(m_mutex);
        return m_entries.size();
    }

    /// Check if empty
    [[nodiscard]] bool is_empty() const {
        std::shared_lock lock(m_mutex);
        return m_entries.empty();
    }

    /// Clear all shaders
    void clear() {
        std::unique_lock lock(m_mutex);
        m_entries.clear();
        m_history.clear();
        m_path_to_shader.clear();
    }

    /// Add listener
    void add_listener(ShaderListener listener) {
        std::unique_lock lock(m_mutex);
        m_listeners.push_back(std::move(listener));
    }

    /// Find shader by path
    [[nodiscard]] std::optional<ShaderId> find_by_path(const std::string& path) const {
        std::shared_lock lock(m_mutex);
        auto it = m_path_to_shader.find(path);
        if (it != m_path_to_shader.end()) {
            return ShaderId(it->second);
        }
        return std::nullopt;
    }

    /// Update path mapping (for hot-reload)
    void update_path_mapping(const ShaderId& id, const std::string& path) {
        std::unique_lock lock(m_mutex);
        m_path_to_shader[path] = id.name();
    }

    /// Iterate over all shaders
    template<typename F>
    void for_each(F&& func) const {
        std::shared_lock lock(m_mutex);
        for (const auto& [name, entry] : m_entries) {
            func(entry.id, entry);
        }
    }

    /// Get all shader IDs
    [[nodiscard]] std::vector<ShaderId> get_all_ids() const {
        std::shared_lock lock(m_mutex);
        std::vector<ShaderId> ids;
        ids.reserve(m_entries.size());
        for (const auto& [name, entry] : m_entries) {
            ids.push_back(entry.id);
        }
        return ids;
    }

private:
    void save_history(const std::string& name, const ShaderEntry& entry) {
        auto& history = m_history[name];
        if (history.size() >= m_config.max_history_depth) {
            history.erase(history.begin());
        }
        history.push_back(entry);
    }

    void notify_listeners(const ShaderId& id, ShaderVersion version) {
        for (const auto& listener : m_listeners) {
            listener(id, version);
        }
    }

    Config m_config;
    std::map<std::string, ShaderEntry> m_entries;
    std::map<std::string, std::vector<ShaderEntry>> m_history;
    std::map<std::string, std::string> m_path_to_shader;
    std::vector<ShaderListener> m_listeners;
    mutable std::shared_mutex m_mutex;
};

// =============================================================================
// ShaderVariantCollection
// =============================================================================

/// Collection of shader variants
class ShaderVariantCollection {
public:
    /// Constructor
    explicit ShaderVariantCollection(ShaderSource base_source)
        : m_base_source(std::move(base_source)) {}

    /// Add variant
    void add_variant(ShaderVariant variant) {
        m_variants.push_back(std::move(variant));
    }

    /// Build variants from builder
    void build_variants(const VariantBuilder& builder) {
        auto variants = builder.build();
        for (auto& v : variants) {
            m_variants.push_back(std::move(v));
        }
    }

    /// Compile all variants
    void_core::Result<void> compile_all(
        ShaderCompiler& compiler,
        const CompilerConfig& config)
    {
        m_compiled.clear();

        for (const auto& variant : m_variants) {
            auto result = compiler.compile_variant(m_base_source, variant, config);
            if (!result) {
                return void_core::Err(result.error());
            }

            if (!result.value().is_success()) {
                return void_core::Err(ShaderError::compile_error(
                    variant.name, result.value().error_message()));
            }

            m_compiled[variant.name] = std::move(result).value();
        }

        return void_core::Ok();
    }

    /// Get compiled variant
    [[nodiscard]] const CompileResult* get_variant(const std::string& name) const {
        auto it = m_compiled.find(name);
        return it != m_compiled.end() ? &it->second : nullptr;
    }

    /// Get variant count
    [[nodiscard]] std::size_t variant_count() const noexcept {
        return m_variants.size();
    }

    /// Get compiled count
    [[nodiscard]] std::size_t compiled_count() const noexcept {
        return m_compiled.size();
    }

    /// Get all variant names
    [[nodiscard]] std::vector<std::string> variant_names() const {
        std::vector<std::string> names;
        names.reserve(m_variants.size());
        for (const auto& v : m_variants) {
            names.push_back(v.name);
        }
        return names;
    }

private:
    ShaderSource m_base_source;
    std::vector<ShaderVariant> m_variants;
    std::map<std::string, CompileResult> m_compiled;
};

} // namespace void_shader
