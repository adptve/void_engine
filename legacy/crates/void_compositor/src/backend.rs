//! Backend abstraction for hot-swappable rendering backends
//!
//! This module provides the abstraction layer that enables runtime
//! backend switching - a key feature of Metaverse OS.

use crate::CompositorError;

/// Backend type identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum BackendType {
    /// DRM/KMS backend (direct hardware access)
    Drm,
    /// Winit backend (for development/testing in a window)
    Winit,
    /// Headless backend (no display, for testing)
    Headless,
    /// X11 backend (running under X11)
    X11,
    /// Wayland backend (running under another Wayland compositor)
    Wayland,
}

impl BackendType {
    /// Get human-readable name
    pub fn name(&self) -> &'static str {
        match self {
            Self::Drm => "DRM/KMS",
            Self::Winit => "Winit",
            Self::Headless => "Headless",
            Self::X11 => "X11",
            Self::Wayland => "Wayland",
        }
    }

    /// Check if this backend requires a display server
    pub fn requires_display_server(&self) -> bool {
        matches!(self, Self::Winit | Self::X11 | Self::Wayland)
    }

    /// Check if this backend has direct hardware access
    pub fn is_direct(&self) -> bool {
        matches!(self, Self::Drm)
    }
}

/// Backend capabilities
#[derive(Debug, Clone)]
pub struct BackendCapabilities {
    /// Backend type
    pub backend_type: BackendType,
    /// Supports VRR (Variable Refresh Rate)
    pub vrr: bool,
    /// Supports HDR
    pub hdr: bool,
    /// Maximum refresh rate
    pub max_refresh_rate: u32,
    /// Supports multiple outputs
    pub multi_output: bool,
    /// Supports hardware cursors
    pub hardware_cursor: bool,
    /// Supports direct scanout
    pub direct_scanout: bool,
}

impl Default for BackendCapabilities {
    fn default() -> Self {
        Self {
            backend_type: BackendType::Headless,
            vrr: false,
            hdr: false,
            max_refresh_rate: 60,
            multi_output: false,
            hardware_cursor: false,
            direct_scanout: false,
        }
    }
}

/// Output (display) information
#[derive(Debug, Clone)]
pub struct OutputInfo {
    /// Output identifier
    pub id: u64,
    /// Human-readable name
    pub name: String,
    /// Physical size in mm (if known)
    pub physical_size: Option<(u32, u32)>,
    /// Current mode
    pub current_mode: OutputMode,
    /// Available modes
    pub available_modes: Vec<OutputMode>,
    /// Is this the primary output?
    pub primary: bool,
    /// Output position in global coordinate space
    pub position: (i32, i32),
    /// Scale factor
    pub scale: f64,
    /// Transform (rotation/flip)
    pub transform: OutputTransform,
}

/// Output display mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputMode {
    /// Width in pixels
    pub width: u32,
    /// Height in pixels
    pub height: u32,
    /// Refresh rate in millihertz (e.g., 60000 for 60Hz)
    pub refresh_mhz: u32,
}

impl OutputMode {
    /// Get refresh rate in Hz
    pub fn refresh_hz(&self) -> f64 {
        self.refresh_mhz as f64 / 1000.0
    }
}

/// Output transform (rotation/flip)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum OutputTransform {
    #[default]
    Normal,
    Rotate90,
    Rotate180,
    Rotate270,
    Flipped,
    Flipped90,
    Flipped180,
    Flipped270,
}

/// Backend selection strategy
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BackendSelector {
    /// Automatically select best available backend
    Auto,
    /// Prefer specific backend, fall back to auto
    Prefer(BackendType),
    /// Require specific backend (fail if unavailable)
    Require(BackendType),
}

impl Default for BackendSelector {
    fn default() -> Self {
        Self::Auto
    }
}

/// Detect available backends on the current system
pub fn detect_available_backends() -> Vec<BackendType> {
    let mut backends = vec![BackendType::Headless]; // Always available

    #[cfg(target_os = "linux")]
    {
        // Check for DRM access
        if std::path::Path::new("/dev/dri").exists() {
            backends.push(BackendType::Drm);
        }

        // Check for Wayland
        if std::env::var("WAYLAND_DISPLAY").is_ok() {
            backends.push(BackendType::Wayland);
        }

        // Check for X11
        if std::env::var("DISPLAY").is_ok() {
            backends.push(BackendType::X11);
        }
    }

    // Winit is available on desktop platforms
    #[cfg(any(target_os = "linux", target_os = "windows", target_os = "macos"))]
    {
        // Winit requires either X11, Wayland, or native windowing
        #[cfg(target_os = "linux")]
        if std::env::var("WAYLAND_DISPLAY").is_ok() || std::env::var("DISPLAY").is_ok() {
            backends.push(BackendType::Winit);
        }

        #[cfg(not(target_os = "linux"))]
        backends.push(BackendType::Winit);
    }

    backends
}

/// Select the best backend based on strategy and availability
pub fn select_backend(
    selector: BackendSelector,
    available: &[BackendType],
) -> Result<BackendType, CompositorError> {
    match selector {
        BackendSelector::Auto => {
            // Priority: DRM > Wayland > X11 > Winit > Headless
            let priority = [
                BackendType::Drm,
                BackendType::Wayland,
                BackendType::X11,
                BackendType::Winit,
                BackendType::Headless,
            ];

            for backend in priority {
                if available.contains(&backend) {
                    return Ok(backend);
                }
            }

            Err(CompositorError::BackendUnavailable(
                "No suitable backend found".into(),
            ))
        }

        BackendSelector::Prefer(preferred) => {
            if available.contains(&preferred) {
                Ok(preferred)
            } else {
                select_backend(BackendSelector::Auto, available)
            }
        }

        BackendSelector::Require(required) => {
            if available.contains(&required) {
                Ok(required)
            } else {
                Err(CompositorError::BackendUnavailable(format!(
                    "Required backend {} not available",
                    required.name()
                )))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_backend_selection_auto() {
        let available = vec![BackendType::Headless, BackendType::Winit];
        let selected = select_backend(BackendSelector::Auto, &available).unwrap();
        assert_eq!(selected, BackendType::Winit); // Winit preferred over Headless
    }

    #[test]
    fn test_backend_selection_prefer() {
        let available = vec![BackendType::Headless, BackendType::Winit];

        // Prefer DRM but it's not available, should fall back
        let selected = select_backend(BackendSelector::Prefer(BackendType::Drm), &available).unwrap();
        assert_eq!(selected, BackendType::Winit);

        // Prefer Winit and it's available
        let selected = select_backend(BackendSelector::Prefer(BackendType::Winit), &available).unwrap();
        assert_eq!(selected, BackendType::Winit);
    }

    #[test]
    fn test_backend_selection_require() {
        let available = vec![BackendType::Headless];

        // Require Headless - should succeed
        let result = select_backend(BackendSelector::Require(BackendType::Headless), &available);
        assert!(result.is_ok());

        // Require DRM - should fail
        let result = select_backend(BackendSelector::Require(BackendType::Drm), &available);
        assert!(result.is_err());
    }

    #[test]
    fn test_output_mode_refresh() {
        let mode = OutputMode {
            width: 1920,
            height: 1080,
            refresh_mhz: 60000,
        };
        assert!((mode.refresh_hz() - 60.0).abs() < 0.001);
    }
}
