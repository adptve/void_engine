//! Gizmo renderer using egui painter for 2D overlay rendering.
//!
//! Projects 3D gizmo geometry to screen space and draws using egui shapes.

use egui::{Color32, Painter, Pos2, Stroke, Vec2};

use super::{GizmoPart, GizmoMode, colors};
use crate::core::Transform;

/// Gizmo renderer for egui overlay.
pub struct GizmoRenderer {
    /// View-projection matrix
    view_proj: [[f32; 4]; 4],
    /// Viewport size
    viewport_size: [f32; 2],
    /// Camera position for depth testing
    camera_pos: [f32; 3],
}

impl GizmoRenderer {
    /// Create a new gizmo renderer.
    pub fn new() -> Self {
        Self {
            view_proj: identity_matrix(),
            viewport_size: [1280.0, 720.0],
            camera_pos: [0.0, 0.0, 5.0],
        }
    }

    /// Update the view-projection matrix.
    pub fn set_view_projection(&mut self, view_proj: [[f32; 4]; 4]) {
        self.view_proj = view_proj;
    }

    /// Update viewport size.
    pub fn set_viewport_size(&mut self, width: f32, height: f32) {
        self.viewport_size = [width, height];
    }

    /// Update camera position.
    pub fn set_camera_pos(&mut self, pos: [f32; 3]) {
        self.camera_pos = pos;
    }

    /// Project a 3D point to screen coordinates.
    pub fn project(&self, point: [f32; 3]) -> Option<Pos2> {
        // Apply view-projection matrix
        let clip = mat4_transform_point(self.view_proj, point);

        // Check if behind camera
        if clip[3] <= 0.0 {
            return None;
        }

        // Perspective divide
        let ndc = [clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3]];

        // Check if in view frustum
        if ndc[0] < -1.0 || ndc[0] > 1.0 || ndc[1] < -1.0 || ndc[1] > 1.0 {
            return None;
        }

        // Convert to screen coordinates
        let x = (ndc[0] * 0.5 + 0.5) * self.viewport_size[0];
        let y = (1.0 - (ndc[1] * 0.5 + 0.5)) * self.viewport_size[1];

        Some(Pos2::new(x, y))
    }

    /// Render a translate gizmo.
    pub fn render_translate_gizmo(
        &self,
        painter: &Painter,
        transform: &Transform,
        hovered_part: GizmoPart,
        scale: f32,
    ) {
        let pos = transform.position;
        let arrow_length = scale;
        let arrow_head_size = scale * 0.15;

        // Draw center point
        if let Some(center) = self.project(pos) {
            painter.circle_filled(center, 4.0, colors::CENTER);
        }

        // Draw X axis (red)
        self.draw_axis_arrow(
            painter,
            pos,
            [1.0, 0.0, 0.0],
            arrow_length,
            arrow_head_size,
            if hovered_part == GizmoPart::AxisX { colors::HIGHLIGHT } else { colors::X_AXIS },
        );

        // Draw Y axis (green)
        self.draw_axis_arrow(
            painter,
            pos,
            [0.0, 1.0, 0.0],
            arrow_length,
            arrow_head_size,
            if hovered_part == GizmoPart::AxisY { colors::HIGHLIGHT } else { colors::Y_AXIS },
        );

        // Draw Z axis (blue)
        self.draw_axis_arrow(
            painter,
            pos,
            [0.0, 0.0, 1.0],
            arrow_length,
            arrow_head_size,
            if hovered_part == GizmoPart::AxisZ { colors::HIGHLIGHT } else { colors::Z_AXIS },
        );

        // Draw plane handles
        let plane_size = scale * 0.3;
        let plane_offset = scale * 0.25;

        // XY plane (blue tint)
        self.draw_plane_handle(
            painter,
            pos,
            [plane_offset, plane_offset, 0.0],
            plane_size,
            if hovered_part == GizmoPart::PlaneXY { colors::HIGHLIGHT } else { Color32::from_rgba_unmultiplied(100, 100, 255, 100) },
        );

        // XZ plane (green tint)
        self.draw_plane_handle(
            painter,
            pos,
            [plane_offset, 0.0, plane_offset],
            plane_size,
            if hovered_part == GizmoPart::PlaneXZ { colors::HIGHLIGHT } else { Color32::from_rgba_unmultiplied(100, 255, 100, 100) },
        );

        // YZ plane (red tint)
        self.draw_plane_handle(
            painter,
            pos,
            [0.0, plane_offset, plane_offset],
            plane_size,
            if hovered_part == GizmoPart::PlaneYZ { colors::HIGHLIGHT } else { Color32::from_rgba_unmultiplied(255, 100, 100, 100) },
        );
    }

    /// Render a rotate gizmo.
    pub fn render_rotate_gizmo(
        &self,
        painter: &Painter,
        transform: &Transform,
        hovered_part: GizmoPart,
        scale: f32,
    ) {
        let pos = transform.position;
        let ring_radius = scale;
        let segments = 32;

        // Draw X ring (red) - rotates around X axis
        self.draw_ring(
            painter,
            pos,
            [1.0, 0.0, 0.0],
            ring_radius,
            segments,
            if hovered_part == GizmoPart::RingX { colors::HIGHLIGHT } else { colors::X_AXIS },
        );

        // Draw Y ring (green) - rotates around Y axis
        self.draw_ring(
            painter,
            pos,
            [0.0, 1.0, 0.0],
            ring_radius,
            segments,
            if hovered_part == GizmoPart::RingY { colors::HIGHLIGHT } else { colors::Y_AXIS },
        );

        // Draw Z ring (blue) - rotates around Z axis
        self.draw_ring(
            painter,
            pos,
            [0.0, 0.0, 1.0],
            ring_radius,
            segments,
            if hovered_part == GizmoPart::RingZ { colors::HIGHLIGHT } else { colors::Z_AXIS },
        );
    }

    /// Render a scale gizmo.
    pub fn render_scale_gizmo(
        &self,
        painter: &Painter,
        transform: &Transform,
        hovered_part: GizmoPart,
        scale: f32,
    ) {
        let pos = transform.position;
        let axis_length = scale;
        let handle_size = scale * 0.12;

        // Draw center (uniform scale)
        if let Some(center) = self.project(pos) {
            let color = if hovered_part == GizmoPart::UniformCenter { colors::HIGHLIGHT } else { colors::CENTER };
            painter.circle_filled(center, 8.0, color);
        }

        // Draw X axis with cube handle
        self.draw_axis_line(
            painter,
            pos,
            [1.0, 0.0, 0.0],
            axis_length,
            if hovered_part == GizmoPart::HandleX { colors::HIGHLIGHT } else { colors::X_AXIS },
        );
        self.draw_handle_cube(
            painter,
            [pos[0] + axis_length, pos[1], pos[2]],
            handle_size,
            if hovered_part == GizmoPart::HandleX { colors::HIGHLIGHT } else { colors::X_AXIS },
        );

        // Draw Y axis with cube handle
        self.draw_axis_line(
            painter,
            pos,
            [0.0, 1.0, 0.0],
            axis_length,
            if hovered_part == GizmoPart::HandleY { colors::HIGHLIGHT } else { colors::Y_AXIS },
        );
        self.draw_handle_cube(
            painter,
            [pos[0], pos[1] + axis_length, pos[2]],
            handle_size,
            if hovered_part == GizmoPart::HandleY { colors::HIGHLIGHT } else { colors::Y_AXIS },
        );

        // Draw Z axis with cube handle
        self.draw_axis_line(
            painter,
            pos,
            [0.0, 0.0, 1.0],
            axis_length,
            if hovered_part == GizmoPart::HandleZ { colors::HIGHLIGHT } else { colors::Z_AXIS },
        );
        self.draw_handle_cube(
            painter,
            [pos[0], pos[1], pos[2] + axis_length],
            handle_size,
            if hovered_part == GizmoPart::HandleZ { colors::HIGHLIGHT } else { colors::Z_AXIS },
        );
    }

    /// Render the appropriate gizmo based on mode.
    pub fn render_gizmo(
        &self,
        painter: &Painter,
        transform: &Transform,
        mode: GizmoMode,
        hovered_part: GizmoPart,
        scale: f32,
    ) {
        match mode {
            GizmoMode::Translate => self.render_translate_gizmo(painter, transform, hovered_part, scale),
            GizmoMode::Rotate => self.render_rotate_gizmo(painter, transform, hovered_part, scale),
            GizmoMode::Scale => self.render_scale_gizmo(painter, transform, hovered_part, scale),
        }
    }

    // ========================================================================
    // Helper drawing functions
    // ========================================================================

    fn draw_axis_arrow(
        &self,
        painter: &Painter,
        origin: [f32; 3],
        direction: [f32; 3],
        length: f32,
        head_size: f32,
        color: Color32,
    ) {
        let end = [
            origin[0] + direction[0] * length,
            origin[1] + direction[1] * length,
            origin[2] + direction[2] * length,
        ];

        if let (Some(start_2d), Some(end_2d)) = (self.project(origin), self.project(end)) {
            // Draw line
            painter.line_segment([start_2d, end_2d], Stroke::new(2.5, color));

            // Draw arrow head as triangle
            let dir = Vec2::new(end_2d.x - start_2d.x, end_2d.y - start_2d.y).normalized();
            let perp = Vec2::new(-dir.y, dir.x);

            let arrow_base = end_2d - dir * head_size * 20.0;
            let arrow_left = arrow_base + perp * head_size * 10.0;
            let arrow_right = arrow_base - perp * head_size * 10.0;

            painter.add(egui::Shape::convex_polygon(
                vec![end_2d, arrow_left, arrow_right],
                color,
                Stroke::NONE,
            ));
        }
    }

    fn draw_axis_line(
        &self,
        painter: &Painter,
        origin: [f32; 3],
        direction: [f32; 3],
        length: f32,
        color: Color32,
    ) {
        let end = [
            origin[0] + direction[0] * length,
            origin[1] + direction[1] * length,
            origin[2] + direction[2] * length,
        ];

        if let (Some(start_2d), Some(end_2d)) = (self.project(origin), self.project(end)) {
            painter.line_segment([start_2d, end_2d], Stroke::new(2.5, color));
        }
    }

    fn draw_plane_handle(
        &self,
        painter: &Painter,
        origin: [f32; 3],
        offset: [f32; 3],
        size: f32,
        color: Color32,
    ) {
        let center = [
            origin[0] + offset[0],
            origin[1] + offset[1],
            origin[2] + offset[2],
        ];

        // Draw a small filled quad
        let half = size * 0.5;
        let corners = if offset[2] == 0.0 {
            // XY plane
            [
                [center[0] - half, center[1] - half, center[2]],
                [center[0] + half, center[1] - half, center[2]],
                [center[0] + half, center[1] + half, center[2]],
                [center[0] - half, center[1] + half, center[2]],
            ]
        } else if offset[1] == 0.0 {
            // XZ plane
            [
                [center[0] - half, center[1], center[2] - half],
                [center[0] + half, center[1], center[2] - half],
                [center[0] + half, center[1], center[2] + half],
                [center[0] - half, center[1], center[2] + half],
            ]
        } else {
            // YZ plane
            [
                [center[0], center[1] - half, center[2] - half],
                [center[0], center[1] + half, center[2] - half],
                [center[0], center[1] + half, center[2] + half],
                [center[0], center[1] - half, center[2] + half],
            ]
        };

        let projected: Vec<Pos2> = corners.iter()
            .filter_map(|&c| self.project(c))
            .collect();

        if projected.len() == 4 {
            // Use a brighter stroke by creating a more opaque version of the color
            let stroke_color = Color32::from_rgba_unmultiplied(
                color.r().saturating_add(50),
                color.g().saturating_add(50),
                color.b().saturating_add(50),
                color.a().saturating_add(100),
            );
            painter.add(egui::Shape::convex_polygon(
                projected,
                color,
                Stroke::new(1.0, stroke_color),
            ));
        }
    }

    fn draw_ring(
        &self,
        painter: &Painter,
        center: [f32; 3],
        axis: [f32; 3],
        radius: f32,
        segments: u32,
        color: Color32,
    ) {
        // Calculate perpendicular vectors
        let (perp1, perp2) = if axis[1].abs() > 0.9 {
            // Y axis - use X and Z
            ([1.0, 0.0, 0.0], [0.0, 0.0, 1.0])
        } else if axis[0].abs() > 0.9 {
            // X axis - use Y and Z
            ([0.0, 1.0, 0.0], [0.0, 0.0, 1.0])
        } else {
            // Z axis - use X and Y
            ([1.0, 0.0, 0.0], [0.0, 1.0, 0.0])
        };

        let mut points = Vec::new();
        for i in 0..=segments {
            let angle = (i as f32 / segments as f32) * std::f32::consts::TAU;
            let cos_a = angle.cos();
            let sin_a = angle.sin();

            let point = [
                center[0] + (perp1[0] * cos_a + perp2[0] * sin_a) * radius,
                center[1] + (perp1[1] * cos_a + perp2[1] * sin_a) * radius,
                center[2] + (perp1[2] * cos_a + perp2[2] * sin_a) * radius,
            ];

            if let Some(p2d) = self.project(point) {
                points.push(p2d);
            }
        }

        // Draw ring as line segments
        for window in points.windows(2) {
            painter.line_segment([window[0], window[1]], Stroke::new(2.5, color));
        }
    }

    fn draw_handle_cube(
        &self,
        painter: &Painter,
        center: [f32; 3],
        size: f32,
        color: Color32,
    ) {
        // Just draw a filled circle for simplicity (cube projection is complex)
        if let Some(center_2d) = self.project(center) {
            painter.circle_filled(center_2d, size * 30.0, color);
        }
    }
}

impl Default for GizmoRenderer {
    fn default() -> Self {
        Self::new()
    }
}

// Matrix math helpers

fn identity_matrix() -> [[f32; 4]; 4] {
    [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
}

fn mat4_transform_point(m: [[f32; 4]; 4], p: [f32; 3]) -> [f32; 4] {
    [
        m[0][0] * p[0] + m[1][0] * p[1] + m[2][0] * p[2] + m[3][0],
        m[0][1] * p[0] + m[1][1] * p[1] + m[2][1] * p[2] + m[3][1],
        m[0][2] * p[0] + m[1][2] * p[1] + m[2][2] * p[2] + m[3][2],
        m[0][3] * p[0] + m[1][3] * p[1] + m[2][3] * p[2] + m[3][3],
    ]
}
