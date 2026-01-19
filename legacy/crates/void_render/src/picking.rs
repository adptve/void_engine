//! Object Picking & Raycasting System
//!
//! Provides raycasting functionality for object selection and interaction.
//!
//! # Features
//!
//! - Screen-to-ray conversion for mouse picking
//! - Ray intersection with primitives (AABB, sphere, mesh)
//! - Hit filtering by layer, tag, and entity
//! - Accurate hit position, normal, and UV coordinates
//! - Hot-reload serialization support
//!
//! # Example
//!
//! ```ignore
//! use void_render::picking::*;
//! use void_math::Ray;
//!
//! // Convert screen click to world ray
//! let ray = screen_to_ray(
//!     [mouse_x, mouse_y],
//!     [screen_width, screen_height],
//!     &view_matrix,
//!     &projection_matrix,
//! );
//!
//! // Create raycast query
//! let query = RaycastQuery::new()
//!     .with_max_distance(100.0)
//!     .with_layer_mask(0b0001);
//!
//! // Perform raycast (returns hits sorted by distance)
//! let hits = raycast_world(&world, &ray, &query);
//! if let Some(hit) = hits.first() {
//!     println!("Hit entity {:?} at {:?}", hit.entity, hit.position);
//! }
//! ```

use alloc::string::String;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use void_ecs::Entity;
use void_math::{Vec3, Mat4, Ray, AABB, ray_aabb, ray_sphere_at, ray_triangle, TriangleHit};

// ============================================================================
// RaycastHit
// ============================================================================

/// Result of a raycast hit
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RaycastHit {
    /// Entity that was hit (stored as bits for serialization)
    pub entity_bits: u64,
    /// Distance from ray origin to hit point
    pub distance: f32,
    /// World-space hit position
    pub position: [f32; 3],
    /// World-space surface normal at hit point
    pub normal: [f32; 3],
    /// UV coordinates at hit point (if available)
    pub uv: Option<[f32; 2]>,
    /// Barycentric coordinates (for triangle hits)
    pub barycentric: Option<[f32; 3]>,
    /// Triangle index (for mesh hits)
    pub triangle_index: Option<u32>,
}

impl RaycastHit {
    /// Create a new raycast hit
    pub fn new(entity: Entity, distance: f32, position: Vec3, normal: Vec3) -> Self {
        Self {
            entity_bits: entity.to_bits(),
            distance,
            position: position.to_array(),
            normal: normal.to_array(),
            uv: None,
            barycentric: None,
            triangle_index: None,
        }
    }

    /// Get the entity that was hit
    pub fn entity(&self) -> Entity {
        Entity::from_bits(self.entity_bits)
    }

    /// Set the entity
    pub fn set_entity(&mut self, entity: Entity) {
        self.entity_bits = entity.to_bits();
    }

    /// Get position as Vec3
    pub fn position_vec3(&self) -> Vec3 {
        Vec3::new(self.position[0], self.position[1], self.position[2])
    }

    /// Get normal as Vec3
    pub fn normal_vec3(&self) -> Vec3 {
        Vec3::new(self.normal[0], self.normal[1], self.normal[2])
    }
}

// ============================================================================
// RaycastQuery
// ============================================================================

/// Query options for raycasting
#[derive(Clone, Debug, Serialize, Deserialize)]
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
    #[serde(skip)]
    pub ignore_entities: Vec<Entity>,
}

impl Default for RaycastQuery {
    fn default() -> Self {
        Self {
            max_distance: f32::MAX,
            layer_mask: u32::MAX,
            require_tags: Vec::new(),
            exclude_tags: Vec::new(),
            hit_back_faces: false,
            first_hit_only: false,
            ignore_entities: Vec::new(),
        }
    }
}

impl RaycastQuery {
    /// Create a new raycast query with default settings
    pub fn new() -> Self {
        Self::default()
    }

    /// Set maximum distance
    pub fn with_max_distance(mut self, distance: f32) -> Self {
        self.max_distance = distance;
        self
    }

    /// Set layer mask for filtering
    pub fn with_layer_mask(mut self, mask: u32) -> Self {
        self.layer_mask = mask;
        self
    }

    /// Require entities to have specific tags
    pub fn require_tag(mut self, tag: impl Into<String>) -> Self {
        self.require_tags.push(tag.into());
        self
    }

    /// Exclude entities with specific tags
    pub fn exclude_tag(mut self, tag: impl Into<String>) -> Self {
        self.exclude_tags.push(tag.into());
        self
    }

    /// Enable back-face hitting
    pub fn with_back_faces(mut self) -> Self {
        self.hit_back_faces = true;
        self
    }

    /// Stop at first hit only
    pub fn first_hit(mut self) -> Self {
        self.first_hit_only = true;
        self
    }

    /// Ignore a specific entity
    pub fn ignore(mut self, entity: Entity) -> Self {
        self.ignore_entities.push(entity);
        self
    }

    /// Ignore multiple entities
    pub fn ignore_all(mut self, entities: &[Entity]) -> Self {
        self.ignore_entities.extend_from_slice(entities);
        self
    }

    /// Check if entity should be ignored
    pub fn should_ignore(&self, entity: Entity) -> bool {
        self.ignore_entities.contains(&entity)
    }
}

// ============================================================================
// Screen-to-Ray Conversion
// ============================================================================

/// Convert screen coordinates to a world-space ray
///
/// # Arguments
/// * `screen_pos` - Screen coordinates [x, y] where (0, 0) is top-left
/// * `screen_size` - Screen dimensions [width, height]
/// * `view_matrix` - Camera view matrix
/// * `projection_matrix` - Camera projection matrix
///
/// # Returns
/// A ray in world space originating from the camera
pub fn screen_to_ray(
    screen_pos: [f32; 2],
    screen_size: [f32; 2],
    view_matrix: &Mat4,
    projection_matrix: &Mat4,
) -> Ray {
    // Convert to normalized device coordinates (-1 to 1)
    let ndc_x = (screen_pos[0] / screen_size[0]) * 2.0 - 1.0;
    let ndc_y = 1.0 - (screen_pos[1] / screen_size[1]) * 2.0; // Flip Y

    // Inverse view-projection matrix
    let inv_view = view_matrix.inverse();
    let inv_proj = projection_matrix.inverse();

    // Unproject near and far points
    let near_ndc = inv_proj.transform_point(Vec3::new(ndc_x, ndc_y, -1.0));
    let far_ndc = inv_proj.transform_point(Vec3::new(ndc_x, ndc_y, 1.0));

    // Transform to world space
    let near_world = inv_view.transform_point(near_ndc);
    let far_world = inv_view.transform_point(far_ndc);

    Ray::from_points(near_world, far_world)
}

/// Convert world-space point to screen coordinates
///
/// # Returns
/// Screen coordinates, or None if the point is behind the camera
pub fn world_to_screen(
    world_pos: Vec3,
    screen_size: [f32; 2],
    view_matrix: &Mat4,
    projection_matrix: &Mat4,
) -> Option<[f32; 2]> {
    // Transform to clip space
    let view_pos = view_matrix.transform_point(world_pos);
    let clip_pos = projection_matrix.transform_point(view_pos);

    // Check if behind camera (in clip space, z < -w means behind)
    // For a standard perspective matrix, if view_pos.z > 0, it's behind
    if view_pos.z > 0.0 {
        return None;
    }

    // Perspective divide (clip pos has w embedded in z for some matrices)
    // Assuming standard projection where w=1 after transform_point
    let ndc_x = clip_pos.x;
    let ndc_y = clip_pos.y;

    // Convert to screen coordinates
    let screen_x = (ndc_x + 1.0) * 0.5 * screen_size[0];
    let screen_y = (1.0 - ndc_y) * 0.5 * screen_size[1];

    Some([screen_x, screen_y])
}

// ============================================================================
// Primitive Intersection Helpers
// ============================================================================

/// Test ray against an axis-aligned bounding box
pub fn raycast_aabb(ray: &Ray, aabb: &AABB, max_distance: f32) -> Option<f32> {
    ray_aabb(ray, aabb).filter(|&t| t <= max_distance && t >= 0.0)
}

/// Test ray against a sphere
pub fn raycast_sphere(ray: &Ray, center: Vec3, radius: f32, max_distance: f32) -> Option<f32> {
    ray_sphere_at(ray, center, radius).filter(|&t| t <= max_distance && t >= 0.0)
}

/// Test ray against a triangle mesh
///
/// Returns the closest hit with barycentric coordinates and triangle index.
pub fn raycast_triangles(
    ray: &Ray,
    positions: &[[f32; 3]],
    indices: &[[u32; 3]],
    cull_backface: bool,
    max_distance: f32,
) -> Option<(f32, [f32; 3], u32)> {
    let mut closest: Option<(f32, [f32; 3], u32)> = None;

    for (tri_idx, tri) in indices.iter().enumerate() {
        let v0 = Vec3::new(
            positions[tri[0] as usize][0],
            positions[tri[0] as usize][1],
            positions[tri[0] as usize][2],
        );
        let v1 = Vec3::new(
            positions[tri[1] as usize][0],
            positions[tri[1] as usize][1],
            positions[tri[1] as usize][2],
        );
        let v2 = Vec3::new(
            positions[tri[2] as usize][0],
            positions[tri[2] as usize][1],
            positions[tri[2] as usize][2],
        );

        if let Some(hit) = ray_triangle(ray, v0, v1, v2, cull_backface) {
            if hit.distance <= max_distance {
                if closest.is_none() || hit.distance < closest.unwrap().0 {
                    closest = Some((hit.distance, hit.barycentric, tri_idx as u32));
                }
            }
        }
    }

    closest
}

/// Calculate world-space normal from triangle hit
pub fn calculate_triangle_normal(
    positions: &[[f32; 3]],
    normals: Option<&[[f32; 3]]>,
    indices: &[u32; 3],
    barycentric: [f32; 3],
    transform: &Mat4,
) -> Vec3 {
    if let Some(normals) = normals {
        // Interpolate vertex normals
        let n0 = Vec3::new(
            normals[indices[0] as usize][0],
            normals[indices[0] as usize][1],
            normals[indices[0] as usize][2],
        );
        let n1 = Vec3::new(
            normals[indices[1] as usize][0],
            normals[indices[1] as usize][1],
            normals[indices[1] as usize][2],
        );
        let n2 = Vec3::new(
            normals[indices[2] as usize][0],
            normals[indices[2] as usize][1],
            normals[indices[2] as usize][2],
        );

        let local_normal = (n0 * barycentric[0] + n1 * barycentric[1] + n2 * barycentric[2]).normalize();
        transform.transform_vector(local_normal).normalize()
    } else {
        // Calculate face normal
        let v0 = Vec3::new(
            positions[indices[0] as usize][0],
            positions[indices[0] as usize][1],
            positions[indices[0] as usize][2],
        );
        let v1 = Vec3::new(
            positions[indices[1] as usize][0],
            positions[indices[1] as usize][1],
            positions[indices[1] as usize][2],
        );
        let v2 = Vec3::new(
            positions[indices[2] as usize][0],
            positions[indices[2] as usize][1],
            positions[indices[2] as usize][2],
        );

        let edge1 = v1 - v0;
        let edge2 = v2 - v0;
        let local_normal = edge1.cross(edge2).normalize();
        transform.transform_vector(local_normal).normalize()
    }
}

/// Calculate UV from triangle hit
pub fn calculate_triangle_uv(
    uvs: &[[f32; 2]],
    indices: &[u32; 3],
    barycentric: [f32; 3],
) -> [f32; 2] {
    let uv0 = uvs[indices[0] as usize];
    let uv1 = uvs[indices[1] as usize];
    let uv2 = uvs[indices[2] as usize];

    [
        uv0[0] * barycentric[0] + uv1[0] * barycentric[1] + uv2[0] * barycentric[2],
        uv0[1] * barycentric[0] + uv1[1] * barycentric[1] + uv2[1] * barycentric[2],
    ]
}

// ============================================================================
// Hot-Reload State
// ============================================================================

/// Raycast system state for hot-reload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RaycastSystemState {
    /// Last frame hit entities for continuity
    pub last_hits: Vec<(u64, f32)>, // (entity id, distance)
}

impl RaycastSystemState {
    /// Create a new empty state
    pub fn new() -> Self {
        Self::default()
    }

    /// Record hits from this frame
    pub fn record_hits(&mut self, hits: &[RaycastHit]) {
        self.last_hits = hits
            .iter()
            .map(|h| (h.entity_bits, h.distance))
            .collect();
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use void_math::{Mat4, Vec3};

    #[test]
    fn test_raycast_hit_creation() {
        let entity = Entity::from_bits(42);
        let hit = RaycastHit::new(
            entity,
            10.0,
            Vec3::new(1.0, 2.0, 3.0),
            Vec3::Y,
        );

        assert_eq!(hit.entity_bits, 42);
        assert_eq!(hit.entity(), entity);
        assert_eq!(hit.distance, 10.0);
        assert_eq!(hit.position, [1.0, 2.0, 3.0]);
        assert_eq!(hit.normal, [0.0, 1.0, 0.0]);
    }

    #[test]
    fn test_raycast_query_builder() {
        let query = RaycastQuery::new()
            .with_max_distance(100.0)
            .with_layer_mask(0b1010)
            .first_hit()
            .with_back_faces();

        assert_eq!(query.max_distance, 100.0);
        assert_eq!(query.layer_mask, 0b1010);
        assert!(query.first_hit_only);
        assert!(query.hit_back_faces);
    }

    #[test]
    fn test_raycast_query_ignore() {
        let entity1 = Entity::from_bits(1);
        let entity2 = Entity::from_bits(2);

        let query = RaycastQuery::new()
            .ignore(entity1)
            .ignore(entity2);

        assert!(query.should_ignore(entity1));
        assert!(query.should_ignore(entity2));
        assert!(!query.should_ignore(Entity::from_bits(3)));
    }

    #[test]
    fn test_raycast_aabb() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let aabb = AABB::new(
            Vec3::new(-1.0, -1.0, 5.0),
            Vec3::new(1.0, 1.0, 7.0),
        );

        let hit = raycast_aabb(&ray, &aabb, 100.0);
        assert!(hit.is_some());
        assert!((hit.unwrap() - 5.0).abs() < 0.01);

        // Beyond max distance
        let miss = raycast_aabb(&ray, &aabb, 3.0);
        assert!(miss.is_none());
    }

    #[test]
    fn test_raycast_sphere() {
        let ray = Ray::new(Vec3::ZERO, Vec3::Z);
        let center = Vec3::new(0.0, 0.0, 10.0);

        let hit = raycast_sphere(&ray, center, 2.0, 100.0);
        assert!(hit.is_some());
        assert!((hit.unwrap() - 8.0).abs() < 0.01);
    }

    #[test]
    fn test_raycast_triangles() {
        let ray = Ray::new(Vec3::new(0.0, 0.0, -5.0), Vec3::Z);

        // Triangle with CCW winding from -Z side (normal points -Z, front faces ray)
        let positions = [
            [-1.0, -1.0, 0.0],
            [0.0, 1.0, 0.0],
            [1.0, -1.0, 0.0],
        ];
        let indices = [[0, 1, 2]];

        let hit = raycast_triangles(&ray, &positions, &indices, true, 100.0);
        assert!(hit.is_some());

        let (dist, bary, tri_idx) = hit.unwrap();
        assert!((dist - 5.0).abs() < 0.01);
        assert_eq!(tri_idx, 0);

        // Barycentric should sum to 1
        let sum: f32 = bary.iter().sum();
        assert!((sum - 1.0).abs() < 0.01);
    }

    #[test]
    fn test_screen_to_ray_center() {
        // Simple orthographic-like setup
        let view = Mat4::look_at(
            Vec3::new(0.0, 0.0, 10.0),
            Vec3::ZERO,
            Vec3::Y,
        );
        let proj = Mat4::perspective(60.0_f32.to_radians(), 1.0, 0.1, 100.0);

        // Center of screen
        let ray = screen_to_ray([500.0, 500.0], [1000.0, 1000.0], &view, &proj);

        // Ray should point roughly toward negative Z (toward origin)
        assert!(ray.direction.z < 0.0);
    }

    #[test]
    fn test_calculate_triangle_uv() {
        let uvs = [
            [0.0, 0.0],
            [1.0, 0.0],
            [0.5, 1.0],
        ];
        let indices = [0, 1, 2];

        // At first vertex
        let uv = calculate_triangle_uv(&uvs, &indices, [1.0, 0.0, 0.0]);
        assert!((uv[0] - 0.0).abs() < 0.01);
        assert!((uv[1] - 0.0).abs() < 0.01);

        // At center
        let uv = calculate_triangle_uv(&uvs, &indices, [0.33, 0.33, 0.34]);
        assert!((uv[0] - 0.5).abs() < 0.1);
    }

    #[test]
    fn test_raycast_system_state_serialization() {
        let mut state = RaycastSystemState::new();
        state.last_hits = vec![(42, 10.0), (43, 15.0)];

        let json = serde_json::to_string(&state).unwrap();
        let restored: RaycastSystemState = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.last_hits.len(), 2);
        assert_eq!(restored.last_hits[0].0, 42);
    }

    #[test]
    fn test_raycast_query_serialization() {
        let query = RaycastQuery::new()
            .with_max_distance(50.0)
            .with_layer_mask(0xFF)
            .require_tag("selectable")
            .exclude_tag("ui");

        let json = serde_json::to_string(&query).unwrap();
        let restored: RaycastQuery = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.max_distance, 50.0);
        assert_eq!(restored.layer_mask, 0xFF);
        assert_eq!(restored.require_tags, vec!["selectable"]);
        assert_eq!(restored.exclude_tags, vec!["ui"]);
        // ignore_entities should be skipped
        assert!(restored.ignore_entities.is_empty());
    }
}
