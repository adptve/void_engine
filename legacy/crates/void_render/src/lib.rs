//! # void_render - Render Graph and Compositor
//!
//! Backend-agnostic rendering infrastructure with:
//! - Declarative render graph
//! - Framebuffer-level compositor for renderer switching
//! - Multiple renderer support (forward, deferred, etc.)
//! - Layer-based composition
//! - Post-processing pipeline
//!
//! ## Architecture
//!
//! The rendering system is built on three main concepts:
//!
//! 1. **Render Graph**: Defines render passes and their dependencies
//! 2. **Compositor**: Manages layers and renderer switching
//! 3. **Resources**: Abstract GPU resource descriptions
//!
//! ## Example
//!
//! ```ignore
//! use void_render::prelude::*;
//!
//! // Create compositor
//! let mut compositor = Compositor::new();
//! compositor.set_output_size(1920, 1080);
//!
//! // Register renderers
//! compositor.register_renderer(MyForwardRenderer::new());
//! compositor.register_renderer(MyDeferredRenderer::new());
//!
//! // Create layers
//! let world_layer = compositor.create_layer("world", LayerConfig {
//!     priority: 0,
//!     ..Default::default()
//! });
//!
//! let ui_layer = compositor.create_layer("ui", LayerConfig {
//!     priority: 100,
//!     blend_mode: BlendMode::Normal,
//!     ..Default::default()
//! });
//!
//! // Assign renderers to layers
//! compositor.assign_renderer_to_layer(
//!     world_layer,
//!     RendererId::from_name("deferred")
//! );
//!
//! // Switch renderer at runtime
//! compositor.switch_renderer(RendererId::from_name("forward"))?;
//!
//! // Render frame
//! compositor.begin_frame();
//! compositor.build_graph();
//! compositor.execute(&mut backend_context);
//! compositor.end_frame();
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

extern crate alloc;

pub mod resource;
pub mod graph;
pub mod layer;
pub mod blend;
pub mod compositor;
pub mod extraction;
pub mod mesh_cache;
pub mod instancing;
pub mod instance_batcher;
pub mod draw_command;
pub mod light_buffer;
pub mod light_extraction;
pub mod shadow;
pub mod material_buffer;
pub mod picking;
pub mod input;
pub mod pass;
pub mod spatial;
pub mod debug;

// Re-exports (avoiding name conflicts between layer and compositor modules)
pub use resource::*;
pub use graph::{
    RenderGraph, PassId, GraphHandle, AccessMode, RenderPass, PassBuilder, PassContext,
    ColorAttachment, DepthStencilAttachment, RenderPassDesc,
};
// Layer types - use layer:: prefix for non-conflicting access
pub use layer::LayerId;
pub use layer::LayerViewport;
pub use layer::LayerCollection;
pub use layer::Layer as RenderLayer;

pub use blend::{
    generate_composite_shader, generate_blit_shader, generate_solid_color_shader,
    BlendConfig, BlendFactor, BlendOperation,
};
// Compositor types - these take precedence for the common API
pub use compositor::{
    Compositor, CompositorRenderer, RendererId, RendererFeatures, RendererContext,
    Layer, Viewport,
    PostProcess, PostProcessContext,
};
// Re-export these from layer with different names to avoid conflicts
pub use layer::BlendMode as LayerBlendMode;
pub use layer::LayerHealth;
pub use layer::LayerConfig;

// Mesh cache
pub use mesh_cache::{
    MeshCache, MeshHandle, CachedMesh, CachedPrimitive,
    GpuVertexBuffer, GpuIndexBuffer, IndexFormat as GpuIndexFormat,
    MeshCacheStats,
};

// Instancing
pub use instancing::{
    InstanceData, InstanceBatch, BatchKey,
    compute_normal_matrix,
};
pub use instance_batcher::{
    InstanceBatcher, BatcherStats, SerializedBatcherState, BatchResult,
};
pub use draw_command::{
    DrawCommand, DrawList, DrawStats, RenderPassType, RenderQueue,
};
pub use extraction::MeshInfo;

// Lighting
pub use light_buffer::{
    LightBuffer, LightBufferConfig, LightBufferState, LightBufferStats,
    GpuDirectionalLight, GpuPointLight, GpuSpotLight, LightCounts,
    MAX_DIRECTIONAL_LIGHTS, MAX_POINT_LIGHTS, MAX_SPOT_LIGHTS,
};
pub use light_extraction::{
    LightExtractor, LightExtractionConfig, LightExtractionStats, ExtractedLightInfo,
};

// Shadow Mapping
pub use shadow::{
    // Config
    ShadowConfig, LightShadowSettings, ShadowUpdateMode, ShadowQuality,
    // Cascades
    ShadowCascades, CascadeMatrixResult, MAX_CASCADES,
    // Atlas
    ShadowAtlas, ShadowAllocation, AtlasStats, ShadowAtlasState, LightId,
    // GPU Data
    GpuShadowLight, GpuCascadeShadow, GpuShadowUniforms, ShadowBuffer,
    ShadowCasterData, ShadowReceiverData, MAX_SHADOW_LIGHTS,
};

// Material Buffer
pub use material_buffer::{
    GpuMaterial, MaterialBuffer, MaterialId, MaterialOverrideData,
    MaterialBufferState, MAX_MATERIALS,
};

// Picking & Raycasting
pub use picking::{
    RaycastHit, RaycastQuery, RaycastSystemState,
    screen_to_ray, world_to_screen,
    raycast_aabb, raycast_sphere, raycast_triangles,
};

// Entity Input Events
pub use input::{
    EntityInputProcessor, EntityInputProcessorState, InputProcessingMode,
    InputSystemState, InputStateSnapshot, DragState,
    InputUpdateQueue, InputSystemUpdate, PersistentEventQueue,
};

// Multi-Pass Rendering (Phase 12)
pub use pass::{
    PassId as RenderPassId, RenderPassFlags,
    RenderPassSystem, RenderPassSystemState, PassConfig, PassDrawCall,
    PassSortMode, CullMode, BlendMode as PassBlendMode, PassQuality,
    RenderPassUpdate, RenderPassUpdateQueue, RenderPassStats,
};

// Custom Render Passes (Phase 13)
pub use pass::{
    CustomRenderPass, ResourceRef, GBufferChannel, ResourceRequirements,
    TextureFormatHint, PassFeatures, PassPriority, CameraRenderData,
    TextureViewHandle, BufferHandle, PipelineHandle, PassResources,
    PassSetupContext, PassExecuteContext, PassError, PassConfigData, CustomPassState,
    PassRegistry, ResourceBudget, PassRegistryUpdate, PassUpdateQueue,
    PassRegistryStats, PassRegistryState,
    BloomPass, BloomPassConfig, BloomPassSimple,
    OutlinePass, OutlinePassConfig,
    SSAOPass, SSAOPassConfig,
    FogPass, FogPassConfig, FogType,
};

// Spatial Queries & Bounds (Phase 14)
pub use spatial::{
    BVH, BVHNode, BVHState,
    SpatialQueryConfig, SpatialUpdate, SpatialUpdateQueue,
    SpatialIndexSystem, SpatialIndexSystemState, SpatialSystemStats,
};

// Debug & Introspection (Phase 18)
pub use debug::{
    RenderStats, StatsCollector, StatsAverager,
    StatsValidation, StatsIssue, StatsCollectorState,
    DebugVisualization, DebugConfig, DebugUpdateQueue, DebugConfigState,
};

/// Prelude - commonly used types
pub mod prelude {
    pub use crate::resource::{
        ResourceId, TextureFormat, TextureDesc, TextureUsage, TextureDimension,
        BufferDesc, BufferUsage, SamplerDesc, FilterMode, AddressMode,
        LoadOp, StoreOp, ClearValue,
    };
    pub use crate::graph::{
        RenderGraph, PassId, GraphHandle, RenderPass, PassBuilder, PassContext,
    };
    pub use crate::layer::{
        LayerId, LayerViewport as Viewport, LayerHealth, LayerConfig,
    };
    pub use crate::layer::BlendMode as LayerBlendMode;
    pub use crate::compositor::{
        Compositor, CompositorRenderer, RendererId, RendererContext, Layer,
    };
    pub use crate::debug::{
        RenderStats, StatsCollector, DebugVisualization, DebugConfig,
    };
}

/// Camera types for rendering
pub mod camera {
    use void_math::{Mat4, Vec3, Quat};

    /// Projection type
    #[derive(Clone, Copy, Debug)]
    pub enum Projection {
        Perspective {
            fov: f32,
            aspect: f32,
            near: f32,
            far: f32,
        },
        Orthographic {
            left: f32,
            right: f32,
            bottom: f32,
            top: f32,
            near: f32,
            far: f32,
        },
    }

    impl Default for Projection {
        fn default() -> Self {
            Self::Perspective {
                fov: 60.0_f32.to_radians(),
                aspect: 16.0 / 9.0,
                near: 0.1,
                far: 1000.0,
            }
        }
    }

    /// Camera for rendering
    #[derive(Clone, Debug)]
    pub struct Camera {
        /// World position
        pub position: Vec3,
        /// Orientation
        pub rotation: Quat,
        /// Projection
        pub projection: Projection,
    }

    impl Camera {
        /// Create a new camera
        pub fn new() -> Self {
            Self {
                position: Vec3::ZERO,
                rotation: Quat::IDENTITY,
                projection: Projection::default(),
            }
        }

        /// Create a perspective camera
        pub fn perspective(fov: f32, aspect: f32, near: f32, far: f32) -> Self {
            Self {
                position: Vec3::ZERO,
                rotation: Quat::IDENTITY,
                projection: Projection::Perspective { fov, aspect, near, far },
            }
        }

        /// Create an orthographic camera
        pub fn orthographic(width: f32, height: f32, near: f32, far: f32) -> Self {
            let half_w = width * 0.5;
            let half_h = height * 0.5;
            Self {
                position: Vec3::ZERO,
                rotation: Quat::IDENTITY,
                projection: Projection::Orthographic {
                    left: -half_w,
                    right: half_w,
                    bottom: -half_h,
                    top: half_h,
                    near,
                    far,
                },
            }
        }

        /// Look at a target
        pub fn look_at(&mut self, target: Vec3, up: Vec3) {
            let forward = (target - self.position).normalize();
            let right = forward.cross(up).normalize();
            let actual_up = right.cross(forward);
            self.rotation = Quat::from_rotation_matrix(&Mat4::from_cols(
                right.extend(0.0),
                actual_up.extend(0.0),
                (-forward).extend(0.0),
                void_math::Vec4::W,
            ));
        }

        /// Get the view matrix
        pub fn view_matrix(&self) -> Mat4 {
            let rotation_matrix = Mat4::from_quat(self.rotation.conjugate());
            let translation_matrix = Mat4::from_translation(-self.position);
            rotation_matrix * translation_matrix
        }

        /// Get the projection matrix
        pub fn projection_matrix(&self) -> Mat4 {
            match self.projection {
                Projection::Perspective { fov, aspect, near, far } => {
                    Mat4::perspective(fov, aspect, near, far)
                }
                Projection::Orthographic { left, right, bottom, top, near, far } => {
                    Mat4::orthographic(left, right, bottom, top, near, far)
                }
            }
        }

        /// Get the combined view-projection matrix
        pub fn view_projection_matrix(&self) -> Mat4 {
            self.projection_matrix() * self.view_matrix()
        }

        /// Get the forward direction
        pub fn forward(&self) -> Vec3 {
            self.rotation * -Vec3::Z
        }

        /// Get the right direction
        pub fn right(&self) -> Vec3 {
            self.rotation * Vec3::X
        }

        /// Get the up direction
        pub fn up(&self) -> Vec3 {
            self.rotation * Vec3::Y
        }
    }

    impl Default for Camera {
        fn default() -> Self {
            Self::new()
        }
    }
}

/// Camera controller for interactive camera control
pub mod camera_controller {
    use void_math::{Mat4, Vec3, Quat};
    use super::camera::{Camera, Projection};

    /// Camera control mode
    #[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
    pub enum CameraMode {
        /// First-person shooter style (WASD + mouse look)
        #[default]
        Fps,
        /// Orbit around a target point
        Orbit,
        /// Fly mode (6DOF)
        Fly,
    }

    /// Input state for camera controller
    #[derive(Clone, Debug, Default)]
    pub struct CameraInput {
        /// Movement input (-1 to 1 for each axis)
        pub move_forward: f32,
        pub move_right: f32,
        pub move_up: f32,
        /// Mouse delta (pixels)
        pub mouse_delta: (f32, f32),
        /// Mouse scroll (for zoom)
        pub scroll_delta: f32,
        /// Sprint modifier
        pub sprint: bool,
        /// Is right mouse button held (for orbit drag)
        pub drag_active: bool,
    }

    /// Camera controller for FPS/Orbit camera control
    #[derive(Clone, Debug)]
    pub struct CameraController {
        /// Control mode
        pub mode: CameraMode,
        /// Movement speed (units per second)
        pub move_speed: f32,
        /// Sprint multiplier
        pub sprint_multiplier: f32,
        /// Mouse sensitivity
        pub mouse_sensitivity: f32,
        /// Orbit target (for Orbit mode)
        pub orbit_target: Vec3,
        /// Orbit distance
        pub orbit_distance: f32,
        /// Min/max orbit distance
        pub orbit_distance_range: (f32, f32),
        /// Yaw angle (radians)
        pub yaw: f32,
        /// Pitch angle (radians)
        pub pitch: f32,
        /// Pitch limits (min, max in radians)
        pub pitch_limits: (f32, f32),
        /// Zoom speed for scroll
        pub zoom_speed: f32,
        /// Smoothing factor (0 = instant, 1 = very smooth)
        pub smoothing: f32,
        /// Current velocity (for smoothing)
        velocity: Vec3,
    }

    impl Default for CameraController {
        fn default() -> Self {
            Self {
                mode: CameraMode::Fps,
                move_speed: 5.0,
                sprint_multiplier: 2.5,
                mouse_sensitivity: 0.003,
                orbit_target: Vec3::ZERO,
                orbit_distance: 5.0,
                orbit_distance_range: (0.5, 100.0),
                yaw: 0.0,
                pitch: 0.0,
                pitch_limits: (-1.5, 1.5), // About ±86 degrees
                zoom_speed: 1.0,
                smoothing: 0.0,
                velocity: Vec3::ZERO,
            }
        }
    }

    impl CameraController {
        /// Create a new FPS camera controller
        pub fn fps() -> Self {
            Self {
                mode: CameraMode::Fps,
                ..Default::default()
            }
        }

        /// Create a new orbit camera controller
        pub fn orbit(target: Vec3, distance: f32) -> Self {
            Self {
                mode: CameraMode::Orbit,
                orbit_target: target,
                orbit_distance: distance,
                ..Default::default()
            }
        }

        /// Create a new fly camera controller
        pub fn fly() -> Self {
            Self {
                mode: CameraMode::Fly,
                ..Default::default()
            }
        }

        /// Set initial orientation from a direction
        pub fn set_direction(&mut self, forward: Vec3) {
            let forward = forward.normalize();
            self.yaw = forward.z.atan2(forward.x);
            self.pitch = (-forward.y).asin().clamp(self.pitch_limits.0, self.pitch_limits.1);
        }

        /// Initialize from a camera's current state
        pub fn init_from_camera(&mut self, camera: &Camera) {
            let forward = camera.forward();
            self.set_direction(forward);

            if self.mode == CameraMode::Orbit {
                self.orbit_distance = camera.position.length();
            }
        }

        /// Update camera based on input
        pub fn update(&mut self, camera: &mut Camera, input: &CameraInput, delta_time: f32) {
            match self.mode {
                CameraMode::Fps => self.update_fps(camera, input, delta_time),
                CameraMode::Orbit => self.update_orbit(camera, input, delta_time),
                CameraMode::Fly => self.update_fly(camera, input, delta_time),
            }
        }

        fn update_fps(&mut self, camera: &mut Camera, input: &CameraInput, delta_time: f32) {
            // Update orientation from mouse
            self.yaw -= input.mouse_delta.0 * self.mouse_sensitivity;
            self.pitch -= input.mouse_delta.1 * self.mouse_sensitivity;
            self.pitch = self.pitch.clamp(self.pitch_limits.0, self.pitch_limits.1);

            // Build rotation quaternion
            let rotation = Quat::from_euler_yxz(self.yaw, self.pitch, 0.0);
            camera.rotation = rotation;

            // Calculate movement direction (forward is -Z in right-handed coordinates)
            let forward = Vec3::new(-self.yaw.sin(), 0.0, -self.yaw.cos());
            let right = Vec3::new(-forward.z, 0.0, forward.x);

            let mut move_dir = Vec3::ZERO;
            move_dir = move_dir + forward * input.move_forward;
            move_dir = move_dir + right * input.move_right;
            move_dir = move_dir + Vec3::Y * input.move_up;

            // Normalize if moving diagonally
            if move_dir.length_squared() > 0.01 {
                move_dir = move_dir.normalize();
            }

            // Apply speed
            let speed = self.move_speed * if input.sprint { self.sprint_multiplier } else { 1.0 };
            let target_velocity = move_dir * speed;

            // Apply smoothing
            if self.smoothing > 0.0 {
                let t = 1.0 - (-delta_time / self.smoothing.max(0.01)).exp();
                self.velocity = self.velocity.lerp(target_velocity, t);
            } else {
                self.velocity = target_velocity;
            }

            // Update position
            camera.position = camera.position + self.velocity * delta_time;
        }

        fn update_orbit(&mut self, camera: &mut Camera, input: &CameraInput, delta_time: f32) {
            // Update orientation from mouse (only when dragging)
            if input.drag_active {
                self.yaw -= input.mouse_delta.0 * self.mouse_sensitivity;
                self.pitch += input.mouse_delta.1 * self.mouse_sensitivity;
                self.pitch = self.pitch.clamp(self.pitch_limits.0, self.pitch_limits.1);
            }

            // Update distance from scroll (proportional zoom for consistent feel)
            if input.scroll_delta.abs() > 0.01 {
                // Use exponential zoom: each scroll notch = ~50% distance change
                // Positive scroll = zoom in (closer), negative = zoom out
                let zoom_per_notch = 0.5 * self.zoom_speed;
                let zoom_factor = (1.0 - input.scroll_delta * zoom_per_notch).clamp(0.2, 5.0);
                self.orbit_distance *= zoom_factor;
                self.orbit_distance = self.orbit_distance.clamp(
                    self.orbit_distance_range.0,
                    self.orbit_distance_range.1,
                );
            }

            // Handle WASD movement by panning the orbit target
            if input.move_forward.abs() > 0.01 || input.move_right.abs() > 0.01 || input.move_up.abs() > 0.01 {
                // Get camera-relative directions (on XZ plane for forward/right)
                // Negate forward so W moves camera forward (target moves toward camera)
                let forward_dir = Vec3::new(-self.yaw.sin(), 0.0, -self.yaw.cos());
                let right_dir = Vec3::new(-forward_dir.z, 0.0, forward_dir.x);

                let speed = self.move_speed * if input.sprint { self.sprint_multiplier } else { 1.0 };

                let mut pan_delta = Vec3::ZERO;
                pan_delta = pan_delta + forward_dir * input.move_forward;
                pan_delta = pan_delta + right_dir * input.move_right;
                pan_delta = pan_delta + Vec3::Y * input.move_up;

                self.orbit_target = self.orbit_target + pan_delta * speed * delta_time;
            }

            // Calculate camera position on sphere around target
            let cos_pitch = self.pitch.cos();
            let offset = Vec3::new(
                self.yaw.sin() * cos_pitch,
                self.pitch.sin(),
                self.yaw.cos() * cos_pitch,
            ) * self.orbit_distance;

            camera.position = self.orbit_target + offset;

            // Look at target
            let forward = (self.orbit_target - camera.position).normalize();
            let right = forward.cross(Vec3::Y).normalize_or_zero();
            let up = right.cross(forward);

            // Build rotation from look-at
            camera.rotation = Quat::from_rotation_matrix(&Mat4::from_cols(
                right.extend(0.0),
                up.extend(0.0),
                (-forward).extend(0.0),
                void_math::Vec4::W,
            ));
        }

        fn update_fly(&mut self, camera: &mut Camera, input: &CameraInput, delta_time: f32) {
            // Update orientation from mouse
            self.yaw -= input.mouse_delta.0 * self.mouse_sensitivity;
            self.pitch -= input.mouse_delta.1 * self.mouse_sensitivity;
            self.pitch = self.pitch.clamp(self.pitch_limits.0, self.pitch_limits.1);

            // Build rotation quaternion
            let rotation = Quat::from_euler_yxz(self.yaw, self.pitch, 0.0);
            camera.rotation = rotation;

            // Movement in camera space
            let forward = camera.forward();
            let right = camera.right();
            let up = camera.up();

            let mut move_dir = Vec3::ZERO;
            move_dir = move_dir + forward * input.move_forward;
            move_dir = move_dir + right * input.move_right;
            move_dir = move_dir + up * input.move_up;

            if move_dir.length_squared() > 0.01 {
                move_dir = move_dir.normalize();
            }

            let speed = self.move_speed * if input.sprint { self.sprint_multiplier } else { 1.0 };

            camera.position = camera.position + move_dir * speed * delta_time;
        }

        /// Pan the orbit target (for orbit mode)
        pub fn pan(&mut self, camera: &Camera, delta: (f32, f32)) {
            if self.mode == CameraMode::Orbit {
                let right = camera.right();
                let up = camera.up();

                let pan_speed = self.orbit_distance * 0.001;
                self.orbit_target = self.orbit_target + right * delta.0 * pan_speed;
                self.orbit_target = self.orbit_target + up * delta.1 * pan_speed;
            }
        }

        /// Focus on a target point (for orbit mode)
        pub fn focus_on(&mut self, target: Vec3, distance: Option<f32>) {
            self.orbit_target = target;
            if let Some(d) = distance {
                self.orbit_distance = d.clamp(
                    self.orbit_distance_range.0,
                    self.orbit_distance_range.1,
                );
            }
        }

        /// Get current forward direction
        pub fn forward(&self) -> Vec3 {
            let cos_pitch = self.pitch.cos();
            Vec3::new(
                self.yaw.sin() * cos_pitch,
                -self.pitch.sin(),
                self.yaw.cos() * cos_pitch,
            )
        }

        /// Configure controller from sensitivity/range parameters
        pub fn configure(
            &mut self,
            orbit_sensitivity: f32,
            pan_sensitivity: f32,
            zoom_sensitivity: f32,
            min_distance: f32,
            max_distance: f32,
            invert_x: bool,
            invert_y: bool,
        ) {
            self.mouse_sensitivity = orbit_sensitivity;
            self.zoom_speed = zoom_sensitivity;
            self.orbit_distance_range = (min_distance, max_distance);
            // Store invert flags for use in update - handled by caller flipping mouse_delta
            let _ = (invert_x, invert_y, pan_sensitivity); // Suppress unused warnings
        }
    }
}

/// Mesh and geometry types
pub mod mesh {
    use alloc::vec;
    use alloc::vec::Vec;
    use void_math::{Vec2, Vec3, Vec4};

    /// Vertex attribute
    #[derive(Clone, Copy, Debug)]
    pub struct Vertex {
        pub position: Vec3,
        pub normal: Vec3,
        pub tangent: Vec4,
        pub uv0: Vec2,
        pub uv1: Vec2,
        pub color: Vec4,
    }

    impl Default for Vertex {
        fn default() -> Self {
            Self {
                position: Vec3::ZERO,
                normal: Vec3::Y,
                tangent: Vec4::new(1.0, 0.0, 0.0, 1.0),
                uv0: Vec2::ZERO,
                uv1: Vec2::ZERO,
                color: Vec4::ONE,
            }
        }
    }

    /// Index type
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum IndexFormat {
        U16,
        U32,
    }

    /// Primitive topology
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum PrimitiveTopology {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
    }

    impl Default for PrimitiveTopology {
        fn default() -> Self {
            Self::TriangleList
        }
    }

    /// Mesh data
    #[derive(Clone, Debug)]
    pub struct MeshData {
        pub vertices: Vec<Vertex>,
        pub indices: Vec<u32>,
        pub topology: PrimitiveTopology,
    }

    impl MeshData {
        /// Create empty mesh data
        pub fn new() -> Self {
            Self {
                vertices: Vec::new(),
                indices: Vec::new(),
                topology: PrimitiveTopology::TriangleList,
            }
        }

        /// Create a quad mesh
        pub fn quad(size: f32) -> Self {
            let half = size * 0.5;
            Self {
                vertices: vec![
                    Vertex {
                        position: Vec3::new(-half, -half, 0.0),
                        normal: Vec3::Z,
                        uv0: Vec2::new(0.0, 1.0),
                        ..Default::default()
                    },
                    Vertex {
                        position: Vec3::new(half, -half, 0.0),
                        normal: Vec3::Z,
                        uv0: Vec2::new(1.0, 1.0),
                        ..Default::default()
                    },
                    Vertex {
                        position: Vec3::new(half, half, 0.0),
                        normal: Vec3::Z,
                        uv0: Vec2::new(1.0, 0.0),
                        ..Default::default()
                    },
                    Vertex {
                        position: Vec3::new(-half, half, 0.0),
                        normal: Vec3::Z,
                        uv0: Vec2::new(0.0, 0.0),
                        ..Default::default()
                    },
                ],
                indices: vec![0, 1, 2, 0, 2, 3],
                topology: PrimitiveTopology::TriangleList,
            }
        }

        /// Get vertex count
        pub fn vertex_count(&self) -> usize {
            self.vertices.len()
        }

        /// Get index count
        pub fn index_count(&self) -> usize {
            self.indices.len()
        }
    }

    impl Default for MeshData {
        fn default() -> Self {
            Self::new()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_camera() {
        let mut camera = camera::Camera::perspective(
            60.0_f32.to_radians(),
            16.0 / 9.0,
            0.1,
            1000.0,
        );

        camera.position = void_math::Vec3::new(0.0, 5.0, 10.0);
        camera.look_at(void_math::Vec3::ZERO, void_math::Vec3::Y);

        let _vp = camera.view_projection_matrix();
    }

    #[test]
    fn test_mesh() {
        let quad = mesh::MeshData::quad(1.0);
        assert_eq!(quad.vertex_count(), 4);
        assert_eq!(quad.index_count(), 6);
    }
}
