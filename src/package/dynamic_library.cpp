/// @file dynamic_library.cpp
/// @brief Cross-platform dynamic library loading implementation

#include <void_engine/package/dynamic_library.hpp>

#include <algorithm>

namespace void_package {

// =============================================================================
// DynamicLibrary - Destructor and Move Operations
// =============================================================================

DynamicLibrary::~DynamicLibrary() {
    unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : m_handle(other.m_handle)
    , m_path(std::move(other.m_path))
{
    other.m_handle = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        unload();
        m_handle = other.m_handle;
        m_path = std::move(other.m_path);
        other.m_handle = nullptr;
    }
    return *this;
}

DynamicLibrary::DynamicLibrary(NativeLibraryHandle handle, std::filesystem::path path)
    : m_handle(handle)
    , m_path(std::move(path))
{}

// =============================================================================
// DynamicLibrary - Platform-Specific Loading
// =============================================================================

#ifdef _WIN32

void_core::Result<DynamicLibrary> DynamicLibrary::load(const std::filesystem::path& path) {
    // Convert to wide string for Windows
    std::wstring wide_path = path.wstring();

    HMODULE handle = LoadLibraryW(wide_path.c_str());
    if (!handle) {
        return void_core::Error("Failed to load library '" + path.string() + "': " + get_last_error());
    }

    return DynamicLibrary(handle, path);
}

void_core::Result<DynamicLibrary> DynamicLibrary::load_with_flags(
    const std::filesystem::path& path,
    int flags)
{
    std::wstring wide_path = path.wstring();

    HMODULE handle = LoadLibraryExW(wide_path.c_str(), nullptr, static_cast<DWORD>(flags));
    if (!handle) {
        return void_core::Error("Failed to load library '" + path.string() + "': " + get_last_error());
    }

    return DynamicLibrary(handle, path);
}

void DynamicLibrary::unload() noexcept {
    if (m_handle) {
        FreeLibrary(m_handle);
        m_handle = nullptr;
    }
    m_path.clear();
}

void* DynamicLibrary::get_symbol(const char* name) const noexcept {
    if (!m_handle) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(m_handle, name));
}

std::string DynamicLibrary::get_last_error() {
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        return "No error";
    }

    LPSTR message_buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr
    );

    if (size == 0) {
        return "Error code: " + std::to_string(error_code);
    }

    std::string message(message_buffer, size);
    LocalFree(message_buffer);

    // Remove trailing newlines
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    return message + " (error " + std::to_string(error_code) + ")";
}

#else // Unix (Linux, macOS)

void_core::Result<DynamicLibrary> DynamicLibrary::load(const std::filesystem::path& path) {
    // Clear any previous errors
    dlerror();

    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return void_core::Error("Failed to load library '" + path.string() + "': " + get_last_error());
    }

    return DynamicLibrary(handle, path);
}

void_core::Result<DynamicLibrary> DynamicLibrary::load_with_flags(
    const std::filesystem::path& path,
    int flags)
{
    dlerror();

    void* handle = dlopen(path.c_str(), flags);
    if (!handle) {
        return void_core::Error("Failed to load library '" + path.string() + "': " + get_last_error());
    }

    return DynamicLibrary(handle, path);
}

void DynamicLibrary::unload() noexcept {
    if (m_handle) {
        dlclose(m_handle);
        m_handle = nullptr;
    }
    m_path.clear();
}

void* DynamicLibrary::get_symbol(const char* name) const noexcept {
    if (!m_handle) {
        return nullptr;
    }

    // Clear previous errors
    dlerror();

    return dlsym(m_handle, name);
}

std::string DynamicLibrary::get_last_error() {
    const char* error = dlerror();
    return error ? std::string(error) : "No error";
}

#endif

// =============================================================================
// DynamicLibraryCache
// =============================================================================

void_core::Result<DynamicLibrary*> DynamicLibraryCache::get_or_load(
    const std::filesystem::path& path)
{
    // Normalize path for consistent lookup
    std::filesystem::path canonical_path;
    std::error_code ec;
    canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical_path = path;  // Fall back to original if canonicalization fails
    }

    // Check if already loaded
    auto it = m_libraries.find(canonical_path);
    if (it != m_libraries.end()) {
        return it->second.get();
    }

    // Load the library
    auto lib_result = DynamicLibrary::load(canonical_path);
    if (!lib_result) {
        return void_core::Error(lib_result.error().message());
    }

    // Store in cache
    auto lib_ptr = std::make_unique<DynamicLibrary>(std::move(*lib_result));
    DynamicLibrary* raw_ptr = lib_ptr.get();
    m_libraries[canonical_path] = std::move(lib_ptr);

    return raw_ptr;
}

bool DynamicLibraryCache::is_loaded(const std::filesystem::path& path) const {
    std::filesystem::path canonical_path;
    std::error_code ec;
    canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical_path = path;
    }
    return m_libraries.find(canonical_path) != m_libraries.end();
}

DynamicLibrary* DynamicLibraryCache::get(const std::filesystem::path& path) {
    std::filesystem::path canonical_path;
    std::error_code ec;
    canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical_path = path;
    }

    auto it = m_libraries.find(canonical_path);
    return it != m_libraries.end() ? it->second.get() : nullptr;
}

const DynamicLibrary* DynamicLibraryCache::get(const std::filesystem::path& path) const {
    std::filesystem::path canonical_path;
    std::error_code ec;
    canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical_path = path;
    }

    auto it = m_libraries.find(canonical_path);
    return it != m_libraries.end() ? it->second.get() : nullptr;
}

bool DynamicLibraryCache::unload(const std::filesystem::path& path) {
    std::filesystem::path canonical_path;
    std::error_code ec;
    canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonical_path = path;
    }

    auto it = m_libraries.find(canonical_path);
    if (it == m_libraries.end()) {
        return false;
    }

    m_libraries.erase(it);
    return true;
}

void DynamicLibraryCache::unload_all() {
    m_libraries.clear();
}

std::vector<std::filesystem::path> DynamicLibraryCache::loaded_paths() const {
    std::vector<std::filesystem::path> paths;
    paths.reserve(m_libraries.size());

    for (const auto& [path, _] : m_libraries) {
        paths.push_back(path);
    }

    return paths;
}

// =============================================================================
// Utility Functions
// =============================================================================

bool has_library_extension(const std::filesystem::path& path) noexcept {
    std::string ext = path.extension().string();

    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return ext == ".dll" || ext == ".so" || ext == ".dylib";
}

std::filesystem::path with_library_extension(const std::filesystem::path& path) {
    if (has_library_extension(path)) {
        return path;
    }

    std::filesystem::path result = path;
    result += library_extension();
    return result;
}

} // namespace void_package
