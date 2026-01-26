#pragma once

/// @file bounds.hpp
/// @brief Bounding volume types for void_math
///
/// AABB, Sphere, and Frustum for spatial queries and culling.

#include "types.hpp"
#include "vec.hpp"
#include "mat.hpp"
#include "plane.hpp"
#include <array>
#include <cmath>
#include <algorithm>
#include <span>

namespace void_math {

// Forward declarations
struct Sphere;
struct AABB;

// =============================================================================
// AABB (Axis-Aligned Bounding Box)
// =============================================================================

/// Axis-Aligned Bounding Box
struct AABB {
    Vec3 min = Vec3(consts::MAX_FLOAT);   ///< Minimum corner
    Vec3 max = Vec3(-consts::MAX_FLOAT);  ///< Maximum corner

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr AABB() noexcept = default;

    /// Create from min and max corners
    constexpr AABB(const Vec3& min_point, const Vec3& max_point) noexcept
        : min(min_point), max(max_point) {}

    /// Create from center and half extents
    static AABB from_center_half_extents(const Vec3& center, const Vec3& half_extents) noexcept {
        return AABB(center - half_extents, center + half_extents);
    }

    /// Create from a list of points
    static AABB from_points(std::span<const Vec3> points) noexcept {
        AABB result;
        for (const auto& p : points) {
            result.expand_to_include(p);
        }
        return result;
    }

    /// Create an empty/invalid AABB
    static AABB empty() noexcept {
        return AABB();
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /// Get center point
    [[nodiscard]] Vec3 center() const noexcept {
        return (min + max) * 0.5f;
    }

    /// Get half extents (half the size along each axis)
    [[nodiscard]] Vec3 half_extents() const noexcept {
        return (max - min) * 0.5f;
    }

    /// Get full size along each axis
    [[nodiscard]] Vec3 size() const noexcept {
        return max - min;
    }

    /// Calculate volume
    [[nodiscard]] float volume() const noexcept {
        Vec3 s = size();
        return s.x * s.y * s.z;
    }

    /// Calculate surface area
    [[nodiscard]] float surface_area() const noexcept {
        Vec3 s = size();
        return 2.0f * (s.x * s.y + s.y * s.z + s.z * s.x);
    }

    /// Check if AABB is valid (min <= max for all components)
    [[nodiscard]] bool is_valid() const noexcept {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    /// Check if AABB is empty/inverted
    [[nodiscard]] bool is_empty() const noexcept {
        return !is_valid();
    }

    // =========================================================================
    // Expansion
    // =========================================================================

    /// Expand to include a point
    void expand_to_include(const Vec3& point) noexcept {
        min = void_math::min(min, point);
        max = void_math::max(max, point);
    }

    /// Expand to include another AABB
    void expand_to_include(const AABB& other) noexcept {
        min = void_math::min(min, other.min);
        max = void_math::max(max, other.max);
    }

    /// Create union of two AABBs
    [[nodiscard]] AABB union_with(const AABB& other) const noexcept {
        AABB result = *this;
        result.expand_to_include(other);
        return result;
    }

    /// Expand uniformly in all directions
    [[nodiscard]] AABB expanded(float amount) const noexcept {
        return AABB(min - Vec3(amount), max + Vec3(amount));
    }

    // =========================================================================
    // Containment Tests
    // =========================================================================

    /// Test if point is inside AABB
    [[nodiscard]] bool contains_point(const Vec3& point) const noexcept {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    /// Test if another AABB is completely contained
    [[nodiscard]] bool contains_aabb(const AABB& other) const noexcept {
        return other.min.x >= min.x && other.max.x <= max.x &&
               other.min.y >= min.y && other.max.y <= max.y &&
               other.min.z >= min.z && other.max.z <= max.z;
    }

    /// Test if another AABB intersects
    [[nodiscard]] bool intersects(const AABB& other) const noexcept {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    // =========================================================================
    // Distance Queries
    // =========================================================================

    /// Get closest point on AABB to given point
    [[nodiscard]] Vec3 closest_point(const Vec3& point) const noexcept {
        return glm::clamp(point, min, max);
    }

    /// Squared distance from point to AABB
    [[nodiscard]] float distance_squared_to_point(const Vec3& point) const noexcept {
        Vec3 closest = closest_point(point);
        return glm::length2(point - closest);
    }

    /// Distance from point to AABB
    [[nodiscard]] float distance_to_point(const Vec3& point) const noexcept {
        return std::sqrt(distance_squared_to_point(point));
    }

    // =========================================================================
    // Transformation
    // =========================================================================

    /// Transform AABB by matrix (result is still axis-aligned, so may be larger)
    [[nodiscard]] AABB transform(const Mat4& matrix) const noexcept {
        // Transform all 8 corners and build new AABB
        std::array<Vec3, 8> corners_arr = corners();
        AABB result;
        for (const auto& corner : corners_arr) {
            result.expand_to_include(transform_point(matrix, corner));
        }
        return result;
    }

    /// Get all 8 corner points
    [[nodiscard]] std::array<Vec3, 8> corners() const noexcept {
        return {{
            Vec3(min.x, min.y, min.z),
            Vec3(max.x, min.y, min.z),
            Vec3(min.x, max.y, min.z),
            Vec3(max.x, max.y, min.z),
            Vec3(min.x, min.y, max.z),
            Vec3(max.x, min.y, max.z),
            Vec3(min.x, max.y, max.z),
            Vec3(max.x, max.y, max.z)
        }};
    }

    bool operator==(const AABB& other) const noexcept {
        return min == other.min && max == other.max;
    }

    bool operator!=(const AABB& other) const noexcept {
        return !(*this == other);
    }
};

// =============================================================================
// Sphere (Bounding Sphere)
// =============================================================================

/// Bounding Sphere
struct Sphere {
    Vec3 center = vec3::ZERO;
    float radius = 0.0f;

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr Sphere() noexcept = default;

    constexpr Sphere(const Vec3& c, float r) noexcept
        : center(c), radius(r) {}

    /// Create bounding sphere from AABB
    static Sphere from_aabb(const AABB& aabb) noexcept {
        Vec3 c = aabb.center();
        float r = glm::length(aabb.half_extents());
        return Sphere(c, r);
    }

    /// Create bounding sphere from points (Ritter's algorithm)
    static Sphere from_points(std::span<const Vec3> points) noexcept {
        if (points.empty()) {
            return Sphere();
        }

        // Start with AABB-based sphere
        AABB aabb = AABB::from_points(points);
        Sphere sphere = from_aabb(aabb);

        // Expand to include all points (Ritter's second pass)
        for (const auto& p : points) {
            Vec3 to_point = p - sphere.center;
            float dist_sq = glm::length2(to_point);
            if (dist_sq > sphere.radius * sphere.radius) {
                float dist = std::sqrt(dist_sq);
                float new_radius = (sphere.radius + dist) * 0.5f;
                float k = (new_radius - sphere.radius) / dist;
                sphere.radius = new_radius;
                sphere.center += to_point * k;
            }
        }

        return sphere;
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /// Calculate volume
    [[nodiscard]] float volume() const noexcept {
        return (4.0f / 3.0f) * consts::PI * radius * radius * radius;
    }

    /// Calculate surface area
    [[nodiscard]] float surface_area() const noexcept {
        return 4.0f * consts::PI * radius * radius;
    }

    // =========================================================================
    // Containment Tests
    // =========================================================================

    /// Test if point is inside sphere
    [[nodiscard]] bool contains_point(const Vec3& point) const noexcept {
        return glm::length2(point - center) <= radius * radius;
    }

    /// Test if another sphere is completely contained
    [[nodiscard]] bool contains_sphere(const Sphere& other) const noexcept {
        float dist = glm::length(other.center - center);
        return dist + other.radius <= radius;
    }

    /// Test if another sphere intersects
    [[nodiscard]] bool intersects_sphere(const Sphere& other) const noexcept {
        float dist_sq = glm::length2(other.center - center);
        float radius_sum = radius + other.radius;
        return dist_sq <= radius_sum * radius_sum;
    }

    /// Test if AABB intersects
    [[nodiscard]] bool intersects_aabb(const AABB& aabb) const noexcept {
        return aabb.distance_squared_to_point(center) <= radius * radius;
    }

    // =========================================================================
    // Distance Queries
    // =========================================================================

    /// Get closest point on sphere surface to given point
    [[nodiscard]] Vec3 closest_point(const Vec3& point) const noexcept {
        Vec3 dir = point - center;
        float len = glm::length(dir);
        if (len < consts::EPSILON) {
            return center + Vec3(radius, 0.0f, 0.0f);
        }
        return center + (dir / len) * radius;
    }

    // =========================================================================
    // Conversion
    // =========================================================================

    /// Convert to bounding AABB
    [[nodiscard]] AABB to_aabb() const noexcept {
        return AABB(center - Vec3(radius), center + Vec3(radius));
    }

    /// Transform sphere by matrix (conservative for non-uniform scale)
    [[nodiscard]] Sphere transform(const Mat4& matrix) const noexcept {
        Vec3 new_center = transform_point(matrix, center);

        // For non-uniform scale, use maximum scale factor
        Vec3 scale_factors = get_scale(matrix);
        float max_scale = std::max({scale_factors.x, scale_factors.y, scale_factors.z});

        return Sphere(new_center, radius * max_scale);
    }

    bool operator==(const Sphere& other) const noexcept {
        return center == other.center && radius == other.radius;
    }

    bool operator!=(const Sphere& other) const noexcept {
        return !(*this == other);
    }
};

// =============================================================================
// Frustum (for quick bounds storage)
// =============================================================================

/// Simple frustum representation as 6 planes stored as Vec4 (normal.xyz, distance)
/// For more advanced culling, use FrustumPlanes
struct Frustum {
    std::array<Vec4, 6> planes;  ///< Planes as (normal.x, normal.y, normal.z, distance)

    /// Extract frustum from view-projection matrix
    static Frustum from_matrix(const Mat4& mvp) noexcept {
        Frustum f;
        FrustumPlanes fp = FrustumPlanes::from_view_projection(mvp);
        for (size_t i = 0; i < 6; ++i) {
            f.planes[i] = Vec4(fp.planes[i].normal, fp.planes[i].distance);
        }
        return f;
    }

    /// Test if point is inside frustum
    [[nodiscard]] bool contains_point(const Vec3& point) const noexcept {
        for (const auto& plane : planes) {
            if (glm::dot(Vec3(plane), point) + plane.w < 0.0f) {
                return false;
            }
        }
        return true;
    }

    /// Test if sphere intersects frustum
    [[nodiscard]] bool intersects_sphere(const Sphere& sphere) const noexcept {
        for (const auto& plane : planes) {
            float dist = glm::dot(Vec3(plane), sphere.center) + plane.w;
            if (dist < -sphere.radius) {
                return false;
            }
        }
        return true;
    }

    /// Test if AABB intersects frustum
    [[nodiscard]] bool intersects_aabb(const AABB& aabb) const noexcept {
        for (const auto& plane : planes) {
            Vec3 normal(plane);
            // Find the corner most in the direction of the plane normal
            Vec3 p_vertex(
                normal.x >= 0.0f ? aabb.max.x : aabb.min.x,
                normal.y >= 0.0f ? aabb.max.y : aabb.min.y,
                normal.z >= 0.0f ? aabb.max.z : aabb.min.z
            );
            if (glm::dot(normal, p_vertex) + plane.w < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

// =============================================================================
// Cross-type Operations
// =============================================================================

/// Test AABB against frustum planes with full result
[[nodiscard]] inline FrustumTestResult test_aabb_frustum(const AABB& aabb,
                                                          const FrustumPlanes& frustum) noexcept {
    bool all_inside = true;

    for (const auto& plane : frustum.planes) {
        // Find P (most positive) and N (most negative) vertices
        Vec3 p_vertex(
            plane.normal.x >= 0.0f ? aabb.max.x : aabb.min.x,
            plane.normal.y >= 0.0f ? aabb.max.y : aabb.min.y,
            plane.normal.z >= 0.0f ? aabb.max.z : aabb.min.z
        );
        Vec3 n_vertex(
            plane.normal.x >= 0.0f ? aabb.min.x : aabb.max.x,
            plane.normal.y >= 0.0f ? aabb.min.y : aabb.max.y,
            plane.normal.z >= 0.0f ? aabb.min.z : aabb.max.z
        );

        if (plane.distance_to_point(p_vertex) < 0.0f) {
            return FrustumTestResult::Outside;
        }
        if (plane.distance_to_point(n_vertex) < 0.0f) {
            all_inside = false;
        }
    }

    return all_inside ? FrustumTestResult::Inside : FrustumTestResult::Intersecting;
}

/// Test sphere against frustum planes with full result
[[nodiscard]] inline FrustumTestResult test_sphere_frustum(const Sphere& sphere,
                                                            const FrustumPlanes& frustum) noexcept {
    bool all_inside = true;

    for (const auto& plane : frustum.planes) {
        float dist = plane.distance_to_point(sphere.center);
        if (dist < -sphere.radius) {
            return FrustumTestResult::Outside;
        }
        if (dist < sphere.radius) {
            all_inside = false;
        }
    }

    return all_inside ? FrustumTestResult::Inside : FrustumTestResult::Intersecting;
}

// =============================================================================
// Free-Standing Helper Functions
// =============================================================================

/// Test if two AABBs intersect (free function for physics compatibility)
[[nodiscard]] inline bool intersects(const AABB& a, const AABB& b) noexcept {
    return a.intersects(b);
}

/// Test if AABB contains a point (free function for physics compatibility)
[[nodiscard]] inline bool contains(const AABB& aabb, const Vec3& point) noexcept {
    return aabb.contains_point(point);
}

/// Combine two AABBs (union, free function for physics compatibility)
[[nodiscard]] inline AABB combine(const AABB& a, const AABB& b) noexcept {
    return a.union_with(b);
}

/// Test if two spheres intersect (free function)
[[nodiscard]] inline bool intersects(const Sphere& a, const Sphere& b) noexcept {
    return a.intersects_sphere(b);
}

/// Test if sphere and AABB intersect (free function)
[[nodiscard]] inline bool intersects(const Sphere& sphere, const AABB& aabb) noexcept {
    return sphere.intersects_aabb(aabb);
}

/// Test if AABB and sphere intersect (free function)
[[nodiscard]] inline bool intersects(const AABB& aabb, const Sphere& sphere) noexcept {
    return sphere.intersects_aabb(aabb);
}

} // namespace void_math
