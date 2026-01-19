//! Physics materials defining surface properties

use serde::{Deserialize, Serialize};

/// Physics material defining friction and restitution
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct PhysicsMaterial {
    /// Friction coefficient (0 = frictionless, 1 = high friction)
    pub friction: f32,
    /// Restitution/bounciness (0 = no bounce, 1 = perfect bounce)
    pub restitution: f32,
    /// Density for mass calculation (kg/m³)
    pub density: f32,
    /// How friction is combined between two colliders
    pub friction_combine: CombineRule,
    /// How restitution is combined between two colliders
    pub restitution_combine: CombineRule,
}

impl Default for PhysicsMaterial {
    fn default() -> Self {
        Self {
            friction: 0.5,
            restitution: 0.0,
            density: 1.0,
            friction_combine: CombineRule::Average,
            restitution_combine: CombineRule::Average,
        }
    }
}

impl PhysicsMaterial {
    /// Create a new physics material
    pub fn new(friction: f32, restitution: f32) -> Self {
        Self {
            friction,
            restitution,
            ..Default::default()
        }
    }

    /// Frictionless ice-like material
    pub fn ice() -> Self {
        Self {
            friction: 0.05,
            restitution: 0.0,
            density: 0.9,
            ..Default::default()
        }
    }

    /// Bouncy rubber-like material
    pub fn rubber() -> Self {
        Self {
            friction: 0.8,
            restitution: 0.8,
            density: 1.1,
            ..Default::default()
        }
    }

    /// Metal material
    pub fn metal() -> Self {
        Self {
            friction: 0.3,
            restitution: 0.2,
            density: 7.8,
            ..Default::default()
        }
    }

    /// Wood material
    pub fn wood() -> Self {
        Self {
            friction: 0.5,
            restitution: 0.3,
            density: 0.6,
            ..Default::default()
        }
    }

    /// Stone/concrete material
    pub fn stone() -> Self {
        Self {
            friction: 0.7,
            restitution: 0.1,
            density: 2.5,
            ..Default::default()
        }
    }

    /// Set friction
    pub fn with_friction(mut self, friction: f32) -> Self {
        self.friction = friction.clamp(0.0, 1.0);
        self
    }

    /// Set restitution
    pub fn with_restitution(mut self, restitution: f32) -> Self {
        self.restitution = restitution.clamp(0.0, 1.0);
        self
    }

    /// Set density
    pub fn with_density(mut self, density: f32) -> Self {
        self.density = density.max(0.001);
        self
    }

    /// Combine two materials to get effective friction
    pub fn combine_friction(&self, other: &PhysicsMaterial) -> f32 {
        let rule = self.friction_combine.max_priority(other.friction_combine);
        rule.combine(self.friction, other.friction)
    }

    /// Combine two materials to get effective restitution
    pub fn combine_restitution(&self, other: &PhysicsMaterial) -> f32 {
        let rule = self.restitution_combine.max_priority(other.restitution_combine);
        rule.combine(self.restitution, other.restitution)
    }
}

/// Rule for combining material properties
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CombineRule {
    /// Use the average of both values
    Average,
    /// Use the minimum value
    Min,
    /// Use the maximum value
    Max,
    /// Multiply the values
    Multiply,
}

impl Default for CombineRule {
    fn default() -> Self {
        Self::Average
    }
}

impl CombineRule {
    /// Get the higher priority rule
    pub fn max_priority(self, other: Self) -> Self {
        use CombineRule::*;
        match (self, other) {
            (Average, _) => other,
            (_, Average) => self,
            (Min, _) => Min,
            (_, Min) => Min,
            (Multiply, _) => Multiply,
            (_, Multiply) => Multiply,
            (Max, Max) => Max,
        }
    }

    /// Combine two values using this rule
    pub fn combine(self, a: f32, b: f32) -> f32 {
        match self {
            Self::Average => (a + b) * 0.5,
            Self::Min => a.min(b),
            Self::Max => a.max(b),
            Self::Multiply => a * b,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_material_combine() {
        let rubber = PhysicsMaterial::rubber();
        let metal = PhysicsMaterial::metal();

        let friction = rubber.combine_friction(&metal);
        assert!(friction > 0.0 && friction < 1.0);

        let restitution = rubber.combine_restitution(&metal);
        assert!(restitution > 0.0 && restitution < 1.0);
    }
}
