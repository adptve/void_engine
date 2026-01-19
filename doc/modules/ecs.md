# void_ecs Module

Archetype-based Entity Component System for cache-efficient iteration and data-oriented design.

## Core Concepts

### Entity

An entity is simply an ID with generation for use-after-free detection:

```cpp
namespace void_engine::ecs {

struct EntityId {
    uint32_t index;
    uint32_t generation;

    bool is_valid() const { return index != UINT32_MAX; }
    bool operator==(EntityId const&) const = default;
};

// Entity with namespace for isolation
struct EntityRef {
    NamespaceId namespace_id;
    EntityId local_id;
};

} // namespace void_engine::ecs
```

### Component

Components are pure data, no behavior:

```cpp
namespace void_engine::ecs {

// Marker trait (empty base class or concept)
template<typename T>
concept Component = std::is_trivially_copyable_v<T> || requires(T t) {
    { t.clone() } -> std::same_as<T>;
};

// Example components
struct Transform {
    glm::vec3 position{0.f};
    glm::quat rotation{glm::identity<glm::quat>()};
    glm::vec3 scale{1.f};

    glm::mat4 to_matrix() const;
    static Transform from_matrix(glm::mat4 const& m);
};

struct Velocity {
    glm::vec3 linear{0.f};
    glm::vec3 angular{0.f};
};

struct Renderable {
    MeshHandle mesh;
    MaterialHandle material;
    LayerId layer;
    bool visible = true;
    bool cast_shadows = true;
};

struct Name {
    std::string value;
};

} // namespace void_engine::ecs
```

### Component Registry

Maps types to runtime IDs:

```cpp
namespace void_engine::ecs {

using ComponentId = uint32_t;

class ComponentRegistry {
public:
    template<typename T>
    ComponentId register_component();

    template<typename T>
    ComponentId get_id() const;

    ComponentId find(std::string_view name) const;

    size_t component_size(ComponentId id) const;
    std::string_view component_name(ComponentId id) const;

private:
    std::unordered_map<std::type_index, ComponentId> m_type_to_id;
    std::vector<ComponentInfo> m_components;
};

} // namespace void_engine::ecs
```

### Archetype

Groups entities with the same component set for cache-efficient storage:

```cpp
namespace void_engine::ecs {

using ComponentMask = std::bitset<MAX_COMPONENTS>;

class Archetype {
public:
    ComponentMask mask() const;
    size_t entity_count() const;

    template<typename T>
    T* get_component_array();

    template<typename T>
    T const* get_component_array() const;

    EntityId entity_at(size_t index) const;

    void add_entity(EntityId id, /* component data */);
    void remove_entity(EntityId id);

private:
    ComponentMask m_mask;
    std::vector<EntityId> m_entities;
    std::vector<std::unique_ptr<IComponentArray>> m_columns;
};

} // namespace void_engine::ecs
```

### World

Central ECS storage and entity management:

```cpp
namespace void_engine::ecs {

class World {
public:
    // Entity management
    EntityId create();
    EntityId create_with(auto&&... components);
    void destroy(EntityId id);
    bool is_valid(EntityId id) const;

    // Component access
    template<typename T>
    T* get(EntityId id);

    template<typename T>
    T const* get(EntityId id) const;

    template<typename T>
    bool has(EntityId id) const;

    template<typename T>
    T& add(EntityId id, T component = {});

    template<typename T>
    void remove(EntityId id);

    // Queries
    template<typename... Components>
    auto query() -> Query<Components...>;

    template<typename... Components>
    auto query_with() -> Query<Components...>;  // All must have

    template<typename... Components>
    auto query_without() -> Query<Components...>;  // None must have

    // Resources (singleton components)
    template<typename T>
    T& resource();

    template<typename T>
    T const& resource() const;

    template<typename T>
    void insert_resource(T resource);

    // Statistics
    size_t entity_count() const;
    size_t archetype_count() const;

private:
    EntityAllocator m_allocator;
    ComponentRegistry m_components;
    std::vector<std::unique_ptr<Archetype>> m_archetypes;
    std::unordered_map<ComponentMask, Archetype*> m_mask_to_archetype;
    std::unordered_map<std::type_index, std::any> m_resources;
};

} // namespace void_engine::ecs
```

### Query System

Type-safe iteration over entities with specific components:

```cpp
namespace void_engine::ecs {

template<typename... Components>
class Query {
public:
    struct Item {
        EntityId entity;
        std::tuple<Components&...> components;
    };

    class Iterator {
    public:
        Item operator*() const;
        Iterator& operator++();
        bool operator!=(Iterator const& other) const;
    };

    Iterator begin();
    Iterator end();

    size_t count() const;
    bool empty() const;

    // Parallel iteration
    void for_each(auto&& func);
    void par_for_each(auto&& func);  // With job system
};

// Usage:
// for (auto [entity, transform, velocity] : world.query<Transform, Velocity>()) {
//     transform.position += velocity.linear * dt;
// }

} // namespace void_engine::ecs
```

### Systems

Systems are functions that operate on component data:

```cpp
namespace void_engine::ecs {

// System as a function
void physics_system(World& world, float dt) {
    for (auto [entity, transform, velocity] : world.query<Transform, Velocity>()) {
        transform.position += velocity.linear * dt;
        transform.rotation *= glm::quat(velocity.angular * dt);
    }
}

// System as a class
class PhysicsSystem {
public:
    void update(World& world, float dt) {
        for (auto [entity, t, v] : world.query<Transform, Velocity>()) {
            t.position += v.linear * dt;
        }
    }
};

// System scheduler
class SystemScheduler {
public:
    template<typename F>
    void add_system(std::string_view name, F&& system);

    void add_dependency(std::string_view after, std::string_view before);

    void run(World& world, float dt);  // Respects dependencies
    void run_parallel(World& world, float dt);  // With job system

private:
    std::vector<SystemEntry> m_systems;
    // Topologically sorted execution order
};

} // namespace void_engine::ecs
```

## Sparse Set Storage

Alternative storage for frequently added/removed components:

```cpp
namespace void_engine::ecs {

template<typename T>
class SparseSet {
public:
    bool contains(EntityId id) const;
    T* get(EntityId id);
    T const* get(EntityId id) const;

    T& insert(EntityId id, T value);
    void remove(EntityId id);
    void clear();

    size_t size() const;

    // Dense iteration (cache-friendly)
    auto begin() { return m_dense.begin(); }
    auto end() { return m_dense.end(); }

private:
    std::vector<T> m_dense;
    std::vector<EntityId> m_dense_to_entity;
    std::vector<uint32_t> m_sparse;  // entity.index -> dense index
};

} // namespace void_engine::ecs
```

## Usage Examples

### Creating and Querying Entities

```cpp
World world;

// Create entity with components
auto player = world.create_with(
    Transform{{0, 0, 0}},
    Velocity{{1, 0, 0}},
    Name{"Player"}
);

// Add component later
world.add<Health>(player, {.max = 100, .current = 100});

// Query and iterate
for (auto [entity, transform, velocity] :
     world.query<Transform, Velocity>()) {
    transform.position += velocity.linear * dt;
}

// Single entity access
if (auto* hp = world.get<Health>(player)) {
    hp->current -= 10;
}
```

### Using Resources

```cpp
// Insert singleton resource
world.insert_resource(Time{.delta = 0.016f, .total = 0.0f});

// Access in system
void update_system(World& world, float dt) {
    auto& time = world.resource<Time>();
    time.total += dt;
}
```
