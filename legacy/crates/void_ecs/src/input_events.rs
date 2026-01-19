//! Entity-Level Input Events
//!
//! Provides input event types for entity interaction including:
//! - Pointer enter/exit/move events
//! - Click and double-click detection
//! - Drag start/drag/end events
//! - Scroll events
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::input_events::{EntityInputEvent, PointerButton};
//!
//! fn handle_event(event: &EntityInputEvent) {
//!     match event {
//!         EntityInputEvent::Click { entity, position, button, .. } => {
//!             println!("Clicked entity {:?} at {:?}", entity, position);
//!         }
//!         _ => {}
//!     }
//! }
//! ```

use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::Entity;

/// Pointer button type
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum PointerButton {
    /// Left mouse button / primary touch
    Primary,
    /// Right mouse button
    Secondary,
    /// Middle mouse button
    Middle,
    /// Mouse button 4
    Extra1,
    /// Mouse button 5
    Extra2,
}

impl Default for PointerButton {
    fn default() -> Self {
        Self::Primary
    }
}

/// Input event targeting an entity
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum EntityInputEvent {
    /// Pointer entered entity bounds
    PointerEnter {
        /// Target entity (stored as bits for serialization)
        entity_bits: u64,
        /// World-space hit position
        position: [f32; 3],
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Pointer exited entity bounds
    PointerExit {
        /// Target entity
        entity_bits: u64,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Pointer moved over entity
    PointerMove {
        /// Target entity
        entity_bits: u64,
        /// World-space position
        position: [f32; 3],
        /// Screen-space delta since last move
        delta: [f32; 2],
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Pointer button pressed on entity
    PointerDown {
        /// Target entity
        entity_bits: u64,
        /// World-space hit position
        position: [f32; 3],
        /// Which button was pressed
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Pointer button released on entity
    PointerUp {
        /// Target entity
        entity_bits: u64,
        /// World-space position
        position: [f32; 3],
        /// Which button was released
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Click (down + up on same entity)
    Click {
        /// Target entity
        entity_bits: u64,
        /// World-space hit position
        position: [f32; 3],
        /// Which button was clicked
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Double click
    DoubleClick {
        /// Target entity
        entity_bits: u64,
        /// World-space hit position
        position: [f32; 3],
        /// Which button was double-clicked
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Drag started
    DragStart {
        /// Target entity being dragged
        entity_bits: u64,
        /// World-space drag start position
        position: [f32; 3],
        /// Which button initiated the drag
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Drag in progress
    Drag {
        /// Target entity being dragged
        entity_bits: u64,
        /// Current world-space position
        position: [f32; 3],
        /// Screen-space delta since drag start
        delta: [f32; 2],
        /// Which button is dragging
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Drag ended
    DragEnd {
        /// Target entity that was dragged
        entity_bits: u64,
        /// Final world-space position
        position: [f32; 3],
        /// Which button ended the drag
        button: PointerButton,
        /// Pointer/touch identifier
        pointer_id: u32,
    },

    /// Scroll wheel over entity
    Scroll {
        /// Target entity
        entity_bits: u64,
        /// World-space position
        position: [f32; 3],
        /// Scroll delta [x, y]
        delta: [f32; 2],
    },
}

impl EntityInputEvent {
    /// Get the entity bits for this event
    pub fn entity_bits(&self) -> u64 {
        match self {
            Self::PointerEnter { entity_bits, .. } => *entity_bits,
            Self::PointerExit { entity_bits, .. } => *entity_bits,
            Self::PointerMove { entity_bits, .. } => *entity_bits,
            Self::PointerDown { entity_bits, .. } => *entity_bits,
            Self::PointerUp { entity_bits, .. } => *entity_bits,
            Self::Click { entity_bits, .. } => *entity_bits,
            Self::DoubleClick { entity_bits, .. } => *entity_bits,
            Self::DragStart { entity_bits, .. } => *entity_bits,
            Self::Drag { entity_bits, .. } => *entity_bits,
            Self::DragEnd { entity_bits, .. } => *entity_bits,
            Self::Scroll { entity_bits, .. } => *entity_bits,
        }
    }

    /// Get the entity for this event
    pub fn entity(&self) -> Entity {
        Entity::from_bits(self.entity_bits())
    }

    /// Get the pointer ID for this event (if applicable)
    pub fn pointer_id(&self) -> Option<u32> {
        match self {
            Self::PointerEnter { pointer_id, .. } => Some(*pointer_id),
            Self::PointerExit { pointer_id, .. } => Some(*pointer_id),
            Self::PointerMove { pointer_id, .. } => Some(*pointer_id),
            Self::PointerDown { pointer_id, .. } => Some(*pointer_id),
            Self::PointerUp { pointer_id, .. } => Some(*pointer_id),
            Self::Click { pointer_id, .. } => Some(*pointer_id),
            Self::DoubleClick { pointer_id, .. } => Some(*pointer_id),
            Self::DragStart { pointer_id, .. } => Some(*pointer_id),
            Self::Drag { pointer_id, .. } => Some(*pointer_id),
            Self::DragEnd { pointer_id, .. } => Some(*pointer_id),
            Self::Scroll { .. } => None,
        }
    }

    /// Get the position for this event (if applicable)
    pub fn position(&self) -> Option<[f32; 3]> {
        match self {
            Self::PointerEnter { position, .. } => Some(*position),
            Self::PointerExit { .. } => None,
            Self::PointerMove { position, .. } => Some(*position),
            Self::PointerDown { position, .. } => Some(*position),
            Self::PointerUp { position, .. } => Some(*position),
            Self::Click { position, .. } => Some(*position),
            Self::DoubleClick { position, .. } => Some(*position),
            Self::DragStart { position, .. } => Some(*position),
            Self::Drag { position, .. } => Some(*position),
            Self::DragEnd { position, .. } => Some(*position),
            Self::Scroll { position, .. } => Some(*position),
        }
    }

    /// Get the button for this event (if applicable)
    pub fn button(&self) -> Option<PointerButton> {
        match self {
            Self::PointerDown { button, .. } => Some(*button),
            Self::PointerUp { button, .. } => Some(*button),
            Self::Click { button, .. } => Some(*button),
            Self::DoubleClick { button, .. } => Some(*button),
            Self::DragStart { button, .. } => Some(*button),
            Self::Drag { button, .. } => Some(*button),
            Self::DragEnd { button, .. } => Some(*button),
            _ => None,
        }
    }

    /// Check if this is a hover event
    pub fn is_hover_event(&self) -> bool {
        matches!(
            self,
            Self::PointerEnter { .. } | Self::PointerExit { .. } | Self::PointerMove { .. }
        )
    }

    /// Check if this is a click-related event
    pub fn is_click_event(&self) -> bool {
        matches!(
            self,
            Self::PointerDown { .. }
                | Self::PointerUp { .. }
                | Self::Click { .. }
                | Self::DoubleClick { .. }
        )
    }

    /// Check if this is a drag event
    pub fn is_drag_event(&self) -> bool {
        matches!(
            self,
            Self::DragStart { .. } | Self::Drag { .. } | Self::DragEnd { .. }
        )
    }

    /// Create a PointerEnter event
    pub fn pointer_enter(entity: Entity, position: [f32; 3], pointer_id: u32) -> Self {
        Self::PointerEnter {
            entity_bits: entity.to_bits(),
            position,
            pointer_id,
        }
    }

    /// Create a PointerExit event
    pub fn pointer_exit(entity: Entity, pointer_id: u32) -> Self {
        Self::PointerExit {
            entity_bits: entity.to_bits(),
            pointer_id,
        }
    }

    /// Create a Click event
    pub fn click(entity: Entity, position: [f32; 3], button: PointerButton, pointer_id: u32) -> Self {
        Self::Click {
            entity_bits: entity.to_bits(),
            position,
            button,
            pointer_id,
        }
    }

    /// Create a minimal fallback click event (for error recovery)
    pub fn fallback_click(entity: Entity) -> Self {
        Self::Click {
            entity_bits: entity.to_bits(),
            position: [0.0, 0.0, 0.0],
            button: PointerButton::Primary,
            pointer_id: 0,
        }
    }
}

/// Collection of entity input events for a frame
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct EntityInputEvents {
    events: Vec<EntityInputEvent>,
}

impl EntityInputEvents {
    /// Create empty event collection
    pub fn new() -> Self {
        Self { events: Vec::new() }
    }

    /// Add an event
    pub fn push(&mut self, event: EntityInputEvent) {
        self.events.push(event);
    }

    /// Get number of events
    pub fn len(&self) -> usize {
        self.events.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.events.is_empty()
    }

    /// Iterate over events
    pub fn iter(&self) -> impl Iterator<Item = &EntityInputEvent> {
        self.events.iter()
    }

    /// Drain all events
    pub fn drain(&mut self) -> impl Iterator<Item = EntityInputEvent> + '_ {
        self.events.drain(..)
    }

    /// Clear all events
    pub fn clear(&mut self) {
        self.events.clear();
    }

    /// Filter events for a specific entity
    pub fn for_entity(&self, entity: Entity) -> impl Iterator<Item = &EntityInputEvent> {
        let bits = entity.to_bits();
        self.events.iter().filter(move |e| e.entity_bits() == bits)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pointer_button_default() {
        assert_eq!(PointerButton::default(), PointerButton::Primary);
    }

    #[test]
    fn test_entity_input_event_entity() {
        let entity = Entity::from_bits(42);
        let event = EntityInputEvent::click(entity, [1.0, 2.0, 3.0], PointerButton::Primary, 0);

        assert_eq!(event.entity_bits(), 42);
        assert_eq!(event.entity(), entity);
    }

    #[test]
    fn test_entity_input_event_position() {
        let entity = Entity::from_bits(1);

        let click = EntityInputEvent::Click {
            entity_bits: entity.to_bits(),
            position: [1.0, 2.0, 3.0],
            button: PointerButton::Primary,
            pointer_id: 0,
        };
        assert_eq!(click.position(), Some([1.0, 2.0, 3.0]));

        let exit = EntityInputEvent::PointerExit {
            entity_bits: entity.to_bits(),
            pointer_id: 0,
        };
        assert_eq!(exit.position(), None);
    }

    #[test]
    fn test_entity_input_event_is_hover() {
        let entity = Entity::from_bits(1);

        let enter = EntityInputEvent::pointer_enter(entity, [0.0; 3], 0);
        assert!(enter.is_hover_event());

        let click = EntityInputEvent::fallback_click(entity);
        assert!(!click.is_hover_event());
    }

    #[test]
    fn test_entity_input_event_is_drag() {
        let entity = Entity::from_bits(1);

        let drag_start = EntityInputEvent::DragStart {
            entity_bits: entity.to_bits(),
            position: [0.0; 3],
            button: PointerButton::Primary,
            pointer_id: 0,
        };
        assert!(drag_start.is_drag_event());

        let click = EntityInputEvent::fallback_click(entity);
        assert!(!click.is_drag_event());
    }

    #[test]
    fn test_entity_input_events_collection() {
        let mut events = EntityInputEvents::new();
        let entity1 = Entity::from_bits(1);
        let entity2 = Entity::from_bits(2);

        events.push(EntityInputEvent::fallback_click(entity1));
        events.push(EntityInputEvent::fallback_click(entity2));
        events.push(EntityInputEvent::pointer_enter(entity1, [0.0; 3], 0));

        assert_eq!(events.len(), 3);

        let entity1_events: Vec<_> = events.for_entity(entity1).collect();
        assert_eq!(entity1_events.len(), 2);
    }

    #[test]
    fn test_entity_input_event_serialization() {
        let entity = Entity::from_bits(42);
        let event = EntityInputEvent::Click {
            entity_bits: entity.to_bits(),
            position: [1.0, 2.0, 3.0],
            button: PointerButton::Primary,
            pointer_id: 0,
        };

        let json = serde_json::to_string(&event).unwrap();
        let restored: EntityInputEvent = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.entity_bits(), 42);
        assert_eq!(restored.position(), Some([1.0, 2.0, 3.0]));
    }

    #[test]
    fn test_pointer_button_serialization() {
        let buttons = [
            PointerButton::Primary,
            PointerButton::Secondary,
            PointerButton::Middle,
            PointerButton::Extra1,
            PointerButton::Extra2,
        ];

        for button in buttons {
            let json = serde_json::to_string(&button).unwrap();
            let restored: PointerButton = serde_json::from_str(&json).unwrap();
            assert_eq!(button, restored);
        }
    }

    #[test]
    fn test_all_event_variants_serialize() {
        let entity = Entity::from_bits(1);
        let pos = [1.0, 2.0, 3.0];

        let events = vec![
            EntityInputEvent::PointerEnter {
                entity_bits: entity.to_bits(),
                position: pos,
                pointer_id: 0,
            },
            EntityInputEvent::PointerExit {
                entity_bits: entity.to_bits(),
                pointer_id: 0,
            },
            EntityInputEvent::PointerMove {
                entity_bits: entity.to_bits(),
                position: pos,
                delta: [1.0, 2.0],
                pointer_id: 0,
            },
            EntityInputEvent::PointerDown {
                entity_bits: entity.to_bits(),
                position: pos,
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::PointerUp {
                entity_bits: entity.to_bits(),
                position: pos,
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::Click {
                entity_bits: entity.to_bits(),
                position: pos,
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::DoubleClick {
                entity_bits: entity.to_bits(),
                position: pos,
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::DragStart {
                entity_bits: entity.to_bits(),
                position: pos,
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::Drag {
                entity_bits: entity.to_bits(),
                position: pos,
                delta: [10.0, 20.0],
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::DragEnd {
                entity_bits: entity.to_bits(),
                position: pos,
                button: PointerButton::Primary,
                pointer_id: 0,
            },
            EntityInputEvent::Scroll {
                entity_bits: entity.to_bits(),
                position: pos,
                delta: [0.0, 1.0],
            },
        ];

        for event in events {
            let json = serde_json::to_string(&event).unwrap();
            let restored: EntityInputEvent = serde_json::from_str(&json).unwrap();
            assert_eq!(event.entity_bits(), restored.entity_bits());
        }
    }
}
