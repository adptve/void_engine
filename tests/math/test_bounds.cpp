// void_math bounds tests (AABB, Sphere, Frustum)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/math/math.hpp>
#include <vector>

using namespace void_math;
using Catch::Matchers::WithinAbs;

// =============================================================================
// AABB Tests
// =============================================================================

TEST_CASE("AABB construction", "[math][aabb]") {
    SECTION("from min/max") {
        AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));
        REQUIRE(box.min == Vec3(-1.0f, -1.0f, -1.0f));
        REQUIRE(box.max == Vec3(1.0f, 1.0f, 1.0f));
    }

    SECTION("from center and half extents") {
        AABB box = AABB::from_center_half_extents(vec3::ZERO, Vec3(2.0f, 2.0f, 2.0f));
        REQUIRE(box.min == Vec3(-2.0f, -2.0f, -2.0f));
        REQUIRE(box.max == Vec3(2.0f, 2.0f, 2.0f));
    }

    SECTION("from points") {
        std::vector<Vec3> points = {
            Vec3(-1.0f, 0.0f, 0.0f),
            Vec3(1.0f, 0.0f, 0.0f),
            Vec3(0.0f, 2.0f, 0.0f),
            Vec3(0.0f, 0.0f, 3.0f)
        };
        AABB box = AABB::from_points(points);
        REQUIRE(box.min == Vec3(-1.0f, 0.0f, 0.0f));
        REQUIRE(box.max == Vec3(1.0f, 2.0f, 3.0f));
    }
}

TEST_CASE("AABB properties", "[math][aabb]") {
    AABB box(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 4.0f, 6.0f));

    SECTION("center") {
        REQUIRE(box.center() == Vec3(1.0f, 2.0f, 3.0f));
    }

    SECTION("half_extents") {
        REQUIRE(box.half_extents() == Vec3(1.0f, 2.0f, 3.0f));
    }

    SECTION("size") {
        REQUIRE(box.size() == Vec3(2.0f, 4.0f, 6.0f));
    }

    SECTION("volume") {
        REQUIRE_THAT(box.volume(), WithinAbs(48.0f, 1e-6f));
    }

    SECTION("surface_area") {
        // 2 * (2*4 + 4*6 + 6*2) = 2 * (8 + 24 + 12) = 88
        REQUIRE_THAT(box.surface_area(), WithinAbs(88.0f, 1e-6f));
    }
}

TEST_CASE("AABB containment", "[math][aabb]") {
    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    SECTION("contains_point") {
        REQUIRE(box.contains_point(vec3::ZERO));
        REQUIRE(box.contains_point(Vec3(0.5f, 0.5f, 0.5f)));
        REQUIRE(box.contains_point(Vec3(1.0f, 1.0f, 1.0f)));  // On boundary
        REQUIRE_FALSE(box.contains_point(Vec3(2.0f, 0.0f, 0.0f)));
    }

    SECTION("contains_aabb") {
        AABB inner(Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, 0.5f));
        AABB outer(Vec3(-2.0f, -2.0f, -2.0f), Vec3(2.0f, 2.0f, 2.0f));

        REQUIRE(box.contains_aabb(inner));
        REQUIRE_FALSE(box.contains_aabb(outer));
    }
}

TEST_CASE("AABB intersection", "[math][aabb]") {
    AABB box1(Vec3(0.0f, 0.0f, 0.0f), Vec3(2.0f, 2.0f, 2.0f));
    AABB box2(Vec3(1.0f, 1.0f, 1.0f), Vec3(3.0f, 3.0f, 3.0f));
    AABB box3(Vec3(5.0f, 5.0f, 5.0f), Vec3(6.0f, 6.0f, 6.0f));

    REQUIRE(box1.intersects(box2));
    REQUIRE_FALSE(box1.intersects(box3));
}

TEST_CASE("AABB closest_point", "[math][aabb]") {
    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    SECTION("point inside") {
        Vec3 closest = box.closest_point(Vec3(0.5f, 0.5f, 0.5f));
        REQUIRE(closest == Vec3(0.5f, 0.5f, 0.5f));
    }

    SECTION("point outside") {
        Vec3 closest = box.closest_point(Vec3(5.0f, 0.0f, 0.0f));
        REQUIRE(closest == Vec3(1.0f, 0.0f, 0.0f));
    }
}

TEST_CASE("AABB expand", "[math][aabb]") {
    AABB box(vec3::ZERO, vec3::ZERO);

    box.expand_to_include(Vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(box.max.x == 1.0f);

    box.expand_to_include(Vec3(-1.0f, 0.0f, 0.0f));
    REQUIRE(box.min.x == -1.0f);
}

TEST_CASE("AABB transform", "[math][aabb]") {
    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));
    Mat4 t = translation(Vec3(5.0f, 0.0f, 0.0f));

    AABB transformed = box.transform(t);
    REQUIRE(transformed.center() == Vec3(5.0f, 0.0f, 0.0f));
}

TEST_CASE("AABB corners", "[math][aabb]") {
    AABB box(vec3::ZERO, vec3::ONE);
    auto corners = box.corners();
    REQUIRE(corners.size() == 8);
    REQUIRE(corners[0] == vec3::ZERO);
    REQUIRE(corners[7] == vec3::ONE);
}

// =============================================================================
// Sphere Tests
// =============================================================================

TEST_CASE("Sphere construction", "[math][sphere]") {
    Sphere s(Vec3(1.0f, 2.0f, 3.0f), 5.0f);
    REQUIRE(s.center == Vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(s.radius == 5.0f);
}

TEST_CASE("Sphere from_aabb", "[math][sphere]") {
    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));
    Sphere s = Sphere::from_aabb(box);

    REQUIRE(s.center == vec3::ZERO);
    REQUIRE_THAT(s.radius, WithinAbs(std::sqrt(3.0f), 1e-6f));
}

TEST_CASE("Sphere properties", "[math][sphere]") {
    Sphere s(vec3::ZERO, 2.0f);

    SECTION("volume") {
        float expected = (4.0f / 3.0f) * consts::PI * 8.0f;  // (4/3) * pi * r^3
        REQUIRE_THAT(s.volume(), WithinAbs(expected, 1e-4f));
    }

    SECTION("surface_area") {
        float expected = 4.0f * consts::PI * 4.0f;  // 4 * pi * r^2
        REQUIRE_THAT(s.surface_area(), WithinAbs(expected, 1e-4f));
    }
}

TEST_CASE("Sphere containment", "[math][sphere]") {
    Sphere s(vec3::ZERO, 5.0f);

    SECTION("contains_point") {
        REQUIRE(s.contains_point(vec3::ZERO));
        REQUIRE(s.contains_point(Vec3(4.0f, 0.0f, 0.0f)));
        REQUIRE(s.contains_point(Vec3(5.0f, 0.0f, 0.0f)));  // On surface
        REQUIRE_FALSE(s.contains_point(Vec3(6.0f, 0.0f, 0.0f)));
    }

    SECTION("contains_sphere") {
        Sphere inner(vec3::ZERO, 2.0f);
        Sphere outer(vec3::ZERO, 10.0f);

        REQUIRE(s.contains_sphere(inner));
        REQUIRE_FALSE(s.contains_sphere(outer));
    }
}

TEST_CASE("Sphere intersection", "[math][sphere]") {
    Sphere s1(vec3::ZERO, 5.0f);
    Sphere s2(Vec3(8.0f, 0.0f, 0.0f), 5.0f);  // Touching
    Sphere s3(Vec3(20.0f, 0.0f, 0.0f), 5.0f); // Far away

    REQUIRE(s1.intersects_sphere(s2));
    REQUIRE_FALSE(s1.intersects_sphere(s3));
}

TEST_CASE("Sphere intersects_aabb", "[math][sphere]") {
    Sphere s(vec3::ZERO, 2.0f);
    AABB box1(Vec3(1.0f, 0.0f, 0.0f), Vec3(3.0f, 1.0f, 1.0f));  // Intersects
    AABB box2(Vec3(10.0f, 0.0f, 0.0f), Vec3(12.0f, 1.0f, 1.0f)); // Far away

    REQUIRE(s.intersects_aabb(box1));
    REQUIRE_FALSE(s.intersects_aabb(box2));
}

TEST_CASE("Sphere to_aabb", "[math][sphere]") {
    Sphere s(Vec3(1.0f, 2.0f, 3.0f), 5.0f);
    AABB box = s.to_aabb();

    REQUIRE(box.min == Vec3(-4.0f, -3.0f, -2.0f));
    REQUIRE(box.max == Vec3(6.0f, 7.0f, 8.0f));
}

// =============================================================================
// Plane Tests
// =============================================================================

TEST_CASE("Plane construction", "[math][plane]") {
    SECTION("from normal and distance") {
        Plane p(vec3::Y, -5.0f);
        REQUIRE_THAT(glm::length(p.normal), WithinAbs(1.0f, 1e-6f));
        REQUIRE(p.distance == -5.0f);
    }

    SECTION("from point and normal") {
        Plane p = Plane::from_point_normal(Vec3(0.0f, 5.0f, 0.0f), vec3::Y);
        REQUIRE_THAT(p.distance, WithinAbs(-5.0f, 1e-6f));
    }

    SECTION("from three points") {
        Plane p = Plane::from_points(
            Vec3(0.0f, 0.0f, 0.0f),
            Vec3(1.0f, 0.0f, 0.0f),
            Vec3(0.0f, 0.0f, 1.0f)
        );
        REQUIRE_THAT(p.normal.y, WithinAbs(1.0f, 1e-6f));  // Y-up plane
    }
}

TEST_CASE("Plane distance_to_point", "[math][plane]") {
    Plane p = Plane::from_point_normal(vec3::ZERO, vec3::Y);

    REQUIRE_THAT(p.distance_to_point(vec3::ZERO), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(p.distance_to_point(Vec3(0.0f, 5.0f, 0.0f)), WithinAbs(5.0f, 1e-6f));
    REQUIRE_THAT(p.distance_to_point(Vec3(0.0f, -3.0f, 0.0f)), WithinAbs(-3.0f, 1e-6f));
}

TEST_CASE("Plane is_in_front/is_behind", "[math][plane]") {
    Plane p = Plane::from_point_normal(vec3::ZERO, vec3::Y);

    REQUIRE(p.is_in_front(Vec3(0.0f, 1.0f, 0.0f)));
    REQUIRE(p.is_behind(Vec3(0.0f, -1.0f, 0.0f)));
}

TEST_CASE("Plane closest_point", "[math][plane]") {
    Plane p = Plane::from_point_normal(vec3::ZERO, vec3::Y);

    Vec3 point(5.0f, 10.0f, 3.0f);
    Vec3 closest = p.closest_point(point);
    REQUIRE_THAT(closest.y, WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(closest.x, WithinAbs(5.0f, 1e-6f));
    REQUIRE_THAT(closest.z, WithinAbs(3.0f, 1e-6f));
}

// =============================================================================
// FrustumPlanes Tests
// =============================================================================

TEST_CASE("FrustumPlanes from_view_projection", "[math][frustum]") {
    Mat4 view = look_at(Vec3(0.0f, 0.0f, 5.0f), vec3::ZERO, vec3::Y);
    Mat4 proj = perspective(radians(90.0f), 1.0f, 0.1f, 100.0f);
    Mat4 vp = proj * view;

    FrustumPlanes frustum = FrustumPlanes::from_view_projection(vp);

    // Point at camera position should be inside
    REQUIRE(frustum.contains_point(Vec3(0.0f, 0.0f, 4.9f)));

    // Point far behind camera should be outside
    REQUIRE_FALSE(frustum.contains_point(Vec3(0.0f, 0.0f, 200.0f)));
}

TEST_CASE("Frustum AABB test", "[math][frustum]") {
    Mat4 view = look_at(Vec3(0.0f, 0.0f, 10.0f), vec3::ZERO, vec3::Y);
    Mat4 proj = perspective(radians(90.0f), 1.0f, 0.1f, 100.0f);
    Mat4 vp = proj * view;

    FrustumPlanes frustum = FrustumPlanes::from_view_projection(vp);

    AABB visible(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));  // At origin
    AABB invisible(Vec3(1000.0f, 1000.0f, 1000.0f), Vec3(1001.0f, 1001.0f, 1001.0f));

    FrustumTestResult visible_result = test_aabb_frustum(visible, frustum);
    FrustumTestResult invisible_result = test_aabb_frustum(invisible, frustum);

    REQUIRE(is_visible(visible_result));
    REQUIRE_FALSE(is_visible(invisible_result));
}

TEST_CASE("Frustum Sphere test", "[math][frustum]") {
    Mat4 view = look_at(Vec3(0.0f, 0.0f, 10.0f), vec3::ZERO, vec3::Y);
    Mat4 proj = perspective(radians(90.0f), 1.0f, 0.1f, 100.0f);
    Mat4 vp = proj * view;

    FrustumPlanes frustum = FrustumPlanes::from_view_projection(vp);

    Sphere visible(vec3::ZERO, 2.0f);
    Sphere invisible(Vec3(1000.0f, 1000.0f, 1000.0f), 1.0f);

    FrustumTestResult visible_result = test_sphere_frustum(visible, frustum);
    FrustumTestResult invisible_result = test_sphere_frustum(invisible, frustum);

    REQUIRE(is_visible(visible_result));
    REQUIRE_FALSE(is_visible(invisible_result));
}
