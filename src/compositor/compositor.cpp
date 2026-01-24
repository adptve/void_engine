/// @file compositor.cpp
/// @brief Compositor factory implementation

#include <void_engine/compositor/compositor.hpp>
#include <void_engine/compositor/layer.hpp>
#include <void_engine/compositor/layer_compositor.hpp>
#include <void_engine/compositor/rehydration.hpp>

#include <memory>

namespace void_compositor {

// =============================================================================
// Layer Integrated Compositor
// =============================================================================

/// Compositor with integrated layer management
class LayerIntegratedCompositor : public ICompositor {
public:
    LayerIntegratedCompositor(const CompositorConfig& config)
        : m_config(config)
        , m_frame_scheduler(config.target_fps)
        , m_layer_manager(std::make_unique<LayerManager>())
    {
        // Create layer compositor
        LayerCompositorConfig lc_config;
        m_layer_compositor = LayerCompositorFactory::create(lc_config);

        // Create a null primary output
        OutputInfo info;
        info.id = 1;
        info.name = "PRIMARY-1";
        info.current_mode = {1920, 1080, 60000};
        info.available_modes = {
            {1920, 1080, 60000},
            {2560, 1440, 60000},
            {3840, 2160, 60000}
        };
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

        // Initialize layer compositor
        m_layer_compositor->initialize(1920, 1080);
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    [[nodiscard]] bool is_running() const override { return m_running; }

    void shutdown() override {
        m_running = false;
        m_layer_compositor->shutdown();
    }

    // =========================================================================
    // Display Management
    // =========================================================================

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

    // =========================================================================
    // Frame Management
    // =========================================================================

    CompositorError dispatch() override {
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

        // Begin layer compositor frame
        m_layer_compositor->begin_frame();

        return std::make_unique<NullRenderTarget>(
            w, h, m_config.preferred_format, frame_num
        );
    }

    CompositorError end_frame(std::unique_ptr<IRenderTarget> target) override {
        // Composite layers
        m_layer_compositor->composite(*m_layer_manager, nullptr);
        m_layer_compositor->end_frame();

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

    // =========================================================================
    // Input
    // =========================================================================

    [[nodiscard]] std::vector<InputEvent> poll_input() override {
        return std::exchange(m_pending_input, {});
    }

    [[nodiscard]] const InputState& input_state() const override {
        return m_input_state;
    }

    // =========================================================================
    // VRR
    // =========================================================================

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

    // =========================================================================
    // HDR
    // =========================================================================

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

    // =========================================================================
    // Content Velocity
    // =========================================================================

    void update_content_velocity(float velocity) override {
        m_frame_scheduler.update_content_velocity(velocity);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    [[nodiscard]] const CompositorConfig& config() const override {
        return m_config;
    }

    // =========================================================================
    // Layer Management Access
    // =========================================================================

    /// Get the layer manager
    [[nodiscard]] LayerManager& layer_manager() { return *m_layer_manager; }
    [[nodiscard]] const LayerManager& layer_manager() const { return *m_layer_manager; }

    /// Get the layer compositor
    [[nodiscard]] ILayerCompositor& layer_compositor() { return *m_layer_compositor; }
    [[nodiscard]] const ILayerCompositor& layer_compositor() const { return *m_layer_compositor; }

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
    std::unique_ptr<LayerManager> m_layer_manager;
    std::unique_ptr<ILayerCompositor> m_layer_compositor;
    InputState m_input_state;
    std::vector<InputEvent> m_pending_input;
    bool m_running = true;

    mutable VrrConfig m_cached_vrr_config;
    mutable HdrConfig m_cached_hdr_config;
};

// =============================================================================
// Compositor Factory Implementation
// =============================================================================

std::unique_ptr<ICompositor> CompositorFactory::create(const CompositorConfig& config) {
    // Platform-specific compositor creation
    // For now, return layer-integrated compositor as the default

#if defined(__linux__) && defined(VOID_HAS_SMITHAY)
    // Linux with Smithay: Create DRM compositor
    // return std::make_unique<SmithayCompositor>(config);
#endif

#if defined(_WIN32) && defined(VOID_HAS_DCOMP)
    // Windows with DirectComposition
    // return std::make_unique<DirectCompositionCompositor>(config);
#endif

#if defined(__APPLE__)
    // macOS: Could use Core Graphics or Metal compositor
    // return std::make_unique<CoreGraphicsCompositor>(config);
#endif

#if defined(__EMSCRIPTEN__)
    // Web: WebGL/Canvas compositor
    // return std::make_unique<WebCompositor>(config);
#endif

    // Default: Layer-integrated compositor with software layer compositing
    return std::make_unique<LayerIntegratedCompositor>(config);
}

bool CompositorFactory::is_available() {
#if defined(__linux__) && defined(VOID_HAS_SMITHAY)
    return true;
#elif defined(_WIN32)
    return true;
#elif defined(__APPLE__)
    return true;
#elif defined(__EMSCRIPTEN__)
    return true;
#else
    return true; // Layer-integrated compositor is always available
#endif
}

const char* CompositorFactory::backend_name() {
#if defined(__linux__) && defined(VOID_HAS_SMITHAY)
    return "Smithay (DRM/KMS)";
#elif defined(_WIN32)
#if defined(VOID_HAS_DCOMP)
    return "Windows (DirectComposition)";
#else
    return "Windows (Software Layer Compositor)";
#endif
#elif defined(__APPLE__)
    return "macOS (Core Graphics)";
#elif defined(__EMSCRIPTEN__)
    return "Web (Canvas/WebGL)";
#else
    return "Software Layer Compositor";
#endif
}

} // namespace void_compositor
