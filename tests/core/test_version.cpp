// void_core Version tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/core/version.hpp>
#include <compare>
#include <unordered_set>

using namespace void_core;

// =============================================================================
// Version Tests
// =============================================================================

TEST_CASE("Version construction", "[core][version]") {
    SECTION("default") {
        Version v;
        REQUIRE(v.major == 0);
        REQUIRE(v.minor == 0);
        REQUIRE(v.patch == 0);
    }

    SECTION("with components") {
        Version v{1, 2, 3};
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 3);
    }

    SECTION("zero factory") {
        Version v = Version::zero();
        REQUIRE(v.major == 0);
        REQUIRE(v.minor == 0);
        REQUIRE(v.patch == 0);
    }

    SECTION("create factory") {
        Version v = Version::create(1, 2, 3);
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 2);
        REQUIRE(v.patch == 3);
    }

    SECTION("create with defaults") {
        Version v = Version::create(1);
        REQUIRE(v.major == 1);
        REQUIRE(v.minor == 0);
        REQUIRE(v.patch == 0);
    }
}

TEST_CASE("Version comparison", "[core][version]") {
    Version v100{1, 0, 0};
    Version v110{1, 1, 0};
    Version v111{1, 1, 1};
    Version v200{2, 0, 0};

    SECTION("equality") {
        REQUIRE(v100 == Version{1, 0, 0});
        REQUIRE_FALSE(v100 == v110);
    }

    SECTION("less than") {
        REQUIRE(v100 < v110);
        REQUIRE(v110 < v111);
        REQUIRE(v111 < v200);
    }

    SECTION("greater than") {
        REQUIRE(v200 > v111);
        REQUIRE(v111 > v110);
        REQUIRE(v110 > v100);
    }

    SECTION("three-way comparison") {
        auto cmp1 = v100 <=> v110;
        auto cmp2 = v110 <=> v100;
        auto cmp3 = v100 <=> v100;
        REQUIRE(std::is_lt(cmp1));
        REQUIRE(std::is_gt(cmp2));
        REQUIRE(std::is_eq(cmp3));
    }
}

TEST_CASE("Version compatibility", "[core][version]") {
    SECTION("pre-1.0 requires exact minor") {
        Version v010{0, 1, 0};
        Version v011{0, 1, 1};
        Version v020{0, 2, 0};

        REQUIRE(v011.is_compatible_with(v010));  // Same minor, higher patch
        REQUIRE_FALSE(v010.is_compatible_with(v011));  // Lower patch
        REQUIRE_FALSE(v020.is_compatible_with(v010));  // Different minor
    }

    SECTION("post-1.0 same major is compatible") {
        Version v100{1, 0, 0};
        Version v110{1, 1, 0};
        Version v111{1, 1, 1};
        Version v200{2, 0, 0};

        REQUIRE(v110.is_compatible_with(v100));  // Higher minor
        REQUIRE(v111.is_compatible_with(v100));  // Higher minor and patch
        REQUIRE(v111.is_compatible_with(v110));  // Same minor, higher patch
        REQUIRE_FALSE(v100.is_compatible_with(v110));  // Lower minor
        REQUIRE_FALSE(v200.is_compatible_with(v100));  // Different major
    }
}

TEST_CASE("Version parsing", "[core][version]") {
    SECTION("full format") {
        auto v = Version::parse("1.2.3");
        REQUIRE(v.has_value());
        REQUIRE(v->major == 1);
        REQUIRE(v->minor == 2);
        REQUIRE(v->patch == 3);
    }

    SECTION("short format") {
        auto v = Version::parse("1.2");
        REQUIRE(v.has_value());
        REQUIRE(v->major == 1);
        REQUIRE(v->minor == 2);
        REQUIRE(v->patch == 0);
    }

    SECTION("invalid format") {
        REQUIRE_FALSE(Version::parse("1").has_value());
        REQUIRE_FALSE(Version::parse("abc").has_value());
        REQUIRE_FALSE(Version::parse("1.2.3.4").has_value());
    }
}

TEST_CASE("Version string conversion", "[core][version]") {
    Version v{1, 2, 3};
    std::string s = v.to_string();
    REQUIRE(s == "1.2.3");

    Version v2{0, 0, 0};
    REQUIRE(v2.to_string() == "0.0.0");
}

TEST_CASE("Version u64 encoding", "[core][version]") {
    Version original{12, 345, 6789};
    std::uint64_t bits = original.to_u64();
    Version decoded = Version::from_u64(bits);

    REQUIRE(decoded.major == original.major);
    REQUIRE(decoded.minor == original.minor);
    REQUIRE(decoded.patch == original.patch);
    REQUIRE(decoded == original);
}

TEST_CASE("Version increment", "[core][version]") {
    Version v{1, 2, 3};

    SECTION("increment patch") {
        Version next = v.increment_patch();
        REQUIRE(next.major == 1);
        REQUIRE(next.minor == 2);
        REQUIRE(next.patch == 4);
    }

    SECTION("increment minor") {
        Version next = v.increment_minor();
        REQUIRE(next.major == 1);
        REQUIRE(next.minor == 3);
        REQUIRE(next.patch == 0);
    }

    SECTION("increment major") {
        Version next = v.increment_major();
        REQUIRE(next.major == 2);
        REQUIRE(next.minor == 0);
        REQUIRE(next.patch == 0);
    }
}

TEST_CASE("Version is_prerelease", "[core][version]") {
    REQUIRE(Version{0, 1, 0}.is_prerelease());
    REQUIRE(Version{0, 0, 1}.is_prerelease());
    REQUIRE_FALSE(Version{1, 0, 0}.is_prerelease());
    REQUIRE_FALSE(Version{2, 3, 4}.is_prerelease());
}

TEST_CASE("Version hashing", "[core][version]") {
    Version v1{1, 2, 3};
    Version v2{1, 2, 3};
    Version v3{1, 2, 4};

    std::unordered_set<Version> set;
    set.insert(v1);

    REQUIRE(set.count(v2) == 1);  // Same as v1
    REQUIRE(set.count(v3) == 0);  // Different
}
