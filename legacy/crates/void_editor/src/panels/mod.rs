//! Editor panels (dockable windows).
//!
//! Panels are the main UI building blocks of the editor. Each panel
//! provides a specific function (hierarchy, inspector, console, etc.).

mod panel;
mod console;
mod asset_browser;
mod hierarchy;
mod inspector;

pub use panel::{Panel, PanelRegistry, PanelId};
pub use console::{Console, LogLevel, LogEntry};
pub use asset_browser::{AssetBrowser, AssetType, AssetEntry};
pub use hierarchy::HierarchyPanel;
pub use inspector::InspectorPanel;
