# Phase 7: Advanced Material Features

## Status: Not Started

## User Story

> As a scene author, I want materials to represent a wider range of real-world surfaces.

## Requirements Checklist

- [ ] Add optional material parameters: Clearcoat, Clearcoat roughness, Transmission (glass), Opacity/alpha mode
- [ ] Support alpha modes: Opaque, Masked, Blended
- [ ] Allow per-entity material overrides
- [ ] Maintain backward compatibility with existing PBR schema

## Current State Analysis

### Existing Implementation

**void_ecs/src/render_components.rs:**
```rust
pub struct Material {
    pub base_color: [f32; 4],
    pub emissive: [f32; 3],
    pub metallic: f32,
    pub roughness: f32,
    pub albedo_texture: Option<String>,
    pub normal_texture: Option<String>,
    pub metallic_roughness_texture: Option<String>,
    pub shader: Option<String>,
    pub layer: Option<u32>,
}
```

### Gaps
1. No clearcoat support
2. No transmission (glass/water)
3. No subsurface scattering
4. No anisotropy
5. No alpha modes (only base_color.a)
6. No material instancing/overrides
7. No sheen (fabric)
8. No iridescence

## Implementation Specification

### 1. Enhanced Material Component

```rust
// crates/void_ecs/src/components/material.rs (REFACTOR)

/// PBR Material with advanced features
#[derive(Clone, Debug)]
pub struct Material {
    // === Core PBR ===
    /// Base color (albedo) with alpha
    pub base_color: [f32; 4],

    /// Metallic factor (0 = dielectric, 1 = metal)
    pub metallic: f32,

    /// Roughness factor (0 = smooth, 1 = rough)
    pub roughness: f32,

    /// Emissive color (HDR, can exceed 1.0)
    pub emissive: [f32; 3],

    /// Ambient occlusion factor
    pub ao: f32,

    // === Alpha ===
    /// Alpha rendering mode
    pub alpha_mode: AlphaMode,

    /// Cutoff for masked alpha
    pub alpha_cutoff: f32,

    /// Double-sided rendering
    pub double_sided: bool,

    // === Clearcoat ===
    /// Clearcoat intensity (0 = none, 1 = full)
    pub clearcoat: f32,

    /// Clearcoat roughness
    pub clearcoat_roughness: f32,

    // === Transmission ===
    /// Transmission factor (0 = opaque, 1 = fully transmissive)
    pub transmission: f32,

    /// Index of refraction (glass ~1.5, water ~1.33)
    pub ior: f32,

    /// Thickness for transmission (in world units)
    pub thickness: f32,

    /// Attenuation color (for colored glass)
    pub attenuation_color: [f32; 3],

    /// Attenuation distance
    pub attenuation_distance: f32,

    // === Subsurface ===
    /// Subsurface scattering factor
    pub subsurface: f32,

    /// Subsurface color
    pub subsurface_color: [f32; 3],

    /// Subsurface radius (RGB)
    pub subsurface_radius: [f32; 3],

    // === Sheen (Fabric) ===
    /// Sheen intensity
    pub sheen: f32,

    /// Sheen color
    pub sheen_color: [f32; 3],

    /// Sheen roughness
    pub sheen_roughness: f32,

    // === Anisotropy ===
    /// Anisotropy strength (-1 to 1)
    pub anisotropy: f32,

    /// Anisotropy rotation in radians
    pub anisotropy_rotation: f32,

    // === Iridescence ===
    /// Iridescence intensity
    pub iridescence: f32,

    /// Iridescence index of refraction
    pub iridescence_ior: f32,

    /// Thin-film thickness range [min, max] in nanometers
    pub iridescence_thickness: [f32; 2],

    // === Textures ===
    pub textures: MaterialTextures,

    // === Rendering ===
    /// Custom shader override
    pub shader: Option<String>,

    /// Render layer
    pub layer: Option<u32>,

    /// Render priority (for sorting)
    pub priority: i32,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum AlphaMode {
    /// Fully opaque, alpha ignored
    #[default]
    Opaque,

    /// Binary alpha (above cutoff = visible)
    Mask,

    /// Full alpha blending
    Blend,

    /// Additive blending
    Add,

    /// Multiplicative blending
    Multiply,
}

#[derive(Clone, Debug, Default)]
pub struct MaterialTextures {
    /// Base color/albedo texture
    pub base_color: Option<TextureRef>,

    /// Normal map
    pub normal: Option<TextureRef>,

    /// Metallic-roughness (G=roughness, B=metallic)
    pub metallic_roughness: Option<TextureRef>,

    /// Ambient occlusion
    pub occlusion: Option<TextureRef>,

    /// Emissive texture
    pub emissive: Option<TextureRef>,

    /// Clearcoat intensity
    pub clearcoat: Option<TextureRef>,

    /// Clearcoat roughness
    pub clearcoat_roughness: Option<TextureRef>,

    /// Clearcoat normal
    pub clearcoat_normal: Option<TextureRef>,

    /// Transmission
    pub transmission: Option<TextureRef>,

    /// Thickness
    pub thickness: Option<TextureRef>,

    /// Sheen color
    pub sheen_color: Option<TextureRef>,

    /// Sheen roughness
    pub sheen_roughness: Option<TextureRef>,

    /// Anisotropy (direction in RG, strength in B)
    pub anisotropy: Option<TextureRef>,

    /// Iridescence
    pub iridescence: Option<TextureRef>,

    /// Iridescence thickness
    pub iridescence_thickness: Option<TextureRef>,
}

#[derive(Clone, Debug)]
pub struct TextureRef {
    /// Asset path
    pub path: String,

    /// UV set index (0 or 1)
    pub uv_set: u32,

    /// Texture transform
    pub transform: Option<TextureTransform>,
}

#[derive(Clone, Debug)]
pub struct TextureTransform {
    pub offset: [f32; 2],
    pub scale: [f32; 2],
    pub rotation: f32,
}

impl Default for Material {
    fn default() -> Self {
        Self {
            base_color: [1.0, 1.0, 1.0, 1.0],
            metallic: 0.0,
            roughness: 0.5,
            emissive: [0.0, 0.0, 0.0],
            ao: 1.0,

            alpha_mode: AlphaMode::Opaque,
            alpha_cutoff: 0.5,
            double_sided: false,

            clearcoat: 0.0,
            clearcoat_roughness: 0.0,

            transmission: 0.0,
            ior: 1.5,
            thickness: 0.0,
            attenuation_color: [1.0, 1.0, 1.0],
            attenuation_distance: f32::INFINITY,

            subsurface: 0.0,
            subsurface_color: [1.0, 1.0, 1.0],
            subsurface_radius: [1.0, 0.2, 0.1],

            sheen: 0.0,
            sheen_color: [1.0, 1.0, 1.0],
            sheen_roughness: 0.5,

            anisotropy: 0.0,
            anisotropy_rotation: 0.0,

            iridescence: 0.0,
            iridescence_ior: 1.3,
            iridescence_thickness: [100.0, 400.0],

            textures: MaterialTextures::default(),

            shader: None,
            layer: None,
            priority: 0,
        }
    }
}

impl Material {
    /// Simple opaque material
    pub fn opaque(color: [f32; 3]) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            ..Default::default()
        }
    }

    /// Metallic material
    pub fn metal(color: [f32; 3], roughness: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            metallic: 1.0,
            roughness,
            ..Default::default()
        }
    }

    /// Glass material
    pub fn glass(tint: [f32; 3], ior: f32, roughness: f32) -> Self {
        Self {
            base_color: [tint[0], tint[1], tint[2], 0.0],
            transmission: 1.0,
            ior,
            roughness,
            alpha_mode: AlphaMode::Blend,
            ..Default::default()
        }
    }

    /// Car paint with clearcoat
    pub fn car_paint(color: [f32; 3], clearcoat: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            metallic: 0.8,
            roughness: 0.3,
            clearcoat,
            clearcoat_roughness: 0.1,
            ..Default::default()
        }
    }

    /// Fabric/cloth with sheen
    pub fn fabric(color: [f32; 3], sheen: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            roughness: 0.8,
            sheen,
            sheen_color: color,
            ..Default::default()
        }
    }

    /// Skin with subsurface scattering
    pub fn skin(color: [f32; 3], subsurface: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            roughness: 0.5,
            subsurface,
            subsurface_color: [0.8, 0.2, 0.1],  // Reddish blood
            ..Default::default()
        }
    }
}
```

### 2. Material GPU Buffer

```rust
// crates/void_render/src/material_buffer.rs (NEW FILE)

use bytemuck::{Pod, Zeroable};

/// GPU-ready material data
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct GpuMaterial {
    // Core PBR (16 bytes)
    pub base_color: [f32; 4],

    // Metallic/Roughness/AO/Flags (16 bytes)
    pub metallic: f32,
    pub roughness: f32,
    pub ao: f32,
    pub flags: u32,  // Packed: alpha_mode, double_sided, etc.

    // Emissive + alpha_cutoff (16 bytes)
    pub emissive: [f32; 3],
    pub alpha_cutoff: f32,

    // Clearcoat (16 bytes)
    pub clearcoat: f32,
    pub clearcoat_roughness: f32,
    pub _pad0: [f32; 2],

    // Transmission (16 bytes)
    pub transmission: f32,
    pub ior: f32,
    pub thickness: f32,
    pub attenuation_distance: f32,

    // Attenuation color + subsurface (16 bytes)
    pub attenuation_color: [f32; 3],
    pub subsurface: f32,

    // Subsurface color + sheen (16 bytes)
    pub subsurface_color: [f32; 3],
    pub sheen: f32,

    // Sheen color + roughness (16 bytes)
    pub sheen_color: [f32; 3],
    pub sheen_roughness: f32,

    // Anisotropy + Iridescence (16 bytes)
    pub anisotropy: f32,
    pub anisotropy_rotation: f32,
    pub iridescence: f32,
    pub iridescence_ior: f32,

    // Iridescence thickness + subsurface radius (16 bytes)
    pub iridescence_thickness_min: f32,
    pub iridescence_thickness_max: f32,
    pub subsurface_radius_r: f32,
    pub subsurface_radius_g: f32,

    // Remaining (16 bytes)
    pub subsurface_radius_b: f32,
    pub _pad1: [f32; 3],

    // Texture indices (16 bytes) - -1 = no texture
    pub tex_base_color: i32,
    pub tex_normal: i32,
    pub tex_metallic_roughness: i32,
    pub tex_emissive: i32,

    // More texture indices (16 bytes)
    pub tex_occlusion: i32,
    pub tex_clearcoat: i32,
    pub tex_transmission: i32,
    pub tex_sheen: i32,
}

impl GpuMaterial {
    pub const FLAGS_DOUBLE_SIDED: u32 = 1 << 0;
    pub const FLAGS_ALPHA_MASK: u32 = 1 << 1;
    pub const FLAGS_ALPHA_BLEND: u32 = 1 << 2;
    pub const FLAGS_HAS_CLEARCOAT: u32 = 1 << 3;
    pub const FLAGS_HAS_TRANSMISSION: u32 = 1 << 4;
    pub const FLAGS_HAS_SUBSURFACE: u32 = 1 << 5;
    pub const FLAGS_HAS_SHEEN: u32 = 1 << 6;
    pub const FLAGS_HAS_ANISOTROPY: u32 = 1 << 7;
    pub const FLAGS_HAS_IRIDESCENCE: u32 = 1 << 8;

    pub fn from_material(mat: &Material, tex_lookup: &impl Fn(&str) -> i32) -> Self {
        let mut flags = 0u32;

        if mat.double_sided { flags |= Self::FLAGS_DOUBLE_SIDED; }
        if mat.alpha_mode == AlphaMode::Mask { flags |= Self::FLAGS_ALPHA_MASK; }
        if mat.alpha_mode == AlphaMode::Blend { flags |= Self::FLAGS_ALPHA_BLEND; }
        if mat.clearcoat > 0.0 { flags |= Self::FLAGS_HAS_CLEARCOAT; }
        if mat.transmission > 0.0 { flags |= Self::FLAGS_HAS_TRANSMISSION; }
        if mat.subsurface > 0.0 { flags |= Self::FLAGS_HAS_SUBSURFACE; }
        if mat.sheen > 0.0 { flags |= Self::FLAGS_HAS_SHEEN; }
        if mat.anisotropy.abs() > 0.0 { flags |= Self::FLAGS_HAS_ANISOTROPY; }
        if mat.iridescence > 0.0 { flags |= Self::FLAGS_HAS_IRIDESCENCE; }

        Self {
            base_color: mat.base_color,
            metallic: mat.metallic,
            roughness: mat.roughness,
            ao: mat.ao,
            flags,
            emissive: mat.emissive,
            alpha_cutoff: mat.alpha_cutoff,
            clearcoat: mat.clearcoat,
            clearcoat_roughness: mat.clearcoat_roughness,
            _pad0: [0.0; 2],
            transmission: mat.transmission,
            ior: mat.ior,
            thickness: mat.thickness,
            attenuation_distance: mat.attenuation_distance,
            attenuation_color: mat.attenuation_color,
            subsurface: mat.subsurface,
            subsurface_color: mat.subsurface_color,
            sheen: mat.sheen,
            sheen_color: mat.sheen_color,
            sheen_roughness: mat.sheen_roughness,
            anisotropy: mat.anisotropy,
            anisotropy_rotation: mat.anisotropy_rotation,
            iridescence: mat.iridescence,
            iridescence_ior: mat.iridescence_ior,
            iridescence_thickness_min: mat.iridescence_thickness[0],
            iridescence_thickness_max: mat.iridescence_thickness[1],
            subsurface_radius_r: mat.subsurface_radius[0],
            subsurface_radius_g: mat.subsurface_radius[1],
            subsurface_radius_b: mat.subsurface_radius[2],
            _pad1: [0.0; 3],
            tex_base_color: mat.textures.base_color.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_normal: mat.textures.normal.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_metallic_roughness: mat.textures.metallic_roughness.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_emissive: mat.textures.emissive.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_occlusion: mat.textures.occlusion.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_clearcoat: mat.textures.clearcoat.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_transmission: mat.textures.transmission.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
            tex_sheen: mat.textures.sheen_color.as_ref()
                .map(|t| tex_lookup(&t.path)).unwrap_or(-1),
        }
    }
}
```

### 3. Material Override Component

```rust
// crates/void_ecs/src/components/material_override.rs (NEW FILE)

/// Per-entity material property overrides
#[derive(Clone, Debug, Default)]
pub struct MaterialOverride {
    /// Override base color
    pub base_color: Option<[f32; 4]>,

    /// Override metallic
    pub metallic: Option<f32>,

    /// Override roughness
    pub roughness: Option<f32>,

    /// Override emissive
    pub emissive: Option<[f32; 3]>,

    /// Override alpha mode
    pub alpha_mode: Option<AlphaMode>,

    /// Override clearcoat
    pub clearcoat: Option<f32>,

    /// Override transmission
    pub transmission: Option<f32>,

    /// Custom properties (name -> value)
    pub custom: HashMap<String, MaterialValue>,
}

#[derive(Clone, Debug)]
pub enum MaterialValue {
    Float(f32),
    Float2([f32; 2]),
    Float3([f32; 3]),
    Float4([f32; 4]),
    Int(i32),
    Texture(String),
}

impl MaterialOverride {
    /// Apply overrides to a material, returning the combined result
    pub fn apply(&self, base: &Material) -> Material {
        let mut result = base.clone();

        if let Some(color) = self.base_color {
            result.base_color = color;
        }
        if let Some(m) = self.metallic {
            result.metallic = m;
        }
        if let Some(r) = self.roughness {
            result.roughness = r;
        }
        if let Some(e) = self.emissive {
            result.emissive = e;
        }
        if let Some(mode) = self.alpha_mode {
            result.alpha_mode = mode;
        }
        if let Some(c) = self.clearcoat {
            result.clearcoat = c;
        }
        if let Some(t) = self.transmission {
            result.transmission = t;
        }

        result
    }
}
```

### 4. Advanced PBR Shader

```wgsl
// crates/void_runtime/src/shaders/pbr_advanced.wgsl

struct Material {
    base_color: vec4<f32>,

    metallic: f32,
    roughness: f32,
    ao: f32,
    flags: u32,

    emissive: vec3<f32>,
    alpha_cutoff: f32,

    clearcoat: f32,
    clearcoat_roughness: f32,
    _pad0: vec2<f32>,

    transmission: f32,
    ior: f32,
    thickness: f32,
    attenuation_distance: f32,

    attenuation_color: vec3<f32>,
    subsurface: f32,

    subsurface_color: vec3<f32>,
    sheen: f32,

    sheen_color: vec3<f32>,
    sheen_roughness: f32,

    anisotropy: f32,
    anisotropy_rotation: f32,
    iridescence: f32,
    iridescence_ior: f32,

    iridescence_thickness_min: f32,
    iridescence_thickness_max: f32,
    subsurface_radius_r: f32,
    subsurface_radius_g: f32,

    subsurface_radius_b: f32,
    _pad1: vec3<f32>,

    tex_base_color: i32,
    tex_normal: i32,
    tex_metallic_roughness: i32,
    tex_emissive: i32,

    tex_occlusion: i32,
    tex_clearcoat: i32,
    tex_transmission: i32,
    tex_sheen: i32,
};

const FLAG_DOUBLE_SIDED: u32 = 1u;
const FLAG_ALPHA_MASK: u32 = 2u;
const FLAG_ALPHA_BLEND: u32 = 4u;
const FLAG_HAS_CLEARCOAT: u32 = 8u;
const FLAG_HAS_TRANSMISSION: u32 = 16u;
const FLAG_HAS_SUBSURFACE: u32 = 32u;
const FLAG_HAS_SHEEN: u32 = 64u;
const FLAG_HAS_ANISOTROPY: u32 = 128u;
const FLAG_HAS_IRIDESCENCE: u32 = 256u;

@group(2) @binding(0)
var<uniform> material: Material;

@group(2) @binding(1)
var texture_sampler: sampler;

@group(2) @binding(2)
var texture_array: binding_array<texture_2d<f32>>;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Sample base color
    var base_color = material.base_color;
    if (material.tex_base_color >= 0) {
        base_color *= textureSample(
            texture_array[material.tex_base_color],
            texture_sampler,
            in.uv
        );
    }

    // Alpha testing
    if ((material.flags & FLAG_ALPHA_MASK) != 0u) {
        if (base_color.a < material.alpha_cutoff) {
            discard;
        }
    }

    // Sample normal
    var N = normalize(in.world_normal);
    if (material.tex_normal >= 0) {
        let tangent_normal = textureSample(
            texture_array[material.tex_normal],
            texture_sampler,
            in.uv
        ).xyz * 2.0 - 1.0;
        N = perturb_normal(N, in.world_tangent.xyz, in.world_tangent.w, tangent_normal);
    }

    // Sample metallic/roughness
    var metallic = material.metallic;
    var roughness = material.roughness;
    if (material.tex_metallic_roughness >= 0) {
        let mr = textureSample(
            texture_array[material.tex_metallic_roughness],
            texture_sampler,
            in.uv
        );
        metallic *= mr.b;
        roughness *= mr.g;
    }

    let V = normalize(camera.position - in.world_position);
    let albedo = base_color.rgb;

    // Base PBR lighting
    var Lo = calculate_lighting(
        in.world_position,
        N,
        V,
        albedo,
        metallic,
        roughness,
        material.ao
    );

    // Clearcoat layer
    if ((material.flags & FLAG_HAS_CLEARCOAT) != 0u) {
        Lo = apply_clearcoat(Lo, N, V, material.clearcoat, material.clearcoat_roughness);
    }

    // Sheen (fabric)
    if ((material.flags & FLAG_HAS_SHEEN) != 0u) {
        Lo += calculate_sheen(N, V, material.sheen_color, material.sheen, material.sheen_roughness);
    }

    // Iridescence
    if ((material.flags & FLAG_HAS_IRIDESCENCE) != 0u) {
        Lo = apply_iridescence(
            Lo, N, V,
            material.iridescence,
            material.iridescence_ior,
            material.iridescence_thickness_min,
            material.iridescence_thickness_max
        );
    }

    // Transmission (glass)
    if ((material.flags & FLAG_HAS_TRANSMISSION) != 0u) {
        let transmitted = calculate_transmission(
            in.world_position,
            N, V,
            material.transmission,
            material.ior,
            material.thickness,
            material.attenuation_color,
            material.attenuation_distance,
            albedo,
            roughness
        );
        Lo = mix(Lo, transmitted, material.transmission);
    }

    // Emissive
    var emissive = material.emissive;
    if (material.tex_emissive >= 0) {
        emissive *= textureSample(
            texture_array[material.tex_emissive],
            texture_sampler,
            in.uv
        ).rgb;
    }
    Lo += emissive;

    return vec4<f32>(Lo, base_color.a);
}

// Clearcoat BRDF layer
fn apply_clearcoat(
    base_color: vec3<f32>,
    N: vec3<f32>,
    V: vec3<f32>,
    clearcoat: f32,
    clearcoat_roughness: f32,
) -> vec3<f32> {
    let NdotV = max(dot(N, V), 0.0);

    // Clearcoat Fresnel (F0 = 0.04 for dielectric)
    let Fc = fresnel_schlick(NdotV, vec3<f32>(0.04)) * clearcoat;

    // Attenuate base by clearcoat Fresnel
    let base_attenuated = base_color * (1.0 - Fc);

    // Add clearcoat specular (simplified - should loop lights)
    // For now, just add ambient clearcoat term
    let clearcoat_specular = Fc * 0.1;  // Ambient clearcoat

    return base_attenuated + clearcoat_specular;
}

// Sheen BRDF (Charlie distribution)
fn calculate_sheen(
    N: vec3<f32>,
    V: vec3<f32>,
    sheen_color: vec3<f32>,
    sheen_intensity: f32,
    sheen_roughness: f32,
) -> vec3<f32> {
    let NdotV = max(dot(N, V), 0.0);

    // Simplified sheen - more complex in full implementation
    let sheen = sheen_color * sheen_intensity * pow(1.0 - NdotV, 5.0);

    return sheen;
}

// Iridescence thin-film interference
fn apply_iridescence(
    base_color: vec3<f32>,
    N: vec3<f32>,
    V: vec3<f32>,
    iridescence: f32,
    ior: f32,
    thickness_min: f32,
    thickness_max: f32,
) -> vec3<f32> {
    let NdotV = max(dot(N, V), 0.0);

    // Thin film thickness based on view angle
    let thickness = mix(thickness_min, thickness_max, NdotV);

    // Wavelength-dependent phase shift
    // Simplified: use RGB wavelengths (650, 550, 450 nm)
    let wavelengths = vec3<f32>(650.0, 550.0, 450.0);
    let phase = 2.0 * 3.14159 * 2.0 * ior * thickness / wavelengths;

    // Interference color
    let interference = 0.5 + 0.5 * cos(phase);

    return mix(base_color, base_color * interference, iridescence);
}

// Transmission (refraction)
fn calculate_transmission(
    world_pos: vec3<f32>,
    N: vec3<f32>,
    V: vec3<f32>,
    transmission: f32,
    ior: f32,
    thickness: f32,
    attenuation_color: vec3<f32>,
    attenuation_distance: f32,
    albedo: vec3<f32>,
    roughness: f32,
) -> vec3<f32> {
    // Refraction direction
    let eta = 1.0 / ior;
    let R = refract(-V, N, eta);

    // Sample background (requires separate pass in real implementation)
    // For now, use attenuation color as transmitted color
    var transmitted = attenuation_color;

    // Apply absorption based on thickness
    if (attenuation_distance < 1000.0) {
        let absorption = exp(-thickness / attenuation_distance * (1.0 - attenuation_color));
        transmitted *= absorption;
    }

    return transmitted;
}
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `void_ecs/src/components/material.rs` | CREATE | Enhanced material component |
| `void_ecs/src/components/material_override.rs` | CREATE | Material overrides |
| `void_render/src/material_buffer.rs` | CREATE | GPU material buffer |
| `void_runtime/src/shaders/pbr_advanced.wgsl` | CREATE | Advanced PBR shader |
| `void_ecs/src/render_components.rs` | MODIFY | Use new Material |
| `void_asset_server/src/loaders/gltf_loader.rs` | MODIFY | Extract advanced materials |
| `void_editor/src/panels/inspector.rs` | MODIFY | Material property UI |

## Testing Strategy

### Unit Tests
```rust
#[test]
fn test_material_defaults() {
    let mat = Material::default();
    assert_eq!(mat.metallic, 0.0);
    assert_eq!(mat.roughness, 0.5);
    assert_eq!(mat.alpha_mode, AlphaMode::Opaque);
}

#[test]
fn test_glass_material() {
    let glass = Material::glass([1.0, 1.0, 1.0], 1.5, 0.0);
    assert_eq!(glass.transmission, 1.0);
    assert_eq!(glass.ior, 1.5);
}

#[test]
fn test_material_override() {
    let base = Material::default();
    let override_ = MaterialOverride {
        base_color: Some([1.0, 0.0, 0.0, 1.0]),
        ..Default::default()
    };

    let combined = override_.apply(&base);
    assert_eq!(combined.base_color, [1.0, 0.0, 0.0, 1.0]);
}
```

### Visual Tests
```rust
#[test]
fn test_material_gallery() {
    // Render gallery of material presets
    // Visual verification required
}
```

## Hot-Swap Support

### Serialization

All material components derive Serde traits for state persistence during hot-reload:

```rust
use serde::{Serialize, Deserialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Material {
    pub base_color: [f32; 4],
    pub metallic: f32,
    pub roughness: f32,
    pub emissive: [f32; 3],
    pub ao: f32,
    pub alpha_mode: AlphaMode,
    pub alpha_cutoff: f32,
    pub double_sided: bool,
    pub clearcoat: f32,
    pub clearcoat_roughness: f32,
    pub transmission: f32,
    pub ior: f32,
    pub thickness: f32,
    pub attenuation_color: [f32; 3],
    pub attenuation_distance: f32,
    pub subsurface: f32,
    pub subsurface_color: [f32; 3],
    pub subsurface_radius: [f32; 3],
    pub sheen: f32,
    pub sheen_color: [f32; 3],
    pub sheen_roughness: f32,
    pub anisotropy: f32,
    pub anisotropy_rotation: f32,
    pub iridescence: f32,
    pub iridescence_ior: f32,
    pub iridescence_thickness: [f32; 2],
    pub textures: MaterialTextures,
    pub shader: Option<String>,
    pub layer: Option<u32>,
    pub priority: i32,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum AlphaMode {
    #[default]
    Opaque,
    Mask,
    Blend,
    Add,
    Multiply,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct MaterialTextures {
    pub base_color: Option<TextureRef>,
    pub normal: Option<TextureRef>,
    pub metallic_roughness: Option<TextureRef>,
    pub occlusion: Option<TextureRef>,
    pub emissive: Option<TextureRef>,
    pub clearcoat: Option<TextureRef>,
    pub clearcoat_roughness: Option<TextureRef>,
    pub clearcoat_normal: Option<TextureRef>,
    pub transmission: Option<TextureRef>,
    pub thickness: Option<TextureRef>,
    pub sheen_color: Option<TextureRef>,
    pub sheen_roughness: Option<TextureRef>,
    pub anisotropy: Option<TextureRef>,
    pub iridescence: Option<TextureRef>,
    pub iridescence_thickness: Option<TextureRef>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct TextureRef {
    pub path: String,
    pub uv_set: u32,
    pub transform: Option<TextureTransform>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MaterialOverride {
    pub base_color: Option<[f32; 4]>,
    pub metallic: Option<f32>,
    pub roughness: Option<f32>,
    pub emissive: Option<[f32; 3]>,
    pub alpha_mode: Option<AlphaMode>,
    pub clearcoat: Option<f32>,
    pub transmission: Option<f32>,
    pub custom: HashMap<String, MaterialValue>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum MaterialValue {
    Float(f32),
    Float2([f32; 2]),
    Float3([f32; 3]),
    Float4([f32; 4]),
    Int(i32),
    Texture(String),
}
```

### HotReloadable Implementation

```rust
use void_core::hot_reload::{HotReloadable, HotReloadContext};

impl HotReloadable for Material {
    fn type_name() -> &'static str {
        "Material"
    }

    fn version() -> u32 {
        1
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(data: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(data).map_err(|e| HotReloadError::Deserialization(e.to_string()))
    }

    fn on_reload(&mut self, ctx: &HotReloadContext) {
        // Mark GPU buffer as dirty for re-upload
        ctx.mark_dirty::<GpuMaterial>(self.id);

        // Re-resolve texture references
        self.resolve_textures(ctx.asset_server());
    }
}

impl HotReloadable for MaterialOverride {
    fn type_name() -> &'static str {
        "MaterialOverride"
    }

    fn version() -> u32 {
        1
    }

    fn serialize_state(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::Serialization(e.to_string()))
    }

    fn deserialize_state(data: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(data).map_err(|e| HotReloadError::Deserialization(e.to_string()))
    }

    fn on_reload(&mut self, _ctx: &HotReloadContext) {
        // Overrides re-apply automatically on next frame
    }
}
```

### Asset Dependencies

Materials depend on texture assets for hot-reload tracking:

```rust
use void_asset::AssetDependent;

impl AssetDependent for Material {
    fn get_dependencies(&self) -> Vec<AssetPath> {
        let mut deps = Vec::new();

        // Collect all texture dependencies
        if let Some(ref tex) = self.textures.base_color {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.normal {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.metallic_roughness {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.emissive {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.occlusion {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.clearcoat {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.transmission {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.sheen_color {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.anisotropy {
            deps.push(AssetPath::new(&tex.path));
        }
        if let Some(ref tex) = self.textures.iridescence {
            deps.push(AssetPath::new(&tex.path));
        }

        // Shader dependency
        if let Some(ref shader) = self.shader {
            deps.push(AssetPath::new(shader));
        }

        deps
    }

    fn on_dependency_changed(&mut self, path: &AssetPath, ctx: &AssetContext) {
        // Re-resolve texture if it changed
        if path.extension() == Some("png") || path.extension() == Some("jpg") {
            self.resolve_textures(ctx.asset_server());
            ctx.mark_dirty::<GpuMaterial>(self.id);
        }

        // Re-compile shader if it changed
        if path.extension() == Some("wgsl") {
            ctx.request_shader_recompile(&self.shader);
        }
    }
}
```

### Shader Hot-Reload

Advanced PBR shader supports hot-reload:

```rust
use void_shader::ShaderHotReload;

impl ShaderHotReload for AdvancedPbrShader {
    fn shader_paths(&self) -> Vec<&'static str> {
        vec![
            "shaders/pbr_advanced.wgsl",
            "shaders/brdf.wgsl",
            "shaders/clearcoat.wgsl",
            "shaders/transmission.wgsl",
            "shaders/sheen.wgsl",
            "shaders/iridescence.wgsl",
        ]
    }

    fn on_shader_changed(&mut self, path: &str, ctx: &ShaderContext) -> Result<(), ShaderError> {
        log::info!("Recompiling PBR shader due to change in: {}", path);

        // Read updated source
        let source = ctx.read_shader_source(path)?;

        // Recompile with Naga
        let module = ctx.compile_wgsl(&source)?;

        // Validate module
        ctx.validate_module(&module)?;

        // Create new pipeline
        let pipeline = ctx.create_pipeline(&module, &self.pipeline_layout)?;

        // Swap pipeline at frame boundary
        ctx.queue_pipeline_swap(self.pipeline_id, pipeline);

        Ok(())
    }
}
```

### Frame-Boundary Updates

GpuMaterial buffer updates at frame boundaries:

```rust
// crates/void_render/src/material_buffer.rs

pub struct MaterialBufferManager {
    buffer: wgpu::Buffer,
    staging: Vec<GpuMaterial>,
    dirty_materials: HashSet<MaterialId>,
    pending_updates: VecDeque<MaterialUpdate>,
}

impl MaterialBufferManager {
    /// Queue a material update (called during hot-reload)
    pub fn queue_update(&mut self, id: MaterialId, material: &Material, tex_lookup: &impl Fn(&str) -> i32) {
        let gpu_material = GpuMaterial::from_material(material, tex_lookup);
        self.pending_updates.push_back(MaterialUpdate {
            id,
            data: gpu_material,
        });
    }

    /// Apply pending updates at frame boundary
    pub fn apply_frame_updates(&mut self, queue: &wgpu::Queue) {
        if self.pending_updates.is_empty() {
            return;
        }

        // Batch all updates into single buffer write
        for update in self.pending_updates.drain(..) {
            let index = update.id.index();
            self.staging[index] = update.data;
            self.dirty_materials.insert(update.id);
        }

        // Upload dirty regions to GPU
        let data = bytemuck::cast_slice(&self.staging);
        queue.write_buffer(&self.buffer, 0, data);

        self.dirty_materials.clear();
    }
}

struct MaterialUpdate {
    id: MaterialId,
    data: GpuMaterial,
}
```

### Hot-Swap Tests

```rust
#[cfg(test)]
mod hot_swap_tests {
    use super::*;

    #[test]
    fn test_material_serialization_roundtrip() {
        let material = Material {
            base_color: [1.0, 0.5, 0.0, 1.0],
            metallic: 0.8,
            roughness: 0.2,
            clearcoat: 0.5,
            transmission: 0.3,
            ..Default::default()
        };

        let serialized = material.serialize_state().unwrap();
        let deserialized = Material::deserialize_state(&serialized).unwrap();

        assert_eq!(material.base_color, deserialized.base_color);
        assert_eq!(material.metallic, deserialized.metallic);
        assert_eq!(material.clearcoat, deserialized.clearcoat);
    }

    #[test]
    fn test_material_override_serialization() {
        let override_ = MaterialOverride {
            base_color: Some([1.0, 0.0, 0.0, 1.0]),
            metallic: Some(1.0),
            transmission: Some(0.5),
            ..Default::default()
        };

        let serialized = override_.serialize_state().unwrap();
        let deserialized = MaterialOverride::deserialize_state(&serialized).unwrap();

        assert_eq!(override_.base_color, deserialized.base_color);
        assert_eq!(override_.metallic, deserialized.metallic);
    }

    #[test]
    fn test_material_texture_dependency_tracking() {
        let mut material = Material::default();
        material.textures.base_color = Some(TextureRef {
            path: "textures/albedo.png".to_string(),
            uv_set: 0,
            transform: None,
        });
        material.textures.normal = Some(TextureRef {
            path: "textures/normal.png".to_string(),
            uv_set: 0,
            transform: None,
        });

        let deps = material.get_dependencies();

        assert_eq!(deps.len(), 2);
        assert!(deps.iter().any(|p| p.as_str() == "textures/albedo.png"));
        assert!(deps.iter().any(|p| p.as_str() == "textures/normal.png"));
    }

    #[test]
    fn test_gpu_material_buffer_hot_update() {
        let mut manager = MaterialBufferManager::new_test();

        let material = Material::glass([1.0, 1.0, 1.0], 1.5, 0.1);
        let id = MaterialId::new(0);

        manager.queue_update(id, &material, &|_| -1);

        assert_eq!(manager.pending_updates.len(), 1);

        // Simulate frame boundary
        let queue = create_test_queue();
        manager.apply_frame_updates(&queue);

        assert!(manager.pending_updates.is_empty());
    }

    #[test]
    fn test_shader_hot_reload_pipeline_swap() {
        let mut shader = AdvancedPbrShader::new_test();
        let ctx = ShaderContext::new_test();

        // Simulate shader file change
        let result = shader.on_shader_changed("shaders/pbr_advanced.wgsl", &ctx);

        assert!(result.is_ok());
        assert!(ctx.has_pending_pipeline_swap(shader.pipeline_id));
    }
}
```

## Fault Tolerance

### Material Loading with Fallback

```rust
impl Material {
    /// Load material with fallback to default on failure
    pub fn load_with_fallback(path: &str, asset_server: &AssetServer) -> Self {
        std::panic::catch_unwind(|| {
            asset_server.load::<Material>(path)
        })
        .unwrap_or_else(|e| {
            log::error!("Failed to load material '{}': {:?}, using fallback", path, e);
            Self::fallback_material()
        })
    }

    /// Fallback material (magenta for visibility)
    pub fn fallback_material() -> Self {
        Self {
            base_color: [1.0, 0.0, 1.0, 1.0],  // Magenta = error
            emissive: [0.5, 0.0, 0.5],
            roughness: 1.0,
            ..Default::default()
        }
    }
}
```

### Texture Resolution with Graceful Degradation

```rust
impl Material {
    /// Resolve textures with graceful degradation
    pub fn resolve_textures(&mut self, asset_server: &AssetServer) {
        self.resolve_texture_slot(&mut self.textures.base_color, asset_server, "base_color");
        self.resolve_texture_slot(&mut self.textures.normal, asset_server, "normal");
        self.resolve_texture_slot(&mut self.textures.metallic_roughness, asset_server, "metallic_roughness");
        // ... other textures
    }

    fn resolve_texture_slot(
        &self,
        slot: &mut Option<TextureRef>,
        asset_server: &AssetServer,
        slot_name: &str,
    ) {
        if let Some(ref mut tex) = slot {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                asset_server.resolve_texture(&tex.path)
            }));

            match result {
                Ok(Ok(_)) => {
                    // Texture resolved successfully
                }
                Ok(Err(e)) => {
                    log::warn!(
                        "Failed to resolve {} texture '{}': {}, using fallback",
                        slot_name, tex.path, e
                    );
                    tex.path = "textures/fallback_checkerboard.png".to_string();
                }
                Err(_) => {
                    log::error!(
                        "Panic while resolving {} texture '{}', using fallback",
                        slot_name, tex.path
                    );
                    tex.path = "textures/fallback_checkerboard.png".to_string();
                }
            }
        }
    }
}
```

### GpuMaterial Buffer Safety

```rust
impl GpuMaterial {
    /// Convert material to GPU format with panic protection
    pub fn from_material_safe(
        mat: &Material,
        tex_lookup: &impl Fn(&str) -> i32
    ) -> Result<Self, MaterialError> {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            Self::from_material(mat, tex_lookup)
        }))
        .map_err(|_| MaterialError::ConversionPanic)
    }
}

impl MaterialBufferManager {
    /// Apply updates with fault tolerance
    pub fn apply_frame_updates_safe(&mut self, queue: &wgpu::Queue) {
        for update in self.pending_updates.drain(..) {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                let index = update.id.index();
                if index < self.staging.len() {
                    self.staging[index] = update.data;
                    true
                } else {
                    false
                }
            }));

            match result {
                Ok(true) => {
                    self.dirty_materials.insert(update.id);
                }
                Ok(false) => {
                    log::error!("Material index {} out of bounds", update.id.index());
                }
                Err(_) => {
                    log::error!("Panic during material buffer update for {:?}", update.id);
                }
            }
        }

        // Upload with error handling
        if !self.dirty_materials.is_empty() {
            let data = bytemuck::cast_slice(&self.staging);
            queue.write_buffer(&self.buffer, 0, data);
            self.dirty_materials.clear();
        }
    }
}
```

## Acceptance Criteria

### Functional

- [ ] Clearcoat works correctly on car paint materials
- [ ] Transmission works for glass objects
- [ ] Alpha mask mode works with cutoff
- [ ] Alpha blend mode works with sorting
- [ ] Sheen works for fabric materials
- [ ] Iridescence creates rainbow effects
- [ ] Material overrides work per-entity
- [ ] glTF materials import correctly
- [ ] Editor shows all material properties
- [ ] Backward compatibility maintained

### Hot-Swap Compliance

- [ ] All material types derive `Serialize` and `Deserialize`
- [ ] Material implements `HotReloadable` trait
- [ ] MaterialOverride implements `HotReloadable` trait
- [ ] Material implements `AssetDependent` for texture tracking
- [ ] Texture changes trigger material re-resolution
- [ ] Shader changes trigger pipeline recompilation
- [ ] GpuMaterial buffer updates at frame boundaries only
- [ ] Serialization roundtrip preserves all material properties
- [ ] Fallback material displays on load failure
- [ ] Texture resolution gracefully degrades to checkerboard
- [ ] Hot-swap tests pass for all material components

## Dependencies

- **Phase 3: Mesh Import** - Materials applied to meshes
- **Phase 5: Lighting** - Materials need lights to render

## Dependents

- None (terminal feature)

---

**Estimated Complexity**: High
**Primary Crates**: void_ecs, void_render
**Reviewer Notes**: Energy conservation must be maintained across all BRDF layers
