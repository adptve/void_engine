//! Bloom Post-Process Pass
//!
//! HDR bloom effect that extracts bright pixels and blurs them
//! to create a glow effect.
//!
//! # Algorithm
//!
//! 1. Threshold pass: Extract pixels above brightness threshold
//! 2. Downsample: Create mip chain with progressive blur
//! 3. Blur: Apply Gaussian blur at each mip level
//! 4. Upsample: Combine mip levels back together
//! 5. Composite: Add bloom result to main color
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::builtin::BloomPass;
//!
//! let bloom = BloomPass::new(1.0, 0.5);
//! registry.register(bloom)?;
//! ```

use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::pass::custom::{
    CustomRenderPass, PassConfigData, PassError, PassExecuteContext, PassPriority,
    PassSetupContext, ResourceRef, ResourceRequirements, TextureFormatHint,
};

/// Configuration for the bloom pass
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BloomPassConfig {
    /// Brightness threshold for bloom extraction (0.0 - 10.0)
    pub threshold: f32,

    /// Bloom intensity (0.0 - 2.0)
    pub intensity: f32,

    /// Number of blur passes (3 - 8)
    pub blur_passes: u32,

    /// Knee for soft threshold (0.0 - 1.0)
    pub knee: f32,

    /// Whether bloom is enabled
    pub enabled: bool,
}

impl Default for BloomPassConfig {
    fn default() -> Self {
        Self {
            threshold: 1.0,
            intensity: 0.5,
            blur_passes: 5,
            knee: 0.5,
            enabled: true,
        }
    }
}

/// Bloom post-process pass
pub struct BloomPass {
    /// Brightness threshold
    threshold: f32,

    /// Bloom intensity
    intensity: f32,

    /// Number of blur passes (mip levels)
    blur_passes: u32,

    /// Soft knee for threshold
    knee: f32,

    /// Whether the pass is enabled
    enabled: bool,

    /// Whether GPU resources are initialized
    initialized: bool,

    /// Current surface size
    surface_size: (u32, u32),

    /// Mip chain sizes
    mip_sizes: Vec<(u32, u32)>,
}

impl BloomPass {
    /// Create a new bloom pass
    pub fn new(threshold: f32, intensity: f32) -> Self {
        Self {
            threshold: threshold.max(0.0),
            intensity: intensity.clamp(0.0, 2.0),
            blur_passes: 5,
            knee: 0.5,
            enabled: true,
            initialized: false,
            surface_size: (1920, 1080),
            mip_sizes: Vec::new(),
        }
    }

    /// Create with full configuration
    pub fn with_config(config: BloomPassConfig) -> Self {
        Self {
            threshold: config.threshold,
            intensity: config.intensity,
            blur_passes: config.blur_passes.clamp(3, 8),
            knee: config.knee,
            enabled: config.enabled,
            initialized: false,
            surface_size: (1920, 1080),
            mip_sizes: Vec::new(),
        }
    }

    /// Set threshold
    pub fn set_threshold(&mut self, threshold: f32) {
        self.threshold = threshold.max(0.0);
    }

    /// Get threshold
    pub fn threshold(&self) -> f32 {
        self.threshold
    }

    /// Set intensity
    pub fn set_intensity(&mut self, intensity: f32) {
        self.intensity = intensity.clamp(0.0, 2.0);
    }

    /// Get intensity
    pub fn intensity(&self) -> f32 {
        self.intensity
    }

    /// Set number of blur passes
    pub fn set_blur_passes(&mut self, count: u32) {
        self.blur_passes = count.clamp(3, 8);
        // Need to reinitialize if this changes
        self.initialized = false;
    }

    /// Get number of blur passes
    pub fn blur_passes(&self) -> u32 {
        self.blur_passes
    }

    /// Set knee for soft threshold
    pub fn set_knee(&mut self, knee: f32) {
        self.knee = knee.clamp(0.0, 1.0);
    }

    /// Get knee
    pub fn knee(&self) -> f32 {
        self.knee
    }

    /// Get current configuration
    pub fn config(&self) -> BloomPassConfig {
        BloomPassConfig {
            threshold: self.threshold,
            intensity: self.intensity,
            blur_passes: self.blur_passes,
            knee: self.knee,
            enabled: self.enabled,
        }
    }

    /// Calculate mip chain sizes
    fn calculate_mip_sizes(&mut self, width: u32, height: u32) {
        self.mip_sizes.clear();

        let mut w = width / 2;
        let mut h = height / 2;

        for _ in 0..self.blur_passes {
            self.mip_sizes.push((w.max(1), h.max(1)));
            w /= 2;
            h /= 2;
        }
    }

    /// Estimate memory usage based on surface size
    fn estimate_memory(&self, width: u32, height: u32) -> u64 {
        // Each mip level uses RGBA16Float (8 bytes per pixel)
        let bytes_per_pixel = 8u64;
        let mut total = 0u64;

        let mut w = width / 2;
        let mut h = height / 2;

        for _ in 0..self.blur_passes {
            // Two textures per mip (ping-pong for blur)
            total += 2 * w as u64 * h as u64 * bytes_per_pixel;
            w = (w / 2).max(1);
            h = (h / 2).max(1);
        }

        total
    }
}

impl CustomRenderPass for BloomPass {
    fn name(&self) -> &str {
        "bloom"
    }

    fn dependencies(&self) -> &[&str] {
        &["main"]
    }

    fn reads(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor]
    }

    fn writes(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor]
    }

    fn setup(&mut self, context: &PassSetupContext) -> Result<(), PassError> {
        self.surface_size = context.surface_size;
        self.calculate_mip_sizes(context.surface_size.0, context.surface_size.1);

        // In a real implementation, this would create GPU resources:
        // - Mip chain textures
        // - Threshold pipeline
        // - Blur pipeline (horizontal + vertical)
        // - Composite pipeline

        self.initialized = true;
        Ok(())
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        if !self.enabled || !self.initialized {
            return Ok(());
        }

        // Get main color texture
        let _main_color = context
            .resources
            .main_color()
            .ok_or_else(|| PassError::Resource("main_color not found".into()))?;

        // In a real implementation, this would:
        // 1. Render threshold pass to extract bright pixels
        // 2. Downsample through mip chain
        // 3. Blur each mip level
        // 4. Upsample and combine
        // 5. Composite back to main color

        // For now, this is a placeholder that demonstrates the interface
        // The actual GPU work would be done through wgpu commands

        Ok(())
    }

    fn cleanup(&mut self) {
        self.initialized = false;
        self.mip_sizes.clear();
    }

    fn resource_requirements(&self) -> ResourceRequirements {
        ResourceRequirements {
            memory_bytes: self.estimate_memory(1920, 1080), // Default estimate
            render_targets: 2, // Ping-pong buffers
            compute: true,
            time_budget_ms: 1.0,
            texture_formats: vec![TextureFormatHint::Rgba16Float],
            ..Default::default()
        }
    }

    fn priority(&self) -> PassPriority {
        PassPriority::POST_PROCESS
    }

    fn is_enabled(&self) -> bool {
        self.enabled
    }

    fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    fn on_resize(&mut self, new_size: (u32, u32)) {
        self.surface_size = new_size;
        self.calculate_mip_sizes(new_size.0, new_size.1);
        // In a real implementation, would recreate mip chain textures
    }

    fn get_config(&self) -> Option<PassConfigData> {
        Some(PassConfigData {
            name: "bloom".to_string(),
            enabled: self.enabled,
            priority: PassPriority::POST_PROCESS.0,
            config: serde_json::to_value(self.config()).unwrap_or_default(),
        })
    }

    fn apply_config(&mut self, config: &PassConfigData) -> Result<(), PassError> {
        self.enabled = config.enabled;

        if let Ok(bloom_config) = serde_json::from_value::<BloomPassConfig>(config.config.clone()) {
            self.threshold = bloom_config.threshold;
            self.intensity = bloom_config.intensity;
            self.blur_passes = bloom_config.blur_passes.clamp(3, 8);
            self.knee = bloom_config.knee;
        }

        Ok(())
    }
}

/// Simplified bloom pass with reduced quality (fallback)
pub struct BloomPassSimple {
    intensity: f32,
    enabled: bool,
}

impl BloomPassSimple {
    /// Create a simple bloom pass
    pub fn new(intensity: f32) -> Self {
        Self {
            intensity: intensity.clamp(0.0, 2.0),
            enabled: true,
        }
    }
}

impl CustomRenderPass for BloomPassSimple {
    fn name(&self) -> &str {
        "bloom_simple"
    }

    fn dependencies(&self) -> &[&str] {
        &["main"]
    }

    fn reads(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor]
    }

    fn writes(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor]
    }

    fn execute(&self, _context: &PassExecuteContext) -> Result<(), PassError> {
        if !self.enabled {
            return Ok(());
        }

        // Simple additive bloom without blur - fast fallback
        // In a real implementation: single-pass bloom approximation

        Ok(())
    }

    fn resource_requirements(&self) -> ResourceRequirements {
        ResourceRequirements {
            memory_bytes: 4 * 1024 * 1024, // Much smaller - ~4MB
            render_targets: 1,
            compute: false,
            time_budget_ms: 0.2,
            ..Default::default()
        }
    }

    fn priority(&self) -> PassPriority {
        PassPriority::POST_PROCESS
    }

    fn is_enabled(&self) -> bool {
        self.enabled
    }

    fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bloom_pass_creation() {
        let bloom = BloomPass::new(1.0, 0.5);
        assert_eq!(bloom.name(), "bloom");
        assert_eq!(bloom.threshold(), 1.0);
        assert_eq!(bloom.intensity(), 0.5);
        assert!(bloom.is_enabled());
    }

    #[test]
    fn test_bloom_pass_config() {
        let config = BloomPassConfig {
            threshold: 1.5,
            intensity: 0.8,
            blur_passes: 6,
            knee: 0.3,
            enabled: true,
        };

        let bloom = BloomPass::with_config(config.clone());
        assert_eq!(bloom.threshold(), 1.5);
        assert_eq!(bloom.intensity(), 0.8);
        assert_eq!(bloom.blur_passes(), 6);
    }

    #[test]
    fn test_bloom_pass_setters() {
        let mut bloom = BloomPass::new(1.0, 0.5);

        bloom.set_threshold(2.0);
        assert_eq!(bloom.threshold(), 2.0);

        bloom.set_intensity(1.0);
        assert_eq!(bloom.intensity(), 1.0);

        bloom.set_blur_passes(7);
        assert_eq!(bloom.blur_passes(), 7);

        bloom.set_knee(0.8);
        assert_eq!(bloom.knee(), 0.8);
    }

    #[test]
    fn test_bloom_pass_clamping() {
        let mut bloom = BloomPass::new(1.0, 0.5);

        bloom.set_intensity(-1.0);
        assert_eq!(bloom.intensity(), 0.0);

        bloom.set_intensity(10.0);
        assert_eq!(bloom.intensity(), 2.0);

        bloom.set_blur_passes(1);
        assert_eq!(bloom.blur_passes(), 3);

        bloom.set_blur_passes(20);
        assert_eq!(bloom.blur_passes(), 8);
    }

    #[test]
    fn test_bloom_pass_dependencies() {
        let bloom = BloomPass::new(1.0, 0.5);
        assert_eq!(bloom.dependencies(), &["main"]);
    }

    #[test]
    fn test_bloom_pass_resources() {
        let bloom = BloomPass::new(1.0, 0.5);
        let reads = bloom.reads();
        let writes = bloom.writes();

        assert!(reads.contains(&ResourceRef::MainColor));
        assert!(writes.contains(&ResourceRef::MainColor));
    }

    #[test]
    fn test_bloom_pass_resource_requirements() {
        let bloom = BloomPass::new(1.0, 0.5);
        let reqs = bloom.resource_requirements();

        assert!(reqs.memory_bytes > 0);
        assert!(reqs.compute);
        assert_eq!(reqs.render_targets, 2);
    }

    #[test]
    fn test_bloom_pass_config_serialization() {
        let config = BloomPassConfig {
            threshold: 1.2,
            intensity: 0.7,
            blur_passes: 5,
            knee: 0.4,
            enabled: true,
        };

        let json = serde_json::to_string(&config).unwrap();
        let restored: BloomPassConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(config.threshold, restored.threshold);
        assert_eq!(config.intensity, restored.intensity);
        assert_eq!(config.blur_passes, restored.blur_passes);
    }

    #[test]
    fn test_bloom_simple_pass() {
        let simple = BloomPassSimple::new(0.5);
        let reqs = simple.resource_requirements();

        assert!(reqs.memory_bytes < BloomPass::new(1.0, 0.5).resource_requirements().memory_bytes);
        assert!(!reqs.compute);
    }

    #[test]
    fn test_bloom_pass_setup() {
        let mut bloom = BloomPass::new(1.0, 0.5);
        let ctx = PassSetupContext {
            surface_size: (1920, 1080),
            ..Default::default()
        };

        assert!(bloom.setup(&ctx).is_ok());
        assert!(bloom.initialized);
        assert!(!bloom.mip_sizes.is_empty());
    }

    #[test]
    fn test_bloom_pass_resize() {
        let mut bloom = BloomPass::new(1.0, 0.5);

        let ctx = PassSetupContext {
            surface_size: (1920, 1080),
            ..Default::default()
        };
        bloom.setup(&ctx).unwrap();

        let original_mip_count = bloom.mip_sizes.len();

        bloom.on_resize((3840, 2160));

        assert_eq!(bloom.surface_size, (3840, 2160));
        assert_eq!(bloom.mip_sizes.len(), original_mip_count);
    }

    #[test]
    fn test_bloom_pass_get_apply_config() {
        let mut bloom = BloomPass::new(1.0, 0.5);

        let config = bloom.get_config().unwrap();
        assert_eq!(config.name, "bloom");

        // Modify and apply
        let modified = PassConfigData {
            name: "bloom".into(),
            enabled: false,
            priority: 100,
            config: serde_json::json!({
                "threshold": 2.0,
                "intensity": 1.0,
                "blur_passes": 6,
                "knee": 0.3,
                "enabled": false
            }),
        };

        bloom.apply_config(&modified).unwrap();

        assert!(!bloom.is_enabled());
        assert_eq!(bloom.threshold(), 2.0);
        assert_eq!(bloom.intensity(), 1.0);
    }
}
