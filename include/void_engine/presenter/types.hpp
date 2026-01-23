#pragma once

/// @file types.hpp
/// @brief Core types for void_presenter
///
/// Defines surface formats, presentation modes, and related enums.

#include <cstdint>

namespace void_presenter {

// =============================================================================
// Surface Format
// =============================================================================

/// Pixel format for surfaces
enum class SurfaceFormat {
    Bgra8Unorm,         ///< 8-bit BGRA, linear
    Bgra8UnormSrgb,     ///< 8-bit BGRA, sRGB
    Rgba8Unorm,         ///< 8-bit RGBA, linear
    Rgba8UnormSrgb,     ///< 8-bit RGBA, sRGB
    Rgba16Float,        ///< 16-bit float RGBA (HDR)
    Rgb10a2Unorm,       ///< 10-bit RGB, 2-bit alpha
};

/// Get bytes per pixel for a format
[[nodiscard]] constexpr std::uint32_t bytes_per_pixel(SurfaceFormat format) {
    switch (format) {
        case SurfaceFormat::Bgra8Unorm:
        case SurfaceFormat::Bgra8UnormSrgb:
        case SurfaceFormat::Rgba8Unorm:
        case SurfaceFormat::Rgba8UnormSrgb:
        case SurfaceFormat::Rgb10a2Unorm:
            return 4;
        case SurfaceFormat::Rgba16Float:
            return 8;
    }
    return 4;
}

/// Check if format is sRGB
[[nodiscard]] constexpr bool is_srgb(SurfaceFormat format) {
    return format == SurfaceFormat::Bgra8UnormSrgb ||
           format == SurfaceFormat::Rgba8UnormSrgb;
}

/// Check if format supports HDR
[[nodiscard]] constexpr bool is_hdr_capable(SurfaceFormat format) {
    return format == SurfaceFormat::Rgba16Float ||
           format == SurfaceFormat::Rgb10a2Unorm;
}

/// Get format name as string
[[nodiscard]] constexpr const char* to_string(SurfaceFormat format) {
    switch (format) {
        case SurfaceFormat::Bgra8Unorm: return "Bgra8Unorm";
        case SurfaceFormat::Bgra8UnormSrgb: return "Bgra8UnormSrgb";
        case SurfaceFormat::Rgba8Unorm: return "Rgba8Unorm";
        case SurfaceFormat::Rgba8UnormSrgb: return "Rgba8UnormSrgb";
        case SurfaceFormat::Rgba16Float: return "Rgba16Float";
        case SurfaceFormat::Rgb10a2Unorm: return "Rgb10a2Unorm";
    }
    return "Unknown";
}

// =============================================================================
// Present Mode
// =============================================================================

/// Presentation mode (Vulkan/wgpu terminology)
enum class PresentMode {
    Immediate,      ///< No sync, may tear, lowest latency
    Mailbox,        ///< Triple buffering, no tear, may drop frames
    Fifo,           ///< VSync, no tear, no drops, higher latency
    FifoRelaxed,    ///< VSync normally, may tear when late
};

/// Check if mode prevents tearing
[[nodiscard]] constexpr bool prevents_tearing(PresentMode mode) {
    return mode == PresentMode::Mailbox || mode == PresentMode::Fifo;
}

/// Check if mode may drop frames
[[nodiscard]] constexpr bool may_drop_frames(PresentMode mode) {
    return mode == PresentMode::Immediate || mode == PresentMode::Mailbox;
}

/// Get mode description
[[nodiscard]] constexpr const char* to_string(PresentMode mode) {
    switch (mode) {
        case PresentMode::Immediate: return "Immediate";
        case PresentMode::Mailbox: return "Mailbox";
        case PresentMode::Fifo: return "Fifo";
        case PresentMode::FifoRelaxed: return "FifoRelaxed";
    }
    return "Unknown";
}

/// Get detailed mode description
[[nodiscard]] constexpr const char* description(PresentMode mode) {
    switch (mode) {
        case PresentMode::Immediate:
            return "Immediate (may tear, lowest latency)";
        case PresentMode::Mailbox:
            return "Mailbox (no tear, may drop frames)";
        case PresentMode::Fifo:
            return "FIFO (no tear, no drops, higher latency)";
        case PresentMode::FifoRelaxed:
            return "FIFO Relaxed (no tear normally, may tear when late)";
    }
    return "Unknown";
}

// =============================================================================
// VSync Mode
// =============================================================================

/// VSync mode (higher-level abstraction)
enum class VSync {
    Off,        ///< VSync disabled
    On,         ///< VSync enabled
    Adaptive,   ///< Adaptive VSync
};

/// Convert VSync to PresentMode
[[nodiscard]] constexpr PresentMode to_present_mode(VSync vsync) {
    switch (vsync) {
        case VSync::Off: return PresentMode::Immediate;
        case VSync::On: return PresentMode::Fifo;
        case VSync::Adaptive: return PresentMode::FifoRelaxed;
    }
    return PresentMode::Fifo;
}

/// Get VSync name
[[nodiscard]] constexpr const char* to_string(VSync vsync) {
    switch (vsync) {
        case VSync::Off: return "Off";
        case VSync::On: return "On";
        case VSync::Adaptive: return "Adaptive";
    }
    return "Unknown";
}

// =============================================================================
// Alpha Mode
// =============================================================================

/// Alpha blending mode for compositing
enum class AlphaMode {
    Opaque,         ///< Fully opaque
    PreMultiplied,  ///< Premultiplied alpha
    PostMultiplied, ///< Post-multiplied alpha
    Inherit,        ///< Inherit from parent
};

/// Get alpha mode name
[[nodiscard]] constexpr const char* to_string(AlphaMode mode) {
    switch (mode) {
        case AlphaMode::Opaque: return "Opaque";
        case AlphaMode::PreMultiplied: return "PreMultiplied";
        case AlphaMode::PostMultiplied: return "PostMultiplied";
        case AlphaMode::Inherit: return "Inherit";
    }
    return "Unknown";
}

// =============================================================================
// Surface State
// =============================================================================

/// Surface lifecycle state
enum class SurfaceState {
    Ready,              ///< Surface is healthy and ready
    NeedsReconfigure,   ///< Needs reconfiguration (resize, format change)
    Lost,               ///< Surface was lost and needs recreation
    Minimized,          ///< Surface is minimized (zero size)
};

/// Get state name
[[nodiscard]] constexpr const char* to_string(SurfaceState state) {
    switch (state) {
        case SurfaceState::Ready: return "Ready";
        case SurfaceState::NeedsReconfigure: return "NeedsReconfigure";
        case SurfaceState::Lost: return "Lost";
        case SurfaceState::Minimized: return "Minimized";
    }
    return "Unknown";
}

// =============================================================================
// Frame State
// =============================================================================

/// Frame lifecycle state
enum class FrameState {
    Preparing,  ///< Frame is being prepared
    Rendering,  ///< Frame is being rendered
    Ready,      ///< Frame is ready for presentation
    Presented,  ///< Frame has been presented
    Dropped,    ///< Frame was dropped (missed deadline)
};

/// Get frame state name
[[nodiscard]] constexpr const char* to_string(FrameState state) {
    switch (state) {
        case FrameState::Preparing: return "Preparing";
        case FrameState::Rendering: return "Rendering";
        case FrameState::Ready: return "Ready";
        case FrameState::Presented: return "Presented";
        case FrameState::Dropped: return "Dropped";
    }
    return "Unknown";
}

} // namespace void_presenter
