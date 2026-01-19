//! Smithay-based Wayland compositor for Linux
//!
//! This is the core compositor implementation using Smithay.
//! It handles DRM/KMS display management, libinput, and the Wayland protocol.

use std::os::unix::io::{AsRawFd, RawFd};
use std::path::Path;
use std::time::Duration;
use std::collections::HashMap;

use calloop::{EventLoop, LoopSignal};
use drm::control::{connector, crtc, property, Device as DrmControlDevice, Mode as DrmMode};
use slog::Drain;
use smithay::{
    backend::{
        allocator::gbm::GbmDevice,
        drm::{DrmDevice, DrmEvent},
        libinput::{LibinputInputBackend, LibinputSessionInterface},
        session::{
            libseat::LibSeatSession,
            Session,
        },
        udev::{UdevBackend, UdevEvent},
    },
    reexports::{
        input::{
            Libinput,
            event::pointer::PointerEventTrait,
            event::keyboard::KeyboardEventTrait,
        },
        nix::fcntl::OFlag,
    },
};

use crate::{
    backend::{OutputInfo, OutputMode, OutputTransform},
    error::CompositorError,
    frame::FrameScheduler,
    input::{InputEvent, InputState},
    vrr::{VrrCapability, VrrConfig, VrrMode},
    hdr::{HdrCapability, HdrConfig},
    CompositorCapabilities, CompositorConfig, RenderFormat, RenderTarget,
};

/// File descriptor wrapper for DRM device
#[derive(Debug)]
struct DrmFd(RawFd);

impl AsRawFd for DrmFd {
    fn as_raw_fd(&self) -> RawFd {
        self.0
    }
}

impl Drop for DrmFd {
    fn drop(&mut self) {
        // File descriptor is owned by the session, don't close it
    }
}

/// Smithay-based Wayland compositor
pub struct Compositor {
    /// Event loop
    event_loop: EventLoop<'static, CompositorState>,
    /// Compositor state
    state: CompositorState,
    /// Loop signal for shutdown
    loop_signal: LoopSignal,
    /// Configuration
    config: CompositorConfig,
    /// Running flag
    running: bool,
}

/// Internal compositor state
struct CompositorState {
    /// Session (libseat)
    session: LibSeatSession,
    /// Frame scheduler
    frame_scheduler: FrameScheduler,
    /// Input state
    input_state: InputState,
    /// Pending input events
    pending_input: Vec<InputEvent>,
    /// DRM devices (keyed by device path)
    drm_devices: HashMap<String, DrmDeviceState>,
    /// Current frame number
    frame_number: u64,
    /// Primary output info
    primary_output: Option<OutputInfo>,
}

/// State for a single DRM device
struct DrmDeviceState {
    /// DRM device
    device: DrmDevice<DrmFd>,
    /// Device path
    path: String,
    /// Active CRTCs
    crtcs: Vec<CrtcState>,
    /// GBM device for buffer allocation
    gbm: Option<GbmDevice<DrmFd>>,
}

/// State for a single CRTC (output)
struct CrtcState {
    /// CRTC handle
    crtc: crtc::Handle,
    /// Connector
    connector: connector::Handle,
    /// Current mode
    mode: DrmMode,
    /// Output info
    info: OutputInfo,
    /// VRR capability
    vrr_capability: VrrCapability,
    /// HDR capability
    hdr_capability: HdrCapability,
    /// Active VRR configuration
    vrr_config: Option<VrrConfig>,
    /// Active HDR configuration
    hdr_config: Option<HdrConfig>,
}

impl Compositor {
    /// Create a new Smithay compositor
    pub fn new(config: CompositorConfig) -> Result<Self, CompositorError> {
        log::info!("Initializing Smithay compositor");

        // Create slog logger that forwards to log crate
        let logger = slog::Logger::root(slog_stdlog::StdLog.fuse(), slog::o!());

        // Create event loop
        let event_loop: EventLoop<CompositorState> =
            EventLoop::try_new().map_err(|e| CompositorError::Session(e.to_string()))?;

        let loop_signal = event_loop.get_signal();

        // Initialize session (libseat)
        let (session, session_notifier) = LibSeatSession::new(logger.clone())
            .map_err(|e| CompositorError::Session(format!("Failed to create session: {}", e)))?;

        log::info!("Session created: seat={}", session.seat());

        // Create initial state
        let state = CompositorState {
            session,
            frame_scheduler: FrameScheduler::new(config.target_fps),
            input_state: InputState::new(),
            pending_input: Vec::new(),
            drm_devices: HashMap::new(),
            frame_number: 0,
            primary_output: None,
        };

        // Insert session notifier into event loop
        event_loop
            .handle()
            .insert_source(session_notifier, |_event, _, _state| {
                // Session events are handled internally by libseat
                // The notifier just signals that something happened
                log::debug!("Session notifier event");
            })
            .map_err(|e| CompositorError::Session(e.to_string()))?;

        let mut compositor = Self {
            event_loop,
            state,
            loop_signal,
            config,
            running: true,
        };

        // Initialize backends
        compositor.init_udev(logger.clone())?;
        compositor.init_libinput(logger)?;

        Ok(compositor)
    }

    /// Initialize udev backend for GPU hotplug
    fn init_udev(&mut self, logger: slog::Logger) -> Result<(), CompositorError> {
        let udev_backend = UdevBackend::new(self.state.session.seat(), logger)
            .map_err(|e| CompositorError::Session(format!("Failed to create udev backend: {}", e)))?;

        // Enumerate existing GPUs
        for (device_id, path) in udev_backend.device_list() {
            if let Err(e) = self.add_drm_device(device_id, &path) {
                log::warn!("Failed to add DRM device {}: {}", path.display(), e);
            }
        }

        // Watch for GPU hotplug
        self.event_loop
            .handle()
            .insert_source(udev_backend, |event, _, _state| match event {
                UdevEvent::Added { device_id: _, path } => {
                    log::info!("GPU hotplug: device added at {}", path.display());
                }
                UdevEvent::Changed { device_id } => {
                    log::info!("GPU hotplug: device changed {:?}", device_id);
                }
                UdevEvent::Removed { device_id } => {
                    log::info!("GPU hotplug: device removed {:?}", device_id);
                }
            })
            .map_err(|e| CompositorError::Session(e.to_string()))?;

        Ok(())
    }

    /// Add a DRM device
    fn add_drm_device(&mut self, _device_id: DevT, path: &Path) -> Result<(), CompositorError> {
        log::info!("Adding DRM device: {}", path.display());

        // Open device
        let fd = self
            .state
            .session
            .open(
                path,
                OFlag::O_RDWR | OFlag::O_CLOEXEC | OFlag::O_NOCTTY | OFlag::O_NONBLOCK,
            )
            .map_err(|e| CompositorError::Drm(format!("Failed to open DRM device: {}", e)))?;

        let drm_fd = DrmFd(fd);

        // Create DRM device - Smithay 0.3 returns just the device
        let drm = DrmDevice::new(drm_fd, true, None)
            .map_err(|e| CompositorError::Drm(format!("Failed to create DRM device: {}", e)))?;

        // Enumerate connectors
        let resources = drm
            .resource_handles()
            .map_err(|e| CompositorError::Drm(format!("Failed to get DRM resources: {}", e)))?;

        let mut crtcs = Vec::new();

        for connector_handle in resources.connectors() {
            let connector_info = drm
                .get_connector(*connector_handle)
                .map_err(|e| CompositorError::Drm(format!("Failed to get connector: {}", e)))?;

            if connector_info.state() != connector::State::Connected {
                continue;
            }

            // Find a suitable CRTC
            let encoder = connector_info
                .current_encoder()
                .and_then(|e| drm.get_encoder(e).ok());

            let crtc_handle = encoder
                .and_then(|e| e.crtc())
                .or_else(|| resources.crtcs().first().copied());

            if let Some(crtc_handle) = crtc_handle {
                // Get preferred mode - the first mode is typically the preferred/native mode
                let mode = connector_info.modes().first().copied();

                if let Some(mode) = mode {
                    let (width, height) = mode.size();
                    let refresh_mhz = mode.vrefresh() as u32 * 1000;

                    // Convert CRTC handle to u64 - use the raw handle value
                    let crtc_id = {
                        use drm::control::RawResourceHandle;
                        Into::<u32>::into(crtc_handle) as u64
                    };

                    let output_info = OutputInfo {
                        id: crtc_id,
                        name: format!("{:?}", connector_info.interface()),
                        physical_size: connector_info.size().map(|(w, h)| (w as u32, h as u32)),
                        current_mode: OutputMode {
                            width: width as u32,
                            height: height as u32,
                            refresh_mhz,
                        },
                        available_modes: connector_info
                            .modes()
                            .iter()
                            .map(|m| {
                                let (w, h) = m.size();
                                OutputMode {
                                    width: w as u32,
                                    height: h as u32,
                                    refresh_mhz: m.vrefresh() as u32 * 1000,
                                }
                            })
                            .collect(),
                        primary: crtcs.is_empty(),
                        position: (0, 0),
                        scale: 1.0,
                        transform: OutputTransform::Normal,
                    };

                    log::info!(
                        "Found output: {} ({}x{}@{}Hz)",
                        output_info.name,
                        width,
                        height,
                        mode.vrefresh()
                    );

                    // Store primary output
                    if crtcs.is_empty() {
                        self.state.primary_output = Some(output_info.clone());
                    }

                    // Detect VRR capability
                    let vrr_capability = Self::detect_vrr_capability(&drm, *connector_handle);
                    if vrr_capability.supported {
                        log::info!(
                            "  VRR supported: {}",
                            vrr_capability.technology.as_deref().unwrap_or("Unknown")
                        );
                    }

                    // Detect HDR capability
                    let hdr_capability = Self::detect_hdr_capability(&drm, *connector_handle);
                    if hdr_capability.supported {
                        log::info!(
                            "  HDR supported: max={} nits",
                            hdr_capability.max_luminance.unwrap_or(0)
                        );
                    }

                    crtcs.push(CrtcState {
                        crtc: crtc_handle,
                        connector: *connector_handle,
                        mode,
                        info: output_info,
                        vrr_capability,
                        hdr_capability,
                        vrr_config: None,
                        hdr_config: None,
                    });
                }
            }
        }

        if crtcs.is_empty() {
            log::warn!("No outputs found on device {}", path.display());
        }

        let path_str = path.to_string_lossy().to_string();

        self.state.drm_devices.insert(path_str.clone(), DrmDeviceState {
            device: drm,
            path: path_str,
            crtcs,
            gbm: None,
        });

        Ok(())
    }

    /// Initialize libinput backend
    fn init_libinput(&mut self, logger: slog::Logger) -> Result<(), CompositorError> {
        let seat = self.state.session.seat();

        let mut libinput_context = Libinput::new_with_udev(
            LibinputSessionInterface::from(self.state.session.clone()),
        );

        libinput_context
            .udev_assign_seat(&seat)
            .map_err(|_| CompositorError::Input("Failed to assign seat to libinput".into()))?;

        let libinput_backend = LibinputInputBackend::new(libinput_context, logger);

        self.event_loop
            .handle()
            .insert_source(libinput_backend, |event, _, state| {
                use smithay::backend::input::{
                    InputEvent as SmithayEvent,
                    KeyboardKeyEvent, PointerAxisEvent, PointerButtonEvent,
                    PointerMotionEvent, PointerMotionAbsoluteEvent,
                };

                match event {
                    SmithayEvent::Keyboard { event } => {
                        let keycode = event.key_code();
                        let key_state = if event.state() == smithay::backend::input::KeyState::Pressed {
                            crate::input::KeyState::Pressed
                        } else {
                            crate::input::KeyState::Released
                        };

                        // Convert time from microseconds to milliseconds
                        let time_ms = (event.time_usec() / 1000) as u32;

                        let input_event = crate::input::InputEvent::Keyboard(
                            crate::input::KeyboardEvent {
                                keycode,
                                state: key_state,
                                time_ms,
                                modifiers: state.input_state.modifiers(),
                            }
                        );

                        state.input_state.handle_event(&input_event);
                        state.pending_input.push(input_event);
                    }

                    SmithayEvent::PointerMotion { event } => {
                        let delta = glam::Vec2::new(
                            event.delta_x() as f32,
                            event.delta_y() as f32,
                        );

                        let time_ms = (event.time_usec() / 1000) as u32;

                        let input_event = crate::input::InputEvent::Pointer(
                            crate::input::PointerEvent::Motion {
                                position: None,
                                delta,
                                time_ms,
                            }
                        );

                        state.input_state.handle_event(&input_event);
                        state.pending_input.push(input_event);
                    }

                    SmithayEvent::PointerMotionAbsolute { event } => {
                        let (width, height) = state.primary_output
                            .as_ref()
                            .map(|o| (o.current_mode.width as i32, o.current_mode.height as i32))
                            .unwrap_or((1920, 1080));

                        let position = glam::Vec2::new(
                            event.x_transformed(width) as f32,
                            event.y_transformed(height) as f32,
                        );

                        let time_ms = (event.time_usec() / 1000) as u32;

                        let input_event = crate::input::InputEvent::Pointer(
                            crate::input::PointerEvent::Motion {
                                position: Some(position),
                                delta: glam::Vec2::ZERO,
                                time_ms,
                            }
                        );

                        state.input_state.handle_event(&input_event);
                        state.pending_input.push(input_event);
                    }

                    SmithayEvent::PointerButton { event } => {
                        let button = crate::input::PointerButton::from(event.button());
                        let button_state = if event.state() == smithay::backend::input::ButtonState::Pressed {
                            crate::input::ButtonState::Pressed
                        } else {
                            crate::input::ButtonState::Released
                        };

                        let time_ms = (event.time_usec() / 1000) as u32;

                        let input_event = crate::input::InputEvent::Pointer(
                            crate::input::PointerEvent::Button {
                                button,
                                state: button_state,
                                time_ms,
                            }
                        );

                        state.input_state.handle_event(&input_event);
                        state.pending_input.push(input_event);
                    }

                    SmithayEvent::PointerAxis { event } => {
                        use smithay::backend::input::Axis;

                        let horizontal = event.amount(Axis::Horizontal).unwrap_or(0.0);
                        let vertical = event.amount(Axis::Vertical).unwrap_or(0.0);
                        let time_ms = (event.time_usec() / 1000) as u32;

                        let input_event = crate::input::InputEvent::Pointer(
                            crate::input::PointerEvent::Axis {
                                horizontal,
                                vertical,
                                source: crate::input::AxisSource::Unknown,
                                time_ms,
                            }
                        );

                        state.input_state.handle_event(&input_event);
                        state.pending_input.push(input_event);
                    }

                    _ => {
                        // Other events (touch, device added/removed, etc.)
                        log::trace!("Unhandled input event");
                    }
                }
            })
            .map_err(|e| CompositorError::Input(e.to_string()))?;

        log::info!("libinput initialized on seat: {}", seat);

        Ok(())
    }

    /// Get compositor capabilities
    pub fn capabilities(&self) -> CompositorCapabilities {
        let mut refresh_rates = Vec::new();
        let mut max_resolution = (0, 0);
        let mut current_resolution = (0, 0);
        let mut display_count = 0;
        let mut vrr_supported = false;
        let mut hdr_supported = false;

        for device in self.state.drm_devices.values() {
            for crtc in &device.crtcs {
                display_count += 1;

                let mode = &crtc.info.current_mode;
                if mode.width > max_resolution.0 {
                    max_resolution = (mode.width, mode.height);
                }
                if current_resolution == (0, 0) {
                    current_resolution = (mode.width, mode.height);
                }

                let refresh = (mode.refresh_mhz / 1000) as u32;
                if !refresh_rates.contains(&refresh) {
                    refresh_rates.push(refresh);
                }

                if crtc.vrr_capability.supported {
                    vrr_supported = true;
                }
                if crtc.hdr_capability.supported {
                    hdr_supported = true;
                }
            }
        }

        CompositorCapabilities {
            refresh_rates,
            max_resolution,
            current_resolution,
            vrr_supported,
            hdr_supported,
            display_count,
        }
    }

    /// Run one iteration of the event loop
    pub fn dispatch(&mut self) -> Result<(), CompositorError> {
        self.event_loop
            .dispatch(Some(Duration::from_millis(1)), &mut self.state)
            .map_err(|e| CompositorError::Session(e.to_string()))?;

        Ok(())
    }

    /// Check if a frame should be rendered
    pub fn should_render(&self) -> bool {
        self.state.frame_scheduler.should_render()
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) -> Result<DrmRenderTarget, CompositorError> {
        self.state.frame_number = self.state.frame_scheduler.begin_frame();

        let (width, height) = self.state.primary_output
            .as_ref()
            .map(|o| (o.current_mode.width, o.current_mode.height))
            .unwrap_or((1920, 1080));

        Ok(DrmRenderTarget {
            size: (width, height),
            frame_number: self.state.frame_number,
        })
    }

    /// End the current frame
    pub fn end_frame(&mut self, _target: DrmRenderTarget) -> Result<(), CompositorError> {
        self.state.frame_scheduler.end_frame();
        Ok(())
    }

    /// Get the frame scheduler
    pub fn frame_scheduler(&self) -> &FrameScheduler {
        &self.state.frame_scheduler
    }

    /// Get mutable frame scheduler
    pub fn frame_scheduler_mut(&mut self) -> &mut FrameScheduler {
        &mut self.state.frame_scheduler
    }

    /// Poll for input events
    pub fn poll_input(&mut self) -> Vec<InputEvent> {
        std::mem::take(&mut self.state.pending_input)
    }

    /// Get current frame number
    pub fn frame_number(&self) -> u64 {
        self.state.frame_number
    }

    /// Check if compositor is running
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// Request shutdown
    pub fn shutdown(&mut self) {
        log::info!("Compositor shutdown requested");
        self.running = false;
        self.loop_signal.stop();
    }

    /// Detect VRR capability for a connector
    fn detect_vrr_capability(
        device: &DrmDevice<DrmFd>,
        connector_handle: connector::Handle,
    ) -> VrrCapability {
        // Get connector info first to check VRR support
        let connector_info = match device.get_connector(connector_handle) {
            Ok(info) => info,
            Err(_) => return VrrCapability::not_supported(),
        };

        // Check if connector properties indicate VRR capability
        // For now, assume VRR is supported if we have valid modes
        let modes = connector_info.modes();
        if modes.is_empty() {
            return VrrCapability::not_supported();
        }

        // Get refresh rate range from available modes
        let refresh_rates: Vec<u32> = modes.iter().map(|m| m.vrefresh() as u32).collect();
        let min_refresh = refresh_rates.iter().min().copied().unwrap_or(48);
        let max_refresh = refresh_rates.iter().max().copied().unwrap_or(144);

        // If the display supports a wide range of refresh rates, assume VRR capable
        if max_refresh > 60 && (max_refresh - min_refresh) > 30 {
            let technology = Some("FreeSync/AdaptiveSync".to_string());
            VrrCapability::supported(min_refresh, max_refresh, technology)
        } else {
            VrrCapability::not_supported()
        }
    }

    /// Detect HDR capability for a connector
    fn detect_hdr_capability(
        _device: &DrmDevice<DrmFd>,
        connector_handle: connector::Handle,
    ) -> HdrCapability {
        // HDR detection requires checking EDID data and connector properties
        // For now, return SDR only as the default
        // TODO: Implement proper HDR detection by parsing EDID and checking
        // for HDR_OUTPUT_METADATA property support
        let _ = connector_handle;
        HdrCapability::sdr_only()
    }

    /// Enable VRR on the primary output
    pub fn enable_vrr(&mut self, mode: VrrMode) -> Result<(), CompositorError> {
        let device_state = self.state.drm_devices.values_mut().next()
            .ok_or_else(|| CompositorError::Drm("No DRM devices".into()))?;

        let crtc_state = device_state.crtcs.first_mut()
            .ok_or_else(|| CompositorError::Drm("No outputs".into()))?;

        if !crtc_state.vrr_capability.supported {
            return Err(CompositorError::Drm("VRR not supported on this display".into()));
        }

        let mut vrr_config = crtc_state.vrr_capability.to_config()
            .ok_or_else(|| CompositorError::Drm("Failed to create VRR config".into()))?;

        vrr_config.enable(mode);
        crtc_state.vrr_config = Some(vrr_config.clone());
        self.state.frame_scheduler.set_vrr_config(Some(vrr_config));

        log::info!("VRR enabled with mode: {:?}", mode);
        Ok(())
    }

    /// Disable VRR
    pub fn disable_vrr(&mut self) -> Result<(), CompositorError> {
        let device_state = self.state.drm_devices.values_mut().next()
            .ok_or_else(|| CompositorError::Drm("No DRM devices".into()))?;

        let crtc_state = device_state.crtcs.first_mut()
            .ok_or_else(|| CompositorError::Drm("No outputs".into()))?;

        crtc_state.vrr_config = None;
        self.state.frame_scheduler.set_vrr_config(None);

        log::info!("VRR disabled");
        Ok(())
    }

    /// Enable HDR on the primary output
    pub fn enable_hdr(&mut self, config: HdrConfig) -> Result<(), CompositorError> {
        let device_state = self.state.drm_devices.values_mut().next()
            .ok_or_else(|| CompositorError::Drm("No DRM devices".into()))?;

        let crtc_state = device_state.crtcs.first_mut()
            .ok_or_else(|| CompositorError::Drm("No outputs".into()))?;

        if !crtc_state.hdr_capability.supported {
            return Err(CompositorError::Drm("HDR not supported on this display".into()));
        }

        if !crtc_state.hdr_capability.supports_transfer_function(config.transfer_function) {
            return Err(CompositorError::Drm(
                format!("Transfer function {:?} not supported", config.transfer_function)
            ));
        }

        crtc_state.hdr_config = Some(config.clone());

        log::info!(
            "HDR enabled: {} with {}",
            config.transfer_function.name(),
            config.color_primaries.name()
        );

        Ok(())
    }

    /// Disable HDR
    pub fn disable_hdr(&mut self) -> Result<(), CompositorError> {
        let device_state = self.state.drm_devices.values_mut().next()
            .ok_or_else(|| CompositorError::Drm("No DRM devices".into()))?;

        let crtc_state = device_state.crtcs.first_mut()
            .ok_or_else(|| CompositorError::Drm("No outputs".into()))?;

        crtc_state.hdr_config = None;

        log::info!("HDR disabled");
        Ok(())
    }

    /// Set HDR metadata
    pub fn set_hdr_metadata(&mut self, config: HdrConfig) -> Result<(), CompositorError> {
        self.enable_hdr(config)
    }

    /// Get VRR capability of primary output
    pub fn vrr_capability(&self) -> Option<&VrrCapability> {
        self.state.drm_devices
            .values()
            .next()
            .and_then(|d| d.crtcs.first())
            .map(|c| &c.vrr_capability)
    }

    /// Get HDR capability of primary output
    pub fn hdr_capability(&self) -> Option<&HdrCapability> {
        self.state.drm_devices
            .values()
            .next()
            .and_then(|d| d.crtcs.first())
            .map(|c| &c.hdr_capability)
    }

    /// Get active VRR configuration
    pub fn vrr_config(&self) -> Option<&VrrConfig> {
        self.state.drm_devices
            .values()
            .next()
            .and_then(|d| d.crtcs.first())
            .and_then(|c| c.vrr_config.as_ref())
    }

    /// Get active HDR configuration
    pub fn hdr_config(&self) -> Option<&HdrConfig> {
        self.state.drm_devices
            .values()
            .next()
            .and_then(|d| d.crtcs.first())
            .and_then(|c| c.hdr_config.as_ref())
    }
}

/// DRM render target
pub struct DrmRenderTarget {
    size: (u32, u32),
    frame_number: u64,
}

impl RenderTarget for DrmRenderTarget {
    fn size(&self) -> (u32, u32) {
        self.size
    }

    fn format(&self) -> RenderFormat {
        RenderFormat::Bgra8UnormSrgb
    }

    fn present(&mut self) -> Result<(), CompositorError> {
        Ok(())
    }
}

// Type alias for device IDs
type DevT = u64;
