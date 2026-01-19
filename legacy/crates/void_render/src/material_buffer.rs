//! GPU Material Buffer
//!
//! GPU-compatible material data structures for shader uniforms.
//! All structures are bytemuck Pod/Zeroable for direct GPU upload.
//!
//! # Hot-Swap Support
//!
//! All structures support serde serialization for hot-reload.
//! GPU buffers are recreated after deserialization.

use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

/// Maximum materials supported in a single buffer
pub const MAX_MATERIALS: usize = 256;

/// GPU-ready material data
///
/// This structure is uploaded to a uniform or storage buffer for shader access.
/// All fields are 16-byte aligned for GPU compatibility.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuMaterial {
    // Core PBR (16 bytes)
    /// Base color (RGBA)
    pub base_color: [f32; 4],

    // Metallic/Roughness/AO/Flags (16 bytes)
    /// Metallic factor
    pub metallic: f32,
    /// Roughness factor
    pub roughness: f32,
    /// Ambient occlusion factor
    pub ao: f32,
    /// Packed feature flags
    pub flags: u32,

    // Emissive + alpha_cutoff (16 bytes)
    /// Emissive color (RGB)
    pub emissive: [f32; 3],
    /// Alpha cutoff for masked mode
    pub alpha_cutoff: f32,

    // Clearcoat (16 bytes)
    /// Clearcoat intensity
    pub clearcoat: f32,
    /// Clearcoat roughness
    pub clearcoat_roughness: f32,
    /// Padding
    pub _pad0: [f32; 2],

    // Transmission (16 bytes)
    /// Transmission factor
    pub transmission: f32,
    /// Index of refraction
    pub ior: f32,
    /// Thickness for absorption
    pub thickness: f32,
    /// Attenuation distance
    pub attenuation_distance: f32,

    // Attenuation color + subsurface (16 bytes)
    /// Attenuation color for transmission
    pub attenuation_color: [f32; 3],
    /// Subsurface scattering factor
    pub subsurface: f32,

    // Subsurface color + sheen (16 bytes)
    /// Subsurface color
    pub subsurface_color: [f32; 3],
    /// Sheen intensity
    pub sheen: f32,

    // Sheen color + roughness (16 bytes)
    /// Sheen color
    pub sheen_color: [f32; 3],
    /// Sheen roughness
    pub sheen_roughness: f32,

    // Anisotropy + Iridescence (16 bytes)
    /// Anisotropy strength
    pub anisotropy: f32,
    /// Anisotropy rotation
    pub anisotropy_rotation: f32,
    /// Iridescence intensity
    pub iridescence: f32,
    /// Iridescence IOR
    pub iridescence_ior: f32,

    // Iridescence thickness + subsurface radius (16 bytes)
    /// Iridescence thickness min
    pub iridescence_thickness_min: f32,
    /// Iridescence thickness max
    pub iridescence_thickness_max: f32,
    /// Subsurface radius R
    pub subsurface_radius_r: f32,
    /// Subsurface radius G
    pub subsurface_radius_g: f32,

    // Remaining subsurface + padding (16 bytes)
    /// Subsurface radius B
    pub subsurface_radius_b: f32,
    /// Padding
    pub _pad1: [f32; 3],

    // Texture indices (16 bytes) - -1 = no texture
    /// Base color texture index
    pub tex_base_color: i32,
    /// Normal map texture index
    pub tex_normal: i32,
    /// Metallic-roughness texture index
    pub tex_metallic_roughness: i32,
    /// Emissive texture index
    pub tex_emissive: i32,

    // More texture indices (16 bytes)
    /// Occlusion texture index
    pub tex_occlusion: i32,
    /// Clearcoat texture index
    pub tex_clearcoat: i32,
    /// Transmission texture index
    pub tex_transmission: i32,
    /// Sheen texture index
    pub tex_sheen: i32,
}

impl GpuMaterial {
    /// Size of GpuMaterial in bytes
    pub const SIZE: usize = core::mem::size_of::<Self>();

    // Feature flags
    /// Double-sided rendering enabled
    pub const FLAG_DOUBLE_SIDED: u32 = 1 << 0;
    /// Alpha mask mode
    pub const FLAG_ALPHA_MASK: u32 = 1 << 1;
    /// Alpha blend mode
    pub const FLAG_ALPHA_BLEND: u32 = 1 << 2;
    /// Has clearcoat
    pub const FLAG_HAS_CLEARCOAT: u32 = 1 << 3;
    /// Has transmission
    pub const FLAG_HAS_TRANSMISSION: u32 = 1 << 4;
    /// Has subsurface scattering
    pub const FLAG_HAS_SUBSURFACE: u32 = 1 << 5;
    /// Has sheen
    pub const FLAG_HAS_SHEEN: u32 = 1 << 6;
    /// Has anisotropy
    pub const FLAG_HAS_ANISOTROPY: u32 = 1 << 7;
    /// Has iridescence
    pub const FLAG_HAS_IRIDESCENCE: u32 = 1 << 8;

    /// Create a default white material
    pub fn default_white() -> Self {
        Self {
            base_color: [1.0, 1.0, 1.0, 1.0],
            metallic: 0.0,
            roughness: 0.5,
            ao: 1.0,
            flags: 0,
            emissive: [0.0, 0.0, 0.0],
            alpha_cutoff: 0.5,
            clearcoat: 0.0,
            clearcoat_roughness: 0.0,
            _pad0: [0.0; 2],
            transmission: 0.0,
            ior: 1.5,
            thickness: 0.0,
            attenuation_distance: 1000.0,
            attenuation_color: [1.0, 1.0, 1.0],
            subsurface: 0.0,
            subsurface_color: [1.0, 1.0, 1.0],
            sheen: 0.0,
            sheen_color: [1.0, 1.0, 1.0],
            sheen_roughness: 0.5,
            anisotropy: 0.0,
            anisotropy_rotation: 0.0,
            iridescence: 0.0,
            iridescence_ior: 1.3,
            iridescence_thickness_min: 100.0,
            iridescence_thickness_max: 400.0,
            subsurface_radius_r: 1.0,
            subsurface_radius_g: 0.2,
            subsurface_radius_b: 0.1,
            _pad1: [0.0; 3],
            tex_base_color: -1,
            tex_normal: -1,
            tex_metallic_roughness: -1,
            tex_emissive: -1,
            tex_occlusion: -1,
            tex_clearcoat: -1,
            tex_transmission: -1,
            tex_sheen: -1,
        }
    }

    /// Create an error material (magenta)
    pub fn error() -> Self {
        Self {
            base_color: [1.0, 0.0, 1.0, 1.0],
            emissive: [0.5, 0.0, 0.5],
            roughness: 1.0,
            ..Self::default_white()
        }
    }

    /// Check if material has any advanced features
    pub fn has_advanced_features(&self) -> bool {
        (self.flags & (
            Self::FLAG_HAS_CLEARCOAT |
            Self::FLAG_HAS_TRANSMISSION |
            Self::FLAG_HAS_SUBSURFACE |
            Self::FLAG_HAS_SHEEN |
            Self::FLAG_HAS_ANISOTROPY |
            Self::FLAG_HAS_IRIDESCENCE
        )) != 0
    }

    /// Check if material is transparent
    pub fn is_transparent(&self) -> bool {
        (self.flags & Self::FLAG_ALPHA_BLEND) != 0 || self.transmission > 0.0
    }

    /// Check if double-sided
    pub fn is_double_sided(&self) -> bool {
        (self.flags & Self::FLAG_DOUBLE_SIDED) != 0
    }
}

/// Material buffer for GPU upload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct MaterialBuffer {
    /// All materials
    pub materials: Vec<GpuMaterial>,

    /// Default material index
    pub default_material: u32,

    /// Error material index
    pub error_material: u32,
}

impl MaterialBuffer {
    /// Create a new material buffer
    pub fn new() -> Self {
        let mut buffer = Self {
            materials: Vec::new(),
            default_material: 0,
            error_material: 1,
        };

        // Add default and error materials
        buffer.materials.push(GpuMaterial::default_white());
        buffer.materials.push(GpuMaterial::error());

        buffer
    }

    /// Create with capacity
    pub fn with_capacity(capacity: usize) -> Self {
        let mut buffer = Self {
            materials: Vec::with_capacity(capacity.max(2)),
            default_material: 0,
            error_material: 1,
        };

        buffer.materials.push(GpuMaterial::default_white());
        buffer.materials.push(GpuMaterial::error());

        buffer
    }

    /// Add a material, returns index
    pub fn add(&mut self, material: GpuMaterial) -> u32 {
        let index = self.materials.len() as u32;
        self.materials.push(material);
        index
    }

    /// Get a material by index
    pub fn get(&self, index: u32) -> Option<&GpuMaterial> {
        self.materials.get(index as usize)
    }

    /// Get a material by index, or default if invalid
    pub fn get_or_default(&self, index: u32) -> &GpuMaterial {
        self.materials.get(index as usize)
            .or_else(|| self.materials.get(self.default_material as usize))
            .unwrap_or(&self.materials[0])
    }

    /// Update a material at index
    pub fn update(&mut self, index: u32, material: GpuMaterial) -> bool {
        if let Some(m) = self.materials.get_mut(index as usize) {
            *m = material;
            true
        } else {
            false
        }
    }

    /// Get material count
    pub fn len(&self) -> usize {
        self.materials.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.materials.is_empty()
    }

    /// Get material data as bytes for GPU upload
    pub fn as_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.materials)
    }

    /// Clear all materials (keeps default and error)
    pub fn clear(&mut self) {
        self.materials.truncate(2);
    }
}

/// Material ID for referencing materials in the buffer
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct MaterialId(pub u32);

impl MaterialId {
    /// Create a new material ID
    pub fn new(index: u32) -> Self {
        Self(index)
    }

    /// Get the default material ID
    pub fn default_material() -> Self {
        Self(0)
    }

    /// Get the error material ID
    pub fn error_material() -> Self {
        Self(1)
    }

    /// Get the index
    pub fn index(&self) -> u32 {
        self.0
    }
}

impl Default for MaterialId {
    fn default() -> Self {
        Self::default_material()
    }
}

/// Material override values for per-entity customization
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct MaterialOverrideData {
    /// Override base color (None = use material default)
    pub base_color: Option<[f32; 4]>,
    /// Override metallic
    pub metallic: Option<f32>,
    /// Override roughness
    pub roughness: Option<f32>,
    /// Override emissive
    pub emissive: Option<[f32; 3]>,
    /// Override clearcoat
    pub clearcoat: Option<f32>,
    /// Override transmission
    pub transmission: Option<f32>,
}

impl MaterialOverrideData {
    /// Check if any overrides are set
    pub fn has_overrides(&self) -> bool {
        self.base_color.is_some()
            || self.metallic.is_some()
            || self.roughness.is_some()
            || self.emissive.is_some()
            || self.clearcoat.is_some()
            || self.transmission.is_some()
    }

    /// Apply overrides to a GpuMaterial
    pub fn apply(&self, material: &mut GpuMaterial) {
        if let Some(color) = self.base_color {
            material.base_color = color;
        }
        if let Some(m) = self.metallic {
            material.metallic = m;
        }
        if let Some(r) = self.roughness {
            material.roughness = r;
        }
        if let Some(e) = self.emissive {
            material.emissive = e;
        }
        if let Some(c) = self.clearcoat {
            material.clearcoat = c;
            if c > 0.0 {
                material.flags |= GpuMaterial::FLAG_HAS_CLEARCOAT;
            }
        }
        if let Some(t) = self.transmission {
            material.transmission = t;
            if t > 0.0 {
                material.flags |= GpuMaterial::FLAG_HAS_TRANSMISSION;
            }
        }
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
}

/// Material buffer state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MaterialBufferState {
    /// All materials
    pub materials: Vec<GpuMaterial>,
    /// Default material index
    pub default_material: u32,
    /// Error material index
    pub error_material: u32,
}

impl MaterialBuffer {
    /// Save state for hot-reload
    pub fn save_state(&self) -> MaterialBufferState {
        MaterialBufferState {
            materials: self.materials.clone(),
            default_material: self.default_material,
            error_material: self.error_material,
        }
    }

    /// Restore state from hot-reload
    pub fn restore_state(&mut self, state: MaterialBufferState) {
        self.materials = state.materials;
        self.default_material = state.default_material;
        self.error_material = state.error_material;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_gpu_material_size() {
        // Verify 16-byte alignment (should be 208 bytes = 13 * 16)
        assert_eq!(GpuMaterial::SIZE % 16, 0);
        // Expected size: 13 rows of 16 bytes = 208 bytes
        assert_eq!(GpuMaterial::SIZE, 208);
    }

    #[test]
    fn test_gpu_material_default() {
        let mat = GpuMaterial::default_white();
        assert_eq!(mat.base_color, [1.0, 1.0, 1.0, 1.0]);
        assert_eq!(mat.metallic, 0.0);
        assert_eq!(mat.roughness, 0.5);
        assert_eq!(mat.flags, 0);
    }

    #[test]
    fn test_gpu_material_error() {
        let mat = GpuMaterial::error();
        assert_eq!(mat.base_color, [1.0, 0.0, 1.0, 1.0]);
        assert!(mat.emissive[0] > 0.0);
    }

    #[test]
    fn test_gpu_material_flags() {
        let mut mat = GpuMaterial::default_white();
        mat.flags = GpuMaterial::FLAG_DOUBLE_SIDED | GpuMaterial::FLAG_HAS_CLEARCOAT;

        assert!(mat.is_double_sided());
        assert!(mat.has_advanced_features());
        assert!(!mat.is_transparent());

        mat.flags |= GpuMaterial::FLAG_ALPHA_BLEND;
        assert!(mat.is_transparent());
    }

    #[test]
    fn test_material_buffer() {
        let mut buffer = MaterialBuffer::new();

        assert_eq!(buffer.len(), 2); // default + error

        let index = buffer.add(GpuMaterial {
            base_color: [1.0, 0.0, 0.0, 1.0],
            ..GpuMaterial::default_white()
        });

        assert_eq!(index, 2);
        assert_eq!(buffer.len(), 3);

        let mat = buffer.get(index).unwrap();
        assert_eq!(mat.base_color, [1.0, 0.0, 0.0, 1.0]);
    }

    #[test]
    fn test_material_buffer_update() {
        let mut buffer = MaterialBuffer::new();

        let index = buffer.add(GpuMaterial::default_white());

        let updated = buffer.update(index, GpuMaterial {
            base_color: [0.0, 1.0, 0.0, 1.0],
            ..GpuMaterial::default_white()
        });

        assert!(updated);
        assert_eq!(buffer.get(index).unwrap().base_color, [0.0, 1.0, 0.0, 1.0]);
    }

    #[test]
    fn test_material_buffer_bytes() {
        let buffer = MaterialBuffer::new();
        let bytes = buffer.as_bytes();

        assert_eq!(bytes.len(), 2 * GpuMaterial::SIZE);
    }

    #[test]
    fn test_material_id() {
        let default = MaterialId::default_material();
        let error = MaterialId::error_material();

        assert_eq!(default.index(), 0);
        assert_eq!(error.index(), 1);
    }

    #[test]
    fn test_material_override() {
        let override_data = MaterialOverrideData::default()
            .with_base_color([1.0, 0.0, 0.0, 1.0])
            .with_metallic(1.0);

        assert!(override_data.has_overrides());

        let mut mat = GpuMaterial::default_white();
        override_data.apply(&mut mat);

        assert_eq!(mat.base_color, [1.0, 0.0, 0.0, 1.0]);
        assert_eq!(mat.metallic, 1.0);
    }

    #[test]
    fn test_material_buffer_serialization() {
        let mut buffer = MaterialBuffer::new();
        buffer.add(GpuMaterial {
            base_color: [0.5, 0.5, 0.5, 1.0],
            clearcoat: 0.8,
            flags: GpuMaterial::FLAG_HAS_CLEARCOAT,
            ..GpuMaterial::default_white()
        });

        let state = buffer.save_state();
        let json = serde_json::to_string(&state).unwrap();
        let restored_state: MaterialBufferState = serde_json::from_str(&json).unwrap();

        let mut new_buffer = MaterialBuffer::new();
        new_buffer.restore_state(restored_state);

        assert_eq!(new_buffer.len(), 3);
        assert_eq!(new_buffer.get(2).unwrap().clearcoat, 0.8);
    }

    #[test]
    fn test_material_override_serialization() {
        let override_data = MaterialOverrideData {
            base_color: Some([1.0, 0.5, 0.0, 1.0]),
            metallic: Some(0.8),
            roughness: None,
            emissive: Some([0.1, 0.0, 0.0]),
            clearcoat: Some(0.5),
            transmission: None,
        };

        let json = serde_json::to_string(&override_data).unwrap();
        let restored: MaterialOverrideData = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.base_color, Some([1.0, 0.5, 0.0, 1.0]));
        assert_eq!(restored.metallic, Some(0.8));
        assert_eq!(restored.roughness, None);
    }
}
