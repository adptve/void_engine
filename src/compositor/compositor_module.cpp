/// @file compositor_module.cpp
/// @brief Compositor module main implementation
///
/// Provides internal compositor helpers and hot-reload registration utilities.

#include <void_engine/compositor/compositor_module.hpp>
#include <void_engine/compositor/compositor.hpp>
#include <void_engine/compositor/frame.hpp>
#include <void_engine/compositor/vrr.hpp>
#include <void_engine/compositor/hdr.hpp>
#include <void_engine/compositor/layer.hpp>
#include <void_engine/compositor/layer_compositor.hpp>
#include <void_engine/compositor/rehydration.hpp>
#include <void_engine/compositor/snapshot.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <chrono>
#include <memory>
#include <sstream>

namespace void_compositor {

// =============================================================================
// Compositor Hot-Reload Wrapper
// =============================================================================

/// Hot-reloadable wrapper for ICompositor instances
class CompositorHotReloadWrapper : public void_core::HotReloadable {
public:
    explicit CompositorHotReloadWrapper(ICompositor& compositor)
        : m_compositor(compositor) {}

    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        void_core::HotReloadSnapshot snap(
            {}, std::type_index(typeid(ICompositor)), "ICompositor",
            void_core::Version(1, 0, 0));

        // Serialize compositor state
        BinaryWriter writer;

        // Frame scheduler state
        const auto& scheduler = m_compositor.frame_scheduler();
        writer.write_u32(scheduler.target_fps());
        writer.write_u64(scheduler.frame_number());
        writer.write_f32(static_cast<float>(scheduler.content_velocity()));

        // Config
        const auto& config = m_compositor.config();
        writer.write_u32(config.target_fps);
        writer.write_bool(config.vsync);
        writer.write_bool(config.enable_vrr);
        writer.write_bool(config.enable_hdr);
        writer.write_u32(static_cast<std::uint32_t>(config.preferred_format));

        // VRR state
        if (const auto* vrr_cfg = m_compositor.vrr_config()) {
            writer.write_bool(true);
            writer.write_bool(vrr_cfg->enabled);
            writer.write_u32(vrr_cfg->min_refresh_rate);
            writer.write_u32(vrr_cfg->max_refresh_rate);
            writer.write_u32(vrr_cfg->current_refresh_rate);
            writer.write_u32(static_cast<std::uint32_t>(vrr_cfg->mode));
        } else {
            writer.write_bool(false);
        }

        // HDR state
        if (const auto* hdr_cfg = m_compositor.hdr_config()) {
            writer.write_bool(true);
            writer.write_bool(hdr_cfg->enabled);
            writer.write_u32(static_cast<std::uint32_t>(hdr_cfg->transfer_function));
            writer.write_u32(static_cast<std::uint32_t>(hdr_cfg->color_primaries));
            writer.write_u32(hdr_cfg->max_luminance);
        } else {
            writer.write_bool(false);
        }

        snap.data = writer.take();
        return void_core::Ok(std::move(snap));
    }

    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (snap.data.size() < 20) {
            return void_core::Err("Invalid compositor snapshot data");
        }

        BinaryReader reader(snap.data);

        // Restore frame scheduler state
        std::uint32_t target_fps = reader.read_u32();
        /* std::uint64_t frame_number = */ reader.read_u64();
        float content_velocity = reader.read_f32();

        auto& scheduler = m_compositor.frame_scheduler();
        scheduler.set_target_fps(target_fps);
        m_compositor.update_content_velocity(content_velocity);

        // Config is read-only after creation, skip restoration

        // Skip config restoration (already read)
        reader.read_u32();  // target_fps
        reader.read_bool(); // vsync
        reader.read_bool(); // enable_vrr
        reader.read_bool(); // enable_hdr
        reader.read_u32();  // preferred_format

        // Restore VRR state
        bool has_vrr = reader.read_bool();
        if (has_vrr) {
            VrrConfig vrr_cfg;
            vrr_cfg.enabled = reader.read_bool();
            vrr_cfg.min_refresh_rate = reader.read_u32();
            vrr_cfg.max_refresh_rate = reader.read_u32();
            vrr_cfg.current_refresh_rate = reader.read_u32();
            vrr_cfg.mode = static_cast<VrrMode>(reader.read_u32());

            if (vrr_cfg.enabled) {
                m_compositor.enable_vrr(vrr_cfg.mode);
            }
        }

        // Restore HDR state
        bool has_hdr = reader.read_bool();
        if (has_hdr) {
            HdrConfig hdr_cfg;
            hdr_cfg.enabled = reader.read_bool();
            hdr_cfg.transfer_function = static_cast<TransferFunction>(reader.read_u32());
            hdr_cfg.color_primaries = static_cast<ColorPrimaries>(reader.read_u32());
            hdr_cfg.max_luminance = reader.read_u32();

            if (hdr_cfg.enabled) {
                m_compositor.enable_hdr(hdr_cfg);
            }
        }

        return void_core::Ok();
    }

    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override {
        // Compatible with any 1.x version
        return new_version.major == 1;
    }

    [[nodiscard]] void_core::Version current_version() const override {
        return void_core::Version(1, 0, 0);
    }

    [[nodiscard]] std::string type_name() const override {
        return "void_compositor::ICompositor";
    }

private:
    ICompositor& m_compositor;
};

// =============================================================================
// Hot-Reload Registration
// =============================================================================

/// Register a compositor with the hot-reload manager
void_core::Result<void> register_compositor_hot_reload(
    void_core::HotReloadManager& manager,
    ICompositor& compositor,
    const std::string& name) {

    auto wrapper = std::make_unique<CompositorHotReloadWrapper>(compositor);
    return manager.register_object(name, std::move(wrapper));
}

// =============================================================================
// Compositor Diagnostics
// =============================================================================

/// Format compositor state for debugging
[[nodiscard]] std::string format_compositor_state(const ICompositor& compositor) {
    std::ostringstream ss;

    ss << "Compositor State:\n";
    ss << "  Running: " << (compositor.is_running() ? "yes" : "no") << "\n";
    ss << "  Frame: " << compositor.frame_number() << "\n";

    const auto& scheduler = compositor.frame_scheduler();
    ss << "  FPS: " << scheduler.current_fps() << " / " << scheduler.target_fps() << " target\n";
    ss << "  Hitting target: " << (scheduler.hitting_target() ? "yes" : "no") << "\n";

    if (const auto* vrr = compositor.vrr_config()) {
        ss << "  VRR: " << (vrr->enabled ? "enabled" : "disabled");
        if (vrr->enabled) {
            ss << " [" << vrr->min_refresh_rate << "-" << vrr->max_refresh_rate << " Hz]";
            ss << " @ " << vrr->current_refresh_rate << " Hz";
        }
        ss << "\n";
    } else {
        ss << "  VRR: not available\n";
    }

    if (const auto* hdr = compositor.hdr_config()) {
        ss << "  HDR: " << (hdr->enabled ? "enabled" : "disabled");
        if (hdr->enabled) {
            ss << " [" << to_string(hdr->transfer_function) << "]";
            ss << " max " << hdr->max_luminance << " nits";
        }
        ss << "\n";
    } else {
        ss << "  HDR: not available\n";
    }

    const auto& caps = compositor.capabilities();
    ss << "  Max resolution: " << caps.max_width << "x" << caps.max_height << "\n";
    ss << "  Displays: " << caps.display_count << "\n";

    return ss.str();
}

/// Format frame scheduler stats
[[nodiscard]] std::string format_frame_stats(const FrameScheduler& scheduler) {
    std::ostringstream ss;

    ss << "Frame Statistics:\n";
    ss << "  Frame: " << scheduler.frame_number() << "\n";
    ss << "  FPS: " << scheduler.current_fps() << " / " << scheduler.target_fps() << " target\n";

    auto avg = scheduler.average_frame_time();
    auto p50 = scheduler.frame_time_p50();
    auto p95 = scheduler.frame_time_p95();
    auto p99 = scheduler.frame_time_p99();

    ss << "  Avg frame time: " << (avg.count() / 1'000'000.0) << " ms\n";
    ss << "  P50: " << (p50.count() / 1'000'000.0) << " ms\n";
    ss << "  P95: " << (p95.count() / 1'000'000.0) << " ms\n";
    ss << "  P99: " << (p99.count() / 1'000'000.0) << " ms\n";
    ss << "  Dropped: " << scheduler.dropped_frame_count() << "\n";

    return ss.str();
}

// =============================================================================
// Module Utility Functions
// =============================================================================

/// Check if HDR is supported on any output
[[nodiscard]] bool any_output_supports_hdr(ICompositor& compositor) {
    for (auto* output : compositor.outputs()) {
        if (output->hdr_capability().supported) {
            return true;
        }
    }
    return false;
}

/// Check if VRR is supported on any output
[[nodiscard]] bool any_output_supports_vrr(ICompositor& compositor) {
    for (auto* output : compositor.outputs()) {
        if (output->vrr_capability().supported) {
            return true;
        }
    }
    return false;
}

/// Get maximum refresh rate across all outputs
[[nodiscard]] std::uint32_t get_max_refresh_rate(ICompositor& compositor) {
    std::uint32_t max_rate = 0;
    for (auto* output : compositor.outputs()) {
        for (const auto& mode : output->info().available_modes) {
            max_rate = std::max(max_rate, mode.refresh_hz());
        }
    }
    return max_rate;
}

/// Get maximum resolution across all outputs
[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> get_max_resolution(ICompositor& compositor) {
    std::uint32_t max_w = 0;
    std::uint32_t max_h = 0;
    for (auto* output : compositor.outputs()) {
        for (const auto& mode : output->info().available_modes) {
            if (mode.width * mode.height > max_w * max_h) {
                max_w = mode.width;
                max_h = mode.height;
            }
        }
    }
    return {max_w, max_h};
}

/// Select optimal configuration for gaming
CompositorConfig optimal_gaming_config(ICompositor& compositor) {
    CompositorConfig config;

    // Target highest refresh rate
    config.target_fps = get_max_refresh_rate(compositor);
    if (config.target_fps == 0) config.target_fps = 60;

    // Enable VSync for tear-free
    config.vsync = true;

    // Allow tearing for lowest latency in VRR
    config.allow_tearing = any_output_supports_vrr(compositor);

    // Enable VRR if available
    config.enable_vrr = any_output_supports_vrr(compositor);

    // Enable HDR if available
    config.enable_hdr = any_output_supports_hdr(compositor);

    // Prefer wide color format for HDR
    config.preferred_format = config.enable_hdr
        ? RenderFormat::Rgba16Float
        : RenderFormat::Bgra8UnormSrgb;

    return config;
}

/// Select optimal configuration for video playback
CompositorConfig optimal_video_config(std::uint32_t video_fps) {
    CompositorConfig config;

    // Match video frame rate
    config.target_fps = video_fps;
    config.vsync = true;
    config.allow_tearing = false;

    // VRR helps with judder
    config.enable_vrr = true;

    // HDR for content that supports it
    config.enable_hdr = true;

    config.preferred_format = RenderFormat::Rgb10a2Unorm;

    return config;
}

} // namespace void_compositor
