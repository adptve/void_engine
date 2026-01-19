//! Editor visual theme configuration.

use egui::{Color32, Visuals, Style};

/// Editor theme configuration.
#[derive(Clone, Debug)]
pub struct EditorTheme {
    pub dark_mode: bool,
    pub accent_color: Color32,
    pub selection_color: Color32,
    pub error_color: Color32,
    pub warning_color: Color32,
    pub success_color: Color32,
}

impl Default for EditorTheme {
    fn default() -> Self {
        Self::dark()
    }
}

impl EditorTheme {
    /// Create dark theme.
    pub fn dark() -> Self {
        Self {
            dark_mode: true,
            accent_color: Color32::from_rgb(100, 150, 255),
            selection_color: Color32::from_rgb(255, 200, 50),
            error_color: Color32::from_rgb(255, 100, 100),
            warning_color: Color32::from_rgb(255, 200, 100),
            success_color: Color32::from_rgb(100, 255, 100),
        }
    }

    /// Create light theme.
    pub fn light() -> Self {
        Self {
            dark_mode: false,
            accent_color: Color32::from_rgb(50, 100, 200),
            selection_color: Color32::from_rgb(200, 150, 0),
            error_color: Color32::from_rgb(200, 50, 50),
            warning_color: Color32::from_rgb(200, 150, 50),
            success_color: Color32::from_rgb(50, 200, 50),
        }
    }

    /// Apply theme to egui context.
    pub fn apply(&self, ctx: &egui::Context) {
        let visuals = if self.dark_mode {
            Visuals::dark()
        } else {
            Visuals::light()
        };

        ctx.set_visuals(visuals);
    }

    /// Get axis X color.
    pub fn axis_x_color(&self) -> Color32 {
        Color32::from_rgb(255, 100, 100)
    }

    /// Get axis Y color.
    pub fn axis_y_color(&self) -> Color32 {
        Color32::from_rgb(100, 255, 100)
    }

    /// Get axis Z color.
    pub fn axis_z_color(&self) -> Color32 {
        Color32::from_rgb(100, 100, 255)
    }
}
