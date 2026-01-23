#pragma once

/// @file math.hpp
/// @brief Main include file for void_math
///
/// Include this file to get all void_math functionality.
/// Equivalent to Rust's `use void_math::prelude::*`.
///
/// @code
/// #include <void_engine/math/math.hpp>
/// using namespace void_math;
///
/// Vec3 position = vec3::ZERO;
/// Mat4 view = look_at(eye, target, vec3::UP);
/// Quat rotation = quat_from_euler(pitch, yaw, roll);
/// @endcode

// Core type definitions and GLM integration
#include "types.hpp"

// Mathematical constants
#include "constants.hpp"

// Vector utilities
#include "vec.hpp"

// Matrix utilities
#include "mat.hpp"

// Quaternion utilities
#include "quat.hpp"

// Transform class
#include "transform.hpp"

// Plane and frustum structures
#include "plane.hpp"

// Bounding volumes (AABB, Sphere, Frustum)
#include "bounds.hpp"

// Ray type
#include "ray.hpp"

// Intersection tests
#include "intersect.hpp"

// Double-precision and large-world support
#include "precision.hpp"

// Utility functions
#include "utils.hpp"

namespace void_math {

/// @defgroup prelude Prelude
/// @brief Commonly used types and functions
///
/// The prelude contains everything needed for typical 3D math operations:
/// - Vector types: Vec2, Vec3, Vec4
/// - Matrix types: Mat3, Mat4
/// - Quaternion: Quat
/// - Transform: Complete 3D transformation
/// - Bounding volumes: AABB, Sphere, Frustum
/// - Ray casting and intersection tests
/// - Precision utilities for large worlds

// All types and functions are already in the void_math namespace
// through the included headers above.

} // namespace void_math

// =============================================================================
// Convenience namespace alias
// =============================================================================

/// Short alias for void_math namespace
namespace vmath = void_math;
