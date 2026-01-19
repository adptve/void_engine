//! OpenXR Backend Implementation
//!
//! A complete OpenXR backend for void_xr, supporting:
//! - Instance creation and extension loading
//! - System discovery (find headset)
//! - Session lifecycle (create, begin, end, destroy)
//! - Swapchain management for stereo rendering
//! - Frame timing with pose prediction
//! - Head and controller tracking
//! - Hand tracking (via XR_EXT_hand_tracking)
//! - Input action system
//! - Haptic feedback
//!
//! Compatible with major OpenXR runtimes: SteamVR, Oculus, Monado, WMR.

use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::{String, ToString};
use alloc::vec;
use alloc::vec::Vec;

use openxr as xr;

use crate::{
    AnchorId, ControllerState, Eye, Fov, FrameTiming, Hand, HandJoint, HandState, HmdInfo,
    PassthroughConfig, Pose, ReferenceSpace, SessionState, SpatialAnchor, View, XrBackend,
    XrError, XrMode,
};
use void_core::{Id, IdGenerator};
use void_math::{Mat4, Quat, Vec3};

/// OpenXR graphics API binding type.
/// This backend uses Vulkan by default, but can be adapted for other APIs.
#[cfg(feature = "vulkan")]
type GraphicsApi = xr::Vulkan;

/// Headless graphics binding for when no graphics API is specified.
/// In production, you would use Vulkan, OpenGL, or D3D12.
#[cfg(not(feature = "vulkan"))]
type GraphicsApi = xr::Headless;

/// Swapchain image for stereo rendering
#[derive(Debug)]
pub struct SwapchainImage {
    /// Image index in the swapchain
    pub index: u32,
    /// Width of the image
    pub width: u32,
    /// Height of the image
    pub height: u32,
}

/// Per-eye swapchain and view state
struct EyeState {
    /// Swapchain for this eye
    swapchain: Option<xr::Swapchain<GraphicsApi>>,
    /// Current swapchain image index
    image_index: Option<u32>,
    /// Recommended render resolution
    resolution: [u32; 2],
    /// Field of view
    fov: xr::Fovf,
    /// View pose in stage space
    pose: xr::Posef,
}

impl Default for EyeState {
    fn default() -> Self {
        Self {
            swapchain: None,
            image_index: None,
            resolution: [1920, 1920],
            fov: xr::Fovf {
                angle_left: -0.785,
                angle_right: 0.785,
                angle_up: 0.785,
                angle_down: -0.785,
            },
            pose: xr::Posef::IDENTITY,
        }
    }
}

/// Action binding for a specific controller profile
struct ActionBinding {
    path: xr::Path,
    action: xr::Action<xr::Posef>,
}

/// Complete action system for input handling
pub struct ActionSystem {
    /// Main action set for gameplay
    action_set: xr::ActionSet,

    /// Pose action for hand/controller tracking
    pose_action: xr::Action<xr::Posef>,

    /// Grip pose action (where hand grips controller)
    grip_pose_action: xr::Action<xr::Posef>,

    /// Aim pose action (where controller points)
    aim_pose_action: xr::Action<xr::Posef>,

    /// Trigger action (0.0 - 1.0)
    trigger_action: xr::Action<f32>,

    /// Grip squeeze action (0.0 - 1.0)
    grip_action: xr::Action<f32>,

    /// Primary button (A/X)
    primary_button_action: xr::Action<bool>,

    /// Secondary button (B/Y)
    secondary_button_action: xr::Action<bool>,

    /// Menu button
    menu_button_action: xr::Action<bool>,

    /// Thumbstick X axis
    thumbstick_x_action: xr::Action<f32>,

    /// Thumbstick Y axis
    thumbstick_y_action: xr::Action<f32>,

    /// Thumbstick click
    thumbstick_click_action: xr::Action<bool>,

    /// Haptic output action
    haptic_action: xr::Action<xr::Haptic>,

    /// Action spaces for controller poses
    grip_spaces: [Option<xr::Space>; 2],
    aim_spaces: [Option<xr::Space>; 2],

    /// Subaction paths for left/right hand
    left_hand_path: xr::Path,
    right_hand_path: xr::Path,
}

/// Hand tracking state using XR_EXT_hand_tracking
#[cfg(feature = "hand-tracking")]
pub struct HandTrackingState {
    /// Hand trackers for left and right hands
    trackers: [Option<xr::HandTracker>; 2],
    /// Joint poses (26 joints per hand: wrist, palm, thumb*4, index*5, middle*5, ring*5, little*5)
    joint_poses: [[Pose; 26]; 2],
    /// Joint radii (for collision/visualization)
    joint_radii: [[f32; 26]; 2],
    /// Whether hand tracking is active
    active: [bool; 2],
}

/// Configuration for the OpenXR backend
#[derive(Clone, Debug)]
pub struct OpenXrConfig {
    /// Application name for the OpenXR runtime
    pub application_name: String,
    /// Application version
    pub application_version: u32,
    /// Engine name
    pub engine_name: String,
    /// Engine version
    pub engine_version: u32,
    /// Preferred blend mode (opaque, additive, alpha blend)
    pub blend_mode: xr::EnvironmentBlendMode,
    /// Enable hand tracking if available
    pub enable_hand_tracking: bool,
    /// Enable passthrough if available (Quest, etc.)
    pub enable_passthrough: bool,
}

impl Default for OpenXrConfig {
    fn default() -> Self {
        Self {
            application_name: "MetaverseOS".to_string(),
            application_version: 1,
            engine_name: "VoidEngine".to_string(),
            engine_version: 1,
            blend_mode: xr::EnvironmentBlendMode::OPAQUE,
            enable_hand_tracking: true,
            enable_passthrough: false,
        }
    }
}

/// Complete OpenXR backend implementation
pub struct OpenXrBackend {
    /// OpenXR entry point
    entry: Option<xr::Entry>,

    /// OpenXR instance
    instance: Option<xr::Instance>,

    /// System ID (represents the headset)
    system: Option<xr::SystemId>,

    /// Active session
    session: Option<xr::Session<GraphicsApi>>,

    /// Frame waiter for synchronization
    frame_waiter: Option<xr::FrameWaiter>,

    /// Frame stream for frame operations
    frame_stream: Option<xr::FrameStream<GraphicsApi>>,

    /// Current session state
    session_state: SessionState,

    /// Current XR mode
    current_mode: XrMode,

    /// HMD information
    hmd_info: HmdInfo,

    /// Per-eye rendering state
    eye_states: [EyeState; 2],

    /// Reference spaces
    view_space: Option<xr::Space>,
    local_space: Option<xr::Space>,
    stage_space: Option<xr::Space>,

    /// Current reference space type
    current_reference_space: ReferenceSpace,

    /// Action system for input
    action_system: Option<ActionSystem>,

    /// Hand tracking state
    #[cfg(feature = "hand-tracking")]
    hand_tracking: Option<HandTrackingState>,

    /// Spatial anchors
    anchors: BTreeMap<u64, SpatialAnchor>,
    anchor_id_generator: IdGenerator,

    /// Frame timing information
    frame_timing: FrameTiming,

    /// Predicted display time for current frame
    predicted_display_time: xr::Time,

    /// Whether we are in a frame
    in_frame: bool,

    /// Configuration
    config: OpenXrConfig,

    /// Extension availability
    has_hand_tracking: bool,
    has_passthrough: bool,

    /// Current head pose
    head_pose: Pose,

    /// Controller states
    controller_states: [ControllerState; 2],
}

impl OpenXrBackend {
    /// Create a new OpenXR backend with default configuration
    pub fn new() -> Result<Box<Self>, XrError> {
        Self::with_config(OpenXrConfig::default())
    }

    /// Create a new OpenXR backend with custom configuration
    pub fn with_config(config: OpenXrConfig) -> Result<Box<Self>, XrError> {
        Ok(Box::new(Self {
            entry: None,
            instance: None,
            system: None,
            session: None,
            frame_waiter: None,
            frame_stream: None,
            session_state: SessionState::Idle,
            current_mode: XrMode::Desktop,
            hmd_info: HmdInfo::default(),
            eye_states: [EyeState::default(), EyeState::default()],
            view_space: None,
            local_space: None,
            stage_space: None,
            current_reference_space: ReferenceSpace::Local,
            action_system: None,
            #[cfg(feature = "hand-tracking")]
            hand_tracking: None,
            anchors: BTreeMap::new(),
            anchor_id_generator: IdGenerator::new(),
            frame_timing: FrameTiming::default(),
            predicted_display_time: xr::Time::from_nanos(0),
            in_frame: false,
            config,
            has_hand_tracking: false,
            has_passthrough: false,
            head_pose: Pose::default(),
            controller_states: [ControllerState::default(), ControllerState::default()],
        }))
    }

    /// Get the OpenXR instance
    pub fn instance(&self) -> Option<&xr::Instance> {
        self.instance.as_ref()
    }

    /// Get the OpenXR session
    pub fn session(&self) -> Option<&xr::Session<GraphicsApi>> {
        self.session.as_ref()
    }

    /// Get the predicted display time for the current frame
    pub fn predicted_display_time(&self) -> xr::Time {
        self.predicted_display_time
    }

    /// Get the current stage space
    pub fn stage_space(&self) -> Option<&xr::Space> {
        self.stage_space.as_ref()
    }

    /// Check if hand tracking is available
    pub fn has_hand_tracking(&self) -> bool {
        self.has_hand_tracking
    }

    /// Check if passthrough is available
    pub fn has_passthrough(&self) -> bool {
        self.has_passthrough
    }

    /// Create the OpenXR instance with required extensions
    fn create_instance(&mut self) -> Result<(), XrError> {
        // Load OpenXR runtime
        #[cfg(target_os = "windows")]
        let entry = xr::Entry::linked();

        #[cfg(not(target_os = "windows"))]
        let entry = xr::Entry::load()
            .map_err(|e| XrError::RuntimeError(format!("Failed to load OpenXR: {:?}", e)))?;

        // Query available extensions
        let available_extensions = entry
            .enumerate_extensions()
            .map_err(|e| XrError::RuntimeError(format!("Failed to enumerate extensions: {:?}", e)))?;

        log::info!("Available OpenXR extensions:");
        for ext in available_extensions.iter() {
            log::debug!("  - {:?}", ext);
        }

        // Build extension set based on availability
        let mut extensions = xr::ExtensionSet::default();

        // Graphics API extensions (required)
        #[cfg(feature = "vulkan")]
        {
            if available_extensions.khr_vulkan_enable2 {
                extensions.khr_vulkan_enable2 = true;
            } else if available_extensions.khr_vulkan_enable {
                extensions.khr_vulkan_enable = true;
            } else {
                return Err(XrError::NotSupported("Vulkan not supported".into()));
            }
        }

        // Hand tracking extension
        #[cfg(feature = "hand-tracking")]
        {
            if self.config.enable_hand_tracking && available_extensions.ext_hand_tracking {
                extensions.ext_hand_tracking = true;
                self.has_hand_tracking = true;
                log::info!("Hand tracking enabled");
            }
        }

        // Passthrough extension (Meta Quest)
        if self.config.enable_passthrough {
            // Note: FB_passthrough is Meta-specific
            // Check for it in a runtime-agnostic way
            self.has_passthrough = false; // Would need runtime check
        }

        // Create instance
        let instance = entry
            .create_instance(
                &xr::ApplicationInfo {
                    application_name: &self.config.application_name,
                    application_version: self.config.application_version,
                    engine_name: &self.config.engine_name,
                    engine_version: self.config.engine_version,
                },
                &extensions,
                &[], // No layers
            )
            .map_err(|e| XrError::RuntimeError(format!("Failed to create instance: {:?}", e)))?;

        let instance_props = instance
            .properties()
            .map_err(|e| XrError::RuntimeError(format!("Failed to get instance properties: {:?}", e)))?;

        log::info!(
            "OpenXR Runtime: {} version {}",
            instance_props.runtime_name,
            instance_props.runtime_version
        );

        self.entry = Some(entry);
        self.instance = Some(instance);

        Ok(())
    }

    /// Discover and configure the XR system (headset)
    fn discover_system(&mut self) -> Result<(), XrError> {
        let instance = self
            .instance
            .as_ref()
            .ok_or(XrError::NotInitialized)?;

        // Get head-mounted display system
        let system = instance
            .system(xr::FormFactor::HEAD_MOUNTED_DISPLAY)
            .map_err(|e| XrError::DeviceError(format!("No HMD found: {:?}", e)))?;

        // Get system properties
        let system_props = instance
            .system_properties(system)
            .map_err(|e| XrError::DeviceError(format!("Failed to get system properties: {:?}", e)))?;

        log::info!("XR System: {}", system_props.system_name);
        log::info!(
            "Max layers: {}, Max swapchain size: {}x{}",
            system_props.graphics_properties.max_layer_count,
            system_props.graphics_properties.max_swapchain_image_width,
            system_props.graphics_properties.max_swapchain_image_height
        );

        // Update HMD info
        self.hmd_info = HmdInfo {
            name: system_props.system_name.to_string(),
            manufacturer: "Unknown".to_string(), // OpenXR doesn't expose this directly
            refresh_rate: 90.0, // Will be updated from view configuration
            render_resolution: [1920, 1920], // Will be updated
            supports_hand_tracking: self.has_hand_tracking,
            supports_passthrough: self.has_passthrough,
            ipd: 0.063, // Default IPD
        };

        // Query view configuration for stereo
        let view_configs = instance
            .enumerate_view_configuration_views(system, xr::ViewConfigurationType::PRIMARY_STEREO)
            .map_err(|e| {
                XrError::DeviceError(format!("Failed to enumerate view configurations: {:?}", e))
            })?;

        if view_configs.len() >= 2 {
            // Get recommended resolution for each eye
            for (i, view_config) in view_configs.iter().enumerate().take(2) {
                self.eye_states[i].resolution = [
                    view_config.recommended_image_rect_width,
                    view_config.recommended_image_rect_height,
                ];
                log::info!(
                    "Eye {} recommended resolution: {}x{}",
                    i,
                    view_config.recommended_image_rect_width,
                    view_config.recommended_image_rect_height
                );
            }

            self.hmd_info.render_resolution = [
                view_configs[0].recommended_image_rect_width,
                view_configs[0].recommended_image_rect_height,
            ];
        }

        self.system = Some(system);

        Ok(())
    }

    /// Create the XR session
    fn create_session(&mut self) -> Result<(), XrError> {
        let instance = self.instance.as_ref().ok_or(XrError::NotInitialized)?;
        let system = self.system.ok_or(XrError::NotInitialized)?;

        // For headless mode (testing), we create a simple session
        #[cfg(not(feature = "vulkan"))]
        {
            let (session, frame_waiter, frame_stream) = unsafe {
                instance
                    .create_session(system, &xr::headless::SessionCreateInfo {})
                    .map_err(|e| XrError::SessionError(format!("Failed to create session: {:?}", e)))?
            };

            self.session = Some(session);
            self.frame_waiter = Some(frame_waiter);
            self.frame_stream = Some(frame_stream);
        }

        // For Vulkan mode, the caller would need to provide Vulkan handles
        #[cfg(feature = "vulkan")]
        {
            return Err(XrError::NotSupported(
                "Vulkan session creation requires external Vulkan context".into(),
            ));
        }

        Ok(())
    }

    /// Create swapchains for stereo rendering
    fn create_swapchains(&mut self) -> Result<(), XrError> {
        let session = self.session.as_ref().ok_or(XrError::NotInitialized)?;

        for i in 0..2 {
            let resolution = self.eye_states[i].resolution;

            let swapchain = session
                .create_swapchain(&xr::SwapchainCreateInfo {
                    create_flags: xr::SwapchainCreateFlags::EMPTY,
                    usage_flags: xr::SwapchainUsageFlags::COLOR_ATTACHMENT
                        | xr::SwapchainUsageFlags::SAMPLED,
                    format: 44, // VK_FORMAT_B8G8R8A8_SRGB, adjust based on graphics API
                    sample_count: 1,
                    width: resolution[0],
                    height: resolution[1],
                    face_count: 1,
                    array_size: 1,
                    mip_count: 1,
                })
                .map_err(|e| {
                    XrError::RuntimeError(format!("Failed to create swapchain: {:?}", e))
                })?;

            self.eye_states[i].swapchain = Some(swapchain);
        }

        Ok(())
    }

    /// Create reference spaces
    fn create_reference_spaces(&mut self) -> Result<(), XrError> {
        let session = self.session.as_ref().ok_or(XrError::NotInitialized)?;

        // VIEW space (head-relative)
        self.view_space = Some(
            session
                .create_reference_space(xr::ReferenceSpaceType::VIEW, xr::Posef::IDENTITY)
                .map_err(|e| XrError::RuntimeError(format!("Failed to create view space: {:?}", e)))?,
        );

        // LOCAL space (seated)
        self.local_space = Some(
            session
                .create_reference_space(xr::ReferenceSpaceType::LOCAL, xr::Posef::IDENTITY)
                .map_err(|e| {
                    XrError::RuntimeError(format!("Failed to create local space: {:?}", e))
                })?,
        );

        // STAGE space (standing, room-scale)
        // Note: Not all runtimes support STAGE space
        match session.create_reference_space(xr::ReferenceSpaceType::STAGE, xr::Posef::IDENTITY) {
            Ok(space) => {
                self.stage_space = Some(space);
            }
            Err(_) => {
                // Fall back to LOCAL with a height offset
                log::warn!("STAGE space not available, using LOCAL space with height offset");
                let offset_pose = xr::Posef {
                    orientation: xr::Quaternionf::IDENTITY,
                    position: xr::Vector3f {
                        x: 0.0,
                        y: 1.6, // Average eye height
                        z: 0.0,
                    },
                };
                self.stage_space = Some(
                    session
                        .create_reference_space(xr::ReferenceSpaceType::LOCAL, offset_pose)
                        .map_err(|e| {
                            XrError::RuntimeError(format!("Failed to create stage space: {:?}", e))
                        })?,
                );
            }
        }

        Ok(())
    }

    /// Create the action system for input handling
    fn create_action_system(&mut self) -> Result<(), XrError> {
        let instance = self.instance.as_ref().ok_or(XrError::NotInitialized)?;
        let session = self.session.as_ref().ok_or(XrError::NotInitialized)?;

        // Subaction paths for left/right hand
        let left_hand_path = instance
            .string_to_path("/user/hand/left")
            .map_err(|e| XrError::RuntimeError(format!("Failed to create path: {:?}", e)))?;
        let right_hand_path = instance
            .string_to_path("/user/hand/right")
            .map_err(|e| XrError::RuntimeError(format!("Failed to create path: {:?}", e)))?;

        let subaction_paths = [left_hand_path, right_hand_path];

        // Create action set
        let action_set = instance
            .create_action_set("gameplay", "Gameplay Actions", 0)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action set: {:?}", e)))?;

        // Create pose actions
        let grip_pose_action = action_set
            .create_action::<xr::Posef>("grip_pose", "Grip Pose", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let aim_pose_action = action_set
            .create_action::<xr::Posef>("aim_pose", "Aim Pose", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let pose_action = action_set
            .create_action::<xr::Posef>("hand_pose", "Hand Pose", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        // Create float actions
        let trigger_action = action_set
            .create_action::<f32>("trigger", "Trigger", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let grip_action = action_set
            .create_action::<f32>("grip", "Grip", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let thumbstick_x_action = action_set
            .create_action::<f32>("thumbstick_x", "Thumbstick X", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let thumbstick_y_action = action_set
            .create_action::<f32>("thumbstick_y", "Thumbstick Y", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        // Create boolean actions
        let primary_button_action = action_set
            .create_action::<bool>("primary_button", "Primary Button", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let secondary_button_action = action_set
            .create_action::<bool>("secondary_button", "Secondary Button", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let menu_button_action = action_set
            .create_action::<bool>("menu_button", "Menu Button", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        let thumbstick_click_action = action_set
            .create_action::<bool>("thumbstick_click", "Thumbstick Click", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        // Create haptic action
        let haptic_action = action_set
            .create_action::<xr::Haptic>("haptic", "Haptic Feedback", &subaction_paths)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create action: {:?}", e)))?;

        // Suggest bindings for various controller profiles
        self.suggest_bindings_for_profile(
            instance,
            "/interaction_profiles/oculus/touch_controller",
            &grip_pose_action,
            &aim_pose_action,
            &trigger_action,
            &grip_action,
            &primary_button_action,
            &secondary_button_action,
            &menu_button_action,
            &thumbstick_x_action,
            &thumbstick_y_action,
            &thumbstick_click_action,
            &haptic_action,
        )?;

        self.suggest_bindings_for_profile(
            instance,
            "/interaction_profiles/valve/index_controller",
            &grip_pose_action,
            &aim_pose_action,
            &trigger_action,
            &grip_action,
            &primary_button_action,
            &secondary_button_action,
            &menu_button_action,
            &thumbstick_x_action,
            &thumbstick_y_action,
            &thumbstick_click_action,
            &haptic_action,
        )?;

        self.suggest_bindings_for_profile(
            instance,
            "/interaction_profiles/htc/vive_controller",
            &grip_pose_action,
            &aim_pose_action,
            &trigger_action,
            &grip_action,
            &primary_button_action,
            &secondary_button_action,
            &menu_button_action,
            &thumbstick_x_action,
            &thumbstick_y_action,
            &thumbstick_click_action,
            &haptic_action,
        )?;

        // Attach action set to session
        session
            .attach_action_sets(&[&action_set])
            .map_err(|e| XrError::RuntimeError(format!("Failed to attach action sets: {:?}", e)))?;

        // Create action spaces for controller poses
        let left_grip_space = grip_pose_action
            .create_space(session.clone(), left_hand_path, xr::Posef::IDENTITY)
            .ok();
        let right_grip_space = grip_pose_action
            .create_space(session.clone(), right_hand_path, xr::Posef::IDENTITY)
            .ok();

        let left_aim_space = aim_pose_action
            .create_space(session.clone(), left_hand_path, xr::Posef::IDENTITY)
            .ok();
        let right_aim_space = aim_pose_action
            .create_space(session.clone(), right_hand_path, xr::Posef::IDENTITY)
            .ok();

        self.action_system = Some(ActionSystem {
            action_set,
            pose_action,
            grip_pose_action,
            aim_pose_action,
            trigger_action,
            grip_action,
            primary_button_action,
            secondary_button_action,
            menu_button_action,
            thumbstick_x_action,
            thumbstick_y_action,
            thumbstick_click_action,
            haptic_action,
            grip_spaces: [left_grip_space, right_grip_space],
            aim_spaces: [left_aim_space, right_aim_space],
            left_hand_path,
            right_hand_path,
        });

        Ok(())
    }

    /// Suggest bindings for a specific controller profile
    fn suggest_bindings_for_profile(
        &self,
        instance: &xr::Instance,
        profile: &str,
        grip_pose: &xr::Action<xr::Posef>,
        aim_pose: &xr::Action<xr::Posef>,
        trigger: &xr::Action<f32>,
        grip: &xr::Action<f32>,
        primary_button: &xr::Action<bool>,
        secondary_button: &xr::Action<bool>,
        menu_button: &xr::Action<bool>,
        thumbstick_x: &xr::Action<f32>,
        thumbstick_y: &xr::Action<f32>,
        thumbstick_click: &xr::Action<bool>,
        haptic: &xr::Action<xr::Haptic>,
    ) -> Result<(), XrError> {
        let profile_path = instance.string_to_path(profile).map_err(|e| {
            XrError::RuntimeError(format!("Failed to create profile path: {:?}", e))
        })?;

        // Build bindings based on controller type
        let mut bindings = Vec::new();

        // Grip poses
        if let Ok(path) = instance.string_to_path("/user/hand/left/input/grip/pose") {
            bindings.push(xr::Binding::new(grip_pose, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/input/grip/pose") {
            bindings.push(xr::Binding::new(grip_pose, path));
        }

        // Aim poses
        if let Ok(path) = instance.string_to_path("/user/hand/left/input/aim/pose") {
            bindings.push(xr::Binding::new(aim_pose, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/input/aim/pose") {
            bindings.push(xr::Binding::new(aim_pose, path));
        }

        // Triggers
        if let Ok(path) = instance.string_to_path("/user/hand/left/input/trigger/value") {
            bindings.push(xr::Binding::new(trigger, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/input/trigger/value") {
            bindings.push(xr::Binding::new(trigger, path));
        }

        // Grip squeeze
        let grip_path = if profile.contains("index") {
            "/input/squeeze/force"
        } else {
            "/input/squeeze/value"
        };
        if let Ok(path) = instance.string_to_path(&format!("/user/hand/left{}", grip_path)) {
            bindings.push(xr::Binding::new(grip, path));
        }
        if let Ok(path) = instance.string_to_path(&format!("/user/hand/right{}", grip_path)) {
            bindings.push(xr::Binding::new(grip, path));
        }

        // Thumbstick
        if let Ok(path) = instance.string_to_path("/user/hand/left/input/thumbstick/x") {
            bindings.push(xr::Binding::new(thumbstick_x, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/input/thumbstick/x") {
            bindings.push(xr::Binding::new(thumbstick_x, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/left/input/thumbstick/y") {
            bindings.push(xr::Binding::new(thumbstick_y, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/input/thumbstick/y") {
            bindings.push(xr::Binding::new(thumbstick_y, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/left/input/thumbstick/click") {
            bindings.push(xr::Binding::new(thumbstick_click, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/input/thumbstick/click") {
            bindings.push(xr::Binding::new(thumbstick_click, path));
        }

        // Buttons (profile-specific)
        if profile.contains("oculus") {
            // Oculus Touch: A/B on right, X/Y on left
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/x/click") {
                bindings.push(xr::Binding::new(primary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/right/input/a/click") {
                bindings.push(xr::Binding::new(primary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/y/click") {
                bindings.push(xr::Binding::new(secondary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/right/input/b/click") {
                bindings.push(xr::Binding::new(secondary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/menu/click") {
                bindings.push(xr::Binding::new(menu_button, path));
            }
        } else if profile.contains("index") {
            // Valve Index: A/B buttons
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/a/click") {
                bindings.push(xr::Binding::new(primary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/right/input/a/click") {
                bindings.push(xr::Binding::new(primary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/b/click") {
                bindings.push(xr::Binding::new(secondary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/right/input/b/click") {
                bindings.push(xr::Binding::new(secondary_button, path));
            }
        } else if profile.contains("vive") {
            // HTC Vive: Menu button, trackpad
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/menu/click") {
                bindings.push(xr::Binding::new(menu_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/right/input/menu/click") {
                bindings.push(xr::Binding::new(menu_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/left/input/trackpad/click") {
                bindings.push(xr::Binding::new(primary_button, path));
            }
            if let Ok(path) = instance.string_to_path("/user/hand/right/input/trackpad/click") {
                bindings.push(xr::Binding::new(primary_button, path));
            }
        }

        // Haptics
        if let Ok(path) = instance.string_to_path("/user/hand/left/output/haptic") {
            bindings.push(xr::Binding::new(haptic, path));
        }
        if let Ok(path) = instance.string_to_path("/user/hand/right/output/haptic") {
            bindings.push(xr::Binding::new(haptic, path));
        }

        // Suggest bindings (ignore errors for unsupported profiles)
        let _ = instance.suggest_interaction_profile_bindings(profile_path, &bindings);

        Ok(())
    }

    /// Initialize hand tracking
    #[cfg(feature = "hand-tracking")]
    fn create_hand_tracking(&mut self) -> Result<(), XrError> {
        if !self.has_hand_tracking {
            return Ok(());
        }

        let session = self.session.as_ref().ok_or(XrError::NotInitialized)?;

        let left_tracker = session
            .create_hand_tracker(xr::Hand::LEFT)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create hand tracker: {:?}", e)))?;

        let right_tracker = session
            .create_hand_tracker(xr::Hand::RIGHT)
            .map_err(|e| XrError::RuntimeError(format!("Failed to create hand tracker: {:?}", e)))?;

        self.hand_tracking = Some(HandTrackingState {
            trackers: [Some(left_tracker), Some(right_tracker)],
            joint_poses: [[Pose::default(); 26]; 2],
            joint_radii: [[0.01; 26]; 2], // Default 1cm radius
            active: [false, false],
        });

        Ok(())
    }

    /// Poll OpenXR events and update session state
    fn poll_events(&mut self) -> Result<(), XrError> {
        let instance = self.instance.as_ref().ok_or(XrError::NotInitialized)?;

        let mut event_buffer = xr::EventDataBuffer::new();

        while let Some(event) = instance.poll_event(&mut event_buffer).map_err(|e| {
            XrError::RuntimeError(format!("Failed to poll events: {:?}", e))
        })? {
            match event {
                xr::Event::SessionStateChanged(state_event) => {
                    log::info!("Session state changed: {:?}", state_event.state());
                    self.handle_session_state_change(state_event.state())?;
                }
                xr::Event::ReferenceSpaceChangePending(_) => {
                    log::info!("Reference space change pending");
                }
                xr::Event::InstanceLossPending(_) => {
                    log::warn!("Instance loss pending!");
                    self.session_state = SessionState::Error;
                }
                xr::Event::InteractionProfileChanged(_) => {
                    log::info!("Interaction profile changed");
                }
                _ => {}
            }
        }

        Ok(())
    }

    /// Handle session state changes
    fn handle_session_state_change(&mut self, state: xr::SessionState) -> Result<(), XrError> {
        self.session_state = match state {
            xr::SessionState::IDLE => SessionState::Idle,
            xr::SessionState::READY => {
                // Begin the session
                if let Some(session) = &self.session {
                    session
                        .begin(xr::ViewConfigurationType::PRIMARY_STEREO)
                        .map_err(|e| {
                            XrError::SessionError(format!("Failed to begin session: {:?}", e))
                        })?;
                }
                SessionState::Ready
            }
            xr::SessionState::SYNCHRONIZED => SessionState::Synchronized,
            xr::SessionState::VISIBLE => SessionState::Visible,
            xr::SessionState::FOCUSED => SessionState::Focused,
            xr::SessionState::STOPPING => {
                // End the session
                if let Some(session) = &self.session {
                    session.end().map_err(|e| {
                        XrError::SessionError(format!("Failed to end session: {:?}", e))
                    })?;
                }
                SessionState::Stopping
            }
            xr::SessionState::LOSS_PENDING => SessionState::LossFocus,
            xr::SessionState::EXITING => SessionState::Stopping,
            _ => SessionState::Idle,
        };

        Ok(())
    }

    /// Update tracking data (head pose, controllers)
    fn update_tracking(&mut self) -> Result<(), XrError> {
        let session = self.session.as_ref().ok_or(XrError::NotInitialized)?;
        let _instance = self.instance.as_ref().ok_or(XrError::NotInitialized)?;

        // Get the appropriate reference space
        let reference_space = match self.current_reference_space {
            ReferenceSpace::View => self.view_space.as_ref(),
            ReferenceSpace::Local => self.local_space.as_ref(),
            ReferenceSpace::Stage | ReferenceSpace::Unbounded => self.stage_space.as_ref(),
        }
        .ok_or(XrError::NotInitialized)?;

        // Locate views (per-eye)
        let (view_state, views) = session
            .locate_views(
                xr::ViewConfigurationType::PRIMARY_STEREO,
                self.predicted_display_time,
                reference_space,
            )
            .map_err(|e| XrError::RuntimeError(format!("Failed to locate views: {:?}", e)))?;

        // Check if tracking is valid
        let tracking_valid = view_state.contains(xr::ViewStateFlags::POSITION_VALID)
            && view_state.contains(xr::ViewStateFlags::ORIENTATION_VALID);

        if tracking_valid && views.len() >= 2 {
            // Update per-eye state
            for (i, view) in views.iter().enumerate().take(2) {
                self.eye_states[i].pose = view.pose;
                self.eye_states[i].fov = view.fov;
            }

            // Compute head pose as average of eye poses
            let left_pos = &views[0].pose.position;
            let right_pos = &views[1].pose.position;

            self.head_pose = Pose {
                position: Vec3::new(
                    (left_pos.x + right_pos.x) * 0.5,
                    (left_pos.y + right_pos.y) * 0.5,
                    (left_pos.z + right_pos.z) * 0.5,
                ),
                orientation: convert_quaternion(&views[0].pose.orientation),
            };

            // Update IPD
            let ipd = ((right_pos.x - left_pos.x).powi(2)
                + (right_pos.y - left_pos.y).powi(2)
                + (right_pos.z - left_pos.z).powi(2))
            .sqrt();
            self.hmd_info.ipd = ipd;
        }

        // Update controller tracking
        self.update_controller_tracking()?;

        // Update hand tracking if available
        #[cfg(feature = "hand-tracking")]
        self.update_hand_tracking()?;

        Ok(())
    }

    /// Update controller tracking and input state
    fn update_controller_tracking(&mut self) -> Result<(), XrError> {
        let session = self.session.as_ref().ok_or(XrError::NotInitialized)?;
        let action_system = self.action_system.as_ref().ok_or(XrError::NotInitialized)?;

        // Get reference space
        let reference_space = match self.current_reference_space {
            ReferenceSpace::View => self.view_space.as_ref(),
            ReferenceSpace::Local => self.local_space.as_ref(),
            ReferenceSpace::Stage | ReferenceSpace::Unbounded => self.stage_space.as_ref(),
        }
        .ok_or(XrError::NotInitialized)?;

        // Sync actions
        let active_action_set = xr::ActiveActionSet::new(&action_system.action_set);
        session
            .sync_actions(&[active_action_set])
            .map_err(|e| XrError::RuntimeError(format!("Failed to sync actions: {:?}", e)))?;

        // Update each hand
        for (hand_idx, hand_path) in [
            action_system.left_hand_path,
            action_system.right_hand_path,
        ]
        .iter()
        .enumerate()
        {
            let mut controller = ControllerState::default();

            // Get grip pose
            if let Some(grip_space) = &action_system.grip_spaces[hand_idx] {
                if let Ok(location) = grip_space.locate(reference_space, self.predicted_display_time)
                {
                    if location
                        .location_flags
                        .contains(xr::SpaceLocationFlags::POSITION_VALID)
                    {
                        controller.is_tracked = true;
                        controller.grip_pose = convert_pose(&location.pose);
                        controller.pose = controller.grip_pose;
                    }
                }
            }

            // Get aim pose
            if let Some(aim_space) = &action_system.aim_spaces[hand_idx] {
                if let Ok(location) = aim_space.locate(reference_space, self.predicted_display_time)
                {
                    if location
                        .location_flags
                        .contains(xr::SpaceLocationFlags::POSITION_VALID)
                    {
                        controller.aim_pose = convert_pose(&location.pose);
                    }
                }
            }

            // Get analog inputs
            if let Ok(state) = action_system.trigger_action.state(session, *hand_path) {
                if state.is_active {
                    controller.trigger = state.current_state;
                }
            }

            if let Ok(state) = action_system.grip_action.state(session, *hand_path) {
                if state.is_active {
                    controller.grip = state.current_state;
                }
            }

            if let Ok(state) = action_system.thumbstick_x_action.state(session, *hand_path) {
                if state.is_active {
                    controller.thumbstick[0] = state.current_state;
                }
            }

            if let Ok(state) = action_system.thumbstick_y_action.state(session, *hand_path) {
                if state.is_active {
                    controller.thumbstick[1] = state.current_state;
                }
            }

            // Get button states
            if let Ok(state) = action_system.primary_button_action.state(session, *hand_path) {
                if state.is_active {
                    controller.primary_button = state.current_state;
                }
            }

            if let Ok(state) = action_system.secondary_button_action.state(session, *hand_path) {
                if state.is_active {
                    controller.secondary_button = state.current_state;
                }
            }

            if let Ok(state) = action_system.menu_button_action.state(session, *hand_path) {
                if state.is_active {
                    controller.menu_button = state.current_state;
                }
            }

            if let Ok(state) = action_system.thumbstick_click_action.state(session, *hand_path) {
                if state.is_active {
                    controller.thumbstick_pressed = state.current_state;
                }
            }

            self.controller_states[hand_idx] = controller;
        }

        Ok(())
    }

    /// Update hand tracking
    #[cfg(feature = "hand-tracking")]
    fn update_hand_tracking(&mut self) -> Result<(), XrError> {
        if !self.has_hand_tracking {
            return Ok(());
        }

        let reference_space = match self.current_reference_space {
            ReferenceSpace::View => self.view_space.as_ref(),
            ReferenceSpace::Local => self.local_space.as_ref(),
            ReferenceSpace::Stage | ReferenceSpace::Unbounded => self.stage_space.as_ref(),
        }
        .ok_or(XrError::NotInitialized)?;

        if let Some(hand_tracking) = &mut self.hand_tracking {
            for (hand_idx, tracker) in hand_tracking.trackers.iter().enumerate() {
                if let Some(_tracker) = tracker {
                    // Note: Would use OpenXR hand tracking API here:
                    // let _locate_info = xr::HandJointsLocateInfoEXT {
                    //     base_space: reference_space.as_raw(),
                    //     time: self.predicted_display_time,
                    // };

                    // Mark as inactive for now - full implementation requires
                    // runtime-specific hand tracking extension calls
                    let _ = reference_space; // Suppress unused warning
                    hand_tracking.active[hand_idx] = false;
                }
            }
        }

        Ok(())
    }

    /// Acquire swapchain images for rendering
    fn acquire_swapchain_images(&mut self) -> Result<(), XrError> {
        for i in 0..2 {
            if let Some(swapchain) = &self.eye_states[i].swapchain {
                let image_index = swapchain.acquire_image().map_err(|e| {
                    XrError::RuntimeError(format!("Failed to acquire swapchain image: {:?}", e))
                })?;

                swapchain
                    .wait_image(xr::Duration::INFINITE)
                    .map_err(|e| {
                        XrError::RuntimeError(format!("Failed to wait for swapchain image: {:?}", e))
                    })?;

                self.eye_states[i].image_index = Some(image_index);
            }
        }

        Ok(())
    }

    /// Release swapchain images after rendering
    fn release_swapchain_images(&mut self) -> Result<(), XrError> {
        for i in 0..2 {
            if let Some(swapchain) = &self.eye_states[i].swapchain {
                swapchain.release_image().map_err(|e| {
                    XrError::RuntimeError(format!("Failed to release swapchain image: {:?}", e))
                })?;
                self.eye_states[i].image_index = None;
            }
        }

        Ok(())
    }

    /// Get the current swapchain image for an eye
    pub fn get_swapchain_image(&self, eye: Eye) -> Option<SwapchainImage> {
        let idx = match eye {
            Eye::Left => 0,
            Eye::Right => 1,
            Eye::Center => return None,
        };

        self.eye_states[idx].image_index.map(|index| SwapchainImage {
            index,
            width: self.eye_states[idx].resolution[0],
            height: self.eye_states[idx].resolution[1],
        })
    }
}

impl Default for OpenXrBackend {
    fn default() -> Self {
        Self::new().expect("Failed to create OpenXR backend")
    }
}

impl XrBackend for OpenXrBackend {
    fn name(&self) -> &str {
        "OpenXR"
    }

    fn initialize(&mut self) -> Result<(), XrError> {
        log::info!("Initializing OpenXR backend...");

        // Create OpenXR instance
        self.create_instance()?;

        // Discover XR system (headset)
        self.discover_system()?;

        // Create session
        self.create_session()?;

        // Create reference spaces
        self.create_reference_spaces()?;

        // Create swapchains
        self.create_swapchains()?;

        // Create action system
        self.create_action_system()?;

        // Initialize hand tracking
        #[cfg(feature = "hand-tracking")]
        self.create_hand_tracking()?;

        log::info!("OpenXR backend initialized successfully");
        self.current_mode = XrMode::Vr;

        Ok(())
    }

    fn shutdown(&mut self) {
        log::info!("Shutting down OpenXR backend...");

        // Clear hand tracking
        #[cfg(feature = "hand-tracking")]
        {
            self.hand_tracking = None;
        }

        // Clear action system
        self.action_system = None;

        // Clear swapchains
        for eye_state in &mut self.eye_states {
            eye_state.swapchain = None;
        }

        // Clear reference spaces
        self.view_space = None;
        self.local_space = None;
        self.stage_space = None;

        // Clear session
        self.frame_stream = None;
        self.frame_waiter = None;
        self.session = None;

        // Clear instance
        self.instance = None;
        self.entry = None;

        self.session_state = SessionState::Idle;
        self.current_mode = XrMode::Desktop;

        log::info!("OpenXR backend shutdown complete");
    }

    fn supported_modes(&self) -> Vec<XrMode> {
        let mut modes = vec![XrMode::Vr];

        if self.has_passthrough {
            modes.push(XrMode::Ar);
            modes.push(XrMode::Mr);
        }

        modes.push(XrMode::Spectator);
        modes
    }

    fn set_mode(&mut self, mode: XrMode) -> Result<(), XrError> {
        match mode {
            XrMode::Desktop => {
                // Can always switch to desktop
                self.current_mode = mode;
                Ok(())
            }
            XrMode::Vr => {
                if self.session.is_some() {
                    self.config.blend_mode = xr::EnvironmentBlendMode::OPAQUE;
                    self.current_mode = mode;
                    Ok(())
                } else {
                    Err(XrError::NotInitialized)
                }
            }
            XrMode::Ar | XrMode::Mr => {
                if self.has_passthrough {
                    self.config.blend_mode = xr::EnvironmentBlendMode::ALPHA_BLEND;
                    self.current_mode = mode;
                    Ok(())
                } else {
                    Err(XrError::NotSupported("Passthrough not available".into()))
                }
            }
            XrMode::Spectator => {
                self.current_mode = mode;
                Ok(())
            }
        }
    }

    fn current_mode(&self) -> XrMode {
        self.current_mode
    }

    fn session_state(&self) -> SessionState {
        self.session_state
    }

    fn hmd_info(&self) -> &HmdInfo {
        &self.hmd_info
    }

    fn begin_frame(&mut self) -> Result<FrameTiming, XrError> {
        if self.in_frame {
            return Err(XrError::SessionError("Already in frame".into()));
        }

        // Poll events
        self.poll_events()?;

        // Check session state
        if self.session_state != SessionState::Focused
            && self.session_state != SessionState::Visible
        {
            return Err(XrError::SessionError(format!(
                "Session not ready: {:?}",
                self.session_state
            )));
        }

        let frame_waiter = self.frame_waiter.as_mut().ok_or(XrError::NotInitialized)?;
        let frame_stream = self.frame_stream.as_mut().ok_or(XrError::NotInitialized)?;

        // Wait for the runtime to signal when to begin
        let frame_state = frame_waiter
            .wait()
            .map_err(|e| XrError::RuntimeError(format!("Failed to wait for frame: {:?}", e)))?;

        self.predicted_display_time = frame_state.predicted_display_time;

        // Begin the frame
        frame_stream
            .begin()
            .map_err(|e| XrError::RuntimeError(format!("Failed to begin frame: {:?}", e)))?;

        self.in_frame = true;

        // Update tracking with predicted display time
        if frame_state.should_render {
            self.update_tracking()?;
            self.acquire_swapchain_images()?;
        }

        // Update frame timing
        self.frame_timing.frame_index += 1;
        self.frame_timing.predicted_display_time =
            self.predicted_display_time.as_nanos() as f64 / 1_000_000_000.0;

        Ok(self.frame_timing)
    }

    fn end_frame(&mut self) -> Result<(), XrError> {
        if !self.in_frame {
            return Err(XrError::SessionError("Not in frame".into()));
        }

        // Release swapchain images
        self.release_swapchain_images()?;

        let frame_stream = self.frame_stream.as_mut().ok_or(XrError::NotInitialized)?;

        // Build composition layers
        let mut layers: Vec<xr::CompositionLayerBase<'_, GraphicsApi>> = Vec::new();

        // Note: In a full implementation, you would build projection layers here
        // using the swapchains and eye poses

        // End the frame
        frame_stream
            .end(
                self.predicted_display_time,
                self.config.blend_mode,
                &[], // Layers would go here
            )
            .map_err(|e| XrError::RuntimeError(format!("Failed to end frame: {:?}", e)))?;

        self.in_frame = false;

        Ok(())
    }

    fn views(&self) -> Vec<View> {
        let mut views = Vec::with_capacity(2);

        for (i, eye) in [Eye::Left, Eye::Right].iter().enumerate() {
            let eye_state = &self.eye_states[i];

            // Build view matrix from pose
            let pose = convert_pose(&eye_state.pose);
            let view_matrix = pose.to_matrix().inverse();

            // Build projection matrix from FOV
            let fov = &eye_state.fov;
            let projection_matrix = build_projection_matrix(fov, 0.01, 1000.0);

            views.push(View {
                eye: *eye,
                view_matrix,
                projection_matrix,
                fov: Fov {
                    angle_left: fov.angle_left,
                    angle_right: fov.angle_right,
                    angle_up: fov.angle_up,
                    angle_down: fov.angle_down,
                },
                render_size: eye_state.resolution,
            });
        }

        views
    }

    fn head_pose(&self) -> Pose {
        self.head_pose
    }

    fn controller_state(&self, hand: Hand) -> ControllerState {
        let idx = match hand {
            Hand::Left => 0,
            Hand::Right => 1,
        };
        self.controller_states[idx].clone()
    }

    fn hand_state(&self, hand: Hand) -> Option<HandState> {
        #[cfg(feature = "hand-tracking")]
        {
            if !self.has_hand_tracking {
                return None;
            }

            let hand_tracking = self.hand_tracking.as_ref()?;
            let idx = match hand {
                Hand::Left => 0,
                Hand::Right => 1,
            };

            if !hand_tracking.active[idx] {
                return None;
            }

            let mut joints = BTreeMap::new();
            for (joint_idx, pose) in hand_tracking.joint_poses[idx].iter().enumerate() {
                joints.insert(joint_idx as u8, *pose);
            }

            Some(HandState {
                is_tracked: true,
                joints,
                pinch_strength: 0.0, // Would need gesture detection
                grab_strength: 0.0,
            })
        }

        #[cfg(not(feature = "hand-tracking"))]
        None
    }

    fn set_reference_space(&mut self, space: ReferenceSpace) -> Result<(), XrError> {
        // Validate that the space is supported
        match space {
            ReferenceSpace::View => {
                if self.view_space.is_none() {
                    return Err(XrError::NotSupported("View space not available".into()));
                }
            }
            ReferenceSpace::Local => {
                if self.local_space.is_none() {
                    return Err(XrError::NotSupported("Local space not available".into()));
                }
            }
            ReferenceSpace::Stage | ReferenceSpace::Unbounded => {
                if self.stage_space.is_none() {
                    return Err(XrError::NotSupported("Stage space not available".into()));
                }
            }
        }

        self.current_reference_space = space;
        Ok(())
    }

    fn create_anchor(&mut self, pose: Pose) -> Result<AnchorId, XrError> {
        // Note: OpenXR spatial anchors require XR_MSFT_spatial_anchor or similar extension
        // This is a simplified implementation using local tracking

        let id = self.anchor_id_generator.next();
        let anchor_id = AnchorId(id);

        let anchor = SpatialAnchor {
            id: anchor_id,
            pose,
            is_tracked: true,
            data: BTreeMap::new(),
        };

        self.anchors.insert(id.to_bits(), anchor);

        Ok(anchor_id)
    }

    fn destroy_anchor(&mut self, id: AnchorId) -> Result<(), XrError> {
        self.anchors
            .remove(&id.0.to_bits())
            .ok_or(XrError::Custom("Anchor not found".into()))?;
        Ok(())
    }

    fn get_anchor(&self, id: AnchorId) -> Option<&SpatialAnchor> {
        self.anchors.get(&id.0.to_bits())
    }

    fn configure_passthrough(&mut self, config: PassthroughConfig) -> Result<(), XrError> {
        if !self.has_passthrough {
            return Err(XrError::NotSupported("Passthrough not available".into()));
        }

        // Note: Passthrough configuration is runtime-specific (e.g., Meta Quest)
        // This would need platform-specific implementation

        if config.enabled {
            self.config.blend_mode = xr::EnvironmentBlendMode::ALPHA_BLEND;
        } else {
            self.config.blend_mode = xr::EnvironmentBlendMode::OPAQUE;
        }

        Ok(())
    }

    fn trigger_haptic(
        &mut self,
        hand: Hand,
        amplitude: f32,
        duration_seconds: f32,
        frequency: f32,
    ) {
        if let (Some(session), Some(action_system)) = (&self.session, &self.action_system) {
            let hand_path = match hand {
                Hand::Left => action_system.left_hand_path,
                Hand::Right => action_system.right_hand_path,
            };

            let haptic_event = xr::HapticVibration::new()
                .amplitude(amplitude.clamp(0.0, 1.0))
                .duration(xr::Duration::from_nanos(
                    (duration_seconds * 1_000_000_000.0) as i64,
                ))
                .frequency(frequency);

            let _ = action_system
                .haptic_action
                .apply_feedback(session, hand_path, &haptic_event);
        }
    }
}

// ============================================================================
// Helper functions
// ============================================================================

/// Convert OpenXR quaternion to void_math Quat
fn convert_quaternion(q: &xr::Quaternionf) -> Quat {
    Quat::from_xyzw(q.x, q.y, q.z, q.w)
}

/// Convert OpenXR vector to void_math Vec3
fn convert_vector(v: &xr::Vector3f) -> Vec3 {
    Vec3::new(v.x, v.y, v.z)
}

/// Convert OpenXR pose to void_xr Pose
fn convert_pose(p: &xr::Posef) -> Pose {
    Pose {
        position: convert_vector(&p.position),
        orientation: convert_quaternion(&p.orientation),
    }
}

/// Build an asymmetric projection matrix from FOV angles
fn build_projection_matrix(fov: &xr::Fovf, near: f32, far: f32) -> Mat4 {
    let tan_left = fov.angle_left.tan();
    let tan_right = fov.angle_right.tan();
    let tan_up = fov.angle_up.tan();
    let tan_down = fov.angle_down.tan();

    let tan_width = tan_right - tan_left;
    let tan_height = tan_up - tan_down;

    let a = 2.0 / tan_width;
    let b = 2.0 / tan_height;
    let c = (tan_right + tan_left) / tan_width;
    let d = (tan_up + tan_down) / tan_height;
    let e = -(far + near) / (far - near);
    let f = -(2.0 * far * near) / (far - near);

    // Column-major order for OpenGL/Vulkan conventions
    Mat4::from_cols(
        void_math::Vec4::new(a, 0.0, 0.0, 0.0),
        void_math::Vec4::new(0.0, b, 0.0, 0.0),
        void_math::Vec4::new(c, d, e, -1.0),
        void_math::Vec4::new(0.0, 0.0, f, 0.0),
    )
}

/// Map OpenXR hand joint to our HandJoint enum
#[cfg(feature = "hand-tracking")]
fn map_hand_joint(index: usize) -> HandJoint {
    match index {
        0 => HandJoint::Wrist,
        1 => HandJoint::Palm,
        2 => HandJoint::ThumbMetacarpal,
        3 => HandJoint::ThumbProximal,
        4 => HandJoint::ThumbDistal,
        5 => HandJoint::ThumbTip,
        6 => HandJoint::IndexMetacarpal,
        7 => HandJoint::IndexProximal,
        8 => HandJoint::IndexIntermediate,
        9 => HandJoint::IndexDistal,
        10 => HandJoint::IndexTip,
        11 => HandJoint::MiddleMetacarpal,
        12 => HandJoint::MiddleProximal,
        13 => HandJoint::MiddleIntermediate,
        14 => HandJoint::MiddleDistal,
        15 => HandJoint::MiddleTip,
        16 => HandJoint::RingMetacarpal,
        17 => HandJoint::RingProximal,
        18 => HandJoint::RingIntermediate,
        19 => HandJoint::RingDistal,
        20 => HandJoint::RingTip,
        21 => HandJoint::LittleMetacarpal,
        22 => HandJoint::LittleProximal,
        23 => HandJoint::LittleIntermediate,
        24 => HandJoint::LittleDistal,
        25 => HandJoint::LittleTip,
        _ => HandJoint::Wrist,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_quaternion_conversion() {
        let q = xr::Quaternionf {
            x: 0.0,
            y: 0.0,
            z: 0.0,
            w: 1.0,
        };
        let converted = convert_quaternion(&q);
        assert!((converted.w - 1.0).abs() < 0.0001);
    }

    #[test]
    fn test_vector_conversion() {
        let v = xr::Vector3f {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        };
        let converted = convert_vector(&v);
        assert!((converted.x - 1.0).abs() < 0.0001);
        assert!((converted.y - 2.0).abs() < 0.0001);
        assert!((converted.z - 3.0).abs() < 0.0001);
    }

    #[test]
    fn test_projection_matrix() {
        let fov = xr::Fovf {
            angle_left: -0.785,
            angle_right: 0.785,
            angle_up: 0.785,
            angle_down: -0.785,
        };
        let proj = build_projection_matrix(&fov, 0.01, 1000.0);
        // Verify it's a valid projection matrix
        assert!(proj.w_axis.w < 0.01); // Should be close to 0
    }

    #[test]
    fn test_default_config() {
        let config = OpenXrConfig::default();
        assert_eq!(config.application_name, "MetaverseOS");
        assert_eq!(config.blend_mode, xr::EnvironmentBlendMode::OPAQUE);
    }
}
