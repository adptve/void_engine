#pragma once

/// @file dynamic_library.hpp
/// @brief Cross-platform dynamic library loading with RAII
///
/// Provides a safe abstraction over platform-specific dynamic library APIs:
/// - Windows: LoadLibrary/GetProcAddress/FreeLibrary
/// - Linux/macOS: dlopen/dlsym/dlclose
///
/// Used by the plugin system to load system implementations at runtime.

#include <void_engine/core/error.hpp>

#include <string>
#include <filesystem>
#include <memory>
#include <functional>
#include <map>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Forward declare World in global namespace
namespace void_ecs { class World; }

namespace void_package {

// =============================================================================
// Platform Types
// =============================================================================

#ifdef _WIN32
using NativeLibraryHandle = HMODULE;
#else
using NativeLibraryHandle = void*;
#endif

// =============================================================================
// DynamicLibrary
// =============================================================================

/// RAII wrapper for a dynamically loaded library
///
/// Automatically unloads the library when destroyed. Provides type-safe
/// symbol lookup with automatic casting to function pointer types.
///
/// Example:
/// ```cpp
/// auto lib_result = DynamicLibrary::load("plugins/combat.dll");
/// if (!lib_result) {
///     // handle error
/// }
/// auto& lib = *lib_result;
///
/// // Get a function pointer
/// using SystemFn = void(*)(void_ecs::World&);
/// auto fn_result = lib.get_function<SystemFn>("damage_system_run");
/// if (fn_result) {
///     (*fn_result)(world);
/// }
/// ```
class DynamicLibrary {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /// Default constructor (no library loaded)
    DynamicLibrary() = default;

    /// Destructor - unloads the library
    ~DynamicLibrary();

    // Non-copyable
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // Movable
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    // =========================================================================
    // Loading
    // =========================================================================

    /// Load a dynamic library from path
    ///
    /// @param path Path to the library (.dll on Windows, .so on Linux, .dylib on macOS)
    /// @return Loaded library or error with message
    [[nodiscard]] static void_core::Result<DynamicLibrary> load(
        const std::filesystem::path& path);

    /// Load a library with custom flags
    ///
    /// @param path Path to the library
    /// @param flags Platform-specific flags (RTLD_* on Unix, LoadLibraryEx flags on Windows)
    /// @return Loaded library or error
    [[nodiscard]] static void_core::Result<DynamicLibrary> load_with_flags(
        const std::filesystem::path& path,
        int flags);

    /// Check if library is loaded
    [[nodiscard]] bool is_loaded() const noexcept { return m_handle != nullptr; }

    /// Explicit bool conversion
    [[nodiscard]] explicit operator bool() const noexcept { return is_loaded(); }

    /// Unload the library
    void unload() noexcept;

    // =========================================================================
    // Symbol Lookup
    // =========================================================================

    /// Get a symbol (raw void pointer)
    ///
    /// @param name Symbol name
    /// @return Pointer to symbol or nullptr if not found
    [[nodiscard]] void* get_symbol(const char* name) const noexcept;

    /// Get a symbol (string overload)
    [[nodiscard]] void* get_symbol(const std::string& name) const noexcept {
        return get_symbol(name.c_str());
    }

    /// Get a typed function pointer
    ///
    /// @tparam F Function pointer type (e.g., void(*)(int))
    /// @param name Function name
    /// @return Function pointer or error
    template<typename F>
    [[nodiscard]] void_core::Result<F> get_function(const char* name) const {
        static_assert(std::is_pointer_v<F> && std::is_function_v<std::remove_pointer_t<F>>,
            "F must be a function pointer type");

        void* sym = get_symbol(name);
        if (!sym) {
            return void_core::Error("Symbol not found: " + std::string(name));
        }

        return reinterpret_cast<F>(sym);
    }

    /// Get a typed function pointer (string overload)
    template<typename F>
    [[nodiscard]] void_core::Result<F> get_function(const std::string& name) const {
        return get_function<F>(name.c_str());
    }

    /// Check if a symbol exists
    [[nodiscard]] bool has_symbol(const char* name) const noexcept {
        return get_symbol(name) != nullptr;
    }

    /// Check if a symbol exists (string overload)
    [[nodiscard]] bool has_symbol(const std::string& name) const noexcept {
        return has_symbol(name.c_str());
    }

    // =========================================================================
    // Information
    // =========================================================================

    /// Get the library path
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

    /// Get the native handle
    [[nodiscard]] NativeLibraryHandle native_handle() const noexcept { return m_handle; }

    /// Get the last error message from the platform
    [[nodiscard]] static std::string get_last_error();

private:
    // Private constructor for factory methods
    explicit DynamicLibrary(NativeLibraryHandle handle, std::filesystem::path path);

    NativeLibraryHandle m_handle = nullptr;
    std::filesystem::path m_path;
};

// =============================================================================
// DynamicLibraryCache
// =============================================================================

/// Cache for loaded dynamic libraries
///
/// Manages library lifetime and provides shared access to loaded libraries.
/// Libraries are reference-counted and unloaded when no longer needed.
///
/// Thread-safety: NOT thread-safe. Should only be accessed from the main thread.
class DynamicLibraryCache {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    DynamicLibraryCache() = default;

    // Non-copyable, movable
    DynamicLibraryCache(const DynamicLibraryCache&) = delete;
    DynamicLibraryCache& operator=(const DynamicLibraryCache&) = delete;
    DynamicLibraryCache(DynamicLibraryCache&&) = default;
    DynamicLibraryCache& operator=(DynamicLibraryCache&&) = default;

    // =========================================================================
    // Library Access
    // =========================================================================

    /// Get or load a library
    ///
    /// If the library is already loaded, returns a pointer to it.
    /// Otherwise loads the library and caches it.
    ///
    /// @param path Path to the library
    /// @return Pointer to library or error
    [[nodiscard]] void_core::Result<DynamicLibrary*> get_or_load(
        const std::filesystem::path& path);

    /// Check if a library is loaded
    [[nodiscard]] bool is_loaded(const std::filesystem::path& path) const;

    /// Get a loaded library (nullptr if not loaded)
    [[nodiscard]] DynamicLibrary* get(const std::filesystem::path& path);

    /// Get a loaded library (const, nullptr if not loaded)
    [[nodiscard]] const DynamicLibrary* get(const std::filesystem::path& path) const;

    // =========================================================================
    // Management
    // =========================================================================

    /// Unload a specific library
    ///
    /// @param path Path to the library
    /// @return true if library was found and unloaded
    bool unload(const std::filesystem::path& path);

    /// Unload all libraries
    void unload_all();

    /// Get number of loaded libraries
    [[nodiscard]] std::size_t size() const noexcept { return m_libraries.size(); }

    /// Check if cache is empty
    [[nodiscard]] bool empty() const noexcept { return m_libraries.empty(); }

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Get all loaded library paths
    [[nodiscard]] std::vector<std::filesystem::path> loaded_paths() const;

private:
    std::map<std::filesystem::path, std::unique_ptr<DynamicLibrary>> m_libraries;
};

// =============================================================================
// System Function Types
// =============================================================================

/// Function signature for plugin system entry points
///
/// Systems loaded from plugins must have this signature:
/// ```cpp
/// extern "C" void my_system_run(void_ecs::World& world);
/// ```
using PluginSystemFn = void(*)(void_ecs::World& world);

/// Function signature for plugin initialization
///
/// Called when plugin is loaded:
/// ```cpp
/// extern "C" bool my_plugin_init(void* context);
/// ```
using PluginInitFn = bool(*)(void* context);

/// Function signature for plugin shutdown
///
/// Called when plugin is unloaded:
/// ```cpp
/// extern "C" void my_plugin_shutdown();
/// ```
using PluginShutdownFn = void(*)();

/// Function signature for event handlers
///
/// Event handlers receive event data as void pointer:
/// ```cpp
/// extern "C" void on_entity_damaged(void* event_data);
/// ```
using PluginEventHandlerFn = void(*)(void* event_data);

// =============================================================================
// Utility Functions
// =============================================================================

/// Get the platform-specific library extension
[[nodiscard]] constexpr const char* library_extension() noexcept {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

/// Check if a path has a valid library extension
[[nodiscard]] bool has_library_extension(const std::filesystem::path& path) noexcept;

/// Ensure a path has the correct library extension for the current platform
[[nodiscard]] std::filesystem::path with_library_extension(
    const std::filesystem::path& path);

} // namespace void_package
