# Phase 15: Level of Detail (LOD)

## Status: Not Started

## User Story

> As a scene author, I want objects to change detail based on distance.

## Requirements Checklist

- [ ] Allow multiple LOD meshes per entity
- [ ] Distance-based switching
- [ ] Optional hysteresis to avoid popping
- [ ] Graceful fallback when LODs are missing

## Implementation Specification

### 1. LOD Component

```rust
// crates/void_ecs/src/components/lod.rs (NEW FILE)

/// Level of Detail configuration
#[derive(Clone, Debug)]
pub struct LodGroup {
    /// LOD levels (sorted by distance)
    pub levels: Vec<LodLevel>,

    /// Current active LOD index
    pub current: u32,

    /// LOD selection mode
    pub mode: LodMode,

    /// Hysteresis factor (0-1, 0 = no hysteresis)
    pub hysteresis: f32,

    /// Fade duration for crossfade mode
    pub fade_duration: f32,

    /// Current fade state
    pub fade_state: Option<LodFadeState>,

    /// Override LOD index (-1 = auto)
    pub force_lod: i32,
}

#[derive(Clone, Debug)]
pub struct LodLevel {
    /// Mesh asset for this LOD
    pub mesh: String,

    /// Screen height threshold (0-1, percentage of screen)
    pub screen_height: f32,

    /// Or use distance threshold
    pub distance: f32,

    /// Render priority offset
    pub priority_offset: i32,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum LodMode {
    /// Instant switch at threshold
    #[default]
    Instant,

    /// Cross-fade between LODs
    CrossFade,

    /// Dither-based transition
    Dither,

    /// Geometry morphing (requires compatible meshes)
    Morph,
}

#[derive(Clone, Debug)]
pub struct LodFadeState {
    pub from_lod: u32,
    pub to_lod: u32,
    pub progress: f32,  // 0-1
}

impl Default for LodGroup {
    fn default() -> Self {
        Self {
            levels: Vec::new(),
            current: 0,
            mode: LodMode::Instant,
            hysteresis: 0.1,
            fade_duration: 0.3,
            fade_state: None,
            force_lod: -1,
        }
    }
}

impl LodGroup {
    /// Create LOD group with distance-based levels
    pub fn from_distances(meshes: &[(String, f32)]) -> Self {
        let levels = meshes.iter()
            .map(|(mesh, dist)| LodLevel {
                mesh: mesh.clone(),
                screen_height: 0.0,
                distance: *dist,
                priority_offset: 0,
            })
            .collect();

        Self {
            levels,
            ..Default::default()
        }
    }

    /// Create LOD group with screen-size-based levels
    pub fn from_screen_heights(meshes: &[(String, f32)]) -> Self {
        let levels = meshes.iter()
            .map(|(mesh, height)| LodLevel {
                mesh: mesh.clone(),
                screen_height: *height,
                distance: 0.0,
                priority_offset: 0,
            })
            .collect();

        Self {
            levels,
            ..Default::default()
        }
    }

    /// Calculate which LOD to use
    pub fn calculate_lod(&self, distance: f32, screen_height: f32) -> u32 {
        if self.force_lod >= 0 {
            return (self.force_lod as u32).min(self.levels.len().saturating_sub(1) as u32);
        }

        for (i, level) in self.levels.iter().enumerate() {
            // Check screen height first (if set)
            if level.screen_height > 0.0 && screen_height >= level.screen_height {
                return i as u32;
            }

            // Then check distance
            if level.distance > 0.0 && distance <= level.distance {
                return i as u32;
            }
        }

        // Default to lowest LOD
        self.levels.len().saturating_sub(1) as u32
    }

    /// Calculate with hysteresis
    pub fn calculate_lod_with_hysteresis(&self, distance: f32, screen_height: f32) -> u32 {
        let target = self.calculate_lod(distance, screen_height);

        if self.hysteresis <= 0.0 || self.current == target {
            return target;
        }

        // Only switch if we've crossed threshold by hysteresis amount
        let current_level = &self.levels[self.current as usize];
        let threshold_dist = current_level.distance;

        if target > self.current {
            // Going to lower detail
            let hysteresis_dist = threshold_dist * (1.0 + self.hysteresis);
            if distance > hysteresis_dist {
                return target;
            }
        } else {
            // Going to higher detail
            let hysteresis_dist = threshold_dist * (1.0 - self.hysteresis);
            if distance < hysteresis_dist {
                return target;
            }
        }

        self.current
    }

    /// Get current mesh to render
    pub fn current_mesh(&self) -> Option<&str> {
        self.levels.get(self.current as usize).map(|l| l.mesh.as_str())
    }

    /// Get mesh for specific LOD
    pub fn mesh_at(&self, index: u32) -> Option<&str> {
        self.levels.get(index as usize).map(|l| l.mesh.as_str())
    }

    /// Get fade alpha for crossfade mode
    pub fn fade_alpha(&self) -> Option<(u32, u32, f32)> {
        self.fade_state.as_ref().map(|s| (s.from_lod, s.to_lod, s.progress))
    }
}
```

### 2. LOD Selection System

```rust
// crates/void_ecs/src/systems/lod_system.rs (NEW FILE)

use void_ecs::{World, Entity};
use void_math::Vec3;

/// Updates LOD selections based on camera distance
pub struct LodSystem;

impl LodSystem {
    pub fn update(
        world: &mut World,
        camera_pos: Vec3,
        camera_fov: f32,
        screen_height: f32,
        dt: f32,
    ) {
        for (entity, (lod, transform)) in
            world.query::<(&mut LodGroup, &GlobalTransform)>()
        {
            let pos = transform.translation();
            let distance = (Vec3::from(pos) - camera_pos).length();

            // Calculate screen coverage
            let bounds_radius = Self::get_bounds_radius(world, entity);
            let screen_coverage = Self::calculate_screen_coverage(
                distance,
                bounds_radius,
                camera_fov,
                screen_height,
            );

            // Determine target LOD
            let target = lod.calculate_lod_with_hysteresis(distance, screen_coverage);

            match lod.mode {
                LodMode::Instant => {
                    lod.current = target;
                    lod.fade_state = None;
                }

                LodMode::CrossFade => {
                    if target != lod.current && lod.fade_state.is_none() {
                        // Start fade
                        lod.fade_state = Some(LodFadeState {
                            from_lod: lod.current,
                            to_lod: target,
                            progress: 0.0,
                        });
                    }

                    // Update fade
                    if let Some(ref mut state) = lod.fade_state {
                        state.progress += dt / lod.fade_duration;

                        if state.progress >= 1.0 {
                            lod.current = state.to_lod;
                            lod.fade_state = None;
                        }
                    }
                }

                LodMode::Dither => {
                    // Dither uses shader-based pattern
                    lod.current = target;
                }

                LodMode::Morph => {
                    // Morph requires blend factor in shader
                    lod.current = target;
                }
            }
        }
    }

    fn get_bounds_radius(world: &World, entity: Entity) -> f32 {
        world.get::<BoundingBox>(entity)
            .map(|b| b.extents().length())
            .unwrap_or(1.0)
    }

    fn calculate_screen_coverage(
        distance: f32,
        radius: f32,
        fov: f32,
        screen_height: f32,
    ) -> f32 {
        // Calculate how much of the screen the object covers
        let angular_size = 2.0 * (radius / distance).atan();
        let screen_fraction = angular_size / fov;
        screen_fraction * screen_height
    }
}
```

### 3. LOD-Aware Rendering

```rust
// crates/void_render/src/extraction.rs (modifications)

impl SceneExtractor {
    pub fn extract_with_lod(
        &self,
        world: &World,
        mesh_cache: &MeshCache,
    ) -> Vec<DrawCall> {
        let mut draws = Vec::new();

        for (entity, (mesh_renderer, lod, transform, material)) in
            world.query::<(&MeshRenderer, Option<&LodGroup>, &GlobalTransform, &Material)>()
        {
            // Determine which mesh to use
            let mesh_path = if let Some(lod) = lod {
                // Use LOD mesh
                match lod.current_mesh() {
                    Some(m) => m.to_string(),
                    None => continue,  // No LOD available
                }
            } else {
                // Use regular mesh
                match &mesh_renderer.mesh_asset {
                    Some(m) => m.clone(),
                    None => continue,
                }
            };

            // Check for crossfade
            let (fade_out_mesh, fade_alpha) = if let Some(lod) = lod {
                if let Some((from, to, alpha)) = lod.fade_alpha() {
                    // Render both meshes with alpha
                    let from_mesh = lod.mesh_at(from).map(String::from);
                    (from_mesh, Some(alpha))
                } else {
                    (None, None)
                }
            } else {
                (None, None)
            };

            // Main LOD draw
            draws.push(DrawCall {
                entity_id: entity.id(),
                mesh_path,
                transform: transform.matrix,
                material: material.clone(),
                alpha_override: fade_alpha,
                ..Default::default()
            });

            // Fade-out mesh (for crossfade)
            if let (Some(mesh), Some(alpha)) = (fade_out_mesh, fade_alpha) {
                draws.push(DrawCall {
                    entity_id: entity.id(),
                    mesh_path: mesh,
                    transform: transform.matrix,
                    material: material.clone(),
                    alpha_override: Some(1.0 - alpha),
                    ..Default::default()
                });
            }
        }

        draws
    }
}
```

### 4. LOD Shader Support

```wgsl
// Dither pattern for LOD transitions
fn dither_lod(screen_pos: vec2<f32>, lod_factor: f32) -> bool {
    // Bayer dither matrix 4x4
    let bayer = array<f32, 16>(
        0.0/16.0, 8.0/16.0, 2.0/16.0, 10.0/16.0,
        12.0/16.0, 4.0/16.0, 14.0/16.0, 6.0/16.0,
        3.0/16.0, 11.0/16.0, 1.0/16.0, 9.0/16.0,
        15.0/16.0, 7.0/16.0, 13.0/16.0, 5.0/16.0
    );

    let x = u32(screen_pos.x) % 4u;
    let y = u32(screen_pos.y) % 4u;
    let threshold = bayer[y * 4u + x];

    return lod_factor > threshold;
}

// In fragment shader
@fragment
fn fs_lod(in: VertexOutput) -> @location(0) vec4<f32> {
    // Dither-based LOD transition
    if (material.lod_transition > 0.0) {
        if (!dither_lod(in.position.xy, material.lod_transition)) {
            discard;
        }
    }

    // Normal shading...
    return shade(in);
}
```

### 5. Auto-LOD Generation (Optional)

```rust
// crates/void_asset_server/src/loaders/lod_generator.rs (NEW FILE)

use void_math::Vec3;

/// Generates LOD meshes automatically
pub struct LodGenerator;

impl LodGenerator {
    /// Generate LOD chain from high-poly mesh
    pub fn generate(mesh: &MeshAsset, levels: u32) -> Vec<MeshAsset> {
        let mut lods = vec![mesh.clone()];

        let mut current = mesh.clone();

        for _ in 1..levels {
            current = Self::simplify(&current, 0.5);  // 50% reduction per level
            lods.push(current.clone());
        }

        lods
    }

    /// Simplify mesh using edge collapse
    fn simplify(mesh: &MeshAsset, ratio: f32) -> MeshAsset {
        // Quadric Error Metrics (QEM) simplification
        // This is a complex algorithm - simplified version here

        let target_triangles = (mesh.primitives[0].indices.as_ref()
            .map(|i| i.len() / 3)
            .unwrap_or(mesh.primitives[0].vertices.len() / 3) as f32 * ratio) as usize;

        // For production, use meshopt or similar library
        todo!("Implement mesh simplification")
    }

    /// Suggest LOD distances based on mesh size
    pub fn suggest_distances(bounds_radius: f32, screen_percentage: f32) -> Vec<f32> {
        // Calculate distances where mesh covers given screen percentage
        let fov = 60.0_f32.to_radians();

        [0.5, 0.25, 0.1, 0.05].iter()
            .map(|&coverage| bounds_radius / (coverage * fov.tan()))
            .collect()
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/lod.rs` | CREATE | LOD component |
| `void_ecs/src/systems/lod_system.rs` | CREATE | LOD selection |
| `void_render/src/extraction.rs` | MODIFY | LOD-aware extraction |
| `void_asset_server/src/loaders/lod_generator.rs` | CREATE | Auto LOD generation |
| `void_runtime/src/shaders/lod.wgsl` | CREATE | Dither/fade shaders |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_lod_selection() {
    let lod = LodGroup::from_distances(&[
        ("high.glb".into(), 10.0),
        ("medium.glb".into(), 50.0),
        ("low.glb".into(), 100.0),
    ]);

    assert_eq!(lod.calculate_lod(5.0, 0.0), 0);   // High
    assert_eq!(lod.calculate_lod(30.0, 0.0), 1);  // Medium
    assert_eq!(lod.calculate_lod(80.0, 0.0), 2);  // Low
}

#[test]
fn test_hysteresis() {
    let mut lod = LodGroup::from_distances(&[
        ("high.glb".into(), 10.0),
        ("low.glb".into(), 20.0),
    ]);
    lod.hysteresis = 0.2;
    lod.current = 0;

    // At threshold but hysteresis prevents switch
    assert_eq!(lod.calculate_lod_with_hysteresis(11.0, 0.0), 0);

    // Past hysteresis threshold
    assert_eq!(lod.calculate_lod_with_hysteresis(13.0, 0.0), 1);
}
```

## Hot-Swap Support

### Serialization

LOD components and configuration must be serializable for scene persistence and hot-swap:

```rust
use serde::{Deserialize, Serialize};

/// Level of Detail configuration (serializable)
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodGroup {
    /// LOD levels (sorted by distance)
    pub levels: Vec<LodLevel>,

    /// Current active LOD index
    #[serde(skip)]
    pub current: u32,

    /// LOD selection mode
    pub mode: LodMode,

    /// Hysteresis factor (0-1, 0 = no hysteresis)
    pub hysteresis: f32,

    /// Fade duration for crossfade mode
    pub fade_duration: f32,

    /// Current fade state (transient, not serialized)
    #[serde(skip)]
    pub fade_state: Option<LodFadeState>,

    /// Override LOD index (-1 = auto)
    pub force_lod: i32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodLevel {
    /// Mesh asset path for this LOD
    pub mesh: String,

    /// Screen height threshold (0-1, percentage of screen)
    pub screen_height: f32,

    /// Or use distance threshold
    pub distance: f32,

    /// Render priority offset
    pub priority_offset: i32,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum LodMode {
    /// Instant switch at threshold
    #[default]
    Instant,

    /// Cross-fade between LODs
    CrossFade,

    /// Dither-based transition
    Dither,

    /// Geometry morphing (requires compatible meshes)
    Morph,
}

/// LOD system configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LodSystemConfig {
    /// Global LOD bias (shifts all thresholds)
    pub lod_bias: f32,
    /// Maximum LOD level allowed
    pub max_lod: u32,
    /// Force specific LOD for all entities (-1 = auto)
    pub global_force_lod: i32,
    /// Enable LOD transitions
    pub transitions_enabled: bool,
}

impl Default for LodSystemConfig {
    fn default() -> Self {
        Self {
            lod_bias: 0.0,
            max_lod: u32::MAX,
            global_force_lod: -1,
            transitions_enabled: true,
        }
    }
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, ReloadContext};

impl HotReloadable for LodGroup {
    fn type_name(&self) -> &'static str {
        "LodGroup"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialize(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8], _ctx: &ReloadContext) -> Result<(), HotReloadError> {
        let restored: LodGroup = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialize(e.to_string()))?;

        // Restore configuration but reset transient state
        self.levels = restored.levels;
        self.mode = restored.mode;
        self.hysteresis = restored.hysteresis;
        self.fade_duration = restored.fade_duration;
        self.force_lod = restored.force_lod;

        // Reset transient state - will be recalculated
        self.current = 0;
        self.fade_state = None;

        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

/// LOD system with hot-reload support
pub struct LodSystem {
    config: LodSystemConfig,
    update_queue: Vec<LodUpdate>,
    mesh_load_requests: Vec<MeshLoadRequest>,
}

impl HotReloadable for LodSystem {
    fn type_name(&self) -> &'static str {
        "LodSystem"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(&self.config).map_err(|e| HotReloadError::Serialize(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> Result<(), HotReloadError> {
        let config: LodSystemConfig = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialize(e.to_string()))?;

        // Queue config update for frame boundary
        ctx.queue_update(LodUpdate::UpdateConfig(config));
        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}
```

### Asset Dependencies

LOD meshes are external assets that need tracking for hot-reload:

```rust
use void_asset::AssetDependent;

impl AssetDependent for LodGroup {
    fn asset_paths(&self) -> Vec<&str> {
        self.levels.iter().map(|l| l.mesh.as_str()).collect()
    }

    fn on_asset_changed(&mut self, path: &str, ctx: &mut AssetReloadContext) {
        // Find which LOD level uses this mesh
        for (i, level) in self.levels.iter().enumerate() {
            if level.mesh == path {
                log::info!("LOD {} mesh changed: {}", i, path);
                ctx.queue_mesh_reload(path);

                // If this is the current LOD, force re-extraction
                if i as u32 == self.current {
                    ctx.mark_entity_dirty(ctx.entity());
                }
            }
        }
    }

    fn on_asset_removed(&mut self, path: &str) -> AssetRemovalAction {
        // Check if any LOD level uses this mesh
        let affected: Vec<_> = self.levels.iter()
            .enumerate()
            .filter(|(_, l)| l.mesh == path)
            .map(|(i, _)| i)
            .collect();

        if affected.is_empty() {
            return AssetRemovalAction::None;
        }

        log::warn!("LOD mesh removed: {}, affected levels: {:?}", path, affected);

        // Fall back to adjacent LOD or placeholder
        AssetRemovalAction::UseFallback
    }
}

/// Track mesh loading for LOD levels
pub struct LodMeshTracker {
    /// Meshes currently loading
    loading: HashMap<String, MeshLoadState>,
    /// Cached mesh handles per entity
    mesh_handles: HashMap<(Entity, u32), MeshHandle>,
}

#[derive(Clone, Debug)]
pub enum MeshLoadState {
    Loading,
    Loaded(MeshHandle),
    Failed(String),
}

impl LodMeshTracker {
    /// Preload all LOD meshes for an entity
    pub fn preload_lod_meshes(&mut self, entity: Entity, lod_group: &LodGroup, asset_server: &AssetServer) {
        for (i, level) in lod_group.levels.iter().enumerate() {
            if !self.loading.contains_key(&level.mesh) {
                let handle = asset_server.load_async::<MeshAsset>(&level.mesh);
                self.loading.insert(level.mesh.clone(), MeshLoadState::Loading);

                // Store handle when loaded
                let mesh_path = level.mesh.clone();
                handle.on_complete(move |result| {
                    match result {
                        Ok(mesh) => MeshLoadState::Loaded(mesh),
                        Err(e) => MeshLoadState::Failed(e.to_string()),
                    }
                });
            }
        }
    }

    /// Check if mesh is ready for a LOD level
    pub fn is_mesh_ready(&self, lod_group: &LodGroup, level: u32) -> bool {
        lod_group.levels.get(level as usize)
            .and_then(|l| self.loading.get(&l.mesh))
            .map(|s| matches!(s, MeshLoadState::Loaded(_)))
            .unwrap_or(false)
    }

    /// Handle mesh hot-reload notification
    pub fn on_mesh_reloaded(&mut self, path: &str, new_handle: MeshHandle) {
        if let Some(state) = self.loading.get_mut(path) {
            *state = MeshLoadState::Loaded(new_handle);
        }
    }
}
```

### Frame-Boundary Updates

LOD updates are queued and applied at frame boundaries:

```rust
/// LOD update operations
pub enum LodUpdate {
    /// Update system configuration
    UpdateConfig(LodSystemConfig),
    /// Force LOD recalculation for entities
    RecalculateLods(Vec<Entity>),
    /// Preload meshes for entities
    PreloadMeshes(Vec<Entity>),
    /// Reset transition state
    ResetTransitions,
}

impl LodSystem {
    /// Apply pending updates at frame boundary
    pub fn apply_pending_updates(&mut self, world: &mut World, asset_server: &AssetServer) {
        let updates = std::mem::take(&mut self.update_queue);

        for update in updates {
            match update {
                LodUpdate::UpdateConfig(config) => {
                    let transitions_changed = self.config.transitions_enabled != config.transitions_enabled;
                    self.config = config;

                    if transitions_changed && !self.config.transitions_enabled {
                        // Clear all fade states
                        for (_, lod) in world.query::<&mut LodGroup>() {
                            lod.fade_state = None;
                        }
                    }
                }
                LodUpdate::RecalculateLods(entities) => {
                    for entity in entities {
                        if let Some(lod) = world.get_mut::<LodGroup>(entity) {
                            // Reset to trigger recalculation
                            lod.current = 0;
                            lod.fade_state = None;
                        }
                    }
                }
                LodUpdate::PreloadMeshes(entities) => {
                    for entity in entities {
                        if let Some(lod) = world.get::<LodGroup>(entity) {
                            self.mesh_tracker.preload_lod_meshes(entity, lod, asset_server);
                        }
                    }
                }
                LodUpdate::ResetTransitions => {
                    for (_, lod) in world.query::<&mut LodGroup>() {
                        lod.fade_state = None;
                    }
                }
            }
        }
    }

    /// Queue an update for frame boundary
    pub fn queue_update(&mut self, update: LodUpdate) {
        self.update_queue.push(update);
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_lod_group_serialization() {
        let lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("medium.glb".into(), 50.0),
            ("low.glb".into(), 100.0),
        ]);

        let serialized = bincode::serialize(&lod).unwrap();
        let deserialized: LodGroup = bincode::deserialize(&serialized).unwrap();

        assert_eq!(deserialized.levels.len(), 3);
        assert_eq!(deserialized.levels[0].mesh, "high.glb");
        assert_eq!(deserialized.levels[1].distance, 50.0);
    }

    #[test]
    fn test_lod_group_hot_reload() {
        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("low.glb".into(), 50.0),
        ]);
        lod.current = 1;
        lod.fade_state = Some(LodFadeState { from_lod: 0, to_lod: 1, progress: 0.5 });

        let state = lod.serialize_state().unwrap();

        let mut restored = LodGroup::default();
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();

        // Config restored
        assert_eq!(restored.levels.len(), 2);
        assert_eq!(restored.levels[0].mesh, "high.glb");

        // Transient state reset
        assert_eq!(restored.current, 0);
        assert!(restored.fade_state.is_none());
    }

    #[test]
    fn test_lod_asset_dependencies() {
        let lod = LodGroup::from_distances(&[
            ("meshes/char_high.glb".into(), 10.0),
            ("meshes/char_med.glb".into(), 50.0),
            ("meshes/char_low.glb".into(), 100.0),
        ]);

        let paths = lod.asset_paths();
        assert_eq!(paths.len(), 3);
        assert!(paths.contains(&"meshes/char_high.glb"));
        assert!(paths.contains(&"meshes/char_med.glb"));
        assert!(paths.contains(&"meshes/char_low.glb"));
    }

    #[test]
    fn test_lod_mesh_change_notification() {
        let mut lod = LodGroup::from_distances(&[
            ("high.glb".into(), 10.0),
            ("low.glb".into(), 50.0),
        ]);
        lod.current = 0;

        let mut ctx = AssetReloadContext::new(Entity::from_raw(1));
        lod.on_asset_changed("high.glb", &mut ctx);

        assert!(ctx.mesh_reload_queued("high.glb"));
        assert!(ctx.entity_marked_dirty());
    }

    #[test]
    fn test_lod_system_config_hot_reload() {
        let mut system = LodSystem::new();
        system.config.lod_bias = 2.0;
        system.config.max_lod = 3;

        let state = system.serialize_state().unwrap();

        let mut restored = LodSystem::new();
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();

        // Config update is queued
        assert!(!restored.update_queue.is_empty());

        // Apply updates
        let mut world = World::new();
        let asset_server = MockAssetServer::new();
        restored.apply_pending_updates(&mut world, &asset_server);

        assert_eq!(restored.config.lod_bias, 2.0);
        assert_eq!(restored.config.max_lod, 3);
    }

    #[test]
    fn test_frame_boundary_lod_recalculation() {
        let mut world = World::new();
        let e1 = world.spawn((
            Transform::default(),
            LodGroup::from_distances(&[("high.glb".into(), 10.0)]),
        ));

        let mut system = LodSystem::new();
        system.queue_update(LodUpdate::RecalculateLods(vec![e1]));

        assert_eq!(system.update_queue.len(), 1);

        let asset_server = MockAssetServer::new();
        system.apply_pending_updates(&mut world, &asset_server);

        assert!(system.update_queue.is_empty());
    }
}
```

## Fault Tolerance

### Catch Unwind for LOD Operations

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};

impl LodSystem {
    /// Update LOD selections with panic recovery
    pub fn update_safe(
        &mut self,
        world: &mut World,
        camera_pos: Vec3,
        camera_fov: f32,
        screen_height: f32,
        dt: f32,
    ) -> Result<(), LodError> {
        catch_unwind(AssertUnwindSafe(|| {
            self.update(world, camera_pos, camera_fov, screen_height, dt);
        })).map_err(|panic_info| {
            log::error!("LOD update panicked: {:?}", panic_info);
            LodError::UpdatePanic
        })
    }
}

#[derive(Debug)]
pub enum LodError {
    UpdatePanic,
    MeshNotFound(String),
    InvalidLodIndex(u32),
}
```

### Fallback/Degradation Behavior

```rust
impl LodGroup {
    /// Get mesh with fallback chain
    pub fn current_mesh_with_fallback(&self, mesh_tracker: &LodMeshTracker) -> Option<&str> {
        // Try current LOD
        if let Some(level) = self.levels.get(self.current as usize) {
            if mesh_tracker.is_mesh_ready(self, self.current) {
                return Some(&level.mesh);
            }
        }

        // Try lower detail LODs (more likely to be loaded)
        for i in (self.current as usize + 1)..self.levels.len() {
            if mesh_tracker.is_mesh_ready(self, i as u32) {
                log::debug!("LOD {} not ready, falling back to LOD {}", self.current, i);
                return Some(&self.levels[i].mesh);
            }
        }

        // Try higher detail LODs
        for i in (0..self.current as usize).rev() {
            if mesh_tracker.is_mesh_ready(self, i as u32) {
                log::debug!("LOD {} not ready, using higher detail LOD {}", self.current, i);
                return Some(&self.levels[i].mesh);
            }
        }

        // No LOD available
        log::warn!("No LOD meshes available");
        None
    }

    /// Handle missing LOD gracefully
    pub fn handle_missing_lod(&mut self, missing_index: u32) {
        log::warn!("LOD {} mesh missing, adjusting levels", missing_index);

        // Remove the missing level
        if (missing_index as usize) < self.levels.len() {
            self.levels.remove(missing_index as usize);

            // Adjust current if needed
            if self.current >= self.levels.len() as u32 {
                self.current = self.levels.len().saturating_sub(1) as u32;
            }

            // Recalculate distances for remaining levels
            self.redistribute_distances();
        }
    }

    /// Redistribute distances after level removal
    fn redistribute_distances(&mut self) {
        if self.levels.len() < 2 {
            return;
        }

        let max_dist = self.levels.last().map(|l| l.distance).unwrap_or(100.0);
        let step = max_dist / self.levels.len() as f32;

        for (i, level) in self.levels.iter_mut().enumerate() {
            level.distance = step * (i + 1) as f32;
        }
    }
}

impl LodSystem {
    /// Handle entity with corrupted LOD data
    pub fn handle_corrupted_lod(&mut self, entity: Entity, world: &mut World) {
        log::error!("Corrupted LOD data for entity {:?}, resetting", entity);

        if let Some(mut lod) = world.get_mut::<LodGroup>(entity) {
            // Reset to safe state
            lod.current = 0;
            lod.fade_state = None;

            // If no valid levels, remove the component
            if lod.levels.is_empty() {
                world.remove::<LodGroup>(entity);
                log::warn!("Removed empty LodGroup from entity {:?}", entity);
            }
        }
    }

    /// Graceful degradation when LOD system fails
    pub fn fallback_extraction(
        &self,
        world: &World,
        mesh_cache: &MeshCache,
    ) -> Vec<DrawCall> {
        log::warn!("Using LOD fallback extraction");

        let mut draws = Vec::new();

        // Ignore LOD, just use base mesh
        for (entity, (mesh_renderer, transform, material)) in
            world.query::<(&MeshRenderer, &GlobalTransform, &Material)>()
        {
            if let Some(mesh_path) = &mesh_renderer.mesh_asset {
                draws.push(DrawCall {
                    entity_id: entity.id(),
                    mesh_path: mesh_path.clone(),
                    transform: transform.matrix,
                    material: material.clone(),
                    ..Default::default()
                });
            }
        }

        draws
    }
}
```

## Acceptance Criteria

### Functional

- [ ] LOD levels switch based on distance
- [ ] Screen-height-based LOD works
- [ ] Hysteresis prevents popping
- [ ] Crossfade transition works
- [ ] Dither transition works
- [ ] Missing LODs fall back gracefully
- [ ] Auto-LOD generation works
- [ ] Editor shows LOD preview
- [ ] Performance: <0.5ms for 10,000 entities

### Hot-Swap Compliance

- [ ] LodGroup derives Serialize/Deserialize
- [ ] LodLevel derives Serialize/Deserialize
- [ ] LodMode derives Serialize/Deserialize
- [ ] LodGroup implements HotReloadable trait
- [ ] LodSystem implements HotReloadable trait
- [ ] LodGroup implements AssetDependent trait
- [ ] Mesh hot-reload updates LOD meshes
- [ ] LOD updates queued for frame boundary
- [ ] Missing LOD meshes fall back to adjacent levels
- [ ] Corrupted LOD data handled gracefully

## Dependencies

- **Phase 3: Mesh Import** - Multiple mesh assets
- **Phase 14: Spatial Queries** - Distance calculation

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: Medium
**Primary Crates**: void_ecs, void_render
**Reviewer Notes**: Hysteresis values need tuning for smooth transitions
