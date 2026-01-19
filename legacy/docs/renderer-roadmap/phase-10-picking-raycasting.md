# Phase 10: Object Picking & Raycasting

## Status: Not Started

## User Story

> As an application author, I want to interact with scene objects using mouse or touch.

## Requirements Checklist

- [ ] Raycast from screen space into the scene
- [ ] Detect entity intersections
- [ ] Support hit filtering by layer or tag
- [ ] Return hit position, normal, and entity ID
- [ ] Allow entities to opt in/out of picking

## Current State Analysis

### Existing Implementation

No raycasting system exists. The editor has basic gizmo interaction but no scene picking.

### Gaps
1. No ray generation from camera
2. No scene intersection testing
3. No spatial acceleration structures
4. No GPU-based picking
5. No hit result API

## Implementation Specification

### 1. Ray Type

```rust
// crates/void_math/src/ray.rs (NEW FILE)

/// 3D ray for intersection testing
#[derive(Clone, Copy, Debug)]
pub struct Ray {
    /// Ray origin
    pub origin: Vec3,

    /// Ray direction (normalized)
    pub direction: Vec3,
}

impl Ray {
    pub fn new(origin: Vec3, direction: Vec3) -> Self {
        Self {
            origin,
            direction: direction.normalize(),
        }
    }

    /// Create ray from two points
    pub fn from_points(start: Vec3, end: Vec3) -> Self {
        Self::new(start, end - start)
    }

    /// Get point at distance t along ray
    pub fn at(&self, t: f32) -> Vec3 {
        self.origin + self.direction * t
    }

    /// Transform ray by matrix
    pub fn transform(&self, matrix: &Mat4) -> Self {
        let origin = matrix.transform_point(self.origin);
        let direction = matrix.transform_vector(self.direction);
        Self::new(origin, direction)
    }
}
```

### 2. Hit Result

```rust
// crates/void_render/src/picking/hit.rs (NEW FILE)

use void_ecs::Entity;
use void_math::Vec3;

/// Result of a raycast hit
#[derive(Clone, Debug)]
pub struct RaycastHit {
    /// Entity that was hit
    pub entity: Entity,

    /// Distance from ray origin
    pub distance: f32,

    /// World-space hit position
    pub position: Vec3,

    /// World-space surface normal at hit point
    pub normal: Vec3,

    /// UV coordinates at hit point (if available)
    pub uv: Option<[f32; 2]>,

    /// Barycentric coordinates (for triangle hits)
    pub barycentric: Option<[f32; 3]>,

    /// Triangle index (for mesh hits)
    pub triangle_index: Option<u32>,
}

/// Query options for raycasting
#[derive(Clone, Debug, Default)]
pub struct RaycastQuery {
    /// Maximum ray distance
    pub max_distance: f32,

    /// Layer mask to filter entities
    pub layer_mask: u32,

    /// Only hit entities with these tags
    pub require_tags: Vec<String>,

    /// Exclude entities with these tags
    pub exclude_tags: Vec<String>,

    /// Include back faces
    pub hit_back_faces: bool,

    /// Stop at first hit (faster)
    pub first_hit_only: bool,

    /// Entities to ignore
    pub ignore_entities: Vec<Entity>,
}

impl RaycastQuery {
    pub fn new() -> Self {
        Self {
            max_distance: f32::INFINITY,
            layer_mask: u32::MAX,
            require_tags: Vec::new(),
            exclude_tags: Vec::new(),
            hit_back_faces: false,
            first_hit_only: false,
            ignore_entities: Vec::new(),
        }
    }

    pub fn with_max_distance(mut self, distance: f32) -> Self {
        self.max_distance = distance;
        self
    }

    pub fn with_layer_mask(mut self, mask: u32) -> Self {
        self.layer_mask = mask;
        self
    }

    pub fn first_hit(mut self) -> Self {
        self.first_hit_only = true;
        self
    }

    pub fn ignore(mut self, entity: Entity) -> Self {
        self.ignore_entities.push(entity);
        self
    }
}
```

### 3. Screen-to-Ray Conversion

```rust
// crates/void_render/src/picking/screen_ray.rs (NEW FILE)

use void_math::{Vec3, Mat4, Ray};

/// Convert screen coordinates to world ray
pub fn screen_to_ray(
    screen_pos: [f32; 2],        // Screen coordinates (0,0 = top-left)
    screen_size: [f32; 2],       // Screen width/height
    view_matrix: &Mat4,          // Camera view matrix
    projection_matrix: &Mat4,    // Camera projection matrix
) -> Ray {
    // Convert to NDC (-1 to 1)
    let ndc_x = (screen_pos[0] / screen_size[0]) * 2.0 - 1.0;
    let ndc_y = 1.0 - (screen_pos[1] / screen_size[1]) * 2.0;  // Flip Y

    // Inverse view-projection
    let inv_view = view_matrix.inverse();
    let inv_proj = projection_matrix.inverse();
    let inv_vp = inv_view * inv_proj;

    // Unproject near and far points
    let near_point = inv_vp.transform_point(Vec3::new(ndc_x, ndc_y, 0.0));
    let far_point = inv_vp.transform_point(Vec3::new(ndc_x, ndc_y, 1.0));

    Ray::from_points(near_point, far_point)
}

/// Convert world point to screen coordinates
pub fn world_to_screen(
    world_pos: Vec3,
    screen_size: [f32; 2],
    view_matrix: &Mat4,
    projection_matrix: &Mat4,
) -> Option<[f32; 2]> {
    let vp = projection_matrix * view_matrix;
    let clip = vp.transform_point(world_pos);

    // Check if behind camera
    if clip.z < 0.0 {
        return None;
    }

    // Perspective divide
    let ndc_x = clip.x / clip.w;
    let ndc_y = clip.y / clip.w;

    // Convert to screen coordinates
    let screen_x = (ndc_x + 1.0) * 0.5 * screen_size[0];
    let screen_y = (1.0 - ndc_y) * 0.5 * screen_size[1];

    Some([screen_x, screen_y])
}
```

### 4. Intersection Tests

```rust
// crates/void_math/src/intersect.rs (NEW FILE)

use crate::{Ray, Vec3, AABB, Sphere};

/// Ray-AABB intersection
pub fn ray_aabb(ray: &Ray, aabb: &AABB) -> Option<f32> {
    let inv_dir = Vec3::new(
        1.0 / ray.direction.x,
        1.0 / ray.direction.y,
        1.0 / ray.direction.z,
    );

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

/// Ray-Sphere intersection
pub fn ray_sphere(ray: &Ray, center: Vec3, radius: f32) -> Option<f32> {
    let oc = ray.origin - center;
    let a = ray.direction.dot(ray.direction);
    let b = 2.0 * oc.dot(ray.direction);
    let c = oc.dot(oc) - radius * radius;
    let discriminant = b * b - 4.0 * a * c;

    if discriminant < 0.0 {
        None
    } else {
        let t = (-b - discriminant.sqrt()) / (2.0 * a);
        if t > 0.0 {
            Some(t)
        } else {
            let t = (-b + discriminant.sqrt()) / (2.0 * a);
            if t > 0.0 { Some(t) } else { None }
        }
    }
}

/// Ray-Triangle intersection (Möller–Trumbore)
pub fn ray_triangle(
    ray: &Ray,
    v0: Vec3,
    v1: Vec3,
    v2: Vec3,
    cull_backface: bool,
) -> Option<(f32, [f32; 3])> {
    const EPSILON: f32 = 0.0000001;

    let edge1 = v1 - v0;
    let edge2 = v2 - v0;
    let h = ray.direction.cross(edge2);
    let a = edge1.dot(h);

    // Check if ray is parallel to triangle
    if a.abs() < EPSILON {
        return None;
    }

    // Backface culling
    if cull_backface && a < 0.0 {
        return None;
    }

    let f = 1.0 / a;
    let s = ray.origin - v0;
    let u = f * s.dot(h);

    if u < 0.0 || u > 1.0 {
        return None;
    }

    let q = s.cross(edge1);
    let v = f * ray.direction.dot(q);

    if v < 0.0 || u + v > 1.0 {
        return None;
    }

    let t = f * edge2.dot(q);

    if t > EPSILON {
        let w = 1.0 - u - v;
        Some((t, [w, u, v]))  // Barycentric coordinates
    } else {
        None
    }
}

/// Calculate triangle normal from barycentric hit
pub fn triangle_normal(
    v0: Vec3,
    v1: Vec3,
    v2: Vec3,
    n0: Vec3,
    n1: Vec3,
    n2: Vec3,
    bary: [f32; 3],
) -> Vec3 {
    // Interpolate vertex normals
    (n0 * bary[0] + n1 * bary[1] + n2 * bary[2]).normalize()
}
```

### 5. Raycast System

```rust
// crates/void_render/src/picking/raycast.rs (NEW FILE)

use void_ecs::{World, Entity};
use void_math::{Ray, Vec3, AABB};
use crate::picking::{RaycastHit, RaycastQuery};

/// Performs raycasts against scene entities
pub struct RaycastSystem;

impl RaycastSystem {
    /// Cast ray and return all hits sorted by distance
    pub fn raycast(
        world: &World,
        ray: &Ray,
        query: &RaycastQuery,
    ) -> Vec<RaycastHit> {
        let mut hits = Vec::new();

        // Query pickable entities
        for (entity, (transform, mesh_renderer, bounds, pickable)) in
            world.query::<(&GlobalTransform, &MeshRenderer, Option<&BoundingBox>, Option<&Pickable>)>()
        {
            // Check pickable settings
            if let Some(p) = pickable {
                if !p.enabled {
                    continue;
                }
            }

            // Check layer mask
            if let Some(visible) = world.get::<Visible>(entity) {
                if visible.layer_mask & query.layer_mask == 0 {
                    continue;
                }
            }

            // Check ignore list
            if query.ignore_entities.contains(&entity) {
                continue;
            }

            // Transform ray to local space
            let inv_transform = transform.matrix.inverse();
            let local_ray = ray.transform(&inv_transform);

            // Broad phase: AABB test
            if let Some(aabb) = bounds {
                if ray_aabb(&local_ray, &aabb.to_aabb()).is_none() {
                    continue;
                }
            }

            // Narrow phase: mesh intersection
            if let Some(hit) = Self::intersect_mesh(
                world, entity, &local_ray, transform, mesh_renderer, query
            ) {
                if hit.distance <= query.max_distance {
                    hits.push(hit);

                    if query.first_hit_only {
                        break;
                    }
                }
            }
        }

        // Sort by distance
        hits.sort_by(|a, b| a.distance.partial_cmp(&b.distance).unwrap());

        hits
    }

    /// Cast ray and return closest hit only
    pub fn raycast_first(
        world: &World,
        ray: &Ray,
        query: &RaycastQuery,
    ) -> Option<RaycastHit> {
        let mut query = query.clone();
        query.first_hit_only = true;
        Self::raycast(world, ray, &query).into_iter().next()
    }

    fn intersect_mesh(
        world: &World,
        entity: Entity,
        local_ray: &Ray,
        transform: &GlobalTransform,
        mesh_renderer: &MeshRenderer,
        query: &RaycastQuery,
    ) -> Option<RaycastHit> {
        // Get mesh data
        let mesh = match &mesh_renderer.primitive {
            Some(prim) => Self::get_primitive_mesh(prim),
            None => {
                // TODO: Get from mesh cache
                return None;
            }
        };

        let mut closest: Option<(f32, [f32; 3], u32)> = None;

        // Test each triangle
        for (i, tri) in mesh.triangles.iter().enumerate() {
            let v0 = mesh.positions[tri[0] as usize];
            let v1 = mesh.positions[tri[1] as usize];
            let v2 = mesh.positions[tri[2] as usize];

            if let Some((t, bary)) = ray_triangle(
                local_ray,
                Vec3::from(v0),
                Vec3::from(v1),
                Vec3::from(v2),
                !query.hit_back_faces,
            ) {
                if closest.is_none() || t < closest.unwrap().0 {
                    closest = Some((t, bary, i as u32));
                }
            }
        }

        closest.map(|(t, bary, tri_idx)| {
            // Transform hit to world space
            let local_pos = local_ray.at(t);
            let world_pos = transform.matrix.transform_point(local_pos);

            // Calculate normal
            let tri = &mesh.triangles[tri_idx as usize];
            let n0 = Vec3::from(mesh.normals[tri[0] as usize]);
            let n1 = Vec3::from(mesh.normals[tri[1] as usize]);
            let n2 = Vec3::from(mesh.normals[tri[2] as usize]);
            let local_normal = triangle_normal(
                Vec3::from(mesh.positions[tri[0] as usize]),
                Vec3::from(mesh.positions[tri[1] as usize]),
                Vec3::from(mesh.positions[tri[2] as usize]),
                n0, n1, n2,
                bary,
            );
            let world_normal = transform.matrix.transform_vector(local_normal).normalize();

            // Calculate UV
            let uv = if !mesh.uvs.is_empty() {
                let uv0 = mesh.uvs[tri[0] as usize];
                let uv1 = mesh.uvs[tri[1] as usize];
                let uv2 = mesh.uvs[tri[2] as usize];
                Some([
                    uv0[0] * bary[0] + uv1[0] * bary[1] + uv2[0] * bary[2],
                    uv0[1] * bary[0] + uv1[1] * bary[1] + uv2[1] * bary[2],
                ])
            } else {
                None
            };

            RaycastHit {
                entity,
                distance: (world_pos - ray.origin).length(),
                position: world_pos.into(),
                normal: world_normal.into(),
                uv,
                barycentric: Some(bary),
                triangle_index: Some(tri_idx),
            }
        })
    }

    fn get_primitive_mesh(primitive: &PrimitiveType) -> SimpleMesh {
        // Return pre-generated mesh data for primitives
        match primitive {
            PrimitiveType::Cube => generate_cube_mesh(),
            PrimitiveType::Sphere { segments } => generate_sphere_mesh(*segments),
            PrimitiveType::Plane { .. } => generate_plane_mesh(),
            _ => SimpleMesh::default(),
        }
    }
}

/// Simplified mesh for raycasting
#[derive(Clone, Debug, Default)]
struct SimpleMesh {
    positions: Vec<[f32; 3]>,
    normals: Vec<[f32; 3]>,
    uvs: Vec<[f32; 2]>,
    triangles: Vec<[u32; 3]>,
}
```

### 6. Pickable Component

```rust
// crates/void_ecs/src/components/pickable.rs (NEW FILE)

/// Marks entity as pickable by raycasts
#[derive(Clone, Debug)]
pub struct Pickable {
    /// Is picking enabled
    pub enabled: bool,

    /// Use mesh collider (accurate) or bounds (fast)
    pub use_mesh_collider: bool,

    /// Custom collision shape override
    pub collider_shape: Option<ColliderShape>,
}

#[derive(Clone, Debug)]
pub enum ColliderShape {
    Box { half_extents: [f32; 3] },
    Sphere { radius: f32 },
    Capsule { radius: f32, height: f32 },
    Custom { mesh: String },
}

impl Default for Pickable {
    fn default() -> Self {
        Self {
            enabled: true,
            use_mesh_collider: false,  // Bounds by default for perf
            collider_shape: None,
        }
    }
}

impl Pickable {
    pub fn mesh_accurate() -> Self {
        Self {
            enabled: true,
            use_mesh_collider: true,
            collider_shape: None,
        }
    }

    pub fn box_collider(half_extents: [f32; 3]) -> Self {
        Self {
            enabled: true,
            use_mesh_collider: false,
            collider_shape: Some(ColliderShape::Box { half_extents }),
        }
    }

    pub fn sphere_collider(radius: f32) -> Self {
        Self {
            enabled: true,
            use_mesh_collider: false,
            collider_shape: Some(ColliderShape::Sphere { radius }),
        }
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_math/src/ray.rs` | CREATE | Ray type |
| `void_math/src/intersect.rs` | CREATE | Intersection tests |
| `void_render/src/picking/mod.rs` | CREATE | Picking module |
| `void_render/src/picking/hit.rs` | CREATE | Hit result types |
| `void_render/src/picking/screen_ray.rs` | CREATE | Screen-to-ray |
| `void_render/src/picking/raycast.rs` | CREATE | Raycast system |
| `void_ecs/src/components/pickable.rs` | CREATE | Pickable component |
| `void_math/src/lib.rs` | MODIFY | Export new modules |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_ray_aabb_hit() {
    let ray = Ray::new(Vec3::ZERO, Vec3::Z);
    let aabb = AABB::new(Vec3::new(-1.0, -1.0, 5.0), Vec3::new(1.0, 1.0, 7.0));

    let hit = ray_aabb(&ray, &aabb);
    assert!(hit.is_some());
    assert!((hit.unwrap() - 5.0).abs() < 0.01);
}

#[test]
fn test_ray_aabb_miss() {
    let ray = Ray::new(Vec3::ZERO, Vec3::X);
    let aabb = AABB::new(Vec3::new(-1.0, -1.0, 5.0), Vec3::new(1.0, 1.0, 7.0));

    assert!(ray_aabb(&ray, &aabb).is_none());
}

#[test]
fn test_ray_triangle() {
    let ray = Ray::new(Vec3::new(0.0, 0.0, -1.0), Vec3::Z);
    let v0 = Vec3::new(-1.0, -1.0, 0.0);
    let v1 = Vec3::new(1.0, -1.0, 0.0);
    let v2 = Vec3::new(0.0, 1.0, 0.0);

    let hit = ray_triangle(&ray, v0, v1, v2, true);
    assert!(hit.is_some());
}

#[test]
fn test_screen_to_ray() {
    let view = Mat4::look_at(Vec3::new(0.0, 0.0, 5.0), Vec3::ZERO, Vec3::Y);
    let proj = Mat4::perspective(60.0_f32.to_radians(), 16.0/9.0, 0.1, 100.0);

    // Center of screen should point at origin
    let ray = screen_to_ray([960.0, 540.0], [1920.0, 1080.0], &view, &proj);

    assert!((ray.direction.z - (-1.0)).abs() < 0.1);
}
```

## Hot-Swap Support

### Serialization

All picking components must be serializable for hot-swap:

```rust
// crates/void_ecs/src/components/pickable.rs

use serde::{Serialize, Deserialize};

/// Marks entity as pickable by raycasts
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Pickable {
    pub enabled: bool,
    pub use_mesh_collider: bool,
    pub collider_shape: Option<ColliderShape>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ColliderShape {
    Box { half_extents: [f32; 3] },
    Sphere { radius: f32 },
    Capsule { radius: f32, height: f32 },
    Custom { mesh: String },
}

// crates/void_render/src/picking/hit.rs

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RaycastHit {
    pub entity: Entity,
    pub distance: f32,
    pub position: Vec3,
    pub normal: Vec3,
    pub uv: Option<[f32; 2]>,
    pub barycentric: Option<[f32; 3]>,
    pub triangle_index: Option<u32>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RaycastQuery {
    pub max_distance: f32,
    pub layer_mask: u32,
    pub require_tags: Vec<String>,
    pub exclude_tags: Vec<String>,
    pub hit_back_faces: bool,
    pub first_hit_only: bool,
    #[serde(skip)]
    pub ignore_entities: Vec<Entity>,
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/picking/raycast.rs

use void_core::hot_reload::{HotReloadable, HotReloadContext};

/// State for RaycastSystem that persists across hot-reloads
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RaycastSystemState {
    /// Cached primitive meshes for intersection testing
    pub primitive_cache_valid: bool,
    /// Last frame's hit results for continuity
    pub last_frame_hits: Vec<(Entity, f32)>,
}

impl HotReloadable for RaycastSystem {
    type State = RaycastSystemState;

    fn save_state(&self) -> Self::State {
        RaycastSystemState {
            primitive_cache_valid: self.primitive_cache.is_some(),
            last_frame_hits: self.last_hits.iter()
                .map(|h| (h.entity, h.distance))
                .collect(),
        }
    }

    fn restore_state(&mut self, state: Self::State, _ctx: &HotReloadContext) {
        // Invalidate cache - will rebuild on next frame
        if !state.primitive_cache_valid {
            self.primitive_cache = None;
        }
        // Restore hit continuity for smooth transitions
        self.pending_restore_hits = state.last_frame_hits;
    }

    fn version() -> u32 {
        1
    }
}

impl HotReloadable for Pickable {
    type State = Pickable;

    fn save_state(&self) -> Self::State {
        self.clone()
    }

    fn restore_state(&mut self, state: Self::State, _ctx: &HotReloadContext) {
        *self = state;
    }

    fn version() -> u32 {
        1
    }
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/picking/mod.rs

use std::sync::mpsc::{channel, Sender, Receiver};

/// Pending updates to be applied at frame boundary
#[derive(Debug)]
pub enum PickingUpdate {
    UpdatePickable { entity: Entity, pickable: Pickable },
    InvalidateCache { entity: Entity },
    ClearAllCaches,
}

pub struct PickingUpdateQueue {
    sender: Sender<PickingUpdate>,
    receiver: Receiver<PickingUpdate>,
}

impl PickingUpdateQueue {
    pub fn new() -> Self {
        let (sender, receiver) = channel();
        Self { sender, receiver }
    }

    /// Queue update for next frame boundary
    pub fn queue(&self, update: PickingUpdate) {
        let _ = self.sender.send(update);
    }

    /// Apply all pending updates (call at frame boundary)
    pub fn apply_pending(&self, system: &mut RaycastSystem, world: &mut World) {
        while let Ok(update) = self.receiver.try_recv() {
            match update {
                PickingUpdate::UpdatePickable { entity, pickable } => {
                    world.insert(entity, pickable);
                }
                PickingUpdate::InvalidateCache { entity } => {
                    system.invalidate_entity_cache(entity);
                }
                PickingUpdate::ClearAllCaches => {
                    system.clear_all_caches();
                }
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
    fn test_pickable_serialization_roundtrip() {
        let pickable = Pickable {
            enabled: true,
            use_mesh_collider: true,
            collider_shape: Some(ColliderShape::Sphere { radius: 1.5 }),
        };

        let serialized = serde_json::to_string(&pickable).unwrap();
        let deserialized: Pickable = serde_json::from_str(&serialized).unwrap();

        assert_eq!(pickable.enabled, deserialized.enabled);
        assert_eq!(pickable.use_mesh_collider, deserialized.use_mesh_collider);
    }

    #[test]
    fn test_raycast_system_state_preservation() {
        let mut system = RaycastSystem::new();
        // Simulate some hits
        system.last_hits.push(RaycastHit {
            entity: Entity::from_raw(42),
            distance: 10.0,
            position: Vec3::ZERO,
            normal: Vec3::Y,
            uv: None,
            barycentric: None,
            triangle_index: None,
        });

        let state = system.save_state();
        let mut new_system = RaycastSystem::new();
        new_system.restore_state(state, &HotReloadContext::default());

        assert_eq!(new_system.pending_restore_hits.len(), 1);
        assert_eq!(new_system.pending_restore_hits[0].0, Entity::from_raw(42));
    }

    #[test]
    fn test_picking_update_queue() {
        let queue = PickingUpdateQueue::new();
        let entity = Entity::from_raw(1);

        queue.queue(PickingUpdate::UpdatePickable {
            entity,
            pickable: Pickable::default(),
        });
        queue.queue(PickingUpdate::InvalidateCache { entity });

        let mut system = RaycastSystem::new();
        let mut world = World::new();

        queue.apply_pending(&mut system, &mut world);

        assert!(world.get::<Pickable>(entity).is_some());
    }

    #[test]
    fn test_collider_shape_all_variants_serialize() {
        let shapes = vec![
            ColliderShape::Box { half_extents: [1.0, 2.0, 3.0] },
            ColliderShape::Sphere { radius: 5.0 },
            ColliderShape::Capsule { radius: 1.0, height: 2.0 },
            ColliderShape::Custom { mesh: "custom.obj".into() },
        ];

        for shape in shapes {
            let json = serde_json::to_string(&shape).unwrap();
            let restored: ColliderShape = serde_json::from_str(&json).unwrap();
            // Verify roundtrip succeeds
            let json2 = serde_json::to_string(&restored).unwrap();
            assert_eq!(json, json2);
        }
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_render/src/picking/raycast.rs

use std::panic::{catch_unwind, AssertUnwindSafe};
use log::{error, warn};

impl RaycastSystem {
    /// Cast ray with fault tolerance - never panics
    pub fn raycast_safe(
        world: &World,
        ray: &Ray,
        query: &RaycastQuery,
    ) -> Vec<RaycastHit> {
        let result = catch_unwind(AssertUnwindSafe(|| {
            Self::raycast(world, ray, query)
        }));

        match result {
            Ok(hits) => hits,
            Err(e) => {
                error!(
                    "Raycast panicked: {:?}. Returning empty results.",
                    e.downcast_ref::<&str>()
                );
                Vec::new()
            }
        }
    }

    /// Intersect mesh with fallback to bounds
    fn intersect_mesh_safe(
        world: &World,
        entity: Entity,
        local_ray: &Ray,
        transform: &GlobalTransform,
        mesh_renderer: &MeshRenderer,
        query: &RaycastQuery,
    ) -> Option<RaycastHit> {
        let result = catch_unwind(AssertUnwindSafe(|| {
            Self::intersect_mesh(world, entity, local_ray, transform, mesh_renderer, query)
        }));

        match result {
            Ok(hit) => hit,
            Err(_) => {
                warn!(
                    "Mesh intersection failed for entity {:?}, falling back to bounds",
                    entity
                );
                // Fallback: use bounding box intersection
                Self::intersect_bounds_fallback(entity, local_ray, transform)
            }
        }
    }

    /// Fallback intersection using entity bounds
    fn intersect_bounds_fallback(
        entity: Entity,
        ray: &Ray,
        transform: &GlobalTransform,
    ) -> Option<RaycastHit> {
        // Use unit AABB as fallback
        let aabb = AABB::new(Vec3::splat(-0.5), Vec3::splat(0.5));
        ray_aabb(ray, &aabb).map(|t| {
            let local_pos = ray.at(t);
            let world_pos = transform.matrix.transform_point(local_pos);
            RaycastHit {
                entity,
                distance: t,
                position: world_pos,
                normal: Vec3::Y, // Approximate normal
                uv: None,
                barycentric: None,
                triangle_index: None,
            }
        })
    }
}
```

### Degradation Behavior

```rust
// crates/void_render/src/picking/raycast.rs

/// Picking quality levels for graceful degradation
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PickingQuality {
    /// Full triangle-level intersection
    Full,
    /// Bounding box only (faster, less accurate)
    BoundsOnly,
    /// Disabled (return no hits)
    Disabled,
}

impl RaycastSystem {
    /// Current quality level, can degrade under pressure
    quality: PickingQuality,

    /// Degrade quality if performance is suffering
    pub fn check_performance_and_degrade(&mut self, last_frame_ms: f32) {
        const DEGRADE_THRESHOLD_MS: f32 = 2.0;
        const RESTORE_THRESHOLD_MS: f32 = 0.5;

        match self.quality {
            PickingQuality::Full if last_frame_ms > DEGRADE_THRESHOLD_MS => {
                warn!("Picking taking {}ms, degrading to bounds-only", last_frame_ms);
                self.quality = PickingQuality::BoundsOnly;
            }
            PickingQuality::BoundsOnly if last_frame_ms < RESTORE_THRESHOLD_MS => {
                self.quality = PickingQuality::Full;
            }
            _ => {}
        }
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Screen coordinates convert to world rays
- [ ] Ray-AABB intersection works
- [ ] Ray-triangle intersection works
- [ ] Mesh picking returns correct entity
- [ ] Hit position and normal are accurate
- [ ] Layer filtering works
- [ ] Entities can opt out via Pickable component
- [ ] Performance: <1ms for 1000 entities
- [ ] Editor selection uses this system

### Hot-Swap Compliance

- [ ] `Pickable` component has `#[derive(Serialize, Deserialize)]`
- [ ] `ColliderShape` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `RaycastHit` has `#[derive(Serialize, Deserialize)]`
- [ ] `RaycastQuery` has `#[derive(Serialize, Deserialize)]` with `#[serde(skip)]` on non-serializable fields
- [ ] `RaycastSystem` implements `HotReloadable` trait
- [ ] `PickingUpdateQueue` supports frame-boundary updates
- [ ] Hot-swap preserves last-frame hit continuity
- [ ] All hot-swap tests pass
- [ ] `catch_unwind` protects raycast operations
- [ ] Fallback to bounds intersection on mesh failure

## Dependencies

- **Phase 1: Scene Graph** - Transform hierarchy
- **Phase 2: Camera System** - View/projection matrices
- **Phase 14: Spatial Queries** - AABB/BVH acceleration

## Dependents

- **Phase 11: Entity Input Events** - Uses picking for hover/click

---

**Estimated Complexity**: Medium
**Primary Crates**: void_math, void_render
**Reviewer Notes**: Verify triangle intersection handles edge cases
