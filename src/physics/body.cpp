/// @file body.cpp
/// @brief Rigidbody implementations for void_physics

#include <void_engine/physics/body.hpp>
#include <void_engine/physics/shape.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace void_physics {

// =============================================================================
// Helper functions
// =============================================================================

namespace {

void_math::Vec3 rotate_vector(const void_math::Vec3& v, const void_math::Quat& q) {
    // Quaternion rotation: q * v * q^-1
    float qx = q.x, qy = q.y, qz = q.z, qw = q.w;

    // Cross products
    float cx = qy * v.z - qz * v.y;
    float cy = qz * v.x - qx * v.z;
    float cz = qx * v.y - qy * v.x;

    // Double cross product
    float ccx = qy * cz - qz * cy;
    float ccy = qz * cx - qx * cz;
    float ccz = qx * cy - qy * cx;

    return {
        v.x + 2.0f * (qw * cx + ccx),
        v.y + 2.0f * (qw * cy + ccy),
        v.z + 2.0f * (qw * cz + ccz)
    };
}

void_math::AABB transform_aabb(const void_math::AABB& local,
                                const void_math::Vec3& position,
                                const void_math::Quat& rotation) {
    void_math::AABB result;
    result.min = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    result.max = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    // Generate 8 corners
    for (int i = 0; i < 8; ++i) {
        void_math::Vec3 corner = {
            (i & 1) ? local.max.x : local.min.x,
            (i & 2) ? local.max.y : local.min.y,
            (i & 4) ? local.max.z : local.min.z
        };

        // Rotate corner by quaternion
        void_math::Vec3 rotated = rotate_vector(corner, rotation);

        // Translate
        void_math::Vec3 transformed = {
            rotated.x + position.x,
            rotated.y + position.y,
            rotated.z + position.z
        };

        result.min.x = std::min(result.min.x, transformed.x);
        result.min.y = std::min(result.min.y, transformed.y);
        result.min.z = std::min(result.min.z, transformed.z);
        result.max.x = std::max(result.max.x, transformed.x);
        result.max.y = std::max(result.max.y, transformed.y);
        result.max.z = std::max(result.max.z, transformed.z);
    }

    return result;
}

} // anonymous namespace

// =============================================================================
// Rigidbody Implementation
// =============================================================================

Rigidbody::Rigidbody(const BodyConfig& config)
    : m_id{config.user_id}
    , m_type(config.type)
    , m_name(config.name)
    , m_position(config.position)
    , m_rotation(config.rotation)
    , m_linear_velocity(config.linear_velocity)
    , m_angular_velocity(config.angular_velocity)
    , m_mass_props(config.mass)
    , m_linear_damping(config.linear_damping)
    , m_angular_damping(config.angular_damping)
    , m_gravity_scale(config.gravity_scale)
    , m_gravity_enabled(true)
    , m_collision_mask(config.collision_mask)
    , m_collision_response(config.response)
    , m_ccd_enabled(config.continuous_detection)
    , m_activation_state(config.start_asleep ? ActivationState::Sleeping : ActivationState::Active)
    , m_can_sleep(config.allow_sleep)
    , m_fixed_rotation(config.fixed_rotation)
    , m_user_data(config.user_data)
    , m_user_id(config.user_id)
{
    // Handle sensor/trigger
    if (config.is_sensor) {
        m_collision_response = CollisionResponse::Trigger;
    }
}

void_math::Transform Rigidbody::transform() const {
    void_math::Transform t;
    t.position = m_position;
    t.rotation = m_rotation;
    t.scale_ = {1.0f, 1.0f, 1.0f};
    return t;
}

void Rigidbody::set_transform(const void_math::Transform& t) {
    m_position = t.position;
    m_rotation = t.rotation;
    wake_up();
}

void_math::Vec3 Rigidbody::world_center_of_mass() const {
    // Rotate center of mass and add position
    void_math::Vec3 rotated_com = rotate_vector(m_mass_props.center_of_mass, m_rotation);
    return {
        m_position.x + rotated_com.x,
        m_position.y + rotated_com.y,
        m_position.z + rotated_com.z
    };
}

void_math::Vec3 Rigidbody::velocity_at_point(const void_math::Vec3& world_point) const {
    // v = linear_vel + angular_vel x r
    void_math::Vec3 r = {
        world_point.x - m_position.x,
        world_point.y - m_position.y,
        world_point.z - m_position.z
    };

    // Cross product: angular_velocity x r
    void_math::Vec3 angular_component = {
        m_angular_velocity.y * r.z - m_angular_velocity.z * r.y,
        m_angular_velocity.z * r.x - m_angular_velocity.x * r.z,
        m_angular_velocity.x * r.y - m_angular_velocity.y * r.x
    };

    return {
        m_linear_velocity.x + angular_component.x,
        m_linear_velocity.y + angular_component.y,
        m_linear_velocity.z + angular_component.z
    };
}

float Rigidbody::inverse_mass() const {
    if (m_type != BodyType::Dynamic || m_mass_props.mass <= 0 ||
        m_mass_props.mass >= std::numeric_limits<float>::max() * 0.5f) {
        return 0.0f;
    }
    return 1.0f / m_mass_props.mass;
}

void Rigidbody::add_force(const void_math::Vec3& force, ForceMode mode) {
    if (m_type != BodyType::Dynamic) return;

    float inv_mass = inverse_mass();
    if (inv_mass == 0.0f) return;

    void_math::Vec3 applied = force;

    // Apply linear locks
    if (m_linear_lock[0]) applied.x = 0.0f;
    if (m_linear_lock[1]) applied.y = 0.0f;
    if (m_linear_lock[2]) applied.z = 0.0f;

    wake_up();

    switch (mode) {
        case ForceMode::Force:
            m_accumulated_force.x += applied.x;
            m_accumulated_force.y += applied.y;
            m_accumulated_force.z += applied.z;
            break;

        case ForceMode::Impulse:
            m_linear_velocity.x += applied.x * inv_mass;
            m_linear_velocity.y += applied.y * inv_mass;
            m_linear_velocity.z += applied.z * inv_mass;
            break;

        case ForceMode::Acceleration:
            m_accumulated_force.x += applied.x * m_mass_props.mass;
            m_accumulated_force.y += applied.y * m_mass_props.mass;
            m_accumulated_force.z += applied.z * m_mass_props.mass;
            break;

        case ForceMode::VelocityChange:
            m_linear_velocity.x += applied.x;
            m_linear_velocity.y += applied.y;
            m_linear_velocity.z += applied.z;
            break;
    }
}

void Rigidbody::add_force_at_position(const void_math::Vec3& force,
                                       const void_math::Vec3& position,
                                       ForceMode mode) {
    if (m_type != BodyType::Dynamic) return;

    // Add linear force
    add_force(force, mode);

    // Calculate torque: T = r x F
    void_math::Vec3 com = world_center_of_mass();
    void_math::Vec3 r = {
        position.x - com.x,
        position.y - com.y,
        position.z - com.z
    };

    void_math::Vec3 torque = {
        r.y * force.z - r.z * force.y,
        r.z * force.x - r.x * force.z,
        r.x * force.y - r.y * force.x
    };

    add_torque(torque, mode);
}

void Rigidbody::add_torque(const void_math::Vec3& torque, ForceMode mode) {
    if (m_type != BodyType::Dynamic) return;
    if (m_fixed_rotation) return;

    void_math::Vec3 applied = torque;

    // Apply angular locks
    if (m_angular_lock[0]) applied.x = 0.0f;
    if (m_angular_lock[1]) applied.y = 0.0f;
    if (m_angular_lock[2]) applied.z = 0.0f;

    wake_up();

    // Compute inverse inertia
    void_math::Vec3 inv_inertia = {
        m_mass_props.inertia_diagonal.x > 0 ? 1.0f / m_mass_props.inertia_diagonal.x : 0.0f,
        m_mass_props.inertia_diagonal.y > 0 ? 1.0f / m_mass_props.inertia_diagonal.y : 0.0f,
        m_mass_props.inertia_diagonal.z > 0 ? 1.0f / m_mass_props.inertia_diagonal.z : 0.0f
    };

    switch (mode) {
        case ForceMode::Force:
            m_accumulated_torque.x += applied.x;
            m_accumulated_torque.y += applied.y;
            m_accumulated_torque.z += applied.z;
            break;

        case ForceMode::Impulse:
            m_angular_velocity.x += applied.x * inv_inertia.x;
            m_angular_velocity.y += applied.y * inv_inertia.y;
            m_angular_velocity.z += applied.z * inv_inertia.z;
            break;

        case ForceMode::Acceleration:
            m_accumulated_torque.x += applied.x * m_mass_props.inertia_diagonal.x;
            m_accumulated_torque.y += applied.y * m_mass_props.inertia_diagonal.y;
            m_accumulated_torque.z += applied.z * m_mass_props.inertia_diagonal.z;
            break;

        case ForceMode::VelocityChange:
            m_angular_velocity.x += applied.x;
            m_angular_velocity.y += applied.y;
            m_angular_velocity.z += applied.z;
            break;
    }
}

void Rigidbody::add_relative_force(const void_math::Vec3& force, ForceMode mode) {
    void_math::Vec3 world_force = rotate_vector(force, m_rotation);
    add_force(world_force, mode);
}

void Rigidbody::add_relative_torque(const void_math::Vec3& torque, ForceMode mode) {
    void_math::Vec3 world_torque = rotate_vector(torque, m_rotation);
    add_torque(world_torque, mode);
}

void Rigidbody::clear_forces() {
    m_accumulated_force = {0, 0, 0};
    m_accumulated_torque = {0, 0, 0};
}

void Rigidbody::set_trigger(bool trigger) {
    m_collision_response = trigger ? CollisionResponse::Trigger : CollisionResponse::Collide;
}

void Rigidbody::wake_up() {
    if (m_activation_state == ActivationState::Sleeping) {
        m_activation_state = ActivationState::Active;
    }
    m_sleep_time = 0.0f;
}

void Rigidbody::sleep() {
    if (m_can_sleep && m_activation_state != ActivationState::AlwaysActive) {
        m_activation_state = ActivationState::Sleeping;
        m_linear_velocity = {0, 0, 0};
        m_angular_velocity = {0, 0, 0};
    }
}

void Rigidbody::lock_linear_axis(bool x, bool y, bool z) {
    m_linear_lock[0] = x;
    m_linear_lock[1] = y;
    m_linear_lock[2] = z;
}

void Rigidbody::lock_angular_axis(bool x, bool y, bool z) {
    m_angular_lock[0] = x;
    m_angular_lock[1] = y;
    m_angular_lock[2] = z;
}

ShapeId Rigidbody::add_shape(std::unique_ptr<IShape> shape) {
    ShapeId id{m_next_shape_id++};
    shape->set_id(id);
    m_shapes.push_back(std::move(shape));
    return id;
}

void Rigidbody::remove_shape(ShapeId shape_id) {
    auto it = std::remove_if(m_shapes.begin(), m_shapes.end(),
        [shape_id](const std::unique_ptr<IShape>& s) {
            return s->id() == shape_id;
        });
    m_shapes.erase(it, m_shapes.end());
}

IShape* Rigidbody::get_shape(std::size_t index) {
    if (index >= m_shapes.size()) return nullptr;
    return m_shapes[index].get();
}

const IShape* Rigidbody::get_shape(std::size_t index) const {
    if (index >= m_shapes.size()) return nullptr;
    return m_shapes[index].get();
}

IShape* Rigidbody::get_shape_by_id(ShapeId id) {
    for (auto& shape : m_shapes) {
        if (shape->id() == id) {
            return shape.get();
        }
    }
    return nullptr;
}

const IShape* Rigidbody::get_shape_by_id(ShapeId id) const {
    for (const auto& shape : m_shapes) {
        if (shape->id() == id) {
            return shape.get();
        }
    }
    return nullptr;
}

void_math::AABB Rigidbody::world_bounds() const {
    if (m_shapes.empty()) {
        return void_math::AABB{{0, 0, 0}, {0, 0, 0}};
    }

    // Start with first shape's bounds
    void_math::AABB result = transform_aabb(m_shapes[0]->local_bounds(), m_position, m_rotation);

    // Expand to include all other shapes
    for (std::size_t i = 1; i < m_shapes.size(); ++i) {
        auto shape_bounds = transform_aabb(m_shapes[i]->local_bounds(), m_position, m_rotation);
        result.min.x = std::min(result.min.x, shape_bounds.min.x);
        result.min.y = std::min(result.min.y, shape_bounds.min.y);
        result.min.z = std::min(result.min.z, shape_bounds.min.z);
        result.max.x = std::max(result.max.x, shape_bounds.max.x);
        result.max.y = std::max(result.max.y, shape_bounds.max.y);
        result.max.z = std::max(result.max.z, shape_bounds.max.z);
    }

    return result;
}

bool Rigidbody::contains_point(const void_math::Vec3& world_point) const {
    // Transform point to local space and check against shapes
    for (const auto& shape : m_shapes) {
        // Inverse transform the point
        void_math::Vec3 local_point = {
            world_point.x - m_position.x,
            world_point.y - m_position.y,
            world_point.z - m_position.z
        };

        // Inverse rotate (conjugate of quaternion)
        void_math::Quat inv_rot = {-m_rotation.x, -m_rotation.y, -m_rotation.z, m_rotation.w};
        local_point = rotate_vector(local_point, inv_rot);

        if (shape->contains_point(local_point)) {
            return true;
        }
    }
    return false;
}

void_math::Vec3 Rigidbody::closest_point(const void_math::Vec3& world_point) const {
    if (m_shapes.empty()) {
        return m_position;
    }

    void_math::Vec3 closest = m_position;
    float min_dist_sq = std::numeric_limits<float>::max();

    for (const auto& shape : m_shapes) {
        // Transform point to local space
        void_math::Vec3 local_point = {
            world_point.x - m_position.x,
            world_point.y - m_position.y,
            world_point.z - m_position.z
        };

        void_math::Quat inv_rot = {-m_rotation.x, -m_rotation.y, -m_rotation.z, m_rotation.w};
        local_point = rotate_vector(local_point, inv_rot);

        void_math::Vec3 local_closest = shape->closest_point(local_point);

        // Transform back to world space
        void_math::Vec3 world_closest = rotate_vector(local_closest, m_rotation);
        world_closest.x += m_position.x;
        world_closest.y += m_position.y;
        world_closest.z += m_position.z;

        float dx = world_closest.x - world_point.x;
        float dy = world_closest.y - world_point.y;
        float dz = world_closest.z - world_point.z;
        float dist_sq = dx * dx + dy * dy + dz * dz;

        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            closest = world_closest;
        }
    }

    return closest;
}

void Rigidbody::move_kinematic(const void_math::Vec3& target_position,
                                const void_math::Quat& target_rotation) {
    if (m_type != BodyType::Kinematic) return;

    m_kinematic_target_position = target_position;
    m_kinematic_target_rotation = target_rotation;

    // Set position/rotation directly for now
    // A full implementation would interpolate over the physics step
    m_position = target_position;
    m_rotation = target_rotation;
}

// =============================================================================
// Body Builder Implementation
// =============================================================================

std::unique_ptr<Rigidbody> BodyBuilder::build() {
    auto body = std::make_unique<Rigidbody>(m_config);

    // Add shapes
    for (auto& shape : m_shapes) {
        body->add_shape(std::move(shape));
    }

    return body;
}

// =============================================================================
// BodyConfig static methods
// =============================================================================

BodyConfig BodyConfig::make_static(const void_math::Vec3& pos) {
    BodyConfig config;
    config.type = BodyType::Static;
    config.position = pos;
    config.mass = MassProperties::infinite();
    config.allow_sleep = false;
    return config;
}

BodyConfig BodyConfig::make_kinematic(const void_math::Vec3& pos) {
    BodyConfig config;
    config.type = BodyType::Kinematic;
    config.position = pos;
    config.mass = MassProperties::infinite();
    return config;
}

BodyConfig BodyConfig::make_dynamic(const void_math::Vec3& pos, float mass) {
    BodyConfig config;
    config.type = BodyType::Dynamic;
    config.position = pos;
    config.mass = MassProperties::from_mass(mass);
    return config;
}

// =============================================================================
// MassProperties static methods
// =============================================================================

MassProperties MassProperties::from_mass(float mass) {
    MassProperties props;
    props.mass = mass;
    // Assume unit cube for simplicity
    float i = mass / 6.0f;
    props.inertia_diagonal = {i, i, i};
    return props;
}

MassProperties MassProperties::infinite() {
    MassProperties props;
    props.mass = std::numeric_limits<float>::max();
    props.inertia_diagonal = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    return props;
}

// =============================================================================
// PhysicsMaterialData presets
// =============================================================================

PhysicsMaterialData PhysicsMaterialData::ice() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.02f;
    mat.dynamic_friction = 0.01f;
    mat.restitution = 0.05f;
    mat.density = 917.0f; // kg/m^3
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::rubber() {
    PhysicsMaterialData mat;
    mat.static_friction = 1.0f;
    mat.dynamic_friction = 0.8f;
    mat.restitution = 0.9f;
    mat.density = 1100.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::metal() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.6f;
    mat.dynamic_friction = 0.4f;
    mat.restitution = 0.2f;
    mat.density = 7850.0f; // Steel
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::wood() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.5f;
    mat.dynamic_friction = 0.4f;
    mat.restitution = 0.3f;
    mat.density = 600.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::concrete() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.7f;
    mat.dynamic_friction = 0.6f;
    mat.restitution = 0.1f;
    mat.density = 2400.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::glass() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.4f;
    mat.dynamic_friction = 0.3f;
    mat.restitution = 0.6f;
    mat.density = 2500.0f;
    return mat;
}

PhysicsMaterialData PhysicsMaterialData::flesh() {
    PhysicsMaterialData mat;
    mat.static_friction = 0.4f;
    mat.dynamic_friction = 0.3f;
    mat.restitution = 0.1f;
    mat.density = 1050.0f;
    return mat;
}

// =============================================================================
// PhysicsConfig presets
// =============================================================================

PhysicsConfig PhysicsConfig::defaults() {
    return PhysicsConfig{};
}

PhysicsConfig PhysicsConfig::high_fidelity() {
    PhysicsConfig config;
    config.max_substeps = 8;
    config.fixed_timestep = 1.0f / 120.0f;
    config.velocity_iterations = 16;
    config.position_iterations = 6;
    return config;
}

PhysicsConfig PhysicsConfig::performance() {
    PhysicsConfig config;
    config.max_substeps = 2;
    config.fixed_timestep = 1.0f / 30.0f;
    config.velocity_iterations = 4;
    config.position_iterations = 2;
    return config;
}

// =============================================================================
// to_string implementations
// =============================================================================

const char* to_string(PhysicsBackend backend) {
    switch (backend) {
        case PhysicsBackend::Null:   return "Null";
        case PhysicsBackend::Jolt:   return "Jolt";
        case PhysicsBackend::PhysX:  return "PhysX";
        case PhysicsBackend::Bullet: return "Bullet";
        case PhysicsBackend::Custom: return "Custom";
        default: return "Unknown";
    }
}

const char* to_string(BodyType type) {
    switch (type) {
        case BodyType::Static:    return "Static";
        case BodyType::Kinematic: return "Kinematic";
        case BodyType::Dynamic:   return "Dynamic";
        default: return "Unknown";
    }
}

const char* to_string(ActivationState state) {
    switch (state) {
        case ActivationState::Active:       return "Active";
        case ActivationState::Sleeping:     return "Sleeping";
        case ActivationState::AlwaysActive: return "AlwaysActive";
        case ActivationState::Disabled:     return "Disabled";
        default: return "Unknown";
    }
}

const char* to_string(ShapeType type) {
    switch (type) {
        case ShapeType::Box:          return "Box";
        case ShapeType::Sphere:       return "Sphere";
        case ShapeType::Capsule:      return "Capsule";
        case ShapeType::Cylinder:     return "Cylinder";
        case ShapeType::Plane:        return "Plane";
        case ShapeType::ConvexHull:   return "ConvexHull";
        case ShapeType::TriangleMesh: return "TriangleMesh";
        case ShapeType::Heightfield:  return "Heightfield";
        case ShapeType::Compound:     return "Compound";
        default: return "Unknown";
    }
}

const char* to_string(JointType type) {
    switch (type) {
        case JointType::Fixed:    return "Fixed";
        case JointType::Hinge:    return "Hinge";
        case JointType::Slider:   return "Slider";
        case JointType::Ball:     return "Ball";
        case JointType::Distance: return "Distance";
        case JointType::Spring:   return "Spring";
        case JointType::Cone:     return "Cone";
        case JointType::Generic:  return "Generic";
        default: return "Unknown";
    }
}

} // namespace void_physics
