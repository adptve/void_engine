//! Trigger filtering system

use serde::{Deserialize, Serialize};
use std::collections::HashSet;

/// Filter for what entities can activate a trigger
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TriggerFilter {
    /// Required layers (entity must have at least one)
    pub required_layers: u32,
    /// Excluded layers (entity must not have any)
    pub excluded_layers: u32,
    /// Required tags (entity must have all of these)
    pub required_tags: HashSet<String>,
    /// Excluded tags (entity must not have any)
    pub excluded_tags: HashSet<String>,
    /// Specific entity IDs that can trigger (empty = any)
    pub allowed_entities: HashSet<u64>,
    /// Specific entity IDs that cannot trigger
    pub blocked_entities: HashSet<u64>,
    /// Whether the trigger owner can activate itself
    pub allow_self: bool,
}

impl TriggerFilter {
    /// Create a new filter that accepts everything
    pub fn new() -> Self {
        Self {
            required_layers: 0xFFFFFFFF, // All layers by default
            excluded_layers: 0,
            required_tags: HashSet::new(),
            excluded_tags: HashSet::new(),
            allowed_entities: HashSet::new(),
            blocked_entities: HashSet::new(),
            allow_self: false,
        }
    }

    /// Set required layers (bitmask)
    pub fn with_layers(mut self, layers: u32) -> Self {
        self.required_layers = layers;
        self
    }

    /// Add a single required layer
    pub fn with_layer(mut self, layer: u8) -> Self {
        // Keep existing layers, just ensure this one is allowed
        self.required_layers |= 1 << layer;
        self
    }

    /// Set excluded layers (bitmask)
    pub fn without_layers(mut self, layers: u32) -> Self {
        self.excluded_layers = layers;
        self
    }

    /// Add a single excluded layer
    pub fn without_layer(mut self, layer: u8) -> Self {
        self.excluded_layers |= 1 << layer;
        self
    }

    /// Require a tag
    pub fn with_tag(mut self, tag: impl Into<String>) -> Self {
        self.required_tags.insert(tag.into());
        self
    }

    /// Require multiple tags
    pub fn with_tags<I, S>(mut self, tags: I) -> Self
    where
        I: IntoIterator<Item = S>,
        S: Into<String>,
    {
        for tag in tags {
            self.required_tags.insert(tag.into());
        }
        self
    }

    /// Exclude a tag
    pub fn without_tag(mut self, tag: impl Into<String>) -> Self {
        self.excluded_tags.insert(tag.into());
        self
    }

    /// Allow only specific entities
    pub fn only_entities<I: IntoIterator<Item = u64>>(mut self, entities: I) -> Self {
        self.allowed_entities = entities.into_iter().collect();
        self
    }

    /// Block specific entities
    pub fn block_entities<I: IntoIterator<Item = u64>>(mut self, entities: I) -> Self {
        for entity in entities {
            self.blocked_entities.insert(entity);
        }
        self
    }

    /// Allow self-triggering
    pub fn allow_self_trigger(mut self) -> Self {
        self.allow_self = true;
        self
    }

    /// Check if an entity passes this filter
    pub fn passes(
        &self,
        entity_id: u64,
        entity_layers: u32,
        entity_tags: &HashSet<String>,
        trigger_owner: u64,
    ) -> bool {
        // Check self-triggering
        if entity_id == trigger_owner && !self.allow_self {
            return false;
        }

        // Check blocked entities
        if self.blocked_entities.contains(&entity_id) {
            return false;
        }

        // Check allowed entities (if specified)
        if !self.allowed_entities.is_empty() && !self.allowed_entities.contains(&entity_id) {
            return false;
        }

        // Check layers
        if entity_layers & self.required_layers == 0 {
            return false;
        }
        if entity_layers & self.excluded_layers != 0 {
            return false;
        }

        // Check required tags
        for tag in &self.required_tags {
            if !entity_tags.contains(tag) {
                return false;
            }
        }

        // Check excluded tags
        for tag in &self.excluded_tags {
            if entity_tags.contains(tag) {
                return false;
            }
        }

        true
    }
}

impl Default for TriggerFilter {
    fn default() -> Self {
        Self::new()
    }
}

/// Common filter presets
impl TriggerFilter {
    /// Filter for player entities only
    pub fn player_only() -> Self {
        Self::new().with_tag("player")
    }

    /// Filter for enemies only
    pub fn enemies_only() -> Self {
        Self::new().with_tag("enemy")
    }

    /// Filter for anything with physics
    pub fn physics_objects() -> Self {
        Self::new().with_layer(0) // Assume layer 0 is physics
    }

    /// Filter for interactive objects
    pub fn interactable() -> Self {
        Self::new().with_tag("interactable")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_filter_layers() {
        let filter = TriggerFilter::new().with_layers(0b0011); // Layers 0 and 1

        let empty_tags = HashSet::new();

        // Entity on layer 0
        assert!(filter.passes(1, 0b0001, &empty_tags, 0));
        // Entity on layer 1
        assert!(filter.passes(2, 0b0010, &empty_tags, 0));
        // Entity on layer 2 (not allowed)
        assert!(!filter.passes(3, 0b0100, &empty_tags, 0));
    }

    #[test]
    fn test_filter_tags() {
        let filter = TriggerFilter::new().with_tag("player");

        let mut player_tags = HashSet::new();
        player_tags.insert("player".to_string());

        let mut enemy_tags = HashSet::new();
        enemy_tags.insert("enemy".to_string());

        assert!(filter.passes(1, 0xFFFFFFFF, &player_tags, 0));
        assert!(!filter.passes(2, 0xFFFFFFFF, &enemy_tags, 0));
    }

    #[test]
    fn test_filter_self() {
        let filter = TriggerFilter::new();
        let empty_tags = HashSet::new();

        // Cannot self-trigger by default
        assert!(!filter.passes(5, 0xFFFFFFFF, &empty_tags, 5));

        let filter_self = TriggerFilter::new().allow_self_trigger();
        assert!(filter_self.passes(5, 0xFFFFFFFF, &empty_tags, 5));
    }

    #[test]
    fn test_filter_blocked() {
        let filter = TriggerFilter::new().block_entities([100, 200]);
        let empty_tags = HashSet::new();

        assert!(filter.passes(50, 0xFFFFFFFF, &empty_tags, 0));
        assert!(!filter.passes(100, 0xFFFFFFFF, &empty_tags, 0));
        assert!(!filter.passes(200, 0xFFFFFFFF, &empty_tags, 0));
    }
}
