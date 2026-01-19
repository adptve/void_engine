//! Render Pass Component
//!
//! Controls which render passes an entity participates in.
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::render_pass::{RenderPasses, RenderPassFlags};
//!
//! // Create an opaque entity (main + depth + shadow)
//! let passes = RenderPasses::opaque();
//!
//! // Create a transparent entity
//! let passes = RenderPasses::transparent();
//!
//! // Custom configuration
//! let passes = RenderPasses::new(RenderPassFlags::MAIN | RenderPassFlags::OUTLINE)
//!     .with_material_override(RenderPassFlags::SHADOW.bits(), "shadow_material")
//!     .with_order_offset(RenderPassFlags::MAIN.bits(), 100);
//! ```

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
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

/// Controls which render passes an entity participates in
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RenderPasses {
    /// Pass participation flags
    pub flags: RenderPassFlags,

    /// Per-pass material overrides (pass_id bits -> material path)
    pub material_overrides: BTreeMap<u32, String>,

    /// Per-pass render order offset
    pub order_offsets: BTreeMap<u32, i32>,

    /// Custom pass names this entity belongs to
    pub custom_passes: Vec<String>,

    /// Priority within passes (higher = rendered first)
    pub priority: i32,

    /// Whether this entity should cast shadows
    pub cast_shadows: bool,

    /// Whether this entity should receive shadows
    pub receive_shadows: bool,
}

impl Default for RenderPasses {
    fn default() -> Self {
        Self {
            flags: RenderPassFlags::OPAQUE,
            material_overrides: BTreeMap::new(),
            order_offsets: BTreeMap::new(),
            custom_passes: Vec::new(),
            priority: 0,
            cast_shadows: true,
            receive_shadows: true,
        }
    }
}

impl RenderPasses {
    /// Create with specific flags
    pub fn new(flags: RenderPassFlags) -> Self {
        Self {
            flags,
            ..Default::default()
        }
    }

    /// Opaque entity (main + depth + shadow)
    pub fn opaque() -> Self {
        Self {
            flags: RenderPassFlags::OPAQUE,
            ..Default::default()
        }
    }

    /// Transparent entity (main + transparent pass)
    pub fn transparent() -> Self {
        Self {
            flags: RenderPassFlags::MAIN | RenderPassFlags::TRANSPARENT,
            cast_shadows: false,
            ..Default::default()
        }
    }

    /// Shadow caster only (no main pass rendering)
    pub fn shadow_only() -> Self {
        Self {
            flags: RenderPassFlags::SHADOW,
            receive_shadows: false,
            ..Default::default()
        }
    }

    /// Main pass only (no shadows)
    pub fn main_only() -> Self {
        Self {
            flags: RenderPassFlags::MAIN,
            cast_shadows: false,
            receive_shadows: false,
            ..Default::default()
        }
    }

    /// Depth prepass only
    pub fn depth_only() -> Self {
        Self {
            flags: RenderPassFlags::DEPTH_PREPASS,
            cast_shadows: false,
            receive_shadows: false,
            ..Default::default()
        }
    }

    /// Add a custom pass by name
    pub fn with_custom_pass(mut self, name: impl Into<String>) -> Self {
        self.custom_passes.push(name.into());
        self
    }

    /// Add flags
    pub fn with_flags(mut self, flags: RenderPassFlags) -> Self {
        self.flags |= flags;
        self
    }

    /// Remove flags
    pub fn without_flags(mut self, flags: RenderPassFlags) -> Self {
        self.flags.remove(flags);
        self
    }

    /// Set material override for a pass
    pub fn with_material_override(mut self, pass_bits: u32, material: impl Into<String>) -> Self {
        self.material_overrides.insert(pass_bits, material.into());
        self
    }

    /// Set order offset for a pass
    pub fn with_order_offset(mut self, pass_bits: u32, offset: i32) -> Self {
        self.order_offsets.insert(pass_bits, offset);
        self
    }

    /// Set priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Enable shadow casting
    pub fn with_shadows(mut self) -> Self {
        self.cast_shadows = true;
        self.receive_shadows = true;
        self
    }

    /// Disable shadow casting
    pub fn without_shadows(mut self) -> Self {
        self.cast_shadows = false;
        self.receive_shadows = false;
        self
    }

    /// Check if entity participates in a pass
    pub fn in_pass(&self, pass: RenderPassFlags) -> bool {
        self.flags.contains(pass)
    }

    /// Check if entity participates in any of the specified passes
    pub fn in_any_pass(&self, passes: RenderPassFlags) -> bool {
        self.flags.intersects(passes)
    }

    /// Get material override for a pass (if any)
    pub fn get_material_override(&self, pass_bits: u32) -> Option<&str> {
        self.material_overrides.get(&pass_bits).map(|s| s.as_str())
    }

    /// Get order offset for a pass
    pub fn get_order_offset(&self, pass_bits: u32) -> i32 {
        self.order_offsets.get(&pass_bits).copied().unwrap_or(0)
    }

    /// Check if entity is in a custom pass by name
    pub fn in_custom_pass(&self, name: &str) -> bool {
        self.custom_passes.iter().any(|p| p == name)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_render_passes_default() {
        let passes = RenderPasses::default();
        assert!(passes.in_pass(RenderPassFlags::MAIN));
        assert!(passes.in_pass(RenderPassFlags::DEPTH_PREPASS));
        assert!(passes.in_pass(RenderPassFlags::SHADOW));
        assert!(passes.cast_shadows);
        assert!(passes.receive_shadows);
    }

    #[test]
    fn test_render_passes_opaque() {
        let passes = RenderPasses::opaque();
        assert!(passes.in_pass(RenderPassFlags::OPAQUE));
        assert!(!passes.in_pass(RenderPassFlags::TRANSPARENT));
    }

    #[test]
    fn test_render_passes_transparent() {
        let passes = RenderPasses::transparent();
        assert!(passes.in_pass(RenderPassFlags::MAIN));
        assert!(passes.in_pass(RenderPassFlags::TRANSPARENT));
        assert!(!passes.in_pass(RenderPassFlags::SHADOW));
        assert!(!passes.cast_shadows);
    }

    #[test]
    fn test_render_passes_shadow_only() {
        let passes = RenderPasses::shadow_only();
        assert!(passes.in_pass(RenderPassFlags::SHADOW));
        assert!(!passes.in_pass(RenderPassFlags::MAIN));
    }

    #[test]
    fn test_render_passes_builder() {
        let passes = RenderPasses::new(RenderPassFlags::MAIN)
            .with_flags(RenderPassFlags::OUTLINE)
            .with_material_override(RenderPassFlags::SHADOW.bits(), "shadow_mat")
            .with_order_offset(RenderPassFlags::MAIN.bits(), 100)
            .with_priority(5)
            .with_custom_pass("glow");

        assert!(passes.in_pass(RenderPassFlags::MAIN));
        assert!(passes.in_pass(RenderPassFlags::OUTLINE));
        assert_eq!(
            passes.get_material_override(RenderPassFlags::SHADOW.bits()),
            Some("shadow_mat")
        );
        assert_eq!(passes.get_order_offset(RenderPassFlags::MAIN.bits()), 100);
        assert_eq!(passes.priority, 5);
        assert!(passes.in_custom_pass("glow"));
    }

    #[test]
    fn test_render_passes_without_flags() {
        let passes = RenderPasses::opaque().without_flags(RenderPassFlags::SHADOW);
        assert!(passes.in_pass(RenderPassFlags::MAIN));
        assert!(!passes.in_pass(RenderPassFlags::SHADOW));
    }

    #[test]
    fn test_render_passes_in_any_pass() {
        let passes = RenderPasses::transparent();
        assert!(passes.in_any_pass(RenderPassFlags::MAIN | RenderPassFlags::SHADOW));
        assert!(!passes.in_any_pass(RenderPassFlags::SHADOW | RenderPassFlags::REFLECTION));
    }

    #[test]
    fn test_render_passes_serialization() {
        let mut passes = RenderPasses::opaque();
        passes.material_overrides.insert(RenderPassFlags::SHADOW.bits(), "shadow_mat".into());
        passes.order_offsets.insert(RenderPassFlags::MAIN.bits(), 10);
        passes.custom_passes.push("outline".into());

        let json = serde_json::to_string(&passes).unwrap();
        let restored: RenderPasses = serde_json::from_str(&json).unwrap();

        assert_eq!(passes.flags, restored.flags);
        assert_eq!(passes.material_overrides, restored.material_overrides);
        assert_eq!(passes.order_offsets, restored.order_offsets);
        assert_eq!(passes.custom_passes, restored.custom_passes);
    }

    #[test]
    fn test_render_pass_flags_serialization() {
        let flags = RenderPassFlags::MAIN | RenderPassFlags::SHADOW;
        let json = serde_json::to_string(&flags).unwrap();
        let restored: RenderPassFlags = serde_json::from_str(&json).unwrap();
        assert_eq!(flags, restored);
    }
}
