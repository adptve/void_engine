# Phase 12: Multiple Render Passes per Entity

## Status: Not Started

## User Story

> As a renderer author, I want entities to participate in more than one render pass.

## Requirements Checklist

- [ ] Allow entities to render in: Main pass, Depth-only pass, Shadow pass
- [ ] Explicit pass participation flags
- [ ] Stable ordering guarantees

## Implementation Specification

### 1. Render Pass Flags

```rust
// crates/void_render/src/pass/flags.rs (NEW FILE)

bitflags::bitflags! {
    /// Flags indicating which render passes an entity participates in
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
    pub struct RenderPassFlags: u32 {
        /// Main color pass
        const MAIN = 1 << 0;

        /// Depth prepass (early Z)
        const DEPTH_PREPASS = 1 << 1;

        /// Shadow map passes
        const SHADOW = 1 << 2;

        /// GBuffer pass (deferred)
        const GBUFFER = 1 << 3;

        /// Transparent pass
        const TRANSPARENT = 1 << 4;

        /// Reflection pass
        const REFLECTION = 1 << 5;

        /// Refraction pass
        const REFRACTION = 1 << 6;

        /// Motion vectors pass
        const MOTION_VECTORS = 1 << 7;

        /// Velocity buffer pass
        const VELOCITY = 1 << 8;

        /// Custom pass 0
        const CUSTOM_0 = 1 << 16;
        const CUSTOM_1 = 1 << 17;
        const CUSTOM_2 = 1 << 18;
        const CUSTOM_3 = 1 << 19;

        /// All standard opaque passes
        const OPAQUE = Self::MAIN.bits() | Self::DEPTH_PREPASS.bits() | Self::SHADOW.bits();

        /// All passes
        const ALL = u32::MAX;
    }
}

impl Default for RenderPassFlags {
    fn default() -> Self {
        Self::OPAQUE
    }
}
```

### 2. Render Pass Component

```rust
// crates/void_ecs/src/components/render_pass.rs (NEW FILE)

use crate::RenderPassFlags;

/// Controls which render passes an entity participates in
#[derive(Clone, Debug)]
pub struct RenderPasses {
    /// Pass participation flags
    pub flags: RenderPassFlags,

    /// Per-pass material overrides (pass id -> material)
    pub material_overrides: HashMap<u32, String>,

    /// Per-pass render order offset
    pub order_offsets: HashMap<u32, i32>,

    /// Custom pass names this entity belongs to
    pub custom_passes: Vec<String>,
}

impl Default for RenderPasses {
    fn default() -> Self {
        Self {
            flags: RenderPassFlags::default(),
            material_overrides: HashMap::new(),
            order_offsets: HashMap::new(),
            custom_passes: Vec::new(),
        }
    }
}

impl RenderPasses {
    /// Opaque entity (main + depth + shadow)
    pub fn opaque() -> Self {
        Self {
            flags: RenderPassFlags::OPAQUE,
            ..Default::default()
        }
    }

    /// Transparent entity (main only, no depth write)
    pub fn transparent() -> Self {
        Self {
            flags: RenderPassFlags::MAIN | RenderPassFlags::TRANSPARENT,
            ..Default::default()
        }
    }

    /// Shadow caster only (no main pass)
    pub fn shadow_only() -> Self {
        Self {
            flags: RenderPassFlags::SHADOW,
            ..Default::default()
        }
    }

    /// Add custom pass participation
    pub fn with_custom_pass(mut self, name: impl Into<String>) -> Self {
        self.custom_passes.push(name.into());
        self
    }

    /// Check if entity participates in pass
    pub fn in_pass(&self, pass: RenderPassFlags) -> bool {
        self.flags.contains(pass)
    }
}
```

### 3. Render Pass System

```rust
// crates/void_render/src/pass/system.rs (NEW FILE)

use std::collections::HashMap;
use void_ecs::{World, Entity};
use crate::extraction::DrawCall;

/// Organizes draw calls by render pass
pub struct RenderPassSystem {
    /// Named passes
    passes: HashMap<String, RenderPassData>,

    /// Pass execution order
    order: Vec<String>,
}

struct RenderPassData {
    /// Pass configuration
    config: PassConfig,

    /// Draw calls for this pass
    draw_calls: Vec<DrawCall>,

    /// Is pass enabled
    enabled: bool,
}

#[derive(Clone, Debug)]
pub struct PassConfig {
    /// Pass name
    pub name: String,

    /// Pass flags
    pub flags: RenderPassFlags,

    /// Sort mode
    pub sort: PassSortMode,

    /// Clear color (None = don't clear)
    pub clear_color: Option<[f32; 4]>,

    /// Clear depth
    pub clear_depth: bool,

    /// Depth test
    pub depth_test: bool,

    /// Depth write
    pub depth_write: bool,

    /// Cull mode
    pub cull_mode: CullMode,

    /// Blend mode
    pub blend_mode: Option<BlendMode>,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum PassSortMode {
    /// No sorting
    None,

    /// Front to back (for opaque)
    #[default]
    FrontToBack,

    /// Back to front (for transparent)
    BackToFront,

    /// By material (minimize state changes)
    ByMaterial,

    /// Custom sort key
    Custom,
}

#[derive(Clone, Copy, Debug, Default)]
pub enum CullMode {
    None,
    #[default]
    Back,
    Front,
}

impl RenderPassSystem {
    pub fn new() -> Self {
        let mut system = Self {
            passes: HashMap::new(),
            order: Vec::new(),
        };

        // Register standard passes
        system.register_pass(PassConfig {
            name: "depth_prepass".into(),
            flags: RenderPassFlags::DEPTH_PREPASS,
            sort: PassSortMode::FrontToBack,
            clear_color: None,
            clear_depth: true,
            depth_test: true,
            depth_write: true,
            cull_mode: CullMode::Back,
            blend_mode: None,
        });

        system.register_pass(PassConfig {
            name: "shadow".into(),
            flags: RenderPassFlags::SHADOW,
            sort: PassSortMode::FrontToBack,
            clear_color: None,
            clear_depth: true,
            depth_test: true,
            depth_write: true,
            cull_mode: CullMode::Front,  // Front-face culling for shadows
            blend_mode: None,
        });

        system.register_pass(PassConfig {
            name: "main".into(),
            flags: RenderPassFlags::MAIN,
            sort: PassSortMode::ByMaterial,
            clear_color: Some([0.1, 0.1, 0.1, 1.0]),
            clear_depth: false,  // Use depth prepass
            depth_test: true,
            depth_write: false,  // Already written in prepass
            cull_mode: CullMode::Back,
            blend_mode: None,
        });

        system.register_pass(PassConfig {
            name: "transparent".into(),
            flags: RenderPassFlags::TRANSPARENT,
            sort: PassSortMode::BackToFront,
            clear_color: None,
            clear_depth: false,
            depth_test: true,
            depth_write: false,
            cull_mode: CullMode::None,  // Draw both sides
            blend_mode: Some(BlendMode::Alpha),
        });

        system
    }

    /// Register a render pass
    pub fn register_pass(&mut self, config: PassConfig) {
        let name = config.name.clone();
        self.passes.insert(name.clone(), RenderPassData {
            config,
            draw_calls: Vec::new(),
            enabled: true,
        });
        self.order.push(name);
    }

    /// Clear all draw calls for new frame
    pub fn begin_frame(&mut self) {
        for pass in self.passes.values_mut() {
            pass.draw_calls.clear();
        }
    }

    /// Add draw call to appropriate passes
    pub fn add_draw_call(
        &mut self,
        entity: Entity,
        draw_call: DrawCall,
        passes: &RenderPasses,
    ) {
        for (name, pass) in &mut self.passes {
            if passes.flags.contains(pass.config.flags) ||
               passes.custom_passes.contains(name)
            {
                // Apply material override if present
                let mut call = draw_call.clone();

                let pass_id = pass.config.flags.bits();
                if let Some(mat_override) = passes.material_overrides.get(&pass_id) {
                    call.material_override = Some(mat_override.clone());
                }

                // Apply order offset
                if let Some(offset) = passes.order_offsets.get(&pass_id) {
                    call.sort_key += *offset;
                }

                pass.draw_calls.push(call);
            }
        }
    }

    /// Sort all passes
    pub fn sort_passes(&mut self, camera_pos: [f32; 3]) {
        for pass in self.passes.values_mut() {
            match pass.config.sort {
                PassSortMode::None => {}
                PassSortMode::FrontToBack => {
                    pass.draw_calls.sort_by(|a, b| {
                        a.camera_distance.partial_cmp(&b.camera_distance)
                            .unwrap_or(std::cmp::Ordering::Equal)
                    });
                }
                PassSortMode::BackToFront => {
                    pass.draw_calls.sort_by(|a, b| {
                        b.camera_distance.partial_cmp(&a.camera_distance)
                            .unwrap_or(std::cmp::Ordering::Equal)
                    });
                }
                PassSortMode::ByMaterial => {
                    pass.draw_calls.sort_by_key(|c| c.material_key());
                }
                PassSortMode::Custom => {
                    pass.draw_calls.sort_by_key(|c| c.sort_key);
                }
            }
        }
    }

    /// Get passes in execution order
    pub fn passes_in_order(&self) -> impl Iterator<Item = (&str, &[DrawCall])> {
        self.order.iter().filter_map(|name| {
            self.passes.get(name).filter(|p| p.enabled).map(|p| {
                (name.as_str(), p.draw_calls.as_slice())
            })
        })
    }

    /// Get pass configuration
    pub fn get_pass_config(&self, name: &str) -> Option<&PassConfig> {
        self.passes.get(name).map(|p| &p.config)
    }

    /// Enable/disable pass
    pub fn set_pass_enabled(&mut self, name: &str, enabled: bool) {
        if let Some(pass) = self.passes.get_mut(name) {
            pass.enabled = enabled;
        }
    }
}
```

### 4. Multi-Pass Renderer Integration

```rust
// crates/void_runtime/src/renderer/multi_pass.rs (NEW FILE)

use void_render::pass::{RenderPassSystem, PassConfig};
use wgpu::{CommandEncoder, RenderPass};

/// Executes render passes in order
pub struct MultiPassRenderer {
    pass_system: RenderPassSystem,
    pipelines: HashMap<String, wgpu::RenderPipeline>,
}

impl MultiPassRenderer {
    pub fn new(device: &wgpu::Device, pass_system: RenderPassSystem) -> Self {
        let mut pipelines = HashMap::new();

        // Create pipeline for each pass
        for (name, pass) in pass_system.passes_in_order() {
            let config = pass_system.get_pass_config(name).unwrap();
            let pipeline = Self::create_pipeline(device, config);
            pipelines.insert(name.to_string(), pipeline);
        }

        Self {
            pass_system,
            pipelines,
        }
    }

    pub fn render(
        &mut self,
        encoder: &mut CommandEncoder,
        targets: &RenderTargets,
        camera: &CameraRenderData,
    ) {
        self.pass_system.sort_passes(camera.position);

        for (name, draw_calls) in self.pass_system.passes_in_order() {
            let config = self.pass_system.get_pass_config(name).unwrap();
            let target = self.get_target_for_pass(name, targets);

            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some(name),
                color_attachments: &[target.color.map(|view| {
                    wgpu::RenderPassColorAttachment {
                        view,
                        resolve_target: None,
                        ops: wgpu::Operations {
                            load: config.clear_color.map_or(
                                wgpu::LoadOp::Load,
                                |c| wgpu::LoadOp::Clear(wgpu::Color {
                                    r: c[0] as f64,
                                    g: c[1] as f64,
                                    b: c[2] as f64,
                                    a: c[3] as f64,
                                })
                            ),
                            store: wgpu::StoreOp::Store,
                        },
                    }
                })],
                depth_stencil_attachment: target.depth.map(|view| {
                    wgpu::RenderPassDepthStencilAttachment {
                        view,
                        depth_ops: Some(wgpu::Operations {
                            load: if config.clear_depth {
                                wgpu::LoadOp::Clear(1.0)
                            } else {
                                wgpu::LoadOp::Load
                            },
                            store: wgpu::StoreOp::Store,
                        }),
                        stencil_ops: None,
                    }
                }),
                ..Default::default()
            });

            if let Some(pipeline) = self.pipelines.get(name) {
                pass.set_pipeline(pipeline);
            }

            // Execute draw calls
            for draw_call in draw_calls {
                self.execute_draw_call(&mut pass, draw_call);
            }
        }
    }

    fn create_pipeline(device: &wgpu::Device, config: &PassConfig) -> wgpu::RenderPipeline {
        // Create pipeline based on pass config
        // Depth test, cull mode, blend mode, etc.
        todo!()
    }

    fn execute_draw_call(&self, pass: &mut RenderPass, draw_call: &DrawCall) {
        // Bind resources and draw
        todo!()
    }
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_render/src/pass/flags.rs` | CREATE | Pass flags |
| `void_render/src/pass/system.rs` | CREATE | Pass system |
| `void_ecs/src/components/render_pass.rs` | CREATE | Pass component |
| `void_runtime/src/renderer/multi_pass.rs` | CREATE | Multi-pass renderer |
| `void_render/Cargo.toml` | MODIFY | Add bitflags |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_pass_flags() {
    let flags = RenderPassFlags::OPAQUE;
    assert!(flags.contains(RenderPassFlags::MAIN));
    assert!(flags.contains(RenderPassFlags::SHADOW));
    assert!(!flags.contains(RenderPassFlags::TRANSPARENT));
}

#[test]
fn test_draw_call_distribution() {
    let mut system = RenderPassSystem::new();
    let entity = Entity::from_raw(1);
    let draw_call = DrawCall::default();
    let passes = RenderPasses::opaque();

    system.add_draw_call(entity, draw_call, &passes);

    // Should appear in main, depth_prepass, shadow
    assert!(system.get_pass("main").draw_calls.len() == 1);
    assert!(system.get_pass("shadow").draw_calls.len() == 1);
    assert!(system.get_pass("transparent").draw_calls.is_empty());
}
```

## Hot-Swap Support

### Serialization

All render pass components must be serializable for hot-swap:

```rust
// crates/void_render/src/pass/flags.rs

use serde::{Serialize, Deserialize};

bitflags::bitflags! {
    /// Flags indicating which render passes an entity participates in
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
    pub struct RenderPassFlags: u32 {
        const MAIN = 1 << 0;
        const DEPTH_PREPASS = 1 << 1;
        const SHADOW = 1 << 2;
        const GBUFFER = 1 << 3;
        const TRANSPARENT = 1 << 4;
        const REFLECTION = 1 << 5;
        const REFRACTION = 1 << 6;
        const MOTION_VECTORS = 1 << 7;
        const VELOCITY = 1 << 8;
        const CUSTOM_0 = 1 << 16;
        const CUSTOM_1 = 1 << 17;
        const CUSTOM_2 = 1 << 18;
        const CUSTOM_3 = 1 << 19;
        const OPAQUE = Self::MAIN.bits() | Self::DEPTH_PREPASS.bits() | Self::SHADOW.bits();
        const ALL = u32::MAX;
    }
}

// crates/void_ecs/src/components/render_pass.rs

use serde::{Serialize, Deserialize};
use std::collections::HashMap;

/// Controls which render passes an entity participates in
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RenderPasses {
    pub flags: RenderPassFlags,
    pub material_overrides: HashMap<u32, String>,
    pub order_offsets: HashMap<u32, i32>,
    pub custom_passes: Vec<String>,
}

// crates/void_render/src/pass/system.rs

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PassConfig {
    pub name: String,
    pub flags: RenderPassFlags,
    pub sort: PassSortMode,
    pub clear_color: Option<[f32; 4]>,
    pub clear_depth: bool,
    pub depth_test: bool,
    pub depth_write: bool,
    pub cull_mode: CullMode,
    pub blend_mode: Option<BlendMode>,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum PassSortMode {
    None,
    #[default]
    FrontToBack,
    BackToFront,
    ByMaterial,
    Custom,
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum CullMode {
    None,
    #[default]
    Back,
    Front,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub enum BlendMode {
    Alpha,
    Additive,
    Multiply,
    Premultiplied,
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/pass/system.rs

use void_core::hot_reload::{HotReloadable, HotReloadContext};
use serde::{Serialize, Deserialize};

/// Serializable state for RenderPassSystem
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct RenderPassSystemState {
    /// Pass configurations (name -> config)
    pub pass_configs: HashMap<String, PassConfig>,
    /// Pass execution order
    pub order: Vec<String>,
    /// Enabled state per pass
    pub enabled: HashMap<String, bool>,
}

impl HotReloadable for RenderPassSystem {
    type State = RenderPassSystemState;

    fn save_state(&self) -> Self::State {
        RenderPassSystemState {
            pass_configs: self.passes.iter()
                .map(|(name, data)| (name.clone(), data.config.clone()))
                .collect(),
            order: self.order.clone(),
            enabled: self.passes.iter()
                .map(|(name, data)| (name.clone(), data.enabled))
                .collect(),
        }
    }

    fn restore_state(&mut self, state: Self::State, _ctx: &HotReloadContext) {
        // Restore pass order
        self.order = state.order;

        // Restore configurations and enabled states
        for (name, config) in state.pass_configs {
            if let Some(pass) = self.passes.get_mut(&name) {
                pass.config = config;
                if let Some(&enabled) = state.enabled.get(&name) {
                    pass.enabled = enabled;
                }
            } else {
                // Re-register pass that was added dynamically
                self.register_pass(config);
            }
        }

        // Clear draw calls - will be rebuilt next frame
        for pass in self.passes.values_mut() {
            pass.draw_calls.clear();
        }
    }

    fn version() -> u32 {
        1
    }
}

impl HotReloadable for RenderPasses {
    type State = RenderPasses;

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

// Multi-pass renderer state
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct MultiPassRendererState {
    pub pass_system_state: RenderPassSystemState,
    /// Pipeline configurations (not GPU resources themselves)
    pub pipeline_configs: HashMap<String, PipelineConfig>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PipelineConfig {
    pub depth_test: bool,
    pub depth_write: bool,
    pub cull_mode: CullMode,
    pub blend_mode: Option<BlendMode>,
}

impl HotReloadable for MultiPassRenderer {
    type State = MultiPassRendererState;

    fn save_state(&self) -> Self::State {
        MultiPassRendererState {
            pass_system_state: self.pass_system.save_state(),
            pipeline_configs: self.pass_system.passes_in_order()
                .map(|(name, _)| {
                    let config = self.pass_system.get_pass_config(name).unwrap();
                    (name.to_string(), PipelineConfig {
                        depth_test: config.depth_test,
                        depth_write: config.depth_write,
                        cull_mode: config.cull_mode,
                        blend_mode: config.blend_mode,
                    })
                })
                .collect(),
        }
    }

    fn restore_state(&mut self, state: Self::State, ctx: &HotReloadContext) {
        self.pass_system.restore_state(state.pass_system_state, ctx);
        // Pipelines will be recreated on demand from configs
        self.pipelines_dirty = true;
    }

    fn version() -> u32 {
        1
    }
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/pass/mod.rs

use std::sync::mpsc::{channel, Sender, Receiver};

/// Pending render pass updates to be applied at frame boundary
#[derive(Debug, Clone)]
pub enum RenderPassUpdate {
    /// Update entity's pass participation
    UpdatePasses { entity: Entity, passes: RenderPasses },
    /// Register new custom pass
    RegisterPass { config: PassConfig },
    /// Unregister pass
    UnregisterPass { name: String },
    /// Enable/disable pass
    SetPassEnabled { name: String, enabled: bool },
    /// Update pass configuration
    UpdatePassConfig { name: String, config: PassConfig },
    /// Reorder passes
    SetPassOrder { order: Vec<String> },
    /// Set material override for entity in pass
    SetMaterialOverride { entity: Entity, pass_id: u32, material: String },
}

pub struct RenderPassUpdateQueue {
    sender: Sender<RenderPassUpdate>,
    receiver: Receiver<RenderPassUpdate>,
}

impl RenderPassUpdateQueue {
    pub fn new() -> Self {
        let (sender, receiver) = channel();
        Self { sender, receiver }
    }

    /// Queue update for next frame boundary
    pub fn queue(&self, update: RenderPassUpdate) {
        let _ = self.sender.send(update);
    }

    /// Apply all pending updates (call at frame boundary)
    pub fn apply_pending(
        &self,
        system: &mut RenderPassSystem,
        world: &mut World,
    ) {
        while let Ok(update) = self.receiver.try_recv() {
            match update {
                RenderPassUpdate::UpdatePasses { entity, passes } => {
                    world.insert(entity, passes);
                }
                RenderPassUpdate::RegisterPass { config } => {
                    system.register_pass(config);
                }
                RenderPassUpdate::UnregisterPass { name } => {
                    system.unregister_pass(&name);
                }
                RenderPassUpdate::SetPassEnabled { name, enabled } => {
                    system.set_pass_enabled(&name, enabled);
                }
                RenderPassUpdate::UpdatePassConfig { name, config } => {
                    system.update_pass_config(&name, config);
                }
                RenderPassUpdate::SetPassOrder { order } => {
                    system.set_pass_order(order);
                }
                RenderPassUpdate::SetMaterialOverride { entity, pass_id, material } => {
                    if let Some(mut passes) = world.get_mut::<RenderPasses>(entity) {
                        passes.material_overrides.insert(pass_id, material);
                    }
                }
            }
        }
    }
}

impl RenderPassSystem {
    /// Unregister a pass
    pub fn unregister_pass(&mut self, name: &str) {
        self.passes.remove(name);
        self.order.retain(|n| n != name);
    }

    /// Update pass configuration
    pub fn update_pass_config(&mut self, name: &str, config: PassConfig) {
        if let Some(pass) = self.passes.get_mut(name) {
            pass.config = config;
        }
    }

    /// Set pass execution order
    pub fn set_pass_order(&mut self, order: Vec<String>) {
        self.order = order;
    }
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_render_pass_flags_serialization() {
        let flags = RenderPassFlags::MAIN | RenderPassFlags::SHADOW;

        let json = serde_json::to_string(&flags).unwrap();
        let restored: RenderPassFlags = serde_json::from_str(&json).unwrap();

        assert!(restored.contains(RenderPassFlags::MAIN));
        assert!(restored.contains(RenderPassFlags::SHADOW));
        assert!(!restored.contains(RenderPassFlags::TRANSPARENT));
    }

    #[test]
    fn test_render_passes_component_roundtrip() {
        let mut passes = RenderPasses::opaque();
        passes.material_overrides.insert(RenderPassFlags::SHADOW.bits(), "shadow_mat".into());
        passes.order_offsets.insert(RenderPassFlags::MAIN.bits(), 10);
        passes.custom_passes.push("outline".into());

        let json = serde_json::to_string(&passes).unwrap();
        let restored: RenderPasses = serde_json::from_str(&json).unwrap();

        assert_eq!(passes.flags, restored.flags);
        assert_eq!(passes.material_overrides, restored.material_overrides);
        assert_eq!(passes.custom_passes, restored.custom_passes);
    }

    #[test]
    fn test_pass_config_serialization() {
        let config = PassConfig {
            name: "custom_outline".into(),
            flags: RenderPassFlags::CUSTOM_0,
            sort: PassSortMode::BackToFront,
            clear_color: Some([0.0, 0.0, 0.0, 0.0]),
            clear_depth: true,
            depth_test: true,
            depth_write: false,
            cull_mode: CullMode::None,
            blend_mode: Some(BlendMode::Additive),
        };

        let json = serde_json::to_string(&config).unwrap();
        let restored: PassConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(config.name, restored.name);
        assert_eq!(config.clear_color, restored.clear_color);
    }

    #[test]
    fn test_render_pass_system_state_preservation() {
        let mut system = RenderPassSystem::new();

        // Add custom pass
        system.register_pass(PassConfig {
            name: "outline".into(),
            flags: RenderPassFlags::CUSTOM_0,
            sort: PassSortMode::None,
            clear_color: None,
            clear_depth: false,
            depth_test: true,
            depth_write: false,
            cull_mode: CullMode::Back,
            blend_mode: Some(BlendMode::Alpha),
        });

        system.set_pass_enabled("shadow", false);

        let state = system.save_state();
        let mut new_system = RenderPassSystem::new();
        new_system.restore_state(state, &HotReloadContext::default());

        assert!(new_system.get_pass_config("outline").is_some());
        // Verify shadow is disabled
        let passes: Vec<_> = new_system.passes_in_order().collect();
        assert!(!passes.iter().any(|(name, _)| *name == "shadow"));
    }

    #[test]
    fn test_all_sort_modes_serialize() {
        let modes = [
            PassSortMode::None,
            PassSortMode::FrontToBack,
            PassSortMode::BackToFront,
            PassSortMode::ByMaterial,
            PassSortMode::Custom,
        ];

        for mode in modes {
            let json = serde_json::to_string(&mode).unwrap();
            let restored: PassSortMode = serde_json::from_str(&json).unwrap();
            assert_eq!(format!("{:?}", mode), format!("{:?}", restored));
        }
    }

    #[test]
    fn test_all_blend_modes_serialize() {
        let modes = [
            BlendMode::Alpha,
            BlendMode::Additive,
            BlendMode::Multiply,
            BlendMode::Premultiplied,
        ];

        for mode in modes {
            let json = serde_json::to_string(&mode).unwrap();
            let restored: BlendMode = serde_json::from_str(&json).unwrap();
            assert_eq!(format!("{:?}", mode), format!("{:?}", restored));
        }
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_render/src/pass/system.rs

use std::panic::{catch_unwind, AssertUnwindSafe};
use log::{error, warn};

impl RenderPassSystem {
    /// Add draw call with fault tolerance
    pub fn add_draw_call_safe(
        &mut self,
        entity: Entity,
        draw_call: DrawCall,
        passes: &RenderPasses,
    ) {
        let result = catch_unwind(AssertUnwindSafe(|| {
            self.add_draw_call(entity, draw_call.clone(), passes)
        }));

        if let Err(e) = result {
            error!(
                "Failed to add draw call for entity {:?}: {:?}",
                entity,
                e.downcast_ref::<&str>()
            );
            // Fallback: add to main pass only
            if let Some(main_pass) = self.passes.get_mut("main") {
                main_pass.draw_calls.push(draw_call);
            }
        }
    }

    /// Sort passes with fault tolerance
    pub fn sort_passes_safe(&mut self, camera_pos: [f32; 3]) {
        for (name, pass) in &mut self.passes {
            let result = catch_unwind(AssertUnwindSafe(|| {
                Self::sort_pass_draw_calls(pass, camera_pos)
            }));

            if let Err(e) = result {
                warn!(
                    "Failed to sort pass '{}': {:?}. Using unsorted.",
                    name,
                    e.downcast_ref::<&str>()
                );
                // Leave draw calls unsorted rather than crashing
            }
        }
    }

    fn sort_pass_draw_calls(pass: &mut RenderPassData, camera_pos: [f32; 3]) {
        match pass.config.sort {
            PassSortMode::None => {}
            PassSortMode::FrontToBack => {
                pass.draw_calls.sort_by(|a, b| {
                    a.camera_distance.partial_cmp(&b.camera_distance)
                        .unwrap_or(std::cmp::Ordering::Equal)
                });
            }
            PassSortMode::BackToFront => {
                pass.draw_calls.sort_by(|a, b| {
                    b.camera_distance.partial_cmp(&a.camera_distance)
                        .unwrap_or(std::cmp::Ordering::Equal)
                });
            }
            PassSortMode::ByMaterial => {
                pass.draw_calls.sort_by_key(|c| c.material_key());
            }
            PassSortMode::Custom => {
                pass.draw_calls.sort_by_key(|c| c.sort_key);
            }
        }
    }
}

// crates/void_runtime/src/renderer/multi_pass.rs

impl MultiPassRenderer {
    /// Render all passes with fault tolerance
    pub fn render_safe(
        &mut self,
        encoder: &mut CommandEncoder,
        targets: &RenderTargets,
        camera: &CameraRenderData,
    ) {
        self.pass_system.sort_passes_safe(camera.position);

        for (name, draw_calls) in self.pass_system.passes_in_order() {
            let result = catch_unwind(AssertUnwindSafe(|| {
                self.render_pass(encoder, targets, camera, name, draw_calls)
            }));

            if let Err(e) = result {
                error!(
                    "Pass '{}' failed to render: {:?}. Skipping.",
                    name,
                    e.downcast_ref::<&str>()
                );
                // Continue with next pass rather than aborting entire frame
            }
        }
    }

    /// Execute single draw call with error handling
    fn execute_draw_call_safe(
        &self,
        pass: &mut RenderPass,
        draw_call: &DrawCall,
    ) -> bool {
        let result = catch_unwind(AssertUnwindSafe(|| {
            self.execute_draw_call(pass, draw_call)
        }));

        match result {
            Ok(_) => true,
            Err(_) => {
                warn!("Draw call failed, skipping");
                false
            }
        }
    }
}
```

### Degradation Behavior

```rust
// crates/void_render/src/pass/system.rs

/// Render pass quality levels for graceful degradation
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum PassQuality {
    /// All passes enabled
    Full,
    /// Skip optional passes (reflection, refraction)
    Essential,
    /// Main and depth only
    Minimal,
    /// Main pass only
    Emergency,
}

impl RenderPassSystem {
    quality: PassQuality,
    error_count: u32,
    frame_count: u32,

    /// Check health and potentially degrade quality
    pub fn check_health_and_degrade(&mut self) {
        const ERROR_THRESHOLD: f32 = 0.05; // 5% error rate

        if self.frame_count == 0 {
            return;
        }

        let error_rate = self.error_count as f32 / self.frame_count as f32;

        if error_rate > ERROR_THRESHOLD {
            self.quality = match self.quality {
                PassQuality::Full => {
                    warn!("High render error rate, switching to essential passes");
                    self.disable_optional_passes();
                    PassQuality::Essential
                }
                PassQuality::Essential => {
                    warn!("Continued errors, switching to minimal passes");
                    self.disable_non_essential_passes();
                    PassQuality::Minimal
                }
                PassQuality::Minimal => {
                    error!("Critical render errors, emergency mode");
                    self.enable_only_main_pass();
                    PassQuality::Emergency
                }
                PassQuality::Emergency => PassQuality::Emergency,
            };
            self.error_count = 0;
            self.frame_count = 0;
        }
    }

    fn disable_optional_passes(&mut self) {
        self.set_pass_enabled("reflection", false);
        self.set_pass_enabled("refraction", false);
        self.set_pass_enabled("motion_vectors", false);
    }

    fn disable_non_essential_passes(&mut self) {
        self.disable_optional_passes();
        self.set_pass_enabled("transparent", false);
        self.set_pass_enabled("shadow", false);
    }

    fn enable_only_main_pass(&mut self) {
        for (name, pass) in &mut self.passes {
            pass.enabled = name == "main";
        }
    }

    /// Attempt to restore full quality
    pub fn try_restore_quality(&mut self) {
        if self.quality != PassQuality::Full {
            // Gradually restore passes
            match self.quality {
                PassQuality::Emergency => {
                    self.set_pass_enabled("depth_prepass", true);
                    self.quality = PassQuality::Minimal;
                }
                PassQuality::Minimal => {
                    self.set_pass_enabled("shadow", true);
                    self.set_pass_enabled("transparent", true);
                    self.quality = PassQuality::Essential;
                }
                PassQuality::Essential => {
                    self.set_pass_enabled("reflection", true);
                    self.set_pass_enabled("refraction", true);
                    self.set_pass_enabled("motion_vectors", true);
                    self.quality = PassQuality::Full;
                }
                PassQuality::Full => {}
            }
        }
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Entities can specify pass participation
- [ ] Depth prepass works correctly
- [ ] Shadow pass gets correct entities
- [ ] Transparent pass sorts back-to-front
- [ ] Pass ordering is stable
- [ ] Per-pass material overrides work
- [ ] Custom passes can be registered
- [ ] Performance: minimal overhead per pass

### Hot-Swap Compliance

- [ ] `RenderPassFlags` has `#[derive(Serialize, Deserialize)]`
- [ ] `RenderPasses` component has `#[derive(Serialize, Deserialize)]`
- [ ] `PassConfig` has `#[derive(Serialize, Deserialize)]`
- [ ] `PassSortMode` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `CullMode` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `BlendMode` enum has `#[derive(Serialize, Deserialize)]`
- [ ] `RenderPassSystem` implements `HotReloadable` trait
- [ ] `MultiPassRenderer` implements `HotReloadable` trait
- [ ] Pass configurations preserved across hot-reload
- [ ] Pass order preserved across hot-reload
- [ ] Pass enabled states preserved across hot-reload
- [ ] All hot-swap tests pass
- [ ] `catch_unwind` protects draw call submission
- [ ] `catch_unwind` protects pass sorting
- [ ] Graceful degradation to simpler pass configurations on errors

## Dependencies

- **Phase 6: Shadow Mapping** - Shadow pass usage

## Dependents

- **Phase 13: Custom Render Passes**

---

**Estimated Complexity**: Medium
**Primary Crates**: void_render
**Reviewer Notes**: Ensure pass order dependencies are clear
