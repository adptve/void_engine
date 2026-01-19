//! Level of Detail (LOD) System
//!
//! Provides distance-based and screen-size-based LOD switching with:
//! - Multiple LOD meshes per entity
//! - Hysteresis to prevent popping
//! - Crossfade and dither transitions
//! - Hot-reload support
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::lod::{LodGroup, LodMode};
//!
//! // Create LOD group with distance-based levels
//! let lod = LodGroup::from_distances(&[
//!     ("meshes/char_high.glb".into(), 10.0),
//!     ("meshes/char_med.glb".into(), 50.0),
//!     ("meshes/char_low.glb".into(), 100.0),
//! ]);
//!
//! // Calculate which LOD to use based on distance
//! let lod_index = lod.calculate_lod(35.0, 0.0);
//! ```

use alloc::string::String;
use alloc::vec::Vec;
use alloc::vec;
use serde::{Deserialize, Serialize};
use void_math::Vec3;
use crate::Entity;

// ============================================================================
// LOD Level
// ============================================================================

/// Individual LOD level configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodLevel {
    /// Mesh asset path for this LOD
    pub mesh: String,

    /// Screen height threshold (0-1, percentage of screen)
    /// If > 0, this is used instead of distance
    pub screen_height: f32,

    /// Distance threshold (meters from camera)
    /// Object uses this LOD when closer than this distance
    pub distance: f32,

    /// Render priority offset (for sorting)
    pub priority_offset: i32,
}

impl LodLevel {
    /// Create a new LOD level with distance threshold
    pub fn new(mesh: impl Into<String>, distance: f32) -> Self {
        Self {
            mesh: mesh.into(),
            screen_height: 0.0,
            distance,
            priority_offset: 0,
        }
    }

    /// Create a LOD level with screen height threshold
    pub fn with_screen_height(mesh: impl Into<String>, screen_height: f32) -> Self {
        Self {
            mesh: mesh.into(),
            screen_height,
            distance: 0.0,
            priority_offset: 0,
        }
    }
}

impl Default for LodLevel {
    fn default() -> Self {
        Self {
            mesh: String::new(),
            screen_height: 0.0,
            distance: 0.0,
            priority_offset: 0,
        }
    }
}

// ============================================================================
// LOD Mode
// ============================================================================

/// LOD transition mode
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum LodMode {
    /// Instant switch at threshold (default, most efficient)
    #[default]
    Instant,

    /// Cross-fade between LODs (render both with alpha blending)
    CrossFade,

    /// Dither-based transition (single draw, shader-based pattern)
    Dither,

    /// Geometry morphing (requires compatible meshes with same vertex count)
    Morph,
}

// ============================================================================
// LOD Fade State
// ============================================================================

/// State for LOD fade transitions
#[derive(Clone, Debug)]
pub struct LodFadeState {
    /// LOD index we're fading from
    pub from_lod: u32,
    /// LOD index we're fading to
    pub to_lod: u32,
    /// Fade progress (0.0 = at from_lod, 1.0 = at to_lod)
    pub progress: f32,
}

// ============================================================================
// LOD Group Component
// ============================================================================

/// Level of Detail configuration component
///
/// Attach to entities to enable LOD switching based on camera distance
/// or screen coverage.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodGroup {
    /// LOD levels (sorted by distance, closest first)
    pub levels: Vec<LodLevel>,

    /// Current active LOD index
    #[serde(skip)]
    pub current: u32,

    /// LOD selection mode
    pub mode: LodMode,

    /// Hysteresis factor (0-1)
    /// Prevents LOD popping by requiring extra distance beyond threshold
    pub hysteresis: f32,

    /// Fade duration in seconds (for CrossFade mode)
    pub fade_duration: f32,

    /// Current fade state (transient, not serialized)
    #[serde(skip)]
    pub fade_state: Option<LodFadeState>,

    /// Override LOD index (-1 = auto, 0+ = forced LOD)
    pub force_lod: i32,
}

impl Default for LodGroup {
    fn default() -> Self {
        Self {
            levels: Vec::new(),
            current: 0,
            mode: LodMode::Instant,
            hysteresis: 0.1,
            fade_duration: 0.3,
            fade_state: None,
            force_lod: -1,
        }
    }
}

impl LodGroup {
    /// Create a new empty LOD group
    pub fn new() -> Self {
        Self::default()
    }

    /// Create LOD group with distance-based levels
    ///
    /// Each tuple is (mesh_path, max_distance).
    /// Levels should be sorted by distance (closest first).
    pub fn from_distances(meshes: &[(String, f32)]) -> Self {
        let levels = meshes
            .iter()
            .map(|(mesh, dist)| LodLevel {
                mesh: mesh.clone(),
                screen_height: 0.0,
                distance: *dist,
                priority_offset: 0,
            })
            .collect();

        Self {
            levels,
            ..Default::default()
        }
    }

    /// Create LOD group with screen-size-based levels
    ///
    /// Each tuple is (mesh_path, min_screen_height).
    /// Levels should be sorted by screen height (largest first).
    pub fn from_screen_heights(meshes: &[(String, f32)]) -> Self {
        let levels = meshes
            .iter()
            .map(|(mesh, height)| LodLevel {
                mesh: mesh.clone(),
                screen_height: *height,
                distance: 0.0,
                priority_offset: 0,
            })
            .collect();

        Self {
            levels,
            ..Default::default()
        }
    }

    /// Add a LOD level
    pub fn add_level(&mut self, level: LodLevel) -> &mut Self {
        self.levels.push(level);
        self
    }

    /// Sort levels by distance (ascending)
    pub fn sort_by_distance(&mut self) {
        self.levels.sort_by(|a, b| {
            a.distance
                .partial_cmp(&b.distance)
                .unwrap_or(core::cmp::Ordering::Equal)
        });
    }

    /// Calculate which LOD to use based on distance and screen coverage
    ///
    /// Returns the LOD index (0 = highest detail).
    pub fn calculate_lod(&self, distance: f32, screen_height: f32) -> u32 {
        // Check for forced LOD
        if self.force_lod >= 0 {
            return (self.force_lod as u32).min(self.levels.len().saturating_sub(1) as u32);
        }

        if self.levels.is_empty() {
            return 0;
        }

        for (i, level) in self.levels.iter().enumerate() {
            // Check screen height first (if set)
            if level.screen_height > 0.0 && screen_height >= level.screen_height {
                return i as u32;
            }

            // Then check distance
            if level.distance > 0.0 && distance <= level.distance {
                return i as u32;
            }
        }

        // Default to lowest LOD (last level)
        self.levels.len().saturating_sub(1) as u32
    }

    /// Calculate LOD with hysteresis to prevent popping
    ///
    /// Only switches LOD when distance crosses threshold by hysteresis amount.
    pub fn calculate_lod_with_hysteresis(&self, distance: f32, screen_height: f32) -> u32 {
        let target = self.calculate_lod(distance, screen_height);

        if self.hysteresis <= 0.0 || self.current == target || self.levels.is_empty() {
            return target;
        }

        if target > self.current {
            // Going to lower detail (farther away)
            // Use current level's threshold - need to be past it by hysteresis amount
            if let Some(current_level) = self.levels.get(self.current as usize) {
                let threshold_dist = current_level.distance;
                let hysteresis_dist = threshold_dist * (1.0 + self.hysteresis);
                if distance > hysteresis_dist {
                    return target;
                }
            }
        } else {
            // Going to higher detail (closer)
            // Use target level's threshold - need to be closer than it by hysteresis amount
            if let Some(target_level) = self.levels.get(target as usize) {
                let threshold_dist = target_level.distance;
                let hysteresis_dist = threshold_dist * (1.0 - self.hysteresis);
                if distance < hysteresis_dist {
                    return target;
                }
            }
        }

        self.current
    }

    /// Get current mesh path to render
    pub fn current_mesh(&self) -> Option<&str> {
        self.levels.get(self.current as usize).map(|l| l.mesh.as_str())
    }

    /// Get mesh path for specific LOD index
    pub fn mesh_at(&self, index: u32) -> Option<&str> {
        self.levels.get(index as usize).map(|l| l.mesh.as_str())
    }

    /// Get fade information for crossfade rendering
    ///
    /// Returns (from_lod, to_lod, progress) if currently fading.
    pub fn fade_alpha(&self) -> Option<(u32, u32, f32)> {
        self.fade_state
            .as_ref()
            .map(|s| (s.from_lod, s.to_lod, s.progress))
    }

    /// Check if currently transitioning
    pub fn is_transitioning(&self) -> bool {
        self.fade_state.is_some()
    }

    /// Get number of LOD levels
    pub fn level_count(&self) -> usize {
        self.levels.len()
    }

    /// Get the asset paths for all LOD meshes
    pub fn asset_paths(&self) -> Vec<&str> {
        self.levels.iter().map(|l| l.mesh.as_str()).collect()
    }
}

// ============================================================================
// LOD System Configuration
// ============================================================================

/// Global LOD system configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodSystemConfig {
    /// Global LOD bias (shifts all distance thresholds)
    /// Positive = use higher detail, negative = use lower detail
    pub lod_bias: f32,

    /// Maximum LOD level allowed (caps detail reduction)
    pub max_lod: u32,

    /// Force specific LOD for all entities (-1 = auto)
    pub global_force_lod: i32,

    /// Enable LOD transitions (crossfade, dither)
    pub transitions_enabled: bool,

    /// Screen height used for coverage calculations
    pub screen_height: f32,

    /// Camera field of view for screen coverage calculations
    pub camera_fov: f32,
}

impl Default for LodSystemConfig {
    fn default() -> Self {
        Self {
            lod_bias: 0.0,
            max_lod: u32::MAX,
            global_force_lod: -1,
            transitions_enabled: true,
            screen_height: 1080.0,
            camera_fov: 60.0_f32.to_radians(),
        }
    }
}

// ============================================================================
// LOD Update Operations
// ============================================================================

/// LOD update operations for frame-boundary processing
#[derive(Clone, Debug)]
pub enum LodUpdate {
    /// Update system configuration
    UpdateConfig(LodSystemConfig),
    /// Force LOD recalculation for specific entities
    RecalculateLods(Vec<Entity>),
    /// Preload meshes for entities
    PreloadMeshes(Vec<Entity>),
    /// Reset all transition states
    ResetTransitions,
}

// ============================================================================
// LOD System
// ============================================================================

/// LOD selection and update system
#[derive(Clone, Debug)]
pub struct LodSystem {
    /// System configuration
    pub config: LodSystemConfig,
    /// Pending updates for frame boundary
    update_queue: Vec<LodUpdate>,
}

impl LodSystem {
    /// Create a new LOD system
    pub fn new() -> Self {
        Self {
            config: LodSystemConfig::default(),
            update_queue: Vec::new(),
        }
    }

    /// Create with custom configuration
    pub fn with_config(config: LodSystemConfig) -> Self {
        Self {
            config,
            update_queue: Vec::new(),
        }
    }

    /// Queue an update for frame boundary
    pub fn queue_update(&mut self, update: LodUpdate) {
        self.update_queue.push(update);
    }

    /// Get pending updates
    pub fn pending_updates(&self) -> &[LodUpdate] {
        &self.update_queue
    }

    /// Clear pending updates
    pub fn clear_updates(&mut self) {
        self.update_queue.clear();
    }

    /// Take all pending updates
    pub fn take_updates(&mut self) -> Vec<LodUpdate> {
        core::mem::take(&mut self.update_queue)
    }

    /// Update a single LOD group based on camera position
    pub fn update_lod(
        &self,
        lod: &mut LodGroup,
        object_position: Vec3,
        camera_pos: Vec3,
        bounds_radius: f32,
        dt: f32,
    ) {
        // Apply global force if set
        if self.config.global_force_lod >= 0 {
            lod.current = (self.config.global_force_lod as u32).min(lod.levels.len().saturating_sub(1) as u32);
            lod.fade_state = None;
            return;
        }

        // Calculate distance
        let distance = (object_position - camera_pos).length();

        // Apply LOD bias
        let biased_distance = distance * (1.0 - self.config.lod_bias * 0.1);

        // Calculate screen coverage
        let screen_coverage = calculate_screen_coverage(
            biased_distance,
            bounds_radius,
            self.config.camera_fov,
            self.config.screen_height,
        );

        // Determine target LOD
        let target = lod.calculate_lod_with_hysteresis(biased_distance, screen_coverage);
        let target = target.min(self.config.max_lod);

        match lod.mode {
            LodMode::Instant => {
                lod.current = target;
                lod.fade_state = None;
            }

            LodMode::CrossFade => {
                if !self.config.transitions_enabled {
                    lod.current = target;
                    lod.fade_state = None;
                    return;
                }

                if target != lod.current && lod.fade_state.is_none() {
                    // Start fade
                    lod.fade_state = Some(LodFadeState {
                        from_lod: lod.current,
                        to_lod: target,
                        progress: 0.0,
                    });
                }

                // Update fade progress
                if let Some(ref mut state) = lod.fade_state {
                    state.progress += dt / lod.fade_duration;

                    if state.progress >= 1.0 {
                        lod.current = state.to_lod;
                        lod.fade_state = None;
                    }
                }
            }

            LodMode::Dither | LodMode::Morph => {
                // These modes use shader-based transitions
                lod.current = target;
            }
        }
    }

    /// Calculate statistics
    pub fn calculate_stats(&self, lod_groups: &[&LodGroup]) -> LodSystemStats {
        let mut stats = LodSystemStats::default();

        for lod in lod_groups {
            stats.total_entities += 1;
            stats.lod_counts[lod.current as usize % 8] += 1;

            if lod.is_transitioning() {
                stats.transitioning_count += 1;
            }
        }

        stats
    }
}

impl Default for LodSystem {
    fn default() -> Self {
        Self::new()
    }
}

/// Calculate screen coverage for an object
fn calculate_screen_coverage(
    distance: f32,
    radius: f32,
    fov: f32,
    screen_height: f32,
) -> f32 {
    if distance <= 0.0 {
        return screen_height; // Object at camera position = full screen
    }

    // Calculate angular size of object
    let angular_size = 2.0 * (radius / distance).atan();

    // Calculate fraction of screen covered
    let screen_fraction = angular_size / fov;

    screen_fraction * screen_height
}

// ============================================================================
// Statistics
// ============================================================================

/// LOD system statistics
#[derive(Clone, Debug, Default)]
pub struct LodSystemStats {
    /// Total entities with LOD
    pub total_entities: usize,
    /// Entities currently transitioning
    pub transitioning_count: usize,
    /// Count per LOD level (up to 8 levels tracked)
    pub lod_counts: [usize; 8],
}

// ============================================================================
// Serializable State
// ============================================================================

/// Serializable LOD system state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodSystemState {
    pub config: LodSystemConfig,
}

impl LodSystem {
    /// Serialize to state
    pub fn to_state(&self) -> LodSystemState {
        LodSystemState {
            config: self.config.clone(),
        }
    }

    /// Restore from state
    pub fn from_state(state: &LodSystemState) -> Self {
        Self {
            config: state.config.clone(),
            update_queue: Vec::new(),
        }
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lod_selection_distance() {
        let lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("medium.glb".into(), 50.0),
            ("low.glb".into(), 100.0),
        ]);

        assert_eq!(lod.calculate_lod(5.0, 0.0), 0); // High (within 10m)
        assert_eq!(lod.calculate_lod(10.0, 0.0), 0); // High (at threshold)
        assert_eq!(lod.calculate_lod(30.0, 0.0), 1); // Medium (within 50m)
        assert_eq!(lod.calculate_lod(80.0, 0.0), 2); // Low (within 100m)
        assert_eq!(lod.calculate_lod(150.0, 0.0), 2); // Low (beyond all thresholds)
    }

    #[test]
    fn test_lod_selection_screen_height() {
        let lod = LodGroup::from_screen_heights(&[
            ("high.glb".into(), 0.5), // Use high when covering 50%+ of screen
            ("medium.glb".into(), 0.2), // Use medium when covering 20%+
            ("low.glb".into(), 0.05), // Use low when covering 5%+
        ]);

        assert_eq!(lod.calculate_lod(0.0, 0.6), 0); // High (60% coverage)
        assert_eq!(lod.calculate_lod(0.0, 0.3), 1); // Medium (30% coverage)
        assert_eq!(lod.calculate_lod(0.0, 0.1), 2); // Low (10% coverage)
    }

    #[test]
    fn test_lod_hysteresis() {
        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("low.glb".into(), 50.0),
        ]);
        lod.hysteresis = 0.2;
        lod.current = 0;

        // At threshold - hysteresis prevents switch
        // With 20% hysteresis on threshold 10, need to be > 12 to switch
        assert_eq!(lod.calculate_lod_with_hysteresis(11.0, 0.0), 0);

        // Well past hysteresis threshold - should switch to LOD 1
        assert_eq!(lod.calculate_lod_with_hysteresis(15.0, 0.0), 1);

        // Now at LOD 1, check switch back
        lod.current = 1;

        // At LOD 1, threshold is 50. To go back to LOD 0, need distance < 10.
        // With hysteresis, need distance < 10 * (1 - 0.2) = 8
        assert_eq!(lod.calculate_lod_with_hysteresis(9.0, 0.0), 1);

        // Past hysteresis threshold - should switch back
        assert_eq!(lod.calculate_lod_with_hysteresis(7.0, 0.0), 0);
    }

    #[test]
    fn test_lod_force() {
        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("medium.glb".into(), 50.0),
            ("low.glb".into(), 100.0),
        ]);

        // Force LOD 2
        lod.force_lod = 2;
        assert_eq!(lod.calculate_lod(5.0, 0.0), 2);

        // Force LOD 0
        lod.force_lod = 0;
        assert_eq!(lod.calculate_lod(80.0, 0.0), 0);

        // Force LOD out of range (should clamp)
        lod.force_lod = 10;
        assert_eq!(lod.calculate_lod(5.0, 0.0), 2);
    }

    #[test]
    fn test_lod_current_mesh() {
        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("medium.glb".into(), 50.0),
            ("low.glb".into(), 100.0),
        ]);

        lod.current = 0;
        assert_eq!(lod.current_mesh(), Some("high.glb"));

        lod.current = 1;
        assert_eq!(lod.current_mesh(), Some("medium.glb"));

        lod.current = 2;
        assert_eq!(lod.current_mesh(), Some("low.glb"));
    }

    #[test]
    fn test_lod_fade_state() {
        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("low.glb".into(), 50.0),
        ]);
        lod.mode = LodMode::CrossFade;

        assert!(lod.fade_alpha().is_none());
        assert!(!lod.is_transitioning());

        lod.fade_state = Some(LodFadeState {
            from_lod: 0,
            to_lod: 1,
            progress: 0.5,
        });

        assert!(lod.is_transitioning());
        let (from, to, progress) = lod.fade_alpha().unwrap();
        assert_eq!(from, 0);
        assert_eq!(to, 1);
        assert!((progress - 0.5).abs() < 0.001);
    }

    #[test]
    fn test_lod_serialization() {
        let lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("medium.glb".into(), 50.0),
            ("low.glb".into(), 100.0),
        ]);

        let serialized = bincode::serialize(&lod).unwrap();
        let deserialized: LodGroup = bincode::deserialize(&serialized).unwrap();

        assert_eq!(deserialized.levels.len(), 3);
        assert_eq!(deserialized.levels[0].mesh, "high.glb");
        assert_eq!(deserialized.levels[1].distance, 50.0);
        assert_eq!(deserialized.mode, LodMode::Instant);
    }

    #[test]
    fn test_lod_system_update() {
        let system = LodSystem::new();

        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("low.glb".into(), 50.0),
        ]);
        lod.current = 0;

        // Object at distance 5 (should stay at LOD 0)
        system.update_lod(
            &mut lod,
            Vec3::new(0.0, 0.0, 5.0),
            Vec3::ZERO,
            1.0,
            0.016,
        );
        assert_eq!(lod.current, 0);

        // Object at distance 30 (should switch to LOD 1)
        system.update_lod(
            &mut lod,
            Vec3::new(0.0, 0.0, 30.0),
            Vec3::ZERO,
            1.0,
            0.016,
        );
        assert_eq!(lod.current, 1);
    }

    #[test]
    fn test_lod_system_config() {
        let config = LodSystemConfig::default();
        assert_eq!(config.lod_bias, 0.0);
        assert_eq!(config.max_lod, u32::MAX);
        assert_eq!(config.global_force_lod, -1);
        assert!(config.transitions_enabled);
    }

    #[test]
    fn test_lod_system_serialization() {
        let mut system = LodSystem::new();
        system.config.lod_bias = 2.0;
        system.config.max_lod = 3;

        let state = system.to_state();
        let restored = LodSystem::from_state(&state);

        assert_eq!(restored.config.lod_bias, 2.0);
        assert_eq!(restored.config.max_lod, 3);
    }

    #[test]
    fn test_lod_asset_paths() {
        let lod = LodGroup::from_distances(&[
            ("meshes/char_high.glb".into(), 10.0),
            ("meshes/char_med.glb".into(), 50.0),
            ("meshes/char_low.glb".into(), 100.0),
        ]);

        let paths = lod.asset_paths();
        assert_eq!(paths.len(), 3);
        assert!(paths.contains(&"meshes/char_high.glb"));
        assert!(paths.contains(&"meshes/char_med.glb"));
        assert!(paths.contains(&"meshes/char_low.glb"));
    }

    #[test]
    fn test_screen_coverage_calculation() {
        // Object at distance 10 with radius 1, 60 degree FOV, 1080p screen
        let coverage = calculate_screen_coverage(10.0, 1.0, 60.0_f32.to_radians(), 1080.0);

        // Should be approximately 2 * atan(0.1) / 1.047 * 1080 ~= 206 pixels
        assert!(coverage > 0.0);
        assert!(coverage < 1080.0);

        // Closer object should have more coverage
        let closer_coverage = calculate_screen_coverage(5.0, 1.0, 60.0_f32.to_radians(), 1080.0);
        assert!(closer_coverage > coverage);

        // Object at camera position should fill screen
        let at_camera = calculate_screen_coverage(0.0, 1.0, 60.0_f32.to_radians(), 1080.0);
        assert_eq!(at_camera, 1080.0);
    }

    #[test]
    fn test_lod_crossfade_transition() {
        let mut system = LodSystem::new();
        system.config.transitions_enabled = true;

        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("low.glb".into(), 50.0),
        ]);
        lod.mode = LodMode::CrossFade;
        lod.fade_duration = 0.5;
        lod.current = 0;

        // Move to distance 30 (should trigger transition to LOD 1)
        system.update_lod(
            &mut lod,
            Vec3::new(0.0, 0.0, 30.0),
            Vec3::ZERO,
            1.0,
            0.1, // dt = 100ms
        );

        // Should have started transition
        assert!(lod.fade_state.is_some());
        let state = lod.fade_state.as_ref().unwrap();
        assert_eq!(state.from_lod, 0);
        assert_eq!(state.to_lod, 1);
        assert!((state.progress - 0.2).abs() < 0.01); // 0.1 / 0.5 = 0.2

        // Update until transition completes
        for _ in 0..5 {
            system.update_lod(
                &mut lod,
                Vec3::new(0.0, 0.0, 30.0),
                Vec3::ZERO,
                1.0,
                0.1,
            );
        }

        // Transition should be complete
        assert!(lod.fade_state.is_none());
        assert_eq!(lod.current, 1);
    }

    #[test]
    fn test_lod_global_force() {
        let mut system = LodSystem::new();
        system.config.global_force_lod = 2;

        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("medium.glb".into(), 50.0),
            ("low.glb".into(), 100.0),
        ]);

        // Even at close distance, should be forced to LOD 2
        system.update_lod(
            &mut lod,
            Vec3::new(0.0, 0.0, 5.0),
            Vec3::ZERO,
            1.0,
            0.016,
        );

        assert_eq!(lod.current, 2);
    }
}
