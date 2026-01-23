#pragma once

/// @file xr_system.hpp
/// @brief XR system and session management
///
/// Provides the XR system abstraction for VR/AR:
/// - XR runtime detection (OpenXR, WebXR)
/// - Session lifecycle management
/// - Reference space handling
/// - Feature enumeration

#include "xr_types.hpp"
#include "../backend.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace void_presenter {
namespace xr {

// =============================================================================
// XR Reference Space
// =============================================================================

/// XR reference space type
enum class ReferenceSpaceType {
    View,           ///< Head-locked (viewer-relative)
    Local,          ///< Seated experience (origin at initial position)
    LocalFloor,     ///< Standing experience (origin on floor)
    Stage,          ///< Room-scale (bounded play area)
    Unbounded,      ///< Large-scale tracking (ARCore, ARKit)
};

/// Reference space bounds (for Stage type)
struct StageBounds {
    float width = 0.0f;     ///< Stage width in meters
    float depth = 0.0f;     ///< Stage depth in meters
    std::vector<Vec3> boundary_points;  ///< Boundary polygon
};

// =============================================================================
// XR System Info
// =============================================================================

/// XR runtime information
struct XrRuntimeInfo {
    std::string name;               ///< Runtime name (e.g., "Oculus", "SteamVR")
    std::string version;            ///< Runtime version
    XrSystemType system_type = XrSystemType::None;
    std::uint64_t system_id = 0;
};

/// XR system capabilities
struct XrSystemCapabilities {
    bool hand_tracking = false;
    bool eye_tracking = false;
    bool foveated_rendering = false;
    bool passthrough = false;
    bool spatial_anchors = false;
    bool scene_understanding = false;
    bool body_tracking = false;

    std::uint32_t max_views = 2;
    std::uint32_t max_layer_count = 16;

    std::vector<ReferenceSpaceType> supported_reference_spaces;
    std::vector<SurfaceFormat> supported_swapchain_formats;

    /// Check if reference space is supported
    [[nodiscard]] bool supports_reference_space(ReferenceSpaceType type) const {
        for (const auto& t : supported_reference_spaces) {
            if (t == type) return true;
        }
        return false;
    }
};

// =============================================================================
// XR Session Configuration
// =============================================================================

/// XR session configuration
struct XrSessionConfig {
    ReferenceSpaceType primary_reference_space = ReferenceSpaceType::LocalFloor;
    bool enable_hand_tracking = true;
    bool enable_eye_tracking = false;
    bool enable_passthrough = false;
    FoveatedRenderingConfig foveation;

    /// View configuration (stereo, mono, etc.)
    std::uint32_t view_count = 2;       // Stereo by default

    /// Swapchain configuration
    SurfaceFormat color_format = SurfaceFormat::Rgba8UnormSrgb;
    SurfaceFormat depth_format = SurfaceFormat::Bgra8Unorm;  // Placeholder for depth
    std::uint32_t sample_count = 1;

    [[nodiscard]] XrSessionConfig with_hand_tracking(bool enable) const {
        XrSessionConfig copy = *this;
        copy.enable_hand_tracking = enable;
        return copy;
    }

    [[nodiscard]] XrSessionConfig with_passthrough(bool enable) const {
        XrSessionConfig copy = *this;
        copy.enable_passthrough = enable;
        return copy;
    }
};

// =============================================================================
// XR Event
// =============================================================================

/// XR event type
enum class XrEventType {
    SessionStateChanged,
    ReferenceSpaceChanged,
    InteractionProfileChanged,
    VisibilityMaskChanged,
    DeviceLost,
};

/// XR event data
struct XrEvent {
    XrEventType type;
    XrSessionState new_session_state = XrSessionState::Unknown;
    ReferenceSpaceType reference_space = ReferenceSpaceType::Local;
    std::string message;
};

/// XR event callback
using XrEventCallback = std::function<void(const XrEvent&)>;

// =============================================================================
// XR Session Interface
// =============================================================================

/// XR session interface
class IXrSession {
public:
    virtual ~IXrSession() = default;

    /// Get session state
    [[nodiscard]] virtual XrSessionState state() const = 0;

    /// Get session configuration
    [[nodiscard]] virtual const XrSessionConfig& config() const = 0;

    /// Begin session
    /// @return true on success
    virtual bool begin() = 0;

    /// End session
    virtual void end() = 0;

    /// Request exit
    virtual void request_exit() = 0;

    /// Wait for and begin frame
    /// @param out_frame Output XR frame data
    /// @return true if should render
    virtual bool wait_frame(XrFrame& out_frame) = 0;

    /// Begin frame rendering
    virtual void begin_frame() = 0;

    /// End frame and submit for display
    /// @param views Rendered views
    virtual void end_frame(const XrStereoTargets& views) = 0;

    /// Get render targets for current frame
    [[nodiscard]] virtual XrStereoTargets acquire_swapchain_images() = 0;

    /// Release swapchain images
    virtual void release_swapchain_images() = 0;

    /// Get current views (call after wait_frame)
    [[nodiscard]] virtual StereoViews get_views() const = 0;

    /// Get head pose
    [[nodiscard]] virtual TrackedPose get_head_pose() const = 0;

    /// Get controller state
    [[nodiscard]] virtual std::optional<ControllerState> get_controller(Hand hand) const = 0;

    /// Get hand tracking data
    [[nodiscard]] virtual std::optional<HandTrackingData> get_hand_tracking(Hand hand) const = 0;

    /// Get stage bounds (if available)
    [[nodiscard]] virtual std::optional<StageBounds> get_stage_bounds() const = 0;

    /// Set foveation config
    virtual void set_foveation(const FoveatedRenderingConfig& config) = 0;

    /// Trigger haptic feedback
    virtual void trigger_haptic(Hand hand, float amplitude, float duration_seconds) = 0;

    /// Poll and dispatch events
    virtual void poll_events() = 0;

    /// Set event callback
    virtual void set_event_callback(XrEventCallback callback) = 0;
};

// =============================================================================
// XR System Interface
// =============================================================================

/// XR system interface - represents the XR runtime
class IXrSystem {
public:
    virtual ~IXrSystem() = default;

    /// Get runtime info
    [[nodiscard]] virtual const XrRuntimeInfo& runtime_info() const = 0;

    /// Get system capabilities
    [[nodiscard]] virtual const XrSystemCapabilities& capabilities() const = 0;

    /// Check if XR is available and ready
    [[nodiscard]] virtual bool is_available() const = 0;

    /// Create a session
    /// @param config Session configuration
    /// @param graphics_backend Graphics backend for rendering
    /// @return Created session or nullptr on failure
    [[nodiscard]] virtual std::unique_ptr<IXrSession> create_session(
        const XrSessionConfig& config,
        IBackend* graphics_backend) = 0;

    /// Get recommended render resolution per eye
    [[nodiscard]] virtual std::pair<std::uint32_t, std::uint32_t> recommended_resolution() const = 0;

    /// Get maximum render resolution per eye
    [[nodiscard]] virtual std::pair<std::uint32_t, std::uint32_t> max_resolution() const = 0;

    /// Get supported refresh rates
    [[nodiscard]] virtual std::vector<float> supported_refresh_rates() const = 0;

    /// Set display refresh rate
    /// @return true on success
    virtual bool set_refresh_rate(float hz) = 0;

    /// Poll system events (call regularly)
    virtual void poll_events() = 0;
};

// =============================================================================
// XR System Factory
// =============================================================================

/// XR system availability
struct XrSystemAvailability {
    bool openxr_available = false;
    bool webxr_available = false;
    std::string openxr_runtime;
    std::string webxr_status;
};

/// Factory for creating XR systems
class XrSystemFactory {
public:
    /// Query XR system availability
    [[nodiscard]] static XrSystemAvailability query_availability();

    /// Create OpenXR system (native platforms)
    /// @param application_name Application name for OpenXR
    /// @param application_version Application version
    [[nodiscard]] static std::unique_ptr<IXrSystem> create_openxr(
        const std::string& application_name,
        std::uint32_t application_version = 1);

    /// Create WebXR system (web platform)
    [[nodiscard]] static std::unique_ptr<IXrSystem> create_webxr();

    /// Create best available XR system
    [[nodiscard]] static std::unique_ptr<IXrSystem> create_best_available(
        const std::string& application_name);
};

} // namespace xr
} // namespace void_presenter
