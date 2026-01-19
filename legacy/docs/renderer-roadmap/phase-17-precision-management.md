# Phase 17: World-Space Precision Management

## Status: Not Started

## User Story

> As a large-scene author, I want stable precision over large distances.

## Requirements Checklist

- [ ] Support origin rebasing
- [ ] Maintain camera-relative rendering
- [ ] Avoid precision loss in shaders
- [ ] Keep transforms numerically stable

## Implementation Specification

### 1. World Origin Component

```rust
// crates/void_ecs/src/components/world_origin.rs (NEW FILE)

/// Defines the current world origin offset
#[derive(Clone, Debug, Default)]
pub struct WorldOrigin {
    /// High-precision world offset (added to all positions)
    pub offset: [f64; 3],

    /// Threshold for rebasing (meters from origin)
    pub rebase_threshold: f64,

    /// Last rebase position
    pub last_rebase: [f64; 3],
}

impl WorldOrigin {
    pub fn new() -> Self {
        Self {
            offset: [0.0; 3],
            rebase_threshold: 10000.0,  // 10km default
            last_rebase: [0.0; 3],
        }
    }

    /// Check if rebasing is needed
    pub fn needs_rebase(&self, camera_pos: [f64; 3]) -> bool {
        let dx = camera_pos[0] - self.last_rebase[0];
        let dy = camera_pos[1] - self.last_rebase[1];
        let dz = camera_pos[2] - self.last_rebase[2];
        let dist_sq = dx * dx + dy * dy + dz * dz;
        dist_sq > self.rebase_threshold * self.rebase_threshold
    }

    /// Perform rebase (returns delta to apply to all entities)
    pub fn rebase(&mut self, camera_pos: [f64; 3]) -> [f64; 3] {
        let delta = [
            camera_pos[0] - self.offset[0],
            camera_pos[1] - self.offset[1],
            camera_pos[2] - self.offset[2],
        ];

        self.offset = camera_pos;
        self.last_rebase = camera_pos;

        delta
    }

    /// Convert high-precision world position to local space
    pub fn world_to_local(&self, world: [f64; 3]) -> [f32; 3] {
        [
            (world[0] - self.offset[0]) as f32,
            (world[1] - self.offset[1]) as f32,
            (world[2] - self.offset[2]) as f32,
        ]
    }

    /// Convert local position to high-precision world space
    pub fn local_to_world(&self, local: [f32; 3]) -> [f64; 3] {
        [
            self.offset[0] + local[0] as f64,
            self.offset[1] + local[1] as f64,
            self.offset[2] + local[2] as f64,
        ]
    }
}
```

### 2. High-Precision Transform

```rust
// crates/void_ecs/src/components/precision_transform.rs (NEW FILE)

/// High-precision position for large worlds
#[derive(Clone, Debug, Default)]
pub struct PrecisionPosition {
    /// High-precision world position
    pub world: [f64; 3],

    /// Cached local position (relative to origin)
    pub local: [f32; 3],

    /// Is local cache valid?
    pub cache_valid: bool,
}

impl PrecisionPosition {
    pub fn new(x: f64, y: f64, z: f64) -> Self {
        Self {
            world: [x, y, z],
            local: [x as f32, y as f32, z as f32],
            cache_valid: false,
        }
    }

    /// Update local cache from world origin
    pub fn update_local(&mut self, origin: &WorldOrigin) {
        self.local = origin.world_to_local(self.world);
        self.cache_valid = true;
    }

    /// Get local position (panics if cache invalid)
    pub fn local(&self) -> [f32; 3] {
        debug_assert!(self.cache_valid, "PrecisionPosition cache invalid");
        self.local
    }

    /// Set world position
    pub fn set_world(&mut self, x: f64, y: f64, z: f64) {
        self.world = [x, y, z];
        self.cache_valid = false;
    }

    /// Translate by delta
    pub fn translate(&mut self, dx: f64, dy: f64, dz: f64) {
        self.world[0] += dx;
        self.world[1] += dy;
        self.world[2] += dz;
        self.cache_valid = false;
    }
}
```

### 3. Origin Rebase System

```rust
// crates/void_ecs/src/systems/origin_system.rs (NEW FILE)

use void_ecs::{World, Resource};
use crate::components::{WorldOrigin, PrecisionPosition, LocalTransform, GlobalTransform};

/// Manages world origin rebasing
pub struct OriginRebaseSystem;

impl OriginRebaseSystem {
    /// Update origin and all transforms if needed
    pub fn update(world: &mut World, camera_world_pos: [f64; 3]) {
        let origin = world.get_resource::<WorldOrigin>()
            .cloned()
            .unwrap_or_default();

        if !origin.needs_rebase(camera_world_pos) {
            // Just update local caches
            Self::update_local_caches(world, &origin);
            return;
        }

        // Perform rebase
        let mut origin = origin;
        let delta = origin.rebase(camera_world_pos);

        // Update all entities with PrecisionPosition
        for (entity, (precision_pos,)) in
            world.query::<(&mut PrecisionPosition,)>()
        {
            // Position doesn't change, but local cache needs update
            precision_pos.cache_valid = false;
        }

        // Update all standard transforms (shift by delta)
        for (entity, (transform,)) in
            world.query::<(&mut LocalTransform,)>()
        {
            // Only root entities get shifted
            if world.get::<Parent>(entity).is_none() {
                transform.translation[0] -= delta[0] as f32;
                transform.translation[1] -= delta[1] as f32;
                transform.translation[2] -= delta[2] as f32;
            }
        }

        // Store updated origin
        world.insert_resource(origin);

        // Update local caches
        let origin = world.get_resource::<WorldOrigin>().unwrap();
        Self::update_local_caches(world, origin);
    }

    fn update_local_caches(world: &mut World, origin: &WorldOrigin) {
        for (entity, (precision_pos,)) in
            world.query::<(&mut PrecisionPosition,)>()
        {
            if !precision_pos.cache_valid {
                precision_pos.update_local(origin);
            }
        }
    }
}
```

### 4. Camera-Relative Rendering

```rust
// crates/void_render/src/camera_relative.rs (NEW FILE)

/// Transforms for camera-relative rendering
pub struct CameraRelativeTransform {
    /// Camera position (used as origin for rendering)
    camera_position: [f32; 3],

    /// Modified view matrix (camera at origin)
    view_matrix: [[f32; 4]; 4],

    /// Inverse camera translation
    inv_camera_translation: [[f32; 4]; 4],
}

impl CameraRelativeTransform {
    pub fn new(camera: &CameraRenderData) -> Self {
        let pos = camera.position;

        // View matrix with camera at origin
        let mut view = camera.view_matrix;

        // Zero out translation in view matrix
        view[3][0] = 0.0;
        view[3][1] = 0.0;
        view[3][2] = 0.0;

        // Inverse translation matrix
        let inv_trans = [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [-pos[0], -pos[1], -pos[2], 1.0],
        ];

        Self {
            camera_position: pos,
            view_matrix: view,
            inv_camera_translation: inv_trans,
        }
    }

    /// Transform model matrix to camera-relative space
    pub fn transform_model(&self, model: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
        // Subtract camera position from model translation
        let mut result = *model;
        result[3][0] -= self.camera_position[0];
        result[3][1] -= self.camera_position[1];
        result[3][2] -= self.camera_position[2];
        result
    }

    /// Get camera-relative view matrix
    pub fn view_matrix(&self) -> &[[f32; 4]; 4] {
        &self.view_matrix
    }
}
```

### 5. Precision-Safe Shader

```wgsl
// Camera-relative vertex transformation
struct CameraRelativeUniforms {
    view: mat4x4<f32>,           // View matrix (rotation only)
    projection: mat4x4<f32>,
    camera_position: vec3<f32>,  // For world-space calculations
    _pad: f32,
};

@group(0) @binding(0)
var<uniform> camera: CameraRelativeUniforms;

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_position: vec3<f32>,  // Relative to camera
    @location(1) world_normal: vec3<f32>,
};

@vertex
fn vs_main(vertex: VertexInput, instance: InstanceInput) -> VertexOutput {
    // Model matrix already has camera position subtracted
    let model_matrix = mat4x4<f32>(
        instance.model_matrix_0,
        instance.model_matrix_1,
        instance.model_matrix_2,
        instance.model_matrix_3,
    );

    var out: VertexOutput;

    // Position is relative to camera
    let world_pos_relative = model_matrix * vec4<f32>(vertex.position, 1.0);

    // View matrix is rotation-only, no translation jitter
    let view_pos = camera.view * world_pos_relative;

    out.clip_position = camera.projection * view_pos;
    out.world_position = world_pos_relative.xyz;

    // Normal transformation
    let normal_matrix = mat3x3<f32>(
        model_matrix[0].xyz,
        model_matrix[1].xyz,
        model_matrix[2].xyz,
    );
    out.world_normal = normalize(normal_matrix * vertex.normal);

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Lighting calculations use camera-relative positions
    // This works because light positions are also camera-relative

    let N = normalize(in.world_normal);
    let V = normalize(-in.world_position);  // Camera is at origin

    // Calculate lighting...
    return vec4<f32>(shade(in.world_position, N, V), 1.0);
}
```

### 6. Precision Utilities

```rust
// crates/void_math/src/precision.rs (NEW FILE)

/// Double-precision vector operations
pub mod f64 {
    #[derive(Clone, Copy, Debug, Default)]
    pub struct Vec3 {
        pub x: f64,
        pub y: f64,
        pub z: f64,
    }

    impl Vec3 {
        pub fn new(x: f64, y: f64, z: f64) -> Self {
            Self { x, y, z }
        }

        pub fn length(&self) -> f64 {
            (self.x * self.x + self.y * self.y + self.z * self.z).sqrt()
        }

        pub fn to_f32(&self) -> [f32; 3] {
            [self.x as f32, self.y as f32, self.z as f32]
        }

        pub fn sub(&self, other: &Vec3) -> Vec3 {
            Vec3 {
                x: self.x - other.x,
                y: self.y - other.y,
                z: self.z - other.z,
            }
        }
    }
}

/// Check if f32 position is losing precision
pub fn check_precision(pos: [f32; 3]) -> PrecisionStatus {
    let max_safe = 100_000.0;  // ~1cm precision at 100km
    let max_coord = pos[0].abs().max(pos[1].abs()).max(pos[2].abs());

    if max_coord > max_safe * 10.0 {
        PrecisionStatus::Critical
    } else if max_coord > max_safe {
        PrecisionStatus::Warning
    } else {
        PrecisionStatus::Good
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PrecisionStatus {
    Good,
    Warning,
    Critical,
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/world_origin.rs` | CREATE | Origin component |
| `void_ecs/src/components/precision_transform.rs` | CREATE | High-precision position |
| `void_ecs/src/systems/origin_system.rs` | CREATE | Rebase system |
| `void_render/src/camera_relative.rs` | CREATE | Camera-relative rendering |
| `void_math/src/precision.rs` | CREATE | f64 utilities |
| `void_runtime/src/shaders/camera_relative.wgsl` | CREATE | Precision-safe shader |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_origin_rebase() {
    let mut origin = WorldOrigin::new();
    origin.rebase_threshold = 100.0;

    // Near origin - no rebase
    assert!(!origin.needs_rebase([50.0, 0.0, 0.0]));

    // Far from origin - rebase
    assert!(origin.needs_rebase([150.0, 0.0, 0.0]));
}

#[test]
fn test_precision_at_large_distance() {
    let origin = WorldOrigin {
        offset: [1_000_000.0, 0.0, 0.0],
        ..Default::default()
    };

    let world_pos = [1_000_100.0, 50.0, 0.0];
    let local = origin.world_to_local(world_pos);

    // Should be [100.0, 50.0, 0.0] with good precision
    assert!((local[0] - 100.0).abs() < 0.01);
}

#[test]
fn test_camera_relative_transform() {
    let camera_pos = [1000.0, 0.0, 0.0];
    let object_pos = [1010.0, 0.0, 0.0];

    let relative = [
        object_pos[0] - camera_pos[0],
        object_pos[1] - camera_pos[1],
        object_pos[2] - camera_pos[2],
    ];

    // Object is 10 units from camera
    assert!((relative[0] - 10.0).abs() < 0.001);
}
```

## Hot-Swap Support

### Serialization

All precision management components derive `Serialize` and `Deserialize`:

```rust
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct WorldOrigin {
    /// High-precision world offset (added to all positions)
    pub offset: [f64; 3],

    /// Threshold for rebasing (meters from origin)
    pub rebase_threshold: f64,

    /// Last rebase position
    pub last_rebase: [f64; 3],
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct PrecisionPosition {
    /// High-precision world position
    pub world: [f64; 3],

    /// Cached local position (relative to origin)
    #[serde(skip)]  // Recomputed on load
    pub local: [f32; 3],

    /// Is local cache valid?
    #[serde(skip, default)]  // Always false after deserialize
    pub cache_valid: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum PrecisionStatus {
    Good,
    Warning,
    Critical,
}

/// State snapshot for origin rebase system
#[derive(Serialize, Deserialize)]
pub struct OriginSystemState {
    pub origin: WorldOrigin,
    pub pending_rebase: bool,
    pub rebase_count: u64,
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, HotReloadError};

impl HotReloadable for WorldOrigin {
    fn type_name(&self) -> &'static str {
        "WorldOrigin"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        let loaded: WorldOrigin = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;

        self.offset = loaded.offset;
        self.rebase_threshold = loaded.rebase_threshold;
        self.last_rebase = loaded.last_rebase;

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

impl HotReloadable for PrecisionPosition {
    fn type_name(&self) -> &'static str {
        "PrecisionPosition"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        // Only serialize the world position - local is derived
        bincode::serialize(&self.world)
            .map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        self.world = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;

        // Mark cache as invalid - will be recomputed on next update
        self.cache_valid = false;

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

impl HotReloadable for OriginRebaseSystem {
    fn type_name(&self) -> &'static str {
        "OriginRebaseSystem"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = OriginSystemState {
            origin: self.current_origin.clone(),
            pending_rebase: self.pending_rebase,
            rebase_count: self.rebase_count,
        };
        bincode::serialize(&state).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8]) -> Result<(), HotReloadError> {
        let state: OriginSystemState = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialization(e.to_string()))?;

        self.current_origin = state.origin;
        self.pending_rebase = state.pending_rebase;
        self.rebase_count = state.rebase_count;

        // Invalidate all precision position caches
        self.invalidate_all_caches = true;

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}
```

### Frame-Boundary Updates

```rust
pub struct PrecisionUpdateQueue {
    /// Queued origin changes to apply at frame boundary
    pending_origin_updates: Vec<WorldOrigin>,
    /// Entities needing cache invalidation
    invalidation_batch: Vec<Entity>,
}

impl PrecisionUpdateQueue {
    /// Queue origin update for frame boundary application
    pub fn queue_origin_update(&mut self, new_origin: WorldOrigin) {
        self.pending_origin_updates.push(new_origin);
    }

    /// Apply at safe frame boundary point
    pub fn apply_at_frame_boundary(&mut self, world: &mut World) {
        if let Some(origin) = self.pending_origin_updates.pop() {
            // Update world resource
            world.insert_resource(origin.clone());

            // Invalidate all PrecisionPosition caches
            for (entity, (pos,)) in world.query::<(&mut PrecisionPosition,)>() {
                pos.cache_valid = false;
            }

            // Clear pending updates
            self.pending_origin_updates.clear();
        }
    }
}

/// Ensures precision state consistency after hot-reload
pub fn post_reload_precision_sync(world: &mut World) {
    let origin = world.get_resource::<WorldOrigin>()
        .cloned()
        .unwrap_or_default();

    // Recompute all local positions from world positions
    for (entity, (precision,)) in world.query::<(&mut PrecisionPosition,)>() {
        precision.update_local(&origin);
    }

    log::info!("Precision positions synchronized after hot-reload");
}
```

### Origin Rebasing State Preservation

```rust
/// Tracks rebasing history for debugging and hot-reload recovery
#[derive(Serialize, Deserialize)]
pub struct RebaseHistory {
    /// Recent rebase events
    pub events: VecDeque<RebaseEvent>,
    pub max_events: usize,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RebaseEvent {
    pub timestamp: f64,
    pub old_origin: [f64; 3],
    pub new_origin: [f64; 3],
    pub delta: [f64; 3],
    pub entities_affected: u32,
}

impl RebaseHistory {
    /// Reconstruct origin from history if needed
    pub fn recover_origin(&self) -> Option<WorldOrigin> {
        self.events.back().map(|event| {
            WorldOrigin {
                offset: event.new_origin,
                last_rebase: event.new_origin,
                ..Default::default()
            }
        })
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_world_origin_serialization_roundtrip() {
        let origin = WorldOrigin {
            offset: [1_000_000.0, 500.0, -2_000_000.0],
            rebase_threshold: 50000.0,
            last_rebase: [1_000_000.0, 500.0, -2_000_000.0],
        };

        let serialized = origin.serialize_state().unwrap();
        let mut restored = WorldOrigin::default();
        restored.deserialize_state(&serialized).unwrap();

        assert_eq!(origin.offset, restored.offset);
        assert_eq!(origin.rebase_threshold, restored.rebase_threshold);
        assert_eq!(origin.last_rebase, restored.last_rebase);
    }

    #[test]
    fn test_precision_position_cache_invalidated_on_reload() {
        let mut pos = PrecisionPosition {
            world: [1000.0, 0.0, 0.0],
            local: [100.0, 0.0, 0.0],
            cache_valid: true,
        };

        let serialized = pos.serialize_state().unwrap();

        let mut restored = PrecisionPosition::default();
        restored.deserialize_state(&serialized).unwrap();

        // World position preserved
        assert_eq!(restored.world, [1000.0, 0.0, 0.0]);
        // Cache marked invalid
        assert!(!restored.cache_valid);
    }

    #[test]
    fn test_origin_rebase_state_preserved() {
        let mut system = OriginRebaseSystem::new();
        system.current_origin = WorldOrigin {
            offset: [50000.0, 0.0, 50000.0],
            ..Default::default()
        };
        system.rebase_count = 5;

        let serialized = system.serialize_state().unwrap();

        let mut restored = OriginRebaseSystem::new();
        restored.deserialize_state(&serialized).unwrap();

        assert_eq!(restored.current_origin.offset, [50000.0, 0.0, 50000.0]);
        assert_eq!(restored.rebase_count, 5);
    }

    #[test]
    fn test_precision_sync_after_reload() {
        let mut world = World::new();

        let origin = WorldOrigin {
            offset: [10000.0, 0.0, 0.0],
            ..Default::default()
        };
        world.insert_resource(origin);

        // Spawn entity with precision position
        let entity = world.spawn((
            PrecisionPosition {
                world: [10100.0, 50.0, 0.0],
                local: [0.0, 0.0, 0.0],  // Wrong value
                cache_valid: false,
            },
        ));

        // Simulate post-reload sync
        post_reload_precision_sync(&mut world);

        let pos = world.get::<PrecisionPosition>(entity).unwrap();
        assert!(pos.cache_valid);
        assert!((pos.local[0] - 100.0).abs() < 0.01);
        assert!((pos.local[1] - 50.0).abs() < 0.01);
    }

    #[test]
    fn test_frame_boundary_origin_update() {
        let mut queue = PrecisionUpdateQueue::default();
        let mut world = World::new();

        // Spawn entities
        world.spawn((PrecisionPosition::new(1000.0, 0.0, 0.0),));

        // Queue origin change
        queue.queue_origin_update(WorldOrigin {
            offset: [900.0, 0.0, 0.0],
            ..Default::default()
        });

        // Apply at boundary
        queue.apply_at_frame_boundary(&mut world);

        // Check origin updated
        let origin = world.get_resource::<WorldOrigin>().unwrap();
        assert_eq!(origin.offset, [900.0, 0.0, 0.0]);

        // Check caches invalidated
        for (_, (pos,)) in world.query::<(&PrecisionPosition,)>() {
            assert!(!pos.cache_valid);
        }
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
impl OriginRebaseSystem {
    pub fn update_with_recovery(world: &mut World, camera_world_pos: [f64; 3]) {
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            Self::update(world, camera_world_pos)
        }));

        match result {
            Ok(()) => {}
            Err(panic) => {
                log::error!("Origin rebase panicked: {:?}", panic);
                Self::recover_origin_state(world);
            }
        }
    }

    fn recover_origin_state(world: &mut World) {
        // Reset to safe default origin
        let origin = WorldOrigin {
            offset: [0.0, 0.0, 0.0],
            rebase_threshold: 10000.0,
            last_rebase: [0.0, 0.0, 0.0],
        };
        world.insert_resource(origin.clone());

        // Invalidate all precision caches
        for (entity, (pos,)) in world.query::<(&mut PrecisionPosition,)>() {
            pos.cache_valid = false;
        }

        log::warn!("Origin state recovered to default");
    }
}
```

### Precision Degradation Handling

```rust
impl WorldOrigin {
    /// Safe conversion that handles extreme values
    pub fn world_to_local_safe(&self, world: [f64; 3]) -> Result<[f32; 3], PrecisionError> {
        let local = [
            (world[0] - self.offset[0]) as f32,
            (world[1] - self.offset[1]) as f32,
            (world[2] - self.offset[2]) as f32,
        ];

        // Check for infinity or NaN
        if local.iter().any(|v| !v.is_finite()) {
            return Err(PrecisionError::Overflow);
        }

        // Check for precision loss
        let status = check_precision(local);
        if status == PrecisionStatus::Critical {
            return Err(PrecisionError::PrecisionLoss);
        }

        Ok(local)
    }
}

#[derive(Debug)]
pub enum PrecisionError {
    Overflow,
    PrecisionLoss,
    InvalidInput,
}

/// Fallback rendering for precision-compromised objects
pub fn render_with_precision_fallback(
    entity: Entity,
    position: &PrecisionPosition,
    origin: &WorldOrigin,
) -> RenderMode {
    match origin.world_to_local_safe(position.world) {
        Ok(local) => RenderMode::Normal { position: local },
        Err(PrecisionError::PrecisionLoss) => {
            log::warn!("Entity {:?} has precision loss, using billboard", entity);
            RenderMode::Billboard {
                direction: compute_direction_to_camera(position.world, origin),
            }
        }
        Err(_) => {
            log::error!("Entity {:?} has invalid position, hiding", entity);
            RenderMode::Hidden
        }
    }
}

pub enum RenderMode {
    Normal { position: [f32; 3] },
    Billboard { direction: [f32; 3] },
    Hidden,
}
```

### Origin Validation

```rust
impl WorldOrigin {
    pub fn validate(&self) -> Result<(), OriginValidationError> {
        // Check for NaN/infinity
        if !self.offset.iter().all(|v| v.is_finite()) {
            return Err(OriginValidationError::InvalidOffset);
        }

        if !self.last_rebase.iter().all(|v| v.is_finite()) {
            return Err(OriginValidationError::InvalidLastRebase);
        }

        if self.rebase_threshold <= 0.0 || !self.rebase_threshold.is_finite() {
            return Err(OriginValidationError::InvalidThreshold);
        }

        Ok(())
    }

    pub fn sanitize(&mut self) {
        // Replace invalid values with defaults
        for i in 0..3 {
            if !self.offset[i].is_finite() {
                self.offset[i] = 0.0;
            }
            if !self.last_rebase[i].is_finite() {
                self.last_rebase[i] = 0.0;
            }
        }

        if self.rebase_threshold <= 0.0 || !self.rebase_threshold.is_finite() {
            self.rebase_threshold = 10000.0;
        }
    }
}

#[derive(Debug)]
pub enum OriginValidationError {
    InvalidOffset,
    InvalidLastRebase,
    InvalidThreshold,
}
```

## Acceptance Criteria

### Functional

- [ ] Objects render correctly at 100km from origin
- [ ] Origin rebasing is smooth (no visual pop)
- [ ] Physics still works after rebase
- [ ] Multiplayer sync uses high-precision positions
- [ ] No jittering at large distances
- [ ] Precision warnings shown in editor
- [ ] Camera-relative rendering works

### Hot-Swap Compliance

- [ ] WorldOrigin derives Serialize/Deserialize
- [ ] PrecisionPosition derives Serialize/Deserialize
- [ ] WorldOrigin implements HotReloadable trait
- [ ] PrecisionPosition implements HotReloadable trait
- [ ] OriginRebaseSystem implements HotReloadable trait
- [ ] Local position caches invalidated after hot-reload
- [ ] post_reload_precision_sync recomputes all local positions
- [ ] Origin rebase history preserved for recovery
- [ ] Frame-boundary update queue prevents mid-rebase hot-swap
- [ ] Rebase count and history preserved across reload
- [ ] PrecisionStatus enum serializable for debug state
- [ ] Hot-swap tests pass in CI

## Dependencies

- **Phase 1: Scene Graph** - Transform propagation
- **Phase 2: Camera System** - Camera matrices

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: High
**Primary Crates**: void_ecs, void_math, void_render
**Reviewer Notes**: Test at extreme distances (1000km+)
