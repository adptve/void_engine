//! Equipment system

use crate::item::ItemStack;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Equipment slot types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum EquipmentSlot {
    /// Head armor (helmet, hat)
    Head,
    /// Chest armor
    Chest,
    /// Leg armor
    Legs,
    /// Foot armor (boots)
    Feet,
    /// Hand armor (gloves)
    Hands,
    /// Main hand weapon
    MainHand,
    /// Off hand (shield, second weapon)
    OffHand,
    /// Two-handed weapon (occupies both hands)
    TwoHand,
    /// Accessory slot 1 (ring, amulet)
    Accessory1,
    /// Accessory slot 2
    Accessory2,
    /// Back slot (cape, backpack)
    Back,
    /// Belt slot
    Belt,
    /// Custom slot
    Custom(u32),
}

impl EquipmentSlot {
    /// Get all standard slots
    pub fn all_standard() -> Vec<Self> {
        vec![
            Self::Head,
            Self::Chest,
            Self::Legs,
            Self::Feet,
            Self::Hands,
            Self::MainHand,
            Self::OffHand,
            Self::TwoHand,
            Self::Accessory1,
            Self::Accessory2,
            Self::Back,
            Self::Belt,
        ]
    }

    /// Check if this is a weapon slot
    pub fn is_weapon(&self) -> bool {
        matches!(self, Self::MainHand | Self::OffHand | Self::TwoHand)
    }

    /// Check if this is an armor slot
    pub fn is_armor(&self) -> bool {
        matches!(
            self,
            Self::Head | Self::Chest | Self::Legs | Self::Feet | Self::Hands
        )
    }

    /// Check if this is an accessory slot
    pub fn is_accessory(&self) -> bool {
        matches!(self, Self::Accessory1 | Self::Accessory2)
    }
}

/// Equipment component
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EquipmentComponent {
    /// Equipped items per slot
    slots: HashMap<EquipmentSlot, ItemStack>,
    /// Available slots for this entity
    available_slots: Vec<EquipmentSlot>,
    /// Cached stat bonuses
    #[serde(skip)]
    stat_cache: HashMap<String, f32>,
    /// Whether cache is dirty
    #[serde(skip)]
    cache_dirty: bool,
}

impl EquipmentComponent {
    /// Create new equipment component with standard slots
    pub fn new() -> Self {
        Self {
            slots: HashMap::new(),
            available_slots: EquipmentSlot::all_standard(),
            stat_cache: HashMap::new(),
            cache_dirty: true,
        }
    }

    /// Create with custom slots
    pub fn with_slots(slots: Vec<EquipmentSlot>) -> Self {
        Self {
            slots: HashMap::new(),
            available_slots: slots,
            stat_cache: HashMap::new(),
            cache_dirty: true,
        }
    }

    /// Check if slot is available
    pub fn has_slot(&self, slot: EquipmentSlot) -> bool {
        self.available_slots.contains(&slot)
    }

    /// Get equipped item in slot
    pub fn get_equipped(&self, slot: EquipmentSlot) -> Option<&ItemStack> {
        self.slots.get(&slot)
    }

    /// Check if slot is occupied
    pub fn is_slot_occupied(&self, slot: EquipmentSlot) -> bool {
        self.slots.contains_key(&slot)
    }

    /// Equip an item
    /// Returns previously equipped item if any
    pub fn equip(&mut self, slot: EquipmentSlot, item: ItemStack) -> Result<Option<ItemStack>, EquipError> {
        if !self.has_slot(slot) {
            return Err(EquipError::SlotNotAvailable);
        }

        // Handle two-handed weapons
        if slot == EquipmentSlot::TwoHand {
            // Unequip both hands first
            let main = self.slots.remove(&EquipmentSlot::MainHand);
            let off = self.slots.remove(&EquipmentSlot::OffHand);

            self.slots.insert(slot, item);
            self.cache_dirty = true;

            // Return main hand item (off hand would need separate handling)
            return Ok(main.or(off));
        }

        // Can't equip main/off hand if two-hand is equipped
        if (slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand)
            && self.is_slot_occupied(EquipmentSlot::TwoHand)
        {
            let two_hand = self.slots.remove(&EquipmentSlot::TwoHand);
            self.slots.insert(slot, item);
            self.cache_dirty = true;
            return Ok(two_hand);
        }

        let previous = self.slots.insert(slot, item);
        self.cache_dirty = true;
        Ok(previous)
    }

    /// Unequip item from slot
    pub fn unequip(&mut self, slot: EquipmentSlot) -> Option<ItemStack> {
        let item = self.slots.remove(&slot);
        if item.is_some() {
            self.cache_dirty = true;
        }
        item
    }

    /// Get all equipped items
    pub fn all_equipped(&self) -> impl Iterator<Item = (&EquipmentSlot, &ItemStack)> {
        self.slots.iter()
    }

    /// Get count of equipped items
    pub fn equipped_count(&self) -> usize {
        self.slots.len()
    }

    /// Clear all equipment
    pub fn clear(&mut self) -> Vec<ItemStack> {
        self.cache_dirty = true;
        self.slots.drain().map(|(_, v)| v).collect()
    }

    /// Get stat bonus from all equipment
    pub fn get_stat(&self, stat: &str) -> f32 {
        // In a real implementation, this would calculate from equipped item properties
        *self.stat_cache.get(stat).unwrap_or(&0.0)
    }

    /// Recalculate stat cache
    pub fn recalculate_stats<F>(&mut self, get_item_stats: F)
    where
        F: Fn(&str) -> HashMap<String, f32>,
    {
        self.stat_cache.clear();

        for item in self.slots.values() {
            let item_stats = get_item_stats(&item.item_id);
            for (stat, value) in item_stats {
                *self.stat_cache.entry(stat).or_insert(0.0) += value;
            }
        }

        self.cache_dirty = false;
    }

    /// Check if cache needs recalculation
    pub fn is_cache_dirty(&self) -> bool {
        self.cache_dirty
    }
}

impl Default for EquipmentComponent {
    fn default() -> Self {
        Self::new()
    }
}

/// Equipment errors
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EquipError {
    /// Slot is not available for this entity
    SlotNotAvailable,
    /// Item cannot be equipped in this slot
    InvalidSlotForItem,
    /// Requirements not met (level, stats, etc.)
    RequirementsNotMet,
}

impl std::fmt::Display for EquipError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::SlotNotAvailable => write!(f, "Equipment slot not available"),
            Self::InvalidSlotForItem => write!(f, "Item cannot be equipped in this slot"),
            Self::RequirementsNotMet => write!(f, "Equipment requirements not met"),
        }
    }
}

impl std::error::Error for EquipError {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_equipment_creation() {
        let eq = EquipmentComponent::new();

        assert!(eq.has_slot(EquipmentSlot::Head));
        assert!(eq.has_slot(EquipmentSlot::MainHand));
        assert_eq!(eq.equipped_count(), 0);
    }

    #[test]
    fn test_equip_item() {
        let mut eq = EquipmentComponent::new();

        let helmet = ItemStack::single("iron_helmet");
        let result = eq.equip(EquipmentSlot::Head, helmet);

        assert!(result.is_ok());
        assert!(result.unwrap().is_none()); // No previous item
        assert!(eq.is_slot_occupied(EquipmentSlot::Head));
    }

    #[test]
    fn test_equip_replace() {
        let mut eq = EquipmentComponent::new();

        let sword1 = ItemStack::single("iron_sword");
        let sword2 = ItemStack::single("steel_sword");

        eq.equip(EquipmentSlot::MainHand, sword1).unwrap();
        let previous = eq.equip(EquipmentSlot::MainHand, sword2).unwrap();

        assert!(previous.is_some());
        assert_eq!(previous.unwrap().item_id, "iron_sword");
    }

    #[test]
    fn test_unequip() {
        let mut eq = EquipmentComponent::new();

        let sword = ItemStack::single("sword");
        eq.equip(EquipmentSlot::MainHand, sword).unwrap();

        let item = eq.unequip(EquipmentSlot::MainHand);
        assert!(item.is_some());
        assert!(!eq.is_slot_occupied(EquipmentSlot::MainHand));
    }

    #[test]
    fn test_two_handed_weapon() {
        let mut eq = EquipmentComponent::new();

        // Equip one-handed weapons
        eq.equip(EquipmentSlot::MainHand, ItemStack::single("sword"))
            .unwrap();
        eq.equip(EquipmentSlot::OffHand, ItemStack::single("shield"))
            .unwrap();

        // Equip two-handed weapon - should remove both
        let previous = eq
            .equip(EquipmentSlot::TwoHand, ItemStack::single("greatsword"))
            .unwrap();

        assert!(previous.is_some()); // Got one of the previous items back
        assert!(!eq.is_slot_occupied(EquipmentSlot::MainHand));
        assert!(!eq.is_slot_occupied(EquipmentSlot::OffHand));
        assert!(eq.is_slot_occupied(EquipmentSlot::TwoHand));
    }

    #[test]
    fn test_custom_slots() {
        let eq = EquipmentComponent::with_slots(vec![
            EquipmentSlot::MainHand,
            EquipmentSlot::OffHand,
        ]);

        assert!(eq.has_slot(EquipmentSlot::MainHand));
        assert!(!eq.has_slot(EquipmentSlot::Head)); // Not available
    }
}
