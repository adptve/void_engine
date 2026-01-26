#pragma once

/// @file plane.hpp
/// @brief Plane and frustum plane types for void_math
///
/// 3D plane representation and frustum culling structures.

#include "types.hpp"
#include "vec.hpp"
#include <array>
#include <cmath>

namespace void_math {

// =============================================================================
// Plane
// =============================================================================

/// 3D Plane represented by equation ax + by + cz + d = 0
/// where (a, b, c) is the normal and d is the signed distance from origin
struct Plane {
    Vec3 normal = vec3::Y;   ///< Plane normal (should be normalized)
    float distance = 0.0f;    ///< Signed distance from origin along normal

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr Plane() noexcept = default;

    /// Create plane from normal and distance
    /// @param n Plane normal (will be normalized)
    /// @param d Distance from origin
    Plane(const Vec3& n, float d) noexcept
        : normal(glm::normalize(n)), distance(d) {}

    /// Create plane from point on plane and normal
    static Plane from_point_normal(const Vec3& point, const Vec3& n) noexcept {
        Vec3 normalized = glm::normalize(n);
        return Plane(normalized, -glm::dot(normalized, point));
    }

    /// Create plane from three points (counter-clockwise winding)
    static Plane from_points(const Vec3& p0, const Vec3& p1, const Vec3& p2) noexcept {
        Vec3 edge1 = p1 - p0;
        Vec3 edge2 = p2 - p0;
        Vec3 n = glm::normalize(glm::cross(edge1, edge2));
        return Plane(n, -glm::dot(n, p0));
    }

    // =========================================================================
    // Operations
    // =========================================================================

    /// Signed distance from point to plane
    /// Positive = in front of plane, Negative = behind plane
    [[nodiscard]] float distance_to_point(const Vec3& point) const noexcept {
        return glm::dot(normal, point) + distance;
    }

    /// Check if point is in front of plane
    [[nodiscard]] bool is_in_front(const Vec3& point) const noexcept {
        return distance_to_point(point) > 0.0f;
    }

    /// Check if point is behind plane
    [[nodiscard]] bool is_behind(const Vec3& point) const noexcept {
        return distance_to_point(point) < 0.0f;
    }

    /// Get closest point on plane to given point
    [[nodiscard]] Vec3 closest_point(const Vec3& point) const noexcept {
        return point - normal * distance_to_point(point);
    }

    /// Project point onto plane (alias for closest_point)
    [[nodiscard]] Vec3 project_point(const Vec3& point) const noexcept {
        return closest_point(point);
    }

    /// Normalize the plane (in case it was modified)
    void normalize() noexcept {
        float len = glm::length(normal);
        if (len > consts::EPSILON) {
            normal /= len;
            distance /= len;
        }
    }

    bool operator==(const Plane& other) const noexcept {
        return normal == other.normal && distance == other.distance;
    }

    bool operator!=(const Plane& other) const noexcept {
        return !(*this == other);
    }
};

// =============================================================================
// Frustum Test Result
// =============================================================================

/// Result of frustum containment test
enum class FrustumTestResult {
    Inside,       ///< Completely inside frustum
    Outside,      ///< Completely outside frustum
    Intersecting  ///< Crosses frustum boundary
};

/// Check if result indicates visibility
[[nodiscard]] inline bool is_visible(FrustumTestResult result) noexcept {
    return result != FrustumTestResult::Outside;
}

/// Check if result indicates full containment
[[nodiscard]] inline bool is_inside(FrustumTestResult result) noexcept {
    return result == FrustumTestResult::Inside;
}

// =============================================================================
// Frustum Planes
// =============================================================================

/// Six-plane view frustum for culling
/// Planes are stored in order: Left, Right, Bottom, Top, Near, Far
struct FrustumPlanes {
    static constexpr size_t LEFT   = 0;
    static constexpr size_t RIGHT  = 1;
    static constexpr size_t BOTTOM = 2;
    static constexpr size_t TOP    = 3;
    static constexpr size_t Z_NEAR = 4;  // Renamed from NEAR to avoid Windows macro conflict
    static constexpr size_t Z_FAR  = 5;  // Renamed from FAR to avoid Windows macro conflict

    std::array<Plane, 6> planes;

    // =========================================================================
    // Constructors
    // =========================================================================

    FrustumPlanes() noexcept = default;

    /// Extract frustum planes from view-projection matrix (Gribb/Hartmann method)
    static FrustumPlanes from_view_projection(const Mat4& vp) noexcept {
        FrustumPlanes frustum;

        // Left plane: row3 + row0
        frustum.planes[LEFT] = Plane(
            Vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]),
            vp[3][3] + vp[3][0]
        );

        // Right plane: row3 - row0
        frustum.planes[RIGHT] = Plane(
            Vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]),
            vp[3][3] - vp[3][0]
        );

        // Bottom plane: row3 + row1
        frustum.planes[BOTTOM] = Plane(
            Vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]),
            vp[3][3] + vp[3][1]
        );

        // Top plane: row3 - row1
        frustum.planes[TOP] = Plane(
            Vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]),
            vp[3][3] - vp[3][1]
        );

        // Near plane: row3 + row2 (for depth [0,1])
        frustum.planes[Z_NEAR] = Plane(
            Vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]),
            vp[3][3] + vp[3][2]
        );

        // Far plane: row3 - row2
        frustum.planes[Z_FAR] = Plane(
            Vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]),
            vp[3][3] - vp[3][2]
        );

        // Normalize all planes
        for (auto& plane : frustum.planes) {
            plane.normalize();
        }

        return frustum;
    }

    // =========================================================================
    // Containment Tests
    // =========================================================================

    /// Test if point is inside frustum
    [[nodiscard]] bool contains_point(const Vec3& point) const noexcept {
        for (const auto& plane : planes) {
            if (plane.distance_to_point(point) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    /// Access plane by index
    [[nodiscard]] const Plane& operator[](size_t index) const noexcept {
        return planes[index];
    }

    [[nodiscard]] Plane& operator[](size_t index) noexcept {
        return planes[index];
    }
};

} // namespace void_math
