# Developer Guide

## Getting Started

### Prerequisites

- **Compiler**: C++20 compatible (GCC 11+, Clang 14+, MSVC 2022+)
- **CMake**: 3.20+
- **GPU**: Vulkan 1.2+ capable

### Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure

# Run engine
./build/void_runtime
```

### Project Structure

```
void_engine/
├── include/void_engine/    # Public headers
├── src/                    # Implementation
├── tests/                  # Unit tests
├── doc/                    # Documentation
├── legacy/                 # Original Rust codebase (reference)
└── examples/               # Example projects
```

## Core Concepts

### Entity-Component-System

Entities are IDs. Components are data. Systems are functions.

```cpp
#include <void_engine/ecs/world.hpp>

using namespace void_engine::ecs;

// Define components (pure data)
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };

// Create world and entities
World world;
auto entity = world.create_with(
    Position{0, 0, 0},
    Velocity{1, 0, 0}
);

// Query and update
for (auto [e, pos, vel] : world.query<Position, Velocity>()) {
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
    pos.z += vel.z * dt;
}
```

### Render Layers

Content is rendered to isolated layers, then composited:

```cpp
#include <void_engine/render/layer.hpp>

// Create layers
auto world_layer = compositor.create_layer({
    .name = "world",
    .type = LayerType::Content,
    .priority = 0
});

auto ui_layer = compositor.create_layer({
    .name = "ui",
    .type = LayerType::Overlay,
    .priority = 100
});

// Render to layers
renderer.set_target(world_layer);
renderer.draw_scene(scene);

renderer.set_target(ui_layer);
renderer.draw_ui(ui);

// Composite all layers
compositor.composite(output);
```

### Hot-Reload

Assets and code reload automatically during development:

```cpp
#include <void_engine/asset/server.hpp>

AssetServer assets;
assets.enable_hot_reload(true);

// Load shader (automatically watched for changes)
auto shader = assets.load<Shader>("shaders/pbr.wgsl");

// In render loop - apply any pending reloads
void Engine::update() {
    assets.process_hot_reload_events();
    // ... rest of update
}
```

### IR Patches

State changes are declared, not mutated directly:

```cpp
#include <void_engine/ir/transaction.hpp>

// Build transaction
auto tx = TransactionBuilder(my_namespace)
    .description("Spawn player")
    .create_entity(player_ref, "Player")
    .set_component(player_ref, "Transform", Value::Object{
        {"position", {0.0, 0.0, 0.0}},
        {"scale", {1.0, 1.0, 1.0}}
    })
    .build();

// Submit to patch bus
patch_bus.submit(std::move(tx));
```

## Creating a Game

### Minimal Setup

```cpp
#include <void_engine/engine/engine.hpp>

int main() {
    void_engine::Engine engine;

    if (!engine.init()) {
        return 1;
    }

    // Load initial scene
    engine.load_scene("scenes/main.toml");

    // Run game loop
    engine.run();

    return 0;
}
```

### Scene File (TOML)

```toml
[scene]
name = "Main Scene"

[[entities]]
name = "player"
archetype = "Player"

[entities.transform]
position = [0, 1, 0]
rotation = [0, 0, 0, 1]
scale = [1, 1, 1]

[entities.renderable]
mesh = "meshes/character.glb"
material = "materials/player.toml"

[entities.player_controller]
speed = 5.0
jump_force = 10.0

[[entities]]
name = "ground"
archetype = "Static"

[entities.transform]
position = [0, 0, 0]
scale = [100, 1, 100]

[entities.renderable]
mesh = "cube"
material = "materials/ground.toml"

[entities.collider]
shape = "box"
size = [100, 1, 100]
```

### Custom Components

```cpp
// Define component
struct PlayerController {
    float speed = 5.0f;
    float jump_force = 10.0f;
    bool grounded = false;
};

// Register with world
world.register_component<PlayerController>("PlayerController");

// Create system
void player_movement_system(World& world, float dt, Input const& input) {
    for (auto [entity, transform, controller] :
         world.query<Transform, PlayerController>()) {

        glm::vec3 move{0};
        if (input.key_held(Key::W)) move.z -= 1;
        if (input.key_held(Key::S)) move.z += 1;
        if (input.key_held(Key::A)) move.x -= 1;
        if (input.key_held(Key::D)) move.x += 1;

        if (glm::length(move) > 0) {
            move = glm::normalize(move) * controller.speed * dt;
            transform.position += move;
        }

        if (controller.grounded && input.key_pressed(Key::Space)) {
            // Apply jump via physics system
        }
    }
}
```

### Custom Materials

```toml
# materials/player.toml
[material]
shader = "shaders/pbr.wgsl"
blend_mode = "opaque"

[material.properties]
base_color = [0.8, 0.2, 0.2, 1.0]
metallic = 0.0
roughness = 0.5

[material.textures]
albedo = "textures/player_albedo.png"
normal = "textures/player_normal.png"
```

## Best Practices

### Performance

1. **Use queries over individual lookups**
```cpp
// Good: batch processing
for (auto [e, t, v] : world.query<Transform, Velocity>()) {
    t.position += v.linear * dt;
}

// Avoid: per-entity lookup
for (auto entity : entities) {
    auto* t = world.get<Transform>(entity);  // Cache miss
    auto* v = world.get<Velocity>(entity);   // Cache miss
    t->position += v->linear * dt;
}
```

2. **Prefer data-oriented layouts**
```cpp
// Good: separate arrays (SoA)
struct Particles {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<float> lifetimes;
};

// Avoid: array of structs (AoS) for large collections
struct Particle { glm::vec3 pos, vel; float life; };
std::vector<Particle> particles;  // Wastes cache on partial reads
```

3. **Avoid allocations in hot loops**
```cpp
// Good: pre-allocate
std::vector<DrawCall> draw_calls;
draw_calls.reserve(1000);

void render() {
    draw_calls.clear();  // No allocation
    collect_draw_calls(draw_calls);
    execute_draw_calls(draw_calls);
}
```

### Hot-Reload Safety

1. **Separate persistent from transient state**
```cpp
struct MySystem {
    // Serialized during hot-reload
    std::vector<EntityId> tracked;

    // Rebuilt after reload
    std::unordered_map<EntityId, Cache> cache;  // Skip serialization
};
```

2. **Handle missing resources gracefully**
```cpp
void draw(MeshHandle mesh) {
    if (auto* m = mesh_cache.get(mesh)) {
        render(*m);
    } else {
        render(placeholder_cube);  // Asset reloading
    }
}
```

### Error Handling

```cpp
// Use Result for expected failures
Result<Asset, AssetError> load_asset(path);

// Use exceptions for exceptional conditions (at boundaries)
void Engine::init() {
    if (!vulkan_available()) {
        throw std::runtime_error("Vulkan not available");
    }
}

// Use assert for programmer errors
void set_index(size_t i) {
    assert(i < m_size && "Index out of bounds");
    m_index = i;
}
```

## Debugging

### Console Commands

```
> help                    # List commands
> spawn Player 0 0 0      # Spawn entity
> inspect player          # Show entity components
> reload shaders          # Force shader reload
> stats                   # Show performance stats
> layer list              # List render layers
> layer hide ui           # Hide layer
```

### Visual Debug

```cpp
// Enable debug drawing
debug_draw.enable(true);

// Draw debug shapes
debug_draw.line(start, end, Color::Red);
debug_draw.box(bounds, Color::Green);
debug_draw.sphere(center, radius, Color::Blue);

// Draw component gizmos
debug_draw.transform(entity_transform);
debug_draw.collider(entity_collider);
```

### Profiling

```cpp
#include <void_engine/debug/profiler.hpp>

void update() {
    PROFILE_SCOPE("Update");

    {
        PROFILE_SCOPE("Physics");
        physics.step(dt);
    }

    {
        PROFILE_SCOPE("AI");
        ai.update(dt);
    }
}

// In debug UI
profiler.draw_timeline();
profiler.draw_flame_graph();
```

## Testing

### Unit Tests

```cpp
#include <catch2/catch_test_macros.hpp>
#include <void_engine/ecs/world.hpp>

TEST_CASE("Entity creation", "[ecs]") {
    World world;
    auto entity = world.create();

    REQUIRE(world.is_valid(entity));
    REQUIRE(world.entity_count() == 1);

    world.destroy(entity);
    REQUIRE(!world.is_valid(entity));
}

TEST_CASE("Component query", "[ecs]") {
    World world;

    auto e1 = world.create_with(Position{1, 0, 0}, Velocity{1, 0, 0});
    auto e2 = world.create_with(Position{2, 0, 0});  // No velocity

    int count = 0;
    for (auto [e, p, v] : world.query<Position, Velocity>()) {
        count++;
    }

    REQUIRE(count == 1);  // Only e1 has both
}
```

### Integration Tests

```cpp
TEST_CASE("Scene loading", "[engine]") {
    Engine engine;
    engine.init();

    auto result = engine.load_scene("test_scenes/simple.toml");
    REQUIRE(result.is_ok());

    auto* player = engine.find_entity("player");
    REQUIRE(player != nullptr);
    REQUIRE(engine.has_component<Transform>(*player));
}
```

### Hot-Reload Tests

```cpp
TEST_CASE("Shader hot-reload", "[render]") {
    ShaderManager shaders;
    shaders.enable_hot_reload(true);

    auto handle = shaders.load("test.wgsl");
    auto* original = shaders.get(handle);

    // Modify file
    write_file("test.wgsl", modified_source);
    shaders.check_for_changes();
    shaders.apply_pending_reloads();

    auto* reloaded = shaders.get(handle);
    REQUIRE(reloaded != original);  // New shader module
    REQUIRE(shaders.is_valid(handle));  // Handle still valid
}
```
