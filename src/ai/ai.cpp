/// @file ai.cpp
/// @brief Main AI system implementation for void_ai module

#include <void_engine/ai/ai.hpp>

#include <cmath>

namespace void_ai {

namespace {

float vec3_length(const void_math::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

void_math::Vec3 vec3_sub(const void_math::Vec3& a, const void_math::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float vec3_distance(const void_math::Vec3& a, const void_math::Vec3& b) {
    return vec3_length(vec3_sub(a, b));
}

} // anonymous namespace

// =============================================================================
// AISystem Implementation
// =============================================================================

AISystem::AISystem()
    : AISystem(AISystemConfig{}) {
}

AISystem::AISystem(const AISystemConfig& config)
    : m_config(config)
    , m_navigation(std::make_unique<NavigationSystem>())
    , m_steering(std::make_unique<SteeringSystem>())
    , m_perception(std::make_unique<PerceptionSystem>()) {
}

AISystem::~AISystem() = default;

BehaviorTreeId AISystem::register_tree(std::unique_ptr<BehaviorTree> tree) {
    BehaviorTreeId id{m_next_tree_id++};
    tree->set_id(id);
    m_trees[id] = std::move(tree);
    return id;
}

void AISystem::unregister_tree(BehaviorTreeId id) {
    m_trees.erase(id);
}

BehaviorTree* AISystem::get_tree(BehaviorTreeId id) {
    auto it = m_trees.find(id);
    return it != m_trees.end() ? it->second.get() : nullptr;
}

BlackboardId AISystem::create_blackboard() {
    BlackboardId id{m_next_blackboard_id++};
    m_blackboards[id] = std::make_unique<Blackboard>();
    return id;
}

void AISystem::destroy_blackboard(BlackboardId id) {
    m_blackboards.erase(id);
}

IBlackboard* AISystem::get_blackboard(BlackboardId id) {
    auto it = m_blackboards.find(id);
    return it != m_blackboards.end() ? it->second.get() : nullptr;
}

void AISystem::update(float dt) {
    // Update all behavior trees
    for (auto& [id, tree] : m_trees) {
        tree->tick(dt);
    }

    // Update navigation
    m_navigation->update(dt);

    // Update steering
    m_steering->update(dt);

    // Update perception
    m_perception->update(dt);
}

AISystem::Stats AISystem::stats() const {
    Stats s;
    s.active_trees = m_trees.size();
    s.active_blackboards = m_blackboards.size();
    s.nav_meshes = m_navigation->navmesh_count();
    s.nav_agents = m_navigation->agent_count();
    s.steering_agents = m_steering->agent_count();
    s.perception_components = m_perception->perceiver_count();
    return s;
}

AISystem::Snapshot AISystem::take_snapshot() const {
    Snapshot snapshot;

    // Capture tree status
    for (const auto& [id, tree] : m_trees) {
        snapshot.tree_status.emplace_back(id, tree->status());
    }

    // Capture blackboard data
    for (const auto& [id, bb] : m_blackboards) {
        snapshot.blackboard_data[id] = bb->get_all();
    }

    return snapshot;
}

void AISystem::apply_snapshot(const Snapshot& snapshot) {
    // Restore blackboard data
    for (const auto& [id, data] : snapshot.blackboard_data) {
        if (auto* bb = get_blackboard(id)) {
            for (const auto& [key, value] : data) {
                bb->set_value(key, value);
            }
        }
    }
}

// =============================================================================
// AIController Implementation
// =============================================================================

AIController::AIController() = default;

AIController::AIController(AISystem& system)
    : m_system(&system) {
}

AIController::~AIController() = default;

void AIController::init(AISystem& system) {
    m_system = &system;
}

void AIController::set_behavior_tree(BehaviorTree* tree) {
    m_tree = tree;
    if (m_tree && m_blackboard) {
        m_tree->set_blackboard(m_blackboard);
    }
}

void AIController::set_blackboard(IBlackboard* blackboard) {
    m_blackboard = blackboard;
    if (m_tree && m_blackboard) {
        m_tree->set_blackboard(m_blackboard);
    }
}

void AIController::set_nav_agent(NavAgent* agent) {
    m_nav_agent = agent;
}

bool AIController::move_to(const void_math::Vec3& destination) {
    if (!m_nav_agent) return false;
    m_nav_agent->set_destination(destination);
    return true;
}

void AIController::stop_movement() {
    if (m_nav_agent) {
        m_nav_agent->stop();
    }
    if (m_steering_agent) {
        m_steering_agent->set_velocity({});
    }
}

bool AIController::has_reached_destination() const {
    if (!m_nav_agent) return true;
    return m_nav_agent->reached_destination();
}

void AIController::set_steering_agent(SteeringAgent* agent) {
    m_steering_agent = agent;
}

void AIController::set_perception(PerceptionComponent* perception) {
    m_perception = perception;
}

void AIController::set_position(const void_math::Vec3& position) {
    m_position = position;
    sync_positions();
}

void AIController::set_forward(const void_math::Vec3& forward) {
    m_forward = forward;
    if (m_perception) {
        m_perception->set_forward(forward);
    }
}

void AIController::update(float dt) {
    // Sync positions from external source
    sync_positions();

    // Update blackboard with common values
    update_blackboard();

    // Tick behavior tree
    if (m_tree) {
        m_tree->tick(dt);
    }

    // Steering agent updates itself via system
    // Nav agent updates itself via system

    // Update our position from nav agent if available
    if (m_nav_agent && !m_nav_agent->is_stopped()) {
        m_position = m_nav_agent->position();
    } else if (m_steering_agent) {
        m_position = m_steering_agent->position();
    }
}

bool AIController::has_target() const {
    if (!m_perception) return false;
    return m_perception->highest_threat() != nullptr;
}

void_math::Vec3 AIController::target_position() const {
    if (!m_perception) return {};
    const auto* target = m_perception->highest_threat();
    return target ? target->last_known_position : void_math::Vec3{};
}

float AIController::target_distance() const {
    if (!m_perception) return std::numeric_limits<float>::max();
    const auto* target = m_perception->highest_threat();
    if (!target) return std::numeric_limits<float>::max();
    return vec3_distance(m_position, target->last_known_position);
}

void AIController::sync_positions() {
    if (m_nav_agent) {
        m_nav_agent->set_position(m_position);
    }
    if (m_steering_agent) {
        m_steering_agent->set_position(m_position);
    }
    if (m_perception) {
        m_perception->set_position(m_position);
        m_perception->set_forward(m_forward);
    }
}

void AIController::update_blackboard() {
    if (!m_blackboard) return;

    // Update self position
    m_blackboard->set_vec3("self_position", m_position);

    // Update target info
    if (m_perception) {
        const auto* target = m_perception->highest_threat();
        m_blackboard->set_bool("has_target", target != nullptr);

        if (target) {
            m_blackboard->set_vec3("target_position", target->last_known_position);
            m_blackboard->set_float("target_distance", vec3_distance(m_position, target->last_known_position));
            m_blackboard->set_bool("can_see_target", target->currently_sensed);
        }
    }

    // Update path info
    if (m_nav_agent) {
        m_blackboard->set_bool("path_valid", m_nav_agent->has_path());
        if (m_nav_agent->has_path()) {
            m_blackboard->set_float("path_progress", m_nav_agent->path().progress());
        }
    }
}

} // namespace void_ai
