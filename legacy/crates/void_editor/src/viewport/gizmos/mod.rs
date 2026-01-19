//! Transform gizmos for visual manipulation.
//!
//! Provides translate, rotate, and scale gizmos similar to Unity/Unreal.

mod gizmo;
mod translate;
mod rotate;
mod scale;
mod renderer;

pub use gizmo::{
    Gizmo, GizmoPart, GizmoMode, InteractionState, TransformDelta,
    snap_value, ray_plane_intersection,
};
pub use translate::TranslateGizmo;
pub use rotate::RotateGizmo;
pub use scale::ScaleGizmo;
pub use renderer::GizmoRenderer;

/// Gizmo axis colors.
pub mod colors {
    use egui::Color32;

    pub const X_AXIS: Color32 = Color32::from_rgb(255, 100, 100);
    pub const Y_AXIS: Color32 = Color32::from_rgb(100, 255, 100);
    pub const Z_AXIS: Color32 = Color32::from_rgb(100, 100, 255);
    pub const HIGHLIGHT: Color32 = Color32::from_rgb(255, 255, 100);
    pub const CENTER: Color32 = Color32::from_rgb(255, 255, 255);
}
