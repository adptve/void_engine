//! Inventory component

use crate::item::ItemStack;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Inventory events
#[derive(Debug, Clone)]
pub enum InventoryEvent {
    /// Item added to inventory
    ItemAdded {
        slot: usize,
        item_id: String,
        quantity: u32,
    },
    /// Item removed from inventory
    ItemRemoved {
        slot: usize,
        item_id: String,
        quantity: u32,
    },
    /// Item moved between slots
    ItemMoved {
        from_slot: usize,
        to_slot: usize,
    },
    /// Items swapped between slots
    ItemsSwapped {
        slot_a: usize,
        slot_b: usize,
    },
    /// Stack split
    StackSplit {
        source_slot: usize,
        target_slot: usize,
        amount: u32,
    },
    /// Inventory full (couldn't add item)
    Full {
        item_id: String,
        overflow: u32,
    },
}

/// Inventory component
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Inventory {
    /// Inventory slots (None = empty)
    slots: Vec<Option<ItemStack>>,
    /// Number of slots
    capacity: usize,
    /// Maximum weight (0 = unlimited)
    max_weight: f32,
    /// Current weight
    current_weight: f32,
    /// Item definitions for stack size lookup
    #[serde(skip)]
    item_defs: HashMap<String, u32>, // item_id -> max_stack
}

impl Inventory {
    /// Create a new inventory with given capacity
    pub fn new(capacity: usize) -> Self {
        Self {
            slots: vec![None; capacity],
            capacity,
            max_weight: 0.0,
            current_weight: 0.0,
            item_defs: HashMap::new(),
        }
    }

    /// Set maximum weight
    pub fn with_max_weight(mut self, weight: f32) -> Self {
        self.max_weight = weight;
        self
    }

    /// Register item definition (for max stack lookup)
    pub fn register_item(&mut self, item_id: impl Into<String>, max_stack: u32) {
        self.item_defs.insert(item_id.into(), max_stack);
    }

    /// Get max stack for an item (defaults to 1)
    fn max_stack(&self, item_id: &str) -> u32 {
        *self.item_defs.get(item_id).unwrap_or(&99)
    }

    /// Get inventory capacity
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// Get number of used slots
    pub fn used_slots(&self) -> usize {
        self.slots.iter().filter(|s| s.is_some()).count()
    }

    /// Get number of free slots
    pub fn free_slots(&self) -> usize {
        self.capacity - self.used_slots()
    }

    /// Check if inventory is full
    pub fn is_full(&self) -> bool {
        self.free_slots() == 0
    }

    /// Check if inventory is empty
    pub fn is_empty(&self) -> bool {
        self.used_slots() == 0
    }

    /// Get slot contents
    pub fn get_slot(&self, slot: usize) -> Option<&ItemStack> {
        self.slots.get(slot)?.as_ref()
    }

    /// Get mutable slot contents
    pub fn get_slot_mut(&mut self, slot: usize) -> Option<&mut ItemStack> {
        self.slots.get_mut(slot)?.as_mut()
    }

    /// Find first empty slot
    pub fn find_empty_slot(&self) -> Option<usize> {
        self.slots.iter().position(|s| s.is_none())
    }

    /// Find slot containing specific item
    pub fn find_item(&self, item_id: &str) -> Option<usize> {
        self.slots
            .iter()
            .position(|s| s.as_ref().map(|i| i.item_id == item_id).unwrap_or(false))
    }

    /// Find all slots containing specific item
    pub fn find_all_items(&self, item_id: &str) -> Vec<usize> {
        self.slots
            .iter()
            .enumerate()
            .filter_map(|(i, s)| {
                if s.as_ref().map(|item| item.item_id == item_id).unwrap_or(false) {
                    Some(i)
                } else {
                    None
                }
            })
            .collect()
    }

    /// Count total quantity of an item
    pub fn count_item(&self, item_id: &str) -> u32 {
        self.slots
            .iter()
            .filter_map(|s| s.as_ref())
            .filter(|i| i.item_id == item_id)
            .map(|i| i.quantity)
            .sum()
    }

    /// Add item to inventory
    /// Returns overflow (amount that couldn't fit)
    pub fn add_item(&mut self, mut stack: ItemStack) -> u32 {
        let max_stack = self.max_stack(&stack.item_id);

        // Try to stack with existing items first
        for slot in &mut self.slots {
            if let Some(existing) = slot {
                if existing.item_id == stack.item_id && existing.quantity < max_stack {
                    if existing.can_merge(&stack) {
                        stack.quantity = existing.add(stack.quantity, max_stack);
                        if stack.quantity == 0 {
                            return 0;
                        }
                    }
                }
            }
        }

        // Add to empty slots
        while stack.quantity > 0 {
            if let Some(empty_slot) = self.find_empty_slot() {
                let amount = stack.quantity.min(max_stack);
                self.slots[empty_slot] = Some(ItemStack {
                    item_id: stack.item_id.clone(),
                    quantity: amount,
                    instance_data: stack.instance_data.clone(),
                });
                stack.quantity -= amount;
            } else {
                break;
            }
        }

        stack.quantity // Return overflow
    }

    /// Add item to specific slot
    pub fn add_to_slot(&mut self, slot: usize, mut stack: ItemStack) -> u32 {
        if slot >= self.capacity {
            return stack.quantity;
        }

        let max_stack = self.max_stack(&stack.item_id);

        match &mut self.slots[slot] {
            Some(existing) if existing.item_id == stack.item_id && existing.can_merge(&stack) => {
                existing.add(stack.quantity, max_stack)
            }
            None => {
                let amount = stack.quantity.min(max_stack);
                stack.quantity -= amount;
                self.slots[slot] = Some(ItemStack {
                    item_id: stack.item_id,
                    quantity: amount,
                    instance_data: stack.instance_data,
                });
                0
            }
            _ => stack.quantity,
        }
    }

    /// Remove item from inventory
    /// Returns amount actually removed
    pub fn remove_item(&mut self, item_id: &str, amount: u32) -> u32 {
        let mut remaining = amount;

        for slot in &mut self.slots {
            if remaining == 0 {
                break;
            }
            if let Some(stack) = slot {
                if stack.item_id == item_id {
                    let removed = stack.remove(remaining);
                    remaining -= removed;
                    if stack.is_empty() {
                        *slot = None;
                    }
                }
            }
        }

        amount - remaining
    }

    /// Remove item from specific slot
    pub fn remove_from_slot(&mut self, slot: usize, amount: u32) -> Option<ItemStack> {
        if slot >= self.capacity {
            return None;
        }

        if let Some(stack) = &mut self.slots[slot] {
            if amount >= stack.quantity {
                self.slots[slot].take()
            } else {
                stack.split(amount)
            }
        } else {
            None
        }
    }

    /// Clear a slot
    pub fn clear_slot(&mut self, slot: usize) -> Option<ItemStack> {
        if slot < self.capacity {
            self.slots[slot].take()
        } else {
            None
        }
    }

    /// Move item from one slot to another
    pub fn move_item(&mut self, from: usize, to: usize) -> bool {
        if from >= self.capacity || to >= self.capacity || from == to {
            return false;
        }

        if self.slots[from].is_none() {
            return false;
        }

        if self.slots[to].is_none() {
            self.slots[to] = self.slots[from].take();
            true
        } else {
            // Try to merge
            let from_stack = self.slots[from].as_ref().unwrap();
            let to_stack = self.slots[to].as_ref().unwrap();

            if from_stack.item_id == to_stack.item_id && from_stack.can_merge(to_stack) {
                let max_stack = self.max_stack(&from_stack.item_id);
                let from_qty = from_stack.quantity;

                if let Some(to_s) = &mut self.slots[to] {
                    let overflow = to_s.add(from_qty, max_stack);
                    if overflow == 0 {
                        self.slots[from] = None;
                    } else if let Some(from_s) = &mut self.slots[from] {
                        from_s.quantity = overflow;
                    }
                }
                true
            } else {
                false
            }
        }
    }

    /// Swap two slots
    pub fn swap_slots(&mut self, slot_a: usize, slot_b: usize) -> bool {
        if slot_a >= self.capacity || slot_b >= self.capacity || slot_a == slot_b {
            return false;
        }

        self.slots.swap(slot_a, slot_b);
        true
    }

    /// Split a stack
    pub fn split_stack(&mut self, from_slot: usize, amount: u32, to_slot: usize) -> bool {
        if from_slot >= self.capacity || to_slot >= self.capacity {
            return false;
        }

        if self.slots[to_slot].is_some() {
            return false;
        }

        if let Some(from_stack) = &mut self.slots[from_slot] {
            if let Some(split) = from_stack.split(amount) {
                self.slots[to_slot] = Some(split);
                return true;
            }
        }

        false
    }

    /// Check if can add item
    pub fn can_add(&self, item_id: &str, amount: u32) -> bool {
        let max_stack = self.max_stack(item_id);
        let mut remaining = amount;

        // Check existing stacks
        for slot in &self.slots {
            if let Some(stack) = slot {
                if stack.item_id == item_id && stack.quantity < max_stack {
                    remaining = remaining.saturating_sub(max_stack - stack.quantity);
                }
            }
        }

        if remaining == 0 {
            return true;
        }

        // Check empty slots
        let free = self.free_slots();
        let stacks_needed = (remaining + max_stack - 1) / max_stack;

        free >= stacks_needed as usize
    }

    /// Get all items as iterator
    pub fn items(&self) -> impl Iterator<Item = (usize, &ItemStack)> {
        self.slots
            .iter()
            .enumerate()
            .filter_map(|(i, s)| s.as_ref().map(|stack| (i, stack)))
    }

    /// Sort inventory by item ID
    pub fn sort(&mut self) {
        // Collect all items
        let mut items: Vec<ItemStack> = self.slots.iter_mut().filter_map(|s| s.take()).collect();

        // Sort by item ID
        items.sort_by(|a, b| a.item_id.cmp(&b.item_id));

        // Put back, merging stacks
        for item in items {
            self.add_item(item);
        }
    }

    /// Compact inventory (merge stacks, remove gaps)
    pub fn compact(&mut self) {
        self.sort();
    }
}

impl Default for Inventory {
    fn default() -> Self {
        Self::new(20)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_inventory_creation() {
        let inv = Inventory::new(10);

        assert_eq!(inv.capacity(), 10);
        assert_eq!(inv.used_slots(), 0);
        assert!(inv.is_empty());
    }

    #[test]
    fn test_add_item() {
        let mut inv = Inventory::new(5);

        let overflow = inv.add_item(ItemStack::new("sword", 1));
        assert_eq!(overflow, 0);
        assert_eq!(inv.used_slots(), 1);

        let stack = inv.get_slot(0).unwrap();
        assert_eq!(stack.item_id, "sword");
    }

    #[test]
    fn test_stacking() {
        let mut inv = Inventory::new(5);
        inv.register_item("gold", 100);

        inv.add_item(ItemStack::new("gold", 50));
        inv.add_item(ItemStack::new("gold", 30));

        // Should stack in one slot
        assert_eq!(inv.used_slots(), 1);
        assert_eq!(inv.count_item("gold"), 80);
    }

    #[test]
    fn test_overflow() {
        let mut inv = Inventory::new(2);
        inv.register_item("gold", 50);

        let overflow = inv.add_item(ItemStack::new("gold", 150));

        assert_eq!(overflow, 50); // 150 - (50 * 2)
        assert_eq!(inv.count_item("gold"), 100);
    }

    #[test]
    fn test_remove_item() {
        let mut inv = Inventory::new(5);

        inv.add_item(ItemStack::new("arrows", 50));
        let removed = inv.remove_item("arrows", 20);

        assert_eq!(removed, 20);
        assert_eq!(inv.count_item("arrows"), 30);
    }

    #[test]
    fn test_swap_slots() {
        let mut inv = Inventory::new(5);

        inv.add_item(ItemStack::new("sword", 1));
        inv.add_item(ItemStack::new("shield", 1));

        assert!(inv.swap_slots(0, 1));

        assert_eq!(inv.get_slot(0).unwrap().item_id, "shield");
        assert_eq!(inv.get_slot(1).unwrap().item_id, "sword");
    }

    #[test]
    fn test_split_stack() {
        let mut inv = Inventory::new(5);
        inv.register_item("gold", 200); // Set max stack to 200

        inv.add_item(ItemStack::new("gold", 100));
        assert!(inv.split_stack(0, 30, 1));

        assert_eq!(inv.get_slot(0).unwrap().quantity, 70);
        assert_eq!(inv.get_slot(1).unwrap().quantity, 30);
    }
}
