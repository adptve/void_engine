//! Health and resource bars

use serde::{Deserialize, Serialize};

/// Bar visual style
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BarStyle {
    /// Fill color (RGBA)
    pub fill_color: [f32; 4],
    /// Background color (RGBA)
    pub background_color: [f32; 4],
    /// Border color (RGBA)
    pub border_color: [f32; 4],
    /// Border width
    pub border_width: f32,
    /// Corner radius
    pub corner_radius: f32,
    /// Use gradient
    pub gradient: bool,
    /// Gradient end color (if enabled)
    pub gradient_color: [f32; 4],
}

impl Default for BarStyle {
    fn default() -> Self {
        Self {
            fill_color: [0.2, 0.8, 0.2, 1.0], // Green
            background_color: [0.1, 0.1, 0.1, 0.8],
            border_color: [0.3, 0.3, 0.3, 1.0],
            border_width: 2.0,
            corner_radius: 4.0,
            gradient: false,
            gradient_color: [0.8, 0.2, 0.2, 1.0], // Red
        }
    }
}

impl BarStyle {
    /// Health bar style (green)
    pub fn health() -> Self {
        Self::default()
    }

    /// Mana/energy style (blue)
    pub fn mana() -> Self {
        Self {
            fill_color: [0.2, 0.4, 0.9, 1.0],
            ..Default::default()
        }
    }

    /// Stamina style (yellow)
    pub fn stamina() -> Self {
        Self {
            fill_color: [0.9, 0.8, 0.2, 1.0],
            ..Default::default()
        }
    }

    /// Experience bar style (purple)
    pub fn experience() -> Self {
        Self {
            fill_color: [0.6, 0.2, 0.8, 1.0],
            ..Default::default()
        }
    }

    /// Boss health style (red with border)
    pub fn boss() -> Self {
        Self {
            fill_color: [0.8, 0.1, 0.1, 1.0],
            border_color: [0.9, 0.7, 0.1, 1.0],
            border_width: 3.0,
            ..Default::default()
        }
    }

    /// Enable gradient from current color to specified color
    pub fn with_gradient(mut self, low_color: [f32; 4]) -> Self {
        self.gradient = true;
        self.gradient_color = low_color;
        self
    }
}

/// Health bar component
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthBar {
    /// Screen position X
    pub x: f32,
    /// Screen position Y
    pub y: f32,
    /// Width
    pub width: f32,
    /// Height
    pub height: f32,
    /// Current value
    pub current: f32,
    /// Maximum value
    pub max: f32,
    /// Visual style
    pub style: BarStyle,
    /// Whether to show text
    pub show_text: bool,
    /// Text format ("current/max" or "percent")
    pub text_format: TextFormat,
    /// Animation speed (0 = instant)
    pub animation_speed: f32,
    /// Display value (for smooth animation)
    display_value: f32,
    /// Whether visible
    pub visible: bool,
}

/// Text display format
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TextFormat {
    /// Show "current / max"
    CurrentMax,
    /// Show percentage
    Percent,
    /// Show only current
    Current,
    /// No text
    None,
}

impl Default for TextFormat {
    fn default() -> Self {
        Self::None
    }
}

impl HealthBar {
    /// Create a new health bar
    pub fn new() -> Self {
        Self {
            x: 20.0,
            y: 20.0,
            width: 200.0,
            height: 20.0,
            current: 100.0,
            max: 100.0,
            style: BarStyle::health(),
            show_text: false,
            text_format: TextFormat::None,
            animation_speed: 5.0,
            display_value: 100.0,
            visible: true,
        }
    }

    /// Set position
    pub fn with_position(mut self, x: f32, y: f32) -> Self {
        self.x = x;
        self.y = y;
        self
    }

    /// Set size
    pub fn with_size(mut self, width: f32, height: f32) -> Self {
        self.width = width;
        self.height = height;
        self
    }

    /// Set style
    pub fn with_style(mut self, style: BarStyle) -> Self {
        self.style = style;
        self
    }

    /// Enable text display
    pub fn with_text(mut self, format: TextFormat) -> Self {
        self.show_text = true;
        self.text_format = format;
        self
    }

    /// Set animation speed
    pub fn with_animation(mut self, speed: f32) -> Self {
        self.animation_speed = speed;
        self
    }

    /// Set current and max values
    pub fn set_values(&mut self, current: f32, max: f32) {
        self.current = current.max(0.0);
        self.max = max.max(1.0);
    }

    /// Set current value only
    pub fn set_current(&mut self, current: f32) {
        self.current = current.max(0.0).min(self.max);
    }

    /// Get fill percentage (0.0 - 1.0)
    pub fn percent(&self) -> f32 {
        if self.max <= 0.0 {
            0.0
        } else {
            (self.current / self.max).clamp(0.0, 1.0)
        }
    }

    /// Get display percentage (animated)
    pub fn display_percent(&self) -> f32 {
        if self.max <= 0.0 {
            0.0
        } else {
            (self.display_value / self.max).clamp(0.0, 1.0)
        }
    }

    /// Update animation
    pub fn update(&mut self, delta_time: f32) {
        if self.animation_speed <= 0.0 {
            self.display_value = self.current;
        } else {
            let diff = self.current - self.display_value;
            self.display_value += diff * self.animation_speed * delta_time;

            // Snap if close enough
            if (self.display_value - self.current).abs() < 0.5 {
                self.display_value = self.current;
            }
        }
    }

    /// Get text to display
    pub fn get_text(&self) -> String {
        match self.text_format {
            TextFormat::CurrentMax => format!("{:.0} / {:.0}", self.current, self.max),
            TextFormat::Percent => format!("{:.0}%", self.percent() * 100.0),
            TextFormat::Current => format!("{:.0}", self.current),
            TextFormat::None => String::new(),
        }
    }

    /// Get fill color (considering gradient)
    pub fn get_fill_color(&self) -> [f32; 4] {
        if self.style.gradient {
            let t = self.percent();
            [
                lerp(self.style.gradient_color[0], self.style.fill_color[0], t),
                lerp(self.style.gradient_color[1], self.style.fill_color[1], t),
                lerp(self.style.gradient_color[2], self.style.fill_color[2], t),
                lerp(self.style.gradient_color[3], self.style.fill_color[3], t),
            ]
        } else {
            self.style.fill_color
        }
    }
}

impl Default for HealthBar {
    fn default() -> Self {
        Self::new()
    }
}

/// Resource bar (mana, stamina, etc.)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResourceBar {
    /// Base health bar
    pub bar: HealthBar,
    /// Resource name
    pub name: String,
    /// Regeneration rate (per second)
    pub regen_rate: f32,
    /// Whether regenerating
    pub regenerating: bool,
}

impl ResourceBar {
    /// Create a new resource bar
    pub fn new(name: impl Into<String>, max: f32) -> Self {
        let mut bar = HealthBar::new();
        bar.max = max;
        bar.current = max;
        bar.display_value = max;

        Self {
            bar,
            name: name.into(),
            regen_rate: 0.0,
            regenerating: true,
        }
    }

    /// Set style
    pub fn with_style(mut self, style: BarStyle) -> Self {
        self.bar.style = style;
        self
    }

    /// Set regen rate
    pub fn with_regen(mut self, rate: f32) -> Self {
        self.regen_rate = rate;
        self
    }

    /// Consume resource
    pub fn consume(&mut self, amount: f32) -> bool {
        if self.bar.current >= amount {
            self.bar.current -= amount;
            true
        } else {
            false
        }
    }

    /// Update with regeneration
    pub fn update(&mut self, delta_time: f32) {
        if self.regenerating && self.regen_rate > 0.0 {
            self.bar.current = (self.bar.current + self.regen_rate * delta_time).min(self.bar.max);
        }
        self.bar.update(delta_time);
    }
}

/// Linear interpolation
fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_health_bar() {
        let mut bar = HealthBar::new()
            .with_position(10.0, 10.0)
            .with_size(100.0, 20.0);

        bar.set_values(75.0, 100.0);
        assert_eq!(bar.percent(), 0.75);

        bar.set_current(50.0);
        assert_eq!(bar.percent(), 0.5);
    }

    #[test]
    fn test_bar_animation() {
        let mut bar = HealthBar::new().with_animation(5.0);

        bar.set_values(100.0, 100.0);
        bar.display_value = 100.0;

        bar.set_current(50.0);
        bar.update(0.05);

        // Should be animating toward 50 (moved by diff * speed * dt = -50 * 5 * 0.05 = -12.5)
        assert!(bar.display_value < 100.0);
        assert!(bar.display_value > 50.0);
    }

    #[test]
    fn test_bar_text() {
        let mut bar = HealthBar::new().with_text(TextFormat::CurrentMax);
        bar.set_values(75.0, 100.0);

        assert_eq!(bar.get_text(), "75 / 100");

        bar.text_format = TextFormat::Percent;
        assert_eq!(bar.get_text(), "75%");
    }

    #[test]
    fn test_resource_bar() {
        let mut resource = ResourceBar::new("Mana", 100.0)
            .with_style(BarStyle::mana())
            .with_regen(10.0);

        assert!(resource.consume(30.0));
        assert_eq!(resource.bar.current, 70.0);

        assert!(!resource.consume(100.0)); // Not enough
        assert_eq!(resource.bar.current, 70.0);

        resource.update(1.0);
        assert_eq!(resource.bar.current, 80.0); // Regenerated
    }
}
