/// @file test_arena.cpp
/// @brief Tests for Arena allocator

#include <catch2/catch_test_macros.hpp>
#include <void_engine/memory/arena.hpp>

using namespace void_memory;

// Helper for testing object lifetime
namespace {
    int g_counter_instances = 0;

    struct Counter {
        Counter() { ++g_counter_instances; }
        ~Counter() { --g_counter_instances; }
    };
}

TEST_CASE("Arena: creation", "[memory][arena]") {
    Arena arena(1024);
    REQUIRE(arena.capacity() == 1024);
    REQUIRE(arena.used() == 0);
    REQUIRE(arena.available() == 1024);
}

TEST_CASE("Arena: with_capacity_kb", "[memory][arena]") {
    auto arena = Arena::with_capacity_kb(4);
    REQUIRE(arena.capacity() == 4 * 1024);
}

TEST_CASE("Arena: with_capacity_mb", "[memory][arena]") {
    auto arena = Arena::with_capacity_mb(1);
    REQUIRE(arena.capacity() == 1024 * 1024);
}

TEST_CASE("Arena: basic allocation", "[memory][arena]") {
    Arena arena(1024);

    auto* a = arena.alloc(42);
    auto* b = arena.alloc(3.14f);

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(*a == 42);
    REQUIRE(*b == 3.14f);
    REQUIRE(arena.used() > 0);
}

TEST_CASE("Arena: allocate_typed", "[memory][arena]") {
    Arena arena(1024);

    auto* ints = arena.allocate_typed<int>(10);
    REQUIRE(ints != nullptr);

    // Should be properly aligned
    REQUIRE(is_aligned(ints, alignof(int)));
}

TEST_CASE("Arena: alloc_slice", "[memory][arena]") {
    Arena arena(1024);

    int data[] = {1, 2, 3, 4, 5};
    auto* slice = arena.alloc_slice(data, 5);

    REQUIRE(slice != nullptr);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(slice[i] == data[i]);
    }
}

TEST_CASE("Arena: alloc_zeroed", "[memory][arena]") {
    Arena arena(1024);

    auto* zeroed = arena.alloc_zeroed<int>(10);
    REQUIRE(zeroed != nullptr);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(zeroed[i] == 0);
    }
}

TEST_CASE("Arena: reset", "[memory][arena]") {
    Arena arena(1024);

    arena.alloc(42);
    arena.alloc(100);
    REQUIRE(arena.used() > 0);

    arena.reset();
    REQUIRE(arena.used() == 0);
    REQUIRE(arena.available() == 1024);
}

TEST_CASE("Arena: out of memory returns nullptr", "[memory][arena]") {
    Arena arena(64);

    // Fill the arena
    auto* ptr = arena.allocate(64, 1);
    REQUIRE(ptr != nullptr);

    // Should fail
    auto* ptr2 = arena.allocate(1, 1);
    REQUIRE(ptr2 == nullptr);
}

TEST_CASE("Arena: save and restore", "[memory][arena]") {
    Arena arena(1024);

    arena.alloc(1);
    auto state = arena.save();
    std::size_t saved_used = arena.used();

    arena.alloc(2);
    arena.alloc(3);
    REQUIRE(arena.used() > saved_used);

    arena.restore(state);
    REQUIRE(arena.used() == saved_used);
}

TEST_CASE("ArenaScope: automatic restore", "[memory][arena]") {
    Arena arena(1024);

    std::size_t initial = arena.used();

    {
        ArenaScope scope(arena);
        arena.alloc(42);
        arena.alloc(100);
        REQUIRE(arena.used() > initial);
    }

    // Memory reclaimed after scope
    REQUIRE(arena.used() == initial);
}

TEST_CASE("Arena: alignment", "[memory][arena]") {
    Arena arena(1024);

    // Allocate with different alignments
    auto* a1 = arena.allocate(1, 1);
    auto* a2 = arena.allocate(1, 2);
    auto* a4 = arena.allocate(1, 4);
    auto* a8 = arena.allocate(1, 8);
    auto* a16 = arena.allocate(1, 16);

    REQUIRE(is_aligned(a1, 1));
    REQUIRE(is_aligned(a2, 2));
    REQUIRE(is_aligned(a4, 4));
    REQUIRE(is_aligned(a8, 8));
    REQUIRE(is_aligned(a16, 16));
}

TEST_CASE("Arena: deallocate is no-op", "[memory][arena]") {
    Arena arena(1024);

    auto* ptr = arena.alloc(42);
    std::size_t used_before = arena.used();

    // Deallocate should do nothing
    arena.deallocate(ptr, sizeof(int), alignof(int));

    REQUIRE(arena.used() == used_before);
}

TEST_CASE("Arena: create and destroy objects", "[memory][arena]") {
    Arena arena(1024);

    g_counter_instances = 0;

    auto* obj = arena.create<Counter>();
    REQUIRE(obj != nullptr);
    REQUIRE(g_counter_instances == 1);

    arena.destroy(obj);
    REQUIRE(g_counter_instances == 0);
}
