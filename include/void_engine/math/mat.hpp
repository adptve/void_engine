#pragma once

/// @file mat.hpp
/// @brief Matrix utility functions for void_math
///
/// Provides matrix creation and manipulation functions that extend GLM
/// to match the Rust void_math API.

#include "types.hpp"
#include "vec.hpp"
#include <array>
#include <cmath>

namespace void_math {

// =============================================================================
// Mat3 Utilities
// =============================================================================

/// Create Mat3 from column vectors
[[nodiscard]] inline Mat3 mat3_from_cols(const Vec3& c0, const Vec3& c1, const Vec3& c2) noexcept {
    return Mat3(c0, c1, c2);
}

/// Create diagonal scale matrix
[[nodiscard]] inline Mat3 mat3_from_scale(const Vec3& scale) noexcept {
    return Mat3(
        Vec3(scale.x, 0.0f, 0.0f),
        Vec3(0.0f, scale.y, 0.0f),
        Vec3(0.0f, 0.0f, scale.z)
    );
}

/// Create 2D scale matrix (for Vec2 operations)
[[nodiscard]] inline Mat3 mat3_from_scale_2d(const Vec2& scale) noexcept {
    return Mat3(
        Vec3(scale.x, 0.0f, 0.0f),
        Vec3(0.0f, scale.y, 0.0f),
        Vec3(0.0f, 0.0f, 1.0f)
    );
}

/// Convert Mat3 to Mat4 (upper-left 3x3, rest is identity)
[[nodiscard]] inline Mat4 mat3_to_mat4(const Mat3& m) noexcept {
    return Mat4(
        Vec4(m[0], 0.0f),
        Vec4(m[1], 0.0f),
        Vec4(m[2], 0.0f),
        vec4::W
    );
}

// =============================================================================
// Mat4 Creation Functions
// =============================================================================

/// Create Mat4 from column vectors
[[nodiscard]] inline Mat4 mat4_from_cols(const Vec4& c0, const Vec4& c1,
                                          const Vec4& c2, const Vec4& c3) noexcept {
    return Mat4(c0, c1, c2, c3);
}

/// Create translation matrix
[[nodiscard]] inline Mat4 translation(const Vec3& v) noexcept {
    return glm::translate(mat4::IDENTITY, v);
}

/// Create scale matrix
[[nodiscard]] inline Mat4 scale(const Vec3& v) noexcept {
    return glm::scale(mat4::IDENTITY, v);
}

/// Create uniform scale matrix
[[nodiscard]] inline Mat4 scale(float s) noexcept {
    return glm::scale(mat4::IDENTITY, Vec3(s, s, s));
}

/// Create rotation matrix around X axis
/// @param angle Angle in radians
[[nodiscard]] inline Mat4 rotation_x(float angle) noexcept {
    return glm::rotate(mat4::IDENTITY, angle, vec3::X);
}

/// Create rotation matrix around Y axis
/// @param angle Angle in radians
[[nodiscard]] inline Mat4 rotation_y(float angle) noexcept {
    return glm::rotate(mat4::IDENTITY, angle, vec3::Y);
}

/// Create rotation matrix around Z axis
/// @param angle Angle in radians
[[nodiscard]] inline Mat4 rotation_z(float angle) noexcept {
    return glm::rotate(mat4::IDENTITY, angle, vec3::Z);
}

/// Create rotation matrix around arbitrary axis (Rodrigues' formula)
/// @param axis Rotation axis (must be normalized)
/// @param angle Angle in radians
[[nodiscard]] inline Mat4 rotation_axis_angle(const Vec3& axis, float angle) noexcept {
    return glm::rotate(mat4::IDENTITY, angle, axis);
}

/// Create combined rotation and translation matrix
[[nodiscard]] inline Mat4 rotation_translation(const Quat& rotation, const Vec3& translation) noexcept {
    Mat4 m = glm::mat4_cast(rotation);
    m[3] = Vec4(translation, 1.0f);
    return m;
}

// =============================================================================
// View/Projection Matrices
// =============================================================================

/// Create look-at view matrix (right-handed)
/// @param eye Camera position
/// @param target Point to look at
/// @param up Up vector (usually Y-axis)
[[nodiscard]] inline Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
    return glm::lookAt(eye, target, up);
}

/// Create perspective projection matrix (right-handed, depth [0,1])
/// @param fov_y Field of view in radians (vertical)
/// @param aspect Aspect ratio (width / height)
/// @param near Near clip plane distance
/// @param far Far clip plane distance
[[nodiscard]] inline Mat4 perspective(float fov_y, float aspect, float near, float far) noexcept {
    return glm::perspectiveRH_ZO(fov_y, aspect, near, far);
}

/// Create perspective projection matrix with infinite far plane
/// @param fov_y Field of view in radians (vertical)
/// @param aspect Aspect ratio (width / height)
/// @param near Near clip plane distance
[[nodiscard]] inline Mat4 perspective_infinite(float fov_y, float aspect, float near) noexcept {
    return glm::infinitePerspectiveRH(fov_y, aspect, near);
}

/// Create orthographic projection matrix (right-handed, depth [0,1])
/// @note This is the Vulkan/wgpu-compatible version with depth range [0,1]
[[nodiscard]] inline Mat4 orthographic(float left, float right, float bottom,
                                        float top, float near, float far) noexcept {
    return glm::orthoRH_ZO(left, right, bottom, top, near, far);
}

/// Create orthographic projection matrix (OpenGL-style, depth [-1,1])
[[nodiscard]] inline Mat4 orthographic_gl(float left, float right, float bottom,
                                           float top, float near, float far) noexcept {
    return glm::orthoRH_NO(left, right, bottom, top, near, far);
}

// =============================================================================
// Mat4 Operations
// =============================================================================

/// Extract translation component from matrix
[[nodiscard]] inline Vec3 get_translation(const Mat4& m) noexcept {
    return Vec3(m[3]);
}

/// Set translation component of matrix
inline void set_translation(Mat4& m, const Vec3& translation) noexcept {
    m[3] = Vec4(translation, 1.0f);
}

/// Extract scale component from matrix (assumes no shear)
[[nodiscard]] inline Vec3 get_scale(const Mat4& m) noexcept {
    return Vec3(
        glm::length(Vec3(m[0])),
        glm::length(Vec3(m[1])),
        glm::length(Vec3(m[2]))
    );
}

/// Transform a point (homogeneous w=1)
[[nodiscard]] inline Vec3 transform_point(const Mat4& m, const Vec3& point) noexcept {
    Vec4 result = m * Vec4(point, 1.0f);
    return Vec3(result) / result.w;
}

/// Transform a direction/vector (homogeneous w=0)
[[nodiscard]] inline Vec3 transform_vector(const Mat4& m, const Vec3& vector) noexcept {
    return Vec3(m * Vec4(vector, 0.0f));
}

/// Transform a normal vector (uses inverse transpose)
[[nodiscard]] inline Vec3 transform_normal(const Mat4& m, const Vec3& normal) noexcept {
    // For normals, we need to use the inverse transpose of the upper 3x3
    Mat3 normal_matrix = glm::transpose(glm::inverse(Mat3(m)));
    return normalize_or_zero(normal_matrix * normal);
}

/// Convert Mat4 to column-major array
[[nodiscard]] inline std::array<float, 16> to_array(const Mat4& m) noexcept {
    std::array<float, 16> result;
    const float* ptr = glm::value_ptr(m);
    std::copy(ptr, ptr + 16, result.begin());
    return result;
}

/// Convert Mat4 to 2D column-major array
[[nodiscard]] inline std::array<std::array<float, 4>, 4> to_cols_array_2d(const Mat4& m) noexcept {
    return {{
        {m[0][0], m[0][1], m[0][2], m[0][3]},
        {m[1][0], m[1][1], m[1][2], m[1][3]},
        {m[2][0], m[2][1], m[2][2], m[2][3]},
        {m[3][0], m[3][1], m[3][2], m[3][3]}
    }};
}

/// Calculate matrix inverse
[[nodiscard]] inline Mat4 inverse(const Mat4& m) noexcept {
    return glm::inverse(m);
}

/// Calculate matrix transpose
[[nodiscard]] inline Mat4 transpose(const Mat4& m) noexcept {
    return glm::transpose(m);
}

/// Calculate matrix determinant
[[nodiscard]] inline float determinant(const Mat4& m) noexcept {
    return glm::determinant(m);
}

/// Check if two matrices are approximately equal
[[nodiscard]] inline bool approx_equal(const Mat4& a, const Mat4& b,
                                        float epsilon = consts::EPSILON) noexcept {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (std::abs(a[i][j] - b[i][j]) > epsilon) {
                return false;
            }
        }
    }
    return true;
}

} // namespace void_math
