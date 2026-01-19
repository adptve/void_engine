//! DRM Presenter - Direct GPU rendering without display server
//!
//! Renders directly to the GPU via DRM/KMS, bypassing X11/Wayland.
//! This is used for the Metaverse OS where we ARE the display server.

use std::os::fd::{AsFd, BorrowedFd, OwnedFd};
use std::sync::Arc;
use std::path::Path;
use std::fs::OpenOptions;

use parking_lot::Mutex;
use drm::control::{Device as ControlDevice, Mode, connector, crtc, framebuffer};
use drm::Device as DrmDevice;
use gbm::{Device as GbmDevice, BufferObject, BufferObjectFlags};

use crate::{
    Presenter, PresenterId, PresenterCapabilities, PresenterConfig, PresenterError,
    Frame, SurfaceFormat, PresentMode, RehydrationState,
};

/// DRM device wrapper that implements the required traits
pub struct Card(OwnedFd);

impl AsFd for Card {
    fn as_fd(&self) -> BorrowedFd<'_> {
        self.0.as_fd()
    }
}

impl DrmDevice for Card {}
impl ControlDevice for Card {}

impl Card {
    /// Open a DRM device
    pub fn open<P: AsRef<Path>>(path: P) -> std::io::Result<Self> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .open(path)?;
        Ok(Self(file.into()))
    }
}

/// GBM state - all non-Sync types wrapped together
struct GbmState {
    gbm: GbmDevice<Card>,
    front_buffer: Option<BufferObject<()>>,
    back_buffer: Option<BufferObject<()>>,
    front_fb: Option<framebuffer::Handle>,
    back_fb: Option<framebuffer::Handle>,
}

/// DRM Presenter - renders directly to GPU
pub struct DrmPresenter {
    id: PresenterId,
    capabilities: PresenterCapabilities,
    config: PresenterConfig,

    // DRM state (Card is Sync)
    card: Card,
    connector: connector::Handle,
    crtc: crtc::Handle,
    mode: Mode,

    // GBM state (wrapped for Sync)
    gbm_state: Mutex<GbmState>,

    // wgpu state (these are Sync)
    instance: wgpu::Instance,
    adapter: Option<wgpu::Adapter>,
    device: Option<Arc<wgpu::Device>>,
    queue: Option<Arc<wgpu::Queue>>,

    // Frame tracking
    frame_number: u64,
}

// Safety: All non-Sync types are wrapped in Mutex<GbmState>
// DRM operations go through the Card which we protect via the Mutex
unsafe impl Sync for DrmPresenter {}

impl DrmPresenter {
    /// Create a new DRM presenter
    pub fn new(id: PresenterId, device_path: &str) -> Result<Self, PresenterError> {
        log::info!("Creating DRM presenter from {}", device_path);

        // Open DRM device
        let card = Card::open(device_path)
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to open DRM device: {}", e)))?;

        // Get DRM resources
        let resources = card.resource_handles()
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to get resources: {}", e)))?;

        // Find first connected connector
        let connector = resources.connectors().iter()
            .find_map(|&conn| {
                let info = card.get_connector(conn, true).ok()?;
                if info.state() == connector::State::Connected {
                    Some(conn)
                } else {
                    None
                }
            })
            .ok_or_else(|| PresenterError::SurfaceCreation("No connected display found".into()))?;

        let connector_info = card.get_connector(connector, true)
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to get connector info: {}", e)))?;

        // Get preferred mode (first mode is usually preferred/native)
        let mode = connector_info.modes().first()
            .cloned()
            .ok_or_else(|| PresenterError::SurfaceCreation("No display modes available".into()))?;

        log::info!("Using mode: {}x{}@{}Hz",
            mode.size().0, mode.size().1, mode.vrefresh());

        // Find a CRTC for this connector
        let crtc = resources.crtcs().first()
            .cloned()
            .ok_or_else(|| PresenterError::SurfaceCreation("No CRTC available".into()))?;

        // Create GBM device (consumes card)
        let gbm_card = Card::open(device_path)
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to open DRM for GBM: {}", e)))?;
        let gbm = GbmDevice::new(gbm_card)
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create GBM device: {}", e)))?;

        // Create wgpu instance
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::VULKAN,
            ..Default::default()
        });

        let config = PresenterConfig {
            format: SurfaceFormat::Bgra8UnormSrgb,
            present_mode: PresentMode::Fifo,
            size: (mode.size().0 as u32, mode.size().1 as u32),
            enable_hdr: false,
            target_frame_rate: mode.vrefresh() as u32,
            allow_tearing: false,
        };

        let capabilities = PresenterCapabilities {
            present_modes: vec![PresentMode::Fifo, PresentMode::Immediate],
            formats: vec![SurfaceFormat::Bgra8UnormSrgb, SurfaceFormat::Rgba8UnormSrgb],
            max_resolution: (mode.size().0 as u32, mode.size().1 as u32),
            hdr_support: false,
            vrr_support: false,
            xr_passthrough: false,
        };

        Ok(Self {
            id,
            capabilities,
            config,
            card,
            connector,
            crtc,
            mode,
            gbm_state: Mutex::new(GbmState {
                gbm,
                front_buffer: None,
                back_buffer: None,
                front_fb: None,
                back_fb: None,
            }),
            instance,
            adapter: None,
            device: None,
            queue: None,
            frame_number: 0,
        })
    }

    /// Initialize wgpu (async)
    pub async fn init_gpu(&mut self) -> Result<(), PresenterError> {
        log::info!("Initializing GPU...");

        // Request adapter
        let adapter = self.instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::HighPerformance,
                compatible_surface: None, // No surface yet
                force_fallback_adapter: false,
            })
            .await
            .ok_or_else(|| PresenterError::BackendNotAvailable("No GPU adapter found".into()))?;

        log::info!("Using GPU: {}", adapter.get_info().name);

        // Request device
        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: Some("drm_presenter_device"),
                    required_features: wgpu::Features::empty(),
                    required_limits: wgpu::Limits::default(),
                },
                None,
            )
            .await
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create device: {}", e)))?;

        self.adapter = Some(adapter);
        self.device = Some(Arc::new(device));
        self.queue = Some(Arc::new(queue));

        // Create GBM buffers for double buffering
        self.create_buffers()?;

        log::info!("DRM presenter initialized: {}x{}", self.config.size.0, self.config.size.1);

        Ok(())
    }

    /// Create GBM buffers for rendering
    fn create_buffers(&mut self) -> Result<(), PresenterError> {
        let (width, height) = self.config.size;

        let mut gbm_state = self.gbm_state.lock();

        // Create front buffer
        let front = gbm_state.gbm.create_buffer_object::<()>(
            width,
            height,
            gbm::Format::Xrgb8888,
            BufferObjectFlags::SCANOUT | BufferObjectFlags::RENDERING,
        ).map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create front buffer: {}", e)))?;

        // Create back buffer
        let back = gbm_state.gbm.create_buffer_object::<()>(
            width,
            height,
            gbm::Format::Xrgb8888,
            BufferObjectFlags::SCANOUT | BufferObjectFlags::RENDERING,
        ).map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create back buffer: {}", e)))?;

        // Create DRM framebuffers
        let front_fb = self.card.add_framebuffer(&front, 32, 32)
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create front FB: {}", e)))?;

        let back_fb = self.card.add_framebuffer(&back, 32, 32)
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to create back FB: {}", e)))?;

        // Set the mode on the CRTC
        self.card.set_crtc(self.crtc, Some(front_fb), (0, 0), &[self.connector], Some(self.mode))
            .map_err(|e| PresenterError::SurfaceCreation(format!("Failed to set CRTC: {}", e)))?;

        // Store buffers
        gbm_state.front_buffer = Some(front);
        gbm_state.back_buffer = Some(back);
        gbm_state.front_fb = Some(front_fb);
        gbm_state.back_fb = Some(back_fb);

        Ok(())
    }

    /// Swap buffers (page flip)
    fn swap_buffers(&mut self) -> Result<(), PresenterError> {
        let mut gbm_state = self.gbm_state.lock();

        // Swap using temp variables to avoid double borrow
        let temp_buffer = gbm_state.front_buffer.take();
        gbm_state.front_buffer = gbm_state.back_buffer.take();
        gbm_state.back_buffer = temp_buffer;

        let temp_fb = gbm_state.front_fb.take();
        gbm_state.front_fb = gbm_state.back_fb.take();
        gbm_state.back_fb = temp_fb;

        // Page flip to the new front buffer
        if let Some(fb) = gbm_state.front_fb {
            self.card.page_flip(self.crtc, fb, drm::control::PageFlipFlags::EVENT, None)
                .map_err(|e| PresenterError::PresentationFailed(format!("Page flip failed: {}", e)))?;
        }

        Ok(())
    }

    /// Get the wgpu device
    pub fn device(&self) -> Option<&Arc<wgpu::Device>> {
        self.device.as_ref()
    }

    /// Get the wgpu queue
    pub fn queue(&self) -> Option<&Arc<wgpu::Queue>> {
        self.queue.as_ref()
    }
}

impl Presenter for DrmPresenter {
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
        // DRM mode is fixed, but we can update config
        self.config.size = (width, height);
        Ok(())
    }

    fn begin_frame(&mut self) -> Result<Frame, PresenterError> {
        self.frame_number += 1;
        Ok(Frame::new(self.frame_number, self.config.size))
    }

    fn present(&mut self, _frame: Frame) -> Result<(), PresenterError> {
        self.swap_buffers()
    }

    fn size(&self) -> (u32, u32) {
        self.config.size
    }

    fn is_valid(&self) -> bool {
        self.device.is_some()
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

/// Try to find the best DRM device
pub fn find_drm_device() -> Option<String> {
    // Try common paths
    for path in &[
        "/dev/dri/card0",
        "/dev/dri/card1",
        "/dev/dri/renderD128",
    ] {
        if std::path::Path::new(path).exists() {
            return Some(path.to_string());
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_find_drm_device() {
        // This test only works on Linux with DRM
        if cfg!(target_os = "linux") {
            // Just check it doesn't panic
            let _ = find_drm_device();
        }
    }
}
