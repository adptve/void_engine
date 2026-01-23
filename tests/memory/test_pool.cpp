/// @file test_pool.cpp
/// @brief Tests for Pool allocator

#include <catch2/catch_test_macros.hpp>
#include <void_engine/memory/pool.hpp>
#include <vector>

using namespace void_memory;

TEST_CASE("Pool: creation", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 100);

    REQUIRE(pool.block_count() == 100);
    REQUIRE(pool.allocated_count() == 0);
    REQUIRE(pool.free_count() == 100);
}

TEST_CASE("Pool: for_type", "[memory][pool]") {
    auto pool = Pool::for_type<double>(50);

    REQUIRE(pool.block_count() == 50);
    REQUIRE(pool.block_size() >= sizeof(double));
}

TEST_CASE("Pool: basic allocation", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 100);

    auto* ptr1 = pool.alloc_block();
    auto* ptr2 = pool.alloc_block();

    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(ptr1 != ptr2);
    REQUIRE(pool.allocated_count() == 2);
}

TEST_CASE("Pool: free block", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 100);

    auto* ptr = pool.alloc_block();
    REQUIRE(pool.allocated_count() == 1);

    pool.free_block(ptr);
    REQUIRE(pool.allocated_count() == 0);
    REQUIRE(pool.free_count() == 100);
}

TEST_CASE("Pool: exhaustion returns nullptr", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 2);

    auto* ptr1 = pool.alloc_block();
    auto* ptr2 = pool.alloc_block();
    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);

    auto* ptr3 = pool.alloc_block();
    REQUIRE(ptr3 == nullptr);
}

TEST_CASE("Pool: reuse freed blocks", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 2);

    auto* ptr1 = pool.alloc_block();
    auto* ptr2 = pool.alloc_block();
    REQUIRE(pool.alloc_block() == nullptr);

    pool.free_block(ptr1);
    auto* ptr3 = pool.alloc_block();
    REQUIRE(ptr3 != nullptr);
    REQUIRE(ptr3 == ptr1); // Should reuse the freed block
}

TEST_CASE("Pool: reset", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 100);

    pool.alloc_block();
    pool.alloc_block();
    pool.alloc_block();
    REQUIRE(pool.allocated_count() == 3);

    pool.reset();
    REQUIRE(pool.allocated_count() == 0);
    REQUIRE(pool.free_count() == 100);
}

TEST_CASE("Pool: stats", "[memory][pool]") {
    Pool pool(sizeof(double), alignof(double), 50);

    pool.alloc_block();
    pool.alloc_block();

    auto stats = pool.stats();
    REQUIRE(stats.total_blocks == 50);
    REQUIRE(stats.allocated_blocks == 2);
    REQUIRE(stats.free_blocks == 48);
    REQUIRE(stats.block_size >= sizeof(double));
}

TEST_CASE("Pool: allocate interface", "[memory][pool]") {
    Pool pool(64, 8, 100);

    // Should succeed - fits in block
    auto* ptr = pool.allocate(32, 8);
    REQUIRE(ptr != nullptr);

    // Should fail - too large
    auto* ptr2 = pool.allocate(128, 8);
    REQUIRE(ptr2 == nullptr);
}

TEST_CASE("Pool: capacity and used", "[memory][pool]") {
    Pool pool(sizeof(int), alignof(int), 100);

    REQUIRE(pool.capacity() == 100 * pool.block_size());
    REQUIRE(pool.used() == 0);

    pool.alloc_block();
    pool.alloc_block();
    REQUIRE(pool.used() == 2 * pool.block_size());
}

// =============================================================================
// TypedPool Tests
// =============================================================================

TEST_CASE("TypedPool: creation", "[memory][pool]") {
    TypedPool<int> pool(100);

    auto stats = pool.stats();
    REQUIRE(stats.total_blocks == 100);
    REQUIRE(stats.allocated_blocks == 0);
}

TEST_CASE("TypedPool: alloc and free", "[memory][pool]") {
    TypedPool<int> pool(100);

    auto* a = pool.alloc(42);
    auto* b = pool.alloc(100);

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(*a == 42);
    REQUIRE(*b == 100);
    REQUIRE(pool.stats().allocated_blocks == 2);

    pool.free(a);
    REQUIRE(pool.stats().allocated_blocks == 1);
}

TEST_CASE("TypedPool: destructor called on free", "[memory][pool]") {
    static int destructor_count = 0;

    struct Counter {
        ~Counter() { ++destructor_count; }
    };

    destructor_count = 0;

    {
        TypedPool<Counter> pool(10);
        auto* c1 = pool.alloc();
        auto* c2 = pool.alloc();

        pool.free(c1);
        REQUIRE(destructor_count == 1);

        pool.free(c2);
        REQUIRE(destructor_count == 2);
    }
}

TEST_CASE("TypedPool: with constructor arguments", "[memory][pool]") {
    struct Point {
        int x, y;
        Point(int x, int y) : x(x), y(y) {}
    };

    TypedPool<Point> pool(10);

    auto* p = pool.alloc(10, 20);
    REQUIRE(p != nullptr);
    REQUIRE(p->x == 10);
    REQUIRE(p->y == 20);

    pool.free(p);
}
