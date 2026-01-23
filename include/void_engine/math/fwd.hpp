#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_math types

#include <glm/fwd.hpp>

namespace void_math {

// =============================================================================
// Vector Types (GLM aliases)
// =============================================================================
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using IVec2 = glm::ivec2;
using IVec3 = glm::ivec3;
using IVec4 = glm::ivec4;

using UVec2 = glm::uvec2;
using UVec3 = glm::uvec3;
using UVec4 = glm::uvec4;

using DVec2 = glm::dvec2;
using DVec3 = glm::dvec3;
using DVec4 = glm::dvec4;

// =============================================================================
// Matrix Types (GLM aliases)
// =============================================================================
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

using DMat3 = glm::dmat3;
using DMat4 = glm::dmat4;

// =============================================================================
// Quaternion Types (GLM aliases)
// =============================================================================
using Quat = glm::quat;
using DQuat = glm::dquat;

// =============================================================================
// Forward Declarations (void_math types)
// =============================================================================
struct Transform;
struct AABB;
struct Sphere;
struct Frustum;
struct FrustumPlanes;
struct Plane;
struct Ray;
struct TriangleHit;

// Double-precision types
struct Vec3d;

// Enums
enum class FrustumTestResult;
enum class PrecisionStatus;
enum class PrecisionError;

} // namespace void_math
