// void_structures BitSet tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/structures/structures.hpp>
#include <vector>

using namespace void_structures;

// =============================================================================
// BitSet Construction Tests
// =============================================================================

TEST_CASE("BitSet construction", "[structures][bitset]") {
    SECTION("default capacity") {
        BitSet bs;
        REQUIRE(bs.size() == 64);
        REQUIRE(bs.none());
    }

    SECTION("custom capacity") {
        BitSet bs(128);
        REQUIRE(bs.size() == 128);
        REQUIRE(bs.none());
    }

    SECTION("from initializer list") {
        BitSet bs({0, 5, 10}, 64);
        REQUIRE(bs.get(0));
        REQUIRE(bs.get(5));
        REQUIRE(bs.get(10));
        REQUIRE_FALSE(bs.get(1));
    }
}

// =============================================================================
// BitSet Bit Operations Tests
// =============================================================================

TEST_CASE("BitSet set and get", "[structures][bitset]") {
    BitSet bs(64);

    SECTION("set single bit") {
        bs.set(5);
        REQUIRE(bs.get(5));
        REQUIRE_FALSE(bs.get(4));
        REQUIRE_FALSE(bs.get(6));
    }

    SECTION("set multiple bits") {
        bs.set(0);
        bs.set(31);
        bs.set(63);
        REQUIRE(bs.get(0));
        REQUIRE(bs.get(31));
        REQUIRE(bs.get(63));
    }

    SECTION("set with value") {
        bs.set(5, true);
        REQUIRE(bs.get(5));

        bs.set(5, false);
        REQUIRE_FALSE(bs.get(5));
    }

    SECTION("operator[]") {
        bs.set(10);
        REQUIRE(bs[10]);
        REQUIRE_FALSE(bs[11]);
    }

    SECTION("test alias") {
        bs.set(7);
        REQUIRE(bs.test(7));
    }
}

TEST_CASE("BitSet clear", "[structures][bitset]") {
    BitSet bs(64);
    bs.set(5);
    bs.set(10);

    bs.clear(5);

    REQUIRE_FALSE(bs.get(5));
    REQUIRE(bs.get(10));
}

TEST_CASE("BitSet toggle", "[structures][bitset]") {
    BitSet bs(64);

    bs.toggle(5);
    REQUIRE(bs.get(5));

    bs.toggle(5);
    REQUIRE_FALSE(bs.get(5));
}

// =============================================================================
// BitSet Bulk Operations Tests
// =============================================================================

TEST_CASE("BitSet set_all", "[structures][bitset]") {
    BitSet bs(100);

    bs.set_all();

    REQUIRE(bs.all());
    REQUIRE(bs.get(0));
    REQUIRE(bs.get(50));
    REQUIRE(bs.get(99));
}

TEST_CASE("BitSet clear_all", "[structures][bitset]") {
    BitSet bs(64);
    bs.set(0);
    bs.set(31);
    bs.set(63);

    bs.clear_all();

    REQUIRE(bs.none());
    REQUIRE_FALSE(bs.get(0));
    REQUIRE_FALSE(bs.get(31));
    REQUIRE_FALSE(bs.get(63));
}

// =============================================================================
// BitSet Aggregation Tests
// =============================================================================

TEST_CASE("BitSet count_ones", "[structures][bitset]") {
    BitSet bs(64);

    REQUIRE(bs.count_ones() == 0);

    bs.set(0);
    REQUIRE(bs.count_ones() == 1);

    bs.set(10);
    bs.set(20);
    bs.set(30);
    REQUIRE(bs.count_ones() == 4);
}

TEST_CASE("BitSet count_zeros", "[structures][bitset]") {
    BitSet bs(64);

    REQUIRE(bs.count_zeros() == 64);

    bs.set(0);
    REQUIRE(bs.count_zeros() == 63);
}

TEST_CASE("BitSet any", "[structures][bitset]") {
    BitSet bs(64);

    REQUIRE_FALSE(bs.any());

    bs.set(42);
    REQUIRE(bs.any());
}

TEST_CASE("BitSet all", "[structures][bitset]") {
    BitSet bs(8);

    REQUIRE_FALSE(bs.all());

    for (std::size_t i = 0; i < 8; ++i) {
        bs.set(i);
    }
    REQUIRE(bs.all());
}

TEST_CASE("BitSet none", "[structures][bitset]") {
    BitSet bs(64);

    REQUIRE(bs.none());

    bs.set(0);
    REQUIRE_FALSE(bs.none());
}

// =============================================================================
// BitSet Resize Tests
// =============================================================================

TEST_CASE("BitSet resize", "[structures][bitset]") {
    BitSet bs(32);
    bs.set(0);
    bs.set(31);

    SECTION("grow") {
        bs.resize(128);
        REQUIRE(bs.size() == 128);
        REQUIRE(bs.get(0));
        REQUIRE(bs.get(31));
    }

    SECTION("shrink clears out-of-bounds bits") {
        bs.resize(16);
        REQUIRE(bs.size() == 16);
        REQUIRE(bs.get(0));
        // Bit 31 is now out of bounds, accessing it returns false
    }
}

// =============================================================================
// BitSet Bitwise Operations Tests
// =============================================================================

TEST_CASE("BitSet AND", "[structures][bitset]") {
    BitSet a({0, 1, 2, 3}, 64);
    BitSet b({2, 3, 4, 5}, 64);

    BitSet result = a & b;

    REQUIRE(result.get(2));
    REQUIRE(result.get(3));
    REQUIRE_FALSE(result.get(0));
    REQUIRE_FALSE(result.get(1));
    REQUIRE_FALSE(result.get(4));
    REQUIRE_FALSE(result.get(5));
}

TEST_CASE("BitSet OR", "[structures][bitset]") {
    BitSet a({0, 1}, 64);
    BitSet b({2, 3}, 64);

    BitSet result = a | b;

    REQUIRE(result.get(0));
    REQUIRE(result.get(1));
    REQUIRE(result.get(2));
    REQUIRE(result.get(3));
}

TEST_CASE("BitSet XOR", "[structures][bitset]") {
    BitSet a({0, 1, 2}, 64);
    BitSet b({1, 2, 3}, 64);

    BitSet result = a ^ b;

    REQUIRE(result.get(0));    // Only in a
    REQUIRE_FALSE(result.get(1));  // In both
    REQUIRE_FALSE(result.get(2));  // In both
    REQUIRE(result.get(3));    // Only in b
}

TEST_CASE("BitSet NOT", "[structures][bitset]") {
    BitSet a({0, 1}, 8);

    BitSet result = ~a;

    REQUIRE_FALSE(result.get(0));
    REQUIRE_FALSE(result.get(1));
    REQUIRE(result.get(2));
    REQUIRE(result.get(3));
    REQUIRE(result.get(7));
}

TEST_CASE("BitSet compound assignment", "[structures][bitset]") {
    SECTION("&=") {
        BitSet a({0, 1, 2}, 64);
        BitSet b({1, 2, 3}, 64);
        a &= b;
        REQUIRE_FALSE(a.get(0));
        REQUIRE(a.get(1));
        REQUIRE(a.get(2));
        REQUIRE_FALSE(a.get(3));
    }

    SECTION("|=") {
        BitSet a({0, 1}, 64);
        BitSet b({2, 3}, 64);
        a |= b;
        REQUIRE(a.count_ones() == 4);
    }

    SECTION("^=") {
        BitSet a({0, 1, 2}, 64);
        BitSet b({1, 2, 3}, 64);
        a ^= b;
        REQUIRE(a.get(0));
        REQUIRE(a.get(3));
        REQUIRE(a.count_ones() == 2);
    }
}

// =============================================================================
// BitSet Iteration Tests
// =============================================================================

TEST_CASE("BitSet iter_ones", "[structures][bitset]") {
    BitSet bs({5, 10, 15, 20}, 64);

    std::vector<std::size_t> indices;
    for (auto idx : bs.iter_ones()) {
        indices.push_back(idx);
    }

    REQUIRE(indices.size() == 4);
    REQUIRE(indices[0] == 5);
    REQUIRE(indices[1] == 10);
    REQUIRE(indices[2] == 15);
    REQUIRE(indices[3] == 20);
}

// =============================================================================
// BitSet Comparison Tests
// =============================================================================

TEST_CASE("BitSet comparison", "[structures][bitset]") {
    BitSet a({0, 5, 10}, 64);
    BitSet b({0, 5, 10}, 64);
    BitSet c({0, 5}, 64);
    BitSet d({0, 5, 10}, 128);

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);  // Different size
    REQUIRE(a != c);
}

// =============================================================================
// BitSet Direct Access Tests
// =============================================================================

TEST_CASE("BitSet as_words", "[structures][bitset]") {
    BitSet bs(128);
    bs.set(0);
    bs.set(64);

    const auto& words = bs.as_words();

    REQUIRE(words.size() == 2);
    REQUIRE((words[0] & 1) == 1);  // Bit 0 in word 0
    REQUIRE((words[1] & 1) == 1);  // Bit 0 in word 1 (bit 64 overall)
}

TEST_CASE("BitSet word_count", "[structures][bitset]") {
    BitSet bs64(64);
    BitSet bs65(65);
    BitSet bs128(128);

    REQUIRE(bs64.word_count() == 1);
    REQUIRE(bs65.word_count() == 2);
    REQUIRE(bs128.word_count() == 2);
}
