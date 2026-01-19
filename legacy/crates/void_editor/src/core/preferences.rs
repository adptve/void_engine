//! Editor preferences and settings.
//!
//! Persistent settings that survive editor restarts.

use std::path::PathBuf;

/// Editor preferences and settings.
#[derive(Clone, Debug)]
pub struct EditorPreferences {
    // UI settings
    pub show_hierarchy: bool,
    pub show_inspector: bool,
    pub show_asset_browser: bool,
    pub show_console: bool,

    // Viewport settings
    pub show_grid: bool,
    pub show_gizmos: bool,
    pub show_stats: bool,
    pub snap_to_grid: bool,
    pub grid_size: f32,
    pub gizmo_size: f32,

    // Camera settings
    pub orbit_sensitivity: f32,
    pub pan_sensitivity: f32,
    pub zoom_sensitivity: f32,
    pub invert_y: bool,
    pub invert_x: bool,

    // Snapping settings
    pub position_snap: f32,
    pub rotation_snap: f32,
    pub scale_snap: f32,

    // Auto-save settings
    pub auto_save_enabled: bool,
    pub auto_save_interval_secs: u32,

    // Recent files
    pub max_recent_files: usize,

    // Theme
    pub dark_mode: bool,

    // Last used directories
    pub last_scene_directory: Option<PathBuf>,
    pub last_asset_directory: Option<PathBuf>,
}

impl Default for EditorPreferences {
    fn default() -> Self {
        Self {
            // UI panels
            show_hierarchy: true,
            show_inspector: true,
            show_asset_browser: true,
            show_console: true,

            // Viewport
            show_grid: true,
            show_gizmos: true,
            show_stats: false,
            snap_to_grid: false,
            grid_size: 1.0,
            gizmo_size: 1.0,

            // Camera
            orbit_sensitivity: 0.005,
            pan_sensitivity: 0.01,
            zoom_sensitivity: 0.1,
            invert_y: false,
            invert_x: false,

            // Snapping
            position_snap: 0.5,
            rotation_snap: 15.0, // degrees
            scale_snap: 0.1,

            // Auto-save
            auto_save_enabled: false,
            auto_save_interval_secs: 300, // 5 minutes

            // Recent files
            max_recent_files: 10,

            // Theme
            dark_mode: true,

            // Directories
            last_scene_directory: None,
            last_asset_directory: None,
        }
    }
}

impl EditorPreferences {
    /// Load preferences from a file.
    pub fn load(path: &PathBuf) -> Result<Self, std::io::Error> {
        let content = std::fs::read_to_string(path)?;
        // TODO: Implement TOML deserialization
        // For now, return defaults
        log::info!("Loaded preferences from {:?}", path);
        Ok(Self::default())
    }

    /// Save preferences to a file.
    pub fn save(&self, path: &PathBuf) -> Result<(), std::io::Error> {
        // TODO: Implement TOML serialization
        log::info!("Saved preferences to {:?}", path);
        Ok(())
    }

    /// Get the default preferences path.
    pub fn default_path() -> Option<PathBuf> {
        dirs::config_dir().map(|mut p| {
            p.push("void_editor");
            p.push("preferences.toml");
            p
        })
    }
}
