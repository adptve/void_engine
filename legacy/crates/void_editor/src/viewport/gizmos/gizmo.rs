//! Gizmo trait and common types.

use crate::core::Transform;

/// Part of a gizmo that can be interacted with.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GizmoPart {
    // Axes
    AxisX,
    AxisY,
    AxisZ,
    // Planes
    PlaneXY,
    PlaneXZ,
    PlaneYZ,
    // Rotation rings
    RingX,
    RingY,
    RingZ,
    RingView,
    // Scale handles
    HandleX,
    HandleY,
    HandleZ,
    UniformCenter,
    // Center (for all operations)
    Center,
    // No part
    None,
}

impl GizmoPart {
    pub fn is_axis(&self) -> bool {
        matches!(self, GizmoPart::AxisX | GizmoPart::AxisY | GizmoPart::AxisZ)
    }

    pub fn is_plane(&self) -> bool {
        matches!(self, GizmoPart::PlaneXY | GizmoPart::PlaneXZ | GizmoPart::PlaneYZ)
    }

    pub fn is_ring(&self) -> bool {
        matches!(self, GizmoPart::RingX | GizmoPart::RingY | GizmoPart::RingZ | GizmoPart::RingView)
    }
}

/// Current gizmo operation mode.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum GizmoMode {
    #[default]
    Translate,
    Rotate,
    Scale,
}

/// State during gizmo interaction.
#[derive(Clone, Debug)]
pub struct InteractionState {
    pub active_part: GizmoPart,
    pub start_position: [f32; 3],
    pub start_transform: Transform,
    pub current_delta: [f32; 3],
    pub is_snapping: bool,
}

impl InteractionState {
    pub fn new(part: GizmoPart, transform: &Transform) -> Self {
        Self {
            active_part: part,
            start_position: transform.position,
            start_transform: *transform,
            current_delta: [0.0, 0.0, 0.0],
            is_snapping: false,
        }
    }
}

/// Transform delta resulting from gizmo manipulation.
#[derive(Clone, Copy, Debug, Default)]
pub struct TransformDelta {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
}

/// Trait for transform gizmos.
pub trait Gizmo: Send + Sync {
    /// Get the gizmo mode.
    fn mode(&self) -> GizmoMode;

    /// Hit test the gizmo at the given screen position.
    /// Returns the hit part and distance along the ray.
    fn hit_test(
        &self,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
        scale: f32,
    ) -> Option<(GizmoPart, f32)>;

    /// Begin an interaction with the gizmo.
    fn begin_interaction(
        &mut self,
        part: GizmoPart,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
    ) -> InteractionState;

    /// Update the interaction and return the transform delta.
    fn update_interaction(
        &mut self,
        state: &mut InteractionState,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
        snap_value: Option<f32>,
    ) -> TransformDelta;

    /// End the interaction.
    fn end_interaction(&mut self, state: InteractionState);

    /// Get the currently hovered part.
    fn hovered_part(&self) -> GizmoPart;

    /// Set the hovered part.
    fn set_hovered_part(&mut self, part: GizmoPart);
}

/// Apply snapping to a value.
pub fn snap_value(value: f32, snap: f32) -> f32 {
    if snap > 0.0 {
        (value / snap).round() * snap
    } else {
        value
    }
}

/// Calculate ray-plane intersection.
pub fn ray_plane_intersection(
    ray_origin: [f32; 3],
    ray_direction: [f32; 3],
    plane_point: [f32; 3],
    plane_normal: [f32; 3],
) -> Option<[f32; 3]> {
    let denom = dot(plane_normal, ray_direction);

    if denom.abs() < 0.0001 {
        return None;
    }

    let t = dot(
        [
            plane_point[0] - ray_origin[0],
            plane_point[1] - ray_origin[1],
            plane_point[2] - ray_origin[2],
        ],
        plane_normal,
    ) / denom;

    if t < 0.0 {
        return None;
    }

    Some([
        ray_origin[0] + ray_direction[0] * t,
        ray_origin[1] + ray_direction[1] * t,
        ray_origin[2] + ray_direction[2] * t,
    ])
}

fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}
