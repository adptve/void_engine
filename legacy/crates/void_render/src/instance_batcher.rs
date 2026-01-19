//! Instance Batching System
//!
//! Batches entities by mesh/material for efficient instanced rendering.
//! GPU buffer management is handled externally - this module focuses on
//! CPU-side batch organization.
//!
//! # Usage
//!
//! ```ignore
//! let mut batcher = InstanceBatcher::new(10000);
//!
//! // Begin new frame
//! batcher.begin_frame();
//!
//! // Add instances during extraction
//! for entity in renderable_entities {
//!     batcher.add_instance(
//!         entity.id(),
//!         mesh_id,
//!         material_id,
//!         model_matrix,
//!         color,
//!     );
//! }
//!
//! // Get batches for rendering
//! for (key, batch) in batcher.batches() {
//!     // Upload batch.as_bytes() to GPU
//!     // Draw instanced with batch.len() instances
//! }
//! ```

use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

use crate::instancing::{InstanceData, InstanceBatch, BatchKey};

/// Batches entities by mesh/material for instanced rendering
#[derive(Debug)]
pub struct InstanceBatcher {
    /// Batches keyed by (mesh_id, material_id, layer_mask)
    batches: BTreeMap<BatchKey, InstanceBatch>,

    /// Maximum instances per batch
    max_instances_per_batch: u32,

    /// Current frame number
    current_frame: u64,

    /// Statistics for debugging
    stats: BatcherStats,
}

/// Statistics about batching performance
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct BatcherStats {
    /// Total instances this frame
    pub total_instances: u32,
    /// Number of batches created
    pub batch_count: u32,
    /// Instances that couldn't fit (overflow)
    pub overflow_count: u32,
    /// Largest batch size
    pub max_batch_size: u32,
    /// Average batch size
    pub avg_batch_size: f32,
}

impl InstanceBatcher {
    /// Create a new batcher with maximum instances per batch
    pub fn new(max_instances_per_batch: u32) -> Self {
        Self {
            batches: BTreeMap::new(),
            max_instances_per_batch: max_instances_per_batch.min(65536),
            current_frame: 0,
            stats: BatcherStats::default(),
        }
    }

    /// Clear all batches for new frame
    pub fn begin_frame(&mut self) {
        self.current_frame += 1;
        self.stats = BatcherStats::default();

        for batch in self.batches.values_mut() {
            batch.clear();
        }
    }

    /// Add an instance to the appropriate batch
    ///
    /// Returns true if the instance was added, false if batch is full.
    pub fn add_instance(
        &mut self,
        entity_id: u64,
        mesh_id: u64,
        material_id: u64,
        model_matrix: [[f32; 4]; 4],
        color_tint: [f32; 4],
    ) -> bool {
        self.add_instance_with_layer(entity_id, mesh_id, material_id, 1, model_matrix, color_tint)
    }

    /// Add an instance with layer mask
    pub fn add_instance_with_layer(
        &mut self,
        entity_id: u64,
        mesh_id: u64,
        material_id: u64,
        layer_mask: u32,
        model_matrix: [[f32; 4]; 4],
        color_tint: [f32; 4],
    ) -> bool {
        let key = BatchKey::with_layer(mesh_id, material_id, layer_mask);

        let batch = self.batches.entry(key).or_insert_with(|| {
            InstanceBatch::with_capacity(256)
        });

        if batch.len() >= self.max_instances_per_batch as usize {
            self.stats.overflow_count += 1;
            return false;
        }

        let instance = InstanceData::new(model_matrix, color_tint);
        batch.push(entity_id, instance);
        self.stats.total_instances += 1;

        true
    }

    /// Add an instance with full custom data
    pub fn add_instance_full(
        &mut self,
        entity_id: u64,
        mesh_id: u64,
        material_id: u64,
        layer_mask: u32,
        model_matrix: [[f32; 4]; 4],
        color_tint: [f32; 4],
        custom_data: [f32; 4],
    ) -> bool {
        let key = BatchKey::with_layer(mesh_id, material_id, layer_mask);

        let batch = self.batches.entry(key).or_insert_with(|| {
            InstanceBatch::with_capacity(256)
        });

        if batch.len() >= self.max_instances_per_batch as usize {
            self.stats.overflow_count += 1;
            return false;
        }

        let instance = InstanceData::with_custom(model_matrix, color_tint, custom_data);
        batch.push(entity_id, instance);
        self.stats.total_instances += 1;

        true
    }

    /// Get all non-empty batches for rendering
    pub fn batches(&self) -> impl Iterator<Item = (&BatchKey, &InstanceBatch)> {
        self.batches.iter().filter(|(_, b)| !b.is_empty())
    }

    /// Get a specific batch by key
    pub fn get_batch(&self, key: &BatchKey) -> Option<&InstanceBatch> {
        self.batches.get(key).filter(|b| !b.is_empty())
    }

    /// Get entity ID from batch and instance index (for picking)
    pub fn get_entity(&self, key: &BatchKey, instance_index: u32) -> Option<u64> {
        self.batches.get(key)?.entity_at(instance_index as usize)
    }

    /// Calculate and return statistics
    pub fn stats(&mut self) -> &BatcherStats {
        let non_empty: Vec<_> = self.batches.values()
            .filter(|b| !b.is_empty())
            .collect();

        self.stats.batch_count = non_empty.len() as u32;

        if !non_empty.is_empty() {
            self.stats.max_batch_size = non_empty.iter()
                .map(|b| b.len() as u32)
                .max()
                .unwrap_or(0);

            let total: usize = non_empty.iter().map(|b| b.len()).sum();
            self.stats.avg_batch_size = total as f32 / non_empty.len() as f32;
        }

        &self.stats
    }

    /// Get current frame number
    pub fn current_frame(&self) -> u64 {
        self.current_frame
    }

    /// Get maximum instances per batch
    pub fn max_instances(&self) -> u32 {
        self.max_instances_per_batch
    }

    /// Get total number of batches (including empty)
    pub fn batch_count(&self) -> usize {
        self.batches.len()
    }

    /// Get total instances across all batches
    pub fn total_instances(&self) -> usize {
        self.batches.values()
            .filter(|b| !b.is_empty())
            .map(|b| b.len())
            .sum()
    }

    /// Clear all batches and reset
    pub fn clear(&mut self) {
        self.batches.clear();
        self.stats = BatcherStats::default();
    }

    /// Serialize state for hot-reload
    pub fn serialize_state(&self) -> SerializedBatcherState {
        SerializedBatcherState {
            max_instances_per_batch: self.max_instances_per_batch,
            current_frame: self.current_frame,
        }
    }

    /// Restore from serialized state
    pub fn restore_state(&mut self, state: SerializedBatcherState) {
        self.max_instances_per_batch = state.max_instances_per_batch;
        self.current_frame = state.current_frame;
        // Batches will be rebuilt on next extraction
        self.clear();
    }
}

impl Default for InstanceBatcher {
    fn default() -> Self {
        Self::new(10000)
    }
}

/// Serialized batcher state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SerializedBatcherState {
    /// Maximum instances per batch
    pub max_instances_per_batch: u32,
    /// Current frame number
    pub current_frame: u64,
}

/// Result of batching for a single mesh type
#[derive(Clone, Debug)]
pub struct BatchResult<'a> {
    /// The batch key
    pub key: &'a BatchKey,
    /// The batch data
    pub batch: &'a InstanceBatch,
    /// Instance count
    pub instance_count: u32,
}

impl InstanceBatcher {
    /// Get batch results for rendering (with instance counts)
    pub fn batch_results(&self) -> impl Iterator<Item = BatchResult<'_>> {
        self.batches.iter()
            .filter(|(_, b)| !b.is_empty())
            .map(|(key, batch)| BatchResult {
                key,
                batch,
                instance_count: batch.len() as u32,
            })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_batcher_basic() {
        let mut batcher = InstanceBatcher::new(1000);
        batcher.begin_frame();

        let mesh_id = 1;
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

        // Add 100 instances of same mesh
        for i in 0..100 {
            let added = batcher.add_instance(i, mesh_id, 0, matrix, [1.0; 4]);
            assert!(added);
        }

        assert_eq!(batcher.total_instances(), 100);

        let batches: Vec<_> = batcher.batches().collect();
        assert_eq!(batches.len(), 1);
        assert_eq!(batches[0].1.len(), 100);
    }

    #[test]
    fn test_batcher_material_separation() {
        let mut batcher = InstanceBatcher::new(1000);
        batcher.begin_frame();

        let mesh_id = 1;
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

        // Add instances with different materials
        batcher.add_instance(1, mesh_id, 0, matrix, [1.0; 4]);
        batcher.add_instance(2, mesh_id, 1, matrix, [1.0; 4]);
        batcher.add_instance(3, mesh_id, 0, matrix, [1.0; 4]);

        let batches: Vec<_> = batcher.batches().collect();
        assert_eq!(batches.len(), 2); // Two different materials

        assert_eq!(batcher.total_instances(), 3);
    }

    #[test]
    fn test_batcher_overflow() {
        let mut batcher = InstanceBatcher::new(10);
        batcher.begin_frame();

        let mesh_id = 1;
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

        // Add more than max
        for i in 0..20 {
            batcher.add_instance(i, mesh_id, 0, matrix, [1.0; 4]);
        }

        // Should cap at 10
        assert_eq!(batcher.total_instances(), 10);

        let stats = batcher.stats();
        assert_eq!(stats.overflow_count, 10);
    }

    #[test]
    fn test_batcher_entity_picking() {
        let mut batcher = InstanceBatcher::new(1000);
        batcher.begin_frame();

        let mesh_id = 1;
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

        batcher.add_instance(100, mesh_id, 0, matrix, [1.0; 4]);
        batcher.add_instance(200, mesh_id, 0, matrix, [1.0; 4]);
        batcher.add_instance(300, mesh_id, 0, matrix, [1.0; 4]);

        let key = BatchKey::new(mesh_id, 0);
        assert_eq!(batcher.get_entity(&key, 0), Some(100));
        assert_eq!(batcher.get_entity(&key, 1), Some(200));
        assert_eq!(batcher.get_entity(&key, 2), Some(300));
        assert_eq!(batcher.get_entity(&key, 3), None);
    }

    #[test]
    fn test_batcher_layer_separation() {
        let mut batcher = InstanceBatcher::new(1000);
        batcher.begin_frame();

        let mesh_id = 1;
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

        // Add instances on different layers
        batcher.add_instance_with_layer(1, mesh_id, 0, 1, matrix, [1.0; 4]);
        batcher.add_instance_with_layer(2, mesh_id, 0, 2, matrix, [1.0; 4]);
        batcher.add_instance_with_layer(3, mesh_id, 0, 1, matrix, [1.0; 4]);

        let batches: Vec<_> = batcher.batches().collect();
        assert_eq!(batches.len(), 2); // Two different layers
    }

    #[test]
    fn test_batcher_frame_clear() {
        let mut batcher = InstanceBatcher::new(1000);

        let mesh_id = 1;
        let matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

        // Frame 1
        batcher.begin_frame();
        batcher.add_instance(1, mesh_id, 0, matrix, [1.0; 4]);
        assert_eq!(batcher.total_instances(), 1);

        // Frame 2 - should be cleared
        batcher.begin_frame();
        assert_eq!(batcher.total_instances(), 0);
    }

    #[test]
    fn test_batcher_serialization() {
        let mut batcher = InstanceBatcher::new(5000);
        batcher.begin_frame();
        batcher.begin_frame();
        batcher.begin_frame();

        let state = batcher.serialize_state();
        assert_eq!(state.max_instances_per_batch, 5000);
        assert_eq!(state.current_frame, 3);

        let mut new_batcher = InstanceBatcher::new(1000);
        new_batcher.restore_state(state);

        assert_eq!(new_batcher.max_instances(), 5000);
        assert_eq!(new_batcher.current_frame(), 3);
    }
}
