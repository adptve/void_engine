//! Pickup system for world items

use crate::item::ItemStack;
use serde::{Deserialize, Serialize};

/// Pickup behavior mode
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum PickupMode {
    /// Pick up automatically on touch
    Automatic,
    /// Require interaction to pick up
    Interactive,
    /// Require key press while in range
    KeyPress,
    /// Can only be picked up by specific entities
    Restricted,
}

impl Default for PickupMode {
    fn default() -> Self {
        Self::Automatic
    }
}

/// Pickup component for world items
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PickupComponent {
    /// Item stack to pick up
    pub item: ItemStack,
    /// Pickup behavior
    pub mode: PickupMode,
    /// Pickup radius (for automatic pickup)
    pub radius: f32,
    /// Whether the pickup is enabled
    pub enabled: bool,
    /// Cooldown before can be picked up (for respawning pickups)
    pub cooldown: f32,
    /// Current cooldown timer
    #[serde(skip)]
    pub cooldown_timer: f32,
    /// Whether this pickup respawns
    pub respawns: bool,
    /// Respawn time in seconds
    pub respawn_time: f32,
    /// Whether currently waiting to respawn
    #[serde(skip)]
    pub is_respawning: bool,
    /// Allowed picker entity IDs (for Restricted mode)
    pub allowed_pickers: Vec<u64>,
    /// Sound to play on pickup
    pub pickup_sound: String,
    /// Effect to spawn on pickup
    pub pickup_effect: String,
    /// Whether to bob/rotate in world
    pub animate: bool,
    /// Animation time (for bobbing)
    #[serde(skip)]
    pub animation_time: f32,
}

impl PickupComponent {
    /// Create a new pickup
    pub fn new(item: ItemStack) -> Self {
        Self {
            item,
            mode: PickupMode::default(),
            radius: 1.0,
            enabled: true,
            cooldown: 0.0,
            cooldown_timer: 0.0,
            respawns: false,
            respawn_time: 30.0,
            is_respawning: false,
            allowed_pickers: Vec::new(),
            pickup_sound: String::new(),
            pickup_effect: String::new(),
            animate: true,
            animation_time: 0.0,
        }
    }

    /// Set pickup mode
    pub fn with_mode(mut self, mode: PickupMode) -> Self {
        self.mode = mode;
        self
    }

    /// Set pickup radius
    pub fn with_radius(mut self, radius: f32) -> Self {
        self.radius = radius;
        self
    }

    /// Set initial cooldown
    pub fn with_cooldown(mut self, cooldown: f32) -> Self {
        self.cooldown = cooldown;
        self.cooldown_timer = cooldown;
        self
    }

    /// Enable respawning
    pub fn with_respawn(mut self, time: f32) -> Self {
        self.respawns = true;
        self.respawn_time = time;
        self
    }

    /// Set allowed pickers (for Restricted mode)
    pub fn with_allowed_pickers(mut self, pickers: Vec<u64>) -> Self {
        self.allowed_pickers = pickers;
        self.mode = PickupMode::Restricted;
        self
    }

    /// Set pickup sound
    pub fn with_sound(mut self, sound: impl Into<String>) -> Self {
        self.pickup_sound = sound.into();
        self
    }

    /// Set pickup effect
    pub fn with_effect(mut self, effect: impl Into<String>) -> Self {
        self.pickup_effect = effect.into();
        self
    }

    /// Disable animation
    pub fn without_animation(mut self) -> Self {
        self.animate = false;
        self
    }

    /// Check if can be picked up
    pub fn can_pickup(&self, picker_id: u64) -> bool {
        if !self.enabled || self.is_respawning || self.cooldown_timer > 0.0 {
            return false;
        }

        match self.mode {
            PickupMode::Restricted => self.allowed_pickers.contains(&picker_id),
            _ => true,
        }
    }

    /// Check if entity is in pickup range
    pub fn in_range(&self, pickup_pos: [f32; 3], picker_pos: [f32; 3]) -> bool {
        let dx = picker_pos[0] - pickup_pos[0];
        let dy = picker_pos[1] - pickup_pos[1];
        let dz = picker_pos[2] - pickup_pos[2];
        let dist_sq = dx * dx + dy * dy + dz * dz;
        dist_sq <= self.radius * self.radius
    }

    /// Attempt to pick up
    /// Returns the item if successful
    pub fn pickup(&mut self, picker_id: u64) -> Option<ItemStack> {
        if !self.can_pickup(picker_id) {
            return None;
        }

        if self.respawns {
            self.is_respawning = true;
            self.cooldown_timer = self.respawn_time;
            Some(self.item.clone())
        } else {
            self.enabled = false;
            Some(self.item.clone())
        }
    }

    /// Update pickup state
    pub fn update(&mut self, delta_time: f32) {
        // Update cooldown
        if self.cooldown_timer > 0.0 {
            self.cooldown_timer -= delta_time;
            if self.cooldown_timer <= 0.0 {
                self.cooldown_timer = 0.0;
                if self.is_respawning {
                    self.is_respawning = false;
                }
            }
        }

        // Update animation
        if self.animate && !self.is_respawning {
            self.animation_time += delta_time;
        }
    }

    /// Get animation offset (for bobbing)
    pub fn get_bob_offset(&self) -> f32 {
        if !self.animate || self.is_respawning {
            return 0.0;
        }
        (self.animation_time * 2.0).sin() * 0.1
    }

    /// Get animation rotation (for spinning)
    pub fn get_spin_rotation(&self) -> f32 {
        if !self.animate || self.is_respawning {
            return 0.0;
        }
        self.animation_time * 1.5 // Radians
    }

    /// Check if visible (not respawning)
    pub fn is_visible(&self) -> bool {
        self.enabled && !self.is_respawning
    }
}

impl Default for PickupComponent {
    fn default() -> Self {
        Self::new(ItemStack::single("unknown"))
    }
}

/// Pickup event
#[derive(Debug, Clone)]
pub struct PickupEvent {
    /// Pickup entity ID
    pub pickup_entity: u64,
    /// Picker entity ID
    pub picker_entity: u64,
    /// Item that was picked up
    pub item: ItemStack,
    /// World position of pickup
    pub position: [f32; 3],
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pickup_creation() {
        let pickup = PickupComponent::new(ItemStack::new("gold_coin", 10))
            .with_mode(PickupMode::Automatic)
            .with_radius(2.0);

        assert_eq!(pickup.item.quantity, 10);
        assert_eq!(pickup.radius, 2.0);
        assert!(pickup.enabled);
    }

    #[test]
    fn test_pickup_range() {
        let pickup = PickupComponent::new(ItemStack::single("item")).with_radius(2.0);

        let pickup_pos = [0.0, 0.0, 0.0];
        assert!(pickup.in_range(pickup_pos, [1.0, 0.0, 0.0])); // In range
        assert!(!pickup.in_range(pickup_pos, [3.0, 0.0, 0.0])); // Out of range
    }

    #[test]
    fn test_pickup_item() {
        let mut pickup = PickupComponent::new(ItemStack::new("gold", 50));

        let item = pickup.pickup(1);
        assert!(item.is_some());
        assert_eq!(item.unwrap().quantity, 50);
        assert!(!pickup.enabled); // Disabled after pickup
    }

    #[test]
    fn test_respawning_pickup() {
        let mut pickup = PickupComponent::new(ItemStack::single("health"))
            .with_respawn(5.0);

        let item = pickup.pickup(1);
        assert!(item.is_some());
        assert!(pickup.is_respawning);
        assert!(!pickup.can_pickup(1)); // Can't pick up while respawning

        // Simulate respawn
        pickup.update(5.1);
        assert!(!pickup.is_respawning);
        assert!(pickup.can_pickup(1)); // Can pick up again
    }

    #[test]
    fn test_restricted_pickup() {
        let mut pickup = PickupComponent::new(ItemStack::single("quest_item"))
            .with_allowed_pickers(vec![100, 200]);

        assert!(pickup.can_pickup(100));
        assert!(pickup.can_pickup(200));
        assert!(!pickup.can_pickup(300)); // Not allowed
    }

    #[test]
    fn test_cooldown() {
        let mut pickup = PickupComponent::new(ItemStack::single("item")).with_cooldown(2.0);

        assert!(!pickup.can_pickup(1)); // On cooldown

        pickup.update(2.1);
        assert!(pickup.can_pickup(1)); // Cooldown expired
    }

    #[test]
    fn test_animation() {
        let mut pickup = PickupComponent::new(ItemStack::single("item"));

        pickup.update(0.5);
        let offset = pickup.get_bob_offset();
        assert!(offset != 0.0); // Should be animating
    }
}
