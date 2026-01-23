// void_ecs Query tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ecs/ecs.hpp>
#include <vector>

using namespace void_ecs;

// Test components
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { int current, max; };
struct Static {};  // Marker component

// =============================================================================
// QueryDescriptor Tests
// =============================================================================

TEST_CASE("QueryDescriptor building", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();
    ComponentId vel_id = world.register_component<Velocity>();
    ComponentId health_id = world.register_component<Health>();

    SECTION("simple read query") {
        auto desc = QueryDescriptor()
            .read(pos_id)
            .build();

        REQUIRE(desc.accesses().size() == 1);
        REQUIRE(desc.accesses()[0].id == pos_id);
        REQUIRE(desc.accesses()[0].access == Access::Read);
    }

    SECTION("multiple components") {
        auto desc = QueryDescriptor()
            .read(pos_id)
            .write(vel_id)
            .build();

        REQUIRE(desc.accesses().size() == 2);
    }

    SECTION("with exclusion") {
        auto desc = QueryDescriptor()
            .read(pos_id)
            .without(health_id)
            .build();

        REQUIRE(desc.accesses().size() == 2);
        REQUIRE(desc.accesses()[1].is_excluded());
    }

    SECTION("optional components") {
        auto desc = QueryDescriptor()
            .read(pos_id)
            .optional_read(vel_id)
            .build();

        REQUIRE(desc.accesses()[0].is_required());
        REQUIRE(desc.accesses()[1].is_optional());
    }
}

TEST_CASE("QueryDescriptor archetype matching", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();
    ComponentId vel_id = world.register_component<Velocity>();
    ComponentId health_id = world.register_component<Health>();
    ComponentId static_id = world.register_component<Static>();

    // Create entities with different component sets
    Entity e1 = build_entity(world)
        .with(Position{0, 0, 0})
        .build();

    Entity e2 = build_entity(world)
        .with(Position{0, 0, 0})
        .with(Velocity{0, 0, 0})
        .build();

    Entity e3 = build_entity(world)
        .with(Position{0, 0, 0})
        .with(Velocity{0, 0, 0})
        .with(Health{100, 100})
        .build();

    // Query for Position only
    auto query1 = QueryDescriptor().read(pos_id).build();

    // Query for Position + Velocity
    auto query2 = QueryDescriptor()
        .read(pos_id)
        .read(vel_id)
        .build();

    // Query for Position without Static
    auto query3 = QueryDescriptor()
        .read(pos_id)
        .without(static_id)
        .build();

    // Get archetypes
    auto loc1 = world.entity_location(e1);
    auto loc2 = world.entity_location(e2);
    auto loc3 = world.entity_location(e3);

    const Archetype* arch1 = world.archetypes().get(loc1->archetype_id);
    const Archetype* arch2 = world.archetypes().get(loc2->archetype_id);
    const Archetype* arch3 = world.archetypes().get(loc3->archetype_id);

    // Position query matches all
    REQUIRE(query1.matches_archetype(*arch1));
    REQUIRE(query1.matches_archetype(*arch2));
    REQUIRE(query1.matches_archetype(*arch3));

    // Position+Velocity query only matches e2, e3
    REQUIRE_FALSE(query2.matches_archetype(*arch1));
    REQUIRE(query2.matches_archetype(*arch2));
    REQUIRE(query2.matches_archetype(*arch3));

    // Position without Static matches all (none have Static)
    REQUIRE(query3.matches_archetype(*arch1));
    REQUIRE(query3.matches_archetype(*arch2));
    REQUIRE(query3.matches_archetype(*arch3));
}

// =============================================================================
// QueryState Tests
// =============================================================================

TEST_CASE("QueryState caching", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();

    // Create initial entity
    build_entity(world).with(Position{0, 0, 0}).build();

    auto state = world.query(QueryDescriptor().read(pos_id).build());

    REQUIRE(state.matched_archetypes().size() == 1);

    // Add more entities to same archetype
    build_entity(world).with(Position{1, 0, 0}).build();
    build_entity(world).with(Position{2, 0, 0}).build();

    // State should still have same matched archetypes
    world.update_query(state);
    REQUIRE(state.matched_archetypes().size() == 1);
}

// =============================================================================
// Query Iteration Tests
// =============================================================================

TEST_CASE("QueryIter basic iteration", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();

    // Create entities
    Entity e1 = build_entity(world).with(Position{1, 0, 0}).build();
    Entity e2 = build_entity(world).with(Position{2, 0, 0}).build();
    Entity e3 = build_entity(world).with(Position{3, 0, 0}).build();

    auto state = world.query(QueryDescriptor().read(pos_id).build());
    auto iter = world.query_iter(state);

    std::vector<Entity> entities;
    while (!iter.empty()) {
        entities.push_back(iter.entity());
        iter.next();
    }

    REQUIRE(entities.size() == 3);

    // All entities should be found
    bool found_e1 = std::find(entities.begin(), entities.end(), e1) != entities.end();
    bool found_e2 = std::find(entities.begin(), entities.end(), e2) != entities.end();
    bool found_e3 = std::find(entities.begin(), entities.end(), e3) != entities.end();

    REQUIRE(found_e1);
    REQUIRE(found_e2);
    REQUIRE(found_e3);
}

TEST_CASE("QueryIter filtered iteration", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();
    ComponentId vel_id = world.register_component<Velocity>();

    // Create mixed entities
    build_entity(world).with(Position{1, 0, 0}).build();  // Position only
    Entity e2 = build_entity(world)
        .with(Position{2, 0, 0})
        .with(Velocity{0, 0, 0})
        .build();  // Position + Velocity
    build_entity(world).with(Position{3, 0, 0}).build();  // Position only

    // Query for Position + Velocity
    auto state = world.query(
        QueryDescriptor()
            .read(pos_id)
            .read(vel_id)
            .build()
    );

    auto iter = world.query_iter(state);

    std::vector<Entity> entities;
    while (!iter.empty()) {
        entities.push_back(iter.entity());
        iter.next();
    }

    // Only e2 should match
    REQUIRE(entities.size() == 1);
    REQUIRE(entities[0] == e2);
}

TEST_CASE("QueryIter with exclusion", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();
    ComponentId static_id = world.register_component<Static>();

    // Create entities
    Entity e1 = build_entity(world).with(Position{1, 0, 0}).build();
    build_entity(world)
        .with(Position{2, 0, 0})
        .with(Static{})
        .build();  // Has Static - should be excluded
    Entity e3 = build_entity(world).with(Position{3, 0, 0}).build();

    // Query for Position without Static
    auto state = world.query(
        QueryDescriptor()
            .read(pos_id)
            .without(static_id)
            .build()
    );

    auto iter = world.query_iter(state);

    std::vector<Entity> entities;
    while (!iter.empty()) {
        entities.push_back(iter.entity());
        iter.next();
    }

    REQUIRE(entities.size() == 2);

    bool found_e1 = std::find(entities.begin(), entities.end(), e1) != entities.end();
    bool found_e3 = std::find(entities.begin(), entities.end(), e3) != entities.end();

    REQUIRE(found_e1);
    REQUIRE(found_e3);
}

TEST_CASE("QueryIter empty query", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();
    ComponentId vel_id = world.register_component<Velocity>();

    // Create entities with Position only
    build_entity(world).with(Position{1, 0, 0}).build();
    build_entity(world).with(Position{2, 0, 0}).build();

    // Query for Velocity (none have it)
    auto state = world.query(QueryDescriptor().read(vel_id).build());
    auto iter = world.query_iter(state);

    REQUIRE(iter.empty());
}

// =============================================================================
// Query Conflict Detection Tests
// =============================================================================

TEST_CASE("QueryDescriptor conflict detection", "[ecs][query]") {
    World world;
    ComponentId pos_id = world.register_component<Position>();
    ComponentId vel_id = world.register_component<Velocity>();

    SECTION("read-read no conflict") {
        auto q1 = QueryDescriptor().read(pos_id).build();
        auto q2 = QueryDescriptor().read(pos_id).build();

        REQUIRE_FALSE(q1.conflicts_with(q2));
    }

    SECTION("read-write conflict") {
        auto q1 = QueryDescriptor().read(pos_id).build();
        auto q2 = QueryDescriptor().write(pos_id).build();

        REQUIRE(q1.conflicts_with(q2));
        REQUIRE(q2.conflicts_with(q1));
    }

    SECTION("write-write conflict") {
        auto q1 = QueryDescriptor().write(pos_id).build();
        auto q2 = QueryDescriptor().write(pos_id).build();

        REQUIRE(q1.conflicts_with(q2));
    }

    SECTION("different components no conflict") {
        auto q1 = QueryDescriptor().write(pos_id).build();
        auto q2 = QueryDescriptor().write(vel_id).build();

        REQUIRE_FALSE(q1.conflicts_with(q2));
    }

    SECTION("excluded component no conflict") {
        auto q1 = QueryDescriptor().write(pos_id).build();
        auto q2 = QueryDescriptor().without(pos_id).build();

        REQUIRE_FALSE(q1.conflicts_with(q2));
    }
}
