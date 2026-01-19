//! Trigger system for processing overlaps

use crate::events::TriggerEvent;
use crate::trigger::TriggerComponent;
use crate::volume::TriggerVolume;
use std::collections::{HashMap, HashSet};

/// Entity data for trigger system
#[derive(Debug, Clone)]
pub struct TriggerEntity {
    /// Entity ID
    pub id: u64,
    /// World position
    pub position: [f32; 3],
    /// World rotation (quaternion)
    pub rotation: [f32; 4],
    /// World scale
    pub scale: [f32; 3],
    /// Collision layers
    pub layers: u32,
    /// Entity tags
    pub tags: HashSet<String>,
}

impl TriggerEntity {
    /// Create a new trigger entity
    pub fn new(id: u64) -> Self {
        Self {
            id,
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0, 1.0],
            scale: [1.0, 1.0, 1.0],
            layers: 0xFFFFFFFF,
            tags: HashSet::new(),
        }
    }

    /// Set position
    pub fn with_position(mut self, position: [f32; 3]) -> Self {
        self.position = position;
        self
    }

    /// Set layers
    pub fn with_layers(mut self, layers: u32) -> Self {
        self.layers = layers;
        self
    }

    /// Add a tag
    pub fn with_tag(mut self, tag: impl Into<String>) -> Self {
        self.tags.insert(tag.into());
        self
    }
}

/// The trigger system that processes all triggers
pub struct TriggerSystem {
    /// Registered triggers (entity_id -> trigger component)
    triggers: HashMap<u64, TriggerComponent>,
    /// Current simulation time
    current_time: f32,
    /// Collected events from last update
    last_events: Vec<TriggerEvent>,
}

impl TriggerSystem {
    /// Create a new trigger system
    pub fn new() -> Self {
        Self {
            triggers: HashMap::new(),
            current_time: 0.0,
            last_events: Vec::new(),
        }
    }

    /// Register a trigger
    pub fn register_trigger(&mut self, entity_id: u64, trigger: TriggerComponent) {
        self.triggers.insert(entity_id, trigger);
    }

    /// Unregister a trigger
    pub fn unregister_trigger(&mut self, entity_id: u64) {
        self.triggers.remove(&entity_id);
    }

    /// Get a trigger
    pub fn get_trigger(&self, entity_id: u64) -> Option<&TriggerComponent> {
        self.triggers.get(&entity_id)
    }

    /// Get a mutable trigger
    pub fn get_trigger_mut(&mut self, entity_id: u64) -> Option<&mut TriggerComponent> {
        self.triggers.get_mut(&entity_id)
    }

    /// Check if a point overlaps with a trigger
    pub fn point_overlaps(
        &self,
        trigger_id: u64,
        trigger_pos: [f32; 3],
        trigger_scale: [f32; 3],
        point: [f32; 3],
    ) -> bool {
        if let Some(trigger) = self.triggers.get(&trigger_id) {
            trigger.volume.contains_point_transformed(
                point,
                trigger_pos,
                [0.0, 0.0, 0.0, 1.0],
                trigger_scale,
            )
        } else {
            false
        }
    }

    /// Update the trigger system
    ///
    /// # Arguments
    /// * `delta_time` - Time since last update
    /// * `trigger_entities` - All entities with triggers (entity_id -> transform)
    /// * `other_entities` - All entities that can trigger (position, layers, tags)
    pub fn update(
        &mut self,
        delta_time: f32,
        trigger_entities: &[(u64, [f32; 3], [f32; 3])], // (id, position, scale)
        other_entities: &[TriggerEntity],
    ) {
        self.current_time += delta_time;
        self.last_events.clear();

        // For each trigger, check overlaps with all other entities
        for &(trigger_id, trigger_pos, trigger_scale) in trigger_entities {
            if let Some(trigger) = self.triggers.get_mut(&trigger_id) {
                // Track which entities are currently overlapping
                let mut currently_overlapping: HashSet<u64> = HashSet::new();

                for entity in other_entities {
                    // Skip self
                    if entity.id == trigger_id {
                        continue;
                    }

                    // Check if entity position is inside trigger volume
                    let is_overlapping = trigger.volume.contains_point_transformed(
                        entity.position,
                        trigger_pos,
                        [0.0, 0.0, 0.0, 1.0], // No rotation support yet
                        trigger_scale,
                    );

                    if is_overlapping {
                        currently_overlapping.insert(entity.id);
                    }

                    // Process overlap state change
                    trigger.process_overlap(
                        trigger_id,
                        entity.id,
                        is_overlapping,
                        self.current_time,
                        entity.layers,
                        &entity.tags,
                    );
                }

                // Check for entities that left (were overlapping but aren't now)
                let previously_overlapping: HashSet<_> =
                    trigger.get_overlapping().collect();

                for &entity_id in &previously_overlapping {
                    if !currently_overlapping.contains(&entity_id) {
                        // Entity left - find their data for the exit event
                        if let Some(entity) = other_entities.iter().find(|e| e.id == entity_id) {
                            trigger.process_overlap(
                                trigger_id,
                                entity_id,
                                false,
                                self.current_time,
                                entity.layers,
                                &entity.tags,
                            );
                        }
                    }
                }

                // Update trigger state
                trigger.update(trigger_id, delta_time, self.current_time);

                // Collect events
                self.last_events.extend(trigger.drain_events());

                // Process callbacks
                trigger.process_events();
            }
        }
    }

    /// Get events from the last update
    pub fn get_events(&self) -> &[TriggerEvent] {
        &self.last_events
    }

    /// Drain events from the last update
    pub fn drain_events(&mut self) -> Vec<TriggerEvent> {
        std::mem::take(&mut self.last_events)
    }

    /// Get all trigger entity IDs
    pub fn trigger_ids(&self) -> impl Iterator<Item = u64> + '_ {
        self.triggers.keys().copied()
    }

    /// Get trigger count
    pub fn trigger_count(&self) -> usize {
        self.triggers.len()
    }
}

impl Default for TriggerSystem {
    fn default() -> Self {
        Self::new()
    }
}

/// Query result for trigger overlaps
#[derive(Debug, Clone)]
pub struct TriggerQueryResult {
    /// Trigger entity ID
    pub trigger_id: u64,
    /// Distance to trigger center (squared)
    pub distance_sq: f32,
}

/// Query the trigger system
impl TriggerSystem {
    /// Find all triggers that contain a point
    pub fn query_point(
        &self,
        point: [f32; 3],
        trigger_transforms: &[(u64, [f32; 3], [f32; 3])],
    ) -> Vec<TriggerQueryResult> {
        let mut results = Vec::new();

        for &(trigger_id, pos, scale) in trigger_transforms {
            if let Some(trigger) = self.triggers.get(&trigger_id) {
                if trigger.volume.contains_point_transformed(
                    point,
                    pos,
                    [0.0, 0.0, 0.0, 1.0],
                    scale,
                ) {
                    let dx = point[0] - pos[0];
                    let dy = point[1] - pos[1];
                    let dz = point[2] - pos[2];
                    results.push(TriggerQueryResult {
                        trigger_id,
                        distance_sq: dx * dx + dy * dy + dz * dz,
                    });
                }
            }
        }

        // Sort by distance
        results.sort_by(|a, b| a.distance_sq.partial_cmp(&b.distance_sq).unwrap());
        results
    }

    /// Find the nearest trigger containing a point
    pub fn query_nearest(
        &self,
        point: [f32; 3],
        trigger_transforms: &[(u64, [f32; 3], [f32; 3])],
    ) -> Option<TriggerQueryResult> {
        self.query_point(point, trigger_transforms).into_iter().next()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_trigger_system() {
        let mut system = TriggerSystem::new();

        // Register a trigger
        let trigger = TriggerComponent::new(TriggerVolume::sphere(1.0));
        system.register_trigger(1, trigger);

        assert_eq!(system.trigger_count(), 1);
        assert!(system.get_trigger(1).is_some());
    }

    #[test]
    fn test_trigger_overlap_detection() {
        let mut system = TriggerSystem::new();

        // Create a sphere trigger at origin
        let trigger = TriggerComponent::new(TriggerVolume::sphere(2.0));
        system.register_trigger(1, trigger);

        // Trigger transform
        let trigger_transforms = vec![(1, [0.0, 0.0, 0.0], [1.0, 1.0, 1.0])];

        // Entity inside
        let entities = vec![TriggerEntity::new(2).with_position([0.5, 0.0, 0.0])];

        system.update(0.016, &trigger_transforms, &entities);

        // Check that entity entered
        let trigger = system.get_trigger(1).unwrap();
        assert!(trigger.is_inside(2));
    }

    #[test]
    fn test_trigger_exit() {
        let mut system = TriggerSystem::new();

        let trigger = TriggerComponent::new(TriggerVolume::sphere(1.0));
        system.register_trigger(1, trigger);

        let trigger_transforms = vec![(1, [0.0, 0.0, 0.0], [1.0, 1.0, 1.0])];

        // Entity enters
        let entities_inside = vec![TriggerEntity::new(2).with_position([0.0, 0.0, 0.0])];
        system.update(0.016, &trigger_transforms, &entities_inside);
        assert!(system.get_trigger(1).unwrap().is_inside(2));

        // Entity exits
        let entities_outside = vec![TriggerEntity::new(2).with_position([10.0, 0.0, 0.0])];
        system.update(0.016, &trigger_transforms, &entities_outside);
        assert!(!system.get_trigger(1).unwrap().is_inside(2));
    }

    #[test]
    fn test_query_point() {
        let mut system = TriggerSystem::new();

        system.register_trigger(1, TriggerComponent::new(TriggerVolume::sphere(2.0)));
        system.register_trigger(2, TriggerComponent::new(TriggerVolume::sphere(1.0)));

        let transforms = vec![
            (1, [0.0, 0.0, 0.0], [1.0, 1.0, 1.0]),
            (2, [5.0, 0.0, 0.0], [1.0, 1.0, 1.0]),
        ];

        // Query point inside trigger 1
        let results = system.query_point([0.5, 0.0, 0.0], &transforms);
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].trigger_id, 1);

        // Query point inside trigger 2
        let results = system.query_point([5.0, 0.0, 0.0], &transforms);
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].trigger_id, 2);

        // Query point outside both
        let results = system.query_point([10.0, 10.0, 10.0], &transforms);
        assert!(results.is_empty());
    }
}
