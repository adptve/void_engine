/// @file types.hpp
/// @brief Common types and configurations for void_ai module

#pragma once

#include "fwd.hpp"
#include <void_engine/math/types.hpp>

#include <any>
#include <chrono>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace void_ai {

// =============================================================================
// Behavior Tree Types
// =============================================================================

/// @brief Result of a behavior node tick
enum class NodeStatus : std::uint8_t {
    Success,        ///< Node completed successfully
    Failure,        ///< Node failed
    Running,        ///< Node still executing
    Invalid         ///< Node in invalid state
};

/// @brief Type of behavior node
enum class NodeType : std::uint8_t {
    // Composites
    Sequence,
    Selector,
    Parallel,
    RandomSelector,
    RandomSequence,

    // Decorators
    Inverter,
    Repeater,
    RepeatUntilFail,
    Succeeder,
    Failer,
    Cooldown,
    Timeout,
    Conditional,

    // Leaf nodes
    Action,
    Condition,
    Wait,
    SubTree,

    // Custom
    Custom
};

/// @brief Policy for parallel node completion
enum class ParallelPolicy : std::uint8_t {
    RequireOne,     ///< Succeed when any child succeeds
    RequireAll,     ///< Succeed only when all children succeed
    RequirePercent  ///< Succeed when percentage succeeds
};

/// @brief Abort type for conditional decorators
enum class AbortType : std::uint8_t {
    None,           ///< No abort
    Self,           ///< Abort own subtree when condition changes
    LowerPriority,  ///< Abort lower priority nodes
    Both            ///< Abort both self and lower priority
};

/// @brief Callback for action nodes
using ActionCallback = std::function<NodeStatus(float dt)>;

/// @brief Callback for condition nodes
using ConditionCallback = std::function<bool()>;

/// @brief Blackboard value variant
using BlackboardValue = std::variant<
    bool,
    int,
    float,
    double,
    std::string,
    void_math::Vec3,
    void_math::Quat,
    std::any
>;

// =============================================================================
// Navigation Types
// =============================================================================

/// @brief Navigation polygon vertex
struct NavVertex {
    void_math::Vec3 position{};
    std::uint32_t index{0};
};

/// @brief Navigation polygon (convex)
struct NavPolygon {
    std::vector<std::uint32_t> vertices;
    std::vector<std::uint32_t> neighbors;  ///< Adjacent polygon indices
    void_math::Vec3 center{};
    float area{0};
    std::uint32_t flags{0};                ///< Area type flags
    float cost{1.0f};                      ///< Traversal cost multiplier
};

/// @brief Navigation mesh link between polygons
struct NavLink {
    std::uint32_t from_poly{0};
    std::uint32_t to_poly{0};
    void_math::Vec3 start{};
    void_math::Vec3 end{};
    float width{1.0f};
    float cost{1.0f};
    std::uint32_t flags{0};                ///< Jump, climb, etc.
    bool bidirectional{true};
};

/// @brief Off-mesh connection
struct OffMeshConnection {
    void_math::Vec3 start{};
    void_math::Vec3 end{};
    float radius{0.5f};
    float cost{1.0f};
    std::uint32_t flags{0};
    bool bidirectional{true};
    std::uint32_t user_id{0};
};

/// @brief Point on a navigation path
struct PathPoint {
    void_math::Vec3 position{};
    std::uint32_t polygon_index{0};
    std::uint32_t flags{0};
};

/// @brief Path query result
struct PathResult {
    std::vector<PathPoint> points;
    float total_distance{0};
    bool complete{false};
    bool partial{false};
};

/// @brief Area type for navmesh
enum class AreaType : std::uint8_t {
    Ground = 0,
    Water = 1,
    Grass = 2,
    Road = 3,
    Door = 4,
    Jump = 5,
    Custom1 = 10,
    Custom2 = 11,
    Custom3 = 12,
    NotWalkable = 255
};

/// @brief Agent configuration for navigation
struct NavAgentConfig {
    float radius{0.5f};
    float height{2.0f};
    float max_climb{0.35f};
    float max_slope{45.0f};
    float step_height{0.3f};
    std::uint32_t area_mask{0xFFFFFFFF};  ///< Which areas can traverse
};

/// @brief NavMesh build configuration
struct NavMeshBuildConfig {
    float cell_size{0.3f};
    float cell_height{0.2f};
    float agent_height{2.0f};
    float agent_radius{0.6f};
    float agent_max_climb{0.9f};
    float agent_max_slope{45.0f};
    float region_min_size{8.0f};
    float region_merge_size{20.0f};
    float edge_max_len{12.0f};
    float edge_max_error{1.3f};
    float verts_per_poly{6.0f};
    float detail_sample_dist{6.0f};
    float detail_sample_max_error{1.0f};
    bool partition_monotone{false};
    bool keep_inter_results{false};
};

// =============================================================================
// Steering Types
// =============================================================================

/// @brief Steering output
struct SteeringOutput {
    void_math::Vec3 linear{};      ///< Linear acceleration
    float angular{0};               ///< Angular acceleration

    SteeringOutput operator+(const SteeringOutput& other) const {
        return {
            {linear.x + other.linear.x, linear.y + other.linear.y, linear.z + other.linear.z},
            angular + other.angular
        };
    }

    SteeringOutput& operator+=(const SteeringOutput& other) {
        linear.x += other.linear.x;
        linear.y += other.linear.y;
        linear.z += other.linear.z;
        angular += other.angular;
        return *this;
    }

    SteeringOutput operator*(float scale) const {
        return {{linear.x * scale, linear.y * scale, linear.z * scale}, angular * scale};
    }
};

/// @brief Kinematic state of a steering agent
struct KinematicState {
    void_math::Vec3 position{};
    void_math::Vec3 velocity{};
    float orientation{0};              ///< Yaw angle in radians
    float rotation{0};                 ///< Angular velocity
    float max_speed{5.0f};
    float max_acceleration{10.0f};
    float max_rotation{3.14159f};
    float max_angular_acceleration{6.0f};
    float radius{0.5f};                ///< Agent radius for avoidance
};

/// @brief Steering behavior weight
struct SteeringWeight {
    ISteeringBehavior* behavior{nullptr};
    float weight{1.0f};
    bool enabled{true};
};

/// @brief Arrive behavior parameters
struct ArriveBehavior_Params {
    float slow_radius{3.0f};
    float target_radius{0.5f};
    float time_to_target{0.1f};
};

/// @brief Wander behavior parameters
struct WanderParams {
    float circle_offset{2.0f};
    float circle_radius{1.0f};
    float jitter{0.5f};
    float rate{0.2f};
};

/// @brief Obstacle avoidance parameters
struct ObstacleAvoidanceParams {
    float look_ahead{3.0f};
    float whisker_angle{0.5f};
    float avoid_margin{0.2f};
};

/// @brief Flocking parameters
struct FlockingParams {
    float separation_weight{1.5f};
    float alignment_weight{1.0f};
    float cohesion_weight{1.0f};
    float neighbor_radius{5.0f};
    float separation_radius{1.0f};
};

// =============================================================================
// Perception Types
// =============================================================================

/// @brief Type of sense
enum class SenseType : std::uint8_t {
    Sight,
    Hearing,
    Damage,
    Proximity,
    Touch,
    Custom
};

/// @brief Stimulus type
enum class StimulusType : std::uint8_t {
    Visual,
    Audio,
    Damage,
    Proximity,
    Custom
};

/// @brief Stimulus data
struct Stimulus {
    StimulusType type{StimulusType::Visual};
    void_math::Vec3 location{};
    void_math::Vec3 direction{};
    float strength{1.0f};
    float age{0};
    float max_age{5.0f};
    PerceptionTargetId source_id{};
    std::uint32_t team{0};
    std::any user_data;

    bool is_expired() const { return age >= max_age; }
};

/// @brief Perception event
struct PerceptionEvent {
    SenseType sense{SenseType::Sight};
    Stimulus stimulus;
    PerceptionTargetId target_id{};
    bool gained{true};                  ///< true = gained, false = lost
    float strength{1.0f};
};

/// @brief Sight sense configuration
struct SightConfig {
    float view_distance{20.0f};
    float peripheral_distance{10.0f};
    float view_angle{120.0f};           ///< Degrees
    float peripheral_angle{180.0f};     ///< Degrees
    float lose_sight_time{2.0f};        ///< Time before losing target
    bool use_los_check{true};           ///< Line of sight raycasts
    std::uint32_t los_collision_mask{0xFFFFFFFF};
};

/// @brief Hearing sense configuration
struct HearingConfig {
    float max_range{30.0f};
    float loudness_scale{1.0f};
    bool blocked_by_walls{true};
    std::uint32_t collision_mask{0xFFFFFFFF};
};

/// @brief Damage sense configuration
struct DamageConfig {
    float memory_time{10.0f};           ///< How long to remember damage source
};

/// @brief Proximity sense configuration
struct ProximityConfig {
    float range{5.0f};
    bool los_required{false};
};

/// @brief Known target information
struct KnownTarget {
    PerceptionTargetId target_id{};
    void_math::Vec3 last_known_position{};
    void_math::Vec3 last_known_velocity{};
    float last_seen_time{0};
    float strength{0};                   ///< Combined sense strength
    bool currently_sensed{false};
    std::uint32_t senses_mask{0};       ///< Which senses detected
    std::uint32_t team{0};
};

// =============================================================================
// AI System Configuration
// =============================================================================

/// @brief Global AI system configuration
struct AISystemConfig {
    std::uint32_t max_behavior_trees{1000};
    std::uint32_t max_blackboards{1000};
    std::uint32_t max_nav_agents{500};
    std::uint32_t max_perception_components{500};
    float perception_update_rate{0.1f};     ///< Seconds between perception updates
    float navmesh_update_rate{1.0f};        ///< Seconds between navmesh updates
    bool threaded_pathfinding{true};
    std::uint32_t pathfinding_threads{2};
};

} // namespace void_ai
