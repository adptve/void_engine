//! Void Triggers - Trigger Volume System
//!
//! This crate provides trigger volumes for detecting entity overlaps.
//!
//! # Features
//!
//! - Multiple volume shapes (box, sphere, capsule, cylinder)
//! - Enter/Exit/Stay events
//! - Layer-based filtering
//! - One-shot and repeatable triggers
//! - Cooldown support
//!
//! # Example
//!
//! ```ignore
//! use void_triggers::prelude::*;
//!
//! // Create a box trigger
//! let trigger = TriggerComponent::new(TriggerVolume::box_shape(2.0, 1.0, 2.0))
//!     .with_filter(TriggerFilter::new().with_layer(1))
//!     .on_enter(|entity| println!("Entity {:?} entered!", entity));
//! ```

pub mod events;
pub mod filter;
pub mod system;
pub mod trigger;
pub mod volume;

pub mod prelude {
    pub use crate::events::{TriggerEvent, TriggerEventType};
    pub use crate::filter::TriggerFilter;
    pub use crate::system::TriggerSystem;
    pub use crate::trigger::{TriggerComponent, TriggerMode};
    pub use crate::volume::TriggerVolume;
}

pub use prelude::*;
