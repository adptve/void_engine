// void_ecs World tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ecs/ecs.hpp>
#include <string>

using namespace void_ecs;

// Test components
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { int current, max; };

// Test resource
struct GameTime { float elapsed, delta; };

// =============================================================================
// World Entity Tests
// =============================================================================

TEST_CASE("World construction", "[ecs][world]") {
    SECTION("default") {
        World world;
        REQUIRE(world.entity_count() == 0);
    }

    SECTION("with capacity") {
        World world(1000);
        REQUIRE(world.entity_count() == 0);
    }
}

TEST_CASE("World spawn", "[ecs][world]") {
    World world;

    SECTION("single spawn") {
        Entity e = world.spawn();
        REQUIRE(e.is_valid());
        REQUIRE(world.is_alive(e));
        REQUIRE(world.entity_count() == 1);
    }

    SECTION("multiple spawns") {
        Entity e1 = world.spawn();
        Entity e2 = world.spawn();
        Entity e3 = world.spawn();

        REQUIRE(world.entity_count() == 3);
        REQUIRE(world.is_alive(e1));
        REQUIRE(world.is_alive(e2));
        REQUIRE(world.is_alive(e3));
    }
}

TEST_CASE("World despawn", "[ecs][world]") {
    World world;
    Entity e = world.spawn();

    REQUIRE(world.despawn(e));
    REQUIRE_FALSE(world.is_alive(e));
    REQUIRE(world.entity_count() == 0);

    // Despawning again should fail
    REQUIRE_FALSE(world.despawn(e));
}

TEST_CASE("World entity location", "[ecs][world]") {
    World world;
    Entity e = world.spawn();

    auto loc = world.entity_location(e);
    REQUIRE(loc.has_value());
    REQUIRE(loc->archetype_id.is_valid());

    world.despawn(e);
    REQUIRE_FALSE(world.entity_location(e).has_value());
}

// =============================================================================
// World Component Tests
// =============================================================================

TEST_CASE("World component registration", "[ecs][world]") {
    World world;

    ComponentId pos_id = world.register_component<Position>();
    ComponentId vel_id = world.register_component<Velocity>();

    REQUIRE(pos_id.is_valid());
    REQUIRE(vel_id.is_valid());
    REQUIRE(pos_id != vel_id);

    // Duplicate registration returns same ID
    REQUIRE(world.register_component<Position>() == pos_id);
}

TEST_CASE("World add_component", "[ecs][world]") {
    World world;
    world.register_component<Position>();
    world.register_component<Velocity>();

    Entity e = world.spawn();

    SECTION("add single component") {
        REQUIRE(world.add_component(e, Position{1.0f, 2.0f, 3.0f}));
        REQUIRE(world.has_component<Position>(e));
        REQUIRE_FALSE(world.has_component<Velocity>(e));
    }

    SECTION("add multiple components") {
        world.add_component(e, Position{1.0f, 0.0f, 0.0f});
        world.add_component(e, Velocity{0.0f, 1.0f, 0.0f});

        REQUIRE(world.has_component<Position>(e));
        REQUIRE(world.has_component<Velocity>(e));
    }

    SECTION("update existing component") {
        world.add_component(e, Position{1.0f, 0.0f, 0.0f});
        world.add_component(e, Position{2.0f, 0.0f, 0.0f});

        const Position* pos = world.get_component<Position>(e);
        REQUIRE(pos != nullptr);
        REQUIRE(pos->x == 2.0f);
    }
}

TEST_CASE("World get_component", "[ecs][world]") {
    World world;
    world.register_component<Position>();

    Entity e = world.spawn();
    world.add_component(e, Position{1.0f, 2.0f, 3.0f});

    SECTION("immutable access") {
        const Position* pos = world.get_component<Position>(e);
        REQUIRE(pos != nullptr);
        REQUIRE(pos->x == 1.0f);
        REQUIRE(pos->y == 2.0f);
        REQUIRE(pos->z == 3.0f);
    }

    SECTION("mutable access") {
        Position* pos = world.get_component<Position>(e);
        REQUIRE(pos != nullptr);
        pos->x = 42.0f;

        REQUIRE(world.get_component<Position>(e)->x == 42.0f);
    }

    SECTION("missing component returns nullptr") {
        const Velocity* vel = world.get_component<Velocity>(e);
        REQUIRE(vel == nullptr);
    }

    SECTION("dead entity returns nullptr") {
        world.despawn(e);
        REQUIRE(world.get_component<Position>(e) == nullptr);
    }
}

TEST_CASE("World remove_component", "[ecs][world]") {
    World world;
    world.register_component<Position>();
    world.register_component<Velocity>();

    Entity e = world.spawn();
    world.add_component(e, Position{1.0f, 2.0f, 3.0f});
    world.add_component(e, Velocity{0.0f, 0.0f, 0.0f});

    SECTION("remove returns value") {
        auto removed = world.remove_component<Position>(e);
        REQUIRE(removed.has_value());
        REQUIRE(removed->x == 1.0f);
        REQUIRE_FALSE(world.has_component<Position>(e));
        REQUIRE(world.has_component<Velocity>(e));  // Other component still there
    }

    SECTION("remove non-existent returns nullopt") {
        auto removed = world.remove_component<Health>(e);
        REQUIRE_FALSE(removed.has_value());
    }
}

// =============================================================================
// World Resource Tests
// =============================================================================

TEST_CASE("World resources", "[ecs][world]") {
    World world;

    SECTION("insert and get") {
        world.insert_resource(GameTime{1.0f, 0.016f});

        const GameTime* time = world.resource<GameTime>();
        REQUIRE(time != nullptr);
        REQUIRE(time->elapsed == 1.0f);
        REQUIRE(time->delta == 0.016f);
    }

    SECTION("mutable access") {
        world.insert_resource(GameTime{0.0f, 0.0f});

        GameTime* time = world.resource<GameTime>();
        time->elapsed = 5.0f;

        REQUIRE(world.resource<GameTime>()->elapsed == 5.0f);
    }

    SECTION("remove resource") {
        world.insert_resource(GameTime{1.0f, 0.0f});

        auto removed = world.remove_resource<GameTime>();
        REQUIRE(removed.has_value());
        REQUIRE(removed->elapsed == 1.0f);
        REQUIRE(world.resource<GameTime>() == nullptr);
    }

    SECTION("has_resource") {
        REQUIRE_FALSE(world.has_resource<GameTime>());

        world.insert_resource(GameTime{0.0f, 0.0f});
        REQUIRE(world.has_resource<GameTime>());
    }
}

// =============================================================================
// World Builder Tests
// =============================================================================

TEST_CASE("EntityBuilder", "[ecs][world]") {
    World world;
    world.register_component<Position>();
    world.register_component<Velocity>();

    Entity e = build_entity(world)
        .with(Position{1.0f, 2.0f, 3.0f})
        .with(Velocity{4.0f, 5.0f, 6.0f})
        .build();

    REQUIRE(world.is_alive(e));
    REQUIRE(world.has_component<Position>(e));
    REQUIRE(world.has_component<Velocity>(e));

    REQUIRE(world.get_component<Position>(e)->x == 1.0f);
    REQUIRE(world.get_component<Velocity>(e)->x == 4.0f);
}

// =============================================================================
// World Clear Tests
// =============================================================================

TEST_CASE("World clear", "[ecs][world]") {
    World world;
    world.register_component<Position>();

    Entity e1 = world.spawn();
    Entity e2 = world.spawn();
    world.add_component(e1, Position{0.0f, 0.0f, 0.0f});
    world.insert_resource(GameTime{0.0f, 0.0f});

    world.clear();

    REQUIRE(world.entity_count() == 0);
    REQUIRE_FALSE(world.is_alive(e1));
    REQUIRE_FALSE(world.is_alive(e2));
    REQUIRE(world.resource<GameTime>() == nullptr);
}

// =============================================================================
// World Archetype Movement Tests
// =============================================================================

TEST_CASE("World archetype transitions", "[ecs][world]") {
    World world;
    world.register_component<Position>();
    world.register_component<Velocity>();
    world.register_component<Health>();

    Entity e = world.spawn();

    // Start in empty archetype
    auto loc1 = world.entity_location(e);
    REQUIRE(loc1.has_value());

    // Add Position - moves to Position archetype
    world.add_component(e, Position{0.0f, 0.0f, 0.0f});
    auto loc2 = world.entity_location(e);
    REQUIRE(loc2.has_value());
    REQUIRE(loc2->archetype_id != loc1->archetype_id);

    // Add Velocity - moves to Position+Velocity archetype
    world.add_component(e, Velocity{0.0f, 0.0f, 0.0f});
    auto loc3 = world.entity_location(e);
    REQUIRE(loc3.has_value());
    REQUIRE(loc3->archetype_id != loc2->archetype_id);

    // Components still accessible
    REQUIRE(world.get_component<Position>(e) != nullptr);
    REQUIRE(world.get_component<Velocity>(e) != nullptr);

    // Remove Position - moves to Velocity-only archetype
    world.remove_component<Position>(e);
    REQUIRE_FALSE(world.has_component<Position>(e));
    REQUIRE(world.has_component<Velocity>(e));
}

TEST_CASE("World multiple entities same archetype", "[ecs][world]") {
    World world;
    world.register_component<Position>();
    world.register_component<Velocity>();

    // Create multiple entities with same component set
    Entity e1 = build_entity(world)
        .with(Position{1.0f, 0.0f, 0.0f})
        .with(Velocity{0.0f, 0.0f, 0.0f})
        .build();

    Entity e2 = build_entity(world)
        .with(Position{2.0f, 0.0f, 0.0f})
        .with(Velocity{0.0f, 0.0f, 0.0f})
        .build();

    Entity e3 = build_entity(world)
        .with(Position{3.0f, 0.0f, 0.0f})
        .with(Velocity{0.0f, 0.0f, 0.0f})
        .build();

    // All should be in same archetype
    auto loc1 = world.entity_location(e1);
    auto loc2 = world.entity_location(e2);
    auto loc3 = world.entity_location(e3);

    REQUIRE(loc1->archetype_id == loc2->archetype_id);
    REQUIRE(loc2->archetype_id == loc3->archetype_id);

    // But different rows
    REQUIRE(loc1->row != loc2->row);
    REQUIRE(loc2->row != loc3->row);

    // Despawn middle - should swap-remove
    world.despawn(e2);

    REQUIRE(world.get_component<Position>(e1)->x == 1.0f);
    REQUIRE(world.get_component<Position>(e3)->x == 3.0f);
}
