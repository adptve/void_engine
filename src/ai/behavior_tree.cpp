/// @file behavior_tree.cpp
/// @brief Behavior tree implementation for void_ai module

#include <void_engine/ai/behavior_tree.hpp>

#include <algorithm>

namespace void_ai {

// =============================================================================
// CompositeNode Implementation
// =============================================================================

void CompositeNode::add_child(BehaviorNodePtr child) {
    if (m_blackboard) {
        child->set_blackboard(m_blackboard);
    }
    m_children.push_back(std::move(child));
}

void CompositeNode::remove_child(std::size_t index) {
    if (index < m_children.size()) {
        m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void CompositeNode::clear_children() {
    m_children.clear();
}

IBehaviorNode* CompositeNode::child(std::size_t index) const {
    return index < m_children.size() ? m_children[index].get() : nullptr;
}

void CompositeNode::reset() {
    IBehaviorNode::reset();
    m_current_child = 0;
    for (auto& child : m_children) {
        child->reset();
    }
}

void CompositeNode::set_blackboard(IBlackboard* bb) {
    m_blackboard = bb;
    for (auto& child : m_children) {
        child->set_blackboard(bb);
    }
}

// =============================================================================
// SequenceNode Implementation
// =============================================================================

NodeStatus SequenceNode::tick(float dt) {
    while (m_current_child < m_children.size()) {
        auto status = m_children[m_current_child]->tick(dt);

        if (status == NodeStatus::Running) {
            m_status = NodeStatus::Running;
            return m_status;
        }

        if (status == NodeStatus::Failure) {
            m_current_child = 0;
            m_status = NodeStatus::Failure;
            return m_status;
        }

        // Success - move to next child
        m_current_child++;
    }

    // All children succeeded
    m_current_child = 0;
    m_status = NodeStatus::Success;
    return m_status;
}

// =============================================================================
// SelectorNode Implementation
// =============================================================================

NodeStatus SelectorNode::tick(float dt) {
    while (m_current_child < m_children.size()) {
        auto status = m_children[m_current_child]->tick(dt);

        if (status == NodeStatus::Running) {
            m_status = NodeStatus::Running;
            return m_status;
        }

        if (status == NodeStatus::Success) {
            m_current_child = 0;
            m_status = NodeStatus::Success;
            return m_status;
        }

        // Failure - try next child
        m_current_child++;
    }

    // All children failed
    m_current_child = 0;
    m_status = NodeStatus::Failure;
    return m_status;
}

// =============================================================================
// ParallelNode Implementation
// =============================================================================

ParallelNode::ParallelNode(ParallelPolicy success_policy, ParallelPolicy failure_policy)
    : m_success_policy(success_policy)
    , m_failure_policy(failure_policy) {
}

NodeStatus ParallelNode::tick(float dt) {
    if (m_child_status.size() != m_children.size()) {
        m_child_status.resize(m_children.size(), NodeStatus::Invalid);
    }

    std::size_t success_count = 0;
    std::size_t failure_count = 0;
    std::size_t running_count = 0;

    for (std::size_t i = 0; i < m_children.size(); ++i) {
        // Only tick children that aren't finished
        if (m_child_status[i] == NodeStatus::Invalid ||
            m_child_status[i] == NodeStatus::Running) {
            m_child_status[i] = m_children[i]->tick(dt);
        }

        switch (m_child_status[i]) {
            case NodeStatus::Success:
                success_count++;
                break;
            case NodeStatus::Failure:
                failure_count++;
                break;
            case NodeStatus::Running:
                running_count++;
                break;
            default:
                break;
        }
    }

    // Check failure policy
    bool failed = false;
    switch (m_failure_policy) {
        case ParallelPolicy::RequireOne:
            failed = failure_count > 0;
            break;
        case ParallelPolicy::RequireAll:
            failed = failure_count == m_children.size();
            break;
        case ParallelPolicy::RequirePercent:
            failed = static_cast<float>(failure_count) / m_children.size() >= m_success_threshold;
            break;
    }

    if (failed) {
        reset();
        m_status = NodeStatus::Failure;
        return m_status;
    }

    // Check success policy
    bool succeeded = false;
    switch (m_success_policy) {
        case ParallelPolicy::RequireOne:
            succeeded = success_count > 0;
            break;
        case ParallelPolicy::RequireAll:
            succeeded = success_count == m_children.size();
            break;
        case ParallelPolicy::RequirePercent:
            succeeded = static_cast<float>(success_count) / m_children.size() >= m_success_threshold;
            break;
    }

    if (succeeded) {
        reset();
        m_status = NodeStatus::Success;
        return m_status;
    }

    // Still running
    m_status = NodeStatus::Running;
    return m_status;
}

void ParallelNode::reset() {
    CompositeNode::reset();
    m_child_status.clear();
}

// =============================================================================
// RandomSelectorNode Implementation
// =============================================================================

RandomSelectorNode::RandomSelectorNode(std::uint32_t seed)
    : m_rng(seed ? seed : std::random_device{}()) {
}

NodeStatus RandomSelectorNode::tick(float dt) {
    if (!m_shuffled && !m_children.empty()) {
        m_shuffle_order.resize(m_children.size());
        for (std::size_t i = 0; i < m_children.size(); ++i) {
            m_shuffle_order[i] = i;
        }
        std::shuffle(m_shuffle_order.begin(), m_shuffle_order.end(), m_rng);
        m_shuffled = true;
    }

    while (m_current_child < m_shuffle_order.size()) {
        std::size_t actual_index = m_shuffle_order[m_current_child];
        auto status = m_children[actual_index]->tick(dt);

        if (status == NodeStatus::Running) {
            m_status = NodeStatus::Running;
            return m_status;
        }

        if (status == NodeStatus::Success) {
            reset();
            m_status = NodeStatus::Success;
            return m_status;
        }

        m_current_child++;
    }

    reset();
    m_status = NodeStatus::Failure;
    return m_status;
}

void RandomSelectorNode::reset() {
    CompositeNode::reset();
    m_shuffled = false;
}

// =============================================================================
// RandomSequenceNode Implementation
// =============================================================================

RandomSequenceNode::RandomSequenceNode(std::uint32_t seed)
    : m_rng(seed ? seed : std::random_device{}()) {
}

NodeStatus RandomSequenceNode::tick(float dt) {
    if (!m_shuffled && !m_children.empty()) {
        m_shuffle_order.resize(m_children.size());
        for (std::size_t i = 0; i < m_children.size(); ++i) {
            m_shuffle_order[i] = i;
        }
        std::shuffle(m_shuffle_order.begin(), m_shuffle_order.end(), m_rng);
        m_shuffled = true;
    }

    while (m_current_child < m_shuffle_order.size()) {
        std::size_t actual_index = m_shuffle_order[m_current_child];
        auto status = m_children[actual_index]->tick(dt);

        if (status == NodeStatus::Running) {
            m_status = NodeStatus::Running;
            return m_status;
        }

        if (status == NodeStatus::Failure) {
            reset();
            m_status = NodeStatus::Failure;
            return m_status;
        }

        m_current_child++;
    }

    reset();
    m_status = NodeStatus::Success;
    return m_status;
}

void RandomSequenceNode::reset() {
    CompositeNode::reset();
    m_shuffled = false;
}

// =============================================================================
// DecoratorNode Implementation
// =============================================================================

void DecoratorNode::set_child(BehaviorNodePtr child) {
    m_child = std::move(child);
    if (m_blackboard && m_child) {
        m_child->set_blackboard(m_blackboard);
    }
}

void DecoratorNode::reset() {
    IBehaviorNode::reset();
    if (m_child) {
        m_child->reset();
    }
}

void DecoratorNode::set_blackboard(IBlackboard* bb) {
    m_blackboard = bb;
    if (m_child) {
        m_child->set_blackboard(bb);
    }
}

// =============================================================================
// InverterNode Implementation
// =============================================================================

NodeStatus InverterNode::tick(float dt) {
    if (!m_child) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    auto child_status = m_child->tick(dt);

    switch (child_status) {
        case NodeStatus::Success:
            m_status = NodeStatus::Failure;
            break;
        case NodeStatus::Failure:
            m_status = NodeStatus::Success;
            break;
        default:
            m_status = child_status;
            break;
    }

    return m_status;
}

// =============================================================================
// RepeaterNode Implementation
// =============================================================================

RepeaterNode::RepeaterNode(std::uint32_t count)
    : m_count(count) {
}

NodeStatus RepeaterNode::tick(float dt) {
    if (!m_child) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    auto child_status = m_child->tick(dt);

    if (child_status == NodeStatus::Running) {
        m_status = NodeStatus::Running;
        return m_status;
    }

    // Child finished, increment count
    m_current++;
    m_child->reset();

    // Check if we should stop
    if (m_count > 0 && m_current >= m_count) {
        m_current = 0;
        m_status = NodeStatus::Success;
        return m_status;
    }

    // Continue repeating
    m_status = NodeStatus::Running;
    return m_status;
}

void RepeaterNode::reset() {
    DecoratorNode::reset();
    m_current = 0;
}

// =============================================================================
// RepeatUntilFailNode Implementation
// =============================================================================

NodeStatus RepeatUntilFailNode::tick(float dt) {
    if (!m_child) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    auto child_status = m_child->tick(dt);

    if (child_status == NodeStatus::Running) {
        m_status = NodeStatus::Running;
        return m_status;
    }

    if (child_status == NodeStatus::Failure) {
        m_status = NodeStatus::Success;
        return m_status;
    }

    // Success - reset and continue
    m_child->reset();
    m_status = NodeStatus::Running;
    return m_status;
}

// =============================================================================
// SucceederNode Implementation
// =============================================================================

NodeStatus SucceederNode::tick(float dt) {
    if (m_child) {
        auto child_status = m_child->tick(dt);
        if (child_status == NodeStatus::Running) {
            m_status = NodeStatus::Running;
            return m_status;
        }
    }

    m_status = NodeStatus::Success;
    return m_status;
}

// =============================================================================
// FailerNode Implementation
// =============================================================================

NodeStatus FailerNode::tick(float dt) {
    if (m_child) {
        auto child_status = m_child->tick(dt);
        if (child_status == NodeStatus::Running) {
            m_status = NodeStatus::Running;
            return m_status;
        }
    }

    m_status = NodeStatus::Failure;
    return m_status;
}

// =============================================================================
// CooldownNode Implementation
// =============================================================================

CooldownNode::CooldownNode(float cooldown_time)
    : m_cooldown_time(cooldown_time) {
}

NodeStatus CooldownNode::tick(float dt) {
    // Update cooldown timer
    if (m_on_cooldown) {
        m_time_remaining -= dt;
        if (m_time_remaining <= 0) {
            m_on_cooldown = false;
        } else {
            m_status = NodeStatus::Failure;
            return m_status;
        }
    }

    if (!m_child) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    auto child_status = m_child->tick(dt);

    if (child_status != NodeStatus::Running) {
        // Start cooldown after child finishes
        m_on_cooldown = true;
        m_time_remaining = m_cooldown_time;
    }

    m_status = child_status;
    return m_status;
}

void CooldownNode::reset() {
    DecoratorNode::reset();
    m_time_remaining = 0;
    m_on_cooldown = false;
}

// =============================================================================
// TimeoutNode Implementation
// =============================================================================

TimeoutNode::TimeoutNode(float timeout)
    : m_timeout(timeout) {
}

NodeStatus TimeoutNode::tick(float dt) {
    if (!m_child) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    m_elapsed += dt;

    if (m_elapsed >= m_timeout) {
        m_child->terminate(NodeStatus::Failure);
        m_elapsed = 0;
        m_status = NodeStatus::Failure;
        return m_status;
    }

    auto child_status = m_child->tick(dt);

    if (child_status != NodeStatus::Running) {
        m_elapsed = 0;
    }

    m_status = child_status;
    return m_status;
}

void TimeoutNode::reset() {
    DecoratorNode::reset();
    m_elapsed = 0;
}

// =============================================================================
// ConditionalNode Implementation
// =============================================================================

ConditionalNode::ConditionalNode(ConditionFunc condition, AbortType abort_type)
    : m_condition(std::move(condition))
    , m_abort_type(abort_type) {
}

void ConditionalNode::initialize() {
    m_was_true = m_condition ? m_condition() : false;
}

NodeStatus ConditionalNode::tick(float dt) {
    if (!m_condition) {
        // No condition, just run child
        if (m_child) {
            m_status = m_child->tick(dt);
        } else {
            m_status = NodeStatus::Failure;
        }
        return m_status;
    }

    bool is_true = m_condition();

    // Handle abort
    if (m_abort_type != AbortType::None && m_was_true && !is_true) {
        if (m_child && m_child->is_running()) {
            m_child->terminate(NodeStatus::Failure);
            m_child->reset();
        }
        m_was_true = false;
        m_status = NodeStatus::Failure;
        return m_status;
    }

    m_was_true = is_true;

    if (!is_true) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    if (m_child) {
        m_status = m_child->tick(dt);
    } else {
        m_status = NodeStatus::Success;
    }

    return m_status;
}

// =============================================================================
// ActionNode Implementation
// =============================================================================

ActionNode::ActionNode(ActionCallback action)
    : m_action(std::move(action)) {
}

ActionNode::ActionNode(std::string_view name, ActionCallback action)
    : m_action(std::move(action)) {
    m_name = std::string(name);
}

NodeStatus ActionNode::tick(float dt) {
    if (!m_action) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    m_status = m_action(dt);
    return m_status;
}

// =============================================================================
// ConditionNode Implementation
// =============================================================================

ConditionNode::ConditionNode(ConditionCallback condition)
    : m_condition(std::move(condition)) {
}

ConditionNode::ConditionNode(std::string_view name, ConditionCallback condition)
    : m_condition(std::move(condition)) {
    m_name = std::string(name);
}

NodeStatus ConditionNode::tick(float /*dt*/) {
    if (!m_condition) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    m_status = m_condition() ? NodeStatus::Success : NodeStatus::Failure;
    return m_status;
}

// =============================================================================
// WaitNode Implementation
// =============================================================================

WaitNode::WaitNode(float duration)
    : m_min_duration(duration)
    , m_max_duration(duration)
    , m_target_duration(duration) {
}

WaitNode::WaitNode(float min_duration, float max_duration)
    : m_min_duration(min_duration)
    , m_max_duration(max_duration) {
    std::uniform_real_distribution<float> dist(m_min_duration, m_max_duration);
    m_target_duration = dist(m_rng);
}

NodeStatus WaitNode::tick(float dt) {
    m_elapsed += dt;

    if (m_elapsed >= m_target_duration) {
        m_elapsed = 0;
        // Pick new target for next time
        if (m_min_duration != m_max_duration) {
            std::uniform_real_distribution<float> dist(m_min_duration, m_max_duration);
            m_target_duration = dist(m_rng);
        }
        m_status = NodeStatus::Success;
        return m_status;
    }

    m_status = NodeStatus::Running;
    return m_status;
}

void WaitNode::reset() {
    IBehaviorNode::reset();
    m_elapsed = 0;
    if (m_min_duration != m_max_duration) {
        std::uniform_real_distribution<float> dist(m_min_duration, m_max_duration);
        m_target_duration = dist(m_rng);
    }
}

// =============================================================================
// SubTreeNode Implementation
// =============================================================================

SubTreeNode::SubTreeNode(BehaviorTree* subtree)
    : m_subtree(subtree) {
}

NodeStatus SubTreeNode::tick(float dt) {
    if (!m_subtree) {
        m_status = NodeStatus::Failure;
        return m_status;
    }

    m_status = m_subtree->tick(dt);
    return m_status;
}

void SubTreeNode::reset() {
    IBehaviorNode::reset();
    if (m_subtree) {
        m_subtree->reset();
    }
}

// =============================================================================
// BehaviorTree Implementation
// =============================================================================

BehaviorTree::BehaviorTree() = default;

BehaviorTree::BehaviorTree(BehaviorNodePtr root)
    : m_root(std::move(root)) {
}

NodeStatus BehaviorTree::tick(float dt) {
    if (!m_root) {
        return NodeStatus::Invalid;
    }

    return m_root->tick(dt);
}

void BehaviorTree::reset() {
    if (m_root) {
        m_root->reset();
    }
}

void BehaviorTree::set_root(BehaviorNodePtr root) {
    m_root = std::move(root);
    if (m_root && m_blackboard) {
        m_root->set_blackboard(m_blackboard);
    }
}

void BehaviorTree::set_blackboard(IBlackboard* bb) {
    m_blackboard = bb;
    if (m_root) {
        m_root->set_blackboard(bb);
    }
}

// =============================================================================
// BehaviorTreeBuilder Implementation
// =============================================================================

BehaviorTreeBuilder::BehaviorTreeBuilder() = default;

BehaviorTreeBuilder& BehaviorTreeBuilder::sequence() {
    auto node = std::make_unique<SequenceNode>();
    push_node(std::move(node), true);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::selector() {
    auto node = std::make_unique<SelectorNode>();
    push_node(std::move(node), true);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::parallel(ParallelPolicy success, ParallelPolicy failure) {
    auto node = std::make_unique<ParallelNode>(success, failure);
    push_node(std::move(node), true);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::random_selector(std::uint32_t seed) {
    auto node = std::make_unique<RandomSelectorNode>(seed);
    push_node(std::move(node), true);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::random_sequence(std::uint32_t seed) {
    auto node = std::make_unique<RandomSequenceNode>(seed);
    push_node(std::move(node), true);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::inverter() {
    auto node = std::make_unique<InverterNode>();
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::repeater(std::uint32_t count) {
    auto node = std::make_unique<RepeaterNode>(count);
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::repeat_until_fail() {
    auto node = std::make_unique<RepeatUntilFailNode>();
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::succeeder() {
    auto node = std::make_unique<SucceederNode>();
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::failer() {
    auto node = std::make_unique<FailerNode>();
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::cooldown(float time) {
    auto node = std::make_unique<CooldownNode>(time);
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::timeout(float time) {
    auto node = std::make_unique<TimeoutNode>(time);
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::conditional(ConditionalNode::ConditionFunc cond, AbortType abort) {
    auto node = std::make_unique<ConditionalNode>(std::move(cond), abort);
    push_node(std::move(node), false);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::action(ActionCallback callback) {
    auto node = std::make_unique<ActionNode>(std::move(callback));
    apply_pending_name(node.get());
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::action(std::string_view name, ActionCallback callback) {
    auto node = std::make_unique<ActionNode>(name, std::move(callback));
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::condition(ConditionCallback callback) {
    auto node = std::make_unique<ConditionNode>(std::move(callback));
    apply_pending_name(node.get());
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::condition(std::string_view name, ConditionCallback callback) {
    auto node = std::make_unique<ConditionNode>(name, std::move(callback));
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::wait(float duration) {
    auto node = std::make_unique<WaitNode>(duration);
    apply_pending_name(node.get());
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::wait(float min_duration, float max_duration) {
    auto node = std::make_unique<WaitNode>(min_duration, max_duration);
    apply_pending_name(node.get());
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::subtree(BehaviorTree* tree) {
    auto node = std::make_unique<SubTreeNode>(tree);
    apply_pending_name(node.get());
    attach_to_parent(std::move(node));
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::end() {
    if (!m_stack.empty()) {
        auto context = std::move(m_stack.back());
        m_stack.pop_back();

        if (m_stack.empty()) {
            m_root = std::move(context.node);
        } else {
            attach_to_parent(std::move(context.node));
        }
    }
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::name(std::string_view name) {
    m_pending_name = std::string(name);
    return *this;
}

BehaviorTreePtr BehaviorTreeBuilder::build() {
    // Close all open scopes
    while (!m_stack.empty()) {
        end();
    }

    auto tree = std::make_unique<BehaviorTree>(std::move(m_root));
    return tree;
}

void BehaviorTreeBuilder::push_node(BehaviorNodePtr node, bool is_composite) {
    apply_pending_name(node.get());

    BuildContext context;
    context.node = std::move(node);
    context.is_composite = is_composite;
    m_stack.push_back(std::move(context));
}

void BehaviorTreeBuilder::attach_to_parent(BehaviorNodePtr node) {
    if (m_stack.empty()) {
        m_root = std::move(node);
        return;
    }

    auto& parent_ctx = m_stack.back();

    if (parent_ctx.is_composite) {
        auto* composite = static_cast<CompositeNode*>(parent_ctx.node.get());
        composite->add_child(std::move(node));
    } else {
        // It's a decorator
        auto* decorator = static_cast<DecoratorNode*>(parent_ctx.node.get());
        decorator->set_child(std::move(node));
        // Pop decorator since it now has its child
        auto context = std::move(m_stack.back());
        m_stack.pop_back();
        attach_to_parent(std::move(context.node));
    }
}

void BehaviorTreeBuilder::apply_pending_name(IBehaviorNode* node) {
    if (!m_pending_name.empty()) {
        node->set_name(m_pending_name);
        m_pending_name.clear();
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* node_status_to_string(NodeStatus status) {
    switch (status) {
        case NodeStatus::Success: return "Success";
        case NodeStatus::Failure: return "Failure";
        case NodeStatus::Running: return "Running";
        case NodeStatus::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NodeType::Sequence: return "Sequence";
        case NodeType::Selector: return "Selector";
        case NodeType::Parallel: return "Parallel";
        case NodeType::RandomSelector: return "RandomSelector";
        case NodeType::RandomSequence: return "RandomSequence";
        case NodeType::Inverter: return "Inverter";
        case NodeType::Repeater: return "Repeater";
        case NodeType::RepeatUntilFail: return "RepeatUntilFail";
        case NodeType::Succeeder: return "Succeeder";
        case NodeType::Failer: return "Failer";
        case NodeType::Cooldown: return "Cooldown";
        case NodeType::Timeout: return "Timeout";
        case NodeType::Conditional: return "Conditional";
        case NodeType::Action: return "Action";
        case NodeType::Condition: return "Condition";
        case NodeType::Wait: return "Wait";
        case NodeType::SubTree: return "SubTree";
        case NodeType::Custom: return "Custom";
        default: return "Unknown";
    }
}

} // namespace void_ai
