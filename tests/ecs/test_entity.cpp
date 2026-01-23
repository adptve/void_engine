// void_ecs Entity tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ecs/ecs.hpp>
#include <unordered_set>

using namespace void_ecs;

// =============================================================================
// Entity Tests
// =============================================================================

TEST_CASE("Entity construction", "[ecs][entity]") {
    SECTION("default is null") {
        Entity e;
        REQUIRE(e.is_null());
        REQUIRE_FALSE(e.is_valid());
        REQUIRE_FALSE(static_cast<bool>(e));
    }

    SECTION("null factory") {
        Entity e = Entity::null();
        REQUIRE(e.is_null());
    }

    SECTION("explicit construction") {
        Entity e(5, 3);
        REQUIRE(e.index == 5);
        REQUIRE(e.generation == 3);
        REQUIRE(e.is_valid());
        REQUIRE_FALSE(e.is_null());
    }
}

TEST_CASE("Entity bit encoding", "[ecs][entity]") {
    Entity original(1234, 5678);
    std::uint64_t bits = original.to_bits();
    Entity decoded = Entity::from_bits(bits);

    REQUIRE(decoded.index == original.index);
    REQUIRE(decoded.generation == original.generation);
    REQUIRE(decoded == original);
}

TEST_CASE("Entity comparison", "[ecs][entity]") {
    Entity a(1, 1);
    Entity b(1, 1);
    Entity c(2, 1);
    Entity d(1, 2);

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
    REQUIRE(a != c);
    REQUIRE(a < c);  // Index comparison first
    REQUIRE(a < d);  // Same index, generation comparison
}

TEST_CASE("Entity hashing", "[ecs][entity]") {
    Entity e1(1, 1);
    Entity e2(1, 1);
    Entity e3(2, 1);

    std::unordered_set<Entity> set;
    set.insert(e1);

    REQUIRE(set.count(e2) == 1);  // Same as e1
    REQUIRE(set.count(e3) == 0);  // Different
}

TEST_CASE("Entity string representation", "[ecs][entity]") {
    Entity e(42, 7);
    std::string str = e.to_string();
    REQUIRE(str.find("42") != std::string::npos);
    REQUIRE(str.find("7") != std::string::npos);

    Entity null_e;
    REQUIRE(null_e.to_string().find("null") != std::string::npos);
}

// =============================================================================
// EntityAllocator Tests
// =============================================================================

TEST_CASE("EntityAllocator construction", "[ecs][entity]") {
    SECTION("default empty") {
        EntityAllocator alloc;
        REQUIRE(alloc.empty());
        REQUIRE(alloc.alive_count() == 0);
    }

    SECTION("with capacity") {
        EntityAllocator alloc(100);
        REQUIRE(alloc.empty());
    }
}

TEST_CASE("EntityAllocator allocate", "[ecs][entity]") {
    EntityAllocator alloc;

    SECTION("single allocation") {
        Entity e = alloc.allocate();
        REQUIRE(e.is_valid());
        REQUIRE(e.index == 0);
        REQUIRE(e.generation == 0);
        REQUIRE(alloc.alive_count() == 1);
        REQUIRE(alloc.is_alive(e));
    }

    SECTION("multiple allocations") {
        Entity e1 = alloc.allocate();
        Entity e2 = alloc.allocate();
        Entity e3 = alloc.allocate();

        REQUIRE(e1.index == 0);
        REQUIRE(e2.index == 1);
        REQUIRE(e3.index == 2);
        REQUIRE(alloc.alive_count() == 3);
    }
}

TEST_CASE("EntityAllocator deallocate", "[ecs][entity]") {
    EntityAllocator alloc;

    Entity e = alloc.allocate();
    REQUIRE(alloc.is_alive(e));

    bool result = alloc.deallocate(e);
    REQUIRE(result);
    REQUIRE_FALSE(alloc.is_alive(e));
    REQUIRE(alloc.alive_count() == 0);

    // Deallocating again should fail
    REQUIRE_FALSE(alloc.deallocate(e));
}

TEST_CASE("EntityAllocator generation tracking", "[ecs][entity]") {
    EntityAllocator alloc;

    Entity e1 = alloc.allocate();
    alloc.deallocate(e1);

    // Allocate again - should reuse index with new generation
    Entity e2 = alloc.allocate();

    REQUIRE(e2.index == e1.index);
    REQUIRE(e2.generation == e1.generation + 1);

    // Old entity should not be alive
    REQUIRE_FALSE(alloc.is_alive(e1));
    REQUIRE(alloc.is_alive(e2));
}

TEST_CASE("EntityAllocator free list reuse", "[ecs][entity]") {
    EntityAllocator alloc;

    Entity e1 = alloc.allocate();
    Entity e2 = alloc.allocate();
    Entity e3 = alloc.allocate();

    // Deallocate middle entity
    alloc.deallocate(e2);

    // Next allocation should reuse e2's index
    Entity e4 = alloc.allocate();
    REQUIRE(e4.index == e2.index);
    REQUIRE(e4.generation == e2.generation + 1);
}

TEST_CASE("EntityAllocator clear", "[ecs][entity]") {
    EntityAllocator alloc;

    Entity e1 = alloc.allocate();
    Entity e2 = alloc.allocate();

    alloc.clear();

    REQUIRE(alloc.empty());
    REQUIRE(alloc.capacity() == 0);
    REQUIRE_FALSE(alloc.is_alive(e1));
    REQUIRE_FALSE(alloc.is_alive(e2));
}

TEST_CASE("EntityAllocator is_alive edge cases", "[ecs][entity]") {
    EntityAllocator alloc;

    // Null entity
    REQUIRE_FALSE(alloc.is_alive(Entity::null()));

    // Entity with out-of-range index
    REQUIRE_FALSE(alloc.is_alive(Entity(1000, 0)));

    // Valid allocation
    Entity e = alloc.allocate();
    REQUIRE(alloc.is_alive(e));

    // Wrong generation
    Entity wrong_gen(e.index, e.generation + 1);
    REQUIRE_FALSE(alloc.is_alive(wrong_gen));
}
