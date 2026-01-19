//! # Void Presenter
//!
//! Presenter abstraction for Void Engine providing:
//! - Multi-platform output adapters (Desktop, WebGPU, WebXR)
//! - Swapchain management
//! - Frame timing and presentation
//! - State rehydration for hot-swap
//!
//! ## Architecture
//!
//! ```text
//! Kernel Frame ──► Presenter ──► GPU/Display
//!                      │
//!                      ▼
//!               Platform Backend
//!               (Desktop/Web/XR)
//! ```
//!
//! ## Key Concepts
//!
//! - **Presenter**: Abstract interface for presenting frames
//! - **Surface**: The renderable target (window, canvas, XR session)
//! - **Frame**: A single presentable frame with timing info
//! - **Rehydration**: State restoration without restart

pub mod surface;
pub mod frame;
pub mod timing;
pub mod rehydration;

// Desktop presenter (winit/wgpu)
#[cfg(feature = "desktop")]
pub mod desktop;

// DRM backend (Linux bare metal)
#[cfg(all(feature = "drm-backend", target_os = "linux"))]
pub mod drm;

// Web/WASM presenter using WebGPU
#[cfg(all(target_arch = "wasm32", feature = "web"))]
pub mod web;

// XR presenter (bridges void_xr with void_render)
#[cfg(feature = "xr")]
pub mod xr;

pub use surface::{Surface, SurfaceConfig, SurfaceCapabilities, SurfaceError, AlphaMode};
pub use frame::{Frame, FrameState, FrameOutput};
pub use timing::{FrameTiming, VSync, PresentMode};
pub use rehydration::{RehydrationState, Rehydratable};

// Re-export desktop presenter
#[cfg(feature = "desktop")]
pub use desktop::DesktopPresenter;

// Re-export web presenter types when building for WASM
#[cfg(all(target_arch = "wasm32", feature = "web"))]
pub use web::{
    WebPresenter, WebPresenterConfig, WebFrame, WebError,
    WebFrameScheduler, WebInputHandler, WebInputEvent,
    PowerPreference, XrMode, PixelRatioMode,
    TouchPoint, GamepadState,
};

// Re-export XR presenter types
#[cfg(feature = "xr")]
pub use xr::{
    XrPresenter, XrPresenterConfig, XrPresenterError,
    XrFrame, XrView, Eye, Fov, Pose, EyeRenderTarget, StereoView,
    convert_view, convert_pose,
};

use parking_lot::RwLock;
use thiserror::Error;
use serde::{Serialize, Deserialize};

/// Presenter errors
#[derive(Debug, Error)]
pub enum PresenterError {
    #[error("Surface creation failed: {0}")]
    SurfaceCreation(String),

    #[error("Surface lost")]
    SurfaceLost,

    #[error("Frame acquisition failed: {0}")]
    FrameAcquisition(String),

    #[error("Presentation failed: {0}")]
    PresentationFailed(String),

    #[error("Backend not available: {0}")]
    BackendNotAvailable(String),

    #[error("Configuration error: {0}")]
    ConfigError(String),

    #[error("Rehydration failed: {0}")]
    RehydrationFailed(String),
}

/// Unique presenter identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct PresenterId(u64);

impl PresenterId {
    pub fn new(id: u64) -> Self {
        Self(id)
    }

    pub fn raw(&self) -> u64 {
        self.0
    }
}

/// Presenter capabilities
#[derive(Debug, Clone)]
pub struct PresenterCapabilities {
    /// Supported present modes
    pub present_modes: Vec<PresentMode>,
    /// Supported formats
    pub formats: Vec<SurfaceFormat>,
    /// Maximum resolution
    pub max_resolution: (u32, u32),
    /// Supports HDR
    pub hdr_support: bool,
    /// Supports variable refresh rate
    pub vrr_support: bool,
    /// Supports XR passthrough
    pub xr_passthrough: bool,
}

impl Default for PresenterCapabilities {
    fn default() -> Self {
        Self {
            present_modes: vec![PresentMode::Fifo],
            formats: vec![SurfaceFormat::Bgra8UnormSrgb],
            max_resolution: (4096, 4096),
            hdr_support: false,
            vrr_support: false,
            xr_passthrough: false,
        }
    }
}

/// Surface format
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum SurfaceFormat {
    Bgra8Unorm,
    Bgra8UnormSrgb,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Rgba16Float,
    Rgb10a2Unorm,
}

impl SurfaceFormat {
    /// Get bytes per pixel
    pub fn bytes_per_pixel(&self) -> u32 {
        match self {
            Self::Bgra8Unorm | Self::Bgra8UnormSrgb => 4,
            Self::Rgba8Unorm | Self::Rgba8UnormSrgb => 4,
            Self::Rgba16Float => 8,
            Self::Rgb10a2Unorm => 4,
        }
    }

    /// Check if sRGB
    pub fn is_srgb(&self) -> bool {
        matches!(self, Self::Bgra8UnormSrgb | Self::Rgba8UnormSrgb)
    }
}

/// Presenter configuration
#[derive(Debug, Clone)]
pub struct PresenterConfig {
    /// Surface format
    pub format: SurfaceFormat,
    /// Present mode (vsync behavior)
    pub present_mode: PresentMode,
    /// Initial size
    pub size: (u32, u32),
    /// Enable HDR if available
    pub enable_hdr: bool,
    /// Target frame rate (0 = unlimited)
    pub target_frame_rate: u32,
    /// Allow tearing
    pub allow_tearing: bool,
}

impl Default for PresenterConfig {
    fn default() -> Self {
        Self {
            format: SurfaceFormat::Bgra8UnormSrgb,
            present_mode: PresentMode::Fifo,
            size: (1920, 1080),
            enable_hdr: false,
            target_frame_rate: 60,
            allow_tearing: false,
        }
    }
}

/// The Presenter trait - implemented by platform-specific backends
pub trait Presenter: Send + Sync {
    /// Get presenter ID
    fn id(&self) -> PresenterId;

    /// Get capabilities
    fn capabilities(&self) -> &PresenterCapabilities;

    /// Get current configuration
    fn config(&self) -> &PresenterConfig;

    /// Reconfigure the presenter
    fn reconfigure(&mut self, config: PresenterConfig) -> Result<(), PresenterError>;

    /// Resize the surface
    fn resize(&mut self, width: u32, height: u32) -> Result<(), PresenterError>;

    /// Begin a new frame
    fn begin_frame(&mut self) -> Result<Frame, PresenterError>;

    /// Present a frame
    fn present(&mut self, frame: Frame) -> Result<(), PresenterError>;

    /// Get current surface size
    fn size(&self) -> (u32, u32);

    /// Check if surface is valid
    fn is_valid(&self) -> bool;

    /// Get rehydration state for hot-swap
    fn rehydration_state(&self) -> RehydrationState;

    /// Restore from rehydration state
    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), PresenterError>;
}

/// Presenter manager - manages multiple presenters
pub struct PresenterManager {
    /// Active presenters
    presenters: RwLock<Vec<Box<dyn Presenter>>>,
    /// Next presenter ID
    next_id: std::sync::atomic::AtomicU64,
    /// Primary presenter index
    primary: RwLock<Option<usize>>,
}

impl PresenterManager {
    /// Create a new presenter manager
    pub fn new() -> Self {
        Self {
            presenters: RwLock::new(Vec::new()),
            next_id: std::sync::atomic::AtomicU64::new(1),
            primary: RwLock::new(None),
        }
    }

    /// Allocate a new presenter ID
    pub fn allocate_id(&self) -> PresenterId {
        PresenterId::new(self.next_id.fetch_add(1, std::sync::atomic::Ordering::Relaxed))
    }

    /// Register a presenter
    pub fn register(&self, presenter: Box<dyn Presenter>) -> PresenterId {
        let id = presenter.id();
        let mut presenters = self.presenters.write();

        // Set as primary if first
        if presenters.is_empty() {
            *self.primary.write() = Some(0);
        }

        presenters.push(presenter);
        id
    }

    /// Unregister a presenter
    pub fn unregister(&self, id: PresenterId) -> Option<Box<dyn Presenter>> {
        let mut presenters = self.presenters.write();
        let pos = presenters.iter().position(|p| p.id() == id)?;

        let presenter = presenters.remove(pos);

        // Update primary if needed
        let mut primary = self.primary.write();
        if let Some(p) = *primary {
            if p == pos {
                *primary = if presenters.is_empty() { None } else { Some(0) };
            } else if p > pos {
                *primary = Some(p - 1);
            }
        }

        Some(presenter)
    }

    /// Get primary presenter (mutable)
    pub fn primary_mut(&self) -> Option<impl std::ops::DerefMut<Target = Box<dyn Presenter>> + '_> {
        let primary = *self.primary.read();
        primary.and_then(|idx| {
            let presenters = self.presenters.write();
            if idx < presenters.len() {
                Some(parking_lot::RwLockWriteGuard::map(presenters, |p| &mut p[idx]))
            } else {
                None
            }
        })
    }

    /// Set primary presenter
    pub fn set_primary(&self, id: PresenterId) -> bool {
        let presenters = self.presenters.read();
        if let Some(pos) = presenters.iter().position(|p| p.id() == id) {
            *self.primary.write() = Some(pos);
            true
        } else {
            false
        }
    }

    /// Get all presenter IDs
    pub fn all_ids(&self) -> Vec<PresenterId> {
        self.presenters.read().iter().map(|p| p.id()).collect()
    }

    /// Get presenter count
    pub fn count(&self) -> usize {
        self.presenters.read().len()
    }

    /// Begin frames on all presenters
    pub fn begin_all_frames(&self) -> Vec<(PresenterId, Result<Frame, PresenterError>)> {
        let mut presenters = self.presenters.write();
        presenters.iter_mut()
            .map(|p| (p.id(), p.begin_frame()))
            .collect()
    }

    /// Present on all presenters
    pub fn present_all(&self, frames: Vec<(PresenterId, Frame)>) -> Vec<(PresenterId, Result<(), PresenterError>)> {
        let mut presenters = self.presenters.write();
        let mut results = Vec::new();

        for (id, frame) in frames {
            if let Some(presenter) = presenters.iter_mut().find(|p| p.id() == id) {
                results.push((id, presenter.present(frame)));
            }
        }

        results
    }

    /// Get rehydration states for all presenters
    pub fn rehydration_states(&self) -> Vec<(PresenterId, RehydrationState)> {
        self.presenters.read()
            .iter()
            .map(|p| (p.id(), p.rehydration_state()))
            .collect()
    }
}

impl Default for PresenterManager {
    fn default() -> Self {
        Self::new()
    }
}

/// Null presenter for testing
pub struct NullPresenter {
    id: PresenterId,
    capabilities: PresenterCapabilities,
    config: PresenterConfig,
    frame_number: u64,
}

impl NullPresenter {
    pub fn new(id: PresenterId) -> Self {
        Self {
            id,
            capabilities: PresenterCapabilities::default(),
            config: PresenterConfig::default(),
            frame_number: 0,
        }
    }
}

impl Presenter for NullPresenter {
    fn id(&self) -> PresenterId {
        self.id
    }

    fn capabilities(&self) -> &PresenterCapabilities {
        &self.capabilities
    }

    fn config(&self) -> &PresenterConfig {
        &self.config
    }

    fn reconfigure(&mut self, config: PresenterConfig) -> Result<(), PresenterError> {
        self.config = config;
        Ok(())
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), PresenterError> {
        self.config.size = (width, height);
        Ok(())
    }

    fn begin_frame(&mut self) -> Result<Frame, PresenterError> {
        self.frame_number += 1;
        Ok(Frame::new(self.frame_number, self.config.size))
    }

    fn present(&mut self, _frame: Frame) -> Result<(), PresenterError> {
        Ok(())
    }

    fn size(&self) -> (u32, u32) {
        self.config.size
    }

    fn is_valid(&self) -> bool {
        true
    }

    fn rehydration_state(&self) -> RehydrationState {
        RehydrationState::new()
            .with_value("frame_number", self.frame_number)
    }

    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), PresenterError> {
        if let Some(frame_number) = state.get_value::<u64>("frame_number") {
            self.frame_number = frame_number;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_null_presenter() {
        let id = PresenterId::new(1);
        let mut presenter = NullPresenter::new(id);

        assert_eq!(presenter.id(), id);
        assert!(presenter.is_valid());

        let frame = presenter.begin_frame().unwrap();
        assert_eq!(frame.number(), 1);

        presenter.present(frame).unwrap();
    }

    #[test]
    fn test_presenter_manager() {
        let manager = PresenterManager::new();

        let id1 = manager.allocate_id();
        let id2 = manager.allocate_id();

        manager.register(Box::new(NullPresenter::new(id1)));
        manager.register(Box::new(NullPresenter::new(id2)));

        assert_eq!(manager.count(), 2);

        let ids = manager.all_ids();
        assert!(ids.contains(&id1));
        assert!(ids.contains(&id2));
    }

    #[test]
    fn test_presenter_resize() {
        let id = PresenterId::new(1);
        let mut presenter = NullPresenter::new(id);

        presenter.resize(1920, 1080).unwrap();
        assert_eq!(presenter.size(), (1920, 1080));

        presenter.resize(2560, 1440).unwrap();
        assert_eq!(presenter.size(), (2560, 1440));
    }

    #[test]
    fn test_rehydration() {
        let id = PresenterId::new(1);
        let mut presenter1 = NullPresenter::new(id);

        // Advance frames
        for _ in 0..10 {
            let frame = presenter1.begin_frame().unwrap();
            presenter1.present(frame).unwrap();
        }

        // Get state
        let state = presenter1.rehydration_state();

        // Create new presenter and rehydrate
        let mut presenter2 = NullPresenter::new(id);
        presenter2.rehydrate(state).unwrap();

        // Frame numbers should continue
        let frame = presenter2.begin_frame().unwrap();
        assert_eq!(frame.number(), 11);
    }
}
