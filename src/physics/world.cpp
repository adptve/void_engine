/// @file world.cpp
/// @brief Physics world implementation using simulation pipeline

#include <void_engine/physics/world.hpp>
#include <void_engine/physics/body.hpp>
#include <void_engine/physics/shape.hpp>
#include <void_engine/physics/simulation.hpp>
#include <void_engine/physics/query.hpp>
#include <void_engine/physics/snapshot.hpp>

#include <algorithm>
#include <cmath>

namespace void_physics {

// =============================================================================
// PhysicsWorld Implementation
// =============================================================================

PhysicsWorld::PhysicsWorld(PhysicsConfig config)
    : m_config(std::move(config))
    , m_pipeline(std::make_unique<PhysicsPipeline>(m_config))
    , m_query_system(std::make_unique<QuerySystem>())
{
    // Create default material
    PhysicsMaterialData default_mat;
    m_default_material = create_material(default_mat);

    // Setup query system
    m_query_system->set_broadphase(&m_pipeline->broadphase());
    m_query_system->set_body_accessor([this](BodyId id) -> IRigidbody* {
        return get_body(id);
    });
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
        // Use simulation pipeline
        m_pipeline->step(m_bodies, m_joint_constraints, m_materials, m_default_material,
                        m_config.fixed_timestep, m_stats);

        // Fire events
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
        m_pipeline->step(m_bodies, m_joint_constraints, m_materials, m_default_material,
                        substep_dt, m_stats);
        fire_collision_events();
    }
}

BodyId PhysicsWorld::create_body(const BodyConfig& config) {
    BodyId id{m_next_body_id++};

    // Create body with assigned ID
    auto body = std::make_unique<Rigidbody>(config);
    // Override the ID
    // Note: In production, Rigidbody constructor should accept ID
    m_bodies[id.value] = std::move(body);

    return id;
}

BodyId PhysicsWorld::create_body(BodyBuilder& builder) {
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

// Joints
JointId PhysicsWorld::create_joint(const JointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = config.type;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;

    // Create constraint for solver
    m_joint_constraints.push_back(std::make_unique<FixedJointConstraint>(id, config));

    return id;
}

JointId PhysicsWorld::create_hinge_joint(const HingeJointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = JointType::Hinge;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;

    m_joint_constraints.push_back(std::make_unique<HingeJointConstraint>(id, config));

    return id;
}

JointId PhysicsWorld::create_slider_joint(const SliderJointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = JointType::Slider;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;

    // Slider uses fixed constraint as placeholder
    JointConfig base_config;
    base_config.body_a = config.body_a;
    base_config.body_b = config.body_b;
    base_config.anchor_a = config.anchor_a;
    base_config.anchor_b = config.anchor_b;
    m_joint_constraints.push_back(std::make_unique<FixedJointConstraint>(id, base_config));

    return id;
}

JointId PhysicsWorld::create_ball_joint(const BallJointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = JointType::Ball;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;

    m_joint_constraints.push_back(std::make_unique<BallJointConstraint>(id, config));

    return id;
}

JointId PhysicsWorld::create_distance_joint(const DistanceJointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = JointType::Distance;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;

    m_joint_constraints.push_back(std::make_unique<DistanceJointConstraint>(id, config));

    return id;
}

JointId PhysicsWorld::create_spring_joint(const SpringJointConfig& config) {
    JointId id{m_next_joint_id++};
    JointData data;
    data.type = JointType::Spring;
    data.body_a = config.body_a;
    data.body_b = config.body_b;
    m_joints[id.value] = data;

    m_joint_constraints.push_back(std::make_unique<SpringJointConstraint>(id, config));

    return id;
}

void PhysicsWorld::destroy_joint(JointId id) {
    m_joints.erase(id.value);

    // Remove constraint
    m_joint_constraints.erase(
        std::remove_if(m_joint_constraints.begin(), m_joint_constraints.end(),
            [id](const auto& c) { return c->id() == id; }),
        m_joint_constraints.end());
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

// Query forwarding to QuerySystem
RaycastHit PhysicsWorld::raycast(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->raycast(origin, direction, max_distance, filter, layer_mask);
}

std::vector<RaycastHit> PhysicsWorld::raycast_all(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->raycast_all(origin, direction, max_distance, filter, layer_mask);
}

void PhysicsWorld::raycast_callback(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask,
    std::function<bool(const RaycastHit&)> callback) const
{
    m_query_system->raycast_callback(origin, direction, max_distance, filter, layer_mask, std::move(callback));
}

ShapeCastHit PhysicsWorld::shape_cast(
    const IShape& shape,
    const void_math::Transform& start,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->shape_cast(shape, start, direction, max_distance, filter, layer_mask);
}

ShapeCastHit PhysicsWorld::sphere_cast(
    float radius,
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->sphere_cast(radius, origin, direction, max_distance, filter, layer_mask);
}

ShapeCastHit PhysicsWorld::box_cast(
    const void_math::Vec3& half_extents,
    const void_math::Transform& start,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->box_cast(half_extents, start, direction, max_distance, filter, layer_mask);
}

ShapeCastHit PhysicsWorld::capsule_cast(
    float radius,
    float height,
    const void_math::Transform& start,
    const void_math::Vec3& direction,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->capsule_cast(radius, height, start, direction, max_distance, filter, layer_mask);
}

bool PhysicsWorld::overlap_test(
    const IShape& shape,
    const void_math::Transform& transform,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->overlap_test(shape, transform, filter, layer_mask);
}

std::vector<OverlapResult> PhysicsWorld::overlap_all(
    const IShape& shape,
    const void_math::Transform& transform,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->overlap_all(shape, transform, filter, layer_mask);
}

std::vector<OverlapResult> PhysicsWorld::overlap_sphere(
    const void_math::Vec3& center,
    float radius,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->overlap_sphere(center, radius, filter, layer_mask);
}

std::vector<OverlapResult> PhysicsWorld::overlap_box(
    const void_math::Vec3& center,
    const void_math::Vec3& half_extents,
    const void_math::Quat& rotation,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->overlap_box(center, half_extents, rotation, filter, layer_mask);
}

BodyId PhysicsWorld::closest_body(
    const void_math::Vec3& point,
    float max_distance,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->closest_body(point, max_distance, filter, layer_mask);
}

std::vector<BodyId> PhysicsWorld::bodies_at_point(
    const void_math::Vec3& point,
    QueryFilter filter,
    CollisionLayer layer_mask) const
{
    return m_query_system->bodies_at_point(point, filter, layer_mask);
}

// Stats
PhysicsStats PhysicsWorld::stats() const {
    return m_stats;
}

// Debug
PhysicsDebugRenderer* PhysicsWorld::debug_renderer() {
    return m_debug_renderer.get();
}

// Serialization
void_core::Result<void_core::HotReloadSnapshot> PhysicsWorld::snapshot() const {
    PhysicsWorldSnapshot snap;
    snap.config = m_config;
    snap.default_material = m_default_material;
    snap.next_body_id = m_next_body_id;
    snap.next_joint_id = m_next_joint_id;
    snap.next_material_id = m_next_material_id;
    snap.time_accumulator = m_time_accumulator;

    // Capture bodies
    for (const auto& [id, body] : m_bodies) {
        if (body) {
            snap.bodies.push_back(BodySnapshot::capture(*body));

            // Capture shapes
            std::vector<ShapeSnapshot> shapes;
            for (std::size_t i = 0; i < body->shape_count(); ++i) {
                if (const auto* shape = body->get_shape(i)) {
                    shapes.push_back(ShapeSnapshot::capture(*shape, ShapeId{i + 1}));
                }
            }
            snap.body_shapes.emplace_back(BodyId{id}, std::move(shapes));
        }
    }

    // Capture materials
    for (const auto& [id, mat] : m_materials) {
        MaterialSnapshot mat_snap;
        mat_snap.id = MaterialId{id};
        mat_snap.data = mat;
        snap.materials.push_back(mat_snap);
    }

    // Serialize
    auto data = snap.serialize();

    return void_core::HotReloadSnapshot{
        std::move(data),
        std::type_index(typeid(PhysicsWorld)),
        "void_physics::PhysicsWorld",
        void_core::Version{1, 0, 0}
    };
}

void_core::Result<void> PhysicsWorld::restore(void_core::HotReloadSnapshot snapshot) {
    auto snap_opt = PhysicsWorldSnapshot::deserialize(snapshot.data);
    if (!snap_opt) {
        return void_core::Err("Failed to deserialize physics snapshot");
    }

    auto& snap = *snap_opt;

    // Clear current state
    clear();

    // Restore config
    m_config = snap.config;
    m_default_material = snap.default_material;
    m_next_body_id = snap.next_body_id;
    m_next_joint_id = snap.next_joint_id;
    m_next_material_id = snap.next_material_id;
    m_time_accumulator = snap.time_accumulator;

    // Restore materials
    for (const auto& mat_snap : snap.materials) {
        m_materials[mat_snap.id.value] = mat_snap.data;
    }

    // Restore bodies
    for (const auto& body_snap : snap.bodies) {
        BodyConfig config;
        config.name = body_snap.name;
        config.type = body_snap.type;
        config.position = body_snap.position;
        config.rotation = body_snap.rotation;
        config.linear_velocity = body_snap.linear_velocity;
        config.angular_velocity = body_snap.angular_velocity;
        config.mass = body_snap.mass_props;
        config.collision_mask = body_snap.collision_mask;
        config.response = body_snap.collision_response;
        config.linear_damping = body_snap.linear_damping;
        config.angular_damping = body_snap.angular_damping;
        config.gravity_scale = body_snap.gravity_scale;
        config.continuous_detection = body_snap.ccd_enabled;
        config.allow_sleep = body_snap.can_sleep;
        config.fixed_rotation = body_snap.fixed_rotation;
        config.user_id = body_snap.user_id;

        auto body = std::make_unique<Rigidbody>(config);
        body_snap.restore_to(*body);
        m_bodies[body_snap.id.value] = std::move(body);
    }

    // Restore shapes
    for (const auto& [body_id, shapes] : snap.body_shapes) {
        auto* body = get_body(body_id);
        if (body) {
            for (const auto& shape_snap : shapes) {
                auto shape = shape_snap.create_shape();
                if (shape) {
                    body->add_shape(std::move(shape));
                }
            }
        }
    }

    // Reinitialize pipeline
    m_pipeline = std::make_unique<PhysicsPipeline>(m_config);
    m_query_system->set_broadphase(&m_pipeline->broadphase());

    return void_core::Ok();
}

// Clear
void PhysicsWorld::clear() {
    m_bodies.clear();
    m_joints.clear();
    m_joint_constraints.clear();
    m_collision_pairs.clear();
    m_trigger_pairs.clear();
}

// Private implementation
void PhysicsWorld::fire_collision_events() {
    // Get events from pipeline
    const auto& collision_events = m_pipeline->collision_events();
    const auto& trigger_events = m_pipeline->trigger_events();

    // Fire collision callbacks
    for (const auto& event : collision_events) {
        switch (event.type) {
            case CollisionEvent::Type::Begin:
                if (m_on_collision_begin) m_on_collision_begin(event);
                break;
            case CollisionEvent::Type::Stay:
                if (m_on_collision_stay) m_on_collision_stay(event);
                break;
            case CollisionEvent::Type::End:
                if (m_on_collision_end) m_on_collision_end(event);
                break;
        }
    }

    // Fire trigger callbacks
    for (const auto& event : trigger_events) {
        switch (event.type) {
            case TriggerEvent::Type::Enter:
                if (m_on_trigger_enter) m_on_trigger_enter(event);
                break;
            case TriggerEvent::Type::Stay:
                if (m_on_trigger_stay) m_on_trigger_stay(event);
                break;
            case TriggerEvent::Type::Exit:
                if (m_on_trigger_exit) m_on_trigger_exit(event);
                break;
        }
    }
}

bool PhysicsWorld::passes_filter(const IRigidbody& body, QueryFilter filter, CollisionLayer layer_mask) const {
    // Check layer mask
    if ((body.collision_mask().layer & layer_mask) == 0) {
        return false;
    }

    // Check filter flags
    if (has_flag(filter, QueryFilter::Static) && body.type() == BodyType::Static) return true;
    if (has_flag(filter, QueryFilter::Dynamic) && body.type() == BodyType::Dynamic) return true;
    if (has_flag(filter, QueryFilter::Kinematic) && body.type() == BodyType::Kinematic) return true;
    if (has_flag(filter, QueryFilter::Triggers) && body.is_trigger()) return true;

    return false;
}

// =============================================================================
// CharacterController Implementation
// =============================================================================

CharacterController::CharacterController(IPhysicsWorld& world, const CharacterControllerConfig& config)
    : m_world(world)
    , m_config(config)
{
}

CharacterController::~CharacterController() = default;

void CharacterController::move(const void_math::Vec3& displacement, float dt) {
    // Apply sliding movement with collision response
    auto result = slide_move(displacement);
    m_position = m_position + result;
    m_velocity = result / dt;

    // Update ground state
    update_grounded();
}

void CharacterController::resize(float height, float radius) {
    m_config.height = height;
    m_config.radius = radius;
}

void_math::Vec3 CharacterController::slide_move(const void_math::Vec3& displacement) {
    // Simple implementation: just return displacement for now
    // TODO: Implement proper collision response with slide
    m_collides_sides = false;
    m_collides_above = false;

    // Cast capsule shape along displacement
    CapsuleShape capsule(m_config.radius, m_config.height);
    void_math::Transform start;
    start.position = m_position;

    auto hit = m_world.shape_cast(
        capsule, start, void_math::normalize(displacement),
        void_math::length(displacement),
        QueryFilter::Dynamic, m_config.collision_mask.layer);

    if (hit.hit) {
        // Hit something - slide along surface
        float safe_distance = hit.distance - m_config.skin_width;
        if (safe_distance < 0.0f) safe_distance = 0.0f;

        void_math::Vec3 safe_move = void_math::normalize(displacement) * safe_distance;

        // Check if we hit above (ceiling) or sides
        if (hit.normal.y < -0.7f) {
            m_collides_above = true;
        } else if (std::abs(hit.normal.y) < 0.7f) {
            m_collides_sides = true;
        }

        // Slide along the surface
        void_math::Vec3 remaining = displacement - safe_move;
        void_math::Vec3 slide = remaining - hit.normal * void_math::dot(remaining, hit.normal);

        return safe_move + slide * (1.0f - m_config.skin_width);
    }

    return displacement;
}

void CharacterController::update_grounded() {
    // Cast downward to check if grounded
    SphereShape foot(m_config.radius * 0.9f);
    void_math::Transform start;
    start.position = m_position - void_math::Vec3{0, m_config.height * 0.5f - m_config.radius, 0};

    auto hit = m_world.shape_cast(
        foot, start, void_math::Vec3{0, -1, 0},
        m_config.step_height + m_config.skin_width,
        QueryFilter::Static | QueryFilter::Dynamic, m_config.collision_mask.layer);

    if (hit.hit) {
        m_grounded = true;
        m_ground_normal = hit.normal;
    } else {
        m_grounded = false;
        m_ground_normal = void_math::Vec3{0, 1, 0};
    }
}

// =============================================================================
// PhysicsDebugRenderer Implementation
// =============================================================================

void PhysicsDebugRenderer::draw_box(
    const void_math::Vec3& center,
    const void_math::Vec3& half_extents,
    const void_math::Quat& rotation,
    std::uint32_t color)
{
    // Compute 8 corners
    void_math::Vec3 corners[8] = {
        {-half_extents.x, -half_extents.y, -half_extents.z},
        { half_extents.x, -half_extents.y, -half_extents.z},
        { half_extents.x,  half_extents.y, -half_extents.z},
        {-half_extents.x,  half_extents.y, -half_extents.z},
        {-half_extents.x, -half_extents.y,  half_extents.z},
        { half_extents.x, -half_extents.y,  half_extents.z},
        { half_extents.x,  half_extents.y,  half_extents.z},
        {-half_extents.x,  half_extents.y,  half_extents.z},
    };

    // Transform corners
    for (auto& c : corners) {
        c = void_math::rotate(rotation, c) + center;
    }

    // Draw 12 edges
    draw_line(corners[0], corners[1], color);
    draw_line(corners[1], corners[2], color);
    draw_line(corners[2], corners[3], color);
    draw_line(corners[3], corners[0], color);

    draw_line(corners[4], corners[5], color);
    draw_line(corners[5], corners[6], color);
    draw_line(corners[6], corners[7], color);
    draw_line(corners[7], corners[4], color);

    draw_line(corners[0], corners[4], color);
    draw_line(corners[1], corners[5], color);
    draw_line(corners[2], corners[6], color);
    draw_line(corners[3], corners[7], color);
}

void PhysicsDebugRenderer::draw_sphere(
    const void_math::Vec3& center,
    float radius,
    std::uint32_t color)
{
    constexpr int segments = 16;
    constexpr float pi = 3.14159265f;

    // Draw 3 circles (XY, XZ, YZ planes)
    for (int i = 0; i < segments; ++i) {
        float a1 = (static_cast<float>(i) / segments) * 2.0f * pi;
        float a2 = (static_cast<float>(i + 1) / segments) * 2.0f * pi;

        // XY plane
        draw_line(
            center + void_math::Vec3{std::cos(a1) * radius, std::sin(a1) * radius, 0},
            center + void_math::Vec3{std::cos(a2) * radius, std::sin(a2) * radius, 0},
            color);

        // XZ plane
        draw_line(
            center + void_math::Vec3{std::cos(a1) * radius, 0, std::sin(a1) * radius},
            center + void_math::Vec3{std::cos(a2) * radius, 0, std::sin(a2) * radius},
            color);

        // YZ plane
        draw_line(
            center + void_math::Vec3{0, std::cos(a1) * radius, std::sin(a1) * radius},
            center + void_math::Vec3{0, std::cos(a2) * radius, std::sin(a2) * radius},
            color);
    }
}

void PhysicsDebugRenderer::draw_capsule(
    const void_math::Vec3& p1,
    const void_math::Vec3& p2,
    float radius,
    std::uint32_t color)
{
    // Draw cylinder
    auto axis = p2 - p1;
    float length = void_math::length(axis);
    if (length < 0.0001f) {
        draw_sphere((p1 + p2) * 0.5f, radius, color);
        return;
    }

    axis = axis / length;

    // Find perpendicular axes
    void_math::Vec3 perp1, perp2;
    if (std::abs(axis.x) > 0.9f) {
        perp1 = void_math::normalize(void_math::cross(axis, void_math::Vec3{0, 1, 0}));
    } else {
        perp1 = void_math::normalize(void_math::cross(axis, void_math::Vec3{1, 0, 0}));
    }
    perp2 = void_math::cross(axis, perp1);

    constexpr int segments = 8;
    constexpr float pi = 3.14159265f;

    // Draw cylinder lines
    for (int i = 0; i < segments; ++i) {
        float a = (static_cast<float>(i) / segments) * 2.0f * pi;
        auto offset = perp1 * std::cos(a) * radius + perp2 * std::sin(a) * radius;
        draw_line(p1 + offset, p2 + offset, color);
    }

    // Draw caps
    draw_sphere(p1, radius, color);
    draw_sphere(p2, radius, color);
}

void PhysicsDebugRenderer::draw_arrow(
    const void_math::Vec3& from,
    const void_math::Vec3& to,
    std::uint32_t color)
{
    draw_line(from, to, color);

    auto dir = to - from;
    float len = void_math::length(dir);
    if (len < 0.0001f) return;

    dir = dir / len;
    float head_size = len * 0.1f;

    // Find perpendicular
    void_math::Vec3 perp;
    if (std::abs(dir.x) > 0.9f) {
        perp = void_math::normalize(void_math::cross(dir, void_math::Vec3{0, 1, 0}));
    } else {
        perp = void_math::normalize(void_math::cross(dir, void_math::Vec3{1, 0, 0}));
    }

    auto head_base = to - dir * head_size;
    draw_line(to, head_base + perp * head_size * 0.5f, color);
    draw_line(to, head_base - perp * head_size * 0.5f, color);
}

void PhysicsDebugRenderer::draw_contact(
    const void_math::Vec3& position,
    const void_math::Vec3& normal,
    float depth,
    std::uint32_t color)
{
    draw_sphere(position, 0.02f, color);
    draw_arrow(position, position + normal * 0.1f, Colors::ContactNormal);
}

void PhysicsDebugRenderer::draw_body(const IRigidbody& body) {
    std::uint32_t color;
    switch (body.type()) {
        case BodyType::Static:
            color = Colors::StaticBody;
            break;
        case BodyType::Kinematic:
            color = Colors::KinematicBody;
            break;
        case BodyType::Dynamic:
            color = body.is_sleeping() ? Colors::SleepingBody : Colors::DynamicBody;
            break;
    }

    auto pos = body.position();
    auto rot = body.rotation();

    for (std::size_t i = 0; i < body.shape_count(); ++i) {
        const auto* shape = body.get_shape(i);
        if (!shape) continue;

        auto local_t = shape->local_transform();
        auto shape_pos = pos + void_math::rotate(rot, local_t.position);
        auto shape_rot = rot * local_t.rotation;

        switch (shape->type()) {
            case ShapeType::Box: {
                const auto& box = static_cast<const BoxShape&>(*shape);
                draw_box(shape_pos, box.half_extents(), shape_rot, color);
                break;
            }
            case ShapeType::Sphere: {
                const auto& sphere = static_cast<const SphereShape&>(*shape);
                draw_sphere(shape_pos, sphere.radius(), color);
                break;
            }
            case ShapeType::Capsule: {
                const auto& capsule = static_cast<const CapsuleShape&>(*shape);
                auto half_h = capsule.half_height();
                auto up = void_math::rotate(shape_rot, void_math::Vec3{0, 1, 0});
                draw_capsule(shape_pos - up * half_h, shape_pos + up * half_h, capsule.radius(), color);
                break;
            }
            default:
                break;
        }
    }
}

void PhysicsDebugRenderer::draw_world(const IPhysicsWorld& world) {
    world.for_each_body([this](const IRigidbody& body) {
        draw_body(body);
    });
}

} // namespace void_physics
