//! XR Presenter - Bridge between OpenXR and void_render
//!
//! This presenter integrates with void_xr's OpenXR backend to render
//! VR/AR content to headset displays.
//!
//! # Architecture
//!
//! ```text
//! ┌─────────────────────────────────────────────────────────────────────┐
//! │                         XR Presenter                                │
//! ├─────────────────────────────────────────────────────────────────────┤
//! │  void_xr (OpenXR)                                                   │
//! │    ├── Swapchain Left Eye  ◄─── Render Graph outputs here          │
//! │    ├── Swapchain Right Eye ◄─── Render Graph outputs here          │
//! │    └── Frame Timing        ◄─── Synchronizes with HMD refresh      │
//! ├─────────────────────────────────────────────────────────────────────┤
//! │  void_render                                                        │
//! │    ├── Layer Compositor    ◄─── Composites app layers per-eye      │
//! │    └── Render Graph        ◄─── Executes render passes             │
//! └─────────────────────────────────────────────────────────────────────┘
//! ```
//!
//! # Usage
//!
//! ```ignore
//! use void_presenter::xr::{XrPresenter, XrPresenterConfig};
//! use void_xr::prelude::*;
//!
//! // Create XR system
//! let mut xr_system = XrSystem::new();
//! xr_system.register_backend(OpenXrBackend::new()?);
//! xr_system.initialize()?;
//!
//! // Create XR presenter
//! let config = XrPresenterConfig::default();
//! let mut presenter = XrPresenter::new(xr_system, device, queue, config)?;
//!
//! // Frame loop
//! loop {
//!     let xr_frame = presenter.begin_frame()?;
//!
//!     for view in xr_frame.views() {
//!         // Get render target for this eye
//!         let target = presenter.render_target(view.eye)?;
//!
//!         // Render to the target
//!         render_graph.execute(&target, &view);
//!     }
//!
//!     presenter.end_frame(xr_frame)?;
//! }
//! ```

use std::sync::Arc;
use parking_lot::RwLock;
use thiserror::Error;

use crate::{
    Presenter, PresenterId, PresenterConfig, PresenterCapabilities,
    PresenterError, SurfaceFormat, PresentMode,
    Frame, RehydrationState,
};

/// XR Presenter errors
#[derive(Debug, Error)]
pub enum XrPresenterError {
    #[error("XR system not initialized")]
    NotInitialized,

    #[error("XR session not ready: {0}")]
    SessionNotReady(String),

    #[error("Failed to acquire swapchain image: {0}")]
    SwapchainAcquire(String),

    #[error("Failed to create render target: {0}")]
    RenderTargetCreate(String),

    #[error("Eye not available: {0:?}")]
    EyeNotAvailable(Eye),

    #[error("Frame timing error: {0}")]
    FrameTiming(String),

    #[error("GPU error: {0}")]
    GpuError(String),
}

impl From<XrPresenterError> for PresenterError {
    fn from(e: XrPresenterError) -> Self {
        PresenterError::PresentationFailed(e.to_string())
    }
}

/// Eye identifier
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Eye {
    Left,
    Right,
}

/// View information for rendering
#[derive(Debug, Clone)]
pub struct XrView {
    /// Which eye
    pub eye: Eye,
    /// View matrix (world to eye space)
    pub view_matrix: [[f32; 4]; 4],
    /// Projection matrix
    pub projection_matrix: [[f32; 4]; 4],
    /// Field of view (radians)
    pub fov: Fov,
    /// Render target size
    pub render_size: (u32, u32),
}

/// Field of view
#[derive(Debug, Clone, Copy)]
pub struct Fov {
    pub angle_left: f32,
    pub angle_right: f32,
    pub angle_up: f32,
    pub angle_down: f32,
}

/// Pose (position + orientation)
#[derive(Debug, Clone, Copy, Default)]
pub struct Pose {
    pub position: [f32; 3],
    pub orientation: [f32; 4], // quaternion (x, y, z, w)
}

/// XR frame data
pub struct XrFrame {
    /// Frame number
    pub frame_number: u64,
    /// Predicted display time (nanoseconds)
    pub predicted_display_time: u64,
    /// Delta time since last frame (seconds)
    pub delta_time: f32,
    /// Views for rendering (one per eye)
    pub views: Vec<XrView>,
    /// Head pose
    pub head_pose: Pose,
    /// Should render this frame
    pub should_render: bool,
}

impl XrFrame {
    /// Get views for rendering
    pub fn views(&self) -> &[XrView] {
        &self.views
    }

    /// Get view for specific eye
    pub fn view(&self, eye: Eye) -> Option<&XrView> {
        self.views.iter().find(|v| v.eye == eye)
    }
}

/// Per-eye render target
pub struct EyeRenderTarget {
    /// Eye this target is for
    pub eye: Eye,
    /// Texture view to render to
    pub texture_view: wgpu::TextureView,
    /// Texture size
    pub size: (u32, u32),
    /// Texture format
    pub format: wgpu::TextureFormat,
}

/// XR Presenter configuration
#[derive(Debug, Clone)]
pub struct XrPresenterConfig {
    /// Color format for render targets
    pub color_format: wgpu::TextureFormat,
    /// Depth format
    pub depth_format: Option<wgpu::TextureFormat>,
    /// Sample count for MSAA
    pub sample_count: u32,
    /// Supersampling factor (1.0 = native resolution)
    pub supersampling: f32,
    /// Enable foveated rendering if available
    pub foveated_rendering: bool,
    /// Foveation level (0.0 - 1.0)
    pub foveation_level: f32,
}

impl Default for XrPresenterConfig {
    fn default() -> Self {
        Self {
            color_format: wgpu::TextureFormat::Rgba8UnormSrgb,
            depth_format: Some(wgpu::TextureFormat::Depth32Float),
            sample_count: 1,
            supersampling: 1.0,
            foveated_rendering: false,
            foveation_level: 0.5,
        }
    }
}

/// Per-eye state
struct EyeState {
    /// Current swapchain image index
    image_index: Option<u32>,
    /// Render target texture (created from swapchain or intermediate)
    render_texture: Option<wgpu::Texture>,
    /// Render target view
    render_view: Option<wgpu::TextureView>,
    /// Depth texture
    depth_texture: Option<wgpu::Texture>,
    /// Depth view
    depth_view: Option<wgpu::TextureView>,
    /// Resolution
    resolution: (u32, u32),
}

impl Default for EyeState {
    fn default() -> Self {
        Self {
            image_index: None,
            render_texture: None,
            render_view: None,
            depth_texture: None,
            depth_view: None,
            resolution: (1920, 1920),
        }
    }
}

/// XR Presenter - bridges void_xr and void_render
pub struct XrPresenter {
    /// Presenter ID
    id: PresenterId,
    /// GPU device
    device: Arc<wgpu::Device>,
    /// GPU queue
    queue: Arc<wgpu::Queue>,
    /// Configuration
    config: XrPresenterConfig,
    /// Presenter config (for trait)
    presenter_config: PresenterConfig,
    /// Capabilities
    capabilities: PresenterCapabilities,
    /// Per-eye state
    eye_states: [EyeState; 2],
    /// Current frame number
    frame_number: u64,
    /// Is currently in a frame
    in_frame: bool,
    /// Last frame time
    last_frame_time: std::time::Instant,
    /// HMD info cache
    hmd_resolution: (u32, u32),
    hmd_refresh_rate: f32,
    /// Session state
    session_ready: bool,
}

impl XrPresenter {
    /// Create a new XR Presenter
    ///
    /// Note: This creates render targets but does NOT own the XrSystem.
    /// The XrSystem should be managed externally and its frame timing
    /// coordinated with this presenter.
    pub fn new(
        id: PresenterId,
        device: Arc<wgpu::Device>,
        queue: Arc<wgpu::Queue>,
        config: XrPresenterConfig,
        resolution_per_eye: (u32, u32),
        refresh_rate: f32,
    ) -> Result<Self, XrPresenterError> {
        let mut presenter = Self {
            id,
            device,
            queue,
            config,
            presenter_config: PresenterConfig {
                format: SurfaceFormat::Rgba8UnormSrgb,
                present_mode: PresentMode::Fifo,
                size: (resolution_per_eye.0 * 2, resolution_per_eye.1),
                enable_hdr: false,
                target_frame_rate: refresh_rate as u32,
                allow_tearing: false,
            },
            capabilities: PresenterCapabilities {
                present_modes: vec![PresentMode::Fifo],
                formats: vec![SurfaceFormat::Rgba8UnormSrgb],
                max_resolution: (resolution_per_eye.0, resolution_per_eye.1),
                hdr_support: false,
                vrr_support: false,
                xr_passthrough: false, // Would be set from XrSystem
            },
            eye_states: [EyeState::default(), EyeState::default()],
            frame_number: 0,
            in_frame: false,
            last_frame_time: std::time::Instant::now(),
            hmd_resolution: resolution_per_eye,
            hmd_refresh_rate: refresh_rate,
            session_ready: false,
        };

        // Create render targets for each eye
        presenter.create_render_targets(resolution_per_eye)?;

        Ok(presenter)
    }

    /// Create render targets for each eye
    fn create_render_targets(&mut self, resolution: (u32, u32)) -> Result<(), XrPresenterError> {
        let scaled_resolution = (
            (resolution.0 as f32 * self.config.supersampling) as u32,
            (resolution.1 as f32 * self.config.supersampling) as u32,
        );

        for (i, eye_state) in self.eye_states.iter_mut().enumerate() {
            let eye_name = if i == 0 { "left" } else { "right" };

            // Create color render target
            let color_texture = self.device.create_texture(&wgpu::TextureDescriptor {
                label: Some(&format!("xr_{}_color", eye_name)),
                size: wgpu::Extent3d {
                    width: scaled_resolution.0,
                    height: scaled_resolution.1,
                    depth_or_array_layers: 1,
                },
                mip_level_count: 1,
                sample_count: self.config.sample_count,
                dimension: wgpu::TextureDimension::D2,
                format: self.config.color_format,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT
                    | wgpu::TextureUsages::TEXTURE_BINDING
                    | wgpu::TextureUsages::COPY_SRC,
                view_formats: &[],
            });

            let color_view = color_texture.create_view(&wgpu::TextureViewDescriptor::default());

            // Create depth render target if configured
            let (depth_texture, depth_view) = if let Some(depth_format) = self.config.depth_format {
                let texture = self.device.create_texture(&wgpu::TextureDescriptor {
                    label: Some(&format!("xr_{}_depth", eye_name)),
                    size: wgpu::Extent3d {
                        width: scaled_resolution.0,
                        height: scaled_resolution.1,
                        depth_or_array_layers: 1,
                    },
                    mip_level_count: 1,
                    sample_count: self.config.sample_count,
                    dimension: wgpu::TextureDimension::D2,
                    format: depth_format,
                    usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
                    view_formats: &[],
                });
                let view = texture.create_view(&wgpu::TextureViewDescriptor::default());
                (Some(texture), Some(view))
            } else {
                (None, None)
            };

            eye_state.render_texture = Some(color_texture);
            eye_state.render_view = Some(color_view);
            eye_state.depth_texture = depth_texture;
            eye_state.depth_view = depth_view;
            eye_state.resolution = scaled_resolution;
        }

        Ok(())
    }

    /// Mark session as ready (call when XR session becomes ready)
    pub fn set_session_ready(&mut self, ready: bool) {
        self.session_ready = ready;
    }

    /// Check if session is ready
    pub fn is_session_ready(&self) -> bool {
        self.session_ready
    }

    /// Begin an XR frame
    ///
    /// This should be called after XrSystem::begin_frame() and before rendering.
    /// Pass the views from XrSystem.
    pub fn begin_xr_frame(
        &mut self,
        views: Vec<XrView>,
        head_pose: Pose,
        predicted_display_time: u64,
        should_render: bool,
    ) -> Result<XrFrame, XrPresenterError> {
        if !self.session_ready {
            return Err(XrPresenterError::SessionNotReady("Session not ready".into()));
        }

        if self.in_frame {
            return Err(XrPresenterError::FrameTiming("Already in frame".into()));
        }

        self.frame_number += 1;
        self.in_frame = true;

        let now = std::time::Instant::now();
        let delta_time = now.duration_since(self.last_frame_time).as_secs_f32();
        self.last_frame_time = now;

        Ok(XrFrame {
            frame_number: self.frame_number,
            predicted_display_time,
            delta_time,
            views,
            head_pose,
            should_render,
        })
    }

    /// Get render target for an eye
    pub fn render_target(&self, eye: Eye) -> Result<EyeRenderTarget, XrPresenterError> {
        if !self.in_frame {
            return Err(XrPresenterError::FrameTiming("Not in frame".into()));
        }

        let idx = match eye {
            Eye::Left => 0,
            Eye::Right => 1,
        };

        let eye_state = &self.eye_states[idx];

        let texture_view = eye_state
            .render_view
            .as_ref()
            .ok_or(XrPresenterError::EyeNotAvailable(eye))?
            .clone();

        Ok(EyeRenderTarget {
            eye,
            texture_view,
            size: eye_state.resolution,
            format: self.config.color_format,
        })
    }

    /// Get depth target for an eye
    pub fn depth_target(&self, eye: Eye) -> Option<&wgpu::TextureView> {
        let idx = match eye {
            Eye::Left => 0,
            Eye::Right => 1,
        };

        self.eye_states[idx].depth_view.as_ref()
    }

    /// End the XR frame
    ///
    /// After this, call XrSystem::end_frame() to submit to OpenXR.
    pub fn end_xr_frame(&mut self, _frame: XrFrame) -> Result<(), XrPresenterError> {
        if !self.in_frame {
            return Err(XrPresenterError::FrameTiming("Not in frame".into()));
        }

        self.in_frame = false;
        Ok(())
    }

    /// Copy rendered content to OpenXR swapchain textures
    ///
    /// This should be called when OpenXR provides swapchain images that
    /// we need to copy our rendered content to.
    pub fn copy_to_swapchain(
        &self,
        encoder: &mut wgpu::CommandEncoder,
        eye: Eye,
        swapchain_texture: &wgpu::Texture,
        swapchain_size: (u32, u32),
    ) -> Result<(), XrPresenterError> {
        let idx = match eye {
            Eye::Left => 0,
            Eye::Right => 1,
        };

        let source_texture = self.eye_states[idx]
            .render_texture
            .as_ref()
            .ok_or(XrPresenterError::EyeNotAvailable(eye))?;

        let source_size = self.eye_states[idx].resolution;

        // Copy from our render target to the swapchain
        encoder.copy_texture_to_texture(
            wgpu::ImageCopyTexture {
                texture: source_texture,
                mip_level: 0,
                origin: wgpu::Origin3d::ZERO,
                aspect: wgpu::TextureAspect::All,
            },
            wgpu::ImageCopyTexture {
                texture: swapchain_texture,
                mip_level: 0,
                origin: wgpu::Origin3d::ZERO,
                aspect: wgpu::TextureAspect::All,
            },
            wgpu::Extent3d {
                width: source_size.0.min(swapchain_size.0),
                height: source_size.1.min(swapchain_size.1),
                depth_or_array_layers: 1,
            },
        );

        Ok(())
    }

    /// Get the combined stereo view (for spectator/recording)
    pub fn stereo_view(&self) -> Option<StereoView> {
        if self.eye_states[0].render_view.is_none() || self.eye_states[1].render_view.is_none() {
            return None;
        }

        Some(StereoView {
            left_eye: self.eye_states[0].render_view.as_ref().unwrap().clone(),
            right_eye: self.eye_states[1].render_view.as_ref().unwrap().clone(),
            size_per_eye: self.eye_states[0].resolution,
        })
    }

    /// Update resolution (e.g., when user changes supersampling)
    pub fn update_resolution(&mut self, resolution_per_eye: (u32, u32)) -> Result<(), XrPresenterError> {
        self.hmd_resolution = resolution_per_eye;
        self.create_render_targets(resolution_per_eye)
    }

    /// Get current resolution per eye
    pub fn resolution_per_eye(&self) -> (u32, u32) {
        self.hmd_resolution
    }

    /// Get current refresh rate
    pub fn refresh_rate(&self) -> f32 {
        self.hmd_refresh_rate
    }
}

/// Combined stereo view for spectator mode
pub struct StereoView {
    pub left_eye: wgpu::TextureView,
    pub right_eye: wgpu::TextureView,
    pub size_per_eye: (u32, u32),
}

// Implement the Presenter trait
impl Presenter for XrPresenter {
    fn id(&self) -> PresenterId {
        self.id
    }

    fn capabilities(&self) -> &PresenterCapabilities {
        &self.capabilities
    }

    fn config(&self) -> &PresenterConfig {
        &self.presenter_config
    }

    fn reconfigure(&mut self, config: PresenterConfig) -> Result<(), PresenterError> {
        self.presenter_config = config;
        Ok(())
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), PresenterError> {
        // For XR, resize means changing resolution per eye
        let resolution_per_eye = (width / 2, height);
        self.update_resolution(resolution_per_eye)
            .map_err(|e| PresenterError::ConfigError(e.to_string()))
    }

    fn begin_frame(&mut self) -> Result<Frame, PresenterError> {
        // Note: For XR, you should use begin_xr_frame() instead
        // This is a compatibility fallback
        self.frame_number += 1;
        Ok(Frame::new(self.frame_number, self.presenter_config.size))
    }

    fn present(&mut self, _frame: Frame) -> Result<(), PresenterError> {
        // Note: For XR, presentation is handled by OpenXR
        // This is a compatibility fallback
        Ok(())
    }

    fn size(&self) -> (u32, u32) {
        (self.hmd_resolution.0 * 2, self.hmd_resolution.1)
    }

    fn is_valid(&self) -> bool {
        self.session_ready
    }

    fn rehydration_state(&self) -> RehydrationState {
        RehydrationState::new()
            .with_value("frame_number", self.frame_number)
            .with_value("resolution_width", self.hmd_resolution.0)
            .with_value("resolution_height", self.hmd_resolution.1)
            .with_value("refresh_rate", self.hmd_refresh_rate)
            .with_value("supersampling", self.config.supersampling)
    }

    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), PresenterError> {
        if let Some(frame_number) = state.get_value::<u64>("frame_number") {
            self.frame_number = frame_number;
        }
        if let (Some(w), Some(h)) = (
            state.get_value::<u32>("resolution_width"),
            state.get_value::<u32>("resolution_height"),
        ) {
            self.hmd_resolution = (w, h);
        }
        if let Some(refresh_rate) = state.get_value::<f32>("refresh_rate") {
            self.hmd_refresh_rate = refresh_rate;
        }
        if let Some(supersampling) = state.get_value::<f32>("supersampling") {
            self.config.supersampling = supersampling;
        }

        // Recreate render targets with new settings
        self.create_render_targets(self.hmd_resolution)
            .map_err(|e| PresenterError::RehydrationFailed(e.to_string()))
    }
}

/// Helper to convert void_xr::View to XrView
pub fn convert_view(view: &void_xr::View) -> XrView {
    XrView {
        eye: match view.eye {
            void_xr::Eye::Left => Eye::Left,
            void_xr::Eye::Right => Eye::Right,
            void_xr::Eye::Center => Eye::Left, // Fallback
        },
        view_matrix: view.view_matrix.to_cols_array_2d(),
        projection_matrix: view.projection_matrix.to_cols_array_2d(),
        fov: Fov {
            angle_left: view.fov.angle_left,
            angle_right: view.fov.angle_right,
            angle_up: view.fov.angle_up,
            angle_down: view.fov.angle_down,
        },
        render_size: (view.render_size[0], view.render_size[1]),
    }
}

/// Helper to convert void_xr::Pose to Pose
pub fn convert_pose(pose: &void_xr::Pose) -> Pose {
    Pose {
        position: [pose.position.x, pose.position.y, pose.position.z],
        orientation: [
            pose.orientation.x,
            pose.orientation.y,
            pose.orientation.z,
            pose.orientation.w,
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_xr_presenter_config() {
        let config = XrPresenterConfig::default();
        assert_eq!(config.sample_count, 1);
        assert_eq!(config.supersampling, 1.0);
        assert!(!config.foveated_rendering);
    }

    #[test]
    fn test_eye_enum() {
        assert_ne!(Eye::Left, Eye::Right);
    }

    #[test]
    fn test_pose_default() {
        let pose = Pose::default();
        assert_eq!(pose.position, [0.0, 0.0, 0.0]);
    }
}
