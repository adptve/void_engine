//! Damage types and information

use serde::{Deserialize, Serialize};

/// Types of damage
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum DamageType {
    /// Physical damage (melee, bullets, falls)
    Physical,
    /// Fire damage
    Fire,
    /// Ice/cold damage
    Ice,
    /// Electric/shock damage
    Electric,
    /// Poison/toxic damage
    Poison,
    /// Energy/plasma damage
    Energy,
    /// True damage (ignores all resistances)
    True,
    /// Custom damage type
    Custom(u32),
}

impl Default for DamageType {
    fn default() -> Self {
        Self::Physical
    }
}

/// Information about a damage instance
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DamageInfo {
    /// Base damage amount
    pub amount: f32,
    /// Type of damage
    pub damage_type: DamageType,
    /// Entity that caused the damage (if any)
    pub source_entity: Option<u64>,
    /// World position where damage was applied
    pub hit_point: Option<[f32; 3]>,
    /// Surface normal at hit point
    pub hit_normal: Option<[f32; 3]>,
    /// Whether this is a critical hit
    pub is_critical: bool,
    /// Critical damage multiplier (if critical)
    pub critical_multiplier: f32,
    /// Whether this damage can be blocked/parried
    pub can_block: bool,
    /// Whether this damage should cause knockback
    pub causes_knockback: bool,
    /// Knockback force magnitude
    pub knockback_force: f32,
}

impl DamageInfo {
    /// Create new damage info
    pub fn new(amount: f32, damage_type: DamageType) -> Self {
        Self {
            amount,
            damage_type,
            source_entity: None,
            hit_point: None,
            hit_normal: None,
            is_critical: false,
            critical_multiplier: 2.0,
            can_block: true,
            causes_knockback: false,
            knockback_force: 0.0,
        }
    }

    /// Set the source entity
    pub fn with_source(mut self, entity: u64) -> Self {
        self.source_entity = Some(entity);
        self
    }

    /// Set the hit point
    pub fn with_hit_point(mut self, point: [f32; 3]) -> Self {
        self.hit_point = Some(point);
        self
    }

    /// Set the hit normal
    pub fn with_hit_normal(mut self, normal: [f32; 3]) -> Self {
        self.hit_normal = Some(normal);
        self
    }

    /// Mark as critical hit
    pub fn with_critical(mut self, multiplier: f32) -> Self {
        self.is_critical = true;
        self.critical_multiplier = multiplier;
        self
    }

    /// Set knockback
    pub fn with_knockback(mut self, force: f32) -> Self {
        self.causes_knockback = true;
        self.knockback_force = force;
        self
    }

    /// Get the final damage amount (including critical)
    pub fn final_amount(&self) -> f32 {
        if self.is_critical {
            self.amount * self.critical_multiplier
        } else {
            self.amount
        }
    }
}

impl Default for DamageInfo {
    fn default() -> Self {
        Self::new(0.0, DamageType::Physical)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_damage_info() {
        let damage = DamageInfo::new(50.0, DamageType::Fire)
            .with_source(123)
            .with_critical(2.5);

        assert_eq!(damage.amount, 50.0);
        assert_eq!(damage.damage_type, DamageType::Fire);
        assert!(damage.is_critical);
        assert_eq!(damage.final_amount(), 125.0);
    }
}
