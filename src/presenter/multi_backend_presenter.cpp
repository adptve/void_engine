/// @file multi_backend_presenter.cpp
/// @brief MultiBackendPresenter implementation for void_presenter

#include <void_engine/presenter/multi_backend_presenter.hpp>
#include <void_engine/presenter/backend.hpp>
#include <void_engine/presenter/backends/null_backend.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace void_presenter {

// =============================================================================
// MultiBackendPresenterConfig Utilities
// =============================================================================

MultiBackendPresenterConfig create_default_multi_backend_config() {
    MultiBackendPresenterConfig config;
    config.backend_config.preferred_type = BackendFactory::recommended();
    config.backend_config.fallback_types = {
        BackendType::Wgpu,
        BackendType::OpenGL,
        BackendType::Null,
    };
    config.target_fps = 60;
    config.enable_frame_pacing = true;
    config.track_detailed_stats = true;
    config.stats_history_size = 300;
    config.enable_hot_swap = true;
    config.enable_validation = false;
    config.enable_debug_markers = false;
    return config;
}

MultiBackendPresenterConfig create_xr_multi_backend_config(
    const xr::XrSessionConfig& xr_config) {

    MultiBackendPresenterConfig config = create_default_multi_backend_config();
    config.xr_config = xr_config;
    config.target_fps = 90; // Common XR refresh rate
    config.enable_frame_pacing = true;
    return config;
}

MultiBackendPresenterConfig create_debug_multi_backend_config() {
    MultiBackendPresenterConfig config = create_default_multi_backend_config();
    config.enable_validation = true;
    config.enable_debug_markers = true;
    config.track_detailed_stats = true;
    return config;
}

// =============================================================================
// OutputTargetConfig Utilities
// =============================================================================

OutputTargetConfig create_window_target_config(
    const std::string& name,
    std::uint32_t width,
    std::uint32_t height,
    bool is_primary) {

    OutputTargetConfig config;
    config.type = OutputTargetType::Window;
    config.name = name;
    config.swapchain_config.width = width;
    config.swapchain_config.height = height;
    config.swapchain_config.format = SurfaceFormat::Bgra8UnormSrgb;
    config.swapchain_config.present_mode = PresentMode::Fifo;
    config.swapchain_config.image_count = 3; // Triple buffering
    config.is_primary = is_primary;
    config.auto_resize = true;
    return config;
}

OutputTargetConfig create_offscreen_target_config(
    const std::string& name,
    std::uint32_t width,
    std::uint32_t height) {

    OutputTargetConfig config;
    config.type = OutputTargetType::Offscreen;
    config.name = name;
    config.swapchain_config.width = width;
    config.swapchain_config.height = height;
    config.swapchain_config.format = SurfaceFormat::Rgba8UnormSrgb;
    config.swapchain_config.present_mode = PresentMode::Immediate;
    config.swapchain_config.image_count = 2; // Double buffering for offscreen
    config.is_primary = false;
    config.auto_resize = false;
    return config;
}

OutputTargetConfig create_xr_target_config(
    const std::string& name,
    std::uint32_t width,
    std::uint32_t height) {

    OutputTargetConfig config;
    config.type = OutputTargetType::XrStereo;
    config.name = name;
    config.swapchain_config.width = width;
    config.swapchain_config.height = height;
    config.swapchain_config.format = SurfaceFormat::Rgba8UnormSrgb;
    config.swapchain_config.present_mode = PresentMode::Immediate; // XR handles timing
    config.swapchain_config.image_count = 3;
    config.is_primary = true; // XR targets are typically primary
    config.auto_resize = false; // XR manages resolution
    return config;
}

// =============================================================================
// PresenterStatistics Utilities
// =============================================================================

std::string format_presenter_statistics(const PresenterStatistics& stats) {
    std::string result = "PresenterStatistics {\n";

    result += "  total_frames: " + std::to_string(stats.total_frames) + "\n";
    result += "  frames_presented: " + std::to_string(stats.frames_presented) + "\n";
    result += "  frames_dropped: " + std::to_string(stats.frames_dropped) + "\n";
    result += "  drop_rate: " + std::to_string(stats.drop_rate() * 100.0) + "%\n";

    result += "  avg_frame_time_us: " + std::to_string(stats.avg_frame_time_us) + "\n";
    result += "  avg_fps: " + std::to_string(stats.average_fps()) + "\n";
    result += "  frame_time_p99: " + std::to_string(stats.frame_time_p99_us) + " us\n";
    result += "  frame_time_p95: " + std::to_string(stats.frame_time_p95_us) + " us\n";
    result += "  frame_time_p50: " + std::to_string(stats.frame_time_p50_us) + " us\n";

    if (stats.min_frame_time_us != UINT64_MAX) {
        result += "  min_frame_time: " + std::to_string(stats.min_frame_time_us) + " us\n";
    }
    result += "  max_frame_time: " + std::to_string(stats.max_frame_time_us) + " us\n";

    result += "  current_backend: " + std::string(to_string(stats.current_backend)) + "\n";
    result += "  backend_switches: " + std::to_string(stats.backend_switches) + "\n";
    result += "  swapchain_recreates: " + std::to_string(stats.swapchain_recreates) + "\n";

    if (stats.xr_active) {
        result += "  xr_active: true\n";
        result += "  xr_compositor_time_us: " + std::to_string(stats.xr_compositor_time_us) + "\n";
        result += "  xr_frames_reprojected: " + std::to_string(stats.xr_frames_reprojected) + "\n";
    }

    result += "}";
    return result;
}

PresenterStatistics merge_statistics(
    const PresenterStatistics& a,
    const PresenterStatistics& b) {

    PresenterStatistics merged;

    merged.total_frames = a.total_frames + b.total_frames;
    merged.frames_presented = a.frames_presented + b.frames_presented;
    merged.frames_dropped = a.frames_dropped + b.frames_dropped;

    // Weighted average for timing stats
    double total = static_cast<double>(merged.frames_presented);
    if (total > 0.0) {
        double a_weight = static_cast<double>(a.frames_presented) / total;
        double b_weight = static_cast<double>(b.frames_presented) / total;

        merged.avg_frame_time_us = a.avg_frame_time_us * a_weight + b.avg_frame_time_us * b_weight;
        merged.avg_cpu_time_us = a.avg_cpu_time_us * a_weight + b.avg_cpu_time_us * b_weight;
        merged.avg_gpu_time_us = a.avg_gpu_time_us * a_weight + b.avg_gpu_time_us * b_weight;
        merged.avg_present_latency_us = a.avg_present_latency_us * a_weight + b.avg_present_latency_us * b_weight;
    }

    // Min/max
    merged.min_frame_time_us = std::min(a.min_frame_time_us, b.min_frame_time_us);
    merged.max_frame_time_us = std::max(a.max_frame_time_us, b.max_frame_time_us);

    // Backend info from most recent (b)
    merged.current_backend = b.current_backend;
    merged.backend_switches = a.backend_switches + b.backend_switches;
    merged.swapchain_recreates = a.swapchain_recreates + b.swapchain_recreates;

    // XR stats
    merged.xr_active = b.xr_active;
    merged.xr_compositor_time_us = b.xr_compositor_time_us;
    merged.xr_frames_reprojected = a.xr_frames_reprojected + b.xr_frames_reprojected;

    return merged;
}

// =============================================================================
// BackendSwitchEvent Utilities
// =============================================================================

std::string format_backend_switch_event(const BackendSwitchEvent& event) {
    std::string result = "BackendSwitchEvent {\n";

    result += "  old_backend: " + std::string(to_string(event.old_backend)) + "\n";
    result += "  new_backend: " + std::string(to_string(event.new_backend)) + "\n";

    const char* reason_str = "Unknown";
    switch (event.reason) {
        case BackendSwitchReason::UserRequested: reason_str = "UserRequested"; break;
        case BackendSwitchReason::DeviceLost: reason_str = "DeviceLost"; break;
        case BackendSwitchReason::PerformanceHint: reason_str = "PerformanceHint"; break;
        case BackendSwitchReason::XrSessionStart: reason_str = "XrSessionStart"; break;
        case BackendSwitchReason::XrSessionEnd: reason_str = "XrSessionEnd"; break;
    }
    result += "  reason: " + std::string(reason_str) + "\n";

    result += "  success: " + std::string(event.success ? "true" : "false") + "\n";
    if (!event.success && !event.error_message.empty()) {
        result += "  error: " + event.error_message + "\n";
    }

    result += "}";
    return result;
}

const char* backend_switch_reason_name(BackendSwitchReason reason) {
    switch (reason) {
        case BackendSwitchReason::UserRequested: return "UserRequested";
        case BackendSwitchReason::DeviceLost: return "DeviceLost";
        case BackendSwitchReason::PerformanceHint: return "PerformanceHint";
        case BackendSwitchReason::XrSessionStart: return "XrSessionStart";
        case BackendSwitchReason::XrSessionEnd: return "XrSessionEnd";
    }
    return "Unknown";
}

// =============================================================================
// OutputTargetStatus Utilities
// =============================================================================

std::string format_output_target_status(const OutputTargetStatus& status) {
    std::string result = "OutputTargetStatus {\n";

    result += "  id: " + std::to_string(status.id.id) + "\n";

    const char* type_str = "Unknown";
    switch (status.type) {
        case OutputTargetType::Window: type_str = "Window"; break;
        case OutputTargetType::Canvas: type_str = "Canvas"; break;
        case OutputTargetType::Offscreen: type_str = "Offscreen"; break;
        case OutputTargetType::XrStereo: type_str = "XrStereo"; break;
    }
    result += "  type: " + std::string(type_str) + "\n";

    result += "  swapchain_state: " + std::string(to_string(status.swapchain_state)) + "\n";
    result += "  size: " + std::to_string(status.width) + "x" + std::to_string(status.height) + "\n";
    result += "  frames_presented: " + std::to_string(status.frames_presented) + "\n";
    result += "  is_primary: " + std::string(status.is_primary ? "true" : "false") + "\n";

    result += "}";
    return result;
}

const char* output_target_type_name(OutputTargetType type) {
    switch (type) {
        case OutputTargetType::Window: return "Window";
        case OutputTargetType::Canvas: return "Canvas";
        case OutputTargetType::Offscreen: return "Offscreen";
        case OutputTargetType::XrStereo: return "XrStereo";
    }
    return "Unknown";
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_multi_backend_presenter_state(const MultiBackendPresenter& presenter) {
    std::string result = "MultiBackendPresenter {\n";

    result += "  running: " + std::string(presenter.is_running() ? "true" : "false") + "\n";
    result += "  current_backend: " + std::string(to_string(presenter.current_backend())) + "\n";
    result += "  xr_active: " + std::string(presenter.is_xr_active() ? "true" : "false") + "\n";

    auto targets = presenter.get_all_targets();
    result += "  output_targets: " + std::to_string(targets.size()) + "\n";

    auto stats = presenter.statistics();
    result += "  total_frames: " + std::to_string(stats.total_frames) + "\n";
    result += "  avg_fps: " + std::to_string(stats.average_fps()) + "\n";

    result += "}";
    return result;
}

std::string format_output_target_config(const OutputTargetConfig& config) {
    std::string result = "OutputTargetConfig {\n";

    result += "  type: " + std::string(output_target_type_name(config.type)) + "\n";
    result += "  name: " + config.name + "\n";
    result += "  size: " + std::to_string(config.swapchain_config.width) + "x" +
              std::to_string(config.swapchain_config.height) + "\n";
    result += "  format: " + std::string(to_string(config.swapchain_config.format)) + "\n";
    result += "  present_mode: " + std::string(to_string(config.swapchain_config.present_mode)) + "\n";
    result += "  is_primary: " + std::string(config.is_primary ? "true" : "false") + "\n";
    result += "  auto_resize: " + std::string(config.auto_resize ? "true" : "false") + "\n";

    result += "}";
    return result;
}

} // namespace debug

} // namespace void_presenter
