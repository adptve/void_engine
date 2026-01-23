#pragma once

/// @file ray.hpp
/// @brief Ray type for void_math
///
/// 3D ray for intersection testing and raycasting.

#include "types.hpp"
#include "vec.hpp"
#include "mat.hpp"
#include <cmath>

namespace void_math {

/// 3D Ray with origin and direction
struct Ray {
    Vec3 origin = vec3::ZERO;      ///< Ray origin point
    Vec3 direction = vec3::NEG_Z;  ///< Ray direction (should be normalized)

    // =========================================================================
    // Constants
    // =========================================================================

    /// Ray along +X axis from origin
    static const Ray& X_AXIS() noexcept {
        static const Ray ray{vec3::ZERO, vec3::X};
        return ray;
    }

    /// Ray along +Y axis from origin
    static const Ray& Y_AXIS() noexcept {
        static const Ray ray{vec3::ZERO, vec3::Y};
        return ray;
    }

    /// Ray along +Z axis from origin
    static const Ray& Z_AXIS() noexcept {
        static const Ray ray{vec3::ZERO, vec3::Z};
        return ray;
    }

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr Ray() noexcept = default;

    /// Create ray from origin and direction (direction will be normalized)
    Ray(const Vec3& orig, const Vec3& dir) noexcept
        : origin(orig), direction(glm::normalize(dir)) {}

    /// Create ray from two points
    static Ray from_points(const Vec3& start, const Vec3& end) noexcept {
        return Ray(start, end - start);
    }

    // =========================================================================
    // Point Evaluation
    // =========================================================================

    /// Get point at distance t along ray
    [[nodiscard]] Vec3 at(float t) const noexcept {
        return origin + direction * t;
    }

    /// Alias for at()
    [[nodiscard]] Vec3 point_at(float t) const noexcept {
        return at(t);
    }

    // =========================================================================
    // Distance Queries
    // =========================================================================

    /// Get closest point on ray to given point
    [[nodiscard]] Vec3 closest_point(const Vec3& point) const noexcept {
        float t = std::max(0.0f, glm::dot(point - origin, direction));
        return at(t);
    }

    /// Distance from ray to point
    [[nodiscard]] float distance_to_point(const Vec3& point) const noexcept {
        return glm::length(point - closest_point(point));
    }

    /// Squared distance from ray to point
    [[nodiscard]] float distance_squared_to_point(const Vec3& point) const noexcept {
        return glm::length2(point - closest_point(point));
    }

    // =========================================================================
    // Transformation
    // =========================================================================

    /// Transform ray by matrix
    [[nodiscard]] Ray transform(const Mat4& matrix) const noexcept {
        Vec3 new_origin = transform_point(matrix, origin);
        Vec3 new_direction = glm::normalize(transform_vector(matrix, direction));
        Ray result;
        result.origin = new_origin;
        result.direction = new_direction;
        return result;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /// Compute inverse direction (useful for optimized AABB tests)
    [[nodiscard]] Vec3 inverse_direction() const noexcept {
        return Vec3(
            1.0f / direction.x,
            1.0f / direction.y,
            1.0f / direction.z
        );
    }

    /// Check if ray is valid (non-zero direction)
    [[nodiscard]] bool is_valid() const noexcept {
        return glm::length2(direction) > consts::EPSILON * consts::EPSILON;
    }

    bool operator==(const Ray& other) const noexcept {
        return origin == other.origin && direction == other.direction;
    }

    bool operator!=(const Ray& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace void_math
