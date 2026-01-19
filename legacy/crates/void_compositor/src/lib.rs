//! # Void Compositor
//!
//! Wayland compositor for Metaverse OS using Smithay.
//!
//! This crate provides the display server functionality, handling:
//! - DRM/KMS display management
//! - Input device handling (keyboard, mouse, touch)
//! - Frame timing and presentation
//! - Wayland protocol for client applications
//!
//! ## Architecture
//!
//! ```text
//! ┌─────────────────────────────────────────────────────────────┐
//! │  Metaverse Kernel (frame scheduler, hot-swap, app isolation)│
//! ├─────────────────────────────────────────────────────────────┤
//! │  void_compositor (this crate)                               │
//! │  ├─ Smithay backend (DRM, libinput)                         │
//! │  ├─ Frame scheduler integration                             │
//! │  └─ Input routing                                           │
//! ├─────────────────────────────────────────────────────────────┤
//! │  Linux Kernel (DRM/KMS, evdev)                              │
//! └─────────────────────────────────────────────────────────────┘
//! ```
//!
//! ## Platform Support
//!
//! - **Linux**: Full Smithay compositor with DRM, libinput
//! - **Other**: Stub implementation (for development/testing)

pub mod error;
pub mod frame;
pub mod input;
pub mod backend;
pub mod vrr;
pub mod hdr;

// Full Smithay compositor (Linux + smithay-compositor feature)
#[cfg(all(target_os = "linux", feature = "smithay-compositor"))]
pub mod compositor;

// Stub compositor for non-Linux or when smithay-compositor feature is disabled
// This allows musl builds without libudev dependency
#[cfg(not(all(target_os = "linux", feature = "smithay-compositor")))]
pub mod stub;

pub use error::CompositorError;
pub use frame::{FrameScheduler, FrameState, PresentationFeedback};
pub use input::{InputEvent, KeyboardEvent, PointerEvent};
pub use vrr::{VrrConfig, VrrMode, VrrCapability};
pub use hdr::{HdrConfig, HdrCapability, TransferFunction, ColorPrimaries, HdrMetadata};

#[cfg(all(target_os = "linux", feature = "smithay-compositor"))]
pub use compositor::Compositor;

#[cfg(not(all(target_os = "linux", feature = "smithay-compositor")))]
pub use stub::Compositor;

/// Compositor configuration
#[derive(Debug, Clone)]
pub struct CompositorConfig {
    /// Target refresh rate (0 = use display default)
    pub target_fps: u32,
    /// Enable VSync
    pub vsync: bool,
    /// Allow tearing for lower latency
    pub allow_tearing: bool,
    /// Enable XWayland for X11 app support
    pub xwayland: bool,
}

impl Default for CompositorConfig {
    fn default() -> Self {
        Self {
            target_fps: 0,  // Use display default
            vsync: true,
            allow_tearing: false,
            xwayland: false,
        }
    }
}

/// Compositor capabilities (queried at runtime)
#[derive(Debug, Clone)]
pub struct CompositorCapabilities {
    /// Available refresh rates
    pub refresh_rates: Vec<u32>,
    /// Maximum resolution
    pub max_resolution: (u32, u32),
    /// Current resolution
    pub current_resolution: (u32, u32),
    /// VRR (Variable Refresh Rate) support
    pub vrr_supported: bool,
    /// HDR support
    pub hdr_supported: bool,
    /// Number of connected displays
    pub display_count: usize,
}

/// Trait for render targets provided by the compositor
pub trait RenderTarget {
    /// Get the size of the render target
    fn size(&self) -> (u32, u32);

    /// Get the format (for wgpu compatibility)
    fn format(&self) -> RenderFormat;

    /// Signal that rendering is complete
    fn present(&mut self) -> Result<(), CompositorError>;
}

/// Render format (compatible with wgpu::TextureFormat)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RenderFormat {
    Bgra8Unorm,
    Bgra8UnormSrgb,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Rgb10a2Unorm,
}

impl RenderFormat {
    /// Convert to wgpu TextureFormat name (for interop)
    pub fn as_wgpu_format_name(&self) -> &'static str {
        match self {
            Self::Bgra8Unorm => "Bgra8Unorm",
            Self::Bgra8UnormSrgb => "Bgra8UnormSrgb",
            Self::Rgba8Unorm => "Rgba8Unorm",
            Self::Rgba8UnormSrgb => "Rgba8UnormSrgb",
            Self::Rgb10a2Unorm => "Rgb10a2Unorm",
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_config() {
        let config = CompositorConfig::default();
        assert!(config.vsync);
        assert!(!config.allow_tearing);
    }
}
