/// @file fwd.hpp
/// @brief Forward declarations for void_ai module

#pragma once

#include <cstdint>
#include <memory>

namespace void_ai {

// =============================================================================
// Handle Types
// =============================================================================

/// @brief Strongly-typed behavior tree ID
struct BehaviorTreeId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const BehaviorTreeId&) const = default;
    auto operator<=>(const BehaviorTreeId&) const = default;
};

/// @brief Strongly-typed blackboard ID
struct BlackboardId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const BlackboardId&) const = default;
    auto operator<=>(const BlackboardId&) const = default;
};

/// @brief Strongly-typed navmesh ID
struct NavMeshId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const NavMeshId&) const = default;
    auto operator<=>(const NavMeshId&) const = default;
};

/// @brief Strongly-typed path ID
struct PathId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const PathId&) const = default;
    auto operator<=>(const PathId&) const = default;
};

/// @brief Strongly-typed agent ID
struct AgentId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const AgentId&) const = default;
    auto operator<=>(const AgentId&) const = default;
};

/// @brief Strongly-typed perception target ID
struct PerceptionTargetId {
    std::uint32_t value{0};
    explicit operator bool() const { return value != 0; }
    bool operator==(const PerceptionTargetId&) const = default;
    auto operator<=>(const PerceptionTargetId&) const = default;
};

// =============================================================================
// Forward Declarations - Behavior Trees
// =============================================================================

class IBehaviorNode;
class BehaviorTree;
class BehaviorTreeBuilder;

// Composites
class SequenceNode;
class SelectorNode;
class ParallelNode;
class RandomSelectorNode;
class RandomSequenceNode;

// Decorators
class InverterNode;
class RepeaterNode;
class RepeatUntilFailNode;
class SucceederNode;
class FailerNode;
class CooldownNode;
class TimeoutNode;
class ConditionalNode;

// Leaf Nodes
class ActionNode;
class ConditionNode;
class WaitNode;
class SubTreeNode;

// =============================================================================
// Forward Declarations - Blackboard
// =============================================================================

class IBlackboard;
class Blackboard;
class BlackboardScope;
class BlackboardObserver;

// =============================================================================
// Forward Declarations - Navigation
// =============================================================================

class INavMesh;
class NavMesh;
class NavMeshBuilder;
class NavMeshQuery;
class NavPath;
class NavAgent;
class NavigationSystem;

// =============================================================================
// Forward Declarations - Steering
// =============================================================================

class ISteeringBehavior;
class SteeringAgent;
class SteeringSystem;

// Behaviors
class SeekBehavior;
class FleeBehavior;
class ArriveBehavior;
class PursueBehavior;
class EvadeBehavior;
class WanderBehavior;
class ObstacleAvoidanceBehavior;
class SeparationBehavior;
class AlignmentBehavior;
class CohesionBehavior;
class PathFollowBehavior;
class HideBehavior;

// =============================================================================
// Forward Declarations - Perception
// =============================================================================

class ISense;
class PerceptionSystem;
class PerceptionComponent;
class StimulusSource;

class SightSense;
class HearingSense;
class DamageSense;
class ProximitySense;

// =============================================================================
// Smart Pointer Aliases
// =============================================================================

using BehaviorNodePtr = std::unique_ptr<IBehaviorNode>;
using BehaviorTreePtr = std::unique_ptr<BehaviorTree>;
using BlackboardPtr = std::unique_ptr<IBlackboard>;
using NavMeshPtr = std::unique_ptr<INavMesh>;
using SensePtr = std::unique_ptr<ISense>;
using SteeringBehaviorPtr = std::unique_ptr<ISteeringBehavior>;

} // namespace void_ai

// Hash specializations
namespace std {
    template<> struct hash<void_ai::BehaviorTreeId> {
        std::size_t operator()(const void_ai::BehaviorTreeId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_ai::BlackboardId> {
        std::size_t operator()(const void_ai::BlackboardId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_ai::NavMeshId> {
        std::size_t operator()(const void_ai::NavMeshId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_ai::PathId> {
        std::size_t operator()(const void_ai::PathId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_ai::AgentId> {
        std::size_t operator()(const void_ai::AgentId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
    template<> struct hash<void_ai::PerceptionTargetId> {
        std::size_t operator()(const void_ai::PerceptionTargetId& id) const noexcept {
            return std::hash<std::uint32_t>{}(id.value);
        }
    };
}
