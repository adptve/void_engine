// void_math quaternion tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/math/math.hpp>

using namespace void_math;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Quaternion Creation Tests
// =============================================================================

TEST_CASE("Quat identity", "[math][quat]") {
    Quat identity = quat::IDENTITY;
    Vec3 v(1.0f, 2.0f, 3.0f);
    Vec3 rotated = rotate(identity, v);
    REQUIRE_THAT(rotated.x, WithinAbs(v.x, 1e-6f));
    REQUIRE_THAT(rotated.y, WithinAbs(v.y, 1e-6f));
    REQUIRE_THAT(rotated.z, WithinAbs(v.z, 1e-6f));
}

TEST_CASE("Quat from axis-angle", "[math][quat]") {
    SECTION("90 degrees around Y") {
        Quat q = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.z, WithinAbs(-1.0f, 1e-6f));
    }

    SECTION("180 degrees around Z") {
        Quat q = quat_from_axis_angle(vec3::Z, consts::PI);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.x, WithinAbs(-1.0f, 1e-6f));
        REQUIRE_THAT(rotated.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.z, WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Quat from Euler angles", "[math][quat]") {
    SECTION("pitch only (X rotation)") {
        Quat q = quat_from_euler(consts::FRAC_PI_2, 0.0f, 0.0f);
        Vec3 rotated = rotate(q, vec3::Y);
        REQUIRE_THAT(rotated.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.z, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("yaw only (Y rotation)") {
        Quat q = quat_from_euler(0.0f, consts::FRAC_PI_2, 0.0f);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.z, WithinAbs(-1.0f, 1e-6f));
    }
}

TEST_CASE("Quat rotation_x/y/z", "[math][quat]") {
    SECTION("rotation_x") {
        Quat q = quat_rotation_x(consts::FRAC_PI_2);
        Vec3 rotated = rotate(q, vec3::Y);
        REQUIRE_THAT(rotated.z, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("rotation_y") {
        Quat q = quat_rotation_y(consts::FRAC_PI_2);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.z, WithinAbs(-1.0f, 1e-6f));
    }

    SECTION("rotation_z") {
        Quat q = quat_rotation_z(consts::FRAC_PI_2);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.y, WithinAbs(1.0f, 1e-6f));
    }
}

TEST_CASE("Quat from rotation arc", "[math][quat]") {
    SECTION("X to Y") {
        Quat q = quat_from_rotation_arc(vec3::X, vec3::Y);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(rotated.y, WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(rotated.z, WithinAbs(0.0f, 1e-6f));
    }

    SECTION("same direction") {
        Quat q = quat_from_rotation_arc(vec3::X, vec3::X);
        REQUIRE(approx_equal(q, quat::IDENTITY));
    }

    SECTION("opposite direction") {
        Quat q = quat_from_rotation_arc(vec3::X, vec3::NEG_X);
        Vec3 rotated = rotate(q, vec3::X);
        REQUIRE_THAT(rotated.x, WithinAbs(-1.0f, 1e-6f));
    }
}

// =============================================================================
// Quaternion Operations Tests
// =============================================================================

TEST_CASE("Quat normalize", "[math][quat]") {
    Quat q(2.0f, 0.0f, 0.0f, 0.0f);  // w, x, y, z - not normalized
    Quat normalized = glm::normalize(q);
    REQUIRE_THAT(glm::length(normalized), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Quat conjugate", "[math][quat]") {
    Quat q = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_4);
    Quat conj = conjugate(q);

    // q * conjugate(q) should be identity for unit quaternions
    Quat result = q * conj;
    REQUIRE(approx_equal(result, quat::IDENTITY));
}

TEST_CASE("Quat inverse", "[math][quat]") {
    Quat q = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_4);
    Quat inv = inverse(q);

    // q * inverse should be identity
    Quat result = q * inv;
    REQUIRE(approx_equal(result, quat::IDENTITY));
}

TEST_CASE("Quat multiplication (composition)", "[math][quat]") {
    Quat q1 = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
    Quat q2 = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
    Quat combined = q1 * q2;

    // Two 90-degree rotations = 180-degree rotation
    Vec3 rotated = rotate(combined, vec3::X);
    REQUIRE_THAT(rotated.x, WithinAbs(-1.0f, 1e-6f));
    REQUIRE_THAT(rotated.y, WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(rotated.z, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Quat slerp", "[math][quat]") {
    Quat q1 = quat::IDENTITY;
    Quat q2 = quat_from_axis_angle(vec3::Y, consts::PI);

    SECTION("t = 0") {
        Quat result = slerp(q1, q2, 0.0f);
        REQUIRE(approx_equal(result, q1));
    }

    SECTION("t = 1") {
        Quat result = slerp(q1, q2, 1.0f);
        REQUIRE(approx_equal(result, q2));
    }

    SECTION("t = 0.5") {
        Quat result = slerp(q1, q2, 0.5f);
        Vec3 rotated = rotate(result, vec3::X);
        // Halfway between 0 and 180 degrees = 90 degrees
        REQUIRE_THAT(rotated.x, WithinAbs(0.0f, 1e-5f));
        REQUIRE_THAT(rotated.z, WithinAbs(-1.0f, 1e-5f));
    }
}

TEST_CASE("Quat lerp", "[math][quat]") {
    Quat q1 = quat::IDENTITY;
    Quat q2 = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);

    Quat result = lerp(q1, q2, 0.5f);
    // Result should be normalized for unit quaternion interpolation
    result = glm::normalize(result);
    REQUIRE_THAT(glm::length(result), WithinAbs(1.0f, 1e-6f));
}

// =============================================================================
// Quaternion Conversion Tests
// =============================================================================

TEST_CASE("Quat to axis-angle", "[math][quat]") {
    Vec3 axis = glm::normalize(Vec3(1.0f, 1.0f, 0.0f));
    float angle = consts::FRAC_PI_4;
    Quat q = quat_from_axis_angle(axis, angle);

    auto [result_axis, result_angle] = to_axis_angle(q);

    REQUIRE_THAT(result_angle, WithinAbs(angle, 1e-5f));
    REQUIRE_THAT(glm::dot(result_axis, axis), WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Quat to Euler", "[math][quat]") {
    Vec3 euler(0.1f, 0.2f, 0.3f);
    Quat q = quat_from_euler(euler);
    Vec3 result = to_euler(q);

    REQUIRE_THAT(result.x, WithinAbs(euler.x, 1e-5f));
    REQUIRE_THAT(result.y, WithinAbs(euler.y, 1e-5f));
    REQUIRE_THAT(result.z, WithinAbs(euler.z, 1e-5f));
}

TEST_CASE("Quat to Mat3", "[math][quat]") {
    Quat q = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);
    Mat3 m = quat_to_mat3(q);

    Vec3 rotated_quat = rotate(q, vec3::X);
    Vec3 rotated_mat = m * vec3::X;

    REQUIRE_THAT(rotated_mat.x, WithinAbs(rotated_quat.x, 1e-6f));
    REQUIRE_THAT(rotated_mat.y, WithinAbs(rotated_quat.y, 1e-6f));
    REQUIRE_THAT(rotated_mat.z, WithinAbs(rotated_quat.z, 1e-6f));
}

TEST_CASE("Quat to Mat4", "[math][quat]") {
    Quat q = quat_from_axis_angle(vec3::Z, consts::FRAC_PI_2);
    Mat4 m = quat_to_mat4(q);

    Vec3 rotated_quat = rotate(q, vec3::X);
    Vec3 rotated_mat = transform_point(m, vec3::X);

    REQUIRE_THAT(rotated_mat.x, WithinAbs(rotated_quat.x, 1e-6f));
    REQUIRE_THAT(rotated_mat.y, WithinAbs(rotated_quat.y, 1e-6f));
    REQUIRE_THAT(rotated_mat.z, WithinAbs(rotated_quat.z, 1e-6f));
}

TEST_CASE("Quat from Mat4", "[math][quat]") {
    Quat original = quat_from_axis_angle(vec3::Y, 0.7f);
    Mat4 m = quat_to_mat4(original);
    Quat extracted = quat_from_mat4(m);

    REQUIRE(approx_equal(original, extracted));
}

TEST_CASE("Quat angle between", "[math][quat]") {
    Quat q1 = quat::IDENTITY;
    Quat q2 = quat_from_axis_angle(vec3::Y, consts::FRAC_PI_2);

    float angle = angle_between(q1, q2);
    REQUIRE_THAT(angle, WithinAbs(consts::FRAC_PI_2, 1e-5f));
}
