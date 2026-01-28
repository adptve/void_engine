/// @file plugin_watcher.cpp
/// @brief Implementation of plugin file watcher and hot-reload machinery

#include <void_engine/plugin_api/plugin_watcher.hpp>

#include <algorithm>
#include <cstdlib>

namespace void_plugin_api {

// =============================================================================
// PluginWatcher Implementation
// =============================================================================

PluginWatcher::PluginWatcher(IPluginLoader& loader)
    : m_loader(loader) {}

PluginWatcher::PluginWatcher(IPluginLoader& loader, const PluginWatcherConfig& config)
    : m_loader(loader)
    , m_config(config) {}

PluginWatcher::~PluginWatcher() {
    stop();
}

void PluginWatcher::start() {
    if (m_running.exchange(true)) {
        return;  // Already running
    }

    // Initial scan
    scan_directories();

    // Start watch thread
    m_watch_thread = std::thread(&PluginWatcher::watch_thread, this);
}

void PluginWatcher::stop() {
    if (!m_running.exchange(false)) {
        return;  // Not running
    }

    if (m_watch_thread.joinable()) {
        m_watch_thread.join();
    }
}

void PluginWatcher::add_watch_path(const std::filesystem::path& path) {
    std::lock_guard lock(m_mutex);
    if (std::find(m_config.watch_paths.begin(), m_config.watch_paths.end(), path)
        == m_config.watch_paths.end()) {
        m_config.watch_paths.push_back(path);
    }
    m_scan_requested.store(true);
}

void PluginWatcher::remove_watch_path(const std::filesystem::path& path) {
    std::lock_guard lock(m_mutex);
    auto it = std::find(m_config.watch_paths.begin(), m_config.watch_paths.end(), path);
    if (it != m_config.watch_paths.end()) {
        m_config.watch_paths.erase(it);
    }
}

void PluginWatcher::set_build_command(const std::string& command) {
    std::lock_guard lock(m_mutex);
    m_config.build_command = command;
}

void PluginWatcher::set_config(const PluginWatcherConfig& config) {
    std::lock_guard lock(m_mutex);
    m_config = config;
}

void PluginWatcher::scan_now() {
    m_scan_requested.store(true);
}

bool PluginWatcher::reload_plugin(const std::string& name) {
    std::lock_guard lock(m_mutex);

    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) {
        emit_event(PluginEventType::ReloadFailed, name, {}, "Plugin not found");
        return false;
    }

    emit_event(PluginEventType::ReloadStarted, name, it->second.path);

    auto start_time = std::chrono::steady_clock::now();
    bool success = m_loader.watcher_hot_reload_plugin(name, it->second.path);
    auto end_time = std::chrono::steady_clock::now();

    if (success) {
        emit_event(PluginEventType::ReloadSucceeded, name, it->second.path);

        std::lock_guard stats_lock(m_stats_mutex);
        ++m_stats.hot_reloads;

        // Update average reload time
        auto reload_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (m_stats.hot_reloads == 1) {
            m_stats.average_reload_time = reload_time;
        } else {
            m_stats.average_reload_time = std::chrono::milliseconds(
                (m_stats.average_reload_time.count() * (m_stats.hot_reloads - 1) + reload_time.count())
                / m_stats.hot_reloads
            );
        }
    } else {
        emit_event(PluginEventType::ReloadFailed, name, it->second.path, "Hot-reload failed");

        std::lock_guard stats_lock(m_stats_mutex);
        ++m_stats.hot_reload_failures;
    }

    return success;
}

bool PluginWatcher::rebuild_plugin(const std::string& name) {
    std::lock_guard lock(m_mutex);

    auto it = m_plugins.find(name);
    if (it == m_plugins.end()) {
        emit_event(PluginEventType::BuildFailed, name, {}, "Plugin not found");
        return false;
    }

    if (!trigger_build(name)) {
        return false;
    }

    // After successful build, reload
    return reload_plugin(name);
}

std::optional<PluginFileInfo> PluginWatcher::get_plugin_info(const std::string& name) const {
    std::lock_guard lock(m_mutex);
    auto it = m_plugins.find(name);
    if (it != m_plugins.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PluginFileInfo> PluginWatcher::all_plugins() const {
    std::lock_guard lock(m_mutex);
    std::vector<PluginFileInfo> result;
    result.reserve(m_plugins.size());
    for (const auto& [name, info] : m_plugins) {
        result.push_back(info);
    }
    return result;
}

void PluginWatcher::on_event(PluginEventCallback callback) {
    std::lock_guard lock(m_event_mutex);
    m_event_callbacks.push_back(std::move(callback));
}

std::vector<PluginEvent> PluginWatcher::recent_events(std::size_t count) const {
    std::lock_guard lock(m_event_mutex);
    std::size_t start = m_event_history.size() > count ? m_event_history.size() - count : 0;
    return std::vector<PluginEvent>(m_event_history.begin() + start, m_event_history.end());
}

PluginWatcher::Stats PluginWatcher::stats() const {
    std::lock_guard lock(m_stats_mutex);
    return m_stats;
}

// =============================================================================
// Private Methods
// =============================================================================

void PluginWatcher::watch_thread() {
    while (m_running.load()) {
        // Check for manual scan request
        if (m_scan_requested.exchange(false)) {
            scan_directories();
        }

        // Check for file changes
        check_file_changes();

        // Process pending reloads (after debounce)
        process_pending_reloads();

        // Sleep for poll interval
        std::this_thread::sleep_for(m_config.poll_interval);
    }
}

void PluginWatcher::scan_directories() {
    std::lock_guard lock(m_mutex);

    for (const auto& watch_path : m_config.watch_paths) {
        if (!std::filesystem::exists(watch_path)) {
            continue;
        }

        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(watch_path, ec)) {
            if (ec) continue;

            if (!entry.is_regular_file()) continue;

            const auto& path = entry.path();

            // Check for plugin files
            if (is_plugin_file(path)) {
                std::string name = extract_plugin_name(path);

                if (m_plugins.find(name) == m_plugins.end()) {
                    // New plugin discovered
                    PluginFileInfo info;
                    info.path = path;
                    info.name = name;
                    info.last_modified = entry.last_write_time();
                    info.file_size = entry.file_size();
                    info.loaded = false;

                    m_plugins[name] = info;

                    emit_event(PluginEventType::Discovered, name, path);

                    {
                        std::lock_guard stats_lock(m_stats_mutex);
                        ++m_stats.plugins_discovered;
                    }

                    // Auto-load if configured
                    if (m_config.auto_load_new) {
                        emit_event(PluginEventType::LoadStarted, name, path);

                        if (m_loader.watcher_load_plugin(path)) {
                            m_plugins[name].loaded = true;
                            emit_event(PluginEventType::LoadSucceeded, name, path);

                            std::lock_guard stats_lock(m_stats_mutex);
                            ++m_stats.plugins_loaded;
                        } else {
                            emit_event(PluginEventType::LoadFailed, name, path);
                        }
                    }
                }
            }

            // Check for source files (map to plugin for recompilation)
            if (m_config.watch_sources && is_source_file(path)) {
                // Try to determine which plugin this source belongs to
                // Convention: source in plugins/name/ belongs to plugin "name"
                auto parent = path.parent_path();
                while (parent != watch_path && !parent.empty()) {
                    std::string potential_name = parent.filename().string();
                    if (m_plugins.find(potential_name) != m_plugins.end()) {
                        m_source_to_plugin[path] = potential_name;
                        m_plugins[potential_name].source_path = parent;
                        break;
                    }
                    parent = parent.parent_path();
                }
            }
        }
    }

    {
        std::lock_guard stats_lock(m_stats_mutex);
        m_stats.last_scan = std::chrono::steady_clock::now();
    }
}

void PluginWatcher::check_file_changes() {
    std::lock_guard lock(m_mutex);

    for (auto& [name, info] : m_plugins) {
        if (!std::filesystem::exists(info.path)) {
            if (info.loaded) {
                emit_event(PluginEventType::Removed, name, info.path);

                // Unload removed plugin
                emit_event(PluginEventType::UnloadStarted, name, info.path);
                if (m_loader.watcher_unload_plugin(name)) {
                    info.loaded = false;
                    emit_event(PluginEventType::UnloadSucceeded, name, info.path);

                    std::lock_guard stats_lock(m_stats_mutex);
                    ++m_stats.plugins_unloaded;
                }
            }
            continue;
        }

        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(info.path, ec);
        if (ec) continue;

        if (current_time != info.last_modified) {
            // File changed
            info.last_modified = current_time;
            info.file_size = std::filesystem::file_size(info.path, ec);

            if (!info.pending_reload) {
                info.pending_reload = true;
                info.change_detected = std::chrono::steady_clock::now();

                emit_event(PluginEventType::Modified, name, info.path);
            }
        }
    }

    // Check source files for changes
    if (m_config.watch_sources) {
        for (const auto& [source_path, plugin_name] : m_source_to_plugin) {
            if (!std::filesystem::exists(source_path)) continue;

            auto plugin_it = m_plugins.find(plugin_name);
            if (plugin_it == m_plugins.end()) continue;

            // For simplicity, mark plugin for rebuild if any source changed
            // A production implementation would track per-source timestamps
            std::error_code ec;
            auto source_time = std::filesystem::last_write_time(source_path, ec);
            if (ec) continue;

            // If source is newer than plugin binary, needs rebuild
            if (source_time > plugin_it->second.last_modified) {
                if (!plugin_it->second.pending_reload) {
                    plugin_it->second.pending_reload = true;
                    plugin_it->second.change_detected = std::chrono::steady_clock::now();

                    emit_event(PluginEventType::Modified, plugin_name, source_path,
                              "Source file changed, rebuild required");
                }
            }
        }
    }
}

void PluginWatcher::process_pending_reloads() {
    std::lock_guard lock(m_mutex);

    auto now = std::chrono::steady_clock::now();

    for (auto& [name, info] : m_plugins) {
        if (!info.pending_reload) continue;

        // Check if debounce time has passed
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - info.change_detected);

        if (elapsed < m_config.debounce_time) continue;

        info.pending_reload = false;

        if (!m_config.auto_reload_changed) continue;

        // Check if rebuild is needed (source changed)
        bool needs_build = false;
        if (m_config.watch_sources && !info.source_path.empty()) {
            for (const auto& [source_path, plugin_name] : m_source_to_plugin) {
                if (plugin_name != name) continue;

                std::error_code ec;
                auto source_time = std::filesystem::last_write_time(source_path, ec);
                if (!ec && source_time > info.last_modified) {
                    needs_build = true;
                    break;
                }
            }
        }

        if (needs_build) {
            if (!trigger_build(name)) {
                continue;  // Build failed, skip reload
            }
        }

        // Perform hot-reload
        if (info.loaded) {
            emit_event(PluginEventType::ReloadStarted, name, info.path);

            auto start_time = std::chrono::steady_clock::now();
            bool success = m_loader.watcher_hot_reload_plugin(name, info.path);
            auto end_time = std::chrono::steady_clock::now();

            if (success) {
                emit_event(PluginEventType::ReloadSucceeded, name, info.path);

                std::lock_guard stats_lock(m_stats_mutex);
                ++m_stats.hot_reloads;

                auto reload_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);
                if (m_stats.hot_reloads == 1) {
                    m_stats.average_reload_time = reload_time;
                } else {
                    m_stats.average_reload_time = std::chrono::milliseconds(
                        (m_stats.average_reload_time.count() * (m_stats.hot_reloads - 1)
                         + reload_time.count()) / m_stats.hot_reloads);
                }
            } else {
                emit_event(PluginEventType::ReloadFailed, name, info.path);

                std::lock_guard stats_lock(m_stats_mutex);
                ++m_stats.hot_reload_failures;
            }
        }
    }
}

bool PluginWatcher::trigger_build(const std::string& plugin_name) {
    if (m_config.build_command.empty()) {
        emit_event(PluginEventType::BuildFailed, plugin_name, {},
                  "No build command configured");
        return false;
    }

    auto it = m_plugins.find(plugin_name);
    if (it == m_plugins.end()) {
        emit_event(PluginEventType::BuildFailed, plugin_name, {}, "Plugin not found");
        return false;
    }

    emit_event(PluginEventType::BuildStarted, plugin_name, it->second.path);

    {
        std::lock_guard stats_lock(m_stats_mutex);
        ++m_stats.builds_triggered;
    }

    // Substitute placeholders in build command
    std::string cmd = m_config.build_command;
    std::string plugin_placeholder = "{plugin}";
    std::string source_placeholder = "{source}";

    auto pos = cmd.find(plugin_placeholder);
    if (pos != std::string::npos) {
        cmd.replace(pos, plugin_placeholder.length(), plugin_name);
    }

    pos = cmd.find(source_placeholder);
    if (pos != std::string::npos) {
        cmd.replace(pos, source_placeholder.length(), it->second.source_path.string());
    }

    // Execute build command
    int result = std::system(cmd.c_str());

    if (result == 0) {
        emit_event(PluginEventType::BuildSucceeded, plugin_name, it->second.path);

        // Update last modified time
        std::error_code ec;
        it->second.last_modified = std::filesystem::last_write_time(it->second.path, ec);

        return true;
    } else {
        emit_event(PluginEventType::BuildFailed, plugin_name, it->second.path,
                  "Build command returned " + std::to_string(result));

        std::lock_guard stats_lock(m_stats_mutex);
        ++m_stats.build_failures;

        return false;
    }
}

void PluginWatcher::emit_event(PluginEventType type, const std::string& name,
                               const std::filesystem::path& path, const std::string& message) {
    PluginEvent event;
    event.type = type;
    event.plugin_name = name;
    event.plugin_path = path;
    event.message = message;
    event.timestamp = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(m_event_mutex);

        // Store in history
        m_event_history.push_back(event);
        if (m_event_history.size() > MAX_EVENT_HISTORY) {
            m_event_history.erase(m_event_history.begin());
        }

        // Notify callbacks
        for (const auto& callback : m_event_callbacks) {
            callback(event);
        }
    }
}

std::string PluginWatcher::extract_plugin_name(const std::filesystem::path& path) const {
    std::string filename = path.stem().string();

    // Remove common prefixes
    if (filename.starts_with("lib")) {
        filename = filename.substr(3);
    }

    // Remove version suffixes like _d, _debug, etc.
    for (const auto& suffix : {"_d", "_debug", "_release", "_r"}) {
        if (filename.ends_with(suffix)) {
            filename = filename.substr(0, filename.length() - std::strlen(suffix));
            break;
        }
    }

    return filename;
}

bool PluginWatcher::is_plugin_file(const std::filesystem::path& path) const {
    std::string ext = path.extension().string();

    // First check configured extensions
    for (const auto& plugin_ext : m_config.plugin_extensions) {
        if (ext == plugin_ext) {
            return true;
        }
    }

    // If cross-platform is enabled, check all known extensions
    if (m_config.accept_cross_platform) {
        auto all_exts = all_plugin_extensions();
        for (const auto& plugin_ext : all_exts) {
            if (ext == plugin_ext) {
                return true;
            }
        }
    }

    return false;
}

bool PluginWatcher::is_source_file(const std::filesystem::path& path) const {
    std::string ext = path.extension().string();
    for (const auto& source_ext : m_config.source_extensions) {
        if (ext == source_ext) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// PluginStateRegistry Implementation
// =============================================================================

IPluginState* PluginStateRegistry::get_state(const std::string& plugin_name,
                                              const std::string& type_id) {
    std::lock_guard lock(m_mutex);

    auto plugin_it = m_plugin_states.find(plugin_name);
    if (plugin_it == m_plugin_states.end()) {
        return nullptr;
    }

    auto state_it = plugin_it->second.find(type_id);
    if (state_it == plugin_it->second.end()) {
        return nullptr;
    }

    return state_it->second.get();
}

std::unordered_map<std::string, std::vector<std::uint8_t>>
PluginStateRegistry::snapshot_plugin(const std::string& plugin_name) {
    std::lock_guard lock(m_mutex);
    std::unordered_map<std::string, std::vector<std::uint8_t>> result;

    auto plugin_it = m_plugin_states.find(plugin_name);
    if (plugin_it == m_plugin_states.end()) {
        return result;
    }

    for (const auto& [type_id, state] : plugin_it->second) {
        result[type_id] = state->serialize();
    }

    return result;
}

void PluginStateRegistry::restore_plugin(
    const std::string& plugin_name,
    const std::unordered_map<std::string, std::vector<std::uint8_t>>& data) {
    std::lock_guard lock(m_mutex);

    auto plugin_it = m_plugin_states.find(plugin_name);
    if (plugin_it == m_plugin_states.end()) {
        return;
    }

    for (const auto& [type_id, serialized] : data) {
        auto state_it = plugin_it->second.find(type_id);
        if (state_it != plugin_it->second.end()) {
            state_it->second->deserialize(serialized);
        }
    }
}

void PluginStateRegistry::clear_plugin(const std::string& plugin_name) {
    std::lock_guard lock(m_mutex);

    auto plugin_it = m_plugin_states.find(plugin_name);
    if (plugin_it == m_plugin_states.end()) {
        return;
    }

    for (auto& [type_id, state] : plugin_it->second) {
        state->clear();
    }
}

void PluginStateRegistry::unregister_plugin(const std::string& plugin_name) {
    std::lock_guard lock(m_mutex);
    m_plugin_states.erase(plugin_name);
}

std::vector<std::string> PluginStateRegistry::registered_plugins() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::string> result;
    result.reserve(m_plugin_states.size());
    for (const auto& [name, _] : m_plugin_states) {
        result.push_back(name);
    }
    return result;
}

std::vector<std::string> PluginStateRegistry::state_types(const std::string& plugin_name) const {
    std::lock_guard lock(m_mutex);
    std::vector<std::string> result;

    auto plugin_it = m_plugin_states.find(plugin_name);
    if (plugin_it != m_plugin_states.end()) {
        result.reserve(plugin_it->second.size());
        for (const auto& [type_id, _] : plugin_it->second) {
            result.push_back(type_id);
        }
    }

    return result;
}

} // namespace void_plugin_api
