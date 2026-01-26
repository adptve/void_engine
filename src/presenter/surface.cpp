/// @file surface.cpp
/// @brief Surface utilities implementation for void_presenter

#include <void_engine/presenter/surface.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

namespace void_presenter {

// =============================================================================
// SurfaceConfig Utilities
// =============================================================================

SurfaceConfig create_default_surface_config(
    std::uint32_t width,
    std::uint32_t height) {

    SurfaceConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.vsync = VSync::Adaptive;
    config.present_mode = PresentMode::Fifo;
    config.alpha_mode = AlphaMode::Opaque;
    return config;
}

SurfaceConfig create_fullscreen_surface_config(
    std::uint32_t width,
    std::uint32_t height,
    bool exclusive) {

    SurfaceConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.vsync = VSync::On;
    config.present_mode = exclusive ? PresentMode::Fifo : PresentMode::Mailbox;
    config.alpha_mode = AlphaMode::Opaque;
    return config;
}

SurfaceConfig create_hdr_surface_config(
    std::uint32_t width,
    std::uint32_t height) {

    SurfaceConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Rgba16Float;
    config.vsync = VSync::On;
    config.present_mode = PresentMode::Fifo;
    config.alpha_mode = AlphaMode::Opaque;
    return config;
}

SurfaceConfig create_composited_surface_config(
    std::uint32_t width,
    std::uint32_t height) {

    SurfaceConfig config;
    config.width = width;
    config.height = height;
    config.format = SurfaceFormat::Rgba8UnormSrgb;
    config.vsync = VSync::Off;
    config.present_mode = PresentMode::Mailbox;
    config.alpha_mode = AlphaMode::PreMultiplied;
    return config;
}

// =============================================================================
// SurfaceCapabilities Utilities
// =============================================================================

std::string format_surface_capabilities(const SurfaceCapabilities& caps) {
    std::string result = "SurfaceCapabilities {\n";

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

    result += "  alpha_modes: [";
    for (std::size_t i = 0; i < caps.alpha_modes.size(); ++i) {
        if (i > 0) result += ", ";
        result += to_string(caps.alpha_modes[i]);
    }
    result += "]\n";

    result += "  extent_range: " + std::to_string(caps.min_width) + "x" +
              std::to_string(caps.min_height) + " - " +
              std::to_string(caps.max_width) + "x" +
              std::to_string(caps.max_height) + "\n";

    result += "  preferred_format: " + std::string(to_string(caps.preferred_format())) + "\n";

    result += "}";
    return result;
}

SurfaceCapabilities merge_surface_capabilities(
    const SurfaceCapabilities& a,
    const SurfaceCapabilities& b) {

    SurfaceCapabilities merged;

    // Intersect formats
    for (const auto& fmt : a.formats) {
        if (std::find(b.formats.begin(), b.formats.end(), fmt) != b.formats.end()) {
            merged.formats.push_back(fmt);
        }
    }

    // Intersect present modes
    for (const auto& mode : a.present_modes) {
        if (std::find(b.present_modes.begin(), b.present_modes.end(), mode) != b.present_modes.end()) {
            merged.present_modes.push_back(mode);
        }
    }

    // Intersect alpha modes
    for (const auto& alpha : a.alpha_modes) {
        if (std::find(b.alpha_modes.begin(), b.alpha_modes.end(), alpha) != b.alpha_modes.end()) {
            merged.alpha_modes.push_back(alpha);
        }
    }

    // Constrain extent
    merged.min_width = std::max(a.min_width, b.min_width);
    merged.min_height = std::max(a.min_height, b.min_height);
    merged.max_width = std::min(a.max_width, b.max_width);
    merged.max_height = std::min(a.max_height, b.max_height);

    return merged;
}

bool is_config_compatible(
    const SurfaceConfig& config,
    const SurfaceCapabilities& caps) {

    // Check format
    bool format_ok = false;
    for (const auto& fmt : caps.formats) {
        if (fmt == config.format) {
            format_ok = true;
            break;
        }
    }
    if (!format_ok) return false;

    // Check present mode
    bool mode_ok = false;
    for (const auto& mode : caps.present_modes) {
        if (mode == config.present_mode) {
            mode_ok = true;
            break;
        }
    }
    if (!mode_ok) return false;

    // Check alpha mode
    bool alpha_ok = false;
    for (const auto& alpha : caps.alpha_modes) {
        if (alpha == config.alpha_mode) {
            alpha_ok = true;
            break;
        }
    }
    if (!alpha_ok) return false;

    // Check extent
    if (config.width < caps.min_width || config.width > caps.max_width) return false;
    if (config.height < caps.min_height || config.height > caps.max_height) return false;

    return true;
}

SurfaceConfig adjust_config_to_capabilities(
    const SurfaceConfig& config,
    const SurfaceCapabilities& caps) {

    SurfaceConfig adjusted = config;

    // Adjust format if needed
    bool format_ok = false;
    for (const auto& fmt : caps.formats) {
        if (fmt == config.format) {
            format_ok = true;
            break;
        }
    }
    if (!format_ok && !caps.formats.empty()) {
        adjusted.format = caps.preferred_format();
    }

    // Adjust present mode if needed
    bool mode_ok = false;
    for (const auto& mode : caps.present_modes) {
        if (mode == config.present_mode) {
            mode_ok = true;
            break;
        }
    }
    if (!mode_ok && !caps.present_modes.empty()) {
        adjusted.present_mode = caps.present_modes[0];
    }

    // Adjust alpha mode if needed
    bool alpha_ok = false;
    for (const auto& alpha : caps.alpha_modes) {
        if (alpha == config.alpha_mode) {
            alpha_ok = true;
            break;
        }
    }
    if (!alpha_ok && !caps.alpha_modes.empty()) {
        adjusted.alpha_mode = caps.alpha_modes[0];
    }

    // Clamp extent
    auto [w, h] = caps.clamp_extent(config.width, config.height);
    adjusted.width = w;
    adjusted.height = h;

    return adjusted;
}

// =============================================================================
// SurfaceState Utilities
// =============================================================================

bool is_surface_ready(SurfaceState state) {
    return state == SurfaceState::Ready;
}

bool is_surface_recoverable(SurfaceState state) {
    switch (state) {
        case SurfaceState::Ready:
        case SurfaceState::Minimized:
        case SurfaceState::Occluded:
        case SurfaceState::Resizing:
            return true;

        case SurfaceState::Lost:
        case SurfaceState::OutOfDate:
            return false; // Need recreation
    }
    return false;
}

const char* surface_state_name(SurfaceState state) {
    switch (state) {
        case SurfaceState::Ready: return "Ready";
        case SurfaceState::Lost: return "Lost";
        case SurfaceState::OutOfDate: return "OutOfDate";
        case SurfaceState::Minimized: return "Minimized";
        case SurfaceState::Occluded: return "Occluded";
        case SurfaceState::Resizing: return "Resizing";
    }
    return "Unknown";
}

// =============================================================================
// SurfaceTexture Utilities
// =============================================================================

std::string format_surface_texture(const SurfaceTexture& texture) {
    std::string result = "SurfaceTexture {\n";

    result += "  size: " + std::to_string(texture.width) + "x" + std::to_string(texture.height) + "\n";
    result += "  format: " + std::string(to_string(texture.format)) + "\n";
    result += "  suboptimal: " + std::string(texture.suboptimal ? "true" : "false") + "\n";
    result += std::string("  native_handle: ") + (texture.native_handle ? "valid" : "null") + "\n";

    result += "}";
    return result;
}

// =============================================================================
// Surface Factory Utilities
// =============================================================================

namespace {

/// Global surface ID counter for unique ID generation
std::atomic<std::uint64_t> g_surface_id_counter{1};

} // anonymous namespace

std::uint64_t generate_surface_id() {
    return g_surface_id_counter.fetch_add(1, std::memory_order_relaxed);
}

void reset_surface_id_counter() {
    g_surface_id_counter.store(1, std::memory_order_relaxed);
}

// =============================================================================
// SurfaceTarget Utilities
// =============================================================================

std::string format_surface_target(const SurfaceTarget& target) {
    if (std::holds_alternative<WindowHandle>(target)) {
        const auto& handle = std::get<WindowHandle>(target);
        return "WindowHandle { hwnd: " + std::to_string(reinterpret_cast<std::uintptr_t>(handle.hwnd)) + " }";
    }
    else if (std::holds_alternative<CanvasHandle>(target)) {
        const auto& handle = std::get<CanvasHandle>(target);
        return "CanvasHandle { canvas_id: " + handle.canvas_id + " }";
    }
    else if (std::holds_alternative<OffscreenConfig>(target)) {
        const auto& config = std::get<OffscreenConfig>(target);
        return "OffscreenConfig { " + std::to_string(config.width) + "x" + std::to_string(config.height) + " }";
    }
    return "UnknownTarget";
}

bool is_window_target(const SurfaceTarget& target) {
    return std::holds_alternative<WindowHandle>(target);
}

bool is_canvas_target(const SurfaceTarget& target) {
    return std::holds_alternative<CanvasHandle>(target);
}

bool is_offscreen_target(const SurfaceTarget& target) {
    return std::holds_alternative<OffscreenConfig>(target);
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_surface_config(const SurfaceConfig& config) {
    std::string result = "SurfaceConfig {\n";

    result += "  size: " + std::to_string(config.width) + "x" + std::to_string(config.height) + "\n";
    result += "  format: " + std::string(to_string(config.format)) + "\n";
    result += "  present_mode: " + std::string(to_string(config.present_mode)) + "\n";
    result += "  alpha_mode: " + std::string(to_string(config.alpha_mode)) + "\n";

    const char* vsync_str = "Unknown";
    switch (config.vsync) {
        case VSync::Off: vsync_str = "Off"; break;
        case VSync::On: vsync_str = "On"; break;
        case VSync::Adaptive: vsync_str = "Adaptive"; break;
    }
    result += "  vsync: " + std::string(vsync_str) + "\n";

    result += "}";
    return result;
}

std::string format_acquired_image(const AcquiredImage& image) {
    std::string result = "AcquiredImage {\n";

    result += "  size: " + std::to_string(image.width) + "x" + std::to_string(image.height) + "\n";
    result += "  format: " + std::string(to_string(image.format)) + "\n";
    result += "  image_index: " + std::to_string(image.image_index) + "\n";
    result += "  suboptimal: " + std::string(image.suboptimal ? "true" : "false") + "\n";
    result += "  texture_id: " + std::to_string(image.texture.id) + "\n";
    result += "  backend: " + std::string(to_string(image.texture.backend)) + "\n";

    result += "}";
    return result;
}

std::string format_gpu_resource_handle(const GpuResourceHandle& handle) {
    return "GpuResourceHandle { id: " + std::to_string(handle.id) +
           ", backend: " + std::string(to_string(handle.backend)) + " }";
}

} // namespace debug

} // namespace void_presenter
