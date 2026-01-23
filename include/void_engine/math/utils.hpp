#pragma once

/// @file utils.hpp
/// @brief General math utility functions for void_math

#include "constants.hpp"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace void_math {

// =============================================================================
// Angle Conversion
// =============================================================================

/// Convert degrees to radians
[[nodiscard]] constexpr float radians(float degrees) noexcept {
    return degrees * consts::DEG_TO_RAD;
}

/// Convert radians to degrees
[[nodiscard]] constexpr float degrees(float radians) noexcept {
    return radians * consts::RAD_TO_DEG;
}

/// Convert degrees to radians (double precision)
[[nodiscard]] constexpr double radians_d(double degrees) noexcept {
    return degrees * consts::d::DEG_TO_RAD;
}

/// Convert radians to degrees (double precision)
[[nodiscard]] constexpr double degrees_d(double radians) noexcept {
    return radians * consts::d::RAD_TO_DEG;
}

// =============================================================================
// Interpolation
// =============================================================================

/// Linear interpolation between two values
/// @param a Start value
/// @param b End value
/// @param t Interpolation factor [0, 1]
[[nodiscard]] constexpr float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

/// Inverse linear interpolation - find t given a value between a and b
/// @return The interpolation factor that would produce value
[[nodiscard]] inline float inverse_lerp(float a, float b, float value) noexcept {
    float range = b - a;
    if (std::abs(range) < consts::EPSILON) {
        return 0.0f;
    }
    return (value - a) / range;
}

/// Remap a value from one range to another
[[nodiscard]] inline float remap(float value, float in_min, float in_max,
                                  float out_min, float out_max) noexcept {
    float t = inverse_lerp(in_min, in_max, value);
    return lerp(out_min, out_max, t);
}

/// Smooth step interpolation (Hermite)
/// @param edge0 Lower edge
/// @param edge1 Upper edge
/// @param x Input value
[[nodiscard]] inline float smoothstep(float edge0, float edge1, float x) noexcept {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// Smoother step interpolation (Ken Perlin's improved version)
[[nodiscard]] inline float smootherstep(float edge0, float edge1, float x) noexcept {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// =============================================================================
// Clamping
// =============================================================================

/// Clamp value to range [min_val, max_val]
[[nodiscard]] constexpr float clamp(float value, float min_val, float max_val) noexcept {
    return value < min_val ? min_val : (value > max_val ? max_val : value);
}

/// Clamp value to range [0, 1]
[[nodiscard]] constexpr float saturate(float value) noexcept {
    return clamp(value, 0.0f, 1.0f);
}

/// Wrap value to range [0, max)
[[nodiscard]] inline float wrap(float value, float max_val) noexcept {
    return std::fmod(std::fmod(value, max_val) + max_val, max_val);
}

/// Wrap value to range [min, max)
[[nodiscard]] inline float wrap(float value, float min_val, float max_val) noexcept {
    float range = max_val - min_val;
    return min_val + wrap(value - min_val, range);
}

/// Wrap angle to range [-PI, PI)
[[nodiscard]] inline float wrap_angle(float angle) noexcept {
    return wrap(angle + consts::PI, consts::TAU) - consts::PI;
}

// =============================================================================
// Comparison
// =============================================================================

/// Check if two floats are approximately equal
[[nodiscard]] inline bool approx_equal(float a, float b,
                                        float epsilon = consts::EPSILON) noexcept {
    return std::abs(a - b) <= epsilon;
}

/// Check if a float is approximately zero
[[nodiscard]] inline bool approx_zero(float value,
                                       float epsilon = consts::EPSILON) noexcept {
    return std::abs(value) <= epsilon;
}

/// Sign function (-1, 0, or 1)
[[nodiscard]] constexpr float sign(float value) noexcept {
    return (value > 0.0f) ? 1.0f : ((value < 0.0f) ? -1.0f : 0.0f);
}

/// Copy sign of b to a
[[nodiscard]] inline float copysign(float a, float b) noexcept {
    return std::copysign(a, b);
}

// =============================================================================
// Fast Math Approximations
// =============================================================================

/// Fast inverse square root (Quake-style, modernized)
/// @note For most use cases, 1.0f / std::sqrt(x) is faster on modern CPUs
[[nodiscard]] inline float fast_inv_sqrt(float x) noexcept {
    // Modern compilers optimize 1/sqrt well, but keep this for reference
    float xhalf = 0.5f * x;
    std::int32_t i;
    std::memcpy(&i, &x, sizeof(float));
    i = 0x5f3759df - (i >> 1);
    std::memcpy(&x, &i, sizeof(float));
    x = x * (1.5f - xhalf * x * x);  // Newton-Raphson iteration
    return x;
}

/// Fast approximate sin (Taylor series, good for small angles)
[[nodiscard]] inline float fast_sin(float x) noexcept {
    // Normalize to [-PI, PI]
    x = wrap_angle(x);
    // Taylor series approximation
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f - x2 / 5040.0f)));
}

/// Fast approximate cos
[[nodiscard]] inline float fast_cos(float x) noexcept {
    return fast_sin(x + consts::FRAC_PI_2);
}

// =============================================================================
// Integer Math
// =============================================================================

/// Check if value is a power of two
[[nodiscard]] constexpr bool is_power_of_two(std::uint32_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

/// Round up to next power of two
[[nodiscard]] constexpr std::uint32_t next_power_of_two(std::uint32_t value) noexcept {
    if (value == 0) return 1;
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}

/// Align value up to alignment (alignment must be power of two)
[[nodiscard]] constexpr std::uint32_t align_up(std::uint32_t value,
                                                std::uint32_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

/// Align value down to alignment (alignment must be power of two)
[[nodiscard]] constexpr std::uint32_t align_down(std::uint32_t value,
                                                  std::uint32_t alignment) noexcept {
    return value & ~(alignment - 1);
}

// =============================================================================
// Easing Functions
// =============================================================================

/// Quadratic ease in
[[nodiscard]] constexpr float ease_in_quad(float t) noexcept {
    return t * t;
}

/// Quadratic ease out
[[nodiscard]] constexpr float ease_out_quad(float t) noexcept {
    return t * (2.0f - t);
}

/// Quadratic ease in-out
[[nodiscard]] constexpr float ease_in_out_quad(float t) noexcept {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

/// Cubic ease in
[[nodiscard]] constexpr float ease_in_cubic(float t) noexcept {
    return t * t * t;
}

/// Cubic ease out
[[nodiscard]] constexpr float ease_out_cubic(float t) noexcept {
    float t1 = t - 1.0f;
    return t1 * t1 * t1 + 1.0f;
}

/// Cubic ease in-out
[[nodiscard]] constexpr float ease_in_out_cubic(float t) noexcept {
    return t < 0.5f
        ? 4.0f * t * t * t
        : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

/// Exponential ease in
[[nodiscard]] inline float ease_in_expo(float t) noexcept {
    return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
}

/// Exponential ease out
[[nodiscard]] inline float ease_out_expo(float t) noexcept {
    return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

/// Elastic ease out (spring-like)
[[nodiscard]] inline float ease_out_elastic(float t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - 0.075f) * consts::TAU / 0.3f) + 1.0f;
}

/// Bounce ease out
[[nodiscard]] inline float ease_out_bounce(float t) noexcept {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= 2.625f / 2.75f;
        return 7.5625f * t * t + 0.984375f;
    }
}

} // namespace void_math
