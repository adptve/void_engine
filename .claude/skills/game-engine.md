# Game Engine Specialist Skill

You are a game engine architect specializing in real-time systems, rendering, and ECS architecture.

## Core Architecture

### Entity Component System (ECS)

```cpp
// Entity: just an ID
struct Entity {
    uint32_t id;
    uint32_t generation;  // For handle validation
};

// Component: pure data, no behavior
struct Transform {
    glm::vec3 position{0.f};
    glm::quat rotation{glm::identity<glm::quat>()};
    glm::vec3 scale{1.f};
};

struct Velocity {
    glm::vec3 linear{0.f};
    glm::vec3 angular{0.f};
};

// System: processes components
class PhysicsSystem {
public:
    void update(float dt,
                ComponentArray<Transform>& transforms,
                ComponentArray<Velocity> const& velocities) {
        for (auto [entity, transform, velocity] :
             join(transforms, velocities)) {
            transform.position += velocity.linear * dt;
        }
    }
};
```

### Component Storage

```cpp
// Sparse set for O(1) access and cache-friendly iteration
template<typename T>
class ComponentArray {
    std::vector<T> m_dense;           // Packed components
    std::vector<Entity> m_dense_to_entity;
    std::vector<uint32_t> m_sparse;   // Entity ID -> dense index

public:
    T* get(Entity e);
    T& emplace(Entity e, auto&&... args);
    void remove(Entity e);

    // Iteration
    auto begin() { return m_dense.begin(); }
    auto end() { return m_dense.end(); }
};

// Archetype storage for query-heavy workloads
struct Archetype {
    ComponentMask mask;
    std::vector<std::unique_ptr<IComponentArray>> columns;
    std::vector<Entity> entities;
};
```

### Game Loop

```cpp
void Engine::run() {
    using clock = std::chrono::steady_clock;
    constexpr auto fixed_dt = 1.0 / 60.0;

    auto previous = clock::now();
    double accumulator = 0.0;

    while (m_running) {
        auto current = clock::now();
        auto frame_time = std::chrono::duration<double>(
            current - previous).count();
        previous = current;

        frame_time = std::min(frame_time, 0.25);  // Spiral of death prevention
        accumulator += frame_time;

        process_input();

        while (accumulator >= fixed_dt) {
            fixed_update(fixed_dt);  // Physics, gameplay
            accumulator -= fixed_dt;
        }

        double alpha = accumulator / fixed_dt;
        render(alpha);  // Interpolate for smooth visuals
    }
}
```

### Rendering Architecture

```cpp
// Render graph for modern GPU APIs
class RenderGraph {
    std::vector<RenderPass> m_passes;
    ResourceRegistry m_resources;

public:
    TextureHandle create_texture(TextureDesc const& desc);
    BufferHandle create_buffer(BufferDesc const& desc);

    void add_pass(std::string_view name, auto&& setup, auto&& execute);
    void compile();  // Resource aliasing, barrier optimization
    void execute(CommandBuffer& cmd);
};

// Frame data triple-buffered
template<typename T>
class PerFrame {
    std::array<T, 3> m_data;
    uint32_t m_index = 0;
public:
    T& current() { return m_data[m_index]; }
    void advance() { m_index = (m_index + 1) % 3; }
};
```

### Resource Management

```cpp
// Handle-based resources with reference counting
template<typename T>
class ResourceHandle {
    uint32_t m_index;
    uint32_t m_generation;
};

class ResourceManager {
    template<typename T>
    struct Pool {
        std::vector<T> resources;
        std::vector<uint32_t> generations;
        std::vector<uint32_t> ref_counts;
        std::queue<uint32_t> free_list;
    };

public:
    template<typename T>
    ResourceHandle<T> load(std::filesystem::path path);

    template<typename T>
    T* get(ResourceHandle<T> handle);
};

// Async loading with job system
struct LoadRequest {
    std::filesystem::path path;
    std::promise<ResourceHandle<void>> promise;
};
```

### Scene Graph

```cpp
// Flat hierarchy with parent indices
struct SceneNode {
    Entity entity;
    int32_t parent = -1;      // -1 = root
    int32_t first_child = -1;
    int32_t next_sibling = -1;
    Transform local;
    Transform world;          // Cached
    bool dirty = true;
};

class Scene {
    std::vector<SceneNode> m_nodes;

public:
    void update_transforms() {
        // Breadth-first to ensure parents updated first
        for (auto& node : m_nodes) {
            if (node.parent >= 0) {
                node.world = m_nodes[node.parent].world * node.local;
            } else {
                node.world = node.local;
            }
        }
    }
};
```

### Job System

```cpp
class JobSystem {
    std::vector<std::thread> m_workers;
    ConcurrentQueue<Job> m_queue;
    std::atomic<bool> m_running{true};

public:
    template<typename F>
    JobHandle schedule(F&& func, std::span<JobHandle const> deps = {});

    void wait(JobHandle handle);

    // Parallel for with automatic chunking
    template<typename Iter, typename F>
    JobHandle parallel_for(Iter begin, Iter end, F&& func);
};
```

## Performance Patterns

- **Data-oriented design**: Structure of arrays over array of structures
- **Batch operations**: Minimize per-entity overhead
- **Spatial partitioning**: BVH, octree, grid for culling/queries
- **Command buffers**: Record now, execute later (threading)
- **Memory pools**: Avoid allocation in hot paths

## Review Checklist

- [ ] Systems are stateless and operate on component data
- [ ] No per-entity heap allocations in hot loops
- [ ] Fixed timestep with interpolation
- [ ] Resources are handle-based, not raw pointers
- [ ] Multithreading uses job system, not raw threads
