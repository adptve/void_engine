/// @file types.hpp
/// @brief Core types for void_physics

#pragma once

#include "fwd.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>
#include <void_engine/math/mat.hpp>
#include <void_engine/math/transform.hpp>
#include <void_engine/math/bounds.hpp>
#include <void_engine/core/id.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace void_physics {

// =============================================================================
// Physics Backend
// =============================================================================

/// Supported physics backends
enum class PhysicsBackend : std::uint8_t {
    Null,           ///< Null backend (testing)
    Jolt,           ///< Jolt Physics (recommended)
    PhysX,          ///< NVIDIA PhysX
    Bullet,         ///< Bullet Physics
    Custom,         ///< User-provided backend
};

/// Get backend name
[[nodiscard]] const char* to_string(PhysicsBackend backend);

// =============================================================================
// Body Types
// =============================================================================

/// Rigidbody motion type
enum class BodyType : std::uint8_t {
    Static,         ///< Never moves, infinite mass
    Kinematic,      ///< Moved by user, infinite mass
    Dynamic,        ///< Simulated by physics
};

/// Get body type name
[[nodiscard]] const char* to_string(BodyType type);

/// Body activation state
enum class ActivationState : std::uint8_t {
    Active,         ///< Simulating
    Sleeping,       ///< At rest
    AlwaysActive,   ///< Never sleeps
    Disabled,       ///< Not simulating
};

/// Get activation state name
[[nodiscard]] const char* to_string(ActivationState state);

// =============================================================================
// Shape Types
// =============================================================================

/// Collision shape types
enum class ShapeType : std::uint8_t {
    Box,            ///< Axis-aligned box
    Sphere,         ///< Perfect sphere
    Capsule,        ///< Cylinder with hemispherical caps
    Cylinder,       ///< Cylinder
    Plane,          ///< Infinite plane
    ConvexHull,     ///< Convex mesh
    TriangleMesh,   ///< Arbitrary triangle mesh (static only)
    Heightfield,    ///< Terrain heightfield
    Compound,       ///< Combination of shapes
};

/// Get shape type name
[[nodiscard]] const char* to_string(ShapeType type);

// =============================================================================
// Joint Types
// =============================================================================

/// Joint/constraint types
enum class JointType : std::uint8_t {
    Fixed,          ///< No relative motion
    Hinge,          ///< Rotation around single axis (door)
    Slider,         ///< Translation along single axis (piston)
    Ball,           ///< Free rotation (ball-and-socket)
    Distance,       ///< Maintain distance between bodies
    Spring,         ///< Spring force between bodies
    Cone,           ///< Limited rotation cone (ragdoll)
    Generic,        ///< Configurable 6-DOF constraint
};

/// Get joint type name
[[nodiscard]] const char* to_string(JointType type);

// =============================================================================
// Force Modes
// =============================================================================

/// How force/torque is applied
enum class ForceMode : std::uint8_t {
    Force,          ///< Continuous force (N), scaled by dt
    Impulse,        ///< Instant impulse (N*s)
    Acceleration,   ///< Acceleration (m/s^2), mass-independent
    VelocityChange, ///< Direct velocity change (m/s)
};

// =============================================================================
// Collision Response
// =============================================================================

/// How collisions are handled
enum class CollisionResponse : std::uint8_t {
    Collide,        ///< Full collision response
    Trigger,        ///< Detection only, no response
    Ignore,         ///< No detection or response
};

// =============================================================================
// Query Filters
// =============================================================================

/// Scene query filter flags
enum class QueryFilter : std::uint32_t {
    None            = 0,
    Static          = 1 << 0,   ///< Include static bodies
    Kinematic       = 1 << 1,   ///< Include kinematic bodies
    Dynamic         = 1 << 2,   ///< Include dynamic bodies
    Triggers        = 1 << 3,   ///< Include triggers
    BackfaceCull    = 1 << 4,   ///< Cull backfaces for meshes
    AnyHit          = 1 << 5,   ///< Return any hit (faster)
    ClosestHit      = 1 << 6,   ///< Return closest hit
    AllHits         = 1 << 7,   ///< Return all hits

    Default         = Static | Kinematic | Dynamic | ClosestHit,
    All             = Static | Kinematic | Dynamic | Triggers,
};

/// Bitwise operations for QueryFilter
inline QueryFilter operator|(QueryFilter a, QueryFilter b) {
    return static_cast<QueryFilter>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline QueryFilter operator&(QueryFilter a, QueryFilter b) {
    return static_cast<QueryFilter>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_flag(QueryFilter flags, QueryFilter check) {
    return (flags & check) == check;
}

// =============================================================================
// Identifiers
// =============================================================================

/// Body identifier
struct BodyId {
    std::uint64_t value = 0;

    [[nodiscard]] bool is_valid() const noexcept { return value != 0; }
    [[nodiscard]] static BodyId invalid() { return BodyId{0}; }

    bool operator==(const BodyId& other) const noexcept { return value == other.value; }
    bool operator!=(const BodyId& other) const noexcept { return value != other.value; }
    bool operator<(const BodyId& other) const noexcept { return value < other.value; }
};

/// Shape identifier
struct ShapeId {
    std::uint64_t value = 0;

    [[nodiscard]] bool is_valid() const noexcept { return value != 0; }
    [[nodiscard]] static ShapeId invalid() { return ShapeId{0}; }

    bool operator==(const ShapeId& other) const noexcept { return value == other.value; }
    bool operator!=(const ShapeId& other) const noexcept { return value != other.value; }
};

/// Joint identifier
struct JointId {
    std::uint64_t value = 0;

    [[nodiscard]] bool is_valid() const noexcept { return value != 0; }
    [[nodiscard]] static JointId invalid() { return JointId{0}; }

    bool operator==(const JointId& other) const noexcept { return value == other.value; }
    bool operator!=(const JointId& other) const noexcept { return value != other.value; }
};

/// Material identifier
struct MaterialId {
    std::uint64_t value = 0;

    [[nodiscard]] bool is_valid() const noexcept { return value != 0; }
    [[nodiscard]] static MaterialId invalid() { return MaterialId{0}; }

    bool operator==(const MaterialId& other) const noexcept { return value == other.value; }
    bool operator!=(const MaterialId& other) const noexcept { return value != other.value; }
};

// =============================================================================
// Collision Layers
// =============================================================================

/// Collision layer (up to 32 layers)
using CollisionLayer = std::uint32_t;

/// Collision layer mask
struct CollisionMask {
    CollisionLayer layer = 1;           ///< This object's layer
    CollisionLayer collides_with = ~0u; ///< Layers this collides with

    /// Check if two masks can collide
    [[nodiscard]] static bool can_collide(const CollisionMask& a, const CollisionMask& b) {
        return (a.layer & b.collides_with) != 0 && (b.layer & a.collides_with) != 0;
    }
};

/// Predefined collision layers
namespace layers {
    constexpr CollisionLayer Default     = 1 << 0;
    constexpr CollisionLayer Static      = 1 << 1;
    constexpr CollisionLayer Dynamic     = 1 << 2;
    constexpr CollisionLayer Kinematic   = 1 << 3;
    constexpr CollisionLayer Player      = 1 << 4;
    constexpr CollisionLayer Enemy       = 1 << 5;
    constexpr CollisionLayer Projectile  = 1 << 6;
    constexpr CollisionLayer Trigger     = 1 << 7;
    constexpr CollisionLayer Debris      = 1 << 8;
    constexpr CollisionLayer Water       = 1 << 9;
    constexpr CollisionLayer Terrain     = 1 << 10;
    constexpr CollisionLayer Vehicle     = 1 << 11;
    constexpr CollisionLayer Ragdoll     = 1 << 12;
    constexpr CollisionLayer Interactive = 1 << 13;
    constexpr CollisionLayer UI          = 1 << 14;
    constexpr CollisionLayer All         = ~0u;
} // namespace layers

// =============================================================================
// Physics Material
// =============================================================================

/// Physics material properties
struct PhysicsMaterialData {
    float static_friction = 0.5f;   ///< Static friction coefficient [0, inf)
    float dynamic_friction = 0.5f;  ///< Dynamic/kinetic friction [0, inf)
    float restitution = 0.3f;       ///< Bounciness [0, 1]
    float density = 1000.0f;        ///< kg/m^3 (water = 1000)

    /// Combine mode for when two materials interact
    enum class CombineMode : std::uint8_t {
        Average,    ///< (a + b) / 2
        Minimum,    ///< min(a, b)
        Maximum,    ///< max(a, b)
        Multiply,   ///< a * b
    };

    CombineMode friction_combine = CombineMode::Average;
    CombineMode restitution_combine = CombineMode::Average;

    /// Common material presets
    [[nodiscard]] static PhysicsMaterialData ice();
    [[nodiscard]] static PhysicsMaterialData rubber();
    [[nodiscard]] static PhysicsMaterialData metal();
    [[nodiscard]] static PhysicsMaterialData wood();
    [[nodiscard]] static PhysicsMaterialData concrete();
    [[nodiscard]] static PhysicsMaterialData glass();
    [[nodiscard]] static PhysicsMaterialData flesh();
};

// =============================================================================
// Mass Properties
// =============================================================================

/// Mass and inertia properties
struct MassProperties {
    float mass = 1.0f;                              ///< Total mass (kg)
    void_math::Vec3 center_of_mass{0, 0, 0};        ///< Local space center of mass
    void_math::Vec3 inertia_diagonal{1, 1, 1};      ///< Principal moments of inertia
    void_math::Quat inertia_rotation{};             ///< Rotation to principal axes

    /// Create from mass only (assumes uniform density box)
    [[nodiscard]] static MassProperties from_mass(float mass);

    /// Create from density and shape (computed automatically)
    [[nodiscard]] static MassProperties from_density(float density, const IShape& shape);

    /// Infinite mass (static objects)
    [[nodiscard]] static MassProperties infinite();

    /// Check if mass is effectively infinite
    [[nodiscard]] bool is_infinite() const noexcept {
        return mass >= std::numeric_limits<float>::max() * 0.5f;
    }
};

// =============================================================================
// Contact Information
// =============================================================================

/// Single contact point between two bodies
struct ContactPoint {
    void_math::Vec3 position;           ///< World position
    void_math::Vec3 normal;             ///< Contact normal (from B to A)
    float penetration_depth = 0.0f;     ///< Penetration depth (negative = separated)
    float impulse = 0.0f;               ///< Applied impulse magnitude

    void_math::Vec3 position_on_a;      ///< Contact point on body A (local)
    void_math::Vec3 position_on_b;      ///< Contact point on body B (local)
};

/// Collision event data
struct CollisionEvent {
    BodyId body_a;
    BodyId body_b;
    ShapeId shape_a;
    ShapeId shape_b;

    std::vector<ContactPoint> contacts;

    void_math::Vec3 relative_velocity;  ///< Velocity of A relative to B
    float total_impulse = 0.0f;         ///< Total impulse magnitude

    /// Event type
    enum class Type : std::uint8_t {
        Begin,      ///< Collision started
        Stay,       ///< Collision ongoing
        End,        ///< Collision ended
    } type = Type::Begin;
};

/// Trigger event data
struct TriggerEvent {
    BodyId trigger_body;
    BodyId other_body;
    ShapeId trigger_shape;
    ShapeId other_shape;

    enum class Type : std::uint8_t {
        Enter,
        Stay,
        Exit,
    } type = Type::Enter;
};

// =============================================================================
// Raycast/Query Results
// =============================================================================

/// Raycast hit result
struct RaycastHit {
    bool hit = false;                   ///< Whether there was a hit
    BodyId body;                        ///< Hit body
    ShapeId shape;                      ///< Hit shape
    void_math::Vec3 position;           ///< World hit position
    void_math::Vec3 normal;             ///< Surface normal at hit
    float distance = 0.0f;              ///< Distance from ray origin
    float fraction = 0.0f;              ///< Fraction along ray [0, 1]
    std::uint32_t face_index = 0;       ///< Triangle index (for meshes)
    void_math::Vec2 barycentric;        ///< Barycentric coords (for meshes)

    [[nodiscard]] explicit operator bool() const noexcept { return hit; }
};

/// Shape cast hit result
struct ShapeCastHit {
    bool hit = false;
    BodyId body;
    ShapeId shape;
    void_math::Vec3 position;           ///< World position at hit
    void_math::Vec3 normal;             ///< Surface normal
    float distance = 0.0f;              ///< Distance traveled
    float fraction = 0.0f;              ///< Fraction along cast [0, 1]
    void_math::Vec3 contact_point;      ///< Exact contact point

    [[nodiscard]] explicit operator bool() const noexcept { return hit; }
};

/// Overlap result
struct OverlapResult {
    BodyId body;
    ShapeId shape;
};

// =============================================================================
// Body Configuration
// =============================================================================

/// Configuration for creating a rigidbody
struct BodyConfig {
    std::string name;
    BodyType type = BodyType::Dynamic;

    void_math::Vec3 position{0, 0, 0};
    void_math::Quat rotation{};
    void_math::Vec3 linear_velocity{0, 0, 0};
    void_math::Vec3 angular_velocity{0, 0, 0};

    MassProperties mass;
    CollisionMask collision_mask;
    CollisionResponse response = CollisionResponse::Collide;

    float linear_damping = 0.01f;       ///< Linear velocity damping [0, 1]
    float angular_damping = 0.05f;      ///< Angular velocity damping [0, 1]
    float gravity_scale = 1.0f;         ///< Gravity multiplier
    float max_linear_velocity = 500.0f; ///< Maximum linear speed (m/s)
    float max_angular_velocity = 100.0f; ///< Maximum angular speed (rad/s)

    bool continuous_detection = false;  ///< Enable CCD for fast objects
    bool allow_sleep = true;            ///< Allow body to sleep when at rest
    bool start_asleep = false;          ///< Start in sleeping state
    bool is_sensor = false;             ///< Trigger-only (no collision response)
    bool fixed_rotation = false;        ///< Disable rotation (2D physics style)

    void* user_data = nullptr;          ///< User pointer
    std::uint64_t user_id = 0;          ///< User identifier (e.g., entity ID)

    /// Create static body config
    [[nodiscard]] static BodyConfig make_static(const void_math::Vec3& pos);

    /// Create kinematic body config
    [[nodiscard]] static BodyConfig make_kinematic(const void_math::Vec3& pos);

    /// Create dynamic body config
    [[nodiscard]] static BodyConfig make_dynamic(const void_math::Vec3& pos, float mass);
};

// =============================================================================
// Joint Configuration
// =============================================================================

/// Base joint configuration
struct JointConfig {
    std::string name;
    JointType type = JointType::Fixed;

    BodyId body_a;
    BodyId body_b;

    void_math::Vec3 anchor_a{0, 0, 0};  ///< Anchor point on body A (local)
    void_math::Vec3 anchor_b{0, 0, 0};  ///< Anchor point on body B (local)

    bool collision_enabled = false;      ///< Allow connected bodies to collide
    float break_force = 0.0f;           ///< Force to break joint (0 = unbreakable)
    float break_torque = 0.0f;          ///< Torque to break joint (0 = unbreakable)
};

/// Hinge joint (revolute) configuration
struct HingeJointConfig : JointConfig {
    void_math::Vec3 axis{0, 1, 0};      ///< Rotation axis

    bool use_limits = false;
    float lower_limit = 0.0f;           ///< Lower angle limit (radians)
    float upper_limit = 0.0f;           ///< Upper angle limit (radians)

    bool use_motor = false;
    float motor_speed = 0.0f;           ///< Target angular velocity
    float max_motor_torque = 0.0f;      ///< Maximum motor torque

    bool use_spring = false;
    float spring_stiffness = 0.0f;
    float spring_damping = 0.0f;

    HingeJointConfig() { type = JointType::Hinge; }
};

/// Slider joint (prismatic) configuration
struct SliderJointConfig : JointConfig {
    void_math::Vec3 axis{1, 0, 0};      ///< Slide axis

    bool use_limits = false;
    float lower_limit = 0.0f;           ///< Lower position limit
    float upper_limit = 0.0f;           ///< Upper position limit

    bool use_motor = false;
    float motor_speed = 0.0f;           ///< Target velocity
    float max_motor_force = 0.0f;       ///< Maximum motor force

    SliderJointConfig() { type = JointType::Slider; }
};

/// Ball joint (spherical) configuration
struct BallJointConfig : JointConfig {
    bool use_cone_limit = false;
    float cone_angle = 0.0f;            ///< Maximum cone angle (radians)
    void_math::Vec3 twist_axis{0, 1, 0};
    float twist_lower = 0.0f;           ///< Lower twist limit
    float twist_upper = 0.0f;           ///< Upper twist limit

    BallJointConfig() { type = JointType::Ball; }
};

/// Distance joint configuration
struct DistanceJointConfig : JointConfig {
    float min_distance = 0.0f;
    float max_distance = 0.0f;
    bool spring_enabled = false;
    float spring_stiffness = 0.0f;
    float spring_damping = 0.0f;

    DistanceJointConfig() { type = JointType::Distance; }
};

/// Spring joint configuration
struct SpringJointConfig : JointConfig {
    float rest_length = 1.0f;
    float stiffness = 100.0f;
    float damping = 1.0f;
    float min_length = 0.0f;
    float max_length = std::numeric_limits<float>::max();

    SpringJointConfig() { type = JointType::Spring; }
};

// =============================================================================
// Character Controller Configuration
// =============================================================================

/// Character controller configuration
struct CharacterControllerConfig {
    float height = 1.8f;                ///< Total height
    float radius = 0.3f;                ///< Capsule radius
    float step_height = 0.35f;          ///< Maximum step height
    float max_slope = 45.0f;            ///< Maximum walkable slope (degrees)
    float skin_width = 0.02f;           ///< Collision skin

    float gravity = -9.81f;             ///< Gravity magnitude
    float walk_speed = 4.0f;            ///< Walking speed (m/s)
    float run_speed = 8.0f;             ///< Running speed (m/s)
    float jump_speed = 5.0f;            ///< Jump velocity (m/s)

    CollisionMask collision_mask;
    void* user_data = nullptr;
};

// =============================================================================
// Physics Configuration
// =============================================================================

/// Physics world configuration
struct PhysicsConfig {
    PhysicsBackend backend = PhysicsBackend::Jolt;

    void_math::Vec3 gravity{0, -9.81f, 0};

    /// Simulation parameters
    std::uint32_t max_substeps = 4;     ///< Maximum substeps per frame
    float fixed_timestep = 1.0f / 60.0f; ///< Physics timestep
    std::uint32_t velocity_iterations = 8;
    std::uint32_t position_iterations = 3;

    /// Broadphase
    std::uint32_t max_bodies = 65536;
    std::uint32_t max_body_pairs = 65536;
    std::uint32_t max_contact_constraints = 65536;

    /// Sleeping
    float sleep_threshold_linear = 0.05f;  ///< m/s
    float sleep_threshold_angular = 0.05f; ///< rad/s
    float time_to_sleep = 0.5f;            ///< Seconds of inactivity

    /// Continuous collision detection
    bool enable_ccd = true;
    float ccd_motion_threshold = 0.1f;  ///< Minimum motion for CCD

    /// Debug
    bool enable_debug_rendering = false;
    bool enable_profiling = false;

    /// Hot-reload
    bool enable_hot_reload = true;

    /// Default configuration
    [[nodiscard]] static PhysicsConfig defaults();

    /// High-fidelity configuration (more iterations, smaller timestep)
    [[nodiscard]] static PhysicsConfig high_fidelity();

    /// Performance configuration (fewer iterations)
    [[nodiscard]] static PhysicsConfig performance();
};

// =============================================================================
// Physics Statistics
// =============================================================================

/// Physics simulation statistics
struct PhysicsStats {
    /// Simulation
    std::uint32_t active_bodies = 0;
    std::uint32_t sleeping_bodies = 0;
    std::uint32_t static_bodies = 0;
    std::uint32_t kinematic_bodies = 0;
    std::uint32_t dynamic_bodies = 0;

    std::uint32_t active_joints = 0;
    std::uint32_t active_contacts = 0;

    /// Performance
    float step_time_ms = 0.0f;
    float broadphase_time_ms = 0.0f;
    float narrowphase_time_ms = 0.0f;
    float solver_time_ms = 0.0f;
    float integration_time_ms = 0.0f;

    std::uint32_t broadphase_pairs = 0;
    std::uint32_t narrowphase_pairs = 0;

    /// Queries
    std::uint32_t raycasts_per_frame = 0;
    std::uint32_t shape_casts_per_frame = 0;
    std::uint32_t overlaps_per_frame = 0;

    /// Memory
    std::size_t memory_usage_bytes = 0;
};

// =============================================================================
// Callbacks
// =============================================================================

/// Collision callback types
using CollisionCallback = std::function<void(const CollisionEvent&)>;
using TriggerCallback = std::function<void(const TriggerEvent&)>;

/// Contact filter - return false to ignore collision
using ContactFilterCallback = std::function<bool(BodyId, BodyId)>;

/// Raycast filter - return false to skip body
using RaycastFilterCallback = std::function<bool(BodyId, ShapeId)>;

/// Joint break callback
using JointBreakCallback = std::function<void(JointId)>;

} // namespace void_physics

// =============================================================================
// Hash Specializations
// =============================================================================

template<>
struct std::hash<void_physics::BodyId> {
    std::size_t operator()(const void_physics::BodyId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct std::hash<void_physics::ShapeId> {
    std::size_t operator()(const void_physics::ShapeId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template<>
struct std::hash<void_physics::JointId> {
    std::size_t operator()(const void_physics::JointId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
