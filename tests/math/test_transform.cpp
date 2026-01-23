// void_math transform tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/math/math.hpp>

using namespace void_math;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Transform Creation Tests
// =============================================================================

TEST_CASE("Transform default", "[math][transform]") {
    Transform t;
    REQUIRE(t.position == vec3::ZERO);
    REQUIRE(approx_equal(t.rotation, quat::IDENTITY));
    REQUIRE(t.scale_ == vec3::ONE);
}

TEST_CASE("Transform from_position", "[math][transform]") {
    Transform t = Transform::from_position(Vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(t.position == Vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(approx_equal(t.rotation, quat::IDENTITY));
    REQUIRE(t.scale_ == vec3::ONE);
}

TEST_CASE("Transform from_position_rotation", "[math][transform]") {
    Quat rot = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
    Transform t = Transform::from_position_rotation(Vec3(1.0f, 0.0f, 0.0f), rot);
    REQUIRE(t.position == Vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(approx_equal(t.rotation, rot));
}

TEST_CASE("Transform builder pattern", "[math][transform]") {
    Transform t = Transform()
        .with_position(Vec3(1.0f, 2.0f, 3.0f))
        .with_scale(Vec3(2.0f, 2.0f, 2.0f));

    REQUIRE(t.position == Vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(t.scale_ == Vec3(2.0f, 2.0f, 2.0f));
}

// =============================================================================
// Transform Matrix Conversion Tests
// =============================================================================

TEST_CASE("Transform to_matrix", "[math][transform]") {
    Transform t;
    t.position = Vec3(10.0f, 0.0f, 0.0f);
    t.rotation = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
    t.scale_ = Vec3(2.0f, 2.0f, 2.0f);

    Mat4 m = t.to_matrix();

    // Test that matrix correctly transforms points
    Vec3 point = transform_point(m, vec3::X);
    // Scale by 2, rotate 90 deg around Y (X -> -Z), translate by (10, 0, 0)
    REQUIRE_THAT(point.x, WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(point.y, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(point.z, WithinAbs(-2.0f, 1e-5f));
}

// =============================================================================
// Transform Point Transformation Tests
// =============================================================================

TEST_CASE("Transform transform_point", "[math][transform]") {
    Transform t;
    t.position = Vec3(5.0f, 0.0f, 0.0f);

    Vec3 result = t.transform_point(vec3::X);
    REQUIRE(result == Vec3(6.0f, 0.0f, 0.0f));
}

TEST_CASE("Transform transform_direction", "[math][transform]") {
    Transform t;
    t.rotation = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);

    Vec3 result = t.transform_direction(vec3::X);
    REQUIRE_THAT(result.x, WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(result.z, WithinAbs(-1.0f, 1e-6f));
}

// =============================================================================
// Transform Direction Vectors Tests
// =============================================================================

TEST_CASE("Transform direction vectors", "[math][transform]") {
    Transform t;

    SECTION("identity transform") {
        REQUIRE_THAT(glm::dot(t.forward(), vec3::NEG_Z), WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(glm::dot(t.right(), vec3::X), WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(glm::dot(t.up(), vec3::Y), WithinAbs(1.0f, 1e-6f));
    }

    SECTION("rotated transform") {
        t.rotation = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
        // After 90 deg rotation around Y:
        // Forward (-Z) becomes -X
        // Right (X) becomes -Z
        Vec3 fwd = t.forward();
        REQUIRE_THAT(fwd.x, WithinAbs(-1.0f, 1e-6f));

        Vec3 rgt = t.right();
        REQUIRE_THAT(rgt.z, WithinAbs(-1.0f, 1e-6f));
    }
}

// =============================================================================
// Transform Inverse and Composition Tests
// =============================================================================

TEST_CASE("Transform inverse", "[math][transform]") {
    Transform t;
    t.position = Vec3(5.0f, 10.0f, 15.0f);
    t.rotation = quat_from_axis_angle(vec3::Y, 0.5f);
    t.scale_ = Vec3(2.0f, 3.0f, 4.0f);

    Transform inv = t.inverse();
    Transform combined = t.combine(inv);

    // Combined should be approximately identity
    REQUIRE(approx_equal(combined.position, vec3::ZERO, 1e-4f));
    REQUIRE(approx_equal(combined.rotation, quat::IDENTITY, 1e-4f));
    REQUIRE(approx_equal(combined.scale_, vec3::ONE, 1e-4f));
}

TEST_CASE("Transform combine", "[math][transform]") {
    Transform parent;
    parent.position = Vec3(10.0f, 0.0f, 0.0f);

    Transform child;
    child.position = Vec3(5.0f, 0.0f, 0.0f);

    Transform combined = parent.combine(child);
    REQUIRE(combined.position == Vec3(15.0f, 0.0f, 0.0f));
}

TEST_CASE("Transform lerp", "[math][transform]") {
    Transform a;
    a.position = vec3::ZERO;
    a.scale_ = vec3::ONE;

    Transform b;
    b.position = Vec3(10.0f, 10.0f, 10.0f);
    b.scale_ = Vec3(2.0f, 2.0f, 2.0f);

    Transform mid = a.lerp(b, 0.5f);

    REQUIRE_THAT(mid.position.x, WithinAbs(5.0f, 1e-6f));
    REQUIRE_THAT(mid.scale_.x, WithinAbs(1.5f, 1e-6f));
}

// =============================================================================
// Transform Mutation Tests
// =============================================================================

TEST_CASE("Transform translate_local", "[math][transform]") {
    Transform t;
    t.rotation = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);

    t.translate_local(Vec3(1.0f, 0.0f, 0.0f));

    // Local X after 90 deg Y rotation is -Z in world
    REQUIRE_THAT(t.position.x, WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(t.position.z, WithinAbs(-1.0f, 1e-6f));
}

TEST_CASE("Transform translate_world", "[math][transform]") {
    Transform t;
    t.rotation = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);

    t.translate_world(Vec3(1.0f, 0.0f, 0.0f));

    // World X should be world X regardless of rotation
    REQUIRE_THAT(t.position.x, WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Transform rotate_around_axis", "[math][transform]") {
    Transform t;
    t.rotate_around_axis(vec3::Y, consts::FRAC_PI_2);

    Vec3 fwd = t.forward();
    REQUIRE_THAT(fwd.x, WithinAbs(-1.0f, 1e-6f));
}

TEST_CASE("Transform operator*", "[math][transform]") {
    Transform a = Transform::from_position(Vec3(5.0f, 0.0f, 0.0f));
    Transform b = Transform::from_position(Vec3(3.0f, 0.0f, 0.0f));

    Transform combined = a * b;
    REQUIRE(combined.position == Vec3(8.0f, 0.0f, 0.0f));
}
