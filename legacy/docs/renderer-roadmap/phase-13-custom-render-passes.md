# Phase 13: Custom Render Passes

## Status: Not Started

## User Story

> As an advanced user, I want to inject custom render passes into the pipeline.

## Requirements Checklist

- [ ] Define named render passes
- [ ] Control pass execution order
- [ ] Allow passes to read previous pass outputs
- [ ] Allow passes to write render targets
- [ ] Enforce resource limits per pass

## Implementation Specification

### 1. Custom Pass Definition

```rust
// crates/void_render/src/pass/custom.rs (NEW FILE)

use std::any::Any;
use wgpu::{CommandEncoder, Device, Queue, TextureView};

/// Trait for implementing custom render passes
pub trait CustomRenderPass: Send + Sync {
    /// Pass name (unique identifier)
    fn name(&self) -> &str;

    /// Dependencies on other passes (must run after these)
    fn dependencies(&self) -> &[&str] {
        &[]
    }

    /// Resources this pass reads from
    fn reads(&self) -> &[ResourceRef] {
        &[]
    }

    /// Resources this pass writes to
    fn writes(&self) -> &[ResourceRef] {
        &[]
    }

    /// Called once when pass is registered
    fn setup(&mut self, context: &PassSetupContext) -> Result<(), PassError>;

    /// Called each frame to record commands
    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError>;

    /// Called when pass is unregistered
    fn cleanup(&mut self);

    /// GPU resource requirements
    fn resource_requirements(&self) -> ResourceRequirements {
        ResourceRequirements::default()
    }
}

/// Reference to a render resource
#[derive(Clone, Debug)]
pub enum ResourceRef {
    /// Main color buffer
    MainColor,

    /// Main depth buffer
    MainDepth,

    /// GBuffer (albedo, normal, etc.)
    GBuffer(GBufferChannel),

    /// Shadow atlas
    ShadowAtlas,

    /// Named texture
    Texture(String),

    /// Named buffer
    Buffer(String),

    /// Previous frame's color (for temporal effects)
    PreviousColor,
}

#[derive(Clone, Copy, Debug)]
pub enum GBufferChannel {
    Albedo,
    Normal,
    MetallicRoughness,
    Depth,
    Motion,
}

/// Context for pass setup
pub struct PassSetupContext<'a> {
    pub device: &'a Device,
    pub queue: &'a Queue,
    pub surface_format: wgpu::TextureFormat,
    pub surface_size: (u32, u32),
}

/// Context for pass execution
pub struct PassExecuteContext<'a> {
    pub encoder: &'a mut CommandEncoder,
    pub device: &'a Device,
    pub queue: &'a Queue,
    pub resources: &'a PassResources,
    pub camera: &'a CameraRenderData,
    pub time: f32,
    pub delta_time: f32,
}

/// Resources available to passes
pub struct PassResources {
    textures: HashMap<String, TextureView>,
    buffers: HashMap<String, wgpu::Buffer>,
}

impl PassResources {
    pub fn get_texture(&self, name: &str) -> Option<&TextureView> {
        self.textures.get(name)
    }

    pub fn get_buffer(&self, name: &str) -> Option<&wgpu::Buffer> {
        self.buffers.get(name)
    }
}

/// Resource requirements for a pass
#[derive(Clone, Debug, Default)]
pub struct ResourceRequirements {
    /// Estimated GPU memory in bytes
    pub memory_bytes: u64,

    /// Number of render targets
    pub render_targets: u32,

    /// Requires compute capability
    pub compute: bool,

    /// Maximum execution time hint (ms)
    pub time_budget_ms: f32,
}

#[derive(Debug)]
pub enum PassError {
    Setup(String),
    Execute(String),
    Resource(String),
    Budget(String),
}
```

### 2. Pass Registry

```rust
// crates/void_render/src/pass/registry.rs (NEW FILE)

use std::collections::HashMap;
use crate::pass::CustomRenderPass;

/// Manages custom render passes
pub struct PassRegistry {
    /// Registered passes
    passes: HashMap<String, Box<dyn CustomRenderPass>>,

    /// Resolved execution order
    order: Vec<String>,

    /// Pass groups (for enabling/disabling sets)
    groups: HashMap<String, Vec<String>>,

    /// Resource budget
    budget: ResourceBudget,
}

#[derive(Clone, Debug)]
pub struct ResourceBudget {
    /// Maximum GPU memory for custom passes
    pub max_memory_bytes: u64,

    /// Maximum render targets
    pub max_render_targets: u32,

    /// Maximum time per frame (ms)
    pub max_time_ms: f32,

    /// Current usage
    pub used_memory: u64,
    pub used_render_targets: u32,
}

impl Default for ResourceBudget {
    fn default() -> Self {
        Self {
            max_memory_bytes: 512 * 1024 * 1024,  // 512 MB
            max_render_targets: 8,
            max_time_ms: 8.0,  // 8ms budget
            used_memory: 0,
            used_render_targets: 0,
        }
    }
}

impl PassRegistry {
    pub fn new() -> Self {
        Self {
            passes: HashMap::new(),
            order: Vec::new(),
            groups: HashMap::new(),
            budget: ResourceBudget::default(),
        }
    }

    /// Register a custom pass
    pub fn register<P: CustomRenderPass + 'static>(
        &mut self,
        pass: P,
    ) -> Result<(), PassError> {
        let name = pass.name().to_string();
        let reqs = pass.resource_requirements();

        // Check budget
        if self.budget.used_memory + reqs.memory_bytes > self.budget.max_memory_bytes {
            return Err(PassError::Budget(format!(
                "Pass {} requires {} bytes, only {} available",
                name,
                reqs.memory_bytes,
                self.budget.max_memory_bytes - self.budget.used_memory
            )));
        }

        self.budget.used_memory += reqs.memory_bytes;
        self.budget.used_render_targets += reqs.render_targets;

        self.passes.insert(name.clone(), Box::new(pass));
        self.rebuild_order();

        Ok(())
    }

    /// Unregister a pass
    pub fn unregister(&mut self, name: &str) {
        if let Some(pass) = self.passes.remove(name) {
            let reqs = pass.resource_requirements();
            self.budget.used_memory -= reqs.memory_bytes;
            self.budget.used_render_targets -= reqs.render_targets;
            self.rebuild_order();
        }
    }

    /// Rebuild execution order based on dependencies
    fn rebuild_order(&mut self) {
        // Topological sort
        let mut order = Vec::new();
        let mut visited = HashSet::new();
        let mut temp = HashSet::new();

        fn visit(
            name: &str,
            passes: &HashMap<String, Box<dyn CustomRenderPass>>,
            visited: &mut HashSet<String>,
            temp: &mut HashSet<String>,
            order: &mut Vec<String>,
        ) -> Result<(), String> {
            if temp.contains(name) {
                return Err(format!("Circular dependency detected: {}", name));
            }
            if visited.contains(name) {
                return Ok(());
            }

            temp.insert(name.to_string());

            if let Some(pass) = passes.get(name) {
                for dep in pass.dependencies() {
                    visit(dep, passes, visited, temp, order)?;
                }
            }

            temp.remove(name);
            visited.insert(name.to_string());
            order.push(name.to_string());

            Ok(())
        }

        for name in self.passes.keys() {
            if !visited.contains(name) {
                if let Err(e) = visit(name, &self.passes, &mut visited, &mut temp, &mut order) {
                    log::error!("Failed to resolve pass order: {}", e);
                    return;
                }
            }
        }

        self.order = order;
    }

    /// Execute all passes in order
    pub fn execute(&self, context: &mut PassExecuteContext) -> Result<(), PassError> {
        for name in &self.order {
            if let Some(pass) = self.passes.get(name) {
                pass.execute(context)?;
            }
        }
        Ok(())
    }

    /// Group passes for bulk enable/disable
    pub fn create_group(&mut self, group_name: &str, passes: Vec<String>) {
        self.groups.insert(group_name.to_string(), passes);
    }

    /// Get execution order
    pub fn order(&self) -> &[String] {
        &self.order
    }
}
```

### 3. Built-in Custom Passes

```rust
// crates/void_render/src/pass/builtin/mod.rs (NEW FILE)

mod bloom;
mod ssao;
mod outline;
mod fog;

pub use bloom::BloomPass;
pub use ssao::SSAOPass;
pub use outline::OutlinePass;
pub use fog::FogPass;
```

```rust
// crates/void_render/src/pass/builtin/bloom.rs

/// Bloom post-process pass
pub struct BloomPass {
    threshold: f32,
    intensity: f32,
    blur_passes: u32,

    // GPU resources
    threshold_pipeline: Option<wgpu::ComputePipeline>,
    blur_pipeline: Option<wgpu::ComputePipeline>,
    combine_pipeline: Option<wgpu::RenderPipeline>,
    mip_chain: Option<Vec<wgpu::TextureView>>,
}

impl BloomPass {
    pub fn new(threshold: f32, intensity: f32) -> Self {
        Self {
            threshold,
            intensity,
            blur_passes: 5,
            threshold_pipeline: None,
            blur_pipeline: None,
            combine_pipeline: None,
            mip_chain: None,
        }
    }
}

impl CustomRenderPass for BloomPass {
    fn name(&self) -> &str {
        "bloom"
    }

    fn dependencies(&self) -> &[&str] {
        &["main"]  // Runs after main pass
    }

    fn reads(&self) -> &[ResourceRef] {
        &[ResourceRef::MainColor]
    }

    fn writes(&self) -> &[ResourceRef] {
        &[ResourceRef::MainColor]  // Modifies in place
    }

    fn setup(&mut self, context: &PassSetupContext) -> Result<(), PassError> {
        // Create mip chain for blur
        let mip_levels = 5;
        let mut views = Vec::new();

        let texture = context.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("Bloom Mip Chain"),
            size: wgpu::Extent3d {
                width: context.surface_size.0 / 2,
                height: context.surface_size.1 / 2,
                depth_or_array_layers: 1,
            },
            mip_level_count: mip_levels,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba16Float,
            usage: wgpu::TextureUsages::TEXTURE_BINDING |
                   wgpu::TextureUsages::STORAGE_BINDING |
                   wgpu::TextureUsages::RENDER_ATTACHMENT,
            view_formats: &[],
        });

        for i in 0..mip_levels {
            views.push(texture.create_view(&wgpu::TextureViewDescriptor {
                base_mip_level: i,
                mip_level_count: Some(1),
                ..Default::default()
            }));
        }

        self.mip_chain = Some(views);

        // Create pipelines...
        // (shader code omitted for brevity)

        Ok(())
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        let main_color = context.resources.get_texture("main_color")
            .ok_or(PassError::Resource("main_color not found".into()))?;

        // 1. Threshold pass - extract bright pixels
        // 2. Downsample mip chain
        // 3. Blur each mip level
        // 4. Upsample and combine
        // 5. Add back to main color

        Ok(())
    }

    fn cleanup(&mut self) {
        self.threshold_pipeline = None;
        self.blur_pipeline = None;
        self.combine_pipeline = None;
        self.mip_chain = None;
    }

    fn resource_requirements(&self) -> ResourceRequirements {
        ResourceRequirements {
            memory_bytes: 32 * 1024 * 1024,  // ~32MB for mip chain
            render_targets: 1,
            compute: true,
            time_budget_ms: 1.0,
        }
    }
}
```

```rust
// crates/void_render/src/pass/builtin/outline.rs

/// Selection outline pass
pub struct OutlinePass {
    color: [f32; 4],
    width: f32,
    selected_entities: Vec<Entity>,

    pipeline: Option<wgpu::RenderPipeline>,
    stencil_texture: Option<wgpu::Texture>,
}

impl OutlinePass {
    pub fn new(color: [f32; 4], width: f32) -> Self {
        Self {
            color,
            width,
            selected_entities: Vec::new(),
            pipeline: None,
            stencil_texture: None,
        }
    }

    pub fn set_selected(&mut self, entities: Vec<Entity>) {
        self.selected_entities = entities;
    }
}

impl CustomRenderPass for OutlinePass {
    fn name(&self) -> &str {
        "outline"
    }

    fn dependencies(&self) -> &[&str] {
        &["main"]
    }

    fn reads(&self) -> &[ResourceRef] {
        &[ResourceRef::MainColor, ResourceRef::MainDepth]
    }

    fn writes(&self) -> &[ResourceRef] {
        &[ResourceRef::MainColor]
    }

    fn setup(&mut self, context: &PassSetupContext) -> Result<(), PassError> {
        // Create stencil texture for selection mask
        // Create outline pipeline (jump flood or edge detection)
        Ok(())
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        if self.selected_entities.is_empty() {
            return Ok(());
        }

        // 1. Render selected entities to stencil
        // 2. Apply edge detection / dilation
        // 3. Composite outline over main color

        Ok(())
    }

    fn cleanup(&mut self) {
        self.pipeline = None;
        self.stencil_texture = None;
    }
}
```

### 4. Pass Integration

```rust
// crates/void_render/src/graph.rs (modifications)

impl RenderGraph {
    /// Add custom pass to graph
    pub fn add_custom_pass<P: CustomRenderPass + 'static>(&mut self, pass: P) {
        self.custom_passes.register(pass);
    }

    /// Remove custom pass
    pub fn remove_custom_pass(&mut self, name: &str) {
        self.custom_passes.unregister(name);
    }

    /// Execute frame with custom passes
    pub fn execute(&mut self, encoder: &mut CommandEncoder) {
        // ... standard passes ...

        // Custom passes
        let mut context = PassExecuteContext {
            encoder,
            device: &self.device,
            queue: &self.queue,
            resources: &self.resources,
            camera: &self.camera,
            time: self.time,
            delta_time: self.delta_time,
        };

        if let Err(e) = self.custom_passes.execute(&mut context) {
            log::error!("Custom pass failed: {:?}", e);
        }
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_render/src/pass/custom.rs` | CREATE | Custom pass trait |
| `void_render/src/pass/registry.rs` | CREATE | Pass registry |
| `void_render/src/pass/builtin/mod.rs` | CREATE | Built-in passes |
| `void_render/src/pass/builtin/bloom.rs` | CREATE | Bloom pass |
| `void_render/src/pass/builtin/outline.rs` | CREATE | Outline pass |
| `void_render/src/graph.rs` | MODIFY | Custom pass integration |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_pass_dependencies() {
    let mut registry = PassRegistry::new();

    struct PassA;
    impl CustomRenderPass for PassA { fn name(&self) -> &str { "a" } ... }

    struct PassB;
    impl CustomRenderPass for PassB {
        fn name(&self) -> &str { "b" }
        fn dependencies(&self) -> &[&str] { &["a"] }
        ...
    }

    registry.register(PassB)?;
    registry.register(PassA)?;

    assert_eq!(registry.order(), &["a", "b"]);
}

#[test]
fn test_budget_enforcement() {
    let mut registry = PassRegistry::new();
    registry.budget.max_memory_bytes = 100;

    struct BigPass;
    impl CustomRenderPass for BigPass {
        fn resource_requirements(&self) -> ResourceRequirements {
            ResourceRequirements { memory_bytes: 200, ..default() }
        }
        ...
    }

    assert!(registry.register(BigPass).is_err());
}
```

## Hot-Swap Support

### Serialization

All pass configuration and state must be serializable for hot-swap:

```rust
use serde::{Deserialize, Serialize};

/// Serializable pass configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BloomPassConfig {
    pub threshold: f32,
    pub intensity: f32,
    pub blur_passes: u32,
}

/// Serializable resource budget
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ResourceBudget {
    pub max_memory_bytes: u64,
    pub max_render_targets: u32,
    pub max_time_ms: f32,
    #[serde(skip)]
    pub used_memory: u64,
    #[serde(skip)]
    pub used_render_targets: u32,
}

/// Pass registry state for serialization
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PassRegistryState {
    pub pass_configs: HashMap<String, PassConfig>,
    pub groups: HashMap<String, Vec<String>>,
    pub budget: ResourceBudget,
    pub enabled_passes: HashSet<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum PassConfig {
    Bloom(BloomPassConfig),
    Outline(OutlinePassConfig),
    SSAO(SSAOPassConfig),
    Fog(FogPassConfig),
    Custom(serde_json::Value),
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, ReloadContext};

impl HotReloadable for PassRegistry {
    fn type_name(&self) -> &'static str {
        "PassRegistry"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = PassRegistryState {
            pass_configs: self.serialize_pass_configs(),
            groups: self.groups.clone(),
            budget: self.budget.clone(),
            enabled_passes: self.enabled_passes(),
        };
        bincode::serialize(&state).map_err(|e| HotReloadError::Serialize(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> Result<(), HotReloadError> {
        let state: PassRegistryState = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialize(e.to_string()))?;

        // Queue pass re-registration for frame boundary
        ctx.queue_update(PassRegistryUpdate::Restore(state));
        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}

impl HotReloadable for BloomPass {
    fn type_name(&self) -> &'static str {
        "BloomPass"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let config = BloomPassConfig {
            threshold: self.threshold,
            intensity: self.intensity,
            blur_passes: self.blur_passes,
        };
        bincode::serialize(&config).map_err(|e| HotReloadError::Serialize(e.to_string()))
    }

    fn deserialize_state(&mut self, data: &[u8], _ctx: &ReloadContext) -> Result<(), HotReloadError> {
        let config: BloomPassConfig = bincode::deserialize(data)
            .map_err(|e| HotReloadError::Deserialize(e.to_string()))?;

        self.threshold = config.threshold;
        self.intensity = config.intensity;
        self.blur_passes = config.blur_passes;
        // GPU resources will be recreated on next setup()
        Ok(())
    }

    fn version(&self) -> u32 {
        1
    }
}
```

### Shader Hot-Reload

Custom render passes integrate with the shader hot-reload system:

```rust
use void_shader::hot_reload::{ShaderHotReload, ShaderWatcher};

impl ShaderHotReload for BloomPass {
    fn shader_paths(&self) -> Vec<&str> {
        vec![
            "shaders/bloom_threshold.wgsl",
            "shaders/bloom_blur.wgsl",
            "shaders/bloom_combine.wgsl",
        ]
    }

    fn on_shader_changed(&mut self, path: &str, ctx: &mut ShaderReloadContext) -> Result<(), ShaderError> {
        match path {
            p if p.ends_with("bloom_threshold.wgsl") => {
                self.threshold_pipeline = None; // Mark for rebuild
                ctx.queue_pipeline_rebuild("bloom_threshold");
            }
            p if p.ends_with("bloom_blur.wgsl") => {
                self.blur_pipeline = None;
                ctx.queue_pipeline_rebuild("bloom_blur");
            }
            p if p.ends_with("bloom_combine.wgsl") => {
                self.combine_pipeline = None;
                ctx.queue_pipeline_rebuild("bloom_combine");
            }
            _ => {}
        }
        Ok(())
    }

    fn rebuild_pipelines(&mut self, ctx: &PassSetupContext) -> Result<(), PassError> {
        if self.threshold_pipeline.is_none() {
            self.threshold_pipeline = Some(self.create_threshold_pipeline(ctx)?);
        }
        if self.blur_pipeline.is_none() {
            self.blur_pipeline = Some(self.create_blur_pipeline(ctx)?);
        }
        if self.combine_pipeline.is_none() {
            self.combine_pipeline = Some(self.create_combine_pipeline(ctx)?);
        }
        Ok(())
    }
}

/// Register passes for shader watching
impl PassRegistry {
    pub fn register_shader_watchers(&self, watcher: &mut ShaderWatcher) {
        for (name, pass) in &self.passes {
            if let Some(hot_reload) = pass.as_any().downcast_ref::<dyn ShaderHotReload>() {
                for path in hot_reload.shader_paths() {
                    watcher.watch(path, name.clone());
                }
            }
        }
    }
}
```

### Frame-Boundary Updates

Pass updates are queued and applied at frame boundaries:

```rust
/// Update queue for frame-boundary application
pub struct PassUpdateQueue {
    pending: Vec<PassUpdate>,
}

pub enum PassUpdate {
    Register(Box<dyn CustomRenderPass>),
    Unregister(String),
    Configure(String, PassConfig),
    RestoreRegistry(PassRegistryState),
    RebuildPipelines(Vec<String>),
}

impl PassRegistry {
    /// Apply queued updates at frame boundary (safe point)
    pub fn apply_pending_updates(&mut self, setup_ctx: &PassSetupContext) {
        let updates = std::mem::take(&mut self.update_queue.pending);

        for update in updates {
            match update {
                PassUpdate::Register(pass) => {
                    if let Err(e) = self.register_internal(pass, setup_ctx) {
                        log::error!("Failed to register pass: {:?}", e);
                    }
                }
                PassUpdate::Unregister(name) => {
                    self.unregister(&name);
                }
                PassUpdate::Configure(name, config) => {
                    if let Some(pass) = self.passes.get_mut(&name) {
                        pass.apply_config(&config);
                    }
                }
                PassUpdate::RestoreRegistry(state) => {
                    self.restore_from_state(state, setup_ctx);
                }
                PassUpdate::RebuildPipelines(names) => {
                    for name in names {
                        if let Some(pass) = self.passes.get_mut(&name) {
                            if let Err(e) = pass.setup(setup_ctx) {
                                log::error!("Failed to rebuild pass {}: {:?}", name, e);
                            }
                        }
                    }
                }
            }
        }
    }

    /// Queue an update for next frame boundary
    pub fn queue_update(&mut self, update: PassUpdate) {
        self.update_queue.pending.push(update);
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_pass_registry_serialization() {
        let mut registry = PassRegistry::new();
        registry.register(BloomPass::new(1.0, 0.5)).unwrap();
        registry.register(OutlinePass::new([1.0, 0.5, 0.0, 1.0], 2.0)).unwrap();
        registry.create_group("post_effects", vec!["bloom".into(), "outline".into()]);

        // Serialize
        let state = registry.serialize_state().unwrap();

        // Deserialize into new registry
        let mut restored = PassRegistry::new();
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();

        // Verify
        assert_eq!(restored.order().len(), 2);
        assert!(restored.groups.contains_key("post_effects"));
    }

    #[test]
    fn test_bloom_pass_hot_reload() {
        let mut pass = BloomPass::new(1.0, 0.5);
        pass.blur_passes = 7;

        let state = pass.serialize_state().unwrap();

        let mut restored = BloomPass::new(0.0, 0.0);
        let ctx = ReloadContext::default();
        restored.deserialize_state(&state, &ctx).unwrap();

        assert_eq!(restored.threshold, 1.0);
        assert_eq!(restored.intensity, 0.5);
        assert_eq!(restored.blur_passes, 7);
    }

    #[test]
    fn test_shader_hot_reload_marks_pipelines_dirty() {
        let mut pass = BloomPass::new(1.0, 0.5);
        pass.threshold_pipeline = Some(mock_pipeline());

        let mut ctx = ShaderReloadContext::new();
        pass.on_shader_changed("shaders/bloom_threshold.wgsl", &mut ctx).unwrap();

        assert!(pass.threshold_pipeline.is_none());
        assert!(ctx.queued_rebuilds().contains(&"bloom_threshold".to_string()));
    }

    #[test]
    fn test_frame_boundary_update_application() {
        let mut registry = PassRegistry::new();
        registry.queue_update(PassUpdate::Register(Box::new(BloomPass::new(1.0, 0.5))));

        assert!(registry.passes.is_empty()); // Not applied yet

        let ctx = mock_setup_context();
        registry.apply_pending_updates(&ctx);

        assert_eq!(registry.passes.len(), 1);
        assert!(registry.passes.contains_key("bloom"));
    }
}
```

## Fault Tolerance

### Catch Unwind for Pass Execution

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};

impl PassRegistry {
    /// Execute all passes with fault isolation
    pub fn execute_with_recovery(&self, context: &mut PassExecuteContext) -> Result<(), PassError> {
        let mut failed_passes = Vec::new();

        for name in &self.order {
            if let Some(pass) = self.passes.get(name) {
                let result = catch_unwind(AssertUnwindSafe(|| {
                    pass.execute(context)
                }));

                match result {
                    Ok(Ok(())) => {
                        // Pass succeeded
                    }
                    Ok(Err(e)) => {
                        log::error!("Pass {} returned error: {:?}", name, e);
                        failed_passes.push((name.clone(), format!("{:?}", e)));
                    }
                    Err(panic_info) => {
                        log::error!("Pass {} panicked: {:?}", name, panic_info);
                        failed_passes.push((name.clone(), "panic".to_string()));
                        // Continue with other passes
                    }
                }
            }
        }

        if failed_passes.is_empty() {
            Ok(())
        } else {
            Err(PassError::PartialFailure(failed_passes))
        }
    }
}
```

### Fallback/Degradation Behavior

```rust
impl PassRegistry {
    /// Gracefully degrade when passes fail
    pub fn handle_pass_failure(&mut self, pass_name: &str, error: &PassError) {
        log::warn!("Disabling pass {} due to failure: {:?}", pass_name, error);

        // Track failure count
        let failure_count = self.failure_counts.entry(pass_name.to_string())
            .and_modify(|c| *c += 1)
            .or_insert(1);

        if *failure_count >= 3 {
            // Disable pass after repeated failures
            self.disabled_passes.insert(pass_name.to_string());
            log::error!("Pass {} disabled after {} failures", pass_name, failure_count);
        }

        // Attempt to use fallback pass if available
        if let Some(fallback) = self.get_fallback(pass_name) {
            log::info!("Switching to fallback pass: {}", fallback);
            self.active_fallbacks.insert(pass_name.to_string(), fallback.to_string());
        }
    }

    /// Register a fallback pass
    pub fn register_fallback(&mut self, pass_name: &str, fallback_name: &str) {
        self.fallbacks.insert(pass_name.to_string(), fallback_name.to_string());
    }

    /// Check if pass should execute
    fn should_execute(&self, pass_name: &str) -> bool {
        !self.disabled_passes.contains(pass_name)
    }

    /// Reset failure state (e.g., after hot-reload)
    pub fn reset_failure_state(&mut self, pass_name: &str) {
        self.failure_counts.remove(pass_name);
        self.disabled_passes.remove(pass_name);
        self.active_fallbacks.remove(pass_name);
    }
}

/// Simplified bloom fallback that skips blur passes
pub struct BloomPassSimple {
    intensity: f32,
}

impl CustomRenderPass for BloomPassSimple {
    fn name(&self) -> &str { "bloom_simple" }

    fn resource_requirements(&self) -> ResourceRequirements {
        ResourceRequirements {
            memory_bytes: 4 * 1024 * 1024,  // Much smaller
            render_targets: 1,
            compute: false,  // No compute required
            time_budget_ms: 0.2,
        }
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        // Simple additive bloom without blur - fast fallback
        Ok(())
    }

    // ... other trait methods
}
```

## Acceptance Criteria

### Functional

- [ ] Custom passes can be registered
- [ ] Dependency order is respected
- [ ] Passes can read previous outputs
- [ ] Passes can write to targets
- [ ] Resource budgets are enforced
- [ ] Bloom pass works
- [ ] Outline pass works
- [ ] Passes can be hot-reloaded
- [ ] Editor shows pass list

### Hot-Swap Compliance

- [ ] PassRegistry implements HotReloadable trait
- [ ] All pass configs derive Serialize/Deserialize
- [ ] Shader hot-reload triggers pipeline rebuild
- [ ] Pass updates are applied at frame boundaries only
- [ ] Pass state survives hot-reload cycle
- [ ] GPU resources are properly recreated after reload
- [ ] Failing passes are isolated and don't crash renderer
- [ ] Fallback passes activate on repeated failures

## Dependencies

- **Phase 12: Multi-Pass Entities** - Pass system foundation

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: High
**Primary Crates**: void_render
**Reviewer Notes**: Ensure pass dependencies don't create cycles
