//! Desktop Presenter - connects to GPU via wgpu

use std::sync::Arc;
use winit::window::Window;
use wgpu::*;

/// Desktop presenter using wgpu
pub struct DesktopPresenter {
    surface: Surface<'static>,
    device: Arc<Device>,
    queue: Arc<Queue>,
    config: SurfaceConfiguration,
    format: TextureFormat,
    size: (u32, u32),
}

impl DesktopPresenter {
    /// Create a new desktop presenter
    pub async fn new(window: Arc<Window>) -> Self {
        log::info!("Creating desktop presenter...");

        // Create wgpu instance
        let instance = Instance::new(InstanceDescriptor {
            backends: Backends::all(),
            ..Default::default()
        });

        // Create surface
        let surface = instance.create_surface(window.clone())
            .expect("Failed to create surface");

        // Request adapter
        let adapter = instance
            .request_adapter(&RequestAdapterOptions {
                power_preference: PowerPreference::HighPerformance,
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await
            .expect("Failed to find adapter");

        log::info!("Using adapter: {:?}", adapter.get_info().name);

        // Request device and queue
        let (device, queue) = adapter
            .request_device(
                &DeviceDescriptor {
                    label: Some("metaverse_device"),
                    required_features: Features::empty(),
                    required_limits: Limits::default(),
                    memory_hints: MemoryHints::default(),
                },
                None,
            )
            .await
            .expect("Failed to create device");

        let device = Arc::new(device);
        let queue = Arc::new(queue);

        // Get surface format
        let surface_caps = surface.get_capabilities(&adapter);
        let format = surface_caps
            .formats
            .iter()
            .find(|f| f.is_srgb())
            .copied()
            .unwrap_or(surface_caps.formats[0]);

        // Configure surface
        let size = window.inner_size();
        let config = SurfaceConfiguration {
            usage: TextureUsages::RENDER_ATTACHMENT,
            format,
            width: size.width.max(1),
            height: size.height.max(1),
            present_mode: PresentMode::AutoVsync,
            alpha_mode: surface_caps.alpha_modes[0],
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };
        surface.configure(&device, &config);

        log::info!("Presenter initialized: {}x{}, format: {:?}", config.width, config.height, format);

        Self {
            surface,
            device,
            queue,
            config,
            format,
            size: (size.width, size.height),
        }
    }

    /// Get the device
    pub fn device(&self) -> &Arc<Device> {
        &self.device
    }

    /// Get the queue
    pub fn queue(&self) -> &Arc<Queue> {
        &self.queue
    }

    /// Get the surface format
    pub fn format(&self) -> TextureFormat {
        self.format
    }

    /// Get current size
    pub fn size(&self) -> (u32, u32) {
        self.size
    }

    /// Resize the presenter
    pub fn resize(&mut self, new_size: (u32, u32)) {
        if new_size.0 > 0 && new_size.1 > 0 {
            self.size = new_size;
            self.config.width = new_size.0;
            self.config.height = new_size.1;
            self.surface.configure(&self.device, &self.config);
            log::debug!("Presenter resized to {}x{}", new_size.0, new_size.1);
        }
    }

    /// Acquire a frame for rendering
    pub fn acquire_frame(&mut self) -> Result<SurfaceTexture, SurfaceError> {
        self.surface.get_current_texture()
    }
}
