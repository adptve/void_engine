//! Collision layers and filtering

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// A collision layer identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct CollisionLayer(pub u32);

impl CollisionLayer {
    /// Default layer (collides with everything)
    pub const DEFAULT: Self = Self(0);
    /// Player layer
    pub const PLAYER: Self = Self(1);
    /// Enemy layer
    pub const ENEMIES: Self = Self(2);
    /// Projectile layer
    pub const PROJECTILES: Self = Self(3);
    /// Trigger/sensor layer
    pub const TRIGGERS: Self = Self(4);
    /// Static environment layer
    pub const ENVIRONMENT: Self = Self(5);
    /// Pickup/item layer
    pub const PICKUPS: Self = Self(6);

    /// Create a custom layer
    pub const fn custom(id: u32) -> Self {
        Self(id)
    }

    /// Get the layer as a bitmask
    pub fn as_mask(&self) -> u32 {
        1 << self.0
    }
}

impl Default for CollisionLayer {
    fn default() -> Self {
        Self::DEFAULT
    }
}

/// Collision groups for filtering
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct CollisionGroups {
    /// Which groups this object belongs to (membership)
    pub memberships: u32,
    /// Which groups this object can collide with (filter)
    pub filter: u32,
}

impl CollisionGroups {
    /// Create collision groups that collide with everything
    pub const ALL: Self = Self {
        memberships: u32::MAX,
        filter: u32::MAX,
    };

    /// Create collision groups that collide with nothing
    pub const NONE: Self = Self {
        memberships: 0,
        filter: 0,
    };

    /// Create new collision groups
    pub fn new(memberships: u32, filter: u32) -> Self {
        Self { memberships, filter }
    }

    /// Create from a single layer that collides with specific layers
    pub fn from_layer(layer: CollisionLayer, collides_with: &[CollisionLayer]) -> Self {
        let memberships = layer.as_mask();
        let filter = collides_with.iter().fold(0u32, |acc, l| acc | l.as_mask());
        Self { memberships, filter }
    }

    /// Check if two groups can collide
    pub fn can_collide(&self, other: &CollisionGroups) -> bool {
        (self.memberships & other.filter) != 0 && (other.memberships & self.filter) != 0
    }

    /// Add a layer to membership
    pub fn add_membership(mut self, layer: CollisionLayer) -> Self {
        self.memberships |= layer.as_mask();
        self
    }

    /// Add a layer to filter
    pub fn add_filter(mut self, layer: CollisionLayer) -> Self {
        self.filter |= layer.as_mask();
        self
    }

    /// Remove a layer from membership
    pub fn remove_membership(mut self, layer: CollisionLayer) -> Self {
        self.memberships &= !layer.as_mask();
        self
    }

    /// Remove a layer from filter
    pub fn remove_filter(mut self, layer: CollisionLayer) -> Self {
        self.filter &= !layer.as_mask();
        self
    }
}

impl Default for CollisionGroups {
    fn default() -> Self {
        Self::ALL
    }
}

/// Collision matrix defining which layers collide
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CollisionMatrix {
    /// Named layers
    layer_names: HashMap<String, CollisionLayer>,
    /// Collision rules: layer -> list of layers it collides with
    rules: HashMap<CollisionLayer, Vec<CollisionLayer>>,
}

impl Default for CollisionMatrix {
    fn default() -> Self {
        let mut matrix = Self {
            layer_names: HashMap::new(),
            rules: HashMap::new(),
        };

        // Register default layers
        matrix.register_layer("default", CollisionLayer::DEFAULT);
        matrix.register_layer("player", CollisionLayer::PLAYER);
        matrix.register_layer("enemies", CollisionLayer::ENEMIES);
        matrix.register_layer("projectiles", CollisionLayer::PROJECTILES);
        matrix.register_layer("triggers", CollisionLayer::TRIGGERS);
        matrix.register_layer("environment", CollisionLayer::ENVIRONMENT);
        matrix.register_layer("pickups", CollisionLayer::PICKUPS);

        // Default collision rules
        matrix.set_collides_with(CollisionLayer::PLAYER, &[
            CollisionLayer::DEFAULT,
            CollisionLayer::ENEMIES,
            CollisionLayer::PROJECTILES,
            CollisionLayer::TRIGGERS,
            CollisionLayer::ENVIRONMENT,
            CollisionLayer::PICKUPS,
        ]);

        matrix.set_collides_with(CollisionLayer::ENEMIES, &[
            CollisionLayer::DEFAULT,
            CollisionLayer::PLAYER,
            CollisionLayer::PROJECTILES,
            CollisionLayer::TRIGGERS,
            CollisionLayer::ENVIRONMENT,
        ]);

        matrix.set_collides_with(CollisionLayer::PROJECTILES, &[
            CollisionLayer::DEFAULT,
            CollisionLayer::PLAYER,
            CollisionLayer::ENEMIES,
            CollisionLayer::ENVIRONMENT,
        ]);

        matrix.set_collides_with(CollisionLayer::TRIGGERS, &[
            CollisionLayer::PLAYER,
            CollisionLayer::ENEMIES,
        ]);

        matrix
    }
}

impl CollisionMatrix {
    /// Create an empty collision matrix
    pub fn new() -> Self {
        Self {
            layer_names: HashMap::new(),
            rules: HashMap::new(),
        }
    }

    /// Register a named layer
    pub fn register_layer(&mut self, name: &str, layer: CollisionLayer) {
        self.layer_names.insert(name.to_string(), layer);
    }

    /// Get a layer by name
    pub fn get_layer(&self, name: &str) -> Option<CollisionLayer> {
        self.layer_names.get(name).copied()
    }

    /// Set which layers a given layer collides with
    pub fn set_collides_with(&mut self, layer: CollisionLayer, collides_with: &[CollisionLayer]) {
        self.rules.insert(layer, collides_with.to_vec());
    }

    /// Get collision groups for a layer
    pub fn get_groups(&self, layer: CollisionLayer) -> CollisionGroups {
        let filter = self
            .rules
            .get(&layer)
            .map(|layers| layers.iter().fold(0u32, |acc, l| acc | l.as_mask()))
            .unwrap_or(u32::MAX);

        CollisionGroups {
            memberships: layer.as_mask(),
            filter,
        }
    }

    /// Get collision groups by layer name
    pub fn get_groups_by_name(&self, name: &str) -> Option<CollisionGroups> {
        self.get_layer(name).map(|layer| self.get_groups(layer))
    }

    /// Check if two layers can collide
    pub fn can_collide(&self, a: CollisionLayer, b: CollisionLayer) -> bool {
        let a_collides = self.rules.get(&a).map(|v| v.contains(&b)).unwrap_or(true);
        let b_collides = self.rules.get(&b).map(|v| v.contains(&a)).unwrap_or(true);
        a_collides && b_collides
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_collision_groups() {
        let player = CollisionGroups::from_layer(
            CollisionLayer::PLAYER,
            &[CollisionLayer::ENEMIES, CollisionLayer::ENVIRONMENT],
        );

        let enemy = CollisionGroups::from_layer(
            CollisionLayer::ENEMIES,
            &[CollisionLayer::PLAYER, CollisionLayer::PROJECTILES],
        );

        assert!(player.can_collide(&enemy));
    }

    #[test]
    fn test_collision_matrix() {
        let matrix = CollisionMatrix::default();

        assert!(matrix.can_collide(CollisionLayer::PLAYER, CollisionLayer::ENEMIES));
        assert!(matrix.can_collide(CollisionLayer::PLAYER, CollisionLayer::TRIGGERS));
        assert!(!matrix.can_collide(CollisionLayer::PICKUPS, CollisionLayer::PROJECTILES));
    }
}
