//! Camera controller for viewport navigation.

use super::ViewportState;

/// Camera controller handling input for viewport navigation.
pub struct CameraController {
    pub orbit_sensitivity: f32,
    pub pan_sensitivity: f32,
    pub zoom_sensitivity: f32,
    pub min_distance: f32,
    pub max_distance: f32,
    pub invert_y: bool,
    pub invert_x: bool,
}

impl Default for CameraController {
    fn default() -> Self {
        Self {
            orbit_sensitivity: 0.01,
            pan_sensitivity: 0.01,
            zoom_sensitivity: 0.5,
            min_distance: 1.0,
            max_distance: 50.0,
            invert_y: false,
            invert_x: false,
        }
    }
}

impl CameraController {
    pub fn new() -> Self {
        Self::default()
    }

    /// Handle mouse drag for orbiting.
    pub fn orbit(&self, viewport: &mut ViewportState, delta_x: f32, delta_y: f32) {
        let dx = if self.invert_x { -delta_x } else { delta_x };
        let dy = if self.invert_y { -delta_y } else { delta_y };

        viewport.camera_yaw += dx * self.orbit_sensitivity;
        viewport.camera_pitch = (viewport.camera_pitch - dy * self.orbit_sensitivity)
            .clamp(-1.4, 1.4);
    }

    /// Handle mouse drag for panning.
    pub fn pan(&self, viewport: &mut ViewportState, delta_x: f32, delta_y: f32) {
        // Pan in the camera's local XY plane
        let yaw = viewport.camera_yaw;
        let pitch = viewport.camera_pitch;

        let right = [yaw.cos(), 0.0, -yaw.sin()];
        let up = [0.0, 1.0, 0.0];

        let dx = delta_x * self.pan_sensitivity;
        let dy = delta_y * self.pan_sensitivity;

        viewport.camera_pos[0] -= right[0] * dx - up[0] * dy;
        viewport.camera_pos[1] -= right[1] * dx - up[1] * dy;
        viewport.camera_pos[2] -= right[2] * dx - up[2] * dy;
    }

    /// Handle scroll for zooming.
    pub fn zoom(&self, viewport: &mut ViewportState, delta: f32) {
        viewport.camera_distance = (viewport.camera_distance - delta * self.zoom_sensitivity)
            .clamp(self.min_distance, self.max_distance);
    }

    /// Handle mouse input.
    pub fn handle_mouse_move(
        &self,
        viewport: &mut ViewportState,
        new_pos: [f32; 2],
        orbiting: bool,
        panning: bool,
    ) {
        let delta_x = new_pos[0] - viewport.mouse_pos[0];
        let delta_y = new_pos[1] - viewport.mouse_pos[1];

        if orbiting {
            self.orbit(viewport, delta_x, delta_y);
        } else if panning {
            self.pan(viewport, delta_x, delta_y);
        }

        viewport.mouse_pos = new_pos;
    }

    /// Frame an object at the given position.
    pub fn frame_object(
        &self,
        viewport: &mut ViewportState,
        position: [f32; 3],
        size: f32,
    ) {
        // Calculate appropriate distance to frame the object
        let distance = (size * 2.0).max(self.min_distance).min(self.max_distance);
        viewport.camera_distance = distance;

        // Point camera at object
        // For now, just reset to look at origin
        viewport.camera_yaw = 0.4;
        viewport.camera_pitch = -0.4;
    }
}
