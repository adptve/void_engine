//! Debug Visualization Configuration
//!
//! Flags and configuration for debug rendering:
//! - Bounding box/sphere visualization
//! - Normal/tangent display
//! - Wireframe mode
//! - Light volumes and shadow frustums
//! - LOD coloring
//!
//! # Example
//!
//! ```ignore
//! use void_render::debug::{DebugVisualization, DebugConfig};
//!
//! let mut config = DebugConfig::default();
//!
//! // Enable specific visualizations
//! config.flags.insert(DebugVisualization::BOUNDS);
//! config.flags.insert(DebugVisualization::NORMALS);
//!
//! // Customize colors
//! config.bounds_color = [1.0, 0.0, 0.0, 1.0]; // Red bounds
//! ```

use serde::{Deserialize, Serialize};

/// Debug visualization flags (bitflags-style)
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[repr(transparent)]
pub struct DebugVisualization(u32);

impl DebugVisualization {
    /// No debug visualization
    pub const NONE: Self = Self(0);

    /// Show bounding boxes (AABB)
    pub const BOUNDS: Self = Self(1 << 0);

    /// Show bounding spheres
    pub const SPHERES: Self = Self(1 << 1);

    /// Show vertex normals
    pub const NORMALS: Self = Self(1 << 2);

    /// Show tangent vectors
    pub const TANGENTS: Self = Self(1 << 3);

    /// Show wireframe overlay
    pub const WIREFRAME: Self = Self(1 << 4);

    /// Show light volumes/ranges
    pub const LIGHT_VOLUMES: Self = Self(1 << 5);

    /// Show light directions
    pub const LIGHT_DIRECTIONS: Self = Self(1 << 6);

    /// Show shadow frustums
    pub const SHADOW_FRUSTUMS: Self = Self(1 << 7);

    /// Show camera frustum
    pub const CAMERA_FRUSTUM: Self = Self(1 << 8);

    /// Show collision shapes
    pub const COLLIDERS: Self = Self(1 << 9);

    /// Show skeleton bones
    pub const BONES: Self = Self(1 << 10);

    /// Show UV checker pattern
    pub const UV_CHECKER: Self = Self(1 << 11);

    /// Show mipmap levels
    pub const MIPMAPS: Self = Self(1 << 12);

    /// Show overdraw
    pub const OVERDRAW: Self = Self(1 << 13);

    /// Show LOD levels with colors
    pub const LOD_COLORS: Self = Self(1 << 14);

    /// Show chunk boundaries
    pub const CHUNK_BOUNDS: Self = Self(1 << 15);

    /// Show BVH structure
    pub const BVH_NODES: Self = Self(1 << 16);

    /// Show physics bodies
    pub const PHYSICS_BODIES: Self = Self(1 << 17);

    /// Show nav mesh
    pub const NAV_MESH: Self = Self(1 << 18);

    /// Show world origin/axes
    pub const WORLD_AXES: Self = Self(1 << 19);

    /// Show entity labels
    pub const ENTITY_LABELS: Self = Self(1 << 20);

    /// All visualizations
    pub const ALL: Self = Self(0xFFFFFFFF);

    /// Create empty flags
    #[inline]
    pub const fn empty() -> Self {
        Self(0)
    }

    /// Create with all flags set
    #[inline]
    pub const fn all() -> Self {
        Self::ALL
    }

    /// Create from raw bits
    #[inline]
    pub const fn from_bits(bits: u32) -> Self {
        Self(bits)
    }

    /// Get raw bits
    #[inline]
    pub const fn bits(&self) -> u32 {
        self.0
    }

    /// Check if empty
    #[inline]
    pub const fn is_empty(&self) -> bool {
        self.0 == 0
    }

    /// Check if contains flag
    #[inline]
    pub const fn contains(&self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }

    /// Insert a flag
    #[inline]
    pub fn insert(&mut self, other: Self) {
        self.0 |= other.0;
    }

    /// Remove a flag
    #[inline]
    pub fn remove(&mut self, other: Self) {
        self.0 &= !other.0;
    }

    /// Toggle a flag
    #[inline]
    pub fn toggle(&mut self, other: Self) {
        self.0 ^= other.0;
    }

    /// Set a flag to a specific state
    #[inline]
    pub fn set(&mut self, other: Self, enabled: bool) {
        if enabled {
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
}

impl core::ops::BitOr for DebugVisualization {
    type Output = Self;
    #[inline]
    fn bitor(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }
}

impl core::ops::BitOrAssign for DebugVisualization {
    #[inline]
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl core::ops::BitAnd for DebugVisualization {
    type Output = Self;
    #[inline]
    fn bitand(self, rhs: Self) -> Self {
        Self(self.0 & rhs.0)
    }
}

impl core::ops::BitAndAssign for DebugVisualization {
    #[inline]
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0;
    }
}

impl core::ops::Not for DebugVisualization {
    type Output = Self;
    #[inline]
    fn not(self) -> Self {
        Self(!self.0)
    }
}

/// Debug renderer configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DebugConfig {
    /// Active debug visualizations
    pub flags: DebugVisualization,

    /// Normal vector display length (meters)
    pub normal_length: f32,

    /// Tangent vector display length (meters)
    pub tangent_length: f32,

    /// Debug line width (pixels)
    pub line_width: f32,

    /// Bounding box color (RGBA)
    pub bounds_color: [f32; 4],

    /// Bounding sphere color (RGBA)
    pub sphere_color: [f32; 4],

    /// Normal vector color (RGBA)
    pub normals_color: [f32; 4],

    /// Tangent vector color (RGBA)
    pub tangents_color: [f32; 4],

    /// Wireframe color (RGBA)
    pub wireframe_color: [f32; 4],

    /// Light volume color (RGBA)
    pub light_color: [f32; 4],

    /// Shadow frustum color (RGBA)
    pub shadow_color: [f32; 4],

    /// Camera frustum color (RGBA)
    pub camera_frustum_color: [f32; 4],

    /// Collider color (RGBA)
    pub collider_color: [f32; 4],

    /// Bone color (RGBA)
    pub bone_color: [f32; 4],

    /// Chunk boundary color (RGBA)
    pub chunk_color: [f32; 4],

    /// BVH node color (RGBA)
    pub bvh_color: [f32; 4],

    /// LOD level colors (one per level)
    pub lod_colors: [[f32; 4]; 8],

    /// Whether to show debug overlay
    pub show_overlay: bool,

    /// Overlay position (screen-relative, 0-1)
    pub overlay_position: [f32; 2],

    /// Overlay scale
    pub overlay_scale: f32,
}

impl Default for DebugConfig {
    fn default() -> Self {
        Self {
            flags: DebugVisualization::empty(),
            normal_length: 0.1,
            tangent_length: 0.1,
            line_width: 1.0,
            bounds_color: [0.0, 1.0, 0.0, 1.0],        // Green
            sphere_color: [0.0, 0.8, 1.0, 0.5],        // Cyan, semi-transparent
            normals_color: [0.0, 0.0, 1.0, 1.0],       // Blue
            tangents_color: [1.0, 0.0, 0.0, 1.0],      // Red
            wireframe_color: [1.0, 1.0, 1.0, 0.5],     // White, semi-transparent
            light_color: [1.0, 1.0, 0.0, 0.5],         // Yellow, semi-transparent
            shadow_color: [0.5, 0.0, 0.5, 0.5],        // Purple, semi-transparent
            camera_frustum_color: [1.0, 0.5, 0.0, 0.5], // Orange, semi-transparent
            collider_color: [0.0, 1.0, 1.0, 0.5],      // Cyan, semi-transparent
            bone_color: [1.0, 0.0, 1.0, 1.0],          // Magenta
            chunk_color: [0.5, 0.5, 0.5, 0.5],         // Gray, semi-transparent
            bvh_color: [0.8, 0.8, 0.0, 0.3],           // Yellow, very transparent
            lod_colors: [
                [0.0, 1.0, 0.0, 1.0], // LOD 0: Green (highest detail)
                [0.5, 1.0, 0.0, 1.0], // LOD 1: Yellow-green
                [1.0, 1.0, 0.0, 1.0], // LOD 2: Yellow
                [1.0, 0.5, 0.0, 1.0], // LOD 3: Orange
                [1.0, 0.0, 0.0, 1.0], // LOD 4: Red
                [1.0, 0.0, 0.5, 1.0], // LOD 5: Red-magenta
                [1.0, 0.0, 1.0, 1.0], // LOD 6: Magenta
                [0.5, 0.0, 1.0, 1.0], // LOD 7: Purple (lowest detail)
            ],
            show_overlay: false,
            overlay_position: [0.01, 0.01], // Top-left corner
            overlay_scale: 1.0,
        }
    }
}

impl DebugConfig {
    /// Create with no visualizations enabled
    pub fn new() -> Self {
        Self::default()
    }

    /// Create with specific visualizations enabled
    pub fn with_flags(flags: DebugVisualization) -> Self {
        Self {
            flags,
            ..Default::default()
        }
    }

    /// Enable a visualization
    pub fn enable(&mut self, flag: DebugVisualization) -> &mut Self {
        self.flags.insert(flag);
        self
    }

    /// Disable a visualization
    pub fn disable(&mut self, flag: DebugVisualization) -> &mut Self {
        self.flags.remove(flag);
        self
    }

    /// Toggle a visualization
    pub fn toggle(&mut self, flag: DebugVisualization) -> &mut Self {
        self.flags.toggle(flag);
        self
    }

    /// Check if visualization is enabled
    pub fn is_enabled(&self, flag: DebugVisualization) -> bool {
        self.flags.contains(flag)
    }

    /// Get color for a specific LOD level
    pub fn get_lod_color(&self, lod: u8) -> [f32; 4] {
        let idx = (lod as usize).min(self.lod_colors.len() - 1);
        self.lod_colors[idx]
    }

    /// Reset to defaults
    pub fn reset(&mut self) {
        *self = Self::default();
    }

    /// Enable common debug visualizations (bounds + wireframe)
    pub fn enable_common(&mut self) -> &mut Self {
        self.flags.insert(DebugVisualization::BOUNDS);
        self.flags.insert(DebugVisualization::WIREFRAME);
        self
    }

    /// Enable lighting debug visualizations
    pub fn enable_lighting(&mut self) -> &mut Self {
        self.flags.insert(DebugVisualization::LIGHT_VOLUMES);
        self.flags.insert(DebugVisualization::LIGHT_DIRECTIONS);
        self.flags.insert(DebugVisualization::SHADOW_FRUSTUMS);
        self
    }

    /// Disable all visualizations
    pub fn disable_all(&mut self) -> &mut Self {
        self.flags = DebugVisualization::empty();
        self
    }
}

/// Queue for debug configuration updates at frame boundaries
#[derive(Clone, Debug, Default)]
pub struct DebugUpdateQueue {
    /// Pending config changes
    pending_config: Option<DebugConfig>,
    /// Pending flag changes (flag, enabled)
    pending_flag_changes: alloc::vec::Vec<(DebugVisualization, bool)>,
}

impl DebugUpdateQueue {
    /// Create a new update queue
    pub fn new() -> Self {
        Self::default()
    }

    /// Queue a complete config change
    pub fn queue_config(&mut self, config: DebugConfig) {
        self.pending_config = Some(config);
    }

    /// Queue a flag toggle
    pub fn queue_flag_toggle(&mut self, flag: DebugVisualization, enabled: bool) {
        self.pending_flag_changes.push((flag, enabled));
    }

    /// Queue enabling a flag
    pub fn queue_enable(&mut self, flag: DebugVisualization) {
        self.pending_flag_changes.push((flag, true));
    }

    /// Queue disabling a flag
    pub fn queue_disable(&mut self, flag: DebugVisualization) {
        self.pending_flag_changes.push((flag, false));
    }

    /// Check if there are pending changes
    pub fn has_pending(&self) -> bool {
        self.pending_config.is_some() || !self.pending_flag_changes.is_empty()
    }

    /// Apply pending changes at frame boundary
    pub fn apply(&mut self, config: &mut DebugConfig) {
        // Apply full config change if pending
        if let Some(new_config) = self.pending_config.take() {
            *config = new_config;
        }

        // Apply individual flag changes
        for (flag, enabled) in self.pending_flag_changes.drain(..) {
            config.flags.set(flag, enabled);
        }
    }

    /// Clear all pending changes
    pub fn clear(&mut self) {
        self.pending_config = None;
        self.pending_flag_changes.clear();
    }
}

/// State snapshot for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DebugConfigState {
    /// Full configuration
    pub config: DebugConfig,
}

impl From<&DebugConfig> for DebugConfigState {
    fn from(config: &DebugConfig) -> Self {
        Self {
            config: config.clone(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_debug_visualization_flags() {
        let mut flags = DebugVisualization::empty();
        assert!(flags.is_empty());

        flags.insert(DebugVisualization::BOUNDS);
        assert!(flags.contains(DebugVisualization::BOUNDS));
        assert!(!flags.contains(DebugVisualization::NORMALS));

        flags.insert(DebugVisualization::NORMALS);
        assert!(flags.contains(DebugVisualization::BOUNDS));
        assert!(flags.contains(DebugVisualization::NORMALS));

        flags.remove(DebugVisualization::BOUNDS);
        assert!(!flags.contains(DebugVisualization::BOUNDS));
        assert!(flags.contains(DebugVisualization::NORMALS));
    }

    #[test]
    fn test_debug_visualization_operators() {
        let a = DebugVisualization::BOUNDS | DebugVisualization::NORMALS;
        let b = DebugVisualization::NORMALS | DebugVisualization::WIREFRAME;

        // Union
        let union = a | b;
        assert!(union.contains(DebugVisualization::BOUNDS));
        assert!(union.contains(DebugVisualization::NORMALS));
        assert!(union.contains(DebugVisualization::WIREFRAME));

        // Intersection
        let intersection = a & b;
        assert!(!intersection.contains(DebugVisualization::BOUNDS));
        assert!(intersection.contains(DebugVisualization::NORMALS));
        assert!(!intersection.contains(DebugVisualization::WIREFRAME));
    }

    #[test]
    fn test_debug_visualization_toggle() {
        let mut flags = DebugVisualization::BOUNDS;
        flags.toggle(DebugVisualization::BOUNDS);
        assert!(!flags.contains(DebugVisualization::BOUNDS));

        flags.toggle(DebugVisualization::BOUNDS);
        assert!(flags.contains(DebugVisualization::BOUNDS));
    }

    #[test]
    fn test_debug_config_default() {
        let config = DebugConfig::default();
        assert!(config.flags.is_empty());
        assert_eq!(config.normal_length, 0.1);
        assert_eq!(config.line_width, 1.0);
    }

    #[test]
    fn test_debug_config_enable_disable() {
        let mut config = DebugConfig::new();

        config.enable(DebugVisualization::BOUNDS);
        assert!(config.is_enabled(DebugVisualization::BOUNDS));

        config.disable(DebugVisualization::BOUNDS);
        assert!(!config.is_enabled(DebugVisualization::BOUNDS));
    }

    #[test]
    fn test_debug_config_get_lod_color() {
        let config = DebugConfig::default();

        // LOD 0 should be green
        let color = config.get_lod_color(0);
        assert_eq!(color, [0.0, 1.0, 0.0, 1.0]);

        // Out of bounds should clamp
        let color = config.get_lod_color(100);
        assert_eq!(color, config.lod_colors[7]);
    }

    #[test]
    fn test_debug_update_queue() {
        let mut queue = DebugUpdateQueue::new();
        let mut config = DebugConfig::new();

        assert!(!queue.has_pending());

        queue.queue_enable(DebugVisualization::BOUNDS);
        queue.queue_enable(DebugVisualization::NORMALS);
        assert!(queue.has_pending());

        queue.apply(&mut config);

        assert!(config.is_enabled(DebugVisualization::BOUNDS));
        assert!(config.is_enabled(DebugVisualization::NORMALS));
        assert!(!queue.has_pending());
    }

    #[test]
    fn test_debug_visualization_serialization() {
        let flags = DebugVisualization::BOUNDS
            | DebugVisualization::NORMALS
            | DebugVisualization::WIREFRAME;

        let serialized = serde_json::to_string(&flags).unwrap();
        let restored: DebugVisualization = serde_json::from_str(&serialized).unwrap();

        assert_eq!(flags, restored);
        assert!(restored.contains(DebugVisualization::BOUNDS));
        assert!(restored.contains(DebugVisualization::NORMALS));
        assert!(restored.contains(DebugVisualization::WIREFRAME));
    }

    #[test]
    fn test_debug_config_serialization() {
        let mut config = DebugConfig::default();
        config.flags = DebugVisualization::BOUNDS | DebugVisualization::COLLIDERS;
        config.normal_length = 0.5;
        config.bounds_color = [1.0, 0.0, 0.0, 1.0];

        let serialized = serde_json::to_string(&config).unwrap();
        let restored: DebugConfig = serde_json::from_str(&serialized).unwrap();

        assert_eq!(restored.flags, config.flags);
        assert_eq!(restored.normal_length, 0.5);
        assert_eq!(restored.bounds_color, [1.0, 0.0, 0.0, 1.0]);
    }

    #[test]
    fn test_debug_config_enable_common() {
        let mut config = DebugConfig::new();
        config.enable_common();

        assert!(config.is_enabled(DebugVisualization::BOUNDS));
        assert!(config.is_enabled(DebugVisualization::WIREFRAME));
    }

    #[test]
    fn test_debug_config_enable_lighting() {
        let mut config = DebugConfig::new();
        config.enable_lighting();

        assert!(config.is_enabled(DebugVisualization::LIGHT_VOLUMES));
        assert!(config.is_enabled(DebugVisualization::LIGHT_DIRECTIONS));
        assert!(config.is_enabled(DebugVisualization::SHADOW_FRUSTUMS));
    }
}
