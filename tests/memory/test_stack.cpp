/// @file test_stack.cpp
/// @brief Tests for StackAllocator

#include <catch2/catch_test_macros.hpp>
#include <void_engine/memory/stack.hpp>

using namespace void_memory;

TEST_CASE("StackAllocator: creation", "[memory][stack]") {
    StackAllocator stack(1024);
    REQUIRE(stack.capacity() == 1024);
    REQUIRE(stack.used() == 0);
}

TEST_CASE("StackAllocator: with_capacity_kb", "[memory][stack]") {
    auto stack = StackAllocator::with_capacity_kb(4);
    REQUIRE(stack.capacity() == 4 * 1024);
}

TEST_CASE("StackAllocator: basic allocation", "[memory][stack]") {
    StackAllocator stack(1024);

    auto* a = stack.alloc(42);
    auto* b = stack.alloc(3.14f);

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(*a == 42);
    REQUIRE(*b == 3.14f);
    REQUIRE(stack.used() > 0);
}

TEST_CASE("StackAllocator: allocate_typed", "[memory][stack]") {
    StackAllocator stack(1024);

    auto* ints = stack.allocate_typed<int>(10);
    REQUIRE(ints != nullptr);
    REQUIRE(is_aligned(ints, alignof(int)));
}

TEST_CASE("StackAllocator: reset", "[memory][stack]") {
    StackAllocator stack(1024);

    stack.alloc(42);
    stack.alloc(100);
    REQUIRE(stack.used() > 0);

    stack.reset();
    REQUIRE(stack.used() == 0);
}

TEST_CASE("StackAllocator: out of memory", "[memory][stack]") {
    StackAllocator stack(64);

    // Fill the stack
    while (stack.allocate(16, 1) != nullptr) {}

    // Should fail
    auto* ptr = stack.allocate(1, 1);
    REQUIRE(ptr == nullptr);
}

TEST_CASE("StackAllocator: marker and rollback", "[memory][stack]") {
    StackAllocator stack(1024);

    stack.alloc(1);
    auto marker = stack.marker();
    std::size_t saved_pos = stack.current_position();

    stack.alloc(2);
    stack.alloc(3);
    REQUIRE(stack.current_position() > saved_pos);

    stack.rollback(marker);
    REQUIRE(stack.current_position() == saved_pos);
}

TEST_CASE("StackScope: automatic rollback", "[memory][stack]") {
    StackAllocator stack(1024);

    std::size_t initial = stack.used();

    {
        StackScope scope(stack);
        stack.alloc(42);
        stack.alloc(100);
        REQUIRE(stack.used() > initial);
    }

    // Memory reclaimed after scope
    REQUIRE(stack.used() == initial);
}

TEST_CASE("StackAllocator: nested scopes", "[memory][stack]") {
    StackAllocator stack(1024);

    std::size_t pos0 = stack.current_position();

    {
        StackScope scope1(stack);
        stack.alloc(1);
        std::size_t pos1 = stack.current_position();

        {
            StackScope scope2(stack);
            stack.alloc(2);
            stack.alloc(3);
            REQUIRE(stack.current_position() > pos1);
        }

        REQUIRE(stack.current_position() == pos1);
    }

    REQUIRE(stack.current_position() == pos0);
}

TEST_CASE("StackAllocator: alignment", "[memory][stack]") {
    StackAllocator stack(1024);

    auto* a1 = stack.allocate(1, 1);
    auto* a2 = stack.allocate(1, 2);
    auto* a4 = stack.allocate(1, 4);
    auto* a8 = stack.allocate(1, 8);
    auto* a16 = stack.allocate(1, 16);

    REQUIRE(is_aligned(a1, 1));
    REQUIRE(is_aligned(a2, 2));
    REQUIRE(is_aligned(a4, 4));
    REQUIRE(is_aligned(a8, 8));
    REQUIRE(is_aligned(a16, 16));
}

TEST_CASE("StackAllocator: LIFO deallocation", "[memory][stack]") {
    StackAllocator stack(1024);

    auto* ptr1 = stack.allocate(32, 8);
    std::size_t pos1 = stack.current_position();

    auto* ptr2 = stack.allocate(32, 8);
    std::size_t pos2 = stack.current_position();

    // Deallocate in LIFO order
    stack.deallocate(ptr2, 32, 8);
    REQUIRE(stack.current_position() == pos1);

    stack.deallocate(ptr1, 32, 8);
    REQUIRE(stack.current_position() == 0);
}

TEST_CASE("StackAllocator: save and restore aliases", "[memory][stack]") {
    StackAllocator stack(1024);

    stack.alloc(1);
    auto state = stack.save();
    std::size_t saved_pos = stack.current_position();

    stack.alloc(2);
    REQUIRE(stack.current_position() > saved_pos);

    stack.restore(state);
    REQUIRE(stack.current_position() == saved_pos);
}

TEST_CASE("StackAllocator: capacity and used", "[memory][stack]") {
    StackAllocator stack(1024);

    REQUIRE(stack.capacity() == 1024);
    REQUIRE(stack.used() == 0);
    REQUIRE(stack.available() == 1024);

    stack.alloc(42);
    REQUIRE(stack.used() > 0);
    REQUIRE(stack.available() < 1024);
}
