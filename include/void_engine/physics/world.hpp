/// @file world.hpp
/// @brief Physics world and scene queries for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "shape.hpp"
#include "body.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/ray.hpp>
#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace void_physics {

// Forward declarations
class PhysicsPipeline;
class QuerySystem;
class CharacterControllerImpl;
class BroadPhaseBvh;
class IJointConstraint;

// =============================================================================
// Physics World Interface
// =============================================================================

/// Interface for the physics simulation world
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    // =========================================================================
    // Simulation
    // =========================================================================

    /// Step the simulation
    /// @param dt Time step in seconds
    virtual void step(float dt) = 0;

    /// Step with substeps
    /// @param dt Total time
    /// @param substeps Number of substeps
    virtual void step_with_substeps(float dt, std::uint32_t substeps) = 0;

    /// Get fixed timestep
    [[nodiscard]] virtual float fixed_timestep() const = 0;

    /// Set fixed timestep
    virtual void set_fixed_timestep(float dt) = 0;

    // =========================================================================
    // World Settings
    // =========================================================================

    /// Get gravity
    [[nodiscard]] virtual void_math::Vec3 gravity() const = 0;

    /// Set gravity
    virtual void set_gravity(const void_math::Vec3& gravity) = 0;

    /// Get configuration
    [[nodiscard]] virtual const PhysicsConfig& config() const = 0;

    // =========================================================================
    // Bodies
    // =========================================================================

    /// Create a rigidbody
    [[nodiscard]] virtual BodyId create_body(const BodyConfig& config) = 0;

    /// Create body with builder
    [[nodiscard]] virtual BodyId create_body(BodyBuilder& builder) = 0;

    /// Destroy a body
    virtual void destroy_body(BodyId id) = 0;

    /// Get body by ID
    [[nodiscard]] virtual IRigidbody* get_body(BodyId id) = 0;
    [[nodiscard]] virtual const IRigidbody* get_body(BodyId id) const = 0;

    /// Check if body exists
    [[nodiscard]] virtual bool body_exists(BodyId id) const = 0;

    /// Get body count
    [[nodiscard]] virtual std::size_t body_count() const = 0;

    /// Iterate all bodies
    virtual void for_each_body(std::function<void(IRigidbody&)> callback) = 0;
    virtual void for_each_body(std::function<void(const IRigidbody&)> callback) const = 0;

    // =========================================================================
    // Joints
    // =========================================================================

    /// Create a joint
    [[nodiscard]] virtual JointId create_joint(const JointConfig& config) = 0;

    /// Create hinge joint
    [[nodiscard]] virtual JointId create_hinge_joint(const HingeJointConfig& config) = 0;

    /// Create slider joint
    [[nodiscard]] virtual JointId create_slider_joint(const SliderJointConfig& config) = 0;

    /// Create ball joint
    [[nodiscard]] virtual JointId create_ball_joint(const BallJointConfig& config) = 0;

    /// Create distance joint
    [[nodiscard]] virtual JointId create_distance_joint(const DistanceJointConfig& config) = 0;

    /// Create spring joint
    [[nodiscard]] virtual JointId create_spring_joint(const SpringJointConfig& config) = 0;

    /// Destroy a joint
    virtual void destroy_joint(JointId id) = 0;

    /// Get joint count
    [[nodiscard]] virtual std::size_t joint_count() const = 0;

    // =========================================================================
    // Materials
    // =========================================================================

    /// Create physics material
    [[nodiscard]] virtual MaterialId create_material(const PhysicsMaterialData& data) = 0;

    /// Get default material
    [[nodiscard]] virtual MaterialId default_material() const = 0;

    /// Get material data
    [[nodiscard]] virtual const PhysicsMaterialData* get_material(MaterialId id) const = 0;

    /// Update material
    virtual void update_material(MaterialId id, const PhysicsMaterialData& data) = 0;

    // =========================================================================
    // Queries - Raycast
    // =========================================================================

    /// Cast ray and get first hit
    [[nodiscard]] virtual RaycastHit raycast(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Cast ray and get all hits
    [[nodiscard]] virtual std::vector<RaycastHit> raycast_all(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Cast ray with callback
    virtual void raycast_callback(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask,
        std::function<bool(const RaycastHit&)> callback) const = 0;

    // =========================================================================
    // Queries - Shape Cast
    // =========================================================================

    /// Cast shape and get first hit
    [[nodiscard]] virtual ShapeCastHit shape_cast(
        const IShape& shape,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Sphere cast (convenience)
    [[nodiscard]] virtual ShapeCastHit sphere_cast(
        float radius,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Box cast (convenience)
    [[nodiscard]] virtual ShapeCastHit box_cast(
        const void_math::Vec3& half_extents,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Capsule cast (convenience)
    [[nodiscard]] virtual ShapeCastHit capsule_cast(
        float radius,
        float height,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    // =========================================================================
    // Queries - Overlap
    // =========================================================================

    /// Test if shape overlaps any bodies
    [[nodiscard]] virtual bool overlap_test(
        const IShape& shape,
        const void_math::Transform& transform,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Get all overlapping bodies
    [[nodiscard]] virtual std::vector<OverlapResult> overlap_all(
        const IShape& shape,
        const void_math::Transform& transform,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Sphere overlap (convenience)
    [[nodiscard]] virtual std::vector<OverlapResult> overlap_sphere(
        const void_math::Vec3& center,
        float radius,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Box overlap (convenience)
    [[nodiscard]] virtual std::vector<OverlapResult> overlap_box(
        const void_math::Vec3& center,
        const void_math::Vec3& half_extents,
        const void_math::Quat& rotation = {},
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    // =========================================================================
    // Queries - Point
    // =========================================================================

    /// Get closest body to point
    [[nodiscard]] virtual BodyId closest_body(
        const void_math::Vec3& point,
        float max_distance = 1000.0f,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    /// Get all bodies containing point
    [[nodiscard]] virtual std::vector<BodyId> bodies_at_point(
        const void_math::Vec3& point,
        QueryFilter filter = QueryFilter::Default,
        CollisionLayer layer_mask = layers::All) const = 0;

    // =========================================================================
    // Collision Events
    // =========================================================================

    /// Set collision begin callback
    virtual void on_collision_begin(CollisionCallback callback) = 0;

    /// Set collision stay callback
    virtual void on_collision_stay(CollisionCallback callback) = 0;

    /// Set collision end callback
    virtual void on_collision_end(CollisionCallback callback) = 0;

    /// Set trigger enter callback
    virtual void on_trigger_enter(TriggerCallback callback) = 0;

    /// Set trigger stay callback
    virtual void on_trigger_stay(TriggerCallback callback) = 0;

    /// Set trigger exit callback
    virtual void on_trigger_exit(TriggerCallback callback) = 0;

    /// Set contact filter
    virtual void set_contact_filter(ContactFilterCallback filter) = 0;

    /// Set joint break callback
    virtual void on_joint_break(JointBreakCallback callback) = 0;

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get physics statistics
    [[nodiscard]] virtual PhysicsStats stats() const = 0;

    // =========================================================================
    // Debug
    // =========================================================================

    /// Enable/disable debug rendering
    virtual void set_debug_render_enabled(bool enabled) = 0;

    /// Check if debug rendering is enabled
    [[nodiscard]] virtual bool debug_render_enabled() const = 0;

    /// Get debug renderer
    [[nodiscard]] virtual PhysicsDebugRenderer* debug_renderer() = 0;

    // =========================================================================
    // Serialization
    // =========================================================================

    /// Snapshot current state (for hot-reload)
    [[nodiscard]] virtual void_core::Result<void_core::HotReloadSnapshot> snapshot() const = 0;

    /// Restore from snapshot
    [[nodiscard]] virtual void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) = 0;

    // =========================================================================
    // Clear
    // =========================================================================

    /// Remove all bodies and joints
    virtual void clear() = 0;
};

// =============================================================================
// Physics World Implementation
// =============================================================================

/// Default physics world implementation
class PhysicsWorld : public IPhysicsWorld {
public:
    explicit PhysicsWorld(PhysicsConfig config = PhysicsConfig::defaults());
    ~PhysicsWorld() override;

    // IPhysicsWorld implementation
    void step(float dt) override;
    void step_with_substeps(float dt, std::uint32_t substeps) override;
    [[nodiscard]] float fixed_timestep() const override { return m_config.fixed_timestep; }
    void set_fixed_timestep(float dt) override { m_config.fixed_timestep = dt; }

    [[nodiscard]] void_math::Vec3 gravity() const override { return m_config.gravity; }
    void set_gravity(const void_math::Vec3& gravity) override { m_config.gravity = gravity; }
    [[nodiscard]] const PhysicsConfig& config() const override { return m_config; }

    [[nodiscard]] BodyId create_body(const BodyConfig& config) override;
    [[nodiscard]] BodyId create_body(BodyBuilder& builder) override;
    void destroy_body(BodyId id) override;
    [[nodiscard]] IRigidbody* get_body(BodyId id) override;
    [[nodiscard]] const IRigidbody* get_body(BodyId id) const override;
    [[nodiscard]] bool body_exists(BodyId id) const override;
    [[nodiscard]] std::size_t body_count() const override { return m_bodies.size(); }
    void for_each_body(std::function<void(IRigidbody&)> callback) override;
    void for_each_body(std::function<void(const IRigidbody&)> callback) const override;

    [[nodiscard]] JointId create_joint(const JointConfig& config) override;
    [[nodiscard]] JointId create_hinge_joint(const HingeJointConfig& config) override;
    [[nodiscard]] JointId create_slider_joint(const SliderJointConfig& config) override;
    [[nodiscard]] JointId create_ball_joint(const BallJointConfig& config) override;
    [[nodiscard]] JointId create_distance_joint(const DistanceJointConfig& config) override;
    [[nodiscard]] JointId create_spring_joint(const SpringJointConfig& config) override;
    void destroy_joint(JointId id) override;
    [[nodiscard]] std::size_t joint_count() const override;

    [[nodiscard]] MaterialId create_material(const PhysicsMaterialData& data) override;
    [[nodiscard]] MaterialId default_material() const override { return m_default_material; }
    [[nodiscard]] const PhysicsMaterialData* get_material(MaterialId id) const override;
    void update_material(MaterialId id, const PhysicsMaterialData& data) override;

    [[nodiscard]] RaycastHit raycast(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] std::vector<RaycastHit> raycast_all(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    void raycast_callback(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask,
        std::function<bool(const RaycastHit&)> callback) const override;

    [[nodiscard]] ShapeCastHit shape_cast(
        const IShape& shape,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] ShapeCastHit sphere_cast(
        float radius,
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] ShapeCastHit box_cast(
        const void_math::Vec3& half_extents,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] ShapeCastHit capsule_cast(
        float radius,
        float height,
        const void_math::Transform& start,
        const void_math::Vec3& direction,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] bool overlap_test(
        const IShape& shape,
        const void_math::Transform& transform,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] std::vector<OverlapResult> overlap_all(
        const IShape& shape,
        const void_math::Transform& transform,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] std::vector<OverlapResult> overlap_sphere(
        const void_math::Vec3& center,
        float radius,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] std::vector<OverlapResult> overlap_box(
        const void_math::Vec3& center,
        const void_math::Vec3& half_extents,
        const void_math::Quat& rotation,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] BodyId closest_body(
        const void_math::Vec3& point,
        float max_distance,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    [[nodiscard]] std::vector<BodyId> bodies_at_point(
        const void_math::Vec3& point,
        QueryFilter filter,
        CollisionLayer layer_mask) const override;

    void on_collision_begin(CollisionCallback callback) override { m_on_collision_begin = std::move(callback); }
    void on_collision_stay(CollisionCallback callback) override { m_on_collision_stay = std::move(callback); }
    void on_collision_end(CollisionCallback callback) override { m_on_collision_end = std::move(callback); }
    void on_trigger_enter(TriggerCallback callback) override { m_on_trigger_enter = std::move(callback); }
    void on_trigger_stay(TriggerCallback callback) override { m_on_trigger_stay = std::move(callback); }
    void on_trigger_exit(TriggerCallback callback) override { m_on_trigger_exit = std::move(callback); }
    void set_contact_filter(ContactFilterCallback filter) override { m_contact_filter = std::move(filter); }
    void on_joint_break(JointBreakCallback callback) override { m_on_joint_break = std::move(callback); }

    [[nodiscard]] PhysicsStats stats() const override;

    void set_debug_render_enabled(bool enabled) override { m_debug_render_enabled = enabled; }
    [[nodiscard]] bool debug_render_enabled() const override { return m_debug_render_enabled; }
    [[nodiscard]] PhysicsDebugRenderer* debug_renderer() override;

    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() const override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;

    void clear() override;

    /// Get broadphase (for internal use)
    [[nodiscard]] BroadPhaseBvh& broadphase();
    [[nodiscard]] const BroadPhaseBvh& broadphase() const;

private:
    void fire_collision_events();
    bool passes_filter(const IRigidbody& body, QueryFilter filter, CollisionLayer layer_mask) const;

private:
    PhysicsConfig m_config;

    // Simulation pipeline
    std::unique_ptr<PhysicsPipeline> m_pipeline;

    // Query system
    std::unique_ptr<QuerySystem> m_query_system;

    // Bodies
    std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>> m_bodies;
    std::uint64_t m_next_body_id = 1;

    // Joints (simplified storage for now)
    struct JointData {
        JointType type;
        BodyId body_a;
        BodyId body_b;
    };
    std::unordered_map<std::uint64_t, JointData> m_joints;
    std::vector<std::unique_ptr<IJointConstraint>> m_joint_constraints;
    std::uint64_t m_next_joint_id = 1;

    // Materials
    std::unordered_map<std::uint64_t, PhysicsMaterialData> m_materials;
    std::uint64_t m_next_material_id = 1;
    MaterialId m_default_material;

    // Collision tracking
    struct CollisionPair {
        BodyId body_a;
        BodyId body_b;
        std::vector<ContactPoint> contacts;
        bool was_colliding = false;
    };
    std::vector<CollisionPair> m_collision_pairs;
    std::vector<CollisionPair> m_trigger_pairs;

    // Callbacks
    CollisionCallback m_on_collision_begin;
    CollisionCallback m_on_collision_stay;
    CollisionCallback m_on_collision_end;
    TriggerCallback m_on_trigger_enter;
    TriggerCallback m_on_trigger_stay;
    TriggerCallback m_on_trigger_exit;
    ContactFilterCallback m_contact_filter;
    JointBreakCallback m_on_joint_break;

    // Statistics
    mutable PhysicsStats m_stats;

    // Debug
    bool m_debug_render_enabled = false;
    std::unique_ptr<PhysicsDebugRenderer> m_debug_renderer;

    // Time accumulator for fixed step
    float m_time_accumulator = 0.0f;

    // Character controller implementation
    std::unique_ptr<CharacterControllerImpl> m_impl;
};

// =============================================================================
// Physics World Builder
// =============================================================================

/// Fluent builder for physics world configuration
class PhysicsWorldBuilder {
public:
    PhysicsWorldBuilder() = default;

    /// Set backend
    PhysicsWorldBuilder& backend(PhysicsBackend b) { m_config.backend = b; return *this; }

    /// Set gravity
    PhysicsWorldBuilder& gravity(const void_math::Vec3& g) { m_config.gravity = g; return *this; }
    PhysicsWorldBuilder& gravity(float x, float y, float z) { return gravity({x, y, z}); }

    /// Set fixed timestep
    PhysicsWorldBuilder& fixed_timestep(float dt) { m_config.fixed_timestep = dt; return *this; }

    /// Set max substeps
    PhysicsWorldBuilder& max_substeps(std::uint32_t n) { m_config.max_substeps = n; return *this; }

    /// Set velocity iterations
    PhysicsWorldBuilder& velocity_iterations(std::uint32_t n) { m_config.velocity_iterations = n; return *this; }

    /// Set position iterations
    PhysicsWorldBuilder& position_iterations(std::uint32_t n) { m_config.position_iterations = n; return *this; }

    /// Set max bodies
    PhysicsWorldBuilder& max_bodies(std::uint32_t n) { m_config.max_bodies = n; return *this; }

    /// Enable CCD
    PhysicsWorldBuilder& enable_ccd(bool enabled = true) { m_config.enable_ccd = enabled; return *this; }

    /// Enable debug rendering
    PhysicsWorldBuilder& debug_rendering(bool enabled = true) { m_config.enable_debug_rendering = enabled; return *this; }

    /// Enable profiling
    PhysicsWorldBuilder& profiling(bool enabled = true) { m_config.enable_profiling = enabled; return *this; }

    /// Enable hot-reload
    PhysicsWorldBuilder& hot_reload(bool enabled = true) { m_config.enable_hot_reload = enabled; return *this; }

    /// Build the world
    [[nodiscard]] std::unique_ptr<PhysicsWorld> build() {
        return std::make_unique<PhysicsWorld>(m_config);
    }

private:
    PhysicsConfig m_config = PhysicsConfig::defaults();
};

// =============================================================================
// Character Controller
// =============================================================================

/// Interface for character controller
class ICharacterController {
public:
    virtual ~ICharacterController() = default;

    /// Move the character
    virtual void move(const void_math::Vec3& displacement, float dt) = 0;

    /// Get position
    [[nodiscard]] virtual void_math::Vec3 position() const = 0;

    /// Set position
    virtual void set_position(const void_math::Vec3& pos) = 0;

    /// Get velocity
    [[nodiscard]] virtual void_math::Vec3 velocity() const = 0;

    /// Check if grounded
    [[nodiscard]] virtual bool is_grounded() const = 0;

    /// Get ground normal
    [[nodiscard]] virtual void_math::Vec3 ground_normal() const = 0;

    /// Check collision flags
    [[nodiscard]] virtual bool collides_above() const = 0;
    [[nodiscard]] virtual bool collides_sides() const = 0;

    /// Resize the controller
    virtual void resize(float height, float radius) = 0;
};

/// Character controller implementation
class CharacterController : public ICharacterController {
public:
    CharacterController(IPhysicsWorld& world, const CharacterControllerConfig& config);
    ~CharacterController() override;

    void move(const void_math::Vec3& displacement, float dt) override;
    [[nodiscard]] void_math::Vec3 position() const override { return m_position; }
    void set_position(const void_math::Vec3& pos) override { m_position = pos; }
    [[nodiscard]] void_math::Vec3 velocity() const override { return m_velocity; }
    [[nodiscard]] bool is_grounded() const override { return m_grounded; }
    [[nodiscard]] void_math::Vec3 ground_normal() const override { return m_ground_normal; }
    [[nodiscard]] bool collides_above() const override { return m_collides_above; }
    [[nodiscard]] bool collides_sides() const override { return m_collides_sides; }
    void resize(float height, float radius) override;

private:
    void_math::Vec3 slide_move(const void_math::Vec3& displacement);
    void update_grounded();

    IPhysicsWorld& m_world;
    CharacterControllerConfig m_config;

    void_math::Vec3 m_position{0, 0, 0};
    void_math::Vec3 m_velocity{0, 0, 0};
    void_math::Vec3 m_ground_normal{0, 1, 0};

    bool m_grounded = false;
    bool m_collides_above = false;
    bool m_collides_sides = false;
};

// =============================================================================
// Debug Renderer
// =============================================================================

/// Physics debug rendering interface
class PhysicsDebugRenderer {
public:
    virtual ~PhysicsDebugRenderer() = default;

    /// Begin frame
    virtual void begin() = 0;

    /// End frame
    virtual void end() = 0;

    /// Draw line
    virtual void draw_line(
        const void_math::Vec3& from,
        const void_math::Vec3& to,
        std::uint32_t color) = 0;

    /// Draw triangle
    virtual void draw_triangle(
        const void_math::Vec3& a,
        const void_math::Vec3& b,
        const void_math::Vec3& c,
        std::uint32_t color) = 0;

    /// Draw box
    virtual void draw_box(
        const void_math::Vec3& center,
        const void_math::Vec3& half_extents,
        const void_math::Quat& rotation,
        std::uint32_t color);

    /// Draw sphere
    virtual void draw_sphere(
        const void_math::Vec3& center,
        float radius,
        std::uint32_t color);

    /// Draw capsule
    virtual void draw_capsule(
        const void_math::Vec3& p1,
        const void_math::Vec3& p2,
        float radius,
        std::uint32_t color);

    /// Draw arrow
    virtual void draw_arrow(
        const void_math::Vec3& from,
        const void_math::Vec3& to,
        std::uint32_t color);

    /// Draw contact point
    virtual void draw_contact(
        const void_math::Vec3& position,
        const void_math::Vec3& normal,
        float depth,
        std::uint32_t color);

    /// Draw body
    virtual void draw_body(const IRigidbody& body);

    /// Draw world
    virtual void draw_world(const IPhysicsWorld& world);

    /// Predefined colors
    struct Colors {
        static constexpr std::uint32_t White = 0xFFFFFFFF;
        static constexpr std::uint32_t Red = 0xFF0000FF;
        static constexpr std::uint32_t Green = 0x00FF00FF;
        static constexpr std::uint32_t Blue = 0x0000FFFF;
        static constexpr std::uint32_t Yellow = 0xFFFF00FF;
        static constexpr std::uint32_t Cyan = 0x00FFFFFF;
        static constexpr std::uint32_t Magenta = 0xFF00FFFF;
        static constexpr std::uint32_t Orange = 0xFFA500FF;

        static constexpr std::uint32_t StaticBody = 0x808080FF;
        static constexpr std::uint32_t DynamicBody = 0x00FF00FF;
        static constexpr std::uint32_t KinematicBody = 0xFFFF00FF;
        static constexpr std::uint32_t SleepingBody = 0x404040FF;
        static constexpr std::uint32_t Contact = 0xFF0000FF;
        static constexpr std::uint32_t ContactNormal = 0x00FF00FF;
    };
};

} // namespace void_physics
