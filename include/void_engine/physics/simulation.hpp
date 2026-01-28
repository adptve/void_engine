/// @file simulation.hpp
/// @brief Physics simulation pipeline for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "body.hpp"
#include "shape.hpp"
#include "broadphase.hpp"
#include "collision.hpp"
#include "solver.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>
#include <void_engine/math/transform.hpp>

#include <chrono>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>

namespace void_physics {

// =============================================================================
// Simulation Island
// =============================================================================

/// Island of interconnected bodies for parallel solving
struct Island {
    std::vector<BodyId> bodies;
    std::vector<std::size_t> contact_indices;
    std::vector<std::size_t> joint_indices;
    bool sleeping = false;
};

// =============================================================================
// Island Builder
// =============================================================================

/// Builds islands of connected bodies
class IslandBuilder {
public:
    /// Build islands from bodies and constraints
    void build(
        const std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        const std::vector<ContactConstraint>& contacts,
        const std::vector<std::unique_ptr<IJointConstraint>>& joints)
    {
        m_islands.clear();
        m_body_to_island.clear();

        // Start with each awake dynamic body as potential island seed
        for (const auto& [id, body] : bodies) {
            if (body->type() == BodyType::Static) continue;
            if (body->is_sleeping()) continue;

            BodyId body_id{id};
            if (m_body_to_island.contains(id)) continue;

            // Start new island
            Island island;
            std::vector<BodyId> stack;
            stack.push_back(body_id);

            while (!stack.empty()) {
                BodyId current = stack.back();
                stack.pop_back();

                if (m_body_to_island.contains(current.value)) continue;

                m_body_to_island[current.value] = m_islands.size();
                island.bodies.push_back(current);

                // Find connected bodies through contacts
                for (std::size_t i = 0; i < contacts.size(); ++i) {
                    const auto& c = contacts[i];
                    BodyId other{0};

                    if (c.body_a == current) other = c.body_b;
                    else if (c.body_b == current) other = c.body_a;
                    else continue;

                    // Record contact in island
                    if (std::find(island.contact_indices.begin(), island.contact_indices.end(), i)
                        == island.contact_indices.end()) {
                        island.contact_indices.push_back(i);
                    }

                    // Add connected body to stack
                    auto it = bodies.find(other.value);
                    if (it != bodies.end() && it->second->type() != BodyType::Static) {
                        if (!m_body_to_island.contains(other.value)) {
                            stack.push_back(other);
                        }
                    }
                }

                // Find connected bodies through joints
                for (std::size_t i = 0; i < joints.size(); ++i) {
                    const auto& j = joints[i];
                    BodyId other{0};

                    if (j->body_a() == current) other = j->body_b();
                    else if (j->body_b() == current) other = j->body_a();
                    else continue;

                    // Record joint in island
                    if (std::find(island.joint_indices.begin(), island.joint_indices.end(), i)
                        == island.joint_indices.end()) {
                        island.joint_indices.push_back(i);
                    }

                    // Add connected body to stack
                    auto it = bodies.find(other.value);
                    if (it != bodies.end() && it->second->type() != BodyType::Static) {
                        if (!m_body_to_island.contains(other.value)) {
                            stack.push_back(other);
                        }
                    }
                }
            }

            if (!island.bodies.empty()) {
                m_islands.push_back(std::move(island));
            }
        }
    }

    [[nodiscard]] const std::vector<Island>& islands() const { return m_islands; }

private:
    std::vector<Island> m_islands;
    std::unordered_map<std::uint64_t, std::size_t> m_body_to_island;
};

// =============================================================================
// Physics Pipeline
// =============================================================================

/// Main physics simulation pipeline
class PhysicsPipeline {
public:
    explicit PhysicsPipeline(const PhysicsConfig& config)
        : m_config(config)
        , m_broadphase(std::make_unique<BroadPhaseBvh>())
        , m_solver(SolverConfig{
            config.velocity_iterations,
            config.position_iterations,
            0.2f,   // baumgarte
            0.005f, // slop
            1.0f,   // restitution threshold
            true,   // warm starting
            0.8f    // warm start factor
          })
    {}

    /// Step the simulation
    void step(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        std::vector<std::unique_ptr<IJointConstraint>>& joints,
        const std::unordered_map<std::uint64_t, PhysicsMaterialData>& materials,
        MaterialId default_material,
        float dt,
        PhysicsStats& stats)
    {
        auto start = std::chrono::high_resolution_clock::now();

        // 1. Update broadphase
        update_broadphase(bodies);

        // 2. Detect collisions (broadphase + narrowphase)
        auto bp_start = std::chrono::high_resolution_clock::now();
        detect_collisions(bodies, materials, default_material);
        auto bp_end = std::chrono::high_resolution_clock::now();

        // 3. Build islands
        m_island_builder.build(bodies, m_contacts, joints);

        // 4. Integrate velocities (apply forces)
        integrate_velocities(bodies, dt);

        // 5. Solve constraints
        auto solver_start = std::chrono::high_resolution_clock::now();
        solve_constraints(bodies, joints, dt);
        auto solver_end = std::chrono::high_resolution_clock::now();

        // 6. Integrate positions
        auto int_start = std::chrono::high_resolution_clock::now();
        integrate_positions(bodies, dt);
        auto int_end = std::chrono::high_resolution_clock::now();

        // 7. Update sleep states
        update_sleep_states(bodies, dt);

        // 8. Clear forces
        for (auto& [id, body] : bodies) {
            body->clear_forces();
        }

        auto end = std::chrono::high_resolution_clock::now();

        // Update statistics
        stats.broadphase_time_ms = std::chrono::duration<float, std::milli>(bp_end - bp_start).count();
        stats.solver_time_ms = std::chrono::duration<float, std::milli>(solver_end - solver_start).count();
        stats.integration_time_ms = std::chrono::duration<float, std::milli>(int_end - int_start).count();
        stats.step_time_ms = std::chrono::duration<float, std::milli>(end - start).count();
        stats.active_contacts = static_cast<std::uint32_t>(m_contacts.size());
        stats.active_joints = static_cast<std::uint32_t>(joints.size());
        stats.broadphase_pairs = static_cast<std::uint32_t>(m_broadphase_pairs.size());

        count_bodies(bodies, stats);
    }

    /// Get collision events from last step
    [[nodiscard]] const std::vector<CollisionEvent>& collision_events() const {
        return m_collision_events;
    }

    /// Get trigger events from last step
    [[nodiscard]] const std::vector<TriggerEvent>& trigger_events() const {
        return m_trigger_events;
    }

    /// Get broadphase for queries
    [[nodiscard]] BroadPhaseBvh& broadphase() { return *m_broadphase; }
    [[nodiscard]] const BroadPhaseBvh& broadphase() const { return *m_broadphase; }

    /// Get collision detector
    [[nodiscard]] CollisionDetector& collision_detector() { return m_collision_detector; }

private:
    void update_broadphase(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies)
    {
        for (auto& [id, body] : bodies) {
            if (!body->is_enabled()) continue;

            auto aabb = body->world_bounds();
            auto velocity = body->linear_velocity();

            // Add margin for CCD
            float margin = 0.05f;
            aabb.min = aabb.min - void_math::Vec3{margin, margin, margin};
            aabb.max = aabb.max + void_math::Vec3{margin, margin, margin};

            BodyId body_id{id};
            ShapeId shape_id{1}; // Simplified: one shape per body

            // Update or insert
            if (!m_broadphase->update(body_id, shape_id, aabb, velocity)) {
                m_broadphase->insert(aabb, body_id, shape_id);
            }
        }

        // Remove deleted bodies
        m_broadphase->remove_invalid([&bodies](BodyId id) {
            return !bodies.contains(id.value);
        });
    }

    void detect_collisions(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        const std::unordered_map<std::uint64_t, PhysicsMaterialData>& materials,
        MaterialId default_material)
    {
        // Store previous contacts for event generation
        m_previous_contacts = std::move(m_contact_set);
        m_contact_set.clear();
        m_contacts.clear();
        m_collision_events.clear();
        m_trigger_events.clear();

        // Get broadphase pairs
        m_broadphase_pairs.clear();
        m_broadphase->query_pairs(m_broadphase_pairs);

        // Narrowphase collision detection
        for (const auto& pair : m_broadphase_pairs) {
            auto it_a = bodies.find(pair.body_a.value);
            auto it_b = bodies.find(pair.body_b.value);
            if (it_a == bodies.end() || it_b == bodies.end()) continue;

            auto& body_a = *it_a->second;
            auto& body_b = *it_b->second;

            // Skip if both static
            if (body_a.type() == BodyType::Static && body_b.type() == BodyType::Static) continue;

            // Skip if both sleeping
            if (body_a.is_sleeping() && body_b.is_sleeping()) continue;

            // Check collision masks
            if (!CollisionMask::can_collide(body_a.collision_mask(), body_b.collision_mask())) continue;

            // Get shapes for collision test
            if (body_a.shape_count() == 0 || body_b.shape_count() == 0) continue;

            const IShape* shape_a = body_a.get_shape(0);
            const IShape* shape_b = body_b.get_shape(0);
            if (!shape_a || !shape_b) continue;

            // Perform narrowphase collision detection
            CollisionDetector::TransformedShape ts_a{shape_a, body_a.position(), body_a.rotation()};
            CollisionDetector::TransformedShape ts_b{shape_b, body_b.position(), body_b.rotation()};

            auto manifold = CollisionDetector::collide(ts_a, ts_b, pair.body_a, pair.body_b);

            if (manifold && !manifold->contacts.empty()) {
                std::uint64_t pair_key = make_pair_key(pair.body_a, pair.body_b);
                m_contact_set.insert(pair_key);

                bool was_colliding = m_previous_contacts.contains(pair_key);

                // Check for trigger
                if (body_a.is_trigger() || body_b.is_trigger()) {
                    TriggerEvent event;
                    event.trigger_body = body_a.is_trigger() ? pair.body_a : pair.body_b;
                    event.other_body = body_a.is_trigger() ? pair.body_b : pair.body_a;
                    event.trigger_shape = ShapeId{1};
                    event.other_shape = ShapeId{1};
                    event.type = was_colliding ? TriggerEvent::Type::Stay : TriggerEvent::Type::Enter;
                    m_trigger_events.push_back(event);
                    continue;
                }

                // Get material properties
                PhysicsMaterialData mat_a = get_material(materials, default_material, ShapeId{1});
                PhysicsMaterialData mat_b = get_material(materials, default_material, ShapeId{1});

                // Create contact constraint
                ContactConstraint constraint;
                constraint.body_a = pair.body_a;
                constraint.body_b = pair.body_b;
                constraint.index_a = get_body_index(pair.body_a);
                constraint.index_b = get_body_index(pair.body_b);
                constraint.normal = manifold->average_normal();
                build_tangent_basis(constraint.normal, constraint.tangent_1, constraint.tangent_2);

                // Combine material properties
                constraint.friction = combine_friction(
                    mat_a.dynamic_friction, mat_b.dynamic_friction, mat_a.friction_combine);
                constraint.restitution = combine_restitution(
                    mat_a.restitution, mat_b.restitution, mat_a.restitution_combine);

                // Set mass properties
                constraint.inv_mass_a = body_a.inverse_mass();
                constraint.inv_mass_b = body_b.inverse_mass();
                auto inertia_a = body_a.inertia();
                auto inertia_b = body_b.inertia();
                constraint.inv_inertia_a = void_math::Vec3{
                    inertia_a.x > 0.0001f ? 1.0f / inertia_a.x : 0.0f,
                    inertia_a.y > 0.0001f ? 1.0f / inertia_a.y : 0.0f,
                    inertia_a.z > 0.0001f ? 1.0f / inertia_a.z : 0.0f
                };
                constraint.inv_inertia_b = void_math::Vec3{
                    inertia_b.x > 0.0001f ? 1.0f / inertia_b.x : 0.0f,
                    inertia_b.y > 0.0001f ? 1.0f / inertia_b.y : 0.0f,
                    inertia_b.z > 0.0001f ? 1.0f / inertia_b.z : 0.0f
                };

                // Add contact points
                for (const auto& contact : manifold->contacts) {
                    ContactConstraint::ContactPointData cp;
                    cp.local_a = void_math::rotate(
                        void_math::conjugate(body_a.rotation()),
                        contact.point_a - body_a.position());
                    cp.local_b = void_math::rotate(
                        void_math::conjugate(body_b.rotation()),
                        contact.point_b - body_b.position());
                    cp.r_a = contact.point_a - body_a.position();
                    cp.r_b = contact.point_b - body_b.position();
                    constraint.points.push_back(cp);
                }

                m_contacts.push_back(std::move(constraint));

                // Generate collision event
                CollisionEvent event;
                event.body_a = pair.body_a;
                event.body_b = pair.body_b;
                event.shape_a = ShapeId{1};
                event.shape_b = ShapeId{1};
                event.type = was_colliding ? CollisionEvent::Type::Stay : CollisionEvent::Type::Begin;

                for (const auto& contact : manifold->contacts) {
                    ContactPoint cp;
                    cp.position = (contact.point_a + contact.point_b) * 0.5f;
                    cp.normal = contact.normal;
                    cp.penetration_depth = contact.depth;
                    event.contacts.push_back(cp);
                }

                auto vel_a = body_a.linear_velocity();
                auto vel_b = body_b.linear_velocity();
                event.relative_velocity = vel_a - vel_b;
                m_collision_events.push_back(std::move(event));
            }
        }

        // Generate collision end events
        for (std::uint64_t key : m_previous_contacts) {
            if (!m_contact_set.contains(key)) {
                auto [id_a, id_b] = decode_pair_key(key);
                CollisionEvent event;
                event.body_a = BodyId{id_a};
                event.body_b = BodyId{id_b};
                event.type = CollisionEvent::Type::End;
                m_collision_events.push_back(std::move(event));
            }
        }
    }

    void integrate_velocities(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        float dt)
    {
        for (auto& [id, body] : bodies) {
            if (body->type() != BodyType::Dynamic) continue;
            if (body->is_sleeping()) continue;

            // Apply gravity
            if (body->gravity_enabled()) {
                auto gravity_force = m_config.gravity * body->mass() * body->gravity_scale();
                body->add_force(gravity_force, ForceMode::Force);
            }

            // Integrate velocity
            float inv_mass = body->inverse_mass();
            if (inv_mass > 0.0f) {
                auto force = body->accumulated_force();
                auto accel = force * inv_mass;
                auto new_vel = body->linear_velocity() + accel * dt;

                // Apply damping
                float damping = 1.0f - body->linear_damping();
                damping = std::pow(damping, dt);
                new_vel = new_vel * damping;

                // Clamp velocity
                float speed = void_math::length(new_vel);
                if (speed > m_config.max_bodies) { // Using max_bodies as placeholder for max velocity
                    new_vel = new_vel * (500.0f / speed);
                }

                body->set_linear_velocity(new_vel);
            }

            // Angular velocity
            auto inertia = body->inertia();
            if (inertia.x > 0.0001f && inertia.y > 0.0001f && inertia.z > 0.0001f) {
                auto torque = body->accumulated_torque();
                auto inv_inertia = void_math::Vec3{
                    1.0f / inertia.x, 1.0f / inertia.y, 1.0f / inertia.z
                };
                auto ang_accel = torque * inv_inertia;
                auto new_ang_vel = body->angular_velocity() + ang_accel * dt;

                // Apply damping
                float damping = 1.0f - body->angular_damping();
                damping = std::pow(damping, dt);
                new_ang_vel = new_ang_vel * damping;

                body->set_angular_velocity(new_ang_vel);
            }
        }
    }

    void solve_constraints(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        std::vector<std::unique_ptr<IJointConstraint>>& joints,
        float dt)
    {
        // Build solver arrays
        m_velocities.clear();
        m_positions.clear();
        m_inv_masses.clear();
        m_inv_inertias.clear();
        m_body_index_map.clear();

        int index = 0;
        for (auto& [id, body] : bodies) {
            m_body_index_map[id] = index++;

            VelocityState vel;
            vel.v = body->linear_velocity();
            vel.w = body->angular_velocity();
            m_velocities.push_back(vel);

            PositionState pos;
            pos.p = body->position();
            pos.q = body->rotation();
            m_positions.push_back(pos);

            m_inv_masses.push_back(body->inverse_mass());

            auto inertia = body->inertia();
            m_inv_inertias.push_back(void_math::Vec3{
                inertia.x > 0.0001f ? 1.0f / inertia.x : 0.0f,
                inertia.y > 0.0001f ? 1.0f / inertia.y : 0.0f,
                inertia.z > 0.0001f ? 1.0f / inertia.z : 0.0f
            });
        }

        // Update contact indices
        for (auto& contact : m_contacts) {
            auto it_a = m_body_index_map.find(contact.body_a.value);
            auto it_b = m_body_index_map.find(contact.body_b.value);
            contact.index_a = it_a != m_body_index_map.end() ? it_a->second : -1;
            contact.index_b = it_b != m_body_index_map.end() ? it_b->second : -1;
        }

        // Solve
        m_solver.solve(m_contacts, joints, m_velocities, m_positions,
                      m_inv_masses, m_inv_inertias, dt);

        // Write back velocities
        index = 0;
        for (auto& [id, body] : bodies) {
            if (body->type() == BodyType::Dynamic && !body->is_sleeping()) {
                body->set_linear_velocity(m_velocities[static_cast<size_t>(index)].v);
                body->set_angular_velocity(m_velocities[static_cast<size_t>(index)].w);
            }
            ++index;
        }
    }

    void integrate_positions(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        float dt)
    {
        int index = 0;
        for (auto& [id, body] : bodies) {
            if (body->type() != BodyType::Dynamic) {
                ++index;
                continue;
            }
            if (body->is_sleeping()) {
                ++index;
                continue;
            }

            // ALWAYS integrate position from velocity first (semi-implicit Euler)
            // This is the core physics step - position = position + velocity * dt
            auto new_pos = body->position() + body->linear_velocity() * dt;
            body->set_position(new_pos);

            // Integrate rotation from angular velocity
            auto w = body->angular_velocity();
            auto q = body->rotation();
            void_math::Quat dq{w.x * dt * 0.5f, w.y * dt * 0.5f, w.z * dt * 0.5f, 0.0f};
            dq = void_math::Quat{
                dq.x * q.w + dq.w * q.x + dq.y * q.z - dq.z * q.y,
                dq.y * q.w + dq.w * q.y + dq.z * q.x - dq.x * q.z,
                dq.z * q.w + dq.w * q.z + dq.x * q.y - dq.y * q.x,
                dq.w * q.w - dq.x * q.x - dq.y * q.y - dq.z * q.z
            };
            q = void_math::Quat{q.x + dq.x, q.y + dq.y, q.z + dq.z, q.w + dq.w};
            body->set_rotation(void_math::normalize(q));

            // Apply solver position corrections for penetration resolution
            // The solver modifies m_positions during position iterations
            // These corrections are applied ON TOP of velocity integration
            if (static_cast<size_t>(index) < m_positions.size()) {
                // Calculate the correction delta from what the solver computed
                auto original_pos = body->position() - body->linear_velocity() * dt;
                auto solver_correction = m_positions[static_cast<size_t>(index)].p - original_pos;

                // Only apply if there's a meaningful correction (from constraint solving)
                float correction_mag = void_math::length(solver_correction);
                if (correction_mag > 0.0001f) {
                    // Add solver's penetration correction to velocity-integrated position
                    body->set_position(body->position() + solver_correction);
                    body->set_rotation(m_positions[static_cast<size_t>(index)].q);
                }
            }

            ++index;
        }
    }

    void update_sleep_states(
        std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        float dt)
    {
        for (auto& [id, body] : bodies) {
            if (body->type() != BodyType::Dynamic) continue;
            if (!body->can_sleep()) continue;
            if (body->activation_state() == ActivationState::AlwaysActive) continue;

            auto vel = body->linear_velocity();
            auto ang_vel = body->angular_velocity();
            float linear_speed = void_math::length(vel);
            float angular_speed = void_math::length(ang_vel);

            if (linear_speed < m_config.sleep_threshold_linear &&
                angular_speed < m_config.sleep_threshold_angular) {
                // Accumulate sleep time (stored in body as m_sleep_time)
                // For now, just check threshold
                if (linear_speed < m_config.sleep_threshold_linear * 0.5f &&
                    angular_speed < m_config.sleep_threshold_angular * 0.5f) {
                    body->sleep();
                }
            } else {
                body->wake_up();
            }
        }
    }

    void count_bodies(
        const std::unordered_map<std::uint64_t, std::unique_ptr<Rigidbody>>& bodies,
        PhysicsStats& stats)
    {
        stats.active_bodies = 0;
        stats.sleeping_bodies = 0;
        stats.static_bodies = 0;
        stats.kinematic_bodies = 0;
        stats.dynamic_bodies = 0;

        for (const auto& [id, body] : bodies) {
            switch (body->type()) {
                case BodyType::Static:
                    ++stats.static_bodies;
                    break;
                case BodyType::Kinematic:
                    ++stats.kinematic_bodies;
                    ++stats.active_bodies;
                    break;
                case BodyType::Dynamic:
                    ++stats.dynamic_bodies;
                    if (body->is_sleeping()) {
                        ++stats.sleeping_bodies;
                    } else {
                        ++stats.active_bodies;
                    }
                    break;
            }
        }
    }

    static void build_tangent_basis(
        const void_math::Vec3& normal,
        void_math::Vec3& tangent1,
        void_math::Vec3& tangent2)
    {
        if (std::abs(normal.x) > 0.9f) {
            tangent1 = void_math::normalize(void_math::cross(normal, void_math::Vec3{0, 1, 0}));
        } else {
            tangent1 = void_math::normalize(void_math::cross(normal, void_math::Vec3{1, 0, 0}));
        }
        tangent2 = void_math::cross(normal, tangent1);
    }

    static std::uint64_t make_pair_key(BodyId a, BodyId b) {
        if (a.value > b.value) std::swap(a, b);
        return (a.value << 32) | b.value;
    }

    static std::pair<std::uint64_t, std::uint64_t> decode_pair_key(std::uint64_t key) {
        return {key >> 32, key & 0xFFFFFFFF};
    }

    int get_body_index(BodyId id) const {
        auto it = m_body_index_map.find(id.value);
        return it != m_body_index_map.end() ? it->second : -1;
    }

    PhysicsMaterialData get_material(
        const std::unordered_map<std::uint64_t, PhysicsMaterialData>& materials,
        MaterialId default_mat,
        ShapeId /*shape_id*/) const
    {
        auto it = materials.find(default_mat.value);
        if (it != materials.end()) {
            return it->second;
        }
        return PhysicsMaterialData{};
    }

private:
    PhysicsConfig m_config;

    // Broadphase
    std::unique_ptr<BroadPhaseBvh> m_broadphase;
    std::vector<CollisionPair> m_broadphase_pairs;

    // Narrowphase
    CollisionDetector m_collision_detector;
    std::vector<ContactConstraint> m_contacts;

    // Contact tracking for events
    std::unordered_set<std::uint64_t> m_contact_set;
    std::unordered_set<std::uint64_t> m_previous_contacts;

    // Events
    std::vector<CollisionEvent> m_collision_events;
    std::vector<TriggerEvent> m_trigger_events;

    // Island building
    IslandBuilder m_island_builder;

    // Solver
    ConstraintSolver m_solver;

    // Solver arrays
    std::vector<VelocityState> m_velocities;
    std::vector<PositionState> m_positions;
    std::vector<float> m_inv_masses;
    std::vector<void_math::Vec3> m_inv_inertias;
    std::unordered_map<std::uint64_t, int> m_body_index_map;
};

// =============================================================================
// Continuous Collision Detection
// =============================================================================

/// Time of impact result
struct TimeOfImpact {
    bool hit = false;
    float t = 1.0f;          ///< Time of impact [0, 1]
    void_math::Vec3 normal;  ///< Contact normal
    void_math::Vec3 point;   ///< Contact point
};

/// Type alias for convenience
using TransformedShape = CollisionDetector::TransformedShape;

/// Compute time of impact between two moving shapes
[[nodiscard]] inline TimeOfImpact compute_toi(
    const TransformedShape& shape_a,
    const void_math::Vec3& vel_a,
    const TransformedShape& shape_b,
    const void_math::Vec3& vel_b,
    float max_t = 1.0f)
{
    TimeOfImpact result;

    // Relative velocity
    auto rel_vel = vel_a - vel_b;
    float rel_speed = void_math::length(rel_vel);

    if (rel_speed < 0.0001f) {
        return result;
    }

    // Binary search for TOI
    float t_min = 0.0f;
    float t_max = max_t;

    const int max_iterations = 20;
    for (int i = 0; i < max_iterations; ++i) {
        float t = (t_min + t_max) * 0.5f;

        // Move shapes to time t
        TransformedShape moved_a = shape_a;
        TransformedShape moved_b = shape_b;
        moved_a.position = shape_a.position + vel_a * t;
        moved_b.position = shape_b.position + vel_b * t;

        // Check for intersection
        auto gjk = CollisionDetector::gjk(moved_a, moved_b);

        if (gjk.intersecting) {
            t_max = t;
            result.hit = true;
            result.t = t;
        } else {
            t_min = t;
        }

        if (t_max - t_min < 0.0001f) {
            break;
        }
    }

    if (result.hit) {
        // Get contact info at TOI
        TransformedShape moved_a = shape_a;
        TransformedShape moved_b = shape_b;
        moved_a.position = shape_a.position + vel_a * result.t;
        moved_b.position = shape_b.position + vel_b * result.t;

        auto manifold = CollisionDetector::collide(moved_a, moved_b, BodyId{0}, BodyId{0});
        if (manifold && !manifold->contacts.empty()) {
            result.normal = manifold->average_normal();
            result.point = manifold->contacts[0].point_a;
        }
    }

    return result;
}

} // namespace void_physics
