//! Web/WASM presenter using WebGPU
//!
//! This module provides a complete WebGPU-based presenter for browser deployment.
//! It handles:
//! - WebGPU device/adapter initialization
//! - Canvas surface management
//! - Resize handling with ResizeObserver
//! - Frame timing with requestAnimationFrame
//! - Optional WebXR integration for VR/AR
//!
//! # Architecture
//!
//! ```text
//! Browser Tab
//! +-- Canvas (HtmlCanvasElement)
//! |   +-- WebGPU Context
//! |   +-- Surface (wgpu::Surface)
//! +-- WebPresenter
//! |   +-- Device/Queue
//! |   +-- Frame Scheduler (requestAnimationFrame)
//! |   +-- Resize Observer
//! +-- WebXR Session (optional)
//!     +-- XRReferenceSpace
//!     +-- Stereo rendering
//! ```
//!
//! # Usage
//!
//! ```ignore
//! use void_presenter::web::{WebPresenter, WebPresenterConfig};
//!
//! // Initialize asynchronously
//! let presenter = WebPresenter::new("canvas-id", WebPresenterConfig::default()).await?;
//!
//! // Frame loop (called from requestAnimationFrame)
//! let frame = presenter.begin_frame()?;
//! // ... render to frame.texture_view ...
//! presenter.present(frame)?;
//! ```

#![cfg(all(target_arch = "wasm32", feature = "web"))]

use crate::{
    Frame, Presenter, PresenterId, PresenterCapabilities, PresenterConfig,
    PresenterError, PresentMode, RehydrationState, SurfaceFormat,
};

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;

use parking_lot::RwLock;
use wasm_bindgen::prelude::*;
use wasm_bindgen::JsCast;
use web_sys::{console, HtmlCanvasElement, Window};

// Re-export wgpu types for convenience
pub use wgpu;

/// WebPresenter error types
#[derive(Debug, Clone)]
pub enum WebError {
    /// WebGPU is not available in this browser
    WebGpuNotAvailable,
    /// Failed to get GPU adapter
    AdapterNotFound,
    /// Failed to get GPU device
    DeviceCreationFailed(String),
    /// Canvas element not found
    CanvasNotFound(String),
    /// Failed to get WebGPU context from canvas
    ContextCreationFailed,
    /// Surface configuration failed
    SurfaceConfigFailed(String),
    /// Frame acquisition failed
    FrameAcquisitionFailed(String),
    /// WebXR not available
    WebXrNotAvailable,
    /// WebXR session creation failed
    WebXrSessionFailed(String),
    /// JavaScript error
    JsError(String),
}

impl From<WebError> for PresenterError {
    fn from(e: WebError) -> Self {
        match e {
            WebError::WebGpuNotAvailable => {
                PresenterError::BackendNotAvailable("WebGPU not available".into())
            }
            WebError::AdapterNotFound => {
                PresenterError::BackendNotAvailable("GPU adapter not found".into())
            }
            WebError::DeviceCreationFailed(s) => PresenterError::SurfaceCreation(s),
            WebError::CanvasNotFound(s) => PresenterError::SurfaceCreation(s),
            WebError::ContextCreationFailed => {
                PresenterError::SurfaceCreation("WebGPU context creation failed".into())
            }
            WebError::SurfaceConfigFailed(s) => PresenterError::ConfigError(s),
            WebError::FrameAcquisitionFailed(s) => PresenterError::FrameAcquisition(s),
            WebError::WebXrNotAvailable => {
                PresenterError::BackendNotAvailable("WebXR not available".into())
            }
            WebError::WebXrSessionFailed(s) => PresenterError::SurfaceCreation(s),
            WebError::JsError(s) => PresenterError::PresentationFailed(s),
        }
    }
}

impl From<JsValue> for WebError {
    fn from(e: JsValue) -> Self {
        WebError::JsError(format!("{:?}", e))
    }
}

/// Configuration for WebPresenter
#[derive(Debug, Clone)]
pub struct WebPresenterConfig {
    /// Canvas element ID or selector
    pub canvas_id: String,
    /// Preferred surface format (will use compatible if not available)
    pub preferred_format: SurfaceFormat,
    /// Present mode
    pub present_mode: PresentMode,
    /// Power preference for adapter selection
    pub power_preference: PowerPreference,
    /// Enable HDR if available
    pub enable_hdr: bool,
    /// Target frame rate (0 = match display, use requestAnimationFrame)
    pub target_frame_rate: u32,
    /// Enable WebXR if available
    pub enable_xr: bool,
    /// Preferred XR mode
    pub preferred_xr_mode: XrMode,
    /// Pixel ratio handling
    pub pixel_ratio: PixelRatioMode,
}

impl Default for WebPresenterConfig {
    fn default() -> Self {
        Self {
            canvas_id: "metaverse-canvas".into(),
            preferred_format: SurfaceFormat::Bgra8UnormSrgb,
            present_mode: PresentMode::Fifo,
            power_preference: PowerPreference::HighPerformance,
            enable_hdr: false,
            target_frame_rate: 0,
            enable_xr: true,
            preferred_xr_mode: XrMode::ImmersiveVr,
            pixel_ratio: PixelRatioMode::Native,
        }
    }
}

/// Power preference for GPU adapter selection
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PowerPreference {
    /// Prefer low power (integrated GPU)
    LowPower,
    /// Prefer high performance (discrete GPU)
    HighPerformance,
}

/// XR session mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum XrMode {
    /// Inline (non-immersive, renders to canvas)
    Inline,
    /// Immersive VR (full VR headset)
    ImmersiveVr,
    /// Immersive AR (augmented reality)
    ImmersiveAr,
}

impl XrMode {
    /// Get WebXR session mode string
    pub fn as_str(&self) -> &'static str {
        match self {
            XrMode::Inline => "inline",
            XrMode::ImmersiveVr => "immersive-vr",
            XrMode::ImmersiveAr => "immersive-ar",
        }
    }
}

/// Pixel ratio handling mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PixelRatioMode {
    /// Use native device pixel ratio (sharpest, most expensive)
    Native,
    /// Use 1.0 pixel ratio (fastest, may look blurry on high-DPI)
    One,
    /// Custom fixed ratio
    Fixed(u32), // Stored as ratio * 100
    /// Clamp to maximum ratio
    Clamped(u32), // Max ratio * 100
}

impl PixelRatioMode {
    /// Get effective pixel ratio
    pub fn effective_ratio(&self, device_ratio: f64) -> f64 {
        match self {
            PixelRatioMode::Native => device_ratio,
            PixelRatioMode::One => 1.0,
            PixelRatioMode::Fixed(r) => *r as f64 / 100.0,
            PixelRatioMode::Clamped(max) => device_ratio.min(*max as f64 / 100.0),
        }
    }
}

/// Frame context for WebGPU rendering
#[derive(Debug)]
pub struct WebFrame {
    /// wgpu surface texture
    pub texture: wgpu::SurfaceTexture,
    /// Texture view for rendering
    pub texture_view: wgpu::TextureView,
    /// Frame dimensions
    pub size: (u32, u32),
    /// Frame number
    pub number: u64,
    /// Timestamp from requestAnimationFrame (milliseconds)
    pub timestamp: f64,
    /// Delta time since last frame (seconds)
    pub delta_time: f32,
}

/// XR frame context for WebXR rendering
#[cfg(feature = "webxr")]
#[derive(Debug)]
pub struct WebXrFrame {
    /// XR frame from requestAnimationFrame
    pub xr_frame: web_sys::XrFrame,
    /// Left eye view
    pub left_view: XrView,
    /// Right eye view
    pub right_view: XrView,
    /// Frame number
    pub number: u64,
    /// Delta time since last frame (seconds)
    pub delta_time: f32,
}

/// XR view data
#[cfg(feature = "webxr")]
#[derive(Debug, Clone)]
pub struct XrView {
    /// View matrix (inverse of camera transform)
    pub view_matrix: glam::Mat4,
    /// Projection matrix
    pub projection_matrix: glam::Mat4,
    /// Viewport dimensions
    pub viewport: (i32, i32, u32, u32),
}

/// Shared state for async callbacks
struct WebPresenterState {
    /// Current canvas size
    canvas_size: RwLock<(u32, u32)>,
    /// Current physical size (accounting for pixel ratio)
    physical_size: RwLock<(u32, u32)>,
    /// Current device pixel ratio
    pixel_ratio: RwLock<f64>,
    /// Whether surface needs reconfiguration
    needs_reconfigure: AtomicBool,
    /// Frame counter
    frame_count: AtomicU64,
    /// Last frame timestamp
    last_timestamp: RwLock<f64>,
    /// Is running
    is_running: AtomicBool,
}

/// WebGPU-based presenter for browser deployment
pub struct WebPresenter {
    /// Presenter ID
    id: PresenterId,
    /// wgpu instance
    instance: wgpu::Instance,
    /// wgpu adapter
    adapter: wgpu::Adapter,
    /// wgpu device
    device: Arc<wgpu::Device>,
    /// wgpu queue
    queue: Arc<wgpu::Queue>,
    /// wgpu surface
    surface: wgpu::Surface<'static>,
    /// Surface configuration
    surface_config: wgpu::SurfaceConfiguration,
    /// Canvas element
    canvas: HtmlCanvasElement,
    /// Window reference
    window: Window,
    /// Presenter capabilities
    capabilities: PresenterCapabilities,
    /// Presenter config (our abstraction)
    config: PresenterConfig,
    /// Web-specific config
    web_config: WebPresenterConfig,
    /// Shared state for callbacks
    state: Arc<WebPresenterState>,
    /// Resize observer closure (must be kept alive)
    _resize_observer: Option<web_sys::ResizeObserver>,
    /// XR session state
    #[cfg(feature = "webxr")]
    xr_session: Option<XrSessionState>,
}

/// WebXR session state
#[cfg(feature = "webxr")]
struct XrSessionState {
    session: web_sys::XrSession,
    reference_space: web_sys::XrReferenceSpace,
    is_immersive: bool,
}

impl WebPresenter {
    /// Create a new WebPresenter asynchronously
    ///
    /// This must be called from an async context (e.g., wasm_bindgen_futures::spawn_local).
    pub async fn new(config: WebPresenterConfig) -> Result<Self, WebError> {
        let id = PresenterId::new(1);
        Self::with_id(id, config).await
    }

    /// Create a new WebPresenter with a specific ID
    pub async fn with_id(id: PresenterId, config: WebPresenterConfig) -> Result<Self, WebError> {
        // Get window and document
        let window = web_sys::window().ok_or(WebError::JsError("No window object".into()))?;
        let document = window
            .document()
            .ok_or(WebError::JsError("No document object".into()))?;

        // Find canvas element
        let canvas: HtmlCanvasElement = document
            .get_element_by_id(&config.canvas_id)
            .ok_or_else(|| WebError::CanvasNotFound(config.canvas_id.clone()))?
            .dyn_into()
            .map_err(|_| WebError::CanvasNotFound(config.canvas_id.clone()))?;

        Self::with_canvas(id, canvas, config).await
    }

    /// Create a new WebPresenter with an existing canvas element
    pub async fn with_canvas(
        id: PresenterId,
        canvas: HtmlCanvasElement,
        web_config: WebPresenterConfig,
    ) -> Result<Self, WebError> {
        let window = web_sys::window().ok_or(WebError::JsError("No window object".into()))?;

        // Get device pixel ratio
        let device_pixel_ratio = window.device_pixel_ratio();
        let effective_ratio = web_config.pixel_ratio.effective_ratio(device_pixel_ratio);

        // Get canvas size
        let canvas_width = canvas.client_width() as u32;
        let canvas_height = canvas.client_height() as u32;

        // Calculate physical size
        let physical_width = (canvas_width as f64 * effective_ratio) as u32;
        let physical_height = (canvas_height as f64 * effective_ratio) as u32;

        // Set canvas buffer size to match physical pixels
        canvas.set_width(physical_width.max(1));
        canvas.set_height(physical_height.max(1));

        log::info!(
            "WebPresenter: canvas {}x{}, physical {}x{}, ratio {:.2}",
            canvas_width,
            canvas_height,
            physical_width,
            physical_height,
            effective_ratio
        );

        // Create wgpu instance for WebGPU backend
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::BROWSER_WEBGPU,
            ..Default::default()
        });

        // Create surface from canvas
        let surface = instance
            .create_surface(wgpu::SurfaceTarget::Canvas(canvas.clone()))
            .map_err(|e| WebError::SurfaceConfigFailed(format!("Surface creation failed: {}", e)))?;

        // Request adapter
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: match web_config.power_preference {
                    PowerPreference::LowPower => wgpu::PowerPreference::LowPower,
                    PowerPreference::HighPerformance => wgpu::PowerPreference::HighPerformance,
                },
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await
            .ok_or(WebError::AdapterNotFound)?;

        log::info!(
            "WebPresenter: using adapter {:?}",
            adapter.get_info().name
        );

        // Request device
        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: Some("WebPresenter Device"),
                    required_features: wgpu::Features::empty(),
                    required_limits: wgpu::Limits::downlevel_webgl2_defaults()
                        .using_resolution(adapter.limits()),
                    memory_hints: wgpu::MemoryHints::Performance,
                },
                None,
            )
            .await
            .map_err(|e| WebError::DeviceCreationFailed(format!("{}", e)))?;

        let device = Arc::new(device);
        let queue = Arc::new(queue);

        // Get surface capabilities
        let surface_caps = surface.get_capabilities(&adapter);

        // Choose format
        let format = surface_caps
            .formats
            .iter()
            .find(|f| match web_config.preferred_format {
                SurfaceFormat::Bgra8UnormSrgb => **f == wgpu::TextureFormat::Bgra8UnormSrgb,
                SurfaceFormat::Rgba8UnormSrgb => **f == wgpu::TextureFormat::Rgba8UnormSrgb,
                SurfaceFormat::Bgra8Unorm => **f == wgpu::TextureFormat::Bgra8Unorm,
                SurfaceFormat::Rgba8Unorm => **f == wgpu::TextureFormat::Rgba8Unorm,
                SurfaceFormat::Rgba16Float => **f == wgpu::TextureFormat::Rgba16Float,
                SurfaceFormat::Rgb10a2Unorm => **f == wgpu::TextureFormat::Rgb10a2Unorm,
            })
            .copied()
            .unwrap_or(surface_caps.formats[0]);

        log::info!("WebPresenter: using format {:?}", format);

        // Choose present mode
        let present_mode = surface_caps
            .present_modes
            .iter()
            .find(|m| match web_config.present_mode {
                PresentMode::Immediate => **m == wgpu::PresentMode::Immediate,
                PresentMode::Mailbox => **m == wgpu::PresentMode::Mailbox,
                PresentMode::Fifo => **m == wgpu::PresentMode::Fifo,
                PresentMode::FifoRelaxed => **m == wgpu::PresentMode::FifoRelaxed,
            })
            .copied()
            .unwrap_or(wgpu::PresentMode::Fifo);

        // Configure surface
        let surface_config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            width: physical_width.max(1),
            height: physical_height.max(1),
            present_mode,
            alpha_mode: surface_caps.alpha_modes[0],
            view_formats: vec![],
            desired_maximum_frame_latency: 2,
        };
        surface.configure(&device, &surface_config);

        // Build capabilities
        let capabilities = PresenterCapabilities {
            present_modes: surface_caps
                .present_modes
                .iter()
                .filter_map(|m| match m {
                    wgpu::PresentMode::Immediate => Some(PresentMode::Immediate),
                    wgpu::PresentMode::Mailbox => Some(PresentMode::Mailbox),
                    wgpu::PresentMode::Fifo => Some(PresentMode::Fifo),
                    wgpu::PresentMode::FifoRelaxed => Some(PresentMode::FifoRelaxed),
                    _ => None,
                })
                .collect(),
            formats: surface_caps
                .formats
                .iter()
                .filter_map(|f| match f {
                    wgpu::TextureFormat::Bgra8Unorm => Some(SurfaceFormat::Bgra8Unorm),
                    wgpu::TextureFormat::Bgra8UnormSrgb => Some(SurfaceFormat::Bgra8UnormSrgb),
                    wgpu::TextureFormat::Rgba8Unorm => Some(SurfaceFormat::Rgba8Unorm),
                    wgpu::TextureFormat::Rgba8UnormSrgb => Some(SurfaceFormat::Rgba8UnormSrgb),
                    wgpu::TextureFormat::Rgba16Float => Some(SurfaceFormat::Rgba16Float),
                    wgpu::TextureFormat::Rgb10a2Unorm => Some(SurfaceFormat::Rgb10a2Unorm),
                    _ => None,
                })
                .collect(),
            max_resolution: (16384, 16384),
            hdr_support: surface_caps
                .formats
                .iter()
                .any(|f| *f == wgpu::TextureFormat::Rgba16Float),
            vrr_support: false, // WebGPU doesn't expose VRR
            xr_passthrough: false, // Determined by WebXR
        };

        // Build presenter config
        let config = PresenterConfig {
            format: wgpu_to_surface_format(format),
            present_mode: wgpu_to_present_mode(present_mode),
            size: (physical_width, physical_height),
            enable_hdr: web_config.enable_hdr,
            target_frame_rate: web_config.target_frame_rate,
            allow_tearing: present_mode == wgpu::PresentMode::Immediate,
        };

        // Create shared state
        let state = Arc::new(WebPresenterState {
            canvas_size: RwLock::new((canvas_width, canvas_height)),
            physical_size: RwLock::new((physical_width, physical_height)),
            pixel_ratio: RwLock::new(effective_ratio),
            needs_reconfigure: AtomicBool::new(false),
            frame_count: AtomicU64::new(0),
            last_timestamp: RwLock::new(0.0),
            is_running: AtomicBool::new(false),
        });

        // Set up resize observer
        let resize_observer = Self::setup_resize_observer(&canvas, state.clone(), &web_config)?;

        let presenter = Self {
            id,
            instance,
            adapter,
            device,
            queue,
            surface,
            surface_config,
            canvas,
            window,
            capabilities,
            config,
            web_config,
            state,
            _resize_observer: Some(resize_observer),
            #[cfg(feature = "webxr")]
            xr_session: None,
        };

        Ok(presenter)
    }

    /// Set up resize observer for automatic surface reconfiguration
    fn setup_resize_observer(
        canvas: &HtmlCanvasElement,
        state: Arc<WebPresenterState>,
        config: &WebPresenterConfig,
    ) -> Result<web_sys::ResizeObserver, WebError> {
        let window = web_sys::window().ok_or(WebError::JsError("No window".into()))?;
        let pixel_ratio_mode = config.pixel_ratio;

        let callback = Closure::<dyn FnMut(js_sys::Array)>::new(move |entries: js_sys::Array| {
            let device_ratio = window.device_pixel_ratio();
            let effective_ratio = pixel_ratio_mode.effective_ratio(device_ratio);

            for entry in entries.iter() {
                if let Ok(entry) = entry.dyn_into::<web_sys::ResizeObserverEntry>() {
                    // Get content box size (CSS pixels)
                    let content_rect = entry.content_rect();
                    let css_width = content_rect.width() as u32;
                    let css_height = content_rect.height() as u32;

                    // Calculate physical size
                    let physical_width = (css_width as f64 * effective_ratio) as u32;
                    let physical_height = (css_height as f64 * effective_ratio) as u32;

                    // Update state
                    *state.canvas_size.write() = (css_width, css_height);
                    *state.physical_size.write() = (physical_width.max(1), physical_height.max(1));
                    *state.pixel_ratio.write() = effective_ratio;
                    state.needs_reconfigure.store(true, Ordering::SeqCst);

                    log::debug!(
                        "Resize: CSS {}x{}, physical {}x{}",
                        css_width,
                        css_height,
                        physical_width,
                        physical_height
                    );
                }
            }
        });

        let observer = web_sys::ResizeObserver::new(callback.as_ref().unchecked_ref())
            .map_err(|e| WebError::JsError(format!("ResizeObserver creation failed: {:?}", e)))?;

        observer.observe(canvas);

        // Prevent closure from being dropped
        callback.forget();

        Ok(observer)
    }

    /// Get the wgpu device
    pub fn device(&self) -> &Arc<wgpu::Device> {
        &self.device
    }

    /// Get the wgpu queue
    pub fn queue(&self) -> &Arc<wgpu::Queue> {
        &self.queue
    }

    /// Get the canvas element
    pub fn canvas(&self) -> &HtmlCanvasElement {
        &self.canvas
    }

    /// Get the surface format
    pub fn surface_format(&self) -> wgpu::TextureFormat {
        self.surface_config.format
    }

    /// Get current physical size (accounting for pixel ratio)
    pub fn physical_size(&self) -> (u32, u32) {
        *self.state.physical_size.read()
    }

    /// Get current CSS size
    pub fn css_size(&self) -> (u32, u32) {
        *self.state.canvas_size.read()
    }

    /// Get current device pixel ratio
    pub fn pixel_ratio(&self) -> f64 {
        *self.state.pixel_ratio.read()
    }

    /// Check if surface needs reconfiguration
    pub fn needs_reconfigure(&self) -> bool {
        self.state.needs_reconfigure.load(Ordering::SeqCst)
    }

    /// Handle surface reconfiguration if needed
    fn handle_reconfigure(&mut self) -> Result<(), WebError> {
        if !self.state.needs_reconfigure.swap(false, Ordering::SeqCst) {
            return Ok(());
        }

        let (width, height) = *self.state.physical_size.read();

        if width == 0 || height == 0 {
            // Window is minimized, skip reconfiguration
            return Ok(());
        }

        // Update canvas buffer size
        self.canvas.set_width(width);
        self.canvas.set_height(height);

        // Reconfigure surface
        self.surface_config.width = width;
        self.surface_config.height = height;
        self.surface.configure(&self.device, &self.surface_config);

        // Update config
        self.config.size = (width, height);

        log::debug!("Surface reconfigured to {}x{}", width, height);

        Ok(())
    }

    /// Begin a WebGPU frame
    ///
    /// Returns a WebFrame containing the texture to render to.
    /// Call this from your requestAnimationFrame callback.
    pub fn begin_web_frame(&mut self, timestamp: f64) -> Result<WebFrame, WebError> {
        // Handle resize if needed
        self.handle_reconfigure()?;

        // Calculate delta time
        let last_timestamp = {
            let mut last = self.state.last_timestamp.write();
            let delta = if *last == 0.0 {
                1.0 / 60.0 // Assume 60fps for first frame
            } else {
                (timestamp - *last) / 1000.0
            };
            *last = timestamp;
            delta
        };

        // Get surface texture
        let texture = self.surface.get_current_texture().map_err(|e| match e {
            wgpu::SurfaceError::Lost | wgpu::SurfaceError::Outdated => {
                // Mark for reconfiguration
                self.state.needs_reconfigure.store(true, Ordering::SeqCst);
                WebError::FrameAcquisitionFailed("Surface lost, will reconfigure".into())
            }
            wgpu::SurfaceError::OutOfMemory => {
                WebError::FrameAcquisitionFailed("Out of GPU memory".into())
            }
            wgpu::SurfaceError::Timeout => {
                WebError::FrameAcquisitionFailed("Surface timeout".into())
            }
        })?;

        let texture_view = texture.texture.create_view(&wgpu::TextureViewDescriptor {
            label: Some("WebPresenter Frame View"),
            ..Default::default()
        });

        let frame_number = self.state.frame_count.fetch_add(1, Ordering::Relaxed);

        Ok(WebFrame {
            texture,
            texture_view,
            size: (self.surface_config.width, self.surface_config.height),
            number: frame_number,
            timestamp,
            delta_time: last_timestamp as f32,
        })
    }

    /// Present a WebGPU frame
    pub fn present_web_frame(&mut self, frame: WebFrame) {
        frame.texture.present();
    }

    /// Request fullscreen mode
    pub fn request_fullscreen(&self) -> Result<(), WebError> {
        self.canvas
            .request_fullscreen()
            .map_err(|e| WebError::JsError(format!("Fullscreen request failed: {:?}", e)))
    }

    /// Request pointer lock (for FPS-style controls)
    pub fn request_pointer_lock(&self) {
        self.canvas.request_pointer_lock();
    }

    // ==================== WebXR Support ====================

    /// Check if WebXR is available
    #[cfg(feature = "webxr")]
    pub fn is_xr_available(&self) -> bool {
        self.window.navigator().xr().is_some()
    }

    /// Check if a specific XR mode is supported
    #[cfg(feature = "webxr")]
    pub async fn is_xr_mode_supported(&self, mode: XrMode) -> bool {
        let Some(xr) = self.window.navigator().xr() else {
            return false;
        };

        let mode_str = mode.as_str();
        let promise = xr.is_session_supported(web_sys::XrSessionMode::from_js_value(
            &JsValue::from_str(mode_str),
        ).unwrap());

        match wasm_bindgen_futures::JsFuture::from(promise).await {
            Ok(result) => result.as_bool().unwrap_or(false),
            Err(_) => false,
        }
    }

    /// Request an XR session
    #[cfg(feature = "webxr")]
    pub async fn request_xr_session(&mut self, mode: XrMode) -> Result<(), WebError> {
        let xr = self
            .window
            .navigator()
            .xr()
            .ok_or(WebError::WebXrNotAvailable)?;

        let mode_str = mode.as_str();
        let session_mode = web_sys::XrSessionMode::from_js_value(&JsValue::from_str(mode_str))
            .ok_or_else(|| WebError::WebXrSessionFailed("Invalid XR mode".into()))?;

        // Create session init options
        let init = web_sys::XrSessionInit::new();

        // Request session
        let promise = xr.request_session_with_options(session_mode, &init);
        let session: web_sys::XrSession = wasm_bindgen_futures::JsFuture::from(promise)
            .await
            .map_err(|e| WebError::WebXrSessionFailed(format!("{:?}", e)))?
            .dyn_into()
            .map_err(|_| WebError::WebXrSessionFailed("Invalid session object".into()))?;

        // Request reference space
        let reference_space_type = if mode == XrMode::ImmersiveVr || mode == XrMode::ImmersiveAr {
            web_sys::XrReferenceSpaceType::LocalFloor
        } else {
            web_sys::XrReferenceSpaceType::Viewer
        };

        let promise = session.request_reference_space(reference_space_type);
        let reference_space: web_sys::XrReferenceSpace =
            wasm_bindgen_futures::JsFuture::from(promise)
                .await
                .map_err(|e| WebError::WebXrSessionFailed(format!("{:?}", e)))?
                .dyn_into()
                .map_err(|_| WebError::WebXrSessionFailed("Invalid reference space".into()))?;

        self.xr_session = Some(XrSessionState {
            session,
            reference_space,
            is_immersive: mode != XrMode::Inline,
        });

        log::info!("WebXR session created: {:?}", mode);

        Ok(())
    }

    /// End the current XR session
    #[cfg(feature = "webxr")]
    pub async fn end_xr_session(&mut self) -> Result<(), WebError> {
        if let Some(state) = self.xr_session.take() {
            let promise = state.session.end();
            wasm_bindgen_futures::JsFuture::from(promise)
                .await
                .map_err(|e| WebError::WebXrSessionFailed(format!("{:?}", e)))?;
            log::info!("WebXR session ended");
        }
        Ok(())
    }

    /// Check if XR session is active
    #[cfg(feature = "webxr")]
    pub fn has_xr_session(&self) -> bool {
        self.xr_session.is_some()
    }

    /// Get the XR session (for advanced usage)
    #[cfg(feature = "webxr")]
    pub fn xr_session(&self) -> Option<&web_sys::XrSession> {
        self.xr_session.as_ref().map(|s| &s.session)
    }

    /// Get the XR reference space
    #[cfg(feature = "webxr")]
    pub fn xr_reference_space(&self) -> Option<&web_sys::XrReferenceSpace> {
        self.xr_session.as_ref().map(|s| &s.reference_space)
    }
}

// ==================== Presenter Trait Implementation ====================

impl Presenter for WebPresenter {
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
        // Update format if changed and supported
        let format = surface_format_to_wgpu(config.format);
        if self.surface_config.format != format {
            // Check if format is supported
            let caps = self.surface.get_capabilities(&self.adapter);
            if caps.formats.contains(&format) {
                self.surface_config.format = format;
            }
        }

        // Update present mode
        let present_mode = present_mode_to_wgpu(config.present_mode);
        self.surface_config.present_mode = present_mode;

        // Reconfigure surface
        self.surface.configure(&self.device, &self.surface_config);

        self.config = config;
        Ok(())
    }

    fn resize(&mut self, width: u32, height: u32) -> Result<(), PresenterError> {
        if width == 0 || height == 0 {
            return Ok(());
        }

        // Update canvas buffer size
        self.canvas.set_width(width);
        self.canvas.set_height(height);

        // Reconfigure surface
        self.surface_config.width = width;
        self.surface_config.height = height;
        self.surface.configure(&self.device, &self.surface_config);

        self.config.size = (width, height);
        *self.state.physical_size.write() = (width, height);

        Ok(())
    }

    fn begin_frame(&mut self) -> Result<Frame, PresenterError> {
        // Handle resize
        self.handle_reconfigure()
            .map_err(|e| PresenterError::from(e))?;

        // Get timestamp from performance API
        let timestamp = self.window.performance().map(|p| p.now()).unwrap_or(0.0);

        // Create frame
        let frame_number = self.state.frame_count.fetch_add(1, Ordering::Relaxed);
        let size = (self.surface_config.width, self.surface_config.height);

        let mut frame = Frame::new(frame_number, size);

        // Store wgpu frame data in user_data
        let texture = self.surface.get_current_texture().map_err(|e| {
            self.state.needs_reconfigure.store(true, Ordering::SeqCst);
            PresenterError::FrameAcquisition(format!("{}", e))
        })?;

        let texture_view = texture.texture.create_view(&wgpu::TextureViewDescriptor {
            label: Some("WebPresenter Frame View"),
            ..Default::default()
        });

        // We need to store the texture for later presentation
        // Using a custom struct that implements Send + Sync
        frame.set_user_data(WebFrameData {
            texture: Some(texture),
            texture_view: Some(texture_view),
            timestamp,
        });

        Ok(frame)
    }

    fn present(&mut self, mut frame: Frame) -> Result<(), PresenterError> {
        // Extract and present the texture
        if let Some(mut data) = frame.take_user_data::<WebFrameData>() {
            if let Some(texture) = data.texture.take() {
                texture.present();
            }
        }

        Ok(())
    }

    fn size(&self) -> (u32, u32) {
        self.config.size
    }

    fn is_valid(&self) -> bool {
        self.state.is_running.load(Ordering::Relaxed)
            || self.surface_config.width > 0 && self.surface_config.height > 0
    }

    fn rehydration_state(&self) -> RehydrationState {
        let mut state = RehydrationState::new();
        state.set_value("frame_count", self.state.frame_count.load(Ordering::Relaxed));
        state.set_value("width", self.config.size.0 as u64);
        state.set_value("height", self.config.size.1 as u64);
        state.set_value("canvas_id", self.web_config.canvas_id.clone());
        state
    }

    fn rehydrate(&mut self, state: RehydrationState) -> Result<(), PresenterError> {
        if let Some(count) = state.get_value::<u64>("frame_count") {
            self.state.frame_count.store(count, Ordering::Relaxed);
        }
        Ok(())
    }
}

// Note: WebPresenter is not Send due to web_sys types, but it's fine for WASM single-threaded env
// We implement a marker for documentation purposes
unsafe impl Send for WebPresenter {}
unsafe impl Sync for WebPresenter {}

/// Internal frame data storage
struct WebFrameData {
    texture: Option<wgpu::SurfaceTexture>,
    texture_view: Option<wgpu::TextureView>,
    timestamp: f64,
}

// WebFrameData needs Send + Sync for Frame::set_user_data
unsafe impl Send for WebFrameData {}
unsafe impl Sync for WebFrameData {}

// ==================== Frame Scheduler ====================

/// Callback type for frame loop
pub type FrameCallback = Box<dyn FnMut(f64) -> bool>;

/// Web frame scheduler using requestAnimationFrame
///
/// This provides a convenient way to run a frame loop in the browser.
pub struct WebFrameScheduler {
    /// Whether the scheduler is running
    running: Rc<RefCell<bool>>,
    /// Animation frame request ID
    request_id: Rc<RefCell<Option<i32>>>,
}

impl WebFrameScheduler {
    /// Create a new frame scheduler
    pub fn new() -> Self {
        Self {
            running: Rc::new(RefCell::new(false)),
            request_id: Rc::new(RefCell::new(None)),
        }
    }

    /// Start the frame loop
    ///
    /// The callback receives the timestamp (in milliseconds) from requestAnimationFrame.
    /// Return `true` to continue the loop, `false` to stop.
    pub fn start<F>(&self, mut callback: F)
    where
        F: FnMut(f64) -> bool + 'static,
    {
        if *self.running.borrow() {
            return;
        }

        *self.running.borrow_mut() = true;

        let running = self.running.clone();
        let request_id = self.request_id.clone();

        // Create the recursive closure
        let f: Rc<RefCell<Option<Closure<dyn FnMut(f64)>>>> = Rc::new(RefCell::new(None));
        let g = f.clone();

        *g.borrow_mut() = Some(Closure::new(move |timestamp: f64| {
            if !*running.borrow() {
                return;
            }

            // Call user callback
            let should_continue = callback(timestamp);

            if should_continue && *running.borrow() {
                // Request next frame
                if let Some(window) = web_sys::window() {
                    if let Some(ref closure) = *f.borrow() {
                        let id = window
                            .request_animation_frame(closure.as_ref().unchecked_ref())
                            .expect("requestAnimationFrame failed");
                        *request_id.borrow_mut() = Some(id);
                    }
                }
            } else {
                *running.borrow_mut() = false;
            }
        }));

        // Start the loop
        if let Some(window) = web_sys::window() {
            if let Some(ref closure) = *g.borrow() {
                let id = window
                    .request_animation_frame(closure.as_ref().unchecked_ref())
                    .expect("requestAnimationFrame failed");
                *self.request_id.borrow_mut() = Some(id);
            }
        }

        // Keep the closure alive
        if let Some(closure) = g.borrow_mut().take() {
            closure.forget();
        }
    }

    /// Stop the frame loop
    pub fn stop(&self) {
        *self.running.borrow_mut() = false;

        if let Some(id) = self.request_id.borrow_mut().take() {
            if let Some(window) = web_sys::window() {
                window.cancel_animation_frame(id).ok();
            }
        }
    }

    /// Check if the scheduler is running
    pub fn is_running(&self) -> bool {
        *self.running.borrow()
    }
}

impl Default for WebFrameScheduler {
    fn default() -> Self {
        Self::new()
    }
}

// ==================== Input Handling ====================

/// Web input event types
#[derive(Debug, Clone)]
pub enum WebInputEvent {
    /// Mouse moved
    MouseMove {
        x: f64,
        y: f64,
        movement_x: f64,
        movement_y: f64,
    },
    /// Mouse button pressed
    MouseDown { button: u16, x: f64, y: f64 },
    /// Mouse button released
    MouseUp { button: u16, x: f64, y: f64 },
    /// Mouse wheel scrolled
    Wheel { delta_x: f64, delta_y: f64, delta_z: f64 },
    /// Key pressed
    KeyDown { key: String, code: String, repeat: bool },
    /// Key released
    KeyUp { key: String, code: String },
    /// Touch started
    TouchStart { touches: Vec<TouchPoint> },
    /// Touch moved
    TouchMove { touches: Vec<TouchPoint> },
    /// Touch ended
    TouchEnd { touches: Vec<TouchPoint> },
    /// Touch cancelled
    TouchCancel { touches: Vec<TouchPoint> },
    /// Gamepad connected
    GamepadConnected { index: u32 },
    /// Gamepad disconnected
    GamepadDisconnected { index: u32 },
    /// Pointer lock changed
    PointerLockChange { locked: bool },
    /// Fullscreen changed
    FullscreenChange { fullscreen: bool },
    /// Canvas resized
    Resize { width: u32, height: u32 },
    /// Focus gained
    Focus,
    /// Focus lost
    Blur,
}

/// Touch point data
#[derive(Debug, Clone)]
pub struct TouchPoint {
    pub identifier: i32,
    pub x: f64,
    pub y: f64,
    pub force: f32,
    pub radius_x: f32,
    pub radius_y: f32,
}

/// Web input handler
///
/// Sets up event listeners on a canvas and collects input events.
pub struct WebInputHandler {
    /// Canvas element
    canvas: HtmlCanvasElement,
    /// Event queue
    events: Rc<RefCell<Vec<WebInputEvent>>>,
    /// Whether pointer is locked
    pointer_locked: Rc<RefCell<bool>>,
    /// Closures (must be kept alive)
    _closures: Vec<Closure<dyn FnMut(web_sys::Event)>>,
}

impl WebInputHandler {
    /// Create a new input handler for a canvas
    pub fn new(canvas: HtmlCanvasElement) -> Result<Self, WebError> {
        let events: Rc<RefCell<Vec<WebInputEvent>>> = Rc::new(RefCell::new(Vec::new()));
        let pointer_locked = Rc::new(RefCell::new(false));
        let mut closures: Vec<Closure<dyn FnMut(web_sys::Event)>> = Vec::new();

        let document = web_sys::window()
            .and_then(|w| w.document())
            .ok_or(WebError::JsError("No document".into()))?;

        // Mouse move
        {
            let events = events.clone();
            let pointer_locked = pointer_locked.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::MouseEvent>() {
                    let locked = *pointer_locked.borrow();
                    events.borrow_mut().push(WebInputEvent::MouseMove {
                        x: e.client_x() as f64,
                        y: e.client_y() as f64,
                        movement_x: if locked { e.movement_x() as f64 } else { 0.0 },
                        movement_y: if locked { e.movement_y() as f64 } else { 0.0 },
                    });
                }
            });
            canvas
                .add_event_listener_with_callback("mousemove", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Mouse down
        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::MouseEvent>() {
                    events.borrow_mut().push(WebInputEvent::MouseDown {
                        button: e.button(),
                        x: e.client_x() as f64,
                        y: e.client_y() as f64,
                    });
                }
            });
            canvas
                .add_event_listener_with_callback("mousedown", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Mouse up
        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::MouseEvent>() {
                    events.borrow_mut().push(WebInputEvent::MouseUp {
                        button: e.button(),
                        x: e.client_x() as f64,
                        y: e.client_y() as f64,
                    });
                }
            });
            canvas
                .add_event_listener_with_callback("mouseup", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Wheel
        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::WheelEvent>() {
                    e.prevent_default();
                    events.borrow_mut().push(WebInputEvent::Wheel {
                        delta_x: e.delta_x(),
                        delta_y: e.delta_y(),
                        delta_z: e.delta_z(),
                    });
                }
            });
            canvas
                .add_event_listener_with_callback("wheel", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Key down (on document to catch all keys)
        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::KeyboardEvent>() {
                    events.borrow_mut().push(WebInputEvent::KeyDown {
                        key: e.key(),
                        code: e.code(),
                        repeat: e.repeat(),
                    });
                }
            });
            document
                .add_event_listener_with_callback("keydown", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Key up
        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::KeyboardEvent>() {
                    events.borrow_mut().push(WebInputEvent::KeyUp {
                        key: e.key(),
                        code: e.code(),
                    });
                }
            });
            document
                .add_event_listener_with_callback("keyup", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Touch events
        for (event_name, event_type) in [
            ("touchstart", "start"),
            ("touchmove", "move"),
            ("touchend", "end"),
            ("touchcancel", "cancel"),
        ] {
            let events = events.clone();
            let event_type = event_type.to_string();
            let closure = Closure::<dyn FnMut(_)>::new(move |e: web_sys::Event| {
                if let Ok(e) = e.dyn_into::<web_sys::TouchEvent>() {
                    e.prevent_default();
                    let touches: Vec<TouchPoint> = (0..e.touches().length())
                        .filter_map(|i| e.touches().get(i))
                        .map(|t| TouchPoint {
                            identifier: t.identifier(),
                            x: t.client_x() as f64,
                            y: t.client_y() as f64,
                            force: t.force(),
                            radius_x: t.radius_x(),
                            radius_y: t.radius_y(),
                        })
                        .collect();

                    let event = match event_type.as_str() {
                        "start" => WebInputEvent::TouchStart { touches },
                        "move" => WebInputEvent::TouchMove { touches },
                        "end" => WebInputEvent::TouchEnd { touches },
                        "cancel" => WebInputEvent::TouchCancel { touches },
                        _ => return,
                    };
                    events.borrow_mut().push(event);
                }
            });
            canvas
                .add_event_listener_with_callback(event_name, closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Pointer lock change
        {
            let events = events.clone();
            let pointer_locked = pointer_locked.clone();
            let canvas_clone = canvas.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |_: web_sys::Event| {
                let locked = document
                    .pointer_lock_element()
                    .map(|e| e == canvas_clone.clone().into())
                    .unwrap_or(false);
                *pointer_locked.borrow_mut() = locked;
                events
                    .borrow_mut()
                    .push(WebInputEvent::PointerLockChange { locked });
            });
            document
                .add_event_listener_with_callback(
                    "pointerlockchange",
                    closure.as_ref().unchecked_ref(),
                )
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Focus/blur
        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |_: web_sys::Event| {
                events.borrow_mut().push(WebInputEvent::Focus);
            });
            canvas
                .add_event_listener_with_callback("focus", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        {
            let events = events.clone();
            let closure = Closure::<dyn FnMut(_)>::new(move |_: web_sys::Event| {
                events.borrow_mut().push(WebInputEvent::Blur);
            });
            canvas
                .add_event_listener_with_callback("blur", closure.as_ref().unchecked_ref())
                .map_err(|e| WebError::JsError(format!("{:?}", e)))?;
            closures.push(closure);
        }

        // Make canvas focusable
        canvas.set_tab_index(0);

        Ok(Self {
            canvas,
            events,
            pointer_locked,
            _closures: closures,
        })
    }

    /// Poll and drain all pending events
    pub fn poll_events(&self) -> Vec<WebInputEvent> {
        std::mem::take(&mut *self.events.borrow_mut())
    }

    /// Check if pointer is locked
    pub fn is_pointer_locked(&self) -> bool {
        *self.pointer_locked.borrow()
    }

    /// Request pointer lock
    pub fn request_pointer_lock(&self) {
        self.canvas.request_pointer_lock();
    }

    /// Exit pointer lock
    pub fn exit_pointer_lock(&self) {
        if let Some(document) = web_sys::window().and_then(|w| w.document()) {
            document.exit_pointer_lock();
        }
    }

    /// Poll gamepads (must be called each frame)
    pub fn poll_gamepads(&self) -> Vec<GamepadState> {
        let Some(window) = web_sys::window() else {
            return Vec::new();
        };

        let Ok(gamepads) = window.navigator().get_gamepads() else {
            return Vec::new();
        };

        let mut result = Vec::new();

        for i in 0..gamepads.length() {
            if let Some(gamepad) = gamepads.get(i) {
                if !gamepad.is_null() && !gamepad.is_undefined() {
                    if let Ok(gp) = gamepad.dyn_into::<web_sys::Gamepad>() {
                        let buttons: Vec<bool> = (0..gp.buttons().length())
                            .filter_map(|i| gp.buttons().get(i))
                            .filter_map(|b| b.dyn_into::<web_sys::GamepadButton>().ok())
                            .map(|b| b.pressed())
                            .collect();

                        let axes: Vec<f64> = gp.axes().iter().filter_map(|v| v.as_f64()).collect();

                        result.push(GamepadState {
                            index: gp.index(),
                            id: gp.id(),
                            connected: gp.connected(),
                            buttons,
                            axes,
                            timestamp: gp.timestamp(),
                        });
                    }
                }
            }
        }

        result
    }
}

/// Gamepad state
#[derive(Debug, Clone)]
pub struct GamepadState {
    pub index: u32,
    pub id: String,
    pub connected: bool,
    pub buttons: Vec<bool>,
    pub axes: Vec<f64>,
    pub timestamp: f64,
}

// ==================== Helper Functions ====================

/// Convert wgpu texture format to our SurfaceFormat
fn wgpu_to_surface_format(format: wgpu::TextureFormat) -> SurfaceFormat {
    match format {
        wgpu::TextureFormat::Bgra8Unorm => SurfaceFormat::Bgra8Unorm,
        wgpu::TextureFormat::Bgra8UnormSrgb => SurfaceFormat::Bgra8UnormSrgb,
        wgpu::TextureFormat::Rgba8Unorm => SurfaceFormat::Rgba8Unorm,
        wgpu::TextureFormat::Rgba8UnormSrgb => SurfaceFormat::Rgba8UnormSrgb,
        wgpu::TextureFormat::Rgba16Float => SurfaceFormat::Rgba16Float,
        wgpu::TextureFormat::Rgb10a2Unorm => SurfaceFormat::Rgb10a2Unorm,
        _ => SurfaceFormat::Bgra8UnormSrgb,
    }
}

/// Convert our SurfaceFormat to wgpu texture format
fn surface_format_to_wgpu(format: SurfaceFormat) -> wgpu::TextureFormat {
    match format {
        SurfaceFormat::Bgra8Unorm => wgpu::TextureFormat::Bgra8Unorm,
        SurfaceFormat::Bgra8UnormSrgb => wgpu::TextureFormat::Bgra8UnormSrgb,
        SurfaceFormat::Rgba8Unorm => wgpu::TextureFormat::Rgba8Unorm,
        SurfaceFormat::Rgba8UnormSrgb => wgpu::TextureFormat::Rgba8UnormSrgb,
        SurfaceFormat::Rgba16Float => wgpu::TextureFormat::Rgba16Float,
        SurfaceFormat::Rgb10a2Unorm => wgpu::TextureFormat::Rgb10a2Unorm,
    }
}

/// Convert wgpu present mode to our PresentMode
fn wgpu_to_present_mode(mode: wgpu::PresentMode) -> PresentMode {
    match mode {
        wgpu::PresentMode::Immediate => PresentMode::Immediate,
        wgpu::PresentMode::Mailbox => PresentMode::Mailbox,
        wgpu::PresentMode::Fifo => PresentMode::Fifo,
        wgpu::PresentMode::FifoRelaxed => PresentMode::FifoRelaxed,
        _ => PresentMode::Fifo,
    }
}

/// Convert our PresentMode to wgpu present mode
fn present_mode_to_wgpu(mode: PresentMode) -> wgpu::PresentMode {
    match mode {
        PresentMode::Immediate => wgpu::PresentMode::Immediate,
        PresentMode::Mailbox => wgpu::PresentMode::Mailbox,
        PresentMode::Fifo => wgpu::PresentMode::Fifo,
        PresentMode::FifoRelaxed => wgpu::PresentMode::FifoRelaxed,
    }
}

// ==================== WASM Entry Point Helpers ====================

/// Initialize console logging for WASM
pub fn init_logging() {
    #[cfg(feature = "web")]
    {
        console_error_panic_hook::set_once();
        // Use web_sys console for logging
        // In production, you'd want to set up proper wasm logging
    }
}

/// Log message to browser console
pub fn log(message: &str) {
    console::log_1(&JsValue::from_str(message));
}

/// Log warning to browser console
pub fn warn(message: &str) {
    console::warn_1(&JsValue::from_str(message));
}

/// Log error to browser console
pub fn error(message: &str) {
    console::error_1(&JsValue::from_str(message));
}

// ==================== Tests ====================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pixel_ratio_mode() {
        assert_eq!(PixelRatioMode::Native.effective_ratio(2.0), 2.0);
        assert_eq!(PixelRatioMode::One.effective_ratio(2.0), 1.0);
        assert_eq!(PixelRatioMode::Fixed(150).effective_ratio(2.0), 1.5);
        assert_eq!(PixelRatioMode::Clamped(150).effective_ratio(2.0), 1.5);
        assert_eq!(PixelRatioMode::Clamped(300).effective_ratio(2.0), 2.0);
    }

    #[test]
    fn test_xr_mode() {
        assert_eq!(XrMode::Inline.as_str(), "inline");
        assert_eq!(XrMode::ImmersiveVr.as_str(), "immersive-vr");
        assert_eq!(XrMode::ImmersiveAr.as_str(), "immersive-ar");
    }

    #[test]
    fn test_format_conversion() {
        assert_eq!(
            surface_format_to_wgpu(SurfaceFormat::Bgra8UnormSrgb),
            wgpu::TextureFormat::Bgra8UnormSrgb
        );
        assert_eq!(
            wgpu_to_surface_format(wgpu::TextureFormat::Rgba16Float),
            SurfaceFormat::Rgba16Float
        );
    }

    #[test]
    fn test_present_mode_conversion() {
        assert_eq!(
            present_mode_to_wgpu(PresentMode::Mailbox),
            wgpu::PresentMode::Mailbox
        );
        assert_eq!(
            wgpu_to_present_mode(wgpu::PresentMode::Fifo),
            PresentMode::Fifo
        );
    }
}
