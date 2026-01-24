#pragma once

/// @file xr.hpp
/// @brief Public API for void_xr Extended Reality (VR/AR) system
///
/// This header provides the public interface for XR support in void_engine.
/// It wraps the presenter XR subsystem and provides a simplified API.
///
/// Supports:
/// - OpenXR backend (native VR headsets: Oculus, Vive, Index, Quest)
/// - WebXR backend (web-based VR)
/// - Stub/Desktop backend (development without VR hardware)
///
/// Example usage:
/// @code
/// #include <void_engine/xr/xr.hpp>
///
/// void setup_vr() {
///     using namespace void_xr;
///
///     // Create XR system
///     auto system = XrSystemFactory::create_best_available("MyApp");
///     if (!system || !system->is_available()) {
///         // No VR available, fall back to desktop
///         return;
///     }
///
///     // Create session
///     XrSessionConfig config;
///     config.enable_hand_tracking = true;
///     auto session = system->create_session(config, graphics_backend);
///
///     // Begin session
///     session->begin();
///
///     // Game loop
///     while (running) {
///         XrFrame frame;
///         if (session->wait_frame(frame)) {
///             session->begin_frame();
///
///             // Render to each eye
///             auto targets = session->acquire_swapchain_images();
///             render_eye(Eye::Left, frame.views.left, targets.left);
///             render_eye(Eye::Right, frame.views.right, targets.right);
///             session->release_swapchain_images();
///
///             session->end_frame(targets);
///         }
///
///         session->poll_events();
///     }
///
///     session->end();
/// }
/// @endcode

// Re-export presenter XR types
#include <void_engine/presenter/xr/xr_types.hpp>
#include <void_engine/presenter/xr/xr_system.hpp>

namespace void_xr {

// Import types from void_presenter::xr namespace
using namespace void_presenter::xr;

// =============================================================================
// Convenience Type Aliases
// =============================================================================

/// XR system type
using System = IXrSystem;

/// XR session type
using Session = IXrSession;

/// XR frame data
using Frame = XrFrame;

/// XR view (per eye)
using View = XrView;

/// XR pose (position + orientation)
using XrPose = Pose;

/// XR controller state
using Controller = ControllerState;

/// XR hand tracking data
using HandTracking = HandTrackingData;

// =============================================================================
// Factory Functions
// =============================================================================

/// @brief Create the best available XR system
/// @param app_name Application name for runtime identification
/// @return XR system or nullptr if no XR available
[[nodiscard]] inline std::unique_ptr<IXrSystem> create_xr_system(
    const std::string& app_name) {
    return XrSystemFactory::create_best_available(app_name);
}

/// @brief Create OpenXR system specifically
/// @param app_name Application name
/// @param app_version Application version (default 1)
/// @return OpenXR system or nullptr
[[nodiscard]] inline std::unique_ptr<IXrSystem> create_openxr_system(
    const std::string& app_name,
    std::uint32_t app_version = 1) {
    return XrSystemFactory::create_openxr(app_name, app_version);
}

/// @brief Check if XR is available on this system
[[nodiscard]] inline bool is_xr_available() {
    auto avail = XrSystemFactory::query_availability();
    return avail.openxr_available || avail.webxr_available;
}

/// @brief Get XR availability details
[[nodiscard]] inline XrSystemAvailability query_xr_availability() {
    return XrSystemFactory::query_availability();
}

// =============================================================================
// Session Configuration Builders
// =============================================================================

/// @brief Create default VR session config (standing, room-scale)
[[nodiscard]] inline XrSessionConfig vr_config() {
    XrSessionConfig config;
    config.primary_reference_space = ReferenceSpaceType::LocalFloor;
    config.enable_hand_tracking = true;
    config.view_count = 2;
    return config;
}

/// @brief Create seated VR session config
[[nodiscard]] inline XrSessionConfig seated_vr_config() {
    XrSessionConfig config;
    config.primary_reference_space = ReferenceSpaceType::Local;
    config.enable_hand_tracking = true;
    config.view_count = 2;
    return config;
}

/// @brief Create room-scale VR session config
[[nodiscard]] inline XrSessionConfig roomscale_vr_config() {
    XrSessionConfig config;
    config.primary_reference_space = ReferenceSpaceType::Stage;
    config.enable_hand_tracking = true;
    config.view_count = 2;
    return config;
}

/// @brief Create AR session config (passthrough enabled)
[[nodiscard]] inline XrSessionConfig ar_config() {
    XrSessionConfig config;
    config.primary_reference_space = ReferenceSpaceType::LocalFloor;
    config.enable_hand_tracking = true;
    config.enable_passthrough = true;
    config.view_count = 2;
    return config;
}

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Get hand joint name as string
[[nodiscard]] constexpr const char* hand_joint_name(HandJoint joint) {
    switch (joint) {
        case HandJoint::Palm: return "Palm";
        case HandJoint::Wrist: return "Wrist";
        case HandJoint::ThumbMetacarpal: return "ThumbMetacarpal";
        case HandJoint::ThumbProximal: return "ThumbProximal";
        case HandJoint::ThumbDistal: return "ThumbDistal";
        case HandJoint::ThumbTip: return "ThumbTip";
        case HandJoint::IndexMetacarpal: return "IndexMetacarpal";
        case HandJoint::IndexProximal: return "IndexProximal";
        case HandJoint::IndexIntermediate: return "IndexIntermediate";
        case HandJoint::IndexDistal: return "IndexDistal";
        case HandJoint::IndexTip: return "IndexTip";
        case HandJoint::MiddleMetacarpal: return "MiddleMetacarpal";
        case HandJoint::MiddleProximal: return "MiddleProximal";
        case HandJoint::MiddleIntermediate: return "MiddleIntermediate";
        case HandJoint::MiddleDistal: return "MiddleDistal";
        case HandJoint::MiddleTip: return "MiddleTip";
        case HandJoint::RingMetacarpal: return "RingMetacarpal";
        case HandJoint::RingProximal: return "RingProximal";
        case HandJoint::RingIntermediate: return "RingIntermediate";
        case HandJoint::RingDistal: return "RingDistal";
        case HandJoint::RingTip: return "RingTip";
        case HandJoint::LittleMetacarpal: return "LittleMetacarpal";
        case HandJoint::LittleProximal: return "LittleProximal";
        case HandJoint::LittleIntermediate: return "LittleIntermediate";
        case HandJoint::LittleDistal: return "LittleDistal";
        case HandJoint::LittleTip: return "LittleTip";
        default: return "Unknown";
    }
}

/// @brief Get session state name as string
[[nodiscard]] constexpr const char* session_state_name(XrSessionState state) {
    switch (state) {
        case XrSessionState::Unknown: return "Unknown";
        case XrSessionState::Idle: return "Idle";
        case XrSessionState::Ready: return "Ready";
        case XrSessionState::Synchronized: return "Synchronized";
        case XrSessionState::Visible: return "Visible";
        case XrSessionState::Focused: return "Focused";
        case XrSessionState::Stopping: return "Stopping";
        case XrSessionState::LossPending: return "LossPending";
        case XrSessionState::Exiting: return "Exiting";
        default: return "Unknown";
    }
}

/// @brief Get reference space name as string
[[nodiscard]] constexpr const char* reference_space_name(ReferenceSpaceType type) {
    switch (type) {
        case ReferenceSpaceType::View: return "View";
        case ReferenceSpaceType::Local: return "Local";
        case ReferenceSpaceType::LocalFloor: return "LocalFloor";
        case ReferenceSpaceType::Stage: return "Stage";
        case ReferenceSpaceType::Unbounded: return "Unbounded";
        default: return "Unknown";
    }
}

/// @brief Check if session state allows rendering
[[nodiscard]] constexpr bool can_render(XrSessionState state) {
    return state == XrSessionState::Visible || state == XrSessionState::Focused;
}

/// @brief Check if session state allows input
[[nodiscard]] constexpr bool can_receive_input(XrSessionState state) {
    return state == XrSessionState::Focused;
}

} // namespace void_xr
