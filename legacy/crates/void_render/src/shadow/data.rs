//! GPU Shadow Data Structures
//!
//! GPU-compatible data structures for shadow mapping. All structures are
//! bytemuck Pod/Zeroable for direct GPU upload.

use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

use super::cascade::MAX_CASCADES;

/// Maximum shadow-casting lights supported
pub const MAX_SHADOW_LIGHTS: usize = 16;

/// GPU shadow data for a single light (uniform buffer)
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuShadowLight {
    /// Light-space view-projection matrix
    pub matrix: [[f32; 4]; 4],

    /// Shadow map atlas layer index
    pub layer: i32,

    /// Shadow bias
    pub bias: f32,

    /// Normal bias
    pub normal_bias: f32,

    /// Shadow strength (0-1)
    pub strength: f32,

    /// Near plane
    pub near: f32,

    /// Far plane
    pub far: f32,

    /// Texel size for PCF (1.0 / resolution)
    pub texel_size: f32,

    /// Light size for PCSS
    pub light_size: f32,
}

impl GpuShadowLight {
    /// Create a disabled shadow light
    pub fn disabled() -> Self {
        Self {
            layer: -1,
            ..Default::default()
        }
    }

    /// Check if this shadow light is enabled
    pub fn is_enabled(&self) -> bool {
        self.layer >= 0
    }

    /// Create from shadow parameters
    pub fn new(
        matrix: [[f32; 4]; 4],
        layer: u32,
        bias: f32,
        normal_bias: f32,
        strength: f32,
        near: f32,
        far: f32,
        resolution: u32,
        light_size: f32,
    ) -> Self {
        Self {
            matrix,
            layer: layer as i32,
            bias,
            normal_bias,
            strength,
            near,
            far,
            texel_size: 1.0 / resolution as f32,
            light_size,
        }
    }
}

/// GPU cascade shadow data for directional lights
#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuCascadeShadow {
    /// View-projection matrices for each cascade
    pub matrices: [[[f32; 4]; 4]; MAX_CASCADES],

    /// Cascade split distances (view-space Z)
    pub splits: [f32; 4],

    /// Atlas layer indices for each cascade
    pub layers: [i32; 4],

    /// Number of active cascades
    pub cascade_count: u32,

    /// Cascade blend distance
    pub blend_distance: f32,

    /// Shadow bias (base)
    pub bias: f32,

    /// Normal bias
    pub normal_bias: f32,

    /// Shadow strength
    pub strength: f32,

    /// Texel size
    pub texel_size: f32,

    /// Light size (for soft shadows)
    pub light_size: f32,

    /// Padding to align to 16 bytes
    pub _pad: f32,
}

impl Default for GpuCascadeShadow {
    fn default() -> Self {
        Self {
            matrices: [[[0.0; 4]; 4]; MAX_CASCADES],
            splits: [0.0; 4],
            layers: [-1; 4],
            cascade_count: 0,
            blend_distance: 2.0,
            bias: 0.005,
            normal_bias: 0.02,
            strength: 1.0,
            texel_size: 1.0 / 2048.0,
            light_size: 1.0,
            _pad: 0.0,
        }
    }
}

impl GpuCascadeShadow {
    /// Create disabled cascade shadow
    pub fn disabled() -> Self {
        Self::default()
    }

    /// Check if cascades are enabled
    pub fn is_enabled(&self) -> bool {
        self.cascade_count > 0 && self.layers[0] >= 0
    }

    /// Create from cascade data
    pub fn from_cascades(
        matrices: &[[[f32; 4]; 4]],
        splits: &[f32],
        layers: &[u32],
        cascade_count: u32,
        bias: f32,
        normal_bias: f32,
        strength: f32,
        resolution: u32,
        light_size: f32,
        blend_distance: f32,
    ) -> Self {
        let mut gpu_matrices = [[[0.0; 4]; 4]; MAX_CASCADES];
        let mut gpu_splits = [0.0f32; 4];
        let mut gpu_layers = [-1i32; 4];

        let count = cascade_count.min(MAX_CASCADES as u32) as usize;

        for i in 0..count {
            if i < matrices.len() {
                gpu_matrices[i] = matrices[i];
            }
            if i + 1 < splits.len() {
                gpu_splits[i] = splits[i + 1]; // Skip near plane, use cascade ends
            }
            if i < layers.len() {
                gpu_layers[i] = layers[i] as i32;
            }
        }

        Self {
            matrices: gpu_matrices,
            splits: gpu_splits,
            layers: gpu_layers,
            cascade_count,
            blend_distance,
            bias,
            normal_bias,
            strength,
            texel_size: 1.0 / resolution as f32,
            light_size,
            _pad: 0.0,
        }
    }
}

/// Shadow buffer containing all shadow data for GPU upload
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct ShadowBuffer {
    /// Spot light shadows
    pub spot_shadows: Vec<GpuShadowLight>,

    /// Point light shadows (cube map indices)
    pub point_shadows: Vec<GpuShadowLight>,

    /// Directional light cascade shadows
    pub directional_shadows: Vec<GpuCascadeShadow>,

    /// Global shadow uniforms
    pub uniforms: GpuShadowUniforms,
}

impl ShadowBuffer {
    /// Create a new shadow buffer
    pub fn new() -> Self {
        Self::default()
    }

    /// Clear all shadow data
    pub fn clear(&mut self) {
        self.spot_shadows.clear();
        self.point_shadows.clear();
        self.directional_shadows.clear();
    }

    /// Add a spot light shadow
    pub fn add_spot(&mut self, shadow: GpuShadowLight) -> usize {
        let index = self.spot_shadows.len();
        self.spot_shadows.push(shadow);
        index
    }

    /// Add a point light shadow
    pub fn add_point(&mut self, shadow: GpuShadowLight) -> usize {
        let index = self.point_shadows.len();
        self.point_shadows.push(shadow);
        index
    }

    /// Add a directional light cascade shadow
    pub fn add_directional(&mut self, shadow: GpuCascadeShadow) -> usize {
        let index = self.directional_shadows.len();
        self.directional_shadows.push(shadow);
        index
    }

    /// Get spot shadow data as bytes
    pub fn spot_shadows_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.spot_shadows)
    }

    /// Get point shadow data as bytes
    pub fn point_shadows_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.point_shadows)
    }

    /// Get directional shadow data as bytes
    pub fn directional_shadows_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.directional_shadows)
    }

    /// Get uniform data as bytes
    pub fn uniforms_bytes(&self) -> &[u8] {
        bytemuck::bytes_of(&self.uniforms)
    }

    /// Update uniforms
    pub fn update_uniforms(&mut self) {
        self.uniforms.spot_shadow_count = self.spot_shadows.len() as u32;
        self.uniforms.point_shadow_count = self.point_shadows.len() as u32;
        self.uniforms.directional_shadow_count = self.directional_shadows.len() as u32;
    }

    /// Get total shadow count
    pub fn total_shadows(&self) -> usize {
        self.spot_shadows.len() + self.point_shadows.len() + self.directional_shadows.len()
    }
}

/// Global shadow uniforms
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuShadowUniforms {
    /// Number of active spot light shadows
    pub spot_shadow_count: u32,

    /// Number of active point light shadows
    pub point_shadow_count: u32,

    /// Number of active directional light cascade shadows
    pub directional_shadow_count: u32,

    /// PCF filter size (1, 3, 5, 7)
    pub pcf_filter_size: u32,

    /// Enable soft shadows (PCSS)
    pub soft_shadows: u32,

    /// Shadow atlas resolution
    pub atlas_resolution: u32,

    /// Global shadow strength multiplier
    pub global_strength: f32,

    /// Padding
    pub _pad: f32,
}

impl GpuShadowUniforms {
    /// Create default uniforms
    pub fn new(
        atlas_resolution: u32,
        pcf_filter_size: u32,
        soft_shadows: bool,
    ) -> Self {
        Self {
            spot_shadow_count: 0,
            point_shadow_count: 0,
            directional_shadow_count: 0,
            pcf_filter_size,
            soft_shadows: if soft_shadows { 1 } else { 0 },
            atlas_resolution,
            global_strength: 1.0,
            _pad: 0.0,
        }
    }
}

/// Shadow caster data for the shadow pass
#[derive(Clone, Debug)]
pub struct ShadowCasterData {
    /// Entity ID
    pub entity_id: u64,

    /// Model matrix
    pub model_matrix: [[f32; 4]; 4],

    /// Mesh ID for batching
    pub mesh_id: u64,

    /// Bounding sphere center (world space)
    pub bounds_center: [f32; 3],

    /// Bounding sphere radius
    pub bounds_radius: f32,

    /// Does this entity cast shadows
    pub cast_shadows: bool,
}

impl ShadowCasterData {
    /// Check if caster is inside a cascade's view frustum
    pub fn in_cascade(&self, cascade_matrix: &[[f32; 4]; 4], cascade_radius: f32) -> bool {
        // Transform center to light space
        let c = self.bounds_center;
        let x = cascade_matrix[0][0] * c[0] + cascade_matrix[1][0] * c[1] + cascade_matrix[2][0] * c[2] + cascade_matrix[3][0];
        let y = cascade_matrix[0][1] * c[0] + cascade_matrix[1][1] * c[1] + cascade_matrix[2][1] * c[2] + cascade_matrix[3][1];
        let z = cascade_matrix[0][2] * c[0] + cascade_matrix[1][2] * c[1] + cascade_matrix[2][2] * c[2] + cascade_matrix[3][2];
        let w = cascade_matrix[0][3] * c[0] + cascade_matrix[1][3] * c[1] + cascade_matrix[2][3] * c[2] + cascade_matrix[3][3];

        if w.abs() < 0.0001 {
            return false;
        }

        let ndc_x = x / w;
        let ndc_y = y / w;
        let ndc_z = z / w;

        // Expand bounds by bounding sphere radius normalized to light space
        let r = self.bounds_radius / cascade_radius;

        // Check if in NDC bounds with radius expansion
        ndc_x >= -1.0 - r && ndc_x <= 1.0 + r &&
        ndc_y >= -1.0 - r && ndc_y <= 1.0 + r &&
        ndc_z >= 0.0 - r && ndc_z <= 1.0 + r
    }
}

/// Shadow receiver data for the main pass
#[derive(Clone, Copy, Debug, Default)]
pub struct ShadowReceiverData {
    /// Index of directional shadow to use (-1 = none)
    pub directional_shadow_index: i32,

    /// Index of spot shadow to use (-1 = none)
    pub spot_shadow_index: i32,

    /// Index of point shadow to use (-1 = none)
    pub point_shadow_index: i32,

    /// Receive shadows flag
    pub receive_shadows: bool,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_gpu_shadow_light_size() {
        // Verify GPU alignment
        assert_eq!(core::mem::size_of::<GpuShadowLight>() % 16, 0);
    }

    #[test]
    fn test_gpu_cascade_shadow_size() {
        // Verify GPU alignment (256 + 16 + 16 + 32 = 320 bytes)
        assert_eq!(core::mem::size_of::<GpuCascadeShadow>() % 16, 0);
    }

    #[test]
    fn test_gpu_shadow_uniforms_size() {
        assert_eq!(core::mem::size_of::<GpuShadowUniforms>() % 16, 0);
    }

    #[test]
    fn test_gpu_shadow_light_disabled() {
        let light = GpuShadowLight::disabled();
        assert!(!light.is_enabled());
        assert_eq!(light.layer, -1);
    }

    #[test]
    fn test_gpu_shadow_light_enabled() {
        let light = GpuShadowLight::new(
            [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
            0,
            0.005,
            0.02,
            1.0,
            0.1,
            100.0,
            2048,
            1.0,
        );

        assert!(light.is_enabled());
        assert_eq!(light.layer, 0);
        assert!((light.texel_size - 1.0 / 2048.0).abs() < 0.0001);
    }

    #[test]
    fn test_gpu_cascade_shadow() {
        let matrices = [
            [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
            [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
        ];
        let splits = [0.1, 10.0, 50.0];
        let layers = [0, 1];

        let cascade = GpuCascadeShadow::from_cascades(
            &matrices,
            &splits,
            &layers,
            2,
            0.005,
            0.02,
            1.0,
            2048,
            1.0,
            2.0,
        );

        assert!(cascade.is_enabled());
        assert_eq!(cascade.cascade_count, 2);
        assert_eq!(cascade.layers[0], 0);
        assert_eq!(cascade.layers[1], 1);
        assert_eq!(cascade.splits[0], 10.0);
        assert_eq!(cascade.splits[1], 50.0);
    }

    #[test]
    fn test_shadow_buffer() {
        let mut buffer = ShadowBuffer::new();

        buffer.add_spot(GpuShadowLight::new(
            [[1.0; 4]; 4],
            0, 0.005, 0.02, 1.0, 0.1, 100.0, 2048, 1.0,
        ));

        buffer.add_directional(GpuCascadeShadow::default());
        buffer.update_uniforms();

        assert_eq!(buffer.uniforms.spot_shadow_count, 1);
        assert_eq!(buffer.uniforms.directional_shadow_count, 1);
        assert_eq!(buffer.total_shadows(), 2);
    }

    #[test]
    fn test_shadow_buffer_bytes() {
        let mut buffer = ShadowBuffer::new();
        buffer.add_spot(GpuShadowLight::disabled());

        let bytes = buffer.spot_shadows_bytes();
        assert_eq!(bytes.len(), core::mem::size_of::<GpuShadowLight>());
    }

    #[test]
    fn test_shadow_caster_in_cascade() {
        let caster = ShadowCasterData {
            entity_id: 1,
            model_matrix: [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]],
            mesh_id: 1,
            bounds_center: [0.0, 0.0, 0.0],
            bounds_radius: 1.0,
            cast_shadows: true,
        };

        // Identity matrix should include origin
        let identity = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];
        assert!(caster.in_cascade(&identity, 10.0));
    }

    #[test]
    fn test_shadow_uniforms() {
        let uniforms = GpuShadowUniforms::new(2048, 3, true);

        assert_eq!(uniforms.atlas_resolution, 2048);
        assert_eq!(uniforms.pcf_filter_size, 3);
        assert_eq!(uniforms.soft_shadows, 1);
    }

    #[test]
    fn test_shadow_buffer_serialization() {
        let mut buffer = ShadowBuffer::new();
        buffer.add_spot(GpuShadowLight::new(
            [[1.0; 4]; 4],
            0, 0.005, 0.02, 1.0, 0.1, 100.0, 2048, 1.0,
        ));
        buffer.update_uniforms();

        let json = serde_json::to_string(&buffer).unwrap();
        let restored: ShadowBuffer = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.spot_shadows.len(), 1);
        assert_eq!(restored.uniforms.spot_shadow_count, 1);
    }
}
