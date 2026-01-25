/// @file physics.cpp
/// @brief Main physics module compilation unit
///
/// This file provides the compilation unit for the main physics.hpp header
/// which aggregates all physics subsystems.

#include <void_engine/physics/physics.hpp>

namespace void_physics {

// =============================================================================
// Physics Module Entry Point
// =============================================================================

// This file ensures the physics.hpp aggregation header compiles correctly.
// The physics module consists of:
//
// Core Types (types.hpp):
//   - BodyId, ShapeId, JointId, MaterialId
//   - BodyType, ShapeType, JointType, ForceMode
//   - PhysicsConfig, PhysicsStats, PhysicsMaterialData
//   - RaycastHit, ShapeCastHit, OverlapResult
//   - CollisionEvent, TriggerEvent, ContactPoint
//   - CollisionMask, CollisionLayer, QueryFilter
//
// Shapes (shape.hpp):
//   - IShape interface
//   - BoxShape, SphereShape, CapsuleShape, PlaneShape
//   - ConvexHullShape, MeshShape, HeightfieldShape
//   - CompoundShape, ShapeFactory
//
// Bodies (body.hpp):
//   - IRigidbody interface
//   - Rigidbody, BodyConfig, BodyBuilder
//   - MassProperties, ActivationState
//
// World (world.hpp):
//   - IPhysicsWorld interface
//   - PhysicsWorld, PhysicsWorldBuilder
//   - JointConfig and variants
//   - CharacterController, CharacterControllerConfig
//
// Backend (backend.hpp):
//   - PhysicsBackend enum
//   - IPhysicsBackend interface
//   - PhysicsBackendFactory
//
// Simulation Pipeline (simulation.hpp):
//   - PhysicsPipeline - main simulation stepping
//   - IslandBuilder - parallel solving optimization
//   - TimeOfImpact - continuous collision detection
//
// Broadphase (broadphase.hpp):
//   - BroadPhaseBvh - dynamic AABB tree
//   - Spatial queries and pair detection
//
// Collision Detection (collision.hpp):
//   - CollisionDetector - GJK/EPA algorithms
//   - ContactManifold, Contact, CollisionPair
//   - Specialized collision tests (sphere-sphere, etc.)
//
// Constraint Solver (solver.hpp):
//   - ConstraintSolver - sequential impulse
//   - ContactSolver - contact response
//   - Joint constraints (Fixed, Distance, Spring, Ball, Hinge)

} // namespace void_physics
