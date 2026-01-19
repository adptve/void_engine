//! Item definitions and stacks

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Item category
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum ItemCategory {
    /// Weapons (swords, guns, etc.)
    Weapon,
    /// Armor pieces
    Armor,
    /// Consumables (potions, food, etc.)
    Consumable,
    /// Materials for crafting
    Material,
    /// Quest items
    Quest,
    /// Key items (cannot be dropped)
    Key,
    /// Ammunition
    Ammo,
    /// Currency
    Currency,
    /// Misc items
    Misc,
    /// Custom category
    Custom(u32),
}

impl Default for ItemCategory {
    fn default() -> Self {
        Self::Misc
    }
}

/// Item rarity
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub enum ItemRarity {
    /// Common items
    Common,
    /// Uncommon items
    Uncommon,
    /// Rare items
    Rare,
    /// Epic items
    Epic,
    /// Legendary items
    Legendary,
    /// Unique items (one per world)
    Unique,
}

impl Default for ItemRarity {
    fn default() -> Self {
        Self::Common
    }
}

impl ItemRarity {
    /// Get color associated with rarity (RGB)
    pub fn color(&self) -> [f32; 3] {
        match self {
            Self::Common => [1.0, 1.0, 1.0],     // White
            Self::Uncommon => [0.0, 1.0, 0.0],   // Green
            Self::Rare => [0.0, 0.5, 1.0],       // Blue
            Self::Epic => [0.5, 0.0, 1.0],       // Purple
            Self::Legendary => [1.0, 0.5, 0.0],  // Orange
            Self::Unique => [1.0, 1.0, 0.0],     // Gold
        }
    }
}

/// Item property value
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ItemProperty {
    /// Integer value
    Int(i64),
    /// Float value
    Float(f64),
    /// Boolean value
    Bool(bool),
    /// String value
    String(String),
    /// Array of values
    Array(Vec<ItemProperty>),
}

impl ItemProperty {
    /// Get as integer
    pub fn as_int(&self) -> Option<i64> {
        match self {
            Self::Int(v) => Some(*v),
            Self::Float(v) => Some(*v as i64),
            _ => None,
        }
    }

    /// Get as float
    pub fn as_float(&self) -> Option<f64> {
        match self {
            Self::Float(v) => Some(*v),
            Self::Int(v) => Some(*v as f64),
            _ => None,
        }
    }

    /// Get as boolean
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Self::Bool(v) => Some(*v),
            _ => None,
        }
    }

    /// Get as string
    pub fn as_string(&self) -> Option<&str> {
        match self {
            Self::String(v) => Some(v),
            _ => None,
        }
    }
}

/// Item definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ItemDefinition {
    /// Unique identifier
    pub id: String,
    /// Display name
    pub name: String,
    /// Description
    pub description: String,
    /// Category
    pub category: ItemCategory,
    /// Rarity
    pub rarity: ItemRarity,
    /// Maximum stack size (1 = not stackable)
    pub max_stack: u32,
    /// Base value/price
    pub value: u32,
    /// Weight per item
    pub weight: f32,
    /// Icon path
    pub icon: String,
    /// 3D model path (for world/pickup)
    pub model: String,
    /// Whether the item can be dropped
    pub droppable: bool,
    /// Whether the item can be traded
    pub tradeable: bool,
    /// Whether the item can be consumed/used
    pub usable: bool,
    /// Custom properties
    pub properties: HashMap<String, ItemProperty>,
    /// Tags for filtering
    pub tags: Vec<String>,
}

impl ItemDefinition {
    /// Create a new item definition
    pub fn new(id: impl Into<String>, name: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            name: name.into(),
            description: String::new(),
            category: ItemCategory::default(),
            rarity: ItemRarity::default(),
            max_stack: 1,
            value: 0,
            weight: 0.0,
            icon: String::new(),
            model: String::new(),
            droppable: true,
            tradeable: true,
            usable: false,
            properties: HashMap::new(),
            tags: Vec::new(),
        }
    }

    /// Set description
    pub fn with_description(mut self, desc: impl Into<String>) -> Self {
        self.description = desc.into();
        self
    }

    /// Set category
    pub fn with_category(mut self, category: ItemCategory) -> Self {
        self.category = category;
        self
    }

    /// Set rarity
    pub fn with_rarity(mut self, rarity: ItemRarity) -> Self {
        self.rarity = rarity;
        self
    }

    /// Set max stack size
    pub fn with_max_stack(mut self, max: u32) -> Self {
        self.max_stack = max.max(1);
        self
    }

    /// Set value
    pub fn with_value(mut self, value: u32) -> Self {
        self.value = value;
        self
    }

    /// Set weight
    pub fn with_weight(mut self, weight: f32) -> Self {
        self.weight = weight;
        self
    }

    /// Set icon path
    pub fn with_icon(mut self, path: impl Into<String>) -> Self {
        self.icon = path.into();
        self
    }

    /// Set model path
    pub fn with_model(mut self, path: impl Into<String>) -> Self {
        self.model = path.into();
        self
    }

    /// Set usable
    pub fn with_usable(mut self, usable: bool) -> Self {
        self.usable = usable;
        self
    }

    /// Make non-droppable
    pub fn non_droppable(mut self) -> Self {
        self.droppable = false;
        self
    }

    /// Make non-tradeable
    pub fn non_tradeable(mut self) -> Self {
        self.tradeable = false;
        self
    }

    /// Add a property
    pub fn with_property(mut self, key: impl Into<String>, value: ItemProperty) -> Self {
        self.properties.insert(key.into(), value);
        self
    }

    /// Add a tag
    pub fn with_tag(mut self, tag: impl Into<String>) -> Self {
        self.tags.push(tag.into());
        self
    }

    /// Check if item has a tag
    pub fn has_tag(&self, tag: &str) -> bool {
        self.tags.iter().any(|t| t == tag)
    }

    /// Get property value
    pub fn get_property(&self, key: &str) -> Option<&ItemProperty> {
        self.properties.get(key)
    }

    /// Check if stackable
    pub fn is_stackable(&self) -> bool {
        self.max_stack > 1
    }
}

impl Default for ItemDefinition {
    fn default() -> Self {
        Self::new("unknown", "Unknown Item")
    }
}

/// A stack of items in an inventory
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ItemStack {
    /// Item ID (references ItemDefinition)
    pub item_id: String,
    /// Quantity
    pub quantity: u32,
    /// Instance-specific properties (durability, enchantments, etc.)
    pub instance_data: HashMap<String, ItemProperty>,
}

impl ItemStack {
    /// Create a new item stack
    pub fn new(item_id: impl Into<String>, quantity: u32) -> Self {
        Self {
            item_id: item_id.into(),
            quantity: quantity.max(1),
            instance_data: HashMap::new(),
        }
    }

    /// Create a single item
    pub fn single(item_id: impl Into<String>) -> Self {
        Self::new(item_id, 1)
    }

    /// Set instance data
    pub fn with_data(mut self, key: impl Into<String>, value: ItemProperty) -> Self {
        self.instance_data.insert(key.into(), value);
        self
    }

    /// Set durability
    pub fn with_durability(self, current: f32, max: f32) -> Self {
        self.with_data("durability", ItemProperty::Float(current as f64))
            .with_data("max_durability", ItemProperty::Float(max as f64))
    }

    /// Get durability (current, max)
    pub fn durability(&self) -> Option<(f32, f32)> {
        let current = self.instance_data.get("durability")?.as_float()? as f32;
        let max = self.instance_data.get("max_durability")?.as_float()? as f32;
        Some((current, max))
    }

    /// Check if this stack is empty
    pub fn is_empty(&self) -> bool {
        self.quantity == 0
    }

    /// Add to this stack (returns overflow if any)
    pub fn add(&mut self, amount: u32, max_stack: u32) -> u32 {
        let space = max_stack.saturating_sub(self.quantity);
        let to_add = amount.min(space);
        self.quantity += to_add;
        amount - to_add // Return overflow
    }

    /// Remove from this stack (returns amount actually removed)
    pub fn remove(&mut self, amount: u32) -> u32 {
        let to_remove = amount.min(self.quantity);
        self.quantity -= to_remove;
        to_remove
    }

    /// Split this stack
    pub fn split(&mut self, amount: u32) -> Option<ItemStack> {
        if amount > 0 && amount < self.quantity {
            let split_amount = amount.min(self.quantity);
            self.quantity -= split_amount;
            Some(ItemStack {
                item_id: self.item_id.clone(),
                quantity: split_amount,
                instance_data: self.instance_data.clone(),
            })
        } else {
            None
        }
    }

    /// Merge another stack into this one
    pub fn merge(&mut self, other: &mut ItemStack, max_stack: u32) -> bool {
        if self.item_id != other.item_id {
            return false;
        }
        let overflow = self.add(other.quantity, max_stack);
        other.quantity = overflow;
        true
    }

    /// Check if stacks can be merged (same item ID and compatible data)
    pub fn can_merge(&self, other: &ItemStack) -> bool {
        self.item_id == other.item_id && self.instance_data.is_empty() && other.instance_data.is_empty()
    }
}

impl Default for ItemStack {
    fn default() -> Self {
        Self::single("unknown")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_item_definition() {
        let item = ItemDefinition::new("health_potion", "Health Potion")
            .with_category(ItemCategory::Consumable)
            .with_rarity(ItemRarity::Common)
            .with_max_stack(10)
            .with_usable(true)
            .with_value(50);

        assert_eq!(item.id, "health_potion");
        assert!(item.is_stackable());
        assert!(item.usable);
    }

    #[test]
    fn test_item_stack() {
        let mut stack = ItemStack::new("gold_coin", 50);

        assert_eq!(stack.quantity, 50);

        // Add more
        let overflow = stack.add(60, 99);
        assert_eq!(stack.quantity, 99);
        assert_eq!(overflow, 11);

        // Remove some
        let removed = stack.remove(20);
        assert_eq!(removed, 20);
        assert_eq!(stack.quantity, 79);
    }

    #[test]
    fn test_stack_split() {
        let mut stack = ItemStack::new("arrows", 50);

        let split = stack.split(20);
        assert!(split.is_some());
        assert_eq!(stack.quantity, 30);
        assert_eq!(split.unwrap().quantity, 20);
    }

    #[test]
    fn test_stack_merge() {
        let mut stack1 = ItemStack::new("gold", 30);
        let mut stack2 = ItemStack::new("gold", 40);

        let merged = stack1.merge(&mut stack2, 50);
        assert!(merged);
        assert_eq!(stack1.quantity, 50);
        assert_eq!(stack2.quantity, 20); // Overflow
    }

    #[test]
    fn test_durability() {
        let stack = ItemStack::single("sword").with_durability(80.0, 100.0);

        let (current, max) = stack.durability().unwrap();
        assert_eq!(current, 80.0);
        assert_eq!(max, 100.0);
    }
}
