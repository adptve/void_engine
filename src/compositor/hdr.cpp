/// @file hdr.cpp
/// @brief HDR (High Dynamic Range) implementation
///
/// Provides tone mapping utilities and HDR configuration helpers.

#include <void_engine/compositor/hdr.hpp>
#include <void_engine/compositor/types.hpp>

#include <algorithm>
#include <cmath>
#include <array>

namespace void_compositor {

// =============================================================================
// HDR Tone Mapping
// =============================================================================

/// PQ (Perceptual Quantizer) constants - SMPTE ST 2084
namespace pq_constants {
    constexpr float m1 = 0.1593017578125f;      // 2610/16384
    constexpr float m2 = 78.84375f;             // 2523/32 * 128
    constexpr float c1 = 0.8359375f;            // 3424/4096
    constexpr float c2 = 18.8515625f;           // 2413/128
    constexpr float c3 = 18.6875f;              // 2392/128
    constexpr float max_luminance = 10000.0f;   // cd/m²
}

/// Apply PQ EOTF (electrical to optical)
/// Converts PQ-encoded value to linear luminance
[[nodiscard]] float pq_eotf(float e) {
    if (e <= 0.0f) return 0.0f;

    float ep = std::pow(e, 1.0f / pq_constants::m2);
    float numerator = std::max(ep - pq_constants::c1, 0.0f);
    float denominator = pq_constants::c2 - pq_constants::c3 * ep;

    if (denominator <= 0.0f) return 0.0f;

    float y = std::pow(numerator / denominator, 1.0f / pq_constants::m1);
    return y * pq_constants::max_luminance;
}

/// Apply inverse PQ EOTF (optical to electrical)
/// Converts linear luminance to PQ-encoded value
[[nodiscard]] float pq_eotf_inverse(float y) {
    if (y <= 0.0f) return 0.0f;

    float yn = y / pq_constants::max_luminance;
    float yp = std::pow(yn, pq_constants::m1);

    float numerator = pq_constants::c1 + pq_constants::c2 * yp;
    float denominator = 1.0f + pq_constants::c3 * yp;

    return std::pow(numerator / denominator, pq_constants::m2);
}

/// HLG (Hybrid Log-Gamma) constants - ARIB STD-B67
namespace hlg_constants {
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;  // 1 - 4*a
    constexpr float c = 0.55991073f;  // 0.5 - a * ln(4*a)
}

/// Apply HLG OETF (optical to electrical)
[[nodiscard]] float hlg_oetf(float e) {
    if (e <= 1.0f / 12.0f) {
        return std::sqrt(3.0f * e);
    } else {
        return hlg_constants::a * std::log(12.0f * e - hlg_constants::b) + hlg_constants::c;
    }
}

/// Apply HLG EOTF (electrical to optical)
[[nodiscard]] float hlg_eotf(float e) {
    if (e <= 0.5f) {
        return (e * e) / 3.0f;
    } else {
        return (std::exp((e - hlg_constants::c) / hlg_constants::a) + hlg_constants::b) / 12.0f;
    }
}

/// sRGB EOTF (electrical to optical)
[[nodiscard]] float srgb_eotf(float e) {
    if (e <= 0.04045f) {
        return e / 12.92f;
    } else {
        return std::pow((e + 0.055f) / 1.055f, 2.4f);
    }
}

/// sRGB OETF (optical to electrical)
[[nodiscard]] float srgb_oetf(float l) {
    if (l <= 0.0031308f) {
        return l * 12.92f;
    } else {
        return 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
    }
}

// =============================================================================
// Tone Mapping Operators
// =============================================================================

/// Simple Reinhard tone mapping
[[nodiscard]] float tonemap_reinhard(float hdr, float max_luminance) {
    float normalized = hdr / max_luminance;
    return normalized / (1.0f + normalized);
}

/// Extended Reinhard with white point
[[nodiscard]] float tonemap_reinhard_extended(float hdr, float max_luminance, float white_point) {
    float normalized = hdr / max_luminance;
    float white_sq = white_point * white_point;
    return normalized * (1.0f + normalized / white_sq) / (1.0f + normalized);
}

/// ACES filmic tone mapping
[[nodiscard]] float tonemap_aces(float hdr) {
    constexpr float a = 2.51f;
    constexpr float b = 0.03f;
    constexpr float c = 2.43f;
    constexpr float d = 0.59f;
    constexpr float e = 0.14f;

    return std::clamp(
        (hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e),
        0.0f, 1.0f
    );
}

/// Uncharted 2 filmic tone mapping
[[nodiscard]] float tonemap_uncharted2(float hdr) {
    constexpr float a = 0.15f;
    constexpr float b = 0.50f;
    constexpr float c = 0.10f;
    constexpr float d = 0.20f;
    constexpr float e = 0.02f;
    constexpr float f = 0.30f;

    auto uncharted = [=](float x) {
        return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
    };

    constexpr float w = 11.2f;  // White point
    float white_scale = 1.0f / uncharted(w);

    return uncharted(hdr) * white_scale;
}

// =============================================================================
// Color Space Conversion
// =============================================================================

/// Convert Rec.709 to Rec.2020
void convert_709_to_2020(float& r, float& g, float& b) {
    // 3x3 matrix for Rec.709 to Rec.2020
    constexpr float m[3][3] = {
        { 0.6274040f,  0.3292820f,  0.0433136f},
        { 0.0690970f,  0.9195400f,  0.0113612f},
        { 0.0163916f,  0.0880132f,  0.8955950f}
    };

    float new_r = m[0][0] * r + m[0][1] * g + m[0][2] * b;
    float new_g = m[1][0] * r + m[1][1] * g + m[1][2] * b;
    float new_b = m[2][0] * r + m[2][1] * g + m[2][2] * b;

    r = new_r;
    g = new_g;
    b = new_b;
}

/// Convert Rec.2020 to Rec.709
void convert_2020_to_709(float& r, float& g, float& b) {
    // Inverse matrix for Rec.2020 to Rec.709
    constexpr float m[3][3] = {
        { 1.6604910f, -0.5876411f, -0.0728499f},
        {-0.1245505f,  1.1328999f, -0.0083494f},
        {-0.0181508f, -0.1005789f,  1.1187297f}
    };

    float new_r = m[0][0] * r + m[0][1] * g + m[0][2] * b;
    float new_g = m[1][0] * r + m[1][1] * g + m[1][2] * b;
    float new_b = m[2][0] * r + m[2][1] * g + m[2][2] * b;

    r = new_r;
    g = new_g;
    b = new_b;
}

/// Convert DCI-P3 to Rec.2020
void convert_p3_to_2020(float& r, float& g, float& b) {
    constexpr float m[3][3] = {
        { 0.7530430f,  0.1986530f,  0.0483041f},
        { 0.0457433f,  0.9417704f,  0.0124862f},
        {-0.0012107f,  0.0176043f,  0.9836063f}
    };

    float new_r = m[0][0] * r + m[0][1] * g + m[0][2] * b;
    float new_g = m[1][0] * r + m[1][1] * g + m[1][2] * b;
    float new_b = m[2][0] * r + m[2][1] * g + m[2][2] * b;

    r = new_r;
    g = new_g;
    b = new_b;
}

// =============================================================================
// HDR Configuration Utilities
// =============================================================================

/// Get optimal HDR config for a display capability
[[nodiscard]] HdrConfig get_optimal_hdr_config(const HdrCapability& capability) {
    if (!capability.supported) {
        return HdrConfig::sdr();
    }

    std::uint32_t max_nits = capability.max_luminance.value_or(1000);

    // Prefer HDR10 (PQ) over HLG
    if (capability.supports_transfer_function(TransferFunction::Pq)) {
        return HdrConfig::hdr10(max_nits);
    } else if (capability.supports_transfer_function(TransferFunction::Hlg)) {
        return HdrConfig::hlg(max_nits);
    }

    return HdrConfig::sdr();
}

/// Create HDR metadata for content
[[nodiscard]] HdrMetadata create_content_metadata(
    std::uint32_t max_cll,
    std::uint32_t max_fall,
    ColorPrimaries primaries,
    TransferFunction transfer_fn) {

    HdrConfig config;
    config.enabled = true;
    config.transfer_function = transfer_fn;
    config.color_primaries = primaries;
    config.max_luminance = max_cll;
    config.max_content_light_level = max_cll;
    config.max_frame_average_light_level = max_fall;

    return config.to_drm_metadata();
}

/// Validate HDR config against capability
[[nodiscard]] bool validate_hdr_config(const HdrConfig& config, const HdrCapability& capability) {
    if (!config.enabled) {
        return true;  // SDR is always valid
    }

    if (!capability.supported) {
        return false;  // HDR config on non-HDR display
    }

    if (!capability.supports_transfer_function(config.transfer_function)) {
        return false;
    }

    if (!capability.supports_color_gamut(config.color_primaries)) {
        return false;
    }

    // Check luminance range
    if (config.max_luminance > capability.max_luminance.value_or(0)) {
        return false;
    }

    return true;
}

// =============================================================================
// HDR Format Selection
// =============================================================================

/// Get the best render format for HDR output
[[nodiscard]] RenderFormat get_hdr_render_format(const HdrConfig& config) {
    if (!config.is_active()) {
        return RenderFormat::Rgba8UnormSrgb;
    }

    // For HDR, prefer float16 for precision
    // Rgb10a2 is also acceptable for HDR10
    if (config.max_luminance > 1000) {
        return RenderFormat::Rgba16Float;
    }

    return RenderFormat::Rgb10a2Unorm;
}

/// Check if a render format supports HDR
[[nodiscard]] bool format_supports_hdr(RenderFormat format) {
    return is_hdr_format(format);
}

// =============================================================================
// Luminance Calculation
// =============================================================================

/// Calculate perceptual luminance (Rec.709 luma)
[[nodiscard]] float calculate_luminance_709(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

/// Calculate perceptual luminance (Rec.2020 luma)
[[nodiscard]] float calculate_luminance_2020(float r, float g, float b) {
    return 0.2627f * r + 0.6780f * g + 0.0593f * b;
}

/// Estimate MaxFALL from content analysis
[[nodiscard]] std::uint32_t estimate_max_fall(
    const float* pixel_data,
    std::size_t pixel_count,
    std::uint32_t max_cll) {

    if (pixel_count == 0 || !pixel_data) {
        return max_cll / 2;
    }

    double total_luminance = 0.0;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        std::size_t idx = i * 3;
        float luma = calculate_luminance_709(
            pixel_data[idx], pixel_data[idx + 1], pixel_data[idx + 2]);
        total_luminance += static_cast<double>(luma);
    }

    double avg = total_luminance / static_cast<double>(pixel_count);
    return static_cast<std::uint32_t>(avg * static_cast<double>(max_cll));
}

} // namespace void_compositor
