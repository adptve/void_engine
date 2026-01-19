//! Shadow Configuration
//!
//! Global and per-light shadow settings with serde support for hot-reload.

use serde::{Serialize, Deserialize};

/// Global shadow configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShadowConfig {
    /// Enable shadows globally
    pub enabled: bool,

    /// Default shadow map resolution (power of 2)
    pub default_resolution: u32,

    /// Maximum shadow maps in atlas
    pub max_shadow_maps: u32,

    /// PCF filter size (1, 3, 5, 7)
    pub pcf_filter_size: u32,

    /// Enable soft shadows (PCSS - Percentage Closer Soft Shadows)
    pub soft_shadows: bool,

    /// Cascade count for directional lights (1-4)
    pub cascade_count: u32,

    /// Cascade split lambda (0 = linear, 1 = logarithmic)
    pub cascade_lambda: f32,

    /// Maximum shadow distance from camera
    pub shadow_distance: f32,

    /// Enable shadow caching for static objects
    pub enable_caching: bool,

    /// Shadow fade distance (starts fading at shadow_distance - fade_distance)
    pub fade_distance: f32,
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
            enable_caching: true,
            fade_distance: 10.0,
        }
    }
}

impl ShadowConfig {
    /// Create a high-quality shadow configuration
    pub fn high_quality() -> Self {
        Self {
            default_resolution: 4096,
            max_shadow_maps: 32,
            pcf_filter_size: 5,
            soft_shadows: true,
            cascade_count: 4,
            cascade_lambda: 0.75,
            ..Default::default()
        }
    }

    /// Create a low-quality shadow configuration for performance
    pub fn low_quality() -> Self {
        Self {
            default_resolution: 1024,
            max_shadow_maps: 8,
            pcf_filter_size: 1,
            soft_shadows: false,
            cascade_count: 2,
            cascade_lambda: 0.5,
            shadow_distance: 50.0,
            ..Default::default()
        }
    }

    /// Create a configuration with shadows disabled
    pub fn disabled() -> Self {
        Self {
            enabled: false,
            ..Default::default()
        }
    }

    /// Validate configuration and clamp values to valid ranges
    pub fn validate(&mut self) {
        self.default_resolution = self.default_resolution.clamp(256, 8192);
        self.default_resolution = self.default_resolution.next_power_of_two();
        self.max_shadow_maps = self.max_shadow_maps.clamp(1, 64);
        self.pcf_filter_size = match self.pcf_filter_size {
            0..=1 => 1,
            2..=3 => 3,
            4..=5 => 5,
            _ => 7,
        };
        self.cascade_count = self.cascade_count.clamp(1, 4);
        self.cascade_lambda = self.cascade_lambda.clamp(0.0, 1.0);
        self.shadow_distance = self.shadow_distance.max(1.0);
        self.fade_distance = self.fade_distance.clamp(0.0, self.shadow_distance * 0.5);
    }
}

/// Per-light shadow settings
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LightShadowSettings {
    /// Resolution override (None = use global default)
    pub resolution: Option<u32>,

    /// Depth bias to prevent shadow acne
    pub depth_bias: f32,

    /// Slope-scaled depth bias
    pub slope_bias: f32,

    /// Normal-based offset to prevent peter-panning
    pub normal_bias: f32,

    /// Near plane for shadow camera
    pub near_plane: f32,

    /// Far plane override for shadow camera (None = auto from light range)
    pub far_plane: Option<f32>,

    /// Soft shadow light size (for PCSS)
    pub light_size: f32,

    /// Cascade-specific settings (for directional lights)
    pub cascade_blend_distance: f32,

    /// Shadow map update mode
    pub update_mode: ShadowUpdateMode,

    /// Shadow strength (0 = no shadow, 1 = full shadow)
    pub strength: f32,
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
            cascade_blend_distance: 2.0,
            update_mode: ShadowUpdateMode::EveryFrame,
            strength: 1.0,
        }
    }
}

impl LightShadowSettings {
    /// Create settings for a directional light
    pub fn directional() -> Self {
        Self {
            depth_bias: 0.002,
            slope_bias: 1.5,
            normal_bias: 0.01,
            near_plane: 0.1,
            light_size: 0.5, // Sun angular diameter
            ..Default::default()
        }
    }

    /// Create settings for a spot light
    pub fn spot() -> Self {
        Self {
            depth_bias: 0.005,
            slope_bias: 2.0,
            normal_bias: 0.02,
            ..Default::default()
        }
    }

    /// Create settings for a point light (omnidirectional)
    pub fn point() -> Self {
        Self {
            depth_bias: 0.01,
            slope_bias: 3.0,
            normal_bias: 0.03,
            near_plane: 0.05,
            ..Default::default()
        }
    }

    /// Set shadow strength
    pub fn with_strength(mut self, strength: f32) -> Self {
        self.strength = strength.clamp(0.0, 1.0);
        self
    }

    /// Set resolution override
    pub fn with_resolution(mut self, resolution: u32) -> Self {
        self.resolution = Some(resolution);
        self
    }

    /// Set update mode
    pub fn with_update_mode(mut self, mode: ShadowUpdateMode) -> Self {
        self.update_mode = mode;
        self
    }

    /// Get effective resolution (override or default)
    pub fn effective_resolution(&self, default: u32) -> u32 {
        self.resolution.unwrap_or(default)
    }
}

/// Shadow map update mode
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum ShadowUpdateMode {
    /// Update every frame
    EveryFrame,
    /// Update every N frames
    Interval(u32),
    /// Only update when light or shadow casters move
    OnChange,
    /// Static shadow map (update once)
    Static,
}

impl Default for ShadowUpdateMode {
    fn default() -> Self {
        Self::EveryFrame
    }
}

impl ShadowUpdateMode {
    /// Check if shadow should update this frame
    pub fn should_update(&self, frame: u64, light_moved: bool, casters_moved: bool) -> bool {
        match self {
            Self::EveryFrame => true,
            Self::Interval(n) => frame % (*n as u64) == 0,
            Self::OnChange => light_moved || casters_moved,
            Self::Static => false,
        }
    }
}

/// Shadow quality preset
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum ShadowQuality {
    /// No shadows
    Off,
    /// Basic shadows with low resolution
    Low,
    /// Balanced quality and performance
    Medium,
    /// High quality shadows
    High,
    /// Maximum quality with soft shadows
    Ultra,
}

impl ShadowQuality {
    /// Convert to a ShadowConfig
    pub fn to_config(self) -> ShadowConfig {
        match self {
            Self::Off => ShadowConfig::disabled(),
            Self::Low => ShadowConfig::low_quality(),
            Self::Medium => ShadowConfig::default(),
            Self::High => ShadowConfig::high_quality(),
            Self::Ultra => ShadowConfig {
                default_resolution: 4096,
                max_shadow_maps: 32,
                pcf_filter_size: 7,
                soft_shadows: true,
                cascade_count: 4,
                cascade_lambda: 0.8,
                shadow_distance: 150.0,
                ..Default::default()
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shadow_config_default() {
        let config = ShadowConfig::default();
        assert!(config.enabled);
        assert_eq!(config.default_resolution, 2048);
        assert_eq!(config.cascade_count, 4);
    }

    #[test]
    fn test_shadow_config_validate() {
        let mut config = ShadowConfig {
            default_resolution: 1000, // Not power of 2
            max_shadow_maps: 100,     // Too high
            pcf_filter_size: 6,       // Invalid
            cascade_count: 10,        // Too high
            cascade_lambda: 2.0,      // Out of range
            ..Default::default()
        };

        config.validate();

        assert_eq!(config.default_resolution, 1024); // Next power of 2
        assert_eq!(config.max_shadow_maps, 64);
        assert_eq!(config.pcf_filter_size, 7);
        assert_eq!(config.cascade_count, 4);
        assert_eq!(config.cascade_lambda, 1.0);
    }

    #[test]
    fn test_shadow_config_serialization() {
        let config = ShadowConfig::high_quality();
        let json = serde_json::to_string(&config).unwrap();
        let restored: ShadowConfig = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.default_resolution, 4096);
        assert!(restored.soft_shadows);
    }

    #[test]
    fn test_light_shadow_settings() {
        let settings = LightShadowSettings::directional()
            .with_strength(0.8)
            .with_resolution(4096);

        assert_eq!(settings.strength, 0.8);
        assert_eq!(settings.resolution, Some(4096));
        assert_eq!(settings.effective_resolution(2048), 4096);
    }

    #[test]
    fn test_shadow_update_mode() {
        assert!(ShadowUpdateMode::EveryFrame.should_update(0, false, false));
        assert!(ShadowUpdateMode::Interval(3).should_update(3, false, false));
        assert!(!ShadowUpdateMode::Interval(3).should_update(4, false, false));
        assert!(ShadowUpdateMode::OnChange.should_update(0, true, false));
        assert!(!ShadowUpdateMode::OnChange.should_update(0, false, false));
        assert!(!ShadowUpdateMode::Static.should_update(0, true, true));
    }

    #[test]
    fn test_shadow_quality_presets() {
        let off = ShadowQuality::Off.to_config();
        assert!(!off.enabled);

        let ultra = ShadowQuality::Ultra.to_config();
        assert!(ultra.soft_shadows);
        assert_eq!(ultra.pcf_filter_size, 7);
    }
}
