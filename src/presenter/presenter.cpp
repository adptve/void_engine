/// @file presenter.cpp
/// @brief Presenter implementation for void_presenter

#include <void_engine/presenter/presenter.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace void_presenter {

// =============================================================================
// PresenterManager Extended Methods
// =============================================================================

// Note: The PresenterManager class is primarily header-only in presenter.hpp.
// This file provides additional utility functions and any non-inline implementations.

// =============================================================================
// Presenter Utilities
// =============================================================================

namespace {

/// Global presenter ID counter for unique ID generation
std::atomic<std::uint64_t> g_presenter_id_counter{1};

} // anonymous namespace

/// Generate a unique presenter ID
PresenterId generate_presenter_id() {
    return PresenterId(g_presenter_id_counter.fetch_add(1, std::memory_order_relaxed));
}

/// Reset the presenter ID counter (for testing)
void reset_presenter_id_counter() {
    g_presenter_id_counter.store(1, std::memory_order_relaxed);
}

// =============================================================================
// PresenterConfig Utilities
// =============================================================================

PresenterConfig create_default_config(std::uint32_t width, std::uint32_t height) {
    PresenterConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.present_mode = PresentMode::Fifo;
    config.enable_hdr = false;
    config.target_frame_rate = 60;
    config.allow_tearing = false;
    return config;
}

PresenterConfig create_low_latency_config(std::uint32_t width, std::uint32_t height) {
    PresenterConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.present_mode = PresentMode::Mailbox;
    config.enable_hdr = false;
    config.target_frame_rate = 0; // Unlimited
    config.allow_tearing = true;
    return config;
}

PresenterConfig create_vsync_config(std::uint32_t width, std::uint32_t height) {
    PresenterConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.present_mode = PresentMode::Fifo;
    config.enable_hdr = false;
    config.target_frame_rate = 60;
    config.allow_tearing = false;
    return config;
}

// =============================================================================
// PresenterCapabilities Utilities
// =============================================================================

PresenterCapabilities query_presenter_capabilities(const BackendCapabilities& backend_caps) {
    PresenterCapabilities caps;

    caps.present_modes = backend_caps.supported_present_modes;
    caps.formats = backend_caps.supported_formats;
    caps.max_width = backend_caps.limits.max_texture_dimension_2d;
    caps.max_height = backend_caps.limits.max_texture_dimension_2d;
    caps.hdr_support = backend_caps.features.hdr_output;
    caps.vrr_support = backend_caps.features.vrr;
    caps.xr_passthrough = false; // Determined by XR system

    return caps;
}

bool is_format_supported(const PresenterCapabilities& caps, SurfaceFormat format) {
    return std::find(caps.formats.begin(), caps.formats.end(), format) != caps.formats.end();
}

bool is_present_mode_supported(const PresenterCapabilities& caps, PresentMode mode) {
    return std::find(caps.present_modes.begin(), caps.present_modes.end(), mode) != caps.present_modes.end();
}

SurfaceFormat select_best_format(const PresenterCapabilities& caps, bool prefer_hdr) {
    if (prefer_hdr && caps.hdr_support) {
        // Try HDR formats first
        for (const auto& fmt : caps.formats) {
            if (is_hdr_capable(fmt)) {
                return fmt;
            }
        }
    }

    // Prefer sRGB formats
    for (const auto& fmt : caps.formats) {
        if (is_srgb(fmt)) {
            return fmt;
        }
    }

    // Return first available format
    return caps.formats.empty() ? SurfaceFormat::Bgra8UnormSrgb : caps.formats[0];
}

PresentMode select_best_present_mode(const PresenterCapabilities& caps, bool prefer_low_latency) {
    if (prefer_low_latency) {
        // Prefer Mailbox for low latency without tearing
        if (is_present_mode_supported(caps, PresentMode::Mailbox)) {
            return PresentMode::Mailbox;
        }
        // Then Immediate (may tear but lowest latency)
        if (is_present_mode_supported(caps, PresentMode::Immediate)) {
            return PresentMode::Immediate;
        }
    }

    // Default to FIFO (VSync)
    if (is_present_mode_supported(caps, PresentMode::Fifo)) {
        return PresentMode::Fifo;
    }

    // Fallback to first available
    return caps.present_modes.empty() ? PresentMode::Fifo : caps.present_modes[0];
}

// =============================================================================
// PresenterError Utilities
// =============================================================================

std::string format_presenter_error(const PresenterError& error) {
    std::string result;

    switch (error.kind) {
        case PresenterErrorKind::SurfaceCreation:
            result = "Surface creation error: ";
            break;
        case PresenterErrorKind::SurfaceLost:
            result = "Surface lost: ";
            break;
        case PresenterErrorKind::FrameAcquisition:
            result = "Frame acquisition error: ";
            break;
        case PresenterErrorKind::PresentationFailed:
            result = "Presentation failed: ";
            break;
        case PresenterErrorKind::BackendNotAvailable:
            result = "Backend not available: ";
            break;
        case PresenterErrorKind::ConfigError:
            result = "Configuration error: ";
            break;
        case PresenterErrorKind::RehydrationFailed:
            result = "Rehydration failed: ";
            break;
    }

    result += error.message;
    return result;
}

const char* presenter_error_kind_name(PresenterErrorKind kind) {
    switch (kind) {
        case PresenterErrorKind::SurfaceCreation: return "SurfaceCreation";
        case PresenterErrorKind::SurfaceLost: return "SurfaceLost";
        case PresenterErrorKind::FrameAcquisition: return "FrameAcquisition";
        case PresenterErrorKind::PresentationFailed: return "PresentationFailed";
        case PresenterErrorKind::BackendNotAvailable: return "BackendNotAvailable";
        case PresenterErrorKind::ConfigError: return "ConfigError";
        case PresenterErrorKind::RehydrationFailed: return "RehydrationFailed";
    }
    return "Unknown";
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_presenter_id(const PresenterId& id) {
    return "PresenterId(" + std::to_string(id.id) + ")";
}

std::string format_presenter_config(const PresenterConfig& config) {
    std::string result = "PresenterConfig {\n";
    result += "  format: " + std::string(to_string(config.format)) + "\n";
    result += "  present_mode: " + std::string(to_string(config.present_mode)) + "\n";
    result += "  size: " + std::to_string(config.width) + "x" + std::to_string(config.height) + "\n";
    result += "  enable_hdr: " + std::string(config.enable_hdr ? "true" : "false") + "\n";
    result += "  target_fps: " + std::to_string(config.target_frame_rate) + "\n";
    result += "  allow_tearing: " + std::string(config.allow_tearing ? "true" : "false") + "\n";
    result += "}";
    return result;
}

std::string format_presenter_capabilities(const PresenterCapabilities& caps) {
    std::string result = "PresenterCapabilities {\n";

    result += "  formats: [";
    for (std::size_t i = 0; i < caps.formats.size(); ++i) {
        if (i > 0) result += ", ";
        result += to_string(caps.formats[i]);
    }
    result += "]\n";

    result += "  present_modes: [";
    for (std::size_t i = 0; i < caps.present_modes.size(); ++i) {
        if (i > 0) result += ", ";
        result += to_string(caps.present_modes[i]);
    }
    result += "]\n";

    result += "  max_resolution: " + std::to_string(caps.max_width) + "x" + std::to_string(caps.max_height) + "\n";
    result += "  hdr_support: " + std::string(caps.hdr_support ? "true" : "false") + "\n";
    result += "  vrr_support: " + std::string(caps.vrr_support ? "true" : "false") + "\n";
    result += "  xr_passthrough: " + std::string(caps.xr_passthrough ? "true" : "false") + "\n";
    result += "}";

    return result;
}

std::string format_presenter_manager_state(const PresenterManager& manager) {
    std::string result = "PresenterManager {\n";
    result += "  count: " + std::to_string(manager.count()) + "\n";

    auto ids = manager.all_ids();
    result += "  presenters: [";
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) result += ", ";
        result += std::to_string(ids[i].id);
    }
    result += "]\n";

    if (auto* primary = manager.primary()) {
        result += "  primary: " + std::to_string(primary->id().id) + "\n";
    } else {
        result += "  primary: none\n";
    }

    result += "}";
    return result;
}

} // namespace debug

} // namespace void_presenter
