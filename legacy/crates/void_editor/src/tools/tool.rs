//! Tool trait and registry.

use std::collections::HashMap;
use crate::core::EditorState;

/// Unique identifier for a tool.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct ToolId(pub &'static str);

impl std::fmt::Display for ToolId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Result of tool operations.
#[derive(Clone, Debug)]
pub enum ToolResult {
    /// No action taken
    None,
    /// Tool handled the input, continue
    Handled,
    /// Tool completed an action
    Completed,
    /// Tool wants to switch to another tool
    SwitchTo(ToolId),
}

/// Mouse event for tool input.
#[derive(Clone, Debug)]
pub struct MouseEvent {
    pub position: [f32; 2],
    pub delta: [f32; 2],
    pub button: Option<MouseButton>,
    pub pressed: bool,
    pub modifiers: Modifiers,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MouseButton {
    Left,
    Right,
    Middle,
}

#[derive(Clone, Copy, Debug, Default)]
pub struct Modifiers {
    pub shift: bool,
    pub ctrl: bool,
    pub alt: bool,
}

/// A tool for viewport interaction.
pub trait Tool: Send + Sync {
    /// Unique identifier for this tool.
    fn id(&self) -> ToolId;

    /// Display name.
    fn name(&self) -> &str;

    /// Keyboard shortcut (e.g., "Q", "W", "E", "R").
    fn shortcut(&self) -> Option<&str> {
        None
    }

    /// Icon for toolbar.
    fn icon(&self) -> Option<&str> {
        None
    }

    /// Called when tool becomes active.
    fn on_activate(&mut self, _state: &mut EditorState) {}

    /// Called when tool becomes inactive.
    fn on_deactivate(&mut self, _state: &mut EditorState) {}

    /// Handle mouse button press/release.
    fn on_mouse_button(
        &mut self,
        _event: &MouseEvent,
        _state: &mut EditorState,
    ) -> ToolResult {
        ToolResult::None
    }

    /// Handle mouse movement.
    fn on_mouse_move(
        &mut self,
        _event: &MouseEvent,
        _state: &mut EditorState,
    ) -> ToolResult {
        ToolResult::None
    }

    /// Handle mouse scroll.
    fn on_scroll(
        &mut self,
        _delta: f32,
        _state: &mut EditorState,
    ) -> ToolResult {
        ToolResult::None
    }

    /// Handle key press.
    fn on_key(
        &mut self,
        _key: &str,
        _pressed: bool,
        _modifiers: Modifiers,
        _state: &mut EditorState,
    ) -> ToolResult {
        ToolResult::None
    }

    /// Update tool state (called each frame).
    fn update(&mut self, _state: &mut EditorState) {}

    /// Render tool overlay/preview.
    fn render_overlay(&self, _state: &EditorState) {}
}

/// Registry for managing tools.
pub struct ToolRegistry {
    tools: HashMap<ToolId, Box<dyn Tool>>,
    active_tool: Option<ToolId>,
    previous_tool: Option<ToolId>,
}

impl Default for ToolRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl ToolRegistry {
    pub fn new() -> Self {
        Self {
            tools: HashMap::new(),
            active_tool: None,
            previous_tool: None,
        }
    }

    /// Register a tool.
    pub fn register(&mut self, tool: Box<dyn Tool>) {
        let id = tool.id();
        if self.active_tool.is_none() {
            self.active_tool = Some(id);
        }
        self.tools.insert(id, tool);
    }

    /// Get the active tool.
    pub fn active(&self) -> Option<&dyn Tool> {
        self.active_tool.and_then(|id| self.tools.get(&id).map(|t| t.as_ref()))
    }

    /// Get the active tool mutably.
    pub fn active_mut(&mut self) -> Option<&mut Box<dyn Tool>> {
        if let Some(id) = self.active_tool {
            self.tools.get_mut(&id)
        } else {
            None
        }
    }

    /// Get the active tool ID.
    pub fn active_id(&self) -> Option<ToolId> {
        self.active_tool
    }

    /// Switch to a tool by ID.
    pub fn switch_to(&mut self, id: ToolId, state: &mut EditorState) {
        // Deactivate current tool
        if let Some(current_id) = self.active_tool {
            if let Some(tool) = self.tools.get_mut(&current_id) {
                tool.on_deactivate(state);
            }
            self.previous_tool = Some(current_id);
        }

        // Activate new tool
        self.active_tool = Some(id);
        if let Some(tool) = self.tools.get_mut(&id) {
            tool.on_activate(state);
        }
    }

    /// Switch to previous tool.
    pub fn switch_to_previous(&mut self, state: &mut EditorState) {
        if let Some(prev) = self.previous_tool {
            self.switch_to(prev, state);
        }
    }

    /// Get a tool by ID.
    pub fn get(&self, id: ToolId) -> Option<&dyn Tool> {
        self.tools.get(&id).map(|t| t.as_ref())
    }

    /// Get all registered tool IDs.
    pub fn tool_ids(&self) -> impl Iterator<Item = ToolId> + '_ {
        self.tools.keys().copied()
    }

    /// Find tool by shortcut key.
    pub fn find_by_shortcut(&self, key: &str) -> Option<ToolId> {
        for (id, tool) in &self.tools {
            if tool.shortcut() == Some(key) {
                return Some(*id);
            }
        }
        None
    }
}
