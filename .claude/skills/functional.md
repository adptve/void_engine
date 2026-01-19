# Functional Programming Skill

You are an expert in functional programming paradigms applied to C++ and Rust systems programming.

## Core Concepts

### Pure Functions

```cpp
// Pure: same input always produces same output, no side effects
constexpr float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Impure: depends on external state
float get_scaled_value() {
    return m_value * g_global_scale;  // Don't do this
}

// Make dependencies explicit
float get_scaled_value(float value, float scale) {
    return value * scale;
}
```

### Immutability

```cpp
// Prefer const by default
void process(std::vector<Entity> const& entities);

// Return new values instead of mutating
[[nodiscard]] Transform translate(Transform t, glm::vec3 offset) {
    t.position += offset;
    return t;  // Return modified copy
}

// Builder pattern for complex construction
class MeshBuilder {
    MeshData m_data;
public:
    MeshBuilder& add_vertex(Vertex v) & {
        m_data.vertices.push_back(v);
        return *this;
    }
    MeshBuilder&& add_vertex(Vertex v) && {
        m_data.vertices.push_back(v);
        return std::move(*this);
    }
    Mesh build() && { return Mesh{std::move(m_data)}; }
};

auto mesh = MeshBuilder{}
    .add_vertex(v1)
    .add_vertex(v2)
    .build();
```

### Higher-Order Functions

```cpp
// Functions that take or return functions
template<typename F, typename G>
auto compose(F f, G g) {
    return [=](auto x) { return f(g(x)); };
}

auto process = compose(normalize, clamp_to_range);

// Currying / partial application
auto add = [](int a) {
    return [a](int b) { return a + b; };
};
auto add_ten = add(10);

// std::bind_front (C++20)
auto scaled_lerp = std::bind_front(lerp, 0.0f, 1.0f);
```

### Algebraic Data Types

```cpp
// Sum types with std::variant
using Value = std::variant<int, float, std::string, std::nullptr_t>;

// Pattern matching with std::visit
auto to_string = [](auto const& v) -> std::string {
    using T = std::decay_t<decltype(v)>;
    if constexpr (std::is_same_v<T, std::nullptr_t>) {
        return "null";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return v;
    } else {
        return std::to_string(v);
    }
};
std::string s = std::visit(to_string, value);

// Product types (tuples/structs)
using Point = std::tuple<float, float, float>;
auto [x, y, z] = point;
```

### Option/Maybe Type

```cpp
// std::optional for nullable values
std::optional<Entity> find_entity(EntityId id) {
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return std::nullopt;
    return it->second;
}

// Monadic operations (C++23)
auto name = find_entity(id)
    .transform([](Entity& e) { return e.name; })
    .value_or("unknown");

// C++20 alternative
auto name = find_entity(id)
    .and_then([](Entity& e) -> std::optional<std::string> {
        return e.has_name() ? std::optional{e.name} : std::nullopt;
    })
    .value_or("unknown");
```

### Result Type

```cpp
// std::expected (C++23) or custom Result
template<typename T, typename E>
class Result {
    std::variant<T, E> m_value;
public:
    bool is_ok() const { return m_value.index() == 0; }
    T& value() { return std::get<0>(m_value); }
    E& error() { return std::get<1>(m_value); }

    template<typename F>
    auto map(F f) -> Result<decltype(f(value())), E>;

    template<typename F>
    auto and_then(F f) -> decltype(f(value()));
};

// Railway-oriented programming
auto result = load_file(path)
    .and_then(parse_json)
    .and_then(validate_schema)
    .map(create_config);
```

### Ranges and Pipelines

```cpp
#include <ranges>

// Declarative data transformations
auto active_players = entities
    | std::views::filter([](auto& e) { return e.has<Player>(); })
    | std::views::filter([](auto& e) { return e.get<Player>().active; })
    | std::views::transform([](auto& e) -> Player& {
        return e.get<Player>();
    });

// Lazy evaluation - only computes when consumed
for (Player& p : active_players) {
    p.update(dt);
}

// Custom range adaptors
auto to_positions = std::views::transform(
    [](Entity const& e) { return e.get<Transform>().position; }
);
```

### Fold/Reduce

```cpp
// Accumulate with initial value
auto total = std::accumulate(scores.begin(), scores.end(), 0);

// Reduce (C++17) - more parallel-friendly
auto total = std::reduce(std::execution::par,
                         scores.begin(), scores.end());

// Fold expressions (C++17)
template<typename... Args>
auto sum(Args... args) {
    return (args + ...);  // ((a + b) + c) + ...
}

template<typename... Args>
bool all_positive(Args... args) {
    return ((args > 0) && ...);
}
```

### Functional Patterns in ECS

```cpp
// Systems as pure functions
struct MovementSystem {
    void operator()(Transform& t, Velocity const& v, float dt) const {
        t.position += v.linear * dt;
        t.rotation *= glm::quat(v.angular * dt);
    }
};

// Map over entities
template<typename System, typename... Components>
void for_each(World& world, System sys) {
    for (auto [entity, components...] : world.query<Components...>()) {
        sys(components...);
    }
}

for_each<MovementSystem, Transform, Velocity>(world, MovementSystem{});
```

## Rust Functional Patterns

```rust
// Iterator chains
let sum: i32 = entities
    .iter()
    .filter(|e| e.is_active())
    .map(|e| e.score)
    .sum();

// Option combinators
let name = entity
    .get_component::<Name>()
    .map(|n| n.as_str())
    .unwrap_or("unnamed");

// Result combinators
let config = fs::read_to_string(path)?
    .parse::<Config>()?;

// Closures with move semantics
let process = move |x| x + captured_value;
```

## Review Checklist

- [ ] Functions are pure where possible
- [ ] Side effects are pushed to boundaries
- [ ] Prefer transformation over mutation
- [ ] Use std::optional for nullable values
- [ ] Use Result/expected for error handling
- [ ] Leverage ranges for declarative pipelines
