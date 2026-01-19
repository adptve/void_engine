# Phase 5: Lighting Entities (Multiple Lights)

## Status: Not Started

## User Story

> As a scene author, I want to place multiple lights in my scene to shape lighting locally.

## Requirements Checklist

- [ ] Introduce `light` entities
- [ ] Supported light types: Directional, Point, Spot
- [ ] Per-light properties: Color, Intensity, Range, Direction/Cone angles
- [ ] Lights may be parented to entities
- [ ] Support multiple active lights per frame
- [ ] Define hard limits via resource configuration

## Current State Analysis

### Existing Implementation

**void_ecs/src/render_components.rs:**
```rust
pub struct Light {
    pub light_type: LightType,
    pub color: [f32; 3],
    pub intensity: f32,
    pub range: f32,
    pub inner_angle: f32,
    pub outer_angle: f32,
    pub cast_shadows: bool,
}

pub enum LightType {
    Directional,
    Point,
    Spot,
}
```

**void_render/src/extraction.rs:**
```rust
pub struct LightData {
    pub light_type: u32,
    pub position: [f32; 3],
    pub direction: [f32; 3],
    pub color: [f32; 3],
    pub intensity: f32,
    pub range: f32,
    pub inner_cone: f32,
    pub outer_cone: f32,
    pub shadow_enabled: bool,
}
```

### Gaps
1. No GPU uniform buffer for lights
2. No light attenuation curves
3. No light culling (all lights processed)
4. No light limits enforcement
5. No area lights
6. No IES profiles
7. No light layers/masks
8. Limited shader integration

## Implementation Specification

### 1. Enhanced Light Component

```rust
// crates/void_ecs/src/components/light.rs (REFACTOR)

/// Light source component
#[derive(Clone, Debug)]
pub struct Light {
    /// Light type determines behavior
    pub light_type: LightType,

    /// Light color (linear RGB, not sRGB)
    pub color: [f32; 3],

    /// Intensity in lumens (point/spot) or lux (directional)
    pub intensity: f32,

    /// Whether this light casts shadows
    pub cast_shadows: bool,

    /// Shadow bias to prevent acne
    pub shadow_bias: f32,

    /// Shadow normal bias
    pub shadow_normal_bias: f32,

    /// Layer mask (only affects matching layers)
    pub layer_mask: u32,

    /// Light cookie texture (optional)
    pub cookie_texture: Option<String>,

    /// Is this light active?
    pub enabled: bool,
}

#[derive(Clone, Debug)]
pub enum LightType {
    /// Sun-like light, affects entire scene
    Directional {
        /// Angular diameter in degrees (0 = sharp shadows)
        angular_diameter: f32,
    },

    /// Omni-directional point light
    Point {
        /// Maximum range in world units
        range: f32,

        /// Attenuation curve
        attenuation: Attenuation,
    },

    /// Cone-shaped spotlight
    Spot {
        /// Maximum range in world units
        range: f32,

        /// Inner cone angle (full intensity) in radians
        inner_angle: f32,

        /// Outer cone angle (falloff to zero) in radians
        outer_angle: f32,

        /// Attenuation curve
        attenuation: Attenuation,
    },

    /// Rectangle area light (for soft shadows)
    RectArea {
        /// Width in world units
        width: f32,

        /// Height in world units
        height: f32,

        /// Range
        range: f32,
    },
}

/// Light attenuation model
#[derive(Clone, Copy, Debug, Default)]
pub enum Attenuation {
    /// Physically-based inverse square
    #[default]
    InverseSquare,

    /// Linear falloff (range-based)
    Linear,

    /// Custom curve (for artistic control)
    Custom {
        constant: f32,
        linear: f32,
        quadratic: f32,
    },
}

impl Default for Light {
    fn default() -> Self {
        Self {
            light_type: LightType::Point {
                range: 10.0,
                attenuation: Attenuation::InverseSquare,
            },
            color: [1.0, 1.0, 1.0],
            intensity: 1000.0,  // 1000 lumens
            cast_shadows: false,
            shadow_bias: 0.005,
            shadow_normal_bias: 0.02,
            layer_mask: u32::MAX,
            cookie_texture: None,
            enabled: true,
        }
    }
}

impl Light {
    pub fn directional(color: [f32; 3], intensity: f32) -> Self {
        Self {
            light_type: LightType::Directional { angular_diameter: 0.0 },
            color,
            intensity,
            ..Default::default()
        }
    }

    pub fn point(color: [f32; 3], intensity: f32, range: f32) -> Self {
        Self {
            light_type: LightType::Point {
                range,
                attenuation: Attenuation::InverseSquare,
            },
            color,
            intensity,
            ..Default::default()
        }
    }

    pub fn spot(color: [f32; 3], intensity: f32, range: f32, inner_deg: f32, outer_deg: f32) -> Self {
        Self {
            light_type: LightType::Spot {
                range,
                inner_angle: inner_deg.to_radians(),
                outer_angle: outer_deg.to_radians(),
                attenuation: Attenuation::InverseSquare,
            },
            color,
            intensity,
            ..Default::default()
        }
    }
}
```

### 2. Light Uniform Buffer

```rust
// crates/void_render/src/light_buffer.rs (NEW FILE)

use bytemuck::{Pod, Zeroable};
use wgpu::{Buffer, Device, Queue};

/// Maximum lights per category (adjustable via config)
pub const MAX_DIRECTIONAL_LIGHTS: usize = 4;
pub const MAX_POINT_LIGHTS: usize = 256;
pub const MAX_SPOT_LIGHTS: usize = 128;

/// GPU-ready light data (matches shader struct)
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct GpuDirectionalLight {
    pub direction: [f32; 3],
    pub _pad0: f32,
    pub color: [f32; 3],
    pub intensity: f32,
    pub shadow_matrix: [[f32; 4]; 4],  // For shadow mapping
    pub shadow_map_index: i32,
    pub _pad1: [f32; 3],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct GpuPointLight {
    pub position: [f32; 3],
    pub range: f32,
    pub color: [f32; 3],
    pub intensity: f32,
    pub attenuation: [f32; 3],  // constant, linear, quadratic
    pub shadow_map_index: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct GpuSpotLight {
    pub position: [f32; 3],
    pub range: f32,
    pub direction: [f32; 3],
    pub inner_cos: f32,
    pub color: [f32; 3],
    pub outer_cos: f32,
    pub attenuation: [f32; 3],
    pub intensity: f32,
    pub shadow_matrix: [[f32; 4]; 4],
    pub shadow_map_index: i32,
    pub _pad: [f32; 3],
}

/// Light counts for shader
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct LightCounts {
    pub directional_count: u32,
    pub point_count: u32,
    pub spot_count: u32,
    pub _pad: u32,
}

/// Manages GPU light buffers
pub struct LightBuffer {
    /// Directional light buffer
    directional_buffer: Buffer,
    directional_lights: Vec<GpuDirectionalLight>,

    /// Point light buffer
    point_buffer: Buffer,
    point_lights: Vec<GpuPointLight>,

    /// Spot light buffer
    spot_buffer: Buffer,
    spot_lights: Vec<GpuSpotLight>,

    /// Light counts buffer
    counts_buffer: Buffer,
    counts: LightCounts,

    /// Bind group for shader access
    bind_group: wgpu::BindGroup,
    bind_group_layout: wgpu::BindGroupLayout,
}

impl LightBuffer {
    pub fn new(device: &Device) -> Self {
        let directional_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Directional Lights Buffer"),
            size: (MAX_DIRECTIONAL_LIGHTS * std::mem::size_of::<GpuDirectionalLight>()) as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let point_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Point Lights Buffer"),
            size: (MAX_POINT_LIGHTS * std::mem::size_of::<GpuPointLight>()) as u64,
            usage: wgpu::BufferUsages::STORAGE | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let spot_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Spot Lights Buffer"),
            size: (MAX_SPOT_LIGHTS * std::mem::size_of::<GpuSpotLight>()) as u64,
            usage: wgpu::BufferUsages::STORAGE | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let counts_buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Light Counts Buffer"),
            size: std::mem::size_of::<LightCounts>() as u64,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });

        let bind_group_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("Light Bind Group Layout"),
            entries: &[
                // Directional lights (uniform)
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                // Point lights (storage)
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Storage { read_only: true },
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                // Spot lights (storage)
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Storage { read_only: true },
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                // Light counts (uniform)
                wgpu::BindGroupLayoutEntry {
                    binding: 3,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
            ],
        });

        let bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("Light Bind Group"),
            layout: &bind_group_layout,
            entries: &[
                wgpu::BindGroupEntry { binding: 0, resource: directional_buffer.as_entire_binding() },
                wgpu::BindGroupEntry { binding: 1, resource: point_buffer.as_entire_binding() },
                wgpu::BindGroupEntry { binding: 2, resource: spot_buffer.as_entire_binding() },
                wgpu::BindGroupEntry { binding: 3, resource: counts_buffer.as_entire_binding() },
            ],
        });

        Self {
            directional_buffer,
            directional_lights: Vec::with_capacity(MAX_DIRECTIONAL_LIGHTS),
            point_buffer,
            point_lights: Vec::with_capacity(MAX_POINT_LIGHTS),
            spot_buffer,
            spot_lights: Vec::with_capacity(MAX_SPOT_LIGHTS),
            counts_buffer,
            counts: LightCounts { directional_count: 0, point_count: 0, spot_count: 0, _pad: 0 },
            bind_group,
            bind_group_layout,
        }
    }

    pub fn clear(&mut self) {
        self.directional_lights.clear();
        self.point_lights.clear();
        self.spot_lights.clear();
    }

    pub fn add_directional(&mut self, light: GpuDirectionalLight) {
        if self.directional_lights.len() < MAX_DIRECTIONAL_LIGHTS {
            self.directional_lights.push(light);
        }
    }

    pub fn add_point(&mut self, light: GpuPointLight) {
        if self.point_lights.len() < MAX_POINT_LIGHTS {
            self.point_lights.push(light);
        }
    }

    pub fn add_spot(&mut self, light: GpuSpotLight) {
        if self.spot_lights.len() < MAX_SPOT_LIGHTS {
            self.spot_lights.push(light);
        }
    }

    pub fn upload(&mut self, queue: &Queue) {
        // Update counts
        self.counts = LightCounts {
            directional_count: self.directional_lights.len() as u32,
            point_count: self.point_lights.len() as u32,
            spot_count: self.spot_lights.len() as u32,
            _pad: 0,
        };

        // Upload buffers
        if !self.directional_lights.is_empty() {
            queue.write_buffer(&self.directional_buffer, 0,
                bytemuck::cast_slice(&self.directional_lights));
        }
        if !self.point_lights.is_empty() {
            queue.write_buffer(&self.point_buffer, 0,
                bytemuck::cast_slice(&self.point_lights));
        }
        if !self.spot_lights.is_empty() {
            queue.write_buffer(&self.spot_buffer, 0,
                bytemuck::cast_slice(&self.spot_lights));
        }
        queue.write_buffer(&self.counts_buffer, 0,
            bytemuck::bytes_of(&self.counts));
    }

    pub fn bind_group(&self) -> &wgpu::BindGroup {
        &self.bind_group
    }

    pub fn bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.bind_group_layout
    }
}
```

### 3. Light Extraction System

```rust
// crates/void_render/src/light_extraction.rs (NEW FILE)

use crate::light_buffer::*;
use void_ecs::{World, Entity};
use void_ecs::components::{Light, LightType, GlobalTransform, Visible};

pub struct LightExtractor;

impl LightExtractor {
    pub fn extract(world: &World, buffer: &mut LightBuffer, camera_pos: [f32; 3]) {
        buffer.clear();

        for (entity, (light, transform, visible)) in
            world.query::<(&Light, &GlobalTransform, Option<&Visible>)>()
        {
            // Skip disabled or invisible lights
            if !light.enabled {
                continue;
            }
            if visible.map(|v| !v.visible).unwrap_or(false) {
                continue;
            }

            let position = transform.translation();
            let direction = transform.forward();  // -Z in local space

            match &light.light_type {
                LightType::Directional { angular_diameter } => {
                    buffer.add_directional(GpuDirectionalLight {
                        direction: direction,
                        _pad0: 0.0,
                        color: light.color,
                        intensity: light.intensity,
                        shadow_matrix: [[0.0; 4]; 4],  // Set by shadow system
                        shadow_map_index: -1,
                        _pad1: [0.0; 3],
                    });
                }

                LightType::Point { range, attenuation } => {
                    // Distance culling
                    let dist = distance(position, camera_pos);
                    if dist > *range * 2.0 {
                        continue;
                    }

                    let atten = attenuation_coefficients(attenuation, *range);

                    buffer.add_point(GpuPointLight {
                        position,
                        range: *range,
                        color: light.color,
                        intensity: light.intensity,
                        attenuation: atten,
                        shadow_map_index: -1,
                    });
                }

                LightType::Spot { range, inner_angle, outer_angle, attenuation } => {
                    // Distance culling
                    let dist = distance(position, camera_pos);
                    if dist > *range * 2.0 {
                        continue;
                    }

                    let atten = attenuation_coefficients(attenuation, *range);

                    buffer.add_spot(GpuSpotLight {
                        position,
                        range: *range,
                        direction,
                        inner_cos: inner_angle.cos(),
                        color: light.color,
                        outer_cos: outer_angle.cos(),
                        attenuation: atten,
                        intensity: light.intensity,
                        shadow_matrix: [[0.0; 4]; 4],
                        shadow_map_index: -1,
                        _pad: [0.0; 3],
                    });
                }

                LightType::RectArea { .. } => {
                    // Area lights require special handling
                    log::warn!("Area lights not yet implemented");
                }
            }
        }
    }
}

fn attenuation_coefficients(attenuation: &Attenuation, range: f32) -> [f32; 3] {
    match attenuation {
        Attenuation::InverseSquare => {
            // Physically based: I / d^2
            // With range cutoff for performance
            [1.0, 0.0, 1.0]  // constant=1, linear=0, quadratic=1
        }
        Attenuation::Linear => {
            // Linear falloff from 1 at center to 0 at range
            [1.0, 1.0 / range, 0.0]
        }
        Attenuation::Custom { constant, linear, quadratic } => {
            [*constant, *linear, *quadratic]
        }
    }
}

fn distance(a: [f32; 3], b: [f32; 3]) -> f32 {
    let dx = a[0] - b[0];
    let dy = a[1] - b[1];
    let dz = a[2] - b[2];
    (dx * dx + dy * dy + dz * dz).sqrt()
}
```

### 4. Lighting Shader

```wgsl
// crates/void_runtime/src/shaders/lighting.wgsl

struct DirectionalLight {
    direction: vec3<f32>,
    _pad0: f32,
    color: vec3<f32>,
    intensity: f32,
    shadow_matrix: mat4x4<f32>,
    shadow_map_index: i32,
    _pad1: vec3<f32>,
};

struct PointLight {
    position: vec3<f32>,
    range: f32,
    color: vec3<f32>,
    intensity: f32,
    attenuation: vec3<f32>,
    shadow_map_index: i32,
};

struct SpotLight {
    position: vec3<f32>,
    range: f32,
    direction: vec3<f32>,
    inner_cos: f32,
    color: vec3<f32>,
    outer_cos: f32,
    attenuation: vec3<f32>,
    intensity: f32,
    shadow_matrix: mat4x4<f32>,
    shadow_map_index: i32,
    _pad: vec3<f32>,
};

struct LightCounts {
    directional_count: u32,
    point_count: u32,
    spot_count: u32,
    _pad: u32,
};

@group(1) @binding(0)
var<uniform> directional_lights: array<DirectionalLight, 4>;

@group(1) @binding(1)
var<storage, read> point_lights: array<PointLight>;

@group(1) @binding(2)
var<storage, read> spot_lights: array<SpotLight>;

@group(1) @binding(3)
var<uniform> light_counts: LightCounts;

// PBR lighting functions
fn calculate_directional_light(
    light: DirectionalLight,
    N: vec3<f32>,
    V: vec3<f32>,
    albedo: vec3<f32>,
    metallic: f32,
    roughness: f32,
) -> vec3<f32> {
    let L = normalize(-light.direction);
    let H = normalize(V + L);

    let NdotL = max(dot(N, L), 0.0);
    let NdotV = max(dot(N, V), 0.0);
    let NdotH = max(dot(N, H), 0.0);
    let VdotH = max(dot(V, H), 0.0);

    let radiance = light.color * light.intensity;

    // BRDF
    let F0 = mix(vec3<f32>(0.04), albedo, metallic);
    let F = fresnel_schlick(VdotH, F0);
    let D = distribution_ggx(NdotH, roughness);
    let G = geometry_smith(NdotV, NdotL, roughness);

    let numerator = D * G * F;
    let denominator = 4.0 * NdotV * NdotL + 0.0001;
    let specular = numerator / denominator;

    let kS = F;
    let kD = (vec3<f32>(1.0) - kS) * (1.0 - metallic);

    return (kD * albedo / 3.14159 + specular) * radiance * NdotL;
}

fn calculate_point_light(
    light: PointLight,
    world_pos: vec3<f32>,
    N: vec3<f32>,
    V: vec3<f32>,
    albedo: vec3<f32>,
    metallic: f32,
    roughness: f32,
) -> vec3<f32> {
    let L_vec = light.position - world_pos;
    let distance = length(L_vec);

    if (distance > light.range) {
        return vec3<f32>(0.0);
    }

    let L = normalize(L_vec);
    let H = normalize(V + L);

    // Attenuation
    let attenuation = 1.0 / (
        light.attenuation.x +
        light.attenuation.y * distance +
        light.attenuation.z * distance * distance
    );

    // Smooth falloff at range edge
    let range_falloff = 1.0 - smoothstep(light.range * 0.75, light.range, distance);

    let NdotL = max(dot(N, L), 0.0);
    let NdotV = max(dot(N, V), 0.0);
    let NdotH = max(dot(N, H), 0.0);
    let VdotH = max(dot(V, H), 0.0);

    let radiance = light.color * light.intensity * attenuation * range_falloff;

    // BRDF (same as directional)
    let F0 = mix(vec3<f32>(0.04), albedo, metallic);
    let F = fresnel_schlick(VdotH, F0);
    let D = distribution_ggx(NdotH, roughness);
    let G = geometry_smith(NdotV, NdotL, roughness);

    let numerator = D * G * F;
    let denominator = 4.0 * NdotV * NdotL + 0.0001;
    let specular = numerator / denominator;

    let kS = F;
    let kD = (vec3<f32>(1.0) - kS) * (1.0 - metallic);

    return (kD * albedo / 3.14159 + specular) * radiance * NdotL;
}

fn calculate_spot_light(
    light: SpotLight,
    world_pos: vec3<f32>,
    N: vec3<f32>,
    V: vec3<f32>,
    albedo: vec3<f32>,
    metallic: f32,
    roughness: f32,
) -> vec3<f32> {
    let L_vec = light.position - world_pos;
    let distance = length(L_vec);

    if (distance > light.range) {
        return vec3<f32>(0.0);
    }

    let L = normalize(L_vec);

    // Spotlight cone
    let theta = dot(L, normalize(-light.direction));
    let epsilon = light.inner_cos - light.outer_cos;
    let spot_intensity = clamp((theta - light.outer_cos) / epsilon, 0.0, 1.0);

    if (spot_intensity <= 0.0) {
        return vec3<f32>(0.0);
    }

    // Same as point light but with spot intensity
    let H = normalize(V + L);

    let attenuation = 1.0 / (
        light.attenuation.x +
        light.attenuation.y * distance +
        light.attenuation.z * distance * distance
    );

    let range_falloff = 1.0 - smoothstep(light.range * 0.75, light.range, distance);

    let NdotL = max(dot(N, L), 0.0);
    let NdotV = max(dot(N, V), 0.0);
    let NdotH = max(dot(N, H), 0.0);
    let VdotH = max(dot(V, H), 0.0);

    let radiance = light.color * light.intensity * attenuation * range_falloff * spot_intensity;

    let F0 = mix(vec3<f32>(0.04), albedo, metallic);
    let F = fresnel_schlick(VdotH, F0);
    let D = distribution_ggx(NdotH, roughness);
    let G = geometry_smith(NdotV, NdotL, roughness);

    let numerator = D * G * F;
    let denominator = 4.0 * NdotV * NdotL + 0.0001;
    let specular = numerator / denominator;

    let kS = F;
    let kD = (vec3<f32>(1.0) - kS) * (1.0 - metallic);

    return (kD * albedo / 3.14159 + specular) * radiance * NdotL;
}

// Main lighting calculation
fn calculate_lighting(
    world_pos: vec3<f32>,
    N: vec3<f32>,
    V: vec3<f32>,
    albedo: vec3<f32>,
    metallic: f32,
    roughness: f32,
    ao: f32,
) -> vec3<f32> {
    var Lo = vec3<f32>(0.0);

    // Directional lights
    for (var i = 0u; i < light_counts.directional_count; i++) {
        Lo += calculate_directional_light(
            directional_lights[i], N, V, albedo, metallic, roughness
        );
    }

    // Point lights
    for (var i = 0u; i < light_counts.point_count; i++) {
        Lo += calculate_point_light(
            point_lights[i], world_pos, N, V, albedo, metallic, roughness
        );
    }

    // Spot lights
    for (var i = 0u; i < light_counts.spot_count; i++) {
        Lo += calculate_spot_light(
            spot_lights[i], world_pos, N, V, albedo, metallic, roughness
        );
    }

    // Ambient (simple, replace with IBL later)
    let ambient = vec3<f32>(0.03) * albedo * ao;

    return ambient + Lo;
}

// PBR helper functions
fn fresnel_schlick(cos_theta: f32, F0: vec3<f32>) -> vec3<f32> {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

fn distribution_ggx(NdotH: f32, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH2 = NdotH * NdotH;

    let num = a2;
    var denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159 * denom * denom;

    return num / denom;
}

fn geometry_schlick_ggx(NdotV: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;

    let num = NdotV;
    let denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

fn geometry_smith(NdotV: f32, NdotL: f32, roughness: f32) -> f32 {
    let ggx2 = geometry_schlick_ggx(NdotV, roughness);
    let ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/light.rs` | CREATE | Enhanced light component |
| `void_render/src/light_buffer.rs` | CREATE | GPU light buffers |
| `void_render/src/light_extraction.rs` | CREATE | Light extraction system |
| `void_runtime/src/shaders/lighting.wgsl` | CREATE | PBR lighting shader |
| `void_ecs/src/render_components.rs` | MODIFY | Use new Light component |
| `void_render/src/extraction.rs` | MODIFY | Include light extraction |
| `void_runtime/src/scene_renderer.rs` | MODIFY | Bind light buffers |
| `void_editor/src/panels/inspector.rs` | MODIFY | Light component UI |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_light_buffer_limits() {
    let device = create_test_device();
    let mut buffer = LightBuffer::new(&device);

    // Add more than max directional lights
    for i in 0..10 {
        buffer.add_directional(GpuDirectionalLight::default());
    }

    assert_eq!(buffer.directional_lights.len(), MAX_DIRECTIONAL_LIGHTS);
}

#[test]
fn test_point_light_culling() {
    let mut world = World::new();

    // Light at origin with range 10
    world.spawn((
        Light::point([1.0; 3], 1000.0, 10.0),
        GlobalTransform::default(),
    ));

    let mut buffer = LightBuffer::new(&device);

    // Camera far away - light should be culled
    LightExtractor::extract(&world, &mut buffer, [100.0, 0.0, 0.0]);
    assert_eq!(buffer.point_lights.len(), 0);

    // Camera close - light should be included
    LightExtractor::extract(&world, &mut buffer, [5.0, 0.0, 0.0]);
    assert_eq!(buffer.point_lights.len(), 1);
}

#[test]
fn test_spot_light_angles() {
    let light = Light::spot([1.0; 3], 1000.0, 10.0, 15.0, 30.0);

    if let LightType::Spot { inner_angle, outer_angle, .. } = light.light_type {
        assert!((inner_angle - 15.0_f32.to_radians()).abs() < 0.001);
        assert!((outer_angle - 30.0_f32.to_radians()).abs() < 0.001);
    }
}
```

### Visual Tests
```rust
#[test]
fn test_three_point_lighting() {
    // Classic three-point lighting setup
    // Key light, fill light, back light
    // Visual verification required
}
```

## Performance Considerations

1. **Light Culling**: Cull lights by distance before extraction
2. **Clustered Shading**: For massive light counts (Phase 13 extension)
3. **Light Importance**: Sort and limit by contribution
4. **Deferred Path**: Consider deferred lighting for many lights

## Hot-Swap Support

### Serialization

All light-related components support serde for state persistence:

```rust
// crates/void_ecs/src/components/light.rs

use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Light {
    pub light_type: LightType,
    pub color: [f32; 3],
    pub intensity: f32,
    pub cast_shadows: bool,
    pub shadow_bias: f32,
    pub shadow_normal_bias: f32,
    pub layer_mask: u32,
    pub cookie_texture: Option<String>,
    pub enabled: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum LightType {
    Directional { angular_diameter: f32 },
    Point { range: f32, attenuation: Attenuation },
    Spot { range: f32, inner_angle: f32, outer_angle: f32, attenuation: Attenuation },
    RectArea { width: f32, height: f32, range: f32 },
}

#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum Attenuation {
    #[default]
    InverseSquare,
    Linear,
    Custom { constant: f32, linear: f32, quadratic: f32 },
}

// GPU-side light data uses bytemuck for buffer compatibility
#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuDirectionalLight {
    pub direction: [f32; 3],
    pub _pad0: f32,
    pub color: [f32; 3],
    pub intensity: f32,
    #[serde(skip)]  // Shadow matrix computed at runtime
    pub shadow_matrix: [[f32; 4]; 4],
    pub shadow_map_index: i32,
    pub _pad1: [f32; 3],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuPointLight {
    pub position: [f32; 3],
    pub range: f32,
    pub color: [f32; 3],
    pub intensity: f32,
    pub attenuation: [f32; 3],
    pub shadow_map_index: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuSpotLight {
    pub position: [f32; 3],
    pub range: f32,
    pub direction: [f32; 3],
    pub inner_cos: f32,
    pub color: [f32; 3],
    pub outer_cos: f32,
    pub attenuation: [f32; 3],
    pub intensity: f32,
    #[serde(skip)]
    pub shadow_matrix: [[f32; 4]; 4],
    pub shadow_map_index: i32,
    pub _pad: [f32; 3],
}
```

### HotReloadable Implementation

```rust
// crates/void_render/src/light_buffer.rs

use void_core::hot_reload::{HotReloadable, ReloadContext, ReloadResult};

impl HotReloadable for LightBuffer {
    fn type_name(&self) -> &'static str {
        "LightBuffer"
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        let state = LightBufferState {
            directional_lights: self.directional_lights.clone(),
            point_lights: self.point_lights.clone(),
            spot_lights: self.spot_lights.clone(),
        };
        bincode::serialize(&state).map_err(HotReloadError::Serialization)
    }

    fn deserialize_state(&mut self, data: &[u8], ctx: &ReloadContext) -> ReloadResult {
        let state: LightBufferState = bincode::deserialize(data)?;

        // Store for application at frame boundary
        self.pending_state = Some(state);

        Ok(())
    }
}

#[derive(Serialize, Deserialize)]
struct LightBufferState {
    directional_lights: Vec<GpuDirectionalLight>,
    point_lights: Vec<GpuPointLight>,
    spot_lights: Vec<GpuSpotLight>,
}
```

### LightBuffer Double-Buffering

```rust
// crates/void_render/src/light_buffer.rs

pub struct LightBuffer {
    /// Active buffer index (0 or 1)
    active_buffer: usize,

    /// Double-buffered directional light storage
    directional_buffers: [Buffer; 2],
    directional_lights: Vec<GpuDirectionalLight>,

    /// Double-buffered point light storage
    point_buffers: [Buffer; 2],
    point_lights: Vec<GpuPointLight>,

    /// Double-buffered spot light storage
    spot_buffers: [Buffer; 2],
    spot_lights: Vec<GpuSpotLight>,

    /// Counts buffer (single, updated atomically)
    counts_buffer: Buffer,
    counts: LightCounts,

    /// Bind groups for each buffer set
    bind_groups: [wgpu::BindGroup; 2],
    bind_group_layout: wgpu::BindGroupLayout,

    /// Pending state from hot-reload
    pending_state: Option<LightBufferState>,
}

impl LightBuffer {
    pub fn new(device: &Device) -> Self {
        // Create double-buffered resources
        let directional_buffers = [
            Self::create_directional_buffer(device),
            Self::create_directional_buffer(device),
        ];

        let point_buffers = [
            Self::create_point_buffer(device),
            Self::create_point_buffer(device),
        ];

        let spot_buffers = [
            Self::create_spot_buffer(device),
            Self::create_spot_buffer(device),
        ];

        // ... create bind groups for both buffer sets

        Self {
            active_buffer: 0,
            directional_buffers,
            directional_lights: Vec::with_capacity(MAX_DIRECTIONAL_LIGHTS),
            point_buffers,
            point_lights: Vec::with_capacity(MAX_POINT_LIGHTS),
            spot_buffers,
            spot_lights: Vec::with_capacity(MAX_SPOT_LIGHTS),
            counts_buffer: Self::create_counts_buffer(device),
            counts: LightCounts::default(),
            bind_groups,
            bind_group_layout,
            pending_state: None,
        }
    }

    /// Apply pending hot-reload state at frame boundary
    pub fn apply_pending_state(&mut self, queue: &Queue) {
        if let Some(state) = self.pending_state.take() {
            let inactive = 1 - self.active_buffer;

            // Upload to inactive buffers
            self.directional_lights = state.directional_lights;
            self.point_lights = state.point_lights;
            self.spot_lights = state.spot_lights;

            self.upload_to_buffer_set(queue, inactive);

            // Swap active buffer
            self.active_buffer = inactive;

            log::info!("Applied light buffer hot-reload ({} dir, {} point, {} spot)",
                self.directional_lights.len(),
                self.point_lights.len(),
                self.spot_lights.len()
            );
        }
    }

    fn upload_to_buffer_set(&self, queue: &Queue, buffer_idx: usize) {
        if !self.directional_lights.is_empty() {
            queue.write_buffer(
                &self.directional_buffers[buffer_idx],
                0,
                bytemuck::cast_slice(&self.directional_lights),
            );
        }
        if !self.point_lights.is_empty() {
            queue.write_buffer(
                &self.point_buffers[buffer_idx],
                0,
                bytemuck::cast_slice(&self.point_lights),
            );
        }
        if !self.spot_lights.is_empty() {
            queue.write_buffer(
                &self.spot_buffers[buffer_idx],
                0,
                bytemuck::cast_slice(&self.spot_lights),
            );
        }
    }

    /// Get current active bind group
    pub fn bind_group(&self) -> &wgpu::BindGroup {
        &self.bind_groups[self.active_buffer]
    }
}
```

### Frame-Boundary Updates

```rust
// crates/void_render/src/light_buffer.rs

impl LightBuffer {
    /// Process all pending updates at frame boundary
    pub fn process_frame_boundary(&mut self, queue: &Queue) {
        // Apply hot-reload state if pending
        self.apply_pending_state(queue);

        // Process any live light modifications
        if self.lights_dirty {
            let inactive = 1 - self.active_buffer;
            self.upload_to_buffer_set(queue, inactive);
            self.active_buffer = inactive;
            self.lights_dirty = false;
        }
    }

    /// Mark lights as dirty (called when Light component modified in editor)
    pub fn mark_dirty(&mut self) {
        self.lights_dirty = true;
    }
}

// Integration with extraction system
impl LightExtractor {
    pub fn extract_and_notify(
        world: &World,
        buffer: &mut LightBuffer,
        camera_pos: [f32; 3],
    ) {
        let old_counts = (
            buffer.directional_lights.len(),
            buffer.point_lights.len(),
            buffer.spot_lights.len(),
        );

        Self::extract(world, buffer, camera_pos);

        let new_counts = (
            buffer.directional_lights.len(),
            buffer.point_lights.len(),
            buffer.spot_lights.len(),
        );

        // Mark dirty if light counts changed
        if old_counts != new_counts {
            buffer.mark_dirty();
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
    fn test_light_component_serialization() {
        let light = Light {
            light_type: LightType::Point {
                range: 15.0,
                attenuation: Attenuation::InverseSquare,
            },
            color: [1.0, 0.8, 0.6],
            intensity: 2000.0,
            cast_shadows: true,
            shadow_bias: 0.005,
            shadow_normal_bias: 0.02,
            layer_mask: 0xFF,
            cookie_texture: Some("cookies/spot.png".to_string()),
            enabled: true,
        };

        let json = serde_json::to_string(&light).unwrap();
        let restored: Light = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.intensity, 2000.0);
        assert_eq!(restored.cookie_texture, Some("cookies/spot.png".to_string()));
        if let LightType::Point { range, .. } = restored.light_type {
            assert_eq!(range, 15.0);
        } else {
            panic!("Wrong light type");
        }
    }

    #[test]
    fn test_gpu_light_serialization() {
        let gpu_light = GpuPointLight {
            position: [1.0, 2.0, 3.0],
            range: 10.0,
            color: [1.0, 1.0, 1.0],
            intensity: 1000.0,
            attenuation: [1.0, 0.0, 1.0],
            shadow_map_index: -1,
        };

        let bytes = bincode::serialize(&gpu_light).unwrap();
        let restored: GpuPointLight = bincode::deserialize(&bytes).unwrap();

        assert_eq!(restored.position, [1.0, 2.0, 3.0]);
        assert_eq!(restored.intensity, 1000.0);
    }

    #[test]
    fn test_light_buffer_hot_reload() {
        let device = create_test_device();
        let queue = create_test_queue();
        let mut buffer = LightBuffer::new(&device);

        // Add some lights
        buffer.add_directional(GpuDirectionalLight::default());
        buffer.add_point(GpuPointLight::default());
        buffer.upload(&queue);

        // Serialize state
        let state = buffer.serialize_state().unwrap();

        // Create new buffer and restore
        let mut new_buffer = LightBuffer::new(&device);
        new_buffer.deserialize_state(&state, &ReloadContext::default()).unwrap();

        assert!(new_buffer.pending_state.is_some());

        // Apply at frame boundary
        new_buffer.apply_pending_state(&queue);

        assert!(new_buffer.pending_state.is_none());
        assert_eq!(new_buffer.directional_lights.len(), 1);
        assert_eq!(new_buffer.point_lights.len(), 1);
    }

    #[test]
    fn test_light_buffer_double_buffering() {
        let device = create_test_device();
        let queue = create_test_queue();
        let mut buffer = LightBuffer::new(&device);

        let initial_active = buffer.active_buffer;

        // Modify and apply
        buffer.add_point(GpuPointLight::default());
        buffer.mark_dirty();
        buffer.process_frame_boundary(&queue);

        // Buffer should have swapped
        assert_ne!(buffer.active_buffer, initial_active);
    }
}
```

## Fault Tolerance

### Critical Operation Protection

```rust
// crates/void_render/src/light_extraction.rs

impl LightExtractor {
    /// Extract lights with panic recovery
    pub fn extract_safe(world: &World, buffer: &mut LightBuffer, camera_pos: [f32; 3]) {
        if let Err(panic) = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            Self::extract(world, buffer, camera_pos);
        })) {
            log::error!("Panic during light extraction: {:?}", panic);
            // Clear buffer and add fallback ambient light
            buffer.clear();
            buffer.add_directional(GpuDirectionalLight {
                direction: [0.0, -1.0, 0.0],
                _pad0: 0.0,
                color: [0.5, 0.5, 0.5],  // Dim white fallback
                intensity: 1.0,
                shadow_matrix: [[0.0; 4]; 4],
                shadow_map_index: -1,
                _pad1: [0.0; 3],
            });
        }
    }
}

// crates/void_render/src/light_buffer.rs

impl LightBuffer {
    /// Upload with GPU error recovery
    pub fn upload_safe(&mut self, queue: &Queue) {
        if let Err(e) = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.upload(queue);
        })) {
            log::error!("Failed to upload light buffer: {:?}", e);
            self.upload_failed = true;
        }
    }
}
```

### Fallback Behavior

```rust
// crates/void_render/src/light_buffer.rs

impl LightBuffer {
    upload_failed: bool,

    /// Get bind group with fallback
    pub fn bind_group_safe(&self) -> &wgpu::BindGroup {
        if self.upload_failed {
            // Return fallback bind group with minimal lighting
            &self.fallback_bind_group
        } else {
            &self.bind_groups[self.active_buffer]
        }
    }

    /// Create fallback bind group with ambient-only lighting
    fn create_fallback_bind_group(device: &Device, layout: &BindGroupLayout) -> BindGroup {
        let fallback_dir = GpuDirectionalLight {
            direction: [0.0, -1.0, 0.0],
            _pad0: 0.0,
            color: [0.3, 0.3, 0.3],
            intensity: 1.0,
            shadow_matrix: [[0.0; 4]; 4],
            shadow_map_index: -1,
            _pad1: [0.0; 3],
        };

        let fallback_buffer = device.create_buffer_init(&BufferInitDescriptor {
            label: Some("Fallback Light Buffer"),
            contents: bytemuck::bytes_of(&fallback_dir),
            usage: BufferUsages::UNIFORM,
        });

        // ... create bind group with fallback buffer
    }
}

// Shader-side fallback (in lighting.wgsl)
// If light_counts.directional_count == 0 && point_count == 0 && spot_count == 0:
//   Use hardcoded ambient (vec3(0.1)) to prevent completely black scenes
```

## Acceptance Criteria

### Functional

- [ ] Directional lights work correctly
- [ ] Point lights with attenuation work
- [ ] Spot lights with cone falloff work
- [ ] Lights can be parented to entities
- [ ] Light limits are enforced
- [ ] PBR shading produces correct results
- [ ] Multiple lights combine correctly
- [ ] Editor shows light gizmos
- [ ] Light properties editable in inspector
- [ ] Performance: 256 point lights at 60 FPS

### Hot-Swap Compliance

- [ ] Light component implements Serialize/Deserialize (serde)
- [ ] LightType and Attenuation enums implement Serialize/Deserialize
- [ ] GpuDirectionalLight/GpuPointLight/GpuSpotLight implement Serialize/Deserialize
- [ ] LightBuffer implements HotReloadable trait
- [ ] GPU buffers use double-buffering for seamless hot-swap
- [ ] Frame-boundary update applies pending hot-reload state
- [ ] Dirty marking triggers buffer swap on next frame
- [ ] Panic recovery falls back to ambient-only lighting
- [ ] Hot-swap tests pass (component serialization, GPU data serialization, buffer reload)

## Dependencies

- **Phase 1: Scene Graph** - For light parenting
- **Phase 2: Camera System** - For view direction

## Dependents

- **Phase 6: Shadow Mapping** - Shadows from lights

---

**Estimated Complexity**: High
**Primary Crates**: void_ecs, void_render
**Reviewer Notes**: Verify PBR math matches reference implementations
