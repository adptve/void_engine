//! Health component and management

use crate::damage::{DamageInfo, DamageType};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Events emitted by the health system
#[derive(Debug, Clone)]
pub enum HealthEvent {
    /// Damage was taken
    DamageTaken {
        entity: u64,
        damage: DamageInfo,
        new_health: f32,
    },
    /// Entity was healed
    Healed {
        entity: u64,
        amount: f32,
        new_health: f32,
    },
    /// Entity died
    Death {
        entity: u64,
        killer: Option<u64>,
        damage_type: DamageType,
    },
    /// Entity respawned
    Respawned {
        entity: u64,
        new_health: f32,
    },
    /// Invulnerability started
    InvulnerabilityStarted {
        entity: u64,
        duration: f32,
    },
    /// Invulnerability ended
    InvulnerabilityEnded {
        entity: u64,
    },
}

/// Health component for entities
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthComponent {
    /// Current health
    pub current: f32,
    /// Maximum health
    pub max: f32,
    /// Health regeneration per second (0 = no regen)
    pub regeneration: f32,
    /// Delay before regeneration starts (after taking damage)
    pub regen_delay: f32,
    /// Time since last damage (for regen delay)
    #[serde(skip)]
    pub time_since_damage: f32,
    /// Whether the entity is invulnerable
    pub invulnerable: bool,
    /// Remaining invulnerability time
    #[serde(skip)]
    pub invulnerability_timer: f32,
    /// Invulnerability duration after taking damage (i-frames)
    pub invulnerability_on_hit: f32,
    /// Damage resistances per type (multiplier: 0 = immune, 1 = normal, 2 = double damage)
    pub resistances: HashMap<DamageType, f32>,
    /// Whether this entity is dead
    #[serde(skip)]
    pub is_dead: bool,
    /// Whether to respawn after death
    pub can_respawn: bool,
    /// Respawn delay in seconds
    pub respawn_delay: f32,
    /// Time until respawn
    #[serde(skip)]
    pub respawn_timer: f32,
}

impl HealthComponent {
    /// Create a new health component
    pub fn new(max_health: f32) -> Self {
        Self {
            current: max_health,
            max: max_health,
            regeneration: 0.0,
            regen_delay: 0.0,
            time_since_damage: f32::MAX,
            invulnerable: false,
            invulnerability_timer: 0.0,
            invulnerability_on_hit: 0.0,
            resistances: HashMap::new(),
            is_dead: false,
            can_respawn: false,
            respawn_delay: 3.0,
            respawn_timer: 0.0,
        }
    }

    /// Set regeneration rate
    pub fn with_regeneration(mut self, rate: f32) -> Self {
        self.regeneration = rate;
        self
    }

    /// Set regeneration delay
    pub fn with_regen_delay(mut self, delay: f32) -> Self {
        self.regen_delay = delay;
        self
    }

    /// Set invulnerability time after hit
    pub fn with_invulnerability_on_hit(mut self, duration: f32) -> Self {
        self.invulnerability_on_hit = duration;
        self
    }

    /// Set resistance to a damage type
    pub fn with_resistance(mut self, damage_type: DamageType, multiplier: f32) -> Self {
        self.resistances.insert(damage_type, multiplier);
        self
    }

    /// Enable respawning
    pub fn with_respawn(mut self, delay: f32) -> Self {
        self.can_respawn = true;
        self.respawn_delay = delay;
        self
    }

    /// Get the resistance multiplier for a damage type
    pub fn get_resistance(&self, damage_type: DamageType) -> f32 {
        // True damage ignores resistances
        if damage_type == DamageType::True {
            return 1.0;
        }
        *self.resistances.get(&damage_type).unwrap_or(&1.0)
    }

    /// Calculate the actual damage after resistances
    pub fn calculate_damage(&self, damage: &DamageInfo) -> f32 {
        let base = damage.final_amount();
        let resistance = self.get_resistance(damage.damage_type);
        (base * resistance).max(0.0)
    }

    /// Apply damage to this health component
    /// Returns the actual damage dealt and whether the entity died
    pub fn apply_damage(&mut self, damage: &DamageInfo) -> (f32, bool) {
        if self.is_dead || self.invulnerable {
            return (0.0, false);
        }

        let actual_damage = self.calculate_damage(damage);
        self.current = (self.current - actual_damage).max(0.0);
        self.time_since_damage = 0.0;

        // Apply i-frames
        if self.invulnerability_on_hit > 0.0 {
            self.invulnerable = true;
            self.invulnerability_timer = self.invulnerability_on_hit;
        }

        let died = self.current <= 0.0;
        if died {
            self.is_dead = true;
            if self.can_respawn {
                self.respawn_timer = self.respawn_delay;
            }
        }

        (actual_damage, died)
    }

    /// Heal the entity
    /// Returns the actual amount healed
    pub fn heal(&mut self, amount: f32) -> f32 {
        if self.is_dead {
            return 0.0;
        }

        let old_health = self.current;
        self.current = (self.current + amount).min(self.max);
        self.current - old_health
    }

    /// Set health directly (clamped to 0..max)
    pub fn set_health(&mut self, health: f32) {
        self.current = health.clamp(0.0, self.max);
        self.is_dead = self.current <= 0.0;
    }

    /// Update the health component (call once per frame)
    pub fn update(&mut self, delta_time: f32) -> Vec<HealthEvent> {
        let mut events = Vec::new();

        // Update invulnerability
        if self.invulnerability_timer > 0.0 {
            self.invulnerability_timer -= delta_time;
            if self.invulnerability_timer <= 0.0 {
                self.invulnerable = false;
                self.invulnerability_timer = 0.0;
            }
        }

        // Update respawn timer
        if self.is_dead && self.can_respawn {
            self.respawn_timer -= delta_time;
            if self.respawn_timer <= 0.0 {
                self.respawn();
            }
        }

        // Update regeneration
        if !self.is_dead && self.regeneration > 0.0 && self.current < self.max {
            self.time_since_damage += delta_time;
            if self.time_since_damage >= self.regen_delay {
                let heal_amount = self.regeneration * delta_time;
                self.heal(heal_amount);
            }
        }

        events
    }

    /// Respawn the entity
    pub fn respawn(&mut self) {
        self.is_dead = false;
        self.current = self.max;
        self.invulnerable = false;
        self.invulnerability_timer = 0.0;
        self.time_since_damage = f32::MAX;
    }

    /// Get health as a percentage (0.0 - 1.0)
    pub fn health_percent(&self) -> f32 {
        if self.max <= 0.0 {
            return 0.0;
        }
        self.current / self.max
    }

    /// Check if at full health
    pub fn is_full(&self) -> bool {
        self.current >= self.max
    }

    /// Check if alive
    pub fn is_alive(&self) -> bool {
        !self.is_dead
    }
}

impl Default for HealthComponent {
    fn default() -> Self {
        Self::new(100.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_health_component() {
        let mut health = HealthComponent::new(100.0)
            .with_resistance(DamageType::Fire, 0.5);

        assert_eq!(health.current, 100.0);
        assert!(!health.is_dead);

        // Apply physical damage
        let damage = DamageInfo::new(30.0, DamageType::Physical);
        let (dealt, died) = health.apply_damage(&damage);
        assert_eq!(dealt, 30.0);
        assert!(!died);
        assert_eq!(health.current, 70.0);

        // Apply fire damage (50% resistance)
        let fire_damage = DamageInfo::new(40.0, DamageType::Fire);
        let (dealt, died) = health.apply_damage(&fire_damage);
        assert_eq!(dealt, 20.0);
        assert!(!died);
        assert_eq!(health.current, 50.0);
    }

    #[test]
    fn test_death() {
        let mut health = HealthComponent::new(50.0);

        let damage = DamageInfo::new(100.0, DamageType::Physical);
        let (_, died) = health.apply_damage(&damage);

        assert!(died);
        assert!(health.is_dead);
        assert_eq!(health.current, 0.0);
    }

    #[test]
    fn test_healing() {
        let mut health = HealthComponent::new(100.0);
        health.set_health(50.0);

        let healed = health.heal(30.0);
        assert_eq!(healed, 30.0);
        assert_eq!(health.current, 80.0);

        // Can't overheal
        let healed = health.heal(50.0);
        assert_eq!(healed, 20.0);
        assert_eq!(health.current, 100.0);
    }
}
