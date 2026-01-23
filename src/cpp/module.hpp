#pragma once

/// @file module.hpp
/// @brief Dynamic module loading and management

#include "types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_cpp {

// =============================================================================
// Dynamic Module
// =============================================================================

/// @brief A dynamically loaded module (DLL/SO)
class DynamicModule {
public:
    DynamicModule(ModuleId id, std::string name);
    ~DynamicModule();

    // Non-copyable, movable
    DynamicModule(const DynamicModule&) = delete;
    DynamicModule& operator=(const DynamicModule&) = delete;
    DynamicModule(DynamicModule&&) noexcept;
    DynamicModule& operator=(DynamicModule&&) noexcept;

    // Identity
    [[nodiscard]] ModuleId id() const { return id_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

    // State
    [[nodiscard]] ModuleState state() const { return state_; }
    [[nodiscard]] bool is_loaded() const { return state_ == ModuleState::Loaded || state_ == ModuleState::Active; }

    // Load/Unload
    CppResult<void> load(const std::filesystem::path& path);
    CppResult<void> unload();
    CppResult<void> reload();

    // Symbols
    [[nodiscard]] void* get_symbol(const std::string& name);
    [[nodiscard]] bool has_symbol(const std::string& name);

    template <typename T>
    [[nodiscard]] T* get_symbol_as(const std::string& name) {
        return reinterpret_cast<T*>(get_symbol(name));
    }

    template <typename Func>
    [[nodiscard]] Func* get_function(const std::string& name) {
        return reinterpret_cast<Func*>(get_symbol(name));
    }

    // Symbol enumeration
    [[nodiscard]] std::vector<SymbolInfo> enumerate_symbols() const;
    [[nodiscard]] const std::vector<SymbolInfo>& symbols() const { return symbols_; }

    // Module info
    [[nodiscard]] const ModuleInfo& info() const { return info_; }

    // File modification time
    [[nodiscard]] std::filesystem::file_time_type file_time() const;
    [[nodiscard]] bool has_file_changed() const;

    // Error
    [[nodiscard]] const std::string& error_message() const { return error_message_; }

private:
    void enumerate_symbols_impl();

    ModuleId id_;
    std::string name_;
    std::filesystem::path path_;
    ModuleState state_ = ModuleState::Unloaded;

    void* handle_ = nullptr;
    ModuleInfo info_;
    std::vector<SymbolInfo> symbols_;
    std::unordered_map<std::string, void*> symbol_cache_;

    std::filesystem::file_time_type loaded_file_time_;
    std::string error_message_;
};

// =============================================================================
// Module Registry
// =============================================================================

/// @brief Registry of loaded modules
class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();

    // Singleton
    [[nodiscard]] static ModuleRegistry& instance();

    // ==========================================================================
    // Module Management
    // ==========================================================================

    /// @brief Load a module
    CppResult<DynamicModule*> load(const std::filesystem::path& path);

    /// @brief Load a module with a name
    CppResult<DynamicModule*> load(const std::string& name, const std::filesystem::path& path);

    /// @brief Unload a module
    bool unload(ModuleId id);

    /// @brief Unload a module by name
    bool unload(const std::string& name);

    /// @brief Reload a module
    CppResult<DynamicModule*> reload(ModuleId id);

    /// @brief Get a module
    [[nodiscard]] DynamicModule* get(ModuleId id);
    [[nodiscard]] const DynamicModule* get(ModuleId id) const;

    /// @brief Find a module by name
    [[nodiscard]] DynamicModule* find(const std::string& name);

    /// @brief Get all modules
    [[nodiscard]] std::vector<DynamicModule*> modules();

    /// @brief Check if module exists
    [[nodiscard]] bool exists(ModuleId id) const;
    [[nodiscard]] bool exists(const std::string& name) const;

    // ==========================================================================
    // Symbol Resolution
    // ==========================================================================

    /// @brief Find symbol across all modules
    [[nodiscard]] void* find_symbol(const std::string& name);

    /// @brief Find symbol in specific module
    [[nodiscard]] void* find_symbol(ModuleId module_id, const std::string& name);

    // ==========================================================================
    // Bulk Operations
    // ==========================================================================

    /// @brief Unload all modules
    void unload_all();

    /// @brief Get modules that have changed on disk
    [[nodiscard]] std::vector<DynamicModule*> get_changed_modules();

    // ==========================================================================
    // Search Paths
    // ==========================================================================

    /// @brief Add module search path
    void add_search_path(const std::filesystem::path& path);

    /// @brief Clear search paths
    void clear_search_paths();

    /// @brief Resolve module path
    [[nodiscard]] std::filesystem::path resolve_path(const std::string& name) const;

private:
    std::unordered_map<ModuleId, std::unique_ptr<DynamicModule>> modules_;
    std::unordered_map<std::string, ModuleId> module_names_;
    std::vector<std::filesystem::path> search_paths_;

    inline static std::uint32_t next_module_id_ = 1;
};

// =============================================================================
// Module Loader
// =============================================================================

/// @brief Utility for loading modules with dependency resolution
class ModuleLoader {
public:
    ModuleLoader();
    explicit ModuleLoader(ModuleRegistry& registry);

    // Load with dependencies
    CppResult<DynamicModule*> load_with_dependencies(
        const std::filesystem::path& path);

    // Get load order
    [[nodiscard]] std::vector<std::filesystem::path> get_load_order(
        const std::filesystem::path& path) const;

    // Dependency analysis
    [[nodiscard]] std::vector<std::string> get_dependencies(
        const std::filesystem::path& path) const;

    [[nodiscard]] std::vector<std::string> get_missing_dependencies(
        const std::filesystem::path& path) const;

private:
    ModuleRegistry& registry_;
};

// =============================================================================
// Platform-Specific Helpers
// =============================================================================

namespace platform {

/// @brief Get shared library extension for current platform
[[nodiscard]] const char* shared_library_extension();

/// @brief Get shared library prefix for current platform
[[nodiscard]] const char* shared_library_prefix();

/// @brief Format shared library name
[[nodiscard]] std::string format_library_name(const std::string& name);

/// @brief Load shared library
[[nodiscard]] void* load_library(const std::filesystem::path& path);

/// @brief Unload shared library
bool unload_library(void* handle);

/// @brief Get symbol from library
[[nodiscard]] void* get_symbol(void* handle, const char* name);

/// @brief Get last error message
[[nodiscard]] std::string get_last_error();

/// @brief Enumerate symbols in library (platform-specific)
[[nodiscard]] std::vector<SymbolInfo> enumerate_symbols(void* handle);

} // namespace platform

} // namespace void_cpp
