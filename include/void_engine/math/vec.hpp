#pragma once

/// @file vec.hpp
/// @brief Vector utility functions for void_math
///
/// Provides additional vector operations that extend GLM functionality
/// to match the Rust void_math API.

#include "types.hpp"
#include <algorithm>
#include <cmath>
#include <array>

namespace void_math {

// =============================================================================
// Core Vector Operations (GLM wrappers)
// =============================================================================

/// Normalize a vector
template<typename T>
[[nodiscard]] inline T normalize(const T& v) noexcept {
    return glm::normalize(v);
}

/// Dot product of two vectors
template<typename T>
[[nodiscard]] inline auto dot(const T& a, const T& b) noexcept {
    return glm::dot(a, b);
}

/// Cross product of two Vec3 vectors
[[nodiscard]] inline Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return glm::cross(a, b);
}

/// Length of a vector
template<typename T>
[[nodiscard]] inline float length(const T& v) noexcept {
    return glm::length(v);
}

/// Squared length of a vector
template<typename T>
[[nodiscard]] inline float length_squared(const T& v) noexcept {
    return glm::length2(v);
}

// =============================================================================
// Vec2 Utilities
// =============================================================================

/// Create a Vec2 with all components equal to v
[[nodiscard]] inline Vec2 splat2(float v) noexcept {
    return Vec2(v, v);
}

/// Get perpendicular vector (rotated 90 degrees counter-clockwise)
[[nodiscard]] inline Vec2 perpendicular(const Vec2& v) noexcept {
    return Vec2(-v.y, v.x);
}

/// Normalize vector, returning zero if length is too small
[[nodiscard]] inline Vec2 normalize_or_zero(const Vec2& v) noexcept {
    const float len_sq = glm::length2(v);
    if (len_sq < consts::EPSILON * consts::EPSILON) {
        return vec2::ZERO;
    }
    return v * (1.0f / std::sqrt(len_sq));
}

/// Convert Vec2 to array
[[nodiscard]] inline std::array<float, 2> to_array(const Vec2& v) noexcept {
    return {v.x, v.y};
}

// =============================================================================
// Vec3 Utilities
// =============================================================================

/// Create a Vec3 with all components equal to v
[[nodiscard]] inline Vec3 splat3(float v) noexcept {
    return Vec3(v, v, v);
}

/// Normalize vector, returning zero if length is too small
[[nodiscard]] inline Vec3 normalize_or_zero(const Vec3& v) noexcept {
    const float len_sq = glm::length2(v);
    if (len_sq < consts::EPSILON * consts::EPSILON) {
        return vec3::ZERO;
    }
    return v * (1.0f / std::sqrt(len_sq));
}

/// Reflect vector around normal
/// @param v The incident vector
/// @param normal The surface normal (must be normalized)
/// @return The reflected vector
[[nodiscard]] inline Vec3 reflect(const Vec3& v, const Vec3& normal) noexcept {
    return glm::reflect(v, normal);
}

/// Project vector onto another vector
/// @param v The vector to project
/// @param onto The vector to project onto
/// @return The projected vector
[[nodiscard]] inline Vec3 project(const Vec3& v, const Vec3& onto) noexcept {
    const float len_sq = glm::length2(onto);
    if (len_sq < consts::EPSILON * consts::EPSILON) {
        return vec3::ZERO;
    }
    return onto * (glm::dot(v, onto) / len_sq);
}

/// Component-wise minimum
[[nodiscard]] inline Vec3 min(const Vec3& a, const Vec3& b) noexcept {
    return glm::min(a, b);
}

/// Component-wise maximum
[[nodiscard]] inline Vec3 max(const Vec3& a, const Vec3& b) noexcept {
    return glm::max(a, b);
}

/// Component-wise absolute value
[[nodiscard]] inline Vec3 abs(const Vec3& v) noexcept {
    return glm::abs(v);
}

/// Extend Vec3 to Vec4 with given w component
[[nodiscard]] inline Vec4 extend(const Vec3& v, float w) noexcept {
    return Vec4(v.x, v.y, v.z, w);
}

/// Convert Vec3 to array
[[nodiscard]] inline std::array<float, 3> to_array(const Vec3& v) noexcept {
    return {v.x, v.y, v.z};
}

/// Check if vector has any NaN or infinite components
[[nodiscard]] inline bool is_finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/// Get the largest component
[[nodiscard]] inline float max_component(const Vec3& v) noexcept {
    return std::max({v.x, v.y, v.z});
}

/// Get the smallest component
[[nodiscard]] inline float min_component(const Vec3& v) noexcept {
    return std::min({v.x, v.y, v.z});
}

/// Distance between two points
[[nodiscard]] inline float distance(const Vec3& a, const Vec3& b) noexcept {
    return glm::distance(a, b);
}

/// Squared distance between two points
[[nodiscard]] inline float distance_squared(const Vec3& a, const Vec3& b) noexcept {
    return glm::length2(b - a);
}

// =============================================================================
// Vec4 Utilities
// =============================================================================

/// Create a Vec4 with all components equal to v
[[nodiscard]] inline Vec4 splat4(float v) noexcept {
    return Vec4(v, v, v, v);
}

/// Normalize vector, returning zero if length is too small
[[nodiscard]] inline Vec4 normalize_or_zero(const Vec4& v) noexcept {
    const float len_sq = glm::length2(v);
    if (len_sq < consts::EPSILON * consts::EPSILON) {
        return vec4::ZERO;
    }
    return v * (1.0f / std::sqrt(len_sq));
}

/// Truncate Vec4 to Vec3 (drop w component)
[[nodiscard]] inline Vec3 truncate(const Vec4& v) noexcept {
    return Vec3(v.x, v.y, v.z);
}

/// Get xyz components as Vec3 (alias for truncate)
[[nodiscard]] inline Vec3 xyz(const Vec4& v) noexcept {
    return truncate(v);
}

/// Convert Vec4 to array
[[nodiscard]] inline std::array<float, 4> to_array(const Vec4& v) noexcept {
    return {v.x, v.y, v.z, v.w};
}

// =============================================================================
// Generic Vector Operations
// =============================================================================

/// Linear interpolation between two vectors
template<typename T>
[[nodiscard]] inline T lerp(const T& a, const T& b, float t) noexcept {
    return glm::mix(a, b, t);
}

/// Clamp each component to [min_val, max_val]
template<typename T>
[[nodiscard]] inline T clamp(const T& v, float min_val, float max_val) noexcept {
    return glm::clamp(v, T(min_val), T(max_val));
}

/// Clamp each component to [0, 1]
template<typename T>
[[nodiscard]] inline T saturate(const T& v) noexcept {
    return glm::clamp(v, T(0.0f), T(1.0f));
}

/// Check if two vectors are approximately equal
template<typename T>
[[nodiscard]] inline bool approx_equal(const T& a, const T& b,
                                        float epsilon = consts::EPSILON) noexcept {
    return glm::all(glm::lessThan(glm::abs(a - b), T(epsilon)));
}

} // namespace void_math
