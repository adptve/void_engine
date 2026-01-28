/// @file dynamic_library.hpp
/// @brief Cross-platform dynamic library loading for plugins

#pragma once

#include "fwd.hpp"

#include <filesystem>
#include <string>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace void_plugin_api {

/// @brief Cross-platform dynamic library loader
class DynamicLibrary {
public:
    DynamicLibrary() = default;
    ~DynamicLibrary() { unload(); }

    // Non-copyable
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // Movable
    DynamicLibrary(DynamicLibrary&& other) noexcept
        : m_handle(other.m_handle)
        , m_path(std::move(other.m_path))
        , m_error(std::move(other.m_error)) {
        other.m_handle = nullptr;
    }

    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept {
        if (this != &other) {
            unload();
            m_handle = other.m_handle;
            m_path = std::move(other.m_path);
            m_error = std::move(other.m_error);
            other.m_handle = nullptr;
        }
        return *this;
    }

    /// @brief Load a dynamic library from path
    bool load(const std::filesystem::path& path) {
        unload();
        m_path = path;
        m_error.clear();

#ifdef _WIN32
        m_handle = LoadLibraryW(path.wstring().c_str());
        if (!m_handle) {
            DWORD err = GetLastError();
            m_error = "LoadLibrary failed with error " + std::to_string(err);
            return false;
        }
#else
        m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            const char* err = dlerror();
            m_error = err ? err : "Unknown dlopen error";
            return false;
        }
#endif
        return true;
    }

    /// @brief Unload the library
    void unload() {
        if (m_handle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(m_handle));
#else
            dlclose(m_handle);
#endif
            m_handle = nullptr;
        }
        m_path.clear();
    }

    /// @brief Check if library is loaded
    [[nodiscard]] bool is_loaded() const { return m_handle != nullptr; }

    /// @brief Get a function pointer by name
    template<typename FuncType>
    FuncType get_function(const char* name) {
        if (!m_handle) return nullptr;

#ifdef _WIN32
        return reinterpret_cast<FuncType>(
            GetProcAddress(static_cast<HMODULE>(m_handle), name));
#else
        return reinterpret_cast<FuncType>(dlsym(m_handle, name));
#endif
    }

    /// @brief Get last error message
    [[nodiscard]] const std::string& error() const { return m_error; }

    /// @brief Get library path
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    void* m_handle{nullptr};
    std::filesystem::path m_path;
    std::string m_error;
};

/// @brief Plugin factory function types
using CreatePluginFunc = GameplayPlugin* (*)();
using DestroyPluginFunc = void (*)(GameplayPlugin*);

/// @brief Loaded plugin with its library handle
struct LoadedPlugin {
    std::unique_ptr<DynamicLibrary> library;
    GameplayPlugin* plugin{nullptr};
    DestroyPluginFunc destroy_func{nullptr};
    std::string name;

    ~LoadedPlugin() {
        if (plugin && destroy_func) {
            destroy_func(plugin);
        }
        // Library unloads automatically via unique_ptr
    }

    // Non-copyable, movable
    LoadedPlugin() = default;
    LoadedPlugin(const LoadedPlugin&) = delete;
    LoadedPlugin& operator=(const LoadedPlugin&) = delete;
    LoadedPlugin(LoadedPlugin&&) = default;
    LoadedPlugin& operator=(LoadedPlugin&&) = default;
};

} // namespace void_plugin_api
