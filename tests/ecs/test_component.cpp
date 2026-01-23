// void_ecs Component tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ecs/ecs.hpp>
#include <string>

using namespace void_ecs;

// Test components
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { int current, max; };
struct Name { std::string value; };

// =============================================================================
// ComponentId Tests
// =============================================================================

TEST_CASE("ComponentId construction", "[ecs][component]") {
    SECTION("default is invalid") {
        ComponentId id;
        REQUIRE_FALSE(id.is_valid());
    }

    SECTION("explicit construction") {
        ComponentId id(5);
        REQUIRE(id.is_valid());
        REQUIRE(id.value() == 5);
    }

    SECTION("invalid factory") {
        ComponentId id = ComponentId::invalid();
        REQUIRE_FALSE(id.is_valid());
    }
}

TEST_CASE("ComponentId comparison", "[ecs][component]") {
    ComponentId a(1);
    ComponentId b(1);
    ComponentId c(2);

    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a < c);
}

// =============================================================================
// ComponentInfo Tests
// =============================================================================

TEST_CASE("ComponentInfo creation", "[ecs][component]") {
    SECTION("basic type") {
        ComponentInfo info = ComponentInfo::of<Position>();
        REQUIRE(info.size == sizeof(Position));
        REQUIRE(info.align == alignof(Position));
        REQUIRE(info.drop_fn != nullptr);
        REQUIRE(info.move_fn != nullptr);
        REQUIRE(info.clone_fn == nullptr);  // Not cloneable by default
    }

    SECTION("cloneable type") {
        ComponentInfo info = ComponentInfo::of_cloneable<Position>();
        REQUIRE(info.is_cloneable());
        REQUIRE(info.clone_fn != nullptr);
    }

    SECTION("complex type") {
        ComponentInfo info = ComponentInfo::of<Name>();
        REQUIRE(info.size == sizeof(Name));
        REQUIRE(info.drop_fn != nullptr);
    }
}

// =============================================================================
// ComponentRegistry Tests
// =============================================================================

TEST_CASE("ComponentRegistry registration", "[ecs][component]") {
    ComponentRegistry registry;

    SECTION("register single type") {
        ComponentId id = registry.register_component<Position>();
        REQUIRE(id.is_valid());
        REQUIRE(id.value() == 0);
        REQUIRE(registry.size() == 1);
    }

    SECTION("register multiple types") {
        ComponentId pos_id = registry.register_component<Position>();
        ComponentId vel_id = registry.register_component<Velocity>();
        ComponentId health_id = registry.register_component<Health>();

        REQUIRE(pos_id.value() == 0);
        REQUIRE(vel_id.value() == 1);
        REQUIRE(health_id.value() == 2);
        REQUIRE(registry.size() == 3);
    }

    SECTION("duplicate registration returns same ID") {
        ComponentId id1 = registry.register_component<Position>();
        ComponentId id2 = registry.register_component<Position>();

        REQUIRE(id1 == id2);
        REQUIRE(registry.size() == 1);
    }
}

TEST_CASE("ComponentRegistry lookup", "[ecs][component]") {
    ComponentRegistry registry;
    registry.register_component<Position>();
    registry.register_component<Velocity>();

    SECTION("get_id by type") {
        auto pos_id = registry.get_id<Position>();
        auto vel_id = registry.get_id<Velocity>();
        auto unknown = registry.get_id<Health>();

        REQUIRE(pos_id.has_value());
        REQUIRE(vel_id.has_value());
        REQUIRE_FALSE(unknown.has_value());
    }

    SECTION("get_info by ID") {
        auto pos_id = *registry.get_id<Position>();
        const ComponentInfo* info = registry.get_info(pos_id);

        REQUIRE(info != nullptr);
        REQUIRE(info->size == sizeof(Position));
    }

    SECTION("get_info invalid ID") {
        const ComponentInfo* info = registry.get_info(ComponentId(999));
        REQUIRE(info == nullptr);
    }
}

// =============================================================================
// ComponentStorage Tests
// =============================================================================

TEST_CASE("ComponentStorage construction", "[ecs][component]") {
    ComponentInfo info = ComponentInfo::of<Position>();
    ComponentStorage storage(info);

    REQUIRE(storage.empty());
    REQUIRE(storage.size() == 0);
}

TEST_CASE("ComponentStorage push and get", "[ecs][component]") {
    ComponentInfo info = ComponentInfo::of<Position>();
    ComponentStorage storage(info);

    SECTION("single push") {
        storage.push(Position{1.0f, 2.0f, 3.0f});

        REQUIRE(storage.size() == 1);
        const Position& pos = storage.get<Position>(0);
        REQUIRE(pos.x == 1.0f);
        REQUIRE(pos.y == 2.0f);
        REQUIRE(pos.z == 3.0f);
    }

    SECTION("multiple pushes") {
        storage.push(Position{1.0f, 0.0f, 0.0f});
        storage.push(Position{2.0f, 0.0f, 0.0f});
        storage.push(Position{3.0f, 0.0f, 0.0f});

        REQUIRE(storage.size() == 3);
        REQUIRE(storage.get<Position>(0).x == 1.0f);
        REQUIRE(storage.get<Position>(1).x == 2.0f);
        REQUIRE(storage.get<Position>(2).x == 3.0f);
    }

    SECTION("mutable access") {
        storage.push(Position{0.0f, 0.0f, 0.0f});
        storage.get<Position>(0).x = 42.0f;

        REQUIRE(storage.get<Position>(0).x == 42.0f);
    }
}

TEST_CASE("ComponentStorage swap_remove", "[ecs][component]") {
    ComponentInfo info = ComponentInfo::of<Position>();
    ComponentStorage storage(info);

    storage.push(Position{1.0f, 0.0f, 0.0f});
    storage.push(Position{2.0f, 0.0f, 0.0f});
    storage.push(Position{3.0f, 0.0f, 0.0f});

    SECTION("remove middle") {
        storage.swap_remove(1);

        REQUIRE(storage.size() == 2);
        REQUIRE(storage.get<Position>(0).x == 1.0f);
        REQUIRE(storage.get<Position>(1).x == 3.0f);  // Swapped from last
    }

    SECTION("remove first") {
        storage.swap_remove(0);

        REQUIRE(storage.size() == 2);
        REQUIRE(storage.get<Position>(0).x == 3.0f);  // Swapped from last
    }

    SECTION("remove last") {
        storage.swap_remove(2);

        REQUIRE(storage.size() == 2);
        REQUIRE(storage.get<Position>(0).x == 1.0f);
        REQUIRE(storage.get<Position>(1).x == 2.0f);
    }
}

TEST_CASE("ComponentStorage as_slice", "[ecs][component]") {
    ComponentInfo info = ComponentInfo::of<Position>();
    ComponentStorage storage(info);

    storage.push(Position{1.0f, 0.0f, 0.0f});
    storage.push(Position{2.0f, 0.0f, 0.0f});
    storage.push(Position{3.0f, 0.0f, 0.0f});

    const Position* slice = storage.as_slice<Position>();

    REQUIRE(slice[0].x == 1.0f);
    REQUIRE(slice[1].x == 2.0f);
    REQUIRE(slice[2].x == 3.0f);
}

TEST_CASE("ComponentStorage with complex types", "[ecs][component]") {
    ComponentInfo info = ComponentInfo::of<Name>();
    ComponentStorage storage(info);

    storage.push(Name{"Alice"});
    storage.push(Name{"Bob"});

    REQUIRE(storage.get<Name>(0).value == "Alice");
    REQUIRE(storage.get<Name>(1).value == "Bob");

    // Swap remove should properly destruct
    storage.swap_remove(0);
    REQUIRE(storage.get<Name>(0).value == "Bob");
}

TEST_CASE("ComponentStorage clear", "[ecs][component]") {
    ComponentInfo info = ComponentInfo::of<Position>();
    ComponentStorage storage(info);

    storage.push(Position{1.0f, 0.0f, 0.0f});
    storage.push(Position{2.0f, 0.0f, 0.0f});

    storage.clear();

    REQUIRE(storage.empty());
    REQUIRE(storage.size() == 0);
}
