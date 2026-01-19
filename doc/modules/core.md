# void_core Module

The foundation module providing plugin system, type registry, and hot-reload infrastructure.

## Components

### Plugin System

```cpp
namespace void_engine::core {

struct Version {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;

    bool is_compatible_with(Version const& other) const;
};

class PluginContext {
public:
    TypeRegistry& type_registry();
    EventBus& event_bus();
    void request_capability(CapabilityKind kind);
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual bool on_load(PluginContext& ctx) = 0;
    virtual void on_unload() = 0;
    virtual void on_config(PluginConfig const& cfg) = 0;
    virtual char const* name() const = 0;
    virtual Version version() const = 0;
};

enum class PluginStatus {
    Registered,  // Registered but not loaded
    Loading,     // on_load() in progress
    Active,      // Successfully loaded
    Unloading,   // on_unload() in progress
    Disabled,    // Explicitly disabled
    Failed       // Load or runtime failure
};

class PluginRegistry {
public:
    PluginId register_plugin(std::unique_ptr<IPlugin> plugin);
    bool load(PluginId id);
    void unload(PluginId id);
    bool reload(PluginId id);  // Hot-reload with state preservation

    PluginStatus status(PluginId id) const;
    IPlugin* get(PluginId id);

    template<typename T>
    T* get_as(PluginId id);

private:
    std::unordered_map<PluginId, PluginEntry> m_plugins;
};

} // namespace void_engine::core
```

### Type Registry

Runtime type information for reflection and serialization:

```cpp
namespace void_engine::core {

using TypeId = uint64_t;

struct TypeInfo {
    TypeId id;
    std::string_view name;
    size_t size;
    size_t alignment;

    // Type operations
    std::function<void*(void)> construct;
    std::function<void(void*)> destruct;
    std::function<void(void const*, void*)> copy;
    std::function<void(void*, void*)> move;

    // Serialization
    std::function<void(void const*, Serializer&)> serialize;
    std::function<void(void*, Deserializer&)> deserialize;
};

class TypeRegistry {
public:
    template<typename T>
    TypeId register_type(std::string_view name);

    template<typename T>
    TypeId type_id() const;

    TypeInfo const* get(TypeId id) const;
    TypeInfo const* find(std::string_view name) const;

    std::span<TypeId const> all_types() const;

private:
    std::unordered_map<TypeId, TypeInfo> m_types;
    std::unordered_map<std::string_view, TypeId> m_name_to_id;
};

// Compile-time type ID generation
template<typename T>
constexpr TypeId type_id_v = /* implementation */;

} // namespace void_engine::core
```

### Hot-Reload Support

```cpp
namespace void_engine::core {

class IHotReloadable {
public:
    virtual ~IHotReloadable() = default;
    virtual std::vector<uint8_t> snapshot() const = 0;
    virtual bool restore(std::span<uint8_t const> bytes) = 0;
    virtual void on_reload() = 0;  // Rebuild transient state
};

class HotReloadManager {
public:
    void register_reloadable(std::string_view name, IHotReloadable* obj);
    void unregister(std::string_view name);

    bool hot_swap(std::string_view name, std::function<bool()> swap_fn);

private:
    struct ReloadEntry {
        IHotReloadable* object;
        std::vector<uint8_t> last_snapshot;
    };
    std::unordered_map<std::string, ReloadEntry> m_reloadables;
};

} // namespace void_engine::core
```

### Handle System

Generational handles for safe resource references:

```cpp
namespace void_engine::core {

template<typename Tag>
struct Handle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    bool is_valid() const { return index != UINT32_MAX; }
    explicit operator bool() const { return is_valid(); }
};

template<typename T, typename Tag = T>
class HandleMap {
public:
    using HandleType = Handle<Tag>;

    HandleType insert(T value);
    void remove(HandleType handle);
    bool is_valid(HandleType handle) const;

    T* get(HandleType handle);
    T const* get(HandleType handle) const;

    size_t size() const;
    void clear();

    // Iteration
    auto begin() { return m_dense.begin(); }
    auto end() { return m_dense.end(); }

private:
    std::vector<T> m_dense;
    std::vector<HandleType> m_dense_to_handle;
    std::vector<uint32_t> m_sparse;  // handle.index -> dense index
    std::vector<uint32_t> m_generations;
    std::queue<uint32_t> m_free_list;
};

} // namespace void_engine::core
```

### Error Handling

```cpp
namespace void_engine::core {

enum class ErrorCode {
    Ok,
    NotFound,
    AlreadyExists,
    InvalidArgument,
    PermissionDenied,
    QuotaExceeded,
    IoError,
    SerializationError,
    ValidationError
};

template<typename T>
class Result {
public:
    static Result ok(T value);
    static Result err(ErrorCode code, std::string message = {});

    bool is_ok() const;
    bool is_err() const;

    T& value();
    T const& value() const;
    T value_or(T default_val) const;

    ErrorCode error_code() const;
    std::string_view error_message() const;

    template<typename F>
    auto map(F f) -> Result<decltype(f(std::declval<T>()))>;

    template<typename F>
    auto and_then(F f) -> decltype(f(std::declval<T>()));

private:
    std::variant<T, Error> m_value;
};

} // namespace void_engine::core
```

## Usage Examples

### Registering a Plugin

```cpp
class MyPlugin : public IPlugin {
public:
    bool on_load(PluginContext& ctx) override {
        ctx.type_registry().register_type<MyComponent>("MyComponent");
        return true;
    }

    void on_unload() override { }
    void on_config(PluginConfig const& cfg) override { }
    char const* name() const override { return "MyPlugin"; }
    Version version() const override { return {1, 0, 0}; }
};

// Registration
registry.register_plugin(std::make_unique<MyPlugin>());
```

### Using Handles

```cpp
HandleMap<Texture, struct TextureTag> textures;

auto handle = textures.insert(Texture{...});
if (auto* tex = textures.get(handle)) {
    tex->bind(0);
}
textures.remove(handle);
// handle is now invalid, get() returns nullptr
```
