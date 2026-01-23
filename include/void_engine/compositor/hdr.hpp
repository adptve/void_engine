#pragma once

/// @file hdr.hpp
/// @brief High Dynamic Range (HDR) support
///
/// This module handles HDR detection, configuration, and metadata management.
/// Supports HDR10, HLG, and wide color gamut displays.

#include "fwd.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace void_compositor {

// =============================================================================
// Transfer Function (EOTF)
// =============================================================================

/// Transfer function (EOTF - Electro-Optical Transfer Function)
enum class TransferFunction : std::uint8_t {
    /// Standard Dynamic Range (sRGB/Rec.709)
    Sdr,
    /// Perceptual Quantizer (HDR10, HDR10+)
    Pq,
    /// Hybrid Log-Gamma (HLG broadcast)
    Hlg,
    /// Linear (for intermediate processing)
    Linear,
};

/// Get the SMPTE ST 2084 EOTF ID
[[nodiscard]] inline std::uint8_t eotf_id(TransferFunction tf) {
    switch (tf) {
        case TransferFunction::Sdr: return 0;      // Traditional gamma (SDR)
        case TransferFunction::Linear: return 1;   // Linear
        case TransferFunction::Pq: return 2;       // SMPTE ST 2084 (PQ)
        case TransferFunction::Hlg: return 3;      // ARIB STD-B67 (HLG)
    }
    return 0;
}

/// Get human-readable name
[[nodiscard]] inline const char* to_string(TransferFunction tf) {
    switch (tf) {
        case TransferFunction::Sdr: return "SDR";
        case TransferFunction::Pq: return "PQ (HDR10)";
        case TransferFunction::Hlg: return "HLG";
        case TransferFunction::Linear: return "Linear";
    }
    return "Unknown";
}

/// Check if this is an HDR transfer function
[[nodiscard]] inline bool is_hdr(TransferFunction tf) {
    return tf == TransferFunction::Pq || tf == TransferFunction::Hlg;
}

// =============================================================================
// Color Primaries
// =============================================================================

/// Color primaries (color gamut)
enum class ColorPrimaries : std::uint8_t {
    /// sRGB / Rec.709 (SDR standard)
    Srgb,
    /// DCI-P3 (digital cinema, common in HDR displays)
    DciP3,
    /// Rec.2020 (ultra-wide gamut, HDR standard)
    Rec2020,
    /// Adobe RGB (photography)
    AdobeRgb,
};

/// CIE 1931 xy coordinates for color primaries
struct CieXyCoordinates {
    float red_x, red_y;
    float green_x, green_y;
    float blue_x, blue_y;
    float white_x, white_y;
};

/// Get primaries as CIE 1931 xy coordinates
[[nodiscard]] inline CieXyCoordinates to_cie_xy(ColorPrimaries primaries) {
    switch (primaries) {
        case ColorPrimaries::Srgb:
            return {
                0.640f, 0.330f, // Red
                0.300f, 0.600f, // Green
                0.150f, 0.060f, // Blue
                0.3127f, 0.3290f, // White point (D65)
            };
        case ColorPrimaries::DciP3:
            return {
                0.680f, 0.320f, // Red
                0.265f, 0.690f, // Green
                0.150f, 0.060f, // Blue
                0.3127f, 0.3290f, // White point (D65)
            };
        case ColorPrimaries::Rec2020:
            return {
                0.708f, 0.292f, // Red
                0.170f, 0.797f, // Green
                0.131f, 0.046f, // Blue
                0.3127f, 0.3290f, // White point (D65)
            };
        case ColorPrimaries::AdobeRgb:
            return {
                0.640f, 0.330f, // Red
                0.210f, 0.710f, // Green
                0.150f, 0.060f, // Blue
                0.3127f, 0.3290f, // White point (D65)
            };
    }
    return {0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f, 0.3127f, 0.3290f};
}

/// Get human-readable name
[[nodiscard]] inline const char* to_string(ColorPrimaries primaries) {
    switch (primaries) {
        case ColorPrimaries::Srgb: return "sRGB/Rec.709";
        case ColorPrimaries::DciP3: return "DCI-P3";
        case ColorPrimaries::Rec2020: return "Rec.2020";
        case ColorPrimaries::AdobeRgb: return "Adobe RGB";
    }
    return "Unknown";
}

/// Get the color space ID (for DRM metadata)
[[nodiscard]] inline std::uint8_t color_space_id(ColorPrimaries primaries) {
    switch (primaries) {
        case ColorPrimaries::Srgb: return 0;
        case ColorPrimaries::DciP3: return 1;
        case ColorPrimaries::Rec2020: return 2;
        case ColorPrimaries::AdobeRgb: return 3;
    }
    return 0;
}

// =============================================================================
// HDR Metadata
// =============================================================================

/// HDR metadata structure (matches kernel DRM hdr_output_metadata)
struct HdrMetadata {
    /// Display primaries (red, green, blue) - x coordinates
    /// Values are in units of 0.00002
    std::array<std::uint16_t, 3> display_primaries_x;
    /// Display primaries (red, green, blue) - y coordinates
    std::array<std::uint16_t, 3> display_primaries_y;
    /// White point x coordinate
    std::uint16_t white_point_x;
    /// White point y coordinate
    std::uint16_t white_point_y;
    /// Maximum display mastering luminance (nits)
    std::uint32_t max_display_mastering_luminance;
    /// Minimum display mastering luminance (0.0001 nits)
    std::uint32_t min_display_mastering_luminance;
    /// Maximum content light level (nits)
    std::uint32_t max_content_light_level;
    /// Maximum frame-average light level (nits)
    std::uint32_t max_frame_average_light_level;
    /// EOTF (transfer function)
    std::uint8_t eotf;
};

// =============================================================================
// HDR Configuration
// =============================================================================

/// HDR configuration
struct HdrConfig {
    /// Is HDR enabled?
    bool enabled = false;
    /// Transfer function (tone curve)
    TransferFunction transfer_function = TransferFunction::Sdr;
    /// Color primaries
    ColorPrimaries color_primaries = ColorPrimaries::Srgb;
    /// Maximum luminance in nits
    std::uint32_t max_luminance = 100;
    /// Minimum luminance in nits
    float min_luminance = 0.0f;
    /// Maximum content light level (MaxCLL) in nits
    std::optional<std::uint32_t> max_content_light_level;
    /// Maximum frame average light level (MaxFALL) in nits
    std::optional<std::uint32_t> max_frame_average_light_level;

    /// Create an HDR10 configuration
    static HdrConfig hdr10(std::uint32_t max_nits) {
        return HdrConfig{
            .enabled = true,
            .transfer_function = TransferFunction::Pq,
            .color_primaries = ColorPrimaries::Rec2020,
            .max_luminance = max_nits,
            .min_luminance = 0.0001f,
            .max_content_light_level = max_nits,
            .max_frame_average_light_level = max_nits / 2,
        };
    }

    /// Create an HLG configuration
    static HdrConfig hlg(std::uint32_t max_nits) {
        return HdrConfig{
            .enabled = true,
            .transfer_function = TransferFunction::Hlg,
            .color_primaries = ColorPrimaries::Rec2020,
            .max_luminance = max_nits,
            .min_luminance = 0.0f,
            .max_content_light_level = std::nullopt,
            .max_frame_average_light_level = std::nullopt,
        };
    }

    /// Create an SDR configuration
    static HdrConfig sdr() {
        return HdrConfig{};
    }

    /// Enable HDR with the given transfer function
    void enable(TransferFunction tf) {
        enabled = true;
        transfer_function = tf;
        // Set appropriate color primaries for HDR
        if (is_hdr(tf)) {
            color_primaries = ColorPrimaries::Rec2020;
        }
    }

    /// Disable HDR (return to SDR)
    void disable() {
        enabled = false;
        transfer_function = TransferFunction::Sdr;
        color_primaries = ColorPrimaries::Srgb;
    }

    /// Check if HDR is active
    [[nodiscard]] bool is_active() const {
        return enabled && is_hdr(transfer_function);
    }

    /// Get the nits-per-stop for exposure calculations
    [[nodiscard]] float nits_per_stop() const {
        if (is_active()) {
            return static_cast<float>(max_luminance) / 10.0f;
        }
        return 10.0f; // SDR: 100 nits / 10 stops
    }

    /// Convert to DRM HDR metadata blob
    [[nodiscard]] HdrMetadata to_drm_metadata() const {
        auto coords = to_cie_xy(color_primaries);

        return HdrMetadata{
            .display_primaries_x = {
                static_cast<std::uint16_t>(coords.red_x * 50000.0f),
                static_cast<std::uint16_t>(coords.green_x * 50000.0f),
                static_cast<std::uint16_t>(coords.blue_x * 50000.0f),
            },
            .display_primaries_y = {
                static_cast<std::uint16_t>(coords.red_y * 50000.0f),
                static_cast<std::uint16_t>(coords.green_y * 50000.0f),
                static_cast<std::uint16_t>(coords.blue_y * 50000.0f),
            },
            .white_point_x = static_cast<std::uint16_t>(coords.white_x * 50000.0f),
            .white_point_y = static_cast<std::uint16_t>(coords.white_y * 50000.0f),
            .max_display_mastering_luminance = max_luminance,
            .min_display_mastering_luminance = static_cast<std::uint32_t>(min_luminance * 10000.0f),
            .max_content_light_level = max_content_light_level.value_or(0),
            .max_frame_average_light_level = max_frame_average_light_level.value_or(0),
            .eotf = eotf_id(transfer_function),
        };
    }
};

// =============================================================================
// HDR Capability
// =============================================================================

/// HDR capability detection result
struct HdrCapability {
    /// Is HDR supported?
    bool supported = false;
    /// Supported transfer functions
    std::vector<TransferFunction> transfer_functions = {TransferFunction::Sdr};
    /// Maximum luminance in nits
    std::optional<std::uint32_t> max_luminance = 100;
    /// Minimum luminance in nits
    std::optional<float> min_luminance = 0.0f;
    /// Supported color gamuts
    std::vector<ColorPrimaries> color_gamuts = {ColorPrimaries::Srgb};

    /// Create a capability for an HDR10-capable display
    static HdrCapability hdr10_capable(std::uint32_t max_nits, float min_nits) {
        return HdrCapability{
            .supported = true,
            .transfer_functions = {TransferFunction::Sdr, TransferFunction::Pq},
            .max_luminance = max_nits,
            .min_luminance = min_nits,
            .color_gamuts = {ColorPrimaries::Srgb, ColorPrimaries::DciP3, ColorPrimaries::Rec2020},
        };
    }

    /// Create a capability for an HLG-capable display
    static HdrCapability hlg_capable(std::uint32_t max_nits) {
        return HdrCapability{
            .supported = true,
            .transfer_functions = {TransferFunction::Sdr, TransferFunction::Hlg},
            .max_luminance = max_nits,
            .min_luminance = 0.0f,
            .color_gamuts = {ColorPrimaries::Srgb, ColorPrimaries::Rec2020},
        };
    }

    /// Create a capability for an HDR10+HLG display
    static HdrCapability full_hdr(std::uint32_t max_nits, float min_nits) {
        return HdrCapability{
            .supported = true,
            .transfer_functions = {TransferFunction::Sdr, TransferFunction::Pq, TransferFunction::Hlg},
            .max_luminance = max_nits,
            .min_luminance = min_nits,
            .color_gamuts = {ColorPrimaries::Srgb, ColorPrimaries::DciP3, ColorPrimaries::Rec2020},
        };
    }

    /// Create a capability for a non-HDR display
    static HdrCapability sdr_only() {
        return HdrCapability{};
    }

    /// Check if a specific transfer function is supported
    [[nodiscard]] bool supports_transfer_function(TransferFunction tf) const {
        for (const auto& t : transfer_functions) {
            if (t == tf) return true;
        }
        return false;
    }

    /// Check if a specific color gamut is supported
    [[nodiscard]] bool supports_color_gamut(ColorPrimaries gamut) const {
        for (const auto& g : color_gamuts) {
            if (g == gamut) return true;
        }
        return false;
    }

    /// Convert to HdrConfig (prefer HDR10 if available)
    [[nodiscard]] HdrConfig to_config(bool prefer_hdr10 = true) const {
        if (!supported) {
            return HdrConfig::sdr();
        }

        std::uint32_t max_nits = max_luminance.value_or(1000);

        if (prefer_hdr10 && supports_transfer_function(TransferFunction::Pq)) {
            return HdrConfig::hdr10(max_nits);
        } else if (supports_transfer_function(TransferFunction::Hlg)) {
            return HdrConfig::hlg(max_nits);
        }

        return HdrConfig::sdr();
    }
};

} // namespace void_compositor
