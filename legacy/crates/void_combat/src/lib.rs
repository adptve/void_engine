//! Void Combat - Health, Damage, and Weapons System
//!
//! This crate provides combat functionality for the Void Engine.
//!
//! # Features
//!
//! - Health component with max health, regeneration, and invulnerability
//! - Damage types with resistances
//! - Weapon system (hitscan, projectile, melee)
//! - Hit feedback (damage numbers, effects)
//! - Death and respawn handling
//!
//! # Example
//!
//! ```ignore
//! use void_combat::prelude::*;
//!
//! // Create a health component
//! let health = HealthComponent::new(100.0)
//!     .with_regeneration(5.0) // 5 HP per second
//!     .with_resistance(DamageType::Fire, 0.5); // 50% fire resistance
//!
//! // Apply damage
//! let damage = DamageInfo::new(25.0, DamageType::Physical)
//!     .with_source(attacker_entity);
//!
//! health.apply_damage(&damage);
//! ```

pub mod damage;
pub mod health;
pub mod weapon;

pub mod prelude {
    pub use crate::damage::{DamageInfo, DamageType};
    pub use crate::health::{HealthComponent, HealthEvent};
    pub use crate::weapon::{WeaponComponent, WeaponType, WeaponStats};
}

pub use prelude::*;
