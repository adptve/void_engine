/// @file module_loader.cpp
/// @brief Dynamic module loading implementation

#include <void_engine/kernel/module_loader.hpp>

#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_set>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace void_kernel {

// =============================================================================
// Platform-specific library extension
// =============================================================================

#ifdef _WIN32
    static constexpr const char* LIBRARY_EXTENSION = ".dll";
#elif defined(__APPLE__)
    static constexpr const char* LIBRARY_EXTENSION = ".dylib";
#else
    static constexpr const char* LIBRARY_EXTENSION = ".so";
#endif

// =============================================================================
// ModuleHandle Implementation
// =============================================================================

ModuleHandle::ModuleHandle(void* handle, std::filesystem::path path)
    : m_handle(handle), m_path(std::move(path)) {}

ModuleHandle::~ModuleHandle() {
    unload();
}

ModuleHandle::ModuleHandle(ModuleHandle&& other) noexcept
    : m_handle(other.m_handle), m_path(std::move(other.m_path)) {
    other.m_handle = nullptr;
}

ModuleHandle& ModuleHandle::operator=(ModuleHandle&& other) noexcept {
    if (this != &other) {
        unload();
        m_handle = other.m_handle;
        m_path = std::move(other.m_path);
        other.m_handle = nullptr;
    }
    return *this;
}

bool ModuleHandle::is_valid() const {
    return m_handle != nullptr;
}

void* ModuleHandle::get_symbol(const char* name) const {
    if (!m_handle) return nullptr;

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(
        static_cast<HMODULE>(m_handle), name));
#else
    return dlsym(m_handle, name);
#endif
}

void_core::Result<ModuleHandle> ModuleHandle::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return void_core::Error{"Module file not found: " + path.string()};
    }

#ifdef _WIN32
    // Convert to wide string for Windows
    std::wstring wpath = path.wstring();
    HMODULE handle = LoadLibraryW(wpath.c_str());

    if (!handle) {
        DWORD error = GetLastError();
        return void_core::Error{
            "Failed to load module: " + path.string() +
            " (error code: " + std::to_string(error) + ")"
        };
    }
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);

    if (!handle) {
        return void_core::Error{
            "Failed to load module: " + path.string() +
            " (" + std::string(dlerror()) + ")"
        };
    }
#endif

    return ModuleHandle(handle, path);
}

void ModuleHandle::unload() {
    if (m_handle) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(m_handle));
#else
        dlclose(m_handle);
#endif
        m_handle = nullptr;
    }
}

// =============================================================================
// ModuleLoader Implementation
// =============================================================================

struct ModuleLoader::Impl {
    std::vector<std::filesystem::path> search_paths;
    std::unordered_map<ModuleId, LoadedModule> modules;
    std::unordered_map<std::string, ModuleId> name_to_id;
    bool hot_reload_enabled = true;

    mutable std::mutex mutex;

    // Callbacks
    ModuleCallback on_loaded;
    ModuleCallback on_unloaded;
    ModuleCallback on_reloaded;
    std::function<void(const std::string&, const std::string&)> on_load_failed;

    // Find module file in search paths
    std::optional<std::filesystem::path> find_module(const std::string& name) const {
        std::string filename = name + LIBRARY_EXTENSION;

        for (const auto& path : search_paths) {
            auto full_path = path / filename;
            if (std::filesystem::exists(full_path)) {
                return full_path;
            }
        }

        return std::nullopt;
    }

    // Get module info from loaded module
    std::optional<ModuleInfo> get_module_info(IModule* module) const {
        if (!module) return std::nullopt;
        return module->info();
    }
};

ModuleLoader::ModuleLoader() : m_impl(std::make_unique<Impl>()) {
    // Add default search paths
    m_impl->search_paths.push_back(".");
    m_impl->search_paths.push_back("modules");
    m_impl->search_paths.push_back("plugins");
}

ModuleLoader::~ModuleLoader() {
    unload_all();
}

void ModuleLoader::set_search_paths(const std::vector<std::filesystem::path>& paths) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->search_paths = paths;
}

void ModuleLoader::add_search_path(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->search_paths.push_back(path);
}

const std::vector<std::filesystem::path>& ModuleLoader::search_paths() const {
    return m_impl->search_paths;
}

void ModuleLoader::set_hot_reload_enabled(bool enabled) {
    m_impl->hot_reload_enabled = enabled;
}

bool ModuleLoader::is_hot_reload_enabled() const {
    return m_impl->hot_reload_enabled;
}

void_core::Result<ModuleId> ModuleLoader::load_module(const std::string& name) {
    auto path = m_impl->find_module(name);
    if (!path) {
        std::string error = "Module not found: " + name;
        if (m_impl->on_load_failed) {
            m_impl->on_load_failed(name, error);
        }
        return void_core::Error{error};
    }
    return load_module_from_path(*path);
}

void_core::Result<ModuleId> ModuleLoader::load_module_from_path(const std::filesystem::path& path) {
    auto start_time = std::chrono::steady_clock::now();

    // Load the library
    auto handle_result = ModuleHandle::load(path);
    if (!handle_result) {
        std::string name = path.stem().string();
        if (m_impl->on_load_failed) {
            m_impl->on_load_failed(name, handle_result.error().message());
        }
        return void_core::Error{handle_result.error()};
    }

    auto handle = std::move(*handle_result);

    // Get entry point
    auto* entry = handle.get_symbol_as<ModuleEntryPoint*>(ModuleEntryPoint::SYMBOL_NAME);
    if (!entry) {
        std::string name = path.stem().string();
        std::string error = "Module missing entry point: " + name;
        if (m_impl->on_load_failed) {
            m_impl->on_load_failed(name, error);
        }
        return void_core::Error{error};
    }

    // Verify API version
    if (entry->api_version != 1) {
        std::string name = path.stem().string();
        std::string error = "Module API version mismatch: " + name;
        if (m_impl->on_load_failed) {
            m_impl->on_load_failed(name, error);
        }
        return void_core::Error{error};
    }

    // Create module instance
    IModule* module_ptr = entry->create();
    if (!module_ptr) {
        std::string name = path.stem().string();
        std::string error = "Module creation failed: " + name;
        if (m_impl->on_load_failed) {
            m_impl->on_load_failed(name, error);
        }
        return void_core::Error{error};
    }

    // Create unique_ptr with custom deleter
    auto module = std::unique_ptr<IModule, void(*)(IModule*)>(module_ptr, entry->destroy);

    const auto& info = module->info();
    ModuleId id = ModuleId::from_name(info.name);

    auto end_time = std::chrono::steady_clock::now();
    auto load_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    // Store loaded module
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        // Check if already loaded
        if (m_impl->modules.find(id) != m_impl->modules.end()) {
            return void_core::Error{"Module already loaded: " + info.name};
        }

        LoadedModule loaded;
        loaded.id = id;
        loaded.name = info.name;
        loaded.handle = std::move(handle);
        loaded.instance = std::move(module);
        loaded.state = ModuleState::Loaded;
        loaded.last_modified = std::filesystem::last_write_time(path);
        loaded.load_time = std::chrono::steady_clock::now();

        m_impl->modules[id] = std::move(loaded);
        m_impl->name_to_id[info.name] = id;
    }

    // Callback
    if (m_impl->on_loaded) {
        m_impl->on_loaded(id, info.name);
    }

    return id;
}

void_core::Result<void> ModuleLoader::unload_module(ModuleId id) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->modules.find(id);
    if (it == m_impl->modules.end()) {
        return void_core::Error{"Module not found"};
    }

    auto& module = it->second;
    std::string name = module.name;

    // Shutdown module if running
    if (module.instance && module.state >= ModuleState::Ready) {
        module.state = ModuleState::Stopping;
        module.instance->shutdown();
    }

    module.state = ModuleState::Unloading;

    // Remove from maps
    m_impl->name_to_id.erase(name);
    m_impl->modules.erase(it);

    // Callback
    if (m_impl->on_unloaded) {
        m_impl->on_unloaded(id, name);
    }

    return void_core::Ok();
}

void_core::Result<void> ModuleLoader::unload_module(const std::string& name) {
    auto id = get_module_id(name);
    if (!id) {
        return void_core::Error{"Module not found: " + name};
    }
    return unload_module(*id);
}

void_core::Result<void> ModuleLoader::reload_module(ModuleId id) {
    if (!m_impl->hot_reload_enabled) {
        return void_core::Error{"Hot-reload is disabled"};
    }

    std::unique_lock<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->modules.find(id);
    if (it == m_impl->modules.end()) {
        return void_core::Error{"Module not found"};
    }

    auto& module = it->second;

    // Check if module supports hot-reload
    if (!module.instance->supports_hot_reload()) {
        return void_core::Error{"Module does not support hot-reload: " + module.name};
    }

    auto path = module.handle.path();
    auto old_version = module.instance->info().version;

    // Prepare for reload (capture state)
    module.state = ModuleState::Reloading;
    auto snapshot_result = module.instance->prepare_reload();
    if (!snapshot_result) {
        module.state = ModuleState::Failed;
        return void_core::Error{"Failed to capture module state: " + snapshot_result.error().message()};
    }
    auto snapshot = std::move(*snapshot_result);

    // Release lock while reloading
    std::string name = module.name;
    lock.unlock();

    // Unload and reload
    auto unload_result = unload_module(id);
    if (!unload_result) {
        return void_core::Error{"Failed to unload module for reload: " + unload_result.error().message()};
    }

    auto reload_result = load_module_from_path(path);
    if (!reload_result) {
        return void_core::Error{"Failed to reload module: " + reload_result.error().message()};
    }

    // Restore state
    lock.lock();
    auto new_it = m_impl->modules.find(*reload_result);
    if (new_it != m_impl->modules.end()) {
        auto restore_result = new_it->second.instance->complete_reload(std::move(snapshot));
        if (!restore_result) {
            new_it->second.state = ModuleState::Failed;
            return void_core::Error{"Failed to restore module state: " + restore_result.error().message()};
        }

        new_it->second.reload_count++;
        auto new_version = new_it->second.instance->info().version;

        // Callback
        if (m_impl->on_reloaded) {
            m_impl->on_reloaded(*reload_result, name);
        }
    }

    return void_core::Ok();
}

void_core::Result<void> ModuleLoader::reload_module(const std::string& name) {
    auto id = get_module_id(name);
    if (!id) {
        return void_core::Error{"Module not found: " + name};
    }
    return reload_module(*id);
}

void ModuleLoader::unload_all() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Shutdown all modules in reverse load order
    std::vector<ModuleId> ids;
    ids.reserve(m_impl->modules.size());
    for (const auto& [id, module] : m_impl->modules) {
        ids.push_back(id);
    }

    // Reverse order
    std::reverse(ids.begin(), ids.end());

    for (const auto& id : ids) {
        auto it = m_impl->modules.find(id);
        if (it != m_impl->modules.end()) {
            auto& module = it->second;
            if (module.instance && module.state >= ModuleState::Ready) {
                module.instance->shutdown();
            }

            if (m_impl->on_unloaded) {
                m_impl->on_unloaded(id, module.name);
            }
        }
    }

    m_impl->modules.clear();
    m_impl->name_to_id.clear();
}

IModule* ModuleLoader::get_module(ModuleId id) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->modules.find(id);
    if (it != m_impl->modules.end()) {
        return it->second.instance.get();
    }
    return nullptr;
}

const IModule* ModuleLoader::get_module(ModuleId id) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->modules.find(id);
    if (it != m_impl->modules.end()) {
        return it->second.instance.get();
    }
    return nullptr;
}

IModule* ModuleLoader::get_module(const std::string& name) {
    auto id = get_module_id(name);
    if (!id) return nullptr;
    return get_module(*id);
}

const IModule* ModuleLoader::get_module(const std::string& name) const {
    auto id = get_module_id(name);
    if (!id) return nullptr;
    return get_module(*id);
}

std::optional<ModuleId> ModuleLoader::get_module_id(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->name_to_id.find(name);
    if (it != m_impl->name_to_id.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ModuleLoader::is_loaded(ModuleId id) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->modules.find(id) != m_impl->modules.end();
}

bool ModuleLoader::is_loaded(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->name_to_id.find(name) != m_impl->name_to_id.end();
}

ModuleState ModuleLoader::get_state(ModuleId id) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->modules.find(id);
    if (it != m_impl->modules.end()) {
        return it->second.state;
    }
    return ModuleState::Unloaded;
}

std::vector<ModuleId> ModuleLoader::loaded_modules() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    std::vector<ModuleId> ids;
    ids.reserve(m_impl->modules.size());
    for (const auto& [id, module] : m_impl->modules) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<std::string> ModuleLoader::loaded_module_names() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    std::vector<std::string> names;
    names.reserve(m_impl->name_to_id.size());
    for (const auto& [name, id] : m_impl->name_to_id) {
        names.push_back(name);
    }
    return names;
}

void ModuleLoader::poll_changes() {
    if (!m_impl->hot_reload_enabled) return;

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    for (auto& [id, module] : m_impl->modules) {
        if (!module.instance->supports_hot_reload()) continue;

        try {
            auto current_time = std::filesystem::last_write_time(module.handle.path());
            if (current_time != module.last_modified) {
                module.last_modified = current_time;

                // Queue for reload (can't reload while iterating)
                // In a real implementation, we'd queue this and process after iteration
            }
        } catch (const std::filesystem::filesystem_error&) {
            // File might be temporarily unavailable during recompile
        }
    }
}

void ModuleLoader::check_all_for_changes() {
    poll_changes();
}

std::vector<ModuleId> ModuleLoader::get_modified_modules() const {
    std::vector<ModuleId> modified;

    std::lock_guard<std::mutex> lock(m_impl->mutex);
    for (const auto& [id, module] : m_impl->modules) {
        if (!module.instance->supports_hot_reload()) continue;

        try {
            auto current_time = std::filesystem::last_write_time(module.handle.path());
            if (current_time != module.last_modified) {
                modified.push_back(id);
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Ignore
        }
    }

    return modified;
}

void ModuleLoader::set_on_loaded(ModuleCallback callback) {
    m_impl->on_loaded = std::move(callback);
}

void ModuleLoader::set_on_unloaded(ModuleCallback callback) {
    m_impl->on_unloaded = std::move(callback);
}

void ModuleLoader::set_on_reloaded(ModuleCallback callback) {
    m_impl->on_reloaded = std::move(callback);
}

void ModuleLoader::set_on_load_failed(std::function<void(const std::string&, const std::string&)> callback) {
    m_impl->on_load_failed = std::move(callback);
}

void_core::Result<std::vector<std::string>> ModuleLoader::resolve_load_order(
    const std::vector<std::string>& module_names) const {

    // Build dependency graph
    std::unordered_map<std::string, std::vector<std::string>> deps;
    std::unordered_map<std::string, int> in_degree;
    std::unordered_set<std::string> all_modules(module_names.begin(), module_names.end());

    for (const auto& name : module_names) {
        auto* module = get_module(name);
        if (module) {
            const auto& info = module->info();
            deps[name] = info.dependencies;
            for (const auto& dep : info.dependencies) {
                all_modules.insert(dep);
            }
        } else {
            deps[name] = {};
        }
        in_degree[name] = 0;
    }

    // Calculate in-degrees
    for (const auto& [name, dependencies] : deps) {
        for (const auto& dep : dependencies) {
            in_degree[dep]++;
        }
    }

    // Topological sort using Kahn's algorithm
    std::queue<std::string> queue;
    for (const auto& name : module_names) {
        if (in_degree[name] == 0) {
            queue.push(name);
        }
    }

    std::vector<std::string> result;
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);

        for (const auto& dep : deps[current]) {
            in_degree[dep]--;
            if (in_degree[dep] == 0) {
                queue.push(dep);
            }
        }
    }

    if (result.size() != module_names.size()) {
        return void_core::Error{"Circular dependency detected in modules"};
    }

    // Reverse to get dependencies first
    std::reverse(result.begin(), result.end());
    return result;
}

bool ModuleLoader::dependencies_satisfied(const std::string& module_name) const {
    auto* module = get_module(module_name);
    if (!module) return true;

    const auto& info = module->info();
    for (const auto& dep : info.dependencies) {
        if (!is_loaded(dep)) return false;
    }
    return true;
}

// =============================================================================
// ModuleRegistry Implementation
// =============================================================================

struct ModuleRegistry::Impl {
    std::unordered_map<std::string, std::unique_ptr<IModule>> modules;
    std::vector<std::string> load_order;
    mutable std::mutex mutex;
};

ModuleRegistry::ModuleRegistry() : m_impl(std::make_unique<Impl>()) {}

ModuleRegistry::~ModuleRegistry() {
    shutdown_all();
}

void_core::Result<void> ModuleRegistry::register_module(std::unique_ptr<IModule> module) {
    if (!module) {
        return void_core::Error{"Cannot register null module"};
    }

    const auto& info = module->info();

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    if (m_impl->modules.find(info.name) != m_impl->modules.end()) {
        return void_core::Error{"Module already registered: " + info.name};
    }

    m_impl->load_order.push_back(info.name);
    m_impl->modules[info.name] = std::move(module);

    return void_core::Ok();
}

void_core::Result<void> ModuleRegistry::unregister_module(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->modules.find(name);
    if (it == m_impl->modules.end()) {
        return void_core::Error{"Module not found: " + name};
    }

    it->second->shutdown();
    m_impl->modules.erase(it);

    // Remove from load order
    auto order_it = std::find(m_impl->load_order.begin(), m_impl->load_order.end(), name);
    if (order_it != m_impl->load_order.end()) {
        m_impl->load_order.erase(order_it);
    }

    return void_core::Ok();
}

IModule* ModuleRegistry::get_module(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->modules.find(name);
    if (it != m_impl->modules.end()) {
        return it->second.get();
    }
    return nullptr;
}

const IModule* ModuleRegistry::get_module(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->modules.find(name);
    if (it != m_impl->modules.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ModuleRegistry::has_module(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->modules.find(name) != m_impl->modules.end();
}

std::vector<std::string> ModuleRegistry::module_names() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->load_order;
}

void_core::Result<void> ModuleRegistry::initialize_all() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    for (const auto& name : m_impl->load_order) {
        auto it = m_impl->modules.find(name);
        if (it != m_impl->modules.end()) {
            auto result = it->second->initialize();
            if (!result) {
                return void_core::Error{
                    "Failed to initialize module " + name + ": " + result.error().message()
                };
            }
        }
    }

    return void_core::Ok();
}

void ModuleRegistry::shutdown_all() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Shutdown in reverse order
    for (auto it = m_impl->load_order.rbegin(); it != m_impl->load_order.rend(); ++it) {
        auto mod_it = m_impl->modules.find(*it);
        if (mod_it != m_impl->modules.end()) {
            mod_it->second->shutdown();
        }
    }
}

void ModuleRegistry::update_all(float dt) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    for (const auto& name : m_impl->load_order) {
        auto it = m_impl->modules.find(name);
        if (it != m_impl->modules.end()) {
            it->second->update(dt);
        }
    }
}

std::size_t ModuleRegistry::count() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->modules.size();
}

std::size_t ModuleRegistry::initialized_count() const {
    // In a full implementation, we'd track initialization state
    return count();
}

} // namespace void_kernel
