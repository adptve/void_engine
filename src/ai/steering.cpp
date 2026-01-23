/// @file steering.cpp
/// @brief Steering behavior implementation for void_ai module

#include <void_engine/ai/steering.hpp>

#include <algorithm>
#include <cmath>

namespace void_ai {

// =============================================================================
// Utility Functions
// =============================================================================

float orientation_from_velocity(const void_math::Vec3& velocity) {
    float len = length(velocity);
    if (len < 1e-6f) return 0;
    return std::atan2(velocity.x, velocity.z);
}

void_math::Vec3 velocity_from_orientation(float orientation, float speed) {
    return {
        std::sin(orientation) * speed,
        0,
        std::cos(orientation) * speed
    };
}

void_math::Vec3 normalize(const void_math::Vec3& v) {
    float len = length(v);
    if (len > 1e-6f) {
        return {v.x / len, v.y / len, v.z / len};
    }
    return v;
}

float length(const void_math::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float length_squared(const void_math::Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float distance(const void_math::Vec3& a, const void_math::Vec3& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float distance_squared(const void_math::Vec3& a, const void_math::Vec3& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return dx * dx + dy * dy + dz * dz;
}

float dot(const void_math::Vec3& a, const void_math::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void_math::Vec3 cross(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

namespace {

void_math::Vec3 vec3_sub(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

void_math::Vec3 vec3_add(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

void_math::Vec3 vec3_scale(const void_math::Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

} // anonymous namespace

// =============================================================================
// SeekBehavior Implementation
// =============================================================================

SeekBehavior::SeekBehavior(const void_math::Vec3& target)
    : m_target(target) {
}

SteeringOutput SeekBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    auto direction = vec3_sub(m_target, agent.position);
    float dist = length(direction);

    if (dist > 1e-6f) {
        auto desired_velocity = vec3_scale(normalize(direction), agent.max_speed);
        output.linear = vec3_sub(desired_velocity, agent.velocity);

        // Limit acceleration
        float accel = length(output.linear);
        if (accel > agent.max_acceleration) {
            output.linear = vec3_scale(normalize(output.linear), agent.max_acceleration);
        }
    }

    return output;
}

// =============================================================================
// FleeBehavior Implementation
// =============================================================================

FleeBehavior::FleeBehavior(const void_math::Vec3& target)
    : m_target(target) {
}

SteeringOutput FleeBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    auto direction = vec3_sub(agent.position, m_target);
    float dist = length(direction);

    // Check panic distance
    if (m_panic_distance > 0 && dist > m_panic_distance) {
        return output;
    }

    if (dist > 1e-6f) {
        auto desired_velocity = vec3_scale(normalize(direction), agent.max_speed);
        output.linear = vec3_sub(desired_velocity, agent.velocity);

        float accel = length(output.linear);
        if (accel > agent.max_acceleration) {
            output.linear = vec3_scale(normalize(output.linear), agent.max_acceleration);
        }
    }

    return output;
}

// =============================================================================
// ArriveBehavior Implementation
// =============================================================================

ArriveBehavior::ArriveBehavior(const void_math::Vec3& target)
    : m_target(target) {
}

ArriveBehavior::ArriveBehavior(const void_math::Vec3& target, const ArriveBehavior_Params& params)
    : m_target(target)
    , m_params(params) {
}

SteeringOutput ArriveBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    auto direction = vec3_sub(m_target, agent.position);
    float dist = length(direction);

    if (dist < m_params.target_radius) {
        // At target, stop
        output.linear = vec3_scale(agent.velocity, -1.0f / m_params.time_to_target);
        return output;
    }

    float target_speed;
    if (dist > m_params.slow_radius) {
        target_speed = agent.max_speed;
    } else {
        // Slow down
        target_speed = agent.max_speed * dist / m_params.slow_radius;
    }

    auto desired_velocity = vec3_scale(normalize(direction), target_speed);
    output.linear = vec3_sub(desired_velocity, agent.velocity);
    output.linear = vec3_scale(output.linear, 1.0f / m_params.time_to_target);

    float accel = length(output.linear);
    if (accel > agent.max_acceleration) {
        output.linear = vec3_scale(normalize(output.linear), agent.max_acceleration);
    }

    return output;
}

// =============================================================================
// PursueBehavior Implementation
// =============================================================================

SteeringOutput PursueBehavior::calculate(const KinematicState& agent) {
    // Calculate prediction time based on distance
    auto to_target = vec3_sub(m_target.position, agent.position);
    float dist = length(to_target);
    float speed = length(agent.velocity);

    float prediction_time = m_max_prediction;
    if (speed > dist / m_max_prediction) {
        prediction_time = dist / speed;
    }

    // Predict target position
    void_math::Vec3 predicted = vec3_add(m_target.position,
                                          vec3_scale(m_target.velocity, prediction_time));

    // Seek toward predicted position
    SeekBehavior seek(predicted);
    return seek.calculate(agent);
}

// =============================================================================
// EvadeBehavior Implementation
// =============================================================================

SteeringOutput EvadeBehavior::calculate(const KinematicState& agent) {
    // Calculate prediction time
    auto to_target = vec3_sub(m_target.position, agent.position);
    float dist = length(to_target);

    // Check panic distance
    if (m_panic_distance > 0 && dist > m_panic_distance) {
        return {};
    }

    float speed = length(agent.velocity);
    float prediction_time = m_max_prediction;
    if (speed > dist / m_max_prediction) {
        prediction_time = dist / speed;
    }

    // Predict target position
    void_math::Vec3 predicted = vec3_add(m_target.position,
                                          vec3_scale(m_target.velocity, prediction_time));

    // Flee from predicted position
    FleeBehavior flee(predicted);
    return flee.calculate(agent);
}

// =============================================================================
// WanderBehavior Implementation
// =============================================================================

WanderBehavior::WanderBehavior() = default;

WanderBehavior::WanderBehavior(const WanderParams& params)
    : m_params(params) {
}

SteeringOutput WanderBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    // Update wander angle with random jitter
    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
    m_wander_angle += jitter(m_rng) * m_params.jitter;

    // Calculate circle center ahead of agent
    void_math::Vec3 forward = velocity_from_orientation(agent.orientation, 1.0f);
    void_math::Vec3 circle_center = vec3_add(agent.position,
                                              vec3_scale(forward, m_params.circle_offset));

    // Calculate target on circle
    float target_x = circle_center.x + std::cos(m_wander_angle) * m_params.circle_radius;
    float target_z = circle_center.z + std::sin(m_wander_angle) * m_params.circle_radius;
    void_math::Vec3 target{target_x, agent.position.y, target_z};

    // Seek toward wander target
    SeekBehavior seek(target);
    output = seek.calculate(agent);

    return output;
}

// =============================================================================
// HideBehavior Implementation
// =============================================================================

HideBehavior::HideBehavior(ObstacleQuery obstacle_query)
    : m_obstacle_query(std::move(obstacle_query)) {
}

SteeringOutput HideBehavior::calculate(const KinematicState& agent) {
    auto hiding_spot = find_hiding_spot(agent);

    // Arrive at hiding spot
    ArriveBehavior arrive(hiding_spot);
    return arrive.calculate(agent);
}

void_math::Vec3 HideBehavior::find_hiding_spot(const KinematicState& agent) const {
    if (!m_obstacle_query) {
        return agent.position;
    }

    auto obstacles = m_obstacle_query();
    if (obstacles.empty()) {
        return agent.position;
    }

    void_math::Vec3 best_spot = agent.position;
    float best_dist_sq = std::numeric_limits<float>::max();

    for (const auto& [obs_pos, obs_radius] : obstacles) {
        // Calculate hiding position behind obstacle
        auto to_obs = vec3_sub(obs_pos, m_target);
        float dist = length(to_obs);
        if (dist < 1e-6f) continue;

        auto dir = normalize(to_obs);
        auto hiding_pos = vec3_add(obs_pos, vec3_scale(dir, obs_radius + m_distance_from_obstacle));

        // Check distance to agent
        float dist_sq = distance_squared(agent.position, hiding_pos);
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_spot = hiding_pos;
        }
    }

    return best_spot;
}

// =============================================================================
// ObstacleAvoidanceBehavior Implementation
// =============================================================================

ObstacleAvoidanceBehavior::ObstacleAvoidanceBehavior(RaycastFunc raycast)
    : m_raycast(std::move(raycast)) {
}

SteeringOutput ObstacleAvoidanceBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    if (!m_raycast) {
        return output;
    }

    float speed = length(agent.velocity);
    if (speed < 1e-6f) {
        return output;
    }

    // Cast rays ahead
    void_math::Vec3 forward = normalize(agent.velocity);
    float look_ahead = m_params.look_ahead * (speed / agent.max_speed);

    void_math::Vec3 ray_end = vec3_add(agent.position, vec3_scale(forward, look_ahead));

    void_math::Vec3 hit_point, hit_normal;
    if (m_raycast(agent.position, ray_end, hit_point, hit_normal)) {
        // Obstacle detected - steer away
        auto avoidance = vec3_scale(hit_normal, agent.max_acceleration);
        output.linear = avoidance;
    }

    // Also cast whisker rays
    float cos_angle = std::cos(m_params.whisker_angle);
    float sin_angle = std::sin(m_params.whisker_angle);

    // Left whisker
    void_math::Vec3 left_dir = {
        forward.x * cos_angle - forward.z * sin_angle,
        forward.y,
        forward.x * sin_angle + forward.z * cos_angle
    };
    void_math::Vec3 left_end = vec3_add(agent.position, vec3_scale(left_dir, look_ahead * 0.5f));

    if (m_raycast(agent.position, left_end, hit_point, hit_normal)) {
        output.linear = vec3_add(output.linear, vec3_scale(hit_normal, agent.max_acceleration * 0.5f));
    }

    // Right whisker
    void_math::Vec3 right_dir = {
        forward.x * cos_angle + forward.z * sin_angle,
        forward.y,
        -forward.x * sin_angle + forward.z * cos_angle
    };
    void_math::Vec3 right_end = vec3_add(agent.position, vec3_scale(right_dir, look_ahead * 0.5f));

    if (m_raycast(agent.position, right_end, hit_point, hit_normal)) {
        output.linear = vec3_add(output.linear, vec3_scale(hit_normal, agent.max_acceleration * 0.5f));
    }

    return output;
}

// =============================================================================
// SeparationBehavior Implementation
// =============================================================================

SeparationBehavior::SeparationBehavior(NeighborQuery query)
    : m_neighbor_query(std::move(query)) {
}

SteeringOutput SeparationBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    if (!m_neighbor_query) {
        return output;
    }

    auto neighbors = m_neighbor_query();
    if (neighbors.empty()) {
        return output;
    }

    void_math::Vec3 force{};
    int count = 0;

    for (const auto& neighbor : neighbors) {
        float dist = distance(agent.position, neighbor.position);
        if (dist > 0 && dist < m_separation_radius) {
            auto away = vec3_sub(agent.position, neighbor.position);
            away = normalize(away);
            // Weight by inverse distance
            away = vec3_scale(away, (m_separation_radius - dist) / m_separation_radius);
            force = vec3_add(force, away);
            count++;
        }
    }

    if (count > 0) {
        force = vec3_scale(force, agent.max_acceleration);
        output.linear = force;
    }

    return output;
}

// =============================================================================
// AlignmentBehavior Implementation
// =============================================================================

AlignmentBehavior::AlignmentBehavior(NeighborQuery query)
    : m_neighbor_query(std::move(query)) {
}

SteeringOutput AlignmentBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    if (!m_neighbor_query) {
        return output;
    }

    auto neighbors = m_neighbor_query();
    if (neighbors.empty()) {
        return output;
    }

    void_math::Vec3 avg_velocity{};
    int count = 0;

    for (const auto& neighbor : neighbors) {
        float dist = distance(agent.position, neighbor.position);
        if (dist < m_neighbor_radius) {
            avg_velocity = vec3_add(avg_velocity, neighbor.velocity);
            count++;
        }
    }

    if (count > 0) {
        avg_velocity = vec3_scale(avg_velocity, 1.0f / static_cast<float>(count));
        output.linear = vec3_sub(avg_velocity, agent.velocity);

        float accel = length(output.linear);
        if (accel > agent.max_acceleration) {
            output.linear = vec3_scale(normalize(output.linear), agent.max_acceleration);
        }
    }

    return output;
}

// =============================================================================
// CohesionBehavior Implementation
// =============================================================================

CohesionBehavior::CohesionBehavior(NeighborQuery query)
    : m_neighbor_query(std::move(query)) {
}

SteeringOutput CohesionBehavior::calculate(const KinematicState& agent) {
    SteeringOutput output;

    if (!m_neighbor_query) {
        return output;
    }

    auto neighbors = m_neighbor_query();
    if (neighbors.empty()) {
        return output;
    }

    void_math::Vec3 center{};
    int count = 0;

    for (const auto& neighbor : neighbors) {
        float dist = distance(agent.position, neighbor.position);
        if (dist < m_neighbor_radius) {
            center = vec3_add(center, neighbor.position);
            count++;
        }
    }

    if (count > 0) {
        center = vec3_scale(center, 1.0f / static_cast<float>(count));

        // Seek toward center
        SeekBehavior seek(center);
        output = seek.calculate(agent);
    }

    return output;
}

// =============================================================================
// PathFollowBehavior Implementation
// =============================================================================

PathFollowBehavior::PathFollowBehavior(const NavPath* path)
    : m_path(path) {
}

SteeringOutput PathFollowBehavior::calculate(const KinematicState& agent) {
    if (!m_path || !m_path->is_valid()) {
        return {};
    }

    // Predict future position
    auto predicted = vec3_add(agent.position, vec3_scale(agent.velocity, m_prediction_time));

    // Find point on path
    auto target = m_path->current_target();

    // Arrive at target
    m_arrive.set_target(target);
    return m_arrive.calculate(agent);
}

// =============================================================================
// SteeringAgent Implementation
// =============================================================================

SteeringAgent::SteeringAgent() = default;

SteeringAgent::SteeringAgent(const KinematicState& initial_state)
    : m_state(initial_state) {
}

void SteeringAgent::add_behavior(std::unique_ptr<ISteeringBehavior> behavior) {
    m_behaviors.push_back(std::move(behavior));
}

void SteeringAgent::remove_behavior(std::string_view name) {
    m_behaviors.erase(
        std::remove_if(m_behaviors.begin(), m_behaviors.end(),
            [name](const auto& b) { return b->name() == name; }),
        m_behaviors.end());
}

ISteeringBehavior* SteeringAgent::get_behavior(std::string_view name) {
    for (auto& behavior : m_behaviors) {
        if (behavior->name() == name) {
            return behavior.get();
        }
    }
    return nullptr;
}

void SteeringAgent::clear_behaviors() {
    m_behaviors.clear();
}

void SteeringAgent::set_behavior_weight(std::string_view name, float weight) {
    if (auto* behavior = get_behavior(name)) {
        behavior->set_weight(weight);
    }
}

void SteeringAgent::update(float dt) {
    m_last_steering = blend_behaviors();
    apply_steering(m_last_steering, dt);
}

SteeringOutput SteeringAgent::blend_behaviors() {
    SteeringOutput result;
    float total_weight = 0;

    for (auto& behavior : m_behaviors) {
        if (!behavior->is_enabled()) continue;

        auto output = behavior->calculate(m_state);
        float weight = behavior->weight();

        result.linear = vec3_add(result.linear, vec3_scale(output.linear, weight));
        result.angular += output.angular * weight;
        total_weight += weight;
    }

    if (total_weight > 0) {
        result.linear = vec3_scale(result.linear, 1.0f / total_weight);
        result.angular /= total_weight;
    }

    return result;
}

void SteeringAgent::apply_steering(const SteeringOutput& steering, float dt) {
    // Apply linear acceleration
    m_state.velocity = vec3_add(m_state.velocity, vec3_scale(steering.linear, dt));

    // Limit speed
    float speed = length(m_state.velocity);
    if (speed > m_state.max_speed) {
        m_state.velocity = vec3_scale(normalize(m_state.velocity), m_state.max_speed);
    }

    // Update position
    m_state.position = vec3_add(m_state.position, vec3_scale(m_state.velocity, dt));

    // Apply angular acceleration
    m_state.rotation += steering.angular * dt;
    m_state.rotation = std::clamp(m_state.rotation, -m_state.max_rotation, m_state.max_rotation);

    // Update orientation
    m_state.orientation += m_state.rotation * dt;

    // Update orientation from velocity if moving
    if (speed > 0.1f) {
        m_state.orientation = orientation_from_velocity(m_state.velocity);
    }
}

// =============================================================================
// FlockingGroup Implementation
// =============================================================================

FlockingGroup::FlockingGroup() = default;

FlockingGroup::FlockingGroup(const FlockingParams& params)
    : m_params(params) {
}

void FlockingGroup::add_agent(SteeringAgent* agent) {
    if (std::find(m_agents.begin(), m_agents.end(), agent) == m_agents.end()) {
        m_agents.push_back(agent);
    }
}

void FlockingGroup::remove_agent(SteeringAgent* agent) {
    m_agents.erase(std::remove(m_agents.begin(), m_agents.end(), agent), m_agents.end());
}

void FlockingGroup::clear_agents() {
    m_agents.clear();
}

void FlockingGroup::update(float dt) {
    for (auto* agent : m_agents) {
        // Get neighbors for this agent
        auto neighbors = get_neighbors(agent);

        // Create temporary behaviors
        auto sep_query = [&]() { return neighbors; };
        auto align_query = [&]() { return neighbors; };
        auto cohesion_query = [&]() { return neighbors; };

        SeparationBehavior separation(sep_query);
        separation.set_separation_radius(m_params.separation_radius);
        separation.set_weight(m_params.separation_weight);

        AlignmentBehavior alignment(align_query);
        alignment.set_neighbor_radius(m_params.neighbor_radius);
        alignment.set_weight(m_params.alignment_weight);

        CohesionBehavior cohesion(cohesion_query);
        cohesion.set_neighbor_radius(m_params.neighbor_radius);
        cohesion.set_weight(m_params.cohesion_weight);

        // Calculate combined steering
        SteeringOutput combined;
        combined += separation.calculate(agent->state()) * m_params.separation_weight;
        combined += alignment.calculate(agent->state()) * m_params.alignment_weight;
        combined += cohesion.calculate(agent->state()) * m_params.cohesion_weight;

        // Apply to agent's existing behaviors
        // The agent should have its behaviors already set up
        agent->update(dt);
    }
}

void_math::Vec3 FlockingGroup::center() const {
    if (m_agents.empty()) return {};

    void_math::Vec3 sum{};
    for (const auto* agent : m_agents) {
        sum = vec3_add(sum, agent->position());
    }
    return vec3_scale(sum, 1.0f / static_cast<float>(m_agents.size()));
}

void_math::Vec3 FlockingGroup::average_velocity() const {
    if (m_agents.empty()) return {};

    void_math::Vec3 sum{};
    for (const auto* agent : m_agents) {
        sum = vec3_add(sum, agent->velocity());
    }
    return vec3_scale(sum, 1.0f / static_cast<float>(m_agents.size()));
}

std::vector<KinematicState> FlockingGroup::get_neighbors(const SteeringAgent* agent) const {
    std::vector<KinematicState> neighbors;
    neighbors.reserve(m_agents.size() - 1);

    for (const auto* other : m_agents) {
        if (other == agent) continue;

        float dist = distance(agent->position(), other->position());
        if (dist < m_params.neighbor_radius) {
            neighbors.push_back(other->state());
        }
    }

    return neighbors;
}

// =============================================================================
// SteeringSystem Implementation
// =============================================================================

SteeringSystem::SteeringSystem() = default;
SteeringSystem::~SteeringSystem() = default;

SteeringAgent* SteeringSystem::create_agent() {
    m_agents.push_back(std::make_unique<SteeringAgent>());
    return m_agents.back().get();
}

void SteeringSystem::destroy_agent(SteeringAgent* agent) {
    m_agents.erase(
        std::remove_if(m_agents.begin(), m_agents.end(),
            [agent](const auto& a) { return a.get() == agent; }),
        m_agents.end());
}

FlockingGroup* SteeringSystem::create_flock(const FlockingParams& params) {
    m_flocks.push_back(std::make_unique<FlockingGroup>(params));
    return m_flocks.back().get();
}

void SteeringSystem::destroy_flock(FlockingGroup* flock) {
    m_flocks.erase(
        std::remove_if(m_flocks.begin(), m_flocks.end(),
            [flock](const auto& f) { return f.get() == flock; }),
        m_flocks.end());
}

void SteeringSystem::update(float dt) {
    // Update flocks first (they manage their agents)
    for (auto& flock : m_flocks) {
        flock->update(dt);
    }

    // Update standalone agents
    for (auto& agent : m_agents) {
        // Only update if not in a flock
        agent->update(dt);
    }
}

} // namespace void_ai
