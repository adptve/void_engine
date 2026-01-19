//! Streaming Manager
//!
//! Manages scene chunk loading/unloading based on camera position.

use alloc::collections::VecDeque;
use alloc::vec::Vec;
use alloc::vec;
use core::cmp::Ordering;
use serde::{Deserialize, Serialize};
use void_math::Vec3;

#[cfg(feature = "std")]
use std::collections::{HashMap, HashSet};
#[cfg(not(feature = "std"))]
use alloc::collections::BTreeMap as HashMap;
#[cfg(not(feature = "std"))]
use alloc::collections::BTreeSet as HashSet;

use crate::Entity;
use super::chunk::{ChunkId, ChunkState, SceneChunk, ChunkError};

/// Configuration for the streaming system
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct StreamingConfig {
    /// Maximum concurrent chunk loads
    pub max_concurrent_loads: u32,

    /// Maximum chunks to unload per frame
    pub max_unloads_per_frame: u32,

    /// Distance at which to start loading chunks
    pub load_distance: f32,

    /// Distance at which to unload chunks (should be > load_distance)
    pub unload_distance: f32,

    /// Minimum time a chunk must stay loaded (prevents thrashing)
    pub min_loaded_time: f32,

    /// Enable priority-based loading
    pub use_priority: bool,
}

impl Default for StreamingConfig {
    fn default() -> Self {
        Self {
            max_concurrent_loads: 2,
            max_unloads_per_frame: 1,
            load_distance: 100.0,
            unload_distance: 150.0,
            min_loaded_time: 5.0,
            use_priority: true,
        }
    }
}

/// Result of a streaming update
#[derive(Clone, Debug, Default)]
pub struct StreamingUpdate {
    /// Chunks to start loading
    pub to_load: Vec<ChunkId>,
    /// Chunks to unload
    pub to_unload: Vec<ChunkId>,
}

impl StreamingUpdate {
    /// Check if there are any updates
    pub fn has_updates(&self) -> bool {
        !self.to_load.is_empty() || !self.to_unload.is_empty()
    }
}

/// Entity ID recycler for stable IDs across chunk load/unload cycles
#[derive(Clone, Debug, Default)]
pub struct EntityRecycler {
    /// Recycled entity IDs available for reuse
    recycled: VecDeque<Entity>,
    /// Next fresh ID to allocate
    next_id: u32,
    /// Generation counter (incremented on recycle)
    generation: u32,
}

impl EntityRecycler {
    /// Create a new entity recycler
    pub fn new() -> Self {
        Self::default()
    }

    /// Allocate an entity ID (reuses recycled IDs when available)
    pub fn allocate(&mut self) -> Entity {
        if let Some(entity) = self.recycled.pop_front() {
            // Increment generation when reusing
            Entity::new(entity.index(), entity.generation().wrapping_add(1))
        } else {
            let id = self.next_id;
            self.next_id = self.next_id.wrapping_add(1);
            Entity::new(id, 0)
        }
    }

    /// Recycle an entity ID for later reuse
    pub fn recycle(&mut self, entity: Entity) {
        self.recycled.push_back(entity);
        self.generation = self.generation.wrapping_add(1);
    }

    /// Recycle multiple entities
    pub fn recycle_all(&mut self, entities: impl IntoIterator<Item = Entity>) {
        for entity in entities {
            self.recycle(entity);
        }
    }

    /// Get the number of recycled IDs available
    pub fn recycled_count(&self) -> usize {
        self.recycled.len()
    }

    /// Get the next fresh ID that would be allocated
    pub fn next_fresh_id(&self) -> u32 {
        self.next_id
    }

    /// Clear all recycled IDs
    pub fn clear(&mut self) {
        self.recycled.clear();
    }
}

/// Manages scene chunk streaming
#[derive(Clone, Debug)]
pub struct StreamingManager {
    /// All registered chunks
    chunks: HashMap<ChunkId, SceneChunk>,

    /// Currently loaded chunk IDs
    loaded: HashSet<ChunkId>,

    /// Loading queue (ordered by priority/distance)
    load_queue: VecDeque<ChunkId>,

    /// Unload queue
    unload_queue: VecDeque<ChunkId>,

    /// Streaming configuration
    config: StreamingConfig,

    /// Entity ID recycler
    entity_recycler: EntityRecycler,

    /// Current time (for min-loaded-time tracking)
    current_time: f32,

    /// Number of currently loading chunks
    active_loads: u32,
}

impl StreamingManager {
    /// Create a new streaming manager
    pub fn new(config: StreamingConfig) -> Self {
        Self {
            chunks: HashMap::new(),
            loaded: HashSet::new(),
            load_queue: VecDeque::new(),
            unload_queue: VecDeque::new(),
            config,
            entity_recycler: EntityRecycler::new(),
            current_time: 0.0,
            active_loads: 0,
        }
    }

    /// Register a scene chunk
    pub fn register_chunk(&mut self, chunk: SceneChunk) {
        self.chunks.insert(chunk.id, chunk);
    }

    /// Unregister a chunk
    pub fn unregister_chunk(&mut self, id: ChunkId) -> Option<SceneChunk> {
        self.loaded.remove(&id);
        self.load_queue.retain(|&x| x != id);
        self.unload_queue.retain(|&x| x != id);
        self.chunks.remove(&id)
    }

    /// Get a chunk by ID
    pub fn get_chunk(&self, id: ChunkId) -> Option<&SceneChunk> {
        self.chunks.get(&id)
    }

    /// Get a mutable chunk by ID
    pub fn get_chunk_mut(&mut self, id: ChunkId) -> Option<&mut SceneChunk> {
        self.chunks.get_mut(&id)
    }

    /// Check if a chunk is loaded
    pub fn is_chunk_loaded(&self, id: ChunkId) -> bool {
        self.loaded.contains(&id)
    }

    /// Get all registered chunk IDs
    pub fn chunk_ids(&self) -> impl Iterator<Item = ChunkId> + '_ {
        self.chunks.keys().copied()
    }

    /// Get all loaded chunk IDs
    pub fn loaded_chunk_ids(&self) -> impl Iterator<Item = ChunkId> + '_ {
        self.loaded.iter().copied()
    }

    /// Allocate an entity ID
    pub fn allocate_entity(&mut self) -> Entity {
        self.entity_recycler.allocate()
    }

    /// Get configuration
    pub fn config(&self) -> &StreamingConfig {
        &self.config
    }

    /// Update configuration
    pub fn set_config(&mut self, config: StreamingConfig) {
        self.config = config;
    }

    /// Update streaming based on camera position
    pub fn update(&mut self, camera_pos: Vec3) -> StreamingUpdate {
        let mut to_load: Vec<(ChunkId, f32, i32)> = Vec::new();
        let mut to_unload: Vec<ChunkId> = Vec::new();

        // Check which chunks should be loaded/unloaded
        for (id, chunk) in &self.chunks {
            let distance = self.distance_to_chunk(camera_pos, chunk);

            match chunk.state {
                ChunkState::Unloaded | ChunkState::Failed => {
                    if distance <= self.config.load_distance {
                        // Check dependencies
                        if self.dependencies_loaded(chunk) {
                            to_load.push((*id, distance, chunk.priority));
                        }
                    }
                }
                ChunkState::Loaded => {
                    if distance > self.config.unload_distance {
                        // Check if anything depends on this chunk
                        if !self.is_depended_on(*id) {
                            // Check minimum loaded time
                            if chunk.can_unload_by_time(self.current_time, self.config.min_loaded_time) {
                                to_unload.push(*id);
                            }
                        }
                    }
                }
                ChunkState::Loading | ChunkState::Unloading => {
                    // Skip chunks in transient states
                }
            }
        }

        // Sort by distance (closest first), then by priority (highest first)
        if self.config.use_priority {
            to_load.sort_by(|a, b| {
                match b.2.cmp(&a.2) {
                    Ordering::Equal => a.1.partial_cmp(&b.1).unwrap_or(Ordering::Equal),
                    other => other,
                }
            });
        } else {
            to_load.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap_or(Ordering::Equal));
        }

        // Limit loads to max concurrent
        let available_slots = self.config.max_concurrent_loads.saturating_sub(self.active_loads);
        let loads_to_queue: Vec<ChunkId> = to_load
            .into_iter()
            .take(available_slots as usize)
            .filter(|(id, _, _)| !self.load_queue.contains(id))
            .map(|(id, _, _)| id)
            .collect();

        // Queue loads
        for id in &loads_to_queue {
            if let Some(chunk) = self.chunks.get_mut(id) {
                chunk.start_loading();
                self.load_queue.push_back(*id);
                self.active_loads += 1;
            }
        }

        // Limit unloads
        let unloads_to_process: Vec<ChunkId> = to_unload
            .into_iter()
            .take(self.config.max_unloads_per_frame as usize)
            .filter(|id| !self.unload_queue.contains(id))
            .collect();

        // Queue unloads
        for id in &unloads_to_process {
            if let Some(chunk) = self.chunks.get_mut(id) {
                chunk.start_unloading();
                self.unload_queue.push_back(*id);
            }
        }

        StreamingUpdate {
            to_load: loads_to_queue,
            to_unload: unloads_to_process,
        }
    }

    /// Notify that a chunk finished loading
    pub fn on_chunk_loaded(&mut self, id: ChunkId, entities: Vec<Entity>) {
        if let Some(chunk) = self.chunks.get_mut(&id) {
            chunk.finish_loading(entities, self.current_time);
            self.loaded.insert(id);
            self.active_loads = self.active_loads.saturating_sub(1);
            self.load_queue.retain(|&x| x != id);
        }
    }

    /// Notify that a chunk load failed
    pub fn on_chunk_load_failed(&mut self, id: ChunkId) {
        if let Some(chunk) = self.chunks.get_mut(&id) {
            chunk.mark_failed();
            self.active_loads = self.active_loads.saturating_sub(1);
            self.load_queue.retain(|&x| x != id);
        }
    }

    /// Notify that a chunk finished unloading
    pub fn on_chunk_unloaded(&mut self, id: ChunkId) {
        if let Some(chunk) = self.chunks.get_mut(&id) {
            let entities = chunk.finish_unloading();
            // Recycle entity IDs
            self.entity_recycler.recycle_all(entities);
            self.loaded.remove(&id);
            self.unload_queue.retain(|&x| x != id);
        }
    }

    /// Force load a chunk (ignoring distance and dependencies)
    pub fn force_load(&mut self, id: ChunkId) -> Result<(), ChunkError> {
        let chunk = self.chunks.get_mut(&id).ok_or(ChunkError::NotFound(id))?;

        if !chunk.can_load() {
            return Err(ChunkError::InvalidState {
                chunk: id,
                state: chunk.state,
            });
        }

        chunk.start_loading();
        if !self.load_queue.contains(&id) {
            self.load_queue.push_front(id); // High priority
            self.active_loads += 1;
        }

        Ok(())
    }

    /// Force unload a chunk
    pub fn force_unload(&mut self, id: ChunkId) -> Result<(), ChunkError> {
        let chunk = self.chunks.get_mut(&id).ok_or(ChunkError::NotFound(id))?;

        if !chunk.can_unload() {
            return Err(ChunkError::InvalidState {
                chunk: id,
                state: chunk.state,
            });
        }

        chunk.start_unloading();
        if !self.unload_queue.contains(&id) {
            self.unload_queue.push_front(id); // High priority
        }

        Ok(())
    }

    /// Update the current time
    pub fn set_time(&mut self, time: f32) {
        self.current_time = time;
    }

    /// Get streaming statistics
    pub fn stats(&self) -> StreamingStats {
        let mut by_state = [0u32; 5];
        for chunk in self.chunks.values() {
            let idx = match chunk.state {
                ChunkState::Unloaded => 0,
                ChunkState::Loading => 1,
                ChunkState::Loaded => 2,
                ChunkState::Unloading => 3,
                ChunkState::Failed => 4,
            };
            by_state[idx] += 1;
        }

        StreamingStats {
            total_chunks: self.chunks.len(),
            loaded_chunks: self.loaded.len(),
            loading_chunks: self.active_loads as usize,
            queued_loads: self.load_queue.len(),
            queued_unloads: self.unload_queue.len(),
            recycled_entities: self.entity_recycler.recycled_count(),
            chunks_by_state: by_state,
        }
    }

    // Helper methods

    fn distance_to_chunk(&self, pos: Vec3, chunk: &SceneChunk) -> f32 {
        // Distance to closest point on chunk bounds
        let closest = Vec3::new(
            pos.x.clamp(chunk.bounds.min.x, chunk.bounds.max.x),
            pos.y.clamp(chunk.bounds.min.y, chunk.bounds.max.y),
            pos.z.clamp(chunk.bounds.min.z, chunk.bounds.max.z),
        );
        (pos - closest).length()
    }

    fn dependencies_loaded(&self, chunk: &SceneChunk) -> bool {
        chunk
            .dependencies
            .iter()
            .all(|dep| self.loaded.contains(dep))
    }

    fn is_depended_on(&self, id: ChunkId) -> bool {
        self.chunks.values().any(|c| {
            c.state == ChunkState::Loaded && c.dependencies.contains(&id)
        })
    }
}

impl Default for StreamingManager {
    fn default() -> Self {
        Self::new(StreamingConfig::default())
    }
}

/// Streaming system statistics
#[derive(Clone, Debug, Default)]
pub struct StreamingStats {
    pub total_chunks: usize,
    pub loaded_chunks: usize,
    pub loading_chunks: usize,
    pub queued_loads: usize,
    pub queued_unloads: usize,
    pub recycled_entities: usize,
    /// [unloaded, loading, loaded, unloading, failed]
    pub chunks_by_state: [u32; 5],
}

// ============================================================================
// Serializable State
// ============================================================================

/// Serializable streaming manager state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct StreamingManagerState {
    pub chunks: Vec<SceneChunk>,
    pub loaded: Vec<ChunkId>,
    pub config: StreamingConfig,
    pub entity_recycler_next_id: u32,
    pub entity_recycler_pool: Vec<(u32, u32)>, // (index, generation)
    pub current_time: f32,
}

impl StreamingManager {
    /// Serialize to state
    pub fn to_state(&self) -> StreamingManagerState {
        StreamingManagerState {
            chunks: self.chunks.values().cloned().collect(),
            loaded: self.loaded.iter().copied().collect(),
            config: self.config.clone(),
            entity_recycler_next_id: self.entity_recycler.next_id,
            entity_recycler_pool: self
                .entity_recycler
                .recycled
                .iter()
                .map(|e| (e.index(), e.generation()))
                .collect(),
            current_time: self.current_time,
        }
    }

    /// Restore from state
    pub fn from_state(state: &StreamingManagerState) -> Self {
        let chunks: HashMap<ChunkId, SceneChunk> = state
            .chunks
            .iter()
            .map(|c| (c.id, c.clone()))
            .collect();

        let loaded: HashSet<ChunkId> = state.loaded.iter().copied().collect();

        let mut entity_recycler = EntityRecycler::new();
        entity_recycler.next_id = state.entity_recycler_next_id;
        entity_recycler.recycled = state
            .entity_recycler_pool
            .iter()
            .map(|(idx, gen)| Entity::new(*idx, *gen))
            .collect();

        Self {
            chunks,
            loaded,
            load_queue: VecDeque::new(),
            unload_queue: VecDeque::new(),
            config: state.config.clone(),
            entity_recycler,
            current_time: state.current_time,
            active_loads: 0,
        }
    }
}

// ============================================================================
// Streaming Update Queue (for frame-boundary processing)
// ============================================================================

/// Streaming update operations
#[derive(Clone, Debug)]
pub enum StreamingManagerUpdate {
    /// Restore full state
    RestoreState(StreamingManagerState),
    /// Update configuration
    UpdateConfig(StreamingConfig),
    /// Force load specific chunks
    ForceLoad(Vec<ChunkId>),
    /// Force unload specific chunks
    ForceUnload(Vec<ChunkId>),
}

/// Queue for streaming updates
#[derive(Clone, Debug, Default)]
pub struct StreamingUpdateQueue {
    updates: VecDeque<StreamingManagerUpdate>,
}

impl StreamingUpdateQueue {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, update: StreamingManagerUpdate) {
        self.updates.push_back(update);
    }

    pub fn pop(&mut self) -> Option<StreamingManagerUpdate> {
        self.updates.pop_front()
    }

    pub fn is_empty(&self) -> bool {
        self.updates.is_empty()
    }

    pub fn len(&self) -> usize {
        self.updates.len()
    }

    pub fn clear(&mut self) {
        self.updates.clear();
    }
}

// ============================================================================
// Health Status
// ============================================================================

/// Health status of the streaming system
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StreamingHealthStatus {
    /// All systems normal
    Healthy,
    /// Some chunks failed to load
    Degraded { failed_chunks: u32 },
    /// Critical failure
    Critical { consecutive_failures: u32 },
}

impl StreamingManager {
    /// Get the health status of the streaming system
    pub fn health_status(&self) -> StreamingHealthStatus {
        let failed = self
            .chunks
            .values()
            .filter(|c| c.state == ChunkState::Failed)
            .count() as u32;

        if failed == 0 {
            StreamingHealthStatus::Healthy
        } else if failed < 5 {
            StreamingHealthStatus::Degraded { failed_chunks: failed }
        } else {
            StreamingHealthStatus::Critical {
                consecutive_failures: failed,
            }
        }
    }

    /// Check if streaming should be paused due to errors
    pub fn should_pause_streaming(&self) -> bool {
        matches!(self.health_status(), StreamingHealthStatus::Critical { .. })
    }

    /// Reset failed chunks to unloaded state
    pub fn reset_failed_chunks(&mut self) {
        for chunk in self.chunks.values_mut() {
            if chunk.state == ChunkState::Failed {
                chunk.state = ChunkState::Unloaded;
            }
        }
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use void_math::AABB;

    #[test]
    fn test_entity_recycler() {
        let mut recycler = EntityRecycler::new();

        let e1 = recycler.allocate();
        let e2 = recycler.allocate();
        assert_eq!(e1.index(), 0);
        assert_eq!(e2.index(), 1);

        recycler.recycle(e1);

        let e3 = recycler.allocate();
        assert_eq!(e3.index(), 0); // Reused ID
        assert_eq!(e3.generation(), 1); // Incremented generation
    }

    #[test]
    fn test_streaming_manager_registration() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        let chunk = SceneChunk::new(
            ChunkId(1),
            "test.toml",
            AABB::new(Vec3::ZERO, Vec3::new(100.0, 50.0, 100.0)),
        );

        manager.register_chunk(chunk);

        assert!(manager.get_chunk(ChunkId(1)).is_some());
        assert!(manager.get_chunk(ChunkId(2)).is_none());
    }

    #[test]
    fn test_chunk_loading_by_distance() {
        let mut config = StreamingConfig::default();
        config.load_distance = 50.0;
        config.max_concurrent_loads = 5;

        let mut manager = StreamingManager::new(config);

        // Chunk at origin (within load distance from origin)
        let chunk1 = SceneChunk::new(
            ChunkId(1),
            "near.toml",
            AABB::new(Vec3::new(-10.0, 0.0, -10.0), Vec3::new(10.0, 10.0, 10.0)),
        );

        // Chunk far away
        let chunk2 = SceneChunk::new(
            ChunkId(2),
            "far.toml",
            AABB::new(Vec3::new(200.0, 0.0, 200.0), Vec3::new(220.0, 10.0, 220.0)),
        );

        manager.register_chunk(chunk1);
        manager.register_chunk(chunk2);

        let update = manager.update(Vec3::ZERO);

        assert!(update.to_load.contains(&ChunkId(1)));
        assert!(!update.to_load.contains(&ChunkId(2)));
    }

    #[test]
    fn test_chunk_dependencies() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        let chunk_a = SceneChunk::new(
            ChunkId(1),
            "a.toml",
            AABB::new(Vec3::ZERO, Vec3::ONE),
        );

        let chunk_b = SceneChunk::new(
            ChunkId(2),
            "b.toml",
            AABB::new(Vec3::ZERO, Vec3::ONE),
        )
        .with_dependency(ChunkId(1));

        manager.register_chunk(chunk_a);
        manager.register_chunk(chunk_b);

        // B should wait for A
        let update = manager.update(Vec3::ZERO);
        assert!(update.to_load.contains(&ChunkId(1)));
        assert!(!update.to_load.contains(&ChunkId(2))); // Blocked by dependency

        // Load A
        manager.on_chunk_loaded(ChunkId(1), vec![]);

        // Now B should be loadable
        let update = manager.update(Vec3::ZERO);
        assert!(update.to_load.contains(&ChunkId(2)));
    }

    #[test]
    fn test_chunk_unloading() {
        let mut config = StreamingConfig::default();
        config.load_distance = 50.0;
        config.unload_distance = 100.0;
        config.min_loaded_time = 0.0; // Disable time check for test

        let mut manager = StreamingManager::new(config);

        let chunk = SceneChunk::new(
            ChunkId(1),
            "test.toml",
            AABB::new(Vec3::ZERO, Vec3::new(10.0, 10.0, 10.0)),
        );

        manager.register_chunk(chunk);

        // Load the chunk
        let _ = manager.update(Vec3::ZERO);
        manager.on_chunk_loaded(ChunkId(1), vec![Entity::new(0, 0)]);

        // Move camera far away
        let update = manager.update(Vec3::new(500.0, 0.0, 500.0));

        assert!(update.to_unload.contains(&ChunkId(1)));
    }

    #[test]
    fn test_streaming_serialization() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        let chunk = SceneChunk::new(
            ChunkId(1),
            "test.toml",
            AABB::new(Vec3::ZERO, Vec3::ONE),
        );

        manager.register_chunk(chunk);
        manager.on_chunk_loaded(ChunkId(1), vec![Entity::new(0, 0), Entity::new(1, 0)]);

        let state = manager.to_state();
        let restored = StreamingManager::from_state(&state);

        assert_eq!(restored.chunks.len(), 1);
        assert!(restored.loaded.contains(&ChunkId(1)));
    }

    #[test]
    fn test_health_status() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        assert_eq!(manager.health_status(), StreamingHealthStatus::Healthy);

        // Add a failed chunk
        let mut chunk = SceneChunk::new(ChunkId(1), "test.toml", AABB::default());
        chunk.mark_failed();
        manager.chunks.insert(ChunkId(1), chunk);

        assert!(matches!(
            manager.health_status(),
            StreamingHealthStatus::Degraded { .. }
        ));
    }

    #[test]
    fn test_stats() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        manager.register_chunk(SceneChunk::new(ChunkId(1), "a.toml", AABB::default()));
        manager.register_chunk(SceneChunk::new(ChunkId(2), "b.toml", AABB::default()));

        manager.on_chunk_loaded(ChunkId(1), vec![]);

        let stats = manager.stats();
        assert_eq!(stats.total_chunks, 2);
        assert_eq!(stats.loaded_chunks, 1);
    }
}
