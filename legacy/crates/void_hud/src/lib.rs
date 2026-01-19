//! Void HUD - UI Elements System
//!
//! This crate provides HUD and UI elements for games.
//!
//! # Features
//!
//! - Health/resource bars
//! - Floating damage numbers
//! - Crosshairs
//! - Minimap support
//! - Notification system
//! - Interaction prompts
//!
//! # Example
//!
//! ```ignore
//! use void_hud::prelude::*;
//!
//! // Create a health bar
//! let health_bar = HealthBar::new()
//!     .with_position(20.0, 20.0)
//!     .with_size(200.0, 20.0);
//!
//! // Show damage number
//! hud.spawn_damage_number(50.0, world_pos, DamageNumberStyle::default());
//! ```

pub mod bar;
pub mod crosshair;
pub mod damage_number;
pub mod minimap;
pub mod notification;
pub mod prompt;

pub mod prelude {
    pub use crate::bar::{BarStyle, HealthBar, ResourceBar};
    pub use crate::crosshair::{Crosshair, CrosshairStyle};
    pub use crate::damage_number::{DamageNumber, DamageNumberStyle};
    pub use crate::minimap::{Minimap, MinimapIcon};
    pub use crate::notification::{Notification, NotificationManager, NotificationType};
    pub use crate::prompt::{InteractionPrompt, PromptStyle};
}

pub use prelude::*;
