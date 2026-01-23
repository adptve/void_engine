#pragma once

/// @file quat.hpp
/// @brief Quaternion utility functions for void_math
///
/// Provides quaternion creation and manipulation functions that extend GLM
/// to match the Rust void_math API.

#include "types.hpp"
#include "vec.hpp"
#include <cmath>

namespace void_math {

// =============================================================================
// Quaternion Creation Functions
// =============================================================================

/// Create quaternion from axis and angle
/// @param axis Rotation axis (must be normalized)
/// @param angle Angle in radians
[[nodiscard]] inline Quat quat_from_axis_angle(const Vec3& axis, float angle) noexcept {
    return glm::angleAxis(angle, axis);
}

/// Create quaternion from Euler angles (XYZ order)
/// @param x Rotation around X in radians
/// @param y Rotation around Y in radians
/// @param z Rotation around Z in radians
[[nodiscard]] inline Quat quat_from_euler(float x, float y, float z) noexcept {
    return Quat(Vec3(x, y, z));
}

/// Create quaternion from Euler angles (YXZ order - common for cameras)
/// @param y Yaw (rotation around Y) in radians
/// @param x Pitch (rotation around X) in radians
/// @param z Roll (rotation around Z) in radians
[[nodiscard]] inline Quat quat_from_euler_yxz(float y, float x, float z) noexcept {
    // YXZ order: first Y, then X, then Z
    Quat qy = glm::angleAxis(y, vec3::Y);
    Quat qx = glm::angleAxis(x, vec3::X);
    Quat qz = glm::angleAxis(z, vec3::Z);
    return qy * qx * qz;
}

/// Create quaternion from Euler angles vector (XYZ order)
[[nodiscard]] inline Quat quat_from_euler(const Vec3& euler) noexcept {
    return Quat(euler);
}

/// Create quaternion for rotation around X axis
/// @param angle Angle in radians
[[nodiscard]] inline Quat quat_rotation_x(float angle) noexcept {
    return glm::angleAxis(angle, vec3::X);
}

/// Create quaternion for rotation around Y axis
/// @param angle Angle in radians
[[nodiscard]] inline Quat quat_rotation_y(float angle) noexcept {
    return glm::angleAxis(angle, vec3::Y);
}

/// Create quaternion for rotation around Z axis
/// @param angle Angle in radians
[[nodiscard]] inline Quat quat_rotation_z(float angle) noexcept {
    return glm::angleAxis(angle, vec3::Z);
}

/// Create quaternion from rotation matrix (extracts rotation from Mat4)
[[nodiscard]] inline Quat quat_from_mat4(const Mat4& m) noexcept {
    return glm::quat_cast(m);
}

/// Create quaternion from rotation matrix (Mat3)
[[nodiscard]] inline Quat quat_from_mat3(const Mat3& m) noexcept {
    return glm::quat_cast(m);
}

/// Create quaternion that rotates from one direction to another
/// @param from Source direction (must be normalized)
/// @param to Target direction (must be normalized)
[[nodiscard]] inline Quat quat_from_rotation_arc(const Vec3& from, const Vec3& to) noexcept {
    // Handle parallel vectors
    float d = glm::dot(from, to);

    if (d >= 1.0f - consts::EPSILON) {
        // Vectors are nearly identical
        return quat::IDENTITY;
    }

    if (d <= -1.0f + consts::EPSILON) {
        // Vectors are nearly opposite - find arbitrary perpendicular axis
        Vec3 axis = glm::cross(vec3::X, from);
        if (glm::length2(axis) < consts::EPSILON * consts::EPSILON) {
            axis = glm::cross(vec3::Y, from);
        }
        return glm::angleAxis(consts::PI, glm::normalize(axis));
    }

    Vec3 axis = glm::cross(from, to);
    float s = std::sqrt((1.0f + d) * 2.0f);
    float inv_s = 1.0f / s;

    return Quat(s * 0.5f, axis.x * inv_s, axis.y * inv_s, axis.z * inv_s);
}

// =============================================================================
// Quaternion Operations
// =============================================================================

/// Normalize quaternion, returning identity if length is too small
[[nodiscard]] inline Quat normalize_or_identity(const Quat& q) noexcept {
    float len_sq = glm::length2(q);
    if (len_sq < consts::EPSILON * consts::EPSILON) {
        return quat::IDENTITY;
    }
    return q * (1.0f / std::sqrt(len_sq));
}

/// Get quaternion conjugate (inverse for unit quaternions)
[[nodiscard]] inline Quat conjugate(const Quat& q) noexcept {
    return glm::conjugate(q);
}

/// Get quaternion inverse
[[nodiscard]] inline Quat inverse(const Quat& q) noexcept {
    return glm::inverse(q);
}

/// Spherical linear interpolation (highest quality rotation interpolation)
/// @param a Start quaternion
/// @param b End quaternion
/// @param t Interpolation factor [0, 1]
[[nodiscard]] inline Quat slerp(const Quat& a, const Quat& b, float t) noexcept {
    return glm::slerp(a, b, t);
}

/// Linear interpolation (faster but less accurate than slerp)
/// @note Result should be normalized for unit quaternions
[[nodiscard]] inline Quat lerp(const Quat& a, const Quat& b, float t) noexcept {
    return glm::lerp(a, b, t);
}

/// Normalized linear interpolation (nlerp)
[[nodiscard]] inline Quat nlerp(const Quat& a, const Quat& b, float t) noexcept {
    return glm::normalize(glm::lerp(a, b, t));
}

/// Rotate a vector by quaternion
[[nodiscard]] inline Vec3 rotate(const Quat& q, const Vec3& v) noexcept {
    return q * v;
}

/// Convert quaternion to axis-angle representation
/// @return Pair of (axis, angle) where angle is in radians
[[nodiscard]] inline std::pair<Vec3, float> to_axis_angle(const Quat& q) noexcept {
    Quat normalized = glm::normalize(q);
    float angle = 2.0f * std::acos(std::clamp(normalized.w, -1.0f, 1.0f));

    float s = std::sqrt(1.0f - normalized.w * normalized.w);
    Vec3 axis;

    if (s < consts::EPSILON) {
        // Angle is close to 0 or 2*PI, axis is arbitrary
        axis = vec3::X;
    } else {
        axis = Vec3(normalized.x, normalized.y, normalized.z) / s;
    }

    return {axis, angle};
}

/// Convert quaternion to Euler angles (XYZ order)
/// @return Vec3 with (pitch, yaw, roll) in radians
[[nodiscard]] inline Vec3 to_euler(const Quat& q) noexcept {
    return glm::eulerAngles(q);
}

/// Convert quaternion to 3x3 rotation matrix
[[nodiscard]] inline Mat3 quat_to_mat3(const Quat& q) noexcept {
    return glm::mat3_cast(q);
}

/// Convert quaternion to 4x4 rotation matrix
[[nodiscard]] inline Mat4 quat_to_mat4(const Quat& q) noexcept {
    return glm::mat4_cast(q);
}

/// Convert quaternion to Vec4 (x, y, z, w)
[[nodiscard]] inline Vec4 quat_to_vec4(const Quat& q) noexcept {
    return Vec4(q.x, q.y, q.z, q.w);
}

/// Create quaternion from Vec4 (x, y, z, w)
[[nodiscard]] inline Quat quat_from_vec4(const Vec4& v) noexcept {
    return Quat(v.w, v.x, v.y, v.z);
}

/// Get the angle of rotation in radians
[[nodiscard]] inline float angle(const Quat& q) noexcept {
    return glm::angle(q);
}

/// Get the rotation axis
[[nodiscard]] inline Vec3 axis(const Quat& q) noexcept {
    return glm::axis(q);
}

/// Check if two quaternions are approximately equal
[[nodiscard]] inline bool approx_equal(const Quat& a, const Quat& b,
                                        float epsilon = consts::EPSILON) noexcept {
    // Quaternions q and -q represent the same rotation
    float dot_pos = glm::dot(a, b);
    float dot_neg = glm::dot(a, -b);
    return std::abs(dot_pos) > 1.0f - epsilon || std::abs(dot_neg) > 1.0f - epsilon;
}

/// Get angle between two quaternions in radians
[[nodiscard]] inline float angle_between(const Quat& a, const Quat& b) noexcept {
    float d = std::abs(glm::dot(a, b));
    return 2.0f * std::acos(std::clamp(d, 0.0f, 1.0f));
}

} // namespace void_math
