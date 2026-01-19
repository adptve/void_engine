//! Void Engine Visual Editor
//!
//! A world-class 3D scene editor for the Void Engine Metaverse OS.
//!
//! ## Features
//!
//! - **Scene Hierarchy**: Tree view of all entities with drag-drop reparenting
//! - **Inspector Panel**: Property editing for selected entities
//! - **Asset Browser**: File-based asset management with thumbnails
//! - **Console**: Logging and debug output
//! - **Transform Gizmos**: Visual manipulation of entities
//! - **Undo/Redo**: Full command history with transaction support
//! - **Multi-Select**: Shift/Ctrl click and box selection
//! - **void_ecs Integration**: Direct ECS world manipulation
//! - **TOML Output**: Hot-swappable scene files
//!
//! ## Architecture
//!
//! The editor follows a command-based architecture:
//!
//! ```text
//! User Input → Tool → Command → EditorState → void_ir Patches
//! ```
//!
//! All modifications go through the command system for undo/redo support.

pub mod core;
pub mod commands;
pub mod panels;
pub mod viewport;
pub mod tools;
pub mod assets;
pub mod integration;
pub mod ui;
pub mod gpu;
pub mod scene;

// Re-export commonly used types
pub use core::{
    EditorState,
    SelectionManager,
    SelectionMode,
    UndoHistory,
    EditorPreferences,
    MeshType,
    Transform,
    SceneEntity,
    EntityId,
    NameComponent,
    TransformComponent,
    MeshComponent,
};

pub use commands::{
    Command,
    CommandResult,
    CommandError,
};

pub use panels::{
    Panel,
    PanelRegistry,
    Console,
    LogLevel,
    LogEntry,
    AssetBrowser,
    AssetType,
    AssetEntry,
};

pub use viewport::{
    ViewportState,
    CameraController,
};

pub use tools::{
    Tool,
    ToolRegistry,
    ToolResult,
};

pub use gpu::{GpuResources, GpuMesh, Uniforms, MAX_ENTITIES, SHADER_SOURCE};

/// Editor version
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

/// Editor name
pub const NAME: &str = "Void Engine Editor";
