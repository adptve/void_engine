# Hot-Swap Compliance Addendum

## Purpose

This addendum ensures all renderer phases comply with the Void GUI v2 core philosophy: **"Everything is Hot-Swappable"**. Every component, asset, shader, and system must support runtime replacement without restart.

Reference: `docs/project-summary.md` Section 2 "Everything is Hot-Swappable"

## Core Requirements

### 1. Serialization Support

All components MUST derive serde traits:

```rust
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LocalTransform {
    pub translation: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
}
```

**Transient State**: Use `#[serde(skip)]` for non-serializable data:

```rust
#[derive(Serialize, Deserialize)]
pub struct MeshRenderer {
    pub mesh_path: String,
    pub material_path: String,

    // GPU handles are rebuilt after reload
    #[serde(skip)]
    pub gpu_mesh: Option<GpuMesh>,

    #[serde(skip)]
    pub gpu_material: Option<GpuMaterial>,
}
```

### 2. HotReloadable Trait

Every component must implement `HotReloadable`:

```rust
pub trait HotReloadable: Sized {
    /// Serialize current state to bytes
    fn snapshot(&self) -> Vec<u8>;

    /// Restore from serialized bytes
    fn restore(bytes: &[u8]) -> Result<Self, HotReloadError>;

    /// Called after restore to rebuild transient state
    fn on_reload(&mut self) {}

    /// Called before snapshot to prepare state
    fn on_unload(&mut self) {}
}
```

### 3. Frame-Boundary Updates

All changes must apply at frame boundaries:

```rust
pub struct FrameBoundaryUpdates {
    /// Pending component updates
    pending_components: Vec<PendingUpdate>,

    /// Pending shader recompilations
    pending_shaders: Vec<PendingShader>,

    /// Pending asset swaps
    pending_assets: Vec<PendingAsset>,
}

impl FrameBoundaryUpdates {
    /// Apply all pending updates (called between frames)
    pub fn apply(&mut self, world: &mut World, device: &wgpu::Device) {
        // 1. Apply component updates
        for update in self.pending_components.drain(..) {
            update.apply(world);
        }

        // 2. Recompile shaders
        for shader in self.pending_shaders.drain(..) {
            shader.recompile(device);
        }

        // 3. Swap assets
        for asset in self.pending_assets.drain(..) {
            asset.swap(world);
        }
    }
}
```

### 4. Rollback Support

All updates must be rollback-capable:

```rust
pub struct RollbackSnapshot {
    /// Serialized world state before update
    world_state: Vec<u8>,

    /// GPU resource handles
    gpu_resources: Vec<GpuResourceHandle>,

    /// Timestamp
    timestamp: Instant,
}

impl RollbackSnapshot {
    pub fn rollback(self, world: &mut World) -> Result<(), RollbackError> {
        // 1. Deserialize world state
        let restored_world: WorldState = bincode::deserialize(&self.world_state)?;

        // 2. Apply to world
        world.restore_from(restored_world);

        // 3. Restore GPU resources
        for handle in self.gpu_resources {
            handle.restore();
        }

        Ok(())
    }
}
```

## Phase-Specific Requirements

### Phase 1: Scene Graph

```rust
// Hot-swap support for hierarchy components

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Parent {
    pub entity: Entity,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Children {
    pub entities: Vec<Entity>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LocalTransform {
    pub translation: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
}

// GlobalTransform is computed, but cache invalidation needed
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct GlobalTransform {
    pub matrix: [[f32; 4]; 4],

    #[serde(skip)]
    pub dirty: bool,  // Mark dirty on reload
}

impl HotReloadable for GlobalTransform {
    fn on_reload(&mut self) {
        self.dirty = true;  // Force recomputation
    }
}
```

**Test Case**:
```rust
#[test]
fn test_hierarchy_hot_reload() {
    let mut world = setup_hierarchy();
    let snapshot = world.snapshot();

    // Simulate hot-reload
    let restored = World::restore(&snapshot).unwrap();

    // Hierarchy should be intact
    assert!(hierarchy_valid(&restored));

    // Transforms should recompute
    TransformPropagationSystem::run(&mut restored);
    assert_transforms_match(&world, &restored);
}
```

### Phase 2: Camera System

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Camera {
    pub projection: Projection,
    pub fov: f32,
    pub near: f32,
    pub far: f32,
    pub aspect: f32,

    #[serde(skip)]
    pub view_matrix: [[f32; 4]; 4],

    #[serde(skip)]
    pub projection_matrix: [[f32; 4]; 4],
}

impl HotReloadable for Camera {
    fn on_reload(&mut self) {
        // Recompute matrices
        self.update_matrices();
    }
}
```

### Phase 3: Mesh Import

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MeshRenderer {
    pub mesh_path: String,

    #[serde(skip)]
    pub mesh_handle: Option<MeshHandle>,

    #[serde(skip)]
    pub pending_load: bool,
}

impl AssetDependent for MeshRenderer {
    fn on_asset_changed(&mut self, asset_id: AssetId, _: u64) {
        // Mesh file changed on disk
        self.mesh_handle = None;
        self.pending_load = true;
    }
}

// Asset hot-reload integration
impl MeshCache {
    pub fn on_file_changed(&mut self, path: &str) {
        if let Some(handle) = self.path_to_handle.get(path) {
            // Mark for reload
            self.pending_reloads.push(*handle);
        }
    }

    pub fn apply_pending_reloads(&mut self, device: &wgpu::Device, queue: &wgpu::Queue) {
        for handle in self.pending_reloads.drain(..) {
            // Reload mesh from disk
            if let Some(mesh) = self.load_mesh(&self.handle_to_path[&handle]) {
                // Double-buffer: keep old until new is ready
                let old = self.meshes.insert(handle, mesh);
                // Old mesh dropped after new is uploaded
                self.pending_drops.push(old);
            }
        }
    }
}
```

### Phase 5: Lighting

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Light {
    pub light_type: LightType,
    pub color: [f32; 3],
    pub intensity: f32,
    pub cast_shadows: bool,
    // All serializable
}

// GPU buffer needs special handling
pub struct LightBuffer {
    // Double-buffered for hot-swap
    buffers: [wgpu::Buffer; 2],
    current: usize,

    // Pending updates
    pending_updates: Vec<LightUpdate>,
}

impl LightBuffer {
    pub fn queue_update(&mut self, update: LightUpdate) {
        self.pending_updates.push(update);
    }

    pub fn apply_at_frame_boundary(&mut self, queue: &wgpu::Queue) {
        // Write to inactive buffer
        let target = 1 - self.current;
        queue.write_buffer(&self.buffers[target], 0, &self.serialize_lights());

        // Swap buffers
        self.current = target;
    }
}
```

### Phase 6: Shadow Mapping

```rust
pub struct ShadowAtlas {
    texture: wgpu::Texture,

    #[serde(skip)]
    pending_resize: Option<(u32, u32)>,
}

impl ShaderHotReload for ShadowPass {
    fn shader_paths(&self) -> Vec<&str> {
        vec!["shaders/shadow.wgsl"]
    }

    fn on_shader_changed(&mut self, _path: &str, new_module: &wgpu::ShaderModule) {
        self.pending_pipeline_rebuild = true;
        self.new_shader_module = Some(new_module.clone());
    }
}
```

### Phase 7: Advanced Materials

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Material {
    pub base_color: [f32; 4],
    pub metallic: f32,
    pub roughness: f32,
    pub normal_map: Option<String>,
    pub emissive: [f32; 3],
    // All fields serializable
}

impl AssetDependent for Material {
    fn on_asset_changed(&mut self, asset_id: AssetId, _: u64) {
        // Texture changed - rebuild GPU material
        self.needs_rebuild = true;
    }

    fn asset_dependencies(&self) -> Vec<AssetId> {
        let mut deps = vec![];
        if let Some(path) = &self.normal_map {
            deps.push(AssetId::from_path(path));
        }
        deps
    }
}
```

### Phase 8: Animation

```rust
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationPlayer {
    pub animations: HashMap<u32, AnimationState>,
    pub speed: f32,
    pub paused: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct AnimationState {
    pub clip_path: String,  // Path, not handle
    pub time: f32,
    pub loop_mode: LoopMode,
    pub weight: f32,
    pub playing: bool,

    #[serde(skip)]
    pub clip_handle: Option<AnimationClipHandle>,
}

impl HotReloadable for AnimationPlayer {
    fn on_reload(&mut self) {
        // Invalidate clip handles, keep playback state
        for (_, state) in &mut self.animations {
            state.clip_handle = None;
        }
    }
}

// Animation clip asset hot-reload
impl AssetDependent for AnimationState {
    fn on_asset_changed(&mut self, _: AssetId, _: u64) {
        // Animation file changed
        self.clip_handle = None;
        // Keep current time for seamless transition
    }
}
```

### Phase 13: Custom Render Passes

```rust
pub trait CustomRenderPass: HotReloadable + ShaderHotReload {
    fn name(&self) -> &str;
    fn execute(&self, context: &RenderContext);

    // Hot-swap support
    fn can_hot_swap(&self) -> bool { true }
    fn prepare_hot_swap(&mut self) -> HotSwapState;
    fn complete_hot_swap(&mut self, state: HotSwapState);
}

// Pass registry supports hot-swap
impl PassRegistry {
    pub fn hot_swap_pass(&mut self, name: &str, new_pass: Box<dyn CustomRenderPass>) {
        if let Some(old_pass) = self.passes.get_mut(name) {
            // Prepare old pass
            let state = old_pass.prepare_hot_swap();

            // Swap
            *old_pass = new_pass;

            // Complete on new pass
            old_pass.complete_hot_swap(state);
        }
    }
}
```

## GPU Resource Hot-Swap

### Double-Buffering Pattern

```rust
pub struct DoubleBuffered<T> {
    buffers: [T; 2],
    current: usize,
}

impl<T> DoubleBuffered<T> {
    pub fn current(&self) -> &T {
        &self.buffers[self.current]
    }

    pub fn pending(&mut self) -> &mut T {
        &mut self.buffers[1 - self.current]
    }

    pub fn swap(&mut self) {
        self.current = 1 - self.current;
    }
}
```

### Pipeline Rebuild Queue

```rust
pub struct PipelineManager {
    pipelines: HashMap<PipelineId, wgpu::RenderPipeline>,
    pending_rebuilds: Vec<PipelineRebuild>,
}

struct PipelineRebuild {
    id: PipelineId,
    new_shader: wgpu::ShaderModule,
    layout: wgpu::PipelineLayout,
}

impl PipelineManager {
    pub fn queue_rebuild(&mut self, id: PipelineId, shader: wgpu::ShaderModule) {
        self.pending_rebuilds.push(PipelineRebuild {
            id,
            new_shader: shader,
            layout: self.pipelines[&id].layout().clone(),
        });
    }

    pub fn apply_pending(&mut self, device: &wgpu::Device) {
        for rebuild in self.pending_rebuilds.drain(..) {
            let new_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                // ... use rebuild.new_shader
            });
            self.pipelines.insert(rebuild.id, new_pipeline);
        }
    }
}
```

## Testing Requirements

Every phase MUST include hot-swap tests:

```rust
#[test]
fn test_component_hot_swap() {
    // 1. Create component
    let original = MyComponent::new();

    // 2. Snapshot
    let bytes = original.snapshot();

    // 3. Modify original (simulate runtime changes)
    // ...

    // 4. Restore
    let mut restored = MyComponent::restore(&bytes).unwrap();
    restored.on_reload();

    // 5. Verify state matches
    assert_eq!(original.significant_state(), restored.significant_state());
}

#[test]
fn test_asset_hot_reload() {
    // 1. Load asset
    let mut renderer = MeshRenderer::new("mesh.glb");

    // 2. Simulate file change
    renderer.on_asset_changed(AssetId::from_path("mesh.glb"), 2);

    // 3. Verify pending reload
    assert!(renderer.pending_load);

    // 4. Apply reload
    renderer.apply_pending_loads(&asset_server);

    // 5. Verify new version loaded
    assert_eq!(renderer.mesh_version(), 2);
}

#[test]
fn test_shader_hot_reload() {
    // 1. Create pass
    let mut pass = LightingPass::new(&device);

    // 2. Simulate shader change
    let new_shader = compile_shader("modified_lighting.wgsl");
    pass.on_shader_changed("lighting.wgsl", &new_shader);

    // 3. Apply at frame boundary
    pass.apply_pending(&device);

    // 4. Verify new pipeline active
    assert!(pass.using_latest_shader());
}

#[test]
fn test_rollback_on_failure() {
    let mut world = setup_world();
    let snapshot = RollbackSnapshot::create(&world);

    // Attempt update that fails
    let result = world.apply_update(bad_update);
    assert!(result.is_err());

    // Rollback
    snapshot.rollback(&mut world).unwrap();

    // Verify original state
    assert_eq!(world.entity_count(), original_count);
}
```

## Acceptance Criteria (All Phases)

Every phase implementation MUST pass:

- [ ] All components derive `Serialize`, `Deserialize`
- [ ] All components implement `HotReloadable`
- [ ] Asset-dependent components implement `AssetDependent`
- [ ] Shader-using passes implement `ShaderHotReload`
- [ ] GPU resources use double-buffering or pending queues
- [ ] All updates apply at frame boundary
- [ ] Rollback works on update failure
- [ ] No visual glitches during hot-swap
- [ ] Hot-swap test passes for each component
- [ ] Asset hot-reload test passes
- [ ] Shader hot-reload test passes
- [ ] Integration test: modify file on disk, verify reload

---

**Document Version**: 1.0
**Applies To**: All 18 renderer phases
**Review Requirement**: Each phase PR must demonstrate hot-swap compliance
