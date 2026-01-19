# Software Architecture Skill

You are a software architect specializing in systems design, modularity, and clean architecture for game engines.

## Core Principles

### Dependency Direction

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│              (Game logic, Editor, Tools)                │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼ depends on
┌─────────────────────────────────────────────────────────┐
│                      Engine Layer                        │
│         (Renderer, Physics, Audio, Scripting)           │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼ depends on
┌─────────────────────────────────────────────────────────┐
│                       Core Layer                         │
│      (Math, Containers, Memory, Platform, Logging)      │
└─────────────────────────────────────────────────────────┘
```

**Rules:**
- Lower layers never depend on higher layers
- Siblings may interact through abstractions
- Platform-specific code isolated in adapters

### Module Design

```cpp
// Module interface (public header)
// include/void_engine/renderer/renderer.hpp
namespace void_engine::renderer {

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void begin_frame() = 0;
    virtual void submit(DrawCommand const& cmd) = 0;
    virtual void end_frame() = 0;
};

// Factory function - implementation hidden
std::unique_ptr<IRenderer> create_renderer(RendererConfig const& config);

} // namespace void_engine::renderer
```

```cpp
// Module implementation (private)
// src/renderer/vulkan/vulkan_renderer.cpp
namespace void_engine::renderer {

class VulkanRenderer final : public IRenderer {
    // Implementation details hidden from consumers
};

std::unique_ptr<IRenderer> create_renderer(RendererConfig const& config) {
    return std::make_unique<VulkanRenderer>(config);
}

} // namespace void_engine::renderer
```

### Dependency Injection

```cpp
// Poor: hard dependency
class Game {
    VulkanRenderer m_renderer;  // Concrete type
};

// Better: inject abstractions
class Game {
    IRenderer& m_renderer;
public:
    explicit Game(IRenderer& renderer) : m_renderer(renderer) {}
};

// Service locator for global services (use sparingly)
class Services {
    static inline IRenderer* s_renderer = nullptr;
public:
    static void provide(IRenderer* r) { s_renderer = r; }
    static IRenderer& renderer() { return *s_renderer; }
};
```

### Event System

```cpp
// Decoupled communication
class EventBus {
    std::unordered_map<
        std::type_index,
        std::vector<std::function<void(void const*)>>
    > m_handlers;

public:
    template<typename E>
    void subscribe(std::function<void(E const&)> handler) {
        m_handlers[typeid(E)].push_back(
            [h = std::move(handler)](void const* e) {
                h(*static_cast<E const*>(e));
            });
    }

    template<typename E>
    void publish(E const& event) {
        auto it = m_handlers.find(typeid(E));
        if (it != m_handlers.end()) {
            for (auto& handler : it->second) {
                handler(&event);
            }
        }
    }
};

// Events are pure data
struct WindowResized {
    uint32_t width, height;
};
```

### Plugin Architecture

```cpp
// Plugin interface
struct IPlugin {
    virtual ~IPlugin() = default;
    virtual void on_load(Engine& engine) = 0;
    virtual void on_unload() = 0;
    virtual char const* name() const = 0;
};

// Plugin registration macro
#define DECLARE_PLUGIN(PluginClass) \
    extern "C" HOT_EXPORT IPlugin* create_plugin() { \
        return new PluginClass(); \
    } \
    extern "C" HOT_EXPORT void destroy_plugin(IPlugin* p) { \
        delete p; \
    }
```

### Configuration

```cpp
// Compile-time configuration
namespace config {
    constexpr bool k_enable_validation = DEBUG_BUILD;
    constexpr size_t k_max_entities = 1'000'000;
}

// Runtime configuration
struct EngineConfig {
    uint32_t window_width = 1920;
    uint32_t window_height = 1080;
    bool vsync = true;
    std::filesystem::path asset_path;
};

EngineConfig load_config(std::filesystem::path path);
```

### SOLID Principles Applied

**Single Responsibility:**
```cpp
// Bad: monolithic class
class Game {
    void handle_input();
    void update_physics();
    void render();
    void play_audio();
};

// Good: separated concerns
class InputSystem { void process(); };
class PhysicsSystem { void simulate(float dt); };
class RenderSystem { void render(); };
class AudioSystem { void update(); };
```

**Open/Closed:**
```cpp
// Open for extension, closed for modification
class AssetLoader {
    std::unordered_map<std::string, std::unique_ptr<ILoader>> m_loaders;
public:
    void register_loader(std::string ext, std::unique_ptr<ILoader> loader);
    Asset load(std::filesystem::path path);
};
```

**Dependency Inversion:**
```cpp
// High-level module depends on abstraction
class Renderer {
    IRenderBackend& m_backend;  // Not VulkanBackend directly
};
```

### Error Handling Strategy

```cpp
// Errors at boundaries: Result types
Result<Texture, LoadError> load_texture(path);

// Errors in invariants: assertions
void set_index(size_t i) {
    assert(i < m_size && "Index out of bounds");
    m_index = i;
}

// Errors in systems: error callbacks
using ErrorCallback = std::function<void(Error const&)>;
void set_error_handler(ErrorCallback cb);
```

## Module Boundaries

```
void_engine/
├── core/          # No dependencies
│   ├── math/
│   ├── memory/
│   └── containers/
├── platform/      # Depends: core
│   ├── window/
│   └── input/
├── renderer/      # Depends: core, platform
├── physics/       # Depends: core
├── audio/         # Depends: core
├── scene/         # Depends: core, renderer, physics
└── editor/        # Depends: all (optional)
```

## Review Checklist

- [ ] Clear module boundaries with explicit public APIs
- [ ] Dependencies flow downward only
- [ ] Abstractions at module boundaries
- [ ] No circular dependencies
- [ ] Configuration separated from code
- [ ] Testable in isolation (mockable dependencies)
