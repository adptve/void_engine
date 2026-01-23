/// @file ai.hpp
/// @brief Main header for void_ai module
///
/// This file provides a unified interface to the void_ai module, which implements
/// comprehensive game AI systems including behavior trees, navigation, steering,
/// and perception. The systems are designed for AAA-quality game AI with full
/// hot-reload support.
///
/// # Module Overview
///
/// ## Behavior Trees
/// Complete hierarchical behavior tree implementation with:
/// - Composite nodes: Sequence, Selector, Parallel, Random variants
/// - Decorator nodes: Inverter, Repeater, Cooldown, Timeout, Conditional
/// - Leaf nodes: Action, Condition, Wait, SubTree
/// - Fluent builder API for easy tree construction
/// - Blackboard data sharing system
///
/// ## Navigation
/// Production-quality pathfinding system:
/// - Navigation mesh representation
/// - A* pathfinding with string-pulling
/// - Navigation agents with path following
/// - Off-mesh connections for jumps, ladders, etc.
/// - NavMesh building from geometry
///
/// ## Steering Behaviors
/// Reynolds-style steering with:
/// - Basic behaviors: Seek, Flee, Arrive, Pursue, Evade
/// - Autonomous: Wander, Hide
/// - Avoidance: Obstacle avoidance with raycasting
/// - Flocking: Separation, Alignment, Cohesion
/// - Path following integration with navigation
///
/// ## Perception
/// AI sensing system:
/// - Multiple sense types: Sight, Hearing, Damage, Proximity
/// - Configurable sight cones and hearing ranges
/// - Line of sight integration with physics
/// - Target tracking with forget time
/// - Team-based filtering
///
/// # Example Usage
///
/// @code
/// #include <void_engine/ai/ai.hpp>
/// using namespace void_ai;
/// using namespace void_ai::prelude;
///
/// // Create a behavior tree
/// auto tree = BehaviorTreeBuilder()
///     .selector()
///         .sequence()
///             .name("Attack")
///             .condition("HasTarget", [&bb]() {
///                 return bb.get_bool("has_target");
///             })
///             .action("AttackTarget", [](float dt) {
///                 // Attack logic
///                 return NodeStatus::Success;
///             })
///         .end()
///         .sequence()
///             .name("Patrol")
///             .action("WalkToWaypoint", [](float dt) {
///                 // Walk logic
///                 return NodeStatus::Running;
///             })
///         .end()
///     .end()
///     .build();
///
/// // Setup navigation
/// NavigationSystem nav_system;
/// auto builder = NavMeshBuilder(NavMeshBuildConfig{});
/// builder.add_mesh(vertices, indices);
/// auto mesh_id = nav_system.add_navmesh(builder.build());
///
/// auto agent_id = nav_system.create_agent();
/// auto* agent = nav_system.get_agent(agent_id);
/// agent->set_destination({10, 0, 20});
///
/// // Create steering agent
/// SteeringAgent steering_agent;
/// steering_agent.add_behavior(std::make_unique<ArriveBehavior>(target));
/// steering_agent.add_behavior(std::make_unique<ObstacleAvoidanceBehavior>(raycast_func));
///
/// // Setup perception
/// PerceptionSystem perception_system;
/// auto* perceiver = perception_system.create_perceiver();
/// perceiver->setup_default_senses();
/// perceiver->on_target_gained([](const PerceptionEvent& event) {
///     // React to seeing/hearing target
/// });
///
/// // Game loop
/// while (running) {
///     tree->tick(dt);
///     nav_system.update(dt);
///     steering_agent.update(dt);
///     perception_system.update(dt);
/// }
/// @endcode
///
/// # Hot Reload Support
///
/// The AI systems support hot reload through:
/// - Behavior tree state serialization
/// - Blackboard value persistence
/// - Navigation mesh rebuilding
/// - Agent state preservation
///
/// @code
/// // Save state before reload
/// auto bb_data = blackboard->get_all();
/// auto nav_state = serialize_nav_agents(nav_system);
///
/// // Reload and restore
/// for (const auto& [key, value] : bb_data) {
///     blackboard->set_value(key, value);
/// }
/// deserialize_nav_agents(nav_system, nav_state);
/// @endcode

#pragma once

// Core types and forward declarations
#include "fwd.hpp"
#include "types.hpp"

// Subsystems
#include "blackboard.hpp"
#include "behavior_tree.hpp"
#include "navmesh.hpp"
#include "steering.hpp"
#include "perception.hpp"

namespace void_ai {

// =============================================================================
// AI System
// =============================================================================

/// @brief High-level AI system manager
class AISystem {
public:
    AISystem();
    explicit AISystem(const AISystemConfig& config);
    ~AISystem();

    // Subsystem access
    NavigationSystem& navigation() { return *m_navigation; }
    const NavigationSystem& navigation() const { return *m_navigation; }

    SteeringSystem& steering() { return *m_steering; }
    const SteeringSystem& steering() const { return *m_steering; }

    PerceptionSystem& perception() { return *m_perception; }
    const PerceptionSystem& perception() const { return *m_perception; }

    // Behavior tree management
    BehaviorTreeId register_tree(std::unique_ptr<BehaviorTree> tree);
    void unregister_tree(BehaviorTreeId id);
    BehaviorTree* get_tree(BehaviorTreeId id);

    // Blackboard management
    BlackboardId create_blackboard();
    void destroy_blackboard(BlackboardId id);
    IBlackboard* get_blackboard(BlackboardId id);

    // Update all systems
    void update(float dt);

    // Statistics
    struct Stats {
        std::size_t active_trees{0};
        std::size_t active_blackboards{0};
        std::size_t nav_meshes{0};
        std::size_t nav_agents{0};
        std::size_t steering_agents{0};
        std::size_t perception_components{0};
    };
    Stats stats() const;

    // Hot reload support
    struct Snapshot {
        std::vector<std::pair<BehaviorTreeId, NodeStatus>> tree_status;
        std::unordered_map<BlackboardId, std::vector<std::pair<std::string, BlackboardValue>>> blackboard_data;
    };
    Snapshot take_snapshot() const;
    void apply_snapshot(const Snapshot& snapshot);

    // Debug
    void set_debug_enabled(bool enabled) { m_debug_enabled = enabled; }
    bool debug_enabled() const { return m_debug_enabled; }

private:
    AISystemConfig m_config;
    std::unique_ptr<NavigationSystem> m_navigation;
    std::unique_ptr<SteeringSystem> m_steering;
    std::unique_ptr<PerceptionSystem> m_perception;

    std::unordered_map<BehaviorTreeId, std::unique_ptr<BehaviorTree>> m_trees;
    std::unordered_map<BlackboardId, std::unique_ptr<Blackboard>> m_blackboards;

    std::uint32_t m_next_tree_id{1};
    std::uint32_t m_next_blackboard_id{1};
    bool m_debug_enabled{false};
};

// =============================================================================
// AI Controller
// =============================================================================

/// @brief Integrated AI controller combining all systems
class AIController {
public:
    AIController();
    explicit AIController(AISystem& system);
    ~AIController();

    // Initialize with AI system
    void init(AISystem& system);

    // Behavior tree
    void set_behavior_tree(BehaviorTree* tree);
    BehaviorTree* behavior_tree() const { return m_tree; }

    // Blackboard
    void set_blackboard(IBlackboard* blackboard);
    IBlackboard* blackboard() const { return m_blackboard; }

    // Navigation
    void set_nav_agent(NavAgent* agent);
    NavAgent* nav_agent() const { return m_nav_agent; }

    bool move_to(const void_math::Vec3& destination);
    void stop_movement();
    bool has_reached_destination() const;

    // Steering
    void set_steering_agent(SteeringAgent* agent);
    SteeringAgent* steering_agent() const { return m_steering_agent; }

    // Perception
    void set_perception(PerceptionComponent* perception);
    PerceptionComponent* perception() const { return m_perception; }

    // Position synchronization
    void set_position(const void_math::Vec3& position);
    const void_math::Vec3& position() const { return m_position; }

    void set_forward(const void_math::Vec3& forward);
    const void_math::Vec3& forward() const { return m_forward; }

    // Update all controlled systems
    void update(float dt);

    // State queries
    bool has_target() const;
    void_math::Vec3 target_position() const;
    float target_distance() const;

private:
    AISystem* m_system{nullptr};
    BehaviorTree* m_tree{nullptr};
    IBlackboard* m_blackboard{nullptr};
    NavAgent* m_nav_agent{nullptr};
    SteeringAgent* m_steering_agent{nullptr};
    PerceptionComponent* m_perception{nullptr};

    void_math::Vec3 m_position{};
    void_math::Vec3 m_forward{0, 0, 1};

    void sync_positions();
    void update_blackboard();
};

// =============================================================================
// Prelude Namespace
// =============================================================================

/// @brief Commonly used types for convenient imports
namespace prelude {
    // Node status
    using void_ai::NodeStatus;
    using void_ai::NodeType;

    // Behavior tree
    using void_ai::BehaviorTree;
    using void_ai::BehaviorTreeBuilder;
    using void_ai::BehaviorTreeId;
    using void_ai::IBehaviorNode;
    using void_ai::ActionNode;
    using void_ai::ConditionNode;
    using void_ai::SequenceNode;
    using void_ai::SelectorNode;

    // Blackboard
    using void_ai::IBlackboard;
    using void_ai::Blackboard;
    using void_ai::BlackboardKey;
    using void_ai::BlackboardId;
    using void_ai::BlackboardValue;

    // Navigation
    using void_ai::NavMesh;
    using void_ai::NavMeshBuilder;
    using void_ai::NavMeshQuery;
    using void_ai::NavPath;
    using void_ai::NavAgent;
    using void_ai::NavigationSystem;
    using void_ai::NavMeshId;
    using void_ai::AgentId;
    using void_ai::PathResult;

    // Steering
    using void_ai::SteeringAgent;
    using void_ai::SteeringOutput;
    using void_ai::KinematicState;
    using void_ai::SeekBehavior;
    using void_ai::FleeBehavior;
    using void_ai::ArriveBehavior;
    using void_ai::WanderBehavior;
    using void_ai::FlockingGroup;

    // Perception
    using void_ai::PerceptionComponent;
    using void_ai::PerceptionSystem;
    using void_ai::StimulusSource;
    using void_ai::SightSense;
    using void_ai::HearingSense;
    using void_ai::KnownTarget;
    using void_ai::Stimulus;
    using void_ai::SenseType;

    // System
    using void_ai::AISystem;
    using void_ai::AIController;
}

} // namespace void_ai
