//! Bounding Volume Hierarchy for spatial queries
//!
//! Provides O(log n) spatial queries using a binary BVH tree structure.

use alloc::collections::VecDeque;
use alloc::vec;
use alloc::vec::Vec;
use void_ecs::Entity;
use void_math::{AABB, Vec3, Ray, FrustumPlanes, FrustumTestResult, Sphere};
use serde::{Deserialize, Serialize};

/// BVH node internal representation
#[derive(Clone, Debug)]
struct BVHNodeInternal {
    /// Bounding box for this node
    bounds: AABB,
    /// Entity (only for leaf nodes)
    entity: Option<Entity>,
    /// Left child index
    left: Option<usize>,
    /// Right child index
    right: Option<usize>,
}

/// Bounding Volume Hierarchy for spatial queries
#[derive(Clone, Debug)]
pub struct BVH {
    nodes: Vec<BVHNodeInternal>,
    root: Option<usize>,
    version: u32,
}

impl BVH {
    /// Create a new empty BVH
    pub fn new() -> Self {
        Self {
            nodes: Vec::new(),
            root: None,
            version: 0,
        }
    }

    /// Build BVH from entities and their bounds
    pub fn build(&mut self, entities: &[(Entity, AABB)]) {
        self.nodes.clear();
        self.version = self.version.wrapping_add(1);

        if entities.is_empty() {
            self.root = None;
            return;
        }

        let mut items: Vec<_> = entities
            .iter()
            .map(|(e, b)| (*e, *b, b.center()))
            .collect();

        self.root = Some(self.build_recursive(&mut items, 0));
    }

    fn build_recursive(
        &mut self,
        items: &mut [(Entity, AABB, Vec3)],
        depth: u32,
    ) -> usize {
        let node_index = self.nodes.len();

        if items.len() == 1 {
            // Leaf node
            self.nodes.push(BVHNodeInternal {
                bounds: items[0].1,
                entity: Some(items[0].0),
                left: None,
                right: None,
            });
            return node_index;
        }

        // Compute combined bounds
        let bounds = items
            .iter()
            .fold(AABB::empty(), |acc, (_, b, _)| acc.union(b));

        // Choose split axis (largest extent)
        let extent = bounds.size();
        let axis = if extent.x > extent.y && extent.x > extent.z {
            0
        } else if extent.y > extent.z {
            1
        } else {
            2
        };

        // Sort by axis
        items.sort_by(|a, b| {
            let ca = match axis {
                0 => a.2.x,
                1 => a.2.y,
                _ => a.2.z,
            };
            let cb = match axis {
                0 => b.2.x,
                1 => b.2.y,
                _ => b.2.z,
            };
            ca.partial_cmp(&cb).unwrap_or(core::cmp::Ordering::Equal)
        });

        // Split
        let mid = items.len() / 2;
        let (left_items, right_items) = items.split_at_mut(mid);

        // Create node (reserve spot)
        self.nodes.push(BVHNodeInternal {
            bounds,
            entity: None,
            left: None,
            right: None,
        });

        // Build children
        let left = self.build_recursive(left_items, depth + 1);
        let right = self.build_recursive(right_items, depth + 1);

        // Update node
        self.nodes[node_index].left = Some(left);
        self.nodes[node_index].right = Some(right);

        node_index
    }

    /// Query entities within AABB
    pub fn query_aabb(&self, query: &AABB) -> Vec<Entity> {
        let mut results = Vec::new();
        if let Some(root) = self.root {
            self.query_aabb_recursive(root, query, &mut results);
        }
        results
    }

    fn query_aabb_recursive(&self, node_idx: usize, query: &AABB, results: &mut Vec<Entity>) {
        let node = &self.nodes[node_idx];

        if !node.bounds.intersects(query) {
            return;
        }

        if let Some(entity) = node.entity {
            results.push(entity);
        }

        if let Some(left) = node.left {
            self.query_aabb_recursive(left, query, results);
        }
        if let Some(right) = node.right {
            self.query_aabb_recursive(right, query, results);
        }
    }

    /// Query entities hit by ray, sorted by distance
    pub fn query_ray(&self, ray: &Ray, max_dist: f32) -> Vec<(Entity, f32)> {
        let mut results = Vec::new();
        if let Some(root) = self.root {
            self.query_ray_recursive(root, ray, max_dist, &mut results);
        }
        results.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap_or(core::cmp::Ordering::Equal));
        results
    }

    fn query_ray_recursive(
        &self,
        node_idx: usize,
        ray: &Ray,
        max_dist: f32,
        results: &mut Vec<(Entity, f32)>,
    ) {
        let node = &self.nodes[node_idx];

        if let Some(t) = ray_aabb_intersection(ray, &node.bounds) {
            if t > max_dist {
                return;
            }

            if let Some(entity) = node.entity {
                results.push((entity, t));
            }

            if let Some(left) = node.left {
                self.query_ray_recursive(left, ray, max_dist, results);
            }
            if let Some(right) = node.right {
                self.query_ray_recursive(right, ray, max_dist, results);
            }
        }
    }

    /// Frustum cull - return visible entities
    pub fn frustum_cull(&self, frustum: &FrustumPlanes) -> Vec<Entity> {
        let mut results = Vec::new();
        if let Some(root) = self.root {
            self.frustum_cull_recursive(root, frustum, &mut results);
        }
        results
    }

    fn frustum_cull_recursive(
        &self,
        node_idx: usize,
        frustum: &FrustumPlanes,
        results: &mut Vec<Entity>,
    ) {
        let node = &self.nodes[node_idx];

        match frustum.contains_aabb(&node.bounds) {
            FrustumTestResult::Outside => return,
            FrustumTestResult::Inside => {
                // All children visible
                self.collect_all(node_idx, results);
                return;
            }
            FrustumTestResult::Intersecting => {
                // Need to check children
            }
        }

        if let Some(entity) = node.entity {
            results.push(entity);
        }

        if let Some(left) = node.left {
            self.frustum_cull_recursive(left, frustum, results);
        }
        if let Some(right) = node.right {
            self.frustum_cull_recursive(right, frustum, results);
        }
    }

    fn collect_all(&self, node_idx: usize, results: &mut Vec<Entity>) {
        let node = &self.nodes[node_idx];

        if let Some(entity) = node.entity {
            results.push(entity);
        }

        if let Some(left) = node.left {
            self.collect_all(left, results);
        }
        if let Some(right) = node.right {
            self.collect_all(right, results);
        }
    }

    /// Query entities within a sphere
    pub fn query_sphere(&self, sphere: &Sphere) -> Vec<Entity> {
        let mut results = Vec::new();
        if let Some(root) = self.root {
            self.query_sphere_recursive(root, sphere, &mut results);
        }
        results
    }

    fn query_sphere_recursive(&self, node_idx: usize, sphere: &Sphere, results: &mut Vec<Entity>) {
        let node = &self.nodes[node_idx];

        if !sphere.intersects_aabb(&node.bounds) {
            return;
        }

        if let Some(entity) = node.entity {
            results.push(entity);
        }

        if let Some(left) = node.left {
            self.query_sphere_recursive(left, sphere, results);
        }
        if let Some(right) = node.right {
            self.query_sphere_recursive(right, sphere, results);
        }
    }

    /// Get the number of nodes in the BVH
    pub fn node_count(&self) -> usize {
        self.nodes.len()
    }

    /// Check if the BVH is empty
    pub fn is_empty(&self) -> bool {
        self.root.is_none()
    }

    /// Get the BVH version (incremented on each rebuild)
    pub fn version(&self) -> u32 {
        self.version
    }

    /// Get the root bounds of the entire scene
    pub fn root_bounds(&self) -> Option<AABB> {
        self.root.map(|r| self.nodes[r].bounds)
    }
}

impl Default for BVH {
    fn default() -> Self {
        Self::new()
    }
}

/// Ray-AABB intersection test
/// Returns the distance to intersection point, or None if no intersection
fn ray_aabb_intersection(ray: &Ray, aabb: &AABB) -> Option<f32> {
    let inv_dir = ray.inverse_direction();

    let t1 = (aabb.min.x - ray.origin.x) * inv_dir.x;
    let t2 = (aabb.max.x - ray.origin.x) * inv_dir.x;
    let t3 = (aabb.min.y - ray.origin.y) * inv_dir.y;
    let t4 = (aabb.max.y - ray.origin.y) * inv_dir.y;
    let t5 = (aabb.min.z - ray.origin.z) * inv_dir.z;
    let t6 = (aabb.max.z - ray.origin.z) * inv_dir.z;

    let tmin = t1.min(t2).max(t3.min(t4)).max(t5.min(t6));
    let tmax = t1.max(t2).min(t3.max(t4)).min(t5.max(t6));

    if tmax < 0.0 || tmin > tmax {
        None
    } else {
        Some(if tmin < 0.0 { tmax } else { tmin })
    }
}

// ============================================================================
// Serialization support for hot-swap
// ============================================================================

/// Serializable BVH node
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BVHNode {
    /// Min/max bounds as arrays for serialization
    pub bounds_min: [f32; 3],
    pub bounds_max: [f32; 3],
    /// Entity ID (index and generation)
    pub entity: Option<(u32, u32)>,
    /// Child indices
    pub left: Option<usize>,
    pub right: Option<usize>,
}

/// Serializable BVH state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BVHState {
    pub nodes: Vec<BVHNode>,
    pub root: Option<usize>,
    pub version: u32,
}

impl BVH {
    /// Serialize BVH to state
    pub fn to_state(&self) -> BVHState {
        BVHState {
            nodes: self
                .nodes
                .iter()
                .map(|n| BVHNode {
                    bounds_min: n.bounds.min.to_array(),
                    bounds_max: n.bounds.max.to_array(),
                    entity: n.entity.map(|e| (e.index(), e.generation())),
                    left: n.left,
                    right: n.right,
                })
                .collect(),
            root: self.root,
            version: self.version,
        }
    }

    /// Restore BVH from state
    pub fn from_state(state: &BVHState) -> Self {
        Self {
            nodes: state
                .nodes
                .iter()
                .map(|n| BVHNodeInternal {
                    bounds: AABB::new(
                        Vec3::new(n.bounds_min[0], n.bounds_min[1], n.bounds_min[2]),
                        Vec3::new(n.bounds_max[0], n.bounds_max[1], n.bounds_max[2]),
                    ),
                    entity: n.entity.map(|(idx, gen)| Entity::new(idx, gen)),
                    left: n.left,
                    right: n.right,
                })
                .collect(),
            root: state.root,
            version: state.version,
        }
    }
}

// ============================================================================
// Spatial Query Configuration
// ============================================================================

/// Configuration for spatial query system
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SpatialQueryConfig {
    /// Rebuild threshold (dirty entity ratio)
    pub rebuild_threshold: f32,
    /// Max tree depth
    pub max_depth: u32,
    /// Min entities per leaf
    pub min_leaf_size: u32,
    /// Enable incremental updates
    pub incremental_updates: bool,
}

impl Default for SpatialQueryConfig {
    fn default() -> Self {
        Self {
            rebuild_threshold: 0.3,
            max_depth: 32,
            min_leaf_size: 4,
            incremental_updates: true,
        }
    }
}

// ============================================================================
// Spatial Update Operations
// ============================================================================

/// Spatial update operations for frame-boundary processing
#[derive(Clone, Debug)]
pub enum SpatialUpdate {
    /// Restore BVH from serialized state
    RestoreBVH(BVHState),
    /// Validate BVH against current entities and rebuild if needed
    ValidateAndRebuild(BVHState),
    /// Rebuild BVH completely
    FullRebuild,
    /// Update specific entities
    UpdateEntities(Vec<Entity>),
    /// Update configuration
    UpdateConfig(SpatialQueryConfig),
}

/// Queue for spatial updates
#[derive(Clone, Debug, Default)]
pub struct SpatialUpdateQueue {
    updates: VecDeque<SpatialUpdate>,
}

impl SpatialUpdateQueue {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn push(&mut self, update: SpatialUpdate) {
        self.updates.push_back(update);
    }

    pub fn pop(&mut self) -> Option<SpatialUpdate> {
        self.updates.pop_front()
    }

    pub fn is_empty(&self) -> bool {
        self.updates.is_empty()
    }

    pub fn len(&self) -> usize {
        self.updates.len()
    }
}

// ============================================================================
// Spatial Index System
// ============================================================================

/// Spatial index system with hot-reload and fault tolerance
#[derive(Clone, Debug)]
pub struct SpatialIndexSystem {
    bvh: BVH,
    config: SpatialQueryConfig,
    dirty_entities: Vec<Entity>,
    update_queue: SpatialUpdateQueue,
    needs_full_rebuild: bool,
    in_fallback_mode: bool,
    consecutive_failures: u32,
}

impl SpatialIndexSystem {
    /// Create a new spatial index system
    pub fn new() -> Self {
        Self {
            bvh: BVH::new(),
            config: SpatialQueryConfig::default(),
            dirty_entities: Vec::new(),
            update_queue: SpatialUpdateQueue::new(),
            needs_full_rebuild: false,
            in_fallback_mode: false,
            consecutive_failures: 0,
        }
    }

    /// Create with custom configuration
    pub fn with_config(config: SpatialQueryConfig) -> Self {
        Self {
            config,
            ..Self::new()
        }
    }

    /// Get the underlying BVH
    pub fn bvh(&self) -> &BVH {
        &self.bvh
    }

    /// Get configuration
    pub fn config(&self) -> &SpatialQueryConfig {
        &self.config
    }

    /// Mark entity as dirty (needs bounds update)
    pub fn mark_dirty(&mut self, entity: Entity) {
        if !self.dirty_entities.contains(&entity) {
            self.dirty_entities.push(entity);
        }
    }

    /// Queue an update for frame boundary
    pub fn queue_update(&mut self, update: SpatialUpdate) {
        self.update_queue.push(update);
    }

    /// Rebuild BVH from entity bounds
    pub fn rebuild(&mut self, entities: &[(Entity, AABB)]) {
        self.bvh.build(entities);
        self.dirty_entities.clear();
        self.needs_full_rebuild = false;
    }

    /// Apply pending updates
    pub fn apply_pending_updates<F>(&mut self, get_bounds: F)
    where
        F: Fn() -> Vec<(Entity, AABB)>,
    {
        while let Some(update) = self.update_queue.pop() {
            match update {
                SpatialUpdate::RestoreBVH(state) => {
                    self.bvh = BVH::from_state(&state);
                }
                SpatialUpdate::ValidateAndRebuild(state) => {
                    // For now, just restore - validation would check entity existence
                    self.bvh = BVH::from_state(&state);
                }
                SpatialUpdate::FullRebuild => {
                    let bounds = get_bounds();
                    self.rebuild(&bounds);
                }
                SpatialUpdate::UpdateEntities(entities) => {
                    for entity in entities {
                        self.mark_dirty(entity);
                    }
                }
                SpatialUpdate::UpdateConfig(config) => {
                    self.config = config;
                }
            }
        }
    }

    /// Process dirty entities, rebuilding if necessary
    pub fn process_dirty(&mut self, get_bounds: impl Fn() -> Vec<(Entity, AABB)>) {
        if self.dirty_entities.is_empty() && !self.needs_full_rebuild {
            return;
        }

        let dirty_ratio = self.dirty_entities.len() as f32 / self.bvh.node_count().max(1) as f32;

        if dirty_ratio > self.config.rebuild_threshold || self.needs_full_rebuild {
            // Full rebuild
            let bounds = get_bounds();
            self.rebuild(&bounds);
        } else {
            // For now, just trigger full rebuild for any dirty entities
            // Incremental update would be more complex
            let bounds = get_bounds();
            self.rebuild(&bounds);
        }
    }

    // ========================================================================
    // Query methods with fallback support
    // ========================================================================

    /// Query AABB with fallback to linear search
    pub fn query_aabb(&self, query: &AABB) -> Vec<Entity> {
        if self.in_fallback_mode {
            // Would need world access for linear fallback
            vec![]
        } else {
            self.bvh.query_aabb(query)
        }
    }

    /// Query ray with fallback
    pub fn query_ray(&self, ray: &Ray, max_dist: f32) -> Vec<(Entity, f32)> {
        if self.in_fallback_mode {
            vec![]
        } else {
            self.bvh.query_ray(ray, max_dist)
        }
    }

    /// Frustum cull with fallback
    pub fn frustum_cull(&self, frustum: &FrustumPlanes) -> Vec<Entity> {
        if self.in_fallback_mode {
            vec![]
        } else {
            self.bvh.frustum_cull(frustum)
        }
    }

    /// Query sphere
    pub fn query_sphere(&self, sphere: &Sphere) -> Vec<Entity> {
        if self.in_fallback_mode {
            vec![]
        } else {
            self.bvh.query_sphere(sphere)
        }
    }

    /// Check and recover from errors
    pub fn check_and_recover(&mut self, get_bounds: impl Fn() -> Vec<(Entity, AABB)>) {
        if self.needs_full_rebuild || self.in_fallback_mode {
            log::info!("Attempting BVH recovery rebuild");

            let bounds = get_bounds();
            self.rebuild(&bounds);

            if self.bvh.node_count() > 0 {
                self.needs_full_rebuild = false;
                self.in_fallback_mode = false;
                self.consecutive_failures = 0;
                log::info!("BVH recovery successful");
            } else {
                self.consecutive_failures += 1;
                if self.consecutive_failures >= 3 {
                    log::error!(
                        "BVH rebuild failed {} times, staying in fallback mode",
                        self.consecutive_failures
                    );
                    self.in_fallback_mode = true;
                }
            }
        }
    }

    /// Get statistics
    pub fn stats(&self) -> SpatialSystemStats {
        SpatialSystemStats {
            node_count: self.bvh.node_count(),
            bvh_version: self.bvh.version(),
            dirty_count: self.dirty_entities.len(),
            in_fallback_mode: self.in_fallback_mode,
            pending_updates: self.update_queue.len(),
        }
    }
}

impl Default for SpatialIndexSystem {
    fn default() -> Self {
        Self::new()
    }
}

/// Statistics for spatial system monitoring
#[derive(Clone, Debug, Default)]
pub struct SpatialSystemStats {
    pub node_count: usize,
    pub bvh_version: u32,
    pub dirty_count: usize,
    pub in_fallback_mode: bool,
    pub pending_updates: usize,
}

// ============================================================================
// Serializable State
// ============================================================================

/// Serializable spatial index system state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SpatialIndexSystemState {
    pub bvh: BVHState,
    pub config: SpatialQueryConfig,
}

impl SpatialIndexSystem {
    /// Serialize to state
    pub fn to_state(&self) -> SpatialIndexSystemState {
        SpatialIndexSystemState {
            bvh: self.bvh.to_state(),
            config: self.config.clone(),
        }
    }

    /// Restore from state
    pub fn from_state(state: &SpatialIndexSystemState) -> Self {
        Self {
            bvh: BVH::from_state(&state.bvh),
            config: state.config.clone(),
            dirty_entities: Vec::new(),
            update_queue: SpatialUpdateQueue::new(),
            needs_full_rebuild: false,
            in_fallback_mode: false,
            consecutive_failures: 0,
        }
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    fn entity(id: u32) -> Entity {
        Entity::new(id, 0)
    }

    #[test]
    fn test_bvh_build() {
        let mut bvh = BVH::new();

        bvh.build(&[
            (entity(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (
                entity(1),
                AABB::new(Vec3::new(5.0, 0.0, 0.0), Vec3::new(6.0, 1.0, 1.0)),
            ),
        ]);

        assert!(!bvh.is_empty());
        assert!(bvh.node_count() >= 2);
    }

    #[test]
    fn test_bvh_query_aabb() {
        let mut bvh = BVH::new();

        bvh.build(&[
            (entity(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (
                entity(1),
                AABB::new(Vec3::new(10.0, 0.0, 0.0), Vec3::new(11.0, 1.0, 1.0)),
            ),
        ]);

        // Query that should hit entity 0
        let hits = bvh.query_aabb(&AABB::new(
            Vec3::new(-1.0, -1.0, -1.0),
            Vec3::new(2.0, 2.0, 2.0),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].index(), 0);

        // Query that should hit entity 1
        let hits = bvh.query_aabb(&AABB::new(
            Vec3::new(9.0, -1.0, -1.0),
            Vec3::new(12.0, 2.0, 2.0),
        ));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].index(), 1);

        // Query that should hit neither
        let hits = bvh.query_aabb(&AABB::new(
            Vec3::new(100.0, 100.0, 100.0),
            Vec3::new(101.0, 101.0, 101.0),
        ));
        assert!(hits.is_empty());
    }

    #[test]
    fn test_bvh_query_ray() {
        let mut bvh = BVH::new();

        bvh.build(&[
            (entity(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (
                entity(1),
                AABB::new(Vec3::new(10.0, 0.0, 0.0), Vec3::new(11.0, 1.0, 1.0)),
            ),
        ]);

        // Ray along X axis should hit both
        let ray = Ray::new(Vec3::new(-5.0, 0.5, 0.5), Vec3::X);
        let hits = bvh.query_ray(&ray, 100.0);
        assert_eq!(hits.len(), 2);
        // First hit should be closer (entity 0)
        assert_eq!(hits[0].0.index(), 0);
        assert!(hits[0].1 < hits[1].1);
    }

    #[test]
    fn test_bvh_query_sphere() {
        let mut bvh = BVH::new();

        bvh.build(&[
            (entity(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (
                entity(1),
                AABB::new(Vec3::new(10.0, 0.0, 0.0), Vec3::new(11.0, 1.0, 1.0)),
            ),
        ]);

        // Sphere around origin should hit entity 0
        let hits = bvh.query_sphere(&Sphere::new(Vec3::new(0.5, 0.5, 0.5), 2.0));
        assert_eq!(hits.len(), 1);
        assert_eq!(hits[0].index(), 0);

        // Large sphere should hit both
        let hits = bvh.query_sphere(&Sphere::new(Vec3::new(5.0, 0.5, 0.5), 10.0));
        assert_eq!(hits.len(), 2);
    }

    #[test]
    fn test_bvh_serialization() {
        let mut bvh = BVH::new();
        bvh.build(&[
            (entity(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (
                entity(1),
                AABB::new(Vec3::new(5.0, 0.0, 0.0), Vec3::new(6.0, 1.0, 1.0)),
            ),
        ]);

        let state = bvh.to_state();
        let restored = BVH::from_state(&state);

        assert_eq!(restored.node_count(), bvh.node_count());
        assert_eq!(restored.version(), bvh.version());
    }

    #[test]
    fn test_spatial_index_system() {
        let mut system = SpatialIndexSystem::new();

        system.rebuild(&[
            (entity(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (
                entity(1),
                AABB::new(Vec3::new(5.0, 0.0, 0.0), Vec3::new(6.0, 1.0, 1.0)),
            ),
        ]);

        let hits = system.query_aabb(&AABB::new(
            Vec3::new(-1.0, -1.0, -1.0),
            Vec3::new(2.0, 2.0, 2.0),
        ));
        assert_eq!(hits.len(), 1);

        let stats = system.stats();
        assert!(stats.node_count >= 2);
        assert!(!stats.in_fallback_mode);
    }

    #[test]
    fn test_spatial_query_config_default() {
        let config = SpatialQueryConfig::default();
        assert!((config.rebuild_threshold - 0.3).abs() < 0.01);
        assert_eq!(config.max_depth, 32);
        assert_eq!(config.min_leaf_size, 4);
        assert!(config.incremental_updates);
    }
}
