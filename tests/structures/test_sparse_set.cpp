// void_structures SparseSet tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/structures/structures.hpp>
#include <string>
#include <numeric>
#include <algorithm>

using namespace void_structures;

// =============================================================================
// SparseSet Construction Tests
// =============================================================================

TEST_CASE("SparseSet construction", "[structures][sparseset]") {
    SECTION("default empty") {
        SparseSet<int> set;
        REQUIRE(set.empty());
        REQUIRE(set.size() == 0);
    }

    SECTION("with capacities") {
        SparseSet<int> set(100, 50);
        REQUIRE(set.empty());
    }
}

// =============================================================================
// SparseSet Insert and Lookup Tests
// =============================================================================

TEST_CASE("SparseSet insert and get", "[structures][sparseset]") {
    SparseSet<int> set;

    SECTION("single insert") {
        auto result = set.insert(5, 42);
        REQUIRE_FALSE(result.has_value());  // No old value
        REQUIRE(set.size() == 1);
        REQUIRE(set.contains(5));

        const int* value = set.get(5);
        REQUIRE(value != nullptr);
        REQUIRE(*value == 42);
    }

    SECTION("multiple inserts") {
        set.insert(0, 10);
        set.insert(5, 50);
        set.insert(10, 100);

        REQUIRE(set.size() == 3);
        REQUIRE(*set.get(0) == 10);
        REQUIRE(*set.get(5) == 50);
        REQUIRE(*set.get(10) == 100);
    }

    SECTION("sparse indices") {
        set.insert(1000, 42);
        REQUIRE(set.contains(1000));
        REQUIRE(*set.get(1000) == 42);
        REQUIRE(set.size() == 1);  // Only one element despite large index
    }

    SECTION("update existing") {
        set.insert(5, 42);
        auto old = set.insert(5, 100);
        REQUIRE(old.has_value());
        REQUIRE(*old == 42);
        REQUIRE(*set.get(5) == 100);
        REQUIRE(set.size() == 1);  // Still just one element
    }
}

TEST_CASE("SparseSet emplace", "[structures][sparseset]") {
    SparseSet<std::string> set;

    auto result = set.emplace(0, "hello");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(*set.get(0) == "hello");
}

TEST_CASE("SparseSet at", "[structures][sparseset]") {
    SparseSet<int> set;
    set.insert(5, 42);

    REQUIRE(set.at(5) == 42);
    REQUIRE_THROWS(set.at(10));
}

TEST_CASE("SparseSet mutable access", "[structures][sparseset]") {
    SparseSet<int> set;
    set.insert(5, 42);

    int* ptr = set.get(5);
    *ptr = 100;
    REQUIRE(*set.get(5) == 100);
}

// =============================================================================
// SparseSet Remove Tests
// =============================================================================

TEST_CASE("SparseSet remove", "[structures][sparseset]") {
    SparseSet<int> set;
    set.insert(0, 10);
    set.insert(5, 50);
    set.insert(10, 100);

    SECTION("remove returns value") {
        auto removed = set.remove(5);
        REQUIRE(removed.has_value());
        REQUIRE(*removed == 50);
        REQUIRE(set.size() == 2);
        REQUIRE_FALSE(set.contains(5));
    }

    SECTION("remove non-existent") {
        auto removed = set.remove(999);
        REQUIRE_FALSE(removed.has_value());
    }

    SECTION("erase returns bool") {
        REQUIRE(set.erase(5));
        REQUIRE_FALSE(set.erase(5));  // Already removed
    }

    SECTION("other elements unaffected") {
        set.remove(5);
        REQUIRE(*set.get(0) == 10);
        REQUIRE(*set.get(10) == 100);
    }
}

TEST_CASE("SparseSet clear", "[structures][sparseset]") {
    SparseSet<int> set;
    set.insert(0, 10);
    set.insert(5, 50);

    set.clear();

    REQUIRE(set.empty());
    REQUIRE(set.size() == 0);
    REQUIRE_FALSE(set.contains(0));
    REQUIRE_FALSE(set.contains(5));
}

// =============================================================================
// SparseSet Direct Access Tests
// =============================================================================

TEST_CASE("SparseSet direct access", "[structures][sparseset]") {
    SparseSet<int> set;
    set.insert(0, 10);
    set.insert(5, 50);
    set.insert(10, 100);

    SECTION("as_slice") {
        auto slice = set.as_slice();
        REQUIRE(slice.size() == 3);

        // Values are stored contiguously
        int sum = 0;
        for (int v : slice) {
            sum += v;
        }
        REQUIRE(sum == 160);
    }

    SECTION("as_mut_slice") {
        auto slice = set.as_mut_slice();
        for (int& v : slice) {
            v *= 2;
        }

        REQUIRE(*set.get(0) == 20);
        REQUIRE(*set.get(5) == 100);
        REQUIRE(*set.get(10) == 200);
    }

    SECTION("indices_slice") {
        auto indices = set.indices_slice();
        REQUIRE(indices.size() == 3);

        // All our sparse indices should be present
        bool has_0 = std::find(indices.begin(), indices.end(), 0) != indices.end();
        bool has_5 = std::find(indices.begin(), indices.end(), 5) != indices.end();
        bool has_10 = std::find(indices.begin(), indices.end(), 10) != indices.end();

        REQUIRE(has_0);
        REQUIRE(has_5);
        REQUIRE(has_10);
    }

    SECTION("data pointer") {
        const int* ptr = set.data();
        REQUIRE(ptr != nullptr);
    }

    SECTION("dense_index_of") {
        auto idx0 = set.dense_index_of(0);
        auto idx5 = set.dense_index_of(5);
        auto idx10 = set.dense_index_of(10);
        auto idx_missing = set.dense_index_of(999);

        REQUIRE(idx0.has_value());
        REQUIRE(idx5.has_value());
        REQUIRE(idx10.has_value());
        REQUIRE_FALSE(idx_missing.has_value());

        // Dense indices should be unique
        REQUIRE(*idx0 != *idx5);
        REQUIRE(*idx5 != *idx10);
    }
}

// =============================================================================
// SparseSet Iteration Tests
// =============================================================================

TEST_CASE("SparseSet iteration", "[structures][sparseset]") {
    SparseSet<int> set;
    set.insert(0, 10);
    set.insert(5, 50);
    set.insert(10, 100);

    SECTION("range-based for") {
        int sum = 0;
        std::size_t count = 0;
        for (auto [index, value] : set) {
            sum += value;
            ++count;
        }
        REQUIRE(count == 3);
        REQUIRE(sum == 160);
    }

    SECTION("values iteration") {
        int sum = 0;
        for (int v : set.values()) {
            sum += v;
        }
        REQUIRE(sum == 160);
    }

    SECTION("indices iteration") {
        std::size_t index_sum = 0;
        for (auto idx : set.indices()) {
            index_sum += idx;
        }
        REQUIRE(index_sum == 15);  // 0 + 5 + 10
    }
}

// =============================================================================
// SparseSet Sort Tests
// =============================================================================

TEST_CASE("SparseSet sort_by_index", "[structures][sparseset]") {
    SparseSet<int> set;

    // Insert in non-sequential order
    set.insert(10, 100);
    set.insert(0, 10);
    set.insert(5, 50);

    set.sort_by_index();

    // After sorting, indices should be in order
    auto indices = set.indices_slice();
    REQUIRE(indices.size() == 3);

    std::vector<std::size_t> idx_vec(indices.begin(), indices.end());
    REQUIRE(std::is_sorted(idx_vec.begin(), idx_vec.end()));

    // Values should still be correct
    REQUIRE(*set.get(0) == 10);
    REQUIRE(*set.get(5) == 50);
    REQUIRE(*set.get(10) == 100);
}

// =============================================================================
// SparseSet with Complex Types
// =============================================================================

TEST_CASE("SparseSet with complex types", "[structures][sparseset]") {
    struct Component {
        float x, y, z;
        std::string name;
    };

    SparseSet<Component> set;

    set.insert(0, Component{1.0f, 2.0f, 3.0f, "entity0"});
    set.insert(100, Component{4.0f, 5.0f, 6.0f, "entity100"});

    REQUIRE(set.get(0)->name == "entity0");
    REQUIRE(set.get(100)->x == 4.0f);
}

TEST_CASE("SparseSet with move-only types", "[structures][sparseset]") {
    SparseSet<std::unique_ptr<int>> set;

    set.insert(0, std::make_unique<int>(42));
    REQUIRE(**set.get(0) == 42);

    auto removed = set.remove(0);
    REQUIRE(removed.has_value());
    REQUIRE(**removed == 42);
}
