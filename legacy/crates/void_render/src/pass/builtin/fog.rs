//! Fog Pass
//!
//! Applies distance-based fog to the scene.
//!
//! # Fog Types
//!
//! - Linear: Fog increases linearly with distance
//! - Exponential: Fog increases exponentially
//! - Exponential Squared: More pronounced exponential falloff
//! - Height: Fog density varies with height
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::builtin::{FogPass, FogType};
//!
//! let fog = FogPass::new(FogType::Exponential, [0.5, 0.6, 0.7, 1.0])
//!     .with_density(0.02);
//! registry.register(fog)?;
//! ```

use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use serde::{Deserialize, Serialize};

use crate::pass::custom::{
    CustomRenderPass, PassConfigData, PassError, PassExecuteContext, PassPriority,
    PassSetupContext, ResourceRef, ResourceRequirements,
};

/// Fog calculation type
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum FogType {
    /// Linear fog: factor = (end - dist) / (end - start)
    Linear,
    /// Exponential fog: factor = exp(-density * dist)
    Exponential,
    /// Exponential squared: factor = exp(-(density * dist)^2)
    ExponentialSquared,
    /// Height-based fog with exponential falloff
    Height,
}

impl Default for FogType {
    fn default() -> Self {
        Self::Exponential
    }
}

/// Configuration for the fog pass
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct FogPassConfig {
    /// Fog type
    pub fog_type: FogType,

    /// Fog color (RGBA)
    pub color: [f32; 4],

    /// Fog density (for exponential types)
    pub density: f32,

    /// Start distance (for linear fog)
    pub start: f32,

    /// End distance (for linear fog)
    pub end: f32,

    /// Height falloff (for height fog)
    pub height_falloff: f32,

    /// Base height (for height fog)
    pub base_height: f32,

    /// Maximum fog amount (0.0 - 1.0)
    pub max_fog: f32,

    /// Whether fog is enabled
    pub enabled: bool,
}

impl Default for FogPassConfig {
    fn default() -> Self {
        Self {
            fog_type: FogType::Exponential,
            color: [0.5, 0.6, 0.7, 1.0], // Light blue-gray
            density: 0.02,
            start: 10.0,
            end: 100.0,
            height_falloff: 0.1,
            base_height: 0.0,
            max_fog: 1.0,
            enabled: true,
        }
    }
}

/// Distance-based fog pass
pub struct FogPass {
    /// Fog type
    fog_type: FogType,

    /// Fog color
    color: [f32; 4],

    /// Density (exponential)
    density: f32,

    /// Start distance (linear)
    start: f32,

    /// End distance (linear)
    end: f32,

    /// Height falloff
    height_falloff: f32,

    /// Base height
    base_height: f32,

    /// Maximum fog
    max_fog: f32,

    /// Enabled state
    enabled: bool,

    /// Initialized state
    initialized: bool,
}

impl FogPass {
    /// Create a new fog pass
    pub fn new(fog_type: FogType, color: [f32; 4]) -> Self {
        Self {
            fog_type,
            color,
            density: 0.02,
            start: 10.0,
            end: 100.0,
            height_falloff: 0.1,
            base_height: 0.0,
            max_fog: 1.0,
            enabled: true,
            initialized: false,
        }
    }

    /// Create with full configuration
    pub fn with_config(config: FogPassConfig) -> Self {
        Self {
            fog_type: config.fog_type,
            color: config.color,
            density: config.density.max(0.001),
            start: config.start,
            end: config.end,
            height_falloff: config.height_falloff.max(0.001),
            base_height: config.base_height,
            max_fog: config.max_fog.clamp(0.0, 1.0),
            enabled: config.enabled,
            initialized: false,
        }
    }

    /// Builder: set density
    pub fn with_density(mut self, density: f32) -> Self {
        self.density = density.max(0.001);
        self
    }

    /// Builder: set linear range
    pub fn with_linear_range(mut self, start: f32, end: f32) -> Self {
        self.start = start;
        self.end = end.max(start + 1.0);
        self
    }

    /// Builder: set height parameters
    pub fn with_height_fog(mut self, base_height: f32, falloff: f32) -> Self {
        self.base_height = base_height;
        self.height_falloff = falloff.max(0.001);
        self
    }

    /// Builder: set max fog
    pub fn with_max_fog(mut self, max: f32) -> Self {
        self.max_fog = max.clamp(0.0, 1.0);
        self
    }

    /// Set fog type
    pub fn set_fog_type(&mut self, fog_type: FogType) {
        self.fog_type = fog_type;
    }

    /// Get fog type
    pub fn fog_type(&self) -> FogType {
        self.fog_type
    }

    /// Set fog color
    pub fn set_color(&mut self, color: [f32; 4]) {
        self.color = color;
    }

    /// Get fog color
    pub fn color(&self) -> [f32; 4] {
        self.color
    }

    /// Set density
    pub fn set_density(&mut self, density: f32) {
        self.density = density.max(0.001);
    }

    /// Get density
    pub fn density(&self) -> f32 {
        self.density
    }

    /// Set linear range
    pub fn set_linear_range(&mut self, start: f32, end: f32) {
        self.start = start;
        self.end = end.max(start + 1.0);
    }

    /// Get start distance
    pub fn start(&self) -> f32 {
        self.start
    }

    /// Get end distance
    pub fn end(&self) -> f32 {
        self.end
    }

    /// Set max fog
    pub fn set_max_fog(&mut self, max: f32) {
        self.max_fog = max.clamp(0.0, 1.0);
    }

    /// Get max fog
    pub fn max_fog(&self) -> f32 {
        self.max_fog
    }

    /// Get current configuration
    pub fn config(&self) -> FogPassConfig {
        FogPassConfig {
            fog_type: self.fog_type,
            color: self.color,
            density: self.density,
            start: self.start,
            end: self.end,
            height_falloff: self.height_falloff,
            base_height: self.base_height,
            max_fog: self.max_fog,
            enabled: self.enabled,
        }
    }

    /// Calculate fog factor for a given distance
    /// Returns a value from 0.0 (no fog) to max_fog (maximum fog)
    pub fn calculate_fog_factor(&self, distance: f32, height: Option<f32>) -> f32 {
        let fog = match self.fog_type {
            FogType::Linear => {
                let d = distance.clamp(self.start, self.end);
                // visibility goes from 1 at start to 0 at end
                let visibility = (self.end - d) / (self.end - self.start);
                1.0 - visibility
            }
            FogType::Exponential => {
                // visibility = exp(-density * distance), fog = 1 - visibility
                1.0 - (-self.density * distance).exp()
            }
            FogType::ExponentialSquared => {
                let x = self.density * distance;
                1.0 - (-x * x).exp()
            }
            FogType::Height => {
                if let Some(h) = height {
                    // Distance-based fog amount
                    let dist_fog = 1.0 - (-self.density * distance).exp();
                    // Height factor: 1.0 at base_height, decreasing exponentially above
                    let height_scale = (-(h - self.base_height).max(0.0) * self.height_falloff).exp();
                    // Combine: fog is strongest at low heights
                    dist_fog * height_scale
                } else {
                    1.0 - (-self.density * distance).exp()
                }
            }
        };

        // Apply max_fog limit
        fog.clamp(0.0, self.max_fog)
    }
}

impl CustomRenderPass for FogPass {
    fn name(&self) -> &str {
        "fog"
    }

    fn dependencies(&self) -> &[&str] {
        &["main"]
    }

    fn reads(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor, ResourceRef::MainDepth]
    }

    fn writes(&self) -> Vec<ResourceRef> {
        vec![ResourceRef::MainColor]
    }

    fn setup(&mut self, _context: &PassSetupContext) -> Result<(), PassError> {
        // Fog is typically applied inline during main pass or as a simple fullscreen pass
        // No additional resources needed

        self.initialized = true;
        Ok(())
    }

    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
        if !self.enabled || !self.initialized {
            return Ok(());
        }

        let _main_color = context
            .resources
            .main_color()
            .ok_or_else(|| PassError::Resource("main_color not found".into()))?;

        let _main_depth = context
            .resources
            .main_depth()
            .ok_or_else(|| PassError::Resource("main_depth not found".into()))?;

        // In a real implementation:
        // 1. Sample depth to get distance
        // 2. Calculate fog factor based on fog type
        // 3. Blend main color with fog color

        Ok(())
    }

    fn cleanup(&mut self) {
        self.initialized = false;
    }

    fn resource_requirements(&self) -> ResourceRequirements {
        // Fog is very lightweight - just a fullscreen shader
        ResourceRequirements {
            memory_bytes: 0, // Uses existing color buffer
            render_targets: 0,
            compute: false,
            time_budget_ms: 0.1,
            ..Default::default()
        }
    }

    fn priority(&self) -> PassPriority {
        // Run after main rendering but before other post-process
        PassPriority(PassPriority::POST_PROCESS.0 - 10)
    }

    fn is_enabled(&self) -> bool {
        self.enabled
    }

    fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    fn get_config(&self) -> Option<PassConfigData> {
        Some(PassConfigData {
            name: "fog".to_string(),
            enabled: self.enabled,
            priority: self.priority().0,
            config: serde_json::to_value(self.config()).unwrap_or_default(),
        })
    }

    fn apply_config(&mut self, config: &PassConfigData) -> Result<(), PassError> {
        self.enabled = config.enabled;

        if let Ok(fog_config) = serde_json::from_value::<FogPassConfig>(config.config.clone()) {
            self.fog_type = fog_config.fog_type;
            self.color = fog_config.color;
            self.density = fog_config.density.max(0.001);
            self.start = fog_config.start;
            self.end = fog_config.end;
            self.height_falloff = fog_config.height_falloff.max(0.001);
            self.base_height = fog_config.base_height;
            self.max_fog = fog_config.max_fog.clamp(0.0, 1.0);
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fog_pass_creation() {
        let fog = FogPass::new(FogType::Exponential, [0.5, 0.6, 0.7, 1.0]);
        assert_eq!(fog.name(), "fog");
        assert_eq!(fog.fog_type(), FogType::Exponential);
        assert_eq!(fog.color(), [0.5, 0.6, 0.7, 1.0]);
    }

    #[test]
    fn test_fog_pass_builder() {
        let fog = FogPass::new(FogType::Linear, [1.0; 4])
            .with_linear_range(5.0, 50.0)
            .with_max_fog(0.8);

        assert_eq!(fog.start(), 5.0);
        assert_eq!(fog.end(), 50.0);
        assert_eq!(fog.max_fog(), 0.8);
    }

    #[test]
    fn test_fog_pass_config() {
        let config = FogPassConfig {
            fog_type: FogType::ExponentialSquared,
            color: [0.3, 0.4, 0.5, 1.0],
            density: 0.05,
            ..Default::default()
        };

        let fog = FogPass::with_config(config);
        assert_eq!(fog.fog_type(), FogType::ExponentialSquared);
        assert_eq!(fog.density(), 0.05);
    }

    #[test]
    fn test_fog_factor_linear() {
        let fog = FogPass::new(FogType::Linear, [1.0; 4])
            .with_linear_range(0.0, 100.0)
            .with_max_fog(1.0);

        // At start (0), fog = 0
        assert!((fog.calculate_fog_factor(0.0, None) - 0.0).abs() < 0.01);

        // At end (100), fog = 1
        assert!((fog.calculate_fog_factor(100.0, None) - 1.0).abs() < 0.01);

        // At midpoint (50), fog = 0.5
        assert!((fog.calculate_fog_factor(50.0, None) - 0.5).abs() < 0.01);
    }

    #[test]
    fn test_fog_factor_exponential() {
        let mut fog = FogPass::new(FogType::Exponential, [1.0; 4]);
        fog.set_density(0.1);
        fog.set_max_fog(1.0);

        // At distance 0, fog should be 0
        assert!((fog.calculate_fog_factor(0.0, None) - 0.0).abs() < 0.01);

        // Fog should increase with distance
        let f1 = fog.calculate_fog_factor(10.0, None);
        let f2 = fog.calculate_fog_factor(20.0, None);
        assert!(f2 > f1);
    }

    #[test]
    fn test_fog_factor_max_clamp() {
        let fog = FogPass::new(FogType::Exponential, [1.0; 4])
            .with_density(1.0) // Very dense
            .with_max_fog(0.5);

        // Even at large distance, fog should be capped
        let factor = fog.calculate_fog_factor(1000.0, None);
        assert!(factor <= 0.5);
    }

    #[test]
    fn test_fog_type_serialization() {
        let types = [
            FogType::Linear,
            FogType::Exponential,
            FogType::ExponentialSquared,
            FogType::Height,
        ];

        for t in types {
            let json = serde_json::to_string(&t).unwrap();
            let restored: FogType = serde_json::from_str(&json).unwrap();
            assert_eq!(t, restored);
        }
    }

    #[test]
    fn test_fog_pass_config_serialization() {
        let config = FogPassConfig::default();
        let json = serde_json::to_string(&config).unwrap();
        let restored: FogPassConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(config.fog_type, restored.fog_type);
        assert_eq!(config.density, restored.density);
    }

    #[test]
    fn test_fog_pass_dependencies() {
        let fog = FogPass::new(FogType::Exponential, [1.0; 4]);
        assert_eq!(fog.dependencies(), &["main"]);
    }

    #[test]
    fn test_fog_pass_resources() {
        let fog = FogPass::new(FogType::Exponential, [1.0; 4]);
        let reads = fog.reads();
        let writes = fog.writes();

        assert!(reads.contains(&ResourceRef::MainColor));
        assert!(reads.contains(&ResourceRef::MainDepth));
        assert!(writes.contains(&ResourceRef::MainColor));
    }

    #[test]
    fn test_fog_pass_lightweight() {
        let fog = FogPass::new(FogType::Exponential, [1.0; 4]);
        let reqs = fog.resource_requirements();

        assert_eq!(reqs.memory_bytes, 0);
        assert_eq!(reqs.render_targets, 0);
        assert!(!reqs.compute);
    }

    #[test]
    fn test_fog_pass_setup() {
        let mut fog = FogPass::new(FogType::Exponential, [1.0; 4]);
        let ctx = PassSetupContext::default();

        assert!(fog.setup(&ctx).is_ok());
        assert!(fog.initialized);
    }

    #[test]
    fn test_fog_height_type() {
        let fog = FogPass::new(FogType::Height, [1.0; 4])
            .with_height_fog(0.0, 0.1)
            .with_density(0.02);

        // Fog should be stronger at lower heights
        let low = fog.calculate_fog_factor(50.0, Some(0.0));
        let high = fog.calculate_fog_factor(50.0, Some(50.0));

        assert!(low > high);
    }
}
