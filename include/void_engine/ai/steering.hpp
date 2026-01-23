/// @file steering.hpp
/// @brief Reynolds-style steering behaviors for AI movement

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "navmesh.hpp"

#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace void_ai {

// =============================================================================
// Steering Behavior Interface
// =============================================================================

/// @brief Base interface for steering behaviors
class ISteeringBehavior {
public:
    virtual ~ISteeringBehavior() = default;

    /// @brief Calculate steering output
    virtual SteeringOutput calculate(const KinematicState& agent) = 0;

    /// @brief Get behavior name
    virtual std::string_view name() const = 0;

    /// @brief Enable/disable the behavior
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    /// @brief Set behavior weight
    void set_weight(float weight) { m_weight = weight; }
    float weight() const { return m_weight; }

protected:
    bool m_enabled{true};
    float m_weight{1.0f};
};

// =============================================================================
// Target-Based Behaviors
// =============================================================================

/// @brief Seek toward a target position
class SeekBehavior : public ISteeringBehavior {
public:
    SeekBehavior() = default;
    explicit SeekBehavior(const void_math::Vec3& target);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Seek"; }

    void set_target(const void_math::Vec3& target) { m_target = target; }
    const void_math::Vec3& target() const { return m_target; }

private:
    void_math::Vec3 m_target{};
};

/// @brief Flee from a target position
class FleeBehavior : public ISteeringBehavior {
public:
    FleeBehavior() = default;
    explicit FleeBehavior(const void_math::Vec3& target);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Flee"; }

    void set_target(const void_math::Vec3& target) { m_target = target; }
    const void_math::Vec3& target() const { return m_target; }

    void set_panic_distance(float dist) { m_panic_distance = dist; }
    float panic_distance() const { return m_panic_distance; }

private:
    void_math::Vec3 m_target{};
    float m_panic_distance{0};  ///< 0 = always flee
};

/// @brief Arrive at a target with smooth deceleration
class ArriveBehavior : public ISteeringBehavior {
public:
    ArriveBehavior() = default;
    explicit ArriveBehavior(const void_math::Vec3& target);
    ArriveBehavior(const void_math::Vec3& target, const ArriveBehavior_Params& params);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Arrive"; }

    void set_target(const void_math::Vec3& target) { m_target = target; }
    const void_math::Vec3& target() const { return m_target; }

    void set_params(const ArriveBehavior_Params& params) { m_params = params; }
    const ArriveBehavior_Params& params() const { return m_params; }

private:
    void_math::Vec3 m_target{};
    ArriveBehavior_Params m_params;
};

/// @brief Pursue a moving target by predicting its position
class PursueBehavior : public ISteeringBehavior {
public:
    PursueBehavior() = default;

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Pursue"; }

    void set_target(const KinematicState& target) { m_target = target; }
    const KinematicState& target() const { return m_target; }

    void set_max_prediction_time(float time) { m_max_prediction = time; }
    float max_prediction_time() const { return m_max_prediction; }

private:
    KinematicState m_target;
    float m_max_prediction{2.0f};
};

/// @brief Evade a moving target by predicting its position
class EvadeBehavior : public ISteeringBehavior {
public:
    EvadeBehavior() = default;

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Evade"; }

    void set_target(const KinematicState& target) { m_target = target; }
    const KinematicState& target() const { return m_target; }

    void set_max_prediction_time(float time) { m_max_prediction = time; }
    void set_panic_distance(float dist) { m_panic_distance = dist; }

private:
    KinematicState m_target;
    float m_max_prediction{2.0f};
    float m_panic_distance{0};
};

// =============================================================================
// Autonomous Behaviors
// =============================================================================

/// @brief Wander randomly
class WanderBehavior : public ISteeringBehavior {
public:
    WanderBehavior();
    explicit WanderBehavior(const WanderParams& params);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Wander"; }

    void set_params(const WanderParams& params) { m_params = params; }
    const WanderParams& params() const { return m_params; }

private:
    WanderParams m_params;
    float m_wander_angle{0};
    std::mt19937 m_rng{std::random_device{}()};
};

/// @brief Hide from a target behind obstacles
class HideBehavior : public ISteeringBehavior {
public:
    using ObstacleQuery = std::function<std::vector<std::pair<void_math::Vec3, float>>()>;

    HideBehavior() = default;
    explicit HideBehavior(ObstacleQuery obstacle_query);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Hide"; }

    void set_target(const void_math::Vec3& target) { m_target = target; }
    void set_obstacle_query(ObstacleQuery query) { m_obstacle_query = std::move(query); }

    void set_distance_from_obstacle(float dist) { m_distance_from_obstacle = dist; }
    float distance_from_obstacle() const { return m_distance_from_obstacle; }

private:
    void_math::Vec3 m_target{};
    ObstacleQuery m_obstacle_query;
    float m_distance_from_obstacle{2.0f};

    void_math::Vec3 find_hiding_spot(const KinematicState& agent) const;
};

// =============================================================================
// Avoidance Behaviors
// =============================================================================

/// @brief Avoid obstacles using raycasting
class ObstacleAvoidanceBehavior : public ISteeringBehavior {
public:
    using RaycastFunc = std::function<bool(const void_math::Vec3& from,
                                            const void_math::Vec3& to,
                                            void_math::Vec3& hit_point,
                                            void_math::Vec3& hit_normal)>;

    ObstacleAvoidanceBehavior() = default;
    explicit ObstacleAvoidanceBehavior(RaycastFunc raycast);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "ObstacleAvoidance"; }

    void set_raycast_func(RaycastFunc func) { m_raycast = std::move(func); }
    void set_params(const ObstacleAvoidanceParams& params) { m_params = params; }
    const ObstacleAvoidanceParams& params() const { return m_params; }

private:
    RaycastFunc m_raycast;
    ObstacleAvoidanceParams m_params;
};

// =============================================================================
// Flocking Behaviors
// =============================================================================

/// @brief Separation - steer away from nearby agents
class SeparationBehavior : public ISteeringBehavior {
public:
    using NeighborQuery = std::function<std::vector<KinematicState>()>;

    SeparationBehavior() = default;
    explicit SeparationBehavior(NeighborQuery query);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Separation"; }

    void set_neighbor_query(NeighborQuery query) { m_neighbor_query = std::move(query); }
    void set_separation_radius(float radius) { m_separation_radius = radius; }

private:
    NeighborQuery m_neighbor_query;
    float m_separation_radius{2.0f};
};

/// @brief Alignment - match velocity with nearby agents
class AlignmentBehavior : public ISteeringBehavior {
public:
    using NeighborQuery = std::function<std::vector<KinematicState>()>;

    AlignmentBehavior() = default;
    explicit AlignmentBehavior(NeighborQuery query);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Alignment"; }

    void set_neighbor_query(NeighborQuery query) { m_neighbor_query = std::move(query); }
    void set_neighbor_radius(float radius) { m_neighbor_radius = radius; }

private:
    NeighborQuery m_neighbor_query;
    float m_neighbor_radius{5.0f};
};

/// @brief Cohesion - steer toward center of nearby agents
class CohesionBehavior : public ISteeringBehavior {
public:
    using NeighborQuery = std::function<std::vector<KinematicState>()>;

    CohesionBehavior() = default;
    explicit CohesionBehavior(NeighborQuery query);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "Cohesion"; }

    void set_neighbor_query(NeighborQuery query) { m_neighbor_query = std::move(query); }
    void set_neighbor_radius(float radius) { m_neighbor_radius = radius; }

private:
    NeighborQuery m_neighbor_query;
    float m_neighbor_radius{5.0f};
};

// =============================================================================
// Path Following
// =============================================================================

/// @brief Follow a navigation path
class PathFollowBehavior : public ISteeringBehavior {
public:
    PathFollowBehavior() = default;
    explicit PathFollowBehavior(const NavPath* path);

    SteeringOutput calculate(const KinematicState& agent) override;
    std::string_view name() const override { return "PathFollow"; }

    void set_path(const NavPath* path) { m_path = path; }
    const NavPath* path() const { return m_path; }

    void set_path_offset(float offset) { m_path_offset = offset; }
    float path_offset() const { return m_path_offset; }

    void set_prediction_time(float time) { m_prediction_time = time; }

private:
    const NavPath* m_path{nullptr};
    float m_path_offset{1.0f};
    float m_prediction_time{0.1f};
    ArriveBehavior m_arrive;
};

// =============================================================================
// Steering Agent
// =============================================================================

/// @brief Agent that combines multiple steering behaviors
class SteeringAgent {
public:
    SteeringAgent();
    explicit SteeringAgent(const KinematicState& initial_state);

    // State access
    void set_state(const KinematicState& state) { m_state = state; }
    const KinematicState& state() const { return m_state; }
    KinematicState& state() { return m_state; }

    // Position/velocity shortcuts
    void set_position(const void_math::Vec3& pos) { m_state.position = pos; }
    const void_math::Vec3& position() const { return m_state.position; }

    void set_velocity(const void_math::Vec3& vel) { m_state.velocity = vel; }
    const void_math::Vec3& velocity() const { return m_state.velocity; }

    void set_orientation(float orient) { m_state.orientation = orient; }
    float orientation() const { return m_state.orientation; }

    // Behavior management
    void add_behavior(std::unique_ptr<ISteeringBehavior> behavior);
    void remove_behavior(std::string_view name);
    ISteeringBehavior* get_behavior(std::string_view name);
    void clear_behaviors();

    // Weighted blending
    void set_behavior_weight(std::string_view name, float weight);

    // Update
    void update(float dt);

    // Get last steering output
    const SteeringOutput& last_steering() const { return m_last_steering; }

    // Movement limits
    void set_max_speed(float speed) { m_state.max_speed = speed; }
    void set_max_acceleration(float accel) { m_state.max_acceleration = accel; }
    void set_max_rotation(float rot) { m_state.max_rotation = rot; }
    void set_max_angular_acceleration(float accel) { m_state.max_angular_acceleration = accel; }

private:
    KinematicState m_state;
    std::vector<std::unique_ptr<ISteeringBehavior>> m_behaviors;
    SteeringOutput m_last_steering;

    SteeringOutput blend_behaviors();
    void apply_steering(const SteeringOutput& steering, float dt);
};

// =============================================================================
// Flocking Group
// =============================================================================

/// @brief Manages a group of flocking agents
class FlockingGroup {
public:
    FlockingGroup();
    explicit FlockingGroup(const FlockingParams& params);

    // Agent management
    void add_agent(SteeringAgent* agent);
    void remove_agent(SteeringAgent* agent);
    void clear_agents();

    std::size_t agent_count() const { return m_agents.size(); }

    // Parameters
    void set_params(const FlockingParams& params) { m_params = params; }
    const FlockingParams& params() const { return m_params; }

    // Update all agents
    void update(float dt);

    // Get group center
    void_math::Vec3 center() const;

    // Get average velocity
    void_math::Vec3 average_velocity() const;

private:
    std::vector<SteeringAgent*> m_agents;
    FlockingParams m_params;

    std::vector<KinematicState> get_neighbors(const SteeringAgent* agent) const;
};

// =============================================================================
// Steering System
// =============================================================================

/// @brief High-level steering behavior system
class SteeringSystem {
public:
    SteeringSystem();
    ~SteeringSystem();

    // Agent management
    SteeringAgent* create_agent();
    void destroy_agent(SteeringAgent* agent);

    // Group management
    FlockingGroup* create_flock(const FlockingParams& params = FlockingParams{});
    void destroy_flock(FlockingGroup* flock);

    // Update all agents
    void update(float dt);

    // Statistics
    std::size_t agent_count() const { return m_agents.size(); }
    std::size_t flock_count() const { return m_flocks.size(); }

private:
    std::vector<std::unique_ptr<SteeringAgent>> m_agents;
    std::vector<std::unique_ptr<FlockingGroup>> m_flocks;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Calculate orientation from velocity
float orientation_from_velocity(const void_math::Vec3& velocity);

/// @brief Calculate velocity from orientation and speed
void_math::Vec3 velocity_from_orientation(float orientation, float speed);

/// @brief Normalize a vector
void_math::Vec3 normalize(const void_math::Vec3& v);

/// @brief Vector length
float length(const void_math::Vec3& v);

/// @brief Vector length squared
float length_squared(const void_math::Vec3& v);

/// @brief Distance between points
float distance(const void_math::Vec3& a, const void_math::Vec3& b);

/// @brief Distance squared between points
float distance_squared(const void_math::Vec3& a, const void_math::Vec3& b);

/// @brief Dot product
float dot(const void_math::Vec3& a, const void_math::Vec3& b);

/// @brief Cross product
void_math::Vec3 cross(const void_math::Vec3& a, const void_math::Vec3& b);

} // namespace void_ai
