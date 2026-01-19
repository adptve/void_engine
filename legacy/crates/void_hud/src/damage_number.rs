//! Floating damage numbers

use serde::{Deserialize, Serialize};

/// Damage number visual style
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DamageNumberStyle {
    /// Text color (RGBA)
    pub color: [f32; 4],
    /// Critical hit color
    pub crit_color: [f32; 4],
    /// Heal color
    pub heal_color: [f32; 4],
    /// Font size
    pub font_size: f32,
    /// Critical font size multiplier
    pub crit_scale: f32,
    /// Rise speed (units per second)
    pub rise_speed: f32,
    /// Horizontal drift range
    pub drift_range: f32,
    /// Lifetime in seconds
    pub lifetime: f32,
    /// Fade out duration
    pub fade_duration: f32,
    /// Whether to show outline
    pub outline: bool,
    /// Outline color
    pub outline_color: [f32; 4],
    /// Scale animation (bounce effect)
    pub scale_animation: bool,
}

impl Default for DamageNumberStyle {
    fn default() -> Self {
        Self {
            color: [1.0, 1.0, 1.0, 1.0],
            crit_color: [1.0, 0.8, 0.0, 1.0], // Gold
            heal_color: [0.2, 1.0, 0.2, 1.0], // Green
            font_size: 20.0,
            crit_scale: 1.5,
            rise_speed: 50.0,
            drift_range: 20.0,
            lifetime: 1.5,
            fade_duration: 0.5,
            outline: true,
            outline_color: [0.0, 0.0, 0.0, 1.0],
            scale_animation: true,
        }
    }
}

impl DamageNumberStyle {
    /// Combat style (white damage, gold crits)
    pub fn combat() -> Self {
        Self::default()
    }

    /// RPG style (larger, more dramatic)
    pub fn rpg() -> Self {
        Self {
            font_size: 24.0,
            crit_scale: 2.0,
            rise_speed: 80.0,
            lifetime: 2.0,
            ..Default::default()
        }
    }

    /// Subtle style (smaller, quicker)
    pub fn subtle() -> Self {
        Self {
            font_size: 14.0,
            rise_speed: 30.0,
            lifetime: 1.0,
            scale_animation: false,
            ..Default::default()
        }
    }
}

/// Type of damage number
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DamageNumberType {
    /// Regular damage
    Damage,
    /// Critical hit
    Critical,
    /// Healing
    Heal,
    /// Shield damage
    Shield,
    /// Blocked/absorbed
    Blocked,
    /// Miss
    Miss,
}

/// A floating damage number
#[derive(Debug, Clone)]
pub struct DamageNumber {
    /// The value to display
    pub value: f32,
    /// Type of damage
    pub number_type: DamageNumberType,
    /// World position
    pub world_position: [f32; 3],
    /// Current screen position (calculated from world pos)
    pub screen_position: [f32; 2],
    /// Horizontal offset (drift)
    pub offset_x: f32,
    /// Current offset Y (rise)
    pub offset_y: f32,
    /// Current scale
    pub scale: f32,
    /// Current opacity
    pub opacity: f32,
    /// Time alive
    pub age: f32,
    /// Style
    pub style: DamageNumberStyle,
    /// Whether still active
    pub active: bool,
}

impl DamageNumber {
    /// Create a new damage number
    pub fn new(value: f32, world_pos: [f32; 3], style: DamageNumberStyle) -> Self {
        // Random horizontal drift
        let drift = (rand_float() - 0.5) * 2.0 * style.drift_range;

        Self {
            value,
            number_type: DamageNumberType::Damage,
            world_position: world_pos,
            screen_position: [0.0, 0.0],
            offset_x: drift,
            offset_y: 0.0,
            scale: if style.scale_animation { 1.5 } else { 1.0 },
            opacity: 1.0,
            age: 0.0,
            style,
            active: true,
        }
    }

    /// Set as critical hit
    pub fn as_critical(mut self) -> Self {
        self.number_type = DamageNumberType::Critical;
        self.scale *= self.style.crit_scale;
        self
    }

    /// Set as heal
    pub fn as_heal(mut self) -> Self {
        self.number_type = DamageNumberType::Heal;
        self
    }

    /// Set as blocked
    pub fn as_blocked(mut self) -> Self {
        self.number_type = DamageNumberType::Blocked;
        self
    }

    /// Update the damage number
    pub fn update(&mut self, delta_time: f32) {
        if !self.active {
            return;
        }

        self.age += delta_time;

        // Rise
        self.offset_y += self.style.rise_speed * delta_time;

        // Scale animation (bounce in)
        if self.style.scale_animation {
            let target_scale = if self.number_type == DamageNumberType::Critical {
                self.style.crit_scale
            } else {
                1.0
            };
            self.scale += (target_scale - self.scale) * 10.0 * delta_time;
        }

        // Fade out
        let fade_start = self.style.lifetime - self.style.fade_duration;
        if self.age >= fade_start {
            let fade_progress = (self.age - fade_start) / self.style.fade_duration;
            self.opacity = 1.0 - fade_progress;
        }

        // Deactivate when done
        if self.age >= self.style.lifetime {
            self.active = false;
        }
    }

    /// Get the color for this damage number
    pub fn get_color(&self) -> [f32; 4] {
        let base_color = match self.number_type {
            DamageNumberType::Critical => self.style.crit_color,
            DamageNumberType::Heal => self.style.heal_color,
            DamageNumberType::Shield => [0.2, 0.6, 1.0, 1.0], // Blue
            DamageNumberType::Blocked => [0.5, 0.5, 0.5, 1.0], // Gray
            DamageNumberType::Miss => [0.5, 0.5, 0.5, 1.0],
            DamageNumberType::Damage => self.style.color,
        };

        [
            base_color[0],
            base_color[1],
            base_color[2],
            base_color[3] * self.opacity,
        ]
    }

    /// Get display text
    pub fn get_text(&self) -> String {
        match self.number_type {
            DamageNumberType::Miss => "MISS".to_string(),
            DamageNumberType::Blocked => "BLOCKED".to_string(),
            DamageNumberType::Heal => format!("+{:.0}", self.value),
            _ => format!("{:.0}", self.value),
        }
    }

    /// Get font size
    pub fn get_font_size(&self) -> f32 {
        self.style.font_size * self.scale
    }
}

/// Simple random float (0.0 - 1.0)
fn rand_float() -> f32 {
    use std::time::{SystemTime, UNIX_EPOCH};
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .subsec_nanos();
    (nanos % 1000) as f32 / 1000.0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_damage_number() {
        let dn = DamageNumber::new(50.0, [0.0, 0.0, 0.0], DamageNumberStyle::default());

        assert_eq!(dn.value, 50.0);
        assert!(dn.active);
        assert_eq!(dn.get_text(), "50");
    }

    #[test]
    fn test_critical() {
        let dn = DamageNumber::new(100.0, [0.0, 0.0, 0.0], DamageNumberStyle::default())
            .as_critical();

        assert_eq!(dn.number_type, DamageNumberType::Critical);
    }

    #[test]
    fn test_heal() {
        let dn = DamageNumber::new(25.0, [0.0, 0.0, 0.0], DamageNumberStyle::default())
            .as_heal();

        assert_eq!(dn.get_text(), "+25");
    }

    #[test]
    fn test_update() {
        let mut dn = DamageNumber::new(50.0, [0.0, 0.0, 0.0], DamageNumberStyle::default());

        dn.update(0.5);
        assert!(dn.offset_y > 0.0); // Should have risen
        assert!(dn.active);

        dn.update(2.0); // Past lifetime
        assert!(!dn.active);
    }
}
