/// @file version.cpp
/// @brief Semantic versioning implementation

#include <void_engine/package/version.hpp>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <cctype>

namespace void_package {

// =============================================================================
// SemanticVersion Implementation
// =============================================================================

void_core::Result<SemanticVersion> SemanticVersion::parse(std::string_view str) {
    // Trim whitespace
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }

    if (str.empty()) {
        return void_core::Err<SemanticVersion>(
            void_core::Error(void_core::ErrorCode::ParseError, "Empty version string"));
    }

    // Skip optional 'v' prefix
    if (str.front() == 'v' || str.front() == 'V') {
        str.remove_prefix(1);
    }

    SemanticVersion result;

    // Find positions of separators
    auto plus_pos = str.find('+');
    auto dash_pos = str.find('-');

    // Extract core version string (before - or +)
    std::string_view core_str = str;
    if (dash_pos != std::string_view::npos) {
        core_str = str.substr(0, dash_pos);
    } else if (plus_pos != std::string_view::npos) {
        core_str = str.substr(0, plus_pos);
    }

    // Parse major.minor.patch
    std::size_t start = 0;
    int part_index = 0;
    std::uint32_t parts[3] = {0, 0, 0};

    for (std::size_t i = 0; i <= core_str.size() && part_index < 3; ++i) {
        if (i == core_str.size() || core_str[i] == '.') {
            if (i > start) {
                std::string_view num_str = core_str.substr(start, i - start);
                std::uint32_t value = 0;
                auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
                if (ec != std::errc{} || ptr != num_str.data() + num_str.size()) {
                    return void_core::Err<SemanticVersion>(
                        void_core::Error(void_core::ErrorCode::ParseError,
                            "Invalid version number component: " + std::string(num_str)));
                }
                parts[part_index++] = value;
            }
            start = i + 1;
        }
    }

    if (part_index == 0) {
        return void_core::Err<SemanticVersion>(
            void_core::Error(void_core::ErrorCode::ParseError,
                "No version numbers found: " + std::string(str)));
    }

    result.major = parts[0];
    result.minor = parts[1];
    result.patch = parts[2];

    // Extract prerelease if present
    if (dash_pos != std::string_view::npos) {
        std::size_t prerelease_end = plus_pos != std::string_view::npos ? plus_pos : str.size();
        result.prerelease = std::string(str.substr(dash_pos + 1, prerelease_end - dash_pos - 1));

        // Validate prerelease (alphanumeric and dots only)
        for (char c : result.prerelease) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') {
                return void_core::Err<SemanticVersion>(
                    void_core::Error(void_core::ErrorCode::ParseError,
                        "Invalid character in prerelease: " + result.prerelease));
            }
        }
    }

    // Extract build metadata if present
    if (plus_pos != std::string_view::npos) {
        result.build_metadata = std::string(str.substr(plus_pos + 1));

        // Validate build metadata
        for (char c : result.build_metadata) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-') {
                return void_core::Err<SemanticVersion>(
                    void_core::Error(void_core::ErrorCode::ParseError,
                        "Invalid character in build metadata: " + result.build_metadata));
            }
        }
    }

    return void_core::Ok(result);
}

std::strong_ordering SemanticVersion::operator<=>(const SemanticVersion& other) const noexcept {
    // Compare major.minor.patch
    if (auto cmp = major <=> other.major; cmp != 0) return cmp;
    if (auto cmp = minor <=> other.minor; cmp != 0) return cmp;
    if (auto cmp = patch <=> other.patch; cmp != 0) return cmp;

    // Prerelease comparison
    // A version with prerelease has LOWER precedence than one without
    if (prerelease.empty() && !other.prerelease.empty()) {
        return std::strong_ordering::greater;
    }
    if (!prerelease.empty() && other.prerelease.empty()) {
        return std::strong_ordering::less;
    }
    if (!prerelease.empty() && !other.prerelease.empty()) {
        return compare_prerelease(prerelease, other.prerelease);
    }

    // Build metadata is ignored in comparison
    return std::strong_ordering::equal;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const noexcept {
    return major == other.major &&
           minor == other.minor &&
           patch == other.patch &&
           prerelease == other.prerelease;
    // Build metadata is ignored in equality
}

std::string SemanticVersion::to_string() const {
    std::ostringstream oss;
    oss << major << '.' << minor << '.' << patch;
    if (!prerelease.empty()) {
        oss << '-' << prerelease;
    }
    if (!build_metadata.empty()) {
        oss << '+' << build_metadata;
    }
    return oss.str();
}

std::string SemanticVersion::to_string_core() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::strong_ordering SemanticVersion::compare_prerelease(
    std::string_view a, std::string_view b) noexcept {

    // Split by dots and compare identifier by identifier
    std::size_t a_start = 0, b_start = 0;

    while (a_start < a.size() || b_start < b.size()) {
        // Find next identifier
        std::size_t a_end = a.find('.', a_start);
        if (a_end == std::string_view::npos) a_end = a.size();
        std::size_t b_end = b.find('.', b_start);
        if (b_end == std::string_view::npos) b_end = b.size();

        std::string_view a_id = a.substr(a_start, a_end - a_start);
        std::string_view b_id = b.substr(b_start, b_end - b_start);

        // Empty identifier means we've run out
        if (a_id.empty() && !b_id.empty()) {
            return std::strong_ordering::less;  // Fewer identifiers = lower precedence
        }
        if (!a_id.empty() && b_id.empty()) {
            return std::strong_ordering::greater;
        }
        if (a_id.empty() && b_id.empty()) {
            break;
        }

        // Check if both are numeric
        bool a_numeric = std::all_of(a_id.begin(), a_id.end(),
            [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
        bool b_numeric = std::all_of(b_id.begin(), b_id.end(),
            [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });

        if (a_numeric && b_numeric) {
            // Compare as numbers
            std::uint64_t a_num = 0, b_num = 0;
            std::from_chars(a_id.data(), a_id.data() + a_id.size(), a_num);
            std::from_chars(b_id.data(), b_id.data() + b_id.size(), b_num);
            if (auto cmp = a_num <=> b_num; cmp != 0) return cmp;
        } else if (a_numeric) {
            // Numeric has lower precedence than alphanumeric
            return std::strong_ordering::less;
        } else if (b_numeric) {
            return std::strong_ordering::greater;
        } else {
            // Compare lexically
            if (auto cmp = a_id.compare(b_id); cmp != 0) {
                return cmp < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
            }
        }

        a_start = a_end + 1;
        b_start = b_end + 1;
    }

    return std::strong_ordering::equal;
}

// =============================================================================
// VersionConstraint Implementation
// =============================================================================

void_core::Result<VersionConstraint> VersionConstraint::parse(std::string_view str) {
    // Trim whitespace
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }

    // Handle empty or "*" as any
    if (str.empty() || str == "*") {
        return void_core::Ok(VersionConstraint::any());
    }

    // Check for comma-separated constraints (range)
    if (str.find(',') != std::string_view::npos) {
        VersionConstraint result;
        result.type = VersionConstraint::Type::Range;

        std::size_t start = 0;
        while (start < str.size()) {
            std::size_t end = str.find(',', start);
            if (end == std::string_view::npos) end = str.size();

            std::string_view part = str.substr(start, end - start);
            // Trim
            while (!part.empty() && std::isspace(static_cast<unsigned char>(part.front()))) {
                part.remove_prefix(1);
            }
            while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back()))) {
                part.remove_suffix(1);
            }

            if (!part.empty()) {
                auto sub_result = parse(part);
                if (!sub_result) {
                    return sub_result;
                }
                result.sub_constraints.push_back(std::move(*sub_result));
            }

            start = end + 1;
        }

        // Convert simple range constraints to min/max form
        if (result.sub_constraints.size() == 2) {
            auto& c1 = result.sub_constraints[0];
            auto& c2 = result.sub_constraints[1];

            bool c1_is_min = (c1.type == Type::Greater || c1.type == Type::GreaterEqual);
            bool c2_is_max = (c2.type == Type::Less || c2.type == Type::LessEqual);

            if (c1_is_min && c2_is_max) {
                result.min_version = c1.version;
                result.min_inclusive = (c1.type == Type::GreaterEqual);
                result.max_version = c2.version;
                result.max_inclusive = (c2.type == Type::LessEqual);
            }
        }

        return void_core::Ok(result);
    }

    VersionConstraint result;

    // Check for operators
    if (str.starts_with(">=")) {
        result.type = Type::GreaterEqual;
        str.remove_prefix(2);
    } else if (str.starts_with(">")) {
        result.type = Type::Greater;
        str.remove_prefix(1);
    } else if (str.starts_with("<=")) {
        result.type = Type::LessEqual;
        str.remove_prefix(2);
    } else if (str.starts_with("<")) {
        result.type = Type::Less;
        str.remove_prefix(1);
    } else if (str.starts_with("^")) {
        result.type = Type::Caret;
        str.remove_prefix(1);
    } else if (str.starts_with("~")) {
        result.type = Type::Tilde;
        str.remove_prefix(1);
    } else if (str.starts_with("==") || str.starts_with("=")) {
        result.type = Type::Exact;
        str.remove_prefix(str.starts_with("==") ? 2 : 1);
    } else {
        // No operator - treat as exact or detect wildcards
        if (str.find('x') != std::string_view::npos || str.find('*') != std::string_view::npos) {
            // Wildcard - convert to range
            result.type = Type::Range;

            // Replace wildcards with 0 for min
            std::string min_str(str);
            std::replace(min_str.begin(), min_str.end(), 'x', '0');
            std::replace(min_str.begin(), min_str.end(), '*', '0');

            auto min_result = SemanticVersion::parse(min_str);
            if (!min_result) {
                return void_core::Err<VersionConstraint>(min_result.error());
            }
            result.min_version = *min_result;
            result.min_inclusive = true;

            // For max, increment the last non-wildcard component
            if (str.find('.') == std::string_view::npos) {
                // Just "1.x" - allow any 1.anything
                result.max_version = result.min_version.increment_major();
            } else if (str.rfind('x') > str.find('.') || str.rfind('*') > str.find('.')) {
                // "1.2.x" - allow 1.2.anything
                result.max_version = result.min_version.increment_minor();
            } else {
                result.max_version = result.min_version.increment_patch();
            }
            result.max_inclusive = false;

            return void_core::Ok(result);
        }
        result.type = Type::Exact;
    }

    // Trim whitespace after operator
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }

    // Parse version
    auto ver_result = SemanticVersion::parse(str);
    if (!ver_result) {
        return void_core::Err<VersionConstraint>(ver_result.error());
    }
    result.version = std::move(*ver_result);

    return void_core::Ok(result);
}

bool VersionConstraint::satisfies(const SemanticVersion& v) const noexcept {
    switch (type) {
        case Type::Any:
            return true;

        case Type::Exact:
            return v == version;

        case Type::Greater:
            return v > version;

        case Type::GreaterEqual:
            return v >= version;

        case Type::Less:
            return v < version;

        case Type::LessEqual:
            return v <= version;

        case Type::Caret: {
            // ^1.2.3 means >=1.2.3 and <2.0.0 (for major > 0)
            // ^0.2.3 means >=0.2.3 and <0.3.0 (for major == 0)
            // ^0.0.3 means >=0.0.3 and <0.0.4 (for major == 0 && minor == 0)
            if (v < version) return false;
            auto upper = next_breaking_version(version);
            return v < upper;
        }

        case Type::Tilde: {
            // ~1.2.3 means >=1.2.3 and <1.3.0
            if (v < version) return false;
            auto upper = next_minor_version(version);
            return v < upper;
        }

        case Type::Range: {
            // Check sub-constraints if present
            if (!sub_constraints.empty()) {
                for (const auto& sub : sub_constraints) {
                    if (!sub.satisfies(v)) {
                        return false;
                    }
                }
                return true;
            }

            // Check min/max bounds
            if (min_inclusive) {
                if (v < min_version) return false;
            } else {
                if (v <= min_version) return false;
            }
            if (max_inclusive) {
                if (v > max_version) return false;
            } else {
                if (v >= max_version) return false;
            }
            return true;
        }
    }

    return false;
}

std::string VersionConstraint::to_string() const {
    switch (type) {
        case Type::Any:
            return "*";
        case Type::Exact:
            return version.to_string();
        case Type::Greater:
            return ">" + version.to_string();
        case Type::GreaterEqual:
            return ">=" + version.to_string();
        case Type::Less:
            return "<" + version.to_string();
        case Type::LessEqual:
            return "<=" + version.to_string();
        case Type::Caret:
            return "^" + version.to_string();
        case Type::Tilde:
            return "~" + version.to_string();
        case Type::Range: {
            if (!sub_constraints.empty()) {
                std::string result;
                for (std::size_t i = 0; i < sub_constraints.size(); ++i) {
                    if (i > 0) result += ",";
                    result += sub_constraints[i].to_string();
                }
                return result;
            }
            std::string result;
            result += min_inclusive ? ">=" : ">";
            result += min_version.to_string();
            result += ",";
            result += max_inclusive ? "<=" : "<";
            result += max_version.to_string();
            return result;
        }
    }
    return "?";
}

// =============================================================================
// Utility Functions
// =============================================================================

bool versions_compatible(
    const SemanticVersion& required,
    const SemanticVersion& available) noexcept {

    // For 0.x.x versions, minor must match
    if (required.major == 0 && available.major == 0) {
        return required.minor == available.minor && available.patch >= required.patch;
    }

    // For 1.0.0+, major must match and available >= required
    return required.major == available.major && available >= required;
}

SemanticVersion next_breaking_version(const SemanticVersion& v) noexcept {
    if (v.major > 0) {
        return SemanticVersion{v.major + 1, 0, 0};
    }
    if (v.minor > 0) {
        return SemanticVersion{0, v.minor + 1, 0};
    }
    return SemanticVersion{0, 0, v.patch + 1};
}

SemanticVersion next_minor_version(const SemanticVersion& v) noexcept {
    return SemanticVersion{v.major, v.minor + 1, 0};
}

} // namespace void_package
