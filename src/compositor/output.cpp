/// @file output.cpp
/// @brief Display output management implementation
///
/// Provides output configuration, mode selection, and multi-display utilities.

#include <void_engine/compositor/output.hpp>
#include <void_engine/compositor/vrr.hpp>
#include <void_engine/compositor/hdr.hpp>
#include <void_engine/compositor/types.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace void_compositor {

// =============================================================================
// Output Mode Utilities
// =============================================================================

/// Find the best matching mode for a target resolution
[[nodiscard]] const OutputMode* find_best_mode(
    const std::vector<OutputMode>& modes,
    std::uint32_t target_width,
    std::uint32_t target_height,
    std::uint32_t target_refresh_hz) {

    if (modes.empty()) {
        return nullptr;
    }

    const OutputMode* best = nullptr;
    std::int32_t best_score = std::numeric_limits<std::int32_t>::min();

    for (const auto& mode : modes) {
        // Score based on how close the mode is to target
        std::int32_t score = 0;

        // Resolution matching (prefer exact, penalize larger/smaller)
        std::int32_t width_diff = static_cast<std::int32_t>(mode.width) -
                                  static_cast<std::int32_t>(target_width);
        std::int32_t height_diff = static_cast<std::int32_t>(mode.height) -
                                   static_cast<std::int32_t>(target_height);

        // Exact match gets bonus
        if (width_diff == 0 && height_diff == 0) {
            score += 1000;
        } else {
            // Penalize based on pixel difference
            score -= std::abs(width_diff) + std::abs(height_diff);
        }

        // Refresh rate matching
        std::int32_t refresh_diff = static_cast<std::int32_t>(mode.refresh_hz()) -
                                    static_cast<std::int32_t>(target_refresh_hz);
        if (refresh_diff == 0) {
            score += 500;
        } else {
            score -= std::abs(refresh_diff) * 10;
        }

        // Prefer higher refresh rates when resolution matches
        if (mode.width == target_width && mode.height == target_height) {
            score += static_cast<std::int32_t>(mode.refresh_hz());
        }

        if (score > best_score) {
            best_score = score;
            best = &mode;
        }
    }

    return best;
}

/// Find the highest refresh rate mode at a given resolution
[[nodiscard]] const OutputMode* find_highest_refresh_mode(
    const std::vector<OutputMode>& modes,
    std::uint32_t width,
    std::uint32_t height) {

    const OutputMode* best = nullptr;
    std::uint32_t best_refresh = 0;

    for (const auto& mode : modes) {
        if (mode.width == width && mode.height == height) {
            if (mode.refresh_mhz > best_refresh) {
                best_refresh = mode.refresh_mhz;
                best = &mode;
            }
        }
    }

    return best;
}

/// Find the highest resolution mode
[[nodiscard]] const OutputMode* find_highest_resolution_mode(
    const std::vector<OutputMode>& modes,
    std::uint32_t min_refresh_hz) {

    const OutputMode* best = nullptr;
    std::uint64_t best_pixels = 0;

    for (const auto& mode : modes) {
        if (mode.refresh_hz() >= min_refresh_hz) {
            std::uint64_t pixels = static_cast<std::uint64_t>(mode.width) *
                                   static_cast<std::uint64_t>(mode.height);
            if (pixels > best_pixels) {
                best_pixels = pixels;
                best = &mode;
            }
        }
    }

    return best;
}

/// Get all unique resolutions from modes
[[nodiscard]] std::vector<std::pair<std::uint32_t, std::uint32_t>> get_unique_resolutions(
    const std::vector<OutputMode>& modes) {

    std::vector<std::pair<std::uint32_t, std::uint32_t>> resolutions;

    for (const auto& mode : modes) {
        auto res = std::make_pair(mode.width, mode.height);
        if (std::find(resolutions.begin(), resolutions.end(), res) == resolutions.end()) {
            resolutions.push_back(res);
        }
    }

    // Sort by pixel count (descending)
    std::sort(resolutions.begin(), resolutions.end(),
        [](const auto& a, const auto& b) {
            return (a.first * a.second) > (b.first * b.second);
        });

    return resolutions;
}

/// Get all unique refresh rates at a resolution
[[nodiscard]] std::vector<std::uint32_t> get_refresh_rates_at_resolution(
    const std::vector<OutputMode>& modes,
    std::uint32_t width,
    std::uint32_t height) {

    std::vector<std::uint32_t> rates;

    for (const auto& mode : modes) {
        if (mode.width == width && mode.height == height) {
            std::uint32_t hz = mode.refresh_hz();
            if (std::find(rates.begin(), rates.end(), hz) == rates.end()) {
                rates.push_back(hz);
            }
        }
    }

    // Sort descending
    std::sort(rates.begin(), rates.end(), std::greater<std::uint32_t>());

    return rates;
}

// =============================================================================
// Output Info Utilities
// =============================================================================

/// Format output info as string
[[nodiscard]] std::string format_output_info(const OutputInfo& info) {
    std::ostringstream ss;

    ss << "Output: " << info.name;
    if (info.primary) {
        ss << " (PRIMARY)";
    }
    ss << "\n";

    ss << "  Mode: " << info.current_mode.to_string() << "\n";

    if (info.physical_size) {
        ss << "  Size: " << info.physical_size->first << "x"
           << info.physical_size->second << " mm";
        if (auto dpi = info.dpi()) {
            ss << " (" << static_cast<int>(*dpi) << " DPI)";
        }
        ss << "\n";
    }

    ss << "  Position: " << info.position_x << ", " << info.position_y << "\n";
    ss << "  Scale: " << info.scale << "x\n";
    ss << "  Transform: " << to_string(info.transform) << "\n";
    ss << "  Aspect: " << info.aspect_ratio_string() << "\n";

    if (!info.manufacturer.empty()) {
        ss << "  Manufacturer: " << info.manufacturer << "\n";
    }
    if (!info.model.empty()) {
        ss << "  Model: " << info.model << "\n";
    }

    ss << "  Available modes: " << info.available_modes.size() << "\n";

    return ss.str();
}

/// Calculate effective resolution after scaling
[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> get_scaled_resolution(
    const OutputInfo& info) {

    std::uint32_t scaled_width = static_cast<std::uint32_t>(
        static_cast<float>(info.current_mode.width) / info.scale);
    std::uint32_t scaled_height = static_cast<std::uint32_t>(
        static_cast<float>(info.current_mode.height) / info.scale);

    return {scaled_width, scaled_height};
}

/// Calculate output bounds after transform
[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::uint32_t, std::uint32_t>
get_output_bounds(const OutputInfo& info) {

    std::uint32_t width = info.current_mode.width;
    std::uint32_t height = info.current_mode.height;

    // Apply transform to dimensions
    switch (info.transform) {
        case OutputTransform::Normal:
        case OutputTransform::Flipped:
            break;
        case OutputTransform::Rotate90:
        case OutputTransform::Rotate270:
        case OutputTransform::FlippedRotate90:
        case OutputTransform::FlippedRotate270:
            std::swap(width, height);
            break;
        case OutputTransform::Rotate180:
        case OutputTransform::FlippedRotate180:
            break;
    }

    // Apply scale
    auto [scaled_w, scaled_h] = get_scaled_resolution(info);

    return {info.position_x, info.position_y, scaled_w, scaled_h};
}

// =============================================================================
// Multi-Output Layout
// =============================================================================

/// Output layout configuration
struct OutputLayout {
    std::vector<const OutputInfo*> outputs;

    /// Get total virtual screen bounds
    [[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::uint32_t, std::uint32_t>
    get_virtual_bounds() const {
        if (outputs.empty()) {
            return {0, 0, 0, 0};
        }

        std::int32_t min_x = std::numeric_limits<std::int32_t>::max();
        std::int32_t min_y = std::numeric_limits<std::int32_t>::max();
        std::int32_t max_x = std::numeric_limits<std::int32_t>::min();
        std::int32_t max_y = std::numeric_limits<std::int32_t>::min();

        for (const auto* output : outputs) {
            auto [x, y, w, h] = get_output_bounds(*output);
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x + static_cast<std::int32_t>(w));
            max_y = std::max(max_y, y + static_cast<std::int32_t>(h));
        }

        return {min_x, min_y,
                static_cast<std::uint32_t>(max_x - min_x),
                static_cast<std::uint32_t>(max_y - min_y)};
    }

    /// Find output containing a point
    [[nodiscard]] const OutputInfo* find_at_point(std::int32_t px, std::int32_t py) const {
        for (const auto* output : outputs) {
            auto [x, y, w, h] = get_output_bounds(*output);
            if (px >= x && px < x + static_cast<std::int32_t>(w) &&
                py >= y && py < y + static_cast<std::int32_t>(h)) {
                return output;
            }
        }
        return nullptr;
    }

    /// Find primary output
    [[nodiscard]] const OutputInfo* find_primary() const {
        for (const auto* output : outputs) {
            if (output->primary) {
                return output;
            }
        }
        return outputs.empty() ? nullptr : outputs[0];
    }
};

/// Create layout from outputs
[[nodiscard]] OutputLayout create_layout(const std::vector<IOutput*>& outputs) {
    OutputLayout layout;
    layout.outputs.reserve(outputs.size());
    for (auto* output : outputs) {
        layout.outputs.push_back(&output->info());
    }
    return layout;
}

// =============================================================================
// Output Selection
// =============================================================================

/// Find output best suited for gaming (highest refresh, low latency)
[[nodiscard]] IOutput* find_gaming_output(const std::vector<IOutput*>& outputs) {
    IOutput* best = nullptr;
    std::uint32_t best_refresh = 0;
    bool best_has_vrr = false;

    for (auto* output : outputs) {
        bool has_vrr = output->vrr_capability().supported;
        std::uint32_t refresh = output->info().current_mode.refresh_hz();

        // Prefer VRR-capable outputs
        if (has_vrr && !best_has_vrr) {
            best = output;
            best_refresh = refresh;
            best_has_vrr = true;
        } else if (has_vrr == best_has_vrr && refresh > best_refresh) {
            best = output;
            best_refresh = refresh;
        }
    }

    return best;
}

/// Find output best suited for HDR content
[[nodiscard]] IOutput* find_hdr_output(const std::vector<IOutput*>& outputs) {
    IOutput* best = nullptr;
    std::uint32_t best_luminance = 0;

    for (auto* output : outputs) {
        const auto& cap = output->hdr_capability();
        if (cap.supported) {
            std::uint32_t lum = cap.max_luminance.value_or(0);
            if (lum > best_luminance) {
                best = output;
                best_luminance = lum;
            }
        }
    }

    return best;
}

/// Find output by name
[[nodiscard]] IOutput* find_output_by_name(
    const std::vector<IOutput*>& outputs,
    const std::string& name) {

    for (auto* output : outputs) {
        if (output->info().name == name) {
            return output;
        }
    }
    return nullptr;
}

// =============================================================================
// Output Transform Utilities
// =============================================================================

/// Check if transform involves rotation
[[nodiscard]] bool transform_is_rotated(OutputTransform transform) {
    switch (transform) {
        case OutputTransform::Rotate90:
        case OutputTransform::Rotate270:
        case OutputTransform::FlippedRotate90:
        case OutputTransform::FlippedRotate270:
            return true;
        default:
            return false;
    }
}

/// Check if transform involves flipping
[[nodiscard]] bool transform_is_flipped(OutputTransform transform) {
    switch (transform) {
        case OutputTransform::Flipped:
        case OutputTransform::FlippedRotate90:
        case OutputTransform::FlippedRotate180:
        case OutputTransform::FlippedRotate270:
            return true;
        default:
            return false;
    }
}

/// Get rotation angle in degrees
[[nodiscard]] int transform_rotation_degrees(OutputTransform transform) {
    switch (transform) {
        case OutputTransform::Normal:
        case OutputTransform::Flipped:
            return 0;
        case OutputTransform::Rotate90:
        case OutputTransform::FlippedRotate90:
            return 90;
        case OutputTransform::Rotate180:
        case OutputTransform::FlippedRotate180:
            return 180;
        case OutputTransform::Rotate270:
        case OutputTransform::FlippedRotate270:
            return 270;
    }
    return 0;
}

/// Combine two transforms
[[nodiscard]] OutputTransform combine_transforms(OutputTransform a, OutputTransform b) {
    // Simplified combination - full implementation would handle all cases
    int rot_a = transform_rotation_degrees(a);
    int rot_b = transform_rotation_degrees(b);
    int combined_rot = (rot_a + rot_b) % 360;

    bool flipped = transform_is_flipped(a) != transform_is_flipped(b);

    if (flipped) {
        switch (combined_rot) {
            case 0: return OutputTransform::Flipped;
            case 90: return OutputTransform::FlippedRotate90;
            case 180: return OutputTransform::FlippedRotate180;
            case 270: return OutputTransform::FlippedRotate270;
        }
    } else {
        switch (combined_rot) {
            case 0: return OutputTransform::Normal;
            case 90: return OutputTransform::Rotate90;
            case 180: return OutputTransform::Rotate180;
            case 270: return OutputTransform::Rotate270;
        }
    }

    return OutputTransform::Normal;
}

} // namespace void_compositor
