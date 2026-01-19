//! Screen-Space Ambient Occlusion (SSAO) Pass
//!
//! Computes ambient occlusion from depth and normal buffers to
//! add contact shadows and depth perception.
//!
//! # Algorithm
//!
//! 1. Sample depth buffer in hemisphere around each pixel
//! 2. Compare depths to estimate occlusion
//! 3. Blur the result to reduce noise
//! 4. Apply to lighting
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::builtin::SSAOPass;
//!
//! let ssao = SSAOPass::new(1.0, 0.5);
//! registry.register(ssao)?;
//! ```

use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::pass::custom::{
    CustomRenderPass, GBufferChannel, PassConfigData, PassError, PassExecuteContext,
    PassPriority, PassSetupContext, ResourceRef, ResourceRequirements, TextureFormatHint,
};

/// Configuration for the SSAO pass
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SSAOPassConfig {
    /// Occlusion radius in world units
    pub radius: f32,

    /// Occlusion intensity (0.0 - 2.0)
    pub intensity: f32,

    /// Bias to prevent self-occlusion
    pub bias: f32,

    /// Number of samples per pixel (16, 32, or 64)
    pub sample_count: u32,

    /// Blur kernel size (0 = no blur)
    pub blur_size: u32,

    /// Power curve for occlusion falloff
    pub power: f32,

    /// Whether SSAO is enabled
    pub enabled: bool,
}

impl Default for SSAOPassConfig {
    fn default() -> Self {
        Self {
            radius: 0.5,
            intensity: 1.0,
            bias: 0.025,
            sample_count: 32,
            blur_size: 4,
            power: 2.0,
            enabled: true,
        }
    }
}

/// Screen-space ambient occlusion pass
pub struct SSAOPass {
    /// Sampling radius
    radius: f32,

    /// Occlusion intensity
    intensity: f32,

    /// Depth bias
    bias: f32,

    /// Number of samples
    sample_count: u32,

    /// Blur kernel size
    blur_size: u32,

    /// Power curve
    power: f32,

    /// Enabled state
    enabled: bool,

    /// Initialized state
    initialized: bool,

    /// Surface size
    surface_size: (u32, u32),

    /// Noise texture size
    noise_size: u32,
}

impl SSAOPass {
    /// Create a new SSAO pass
    pub fn new(radius: f32, intensity: f32) -> Self {
        Self {
            radius: radius.max(0.1),
            intensity: intensity.clamp(0.0, 2.0),
            bias: 0.025,
            sample_count: 32,
            blur_size: 4,
            power: 2.0,
            enabled: true,
            initialized: false,
            surface_size: (1920, 1080),
            noise_size: 4,
        }
    }

    /// Create with full configuration
    pub fn with_config(config: SSAOPassConfig) -> Self {
        Self {
            radius: config.radius.max(0.1),
            intensity: config.intensity.clamp(0.0, 2.0),
            bias: config.bias,
            sample_count: config.sample_count.clamp(16, 64),
            blur_size: config.blur_size,
            power: config.power.max(0.1),
            enabled: config.enabled,
            initialized: false,
            surface_size: (1920, 1080),
            noise_size: 4,
        }
    }

    /// Set radius
    pub fn set_radius(&mut self, radius: f32) {
        self.radius = radius.max(0.1);
    }

    /// Get radius
    pub fn radius(&self) -> f32 {
        self.radius
    }

    /// Set intensity
    pub fn set_intensity(&mut self, intensity: f32) {
        self.intensity = intensity.clamp(0.0, 2.0);
    }

    /// Get intensity
    pub fn intensity(&self) -> f32 {
        self.intensity
    }

    /// Set bias
    pub fn set_bias(&mut self, bias: f32) {
        self.bias = bias.max(0.0);
    }

    /// Get bias
    pub fn bias(&self) -> f32 {
        self.bias
    }

    /// Set sample count
    pub fn set_sample_count(&mut self, count: u32) {
        self.sample_count = count.clamp(16, 64);
        // Need to regenerate kernel
        self.initialized = false;
    }

    /// Get sample count
    pub fn sample_count(&self) -> u32 {
        self.sample_count
    }

    /// Set blur size
    pub fn set_blur_size(&mut self, size: u32) {
        self.blur_size = size;
    }

    /// Get blur size
    pub fn blur_size(&self) -> u32 {
        self.blur_size
    }

    /// Get current configuration
    pub fn config(&self) -> SSAOPassConfig {
        SSAOPassConfig {
            radius: self.radius,
            intensity: self.intensity,
            bias: self.bias,
            sample_count: self.sample_count,
            blur_size: self.blur_size,
            power: self.power,
            enabled: self.enabled,
        }
    }
}

impl CustomRenderPass for SSAOPass {
    fn name(&self) -> &str {
        "ssao"
    }

    fn dependencies(&self) -> &[&str] {
        &["depth_prepass"]
    }

    fn reads(&self) -> Vec<ResourceRef> {
        vec![
            ResourceRef::MainDepth,
            ResourceRef::GBuffer(GBufferChannel::Normal),
        ]
    }

    fn writes(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::GBuffer(GBufferChannel::AO)]
    }

    fn setup(&mut self, context: &PassSetupContext) -> Result<(), PassError> {
        self.surface_size = context.surface_size;

        // In a real implementation:
        // - Generate sample kernel (hemisphere samples)
        // - Create noise texture (random rotation vectors)
        // - Create AO output texture
        // - Create blur textures
        // - Compile SSAO shader
        // - Compile blur shader

        self.initialized = true;
        Ok(())
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        if !self.enabled || !self.initialized {
            return Ok(());
        }

        let _depth = context
            .resources
            .main_depth()
            .ok_or_else(|| PassError::Resource("main_depth not found".into()))?;

        // In a real implementation:
        // 1. For each pixel, sample depth in hemisphere
        // 2. Compare depths and accumulate occlusion
        // 3. Blur the result
        // 4. Output to AO buffer

        Ok(())
    }

    fn cleanup(&mut self) {
        self.initialized = false;
    }

    fn resource_requirements(&self) -> ResourceRequirements {
        // AO texture (R8) + blur ping-pong (R8 x 2) + noise texture
        let pixels = self.surface_size.0 as u64 * self.surface_size.1 as u64;
        let ao_size = pixels; // R8 = 1 byte per pixel
        let blur_size = pixels * 2;
        let noise_size = (self.noise_size * self.noise_size * 4) as u64; // RGBA8

        ResourceRequirements {
            memory_bytes: ao_size + blur_size + noise_size,
            render_targets: 2,
            compute: false,
            time_budget_ms: 1.5,
            texture_formats: vec![TextureFormatHint::R8Unorm],
            ..Default::default()
        }
    }

    fn priority(&self) -> PassPriority {
        // Run early, after depth prepass
        PassPriority(PassPriority::SHADOW.0 + 10)
    }

    fn is_enabled(&self) -> bool {
        self.enabled
    }

    fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    fn on_resize(&mut self, new_size: (u32, u32)) {
        self.surface_size = new_size;
    }

    fn get_config(&self) -> Option<PassConfigData> {
        Some(PassConfigData {
            name: "ssao".to_string(),
            enabled: self.enabled,
            priority: self.priority().0,
            config: serde_json::to_value(self.config()).unwrap_or_default(),
        })
    }

    fn apply_config(&mut self, config: &PassConfigData) -> Result<(), PassError> {
        self.enabled = config.enabled;

        if let Ok(ssao_config) = serde_json::from_value::<SSAOPassConfig>(config.config.clone()) {
            self.radius = ssao_config.radius.max(0.1);
            self.intensity = ssao_config.intensity.clamp(0.0, 2.0);
            self.bias = ssao_config.bias;
            self.sample_count = ssao_config.sample_count.clamp(16, 64);
            self.blur_size = ssao_config.blur_size;
            self.power = ssao_config.power.max(0.1);
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ssao_pass_creation() {
        let ssao = SSAOPass::new(0.5, 1.0);
        assert_eq!(ssao.name(), "ssao");
        assert_eq!(ssao.radius(), 0.5);
        assert_eq!(ssao.intensity(), 1.0);
    }

    #[test]
    fn test_ssao_pass_config() {
        let config = SSAOPassConfig {
            radius: 0.8,
            intensity: 1.5,
            bias: 0.02,
            sample_count: 64,
            blur_size: 2,
            power: 1.5,
            enabled: true,
        };

        let ssao = SSAOPass::with_config(config);
        assert_eq!(ssao.radius(), 0.8);
        assert_eq!(ssao.intensity(), 1.5);
        assert_eq!(ssao.sample_count(), 64);
    }

    #[test]
    fn test_ssao_pass_clamping() {
        let mut ssao = SSAOPass::new(0.01, 0.5);
        assert_eq!(ssao.radius(), 0.1); // Clamped to minimum

        ssao.set_intensity(10.0);
        assert_eq!(ssao.intensity(), 2.0);

        ssao.set_sample_count(1000);
        assert_eq!(ssao.sample_count(), 64);

        ssao.set_sample_count(1);
        assert_eq!(ssao.sample_count(), 16);
    }

    #[test]
    fn test_ssao_pass_dependencies() {
        let ssao = SSAOPass::new(0.5, 1.0);
        assert_eq!(ssao.dependencies(), &["depth_prepass"]);
    }

    #[test]
    fn test_ssao_pass_resources() {
        let ssao = SSAOPass::new(0.5, 1.0);
        let reads = ssao.reads();
        let writes = ssao.writes();

        assert!(reads.contains(&ResourceRef::MainDepth));
        assert!(writes.contains(&ResourceRef::GBuffer(GBufferChannel::AO)));
    }

    #[test]
    fn test_ssao_pass_config_serialization() {
        let config = SSAOPassConfig::default();
        let json = serde_json::to_string(&config).unwrap();
        let restored: SSAOPassConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(config.radius, restored.radius);
        assert_eq!(config.intensity, restored.intensity);
        assert_eq!(config.sample_count, restored.sample_count);
    }

    #[test]
    fn test_ssao_pass_setup() {
        let mut ssao = SSAOPass::new(0.5, 1.0);
        let ctx = PassSetupContext::default();

        assert!(ssao.setup(&ctx).is_ok());
        assert!(ssao.initialized);
    }
}
