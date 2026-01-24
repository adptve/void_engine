/// @file solver.hpp
/// @brief Constraint solver for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "body.hpp"
#include "collision.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>
#include <void_engine/math/mat.hpp>

#include <vector>
#include <cmath>
#include <algorithm>

namespace void_physics {

// =============================================================================
// Solver Configuration
// =============================================================================

/// Solver configuration
struct SolverConfig {
    std::uint32_t velocity_iterations = 8;
    std::uint32_t position_iterations = 3;
    float baumgarte = 0.2f;              ///< Position correction factor
    float slop = 0.005f;                 ///< Allowed penetration
    float restitution_threshold = 1.0f;  ///< Min velocity for bounce
    bool warm_starting = true;
    float warm_start_factor = 0.8f;
};

// =============================================================================
// Velocity State (for solver)
// =============================================================================

/// Body velocity state used during solving
struct VelocityState {
    void_math::Vec3 v{0, 0, 0};    ///< Linear velocity
    void_math::Vec3 w{0, 0, 0};    ///< Angular velocity
};

/// Body position state used during solving
struct PositionState {
    void_math::Vec3 p{0, 0, 0};    ///< Position
    void_math::Quat q{};           ///< Rotation
};

// =============================================================================
// Contact Constraint
// =============================================================================

/// Contact constraint for collision response
struct ContactConstraint {
    BodyId body_a;
    BodyId body_b;
    int index_a = -1;  ///< Index into solver arrays
    int index_b = -1;

    /// Contact point data
    struct ContactPointData {
        void_math::Vec3 local_a;     ///< Local point on A
        void_math::Vec3 local_b;     ///< Local point on B
        void_math::Vec3 r_a;         ///< World offset from center A
        void_math::Vec3 r_b;         ///< World offset from center B
        float normal_mass = 0.0f;    ///< Effective mass for normal
        float tangent_mass_1 = 0.0f; ///< Effective mass for tangent 1
        float tangent_mass_2 = 0.0f; ///< Effective mass for tangent 2
        float velocity_bias = 0.0f;  ///< Restitution bias

        // Accumulated impulses (warm starting)
        float normal_impulse = 0.0f;
        float tangent_impulse_1 = 0.0f;
        float tangent_impulse_2 = 0.0f;
    };

    std::vector<ContactPointData> points;

    void_math::Vec3 normal{0, 1, 0};   ///< Contact normal (B to A)
    void_math::Vec3 tangent_1{1, 0, 0};
    void_math::Vec3 tangent_2{0, 0, 1};

    float friction = 0.5f;
    float restitution = 0.0f;

    float inv_mass_a = 0.0f;
    float inv_mass_b = 0.0f;
    void_math::Vec3 inv_inertia_a{0, 0, 0};
    void_math::Vec3 inv_inertia_b{0, 0, 0};
};

// =============================================================================
// Joint Constraint Base
// =============================================================================

/// Base class for joint constraints
class IJointConstraint {
public:
    virtual ~IJointConstraint() = default;

    [[nodiscard]] virtual JointType type() const noexcept = 0;
    [[nodiscard]] virtual JointId id() const noexcept = 0;
    [[nodiscard]] virtual BodyId body_a() const noexcept = 0;
    [[nodiscard]] virtual BodyId body_b() const noexcept = 0;

    /// Initialize constraint for solving
    virtual void initialize(
        const PositionState& pos_a, const PositionState& pos_b,
        const VelocityState& vel_a, const VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b,
        float dt) = 0;

    /// Apply warm starting impulses
    virtual void warm_start(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) = 0;

    /// Solve velocity constraints
    virtual void solve_velocity(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) = 0;

    /// Solve position constraints
    virtual bool solve_position(
        PositionState& pos_a, PositionState& pos_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) = 0;
};

// =============================================================================
// Fixed Joint Constraint
// =============================================================================

/// Fixed joint - maintains relative position and orientation
class FixedJointConstraint : public IJointConstraint {
public:
    FixedJointConstraint(JointId id, const JointConfig& config)
        : m_id(id)
        , m_body_a(config.body_a)
        , m_body_b(config.body_b)
        , m_local_anchor_a(config.anchor_a)
        , m_local_anchor_b(config.anchor_b)
    {}

    [[nodiscard]] JointType type() const noexcept override { return JointType::Fixed; }
    [[nodiscard]] JointId id() const noexcept override { return m_id; }
    [[nodiscard]] BodyId body_a() const noexcept override { return m_body_a; }
    [[nodiscard]] BodyId body_b() const noexcept override { return m_body_b; }

    void initialize(
        const PositionState& pos_a, const PositionState& pos_b,
        const VelocityState& /*vel_a*/, const VelocityState& /*vel_b*/,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b,
        float dt) override
    {
        m_r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        m_r_b = void_math::rotate(pos_b.q, m_local_anchor_b);

        // Compute effective mass for linear constraint
        void_math::Mat3 k = void_math::Mat3::identity();
        float total_inv_mass = inv_mass_a + inv_mass_b;
        k.m[0][0] = total_inv_mass;
        k.m[1][1] = total_inv_mass;
        k.m[2][2] = total_inv_mass;

        // Add angular terms
        auto skew_a = skew_symmetric(m_r_a);
        auto skew_b = skew_symmetric(m_r_b);
        auto inertia_a = void_math::Mat3::diagonal(inv_inertia_a);
        auto inertia_b = void_math::Mat3::diagonal(inv_inertia_b);

        k = k + skew_a * inertia_a * transpose(skew_a);
        k = k + skew_b * inertia_b * transpose(skew_b);

        m_linear_mass = inverse(k);
        m_dt = dt;
    }

    void warm_start(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        vel_a.v = vel_a.v - m_accumulated_linear * inv_mass_a;
        vel_b.v = vel_b.v + m_accumulated_linear * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, m_accumulated_linear) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, m_accumulated_linear) * inv_inertia_b;

        vel_a.w = vel_a.w - m_accumulated_angular * inv_inertia_a;
        vel_b.w = vel_b.w + m_accumulated_angular * inv_inertia_b;
    }

    void solve_velocity(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        // Linear constraint
        auto c_dot = vel_b.v + void_math::cross(vel_b.w, m_r_b)
                   - vel_a.v - void_math::cross(vel_a.w, m_r_a);
        auto impulse = m_linear_mass * (-c_dot);
        m_accumulated_linear = m_accumulated_linear + impulse;

        vel_a.v = vel_a.v - impulse * inv_mass_a;
        vel_b.v = vel_b.v + impulse * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, impulse) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, impulse) * inv_inertia_b;

        // Angular constraint (lock rotation)
        auto w_diff = vel_b.w - vel_a.w;
        auto inv_i = inv_inertia_a + inv_inertia_b;
        auto ang_impulse = -w_diff / (inv_i + void_math::Vec3{0.0001f, 0.0001f, 0.0001f});
        m_accumulated_angular = m_accumulated_angular + ang_impulse;

        vel_a.w = vel_a.w - ang_impulse * inv_inertia_a;
        vel_b.w = vel_b.w + ang_impulse * inv_inertia_b;
    }

    bool solve_position(
        PositionState& pos_a, PositionState& pos_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        auto r_b = void_math::rotate(pos_b.q, m_local_anchor_b);

        auto c = (pos_b.p + r_b) - (pos_a.p + r_a);
        float error = void_math::length(c);

        if (error < 0.005f) return true;

        float total_inv_mass = inv_mass_a + inv_mass_b;
        if (total_inv_mass < 0.0001f) return true;

        auto correction = c * (0.2f / total_inv_mass);
        pos_a.p = pos_a.p + correction * inv_mass_a;
        pos_b.p = pos_b.p - correction * inv_mass_b;

        return error < 0.01f;
    }

private:
    static void_math::Mat3 skew_symmetric(const void_math::Vec3& v) {
        return void_math::Mat3{
            0, -v.z, v.y,
            v.z, 0, -v.x,
            -v.y, v.x, 0
        };
    }

    static void_math::Mat3 transpose(const void_math::Mat3& m) {
        return void_math::Mat3{
            m.m[0][0], m.m[1][0], m.m[2][0],
            m.m[0][1], m.m[1][1], m.m[2][1],
            m.m[0][2], m.m[1][2], m.m[2][2]
        };
    }

    static void_math::Mat3 inverse(const void_math::Mat3& m) {
        float det = m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])
                  - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])
                  + m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
        if (std::abs(det) < 0.0001f) return void_math::Mat3::identity();
        float inv_det = 1.0f / det;

        return void_math::Mat3{
            (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * inv_det,
            (m.m[0][2] * m.m[2][1] - m.m[0][1] * m.m[2][2]) * inv_det,
            (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * inv_det,
            (m.m[1][2] * m.m[2][0] - m.m[1][0] * m.m[2][2]) * inv_det,
            (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * inv_det,
            (m.m[0][2] * m.m[1][0] - m.m[0][0] * m.m[1][2]) * inv_det,
            (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * inv_det,
            (m.m[0][1] * m.m[2][0] - m.m[0][0] * m.m[2][1]) * inv_det,
            (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * inv_det
        };
    }

    JointId m_id;
    BodyId m_body_a;
    BodyId m_body_b;
    void_math::Vec3 m_local_anchor_a;
    void_math::Vec3 m_local_anchor_b;
    void_math::Vec3 m_r_a;
    void_math::Vec3 m_r_b;
    void_math::Mat3 m_linear_mass;
    void_math::Vec3 m_accumulated_linear{0, 0, 0};
    void_math::Vec3 m_accumulated_angular{0, 0, 0};
    float m_dt = 0.0f;
};

// =============================================================================
// Distance Joint Constraint
// =============================================================================

/// Distance joint - maintains distance between anchor points
class DistanceJointConstraint : public IJointConstraint {
public:
    DistanceJointConstraint(JointId id, const DistanceJointConfig& config)
        : m_id(id)
        , m_body_a(config.body_a)
        , m_body_b(config.body_b)
        , m_local_anchor_a(config.anchor_a)
        , m_local_anchor_b(config.anchor_b)
        , m_min_distance(config.min_distance)
        , m_max_distance(config.max_distance)
        , m_spring_enabled(config.spring_enabled)
        , m_stiffness(config.spring_stiffness)
        , m_damping(config.spring_damping)
    {
        if (m_max_distance < m_min_distance) {
            m_max_distance = m_min_distance;
        }
    }

    [[nodiscard]] JointType type() const noexcept override { return JointType::Distance; }
    [[nodiscard]] JointId id() const noexcept override { return m_id; }
    [[nodiscard]] BodyId body_a() const noexcept override { return m_body_a; }
    [[nodiscard]] BodyId body_b() const noexcept override { return m_body_b; }

    void initialize(
        const PositionState& pos_a, const PositionState& pos_b,
        const VelocityState& vel_a, const VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b,
        float dt) override
    {
        m_r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        m_r_b = void_math::rotate(pos_b.q, m_local_anchor_b);

        auto world_a = pos_a.p + m_r_a;
        auto world_b = pos_b.p + m_r_b;
        m_u = world_b - world_a;
        m_current_length = void_math::length(m_u);

        if (m_current_length > 0.0001f) {
            m_u = m_u / m_current_length;
        } else {
            m_u = void_math::Vec3{1, 0, 0};
        }

        // Compute effective mass
        float cr_a = void_math::dot(void_math::cross(m_r_a, m_u), void_math::cross(m_r_a, m_u) * inv_inertia_a);
        float cr_b = void_math::dot(void_math::cross(m_r_b, m_u), void_math::cross(m_r_b, m_u) * inv_inertia_b);
        float inv_mass = inv_mass_a + inv_mass_b + cr_a + cr_b;
        m_mass = inv_mass > 0.0001f ? 1.0f / inv_mass : 0.0f;

        if (m_spring_enabled && m_stiffness > 0.0f) {
            float omega = std::sqrt(m_stiffness / m_mass);
            float d = 2.0f * m_mass * m_damping * omega;
            float k = m_stiffness;
            float gamma = dt * (d + dt * k);
            m_gamma = gamma > 0.0f ? 1.0f / gamma : 0.0f;
            m_bias = m_current_length * dt * k * m_gamma;
            m_mass = inv_mass + m_gamma;
            m_mass = m_mass > 0.0f ? 1.0f / m_mass : 0.0f;
        } else {
            m_gamma = 0.0f;
            m_bias = 0.0f;
        }

        m_dt = dt;
    }

    void warm_start(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto p = m_u * m_accumulated_impulse;
        vel_a.v = vel_a.v - p * inv_mass_a;
        vel_b.v = vel_b.v + p * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, p) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, p) * inv_inertia_b;
    }

    void solve_velocity(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto v_a = vel_a.v + void_math::cross(vel_a.w, m_r_a);
        auto v_b = vel_b.v + void_math::cross(vel_b.w, m_r_b);
        float c_dot = void_math::dot(m_u, v_b - v_a);

        float impulse = -m_mass * (c_dot + m_bias + m_gamma * m_accumulated_impulse);
        m_accumulated_impulse += impulse;

        auto p = m_u * impulse;
        vel_a.v = vel_a.v - p * inv_mass_a;
        vel_b.v = vel_b.v + p * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, p) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, p) * inv_inertia_b;
    }

    bool solve_position(
        PositionState& pos_a, PositionState& pos_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        if (m_spring_enabled) return true;

        auto r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        auto r_b = void_math::rotate(pos_b.q, m_local_anchor_b);
        auto u = (pos_b.p + r_b) - (pos_a.p + r_a);
        float length = void_math::length(u);

        if (length < 0.0001f) return true;
        u = u / length;

        float c = length - m_max_distance;
        if (c < 0.0f) {
            c = length - m_min_distance;
            if (c > 0.0f) return true;
        }
        c = std::clamp(c, -0.2f, 0.2f);

        float cr_a = void_math::dot(void_math::cross(r_a, u), void_math::cross(r_a, u) * inv_inertia_a);
        float cr_b = void_math::dot(void_math::cross(r_b, u), void_math::cross(r_b, u) * inv_inertia_b);
        float inv_mass = inv_mass_a + inv_mass_b + cr_a + cr_b;
        if (inv_mass < 0.0001f) return true;

        float impulse = -c / inv_mass;
        auto p = u * impulse;

        pos_a.p = pos_a.p - p * inv_mass_a;
        pos_b.p = pos_b.p + p * inv_mass_b;

        return std::abs(c) < 0.005f;
    }

private:
    JointId m_id;
    BodyId m_body_a;
    BodyId m_body_b;
    void_math::Vec3 m_local_anchor_a;
    void_math::Vec3 m_local_anchor_b;
    float m_min_distance;
    float m_max_distance;
    bool m_spring_enabled;
    float m_stiffness;
    float m_damping;

    void_math::Vec3 m_r_a;
    void_math::Vec3 m_r_b;
    void_math::Vec3 m_u;
    float m_current_length = 0.0f;
    float m_mass = 0.0f;
    float m_gamma = 0.0f;
    float m_bias = 0.0f;
    float m_accumulated_impulse = 0.0f;
    float m_dt = 0.0f;
};

// =============================================================================
// Spring Joint Constraint
// =============================================================================

/// Spring joint - spring force between anchor points
class SpringJointConstraint : public IJointConstraint {
public:
    SpringJointConstraint(JointId id, const SpringJointConfig& config)
        : m_id(id)
        , m_body_a(config.body_a)
        , m_body_b(config.body_b)
        , m_local_anchor_a(config.anchor_a)
        , m_local_anchor_b(config.anchor_b)
        , m_rest_length(config.rest_length)
        , m_stiffness(config.stiffness)
        , m_damping(config.damping)
        , m_min_length(config.min_length)
        , m_max_length(config.max_length)
    {}

    [[nodiscard]] JointType type() const noexcept override { return JointType::Spring; }
    [[nodiscard]] JointId id() const noexcept override { return m_id; }
    [[nodiscard]] BodyId body_a() const noexcept override { return m_body_a; }
    [[nodiscard]] BodyId body_b() const noexcept override { return m_body_b; }

    void initialize(
        const PositionState& pos_a, const PositionState& pos_b,
        const VelocityState& /*vel_a*/, const VelocityState& /*vel_b*/,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b,
        float dt) override
    {
        m_r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        m_r_b = void_math::rotate(pos_b.q, m_local_anchor_b);

        auto world_a = pos_a.p + m_r_a;
        auto world_b = pos_b.p + m_r_b;
        m_u = world_b - world_a;
        m_current_length = void_math::length(m_u);

        if (m_current_length > 0.0001f) {
            m_u = m_u / m_current_length;
        } else {
            m_u = void_math::Vec3{1, 0, 0};
        }

        float cr_a = void_math::dot(void_math::cross(m_r_a, m_u), void_math::cross(m_r_a, m_u) * inv_inertia_a);
        float cr_b = void_math::dot(void_math::cross(m_r_b, m_u), void_math::cross(m_r_b, m_u) * inv_inertia_b);
        float inv_mass = inv_mass_a + inv_mass_b + cr_a + cr_b;

        // Soft constraint with spring/damper
        float omega = std::sqrt(m_stiffness * inv_mass);
        float d = 2.0f * m_damping * omega / inv_mass;
        float k = m_stiffness;
        float gamma = dt * (d + dt * k);
        m_gamma = gamma > 0.0f ? 1.0f / gamma : 0.0f;

        float c = m_current_length - m_rest_length;
        c = std::clamp(c, m_min_length - m_rest_length, m_max_length - m_rest_length);
        m_bias = c * dt * k * m_gamma;

        m_mass = inv_mass + m_gamma;
        m_mass = m_mass > 0.0f ? 1.0f / m_mass : 0.0f;
        m_dt = dt;
    }

    void warm_start(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto p = m_u * m_accumulated_impulse;
        vel_a.v = vel_a.v - p * inv_mass_a;
        vel_b.v = vel_b.v + p * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, p) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, p) * inv_inertia_b;
    }

    void solve_velocity(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto v_a = vel_a.v + void_math::cross(vel_a.w, m_r_a);
        auto v_b = vel_b.v + void_math::cross(vel_b.w, m_r_b);
        float c_dot = void_math::dot(m_u, v_b - v_a);

        float impulse = -m_mass * (c_dot + m_bias + m_gamma * m_accumulated_impulse);
        m_accumulated_impulse += impulse;

        auto p = m_u * impulse;
        vel_a.v = vel_a.v - p * inv_mass_a;
        vel_b.v = vel_b.v + p * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, p) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, p) * inv_inertia_b;
    }

    bool solve_position(
        PositionState& /*pos_a*/, PositionState& /*pos_b*/,
        float /*inv_mass_a*/, float /*inv_mass_b*/,
        const void_math::Vec3& /*inv_inertia_a*/, const void_math::Vec3& /*inv_inertia_b*/) override
    {
        // Spring joints use soft constraints, no position correction needed
        return true;
    }

private:
    JointId m_id;
    BodyId m_body_a;
    BodyId m_body_b;
    void_math::Vec3 m_local_anchor_a;
    void_math::Vec3 m_local_anchor_b;
    float m_rest_length;
    float m_stiffness;
    float m_damping;
    float m_min_length;
    float m_max_length;

    void_math::Vec3 m_r_a;
    void_math::Vec3 m_r_b;
    void_math::Vec3 m_u;
    float m_current_length = 0.0f;
    float m_mass = 0.0f;
    float m_gamma = 0.0f;
    float m_bias = 0.0f;
    float m_accumulated_impulse = 0.0f;
    float m_dt = 0.0f;
};

// =============================================================================
// Ball Joint Constraint
// =============================================================================

/// Ball joint - free rotation at anchor point
class BallJointConstraint : public IJointConstraint {
public:
    BallJointConstraint(JointId id, const BallJointConfig& config)
        : m_id(id)
        , m_body_a(config.body_a)
        , m_body_b(config.body_b)
        , m_local_anchor_a(config.anchor_a)
        , m_local_anchor_b(config.anchor_b)
        , m_use_cone_limit(config.use_cone_limit)
        , m_cone_angle(config.cone_angle)
    {}

    [[nodiscard]] JointType type() const noexcept override { return JointType::Ball; }
    [[nodiscard]] JointId id() const noexcept override { return m_id; }
    [[nodiscard]] BodyId body_a() const noexcept override { return m_body_a; }
    [[nodiscard]] BodyId body_b() const noexcept override { return m_body_b; }

    void initialize(
        const PositionState& pos_a, const PositionState& pos_b,
        const VelocityState& /*vel_a*/, const VelocityState& /*vel_b*/,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b,
        float /*dt*/) override
    {
        m_r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        m_r_b = void_math::rotate(pos_b.q, m_local_anchor_b);

        // Compute 3x3 effective mass matrix
        float total_inv_mass = inv_mass_a + inv_mass_b;
        m_k.m[0][0] = total_inv_mass;
        m_k.m[1][1] = total_inv_mass;
        m_k.m[2][2] = total_inv_mass;
        m_k.m[0][1] = m_k.m[0][2] = m_k.m[1][0] = m_k.m[1][2] = m_k.m[2][0] = m_k.m[2][1] = 0;

        // Add angular contributions
        add_skew_inertia(m_k, m_r_a, inv_inertia_a);
        add_skew_inertia(m_k, m_r_b, inv_inertia_b);

        m_mass = inverse_3x3(m_k);
    }

    void warm_start(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        vel_a.v = vel_a.v - m_accumulated_impulse * inv_mass_a;
        vel_b.v = vel_b.v + m_accumulated_impulse * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, m_accumulated_impulse) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, m_accumulated_impulse) * inv_inertia_b;
    }

    void solve_velocity(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto v_a = vel_a.v + void_math::cross(vel_a.w, m_r_a);
        auto v_b = vel_b.v + void_math::cross(vel_b.w, m_r_b);
        auto c_dot = v_b - v_a;

        auto impulse = mul_3x3(m_mass, -c_dot);
        m_accumulated_impulse = m_accumulated_impulse + impulse;

        vel_a.v = vel_a.v - impulse * inv_mass_a;
        vel_b.v = vel_b.v + impulse * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, impulse) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, impulse) * inv_inertia_b;
    }

    bool solve_position(
        PositionState& pos_a, PositionState& pos_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        auto r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        auto r_b = void_math::rotate(pos_b.q, m_local_anchor_b);

        auto c = (pos_b.p + r_b) - (pos_a.p + r_a);
        float error = void_math::length(c);

        if (error < 0.005f) return true;

        // Recompute mass matrix
        void_math::Mat3 k;
        float total_inv_mass = inv_mass_a + inv_mass_b;
        k.m[0][0] = total_inv_mass; k.m[0][1] = 0; k.m[0][2] = 0;
        k.m[1][0] = 0; k.m[1][1] = total_inv_mass; k.m[1][2] = 0;
        k.m[2][0] = 0; k.m[2][1] = 0; k.m[2][2] = total_inv_mass;

        add_skew_inertia(k, r_a, inv_inertia_a);
        add_skew_inertia(k, r_b, inv_inertia_b);

        auto mass = inverse_3x3(k);
        auto impulse = mul_3x3(mass, -c * 0.2f);

        pos_a.p = pos_a.p - impulse * inv_mass_a;
        pos_b.p = pos_b.p + impulse * inv_mass_b;

        // Apply angular correction
        auto da = void_math::cross(r_a, impulse) * inv_inertia_a;
        auto db = void_math::cross(r_b, impulse) * inv_inertia_b;
        pos_a.q = void_math::normalize(apply_angular_impulse(pos_a.q, -da * 0.5f));
        pos_b.q = void_math::normalize(apply_angular_impulse(pos_b.q, db * 0.5f));

        return error < 0.01f;
    }

private:
    static void add_skew_inertia(void_math::Mat3& k, const void_math::Vec3& r, const void_math::Vec3& inv_i) {
        // K += skew(r) * diag(inv_i) * skew(r)^T
        float rx2 = r.x * r.x, ry2 = r.y * r.y, rz2 = r.z * r.z;
        k.m[0][0] += inv_i.y * rz2 + inv_i.z * ry2;
        k.m[1][1] += inv_i.x * rz2 + inv_i.z * rx2;
        k.m[2][2] += inv_i.x * ry2 + inv_i.y * rx2;

        float t = -inv_i.z * r.x * r.y;
        k.m[0][1] += t; k.m[1][0] += t;
        t = -inv_i.y * r.x * r.z;
        k.m[0][2] += t; k.m[2][0] += t;
        t = -inv_i.x * r.y * r.z;
        k.m[1][2] += t; k.m[2][1] += t;
    }

    static void_math::Mat3 inverse_3x3(const void_math::Mat3& m) {
        float det = m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])
                  - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])
                  + m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
        if (std::abs(det) < 0.0001f) return void_math::Mat3::identity();
        float inv_det = 1.0f / det;

        void_math::Mat3 result;
        result.m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * inv_det;
        result.m[0][1] = (m.m[0][2] * m.m[2][1] - m.m[0][1] * m.m[2][2]) * inv_det;
        result.m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * inv_det;
        result.m[1][0] = (m.m[1][2] * m.m[2][0] - m.m[1][0] * m.m[2][2]) * inv_det;
        result.m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * inv_det;
        result.m[1][2] = (m.m[0][2] * m.m[1][0] - m.m[0][0] * m.m[1][2]) * inv_det;
        result.m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * inv_det;
        result.m[2][1] = (m.m[0][1] * m.m[2][0] - m.m[0][0] * m.m[2][1]) * inv_det;
        result.m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * inv_det;
        return result;
    }

    static void_math::Vec3 mul_3x3(const void_math::Mat3& m, const void_math::Vec3& v) {
        return void_math::Vec3{
            m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
            m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
            m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z
        };
    }

    static void_math::Quat apply_angular_impulse(const void_math::Quat& q, const void_math::Vec3& impulse) {
        void_math::Quat dq{impulse.x, impulse.y, impulse.z, 0};
        dq = void_math::Quat{
            dq.x * q.w + dq.w * q.x + dq.y * q.z - dq.z * q.y,
            dq.y * q.w + dq.w * q.y + dq.z * q.x - dq.x * q.z,
            dq.z * q.w + dq.w * q.z + dq.x * q.y - dq.y * q.x,
            dq.w * q.w - dq.x * q.x - dq.y * q.y - dq.z * q.z
        };
        return void_math::Quat{
            q.x + 0.5f * dq.x,
            q.y + 0.5f * dq.y,
            q.z + 0.5f * dq.z,
            q.w + 0.5f * dq.w
        };
    }

    JointId m_id;
    BodyId m_body_a;
    BodyId m_body_b;
    void_math::Vec3 m_local_anchor_a;
    void_math::Vec3 m_local_anchor_b;
    bool m_use_cone_limit;
    float m_cone_angle;

    void_math::Vec3 m_r_a;
    void_math::Vec3 m_r_b;
    void_math::Mat3 m_k;
    void_math::Mat3 m_mass;
    void_math::Vec3 m_accumulated_impulse{0, 0, 0};
};

// =============================================================================
// Hinge Joint Constraint
// =============================================================================

/// Hinge joint - rotation around single axis
class HingeJointConstraint : public IJointConstraint {
public:
    HingeJointConstraint(JointId id, const HingeJointConfig& config)
        : m_id(id)
        , m_body_a(config.body_a)
        , m_body_b(config.body_b)
        , m_local_anchor_a(config.anchor_a)
        , m_local_anchor_b(config.anchor_b)
        , m_local_axis(void_math::normalize(config.axis))
        , m_use_limits(config.use_limits)
        , m_lower_limit(config.lower_limit)
        , m_upper_limit(config.upper_limit)
        , m_use_motor(config.use_motor)
        , m_motor_speed(config.motor_speed)
        , m_max_motor_torque(config.max_motor_torque)
    {}

    [[nodiscard]] JointType type() const noexcept override { return JointType::Hinge; }
    [[nodiscard]] JointId id() const noexcept override { return m_id; }
    [[nodiscard]] BodyId body_a() const noexcept override { return m_body_a; }
    [[nodiscard]] BodyId body_b() const noexcept override { return m_body_b; }

    void initialize(
        const PositionState& pos_a, const PositionState& pos_b,
        const VelocityState& /*vel_a*/, const VelocityState& /*vel_b*/,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b,
        float /*dt*/) override
    {
        m_r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        m_r_b = void_math::rotate(pos_b.q, m_local_anchor_b);
        m_axis_a = void_math::rotate(pos_a.q, m_local_axis);
        m_axis_b = void_math::rotate(pos_b.q, m_local_axis);

        // Build perpendicular axes for angular constraints
        build_perpendicular_axes(m_axis_a, m_perp1, m_perp2);

        // Linear constraint mass (same as ball joint)
        float total_inv_mass = inv_mass_a + inv_mass_b;
        m_linear_k.m[0][0] = total_inv_mass; m_linear_k.m[0][1] = 0; m_linear_k.m[0][2] = 0;
        m_linear_k.m[1][0] = 0; m_linear_k.m[1][1] = total_inv_mass; m_linear_k.m[1][2] = 0;
        m_linear_k.m[2][0] = 0; m_linear_k.m[2][1] = 0; m_linear_k.m[2][2] = total_inv_mass;

        add_skew_inertia(m_linear_k, m_r_a, inv_inertia_a);
        add_skew_inertia(m_linear_k, m_r_b, inv_inertia_b);
        m_linear_mass = inverse_3x3(m_linear_k);

        // Angular constraint mass (2 DOF locked)
        m_angular_mass_1 = void_math::dot(m_perp1, m_perp1 * (inv_inertia_a + inv_inertia_b));
        m_angular_mass_2 = void_math::dot(m_perp2, m_perp2 * (inv_inertia_a + inv_inertia_b));
        if (m_angular_mass_1 > 0.0001f) m_angular_mass_1 = 1.0f / m_angular_mass_1;
        if (m_angular_mass_2 > 0.0001f) m_angular_mass_2 = 1.0f / m_angular_mass_2;

        // Motor mass
        m_motor_mass = void_math::dot(m_axis_a, m_axis_a * (inv_inertia_a + inv_inertia_b));
        if (m_motor_mass > 0.0001f) m_motor_mass = 1.0f / m_motor_mass;
    }

    void warm_start(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        // Linear
        vel_a.v = vel_a.v - m_accumulated_linear * inv_mass_a;
        vel_b.v = vel_b.v + m_accumulated_linear * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, m_accumulated_linear) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, m_accumulated_linear) * inv_inertia_b;

        // Angular (2 DOF)
        auto ang = m_perp1 * m_accumulated_angular_1 + m_perp2 * m_accumulated_angular_2;
        vel_a.w = vel_a.w - ang * inv_inertia_a;
        vel_b.w = vel_b.w + ang * inv_inertia_b;

        // Motor
        vel_a.w = vel_a.w - m_axis_a * m_accumulated_motor * inv_inertia_a;
        vel_b.w = vel_b.w + m_axis_a * m_accumulated_motor * inv_inertia_b;
    }

    void solve_velocity(
        VelocityState& vel_a, VelocityState& vel_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        // Linear constraint
        auto v_a = vel_a.v + void_math::cross(vel_a.w, m_r_a);
        auto v_b = vel_b.v + void_math::cross(vel_b.w, m_r_b);
        auto c_dot = v_b - v_a;
        auto linear_impulse = mul_3x3(m_linear_mass, -c_dot);
        m_accumulated_linear = m_accumulated_linear + linear_impulse;

        vel_a.v = vel_a.v - linear_impulse * inv_mass_a;
        vel_b.v = vel_b.v + linear_impulse * inv_mass_b;
        vel_a.w = vel_a.w - void_math::cross(m_r_a, linear_impulse) * inv_inertia_a;
        vel_b.w = vel_b.w + void_math::cross(m_r_b, linear_impulse) * inv_inertia_b;

        // Angular constraint (lock 2 axes)
        auto w_diff = vel_b.w - vel_a.w;

        float c_dot_1 = void_math::dot(m_perp1, w_diff);
        float impulse_1 = -m_angular_mass_1 * c_dot_1;
        m_accumulated_angular_1 += impulse_1;

        float c_dot_2 = void_math::dot(m_perp2, w_diff);
        float impulse_2 = -m_angular_mass_2 * c_dot_2;
        m_accumulated_angular_2 += impulse_2;

        auto ang_impulse = m_perp1 * impulse_1 + m_perp2 * impulse_2;
        vel_a.w = vel_a.w - ang_impulse * inv_inertia_a;
        vel_b.w = vel_b.w + ang_impulse * inv_inertia_b;

        // Motor
        if (m_use_motor) {
            w_diff = vel_b.w - vel_a.w;
            float w_axis = void_math::dot(m_axis_a, w_diff);
            float motor_impulse = m_motor_mass * (m_motor_speed - w_axis);

            float old = m_accumulated_motor;
            m_accumulated_motor = std::clamp(m_accumulated_motor + motor_impulse, -m_max_motor_torque, m_max_motor_torque);
            motor_impulse = m_accumulated_motor - old;

            vel_a.w = vel_a.w - m_axis_a * motor_impulse * inv_inertia_a;
            vel_b.w = vel_b.w + m_axis_a * motor_impulse * inv_inertia_b;
        }
    }

    bool solve_position(
        PositionState& pos_a, PositionState& pos_b,
        float inv_mass_a, float inv_mass_b,
        const void_math::Vec3& inv_inertia_a, const void_math::Vec3& inv_inertia_b) override
    {
        // Linear position correction (same as ball joint)
        auto r_a = void_math::rotate(pos_a.q, m_local_anchor_a);
        auto r_b = void_math::rotate(pos_b.q, m_local_anchor_b);
        auto c = (pos_b.p + r_b) - (pos_a.p + r_a);
        float linear_error = void_math::length(c);

        if (linear_error > 0.005f) {
            void_math::Mat3 k;
            float total_inv_mass = inv_mass_a + inv_mass_b;
            k.m[0][0] = total_inv_mass; k.m[0][1] = 0; k.m[0][2] = 0;
            k.m[1][0] = 0; k.m[1][1] = total_inv_mass; k.m[1][2] = 0;
            k.m[2][0] = 0; k.m[2][1] = 0; k.m[2][2] = total_inv_mass;
            add_skew_inertia(k, r_a, inv_inertia_a);
            add_skew_inertia(k, r_b, inv_inertia_b);

            auto mass = inverse_3x3(k);
            auto impulse = mul_3x3(mass, -c * 0.2f);

            pos_a.p = pos_a.p - impulse * inv_mass_a;
            pos_b.p = pos_b.p + impulse * inv_mass_b;
        }

        return linear_error < 0.01f;
    }

private:
    static void build_perpendicular_axes(const void_math::Vec3& axis, void_math::Vec3& perp1, void_math::Vec3& perp2) {
        if (std::abs(axis.x) > 0.9f) {
            perp1 = void_math::normalize(void_math::cross(axis, void_math::Vec3{0, 1, 0}));
        } else {
            perp1 = void_math::normalize(void_math::cross(axis, void_math::Vec3{1, 0, 0}));
        }
        perp2 = void_math::cross(axis, perp1);
    }

    static void add_skew_inertia(void_math::Mat3& k, const void_math::Vec3& r, const void_math::Vec3& inv_i) {
        float rx2 = r.x * r.x, ry2 = r.y * r.y, rz2 = r.z * r.z;
        k.m[0][0] += inv_i.y * rz2 + inv_i.z * ry2;
        k.m[1][1] += inv_i.x * rz2 + inv_i.z * rx2;
        k.m[2][2] += inv_i.x * ry2 + inv_i.y * rx2;
        float t = -inv_i.z * r.x * r.y;
        k.m[0][1] += t; k.m[1][0] += t;
        t = -inv_i.y * r.x * r.z;
        k.m[0][2] += t; k.m[2][0] += t;
        t = -inv_i.x * r.y * r.z;
        k.m[1][2] += t; k.m[2][1] += t;
    }

    static void_math::Mat3 inverse_3x3(const void_math::Mat3& m) {
        float det = m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])
                  - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])
                  + m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
        if (std::abs(det) < 0.0001f) return void_math::Mat3::identity();
        float inv_det = 1.0f / det;
        void_math::Mat3 result;
        result.m[0][0] = (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) * inv_det;
        result.m[0][1] = (m.m[0][2] * m.m[2][1] - m.m[0][1] * m.m[2][2]) * inv_det;
        result.m[0][2] = (m.m[0][1] * m.m[1][2] - m.m[0][2] * m.m[1][1]) * inv_det;
        result.m[1][0] = (m.m[1][2] * m.m[2][0] - m.m[1][0] * m.m[2][2]) * inv_det;
        result.m[1][1] = (m.m[0][0] * m.m[2][2] - m.m[0][2] * m.m[2][0]) * inv_det;
        result.m[1][2] = (m.m[0][2] * m.m[1][0] - m.m[0][0] * m.m[1][2]) * inv_det;
        result.m[2][0] = (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]) * inv_det;
        result.m[2][1] = (m.m[0][1] * m.m[2][0] - m.m[0][0] * m.m[2][1]) * inv_det;
        result.m[2][2] = (m.m[0][0] * m.m[1][1] - m.m[0][1] * m.m[1][0]) * inv_det;
        return result;
    }

    static void_math::Vec3 mul_3x3(const void_math::Mat3& m, const void_math::Vec3& v) {
        return void_math::Vec3{
            m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
            m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
            m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z
        };
    }

    JointId m_id;
    BodyId m_body_a;
    BodyId m_body_b;
    void_math::Vec3 m_local_anchor_a;
    void_math::Vec3 m_local_anchor_b;
    void_math::Vec3 m_local_axis;
    bool m_use_limits;
    float m_lower_limit;
    float m_upper_limit;
    bool m_use_motor;
    float m_motor_speed;
    float m_max_motor_torque;

    void_math::Vec3 m_r_a;
    void_math::Vec3 m_r_b;
    void_math::Vec3 m_axis_a;
    void_math::Vec3 m_axis_b;
    void_math::Vec3 m_perp1;
    void_math::Vec3 m_perp2;
    void_math::Mat3 m_linear_k;
    void_math::Mat3 m_linear_mass;
    float m_angular_mass_1 = 0.0f;
    float m_angular_mass_2 = 0.0f;
    float m_motor_mass = 0.0f;

    void_math::Vec3 m_accumulated_linear{0, 0, 0};
    float m_accumulated_angular_1 = 0.0f;
    float m_accumulated_angular_2 = 0.0f;
    float m_accumulated_motor = 0.0f;
};

// =============================================================================
// Contact Solver
// =============================================================================

/// Solves contact constraints using sequential impulse
class ContactSolver {
public:
    ContactSolver() = default;

    /// Initialize contact constraints
    void initialize(
        std::vector<ContactConstraint>& contacts,
        std::vector<VelocityState>& velocities,
        const std::vector<PositionState>& positions,
        const SolverConfig& config,
        float dt)
    {
        m_config = config;

        for (auto& contact : contacts) {
            for (auto& cp : contact.points) {
                // Compute effective masses
                auto r_a_cross_n = void_math::cross(cp.r_a, contact.normal);
                auto r_b_cross_n = void_math::cross(cp.r_b, contact.normal);

                float k_normal = contact.inv_mass_a + contact.inv_mass_b
                    + void_math::dot(r_a_cross_n * contact.inv_inertia_a, r_a_cross_n)
                    + void_math::dot(r_b_cross_n * contact.inv_inertia_b, r_b_cross_n);
                cp.normal_mass = k_normal > 0.0001f ? 1.0f / k_normal : 0.0f;

                // Tangent masses
                auto r_a_cross_t1 = void_math::cross(cp.r_a, contact.tangent_1);
                auto r_b_cross_t1 = void_math::cross(cp.r_b, contact.tangent_1);
                float k_tangent_1 = contact.inv_mass_a + contact.inv_mass_b
                    + void_math::dot(r_a_cross_t1 * contact.inv_inertia_a, r_a_cross_t1)
                    + void_math::dot(r_b_cross_t1 * contact.inv_inertia_b, r_b_cross_t1);
                cp.tangent_mass_1 = k_tangent_1 > 0.0001f ? 1.0f / k_tangent_1 : 0.0f;

                auto r_a_cross_t2 = void_math::cross(cp.r_a, contact.tangent_2);
                auto r_b_cross_t2 = void_math::cross(cp.r_b, contact.tangent_2);
                float k_tangent_2 = contact.inv_mass_a + contact.inv_mass_b
                    + void_math::dot(r_a_cross_t2 * contact.inv_inertia_a, r_a_cross_t2)
                    + void_math::dot(r_b_cross_t2 * contact.inv_inertia_b, r_b_cross_t2);
                cp.tangent_mass_2 = k_tangent_2 > 0.0001f ? 1.0f / k_tangent_2 : 0.0f;

                // Restitution bias
                if (contact.index_a >= 0 && contact.index_b >= 0) {
                    auto& vel_a = velocities[static_cast<size_t>(contact.index_a)];
                    auto& vel_b = velocities[static_cast<size_t>(contact.index_b)];
                    auto v_a = vel_a.v + void_math::cross(vel_a.w, cp.r_a);
                    auto v_b = vel_b.v + void_math::cross(vel_b.w, cp.r_b);
                    float v_rel = void_math::dot(contact.normal, v_b - v_a);

                    if (v_rel < -config.restitution_threshold) {
                        cp.velocity_bias = -contact.restitution * v_rel;
                    }
                }
            }
        }
    }

    /// Apply warm starting
    void warm_start(
        std::vector<ContactConstraint>& contacts,
        std::vector<VelocityState>& velocities)
    {
        if (!m_config.warm_starting) return;

        for (auto& contact : contacts) {
            if (contact.index_a < 0 || contact.index_b < 0) continue;

            auto& vel_a = velocities[static_cast<size_t>(contact.index_a)];
            auto& vel_b = velocities[static_cast<size_t>(contact.index_b)];

            for (auto& cp : contact.points) {
                auto p = contact.normal * cp.normal_impulse
                       + contact.tangent_1 * cp.tangent_impulse_1
                       + contact.tangent_2 * cp.tangent_impulse_2;
                p = p * m_config.warm_start_factor;

                vel_a.v = vel_a.v - p * contact.inv_mass_a;
                vel_a.w = vel_a.w - void_math::cross(cp.r_a, p) * contact.inv_inertia_a;
                vel_b.v = vel_b.v + p * contact.inv_mass_b;
                vel_b.w = vel_b.w + void_math::cross(cp.r_b, p) * contact.inv_inertia_b;
            }
        }
    }

    /// Solve velocity constraints
    void solve_velocity(
        std::vector<ContactConstraint>& contacts,
        std::vector<VelocityState>& velocities)
    {
        for (auto& contact : contacts) {
            if (contact.index_a < 0 || contact.index_b < 0) continue;

            auto& vel_a = velocities[static_cast<size_t>(contact.index_a)];
            auto& vel_b = velocities[static_cast<size_t>(contact.index_b)];

            for (auto& cp : contact.points) {
                // Friction
                auto v_a = vel_a.v + void_math::cross(vel_a.w, cp.r_a);
                auto v_b = vel_b.v + void_math::cross(vel_b.w, cp.r_b);
                auto dv = v_b - v_a;

                float max_friction = contact.friction * cp.normal_impulse;

                // Tangent 1
                float vt1 = void_math::dot(dv, contact.tangent_1);
                float dt1 = cp.tangent_mass_1 * (-vt1);
                float old_t1 = cp.tangent_impulse_1;
                cp.tangent_impulse_1 = std::clamp(old_t1 + dt1, -max_friction, max_friction);
                dt1 = cp.tangent_impulse_1 - old_t1;

                auto pt1 = contact.tangent_1 * dt1;
                vel_a.v = vel_a.v - pt1 * contact.inv_mass_a;
                vel_a.w = vel_a.w - void_math::cross(cp.r_a, pt1) * contact.inv_inertia_a;
                vel_b.v = vel_b.v + pt1 * contact.inv_mass_b;
                vel_b.w = vel_b.w + void_math::cross(cp.r_b, pt1) * contact.inv_inertia_b;

                // Tangent 2
                float vt2 = void_math::dot(dv, contact.tangent_2);
                float dt2 = cp.tangent_mass_2 * (-vt2);
                float old_t2 = cp.tangent_impulse_2;
                cp.tangent_impulse_2 = std::clamp(old_t2 + dt2, -max_friction, max_friction);
                dt2 = cp.tangent_impulse_2 - old_t2;

                auto pt2 = contact.tangent_2 * dt2;
                vel_a.v = vel_a.v - pt2 * contact.inv_mass_a;
                vel_a.w = vel_a.w - void_math::cross(cp.r_a, pt2) * contact.inv_inertia_a;
                vel_b.v = vel_b.v + pt2 * contact.inv_mass_b;
                vel_b.w = vel_b.w + void_math::cross(cp.r_b, pt2) * contact.inv_inertia_b;

                // Normal constraint
                v_a = vel_a.v + void_math::cross(vel_a.w, cp.r_a);
                v_b = vel_b.v + void_math::cross(vel_b.w, cp.r_b);
                float vn = void_math::dot(v_b - v_a, contact.normal);

                float dn = cp.normal_mass * (-vn + cp.velocity_bias);
                float old_n = cp.normal_impulse;
                cp.normal_impulse = std::max(old_n + dn, 0.0f);
                dn = cp.normal_impulse - old_n;

                auto pn = contact.normal * dn;
                vel_a.v = vel_a.v - pn * contact.inv_mass_a;
                vel_a.w = vel_a.w - void_math::cross(cp.r_a, pn) * contact.inv_inertia_a;
                vel_b.v = vel_b.v + pn * contact.inv_mass_b;
                vel_b.w = vel_b.w + void_math::cross(cp.r_b, pn) * contact.inv_inertia_b;
            }
        }
    }

    /// Solve position constraints (penetration resolution)
    bool solve_position(
        const std::vector<ContactConstraint>& contacts,
        std::vector<PositionState>& positions)
    {
        float max_penetration = 0.0f;

        for (const auto& contact : contacts) {
            if (contact.index_a < 0 || contact.index_b < 0) continue;

            auto& pos_a = positions[static_cast<size_t>(contact.index_a)];
            auto& pos_b = positions[static_cast<size_t>(contact.index_b)];

            for (const auto& cp : contact.points) {
                auto r_a = void_math::rotate(pos_a.q, cp.local_a);
                auto r_b = void_math::rotate(pos_b.q, cp.local_b);

                auto world_a = pos_a.p + r_a;
                auto world_b = pos_b.p + r_b;

                float separation = void_math::dot(world_b - world_a, contact.normal);
                max_penetration = std::min(max_penetration, separation);

                float c = std::clamp(m_config.baumgarte * (separation + m_config.slop), -0.2f, 0.0f);

                auto r_a_cross_n = void_math::cross(r_a, contact.normal);
                auto r_b_cross_n = void_math::cross(r_b, contact.normal);
                float k = contact.inv_mass_a + contact.inv_mass_b
                    + void_math::dot(r_a_cross_n * contact.inv_inertia_a, r_a_cross_n)
                    + void_math::dot(r_b_cross_n * contact.inv_inertia_b, r_b_cross_n);

                float impulse = k > 0.0001f ? -c / k : 0.0f;
                auto p = contact.normal * impulse;

                pos_a.p = pos_a.p - p * contact.inv_mass_a;
                pos_b.p = pos_b.p + p * contact.inv_mass_b;

                // Angular correction
                auto da = void_math::cross(r_a, p) * contact.inv_inertia_a;
                auto db = void_math::cross(r_b, p) * contact.inv_inertia_b;
                pos_a.q = void_math::normalize(apply_angular(pos_a.q, -da * 0.5f));
                pos_b.q = void_math::normalize(apply_angular(pos_b.q, db * 0.5f));
            }
        }

        return max_penetration >= -3.0f * m_config.slop;
    }

private:
    static void_math::Quat apply_angular(const void_math::Quat& q, const void_math::Vec3& impulse) {
        void_math::Quat dq{impulse.x, impulse.y, impulse.z, 0};
        dq = void_math::Quat{
            dq.x * q.w + dq.w * q.x + dq.y * q.z - dq.z * q.y,
            dq.y * q.w + dq.w * q.y + dq.z * q.x - dq.x * q.z,
            dq.z * q.w + dq.w * q.z + dq.x * q.y - dq.y * q.x,
            dq.w * q.w - dq.x * q.x - dq.y * q.y - dq.z * q.z
        };
        return void_math::Quat{
            q.x + 0.5f * dq.x,
            q.y + 0.5f * dq.y,
            q.z + 0.5f * dq.z,
            q.w + 0.5f * dq.w
        };
    }

    SolverConfig m_config;
};

// =============================================================================
// Constraint Solver
// =============================================================================

/// Main constraint solver combining contacts and joints
class ConstraintSolver {
public:
    explicit ConstraintSolver(const SolverConfig& config = {})
        : m_config(config)
    {}

    /// Solve all constraints
    void solve(
        std::vector<ContactConstraint>& contacts,
        std::vector<std::unique_ptr<IJointConstraint>>& joints,
        std::vector<VelocityState>& velocities,
        std::vector<PositionState>& positions,
        const std::vector<float>& inv_masses,
        const std::vector<void_math::Vec3>& inv_inertias,
        float dt)
    {
        // Initialize contact solver
        m_contact_solver.initialize(contacts, velocities, positions, m_config, dt);

        // Initialize joints
        for (auto& joint : joints) {
            auto ba = joint->body_a();
            auto bb = joint->body_b();
            // Note: In real implementation, map body IDs to indices
            // For now, assume body ID value - 1 = index
            int ia = static_cast<int>(ba.value) - 1;
            int ib = static_cast<int>(bb.value) - 1;

            if (ia >= 0 && ia < static_cast<int>(positions.size()) &&
                ib >= 0 && ib < static_cast<int>(positions.size())) {
                joint->initialize(
                    positions[static_cast<size_t>(ia)], positions[static_cast<size_t>(ib)],
                    velocities[static_cast<size_t>(ia)], velocities[static_cast<size_t>(ib)],
                    inv_masses[static_cast<size_t>(ia)], inv_masses[static_cast<size_t>(ib)],
                    inv_inertias[static_cast<size_t>(ia)], inv_inertias[static_cast<size_t>(ib)],
                    dt);
            }
        }

        // Warm start
        m_contact_solver.warm_start(contacts, velocities);
        if (m_config.warm_starting) {
            for (auto& joint : joints) {
                auto ba = joint->body_a();
                auto bb = joint->body_b();
                int ia = static_cast<int>(ba.value) - 1;
                int ib = static_cast<int>(bb.value) - 1;

                if (ia >= 0 && ia < static_cast<int>(velocities.size()) &&
                    ib >= 0 && ib < static_cast<int>(velocities.size())) {
                    joint->warm_start(
                        velocities[static_cast<size_t>(ia)], velocities[static_cast<size_t>(ib)],
                        inv_masses[static_cast<size_t>(ia)], inv_masses[static_cast<size_t>(ib)],
                        inv_inertias[static_cast<size_t>(ia)], inv_inertias[static_cast<size_t>(ib)]);
                }
            }
        }

        // Velocity iterations
        for (std::uint32_t i = 0; i < m_config.velocity_iterations; ++i) {
            for (auto& joint : joints) {
                auto ba = joint->body_a();
                auto bb = joint->body_b();
                int ia = static_cast<int>(ba.value) - 1;
                int ib = static_cast<int>(bb.value) - 1;

                if (ia >= 0 && ia < static_cast<int>(velocities.size()) &&
                    ib >= 0 && ib < static_cast<int>(velocities.size())) {
                    joint->solve_velocity(
                        velocities[static_cast<size_t>(ia)], velocities[static_cast<size_t>(ib)],
                        inv_masses[static_cast<size_t>(ia)], inv_masses[static_cast<size_t>(ib)],
                        inv_inertias[static_cast<size_t>(ia)], inv_inertias[static_cast<size_t>(ib)]);
                }
            }

            m_contact_solver.solve_velocity(contacts, velocities);
        }

        // Position iterations
        for (std::uint32_t i = 0; i < m_config.position_iterations; ++i) {
            bool contacts_ok = m_contact_solver.solve_position(contacts, positions);

            bool joints_ok = true;
            for (auto& joint : joints) {
                auto ba = joint->body_a();
                auto bb = joint->body_b();
                int ia = static_cast<int>(ba.value) - 1;
                int ib = static_cast<int>(bb.value) - 1;

                if (ia >= 0 && ia < static_cast<int>(positions.size()) &&
                    ib >= 0 && ib < static_cast<int>(positions.size())) {
                    if (!joint->solve_position(
                        positions[static_cast<size_t>(ia)], positions[static_cast<size_t>(ib)],
                        inv_masses[static_cast<size_t>(ia)], inv_masses[static_cast<size_t>(ib)],
                        inv_inertias[static_cast<size_t>(ia)], inv_inertias[static_cast<size_t>(ib)])) {
                        joints_ok = false;
                    }
                }
            }

            if (contacts_ok && joints_ok) break;
        }
    }

    /// Get configuration
    [[nodiscard]] const SolverConfig& config() const { return m_config; }

    /// Set configuration
    void set_config(const SolverConfig& config) { m_config = config; }

private:
    SolverConfig m_config;
    ContactSolver m_contact_solver;
};

// =============================================================================
// Material Combine Functions
// =============================================================================

/// Combine friction values
[[nodiscard]] inline float combine_friction(
    float a, float b,
    PhysicsMaterialData::CombineMode mode)
{
    switch (mode) {
        case PhysicsMaterialData::CombineMode::Average:
            return (a + b) * 0.5f;
        case PhysicsMaterialData::CombineMode::Minimum:
            return std::min(a, b);
        case PhysicsMaterialData::CombineMode::Maximum:
            return std::max(a, b);
        case PhysicsMaterialData::CombineMode::Multiply:
            return a * b;
    }
    return (a + b) * 0.5f;
}

/// Combine restitution values
[[nodiscard]] inline float combine_restitution(
    float a, float b,
    PhysicsMaterialData::CombineMode mode)
{
    return combine_friction(a, b, mode);  // Same logic
}

} // namespace void_physics
