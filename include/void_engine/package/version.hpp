#pragma once

/// @file version.hpp
/// @brief Semantic versioning for package system
///
/// Supports full SemVer 2.0.0:
/// - Parse "1.2.3", "1.2.3-beta", "1.2.3-beta.1+build123"
/// - Compare versions (==, <, >, <=, >=)
/// - Match constraints: ">=1.0.0", "^1.2", "~1.2.3", ranges

#include "fwd.hpp"
#include <void_engine/core/error.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <compare>
#include <optional>

namespace void_package {

// =============================================================================
// SemanticVersion
// =============================================================================

/// Full semantic version (major.minor.patch[-prerelease][+build])
///
/// Follows SemVer 2.0.0 specification:
/// - Prerelease has lower precedence than normal version
/// - Build metadata is ignored in comparisons
/// - Prerelease identifiers compared as numbers if numeric, else lexically
struct SemanticVersion {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;
    std::string prerelease;      ///< e.g., "alpha", "beta.1", "rc.2"
    std::string build_metadata;  ///< e.g., "build123", "sha.a1b2c3d"

    /// Default constructor - creates 0.0.0
    constexpr SemanticVersion() noexcept = default;

    /// Construct with major.minor.patch
    constexpr SemanticVersion(std::uint32_t maj, std::uint32_t min, std::uint32_t pat) noexcept
        : major(maj), minor(min), patch(pat) {}

    /// Construct with all fields
    SemanticVersion(std::uint32_t maj, std::uint32_t min, std::uint32_t pat,
                    std::string pre, std::string build = "")
        : major(maj), minor(min), patch(pat)
        , prerelease(std::move(pre))
        , build_metadata(std::move(build)) {}

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Parse a version string
    ///
    /// Supported formats:
    /// - "1"
    /// - "1.2"
    /// - "1.2.3"
    /// - "1.2.3-alpha"
    /// - "1.2.3-alpha.1"
    /// - "1.2.3+build123"
    /// - "1.2.3-alpha.1+build123"
    [[nodiscard]] static void_core::Result<SemanticVersion> parse(std::string_view str);

    // =========================================================================
    // Comparison
    // =========================================================================

    /// Three-way comparison (SemVer rules)
    ///
    /// Ordering rules:
    /// 1. Compare major, minor, patch numerically
    /// 2. Version with prerelease has LOWER precedence than without
    /// 3. Prerelease identifiers compared left-to-right
    /// 4. Numeric identifiers compared as integers
    /// 5. Alphanumeric identifiers compared lexically (ASCII)
    /// 6. Build metadata is IGNORED in comparisons
    [[nodiscard]] std::strong_ordering operator<=>(const SemanticVersion& other) const noexcept;

    /// Equality comparison
    [[nodiscard]] bool operator==(const SemanticVersion& other) const noexcept;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Check if this is a prerelease version
    [[nodiscard]] bool is_prerelease() const noexcept {
        return !prerelease.empty();
    }

    /// Check if this has build metadata
    [[nodiscard]] bool has_build_metadata() const noexcept {
        return !build_metadata.empty();
    }

    /// Check if version is 0.x.x (unstable API)
    [[nodiscard]] bool is_unstable() const noexcept {
        return major == 0;
    }

    /// Get core version (without prerelease/build)
    [[nodiscard]] SemanticVersion core() const {
        return SemanticVersion{major, minor, patch};
    }

    // =========================================================================
    // String Conversion
    // =========================================================================

    /// Convert to string representation
    [[nodiscard]] std::string to_string() const;

    /// Convert to string (core version only, no prerelease/build)
    [[nodiscard]] std::string to_string_core() const;

    // =========================================================================
    // Version Incrementing
    // =========================================================================

    /// Increment patch version (resets prerelease)
    [[nodiscard]] SemanticVersion increment_patch() const {
        return SemanticVersion{major, minor, patch + 1};
    }

    /// Increment minor version (resets patch and prerelease)
    [[nodiscard]] SemanticVersion increment_minor() const {
        return SemanticVersion{major, minor + 1, 0};
    }

    /// Increment major version (resets minor, patch, and prerelease)
    [[nodiscard]] SemanticVersion increment_major() const {
        return SemanticVersion{major + 1, 0, 0};
    }

private:
    /// Compare prerelease strings per SemVer rules
    [[nodiscard]] static std::strong_ordering compare_prerelease(
        std::string_view a, std::string_view b) noexcept;
};

// =============================================================================
// VersionConstraint
// =============================================================================

/// A constraint that can match versions
///
/// Supported constraint types:
/// - Exact: "1.2.3" (matches only 1.2.3)
/// - Greater/Less: ">1.0.0", ">=1.0.0", "<2.0.0", "<=2.0.0"
/// - Caret: "^1.2.3" (>=1.2.3, <2.0.0 for 1.x; >=0.2.3, <0.3.0 for 0.x)
/// - Tilde: "~1.2.3" (>=1.2.3, <1.3.0)
/// - Wildcard: "1.x", "1.2.x", "1.*", "1.2.*" (any matching)
/// - Range: ">=1.0.0,<2.0.0" (multiple constraints ANDed)
struct VersionConstraint {
    enum class Type : std::uint8_t {
        Any,           ///< Matches any version (*)
        Exact,         ///< Exact match (=1.2.3 or 1.2.3)
        Greater,       ///< Greater than (>1.2.3)
        GreaterEqual,  ///< Greater or equal (>=1.2.3)
        Less,          ///< Less than (<1.2.3)
        LessEqual,     ///< Less or equal (<=1.2.3)
        Caret,         ///< Compatible with (^1.2.3)
        Tilde,         ///< Approximately (~1.2.3)
        Range          ///< Multiple constraints (>=1.0.0,<2.0.0)
    };

    Type type = Type::Any;
    SemanticVersion version;                      ///< For single version constraints
    SemanticVersion min_version;                  ///< For range constraints
    SemanticVersion max_version;                  ///< For range constraints
    bool min_inclusive = true;                    ///< For range: include min?
    bool max_inclusive = false;                   ///< For range: include max?
    std::vector<VersionConstraint> sub_constraints;  ///< For complex ranges

    /// Default constructor - matches any version
    VersionConstraint() = default;

    // =========================================================================
    // Parsing
    // =========================================================================

    /// Parse a version constraint string
    ///
    /// Examples:
    /// - "*" or "" -> any version
    /// - "1.2.3" -> exact 1.2.3
    /// - ">=1.0.0" -> 1.0.0 or higher
    /// - "^1.2.3" -> compatible with 1.2.3
    /// - "~1.2.3" -> approximately 1.2.3
    /// - ">=1.0.0,<2.0.0" -> range (ANDed)
    [[nodiscard]] static void_core::Result<VersionConstraint> parse(std::string_view str);

    // =========================================================================
    // Matching
    // =========================================================================

    /// Check if a version satisfies this constraint
    [[nodiscard]] bool satisfies(const SemanticVersion& v) const noexcept;

    // =========================================================================
    // String Conversion
    // =========================================================================

    /// Convert to string representation
    [[nodiscard]] std::string to_string() const;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /// Create "any version" constraint
    [[nodiscard]] static VersionConstraint any() {
        return VersionConstraint{};
    }

    /// Create exact version constraint
    [[nodiscard]] static VersionConstraint exact(SemanticVersion v) {
        VersionConstraint c;
        c.type = Type::Exact;
        c.version = std::move(v);
        return c;
    }

    /// Create >= constraint
    [[nodiscard]] static VersionConstraint greater_equal(SemanticVersion v) {
        VersionConstraint c;
        c.type = Type::GreaterEqual;
        c.version = std::move(v);
        return c;
    }

    /// Create caret constraint (^1.2.3)
    [[nodiscard]] static VersionConstraint caret(SemanticVersion v) {
        VersionConstraint c;
        c.type = Type::Caret;
        c.version = std::move(v);
        return c;
    }

    /// Create tilde constraint (~1.2.3)
    [[nodiscard]] static VersionConstraint tilde(SemanticVersion v) {
        VersionConstraint c;
        c.type = Type::Tilde;
        c.version = std::move(v);
        return c;
    }

    /// Create range constraint
    [[nodiscard]] static VersionConstraint range(
        SemanticVersion min, bool min_incl,
        SemanticVersion max, bool max_incl) {
        VersionConstraint c;
        c.type = Type::Range;
        c.min_version = std::move(min);
        c.max_version = std::move(max);
        c.min_inclusive = min_incl;
        c.max_inclusive = max_incl;
        return c;
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

/// Check if two versions are compatible (same major, v2 >= v1)
/// For 0.x versions, same minor required
[[nodiscard]] bool versions_compatible(
    const SemanticVersion& required,
    const SemanticVersion& available) noexcept;

/// Get the next breaking version (for caret bounds)
/// 1.2.3 -> 2.0.0
/// 0.2.3 -> 0.3.0
/// 0.0.3 -> 0.0.4
[[nodiscard]] SemanticVersion next_breaking_version(const SemanticVersion& v) noexcept;

/// Get the next minor version (for tilde bounds)
/// 1.2.3 -> 1.3.0
[[nodiscard]] SemanticVersion next_minor_version(const SemanticVersion& v) noexcept;

} // namespace void_package
