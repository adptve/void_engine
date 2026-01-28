/// @file body.hpp
/// @brief Rigidbody definitions for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "shape.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>
#include <void_engine/math/transform.hpp>
#include <void_engine/core/error.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace void_physics {

// =============================================================================
// Rigidbody Interface
// =============================================================================

/// Interface for rigidbodies
class IRigidbody {
public:
    virtual ~IRigidbody() = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /// Get body ID
    [[nodiscard]] virtual BodyId id() const noexcept = 0;

    /// Get body type
    [[nodiscard]] virtual BodyType type() const noexcept = 0;

    /// Get body name
    [[nodiscard]] virtual const std::string& name() const = 0;

    /// Get user data pointer
    [[nodiscard]] virtual void* user_data() const noexcept = 0;

    /// Set user data pointer
    virtual void set_user_data(void* data) = 0;

    /// Get user ID (e.g., entity ID)
    [[nodiscard]] virtual std::uint64_t user_id() const noexcept = 0;

    /// Set user ID
    virtual void set_user_id(std::uint64_t id) = 0;

    // =========================================================================
    // Transform
    // =========================================================================

    /// Get world position
    [[nodiscard]] virtual void_math::Vec3 position() const = 0;

    /// Set world position
    virtual void set_position(const void_math::Vec3& pos) = 0;

    /// Get world rotation
    [[nodiscard]] virtual void_math::Quat rotation() const = 0;

    /// Set world rotation
    virtual void set_rotation(const void_math::Quat& rot) = 0;

    /// Get world transform
    [[nodiscard]] virtual void_math::Transform transform() const = 0;

    /// Set world transform
    virtual void set_transform(const void_math::Transform& t) = 0;

    /// Get world-space center of mass
    [[nodiscard]] virtual void_math::Vec3 world_center_of_mass() const = 0;

    // =========================================================================
    // Velocity
    // =========================================================================

    /// Get linear velocity
    [[nodiscard]] virtual void_math::Vec3 linear_velocity() const = 0;

    /// Set linear velocity
    virtual void set_linear_velocity(const void_math::Vec3& vel) = 0;

    /// Get angular velocity
    [[nodiscard]] virtual void_math::Vec3 angular_velocity() const = 0;

    /// Set angular velocity
    virtual void set_angular_velocity(const void_math::Vec3& vel) = 0;

    /// Get velocity at world point
    [[nodiscard]] virtual void_math::Vec3 velocity_at_point(const void_math::Vec3& world_point) const = 0;

    // =========================================================================
    // Forces
    // =========================================================================

    /// Add force at center of mass
    virtual void add_force(const void_math::Vec3& force, ForceMode mode = ForceMode::Force) = 0;

    /// Add force at world position
    virtual void add_force_at_position(
        const void_math::Vec3& force,
        const void_math::Vec3& position,
        ForceMode mode = ForceMode::Force) = 0;

    /// Add torque
    virtual void add_torque(const void_math::Vec3& torque, ForceMode mode = ForceMode::Force) = 0;

    /// Add relative force (body-local direction)
    virtual void add_relative_force(const void_math::Vec3& force, ForceMode mode = ForceMode::Force) = 0;

    /// Add relative torque (body-local)
    virtual void add_relative_torque(const void_math::Vec3& torque, ForceMode mode = ForceMode::Force) = 0;

    /// Clear all accumulated forces
    virtual void clear_forces() = 0;

    // =========================================================================
    // Mass
    // =========================================================================

    /// Get mass
    [[nodiscard]] virtual float mass() const = 0;

    /// Set mass
    virtual void set_mass(float mass) = 0;

    /// Get inverse mass (0 for static/kinematic)
    [[nodiscard]] virtual float inverse_mass() const = 0;

    /// Get inertia tensor (world space)
    [[nodiscard]] virtual void_math::Vec3 inertia() const = 0;

    /// Set inertia tensor
    virtual void set_inertia(const void_math::Vec3& inertia) = 0;

    /// Get mass properties
    [[nodiscard]] virtual MassProperties mass_properties() const = 0;

    /// Set mass properties
    virtual void set_mass_properties(const MassProperties& props) = 0;

    // =========================================================================
    // Damping
    // =========================================================================

    /// Get linear damping
    [[nodiscard]] virtual float linear_damping() const = 0;

    /// Set linear damping
    virtual void set_linear_damping(float damping) = 0;

    /// Get angular damping
    [[nodiscard]] virtual float angular_damping() const = 0;

    /// Set angular damping
    virtual void set_angular_damping(float damping) = 0;

    // =========================================================================
    // Gravity
    // =========================================================================

    /// Get gravity scale
    [[nodiscard]] virtual float gravity_scale() const = 0;

    /// Set gravity scale
    virtual void set_gravity_scale(float scale) = 0;

    /// Check if gravity is enabled
    [[nodiscard]] virtual bool gravity_enabled() const = 0;

    /// Enable/disable gravity
    virtual void set_gravity_enabled(bool enabled) = 0;

    // =========================================================================
    // Collision
    // =========================================================================

    /// Get collision mask
    [[nodiscard]] virtual CollisionMask collision_mask() const = 0;

    /// Set collision mask
    virtual void set_collision_mask(const CollisionMask& mask) = 0;

    /// Set collision layer
    virtual void set_layer(CollisionLayer layer) = 0;

    /// Set collision filter
    virtual void set_collides_with(CollisionLayer mask) = 0;

    /// Get collision response
    [[nodiscard]] virtual CollisionResponse collision_response() const = 0;

    /// Set collision response
    virtual void set_collision_response(CollisionResponse response) = 0;

    /// Check if this is a trigger
    [[nodiscard]] virtual bool is_trigger() const = 0;

    /// Set as trigger
    virtual void set_trigger(bool trigger) = 0;

    // =========================================================================
    // CCD
    // =========================================================================

    /// Check if continuous collision detection is enabled
    [[nodiscard]] virtual bool continuous_detection() const = 0;

    /// Enable/disable CCD
    virtual void set_continuous_detection(bool enabled) = 0;

    // =========================================================================
    // Sleeping
    // =========================================================================

    /// Get activation state
    [[nodiscard]] virtual ActivationState activation_state() const = 0;

    /// Set activation state
    virtual void set_activation_state(ActivationState state) = 0;

    /// Check if sleeping
    [[nodiscard]] virtual bool is_sleeping() const = 0;

    /// Wake up the body
    virtual void wake_up() = 0;

    /// Put to sleep
    virtual void sleep() = 0;

    /// Check if sleep is allowed
    [[nodiscard]] virtual bool can_sleep() const = 0;

    /// Allow/disallow sleeping
    virtual void set_can_sleep(bool can_sleep) = 0;

    // =========================================================================
    // Constraints
    // =========================================================================

    /// Lock linear axes (x, y, z)
    virtual void lock_linear_axis(bool x, bool y, bool z) = 0;

    /// Lock angular axes (x, y, z)
    virtual void lock_angular_axis(bool x, bool y, bool z) = 0;

    /// Check if rotation is fixed
    [[nodiscard]] virtual bool fixed_rotation() const = 0;

    /// Set fixed rotation
    virtual void set_fixed_rotation(bool fixed) = 0;

    // =========================================================================
    // Shapes
    // =========================================================================

    /// Add shape to body
    virtual ShapeId add_shape(std::unique_ptr<IShape> shape) = 0;

    /// Remove shape from body
    virtual void remove_shape(ShapeId shape_id) = 0;

    /// Get shape count
    [[nodiscard]] virtual std::size_t shape_count() const = 0;

    /// Get shape by index
    [[nodiscard]] virtual IShape* get_shape(std::size_t index) = 0;
    [[nodiscard]] virtual const IShape* get_shape(std::size_t index) const = 0;

    /// Get shape by ID
    [[nodiscard]] virtual IShape* get_shape_by_id(ShapeId id) = 0;
    [[nodiscard]] virtual const IShape* get_shape_by_id(ShapeId id) const = 0;

    /// Get world bounds (all shapes combined)
    [[nodiscard]] virtual void_math::AABB world_bounds() const = 0;

    // =========================================================================
    // Queries
    // =========================================================================

    /// Test if point is inside any shape
    [[nodiscard]] virtual bool contains_point(const void_math::Vec3& world_point) const = 0;

    /// Get closest point on body surface
    [[nodiscard]] virtual void_math::Vec3 closest_point(const void_math::Vec3& world_point) const = 0;

    // =========================================================================
    // Kinematic
    // =========================================================================

    /// Move kinematic body to target (interpolated)
    virtual void move_kinematic(const void_math::Vec3& target_position, const void_math::Quat& target_rotation) = 0;

    // =========================================================================
    // State
    // =========================================================================

    /// Check if body is valid (in physics world)
    [[nodiscard]] virtual bool is_valid() const = 0;

    /// Enable/disable the body
    virtual void set_enabled(bool enabled) = 0;

    /// Check if enabled
    [[nodiscard]] virtual bool is_enabled() const = 0;
};

// =============================================================================
// Rigidbody Implementation
// =============================================================================

/// Default rigidbody implementation
class Rigidbody : public IRigidbody {
public:
    explicit Rigidbody(const BodyConfig& config);
    ~Rigidbody() override = default;

    // IRigidbody implementation
    [[nodiscard]] BodyId id() const noexcept override { return m_id; }
    [[nodiscard]] BodyType type() const noexcept override { return m_type; }
    [[nodiscard]] const std::string& name() const override { return m_name; }
    [[nodiscard]] void* user_data() const noexcept override { return m_user_data; }
    void set_user_data(void* data) override { m_user_data = data; }
    [[nodiscard]] std::uint64_t user_id() const noexcept override { return m_user_id; }
    void set_user_id(std::uint64_t id) override { m_user_id = id; }

    [[nodiscard]] void_math::Vec3 position() const override { return m_position; }
    void set_position(const void_math::Vec3& pos) override { m_position = pos; }
    [[nodiscard]] void_math::Quat rotation() const override { return m_rotation; }
    void set_rotation(const void_math::Quat& rot) override { m_rotation = rot; }
    [[nodiscard]] void_math::Transform transform() const override;
    void set_transform(const void_math::Transform& t) override;
    [[nodiscard]] void_math::Vec3 world_center_of_mass() const override;

    [[nodiscard]] void_math::Vec3 linear_velocity() const override { return m_linear_velocity; }
    void set_linear_velocity(const void_math::Vec3& vel) override { m_linear_velocity = vel; }
    [[nodiscard]] void_math::Vec3 angular_velocity() const override { return m_angular_velocity; }
    void set_angular_velocity(const void_math::Vec3& vel) override { m_angular_velocity = vel; }
    [[nodiscard]] void_math::Vec3 velocity_at_point(const void_math::Vec3& world_point) const override;

    void add_force(const void_math::Vec3& force, ForceMode mode) override;
    void add_force_at_position(const void_math::Vec3& force, const void_math::Vec3& position, ForceMode mode) override;
    void add_torque(const void_math::Vec3& torque, ForceMode mode) override;
    void add_relative_force(const void_math::Vec3& force, ForceMode mode) override;
    void add_relative_torque(const void_math::Vec3& torque, ForceMode mode) override;
    void clear_forces() override;

    [[nodiscard]] float mass() const override { return m_mass_props.mass; }
    void set_mass(float mass) override { m_mass_props.mass = mass; }
    [[nodiscard]] float inverse_mass() const override;
    [[nodiscard]] void_math::Vec3 inertia() const override { return m_mass_props.inertia_diagonal; }
    void set_inertia(const void_math::Vec3& inertia) override { m_mass_props.inertia_diagonal = inertia; }
    [[nodiscard]] MassProperties mass_properties() const override { return m_mass_props; }
    void set_mass_properties(const MassProperties& props) override { m_mass_props = props; }

    [[nodiscard]] float linear_damping() const override { return m_linear_damping; }
    void set_linear_damping(float damping) override { m_linear_damping = damping; }
    [[nodiscard]] float angular_damping() const override { return m_angular_damping; }
    void set_angular_damping(float damping) override { m_angular_damping = damping; }

    [[nodiscard]] float gravity_scale() const override { return m_gravity_scale; }
    void set_gravity_scale(float scale) override { m_gravity_scale = scale; }
    [[nodiscard]] bool gravity_enabled() const override { return m_gravity_enabled; }
    void set_gravity_enabled(bool enabled) override { m_gravity_enabled = enabled; }

    [[nodiscard]] CollisionMask collision_mask() const override { return m_collision_mask; }
    void set_collision_mask(const CollisionMask& mask) override { m_collision_mask = mask; }
    void set_layer(CollisionLayer layer) override { m_collision_mask.layer = layer; }
    void set_collides_with(CollisionLayer mask) override { m_collision_mask.collides_with = mask; }
    [[nodiscard]] CollisionResponse collision_response() const override { return m_collision_response; }
    void set_collision_response(CollisionResponse response) override { m_collision_response = response; }
    [[nodiscard]] bool is_trigger() const override { return m_collision_response == CollisionResponse::Trigger; }
    void set_trigger(bool trigger) override;

    [[nodiscard]] bool continuous_detection() const override { return m_ccd_enabled; }
    void set_continuous_detection(bool enabled) override { m_ccd_enabled = enabled; }

    [[nodiscard]] ActivationState activation_state() const override { return m_activation_state; }
    void set_activation_state(ActivationState state) override { m_activation_state = state; }
    [[nodiscard]] bool is_sleeping() const override { return m_activation_state == ActivationState::Sleeping; }
    void wake_up() override;
    void sleep() override;
    [[nodiscard]] bool can_sleep() const override { return m_can_sleep; }
    void set_can_sleep(bool can_sleep) override { m_can_sleep = can_sleep; }

    void lock_linear_axis(bool x, bool y, bool z) override;
    void lock_angular_axis(bool x, bool y, bool z) override;
    [[nodiscard]] bool fixed_rotation() const override { return m_fixed_rotation; }
    void set_fixed_rotation(bool fixed) override { m_fixed_rotation = fixed; }

    ShapeId add_shape(std::unique_ptr<IShape> shape) override;
    void remove_shape(ShapeId shape_id) override;
    [[nodiscard]] std::size_t shape_count() const override { return m_shapes.size(); }
    [[nodiscard]] IShape* get_shape(std::size_t index) override;
    [[nodiscard]] const IShape* get_shape(std::size_t index) const override;
    [[nodiscard]] IShape* get_shape_by_id(ShapeId id) override;
    [[nodiscard]] const IShape* get_shape_by_id(ShapeId id) const override;
    [[nodiscard]] void_math::AABB world_bounds() const override;

    [[nodiscard]] bool contains_point(const void_math::Vec3& world_point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& world_point) const override;

    void move_kinematic(const void_math::Vec3& target_position, const void_math::Quat& target_rotation) override;

    [[nodiscard]] bool is_valid() const override { return m_valid; }
    void set_enabled(bool enabled) override { m_enabled = enabled; }
    [[nodiscard]] bool is_enabled() const override { return m_enabled; }

    // Additional methods for physics simulation
    [[nodiscard]] void_math::Vec3 accumulated_force() const { return m_accumulated_force; }
    [[nodiscard]] void_math::Vec3 accumulated_torque() const { return m_accumulated_torque; }

private:
    BodyId m_id;
    BodyType m_type;
    std::string m_name;

    // Transform
    void_math::Vec3 m_position{0, 0, 0};
    void_math::Quat m_rotation{};

    // Velocity
    void_math::Vec3 m_linear_velocity{0, 0, 0};
    void_math::Vec3 m_angular_velocity{0, 0, 0};

    // Forces
    void_math::Vec3 m_accumulated_force{0, 0, 0};
    void_math::Vec3 m_accumulated_torque{0, 0, 0};

    // Mass
    MassProperties m_mass_props;

    // Damping
    float m_linear_damping = 0.01f;
    float m_angular_damping = 0.05f;

    // Gravity
    float m_gravity_scale = 1.0f;
    bool m_gravity_enabled = true;

    // Collision
    CollisionMask m_collision_mask;
    CollisionResponse m_collision_response = CollisionResponse::Collide;

    // CCD
    bool m_ccd_enabled = false;

    // Sleep
    ActivationState m_activation_state = ActivationState::Active;
    bool m_can_sleep = true;
    float m_sleep_time = 0.0f;

    // Constraints
    bool m_linear_lock[3] = {false, false, false};
    bool m_angular_lock[3] = {false, false, false};
    bool m_fixed_rotation = false;

    // Shapes
    std::vector<std::unique_ptr<IShape>> m_shapes;
    std::uint64_t m_next_shape_id = 1;

    // User data
    void* m_user_data = nullptr;
    std::uint64_t m_user_id = 0;

    // State
    bool m_valid = true;
    bool m_enabled = true;

    // Kinematic target
    void_math::Vec3 m_kinematic_target_position{0, 0, 0};
    void_math::Quat m_kinematic_target_rotation{};
};

// =============================================================================
// Body Builder
// =============================================================================

/// Fluent builder for rigidbodies
class BodyBuilder {
public:
    BodyBuilder() = default;

    // Move-only (contains unique_ptr)
    BodyBuilder(BodyBuilder&&) noexcept = default;
    BodyBuilder& operator=(BodyBuilder&&) noexcept = default;
    BodyBuilder(const BodyBuilder&) = delete;
    BodyBuilder& operator=(const BodyBuilder&) = delete;

    /// Set body type
    BodyBuilder& type(BodyType t) { m_config.type = t; return *this; }
    BodyBuilder& static_body() { return type(BodyType::Static); }
    BodyBuilder& kinematic_body() { return type(BodyType::Kinematic); }
    BodyBuilder& dynamic_body() { return type(BodyType::Dynamic); }

    /// Set name
    BodyBuilder& name(const std::string& n) { m_config.name = n; return *this; }

    /// Set position
    BodyBuilder& position(const void_math::Vec3& p) { m_config.position = p; return *this; }
    BodyBuilder& position(float x, float y, float z) { return position({x, y, z}); }

    /// Set rotation
    BodyBuilder& rotation(const void_math::Quat& r) { m_config.rotation = r; return *this; }

    /// Set linear velocity
    BodyBuilder& linear_velocity(const void_math::Vec3& v) { m_config.linear_velocity = v; return *this; }

    /// Set angular velocity
    BodyBuilder& angular_velocity(const void_math::Vec3& v) { m_config.angular_velocity = v; return *this; }

    /// Set mass
    BodyBuilder& mass(float m) { m_config.mass.mass = m; return *this; }

    /// Set collision layer
    BodyBuilder& layer(CollisionLayer l) { m_config.collision_mask.layer = l; return *this; }

    /// Set collision filter
    BodyBuilder& collides_with(CollisionLayer l) { m_config.collision_mask.collides_with = l; return *this; }

    /// Set damping
    BodyBuilder& linear_damping(float d) { m_config.linear_damping = d; return *this; }
    BodyBuilder& angular_damping(float d) { m_config.angular_damping = d; return *this; }

    /// Set gravity scale
    BodyBuilder& gravity_scale(float s) { m_config.gravity_scale = s; return *this; }

    /// Enable CCD
    BodyBuilder& continuous(bool enabled = true) { m_config.continuous_detection = enabled; return *this; }

    /// Set as trigger
    BodyBuilder& trigger(bool enabled = true) { m_config.is_sensor = enabled; return *this; }

    /// Fix rotation (2D style)
    BodyBuilder& fixed_rotation(bool fixed = true) { m_config.fixed_rotation = fixed; return *this; }

    /// Start asleep
    BodyBuilder& start_asleep(bool asleep = true) { m_config.start_asleep = asleep; return *this; }

    /// Allow/disallow sleep
    BodyBuilder& allow_sleep(bool allow = true) { m_config.allow_sleep = allow; return *this; }

    /// Set user data
    BodyBuilder& user_data(void* data) { m_config.user_data = data; return *this; }

    /// Set user ID
    BodyBuilder& user_id(std::uint64_t id) { m_config.user_id = id; return *this; }

    /// Add shape
    BodyBuilder& with_shape(std::unique_ptr<IShape> shape) {
        m_shapes.push_back(std::move(shape));
        return *this;
    }

    /// Add box shape
    BodyBuilder& with_box(const void_math::Vec3& half_extents) {
        return with_shape(std::make_unique<BoxShape>(half_extents));
    }

    /// Add sphere shape
    BodyBuilder& with_sphere(float radius) {
        return with_shape(std::make_unique<SphereShape>(radius));
    }

    /// Add capsule shape
    BodyBuilder& with_capsule(float radius, float height) {
        return with_shape(std::make_unique<CapsuleShape>(radius, height));
    }

    /// Get config
    [[nodiscard]] const BodyConfig& config() const { return m_config; }

    /// Build the body
    [[nodiscard]] std::unique_ptr<Rigidbody> build();

private:
    BodyConfig m_config;
    std::vector<std::unique_ptr<IShape>> m_shapes;
};

} // namespace void_physics
