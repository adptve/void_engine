//! Desktop Presenter - winit/wgpu implementation
//!
//! Provides windowed and fullscreen presentation on desktop platforms using:
//! - winit for window management and input
//! - wgpu for GPU rendering (Vulkan/Metal/DX12/WebGPU)
//!
//! ## Features
//! - Automatic surface configuration and reconfiguration
//! - Proper resize handling with surface recreation
//! - VSync control via present modes
//! - Rehydration support for hot-swap
//! - Multi-window capability via PresenterManager

use crate::{
    Presenter, PresenterCapabilities, PresenterConfig, PresenterError, PresenterId,
    Frame, FrameState, RehydrationState, Rehydratable,
    SurfaceFormat, SurfaceConfig, PresentMode, AlphaMode,
};

use std::sync::Arc;
use parking_lot::RwLock;
use winit::window::Window;
use wgpu::{
    Instance, InstanceDescriptor, Backends, Adapter, Device, Queue, Surface,
    SurfaceConfiguration, TextureUsages, PowerPreference, RequestAdapterOptions,
    DeviceDescriptor, Features, Limits, MemoryHints, SurfaceTexture, SurfaceError,
    TextureFormat,
};

/// Desktop presenter using winit and wgpu
pub struct DesktopPresenter {
    /// Unique presenter ID
    id: PresenterId,

    /// winit window (wrapped in Arc for sharing)
    window: Arc<Window>,

    /// wgpu instance
    instance: Instance,

    /// wgpu adapter
    adapter: Adapter,

    /// wgpu device
    device: Arc<Device>,

    /// wgpu queue
    queue: Arc<Queue>,

    /// wgpu surface
    surface: Surface<'static>,

    /// Surface configuration
    surface_config: SurfaceConfiguration,

    /// Presenter configuration
    config: PresenterConfig,

    /// Presenter capabilities
    capabilities: PresenterCapabilities,

    /// Current frame number
    frame_number: u64,

    /// Current surface texture (if acquired)
    current_texture: RwLock<Option<SurfaceTexture>>,

    /// Surface validity
    surface_valid: bool,
}

impl DesktopPresenter {
    /// Create a new desktop presenter
    pub async fn new(id: PresenterId, window: Arc<Window>) -> Result<Self, PresenterError> {
        Self::with_config(id, window, PresenterConfig::default()).await
    }

    /// Create with specific configuration
    pub async fn with_config(
        id: PresenterId,
        window: Arc<Window>,
        config: PresenterConfig,
    ) -> Result<Self, PresenterError> {
        log::info!("Creating desktop presenter {:?}...", id);

        // Create wgpu instance
        let instance = Instance::new(InstanceDescriptor {
            backends: Backends::all(),
            ..Default::default()
        });

        // Create surface
        let surface = instance
            .create_surface(window.clone())
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create surface: {:?}", e)))?;

        // Request adapter
        let adapter = instance
            .request_adapter(&RequestAdapterOptions {
                power_preference: PowerPreference::HighPerformance,
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await
            .ok_or_else(|| PresenterError::BackendNotAvailable("No suitable adapter found".into()))?;

        log::info!("Using adapter: {:?}", adapter.get_info().name);

        // Request device and queue
        let (device, queue) = adapter
            .request_device(
                &DeviceDescriptor {
                    label: Some("metaverse_desktop_device"),
                    required_features: Features::empty(),
                    required_limits: Limits::default(),
                    memory_hints: MemoryHints::default(),
                },
                None,
            )
            .await
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create device: {:?}", e)))?;

        let device = Arc::new(device);
        let queue = Arc::new(queue);

        // Get surface capabilities
        let surface_caps = surface.get_capabilities(&adapter);
        let formats = surface_caps.formats;
        let present_modes = surface_caps.present_modes;
        let alpha_modes = surface_caps.alpha_modes;

        // Choose format (prefer sRGB)
        let format = formats
            .iter()
            .find(|f| matches!(f, TextureFormat::Bgra8UnormSrgb | TextureFormat::Rgba8UnormSrgb))
            .copied()
            .unwrap_or(formats[0]);

        // Configure surface
        let size = window.inner_size();
        let surface_config = SurfaceConfiguration {
            usage: TextureUsages::RENDER_ATTACHMENT,
            format,
            width: size.width.max(1),
            height: size.height.max(1),
            present_mode: convert_present_mode(&config.present_mode, &present_modes),
            alpha_mode: alpha_modes.first().copied().unwrap_or(wgpu::CompositeAlphaMode::Auto),
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };

        surface.configure(&device, &surface_config);

        // Build capabilities
        let capabilities = PresenterCapabilities {
            present_modes: present_modes.iter().map(|m| convert_present_mode_to_void(*m)).collect(),
            formats: formats.iter().map(|f| convert_format_to_void(*f)).collect(),
            max_resolution: (surface_caps.max_texture_dimension_2d, surface_caps.max_texture_dimension_2d),
            hdr_support: formats.iter().any(|f| matches!(f, TextureFormat::Rgba16Float)),
            vrr_support: present_modes.contains(&wgpu::PresentMode::Immediate),
            xr_passthrough: false,
        };

        log::info!(
            "Desktop presenter initialized: {}x{}, format: {:?}, present_mode: {:?}",
            surface_config.width,
            surface_config.height,
            format,
            surface_config.present_mode
        );

        Ok(Self {
            id,
            window,
            instance,
            adapter,
            device,
            queue,
            surface,
            surface_config,
            config,
            capabilities,
            frame_number: 0,
            current_texture: RwLock::new(None),
            surface_valid: true,
        })
    }

    /// Get the wgpu device
    pub fn device(&self) -> &Arc<Device> {
        &self.device
    }

    /// Get the wgpu queue
    pub fn queue(&self) -> &Arc<Queue> {
        &self.queue
    }

    /// Get the window
    pub fn window(&self) -> &Arc<Window> {
        &self.window
    }

    /// Get the surface format
    pub fn surface_format(&self) -> TextureFormat {
        self.surface_config.format
    }

    /// Reconfigure the surface (internal)
    fn reconfigure_surface(&mut self) -> Result<(), PresenterError> {
        log::debug!("Reconfiguring surface: {}x{}", self.surface_config.width, self.surface_config.height);

        // Clear current texture if any
        *self.current_texture.write() = None;

        // Reconfigure
        self.surface.configure(&self.device, &self.surface_config);
        self.surface_valid = true;

        Ok(())
    }

    /// Handle surface lost error
    fn handle_surface_lost(&mut self) -> Result<(), PresenterError> {
        log::warn!("Surface lost, attempting to recreate");
        self.surface_valid = false;
        self.reconfigure_surface()
    }
}

impl Presenter for DesktopPresenter {
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

        // Update surface configuration
        self.surface_config.present_mode = convert_present_mode(
            &self.config.present_mode,
            &self.capabilities.present_modes.iter()
                .map(|m| convert_present_mode_from_void(*m))
                .collect::<Vec<_>>()
        );

        // Update size if specified
        self.surface_config.width = self.config.size.0.max(1);
        self.surface_config.height = self.config.size.1.max(1);

        self.reconfigure_surface()
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), PresenterError> {
        if width == 0 || height == 0 {
            log::warn!("Attempted to resize to zero size: {}x{}", width, height);
            return Ok(());
        }

        if self.surface_config.width == width && self.surface_config.height == height {
            return Ok(());
        }

        log::debug!("Resizing presenter from {}x{} to {}x{}",
            self.surface_config.width, self.surface_config.height,
            width, height
        );

        self.surface_config.width = width;
        self.surface_config.height = height;
        self.config.size = (width, height);

        self.reconfigure_surface()
    }

    fn begin_frame(&mut self) -> Result<Frame, PresenterError> {
        // Acquire surface texture
        let texture = match self.surface.get_current_texture() {
            Ok(texture) => texture,
            Err(SurfaceError::Lost) => {
                self.handle_surface_lost()?;
                self.surface.get_current_texture()
                    .map_err(|e| PresenterError::FrameAcquisition(format!("Surface lost and recreation failed: {:?}", e)))?
            }
            Err(SurfaceError::Outdated) => {
                self.reconfigure_surface()?;
                self.surface.get_current_texture()
                    .map_err(|e| PresenterError::FrameAcquisition(format!("Surface outdated and recreation failed: {:?}", e)))?
            }
            Err(e) => {
                return Err(PresenterError::FrameAcquisition(format!("{:?}", e)));
            }
        };

        // Store texture for presentation
        *self.current_texture.write() = Some(texture);

        // Increment frame number
        self.frame_number += 1;

        // Create frame
        let mut frame = Frame::new(self.frame_number, (self.surface_config.width, self.surface_config.height));

        // Set target FPS if configured
        if self.config.target_frame_rate > 0 {
            frame = frame.with_target_fps(self.config.target_frame_rate);
        }

        Ok(frame)
    }

    fn present(&mut self, mut frame: Frame) -> Result<(), PresenterError> {
        // Take the surface texture
        let texture = self.current_texture.write().take()
            .ok_or_else(|| PresenterError::PresentationFailed("No texture to present".into()))?;

        // Mark frame as presented
        frame.mark_presented();

        // Present the texture
        texture.present();

        Ok(())
    }

    fn size(&self) -> (u32, u32) {
        (self.surface_config.width, self.surface_config.height)
    }

    fn is_valid(&self) -> bool {
        self.surface_valid
    }

    fn rehydration_state(&self) -> RehydrationState {
        RehydrationState::new()
            .with_value("frame_number", self.frame_number)
            .with_value("width", self.surface_config.width as i64)
            .with_value("height", self.surface_config.height as i64)
            .with_value("format", format!("{:?}", self.surface_config.format))
    }

    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), PresenterError> {
        // Restore frame number
        if let Some(frame_number) = state.get_value::<u64>("frame_number") {
            self.frame_number = frame_number;
            log::debug!("Rehydrated frame number: {}", frame_number);
        }

        // Restore size
        let width = state.get_value::<i64>("width").unwrap_or(800) as u32;
        let height = state.get_value::<i64>("height").unwrap_or(600) as u32;

        if width != self.surface_config.width || height != self.surface_config.height {
            self.resize(width, height)?;
        }

        log::info!("Desktop presenter rehydrated successfully");
        Ok(())
    }
}

// Conversion utilities

fn convert_present_mode(mode: &PresentMode, available: &[wgpu::PresentMode]) -> wgpu::PresentMode {
    let target = convert_present_mode_from_void(*mode);

    if available.contains(&target) {
        target
    } else {
        // Fallback to Fifo (always available)
        wgpu::PresentMode::Fifo
    }
}

fn convert_present_mode_from_void(mode: PresentMode) -> wgpu::PresentMode {
    match mode {
        PresentMode::Immediate => wgpu::PresentMode::Immediate,
        PresentMode::Mailbox => wgpu::PresentMode::Mailbox,
        PresentMode::Fifo => wgpu::PresentMode::Fifo,
        PresentMode::FifoRelaxed => wgpu::PresentMode::FifoRelaxed,
    }
}

fn convert_present_mode_to_void(mode: wgpu::PresentMode) -> PresentMode {
    match mode {
        wgpu::PresentMode::Immediate => PresentMode::Immediate,
        wgpu::PresentMode::Mailbox => PresentMode::Mailbox,
        wgpu::PresentMode::Fifo => PresentMode::Fifo,
        wgpu::PresentMode::FifoRelaxed => PresentMode::FifoRelaxed,
        wgpu::PresentMode::AutoVsync => PresentMode::Fifo,
        wgpu::PresentMode::AutoNoVsync => PresentMode::Immediate,
    }
}

fn convert_format_to_void(format: TextureFormat) -> SurfaceFormat {
    match format {
        TextureFormat::Bgra8Unorm => SurfaceFormat::Bgra8Unorm,
        TextureFormat::Bgra8UnormSrgb => SurfaceFormat::Bgra8UnormSrgb,
        TextureFormat::Rgba8Unorm => SurfaceFormat::Rgba8Unorm,
        TextureFormat::Rgba8UnormSrgb => SurfaceFormat::Rgba8UnormSrgb,
        TextureFormat::Rgba16Float => SurfaceFormat::Rgba16Float,
        TextureFormat::Rgb10a2Unorm => SurfaceFormat::Rgb10a2Unorm,
        _ => SurfaceFormat::Bgra8UnormSrgb, // Default fallback
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_present_mode_conversion() {
        assert_eq!(
            convert_present_mode_from_void(PresentMode::Fifo),
            wgpu::PresentMode::Fifo
        );
        assert_eq!(
            convert_present_mode_to_void(wgpu::PresentMode::Mailbox),
            PresentMode::Mailbox
        );
    }

    #[test]
    fn test_format_conversion() {
        assert_eq!(
            convert_format_to_void(TextureFormat::Bgra8UnormSrgb),
            SurfaceFormat::Bgra8UnormSrgb
        );
        assert_eq!(
            convert_format_to_void(TextureFormat::Rgba16Float),
            SurfaceFormat::Rgba16Float
        );
    }
}
