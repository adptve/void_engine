// void_core Id, IdGenerator, NamedId tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/core/id.hpp>
#include <unordered_set>
#include <thread>
#include <vector>

using namespace void_core;

// =============================================================================
// Id Tests
// =============================================================================

TEST_CASE("Id construction", "[core][id]") {
    SECTION("default is null") {
        Id id;
        REQUIRE(id.is_null());
        REQUIRE_FALSE(id.is_valid());
        REQUIRE_FALSE(static_cast<bool>(id));
    }

    SECTION("null factory") {
        Id id = Id::null();
        REQUIRE(id.is_null());
    }

    SECTION("from raw bits") {
        Id id(0x12345678);
        REQUIRE(id.to_bits() == 0x12345678);
        REQUIRE(id.is_valid());
    }

    SECTION("create with index and generation") {
        Id id = Id::create(100, 5);
        REQUIRE(id.index() == 100);
        REQUIRE(id.generation() == 5);
        REQUIRE(id.is_valid());
    }
}

TEST_CASE("Id bit encoding", "[core][id]") {
    Id original = Id::create(0xABCD, 0x1234);

    REQUIRE(original.index() == 0xABCD);
    REQUIRE(original.generation() == 0x1234);

    std::uint64_t bits = original.to_bits();
    Id decoded = Id::from_bits(bits);

    REQUIRE(decoded.index() == original.index());
    REQUIRE(decoded.generation() == original.generation());
    REQUIRE(decoded == original);
}

TEST_CASE("Id from_name", "[core][id]") {
    Id id1 = Id::from_name("test_id");
    Id id2 = Id::from_name("test_id");
    Id id3 = Id::from_name("other_id");

    REQUIRE(id1 == id2);
    REQUIRE(id1 != id3);
    REQUIRE(id1.is_valid());
}

TEST_CASE("Id comparison", "[core][id]") {
    Id a = Id::create(1, 1);
    Id b = Id::create(1, 1);
    Id c = Id::create(2, 1);
    Id d = Id::create(1, 2);

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE_FALSE(a == d);
    REQUIRE(a != c);

    // Ordering by bits
    REQUIRE(a < c);
}

TEST_CASE("Id hashing", "[core][id]") {
    Id id1 = Id::create(1, 1);
    Id id2 = Id::create(1, 1);
    Id id3 = Id::create(2, 1);

    std::unordered_set<Id> set;
    set.insert(id1);

    REQUIRE(set.count(id2) == 1);
    REQUIRE(set.count(id3) == 0);
}

// =============================================================================
// IdGenerator Tests
// =============================================================================

TEST_CASE("IdGenerator construction", "[core][id]") {
    IdGenerator gen;
    REQUIRE(gen.current() == 0);
}

TEST_CASE("IdGenerator next", "[core][id]") {
    IdGenerator gen;

    Id id1 = gen.next();
    Id id2 = gen.next();
    Id id3 = gen.next();

    REQUIRE(id1.index() == 0);
    REQUIRE(id2.index() == 1);
    REQUIRE(id3.index() == 2);

    REQUIRE(gen.current() == 3);
}

TEST_CASE("IdGenerator next_batch", "[core][id]") {
    IdGenerator gen;

    Id start = gen.next_batch(10);
    REQUIRE(start.index() == 0);
    REQUIRE(gen.current() == 10);

    Id next = gen.next();
    REQUIRE(next.index() == 10);
}

TEST_CASE("IdGenerator reset", "[core][id]") {
    IdGenerator gen;

    gen.next();
    gen.next();
    REQUIRE(gen.current() == 2);

    gen.reset();
    REQUIRE(gen.current() == 0);

    Id id = gen.next();
    REQUIRE(id.index() == 0);
}

TEST_CASE("IdGenerator thread safety", "[core][id]") {
    IdGenerator gen;
    const int threads = 4;
    const int ids_per_thread = 100;

    std::vector<std::thread> workers;
    std::vector<std::vector<Id>> results(threads);

    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&gen, &results, t, ids_per_thread]() {
            for (int i = 0; i < ids_per_thread; ++i) {
                results[t].push_back(gen.next());
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    // All IDs should be unique
    std::unordered_set<Id> all_ids;
    for (const auto& vec : results) {
        for (const auto& id : vec) {
            REQUIRE(all_ids.insert(id).second);  // Insert should succeed
        }
    }

    REQUIRE(all_ids.size() == threads * ids_per_thread);
    REQUIRE(gen.current() == threads * ids_per_thread);
}

// =============================================================================
// NamedId Tests
// =============================================================================

TEST_CASE("NamedId construction", "[core][id]") {
    SECTION("default") {
        NamedId id;
        REQUIRE(id.name.empty());
        REQUIRE(id.hash == 0);
        REQUIRE_FALSE(static_cast<bool>(id));
    }

    SECTION("from string") {
        NamedId id("test_name");
        REQUIRE(id.name == "test_name");
        REQUIRE(id.hash != 0);
        REQUIRE(static_cast<bool>(id));
    }

    SECTION("from std::string") {
        std::string s = "test_name";
        NamedId id(s);
        REQUIRE(id.name == "test_name");
    }

    SECTION("from rvalue string") {
        NamedId id(std::string("test_name"));
        REQUIRE(id.name == "test_name");
    }
}

TEST_CASE("NamedId hash consistency", "[core][id]") {
    NamedId id1("test");
    NamedId id2("test");
    NamedId id3("other");

    REQUIRE(id1.hash == id2.hash);
    REQUIRE(id1.hash != id3.hash);

    // Hash should match direct FNV-1a
    REQUIRE(id1.hash == detail::fnv1a_hash("test"));
}

TEST_CASE("NamedId comparison", "[core][id]") {
    NamedId a("alpha");
    NamedId b("alpha");
    NamedId c("beta");

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE(a != c);

    // Ordering
    bool has_ordering = (a < c) || (c < a);
    REQUIRE(has_ordering);  // Some ordering exists
}

TEST_CASE("NamedId to_id conversion", "[core][id]") {
    NamedId named("test_id");
    Id id = named.to_id();

    REQUIRE(id.is_valid());
    REQUIRE(id.to_bits() == named.hash);
}

TEST_CASE("NamedId hashing", "[core][id]") {
    NamedId id1("test1");
    NamedId id2("test1");
    NamedId id3("test2");

    std::unordered_set<NamedId> set;
    set.insert(id1);

    REQUIRE(set.count(id2) == 1);
    REQUIRE(set.count(id3) == 0);
}

// =============================================================================
// FNV-1a Hash Tests
// =============================================================================

TEST_CASE("FNV-1a hash", "[core][id]") {
    SECTION("empty string") {
        std::uint64_t h = detail::fnv1a_hash("");
        REQUIRE(h == detail::FNV_OFFSET_BASIS);
    }

    SECTION("consistency") {
        std::uint64_t h1 = detail::fnv1a_hash("test");
        std::uint64_t h2 = detail::fnv1a_hash("test");
        REQUIRE(h1 == h2);
    }

    SECTION("different strings different hashes") {
        std::uint64_t h1 = detail::fnv1a_hash("alpha");
        std::uint64_t h2 = detail::fnv1a_hash("beta");
        REQUIRE(h1 != h2);
    }

    SECTION("constexpr usage") {
        constexpr std::uint64_t h = detail::fnv1a_hash("compile_time", 12);
        REQUIRE(h != 0);
    }
}
