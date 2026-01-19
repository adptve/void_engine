//! Trigger component

use crate::events::{TriggerEvent, TriggerHandler};
use crate::filter::TriggerFilter;
use crate::volume::TriggerVolume;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Trigger behavior mode
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TriggerMode {
    /// Fire on every enter/exit
    Repeatable,
    /// Fire once per entity, then ignore that entity
    OncePerEntity,
    /// Fire once total, then disable
    OneShot,
    /// Fire continuously while entity is inside (using cooldown)
    Continuous,
}

impl Default for TriggerMode {
    fn default() -> Self {
        Self::Repeatable
    }
}

/// State of an entity inside a trigger
#[derive(Debug, Clone)]
pub struct TriggerOverlapState {
    /// When the entity entered
    pub enter_time: f32,
    /// Total time inside
    pub time_inside: f32,
    /// Last continuous trigger time
    pub last_trigger_time: f32,
}

/// Trigger component for detecting overlaps
pub struct TriggerComponent {
    /// Trigger volume shape
    pub volume: TriggerVolume,
    /// Trigger filter
    pub filter: TriggerFilter,
    /// Trigger mode
    pub mode: TriggerMode,
    /// Whether the trigger is enabled
    pub enabled: bool,
    /// Cooldown between triggers (for Continuous mode)
    pub cooldown: f32,
    /// Current cooldown timer
    current_cooldown: f32,
    /// Whether to emit Stay events
    pub emit_stay_events: bool,
    /// Currently overlapping entities
    overlapping: HashMap<u64, TriggerOverlapState>,
    /// Entities that have been triggered (for OncePerEntity)
    triggered_entities: std::collections::HashSet<u64>,
    /// Whether the trigger has fired (for OneShot)
    has_fired: bool,
    /// Event handler (not serialized)
    handler: Option<TriggerHandler>,
    /// Queue of events to be processed
    event_queue: Vec<TriggerEvent>,
    /// Total trigger activations
    pub activation_count: u32,
}

impl TriggerComponent {
    /// Create a new trigger
    pub fn new(volume: TriggerVolume) -> Self {
        Self {
            volume,
            filter: TriggerFilter::new(),
            mode: TriggerMode::Repeatable,
            enabled: true,
            cooldown: 0.0,
            current_cooldown: 0.0,
            emit_stay_events: false,
            overlapping: HashMap::new(),
            triggered_entities: std::collections::HashSet::new(),
            has_fired: false,
            handler: None,
            event_queue: Vec::new(),
            activation_count: 0,
        }
    }

    /// Set filter
    pub fn with_filter(mut self, filter: TriggerFilter) -> Self {
        self.filter = filter;
        self
    }

    /// Set mode
    pub fn with_mode(mut self, mode: TriggerMode) -> Self {
        self.mode = mode;
        self
    }

    /// Set cooldown (for Continuous mode)
    pub fn with_cooldown(mut self, cooldown: f32) -> Self {
        self.cooldown = cooldown;
        self
    }

    /// Enable stay events
    pub fn with_stay_events(mut self) -> Self {
        self.emit_stay_events = true;
        self
    }

    /// Set event handler
    pub fn with_handler(mut self, handler: TriggerHandler) -> Self {
        self.handler = Some(handler);
        self
    }

    /// Set enter callback
    pub fn on_enter<F>(mut self, f: F) -> Self
    where
        F: Fn(&TriggerEvent) + Send + Sync + 'static,
    {
        let handler = self.handler.take().unwrap_or_else(TriggerHandler::new);
        self.handler = Some(handler.on_enter(f));
        self
    }

    /// Set exit callback
    pub fn on_exit<F>(mut self, f: F) -> Self
    where
        F: Fn(&TriggerEvent) + Send + Sync + 'static,
    {
        let handler = self.handler.take().unwrap_or_else(TriggerHandler::new);
        self.handler = Some(handler.on_exit(f));
        self
    }

    /// Set stay callback
    pub fn on_stay<F>(mut self, f: F) -> Self
    where
        F: Fn(&TriggerEvent) + Send + Sync + 'static,
    {
        let handler = self.handler.take().unwrap_or_else(TriggerHandler::new);
        self.handler = Some(handler.on_stay(f));
        self.emit_stay_events = true;
        self
    }

    /// Check if an entity is currently inside this trigger
    pub fn is_inside(&self, entity: u64) -> bool {
        self.overlapping.contains_key(&entity)
    }

    /// Get all entities currently inside
    pub fn get_overlapping(&self) -> impl Iterator<Item = u64> + '_ {
        self.overlapping.keys().copied()
    }

    /// Get the time an entity has been inside
    pub fn time_inside(&self, entity: u64) -> Option<f32> {
        self.overlapping.get(&entity).map(|s| s.time_inside)
    }

    /// Enable the trigger
    pub fn enable(&mut self) {
        self.enabled = true;
    }

    /// Disable the trigger
    pub fn disable(&mut self) {
        self.enabled = false;
    }

    /// Reset the trigger (for OneShot or OncePerEntity modes)
    pub fn reset(&mut self) {
        self.has_fired = false;
        self.triggered_entities.clear();
        self.overlapping.clear();
        self.current_cooldown = 0.0;
    }

    /// Process an entity entering/exiting
    pub fn process_overlap(
        &mut self,
        trigger_entity: u64,
        other_entity: u64,
        is_overlapping: bool,
        current_time: f32,
        entity_layers: u32,
        entity_tags: &std::collections::HashSet<String>,
    ) {
        if !self.enabled {
            return;
        }

        // Check filter
        if !self.filter.passes(other_entity, entity_layers, entity_tags, trigger_entity) {
            return;
        }

        let was_overlapping = self.overlapping.contains_key(&other_entity);

        // Check mode restrictions for new entries only
        if is_overlapping && !was_overlapping {
            match self.mode {
                TriggerMode::OneShot if self.has_fired => return,
                TriggerMode::OncePerEntity if self.triggered_entities.contains(&other_entity) => {
                    return
                }
                _ => {}
            }
        }

        if is_overlapping && !was_overlapping {
            // Enter event
            self.overlapping.insert(
                other_entity,
                TriggerOverlapState {
                    enter_time: current_time,
                    time_inside: 0.0,
                    last_trigger_time: current_time,
                },
            );

            let event = TriggerEvent::enter(trigger_entity, other_entity);
            self.queue_event(event);

            // Mark as triggered for OneShot/OncePerEntity
            match self.mode {
                TriggerMode::OneShot => self.has_fired = true,
                TriggerMode::OncePerEntity => {
                    self.triggered_entities.insert(other_entity);
                }
                _ => {}
            }

            self.activation_count += 1;
        } else if !is_overlapping && was_overlapping {
            // Exit event
            let state = self.overlapping.remove(&other_entity).unwrap();
            let event = TriggerEvent::exit(trigger_entity, other_entity, state.time_inside);
            self.queue_event(event);
        }
    }

    /// Update the trigger (call each frame)
    pub fn update(&mut self, trigger_entity: u64, delta_time: f32, current_time: f32) {
        if !self.enabled {
            return;
        }

        // Update cooldown
        if self.current_cooldown > 0.0 {
            self.current_cooldown -= delta_time;
        }

        // Update overlap states and generate stay/continuous events
        for (&entity, state) in self.overlapping.iter_mut() {
            state.time_inside += delta_time;

            if self.emit_stay_events {
                let event = TriggerEvent::stay(trigger_entity, entity, state.time_inside);
                self.event_queue.push(event);
            }

            if self.mode == TriggerMode::Continuous {
                if self.current_cooldown <= 0.0 {
                    // Trigger continuous event
                    state.last_trigger_time = current_time;
                    self.current_cooldown = self.cooldown;
                    self.activation_count += 1;
                    // Could emit a specific "continuous trigger" event here
                }
            }
        }
    }

    /// Process queued events
    pub fn process_events(&mut self) {
        let events: Vec<_> = self.event_queue.drain(..).collect();
        if let Some(ref handler) = self.handler {
            for event in &events {
                handler.handle(event);
            }
        }
    }

    /// Drain events for external processing
    pub fn drain_events(&mut self) -> Vec<TriggerEvent> {
        self.event_queue.drain(..).collect()
    }

    /// Queue an event
    fn queue_event(&mut self, event: TriggerEvent) {
        self.event_queue.push(event);
    }
}

impl Default for TriggerComponent {
    fn default() -> Self {
        Self::new(TriggerVolume::default())
    }
}

// Manual Debug implementation (skip handler)
impl std::fmt::Debug for TriggerComponent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TriggerComponent")
            .field("volume", &self.volume)
            .field("filter", &self.filter)
            .field("mode", &self.mode)
            .field("enabled", &self.enabled)
            .field("overlapping_count", &self.overlapping.len())
            .field("has_fired", &self.has_fired)
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_trigger_creation() {
        let trigger = TriggerComponent::new(TriggerVolume::sphere(5.0))
            .with_mode(TriggerMode::OneShot)
            .with_filter(TriggerFilter::player_only());

        assert_eq!(trigger.mode, TriggerMode::OneShot);
        assert!(trigger.enabled);
    }

    #[test]
    fn test_trigger_overlap() {
        let mut trigger = TriggerComponent::new(TriggerVolume::sphere(1.0));
        let tags = std::collections::HashSet::new();

        // Entity enters
        trigger.process_overlap(1, 2, true, 0.0, 0xFFFFFFFF, &tags);
        assert!(trigger.is_inside(2));

        // Entity exits
        trigger.process_overlap(1, 2, false, 1.0, 0xFFFFFFFF, &tags);
        assert!(!trigger.is_inside(2));
    }

    #[test]
    fn test_one_shot_mode() {
        let mut trigger =
            TriggerComponent::new(TriggerVolume::sphere(1.0)).with_mode(TriggerMode::OneShot);
        let tags = std::collections::HashSet::new();

        // First entity triggers
        trigger.process_overlap(1, 2, true, 0.0, 0xFFFFFFFF, &tags);
        trigger.process_overlap(1, 2, false, 0.5, 0xFFFFFFFF, &tags);

        // Second entity should not trigger
        trigger.process_overlap(1, 3, true, 1.0, 0xFFFFFFFF, &tags);
        assert!(!trigger.is_inside(3)); // Wasn't recorded because trigger already fired
    }

    #[test]
    fn test_once_per_entity() {
        let mut trigger =
            TriggerComponent::new(TriggerVolume::sphere(1.0)).with_mode(TriggerMode::OncePerEntity);
        let tags = std::collections::HashSet::new();

        // First enter
        trigger.process_overlap(1, 2, true, 0.0, 0xFFFFFFFF, &tags);
        assert!(trigger.is_inside(2));
        trigger.process_overlap(1, 2, false, 0.5, 0xFFFFFFFF, &tags);

        // Second enter for same entity - should not register
        trigger.process_overlap(1, 2, true, 1.0, 0xFFFFFFFF, &tags);
        assert!(!trigger.is_inside(2));

        // Different entity should still work
        trigger.process_overlap(1, 3, true, 1.5, 0xFFFFFFFF, &tags);
        assert!(trigger.is_inside(3));
    }

    #[test]
    fn test_trigger_callbacks() {
        let enter_count = Arc::new(AtomicU32::new(0));
        let enter_clone = enter_count.clone();

        let mut trigger = TriggerComponent::new(TriggerVolume::sphere(1.0)).on_enter(move |_| {
            enter_clone.fetch_add(1, Ordering::SeqCst);
        });

        let tags = std::collections::HashSet::new();

        trigger.process_overlap(1, 2, true, 0.0, 0xFFFFFFFF, &tags);
        trigger.process_events();

        assert_eq!(enter_count.load(Ordering::SeqCst), 1);
    }
}
