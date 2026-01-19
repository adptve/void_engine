//! Transform tools (Move, Rotate, Scale).

use super::{Tool, ToolId, ToolResult, MouseEvent, MouseButton, Modifiers};
use crate::core::EditorState;
use crate::viewport::gizmos::{GizmoMode, GizmoPart, TranslateGizmo, RotateGizmo, ScaleGizmo, Gizmo};

pub const MOVE_TOOL_ID: ToolId = ToolId("move");
pub const ROTATE_TOOL_ID: ToolId = ToolId("rotate");
pub const SCALE_TOOL_ID: ToolId = ToolId("scale");

/// Move tool for translating entities.
pub struct MoveTool {
    gizmo: TranslateGizmo,
    is_dragging: bool,
    snap_enabled: bool,
    snap_value: f32,
}

impl Default for MoveTool {
    fn default() -> Self {
        Self::new()
    }
}

impl MoveTool {
    pub fn new() -> Self {
        Self {
            gizmo: TranslateGizmo::new(),
            is_dragging: false,
            snap_enabled: false,
            snap_value: 0.5,
        }
    }
}

impl Tool for MoveTool {
    fn id(&self) -> ToolId {
        MOVE_TOOL_ID
    }

    fn name(&self) -> &str {
        "Move"
    }

    fn shortcut(&self) -> Option<&str> {
        Some("W")
    }

    fn icon(&self) -> Option<&str> {
        Some("[M]")
    }

    fn on_activate(&mut self, _state: &mut EditorState) {
        self.is_dragging = false;
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
            // Check if clicking on gizmo
            if self.gizmo.hovered_part() != GizmoPart::None {
                self.is_dragging = true;
                // Would begin gizmo interaction here
                return ToolResult::Handled;
            }
        } else if self.is_dragging {
            // End drag
            self.is_dragging = false;
            state.scene_modified = true;
            return ToolResult::Completed;
        }

        ToolResult::None
    }

    fn on_mouse_move(
        &mut self,
        event: &MouseEvent,
        state: &mut EditorState,
    ) -> ToolResult {
        if self.is_dragging {
            // Update gizmo interaction
            // Would use gizmo.update_interaction() here

            // Mark as modified
            state.scene_modified = true;
            return ToolResult::Handled;
        }

        // Update hover state
        // Would do hit testing here

        ToolResult::None
    }

    fn on_key(
        &mut self,
        key: &str,
        pressed: bool,
        modifiers: Modifiers,
        _state: &mut EditorState,
    ) -> ToolResult {
        if !pressed {
            return ToolResult::None;
        }

        // Axis constraints
        match key {
            "X" => {
                // Constrain to X axis
                ToolResult::Handled
            }
            "Y" => {
                // Constrain to Y axis
                ToolResult::Handled
            }
            "Z" => {
                // Constrain to Z axis
                ToolResult::Handled
            }
            "Shift" => {
                // Toggle snapping
                self.snap_enabled = !self.snap_enabled;
                ToolResult::Handled
            }
            _ => ToolResult::None,
        }
    }
}

/// Rotate tool for rotating entities.
pub struct RotateTool {
    gizmo: RotateGizmo,
    is_dragging: bool,
    snap_enabled: bool,
    snap_value: f32, // degrees
}

impl Default for RotateTool {
    fn default() -> Self {
        Self::new()
    }
}

impl RotateTool {
    pub fn new() -> Self {
        Self {
            gizmo: RotateGizmo::new(),
            is_dragging: false,
            snap_enabled: false,
            snap_value: 15.0, // 15 degree increments
        }
    }
}

impl Tool for RotateTool {
    fn id(&self) -> ToolId {
        ROTATE_TOOL_ID
    }

    fn name(&self) -> &str {
        "Rotate"
    }

    fn shortcut(&self) -> Option<&str> {
        Some("E")
    }

    fn icon(&self) -> Option<&str> {
        Some("[R]")
    }

    fn on_activate(&mut self, _state: &mut EditorState) {
        self.is_dragging = false;
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
            if self.gizmo.hovered_part() != GizmoPart::None {
                self.is_dragging = true;
                return ToolResult::Handled;
            }
        } else if self.is_dragging {
            self.is_dragging = false;
            state.scene_modified = true;
            return ToolResult::Completed;
        }

        ToolResult::None
    }

    fn on_mouse_move(
        &mut self,
        event: &MouseEvent,
        state: &mut EditorState,
    ) -> ToolResult {
        if self.is_dragging {
            state.scene_modified = true;
            return ToolResult::Handled;
        }

        ToolResult::None
    }

    fn on_key(
        &mut self,
        key: &str,
        pressed: bool,
        _modifiers: Modifiers,
        _state: &mut EditorState,
    ) -> ToolResult {
        if !pressed {
            return ToolResult::None;
        }

        match key {
            "X" | "Y" | "Z" => ToolResult::Handled,
            "Shift" => {
                self.snap_enabled = !self.snap_enabled;
                ToolResult::Handled
            }
            _ => ToolResult::None,
        }
    }
}

/// Scale tool for scaling entities.
pub struct ScaleTool {
    gizmo: ScaleGizmo,
    is_dragging: bool,
    uniform: bool,
    snap_enabled: bool,
    snap_value: f32,
}

impl Default for ScaleTool {
    fn default() -> Self {
        Self::new()
    }
}

impl ScaleTool {
    pub fn new() -> Self {
        Self {
            gizmo: ScaleGizmo::new(),
            is_dragging: false,
            uniform: true,
            snap_enabled: false,
            snap_value: 0.1,
        }
    }
}

impl Tool for ScaleTool {
    fn id(&self) -> ToolId {
        SCALE_TOOL_ID
    }

    fn name(&self) -> &str {
        "Scale"
    }

    fn shortcut(&self) -> Option<&str> {
        Some("R")
    }

    fn icon(&self) -> Option<&str> {
        Some("[S]")
    }

    fn on_activate(&mut self, _state: &mut EditorState) {
        self.is_dragging = false;
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
            if self.gizmo.hovered_part() != GizmoPart::None {
                self.is_dragging = true;
                return ToolResult::Handled;
            }
        } else if self.is_dragging {
            self.is_dragging = false;
            state.scene_modified = true;
            return ToolResult::Completed;
        }

        ToolResult::None
    }

    fn on_mouse_move(
        &mut self,
        event: &MouseEvent,
        state: &mut EditorState,
    ) -> ToolResult {
        if self.is_dragging {
            state.scene_modified = true;
            return ToolResult::Handled;
        }

        ToolResult::None
    }

    fn on_key(
        &mut self,
        key: &str,
        pressed: bool,
        _modifiers: Modifiers,
        _state: &mut EditorState,
    ) -> ToolResult {
        if !pressed {
            return ToolResult::None;
        }

        match key {
            "X" | "Y" | "Z" => {
                self.uniform = false;
                ToolResult::Handled
            }
            "Shift" => {
                self.snap_enabled = !self.snap_enabled;
                ToolResult::Handled
            }
            _ => ToolResult::None,
        }
    }
}
