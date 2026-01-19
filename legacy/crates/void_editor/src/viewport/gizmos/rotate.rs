//! Rotate gizmo implementation.

use super::{Gizmo, GizmoMode, GizmoPart, InteractionState, TransformDelta, snap_value};
use crate::core::Transform;

/// Rotate gizmo with rotation rings.
pub struct RotateGizmo {
    hovered: GizmoPart,
    start_angle: f32,
}

impl Default for RotateGizmo {
    fn default() -> Self {
        Self::new()
    }
}

impl RotateGizmo {
    pub fn new() -> Self {
        Self {
            hovered: GizmoPart::None,
            start_angle: 0.0,
        }
    }
}

impl Gizmo for RotateGizmo {
    fn mode(&self) -> GizmoMode {
        GizmoMode::Rotate
    }

    fn hit_test(
        &self,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
        scale: f32,
    ) -> Option<(GizmoPart, f32)> {
        let pos = transform.position;
        let ring_radius = scale;
        let ring_thickness = scale * 0.1;

        let rings = [
            (GizmoPart::RingX, [1.0, 0.0, 0.0]),
            (GizmoPart::RingY, [0.0, 1.0, 0.0]),
            (GizmoPart::RingZ, [0.0, 0.0, 1.0]),
        ];

        let mut closest: Option<(GizmoPart, f32)> = None;

        for (part, normal) in rings {
            // Ray-plane intersection
            let denom = normal[0] * ray_direction[0]
                + normal[1] * ray_direction[1]
                + normal[2] * ray_direction[2];

            if denom.abs() < 0.0001 {
                continue;
            }

            let t = ((pos[0] - ray_origin[0]) * normal[0]
                + (pos[1] - ray_origin[1]) * normal[1]
                + (pos[2] - ray_origin[2]) * normal[2])
                / denom;

            if t < 0.0 {
                continue;
            }

            let hit = [
                ray_origin[0] + ray_direction[0] * t,
                ray_origin[1] + ray_direction[1] * t,
                ray_origin[2] + ray_direction[2] * t,
            ];

            // Distance from hit point to ring center (in the plane)
            let dx = hit[0] - pos[0];
            let dy = hit[1] - pos[1];
            let dz = hit[2] - pos[2];

            let dist_from_center = match part {
                GizmoPart::RingX => (dy * dy + dz * dz).sqrt(),
                GizmoPart::RingY => (dx * dx + dz * dz).sqrt(),
                GizmoPart::RingZ => (dx * dx + dy * dy).sqrt(),
                _ => 0.0,
            };

            // Check if on the ring
            if (dist_from_center - ring_radius).abs() < ring_thickness {
                if closest.is_none() || t < closest.unwrap().1 {
                    closest = Some((part, t));
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
        ray_direction: [f32; 3],
    ) -> InteractionState {
        let state = InteractionState::new(part, transform);

        // Calculate initial angle
        let normal = match part {
            GizmoPart::RingX => [1.0, 0.0, 0.0],
            GizmoPart::RingY => [0.0, 1.0, 0.0],
            GizmoPart::RingZ => [0.0, 0.0, 1.0],
            _ => [0.0, 1.0, 0.0],
        };

        if let Some(hit) = ray_plane_hit(ray_origin, ray_direction, transform.position, normal) {
            let dx = hit[0] - transform.position[0];
            let dy = hit[1] - transform.position[1];
            let dz = hit[2] - transform.position[2];

            self.start_angle = match part {
                GizmoPart::RingX => dy.atan2(dz),
                GizmoPart::RingY => dx.atan2(dz),
                GizmoPart::RingZ => dx.atan2(dy),
                _ => 0.0,
            };
        }

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

        let normal = match state.active_part {
            GizmoPart::RingX => [1.0, 0.0, 0.0],
            GizmoPart::RingY => [0.0, 1.0, 0.0],
            GizmoPart::RingZ => [0.0, 0.0, 1.0],
            _ => return delta,
        };

        if let Some(hit) = ray_plane_hit(ray_origin, ray_direction, state.start_position, normal) {
            let dx = hit[0] - state.start_position[0];
            let dy = hit[1] - state.start_position[1];
            let dz = hit[2] - state.start_position[2];

            let current_angle = match state.active_part {
                GizmoPart::RingX => dy.atan2(dz),
                GizmoPart::RingY => dx.atan2(dz),
                GizmoPart::RingZ => dx.atan2(dy),
                _ => 0.0,
            };

            let mut angle_delta = current_angle - self.start_angle;

            // Apply snapping (in radians, convert from degrees if snap is in degrees)
            if let Some(snap) = snap_value_opt {
                let snap_rad = snap.to_radians();
                angle_delta = snap_value(angle_delta, snap_rad);
            }

            match state.active_part {
                GizmoPart::RingX => delta.rotation[0] = angle_delta,
                GizmoPart::RingY => delta.rotation[1] = angle_delta,
                GizmoPart::RingZ => delta.rotation[2] = angle_delta,
                _ => {}
            }

            state.current_delta = delta.rotation;
        }

        delta
    }

    fn end_interaction(&mut self, _state: InteractionState) {
        self.start_angle = 0.0;
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
