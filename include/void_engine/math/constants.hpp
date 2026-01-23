#pragma once

/// @file constants.hpp
/// @brief Mathematical constants for void_math

#include <cmath>
#include <limits>

namespace void_math {

/// Mathematical constants
namespace consts {

/// Pi (π)
inline constexpr float PI = 3.14159265358979323846f;

/// Tau (2π)
inline constexpr float TAU = 6.28318530717958647692f;

/// Half Pi (π/2)
inline constexpr float FRAC_PI_2 = 1.57079632679489661923f;

/// Quarter Pi (π/4)
inline constexpr float FRAC_PI_4 = 0.78539816339744830962f;

/// Degrees to radians conversion factor
inline constexpr float DEG_TO_RAD = PI / 180.0f;

/// Radians to degrees conversion factor
inline constexpr float RAD_TO_DEG = 180.0f / PI;

/// Small epsilon for floating point comparisons
inline constexpr float EPSILON = 1e-6f;

/// Larger epsilon for less precise comparisons
inline constexpr float EPSILON_LOOSE = 1e-4f;

/// Machine epsilon for float
inline constexpr float FLOAT_EPSILON = std::numeric_limits<float>::epsilon();

/// Infinity
inline constexpr float INFINITY_F = std::numeric_limits<float>::infinity();

/// Maximum float value
inline constexpr float MAX_FLOAT = std::numeric_limits<float>::max();

/// Minimum positive float value
inline constexpr float MIN_FLOAT = std::numeric_limits<float>::min();

// Double precision constants
namespace d {

inline constexpr double PI = 3.14159265358979323846;
inline constexpr double TAU = 6.28318530717958647692;
inline constexpr double FRAC_PI_2 = 1.57079632679489661923;
inline constexpr double FRAC_PI_4 = 0.78539816339744830962;
inline constexpr double DEG_TO_RAD = PI / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / PI;
inline constexpr double EPSILON = 1e-10;

} // namespace d

} // namespace consts

} // namespace void_math
