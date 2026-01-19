//! Camera Manager System
//!
//! Provides camera management including:
//! - Tracking active cameras
//! - Main camera selection
//! - Camera render data extraction
//! - Frame-boundary camera updates
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::camera_system::CameraManager;
//!
//! let mut manager = CameraManager::new();
//! manager.update(&world);
//!
//! // Get main camera data for rendering
//! if let Some(data) = manager.get_main_camera_data(&world, (1920, 1080)) {
//!     render_with_camera(data);
//! }
//! ```

use crate::camera::{Camera, CameraAnimation, Projection, Viewport, IDENTITY_MATRIX};
use crate::entity::Entity;
use crate::hierarchy::GlobalTransform;
use crate::world::World;
use serde::{Deserialize, Serialize};

#[cfg(feature = "hot-reload")]
use void_core::hot_reload::{HotReloadError, HotReloadable};

/// Manages active cameras and provides rendering views
#[derive(Clone, Debug, Default)]
pub struct CameraManager {
    /// The main camera entity
    main_camera: Option<Entity>,

    /// All active cameras sorted by priority
    active_cameras: Vec<Entity>,

    /// Cameras with pending animations
    animating_cameras: Vec<Entity>,
}

impl CameraManager {
    /// Create a new camera manager
    pub fn new() -> Self {
        Self {
            main_camera: None,
            active_cameras: Vec::new(),
            animating_cameras: Vec::new(),
        }
    }

    /// Update the camera list from world state
    pub fn update(&mut self, world: &World) {
        self.active_cameras.clear();
        self.main_camera = None;

        // Get the Camera component ID
        let camera_id = match world.component_id::<Camera>() {
            Some(id) => id,
            None => return, // No cameras registered
        };

        // Find all entities with Camera component
        for archetype in world.archetypes().iter() {
            if !archetype.has_component(camera_id) {
                continue;
            }

            for entity in archetype.entities() {
                if let Some(camera) = world.get_component::<Camera>(*entity) {
                    if !camera.active {
                        continue;
                    }

                    self.active_cameras.push(*entity);

                    if camera.is_main && self.main_camera.is_none() {
                        self.main_camera = Some(*entity);
                    }
                }
            }
        }

        // Sort by priority (lower priority renders first)
        let world_ref = world;
        self.active_cameras.sort_by_key(|e| {
            world_ref
                .get_component::<Camera>(*e)
                .map(|c| c.priority)
                .unwrap_or(0)
        });

        // If no main camera designated, use first active camera
        if self.main_camera.is_none() && !self.active_cameras.is_empty() {
            self.main_camera = Some(self.active_cameras[0]);
        }
    }

    /// Get the main camera entity
    pub fn main_camera(&self) -> Option<Entity> {
        self.main_camera
    }

    /// Get all active cameras in render order
    pub fn active_cameras(&self) -> &[Entity] {
        &self.active_cameras
    }

    /// Get number of active cameras
    pub fn camera_count(&self) -> usize {
        self.active_cameras.len()
    }

    /// Set the main camera
    pub fn set_main_camera(&mut self, world: &mut World, entity: Entity) {
        // Clear previous main camera flag
        if let Some(old_main) = self.main_camera {
            if let Some(mut old_cam) = world.get_component_mut::<Camera>(old_main) {
                old_cam.is_main = false;
            }
        }

        // Set new main camera flag
        if let Some(mut new_cam) = world.get_component_mut::<Camera>(entity) {
            new_cam.is_main = true;
            self.main_camera = Some(entity);
        }
    }

    /// Get camera render data for a specific camera
    pub fn get_camera_data(
        &self,
        world: &World,
        entity: Entity,
        viewport_size: (u32, u32),
    ) -> Option<CameraRenderData> {
        let camera = world.get_component::<Camera>(entity)?;
        let transform = world
            .get_component::<GlobalTransform>(entity)
            .cloned()
            .unwrap_or_default();

        let viewport_aspect = if viewport_size.1 > 0 {
            viewport_size.0 as f32 / viewport_size.1 as f32
        } else {
            1.0
        };

        // Calculate effective aspect based on camera viewport
        let effective_aspect = if let Some(ref vp) = camera.viewport {
            vp.aspect_ratio(viewport_size.0, viewport_size.1)
        } else {
            viewport_aspect
        };

        let view_matrix = camera.view_matrix(&transform);
        let projection_matrix = camera.projection_matrix(effective_aspect);

        Some(CameraRenderData {
            entity,
            view_matrix,
            projection_matrix,
            view_projection: crate::camera::multiply_matrices(&projection_matrix, &view_matrix),
            position: transform.translation(),
            forward: transform.forward(),
            up: transform.up(),
            right: transform.right(),
            near: camera.near,
            far: camera.far,
            fov: match &camera.projection {
                Projection::Perspective { fov_y } => *fov_y,
                Projection::Orthographic { .. } => 0.0,
            },
            clear_color: camera.clear_color,
            viewport: camera.viewport.clone(),
            layer_mask: camera.layer_mask,
            is_main: camera.is_main,
            priority: camera.priority,
            render_target: camera.render_target.clone(),
        })
    }

    /// Get render data for the main camera
    pub fn get_main_camera_data(
        &self,
        world: &World,
        viewport_size: (u32, u32),
    ) -> Option<CameraRenderData> {
        self.main_camera
            .and_then(|e| self.get_camera_data(world, e, viewport_size))
    }

    /// Get render data for all active cameras
    pub fn get_all_camera_data(
        &self,
        world: &World,
        viewport_size: (u32, u32),
    ) -> Vec<CameraRenderData> {
        self.active_cameras
            .iter()
            .filter_map(|&e| self.get_camera_data(world, e, viewport_size))
            .collect()
    }

    /// Update camera animations
    pub fn update_animations(&mut self, world: &mut World, delta_time: f32) {
        let mut completed = Vec::new();

        for &entity in &self.animating_cameras {
            // Get the camera and check if it has an animation
            let animation_complete = if let Some(camera) = world.get_component_mut::<Camera>(entity) {
                if let Some(mut anim) = camera.animation.take() {
                    // Update the animation (returns true if complete)
                    let is_complete = anim.update(delta_time, camera);

                    if !is_complete {
                        // Put animation back
                        camera.animation = Some(anim);
                    }

                    is_complete
                } else {
                    true // No animation, mark as complete
                }
            } else {
                true // Camera missing, mark as complete
            };

            if animation_complete {
                completed.push(entity);
            }
        }

        // Remove completed animations from tracking list
        for entity in completed {
            self.animating_cameras.retain(|&e| e != entity);
        }
    }

    /// Start an animation on a camera
    pub fn start_animation(&mut self, world: &mut World, entity: Entity, animation: CameraAnimation) {
        if let Some(camera) = world.get_component_mut::<Camera>(entity) {
            camera.animation = Some(animation);
        }
        if !self.animating_cameras.contains(&entity) {
            self.animating_cameras.push(entity);
        }
    }

    /// Check if a camera is animating
    pub fn is_animating(&self, entity: Entity) -> bool {
        self.animating_cameras.contains(&entity)
    }

    /// Clear all state (for hot-reload)
    pub fn clear(&mut self) {
        self.main_camera = None;
        self.active_cameras.clear();
        self.animating_cameras.clear();
    }
}

#[cfg(feature = "hot-reload")]
impl HotReloadable for CameraManager {
    fn type_name() -> &'static str {
        "CameraManager"
    }

    fn snapshot(&self) -> Result<Vec<u8>, HotReloadError> {
        // Only persist main camera identity, rest is rebuilt
        let main_bits = self.main_camera.map(|e| e.to_bits());
        bincode::serialize(&main_bits)
            .map_err(|e| HotReloadError::SerializationFailed(e.to_string()))
    }

    fn restore(data: &[u8]) -> Result<Self, HotReloadError> {
        let main_bits: Option<u64> = bincode::deserialize(data)
            .map_err(|e| HotReloadError::DeserializationFailed(e.to_string()))?;

        Ok(Self {
            main_camera: main_bits.map(Entity::from_bits),
            active_cameras: Vec::new(), // Rebuilt on update()
            animating_cameras: Vec::new(),
        })
    }
}

/// Camera data ready for GPU upload
#[derive(Clone, Debug)]
pub struct CameraRenderData {
    /// The camera entity
    pub entity: Entity,

    /// View matrix (world to camera space)
    pub view_matrix: [[f32; 4]; 4],

    /// Projection matrix (camera to clip space)
    pub projection_matrix: [[f32; 4]; 4],

    /// Combined view-projection matrix
    pub view_projection: [[f32; 4]; 4],

    /// Camera world position
    pub position: [f32; 3],

    /// Camera forward direction (normalized)
    pub forward: [f32; 3],

    /// Camera up direction (normalized)
    pub up: [f32; 3],

    /// Camera right direction (normalized)
    pub right: [f32; 3],

    /// Near clip plane distance
    pub near: f32,

    /// Far clip plane distance
    pub far: f32,

    /// Field of view in radians (0 for orthographic)
    pub fov: f32,

    /// Clear color (RGBA)
    pub clear_color: [f32; 4],

    /// Viewport rectangle (None = full screen)
    pub viewport: Option<Viewport>,

    /// Layer mask for filtering
    pub layer_mask: Option<u32>,

    /// Is this the main camera?
    pub is_main: bool,

    /// Render priority
    pub priority: i32,

    /// Target render texture (None = main framebuffer)
    pub render_target: Option<String>,
}

impl Default for CameraRenderData {
    fn default() -> Self {
        Self::fallback(Entity::null())
    }
}

impl CameraRenderData {
    /// Create fallback camera data (used on errors)
    pub fn fallback(entity: Entity) -> Self {
        let fov = core::f32::consts::FRAC_PI_3;
        let proj = crate::camera::perspective_matrix(fov, 1.0, 0.1, 1000.0);

        Self {
            entity,
            view_matrix: IDENTITY_MATRIX,
            projection_matrix: proj,
            view_projection: proj,
            position: [0.0, 0.0, 0.0],
            forward: [0.0, 0.0, -1.0],
            up: [0.0, 1.0, 0.0],
            right: [1.0, 0.0, 0.0],
            near: 0.1,
            far: 1000.0,
            fov,
            clear_color: [0.0, 0.0, 0.0, 1.0],
            viewport: None,
            layer_mask: None,
            is_main: false,
            priority: 0,
            render_target: None,
        }
    }

    /// Get the inverse view matrix (camera to world)
    pub fn inverse_view_matrix(&self) -> [[f32; 4]; 4] {
        crate::camera::invert_transform_matrix(&self.view_matrix)
    }

    /// Transform a world position to screen coordinates (0-1 range)
    pub fn world_to_screen(&self, world_pos: [f32; 3]) -> Option<[f32; 2]> {
        // Transform to clip space
        let pos = [world_pos[0], world_pos[1], world_pos[2], 1.0];

        let clip = transform_point(&self.view_projection, pos);

        // Check if behind camera
        if clip[3] <= 0.0 {
            return None;
        }

        // Perspective divide
        let ndc_x = clip[0] / clip[3];
        let ndc_y = clip[1] / clip[3];

        // NDC to screen (0-1)
        let screen_x = (ndc_x + 1.0) * 0.5;
        let screen_y = (1.0 - ndc_y) * 0.5; // Y flipped

        Some([screen_x, screen_y])
    }

    /// Transform screen coordinates (0-1 range) to world ray
    pub fn screen_to_ray(&self, screen_pos: [f32; 2]) -> ([f32; 3], [f32; 3]) {
        // Screen to NDC
        let ndc_x = screen_pos[0] * 2.0 - 1.0;
        let ndc_y = 1.0 - screen_pos[1] * 2.0; // Y flipped

        // Near point in NDC
        let near_ndc = [ndc_x, ndc_y, 0.0, 1.0];
        // Far point in NDC
        let far_ndc = [ndc_x, ndc_y, 1.0, 1.0];

        // Get inverse view-projection
        let inv_vp = invert_matrix(&self.view_projection);

        // Transform to world
        let near_world = transform_point(&inv_vp, near_ndc);
        let far_world = transform_point(&inv_vp, far_ndc);

        let near_pos = [
            near_world[0] / near_world[3],
            near_world[1] / near_world[3],
            near_world[2] / near_world[3],
        ];

        let far_pos = [
            far_world[0] / far_world[3],
            far_world[1] / far_world[3],
            far_world[2] / far_world[3],
        ];

        // Ray direction
        let dir = normalize([
            far_pos[0] - near_pos[0],
            far_pos[1] - near_pos[1],
            far_pos[2] - near_pos[2],
        ]);

        (near_pos, dir)
    }
}

/// Queue for frame-boundary camera updates
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct CameraUpdateQueue {
    /// Pending main camera change
    pending_main_camera: Option<u64>, // Entity bits

    /// Pending camera activations (entity bits, active state)
    pending_activations: Vec<(u64, bool)>,

    /// Pending projection changes (entity bits, fov in radians, is_perspective)
    pending_projections: Vec<(u64, f32, bool)>,

    /// Pending FOV animations (entity bits, target fov radians, duration)
    pending_animations: Vec<(u64, f32, f32)>,
}

impl CameraUpdateQueue {
    /// Create a new empty queue
    pub fn new() -> Self {
        Self::default()
    }

    /// Queue a main camera change
    pub fn set_main_camera(&mut self, entity: Entity) {
        self.pending_main_camera = Some(entity.to_bits());
    }

    /// Queue a camera activation change
    pub fn set_active(&mut self, entity: Entity, active: bool) {
        // Remove any existing pending change for this entity
        let bits = entity.to_bits();
        self.pending_activations.retain(|(e, _)| *e != bits);
        self.pending_activations.push((bits, active));
    }

    /// Queue a projection change
    pub fn set_perspective(&mut self, entity: Entity, fov_degrees: f32) {
        let bits = entity.to_bits();
        self.pending_projections.retain(|(e, _, _)| *e != bits);
        self.pending_projections
            .push((bits, fov_degrees.to_radians(), true));
    }

    /// Queue an orthographic projection change
    pub fn set_orthographic(&mut self, entity: Entity, height: f32) {
        let bits = entity.to_bits();
        self.pending_projections.retain(|(e, _, _)| *e != bits);
        self.pending_projections.push((bits, height, false));
    }

    /// Queue an FOV animation
    pub fn animate_fov(&mut self, entity: Entity, target_fov_degrees: f32, duration: f32) {
        let bits = entity.to_bits();
        self.pending_animations.retain(|(e, _, _)| *e != bits);
        self.pending_animations
            .push((bits, target_fov_degrees.to_radians(), duration));
    }

    /// Apply all pending updates at frame boundary
    pub fn apply_at_frame_boundary(&mut self, world: &mut World, manager: &mut CameraManager) {
        // Apply main camera change
        if let Some(bits) = self.pending_main_camera.take() {
            let entity = Entity::from_bits(bits);
            manager.set_main_camera(world, entity);
        }

        // Apply activation changes
        for (bits, active) in self.pending_activations.drain(..) {
            let entity = Entity::from_bits(bits);
            if let Some(mut camera) = world.get_component_mut::<Camera>(entity) {
                camera.active = active;
            }
        }

        // Apply projection changes
        for (bits, value, is_perspective) in self.pending_projections.drain(..) {
            let entity = Entity::from_bits(bits);
            if let Some(mut camera) = world.get_component_mut::<Camera>(entity) {
                camera.projection = if is_perspective {
                    Projection::Perspective { fov_y: value }
                } else {
                    Projection::Orthographic { height: value }
                };
                camera.invalidate_cache();
            }
        }

        // Start animations
        for (bits, target_fov, duration) in self.pending_animations.drain(..) {
            let entity = Entity::from_bits(bits);
            let anim = CameraAnimation::fov_transition(target_fov, duration);
            manager.start_animation(world, entity, anim);
        }

        // Refresh camera list
        manager.update(world);
    }

    /// Check if there are pending updates
    pub fn has_pending(&self) -> bool {
        self.pending_main_camera.is_some()
            || !self.pending_activations.is_empty()
            || !self.pending_projections.is_empty()
            || !self.pending_animations.is_empty()
    }

    /// Clear all pending updates
    pub fn clear(&mut self) {
        self.pending_main_camera = None;
        self.pending_activations.clear();
        self.pending_projections.clear();
        self.pending_animations.clear();
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Transform a point by a 4x4 matrix
fn transform_point(m: &[[f32; 4]; 4], p: [f32; 4]) -> [f32; 4] {
    [
        m[0][0] * p[0] + m[1][0] * p[1] + m[2][0] * p[2] + m[3][0] * p[3],
        m[0][1] * p[0] + m[1][1] * p[1] + m[2][1] * p[2] + m[3][1] * p[3],
        m[0][2] * p[0] + m[1][2] * p[1] + m[2][2] * p[2] + m[3][2] * p[3],
        m[0][3] * p[0] + m[1][3] * p[1] + m[2][3] * p[2] + m[3][3] * p[3],
    ]
}

/// Normalize a vector
fn normalize(v: [f32; 3]) -> [f32; 3] {
    let len = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if len > 0.0001 {
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, 0.0, 1.0]
    }
}

/// Invert a 4x4 matrix (general case)
fn invert_matrix(m: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    // Calculate cofactors
    let mut inv = [[0.0f32; 4]; 4];

    inv[0][0] = m[1][1] * m[2][2] * m[3][3] - m[1][1] * m[2][3] * m[3][2]
        - m[2][1] * m[1][2] * m[3][3]
        + m[2][1] * m[1][3] * m[3][2]
        + m[3][1] * m[1][2] * m[2][3]
        - m[3][1] * m[1][3] * m[2][2];

    inv[1][0] = -m[1][0] * m[2][2] * m[3][3]
        + m[1][0] * m[2][3] * m[3][2]
        + m[2][0] * m[1][2] * m[3][3]
        - m[2][0] * m[1][3] * m[3][2]
        - m[3][0] * m[1][2] * m[2][3]
        + m[3][0] * m[1][3] * m[2][2];

    inv[2][0] = m[1][0] * m[2][1] * m[3][3] - m[1][0] * m[2][3] * m[3][1]
        - m[2][0] * m[1][1] * m[3][3]
        + m[2][0] * m[1][3] * m[3][1]
        + m[3][0] * m[1][1] * m[2][3]
        - m[3][0] * m[1][3] * m[2][1];

    inv[3][0] = -m[1][0] * m[2][1] * m[3][2]
        + m[1][0] * m[2][2] * m[3][1]
        + m[2][0] * m[1][1] * m[3][2]
        - m[2][0] * m[1][2] * m[3][1]
        - m[3][0] * m[1][1] * m[2][2]
        + m[3][0] * m[1][2] * m[2][1];

    inv[0][1] = -m[0][1] * m[2][2] * m[3][3]
        + m[0][1] * m[2][3] * m[3][2]
        + m[2][1] * m[0][2] * m[3][3]
        - m[2][1] * m[0][3] * m[3][2]
        - m[3][1] * m[0][2] * m[2][3]
        + m[3][1] * m[0][3] * m[2][2];

    inv[1][1] = m[0][0] * m[2][2] * m[3][3] - m[0][0] * m[2][3] * m[3][2]
        - m[2][0] * m[0][2] * m[3][3]
        + m[2][0] * m[0][3] * m[3][2]
        + m[3][0] * m[0][2] * m[2][3]
        - m[3][0] * m[0][3] * m[2][2];

    inv[2][1] = -m[0][0] * m[2][1] * m[3][3]
        + m[0][0] * m[2][3] * m[3][1]
        + m[2][0] * m[0][1] * m[3][3]
        - m[2][0] * m[0][3] * m[3][1]
        - m[3][0] * m[0][1] * m[2][3]
        + m[3][0] * m[0][3] * m[2][1];

    inv[3][1] = m[0][0] * m[2][1] * m[3][2] - m[0][0] * m[2][2] * m[3][1]
        - m[2][0] * m[0][1] * m[3][2]
        + m[2][0] * m[0][2] * m[3][1]
        + m[3][0] * m[0][1] * m[2][2]
        - m[3][0] * m[0][2] * m[2][1];

    inv[0][2] = m[0][1] * m[1][2] * m[3][3] - m[0][1] * m[1][3] * m[3][2]
        - m[1][1] * m[0][2] * m[3][3]
        + m[1][1] * m[0][3] * m[3][2]
        + m[3][1] * m[0][2] * m[1][3]
        - m[3][1] * m[0][3] * m[1][2];

    inv[1][2] = -m[0][0] * m[1][2] * m[3][3]
        + m[0][0] * m[1][3] * m[3][2]
        + m[1][0] * m[0][2] * m[3][3]
        - m[1][0] * m[0][3] * m[3][2]
        - m[3][0] * m[0][2] * m[1][3]
        + m[3][0] * m[0][3] * m[1][2];

    inv[2][2] = m[0][0] * m[1][1] * m[3][3] - m[0][0] * m[1][3] * m[3][1]
        - m[1][0] * m[0][1] * m[3][3]
        + m[1][0] * m[0][3] * m[3][1]
        + m[3][0] * m[0][1] * m[1][3]
        - m[3][0] * m[0][3] * m[1][1];

    inv[3][2] = -m[0][0] * m[1][1] * m[3][2]
        + m[0][0] * m[1][2] * m[3][1]
        + m[1][0] * m[0][1] * m[3][2]
        - m[1][0] * m[0][2] * m[3][1]
        - m[3][0] * m[0][1] * m[1][2]
        + m[3][0] * m[0][2] * m[1][1];

    inv[0][3] = -m[0][1] * m[1][2] * m[2][3]
        + m[0][1] * m[1][3] * m[2][2]
        + m[1][1] * m[0][2] * m[2][3]
        - m[1][1] * m[0][3] * m[2][2]
        - m[2][1] * m[0][2] * m[1][3]
        + m[2][1] * m[0][3] * m[1][2];

    inv[1][3] = m[0][0] * m[1][2] * m[2][3] - m[0][0] * m[1][3] * m[2][2]
        - m[1][0] * m[0][2] * m[2][3]
        + m[1][0] * m[0][3] * m[2][2]
        + m[2][0] * m[0][2] * m[1][3]
        - m[2][0] * m[0][3] * m[1][2];

    inv[2][3] = -m[0][0] * m[1][1] * m[2][3]
        + m[0][0] * m[1][3] * m[2][1]
        + m[1][0] * m[0][1] * m[2][3]
        - m[1][0] * m[0][3] * m[2][1]
        - m[2][0] * m[0][1] * m[1][3]
        + m[2][0] * m[0][3] * m[1][1];

    inv[3][3] = m[0][0] * m[1][1] * m[2][2] - m[0][0] * m[1][2] * m[2][1]
        - m[1][0] * m[0][1] * m[2][2]
        + m[1][0] * m[0][2] * m[2][1]
        + m[2][0] * m[0][1] * m[1][2]
        - m[2][0] * m[0][2] * m[1][1];

    let det = m[0][0] * inv[0][0] + m[0][1] * inv[1][0] + m[0][2] * inv[2][0] + m[0][3] * inv[3][0];

    if det.abs() < 0.0001 {
        return IDENTITY_MATRIX;
    }

    let det_inv = 1.0 / det;
    for i in 0..4 {
        for j in 0..4 {
            inv[i][j] *= det_inv;
        }
    }

    inv
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::hierarchy::LocalTransform;

    fn setup_world_with_cameras() -> World {
        let mut world = World::new();

        // Register components
        world.register_component::<Camera>();
        world.register_component::<LocalTransform>();
        world.register_component::<GlobalTransform>();

        world
    }

    #[test]
    fn test_camera_manager_update() {
        let mut world = setup_world_with_cameras();
        let mut manager = CameraManager::new();

        // Spawn cameras
        let cam1 = world.spawn();
        world.add_component(cam1, Camera::perspective(60.0, 0.1, 100.0).with_main(true));

        let cam2 = world.spawn();
        world.add_component(
            cam2,
            Camera::perspective(45.0, 0.1, 100.0).with_priority(10),
        );

        manager.update(&world);

        assert_eq!(manager.main_camera(), Some(cam1));
        assert_eq!(manager.camera_count(), 2);
    }

    #[test]
    fn test_camera_switching() {
        let mut world = setup_world_with_cameras();
        let mut manager = CameraManager::new();

        let cam1 = world.spawn();
        world.add_component(cam1, Camera::perspective(60.0, 0.1, 100.0).with_main(true));

        let cam2 = world.spawn();
        world.add_component(cam2, Camera::perspective(45.0, 0.1, 100.0));

        manager.update(&world);
        assert_eq!(manager.main_camera(), Some(cam1));

        // Switch main camera
        manager.set_main_camera(&mut world, cam2);
        manager.update(&world);

        assert_eq!(manager.main_camera(), Some(cam2));

        // Verify flags updated
        let cam1_data = world.get_component::<Camera>(cam1).unwrap();
        let cam2_data = world.get_component::<Camera>(cam2).unwrap();
        assert!(!cam1_data.is_main);
        assert!(cam2_data.is_main);
    }

    #[test]
    fn test_inactive_cameras_filtered() {
        let mut world = setup_world_with_cameras();
        let mut manager = CameraManager::new();

        let cam1 = world.spawn();
        world.add_component(cam1, Camera::perspective(60.0, 0.1, 100.0));

        let cam2 = world.spawn();
        let mut inactive_cam = Camera::perspective(45.0, 0.1, 100.0);
        inactive_cam.active = false;
        world.add_component(cam2, inactive_cam);

        manager.update(&world);

        assert_eq!(manager.camera_count(), 1);
        assert!(manager.active_cameras().contains(&cam1));
        assert!(!manager.active_cameras().contains(&cam2));
    }

    #[test]
    fn test_camera_render_data() {
        let mut world = setup_world_with_cameras();
        let mut manager = CameraManager::new();

        let cam = world.spawn();
        world.add_component(
            cam,
            Camera::perspective(60.0, 0.1, 100.0)
                .with_main(true)
                .with_clear_color([1.0, 0.0, 0.0, 1.0]),
        );
        world.add_component(cam, GlobalTransform::default());

        manager.update(&world);

        let data = manager.get_camera_data(&world, cam, (1920, 1080)).unwrap();

        assert!(data.is_main);
        assert_eq!(data.clear_color, [1.0, 0.0, 0.0, 1.0]);
        assert!((data.near - 0.1).abs() < 0.001);
        assert!((data.far - 100.0).abs() < 0.001);
    }

    #[test]
    fn test_camera_update_queue() {
        let mut world = setup_world_with_cameras();
        let mut manager = CameraManager::new();
        let mut queue = CameraUpdateQueue::new();

        let cam1 = world.spawn();
        world.add_component(cam1, Camera::perspective(60.0, 0.1, 100.0).with_main(true));

        let cam2 = world.spawn();
        world.add_component(cam2, Camera::perspective(45.0, 0.1, 100.0));

        manager.update(&world);

        // Queue main camera switch
        queue.set_main_camera(cam2);
        assert!(queue.has_pending());

        // Apply at frame boundary
        queue.apply_at_frame_boundary(&mut world, &mut manager);

        assert!(!queue.has_pending());
        assert_eq!(manager.main_camera(), Some(cam2));
    }

    #[test]
    fn test_camera_render_data_fallback() {
        let entity = Entity::null();
        let data = CameraRenderData::fallback(entity);

        // Should have valid matrices
        assert!(data.projection_matrix[0][0] > 0.0);
        assert!((data.near - 0.1).abs() < 0.001);
        assert!((data.far - 1000.0).abs() < 0.001);
    }

    #[test]
    fn test_priority_ordering() {
        let mut world = setup_world_with_cameras();
        let mut manager = CameraManager::new();

        let cam1 = world.spawn();
        world.add_component(cam1, Camera::perspective(60.0, 0.1, 100.0).with_priority(10));

        let cam2 = world.spawn();
        world.add_component(cam2, Camera::perspective(45.0, 0.1, 100.0).with_priority(5));

        let cam3 = world.spawn();
        world.add_component(cam3, Camera::perspective(30.0, 0.1, 100.0).with_priority(15));

        manager.update(&world);

        let cameras = manager.active_cameras();
        assert_eq!(cameras.len(), 3);

        // Should be sorted by priority (lower first)
        let p0 = world.get_component::<Camera>(cameras[0]).unwrap().priority;
        let p1 = world.get_component::<Camera>(cameras[1]).unwrap().priority;
        let p2 = world.get_component::<Camera>(cameras[2]).unwrap().priority;

        assert!(p0 <= p1);
        assert!(p1 <= p2);
    }
}
