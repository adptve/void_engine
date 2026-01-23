#pragma once

/// @file types.hpp
/// @brief Core types for void_compositor

#include "fwd.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace void_compositor {

// =============================================================================
// Render Format
// =============================================================================

/// Render format (compatible with wgpu/void_presenter surface formats)
enum class RenderFormat : std::uint8_t {
    Bgra8Unorm,
    Bgra8UnormSrgb,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Rgb10a2Unorm,
    Rgba16Float,
};

/// Get format name for wgpu interop
[[nodiscard]] inline const char* to_wgpu_format_name(RenderFormat format) {
    switch (format) {
        case RenderFormat::Bgra8Unorm: return "Bgra8Unorm";
        case RenderFormat::Bgra8UnormSrgb: return "Bgra8UnormSrgb";
        case RenderFormat::Rgba8Unorm: return "Rgba8Unorm";
        case RenderFormat::Rgba8UnormSrgb: return "Rgba8UnormSrgb";
        case RenderFormat::Rgb10a2Unorm: return "Rgb10a2Unorm";
        case RenderFormat::Rgba16Float: return "Rgba16Float";
    }
    return "Unknown";
}

/// Get bytes per pixel for a format
[[nodiscard]] inline std::uint32_t bytes_per_pixel(RenderFormat format) {
    switch (format) {
        case RenderFormat::Bgra8Unorm:
        case RenderFormat::Bgra8UnormSrgb:
        case RenderFormat::Rgba8Unorm:
        case RenderFormat::Rgba8UnormSrgb:
        case RenderFormat::Rgb10a2Unorm:
            return 4;
        case RenderFormat::Rgba16Float:
            return 8;
    }
    return 4;
}

/// Check if format is sRGB
[[nodiscard]] inline bool is_srgb(RenderFormat format) {
    return format == RenderFormat::Bgra8UnormSrgb ||
           format == RenderFormat::Rgba8UnormSrgb;
}

/// Check if format supports HDR
[[nodiscard]] inline bool is_hdr_format(RenderFormat format) {
    return format == RenderFormat::Rgb10a2Unorm ||
           format == RenderFormat::Rgba16Float;
}

// =============================================================================
// Compositor Configuration
// =============================================================================

/// Compositor configuration
struct CompositorConfig {
    /// Target refresh rate (0 = use display default)
    std::uint32_t target_fps = 0;
    /// Enable VSync
    bool vsync = true;
    /// Allow tearing for lower latency
    bool allow_tearing = false;
    /// Enable XWayland for X11 app support (Linux only)
    bool xwayland = false;
    /// Enable VRR if available
    bool enable_vrr = true;
    /// Enable HDR if available
    bool enable_hdr = true;
    /// Preferred render format
    RenderFormat preferred_format = RenderFormat::Bgra8UnormSrgb;
};

/// Compositor capabilities (queried at runtime)
struct CompositorCapabilities {
    /// Available refresh rates
    std::vector<std::uint32_t> refresh_rates;
    /// Maximum resolution
    std::uint32_t max_width = 0;
    std::uint32_t max_height = 0;
    /// Current resolution
    std::uint32_t current_width = 0;
    std::uint32_t current_height = 0;
    /// VRR (Variable Refresh Rate) support
    bool vrr_supported = false;
    /// HDR support
    bool hdr_supported = false;
    /// Number of connected displays
    std::size_t display_count = 0;
    /// Supported formats
    std::vector<RenderFormat> supported_formats;
};

// =============================================================================
// Output Transform
// =============================================================================

/// Output transform (rotation and reflection)
enum class OutputTransform : std::uint8_t {
    Normal,
    Rotate90,
    Rotate180,
    Rotate270,
    Flipped,
    FlippedRotate90,
    FlippedRotate180,
    FlippedRotate270,
};

/// Get transform name
[[nodiscard]] inline const char* to_string(OutputTransform transform) {
    switch (transform) {
        case OutputTransform::Normal: return "Normal";
        case OutputTransform::Rotate90: return "Rotate90";
        case OutputTransform::Rotate180: return "Rotate180";
        case OutputTransform::Rotate270: return "Rotate270";
        case OutputTransform::Flipped: return "Flipped";
        case OutputTransform::FlippedRotate90: return "FlippedRotate90";
        case OutputTransform::FlippedRotate180: return "FlippedRotate180";
        case OutputTransform::FlippedRotate270: return "FlippedRotate270";
    }
    return "Unknown";
}

// =============================================================================
// Error Types
// =============================================================================

/// Compositor error
struct CompositorError {
    enum class Type {
        None,
        Session,
        Drm,
        Input,
        Display,
        Backend,
        Configuration,
    };

    Type type = Type::None;
    std::string message;

    [[nodiscard]] bool ok() const { return type == Type::None; }
    [[nodiscard]] operator bool() const { return !ok(); }

    static CompositorError none() { return {Type::None, ""}; }
    static CompositorError session(const std::string& msg) { return {Type::Session, msg}; }
    static CompositorError drm(const std::string& msg) { return {Type::Drm, msg}; }
    static CompositorError input(const std::string& msg) { return {Type::Input, msg}; }
    static CompositorError display(const std::string& msg) { return {Type::Display, msg}; }
    static CompositorError backend(const std::string& msg) { return {Type::Backend, msg}; }
    static CompositorError configuration(const std::string& msg) { return {Type::Configuration, msg}; }
};

} // namespace void_compositor
