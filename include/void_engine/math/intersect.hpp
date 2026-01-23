#pragma once

/// @file intersect.hpp
/// @brief Intersection testing functions for void_math
///
/// Ray-primitive intersection tests and related utilities.

#include "types.hpp"
#include "vec.hpp"
#include "ray.hpp"
#include "bounds.hpp"
#include "plane.hpp"
#include <optional>
#include <cmath>
#include <array>

namespace void_math {

// =============================================================================
// Intersection Result Types
// =============================================================================

/// Result of ray-triangle intersection
struct TriangleHit {
    float distance;                    ///< Distance along ray to hit point
    std::array<float, 3> barycentric;  ///< Barycentric coordinates [w, u, v]

    /// Get hit point from ray
    [[nodiscard]] Vec3 point(const Ray& ray) const noexcept {
        return ray.at(distance);
    }
};

// =============================================================================
// Ray-AABB Intersection (Slab Method)
// =============================================================================

/// Ray-AABB intersection test
/// @return Distance to hit point, or nullopt if no intersection
[[nodiscard]] inline std::optional<float> ray_aabb(const Ray& ray, const AABB& aabb) noexcept {
    Vec3 inv_dir = ray.inverse_direction();

    float t1 = (aabb.min.x - ray.origin.x) * inv_dir.x;
    float t2 = (aabb.max.x - ray.origin.x) * inv_dir.x;
    float t3 = (aabb.min.y - ray.origin.y) * inv_dir.y;
    float t4 = (aabb.max.y - ray.origin.y) * inv_dir.y;
    float t5 = (aabb.min.z - ray.origin.z) * inv_dir.z;
    float t6 = (aabb.max.z - ray.origin.z) * inv_dir.z;

    float tmin = std::max({std::min(t1, t2), std::min(t3, t4), std::min(t5, t6)});
    float tmax = std::min({std::max(t1, t2), std::max(t3, t4), std::max(t5, t6)});

    // Ray is intersecting AABB, but whole AABB is behind us
    if (tmax < 0.0f) {
        return std::nullopt;
    }

    // Ray doesn't intersect AABB
    if (tmin > tmax) {
        return std::nullopt;
    }

    // Return nearest intersection (could be inside AABB if tmin < 0)
    return tmin >= 0.0f ? tmin : tmax;
}

/// Ray-AABB intersection with normal
/// @return Pair of (distance, normal), or nullopt if no intersection
[[nodiscard]] inline std::optional<std::pair<float, Vec3>> ray_aabb_with_normal(
    const Ray& ray, const AABB& aabb) noexcept {

    Vec3 inv_dir = ray.inverse_direction();

    float t1 = (aabb.min.x - ray.origin.x) * inv_dir.x;
    float t2 = (aabb.max.x - ray.origin.x) * inv_dir.x;
    float t3 = (aabb.min.y - ray.origin.y) * inv_dir.y;
    float t4 = (aabb.max.y - ray.origin.y) * inv_dir.y;
    float t5 = (aabb.min.z - ray.origin.z) * inv_dir.z;
    float t6 = (aabb.max.z - ray.origin.z) * inv_dir.z;

    int face = 0;
    float tmin = std::min(t1, t2);
    if (std::min(t3, t4) > tmin) { tmin = std::min(t3, t4); face = 1; }
    if (std::min(t5, t6) > tmin) { tmin = std::min(t5, t6); face = 2; }

    float tmax = std::min({std::max(t1, t2), std::max(t3, t4), std::max(t5, t6)});

    if (tmax < 0.0f || tmin > tmax) {
        return std::nullopt;
    }

    float t = tmin >= 0.0f ? tmin : tmax;

    // Determine normal based on which face was hit
    Vec3 normal;
    switch (face) {
        case 0: normal = ray.direction.x > 0.0f ? vec3::NEG_X : vec3::X; break;
        case 1: normal = ray.direction.y > 0.0f ? vec3::NEG_Y : vec3::Y; break;
        case 2: normal = ray.direction.z > 0.0f ? vec3::NEG_Z : vec3::Z; break;
        default: normal = vec3::Y; break;
    }

    return std::make_pair(t, normal);
}

// =============================================================================
// Ray-Sphere Intersection
// =============================================================================

/// Ray-Sphere intersection test
/// @return Distance to nearest hit point, or nullopt if no intersection
[[nodiscard]] inline std::optional<float> ray_sphere(const Ray& ray, const Sphere& sphere) noexcept {
    Vec3 oc = ray.origin - sphere.center;
    float b = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - sphere.radius * sphere.radius;

    float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    float sqrt_d = std::sqrt(discriminant);
    float t1 = -b - sqrt_d;
    float t2 = -b + sqrt_d;

    if (t1 >= 0.0f) {
        return t1;
    }
    if (t2 >= 0.0f) {
        return t2;
    }
    return std::nullopt;
}

/// Ray-Sphere intersection at specific center and radius
[[nodiscard]] inline std::optional<float> ray_sphere_at(
    const Ray& ray, const Vec3& center, float radius) noexcept {
    return ray_sphere(ray, Sphere(center, radius));
}

/// Ray-Sphere intersection with normal
[[nodiscard]] inline std::optional<std::pair<float, Vec3>> ray_sphere_with_normal(
    const Ray& ray, const Sphere& sphere) noexcept {

    auto result = ray_sphere(ray, sphere);
    if (!result) {
        return std::nullopt;
    }

    Vec3 hit_point = ray.at(*result);
    Vec3 normal = glm::normalize(hit_point - sphere.center);
    return std::make_pair(*result, normal);
}

// =============================================================================
// Ray-Triangle Intersection (Möller-Trumbore Algorithm)
// =============================================================================

/// Ray-Triangle intersection test
/// @param ray The ray to test
/// @param v0, v1, v2 Triangle vertices (counter-clockwise winding)
/// @param cull_backface If true, only front-facing triangles are hit
/// @return TriangleHit with distance and barycentric coords, or nullopt
[[nodiscard]] inline std::optional<TriangleHit> ray_triangle(
    const Ray& ray,
    const Vec3& v0, const Vec3& v1, const Vec3& v2,
    bool cull_backface = true) noexcept {

    const float EPSILON = 1e-8f;

    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    if (cull_backface) {
        if (a < EPSILON) {
            return std::nullopt;  // Backface or parallel
        }
    } else {
        if (std::abs(a) < EPSILON) {
            return std::nullopt;  // Parallel
        }
    }

    float f = 1.0f / a;
    Vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    Vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }

    float t = f * glm::dot(edge2, q);

    if (t < EPSILON) {
        return std::nullopt;  // Behind ray origin
    }

    float w = 1.0f - u - v;
    return TriangleHit{t, {w, u, v}};
}

// =============================================================================
// Ray-Plane Intersection
// =============================================================================

/// Ray-Plane intersection test
/// @param ray The ray to test
/// @param plane_point A point on the plane
/// @param plane_normal The plane normal (should be normalized)
/// @return Distance to hit point, or nullopt if parallel
[[nodiscard]] inline std::optional<float> ray_plane(
    const Ray& ray,
    const Vec3& plane_point,
    const Vec3& plane_normal) noexcept {

    float denom = glm::dot(plane_normal, ray.direction);
    if (std::abs(denom) < consts::EPSILON) {
        return std::nullopt;  // Parallel to plane
    }

    float t = glm::dot(plane_point - ray.origin, plane_normal) / denom;
    if (t < 0.0f) {
        return std::nullopt;  // Behind ray
    }

    return t;
}

/// Ray-Plane intersection test using Plane struct
[[nodiscard]] inline std::optional<float> ray_plane(
    const Ray& ray, const Plane& plane) noexcept {

    float denom = glm::dot(plane.normal, ray.direction);
    if (std::abs(denom) < consts::EPSILON) {
        return std::nullopt;
    }

    float t = -(glm::dot(plane.normal, ray.origin) + plane.distance) / denom;
    if (t < 0.0f) {
        return std::nullopt;
    }

    return t;
}

// =============================================================================
// Ray-Disk Intersection
// =============================================================================

/// Ray-Disk intersection test
/// @param ray The ray to test
/// @param center Disk center
/// @param normal Disk normal (should be normalized)
/// @param radius Disk radius
/// @return Distance to hit point, or nullopt if no intersection
[[nodiscard]] inline std::optional<float> ray_disk(
    const Ray& ray,
    const Vec3& center,
    const Vec3& normal,
    float radius) noexcept {

    auto plane_hit = ray_plane(ray, center, normal);
    if (!plane_hit) {
        return std::nullopt;
    }

    Vec3 hit_point = ray.at(*plane_hit);
    float dist_sq = glm::length2(hit_point - center);
    if (dist_sq > radius * radius) {
        return std::nullopt;
    }

    return plane_hit;
}

// =============================================================================
// Ray-Capsule Intersection
// =============================================================================

/// Ray-Capsule intersection test
/// @param ray The ray to test
/// @param a First capsule endpoint
/// @param b Second capsule endpoint
/// @param radius Capsule radius
/// @return Distance to hit point, or nullopt if no intersection
[[nodiscard]] inline std::optional<float> ray_capsule(
    const Ray& ray,
    const Vec3& a,
    const Vec3& b,
    float radius) noexcept {

    Vec3 ab = b - a;
    Vec3 ao = ray.origin - a;

    float ab_dot_d = glm::dot(ab, ray.direction);
    float ab_dot_ao = glm::dot(ab, ao);
    float ab_dot_ab = glm::dot(ab, ab);

    float m = ab_dot_d / ab_dot_ab;
    float n = ab_dot_ao / ab_dot_ab;

    Vec3 q = ray.direction - ab * m;
    Vec3 r = ao - ab * n;

    float qa = glm::dot(q, q);
    float qb = 2.0f * glm::dot(q, r);
    float qc = glm::dot(r, r) - radius * radius;

    if (qa < consts::EPSILON) {
        // Ray is parallel to capsule axis, check spheres at endpoints
        auto hit_a = ray_sphere_at(ray, a, radius);
        auto hit_b = ray_sphere_at(ray, b, radius);

        if (hit_a && hit_b) {
            return std::min(*hit_a, *hit_b);
        }
        return hit_a ? hit_a : hit_b;
    }

    float discriminant = qb * qb - 4.0f * qa * qc;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    float sqrt_d = std::sqrt(discriminant);
    float t1 = (-qb - sqrt_d) / (2.0f * qa);
    float t2 = (-qb + sqrt_d) / (2.0f * qa);

    auto check_hit = [&](float t) -> std::optional<float> {
        if (t < 0.0f) return std::nullopt;

        float s = m * t + n;
        if (s >= 0.0f && s <= 1.0f) {
            return t;  // Hit the cylinder part
        } else if (s < 0.0f) {
            return ray_sphere_at(ray, a, radius);  // Hit sphere at a
        } else {
            return ray_sphere_at(ray, b, radius);  // Hit sphere at b
        }
    };

    auto hit1 = check_hit(t1);
    auto hit2 = check_hit(t2);

    if (hit1 && hit2) {
        return std::min(*hit1, *hit2);
    }
    return hit1 ? hit1 : hit2;
}

// =============================================================================
// Interpolation Utilities
// =============================================================================

/// Interpolate normal using barycentric coordinates
/// @param n0, n1, n2 Vertex normals
/// @param bary Barycentric coordinates [w, u, v]
/// @return Interpolated and normalized normal
[[nodiscard]] inline Vec3 interpolate_normal(
    const Vec3& n0, const Vec3& n1, const Vec3& n2,
    const std::array<float, 3>& bary) noexcept {
    return glm::normalize(n0 * bary[0] + n1 * bary[1] + n2 * bary[2]);
}

/// Interpolate UV coordinates using barycentric coordinates
/// @param uv0, uv1, uv2 Vertex UV coordinates
/// @param bary Barycentric coordinates [w, u, v]
/// @return Interpolated UV coordinates
[[nodiscard]] inline std::array<float, 2> interpolate_uv(
    const std::array<float, 2>& uv0,
    const std::array<float, 2>& uv1,
    const std::array<float, 2>& uv2,
    const std::array<float, 3>& bary) noexcept {
    return {
        uv0[0] * bary[0] + uv1[0] * bary[1] + uv2[0] * bary[2],
        uv0[1] * bary[0] + uv1[1] * bary[1] + uv2[1] * bary[2]
    };
}

/// Interpolate Vec2 UV coordinates using barycentric coordinates
[[nodiscard]] inline Vec2 interpolate_uv(
    const Vec2& uv0, const Vec2& uv1, const Vec2& uv2,
    const std::array<float, 3>& bary) noexcept {
    return uv0 * bary[0] + uv1 * bary[1] + uv2 * bary[2];
}

} // namespace void_math
