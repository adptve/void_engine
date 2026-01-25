# Physics Integration Diagram

This document describes the integration architecture of the void_physics module.

## Module Architecture

```
void_physics
├── Core Types (types.hpp, fwd.hpp)
│   ├── BodyId, ShapeId, JointId, MaterialId
│   ├── BodyType, ShapeType, JointType
│   ├── PhysicsConfig, PhysicsStats
│   └── CollisionMask, CollisionLayer
│
├── Shapes (shape.hpp)
│   ├── IShape interface
│   ├── BoxShape, SphereShape, CapsuleShape
│   ├── PlaneShape, ConvexHullShape
│   ├── MeshShape, HeightfieldShape
│   └── CompoundShape, ShapeFactory
│
├── Bodies (body.hpp)
│   ├── IRigidbody interface
│   ├── Rigidbody implementation
│   ├── BodyConfig, BodyBuilder
│   └── MassProperties
│
├── Broadphase (broadphase.hpp)
│   └── BroadPhaseBvh (Dynamic AABB Tree)
│       ├── insert/remove/update proxies
│       ├── query_pairs (collision pairs)
│       ├── query_aabb (spatial query)
│       ├── raycast (ray query)
│       └── query_point (point query)
│
├── Collision Detection (collision.hpp)
│   └── CollisionDetector
│       ├── GJK intersection test
│       ├── EPA contact generation
│       ├── Specialized tests:
│       │   ├── sphere-sphere
│       │   ├── sphere-plane
│       │   └── box-plane
│       └── ContactManifold output
│
├── Constraint Solver (solver.hpp)
│   ├── ConstraintSolver (main solver)
│   ├── ContactSolver (contact response)
│   └── Joint Constraints:
│       ├── FixedJointConstraint
│       ├── DistanceJointConstraint
│       ├── SpringJointConstraint
│       ├── BallJointConstraint
│       └── HingeJointConstraint
│
├── Simulation Pipeline (simulation.hpp)
│   ├── PhysicsPipeline
│   │   ├── step() - main simulation step
│   │   ├── Broadphase update
│   │   ├── Narrowphase detection
│   │   ├── Island building
│   │   ├── Constraint solving
│   │   └── Position integration
│   ├── IslandBuilder (parallel solving)
│   └── compute_toi (CCD)
│
├── World (world.hpp)
│   ├── IPhysicsWorld interface
│   ├── PhysicsWorld implementation
│   │   ├── Body management
│   │   ├── Joint management
│   │   ├── Material management
│   │   ├── Scene queries
│   │   └── HotReloadable support
│   └── CharacterController
│
└── Backend Abstraction (backend.hpp)
    ├── PhysicsBackend enum
    ├── IPhysicsBackend interface
    └── PhysicsBackendFactory
```

## Simulation Step Flow

```
PhysicsPipeline::step(dt)
    │
    ▼
┌─────────────────────────────────────┐
│ 1. Update Broadphase               │
│    - Sync body AABBs to BVH        │
│    - Expand AABBs by velocity      │
│    - Remove deleted bodies         │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 2. Detect Collisions               │
│    - BVH query for pairs           │
│    - Layer/mask filtering          │
│    - GJK/EPA narrowphase           │
│    - Build ContactConstraints      │
│    - Generate collision events     │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 3. Build Islands                   │
│    - Group connected bodies        │
│    - Track contacts per island     │
│    - Track joints per island       │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 4. Integrate Velocities            │
│    - Apply gravity                 │
│    - Apply external forces         │
│    - Apply damping                 │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 5. Solve Constraints               │
│    - Initialize contact solver     │
│    - Initialize joint constraints  │
│    - Warm starting (if enabled)    │
│    - Velocity iterations           │
│    - Position iterations           │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 6. Integrate Positions             │
│    - Apply solved positions        │
│    - Update body transforms        │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 7. Update Sleep States             │
│    - Check velocity thresholds     │
│    - Put slow bodies to sleep      │
│    - Wake touching bodies          │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ 8. Clear Forces                    │
│    - Reset accumulated forces      │
│    - Reset accumulated torques     │
└─────────────────────────────────────┘
```

## GJK/EPA Algorithm Flow

```
CollisionDetector::collide(shape_a, shape_b)
    │
    ▼
┌─────────────────────────────────────┐
│ AABB Early-Out                     │
│ - Check bounding box intersection  │
│ - Skip if no overlap              │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ GJK (Gilbert-Johnson-Keerthi)      │
│ - Find if Minkowski diff contains  │
│   the origin                       │
│ - Build simplex (point→tetrahedron)│
│ - Return: intersecting + simplex   │
└─────────────────────────────────────┘
    │
    ▼ (if intersecting)
┌─────────────────────────────────────┐
│ EPA (Expanding Polytope Algorithm) │
│ - Expand simplex to polytope       │
│ - Find closest face to origin      │
│ - Compute penetration depth        │
│ - Compute contact points           │
│ - Return: Contact with depth/normal│
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ Build ContactManifold              │
│ - Store contact points             │
│ - Combine material properties      │
│ - Return manifold                  │
└─────────────────────────────────────┘
```

## Sequential Impulse Solver Flow

```
ConstraintSolver::solve(contacts, joints, dt)
    │
    ▼
┌─────────────────────────────────────┐
│ Initialize Constraints             │
│ - Compute effective masses         │
│ - Compute Jacobians                │
│ - Compute bias velocities          │
│ - Build solver arrays              │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ Warm Starting                      │
│ - Apply cached impulses            │
│ - Scaled by warm_start_factor      │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ Velocity Iterations (8 default)    │
│ FOR each iteration:                │
│   - Solve joint velocity constraints│
│   - Solve contact velocity:         │
│     - Friction (tangent impulses)   │
│     - Normal (separation impulse)   │
│   - Clamp accumulated impulses     │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│ Position Iterations (3 default)    │
│ FOR each iteration:                │
│   - Solve contact penetration      │
│   - Solve joint position errors    │
│   - Apply Baumgarte stabilization  │
│   - Early exit if converged        │
└─────────────────────────────────────┘
```

## Hot-Reload Integration

```
PhysicsWorld (implements HotReloadable)
    │
    ├── snapshot()
    │   ├── Serialize PhysicsConfig
    │   ├── Serialize all body states:
    │   │   ├── Position, rotation
    │   │   ├── Linear/angular velocity
    │   │   ├── Mass properties
    │   │   └── Shape configurations
    │   ├── Serialize materials
    │   └── Pack into HotReloadSnapshot
    │
    ├── restore(snapshot)
    │   ├── Deserialize snapshot
    │   ├── Clear current state
    │   ├── Restore materials
    │   ├── Recreate bodies with shapes
    │   ├── Rebuild broadphase (transient)
    │   └── Reinitialize pipeline
    │
    └── is_compatible(version)
        └── Check version compatibility
```

## Dependencies

```
void_physics
    ├── void_core
    │   ├── Error/Result pattern
    │   ├── HotReloadable interface
    │   └── Version management
    │
    ├── void_math
    │   ├── Vec2, Vec3, Vec4
    │   ├── Quat
    │   ├── Mat3, Mat4
    │   ├── Transform
    │   ├── AABB, Sphere
    │   └── Utility functions
    │
    └── void_ecs (optional)
        └── ECS integration for physics components
```

## Performance Considerations

1. **Header-Only Design**: Core algorithms (GJK, EPA, solver) are inline for optimal performance in tight loops.

2. **BVH Broadphase**: O(log n) queries, O(n log n) pair detection. Fattened AABBs reduce update frequency.

3. **Island Solving**: Connected bodies form islands for potential parallelization.

4. **Warm Starting**: Cached impulses from previous frame accelerate convergence.

5. **Sleep System**: Inactive bodies are skipped entirely, reducing computational load.

6. **No Allocations in Step**: All temporary storage is pre-allocated in pipeline members.
