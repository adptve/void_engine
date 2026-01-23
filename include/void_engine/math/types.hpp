#pragma once

/// @file types.hpp
/// @brief Core type definitions for void_math

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/compatibility.hpp>

#include "fwd.hpp"
#include "constants.hpp"

namespace void_math {

// =============================================================================
// Vector Type Traits
// =============================================================================

/// Check if a type is a void_math vector type
template<typename T>
struct is_vector : std::false_type {};

template<> struct is_vector<Vec2> : std::true_type {};
template<> struct is_vector<Vec3> : std::true_type {};
template<> struct is_vector<Vec4> : std::true_type {};
template<> struct is_vector<DVec3> : std::true_type {};

template<typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// =============================================================================
// Vector Constants
// =============================================================================

namespace vec2 {
    inline constexpr Vec2 ZERO  = Vec2(0.0f, 0.0f);
    inline constexpr Vec2 ONE   = Vec2(1.0f, 1.0f);
    inline constexpr Vec2 X     = Vec2(1.0f, 0.0f);
    inline constexpr Vec2 Y     = Vec2(0.0f, 1.0f);
    inline constexpr Vec2 NEG_X = Vec2(-1.0f, 0.0f);
    inline constexpr Vec2 NEG_Y = Vec2(0.0f, -1.0f);
}

namespace vec3 {
    inline constexpr Vec3 ZERO  = Vec3(0.0f, 0.0f, 0.0f);
    inline constexpr Vec3 ONE   = Vec3(1.0f, 1.0f, 1.0f);
    inline constexpr Vec3 X     = Vec3(1.0f, 0.0f, 0.0f);
    inline constexpr Vec3 Y     = Vec3(0.0f, 1.0f, 0.0f);
    inline constexpr Vec3 Z     = Vec3(0.0f, 0.0f, 1.0f);
    inline constexpr Vec3 NEG_X = Vec3(-1.0f, 0.0f, 0.0f);
    inline constexpr Vec3 NEG_Y = Vec3(0.0f, -1.0f, 0.0f);
    inline constexpr Vec3 NEG_Z = Vec3(0.0f, 0.0f, -1.0f);

    // Common directions
    inline constexpr Vec3 UP      = Y;
    inline constexpr Vec3 DOWN    = NEG_Y;
    inline constexpr Vec3 RIGHT   = X;
    inline constexpr Vec3 LEFT    = NEG_X;
    inline constexpr Vec3 FORWARD = NEG_Z;  // -Z is forward in OpenGL/Vulkan
    inline constexpr Vec3 BACK    = Z;
}

namespace vec4 {
    inline constexpr Vec4 ZERO = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    inline constexpr Vec4 ONE  = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    inline constexpr Vec4 X    = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
    inline constexpr Vec4 Y    = Vec4(0.0f, 1.0f, 0.0f, 0.0f);
    inline constexpr Vec4 Z    = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
    inline constexpr Vec4 W    = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

// =============================================================================
// Matrix Constants
// =============================================================================

namespace mat3 {
    inline const Mat3 IDENTITY = Mat3(1.0f);
    inline const Mat3 ZERO     = Mat3(0.0f);
}

namespace mat4 {
    inline const Mat4 IDENTITY = Mat4(1.0f);
    inline const Mat4 ZERO     = Mat4(0.0f);
}

// =============================================================================
// Quaternion Constants
// =============================================================================

namespace quat {
    inline const Quat IDENTITY = Quat(1.0f, 0.0f, 0.0f, 0.0f); // w, x, y, z
}

} // namespace void_math
