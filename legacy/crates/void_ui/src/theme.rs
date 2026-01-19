//! Theme - Color schemes and styling for UI elements

/// Color scheme for UI elements
#[derive(Clone, Debug)]
pub struct ThemeColors {
    /// Background color for panels
    pub panel_bg: [f32; 4],
    /// Border color for panels
    pub panel_border: [f32; 4],
    /// Primary text color
    pub text: [f32; 4],
    /// Secondary/dimmed text color
    pub text_dim: [f32; 4],
    /// Success/positive color (green)
    pub success: [f32; 4],
    /// Warning color (yellow/orange)
    pub warning: [f32; 4],
    /// Error/negative color (red)
    pub error: [f32; 4],
    /// Info/highlight color (cyan/blue)
    pub info: [f32; 4],
    /// Accent color
    pub accent: [f32; 4],
}

/// Complete theme configuration
#[derive(Clone, Debug)]
pub struct Theme {
    /// Color scheme
    pub colors: ThemeColors,
    /// Default text scale
    pub text_scale: f32,
    /// Line height multiplier
    pub line_height: f32,
    /// Panel padding
    pub padding: f32,
    /// Panel border radius (for future use)
    pub border_radius: f32,
    /// Panel border width
    pub border_width: f32,
}

impl Theme {
    /// Dark theme (default)
    pub fn dark() -> Self {
        Self {
            colors: ThemeColors {
                panel_bg: [0.1, 0.1, 0.12, 0.9],
                panel_border: [0.3, 0.3, 0.35, 1.0],
                text: [1.0, 1.0, 1.0, 1.0],
                text_dim: [0.6, 0.6, 0.6, 1.0],
                success: [0.3, 0.9, 0.3, 1.0],
                warning: [0.9, 0.8, 0.2, 1.0],
                error: [0.9, 0.3, 0.3, 1.0],
                info: [0.3, 0.7, 0.9, 1.0],
                accent: [0.4, 0.6, 1.0, 1.0],
            },
            text_scale: 1.0,
            line_height: 1.4,
            padding: 8.0,
            border_radius: 4.0,
            border_width: 1.0,
        }
    }

    /// Light theme
    pub fn light() -> Self {
        Self {
            colors: ThemeColors {
                panel_bg: [0.95, 0.95, 0.95, 0.95],
                panel_border: [0.7, 0.7, 0.7, 1.0],
                text: [0.1, 0.1, 0.1, 1.0],
                text_dim: [0.4, 0.4, 0.4, 1.0],
                success: [0.1, 0.7, 0.1, 1.0],
                warning: [0.8, 0.6, 0.0, 1.0],
                error: [0.8, 0.1, 0.1, 1.0],
                info: [0.1, 0.5, 0.8, 1.0],
                accent: [0.2, 0.4, 0.8, 1.0],
            },
            text_scale: 1.0,
            line_height: 1.4,
            padding: 8.0,
            border_radius: 4.0,
            border_width: 1.0,
        }
    }

    /// High contrast theme for accessibility
    pub fn high_contrast() -> Self {
        Self {
            colors: ThemeColors {
                panel_bg: [0.0, 0.0, 0.0, 1.0],
                panel_border: [1.0, 1.0, 1.0, 1.0],
                text: [1.0, 1.0, 1.0, 1.0],
                text_dim: [0.8, 0.8, 0.8, 1.0],
                success: [0.0, 1.0, 0.0, 1.0],
                warning: [1.0, 1.0, 0.0, 1.0],
                error: [1.0, 0.0, 0.0, 1.0],
                info: [0.0, 1.0, 1.0, 1.0],
                accent: [1.0, 0.0, 1.0, 1.0],
            },
            text_scale: 1.2,
            line_height: 1.5,
            padding: 10.0,
            border_radius: 0.0,
            border_width: 2.0,
        }
    }

    /// Retro/terminal green theme
    pub fn retro() -> Self {
        Self {
            colors: ThemeColors {
                panel_bg: [0.0, 0.05, 0.0, 0.95],
                panel_border: [0.0, 0.4, 0.0, 1.0],
                text: [0.0, 1.0, 0.0, 1.0],
                text_dim: [0.0, 0.5, 0.0, 1.0],
                success: [0.0, 1.0, 0.0, 1.0],
                warning: [0.5, 1.0, 0.0, 1.0],
                error: [1.0, 0.3, 0.0, 1.0],
                info: [0.0, 0.8, 0.5, 1.0],
                accent: [0.0, 1.0, 0.5, 1.0],
            },
            text_scale: 1.0,
            line_height: 1.3,
            padding: 6.0,
            border_radius: 0.0,
            border_width: 1.0,
        }
    }

    /// Get scaled line height in pixels
    pub fn line_height_px(&self) -> f32 {
        16.0 * self.text_scale * self.line_height
    }
}

impl Default for Theme {
    fn default() -> Self {
        Self::dark()
    }
}
