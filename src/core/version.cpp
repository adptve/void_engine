/// @file version.cpp
/// @brief Version utilities implementation for void_core
///
/// This file provides:
/// - Version comparison and compatibility utilities
/// - Version serialization support
/// - Version string formatting and parsing extensions

#include <void_engine/core/version.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <regex>

namespace void_core {

// =============================================================================
// Version Constants
// =============================================================================

/// void_core module version
static constexpr Version VOID_CORE_VERSION = Version{1, 0, 0};

/// Get the void_core module version
Version void_core_version() {
    return VOID_CORE_VERSION;
}

// =============================================================================
// Extended Version Parsing
// =============================================================================

/// Parse version with optional prerelease/build metadata
/// Format: major.minor.patch[-prerelease][+build]
Result<Version> parse_version_extended(const std::string& str) {
    // Try simple parse first
    auto simple = Version::parse(str);
    if (simple.has_value()) {
        return Ok(simple.value());
    }

    // Try to parse with prerelease/build suffix
    std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+)(?:-[a-zA-Z0-9.]+)?(?:\+[a-zA-Z0-9.]+)?$)");
    std::smatch match;

    if (!std::regex_match(str, match, version_regex)) {
        return Err<Version>(Error(ErrorCode::ParseError, "Invalid version format: " + str));
    }

    try {
        std::uint16_t major = static_cast<std::uint16_t>(std::stoul(match[1].str()));
        std::uint16_t minor = static_cast<std::uint16_t>(std::stoul(match[2].str()));
        std::uint16_t patch = static_cast<std::uint16_t>(std::stoul(match[3].str()));
        return Ok(Version{major, minor, patch});
    } catch (...) {
        return Err<Version>(Error(ErrorCode::ParseError, "Version number overflow: " + str));
    }
}

/// Parse version range (e.g., ">=1.0.0,<2.0.0")
struct VersionRange {
    Version min_version;
    Version max_version;
    bool min_inclusive = true;
    bool max_inclusive = false;
    bool has_min = false;
    bool has_max = false;

    /// Check if a version is within this range
    [[nodiscard]] bool contains(const Version& v) const {
        if (has_min) {
            if (min_inclusive) {
                if (v < min_version) return false;
            } else {
                if (v <= min_version) return false;
            }
        }

        if (has_max) {
            if (max_inclusive) {
                if (v > max_version) return false;
            } else {
                if (v >= max_version) return false;
            }
        }

        return true;
    }
};

/// Parse a version range string
Result<VersionRange> parse_version_range(const std::string& str) {
    VersionRange range;

    // Split by comma
    std::vector<std::string> parts;
    std::istringstream iss(str);
    std::string part;
    while (std::getline(iss, part, ',')) {
        // Trim whitespace
        std::size_t start = part.find_first_not_of(" \t");
        std::size_t end = part.find_last_not_of(" \t");
        if (start != std::string::npos) {
            parts.push_back(part.substr(start, end - start + 1));
        }
    }

    for (const auto& p : parts) {
        if (p.starts_with(">=")) {
            auto ver = Version::parse(p.substr(2));
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.min_version = ver.value();
            range.min_inclusive = true;
            range.has_min = true;
        } else if (p.starts_with(">")) {
            auto ver = Version::parse(p.substr(1));
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.min_version = ver.value();
            range.min_inclusive = false;
            range.has_min = true;
        } else if (p.starts_with("<=")) {
            auto ver = Version::parse(p.substr(2));
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.max_version = ver.value();
            range.max_inclusive = true;
            range.has_max = true;
        } else if (p.starts_with("<")) {
            auto ver = Version::parse(p.substr(1));
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.max_version = ver.value();
            range.max_inclusive = false;
            range.has_max = true;
        } else if (p.starts_with("=") || p.starts_with("==")) {
            std::string ver_str = p.starts_with("==") ? p.substr(2) : p.substr(1);
            auto ver = Version::parse(ver_str);
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.min_version = ver.value();
            range.max_version = ver.value();
            range.min_inclusive = true;
            range.max_inclusive = true;
            range.has_min = true;
            range.has_max = true;
        } else if (p.starts_with("^")) {
            // Caret: compatible with major version
            auto ver = Version::parse(p.substr(1));
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.min_version = ver.value();
            range.max_version = ver.value().increment_major();
            range.min_inclusive = true;
            range.max_inclusive = false;
            range.has_min = true;
            range.has_max = true;
        } else if (p.starts_with("~")) {
            // Tilde: compatible with minor version
            auto ver = Version::parse(p.substr(1));
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.min_version = ver.value();
            range.max_version = ver.value().increment_minor();
            range.min_inclusive = true;
            range.max_inclusive = false;
            range.has_min = true;
            range.has_max = true;
        } else {
            // Plain version - exact match
            auto ver = Version::parse(p);
            if (!ver.has_value()) {
                return Err<VersionRange>(Error(ErrorCode::ParseError, "Invalid version in range: " + p));
            }
            range.min_version = ver.value();
            range.max_version = ver.value();
            range.min_inclusive = true;
            range.max_inclusive = true;
            range.has_min = true;
            range.has_max = true;
        }
    }

    return Ok(std::move(range));
}

// =============================================================================
// Version Serialization
// =============================================================================

namespace serialization {

/// Binary serialization constants for Version
namespace version_binary {
    constexpr std::uint32_t MAGIC = 0x56455253;  // "VERS"
    constexpr std::uint32_t VERSION = 1;
}

/// Serialize a Version to binary
std::vector<std::uint8_t> serialize_version(const Version& version) {
    std::vector<std::uint8_t> data(sizeof(std::uint32_t) * 2 + sizeof(std::uint64_t));

    auto* ptr = data.data();

    // Magic
    std::memcpy(ptr, &version_binary::MAGIC, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Serialization version
    std::memcpy(ptr, &version_binary::VERSION, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Version as u64
    std::uint64_t bits = version.to_u64();
    std::memcpy(ptr, &bits, sizeof(std::uint64_t));

    return data;
}

/// Deserialize a Version from binary
Result<Version> deserialize_version(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 2 + sizeof(std::uint64_t)) {
        return Err<Version>(Error(ErrorCode::ParseError, "Version data too short"));
    }

    const auto* ptr = data.data();

    // Verify magic
    std::uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (magic != version_binary::MAGIC) {
        return Err<Version>(Error(ErrorCode::ParseError, "Invalid version magic"));
    }

    // Verify serialization version
    std::uint32_t ser_version;
    std::memcpy(&ser_version, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (ser_version != version_binary::VERSION) {
        return Err<Version>(Error(ErrorCode::IncompatibleVersion, "Unsupported version serialization format"));
    }

    // Read version bits
    std::uint64_t bits;
    std::memcpy(&bits, ptr, sizeof(std::uint64_t));

    return Ok(Version::from_u64(bits));
}

} // namespace serialization

// =============================================================================
// Version Formatting
// =============================================================================

/// Format version with optional prefix
std::string format_version(const Version& version, const std::string& prefix) {
    std::ostringstream oss;
    if (!prefix.empty()) {
        oss << prefix;
    }
    oss << version.major << '.' << version.minor << '.' << version.patch;
    return oss.str();
}

/// Format version with "v" prefix
std::string format_version_prefixed(const Version& version) {
    return format_version(version, "v");
}

// =============================================================================
// Version Comparison Utilities
// =============================================================================

/// Compare two versions and return detailed result
struct VersionComparison {
    int major_diff = 0;
    int minor_diff = 0;
    int patch_diff = 0;
    bool is_major_change = false;
    bool is_minor_change = false;
    bool is_patch_change = false;
    bool is_upgrade = false;
    bool is_downgrade = false;
    bool is_equal = false;
};

/// Compare two versions in detail
VersionComparison compare_versions(const Version& from, const Version& to) {
    VersionComparison result;

    result.major_diff = static_cast<int>(to.major) - static_cast<int>(from.major);
    result.minor_diff = static_cast<int>(to.minor) - static_cast<int>(from.minor);
    result.patch_diff = static_cast<int>(to.patch) - static_cast<int>(from.patch);

    result.is_major_change = result.major_diff != 0;
    result.is_minor_change = !result.is_major_change && result.minor_diff != 0;
    result.is_patch_change = !result.is_major_change && !result.is_minor_change && result.patch_diff != 0;

    if (to > from) {
        result.is_upgrade = true;
    } else if (to < from) {
        result.is_downgrade = true;
    } else {
        result.is_equal = true;
    }

    return result;
}

/// Format version comparison as human-readable string
std::string format_version_comparison(const Version& from, const Version& to) {
    auto cmp = compare_versions(from, to);
    std::ostringstream oss;

    oss << from.to_string() << " -> " << to.to_string() << ": ";

    if (cmp.is_equal) {
        oss << "no change";
    } else if (cmp.is_upgrade) {
        if (cmp.is_major_change) {
            oss << "MAJOR upgrade (breaking changes expected)";
        } else if (cmp.is_minor_change) {
            oss << "minor upgrade (new features)";
        } else {
            oss << "patch upgrade (bug fixes)";
        }
    } else {
        if (cmp.is_major_change) {
            oss << "MAJOR downgrade (compatibility unknown)";
        } else if (cmp.is_minor_change) {
            oss << "minor downgrade";
        } else {
            oss << "patch downgrade";
        }
    }

    return oss.str();
}

// =============================================================================
// Build Information
// =============================================================================

namespace build {

/// Build configuration
struct BuildInfo {
    const char* version;
    const char* build_date;
    const char* build_type;
    const char* compiler;
    const char* platform;
};

/// Get build information
BuildInfo get_build_info() {
    return BuildInfo{
        .version = "1.0.0",
        .build_date = __DATE__ " " __TIME__,
#ifdef NDEBUG
        .build_type = "Release",
#else
        .build_type = "Debug",
#endif
#if defined(__clang__)
        .compiler = "Clang " __clang_version__,
#elif defined(__GNUC__)
        .compiler = "GCC " __VERSION__,
#elif defined(_MSC_VER)
        .compiler = "MSVC",
#else
        .compiler = "Unknown",
#endif
#if defined(_WIN32)
        .platform = "Windows",
#elif defined(__APPLE__)
        .platform = "macOS",
#elif defined(__linux__)
        .platform = "Linux",
#else
        .platform = "Unknown",
#endif
    };
}

/// Format build information
std::string format_build_info() {
    auto info = get_build_info();
    std::ostringstream oss;
    oss << "void_core " << info.version << "\n"
        << "  Built: " << info.build_date << "\n"
        << "  Type: " << info.build_type << "\n"
        << "  Compiler: " << info.compiler << "\n"
        << "  Platform: " << info.platform << "\n";
    return oss.str();
}

} // namespace build

} // namespace void_core
