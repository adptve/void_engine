/// @file xr_system.cpp
/// @brief XR system implementation with stub/desktop backend

#include <void_engine/presenter/xr/xr_system.hpp>

#include <chrono>
#include <cmath>

namespace void_presenter {
namespace xr {

// =============================================================================
// Stub/Desktop XR Session
// =============================================================================

/// @brief Desktop XR session for development without VR hardware
class StubXrSession : public IXrSession {
public:
    explicit StubXrSession(const XrSessionConfig& config)
        : config_(config)
        , state_(XrSessionState::Idle) {
        // Initialize stereo views with default values
        views_.left.eye = Eye::Left;
        views_.left.width = 1920;
        views_.left.height = 1080;
        views_.left.fov = Fov::symmetric(1.57f, 1.57f);  // ~90 degrees
        views_.left.pose.position = {-0.032f, 1.6f, 0.0f};  // Left eye offset

        views_.right.eye = Eye::Right;
        views_.right.width = 1920;
        views_.right.height = 1080;
        views_.right.fov = Fov::symmetric(1.57f, 1.57f);
        views_.right.pose.position = {0.032f, 1.6f, 0.0f};  // Right eye offset

        // Initialize head pose at standing height
        head_pose_.pose.position = {0.0f, 1.6f, 0.0f};
        head_pose_.pose.orientation = Quat::identity();
        head_pose_.position_valid = true;
        head_pose_.orientation_valid = true;
    }

    [[nodiscard]] XrSessionState state() const override { return state_; }
    [[nodiscard]] const XrSessionConfig& config() const override { return config_; }

    bool begin() override {
        if (state_ != XrSessionState::Idle && state_ != XrSessionState::Ready) {
            return false;
        }
        state_ = XrSessionState::Focused;
        return true;
    }

    void end() override {
        state_ = XrSessionState::Stopping;
        state_ = XrSessionState::Idle;
    }

    void request_exit() override {
        state_ = XrSessionState::Exiting;
    }

    bool wait_frame(XrFrame& out_frame) override {
        if (state_ != XrSessionState::Focused && state_ != XrSessionState::Visible) {
            out_frame.should_render = false;
            return false;
        }

        // Update timing
        auto now = std::chrono::steady_clock::now();
        out_frame.frame_number = frame_number_++;
        out_frame.timing.frame_begin = now;
        out_frame.timing.predicted_display_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count() + 16666667;  // +16.67ms
        out_frame.timing.predicted_display_period = 16666667;  // 60Hz

        // Simulate head movement (gentle sway for testing)
        float t = static_cast<float>(frame_number_) * 0.016f;
        head_pose_.pose.position.x = std::sin(t * 0.5f) * 0.05f;
        head_pose_.pose.position.y = 1.6f + std::sin(t * 0.3f) * 0.02f;

        // Update views relative to head
        views_.left.pose = head_pose_.pose;
        views_.left.pose.position = views_.left.pose.position + Vec3{-0.032f, 0.0f, 0.0f};
        views_.right.pose = head_pose_.pose;
        views_.right.pose.position = views_.right.pose.position + Vec3{0.032f, 0.0f, 0.0f};

        out_frame.views = views_;
        out_frame.head_pose = head_pose_;
        out_frame.should_render = true;
        out_frame.session_active = true;

        // Simulate controllers
        if (config_.enable_hand_tracking) {
            out_frame.left_controller = simulate_controller(Hand::Left, t);
            out_frame.right_controller = simulate_controller(Hand::Right, t);
        }

        return true;
    }

    void begin_frame() override {
        // No-op for stub
    }

    void end_frame(const XrStereoTargets& /*views*/) override {
        // No-op for stub - would copy to display in real implementation
    }

    [[nodiscard]] XrStereoTargets acquire_swapchain_images() override {
        XrStereoTargets targets;
        targets.left.width = views_.left.width;
        targets.left.height = views_.left.height;
        targets.left.format = config_.color_format;
        targets.right.width = views_.right.width;
        targets.right.height = views_.right.height;
        targets.right.format = config_.color_format;
        return targets;
    }

    void release_swapchain_images() override {
        // No-op for stub
    }

    [[nodiscard]] StereoViews get_views() const override {
        return views_;
    }

    [[nodiscard]] TrackedPose get_head_pose() const override {
        return head_pose_;
    }

    [[nodiscard]] std::optional<ControllerState> get_controller(Hand hand) const override {
        if (!config_.enable_hand_tracking) {
            return std::nullopt;
        }
        float t = static_cast<float>(frame_number_) * 0.016f;
        return simulate_controller(hand, t);
    }

    [[nodiscard]] std::optional<HandTrackingData> get_hand_tracking(Hand hand) const override {
        if (!config_.enable_hand_tracking) {
            return std::nullopt;
        }
        return simulate_hand_tracking(hand);
    }

    [[nodiscard]] std::optional<StageBounds> get_stage_bounds() const override {
        // Return a default 2m x 2m play area
        StageBounds bounds;
        bounds.width = 2.0f;
        bounds.depth = 2.0f;
        bounds.boundary_points = {
            {-1.0f, 0.0f, -1.0f},
            { 1.0f, 0.0f, -1.0f},
            { 1.0f, 0.0f,  1.0f},
            {-1.0f, 0.0f,  1.0f},
        };
        return bounds;
    }

    void set_foveation(const FoveatedRenderingConfig& foveation) override {
        foveation_ = foveation;
    }

    void trigger_haptic(Hand /*hand*/, float /*amplitude*/, float /*duration*/) override {
        // No-op for stub - no haptic feedback
    }

    void poll_events() override {
        // Dispatch any pending events
        if (event_callback_) {
            // Could simulate events here if needed
        }
    }

    void set_event_callback(XrEventCallback callback) override {
        event_callback_ = std::move(callback);
    }

private:
    [[nodiscard]] ControllerState simulate_controller(Hand hand, float t) const {
        ControllerState state;
        state.hand = hand;
        state.active = true;

        // Position controller in front and to the side
        float side = (hand == Hand::Left) ? -0.3f : 0.3f;
        state.pose.pose.position = {
            side + std::sin(t + side) * 0.05f,
            1.0f + std::sin(t * 1.5f) * 0.03f,
            -0.4f
        };
        state.pose.pose.orientation = Quat::identity();
        state.pose.position_valid = true;
        state.pose.orientation_valid = true;

        // Simulate some input for testing
        state.trigger = 0.0f;
        state.grip = 0.0f;

        return state;
    }

    [[nodiscard]] HandTrackingData simulate_hand_tracking(Hand hand) const {
        HandTrackingData data;
        data.hand = hand;
        data.active = true;

        // Position hand
        float side = (hand == Hand::Left) ? -0.25f : 0.25f;
        Vec3 palm_pos = {side, 1.0f, -0.3f};

        // Set palm and wrist
        data.joints[0].pose.position = palm_pos;  // Palm
        data.joints[0].radius = 0.03f;
        data.joints[0].valid = true;

        data.joints[1].pose.position = palm_pos + Vec3{0.0f, 0.0f, 0.08f};  // Wrist
        data.joints[1].radius = 0.025f;
        data.joints[1].valid = true;

        // Set finger tips (simplified)
        for (int i = 2; i < static_cast<int>(HandJoint::Count); ++i) {
            data.joints[i].pose.position = palm_pos + Vec3{0.0f, 0.0f, -0.1f};
            data.joints[i].radius = 0.01f;
            data.joints[i].valid = true;
        }

        return data;
    }

    XrSessionConfig config_;
    XrSessionState state_;
    StereoViews views_;
    TrackedPose head_pose_;
    FoveatedRenderingConfig foveation_;
    XrEventCallback event_callback_;
    std::uint64_t frame_number_ = 0;
};

// =============================================================================
// Stub/Desktop XR System
// =============================================================================

/// @brief Desktop XR system for development without VR hardware
class StubXrSystem : public IXrSystem {
public:
    StubXrSystem() {
        // Initialize runtime info
        runtime_info_.name = "Void Engine Stub XR";
        runtime_info_.version = "1.0.0";
        runtime_info_.system_type = XrSystemType::HeadMountedVR;
        runtime_info_.system_id = 1;

        // Initialize capabilities
        capabilities_.hand_tracking = true;
        capabilities_.eye_tracking = false;
        capabilities_.foveated_rendering = false;
        capabilities_.passthrough = false;
        capabilities_.spatial_anchors = true;
        capabilities_.scene_understanding = false;
        capabilities_.body_tracking = false;
        capabilities_.max_views = 2;
        capabilities_.max_layer_count = 16;

        capabilities_.supported_reference_spaces = {
            ReferenceSpaceType::View,
            ReferenceSpaceType::Local,
            ReferenceSpaceType::LocalFloor,
            ReferenceSpaceType::Stage,
        };

        capabilities_.supported_swapchain_formats = {
            SurfaceFormat::Rgba8UnormSrgb,
            SurfaceFormat::Bgra8UnormSrgb,
            SurfaceFormat::Rgba16Float,
        };
    }

    [[nodiscard]] const XrRuntimeInfo& runtime_info() const override {
        return runtime_info_;
    }

    [[nodiscard]] const XrSystemCapabilities& capabilities() const override {
        return capabilities_;
    }

    [[nodiscard]] bool is_available() const override {
        return true;  // Stub is always available
    }

    [[nodiscard]] std::unique_ptr<IXrSession> create_session(
        const XrSessionConfig& config,
        IBackend* /*graphics_backend*/) override {
        return std::make_unique<StubXrSession>(config);
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> recommended_resolution() const override {
        return {1920, 1080};  // 1080p per eye for desktop preview
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> max_resolution() const override {
        return {3840, 2160};  // 4K per eye max
    }

    [[nodiscard]] std::vector<float> supported_refresh_rates() const override {
        return {60.0f, 72.0f, 90.0f, 120.0f};
    }

    bool set_refresh_rate(float /*hz*/) override {
        return true;  // Stub accepts any rate
    }

    void poll_events() override {
        // No-op for stub
    }

private:
    XrRuntimeInfo runtime_info_;
    XrSystemCapabilities capabilities_;
};

// =============================================================================
// XrSystemFactory Implementation
// =============================================================================

XrSystemAvailability XrSystemFactory::query_availability() {
    XrSystemAvailability availability;

    // Check for OpenXR
#if defined(VOID_HAS_OPENXR)
    availability.openxr_available = true;
    availability.openxr_runtime = "OpenXR";  // Would query actual runtime
#else
    availability.openxr_available = false;
    availability.openxr_runtime = "OpenXR not compiled";
#endif

    // Check for WebXR
#if defined(__EMSCRIPTEN__)
    availability.webxr_available = true;
    availability.webxr_status = "WebXR available";
#else
    availability.webxr_available = false;
    availability.webxr_status = "Not on web platform";
#endif

    return availability;
}

std::unique_ptr<IXrSystem> XrSystemFactory::create_openxr(
    const std::string& /*application_name*/,
    std::uint32_t /*application_version*/) {
#if defined(VOID_HAS_OPENXR)
    // OpenXR implementation would go here
    // For now, return stub if OpenXR fails to initialize
    return std::make_unique<StubXrSystem>();
#else
    // Fall back to stub system
    return std::make_unique<StubXrSystem>();
#endif
}

std::unique_ptr<IXrSystem> XrSystemFactory::create_webxr() {
#if defined(__EMSCRIPTEN__)
    // WebXR implementation would go here
    return nullptr;
#else
    return nullptr;  // Not available on non-web platforms
#endif
}

std::unique_ptr<IXrSystem> XrSystemFactory::create_best_available(
    const std::string& application_name) {
    auto availability = query_availability();

    // Try OpenXR first
    if (availability.openxr_available) {
        auto system = create_openxr(application_name);
        if (system && system->is_available()) {
            return system;
        }
    }

    // Try WebXR on web platforms
    if (availability.webxr_available) {
        auto system = create_webxr();
        if (system && system->is_available()) {
            return system;
        }
    }

    // Fall back to stub system for development
    return std::make_unique<StubXrSystem>();
}

} // namespace xr
} // namespace void_presenter
