//! GPU Light Buffer Management
//!
//! Manages GPU buffer resources for light data:
//! - GPU-ready light data structures (Pod/Zeroable)
//! - Light buffer with limits enforcement
//! - Light extraction from ECS components
//! - Hot-reload serialization support
//!
//! # Light Limits
//!
//! Default limits (configurable):
//! - Directional lights: 4 (uniform buffer)
//! - Point lights: 256 (storage buffer)
//! - Spot lights: 128 (storage buffer)

use alloc::vec::Vec;
use serde::{Serialize, Deserialize};

/// Maximum directional lights (uniform buffer, small count)
pub const MAX_DIRECTIONAL_LIGHTS: usize = 4;
/// Maximum point lights (storage buffer, larger count)
pub const MAX_POINT_LIGHTS: usize = 256;
/// Maximum spot lights (storage buffer)
pub const MAX_SPOT_LIGHTS: usize = 128;

/// GPU-ready directional light data
///
/// Matches shader struct layout with proper alignment.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuDirectionalLight {
    /// Light direction (normalized, world space)
    pub direction: [f32; 3],
    /// Padding for alignment
    pub _pad0: f32,
    /// Light color (linear RGB)
    pub color: [f32; 3],
    /// Intensity in lux
    pub intensity: f32,
    /// Shadow view-projection matrix (4x4 column-major)
    pub shadow_matrix: [[f32; 4]; 4],
    /// Shadow map array index (-1 if no shadows)
    pub shadow_map_index: i32,
    /// Padding for alignment
    pub _pad1: [f32; 3],
}

impl GpuDirectionalLight {
    /// Size in bytes (must be 16-byte aligned)
    pub const SIZE: usize = core::mem::size_of::<Self>();

    /// Create a new directional light
    pub fn new(direction: [f32; 3], color: [f32; 3], intensity: f32) -> Self {
        Self {
            direction,
            _pad0: 0.0,
            color,
            intensity,
            shadow_matrix: [[0.0; 4]; 4],
            shadow_map_index: -1,
            _pad1: [0.0; 3],
        }
    }
}

/// GPU-ready point light data
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuPointLight {
    /// World position
    pub position: [f32; 3],
    /// Maximum range
    pub range: f32,
    /// Light color (linear RGB)
    pub color: [f32; 3],
    /// Intensity in lumens
    pub intensity: f32,
    /// Attenuation coefficients [constant, linear, quadratic]
    pub attenuation: [f32; 3],
    /// Shadow cubemap index (-1 if no shadows)
    pub shadow_map_index: i32,
}

impl GpuPointLight {
    /// Size in bytes
    pub const SIZE: usize = core::mem::size_of::<Self>();

    /// Create a new point light
    pub fn new(position: [f32; 3], range: f32, color: [f32; 3], intensity: f32, attenuation: [f32; 3]) -> Self {
        Self {
            position,
            range,
            color,
            intensity,
            attenuation,
            shadow_map_index: -1,
        }
    }
}

/// GPU-ready spot light data
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct GpuSpotLight {
    /// World position
    pub position: [f32; 3],
    /// Maximum range
    pub range: f32,
    /// Light direction (normalized)
    pub direction: [f32; 3],
    /// Cosine of inner cone angle
    pub inner_cos: f32,
    /// Light color (linear RGB)
    pub color: [f32; 3],
    /// Cosine of outer cone angle
    pub outer_cos: f32,
    /// Attenuation coefficients
    pub attenuation: [f32; 3],
    /// Intensity in lumens
    pub intensity: f32,
    /// Shadow view-projection matrix
    pub shadow_matrix: [[f32; 4]; 4],
    /// Shadow map index
    pub shadow_map_index: i32,
    /// Padding
    pub _pad: [f32; 3],
}

impl GpuSpotLight {
    /// Size in bytes
    pub const SIZE: usize = core::mem::size_of::<Self>();

    /// Create a new spot light
    pub fn new(
        position: [f32; 3],
        direction: [f32; 3],
        range: f32,
        inner_angle: f32,
        outer_angle: f32,
        color: [f32; 3],
        intensity: f32,
        attenuation: [f32; 3],
    ) -> Self {
        Self {
            position,
            range,
            direction,
            inner_cos: inner_angle.cos(),
            color,
            outer_cos: outer_angle.cos(),
            attenuation,
            intensity,
            shadow_matrix: [[0.0; 4]; 4],
            shadow_map_index: -1,
            _pad: [0.0; 3],
        }
    }
}

/// Light counts for shader uniform
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize, bytemuck::Pod, bytemuck::Zeroable)]
pub struct LightCounts {
    /// Number of active directional lights
    pub directional_count: u32,
    /// Number of active point lights
    pub point_count: u32,
    /// Number of active spot lights
    pub spot_count: u32,
    /// Padding
    pub _pad: u32,
}

/// Light buffer configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LightBufferConfig {
    /// Maximum directional lights
    pub max_directional: usize,
    /// Maximum point lights
    pub max_point: usize,
    /// Maximum spot lights
    pub max_spot: usize,
}

impl Default for LightBufferConfig {
    fn default() -> Self {
        Self {
            max_directional: MAX_DIRECTIONAL_LIGHTS,
            max_point: MAX_POINT_LIGHTS,
            max_spot: MAX_SPOT_LIGHTS,
        }
    }
}

/// CPU-side light buffer for collecting lights before GPU upload
///
/// This is backend-agnostic; actual GPU buffer creation is handled
/// by the rendering backend.
#[derive(Clone, Debug, Default)]
pub struct LightBuffer {
    /// Directional lights
    pub directional_lights: Vec<GpuDirectionalLight>,
    /// Point lights
    pub point_lights: Vec<GpuPointLight>,
    /// Spot lights
    pub spot_lights: Vec<GpuSpotLight>,
    /// Configuration
    config: LightBufferConfig,
    /// Current frame
    frame: u64,
    /// Statistics
    stats: LightBufferStats,
}

/// Light buffer statistics
#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct LightBufferStats {
    /// Directional lights added
    pub directional_count: u32,
    /// Point lights added
    pub point_count: u32,
    /// Spot lights added
    pub spot_count: u32,
    /// Lights culled by distance
    pub culled_count: u32,
    /// Lights over limit (dropped)
    pub overflow_count: u32,
}

impl LightBuffer {
    /// Create a new light buffer with default configuration
    pub fn new() -> Self {
        Self::with_config(LightBufferConfig::default())
    }

    /// Create with custom configuration
    pub fn with_config(config: LightBufferConfig) -> Self {
        Self {
            directional_lights: Vec::with_capacity(config.max_directional),
            point_lights: Vec::with_capacity(config.max_point),
            spot_lights: Vec::with_capacity(config.max_spot),
            config,
            frame: 0,
            stats: LightBufferStats::default(),
        }
    }

    /// Clear all lights for new frame
    pub fn clear(&mut self) {
        self.directional_lights.clear();
        self.point_lights.clear();
        self.spot_lights.clear();
        self.stats = LightBufferStats::default();
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) {
        self.frame += 1;
        self.clear();
    }

    /// Add a directional light
    ///
    /// Returns true if added, false if at limit.
    pub fn add_directional(&mut self, light: GpuDirectionalLight) -> bool {
        if self.directional_lights.len() >= self.config.max_directional {
            self.stats.overflow_count += 1;
            return false;
        }
        self.directional_lights.push(light);
        self.stats.directional_count += 1;
        true
    }

    /// Add a point light
    pub fn add_point(&mut self, light: GpuPointLight) -> bool {
        if self.point_lights.len() >= self.config.max_point {
            self.stats.overflow_count += 1;
            return false;
        }
        self.point_lights.push(light);
        self.stats.point_count += 1;
        true
    }

    /// Add a spot light
    pub fn add_spot(&mut self, light: GpuSpotLight) -> bool {
        if self.spot_lights.len() >= self.config.max_spot {
            self.stats.overflow_count += 1;
            return false;
        }
        self.spot_lights.push(light);
        self.stats.spot_count += 1;
        true
    }

    /// Get light counts for shader
    pub fn counts(&self) -> LightCounts {
        LightCounts {
            directional_count: self.directional_lights.len() as u32,
            point_count: self.point_lights.len() as u32,
            spot_count: self.spot_lights.len() as u32,
            _pad: 0,
        }
    }

    /// Get directional lights as bytes for GPU upload
    pub fn directional_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.directional_lights)
    }

    /// Get point lights as bytes for GPU upload
    pub fn point_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.point_lights)
    }

    /// Get spot lights as bytes for GPU upload
    pub fn spot_bytes(&self) -> &[u8] {
        bytemuck::cast_slice(&self.spot_lights)
    }

    /// Get counts for GPU upload (call bytemuck::bytes_of on result)
    pub fn counts_data(&self) -> LightCounts {
        self.counts()
    }

    /// Get current statistics
    pub fn stats(&self) -> &LightBufferStats {
        &self.stats
    }

    /// Get current frame
    pub fn frame(&self) -> u64 {
        self.frame
    }

    /// Get total light count
    pub fn total_lights(&self) -> usize {
        self.directional_lights.len() + self.point_lights.len() + self.spot_lights.len()
    }

    /// Check if buffer has any lights
    pub fn has_lights(&self) -> bool {
        !self.directional_lights.is_empty()
            || !self.point_lights.is_empty()
            || !self.spot_lights.is_empty()
    }

    /// Sort point lights by distance to camera (closest first for importance)
    pub fn sort_point_lights_by_distance(&mut self, camera_pos: [f32; 3]) {
        self.point_lights.sort_by(|a, b| {
            let dist_a = distance_squared(a.position, camera_pos);
            let dist_b = distance_squared(b.position, camera_pos);
            dist_a.partial_cmp(&dist_b).unwrap_or(core::cmp::Ordering::Equal)
        });
    }

    /// Sort spot lights by distance to camera
    pub fn sort_spot_lights_by_distance(&mut self, camera_pos: [f32; 3]) {
        self.spot_lights.sort_by(|a, b| {
            let dist_a = distance_squared(a.position, camera_pos);
            let dist_b = distance_squared(b.position, camera_pos);
            dist_a.partial_cmp(&dist_b).unwrap_or(core::cmp::Ordering::Equal)
        });
    }

    /// Serialize state for hot-reload
    pub fn serialize_state(&self) -> LightBufferState {
        LightBufferState {
            directional_lights: self.directional_lights.clone(),
            point_lights: self.point_lights.clone(),
            spot_lights: self.spot_lights.clone(),
        }
    }

    /// Restore from serialized state
    pub fn restore_state(&mut self, state: LightBufferState) {
        self.directional_lights = state.directional_lights;
        self.point_lights = state.point_lights;
        self.spot_lights = state.spot_lights;

        // Enforce limits
        self.directional_lights.truncate(self.config.max_directional);
        self.point_lights.truncate(self.config.max_point);
        self.spot_lights.truncate(self.config.max_spot);

        // Update stats
        self.stats.directional_count = self.directional_lights.len() as u32;
        self.stats.point_count = self.point_lights.len() as u32;
        self.stats.spot_count = self.spot_lights.len() as u32;
    }
}

/// Serialized light buffer state for hot-reload
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LightBufferState {
    /// Directional lights
    pub directional_lights: Vec<GpuDirectionalLight>,
    /// Point lights
    pub point_lights: Vec<GpuPointLight>,
    /// Spot lights
    pub spot_lights: Vec<GpuSpotLight>,
}

/// Calculate squared distance between two points
fn distance_squared(a: [f32; 3], b: [f32; 3]) -> f32 {
    let dx = a[0] - b[0];
    let dy = a[1] - b[1];
    let dz = a[2] - b[2];
    dx * dx + dy * dy + dz * dz
}

/// Calculate distance between two points
pub fn distance(a: [f32; 3], b: [f32; 3]) -> f32 {
    distance_squared(a, b).sqrt()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_gpu_light_sizes() {
        // Verify 16-byte alignment
        assert_eq!(GpuDirectionalLight::SIZE % 16, 0);
        assert_eq!(GpuPointLight::SIZE % 16, 0);
        assert_eq!(GpuSpotLight::SIZE % 16, 0);
        assert_eq!(core::mem::size_of::<LightCounts>() % 16, 0);
    }

    #[test]
    fn test_light_buffer_basic() {
        let mut buffer = LightBuffer::new();
        buffer.begin_frame();

        assert!(buffer.add_directional(GpuDirectionalLight::new(
            [0.0, -1.0, 0.0],
            [1.0, 1.0, 1.0],
            1.0,
        )));

        assert!(buffer.add_point(GpuPointLight::new(
            [0.0, 5.0, 0.0],
            10.0,
            [1.0, 0.8, 0.6],
            1000.0,
            [1.0, 0.0, 1.0],
        )));

        let counts = buffer.counts();
        assert_eq!(counts.directional_count, 1);
        assert_eq!(counts.point_count, 1);
        assert_eq!(counts.spot_count, 0);
    }

    #[test]
    fn test_light_buffer_limits() {
        let config = LightBufferConfig {
            max_directional: 2,
            max_point: 3,
            max_spot: 2,
        };
        let mut buffer = LightBuffer::with_config(config);
        buffer.begin_frame();

        // Add up to limit
        assert!(buffer.add_directional(GpuDirectionalLight::default()));
        assert!(buffer.add_directional(GpuDirectionalLight::default()));

        // Over limit should fail
        assert!(!buffer.add_directional(GpuDirectionalLight::default()));
        assert_eq!(buffer.stats().overflow_count, 1);
    }

    #[test]
    fn test_spot_light_creation() {
        let spot = GpuSpotLight::new(
            [0.0, 5.0, 0.0],
            [0.0, -1.0, 0.0],
            15.0,
            30.0_f32.to_radians(),
            45.0_f32.to_radians(),
            [1.0, 1.0, 1.0],
            2000.0,
            [1.0, 0.0, 1.0],
        );

        assert!((spot.inner_cos - 30.0_f32.to_radians().cos()).abs() < 0.001);
        assert!((spot.outer_cos - 45.0_f32.to_radians().cos()).abs() < 0.001);
    }

    #[test]
    fn test_light_buffer_serialization() {
        let mut buffer = LightBuffer::new();
        buffer.begin_frame();

        buffer.add_directional(GpuDirectionalLight::new(
            [0.0, -1.0, 0.0],
            [1.0, 0.9, 0.8],
            100.0,
        ));
        buffer.add_point(GpuPointLight::new(
            [1.0, 2.0, 3.0],
            10.0,
            [1.0, 1.0, 1.0],
            1000.0,
            [1.0, 0.0, 1.0],
        ));

        // Serialize
        let state = buffer.serialize_state();

        // Restore to new buffer
        let mut new_buffer = LightBuffer::new();
        new_buffer.restore_state(state);

        assert_eq!(new_buffer.directional_lights.len(), 1);
        assert_eq!(new_buffer.point_lights.len(), 1);
        assert_eq!(new_buffer.directional_lights[0].intensity, 100.0);
    }

    #[test]
    fn test_light_sorting() {
        let mut buffer = LightBuffer::new();
        buffer.begin_frame();

        // Add lights at different distances
        buffer.add_point(GpuPointLight::new([10.0, 0.0, 0.0], 5.0, [1.0; 3], 100.0, [1.0, 0.0, 1.0]));
        buffer.add_point(GpuPointLight::new([1.0, 0.0, 0.0], 5.0, [1.0; 3], 100.0, [1.0, 0.0, 1.0]));
        buffer.add_point(GpuPointLight::new([5.0, 0.0, 0.0], 5.0, [1.0; 3], 100.0, [1.0, 0.0, 1.0]));

        buffer.sort_point_lights_by_distance([0.0, 0.0, 0.0]);

        // Should be sorted closest first
        assert_eq!(buffer.point_lights[0].position[0], 1.0);
        assert_eq!(buffer.point_lights[1].position[0], 5.0);
        assert_eq!(buffer.point_lights[2].position[0], 10.0);
    }

    #[test]
    fn test_distance() {
        let d = distance([0.0, 0.0, 0.0], [3.0, 4.0, 0.0]);
        assert!((d - 5.0).abs() < 0.001);
    }
}
