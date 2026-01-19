# Hot-Reload Specialist Skill

You are an expert in hot-loading, dynamic code reloading, and live programming systems.

## Core Concepts

### Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                   Host Process                   │
│  ┌─────────────┐  ┌─────────────┐              │
│  │ Stable Core │  │   State     │              │
│  │  (static)   │  │  (persists) │              │
│  └─────────────┘  └─────────────┘              │
│         │                │                      │
│         ▼                ▼                      │
│  ┌─────────────────────────────────────────┐   │
│  │           Function Pointer Table         │   │
│  │  update_fn, render_fn, init_fn, ...     │   │
│  └─────────────────────────────────────────┘   │
│                      │                          │
│                      ▼                          │
│  ┌──────────────────────────────────────────┐  │
│  │        Hot-Loadable Module (DLL/SO)       │  │
│  │   game.dll ←──── rebuild & reload        │  │
│  └──────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

### Module Interface

```cpp
// hot_module.hpp - Shared interface between host and module
#pragma once

#ifdef _WIN32
    #define HOT_EXPORT __declspec(dllexport)
#else
    #define HOT_EXPORT __attribute__((visibility("default")))
#endif

struct GameState;  // Forward declare, defined in module

struct ModuleAPI {
    void (*init)(GameState** state);
    void (*shutdown)(GameState* state);
    void (*update)(GameState* state, float dt);
    void (*render)(GameState* state);
    void (*on_reload)(GameState* state);  // Called after reload

    // Serialization for state migration
    size_t (*serialize)(GameState* state, void* buffer, size_t size);
    void (*deserialize)(GameState** state, void const* buffer, size_t size);
};

extern "C" HOT_EXPORT ModuleAPI* get_module_api();
```

### Host Implementation

```cpp
// hot_loader.hpp
#pragma once
#include <filesystem>

class HotLoader {
public:
    explicit HotLoader(std::filesystem::path module_path);
    ~HotLoader();

    bool load();
    void unload();
    bool reload();

    template<typename T>
    T* get_function(char const* name);

    ModuleAPI* api() const { return m_api; }
    bool is_loaded() const { return m_handle != nullptr; }

private:
    std::filesystem::path m_module_path;
    std::filesystem::path m_temp_path;
    void* m_handle = nullptr;
    ModuleAPI* m_api = nullptr;
    std::filesystem::file_time_type m_last_write;
};
```

```cpp
// hot_loader.cpp
#include "hot_loader.hpp"

#ifdef _WIN32
    #include <windows.h>
    #define LOAD_LIB(path) LoadLibraryA(path)
    #define FREE_LIB(h) FreeLibrary((HMODULE)h)
    #define GET_SYM(h, name) GetProcAddress((HMODULE)h, name)
#else
    #include <dlfcn.h>
    #define LOAD_LIB(path) dlopen(path, RTLD_NOW)
    #define FREE_LIB(h) dlclose(h)
    #define GET_SYM(h, name) dlsym(h, name)
#endif

bool HotLoader::load() {
    // Copy to temp file to avoid locking original
    m_temp_path = m_module_path;
    m_temp_path += ".hot";
    std::filesystem::copy_file(m_module_path, m_temp_path,
        std::filesystem::copy_options::overwrite_existing);

    m_handle = LOAD_LIB(m_temp_path.string().c_str());
    if (!m_handle) return false;

    auto get_api = (ModuleAPI*(*)())GET_SYM(m_handle, "get_module_api");
    if (!get_api) {
        unload();
        return false;
    }

    m_api = get_api();
    m_last_write = std::filesystem::last_write_time(m_module_path);
    return true;
}

bool HotLoader::reload() {
    auto current = std::filesystem::last_write_time(m_module_path);
    if (current <= m_last_write) return false;

    // Serialize state before unload
    std::vector<uint8_t> state_buffer;
    if (m_api && m_api->serialize) {
        size_t size = m_api->serialize(m_state, nullptr, 0);
        state_buffer.resize(size);
        m_api->serialize(m_state, state_buffer.data(), size);
    }

    unload();
    if (!load()) return false;

    // Deserialize state after load
    if (!state_buffer.empty() && m_api->deserialize) {
        m_api->deserialize(&m_state, state_buffer.data(),
                          state_buffer.size());
    }

    if (m_api->on_reload) {
        m_api->on_reload(m_state);
    }
    return true;
}
```

### State Preservation

```cpp
// Use stable memory layout for hot-reloadable state
struct alignas(16) GameState {
    uint32_t version = 1;  // For migration

    // POD types survive reload
    float player_x, player_y;
    int score;

    // Handles instead of pointers
    ResourceHandle<Texture> sprite;
    EntityHandle player_entity;

    // Rebuild transient state in on_reload
    // (e.g., cached pointers, GPU resources)
};

// State migration when layout changes
void migrate_state(GameState* state, uint32_t from_version) {
    if (from_version < 2) {
        // v1 -> v2: added new field
        state->new_field = default_value;
    }
}
```

### File Watching

```cpp
class FileWatcher {
    std::unordered_map<std::filesystem::path, FileInfo> m_watched;
    std::thread m_thread;
    std::atomic<bool> m_running{true};

public:
    void watch(std::filesystem::path path,
               std::function<void()> callback);

    void poll() {
        for (auto& [path, info] : m_watched) {
            auto current = std::filesystem::last_write_time(path);
            if (current > info.last_modified) {
                info.last_modified = current;
                info.callback();
            }
        }
    }
};
```

### Build Integration

```cmake
# CMakeLists.txt for hot-reloadable module
add_library(game_module SHARED
    game_module.cpp
)

# Fast incremental builds
target_precompile_headers(game_module PRIVATE pch.hpp)
set_target_properties(game_module PROPERTIES
    # Output directly to runtime directory
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
```

## Best Practices

1. **Stable ABI**: Use C interfaces, POD structs, explicit padding
2. **Handle-based resources**: Never store raw pointers in reloadable state
3. **Version your state**: Support migration between versions
4. **Copy before load**: Prevents file locking issues on Windows
5. **Transient vs persistent**: Clearly separate what survives reload
6. **Graceful degradation**: Handle reload failures without crashing

## Review Checklist

- [ ] Module interface uses C linkage
- [ ] State structs are POD or explicitly serializable
- [ ] No raw pointers in persisted state
- [ ] on_reload rebuilds transient state
- [ ] File watcher handles rapid successive changes (debounce)
