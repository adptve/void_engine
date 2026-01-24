#pragma once

/// @file hot_reload.hpp
/// @brief Shader hot-reload functionality for void_shader

#include "fwd.hpp"
#include "types.hpp"
#include "registry.hpp"
#include "compiler.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <mutex>
#include <filesystem>

namespace void_shader {

// =============================================================================
// ShaderChangeEvent
// =============================================================================

/// Shader change event from file watcher
struct ShaderChangeEvent {
    ShaderId id;
    std::string path;
    void_core::ReloadEventType type;
    std::chrono::steady_clock::time_point timestamp;

    /// Default constructor
    ShaderChangeEvent()
        : type(void_core::ReloadEventType::FileModified)
        , timestamp(std::chrono::steady_clock::now()) {}

    /// Construct with values
    ShaderChangeEvent(ShaderId sid, std::string p, void_core::ReloadEventType t)
        : id(std::move(sid))
        , path(std::move(p))
        , type(t)
        , timestamp(std::chrono::steady_clock::now()) {}
};

// =============================================================================
// ShaderReloadResult
// =============================================================================

/// Result of a shader reload operation
struct ShaderReloadResult {
    ShaderId id;
    std::string path;
    bool success;
    std::string error_message;
    ShaderVersion old_version;
    ShaderVersion new_version;

    /// Check if successful
    [[nodiscard]] bool is_ok() const noexcept { return success; }

    /// Create success result
    [[nodiscard]] static ShaderReloadResult ok(
        ShaderId sid,
        const std::string& p,
        ShaderVersion old_ver,
        ShaderVersion new_ver)
    {
        ShaderReloadResult r;
        r.id = std::move(sid);
        r.path = p;
        r.success = true;
        r.old_version = old_ver;
        r.new_version = new_ver;
        return r;
    }

    /// Create failure result
    [[nodiscard]] static ShaderReloadResult fail(
        ShaderId sid,
        const std::string& p,
        const std::string& err)
    {
        ShaderReloadResult r;
        r.id = std::move(sid);
        r.path = p;
        r.success = false;
        r.error_message = err;
        return r;
    }
};

// =============================================================================
// ShaderWatcher
// =============================================================================

/// File watcher specialized for shaders
class ShaderWatcher {
public:
    using Callback = std::function<void(const ShaderChangeEvent&)>;

    /// Configuration
    struct Config {
        std::chrono::milliseconds debounce_interval{100};
        std::vector<std::string> watch_extensions{".wgsl", ".glsl", ".vert", ".frag", ".comp"};
        bool recursive = true;
    };

    /// Constructor
    explicit ShaderWatcher(Config config = {})
        : m_config(config)
        , m_watcher(std::make_unique<void_core::PollingFileWatcher>(config.debounce_interval)) {}

    /// Start watching a directory
    void_core::Result<void> watch_directory(const std::string& path) {
        std::filesystem::path dir_path(path);

        if (!std::filesystem::exists(dir_path)) {
            return void_core::Err("Directory does not exist: " + path);
        }

        if (!std::filesystem::is_directory(dir_path)) {
            return void_core::Err("Path is not a directory: " + path);
        }

        // Watch all matching files in directory
        std::error_code ec;

        if (m_config.recursive) {
            auto options = std::filesystem::directory_options::follow_directory_symlink;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path, options, ec)) {
                if (ec) {
                    return void_core::Err("Failed to iterate directory: " + ec.message());
                }
                if (entry.is_regular_file() && is_shader_file(entry.path().string())) {
                    m_watcher->watch(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
                if (ec) {
                    return void_core::Err("Failed to iterate directory: " + ec.message());
                }
                if (entry.is_regular_file() && is_shader_file(entry.path().string())) {
                    m_watcher->watch(entry.path().string());
                }
            }
        }

        m_watched_directories.insert(path);
        return void_core::Ok();
    }

    /// Watch single file
    void_core::Result<void> watch_file(const std::string& path) {
        if (!is_shader_file(path)) {
            return void_core::Err("Not a shader file: " + path);
        }
        return m_watcher->watch(path);
    }

    /// Stop watching a path
    void_core::Result<void> unwatch(const std::string& path) {
        return m_watcher->unwatch(path);
    }

    /// Poll for changes
    std::vector<void_core::ReloadEvent> poll() {
        return m_watcher->poll();
    }

    /// Set callback for events
    void set_callback(Callback cb) {
        m_callback = std::move(cb);
    }

    /// Clear all watches
    void clear() {
        m_watcher->clear();
        m_watched_directories.clear();
    }

    /// Check if path is shader file
    [[nodiscard]] bool is_shader_file(const std::string& path) const {
        std::filesystem::path p(path);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        for (const auto& watch_ext : m_config.watch_extensions) {
            if (ext == watch_ext) return true;
        }
        return false;
    }

private:
    Config m_config;
    std::unique_ptr<void_core::FileWatcher> m_watcher;
    std::set<std::string> m_watched_directories;
    Callback m_callback;
};

// =============================================================================
// ShaderHotReloadManager
// =============================================================================

/// Manager for shader hot-reload operations
class ShaderHotReloadManager {
public:
    /// Configuration
    struct Config {
        std::chrono::milliseconds debounce_interval{100};
        bool auto_rollback_on_failure = true;
        bool log_events = true;
    };

    /// Constructor
    ShaderHotReloadManager(
        ShaderRegistry& registry,
        ShaderCompiler& compiler,
        CompilerConfig compiler_config,
        Config config = {})
        : m_registry(registry)
        , m_compiler(compiler)
        , m_compiler_config(std::move(compiler_config))
        , m_config(config)
    {
        ShaderWatcher::Config watcher_config;
        watcher_config.debounce_interval = config.debounce_interval;
        m_watcher = std::make_unique<ShaderWatcher>(watcher_config);
    }

    /// Start watching for shader changes
    void_core::Result<void> start_watching(const std::string& shader_directory) {
        m_shader_directory = shader_directory;
        return m_watcher->watch_directory(shader_directory);
    }

    /// Stop watching
    void stop_watching() {
        m_watcher->clear();
    }

    /// Poll and process shader changes
    std::vector<ShaderReloadResult> poll_changes() {
        std::vector<ShaderReloadResult> results;

        auto events = m_watcher->poll();
        for (const auto& event : events) {
            auto result = process_event(event);
            if (result.has_value()) {
                results.push_back(std::move(*result));
            }
        }

        return results;
    }

    /// Force reload shader by path
    void_core::Result<ShaderReloadResult> reload_shader(const std::string& path) {
        auto shader_id = m_registry.find_by_path(path);
        if (!shader_id.has_value()) {
            return void_core::Err<ShaderReloadResult>(
                ShaderError::not_found("Shader not found for path: " + path));
        }

        return reload_shader_by_id(*shader_id, path);
    }

    /// Force reload shader by ID
    void_core::Result<ShaderReloadResult> reload_shader_by_id(
        const ShaderId& id,
        const std::string& path = "")
    {
        const auto* entry = m_registry.get(id);
        if (!entry) {
            return void_core::Err<ShaderReloadResult>(ShaderError::not_found(id.name()));
        }

        ShaderVersion old_version = entry->version;

        // Load new source
        std::string source_path = path.empty() ? entry->source.source_path : path;
        auto source_result = ShaderSource::from_file(source_path);
        if (!source_result) {
            return void_core::Err<ShaderReloadResult>(source_result.error());
        }

        // Recompile
        auto compile_result = m_registry.recompile(
            id,
            std::move(source_result).value(),
            m_compiler,
            m_compiler_config);

        if (!compile_result) {
            if (m_config.auto_rollback_on_failure) {
                m_registry.rollback(id);
            }
            return void_core::Ok(ShaderReloadResult::fail(
                id, source_path, compile_result.error().message()));
        }

        ShaderVersion new_version = m_registry.get_version(id);
        return void_core::Ok(ShaderReloadResult::ok(id, source_path, old_version, new_version));
    }

    /// Register callback for reload events
    using ReloadCallback = std::function<void(const ShaderReloadResult&)>;
    void on_reload(ReloadCallback callback) {
        m_callbacks.push_back(std::move(callback));
    }

    /// Get pending reload count
    [[nodiscard]] std::size_t pending_count() const {
        return m_pending_reloads.size();
    }

    /// Check if watching
    [[nodiscard]] bool is_watching() const noexcept {
        return !m_shader_directory.empty();
    }

private:
    std::optional<ShaderReloadResult> process_event(const void_core::ReloadEvent& event) {
        // Only process shader files
        if (!m_watcher->is_shader_file(event.path)) {
            return std::nullopt;
        }

        // Find shader for this path
        auto shader_id = m_registry.find_by_path(event.path);

        switch (event.type) {
            case void_core::ReloadEventType::FileModified:
            case void_core::ReloadEventType::ForceReload:
                if (shader_id.has_value()) {
                    auto result = reload_shader_by_id(*shader_id, event.path);
                    if (result.is_ok()) {
                        notify_callbacks(result.value());
                        return result.value();
                    } else {
                        auto fail = ShaderReloadResult::fail(
                            *shader_id, event.path, result.error().message());
                        notify_callbacks(fail);
                        return fail;
                    }
                }
                break;

            case void_core::ReloadEventType::FileCreated:
                // Track for potential future registration
                m_pending_reloads.insert(event.path);
                break;

            case void_core::ReloadEventType::FileDeleted:
                // Remove from pending
                m_pending_reloads.erase(event.path);
                break;

            case void_core::ReloadEventType::FileRenamed:
                // Update path mapping if shader exists
                if (shader_id.has_value()) {
                    m_registry.update_path_mapping(*shader_id, event.path);
                }
                break;
        }

        return std::nullopt;
    }

    void notify_callbacks(const ShaderReloadResult& result) {
        for (const auto& cb : m_callbacks) {
            cb(result);
        }
    }

    ShaderRegistry& m_registry;
    ShaderCompiler& m_compiler;
    CompilerConfig m_compiler_config;
    Config m_config;
    std::unique_ptr<ShaderWatcher> m_watcher;
    std::string m_shader_directory;
    std::set<std::string> m_pending_reloads;
    std::vector<ReloadCallback> m_callbacks;
};

} // namespace void_shader
