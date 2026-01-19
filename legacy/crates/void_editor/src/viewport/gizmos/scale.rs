//! Scale gizmo implementation.

use super::{Gizmo, GizmoMode, GizmoPart, InteractionState, TransformDelta, snap_value};
use crate::core::Transform;

/// Scale gizmo with axis handles and center for uniform scale.
pub struct ScaleGizmo {
    hovered: GizmoPart,
    start_distance: f32,
}

impl Default for ScaleGizmo {
    fn default() -> Self {
        Self::new()
    }
}

impl ScaleGizmo {
    pub fn new() -> Self {
        Self {
            hovered: GizmoPart::None,
            start_distance: 0.0,
        }
    }
}

impl Gizmo for ScaleGizmo {
    fn mode(&self) -> GizmoMode {
        GizmoMode::Scale
    }

    fn hit_test(
        &self,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
        scale: f32,
    ) -> Option<(GizmoPart, f32)> {
        let pos = transform.position;
        let handle_size = scale * 0.15;
        let axis_length = scale;

        // Test center (uniform scale)
        let center_size = scale * 0.2;
        let to_center = [
            pos[0] - ray_origin[0],
            pos[1] - ray_origin[1],
            pos[2] - ray_origin[2],
        ];
        let center_dist = (to_center[0].powi(2) + to_center[1].powi(2) + to_center[2].powi(2)).sqrt();

        if center_dist < center_size {
            return Some((GizmoPart::UniformCenter, center_dist));
        }

        // Test axis handles (cubes at the end of each axis)
        let handles = [
            (GizmoPart::HandleX, [axis_length, 0.0, 0.0]),
            (GizmoPart::HandleY, [0.0, axis_length, 0.0]),
            (GizmoPart::HandleZ, [0.0, 0.0, axis_length]),
        ];

        let mut closest: Option<(GizmoPart, f32)> = None;

        for (part, offset) in handles {
            let handle_pos = [
                pos[0] + offset[0],
                pos[1] + offset[1],
                pos[2] + offset[2],
            ];

            // Simple sphere test for handle
            let to_handle = [
                handle_pos[0] - ray_origin[0],
                handle_pos[1] - ray_origin[1],
                handle_pos[2] - ray_origin[2],
            ];

            let a = ray_direction[0].powi(2) + ray_direction[1].powi(2) + ray_direction[2].powi(2);
            let b = -2.0
                * (to_handle[0] * ray_direction[0]
                    + to_handle[1] * ray_direction[1]
                    + to_handle[2] * ray_direction[2]);
            let c = to_handle[0].powi(2) + to_handle[1].powi(2) + to_handle[2].powi(2)
                - handle_size.powi(2);

            let discriminant = b * b - 4.0 * a * c;

            if discriminant >= 0.0 {
                let t = (-b - discriminant.sqrt()) / (2.0 * a);
                if t > 0.0 {
                    if closest.is_none() || t < closest.unwrap().1 {
                        closest = Some((part, t));
                    }
                }
            }
        }

        closest
    }

    fn begin_interaction(
        &mut self,
        part: GizmoPart,
        transform: &Transform,
        ray_origin: [f32; 3],
        _ray_direction: [f32; 3],
    ) -> InteractionState {
        let state = InteractionState::new(part, transform);

        // Calculate initial distance from camera to object
        self.start_distance = ((transform.position[0] - ray_origin[0]).powi(2)
            + (transform.position[1] - ray_origin[1]).powi(2)
            + (transform.position[2] - ray_origin[2]).powi(2))
        .sqrt();

        state
    }

    fn update_interaction(
        &mut self,
        state: &mut InteractionState,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
        snap_value_opt: Option<f32>,
    ) -> TransformDelta {
        let mut delta = TransformDelta::default();
        delta.scale = [1.0, 1.0, 1.0]; // Default to no scaling

        // Calculate current distance
        let current_distance = ((state.start_position[0] - ray_origin[0]).powi(2)
            + (state.start_position[1] - ray_origin[1]).powi(2)
            + (state.start_position[2] - ray_origin[2]).powi(2))
        .sqrt();

        if self.start_distance < 0.001 {
            return delta;
        }

        // Scale factor based on distance change
        // Moving closer = smaller, moving away = larger
        // Actually, we want to use mouse movement along the axis

        // For simplicity, use a plane projection approach
        let plane_normal = match state.active_part {
            GizmoPart::HandleX | GizmoPart::HandleY | GizmoPart::HandleZ => [0.0, 1.0, 0.0],
            _ => [0.0, 1.0, 0.0],
        };

        if let Some(hit) = ray_plane_hit(ray_origin, ray_direction, state.start_position, plane_normal) {
            let axis = match state.active_part {
                GizmoPart::HandleX => [1.0, 0.0, 0.0],
                GizmoPart::HandleY => [0.0, 1.0, 0.0],
                GizmoPart::HandleZ => [0.0, 0.0, 1.0],
                GizmoPart::UniformCenter => [1.0, 1.0, 1.0],
                _ => return delta,
            };

            // Calculate scale based on projected distance
            let dx = hit[0] - state.start_position[0];
            let dy = hit[1] - state.start_position[1];
            let dz = hit[2] - state.start_position[2];

            let scale_factor = match state.active_part {
                GizmoPart::HandleX => 1.0 + dx * 0.5,
                GizmoPart::HandleY => 1.0 + dy * 0.5,
                GizmoPart::HandleZ => 1.0 + dz * 0.5,
                GizmoPart::UniformCenter => {
                    let dist = (dx * dx + dy * dy + dz * dz).sqrt();
                    1.0 + dist * 0.5 * if dx + dy + dz > 0.0 { 1.0 } else { -1.0 }
                }
                _ => 1.0,
            };

            let scale_factor = scale_factor.max(0.01); // Prevent negative/zero scale

            // Apply snapping
            let snapped = if let Some(snap) = snap_value_opt {
                snap_value(scale_factor, snap)
            } else {
                scale_factor
            };

            match state.active_part {
                GizmoPart::HandleX => delta.scale = [snapped, 1.0, 1.0],
                GizmoPart::HandleY => delta.scale = [1.0, snapped, 1.0],
                GizmoPart::HandleZ => delta.scale = [1.0, 1.0, snapped],
                GizmoPart::UniformCenter => delta.scale = [snapped, snapped, snapped],
                _ => {}
            }
        }

        delta
    }

    fn end_interaction(&mut self, _state: InteractionState) {
        self.start_distance = 0.0;
    }

    fn hovered_part(&self) -> GizmoPart {
        self.hovered
    }

    fn set_hovered_part(&mut self, part: GizmoPart) {
        self.hovered = part;
    }
}

fn ray_plane_hit(
    ray_origin: [f32; 3],
    ray_direction: [f32; 3],
    plane_point: [f32; 3],
    plane_normal: [f32; 3],
) -> Option<[f32; 3]> {
    let denom = plane_normal[0] * ray_direction[0]
        + plane_normal[1] * ray_direction[1]
        + plane_normal[2] * ray_direction[2];

    if denom.abs() < 0.0001 {
        return None;
    }

    let t = ((plane_point[0] - ray_origin[0]) * plane_normal[0]
        + (plane_point[1] - ray_origin[1]) * plane_normal[1]
        + (plane_point[2] - ray_origin[2]) * plane_normal[2])
        / denom;

    if t < 0.0 {
        return None;
    }

    Some([
        ray_origin[0] + ray_direction[0] * t,
        ray_origin[1] + ray_direction[1] * t,
        ray_origin[2] + ray_direction[2] * t,
    ])
}
