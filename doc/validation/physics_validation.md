# Physics Validation Checklist

This document provides validation criteria for the void_physics module implementation.

## Build Validation

### Compilation

- [ ] All headers compile independently
- [ ] No unused variable warnings
- [ ] No deprecated API usage
- [ ] Clean build with `-Wall -Wextra -Wpedantic`
- [ ] MSVC `/W4` clean
- [ ] No ODR violations

### Linkage

- [ ] All symbols resolve correctly
- [ ] No missing template instantiations
- [ ] Static initialization order correct
- [ ] No circular dependencies

## API Completeness

### Core Types (types.hpp)

- [x] BodyId with hash support
- [x] ShapeId with hash support
- [x] JointId with hash support
- [x] MaterialId with hash support
- [x] BodyType enum (Static, Dynamic, Kinematic)
- [x] ShapeType enum
- [x] JointType enum
- [x] ForceMode enum
- [x] PhysicsConfig with sensible defaults
- [x] PhysicsStats for profiling
- [x] PhysicsMaterialData with presets
- [x] CollisionMask and CollisionLayer
- [x] QueryFilter flags
- [x] RaycastHit, ShapeCastHit, OverlapResult
- [x] CollisionEvent, TriggerEvent
- [x] ContactPoint

### Shapes (shape.hpp)

- [x] IShape interface with support function
- [x] BoxShape with half-extents
- [x] SphereShape with radius
- [x] CapsuleShape with radius and height
- [x] PlaneShape with normal and distance
- [x] ConvexHullShape
- [x] MeshShape (triangle mesh)
- [x] HeightfieldShape
- [x] CompoundShape (multi-shape)
- [x] ShapeFactory for creation

### Bodies (body.hpp)

- [x] IRigidbody interface
- [x] Rigidbody implementation
- [x] BodyConfig for construction
- [x] BodyBuilder fluent API
- [x] Position/rotation accessors
- [x] Velocity accessors
- [x] Force/torque application
- [x] Mass property management
- [x] Sleep state management
- [x] Collision mask management

### Broadphase (broadphase.hpp)

- [x] BroadPhaseBvh implementation
- [x] insert() for new proxies
- [x] remove() for deleted proxies
- [x] update() with velocity prediction
- [x] remove_invalid() for cleanup
- [x] query_pairs() for collision pairs
- [x] query_aabb() for spatial queries
- [x] raycast() with callback
- [x] query_point() for point queries
- [x] Tree balancing (AVL-style)
- [x] AABB fattening for dynamic objects

### Collision Detection (collision.hpp)

- [x] CollisionDetector::gjk()
- [x] CollisionDetector::epa()
- [x] CollisionDetector::collide()
- [x] Simplex class for GJK
- [x] Support point computation
- [x] GjkResult with simplex output
- [x] ContactManifold generation
- [x] Contact with depth and normal
- [x] Specialized sphere-sphere
- [x] Specialized sphere-plane
- [x] Specialized box-plane
- [x] CollisionPair with hash

### Constraint Solver (solver.hpp)

- [x] SolverConfig with tuning params
- [x] VelocityState, PositionState
- [x] ContactConstraint structure
- [x] IJointConstraint interface
- [x] FixedJointConstraint
- [x] DistanceJointConstraint with spring
- [x] SpringJointConstraint
- [x] BallJointConstraint with cone limit
- [x] HingeJointConstraint with motor
- [x] ContactSolver
- [x] ConstraintSolver (combined)
- [x] Warm starting support
- [x] Velocity iterations
- [x] Position iterations
- [x] Material combination functions

### Simulation Pipeline (simulation.hpp)

- [x] PhysicsPipeline class
- [x] step() main simulation
- [x] Broadphase update
- [x] Narrowphase detection
- [x] Island building
- [x] Velocity integration
- [x] Position integration
- [x] Sleep state management
- [x] Force clearing
- [x] Event generation (collision/trigger)
- [x] TimeOfImpact for CCD
- [x] TransformedShape type alias

### World (world.hpp)

- [x] IPhysicsWorld interface
- [x] PhysicsWorld implementation
- [x] Body creation/destruction
- [x] Joint creation/destruction
- [x] Material management
- [x] Step with fixed timestep
- [x] Raycast queries
- [x] Shape cast queries
- [x] Overlap queries
- [x] Collision callbacks
- [x] Trigger callbacks
- [x] HotReloadable implementation
- [x] snapshot() serialization
- [x] restore() deserialization
- [x] CharacterController

## Functional Tests

### Basic Simulation

- [ ] Single body falls under gravity
- [ ] Two spheres collide and separate
- [ ] Box rests on plane (stable stacking)
- [ ] Multiple bodies in stack
- [ ] Bodies come to rest (sleeping)

### Collision Detection

- [ ] Sphere-sphere intersection correct
- [ ] Sphere-plane intersection correct
- [ ] Box-box intersection correct
- [ ] Box-plane intersection correct
- [ ] Capsule collisions work
- [ ] Contact normals point correctly
- [ ] Penetration depths accurate

### Joints

- [ ] Fixed joint maintains relative position
- [ ] Distance joint maintains distance
- [ ] Spring joint oscillates correctly
- [ ] Ball joint allows rotation
- [ ] Hinge joint rotates on axis
- [ ] Joint motors apply torque
- [ ] Joint limits stop at bounds

### Scene Queries

- [ ] Raycast hits closest body
- [ ] Raycast all returns sorted hits
- [ ] Sphere cast detects collisions
- [ ] Box cast with rotation works
- [ ] Overlap test returns correct bodies
- [ ] Query filter respects layers

### Character Controller

- [ ] Moves with displacement
- [ ] Steps up small obstacles
- [ ] Slides along walls
- [ ] Detects ground correctly
- [ ] Handles slopes appropriately

### Hot-Reload

- [ ] Snapshot captures all state
- [ ] Restore recreates physics world
- [ ] Version compatibility checked
- [ ] Broadphase rebuilt on restore
- [ ] No memory leaks on reload

## Performance Tests

### Timing Benchmarks

- [ ] 1000 bodies step < 5ms
- [ ] Raycast in 10k bodies < 0.1ms
- [ ] Broadphase scales O(n log n)
- [ ] Narrowphase GJK < 1us per pair
- [ ] Solver converges in < 10 iterations

### Memory Usage

- [ ] No allocations during step()
- [ ] BVH memory proportional to bodies
- [ ] Contact cache bounded size
- [ ] No memory leaks in long runs

### Stability

- [ ] No jitter in stacking scenarios
- [ ] No tunneling at high velocities
- [ ] Joints stable under load
- [ ] No explosion from deep penetration

## Error Handling

### Invalid Input

- [ ] Zero-sized shapes rejected
- [ ] Invalid body IDs return null
- [ ] Zero timestep handled gracefully
- [ ] Infinite mass bodies handled
- [ ] NaN detection and recovery

### Edge Cases

- [ ] Bodies at origin
- [ ] Coincident shapes
- [ ] Very small/large mass ratios
- [ ] Very fast moving objects
- [ ] Many simultaneous contacts

## Code Quality

### Style Compliance

- [x] Member variables prefixed m_
- [x] Constants prefixed k_
- [x] Headers use #pragma once
- [x] Proper const correctness
- [x] [[nodiscard]] on getters
- [x] noexcept where applicable

### Documentation

- [x] All public APIs documented
- [x] Complex algorithms explained
- [x] Usage examples provided
- [x] Integration diagram complete
- [x] Validation checklist complete

## Integration Tests

### With void_ecs

- [ ] Physics components register
- [ ] Transform sync works
- [ ] Entity destruction cleans up
- [ ] System update order correct

### With void_render

- [ ] Debug rendering works
- [ ] Body positions match visuals
- [ ] Contact points visualized
- [ ] Velocity vectors shown

### With void_core

- [ ] Error handling consistent
- [ ] Hot-reload integrates
- [ ] Version management works
- [ ] Type registry compatible
