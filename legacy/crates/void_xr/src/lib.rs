//! # void_xr - XR/AR/VR Abstraction Layer
//!
//! Dynamic and extensible XR support with:
//! - Runtime mode switching (VR, AR, MR, Desktop)
//! - Pluggable backend support (OpenXR, etc.)
//! - Hand tracking
//! - Spatial anchors
//! - Passthrough camera
//!
//! ## Architecture
//!
//! The XR system is designed to be:
//! 1. **Dynamic**: Switch between VR/AR/Desktop at runtime
//! 2. **Extensible**: New XR features can be added via plugins
//! 3. **Backend-agnostic**: Works with OpenXR, WebXR, or custom backends
//!
//! ## Example
//!
//! ```ignore
//! use void_xr::prelude::*;
//!
//! // Create XR system
//! let mut xr = XrSystem::new();
//!
//! // Register a backend (requires "openxr-backend" feature)
//! #[cfg(feature = "openxr-backend")]
//! xr.register_backend(OpenXrBackend::new()?);
//!
//! // Initialize
//! xr.initialize()?;
//!
//! // Switch to VR mode
//! xr.set_mode(XrMode::Vr)?;
//!
//! // Game loop
//! loop {
//!     xr.begin_frame()?;
//!
//!     let views = xr.views();
//!     for view in views {
//!         // Render for each eye
//!     }
//!
//!     xr.end_frame()?;
//! }
//! ```
//!
//! ## Features
//!
//! - `openxr-backend`: Enable OpenXR backend (requires OpenXR runtime)
//! - `hand-tracking`: Enable hand tracking support (requires runtime support)
//! - `passthrough`: Enable passthrough/AR support (requires compatible hardware)

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

// OpenXR backend module (optional)
#[cfg(feature = "openxr-backend")]
pub mod openxr_backend;

#[cfg(feature = "openxr-backend")]
pub use openxr_backend::{OpenXrBackend, OpenXrConfig, SwapchainImage};

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;
use void_math::{Vec3, Quat, Mat4};
use void_core::Id;

/// XR operation mode
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum XrMode {
    /// Desktop mode (no XR)
    Desktop,
    /// Virtual Reality (fully immersive)
    Vr,
    /// Augmented Reality (overlay on real world)
    Ar,
    /// Mixed Reality (VR with passthrough)
    Mr,
    /// Spectator mode (third-person view)
    Spectator,
}

impl Default for XrMode {
    fn default() -> Self {
        Self::Desktop
    }
}

/// XR session state
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SessionState {
    /// Session is idle
    Idle,
    /// Session is ready to begin
    Ready,
    /// Session is synchronized
    Synchronized,
    /// Session is visible
    Visible,
    /// Session is focused and interactive
    Focused,
    /// Session is stopping
    Stopping,
    /// Session has lost focus
    LossFocus,
    /// Session encountered an error
    Error,
}

impl Default for SessionState {
    fn default() -> Self {
        Self::Idle
    }
}

/// Eye/view type
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Eye {
    Left,
    Right,
    Center,
}

/// View for rendering
#[derive(Clone, Debug)]
pub struct View {
    /// Which eye
    pub eye: Eye,
    /// View matrix (world to eye space)
    pub view_matrix: Mat4,
    /// Projection matrix
    pub projection_matrix: Mat4,
    /// Field of view (radians)
    pub fov: Fov,
    /// Render target size
    pub render_size: [u32; 2],
}

/// Field of view definition
#[derive(Clone, Copy, Debug)]
pub struct Fov {
    pub angle_left: f32,
    pub angle_right: f32,
    pub angle_up: f32,
    pub angle_down: f32,
}

impl Default for Fov {
    fn default() -> Self {
        let half_fov = 45.0_f32.to_radians();
        Self {
            angle_left: -half_fov,
            angle_right: half_fov,
            angle_up: half_fov,
            angle_down: -half_fov,
        }
    }
}

/// Pose (position + orientation)
#[derive(Clone, Copy, Debug)]
pub struct Pose {
    pub position: Vec3,
    pub orientation: Quat,
}

impl Default for Pose {
    fn default() -> Self {
        Self {
            position: Vec3::ZERO,
            orientation: Quat::IDENTITY,
        }
    }
}

impl Pose {
    /// Create a new pose
    pub fn new(position: Vec3, orientation: Quat) -> Self {
        Self { position, orientation }
    }

    /// Convert to transformation matrix
    pub fn to_matrix(&self) -> Mat4 {
        Mat4::from_rotation_translation(self.orientation, self.position)
    }

    /// Interpolate between two poses
    pub fn lerp(&self, other: &Pose, t: f32) -> Pose {
        Pose {
            position: self.position.lerp(other.position, t),
            orientation: self.orientation.slerp(other.orientation, t),
        }
    }
}

/// Tracking space reference
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ReferenceSpace {
    /// Head-relative
    View,
    /// Local (seated)
    Local,
    /// Stage (standing, room-scale)
    Stage,
    /// Unbounded (world-scale)
    Unbounded,
}

impl Default for ReferenceSpace {
    fn default() -> Self {
        Self::Local
    }
}

/// Head-mounted display info
#[derive(Clone, Debug)]
pub struct HmdInfo {
    /// Device name
    pub name: String,
    /// Manufacturer
    pub manufacturer: String,
    /// Display refresh rate
    pub refresh_rate: f32,
    /// Render resolution per eye
    pub render_resolution: [u32; 2],
    /// Supports hand tracking
    pub supports_hand_tracking: bool,
    /// Supports passthrough
    pub supports_passthrough: bool,
    /// IPD (interpupillary distance) in meters
    pub ipd: f32,
}

impl Default for HmdInfo {
    fn default() -> Self {
        Self {
            name: "Unknown".to_string(),
            manufacturer: "Unknown".to_string(),
            refresh_rate: 90.0,
            render_resolution: [1920, 1920],
            supports_hand_tracking: false,
            supports_passthrough: false,
            ipd: 0.063, // 63mm default
        }
    }
}

/// Controller hand
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Hand {
    Left,
    Right,
}

/// Controller input state
#[derive(Clone, Debug, Default)]
pub struct ControllerState {
    /// Controller pose
    pub pose: Pose,
    /// Grip pose (where hand grips the controller)
    pub grip_pose: Pose,
    /// Aim pose (where controller points)
    pub aim_pose: Pose,
    /// Is tracked
    pub is_tracked: bool,
    /// Trigger value (0.0 - 1.0)
    pub trigger: f32,
    /// Grip value (0.0 - 1.0)
    pub grip: f32,
    /// Primary button (A/X)
    pub primary_button: bool,
    /// Secondary button (B/Y)
    pub secondary_button: bool,
    /// Menu button
    pub menu_button: bool,
    /// Thumbstick position
    pub thumbstick: [f32; 2],
    /// Thumbstick pressed
    pub thumbstick_pressed: bool,
    /// Touchpad position
    pub touchpad: [f32; 2],
    /// Touchpad touched
    pub touchpad_touched: bool,
    /// Touchpad pressed
    pub touchpad_pressed: bool,
}

/// Hand joint
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum HandJoint {
    Wrist,
    Palm,
    ThumbMetacarpal,
    ThumbProximal,
    ThumbDistal,
    ThumbTip,
    IndexMetacarpal,
    IndexProximal,
    IndexIntermediate,
    IndexDistal,
    IndexTip,
    MiddleMetacarpal,
    MiddleProximal,
    MiddleIntermediate,
    MiddleDistal,
    MiddleTip,
    RingMetacarpal,
    RingProximal,
    RingIntermediate,
    RingDistal,
    RingTip,
    LittleMetacarpal,
    LittleProximal,
    LittleIntermediate,
    LittleDistal,
    LittleTip,
}

/// Hand tracking state
#[derive(Clone, Debug, Default)]
pub struct HandState {
    /// Is hand tracked
    pub is_tracked: bool,
    /// Joint poses
    pub joints: BTreeMap<u8, Pose>,
    /// Pinch gesture (thumb + index)
    pub pinch_strength: f32,
    /// Grab gesture
    pub grab_strength: f32,
}

/// Spatial anchor
#[derive(Clone, Debug)]
pub struct SpatialAnchor {
    /// Unique ID
    pub id: AnchorId,
    /// Anchor pose
    pub pose: Pose,
    /// Is anchor tracked
    pub is_tracked: bool,
    /// Custom data
    pub data: BTreeMap<String, String>,
}

/// Anchor identifier
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct AnchorId(pub Id);

impl AnchorId {
    pub fn new(id: u64) -> Self {
        Self(Id::from_bits(id))
    }
}

/// Frame timing info
#[derive(Clone, Copy, Debug, Default)]
pub struct FrameTiming {
    /// Predicted display time
    pub predicted_display_time: f64,
    /// Time since last frame
    pub delta_time: f32,
    /// Frame index
    pub frame_index: u64,
}

/// Passthrough configuration
#[derive(Clone, Copy, Debug)]
pub struct PassthroughConfig {
    /// Enable passthrough
    pub enabled: bool,
    /// Opacity (0.0 - 1.0)
    pub opacity: f32,
    /// Brightness adjustment
    pub brightness: f32,
    /// Contrast adjustment
    pub contrast: f32,
    /// Edge enhancement
    pub edge_enhancement: f32,
}

impl Default for PassthroughConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            opacity: 1.0,
            brightness: 1.0,
            contrast: 1.0,
            edge_enhancement: 0.0,
        }
    }
}

/// XR backend trait
pub trait XrBackend: Send + Sync {
    /// Get backend name
    fn name(&self) -> &str;

    /// Initialize the backend
    fn initialize(&mut self) -> Result<(), XrError>;

    /// Shutdown the backend
    fn shutdown(&mut self);

    /// Get supported modes
    fn supported_modes(&self) -> Vec<XrMode>;

    /// Set the current mode
    fn set_mode(&mut self, mode: XrMode) -> Result<(), XrError>;

    /// Get current mode
    fn current_mode(&self) -> XrMode;

    /// Get session state
    fn session_state(&self) -> SessionState;

    /// Get HMD info
    fn hmd_info(&self) -> &HmdInfo;

    /// Begin frame
    fn begin_frame(&mut self) -> Result<FrameTiming, XrError>;

    /// End frame
    fn end_frame(&mut self) -> Result<(), XrError>;

    /// Get views for rendering
    fn views(&self) -> Vec<View>;

    /// Get head pose
    fn head_pose(&self) -> Pose;

    /// Get controller state
    fn controller_state(&self, hand: Hand) -> ControllerState;

    /// Get hand tracking state
    fn hand_state(&self, hand: Hand) -> Option<HandState>;

    /// Set reference space
    fn set_reference_space(&mut self, space: ReferenceSpace) -> Result<(), XrError>;

    /// Create spatial anchor
    fn create_anchor(&mut self, pose: Pose) -> Result<AnchorId, XrError>;

    /// Destroy spatial anchor
    fn destroy_anchor(&mut self, id: AnchorId) -> Result<(), XrError>;

    /// Get spatial anchor
    fn get_anchor(&self, id: AnchorId) -> Option<&SpatialAnchor>;

    /// Configure passthrough
    fn configure_passthrough(&mut self, config: PassthroughConfig) -> Result<(), XrError>;

    /// Trigger haptic feedback
    fn trigger_haptic(
        &mut self,
        hand: Hand,
        amplitude: f32,
        duration_seconds: f32,
        frequency: f32,
    );
}

/// XR error types
#[derive(Clone, Debug)]
pub enum XrError {
    /// Backend not initialized
    NotInitialized,
    /// Feature not supported
    NotSupported(String),
    /// Session error
    SessionError(String),
    /// Runtime error
    RuntimeError(String),
    /// Device error
    DeviceError(String),
    /// Custom error
    Custom(String),
}

impl core::fmt::Display for XrError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::NotInitialized => write!(f, "XR not initialized"),
            Self::NotSupported(s) => write!(f, "Not supported: {}", s),
            Self::SessionError(s) => write!(f, "Session error: {}", s),
            Self::RuntimeError(s) => write!(f, "Runtime error: {}", s),
            Self::DeviceError(s) => write!(f, "Device error: {}", s),
            Self::Custom(s) => write!(f, "{}", s),
        }
    }
}

/// The main XR system
pub struct XrSystem {
    /// Registered backends
    backends: BTreeMap<String, Box<dyn XrBackend>>,
    /// Active backend
    active_backend: Option<String>,
    /// Current mode
    current_mode: XrMode,
    /// Is initialized
    initialized: bool,
    /// Frame index
    frame_index: u64,
}

impl XrSystem {
    /// Create a new XR system
    pub fn new() -> Self {
        Self {
            backends: BTreeMap::new(),
            active_backend: None,
            current_mode: XrMode::Desktop,
            initialized: false,
            frame_index: 0,
        }
    }

    /// Register a backend
    pub fn register_backend(&mut self, backend: Box<dyn XrBackend>) {
        let name = backend.name().to_string();
        if self.active_backend.is_none() {
            self.active_backend = Some(name.clone());
        }
        self.backends.insert(name, backend);
    }

    /// Get active backend
    fn active(&self) -> Option<&dyn XrBackend> {
        self.active_backend
            .as_ref()
            .and_then(|name| self.backends.get(name))
            .map(|b| b.as_ref())
    }

    /// Get active backend mutably
    fn active_mut(&mut self) -> Option<&mut (dyn XrBackend + '_)> {
        let name = self.active_backend.clone()?;
        match self.backends.get_mut(&name) {
            Some(b) => Some(b.as_mut()),
            None => None,
        }
    }

    /// Initialize the XR system
    pub fn initialize(&mut self) -> Result<(), XrError> {
        if let Some(backend) = self.active_mut() {
            backend.initialize()?;
            self.initialized = true;
            Ok(())
        } else {
            // No backend - just desktop mode
            self.initialized = true;
            Ok(())
        }
    }

    /// Shutdown the XR system
    pub fn shutdown(&mut self) {
        if let Some(backend) = self.active_mut() {
            backend.shutdown();
        }
        self.initialized = false;
    }

    /// Check if initialized
    pub fn is_initialized(&self) -> bool {
        self.initialized
    }

    /// Set XR mode
    pub fn set_mode(&mut self, mode: XrMode) -> Result<(), XrError> {
        if mode == XrMode::Desktop {
            self.current_mode = mode;
            return Ok(());
        }

        if let Some(backend) = self.active_mut() {
            backend.set_mode(mode)?;
            self.current_mode = mode;
            Ok(())
        } else {
            Err(XrError::NotSupported("No XR backend available".into()))
        }
    }

    /// Get current mode
    pub fn current_mode(&self) -> XrMode {
        self.current_mode
    }

    /// Get session state
    pub fn session_state(&self) -> SessionState {
        self.active()
            .map(|b| b.session_state())
            .unwrap_or(SessionState::Idle)
    }

    /// Get HMD info
    pub fn hmd_info(&self) -> Option<&HmdInfo> {
        self.active().map(|b| b.hmd_info())
    }

    /// Begin frame
    pub fn begin_frame(&mut self) -> Result<FrameTiming, XrError> {
        self.frame_index += 1;

        if let Some(backend) = self.active_mut() {
            backend.begin_frame()
        } else {
            Ok(FrameTiming {
                predicted_display_time: 0.0,
                delta_time: 1.0 / 60.0,
                frame_index: self.frame_index,
            })
        }
    }

    /// End frame
    pub fn end_frame(&mut self) -> Result<(), XrError> {
        if let Some(backend) = self.active_mut() {
            backend.end_frame()
        } else {
            Ok(())
        }
    }

    /// Get views for rendering
    pub fn views(&self) -> Vec<View> {
        if let Some(backend) = self.active() {
            backend.views()
        } else {
            // Desktop mode - single view
            vec![View {
                eye: Eye::Center,
                view_matrix: Mat4::IDENTITY,
                projection_matrix: Mat4::perspective(
                    60.0_f32.to_radians(),
                    16.0 / 9.0,
                    0.1,
                    1000.0,
                ),
                fov: Fov::default(),
                render_size: [1920, 1080],
            }]
        }
    }

    /// Get head pose
    pub fn head_pose(&self) -> Pose {
        self.active()
            .map(|b| b.head_pose())
            .unwrap_or_default()
    }

    /// Get controller state
    pub fn controller_state(&self, hand: Hand) -> ControllerState {
        self.active()
            .map(|b| b.controller_state(hand))
            .unwrap_or_default()
    }

    /// Get hand tracking state
    pub fn hand_state(&self, hand: Hand) -> Option<HandState> {
        self.active().and_then(|b| b.hand_state(hand))
    }

    /// Create spatial anchor
    pub fn create_anchor(&mut self, pose: Pose) -> Result<AnchorId, XrError> {
        if let Some(backend) = self.active_mut() {
            backend.create_anchor(pose)
        } else {
            Err(XrError::NotSupported("Anchors not available in desktop mode".into()))
        }
    }

    /// Trigger haptic feedback
    pub fn trigger_haptic(
        &mut self,
        hand: Hand,
        amplitude: f32,
        duration_seconds: f32,
        frequency: f32,
    ) {
        if let Some(backend) = self.active_mut() {
            backend.trigger_haptic(hand, amplitude, duration_seconds, frequency);
        }
    }

    /// Configure passthrough
    pub fn configure_passthrough(&mut self, config: PassthroughConfig) -> Result<(), XrError> {
        if let Some(backend) = self.active_mut() {
            backend.configure_passthrough(config)
        } else {
            Err(XrError::NotSupported("Passthrough not available".into()))
        }
    }

    /// Check if XR is available
    pub fn is_xr_available(&self) -> bool {
        !self.backends.is_empty()
    }

    /// Get available modes
    pub fn available_modes(&self) -> Vec<XrMode> {
        let mut modes = vec![XrMode::Desktop];
        if let Some(backend) = self.active() {
            modes.extend(backend.supported_modes());
        }
        modes
    }
}

impl Default for XrSystem {
    fn default() -> Self {
        Self::new()
    }
}

/// Prelude - commonly used types
pub mod prelude {
    pub use crate::{
        XrSystem, XrMode, XrBackend, XrError,
        SessionState, View, Eye, Fov, Pose,
        ReferenceSpace, HmdInfo, FrameTiming,
        Hand, ControllerState, HandState, HandJoint,
        SpatialAnchor, AnchorId,
        PassthroughConfig,
    };

    // OpenXR backend types (when enabled)
    #[cfg(feature = "openxr-backend")]
    pub use crate::openxr_backend::{OpenXrBackend, OpenXrConfig, SwapchainImage};
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_xr_system() {
        let mut xr = XrSystem::new();

        assert!(!xr.is_xr_available());
        assert_eq!(xr.current_mode(), XrMode::Desktop);

        xr.initialize().unwrap();
        assert!(xr.is_initialized());

        let views = xr.views();
        assert_eq!(views.len(), 1);
        assert_eq!(views[0].eye, Eye::Center);
    }

    #[test]
    fn test_pose() {
        let pose1 = Pose::new(Vec3::ZERO, Quat::IDENTITY);
        let pose2 = Pose::new(Vec3::new(1.0, 0.0, 0.0), Quat::IDENTITY);

        let lerped = pose1.lerp(&pose2, 0.5);
        assert!((lerped.position.x - 0.5).abs() < 0.001);
    }
}
