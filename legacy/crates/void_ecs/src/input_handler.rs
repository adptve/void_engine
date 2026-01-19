//! Input Handler Component
//!
//! Configures how an entity receives and processes input events.
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::input_handler::{InputHandler, InputEventKind, CursorIcon};
//!
//! // Create a clickable button
//! let handler = InputHandler::clickable();
//!
//! // Create a draggable object
//! let handler = InputHandler::draggable();
//!
//! // Custom configuration
//! let handler = InputHandler::new()
//!     .with_event(InputEventKind::Click)
//!     .with_event(InputEventKind::DoubleClick)
//!     .with_cursor(CursorIcon::Pointer)
//!     .blocking();
//! ```

use alloc::collections::BTreeSet;
use serde::{Deserialize, Serialize};

/// Types of input events that can be listened for
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub enum InputEventKind {
    /// Pointer entered entity bounds
    PointerEnter,
    /// Pointer exited entity bounds
    PointerExit,
    /// Pointer moved over entity
    PointerMove,
    /// Pointer button pressed
    PointerDown,
    /// Pointer button released
    PointerUp,
    /// Click (press + release on same entity)
    Click,
    /// Double click
    DoubleClick,
    /// Drag operation started
    DragStart,
    /// Drag in progress
    Drag,
    /// Drag operation ended
    DragEnd,
    /// Scroll wheel
    Scroll,
}

impl InputEventKind {
    /// Get all hover-related events
    pub fn hover_events() -> &'static [Self] {
        &[Self::PointerEnter, Self::PointerExit, Self::PointerMove]
    }

    /// Get all click-related events
    pub fn click_events() -> &'static [Self] {
        &[Self::PointerDown, Self::PointerUp, Self::Click, Self::DoubleClick]
    }

    /// Get all drag-related events
    pub fn drag_events() -> &'static [Self] {
        &[Self::DragStart, Self::Drag, Self::DragEnd]
    }
}

/// Cursor icon to display when hovering
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum CursorIcon {
    /// Default arrow cursor
    Default,
    /// Pointing hand (for clickable elements)
    Pointer,
    /// Crosshair (for precision selection)
    Crosshair,
    /// Move cursor (for draggable elements)
    Move,
    /// Text cursor (for text fields)
    Text,
    /// Wait/busy cursor
    Wait,
    /// Help cursor
    Help,
    /// Not allowed / disabled
    NotAllowed,
    /// Grab cursor (before dragging)
    Grab,
    /// Grabbing cursor (during drag)
    Grabbing,
    /// North-South resize
    ResizeNS,
    /// East-West resize
    ResizeEW,
    /// Northeast-Southwest resize
    ResizeNESW,
    /// Northwest-Southeast resize
    ResizeNWSE,
    /// Column resize
    ResizeCol,
    /// Row resize
    ResizeRow,
    /// Zoom in
    ZoomIn,
    /// Zoom out
    ZoomOut,
}

impl Default for CursorIcon {
    fn default() -> Self {
        Self::Default
    }
}

/// Configures how an entity receives input events
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct InputHandler {
    /// Which events this entity listens for
    pub events: BTreeSet<InputEventKind>,

    /// Block events from reaching entities behind this one
    pub blocks_raycast: bool,

    /// Allow events to bubble up to parent entities
    pub bubble_up: bool,

    /// Allow events to propagate down to children
    pub propagate_down: bool,

    /// Capture all pointer events while pressed
    pub capture_on_press: bool,

    /// Custom cursor when hovering
    pub hover_cursor: Option<CursorIcon>,

    /// Is handler active
    pub enabled: bool,

    /// Priority for event handling (higher = first)
    pub priority: i32,
}

impl Default for InputHandler {
    fn default() -> Self {
        Self {
            events: BTreeSet::new(),
            blocks_raycast: true,
            bubble_up: true,
            propagate_down: false,
            capture_on_press: false,
            hover_cursor: None,
            enabled: true,
            priority: 0,
        }
    }
}

impl InputHandler {
    /// Create a new empty input handler
    pub fn new() -> Self {
        Self::default()
    }

    /// Create a clickable handler (for buttons, links, etc.)
    pub fn clickable() -> Self {
        Self {
            events: [
                InputEventKind::PointerEnter,
                InputEventKind::PointerExit,
                InputEventKind::PointerDown,
                InputEventKind::PointerUp,
                InputEventKind::Click,
            ]
            .into_iter()
            .collect(),
            hover_cursor: Some(CursorIcon::Pointer),
            ..Default::default()
        }
    }

    /// Create a draggable handler (for moveable objects)
    pub fn draggable() -> Self {
        Self {
            events: [
                InputEventKind::PointerEnter,
                InputEventKind::PointerExit,
                InputEventKind::PointerDown,
                InputEventKind::PointerUp,
                InputEventKind::DragStart,
                InputEventKind::Drag,
                InputEventKind::DragEnd,
            ]
            .into_iter()
            .collect(),
            capture_on_press: true,
            hover_cursor: Some(CursorIcon::Grab),
            ..Default::default()
        }
    }

    /// Create a hover-only handler (for tooltips, highlights)
    pub fn hover_only() -> Self {
        Self {
            events: [
                InputEventKind::PointerEnter,
                InputEventKind::PointerExit,
                InputEventKind::PointerMove,
            ]
            .into_iter()
            .collect(),
            blocks_raycast: false,
            ..Default::default()
        }
    }

    /// Create a scrollable handler
    pub fn scrollable() -> Self {
        Self {
            events: [InputEventKind::Scroll].into_iter().collect(),
            ..Default::default()
        }
    }

    /// Create a fully interactive handler (all events)
    pub fn interactive() -> Self {
        Self {
            events: [
                InputEventKind::PointerEnter,
                InputEventKind::PointerExit,
                InputEventKind::PointerMove,
                InputEventKind::PointerDown,
                InputEventKind::PointerUp,
                InputEventKind::Click,
                InputEventKind::DoubleClick,
                InputEventKind::DragStart,
                InputEventKind::Drag,
                InputEventKind::DragEnd,
                InputEventKind::Scroll,
            ]
            .into_iter()
            .collect(),
            capture_on_press: true,
            hover_cursor: Some(CursorIcon::Pointer),
            ..Default::default()
        }
    }

    /// Add a single event type
    pub fn with_event(mut self, event: InputEventKind) -> Self {
        self.events.insert(event);
        self
    }

    /// Add multiple event types
    pub fn with_events(mut self, events: impl IntoIterator<Item = InputEventKind>) -> Self {
        self.events.extend(events);
        self
    }

    /// Set hover cursor
    pub fn with_cursor(mut self, cursor: CursorIcon) -> Self {
        self.hover_cursor = Some(cursor);
        self
    }

    /// Make handler block raycasts
    pub fn blocking(mut self) -> Self {
        self.blocks_raycast = true;
        self
    }

    /// Make handler not block raycasts
    pub fn non_blocking(mut self) -> Self {
        self.blocks_raycast = false;
        self
    }

    /// Enable event bubbling to parents
    pub fn with_bubble(mut self) -> Self {
        self.bubble_up = true;
        self
    }

    /// Disable event bubbling
    pub fn without_bubble(mut self) -> Self {
        self.bubble_up = false;
        self
    }

    /// Enable capture on press
    pub fn with_capture(mut self) -> Self {
        self.capture_on_press = true;
        self
    }

    /// Set priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Enable the handler
    pub fn enable(&mut self) {
        self.enabled = true;
    }

    /// Disable the handler
    pub fn disable(&mut self) {
        self.enabled = false;
    }

    /// Check if handler listens for a specific event
    pub fn listens_for(&self, event: InputEventKind) -> bool {
        self.enabled && self.events.contains(&event)
    }

    /// Check if handler is active and listens for any events
    pub fn is_active(&self) -> bool {
        self.enabled && !self.events.is_empty()
    }

    /// Get the cursor to display when hovering
    pub fn cursor(&self) -> CursorIcon {
        self.hover_cursor.unwrap_or(CursorIcon::Default)
    }
}

/// State component tracking current input state for an entity
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct InputState {
    /// Is currently hovered
    pub hovered: bool,
    /// Is currently pressed
    pub pressed: bool,
    /// Is currently being dragged
    pub dragging: bool,
    /// Has input capture
    pub captured: bool,
    /// Current hover position (if hovered)
    pub hover_position: Option<[f32; 3]>,
}

impl InputState {
    /// Create new default input state
    pub fn new() -> Self {
        Self::default()
    }

    /// Check if entity is in any active state
    pub fn is_active(&self) -> bool {
        self.hovered || self.pressed || self.dragging
    }

    /// Reset all state flags
    pub fn reset(&mut self) {
        self.hovered = false;
        self.pressed = false;
        self.dragging = false;
        self.captured = false;
        self.hover_position = None;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_input_handler_default() {
        let handler = InputHandler::default();
        assert!(handler.events.is_empty());
        assert!(handler.enabled);
        assert!(handler.blocks_raycast);
        assert!(handler.bubble_up);
    }

    #[test]
    fn test_input_handler_clickable() {
        let handler = InputHandler::clickable();
        assert!(handler.events.contains(&InputEventKind::Click));
        assert!(handler.events.contains(&InputEventKind::PointerEnter));
        assert!(handler.events.contains(&InputEventKind::PointerDown));
        assert_eq!(handler.hover_cursor, Some(CursorIcon::Pointer));
    }

    #[test]
    fn test_input_handler_draggable() {
        let handler = InputHandler::draggable();
        assert!(handler.events.contains(&InputEventKind::DragStart));
        assert!(handler.events.contains(&InputEventKind::Drag));
        assert!(handler.events.contains(&InputEventKind::DragEnd));
        assert!(handler.capture_on_press);
        assert_eq!(handler.hover_cursor, Some(CursorIcon::Grab));
    }

    #[test]
    fn test_input_handler_hover_only() {
        let handler = InputHandler::hover_only();
        assert!(handler.events.contains(&InputEventKind::PointerEnter));
        assert!(handler.events.contains(&InputEventKind::PointerExit));
        assert!(handler.events.contains(&InputEventKind::PointerMove));
        assert!(!handler.blocks_raycast);
    }

    #[test]
    fn test_input_handler_builder() {
        let handler = InputHandler::new()
            .with_event(InputEventKind::Click)
            .with_event(InputEventKind::DoubleClick)
            .with_cursor(CursorIcon::Pointer)
            .with_priority(10)
            .blocking();

        assert!(handler.events.contains(&InputEventKind::Click));
        assert!(handler.events.contains(&InputEventKind::DoubleClick));
        assert_eq!(handler.hover_cursor, Some(CursorIcon::Pointer));
        assert_eq!(handler.priority, 10);
        assert!(handler.blocks_raycast);
    }

    #[test]
    fn test_input_handler_listens_for() {
        let handler = InputHandler::clickable();
        assert!(handler.listens_for(InputEventKind::Click));
        assert!(!handler.listens_for(InputEventKind::Scroll));
    }

    #[test]
    fn test_input_handler_disabled() {
        let mut handler = InputHandler::clickable();
        handler.disable();
        assert!(!handler.listens_for(InputEventKind::Click));
        assert!(!handler.is_active());
    }

    #[test]
    fn test_input_state_default() {
        let state = InputState::new();
        assert!(!state.hovered);
        assert!(!state.pressed);
        assert!(!state.dragging);
        assert!(!state.is_active());
    }

    #[test]
    fn test_input_state_reset() {
        let mut state = InputState {
            hovered: true,
            pressed: true,
            dragging: true,
            captured: true,
            hover_position: Some([1.0, 2.0, 3.0]),
        };

        state.reset();
        assert!(!state.hovered);
        assert!(!state.pressed);
        assert!(!state.dragging);
        assert!(state.hover_position.is_none());
    }

    #[test]
    fn test_input_handler_serialization() {
        let handler = InputHandler {
            events: [InputEventKind::Click, InputEventKind::Drag]
                .into_iter()
                .collect(),
            blocks_raycast: true,
            bubble_up: true,
            propagate_down: false,
            capture_on_press: true,
            hover_cursor: Some(CursorIcon::Pointer),
            enabled: true,
            priority: 5,
        };

        let json = serde_json::to_string(&handler).unwrap();
        let restored: InputHandler = serde_json::from_str(&json).unwrap();

        assert_eq!(handler.events, restored.events);
        assert_eq!(handler.blocks_raycast, restored.blocks_raycast);
        assert_eq!(handler.capture_on_press, restored.capture_on_press);
        assert_eq!(handler.priority, restored.priority);
    }

    #[test]
    fn test_cursor_icon_all_variants() {
        let icons = [
            CursorIcon::Default,
            CursorIcon::Pointer,
            CursorIcon::Crosshair,
            CursorIcon::Move,
            CursorIcon::Text,
            CursorIcon::Wait,
            CursorIcon::Help,
            CursorIcon::NotAllowed,
            CursorIcon::Grab,
            CursorIcon::Grabbing,
            CursorIcon::ResizeNS,
            CursorIcon::ResizeEW,
            CursorIcon::ResizeNESW,
            CursorIcon::ResizeNWSE,
            CursorIcon::ResizeCol,
            CursorIcon::ResizeRow,
            CursorIcon::ZoomIn,
            CursorIcon::ZoomOut,
        ];

        for icon in icons {
            let json = serde_json::to_string(&icon).unwrap();
            let restored: CursorIcon = serde_json::from_str(&json).unwrap();
            assert_eq!(icon, restored);
        }
    }

    #[test]
    fn test_input_event_kind_groups() {
        let hover = InputEventKind::hover_events();
        assert!(hover.contains(&InputEventKind::PointerEnter));
        assert!(hover.contains(&InputEventKind::PointerExit));

        let click = InputEventKind::click_events();
        assert!(click.contains(&InputEventKind::Click));
        assert!(click.contains(&InputEventKind::DoubleClick));

        let drag = InputEventKind::drag_events();
        assert!(drag.contains(&InputEventKind::DragStart));
        assert!(drag.contains(&InputEventKind::DragEnd));
    }

    #[test]
    fn test_input_state_serialization() {
        let state = InputState {
            hovered: true,
            pressed: false,
            dragging: true,
            captured: false,
            hover_position: Some([1.0, 2.0, 3.0]),
        };

        let json = serde_json::to_string(&state).unwrap();
        let restored: InputState = serde_json::from_str(&json).unwrap();

        assert_eq!(state.hovered, restored.hovered);
        assert_eq!(state.dragging, restored.dragging);
        assert_eq!(state.hover_position, restored.hover_position);
    }
}
