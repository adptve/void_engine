// void_core Handle, HandleAllocator, HandleMap tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/core/handle.hpp>
#include <algorithm>
#include <unordered_set>
#include <string>

using namespace void_core;

// Tag types for type-safe handles
struct TestEntity {};
struct OtherEntity {};

// =============================================================================
// Handle Tests
// =============================================================================

TEST_CASE("Handle construction", "[core][handle]") {
    SECTION("default is null") {
        Handle<TestEntity> h;
        REQUIRE(h.is_null());
        REQUIRE_FALSE(h.is_valid());
        REQUIRE_FALSE(static_cast<bool>(h));
    }

    SECTION("null factory") {
        Handle<TestEntity> h = Handle<TestEntity>::null();
        REQUIRE(h.is_null());
    }

    SECTION("create with index and generation") {
        Handle<TestEntity> h = Handle<TestEntity>::create(42, 7);
        REQUIRE(h.index() == 42);
        REQUIRE(h.generation() == 7);
        REQUIRE(h.is_valid());
    }
}

TEST_CASE("Handle bit layout", "[core][handle]") {
    // Layout: [Generation(8 bits) | Index(24 bits)]
    Handle<TestEntity> h = Handle<TestEntity>::create(0xABCDEF, 0x12);

    REQUIRE(h.index() == 0xABCDEF);
    REQUIRE(h.generation() == 0x12);

    std::uint32_t bits = h.to_bits();
    Handle<TestEntity> decoded = Handle<TestEntity>::from_bits(bits);

    REQUIRE(decoded.index() == h.index());
    REQUIRE(decoded.generation() == h.generation());
    REQUIRE(decoded == h);
}

TEST_CASE("Handle max values", "[core][handle]") {
    // Max index is 24 bits
    Handle<TestEntity> h1 = Handle<TestEntity>::create(handle_constants::MAX_INDEX, 0);
    REQUIRE(h1.index() == handle_constants::MAX_INDEX);

    // Max generation is 8 bits
    Handle<TestEntity> h2 = Handle<TestEntity>::create(0, handle_constants::MAX_GENERATION);
    REQUIRE(h2.generation() == handle_constants::MAX_GENERATION);
}

TEST_CASE("Handle comparison", "[core][handle]") {
    Handle<TestEntity> a = Handle<TestEntity>::create(1, 1);
    Handle<TestEntity> b = Handle<TestEntity>::create(1, 1);
    Handle<TestEntity> c = Handle<TestEntity>::create(2, 1);
    Handle<TestEntity> d = Handle<TestEntity>::create(1, 2);

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
    REQUIRE(a != c);
}

TEST_CASE("Handle cast", "[core][handle]") {
    Handle<TestEntity> original = Handle<TestEntity>::create(42, 7);
    Handle<OtherEntity> casted = original.cast<OtherEntity>();

    REQUIRE(casted.index() == original.index());
    REQUIRE(casted.generation() == original.generation());
}

TEST_CASE("Handle hashing", "[core][handle]") {
    Handle<TestEntity> h1 = Handle<TestEntity>::create(1, 1);
    Handle<TestEntity> h2 = Handle<TestEntity>::create(1, 1);
    Handle<TestEntity> h3 = Handle<TestEntity>::create(2, 1);

    std::unordered_set<Handle<TestEntity>> set;
    set.insert(h1);

    REQUIRE(set.count(h2) == 1);
    REQUIRE(set.count(h3) == 0);
}

// =============================================================================
// HandleAllocator Tests
// =============================================================================

TEST_CASE("HandleAllocator construction", "[core][handle]") {
    SECTION("default empty") {
        HandleAllocator<TestEntity> alloc;
        REQUIRE(alloc.is_empty());
        REQUIRE(alloc.len() == 0);
        REQUIRE(alloc.capacity() == 0);
    }

    SECTION("with reserved capacity") {
        HandleAllocator<TestEntity> alloc(100);
        REQUIRE(alloc.is_empty());
        REQUIRE(alloc.len() == 0);
    }
}

TEST_CASE("HandleAllocator allocate", "[core][handle]") {
    HandleAllocator<TestEntity> alloc;

    SECTION("single allocation") {
        Handle<TestEntity> h = alloc.allocate();
        REQUIRE(h.is_valid());
        REQUIRE(h.index() == 0);
        REQUIRE(h.generation() == 0);
        REQUIRE(alloc.len() == 1);
        REQUIRE(alloc.is_valid(h));
    }

    SECTION("multiple allocations") {
        Handle<TestEntity> h1 = alloc.allocate();
        Handle<TestEntity> h2 = alloc.allocate();
        Handle<TestEntity> h3 = alloc.allocate();

        REQUIRE(h1.index() == 0);
        REQUIRE(h2.index() == 1);
        REQUIRE(h3.index() == 2);
        REQUIRE(alloc.len() == 3);
    }
}

TEST_CASE("HandleAllocator free", "[core][handle]") {
    HandleAllocator<TestEntity> alloc;

    Handle<TestEntity> h = alloc.allocate();
    REQUIRE(alloc.is_valid(h));

    bool result = alloc.free(h);
    REQUIRE(result);
    REQUIRE_FALSE(alloc.is_valid(h));
    REQUIRE(alloc.len() == 0);

    // Freeing again should fail
    REQUIRE_FALSE(alloc.free(h));
}

TEST_CASE("HandleAllocator generation tracking", "[core][handle]") {
    HandleAllocator<TestEntity> alloc;

    Handle<TestEntity> h1 = alloc.allocate();
    alloc.free(h1);

    // Allocate again - should reuse index with incremented generation
    Handle<TestEntity> h2 = alloc.allocate();

    REQUIRE(h2.index() == h1.index());
    REQUIRE(h2.generation() == h1.generation() + 1);

    // Old handle should be invalid
    REQUIRE_FALSE(alloc.is_valid(h1));
    REQUIRE(alloc.is_valid(h2));
}

TEST_CASE("HandleAllocator free list reuse", "[core][handle]") {
    HandleAllocator<TestEntity> alloc;

    Handle<TestEntity> h1 = alloc.allocate();
    Handle<TestEntity> h2 = alloc.allocate();
    Handle<TestEntity> h3 = alloc.allocate();

    // Free in LIFO order
    alloc.free(h2);
    alloc.free(h3);

    // Next allocations reuse freed slots (LIFO)
    Handle<TestEntity> h4 = alloc.allocate();
    REQUIRE(h4.index() == h3.index());

    Handle<TestEntity> h5 = alloc.allocate();
    REQUIRE(h5.index() == h2.index());
}

TEST_CASE("HandleAllocator clear", "[core][handle]") {
    HandleAllocator<TestEntity> alloc;

    Handle<TestEntity> h1 = alloc.allocate();
    Handle<TestEntity> h2 = alloc.allocate();

    alloc.clear();

    REQUIRE(alloc.is_empty());
    REQUIRE(alloc.capacity() == 0);
    REQUIRE_FALSE(alloc.is_valid(h1));
    REQUIRE_FALSE(alloc.is_valid(h2));
}

TEST_CASE("HandleAllocator is_valid edge cases", "[core][handle]") {
    HandleAllocator<TestEntity> alloc;

    // Null handle
    REQUIRE_FALSE(alloc.is_valid(Handle<TestEntity>::null()));

    // Out of range index
    REQUIRE_FALSE(alloc.is_valid(Handle<TestEntity>::create(1000, 0)));

    // Valid allocation
    Handle<TestEntity> h = alloc.allocate();
    REQUIRE(alloc.is_valid(h));

    // Wrong generation
    Handle<TestEntity> wrong_gen = Handle<TestEntity>::create(h.index(), h.generation() + 1);
    REQUIRE_FALSE(alloc.is_valid(wrong_gen));
}

// =============================================================================
// HandleMap Tests
// =============================================================================

TEST_CASE("HandleMap construction", "[core][handle]") {
    SECTION("default empty") {
        HandleMap<std::string> map;
        REQUIRE(map.is_empty());
        REQUIRE(map.len() == 0);
    }

    SECTION("with reserved capacity") {
        HandleMap<std::string> map(100);
        REQUIRE(map.is_empty());
    }
}

TEST_CASE("HandleMap insert", "[core][handle]") {
    HandleMap<std::string> map;

    Handle<std::string> h = map.insert("hello");
    REQUIRE(h.is_valid());
    REQUIRE(map.len() == 1);
    REQUIRE(map.contains(h));

    const std::string* ptr = map.get(h);
    REQUIRE(ptr != nullptr);
    REQUIRE(*ptr == "hello");
}

TEST_CASE("HandleMap get", "[core][handle]") {
    HandleMap<int> map;

    Handle<int> h = map.insert(42);

    SECTION("const get") {
        const HandleMap<int>& cmap = map;
        const int* ptr = cmap.get(h);
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 42);
    }

    SECTION("mutable get") {
        int* ptr = map.get_mut(h);
        REQUIRE(ptr != nullptr);
        *ptr = 100;
        REQUIRE(*map.get(h) == 100);
    }

    SECTION("invalid handle returns nullptr") {
        Handle<int> invalid = Handle<int>::null();
        REQUIRE(map.get(invalid) == nullptr);
        REQUIRE(map.get_mut(invalid) == nullptr);
    }
}

TEST_CASE("HandleMap remove", "[core][handle]") {
    HandleMap<std::string> map;

    Handle<std::string> h = map.insert("test");
    REQUIRE(map.contains(h));

    auto removed = map.remove(h);
    REQUIRE(removed.has_value());
    REQUIRE(*removed == "test");
    REQUIRE_FALSE(map.contains(h));
    REQUIRE(map.len() == 0);

    // Remove again should fail
    REQUIRE_FALSE(map.remove(h).has_value());
}

TEST_CASE("HandleMap generational safety", "[core][handle]") {
    HandleMap<int> map;

    Handle<int> h1 = map.insert(1);
    map.remove(h1);

    Handle<int> h2 = map.insert(2);

    // Old handle should not access new value
    REQUIRE_FALSE(map.contains(h1));
    REQUIRE(map.get(h1) == nullptr);

    // New handle works
    REQUIRE(map.contains(h2));
    REQUIRE(*map.get(h2) == 2);
}

TEST_CASE("HandleMap for_each", "[core][handle]") {
    HandleMap<int> map;

    Handle<int> h1 = map.insert(1);
    Handle<int> h2 = map.insert(2);
    Handle<int> h3 = map.insert(3);

    std::vector<int> values;
    map.for_each([&values](Handle<int>, const int& val) {
        values.push_back(val);
    });

    REQUIRE(values.size() == 3);
    // Order may vary, but all values should be present
    std::sort(values.begin(), values.end());
    REQUIRE(values == std::vector<int>{1, 2, 3});

    (void)h1; (void)h2; (void)h3;  // Suppress unused warnings
}

TEST_CASE("HandleMap for_each_mut", "[core][handle]") {
    HandleMap<int> map;

    map.insert(1);
    map.insert(2);
    map.insert(3);

    map.for_each_mut([](Handle<int>, int& val) {
        val *= 10;
    });

    int sum = 0;
    map.for_each([&sum](Handle<int>, const int& val) {
        sum += val;
    });

    REQUIRE(sum == 60);  // 10 + 20 + 30
}

TEST_CASE("HandleMap get_result", "[core][handle]") {
    HandleMap<int> map;

    Handle<int> h = map.insert(42);

    SECTION("success") {
        auto result = map.get_result(h);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().get() == 42);
    }

    SECTION("null handle") {
        auto result = map.get_result(Handle<int>::null());
        REQUIRE(result.is_err());
        REQUIRE(result.error().code() == ErrorCode::InvalidArgument);
    }

    SECTION("stale handle") {
        map.remove(h);
        Handle<int> h2 = map.insert(100);
        (void)h2;

        auto result = map.get_result(h);
        REQUIRE(result.is_err());
        REQUIRE(result.error().code() == ErrorCode::InvalidState);
    }
}

TEST_CASE("HandleMap clear", "[core][handle]") {
    HandleMap<int> map;

    Handle<int> h1 = map.insert(1);
    Handle<int> h2 = map.insert(2);

    map.clear();

    REQUIRE(map.is_empty());
    REQUIRE_FALSE(map.contains(h1));
    REQUIRE_FALSE(map.contains(h2));
}

TEST_CASE("HandleMap with complex types", "[core][handle]") {
    struct Data {
        int x;
        std::string s;
        std::vector<int> v;
    };

    HandleMap<Data> map;

    Handle<Data> h = map.insert(Data{42, "hello", {1, 2, 3}});

    const Data* ptr = map.get(h);
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr->x == 42);
    REQUIRE(ptr->s == "hello");
    REQUIRE(ptr->v.size() == 3);
}
