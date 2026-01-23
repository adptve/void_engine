/// @file fwd.hpp
/// @brief Forward declarations for void_physics

#pragma once

#include <cstdint>
#include <memory>

namespace void_physics {

// =============================================================================
// Forward Declarations
// =============================================================================

// Core Types
struct PhysicsConfig;
struct PhysicsStats;
struct RaycastHit;
struct ShapeCastHit;
struct ContactPoint;
struct CollisionEvent;

// Identifiers
struct BodyId;
struct ShapeId;
struct JointId;
struct MaterialId;

// Enums
enum class BodyType : std::uint8_t;
enum class ShapeType : std::uint8_t;
enum class JointType : std::uint8_t;
enum class ForceMode : std::uint8_t;
enum class QueryFilter : std::uint32_t;
enum class CollisionResponse : std::uint8_t;

// Physics Material
class PhysicsMaterial;
class PhysicsMaterialRegistry;

// Shapes
class IShape;
class BoxShape;
class SphereShape;
class CapsuleShape;
class PlaneShape;
class MeshShape;
class ConvexHullShape;
class HeightfieldShape;
class CompoundShape;

// Bodies
class IRigidbody;
class Rigidbody;
class StaticBody;
class KinematicBody;

// Joints
class IJoint;
class FixedJoint;
class HingeJoint;
class SliderJoint;
class BallJoint;
class DistanceJoint;
class SpringJoint;
class GenericJoint;

// Character Controller
class ICharacterController;
class CharacterController;

// World
class IPhysicsWorld;
class PhysicsWorld;

// Backend
enum class PhysicsBackend : std::uint8_t;
class IPhysicsBackend;
class PhysicsBackendFactory;

// Scene Query
class SceneQuery;
class RaycastQuery;
class ShapeCastQuery;
class OverlapQuery;

// Debug
class PhysicsDebugRenderer;

// Smart pointer aliases
using ShapePtr = std::shared_ptr<IShape>;
using BodyPtr = std::shared_ptr<IRigidbody>;
using JointPtr = std::shared_ptr<IJoint>;
using MaterialPtr = std::shared_ptr<PhysicsMaterial>;
using WorldPtr = std::unique_ptr<IPhysicsWorld>;

} // namespace void_physics
