#pragma once

/// @file xr_types.hpp
/// @brief XR (VR/AR) types for void_presenter
///
/// Core types for VR/XR rendering including:
/// - Views and projections (stereo rendering)
/// - Poses and tracking
/// - XR session management
/// - Hand tracking
/// - Foveated rendering

#include "../fwd.hpp"
#include "../types.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace void_presenter {
namespace xr {

// =============================================================================
// XR System Type
// =============================================================================

/// XR system type
enum class XrSystemType {
    None,           ///< No XR
    HeadMountedVR,  ///< VR headset (Oculus, Vive, Index, Quest)
    HeadMountedAR,  ///< AR headset (HoloLens, Magic Leap)
    HandheldAR,     ///< Phone/tablet AR (ARKit, ARCore)
    Inline,         ///< Inline XR (non-immersive)
};

/// XR session state
enum class XrSessionState {
    Unknown,
    Idle,           ///< Session created, not running
    Ready,          ///< Ready to begin
    Synchronized,   ///< Visible but not focused
    Visible,        ///< Visible and has focus
    Focused,        ///< Visible, focused, receiving input
    Stopping,       ///< Session stopping
    LossPending,    ///< About to lose session
    Exiting,        ///< Session ending
};

// =============================================================================
// Eye / View
// =============================================================================

/// Eye identifier
enum class Eye : std::uint8_t {
    Left = 0,
    Right = 1,
};

/// Field of view (angles in radians)
struct Fov {
    float angle_left = -0.785f;     ///< Left angle (negative)
    float angle_right = 0.785f;     ///< Right angle (positive)
    float angle_up = 0.785f;        ///< Up angle (positive)
    float angle_down = -0.785f;     ///< Down angle (negative)

    /// Check if FOV is valid
    [[nodiscard]] bool is_valid() const {
        return angle_left < angle_right && angle_down < angle_up;
    }

    /// Get horizontal FOV in radians
    [[nodiscard]] float horizontal_fov() const {
        return angle_right - angle_left;
    }

    /// Get vertical FOV in radians
    [[nodiscard]] float vertical_fov() const {
        return angle_up - angle_down;
    }

    /// Get aspect ratio
    [[nodiscard]] float aspect_ratio() const {
        return horizontal_fov() / vertical_fov();
    }

    /// Create symmetric FOV from total angles
    [[nodiscard]] static Fov symmetric(float horizontal_rad, float vertical_rad) {
        float h = horizontal_rad * 0.5f;
        float v = vertical_rad * 0.5f;
        return {-h, h, v, -v};
    }

    /// Create from degrees
    [[nodiscard]] static Fov from_degrees(float left, float right, float up, float down) {
        constexpr float deg2rad = 3.14159265358979f / 180.0f;
        return {left * deg2rad, right * deg2rad, up * deg2rad, down * deg2rad};
    }
};

// =============================================================================
// Pose (Position + Orientation)
// =============================================================================

/// 3D vector
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] Vec3 operator+(const Vec3& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    [[nodiscard]] Vec3 operator-(const Vec3& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    [[nodiscard]] Vec3 operator*(float s) const {
        return {x * s, y * s, z * s};
    }

    [[nodiscard]] float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    [[nodiscard]] Vec3 normalized() const {
        float len = length();
        if (len > 0.0001f) {
            return *this * (1.0f / len);
        }
        return *this;
    }
};

/// Quaternion (orientation)
struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    /// Identity quaternion
    [[nodiscard]] static Quat identity() {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }

    /// Rotate a vector
    [[nodiscard]] Vec3 rotate(const Vec3& v) const {
        // q * v * q^-1
        float qx = x, qy = y, qz = z, qw = w;

        float ix = qw * v.x + qy * v.z - qz * v.y;
        float iy = qw * v.y + qz * v.x - qx * v.z;
        float iz = qw * v.z + qx * v.y - qy * v.x;
        float iw = -qx * v.x - qy * v.y - qz * v.z;

        return {
            ix * qw + iw * -qx + iy * -qz - iz * -qy,
            iy * qw + iw * -qy + iz * -qx - ix * -qz,
            iz * qw + iw * -qz + ix * -qy - iy * -qx
        };
    }

    /// Get forward vector
    [[nodiscard]] Vec3 forward() const {
        return rotate({0.0f, 0.0f, -1.0f});
    }

    /// Get up vector
    [[nodiscard]] Vec3 up() const {
        return rotate({0.0f, 1.0f, 0.0f});
    }

    /// Get right vector
    [[nodiscard]] Vec3 right() const {
        return rotate({1.0f, 0.0f, 0.0f});
    }

    /// Multiply quaternions
    [[nodiscard]] Quat operator*(const Quat& other) const {
        return {
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        };
    }

    /// Conjugate (inverse for unit quaternion)
    [[nodiscard]] Quat conjugate() const {
        return {-x, -y, -z, w};
    }
};

/// Pose (position + orientation)
struct Pose {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat orientation = Quat::identity();

    /// Identity pose
    [[nodiscard]] static Pose identity() {
        return {};
    }

    /// Transform a point
    [[nodiscard]] Vec3 transform_point(const Vec3& point) const {
        return position + orientation.rotate(point);
    }

    /// Transform a direction (no translation)
    [[nodiscard]] Vec3 transform_direction(const Vec3& dir) const {
        return orientation.rotate(dir);
    }

    /// Get inverse pose
    [[nodiscard]] Pose inverse() const {
        Quat inv_orient = orientation.conjugate();
        return {
            inv_orient.rotate(position * -1.0f),
            inv_orient
        };
    }

    /// Combine poses (this * other)
    [[nodiscard]] Pose operator*(const Pose& other) const {
        return {
            transform_point(other.position),
            orientation * other.orientation
        };
    }
};

/// Pose velocity
struct PoseVelocity {
    Vec3 linear{0.0f, 0.0f, 0.0f};   ///< Linear velocity (m/s)
    Vec3 angular{0.0f, 0.0f, 0.0f};  ///< Angular velocity (rad/s)
};

/// Tracked pose with validity
struct TrackedPose {
    Pose pose;
    PoseVelocity velocity;
    bool position_valid = false;
    bool orientation_valid = false;
    bool velocity_valid = false;

    [[nodiscard]] bool is_valid() const {
        return position_valid && orientation_valid;
    }
};

// =============================================================================
// XR View
// =============================================================================

/// XR view (one eye's perspective)
struct XrView {
    Eye eye = Eye::Left;
    Pose pose;              ///< View pose (eye position/orientation)
    Fov fov;                ///< Field of view
    std::uint32_t width = 0;    ///< Recommended render width
    std::uint32_t height = 0;   ///< Recommended render height

    /// Get view matrix (4x4 column-major)
    [[nodiscard]] std::array<float, 16> view_matrix() const {
        // Inverse of pose transform
        Pose inv = pose.inverse();
        Quat& q = inv.orientation;
        Vec3& p = inv.position;

        // Convert quaternion to rotation matrix
        float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

        return {
            1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),       2.0f * (xz - wy),       0.0f,
            2.0f * (xy - wz),       1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),       0.0f,
            2.0f * (xz + wy),       2.0f * (yz - wx),       1.0f - 2.0f * (xx + yy), 0.0f,
            p.x,                    p.y,                    p.z,                    1.0f
        };
    }

    /// Get projection matrix (4x4 column-major, reverse-Z)
    [[nodiscard]] std::array<float, 16> projection_matrix(float near_z = 0.01f, float far_z = 1000.0f) const {
        float tan_left = std::tan(fov.angle_left);
        float tan_right = std::tan(fov.angle_right);
        float tan_up = std::tan(fov.angle_up);
        float tan_down = std::tan(fov.angle_down);

        float tan_width = tan_right - tan_left;
        float tan_height = tan_up - tan_down;

        // Reverse-Z for better depth precision
        return {
            2.0f / tan_width, 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f / tan_height, 0.0f, 0.0f,
            (tan_right + tan_left) / tan_width, (tan_up + tan_down) / tan_height, far_z / (near_z - far_z), -1.0f,
            0.0f, 0.0f, (near_z * far_z) / (near_z - far_z), 0.0f
        };
    }
};

/// Stereo views (both eyes)
struct StereoViews {
    XrView left;
    XrView right;

    /// Get view for eye
    [[nodiscard]] const XrView& view(Eye eye) const {
        return eye == Eye::Left ? left : right;
    }

    [[nodiscard]] XrView& view(Eye eye) {
        return eye == Eye::Left ? left : right;
    }

    /// Get IPD (interpupillary distance)
    [[nodiscard]] float ipd() const {
        return (left.pose.position - right.pose.position).length();
    }
};

// =============================================================================
// Hand Tracking
// =============================================================================

/// Hand identifier
enum class Hand : std::uint8_t {
    Left = 0,
    Right = 1,
};

/// Hand joint (OpenXR hand tracking extension)
enum class HandJoint : std::uint8_t {
    Palm = 0,
    Wrist = 1,
    ThumbMetacarpal = 2,
    ThumbProximal = 3,
    ThumbDistal = 4,
    ThumbTip = 5,
    IndexMetacarpal = 6,
    IndexProximal = 7,
    IndexIntermediate = 8,
    IndexDistal = 9,
    IndexTip = 10,
    MiddleMetacarpal = 11,
    MiddleProximal = 12,
    MiddleIntermediate = 13,
    MiddleDistal = 14,
    MiddleTip = 15,
    RingMetacarpal = 16,
    RingProximal = 17,
    RingIntermediate = 18,
    RingDistal = 19,
    RingTip = 20,
    LittleMetacarpal = 21,
    LittleProximal = 22,
    LittleIntermediate = 23,
    LittleDistal = 24,
    LittleTip = 25,
    Count = 26,
};

/// Hand joint pose
struct HandJointPose {
    Pose pose;
    float radius = 0.0f;    ///< Joint radius (for collision)
    bool valid = false;
};

/// Hand tracking data
struct HandTrackingData {
    Hand hand = Hand::Left;
    std::array<HandJointPose, static_cast<std::size_t>(HandJoint::Count)> joints;
    bool active = false;

    /// Get joint pose
    [[nodiscard]] const HandJointPose& joint(HandJoint j) const {
        return joints[static_cast<std::size_t>(j)];
    }

    /// Check if any joints are tracked
    [[nodiscard]] bool has_tracking() const {
        for (const auto& j : joints) {
            if (j.valid) return true;
        }
        return false;
    }

    /// Get pinch strength (index-thumb distance)
    [[nodiscard]] float pinch_strength() const {
        const auto& thumb = joints[static_cast<std::size_t>(HandJoint::ThumbTip)];
        const auto& index = joints[static_cast<std::size_t>(HandJoint::IndexTip)];

        if (!thumb.valid || !index.valid) return 0.0f;

        float dist = (thumb.pose.position - index.pose.position).length();
        // 0.0 = touching, 1.0 = max spread (~10cm)
        return 1.0f - std::min(dist / 0.1f, 1.0f);
    }
};

// =============================================================================
// Controller
// =============================================================================

/// Controller button
enum class ControllerButton : std::uint32_t {
    None = 0,
    Trigger = 1 << 0,
    Grip = 1 << 1,
    Menu = 1 << 2,
    System = 1 << 3,
    PrimaryButton = 1 << 4,     // A/X
    SecondaryButton = 1 << 5,   // B/Y
    ThumbstickClick = 1 << 6,
    ThumbstickTouch = 1 << 7,
    TrackpadClick = 1 << 8,
    TrackpadTouch = 1 << 9,
};

inline ControllerButton operator|(ControllerButton a, ControllerButton b) {
    return static_cast<ControllerButton>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline ControllerButton operator&(ControllerButton a, ControllerButton b) {
    return static_cast<ControllerButton>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

/// Controller state
struct ControllerState {
    Hand hand = Hand::Left;
    TrackedPose pose;

    ControllerButton buttons_pressed = ControllerButton::None;
    ControllerButton buttons_touched = ControllerButton::None;

    float trigger = 0.0f;           ///< Trigger value (0-1)
    float grip = 0.0f;              ///< Grip value (0-1)
    Vec3 thumbstick{0, 0, 0};       ///< x, y, click
    Vec3 trackpad{0, 0, 0};         ///< x, y, click

    bool active = false;

    /// Check if button is pressed
    [[nodiscard]] bool is_pressed(ControllerButton btn) const {
        return (buttons_pressed & btn) != ControllerButton::None;
    }

    /// Check if button is touched
    [[nodiscard]] bool is_touched(ControllerButton btn) const {
        return (buttons_touched & btn) != ControllerButton::None;
    }
};

// =============================================================================
// XR Frame
// =============================================================================

/// XR frame timing
struct XrFrameTiming {
    std::int64_t predicted_display_time = 0;    ///< When frame will display (ns)
    std::int64_t predicted_display_period = 0;  ///< Display period (ns)
    std::chrono::steady_clock::time_point frame_begin;
    std::chrono::steady_clock::time_point frame_end;

    /// Get predicted display time as duration from now
    [[nodiscard]] std::chrono::nanoseconds time_until_display() const {
        return std::chrono::nanoseconds(predicted_display_time);
    }
};

/// XR frame data
struct XrFrame {
    std::uint64_t frame_number = 0;
    XrFrameTiming timing;
    StereoViews views;
    TrackedPose head_pose;

    std::optional<ControllerState> left_controller;
    std::optional<ControllerState> right_controller;
    std::optional<HandTrackingData> left_hand;
    std::optional<HandTrackingData> right_hand;

    bool should_render = true;          ///< False if session not visible
    bool session_active = true;

    /// Get controller for hand
    [[nodiscard]] const ControllerState* controller(Hand hand) const {
        const auto& opt = (hand == Hand::Left) ? left_controller : right_controller;
        return opt.has_value() ? &*opt : nullptr;
    }

    /// Get hand tracking for hand
    [[nodiscard]] const HandTrackingData* hand_tracking(Hand hand) const {
        const auto& opt = (hand == Hand::Left) ? left_hand : right_hand;
        return opt.has_value() ? &*opt : nullptr;
    }
};

// =============================================================================
// Render Target
// =============================================================================

/// XR eye render target
struct XrRenderTarget {
    void* color_texture = nullptr;      ///< Color texture handle
    void* depth_texture = nullptr;      ///< Depth texture handle (optional)
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t array_index = 0;      ///< For texture arrays
    SurfaceFormat format = SurfaceFormat::Rgba8UnormSrgb;
};

/// XR render targets for both eyes
struct XrStereoTargets {
    XrRenderTarget left;
    XrRenderTarget right;
    bool is_array_texture = false;      ///< True if using single array texture
};

// =============================================================================
// Foveated Rendering
// =============================================================================

/// Foveated rendering level
enum class FoveationLevel {
    None,       ///< No foveation
    Low,        ///< Light foveation
    Medium,     ///< Medium foveation
    High,       ///< Aggressive foveation
};

/// Foveated rendering configuration
struct FoveatedRenderingConfig {
    FoveationLevel level = FoveationLevel::None;
    bool dynamic = false;       ///< Use eye tracking for foveation center
    float inner_radius = 0.3f;  ///< Inner high-quality region radius
    float middle_radius = 0.6f; ///< Middle region radius
};

} // namespace xr
} // namespace void_presenter
