//! Standard rendering components for void_ecs
//!
//! These components are queried by the render extraction system to
//! build draw calls for GPU rendering.
//!
//! # Core Components
//!
//! - `Transform`: Position, rotation, scale in world space
//! - `LocalTransform`: Position, rotation, scale relative to parent
//! - `MeshRenderer`: References a mesh asset for rendering
//! - `Material`: Shader parameters, textures, colors
//! - `Visible`: Controls entity visibility and culling
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::prelude::*;
//! use void_ecs::render_components::*;
//!
//! let mut world = World::new();
//!
//! // Create a renderable entity
//! let entity = world.build_entity()
//!     .with(Transform::default())
//!     .with(MeshRenderer::cube())
//!     .with(Material::default())
//!     .with(Visible::default())
//!     .build();
//! ```

use alloc::string::String;
use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

/// Transform component - world-space position, rotation, and scale
#[derive(Clone, Copy, Debug, Default)]
pub struct Transform {
    /// World position
    pub position: [f32; 3],
    /// Euler rotation (pitch, yaw, roll) in radians
    pub rotation: [f32; 3],
    /// Scale factors
    pub scale: [f32; 3],
}

impl Transform {
    /// Create a new transform at a position
    pub fn from_position(x: f32, y: f32, z: f32) -> Self {
        Self {
            position: [x, y, z],
            rotation: [0.0, 0.0, 0.0],
            scale: [1.0, 1.0, 1.0],
        }
    }

    /// Create a new transform with position and scale
    pub fn from_position_scale(position: [f32; 3], scale: f32) -> Self {
        Self {
            position,
            rotation: [0.0, 0.0, 0.0],
            scale: [scale, scale, scale],
        }
    }

    /// Get the model matrix (4x4 column-major)
    pub fn model_matrix(&self) -> [[f32; 4]; 4] {
        // Simplified: just translation + scale (rotation needs quaternions for proper impl)
        let [tx, ty, tz] = self.position;
        let [sx, sy, sz] = self.scale;

        // For now, just translation and scale
        // Full rotation would require quaternion math
        [
            [sx, 0.0, 0.0, 0.0],
            [0.0, sy, 0.0, 0.0],
            [0.0, 0.0, sz, 0.0],
            [tx, ty, tz, 1.0],
        ]
    }
}

/// Local transform component - position relative to parent
#[derive(Clone, Copy, Debug, Default)]
pub struct LocalTransform {
    /// Local position offset
    pub position: [f32; 3],
    /// Local rotation
    pub rotation: [f32; 3],
    /// Local scale
    pub scale: [f32; 3],
}

/// Parent-child relationship for hierarchical transforms
#[derive(Clone, Copy, Debug)]
pub struct Parent {
    /// Parent entity
    pub entity: crate::Entity,
}

/// Children component (managed by the system, not set directly)
#[derive(Clone, Debug, Default)]
pub struct Children {
    /// Child entities
    pub entities: Vec<crate::Entity>,
}

/// Mesh type for built-in primitives
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum MeshType {
    /// Unit cube (1x1x1)
    Cube,
    /// Unit sphere (radius 1)
    Sphere,
    /// Unit cylinder (radius 1, height 1)
    Cylinder,
    /// Flat plane (1x1)
    Plane,
    /// Quad (2D rectangle)
    Quad,
    /// Custom mesh (requires mesh asset)
    Custom,
}

impl Default for MeshType {
    fn default() -> Self {
        Self::Cube
    }
}

/// Mesh renderer component - references a mesh for rendering
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MeshRenderer {
    /// Mesh type (built-in primitive or custom)
    pub mesh_type: MeshType,
    /// Custom mesh asset path (for MeshType::Custom)
    pub mesh_asset: Option<String>,
    /// GPU mesh cache asset ID (set by render system)
    #[serde(skip)]
    pub asset_id: Option<u64>,
    /// Cast shadows
    pub cast_shadows: bool,
    /// Receive shadows
    pub receive_shadows: bool,
    /// Render layer mask (bitfield for layer-based rendering)
    #[serde(default = "default_layer_mask")]
    pub layer_mask: u32,
    /// LOD bias (positive = prefer lower detail, negative = prefer higher detail)
    #[serde(default)]
    pub lod_bias: f32,
}

fn default_layer_mask() -> u32 {
    1
}

impl Default for MeshRenderer {
    fn default() -> Self {
        Self {
            mesh_type: MeshType::default(),
            mesh_asset: None,
            asset_id: None,
            cast_shadows: true,
            receive_shadows: true,
            layer_mask: 1,
            lod_bias: 0.0,
        }
    }
}

impl MeshRenderer {
    /// Create a cube renderer
    pub fn cube() -> Self {
        Self {
            mesh_type: MeshType::Cube,
            ..Default::default()
        }
    }

    /// Create a sphere renderer
    pub fn sphere() -> Self {
        Self {
            mesh_type: MeshType::Sphere,
            ..Default::default()
        }
    }

    /// Create a plane renderer
    pub fn plane() -> Self {
        Self {
            mesh_type: MeshType::Plane,
            cast_shadows: false,
            ..Default::default()
        }
    }

    /// Create a cylinder renderer
    pub fn cylinder() -> Self {
        Self {
            mesh_type: MeshType::Cylinder,
            ..Default::default()
        }
    }

    /// Create a custom mesh renderer
    pub fn custom(asset_path: impl Into<String>) -> Self {
        Self {
            mesh_type: MeshType::Custom,
            mesh_asset: Some(asset_path.into()),
            ..Default::default()
        }
    }

    /// Set the render layer mask
    pub fn with_layer_mask(mut self, mask: u32) -> Self {
        self.layer_mask = mask;
        self
    }

    /// Set the LOD bias
    pub fn with_lod_bias(mut self, bias: f32) -> Self {
        self.lod_bias = bias;
        self
    }
}

// ============================================================================
// Advanced Material System
// ============================================================================

/// Alpha blending/testing mode
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
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

impl AlphaMode {
    /// Check if this mode requires transparency sorting
    pub fn requires_sorting(&self) -> bool {
        matches!(self, Self::Blend | Self::Add | Self::Multiply)
    }

    /// Check if this mode uses alpha testing
    pub fn uses_alpha_test(&self) -> bool {
        matches!(self, Self::Mask)
    }
}

/// UV texture transform
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct TextureTransform {
    /// UV offset
    pub offset: [f32; 2],
    /// UV scale
    pub scale: [f32; 2],
    /// Rotation in radians
    pub rotation: f32,
}

impl Default for TextureTransform {
    fn default() -> Self {
        Self {
            offset: [0.0, 0.0],
            scale: [1.0, 1.0],
            rotation: 0.0,
        }
    }
}

impl TextureTransform {
    /// Create a tiling transform
    pub fn tiling(scale_x: f32, scale_y: f32) -> Self {
        Self {
            scale: [scale_x, scale_y],
            ..Default::default()
        }
    }

    /// Create an offset transform
    pub fn with_offset(offset_x: f32, offset_y: f32) -> Self {
        Self {
            offset: [offset_x, offset_y],
            ..Default::default()
        }
    }
}

/// Reference to a texture asset
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct TextureRef {
    /// Asset path
    pub path: String,
    /// UV set index (0 or 1)
    pub uv_set: u32,
    /// Texture transform
    pub transform: Option<TextureTransform>,
}

impl TextureRef {
    /// Create a texture reference from a path
    pub fn new(path: impl Into<String>) -> Self {
        Self {
            path: path.into(),
            uv_set: 0,
            transform: None,
        }
    }

    /// Create with UV set
    pub fn with_uv_set(mut self, uv_set: u32) -> Self {
        self.uv_set = uv_set;
        self
    }

    /// Create with transform
    pub fn with_transform(mut self, transform: TextureTransform) -> Self {
        self.transform = Some(transform);
        self
    }
}

/// Collection of material textures
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
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

impl MaterialTextures {
    /// Check if any textures are defined
    pub fn has_textures(&self) -> bool {
        self.base_color.is_some()
            || self.normal.is_some()
            || self.metallic_roughness.is_some()
            || self.occlusion.is_some()
            || self.emissive.is_some()
    }

    /// Get all texture paths for dependency tracking
    pub fn get_paths(&self) -> Vec<&str> {
        let mut paths = Vec::new();
        if let Some(ref t) = self.base_color { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.normal { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.metallic_roughness { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.occlusion { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.emissive { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.clearcoat { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.clearcoat_roughness { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.clearcoat_normal { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.transmission { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.thickness { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.sheen_color { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.sheen_roughness { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.anisotropy { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.iridescence { paths.push(t.path.as_str()); }
        if let Some(ref t) = self.iridescence_thickness { paths.push(t.path.as_str()); }
        paths
    }
}

/// PBR Material with advanced features
///
/// Supports standard metallic-roughness workflow plus extensions:
/// - Clearcoat (car paint, lacquered surfaces)
/// - Transmission (glass, water, gems)
/// - Subsurface scattering (skin, wax, marble)
/// - Sheen (fabric, velvet)
/// - Anisotropy (brushed metal, hair)
/// - Iridescence (soap bubbles, oil slicks, beetle shells)
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Material {
    // === Core PBR ===
    /// Base color (albedo) with alpha
    pub base_color: [f32; 4],
    /// Metallic factor (0 = dielectric, 1 = metal)
    pub metallic: f32,
    /// Roughness factor (0 = smooth/glossy, 1 = rough/matte)
    pub roughness: f32,
    /// Emissive color (HDR, can exceed 1.0)
    pub emissive: [f32; 3],
    /// Ambient occlusion factor (0 = fully occluded, 1 = no occlusion)
    pub ao: f32,

    // === Alpha ===
    /// Alpha rendering mode
    pub alpha_mode: AlphaMode,
    /// Cutoff threshold for masked alpha (0-1)
    pub alpha_cutoff: f32,
    /// Double-sided rendering (disable backface culling)
    pub double_sided: bool,

    // === Clearcoat ===
    /// Clearcoat intensity (0 = none, 1 = full)
    pub clearcoat: f32,
    /// Clearcoat roughness
    pub clearcoat_roughness: f32,

    // === Transmission (Glass/Water) ===
    /// Transmission factor (0 = opaque, 1 = fully transmissive)
    pub transmission: f32,
    /// Index of refraction (glass ~1.5, water ~1.33, diamond ~2.4)
    pub ior: f32,
    /// Thickness for transmission attenuation (in world units)
    pub thickness: f32,
    /// Attenuation color (for colored glass)
    pub attenuation_color: [f32; 3],
    /// Attenuation distance (how far light travels before full absorption)
    pub attenuation_distance: f32,

    // === Subsurface Scattering ===
    /// Subsurface scattering factor (0 = none, 1 = full)
    pub subsurface: f32,
    /// Subsurface color (light that scatters through the material)
    pub subsurface_color: [f32; 3],
    /// Subsurface radius per RGB channel (how far light scatters)
    pub subsurface_radius: [f32; 3],

    // === Sheen (Fabric) ===
    /// Sheen intensity (0 = none, 1 = full)
    pub sheen: f32,
    /// Sheen color
    pub sheen_color: [f32; 3],
    /// Sheen roughness
    pub sheen_roughness: f32,

    // === Anisotropy ===
    /// Anisotropy strength (-1 to 1, 0 = isotropic)
    pub anisotropy: f32,
    /// Anisotropy rotation in radians
    pub anisotropy_rotation: f32,

    // === Iridescence ===
    /// Iridescence intensity (0 = none, 1 = full)
    pub iridescence: f32,
    /// Iridescence index of refraction
    pub iridescence_ior: f32,
    /// Thin-film thickness range [min, max] in nanometers
    pub iridescence_thickness: [f32; 2],

    // === Textures ===
    /// All material textures
    pub textures: MaterialTextures,

    // === Rendering ===
    /// Custom shader override (None = default PBR)
    pub shader: Option<String>,
    /// Render layer (for layer-based rendering)
    pub layer: Option<u32>,
    /// Render priority (for sorting within same layer)
    pub priority: i32,
}

impl Default for Material {
    fn default() -> Self {
        Self {
            // Core PBR
            base_color: [1.0, 1.0, 1.0, 1.0],
            metallic: 0.0,
            roughness: 0.5,
            emissive: [0.0, 0.0, 0.0],
            ao: 1.0,

            // Alpha
            alpha_mode: AlphaMode::Opaque,
            alpha_cutoff: 0.5,
            double_sided: false,

            // Clearcoat
            clearcoat: 0.0,
            clearcoat_roughness: 0.0,

            // Transmission
            transmission: 0.0,
            ior: 1.5,
            thickness: 0.0,
            attenuation_color: [1.0, 1.0, 1.0],
            attenuation_distance: f32::INFINITY,

            // Subsurface
            subsurface: 0.0,
            subsurface_color: [1.0, 1.0, 1.0],
            subsurface_radius: [1.0, 0.2, 0.1], // Skin-like default

            // Sheen
            sheen: 0.0,
            sheen_color: [1.0, 1.0, 1.0],
            sheen_roughness: 0.5,

            // Anisotropy
            anisotropy: 0.0,
            anisotropy_rotation: 0.0,

            // Iridescence
            iridescence: 0.0,
            iridescence_ior: 1.3,
            iridescence_thickness: [100.0, 400.0],

            // Textures
            textures: MaterialTextures::default(),

            // Rendering
            shader: None,
            layer: None,
            priority: 0,
        }
    }
}

impl Material {
    // === Basic Constructors ===

    /// Create a simple opaque material with a color
    pub fn color(r: f32, g: f32, b: f32) -> Self {
        Self {
            base_color: [r, g, b, 1.0],
            ..Default::default()
        }
    }

    /// Create a simple opaque material from color array
    pub fn opaque(color: [f32; 3]) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            ..Default::default()
        }
    }

    /// Create a metallic material
    pub fn metal(color: [f32; 3], roughness: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            metallic: 1.0,
            roughness,
            ..Default::default()
        }
    }

    /// Create an emissive (glowing) material
    pub fn emissive_material(color: [f32; 3], intensity: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            emissive: [color[0] * intensity, color[1] * intensity, color[2] * intensity],
            ..Default::default()
        }
    }

    // === Advanced Material Presets ===

    /// Create a glass material
    pub fn glass(tint: [f32; 3], ior: f32, roughness: f32) -> Self {
        Self {
            base_color: [tint[0], tint[1], tint[2], 0.0],
            roughness,
            transmission: 1.0,
            ior,
            alpha_mode: AlphaMode::Blend,
            ..Default::default()
        }
    }

    /// Create a water material
    pub fn water() -> Self {
        Self {
            base_color: [0.2, 0.4, 0.6, 0.3],
            roughness: 0.0,
            transmission: 0.9,
            ior: 1.33,
            alpha_mode: AlphaMode::Blend,
            ..Default::default()
        }
    }

    /// Create a car paint material with clearcoat
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

    /// Create a fabric/cloth material with sheen
    pub fn fabric(color: [f32; 3], sheen: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            roughness: 0.8,
            sheen,
            sheen_color: color,
            sheen_roughness: 0.5,
            ..Default::default()
        }
    }

    /// Create a velvet material
    pub fn velvet(color: [f32; 3]) -> Self {
        Self::fabric(color, 1.0).with_sheen_roughness(0.3)
    }

    /// Create a skin material with subsurface scattering
    pub fn skin(color: [f32; 3], subsurface: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            roughness: 0.5,
            subsurface,
            subsurface_color: [0.8, 0.2, 0.1], // Blood red
            subsurface_radius: [1.0, 0.2, 0.1],
            ..Default::default()
        }
    }

    /// Create a wax/candle material
    pub fn wax(color: [f32; 3]) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            roughness: 0.4,
            subsurface: 0.8,
            subsurface_color: color,
            subsurface_radius: [0.5, 0.3, 0.2],
            ..Default::default()
        }
    }

    /// Create a brushed metal material with anisotropy
    pub fn brushed_metal(color: [f32; 3], anisotropy: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 1.0],
            metallic: 1.0,
            roughness: 0.3,
            anisotropy,
            ..Default::default()
        }
    }

    /// Create an iridescent material (soap bubble, oil slick)
    pub fn iridescent(base_color: [f32; 3], iridescence: f32) -> Self {
        Self {
            base_color: [base_color[0], base_color[1], base_color[2], 1.0],
            iridescence,
            iridescence_ior: 1.3,
            iridescence_thickness: [100.0, 400.0],
            ..Default::default()
        }
    }

    /// Create a diamond/gem material
    pub fn gem(color: [f32; 3], ior: f32) -> Self {
        Self {
            base_color: [color[0], color[1], color[2], 0.0],
            roughness: 0.0,
            transmission: 1.0,
            ior,
            alpha_mode: AlphaMode::Blend,
            ..Default::default()
        }
    }

    /// Create a fallback error material (magenta)
    pub fn fallback() -> Self {
        Self {
            base_color: [1.0, 0.0, 1.0, 1.0],
            emissive: [0.5, 0.0, 0.5],
            roughness: 1.0,
            ..Default::default()
        }
    }

    // === Builder Methods ===

    /// Set base color
    pub fn with_base_color(mut self, color: [f32; 4]) -> Self {
        self.base_color = color;
        self
    }

    /// Set metallic factor
    pub fn with_metallic(mut self, metallic: f32) -> Self {
        self.metallic = metallic.clamp(0.0, 1.0);
        self
    }

    /// Set roughness factor
    pub fn with_roughness(mut self, roughness: f32) -> Self {
        self.roughness = roughness.clamp(0.0, 1.0);
        self
    }

    /// Set emissive color
    pub fn with_emissive(mut self, emissive: [f32; 3]) -> Self {
        self.emissive = emissive;
        self
    }

    /// Set alpha mode
    pub fn with_alpha_mode(mut self, mode: AlphaMode) -> Self {
        self.alpha_mode = mode;
        self
    }

    /// Set alpha cutoff (for masked mode)
    pub fn with_alpha_cutoff(mut self, cutoff: f32) -> Self {
        self.alpha_cutoff = cutoff.clamp(0.0, 1.0);
        self
    }

    /// Enable double-sided rendering
    pub fn double_sided(mut self) -> Self {
        self.double_sided = true;
        self
    }

    /// Set clearcoat
    pub fn with_clearcoat(mut self, clearcoat: f32, roughness: f32) -> Self {
        self.clearcoat = clearcoat.clamp(0.0, 1.0);
        self.clearcoat_roughness = roughness.clamp(0.0, 1.0);
        self
    }

    /// Set transmission
    pub fn with_transmission(mut self, transmission: f32, ior: f32) -> Self {
        self.transmission = transmission.clamp(0.0, 1.0);
        self.ior = ior.max(1.0);
        self
    }

    /// Set subsurface scattering
    pub fn with_subsurface(mut self, subsurface: f32, color: [f32; 3]) -> Self {
        self.subsurface = subsurface.clamp(0.0, 1.0);
        self.subsurface_color = color;
        self
    }

    /// Set sheen
    pub fn with_sheen(mut self, sheen: f32, color: [f32; 3]) -> Self {
        self.sheen = sheen.clamp(0.0, 1.0);
        self.sheen_color = color;
        self
    }

    /// Set sheen roughness
    pub fn with_sheen_roughness(mut self, roughness: f32) -> Self {
        self.sheen_roughness = roughness.clamp(0.0, 1.0);
        self
    }

    /// Set anisotropy
    pub fn with_anisotropy(mut self, strength: f32, rotation: f32) -> Self {
        self.anisotropy = strength.clamp(-1.0, 1.0);
        self.anisotropy_rotation = rotation;
        self
    }

    /// Set iridescence
    pub fn with_iridescence(mut self, iridescence: f32, ior: f32, thickness: [f32; 2]) -> Self {
        self.iridescence = iridescence.clamp(0.0, 1.0);
        self.iridescence_ior = ior;
        self.iridescence_thickness = thickness;
        self
    }

    /// Set base color texture
    pub fn with_base_color_texture(mut self, path: impl Into<String>) -> Self {
        self.textures.base_color = Some(TextureRef::new(path));
        self
    }

    /// Set normal map
    pub fn with_normal_texture(mut self, path: impl Into<String>) -> Self {
        self.textures.normal = Some(TextureRef::new(path));
        self
    }

    /// Set metallic-roughness texture
    pub fn with_metallic_roughness_texture(mut self, path: impl Into<String>) -> Self {
        self.textures.metallic_roughness = Some(TextureRef::new(path));
        self
    }

    /// Set all textures
    pub fn with_textures(mut self, textures: MaterialTextures) -> Self {
        self.textures = textures;
        self
    }

    /// Set custom shader
    pub fn with_shader(mut self, shader: impl Into<String>) -> Self {
        self.shader = Some(shader.into());
        self
    }

    /// Set render layer
    pub fn with_layer(mut self, layer: u32) -> Self {
        self.layer = Some(layer);
        self
    }

    /// Set render priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    // === Query Methods ===

    /// Check if material has any advanced features enabled
    pub fn has_advanced_features(&self) -> bool {
        self.clearcoat > 0.0
            || self.transmission > 0.0
            || self.subsurface > 0.0
            || self.sheen > 0.0
            || self.anisotropy.abs() > 0.0
            || self.iridescence > 0.0
    }

    /// Check if material is transparent
    pub fn is_transparent(&self) -> bool {
        self.alpha_mode.requires_sorting() || self.transmission > 0.0
    }

    /// Get material feature flags as a bitmask
    pub fn feature_flags(&self) -> u32 {
        let mut flags = 0u32;
        if self.double_sided { flags |= 1 << 0; }
        if self.alpha_mode == AlphaMode::Mask { flags |= 1 << 1; }
        if self.alpha_mode.requires_sorting() { flags |= 1 << 2; }
        if self.clearcoat > 0.0 { flags |= 1 << 3; }
        if self.transmission > 0.0 { flags |= 1 << 4; }
        if self.subsurface > 0.0 { flags |= 1 << 5; }
        if self.sheen > 0.0 { flags |= 1 << 6; }
        if self.anisotropy.abs() > 0.0 { flags |= 1 << 7; }
        if self.iridescence > 0.0 { flags |= 1 << 8; }
        flags
    }
}

/// Per-entity material property overrides
///
/// Allows customizing specific material properties without creating
/// a new material. Useful for color variations, damage effects, etc.
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct MaterialOverride {
    /// Override base color
    pub base_color: Option<[f32; 4]>,
    /// Override metallic factor
    pub metallic: Option<f32>,
    /// Override roughness factor
    pub roughness: Option<f32>,
    /// Override emissive color
    pub emissive: Option<[f32; 3]>,
    /// Override alpha mode
    pub alpha_mode: Option<AlphaMode>,
    /// Override alpha cutoff
    pub alpha_cutoff: Option<f32>,
    /// Override clearcoat
    pub clearcoat: Option<f32>,
    /// Override transmission
    pub transmission: Option<f32>,
}

impl MaterialOverride {
    /// Create a new empty override
    pub fn new() -> Self {
        Self::default()
    }

    /// Check if any overrides are set
    pub fn has_overrides(&self) -> bool {
        self.base_color.is_some()
            || self.metallic.is_some()
            || self.roughness.is_some()
            || self.emissive.is_some()
            || self.alpha_mode.is_some()
            || self.alpha_cutoff.is_some()
            || self.clearcoat.is_some()
            || self.transmission.is_some()
    }

    /// Apply overrides to a material, returning the modified material
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
        if let Some(cutoff) = self.alpha_cutoff {
            result.alpha_cutoff = cutoff;
        }
        if let Some(c) = self.clearcoat {
            result.clearcoat = c;
        }
        if let Some(t) = self.transmission {
            result.transmission = t;
        }

        result
    }

    /// Set base color override
    pub fn with_base_color(mut self, color: [f32; 4]) -> Self {
        self.base_color = Some(color);
        self
    }

    /// Set metallic override
    pub fn with_metallic(mut self, metallic: f32) -> Self {
        self.metallic = Some(metallic);
        self
    }

    /// Set roughness override
    pub fn with_roughness(mut self, roughness: f32) -> Self {
        self.roughness = Some(roughness);
        self
    }

    /// Set emissive override
    pub fn with_emissive(mut self, emissive: [f32; 3]) -> Self {
        self.emissive = Some(emissive);
        self
    }

    /// Set alpha mode override
    pub fn with_alpha_mode(mut self, mode: AlphaMode) -> Self {
        self.alpha_mode = Some(mode);
        self
    }

    /// Set clearcoat override
    pub fn with_clearcoat(mut self, clearcoat: f32) -> Self {
        self.clearcoat = Some(clearcoat);
        self
    }

    /// Set transmission override
    pub fn with_transmission(mut self, transmission: f32) -> Self {
        self.transmission = Some(transmission);
        self
    }
}

/// Visibility component - controls rendering and culling
#[derive(Clone, Copy, Debug)]
pub struct Visible {
    /// Is entity visible
    pub visible: bool,
    /// Frustum culling enabled
    pub frustum_cull: bool,
    /// Occlusion culling enabled
    pub occlusion_cull: bool,
    /// Render layer mask (bitfield)
    pub layer_mask: u32,
}

impl Default for Visible {
    fn default() -> Self {
        Self {
            visible: true,
            frustum_cull: true,
            occlusion_cull: false,
            layer_mask: 1, // Default layer
        }
    }
}

impl Visible {
    /// Create a visible entity on all layers
    pub fn all_layers() -> Self {
        Self {
            layer_mask: u32::MAX,
            ..Default::default()
        }
    }

    /// Create an invisible entity
    pub fn hidden() -> Self {
        Self {
            visible: false,
            ..Default::default()
        }
    }
}

/// Bounding box component for culling (axis-aligned)
#[derive(Clone, Copy, Debug)]
pub struct BoundingBox {
    /// Minimum corner
    pub min: [f32; 3],
    /// Maximum corner
    pub max: [f32; 3],
}

impl Default for BoundingBox {
    fn default() -> Self {
        Self {
            min: [-0.5, -0.5, -0.5],
            max: [0.5, 0.5, 0.5],
        }
    }
}

impl BoundingBox {
    /// Create a unit bounding box centered at origin
    pub fn unit() -> Self {
        Self::default()
    }

    /// Create a bounding box from center and half-extents
    pub fn from_center_extents(center: [f32; 3], extents: [f32; 3]) -> Self {
        Self {
            min: [
                center[0] - extents[0],
                center[1] - extents[1],
                center[2] - extents[2],
            ],
            max: [
                center[0] + extents[0],
                center[1] + extents[1],
                center[2] + extents[2],
            ],
        }
    }

    /// Get the center point
    pub fn center(&self) -> [f32; 3] {
        [
            (self.min[0] + self.max[0]) * 0.5,
            (self.min[1] + self.max[1]) * 0.5,
            (self.min[2] + self.max[2]) * 0.5,
        ]
    }

    /// Get the half-extents
    pub fn extents(&self) -> [f32; 3] {
        [
            (self.max[0] - self.min[0]) * 0.5,
            (self.max[1] - self.min[1]) * 0.5,
            (self.max[2] - self.min[2]) * 0.5,
        ]
    }
}

/// Light attenuation model
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum Attenuation {
    /// Physically-based inverse square falloff
    InverseSquare,
    /// Linear falloff from center to range
    Linear,
    /// Custom curve (constant, linear, quadratic coefficients)
    Custom {
        constant: f32,
        linear: f32,
        quadratic: f32,
    },
}

impl Default for Attenuation {
    fn default() -> Self {
        Self::InverseSquare
    }
}

impl Attenuation {
    /// Get attenuation coefficients as [constant, linear, quadratic]
    pub fn coefficients(&self, range: f32) -> [f32; 3] {
        match self {
            Self::InverseSquare => [1.0, 0.0, 1.0],
            Self::Linear => [1.0, 1.0 / range.max(0.001), 0.0],
            Self::Custom { constant, linear, quadratic } => [*constant, *linear, *quadratic],
        }
    }
}

/// Light type with embedded parameters
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum LightType {
    /// Sun-like directional light affecting entire scene
    Directional {
        /// Angular diameter in degrees (0 = sharp shadows, >0 = soft)
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
        /// Maximum range
        range: f32,
    },
}

impl Default for LightType {
    fn default() -> Self {
        Self::Point {
            range: 10.0,
            attenuation: Attenuation::InverseSquare,
        }
    }
}

impl LightType {
    /// Get the range of the light (0 for directional)
    pub fn range(&self) -> f32 {
        match self {
            Self::Directional { .. } => 0.0,
            Self::Point { range, .. } => *range,
            Self::Spot { range, .. } => *range,
            Self::RectArea { range, .. } => *range,
        }
    }

    /// Check if this light type is directional
    pub fn is_directional(&self) -> bool {
        matches!(self, Self::Directional { .. })
    }

    /// Check if this light type is a point light
    pub fn is_point(&self) -> bool {
        matches!(self, Self::Point { .. })
    }

    /// Check if this light type is a spot light
    pub fn is_spot(&self) -> bool {
        matches!(self, Self::Spot { .. })
    }
}

/// Shadow update mode for per-light control
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum ShadowUpdateMode {
    /// Update shadow map every frame
    EveryFrame,
    /// Update every N frames
    Interval(u32),
    /// Only update when light or shadow casters move
    OnChange,
    /// Static shadow map (render once)
    Static,
}

impl Default for ShadowUpdateMode {
    fn default() -> Self {
        Self::EveryFrame
    }
}

/// Per-light shadow settings
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LightShadowSettings {
    /// Shadow map resolution override (None = use global default)
    pub resolution: Option<u32>,

    /// Depth bias to prevent shadow acne
    pub depth_bias: f32,

    /// Slope-scaled depth bias
    pub slope_bias: f32,

    /// Normal-based offset to prevent peter-panning
    pub normal_bias: f32,

    /// Near plane for shadow camera
    pub near_plane: f32,

    /// Far plane override (None = auto from light range)
    pub far_plane: Option<f32>,

    /// Light size for soft shadows (PCSS)
    pub light_size: f32,

    /// Shadow strength (0 = no shadow, 1 = full shadow)
    pub strength: f32,

    /// Shadow update mode
    pub update_mode: ShadowUpdateMode,

    /// Cascade blend distance (for directional lights)
    pub cascade_blend_distance: f32,
}

impl Default for LightShadowSettings {
    fn default() -> Self {
        Self {
            resolution: None,
            depth_bias: 0.005,
            slope_bias: 2.0,
            normal_bias: 0.02,
            near_plane: 0.1,
            far_plane: None,
            light_size: 1.0,
            strength: 1.0,
            update_mode: ShadowUpdateMode::EveryFrame,
            cascade_blend_distance: 2.0,
        }
    }
}

impl LightShadowSettings {
    /// Create settings optimized for directional lights
    pub fn directional() -> Self {
        Self {
            depth_bias: 0.002,
            slope_bias: 1.5,
            normal_bias: 0.01,
            light_size: 0.5, // Sun angular diameter
            ..Default::default()
        }
    }

    /// Create settings optimized for spot lights
    pub fn spot() -> Self {
        Self {
            depth_bias: 0.005,
            slope_bias: 2.0,
            normal_bias: 0.02,
            ..Default::default()
        }
    }

    /// Create settings optimized for point lights
    pub fn point() -> Self {
        Self {
            depth_bias: 0.01,
            slope_bias: 3.0,
            normal_bias: 0.03,
            near_plane: 0.05,
            ..Default::default()
        }
    }

    /// Get effective resolution
    pub fn effective_resolution(&self, default: u32) -> u32 {
        self.resolution.unwrap_or(default)
    }
}

/// Light source component
///
/// Supports directional, point, spot, and area lights with PBR-compatible
/// intensity values (lumens for point/spot, lux for directional).
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Light {
    /// Light type determines behavior and parameters
    pub light_type: LightType,

    /// Light color (linear RGB, not sRGB)
    pub color: [f32; 3],

    /// Intensity in lumens (point/spot) or lux (directional)
    pub intensity: f32,

    /// Whether this light casts shadows
    pub cast_shadows: bool,

    /// Shadow bias to prevent shadow acne
    pub shadow_bias: f32,

    /// Shadow normal bias for thin geometry
    pub shadow_normal_bias: f32,

    /// Detailed shadow settings
    pub shadow_settings: LightShadowSettings,

    /// Layer mask (only affects entities with matching layer)
    pub layer_mask: u32,

    /// Light cookie texture path (optional projection pattern)
    pub cookie_texture: Option<String>,

    /// Is this light active?
    pub enabled: bool,
}

impl Default for Light {
    fn default() -> Self {
        Self {
            light_type: LightType::Point {
                range: 10.0,
                attenuation: Attenuation::InverseSquare,
            },
            color: [1.0, 1.0, 1.0],
            intensity: 1000.0, // 1000 lumens
            cast_shadows: false,
            shadow_bias: 0.005,
            shadow_normal_bias: 0.02,
            shadow_settings: LightShadowSettings::default(),
            layer_mask: u32::MAX,
            cookie_texture: None,
            enabled: true,
        }
    }
}

impl Light {
    /// Create a directional light (sun-like)
    pub fn directional(color: [f32; 3], intensity: f32) -> Self {
        Self {
            light_type: LightType::Directional { angular_diameter: 0.0 },
            color,
            intensity,
            cast_shadows: true,
            shadow_settings: LightShadowSettings::directional(),
            ..Default::default()
        }
    }

    /// Create a point light
    pub fn point(color: [f32; 3], intensity: f32, range: f32) -> Self {
        Self {
            light_type: LightType::Point {
                range,
                attenuation: Attenuation::InverseSquare,
            },
            color,
            intensity,
            shadow_settings: LightShadowSettings::point(),
            ..Default::default()
        }
    }

    /// Create a spot light
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
            shadow_settings: LightShadowSettings::spot(),
            ..Default::default()
        }
    }

    /// Create a rectangular area light
    pub fn rect_area(color: [f32; 3], intensity: f32, width: f32, height: f32, range: f32) -> Self {
        Self {
            light_type: LightType::RectArea { width, height, range },
            color,
            intensity,
            cast_shadows: false, // Area light shadows are complex
            ..Default::default()
        }
    }

    /// Set shadow bias
    pub fn with_shadow_bias(mut self, bias: f32, normal_bias: f32) -> Self {
        self.shadow_bias = bias;
        self.shadow_normal_bias = normal_bias;
        self
    }

    /// Enable shadows
    pub fn with_shadows(mut self) -> Self {
        self.cast_shadows = true;
        self
    }

    /// Set layer mask
    pub fn with_layer_mask(mut self, mask: u32) -> Self {
        self.layer_mask = mask;
        self
    }

    /// Set cookie texture
    pub fn with_cookie(mut self, path: impl Into<String>) -> Self {
        self.cookie_texture = Some(path.into());
        self
    }

    /// Set shadow settings
    pub fn with_shadow_settings(mut self, settings: LightShadowSettings) -> Self {
        self.shadow_settings = settings;
        self
    }

    /// Set shadow resolution override
    pub fn with_shadow_resolution(mut self, resolution: u32) -> Self {
        self.shadow_settings.resolution = Some(resolution);
        self
    }

    /// Set shadow strength (0-1)
    pub fn with_shadow_strength(mut self, strength: f32) -> Self {
        self.shadow_settings.strength = strength.clamp(0.0, 1.0);
        self
    }

    /// Get the light range (0 for directional lights)
    pub fn range(&self) -> f32 {
        self.light_type.range()
    }

    /// Get attenuation coefficients for this light
    pub fn attenuation_coefficients(&self) -> [f32; 3] {
        match &self.light_type {
            LightType::Directional { .. } => [1.0, 0.0, 0.0],
            LightType::Point { range, attenuation } => attenuation.coefficients(*range),
            LightType::Spot { range, attenuation, .. } => attenuation.coefficients(*range),
            LightType::RectArea { range, .. } => Attenuation::Linear.coefficients(*range),
        }
    }
}

/// Camera component
#[derive(Clone, Debug)]
pub struct Camera {
    /// Field of view in radians (for perspective)
    pub fov: f32,
    /// Near clip plane
    pub near: f32,
    /// Far clip plane
    pub far: f32,
    /// Is this the main camera
    pub is_main: bool,
    /// Clear color (RGBA)
    pub clear_color: [f32; 4],
    /// Render priority (higher = rendered later)
    pub priority: i32,
    /// Target layer name (None = main target)
    pub target_layer: Option<String>,
}

impl Default for Camera {
    fn default() -> Self {
        Self {
            fov: 60.0_f32.to_radians(),
            near: 0.1,
            far: 1000.0,
            is_main: false,
            clear_color: [0.0, 0.0, 0.0, 1.0],
            priority: 0,
            target_layer: None,
        }
    }
}

impl Camera {
    /// Create a main camera
    pub fn main() -> Self {
        Self {
            is_main: true,
            ..Default::default()
        }
    }

    /// Set the field of view in degrees
    pub fn with_fov_degrees(mut self, degrees: f32) -> Self {
        self.fov = degrees.to_radians();
        self
    }
}

// ============================================================================
// Instancing Components
// ============================================================================

/// Instance group identifier for batching entities together
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct InstanceGroupId(pub u64);

impl InstanceGroupId {
    /// Create from mesh asset ID (auto-batching by mesh)
    pub fn from_mesh(mesh_id: u64) -> Self {
        Self(mesh_id)
    }

    /// Create a manual group ID
    pub fn manual(id: u64) -> Self {
        Self(id)
    }

    /// Get the inner ID
    pub fn id(&self) -> u64 {
        self.0
    }
}

impl Default for InstanceGroupId {
    fn default() -> Self {
        Self(0)
    }
}

/// Instance group component for batching similar entities
///
/// Entities with the same group_id and material will be batched together
/// for GPU instanced rendering.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct InstanceGroup {
    /// Group identifier (entities with same ID are batched)
    pub group_id: InstanceGroupId,

    /// Per-instance color tint override (None = use material color)
    pub color_tint: Option<[f32; 4]>,

    /// Custom instance data (shader-specific, e.g., animation state)
    pub custom_data: [f32; 4],

    /// Enable instancing for this entity
    pub enabled: bool,
}

impl Default for InstanceGroup {
    fn default() -> Self {
        Self {
            group_id: InstanceGroupId::default(),
            color_tint: None,
            custom_data: [0.0; 4],
            enabled: true,
        }
    }
}

impl InstanceGroup {
    /// Create a new instance group
    pub fn new(group_id: InstanceGroupId) -> Self {
        Self {
            group_id,
            ..Default::default()
        }
    }

    /// Create from mesh asset ID
    pub fn from_mesh(mesh_id: u64) -> Self {
        Self {
            group_id: InstanceGroupId::from_mesh(mesh_id),
            ..Default::default()
        }
    }

    /// Set color tint
    pub fn with_color(mut self, color: [f32; 4]) -> Self {
        self.color_tint = Some(color);
        self
    }

    /// Set custom data
    pub fn with_custom_data(mut self, data: [f32; 4]) -> Self {
        self.custom_data = data;
        self
    }

    /// Disable instancing for this entity
    pub fn disabled(mut self) -> Self {
        self.enabled = false;
        self
    }
}

/// Computed instance data (runtime-only, not serialized)
///
/// This is computed each frame during render extraction and
/// provides information about where this entity is in the instance buffer.
#[derive(Clone, Copy, Debug, Default)]
pub struct ComputedInstanceData {
    /// Batch index this entity belongs to
    pub batch_index: u32,

    /// Instance index within the batch
    pub instance_index: u32,

    /// Frame this was computed on
    pub frame: u64,
}

impl ComputedInstanceData {
    /// Check if this data is valid for the current frame
    pub fn is_valid(&self, current_frame: u64) -> bool {
        self.frame == current_frame
    }
}

// ============================================================================
// Picking Components
// ============================================================================

/// Collision shape for picking
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ColliderShape {
    /// Box collider with half-extents
    Box { half_extents: [f32; 3] },
    /// Sphere collider
    Sphere { radius: f32 },
    /// Capsule collider (axis-aligned along Y)
    Capsule { radius: f32, height: f32 },
    /// Custom mesh collider (path to mesh asset)
    Mesh { path: String },
}

impl ColliderShape {
    /// Create a unit box collider
    pub fn unit_box() -> Self {
        Self::Box { half_extents: [0.5, 0.5, 0.5] }
    }

    /// Create a box collider
    pub fn box_shape(half_x: f32, half_y: f32, half_z: f32) -> Self {
        Self::Box { half_extents: [half_x, half_y, half_z] }
    }

    /// Create a sphere collider
    pub fn sphere(radius: f32) -> Self {
        Self::Sphere { radius }
    }

    /// Create a capsule collider
    pub fn capsule(radius: f32, height: f32) -> Self {
        Self::Capsule { radius, height }
    }

    /// Create a mesh collider
    pub fn mesh(path: impl Into<String>) -> Self {
        Self::Mesh { path: path.into() }
    }
}

/// Marks an entity as pickable by raycasts
///
/// Entities with this component can be selected via mouse/touch interaction.
/// The picking system uses this component to determine which entities
/// participate in raycasting.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Pickable {
    /// Is picking enabled for this entity
    pub enabled: bool,
    /// Use mesh collider (accurate but slower) or bounds (fast but less accurate)
    pub use_mesh_collider: bool,
    /// Custom collision shape override (None = use bounding box)
    pub collider_shape: Option<ColliderShape>,
    /// Layer mask for picking (must match query layer mask)
    pub layer_mask: u32,
    /// Priority for overlapping picks (higher = selected first)
    pub priority: i32,
}

impl Default for Pickable {
    fn default() -> Self {
        Self {
            enabled: true,
            use_mesh_collider: false, // Bounds by default for performance
            collider_shape: None,
            layer_mask: u32::MAX, // All layers by default
            priority: 0,
        }
    }
}

impl Pickable {
    /// Create a pickable component with default settings
    pub fn new() -> Self {
        Self::default()
    }

    /// Create a pickable with mesh-accurate collision
    pub fn mesh_accurate() -> Self {
        Self {
            use_mesh_collider: true,
            ..Default::default()
        }
    }

    /// Create a pickable with box collider
    pub fn with_box(half_extents: [f32; 3]) -> Self {
        Self {
            collider_shape: Some(ColliderShape::Box { half_extents }),
            ..Default::default()
        }
    }

    /// Create a pickable with sphere collider
    pub fn with_sphere(radius: f32) -> Self {
        Self {
            collider_shape: Some(ColliderShape::Sphere { radius }),
            ..Default::default()
        }
    }

    /// Create a pickable with capsule collider
    pub fn with_capsule(radius: f32, height: f32) -> Self {
        Self {
            collider_shape: Some(ColliderShape::Capsule { radius, height }),
            ..Default::default()
        }
    }

    /// Create a disabled pickable
    pub fn disabled() -> Self {
        Self {
            enabled: false,
            ..Default::default()
        }
    }

    /// Set layer mask
    pub fn with_layer_mask(mut self, mask: u32) -> Self {
        self.layer_mask = mask;
        self
    }

    /// Set priority
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Enable mesh collider
    pub fn with_mesh_collider(mut self) -> Self {
        self.use_mesh_collider = true;
        self
    }

    /// Set collider shape
    pub fn with_collider(mut self, shape: ColliderShape) -> Self {
        self.collider_shape = Some(shape);
        self
    }

    /// Disable picking
    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    /// Check if this pickable matches a layer mask
    pub fn matches_layer(&self, query_mask: u32) -> bool {
        self.layer_mask & query_mask != 0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_transform_model_matrix() {
        let transform = Transform::from_position(1.0, 2.0, 3.0);
        let matrix = transform.model_matrix();

        // Translation is in the last column
        assert_eq!(matrix[3][0], 1.0);
        assert_eq!(matrix[3][1], 2.0);
        assert_eq!(matrix[3][2], 3.0);
    }

    #[test]
    fn test_material_default() {
        let mat = Material::default();
        assert_eq!(mat.base_color, [1.0, 1.0, 1.0, 1.0]);
        assert_eq!(mat.metallic, 0.0);
    }

    #[test]
    fn test_bounding_box() {
        let bbox = BoundingBox::from_center_extents([0.0, 0.0, 0.0], [1.0, 1.0, 1.0]);
        assert_eq!(bbox.min, [-1.0, -1.0, -1.0]);
        assert_eq!(bbox.max, [1.0, 1.0, 1.0]);
        assert_eq!(bbox.center(), [0.0, 0.0, 0.0]);
        assert_eq!(bbox.extents(), [1.0, 1.0, 1.0]);
    }

    #[test]
    fn test_instance_group_id() {
        let id1 = InstanceGroupId::from_mesh(42);
        let id2 = InstanceGroupId::manual(42);
        assert_eq!(id1, id2);
        assert_eq!(id1.id(), 42);
    }

    #[test]
    fn test_instance_group() {
        let group = InstanceGroup::from_mesh(123)
            .with_color([1.0, 0.0, 0.0, 1.0])
            .with_custom_data([1.0, 2.0, 3.0, 4.0]);

        assert_eq!(group.group_id.id(), 123);
        assert_eq!(group.color_tint, Some([1.0, 0.0, 0.0, 1.0]));
        assert_eq!(group.custom_data, [1.0, 2.0, 3.0, 4.0]);
        assert!(group.enabled);
    }

    #[test]
    fn test_instance_group_serialization() {
        let group = InstanceGroup::from_mesh(456)
            .with_color([0.5, 0.5, 0.5, 1.0]);

        let json = serde_json::to_string(&group).unwrap();
        let restored: InstanceGroup = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.group_id.id(), 456);
        assert_eq!(restored.color_tint, Some([0.5, 0.5, 0.5, 1.0]));
    }

    #[test]
    fn test_computed_instance_data() {
        let data = ComputedInstanceData {
            batch_index: 1,
            instance_index: 42,
            frame: 100,
        };

        assert!(data.is_valid(100));
        assert!(!data.is_valid(101));
    }

    // ==================== Light Tests ====================

    #[test]
    fn test_attenuation_coefficients() {
        let inv_sq = Attenuation::InverseSquare;
        let coef = inv_sq.coefficients(10.0);
        assert_eq!(coef, [1.0, 0.0, 1.0]);

        let linear = Attenuation::Linear;
        let coef = linear.coefficients(10.0);
        assert!((coef[1] - 0.1).abs() < 0.001);

        let custom = Attenuation::Custom {
            constant: 1.0,
            linear: 0.5,
            quadratic: 0.25,
        };
        let coef = custom.coefficients(10.0);
        assert_eq!(coef, [1.0, 0.5, 0.25]);
    }

    #[test]
    fn test_light_type_range() {
        let dir = LightType::Directional { angular_diameter: 0.5 };
        assert_eq!(dir.range(), 0.0);
        assert!(dir.is_directional());

        let point = LightType::Point { range: 15.0, attenuation: Attenuation::InverseSquare };
        assert_eq!(point.range(), 15.0);
        assert!(point.is_point());

        let spot = LightType::Spot {
            range: 20.0,
            inner_angle: 0.5,
            outer_angle: 0.8,
            attenuation: Attenuation::Linear,
        };
        assert_eq!(spot.range(), 20.0);
        assert!(spot.is_spot());
    }

    #[test]
    fn test_light_constructors() {
        let dir = Light::directional([1.0, 0.9, 0.8], 100.0);
        assert!(dir.light_type.is_directional());
        assert_eq!(dir.intensity, 100.0);
        assert!(dir.cast_shadows);

        let point = Light::point([1.0, 1.0, 1.0], 1000.0, 15.0);
        assert!(point.light_type.is_point());
        assert_eq!(point.range(), 15.0);

        let spot = Light::spot([1.0, 1.0, 0.9], 2000.0, 20.0, 30.0, 45.0);
        assert!(spot.light_type.is_spot());
        if let LightType::Spot { inner_angle, outer_angle, .. } = spot.light_type {
            assert!((inner_angle - 30.0_f32.to_radians()).abs() < 0.001);
            assert!((outer_angle - 45.0_f32.to_radians()).abs() < 0.001);
        }
    }

    #[test]
    fn test_light_builder() {
        let light = Light::point([1.0; 3], 1000.0, 10.0)
            .with_shadows()
            .with_shadow_bias(0.01, 0.05)
            .with_layer_mask(0xFF)
            .with_cookie("cookies/star.png");

        assert!(light.cast_shadows);
        assert_eq!(light.shadow_bias, 0.01);
        assert_eq!(light.shadow_normal_bias, 0.05);
        assert_eq!(light.layer_mask, 0xFF);
        assert_eq!(light.cookie_texture, Some("cookies/star.png".to_string()));
    }

    #[test]
    fn test_light_attenuation_coefficients() {
        let point = Light::point([1.0; 3], 1000.0, 10.0);
        let coef = point.attenuation_coefficients();
        assert_eq!(coef, [1.0, 0.0, 1.0]); // InverseSquare default
    }

    #[test]
    fn test_light_serialization() {
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
            shadow_settings: LightShadowSettings::point(),
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
            panic!("Wrong light type after deserialization");
        }
    }

    #[test]
    fn test_rect_area_light() {
        let area = Light::rect_area([1.0, 1.0, 1.0], 5000.0, 2.0, 1.0, 10.0);
        assert_eq!(area.range(), 10.0);
        assert!(!area.cast_shadows); // Area lights don't cast shadows by default
    }

    // ==================== Shadow Settings Tests ====================

    #[test]
    fn test_shadow_update_mode() {
        assert!(ShadowUpdateMode::EveryFrame == ShadowUpdateMode::default());

        let interval = ShadowUpdateMode::Interval(3);
        assert_eq!(interval, ShadowUpdateMode::Interval(3));
    }

    #[test]
    fn test_light_shadow_settings_defaults() {
        let settings = LightShadowSettings::default();
        assert_eq!(settings.resolution, None);
        assert!((settings.depth_bias - 0.005).abs() < 0.001);
        assert!((settings.strength - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_light_shadow_settings_presets() {
        let dir = LightShadowSettings::directional();
        assert!((dir.depth_bias - 0.002).abs() < 0.001);

        let spot = LightShadowSettings::spot();
        assert!((spot.depth_bias - 0.005).abs() < 0.001);

        let point = LightShadowSettings::point();
        assert!((point.depth_bias - 0.01).abs() < 0.001);
        assert!((point.near_plane - 0.05).abs() < 0.001);
    }

    #[test]
    fn test_light_shadow_settings_effective_resolution() {
        let mut settings = LightShadowSettings::default();
        assert_eq!(settings.effective_resolution(2048), 2048);

        settings.resolution = Some(4096);
        assert_eq!(settings.effective_resolution(2048), 4096);
    }

    #[test]
    fn test_light_with_shadow_settings() {
        let light = Light::directional([1.0; 3], 100.0)
            .with_shadow_resolution(4096)
            .with_shadow_strength(0.8);

        assert_eq!(light.shadow_settings.resolution, Some(4096));
        assert!((light.shadow_settings.strength - 0.8).abs() < 0.001);
    }

    #[test]
    fn test_light_shadow_settings_serialization() {
        let settings = LightShadowSettings {
            resolution: Some(4096),
            depth_bias: 0.01,
            slope_bias: 3.0,
            normal_bias: 0.05,
            near_plane: 0.5,
            far_plane: Some(100.0),
            light_size: 2.0,
            strength: 0.75,
            update_mode: ShadowUpdateMode::Interval(2),
            cascade_blend_distance: 5.0,
        };

        let json = serde_json::to_string(&settings).unwrap();
        let restored: LightShadowSettings = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.resolution, Some(4096));
        assert_eq!(restored.far_plane, Some(100.0));
        assert_eq!(restored.update_mode, ShadowUpdateMode::Interval(2));
        assert!((restored.strength - 0.75).abs() < 0.001);
    }

    #[test]
    fn test_light_constructors_use_appropriate_shadow_settings() {
        let dir = Light::directional([1.0; 3], 100.0);
        assert!((dir.shadow_settings.depth_bias - 0.002).abs() < 0.001);

        let point = Light::point([1.0; 3], 1000.0, 10.0);
        assert!((point.shadow_settings.depth_bias - 0.01).abs() < 0.001);

        let spot = Light::spot([1.0; 3], 2000.0, 20.0, 30.0, 45.0);
        assert!((spot.shadow_settings.depth_bias - 0.005).abs() < 0.001);
    }

    // ==================== Pickable Tests ====================

    #[test]
    fn test_pickable_default() {
        let pickable = Pickable::default();
        assert!(pickable.enabled);
        assert!(!pickable.use_mesh_collider);
        assert!(pickable.collider_shape.is_none());
        assert_eq!(pickable.layer_mask, u32::MAX);
        assert_eq!(pickable.priority, 0);
    }

    #[test]
    fn test_pickable_constructors() {
        let mesh = Pickable::mesh_accurate();
        assert!(mesh.use_mesh_collider);

        let box_pick = Pickable::with_box([1.0, 2.0, 3.0]);
        assert!(matches!(
            box_pick.collider_shape,
            Some(ColliderShape::Box { half_extents: [1.0, 2.0, 3.0] })
        ));

        let sphere_pick = Pickable::with_sphere(2.5);
        assert!(matches!(
            sphere_pick.collider_shape,
            Some(ColliderShape::Sphere { radius: r }) if (r - 2.5).abs() < 0.001
        ));

        let disabled = Pickable::disabled();
        assert!(!disabled.enabled);
    }

    #[test]
    fn test_pickable_builder() {
        let pickable = Pickable::new()
            .with_layer_mask(0b1010)
            .with_priority(5)
            .with_mesh_collider();

        assert_eq!(pickable.layer_mask, 0b1010);
        assert_eq!(pickable.priority, 5);
        assert!(pickable.use_mesh_collider);
    }

    #[test]
    fn test_pickable_layer_matching() {
        let pickable = Pickable::new().with_layer_mask(0b0101);

        assert!(pickable.matches_layer(0b0001)); // Match
        assert!(pickable.matches_layer(0b0100)); // Match
        assert!(!pickable.matches_layer(0b0010)); // No match
        assert!(!pickable.matches_layer(0b1000)); // No match
    }

    #[test]
    fn test_pickable_serialization() {
        let pickable = Pickable::new()
            .with_layer_mask(0xFF)
            .with_priority(10)
            .with_collider(ColliderShape::sphere(3.0));

        let json = serde_json::to_string(&pickable).unwrap();
        let restored: Pickable = serde_json::from_str(&json).unwrap();

        assert!(restored.enabled);
        assert_eq!(restored.layer_mask, 0xFF);
        assert_eq!(restored.priority, 10);
        assert!(matches!(
            restored.collider_shape,
            Some(ColliderShape::Sphere { radius: r }) if (r - 3.0).abs() < 0.001
        ));
    }

    #[test]
    fn test_collider_shape_variants() {
        let shapes = vec![
            ColliderShape::unit_box(),
            ColliderShape::box_shape(1.0, 2.0, 3.0),
            ColliderShape::sphere(5.0),
            ColliderShape::capsule(0.5, 2.0),
            ColliderShape::mesh("models/custom.obj"),
        ];

        for shape in shapes {
            let json = serde_json::to_string(&shape).unwrap();
            let _: ColliderShape = serde_json::from_str(&json).unwrap();
        }
    }
}
