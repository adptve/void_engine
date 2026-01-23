#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/render/camera.hpp>
#include <numbers>

using namespace void_render;
using Catch::Matchers::WithinAbs;

TEST_CASE("PerspectiveProjection", "[render][camera]") {
    SECTION("default values") {
        PerspectiveProjection proj;
        REQUIRE_THAT(proj.fov_y, WithinAbs(std::numbers::pi_v<float> / 4.0f, 0.001f));
        REQUIRE_THAT(proj.near_plane, WithinAbs(0.1f, 0.001f));
        REQUIRE_THAT(proj.far_plane, WithinAbs(1000.0f, 0.001f));
    }

    SECTION("with aspect ratio") {
        auto proj = PerspectiveProjection::with_aspect(2.0f);
        REQUIRE_THAT(proj.aspect_ratio, WithinAbs(2.0f, 0.001f));
    }

    SECTION("with size") {
        auto proj = PerspectiveProjection::with_size(1920.0f, 1080.0f);
        REQUIRE_THAT(proj.aspect_ratio, WithinAbs(1920.0f / 1080.0f, 0.001f));
    }

    SECTION("projection matrix is valid") {
        PerspectiveProjection proj;
        auto mat = proj.matrix();

        // Check key elements
        REQUIRE(mat[0][0] != 0.0f);  // X scale
        REQUIRE(mat[1][1] != 0.0f);  // Y scale
        REQUIRE(mat[2][3] != 0.0f);  // Perspective divide
    }
}

TEST_CASE("OrthographicProjection", "[render][camera]") {
    SECTION("symmetric creation") {
        auto proj = OrthographicProjection::symmetric(20.0f, 10.0f);
        REQUIRE_THAT(proj.left, WithinAbs(-10.0f, 0.001f));
        REQUIRE_THAT(proj.right, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(proj.bottom, WithinAbs(-5.0f, 0.001f));
        REQUIRE_THAT(proj.top, WithinAbs(5.0f, 0.001f));
    }

    SECTION("projection matrix is valid") {
        auto proj = OrthographicProjection::symmetric(10.0f, 10.0f);
        auto mat = proj.matrix();

        // Orthographic has no perspective divide
        REQUIRE_THAT(mat[2][3], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(mat[3][3], WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("Camera", "[render][camera]") {
    SECTION("default camera") {
        Camera cam;
        auto pos = cam.position();
        REQUIRE_THAT(pos[0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(pos[1], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(pos[2], WithinAbs(0.0f, 0.001f));
    }

    SECTION("set position") {
        Camera cam;
        cam.set_position(10.0f, 20.0f, 30.0f);
        cam.update();

        auto pos = cam.position();
        REQUIRE_THAT(pos[0], WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(pos[1], WithinAbs(20.0f, 0.001f));
        REQUIRE_THAT(pos[2], WithinAbs(30.0f, 0.001f));
    }

    SECTION("forward direction") {
        Camera cam;
        cam.set_rotation(0.0f, 0.0f);  // Looking along -Z
        cam.update();

        auto fwd = cam.forward();
        REQUIRE_THAT(fwd[0], WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(fwd[1], WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(fwd[2], WithinAbs(-1.0f, 0.01f));
    }

    SECTION("right direction") {
        Camera cam;
        cam.set_rotation(0.0f, 0.0f);
        cam.update();

        auto rgt = cam.right();
        REQUIRE_THAT(rgt[0], WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(rgt[1], WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(rgt[2], WithinAbs(0.0f, 0.01f));
    }

    SECTION("look_at") {
        Camera cam;
        cam.set_position(0.0f, 0.0f, 10.0f);
        cam.look_at({0.0f, 0.0f, 0.0f});
        cam.update();

        auto fwd = cam.forward();
        // Should be looking towards origin (negative Z from position)
        REQUIRE_THAT(fwd[2], WithinAbs(-1.0f, 0.1f));
    }

    SECTION("move camera") {
        Camera cam;
        cam.set_position(0.0f, 0.0f, 0.0f);
        cam.set_rotation(0.0f, 0.0f);
        cam.update();

        cam.move(1.0f, 0.0f, 0.0f);  // Move forward
        cam.update();

        auto pos = cam.position();
        // Moved along forward direction (-Z)
        REQUIRE_THAT(pos[2], WithinAbs(-1.0f, 0.01f));
    }

    SECTION("GPU data") {
        Camera cam;
        cam.set_position(5.0f, 5.0f, 5.0f);
        cam.update();

        auto gpu = cam.gpu_data();
        REQUIRE(sizeof(gpu) == GpuCameraData::SIZE);
    }
}

TEST_CASE("CameraController", "[render][camera]") {
    Camera cam;
    CameraController controller(&cam);

    SECTION("default mode is FPS") {
        REQUIRE(controller.mode() == CameraMode::Fps);
    }

    SECTION("change mode") {
        controller.set_mode(CameraMode::Orbit);
        REQUIRE(controller.mode() == CameraMode::Orbit);
    }

    SECTION("FPS mode movement") {
        cam.set_position(0.0f, 0.0f, 0.0f);
        cam.set_rotation(0.0f, 0.0f);
        cam.update();

        CameraInput input;
        input.forward = 1.0f;
        input.delta_time = 1.0f;

        controller.update(input);

        auto pos = cam.position();
        // Should have moved forward
        REQUIRE(pos[2] < 0.0f);  // -Z is forward
    }

    SECTION("orbit mode") {
        controller.set_mode(CameraMode::Orbit);
        controller.set_orbit_target({0.0f, 0.0f, 0.0f});
        controller.settings().orbit_distance = 10.0f;

        CameraInput input;
        input.delta_time = 0.016f;

        controller.update(input);

        // Camera should be at orbit distance from target
        auto pos = cam.position();
        float dist = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
        REQUIRE_THAT(dist, WithinAbs(10.0f, 1.0f));
    }
}

TEST_CASE("Frustum", "[render][camera]") {
    Camera cam;
    cam.set_position(0.0f, 0.0f, 10.0f);
    cam.look_at({0.0f, 0.0f, 0.0f});
    cam.update();

    Frustum frustum;
    frustum.extract(cam);

    SECTION("contains_sphere - inside") {
        // Sphere at origin, should be inside frustum
        bool inside = frustum.contains_sphere({0.0f, 0.0f, 0.0f}, 1.0f);
        REQUIRE(inside);
    }

    SECTION("contains_sphere - outside") {
        // Sphere far behind camera, should be outside
        bool inside = frustum.contains_sphere({0.0f, 0.0f, 100.0f}, 1.0f);
        REQUIRE_FALSE(inside);
    }

    SECTION("contains_aabb - inside") {
        bool inside = frustum.contains_aabb({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f});
        REQUIRE(inside);
    }

    SECTION("contains_aabb - outside") {
        bool inside = frustum.contains_aabb({-100.0f, -100.0f, -100.0f}, {-90.0f, -90.0f, -90.0f});
        REQUIRE_FALSE(inside);
    }
}
