/// @file module_loader.hpp
/// @brief Dynamic module loading with hot-reload support
///
/// Provides platform-agnostic dynamic library loading with:
/// - Hot-reload support via file watching
/// - Symbol resolution and type-safe function pointers
/// - Module lifecycle management
/// - Dependency resolution

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_kernel {

// =============================================================================
// Module Interface
// =============================================================================

/// Module interface that all loadable modules must implement
class IModule {
public:
    virtual ~IModule() = default;

    /// Get module information
    [[nodiscard]] virtual const ModuleInfo& info() const = 0;

    /// Initialize the module
    [[nodiscard]] virtual void_core::Result<void> initialize() = 0;

    /// Shutdown the module
    virtual void shutdown() = 0;

    /// Update the module (called each frame if active)
    virtual void update(float dt) {}

    /// Check if module supports hot-reload
    [[nodiscard]] virtual bool supports_hot_reload() const { return false; }

    /// Prepare for hot-reload (save state)
    [[nodiscard]] virtual void_core::Result<void_core::HotReloadSnapshot> prepare_reload() {
        return void_core::HotReloadSnapshot{};
    }

    /// Complete hot-reload (restore state)
    [[nodiscard]] virtual void_core::Result<void> complete_reload(void_core::HotReloadSnapshot snapshot) {
        return void_core::Ok();
    }
};

/// Module factory function signature
using ModuleFactoryFn = IModule* (*)();

/// Module destroy function signature
using ModuleDestroyFn = void (*)(IModule*);

/// Module entry point structure (exported by modules)
struct ModuleEntryPoint {
    static constexpr const char* SYMBOL_NAME = "void_module_entry";

    const char* name;
    std::uint32_t api_version;
    ModuleFactoryFn create;
    ModuleDestroyFn destroy;
};

// =============================================================================
// Module Handle
// =============================================================================

/// Platform-specific module handle wrapping native library handle
class ModuleHandle {
public:
    ModuleHandle() = default;
    ~ModuleHandle();

    // Non-copyable, movable
    ModuleHandle(const ModuleHandle&) = delete;
    ModuleHandle& operator=(const ModuleHandle&) = delete;
    ModuleHandle(ModuleHandle&& other) noexcept;
    ModuleHandle& operator=(ModuleHandle&& other) noexcept;

    /// Check if handle is valid
    [[nodiscard]] bool is_valid() const;

    /// Get symbol address
    [[nodiscard]] void* get_symbol(const char* name) const;

    /// Get symbol as typed function pointer
    template<typename T>
    [[nodiscard]] T get_symbol_as(const char* name) const {
        return reinterpret_cast<T>(get_symbol(name));
    }

    /// Get native handle
    [[nodiscard]] void* native_handle() const { return m_handle; }

    /// Get library path
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    /// Load library from path
    [[nodiscard]] static void_core::Result<ModuleHandle> load(const std::filesystem::path& path);

    /// Unload library
    void unload();

private:
    explicit ModuleHandle(void* handle, std::filesystem::path path);

    void* m_handle = nullptr;
    std::filesystem::path m_path;
};

// =============================================================================
// Loaded Module
// =============================================================================

/// Represents a loaded module with its handle and instance
struct LoadedModule {
    ModuleId id;
    std::string name;
    ModuleHandle handle;
    std::unique_ptr<IModule, void(*)(IModule*)> instance;
    ModuleState state = ModuleState::Unloaded;
    std::filesystem::file_time_type last_modified;
    std::chrono::steady_clock::time_point load_time;
    std::uint32_t reload_count = 0;

    LoadedModule() : instance(nullptr, nullptr) {}
};

// =============================================================================
// Module Loader
// =============================================================================

/// Dynamic module loader with hot-reload support
class ModuleLoader {
public:
    /// Callback for module events
    using ModuleCallback = std::function<void(const ModuleId&, const std::string&)>;

    ModuleLoader();
    ~ModuleLoader();

    // Non-copyable
    ModuleLoader(const ModuleLoader&) = delete;
    ModuleLoader& operator=(const ModuleLoader&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Set module search paths
    void set_search_paths(const std::vector<std::filesystem::path>& paths);

    /// Add a search path
    void add_search_path(const std::filesystem::path& path);

    /// Get search paths
    [[nodiscard]] const std::vector<std::filesystem::path>& search_paths() const;

    /// Set hot-reload enabled
    void set_hot_reload_enabled(bool enabled);

    /// Check if hot-reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const;

    // =========================================================================
    // Module Loading
    // =========================================================================

    /// Load a module by name (searches paths)
    [[nodiscard]] void_core::Result<ModuleId> load_module(const std::string& name);

    /// Load a module from a specific path
    [[nodiscard]] void_core::Result<ModuleId> load_module_from_path(const std::filesystem::path& path);

    /// Unload a module
    [[nodiscard]] void_core::Result<void> unload_module(ModuleId id);

    /// Unload a module by name
    [[nodiscard]] void_core::Result<void> unload_module(const std::string& name);

    /// Reload a module (hot-reload)
    [[nodiscard]] void_core::Result<void> reload_module(ModuleId id);

    /// Reload a module by name
    [[nodiscard]] void_core::Result<void> reload_module(const std::string& name);

    /// Unload all modules
    void unload_all();

    // =========================================================================
    // Module Queries
    // =========================================================================

    /// Get module by ID
    [[nodiscard]] IModule* get_module(ModuleId id);
    [[nodiscard]] const IModule* get_module(ModuleId id) const;

    /// Get module by name
    [[nodiscard]] IModule* get_module(const std::string& name);
    [[nodiscard]] const IModule* get_module(const std::string& name) const;

    /// Get module ID by name
    [[nodiscard]] std::optional<ModuleId> get_module_id(const std::string& name) const;

    /// Check if module is loaded
    [[nodiscard]] bool is_loaded(ModuleId id) const;
    [[nodiscard]] bool is_loaded(const std::string& name) const;

    /// Get module state
    [[nodiscard]] ModuleState get_state(ModuleId id) const;

    /// Get all loaded module IDs
    [[nodiscard]] std::vector<ModuleId> loaded_modules() const;

    /// Get all loaded module names
    [[nodiscard]] std::vector<std::string> loaded_module_names() const;

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Poll for file changes and reload modified modules
    void poll_changes();

    /// Force check all modules for changes
    void check_all_for_changes();

    /// Get modules that have been modified
    [[nodiscard]] std::vector<ModuleId> get_modified_modules() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Set callback for module loaded
    void set_on_loaded(ModuleCallback callback);

    /// Set callback for module unloaded
    void set_on_unloaded(ModuleCallback callback);

    /// Set callback for module reloaded
    void set_on_reloaded(ModuleCallback callback);

    /// Set callback for module load failed
    void set_on_load_failed(std::function<void(const std::string&, const std::string&)> callback);

    // =========================================================================
    // Dependency Resolution
    // =========================================================================

    /// Get load order respecting dependencies
    [[nodiscard]] void_core::Result<std::vector<std::string>> resolve_load_order(
        const std::vector<std::string>& module_names) const;

    /// Check if dependencies are satisfied for a module
    [[nodiscard]] bool dependencies_satisfied(const std::string& module_name) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Module Registry
// =============================================================================

/// Registry for managing loaded modules
class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();

    // Non-copyable
    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    // =========================================================================
    // Module Management
    // =========================================================================

    /// Register a module (takes ownership)
    void_core::Result<void> register_module(std::unique_ptr<IModule> module);

    /// Unregister a module
    void_core::Result<void> unregister_module(const std::string& name);

    /// Get module by name
    [[nodiscard]] IModule* get_module(const std::string& name);
    [[nodiscard]] const IModule* get_module(const std::string& name) const;

    /// Check if module is registered
    [[nodiscard]] bool has_module(const std::string& name) const;

    /// Get all registered module names
    [[nodiscard]] std::vector<std::string> module_names() const;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Initialize all modules in dependency order
    void_core::Result<void> initialize_all();

    /// Shutdown all modules in reverse order
    void shutdown_all();

    /// Update all active modules
    void update_all(float dt);

    // =========================================================================
    // Queries
    // =========================================================================

    /// Get module count
    [[nodiscard]] std::size_t count() const;

    /// Get initialized module count
    [[nodiscard]] std::size_t initialized_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Module Registration Helpers
// =============================================================================

/// Helper macro for defining module entry point
#define VOID_MODULE_ENTRY(ModuleClass) \
    extern "C" { \
        VOID_EXPORT void_kernel::ModuleEntryPoint void_module_entry = { \
            .name = #ModuleClass, \
            .api_version = 1, \
            .create = []() -> void_kernel::IModule* { return new ModuleClass(); }, \
            .destroy = [](void_kernel::IModule* m) { delete m; }, \
        }; \
    }

/// Platform-specific export macro
#if defined(_WIN32)
    #define VOID_EXPORT __declspec(dllexport)
#else
    #define VOID_EXPORT __attribute__((visibility("default")))
#endif

} // namespace void_kernel
