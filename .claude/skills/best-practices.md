# Best Practices Skill

You are an expert in software engineering best practices for game engine development.

## Code Quality

### Naming Conventions

```cpp
namespace void_engine {          // snake_case namespaces
class RenderSystem;              // PascalCase types
void process_input();            // snake_case functions
constexpr int k_max_entities;    // k_ prefix constants
int m_member_variable;           // m_ prefix members
int s_static_variable;           // s_ prefix statics
template<typename ValueT>        // T suffix type params
bool is_valid();                 // is_, has_, can_ for booleans
int entity_count;                // Noun for variables
void update_physics();           // Verb for functions
}
```

### Const Correctness

```cpp
// Const by default
void process(std::vector<Entity> const& entities);
int get_count() const;
Entity const* find(EntityId id) const;

// Mutable only when mutation is the point
void add_entity(Entity entity);
Entity& get_mut(EntityId id);

// Const propagation through pointers
struct Node {
    std::unique_ptr<Node> child;
    // child is const in const context due to propagate_const (C++20)
    // or use custom wrapper
};
```

### Error Handling

```cpp
// Use appropriate mechanism for each case

// Expected failures: std::optional / std::expected
std::optional<Config> load_config(fs::path path);
std::expected<Texture, LoadError> load_texture(fs::path path);

// Precondition violations: assertions
void set_index(size_t i) {
    assert(i < m_size);  // Programming error if false
    m_index = i;
}

// Unrecoverable: exceptions (at system boundaries)
if (!vulkan_available) {
    throw std::runtime_error("Vulkan not available");
}

// Never: error codes mixed with return values
// int load() { return -1; }  // Don't do this
```

### Resource Management

```cpp
// RAII everywhere
class File {
    FILE* m_handle;
public:
    explicit File(char const* path) : m_handle(fopen(path, "r")) {
        if (!m_handle) throw std::runtime_error("Failed to open");
    }
    ~File() { if (m_handle) fclose(m_handle); }

    // Rule of 5: if you define destructor, define all
    File(File const&) = delete;
    File& operator=(File const&) = delete;
    File(File&& other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}
    File& operator=(File&& other) noexcept {
        if (this != &other) {
            if (m_handle) fclose(m_handle);
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }
};

// Or rule of 0: use smart pointers
struct FileDeleter {
    void operator()(FILE* f) { if (f) fclose(f); }
};
using File = std::unique_ptr<FILE, FileDeleter>;
```

### Thread Safety

```cpp
// Document thread safety explicitly
/// @thread_safety This class is NOT thread-safe
class EntityManager { /* ... */ };

/// @thread_safety Thread-safe after construction
class ResourceCache {
    mutable std::shared_mutex m_mutex;
    std::unordered_map<Key, Resource> m_cache;

public:
    Resource const* get(Key k) const {
        std::shared_lock lock(m_mutex);
        auto it = m_cache.find(k);
        return it != m_cache.end() ? &it->second : nullptr;
    }

    void insert(Key k, Resource r) {
        std::unique_lock lock(m_mutex);
        m_cache.emplace(k, std::move(r));
    }
};
```

## Performance

### Allocation Strategy

```cpp
// 1. Prefer stack allocation
std::array<float, 16> matrix;

// 2. Reserve capacity for vectors
std::vector<Entity> entities;
entities.reserve(1000);

// 3. Use object pools for frequently allocated objects
template<typename T>
class Pool {
    std::vector<T> m_storage;
    std::vector<T*> m_free;
public:
    T* acquire();
    void release(T* obj);
};

// 4. Arena allocation for frame-scoped data
class FrameArena {
    std::vector<uint8_t> m_buffer;
    size_t m_offset = 0;
public:
    void* alloc(size_t size, size_t align);
    void reset() { m_offset = 0; }
};
```

### Cache Efficiency

```cpp
// Structure of Arrays (SoA) for iteration
struct Particles {
    std::vector<float> x, y, z;      // Positions
    std::vector<float> vx, vy, vz;   // Velocities
    std::vector<float> life;
};

// vs Array of Structures (AoS) - worse cache usage
struct Particle { float x, y, z, vx, vy, vz, life; };
std::vector<Particle> particles;

// Prefer contiguous containers
std::vector<T> > std::list<T>  // Almost always
std::vector<T> > std::map<K,V> // For small N, even for lookup
```

## API Design

### Interface Design

```cpp
// Minimal, complete, orthogonal
class Renderer {
public:
    void draw(Mesh const& mesh, Material const& material);
    // NOT: draw_mesh(), draw_textured_mesh(), draw_lit_mesh(), etc.
};

// Accept the widest type that works
void process(std::span<float const> data);  // Not vector<float>&
void print(std::string_view text);          // Not const string&

// Return the narrowest type that's useful
std::unique_ptr<Widget> create();  // Caller chooses storage
std::vector<int> compute();        // Clear ownership
```

### Builder Pattern

```cpp
class PipelineBuilder {
    PipelineDesc m_desc;
public:
    PipelineBuilder& set_shader(Shader s) & {
        m_desc.shader = s; return *this;
    }
    PipelineBuilder&& set_shader(Shader s) && {
        m_desc.shader = s; return std::move(*this);
    }
    Pipeline build() &&;
};

auto pipeline = PipelineBuilder{}
    .set_shader(shader)
    .set_blend_mode(BlendMode::Alpha)
    .build();
```

## Defensive Programming

```cpp
// Validate at boundaries
void load_from_file(fs::path path) {
    if (!fs::exists(path)) {
        throw std::invalid_argument("File not found: " + path.string());
    }
    // ... proceed knowing file exists
}

// Internal invariants with asserts
void remove(size_t index) {
    assert(index < m_size && "Index out of bounds");
    // ... implementation
}

// Sanitizers in debug builds
// -fsanitize=address,undefined (Clang/GCC)
// /fsanitize=address (MSVC)
```

## Code Review Checklist

- [ ] Names are descriptive and follow conventions
- [ ] Functions do one thing
- [ ] No magic numbers (use named constants)
- [ ] Error handling is appropriate
- [ ] Resources are managed via RAII
- [ ] Thread safety is documented
- [ ] No premature optimization
- [ ] No dead code or commented-out code
- [ ] Public API is minimal and documented
