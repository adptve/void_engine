#pragma once

/// @file transform.hpp
/// @brief Transform class for void_math
///
/// Complete 3D transform with position, rotation, and scale.
/// Matches the Rust void_math Transform API.

#include "types.hpp"
#include "vec.hpp"
#include "mat.hpp"
#include "quat.hpp"

namespace void_math {

/// Complete 3D transform with position, rotation, and scale
struct Transform {
    Vec3 position = vec3::ZERO;
    Quat rotation = quat::IDENTITY;
    Vec3 scale_   = vec3::ONE;  // Named scale_ to avoid conflict with scale function

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor - identity transform
    constexpr Transform() noexcept = default;

    /// Full constructor
    constexpr Transform(const Vec3& pos, const Quat& rot, const Vec3& scl) noexcept
        : position(pos), rotation(rot), scale_(scl) {}

    /// Position only
    static Transform from_position(const Vec3& pos) noexcept {
        return Transform(pos, quat::IDENTITY, vec3::ONE);
    }

    /// Position and rotation
    static Transform from_position_rotation(const Vec3& pos, const Quat& rot) noexcept {
        return Transform(pos, rot, vec3::ONE);
    }

    /// Position and scale
    static Transform from_position_scale(const Vec3& pos, const Vec3& scl) noexcept {
        return Transform(pos, quat::IDENTITY, scl);
    }

    /// Identity transform constant
    static const Transform& identity() noexcept {
        static const Transform IDENTITY_TRANSFORM;
        return IDENTITY_TRANSFORM;
    }

    // =========================================================================
    // Builder Pattern
    // =========================================================================

    /// Set position and return modified transform
    [[nodiscard]] Transform with_position(const Vec3& pos) const noexcept {
        return Transform(pos, rotation, scale_);
    }

    /// Set rotation and return modified transform
    [[nodiscard]] Transform with_rotation(const Quat& rot) const noexcept {
        return Transform(position, rot, scale_);
    }

    /// Set scale and return modified transform
    [[nodiscard]] Transform with_scale(const Vec3& scl) const noexcept {
        return Transform(position, rotation, scl);
    }

    /// Set uniform scale and return modified transform
    [[nodiscard]] Transform with_scale(float scl) const noexcept {
        return Transform(position, rotation, Vec3(scl, scl, scl));
    }

    // =========================================================================
    // Conversion
    // =========================================================================

    /// Convert to 4x4 matrix (T * R * S composition)
    [[nodiscard]] Mat4 to_matrix() const noexcept {
        // Scale matrix
        Mat4 s = void_math::scale(scale_);
        // Rotation matrix
        Mat4 r = quat_to_mat4(rotation);
        // Translation matrix
        Mat4 t = translation(position);
        // Compose: T * R * S
        return t * r * s;
    }

    /// Convert to 4x4 matrix without scale (T * R only)
    [[nodiscard]] Mat4 to_matrix_no_scale() const noexcept {
        Mat4 m = quat_to_mat4(rotation);
        m[3] = Vec4(position, 1.0f);
        return m;
    }

    // =========================================================================
    // Transformation
    // =========================================================================

    /// Transform a point (applies translation, rotation, and scale)
    [[nodiscard]] Vec3 transform_point(const Vec3& point) const noexcept {
        // Apply scale, then rotation, then translation
        return position + rotate(rotation, scale_ * point);
    }

    /// Transform a direction (applies rotation only, no translation or scale)
    [[nodiscard]] Vec3 transform_direction(const Vec3& direction) const noexcept {
        return rotate(rotation, direction);
    }

    /// Transform a vector (applies rotation and scale, no translation)
    [[nodiscard]] Vec3 transform_vector(const Vec3& vector) const noexcept {
        return rotate(rotation, scale_ * vector);
    }

    // =========================================================================
    // Direction Vectors
    // =========================================================================

    /// Get forward direction (-Z in local space)
    [[nodiscard]] Vec3 forward() const noexcept {
        return rotate(rotation, vec3::FORWARD);
    }

    /// Get right direction (+X in local space)
    [[nodiscard]] Vec3 right() const noexcept {
        return rotate(rotation, vec3::RIGHT);
    }

    /// Get up direction (+Y in local space)
    [[nodiscard]] Vec3 up() const noexcept {
        return rotate(rotation, vec3::UP);
    }

    /// Get back direction (+Z in local space)
    [[nodiscard]] Vec3 back() const noexcept {
        return rotate(rotation, vec3::BACK);
    }

    /// Get left direction (-X in local space)
    [[nodiscard]] Vec3 left() const noexcept {
        return rotate(rotation, vec3::LEFT);
    }

    /// Get down direction (-Y in local space)
    [[nodiscard]] Vec3 down() const noexcept {
        return rotate(rotation, vec3::DOWN);
    }

    // =========================================================================
    // Inverse and Composition
    // =========================================================================

    /// Compute inverse transform
    [[nodiscard]] Transform inverse() const noexcept {
        Quat inv_rot = void_math::inverse(rotation);
        Vec3 inv_scale = Vec3(1.0f / scale_.x, 1.0f / scale_.y, 1.0f / scale_.z);
        Vec3 inv_pos = rotate(inv_rot, -position * inv_scale);
        return Transform(inv_pos, inv_rot, inv_scale);
    }

    /// Compose transforms (this * other)
    [[nodiscard]] Transform combine(const Transform& other) const noexcept {
        return Transform(
            transform_point(other.position),
            rotation * other.rotation,
            scale_ * other.scale_
        );
    }

    /// Interpolate between transforms
    [[nodiscard]] Transform lerp(const Transform& other, float t) const noexcept {
        return Transform(
            void_math::lerp(position, other.position, t),
            void_math::slerp(rotation, other.rotation, t),
            void_math::lerp(scale_, other.scale_, t)
        );
    }

    // =========================================================================
    // Mutation Methods
    // =========================================================================

    /// Orient to look at target point
    void look_at(const Vec3& target, const Vec3& up_hint = vec3::UP) noexcept {
        Vec3 direction = glm::normalize(target - position);
        if (glm::length2(direction) < consts::EPSILON * consts::EPSILON) {
            return; // Target is at our position
        }
        rotation = quat_from_rotation_arc(vec3::FORWARD, direction);
    }

    /// Rotate around axis by angle
    void rotate_around_axis(const Vec3& axis, float angle) noexcept {
        rotation = quat_from_axis_angle(axis, angle) * rotation;
    }

    /// Translate in local space
    void translate_local(const Vec3& offset) noexcept {
        position += transform_direction(offset);
    }

    /// Translate in world space
    void translate_world(const Vec3& offset) noexcept {
        position += offset;
    }

    /// Rotate in local space
    void rotate_local(const Quat& rot) noexcept {
        rotation = rotation * rot;
    }

    /// Rotate in world space
    void rotate_world(const Quat& rot) noexcept {
        rotation = rot * rotation;
    }

    // =========================================================================
    // Operators
    // =========================================================================

    /// Compose transforms
    Transform operator*(const Transform& other) const noexcept {
        return combine(other);
    }

    /// Compare for equality
    bool operator==(const Transform& other) const noexcept {
        return position == other.position &&
               rotation == other.rotation &&
               scale_ == other.scale_;
    }

    bool operator!=(const Transform& other) const noexcept {
        return !(*this == other);
    }
};

/// Check if two transforms are approximately equal
[[nodiscard]] inline bool approx_equal(const Transform& a, const Transform& b,
                                        float epsilon = consts::EPSILON) noexcept {
    return approx_equal(a.position, b.position, epsilon) &&
           approx_equal(a.rotation, b.rotation, epsilon) &&
           approx_equal(a.scale_, b.scale_, epsilon);
}

} // namespace void_math
