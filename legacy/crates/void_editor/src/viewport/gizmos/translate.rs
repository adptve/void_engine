//! Translate gizmo implementation.

use super::{Gizmo, GizmoMode, GizmoPart, InteractionState, TransformDelta, snap_value, ray_plane_intersection};
use crate::core::Transform;

/// Translate gizmo with axis arrows and plane handles.
pub struct TranslateGizmo {
    hovered: GizmoPart,
    interaction_start: Option<[f32; 3]>,
}

impl Default for TranslateGizmo {
    fn default() -> Self {
        Self::new()
    }
}

impl TranslateGizmo {
    pub fn new() -> Self {
        Self {
            hovered: GizmoPart::None,
            interaction_start: None,
        }
    }
}

impl Gizmo for TranslateGizmo {
    fn mode(&self) -> GizmoMode {
        GizmoMode::Translate
    }

    fn hit_test(
        &self,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
        scale: f32,
    ) -> Option<(GizmoPart, f32)> {
        let pos = transform.position;
        let arrow_length = scale;
        let hit_radius = scale * 0.1;

        // Test each axis
        let axes = [
            (GizmoPart::AxisX, [1.0, 0.0, 0.0]),
            (GizmoPart::AxisY, [0.0, 1.0, 0.0]),
            (GizmoPart::AxisZ, [0.0, 0.0, 1.0]),
        ];

        let mut closest: Option<(GizmoPart, f32)> = None;

        for (part, dir) in axes {
            // Line-ray distance test
            let end = [
                pos[0] + dir[0] * arrow_length,
                pos[1] + dir[1] * arrow_length,
                pos[2] + dir[2] * arrow_length,
            ];

            if let Some(dist) = line_ray_distance(pos, end, ray_origin, ray_direction) {
                if dist < hit_radius {
                    let t = point_on_ray_closest_to_line(pos, end, ray_origin, ray_direction);
                    if closest.is_none() || t < closest.unwrap().1 {
                        closest = Some((part, t));
                    }
                }
            }
        }

        // Test plane handles
        let plane_size = scale * 0.3;
        let plane_offset = scale * 0.2;

        let planes = [
            (GizmoPart::PlaneXY, [0.0, 0.0, 1.0], [plane_offset, plane_offset, 0.0]),
            (GizmoPart::PlaneXZ, [0.0, 1.0, 0.0], [plane_offset, 0.0, plane_offset]),
            (GizmoPart::PlaneYZ, [1.0, 0.0, 0.0], [0.0, plane_offset, plane_offset]),
        ];

        for (part, normal, offset) in planes {
            let plane_center = [
                pos[0] + offset[0],
                pos[1] + offset[1],
                pos[2] + offset[2],
            ];

            if let Some(hit) = ray_plane_intersection(ray_origin, ray_direction, plane_center, normal) {
                let dx = hit[0] - plane_center[0];
                let dy = hit[1] - plane_center[1];
                let dz = hit[2] - plane_center[2];

                // Check if within plane bounds
                let in_bounds = match part {
                    GizmoPart::PlaneXY => dx.abs() < plane_size && dy.abs() < plane_size,
                    GizmoPart::PlaneXZ => dx.abs() < plane_size && dz.abs() < plane_size,
                    GizmoPart::PlaneYZ => dy.abs() < plane_size && dz.abs() < plane_size,
                    _ => false,
                };

                if in_bounds {
                    let dist = ((hit[0] - ray_origin[0]).powi(2)
                        + (hit[1] - ray_origin[1]).powi(2)
                        + (hit[2] - ray_origin[2]).powi(2))
                    .sqrt();

                    if closest.is_none() || dist < closest.unwrap().1 {
                        closest = Some((part, dist));
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
        ray_direction: [f32; 3],
    ) -> InteractionState {
        let state = InteractionState::new(part, transform);

        // Calculate initial intersection point
        let plane_normal = match part {
            GizmoPart::AxisX => [0.0, 1.0, 0.0],
            GizmoPart::AxisY => [0.0, 0.0, 1.0],
            GizmoPart::AxisZ => [0.0, 1.0, 0.0],
            GizmoPart::PlaneXY => [0.0, 0.0, 1.0],
            GizmoPart::PlaneXZ => [0.0, 1.0, 0.0],
            GizmoPart::PlaneYZ => [1.0, 0.0, 0.0],
            _ => [0.0, 1.0, 0.0],
        };

        self.interaction_start = ray_plane_intersection(
            ray_origin,
            ray_direction,
            transform.position,
            plane_normal,
        );

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

        let plane_normal = match state.active_part {
            GizmoPart::AxisX => [0.0, 1.0, 0.0],
            GizmoPart::AxisY => [0.0, 0.0, 1.0],
            GizmoPart::AxisZ => [0.0, 1.0, 0.0],
            GizmoPart::PlaneXY => [0.0, 0.0, 1.0],
            GizmoPart::PlaneXZ => [0.0, 1.0, 0.0],
            GizmoPart::PlaneYZ => [1.0, 0.0, 0.0],
            _ => return delta,
        };

        if let (Some(start), Some(current)) = (
            self.interaction_start,
            ray_plane_intersection(ray_origin, ray_direction, state.start_position, plane_normal),
        ) {
            let raw_delta = [
                current[0] - start[0],
                current[1] - start[1],
                current[2] - start[2],
            ];

            // Constrain to axis if needed
            let constrained = match state.active_part {
                GizmoPart::AxisX => [raw_delta[0], 0.0, 0.0],
                GizmoPart::AxisY => [0.0, raw_delta[1], 0.0],
                GizmoPart::AxisZ => [0.0, 0.0, raw_delta[2]],
                GizmoPart::PlaneXY => [raw_delta[0], raw_delta[1], 0.0],
                GizmoPart::PlaneXZ => [raw_delta[0], 0.0, raw_delta[2]],
                GizmoPart::PlaneYZ => [0.0, raw_delta[1], raw_delta[2]],
                _ => raw_delta,
            };

            // Apply snapping
            let snapped = if let Some(snap) = snap_value_opt {
                [
                    snap_value(constrained[0], snap),
                    snap_value(constrained[1], snap),
                    snap_value(constrained[2], snap),
                ]
            } else {
                constrained
            };

            delta.position = snapped;
            state.current_delta = snapped;
        }

        delta
    }

    fn end_interaction(&mut self, _state: InteractionState) {
        self.interaction_start = None;
    }

    fn hovered_part(&self) -> GizmoPart {
        self.hovered
    }

    fn set_hovered_part(&mut self, part: GizmoPart) {
        self.hovered = part;
    }
}

// Helper functions

fn line_ray_distance(
    line_start: [f32; 3],
    line_end: [f32; 3],
    ray_origin: [f32; 3],
    ray_direction: [f32; 3],
) -> Option<f32> {
    // Simplified line-ray distance calculation
    let line_dir = [
        line_end[0] - line_start[0],
        line_end[1] - line_start[1],
        line_end[2] - line_start[2],
    ];

    let line_len = (line_dir[0].powi(2) + line_dir[1].powi(2) + line_dir[2].powi(2)).sqrt();
    if line_len < 0.0001 {
        return None;
    }

    // Project ray origin onto line
    let to_origin = [
        ray_origin[0] - line_start[0],
        ray_origin[1] - line_start[1],
        ray_origin[2] - line_start[2],
    ];

    let t = (to_origin[0] * line_dir[0] + to_origin[1] * line_dir[1] + to_origin[2] * line_dir[2])
        / (line_len * line_len);

    let t_clamped = t.clamp(0.0, 1.0);

    let closest = [
        line_start[0] + line_dir[0] * t_clamped,
        line_start[1] + line_dir[1] * t_clamped,
        line_start[2] + line_dir[2] * t_clamped,
    ];

    let dist = ((closest[0] - ray_origin[0]).powi(2)
        + (closest[1] - ray_origin[1]).powi(2)
        + (closest[2] - ray_origin[2]).powi(2))
    .sqrt();

    Some(dist)
}

fn point_on_ray_closest_to_line(
    line_start: [f32; 3],
    line_end: [f32; 3],
    ray_origin: [f32; 3],
    _ray_direction: [f32; 3],
) -> f32 {
    // Return distance from ray origin to line midpoint (simplified)
    let mid = [
        (line_start[0] + line_end[0]) / 2.0,
        (line_start[1] + line_end[1]) / 2.0,
        (line_start[2] + line_end[2]) / 2.0,
    ];

    ((mid[0] - ray_origin[0]).powi(2)
        + (mid[1] - ray_origin[1]).powi(2)
        + (mid[2] - ray_origin[2]).powi(2))
    .sqrt()
}
