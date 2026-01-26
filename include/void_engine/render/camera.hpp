#pragma once

/// @file camera.hpp
/// @brief Camera system for void_render

#include "fwd.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <cmath>
#include <algorithm>
#include <numbers>
#include <glm/glm.hpp>

namespace void_render {

// Forward declarations for Frustum methods
struct AABB;
struct BoundingSphere;

// =============================================================================
// Constants
// =============================================================================

/// Default field of view (radians)
static constexpr float DEFAULT_FOV = std::numbers::pi_v<float> / 4.0f;  // 45 degrees

/// Default near plane
static constexpr float DEFAULT_NEAR = 0.1f;

/// Default far plane
static constexpr float DEFAULT_FAR = 1000.0f;

/// Minimum pitch angle (radians)
static constexpr float MIN_PITCH = -std::numbers::pi_v<float> / 2.0f + 0.01f;

/// Maximum pitch angle (radians)
static constexpr float MAX_PITCH = std::numbers::pi_v<float> / 2.0f - 0.01f;

// =============================================================================
// Projection
// =============================================================================

/// Projection type
enum class ProjectionType : std::uint8_t {
    Perspective = 0,
    Orthographic = 1,
};

/// Perspective projection parameters
struct PerspectiveProjection {
    float fov_y = DEFAULT_FOV;      // Vertical field of view (radians)
    float aspect_ratio = 16.0f / 9.0f;
    float near_plane = DEFAULT_NEAR;
    float far_plane = DEFAULT_FAR;

    /// Create with aspect ratio
    [[nodiscard]] static PerspectiveProjection with_aspect(float aspect) {
        PerspectiveProjection proj;
        proj.aspect_ratio = aspect;
        return proj;
    }

    /// Create with dimensions
    [[nodiscard]] static PerspectiveProjection with_size(float width, float height) {
        return with_aspect(width / height);
    }

    /// Compute projection matrix (column-major, right-handed, reverse-Z for depth precision)
    [[nodiscard]] std::array<std::array<float, 4>, 4> matrix() const {
        float tan_half_fov = std::tan(fov_y / 2.0f);
        float f = 1.0f / tan_half_fov;

        // Reverse-Z: near maps to 1, far maps to 0 (better depth precision)
        // Right-handed coordinate system with -Z forward
        std::array<std::array<float, 4>, 4> m = {};
        m[0][0] = f / aspect_ratio;
        m[1][1] = f;
        m[2][2] = near_plane / (far_plane - near_plane);  // Reverse-Z
        m[2][3] = -1.0f;  // -Z forward
        m[3][2] = (far_plane * near_plane) / (far_plane - near_plane);  // Reverse-Z
        return m;
    }

    /// Compute standard projection matrix (non-reverse-Z, for compatibility)
    [[nodiscard]] std::array<std::array<float, 4>, 4> matrix_standard() const {
        float tan_half_fov = std::tan(fov_y / 2.0f);
        float f = 1.0f / tan_half_fov;

        std::array<std::array<float, 4>, 4> m = {};
        m[0][0] = f / aspect_ratio;
        m[1][1] = f;
        m[2][2] = far_plane / (near_plane - far_plane);
        m[2][3] = -1.0f;
        m[3][2] = (near_plane * far_plane) / (near_plane - far_plane);
        return m;
    }
};

/// Orthographic projection parameters
struct OrthographicProjection {
    float left = -10.0f;
    float right = 10.0f;
    float bottom = -10.0f;
    float top = 10.0f;
    float near_plane = DEFAULT_NEAR;
    float far_plane = DEFAULT_FAR;

    /// Create symmetric orthographic projection
    [[nodiscard]] static OrthographicProjection symmetric(float width, float height, float near_p = DEFAULT_NEAR, float far_p = DEFAULT_FAR) {
        float half_w = width / 2.0f;
        float half_h = height / 2.0f;
        return OrthographicProjection{-half_w, half_w, -half_h, half_h, near_p, far_p};
    }

    /// Compute projection matrix (column-major, reverse-Z)
    [[nodiscard]] std::array<std::array<float, 4>, 4> matrix() const {
        std::array<std::array<float, 4>, 4> m = {};
        m[0][0] = 2.0f / (right - left);
        m[1][1] = 2.0f / (top - bottom);
        m[2][2] = 1.0f / (far_plane - near_plane);  // Reverse-Z
        m[3][0] = -(right + left) / (right - left);
        m[3][1] = -(top + bottom) / (top - bottom);
        m[3][2] = far_plane / (far_plane - near_plane);  // Reverse-Z
        m[3][3] = 1.0f;
        return m;
    }
};

// =============================================================================
// GpuCameraData (GPU-ready)
// =============================================================================

/// GPU camera data (256 bytes, aligned for uniform buffer)
struct alignas(16) GpuCameraData {
    std::array<std::array<float, 4>, 4> view_matrix;         // 64 bytes
    std::array<std::array<float, 4>, 4> projection_matrix;   // 64 bytes
    std::array<std::array<float, 4>, 4> view_proj_matrix;    // 64 bytes
    std::array<std::array<float, 4>, 4> inv_view_matrix;     // 64 bytes (for world-space reconstruction)

    /// Size in bytes
    static constexpr std::size_t SIZE = 256;

    /// Default constructor (identity matrices)
    GpuCameraData() {
        auto identity = []() -> std::array<std::array<float, 4>, 4> {
            return {{
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {0, 0, 0, 1}
            }};
        };
        view_matrix = identity();
        projection_matrix = identity();
        view_proj_matrix = identity();
        inv_view_matrix = identity();
    }
};

static_assert(sizeof(GpuCameraData) == 256, "GpuCameraData must be 256 bytes");

/// Extended GPU camera data with additional info (512 bytes)
struct alignas(16) GpuCameraDataExtended {
    std::array<std::array<float, 4>, 4> view_matrix;         // 64 bytes
    std::array<std::array<float, 4>, 4> projection_matrix;   // 64 bytes
    std::array<std::array<float, 4>, 4> view_proj_matrix;    // 64 bytes
    std::array<std::array<float, 4>, 4> inv_view_matrix;     // 64 bytes
    std::array<std::array<float, 4>, 4> inv_proj_matrix;     // 64 bytes
    std::array<std::array<float, 4>, 4> inv_view_proj_matrix;// 64 bytes

    std::array<float, 3> camera_position;                    // 12 bytes
    float near_plane;                                        // 4 bytes
    std::array<float, 3> camera_forward;                     // 12 bytes
    float far_plane;                                         // 4 bytes
    std::array<float, 2> viewport_size;                      // 8 bytes
    float fov_y;                                             // 4 bytes
    float aspect_ratio;                                      // 4 bytes

    // Padding to reach 512 bytes (80 bytes = 5 x 16)
    std::array<float, 4> _pad0 = {0, 0, 0, 0};               // 16 bytes
    std::array<float, 4> _pad1 = {0, 0, 0, 0};               // 16 bytes
    std::array<float, 4> _pad2 = {0, 0, 0, 0};               // 16 bytes
    std::array<float, 4> _pad3 = {0, 0, 0, 0};               // 16 bytes
    std::array<float, 4> _pad4 = {0, 0, 0, 0};               // 16 bytes

    /// Size in bytes
    static constexpr std::size_t SIZE = 512;
};

static_assert(sizeof(GpuCameraDataExtended) == 512, "GpuCameraDataExtended must be 512 bytes");

// =============================================================================
// Camera
// =============================================================================

/// Camera with position, rotation, and projection
class Camera {
public:
    /// Default constructor
    Camera() {
        update_matrices();
    }

    /// Construct with perspective projection
    explicit Camera(const PerspectiveProjection& proj)
        : m_projection_type(ProjectionType::Perspective)
        , m_perspective(proj) {
        update_matrices();
    }

    /// Construct with orthographic projection
    explicit Camera(const OrthographicProjection& proj)
        : m_projection_type(ProjectionType::Orthographic)
        , m_orthographic(proj) {
        update_matrices();
    }

    // -------------------------------------------------------------------------
    // Position/Rotation
    // -------------------------------------------------------------------------

    /// Set position
    void set_position(float x, float y, float z) {
        m_position = {x, y, z};
        m_dirty = true;
    }

    void set_position(const std::array<float, 3>& pos) {
        m_position = pos;
        m_dirty = true;
    }

    /// Get position
    [[nodiscard]] const std::array<float, 3>& position() const noexcept {
        return m_position;
    }

    /// Set Euler rotation (pitch, yaw, roll in radians)
    void set_rotation(float pitch, float yaw, float roll = 0.0f) {
        m_pitch = std::clamp(pitch, MIN_PITCH, MAX_PITCH);
        m_yaw = yaw;
        m_roll = roll;
        m_dirty = true;
    }

    /// Set pitch (radians)
    void set_pitch(float pitch) {
        m_pitch = std::clamp(pitch, MIN_PITCH, MAX_PITCH);
        m_dirty = true;
    }

    /// Set yaw (radians)
    void set_yaw(float yaw) {
        m_yaw = yaw;
        m_dirty = true;
    }

    /// Get pitch (radians)
    [[nodiscard]] float pitch() const noexcept { return m_pitch; }

    /// Get yaw (radians)
    [[nodiscard]] float yaw() const noexcept { return m_yaw; }

    /// Get roll (radians)
    [[nodiscard]] float roll() const noexcept { return m_roll; }

    /// Look at target position
    void look_at(const std::array<float, 3>& target, const std::array<float, 3>& up = {0, 1, 0}) {
        std::array<float, 3> dir = {
            target[0] - m_position[0],
            target[1] - m_position[1],
            target[2] - m_position[2]
        };

        float length = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        if (length > 1e-6f) {
            dir[0] /= length;
            dir[1] /= length;
            dir[2] /= length;
        }

        // Calculate pitch and yaw from direction
        m_pitch = std::asin(-dir[1]);
        m_yaw = std::atan2(dir[0], -dir[2]);
        m_roll = 0.0f;  // Simplified, ignores up vector for roll

        (void)up;  // TODO: Calculate roll from up vector
        m_dirty = true;
    }

    // -------------------------------------------------------------------------
    // Direction Vectors
    // -------------------------------------------------------------------------

    /// Get forward direction (-Z in camera space)
    [[nodiscard]] std::array<float, 3> forward() const {
        float cos_pitch = std::cos(m_pitch);
        return {
            std::sin(m_yaw) * cos_pitch,
            -std::sin(m_pitch),
            -std::cos(m_yaw) * cos_pitch
        };
    }

    /// Get right direction (+X in camera space)
    [[nodiscard]] std::array<float, 3> right() const {
        float cos_roll = std::cos(m_roll);
        float sin_roll = std::sin(m_roll);
        float cos_yaw = std::cos(m_yaw);
        float sin_yaw = std::sin(m_yaw);
        return {
            cos_yaw * cos_roll,
            sin_roll,
            sin_yaw * cos_roll
        };
    }

    /// Get up direction (+Y in camera space)
    [[nodiscard]] std::array<float, 3> up() const {
        auto fwd = forward();
        auto rgt = right();
        // Cross product: up = right × forward
        return {
            rgt[1] * fwd[2] - rgt[2] * fwd[1],
            rgt[2] * fwd[0] - rgt[0] * fwd[2],
            rgt[0] * fwd[1] - rgt[1] * fwd[0]
        };
    }

    // -------------------------------------------------------------------------
    // Projection
    // -------------------------------------------------------------------------

    /// Get projection type
    [[nodiscard]] ProjectionType projection_type() const noexcept {
        return m_projection_type;
    }

    /// Set perspective projection
    void set_perspective(const PerspectiveProjection& proj) {
        m_projection_type = ProjectionType::Perspective;
        m_perspective = proj;
        m_dirty = true;
    }

    /// Set orthographic projection
    void set_orthographic(const OrthographicProjection& proj) {
        m_projection_type = ProjectionType::Orthographic;
        m_orthographic = proj;
        m_dirty = true;
    }

    /// Get perspective projection (only valid if type is Perspective)
    [[nodiscard]] PerspectiveProjection& perspective() noexcept {
        return m_perspective;
    }

    [[nodiscard]] const PerspectiveProjection& perspective() const noexcept {
        return m_perspective;
    }

    /// Get orthographic projection (only valid if type is Orthographic)
    [[nodiscard]] OrthographicProjection& orthographic() noexcept {
        return m_orthographic;
    }

    [[nodiscard]] const OrthographicProjection& orthographic() const noexcept {
        return m_orthographic;
    }

    /// Set aspect ratio (for perspective)
    void set_aspect_ratio(float aspect) {
        m_perspective.aspect_ratio = aspect;
        m_dirty = true;
    }

    /// Set field of view (for perspective, radians)
    void set_fov(float fov) {
        m_perspective.fov_y = fov;
        m_dirty = true;
    }

    /// Set near/far planes
    void set_clip_planes(float near_p, float far_p) {
        m_perspective.near_plane = near_p;
        m_perspective.far_plane = far_p;
        m_orthographic.near_plane = near_p;
        m_orthographic.far_plane = far_p;
        m_dirty = true;
    }

    // -------------------------------------------------------------------------
    // Matrices
    // -------------------------------------------------------------------------

    /// Update matrices if dirty
    void update() {
        if (m_dirty) {
            update_matrices();
            m_dirty = false;
        }
    }

    /// Force matrix update
    void update_matrices() {
        compute_view_matrix();
        compute_projection_matrix();
        compute_view_proj_matrix();
        compute_inverse_matrices();
    }

    /// Get view matrix
    [[nodiscard]] const std::array<std::array<float, 4>, 4>& view_matrix() const noexcept {
        return m_view_matrix;
    }

    /// Get projection matrix
    [[nodiscard]] const std::array<std::array<float, 4>, 4>& projection_matrix() const noexcept {
        return m_projection_matrix;
    }

    /// Get view-projection matrix
    [[nodiscard]] const std::array<std::array<float, 4>, 4>& view_proj_matrix() const noexcept {
        return m_view_proj_matrix;
    }

    /// Get inverse view matrix
    [[nodiscard]] const std::array<std::array<float, 4>, 4>& inv_view_matrix() const noexcept {
        return m_inv_view_matrix;
    }

    /// Get inverse projection matrix
    [[nodiscard]] const std::array<std::array<float, 4>, 4>& inv_proj_matrix() const noexcept {
        return m_inv_proj_matrix;
    }

    // -------------------------------------------------------------------------
    // GPU Data
    // -------------------------------------------------------------------------

    /// Get GPU camera data
    [[nodiscard]] GpuCameraData gpu_data() const {
        GpuCameraData data;
        data.view_matrix = m_view_matrix;
        data.projection_matrix = m_projection_matrix;
        data.view_proj_matrix = m_view_proj_matrix;
        data.inv_view_matrix = m_inv_view_matrix;
        return data;
    }

    /// Get extended GPU camera data
    [[nodiscard]] GpuCameraDataExtended gpu_data_extended() const {
        GpuCameraDataExtended data;
        data.view_matrix = m_view_matrix;
        data.projection_matrix = m_projection_matrix;
        data.view_proj_matrix = m_view_proj_matrix;
        data.inv_view_matrix = m_inv_view_matrix;
        data.inv_proj_matrix = m_inv_proj_matrix;
        data.inv_view_proj_matrix = m_inv_view_proj_matrix;
        data.camera_position = m_position;
        data.near_plane = m_perspective.near_plane;
        data.camera_forward = forward();
        data.far_plane = m_perspective.far_plane;
        data.viewport_size = {
            m_perspective.aspect_ratio * 1000.0f,  // Approximate
            1000.0f
        };
        data.fov_y = m_perspective.fov_y;
        data.aspect_ratio = m_perspective.aspect_ratio;
        return data;
    }

    // -------------------------------------------------------------------------
    // Movement
    // -------------------------------------------------------------------------

    /// Move relative to camera orientation
    void move(float forward_amount, float right_amount, float up_amount) {
        auto fwd = forward();
        auto rgt = right();
        auto u = up();
        m_position[0] += fwd[0] * forward_amount + rgt[0] * right_amount + u[0] * up_amount;
        m_position[1] += fwd[1] * forward_amount + rgt[1] * right_amount + u[1] * up_amount;
        m_position[2] += fwd[2] * forward_amount + rgt[2] * right_amount + u[2] * up_amount;
        m_dirty = true;
    }

    /// Move in world space
    void translate(float dx, float dy, float dz) {
        m_position[0] += dx;
        m_position[1] += dy;
        m_position[2] += dz;
        m_dirty = true;
    }

    /// Rotate by delta angles (radians)
    void rotate(float delta_pitch, float delta_yaw, float delta_roll = 0.0f) {
        m_pitch = std::clamp(m_pitch + delta_pitch, MIN_PITCH, MAX_PITCH);
        m_yaw += delta_yaw;
        m_roll += delta_roll;
        m_dirty = true;
    }

private:
    void compute_view_matrix() {
        // Rotation matrix from Euler angles (YXZ order: yaw, pitch, roll)
        float cp = std::cos(m_pitch);
        float sp = std::sin(m_pitch);
        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);
        float cr = std::cos(m_roll);
        float sr = std::sin(m_roll);

        // Rotation matrix
        std::array<std::array<float, 3>, 3> r;
        r[0][0] = cy * cr + sy * sp * sr;
        r[0][1] = sr * cp;
        r[0][2] = -sy * cr + cy * sp * sr;
        r[1][0] = -cy * sr + sy * sp * cr;
        r[1][1] = cr * cp;
        r[1][2] = sy * sr + cy * sp * cr;
        r[2][0] = sy * cp;
        r[2][1] = -sp;
        r[2][2] = cy * cp;

        // View matrix = transpose(R) * translate(-position)
        m_view_matrix[0][0] = r[0][0];
        m_view_matrix[0][1] = r[1][0];
        m_view_matrix[0][2] = r[2][0];
        m_view_matrix[0][3] = 0.0f;

        m_view_matrix[1][0] = r[0][1];
        m_view_matrix[1][1] = r[1][1];
        m_view_matrix[1][2] = r[2][1];
        m_view_matrix[1][3] = 0.0f;

        m_view_matrix[2][0] = r[0][2];
        m_view_matrix[2][1] = r[1][2];
        m_view_matrix[2][2] = r[2][2];
        m_view_matrix[2][3] = 0.0f;

        // Translation: -dot(R_col, position)
        m_view_matrix[3][0] = -(r[0][0] * m_position[0] + r[0][1] * m_position[1] + r[0][2] * m_position[2]);
        m_view_matrix[3][1] = -(r[1][0] * m_position[0] + r[1][1] * m_position[1] + r[1][2] * m_position[2]);
        m_view_matrix[3][2] = -(r[2][0] * m_position[0] + r[2][1] * m_position[1] + r[2][2] * m_position[2]);
        m_view_matrix[3][3] = 1.0f;
    }

    void compute_projection_matrix() {
        if (m_projection_type == ProjectionType::Perspective) {
            m_projection_matrix = m_perspective.matrix();
        } else {
            m_projection_matrix = m_orthographic.matrix();
        }
    }

    void compute_view_proj_matrix() {
        // Matrix multiplication: view_proj = projection * view
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m_view_proj_matrix[i][j] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    m_view_proj_matrix[i][j] += m_projection_matrix[k][j] * m_view_matrix[i][k];
                }
            }
        }
    }

    void compute_inverse_matrices() {
        // Inverse view matrix (rotation transpose, negated translation)
        // Since view = transpose(R) * T(-pos), inv_view = T(pos) * R
        float cp = std::cos(m_pitch);
        float sp = std::sin(m_pitch);
        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);
        float cr = std::cos(m_roll);
        float sr = std::sin(m_roll);

        m_inv_view_matrix[0][0] = cy * cr + sy * sp * sr;
        m_inv_view_matrix[0][1] = -cy * sr + sy * sp * cr;
        m_inv_view_matrix[0][2] = sy * cp;
        m_inv_view_matrix[0][3] = 0.0f;

        m_inv_view_matrix[1][0] = sr * cp;
        m_inv_view_matrix[1][1] = cr * cp;
        m_inv_view_matrix[1][2] = -sp;
        m_inv_view_matrix[1][3] = 0.0f;

        m_inv_view_matrix[2][0] = -sy * cr + cy * sp * sr;
        m_inv_view_matrix[2][1] = sy * sr + cy * sp * cr;
        m_inv_view_matrix[2][2] = cy * cp;
        m_inv_view_matrix[2][3] = 0.0f;

        m_inv_view_matrix[3][0] = m_position[0];
        m_inv_view_matrix[3][1] = m_position[1];
        m_inv_view_matrix[3][2] = m_position[2];
        m_inv_view_matrix[3][3] = 1.0f;

        // Inverse projection (analytical for perspective)
        if (m_projection_type == ProjectionType::Perspective) {
            float tan_half_fov = std::tan(m_perspective.fov_y / 2.0f);
            float n = m_perspective.near_plane;
            float f = m_perspective.far_plane;

            m_inv_proj_matrix = {};
            m_inv_proj_matrix[0][0] = m_perspective.aspect_ratio * tan_half_fov;
            m_inv_proj_matrix[1][1] = tan_half_fov;
            m_inv_proj_matrix[2][3] = (f - n) / (n * f);
            m_inv_proj_matrix[3][2] = -1.0f;
            m_inv_proj_matrix[3][3] = 1.0f / n;
        } else {
            // Orthographic inverse
            float w = m_orthographic.right - m_orthographic.left;
            float h = m_orthographic.top - m_orthographic.bottom;
            float d = m_orthographic.far_plane - m_orthographic.near_plane;

            m_inv_proj_matrix = {};
            m_inv_proj_matrix[0][0] = w / 2.0f;
            m_inv_proj_matrix[1][1] = h / 2.0f;
            m_inv_proj_matrix[2][2] = d;
            m_inv_proj_matrix[3][0] = (m_orthographic.right + m_orthographic.left) / 2.0f;
            m_inv_proj_matrix[3][1] = (m_orthographic.top + m_orthographic.bottom) / 2.0f;
            m_inv_proj_matrix[3][2] = -m_orthographic.far_plane;
            m_inv_proj_matrix[3][3] = 1.0f;
        }

        // Inverse view-proj = inv_view * inv_proj
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m_inv_view_proj_matrix[i][j] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    m_inv_view_proj_matrix[i][j] += m_inv_view_matrix[k][j] * m_inv_proj_matrix[i][k];
                }
            }
        }
    }

private:
    // Position and rotation
    std::array<float, 3> m_position = {0, 0, 0};
    float m_pitch = 0.0f;  // X rotation (look up/down)
    float m_yaw = 0.0f;    // Y rotation (look left/right)
    float m_roll = 0.0f;   // Z rotation (tilt)

    // Projection
    ProjectionType m_projection_type = ProjectionType::Perspective;
    PerspectiveProjection m_perspective;
    OrthographicProjection m_orthographic;

    // Cached matrices
    std::array<std::array<float, 4>, 4> m_view_matrix = {};
    std::array<std::array<float, 4>, 4> m_projection_matrix = {};
    std::array<std::array<float, 4>, 4> m_view_proj_matrix = {};
    std::array<std::array<float, 4>, 4> m_inv_view_matrix = {};
    std::array<std::array<float, 4>, 4> m_inv_proj_matrix = {};
    std::array<std::array<float, 4>, 4> m_inv_view_proj_matrix = {};

    bool m_dirty = true;
};

// =============================================================================
// CameraMode
// =============================================================================

/// Camera controller mode
enum class CameraMode : std::uint8_t {
    /// First-person shooter style (WASD + mouse look)
    Fps = 0,
    /// Orbit around target point
    Orbit = 1,
    /// Free fly mode (6DOF)
    Fly = 2,
};

// =============================================================================
// CameraInput
// =============================================================================

/// Input state for camera controller
struct CameraInput {
    // Movement (normalized -1 to 1)
    float forward = 0.0f;     // W/S
    float right = 0.0f;       // A/D
    float up = 0.0f;          // Space/Ctrl

    // Mouse movement (pixels)
    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;

    // Mouse scroll (lines)
    float scroll = 0.0f;

    // Modifiers
    bool shift = false;       // Sprint/fast mode
    bool alt = false;         // Alternative behavior

    // Delta time
    float delta_time = 0.016f;  // ~60fps default

    /// Reset input state
    void reset() {
        forward = right = up = 0.0f;
        mouse_dx = mouse_dy = 0.0f;
        scroll = 0.0f;
        shift = alt = false;
    }
};

// =============================================================================
// CameraControllerSettings
// =============================================================================

/// Settings for camera controller
struct CameraControllerSettings {
    // Movement speeds
    float move_speed = 5.0f;           // Units per second
    float sprint_multiplier = 2.5f;    // When shift is held
    float slow_multiplier = 0.2f;      // When alt is held

    // Mouse sensitivity
    float mouse_sensitivity = 0.002f;  // Radians per pixel
    float scroll_sensitivity = 1.0f;   // Units per scroll line

    // Orbit mode
    float orbit_distance = 10.0f;      // Distance from target
    float min_orbit_distance = 0.5f;
    float max_orbit_distance = 100.0f;

    // Smoothing (0 = instant, 1 = never reaches target)
    float position_smoothing = 0.0f;
    float rotation_smoothing = 0.0f;

    // Constraints
    bool constrain_pitch = true;
    float min_pitch = MIN_PITCH;
    float max_pitch = MAX_PITCH;

    /// Create default FPS settings
    [[nodiscard]] static CameraControllerSettings fps() {
        return CameraControllerSettings{};
    }

    /// Create orbit settings
    [[nodiscard]] static CameraControllerSettings orbit() {
        CameraControllerSettings s;
        s.move_speed = 0.0f;  // No direct movement in orbit mode
        s.orbit_distance = 10.0f;
        return s;
    }

    /// Create fly settings (6DOF)
    [[nodiscard]] static CameraControllerSettings fly() {
        CameraControllerSettings s;
        s.move_speed = 10.0f;
        s.constrain_pitch = false;
        return s;
    }
};

// =============================================================================
// CameraController
// =============================================================================

/// Camera controller with multiple modes
class CameraController {
public:
    /// Default constructor
    CameraController() = default;

    /// Construct with camera reference
    explicit CameraController(Camera* camera)
        : m_camera(camera) {}

    /// Set camera
    void set_camera(Camera* camera) {
        m_camera = camera;
    }

    /// Get camera
    [[nodiscard]] Camera* camera() noexcept { return m_camera; }
    [[nodiscard]] const Camera* camera() const noexcept { return m_camera; }

    /// Set mode
    void set_mode(CameraMode mode) {
        m_mode = mode;
    }

    /// Get mode
    [[nodiscard]] CameraMode mode() const noexcept { return m_mode; }

    /// Get settings
    [[nodiscard]] CameraControllerSettings& settings() noexcept { return m_settings; }
    [[nodiscard]] const CameraControllerSettings& settings() const noexcept { return m_settings; }

    /// Set orbit target
    void set_orbit_target(const std::array<float, 3>& target) {
        m_orbit_target = target;
    }

    /// Get orbit target
    [[nodiscard]] const std::array<float, 3>& orbit_target() const noexcept {
        return m_orbit_target;
    }

    /// Process input and update camera
    void update(const CameraInput& input) {
        if (!m_camera) return;

        switch (m_mode) {
            case CameraMode::Fps:
                update_fps(input);
                break;
            case CameraMode::Orbit:
                update_orbit(input);
                break;
            case CameraMode::Fly:
                update_fly(input);
                break;
        }

        m_camera->update();
    }

private:
    void update_fps(const CameraInput& input) {
        // Calculate speed multiplier
        float speed = m_settings.move_speed;
        if (input.shift) speed *= m_settings.sprint_multiplier;
        if (input.alt) speed *= m_settings.slow_multiplier;

        // Move relative to camera orientation (but Y-locked for up/down)
        float move_forward = input.forward * speed * input.delta_time;
        float move_right = input.right * speed * input.delta_time;
        float move_up = input.up * speed * input.delta_time;

        // Get horizontal forward direction (ignore pitch for movement)
        float yaw = m_camera->yaw();
        std::array<float, 3> fwd = {std::sin(yaw), 0.0f, -std::cos(yaw)};
        std::array<float, 3> rgt = {std::cos(yaw), 0.0f, std::sin(yaw)};

        auto pos = m_camera->position();
        pos[0] += fwd[0] * move_forward + rgt[0] * move_right;
        pos[1] += move_up;  // World-space up
        pos[2] += fwd[2] * move_forward + rgt[2] * move_right;
        m_camera->set_position(pos);

        // Rotate based on mouse
        float dpitch = -input.mouse_dy * m_settings.mouse_sensitivity;
        float dyaw = -input.mouse_dx * m_settings.mouse_sensitivity;
        m_camera->rotate(dpitch, dyaw);
    }

    void update_orbit(const CameraInput& input) {
        // Rotate around target
        float dpitch = -input.mouse_dy * m_settings.mouse_sensitivity;
        float dyaw = -input.mouse_dx * m_settings.mouse_sensitivity;

        m_orbit_pitch = std::clamp(m_orbit_pitch + dpitch, m_settings.min_pitch, m_settings.max_pitch);
        m_orbit_yaw += dyaw;

        // Zoom with scroll
        m_settings.orbit_distance = std::clamp(
            m_settings.orbit_distance - input.scroll * m_settings.scroll_sensitivity,
            m_settings.min_orbit_distance,
            m_settings.max_orbit_distance
        );

        // Calculate camera position
        float cos_pitch = std::cos(m_orbit_pitch);
        float sin_pitch = std::sin(m_orbit_pitch);
        float cos_yaw = std::cos(m_orbit_yaw);
        float sin_yaw = std::sin(m_orbit_yaw);

        std::array<float, 3> offset = {
            sin_yaw * cos_pitch * m_settings.orbit_distance,
            -sin_pitch * m_settings.orbit_distance,
            cos_yaw * cos_pitch * m_settings.orbit_distance
        };

        m_camera->set_position(
            m_orbit_target[0] + offset[0],
            m_orbit_target[1] + offset[1],
            m_orbit_target[2] + offset[2]
        );
        m_camera->look_at(m_orbit_target);
    }

    void update_fly(const CameraInput& input) {
        // Calculate speed multiplier
        float speed = m_settings.move_speed;
        if (input.shift) speed *= m_settings.sprint_multiplier;
        if (input.alt) speed *= m_settings.slow_multiplier;

        // Move relative to full camera orientation (6DOF)
        m_camera->move(
            input.forward * speed * input.delta_time,
            input.right * speed * input.delta_time,
            input.up * speed * input.delta_time
        );

        // Rotate based on mouse (including roll with alt+mouse_x)
        float dpitch = -input.mouse_dy * m_settings.mouse_sensitivity;
        float dyaw = -input.mouse_dx * m_settings.mouse_sensitivity;
        float droll = 0.0f;

        if (m_settings.constrain_pitch) {
            float new_pitch = m_camera->pitch() + dpitch;
            dpitch = std::clamp(new_pitch, m_settings.min_pitch, m_settings.max_pitch) - m_camera->pitch();
        }

        m_camera->rotate(dpitch, dyaw, droll);
    }

private:
    Camera* m_camera = nullptr;
    CameraMode m_mode = CameraMode::Fps;
    CameraControllerSettings m_settings;

    // Orbit state
    std::array<float, 3> m_orbit_target = {0, 0, 0};
    float m_orbit_pitch = 0.0f;
    float m_orbit_yaw = 0.0f;
};

// =============================================================================
// Frustum
// =============================================================================

/// Frustum plane
struct FrustumPlane {
    std::array<float, 3> normal = {0, 0, 0};
    float distance = 0.0f;

    /// Distance from point to plane (positive = in front)
    [[nodiscard]] float point_distance(const std::array<float, 3>& point) const {
        return normal[0] * point[0] + normal[1] * point[1] + normal[2] * point[2] + distance;
    }
};

/// Camera frustum for culling
class Frustum {
public:
    /// Plane indices
    enum PlaneIndex : std::size_t {
        Left = 0,
        Right = 1,
        Bottom = 2,
        Top = 3,
        Near = 4,
        Far = 5,
    };

    /// Default constructor
    Frustum() = default;

    /// Extract frustum from view-projection matrix
    void extract(const std::array<std::array<float, 4>, 4>& view_proj) {
        // Left: row4 + row1
        m_planes[Left].normal[0] = view_proj[0][3] + view_proj[0][0];
        m_planes[Left].normal[1] = view_proj[1][3] + view_proj[1][0];
        m_planes[Left].normal[2] = view_proj[2][3] + view_proj[2][0];
        m_planes[Left].distance = view_proj[3][3] + view_proj[3][0];

        // Right: row4 - row1
        m_planes[Right].normal[0] = view_proj[0][3] - view_proj[0][0];
        m_planes[Right].normal[1] = view_proj[1][3] - view_proj[1][0];
        m_planes[Right].normal[2] = view_proj[2][3] - view_proj[2][0];
        m_planes[Right].distance = view_proj[3][3] - view_proj[3][0];

        // Bottom: row4 + row2
        m_planes[Bottom].normal[0] = view_proj[0][3] + view_proj[0][1];
        m_planes[Bottom].normal[1] = view_proj[1][3] + view_proj[1][1];
        m_planes[Bottom].normal[2] = view_proj[2][3] + view_proj[2][1];
        m_planes[Bottom].distance = view_proj[3][3] + view_proj[3][1];

        // Top: row4 - row2
        m_planes[Top].normal[0] = view_proj[0][3] - view_proj[0][1];
        m_planes[Top].normal[1] = view_proj[1][3] - view_proj[1][1];
        m_planes[Top].normal[2] = view_proj[2][3] - view_proj[2][1];
        m_planes[Top].distance = view_proj[3][3] - view_proj[3][1];

        // Near: row4 + row3 (for reverse-Z: row3)
        m_planes[Near].normal[0] = view_proj[0][2];
        m_planes[Near].normal[1] = view_proj[1][2];
        m_planes[Near].normal[2] = view_proj[2][2];
        m_planes[Near].distance = view_proj[3][2];

        // Far: row4 - row3 (for reverse-Z: row4 - row3)
        m_planes[Far].normal[0] = view_proj[0][3] - view_proj[0][2];
        m_planes[Far].normal[1] = view_proj[1][3] - view_proj[1][2];
        m_planes[Far].normal[2] = view_proj[2][3] - view_proj[2][2];
        m_planes[Far].distance = view_proj[3][3] - view_proj[3][2];

        // Normalize planes
        for (auto& plane : m_planes) {
            float len = std::sqrt(
                plane.normal[0] * plane.normal[0] +
                plane.normal[1] * plane.normal[1] +
                plane.normal[2] * plane.normal[2]
            );
            if (len > 1e-6f) {
                plane.normal[0] /= len;
                plane.normal[1] /= len;
                plane.normal[2] /= len;
                plane.distance /= len;
            }
        }
    }

    /// Extract from camera
    void extract(const Camera& camera) {
        extract(camera.view_proj_matrix());
    }

    /// Test if sphere is inside frustum
    [[nodiscard]] bool contains_sphere(const std::array<float, 3>& center, float radius) const {
        for (const auto& plane : m_planes) {
            if (plane.point_distance(center) < -radius) {
                return false;
            }
        }
        return true;
    }

    /// Test if AABB is inside frustum
    [[nodiscard]] bool contains_aabb(const std::array<float, 3>& min, const std::array<float, 3>& max) const {
        for (const auto& plane : m_planes) {
            // Find the positive vertex (furthest along plane normal)
            std::array<float, 3> p;
            p[0] = (plane.normal[0] >= 0) ? max[0] : min[0];
            p[1] = (plane.normal[1] >= 0) ? max[1] : min[1];
            p[2] = (plane.normal[2] >= 0) ? max[2] : min[2];

            if (plane.point_distance(p) < 0) {
                return false;
            }
        }
        return true;
    }

    /// Get plane
    [[nodiscard]] const FrustumPlane& plane(PlaneIndex index) const noexcept {
        return m_planes[index];
    }

    /// Check if AABB intersects frustum (for glm-based code)
    [[nodiscard]] bool intersects_aabb(const AABB& aabb) const;

    /// Check if sphere intersects frustum
    [[nodiscard]] bool intersects_sphere(const BoundingSphere& sphere) const;

    /// Check if point is inside frustum
    [[nodiscard]] bool contains_point(const glm::vec3& point) const;

    /// Create from glm view-projection matrix
    [[nodiscard]] static Frustum from_view_projection(const glm::mat4& vp);

    /// Public planes array (for direct access in spatial.cpp)
    /// Note: Using glm::vec4 for compatibility with spatial.cpp
    std::array<glm::vec4, 6> planes;

private:
    std::array<FrustumPlane, 6> m_planes;
};

} // namespace void_render
