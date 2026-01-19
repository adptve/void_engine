# Refactoring Skill

You are an expert in safe, systematic code refactoring and technical debt reduction.

## Refactoring Process

### 1. Characterize Before Changing

```cpp
// Before refactoring, ensure tests exist
// If not, write characterization tests first

TEST_CASE("Legacy behavior - document current output") {
    auto result = legacy_function(test_input);
    // Capture current behavior, even if "wrong"
    CHECK(result == current_observed_output);
}
```

### 2. Small, Incremental Steps

Each step should:
- Compile successfully
- Pass all tests
- Be committable independently

### Extract Function

```cpp
// Before: complex inline logic
void process() {
    // ... 50 lines of validation
    // ... 30 lines of transformation
    // ... 20 lines of output
}

// After: extracted functions
void process() {
    if (!validate(input)) return;
    auto result = transform(input);
    output(result);
}

bool validate(Input const& input) { /* ... */ }
Result transform(Input const& input) { /* ... */ }
void output(Result const& result) { /* ... */ }
```

### Extract Class

```cpp
// Before: class with multiple responsibilities
class Game {
    // Player management
    void spawn_player();
    void update_player();
    std::vector<Player> m_players;

    // Rendering
    void render();
    RenderTarget m_target;
    // ...
};

// After: single responsibility
class PlayerManager {
    void spawn();
    void update();
    std::vector<Player> m_players;
};

class Renderer {
    void render(Scene const& scene);
    RenderTarget m_target;
};

class Game {
    PlayerManager m_players;
    Renderer m_renderer;
};
```

### Replace Conditional with Polymorphism

```cpp
// Before: type checking with switch
void process(Entity& e) {
    switch (e.type) {
        case EntityType::Player:
            update_player(e);
            break;
        case EntityType::Enemy:
            update_enemy(e);
            break;
        // ... many cases
    }
}

// After: virtual dispatch or variant
struct IEntity {
    virtual void update() = 0;
};

// Or with std::variant + visitor
using Entity = std::variant<Player, Enemy, Item>;
void process(Entity& e) {
    std::visit([](auto& entity) { entity.update(); }, e);
}
```

### Introduce Parameter Object

```cpp
// Before: long parameter list
void create_window(int x, int y, int width, int height,
                   char const* title, bool fullscreen,
                   bool vsync, int msaa);

// After: parameter object
struct WindowConfig {
    int x = 0, y = 0;
    int width = 1280, height = 720;
    std::string title = "Window";
    bool fullscreen = false;
    bool vsync = true;
    int msaa = 4;
};

void create_window(WindowConfig const& config);

// Usage with designated initializers (C++20)
create_window({.width = 1920, .height = 1080, .vsync = true});
```

### Replace Inheritance with Composition

```cpp
// Before: inheritance for code reuse
class Button : public Widget, public Clickable, public Drawable {
    // Diamond problem, tight coupling
};

// After: composition
class Button {
    WidgetState m_state;
    ClickHandler m_click;
    DrawStyle m_style;

public:
    void on_click(auto handler) { m_click.set(handler); }
    void draw(Canvas& c) { draw_button(c, m_style, m_state); }
};
```

## Code Smells & Remedies

| Smell | Remedy |
|-------|--------|
| Long function (>30 lines) | Extract Function |
| Long parameter list (>4) | Introduce Parameter Object |
| Duplicated code | Extract Function/Template |
| Feature envy | Move Method |
| Data clumps | Extract Class |
| Primitive obsession | Introduce Value Object |
| Switch statements | Polymorphism/Variant |
| Parallel inheritance | Merge hierarchies |
| Speculative generality | Remove unused abstractions |

## Legacy Code Strategies

### Seam Identification

```cpp
// Find seams: places where behavior can be altered without editing
class LegacyProcessor {
    Database* m_db;  // Seam: can inject mock
public:
    explicit LegacyProcessor(Database* db) : m_db(db) {}
};

// Preprocessing seam
#ifdef TESTING
    #define DB_QUERY(q) mock_query(q)
#else
    #define DB_QUERY(q) real_query(q)
#endif
```

### Sprout Method/Class

```cpp
// Need to add feature but can't test existing code
class LegacyClass {
    void complex_untested_method() {
        // ... existing untested code ...

        // Sprout: new tested code in separate method
        auto result = new_feature(extracted_data);

        // ... rest of untested code ...
    }

    // New, fully tested method
    Result new_feature(Data const& data);
};
```

### Wrap Method

```cpp
// Add behavior before/after without modifying original
class Original {
    void do_work() { /* legacy code */ }
};

class Wrapper {
    Original m_original;
public:
    void do_work() {
        log_start();
        m_original.do_work();
        log_end();
    }
};
```

## Mechanical Refactorings

Safe transformations that preserve behavior:

```cpp
// Rename (IDE-assisted)
// old: getData() -> new: get_data()

// Move to separate file
// Extract class/function to new .hpp/.cpp

// Change signature
void f(int a, int b);  // -> void f(Params p);

// Inline (opposite of extract)
auto x = get_value();  // -> auto x = m_value;

// Convert loop to algorithm
for (auto& x : v) { sum += x; }
// -> auto sum = std::accumulate(v.begin(), v.end(), 0);
```

## Review Checklist

- [ ] Tests pass before and after each step
- [ ] Each commit is a single, atomic refactoring
- [ ] No behavior changes mixed with refactoring
- [ ] Code compiles at each step
- [ ] Renamed symbols updated across entire codebase
- [ ] No dead code left behind
