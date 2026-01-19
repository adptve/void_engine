//! Selection tool for picking and box selecting entities.

use super::{Tool, ToolId, ToolResult, MouseEvent, MouseButton, Modifiers};
use crate::core::{EditorState, EntityId, SelectionMode};

pub const SELECTION_TOOL_ID: ToolId = ToolId("select");

/// Selection tool for clicking and box-selecting entities.
pub struct SelectionTool {
    /// Start of box selection (if in progress)
    box_start: Option<[f32; 2]>,
    /// Current mouse position during box selection
    box_current: [f32; 2],
    /// Whether a box selection is in progress
    is_box_selecting: bool,
}

impl Default for SelectionTool {
    fn default() -> Self {
        Self::new()
    }
}

impl SelectionTool {
    pub fn new() -> Self {
        Self {
            box_start: None,
            box_current: [0.0, 0.0],
            is_box_selecting: false,
        }
    }
}

impl Tool for SelectionTool {
    fn id(&self) -> ToolId {
        SELECTION_TOOL_ID
    }

    fn name(&self) -> &str {
        "Select"
    }

    fn shortcut(&self) -> Option<&str> {
        Some("Q")
    }

    fn icon(&self) -> Option<&str> {
        Some("[S]")
    }

    fn on_mouse_button(
        &mut self,
        event: &MouseEvent,
        state: &mut EditorState,
    ) -> ToolResult {
        if event.button != Some(MouseButton::Left) {
            return ToolResult::None;
        }

        if event.pressed {
            // Start potential box selection
            self.box_start = Some(event.position);
            self.is_box_selecting = false;
            ToolResult::Handled
        } else {
            // Mouse released
            if self.is_box_selecting {
                // Complete box selection
                if let Some(start) = self.box_start {
                    self.complete_box_selection(start, event.position, &event.modifiers, state);
                }
            } else {
                // Single click - pick entity at position
                self.pick_at_position(event.position, &event.modifiers, state);
            }

            self.box_start = None;
            self.is_box_selecting = false;
            ToolResult::Completed
        }
    }

    fn on_mouse_move(
        &mut self,
        event: &MouseEvent,
        _state: &mut EditorState,
    ) -> ToolResult {
        // Always track current position for box selection rendering
        self.box_current = event.position;

        if let Some(start) = self.box_start {
            // Check if we've dragged enough to start box selection
            let dx = (event.position[0] - start[0]).abs();
            let dy = (event.position[1] - start[1]).abs();

            if dx > 5.0 || dy > 5.0 {
                self.is_box_selecting = true;
            }

            ToolResult::Handled
        } else {
            ToolResult::None
        }
    }

    fn on_key(
        &mut self,
        key: &str,
        pressed: bool,
        modifiers: Modifiers,
        state: &mut EditorState,
    ) -> ToolResult {
        if !pressed {
            return ToolResult::None;
        }

        match key {
            "Delete" => {
                state.delete_selected();
                ToolResult::Completed
            }
            "D" if modifiers.ctrl => {
                state.duplicate_selected();
                ToolResult::Completed
            }
            "A" if modifiers.ctrl => {
                state.select_all();
                ToolResult::Completed
            }
            "Escape" => {
                state.deselect_all();
                ToolResult::Completed
            }
            _ => ToolResult::None,
        }
    }
}

impl SelectionTool {
    fn pick_at_position(&self, _position: [f32; 2], modifiers: &Modifiers, state: &mut EditorState) {
        // In a real implementation, this would do raycasting to find the entity
        // For now, just demonstrate the selection mode logic

        let mode = SelectionMode::from_modifiers(modifiers.shift, modifiers.ctrl);

        // Placeholder: would normally pick entity via raycasting
        // let picked_entity = raycast(position, state);
        // if let Some(id) = picked_entity {
        //     state.selection.select(id, mode);
        // } else if mode == SelectionMode::Replace {
        //     state.selection.clear();
        // }

        // For now, if click misses all entities and mode is Replace, clear selection
        if mode == SelectionMode::Replace {
            // Would only clear if no entity was hit
        }
    }

    fn complete_box_selection(
        &self,
        start: [f32; 2],
        end: [f32; 2],
        modifiers: &Modifiers,
        state: &mut EditorState,
    ) {
        let mode = SelectionMode::from_modifiers(modifiers.shift, modifiers.ctrl);

        // Calculate box bounds
        let min_x = start[0].min(end[0]);
        let max_x = start[0].max(end[0]);
        let min_y = start[1].min(end[1]);
        let max_y = start[1].max(end[1]);

        // Get viewport dimensions and camera info for projection
        let viewport_size = state.viewport.size();
        let viewport_width = viewport_size[0];
        let viewport_height = viewport_size[1];

        if viewport_width == 0.0 || viewport_height == 0.0 {
            return;
        }

        // Find entities whose projected screen positions fall within the box
        let entities_in_box: Vec<EntityId> = state.entities.iter()
            .filter(|entity| {
                // Project entity position to screen space
                let screen_pos = self.project_to_screen(
                    entity.transform.position,
                    &state.viewport,
                );

                if let Some([sx, sy]) = screen_pos {
                    sx >= min_x && sx <= max_x && sy >= min_y && sy <= max_y
                } else {
                    false
                }
            })
            .map(|e| e.id)
            .collect();

        // Apply selection based on mode
        if !entities_in_box.is_empty() {
            let count = entities_in_box.len();
            match mode {
                SelectionMode::Replace => {
                    state.selection.select_multiple(entities_in_box);
                }
                SelectionMode::Add => {
                    for id in entities_in_box {
                        state.selection.add(id);
                    }
                }
                SelectionMode::Remove => {
                    for id in entities_in_box {
                        state.selection.remove_entity(id);
                    }
                }
                SelectionMode::Toggle => {
                    for id in entities_in_box {
                        state.selection.toggle(id);
                    }
                }
            }

            state.console.info(format!(
                "Box selected {} entities",
                count
            ));
        } else if mode == SelectionMode::Replace {
            state.selection.clear();
        }
    }

    /// Project a 3D world position to screen coordinates.
    fn project_to_screen(
        &self,
        world_pos: [f32; 3],
        viewport: &crate::viewport::ViewportState,
    ) -> Option<[f32; 2]> {
        // Build view and projection matrices
        let eye = viewport.camera_position();
        let target = viewport.camera_target();

        // Simple perspective projection
        let dir = [
            world_pos[0] - eye[0],
            world_pos[1] - eye[1],
            world_pos[2] - eye[2],
        ];

        // Forward direction
        let forward = [
            target[0] - eye[0],
            target[1] - eye[1],
            target[2] - eye[2],
        ];
        let forward_len = (forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]).sqrt();
        if forward_len < 0.0001 {
            return None;
        }
        let forward = [forward[0]/forward_len, forward[1]/forward_len, forward[2]/forward_len];

        // Check if point is in front of camera
        let depth = dir[0]*forward[0] + dir[1]*forward[1] + dir[2]*forward[2];
        if depth < 0.1 {
            return None;
        }

        // Right and up vectors
        let up = [0.0, 1.0, 0.0];
        let right = [
            forward[1]*up[2] - forward[2]*up[1],
            forward[2]*up[0] - forward[0]*up[2],
            forward[0]*up[1] - forward[1]*up[0],
        ];
        let right_len = (right[0]*right[0] + right[1]*right[1] + right[2]*right[2]).sqrt();
        if right_len < 0.0001 {
            return None;
        }
        let right = [right[0]/right_len, right[1]/right_len, right[2]/right_len];
        let up = [
            right[1]*forward[2] - right[2]*forward[1],
            right[2]*forward[0] - right[0]*forward[2],
            right[0]*forward[1] - right[1]*forward[0],
        ];

        // Project onto view plane
        let x = dir[0]*right[0] + dir[1]*right[1] + dir[2]*right[2];
        let y = dir[0]*up[0] + dir[1]*up[1] + dir[2]*up[2];

        // Apply perspective
        let fov = std::f32::consts::PI / 4.0; // 45 degrees
        let scale = depth * fov.tan();

        let size = viewport.size();
        let screen_x = size[0] * 0.5 + (x / scale) * size[1] * 0.5;
        let screen_y = size[1] * 0.5 - (y / scale) * size[1] * 0.5;

        Some([screen_x, screen_y])
    }

    /// Get current box selection bounds (if any).
    pub fn box_selection_bounds(&self) -> Option<([f32; 2], [f32; 2])> {
        if self.is_box_selecting {
            self.box_start.map(|start| (start, self.box_current))
        } else {
            None
        }
    }

    /// Check if box selection is in progress.
    pub fn is_box_selecting(&self) -> bool {
        self.is_box_selecting
    }

    /// Get the current selection box rectangle (min, max corners).
    pub fn selection_rect(&self) -> Option<([f32; 2], [f32; 2])> {
        if self.is_box_selecting {
            if let Some(start) = self.box_start {
                let min = [
                    start[0].min(self.box_current[0]),
                    start[1].min(self.box_current[1]),
                ];
                let max = [
                    start[0].max(self.box_current[0]),
                    start[1].max(self.box_current[1]),
                ];
                return Some((min, max));
            }
        }
        None
    }
}
