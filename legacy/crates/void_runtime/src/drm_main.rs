//! DRM-based main loop
//!
//! Runs the Metaverse OS directly on DRM/KMS without a display server.
//! This module provides GPU buffer allocation, rendering, and page flipping.

use std::sync::Arc;
use std::time::{Duration, Instant};

use void_presenter::drm::{DrmPresenter, find_drm_device};
use void_presenter::{Presenter, PresenterId};

use crate::runtime::Runtime;
use crate::compositor::Compositor;
use crate::input::{InputManager, TtyInput};

/// DRM render buffer for GPU rendering
///
/// Manages wgpu textures that can be presented to DRM framebuffers.
/// Uses double buffering to avoid tearing.
pub struct DrmRenderBuffer {
    /// Front buffer texture (currently displayed)
    front_texture: wgpu::Texture,
    /// Front buffer view
    front_view: wgpu::TextureView,
    /// Back buffer texture (being rendered to)
    back_texture: wgpu::Texture,
    /// Back buffer view
    back_view: wgpu::TextureView,
    /// Buffer size
    size: (u32, u32),
    /// Format
    format: wgpu::TextureFormat,
    /// Current front buffer index (0 or 1)
    front_index: usize,
}

impl DrmRenderBuffer {
    /// Create a new DRM render buffer with double buffering
    pub fn new(device: &wgpu::Device, size: (u32, u32), format: wgpu::TextureFormat) -> Self {
        log::info!("Creating DRM render buffers: {}x{} {:?}", size.0, size.1, format);

        let create_buffer = |label: &str| {
            device.create_texture(&wgpu::TextureDescriptor {
                label: Some(label),
                size: wgpu::Extent3d {
                    width: size.0,
                    height: size.1,
                    depth_or_array_layers: 1,
                },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT
                    | wgpu::TextureUsages::COPY_SRC
                    | wgpu::TextureUsages::TEXTURE_BINDING,
                view_formats: &[],
            })
        };

        let front_texture = create_buffer("drm_front_buffer");
        let back_texture = create_buffer("drm_back_buffer");

        let front_view = front_texture.create_view(&wgpu::TextureViewDescriptor::default());
        let back_view = back_texture.create_view(&wgpu::TextureViewDescriptor::default());

        Self {
            front_texture,
            front_view,
            back_texture,
            back_view,
            size,
            format,
            front_index: 0,
        }
    }

    /// Get the back buffer view for rendering
    pub fn back_buffer_view(&self) -> &wgpu::TextureView {
        if self.front_index == 0 {
            &self.back_view
        } else {
            &self.front_view
        }
    }

    /// Get the front buffer texture for reading/copying
    pub fn front_buffer_texture(&self) -> &wgpu::Texture {
        if self.front_index == 0 {
            &self.front_texture
        } else {
            &self.back_texture
        }
    }

    /// Swap front and back buffers
    pub fn swap(&mut self) {
        self.front_index = 1 - self.front_index;
    }

    /// Get buffer size
    pub fn size(&self) -> (u32, u32) {
        self.size
    }

    /// Resize buffers (recreates textures)
    pub fn resize(&mut self, device: &wgpu::Device, new_size: (u32, u32)) {
        if new_size == self.size {
            return;
        }

        log::info!("Resizing DRM buffers to {}x{}", new_size.0, new_size.1);

        let create_buffer = |label: &str| {
            device.create_texture(&wgpu::TextureDescriptor {
                label: Some(label),
                size: wgpu::Extent3d {
                    width: new_size.0,
                    height: new_size.1,
                    depth_or_array_layers: 1,
                },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: self.format,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT
                    | wgpu::TextureUsages::COPY_SRC
                    | wgpu::TextureUsages::TEXTURE_BINDING,
                view_formats: &[],
            })
        };

        self.front_texture = create_buffer("drm_front_buffer");
        self.back_texture = create_buffer("drm_back_buffer");
        self.front_view = self.front_texture.create_view(&wgpu::TextureViewDescriptor::default());
        self.back_view = self.back_texture.create_view(&wgpu::TextureViewDescriptor::default());
        self.size = new_size;
        self.front_index = 0;
    }
}

/// Copy texture data to a staging buffer for DRM scanout
pub struct DrmStagingBuffer {
    /// Staging buffer for CPU readback
    buffer: wgpu::Buffer,
    /// Buffer size in bytes
    size_bytes: u64,
    /// Bytes per row (with alignment)
    bytes_per_row: u32,
    /// Texture dimensions
    dimensions: (u32, u32),
}

impl DrmStagingBuffer {
    /// Create a new staging buffer
    pub fn new(device: &wgpu::Device, width: u32, height: u32, bytes_per_pixel: u32) -> Self {
        // wgpu requires 256-byte row alignment
        let unpadded_bytes_per_row = width * bytes_per_pixel;
        let align = wgpu::COPY_BYTES_PER_ROW_ALIGNMENT;
        let bytes_per_row = (unpadded_bytes_per_row + align - 1) / align * align;
        let size_bytes = (bytes_per_row * height) as u64;

        let buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("drm_staging_buffer"),
            size: size_bytes,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });

        Self {
            buffer,
            size_bytes,
            bytes_per_row,
            dimensions: (width, height),
        }
    }

    /// Copy texture to staging buffer
    pub fn copy_from_texture(
        &self,
        encoder: &mut wgpu::CommandEncoder,
        texture: &wgpu::Texture,
    ) {
        encoder.copy_texture_to_buffer(
            wgpu::ImageCopyTexture {
                texture,
                mip_level: 0,
                origin: wgpu::Origin3d::ZERO,
                aspect: wgpu::TextureAspect::All,
            },
            wgpu::ImageCopyBuffer {
                buffer: &self.buffer,
                layout: wgpu::ImageDataLayout {
                    offset: 0,
                    bytes_per_row: Some(self.bytes_per_row),
                    rows_per_image: Some(self.dimensions.1),
                },
            },
            wgpu::Extent3d {
                width: self.dimensions.0,
                height: self.dimensions.1,
                depth_or_array_layers: 1,
            },
        );
    }

    /// Map buffer for reading (async)
    pub async fn map_read(&self) -> Result<wgpu::BufferView<'_>, wgpu::BufferAsyncError> {
        let slice = self.buffer.slice(..);
        let (tx, rx) = futures_lite::future::block_on(async {
            let (tx, rx) = async_channel::bounded(1);
            (tx, rx)
        });

        slice.map_async(wgpu::MapMode::Read, move |result| {
            let _ = futures_lite::future::block_on(tx.send(result));
        });

        // Poll the device to process the mapping
        // In a real implementation, this would be integrated with the event loop

        futures_lite::future::block_on(rx.recv())
            .map_err(|_| wgpu::BufferAsyncError)?;

        Ok(slice.get_mapped_range())
    }

    /// Unmap the buffer after reading
    pub fn unmap(&self) {
        self.buffer.unmap();
    }
}

/// Run the renderer using DRM direct rendering
pub fn run_drm() {
    log::info!("Starting DRM renderer...");

    // Find DRM device
    let device_path = match find_drm_device() {
        Some(path) => path,
        None => {
            log::error!("No DRM device found. Available devices:");
            if let Ok(entries) = std::fs::read_dir("/dev/dri") {
                for entry in entries.flatten() {
                    log::error!("  {:?}", entry.path());
                }
            }
            log::error!("Make sure you have GPU drivers and proper permissions.");
            return;
        }
    };

    log::info!("Using DRM device: {}", device_path);

    // Create presenter
    let presenter_id = PresenterId::new(1);
    let mut presenter = match DrmPresenter::new(presenter_id, &device_path) {
        Ok(p) => p,
        Err(e) => {
            log::error!("Failed to create DRM presenter: {}", e);
            return;
        }
    };

    // Initialize GPU
    if let Err(e) = pollster::block_on(presenter.init_gpu()) {
        log::error!("Failed to initialize GPU: {}", e);
        return;
    }

    let device = match presenter.device() {
        Some(d) => d.clone(),
        None => {
            log::error!("No GPU device available");
            return;
        }
    };

    let queue = match presenter.queue() {
        Some(q) => q.clone(),
        None => {
            log::error!("No GPU queue available");
            return;
        }
    };

    // Get surface format
    let format = wgpu::TextureFormat::Bgra8UnormSrgb;

    // Create render buffers for DRM output
    let size = presenter.size();
    let mut render_buffer = DrmRenderBuffer::new(&device, size, format);

    // Create compositor
    let mut compositor = Compositor::new(&device, &queue, format);
    compositor.resize(size.0, size.1);

    // Create runtime
    let mut runtime = Runtime::new();

    // Create input manager for TTY input handling
    let mut input_manager = InputManager::new();

    // Frame tracking
    let mut frame: u64 = 0;
    let mut last_time = Instant::now();
    let target_frame_time = Duration::from_secs_f64(1.0 / 60.0);

    // Shell state
    let mut show_shell = true;
    let mut shell_input = String::new();

    log::info!("DRM renderer initialized. Running at {}x{}", size.0, size.1);
    log::info!("Press Ctrl+C to exit, ` to toggle shell");

    // Install signal handler for graceful shutdown
    let running = Arc::new(std::sync::atomic::AtomicBool::new(true));
    let r = running.clone();

    if let Err(e) = ctrlc::set_handler(move || {
        log::info!("Received Ctrl+C, shutting down...");
        r.store(false, std::sync::atomic::Ordering::SeqCst);
    }) {
        log::warn!("Failed to set Ctrl+C handler: {}", e);
    }

    // Main loop
    while running.load(std::sync::atomic::Ordering::SeqCst) {
        frame += 1;

        // Calculate delta time
        let now = Instant::now();
        let delta = (now - last_time).as_secs_f32();
        last_time = now;

        // Process TTY input (non-blocking)
        if let Some(input_event) = input_manager.poll_tty_input() {
            match input_event {
                TtyInput::Character('`') => {
                    show_shell = !show_shell;
                    log::info!("Shell overlay: {}", if show_shell { "ON" } else { "OFF" });
                }
                TtyInput::Character('\n') if show_shell => {
                    if !shell_input.is_empty() {
                        runtime.execute_command(&shell_input);
                        shell_input.clear();
                    }
                }
                TtyInput::Character('\x7f') if show_shell => {
                    // Backspace
                    shell_input.pop();
                }
                TtyInput::Character(c) if show_shell && c.is_ascii() && !c.is_control() => {
                    shell_input.push(c);
                }
                TtyInput::Escape => {
                    log::info!("Escape pressed, shutting down...");
                    runtime.shutdown();
                }
                _ => {}
            }
        }

        // Update runtime
        runtime.update(delta, frame);

        // Check for shutdown
        if runtime.state() == void_kernel::KernelState::Stopped {
            log::info!("Runtime shutdown requested");
            break;
        }

        // Begin presenter frame
        let presenter_frame = match void_presenter::Presenter::begin_frame(&mut presenter) {
            Ok(f) => f,
            Err(e) => {
                log::warn!("Failed to begin frame: {}", e);
                continue;
            }
        };

        // Render to the back buffer
        let back_view = render_buffer.back_buffer_view();

        // Build shell output
        let mut shell_lines = runtime.status_text();
        if show_shell && !shell_input.is_empty() {
            shell_lines.push(format!("> {}", shell_input));
        }

        // Render frame using compositor
        compositor.render(
            &device,
            &queue,
            back_view,
            runtime.render_graph(),
            frame,
            show_shell,
            shell_lines,
        );

        // Swap buffers
        render_buffer.swap();

        // Present through DRM (page flip)
        if let Err(e) = void_presenter::Presenter::present(&mut presenter, presenter_frame) {
            log::warn!("Failed to present: {}", e);
        }

        // Log status periodically
        if frame % 300 == 1 {
            let state = runtime.state();
            log::info!(
                "Frame {}: State={:?}, Layers={}, FPS={:.1}",
                frame,
                state,
                runtime.render_graph().layers.len(),
                1.0 / delta
            );
        }

        // Frame pacing
        let elapsed = now.elapsed();
        if elapsed < target_frame_time {
            std::thread::sleep(target_frame_time - elapsed);
        }
    }

    log::info!("DRM renderer shutdown complete");
}
