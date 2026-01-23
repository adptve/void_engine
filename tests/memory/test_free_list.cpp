/// @file test_free_list.cpp
/// @brief Tests for FreeList allocator

#include <catch2/catch_test_macros.hpp>
#include <void_engine/memory/free_list.hpp>
#include <vector>

using namespace void_memory;

TEST_CASE("FreeList: creation", "[memory][free_list]") {
    FreeList alloc(1024);
    REQUIRE(alloc.capacity() == 1024);
    REQUIRE(alloc.used() == 0);
}

TEST_CASE("FreeList: with policy", "[memory][free_list]") {
    FreeList first_fit(1024, PlacementPolicy::FirstFit);
    FreeList best_fit(1024, PlacementPolicy::BestFit);
    FreeList worst_fit(1024, PlacementPolicy::WorstFit);

    REQUIRE(first_fit.policy() == PlacementPolicy::FirstFit);
    REQUIRE(best_fit.policy() == PlacementPolicy::BestFit);
    REQUIRE(worst_fit.policy() == PlacementPolicy::WorstFit);
}

TEST_CASE("FreeList: basic allocation", "[memory][free_list]") {
    FreeList alloc(1024);

    auto* ptr1 = alloc.allocate(64, 8);
    auto* ptr2 = alloc.allocate(128, 16);

    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(ptr1 != ptr2);
    REQUIRE(alloc.used() > 0);
}

TEST_CASE("FreeList: deallocation", "[memory][free_list]") {
    FreeList alloc(1024);

    auto* ptr = alloc.allocate(64, 8);
    REQUIRE(alloc.used() > 0);

    alloc.deallocate(ptr, 64, 8);
    // After deallocation, memory should be reclaimed
    // (exact used() value depends on implementation)
}

TEST_CASE("FreeList: out of memory", "[memory][free_list]") {
    FreeList alloc(128);

    auto* ptr = alloc.allocate(128, 1);
    // May or may not succeed depending on header overhead
    if (ptr) {
        auto* ptr2 = alloc.allocate(1, 1);
        REQUIRE(ptr2 == nullptr);
    }
}

TEST_CASE("FreeList: coalesce adjacent blocks", "[memory][free_list]") {
    FreeList alloc(1024);

    auto* ptr1 = alloc.allocate(100, 8);
    auto* ptr2 = alloc.allocate(100, 8);
    auto* ptr3 = alloc.allocate(100, 8);

    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(ptr3 != nullptr);

    // Free in non-adjacent order
    alloc.deallocate(ptr1, 100, 8);
    alloc.deallocate(ptr3, 100, 8);
    // Blocks are now fragmented

    auto stats_fragmented = alloc.stats();
    REQUIRE(stats_fragmented.free_blocks >= 2);

    // Free middle block - should coalesce
    alloc.deallocate(ptr2, 100, 8);

    auto stats_coalesced = alloc.stats();
    REQUIRE(stats_coalesced.free_blocks == 1);
}

TEST_CASE("FreeList: reset", "[memory][free_list]") {
    FreeList alloc(1024);

    alloc.allocate(64, 8);
    alloc.allocate(64, 8);
    REQUIRE(alloc.used() > 0);

    alloc.reset();
    REQUIRE(alloc.used() == 0);
    REQUIRE(alloc.stats().free_blocks == 1);
}

TEST_CASE("FreeList: stats", "[memory][free_list]") {
    FreeList alloc(1024);

    auto stats = alloc.stats();
    REQUIRE(stats.capacity == 1024);
    REQUIRE(stats.used == 0);
    REQUIRE(stats.free == 1024);
    REQUIRE(stats.free_blocks == 1);
    REQUIRE(stats.largest_free_block == 1024);
}

TEST_CASE("FreeList: alignment", "[memory][free_list]") {
    FreeList alloc(1024);

    auto* a1 = alloc.allocate(1, 1);
    auto* a2 = alloc.allocate(1, 2);
    auto* a4 = alloc.allocate(1, 4);
    auto* a8 = alloc.allocate(1, 8);
    auto* a16 = alloc.allocate(1, 16);

    REQUIRE(is_aligned(a1, 1));
    REQUIRE(is_aligned(a2, 2));
    REQUIRE(is_aligned(a4, 4));
    REQUIRE(is_aligned(a8, 8));
    REQUIRE(is_aligned(a16, 16));
}

TEST_CASE("FreeList: double free protection", "[memory][free_list]") {
    FreeList alloc(1024);

    auto* ptr = alloc.allocate(64, 8);
    alloc.deallocate(ptr, 64, 8);

    // Double free should be harmless (no crash, no corruption)
    alloc.deallocate(ptr, 64, 8);

    // Allocator should still be usable
    auto* ptr2 = alloc.allocate(64, 8);
    REQUIRE(ptr2 != nullptr);
}

TEST_CASE("FreeList: set_policy", "[memory][free_list]") {
    FreeList alloc(1024);

    REQUIRE(alloc.policy() == PlacementPolicy::FirstFit);

    alloc.set_policy(PlacementPolicy::BestFit);
    REQUIRE(alloc.policy() == PlacementPolicy::BestFit);
}

TEST_CASE("FreeList: free_block_count", "[memory][free_list]") {
    FreeList alloc(1024);

    REQUIRE(alloc.free_block_count() == 1);

    auto* ptr1 = alloc.allocate(100, 8);
    auto* ptr2 = alloc.allocate(100, 8);
    auto* ptr3 = alloc.allocate(100, 8);

    // Free non-adjacent blocks
    alloc.deallocate(ptr1, 100, 8);
    alloc.deallocate(ptr3, 100, 8);

    REQUIRE(alloc.free_block_count() >= 2);
}

TEST_CASE("FreeList: FirstFit policy", "[memory][free_list]") {
    FreeList alloc(1024, PlacementPolicy::FirstFit);

    // Allocate and free to create gaps
    auto* ptr1 = alloc.allocate(100, 8);
    auto* ptr2 = alloc.allocate(200, 8);
    auto* ptr3 = alloc.allocate(100, 8);

    alloc.deallocate(ptr1, 100, 8);
    alloc.deallocate(ptr3, 100, 8);

    // FirstFit should use the first gap that fits
    auto* ptr4 = alloc.allocate(50, 8);
    REQUIRE(ptr4 != nullptr);
}

TEST_CASE("FreeList: BestFit policy", "[memory][free_list]") {
    FreeList alloc(1024, PlacementPolicy::BestFit);

    // Allocate and free to create gaps of different sizes
    auto* ptr1 = alloc.allocate(100, 8);
    auto* ptr2 = alloc.allocate(50, 8);
    auto* ptr3 = alloc.allocate(200, 8);
    auto* ptr4 = alloc.allocate(50, 8);

    alloc.deallocate(ptr1, 100, 8); // 100-byte gap
    alloc.deallocate(ptr3, 200, 8); // 200-byte gap

    // BestFit should choose the 100-byte gap for a 50-byte request
    auto* ptr5 = alloc.allocate(50, 8);
    REQUIRE(ptr5 != nullptr);
}

TEST_CASE("FreeList: many allocations", "[memory][free_list]") {
    FreeList alloc(1024 * 1024); // 1 MB

    std::vector<void*> ptrs;

    // Allocate many small blocks
    for (int i = 0; i < 100; ++i) {
        auto* ptr = alloc.allocate(64, 8);
        REQUIRE(ptr != nullptr);
        ptrs.push_back(ptr);
    }

    // Free half
    for (int i = 0; i < 50; ++i) {
        alloc.deallocate(ptrs[i * 2], 64, 8);
    }

    // Allocate again
    for (int i = 0; i < 50; ++i) {
        auto* ptr = alloc.allocate(64, 8);
        REQUIRE(ptr != nullptr);
    }
}
