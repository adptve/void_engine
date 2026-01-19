# Hot-Reload System

The signature feature of void_engine: everything is hot-swappable without engine restart.

## Overview

void_engine is designed to never shut down between applications. All components can be replaced at runtime with full state preservation:

| Component | Mechanism | State Preserved |
|-----------|-----------|-----------------|
| **Shaders** | File watch + recompile | Pipeline rebuilt at frame boundary |
| **Assets** | File watch + atomic swap | Handle remains valid |
| **Materials** | Shader dependency tracking | Properties preserved |
| **Plugins/DLLs** | Unload → Serialize → Load → Deserialize | Full state snapshot |
| **Components** | ECS archetype migration | Entity relationships intact |
| **Scenes** | Incremental patch application | World state preserved |

## Shader Hot-Reload

### Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   File Watcher  │────▶│ Shader Compiler │────▶│  Shader Cache   │
│    (notify)     │     │   (WGSL→SPIRV)  │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                                        │
                                                        ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Materials     │◀────│ Pipeline Cache  │◀────│  Frame Boundary │
│  (invalidated)  │     │   (rebuilt)     │     │    (swap)       │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### Implementation

```cpp
namespace void_engine::shader {

class ShaderHotReload {
public:
    void watch(std::filesystem::path path);
    void unwatch(std::filesystem::path path);

    // Called each frame
    void check_for_changes();

    // Shader change callback
    using Callback = std::function<void(std::filesystem::path const&)>;
    void on_shader_changed(Callback cb);

private:
    std::unique_ptr<FileWatcher> m_watcher;
    std::vector<Callback> m_callbacks;
    std::unordered_set<std::filesystem::path> m_pending_reloads;
};

class ShaderManager {
public:
    ShaderHandle load(std::filesystem::path path);
    ShaderModule const* get(ShaderHandle handle) const;

    void enable_hot_reload(bool enable);

    // Called at frame boundary
    void apply_pending_reloads();

private:
    void on_file_changed(std::filesystem::path const& path);
    void recompile(ShaderHandle handle);

    HandleMap<ShaderModule> m_shaders;
    std::unordered_map<std::filesystem::path, ShaderHandle> m_path_to_handle;
    ShaderHotReload m_hot_reload;
    std::vector<ShaderHandle> m_pending;
};

} // namespace void_engine::shader
```

### Usage

```cpp
// Enable hot-reload
shader_manager.enable_hot_reload(true);

// Load shader (automatically watched)
auto handle = shader_manager.load("shaders/pbr.wgsl");

// In render loop
void Engine::render_frame() {
    // Apply any pending shader reloads at safe point
    m_shader_manager.apply_pending_reloads();

    // Render with potentially updated shaders
    render();
}
```

## Asset Hot-Reload

### File Watching

```cpp
namespace void_engine::asset {

class FileWatcher {
public:
    void watch(std::filesystem::path path);
    void watch_directory(std::filesystem::path dir, bool recursive = true);

    struct Event {
        std::filesystem::path path;
        enum class Type { Created, Modified, Deleted, Renamed } type;
    };

    std::vector<Event> poll_events();

private:
    // Platform-specific: inotify (Linux), FSEvents (macOS), ReadDirectoryChangesW (Windows)
};

class AssetServer {
public:
    template<typename T>
    Handle<T> load(std::filesystem::path path);

    template<typename T>
    T* get(Handle<T> handle);

    void enable_hot_reload(bool enable);
    void process_hot_reload_events();

private:
    void on_asset_modified(std::filesystem::path const& path);

    FileWatcher m_watcher;
    std::unordered_map<std::filesystem::path, AssetId> m_path_to_id;
    std::unordered_map<AssetId, std::any> m_assets;
};

} // namespace void_engine::asset
```

### Atomic Asset Swap

```cpp
template<typename T>
void AssetServer::reload_asset(Handle<T> handle) {
    auto path = get_path(handle);

    // Load new version
    auto new_asset = load_from_disk<T>(path);
    if (!new_asset) {
        log_error("Hot-reload failed for {}", path);
        return;  // Keep old version
    }

    // Atomic swap at frame boundary
    m_pending_swaps.push_back({
        .handle = handle,
        .new_asset = std::move(new_asset)
    });
}

void AssetServer::apply_pending_swaps() {
    for (auto& swap : m_pending_swaps) {
        // Old asset destroyed, new asset in place
        m_assets[swap.handle] = std::move(swap.new_asset);

        // Notify dependents
        for (auto& listener : m_listeners) {
            listener.on_asset_reloaded(swap.handle);
        }
    }
    m_pending_swaps.clear();
}
```

## Plugin/DLL Hot-Reload

### State Preservation Flow

```
┌─────────────┐
│  Snapshot   │ ── Serialize current state to bytes
│  Old State  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Unload    │ ── dlclose() / FreeLibrary()
│  Old DLL    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│    Load     │ ── dlopen() / LoadLibrary()
│   New DLL   │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Deserialize │ ── Restore state from bytes
│  New State  │
└──────┬──────┘
       │
       ├──── Success ────▶ Continue
       │
       └──── Failure ────▶ Rollback to snapshot
```

### Implementation

```cpp
namespace void_engine::core {

// Interface for hot-reloadable objects
class IHotReloadable {
public:
    virtual ~IHotReloadable() = default;

    // Serialize state for preservation
    virtual std::vector<uint8_t> snapshot() const = 0;

    // Restore state after reload
    virtual bool restore(std::span<uint8_t const> data) = 0;

    // Called after reload to rebuild transient state
    virtual void on_reload() = 0;
};

class PluginHotReload {
public:
    bool reload_plugin(PluginId id);

private:
    struct ReloadContext {
        std::vector<uint8_t> state_snapshot;
        std::filesystem::path dll_path;
        std::filesystem::path temp_path;
    };

    bool prepare_reload(PluginId id, ReloadContext& ctx);
    bool perform_swap(PluginId id, ReloadContext& ctx);
    void rollback(PluginId id, ReloadContext const& ctx);
};

} // namespace void_engine::core
```

### Plugin State Serialization

```cpp
class MyPlugin : public IPlugin, public IHotReloadable {
    // Persistent state (survives reload)
    int m_counter = 0;
    std::vector<EntityId> m_managed_entities;

    // Transient state (rebuilt after reload)
    std::unordered_map<EntityId, CachedData> m_cache;

public:
    std::vector<uint8_t> snapshot() const override {
        Serializer s;
        s.write(m_counter);
        s.write(m_managed_entities);
        return s.data();
    }

    bool restore(std::span<uint8_t const> data) override {
        Deserializer d(data);
        d.read(m_counter);
        d.read(m_managed_entities);
        return d.success();
    }

    void on_reload() override {
        // Rebuild transient state
        m_cache.clear();
        for (auto entity : m_managed_entities) {
            m_cache[entity] = compute_cached_data(entity);
        }
    }
};
```

## Component Hot-Reload

Components can be modified at runtime with automatic archetype migration:

```cpp
namespace void_engine::ecs {

class ComponentHotReload {
public:
    // Register new version of component type
    template<typename OldT, typename NewT>
    void register_migration(std::function<NewT(OldT const&)> migrate);

    // Apply pending migrations
    void apply_migrations(World& world);

private:
    struct Migration {
        TypeId old_type;
        TypeId new_type;
        std::function<void(void const*, void*)> migrate_fn;
    };
    std::vector<Migration> m_migrations;
};

} // namespace void_engine::ecs
```

### Usage

```cpp
// Old component version
struct HealthV1 {
    float hp;
};

// New component version
struct HealthV2 {
    float current;
    float max;
    float regen_rate;
};

// Register migration
hot_reload.register_migration<HealthV1, HealthV2>(
    [](HealthV1 const& old) {
        return HealthV2{
            .current = old.hp,
            .max = old.hp,
            .regen_rate = 0.0f
        };
    }
);

// Apply at safe point (e.g., frame boundary)
hot_reload.apply_migrations(world);
```

## Frame Boundary Synchronization

All hot-reload operations are applied at frame boundaries to avoid mid-frame corruption:

```cpp
void Engine::run_frame() {
    // 1. Input processing
    process_input();

    // 2. Check for hot-reload events
    m_file_watcher.poll();

    // 3. Apply pending hot-reloads (SAFE POINT)
    m_shader_manager.apply_pending_reloads();
    m_asset_server.apply_pending_swaps();
    m_plugin_manager.apply_pending_reloads();
    m_component_hot_reload.apply_migrations(m_world);

    // 4. Game update
    update(m_delta_time);

    // 5. Render
    render();

    // 6. Present
    present();
}
```

## Best Practices

### 1. Separate Persistent and Transient State

```cpp
struct MySystem {
    // Persistent: serialized during hot-reload
    std::vector<EntityId> tracked_entities;
    Configuration config;

    // Transient: rebuilt in on_reload()
    std::unordered_map<EntityId, CachedComputation> cache;
    RenderPipeline* pipeline;  // Pointer to external resource
};
```

### 2. Handle Missing Resources Gracefully

```cpp
void render_mesh(MeshHandle handle) {
    if (auto* mesh = mesh_cache.get(handle)) {
        draw(*mesh);
    } else {
        // Asset being reloaded - draw placeholder
        draw(placeholder_mesh);
    }
}
```

### 3. Version Your Serialized State

```cpp
std::vector<uint8_t> snapshot() const {
    Serializer s;
    s.write<uint32_t>(STATE_VERSION);  // Version tag
    s.write(m_data);
    return s.data();
}

bool restore(std::span<uint8_t const> data) {
    Deserializer d(data);
    uint32_t version = d.read<uint32_t>();

    if (version == STATE_VERSION) {
        d.read(m_data);
    } else if (version == STATE_VERSION - 1) {
        // Migration from old format
        migrate_from_old_format(d);
    } else {
        return false;  // Incompatible version
    }
    return d.success();
}
```

### 4. Test Hot-Reload Cycles

```cpp
TEST_CASE("Plugin survives hot-reload") {
    PluginManager manager;
    auto id = manager.load("my_plugin.dll");

    auto* plugin = manager.get<MyPlugin>(id);
    plugin->set_value(42);

    // Simulate hot-reload
    REQUIRE(manager.reload(id));

    plugin = manager.get<MyPlugin>(id);
    REQUIRE(plugin->get_value() == 42);  // State preserved
}
```
