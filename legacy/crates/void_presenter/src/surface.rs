//! Surface abstraction
//!
//! Represents the renderable target (window, canvas, XR session).

use crate::{SurfaceFormat, PresentMode};
use serde::{Serialize, Deserialize};

/// Surface error types
#[derive(Debug, thiserror::Error)]
pub enum SurfaceError {
    #[error("Surface creation failed: {0}")]
    CreationFailed(String),

    #[error("Surface lost")]
    Lost,

    #[error("Surface outdated")]
    Outdated,

    #[error("Surface timeout")]
    Timeout,
}

/// Surface configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SurfaceConfig {
    /// Width in pixels
    pub width: u32,
    /// Height in pixels
    pub height: u32,
    /// Surface format
    pub format: SurfaceFormat,
    /// Present mode
    pub present_mode: PresentMode,
    /// Alpha mode
    pub alpha_mode: AlphaMode,
    /// Desired frame latency
    pub desired_maximum_frame_latency: u32,
}

impl Default for SurfaceConfig {
    fn default() -> Self {
        Self {
            width: 800,
            height: 600,
            format: SurfaceFormat::Bgra8UnormSrgb,
            present_mode: PresentMode::Fifo,
            alpha_mode: AlphaMode::Opaque,
            desired_maximum_frame_latency: 2,
        }
    }
}

impl SurfaceConfig {
    /// Create config with specific size
    pub fn with_size(mut self, width: u32, height: u32) -> Self {
        self.width = width;
        self.height = height;
        self
    }

    /// Set format
    pub fn with_format(mut self, format: SurfaceFormat) -> Self {
        self.format = format;
        self
    }

    /// Set present mode
    pub fn with_present_mode(mut self, mode: PresentMode) -> Self {
        self.present_mode = mode;
        self
    }

    /// Get aspect ratio
    pub fn aspect_ratio(&self) -> f32 {
        self.width as f32 / self.height.max(1) as f32
    }
}

/// Alpha blending mode
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum AlphaMode {
    /// Fully opaque
    Opaque,
    /// Premultiplied alpha
    PreMultiplied,
    /// Post-multiplied alpha
    PostMultiplied,
    /// Inherent transparency
    Inherit,
}

/// Surface capabilities
#[derive(Debug, Clone)]
pub struct SurfaceCapabilities {
    /// Supported formats
    pub formats: Vec<SurfaceFormat>,
    /// Supported present modes
    pub present_modes: Vec<PresentMode>,
    /// Supported alpha modes
    pub alpha_modes: Vec<AlphaMode>,
    /// Minimum texture extent
    pub min_extent: (u32, u32),
    /// Maximum texture extent
    pub max_extent: (u32, u32),
}

impl Default for SurfaceCapabilities {
    fn default() -> Self {
        Self {
            formats: vec![SurfaceFormat::Bgra8UnormSrgb],
            present_modes: vec![PresentMode::Fifo],
            alpha_modes: vec![AlphaMode::Opaque],
            min_extent: (1, 1),
            max_extent: (16384, 16384),
        }
    }
}

impl SurfaceCapabilities {
    /// Check if format is supported
    pub fn supports_format(&self, format: SurfaceFormat) -> bool {
        self.formats.contains(&format)
    }

    /// Check if present mode is supported
    pub fn supports_present_mode(&self, mode: PresentMode) -> bool {
        self.present_modes.contains(&mode)
    }

    /// Get best format (prefer sRGB)
    pub fn preferred_format(&self) -> SurfaceFormat {
        self.formats
            .iter()
            .find(|f| f.is_srgb())
            .copied()
            .or_else(|| self.formats.first().copied())
            .unwrap_or(SurfaceFormat::Bgra8UnormSrgb)
    }

    /// Get best present mode for low latency
    pub fn preferred_present_mode_low_latency(&self) -> PresentMode {
        if self.present_modes.contains(&PresentMode::Mailbox) {
            PresentMode::Mailbox
        } else if self.present_modes.contains(&PresentMode::Immediate) {
            PresentMode::Immediate
        } else {
            PresentMode::Fifo
        }
    }

    /// Get best present mode for no tearing
    pub fn preferred_present_mode_vsync(&self) -> PresentMode {
        if self.present_modes.contains(&PresentMode::Fifo) {
            PresentMode::Fifo
        } else {
            self.present_modes.first().copied().unwrap_or(PresentMode::Fifo)
        }
    }

    /// Clamp extent to supported range
    pub fn clamp_extent(&self, width: u32, height: u32) -> (u32, u32) {
        (
            width.clamp(self.min_extent.0, self.max_extent.0),
            height.clamp(self.min_extent.1, self.max_extent.1),
        )
    }
}

/// Surface state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SurfaceState {
    /// Surface is healthy and ready
    Ready,
    /// Surface needs reconfiguration (resize, format change)
    NeedsReconfigure,
    /// Surface was lost and needs recreation
    Lost,
    /// Surface is minimized (zero size)
    Minimized,
}

/// Abstract surface trait
pub trait Surface: Send + Sync {
    /// Get current configuration
    fn config(&self) -> &SurfaceConfig;

    /// Get capabilities
    fn capabilities(&self) -> &SurfaceCapabilities;

    /// Get current state
    fn state(&self) -> SurfaceState;

    /// Reconfigure the surface
    fn configure(&mut self, config: SurfaceConfig) -> Result<(), SurfaceError>;

    /// Get current texture for rendering
    fn get_current_texture(&mut self) -> Result<SurfaceTexture, SurfaceError>;

    /// Present the current texture
    fn present(&mut self);
}

/// Surface texture handle
#[derive(Debug)]
pub struct SurfaceTexture {
    /// Texture identifier
    pub id: u64,
    /// Texture size
    pub size: (u32, u32),
    /// Texture format
    pub format: SurfaceFormat,
    /// Whether texture is suboptimal (should reconfigure soon)
    pub suboptimal: bool,
}

impl SurfaceTexture {
    /// Create a new surface texture
    pub fn new(id: u64, size: (u32, u32), format: SurfaceFormat) -> Self {
        Self {
            id,
            size,
            format,
            suboptimal: false,
        }
    }

    /// Mark as suboptimal
    pub fn with_suboptimal(mut self, suboptimal: bool) -> Self {
        self.suboptimal = suboptimal;
        self
    }
}

/// Null surface for testing
pub struct NullSurface {
    config: SurfaceConfig,
    capabilities: SurfaceCapabilities,
    state: SurfaceState,
    texture_id: u64,
}

impl NullSurface {
    pub fn new() -> Self {
        Self {
            config: SurfaceConfig::default(),
            capabilities: SurfaceCapabilities::default(),
            state: SurfaceState::Ready,
            texture_id: 0,
        }
    }

    pub fn with_config(mut self, config: SurfaceConfig) -> Self {
        self.config = config;
        self
    }
}

impl Default for NullSurface {
    fn default() -> Self {
        Self::new()
    }
}

impl Surface for NullSurface {
    fn config(&self) -> &SurfaceConfig {
        &self.config
    }

    fn capabilities(&self) -> &SurfaceCapabilities {
        &self.capabilities
    }

    fn state(&self) -> SurfaceState {
        self.state
    }

    fn configure(&mut self, config: SurfaceConfig) -> Result<(), SurfaceError> {
        self.config = config;
        self.state = SurfaceState::Ready;
        Ok(())
    }

    fn get_current_texture(&mut self) -> Result<SurfaceTexture, SurfaceError> {
        self.texture_id += 1;
        Ok(SurfaceTexture::new(
            self.texture_id,
            (self.config.width, self.config.height),
            self.config.format,
        ))
    }

    fn present(&mut self) {
        // No-op for null surface
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_surface_config() {
        let config = SurfaceConfig::default()
            .with_size(1920, 1080)
            .with_format(SurfaceFormat::Rgba8UnormSrgb);

        assert_eq!(config.width, 1920);
        assert_eq!(config.height, 1080);
        assert!(config.format.is_srgb());
    }

    #[test]
    fn test_surface_capabilities() {
        let caps = SurfaceCapabilities {
            formats: vec![SurfaceFormat::Bgra8UnormSrgb, SurfaceFormat::Rgba8Unorm],
            present_modes: vec![PresentMode::Fifo, PresentMode::Mailbox],
            ..Default::default()
        };

        assert!(caps.supports_format(SurfaceFormat::Bgra8UnormSrgb));
        assert!(!caps.supports_format(SurfaceFormat::Rgba16Float));

        assert_eq!(caps.preferred_format(), SurfaceFormat::Bgra8UnormSrgb);
        assert_eq!(caps.preferred_present_mode_low_latency(), PresentMode::Mailbox);
    }

    #[test]
    fn test_null_surface() {
        let mut surface = NullSurface::new();

        assert_eq!(surface.state(), SurfaceState::Ready);

        let config = SurfaceConfig::default().with_size(800, 600);
        surface.configure(config).unwrap();

        let texture = surface.get_current_texture().unwrap();
        assert_eq!(texture.size, (800, 600));

        surface.present();
    }
}
