/// @file physics.hpp
/// @brief Main include header for void_physics
///
/// void_physics provides comprehensive physics simulation:
/// - Multi-backend support (Jolt, PhysX, Bullet)
/// - Rigidbody dynamics with full constraint system
/// - Collision detection with layers and masks
/// - Scene queries (raycast, shape cast, overlap)
/// - Character controller
/// - Hot-reload support for physics state
///
/// ## Quick Start
///
/// ### Creating a Physics World
/// ```cpp
/// #include <void_engine/physics/physics.hpp>
///
/// // Create physics system
/// auto physics = std::make_unique<void_physics::PhysicsSystem>(void_physics::PhysicsBackend::Jolt);
/// physics->initialize(void_physics::PhysicsConfig::defaults());
///
/// // Or use the builder
/// auto world = void_physics::PhysicsWorldBuilder()
///     .gravity(0, -9.81f, 0)
///     .fixed_timestep(1.0f / 60.0f)
///     .max_bodies(10000)
///     .build();
/// ```
///
/// ### Creating Bodies
/// ```cpp
/// // Static floor
/// auto floor_id = world->create_body(
///     void_physics::BodyBuilder()
///         .static_body()
///         .position(0, 0, 0)
///         .with_box({50, 0.5f, 50})
///         .build()
/// );
///
/// // Dynamic sphere
/// auto sphere_id = world->create_body(
///     void_physics::BodyBuilder()
///         .dynamic_body()
///         .position(0, 10, 0)
///         .mass(1.0f)
///         .with_sphere(0.5f)
///         .build()
/// );
///
/// // Apply force
/// if (auto* body = world->get_body(sphere_id)) {
///     body->add_force({0, 100, 0});
/// }
/// ```
///
/// ### Raycasting
/// ```cpp
/// auto hit = world->raycast(
///     {0, 10, 0},      // origin
///     {0, -1, 0},      // direction
///     100.0f           // max distance
/// );
///
/// if (hit) {
///     std::cout << "Hit at " << hit.position << std::endl;
///     std::cout << "Distance: " << hit.distance << std::endl;
/// }
/// ```
///
/// ### Collision Callbacks
/// ```cpp
/// world->on_collision_begin([](const void_physics::CollisionEvent& event) {
///     std::cout << "Collision started between "
///               << event.body_a.value << " and "
///               << event.body_b.value << std::endl;
/// });
///
/// world->on_trigger_enter([](const void_physics::TriggerEvent& event) {
///     std::cout << "Entered trigger!" << std::endl;
/// });
/// ```
///
/// ### Character Controller
/// ```cpp
/// void_physics::CharacterControllerConfig cc_config;
/// cc_config.height = 1.8f;
/// cc_config.radius = 0.3f;
/// cc_config.step_height = 0.35f;
///
/// auto controller = std::make_unique<void_physics::CharacterController>(*world, cc_config);
/// controller->set_position({0, 1, 0});
///
/// // In update loop
/// void_math::Vec3 move_dir = get_input_direction();
/// controller->move(move_dir * speed, dt);
/// ```
///
/// ### Joints
/// ```cpp
/// // Create hinge joint (door)
/// void_physics::HingeJointConfig hinge_config;
/// hinge_config.body_a = frame_id;
/// hinge_config.body_b = door_id;
/// hinge_config.anchor_a = {1, 0, 0};
/// hinge_config.anchor_b = {-0.5f, 0, 0};
/// hinge_config.axis = {0, 1, 0};
/// hinge_config.use_limits = true;
/// hinge_config.lower_limit = 0;
/// hinge_config.upper_limit = 3.14159f / 2;
///
/// auto joint_id = world->create_hinge_joint(hinge_config);
/// ```
///
/// ### Materials
/// ```cpp
/// // Create bouncy material
/// void_physics::PhysicsMaterialData rubber;
/// rubber.static_friction = 0.9f;
/// rubber.dynamic_friction = 0.8f;
/// rubber.restitution = 0.8f;
///
/// auto rubber_id = world->create_material(rubber);
///
/// // Use preset
/// auto ice_id = world->create_material(void_physics::PhysicsMaterialData::ice());
/// ```

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "shape.hpp"
#include "body.hpp"
#include "world.hpp"
#include "backend.hpp"

namespace void_physics {

/// Prelude - commonly used types
namespace prelude {
    using void_physics::PhysicsWorld;
    using void_physics::PhysicsWorldBuilder;
    using void_physics::PhysicsConfig;
    using void_physics::PhysicsStats;
    using void_physics::PhysicsSystem;

    using void_physics::IRigidbody;
    using void_physics::Rigidbody;
    using void_physics::BodyConfig;
    using void_physics::BodyBuilder;
    using void_physics::BodyId;
    using void_physics::BodyType;

    using void_physics::IShape;
    using void_physics::BoxShape;
    using void_physics::SphereShape;
    using void_physics::CapsuleShape;
    using void_physics::PlaneShape;
    using void_physics::ConvexHullShape;
    using void_physics::MeshShape;
    using void_physics::HeightfieldShape;
    using void_physics::CompoundShape;
    using void_physics::ShapeFactory;
    using void_physics::ShapeId;
    using void_physics::ShapeType;

    using void_physics::JointConfig;
    using void_physics::HingeJointConfig;
    using void_physics::SliderJointConfig;
    using void_physics::BallJointConfig;
    using void_physics::DistanceJointConfig;
    using void_physics::SpringJointConfig;
    using void_physics::JointId;
    using void_physics::JointType;

    using void_physics::PhysicsMaterialData;
    using void_physics::MaterialId;
    using void_physics::MassProperties;

    using void_physics::RaycastHit;
    using void_physics::ShapeCastHit;
    using void_physics::OverlapResult;
    using void_physics::ContactPoint;
    using void_physics::CollisionEvent;
    using void_physics::TriggerEvent;

    using void_physics::QueryFilter;
    using void_physics::CollisionMask;
    using void_physics::CollisionLayer;
    using void_physics::CollisionResponse;
    using void_physics::ForceMode;

    using void_physics::CharacterController;
    using void_physics::CharacterControllerConfig;

    using void_physics::PhysicsBackend;
    using void_physics::IPhysicsBackend;
    using void_physics::PhysicsBackendFactory;

    namespace layers = void_physics::layers;
} // namespace prelude

} // namespace void_physics
