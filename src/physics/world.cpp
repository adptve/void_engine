/// @file world.cpp
/// @brief Physics world implementations for void_physics

#include <void_engine/physics/world.hpp>
#include <void_engine/physics/body.hpp>
#include <void_engine/physics/shape.hpp>

#include <algorithm>
#include <cmath>

namespace void_physics {

// =============================================================================
// PhysicsWorld Implementation
// =============================================================================

PhysicsWorld::PhysicsWorld(PhysicsConfig config)
    : m_config(std::move(config)) {
    // Create default material
    PhysicsMaterialData default_mat;
    m_default_material = create_material(default_mat);
}

PhysicsWorld::~PhysicsWorld() {
    clear();
}

void PhysicsWorld::step(float dt) {
    if (dt <= 0) return;

    // Fixed timestep accumulation
    m_time_accumulator += dt;

    std::uint32_t substeps = 0;
    while (m_time_accumulator >= m_config.fixed_timestep && substeps < m_config.max_substeps) {
        integrate_velocities(m_config.fixed_timestep);
        detect_collisions();
        solve_constraints(m_config.fixed_timestep);
        integrate_positions(m_config.fixed_timestep);
        update_sleep_states(m_config.fixed_timestep);
        fire_collision_events();

        m_time_accumulator -= m_config.fixed_timestep;
        ++substeps;
    }

    // Clamp accumulator to prevent spiral of death
    if (m_time_accumulator > m_config.fixed_timestep * 4) {
        m_time_accumulator = m_config.fixed_timestep * 4;
    }
}

void PhysicsWorld::step_with_substeps(float dt, std::uint32_t substeps) {
    if (dt <= 0 || substeps == 0) return;

    float substep_dt = dt / static_cast<float>(substeps);
    for (std::uint32_t i = 0; i < substeps; ++i) {
        integrate_velocities(substep_dt);
        detect_collisions();
        solve_constraints(substep_dt);
        integrate_positions(substep_dt);
        update_sleep_states(substep_dt);
        fire_collision_events();
    }
}

BodyId PhysicsWorld::create_body(const BodyConfig& config) {
    // Rigidbody constructor uses config.user_id as the body ID
    // We create a modified config with our generated ID
    BodyConfig mod_config = config;
    mod_config.user_id = m_next_body_id++;

    auto body = std::make_unique<Rigidbody>(mod_config);
    BodyId id = body->id();
    m_bodies[id.value] = std::move(body);
    return id;
}

BodyId PhysicsWorld::create_body(BodyBuilder& builder) {
    // builder.build() returns unique_ptr<Rigidbody>
    auto body = builder.build();
    if (!body) {
        return BodyId::invalid();
    }
    BodyId id = body->id();
    m_bodies[id.value] = std::move(body);
    return id;
}

void PhysicsWorld::destroy_body(BodyId id) {
    m_bodies.erase(id.value);
}

IRigidbody* PhysicsWorld::get_body(BodyId id) {
    auto it = m_bodies.find(id.value);
    return it != m_bodies.end() ? it->second.get() : nullptr;
}

const IRigidbody* PhysicsWorld::get_body(BodyId id) const {
    auto it = m_bodies.find(id.value);
    return it != m_bodies.end() ? it->second.get() : nullptr;
}

bool PhysicsWorld::body_exists(BodyId id) const {
    return m_bodies.find(id.value) != m_bodies.end();
}

void PhysicsWorld::for_each_body(std::function<void(IRigidbody&)> callback) {
    for (auto& [id, body] : m_bodies) {
        if (body) callback(*body);
    }
}

void PhysicsWorld::for_each_body(std::function<void(const IRigidbody&)> callback) const {
    for (const auto& [id, body] : m_bodies) {
        if (body) callback(*body);
    }
}

// Joint stubs
JointId PhysicsWorld::create_joint(const JointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = config.type;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;
    return id;
}

JointId PhysicsWorld::create_hinge_joint(const HingeJointConfig& config) {
    return create_joint(config);
}

JointId PhysicsWorld::create_slider_joint(const SliderJointConfig& config) {
    return create_joint(config);
}

JointId PhysicsWorld::create_ball_joint(const BallJointConfig& config) {
    return create_joint(config);
}

JointId PhysicsWorld::create_distance_joint(const DistanceJointConfig& config) {
    return create_joint(config);
}

JointId PhysicsWorld::create_spring_joint(const SpringJointConfig& config) {
    return create_joint(config);
}

void PhysicsWorld::destroy_joint(JointId id) {
    m_joints.erase(id.value);
}

std::size_t PhysicsWorld::joint_count() const {
    return m_joints.size();
}

// Materials
MaterialId PhysicsWorld::create_material(const PhysicsMaterialData& data) {
    MaterialId id{m_next_material_id++};
    m_materials[id.value] = data;
    return id;
}

const PhysicsMaterialData* PhysicsWorld::get_material(MaterialId id) const {
    auto it = m_materials.find(id.value);
    return it != m_materials.end() ? &it->second : nullptr;
}

void PhysicsWorld::update_material(MaterialId id, const PhysicsMaterialData& data) {
    auto it = m_materials.find(id.value);
    if (it != m_materials.end()) {
        it->second = data;
    }
}

// Raycast stubs
RaycastHit PhysicsWorld::raycast(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    // Stub - return no hit
    RaycastHit hit;
    hit.hit = false;
    return hit;
}

std::vector<RaycastHit> PhysicsWorld::raycast_all(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    // Stub
    return {};
}

void PhysicsWorld::raycast_callback(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask,
    std::function<bool(const RaycastHit&)> callback) const {
    // Stub
}

// Shape cast stubs
ShapeCastHit PhysicsWorld::shape_cast(
    const IShape& shape,
    const void_math::Transform& start,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    ShapeCastHit hit;
    hit.hit = false;
    return hit;
}

ShapeCastHit PhysicsWorld::sphere_cast(
    float radius,
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    ShapeCastHit hit;
    hit.hit = false;
    return hit;
}

ShapeCastHit PhysicsWorld::box_cast(
    const void_math::Vec3& half_extents,
    const void_math::Transform& start,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    ShapeCastHit hit;
    hit.hit = false;
    return hit;
}

ShapeCastHit PhysicsWorld::capsule_cast(
    float radius,
    float height,
    const void_math::Transform& start,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    ShapeCastHit hit;
    hit.hit = false;
    return hit;
}

// Overlap stubs
bool PhysicsWorld::overlap_test(
    const IShape& shape,
    const void_math::Transform& transform,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    return false;
}

std::vector<OverlapResult> PhysicsWorld::overlap_all(
    const IShape& shape,
    const void_math::Transform& transform,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    return {};
}

std::vector<OverlapResult> PhysicsWorld::overlap_sphere(
    const void_math::Vec3& center,
    float radius,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    return {};
}

std::vector<OverlapResult> PhysicsWorld::overlap_box(
    const void_math::Vec3& center,
    const void_math::Vec3& half_extents,
    const void_math::Quat& rotation,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    return {};
}

// Point query stubs
BodyId PhysicsWorld::closest_body(
    const void_math::Vec3& point,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    return BodyId::invalid();
}

std::vector<BodyId> PhysicsWorld::bodies_at_point(
    const void_math::Vec3& point,
    QueryFilter filter,
    CollisionLayer layer_mask) const {
    return {};
}

// Stats
PhysicsStats PhysicsWorld::stats() const {
    // Count bodies by type and state
    PhysicsStats result = m_stats;
    result.dynamic_bodies = 0;
    result.static_bodies = 0;
    result.kinematic_bodies = 0;
    result.active_bodies = 0;
    result.sleeping_bodies = 0;

    for (const auto& [id, body] : m_bodies) {
        if (!body) continue;

        switch (body->type()) {
            case BodyType::Dynamic:
                ++result.dynamic_bodies;
                break;
            case BodyType::Static:
                ++result.static_bodies;
                break;
            case BodyType::Kinematic:
                ++result.kinematic_bodies;
                break;
        }

        if (body->is_sleeping()) {
            ++result.sleeping_bodies;
        } else {
            ++result.active_bodies;
        }
    }

    result.active_joints = static_cast<std::uint32_t>(m_joints.size());
    return result;
}

// Debug
PhysicsDebugRenderer* PhysicsWorld::debug_renderer() {
    return m_debug_renderer.get();
}

// Serialization stubs
void_core::Result<void_core::HotReloadSnapshot> PhysicsWorld::snapshot() const {
    return void_core::HotReloadSnapshot{};
}

void_core::Result<void> PhysicsWorld::restore(void_core::HotReloadSnapshot snapshot) {
    return void_core::Result<void>::ok();
}

// Clear
void PhysicsWorld::clear() {
    m_bodies.clear();
    m_joints.clear();
    m_collision_pairs.clear();
    m_trigger_pairs.clear();
}

// Private implementation
void PhysicsWorld::integrate_velocities(float dt) {
    for (auto& [id, body] : m_bodies) {
        if (body && body->type() == BodyType::Dynamic && !body->is_sleeping()) {
            // Apply gravity
            body->add_force(m_config.gravity * body->mass(), ForceMode::Force);
        }
    }
}

void PhysicsWorld::detect_collisions() {
    // Stub - collision detection would go here
}

void PhysicsWorld::solve_constraints(float dt) {
    // Stub - constraint solving would go here
}

void PhysicsWorld::integrate_positions(float dt) {
    for (auto& [id, body] : m_bodies) {
        if (body && !body->is_sleeping() && body->type() != BodyType::Static) {
            // Integrate position: p' = p + v * dt
            void_math::Vec3 pos = body->position();
            void_math::Vec3 vel = body->linear_velocity();
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            pos.z += vel.z * dt;
            body->set_position(pos);

            // Integrate rotation: simple euler for angular velocity
            void_math::Vec3 ang_vel = body->angular_velocity();
            float angle = std::sqrt(ang_vel.x * ang_vel.x + ang_vel.y * ang_vel.y + ang_vel.z * ang_vel.z) * dt;
            if (angle > 0.0001f) {
                void_math::Vec3 axis = {ang_vel.x / (angle / dt), ang_vel.y / (angle / dt), ang_vel.z / (angle / dt)};
                // Create rotation quaternion from axis-angle
                float half_angle = angle * 0.5f;
                float s = std::sin(half_angle);
                void_math::Quat delta_rot = {axis.x * s, axis.y * s, axis.z * s, std::cos(half_angle)};
                // Multiply current rotation by delta
                void_math::Quat cur = body->rotation();
                void_math::Quat new_rot = {
                    cur.w * delta_rot.x + cur.x * delta_rot.w + cur.y * delta_rot.z - cur.z * delta_rot.y,
                    cur.w * delta_rot.y - cur.x * delta_rot.z + cur.y * delta_rot.w + cur.z * delta_rot.x,
                    cur.w * delta_rot.z + cur.x * delta_rot.y - cur.y * delta_rot.x + cur.z * delta_rot.w,
                    cur.w * delta_rot.w - cur.x * delta_rot.x - cur.y * delta_rot.y - cur.z * delta_rot.z
                };
                // Normalize
                float len = std::sqrt(new_rot.x * new_rot.x + new_rot.y * new_rot.y +
                                       new_rot.z * new_rot.z + new_rot.w * new_rot.w);
                if (len > 0.0f) {
                    new_rot.x /= len; new_rot.y /= len; new_rot.z /= len; new_rot.w /= len;
                }
                body->set_rotation(new_rot);
            }

            // Clear accumulated forces after integration
            body->clear_forces();
        }
    }
}

void PhysicsWorld::update_sleep_states(float dt) {
    // Stub - sleep state management would go here
}

void PhysicsWorld::fire_collision_events() {
    // Stub - collision event firing would go here
}

bool PhysicsWorld::passes_filter(const IRigidbody& body, QueryFilter filter, CollisionLayer layer_mask) const {
    // Check layer mask
    if ((body.collision_mask().layer & layer_mask) == 0) {
        return false;
    }

    // Check filter
    switch (filter) {
        case QueryFilter::Default:
            return true;
        case QueryFilter::Static:
            return body.type() == BodyType::Static;
        case QueryFilter::Dynamic:
            return body.type() == BodyType::Dynamic;
        case QueryFilter::Kinematic:
            return body.type() == BodyType::Kinematic;
        case QueryFilter::All:
            return true;
        default:
            return true;
    }
}

// =============================================================================
// CharacterController Implementation
// =============================================================================

CharacterController::CharacterController(IPhysicsWorld& world, const CharacterControllerConfig& config)
    : m_world(world)
    , m_config(config) {
}

CharacterController::~CharacterController() = default;

void CharacterController::move(const void_math::Vec3& displacement, float dt) {
    // Simple movement with collision
    void_math::Vec3 final_displacement = slide_move(displacement);
    m_position = m_position + final_displacement;
    m_velocity = (dt > 0) ? final_displacement * (1.0f / dt) : void_math::Vec3{0, 0, 0};
    update_grounded();
}

void CharacterController::resize(float height, float radius) {
    m_config.height = height;
    m_config.radius = radius;
}

void_math::Vec3 CharacterController::slide_move(const void_math::Vec3& displacement) {
    // Stub - would perform collision detection and sliding
    return displacement;
}

void CharacterController::update_grounded() {
    // Stub - would check if grounded
    m_grounded = false;
}

// =============================================================================
// PhysicsDebugRenderer Implementation
// =============================================================================

void PhysicsDebugRenderer::draw_box(
    const void_math::Vec3& center,
    const void_math::Vec3& half_extents,
    const void_math::Quat& rotation,
    std::uint32_t color) {
    // Draw 12 edges of box
    // Stub implementation
}

void PhysicsDebugRenderer::draw_sphere(
    const void_math::Vec3& center,
    float radius,
    std::uint32_t color) {
    // Stub implementation
}

void PhysicsDebugRenderer::draw_capsule(
    const void_math::Vec3& p1,
    const void_math::Vec3& p2,
    float radius,
    std::uint32_t color) {
    // Stub implementation
}

void PhysicsDebugRenderer::draw_arrow(
    const void_math::Vec3& from,
    const void_math::Vec3& to,
    std::uint32_t color) {
    draw_line(from, to, color);
}

void PhysicsDebugRenderer::draw_contact(
    const void_math::Vec3& position,
    const void_math::Vec3& normal,
    float depth,
    std::uint32_t color) {
    // Stub implementation
}

void PhysicsDebugRenderer::draw_body(const IRigidbody& body) {
    // Stub implementation
}

void PhysicsDebugRenderer::draw_world(const IPhysicsWorld& world) {
    // Stub implementation
}

} // namespace void_physics
