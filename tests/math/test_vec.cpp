// void_math vector tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/math/math.hpp>

using namespace void_math;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Vec2 Tests
// =============================================================================

TEST_CASE("Vec2 constants", "[math][vec2]") {
    REQUIRE(vec2::ZERO == Vec2(0.0f, 0.0f));
    REQUIRE(vec2::ONE == Vec2(1.0f, 1.0f));
    REQUIRE(vec2::X == Vec2(1.0f, 0.0f));
    REQUIRE(vec2::Y == Vec2(0.0f, 1.0f));
}

TEST_CASE("Vec2 operations", "[math][vec2]") {
    Vec2 a(3.0f, 4.0f);
    Vec2 b(1.0f, 2.0f);

    SECTION("basic arithmetic") {
        REQUIRE(a + b == Vec2(4.0f, 6.0f));
        REQUIRE(a - b == Vec2(2.0f, 2.0f));
        REQUIRE(a * 2.0f == Vec2(6.0f, 8.0f));
        REQUIRE(a / 2.0f == Vec2(1.5f, 2.0f));
    }

    SECTION("dot product") {
        REQUIRE_THAT(glm::dot(a, b), WithinAbs(11.0f, 1e-6f));
    }

    SECTION("length") {
        REQUIRE_THAT(glm::length(a), WithinAbs(5.0f, 1e-6f));
        REQUIRE_THAT(glm::length2(a), WithinAbs(25.0f, 1e-6f));
    }

    SECTION("normalize") {
        Vec2 normalized = glm::normalize(a);
        REQUIRE_THAT(glm::length(normalized), WithinAbs(1.0f, 1e-6f));
    }

    SECTION("perpendicular") {
        Vec2 perp = perpendicular(vec2::X);
        REQUIRE_THAT(glm::dot(vec2::X, perp), WithinAbs(0.0f, 1e-6f));
    }
}

// =============================================================================
// Vec3 Tests
// =============================================================================

TEST_CASE("Vec3 constants", "[math][vec3]") {
    REQUIRE(vec3::ZERO == Vec3(0.0f, 0.0f, 0.0f));
    REQUIRE(vec3::ONE == Vec3(1.0f, 1.0f, 1.0f));
    REQUIRE(vec3::X == Vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(vec3::Y == Vec3(0.0f, 1.0f, 0.0f));
    REQUIRE(vec3::Z == Vec3(0.0f, 0.0f, 1.0f));
    REQUIRE(vec3::FORWARD == vec3::NEG_Z);
    REQUIRE(vec3::UP == vec3::Y);
    REQUIRE(vec3::RIGHT == vec3::X);
}

TEST_CASE("Vec3 operations", "[math][vec3]") {
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);

    SECTION("basic arithmetic") {
        REQUIRE(a + b == Vec3(5.0f, 7.0f, 9.0f));
        REQUIRE(a - b == Vec3(-3.0f, -3.0f, -3.0f));
        REQUIRE(a * 2.0f == Vec3(2.0f, 4.0f, 6.0f));
        REQUIRE(-a == Vec3(-1.0f, -2.0f, -3.0f));
    }

    SECTION("dot product") {
        REQUIRE_THAT(glm::dot(a, b), WithinAbs(32.0f, 1e-6f));
    }

    SECTION("cross product") {
        Vec3 cross = glm::cross(vec3::X, vec3::Y);
        REQUIRE(cross == vec3::Z);

        cross = glm::cross(vec3::Y, vec3::X);
        REQUIRE(cross == vec3::NEG_Z);
    }

    SECTION("length") {
        Vec3 v(3.0f, 4.0f, 0.0f);
        REQUIRE_THAT(glm::length(v), WithinAbs(5.0f, 1e-6f));
    }

    SECTION("normalize") {
        Vec3 v(3.0f, 0.0f, 4.0f);
        Vec3 normalized = glm::normalize(v);
        REQUIRE_THAT(glm::length(normalized), WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(normalized.x, WithinAbs(0.6f, 1e-6f));
        REQUIRE_THAT(normalized.z, WithinAbs(0.8f, 1e-6f));
    }

    SECTION("normalize_or_zero") {
        Vec3 zero_normalized = normalize_or_zero(vec3::ZERO);
        REQUIRE(zero_normalized == vec3::ZERO);

        Vec3 valid_normalized = normalize_or_zero(Vec3(3.0f, 4.0f, 0.0f));
        REQUIRE_THAT(glm::length(valid_normalized), WithinAbs(1.0f, 1e-6f));
    }

    SECTION("reflect") {
        Vec3 incident(1.0f, -1.0f, 0.0f);
        Vec3 normal = vec3::Y;
        Vec3 reflected = reflect(glm::normalize(incident), normal);
        REQUIRE_THAT(reflected.y, WithinAbs(glm::normalize(incident).x, 1e-6f));
    }

    SECTION("project") {
        Vec3 v(1.0f, 2.0f, 3.0f);
        Vec3 onto = vec3::X;
        Vec3 projected = project(v, onto);
        REQUIRE(projected == Vec3(1.0f, 0.0f, 0.0f));
    }

    SECTION("min/max") {
        Vec3 result_min = min(a, b);
        Vec3 result_max = max(a, b);
        REQUIRE(result_min == a);
        REQUIRE(result_max == b);
    }

    SECTION("lerp") {
        Vec3 result = void_math::lerp(vec3::ZERO, vec3::ONE, 0.5f);
        REQUIRE_THAT(result.x, WithinAbs(0.5f, 1e-6f));
        REQUIRE_THAT(result.y, WithinAbs(0.5f, 1e-6f));
        REQUIRE_THAT(result.z, WithinAbs(0.5f, 1e-6f));
    }

    SECTION("extend") {
        Vec4 extended = extend(a, 1.0f);
        REQUIRE(extended == Vec4(1.0f, 2.0f, 3.0f, 1.0f));
    }

    SECTION("distance") {
        Vec3 p1(0.0f, 0.0f, 0.0f);
        Vec3 p2(3.0f, 4.0f, 0.0f);
        REQUIRE_THAT(distance(p1, p2), WithinAbs(5.0f, 1e-6f));
    }
}

// =============================================================================
// Vec4 Tests
// =============================================================================

TEST_CASE("Vec4 operations", "[math][vec4]") {
    Vec4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vec4 b(5.0f, 6.0f, 7.0f, 8.0f);

    SECTION("basic arithmetic") {
        REQUIRE(a + b == Vec4(6.0f, 8.0f, 10.0f, 12.0f));
        REQUIRE(a * 2.0f == Vec4(2.0f, 4.0f, 6.0f, 8.0f));
    }

    SECTION("dot product") {
        REQUIRE_THAT(glm::dot(a, b), WithinAbs(70.0f, 1e-6f));
    }

    SECTION("truncate") {
        Vec3 truncated = truncate(a);
        REQUIRE(truncated == Vec3(1.0f, 2.0f, 3.0f));
    }

    SECTION("xyz alias") {
        REQUIRE(xyz(a) == Vec3(1.0f, 2.0f, 3.0f));
    }
}

// =============================================================================
// Vec3d (Double Precision) Tests
// =============================================================================

TEST_CASE("Vec3d operations", "[math][vec3d]") {
    Vec3d a(1.0, 2.0, 3.0);
    Vec3d b(4.0, 5.0, 6.0);

    SECTION("basic arithmetic") {
        Vec3d sum = a + b;
        REQUIRE(sum.x == 5.0);
        REQUIRE(sum.y == 7.0);
        REQUIRE(sum.z == 9.0);
    }

    SECTION("dot product") {
        REQUIRE_THAT(a.dot(b), WithinAbs(32.0, 1e-10));
    }

    SECTION("cross product") {
        Vec3d x = Vec3d::X();
        Vec3d y = Vec3d::Y();
        Vec3d z = x.cross(y);
        REQUIRE_THAT(z.x, WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(z.y, WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(z.z, WithinAbs(1.0, 1e-10));
    }

    SECTION("conversion to f32") {
        Vec3 f32 = a.to_f32();
        REQUIRE_THAT(f32.x, WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(f32.y, WithinAbs(2.0f, 1e-6f));
        REQUIRE_THAT(f32.z, WithinAbs(3.0f, 1e-6f));
    }

    SECTION("large world coordinates") {
        // Test coordinates at 500km from origin
        Vec3d large_pos(500000.0, 500000.0, 500000.0);
        REQUIRE(large_pos.is_finite());
        REQUIRE_THAT(large_pos.length(), WithinAbs(866025.403784, 0.001));
    }
}

// =============================================================================
// Utility Functions Tests
// =============================================================================

TEST_CASE("Math utility functions", "[math][utils]") {
    SECTION("radians/degrees conversion") {
        REQUIRE_THAT(radians(180.0f), WithinAbs(consts::PI, 1e-6f));
        REQUIRE_THAT(degrees(consts::PI), WithinAbs(180.0f, 1e-6f));
        REQUIRE_THAT(radians(90.0f), WithinAbs(consts::FRAC_PI_2, 1e-6f));
    }

    SECTION("lerp") {
        REQUIRE_THAT(lerp(0.0f, 10.0f, 0.5f), WithinAbs(5.0f, 1e-6f));
        REQUIRE_THAT(lerp(0.0f, 10.0f, 0.0f), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(lerp(0.0f, 10.0f, 1.0f), WithinAbs(10.0f, 1e-6f));
    }

    SECTION("clamp") {
        REQUIRE(clamp(5.0f, 0.0f, 10.0f) == 5.0f);
        REQUIRE(clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
        REQUIRE(clamp(15.0f, 0.0f, 10.0f) == 10.0f);
    }

    SECTION("smoothstep") {
        REQUIRE_THAT(smoothstep(0.0f, 1.0f, 0.0f), WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(smoothstep(0.0f, 1.0f, 1.0f), WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(smoothstep(0.0f, 1.0f, 0.5f), WithinAbs(0.5f, 1e-6f));
    }

    SECTION("approx_equal") {
        REQUIRE(approx_equal(1.0f, 1.0f + 1e-7f));
        REQUIRE_FALSE(approx_equal(1.0f, 2.0f));
    }
}
