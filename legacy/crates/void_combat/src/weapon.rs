//! Weapon system

use crate::damage::DamageType;
use serde::{Deserialize, Serialize};

/// Type of weapon
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum WeaponType {
    /// Instant hit (raycast)
    Hitscan,
    /// Spawns a projectile entity
    Projectile,
    /// Close range attack
    Melee,
}

impl Default for WeaponType {
    fn default() -> Self {
        Self::Hitscan
    }
}

/// Weapon statistics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WeaponStats {
    /// Base damage per hit
    pub damage: f32,
    /// Damage type
    pub damage_type: DamageType,
    /// Fire rate (shots per second)
    pub fire_rate: f32,
    /// Range in world units
    pub range: f32,
    /// Spread angle in degrees (for hitscan)
    pub spread: f32,
    /// Number of pellets per shot (for shotguns)
    pub pellets: u32,
    /// Magazine size (0 = infinite)
    pub magazine_size: u32,
    /// Reload time in seconds
    pub reload_time: f32,
    /// Critical hit chance (0.0 - 1.0)
    pub crit_chance: f32,
    /// Critical hit multiplier
    pub crit_multiplier: f32,
}

impl Default for WeaponStats {
    fn default() -> Self {
        Self {
            damage: 10.0,
            damage_type: DamageType::Physical,
            fire_rate: 5.0,
            range: 100.0,
            spread: 0.0,
            pellets: 1,
            magazine_size: 30,
            reload_time: 2.0,
            crit_chance: 0.1,
            crit_multiplier: 2.0,
        }
    }
}

/// Projectile configuration (for projectile weapons)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProjectileConfig {
    /// Projectile speed
    pub speed: f32,
    /// Gravity scale (0 = no gravity)
    pub gravity_scale: f32,
    /// Lifetime in seconds
    pub lifetime: f32,
    /// Splash damage radius (0 = no splash)
    pub splash_radius: f32,
    /// Splash damage falloff type
    pub splash_falloff: SplashFalloff,
    /// Prefab/entity path to spawn
    pub prefab: String,
}

impl Default for ProjectileConfig {
    fn default() -> Self {
        Self {
            speed: 50.0,
            gravity_scale: 0.0,
            lifetime: 10.0,
            splash_radius: 0.0,
            splash_falloff: SplashFalloff::Linear,
            prefab: String::new(),
        }
    }
}

/// Melee configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MeleeConfig {
    /// Swing arc in degrees
    pub arc: f32,
    /// Swing time in seconds
    pub swing_time: f32,
    /// Combo window (time to chain attacks)
    pub combo_window: f32,
    /// Maximum combo count
    pub max_combo: u32,
    /// Damage multiplier per combo hit
    pub combo_multiplier: f32,
}

impl Default for MeleeConfig {
    fn default() -> Self {
        Self {
            arc: 90.0,
            swing_time: 0.3,
            combo_window: 0.5,
            max_combo: 3,
            combo_multiplier: 1.2,
        }
    }
}

/// Splash damage falloff type
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SplashFalloff {
    /// No falloff (full damage at all distances)
    None,
    /// Linear falloff
    Linear,
    /// Exponential falloff
    Exponential,
}

impl Default for SplashFalloff {
    fn default() -> Self {
        Self::Linear
    }
}

/// Weapon component for entities
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WeaponComponent {
    /// Weapon name/id
    pub name: String,
    /// Weapon type
    pub weapon_type: WeaponType,
    /// Weapon stats
    pub stats: WeaponStats,
    /// Projectile config (for projectile weapons)
    pub projectile: Option<ProjectileConfig>,
    /// Melee config (for melee weapons)
    pub melee: Option<MeleeConfig>,
    /// Current ammo in magazine
    #[serde(skip)]
    pub current_ammo: u32,
    /// Time until can fire again
    #[serde(skip)]
    pub cooldown: f32,
    /// Whether currently reloading
    #[serde(skip)]
    pub is_reloading: bool,
    /// Reload progress (0.0 - 1.0)
    #[serde(skip)]
    pub reload_progress: f32,
    /// Current combo count (melee)
    #[serde(skip)]
    pub combo_count: u32,
    /// Time since last attack (for combo)
    #[serde(skip)]
    pub time_since_attack: f32,
}

impl WeaponComponent {
    /// Create a new weapon
    pub fn new(name: impl Into<String>, weapon_type: WeaponType) -> Self {
        let stats = WeaponStats::default();
        let current_ammo = stats.magazine_size;

        Self {
            name: name.into(),
            weapon_type,
            stats,
            projectile: if weapon_type == WeaponType::Projectile {
                Some(ProjectileConfig::default())
            } else {
                None
            },
            melee: if weapon_type == WeaponType::Melee {
                Some(MeleeConfig::default())
            } else {
                None
            },
            current_ammo,
            cooldown: 0.0,
            is_reloading: false,
            reload_progress: 0.0,
            combo_count: 0,
            time_since_attack: f32::MAX,
        }
    }

    /// Create a hitscan weapon
    pub fn hitscan(name: impl Into<String>) -> Self {
        Self::new(name, WeaponType::Hitscan)
    }

    /// Create a projectile weapon
    pub fn projectile(name: impl Into<String>) -> Self {
        Self::new(name, WeaponType::Projectile)
    }

    /// Create a melee weapon
    pub fn melee(name: impl Into<String>) -> Self {
        Self::new(name, WeaponType::Melee)
    }

    /// Set weapon stats
    pub fn with_stats(mut self, stats: WeaponStats) -> Self {
        self.stats = stats;
        self.current_ammo = self.stats.magazine_size;
        self
    }

    /// Set damage
    pub fn with_damage(mut self, damage: f32, damage_type: DamageType) -> Self {
        self.stats.damage = damage;
        self.stats.damage_type = damage_type;
        self
    }

    /// Set fire rate
    pub fn with_fire_rate(mut self, rate: f32) -> Self {
        self.stats.fire_rate = rate;
        self
    }

    /// Set range
    pub fn with_range(mut self, range: f32) -> Self {
        self.stats.range = range;
        self
    }

    /// Check if can fire
    pub fn can_fire(&self) -> bool {
        !self.is_reloading
            && self.cooldown <= 0.0
            && (self.stats.magazine_size == 0 || self.current_ammo > 0)
    }

    /// Attempt to fire the weapon
    /// Returns true if fired
    pub fn fire(&mut self) -> bool {
        if !self.can_fire() {
            return false;
        }

        // Consume ammo
        if self.stats.magazine_size > 0 {
            self.current_ammo = self.current_ammo.saturating_sub(1);
        }

        // Set cooldown
        self.cooldown = 1.0 / self.stats.fire_rate;

        // Update combo for melee
        if self.weapon_type == WeaponType::Melee {
            if let Some(ref melee) = self.melee {
                if self.time_since_attack <= melee.combo_window {
                    self.combo_count = (self.combo_count + 1).min(melee.max_combo);
                } else {
                    self.combo_count = 1;
                }
            }
            self.time_since_attack = 0.0;
        }

        true
    }

    /// Start reloading
    pub fn reload(&mut self) -> bool {
        if self.is_reloading || self.current_ammo >= self.stats.magazine_size {
            return false;
        }

        self.is_reloading = true;
        self.reload_progress = 0.0;
        true
    }

    /// Cancel reload
    pub fn cancel_reload(&mut self) {
        self.is_reloading = false;
        self.reload_progress = 0.0;
    }

    /// Update weapon state
    pub fn update(&mut self, delta_time: f32) {
        // Update cooldown
        if self.cooldown > 0.0 {
            self.cooldown -= delta_time;
        }

        // Update reload
        if self.is_reloading {
            self.reload_progress += delta_time / self.stats.reload_time;
            if self.reload_progress >= 1.0 {
                self.current_ammo = self.stats.magazine_size;
                self.is_reloading = false;
                self.reload_progress = 0.0;
            }
        }

        // Update melee combo timer
        if self.weapon_type == WeaponType::Melee {
            self.time_since_attack += delta_time;
            if let Some(ref melee) = self.melee {
                if self.time_since_attack > melee.combo_window {
                    self.combo_count = 0;
                }
            }
        }
    }

    /// Get current damage (including combo multiplier for melee)
    pub fn get_damage(&self) -> f32 {
        let mut damage = self.stats.damage;

        if self.weapon_type == WeaponType::Melee && self.combo_count > 1 {
            if let Some(ref melee) = self.melee {
                damage *= melee.combo_multiplier.powf((self.combo_count - 1) as f32);
            }
        }

        damage
    }

    /// Check if critical hit (random)
    pub fn roll_critical(&self) -> bool {
        rand_float() < self.stats.crit_chance
    }

    /// Get ammo display string
    pub fn ammo_display(&self) -> String {
        if self.stats.magazine_size == 0 {
            "∞".to_string()
        } else {
            format!("{}/{}", self.current_ammo, self.stats.magazine_size)
        }
    }
}

impl Default for WeaponComponent {
    fn default() -> Self {
        Self::hitscan("Default Weapon")
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
    fn test_weapon_creation() {
        let weapon = WeaponComponent::hitscan("Pistol")
            .with_damage(25.0, DamageType::Physical)
            .with_fire_rate(3.0);

        assert_eq!(weapon.stats.damage, 25.0);
        assert_eq!(weapon.stats.fire_rate, 3.0);
        assert!(weapon.can_fire());
    }

    #[test]
    fn test_weapon_firing() {
        let mut weapon = WeaponComponent::hitscan("Pistol")
            .with_fire_rate(10.0);

        weapon.stats.magazine_size = 5;
        weapon.current_ammo = 5;

        assert!(weapon.fire());
        assert_eq!(weapon.current_ammo, 4);
        assert!(!weapon.can_fire()); // On cooldown

        weapon.cooldown = 0.0;
        assert!(weapon.can_fire());
    }

    #[test]
    fn test_reload() {
        let mut weapon = WeaponComponent::hitscan("Pistol");
        weapon.stats.magazine_size = 10;
        weapon.current_ammo = 2;

        assert!(weapon.reload());
        assert!(weapon.is_reloading);

        // Simulate reload completion
        weapon.update(weapon.stats.reload_time + 0.1);
        assert!(!weapon.is_reloading);
        assert_eq!(weapon.current_ammo, 10);
    }
}
