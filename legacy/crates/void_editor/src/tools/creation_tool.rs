//! Primitive creation tool for spawning new entities.

use super::{Tool, ToolId, ToolResult, MouseEvent, MouseButton, Modifiers};
use crate::core::{EditorState, MeshType, SelectionMode};

pub const CREATION_TOOL_ID: ToolId = ToolId("create");

/// Tool for creating primitive entities.
pub struct PrimitiveCreationTool {
    /// Preview position (where entity will be created)
    preview_position: Option<[f32; 3]>,
    /// Whether to auto-select after creation
    auto_select: bool,
    /// Counter for unique naming
    creation_count: u32,
}

impl Default for PrimitiveCreationTool {
    fn default() -> Self {
        Self::new()
    }
}

impl PrimitiveCreationTool {
    pub fn new() -> Self {
        Self {
            preview_position: None,
            auto_select: true,
            creation_count: 0,
        }
    }

    /// Calculate world position from screen coordinates.
    fn screen_to_world(&self, _screen_pos: [f32; 2], state: &EditorState) -> [f32; 3] {
        // Simple placement on the XZ plane at Y=0
        // In a full implementation, this would do proper raycasting

        let camera_pos = state.viewport.camera_eye();
        let camera_target = state.viewport.camera_target();

        // Direction from camera to target
        let dir = [
            camera_target[0] - camera_pos[0],
            camera_target[1] - camera_pos[1],
            camera_target[2] - camera_pos[2],
        ];
        let dir_len = (dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]).sqrt();
        let dir = [dir[0]/dir_len, dir[1]/dir_len, dir[2]/dir_len];

        // Place at a fixed distance from camera in look direction, on XZ plane
        let dist = 5.0;
        [
            camera_pos[0] + dir[0] * dist,
            0.0, // Always on ground plane
            camera_pos[2] + dir[2] * dist,
        ]
    }
}

impl Tool for PrimitiveCreationTool {
    fn id(&self) -> ToolId {
        CREATION_TOOL_ID
    }

    fn name(&self) -> &str {
        "Create Primitive"
    }

    fn shortcut(&self) -> Option<&str> {
        Some("C")
    }

    fn icon(&self) -> Option<&str> {
        Some("[+]")
    }

    fn on_activate(&mut self, state: &mut EditorState) {
        self.preview_position = None;
        let prim_type = state.creation_primitive_type;
        state.set_status(format!("Create mode: {} (click to place)", prim_type.name()));
    }

    fn on_deactivate(&mut self, _state: &mut EditorState) {
        self.preview_position = None;
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
            // Create entity at preview position or calculate new position
            let position = self.preview_position
                .unwrap_or_else(|| self.screen_to_world(event.position, state));

            // Get primitive type from shared state
            let primitive_type = state.creation_primitive_type;

            // Generate unique name
            self.creation_count += 1;
            let name = format!("{} {}", primitive_type.name(), self.creation_count);

            // Create the entity
            let id = state.create_entity(name.clone(), primitive_type);

            // Set position
            if let Some(entity) = state.get_entity_mut(id) {
                entity.transform.position = position;
            }

            // Auto-select if enabled
            if self.auto_select {
                state.selection.select(id, SelectionMode::Replace);
            }

            state.set_status(format!("Created: {}", name));

            return ToolResult::Completed;
        }

        ToolResult::None
    }

    fn on_mouse_move(
        &mut self,
        event: &MouseEvent,
        state: &mut EditorState,
    ) -> ToolResult {
        // Update preview position
        self.preview_position = Some(self.screen_to_world(event.position, state));
        ToolResult::Handled
    }

    fn on_key(
        &mut self,
        key: &str,
        pressed: bool,
        _modifiers: Modifiers,
        state: &mut EditorState,
    ) -> ToolResult {
        if !pressed {
            return ToolResult::None;
        }

        // Number keys to select primitive type
        match key {
            "1" => {
                state.creation_primitive_type = MeshType::Cube;
                state.set_status("Primitive: Cube");
                ToolResult::Handled
            }
            "2" => {
                state.creation_primitive_type = MeshType::Sphere;
                state.set_status("Primitive: Sphere");
                ToolResult::Handled
            }
            "3" => {
                state.creation_primitive_type = MeshType::Cylinder;
                state.set_status("Primitive: Cylinder");
                ToolResult::Handled
            }
            "4" => {
                state.creation_primitive_type = MeshType::Plane;
                state.set_status("Primitive: Plane");
                ToolResult::Handled
            }
            "5" => {
                state.creation_primitive_type = MeshType::Torus;
                state.set_status("Primitive: Torus");
                ToolResult::Handled
            }
            "6" => {
                state.creation_primitive_type = MeshType::Diamond;
                state.set_status("Primitive: Diamond");
                ToolResult::Handled
            }
            _ => ToolResult::None,
        }
    }
}
