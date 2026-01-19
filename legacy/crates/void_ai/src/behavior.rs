//! Behavior Tree implementation

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Status of a behavior node execution
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum BehaviorStatus {
    /// Node succeeded
    Success,
    /// Node failed
    Failure,
    /// Node is still running
    Running,
}

/// Behavior node types
#[derive(Debug, Clone)]
pub enum BehaviorNode {
    /// Sequence - runs children in order, fails if any fails
    Sequence(Vec<BehaviorNode>),
    /// Selector/Fallback - runs children until one succeeds
    Selector(Vec<BehaviorNode>),
    /// Parallel - runs all children simultaneously
    Parallel {
        children: Vec<BehaviorNode>,
        /// Minimum successes required
        success_threshold: usize,
        /// Minimum failures to fail
        failure_threshold: usize,
    },
    /// Condition - checks a named condition
    Condition(String),
    /// Action - executes a named action
    Action(String),
    /// Inverter - inverts child result
    Inverter(Box<BehaviorNode>),
    /// Succeeder - always returns success
    Succeeder(Box<BehaviorNode>),
    /// Repeater - repeats child N times
    Repeater {
        child: Box<BehaviorNode>,
        times: u32,
    },
    /// RepeatUntilFail - repeats until child fails
    RepeatUntilFail(Box<BehaviorNode>),
    /// Wait - waits for specified time
    Wait(f32),
}

impl BehaviorNode {
    /// Create a sequence node
    pub fn sequence(children: Vec<BehaviorNode>) -> Self {
        Self::Sequence(children)
    }

    /// Create a selector node
    pub fn selector(children: Vec<BehaviorNode>) -> Self {
        Self::Selector(children)
    }

    /// Create a parallel node
    pub fn parallel(children: Vec<BehaviorNode>, success_threshold: usize) -> Self {
        Self::Parallel {
            children,
            success_threshold,
            failure_threshold: 1,
        }
    }

    /// Create a condition node
    pub fn condition(name: impl Into<String>) -> Self {
        Self::Condition(name.into())
    }

    /// Create an action node
    pub fn action(name: impl Into<String>) -> Self {
        Self::Action(name.into())
    }

    /// Create an inverter node
    pub fn inverter(child: BehaviorNode) -> Self {
        Self::Inverter(Box::new(child))
    }

    /// Create a repeater node
    pub fn repeater(child: BehaviorNode, times: u32) -> Self {
        Self::Repeater {
            child: Box::new(child),
            times,
        }
    }

    /// Create a wait node
    pub fn wait(seconds: f32) -> Self {
        Self::Wait(seconds)
    }
}

/// Context for behavior tree execution
pub trait BehaviorContext {
    /// Check a condition by name
    fn check_condition(&self, name: &str) -> bool;
    /// Execute an action by name
    fn execute_action(&mut self, name: &str) -> BehaviorStatus;
}

/// Behavior tree execution state
#[derive(Debug, Default)]
pub struct BehaviorTreeState {
    /// Running node index per composite node
    running_indices: HashMap<usize, usize>,
    /// Timer for wait nodes
    wait_timer: f32,
    /// Repeat counter
    repeat_counter: u32,
    /// Node counter for generating IDs
    node_counter: usize,
}

impl BehaviorTreeState {
    /// Create new state
    pub fn new() -> Self {
        Self::default()
    }

    /// Reset state
    pub fn reset(&mut self) {
        self.running_indices.clear();
        self.wait_timer = 0.0;
        self.repeat_counter = 0;
        self.node_counter = 0;
    }
}

/// Behavior tree
pub struct BehaviorTree {
    /// Root node
    root: BehaviorNode,
}

impl BehaviorTree {
    /// Create a new behavior tree
    pub fn new(root: BehaviorNode) -> Self {
        Self { root }
    }

    /// Execute the behavior tree
    pub fn execute<C: BehaviorContext>(
        &self,
        context: &mut C,
        state: &mut BehaviorTreeState,
        delta_time: f32,
    ) -> BehaviorStatus {
        state.node_counter = 0;
        Self::execute_node(&self.root, context, state, delta_time)
    }

    fn execute_node<C: BehaviorContext>(
        node: &BehaviorNode,
        context: &mut C,
        state: &mut BehaviorTreeState,
        delta_time: f32,
    ) -> BehaviorStatus {
        let node_id = state.node_counter;
        state.node_counter += 1;

        match node {
            BehaviorNode::Sequence(children) => {
                let start_idx = *state.running_indices.get(&node_id).unwrap_or(&0);

                for (i, child) in children.iter().enumerate().skip(start_idx) {
                    let status = Self::execute_node(child, context, state, delta_time);
                    match status {
                        BehaviorStatus::Failure => {
                            state.running_indices.remove(&node_id);
                            return BehaviorStatus::Failure;
                        }
                        BehaviorStatus::Running => {
                            state.running_indices.insert(node_id, i);
                            return BehaviorStatus::Running;
                        }
                        BehaviorStatus::Success => continue,
                    }
                }

                state.running_indices.remove(&node_id);
                BehaviorStatus::Success
            }

            BehaviorNode::Selector(children) => {
                let start_idx = *state.running_indices.get(&node_id).unwrap_or(&0);

                for (i, child) in children.iter().enumerate().skip(start_idx) {
                    let status = Self::execute_node(child, context, state, delta_time);
                    match status {
                        BehaviorStatus::Success => {
                            state.running_indices.remove(&node_id);
                            return BehaviorStatus::Success;
                        }
                        BehaviorStatus::Running => {
                            state.running_indices.insert(node_id, i);
                            return BehaviorStatus::Running;
                        }
                        BehaviorStatus::Failure => continue,
                    }
                }

                state.running_indices.remove(&node_id);
                BehaviorStatus::Failure
            }

            BehaviorNode::Parallel {
                children,
                success_threshold,
                failure_threshold,
            } => {
                let mut successes = 0;
                let mut failures = 0;
                let mut any_running = false;

                for child in children {
                    let status = Self::execute_node(child, context, state, delta_time);
                    match status {
                        BehaviorStatus::Success => successes += 1,
                        BehaviorStatus::Failure => failures += 1,
                        BehaviorStatus::Running => any_running = true,
                    }
                }

                if successes >= *success_threshold {
                    BehaviorStatus::Success
                } else if failures >= *failure_threshold {
                    BehaviorStatus::Failure
                } else if any_running {
                    BehaviorStatus::Running
                } else {
                    BehaviorStatus::Failure
                }
            }

            BehaviorNode::Condition(name) => {
                if context.check_condition(name) {
                    BehaviorStatus::Success
                } else {
                    BehaviorStatus::Failure
                }
            }

            BehaviorNode::Action(name) => context.execute_action(name),

            BehaviorNode::Inverter(child) => {
                match Self::execute_node(child, context, state, delta_time) {
                    BehaviorStatus::Success => BehaviorStatus::Failure,
                    BehaviorStatus::Failure => BehaviorStatus::Success,
                    BehaviorStatus::Running => BehaviorStatus::Running,
                }
            }

            BehaviorNode::Succeeder(child) => {
                let status = Self::execute_node(child, context, state, delta_time);
                if status == BehaviorStatus::Running {
                    BehaviorStatus::Running
                } else {
                    BehaviorStatus::Success
                }
            }

            BehaviorNode::Repeater { child, times } => {
                if state.repeat_counter < *times {
                    let status = Self::execute_node(child, context, state, delta_time);
                    match status {
                        BehaviorStatus::Running => BehaviorStatus::Running,
                        _ => {
                            state.repeat_counter += 1;
                            if state.repeat_counter >= *times {
                                state.repeat_counter = 0;
                                BehaviorStatus::Success
                            } else {
                                BehaviorStatus::Running
                            }
                        }
                    }
                } else {
                    state.repeat_counter = 0;
                    BehaviorStatus::Success
                }
            }

            BehaviorNode::RepeatUntilFail(child) => {
                let status = Self::execute_node(child, context, state, delta_time);
                match status {
                    BehaviorStatus::Failure => BehaviorStatus::Success,
                    _ => BehaviorStatus::Running,
                }
            }

            BehaviorNode::Wait(duration) => {
                state.wait_timer += delta_time;
                if state.wait_timer >= *duration {
                    state.wait_timer = 0.0;
                    BehaviorStatus::Success
                } else {
                    BehaviorStatus::Running
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestContext {
        conditions: HashMap<String, bool>,
        action_results: HashMap<String, BehaviorStatus>,
    }

    impl BehaviorContext for TestContext {
        fn check_condition(&self, name: &str) -> bool {
            *self.conditions.get(name).unwrap_or(&false)
        }

        fn execute_action(&mut self, name: &str) -> BehaviorStatus {
            *self.action_results.get(name).unwrap_or(&BehaviorStatus::Success)
        }
    }

    #[test]
    fn test_sequence() {
        let tree = BehaviorTree::new(BehaviorNode::sequence(vec![
            BehaviorNode::action("action1"),
            BehaviorNode::action("action2"),
        ]));

        let mut context = TestContext {
            conditions: HashMap::new(),
            action_results: HashMap::new(),
        };
        let mut state = BehaviorTreeState::new();

        let status = tree.execute(&mut context, &mut state, 0.016);
        assert_eq!(status, BehaviorStatus::Success);
    }

    #[test]
    fn test_sequence_failure() {
        let tree = BehaviorTree::new(BehaviorNode::sequence(vec![
            BehaviorNode::action("action1"),
            BehaviorNode::action("action2"),
        ]));

        let mut context = TestContext {
            conditions: HashMap::new(),
            action_results: HashMap::from([("action1".to_string(), BehaviorStatus::Failure)]),
        };
        let mut state = BehaviorTreeState::new();

        let status = tree.execute(&mut context, &mut state, 0.016);
        assert_eq!(status, BehaviorStatus::Failure);
    }

    #[test]
    fn test_selector() {
        let tree = BehaviorTree::new(BehaviorNode::selector(vec![
            BehaviorNode::action("action1"),
            BehaviorNode::action("action2"),
        ]));

        let mut context = TestContext {
            conditions: HashMap::new(),
            action_results: HashMap::from([("action1".to_string(), BehaviorStatus::Failure)]),
        };
        let mut state = BehaviorTreeState::new();

        // First fails, second succeeds
        let status = tree.execute(&mut context, &mut state, 0.016);
        assert_eq!(status, BehaviorStatus::Success);
    }

    #[test]
    fn test_condition() {
        let tree = BehaviorTree::new(BehaviorNode::sequence(vec![
            BehaviorNode::condition("is_enemy_visible"),
            BehaviorNode::action("attack"),
        ]));

        let mut context = TestContext {
            conditions: HashMap::from([("is_enemy_visible".to_string(), true)]),
            action_results: HashMap::new(),
        };
        let mut state = BehaviorTreeState::new();

        let status = tree.execute(&mut context, &mut state, 0.016);
        assert_eq!(status, BehaviorStatus::Success);

        // Now condition fails
        context.conditions.insert("is_enemy_visible".to_string(), false);
        let status = tree.execute(&mut context, &mut state, 0.016);
        assert_eq!(status, BehaviorStatus::Failure);
    }

    #[test]
    fn test_inverter() {
        let tree = BehaviorTree::new(BehaviorNode::inverter(BehaviorNode::condition("test")));

        let mut context = TestContext {
            conditions: HashMap::from([("test".to_string(), false)]),
            action_results: HashMap::new(),
        };
        let mut state = BehaviorTreeState::new();

        // Inverted: false becomes success
        let status = tree.execute(&mut context, &mut state, 0.016);
        assert_eq!(status, BehaviorStatus::Success);
    }
}
