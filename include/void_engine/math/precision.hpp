#pragma once

/// @file precision.hpp
/// @brief Double-precision and large-world support for void_math
///
/// Provides Vec3d and precision management utilities for handling
/// large game worlds without floating-point precision loss.

#include "types.hpp"
#include "vec.hpp"
#include <cmath>
#include <array>
#include <variant>
#include <string>

namespace void_math {

// =============================================================================
// Precision Thresholds
// =============================================================================

/// Distance from origin (in meters) before precision warning
inline constexpr float PRECISION_WARNING_THRESHOLD = 100000.0f;  // 100km

/// Distance from origin (in meters) before critical precision loss
inline constexpr float PRECISION_CRITICAL_THRESHOLD = 1000000.0f;  // 1000km

// =============================================================================
// Precision Status
// =============================================================================

/// Status of floating-point precision at a given position
enum class PrecisionStatus {
    Good,      ///< < 100km from origin, precision is acceptable
    Warning,   ///< 100km - 1000km, precision may be degraded
    Critical   ///< > 1000km, significant precision loss
};

/// Check if precision status is acceptable for rendering
[[nodiscard]] inline bool is_acceptable(PrecisionStatus status) noexcept {
    return status == PrecisionStatus::Good || status == PrecisionStatus::Warning;
}

/// Check if rebase is needed
[[nodiscard]] inline bool needs_rebase(PrecisionStatus status) noexcept {
    return status == PrecisionStatus::Warning || status == PrecisionStatus::Critical;
}

// =============================================================================
// Precision Error
// =============================================================================

/// Error types for precision operations
enum class PrecisionError {
    Overflow,         ///< Result would be infinity or NaN
    PrecisionLoss,    ///< Significant precision would be lost
    InvalidInput      ///< Input coordinates are invalid
};

/// Get human-readable description of precision error
[[nodiscard]] inline const char* to_string(PrecisionError error) noexcept {
    switch (error) {
        case PrecisionError::Overflow:
            return "Coordinate overflow (infinity or NaN)";
        case PrecisionError::PrecisionLoss:
            return "Significant precision loss detected";
        case PrecisionError::InvalidInput:
            return "Invalid input coordinates";
        default:
            return "Unknown precision error";
    }
}

// =============================================================================
// Vec3d (Double-Precision 3D Vector)
// =============================================================================

/// Double-precision 3D vector for large-world coordinates
struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr Vec3d ZERO() noexcept { return {0.0, 0.0, 0.0}; }
    static constexpr Vec3d X() noexcept { return {1.0, 0.0, 0.0}; }
    static constexpr Vec3d Y() noexcept { return {0.0, 1.0, 0.0}; }
    static constexpr Vec3d Z() noexcept { return {0.0, 0.0, 1.0}; }

    // =========================================================================
    // Constructors
    // =========================================================================

    constexpr Vec3d() noexcept = default;

    constexpr Vec3d(double x_, double y_, double z_) noexcept
        : x(x_), y(y_), z(z_) {}

    static constexpr Vec3d from_array(const std::array<double, 3>& arr) noexcept {
        return Vec3d(arr[0], arr[1], arr[2]);
    }

    static constexpr Vec3d splat(double v) noexcept {
        return Vec3d(v, v, v);
    }

    /// Create from single-precision Vec3
    static Vec3d from_f32(const Vec3& v) noexcept {
        return Vec3d(static_cast<double>(v.x),
                     static_cast<double>(v.y),
                     static_cast<double>(v.z));
    }

    /// Create from single-precision array
    static Vec3d from_f32(const std::array<float, 3>& arr) noexcept {
        return Vec3d(static_cast<double>(arr[0]),
                     static_cast<double>(arr[1]),
                     static_cast<double>(arr[2]));
    }

    // =========================================================================
    // Conversion
    // =========================================================================

    /// Convert to array
    [[nodiscard]] constexpr std::array<double, 3> to_array() const noexcept {
        return {x, y, z};
    }

    /// Convert to single-precision (may lose precision!)
    [[nodiscard]] Vec3 to_f32() const noexcept {
        return Vec3(static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(z));
    }

    /// Convert to single-precision array
    [[nodiscard]] std::array<float, 3> to_f32_array() const noexcept {
        return {static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(z)};
    }

    // =========================================================================
    // Vector Operations
    // =========================================================================

    [[nodiscard]] double dot(const Vec3d& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }

    [[nodiscard]] Vec3d cross(const Vec3d& other) const noexcept {
        return Vec3d(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    [[nodiscard]] double length_squared() const noexcept {
        return x * x + y * y + z * z;
    }

    [[nodiscard]] double length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] double distance(const Vec3d& other) const noexcept {
        return (*this - other).length();
    }

    [[nodiscard]] double distance_squared(const Vec3d& other) const noexcept {
        return (*this - other).length_squared();
    }

    [[nodiscard]] Vec3d normalize() const noexcept {
        double len = length();
        if (len < consts::d::EPSILON) {
            return ZERO();
        }
        return *this / len;
    }

    [[nodiscard]] Vec3d normalize_or_zero() const noexcept {
        return normalize();
    }

    [[nodiscard]] Vec3d lerp(const Vec3d& other, double t) const noexcept {
        return *this + (other - *this) * t;
    }

    [[nodiscard]] Vec3d min(const Vec3d& other) const noexcept {
        return Vec3d(std::min(x, other.x), std::min(y, other.y), std::min(z, other.z));
    }

    [[nodiscard]] Vec3d max(const Vec3d& other) const noexcept {
        return Vec3d(std::max(x, other.x), std::max(y, other.y), std::max(z, other.z));
    }

    [[nodiscard]] Vec3d abs() const noexcept {
        return Vec3d(std::abs(x), std::abs(y), std::abs(z));
    }

    [[nodiscard]] bool is_finite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    [[nodiscard]] double max_component() const noexcept {
        return std::max({x, y, z});
    }

    [[nodiscard]] double min_component() const noexcept {
        return std::min({x, y, z});
    }

    // =========================================================================
    // Operators
    // =========================================================================

    Vec3d operator+(const Vec3d& other) const noexcept {
        return Vec3d(x + other.x, y + other.y, z + other.z);
    }

    Vec3d operator-(const Vec3d& other) const noexcept {
        return Vec3d(x - other.x, y - other.y, z - other.z);
    }

    Vec3d operator*(double scalar) const noexcept {
        return Vec3d(x * scalar, y * scalar, z * scalar);
    }

    Vec3d operator/(double scalar) const noexcept {
        return Vec3d(x / scalar, y / scalar, z / scalar);
    }

    Vec3d operator-() const noexcept {
        return Vec3d(-x, -y, -z);
    }

    Vec3d& operator+=(const Vec3d& other) noexcept {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    Vec3d& operator-=(const Vec3d& other) noexcept {
        x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    Vec3d& operator*=(double scalar) noexcept {
        x *= scalar; y *= scalar; z *= scalar;
        return *this;
    }

    bool operator==(const Vec3d& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vec3d& other) const noexcept {
        return !(*this == other);
    }
};

/// Scalar * Vec3d
inline Vec3d operator*(double scalar, const Vec3d& v) noexcept {
    return v * scalar;
}

// =============================================================================
// Precision Checking Functions
// =============================================================================

/// Check precision status of a position
[[nodiscard]] inline PrecisionStatus check_precision(const Vec3& pos) noexcept {
    float dist_sq = glm::length2(pos);

    if (dist_sq > PRECISION_CRITICAL_THRESHOLD * PRECISION_CRITICAL_THRESHOLD) {
        return PrecisionStatus::Critical;
    }
    if (dist_sq > PRECISION_WARNING_THRESHOLD * PRECISION_WARNING_THRESHOLD) {
        return PrecisionStatus::Warning;
    }
    return PrecisionStatus::Good;
}

/// Check precision status with custom thresholds
[[nodiscard]] inline PrecisionStatus check_precision_with_thresholds(
    const Vec3& pos,
    float warning_threshold,
    float critical_threshold) noexcept {

    float dist_sq = glm::length2(pos);

    if (dist_sq > critical_threshold * critical_threshold) {
        return PrecisionStatus::Critical;
    }
    if (dist_sq > warning_threshold * warning_threshold) {
        return PrecisionStatus::Warning;
    }
    return PrecisionStatus::Good;
}

/// Check precision status of a position (array version)
[[nodiscard]] inline PrecisionStatus check_precision(const std::array<float, 3>& pos) noexcept {
    return check_precision(Vec3(pos[0], pos[1], pos[2]));
}

// =============================================================================
// Coordinate Conversion Functions
// =============================================================================

/// Result type for safe conversions
using PrecisionResult = std::variant<std::array<float, 3>, PrecisionError>;

/// Safely convert world coordinates to local coordinates
/// @param world World position (double precision)
/// @param origin Local origin (double precision)
/// @return Local coordinates or error
[[nodiscard]] inline PrecisionResult world_to_local_safe(
    const Vec3d& world,
    const Vec3d& origin) noexcept {

    if (!world.is_finite() || !origin.is_finite()) {
        return PrecisionError::InvalidInput;
    }

    Vec3d local = world - origin;
    std::array<float, 3> result = local.to_f32_array();

    // Check for overflow
    if (!std::isfinite(result[0]) || !std::isfinite(result[1]) || !std::isfinite(result[2])) {
        return PrecisionError::Overflow;
    }

    // Check for precision loss
    Vec3 local_f32(result[0], result[1], result[2]);
    if (check_precision(local_f32) == PrecisionStatus::Critical) {
        return PrecisionError::PrecisionLoss;
    }

    return result;
}

/// Convert world coordinates to local (array version)
[[nodiscard]] inline PrecisionResult world_to_local_safe(
    const std::array<double, 3>& world,
    const std::array<double, 3>& origin) noexcept {
    return world_to_local_safe(Vec3d::from_array(world), Vec3d::from_array(origin));
}

/// Convert local coordinates back to world
[[nodiscard]] inline Vec3d local_to_world(
    const Vec3& local,
    const Vec3d& origin) noexcept {
    return origin + Vec3d::from_f32(local);
}

/// Convert local coordinates back to world (array version)
[[nodiscard]] inline std::array<double, 3> local_to_world(
    const std::array<float, 3>& local,
    const std::array<double, 3>& origin) noexcept {
    Vec3d result = local_to_world(
        Vec3(local[0], local[1], local[2]),
        Vec3d::from_array(origin)
    );
    return result.to_array();
}

/// Get normalized direction from origin to world position
/// Useful for billboards and other direction-dependent features when position has precision issues
[[nodiscard]] inline Vec3 direction_from_origin(
    const Vec3d& world,
    const Vec3d& origin) noexcept {

    Vec3d dir = world - origin;
    Vec3d normalized = dir.normalize();
    return normalized.to_f32();
}

/// Direction from origin (array version)
[[nodiscard]] inline std::array<float, 3> direction_from_origin(
    const std::array<double, 3>& world,
    const std::array<double, 3>& origin) noexcept {
    Vec3 result = direction_from_origin(Vec3d::from_array(world), Vec3d::from_array(origin));
    return {result.x, result.y, result.z};
}

} // namespace void_math
