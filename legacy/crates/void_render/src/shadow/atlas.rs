//! Shadow Atlas Allocation
//!
//! Backend-agnostic shadow map allocation tracking. This module manages
//! which lights have shadow maps allocated and their positions in the atlas,
//! but does not create GPU resources directly.
//!
//! # Atlas Organization
//!
//! The shadow atlas is a 2D texture array where each layer contains one
//! shadow map. This simplifies allocation compared to packing multiple
//! shadow maps into a single texture.

use alloc::vec::Vec;
use alloc::collections::BTreeMap;
use serde::{Serialize, Deserialize};

/// Unique identifier for a light (entity ID or similar)
pub type LightId = u64;

/// Shadow atlas allocation manager
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowAtlas {
    /// Atlas layer size (width = height = size)
    pub size: u32,

    /// Number of layers in the atlas
    pub layer_count: u32,

    /// Active allocations (light ID -> allocation)
    allocations: BTreeMap<LightId, ShadowAllocation>,

    /// Free layer indices (stack, pop from end)
    free_layers: Vec<u32>,

    /// Current frame number
    frame: u64,

    /// Statistics
    stats: AtlasStats,
}

impl ShadowAtlas {
    /// Create a new shadow atlas manager
    pub fn new(size: u32, layer_count: u32) -> Self {
        // Stack: push 0, 1, 2, 3 -> pop returns 3, 2, 1, 0
        let free_layers = (0..layer_count).collect();

        Self {
            size,
            layer_count,
            allocations: BTreeMap::new(),
            free_layers,
            frame: 0,
            stats: AtlasStats::default(),
        }
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) {
        self.frame += 1;
        self.stats.allocations_this_frame = 0;
        self.stats.deallocations_this_frame = 0;
    }

    /// Allocate a shadow map for a light
    ///
    /// Returns the allocation if successful, or None if the atlas is full.
    /// If the light already has an allocation, it is reused.
    pub fn allocate(&mut self, light_id: LightId, resolution: u32) -> Option<ShadowAllocation> {
        // Check if already allocated
        if let Some(alloc) = self.allocations.get_mut(&light_id) {
            alloc.last_frame = self.frame;
            return Some(alloc.clone());
        }

        // Try to allocate a new layer
        let layer = self.free_layers.pop()?;

        let alloc = ShadowAllocation {
            layer,
            resolution: resolution.min(self.size),
            light_id,
            first_frame: self.frame,
            last_frame: self.frame,
            cascade_count: 1,
            cascade_layers: [layer, 0, 0, 0],
        };

        self.allocations.insert(light_id, alloc.clone());
        self.stats.allocations_this_frame += 1;
        self.stats.total_allocations += 1;

        Some(alloc)
    }

    /// Allocate multiple layers for cascaded shadow maps
    pub fn allocate_cascades(
        &mut self,
        light_id: LightId,
        resolution: u32,
        cascade_count: u32,
    ) -> Option<ShadowAllocation> {
        let cascade_count = cascade_count.clamp(1, 4);

        // Check if already allocated with enough cascades
        if let Some(alloc) = self.allocations.get_mut(&light_id) {
            if alloc.cascade_count >= cascade_count {
                alloc.last_frame = self.frame;
                return Some(alloc.clone());
            }
        }

        // Need more cascades or new allocation - deallocate first if exists
        if self.allocations.contains_key(&light_id) {
            self.deallocate(light_id);
        }

        // Check if we have enough free layers
        if self.free_layers.len() < cascade_count as usize {
            return None;
        }

        // Allocate layers
        let mut cascade_layers = [0u32; 4];
        for i in 0..cascade_count as usize {
            cascade_layers[i] = self.free_layers.pop()?;
        }

        let alloc = ShadowAllocation {
            layer: cascade_layers[0],
            resolution: resolution.min(self.size),
            light_id,
            first_frame: self.frame,
            last_frame: self.frame,
            cascade_count,
            cascade_layers,
        };

        self.allocations.insert(light_id, alloc.clone());
        self.stats.allocations_this_frame += 1;
        self.stats.total_allocations += 1;

        Some(alloc)
    }

    /// Deallocate a shadow map
    pub fn deallocate(&mut self, light_id: LightId) -> bool {
        if let Some(alloc) = self.allocations.remove(&light_id) {
            // Return all cascade layers to the free list
            for i in 0..alloc.cascade_count as usize {
                self.free_layers.push(alloc.cascade_layers[i]);
            }
            self.stats.deallocations_this_frame += 1;
            true
        } else {
            false
        }
    }

    /// Get allocation for a light
    pub fn get(&self, light_id: LightId) -> Option<&ShadowAllocation> {
        self.allocations.get(&light_id)
    }

    /// Check if a light has an allocation
    pub fn contains(&self, light_id: LightId) -> bool {
        self.allocations.contains_key(&light_id)
    }

    /// Clean up stale allocations
    ///
    /// Removes allocations that haven't been used for `max_age` frames.
    pub fn cleanup(&mut self, max_age: u64) {
        let stale: Vec<LightId> = self.allocations
            .iter()
            .filter(|(_, a)| self.frame - a.last_frame > max_age)
            .map(|(id, _)| *id)
            .collect();

        for light_id in stale {
            self.deallocate(light_id);
        }
    }

    /// Get number of allocated shadow maps
    pub fn allocated_count(&self) -> usize {
        self.allocations.len()
    }

    /// Get number of free layers
    pub fn free_count(&self) -> usize {
        self.free_layers.len()
    }

    /// Check if atlas is full
    pub fn is_full(&self) -> bool {
        self.free_layers.is_empty()
    }

    /// Get utilization ratio (0-1)
    pub fn utilization(&self) -> f32 {
        if self.layer_count == 0 {
            return 0.0;
        }
        1.0 - (self.free_layers.len() as f32 / self.layer_count as f32)
    }

    /// Get atlas statistics
    pub fn stats(&self) -> &AtlasStats {
        &self.stats
    }

    /// Get all allocations (for GPU buffer updates)
    pub fn allocations(&self) -> impl Iterator<Item = &ShadowAllocation> {
        self.allocations.values()
    }

    /// Get current frame
    pub fn frame(&self) -> u64 {
        self.frame
    }

    /// Reset atlas (clears all allocations)
    pub fn reset(&mut self) {
        self.allocations.clear();
        self.free_layers = (0..self.layer_count).rev().collect();
        self.stats = AtlasStats::default();
    }

    /// Resize atlas (clears all allocations)
    pub fn resize(&mut self, new_size: u32, new_layer_count: u32) {
        self.size = new_size;
        self.layer_count = new_layer_count;
        self.reset();
    }
}

impl Default for ShadowAtlas {
    fn default() -> Self {
        Self::new(2048, 16)
    }
}

/// Shadow map allocation
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowAllocation {
    /// Primary layer index in the atlas
    pub layer: u32,

    /// Resolution for this shadow map
    pub resolution: u32,

    /// Light entity that owns this allocation
    pub light_id: LightId,

    /// Frame when first allocated
    pub first_frame: u64,

    /// Frame when last used
    pub last_frame: u64,

    /// Number of cascades (1 for non-directional lights)
    pub cascade_count: u32,

    /// Layer indices for each cascade
    pub cascade_layers: [u32; 4],
}

impl ShadowAllocation {
    /// Get the layer for a specific cascade
    pub fn cascade_layer(&self, cascade: usize) -> u32 {
        self.cascade_layers[cascade.min(self.cascade_count as usize - 1)]
    }

    /// Check if this is a cascaded shadow map
    pub fn is_cascaded(&self) -> bool {
        self.cascade_count > 1
    }

    /// Get age in frames
    pub fn age(&self, current_frame: u64) -> u64 {
        current_frame.saturating_sub(self.first_frame)
    }

    /// Check if this allocation was used this frame
    pub fn used_this_frame(&self, current_frame: u64) -> bool {
        self.last_frame == current_frame
    }
}

/// Atlas statistics
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct AtlasStats {
    /// Allocations made this frame
    pub allocations_this_frame: u32,

    /// Deallocations made this frame
    pub deallocations_this_frame: u32,

    /// Total allocations ever made
    pub total_allocations: u64,
}

/// Shadow atlas state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowAtlasState {
    /// Atlas size
    pub size: u32,

    /// Layer count
    pub layer_count: u32,

    /// Current allocations
    pub allocations: BTreeMap<LightId, ShadowAllocation>,

    /// Current frame
    pub frame: u64,
}

impl ShadowAtlas {
    /// Serialize state for hot-reload
    pub fn save_state(&self) -> ShadowAtlasState {
        ShadowAtlasState {
            size: self.size,
            layer_count: self.layer_count,
            allocations: self.allocations.clone(),
            frame: self.frame,
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: ShadowAtlasState) {
        // Check if atlas needs resize
        if state.size != self.size || state.layer_count != self.layer_count {
            self.size = state.size;
            self.layer_count = state.layer_count;
        }

        // Restore allocations
        self.allocations = state.allocations;
        self.frame = state.frame;

        // Rebuild free list
        self.free_layers = (0..self.layer_count)
            .filter(|layer| {
                !self.allocations.values().any(|a| {
                    (0..a.cascade_count as usize).any(|i| a.cascade_layers[i] == *layer)
                })
            })
                        .collect();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_atlas_allocation() {
        let mut atlas = ShadowAtlas::new(2048, 4);

        let alloc = atlas.allocate(1, 2048).unwrap();
        assert_eq!(alloc.layer, 3); // Stack pops from end
        assert_eq!(alloc.resolution, 2048);

        let alloc = atlas.allocate(2, 2048).unwrap();
        assert_eq!(alloc.layer, 2);

        assert_eq!(atlas.allocated_count(), 2);
        assert_eq!(atlas.free_count(), 2);
    }

    #[test]
    fn test_atlas_reuse() {
        let mut atlas = ShadowAtlas::new(2048, 4);
        atlas.begin_frame();

        let alloc1 = atlas.allocate(1, 2048).unwrap();
        let layer = alloc1.layer;

        // Allocating same light should reuse
        atlas.begin_frame();
        let alloc2 = atlas.allocate(1, 2048).unwrap();
        assert_eq!(alloc2.layer, layer);
        assert_eq!(atlas.allocated_count(), 1);
    }

    #[test]
    fn test_atlas_full() {
        let mut atlas = ShadowAtlas::new(2048, 2);

        atlas.allocate(1, 2048).unwrap();
        atlas.allocate(2, 2048).unwrap();
        assert!(atlas.allocate(3, 2048).is_none());
        assert!(atlas.is_full());
    }

    #[test]
    fn test_atlas_deallocate() {
        let mut atlas = ShadowAtlas::new(2048, 2);

        atlas.allocate(1, 2048);
        atlas.allocate(2, 2048);
        assert!(atlas.is_full());

        atlas.deallocate(1);
        assert!(!atlas.is_full());
        assert_eq!(atlas.allocated_count(), 1);

        // Can allocate again
        atlas.allocate(3, 2048).unwrap();
    }

    #[test]
    fn test_atlas_cleanup() {
        let mut atlas = ShadowAtlas::new(2048, 4);

        atlas.begin_frame(); // frame 1
        atlas.allocate(1, 2048);
        atlas.allocate(2, 2048);

        atlas.begin_frame(); // frame 2
        atlas.allocate(1, 2048); // Update last_frame for light 1

        atlas.begin_frame(); // frame 3
        atlas.begin_frame(); // frame 4
        atlas.begin_frame(); // frame 5

        // Light 2 hasn't been used for 4 frames
        atlas.cleanup(3);

        assert!(atlas.contains(1));
        assert!(!atlas.contains(2));
    }

    #[test]
    fn test_cascade_allocation() {
        let mut atlas = ShadowAtlas::new(2048, 8);

        let alloc = atlas.allocate_cascades(1, 2048, 4).unwrap();
        assert_eq!(alloc.cascade_count, 4);
        assert_eq!(atlas.free_count(), 4);

        // Layers should be different
        assert_ne!(alloc.cascade_layers[0], alloc.cascade_layers[1]);
        assert_ne!(alloc.cascade_layers[1], alloc.cascade_layers[2]);
        assert_ne!(alloc.cascade_layers[2], alloc.cascade_layers[3]);
    }

    #[test]
    fn test_cascade_allocation_reuse() {
        let mut atlas = ShadowAtlas::new(2048, 8);
        atlas.begin_frame();

        let alloc1 = atlas.allocate_cascades(1, 2048, 4).unwrap();
        let layers1 = alloc1.cascade_layers;

        atlas.begin_frame();
        let alloc2 = atlas.allocate_cascades(1, 2048, 4).unwrap();

        assert_eq!(alloc2.cascade_layers, layers1);
        assert_eq!(atlas.free_count(), 4);
    }

    #[test]
    fn test_utilization() {
        let mut atlas = ShadowAtlas::new(2048, 4);

        assert_eq!(atlas.utilization(), 0.0);

        atlas.allocate(1, 2048);
        assert!((atlas.utilization() - 0.25).abs() < 0.01);

        atlas.allocate(2, 2048);
        assert!((atlas.utilization() - 0.5).abs() < 0.01);
    }

    #[test]
    fn test_state_save_restore() {
        let mut atlas = ShadowAtlas::new(2048, 8);
        atlas.begin_frame();
        atlas.allocate_cascades(1, 2048, 4);
        atlas.allocate(2, 1024);

        let state = atlas.save_state();

        // Create new atlas and restore
        let mut new_atlas = ShadowAtlas::new(1024, 4); // Different config
        new_atlas.restore_state(state);

        assert_eq!(new_atlas.size, 2048);
        assert_eq!(new_atlas.layer_count, 8);
        assert!(new_atlas.contains(1));
        assert!(new_atlas.contains(2));
        assert_eq!(new_atlas.get(1).unwrap().cascade_count, 4);
    }

    #[test]
    fn test_resolution_clamping() {
        let mut atlas = ShadowAtlas::new(2048, 4);

        // Request higher resolution than atlas size
        let alloc = atlas.allocate(1, 4096).unwrap();
        assert_eq!(alloc.resolution, 2048);
    }

    #[test]
    fn test_allocation_age() {
        let mut atlas = ShadowAtlas::new(2048, 4);

        atlas.begin_frame(); // frame 1
        atlas.allocate(1, 2048);

        atlas.begin_frame(); // frame 2
        atlas.begin_frame(); // frame 3

        let alloc = atlas.get(1).unwrap();
        assert_eq!(alloc.age(atlas.frame()), 2);
    }
}
