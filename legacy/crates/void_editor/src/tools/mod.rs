//! Editor tools for user interaction.
//!
//! Tools handle user input in the viewport and perform actions
//! like selection, transformation, and creation.

mod tool;
mod selection_tool;
mod transform_tools;
mod creation_tool;

pub use tool::{Tool, ToolResult, ToolRegistry, ToolId, MouseEvent, MouseButton, Modifiers};
pub use selection_tool::{SelectionTool, SELECTION_TOOL_ID};
pub use transform_tools::{MoveTool, RotateTool, ScaleTool, MOVE_TOOL_ID, ROTATE_TOOL_ID, SCALE_TOOL_ID};
pub use creation_tool::{PrimitiveCreationTool, CREATION_TOOL_ID};
