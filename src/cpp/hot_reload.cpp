/// @file hot_reload.cpp
/// @brief C++ hot reload system implementation

#include "hot_reload.hpp"

#include <void_engine/core/log.hpp>

#include <algorithm>
#include <fstream>
#include <regex>

namespace void_cpp {

// =============================================================================
// FileWatcher Implementation
// =============================================================================

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    stop();
}

WatcherId FileWatcher::watch(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    WatcherId id = WatcherId::create(next_watcher_id_++, 0);

    WatchEntry entry;
    entry.id = id;
    entry.path = path;
    entry.is_directory = std::filesystem::is_directory(path);
    entry.recursive = false;

    if (std::filesystem::exists(path)) {
        entry.last_time = std::filesystem::last_write_time(path);
        if (!entry.is_directory) {
            file_times_[path] = entry.last_time;
        }
    }

    watches_[id] = std::move(entry);

    return id;
}

WatcherId FileWatcher::watch_directory(const std::filesystem::path& dir, bool recursive) {
    std::lock_guard<std::mutex> lock(mutex_);

    WatcherId id = WatcherId::create(next_watcher_id_++, 0);

    WatchEntry entry;
    entry.id = id;
    entry.path = dir;
    entry.is_directory = true;
    entry.recursive = recursive;

    if (std::filesystem::exists(dir)) {
        entry.last_time = std::filesystem::last_write_time(dir);

        // Scan initial files
        if (recursive) {
            for (const auto& item : std::filesystem::recursive_directory_iterator(dir)) {
                if (item.is_regular_file() && matches_filters(item.path())) {
                    file_times_[item.path()] = item.last_write_time();
                }
            }
        } else {
            for (const auto& item : std::filesystem::directory_iterator(dir)) {
                if (item.is_regular_file() && matches_filters(item.path())) {
                    file_times_[item.path()] = item.last_write_time();
                }
            }
        }
    }

    watches_[id] = std::move(entry);

    return id;
}

void FileWatcher::unwatch(WatcherId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    watches_.erase(id);
}

void FileWatcher::unwatch_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    watches_.clear();
    file_times_.clear();
}

void FileWatcher::add_extension_filter(const std::string& ext) {
    extension_filters_.push_back(ext);
}

void FileWatcher::clear_extension_filters() {
    extension_filters_.clear();
}

void FileWatcher::set_ignore_patterns(const std::vector<std::string>& patterns) {
    ignore_patterns_ = patterns;
}

std::vector<FileChangeEvent> FileWatcher::poll() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<FileChangeEvent> events;

    // Track seen files for deletion detection
    std::unordered_set<std::filesystem::path> seen_files;

    for (const auto& [id, entry] : watches_) {
        if (!std::filesystem::exists(entry.path)) {
            continue;
        }

        if (entry.is_directory) {
            // Scan directory
            auto scan = [&](const std::filesystem::path& p) {
                if (!std::filesystem::is_regular_file(p)) return;
                if (!matches_filters(p)) return;
                if (matches_ignore(p)) return;

                seen_files.insert(p);

                auto it = file_times_.find(p);
                auto current_time = std::filesystem::last_write_time(p);

                if (it == file_times_.end()) {
                    // New file
                    FileChangeEvent event;
                    event.type = FileChangeType::Created;
                    event.path = p;
                    event.timestamp = std::chrono::system_clock::now();
                    events.push_back(std::move(event));
                    file_times_[p] = current_time;
                } else if (current_time > it->second) {
                    // Modified file
                    FileChangeEvent event;
                    event.type = FileChangeType::Modified;
                    event.path = p;
                    event.timestamp = std::chrono::system_clock::now();
                    events.push_back(std::move(event));
                    it->second = current_time;
                }
            };

            if (entry.recursive) {
                for (const auto& item : std::filesystem::recursive_directory_iterator(entry.path)) {
                    scan(item.path());
                }
            } else {
                for (const auto& item : std::filesystem::directory_iterator(entry.path)) {
                    scan(item.path());
                }
            }
        } else {
            // Single file
            seen_files.insert(entry.path);

            auto current_time = std::filesystem::last_write_time(entry.path);
            auto it = file_times_.find(entry.path);

            if (it == file_times_.end() || current_time > it->second) {
                FileChangeEvent event;
                event.type = (it == file_times_.end()) ? FileChangeType::Created : FileChangeType::Modified;
                event.path = entry.path;
                event.timestamp = std::chrono::system_clock::now();
                events.push_back(std::move(event));
                file_times_[entry.path] = current_time;
            }
        }
    }

    // Check for deletions
    std::vector<std::filesystem::path> to_remove;
    for (const auto& [path, time] : file_times_) {
        if (seen_files.find(path) == seen_files.end() && !std::filesystem::exists(path)) {
            FileChangeEvent event;
            event.type = FileChangeType::Deleted;
            event.path = path;
            event.timestamp = std::chrono::system_clock::now();
            events.push_back(std::move(event));
            to_remove.push_back(path);
        }
    }

    for (const auto& path : to_remove) {
        file_times_.erase(path);
    }

    return events;
}

void FileWatcher::set_callback(ChangeCallback callback) {
    callback_ = std::move(callback);
}

void FileWatcher::start(std::chrono::milliseconds poll_interval) {
    if (running_) return;

    poll_interval_ = poll_interval;
    running_ = true;
    watch_thread_ = std::thread(&FileWatcher::watch_thread, this);
}

void FileWatcher::stop() {
    if (!running_) return;

    running_ = false;
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
}

void FileWatcher::watch_thread() {
    while (running_) {
        auto events = poll();

        if (callback_) {
            for (const auto& event : events) {
                callback_(event);
            }
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_events_.insert(pending_events_.end(), events.begin(), events.end());
        }

        std::this_thread::sleep_for(poll_interval_);
    }
}

bool FileWatcher::matches_filters(const std::filesystem::path& path) const {
    if (extension_filters_.empty()) {
        return true;
    }

    std::string ext = path.extension().string();
    for (const auto& filter : extension_filters_) {
        if (ext == filter) {
            return true;
        }
    }

    return false;
}

bool FileWatcher::matches_ignore(const std::filesystem::path& path) const {
    std::string path_str = path.string();

    for (const auto& pattern : ignore_patterns_) {
        // Simple glob matching
        if (path_str.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

// =============================================================================
// StatePreserver Implementation
// =============================================================================

StatePreserver::StatePreserver() = default;
StatePreserver::~StatePreserver() = default;

void StatePreserver::unregister_state(const std::string& name) {
    states_.erase(name);
    saved_data_.erase(name);
}

void StatePreserver::clear() {
    states_.clear();
    saved_data_.clear();
}

void StatePreserver::save_all() {
    for (const auto& [name, entry] : states_) {
        save(name);
    }
}

void StatePreserver::restore_all() {
    for (const auto& [name, data] : saved_data_) {
        restore(name);
    }
}

void StatePreserver::save(const std::string& name) {
    auto it = states_.find(name);
    if (it == states_.end()) return;

    const auto& entry = it->second;
    std::vector<std::uint8_t> data(entry.size);

    entry.save_func(data.data(), entry.ptr, entry.size);

    if (save_callback_) {
        save_callback_(name, data);
    }

    saved_data_[name] = std::move(data);
}

void StatePreserver::restore(const std::string& name) {
    auto state_it = states_.find(name);
    if (state_it == states_.end()) return;

    auto data_it = saved_data_.find(name);
    if (data_it == saved_data_.end()) return;

    const auto& entry = state_it->second;
    const auto& data = data_it->second;

    if (restore_callback_) {
        restore_callback_(name, data);
    }

    entry.restore_func(entry.ptr, data.data(), entry.size);
}

CppResult<void> StatePreserver::save_to_file(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return CppError::IoError;
    }

    // Write number of entries
    std::uint32_t count = static_cast<std::uint32_t>(saved_data_.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [name, data] : saved_data_) {
        // Write name
        std::uint32_t name_len = static_cast<std::uint32_t>(name.size());
        file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file.write(name.data(), name_len);

        // Write data
        std::uint32_t data_len = static_cast<std::uint32_t>(data.size());
        file.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
        file.write(reinterpret_cast<const char*>(data.data()), data_len);
    }

    return CppResult<void>::ok();
}

CppResult<void> StatePreserver::restore_from_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return CppError::IoError;
    }

    std::uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (std::uint32_t i = 0; i < count; ++i) {
        // Read name
        std::uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));

        std::string name(name_len, '\0');
        file.read(name.data(), name_len);

        // Read data
        std::uint32_t data_len;
        file.read(reinterpret_cast<char*>(&data_len), sizeof(data_len));

        std::vector<std::uint8_t> data(data_len);
        file.read(reinterpret_cast<char*>(data.data()), data_len);

        saved_data_[name] = std::move(data);
    }

    restore_all();

    return CppResult<void>::ok();
}

// =============================================================================
// HotReloader Implementation
// =============================================================================

namespace {
    HotReloader* g_hot_reloader_instance = nullptr;
}

HotReloader::HotReloader()
    : compiler_(Compiler::instance())
    , registry_(ModuleRegistry::instance()) {
    g_hot_reloader_instance = this;

    // Setup file watcher
    file_watcher_.add_extension_filter(".cpp");
    file_watcher_.add_extension_filter(".hpp");
    file_watcher_.add_extension_filter(".h");
    file_watcher_.add_extension_filter(".cc");
    file_watcher_.add_extension_filter(".cxx");

    file_watcher_.set_callback([this](const FileChangeEvent& event) {
        on_file_changed(event);
    });
}

HotReloader::HotReloader(Compiler& compiler, ModuleRegistry& registry)
    : compiler_(compiler)
    , registry_(registry) {
    g_hot_reloader_instance = this;

    file_watcher_.add_extension_filter(".cpp");
    file_watcher_.add_extension_filter(".hpp");
    file_watcher_.add_extension_filter(".h");
    file_watcher_.add_extension_filter(".cc");
    file_watcher_.add_extension_filter(".cxx");

    file_watcher_.set_callback([this](const FileChangeEvent& event) {
        on_file_changed(event);
    });
}

HotReloader::~HotReloader() {
    stop();
    if (g_hot_reloader_instance == this) {
        g_hot_reloader_instance = nullptr;
    }
}

HotReloader& HotReloader::instance() {
    if (!g_hot_reloader_instance) {
        static HotReloader default_instance;
        g_hot_reloader_instance = &default_instance;
    }
    return *g_hot_reloader_instance;
}

void HotReloader::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) {
        start();
    } else {
        stop();
    }
}

void HotReloader::add_source_directory(const std::filesystem::path& dir) {
    source_directories_.push_back(dir);
    file_watcher_.watch_directory(dir, true);
}

void HotReloader::clear_source_directories() {
    source_directories_.clear();
    file_watcher_.unwatch_all();
}

void HotReloader::set_compiler_config(const CompilerConfig& config) {
    compiler_config_ = config;
}

void HotReloader::set_debounce_time(std::chrono::milliseconds time) {
    debounce_time_ = time;
    file_watcher_.set_debounce_time(time);
}

void HotReloader::register_module(ModuleId id, const std::vector<std::filesystem::path>& sources) {
    ModuleEntry entry;
    entry.id = id;
    entry.sources = sources;
    entry.last_compile_time = std::chrono::file_clock::now();

    registered_modules_[id] = std::move(entry);

    // Map sources to module
    for (const auto& source : sources) {
        source_to_module_[source] = id;
    }

    VOID_LOG_DEBUG("[HotReloader] Registered module {} with {} sources",
                  id.index(), sources.size());
}

void HotReloader::unregister_module(ModuleId id) {
    auto it = registered_modules_.find(id);
    if (it != registered_modules_.end()) {
        // Remove source mappings
        for (const auto& source : it->second.sources) {
            source_to_module_.erase(source);
        }
        registered_modules_.erase(it);
    }
}

std::vector<ModuleId> HotReloader::registered_modules() const {
    std::vector<ModuleId> result;
    result.reserve(registered_modules_.size());
    for (const auto& [id, entry] : registered_modules_) {
        result.push_back(id);
    }
    return result;
}

void HotReloader::set_pre_reload_callback(PreReloadCallback callback) {
    pre_reload_callback_ = std::move(callback);
}

void HotReloader::set_post_reload_callback(PostReloadCallback callback) {
    post_reload_callback_ = std::move(callback);
}

void HotReloader::set_reload_callback(ReloadCallback callback) {
    reload_callback_ = std::move(callback);
}

CppResult<void> HotReloader::reload(ModuleId module_id) {
    auto it = registered_modules_.find(module_id);
    if (it == registered_modules_.end()) {
        return CppError::ModuleNotFound;
    }

    ReloadContext context;
    context.module_id = module_id;
    context.start_time = std::chrono::system_clock::now();

    auto* module = registry_.get(module_id);
    if (module) {
        context.module_name = module->name();
        context.old_path = module->path();
    }

    return do_reload(module_id, context);
}

void HotReloader::reload_changed() {
    std::lock_guard<std::mutex> lock(pending_mutex_);

    for (auto id : pending_reloads_) {
        reload(id);
    }

    pending_reloads_.clear();
}

void HotReloader::poll() {
    if (!enabled_) return;

    // Process pending reloads with debounce
    auto now = std::chrono::steady_clock::now();

    std::vector<ModuleId> ready_to_reload;

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (const auto& [id, timestamp] : reload_timestamps_) {
            if (now - timestamp >= debounce_time_) {
                ready_to_reload.push_back(id);
            }
        }

        for (auto id : ready_to_reload) {
            reload_timestamps_.erase(id);
        }
    }

    for (auto id : ready_to_reload) {
        reload(id);
    }
}

void HotReloader::start() {
    file_watcher_.start(std::chrono::milliseconds(100));
    VOID_LOG_INFO("[HotReloader] Started file watching");
}

void HotReloader::stop() {
    file_watcher_.stop();
    VOID_LOG_INFO("[HotReloader] Stopped file watching");
}

void HotReloader::update() {
    poll();
}

void HotReloader::on_file_changed(const FileChangeEvent& event) {
    if (!enabled_) return;

    // Emit event
    if (event_bus_) {
        FileChangedEvent e;
        e.type = event.type;
        e.path = event.path;
        event_bus_->publish(e);
    }

    // Find module for this file
    auto it = source_to_module_.find(event.path);
    if (it == source_to_module_.end()) {
        // Check if it's a header that matches any module
        for (const auto& [id, entry] : registered_modules_) {
            for (const auto& source : entry.sources) {
                // If header is in same directory as a source
                if (source.parent_path() == event.path.parent_path()) {
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    pending_reloads_.insert(id);
                    reload_timestamps_[id] = std::chrono::steady_clock::now();
                    break;
                }
            }
        }
        return;
    }

    ModuleId module_id = it->second;

    VOID_LOG_DEBUG("[HotReloader] File changed: {} (module {})",
                  event.path.string(), module_id.index());

    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_reloads_.insert(module_id);
    reload_timestamps_[module_id] = std::chrono::steady_clock::now();
}

void HotReloader::process_pending_reloads() {
    reload_changed();
}

CppResult<void> HotReloader::do_reload(ModuleId module_id, const ReloadContext& context) {
    stats_.total_reloads++;

    auto* module = registry_.get(module_id);
    std::string module_name = module ? module->name() : "unknown";

    VOID_LOG_INFO("[HotReloader] Reloading module '{}'...", module_name);

    // Emit start event
    if (event_bus_) {
        ReloadStartedEvent event;
        event.module_id = module_id;
        event.module_name = module_name;
        event_bus_->publish(event);
    }

    // Save state
    void* saved_state = nullptr;
    if (pre_reload_callback_) {
        pre_reload_callback_(module_id, &saved_state);
    }
    state_preserver_.save_all();

    auto reload_start = std::chrono::steady_clock::now();

    // Get sources
    auto it = registered_modules_.find(module_id);
    if (it == registered_modules_.end()) {
        stats_.failed_reloads++;
        return CppError::ModuleNotFound;
    }

    const auto& sources = it->second.sources;

    // Compile
    auto compile_result = compiler_.compile(sources, module_name);
    if (!compile_result || !compile_result.value().success()) {
        VOID_LOG_ERROR("[HotReloader] Compilation failed for '{}'", module_name);

        if (event_bus_) {
            ReloadCompletedEvent event;
            event.module_id = module_id;
            event.module_name = module_name;
            event.success = false;
            event.error_message = "Compilation failed";
            event_bus_->publish(event);
        }

        stats_.failed_reloads++;
        return CppError::CompilationFailed;
    }

    auto compile_end = std::chrono::steady_clock::now();
    auto compile_time = std::chrono::duration_cast<std::chrono::milliseconds>(compile_end - reload_start);

    // Unload old module
    if (module) {
        registry_.unload(module_id);
    }

    // Load new module
    auto load_result = registry_.load(module_name, compile_result.value().output_path);
    if (!load_result) {
        VOID_LOG_ERROR("[HotReloader] Failed to load recompiled module '{}'", module_name);

        if (event_bus_) {
            ReloadCompletedEvent event;
            event.module_id = module_id;
            event.module_name = module_name;
            event.success = false;
            event.error_message = "Load failed";
            event_bus_->publish(event);
        }

        stats_.failed_reloads++;
        return CppError::LoadFailed;
    }

    auto reload_end = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(reload_end - reload_start);

    // Restore state
    state_preserver_.restore_all();
    if (post_reload_callback_) {
        post_reload_callback_(module_id, saved_state);
    }

    // Update entry
    it->second.last_compile_time = std::chrono::file_clock::now();

    // Emit completion event
    if (event_bus_) {
        ReloadCompletedEvent event;
        event.module_id = module_id;
        event.module_name = module_name;
        event.success = true;
        event.total_time = total_time;
        event_bus_->publish(event);
    }

    // Callback
    if (reload_callback_) {
        reload_callback_(module_id, true);
    }

    stats_.successful_reloads++;
    stats_.total_reload_time += total_time;
    stats_.average_reload_time = std::chrono::milliseconds(
        stats_.total_reload_time.count() / stats_.total_reloads);

    VOID_LOG_INFO("[HotReloader] Successfully reloaded '{}' ({}ms compile, {}ms total)",
                  module_name, compile_time.count(), total_time.count());

    return CppResult<void>::ok();
}

} // namespace void_cpp
