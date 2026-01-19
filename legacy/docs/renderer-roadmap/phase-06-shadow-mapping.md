# Phase 6: Shadow Mapping

## Status: Not Started

## User Story

> As a scene author, I want objects to cast and receive shadows from lights.

## Requirements Checklist

- [ ] Enable shadow casting per light
- [ ] Enable shadow receiving per entity
- [ ] Configurable shadow resolution
- [ ] Support directional and spot light shadows
- [ ] Allow shadows to be globally enabled/disabled
- [ ] Reasonable defaults for quality vs performance

## Current State Analysis

### Existing Implementation

The Light component has `cast_shadows` and `shadow_bias` fields, but no actual shadow mapping is implemented. MeshRenderer has `cast_shadows` and `receive_shadows` booleans.

### Gaps
1. No shadow map textures
2. No shadow pass in render graph
3. No cascaded shadow maps for directional lights
4. No shadow filtering (PCF, PCSS)
5. No shadow atlas management
6. No point light shadows (omnidirectional)

## Implementation Specification

### 1. Shadow Map Configuration

```rust
// crates/void_render/src/shadow/config.rs (NEW FILE)

/// Global shadow configuration
#[derive(Clone, Debug)]
pub struct ShadowConfig {
    /// Enable shadows globally
    pub enabled: bool,

    /// Default shadow map resolution
    pub default_resolution: u32,

    /// Maximum shadow maps in atlas
    pub max_shadow_maps: u32,

    /// PCF filter size (1, 3, 5, 7)
    pub pcf_filter_size: u32,

    /// Enable soft shadows (PCSS)
    pub soft_shadows: bool,

    /// Cascade count for directional lights
    pub cascade_count: u32,

    /// Cascade split lambda (0 = linear, 1 = logarithmic)
    pub cascade_lambda: f32,

    /// Shadow distance (max distance for shadows)
    pub shadow_distance: f32,
}

impl Default for ShadowConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            default_resolution: 2048,
            max_shadow_maps: 16,
            pcf_filter_size: 3,
            soft_shadows: false,
            cascade_count: 4,
            cascade_lambda: 0.5,
            shadow_distance: 100.0,
        }
    }
}

/// Per-light shadow settings (stored in Light component)
#[derive(Clone, Debug)]
pub struct LightShadowSettings {
    /// Resolution override (None = use default)
    pub resolution: Option<u32>,

    /// Depth bias to prevent shadow acne
    pub depth_bias: f32,

    /// Normal-based bias
    pub normal_bias: f32,

    /// Near plane for shadow camera
    pub near_plane: f32,

    /// Soft shadow light size (for PCSS)
    pub light_size: f32,
}

impl Default for LightShadowSettings {
    fn default() -> Self {
        Self {
            resolution: None,
            depth_bias: 0.005,
            normal_bias: 0.02,
            near_plane: 0.1,
            light_size: 1.0,
        }
    }
}
```

### 2. Shadow Atlas

```rust
// crates/void_render/src/shadow/atlas.rs (NEW FILE)

use std::collections::HashMap;
use wgpu::{Device, Texture, TextureView};

/// Manages shadow map allocation in a texture atlas
pub struct ShadowAtlas {
    /// Atlas texture (2D array)
    texture: Texture,
    texture_view: TextureView,

    /// Individual layer views for rendering
    layer_views: Vec<TextureView>,

    /// Atlas size
    size: u32,

    /// Number of layers
    layer_count: u32,

    /// Allocated shadow maps (light entity -> layer index)
    allocations: HashMap<Entity, ShadowAllocation>,

    /// Free layer indices
    free_layers: Vec<u32>,
}

#[derive(Clone, Debug)]
pub struct ShadowAllocation {
    /// Layer index in the atlas
    pub layer: u32,

    /// Resolution (for cascades, this is per-cascade)
    pub resolution: u32,

    /// Light entity
    pub light: Entity,

    /// Frame last used (for cleanup)
    pub last_frame: u64,
}

impl ShadowAtlas {
    pub fn new(device: &Device, size: u32, layer_count: u32) -> Self {
        let texture = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("Shadow Atlas"),
            size: wgpu::Extent3d {
                width: size,
                height: size,
                depth_or_array_layers: layer_count,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Depth32Float,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT |
                   wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });

        let texture_view = texture.create_view(&wgpu::TextureViewDescriptor {
            dimension: Some(wgpu::TextureViewDimension::D2Array),
            ..Default::default()
        });

        let layer_views = (0..layer_count)
            .map(|i| {
                texture.create_view(&wgpu::TextureViewDescriptor {
                    dimension: Some(wgpu::TextureViewDimension::D2),
                    base_array_layer: i,
                    array_layer_count: Some(1),
                    ..Default::default()
                })
            })
            .collect();

        let free_layers = (0..layer_count).rev().collect();

        Self {
            texture,
            texture_view,
            layer_views,
            size,
            layer_count,
            allocations: HashMap::new(),
            free_layers,
        }
    }

    /// Allocate shadow map for a light
    pub fn allocate(&mut self, light: Entity, resolution: u32, frame: u64)
        -> Option<&ShadowAllocation>
    {
        // Check if already allocated
        if let Some(alloc) = self.allocations.get_mut(&light) {
            alloc.last_frame = frame;
            return Some(alloc);
        }

        // Allocate new layer
        let layer = self.free_layers.pop()?;

        let alloc = ShadowAllocation {
            layer,
            resolution: resolution.min(self.size),
            light,
            last_frame: frame,
        };

        self.allocations.insert(light, alloc);
        self.allocations.get(&light)
    }

    /// Get shadow map layer view for rendering
    pub fn get_layer_view(&self, layer: u32) -> Option<&TextureView> {
        self.layer_views.get(layer as usize)
    }

    /// Get full atlas view for sampling
    pub fn atlas_view(&self) -> &TextureView {
        &self.texture_view
    }

    /// Clean up unused allocations
    pub fn cleanup(&mut self, current_frame: u64, max_age: u64) {
        let stale: Vec<Entity> = self.allocations
            .iter()
            .filter(|(_, a)| current_frame - a.last_frame > max_age)
            .map(|(e, _)| *e)
            .collect();

        for entity in stale {
            if let Some(alloc) = self.allocations.remove(&entity) {
                self.free_layers.push(alloc.layer);
            }
        }
    }
}
```

### 3. Cascaded Shadow Maps

```rust
// crates/void_render/src/shadow/cascade.rs (NEW FILE)

use void_math::{Mat4, Vec3};

/// Cascade data for directional light shadows
pub struct ShadowCascades {
    /// Cascade count
    pub count: u32,

    /// Split distances (near plane of each cascade)
    pub splits: [f32; 5],  // Up to 4 cascades + far plane

    /// View-projection matrices for each cascade
    pub matrices: [[[f32; 4]; 4]; 4],

    /// Atlas layer indices
    pub layers: [u32; 4],
}

impl ShadowCascades {
    /// Calculate cascade splits using practical split scheme
    pub fn calculate_splits(
        near: f32,
        far: f32,
        cascade_count: u32,
        lambda: f32,  // 0 = linear, 1 = logarithmic
    ) -> [f32; 5] {
        let mut splits = [0.0f32; 5];
        splits[0] = near;

        for i in 1..=cascade_count as usize {
            let p = i as f32 / cascade_count as f32;

            // Logarithmic split
            let log_split = near * (far / near).powf(p);

            // Linear split
            let lin_split = near + (far - near) * p;

            // Blend
            splits[i] = lambda * log_split + (1.0 - lambda) * lin_split;
        }

        splits
    }

    /// Calculate light view-projection for a cascade
    pub fn calculate_cascade_matrix(
        cascade_index: usize,
        splits: &[f32],
        camera_view: &[[f32; 4]; 4],
        camera_proj: &[[f32; 4]; 4],
        light_direction: [f32; 3],
    ) -> [[f32; 4]; 4] {
        let near = splits[cascade_index];
        let far = splits[cascade_index + 1];

        // Get frustum corners in world space
        let corners = frustum_corners_world(camera_view, camera_proj, near, far);

        // Calculate bounding sphere of frustum corners
        let center = corners.iter()
            .fold([0.0f32; 3], |acc, c| [acc[0] + c[0], acc[1] + c[1], acc[2] + c[2]]);
        let center = [center[0] / 8.0, center[1] / 8.0, center[2] / 8.0];

        let radius = corners.iter()
            .map(|c| {
                let dx = c[0] - center[0];
                let dy = c[1] - center[1];
                let dz = c[2] - center[2];
                (dx * dx + dy * dy + dz * dz).sqrt()
            })
            .fold(0.0f32, f32::max);

        // Build light view matrix
        let light_pos = [
            center[0] - light_direction[0] * radius,
            center[1] - light_direction[1] * radius,
            center[2] - light_direction[2] * radius,
        ];

        let view = look_at(light_pos, center, [0.0, 1.0, 0.0]);

        // Orthographic projection
        let proj = orthographic(-radius, radius, -radius, radius, 0.0, radius * 2.0);

        // Texel snapping to prevent shadow swimming
        let shadow_matrix = multiply_mat4(&proj, &view);
        snap_to_texel(shadow_matrix, 2048)  // Assume 2048 resolution
    }
}

fn frustum_corners_world(
    view: &[[f32; 4]; 4],
    proj: &[[f32; 4]; 4],
    near: f32,
    far: f32,
) -> [[f32; 3]; 8] {
    // NDC corners
    let ndc_corners = [
        [-1.0, -1.0, 0.0],
        [ 1.0, -1.0, 0.0],
        [-1.0,  1.0, 0.0],
        [ 1.0,  1.0, 0.0],
        [-1.0, -1.0, 1.0],
        [ 1.0, -1.0, 1.0],
        [-1.0,  1.0, 1.0],
        [ 1.0,  1.0, 1.0],
    ];

    let inv_vp = invert_mat4(&multiply_mat4(proj, view));

    let mut world_corners = [[0.0f32; 3]; 8];
    for (i, ndc) in ndc_corners.iter().enumerate() {
        let clip = [ndc[0], ndc[1], ndc[2], 1.0];
        let world = transform_vec4(&inv_vp, clip);
        world_corners[i] = [
            world[0] / world[3],
            world[1] / world[3],
            world[2] / world[3],
        ];
    }

    world_corners
}
```

### 4. Shadow Pass

```rust
// crates/void_render/src/shadow/pass.rs (NEW FILE)

use wgpu::{CommandEncoder, Device, RenderPass};

/// Renders shadow maps for all shadow-casting lights
pub struct ShadowPass {
    /// Depth-only pipeline
    pipeline: wgpu::RenderPipeline,

    /// Shadow atlas
    atlas: ShadowAtlas,

    /// Cascade data for directional lights
    cascades: HashMap<Entity, ShadowCascades>,

    /// Shadow uniform buffer
    uniform_buffer: wgpu::Buffer,
}

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct ShadowUniforms {
    light_view_proj: [[f32; 4]; 4],
}

impl ShadowPass {
    pub fn new(device: &Device, config: &ShadowConfig) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Shadow Shader"),
            source: wgpu::ShaderSource::Wgsl(include_str!("shaders/shadow.wgsl").into()),
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("Shadow Pipeline Layout"),
            bind_group_layouts: &[],  // Just vertex transform
            push_constants: &[wgpu::PushConstantRange {
                stages: wgpu::ShaderStages::VERTEX,
                range: 0..64,  // mat4x4
            }],
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Shadow Pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vs_main"),
                buffers: &[Vertex::LAYOUT, InstanceData::LAYOUT],
                compilation_options: Default::default(),
            },
            fragment: None,  // Depth-only
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                cull_mode: Some(wgpu::Face::Front),  // Front-face culling for shadows
                ..Default::default()
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::Less,
                stencil: Default::default(),
                bias: wgpu::DepthBiasState {
                    constant: 2,
                    slope_scale: 2.0,
                    clamp: 0.0,
                },
            }),
            multisample: Default::default(),
            multiview: None,
            cache: None,
        });

        let atlas = ShadowAtlas::new(
            device,
            config.default_resolution,
            config.max_shadow_maps,
        );

        let uniform_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Shadow Uniforms"),
            size: std::mem::size_of::<ShadowUniforms>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        Self {
            pipeline,
            atlas,
            cascades: HashMap::new(),
            uniform_buffer,
        }
    }

    /// Render shadow maps for frame
    pub fn render(
        &mut self,
        encoder: &mut CommandEncoder,
        device: &Device,
        queue: &wgpu::Queue,
        shadow_casters: &[ShadowCaster],
        lights: &[ShadowLight],
        frame: u64,
    ) {
        for light in lights {
            if !light.cast_shadows {
                continue;
            }

            match &light.light_type {
                LightType::Directional { .. } => {
                    self.render_directional_shadow(
                        encoder, queue, light, shadow_casters, frame
                    );
                }
                LightType::Spot { .. } => {
                    self.render_spot_shadow(
                        encoder, queue, light, shadow_casters, frame
                    );
                }
                LightType::Point { .. } => {
                    // Point lights need cube maps - Phase 6 extension
                    log::warn!("Point light shadows not yet implemented");
                }
                _ => {}
            }
        }
    }

    fn render_directional_shadow(
        &mut self,
        encoder: &mut CommandEncoder,
        queue: &wgpu::Queue,
        light: &ShadowLight,
        shadow_casters: &[ShadowCaster],
        frame: u64,
    ) {
        // Calculate cascades
        let cascades = self.cascades.entry(light.entity)
            .or_insert_with(|| ShadowCascades::default());

        for cascade_idx in 0..cascades.count as usize {
            let alloc = match self.atlas.allocate(light.entity, 2048, frame) {
                Some(a) => a,
                None => {
                    log::warn!("Shadow atlas full");
                    return;
                }
            };

            let view = self.atlas.get_layer_view(alloc.layer).unwrap();

            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Shadow Pass"),
                color_attachments: &[],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view,
                    depth_ops: Some(wgpu::Operations {
                        load: wgpu::LoadOp::Clear(1.0),
                        store: wgpu::StoreOp::Store,
                    }),
                    stencil_ops: None,
                }),
                ..Default::default()
            });

            pass.set_pipeline(&self.pipeline);

            // Set light view-projection via push constants
            pass.set_push_constants(
                wgpu::ShaderStages::VERTEX,
                0,
                bytemuck::bytes_of(&cascades.matrices[cascade_idx]),
            );

            // Render shadow casters
            for caster in shadow_casters {
                if !caster.cast_shadows {
                    continue;
                }

                // Bind mesh and instance buffers, draw
                pass.set_vertex_buffer(0, caster.vertex_buffer.slice(..));
                pass.set_vertex_buffer(1, caster.instance_buffer.slice(..));

                if let Some(index_buffer) = &caster.index_buffer {
                    pass.set_index_buffer(index_buffer.slice(..), wgpu::IndexFormat::Uint32);
                    pass.draw_indexed(0..caster.index_count, 0, 0..caster.instance_count);
                } else {
                    pass.draw(0..caster.vertex_count, 0..caster.instance_count);
                }
            }
        }
    }
}

/// Data for shadow caster entities
pub struct ShadowCaster {
    pub entity: Entity,
    pub vertex_buffer: wgpu::Buffer,
    pub index_buffer: Option<wgpu::Buffer>,
    pub instance_buffer: wgpu::Buffer,
    pub vertex_count: u32,
    pub index_count: u32,
    pub instance_count: u32,
    pub cast_shadows: bool,
}

/// Light data for shadow rendering
pub struct ShadowLight {
    pub entity: Entity,
    pub light_type: LightType,
    pub position: [f32; 3],
    pub direction: [f32; 3],
    pub cast_shadows: bool,
    pub shadow_settings: LightShadowSettings,
}
```

### 5. Shadow Shader

```wgsl
// crates/void_render/src/shadow/shaders/shadow.wgsl

struct VertexInput {
    @location(0) position: vec3<f32>,
};

struct InstanceInput {
    @location(10) model_matrix_0: vec4<f32>,
    @location(11) model_matrix_1: vec4<f32>,
    @location(12) model_matrix_2: vec4<f32>,
    @location(13) model_matrix_3: vec4<f32>,
};

var<push_constant> light_view_proj: mat4x4<f32>;

@vertex
fn vs_main(vertex: VertexInput, instance: InstanceInput) -> @builtin(position) vec4<f32> {
    let model_matrix = mat4x4<f32>(
        instance.model_matrix_0,
        instance.model_matrix_1,
        instance.model_matrix_2,
        instance.model_matrix_3,
    );

    let world_pos = model_matrix * vec4<f32>(vertex.position, 1.0);
    return light_view_proj * world_pos;
}
```

### 6. Shadow Sampling

```wgsl
// crates/void_runtime/src/shaders/shadow_sampling.wgsl

@group(2) @binding(0)
var shadow_atlas: texture_depth_2d_array;

@group(2) @binding(1)
var shadow_sampler: sampler_comparison;

struct ShadowData {
    matrices: array<mat4x4<f32>, 16>,  // Up to 16 shadow maps
    cascade_splits: vec4<f32>,
    cascade_count: u32,
    _pad: vec3<u32>,
};

@group(2) @binding(2)
var<uniform> shadow_data: ShadowData;

fn calculate_shadow(
    world_pos: vec3<f32>,
    world_normal: vec3<f32>,
    light_index: u32,
    cascade_count: u32,
    view_depth: f32,
) -> f32 {
    // Find appropriate cascade
    var cascade_index = 0u;
    for (var i = 0u; i < cascade_count; i++) {
        if (view_depth > shadow_data.cascade_splits[i]) {
            cascade_index = i;
        }
    }

    let shadow_matrix = shadow_data.matrices[light_index * 4u + cascade_index];

    // Transform to light space
    let light_space = shadow_matrix * vec4<f32>(world_pos, 1.0);
    var proj_coords = light_space.xyz / light_space.w;

    // Transform to [0, 1] UV space
    proj_coords.x = proj_coords.x * 0.5 + 0.5;
    proj_coords.y = proj_coords.y * -0.5 + 0.5;  // Flip Y

    // Out of shadow map bounds
    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 ||
        proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
        proj_coords.z < 0.0 || proj_coords.z > 1.0) {
        return 1.0;
    }

    // PCF sampling
    var shadow = 0.0;
    let texel_size = 1.0 / 2048.0;  // Atlas resolution

    for (var x = -1; x <= 1; x++) {
        for (var y = -1; y <= 1; y++) {
            let offset = vec2<f32>(f32(x), f32(y)) * texel_size;
            let sample_coords = proj_coords.xy + offset;

            shadow += textureSampleCompare(
                shadow_atlas,
                shadow_sampler,
                sample_coords,
                i32(cascade_index),
                proj_coords.z - 0.005  // Bias
            );
        }
    }

    return shadow / 9.0;
}

// Soft shadow (PCSS) - optional quality upgrade
fn calculate_soft_shadow(
    world_pos: vec3<f32>,
    light_pos: vec3<f32>,
    light_size: f32,
    shadow_map_index: u32,
) -> f32 {
    // 1. Blocker search
    // 2. Penumbra estimation
    // 3. PCF with variable kernel
    // Complex - implement if soft_shadows enabled
    return 1.0;
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_render/src/shadow/mod.rs` | CREATE | Shadow module |
| `void_render/src/shadow/config.rs` | CREATE | Shadow configuration |
| `void_render/src/shadow/atlas.rs` | CREATE | Shadow atlas management |
| `void_render/src/shadow/cascade.rs` | CREATE | Cascade shadow maps |
| `void_render/src/shadow/pass.rs` | CREATE | Shadow render pass |
| `void_render/src/shadow/shaders/` | CREATE | Shadow shaders |
| `void_runtime/src/shaders/shadow_sampling.wgsl` | CREATE | Shadow sampling |
| `void_ecs/src/components/light.rs` | MODIFY | Add shadow settings |
| `void_runtime/src/scene_renderer.rs` | MODIFY | Integrate shadow pass |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_cascade_splits() {
    let splits = ShadowCascades::calculate_splits(0.1, 100.0, 4, 0.5);

    assert!(splits[0] < splits[1]);
    assert!(splits[1] < splits[2]);
    assert!(splits[2] < splits[3]);
    assert!(splits[3] < splits[4]);
}

#[test]
fn test_shadow_atlas_allocation() {
    let device = create_test_device();
    let mut atlas = ShadowAtlas::new(&device, 2048, 16);

    let e1 = Entity::from_raw(1);
    let e2 = Entity::from_raw(2);

    let a1 = atlas.allocate(e1, 2048, 0).unwrap();
    let a2 = atlas.allocate(e2, 2048, 0).unwrap();

    assert_ne!(a1.layer, a2.layer);
}

#[test]
fn test_shadow_atlas_cleanup() {
    let device = create_test_device();
    let mut atlas = ShadowAtlas::new(&device, 2048, 2);

    let e1 = Entity::from_raw(1);
    atlas.allocate(e1, 2048, 0);

    // Advance 10 frames
    atlas.cleanup(10, 5);

    // Should have freed the allocation
    let e2 = Entity::from_raw(2);
    let alloc = atlas.allocate(e2, 2048, 10);
    assert!(alloc.is_some());
}
```

### Visual Tests
```rust
#[test]
fn test_shadow_acne() {
    // Render scene, check for shadow acne artifacts
}

#[test]
fn test_peter_panning() {
    // Check shadows don't detach from objects
}

#[test]
fn test_cascade_transitions() {
    // Verify smooth transitions between cascades
}
```

## Performance Considerations

1. **Cascade Updates**: Only update visible cascades
2. **Shadow Caching**: Skip static shadow updates
3. **LOD for Shadows**: Lower-detail meshes for shadow pass
4. **Early Z**: Enable depth-only rendering
5. **Atlas Packing**: Efficient shadow map packing

## Hot-Swap Support

### Serialization

All shadow-related configuration and state supports serde:

```rust
// crates/void_render/src/shadow/config.rs

use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowConfig {
    pub enabled: bool,
    pub default_resolution: u32,
    pub max_shadow_maps: u32,
    pub pcf_filter_size: u32,
    pub soft_shadows: bool,
    pub cascade_count: u32,
    pub cascade_lambda: f32,
    pub shadow_distance: f32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LightShadowSettings {
    pub resolution: Option<u32>,
    pub depth_bias: f32,
    pub normal_bias: f32,
    pub near_plane: f32,
    pub light_size: f32,
}

// crates/void_render/src/shadow/atlas.rs

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowAllocation {
    pub layer: u32,
    pub resolution: u32,
    #[serde(with = "entity_serde")]
    pub light: Entity,
    pub last_frame: u64,
}

// crates/void_render/src/shadow/cascade.rs

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowCascades {
    pub count: u32,
    pub splits: [f32; 5],
    pub matrices: [[[f32; 4]; 4]; 4],
    pub layers: [u32; 4],
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/shadow/atlas.rs

use void_core::hot_reload::{HotReloadable, ReloadContext, ReloadResult};

impl HotReloadable for ShadowAtlas {
    fn type_name(&self) -> &'static str {
        "ShadowAtlas"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = ShadowAtlasState {
            size: self.size,
            layer_count: self.layer_count,
            allocations: self.allocations.clone(),
        };
        bincode::serialize(&state).map_err(HotReloadError::Serialization)
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> ReloadResult {
        let state: ShadowAtlasState = bincode::deserialize(data)?;

        // Queue for recreation at frame boundary
        self.pending_reload = Some(state);

        Ok(())
    }
}

#[derive(Serialize, Deserialize)]
struct ShadowAtlasState {
    size: u32,
    layer_count: u32,
    allocations: HashMap<Entity, ShadowAllocation>,
}

// crates/void_render/src/shadow/pass.rs

impl HotReloadable for ShadowPass {
    fn type_name(&self) -> &'static str {
        "ShadowPass"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = ShadowPassState {
            cascades: self.cascades.clone(),
            config: self.config.clone(),
        };
        bincode::serialize(&state).map_err(HotReloadError::Serialization)
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> ReloadResult {
        let state: ShadowPassState = bincode::deserialize(data)?;

        self.cascades = state.cascades;
        self.pending_config = Some(state.config);

        Ok(())
    }
}

#[derive(Serialize, Deserialize)]
struct ShadowPassState {
    cascades: HashMap<Entity, ShadowCascades>,
    config: ShadowConfig,
}
```

### Shader Hot-Reload

```rust
// crates/void_render/src/shadow/pass.rs

use void_shader::ShaderHotReload;

impl ShaderHotReload for ShadowPass {
    fn shader_paths(&self) -> Vec<&'static str> {
        vec![
            "shaders/shadow.wgsl",
            "shaders/shadow_sampling.wgsl",
        ]
    }

    fn on_shader_changed(&mut self, path: &str, new_source: &str, ctx: &ShaderReloadContext) {
        log::info!("Shadow shader changed: {}", path);

        // Queue pipeline recreation
        self.pending_pipeline_rebuild = true;
        self.pending_shader_source = Some((path.to_string(), new_source.to_string()));
    }

    fn rebuild_pipeline(&mut self, device: &Device) -> Result<(), ShaderError> {
        if !self.pending_pipeline_rebuild {
            return Ok(());
        }

        if let Some((path, source)) = self.pending_shader_source.take() {
            let module = device.create_shader_module(wgpu::ShaderModuleDescriptor {
                label: Some(&format!("Shadow Shader (hot-reload: {})", path)),
                source: wgpu::ShaderSource::Wgsl(source.into()),
            })?;

            // Recreate pipeline with new shader
            self.pipeline = self.create_shadow_pipeline(device, &module)?;

            log::info!("Shadow pipeline rebuilt after shader hot-reload");
        }

        self.pending_pipeline_rebuild = false;
        Ok(())
    }
}
```

### ShadowAtlas Hot-Reload

```rust
// crates/void_render/src/shadow/atlas.rs

impl ShadowAtlas {
    pending_reload: Option<ShadowAtlasState>,

    /// Apply pending hot-reload at frame boundary
    pub fn apply_pending_reload(&mut self, device: &Device) {
        if let Some(state) = self.pending_reload.take() {
            // Check if atlas needs to be resized
            if state.size != self.size || state.layer_count != self.layer_count {
                log::info!(
                    "Recreating shadow atlas: {}x{} with {} layers -> {}x{} with {} layers",
                    self.size, self.size, self.layer_count,
                    state.size, state.size, state.layer_count
                );

                // Create new atlas texture
                let (texture, texture_view, layer_views) =
                    Self::create_texture(device, state.size, state.layer_count);

                self.texture = texture;
                self.texture_view = texture_view;
                self.layer_views = layer_views;
                self.size = state.size;
                self.layer_count = state.layer_count;
            }

            // Restore allocations (shadow maps will be re-rendered)
            self.allocations = state.allocations;
            self.free_layers = (0..self.layer_count)
                .filter(|l| !self.allocations.values().any(|a| a.layer == *l))
                .rev()
                .collect();

            log::info!(
                "Shadow atlas hot-reload complete: {} allocations restored",
                self.allocations.len()
            );
        }
    }

    /// Double-buffered allocation for seamless transition
    pub fn allocate_double_buffered(
        &mut self,
        light: Entity,
        resolution: u32,
        frame: u64,
    ) -> Option<(ShadowAllocation, ShadowAllocation)> {
        // Allocate two layers for double-buffering
        let layer_a = self.free_layers.pop()?;
        let layer_b = self.free_layers.pop()?;

        let alloc_a = ShadowAllocation {
            layer: layer_a,
            resolution: resolution.min(self.size),
            light,
            last_frame: frame,
        };

        let alloc_b = ShadowAllocation {
            layer: layer_b,
            resolution: resolution.min(self.size),
            light,
            last_frame: frame,
        };

        Some((alloc_a, alloc_b))
    }
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/shadow/pass.rs

impl ShadowPass {
    pending_pipeline_rebuild: bool,
    pending_shader_source: Option<(String, String)>,
    pending_config: Option<ShadowConfig>,

    /// Process all pending updates at frame boundary
    pub fn process_frame_boundary(&mut self, device: &Device, queue: &Queue) {
        // Apply hot-reload state
        self.atlas.apply_pending_reload(device);

        // Apply config changes
        if let Some(config) = self.pending_config.take() {
            self.apply_config_changes(device, &config);
            self.config = config;
        }

        // Rebuild pipeline if shader changed
        if let Err(e) = self.rebuild_pipeline(device) {
            log::error!("Failed to rebuild shadow pipeline: {:?}", e);
            // Keep using old pipeline
        }
    }

    fn apply_config_changes(&mut self, device: &Device, new_config: &ShadowConfig) {
        // Check if atlas needs resize
        if new_config.default_resolution != self.config.default_resolution ||
           new_config.max_shadow_maps != self.config.max_shadow_maps {
            // Recreate atlas with new settings
            self.atlas = ShadowAtlas::new(
                device,
                new_config.default_resolution,
                new_config.max_shadow_maps,
            );
        }

        // Update cascade count for existing lights
        if new_config.cascade_count != self.config.cascade_count {
            for cascades in self.cascades.values_mut() {
                cascades.count = new_config.cascade_count;
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
    fn test_shadow_config_serialization() {
        let config = ShadowConfig {
            enabled: true,
            default_resolution: 4096,
            max_shadow_maps: 32,
            pcf_filter_size: 5,
            soft_shadows: true,
            cascade_count: 4,
            cascade_lambda: 0.75,
            shadow_distance: 200.0,
        };

        let json = serde_json::to_string(&config).unwrap();
        let restored: ShadowConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.default_resolution, 4096);
        assert_eq!(restored.soft_shadows, true);
        assert_eq!(restored.cascade_lambda, 0.75);
    }

    #[test]
    fn test_shadow_cascades_serialization() {
        let cascades = ShadowCascades {
            count: 4,
            splits: [0.1, 5.0, 15.0, 50.0, 100.0],
            matrices: [[[0.0; 4]; 4]; 4],
            layers: [0, 1, 2, 3],
        };

        let bytes = bincode::serialize(&cascades).unwrap();
        let restored: ShadowCascades = bincode::deserialize(&bytes).unwrap();

        assert_eq!(restored.count, 4);
        assert_eq!(restored.splits[2], 15.0);
    }

    #[test]
    fn test_shadow_atlas_hot_reload() {
        let device = create_test_device();
        let mut atlas = ShadowAtlas::new(&device, 2048, 16);

        // Allocate some shadows
        let e1 = Entity::from_raw(1);
        atlas.allocate(e1, 2048, 0);

        // Serialize
        let state = atlas.serialize_state().unwrap();

        // Create new atlas and restore
        let mut new_atlas = ShadowAtlas::new(&device, 1024, 8); // Different size
        new_atlas.deserialize_state(&state, &ReloadContext::default()).unwrap();

        assert!(new_atlas.pending_reload.is_some());

        // Apply at frame boundary
        new_atlas.apply_pending_reload(&device);

        // Should be resized to original dimensions
        assert_eq!(new_atlas.size, 2048);
        assert_eq!(new_atlas.layer_count, 16);
        assert!(new_atlas.allocations.contains_key(&e1));
    }

    #[test]
    fn test_shadow_pass_hot_reload() {
        let device = create_test_device();
        let config = ShadowConfig::default();
        let mut pass = ShadowPass::new(&device, &config);

        // Add cascade data
        let e1 = Entity::from_raw(1);
        pass.cascades.insert(e1, ShadowCascades::default());

        // Serialize
        let state = pass.serialize_state().unwrap();

        // Create new pass and restore
        let mut new_pass = ShadowPass::new(&device, &config);
        new_pass.deserialize_state(&state, &ReloadContext::default()).unwrap();

        assert!(new_pass.cascades.contains_key(&e1));
    }

    #[test]
    fn test_shadow_shader_hot_reload() {
        let device = create_test_device();
        let config = ShadowConfig::default();
        let mut pass = ShadowPass::new(&device, &config);

        // Simulate shader change
        let new_shader = r#"
            @vertex
            fn vs_main(@location(0) position: vec3<f32>) -> @builtin(position) vec4<f32> {
                return vec4<f32>(position, 1.0);
            }
        "#;

        pass.on_shader_changed("shaders/shadow.wgsl", new_shader, &ShaderReloadContext::default());

        assert!(pass.pending_pipeline_rebuild);
        assert!(pass.pending_shader_source.is_some());
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_render/src/shadow/pass.rs

impl ShadowPass {
    /// Render shadows with panic recovery
    pub fn render_safe(
        &mut self,
        encoder: &mut CommandEncoder,
        device: &Device,
        queue: &Queue,
        shadow_casters: &[ShadowCaster],
        lights: &[ShadowLight],
        frame: u64,
    ) {
        if let Err(panic) = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.render(encoder, device, queue, shadow_casters, lights, frame);
        })) {
            log::error!("Panic during shadow rendering: {:?}", panic);
            // Shadows will be missing this frame, but rendering continues
            self.render_failed = true;
        }
    }

    /// Cascade calculation with validation
    pub fn calculate_cascades_safe(
        &mut self,
        light: &ShadowLight,
        camera: &Camera,
    ) -> Option<&ShadowCascades> {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.calculate_cascades(light, camera)
        }))
        .map_err(|panic| {
            log::error!(
                "Panic calculating cascades for light {:?}: {:?}",
                light.entity, panic
            );
        })
        .ok()?
    }
}

// crates/void_render/src/shadow/atlas.rs

impl ShadowAtlas {
    /// Allocate with bounds checking
    pub fn allocate_safe(
        &mut self,
        light: Entity,
        resolution: u32,
        frame: u64,
    ) -> Option<&ShadowAllocation> {
        // Validate resolution
        let safe_resolution = resolution.clamp(64, self.size);

        if self.free_layers.is_empty() {
            log::warn!("Shadow atlas exhausted, skipping shadow for light {:?}", light);
            return None;
        }

        self.allocate(light, safe_resolution, frame)
    }
}
```

### Fallback Behavior

```rust
// crates/void_render/src/shadow/pass.rs

impl ShadowPass {
    render_failed: bool,

    /// Check if shadows are available
    pub fn shadows_available(&self) -> bool {
        self.config.enabled && !self.render_failed
    }

    /// Get shadow bind group with fallback
    pub fn shadow_bind_group_safe(&self) -> &wgpu::BindGroup {
        if self.render_failed {
            &self.no_shadow_bind_group
        } else {
            &self.shadow_bind_group
        }
    }

    /// Create no-shadow bind group (all white, no shadows)
    fn create_no_shadow_bind_group(device: &Device, layout: &BindGroupLayout) -> BindGroup {
        // Create 1x1 white depth texture
        let white_texture = device.create_texture(&TextureDescriptor {
            label: Some("No Shadow Texture"),
            size: Extent3d { width: 1, height: 1, depth_or_array_layers: 1 },
            format: TextureFormat::Depth32Float,
            usage: TextureUsages::TEXTURE_BINDING,
            ..Default::default()
        });

        // Initialize to max depth (no shadow)
        // ... create bind group
    }
}

// In shadow_sampling.wgsl, handle missing shadows gracefully:
fn calculate_shadow_safe(
    world_pos: vec3<f32>,
    light_index: u32,
    shadow_enabled: bool,
) -> f32 {
    if (!shadow_enabled) {
        return 1.0;  // No shadow = full light
    }

    // ... normal shadow calculation
}
```

## Acceptance Criteria

### Functional

- [ ] Directional light shadows work
- [ ] Spot light shadows work
- [ ] Cascaded shadow maps for directional lights
- [ ] Per-entity cast/receive shadow flags work
- [ ] Shadow resolution is configurable
- [ ] PCF filtering reduces aliasing
- [ ] Shadow bias prevents acne
- [ ] Normal bias prevents peter-panning
- [ ] Shadows can be globally disabled
- [ ] Performance: <2ms per shadow map

### Hot-Swap Compliance

- [ ] ShadowConfig implements Serialize/Deserialize (serde)
- [ ] LightShadowSettings implements Serialize/Deserialize
- [ ] ShadowCascades implements Serialize/Deserialize
- [ ] ShadowAtlas implements HotReloadable trait
- [ ] ShadowPass implements HotReloadable trait
- [ ] ShadowPass implements ShaderHotReload for shadow.wgsl and shadow_sampling.wgsl
- [ ] Shadow atlas recreated on resolution/count changes during hot-reload
- [ ] Frame-boundary update applies pending configuration changes
- [ ] Shader changes trigger pipeline rebuild at frame boundary
- [ ] Panic recovery disables shadows instead of crashing
- [ ] No-shadow fallback bind group used when render fails
- [ ] Hot-swap tests pass (config serialization, atlas reload, shader hot-reload)

## Dependencies

- **Phase 5: Lighting** - Need lights to cast shadows

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: Very High
**Primary Crates**: void_render
**Reviewer Notes**: Shadow mapping has many edge cases - thorough visual testing required
