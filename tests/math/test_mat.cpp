// void_math matrix tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/math/math.hpp>

using namespace void_math;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Mat3 Tests
// =============================================================================

TEST_CASE("Mat3 operations", "[math][mat3]") {
    SECTION("identity") {
        Mat3 identity = mat3::IDENTITY;
        Vec3 v(1.0f, 2.0f, 3.0f);
        Vec3 result = identity * v;
        REQUIRE(result == v);
    }

    SECTION("from_scale") {
        Mat3 scale_mat = mat3_from_scale(Vec3(2.0f, 3.0f, 4.0f));
        Vec3 v(1.0f, 1.0f, 1.0f);
        Vec3 result = scale_mat * v;
        REQUIRE(result == Vec3(2.0f, 3.0f, 4.0f));
    }

    SECTION("transpose") {
        Mat3 m = mat3_from_cols(
            Vec3(1.0f, 2.0f, 3.0f),
            Vec3(4.0f, 5.0f, 6.0f),
            Vec3(7.0f, 8.0f, 9.0f)
        );
        Mat3 t = glm::transpose(m);
        REQUIRE(t[0][0] == 1.0f);
        REQUIRE(t[0][1] == 4.0f);
        REQUIRE(t[0][2] == 7.0f);
    }
}

// =============================================================================
// Mat4 Tests
// =============================================================================

TEST_CASE("Mat4 identity", "[math][mat4]") {
    Mat4 identity = mat4::IDENTITY;
    Vec4 v(1.0f, 2.0f, 3.0f, 1.0f);
    Vec4 result = identity * v;
    REQUIRE(result == v);
}

TEST_CASE("Mat4 translation", "[math][mat4]") {
    Mat4 t = translation(Vec3(10.0f, 20.0f, 30.0f));
    Vec3 point = transform_point(t, vec3::ZERO);
    REQUIRE_THAT(point.x, WithinAbs(10.0f, 1e-6f));
    REQUIRE_THAT(point.y, WithinAbs(20.0f, 1e-6f));
    REQUIRE_THAT(point.z, WithinAbs(30.0f, 1e-6f));

    // Vectors should not be affected by translation
    Vec3 vector = transform_vector(t, Vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(vector == Vec3(1.0f, 0.0f, 0.0f));
}

TEST_CASE("Mat4 scale", "[math][mat4]") {
    Mat4 s = scale(Vec3(2.0f, 3.0f, 4.0f));
    Vec3 point = transform_point(s, Vec3(1.0f, 1.0f, 1.0f));
    REQUIRE_THAT(point.x, WithinAbs(2.0f, 1e-6f));
    REQUIRE_THAT(point.y, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(point.z, WithinAbs(4.0f, 1e-6f));
}

TEST_CASE("Mat4 rotation", "[math][mat4]") {
    SECTION("rotation_x") {
        Mat4 r = rotation_x(consts::FRAC_PI_2);  // 90 degrees
        Vec3 point = transform_point(r, vec3::Y);
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(point.z, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("rotation_y") {
        Mat4 r = rotation_y(consts::FRAC_PI_2);  // 90 degrees
        Vec3 point = transform_point(r, vec3::X);
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(point.z, WithinAbs(-1.0f, 1e-6f));
    }

    SECTION("rotation_z") {
        Mat4 r = rotation_z(consts::FRAC_PI_2);  // 90 degrees
        Vec3 point = transform_point(r, vec3::X);
        REQUIRE_THAT(point.x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(point.y, WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(point.z, WithinAbs(0.0f, 1e-6f));
    }

    SECTION("rotation_axis_angle") {
        Mat4 r = rotation_axis_angle(vec3::Y, consts::PI);  // 180 degrees around Y
        Vec3 point = transform_point(r, vec3::X);
        REQUIRE_THAT(point.x, WithinAbs(-1.0f, 1e-6f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(point.z, WithinAbs(0.0f, 1e-6f));
    }
}

TEST_CASE("Mat4 inverse", "[math][mat4]") {
    Mat4 t = translation(Vec3(5.0f, 10.0f, 15.0f));
    Mat4 inv = inverse(t);
    Mat4 identity_check = t * inv;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            REQUIRE_THAT(identity_check[i][j], WithinAbs(expected, 1e-5f));
        }
    }
}

TEST_CASE("Mat4 look_at", "[math][mat4]") {
    Vec3 eye(0.0f, 0.0f, 5.0f);
    Vec3 target = vec3::ZERO;
    Vec3 up = vec3::Y;

    Mat4 view = look_at(eye, target, up);

    // Transform eye position should be at origin in view space
    Vec3 eye_view = transform_point(view, eye);
    REQUIRE_THAT(eye_view.x, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(eye_view.y, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(eye_view.z, WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Mat4 perspective", "[math][mat4]") {
    float fov = radians(60.0f);
    float aspect = 16.0f / 9.0f;
    float near = 0.1f;
    float far = 100.0f;

    Mat4 proj = perspective(fov, aspect, near, far);

    // A point at -near on Z axis should map to Z=0 in NDC (depth [0,1])
    Vec4 near_point(0.0f, 0.0f, -near, 1.0f);
    Vec4 proj_point = proj * near_point;
    float ndc_z = proj_point.z / proj_point.w;
    REQUIRE_THAT(ndc_z, WithinAbs(0.0f, 1e-5f));

    // A point at -far on Z axis should map to Z=1 in NDC
    Vec4 far_point(0.0f, 0.0f, -far, 1.0f);
    proj_point = proj * far_point;
    ndc_z = proj_point.z / proj_point.w;
    REQUIRE_THAT(ndc_z, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Mat4 orthographic", "[math][mat4]") {
    Mat4 ortho = orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);

    // Center of the view volume should map to (0, 0, ~0.5)
    Vec4 center(0.0f, 0.0f, -50.0f, 1.0f);
    Vec4 proj = ortho * center;
    REQUIRE_THAT(proj.x, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(proj.y, WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("Mat4 get/set translation", "[math][mat4]") {
    Mat4 m = translation(Vec3(1.0f, 2.0f, 3.0f));
    Vec3 t = get_translation(m);
    REQUIRE(t == Vec3(1.0f, 2.0f, 3.0f));

    set_translation(m, Vec3(4.0f, 5.0f, 6.0f));
    t = get_translation(m);
    REQUIRE(t == Vec3(4.0f, 5.0f, 6.0f));
}

TEST_CASE("Mat4 get_scale", "[math][mat4]") {
    Mat4 s = scale(Vec3(2.0f, 3.0f, 4.0f));
    Vec3 extracted = get_scale(s);
    REQUIRE_THAT(extracted.x, WithinAbs(2.0f, 1e-6f));
    REQUIRE_THAT(extracted.y, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(extracted.z, WithinAbs(4.0f, 1e-6f));
}

TEST_CASE("Mat4 to_array", "[math][mat4]") {
    Mat4 m = mat4::IDENTITY;
    auto arr = to_array(m);
    REQUIRE(arr.size() == 16);
    // Column-major: first column is (1,0,0,0)
    REQUIRE_THAT(arr[0], WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(arr[1], WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(arr[5], WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Mat4 composition", "[math][mat4]") {
    Mat4 t = translation(Vec3(10.0f, 0.0f, 0.0f));
    Mat4 r = rotation_y(consts::FRAC_PI_2);

    // Apply rotation then translation
    Mat4 combined = t * r;
    Vec3 point = transform_point(combined, vec3::X);

    // X rotated 90 degrees around Y becomes -Z, then translated by (10, 0, 0)
    REQUIRE_THAT(point.x, WithinAbs(10.0f, 1e-5f));
    REQUIRE_THAT(point.y, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(point.z, WithinAbs(-1.0f, 1e-5f));
}
