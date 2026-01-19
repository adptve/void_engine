# Phase 14: Spatial Queries & Bounds

## Status: Not Started

## User Story

> As an engine/system author, I want spatial data for culling and interaction.

## Requirements Checklist

- [ ] Compute bounding volumes per entity
- [ ] Support: Bounding box, Bounding sphere
- [ ] Expose bounds for: Frustum culling, Raycasting
- [ ] Update bounds when transforms change

## Implementation Specification

### 1. Bounding Volume Types

```rust
// crates/void_math/src/bounds.rs (ENHANCED)

/// Axis-Aligned Bounding Box
#[derive(Clone, Copy, Debug, Default)]
pub struct AABB {
    pub min: Vec3,
    pub max: Vec3,
}

impl AABB {
    pub fn new(min: Vec3, max: Vec3) -> Self {
        Self { min, max }
    }

    pub fn from_points(points: &[[f32; 3]]) -> Self {
        if points.is_empty() {
            return Self::default();
        }

        let mut min = Vec3::from(points[0]);
        let mut max = Vec3::from(points[0]);

        for p in points.iter().skip(1) {
            let p = Vec3::from(*p);
            min = min.min(p);
            max = max.max(p);
        }

        Self { min, max }
    }

    pub fn center(&self) -> Vec3 {
        (self.min + self.max) * 0.5
    }

    pub fn extents(&self) -> Vec3 {
        (self.max - self.min) * 0.5
    }

    pub fn size(&self) -> Vec3 {
        self.max - self.min
    }

    pub fn contains(&self, point: Vec3) -> bool {
        point.x >= self.min.x && point.x <= self.max.x &&
        point.y >= self.min.y && point.y <= self.max.y &&
        point.z >= self.min.z && point.z <= self.max.z
    }

    pub fn intersects(&self, other: &AABB) -> bool {
        self.min.x <= other.max.x && self.max.x >= other.min.x &&
        self.min.y <= other.max.y && self.max.y >= other.min.y &&
        self.min.z <= other.max.z && self.max.z >= other.min.z
    }

    pub fn union(&self, other: &AABB) -> AABB {
        AABB {
            min: self.min.min(other.min),
            max: self.max.max(other.max),
        }
    }

    pub fn expand(&self, amount: f32) -> AABB {
        AABB {
            min: self.min - Vec3::splat(amount),
            max: self.max + Vec3::splat(amount),
        }
    }

    pub fn transform(&self, matrix: &Mat4) -> AABB {
        // Transform all 8 corners and compute new AABB
        let corners = [
            Vec3::new(self.min.x, self.min.y, self.min.z),
            Vec3::new(self.max.x, self.min.y, self.min.z),
            Vec3::new(self.min.x, self.max.y, self.min.z),
            Vec3::new(self.max.x, self.max.y, self.min.z),
            Vec3::new(self.min.x, self.min.y, self.max.z),
            Vec3::new(self.max.x, self.min.y, self.max.z),
            Vec3::new(self.min.x, self.max.y, self.max.z),
            Vec3::new(self.max.x, self.max.y, self.max.z),
        ];

        let transformed: Vec<[f32; 3]> = corners.iter()
            .map(|c| matrix.transform_point(*c).into())
            .collect();

        AABB::from_points(&transformed)
    }

    pub fn to_sphere(&self) -> Sphere {
        Sphere {
            center: self.center(),
            radius: self.extents().length(),
        }
    }

    pub fn empty() -> Self {
        Self {
            min: Vec3::splat(f32::INFINITY),
            max: Vec3::splat(f32::NEG_INFINITY),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.min.x > self.max.x || self.min.y > self.max.y || self.min.z > self.max.z
    }
}

/// Bounding Sphere
#[derive(Clone, Copy, Debug, Default)]
pub struct Sphere {
    pub center: Vec3,
    pub radius: f32,
}

impl Sphere {
    pub fn new(center: Vec3, radius: f32) -> Self {
        Self { center, radius }
    }

    pub fn from_points(points: &[[f32; 3]]) -> Self {
        if points.is_empty() {
            return Self::default();
        }

        // Ritter's bounding sphere algorithm
        let mut center = Vec3::from(points[0]);
        let mut radius = 0.0f32;

        // Find point farthest from initial
        let mut max_dist = 0.0;
        let mut farthest = points[0];
        for p in points {
            let d = (Vec3::from(*p) - center).length_squared();
            if d > max_dist {
                max_dist = d;
                farthest = *p;
            }
        }

        // Find point farthest from that
        center = Vec3::from(farthest);
        max_dist = 0.0;
        for p in points {
            let d = (Vec3::from(*p) - center).length_squared();
            if d > max_dist {
                max_dist = d;
                farthest = *p;
            }
        }

        // Initial sphere
        center = (center + Vec3::from(farthest)) * 0.5;
        radius = (center - Vec3::from(farthest)).length();

        // Expand to include all points
        for p in points {
            let p = Vec3::from(*p);
            let dist = (p - center).length();
            if dist > radius {
                let new_radius = (radius + dist) * 0.5;
                center = center + (p - center).normalize() * (new_radius - radius);
                radius = new_radius;
            }
        }

        Self { center, radius }
    }

    pub fn contains(&self, point: Vec3) -> bool {
        (point - self.center).length_squared() <= self.radius * self.radius
    }

    pub fn intersects(&self, other: &Sphere) -> bool {
        let combined_radius = self.radius + other.radius;
        (self.center - other.center).length_squared() <= combined_radius * combined_radius
    }

    pub fn transform(&self, matrix: &Mat4) -> Sphere {
        let center = matrix.transform_point(self.center);
        // Scale radius by maximum scale factor
        let scale = matrix.transform_vector(Vec3::X).length()
            .max(matrix.transform_vector(Vec3::Y).length())
            .max(matrix.transform_vector(Vec3::Z).length());

        Sphere { center, radius: self.radius * scale }
    }

    pub fn to_aabb(&self) -> AABB {
        AABB {
            min: self.center - Vec3::splat(self.radius),
            max: self.center + Vec3::splat(self.radius),
        }
    }
}
```

### 2. Frustum

```rust
// crates/void_math/src/frustum.rs (NEW FILE)

/// View frustum for culling
#[derive(Clone, Debug)]
pub struct Frustum {
    /// Six frustum planes (left, right, bottom, top, near, far)
    pub planes: [Plane; 6],
}

/// Plane in 3D space (ax + by + cz + d = 0)
#[derive(Clone, Copy, Debug)]
pub struct Plane {
    pub normal: Vec3,
    pub distance: f32,
}

impl Plane {
    pub fn new(normal: Vec3, distance: f32) -> Self {
        Self { normal: normal.normalize(), distance }
    }

    pub fn from_point_normal(point: Vec3, normal: Vec3) -> Self {
        let normal = normal.normalize();
        Self {
            normal,
            distance: -normal.dot(point),
        }
    }

    pub fn distance_to_point(&self, point: Vec3) -> f32 {
        self.normal.dot(point) + self.distance
    }
}

impl Frustum {
    /// Extract frustum from view-projection matrix
    pub fn from_view_projection(vp: &Mat4) -> Self {
        let m = vp.to_cols_array_2d();

        // Left plane
        let left = Plane::new(
            Vec3::new(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]),
            m[3][3] + m[3][0],
        );

        // Right plane
        let right = Plane::new(
            Vec3::new(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]),
            m[3][3] - m[3][0],
        );

        // Bottom plane
        let bottom = Plane::new(
            Vec3::new(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]),
            m[3][3] + m[3][1],
        );

        // Top plane
        let top = Plane::new(
            Vec3::new(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]),
            m[3][3] - m[3][1],
        );

        // Near plane
        let near = Plane::new(
            Vec3::new(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]),
            m[3][3] + m[3][2],
        );

        // Far plane
        let far = Plane::new(
            Vec3::new(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]),
            m[3][3] - m[3][2],
        );

        Self {
            planes: [left, right, bottom, top, near, far],
        }
    }

    /// Test if AABB is inside or intersecting frustum
    pub fn contains_aabb(&self, aabb: &AABB) -> FrustumTestResult {
        let mut result = FrustumTestResult::Inside;

        for plane in &self.planes {
            // Get the corner most aligned with plane normal
            let p = Vec3::new(
                if plane.normal.x >= 0.0 { aabb.max.x } else { aabb.min.x },
                if plane.normal.y >= 0.0 { aabb.max.y } else { aabb.min.y },
                if plane.normal.z >= 0.0 { aabb.max.z } else { aabb.min.z },
            );

            // Get the corner least aligned
            let n = Vec3::new(
                if plane.normal.x >= 0.0 { aabb.min.x } else { aabb.max.x },
                if plane.normal.y >= 0.0 { aabb.min.y } else { aabb.max.y },
                if plane.normal.z >= 0.0 { aabb.min.z } else { aabb.max.z },
            );

            if plane.distance_to_point(p) < 0.0 {
                return FrustumTestResult::Outside;
            }

            if plane.distance_to_point(n) < 0.0 {
                result = FrustumTestResult::Intersecting;
            }
        }

        result
    }

    /// Test if sphere is inside or intersecting frustum
    pub fn contains_sphere(&self, sphere: &Sphere) -> FrustumTestResult {
        let mut result = FrustumTestResult::Inside;

        for plane in &self.planes {
            let dist = plane.distance_to_point(sphere.center);

            if dist < -sphere.radius {
                return FrustumTestResult::Outside;
            }

            if dist < sphere.radius {
                result = FrustumTestResult::Intersecting;
            }
        }

        result
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FrustumTestResult {
    Inside,
    Outside,
    Intersecting,
}
```

### 3. Spatial Acceleration (BVH)

```rust
// crates/void_render/src/spatial/bvh.rs (NEW FILE)

use void_math::{AABB, Vec3};
use void_ecs::Entity;

/// Bounding Volume Hierarchy for spatial queries
pub struct BVH {
    nodes: Vec<BVHNode>,
    root: Option<usize>,
}

struct BVHNode {
    bounds: AABB,
    entity: Option<Entity>,  // Leaf nodes have entity
    left: Option<usize>,
    right: Option<usize>,
}

impl BVH {
    pub fn new() -> Self {
        Self {
            nodes: Vec::new(),
            root: None,
        }
    }

    /// Build BVH from entities and their bounds
    pub fn build(&mut self, entities: &[(Entity, AABB)]) {
        self.nodes.clear();

        if entities.is_empty() {
            self.root = None;
            return;
        }

        let mut items: Vec<_> = entities.iter()
            .map(|(e, b)| (*e, *b, b.center()))
            .collect();

        self.root = Some(self.build_recursive(&mut items, 0));
    }

    fn build_recursive(&mut self, items: &mut [(Entity, AABB, Vec3)], depth: u32) -> usize {
        let node_index = self.nodes.len();

        if items.len() == 1 {
            // Leaf node
            self.nodes.push(BVHNode {
                bounds: items[0].1,
                entity: Some(items[0].0),
                left: None,
                right: None,
            });
            return node_index;
        }

        // Compute combined bounds
        let bounds = items.iter()
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
            ca.partial_cmp(&cb).unwrap()
        });

        // Split
        let mid = items.len() / 2;
        let (left_items, right_items) = items.split_at_mut(mid);

        // Create node (reserve spot)
        self.nodes.push(BVHNode {
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

    /// Query entities hit by ray
    pub fn query_ray(&self, ray: &Ray, max_dist: f32) -> Vec<(Entity, f32)> {
        let mut results = Vec::new();
        if let Some(root) = self.root {
            self.query_ray_recursive(root, ray, max_dist, &mut results);
        }
        results.sort_by(|a, b| a.1.partial_cmp(&b.1).unwrap());
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

        if let Some(t) = ray_aabb(ray, &node.bounds) {
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
    pub fn frustum_cull(&self, frustum: &Frustum) -> Vec<Entity> {
        let mut results = Vec::new();
        if let Some(root) = self.root {
            self.frustum_cull_recursive(root, frustum, &mut results);
        }
        results
    }

    fn frustum_cull_recursive(
        &self,
        node_idx: usize,
        frustum: &Frustum,
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
}
```

### 4. Bounds Update System

```rust
// crates/void_ecs/src/systems/bounds_system.rs (NEW FILE)

use void_ecs::{World, Entity};
use void_math::AABB;

/// Updates bounding volumes when transforms change
pub struct BoundsUpdateSystem;

impl BoundsUpdateSystem {
    pub fn update(world: &mut World, bvh: &mut BVH) {
        let mut bounds_data = Vec::new();

        // Collect all entities with bounds
        for (entity, (mesh_renderer, transform, bounds)) in
            world.query::<(&MeshRenderer, &GlobalTransform, Option<&mut BoundingBox>)>()
        {
            // Get local bounds from mesh
            let local_bounds = Self::get_mesh_bounds(mesh_renderer);

            // Transform to world space
            let world_bounds = local_bounds.transform(&transform.matrix);

            // Update component
            if let Some(mut b) = bounds {
                b.min = world_bounds.min.into();
                b.max = world_bounds.max.into();
            }

            bounds_data.push((entity, world_bounds));
        }

        // Rebuild BVH
        bvh.build(&bounds_data);
    }

    fn get_mesh_bounds(mesh_renderer: &MeshRenderer) -> AABB {
        // Get from primitive or mesh asset
        match &mesh_renderer.primitive {
            Some(PrimitiveType::Cube) => AABB::new(
                Vec3::new(-0.5, -0.5, -0.5),
                Vec3::new(0.5, 0.5, 0.5),
            ),
            Some(PrimitiveType::Sphere { .. }) => AABB::new(
                Vec3::new(-1.0, -1.0, -1.0),
                Vec3::new(1.0, 1.0, 1.0),
            ),
            // ... other primitives
            _ => AABB::default(),
        }
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_math/src/bounds.rs` | MODIFY | Enhanced AABB and Sphere |
| `void_math/src/frustum.rs` | CREATE | Frustum culling |
| `void_render/src/spatial/bvh.rs` | CREATE | BVH acceleration |
| `void_ecs/src/systems/bounds_system.rs` | CREATE | Bounds updates |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_aabb_contains() {
    let aabb = AABB::new(Vec3::ZERO, Vec3::ONE);
    assert!(aabb.contains(Vec3::new(0.5, 0.5, 0.5)));
    assert!(!aabb.contains(Vec3::new(2.0, 0.5, 0.5)));
}

#[test]
fn test_frustum_cull() {
    let frustum = Frustum::from_view_projection(&view_proj);
    let inside = AABB::new(Vec3::ZERO, Vec3::ONE);
    let outside = AABB::new(Vec3::new(1000.0, 0.0, 0.0), Vec3::new(1001.0, 1.0, 1.0));

    assert_ne!(frustum.contains_aabb(&inside), FrustumTestResult::Outside);
    assert_eq!(frustum.contains_aabb(&outside), FrustumTestResult::Outside);
}

#[test]
fn test_bvh_ray_query() {
    let mut bvh = BVH::new();
    bvh.build(&[
        (Entity::from_raw(0), AABB::new(Vec3::ZERO, Vec3::ONE)),
        (Entity::from_raw(1), AABB::new(Vec3::new(10.0, 0.0, 0.0), Vec3::new(11.0, 1.0, 1.0))),
    ]);

    let ray = Ray::new(Vec3::new(-1.0, 0.5, 0.5), Vec3::X);
    let hits = bvh.query_ray(&ray, 100.0);

    assert_eq!(hits.len(), 2);
    assert_eq!(hits[0].0, Entity::from_raw(0));  // Closer one first
}
```

## Hot-Swap Support

### Serialization

BVH and spatial query structures must be serializable for scene persistence and hot-swap:

```rust
use serde::{Deserialize, Serialize};

/// Serializable AABB
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct AABB {
    pub min: Vec3,
    pub max: Vec3,
}

/// Serializable Sphere
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub struct Sphere {
    pub center: Vec3,
    pub radius: f32,
}

/// Serializable BVH node
#[derive(Clone, Debug, Serialize, Deserialize)]
struct BVHNode {
    bounds: AABB,
    entity: Option<u64>,  // Entity ID for serialization
    left: Option<usize>,
    right: Option<usize>,
}

/// Serializable BVH state
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BVHState {
    pub nodes: Vec<BVHNode>,
    pub root: Option<usize>,
    pub version: u32,
}

/// Serializable spatial query config
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
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, ReloadContext};

impl HotReloadable for BVH {
    fn type_name(&self) -> &'static str {
        "BVH"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = BVHState {
            nodes: self.nodes.iter().map(|n| BVHNode {
                bounds: n.bounds,
                entity: n.entity.map(|e| e.id()),
                left: n.left,
                right: n.right,
            }).collect(),
            root: self.root,
            version: self.version,
        };
        bincode::serialize(&state).map_err(|e| HotReloadError::Serialize(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> Result<(), HotReloadError> {
        let state: BVHState = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialize(e.to_string()))?;

        // Queue rebuild for frame boundary - entities may have changed
        ctx.queue_update(SpatialUpdate::RestoreBVH(state));
        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

/// Spatial index system with hot-reload support
pub struct SpatialIndexSystem {
    bvh: BVH,
    config: SpatialQueryConfig,
    dirty_entities: HashSet<Entity>,
    update_queue: VecDeque<SpatialUpdate>,
}

impl HotReloadable for SpatialIndexSystem {
    fn type_name(&self) -> &'static str {
        "SpatialIndexSystem"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        #[derive(Serialize)]
        struct State {
            bvh: BVHState,
            config: SpatialQueryConfig,
        }

        let state = State {
            bvh: BVHState {
                nodes: self.bvh.nodes.iter().map(|n| BVHNode {
                    bounds: n.bounds,
                    entity: n.entity.map(|e| e.id()),
                    left: n.left,
                    right: n.right,
                }).collect(),
                root: self.bvh.root,
                version: self.bvh.version,
            },
            config: self.config.clone(),
        };

        bincode::serialize(&state).map_err(|e| HotReloadError::Serialize(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> Result<(), HotReloadError> {
        #[derive(Deserialize)]
        struct State {
            bvh: BVHState,
            config: SpatialQueryConfig,
        }

        let state: State = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialize(e.to_string()))?;

        self.config = state.config;

        // BVH may need full rebuild if entities changed during reload
        // Queue validation and potential rebuild
        ctx.queue_update(SpatialUpdate::ValidateAndRebuild(state.bvh));

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}
```

### Frame-Boundary Updates

Spatial index updates are applied at frame boundaries to ensure consistency:

```rust
/// Spatial update operations
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

impl SpatialIndexSystem {
    /// Apply pending updates at frame boundary
    pub fn apply_pending_updates(&mut self, world: &World) {
        while let Some(update) = self.update_queue.pop_front() {
            match update {
                SpatialUpdate::RestoreBVH(state) => {
                    self.restore_bvh(state, world);
                }
                SpatialUpdate::ValidateAndRebuild(state) => {
                    if self.validate_bvh_state(&state, world) {
                        self.restore_bvh(state, world);
                    } else {
                        log::info!("BVH state invalid after hot-reload, rebuilding");
                        self.rebuild_full(world);
                    }
                }
                SpatialUpdate::FullRebuild => {
                    self.rebuild_full(world);
                }
                SpatialUpdate::UpdateEntities(entities) => {
                    for entity in entities {
                        self.dirty_entities.insert(entity);
                    }
                }
                SpatialUpdate::UpdateConfig(config) => {
                    self.config = config;
                    if !self.config.incremental_updates {
                        self.rebuild_full(world);
                    }
                }
            }
        }

        // Process dirty entities
        self.process_dirty_entities(world);
    }

    /// Validate BVH state against current world
    fn validate_bvh_state(&self, state: &BVHState, world: &World) -> bool {
        // Check that all entities in BVH still exist
        for node in &state.nodes {
            if let Some(entity_id) = node.entity {
                let entity = Entity::from_raw(entity_id);
                if !world.contains(entity) {
                    return false;
                }
            }
        }

        // Check version matches expected
        state.version == self.bvh.version
    }

    /// Restore BVH from serialized state
    fn restore_bvh(&mut self, state: BVHState, world: &World) {
        self.bvh.nodes = state.nodes.iter().map(|n| BVHNodeInternal {
            bounds: n.bounds,
            entity: n.entity.map(Entity::from_raw),
            left: n.left,
            right: n.right,
        }).collect();
        self.bvh.root = state.root;
        self.bvh.version = state.version;
        self.dirty_entities.clear();
    }

    /// Queue an update for frame boundary
    pub fn queue_update(&mut self, update: SpatialUpdate) {
        self.update_queue.push_back(update);
    }

    /// Process incrementally dirty entities
    fn process_dirty_entities(&mut self, world: &World) {
        if self.dirty_entities.is_empty() {
            return;
        }

        let dirty_ratio = self.dirty_entities.len() as f32 / self.bvh.nodes.len().max(1) as f32;

        if dirty_ratio > self.config.rebuild_threshold {
            // Too many dirty - full rebuild is faster
            self.rebuild_full(world);
        } else {
            // Incremental update
            for entity in self.dirty_entities.drain() {
                self.update_entity_bounds(entity, world);
            }
        }
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_aabb_serialization() {
        let aabb = AABB::new(Vec3::new(-1.0, -2.0, -3.0), Vec3::new(1.0, 2.0, 3.0));

        let serialized = bincode::serialize(&aabb).unwrap();
        let deserialized: AABB = bincode::deserialize(&serialized).unwrap();

        assert_eq!(aabb.min, deserialized.min);
        assert_eq!(aabb.max, deserialized.max);
    }

    #[test]
    fn test_bvh_serialization() {
        let mut bvh = BVH::new();
        bvh.build(&[
            (Entity::from_raw(1), AABB::new(Vec3::ZERO, Vec3::ONE)),
            (Entity::from_raw(2), AABB::new(Vec3::new(5.0, 0.0, 0.0), Vec3::new(6.0, 1.0, 1.0))),
        ]);

        let state = bvh.serialize_state().unwrap();

        let mut restored = BVH::new();
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();

        // Process queued updates
        restored.apply_pending_updates();

        assert_eq!(restored.nodes.len(), bvh.nodes.len());
        assert_eq!(restored.root, bvh.root);
    }

    #[test]
    fn test_spatial_system_hot_reload() {
        let mut world = World::new();
        let e1 = world.spawn((Transform::default(), AABB::new(Vec3::ZERO, Vec3::ONE)));
        let e2 = world.spawn((Transform::default(), AABB::new(Vec3::splat(10.0), Vec3::splat(11.0))));

        let mut system = SpatialIndexSystem::new();
        system.rebuild_full(&world);

        // Serialize
        let state = system.serialize_state().unwrap();

        // Deserialize
        let mut restored = SpatialIndexSystem::new();
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();
        restored.apply_pending_updates(&world);

        // Verify queries work
        let hits = restored.query_aabb(&AABB::new(Vec3::ZERO, Vec3::splat(2.0)));
        assert_eq!(hits.len(), 1);
    }

    #[test]
    fn test_bvh_invalidation_on_entity_removal() {
        let mut world = World::new();
        let e1 = world.spawn((Transform::default(), AABB::new(Vec3::ZERO, Vec3::ONE)));

        let mut system = SpatialIndexSystem::new();
        system.rebuild_full(&world);

        let state = system.serialize_state().unwrap();

        // Remove entity
        world.despawn(e1);

        // Try to restore - should detect invalid state
        let mut restored = SpatialIndexSystem::new();
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();

        // Validation should fail and trigger rebuild
        let bvh_state: BVHState = bincode::deserialize(&state).unwrap();
        assert!(!restored.validate_bvh_state(&bvh_state, &world));
    }

    #[test]
    fn test_frame_boundary_update_queue() {
        let mut system = SpatialIndexSystem::new();

        system.queue_update(SpatialUpdate::UpdateConfig(SpatialQueryConfig {
            rebuild_threshold: 0.5,
            ..Default::default()
        }));
        system.queue_update(SpatialUpdate::FullRebuild);

        assert_eq!(system.update_queue.len(), 2);

        let world = World::new();
        system.apply_pending_updates(&world);

        assert!(system.update_queue.is_empty());
        assert_eq!(system.config.rebuild_threshold, 0.5);
    }
}
```

## Fault Tolerance

### Catch Unwind for Spatial Queries

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};

impl BVH {
    /// Query with panic recovery
    pub fn query_aabb_safe(&self, query: &AABB) -> Result<Vec<Entity>, SpatialError> {
        catch_unwind(AssertUnwindSafe(|| {
            self.query_aabb(query)
        })).map_err(|_| SpatialError::QueryPanic)
    }

    /// Ray query with panic recovery
    pub fn query_ray_safe(&self, ray: &Ray, max_dist: f32) -> Result<Vec<(Entity, f32)>, SpatialError> {
        catch_unwind(AssertUnwindSafe(|| {
            self.query_ray(ray, max_dist)
        })).map_err(|_| SpatialError::QueryPanic)
    }

    /// Frustum cull with panic recovery
    pub fn frustum_cull_safe(&self, frustum: &Frustum) -> Result<Vec<Entity>, SpatialError> {
        catch_unwind(AssertUnwindSafe(|| {
            self.frustum_cull(frustum)
        })).map_err(|_| SpatialError::QueryPanic)
    }
}

impl SpatialIndexSystem {
    /// Safe bounds update with recovery
    pub fn update_safe(&mut self, world: &World) -> Result<(), SpatialError> {
        catch_unwind(AssertUnwindSafe(|| {
            BoundsUpdateSystem::update(world, &mut self.bvh);
        })).map_err(|panic_info| {
            log::error!("BVH update panicked: {:?}", panic_info);
            // Mark for full rebuild on next frame
            self.needs_full_rebuild = true;
            SpatialError::UpdatePanic
        })
    }
}

#[derive(Debug)]
pub enum SpatialError {
    QueryPanic,
    UpdatePanic,
    InvalidState,
    BoundsOverflow,
}
```

### Fallback/Degradation Behavior

```rust
impl SpatialIndexSystem {
    /// Fallback to linear search when BVH is corrupted
    pub fn query_aabb_with_fallback(&self, query: &AABB, world: &World) -> Vec<Entity> {
        // Try BVH first
        match self.bvh.query_aabb_safe(query) {
            Ok(results) => results,
            Err(e) => {
                log::warn!("BVH query failed ({:?}), falling back to linear search", e);
                self.linear_aabb_query(query, world)
            }
        }
    }

    /// Linear fallback - O(n) but always works
    fn linear_aabb_query(&self, query: &AABB, world: &World) -> Vec<Entity> {
        let mut results = Vec::new();

        for (entity, bounds) in world.query::<&BoundingBox>() {
            let aabb = AABB::new(bounds.min.into(), bounds.max.into());
            if aabb.intersects(query) {
                results.push(entity);
            }
        }

        results
    }

    /// Frustum cull with fallback
    pub fn frustum_cull_with_fallback(&self, frustum: &Frustum, world: &World) -> Vec<Entity> {
        match self.bvh.frustum_cull_safe(frustum) {
            Ok(results) => results,
            Err(_) => {
                log::warn!("BVH frustum cull failed, falling back to linear");
                self.linear_frustum_cull(frustum, world)
            }
        }
    }

    fn linear_frustum_cull(&self, frustum: &Frustum, world: &World) -> Vec<Entity> {
        let mut results = Vec::new();

        for (entity, bounds) in world.query::<&BoundingBox>() {
            let aabb = AABB::new(bounds.min.into(), bounds.max.into());
            if frustum.contains_aabb(&aabb) != FrustumTestResult::Outside {
                results.push(entity);
            }
        }

        results
    }

    /// Auto-recovery: rebuild BVH if in degraded state
    pub fn check_and_recover(&mut self, world: &World) {
        if self.needs_full_rebuild || self.in_fallback_mode {
            log::info!("Attempting BVH recovery rebuild");

            match catch_unwind(AssertUnwindSafe(|| {
                self.rebuild_full(world);
            })) {
                Ok(()) => {
                    self.needs_full_rebuild = false;
                    self.in_fallback_mode = false;
                    self.consecutive_failures = 0;
                    log::info!("BVH recovery successful");
                }
                Err(_) => {
                    self.consecutive_failures += 1;
                    if self.consecutive_failures >= 3 {
                        log::error!("BVH rebuild failed {} times, staying in fallback mode",
                            self.consecutive_failures);
                        self.in_fallback_mode = true;
                    }
                }
            }
        }
    }
}

/// Graceful degradation stats for monitoring
#[derive(Default)]
pub struct SpatialSystemStats {
    pub bvh_queries: u64,
    pub fallback_queries: u64,
    pub rebuild_count: u64,
    pub last_rebuild_time_ms: f32,
}
```

## Acceptance Criteria

### Functional

- [ ] AABB computation works correctly
- [ ] Sphere computation works correctly
- [ ] Frustum extraction from matrix works
- [ ] Frustum culling rejects outside objects
- [ ] BVH accelerates spatial queries
- [ ] Bounds update when transforms change
- [ ] Raycasting uses BVH
- [ ] Performance: <1ms for 10,000 entities

### Hot-Swap Compliance

- [ ] AABB and Sphere derive Serialize/Deserialize
- [ ] BVH implements HotReloadable trait
- [ ] SpatialIndexSystem implements HotReloadable trait
- [ ] BVH state validated against world after reload
- [ ] Invalid BVH triggers automatic rebuild
- [ ] Spatial updates queued for frame boundary
- [ ] Query failures fall back to linear search
- [ ] System auto-recovers from corrupted state

## Dependencies

- **Phase 1: Scene Graph** - Transforms for bounds computation

## Dependents

- **Phase 10: Picking & Raycasting** - Acceleration structure
- **Phase 15: LOD System** - Distance queries

---

**Estimated Complexity**: Medium
**Primary Crates**: void_math, void_render
**Reviewer Notes**: BVH quality affects performance significantly
