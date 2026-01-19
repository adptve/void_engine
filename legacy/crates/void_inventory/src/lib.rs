//! Void Inventory - Item and Inventory System
//!
//! This crate provides inventory management and item systems.
//!
//! # Features
//!
//! - Item definitions with properties
//! - Inventory component with configurable slots
//! - Item stacking with max stack sizes
//! - Equipment slots system
//! - Pickup components
//! - Item use and consumption
//!
//! # Example
//!
//! ```ignore
//! use void_inventory::prelude::*;
//!
//! // Create an item
//! let sword = ItemDefinition::new("iron_sword", "Iron Sword")
//!     .with_category(ItemCategory::Weapon)
//!     .with_rarity(ItemRarity::Common)
//!     .with_max_stack(1);
//!
//! // Create inventory
//! let mut inventory = Inventory::new(20);
//! inventory.add_item(ItemStack::new(sword.id.clone(), 1));
//! ```

pub mod equipment;
pub mod inventory;
pub mod item;
pub mod pickup;

pub mod prelude {
    pub use crate::equipment::{EquipmentSlot, EquipmentComponent};
    pub use crate::inventory::{Inventory, InventoryEvent};
    pub use crate::item::{ItemCategory, ItemDefinition, ItemProperty, ItemRarity, ItemStack};
    pub use crate::pickup::{PickupComponent, PickupMode};
}

pub use prelude::*;
