/// @file animation.cpp
/// @brief Skeletal animation and morph target system for void_render

#include "void_engine/render/mesh.hpp"

#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <optional>
#include <functional>

namespace void_render {

// =============================================================================
// Math Utilities for Animation
// =============================================================================

/// Quaternion structure
struct Quat {
    float x = 0, y = 0, z = 0, w = 1;

    Quat() = default;
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    /// Identity quaternion
    [[nodiscard]] static Quat identity() { return Quat(0, 0, 0, 1); }

    /// Quaternion multiplication
    [[nodiscard]] Quat operator*(const Quat& q) const {
        return Quat(
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        );
    }

    /// Normalize quaternion
    [[nodiscard]] Quat normalized() const {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        if (len < 1e-8f) return identity();
        return Quat(x / len, y / len, z / len, w / len);
    }

    /// Conjugate (inverse for unit quaternions)
    [[nodiscard]] Quat conjugate() const {
        return Quat(-x, -y, -z, w);
    }

    /// Spherical linear interpolation
    [[nodiscard]] static Quat slerp(const Quat& a, const Quat& b, float t) {
        float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        Quat b2 = b;
        if (dot < 0.0f) {
            b2 = Quat(-b.x, -b.y, -b.z, -b.w);
            dot = -dot;
        }

        if (dot > 0.9995f) {
            // Linear interpolation for very close quaternions
            return Quat(
                a.x + t * (b2.x - a.x),
                a.y + t * (b2.y - a.y),
                a.z + t * (b2.z - a.z),
                a.w + t * (b2.w - a.w)
            ).normalized();
        }

        float theta0 = std::acos(dot);
        float theta = theta0 * t;
        float sin_theta = std::sin(theta);
        float sin_theta0 = std::sin(theta0);

        float s0 = std::cos(theta) - dot * sin_theta / sin_theta0;
        float s1 = sin_theta / sin_theta0;

        return Quat(
            s0 * a.x + s1 * b2.x,
            s0 * a.y + s1 * b2.y,
            s0 * a.z + s1 * b2.z,
            s0 * a.w + s1 * b2.w
        );
    }

    /// Convert to 4x4 rotation matrix (column-major)
    [[nodiscard]] std::array<float, 16> to_matrix() const {
        float x2 = x + x, y2 = y + y, z2 = z + z;
        float xx = x * x2, xy = x * y2, xz = x * z2;
        float yy = y * y2, yz = y * z2, zz = z * z2;
        float wx = w * x2, wy = w * y2, wz = w * z2;

        return {{
            1 - (yy + zz), xy + wz,       xz - wy,       0,
            xy - wz,       1 - (xx + zz), yz + wx,       0,
            xz + wy,       yz - wx,       1 - (xx + yy), 0,
            0,             0,             0,             1
        }};
    }
};

/// Vec3 structure
struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    [[nodiscard]] Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    [[nodiscard]] Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    [[nodiscard]] Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }

    [[nodiscard]] static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return Vec3(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z)
        );
    }
};

/// 4x4 Matrix utilities
struct Mat4 {
    std::array<float, 16> m = {{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    }};

    [[nodiscard]] static Mat4 identity() { return Mat4{}; }

    [[nodiscard]] static Mat4 from_trs(const Vec3& t, const Quat& r, const Vec3& s) {
        Mat4 result;
        auto rot = r.to_matrix();

        // Apply scale to rotation columns
        result.m[0] = rot[0] * s.x;
        result.m[1] = rot[1] * s.x;
        result.m[2] = rot[2] * s.x;
        result.m[3] = 0;

        result.m[4] = rot[4] * s.y;
        result.m[5] = rot[5] * s.y;
        result.m[6] = rot[6] * s.y;
        result.m[7] = 0;

        result.m[8] = rot[8] * s.z;
        result.m[9] = rot[9] * s.z;
        result.m[10] = rot[10] * s.z;
        result.m[11] = 0;

        result.m[12] = t.x;
        result.m[13] = t.y;
        result.m[14] = t.z;
        result.m[15] = 1;

        return result;
    }

    [[nodiscard]] Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                result.m[col * 4 + row] =
                    m[0 * 4 + row] * other.m[col * 4 + 0] +
                    m[1 * 4 + row] * other.m[col * 4 + 1] +
                    m[2 * 4 + row] * other.m[col * 4 + 2] +
                    m[3 * 4 + row] * other.m[col * 4 + 3];
            }
        }
        return result;
    }

    [[nodiscard]] Mat4 inverse() const {
        Mat4 inv;
        float det;

        inv.m[0] = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - m[9]  * m[6]  * m[15] +
                   m[9]  * m[7]  * m[14] + m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];
        inv.m[4] = -m[4]  * m[10] * m[15] + m[4]  * m[11] * m[14] + m[8]  * m[6]  * m[15] -
                   m[8]  * m[7]  * m[14] - m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];
        inv.m[8] = m[4]  * m[9] * m[15] - m[4]  * m[11] * m[13] - m[8]  * m[5] * m[15] +
                   m[8]  * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv.m[12] = -m[4]  * m[9] * m[14] + m[4]  * m[10] * m[13] + m[8]  * m[5] * m[14] -
                    m[8]  * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

        inv.m[1] = -m[1]  * m[10] * m[15] + m[1]  * m[11] * m[14] + m[9]  * m[2] * m[15] -
                   m[9]  * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv.m[5] = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - m[8]  * m[2] * m[15] +
                   m[8]  * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv.m[9] = -m[0]  * m[9] * m[15] + m[0]  * m[11] * m[13] + m[8]  * m[1] * m[15] -
                   m[8]  * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv.m[13] = m[0]  * m[9] * m[14] - m[0]  * m[10] * m[13] - m[8]  * m[1] * m[14] +
                    m[8]  * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

        inv.m[2] = m[1]  * m[6] * m[15] - m[1]  * m[7] * m[14] - m[5]  * m[2] * m[15] +
                   m[5]  * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv.m[6] = -m[0]  * m[6] * m[15] + m[0]  * m[7] * m[14] + m[4]  * m[2] * m[15] -
                   m[4]  * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv.m[10] = m[0]  * m[5] * m[15] - m[0]  * m[7] * m[13] - m[4]  * m[1] * m[15] +
                    m[4]  * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv.m[14] = -m[0]  * m[5] * m[14] + m[0]  * m[6] * m[13] + m[4]  * m[1] * m[14] -
                    m[4]  * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

        inv.m[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
                   m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv.m[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
                   m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv.m[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
                    m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv.m[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
                    m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        det = m[0] * inv.m[0] + m[1] * inv.m[4] + m[2] * inv.m[8] + m[3] * inv.m[12];
        if (std::abs(det) < 1e-10f) return identity();

        det = 1.0f / det;
        for (int i = 0; i < 16; i++) inv.m[i] *= det;

        return inv;
    }
};

// =============================================================================
// Joint / Bone
// =============================================================================

/// Joint transform (local space)
struct JointTransform {
    Vec3 translation = {0, 0, 0};
    Quat rotation = Quat::identity();
    Vec3 scale = {1, 1, 1};

    /// Interpolate between two transforms
    [[nodiscard]] static JointTransform lerp(const JointTransform& a, const JointTransform& b, float t) {
        JointTransform result;
        result.translation = Vec3::lerp(a.translation, b.translation, t);
        result.rotation = Quat::slerp(a.rotation, b.rotation, t);
        result.scale = Vec3::lerp(a.scale, b.scale, t);
        return result;
    }

    /// Convert to matrix
    [[nodiscard]] Mat4 to_matrix() const {
        return Mat4::from_trs(translation, rotation, scale);
    }
};

/// Joint definition
struct Joint {
    std::string name;
    std::int32_t parent = -1;  // -1 = root joint
    JointTransform bind_pose;  // Local bind pose
    Mat4 inverse_bind_matrix;  // Inverse bind matrix (model space)

    // Children (for hierarchy traversal)
    std::vector<std::int32_t> children;
};

// =============================================================================
// Skeleton
// =============================================================================

/// Maximum joints per skeleton (GPU uniform limit)
static constexpr std::size_t MAX_JOINTS = 256;

/// Skeleton definition
class Skeleton {
public:
    /// Add a joint
    std::int32_t add_joint(const Joint& joint) {
        if (m_joints.size() >= MAX_JOINTS) return -1;

        std::int32_t index = static_cast<std::int32_t>(m_joints.size());
        m_joints.push_back(joint);
        m_joint_names[joint.name] = index;

        // Add to parent's children
        if (joint.parent >= 0 && joint.parent < static_cast<std::int32_t>(m_joints.size())) {
            m_joints[joint.parent].children.push_back(index);
        } else {
            m_root_joints.push_back(index);
        }

        return index;
    }

    /// Get joint by index
    [[nodiscard]] const Joint* get_joint(std::int32_t index) const {
        if (index < 0 || index >= static_cast<std::int32_t>(m_joints.size())) return nullptr;
        return &m_joints[index];
    }

    /// Get joint by name
    [[nodiscard]] std::int32_t find_joint(const std::string& name) const {
        auto it = m_joint_names.find(name);
        return it != m_joint_names.end() ? it->second : -1;
    }

    /// Get joint count
    [[nodiscard]] std::size_t joint_count() const noexcept { return m_joints.size(); }

    /// Get all joints
    [[nodiscard]] const std::vector<Joint>& joints() const noexcept { return m_joints; }

    /// Get root joints
    [[nodiscard]] const std::vector<std::int32_t>& root_joints() const noexcept { return m_root_joints; }

    /// Compute world-space joint matrices from local poses
    void compute_world_matrices(
        const std::vector<JointTransform>& local_poses,
        std::vector<Mat4>& world_matrices) const {

        world_matrices.resize(m_joints.size());

        // Process joints in hierarchy order (parents before children)
        for (std::int32_t root : m_root_joints) {
            compute_joint_recursive(root, Mat4::identity(), local_poses, world_matrices);
        }
    }

    /// Compute final skinning matrices (world * inverse bind)
    void compute_skinning_matrices(
        const std::vector<Mat4>& world_matrices,
        std::vector<Mat4>& skinning_matrices) const {

        skinning_matrices.resize(m_joints.size());

        for (std::size_t i = 0; i < m_joints.size(); ++i) {
            skinning_matrices[i] = world_matrices[i] * m_joints[i].inverse_bind_matrix;
        }
    }

private:
    void compute_joint_recursive(
        std::int32_t joint_index,
        const Mat4& parent_world,
        const std::vector<JointTransform>& local_poses,
        std::vector<Mat4>& world_matrices) const {

        const auto& joint = m_joints[joint_index];
        Mat4 local = joint_index < static_cast<std::int32_t>(local_poses.size())
            ? local_poses[joint_index].to_matrix()
            : joint.bind_pose.to_matrix();

        world_matrices[joint_index] = parent_world * local;

        for (std::int32_t child : joint.children) {
            compute_joint_recursive(child, world_matrices[joint_index], local_poses, world_matrices);
        }
    }

    std::vector<Joint> m_joints;
    std::unordered_map<std::string, std::int32_t> m_joint_names;
    std::vector<std::int32_t> m_root_joints;
};

// =============================================================================
// Animation Keyframe
// =============================================================================

/// Interpolation mode
enum class Interpolation : std::uint8_t {
    Step = 0,      // No interpolation
    Linear,        // Linear interpolation
    CubicSpline    // Cubic spline (requires tangents)
};

/// Animation channel target
enum class AnimationTarget : std::uint8_t {
    Translation = 0,
    Rotation,
    Scale,
    Weights  // Morph target weights
};

/// Keyframe with tangents for cubic spline
template<typename T>
struct Keyframe {
    float time = 0;
    T value;
    T in_tangent;   // For cubic spline
    T out_tangent;  // For cubic spline
};

// =============================================================================
// Animation Channel
// =============================================================================

/// Animation channel (animates one property of one joint)
class AnimationChannel {
public:
    AnimationChannel(std::int32_t target_joint, AnimationTarget target_prop, Interpolation interp)
        : m_target_joint(target_joint)
        , m_target_property(target_prop)
        , m_interpolation(interp) {}

    /// Add translation keyframe
    void add_translation_key(float time, const Vec3& value, const Vec3& in_tan = {}, const Vec3& out_tan = {}) {
        m_translation_keys.push_back({time, value, in_tan, out_tan});
        update_duration(time);
    }

    /// Add rotation keyframe
    void add_rotation_key(float time, const Quat& value, const Quat& in_tan = {}, const Quat& out_tan = {}) {
        m_rotation_keys.push_back({time, value, in_tan, out_tan});
        update_duration(time);
    }

    /// Add scale keyframe
    void add_scale_key(float time, const Vec3& value, const Vec3& in_tan = {}, const Vec3& out_tan = {}) {
        m_scale_keys.push_back({time, value, in_tan, out_tan});
        update_duration(time);
    }

    /// Add morph weight keyframe
    void add_weight_key(float time, float value, float in_tan = 0, float out_tan = 0) {
        m_weight_keys.push_back({time, value, in_tan, out_tan});
        update_duration(time);
    }

    /// Sample translation at time
    [[nodiscard]] Vec3 sample_translation(float time) const {
        return sample_vec3(m_translation_keys, time);
    }

    /// Sample rotation at time
    [[nodiscard]] Quat sample_rotation(float time) const {
        return sample_quat(m_rotation_keys, time);
    }

    /// Sample scale at time
    [[nodiscard]] Vec3 sample_scale(float time) const {
        return sample_vec3(m_scale_keys, time);
    }

    /// Sample morph weight at time
    [[nodiscard]] float sample_weight(float time) const {
        return sample_scalar(m_weight_keys, time);
    }

    [[nodiscard]] std::int32_t target_joint() const noexcept { return m_target_joint; }
    [[nodiscard]] AnimationTarget target_property() const noexcept { return m_target_property; }
    [[nodiscard]] float duration() const noexcept { return m_duration; }

private:
    std::int32_t m_target_joint;
    AnimationTarget m_target_property;
    Interpolation m_interpolation;
    float m_duration = 0;

    std::vector<Keyframe<Vec3>> m_translation_keys;
    std::vector<Keyframe<Quat>> m_rotation_keys;
    std::vector<Keyframe<Vec3>> m_scale_keys;
    std::vector<Keyframe<float>> m_weight_keys;

    void update_duration(float time) {
        m_duration = std::max(m_duration, time);
    }

    template<typename T>
    [[nodiscard]] std::pair<std::size_t, std::size_t> find_keys(
        const std::vector<Keyframe<T>>& keys, float time) const {

        if (keys.empty()) return {0, 0};
        if (time <= keys.front().time) return {0, 0};
        if (time >= keys.back().time) return {keys.size() - 1, keys.size() - 1};

        for (std::size_t i = 0; i < keys.size() - 1; ++i) {
            if (time >= keys[i].time && time < keys[i + 1].time) {
                return {i, i + 1};
            }
        }
        return {keys.size() - 1, keys.size() - 1};
    }

    [[nodiscard]] Vec3 sample_vec3(const std::vector<Keyframe<Vec3>>& keys, float time) const {
        if (keys.empty()) return Vec3();

        auto [i0, i1] = find_keys(keys, time);
        if (i0 == i1) return keys[i0].value;

        float t = (time - keys[i0].time) / (keys[i1].time - keys[i0].time);

        if (m_interpolation == Interpolation::Step) {
            return keys[i0].value;
        } else if (m_interpolation == Interpolation::CubicSpline) {
            return cubic_spline_vec3(keys[i0], keys[i1], t, keys[i1].time - keys[i0].time);
        }
        return Vec3::lerp(keys[i0].value, keys[i1].value, t);
    }

    [[nodiscard]] Quat sample_quat(const std::vector<Keyframe<Quat>>& keys, float time) const {
        if (keys.empty()) return Quat::identity();

        auto [i0, i1] = find_keys(keys, time);
        if (i0 == i1) return keys[i0].value;

        float t = (time - keys[i0].time) / (keys[i1].time - keys[i0].time);

        if (m_interpolation == Interpolation::Step) {
            return keys[i0].value;
        }
        // Cubic spline for quaternions is complex; use slerp
        return Quat::slerp(keys[i0].value, keys[i1].value, t);
    }

    [[nodiscard]] float sample_scalar(const std::vector<Keyframe<float>>& keys, float time) const {
        if (keys.empty()) return 0;

        auto [i0, i1] = find_keys(keys, time);
        if (i0 == i1) return keys[i0].value;

        float t = (time - keys[i0].time) / (keys[i1].time - keys[i0].time);

        if (m_interpolation == Interpolation::Step) {
            return keys[i0].value;
        } else if (m_interpolation == Interpolation::CubicSpline) {
            return cubic_spline_scalar(keys[i0], keys[i1], t, keys[i1].time - keys[i0].time);
        }
        return keys[i0].value + t * (keys[i1].value - keys[i0].value);
    }

    [[nodiscard]] Vec3 cubic_spline_vec3(
        const Keyframe<Vec3>& k0, const Keyframe<Vec3>& k1, float t, float dt) const {

        float t2 = t * t;
        float t3 = t2 * t;

        Vec3 p0 = k0.value;
        Vec3 m0 = k0.out_tangent * dt;
        Vec3 p1 = k1.value;
        Vec3 m1 = k1.in_tangent * dt;

        return Vec3(
            (2*t3 - 3*t2 + 1) * p0.x + (t3 - 2*t2 + t) * m0.x + (-2*t3 + 3*t2) * p1.x + (t3 - t2) * m1.x,
            (2*t3 - 3*t2 + 1) * p0.y + (t3 - 2*t2 + t) * m0.y + (-2*t3 + 3*t2) * p1.y + (t3 - t2) * m1.y,
            (2*t3 - 3*t2 + 1) * p0.z + (t3 - 2*t2 + t) * m0.z + (-2*t3 + 3*t2) * p1.z + (t3 - t2) * m1.z
        );
    }

    [[nodiscard]] float cubic_spline_scalar(
        const Keyframe<float>& k0, const Keyframe<float>& k1, float t, float dt) const {

        float t2 = t * t;
        float t3 = t2 * t;

        float p0 = k0.value;
        float m0 = k0.out_tangent * dt;
        float p1 = k1.value;
        float m1 = k1.in_tangent * dt;

        return (2*t3 - 3*t2 + 1) * p0 + (t3 - 2*t2 + t) * m0 + (-2*t3 + 3*t2) * p1 + (t3 - t2) * m1;
    }
};

// =============================================================================
// Animation Clip
// =============================================================================

/// Animation clip (collection of channels)
class AnimationClip {
public:
    explicit AnimationClip(std::string name = "") : m_name(std::move(name)) {}

    /// Add channel
    void add_channel(AnimationChannel channel) {
        m_duration = std::max(m_duration, channel.duration());
        m_channels.push_back(std::move(channel));
    }

    /// Sample all channels at time, write to local poses
    void sample(float time, std::vector<JointTransform>& poses) const {
        for (const auto& channel : m_channels) {
            std::int32_t joint = channel.target_joint();
            if (joint < 0 || joint >= static_cast<std::int32_t>(poses.size())) continue;

            switch (channel.target_property()) {
                case AnimationTarget::Translation:
                    poses[joint].translation = channel.sample_translation(time);
                    break;
                case AnimationTarget::Rotation:
                    poses[joint].rotation = channel.sample_rotation(time);
                    break;
                case AnimationTarget::Scale:
                    poses[joint].scale = channel.sample_scale(time);
                    break;
                default:
                    break;
            }
        }
    }

    /// Sample morph weights at time
    void sample_weights(float time, std::vector<float>& weights) const {
        for (const auto& channel : m_channels) {
            if (channel.target_property() != AnimationTarget::Weights) continue;

            std::int32_t idx = channel.target_joint();  // Reused as weight index
            if (idx >= 0 && idx < static_cast<std::int32_t>(weights.size())) {
                weights[idx] = channel.sample_weight(time);
            }
        }
    }

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] float duration() const noexcept { return m_duration; }
    [[nodiscard]] const std::vector<AnimationChannel>& channels() const noexcept { return m_channels; }

private:
    std::string m_name;
    float m_duration = 0;
    std::vector<AnimationChannel> m_channels;
};

// =============================================================================
// Animation State
// =============================================================================

/// Loop mode
enum class LoopMode : std::uint8_t {
    Once = 0,      // Play once and stop
    Loop,          // Loop continuously
    PingPong,      // Alternate forward/backward
    Clamp          // Clamp to end
};

/// Animation playback state
struct AnimationState {
    const AnimationClip* clip = nullptr;
    float time = 0;
    float speed = 1.0f;
    float weight = 1.0f;  // For blending
    LoopMode loop_mode = LoopMode::Loop;
    bool playing = false;
    bool reverse = false;  // For ping-pong

    /// Advance time
    void update(float dt) {
        if (!playing || !clip) return;

        float direction = reverse ? -1.0f : 1.0f;
        time += dt * speed * direction;

        float duration = clip->duration();
        if (duration <= 0) return;

        switch (loop_mode) {
            case LoopMode::Once:
                if (time >= duration) {
                    time = duration;
                    playing = false;
                }
                break;

            case LoopMode::Loop:
                while (time >= duration) time -= duration;
                while (time < 0) time += duration;
                break;

            case LoopMode::PingPong:
                if (time >= duration) {
                    time = duration;
                    reverse = true;
                } else if (time <= 0) {
                    time = 0;
                    reverse = false;
                }
                break;

            case LoopMode::Clamp:
                time = std::clamp(time, 0.0f, duration);
                break;
        }
    }

    /// Get normalized time (0-1)
    [[nodiscard]] float normalized_time() const {
        if (!clip || clip->duration() <= 0) return 0;
        return time / clip->duration();
    }
};

// =============================================================================
// Animation Mixer (blending multiple animations)
// =============================================================================

/// Blend mode for combining animations
enum class BlendMode : std::uint8_t {
    Override = 0,  // Replace base pose
    Additive,      // Add to base pose
    Layered        // Blend based on weight
};

/// Animation layer for mixing
struct AnimationLayer {
    std::string name;
    AnimationState state;
    BlendMode blend_mode = BlendMode::Layered;
    std::uint32_t mask = 0xFFFFFFFF;  // Joint mask (bitfield for first 32 joints)

    /// Check if joint is affected by this layer
    [[nodiscard]] bool affects_joint(std::int32_t joint_index) const {
        if (joint_index >= 32) return true;  // Beyond mask, assume affected
        return (mask & (1u << joint_index)) != 0;
    }
};

/// Animation mixer
class AnimationMixer {
public:
    explicit AnimationMixer(const Skeleton* skeleton) : m_skeleton(skeleton) {
        if (skeleton) {
            m_base_pose.resize(skeleton->joint_count());
            m_blended_pose.resize(skeleton->joint_count());

            // Initialize with bind pose
            for (std::size_t i = 0; i < skeleton->joint_count(); ++i) {
                m_base_pose[i] = skeleton->joints()[i].bind_pose;
            }
        }
    }

    /// Add animation layer
    std::size_t add_layer(const std::string& name) {
        AnimationLayer layer;
        layer.name = name;
        m_layers.push_back(std::move(layer));
        return m_layers.size() - 1;
    }

    /// Get layer
    [[nodiscard]] AnimationLayer* get_layer(std::size_t index) {
        return index < m_layers.size() ? &m_layers[index] : nullptr;
    }

    /// Play animation on layer
    void play(std::size_t layer_index, const AnimationClip* clip, float blend_time = 0.2f) {
        if (layer_index >= m_layers.size() || !clip) return;

        auto& layer = m_layers[layer_index];
        layer.state.clip = clip;
        layer.state.time = 0;
        layer.state.playing = true;
        layer.state.reverse = false;

        // TODO: Handle blend time for smooth transitions
        (void)blend_time;
    }

    /// Stop animation on layer
    void stop(std::size_t layer_index) {
        if (layer_index < m_layers.size()) {
            m_layers[layer_index].state.playing = false;
        }
    }

    /// Update all layers
    void update(float dt) {
        for (auto& layer : m_layers) {
            layer.state.update(dt);
        }
    }

    /// Evaluate and blend all layers
    void evaluate(std::vector<Mat4>& skinning_matrices) {
        if (!m_skeleton) return;

        // Start with bind pose
        m_blended_pose = m_base_pose;

        // Apply each layer
        for (const auto& layer : m_layers) {
            if (!layer.state.playing || !layer.state.clip) continue;

            // Sample animation into temporary pose
            std::vector<JointTransform> layer_pose = m_base_pose;
            layer.state.clip->sample(layer.state.time, layer_pose);

            // Blend into final pose
            float weight = layer.state.weight;

            for (std::size_t i = 0; i < m_blended_pose.size(); ++i) {
                if (!layer.affects_joint(static_cast<std::int32_t>(i))) continue;

                switch (layer.blend_mode) {
                    case BlendMode::Override:
                        m_blended_pose[i] = layer_pose[i];
                        break;

                    case BlendMode::Additive:
                        // Add delta from bind pose
                        m_blended_pose[i].translation = m_blended_pose[i].translation +
                            (layer_pose[i].translation - m_base_pose[i].translation) * weight;
                        // Rotation additive is complex - simplified here
                        m_blended_pose[i].rotation = Quat::slerp(
                            m_blended_pose[i].rotation,
                            m_blended_pose[i].rotation * layer_pose[i].rotation * m_base_pose[i].rotation.conjugate(),
                            weight);
                        m_blended_pose[i].scale = m_blended_pose[i].scale +
                            (layer_pose[i].scale - m_base_pose[i].scale) * weight;
                        break;

                    case BlendMode::Layered:
                        m_blended_pose[i] = JointTransform::lerp(m_blended_pose[i], layer_pose[i], weight);
                        break;
                }
            }
        }

        // Compute final matrices
        std::vector<Mat4> world_matrices;
        m_skeleton->compute_world_matrices(m_blended_pose, world_matrices);
        m_skeleton->compute_skinning_matrices(world_matrices, skinning_matrices);
    }

    /// Get current pose
    [[nodiscard]] const std::vector<JointTransform>& current_pose() const noexcept {
        return m_blended_pose;
    }

private:
    const Skeleton* m_skeleton;
    std::vector<AnimationLayer> m_layers;
    std::vector<JointTransform> m_base_pose;
    std::vector<JointTransform> m_blended_pose;
};

// =============================================================================
// Morph Target
// =============================================================================

/// Morph target (blend shape) delta
struct MorphTargetDelta {
    std::uint32_t vertex_index;
    Vec3 position_delta;
    Vec3 normal_delta;
    Vec3 tangent_delta;
};

/// Morph target
class MorphTarget {
public:
    explicit MorphTarget(std::string name = "") : m_name(std::move(name)) {}

    /// Add vertex delta
    void add_delta(std::uint32_t vertex_index, const Vec3& pos_delta,
                   const Vec3& norm_delta = {}, const Vec3& tan_delta = {}) {
        m_deltas.push_back({vertex_index, pos_delta, norm_delta, tan_delta});
    }

    /// Apply morph to mesh data
    void apply(MeshData& mesh, float weight) const {
        if (weight < 0.001f) return;

        auto& vertices = mesh.vertices();

        for (const auto& delta : m_deltas) {
            if (delta.vertex_index >= vertices.size()) continue;

            auto& v = vertices[delta.vertex_index];
            v.position[0] += delta.position_delta.x * weight;
            v.position[1] += delta.position_delta.y * weight;
            v.position[2] += delta.position_delta.z * weight;

            v.normal[0] += delta.normal_delta.x * weight;
            v.normal[1] += delta.normal_delta.y * weight;
            v.normal[2] += delta.normal_delta.z * weight;

            // Renormalize normal
            float len = std::sqrt(v.normal[0]*v.normal[0] + v.normal[1]*v.normal[1] + v.normal[2]*v.normal[2]);
            if (len > 1e-6f) {
                v.normal[0] /= len;
                v.normal[1] /= len;
                v.normal[2] /= len;
            }
        }
    }

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<MorphTargetDelta>& deltas() const noexcept { return m_deltas; }

private:
    std::string m_name;
    std::vector<MorphTargetDelta> m_deltas;
};

// =============================================================================
// Morph Target Set
// =============================================================================

/// Collection of morph targets for a mesh
class MorphTargetSet {
public:
    /// Add morph target
    std::size_t add_target(MorphTarget target) {
        m_weights.push_back(0.0f);
        m_targets.push_back(std::move(target));
        return m_targets.size() - 1;
    }

    /// Get target by index
    [[nodiscard]] const MorphTarget* get_target(std::size_t index) const {
        return index < m_targets.size() ? &m_targets[index] : nullptr;
    }

    /// Get target by name
    [[nodiscard]] std::size_t find_target(const std::string& name) const {
        for (std::size_t i = 0; i < m_targets.size(); ++i) {
            if (m_targets[i].name() == name) return i;
        }
        return SIZE_MAX;
    }

    /// Set weight
    void set_weight(std::size_t index, float weight) {
        if (index < m_weights.size()) {
            m_weights[index] = std::clamp(weight, 0.0f, 1.0f);
        }
    }

    /// Get weight
    [[nodiscard]] float get_weight(std::size_t index) const {
        return index < m_weights.size() ? m_weights[index] : 0.0f;
    }

    /// Get all weights (for animation)
    [[nodiscard]] std::vector<float>& weights() noexcept { return m_weights; }
    [[nodiscard]] const std::vector<float>& weights() const noexcept { return m_weights; }

    /// Apply all active morphs to mesh
    void apply_to_mesh(MeshData& mesh, const MeshData& base_mesh) const {
        // Reset to base mesh
        mesh = base_mesh;

        // Apply each morph with non-zero weight
        for (std::size_t i = 0; i < m_targets.size(); ++i) {
            if (m_weights[i] > 0.001f) {
                m_targets[i].apply(mesh, m_weights[i]);
            }
        }
    }

    [[nodiscard]] std::size_t target_count() const noexcept { return m_targets.size(); }

private:
    std::vector<MorphTarget> m_targets;
    std::vector<float> m_weights;
};

// =============================================================================
// Skinned Mesh
// =============================================================================

/// Skinned vertex data (extends base Vertex)
struct SkinnedVertexData {
    std::array<std::uint8_t, 4> joint_indices = {0, 0, 0, 0};
    std::array<float, 4> joint_weights = {0, 0, 0, 0};
};

/// Skinned mesh with skeleton binding
class SkinnedMesh {
public:
    SkinnedMesh() = default;

    /// Set base mesh
    void set_mesh(MeshData mesh) {
        m_base_mesh = std::move(mesh);
        m_skinned_mesh = m_base_mesh;
        m_skin_data.resize(m_base_mesh.vertex_count());
    }

    /// Set skeleton
    void set_skeleton(std::shared_ptr<Skeleton> skeleton) {
        m_skeleton = std::move(skeleton);
        if (m_skeleton) {
            m_skinning_matrices.resize(m_skeleton->joint_count());
        }
    }

    /// Set skin data for vertex
    void set_skin_data(std::size_t vertex_index, const SkinnedVertexData& data) {
        if (vertex_index < m_skin_data.size()) {
            m_skin_data[vertex_index] = data;
        }
    }

    /// Set morph targets
    void set_morph_targets(MorphTargetSet targets) {
        m_morph_targets = std::move(targets);
    }

    /// Get morph targets
    [[nodiscard]] MorphTargetSet& morph_targets() noexcept { return m_morph_targets; }

    /// Update mesh with current animation state
    void update(const std::vector<Mat4>& skinning_matrices) {
        if (m_base_mesh.vertex_count() == 0) return;

        // Apply morph targets first
        if (m_morph_targets.target_count() > 0) {
            m_morph_targets.apply_to_mesh(m_skinned_mesh, m_base_mesh);
        } else {
            m_skinned_mesh = m_base_mesh;
        }

        // Apply skinning
        auto& vertices = m_skinned_mesh.vertices();

        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto& skin = m_skin_data[i];
            const auto& base_v = m_base_mesh.vertices()[i];

            // Accumulate weighted transforms
            Vec3 skinned_pos = {0, 0, 0};
            Vec3 skinned_norm = {0, 0, 0};

            for (int j = 0; j < 4; ++j) {
                float weight = skin.joint_weights[j];
                if (weight < 0.001f) continue;

                std::uint8_t joint_idx = skin.joint_indices[j];
                if (joint_idx >= skinning_matrices.size()) continue;

                const auto& mat = skinning_matrices[joint_idx].m;

                // Transform position
                float px = base_v.position[0], py = base_v.position[1], pz = base_v.position[2];
                skinned_pos.x += (mat[0]*px + mat[4]*py + mat[8]*pz + mat[12]) * weight;
                skinned_pos.y += (mat[1]*px + mat[5]*py + mat[9]*pz + mat[13]) * weight;
                skinned_pos.z += (mat[2]*px + mat[6]*py + mat[10]*pz + mat[14]) * weight;

                // Transform normal (no translation)
                float nx = base_v.normal[0], ny = base_v.normal[1], nz = base_v.normal[2];
                skinned_norm.x += (mat[0]*nx + mat[4]*ny + mat[8]*nz) * weight;
                skinned_norm.y += (mat[1]*nx + mat[5]*ny + mat[9]*nz) * weight;
                skinned_norm.z += (mat[2]*nx + mat[6]*ny + mat[10]*nz) * weight;
            }

            // Write back
            vertices[i].position = {skinned_pos.x, skinned_pos.y, skinned_pos.z};

            // Normalize
            float len = std::sqrt(skinned_norm.x*skinned_norm.x + skinned_norm.y*skinned_norm.y + skinned_norm.z*skinned_norm.z);
            if (len > 1e-6f) {
                vertices[i].normal = {skinned_norm.x/len, skinned_norm.y/len, skinned_norm.z/len};
            }
        }
    }

    /// Get current skinned mesh (for rendering)
    [[nodiscard]] const MeshData& skinned_mesh() const noexcept { return m_skinned_mesh; }

    /// Get base mesh
    [[nodiscard]] const MeshData& base_mesh() const noexcept { return m_base_mesh; }

    /// Get skeleton
    [[nodiscard]] const Skeleton* skeleton() const noexcept { return m_skeleton.get(); }

    /// Get skin data
    [[nodiscard]] const std::vector<SkinnedVertexData>& skin_data() const noexcept { return m_skin_data; }

private:
    MeshData m_base_mesh;
    MeshData m_skinned_mesh;
    std::vector<SkinnedVertexData> m_skin_data;
    std::shared_ptr<Skeleton> m_skeleton;
    std::vector<Mat4> m_skinning_matrices;
    MorphTargetSet m_morph_targets;
};

// =============================================================================
// Animation Manager
// =============================================================================

class AnimationManager {
public:
    /// Register skeleton
    std::size_t register_skeleton(std::shared_ptr<Skeleton> skeleton) {
        std::size_t id = m_next_skeleton_id++;
        m_skeletons[id] = std::move(skeleton);
        return id;
    }

    /// Get skeleton
    [[nodiscard]] Skeleton* get_skeleton(std::size_t id) {
        auto it = m_skeletons.find(id);
        return it != m_skeletons.end() ? it->second.get() : nullptr;
    }

    /// Register animation clip
    std::size_t register_clip(std::shared_ptr<AnimationClip> clip) {
        std::size_t id = m_next_clip_id++;
        m_clips[id] = std::move(clip);
        return id;
    }

    /// Get animation clip
    [[nodiscard]] AnimationClip* get_clip(std::size_t id) {
        auto it = m_clips.find(id);
        return it != m_clips.end() ? it->second.get() : nullptr;
    }

    /// Find clip by name
    [[nodiscard]] std::size_t find_clip(const std::string& name) const {
        for (const auto& [id, clip] : m_clips) {
            if (clip->name() == name) return id;
        }
        return SIZE_MAX;
    }

    /// Create mixer for skeleton
    [[nodiscard]] std::unique_ptr<AnimationMixer> create_mixer(std::size_t skeleton_id) {
        auto* skeleton = get_skeleton(skeleton_id);
        if (!skeleton) return nullptr;
        return std::make_unique<AnimationMixer>(skeleton);
    }

    /// Update all registered mixers
    void update(float dt) {
        // Note: In real implementation, would track active mixers
        (void)dt;
    }

    /// Get statistics
    struct Stats {
        std::size_t skeleton_count = 0;
        std::size_t clip_count = 0;
        std::size_t total_joints = 0;
        std::size_t total_channels = 0;
    };

    [[nodiscard]] Stats get_stats() const {
        Stats stats;
        stats.skeleton_count = m_skeletons.size();
        stats.clip_count = m_clips.size();

        for (const auto& [id, skeleton] : m_skeletons) {
            stats.total_joints += skeleton->joint_count();
        }

        for (const auto& [id, clip] : m_clips) {
            stats.total_channels += clip->channels().size();
        }

        return stats;
    }

private:
    std::unordered_map<std::size_t, std::shared_ptr<Skeleton>> m_skeletons;
    std::unordered_map<std::size_t, std::shared_ptr<AnimationClip>> m_clips;
    std::size_t m_next_skeleton_id = 1;
    std::size_t m_next_clip_id = 1;
};

// =============================================================================
// GPU Skinning Data (for shader upload)
// =============================================================================

/// GPU-ready skinning uniform data
struct alignas(16) GpuSkinningData {
    static constexpr std::size_t MAX_BONES = 256;

    // Joint matrices (4x4 each, 64 bytes per joint)
    std::array<std::array<float, 16>, MAX_BONES> joint_matrices;

    // Active joint count
    std::uint32_t joint_count = 0;
    std::uint32_t _pad[3] = {0, 0, 0};

    /// Upload matrices from animation evaluation
    void upload(const std::vector<Mat4>& matrices) {
        joint_count = static_cast<std::uint32_t>(std::min(matrices.size(), MAX_BONES));
        for (std::size_t i = 0; i < joint_count; ++i) {
            joint_matrices[i] = matrices[i].m;
        }
    }
};

/// GPU-ready morph weights
struct alignas(16) GpuMorphWeights {
    static constexpr std::size_t MAX_MORPHS = 64;

    std::array<float, MAX_MORPHS> weights = {};
    std::uint32_t morph_count = 0;
    std::uint32_t _pad[3] = {0, 0, 0};

    void upload(const std::vector<float>& w) {
        morph_count = static_cast<std::uint32_t>(std::min(w.size(), MAX_MORPHS));
        for (std::size_t i = 0; i < morph_count; ++i) {
            weights[i] = w[i];
        }
    }
};

// =============================================================================
// Dual Quaternion for Skinning
// =============================================================================

/// Dual quaternion for volume-preserving skinning
struct DualQuat {
    Quat real;   // Rotation part
    Quat dual;   // Translation part (encoded)

    DualQuat() : real(Quat::identity()), dual(0, 0, 0, 0) {}

    DualQuat(const Quat& rotation, const Vec3& translation)
        : real(rotation.normalized()) {
        // dual = 0.5 * translation_quat * real
        Quat t(translation.x, translation.y, translation.z, 0);
        dual = Quat(
            0.5f * (t.w * real.x + t.x * real.w + t.y * real.z - t.z * real.y),
            0.5f * (t.w * real.y - t.x * real.z + t.y * real.w + t.z * real.x),
            0.5f * (t.w * real.z + t.x * real.y - t.y * real.x + t.z * real.w),
            0.5f * (t.w * real.w - t.x * real.x - t.y * real.y - t.z * real.z)
        );
    }

    /// Create from 4x4 matrix
    [[nodiscard]] static DualQuat from_matrix(const Mat4& m) {
        // Extract rotation as quaternion
        Quat rot;
        float trace = m.m[0] + m.m[5] + m.m[10];
        if (trace > 0) {
            float s = 0.5f / std::sqrt(trace + 1.0f);
            rot.w = 0.25f / s;
            rot.x = (m.m[6] - m.m[9]) * s;
            rot.y = (m.m[8] - m.m[2]) * s;
            rot.z = (m.m[1] - m.m[4]) * s;
        } else if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
            float s = 2.0f * std::sqrt(1.0f + m.m[0] - m.m[5] - m.m[10]);
            rot.w = (m.m[6] - m.m[9]) / s;
            rot.x = 0.25f * s;
            rot.y = (m.m[4] + m.m[1]) / s;
            rot.z = (m.m[8] + m.m[2]) / s;
        } else if (m.m[5] > m.m[10]) {
            float s = 2.0f * std::sqrt(1.0f + m.m[5] - m.m[0] - m.m[10]);
            rot.w = (m.m[8] - m.m[2]) / s;
            rot.x = (m.m[4] + m.m[1]) / s;
            rot.y = 0.25f * s;
            rot.z = (m.m[9] + m.m[6]) / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + m.m[10] - m.m[0] - m.m[5]);
            rot.w = (m.m[1] - m.m[4]) / s;
            rot.x = (m.m[8] + m.m[2]) / s;
            rot.y = (m.m[9] + m.m[6]) / s;
            rot.z = 0.25f * s;
        }

        // Extract translation
        Vec3 trans(m.m[12], m.m[13], m.m[14]);

        return DualQuat(rot, trans);
    }

    /// Normalize the dual quaternion
    [[nodiscard]] DualQuat normalized() const {
        float len = std::sqrt(real.x*real.x + real.y*real.y + real.z*real.z + real.w*real.w);
        if (len < 1e-8f) return DualQuat();

        DualQuat result;
        result.real = Quat(real.x/len, real.y/len, real.z/len, real.w/len);
        result.dual = Quat(dual.x/len, dual.y/len, dual.z/len, dual.w/len);
        return result;
    }

    /// Blend multiple dual quaternions (DLB - Dual quaternion Linear Blending)
    [[nodiscard]] static DualQuat blend(const std::vector<DualQuat>& dqs, const std::vector<float>& weights) {
        if (dqs.empty()) return DualQuat();

        DualQuat result;
        result.real = Quat(0, 0, 0, 0);
        result.dual = Quat(0, 0, 0, 0);

        // Ensure consistent hemisphere (all quaternions should be in same hemisphere as first)
        float sign = 1.0f;

        for (std::size_t i = 0; i < dqs.size(); ++i) {
            float w = i < weights.size() ? weights[i] : 0.0f;
            if (w < 1e-6f) continue;

            // Check hemisphere
            if (i > 0) {
                float dot = dqs[0].real.x * dqs[i].real.x +
                           dqs[0].real.y * dqs[i].real.y +
                           dqs[0].real.z * dqs[i].real.z +
                           dqs[0].real.w * dqs[i].real.w;
                sign = dot < 0 ? -1.0f : 1.0f;
            }

            result.real.x += w * sign * dqs[i].real.x;
            result.real.y += w * sign * dqs[i].real.y;
            result.real.z += w * sign * dqs[i].real.z;
            result.real.w += w * sign * dqs[i].real.w;

            result.dual.x += w * sign * dqs[i].dual.x;
            result.dual.y += w * sign * dqs[i].dual.y;
            result.dual.z += w * sign * dqs[i].dual.z;
            result.dual.w += w * sign * dqs[i].dual.w;
        }

        return result.normalized();
    }

    /// Transform a point using the dual quaternion
    [[nodiscard]] Vec3 transform_point(const Vec3& p) const {
        // Rotation: q * p * q^-1
        Quat pq(p.x, p.y, p.z, 0);
        Quat rotated = real * pq * real.conjugate();

        // Translation: 2 * dual * real^-1
        Quat trans = Quat(
            2.0f * (dual.w * real.x - dual.x * real.w - dual.y * real.z + dual.z * real.y),
            2.0f * (dual.w * real.y + dual.x * real.z - dual.y * real.w - dual.z * real.x),
            2.0f * (dual.w * real.z - dual.x * real.y + dual.y * real.x - dual.z * real.w),
            0
        );

        return Vec3(rotated.x + trans.x, rotated.y + trans.y, rotated.z + trans.z);
    }

    /// Transform a vector (direction, no translation)
    [[nodiscard]] Vec3 transform_vector(const Vec3& v) const {
        Quat vq(v.x, v.y, v.z, 0);
        Quat rotated = real * vq * real.conjugate();
        return Vec3(rotated.x, rotated.y, rotated.z);
    }
};

// =============================================================================
// Dual Quaternion Skinning
// =============================================================================

/// Dual quaternion skinning data for GPU upload
struct alignas(16) GpuDualQuatSkinningData {
    static constexpr std::size_t MAX_BONES = 256;

    // Dual quaternions (8 floats each: real.xyzw + dual.xyzw)
    std::array<std::array<float, 8>, MAX_BONES> dual_quats;

    std::uint32_t joint_count = 0;
    std::uint32_t _pad[3] = {0, 0, 0};

    void upload(const std::vector<Mat4>& matrices) {
        joint_count = static_cast<std::uint32_t>(std::min(matrices.size(), MAX_BONES));
        for (std::size_t i = 0; i < joint_count; ++i) {
            DualQuat dq = DualQuat::from_matrix(matrices[i]);
            dual_quats[i][0] = dq.real.x;
            dual_quats[i][1] = dq.real.y;
            dual_quats[i][2] = dq.real.z;
            dual_quats[i][3] = dq.real.w;
            dual_quats[i][4] = dq.dual.x;
            dual_quats[i][5] = dq.dual.y;
            dual_quats[i][6] = dq.dual.z;
            dual_quats[i][7] = dq.dual.w;
        }
    }
};

/// CPU dual quaternion skinning implementation
class DualQuatSkinner {
public:
    /// Apply dual quaternion skinning to mesh
    static void skin(
        const std::vector<Vertex>& base_vertices,
        const std::vector<SkinnedVertexData>& skin_data,
        const std::vector<Mat4>& skinning_matrices,
        std::vector<Vertex>& output_vertices) {

        if (base_vertices.size() != skin_data.size()) return;
        output_vertices.resize(base_vertices.size());

        // Convert matrices to dual quaternions
        std::vector<DualQuat> dual_quats(skinning_matrices.size());
        for (std::size_t i = 0; i < skinning_matrices.size(); ++i) {
            dual_quats[i] = DualQuat::from_matrix(skinning_matrices[i]);
        }

        // Skin each vertex
        for (std::size_t i = 0; i < base_vertices.size(); ++i) {
            const auto& base_v = base_vertices[i];
            const auto& skin = skin_data[i];
            auto& out_v = output_vertices[i];

            // Blend dual quaternions
            std::vector<DualQuat> vertex_dqs;
            std::vector<float> weights;

            for (int j = 0; j < 4; ++j) {
                float w = skin.joint_weights[j];
                if (w < 0.001f) continue;

                std::uint8_t joint_idx = skin.joint_indices[j];
                if (joint_idx >= dual_quats.size()) continue;

                vertex_dqs.push_back(dual_quats[joint_idx]);
                weights.push_back(w);
            }

            DualQuat blended = DualQuat::blend(vertex_dqs, weights);

            // Transform position and normal
            Vec3 pos(base_v.position[0], base_v.position[1], base_v.position[2]);
            Vec3 norm(base_v.normal[0], base_v.normal[1], base_v.normal[2]);

            Vec3 skinned_pos = blended.transform_point(pos);
            Vec3 skinned_norm = blended.transform_vector(norm);

            // Normalize
            float len = std::sqrt(skinned_norm.x*skinned_norm.x +
                                  skinned_norm.y*skinned_norm.y +
                                  skinned_norm.z*skinned_norm.z);
            if (len > 1e-6f) {
                skinned_norm.x /= len;
                skinned_norm.y /= len;
                skinned_norm.z /= len;
            }

            // Write output
            out_v = base_v;
            out_v.position = {skinned_pos.x, skinned_pos.y, skinned_pos.z};
            out_v.normal = {skinned_norm.x, skinned_norm.y, skinned_norm.z};
        }
    }
};

// =============================================================================
// Compute Shader Skinning
// =============================================================================

/// GLSL compute shader source for GPU skinning (linear blend)
static const char* COMPUTE_SKINNING_SHADER_LBS = R"(
#version 450

layout(local_size_x = 256) in;

// Input vertex data
struct Vertex {
    vec3 position;
    float pad0;
    vec3 normal;
    float pad1;
    vec2 texcoord;
    vec2 pad2;
    vec3 tangent;
    float pad3;
    vec4 color;
};

// Skin weights per vertex
struct SkinData {
    uvec4 joint_indices;
    vec4 joint_weights;
};

layout(std430, binding = 0) readonly buffer InputVertices {
    Vertex input_vertices[];
};

layout(std430, binding = 1) readonly buffer SkinDataBuffer {
    SkinData skin_data[];
};

layout(std430, binding = 2) readonly buffer JointMatrices {
    mat4 joint_matrices[256];
};

layout(std430, binding = 3) writeonly buffer OutputVertices {
    Vertex output_vertices[];
};

layout(push_constant) uniform PushConstants {
    uint vertex_count;
    uint joint_count;
} constants;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= constants.vertex_count) return;

    Vertex v = input_vertices[idx];
    SkinData skin = skin_data[idx];

    vec3 skinned_pos = vec3(0.0);
    vec3 skinned_norm = vec3(0.0);
    vec3 skinned_tan = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        float weight = skin.joint_weights[i];
        if (weight < 0.001) continue;

        uint joint_idx = skin.joint_indices[i];
        if (joint_idx >= constants.joint_count) continue;

        mat4 m = joint_matrices[joint_idx];

        skinned_pos += weight * (m * vec4(v.position, 1.0)).xyz;
        skinned_norm += weight * (mat3(m) * v.normal);
        skinned_tan += weight * (mat3(m) * v.tangent);
    }

    output_vertices[idx].position = skinned_pos;
    output_vertices[idx].normal = normalize(skinned_norm);
    output_vertices[idx].tangent = normalize(skinned_tan);
    output_vertices[idx].texcoord = v.texcoord;
    output_vertices[idx].color = v.color;
}
)";

/// GLSL compute shader source for dual quaternion skinning
static const char* COMPUTE_SKINNING_SHADER_DQS = R"(
#version 450

layout(local_size_x = 256) in;

struct Vertex {
    vec3 position;
    float pad0;
    vec3 normal;
    float pad1;
    vec2 texcoord;
    vec2 pad2;
    vec3 tangent;
    float pad3;
    vec4 color;
};

struct SkinData {
    uvec4 joint_indices;
    vec4 joint_weights;
};

// Dual quaternion: real (xyzw) + dual (xyzw)
struct DualQuat {
    vec4 real;
    vec4 dual;
};

layout(std430, binding = 0) readonly buffer InputVertices {
    Vertex input_vertices[];
};

layout(std430, binding = 1) readonly buffer SkinDataBuffer {
    SkinData skin_data[];
};

layout(std430, binding = 2) readonly buffer DualQuaternions {
    DualQuat dual_quats[256];
};

layout(std430, binding = 3) writeonly buffer OutputVertices {
    Vertex output_vertices[];
};

layout(push_constant) uniform PushConstants {
    uint vertex_count;
    uint joint_count;
} constants;

// Quaternion multiplication
vec4 quat_mul(vec4 a, vec4 b) {
    return vec4(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    );
}

// Conjugate of quaternion
vec4 quat_conj(vec4 q) {
    return vec4(-q.xyz, q.w);
}

// Transform point by dual quaternion
vec3 dq_transform_point(DualQuat dq, vec3 p) {
    // Rotation: q * p * q^-1
    vec4 pq = vec4(p, 0.0);
    vec4 rotated = quat_mul(quat_mul(dq.real, pq), quat_conj(dq.real));

    // Translation: 2 * dual * real^-1
    vec4 trans = 2.0 * quat_mul(dq.dual, quat_conj(dq.real));

    return rotated.xyz + trans.xyz;
}

// Transform vector by dual quaternion (rotation only)
vec3 dq_transform_vector(DualQuat dq, vec3 v) {
    vec4 vq = vec4(v, 0.0);
    vec4 rotated = quat_mul(quat_mul(dq.real, vq), quat_conj(dq.real));
    return rotated.xyz;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= constants.vertex_count) return;

    Vertex v = input_vertices[idx];
    SkinData skin = skin_data[idx];

    // Blend dual quaternions
    DualQuat blended;
    blended.real = vec4(0.0);
    blended.dual = vec4(0.0);

    // Reference quaternion for hemisphere check
    DualQuat ref_dq = dual_quats[skin.joint_indices.x];

    for (int i = 0; i < 4; ++i) {
        float weight = skin.joint_weights[i];
        if (weight < 0.001) continue;

        uint joint_idx = skin.joint_indices[i];
        if (joint_idx >= constants.joint_count) continue;

        DualQuat dq = dual_quats[joint_idx];

        // Hemisphere check
        float sign = sign(dot(ref_dq.real, dq.real));
        if (sign < 0.0) {
            dq.real = -dq.real;
            dq.dual = -dq.dual;
        }

        blended.real += weight * dq.real;
        blended.dual += weight * dq.dual;
    }

    // Normalize
    float len = length(blended.real);
    if (len > 0.0001) {
        blended.real /= len;
        blended.dual /= len;
    }

    // Transform
    output_vertices[idx].position = dq_transform_point(blended, v.position);
    output_vertices[idx].normal = normalize(dq_transform_vector(blended, v.normal));
    output_vertices[idx].tangent = normalize(dq_transform_vector(blended, v.tangent));
    output_vertices[idx].texcoord = v.texcoord;
    output_vertices[idx].color = v.color;
}
)";

/// Compute skinning configuration
struct ComputeSkinningConfig {
    bool use_dual_quaternions = false;
    std::uint32_t workgroup_size = 256;
};

/// Compute skinning manager for GPU-accelerated skeletal animation
class ComputeSkinner {
public:
    /// Get linear blend skinning compute shader source
    [[nodiscard]] static const char* get_lbs_shader() { return COMPUTE_SKINNING_SHADER_LBS; }

    /// Get dual quaternion skinning compute shader source
    [[nodiscard]] static const char* get_dqs_shader() { return COMPUTE_SKINNING_SHADER_DQS; }

    /// Calculate dispatch size for given vertex count
    [[nodiscard]] static std::uint32_t calculate_dispatch_groups(
        std::uint32_t vertex_count, std::uint32_t workgroup_size = 256) {
        return (vertex_count + workgroup_size - 1) / workgroup_size;
    }

    /// Generate SPIR-V bytecode for skinning shaders (placeholder)
    /// In production, would use shaderc or similar to compile GLSL to SPIR-V
    [[nodiscard]] static std::vector<std::uint32_t> compile_to_spirv(
        const char* glsl_source, bool /* is_compute */ = true) {
        // Placeholder - real implementation would use shaderc
        (void)glsl_source;
        return {};
    }
};

// =============================================================================
// Enhanced Skinned Mesh with Dual Quaternion Support
// =============================================================================

/// Skinning method selection
enum class SkinningMethod : std::uint8_t {
    LinearBlend = 0,      // Traditional LBS
    DualQuaternion = 1,   // DQS (better volume preservation)
    ComputeLBS = 2,       // GPU compute LBS
    ComputeDQS = 3        // GPU compute DQS
};

/// Extended skinned mesh with multiple skinning methods
class SkinnedMeshEx : public SkinnedMesh {
public:
    /// Set skinning method
    void set_skinning_method(SkinningMethod method) { m_method = method; }
    [[nodiscard]] SkinningMethod skinning_method() const noexcept { return m_method; }

    /// Update with specified skinning method
    void update_with_method(const std::vector<Mat4>& skinning_matrices, SkinningMethod method) {
        if (base_mesh().vertex_count() == 0) return;

        // Apply morph targets first
        MeshData morphed_mesh;
        if (morph_targets().target_count() > 0) {
            morph_targets().apply_to_mesh(morphed_mesh, base_mesh());
        } else {
            morphed_mesh = base_mesh();
        }

        switch (method) {
            case SkinningMethod::DualQuaternion:
                apply_dqs_skinning(morphed_mesh, skinning_matrices);
                break;

            case SkinningMethod::LinearBlend:
            default:
                // Use parent class LBS implementation
                update(skinning_matrices);
                return;
        }
    }

private:
    SkinningMethod m_method = SkinningMethod::LinearBlend;

    void apply_dqs_skinning(const MeshData& base, const std::vector<Mat4>& skinning_matrices) {
        MeshData& output = const_cast<MeshData&>(skinned_mesh());
        output = base;

        DualQuatSkinner::skin(
            base.vertices(),
            skin_data(),
            skinning_matrices,
            output.vertices()
        );
    }
};

} // namespace void_render
