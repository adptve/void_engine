# Phase 11: Entity-Level Input Events

## Status: Not Started

## User Story

> As an application author, I want entities to respond directly to user input.

## Requirements Checklist

- [ ] Define per-entity input handlers: Hover, Click, Press, Release
- [ ] Support mouse and touch input
- [ ] Allow event bubbling through parent hierarchy
- [ ] Allow exclusive input capture

## Implementation Specification

### 1. Input Event Types

```rust
// crates/void_ecs/src/components/input_events.rs (NEW FILE)

use void_ecs::Entity;
use void_math::Vec3;

/// Input event targeting an entity
#[derive(Clone, Debug)]
pub enum EntityInputEvent {
    /// Pointer entered entity bounds
    PointerEnter {
        entity: Entity,
        position: Vec3,
        pointer_id: u32,
    },

    /// Pointer exited entity bounds
    PointerExit {
        entity: Entity,
        pointer_id: u32,
    },

    /// Pointer moved over entity
    PointerMove {
        entity: Entity,
        position: Vec3,
        delta: [f32; 2],
        pointer_id: u32,
    },

    /// Pointer button pressed on entity
    PointerDown {
        entity: Entity,
        position: Vec3,
        button: PointerButton,
        pointer_id: u32,
    },

    /// Pointer button released on entity
    PointerUp {
        entity: Entity,
        position: Vec3,
        button: PointerButton,
        pointer_id: u32,
    },

    /// Click (down + up on same entity)
    Click {
        entity: Entity,
        position: Vec3,
        button: PointerButton,
        pointer_id: u32,
    },

    /// Double click
    DoubleClick {
        entity: Entity,
        position: Vec3,
        button: PointerButton,
        pointer_id: u32,
    },

    /// Drag started
    DragStart {
        entity: Entity,
        position: Vec3,
        button: PointerButton,
        pointer_id: u32,
    },

    /// Drag in progress
    Drag {
        entity: Entity,
        position: Vec3,
        delta: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
    },

    /// Drag ended
    DragEnd {
        entity: Entity,
        position: Vec3,
        button: PointerButton,
        pointer_id: u32,
    },

    /// Scroll wheel over entity
    Scroll {
        entity: Entity,
        position: Vec3,
        delta: [f32; 2],
    },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PointerButton {
    Primary,    // Left mouse / touch
    Secondary,  // Right mouse
    Middle,     // Middle mouse
    Extra1,     // Mouse button 4
    Extra2,     // Mouse button 5
}

impl EntityInputEvent {
    pub fn entity(&self) -> Entity {
        match self {
            Self::PointerEnter { entity, .. } => *entity,
            Self::PointerExit { entity, .. } => *entity,
            Self::PointerMove { entity, .. } => *entity,
            Self::PointerDown { entity, .. } => *entity,
            Self::PointerUp { entity, .. } => *entity,
            Self::Click { entity, .. } => *entity,
            Self::DoubleClick { entity, .. } => *entity,
            Self::DragStart { entity, .. } => *entity,
            Self::Drag { entity, .. } => *entity,
            Self::DragEnd { entity, .. } => *entity,
            Self::Scroll { entity, .. } => *entity,
        }
    }
}
```

### 2. Entity Input Handler Component

```rust
// crates/void_ecs/src/components/input_handler.rs (NEW FILE)

use std::collections::HashSet;

/// Configures how an entity receives input events
#[derive(Clone, Debug)]
pub struct InputHandler {
    /// Which events this entity listens for
    pub events: HashSet<InputEventKind>,

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
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum InputEventKind {
    PointerEnter,
    PointerExit,
    PointerMove,
    PointerDown,
    PointerUp,
    Click,
    DoubleClick,
    DragStart,
    Drag,
    DragEnd,
    Scroll,
}

#[derive(Clone, Copy, Debug)]
pub enum CursorIcon {
    Default,
    Pointer,
    Crosshair,
    Move,
    Text,
    Wait,
    Help,
    NotAllowed,
    Grab,
    Grabbing,
    ResizeNS,
    ResizeEW,
    ResizeNESW,
    ResizeNWSE,
}

impl Default for InputHandler {
    fn default() -> Self {
        Self {
            events: HashSet::new(),
            blocks_raycast: true,
            bubble_up: true,
            propagate_down: false,
            capture_on_press: false,
            hover_cursor: None,
            enabled: true,
        }
    }
}

impl InputHandler {
    pub fn clickable() -> Self {
        Self {
            events: [
                InputEventKind::PointerEnter,
                InputEventKind::PointerExit,
                InputEventKind::PointerDown,
                InputEventKind::PointerUp,
                InputEventKind::Click,
            ].into_iter().collect(),
            hover_cursor: Some(CursorIcon::Pointer),
            ..Default::default()
        }
    }

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
            ].into_iter().collect(),
            capture_on_press: true,
            hover_cursor: Some(CursorIcon::Grab),
            ..Default::default()
        }
    }

    pub fn hover_only() -> Self {
        Self {
            events: [
                InputEventKind::PointerEnter,
                InputEventKind::PointerExit,
                InputEventKind::PointerMove,
            ].into_iter().collect(),
            blocks_raycast: false,
            ..Default::default()
        }
    }
}
```

### 3. Input State Tracking

```rust
// crates/void_render/src/input/state.rs (NEW FILE)

use std::collections::{HashMap, HashSet};
use void_ecs::Entity;

/// Tracks input state across frames
pub struct InputState {
    /// Currently hovered entities per pointer
    hovered: HashMap<u32, HashSet<Entity>>,

    /// Currently pressed entities per pointer+button
    pressed: HashMap<(u32, PointerButton), Entity>,

    /// Entity capturing all input
    captured: Option<(Entity, u32)>,

    /// Last click time for double-click detection
    last_click: HashMap<(u32, PointerButton), (f64, Entity)>,

    /// Drag state
    drag_state: HashMap<u32, DragState>,

    /// Double-click threshold in seconds
    double_click_threshold: f64,

    /// Drag start threshold in pixels
    drag_threshold: f32,
}

#[derive(Clone, Debug)]
struct DragState {
    entity: Entity,
    button: PointerButton,
    start_screen: [f32; 2],
    start_world: Vec3,
    dragging: bool,
}

impl InputState {
    pub fn new() -> Self {
        Self {
            hovered: HashMap::new(),
            pressed: HashMap::new(),
            captured: None,
            last_click: HashMap::new(),
            drag_state: HashMap::new(),
            double_click_threshold: 0.3,
            drag_threshold: 5.0,
        }
    }

    /// Get currently hovered entity for pointer
    pub fn hovered(&self, pointer_id: u32) -> Option<&HashSet<Entity>> {
        self.hovered.get(&pointer_id)
    }

    /// Get entity capturing input
    pub fn captured(&self) -> Option<Entity> {
        self.captured.map(|(e, _)| e)
    }

    /// Capture input for entity
    pub fn capture(&mut self, entity: Entity, pointer_id: u32) {
        self.captured = Some((entity, pointer_id));
    }

    /// Release captured input
    pub fn release_capture(&mut self) {
        self.captured = None;
    }
}
```

### 4. Input Processing System

```rust
// crates/void_render/src/input/processor.rs (NEW FILE)

use void_ecs::{World, Entity};
use void_event::EventBus;
use crate::picking::{RaycastSystem, RaycastQuery, screen_to_ray};

/// Processes raw input into entity events
pub struct EntityInputProcessor {
    state: InputState,
    event_queue: Vec<EntityInputEvent>,
}

impl EntityInputProcessor {
    pub fn new() -> Self {
        Self {
            state: InputState::new(),
            event_queue: Vec::new(),
        }
    }

    /// Process pointer move event
    pub fn on_pointer_move(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        pointer_id: u32,
        camera: &CameraRenderData,
    ) {
        let ray = screen_to_ray(screen_pos, screen_size, &camera.view_matrix, &camera.projection_matrix);

        // Check if input is captured
        if let Some(captured) = self.state.captured() {
            self.handle_captured_move(world, captured, screen_pos, pointer_id);
            return;
        }

        // Raycast for hover
        let query = RaycastQuery::new().with_layer_mask(u32::MAX);
        let hits = RaycastSystem::raycast(world, &ray, &query);

        let current_hit = hits.first().map(|h| h.entity);

        // Get previous hovered entities
        let prev_hovered = self.state.hovered.get(&pointer_id)
            .cloned()
            .unwrap_or_default();

        // Determine enter/exit events
        let mut new_hovered = HashSet::new();

        if let Some(entity) = current_hit {
            new_hovered.insert(entity);

            // Check parents for bubble
            self.collect_bubble_targets(world, entity, &mut new_hovered);

            // Enter events
            for &e in &new_hovered {
                if !prev_hovered.contains(&e) {
                    if let Some(hit) = hits.iter().find(|h| h.entity == e) {
                        self.queue_event(EntityInputEvent::PointerEnter {
                            entity: e,
                            position: hit.position,
                            pointer_id,
                        });
                    }
                }
            }
        }

        // Exit events
        for &e in &prev_hovered {
            if !new_hovered.contains(&e) {
                self.queue_event(EntityInputEvent::PointerExit {
                    entity: e,
                    pointer_id,
                });
            }
        }

        // Move events
        if let Some(hit) = hits.first() {
            self.queue_event(EntityInputEvent::PointerMove {
                entity: hit.entity,
                position: hit.position,
                delta: [0.0, 0.0],  // TODO: Calculate from previous
                pointer_id,
            });
        }

        self.state.hovered.insert(pointer_id, new_hovered);

        // Update drag if active
        if let Some(drag) = self.state.drag_state.get_mut(&pointer_id) {
            let delta = [
                screen_pos[0] - drag.start_screen[0],
                screen_pos[1] - drag.start_screen[1],
            ];

            let dist = (delta[0] * delta[0] + delta[1] * delta[1]).sqrt();

            if !drag.dragging && dist > self.state.drag_threshold {
                drag.dragging = true;
                self.queue_event(EntityInputEvent::DragStart {
                    entity: drag.entity,
                    position: drag.start_world,
                    button: drag.button,
                    pointer_id,
                });
            }

            if drag.dragging {
                let pos = hits.first()
                    .map(|h| h.position)
                    .unwrap_or(drag.start_world);

                self.queue_event(EntityInputEvent::Drag {
                    entity: drag.entity,
                    position: pos,
                    delta,
                    button: drag.button,
                    pointer_id,
                });
            }
        }
    }

    /// Process pointer down event
    pub fn on_pointer_down(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
        camera: &CameraRenderData,
    ) {
        let ray = screen_to_ray(screen_pos, screen_size, &camera.view_matrix, &camera.projection_matrix);
        let query = RaycastQuery::new();
        let hits = RaycastSystem::raycast(world, &ray, &query);

        if let Some(hit) = hits.first() {
            // Check if entity wants input
            if let Some(handler) = world.get::<InputHandler>(hit.entity) {
                if !handler.enabled || !handler.events.contains(&InputEventKind::PointerDown) {
                    return;
                }

                self.queue_event(EntityInputEvent::PointerDown {
                    entity: hit.entity,
                    position: hit.position,
                    button,
                    pointer_id,
                });

                self.state.pressed.insert((pointer_id, button), hit.entity);

                // Start potential drag
                if handler.events.contains(&InputEventKind::Drag) {
                    self.state.drag_state.insert(pointer_id, DragState {
                        entity: hit.entity,
                        button,
                        start_screen: screen_pos,
                        start_world: hit.position,
                        dragging: false,
                    });
                }

                // Capture if requested
                if handler.capture_on_press {
                    self.state.capture(hit.entity, pointer_id);
                }

                // Bubble up
                if handler.bubble_up {
                    self.bubble_event(world, hit.entity, |e| {
                        EntityInputEvent::PointerDown {
                            entity: e,
                            position: hit.position,
                            button,
                            pointer_id,
                        }
                    });
                }
            }
        }
    }

    /// Process pointer up event
    pub fn on_pointer_up(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
        camera: &CameraRenderData,
        time: f64,
    ) {
        let ray = screen_to_ray(screen_pos, screen_size, &camera.view_matrix, &camera.projection_matrix);
        let query = RaycastQuery::new();
        let hits = RaycastSystem::raycast(world, &ray, &query);

        // Get pressed entity
        let pressed = self.state.pressed.remove(&(pointer_id, button));

        if let Some(hit) = hits.first() {
            self.queue_event(EntityInputEvent::PointerUp {
                entity: hit.entity,
                position: hit.position,
                button,
                pointer_id,
            });

            // Check for click (up on same entity as down)
            if pressed == Some(hit.entity) {
                // Check for double click
                let key = (pointer_id, button);
                let is_double = self.state.last_click.get(&key)
                    .map(|(t, e)| {
                        *e == hit.entity && (time - t) < self.state.double_click_threshold
                    })
                    .unwrap_or(false);

                if is_double {
                    self.queue_event(EntityInputEvent::DoubleClick {
                        entity: hit.entity,
                        position: hit.position,
                        button,
                        pointer_id,
                    });
                    self.state.last_click.remove(&key);
                } else {
                    self.queue_event(EntityInputEvent::Click {
                        entity: hit.entity,
                        position: hit.position,
                        button,
                        pointer_id,
                    });
                    self.state.last_click.insert(key, (time, hit.entity));
                }
            }
        }

        // End drag
        if let Some(drag) = self.state.drag_state.remove(&pointer_id) {
            if drag.dragging {
                let pos = hits.first()
                    .map(|h| h.position)
                    .unwrap_or(drag.start_world);

                self.queue_event(EntityInputEvent::DragEnd {
                    entity: drag.entity,
                    position: pos,
                    button: drag.button,
                    pointer_id,
                });
            }
        }

        // Release capture
        if self.state.captured.map(|(_, p)| p) == Some(pointer_id) {
            self.state.release_capture();
        }
    }

    /// Process scroll event
    pub fn on_scroll(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        delta: [f32; 2],
        camera: &CameraRenderData,
    ) {
        let ray = screen_to_ray(screen_pos, screen_size, &camera.view_matrix, &camera.projection_matrix);
        let query = RaycastQuery::new();

        if let Some(hit) = RaycastSystem::raycast_first(world, &ray, &query) {
            self.queue_event(EntityInputEvent::Scroll {
                entity: hit.entity,
                position: hit.position,
                delta,
            });
        }
    }

    /// Drain queued events
    pub fn drain_events(&mut self) -> impl Iterator<Item = EntityInputEvent> + '_ {
        self.event_queue.drain(..)
    }

    fn queue_event(&mut self, event: EntityInputEvent) {
        self.event_queue.push(event);
    }

    fn bubble_event(
        &mut self,
        world: &World,
        entity: Entity,
        make_event: impl Fn(Entity) -> EntityInputEvent,
    ) {
        let mut current = entity;
        while let Some(parent) = world.get::<Parent>(current) {
            current = parent.entity;

            if let Some(handler) = world.get::<InputHandler>(current) {
                if handler.enabled {
                    self.queue_event(make_event(current));
                }

                if !handler.bubble_up {
                    break;
                }
            }
        }
    }

    fn collect_bubble_targets(&self, world: &World, entity: Entity, set: &mut HashSet<Entity>) {
        let mut current = entity;
        while let Some(parent) = world.get::<Parent>(current) {
            current = parent.entity;
            set.insert(current);

            if let Some(handler) = world.get::<InputHandler>(current) {
                if !handler.bubble_up {
                    break;
                }
            }
        }
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/input_events.rs` | CREATE | Event types |
| `void_ecs/src/components/input_handler.rs` | CREATE | Handler component |
| `void_render/src/input/state.rs` | CREATE | Input state |
| `void_render/src/input/processor.rs` | CREATE | Event processor |
| `void_event/src/lib.rs` | MODIFY | Entity event support |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_click_detection() {
    let mut processor = EntityInputProcessor::new();
    // Simulate down then up on same entity
    // Verify Click event generated
}

#[test]
fn test_double_click() {
    let mut processor = EntityInputProcessor::new();
    // Two clicks within threshold
    // Verify DoubleClick event
}

#[test]
fn test_drag_threshold() {
    let mut processor = EntityInputProcessor::new();
    // Move less than threshold - no DragStart
    // Move more than threshold - DragStart
}

#[test]
fn test_event_bubbling() {
    // Create parent-child hierarchy
    // Click on child
    // Verify parent receives event
}
```

## Hot-Swap Support

### Serialization

All input components and events must be serializable for hot-swap:

```rust
// crates/void_ecs/src/components/input_handler.rs

use serde::{Serialize, Deserialize};
use std::collections::HashSet;

/// Configures how an entity receives input events
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct InputHandler {
    pub events: HashSet<InputEventKind>,
    pub blocks_raycast: bool,
    pub bubble_up: bool,
    pub propagate_down: bool,
    pub capture_on_press: bool,
    pub hover_cursor: Option<CursorIcon>,
    pub enabled: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum InputEventKind {
    PointerEnter,
    PointerExit,
    PointerMove,
    PointerDown,
    PointerUp,
    Click,
    DoubleClick,
    DragStart,
    Drag,
    DragEnd,
    Scroll,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum CursorIcon {
    Default,
    Pointer,
    Crosshair,
    Move,
    Text,
    Wait,
    Help,
    NotAllowed,
    Grab,
    Grabbing,
    ResizeNS,
    ResizeEW,
    ResizeNESW,
    ResizeNWSE,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum PointerButton {
    Primary,
    Secondary,
    Middle,
    Extra1,
    Extra2,
}

// crates/void_ecs/src/components/input_events.rs

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum EntityInputEvent {
    PointerEnter { entity: Entity, position: Vec3, pointer_id: u32 },
    PointerExit { entity: Entity, pointer_id: u32 },
    PointerMove { entity: Entity, position: Vec3, delta: [f32; 2], pointer_id: u32 },
    PointerDown { entity: Entity, position: Vec3, button: PointerButton, pointer_id: u32 },
    PointerUp { entity: Entity, position: Vec3, button: PointerButton, pointer_id: u32 },
    Click { entity: Entity, position: Vec3, button: PointerButton, pointer_id: u32 },
    DoubleClick { entity: Entity, position: Vec3, button: PointerButton, pointer_id: u32 },
    DragStart { entity: Entity, position: Vec3, button: PointerButton, pointer_id: u32 },
    Drag { entity: Entity, position: Vec3, delta: [f32; 2], button: PointerButton, pointer_id: u32 },
    DragEnd { entity: Entity, position: Vec3, button: PointerButton, pointer_id: u32 },
    Scroll { entity: Entity, position: Vec3, delta: [f32; 2] },
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/input/state.rs

use void_core::hot_reload::{HotReloadable, HotReloadContext};
use serde::{Serialize, Deserialize};

/// Serializable snapshot of input state for hot-reload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct InputStateSnapshot {
    /// Currently hovered entities per pointer
    pub hovered: HashMap<u32, HashSet<Entity>>,
    /// Currently pressed entities (pointer_id, button) -> entity
    pub pressed: Vec<((u32, PointerButton), Entity)>,
    /// Entity currently capturing input
    pub captured: Option<(Entity, u32)>,
    /// Double-click threshold
    pub double_click_threshold: f64,
    /// Drag threshold in pixels
    pub drag_threshold: f32,
}

impl HotReloadable for InputState {
    type State = InputStateSnapshot;

    fn save_state(&self) -> Self::State {
        InputStateSnapshot {
            hovered: self.hovered.clone(),
            pressed: self.pressed.iter()
                .map(|(k, v)| (*k, *v))
                .collect(),
            captured: self.captured,
            double_click_threshold: self.double_click_threshold,
            drag_threshold: self.drag_threshold,
        }
    }

    fn restore_state(&mut self, state: Self::State, _ctx: &HotReloadContext) {
        self.hovered = state.hovered;
        self.pressed = state.pressed.into_iter().collect();
        self.captured = state.captured;
        self.double_click_threshold = state.double_click_threshold;
        self.drag_threshold = state.drag_threshold;
        // Clear transient state
        self.last_click.clear();
        self.drag_state.clear();
    }

    fn version() -> u32 {
        1
    }
}

// crates/void_render/src/input/processor.rs

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct EntityInputProcessorState {
    pub input_state: InputStateSnapshot,
    /// Pending events preserved across reload
    pub pending_events: Vec<EntityInputEvent>,
}

impl HotReloadable for EntityInputProcessor {
    type State = EntityInputProcessorState;

    fn save_state(&self) -> Self::State {
        EntityInputProcessorState {
            input_state: self.state.save_state(),
            pending_events: self.event_queue.clone(),
        }
    }

    fn restore_state(&mut self, state: Self::State, ctx: &HotReloadContext) {
        self.state.restore_state(state.input_state, ctx);
        // Preserve pending events to prevent event loss during hot-reload
        self.event_queue = state.pending_events;
    }

    fn version() -> u32 {
        1
    }
}

impl HotReloadable for InputHandler {
    type State = InputHandler;

    fn save_state(&self) -> Self::State {
        self.clone()
    }

    fn restore_state(&mut self, state: Self::State, _ctx: &HotReloadContext) {
        *self = state;
    }

    fn version() -> u32 {
        1
    }
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/input/mod.rs

use std::sync::mpsc::{channel, Sender, Receiver};

/// Pending input system updates to be applied at frame boundary
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum InputSystemUpdate {
    /// Update input handler for entity
    UpdateHandler { entity: Entity, handler: InputHandler },
    /// Force release capture
    ReleaseCapture,
    /// Clear hover state for pointer
    ClearHover { pointer_id: u32 },
    /// Inject synthetic event
    InjectEvent { event: EntityInputEvent },
    /// Update thresholds
    SetDoubleClickThreshold { seconds: f64 },
    SetDragThreshold { pixels: f32 },
}

pub struct InputUpdateQueue {
    sender: Sender<InputSystemUpdate>,
    receiver: Receiver<InputSystemUpdate>,
}

impl InputUpdateQueue {
    pub fn new() -> Self {
        let (sender, receiver) = channel();
        Self { sender, receiver }
    }

    /// Queue update for next frame boundary
    pub fn queue(&self, update: InputSystemUpdate) {
        let _ = self.sender.send(update);
    }

    /// Apply all pending updates (call at frame boundary)
    pub fn apply_pending(&self, processor: &mut EntityInputProcessor, world: &mut World) {
        while let Ok(update) = self.receiver.try_recv() {
            match update {
                InputSystemUpdate::UpdateHandler { entity, handler } => {
                    world.insert(entity, handler);
                }
                InputSystemUpdate::ReleaseCapture => {
                    processor.state.release_capture();
                }
                InputSystemUpdate::ClearHover { pointer_id } => {
                    processor.state.hovered.remove(&pointer_id);
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
}

/// Event queue that preserves events across hot-reload
pub struct PersistentEventQueue {
    events: Vec<EntityInputEvent>,
    max_preserved: usize,
}

impl PersistentEventQueue {
    pub fn new(max_preserved: usize) -> Self {
        Self {
            events: Vec::new(),
            max_preserved,
        }
    }

    pub fn push(&mut self, event: EntityInputEvent) {
        self.events.push(event);
    }

    pub fn drain(&mut self) -> impl Iterator<Item = EntityInputEvent> + '_ {
        self.events.drain(..)
    }

    /// Save events for hot-reload (limits to prevent unbounded growth)
    pub fn save_for_reload(&self) -> Vec<EntityInputEvent> {
        self.events.iter()
            .take(self.max_preserved)
            .cloned()
            .collect()
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_input_handler_serialization_roundtrip() {
        let handler = InputHandler {
            events: [InputEventKind::Click, InputEventKind::Drag].into_iter().collect(),
            blocks_raycast: true,
            bubble_up: true,
            propagate_down: false,
            capture_on_press: true,
            hover_cursor: Some(CursorIcon::Pointer),
            enabled: true,
        };

        let json = serde_json::to_string(&handler).unwrap();
        let restored: InputHandler = serde_json::from_str(&json).unwrap();

        assert_eq!(handler.events, restored.events);
        assert_eq!(handler.blocks_raycast, restored.blocks_raycast);
        assert_eq!(handler.capture_on_press, restored.capture_on_press);
    }

    #[test]
    fn test_input_event_all_variants_serialize() {
        let entity = Entity::from_raw(1);
        let pos = Vec3::new(1.0, 2.0, 3.0);

        let events = vec![
            EntityInputEvent::PointerEnter { entity, position: pos, pointer_id: 0 },
            EntityInputEvent::PointerExit { entity, pointer_id: 0 },
            EntityInputEvent::Click { entity, position: pos, button: PointerButton::Primary, pointer_id: 0 },
            EntityInputEvent::DragStart { entity, position: pos, button: PointerButton::Primary, pointer_id: 0 },
            EntityInputEvent::Scroll { entity, position: pos, delta: [0.0, 1.0] },
        ];

        for event in events {
            let json = serde_json::to_string(&event).unwrap();
            let restored: EntityInputEvent = serde_json::from_str(&json).unwrap();
            assert_eq!(event.entity(), restored.entity());
        }
    }

    #[test]
    fn test_input_state_preservation() {
        let mut state = InputState::new();
        let entity = Entity::from_raw(42);

        state.hovered.insert(0, [entity].into_iter().collect());
        state.pressed.insert((0, PointerButton::Primary), entity);
        state.capture(entity, 0);

        let snapshot = state.save_state();
        let mut new_state = InputState::new();
        new_state.restore_state(snapshot, &HotReloadContext::default());

        assert!(new_state.hovered.get(&0).unwrap().contains(&entity));
        assert_eq!(new_state.pressed.get(&(0, PointerButton::Primary)), Some(&entity));
        assert_eq!(new_state.captured(), Some(entity));
    }

    #[test]
    fn test_event_queue_preservation() {
        let mut processor = EntityInputProcessor::new();
        let entity = Entity::from_raw(1);

        processor.queue_event(EntityInputEvent::Click {
            entity,
            position: Vec3::ZERO,
            button: PointerButton::Primary,
            pointer_id: 0,
        });

        let state = processor.save_state();
        assert_eq!(state.pending_events.len(), 1);

        let mut new_processor = EntityInputProcessor::new();
        new_processor.restore_state(state, &HotReloadContext::default());

        let events: Vec<_> = new_processor.drain_events().collect();
        assert_eq!(events.len(), 1);
    }

    #[test]
    fn test_cursor_icon_all_variants() {
        let icons = [
            CursorIcon::Default, CursorIcon::Pointer, CursorIcon::Crosshair,
            CursorIcon::Move, CursorIcon::Text, CursorIcon::Wait,
            CursorIcon::Help, CursorIcon::NotAllowed, CursorIcon::Grab,
            CursorIcon::Grabbing, CursorIcon::ResizeNS, CursorIcon::ResizeEW,
            CursorIcon::ResizeNESW, CursorIcon::ResizeNWSE,
        ];

        for icon in icons {
            let json = serde_json::to_string(&icon).unwrap();
            let restored: CursorIcon = serde_json::from_str(&json).unwrap();
            assert_eq!(format!("{:?}", icon), format!("{:?}", restored));
        }
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_render/src/input/processor.rs

use std::panic::{catch_unwind, AssertUnwindSafe};
use log::{error, warn};

impl EntityInputProcessor {
    /// Process pointer move with fault tolerance
    pub fn on_pointer_move_safe(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        pointer_id: u32,
        camera: &CameraRenderData,
    ) {
        let result = catch_unwind(AssertUnwindSafe(|| {
            self.on_pointer_move(world, screen_pos, screen_size, pointer_id, camera)
        }));

        if let Err(e) = result {
            error!(
                "Pointer move processing panicked for pointer {}: {:?}",
                pointer_id,
                e.downcast_ref::<&str>()
            );
            // Clear potentially corrupted state for this pointer
            self.state.hovered.remove(&pointer_id);
        }
    }

    /// Process pointer down with fault tolerance
    pub fn on_pointer_down_safe(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
        camera: &CameraRenderData,
    ) {
        let result = catch_unwind(AssertUnwindSafe(|| {
            self.on_pointer_down(world, screen_pos, screen_size, button, pointer_id, camera)
        }));

        if let Err(e) = result {
            error!(
                "Pointer down processing panicked: {:?}",
                e.downcast_ref::<&str>()
            );
        }
    }

    /// Process pointer up with fault tolerance
    pub fn on_pointer_up_safe(
        &mut self,
        world: &World,
        screen_pos: [f32; 2],
        screen_size: [f32; 2],
        button: PointerButton,
        pointer_id: u32,
        camera: &CameraRenderData,
        time: f64,
    ) {
        let result = catch_unwind(AssertUnwindSafe(|| {
            self.on_pointer_up(world, screen_pos, screen_size, button, pointer_id, camera, time)
        }));

        if let Err(e) = result {
            error!(
                "Pointer up processing panicked: {:?}",
                e.downcast_ref::<&str>()
            );
            // Force cleanup of pressed/drag state
            self.state.pressed.remove(&(pointer_id, button));
            self.state.drag_state.remove(&pointer_id);
            self.state.release_capture();
        }
    }

    /// Safe event bubbling that won't panic on hierarchy issues
    fn bubble_event_safe(
        &mut self,
        world: &World,
        entity: Entity,
        make_event: impl Fn(Entity) -> EntityInputEvent,
    ) {
        let result = catch_unwind(AssertUnwindSafe(|| {
            self.bubble_event(world, entity, make_event)
        }));

        if let Err(_) = result {
            warn!("Event bubbling failed for entity {:?}, event not propagated", entity);
        }
    }
}
```

### Degradation Behavior

```rust
// crates/void_render/src/input/processor.rs

/// Input processing mode for graceful degradation
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum InputProcessingMode {
    /// Full processing with all features
    Full,
    /// Skip drag detection (simpler, faster)
    NoDrag,
    /// Skip bubbling (flat event delivery)
    NoBubbling,
    /// Only basic click detection
    BasicOnly,
    /// Disabled (no event processing)
    Disabled,
}

impl EntityInputProcessor {
    mode: InputProcessingMode,

    /// Degrade processing if errors are occurring frequently
    pub fn check_health_and_degrade(&mut self, error_count: u32, frame_count: u32) {
        const ERROR_THRESHOLD: f32 = 0.1; // 10% error rate

        if frame_count == 0 {
            return;
        }

        let error_rate = error_count as f32 / frame_count as f32;

        if error_rate > ERROR_THRESHOLD {
            self.mode = match self.mode {
                InputProcessingMode::Full => {
                    warn!("High error rate in input processing, disabling drag");
                    InputProcessingMode::NoDrag
                }
                InputProcessingMode::NoDrag => {
                    warn!("Continued errors, disabling bubbling");
                    InputProcessingMode::NoBubbling
                }
                InputProcessingMode::NoBubbling => {
                    warn!("Continued errors, switching to basic mode");
                    InputProcessingMode::BasicOnly
                }
                InputProcessingMode::BasicOnly => {
                    error!("Too many errors, disabling input processing");
                    InputProcessingMode::Disabled
                }
                InputProcessingMode::Disabled => InputProcessingMode::Disabled,
            };
        }
    }

    /// Attempt to restore full processing
    pub fn try_restore_full_processing(&mut self) {
        if self.mode != InputProcessingMode::Full {
            self.mode = InputProcessingMode::Full;
        }
    }
}

/// Fallback event for when normal processing fails
impl EntityInputEvent {
    /// Create minimal fallback event
    pub fn fallback_click(entity: Entity) -> Self {
        Self::Click {
            entity,
            position: Vec3::ZERO,
            button: PointerButton::Primary,
            pointer_id: 0,
        }
    }
}
```

## Acceptance Criteria

### Functional

- [ ] PointerEnter/Exit work correctly
- [ ] Click events fire on press+release
- [ ] Double click detection works
- [ ] Drag start/drag/end work
- [ ] Events bubble up hierarchy
- [ ] Input capture blocks other entities
- [ ] Layer filtering works
- [ ] Touch input maps to pointer events
- [ ] Editor uses this for selection

### Hot-Swap Compliance

- [ ] `InputHandler` component has `#[derive(Serialize, Deserialize)]`
- [ ] `InputEventKind` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `CursorIcon` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `PointerButton` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `EntityInputEvent` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `InputState` implements `HotReloadable` trait
- [ ] `EntityInputProcessor` implements `HotReloadable` trait
- [ ] Event queue is preserved across hot-reload
- [ ] Hover/pressed state is preserved across hot-reload
- [ ] Input capture is preserved across hot-reload
- [ ] All hot-swap tests pass
- [ ] `catch_unwind` protects all pointer event handlers
- [ ] Graceful degradation under high error rates

## Dependencies

- **Phase 10: Picking & Raycasting** - Entity hit detection

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: Medium
**Primary Crates**: void_ecs, void_render
**Reviewer Notes**: Ensure touch and mouse have consistent behavior
