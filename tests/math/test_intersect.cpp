// void_math ray and intersection tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/math/math.hpp>

using namespace void_math;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Ray Tests
// =============================================================================

TEST_CASE("Ray construction", "[math][ray]") {
    SECTION("from origin and direction") {
        Ray r(Vec3(1.0f, 2.0f, 3.0f), Vec3(0.0f, 1.0f, 0.0f));
        REQUIRE(r.origin == Vec3(1.0f, 2.0f, 3.0f));
        REQUIRE_THAT(glm::length(r.direction), WithinAbs(1.0f, 1e-6f));
    }

    SECTION("from two points") {
        Ray r = Ray::from_points(vec3::ZERO, Vec3(10.0f, 0.0f, 0.0f));
        REQUIRE(r.origin == vec3::ZERO);
        REQUIRE_THAT(r.direction.x, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("axis constants") {
        REQUIRE(Ray::X_AXIS().direction == vec3::X);
        REQUIRE(Ray::Y_AXIS().direction == vec3::Y);
        REQUIRE(Ray::Z_AXIS().direction == vec3::Z);
    }
}

TEST_CASE("Ray at", "[math][ray]") {
    Ray r(vec3::ZERO, vec3::X);

    REQUIRE(r.at(0.0f) == vec3::ZERO);
    REQUIRE(r.at(5.0f) == Vec3(5.0f, 0.0f, 0.0f));
    REQUIRE(r.at(10.0f) == Vec3(10.0f, 0.0f, 0.0f));
}

TEST_CASE("Ray closest_point", "[math][ray]") {
    Ray r(vec3::ZERO, vec3::X);

    SECTION("point on ray") {
        Vec3 closest = r.closest_point(Vec3(5.0f, 0.0f, 0.0f));
        REQUIRE(closest == Vec3(5.0f, 0.0f, 0.0f));
    }

    SECTION("point off ray") {
        Vec3 closest = r.closest_point(Vec3(5.0f, 3.0f, 0.0f));
        REQUIRE(closest == Vec3(5.0f, 0.0f, 0.0f));
    }

    SECTION("point behind ray") {
        Vec3 closest = r.closest_point(Vec3(-5.0f, 3.0f, 0.0f));
        REQUIRE(closest == vec3::ZERO);  // Clamped to origin
    }
}

TEST_CASE("Ray distance_to_point", "[math][ray]") {
    Ray r(vec3::ZERO, vec3::X);

    REQUIRE_THAT(r.distance_to_point(Vec3(5.0f, 0.0f, 0.0f)), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(r.distance_to_point(Vec3(5.0f, 3.0f, 0.0f)), WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(r.distance_to_point(Vec3(5.0f, 3.0f, 4.0f)), WithinAbs(5.0f, 1e-6f));
}

TEST_CASE("Ray transform", "[math][ray]") {
    Ray r(vec3::ZERO, vec3::X);
    Mat4 t = translation(Vec3(5.0f, 0.0f, 0.0f));

    Ray transformed = r.transform(t);
    REQUIRE(transformed.origin == Vec3(5.0f, 0.0f, 0.0f));
    REQUIRE_THAT(transformed.direction.x, WithinAbs(1.0f, 1e-6f));
}

// =============================================================================
// Ray-AABB Intersection Tests
// =============================================================================

TEST_CASE("ray_aabb intersection", "[math][intersect]") {
    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    SECTION("ray hits box") {
        Ray r(Vec3(0.0f, 0.0f, -5.0f), vec3::Z);
        auto hit = ray_aabb(r, box);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(4.0f, 1e-6f));
    }

    SECTION("ray misses box") {
        Ray r(Vec3(5.0f, 5.0f, -5.0f), vec3::Z);
        auto hit = ray_aabb(r, box);
        REQUIRE_FALSE(hit.has_value());
    }

    SECTION("ray inside box") {
        Ray r(vec3::ZERO, vec3::X);
        auto hit = ray_aabb(r, box);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(1.0f, 1e-6f));  // Distance to exit
    }

    SECTION("ray behind box") {
        Ray r(Vec3(0.0f, 0.0f, 5.0f), vec3::Z);
        auto hit = ray_aabb(r, box);
        REQUIRE_FALSE(hit.has_value());
    }
}

TEST_CASE("ray_aabb_with_normal", "[math][intersect]") {
    AABB box(Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f));

    Ray r(Vec3(0.0f, 0.0f, -5.0f), vec3::Z);
    auto hit = ray_aabb_with_normal(r, box);

    REQUIRE(hit.has_value());
    auto [distance, normal] = *hit;
    REQUIRE_THAT(distance, WithinAbs(4.0f, 1e-6f));
    REQUIRE_THAT(normal.z, WithinAbs(-1.0f, 1e-6f));  // Hit -Z face
}

// =============================================================================
// Ray-Sphere Intersection Tests
// =============================================================================

TEST_CASE("ray_sphere intersection", "[math][intersect]") {
    Sphere sphere(vec3::ZERO, 2.0f);

    SECTION("ray hits sphere") {
        Ray r(Vec3(0.0f, 0.0f, -5.0f), vec3::Z);
        auto hit = ray_sphere(r, sphere);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(3.0f, 1e-6f));  // 5 - 2 = 3
    }

    SECTION("ray misses sphere") {
        Ray r(Vec3(5.0f, 0.0f, -5.0f), vec3::Z);
        auto hit = ray_sphere(r, sphere);
        REQUIRE_FALSE(hit.has_value());
    }

    SECTION("ray inside sphere") {
        Ray r(vec3::ZERO, vec3::X);
        auto hit = ray_sphere(r, sphere);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(2.0f, 1e-6f));  // Distance to exit
    }
}

TEST_CASE("ray_sphere_with_normal", "[math][intersect]") {
    Sphere sphere(vec3::ZERO, 2.0f);
    Ray r(Vec3(0.0f, 0.0f, -5.0f), vec3::Z);

    auto hit = ray_sphere_with_normal(r, sphere);
    REQUIRE(hit.has_value());
    auto [distance, normal] = *hit;

    REQUIRE_THAT(distance, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(normal.z, WithinAbs(-1.0f, 1e-6f));  // Normal points toward ray
}

// =============================================================================
// Ray-Triangle Intersection Tests
// =============================================================================

TEST_CASE("ray_triangle intersection", "[math][intersect]") {
    Vec3 v0(0.0f, 0.0f, 0.0f);
    Vec3 v1(2.0f, 0.0f, 0.0f);
    Vec3 v2(1.0f, 2.0f, 0.0f);

    SECTION("ray hits triangle") {
        Ray r(Vec3(1.0f, 0.5f, -5.0f), vec3::Z);
        auto hit = ray_triangle(r, v0, v1, v2, false);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(hit->distance, WithinAbs(5.0f, 1e-6f));

        // Check barycentric coordinates sum to 1
        float bary_sum = hit->barycentric[0] + hit->barycentric[1] + hit->barycentric[2];
        REQUIRE_THAT(bary_sum, WithinAbs(1.0f, 1e-6f));
    }

    SECTION("ray misses triangle") {
        Ray r(Vec3(5.0f, 5.0f, -5.0f), vec3::Z);
        auto hit = ray_triangle(r, v0, v1, v2, false);
        REQUIRE_FALSE(hit.has_value());
    }

    SECTION("backface culling") {
        Ray r(Vec3(1.0f, 0.5f, 5.0f), vec3::NEG_Z);  // From behind

        auto hit_culled = ray_triangle(r, v0, v1, v2, true);
        auto hit_not_culled = ray_triangle(r, v0, v1, v2, false);

        REQUIRE_FALSE(hit_culled.has_value());
        REQUIRE(hit_not_culled.has_value());
    }
}

// =============================================================================
// Ray-Plane Intersection Tests
// =============================================================================

TEST_CASE("ray_plane intersection", "[math][intersect]") {
    SECTION("ray hits plane") {
        Ray r(Vec3(0.0f, 5.0f, 0.0f), vec3::NEG_Y);
        auto hit = ray_plane(r, vec3::ZERO, vec3::Y);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(5.0f, 1e-6f));
    }

    SECTION("ray parallel to plane") {
        Ray r(Vec3(0.0f, 5.0f, 0.0f), vec3::X);
        auto hit = ray_plane(r, vec3::ZERO, vec3::Y);
        REQUIRE_FALSE(hit.has_value());
    }

    SECTION("ray pointing away from plane") {
        Ray r(Vec3(0.0f, 5.0f, 0.0f), vec3::Y);
        auto hit = ray_plane(r, vec3::ZERO, vec3::Y);
        REQUIRE_FALSE(hit.has_value());
    }
}

TEST_CASE("ray_plane with Plane struct", "[math][intersect]") {
    Plane p = Plane::from_point_normal(vec3::ZERO, vec3::Y);
    Ray r(Vec3(0.0f, 10.0f, 0.0f), vec3::NEG_Y);

    auto hit = ray_plane(r, p);
    REQUIRE(hit.has_value());
    REQUIRE_THAT(*hit, WithinAbs(10.0f, 1e-6f));
}

// =============================================================================
// Ray-Disk Intersection Tests
// =============================================================================

TEST_CASE("ray_disk intersection", "[math][intersect]") {
    SECTION("ray hits disk") {
        Ray r(Vec3(0.0f, 5.0f, 0.0f), vec3::NEG_Y);
        auto hit = ray_disk(r, vec3::ZERO, vec3::Y, 2.0f);
        REQUIRE(hit.has_value());
    }

    SECTION("ray hits plane but misses disk") {
        Ray r(Vec3(5.0f, 5.0f, 0.0f), vec3::NEG_Y);
        auto hit = ray_disk(r, vec3::ZERO, vec3::Y, 2.0f);
        REQUIRE_FALSE(hit.has_value());
    }
}

// =============================================================================
// Ray-Capsule Intersection Tests
// =============================================================================

TEST_CASE("ray_capsule intersection", "[math][intersect]") {
    Vec3 a(0.0f, 0.0f, 0.0f);
    Vec3 b(0.0f, 5.0f, 0.0f);
    float radius = 1.0f;

    SECTION("ray hits capsule cylinder") {
        Ray r(Vec3(-5.0f, 2.5f, 0.0f), vec3::X);
        auto hit = ray_capsule(r, a, b, radius);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(4.0f, 1e-6f));
    }

    SECTION("ray hits capsule sphere cap") {
        Ray r(Vec3(0.0f, -5.0f, 0.0f), vec3::Y);
        auto hit = ray_capsule(r, a, b, radius);
        REQUIRE(hit.has_value());
        REQUIRE_THAT(*hit, WithinAbs(4.0f, 1e-6f));
    }

    SECTION("ray misses capsule") {
        Ray r(Vec3(5.0f, 2.5f, 0.0f), vec3::Z);
        auto hit = ray_capsule(r, a, b, radius);
        REQUIRE_FALSE(hit.has_value());
    }
}

// =============================================================================
// Interpolation Utility Tests
// =============================================================================

TEST_CASE("interpolate_normal", "[math][intersect]") {
    Vec3 n0 = glm::normalize(Vec3(1.0f, 0.0f, 0.0f));
    Vec3 n1 = glm::normalize(Vec3(0.0f, 1.0f, 0.0f));
    Vec3 n2 = glm::normalize(Vec3(0.0f, 0.0f, 1.0f));

    // Barycentric coords at vertex 0
    std::array<float, 3> bary0 = {1.0f, 0.0f, 0.0f};
    Vec3 result0 = interpolate_normal(n0, n1, n2, bary0);
    REQUIRE_THAT(glm::dot(result0, n0), WithinAbs(1.0f, 1e-6f));

    // Center of triangle
    std::array<float, 3> bary_center = {1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f};
    Vec3 result_center = interpolate_normal(n0, n1, n2, bary_center);
    REQUIRE_THAT(glm::length(result_center), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("interpolate_uv", "[math][intersect]") {
    std::array<float, 2> uv0 = {0.0f, 0.0f};
    std::array<float, 2> uv1 = {1.0f, 0.0f};
    std::array<float, 2> uv2 = {0.5f, 1.0f};

    std::array<float, 3> bary = {1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f};
    auto result = interpolate_uv(uv0, uv1, uv2, bary);

    REQUIRE_THAT(result[0], WithinAbs(0.5f, 1e-6f));
    REQUIRE_THAT(result[1], WithinAbs(1.0f/3.0f, 1e-6f));
}
