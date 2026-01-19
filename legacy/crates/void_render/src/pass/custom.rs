//! Custom Render Pass Trait
//!
//! Defines the interface for implementing custom render passes that can be
//! injected into the rendering pipeline.
//!
//! # Example
//!
//! ```ignore
//! use void_render::pass::custom::*;
//!
//! struct MyPostProcess {
//!     intensity: f32,
//! }
//!
//! impl CustomRenderPass for MyPostProcess {
//!     fn name(&self) -> &str { "my_post_process" }
//!
//!     fn dependencies(&self) -> &[&str] { &["main"] }
//!
//!     fn reads(&self) -> Vec<ResourceRef> {
//!         vec![ResourceRef::MainColor]
//!     }
//!
//!     fn writes(&self) -> Vec<ResourceRef> {
//!         vec![ResourceRef::MainColor]
//!     }
//!
//!     fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError> {
//!         // Apply post-processing...
//!         Ok(())
//!     }
//! }
//! ```

use alloc::string::String;
use alloc::vec::Vec;
use core::fmt;
use serde::{Deserialize, Serialize};

use crate::camera::Camera;

/// Reference to a render resource
#[derive(Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum ResourceRef {
    /// Main color buffer
    MainColor,

    /// Main depth buffer
    MainDepth,

    /// GBuffer channel
    GBuffer(GBufferChannel),

    /// Shadow atlas
    ShadowAtlas,

    /// Named texture
    Texture(String),

    /// Named buffer
    Buffer(String),

    /// Previous frame's color (for temporal effects)
    PreviousColor,

    /// Custom resource
    Custom(String),
}

impl ResourceRef {
    /// Create a texture reference
    pub fn texture(name: impl Into<String>) -> Self {
        Self::Texture(name.into())
    }

    /// Create a buffer reference
    pub fn buffer(name: impl Into<String>) -> Self {
        Self::Buffer(name.into())
    }

    /// Create a custom resource reference
    pub fn custom(name: impl Into<String>) -> Self {
        Self::Custom(name.into())
    }
}

/// GBuffer channel types
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum GBufferChannel {
    /// Albedo/base color
    Albedo,
    /// World-space normals
    Normal,
    /// Metallic and roughness
    MetallicRoughness,
    /// Depth buffer
    Depth,
    /// Motion vectors
    Motion,
    /// Emissive
    Emissive,
    /// Ambient occlusion
    AO,
}

/// Resource requirements for a custom pass
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ResourceRequirements {
    /// Estimated GPU memory usage in bytes
    pub memory_bytes: u64,

    /// Number of render targets required
    pub render_targets: u32,

    /// Whether compute capability is required
    pub compute: bool,

    /// Maximum execution time hint in milliseconds
    pub time_budget_ms: f32,

    /// Required texture formats
    pub texture_formats: Vec<TextureFormatHint>,

    /// Minimum required features
    pub features: PassFeatures,
}

impl Default for ResourceRequirements {
    fn default() -> Self {
        Self {
            memory_bytes: 0,
            render_targets: 0,
            compute: false,
            time_budget_ms: 1.0,
            texture_formats: Vec::new(),
            features: PassFeatures::default(),
        }
    }
}

/// Texture format hints for resource allocation
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum TextureFormatHint {
    /// RGBA 8-bit unorm
    #[default]
    Rgba8Unorm,
    /// RGBA 16-bit float
    Rgba16Float,
    /// RGBA 32-bit float
    Rgba32Float,
    /// Depth 32-bit float
    Depth32Float,
    /// Depth 24 + Stencil 8
    Depth24Stencil8,
    /// R8 unorm (single channel)
    R8Unorm,
    /// R16 float
    R16Float,
    /// RG16 float
    Rg16Float,
}

/// Feature flags for custom passes
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct PassFeatures {
    /// Requires compute shaders
    pub compute_shaders: bool,
    /// Requires storage textures
    pub storage_textures: bool,
    /// Requires indirect drawing
    pub indirect_draw: bool,
    /// Requires timestamp queries
    pub timestamp_queries: bool,
    /// Requires multiview rendering
    pub multiview: bool,
}

/// Pass execution priority
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub struct PassPriority(pub i32);

impl Default for PassPriority {
    fn default() -> Self {
        Self(0)
    }
}

impl PassPriority {
    /// Pre-depth passes
    pub const PRE_DEPTH: Self = Self(-100);
    /// Shadow passes
    pub const SHADOW: Self = Self(-50);
    /// Main geometry passes
    pub const MAIN: Self = Self(0);
    /// Transparent passes
    pub const TRANSPARENT: Self = Self(50);
    /// Post-process passes
    pub const POST_PROCESS: Self = Self(100);
    /// UI/overlay passes
    pub const UI: Self = Self(200);
}

/// Camera render data for passes
#[derive(Clone, Debug)]
pub struct CameraRenderData {
    /// View matrix
    pub view_matrix: [[f32; 4]; 4],
    /// Projection matrix
    pub projection_matrix: [[f32; 4]; 4],
    /// View-projection matrix
    pub view_projection: [[f32; 4]; 4],
    /// Inverse view matrix
    pub inverse_view: [[f32; 4]; 4],
    /// Inverse projection matrix
    pub inverse_projection: [[f32; 4]; 4],
    /// Camera world position
    pub position: [f32; 3],
    /// Camera forward direction
    pub forward: [f32; 3],
    /// Near plane distance
    pub near: f32,
    /// Far plane distance
    pub far: f32,
    /// Field of view (if perspective)
    pub fov: f32,
    /// Aspect ratio
    pub aspect: f32,
}

impl Default for CameraRenderData {
    fn default() -> Self {
        let identity = [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ];
        Self {
            view_matrix: identity,
            projection_matrix: identity,
            view_projection: identity,
            inverse_view: identity,
            inverse_projection: identity,
            position: [0.0, 0.0, 0.0],
            forward: [0.0, 0.0, -1.0],
            near: 0.1,
            far: 1000.0,
            fov: 60.0_f32.to_radians(),
            aspect: 16.0 / 9.0,
        }
    }
}

impl CameraRenderData {
    /// Create from a Camera
    pub fn from_camera(camera: &Camera) -> Self {
        let view = camera.view_matrix();
        let proj = camera.projection_matrix();
        let vp = camera.view_projection_matrix();

        let (near, far, fov, aspect) = match camera.projection {
            crate::camera::Projection::Perspective { fov, aspect, near, far } => {
                (near, far, fov, aspect)
            }
            crate::camera::Projection::Orthographic { near, far, left, right, bottom, top } => {
                let width = right - left;
                let height = top - bottom;
                (near, far, 0.0, width / height)
            }
        };

        Self {
            view_matrix: mat4_to_array(view),
            projection_matrix: mat4_to_array(proj),
            view_projection: mat4_to_array(vp),
            inverse_view: mat4_to_array(view.inverse()),
            inverse_projection: mat4_to_array(proj.inverse()),
            position: [camera.position.x, camera.position.y, camera.position.z],
            forward: {
                let f = camera.forward();
                [f.x, f.y, f.z]
            },
            near,
            far,
            fov,
            aspect,
        }
    }
}

fn mat4_to_array(m: void_math::Mat4) -> [[f32; 4]; 4] {
    [
        [m.cols[0].x, m.cols[0].y, m.cols[0].z, m.cols[0].w],
        [m.cols[1].x, m.cols[1].y, m.cols[1].z, m.cols[1].w],
        [m.cols[2].x, m.cols[2].y, m.cols[2].z, m.cols[2].w],
        [m.cols[3].x, m.cols[3].y, m.cols[3].z, m.cols[3].w],
    ]
}

/// Abstract texture view handle
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct TextureViewHandle(pub u64);

/// Abstract buffer handle
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct BufferHandle(pub u64);

/// Abstract pipeline handle
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct PipelineHandle(pub u64);

/// Resources available to passes
#[derive(Clone, Debug, Default)]
pub struct PassResources {
    /// Available textures by name
    textures: alloc::collections::BTreeMap<String, TextureViewHandle>,
    /// Available buffers by name
    buffers: alloc::collections::BTreeMap<String, BufferHandle>,
    /// Surface dimensions
    pub surface_size: (u32, u32),
    /// Surface format
    pub surface_format: TextureFormatHint,
}

impl PassResources {
    /// Create new pass resources
    pub fn new() -> Self {
        Self::default()
    }

    /// Get a texture by name
    pub fn get_texture(&self, name: &str) -> Option<TextureViewHandle> {
        self.textures.get(name).copied()
    }

    /// Get a buffer by name
    pub fn get_buffer(&self, name: &str) -> Option<BufferHandle> {
        self.buffers.get(name).copied()
    }

    /// Register a texture
    pub fn register_texture(&mut self, name: impl Into<String>, handle: TextureViewHandle) {
        self.textures.insert(name.into(), handle);
    }

    /// Register a buffer
    pub fn register_buffer(&mut self, name: impl Into<String>, handle: BufferHandle) {
        self.buffers.insert(name.into(), handle);
    }

    /// Get main color texture
    pub fn main_color(&self) -> Option<TextureViewHandle> {
        self.get_texture("main_color")
    }

    /// Get main depth texture
    pub fn main_depth(&self) -> Option<TextureViewHandle> {
        self.get_texture("main_depth")
    }

    /// Get shadow atlas texture
    pub fn shadow_atlas(&self) -> Option<TextureViewHandle> {
        self.get_texture("shadow_atlas")
    }

    /// Get GBuffer texture
    pub fn gbuffer(&self, channel: GBufferChannel) -> Option<TextureViewHandle> {
        let name = match channel {
            GBufferChannel::Albedo => "gbuffer_albedo",
            GBufferChannel::Normal => "gbuffer_normal",
            GBufferChannel::MetallicRoughness => "gbuffer_metallic_roughness",
            GBufferChannel::Depth => "gbuffer_depth",
            GBufferChannel::Motion => "gbuffer_motion",
            GBufferChannel::Emissive => "gbuffer_emissive",
            GBufferChannel::AO => "gbuffer_ao",
        };
        self.get_texture(name)
    }

    /// List all available textures
    pub fn texture_names(&self) -> impl Iterator<Item = &str> {
        self.textures.keys().map(|s| s.as_str())
    }

    /// List all available buffers
    pub fn buffer_names(&self) -> impl Iterator<Item = &str> {
        self.buffers.keys().map(|s| s.as_str())
    }
}

/// Context for pass setup
#[derive(Clone, Debug)]
pub struct PassSetupContext {
    /// Surface dimensions
    pub surface_size: (u32, u32),
    /// Surface format
    pub surface_format: TextureFormatHint,
    /// Available features
    pub features: PassFeatures,
    /// Maximum allowed memory
    pub max_memory: u64,
}

impl Default for PassSetupContext {
    fn default() -> Self {
        Self {
            surface_size: (1920, 1080),
            surface_format: TextureFormatHint::Rgba8Unorm,
            features: PassFeatures::default(),
            max_memory: 256 * 1024 * 1024, // 256 MB
        }
    }
}

/// Context for pass execution
#[derive(Clone, Debug)]
pub struct PassExecuteContext {
    /// Available resources
    pub resources: PassResources,
    /// Camera data
    pub camera: CameraRenderData,
    /// Current time in seconds
    pub time: f32,
    /// Delta time since last frame
    pub delta_time: f32,
    /// Frame number
    pub frame: u64,
    /// Surface dimensions
    pub surface_size: (u32, u32),
}

impl Default for PassExecuteContext {
    fn default() -> Self {
        Self {
            resources: PassResources::default(),
            camera: CameraRenderData::default(),
            time: 0.0,
            delta_time: 0.016,
            frame: 0,
            surface_size: (1920, 1080),
        }
    }
}

/// Errors that can occur during pass operations
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum PassError {
    /// Error during pass setup
    Setup(String),
    /// Error during pass execution
    Execute(String),
    /// Resource not found or unavailable
    Resource(String),
    /// Resource budget exceeded
    Budget(String),
    /// Dependency error
    Dependency(String),
    /// Feature not supported
    UnsupportedFeature(String),
    /// Pass was disabled due to repeated failures
    Disabled(String),
    /// Partial failure with list of (pass_name, error)
    PartialFailure(Vec<(String, String)>),
}

impl fmt::Display for PassError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Setup(msg) => write!(f, "Setup error: {}", msg),
            Self::Execute(msg) => write!(f, "Execute error: {}", msg),
            Self::Resource(msg) => write!(f, "Resource error: {}", msg),
            Self::Budget(msg) => write!(f, "Budget error: {}", msg),
            Self::Dependency(msg) => write!(f, "Dependency error: {}", msg),
            Self::UnsupportedFeature(msg) => write!(f, "Unsupported feature: {}", msg),
            Self::Disabled(msg) => write!(f, "Pass disabled: {}", msg),
            Self::PartialFailure(failures) => {
                write!(f, "Partial failure: ")?;
                for (name, err) in failures {
                    write!(f, "[{}: {}] ", name, err)?;
                }
                Ok(())
            }
        }
    }
}

/// Trait for implementing custom render passes
pub trait CustomRenderPass: Send + Sync {
    /// Get the unique name of this pass
    fn name(&self) -> &str;

    /// Get dependencies on other passes (must run after these)
    fn dependencies(&self) -> &[&str] {
        &[]
    }

    /// Get resources this pass reads from
    fn reads(&self) -> Vec<ResourceRef> {
        Vec::new()
    }

    /// Get resources this pass writes to
    fn writes(&self) -> Vec<ResourceRef> {
        Vec::new()
    }

    /// Called once when pass is registered
    fn setup(&mut self, _context: &PassSetupContext) -> Result<(), PassError> {
        Ok(())
    }

    /// Called each frame to execute the pass
    fn execute(&self, context: &PassExecuteContext) -> Result<(), PassError>;

    /// Called when pass is unregistered or reset
    fn cleanup(&mut self) {}

    /// Get resource requirements for this pass
    fn resource_requirements(&self) -> ResourceRequirements {
        ResourceRequirements::default()
    }

    /// Get execution priority
    fn priority(&self) -> PassPriority {
        PassPriority::POST_PROCESS
    }

    /// Check if this pass is currently enabled
    fn is_enabled(&self) -> bool {
        true
    }

    /// Enable or disable this pass
    fn set_enabled(&mut self, _enabled: bool) {}

    /// Called when the surface is resized
    fn on_resize(&mut self, _new_size: (u32, u32)) {}

    /// Get pass configuration for serialization
    fn get_config(&self) -> Option<PassConfigData> {
        None
    }

    /// Apply configuration from deserialization
    fn apply_config(&mut self, _config: &PassConfigData) -> Result<(), PassError> {
        Ok(())
    }
}

/// Serializable pass configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PassConfigData {
    /// Pass name
    pub name: String,
    /// Enabled state
    pub enabled: bool,
    /// Priority
    pub priority: i32,
    /// Pass-specific configuration as JSON
    pub config: serde_json::Value,
}

impl Default for PassConfigData {
    fn default() -> Self {
        Self {
            name: String::new(),
            enabled: true,
            priority: 0,
            config: serde_json::Value::Null,
        }
    }
}

/// State for hot-reload serialization
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CustomPassState {
    /// Pass configurations
    pub configs: Vec<PassConfigData>,
    /// Enabled passes
    pub enabled: Vec<String>,
    /// Pass execution order
    pub order: Vec<String>,
}

impl Default for CustomPassState {
    fn default() -> Self {
        Self {
            configs: Vec::new(),
            enabled: Vec::new(),
            order: Vec::new(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestPass {
        name: String,
        enabled: bool,
    }

    impl TestPass {
        fn new(name: &str) -> Self {
            Self {
                name: name.to_string(),
                enabled: true,
            }
        }
    }

    impl CustomRenderPass for TestPass {
        fn name(&self) -> &str {
            &self.name
        }

        fn execute(&self, _context: &PassExecuteContext) -> Result<(), PassError> {
            Ok(())
        }

        fn is_enabled(&self) -> bool {
            self.enabled
        }

        fn set_enabled(&mut self, enabled: bool) {
            self.enabled = enabled;
        }
    }

    #[test]
    fn test_resource_ref() {
        let tex = ResourceRef::texture("my_texture");
        assert!(matches!(tex, ResourceRef::Texture(s) if s == "my_texture"));

        let buf = ResourceRef::buffer("my_buffer");
        assert!(matches!(buf, ResourceRef::Buffer(s) if s == "my_buffer"));
    }

    #[test]
    fn test_resource_ref_serialization() {
        let refs = vec![
            ResourceRef::MainColor,
            ResourceRef::MainDepth,
            ResourceRef::GBuffer(GBufferChannel::Normal),
            ResourceRef::Texture("custom".into()),
        ];

        for r in refs {
            let json = serde_json::to_string(&r).unwrap();
            let restored: ResourceRef = serde_json::from_str(&json).unwrap();
            assert_eq!(r, restored);
        }
    }

    #[test]
    fn test_resource_requirements() {
        let reqs = ResourceRequirements {
            memory_bytes: 64 * 1024 * 1024,
            render_targets: 2,
            compute: true,
            time_budget_ms: 2.0,
            ..Default::default()
        };

        assert_eq!(reqs.memory_bytes, 64 * 1024 * 1024);
        assert!(reqs.compute);
    }

    #[test]
    fn test_pass_resources() {
        let mut resources = PassResources::new();
        resources.register_texture("main_color", TextureViewHandle(1));
        resources.register_texture("main_depth", TextureViewHandle(2));
        resources.register_buffer("uniform_buffer", BufferHandle(100));

        assert_eq!(resources.main_color(), Some(TextureViewHandle(1)));
        assert_eq!(resources.main_depth(), Some(TextureViewHandle(2)));
        assert_eq!(resources.get_buffer("uniform_buffer"), Some(BufferHandle(100)));
        assert_eq!(resources.get_texture("nonexistent"), None);
    }

    #[test]
    fn test_custom_pass_trait() {
        let pass = TestPass::new("test_pass");
        assert_eq!(pass.name(), "test_pass");
        assert!(pass.dependencies().is_empty());
        assert!(pass.reads().is_empty());
        assert!(pass.writes().is_empty());
        assert!(pass.is_enabled());

        let ctx = PassExecuteContext::default();
        assert!(pass.execute(&ctx).is_ok());
    }

    #[test]
    fn test_pass_priority_ordering() {
        assert!(PassPriority::PRE_DEPTH < PassPriority::SHADOW);
        assert!(PassPriority::SHADOW < PassPriority::MAIN);
        assert!(PassPriority::MAIN < PassPriority::TRANSPARENT);
        assert!(PassPriority::TRANSPARENT < PassPriority::POST_PROCESS);
        assert!(PassPriority::POST_PROCESS < PassPriority::UI);
    }

    #[test]
    fn test_pass_error_display() {
        let err = PassError::Resource("texture not found".into());
        assert!(err.to_string().contains("texture not found"));

        let err = PassError::PartialFailure(vec![
            ("bloom".into(), "out of memory".into()),
            ("outline".into(), "shader error".into()),
        ]);
        let msg = err.to_string();
        assert!(msg.contains("bloom"));
        assert!(msg.contains("outline"));
    }

    #[test]
    fn test_camera_render_data() {
        let camera = Camera::perspective(60.0_f32.to_radians(), 16.0 / 9.0, 0.1, 1000.0);
        let data = CameraRenderData::from_camera(&camera);

        assert_eq!(data.near, 0.1);
        assert_eq!(data.far, 1000.0);
        assert!((data.aspect - 16.0 / 9.0).abs() < 0.001);
    }

    #[test]
    fn test_pass_config_serialization() {
        let config = PassConfigData {
            name: "bloom".into(),
            enabled: true,
            priority: 100,
            config: serde_json::json!({
                "threshold": 1.0,
                "intensity": 0.5
            }),
        };

        let json = serde_json::to_string(&config).unwrap();
        let restored: PassConfigData = serde_json::from_str(&json).unwrap();

        assert_eq!(restored.name, "bloom");
        assert!(restored.enabled);
        assert_eq!(restored.priority, 100);
    }
}
