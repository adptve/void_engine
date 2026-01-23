#pragma once

/// @file version.hpp
/// @brief Semantic versioning for void_core

#include "fwd.hpp"
#include <cstdint>
#include <string>
#include <sstream>
#include <optional>
#include <compare>

namespace void_core {

// =============================================================================
// Version
// =============================================================================

/// Semantic version (major.minor.patch)
struct Version {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t patch = 0;

    /// Default constructor
    constexpr Version() noexcept = default;

    /// Construct with components
    constexpr Version(std::uint16_t maj, std::uint16_t min, std::uint16_t pat) noexcept
        : major(maj), minor(min), patch(pat) {}

    /// Zero version
    static constexpr Version zero() noexcept {
        return Version{0, 0, 0};
    }

    /// Create version (convenience)
    [[nodiscard]] static constexpr Version create(std::uint16_t maj, std::uint16_t min = 0, std::uint16_t pat = 0) noexcept {
        return Version{maj, min, pat};
    }

    /// Check compatibility with another version
    /// Pre-1.0: minor must match exactly
    /// Post-1.0: major must match, self >= other in minor/patch
    [[nodiscard]] constexpr bool is_compatible_with(const Version& other) const noexcept {
        if (major == 0 && other.major == 0) {
            // Pre-1.0: minor must match exactly
            return minor == other.minor && patch >= other.patch;
        }
        // Post-1.0: major must match, and we must be >= other
        return major == other.major &&
               (minor > other.minor || (minor == other.minor && patch >= other.patch));
    }

    /// Parse version string "major.minor.patch"
    [[nodiscard]] static std::optional<Version> parse(const std::string& s) {
        Version v;
        char dot1, dot2;
        std::istringstream iss(s);

        // Try full format: major.minor.patch
        if (iss >> v.major >> dot1 >> v.minor >> dot2 >> v.patch) {
            if (dot1 == '.' && dot2 == '.') {
                return v;
            }
        }

        // Try short format: major.minor
        iss.clear();
        iss.str(s);
        v.patch = 0;
        if (iss >> v.major >> dot1 >> v.minor) {
            if (dot1 == '.') {
                return v;
            }
        }

        return std::nullopt;
    }

    /// Convert to packed 64-bit value
    [[nodiscard]] constexpr std::uint64_t to_u64() const noexcept {
        return (static_cast<std::uint64_t>(major) << 32) |
               (static_cast<std::uint64_t>(minor) << 16) |
               static_cast<std::uint64_t>(patch);
    }

    /// Create from packed 64-bit value
    [[nodiscard]] static constexpr Version from_u64(std::uint64_t bits) noexcept {
        return Version{
            static_cast<std::uint16_t>((bits >> 32) & 0xFFFF),
            static_cast<std::uint16_t>((bits >> 16) & 0xFFFF),
            static_cast<std::uint16_t>(bits & 0xFFFF)
        };
    }

    /// Convert to string
    [[nodiscard]] std::string to_string() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    /// Comparison operators
    constexpr auto operator<=>(const Version&) const noexcept = default;
    constexpr bool operator==(const Version&) const noexcept = default;

    /// Increment patch
    constexpr Version increment_patch() const noexcept {
        return Version{major, minor, static_cast<std::uint16_t>(patch + 1)};
    }

    /// Increment minor (resets patch)
    constexpr Version increment_minor() const noexcept {
        return Version{major, static_cast<std::uint16_t>(minor + 1), 0};
    }

    /// Increment major (resets minor and patch)
    constexpr Version increment_major() const noexcept {
        return Version{static_cast<std::uint16_t>(major + 1), 0, 0};
    }

    /// Check if pre-release (major == 0)
    [[nodiscard]] constexpr bool is_prerelease() const noexcept {
        return major == 0;
    }
};

/// Output stream operator
inline std::ostream& operator<<(std::ostream& os, const Version& v) {
    return os << v.major << '.' << v.minor << '.' << v.patch;
}

} // namespace void_core

/// Hash specialization
template<>
struct std::hash<void_core::Version> {
    std::size_t operator()(const void_core::Version& v) const noexcept {
        return std::hash<std::uint64_t>{}(v.to_u64());
    }
};
