//! Input State Tracking
//!
//! Tracks pointer, hover, drag, and capture state across frames.

use alloc::collections::BTreeMap;
use alloc::collections::BTreeSet;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use void_ecs::{Entity, PointerButton};

/// Tracks input state across frames
#[derive(Clone, Debug, Default)]
pub struct InputSystemState {
    /// Currently hovered entities per pointer
    pub hovered: BTreeMap<u32, BTreeSet<u64>>,

    /// Currently pressed entities per (pointer, button)
    pub pressed: BTreeMap<(u32, u8), u64>,

    /// Entity capturing all input (entity bits, pointer_id)
    pub captured: Option<(u64, u32)>,

    /// Last click time for double-click detection (pointer, button) -> (time, entity)
    pub last_click: BTreeMap<(u32, u8), (f64, u64)>,

    /// Drag state per pointer
    pub drag_state: BTreeMap<u32, DragState>,

    /// Double-click threshold in seconds
    pub double_click_threshold: f64,

    /// Drag start threshold in pixels
    pub drag_threshold: f32,
}

/// State of an active drag operation
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DragState {
    /// Entity being dragged (as bits)
    pub entity_bits: u64,
    /// Button used for drag
    pub button: u8,
    /// Screen position where drag started
    pub start_screen: [f32; 2],
    /// World position where drag started
    pub start_world: [f32; 3],
    /// Whether drag has actually started (passed threshold)
    pub dragging: bool,
}

impl DragState {
    /// Create a new drag state
    pub fn new(
        entity: Entity,
        button: PointerButton,
        start_screen: [f32; 2],
        start_world: [f32; 3],
    ) -> Self {
        Self {
            entity_bits: entity.to_bits(),
            button: pointer_button_to_u8(button),
            start_screen,
            start_world,
            dragging: false,
        }
    }

    /// Get the entity being dragged
    pub fn entity(&self) -> Entity {
        Entity::from_bits(self.entity_bits)
    }

    /// Get the button used for drag
    pub fn pointer_button(&self) -> PointerButton {
        u8_to_pointer_button(self.button)
    }
}

impl InputSystemState {
    /// Create a new input state with default settings
    pub fn new() -> Self {
        Self {
            hovered: BTreeMap::new(),
            pressed: BTreeMap::new(),
            captured: None,
            last_click: BTreeMap::new(),
            drag_state: BTreeMap::new(),
            double_click_threshold: 0.3,
            drag_threshold: 5.0,
        }
    }

    /// Get currently hovered entities for a pointer
    pub fn hovered(&self, pointer_id: u32) -> Option<&BTreeSet<u64>> {
        self.hovered.get(&pointer_id)
    }

    /// Check if an entity is hovered by any pointer
    pub fn is_entity_hovered(&self, entity: Entity) -> bool {
        let bits = entity.to_bits();
        self.hovered.values().any(|set| set.contains(&bits))
    }

    /// Get entity capturing input (if any)
    pub fn captured_entity(&self) -> Option<Entity> {
        self.captured.map(|(bits, _)| Entity::from_bits(bits))
    }

    /// Get the pointer ID that has capture
    pub fn capture_pointer(&self) -> Option<u32> {
        self.captured.map(|(_, p)| p)
    }

    /// Capture input for an entity
    pub fn capture(&mut self, entity: Entity, pointer_id: u32) {
        self.captured = Some((entity.to_bits(), pointer_id));
    }

    /// Release input capture
    pub fn release_capture(&mut self) {
        self.captured = None;
    }

    /// Check if input is captured
    pub fn is_captured(&self) -> bool {
        self.captured.is_some()
    }

    /// Set hovered entities for a pointer
    pub fn set_hovered(&mut self, pointer_id: u32, entities: impl IntoIterator<Item = Entity>) {
        let set: BTreeSet<u64> = entities.into_iter().map(|e| e.to_bits()).collect();
        self.hovered.insert(pointer_id, set);
    }

    /// Clear hover state for a pointer
    pub fn clear_hovered(&mut self, pointer_id: u32) {
        self.hovered.remove(&pointer_id);
    }

    /// Set pressed entity for a pointer/button combo
    pub fn set_pressed(&mut self, pointer_id: u32, button: PointerButton, entity: Entity) {
        let key = (pointer_id, pointer_button_to_u8(button));
        self.pressed.insert(key, entity.to_bits());
    }

    /// Get and remove pressed entity for a pointer/button combo
    pub fn take_pressed(&mut self, pointer_id: u32, button: PointerButton) -> Option<Entity> {
        let key = (pointer_id, pointer_button_to_u8(button));
        self.pressed.remove(&key).map(Entity::from_bits)
    }

    /// Get pressed entity without removing
    pub fn get_pressed(&self, pointer_id: u32, button: PointerButton) -> Option<Entity> {
        let key = (pointer_id, pointer_button_to_u8(button));
        self.pressed.get(&key).map(|&bits| Entity::from_bits(bits))
    }

    /// Start tracking a potential drag
    pub fn start_potential_drag(&mut self, pointer_id: u32, state: DragState) {
        self.drag_state.insert(pointer_id, state);
    }

    /// Get drag state for a pointer
    pub fn get_drag(&self, pointer_id: u32) -> Option<&DragState> {
        self.drag_state.get(&pointer_id)
    }

    /// Get mutable drag state for a pointer
    pub fn get_drag_mut(&mut self, pointer_id: u32) -> Option<&mut DragState> {
        self.drag_state.get_mut(&pointer_id)
    }

    /// Remove drag state for a pointer
    pub fn take_drag(&mut self, pointer_id: u32) -> Option<DragState> {
        self.drag_state.remove(&pointer_id)
    }

    /// Record a click for double-click detection
    pub fn record_click(&mut self, pointer_id: u32, button: PointerButton, time: f64, entity: Entity) {
        let key = (pointer_id, pointer_button_to_u8(button));
        self.last_click.insert(key, (time, entity.to_bits()));
    }

    /// Check if this click is a double-click
    pub fn is_double_click(
        &mut self,
        pointer_id: u32,
        button: PointerButton,
        time: f64,
        entity: Entity,
    ) -> bool {
        let key = (pointer_id, pointer_button_to_u8(button));
        if let Some((last_time, last_entity)) = self.last_click.get(&key) {
            let is_double = *last_entity == entity.to_bits()
                && (time - last_time) < self.double_click_threshold;
            if is_double {
                self.last_click.remove(&key);
            }
            is_double
        } else {
            false
        }
    }

    /// Reset all state
    pub fn reset(&mut self) {
        self.hovered.clear();
        self.pressed.clear();
        self.captured = None;
        self.last_click.clear();
        self.drag_state.clear();
    }
}

/// Serializable snapshot of input state for hot-reload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct InputStateSnapshot {
    /// Currently hovered entities per pointer
    pub hovered: Vec<(u32, Vec<u64>)>,
    /// Currently pressed entities (pointer_id, button, entity_bits)
    pub pressed: Vec<(u32, u8, u64)>,
    /// Entity currently capturing input
    pub captured: Option<(u64, u32)>,
    /// Double-click threshold
    pub double_click_threshold: f64,
    /// Drag threshold in pixels
    pub drag_threshold: f32,
}

impl InputSystemState {
    /// Save state for hot-reload
    pub fn save_state(&self) -> InputStateSnapshot {
        InputStateSnapshot {
            hovered: self
                .hovered
                .iter()
                .map(|(&k, v)| (k, v.iter().copied().collect()))
                .collect(),
            pressed: self
                .pressed
                .iter()
                .map(|(&(p, b), &e)| (p, b, e))
                .collect(),
            captured: self.captured,
            double_click_threshold: self.double_click_threshold,
            drag_threshold: self.drag_threshold,
        }
    }

    /// Restore state from hot-reload snapshot
    pub fn restore_state(&mut self, state: InputStateSnapshot) {
        self.hovered = state
            .hovered
            .into_iter()
            .map(|(k, v)| (k, v.into_iter().collect()))
            .collect();
        self.pressed = state.pressed.into_iter().map(|(p, b, e)| ((p, b), e)).collect();
        self.captured = state.captured;
        self.double_click_threshold = state.double_click_threshold;
        self.drag_threshold = state.drag_threshold;
        // Clear transient state that shouldn't persist
        self.last_click.clear();
        self.drag_state.clear();
    }
}

/// Convert PointerButton to u8 for storage
fn pointer_button_to_u8(button: PointerButton) -> u8 {
    match button {
        PointerButton::Primary => 0,
        PointerButton::Secondary => 1,
        PointerButton::Middle => 2,
        PointerButton::Extra1 => 3,
        PointerButton::Extra2 => 4,
    }
}

/// Convert u8 back to PointerButton
fn u8_to_pointer_button(value: u8) -> PointerButton {
    match value {
        0 => PointerButton::Primary,
        1 => PointerButton::Secondary,
        2 => PointerButton::Middle,
        3 => PointerButton::Extra1,
        4 => PointerButton::Extra2,
        _ => PointerButton::Primary,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_input_state_new() {
        let state = InputSystemState::new();
        assert!(state.hovered.is_empty());
        assert!(state.pressed.is_empty());
        assert!(!state.is_captured());
    }

    #[test]
    fn test_input_state_capture() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        state.capture(entity, 0);
        assert!(state.is_captured());
        assert_eq!(state.captured_entity(), Some(entity));
        assert_eq!(state.capture_pointer(), Some(0));

        state.release_capture();
        assert!(!state.is_captured());
    }

    #[test]
    fn test_input_state_hovered() {
        let mut state = InputSystemState::new();
        let entity1 = Entity::from_bits(1);
        let entity2 = Entity::from_bits(2);

        state.set_hovered(0, [entity1, entity2]);

        assert!(state.is_entity_hovered(entity1));
        assert!(state.is_entity_hovered(entity2));
        assert!(!state.is_entity_hovered(Entity::from_bits(3)));

        let hovered = state.hovered(0).unwrap();
        assert_eq!(hovered.len(), 2);

        state.clear_hovered(0);
        assert!(state.hovered(0).is_none());
    }

    #[test]
    fn test_input_state_pressed() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        state.set_pressed(0, PointerButton::Primary, entity);
        assert_eq!(state.get_pressed(0, PointerButton::Primary), Some(entity));

        let taken = state.take_pressed(0, PointerButton::Primary);
        assert_eq!(taken, Some(entity));
        assert!(state.get_pressed(0, PointerButton::Primary).is_none());
    }

    #[test]
    fn test_input_state_drag() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        let drag = DragState::new(entity, PointerButton::Primary, [100.0, 100.0], [0.0, 0.0, 0.0]);
        state.start_potential_drag(0, drag);

        assert!(state.get_drag(0).is_some());
        assert_eq!(state.get_drag(0).unwrap().entity(), entity);

        let taken = state.take_drag(0);
        assert!(taken.is_some());
        assert!(state.get_drag(0).is_none());
    }

    #[test]
    fn test_input_state_double_click() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        // First click
        state.record_click(0, PointerButton::Primary, 0.0, entity);

        // Second click within threshold
        assert!(state.is_double_click(0, PointerButton::Primary, 0.2, entity));

        // Third click - should not be double click (record was cleared)
        assert!(!state.is_double_click(0, PointerButton::Primary, 0.3, entity));
    }

    #[test]
    fn test_input_state_double_click_timeout() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        state.record_click(0, PointerButton::Primary, 0.0, entity);

        // Click after threshold
        assert!(!state.is_double_click(0, PointerButton::Primary, 0.5, entity));
    }

    #[test]
    fn test_input_state_snapshot() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        state.set_hovered(0, [entity]);
        state.set_pressed(0, PointerButton::Primary, entity);
        state.capture(entity, 0);

        let snapshot = state.save_state();
        assert_eq!(snapshot.hovered.len(), 1);
        assert_eq!(snapshot.pressed.len(), 1);
        assert!(snapshot.captured.is_some());

        // Restore to a new state
        let mut new_state = InputSystemState::new();
        new_state.restore_state(snapshot);

        assert!(new_state.is_entity_hovered(entity));
        assert_eq!(new_state.get_pressed(0, PointerButton::Primary), Some(entity));
        assert_eq!(new_state.captured_entity(), Some(entity));
    }

    #[test]
    fn test_drag_state() {
        let entity = Entity::from_bits(42);
        let drag = DragState::new(entity, PointerButton::Secondary, [50.0, 50.0], [1.0, 2.0, 3.0]);

        assert_eq!(drag.entity(), entity);
        assert_eq!(drag.pointer_button(), PointerButton::Secondary);
        assert_eq!(drag.start_screen, [50.0, 50.0]);
        assert_eq!(drag.start_world, [1.0, 2.0, 3.0]);
        assert!(!drag.dragging);
    }

    #[test]
    fn test_pointer_button_conversion() {
        let buttons = [
            PointerButton::Primary,
            PointerButton::Secondary,
            PointerButton::Middle,
            PointerButton::Extra1,
            PointerButton::Extra2,
        ];

        for button in buttons {
            let u8_val = pointer_button_to_u8(button);
            let restored = u8_to_pointer_button(u8_val);
            assert_eq!(button, restored);
        }
    }

    #[test]
    fn test_input_state_reset() {
        let mut state = InputSystemState::new();
        let entity = Entity::from_bits(42);

        state.set_hovered(0, [entity]);
        state.set_pressed(0, PointerButton::Primary, entity);
        state.capture(entity, 0);

        state.reset();

        assert!(state.hovered.is_empty());
        assert!(state.pressed.is_empty());
        assert!(!state.is_captured());
    }
}
