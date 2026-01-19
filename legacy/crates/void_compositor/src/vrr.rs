//! Variable Refresh Rate (VRR) support
//!
//! This module handles VRR (VSync-off, FreeSync, G-Sync) detection and configuration.
//! VRR allows dynamic refresh rate adjustment for lower latency and smoother experience.

use std::time::Duration;

/// VRR configuration
#[derive(Debug, Clone)]
pub struct VrrConfig {
    /// Is VRR enabled?
    pub enabled: bool,
    /// Minimum refresh rate (Hz)
    pub min_refresh_rate: u32,
    /// Maximum refresh rate (Hz)
    pub max_refresh_rate: u32,
    /// Current dynamic refresh rate (Hz)
    pub current_refresh_rate: u32,
    /// VRR mode
    pub mode: VrrMode,
}

/// VRR mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VrrMode {
    /// VRR disabled (fixed refresh rate)
    Disabled,
    /// Automatic VRR (adapt based on content)
    Auto,
    /// Always run at maximum refresh rate
    MaximumPerformance,
    /// Prefer lower refresh rates for power saving
    PowerSaving,
}

impl Default for VrrConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            min_refresh_rate: 48,
            max_refresh_rate: 144,
            current_refresh_rate: 60,
            mode: VrrMode::Disabled,
        }
    }
}

impl VrrConfig {
    /// Create a new VRR configuration
    pub fn new(min_refresh: u32, max_refresh: u32) -> Self {
        Self {
            enabled: false,
            min_refresh_rate: min_refresh,
            max_refresh_rate: max_refresh,
            current_refresh_rate: max_refresh,
            mode: VrrMode::Disabled,
        }
    }

    /// Enable VRR with the given mode
    pub fn enable(&mut self, mode: VrrMode) {
        self.enabled = true;
        self.mode = mode;
        self.current_refresh_rate = match mode {
            VrrMode::Disabled => self.max_refresh_rate,
            VrrMode::Auto => self.max_refresh_rate,
            VrrMode::MaximumPerformance => self.max_refresh_rate,
            VrrMode::PowerSaving => self.min_refresh_rate,
        };
    }

    /// Disable VRR
    pub fn disable(&mut self) {
        self.enabled = false;
        self.mode = VrrMode::Disabled;
    }

    /// Check if VRR is active
    pub fn is_active(&self) -> bool {
        self.enabled && self.mode != VrrMode::Disabled
    }

    /// Get the current frame time target
    pub fn frame_time(&self) -> Duration {
        if self.current_refresh_rate > 0 {
            Duration::from_secs_f64(1.0 / self.current_refresh_rate as f64)
        } else {
            Duration::from_millis(16) // Fallback to 60Hz
        }
    }

    /// Adapt refresh rate based on content
    ///
    /// This is called by the frame scheduler to dynamically adjust the refresh rate.
    /// The algorithm tries to:
    /// - Use maximum refresh for fast-moving content
    /// - Drop to lower refresh for static content (power saving)
    /// - Avoid frequent changes (hysteresis)
    pub fn adapt_refresh_rate(&mut self, frame_time: Duration, content_velocity: f32) {
        if !self.is_active() || self.mode != VrrMode::Auto {
            return;
        }

        let target_refresh = if content_velocity > 0.5 {
            // Fast-moving content: use max refresh
            self.max_refresh_rate
        } else if content_velocity < 0.1 {
            // Static content: use min refresh
            self.min_refresh_rate
        } else {
            // Medium content: interpolate
            let t = (content_velocity - 0.1) / 0.4; // Normalize to 0-1
            self.min_refresh_rate + ((self.max_refresh_rate - self.min_refresh_rate) as f32 * t) as u32
        };

        // Apply hysteresis: only change if difference is significant
        if (target_refresh as i32 - self.current_refresh_rate as i32).abs() > 5 {
            self.current_refresh_rate = target_refresh.clamp(self.min_refresh_rate, self.max_refresh_rate);
        }
    }

    /// Check if a refresh rate is within VRR range
    pub fn supports_refresh_rate(&self, rate: u32) -> bool {
        rate >= self.min_refresh_rate && rate <= self.max_refresh_rate
    }

    /// Get the VRR range as a string (for display)
    pub fn range_string(&self) -> String {
        format!("{}-{}Hz", self.min_refresh_rate, self.max_refresh_rate)
    }
}

/// VRR capability detection result
#[derive(Debug, Clone)]
pub struct VrrCapability {
    /// Is VRR supported by the display?
    pub supported: bool,
    /// Minimum refresh rate (if supported)
    pub min_refresh_rate: Option<u32>,
    /// Maximum refresh rate (if supported)
    pub max_refresh_rate: Option<u32>,
    /// VRR technology name (FreeSync, G-Sync, VESA AdaptiveSync, etc.)
    pub technology: Option<String>,
}

impl Default for VrrCapability {
    fn default() -> Self {
        Self {
            supported: false,
            min_refresh_rate: None,
            max_refresh_rate: None,
            technology: None,
        }
    }
}

impl VrrCapability {
    /// Create a VRR capability for a non-VRR display
    pub fn not_supported() -> Self {
        Self::default()
    }

    /// Create a VRR capability for a VRR-capable display
    pub fn supported(min_refresh: u32, max_refresh: u32, technology: Option<String>) -> Self {
        Self {
            supported: true,
            min_refresh_rate: Some(min_refresh),
            max_refresh_rate: Some(max_refresh),
            technology,
        }
    }

    /// Convert to VrrConfig (if supported)
    pub fn to_config(&self) -> Option<VrrConfig> {
        if self.supported {
            Some(VrrConfig::new(
                self.min_refresh_rate.unwrap_or(48),
                self.max_refresh_rate.unwrap_or(144),
            ))
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vrr_config_creation() {
        let config = VrrConfig::new(48, 144);
        assert!(!config.enabled);
        assert_eq!(config.min_refresh_rate, 48);
        assert_eq!(config.max_refresh_rate, 144);
    }

    #[test]
    fn test_vrr_enable_disable() {
        let mut config = VrrConfig::new(48, 144);
        assert!(!config.is_active());

        config.enable(VrrMode::Auto);
        assert!(config.is_active());
        assert_eq!(config.mode, VrrMode::Auto);

        config.disable();
        assert!(!config.is_active());
    }

    #[test]
    fn test_vrr_frame_time() {
        let mut config = VrrConfig::new(48, 144);
        config.enable(VrrMode::MaximumPerformance);

        let frame_time = config.frame_time();
        assert!((frame_time.as_secs_f64() - 1.0 / 144.0).abs() < 0.0001);
    }

    #[test]
    fn test_vrr_adapt_refresh_rate() {
        let mut config = VrrConfig::new(48, 144);
        config.enable(VrrMode::Auto);

        // Fast content -> max refresh
        config.adapt_refresh_rate(Duration::from_millis(16), 0.8);
        assert_eq!(config.current_refresh_rate, 144);

        // Static content -> min refresh
        config.adapt_refresh_rate(Duration::from_millis(16), 0.05);
        assert_eq!(config.current_refresh_rate, 48);
    }

    #[test]
    fn test_vrr_capability() {
        let cap = VrrCapability::supported(48, 144, Some("FreeSync".to_string()));
        assert!(cap.supported);
        assert_eq!(cap.min_refresh_rate, Some(48));
        assert_eq!(cap.max_refresh_rate, Some(144));

        let config = cap.to_config().unwrap();
        assert_eq!(config.min_refresh_rate, 48);
        assert_eq!(config.max_refresh_rate, 144);
    }
}
