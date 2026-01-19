//! Scene Chunk Definition
//!
//! Defines streamable scene chunks with spatial bounds and dependencies.

use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};
use void_math::AABB;
use crate::Entity;

/// Unique identifier for a scene chunk
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ChunkId(pub u64);

impl ChunkId {
    /// Create a new chunk ID
    pub const fn new(id: u64) -> Self {
        Self(id)
    }

    /// Get the raw ID value
    pub const fn raw(&self) -> u64 {
        self.0
    }
}

impl From<u64> for ChunkId {
    fn from(id: u64) -> Self {
        Self(id)
    }
}

/// Current state of a scene chunk
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum ChunkState {
    /// Not loaded into memory
    #[default]
    Unloaded,
    /// Currently loading (async operation in progress)
    Loading,
    /// Fully loaded and active
    Loaded,
    /// Currently unloading
    Unloading,
    /// Load failed
    Failed,
}

impl ChunkState {
    /// Check if chunk is in a transient state (loading/unloading)
    pub fn is_transient(&self) -> bool {
        matches!(self, ChunkState::Loading | ChunkState::Unloading)
    }

    /// Check if chunk content is accessible
    pub fn is_accessible(&self) -> bool {
        *self == ChunkState::Loaded
    }
}

/// A chunk of streamable scene content
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SceneChunk {
    /// Unique chunk identifier
    pub id: ChunkId,

    /// Asset path for this chunk's scene file
    pub path: String,

    /// World-space bounds for distance calculations
    pub bounds: AABB,

    /// Loading priority (higher = load first when equidistant)
    pub priority: i32,

    /// Dependencies (chunks that must be loaded before this one)
    pub dependencies: Vec<ChunkId>,

    /// Current load state
    pub state: ChunkState,

    /// Entities owned by this chunk (populated when loaded)
    #[serde(skip)]
    pub entities: Vec<Entity>,

    /// Referenced asset paths (meshes, textures, etc.)
    pub assets: Vec<String>,

    /// Time when chunk was loaded (for min-loaded-time tracking)
    #[serde(skip)]
    pub loaded_at: Option<f32>,
}

impl SceneChunk {
    /// Create a new scene chunk
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
            loaded_at: None,
        }
    }

    /// Set the loading priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Add a dependency
    pub fn with_dependency(mut self, dep: ChunkId) -> Self {
        self.dependencies.push(dep);
        self
    }

    /// Add multiple dependencies
    pub fn with_dependencies(mut self, deps: impl IntoIterator<Item = ChunkId>) -> Self {
        self.dependencies.extend(deps);
        self
    }

    /// Add a referenced asset
    pub fn with_asset(mut self, asset: impl Into<String>) -> Self {
        self.assets.push(asset.into());
        self
    }

    /// Check if the chunk is ready for use
    pub fn is_ready(&self) -> bool {
        self.state == ChunkState::Loaded
    }

    /// Check if the chunk can be unloaded (not in transient state)
    pub fn can_unload(&self) -> bool {
        self.state == ChunkState::Loaded
    }

    /// Check if the chunk can be loaded (not already loaded or loading)
    pub fn can_load(&self) -> bool {
        self.state == ChunkState::Unloaded || self.state == ChunkState::Failed
    }

    /// Get the center of the chunk's bounds
    pub fn center(&self) -> void_math::Vec3 {
        self.bounds.center()
    }

    /// Get all asset paths (including chunk file itself)
    pub fn all_asset_paths(&self) -> Vec<&str> {
        let mut paths = vec![self.path.as_str()];
        paths.extend(self.assets.iter().map(|s| s.as_str()));
        paths
    }

    /// Mark chunk as loading
    pub fn start_loading(&mut self) {
        self.state = ChunkState::Loading;
    }

    /// Mark chunk as loaded with entities
    pub fn finish_loading(&mut self, entities: Vec<Entity>, current_time: f32) {
        self.state = ChunkState::Loaded;
        self.entities = entities;
        self.loaded_at = Some(current_time);
    }

    /// Mark chunk as failed
    pub fn mark_failed(&mut self) {
        self.state = ChunkState::Failed;
        self.entities.clear();
        self.loaded_at = None;
    }

    /// Mark chunk as unloading
    pub fn start_unloading(&mut self) {
        self.state = ChunkState::Unloading;
    }

    /// Mark chunk as fully unloaded
    pub fn finish_unloading(&mut self) -> Vec<Entity> {
        self.state = ChunkState::Unloaded;
        self.loaded_at = None;
        core::mem::take(&mut self.entities)
    }

    /// Check if chunk has been loaded long enough to unload
    pub fn can_unload_by_time(&self, current_time: f32, min_loaded_time: f32) -> bool {
        if let Some(loaded_at) = self.loaded_at {
            (current_time - loaded_at) >= min_loaded_time
        } else {
            true // Not tracking time, allow unload
        }
    }
}

impl Default for SceneChunk {
    fn default() -> Self {
        Self {
            id: ChunkId(0),
            path: String::new(),
            bounds: AABB::EMPTY,
            priority: 0,
            dependencies: Vec::new(),
            state: ChunkState::Unloaded,
            entities: Vec::new(),
            assets: Vec::new(),
            loaded_at: None,
        }
    }
}

/// Error during chunk operations
#[derive(Clone, Debug)]
pub enum ChunkError {
    /// Chunk not found
    NotFound(ChunkId),
    /// Invalid state for operation
    InvalidState { chunk: ChunkId, state: ChunkState },
    /// Dependency not loaded
    DependencyNotLoaded { chunk: ChunkId, dependency: ChunkId },
    /// Load error
    LoadError(String),
    /// Parse error
    ParseError(String),
}

impl core::fmt::Display for ChunkError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            ChunkError::NotFound(id) => write!(f, "Chunk {:?} not found", id),
            ChunkError::InvalidState { chunk, state } => {
                write!(f, "Chunk {:?} in invalid state {:?}", chunk, state)
            }
            ChunkError::DependencyNotLoaded { chunk, dependency } => {
                write!(
                    f,
                    "Chunk {:?} dependency {:?} not loaded",
                    chunk, dependency
                )
            }
            ChunkError::LoadError(msg) => write!(f, "Load error: {}", msg),
            ChunkError::ParseError(msg) => write!(f, "Parse error: {}", msg),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use void_math::Vec3;

    #[test]
    fn test_chunk_id() {
        let id = ChunkId::new(42);
        assert_eq!(id.raw(), 42);

        let id2: ChunkId = 100.into();
        assert_eq!(id2.raw(), 100);
    }

    #[test]
    fn test_chunk_state() {
        assert!(ChunkState::Loading.is_transient());
        assert!(ChunkState::Unloading.is_transient());
        assert!(!ChunkState::Loaded.is_transient());
        assert!(!ChunkState::Unloaded.is_transient());

        assert!(ChunkState::Loaded.is_accessible());
        assert!(!ChunkState::Loading.is_accessible());
    }

    #[test]
    fn test_scene_chunk_creation() {
        let bounds = AABB::new(Vec3::ZERO, Vec3::new(100.0, 50.0, 100.0));
        let chunk = SceneChunk::new(ChunkId(1), "level1/area_a.toml", bounds)
            .with_priority(10)
            .with_dependency(ChunkId(0))
            .with_asset("meshes/tree.glb");

        assert_eq!(chunk.id.raw(), 1);
        assert_eq!(chunk.path, "level1/area_a.toml");
        assert_eq!(chunk.priority, 10);
        assert_eq!(chunk.dependencies.len(), 1);
        assert_eq!(chunk.assets.len(), 1);
        assert_eq!(chunk.state, ChunkState::Unloaded);
    }

    #[test]
    fn test_chunk_state_transitions() {
        let mut chunk = SceneChunk::new(
            ChunkId(1),
            "test.toml",
            AABB::new(Vec3::ZERO, Vec3::ONE),
        );

        assert!(chunk.can_load());
        assert!(!chunk.is_ready());

        chunk.start_loading();
        assert_eq!(chunk.state, ChunkState::Loading);
        assert!(!chunk.can_load());

        let entities = vec![Entity::new(0, 0), Entity::new(1, 0)];
        chunk.finish_loading(entities.clone(), 0.0);
        assert!(chunk.is_ready());
        assert!(chunk.can_unload());
        assert_eq!(chunk.entities.len(), 2);

        chunk.start_unloading();
        assert_eq!(chunk.state, ChunkState::Unloading);

        let recycled = chunk.finish_unloading();
        assert_eq!(recycled.len(), 2);
        assert_eq!(chunk.state, ChunkState::Unloaded);
        assert!(chunk.entities.is_empty());
    }

    #[test]
    fn test_chunk_time_tracking() {
        let mut chunk = SceneChunk::new(
            ChunkId(1),
            "test.toml",
            AABB::new(Vec3::ZERO, Vec3::ONE),
        );

        chunk.finish_loading(vec![], 10.0);

        // Too soon to unload
        assert!(!chunk.can_unload_by_time(12.0, 5.0));

        // Enough time has passed
        assert!(chunk.can_unload_by_time(16.0, 5.0));
    }

    #[test]
    fn test_chunk_serialization() {
        let chunk = SceneChunk::new(
            ChunkId(42),
            "test.toml",
            AABB::new(Vec3::ZERO, Vec3::ONE),
        )
        .with_priority(5)
        .with_dependency(ChunkId(10));

        let serialized = bincode::serialize(&chunk).unwrap();
        let deserialized: SceneChunk = bincode::deserialize(&serialized).unwrap();

        assert_eq!(deserialized.id, ChunkId(42));
        assert_eq!(deserialized.path, "test.toml");
        assert_eq!(deserialized.priority, 5);
        assert_eq!(deserialized.dependencies, vec![ChunkId(10)]);
    }
}
