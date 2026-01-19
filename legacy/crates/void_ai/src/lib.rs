//! Void AI - AI and Navigation System
//!
//! This crate provides AI behaviors and navigation for game entities.
//!
//! # Features
//!
//! - Finite State Machines (FSM)
//! - Behavior Trees
//! - Steering behaviors
//! - Basic pathfinding (A*)
//! - Perception/sensing system
//!
//! # Example
//!
//! ```ignore
//! use void_ai::prelude::*;
//!
//! // Create a simple FSM
//! let mut fsm = StateMachine::new(EnemyState::Idle);
//! fsm.add_transition(EnemyState::Idle, EnemyState::Chase, |ctx| ctx.player_visible);
//! fsm.add_transition(EnemyState::Chase, EnemyState::Attack, |ctx| ctx.in_attack_range);
//! ```

pub mod behavior;
pub mod navigation;
pub mod perception;
pub mod state_machine;
pub mod steering;

pub mod prelude {
    pub use crate::behavior::{BehaviorNode, BehaviorStatus, BehaviorTree};
    pub use crate::navigation::{NavAgent, NavMesh, NavPath};
    pub use crate::perception::{PerceptionComponent, Stimulus, StimulusType};
    pub use crate::state_machine::{State, StateMachine, Transition};
    pub use crate::steering::{SteeringBehavior, SteeringOutput};
}

pub use prelude::*;
