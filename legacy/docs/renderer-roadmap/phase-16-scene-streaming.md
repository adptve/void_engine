# Phase 16: Scene Streaming (Foundational)

## Status: Not Started

## User Story

> As an application author, I want to load and unload scene content dynamically.

## Requirements Checklist

- [ ] Allow scenes to be split into chunks
- [ ] Load/unload entities at runtime
- [ ] Preserve entity IDs across loads
- [ ] Explicit control over streaming boundaries
- [ ] No implicit threading assumptions

## Implementation Specification

### 1. Chunk Definition

```rust
// crates/void_ecs/src/streaming/chunk.rs (NEW FILE)

use std::collections::HashSet;

/// A chunk of streamable scene content
#[derive(Clone, Debug)]
pub struct SceneChunk {
    /// Unique chunk identifier
    pub id: ChunkId,

    /// Asset path for this chunk
    pub path: String,

    /// World-space bounds
    pub bounds: AABB,

    /// Priority (higher = load first)
    pub priority: i32,

    /// Dependencies (must be loaded before this)
    pub dependencies: Vec<ChunkId>,

    /// Current load state
    pub state: ChunkState,

    /// Entities owned by this chunk
    pub entities: Vec<Entity>,

    /// Referenced assets
    pub assets: Vec<String>,
}

/// Unique identifier for a chunk
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct ChunkId(pub u64);

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum ChunkState {
    #[default]
    Unloaded,
    Loading,
    Loaded,
    Unloading,
    Failed,
}

impl SceneChunk {
    pub fn new(id: ChunkId, path: impl Into<String>, bounds: AABB) -> Self {
        Self {
            id,
            path: path.into(),
            bounds,
            priority: 0,
            dependencies: Vec::new(),
            state: ChunkState::Unloaded,
            entities: Vec::new(),
            assets: Vec::new(),
        }
    }

    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    pub fn with_dependency(mut self, dep: ChunkId) -> Self {
        self.dependencies.push(dep);
        self
    }

    pub fn is_ready(&self) -> bool {
        self.state == ChunkState::Loaded
    }
}
```

### 2. Streaming Manager

```rust
// crates/void_ecs/src/streaming/manager.rs (NEW FILE)

use std::collections::{HashMap, HashSet, VecDeque};
use crate::streaming::chunk::*;

/// Manages scene chunk streaming
pub struct StreamingManager {
    /// All registered chunks
    chunks: HashMap<ChunkId, SceneChunk>,

    /// Currently loaded chunks
    loaded: HashSet<ChunkId>,

    /// Loading queue
    load_queue: VecDeque<ChunkId>,

    /// Unload queue
    unload_queue: VecDeque<ChunkId>,

    /// Streaming configuration
    config: StreamingConfig,

    /// Entity ID recycler
    entity_recycler: EntityRecycler,
}

#[derive(Clone, Debug)]
pub struct StreamingConfig {
    /// Maximum concurrent loads
    pub max_concurrent_loads: u32,

    /// Maximum chunks to unload per frame
    pub max_unloads_per_frame: u32,

    /// Distance to start loading
    pub load_distance: f32,

    /// Distance to unload (should be > load_distance)
    pub unload_distance: f32,

    /// Minimum time chunk stays loaded (prevents thrashing)
    pub min_loaded_time: f32,
}

impl Default for StreamingConfig {
    fn default() -> Self {
        Self {
            max_concurrent_loads: 2,
            max_unloads_per_frame: 1,
            load_distance: 100.0,
            unload_distance: 150.0,
            min_loaded_time: 5.0,
        }
    }
}

impl StreamingManager {
    pub fn new(config: StreamingConfig) -> Self {
        Self {
            chunks: HashMap::new(),
            loaded: HashSet::new(),
            load_queue: VecDeque::new(),
            unload_queue: VecDeque::new(),
            config,
            entity_recycler: EntityRecycler::new(),
        }
    }

    /// Register a chunk
    pub fn register_chunk(&mut self, chunk: SceneChunk) {
        self.chunks.insert(chunk.id, chunk);
    }

    /// Update streaming based on camera position
    pub fn update(&mut self, camera_pos: Vec3) -> StreamingUpdate {
        let mut to_load = Vec::new();
        let mut to_unload = Vec::new();

        // Check which chunks should be loaded/unloaded
        for (id, chunk) in &self.chunks {
            let distance = self.distance_to_chunk(camera_pos, chunk);

            if chunk.state == ChunkState::Unloaded {
                if distance <= self.config.load_distance {
                    // Check dependencies
                    if self.dependencies_loaded(chunk) {
                        to_load.push((*id, distance));
                    }
                }
            } else if chunk.state == ChunkState::Loaded {
                if distance > self.config.unload_distance {
                    // Check if anything depends on this
                    if !self.is_depended_on(*id) {
                        to_unload.push(*id);
                    }
                }
            }
        }

        // Sort by distance (closest first)
        to_load.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap());

        // Queue loads
        for (id, _) in to_load.into_iter().take(self.config.max_concurrent_loads as usize) {
            if !self.load_queue.contains(&id) {
                self.load_queue.push_back(id);
            }
        }

        // Queue unloads
        for id in to_unload.into_iter().take(self.config.max_unloads_per_frame as usize) {
            if !self.unload_queue.contains(&id) {
                self.unload_queue.push_back(id);
            }
        }

        // Process queues
        let loads: Vec<_> = (0..self.config.max_concurrent_loads)
            .filter_map(|_| self.load_queue.pop_front())
            .collect();

        let unloads: Vec<_> = (0..self.config.max_unloads_per_frame)
            .filter_map(|_| self.unload_queue.pop_front())
            .collect();

        // Mark states
        for id in &loads {
            if let Some(chunk) = self.chunks.get_mut(id) {
                chunk.state = ChunkState::Loading;
            }
        }

        for id in &unloads {
            if let Some(chunk) = self.chunks.get_mut(id) {
                chunk.state = ChunkState::Unloading;
            }
        }

        StreamingUpdate {
            to_load: loads,
            to_unload: unloads,
        }
    }

    /// Mark chunk as loaded
    pub fn on_chunk_loaded(&mut self, id: ChunkId, entities: Vec<Entity>) {
        if let Some(chunk) = self.chunks.get_mut(&id) {
            chunk.state = ChunkState::Loaded;
            chunk.entities = entities;
            self.loaded.insert(id);
        }
    }

    /// Mark chunk as unloaded
    pub fn on_chunk_unloaded(&mut self, id: ChunkId) {
        if let Some(chunk) = self.chunks.get_mut(&id) {
            // Recycle entity IDs
            for entity in &chunk.entities {
                self.entity_recycler.recycle(*entity);
            }

            chunk.state = ChunkState::Unloaded;
            chunk.entities.clear();
            self.loaded.remove(&id);
        }
    }

    /// Allocate entity ID (may reuse recycled)
    pub fn allocate_entity(&mut self) -> Entity {
        self.entity_recycler.allocate()
    }

    fn distance_to_chunk(&self, pos: Vec3, chunk: &SceneChunk) -> f32 {
        // Distance to chunk bounds
        let closest = Vec3::new(
            pos.x.clamp(chunk.bounds.min.x, chunk.bounds.max.x),
            pos.y.clamp(chunk.bounds.min.y, chunk.bounds.max.y),
            pos.z.clamp(chunk.bounds.min.z, chunk.bounds.max.z),
        );
        (pos - closest).length()
    }

    fn dependencies_loaded(&self, chunk: &SceneChunk) -> bool {
        chunk.dependencies.iter().all(|dep| self.loaded.contains(dep))
    }

    fn is_depended_on(&self, id: ChunkId) -> bool {
        self.chunks.values().any(|c| {
            c.state == ChunkState::Loaded && c.dependencies.contains(&id)
        })
    }
}

/// Streaming update result
#[derive(Clone, Debug)]
pub struct StreamingUpdate {
    pub to_load: Vec<ChunkId>,
    pub to_unload: Vec<ChunkId>,
}

/// Recycles entity IDs to maintain stability
struct EntityRecycler {
    recycled: VecDeque<Entity>,
    next_id: u32,
}

impl EntityRecycler {
    fn new() -> Self {
        Self {
            recycled: VecDeque::new(),
            next_id: 0,
        }
    }

    fn allocate(&mut self) -> Entity {
        self.recycled.pop_front().unwrap_or_else(|| {
            let id = self.next_id;
            self.next_id += 1;
            Entity::from_raw(id)
        })
    }

    fn recycle(&mut self, entity: Entity) {
        self.recycled.push_back(entity);
    }
}
```

### 3. Chunk Loader

```rust
// crates/void_ecs/src/streaming/loader.rs (NEW FILE)

use crate::{World, Entity};
use crate::streaming::chunk::*;

/// Loads chunk content into the world
pub struct ChunkLoader;

impl ChunkLoader {
    /// Load chunk asynchronously (returns immediately, loading happens in background)
    pub fn load_async(
        chunk_id: ChunkId,
        path: &str,
        on_complete: impl FnOnce(Result<Vec<Entity>, ChunkLoadError>) + Send + 'static,
    ) {
        // Spawn background task
        std::thread::spawn(move || {
            let result = Self::load_sync(&path);
            on_complete(result);
        });
    }

    /// Load chunk synchronously
    pub fn load_sync(path: &str) -> Result<Vec<Entity>, ChunkLoadError> {
        // Load scene file
        let content = std::fs::read_to_string(path)
            .map_err(|e| ChunkLoadError::Io(e.to_string()))?;

        // Parse (TOML format)
        let scene: SceneData = toml::from_str(&content)
            .map_err(|e| ChunkLoadError::Parse(e.to_string()))?;

        Ok(scene.entities)
    }

    /// Spawn chunk entities into world
    pub fn spawn_into_world(
        world: &mut World,
        entities: &[EntityData],
        id_allocator: &mut impl FnMut() -> Entity,
    ) -> Vec<Entity> {
        let mut spawned = Vec::new();

        for entity_data in entities {
            let entity = id_allocator();

            // Spawn with components
            world.spawn_with_id(entity, (
                // Transform
                LocalTransform {
                    translation: entity_data.position,
                    rotation: entity_data.rotation,
                    scale: entity_data.scale,
                },
                // Mesh
                MeshRenderer::from_asset(&entity_data.mesh),
                // Material
                Material {
                    base_color: entity_data.color,
                    ..Default::default()
                },
            ));

            spawned.push(entity);
        }

        spawned
    }

    /// Despawn chunk entities
    pub fn despawn_from_world(world: &mut World, entities: &[Entity]) {
        for entity in entities {
            world.despawn(*entity);
        }
    }
}

#[derive(Debug)]
pub enum ChunkLoadError {
    Io(String),
    Parse(String),
    Asset(String),
}
```

### 4. Streaming Integration

```rust
// crates/void_runtime/src/streaming.rs (NEW FILE)

use void_ecs::streaming::{StreamingManager, ChunkLoader, ChunkId};
use std::sync::mpsc::{channel, Receiver, Sender};

/// Runtime streaming coordinator
pub struct StreamingCoordinator {
    manager: StreamingManager,
    world: Arc<Mutex<World>>,

    /// Completed load notifications
    load_rx: Receiver<(ChunkId, Result<Vec<Entity>, ChunkLoadError>)>,
    load_tx: Sender<(ChunkId, Result<Vec<Entity>, ChunkLoadError>)>,

    /// Active loads
    active_loads: u32,
}

impl StreamingCoordinator {
    pub fn new(world: Arc<Mutex<World>>, config: StreamingConfig) -> Self {
        let (load_tx, load_rx) = channel();

        Self {
            manager: StreamingManager::new(config),
            world,
            load_rx,
            load_tx,
            active_loads: 0,
        }
    }

    /// Update each frame
    pub fn update(&mut self, camera_pos: Vec3) {
        // Process completed loads
        while let Ok((id, result)) = self.load_rx.try_recv() {
            self.active_loads -= 1;

            match result {
                Ok(entities) => {
                    // Spawn into world
                    let mut world = self.world.lock().unwrap();
                    let spawned = ChunkLoader::spawn_into_world(
                        &mut world,
                        &entities,
                        &mut || self.manager.allocate_entity(),
                    );
                    self.manager.on_chunk_loaded(id, spawned);
                }
                Err(e) => {
                    log::error!("Failed to load chunk {:?}: {:?}", id, e);
                    // Mark as failed
                }
            }
        }

        // Get streaming decisions
        let update = self.manager.update(camera_pos);

        // Start loads
        for id in update.to_load {
            if let Some(chunk) = self.manager.chunks.get(&id) {
                let path = chunk.path.clone();
                let tx = self.load_tx.clone();

                ChunkLoader::load_async(id, &path, move |result| {
                    let _ = tx.send((id, result));
                });

                self.active_loads += 1;
            }
        }

        // Process unloads
        for id in update.to_unload {
            if let Some(chunk) = self.manager.chunks.get(&id) {
                let mut world = self.world.lock().unwrap();
                ChunkLoader::despawn_from_world(&mut world, &chunk.entities);
                self.manager.on_chunk_unloaded(id);
            }
        }
    }

    /// Force load a specific chunk (blocking)
    pub fn force_load(&mut self, id: ChunkId) {
        if let Some(chunk) = self.manager.chunks.get(&id) {
            if let Ok(entities) = ChunkLoader::load_sync(&chunk.path) {
                let mut world = self.world.lock().unwrap();
                let spawned = ChunkLoader::spawn_into_world(
                    &mut world,
                    &entities,
                    &mut || self.manager.allocate_entity(),
                );
                self.manager.on_chunk_loaded(id, spawned);
            }
        }
    }

    /// Get loading progress
    pub fn loading_progress(&self) -> (u32, u32) {
        let total = self.manager.chunks.len() as u32;
        let loaded = self.manager.loaded.len() as u32;
        (loaded, total)
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/streaming/mod.rs` | CREATE | Streaming module |
| `void_ecs/src/streaming/chunk.rs` | CREATE | Chunk definition |
| `void_ecs/src/streaming/manager.rs` | CREATE | Streaming manager |
| `void_ecs/src/streaming/loader.rs` | CREATE | Chunk loader |
| `void_runtime/src/streaming.rs` | CREATE | Runtime integration |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_chunk_loading_order() {
    let mut manager = StreamingManager::new(StreamingConfig::default());

    let chunk_a = SceneChunk::new(ChunkId(1), "a.toml", AABB::default());
    let chunk_b = SceneChunk::new(ChunkId(2), "b.toml", AABB::default())
        .with_dependency(ChunkId(1));

    manager.register_chunk(chunk_a);
    manager.register_chunk(chunk_b);

    // B should wait for A
    let update = manager.update(Vec3::ZERO);
    assert!(update.to_load.contains(&ChunkId(1)));
    assert!(!update.to_load.contains(&ChunkId(2)));
}

#[test]
fn test_entity_id_recycling() {
    let mut recycler = EntityRecycler::new();

    let e1 = recycler.allocate();
    let e2 = recycler.allocate();

    recycler.recycle(e1);

    let e3 = recycler.allocate();
    assert_eq!(e3, e1);  // Reused ID
}
```

## Hot-Swap Support

### Serialization

All streaming components derive `Serialize` and `Deserialize` for state preservation:

```rust
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SceneChunk {
    pub id: ChunkId,
    pub path: String,
    pub bounds: AABB,
    pub priority: i32,
    pub dependencies: Vec<ChunkId>,
    pub state: ChunkState,
    pub entities: Vec<Entity>,
    pub assets: Vec<String>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ChunkId(pub u64);

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum ChunkState {
    #[default]
    Unloaded,
    Loading,
    Loaded,
    Unloading,
    Failed,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct StreamingConfig {
    pub max_concurrent_loads: u32,
    pub max_unloads_per_frame: u32,
    pub load_distance: f32,
    pub unload_distance: f32,
    pub min_loaded_time: f32,
}

#[derive(Serialize, Deserialize)]
pub struct StreamingManagerState {
    pub chunks: HashMap<ChunkId, SceneChunk>,
    pub loaded: HashSet<ChunkId>,
    pub config: StreamingConfig,
    pub entity_recycler_next_id: u32,
    pub entity_recycler_pool: VecDeque<u32>,
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, HotReloadContext};

impl HotReloadable for StreamingManager {
    fn type_name(&self) -> &'static str {
        "StreamingManager"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = StreamingManagerState {
            chunks: self.chunks.clone(),
            loaded: self.loaded.clone(),
            config: self.config.clone(),
            entity_recycler_next_id: self.entity_recycler.next_id,
            entity_recycler_pool: self.entity_recycler.recycled.iter().map(|e| e.index()).collect(),
        };
        bincode::serialize(&state).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        let state: StreamingManagerState = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;

        self.chunks = state.chunks;
        self.loaded = state.loaded;
        self.config = state.config;
        self.entity_recycler.next_id = state.entity_recycler_next_id;
        self.entity_recycler.recycled = state.entity_recycler_pool
            .into_iter()
            .map(Entity::from_raw)
            .collect();

        // Clear queues - they will be repopulated on next update
        self.load_queue.clear();
        self.unload_queue.clear();

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}
```

### Asset Dependencies

```rust
use void_asset::AssetDependent;

impl AssetDependent for SceneChunk {
    fn asset_paths(&self) -> Vec<&str> {
        let mut paths = vec![self.path.as_str()];
        paths.extend(self.assets.iter().map(|s| s.as_str()));
        paths
    }

    fn on_asset_changed(&mut self, path: &str) -> AssetReloadAction {
        if path == self.path {
            // Chunk file changed - mark for reload
            AssetReloadAction::Reload
        } else if self.assets.contains(&path.to_string()) {
            // Referenced asset changed - may need partial update
            AssetReloadAction::Update
        } else {
            AssetReloadAction::None
        }
    }
}

impl AssetDependent for StreamingManager {
    fn asset_paths(&self) -> Vec<&str> {
        self.chunks.values()
            .flat_map(|c| c.asset_paths())
            .collect()
    }

    fn on_asset_changed(&mut self, path: &str) -> AssetReloadAction {
        for chunk in self.chunks.values_mut() {
            if chunk.path == path && chunk.state == ChunkState::Loaded {
                // Mark loaded chunk for hot-reload
                chunk.state = ChunkState::Loading;
                self.load_queue.push_back(chunk.id);
                return AssetReloadAction::Reload;
            }
        }
        AssetReloadAction::None
    }
}
```

### Frame-Boundary Updates

```rust
pub struct StreamingUpdateQueue {
    pending_reloads: Vec<(ChunkId, StreamingManagerState)>,
}

impl StreamingUpdateQueue {
    pub fn queue_reload(&mut self, chunk_id: ChunkId, state: StreamingManagerState) {
        self.pending_reloads.push((chunk_id, state));
    }

    /// Apply queued updates at frame boundary (safe point)
    pub fn apply_at_frame_boundary(&mut self, manager: &mut StreamingManager) {
        for (chunk_id, state) in self.pending_reloads.drain(..) {
            // Restore state while preserving entity ID stability
            if let Err(e) = manager.deserialize_state(&bincode::serialize(&state).unwrap()) {
                log::error!("Failed to apply streaming state: {:?}", e);
            }
        }
    }
}
```

### Entity ID Stability Across Hot-Reload

```rust
/// Ensures entity IDs remain stable across hot-reload cycles
#[derive(Serialize, Deserialize)]
pub struct EntityIdMapping {
    /// Maps chunk-local entity index to world entity ID
    pub mappings: HashMap<ChunkId, Vec<Entity>>,
    /// Generation counter for detecting stale references
    pub generation: u64,
}

impl EntityIdMapping {
    pub fn preserve_ids(&mut self, chunk_id: ChunkId, entities: &[Entity]) {
        self.mappings.insert(chunk_id, entities.to_vec());
    }

    pub fn restore_ids(&self, chunk_id: ChunkId) -> Option<&[Entity]> {
        self.mappings.get(&chunk_id).map(|v| v.as_slice())
    }

    pub fn increment_generation(&mut self) {
        self.generation += 1;
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_streaming_manager_serialization_roundtrip() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        let chunk = SceneChunk::new(ChunkId(1), "test.toml", AABB::default());
        manager.register_chunk(chunk);
        manager.on_chunk_loaded(ChunkId(1), vec![Entity::from_raw(0), Entity::from_raw(1)]);

        let serialized = manager.serialize_state().unwrap();

        let mut restored = StreamingManager::new(StreamingConfig::default());
        restored.deserialize_state(&serialized).unwrap();

        assert_eq!(restored.chunks.len(), 1);
        assert!(restored.loaded.contains(&ChunkId(1)));
    }

    #[test]
    fn test_entity_id_stability_across_reload() {
        let mut manager = StreamingManager::new(StreamingConfig::default());
        let mut id_mapping = EntityIdMapping::default();

        // Simulate chunk load
        let entities = vec![Entity::from_raw(5), Entity::from_raw(10)];
        manager.on_chunk_loaded(ChunkId(1), entities.clone());
        id_mapping.preserve_ids(ChunkId(1), &entities);

        // Hot-reload
        let state = manager.serialize_state().unwrap();
        let mut new_manager = StreamingManager::new(StreamingConfig::default());
        new_manager.deserialize_state(&state).unwrap();

        // Verify IDs preserved
        let restored_ids = id_mapping.restore_ids(ChunkId(1)).unwrap();
        assert_eq!(restored_ids, &entities);
    }

    #[test]
    fn test_chunk_state_preserved() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        manager.register_chunk(SceneChunk::new(ChunkId(1), "a.toml", AABB::default()));
        manager.register_chunk(SceneChunk::new(ChunkId(2), "b.toml", AABB::default()));

        manager.on_chunk_loaded(ChunkId(1), vec![]);

        let state = manager.serialize_state().unwrap();
        let mut restored = StreamingManager::new(StreamingConfig::default());
        restored.deserialize_state(&state).unwrap();

        assert_eq!(restored.chunks[&ChunkId(1)].state, ChunkState::Loaded);
        assert_eq!(restored.chunks[&ChunkId(2)].state, ChunkState::Unloaded);
    }

    #[test]
    fn test_entity_recycler_preserved() {
        let mut manager = StreamingManager::new(StreamingConfig::default());

        // Allocate and recycle some entities
        let e1 = manager.allocate_entity();
        let e2 = manager.allocate_entity();
        manager.on_chunk_unloaded(ChunkId(1)); // This recycles entities

        let state = manager.serialize_state().unwrap();
        let mut restored = StreamingManager::new(StreamingConfig::default());
        restored.deserialize_state(&state).unwrap();

        // Next allocation should reuse recycled IDs
        let e3 = restored.allocate_entity();
        assert!(e3.index() <= e2.index());
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
impl StreamingCoordinator {
    pub fn update_with_recovery(&mut self, camera_pos: Vec3) {
        // Wrap streaming update in catch_unwind
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.update(camera_pos)
        }));

        match result {
            Ok(()) => {}
            Err(panic) => {
                log::error!("Streaming update panicked: {:?}", panic);
                self.recover_from_panic();
            }
        }
    }

    fn recover_from_panic(&mut self) {
        // Clear in-flight operations
        self.active_loads = 0;

        // Reset chunks in transient states to safe states
        for chunk in self.manager.chunks.values_mut() {
            match chunk.state {
                ChunkState::Loading => chunk.state = ChunkState::Unloaded,
                ChunkState::Unloading => chunk.state = ChunkState::Loaded,
                _ => {}
            }
        }

        // Clear queues
        self.manager.load_queue.clear();
        self.manager.unload_queue.clear();

        log::warn!("Streaming manager recovered from panic");
    }
}
```

### Chunk Load Fallback

```rust
impl ChunkLoader {
    pub fn load_with_fallback(path: &str) -> Result<Vec<Entity>, ChunkLoadError> {
        // Try loading the chunk
        match Self::load_sync(path) {
            Ok(entities) => Ok(entities),
            Err(e) => {
                log::warn!("Failed to load chunk {}: {:?}, using fallback", path, e);

                // Return empty chunk as fallback
                Ok(Vec::new())
            }
        }
    }

    pub fn spawn_with_validation(
        world: &mut World,
        entities: &[EntityData],
        id_allocator: &mut impl FnMut() -> Entity,
    ) -> Vec<Entity> {
        let mut spawned = Vec::new();

        for entity_data in entities {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                let entity = id_allocator();
                world.spawn_with_id(entity, (
                    LocalTransform {
                        translation: entity_data.position,
                        rotation: entity_data.rotation,
                        scale: entity_data.scale,
                    },
                ));
                entity
            }));

            match result {
                Ok(entity) => spawned.push(entity),
                Err(_) => {
                    log::error!("Failed to spawn entity, skipping");
                    continue;
                }
            }
        }

        spawned
    }
}
```

### Degraded Mode Operation

```rust
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StreamingHealthStatus {
    Healthy,
    Degraded { failed_chunks: u32 },
    Critical { consecutive_failures: u32 },
}

impl StreamingManager {
    pub fn health_status(&self) -> StreamingHealthStatus {
        let failed = self.chunks.values()
            .filter(|c| c.state == ChunkState::Failed)
            .count() as u32;

        if failed == 0 {
            StreamingHealthStatus::Healthy
        } else if failed < 5 {
            StreamingHealthStatus::Degraded { failed_chunks: failed }
        } else {
            StreamingHealthStatus::Critical { consecutive_failures: failed }
        }
    }

    pub fn should_pause_streaming(&self) -> bool {
        matches!(self.health_status(), StreamingHealthStatus::Critical { .. })
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Chunks load based on distance
- [ ] Chunks unload when far away
- [ ] Dependencies respected
- [ ] Entity IDs stable across unload/reload
- [ ] No threading issues
- [ ] Loading is non-blocking
- [ ] Unloading is immediate
- [ ] Progress reporting works
- [ ] Editor shows chunk boundaries

### Hot-Swap Compliance

- [ ] SceneChunk derives Serialize/Deserialize
- [ ] ChunkId derives Serialize/Deserialize
- [ ] ChunkState derives Serialize/Deserialize
- [ ] StreamingConfig derives Serialize/Deserialize
- [ ] StreamingManager implements HotReloadable trait
- [ ] Entity IDs stable across hot-reload cycles
- [ ] Entity recycler state preserved across hot-reload
- [ ] Load/unload queues safely cleared on reload
- [ ] StreamingManager implements AssetDependent for chunk file changes
- [ ] Chunk state transitions handled correctly after reload
- [ ] Frame-boundary update queue prevents mid-frame state corruption
- [ ] Hot-swap tests pass in CI

## Dependencies

- **Phase 1: Scene Graph** - Entity hierarchy in chunks

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: High
**Primary Crates**: void_ecs, void_runtime
**Reviewer Notes**: Entity ID stability is critical for references
