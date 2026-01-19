//! Render Pass Flags
//!
//! Flags indicating which render passes an entity participates in.

use serde::{Deserialize, Serialize};

/// Flags indicating which render passes an entity participates in
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct RenderPassFlags(u32);

impl RenderPassFlags {
    /// No passes
    pub const NONE: Self = Self(0);

    /// Main color pass
    pub const MAIN: Self = Self(1 << 0);

    /// Depth prepass (early Z)
    pub const DEPTH_PREPASS: Self = Self(1 << 1);

    /// Shadow map passes
    pub const SHADOW: Self = Self(1 << 2);

    /// GBuffer pass (deferred rendering)
    pub const GBUFFER: Self = Self(1 << 3);

    /// Transparent pass
    pub const TRANSPARENT: Self = Self(1 << 4);

    /// Reflection pass
    pub const REFLECTION: Self = Self(1 << 5);

    /// Refraction pass
    pub const REFRACTION: Self = Self(1 << 6);

    /// Motion vectors pass
    pub const MOTION_VECTORS: Self = Self(1 << 7);

    /// Velocity buffer pass
    pub const VELOCITY: Self = Self(1 << 8);

    /// Outline pass
    pub const OUTLINE: Self = Self(1 << 9);

    /// Selection highlight pass
    pub const SELECTION: Self = Self(1 << 10);

    /// Custom pass 0
    pub const CUSTOM_0: Self = Self(1 << 16);

    /// Custom pass 1
    pub const CUSTOM_1: Self = Self(1 << 17);

    /// Custom pass 2
    pub const CUSTOM_2: Self = Self(1 << 18);

    /// Custom pass 3
    pub const CUSTOM_3: Self = Self(1 << 19);

    /// All standard opaque passes (main + depth prepass + shadow)
    pub const OPAQUE: Self = Self(Self::MAIN.0 | Self::DEPTH_PREPASS.0 | Self::SHADOW.0);

    /// All passes
    pub const ALL: Self = Self(u32::MAX);

    /// Create flags from raw bits
    #[inline]
    pub const fn from_bits(bits: u32) -> Self {
        Self(bits)
    }

    /// Get raw bits
    #[inline]
    pub const fn bits(self) -> u32 {
        self.0
    }

    /// Check if empty (no flags set)
    #[inline]
    pub const fn is_empty(self) -> bool {
        self.0 == 0
    }

    /// Check if all specified flags are set
    #[inline]
    pub const fn contains(self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }

    /// Check if any of the specified flags are set
    #[inline]
    pub const fn intersects(self, other: Self) -> bool {
        (self.0 & other.0) != 0
    }

    /// Insert flags
    #[inline]
    pub fn insert(&mut self, other: Self) {
        self.0 |= other.0;
    }

    /// Remove flags
    #[inline]
    pub fn remove(&mut self, other: Self) {
        self.0 &= !other.0;
    }

    /// Toggle flags
    #[inline]
    pub fn toggle(&mut self, other: Self) {
        self.0 ^= other.0;
    }

    /// Set flags based on condition
    #[inline]
    pub fn set(&mut self, other: Self, value: bool) {
        if value {
            self.insert(other);
        } else {
            self.remove(other);
        }
    }

    /// Union of two flag sets
    #[inline]
    pub const fn union(self, other: Self) -> Self {
        Self(self.0 | other.0)
    }

    /// Intersection of two flag sets
    #[inline]
    pub const fn intersection(self, other: Self) -> Self {
        Self(self.0 & other.0)
    }

    /// Difference of two flag sets
    #[inline]
    pub const fn difference(self, other: Self) -> Self {
        Self(self.0 & !other.0)
    }

    /// Complement (all flags not in self)
    #[inline]
    pub const fn complement(self) -> Self {
        Self(!self.0)
    }
}

impl core::ops::BitOr for RenderPassFlags {
    type Output = Self;

    #[inline]
    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

impl core::ops::BitOrAssign for RenderPassFlags {
    #[inline]
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl core::ops::BitAnd for RenderPassFlags {
    type Output = Self;

    #[inline]
    fn bitand(self, rhs: Self) -> Self::Output {
        Self(self.0 & rhs.0)
    }
}

impl core::ops::BitAndAssign for RenderPassFlags {
    #[inline]
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0;
    }
}

impl core::ops::BitXor for RenderPassFlags {
    type Output = Self;

    #[inline]
    fn bitxor(self, rhs: Self) -> Self::Output {
        Self(self.0 ^ rhs.0)
    }
}

impl core::ops::BitXorAssign for RenderPassFlags {
    #[inline]
    fn bitxor_assign(&mut self, rhs: Self) {
        self.0 ^= rhs.0;
    }
}

impl core::ops::Not for RenderPassFlags {
    type Output = Self;

    #[inline]
    fn not(self) -> Self::Output {
        Self(!self.0)
    }
}

/// Pass identifier for indexing
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum PassId {
    Main,
    DepthPrepass,
    Shadow,
    GBuffer,
    Transparent,
    Reflection,
    Refraction,
    MotionVectors,
    Velocity,
    Outline,
    Selection,
    Custom(u8),
}

impl PassId {
    /// Get the flag for this pass
    pub const fn flag(self) -> RenderPassFlags {
        match self {
            Self::Main => RenderPassFlags::MAIN,
            Self::DepthPrepass => RenderPassFlags::DEPTH_PREPASS,
            Self::Shadow => RenderPassFlags::SHADOW,
            Self::GBuffer => RenderPassFlags::GBUFFER,
            Self::Transparent => RenderPassFlags::TRANSPARENT,
            Self::Reflection => RenderPassFlags::REFLECTION,
            Self::Refraction => RenderPassFlags::REFRACTION,
            Self::MotionVectors => RenderPassFlags::MOTION_VECTORS,
            Self::Velocity => RenderPassFlags::VELOCITY,
            Self::Outline => RenderPassFlags::OUTLINE,
            Self::Selection => RenderPassFlags::SELECTION,
            Self::Custom(n) => RenderPassFlags::from_bits(1 << (16 + n as u32)),
        }
    }

    /// Get pass name
    pub const fn name(self) -> &'static str {
        match self {
            Self::Main => "main",
            Self::DepthPrepass => "depth_prepass",
            Self::Shadow => "shadow",
            Self::GBuffer => "gbuffer",
            Self::Transparent => "transparent",
            Self::Reflection => "reflection",
            Self::Refraction => "refraction",
            Self::MotionVectors => "motion_vectors",
            Self::Velocity => "velocity",
            Self::Outline => "outline",
            Self::Selection => "selection",
            Self::Custom(_) => "custom",
        }
    }

    /// Get all standard pass IDs
    pub const fn standard_passes() -> &'static [Self] {
        &[
            Self::DepthPrepass,
            Self::Shadow,
            Self::Main,
            Self::Transparent,
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_flags_default() {
        let flags = RenderPassFlags::default();
        assert!(flags.is_empty());
    }

    #[test]
    fn test_flags_opaque() {
        let flags = RenderPassFlags::OPAQUE;
        assert!(flags.contains(RenderPassFlags::MAIN));
        assert!(flags.contains(RenderPassFlags::DEPTH_PREPASS));
        assert!(flags.contains(RenderPassFlags::SHADOW));
        assert!(!flags.contains(RenderPassFlags::TRANSPARENT));
    }

    #[test]
    fn test_flags_contains() {
        let flags = RenderPassFlags::MAIN | RenderPassFlags::SHADOW;
        assert!(flags.contains(RenderPassFlags::MAIN));
        assert!(flags.contains(RenderPassFlags::SHADOW));
        assert!(!flags.contains(RenderPassFlags::TRANSPARENT));
    }

    #[test]
    fn test_flags_intersects() {
        let flags1 = RenderPassFlags::MAIN | RenderPassFlags::SHADOW;
        let flags2 = RenderPassFlags::SHADOW | RenderPassFlags::TRANSPARENT;
        assert!(flags1.intersects(flags2));

        let flags3 = RenderPassFlags::TRANSPARENT | RenderPassFlags::REFLECTION;
        assert!(!flags1.intersects(flags3));
    }

    #[test]
    fn test_flags_insert_remove() {
        let mut flags = RenderPassFlags::MAIN;
        flags.insert(RenderPassFlags::SHADOW);
        assert!(flags.contains(RenderPassFlags::SHADOW));

        flags.remove(RenderPassFlags::MAIN);
        assert!(!flags.contains(RenderPassFlags::MAIN));
        assert!(flags.contains(RenderPassFlags::SHADOW));
    }

    #[test]
    fn test_flags_toggle() {
        let mut flags = RenderPassFlags::MAIN;
        flags.toggle(RenderPassFlags::SHADOW);
        assert!(flags.contains(RenderPassFlags::SHADOW));

        flags.toggle(RenderPassFlags::SHADOW);
        assert!(!flags.contains(RenderPassFlags::SHADOW));
    }

    #[test]
    fn test_flags_bitwise_ops() {
        let a = RenderPassFlags::MAIN | RenderPassFlags::SHADOW;
        let b = RenderPassFlags::SHADOW | RenderPassFlags::TRANSPARENT;

        // Union
        let union = a | b;
        assert!(union.contains(RenderPassFlags::MAIN));
        assert!(union.contains(RenderPassFlags::SHADOW));
        assert!(union.contains(RenderPassFlags::TRANSPARENT));

        // Intersection
        let intersection = a & b;
        assert!(!intersection.contains(RenderPassFlags::MAIN));
        assert!(intersection.contains(RenderPassFlags::SHADOW));
        assert!(!intersection.contains(RenderPassFlags::TRANSPARENT));
    }

    #[test]
    fn test_flags_serialization() {
        let flags = RenderPassFlags::MAIN | RenderPassFlags::SHADOW;
        let json = serde_json::to_string(&flags).unwrap();
        let restored: RenderPassFlags = serde_json::from_str(&json).unwrap();
        assert_eq!(flags, restored);
    }

    #[test]
    fn test_pass_id_flag() {
        assert_eq!(PassId::Main.flag(), RenderPassFlags::MAIN);
        assert_eq!(PassId::Shadow.flag(), RenderPassFlags::SHADOW);
        assert_eq!(PassId::Custom(0).flag(), RenderPassFlags::CUSTOM_0);
    }

    #[test]
    fn test_pass_id_serialization() {
        let ids = [
            PassId::Main,
            PassId::Shadow,
            PassId::Transparent,
            PassId::Custom(2),
        ];

        for id in ids {
            let json = serde_json::to_string(&id).unwrap();
            let restored: PassId = serde_json::from_str(&json).unwrap();
            assert_eq!(id, restored);
        }
    }
}
