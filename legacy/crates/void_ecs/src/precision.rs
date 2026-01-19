//! World-Space Precision Management for ECS
//!
//! Components and systems for handling precision in large worlds:
//! - WorldOrigin: Tracks the current rendering origin for precision
//! - PrecisionPosition: High-precision (f64) position component
//! - OriginRebaseSystem: Manages automatic origin rebasing
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::precision::{WorldOrigin, PrecisionPosition};
//!
//! let mut world = World::new();
//!
//! // Set up origin for a scene 1000km from true origin
//! let origin = WorldOrigin::new()
//!     .with_offset([1_000_000.0, 0.0, 0.0])
//!     .with_threshold(10_000.0);
//! world.insert_resource(origin);
//!
//! // Spawn entity with high-precision position
//! let entity = world.spawn();
//! world.add_component(entity, PrecisionPosition::new(1_000_100.0, 50.0, 0.0));
//! ```

use alloc::collections::VecDeque;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::Entity;
use void_math::precision::{check_precision, PrecisionError, PrecisionStatus};

/// Defines the current world origin offset for precision management
///
/// All rendering is done relative to this origin to maintain f32 precision
/// over large distances. When the camera moves far from the origin, the
/// system performs a "rebase" operation to shift everything.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct WorldOrigin {
    /// High-precision world offset (subtracted from all positions)
    pub offset: [f64; 3],

    /// Threshold distance for triggering automatic rebasing (meters)
    pub rebase_threshold: f64,

    /// Position of last rebase operation
    pub last_rebase: [f64; 3],
}

impl Default for WorldOrigin {
    fn default() -> Self {
        Self::new()
    }
}

impl WorldOrigin {
    /// Create a new world origin at the true origin
    pub fn new() -> Self {
        Self {
            offset: [0.0; 3],
            rebase_threshold: 10_000.0, // 10km default
            last_rebase: [0.0; 3],
        }
    }

    /// Set the initial offset
    pub fn with_offset(mut self, offset: [f64; 3]) -> Self {
        self.offset = offset;
        self.last_rebase = offset;
        self
    }

    /// Set the rebase threshold
    pub fn with_threshold(mut self, threshold: f64) -> Self {
        self.rebase_threshold = threshold;
        self
    }

    /// Check if rebasing is needed based on camera position
    pub fn needs_rebase(&self, camera_world_pos: [f64; 3]) -> bool {
        let dx = camera_world_pos[0] - self.last_rebase[0];
        let dy = camera_world_pos[1] - self.last_rebase[1];
        let dz = camera_world_pos[2] - self.last_rebase[2];
        let dist_sq = dx * dx + dy * dy + dz * dz;
        dist_sq > self.rebase_threshold * self.rebase_threshold
    }

    /// Perform rebase operation, returning the delta to apply to all entities
    ///
    /// Returns the offset delta that should be subtracted from all positions.
    pub fn rebase(&mut self, camera_world_pos: [f64; 3]) -> [f64; 3] {
        let delta = [
            camera_world_pos[0] - self.offset[0],
            camera_world_pos[1] - self.offset[1],
            camera_world_pos[2] - self.offset[2],
        ];

        self.offset = camera_world_pos;
        self.last_rebase = camera_world_pos;

        delta
    }

    /// Convert high-precision world position to local space (f32)
    pub fn world_to_local(&self, world: [f64; 3]) -> [f32; 3] {
        [
            (world[0] - self.offset[0]) as f32,
            (world[1] - self.offset[1]) as f32,
            (world[2] - self.offset[2]) as f32,
        ]
    }

    /// Safe conversion with precision checking
    pub fn world_to_local_safe(&self, world: [f64; 3]) -> Result<[f32; 3], PrecisionError> {
        let local = self.world_to_local(world);

        // Check for infinity or NaN
        if local.iter().any(|v| !v.is_finite()) {
            return Err(PrecisionError::Overflow);
        }

        // Check for precision loss
        if check_precision(local) == PrecisionStatus::Critical {
            return Err(PrecisionError::PrecisionLoss);
        }

        Ok(local)
    }

    /// Convert local position (f32) to high-precision world space
    pub fn local_to_world(&self, local: [f32; 3]) -> [f64; 3] {
        [
            self.offset[0] + local[0] as f64,
            self.offset[1] + local[1] as f64,
            self.offset[2] + local[2] as f64,
        ]
    }

    /// Validate the origin state
    pub fn validate(&self) -> Result<(), OriginValidationError> {
        // Check for NaN/infinity
        if !self.offset.iter().all(|v| v.is_finite()) {
            return Err(OriginValidationError::InvalidOffset);
        }

        if !self.last_rebase.iter().all(|v| v.is_finite()) {
            return Err(OriginValidationError::InvalidLastRebase);
        }

        if self.rebase_threshold <= 0.0 || !self.rebase_threshold.is_finite() {
            return Err(OriginValidationError::InvalidThreshold);
        }

        Ok(())
    }

    /// Sanitize invalid values to safe defaults
    pub fn sanitize(&mut self) {
        for i in 0..3 {
            if !self.offset[i].is_finite() {
                self.offset[i] = 0.0;
            }
            if !self.last_rebase[i].is_finite() {
                self.last_rebase[i] = 0.0;
            }
        }

        if self.rebase_threshold <= 0.0 || !self.rebase_threshold.is_finite() {
            self.rebase_threshold = 10_000.0;
        }
    }
}

/// Validation errors for WorldOrigin
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum OriginValidationError {
    /// Offset contains invalid values
    InvalidOffset,
    /// Last rebase position contains invalid values
    InvalidLastRebase,
    /// Threshold is invalid
    InvalidThreshold,
}

impl core::fmt::Display for OriginValidationError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            OriginValidationError::InvalidOffset => write!(f, "Origin offset contains NaN or infinity"),
            OriginValidationError::InvalidLastRebase => write!(f, "Last rebase position contains NaN or infinity"),
            OriginValidationError::InvalidThreshold => write!(f, "Rebase threshold must be positive and finite"),
        }
    }
}

/// High-precision position component for large worlds
///
/// Stores position in f64 for maximum precision, with a cached f32
/// local position relative to the current world origin.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrecisionPosition {
    /// High-precision world position (f64)
    pub world: [f64; 3],

    /// Cached local position relative to origin (f32)
    #[serde(skip)]
    pub local: [f32; 3],

    /// Is the local cache valid?
    #[serde(skip)]
    pub cache_valid: bool,
}

impl Default for PrecisionPosition {
    fn default() -> Self {
        Self {
            world: [0.0; 3],
            local: [0.0; 3],
            cache_valid: false,
        }
    }
}

impl PrecisionPosition {
    /// Create a new precision position
    pub fn new(x: f64, y: f64, z: f64) -> Self {
        Self {
            world: [x, y, z],
            local: [x as f32, y as f32, z as f32],
            cache_valid: false,
        }
    }

    /// Create from array
    pub fn from_array(arr: [f64; 3]) -> Self {
        Self::new(arr[0], arr[1], arr[2])
    }

    /// Update local cache from world origin
    pub fn update_local(&mut self, origin: &WorldOrigin) {
        self.local = origin.world_to_local(self.world);
        self.cache_valid = true;
    }

    /// Get local position (asserts cache is valid in debug mode)
    pub fn local(&self) -> [f32; 3] {
        debug_assert!(self.cache_valid, "PrecisionPosition cache invalid - call update_local first");
        self.local
    }

    /// Try to get local position, returning None if cache invalid
    pub fn try_local(&self) -> Option<[f32; 3]> {
        if self.cache_valid {
            Some(self.local)
        } else {
            None
        }
    }

    /// Set world position (invalidates cache)
    pub fn set_world(&mut self, x: f64, y: f64, z: f64) {
        self.world = [x, y, z];
        self.cache_valid = false;
    }

    /// Translate by delta (invalidates cache)
    pub fn translate(&mut self, dx: f64, dy: f64, dz: f64) {
        self.world[0] += dx;
        self.world[1] += dy;
        self.world[2] += dz;
        self.cache_valid = false;
    }

    /// Invalidate the local cache
    pub fn invalidate_cache(&mut self) {
        self.cache_valid = false;
    }

    /// Check precision status of this position relative to origin
    pub fn check_precision(&self, origin: &WorldOrigin) -> PrecisionStatus {
        let local = origin.world_to_local(self.world);
        check_precision(local)
    }
}

/// Event representing a rebase operation
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RebaseEvent {
    /// Time when rebase occurred
    pub timestamp: f64,
    /// Previous origin offset
    pub old_origin: [f64; 3],
    /// New origin offset
    pub new_origin: [f64; 3],
    /// Delta applied to positions
    pub delta: [f64; 3],
    /// Number of entities affected
    pub entities_affected: u32,
}

/// History of rebase events for debugging and recovery
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RebaseHistory {
    /// Recent rebase events
    pub events: VecDeque<RebaseEvent>,
    /// Maximum events to keep
    pub max_events: usize,
}

impl RebaseHistory {
    /// Create with default capacity
    pub fn new() -> Self {
        Self {
            events: VecDeque::new(),
            max_events: 100,
        }
    }

    /// Create with custom capacity
    pub fn with_capacity(max_events: usize) -> Self {
        Self {
            events: VecDeque::with_capacity(max_events),
            max_events,
        }
    }

    /// Record a rebase event
    pub fn record(&mut self, event: RebaseEvent) {
        if self.events.len() >= self.max_events {
            self.events.pop_front();
        }
        self.events.push_back(event);
    }

    /// Get the most recent event
    pub fn last(&self) -> Option<&RebaseEvent> {
        self.events.back()
    }

    /// Reconstruct origin from history if needed
    pub fn recover_origin(&self) -> Option<WorldOrigin> {
        self.events.back().map(|event| {
            WorldOrigin {
                offset: event.new_origin,
                last_rebase: event.new_origin,
                rebase_threshold: 10_000.0, // Default threshold
            }
        })
    }

    /// Get total number of rebases
    pub fn rebase_count(&self) -> usize {
        self.events.len()
    }
}

/// Configuration for the origin rebase system
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct OriginRebaseConfig {
    /// Whether automatic rebasing is enabled
    pub auto_rebase: bool,
    /// Distance threshold for automatic rebasing
    pub threshold: f64,
    /// Minimum time between rebases (seconds)
    pub min_rebase_interval: f64,
    /// Whether to record rebase history
    pub record_history: bool,
}

impl Default for OriginRebaseConfig {
    fn default() -> Self {
        Self {
            auto_rebase: true,
            threshold: 10_000.0,
            min_rebase_interval: 1.0,
            record_history: true,
        }
    }
}

/// Update request for the precision system
#[derive(Clone, Debug)]
pub enum PrecisionUpdate {
    /// Manually set the origin
    SetOrigin(WorldOrigin),
    /// Force a rebase to the given position
    ForceRebase([f64; 3]),
    /// Invalidate all position caches
    InvalidateAll,
}

/// Queue for precision updates at frame boundaries
#[derive(Clone, Debug, Default)]
pub struct PrecisionUpdateQueue {
    /// Pending updates
    updates: Vec<PrecisionUpdate>,
}

impl PrecisionUpdateQueue {
    /// Create a new update queue
    pub fn new() -> Self {
        Self { updates: Vec::new() }
    }

    /// Queue an origin update
    pub fn queue_origin_update(&mut self, origin: WorldOrigin) {
        self.updates.push(PrecisionUpdate::SetOrigin(origin));
    }

    /// Queue a forced rebase
    pub fn queue_rebase(&mut self, position: [f64; 3]) {
        self.updates.push(PrecisionUpdate::ForceRebase(position));
    }

    /// Queue cache invalidation
    pub fn queue_invalidate_all(&mut self) {
        self.updates.push(PrecisionUpdate::InvalidateAll);
    }

    /// Take all pending updates
    pub fn take_updates(&mut self) -> Vec<PrecisionUpdate> {
        core::mem::take(&mut self.updates)
    }

    /// Check if there are pending updates
    pub fn has_pending(&self) -> bool {
        !self.updates.is_empty()
    }
}

/// Statistics for precision management
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct PrecisionStats {
    /// Total rebase operations performed
    pub total_rebases: u64,
    /// Entities with precision warnings
    pub warning_count: u32,
    /// Entities with critical precision
    pub critical_count: u32,
    /// Current maximum distance from origin
    pub max_distance: f32,
    /// Time of last rebase
    pub last_rebase_time: f64,
}

/// State snapshot for hot-reload support
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrecisionSystemState {
    /// Current world origin
    pub origin: WorldOrigin,
    /// Rebase history
    pub history: RebaseHistory,
    /// Configuration
    pub config: OriginRebaseConfig,
    /// Statistics
    pub stats: PrecisionStats,
}

impl Default for PrecisionSystemState {
    fn default() -> Self {
        Self {
            origin: WorldOrigin::new(),
            history: RebaseHistory::new(),
            config: OriginRebaseConfig::default(),
            stats: PrecisionStats::default(),
        }
    }
}

/// Manager for world-space precision
///
/// Handles origin rebasing and precision position cache management.
#[derive(Clone, Debug)]
pub struct PrecisionManager {
    /// Current world origin
    pub origin: WorldOrigin,
    /// Configuration
    pub config: OriginRebaseConfig,
    /// History of rebases
    pub history: RebaseHistory,
    /// Statistics
    pub stats: PrecisionStats,
    /// Time of last update
    pub last_update_time: f64,
    /// Entities that need cache updates
    pub dirty_entities: Vec<Entity>,
}

impl Default for PrecisionManager {
    fn default() -> Self {
        Self::new(OriginRebaseConfig::default())
    }
}

impl PrecisionManager {
    /// Create a new precision manager
    pub fn new(config: OriginRebaseConfig) -> Self {
        let mut origin = WorldOrigin::new();
        origin.rebase_threshold = config.threshold;

        Self {
            origin,
            config,
            history: RebaseHistory::new(),
            stats: PrecisionStats::default(),
            last_update_time: 0.0,
            dirty_entities: Vec::new(),
        }
    }

    /// Update the precision system based on camera position
    ///
    /// Returns true if a rebase was performed.
    pub fn update(&mut self, camera_world_pos: [f64; 3], current_time: f64) -> bool {
        // Check if we should auto-rebase
        if !self.config.auto_rebase {
            return false;
        }

        // Check minimum interval
        if current_time - self.last_update_time < self.config.min_rebase_interval {
            return false;
        }

        // Check if rebase needed
        if !self.origin.needs_rebase(camera_world_pos) {
            return false;
        }

        // Perform rebase
        self.perform_rebase(camera_world_pos, current_time);
        true
    }

    /// Force a rebase to the given position
    pub fn force_rebase(&mut self, position: [f64; 3], current_time: f64) {
        self.perform_rebase(position, current_time);
    }

    fn perform_rebase(&mut self, position: [f64; 3], current_time: f64) {
        let old_origin = self.origin.offset;
        let delta = self.origin.rebase(position);

        // Record in history
        if self.config.record_history {
            self.history.record(RebaseEvent {
                timestamp: current_time,
                old_origin,
                new_origin: self.origin.offset,
                delta,
                entities_affected: 0, // Updated externally
            });
        }

        // Update stats
        self.stats.total_rebases += 1;
        self.stats.last_rebase_time = current_time;
        self.last_update_time = current_time;
    }

    /// Manually set the origin (for loading scenes at specific positions)
    pub fn set_origin(&mut self, origin: WorldOrigin) {
        self.origin = origin;
    }

    /// Get current origin
    pub fn origin(&self) -> &WorldOrigin {
        &self.origin
    }

    /// Get statistics
    pub fn stats(&self) -> &PrecisionStats {
        &self.stats
    }

    /// Serialize state for hot-reload
    pub fn save_state(&self) -> PrecisionSystemState {
        PrecisionSystemState {
            origin: self.origin.clone(),
            history: self.history.clone(),
            config: self.config.clone(),
            stats: self.stats.clone(),
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: PrecisionSystemState) {
        self.origin = state.origin;
        self.history = state.history;
        self.config = state.config;
        self.stats = state.stats;
    }

    /// Mark an entity as needing cache update
    pub fn mark_dirty(&mut self, entity: Entity) {
        if !self.dirty_entities.contains(&entity) {
            self.dirty_entities.push(entity);
        }
    }

    /// Take list of dirty entities
    pub fn take_dirty(&mut self) -> Vec<Entity> {
        core::mem::take(&mut self.dirty_entities)
    }
}

/// Rendering mode based on precision status
#[derive(Clone, Debug, PartialEq)]
pub enum RenderMode {
    /// Normal rendering with precise position
    Normal { position: [f32; 3] },
    /// Billboard/impostor rendering (direction only)
    Billboard { direction: [f32; 3] },
    /// Object is hidden due to precision issues
    Hidden,
}

/// Determine rendering mode for an entity based on precision
pub fn determine_render_mode(
    position: &PrecisionPosition,
    origin: &WorldOrigin,
) -> RenderMode {
    match origin.world_to_local_safe(position.world) {
        Ok(local) => RenderMode::Normal { position: local },
        Err(PrecisionError::PrecisionLoss) => {
            // Fall back to billboard rendering
            let direction = void_math::precision::direction_from_origin(position.world, origin.offset);
            RenderMode::Billboard { direction }
        }
        Err(_) => RenderMode::Hidden,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_world_origin_new() {
        let origin = WorldOrigin::new();
        assert_eq!(origin.offset, [0.0, 0.0, 0.0]);
        assert_eq!(origin.rebase_threshold, 10_000.0);
    }

    #[test]
    fn test_world_origin_builder() {
        let origin = WorldOrigin::new()
            .with_offset([1000.0, 0.0, 0.0])
            .with_threshold(5000.0);

        assert_eq!(origin.offset, [1000.0, 0.0, 0.0]);
        assert_eq!(origin.rebase_threshold, 5000.0);
    }

    #[test]
    fn test_origin_needs_rebase() {
        let mut origin = WorldOrigin::new().with_threshold(100.0);

        // Near origin - no rebase
        assert!(!origin.needs_rebase([50.0, 0.0, 0.0]));

        // Far from origin - needs rebase
        assert!(origin.needs_rebase([150.0, 0.0, 0.0]));

        // After rebase at new position, close positions don't need rebase
        origin.rebase([150.0, 0.0, 0.0]);
        assert!(!origin.needs_rebase([160.0, 0.0, 0.0]));
    }

    #[test]
    fn test_origin_world_to_local() {
        let origin = WorldOrigin::new()
            .with_offset([1_000_000.0, 0.0, 0.0]);

        let world = [1_000_100.0, 50.0, 0.0];
        let local = origin.world_to_local(world);

        assert!((local[0] - 100.0).abs() < 0.01);
        assert!((local[1] - 50.0).abs() < 0.01);
        assert!((local[2] - 0.0).abs() < 0.01);
    }

    #[test]
    fn test_origin_local_to_world() {
        let origin = WorldOrigin::new()
            .with_offset([1_000_000.0, 500.0, -2_000.0]);

        let local = [100.0f32, 50.0, 0.0];
        let world = origin.local_to_world(local);

        assert!((world[0] - 1_000_100.0).abs() < 0.01);
        assert!((world[1] - 550.0).abs() < 0.01);
        assert!((world[2] - -2_000.0).abs() < 0.01);
    }

    #[test]
    fn test_origin_validate() {
        let valid = WorldOrigin::new();
        assert!(valid.validate().is_ok());

        let invalid = WorldOrigin {
            offset: [f64::NAN, 0.0, 0.0],
            rebase_threshold: 1000.0,
            last_rebase: [0.0, 0.0, 0.0],
        };
        assert_eq!(invalid.validate(), Err(OriginValidationError::InvalidOffset));
    }

    #[test]
    fn test_origin_sanitize() {
        let mut origin = WorldOrigin {
            offset: [f64::NAN, 0.0, f64::INFINITY],
            rebase_threshold: -5.0,
            last_rebase: [0.0, f64::NAN, 0.0],
        };

        origin.sanitize();

        assert_eq!(origin.offset[0], 0.0);
        assert_eq!(origin.offset[2], 0.0);
        assert_eq!(origin.rebase_threshold, 10_000.0);
        assert_eq!(origin.last_rebase[1], 0.0);
    }

    #[test]
    fn test_precision_position_new() {
        let pos = PrecisionPosition::new(1000.0, 500.0, 250.0);
        assert_eq!(pos.world, [1000.0, 500.0, 250.0]);
        assert!(!pos.cache_valid);
    }

    #[test]
    fn test_precision_position_update_local() {
        let mut pos = PrecisionPosition::new(1_000_100.0, 50.0, 0.0);
        let origin = WorldOrigin::new().with_offset([1_000_000.0, 0.0, 0.0]);

        pos.update_local(&origin);

        assert!(pos.cache_valid);
        assert!((pos.local[0] - 100.0).abs() < 0.01);
        assert!((pos.local[1] - 50.0).abs() < 0.01);
    }

    #[test]
    fn test_precision_position_translate() {
        let mut pos = PrecisionPosition::new(100.0, 0.0, 0.0);
        let origin = WorldOrigin::new();
        pos.update_local(&origin);

        assert!(pos.cache_valid);

        pos.translate(50.0, 25.0, 0.0);

        assert!(!pos.cache_valid);
        assert_eq!(pos.world, [150.0, 25.0, 0.0]);
    }

    #[test]
    fn test_rebase_history() {
        let mut history = RebaseHistory::with_capacity(3);

        for i in 0..5 {
            history.record(RebaseEvent {
                timestamp: i as f64,
                old_origin: [0.0; 3],
                new_origin: [i as f64 * 1000.0, 0.0, 0.0],
                delta: [1000.0, 0.0, 0.0],
                entities_affected: 10,
            });
        }

        // Should only keep last 3
        assert_eq!(history.events.len(), 3);
        assert_eq!(history.last().unwrap().timestamp, 4.0);
    }

    #[test]
    fn test_precision_manager_auto_rebase() {
        let config = OriginRebaseConfig {
            auto_rebase: true,
            threshold: 100.0,
            min_rebase_interval: 0.0,
            record_history: true,
        };
        let mut manager = PrecisionManager::new(config);

        // Near origin - no rebase
        assert!(!manager.update([50.0, 0.0, 0.0], 0.0));
        assert_eq!(manager.stats.total_rebases, 0);

        // Far from origin - rebase
        assert!(manager.update([150.0, 0.0, 0.0], 1.0));
        assert_eq!(manager.stats.total_rebases, 1);
        assert_eq!(manager.origin.offset, [150.0, 0.0, 0.0]);
    }

    #[test]
    fn test_precision_manager_disabled_auto_rebase() {
        let config = OriginRebaseConfig {
            auto_rebase: false,
            ..Default::default()
        };
        let mut manager = PrecisionManager::new(config);

        // Even far positions won't trigger rebase
        assert!(!manager.update([1_000_000.0, 0.0, 0.0], 0.0));
        assert_eq!(manager.stats.total_rebases, 0);
    }

    #[test]
    fn test_determine_render_mode_normal() {
        let origin = WorldOrigin::new();
        let position = PrecisionPosition::new(100.0, 50.0, 0.0);

        let mode = determine_render_mode(&position, &origin);

        match mode {
            RenderMode::Normal { position } => {
                assert!((position[0] - 100.0).abs() < 0.01);
                assert!((position[1] - 50.0).abs() < 0.01);
            }
            _ => panic!("Expected Normal render mode"),
        }
    }

    #[test]
    fn test_determine_render_mode_precision_loss() {
        let origin = WorldOrigin::new();
        let position = PrecisionPosition::new(10_000_000.0, 0.0, 0.0); // 10,000 km

        let mode = determine_render_mode(&position, &origin);

        match mode {
            RenderMode::Billboard { direction } => {
                // Direction should point towards the position
                assert!((direction[0] - 1.0).abs() < 0.01);
            }
            _ => panic!("Expected Billboard render mode"),
        }
    }

    #[test]
    fn test_world_origin_serialization() {
        let origin = WorldOrigin::new()
            .with_offset([1_000_000.0, 500.0, -2_000_000.0])
            .with_threshold(50_000.0);

        let serialized = bincode::serialize(&origin).unwrap();
        let restored: WorldOrigin = bincode::deserialize(&serialized).unwrap();

        assert_eq!(origin.offset, restored.offset);
        assert_eq!(origin.rebase_threshold, restored.rebase_threshold);
        assert_eq!(origin.last_rebase, restored.last_rebase);
    }

    #[test]
    fn test_precision_position_serialization() {
        let mut pos = PrecisionPosition::new(1000.0, 500.0, 0.0);
        pos.local = [100.0, 50.0, 0.0];
        pos.cache_valid = true;

        let serialized = bincode::serialize(&pos).unwrap();
        let restored: PrecisionPosition = bincode::deserialize(&serialized).unwrap();

        // World position preserved
        assert_eq!(restored.world, [1000.0, 500.0, 0.0]);
        // Cache invalidated after deserialize (serde skip)
        assert!(!restored.cache_valid);
    }

    #[test]
    fn test_precision_system_state_roundtrip() {
        let state = PrecisionSystemState {
            origin: WorldOrigin::new().with_offset([5000.0, 0.0, 0.0]),
            history: RebaseHistory::new(),
            config: OriginRebaseConfig::default(),
            stats: PrecisionStats {
                total_rebases: 5,
                ..Default::default()
            },
        };

        let serialized = bincode::serialize(&state).unwrap();
        let restored: PrecisionSystemState = bincode::deserialize(&serialized).unwrap();

        assert_eq!(state.origin.offset, restored.origin.offset);
        assert_eq!(state.stats.total_rebases, restored.stats.total_rebases);
    }
}
