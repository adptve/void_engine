//! Entity Input Event Processor
//!
//! Converts raw pointer events into entity-level input events.

use alloc::collections::BTreeSet;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use void_ecs::{Entity, EntityInputEvent, InputEventKind, InputHandler, PointerButton};
use void_math::{Mat4, Ray, Vec3};

use crate::picking::screen_to_ray;

use super::state::{DragState, InputStateSnapshot, InputSystemState};

/// Input processing mode for graceful degradation
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum InputProcessingMode {
    /// Full processing with all features
    #[default]
    Full,
    /// Skip drag detection
    NoDrag,
    /// Skip bubbling
    NoBubbling,
    /// Only basic click detection
    BasicOnly,
    /// Disabled
    Disabled,
}

/// Processes raw input into entity events
#[derive(Debug)]
pub struct EntityInputProcessor {
    /// Input state tracking
    pub state: InputSystemState,
    /// Queued events for this frame
    pub event_queue: Vec<EntityInputEvent>,
    /// Processing mode
    mode: InputProcessingMode,
    /// Error counter for degradation
    error_count: u32,
    /// Frame counter for degradation tracking
    frame_count: u32,
}

impl Default for EntityInputProcessor {
    fn default() -> Self {
        Self::new()
    }
}

impl EntityInputProcessor {
    /// Create a new input processor
    pub fn new() -> Self {
        Self {
            state: InputSystemState::new(),
            event_queue: Vec::new(),
            mode: InputProcessingMode::Full,
            error_count: 0,
            frame_count: 0,
        }
    }

    /// Get current processing mode
    pub fn mode(&self) -> InputProcessingMode {
        self.mode
    }

    /// Set processing mode
    pub fn set_mode(&mut self, mode: InputProcessingMode) {
        self.mode = mode;
    }

    /// Process pointer move event
    pub fn on_pointer_move(
        &mut self,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        pointer_id: u32,
        view_matrix: &Mat4,
        projection_matrix: &Mat4,
        hit_entity: Option<(Entity, [f32; 3])>,
        entity_handlers: &dyn Fn(Entity) -> Option<InputHandler>,
        get_parent: &dyn Fn(Entity) -> Option<Entity>,
    ) {
        if self.mode == InputProcessingMode::Disabled {
            return;
        }

        // Check if input is captured
        if let Some(captured) = self.state.captured_entity() {
            if self.state.capture_pointer() == Some(pointer_id) {
                // Send move to captured entity
                if let Some((_, pos)) = hit_entity {
                    self.queue_event(EntityInputEvent::PointerMove {
                        entity_bits: captured.to_bits(),
                        position: pos,
                        delta: [0.0, 0.0],
                        pointer_id,
                    });
                }
                self.update_drag(screen_pos, hit_entity, pointer_id);
                return;
            }
        }

        // Get previous hovered entities
        let prev_hovered: BTreeSet<u64> = self
            .state
            .hovered(pointer_id)
            .cloned()
            .unwrap_or_default();

        // Determine new hovered entities
        let mut new_hovered = BTreeSet::new();

        if let Some((entity, position)) = hit_entity {
            new_hovered.insert(entity.to_bits());

            // Collect bubble targets
            if self.mode != InputProcessingMode::NoBubbling {
                self.collect_bubble_targets(entity, &mut new_hovered, entity_handlers, get_parent);
            }

            // Enter events
            for &bits in &new_hovered {
                if !prev_hovered.contains(&bits) {
                    let e = Entity::from_bits(bits);
                    if let Some(handler) = entity_handlers(e) {
                        if handler.listens_for(InputEventKind::PointerEnter) {
                            self.queue_event(EntityInputEvent::PointerEnter {
                                entity_bits: bits,
                                position,
                                pointer_id,
                            });
                        }
                    }
                }
            }

            // Move event
            if let Some(handler) = entity_handlers(entity) {
                if handler.listens_for(InputEventKind::PointerMove) {
                    self.queue_event(EntityInputEvent::PointerMove {
                        entity_bits: entity.to_bits(),
                        position,
                        delta: [0.0, 0.0],
                        pointer_id,
                    });
                }
            }
        }

        // Exit events
        for &bits in &prev_hovered {
            if !new_hovered.contains(&bits) {
                let e = Entity::from_bits(bits);
                if let Some(handler) = entity_handlers(e) {
                    if handler.listens_for(InputEventKind::PointerExit) {
                        self.queue_event(EntityInputEvent::PointerExit {
                            entity_bits: bits,
                            pointer_id,
                        });
                    }
                }
            }
        }

        // Update hover state
        self.state.hovered.insert(pointer_id, new_hovered);

        // Update drag state
        self.update_drag(screen_pos, hit_entity, pointer_id);
    }

    /// Process pointer down event
    pub fn on_pointer_down(
        &mut self,
        screen_pos: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
        hit_entity: Option<(Entity, [f32; 3])>,
        entity_handlers: &dyn Fn(Entity) -> Option<InputHandler>,
        get_parent: &dyn Fn(Entity) -> Option<Entity>,
    ) {
        if self.mode == InputProcessingMode::Disabled {
            return;
        }

        if let Some((entity, position)) = hit_entity {
            if let Some(handler) = entity_handlers(entity) {
                if !handler.enabled {
                    return;
                }

                // Queue pointer down event
                if handler.listens_for(InputEventKind::PointerDown) {
                    self.queue_event(EntityInputEvent::PointerDown {
                        entity_bits: entity.to_bits(),
                        position,
                        button,
                        pointer_id,
                    });
                }

                // Track pressed entity
                self.state.set_pressed(pointer_id, button, entity);

                // Start potential drag
                if self.mode != InputProcessingMode::NoDrag
                    && self.mode != InputProcessingMode::BasicOnly
                {
                    if handler.listens_for(InputEventKind::Drag) {
                        let drag = DragState::new(entity, button, screen_pos, position);
                        self.state.start_potential_drag(pointer_id, drag);
                    }
                }

                // Capture if requested
                if handler.capture_on_press {
                    self.state.capture(entity, pointer_id);
                }

                // Bubble to parents
                if handler.bubble_up && self.mode != InputProcessingMode::NoBubbling {
                    self.bubble_pointer_down(entity, position, button, pointer_id, entity_handlers, get_parent);
                }
            }
        }
    }

    /// Process pointer up event
    pub fn on_pointer_up(
        &mut self,
        screen_pos: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
        time: f64,
        hit_entity: Option<(Entity, [f32; 3])>,
        entity_handlers: &dyn Fn(Entity) -> Option<InputHandler>,
    ) {
        if self.mode == InputProcessingMode::Disabled {
            return;
        }

        // Get pressed entity
        let pressed = self.state.take_pressed(pointer_id, button);

        if let Some((entity, position)) = hit_entity {
            if let Some(handler) = entity_handlers(entity) {
                // Queue pointer up event
                if handler.listens_for(InputEventKind::PointerUp) {
                    self.queue_event(EntityInputEvent::PointerUp {
                        entity_bits: entity.to_bits(),
                        position,
                        button,
                        pointer_id,
                    });
                }

                // Check for click
                if pressed == Some(entity) {
                    // Check for double-click
                    if self.state.is_double_click(pointer_id, button, time, entity) {
                        if handler.listens_for(InputEventKind::DoubleClick) {
                            self.queue_event(EntityInputEvent::DoubleClick {
                                entity_bits: entity.to_bits(),
                                position,
                                button,
                                pointer_id,
                            });
                        }
                    } else {
                        if handler.listens_for(InputEventKind::Click) {
                            self.queue_event(EntityInputEvent::Click {
                                entity_bits: entity.to_bits(),
                                position,
                                button,
                                pointer_id,
                            });
                        }
                        self.state.record_click(pointer_id, button, time, entity);
                    }
                }
            }
        }

        // End drag
        if let Some(drag) = self.state.take_drag(pointer_id) {
            if drag.dragging {
                let position = hit_entity.map(|(_, p)| p).unwrap_or(drag.start_world);
                self.queue_event(EntityInputEvent::DragEnd {
                    entity_bits: drag.entity_bits,
                    position,
                    button: drag.pointer_button(),
                    pointer_id,
                });
            }
        }

        // Release capture if this pointer had it
        if self.state.capture_pointer() == Some(pointer_id) {
            self.state.release_capture();
        }
    }

    /// Process scroll event
    pub fn on_scroll(
        &mut self,
        delta: [f32; 2],
        hit_entity: Option<(Entity, [f32; 3])>,
        entity_handlers: &dyn Fn(Entity) -> Option<InputHandler>,
    ) {
        if self.mode == InputProcessingMode::Disabled {
            return;
        }

        if let Some((entity, position)) = hit_entity {
            if let Some(handler) = entity_handlers(entity) {
                if handler.listens_for(InputEventKind::Scroll) {
                    self.queue_event(EntityInputEvent::Scroll {
                        entity_bits: entity.to_bits(),
                        position,
                        delta,
                    });
                }
            }
        }
    }

    /// Update drag state based on current pointer position
    fn update_drag(
        &mut self,
        screen_pos: [f32; 2],
        hit_entity: Option<(Entity, [f32; 3])>,
        pointer_id: u32,
    ) {
        if self.mode == InputProcessingMode::NoDrag || self.mode == InputProcessingMode::BasicOnly {
            return;
        }

        // First check if drag exists and gather needed data
        let drag_info = if let Some(drag) = self.state.get_drag(pointer_id) {
            let delta = [
                screen_pos[0] - drag.start_screen[0],
                screen_pos[1] - drag.start_screen[1],
            ];
            let dist = (delta[0] * delta[0] + delta[1] * delta[1]).sqrt();
            Some((
                drag.entity_bits,
                drag.start_world,
                drag.pointer_button(),
                drag.dragging,
                delta,
                dist,
            ))
        } else {
            None
        };

        let threshold = self.state.drag_threshold;

        if let Some((entity_bits, start_world, button, was_dragging, delta, dist)) = drag_info {
            // Check if drag threshold reached
            if !was_dragging && dist > threshold {
                // Mark as dragging
                if let Some(drag) = self.state.get_drag_mut(pointer_id) {
                    drag.dragging = true;
                }
                self.queue_event(EntityInputEvent::DragStart {
                    entity_bits,
                    position: start_world,
                    button,
                    pointer_id,
                });
            }

            // Send drag event if dragging (either was or just started)
            let is_dragging = was_dragging || dist > threshold;
            if is_dragging {
                let position = hit_entity.map(|(_, p)| p).unwrap_or(start_world);
                self.queue_event(EntityInputEvent::Drag {
                    entity_bits,
                    position,
                    delta,
                    button,
                    pointer_id,
                });
            }
        }
    }

    /// Collect entities that should receive bubbled events
    fn collect_bubble_targets(
        &self,
        entity: Entity,
        set: &mut BTreeSet<u64>,
        entity_handlers: &dyn Fn(Entity) -> Option<InputHandler>,
        get_parent: &dyn Fn(Entity) -> Option<Entity>,
    ) {
        let mut current = entity;
        while let Some(parent) = get_parent(current) {
            current = parent;
            set.insert(current.to_bits());

            if let Some(handler) = entity_handlers(current) {
                if !handler.bubble_up {
                    break;
                }
            }
        }
    }

    /// Bubble pointer down event to parents
    fn bubble_pointer_down(
        &mut self,
        entity: Entity,
        position: [f32; 3],
        button: PointerButton,
        pointer_id: u32,
        entity_handlers: &dyn Fn(Entity) -> Option<InputHandler>,
        get_parent: &dyn Fn(Entity) -> Option<Entity>,
    ) {
        let mut current = entity;
        while let Some(parent) = get_parent(current) {
            current = parent;

            if let Some(handler) = entity_handlers(current) {
                if handler.enabled && handler.listens_for(InputEventKind::PointerDown) {
                    self.queue_event(EntityInputEvent::PointerDown {
                        entity_bits: current.to_bits(),
                        position,
                        button,
                        pointer_id,
                    });
                }

                if !handler.bubble_up {
                    break;
                }
            }
        }
    }

    /// Queue an event
    fn queue_event(&mut self, event: EntityInputEvent) {
        self.event_queue.push(event);
    }

    /// Drain queued events
    pub fn drain_events(&mut self) -> impl Iterator<Item = EntityInputEvent> + '_ {
        self.event_queue.drain(..)
    }

    /// Get pending event count
    pub fn pending_event_count(&self) -> usize {
        self.event_queue.len()
    }

    /// Clear all pending events
    pub fn clear_events(&mut self) {
        self.event_queue.clear();
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) {
        self.frame_count = self.frame_count.wrapping_add(1);
    }

    /// Record an error for degradation tracking
    pub fn record_error(&mut self) {
        self.error_count = self.error_count.saturating_add(1);
    }

    /// Check health and degrade if needed
    pub fn check_health_and_degrade(&mut self) {
        const ERROR_THRESHOLD: f32 = 0.1;

        if self.frame_count == 0 {
            return;
        }

        let error_rate = self.error_count as f32 / self.frame_count as f32;

        if error_rate > ERROR_THRESHOLD {
            self.mode = match self.mode {
                InputProcessingMode::Full => InputProcessingMode::NoDrag,
                InputProcessingMode::NoDrag => InputProcessingMode::NoBubbling,
                InputProcessingMode::NoBubbling => InputProcessingMode::BasicOnly,
                InputProcessingMode::BasicOnly => InputProcessingMode::Disabled,
                InputProcessingMode::Disabled => InputProcessingMode::Disabled,
            };
        }
    }

    /// Try to restore full processing
    pub fn try_restore_full_processing(&mut self) {
        self.mode = InputProcessingMode::Full;
        self.error_count = 0;
        self.frame_count = 0;
    }
}

/// Serializable snapshot for hot-reload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct EntityInputProcessorState {
    /// Input state snapshot
    pub input_state: InputStateSnapshot,
    /// Pending events
    pub pending_events: Vec<EntityInputEvent>,
    /// Processing mode
    pub mode: InputProcessingMode,
}

impl EntityInputProcessor {
    /// Save state for hot-reload
    pub fn save_state(&self) -> EntityInputProcessorState {
        EntityInputProcessorState {
            input_state: self.state.save_state(),
            pending_events: self.event_queue.clone(),
            mode: self.mode,
        }
    }

    /// Restore state from hot-reload snapshot
    pub fn restore_state(&mut self, state: EntityInputProcessorState) {
        self.state.restore_state(state.input_state);
        self.event_queue = state.pending_events;
        self.mode = state.mode;
        self.error_count = 0;
        self.frame_count = 0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use void_ecs::InputEventKind;

    fn make_clickable_handler() -> InputHandler {
        InputHandler::clickable()
    }

    fn make_draggable_handler() -> InputHandler {
        InputHandler::draggable()
    }

    #[test]
    fn test_processor_new() {
        let processor = EntityInputProcessor::new();
        assert_eq!(processor.mode(), InputProcessingMode::Full);
        assert_eq!(processor.pending_event_count(), 0);
    }

    #[test]
    fn test_processor_mode() {
        let mut processor = EntityInputProcessor::new();

        processor.set_mode(InputProcessingMode::NoDrag);
        assert_eq!(processor.mode(), InputProcessingMode::NoDrag);
    }

    #[test]
    fn test_processor_pointer_down() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        let handler = make_clickable_handler();
        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        assert!(processor.pending_event_count() > 0);

        let events: Vec<_> = processor.drain_events().collect();
        assert!(events.iter().any(|e| matches!(e, EntityInputEvent::PointerDown { .. })));
    }

    #[test]
    fn test_processor_click_detection() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        let handler = make_clickable_handler();
        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        // Press
        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        // Release on same entity
        processor.on_pointer_up(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            0.1,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
        );

        let events: Vec<_> = processor.drain_events().collect();
        assert!(events.iter().any(|e| matches!(e, EntityInputEvent::Click { .. })));
    }

    #[test]
    fn test_processor_double_click() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        let handler = InputHandler::new()
            .with_event(InputEventKind::Click)
            .with_event(InputEventKind::DoubleClick)
            .with_event(InputEventKind::PointerDown)
            .with_event(InputEventKind::PointerUp);

        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        // First click
        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );
        processor.on_pointer_up(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            0.0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
        );

        processor.drain_events().for_each(drop);

        // Second click within threshold
        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );
        processor.on_pointer_up(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            0.2, // Within 0.3s threshold
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
        );

        let events: Vec<_> = processor.drain_events().collect();
        assert!(events.iter().any(|e| matches!(e, EntityInputEvent::DoubleClick { .. })));
    }

    #[test]
    fn test_processor_drag() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        let handler = make_draggable_handler();
        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        let view = Mat4::IDENTITY;
        let proj = Mat4::IDENTITY;

        // Press
        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        // Move beyond threshold
        processor.on_pointer_move(
            [110.0, 100.0], // 10px movement, beyond default 5px threshold
            [800.0, 600.0],
            0,
            &view,
            &proj,
            Some((entity, [1.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        let events: Vec<_> = processor.drain_events().collect();
        assert!(events.iter().any(|e| matches!(e, EntityInputEvent::DragStart { .. })));
    }

    #[test]
    fn test_processor_hover_enter_exit() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        let handler = InputHandler::hover_only();
        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        let view = Mat4::IDENTITY;
        let proj = Mat4::IDENTITY;

        // Enter
        processor.on_pointer_move(
            [100.0, 100.0],
            [800.0, 600.0],
            0,
            &view,
            &proj,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        let events: Vec<_> = processor.drain_events().collect();
        assert!(events.iter().any(|e| matches!(e, EntityInputEvent::PointerEnter { .. })));

        // Exit
        processor.on_pointer_move(
            [200.0, 200.0],
            [800.0, 600.0],
            0,
            &view,
            &proj,
            None, // No hit
            &handlers,
            &get_parent,
        );

        let events: Vec<_> = processor.drain_events().collect();
        assert!(events.iter().any(|e| matches!(e, EntityInputEvent::PointerExit { .. })));
    }

    #[test]
    fn test_processor_disabled_mode() {
        let mut processor = EntityInputProcessor::new();
        processor.set_mode(InputProcessingMode::Disabled);

        let entity = Entity::from_bits(42);
        let handler = make_clickable_handler();
        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        assert_eq!(processor.pending_event_count(), 0);
    }

    #[test]
    fn test_processor_capture() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        let handler = InputHandler::draggable(); // Has capture_on_press
        let handlers = |e: Entity| {
            if e == entity {
                Some(handler.clone())
            } else {
                None
            }
        };
        let get_parent = |_: Entity| None;

        processor.on_pointer_down(
            [100.0, 100.0],
            PointerButton::Primary,
            0,
            Some((entity, [0.0, 0.0, 0.0])),
            &handlers,
            &get_parent,
        );

        assert!(processor.state.is_captured());
        assert_eq!(processor.state.captured_entity(), Some(entity));
    }

    #[test]
    fn test_processor_save_restore() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_bits(42);

        processor.state.capture(entity, 0);
        processor.queue_event(EntityInputEvent::fallback_click(entity));

        let state = processor.save_state();
        assert_eq!(state.pending_events.len(), 1);

        let mut new_processor = EntityInputProcessor::new();
        new_processor.restore_state(state);

        assert_eq!(new_processor.state.captured_entity(), Some(entity));
        assert_eq!(new_processor.pending_event_count(), 1);
    }

    #[test]
    fn test_processor_degradation() {
        let mut processor = EntityInputProcessor::new();

        // Simulate high error rate
        processor.frame_count = 100;
        processor.error_count = 20; // 20% error rate

        processor.check_health_and_degrade();
        assert_eq!(processor.mode(), InputProcessingMode::NoDrag);

        // Continue degradation
        processor.frame_count = 200;
        processor.error_count = 40;
        processor.check_health_and_degrade();
        assert_eq!(processor.mode(), InputProcessingMode::NoBubbling);
    }

    #[test]
    fn test_processor_restore_full() {
        let mut processor = EntityInputProcessor::new();
        processor.set_mode(InputProcessingMode::Disabled);

        processor.try_restore_full_processing();
        assert_eq!(processor.mode(), InputProcessingMode::Full);
    }
}
