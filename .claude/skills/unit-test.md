# Unit Testing Skill

You are an expert in testing strategies, test frameworks, and test-driven development for C++ and Rust.

## Test Structure

### Catch2 (Recommended for C++)

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("Vector3 operations", "[math][vector]") {
    using namespace void_engine::math;

    SECTION("default construction is zero") {
        Vector3 v;
        REQUIRE(v.x == 0.0f);
        REQUIRE(v.y == 0.0f);
        REQUIRE(v.z == 0.0f);
    }

    SECTION("addition") {
        Vector3 a{1, 2, 3};
        Vector3 b{4, 5, 6};
        auto c = a + b;

        REQUIRE(c.x == 5.0f);
        REQUIRE(c.y == 7.0f);
        REQUIRE(c.z == 9.0f);
    }

    SECTION("normalization") {
        Vector3 v{3, 0, 4};
        auto n = normalize(v);

        REQUIRE_THAT(length(n),
            Catch::Matchers::WithinAbs(1.0f, 0.0001f));
    }
}
```

### GoogleTest Alternative

```cpp
#include <gtest/gtest.h>

class EntityManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<EntityManager>();
    }

    std::unique_ptr<EntityManager> manager;
};

TEST_F(EntityManagerTest, CreateEntity) {
    auto entity = manager->create();
    EXPECT_TRUE(manager->is_valid(entity));
}

TEST_F(EntityManagerTest, DestroyEntity) {
    auto entity = manager->create();
    manager->destroy(entity);
    EXPECT_FALSE(manager->is_valid(entity));
}

// Parameterized tests
class MathTest : public ::testing::TestWithParam<std::tuple<float, float, float>> {};

TEST_P(MathTest, Lerp) {
    auto [a, b, expected] = GetParam();
    EXPECT_FLOAT_EQ(lerp(a, b, 0.5f), expected);
}

INSTANTIATE_TEST_SUITE_P(LerpValues, MathTest, ::testing::Values(
    std::make_tuple(0.0f, 10.0f, 5.0f),
    std::make_tuple(-5.0f, 5.0f, 0.0f)
));
```

### Rust Testing

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn vector_addition() {
        let a = Vec3::new(1.0, 2.0, 3.0);
        let b = Vec3::new(4.0, 5.0, 6.0);
        assert_eq!(a + b, Vec3::new(5.0, 7.0, 9.0));
    }

    #[test]
    #[should_panic(expected = "division by zero")]
    fn normalize_zero_vector_panics() {
        let v = Vec3::ZERO;
        v.normalize();
    }

    // Property-based testing with proptest
    proptest! {
        #[test]
        fn normalize_has_unit_length(x in -1000.0f32..1000.0,
                                      y in -1000.0f32..1000.0,
                                      z in -1000.0f32..1000.0) {
            let v = Vec3::new(x, y, z);
            if v.length() > f32::EPSILON {
                let n = v.normalize();
                prop_assert!((n.length() - 1.0).abs() < 0.0001);
            }
        }
    }
}
```

## Mocking

### Manual Mocks

```cpp
// Interface
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void draw(Mesh const& mesh) = 0;
};

// Mock for testing
class MockRenderer : public IRenderer {
public:
    std::vector<Mesh const*> drawn_meshes;
    int draw_count = 0;

    void draw(Mesh const& mesh) override {
        drawn_meshes.push_back(&mesh);
        ++draw_count;
    }
};

TEST_CASE("Scene renders all entities") {
    MockRenderer renderer;
    Scene scene(renderer);

    scene.add_entity(create_entity());
    scene.add_entity(create_entity());
    scene.render();

    REQUIRE(renderer.draw_count == 2);
}
```

### GMock (with GoogleTest)

```cpp
#include <gmock/gmock.h>

class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(bool, exists, (fs::path const&), (const, override));
    MOCK_METHOD(std::vector<uint8_t>, read, (fs::path const&), (override));
};

TEST_F(AssetLoaderTest, LoadsFromDisk) {
    MockFileSystem fs;
    EXPECT_CALL(fs, exists(testing::_))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(fs, read(testing::_))
        .WillOnce(testing::Return(test_data));

    AssetLoader loader(fs);
    auto asset = loader.load("test.asset");

    EXPECT_TRUE(asset.has_value());
}
```

## Test Patterns

### Arrange-Act-Assert

```cpp
TEST_CASE("Entity component addition") {
    // Arrange
    EntityManager manager;
    auto entity = manager.create();

    // Act
    manager.add_component<Transform>(entity, Transform{});

    // Assert
    REQUIRE(manager.has_component<Transform>(entity));
}
```

### Test Data Builders

```cpp
class EntityBuilder {
    Entity m_entity;
    EntityManager& m_manager;
public:
    explicit EntityBuilder(EntityManager& m) : m_manager(m) {
        m_entity = m_manager.create();
    }

    EntityBuilder& with_transform(glm::vec3 pos = {}) {
        m_manager.add<Transform>(m_entity, {pos});
        return *this;
    }

    EntityBuilder& with_velocity(glm::vec3 vel = {}) {
        m_manager.add<Velocity>(m_entity, {vel});
        return *this;
    }

    Entity build() { return m_entity; }
};

TEST_CASE("Physics updates position") {
    EntityManager manager;
    auto entity = EntityBuilder(manager)
        .with_transform({0, 0, 0})
        .with_velocity({1, 0, 0})
        .build();

    PhysicsSystem{}.update(manager, 1.0f);

    auto& t = manager.get<Transform>(entity);
    REQUIRE(t.position.x == 1.0f);
}
```

### Parameterized Tests

```cpp
TEMPLATE_TEST_CASE("Container operations", "[container]",
                   std::vector<int>, std::deque<int>) {
    TestType container;
    container.push_back(1);
    container.push_back(2);

    REQUIRE(container.size() == 2);
    REQUIRE(container.front() == 1);
}
```

## Testing Strategies

### Unit Tests
- Test single function/class in isolation
- Fast execution (<1ms per test)
- Mock external dependencies

### Integration Tests
- Test module interactions
- May use real dependencies
- Slower but more realistic

### System Tests
- End-to-end scenarios
- Test full engine features
- Frame-based assertions for graphics

```cpp
// Frame-based test for rendering
TEST_CASE("Renderer produces correct output") {
    Engine engine;
    engine.init();

    // Run a few frames
    for (int i = 0; i < 10; ++i) {
        engine.tick();
    }

    // Capture and compare
    auto screenshot = engine.capture_frame();
    REQUIRE(compare_images(screenshot, "expected.png", 0.99f));
}
```

## CMake Integration

```cmake
# tests/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests
    test_main.cpp
    test_math.cpp
    test_ecs.cpp
)

target_link_libraries(tests PRIVATE
    void_engine
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(tests)
```

## Test Checklist

- [ ] Test names describe behavior, not implementation
- [ ] Each test tests one thing
- [ ] Tests are independent (no shared state)
- [ ] Tests run fast (<100ms total for unit tests)
- [ ] Edge cases covered (empty, null, max values)
- [ ] Failure messages are descriptive
- [ ] No test interdependencies
