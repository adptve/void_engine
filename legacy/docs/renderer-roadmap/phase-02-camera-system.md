# Phase 2: Camera System

## Status: Not Started

## User Story

> As a scene author, I want to define and control cameras as first-class scene entities.

## Requirements Checklist

- [ ] Introduce `camera` entities
- [ ] Support camera types: Perspective, Orthographic
- [ ] Configurable properties: FOV, near/far clip, aspect ratio override
- [ ] Support multiple cameras per scene
- [ ] Allow runtime camera switching
- [ ] Allow camera animation (position, rotation, FOV)
- [ ] Allow cameras to be parented to entities

## Current State Analysis

### Existing Implementation (void_render/src/camera.rs)

```rust
pub struct Camera {
    pub position: Vec3,
    pub target: Vec3,
    pub up: Vec3,
    pub projection: Projection,
    pub control_mode: ControlMode,
    // ... control settings
}

pub enum Projection {
    Perspective { fov: f32, near: f32, far: f32 },
    Orthographic { left: f32, right: f32, bottom: f32, top: f32, near: f32, far: f32 },
}
```

### Existing ECS Component (void_ecs/src/render_components.rs)

```rust
pub struct Camera {
    pub fov: f32,
    pub near: f32,
    pub far: f32,
    pub is_main: bool,
    pub clear_color: [f32; 4],
    pub priority: i32,
    pub target_layer: Option<u32>,
}
```

### Gaps
1. No orthographic support in ECS component
2. No aspect ratio override
3. No camera switching API
4. Not integrated with hierarchy system
5. No camera animation support
6. Duplicate camera definitions (render vs ECS)

## Implementation Specification

### 1. Unified Camera Component

```rust
// crates/void_ecs/src/components/camera.rs (REFACTOR)

use crate::Entity;

/// Camera projection mode
#[derive(Clone, Debug)]
pub enum Projection {
    Perspective {
        /// Vertical field of view in radians
        fov_y: f32,
    },
    Orthographic {
        /// Orthographic height (width derived from aspect)
        height: f32,
    },
}

impl Default for Projection {
    fn default() -> Self {
        Projection::Perspective { fov_y: std::f32::consts::FRAC_PI_3 }  // 60 degrees
    }
}

/// Camera component for rendering viewpoints
#[derive(Clone, Debug)]
pub struct Camera {
    /// Projection mode
    pub projection: Projection,

    /// Near clipping plane distance
    pub near: f32,

    /// Far clipping plane distance
    pub far: f32,

    /// Aspect ratio override (None = use viewport)
    pub aspect_ratio: Option<f32>,

    /// Clear color for this camera's render target
    pub clear_color: [f32; 4],

    /// Render priority (higher = rendered later, on top)
    pub priority: i32,

    /// Target render layer mask (None = render all layers)
    pub layer_mask: Option<u32>,

    /// Viewport rectangle (0-1 normalized, None = full screen)
    pub viewport: Option<Viewport>,

    /// Is this the main camera?
    pub is_main: bool,

    /// Is this camera active?
    pub active: bool,
}

impl Default for Camera {
    fn default() -> Self {
        Self {
            projection: Projection::default(),
            near: 0.1,
            far: 1000.0,
            aspect_ratio: None,
            clear_color: [0.1, 0.1, 0.1, 1.0],
            priority: 0,
            layer_mask: None,
            viewport: None,
            is_main: false,
            active: true,
        }
    }
}

/// Normalized viewport rectangle
#[derive(Clone, Debug)]
pub struct Viewport {
    pub x: f32,      // 0.0 - 1.0
    pub y: f32,      // 0.0 - 1.0
    pub width: f32,  // 0.0 - 1.0
    pub height: f32, // 0.0 - 1.0
}

impl Camera {
    /// Create perspective camera with FOV in degrees
    pub fn perspective(fov_degrees: f32, near: f32, far: f32) -> Self {
        Self {
            projection: Projection::Perspective {
                fov_y: fov_degrees.to_radians(),
            },
            near,
            far,
            ..Default::default()
        }
    }

    /// Create orthographic camera
    pub fn orthographic(height: f32, near: f32, far: f32) -> Self {
        Self {
            projection: Projection::Orthographic { height },
            near,
            far,
            ..Default::default()
        }
    }

    /// Compute projection matrix
    pub fn projection_matrix(&self, aspect: f32) -> [[f32; 4]; 4] {
        let aspect = self.aspect_ratio.unwrap_or(aspect);

        match &self.projection {
            Projection::Perspective { fov_y } => {
                perspective_matrix(*fov_y, aspect, self.near, self.far)
            }
            Projection::Orthographic { height } => {
                let half_height = height / 2.0;
                let half_width = half_height * aspect;
                orthographic_matrix(
                    -half_width, half_width,
                    -half_height, half_height,
                    self.near, self.far
                )
            }
        }
    }

    /// Compute view matrix from GlobalTransform
    pub fn view_matrix(&self, global_transform: &GlobalTransform) -> [[f32; 4]; 4] {
        // Invert the camera's world transform
        invert_transform_matrix(&global_transform.matrix)
    }
}
```

### 2. Camera Manager System

```rust
// crates/void_ecs/src/systems/camera_system.rs (NEW FILE)

use crate::{World, Entity, Query};
use crate::components::{Camera, GlobalTransform};

/// Manages active cameras and provides rendering views
pub struct CameraManager {
    main_camera: Option<Entity>,
    active_cameras: Vec<Entity>,
}

impl CameraManager {
    pub fn new() -> Self {
        Self {
            main_camera: None,
            active_cameras: Vec::new(),
        }
    }

    /// Update camera list from world state
    pub fn update(&mut self, world: &World) {
        self.active_cameras.clear();
        self.main_camera = None;

        // Query all active cameras
        for (entity, camera) in world.query::<&Camera>() {
            if !camera.active {
                continue;
            }

            self.active_cameras.push(entity);

            if camera.is_main {
                self.main_camera = Some(entity);
            }
        }

        // Sort by priority
        self.active_cameras.sort_by_key(|e| {
            world.get::<Camera>(*e).map(|c| c.priority).unwrap_or(0)
        });
    }

    /// Get the main camera entity
    pub fn main_camera(&self) -> Option<Entity> {
        self.main_camera
    }

    /// Get all active cameras in render order
    pub fn active_cameras(&self) -> &[Entity] {
        &self.active_cameras
    }

    /// Set the main camera
    pub fn set_main_camera(&mut self, world: &mut World, entity: Entity) {
        // Clear previous main camera
        if let Some(old) = self.main_camera {
            if let Some(mut cam) = world.get_mut::<Camera>(old) {
                cam.is_main = false;
            }
        }

        // Set new main camera
        if let Some(mut cam) = world.get_mut::<Camera>(entity) {
            cam.is_main = true;
            self.main_camera = Some(entity);
        }
    }

    /// Get camera render data for a specific camera
    pub fn get_camera_data(&self, world: &World, entity: Entity, viewport_size: (u32, u32))
        -> Option<CameraRenderData>
    {
        let camera = world.get::<Camera>(entity)?;
        let transform = world.get::<GlobalTransform>(entity)?;

        let aspect = viewport_size.0 as f32 / viewport_size.1 as f32;

        Some(CameraRenderData {
            entity,
            view_matrix: camera.view_matrix(&transform),
            projection_matrix: camera.projection_matrix(aspect),
            view_projection: multiply_matrices(
                &camera.projection_matrix(aspect),
                &camera.view_matrix(&transform)
            ),
            position: transform.translation(),
            near: camera.near,
            far: camera.far,
            clear_color: camera.clear_color,
            viewport: camera.viewport.clone(),
            layer_mask: camera.layer_mask,
        })
    }
}

/// Camera data ready for GPU upload
#[derive(Clone, Debug)]
pub struct CameraRenderData {
    pub entity: Entity,
    pub view_matrix: [[f32; 4]; 4],
    pub projection_matrix: [[f32; 4]; 4],
    pub view_projection: [[f32; 4]; 4],
    pub position: [f32; 3],
    pub near: f32,
    pub far: f32,
    pub clear_color: [f32; 4],
    pub viewport: Option<Viewport>,
    pub layer_mask: Option<u32>,
}
```

### 3. Camera Animation Support

```rust
// crates/void_ecs/src/components/camera.rs (additions)

/// Animatable camera properties
#[derive(Clone, Debug)]
pub struct CameraAnimation {
    /// Target FOV (for perspective)
    pub target_fov: Option<f32>,

    /// Transition duration in seconds
    pub duration: f32,

    /// Elapsed time
    pub elapsed: f32,

    /// Easing function
    pub easing: EasingFunction,

    /// Starting values (captured when animation starts)
    pub start_fov: f32,
}

#[derive(Clone, Debug, Default)]
pub enum EasingFunction {
    #[default]
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
}

impl CameraAnimation {
    pub fn fov_transition(target_fov: f32, duration: f32) -> Self {
        Self {
            target_fov: Some(target_fov),
            duration,
            elapsed: 0.0,
            easing: EasingFunction::EaseInOut,
            start_fov: 0.0,  // Set when animation starts
        }
    }

    pub fn update(&mut self, dt: f32, camera: &mut Camera) -> bool {
        self.elapsed += dt;
        let t = (self.elapsed / self.duration).min(1.0);
        let t = self.easing.apply(t);

        if let (Some(target), Projection::Perspective { fov_y }) =
            (self.target_fov, &mut camera.projection)
        {
            *fov_y = lerp(self.start_fov, target, t);
        }

        self.elapsed >= self.duration
    }
}
```

### 4. IR Patch Integration

```rust
// crates/void_ir/src/patch.rs (additions)

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum CameraPatch {
    SetMainCamera { entity: EntityId },
    SetActive { entity: EntityId, active: bool },
    SetProjection { entity: EntityId, projection: ProjectionData },
    AnimateFov { entity: EntityId, target: f32, duration: f32 },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ProjectionData {
    Perspective { fov_degrees: f32 },
    Orthographic { height: f32 },
}
```

### 5. Extraction Integration

```rust
// crates/void_render/src/extraction.rs (modifications)

impl SceneExtractor {
    pub fn extract_cameras(&self, world: &World, viewport_size: (u32, u32))
        -> Vec<CameraRenderData>
    {
        let mut cameras = Vec::new();

        for (entity, (camera, transform)) in
            world.query::<(&Camera, &GlobalTransform)>()
        {
            if !camera.active {
                continue;
            }

            cameras.push(CameraRenderData {
                entity,
                view_matrix: camera.view_matrix(&transform),
                projection_matrix: camera.projection_matrix(
                    viewport_size.0 as f32 / viewport_size.1 as f32
                ),
                // ... other fields
            });
        }

        // Sort by priority
        cameras.sort_by_key(|c| {
            world.get::<Camera>(c.entity).map(|c| c.priority).unwrap_or(0)
        });

        cameras
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/camera.rs` | CREATE | New unified camera component |
| `void_ecs/src/systems/camera_system.rs` | CREATE | Camera manager system |
| `void_ecs/src/render_components.rs` | MODIFY | Remove old Camera, re-export new |
| `void_render/src/camera.rs` | MODIFY | Remove duplicate, use ECS camera |
| `void_render/src/extraction.rs` | MODIFY | Extract camera data from ECS |
| `void_ir/src/patch.rs` | MODIFY | Add CameraPatch |
| `void_editor/src/panels/inspector.rs` | MODIFY | Camera component inspector UI |
| `void_runtime/src/scene_renderer.rs` | MODIFY | Use new camera system |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_perspective_projection() {
    let camera = Camera::perspective(60.0, 0.1, 100.0);
    let matrix = camera.projection_matrix(16.0 / 9.0);

    // Verify matrix properties
    assert!(matrix[0][0] > 0.0);  // Scale X
    assert!(matrix[1][1] > 0.0);  // Scale Y
    assert!(matrix[2][2] < 0.0);  // Depth mapping
}

#[test]
fn test_orthographic_projection() {
    let camera = Camera::orthographic(10.0, 0.1, 100.0);
    let matrix = camera.projection_matrix(2.0);

    // Orthographic has no perspective divide
    assert_eq!(matrix[3][3], 1.0);
}

#[test]
fn test_camera_parenting() {
    let mut world = World::new();

    let parent = world.spawn((
        LocalTransform { translation: [10.0, 0.0, 0.0], ..default() },
    ));

    let camera_entity = world.spawn((
        Camera::perspective(60.0, 0.1, 100.0),
        Parent { entity: parent },
        LocalTransform { translation: [5.0, 0.0, 0.0], ..default() },
    ));

    // After transform propagation
    TransformPropagationSystem::run(&mut world);

    let global = world.get::<GlobalTransform>(camera_entity).unwrap();
    assert_eq!(global.translation(), [15.0, 0.0, 0.0]);
}

#[test]
fn test_camera_switching() {
    let mut world = World::new();
    let mut manager = CameraManager::new();

    let cam1 = world.spawn((Camera { is_main: true, ..default() },));
    let cam2 = world.spawn((Camera { is_main: false, ..default() },));

    manager.update(&world);
    assert_eq!(manager.main_camera(), Some(cam1));

    manager.set_main_camera(&mut world, cam2);
    manager.update(&world);
    assert_eq!(manager.main_camera(), Some(cam2));
}

#[test]
fn test_fov_animation() {
    let mut camera = Camera::perspective(60.0, 0.1, 100.0);
    let mut anim = CameraAnimation::fov_transition(90.0_f32.to_radians(), 1.0);
    anim.start_fov = 60.0_f32.to_radians();

    // Half-way through
    anim.update(0.5, &mut camera);

    if let Projection::Perspective { fov_y } = camera.projection {
        assert!((fov_y - 75.0_f32.to_radians()).abs() < 0.1);
    }
}
```

### Integration Tests
```rust
#[test]
fn test_multi_camera_rendering() {
    // Set up scene with multiple cameras rendering to different viewports
}

#[test]
fn test_camera_render_order() {
    // Verify cameras render in priority order
}
```

## Performance Considerations

1. **View Frustum Caching**: Cache frustum planes for culling
2. **Matrix Updates**: Only recompute when transform changes
3. **Minimal Cameras**: Avoid excessive camera count (< 8 typically)

## Hot-Swap Support

### Serialization

```rust
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum Projection {
    Perspective { fov_y: f32 },
    Orthographic { height: f32 },
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Camera {
    pub projection: Projection,
    pub near: f32,
    pub far: f32,
    pub aspect_ratio: Option<f32>,
    pub clear_color: [f32; 4],
    pub priority: i32,
    pub layer_mask: Option<u32>,
    pub viewport: Option<Viewport>,
    pub is_main: bool,
    pub active: bool,

    // Transient - recomputed from GlobalTransform
    #[serde(skip)]
    pub cached_view_matrix: Option<[[f32; 4]; 4]>,

    #[serde(skip)]
    pub cached_projection_matrix: Option<[[f32; 4]; 4]>,
}
```

### HotReloadable Implementation

```rust
impl HotReloadable for Camera {
    fn snapshot(&self) -> Vec<u8> {
        bincode::serialize(self).unwrap()
    }

    fn restore(bytes: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(bytes).map_err(|e| HotReloadError::Deserialize(e.to_string()))
    }

    fn on_reload(&mut self) {
        // Invalidate cached matrices - will be recomputed
        self.cached_view_matrix = None;
        self.cached_projection_matrix = None;
    }
}

impl HotReloadable for CameraManager {
    fn on_reload(&mut self) {
        // Camera list will be rebuilt on next update()
        self.active_cameras.clear();
        self.main_camera = None;
    }
}
```

### Frame-Boundary Updates

```rust
pub struct CameraUpdateQueue {
    pending_main_camera: Option<Entity>,
    pending_activations: Vec<(Entity, bool)>,
    pending_projections: Vec<(Entity, Projection)>,
}

impl CameraUpdateQueue {
    pub fn apply_at_frame_boundary(&mut self, world: &mut World, manager: &mut CameraManager) {
        // Apply main camera switch
        if let Some(entity) = self.pending_main_camera.take() {
            manager.set_main_camera(world, entity);
        }

        // Apply activation changes
        for (entity, active) in self.pending_activations.drain(..) {
            if let Some(mut camera) = world.get_mut::<Camera>(entity) {
                camera.active = active;
            }
        }

        // Apply projection changes
        for (entity, projection) in self.pending_projections.drain(..) {
            if let Some(mut camera) = world.get_mut::<Camera>(entity) {
                camera.projection = projection;
                camera.cached_projection_matrix = None;
            }
        }

        // Refresh camera list
        manager.update(world);
    }
}
```

### Hot-Swap Tests

```rust
#[test]
fn test_camera_hot_reload() {
    let mut world = setup_scene_with_camera();
    let camera_entity = world.query::<&Camera>().next().unwrap().0;

    // Snapshot
    let snapshot = world.snapshot_components::<Camera>();

    // Modify and simulate reload
    world.clear_transient_state();
    world.restore_from(&snapshot).unwrap();

    // Camera should still work
    let camera = world.get::<Camera>(camera_entity).unwrap();
    camera.on_reload();

    // Matrices recompute correctly
    let matrix = camera.projection_matrix(16.0 / 9.0);
    assert!(matrix[0][0] > 0.0);
}

#[test]
fn test_camera_manager_hot_reload() {
    let mut world = setup_multi_camera_scene();
    let mut manager = CameraManager::new();
    manager.update(&world);

    let original_main = manager.main_camera();

    // Simulate hot-reload
    manager.on_reload();

    // Should rebuild on update
    manager.update(&world);
    assert_eq!(manager.main_camera(), original_main);
}
```

## Fault Tolerance

```rust
impl CameraManager {
    pub fn get_camera_data(&self, world: &World, entity: Entity, viewport_size: (u32, u32))
        -> Option<CameraRenderData>
    {
        let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
            let camera = world.get::<Camera>(entity)?;
            let transform = world.get::<GlobalTransform>(entity)?;

            let aspect = viewport_size.0 as f32 / viewport_size.1.max(1) as f32;

            Some(CameraRenderData {
                entity,
                view_matrix: camera.view_matrix(&transform),
                projection_matrix: camera.projection_matrix(aspect),
                // ...
            })
        }));

        match result {
            Ok(data) => data,
            Err(_) => {
                log::error!("Camera data extraction failed for {:?}, using fallback", entity);
                Some(CameraRenderData::fallback(entity))
            }
        }
    }
}

impl CameraRenderData {
    pub fn fallback(entity: Entity) -> Self {
        Self {
            entity,
            view_matrix: IDENTITY_MATRIX,
            projection_matrix: perspective_matrix(1.0, 1.0, 0.1, 1000.0),
            view_projection: perspective_matrix(1.0, 1.0, 0.1, 1000.0),
            position: [0.0, 0.0, 0.0],
            near: 0.1,
            far: 1000.0,
            clear_color: [0.0, 0.0, 0.0, 1.0],
            viewport: None,
            layer_mask: None,
        }
    }
}
```

## Acceptance Criteria

### Functional
- [ ] Camera component supports perspective and orthographic modes
- [ ] Cameras can be parented to other entities
- [ ] Camera switching works via `set_main_camera()`
- [ ] Multiple cameras render to separate viewports
- [ ] Camera FOV can be animated smoothly
- [ ] Editor inspector shows all camera properties
- [ ] Aspect ratio override works correctly
- [ ] Clear color applies per-camera
- [ ] Layer mask filters rendered entities
- [ ] GlobalTransform drives camera view matrix

### Hot-Swap Compliance
- [ ] Camera component implements `Serialize`/`Deserialize`
- [ ] Camera component implements `HotReloadable`
- [ ] CameraManager survives hot-reload
- [ ] Cached matrices invalidate and recompute after reload
- [ ] Camera switching queued for frame boundary
- [ ] Fallback camera data on extraction failure
- [ ] Main camera selection preserved across reload

## Dependencies

- **Phase 1: Scene Graph** - For camera parenting and transform propagation

## Dependents

- Phase 6: Shadow Mapping (light cameras)
- Phase 10: Picking & Raycasting (screen-to-world)
- Phase 17: Precision Management (camera-relative rendering)

---

**Estimated Complexity**: Medium
**Primary Crates**: void_ecs, void_render
**Reviewer Notes**: Ensure matrix math is correct (row vs column major)
