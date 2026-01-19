//! Entity-Level Input Event System
//!
//! Provides input event processing for entity interaction.
//!
//! # Overview
//!
//! This module converts raw pointer/touch events into entity-level events
//! like hover, click, drag, and scroll. It supports:
//!
//! - Pointer enter/exit detection
//! - Click and double-click detection
//! - Drag operations with threshold
//! - Event bubbling through hierarchy
//! - Input capture for modal interactions
//! - Hot-reload state preservation
//!
//! # Example
//!
//! ```ignore
//! use void_render::input::{EntityInputProcessor, InputProcessingMode};
//! use void_ecs::{InputHandler, PointerButton};
//!
//! let mut processor = EntityInputProcessor::new();
//!
//! // Process pointer events
//! processor.on_pointer_move(
//!     screen_pos,
//!     screen_size,
//!     pointer_id,
//!     &view_matrix,
//!     &projection_matrix,
//!     hit_result,
//!     &|e| world.get::<InputHandler>(e),
//!     &|e| world.get::<Parent>(e).map(|p| p.entity),
//! );
//!
//! // Drain and process events
//! for event in processor.drain_events() {
//!     match event {
//!         EntityInputEvent::Click { entity_bits, .. } => {
//!             // Handle click
//!         }
//!         _ => {}
//!     }
//! }
//! ```

pub mod state;
pub mod processor;

pub use state::{InputSystemState, InputStateSnapshot, DragState};
pub use processor::{EntityInputProcessor, EntityInputProcessorState, InputProcessingMode};

use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use void_ecs::{Entity, EntityInputEvent, InputHandler};

/// Pending input system updates to be applied at frame boundary
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum InputSystemUpdate {
    /// Update input handler for entity
    UpdateHandler {
        entity_bits: u64,
        handler: InputHandler,
    },
    /// Force release capture
    ReleaseCapture,
    /// Clear hover state for pointer
    ClearHover { pointer_id: u32 },
    /// Inject synthetic event
    InjectEvent { event: EntityInputEvent },
    /// Update double-click threshold
    SetDoubleClickThreshold { seconds: f64 },
    /// Update drag threshold
    SetDragThreshold { pixels: f32 },
}

/// Queue for input system updates
#[derive(Debug, Default)]
pub struct InputUpdateQueue {
    updates: Vec<InputSystemUpdate>,
}

impl InputUpdateQueue {
    /// Create a new update queue
    pub fn new() -> Self {
        Self {
            updates: Vec::new(),
        }
    }

    /// Queue an update for next frame boundary
    pub fn queue(&mut self, update: InputSystemUpdate) {
        self.updates.push(update);
    }

    /// Queue handler update
    pub fn update_handler(&mut self, entity: Entity, handler: InputHandler) {
        self.queue(InputSystemUpdate::UpdateHandler {
            entity_bits: entity.to_bits(),
            handler,
        });
    }

    /// Queue capture release
    pub fn release_capture(&mut self) {
        self.queue(InputSystemUpdate::ReleaseCapture);
    }

    /// Queue hover clear
    pub fn clear_hover(&mut self, pointer_id: u32) {
        self.queue(InputSystemUpdate::ClearHover { pointer_id });
    }

    /// Queue event injection
    pub fn inject_event(&mut self, event: EntityInputEvent) {
        self.queue(InputSystemUpdate::InjectEvent { event });
    }

    /// Set double-click threshold
    pub fn set_double_click_threshold(&mut self, seconds: f64) {
        self.queue(InputSystemUpdate::SetDoubleClickThreshold { seconds });
    }

    /// Set drag threshold
    pub fn set_drag_threshold(&mut self, pixels: f32) {
        self.queue(InputSystemUpdate::SetDragThreshold { pixels });
    }

    /// Apply all pending updates
    pub fn apply_pending<F>(
        &mut self,
        processor: &mut EntityInputProcessor,
        mut update_handler: F,
    ) where
        F: FnMut(Entity, InputHandler),
    {
        for update in self.updates.drain(..) {
            match update {
                InputSystemUpdate::UpdateHandler { entity_bits, handler } => {
                    update_handler(Entity::from_bits(entity_bits), handler);
                }
                InputSystemUpdate::ReleaseCapture => {
                    processor.state.release_capture();
                }
                InputSystemUpdate::ClearHover { pointer_id } => {
                    processor.state.clear_hovered(pointer_id);
                }
                InputSystemUpdate::InjectEvent { event } => {
                    processor.event_queue.push(event);
                }
                InputSystemUpdate::SetDoubleClickThreshold { seconds } => {
                    processor.state.double_click_threshold = seconds;
                }
                InputSystemUpdate::SetDragThreshold { pixels } => {
                    processor.state.drag_threshold = pixels;
                }
            }
        }
    }

    /// Check if queue is empty
    pub fn is_empty(&self) -> bool {
        self.updates.is_empty()
    }

    /// Get number of pending updates
    pub fn len(&self) -> usize {
        self.updates.len()
    }
}

/// Event queue that preserves events across hot-reload
#[derive(Debug, Default, Clone)]
pub struct PersistentEventQueue {
    events: Vec<EntityInputEvent>,
    max_preserved: usize,
}

impl PersistentEventQueue {
    /// Create a new persistent event queue
    pub fn new(max_preserved: usize) -> Self {
        Self {
            events: Vec::new(),
            max_preserved,
        }
    }

    /// Push an event
    pub fn push(&mut self, event: EntityInputEvent) {
        self.events.push(event);
    }

    /// Drain all events
    pub fn drain(&mut self) -> impl Iterator<Item = EntityInputEvent> + '_ {
        self.events.drain(..)
    }

    /// Get number of events
    pub fn len(&self) -> usize {
        self.events.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.events.is_empty()
    }

    /// Save events for hot-reload (limits to prevent unbounded growth)
    pub fn save_for_reload(&self) -> Vec<EntityInputEvent> {
        self.events
            .iter()
            .take(self.max_preserved)
            .cloned()
            .collect()
    }

    /// Restore events from hot-reload
    pub fn restore_from_reload(&mut self, events: Vec<EntityInputEvent>) {
        self.events = events;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use void_ecs::InputEventKind;

    #[test]
    fn test_input_update_queue() {
        let mut queue = InputUpdateQueue::new();
        let entity = Entity::from_bits(42);

        queue.update_handler(entity, InputHandler::clickable());
        queue.release_capture();
        queue.clear_hover(0);

        assert_eq!(queue.len(), 3);
        assert!(!queue.is_empty());
    }

    #[test]
    fn test_input_update_queue_apply() {
        let mut queue = InputUpdateQueue::new();
        let mut processor = EntityInputProcessor::new();

        let entity = Entity::from_bits(42);
        processor.state.capture(entity, 0);

        queue.release_capture();

        let mut updated_handlers = Vec::new();
        queue.apply_pending(&mut processor, |e, h| {
            updated_handlers.push((e, h));
        });

        assert!(!processor.state.is_captured());
    }

    #[test]
    fn test_persistent_event_queue() {
        let mut queue = PersistentEventQueue::new(10);
        let entity = Entity::from_bits(42);

        queue.push(EntityInputEvent::fallback_click(entity));
        queue.push(EntityInputEvent::pointer_enter(entity, [0.0; 3], 0));

        assert_eq!(queue.len(), 2);

        let saved = queue.save_for_reload();
        assert_eq!(saved.len(), 2);

        let mut new_queue = PersistentEventQueue::new(10);
        new_queue.restore_from_reload(saved);
        assert_eq!(new_queue.len(), 2);
    }

    #[test]
    fn test_persistent_event_queue_max() {
        let mut queue = PersistentEventQueue::new(2);
        let entity = Entity::from_bits(42);

        for i in 0..5 {
            queue.push(EntityInputEvent::fallback_click(Entity::from_bits(i)));
        }

        let saved = queue.save_for_reload();
        assert_eq!(saved.len(), 2); // Limited to max_preserved
    }

    #[test]
    fn test_input_system_update_serialization() {
        let entity = Entity::from_bits(42);

        let updates = vec![
            InputSystemUpdate::ReleaseCapture,
            InputSystemUpdate::ClearHover { pointer_id: 0 },
            InputSystemUpdate::SetDoubleClickThreshold { seconds: 0.5 },
            InputSystemUpdate::SetDragThreshold { pixels: 10.0 },
        ];

        for update in updates {
            let json = serde_json::to_string(&update).unwrap();
            let restored: InputSystemUpdate = serde_json::from_str(&json).unwrap();
            // Just verify it deserializes without error
            let _ = restored;
        }
    }
}
