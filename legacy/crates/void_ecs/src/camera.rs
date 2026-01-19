//! Camera System Components
//!
//! Provides camera entities for rendering viewpoints with support for:
//! - Perspective and orthographic projections
//! - Multiple cameras with priority-based rendering
//! - Camera animation (FOV transitions)
//! - Viewport rectangles for split-screen
//! - Integration with hierarchy system for camera parenting
//!
//! # Example
//!
//! ```ignore
//! use void_ecs::camera::{Camera, Projection};
//!
//! // Create a perspective camera
//! let camera = Camera::perspective(60.0, 0.1, 1000.0);
//!
//! // Create an orthographic camera for 2D
//! let camera_2d = Camera::orthographic(10.0, -100.0, 100.0);
//! ```

use crate::hierarchy::GlobalTransform;
use serde::{Deserialize, Serialize};

#[cfg(feature = "hot-reload")]
use void_core::hot_reload::{HotReloadError, HotReloadable};

/// Camera projection mode
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum Projection {
    /// Perspective projection with field of view
    Perspective {
        /// Vertical field of view in radians
        fov_y: f32,
    },
    /// Orthographic projection
    Orthographic {
        /// Orthographic height (width derived from aspect ratio)
        height: f32,
    },
}

impl Default for Projection {
    fn default() -> Self {
        // 60 degrees vertical FOV
        Projection::Perspective {
            fov_y: core::f32::consts::FRAC_PI_3,
        }
    }
}

impl Projection {
    /// Create perspective projection with FOV in degrees
    pub fn perspective_degrees(fov_degrees: f32) -> Self {
        Projection::Perspective {
            fov_y: fov_degrees.to_radians(),
        }
    }

    /// Create perspective projection with FOV in radians
    pub fn perspective_radians(fov_radians: f32) -> Self {
        Projection::Perspective { fov_y: fov_radians }
    }

    /// Create orthographic projection
    pub fn orthographic(height: f32) -> Self {
        Projection::Orthographic { height }
    }

    /// Get the FOV in radians (returns 0 for orthographic)
    pub fn fov_radians(&self) -> f32 {
        match self {
            Projection::Perspective { fov_y } => *fov_y,
            Projection::Orthographic { .. } => 0.0,
        }
    }

    /// Get the FOV in degrees (returns 0 for orthographic)
    pub fn fov_degrees(&self) -> f32 {
        self.fov_radians().to_degrees()
    }
}

/// Normalized viewport rectangle (0.0 - 1.0)
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Viewport {
    /// X position (0.0 = left edge)
    pub x: f32,
    /// Y position (0.0 = top edge)
    pub y: f32,
    /// Width (1.0 = full width)
    pub width: f32,
    /// Height (1.0 = full height)
    pub height: f32,
}

impl Default for Viewport {
    fn default() -> Self {
        Self::full()
    }
}

impl Viewport {
    /// Full screen viewport
    pub fn full() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: 1.0,
            height: 1.0,
        }
    }

    /// Left half of screen (for split-screen)
    pub fn left_half() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: 0.5,
            height: 1.0,
        }
    }

    /// Right half of screen (for split-screen)
    pub fn right_half() -> Self {
        Self {
            x: 0.5,
            y: 0.0,
            width: 0.5,
            height: 1.0,
        }
    }

    /// Top half of screen
    pub fn top_half() -> Self {
        Self {
            x: 0.0,
            y: 0.0,
            width: 1.0,
            height: 0.5,
        }
    }

    /// Bottom half of screen
    pub fn bottom_half() -> Self {
        Self {
            x: 0.0,
            y: 0.5,
            width: 1.0,
            height: 0.5,
        }
    }

    /// Create a custom viewport
    pub fn new(x: f32, y: f32, width: f32, height: f32) -> Self {
        Self {
            x: x.clamp(0.0, 1.0),
            y: y.clamp(0.0, 1.0),
            width: width.clamp(0.0, 1.0),
            height: height.clamp(0.0, 1.0),
        }
    }

    /// Get the aspect ratio of this viewport
    pub fn aspect_ratio(&self, screen_width: u32, screen_height: u32) -> f32 {
        let pixel_width = self.width * screen_width as f32;
        let pixel_height = self.height * screen_height as f32;
        if pixel_height > 0.0 {
            pixel_width / pixel_height
        } else {
            1.0
        }
    }

    /// Convert to pixel coordinates
    pub fn to_pixels(&self, screen_width: u32, screen_height: u32) -> (u32, u32, u32, u32) {
        let x = (self.x * screen_width as f32) as u32;
        let y = (self.y * screen_height as f32) as u32;
        let w = (self.width * screen_width as f32) as u32;
        let h = (self.height * screen_height as f32) as u32;
        (x, y, w, h)
    }
}

/// Camera component for rendering viewpoints
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Camera {
    /// Projection mode (perspective or orthographic)
    pub projection: Projection,

    /// Near clipping plane distance
    pub near: f32,

    /// Far clipping plane distance
    pub far: f32,

    /// Aspect ratio override (None = use viewport aspect ratio)
    pub aspect_ratio: Option<f32>,

    /// Clear color for this camera's render target (RGBA)
    pub clear_color: [f32; 4],

    /// Render priority (higher = rendered later, on top)
    pub priority: i32,

    /// Target render layer mask (None = render all layers)
    pub layer_mask: Option<u32>,

    /// Viewport rectangle (None = full screen)
    pub viewport: Option<Viewport>,

    /// Is this the main camera?
    pub is_main: bool,

    /// Is this camera active (will it render)?
    pub active: bool,

    /// Target render texture name (None = main framebuffer)
    pub render_target: Option<String>,

    /// Active animation on this camera (not serialized)
    #[serde(skip)]
    pub animation: Option<CameraAnimation>,

    // Transient cached data (not serialized)
    #[serde(skip)]
    cached_view_matrix: Option<[[f32; 4]; 4]>,

    #[serde(skip)]
    cached_projection_matrix: Option<[[f32; 4]; 4]>,

    #[serde(skip)]
    cached_aspect: Option<f32>,
}

impl Default for Camera {
    fn default() -> Self {
        Self {
            projection: Projection::default(),
            near: 0.1,
            far: 1000.0,
            aspect_ratio: None,
            clear_color: [0.1, 0.1, 0.1, 1.0],
            priority: 0,
            layer_mask: None,
            viewport: None,
            is_main: false,
            active: true,
            render_target: None,
            animation: None,
            cached_view_matrix: None,
            cached_projection_matrix: None,
            cached_aspect: None,
        }
    }
}

impl Camera {
    /// Create a perspective camera with FOV in degrees
    pub fn perspective(fov_degrees: f32, near: f32, far: f32) -> Self {
        Self {
            projection: Projection::perspective_degrees(fov_degrees),
            near,
            far,
            ..Default::default()
        }
    }

    /// Create an orthographic camera
    pub fn orthographic(height: f32, near: f32, far: f32) -> Self {
        Self {
            projection: Projection::orthographic(height),
            near,
            far,
            ..Default::default()
        }
    }

    /// Create a main camera (perspective, 60 FOV)
    pub fn main() -> Self {
        Self {
            is_main: true,
            ..Self::perspective(60.0, 0.1, 1000.0)
        }
    }

    /// Set as main camera (builder pattern)
    pub fn with_main(mut self, is_main: bool) -> Self {
        self.is_main = is_main;
        self
    }

    /// Set clear color (builder pattern)
    pub fn with_clear_color(mut self, color: [f32; 4]) -> Self {
        self.clear_color = color;
        self
    }

    /// Set priority (builder pattern)
    pub fn with_priority(mut self, priority: i32) -> Self {
        self.priority = priority;
        self
    }

    /// Set viewport (builder pattern)
    pub fn with_viewport(mut self, viewport: Viewport) -> Self {
        self.viewport = Some(viewport);
        self
    }

    /// Set layer mask (builder pattern)
    pub fn with_layer_mask(mut self, mask: u32) -> Self {
        self.layer_mask = Some(mask);
        self
    }

    /// Set aspect ratio override (builder pattern)
    pub fn with_aspect_ratio(mut self, aspect: f32) -> Self {
        self.aspect_ratio = Some(aspect);
        self
    }

    /// Set render target (builder pattern)
    pub fn with_render_target(mut self, target: impl Into<String>) -> Self {
        self.render_target = Some(target.into());
        self
    }

    /// Compute the projection matrix
    pub fn projection_matrix(&self, viewport_aspect: f32) -> [[f32; 4]; 4] {
        let aspect = self.aspect_ratio.unwrap_or(viewport_aspect);

        match &self.projection {
            Projection::Perspective { fov_y } => {
                perspective_matrix(*fov_y, aspect, self.near, self.far)
            }
            Projection::Orthographic { height } => {
                let half_height = height / 2.0;
                let half_width = half_height * aspect;
                orthographic_matrix(
                    -half_width,
                    half_width,
                    -half_height,
                    half_height,
                    self.near,
                    self.far,
                )
            }
        }
    }

    /// Compute the view matrix from a GlobalTransform
    pub fn view_matrix(&self, global_transform: &GlobalTransform) -> [[f32; 4]; 4] {
        invert_transform_matrix(&global_transform.matrix)
    }

    /// Compute view-projection matrix
    pub fn view_projection_matrix(
        &self,
        global_transform: &GlobalTransform,
        viewport_aspect: f32,
    ) -> [[f32; 4]; 4] {
        let view = self.view_matrix(global_transform);
        let proj = self.projection_matrix(viewport_aspect);
        multiply_matrices(&proj, &view)
    }

    /// Get the effective aspect ratio
    pub fn effective_aspect(&self, viewport_aspect: f32) -> f32 {
        self.aspect_ratio.unwrap_or(viewport_aspect)
    }

    /// Invalidate cached matrices (call after transform changes)
    pub fn invalidate_cache(&mut self) {
        self.cached_view_matrix = None;
        self.cached_projection_matrix = None;
        self.cached_aspect = None;
    }

    /// Check if this camera should render an entity with the given layer mask
    pub fn should_render_layer(&self, entity_layer_mask: u32) -> bool {
        match self.layer_mask {
            Some(camera_mask) => (camera_mask & entity_layer_mask) != 0,
            None => true, // Render all layers if no mask set
        }
    }
}

#[cfg(feature = "hot-reload")]
impl HotReloadable for Camera {
    fn type_name() -> &'static str {
        "Camera"
    }

    fn snapshot(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::SerializationFailed(e.to_string()))
    }

    fn restore(data: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(data).map_err(|e| HotReloadError::DeserializationFailed(e.to_string()))
    }
}

#[cfg(feature = "hot-reload")]
impl HotReloadable for Projection {
    fn type_name() -> &'static str {
        "Projection"
    }

    fn snapshot(&self) -> Result<Vec<u8>, HotReloadError> {
        bincode::serialize(self).map_err(|e| HotReloadError::SerializationFailed(e.to_string()))
    }

    fn restore(data: &[u8]) -> Result<Self, HotReloadError> {
        bincode::deserialize(data).map_err(|e| HotReloadError::DeserializationFailed(e.to_string()))
    }
}

/// Easing functions for camera animations
#[derive(Clone, Copy, Debug, Default, Serialize, Deserialize)]
pub enum EasingFunction {
    /// Linear interpolation (no easing)
    #[default]
    Linear,
    /// Slow start
    EaseIn,
    /// Slow end
    EaseOut,
    /// Slow start and end
    EaseInOut,
    /// Quadratic ease in
    QuadIn,
    /// Quadratic ease out
    QuadOut,
    /// Cubic ease in
    CubicIn,
    /// Cubic ease out
    CubicOut,
}

impl EasingFunction {
    /// Apply the easing function to a normalized time value (0.0 - 1.0)
    pub fn apply(&self, t: f32) -> f32 {
        let t = t.clamp(0.0, 1.0);
        match self {
            EasingFunction::Linear => t,
            EasingFunction::EaseIn => t * t,
            EasingFunction::EaseOut => 1.0 - (1.0 - t) * (1.0 - t),
            EasingFunction::EaseInOut => {
                if t < 0.5 {
                    2.0 * t * t
                } else {
                    1.0 - (-2.0 * t + 2.0).powi(2) / 2.0
                }
            }
            EasingFunction::QuadIn => t * t,
            EasingFunction::QuadOut => t * (2.0 - t),
            EasingFunction::CubicIn => t * t * t,
            EasingFunction::CubicOut => {
                let t = t - 1.0;
                t * t * t + 1.0
            }
        }
    }
}

/// Animatable camera properties
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CameraAnimation {
    /// Target FOV in radians (for perspective cameras)
    pub target_fov: Option<f32>,

    /// Target near plane
    pub target_near: Option<f32>,

    /// Target far plane
    pub target_far: Option<f32>,

    /// Animation duration in seconds
    pub duration: f32,

    /// Elapsed time in seconds
    pub elapsed: f32,

    /// Easing function for the animation
    pub easing: EasingFunction,

    /// Starting FOV (captured when animation starts)
    start_fov: f32,

    /// Starting near plane
    start_near: f32,

    /// Starting far plane
    start_far: f32,

    /// Has the animation been initialized?
    initialized: bool,
}

impl Default for CameraAnimation {
    fn default() -> Self {
        Self {
            target_fov: None,
            target_near: None,
            target_far: None,
            duration: 1.0,
            elapsed: 0.0,
            easing: EasingFunction::default(),
            start_fov: 0.0,
            start_near: 0.0,
            start_far: 0.0,
            initialized: false,
        }
    }
}

impl CameraAnimation {
    /// Create an FOV transition animation
    pub fn fov_transition(target_fov_radians: f32, duration: f32) -> Self {
        Self {
            target_fov: Some(target_fov_radians),
            duration,
            ..Default::default()
        }
    }

    /// Create an FOV transition animation with FOV in degrees
    pub fn fov_transition_degrees(target_fov_degrees: f32, duration: f32) -> Self {
        Self::fov_transition(target_fov_degrees.to_radians(), duration)
    }

    /// Create a clip plane transition animation
    pub fn clip_transition(near: f32, far: f32, duration: f32) -> Self {
        Self {
            target_near: Some(near),
            target_far: Some(far),
            duration,
            ..Default::default()
        }
    }

    /// Set easing function (builder pattern)
    pub fn with_easing(mut self, easing: EasingFunction) -> Self {
        self.easing = easing;
        self
    }

    /// Initialize the animation with current camera values
    pub fn initialize(&mut self, camera: &Camera) {
        if !self.initialized {
            self.start_fov = match &camera.projection {
                Projection::Perspective { fov_y } => *fov_y,
                Projection::Orthographic { .. } => 0.0,
            };
            self.start_near = camera.near;
            self.start_far = camera.far;
            self.initialized = true;
        }
    }

    /// Update the animation, returning true if complete
    pub fn update(&mut self, delta_time: f32, camera: &mut Camera) -> bool {
        // Initialize on first update
        self.initialize(camera);

        self.elapsed += delta_time;
        let t = (self.elapsed / self.duration).min(1.0);
        let eased_t = self.easing.apply(t);

        // Animate FOV
        if let Some(target_fov) = self.target_fov {
            if let Projection::Perspective { fov_y } = &mut camera.projection {
                *fov_y = lerp(self.start_fov, target_fov, eased_t);
            }
        }

        // Animate near plane
        if let Some(target_near) = self.target_near {
            camera.near = lerp(self.start_near, target_near, eased_t);
        }

        // Animate far plane
        if let Some(target_far) = self.target_far {
            camera.far = lerp(self.start_far, target_far, eased_t);
        }

        // Invalidate cache since values changed
        camera.invalidate_cache();

        // Return true if animation is complete
        self.elapsed >= self.duration
    }

    /// Check if animation is complete
    pub fn is_complete(&self) -> bool {
        self.elapsed >= self.duration
    }

    /// Get progress (0.0 - 1.0)
    pub fn progress(&self) -> f32 {
        (self.elapsed / self.duration).min(1.0)
    }

    /// Reset the animation to start
    pub fn reset(&mut self) {
        self.elapsed = 0.0;
        self.initialized = false;
    }
}

// ============================================================================
// Matrix Utilities
// ============================================================================

/// Create a perspective projection matrix (right-handed, depth 0 to 1)
pub fn perspective_matrix(fov_y: f32, aspect: f32, near: f32, far: f32) -> [[f32; 4]; 4] {
    let f = 1.0 / (fov_y / 2.0).tan();
    let nf = 1.0 / (near - far);

    [
        [f / aspect, 0.0, 0.0, 0.0],
        [0.0, f, 0.0, 0.0],
        [0.0, 0.0, far * nf, -1.0],
        [0.0, 0.0, far * near * nf, 0.0],
    ]
}

/// Create an orthographic projection matrix
pub fn orthographic_matrix(
    left: f32,
    right: f32,
    bottom: f32,
    top: f32,
    near: f32,
    far: f32,
) -> [[f32; 4]; 4] {
    let lr = 1.0 / (left - right);
    let bt = 1.0 / (bottom - top);
    let nf = 1.0 / (near - far);

    [
        [-2.0 * lr, 0.0, 0.0, 0.0],
        [0.0, -2.0 * bt, 0.0, 0.0],
        [0.0, 0.0, nf, 0.0],
        [(left + right) * lr, (top + bottom) * bt, near * nf, 1.0],
    ]
}

/// Multiply two 4x4 matrices
pub fn multiply_matrices(a: &[[f32; 4]; 4], b: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let mut result = [[0.0f32; 4]; 4];

    for i in 0..4 {
        for j in 0..4 {
            result[i][j] = a[i][0] * b[0][j]
                + a[i][1] * b[1][j]
                + a[i][2] * b[2][j]
                + a[i][3] * b[3][j];
        }
    }

    result
}

/// Invert a transform matrix (assumes affine transform)
pub fn invert_transform_matrix(m: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    // For a camera transform, we need the inverse
    // Extract rotation (upper-left 3x3) and translation (last column)

    // Transpose the rotation part (inverse of orthonormal rotation)
    let r00 = m[0][0];
    let r01 = m[1][0];
    let r02 = m[2][0];
    let r10 = m[0][1];
    let r11 = m[1][1];
    let r12 = m[2][1];
    let r20 = m[0][2];
    let r21 = m[1][2];
    let r22 = m[2][2];

    // Translation
    let tx = m[3][0];
    let ty = m[3][1];
    let tz = m[3][2];

    // Inverse translation = -R^T * T
    let inv_tx = -(r00 * tx + r01 * ty + r02 * tz);
    let inv_ty = -(r10 * tx + r11 * ty + r12 * tz);
    let inv_tz = -(r20 * tx + r21 * ty + r22 * tz);

    [
        [r00, r10, r20, 0.0],
        [r01, r11, r21, 0.0],
        [r02, r12, r22, 0.0],
        [inv_tx, inv_ty, inv_tz, 1.0],
    ]
}

/// Linear interpolation
#[inline]
pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t
}

/// Identity matrix
pub const IDENTITY_MATRIX: [[f32; 4]; 4] = [
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
];

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_perspective_camera_creation() {
        let camera = Camera::perspective(60.0, 0.1, 100.0);

        assert!(!camera.is_main);
        assert!(camera.active);
        assert!((camera.near - 0.1).abs() < 0.001);
        assert!((camera.far - 100.0).abs() < 0.001);

        if let Projection::Perspective { fov_y } = camera.projection {
            assert!((fov_y - 60.0_f32.to_radians()).abs() < 0.001);
        } else {
            panic!("Expected perspective projection");
        }
    }

    #[test]
    fn test_orthographic_camera_creation() {
        let camera = Camera::orthographic(10.0, -100.0, 100.0);

        if let Projection::Orthographic { height } = camera.projection {
            assert!((height - 10.0).abs() < 0.001);
        } else {
            panic!("Expected orthographic projection");
        }
    }

    #[test]
    fn test_perspective_projection_matrix() {
        let camera = Camera::perspective(60.0, 0.1, 100.0);
        let matrix = camera.projection_matrix(16.0 / 9.0);

        // Perspective matrix should have non-zero values
        assert!(matrix[0][0] > 0.0); // X scale
        assert!(matrix[1][1] > 0.0); // Y scale
        assert!(matrix[2][2] < 0.0); // Depth mapping
        assert!((matrix[2][3] - (-1.0)).abs() < 0.001); // Perspective divide
    }

    #[test]
    fn test_orthographic_projection_matrix() {
        let camera = Camera::orthographic(10.0, 0.1, 100.0);
        let matrix = camera.projection_matrix(2.0);

        // Orthographic has no perspective divide
        assert!((matrix[3][3] - 1.0).abs() < 0.001);
        assert!((matrix[2][3] - 0.0).abs() < 0.001); // No perspective
    }

    #[test]
    fn test_viewport() {
        let viewport = Viewport::new(0.25, 0.25, 0.5, 0.5);
        let (x, y, w, h) = viewport.to_pixels(1920, 1080);

        assert_eq!(x, 480);
        assert_eq!(y, 270);
        assert_eq!(w, 960);
        assert_eq!(h, 540);
    }

    #[test]
    fn test_viewport_aspect_ratio() {
        let viewport = Viewport::left_half();
        let aspect = viewport.aspect_ratio(1920, 1080);

        // Left half: 960x1080, aspect should be 960/1080
        let expected = 960.0 / 1080.0;
        assert!((aspect - expected).abs() < 0.01);
    }

    #[test]
    fn test_camera_builder_pattern() {
        let camera = Camera::perspective(60.0, 0.1, 1000.0)
            .with_main(true)
            .with_clear_color([1.0, 0.0, 0.0, 1.0])
            .with_priority(10)
            .with_layer_mask(0b1111);

        assert!(camera.is_main);
        assert_eq!(camera.clear_color, [1.0, 0.0, 0.0, 1.0]);
        assert_eq!(camera.priority, 10);
        assert_eq!(camera.layer_mask, Some(0b1111));
    }

    #[test]
    fn test_easing_functions() {
        let linear = EasingFunction::Linear;
        let ease_in = EasingFunction::EaseIn;
        let ease_out = EasingFunction::EaseOut;

        // At t=0, all should be 0
        assert!((linear.apply(0.0) - 0.0).abs() < 0.001);
        assert!((ease_in.apply(0.0) - 0.0).abs() < 0.001);
        assert!((ease_out.apply(0.0) - 0.0).abs() < 0.001);

        // At t=1, all should be 1
        assert!((linear.apply(1.0) - 1.0).abs() < 0.001);
        assert!((ease_in.apply(1.0) - 1.0).abs() < 0.001);
        assert!((ease_out.apply(1.0) - 1.0).abs() < 0.001);

        // At t=0.5
        assert!((linear.apply(0.5) - 0.5).abs() < 0.001);
        assert!(ease_in.apply(0.5) < 0.5); // Slow start
        assert!(ease_out.apply(0.5) > 0.5); // Fast start
    }

    #[test]
    fn test_camera_animation() {
        let mut camera = Camera::perspective(60.0, 0.1, 100.0);
        let mut anim = CameraAnimation::fov_transition_degrees(90.0, 1.0);

        // Initialize and update halfway
        anim.initialize(&camera);
        let complete = anim.update(0.5, &mut camera);

        assert!(!complete);

        if let Projection::Perspective { fov_y } = camera.projection {
            // Should be approximately halfway between 60 and 90 degrees
            let expected = (60.0 + 90.0) / 2.0;
            assert!((fov_y.to_degrees() - expected).abs() < 1.0);
        }

        // Complete the animation
        let complete = anim.update(0.6, &mut camera);
        assert!(complete);

        if let Projection::Perspective { fov_y } = camera.projection {
            assert!((fov_y.to_degrees() - 90.0).abs() < 0.1);
        }
    }

    #[test]
    fn test_layer_mask_filtering() {
        let camera = Camera::perspective(60.0, 0.1, 100.0).with_layer_mask(0b0011);

        assert!(camera.should_render_layer(0b0001)); // Layer 0
        assert!(camera.should_render_layer(0b0010)); // Layer 1
        assert!(!camera.should_render_layer(0b0100)); // Layer 2
        assert!(camera.should_render_layer(0b0011)); // Both
    }

    #[test]
    fn test_matrix_multiply() {
        let a = IDENTITY_MATRIX;
        let b = [
            [2.0, 0.0, 0.0, 0.0],
            [0.0, 2.0, 0.0, 0.0],
            [0.0, 0.0, 2.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ];

        let result = multiply_matrices(&a, &b);

        // Identity * B = B
        for i in 0..4 {
            for j in 0..4 {
                assert!((result[i][j] - b[i][j]).abs() < 0.001);
            }
        }
    }

    #[test]
    fn test_identity_matrix_invert() {
        let inverted = invert_transform_matrix(&IDENTITY_MATRIX);

        // Inverse of identity is identity
        for i in 0..4 {
            for j in 0..4 {
                assert!((inverted[i][j] - IDENTITY_MATRIX[i][j]).abs() < 0.001);
            }
        }
    }

    #[test]
    fn test_lerp() {
        assert!((lerp(0.0, 10.0, 0.0) - 0.0).abs() < 0.001);
        assert!((lerp(0.0, 10.0, 0.5) - 5.0).abs() < 0.001);
        assert!((lerp(0.0, 10.0, 1.0) - 10.0).abs() < 0.001);
    }

    #[test]
    fn test_camera_serialization() {
        let camera = Camera::perspective(60.0, 0.1, 1000.0)
            .with_main(true)
            .with_clear_color([0.5, 0.5, 0.5, 1.0]);

        let serialized = serde_json::to_string(&camera).unwrap();
        let deserialized: Camera = serde_json::from_str(&serialized).unwrap();

        assert!(deserialized.is_main);
        assert_eq!(deserialized.clear_color, [0.5, 0.5, 0.5, 1.0]);
    }
}
