//! Scene Streaming System
//!
//! Provides dynamic loading and unloading of scene content:
//! - Scene chunks with spatial bounds
//! - Distance-based streaming
//! - Entity ID recycling for stability
//! - Dependency tracking between chunks
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::streaming::{StreamingManager, StreamingConfig, SceneChunk, ChunkId};
//! use void_math::AABB;
//!
//! let mut manager = StreamingManager::new(StreamingConfig::default());
//!
//! // Register chunks
//! let chunk = SceneChunk::new(ChunkId(1), "level1/area_a.toml", AABB::default());
//! manager.register_chunk(chunk);
//!
//! // Update based on camera position
//! let update = manager.update(camera_pos);
//!
//! // Process load/unload requests
//! for chunk_id in update.to_load {
//!     // Load chunk asynchronously
//! }
//! for chunk_id in update.to_unload {
//!     // Unload chunk
//! }
//! ```

mod chunk;
mod manager;

pub use chunk::*;
pub use manager::*;
