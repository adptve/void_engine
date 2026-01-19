//! Viewport state and configuration.

/// Viewport rendering state.
#[derive(Clone, Debug)]
pub struct ViewportState {
    // Camera
    pub camera_pos: [f32; 3],
    pub camera_yaw: f32,
    pub camera_pitch: f32,
    pub camera_distance: f32,

    // Mouse state
    pub mouse_pos: [f32; 2],
    pub mouse_dragging: bool,
    pub mouse_orbiting: bool,

    // Viewport size
    pub width: u32,
    pub height: u32,

    // Rendering options
    pub wireframe_mode: bool,
    pub debug_normals: bool,
}

impl Default for ViewportState {
    fn default() -> Self {
        Self {
            camera_pos: [0.0, 2.0, 5.0],
            camera_yaw: 0.0,
            camera_pitch: -0.3,
            camera_distance: 5.0,
            mouse_pos: [0.0, 0.0],
            mouse_dragging: false,
            mouse_orbiting: false,
            width: 1280,
            height: 720,
            wireframe_mode: false,
            debug_normals: false,
        }
    }
}

impl ViewportState {
    /// Get the aspect ratio.
    pub fn aspect_ratio(&self) -> f32 {
        if self.height > 0 {
            self.width as f32 / self.height as f32
        } else {
            1.0
        }
    }

    /// Update viewport size.
    pub fn resize(&mut self, width: u32, height: u32) {
        self.width = width;
        self.height = height;
    }

    /// Calculate camera eye position.
    pub fn camera_eye(&self) -> [f32; 3] {
        let dist = self.camera_distance;
        let yaw = self.camera_yaw;
        let pitch = self.camera_pitch;

        [
            dist * pitch.cos() * yaw.sin(),
            dist * pitch.sin() + 1.0,
            dist * pitch.cos() * yaw.cos(),
        ]
    }

    /// Calculate camera target position.
    pub fn camera_target(&self) -> [f32; 3] {
        [0.0, 1.0, 0.0]
    }

    /// Reset camera to default view.
    pub fn reset_camera(&mut self) {
        self.camera_yaw = 0.0;
        self.camera_pitch = -0.3;
        self.camera_distance = 5.0;
    }

    /// Focus camera on a position.
    pub fn focus_on(&mut self, position: [f32; 3], distance: f32) {
        self.camera_pos = position;
        self.camera_distance = distance;
    }

    /// Get viewport size as [width, height].
    pub fn size(&self) -> [f32; 2] {
        [self.width as f32, self.height as f32]
    }

    /// Get camera position (alias for camera_eye).
    pub fn camera_position(&self) -> [f32; 3] {
        self.camera_eye()
    }
}
