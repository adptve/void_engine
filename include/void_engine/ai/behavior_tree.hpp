/// @file behavior_tree.hpp
/// @brief Comprehensive behavior tree implementation

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "blackboard.hpp"

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace void_ai {

// =============================================================================
// Behavior Node Interface
// =============================================================================

/// @brief Base interface for all behavior tree nodes
class IBehaviorNode {
public:
    virtual ~IBehaviorNode() = default;

    /// @brief Initialize the node
    virtual void initialize() {}

    /// @brief Tick the node
    /// @param dt Delta time
    /// @return Node status
    virtual NodeStatus tick(float dt) = 0;

    /// @brief Terminate the node (called when node is aborted)
    virtual void terminate(NodeStatus status) {}

    /// @brief Reset the node to initial state
    virtual void reset() { m_status = NodeStatus::Invalid; }

    // Properties
    virtual NodeType type() const = 0;
    virtual std::string_view name() const { return m_name; }
    virtual void set_name(std::string_view name) { m_name = std::string(name); }

    NodeStatus status() const { return m_status; }
    bool is_running() const { return m_status == NodeStatus::Running; }
    bool is_success() const { return m_status == NodeStatus::Success; }
    bool is_failure() const { return m_status == NodeStatus::Failure; }

    // Blackboard access
    void set_blackboard(IBlackboard* bb) { m_blackboard = bb; }
    IBlackboard* blackboard() const { return m_blackboard; }

protected:
    std::string m_name;
    NodeStatus m_status{NodeStatus::Invalid};
    IBlackboard* m_blackboard{nullptr};
};

// =============================================================================
// Composite Nodes
// =============================================================================

/// @brief Base class for composite nodes with children
class CompositeNode : public IBehaviorNode {
public:
    void add_child(BehaviorNodePtr child);
    void remove_child(std::size_t index);
    void clear_children();

    std::size_t child_count() const { return m_children.size(); }
    IBehaviorNode* child(std::size_t index) const;

    void reset() override;
    void set_blackboard(IBlackboard* bb);

protected:
    std::vector<BehaviorNodePtr> m_children;
    std::size_t m_current_child{0};
};

/// @brief Executes children in order until one fails
class SequenceNode : public CompositeNode {
public:
    NodeType type() const override { return NodeType::Sequence; }
    NodeStatus tick(float dt) override;
};

/// @brief Executes children in order until one succeeds
class SelectorNode : public CompositeNode {
public:
    NodeType type() const override { return NodeType::Selector; }
    NodeStatus tick(float dt) override;
};

/// @brief Executes children in parallel
class ParallelNode : public CompositeNode {
public:
    explicit ParallelNode(ParallelPolicy success_policy = ParallelPolicy::RequireAll,
                          ParallelPolicy failure_policy = ParallelPolicy::RequireOne);

    NodeType type() const override { return NodeType::Parallel; }
    NodeStatus tick(float dt) override;
    void reset() override;

    void set_success_policy(ParallelPolicy policy) { m_success_policy = policy; }
    void set_failure_policy(ParallelPolicy policy) { m_failure_policy = policy; }
    void set_success_threshold(float threshold) { m_success_threshold = threshold; }

private:
    ParallelPolicy m_success_policy{ParallelPolicy::RequireAll};
    ParallelPolicy m_failure_policy{ParallelPolicy::RequireOne};
    float m_success_threshold{0.5f};
    std::vector<NodeStatus> m_child_status;
};

/// @brief Selects children in random order until one succeeds
class RandomSelectorNode : public CompositeNode {
public:
    explicit RandomSelectorNode(std::uint32_t seed = 0);

    NodeType type() const override { return NodeType::RandomSelector; }
    NodeStatus tick(float dt) override;
    void reset() override;

private:
    std::mt19937 m_rng;
    std::vector<std::size_t> m_shuffle_order;
    bool m_shuffled{false};
};

/// @brief Executes children in random order until one fails
class RandomSequenceNode : public CompositeNode {
public:
    explicit RandomSequenceNode(std::uint32_t seed = 0);

    NodeType type() const override { return NodeType::RandomSequence; }
    NodeStatus tick(float dt) override;
    void reset() override;

private:
    std::mt19937 m_rng;
    std::vector<std::size_t> m_shuffle_order;
    bool m_shuffled{false};
};

// =============================================================================
// Decorator Nodes
// =============================================================================

/// @brief Base class for decorator nodes with single child
class DecoratorNode : public IBehaviorNode {
public:
    void set_child(BehaviorNodePtr child);
    IBehaviorNode* child() const { return m_child.get(); }

    void reset() override;
    void set_blackboard(IBlackboard* bb);

protected:
    BehaviorNodePtr m_child;
};

/// @brief Inverts child result
class InverterNode : public DecoratorNode {
public:
    NodeType type() const override { return NodeType::Inverter; }
    NodeStatus tick(float dt) override;
};

/// @brief Repeats child execution
class RepeaterNode : public DecoratorNode {
public:
    explicit RepeaterNode(std::uint32_t count = 0);  ///< 0 = infinite

    NodeType type() const override { return NodeType::Repeater; }
    NodeStatus tick(float dt) override;
    void reset() override;

    void set_count(std::uint32_t count) { m_count = count; }

private:
    std::uint32_t m_count{0};
    std::uint32_t m_current{0};
};

/// @brief Repeats child until it fails
class RepeatUntilFailNode : public DecoratorNode {
public:
    NodeType type() const override { return NodeType::RepeatUntilFail; }
    NodeStatus tick(float dt) override;
};

/// @brief Always returns success
class SucceederNode : public DecoratorNode {
public:
    NodeType type() const override { return NodeType::Succeeder; }
    NodeStatus tick(float dt) override;
};

/// @brief Always returns failure
class FailerNode : public DecoratorNode {
public:
    NodeType type() const override { return NodeType::Failer; }
    NodeStatus tick(float dt) override;
};

/// @brief Prevents execution until cooldown expires
class CooldownNode : public DecoratorNode {
public:
    explicit CooldownNode(float cooldown_time);

    NodeType type() const override { return NodeType::Cooldown; }
    NodeStatus tick(float dt) override;
    void reset() override;

    void set_cooldown(float time) { m_cooldown_time = time; }

private:
    float m_cooldown_time{1.0f};
    float m_time_remaining{0};
    bool m_on_cooldown{false};
};

/// @brief Fails if child takes too long
class TimeoutNode : public DecoratorNode {
public:
    explicit TimeoutNode(float timeout);

    NodeType type() const override { return NodeType::Timeout; }
    NodeStatus tick(float dt) override;
    void reset() override;

    void set_timeout(float time) { m_timeout = time; }

private:
    float m_timeout{5.0f};
    float m_elapsed{0};
};

/// @brief Only runs child if condition is true
class ConditionalNode : public DecoratorNode {
public:
    using ConditionFunc = std::function<bool()>;

    explicit ConditionalNode(ConditionFunc condition, AbortType abort_type = AbortType::None);

    NodeType type() const override { return NodeType::Conditional; }
    void initialize() override;
    NodeStatus tick(float dt) override;

    void set_condition(ConditionFunc condition) { m_condition = std::move(condition); }
    void set_abort_type(AbortType type) { m_abort_type = type; }

private:
    ConditionFunc m_condition;
    AbortType m_abort_type{AbortType::None};
    bool m_was_true{false};
};

// =============================================================================
// Leaf Nodes
// =============================================================================

/// @brief Executes an action callback
class ActionNode : public IBehaviorNode {
public:
    explicit ActionNode(ActionCallback action);
    ActionNode(std::string_view name, ActionCallback action);

    NodeType type() const override { return NodeType::Action; }
    NodeStatus tick(float dt) override;

    void set_action(ActionCallback action) { m_action = std::move(action); }

private:
    ActionCallback m_action;
};

/// @brief Checks a condition
class ConditionNode : public IBehaviorNode {
public:
    explicit ConditionNode(ConditionCallback condition);
    ConditionNode(std::string_view name, ConditionCallback condition);

    NodeType type() const override { return NodeType::Condition; }
    NodeStatus tick(float dt) override;

    void set_condition(ConditionCallback condition) { m_condition = std::move(condition); }

private:
    ConditionCallback m_condition;
};

/// @brief Waits for a specified duration
class WaitNode : public IBehaviorNode {
public:
    explicit WaitNode(float duration);
    WaitNode(float min_duration, float max_duration);

    NodeType type() const override { return NodeType::Wait; }
    NodeStatus tick(float dt) override;
    void reset() override;

private:
    float m_min_duration{1.0f};
    float m_max_duration{1.0f};
    float m_target_duration{1.0f};
    float m_elapsed{0};
    std::mt19937 m_rng{std::random_device{}()};
};

/// @brief References another behavior tree
class SubTreeNode : public IBehaviorNode {
public:
    explicit SubTreeNode(BehaviorTree* subtree);

    NodeType type() const override { return NodeType::SubTree; }
    NodeStatus tick(float dt) override;
    void reset() override;

    void set_subtree(BehaviorTree* tree) { m_subtree = tree; }

private:
    BehaviorTree* m_subtree{nullptr};
};

// =============================================================================
// Behavior Tree
// =============================================================================

/// @brief Complete behavior tree
class BehaviorTree {
public:
    BehaviorTree();
    explicit BehaviorTree(BehaviorNodePtr root);
    ~BehaviorTree() = default;

    /// @brief Tick the tree
    NodeStatus tick(float dt);

    /// @brief Reset the tree
    void reset();

    // Root access
    void set_root(BehaviorNodePtr root);
    IBehaviorNode* root() const { return m_root.get(); }

    // Blackboard access
    void set_blackboard(IBlackboard* bb);
    IBlackboard* blackboard() const { return m_blackboard; }

    // Identifiers
    void set_id(BehaviorTreeId id) { m_id = id; }
    BehaviorTreeId id() const { return m_id; }

    void set_name(std::string_view name) { m_name = std::string(name); }
    std::string_view name() const { return m_name; }

    // Status
    NodeStatus status() const { return m_root ? m_root->status() : NodeStatus::Invalid; }

private:
    BehaviorNodePtr m_root;
    IBlackboard* m_blackboard{nullptr};
    BehaviorTreeId m_id{};
    std::string m_name;
};

// =============================================================================
// Behavior Tree Builder
// =============================================================================

/// @brief Fluent builder for behavior trees
class BehaviorTreeBuilder {
public:
    BehaviorTreeBuilder();

    // Composites
    BehaviorTreeBuilder& sequence();
    BehaviorTreeBuilder& selector();
    BehaviorTreeBuilder& parallel(ParallelPolicy success = ParallelPolicy::RequireAll,
                                   ParallelPolicy failure = ParallelPolicy::RequireOne);
    BehaviorTreeBuilder& random_selector(std::uint32_t seed = 0);
    BehaviorTreeBuilder& random_sequence(std::uint32_t seed = 0);

    // Decorators
    BehaviorTreeBuilder& inverter();
    BehaviorTreeBuilder& repeater(std::uint32_t count = 0);
    BehaviorTreeBuilder& repeat_until_fail();
    BehaviorTreeBuilder& succeeder();
    BehaviorTreeBuilder& failer();
    BehaviorTreeBuilder& cooldown(float time);
    BehaviorTreeBuilder& timeout(float time);
    BehaviorTreeBuilder& conditional(ConditionalNode::ConditionFunc cond,
                                     AbortType abort = AbortType::None);

    // Leaf nodes
    BehaviorTreeBuilder& action(ActionCallback callback);
    BehaviorTreeBuilder& action(std::string_view name, ActionCallback callback);
    BehaviorTreeBuilder& condition(ConditionCallback callback);
    BehaviorTreeBuilder& condition(std::string_view name, ConditionCallback callback);
    BehaviorTreeBuilder& wait(float duration);
    BehaviorTreeBuilder& wait(float min_duration, float max_duration);
    BehaviorTreeBuilder& subtree(BehaviorTree* tree);

    // Structure
    BehaviorTreeBuilder& end();  ///< End current composite/decorator
    BehaviorTreeBuilder& name(std::string_view name);

    // Build
    BehaviorTreePtr build();

private:
    struct BuildContext {
        BehaviorNodePtr node;
        bool is_composite{false};
    };

    std::vector<BuildContext> m_stack;
    BehaviorNodePtr m_root;
    std::string m_pending_name;

    void push_node(BehaviorNodePtr node, bool is_composite);
    void attach_to_parent(BehaviorNodePtr node);
    void apply_pending_name(IBehaviorNode* node);
};

// =============================================================================
// Utility Functions
// =============================================================================

/// @brief Create a simple action node
inline BehaviorNodePtr make_action(ActionCallback callback) {
    return std::make_unique<ActionNode>(std::move(callback));
}

/// @brief Create a simple condition node
inline BehaviorNodePtr make_condition(ConditionCallback callback) {
    return std::make_unique<ConditionNode>(std::move(callback));
}

/// @brief Create a wait node
inline BehaviorNodePtr make_wait(float duration) {
    return std::make_unique<WaitNode>(duration);
}

/// @brief Convert node status to string
const char* node_status_to_string(NodeStatus status);

/// @brief Convert node type to string
const char* node_type_to_string(NodeType type);

} // namespace void_ai
