//! Crosshair/reticle system

use serde::{Deserialize, Serialize};

/// Crosshair style
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CrosshairStyle {
    /// Simple dot
    Dot,
    /// Plus/cross shape
    Cross,
    /// Circle
    Circle,
    /// Circle with dot
    CircleDot,
    /// T-shape (sniper)
    TShape,
    /// Chevron/V-shape
    Chevron,
    /// Custom (use custom renderer)
    Custom,
}

impl Default for CrosshairStyle {
    fn default() -> Self {
        Self::Cross
    }
}

/// Crosshair component
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Crosshair {
    /// Visual style
    pub style: CrosshairStyle,
    /// Color (RGBA)
    pub color: [f32; 4],
    /// Outline color (RGBA)
    pub outline_color: [f32; 4],
    /// Size (radius for circle, length for cross)
    pub size: f32,
    /// Thickness
    pub thickness: f32,
    /// Gap in center (for cross/T styles)
    pub gap: f32,
    /// Outline thickness (0 = no outline)
    pub outline: f32,
    /// Whether to show dot in center
    pub center_dot: bool,
    /// Dot size
    pub dot_size: f32,
    /// Dynamic spread (for accuracy feedback)
    pub spread: f32,
    /// Maximum spread
    pub max_spread: f32,
    /// Spread recovery speed
    pub spread_recovery: f32,
    /// Whether visible
    pub visible: bool,
    /// Opacity
    pub opacity: f32,
}

impl Crosshair {
    /// Create a new crosshair
    pub fn new(style: CrosshairStyle) -> Self {
        Self {
            style,
            color: [1.0, 1.0, 1.0, 1.0],
            outline_color: [0.0, 0.0, 0.0, 0.5],
            size: 10.0,
            thickness: 2.0,
            gap: 4.0,
            outline: 1.0,
            center_dot: false,
            dot_size: 2.0,
            spread: 0.0,
            max_spread: 50.0,
            spread_recovery: 5.0,
            visible: true,
            opacity: 1.0,
        }
    }

    /// Set color
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color = color;
        self
    }

    /// Set size
    pub fn with_size(mut self, size: f32) -> Self {
        self.size = size;
        self
    }

    /// Set thickness
    pub fn with_thickness(mut self, thickness: f32) -> Self {
        self.thickness = thickness;
        self
    }

    /// Set gap
    pub fn with_gap(mut self, gap: f32) -> Self {
        self.gap = gap;
        self
    }

    /// Enable center dot
    pub fn with_center_dot(mut self, size: f32) -> Self {
        self.center_dot = true;
        self.dot_size = size;
        self
    }

    /// Set outline
    pub fn with_outline(mut self, thickness: f32, color: [f32; 4]) -> Self {
        self.outline = thickness;
        self.outline_color = color;
        self
    }

    /// Set spread settings
    pub fn with_spread(mut self, max: f32, recovery: f32) -> Self {
        self.max_spread = max;
        self.spread_recovery = recovery;
        self
    }

    /// Add spread (from shooting, movement, etc.)
    pub fn add_spread(&mut self, amount: f32) {
        self.spread = (self.spread + amount).min(self.max_spread);
    }

    /// Update spread recovery
    pub fn update(&mut self, delta_time: f32) {
        if self.spread > 0.0 {
            self.spread = (self.spread - self.spread_recovery * delta_time).max(0.0);
        }
    }

    /// Get current effective size (base + spread)
    pub fn effective_size(&self) -> f32 {
        self.size + self.spread
    }

    /// Get effective gap
    pub fn effective_gap(&self) -> f32 {
        self.gap + self.spread * 0.5
    }

    /// Get color with opacity
    pub fn effective_color(&self) -> [f32; 4] {
        [
            self.color[0],
            self.color[1],
            self.color[2],
            self.color[3] * self.opacity,
        ]
    }

    /// Set hit marker effect (temporary color change)
    pub fn show_hit(&mut self) {
        self.color = [1.0, 0.2, 0.2, 1.0]; // Red flash
    }

    /// Reset to default color
    pub fn reset_color(&mut self) {
        self.color = [1.0, 1.0, 1.0, 1.0];
    }
}

impl Default for Crosshair {
    fn default() -> Self {
        Self::new(CrosshairStyle::Cross)
    }
}

// Preset crosshairs

impl Crosshair {
    /// Simple dot crosshair
    pub fn dot() -> Self {
        Self::new(CrosshairStyle::Dot)
            .with_size(4.0)
    }

    /// Classic cross
    pub fn classic() -> Self {
        Self::new(CrosshairStyle::Cross)
            .with_size(8.0)
            .with_gap(4.0)
            .with_thickness(2.0)
    }

    /// Circle crosshair
    pub fn circle() -> Self {
        Self::new(CrosshairStyle::Circle)
            .with_size(15.0)
            .with_thickness(2.0)
    }

    /// Sniper crosshair
    pub fn sniper() -> Self {
        Self::new(CrosshairStyle::TShape)
            .with_size(20.0)
            .with_gap(8.0)
            .with_thickness(1.0)
            .with_center_dot(2.0)
    }

    /// Dynamic spread crosshair
    pub fn dynamic() -> Self {
        Self::new(CrosshairStyle::Cross)
            .with_size(6.0)
            .with_gap(4.0)
            .with_spread(30.0, 8.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_crosshair() {
        let crosshair = Crosshair::classic()
            .with_color([0.0, 1.0, 0.0, 1.0]);

        assert_eq!(crosshair.style, CrosshairStyle::Cross);
        assert_eq!(crosshair.color, [0.0, 1.0, 0.0, 1.0]);
    }

    #[test]
    fn test_spread() {
        let mut crosshair = Crosshair::dynamic();

        crosshair.add_spread(10.0);
        assert_eq!(crosshair.spread, 10.0);

        // Recovery
        crosshair.update(0.5);
        assert!(crosshair.spread < 10.0);

        // Capped at max
        crosshair.add_spread(100.0);
        assert_eq!(crosshair.spread, crosshair.max_spread);
    }

    #[test]
    fn test_effective_size() {
        let mut crosshair = Crosshair::new(CrosshairStyle::Cross)
            .with_size(10.0);

        assert_eq!(crosshair.effective_size(), 10.0);

        crosshair.add_spread(5.0);
        assert_eq!(crosshair.effective_size(), 15.0);
    }
}
