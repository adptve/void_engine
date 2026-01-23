#pragma once

/// @file compositor.hpp
/// @brief Main compositor interface
///
/// Provides the core compositor functionality for display management,
/// frame scheduling, input handling, VRR, and HDR support.

#include "fwd.hpp"
#include "types.hpp"
#include "frame.hpp"
#include "input.hpp"
#include "output.hpp"
#include "vrr.hpp"
#include "hdr.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace void_compositor {

// =============================================================================
// Render Target
// =============================================================================

/// Render target interface
class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    /// Get the size of the render target
    [[nodiscard]] virtual std::pair<std::uint32_t, std::uint32_t> size() const = 0;

    /// Get the format
    [[nodiscard]] virtual RenderFormat format() const = 0;

    /// Get the frame number this target belongs to
    [[nodiscard]] virtual std::uint64_t frame_number() const = 0;

    /// Signal that rendering is complete
    virtual CompositorError present() = 0;

    /// Get native texture handle (platform-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

/// Null render target for testing
class NullRenderTarget : public IRenderTarget {
public:
    NullRenderTarget(std::uint32_t width, std::uint32_t height,
                     RenderFormat fmt, std::uint64_t frame_num)
        : m_width(width), m_height(height), m_format(fmt), m_frame_number(frame_num) {}

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const override {
        return {m_width, m_height};
    }

    [[nodiscard]] RenderFormat format() const override { return m_format; }
    [[nodiscard]] std::uint64_t frame_number() const override { return m_frame_number; }

    CompositorError present() override { return CompositorError::none(); }
    [[nodiscard]] void* native_handle() const override { return nullptr; }

private:
    std::uint32_t m_width;
    std::uint32_t m_height;
    RenderFormat m_format;
    std::uint64_t m_frame_number;
};

// =============================================================================
// Compositor Interface
// =============================================================================

/// Main compositor interface
class ICompositor {
public:
    virtual ~ICompositor() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Check if compositor is running
    [[nodiscard]] virtual bool is_running() const = 0;

    /// Request shutdown
    virtual void shutdown() = 0;

    // =========================================================================
    // Display Management
    // =========================================================================

    /// Get compositor capabilities
    [[nodiscard]] virtual CompositorCapabilities capabilities() const = 0;

    /// Get all connected outputs
    [[nodiscard]] virtual std::vector<IOutput*> outputs() = 0;

    /// Get primary output
    [[nodiscard]] virtual IOutput* primary_output() = 0;

    /// Get output by ID
    [[nodiscard]] virtual IOutput* output(std::uint64_t id) = 0;

    // =========================================================================
    // Frame Management
    // =========================================================================

    /// Dispatch one iteration of the event loop
    virtual CompositorError dispatch() = 0;

    /// Check if a frame should be rendered
    [[nodiscard]] virtual bool should_render() const = 0;

    /// Begin a new frame
    [[nodiscard]] virtual std::unique_ptr<IRenderTarget> begin_frame() = 0;

    /// End the current frame
    virtual CompositorError end_frame(std::unique_ptr<IRenderTarget> target) = 0;

    /// Get the frame scheduler
    [[nodiscard]] virtual FrameScheduler& frame_scheduler() = 0;
    [[nodiscard]] virtual const FrameScheduler& frame_scheduler() const = 0;

    /// Get current frame number
    [[nodiscard]] virtual std::uint64_t frame_number() const = 0;

    // =========================================================================
    // Input
    // =========================================================================

    /// Poll for input events
    [[nodiscard]] virtual std::vector<InputEvent> poll_input() = 0;

    /// Get current input state
    [[nodiscard]] virtual const InputState& input_state() const = 0;

    // =========================================================================
    // VRR
    // =========================================================================

    /// Enable VRR on the primary output
    virtual CompositorError enable_vrr(VrrMode mode) = 0;

    /// Disable VRR
    virtual CompositorError disable_vrr() = 0;

    /// Get VRR capability of primary output
    [[nodiscard]] virtual const VrrCapability* vrr_capability() const = 0;

    /// Get active VRR configuration
    [[nodiscard]] virtual const VrrConfig* vrr_config() const = 0;

    // =========================================================================
    // HDR
    // =========================================================================

    /// Enable HDR on the primary output
    virtual CompositorError enable_hdr(const HdrConfig& config) = 0;

    /// Disable HDR
    virtual CompositorError disable_hdr() = 0;

    /// Get HDR capability of primary output
    [[nodiscard]] virtual const HdrCapability* hdr_capability() const = 0;

    /// Get active HDR configuration
    [[nodiscard]] virtual const HdrConfig* hdr_config() const = 0;

    /// Set HDR metadata
    virtual CompositorError set_hdr_metadata(const HdrConfig& config) = 0;

    // =========================================================================
    // Content Velocity (for VRR)
    // =========================================================================

    /// Update content velocity for VRR adaptation
    virtual void update_content_velocity(float velocity) = 0;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get compositor configuration
    [[nodiscard]] virtual const CompositorConfig& config() const = 0;
};

// =============================================================================
// Null Compositor (for testing)
// =============================================================================

/// Null compositor implementation for testing
class NullCompositor : public ICompositor {
public:
    explicit NullCompositor(const CompositorConfig& config = CompositorConfig{})
        : m_config(config)
        , m_frame_scheduler(config.target_fps)
    {
        // Create a null primary output
        OutputInfo info;
        info.id = 1;
        info.name = "NULL-1";
        info.current_mode = {1920, 1080, 60000};
        info.available_modes = {{1920, 1080, 60000}, {2560, 1440, 60000}, {3840, 2160, 60000}};
        info.primary = true;
        m_primary_output = std::make_unique<NullOutput>(info);

        // Setup capabilities
        m_capabilities.refresh_rates = {60, 120, 144};
        m_capabilities.max_width = 3840;
        m_capabilities.max_height = 2160;
        m_capabilities.current_width = 1920;
        m_capabilities.current_height = 1080;
        m_capabilities.vrr_supported = true;
        m_capabilities.hdr_supported = true;
        m_capabilities.display_count = 1;
        m_capabilities.supported_formats = {
            RenderFormat::Bgra8UnormSrgb,
            RenderFormat::Rgba8UnormSrgb,
            RenderFormat::Rgb10a2Unorm,
            RenderFormat::Rgba16Float,
        };
    }

    [[nodiscard]] bool is_running() const override { return m_running; }
    void shutdown() override { m_running = false; }

    [[nodiscard]] CompositorCapabilities capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] std::vector<IOutput*> outputs() override {
        return {m_primary_output.get()};
    }

    [[nodiscard]] IOutput* primary_output() override {
        return m_primary_output.get();
    }

    [[nodiscard]] IOutput* output(std::uint64_t id) override {
        if (m_primary_output && m_primary_output->info().id == id) {
            return m_primary_output.get();
        }
        return nullptr;
    }

    CompositorError dispatch() override {
        // Simulate frame callback
        m_frame_scheduler.on_frame_callback();
        return CompositorError::none();
    }

    [[nodiscard]] bool should_render() const override {
        return m_frame_scheduler.should_render();
    }

    [[nodiscard]] std::unique_ptr<IRenderTarget> begin_frame() override {
        auto frame_num = m_frame_scheduler.begin_frame();
        auto [w, h] = std::make_pair(
            m_primary_output->info().current_mode.width,
            m_primary_output->info().current_mode.height
        );
        return std::make_unique<NullRenderTarget>(
            w, h, m_config.preferred_format, frame_num
        );
    }

    CompositorError end_frame(std::unique_ptr<IRenderTarget> /*target*/) override {
        m_frame_scheduler.end_frame();
        // Simulate presentation feedback
        PresentationFeedback feedback;
        feedback.presented_at = std::chrono::steady_clock::now();
        feedback.sequence = m_frame_scheduler.frame_number();
        feedback.vsync = m_config.vsync;
        feedback.refresh_rate = 60;
        m_frame_scheduler.on_presentation_feedback(feedback);
        return CompositorError::none();
    }

    [[nodiscard]] FrameScheduler& frame_scheduler() override {
        return m_frame_scheduler;
    }

    [[nodiscard]] const FrameScheduler& frame_scheduler() const override {
        return m_frame_scheduler;
    }

    [[nodiscard]] std::uint64_t frame_number() const override {
        return m_frame_scheduler.frame_number();
    }

    [[nodiscard]] std::vector<InputEvent> poll_input() override {
        return std::exchange(m_pending_input, {});
    }

    [[nodiscard]] const InputState& input_state() const override {
        return m_input_state;
    }

    CompositorError enable_vrr(VrrMode mode) override {
        return m_primary_output->enable_vrr(mode)
            ? CompositorError::none()
            : CompositorError::drm("VRR not supported");
    }

    CompositorError disable_vrr() override {
        m_primary_output->disable_vrr();
        return CompositorError::none();
    }

    [[nodiscard]] const VrrCapability* vrr_capability() const override {
        return &m_primary_output->vrr_capability();
    }

    [[nodiscard]] const VrrConfig* vrr_config() const override {
        auto cfg = m_primary_output->vrr_config();
        if (cfg) {
            m_cached_vrr_config = *cfg;
            return &m_cached_vrr_config;
        }
        return nullptr;
    }

    CompositorError enable_hdr(const HdrConfig& config) override {
        return m_primary_output->enable_hdr(config)
            ? CompositorError::none()
            : CompositorError::drm("HDR not supported");
    }

    CompositorError disable_hdr() override {
        m_primary_output->disable_hdr();
        return CompositorError::none();
    }

    [[nodiscard]] const HdrCapability* hdr_capability() const override {
        return &m_primary_output->hdr_capability();
    }

    [[nodiscard]] const HdrConfig* hdr_config() const override {
        auto cfg = m_primary_output->hdr_config();
        if (cfg) {
            m_cached_hdr_config = *cfg;
            return &m_cached_hdr_config;
        }
        return nullptr;
    }

    CompositorError set_hdr_metadata(const HdrConfig& config) override {
        return m_primary_output->set_hdr_metadata(config)
            ? CompositorError::none()
            : CompositorError::drm("Failed to set HDR metadata");
    }

    void update_content_velocity(float velocity) override {
        m_frame_scheduler.update_content_velocity(velocity);
    }

    [[nodiscard]] const CompositorConfig& config() const override {
        return m_config;
    }

    /// Inject input event (for testing)
    void inject_input(const InputEvent& event) {
        m_pending_input.push_back(event);
        m_input_state.handle_event(event);
    }

private:
    CompositorConfig m_config;
    CompositorCapabilities m_capabilities;
    FrameScheduler m_frame_scheduler;
    std::unique_ptr<NullOutput> m_primary_output;
    InputState m_input_state;
    std::vector<InputEvent> m_pending_input;
    bool m_running = true;

    mutable VrrConfig m_cached_vrr_config;
    mutable HdrConfig m_cached_hdr_config;
};

// =============================================================================
// Compositor Factory
// =============================================================================

/// Factory for creating compositor instances
class CompositorFactory {
public:
    /// Create a compositor for the current platform
    /// @param config Compositor configuration
    /// @return Compositor instance or nullptr on failure
    static std::unique_ptr<ICompositor> create(const CompositorConfig& config = CompositorConfig{});

    /// Create a null compositor for testing
    static std::unique_ptr<ICompositor> create_null(const CompositorConfig& config = CompositorConfig{}) {
        return std::make_unique<NullCompositor>(config);
    }

    /// Check if a compositor is available on this platform
    [[nodiscard]] static bool is_available();

    /// Get available compositor backend name
    [[nodiscard]] static const char* backend_name();
};

} // namespace void_compositor
