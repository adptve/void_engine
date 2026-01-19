//! Finite State Machine (FSM) implementation

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::hash::Hash;

/// A state in the state machine
pub trait State: Clone + Eq + Hash {
    /// Called when entering this state
    fn on_enter(&self) {}
    /// Called when exiting this state
    fn on_exit(&self) {}
    /// Called every update while in this state
    fn on_update(&self, _delta_time: f32) {}
}

/// Transition condition
pub type TransitionCondition<C> = Box<dyn Fn(&C) -> bool + Send + Sync>;

/// A state transition
pub struct Transition<S, C> {
    /// Target state
    pub to: S,
    /// Condition function
    pub condition: TransitionCondition<C>,
    /// Priority (higher = checked first)
    pub priority: i32,
}

impl<S, C> Transition<S, C> {
    /// Create a new transition
    pub fn new<F>(to: S, condition: F) -> Self
    where
        F: Fn(&C) -> bool + Send + Sync + 'static,
    {
        Self {
            to,
            condition: Box::new(condition),
            priority: 0,
        }
    }

    /// Set priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Check if transition should occur
    pub fn should_transition(&self, context: &C) -> bool {
        (self.condition)(context)
    }
}

/// Finite State Machine
pub struct StateMachine<S, C>
where
    S: State,
{
    /// Current state
    current: S,
    /// Previous state
    previous: Option<S>,
    /// Transitions from each state
    transitions: HashMap<S, Vec<Transition<S, C>>>,
    /// Global transitions (checked from any state)
    global_transitions: Vec<Transition<S, C>>,
    /// Whether currently transitioning
    transitioning: bool,
}

impl<S, C> StateMachine<S, C>
where
    S: State,
{
    /// Create a new state machine
    pub fn new(initial: S) -> Self {
        initial.on_enter();
        Self {
            current: initial,
            previous: None,
            transitions: HashMap::new(),
            global_transitions: Vec::new(),
            transitioning: false,
        }
    }

    /// Add a transition
    pub fn add_transition<F>(&mut self, from: S, to: S, condition: F)
    where
        F: Fn(&C) -> bool + Send + Sync + 'static,
    {
        self.transitions
            .entry(from)
            .or_default()
            .push(Transition::new(to, condition));
    }

    /// Add a transition with priority
    pub fn add_transition_priority<F>(&mut self, from: S, to: S, condition: F, priority: i32)
    where
        F: Fn(&C) -> bool + Send + Sync + 'static,
    {
        self.transitions
            .entry(from)
            .or_default()
            .push(Transition::new(to, condition).with_priority(priority));
    }

    /// Add a global transition (can occur from any state)
    pub fn add_global_transition<F>(&mut self, to: S, condition: F)
    where
        F: Fn(&C) -> bool + Send + Sync + 'static,
    {
        self.global_transitions.push(Transition::new(to, condition));
    }

    /// Get current state
    pub fn current(&self) -> &S {
        &self.current
    }

    /// Get previous state
    pub fn previous(&self) -> Option<&S> {
        self.previous.as_ref()
    }

    /// Force transition to a state
    pub fn force_transition(&mut self, to: S) {
        self.current.on_exit();
        self.previous = Some(self.current.clone());
        self.current = to;
        self.current.on_enter();
    }

    /// Update the state machine
    pub fn update(&mut self, context: &C, delta_time: f32) {
        // Update current state
        self.current.on_update(delta_time);

        // Check global transitions first
        for transition in &self.global_transitions {
            if self.current != transition.to && transition.should_transition(context) {
                self.force_transition(transition.to.clone());
                return;
            }
        }

        // Check state-specific transitions
        if let Some(transitions) = self.transitions.get(&self.current) {
            // Sort by priority (higher first)
            let mut sorted: Vec<_> = transitions.iter().collect();
            sorted.sort_by(|a, b| b.priority.cmp(&a.priority));

            for transition in sorted {
                if transition.should_transition(context) {
                    self.force_transition(transition.to.clone());
                    return;
                }
            }
        }
    }

    /// Check if in a specific state
    pub fn is_in(&self, state: &S) -> bool {
        &self.current == state
    }
}

/// Simple state enum for basic use cases
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum SimpleState {
    Idle,
    Active,
    Paused,
    Custom(u32),
}

impl State for SimpleState {}

// Implement State for common types
impl State for u32 {}
impl State for i32 {}
impl State for String {}
impl State for &'static str {}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    enum TestState {
        Idle,
        Walking,
        Running,
    }

    impl State for TestState {}

    struct TestContext {
        speed: f32,
        stamina: f32,
    }

    #[test]
    fn test_state_machine() {
        let mut fsm = StateMachine::<TestState, TestContext>::new(TestState::Idle);

        assert!(fsm.is_in(&TestState::Idle));
        assert_eq!(*fsm.current(), TestState::Idle);
    }

    #[test]
    fn test_transitions() {
        let mut fsm = StateMachine::<TestState, TestContext>::new(TestState::Idle);

        fsm.add_transition(TestState::Idle, TestState::Walking, |ctx| ctx.speed > 0.0);
        fsm.add_transition(TestState::Walking, TestState::Running, |ctx| {
            ctx.speed > 5.0 && ctx.stamina > 0.0
        });
        fsm.add_transition(TestState::Running, TestState::Walking, |ctx| {
            ctx.stamina <= 0.0
        });

        let mut context = TestContext {
            speed: 0.0,
            stamina: 100.0,
        };

        // Still idle
        fsm.update(&context, 0.016);
        assert!(fsm.is_in(&TestState::Idle));

        // Start walking
        context.speed = 2.0;
        fsm.update(&context, 0.016);
        assert!(fsm.is_in(&TestState::Walking));

        // Start running
        context.speed = 10.0;
        fsm.update(&context, 0.016);
        assert!(fsm.is_in(&TestState::Running));

        // Out of stamina
        context.stamina = 0.0;
        fsm.update(&context, 0.016);
        assert!(fsm.is_in(&TestState::Walking));
    }

    #[test]
    fn test_global_transition() {
        let mut fsm = StateMachine::<TestState, TestContext>::new(TestState::Running);

        // Global "stop" transition
        fsm.add_global_transition(TestState::Idle, |ctx| ctx.speed == 0.0);

        let mut context = TestContext {
            speed: 5.0,
            stamina: 100.0,
        };

        fsm.update(&context, 0.016);
        assert!(fsm.is_in(&TestState::Running));

        context.speed = 0.0;
        fsm.update(&context, 0.016);
        assert!(fsm.is_in(&TestState::Idle));
    }

    #[test]
    fn test_force_transition() {
        let mut fsm = StateMachine::<TestState, TestContext>::new(TestState::Idle);

        fsm.force_transition(TestState::Running);
        assert!(fsm.is_in(&TestState::Running));
        assert_eq!(fsm.previous(), Some(&TestState::Idle));
    }
}
