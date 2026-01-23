// void_structures SlotMap tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/structures/structures.hpp>
#include <string>
#include <unordered_set>
#include <limits>

using namespace void_structures;

// =============================================================================
// SlotKey Tests
// =============================================================================

TEST_CASE("SlotKey construction", "[structures][slotkey]") {
    SECTION("default is null") {
        SlotKey<int> key;
        REQUIRE(key.is_null());
        REQUIRE(key.get_index() == std::numeric_limits<std::uint32_t>::max());
        REQUIRE(key.get_generation() == 0);
    }

    SECTION("null key factory") {
        SlotKey<int> key = SlotKey<int>::null();
        REQUIRE(key.is_null());
    }

    SECTION("from index and generation") {
        SlotKey<int> key(5, 3);
        REQUIRE(key.get_index() == 5);
        REQUIRE(key.get_generation() == 3);
        REQUIRE_FALSE(key.is_null());
    }
}

TEST_CASE("SlotKey comparison", "[structures][slotkey]") {
    SlotKey<int> a(1, 1);
    SlotKey<int> b(1, 1);
    SlotKey<int> c(2, 1);
    SlotKey<int> d(1, 2);

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
    REQUIRE(a != c);
}

TEST_CASE("SlotKey hashing", "[structures][slotkey]") {
    SlotKey<int> key1(1, 1);
    SlotKey<int> key2(1, 1);
    SlotKey<int> key3(2, 1);

    std::unordered_set<SlotKey<int>> set;
    set.insert(key1);

    REQUIRE(set.count(key2) == 1);  // Same as key1
    REQUIRE(set.count(key3) == 0);  // Different
}

// =============================================================================
// SlotMap Tests
// =============================================================================

TEST_CASE("SlotMap construction", "[structures][slotmap]") {
    SECTION("default empty") {
        SlotMap<int> map;
        REQUIRE(map.empty());
        REQUIRE(map.size() == 0);
    }

    SECTION("with capacity") {
        SlotMap<std::string> map(100);
        REQUIRE(map.empty());
    }
}

TEST_CASE("SlotMap insert and get", "[structures][slotmap]") {
    SlotMap<int> map;

    SECTION("single insert") {
        SlotKey<int> key = map.insert(42);
        REQUIRE_FALSE(key.is_null());
        REQUIRE(map.size() == 1);
        REQUIRE(map.contains_key(key));

        const int* value = map.get(key);
        REQUIRE(value != nullptr);
        REQUIRE(*value == 42);
    }

    SECTION("multiple inserts") {
        SlotKey<int> k1 = map.insert(1);
        SlotKey<int> k2 = map.insert(2);
        SlotKey<int> k3 = map.insert(3);

        REQUIRE(map.size() == 3);
        REQUIRE(*map.get(k1) == 1);
        REQUIRE(*map.get(k2) == 2);
        REQUIRE(*map.get(k3) == 3);
    }

    SECTION("mutable access") {
        SlotKey<int> key = map.insert(10);
        int* value = map.get(key);
        *value = 20;
        REQUIRE(*map.get(key) == 20);
    }
}

TEST_CASE("SlotMap emplace", "[structures][slotmap]") {
    SlotMap<std::string> map;

    SlotKey<std::string> key = map.emplace("hello world");
    REQUIRE(map.contains_key(key));
    REQUIRE(*map.get(key) == "hello world");
}

TEST_CASE("SlotMap remove", "[structures][slotmap]") {
    SlotMap<int> map;

    SlotKey<int> k1 = map.insert(1);
    SlotKey<int> k2 = map.insert(2);
    SlotKey<int> k3 = map.insert(3);

    SECTION("remove returns value") {
        auto removed = map.remove(k2);
        REQUIRE(removed.has_value());
        REQUIRE(*removed == 2);
        REQUIRE(map.size() == 2);
        REQUIRE_FALSE(map.contains_key(k2));
    }

    SECTION("remove invalidates key") {
        map.remove(k1);
        REQUIRE(map.get(k1) == nullptr);
    }

    SECTION("remove non-existent returns nullopt") {
        SlotKey<int> fake(999, 999);
        auto removed = map.remove(fake);
        REQUIRE_FALSE(removed.has_value());
    }

    SECTION("erase returns bool") {
        REQUIRE(map.erase(k1));
        REQUIRE_FALSE(map.erase(k1));  // Already removed
    }
}

TEST_CASE("SlotMap generation tracking", "[structures][slotmap]") {
    SlotMap<int> map;

    SlotKey<int> k1 = map.insert(1);
    map.remove(k1);

    // Insert again - should reuse slot with new generation
    SlotKey<int> k2 = map.insert(2);

    SECTION("old key doesn't work") {
        REQUIRE_FALSE(map.contains_key(k1));
        REQUIRE(map.get(k1) == nullptr);
    }

    SECTION("new key works") {
        REQUIRE(map.contains_key(k2));
        REQUIRE(*map.get(k2) == 2);
    }

    SECTION("generations differ") {
        // k1 and k2 might use same slot, but different generations
        // So they're not equal even if same index
        if (k1.get_index() == k2.get_index()) {
            REQUIRE(k1.get_generation() != k2.get_generation());
        }
    }
}

TEST_CASE("SlotMap at throws", "[structures][slotmap]") {
    SlotMap<int> map;
    SlotKey<int> key = map.insert(42);

    REQUIRE(map.at(key) == 42);

    SlotKey<int> invalid(999, 999);
    REQUIRE_THROWS(map.at(invalid));
}

TEST_CASE("SlotMap clear", "[structures][slotmap]") {
    SlotMap<int> map;
    SlotKey<int> k1 = map.insert(1);
    SlotKey<int> k2 = map.insert(2);

    map.clear();

    REQUIRE(map.empty());
    REQUIRE(map.size() == 0);
    REQUIRE_FALSE(map.contains_key(k1));
    REQUIRE_FALSE(map.contains_key(k2));
}

TEST_CASE("SlotMap iteration", "[structures][slotmap]") {
    SlotMap<int> map;
    map.insert(1);
    map.insert(2);
    map.insert(3);

    SECTION("range-based for") {
        int sum = 0;
        for (auto [key, value] : map) {
            sum += value;
        }
        REQUIRE(sum == 6);
    }

    SECTION("keys iteration") {
        std::vector<SlotKey<int>> keys;
        for (auto key : map.keys()) {
            keys.push_back(key);
        }
        REQUIRE(keys.size() == 3);
    }

    SECTION("values iteration") {
        int sum = 0;
        for (int& v : map.values()) {
            sum += v;
        }
        REQUIRE(sum == 6);
    }

    SECTION("const values") {
        const SlotMap<int>& cmap = map;
        int sum = 0;
        for (const int& v : cmap.values()) {
            sum += v;
        }
        REQUIRE(sum == 6);
    }
}

TEST_CASE("SlotMap with move-only types", "[structures][slotmap]") {
    SlotMap<std::unique_ptr<int>> map;

    SlotKey<std::unique_ptr<int>> key = map.insert(std::make_unique<int>(42));
    REQUIRE(**map.get(key) == 42);

    auto removed = map.remove(key);
    REQUIRE(removed.has_value());
    REQUIRE(**removed == 42);
}
