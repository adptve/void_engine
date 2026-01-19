//! Gizmo state management.
//!
//! Tracks current gizmo mode, interaction state, and settings.

use crate::viewport::gizmos::{
    Gizmo, GizmoMode, GizmoPart, GizmoRenderer, InteractionState, TransformDelta,
    TranslateGizmo, RotateGizmo, ScaleGizmo,
};
use crate::core::Transform;

/// Space for gizmo operations.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum GizmoSpace {
    /// Local space (relative to object rotation)
    Local,
    /// World space (aligned to world axes)
    #[default]
    World,
}

/// Snap settings for gizmo operations.
#[derive(Clone, Copy, Debug)]
pub struct SnapSettings {
    /// Enable snap
    pub enabled: bool,
    /// Translation snap value (units)
    pub translate: f32,
    /// Rotation snap value (degrees)
    pub rotate: f32,
    /// Scale snap value (multiplier)
    pub scale: f32,
}

impl Default for SnapSettings {
    fn default() -> Self {
        Self {
            enabled: false,
            translate: 0.5,
            rotate: 15.0,
            scale: 0.1,
        }
    }
}

/// State for gizmo system.
pub struct GizmoState {
    /// Current gizmo mode
    pub mode: GizmoMode,
    /// Current space (local/world)
    pub space: GizmoSpace,
    /// Snap settings
    pub snap: SnapSettings,
    /// Currently hovered gizmo part
    pub hovered_part: GizmoPart,
    /// Active interaction state (if interacting)
    pub interaction: Option<InteractionState>,
    /// Gizmo scale (based on camera distance)
    pub gizmo_scale: f32,
    /// Gizmo renderer
    pub renderer: GizmoRenderer,

    // Gizmo instances
    translate_gizmo: TranslateGizmo,
    rotate_gizmo: RotateGizmo,
    scale_gizmo: ScaleGizmo,
}

impl Default for GizmoState {
    fn default() -> Self {
        Self::new()
    }
}

impl GizmoState {
    /// Create a new gizmo state.
    pub fn new() -> Self {
        Self {
            mode: GizmoMode::Translate,
            space: GizmoSpace::World,
            snap: SnapSettings::default(),
            hovered_part: GizmoPart::None,
            interaction: None,
            gizmo_scale: 1.0,
            renderer: GizmoRenderer::new(),
            translate_gizmo: TranslateGizmo::new(),
            rotate_gizmo: RotateGizmo::new(),
            scale_gizmo: ScaleGizmo::new(),
        }
    }

    /// Set the gizmo mode.
    pub fn set_mode(&mut self, mode: GizmoMode) {
        if self.interaction.is_none() {
            self.mode = mode;
            self.hovered_part = GizmoPart::None;
        }
    }

    /// Toggle between local and world space.
    pub fn toggle_space(&mut self) {
        self.space = match self.space {
            GizmoSpace::Local => GizmoSpace::World,
            GizmoSpace::World => GizmoSpace::Local,
        };
    }

    /// Toggle snap on/off.
    pub fn toggle_snap(&mut self) {
        self.snap.enabled = !self.snap.enabled;
    }

    /// Check if currently interacting with a gizmo.
    pub fn is_interacting(&self) -> bool {
        self.interaction.is_some()
    }

    /// Get the current active gizmo.
    fn current_gizmo(&self) -> &dyn Gizmo {
        match self.mode {
            GizmoMode::Translate => &self.translate_gizmo,
            GizmoMode::Rotate => &self.rotate_gizmo,
            GizmoMode::Scale => &self.scale_gizmo,
        }
    }

    /// Get the current active gizmo mutably.
    fn current_gizmo_mut(&mut self) -> &mut dyn Gizmo {
        match self.mode {
            GizmoMode::Translate => &mut self.translate_gizmo,
            GizmoMode::Rotate => &mut self.rotate_gizmo,
            GizmoMode::Scale => &mut self.scale_gizmo,
        }
    }

    /// Hit test the current gizmo.
    pub fn hit_test(
        &mut self,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
    ) -> GizmoPart {
        if let Some((part, _)) = self.current_gizmo().hit_test(
            transform,
            ray_origin,
            ray_direction,
            self.gizmo_scale,
        ) {
            self.hovered_part = part;
            part
        } else {
            self.hovered_part = GizmoPart::None;
            GizmoPart::None
        }
    }

    /// Begin a gizmo interaction.
    pub fn begin_interaction(
        &mut self,
        transform: &Transform,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
    ) -> bool {
        let part = self.hovered_part;
        if part != GizmoPart::None {
            let state = self.current_gizmo_mut().begin_interaction(
                part,
                transform,
                ray_origin,
                ray_direction,
            );
            self.interaction = Some(state);
            true
        } else {
            false
        }
    }

    /// Update the current interaction.
    pub fn update_interaction(
        &mut self,
        ray_origin: [f32; 3],
        ray_direction: [f32; 3],
    ) -> Option<TransformDelta> {
        // Take state out to avoid borrow conflict
        let mut state = self.interaction.take()?;

        let snap_value = if self.snap.enabled {
            Some(match self.mode {
                GizmoMode::Translate => self.snap.translate,
                GizmoMode::Rotate => self.snap.rotate,
                GizmoMode::Scale => self.snap.scale,
            })
        } else {
            None
        };

        let delta = self.current_gizmo_mut().update_interaction(
            &mut state,
            ray_origin,
            ray_direction,
            snap_value,
        );

        // Put state back
        self.interaction = Some(state);
        Some(delta)
    }

    /// End the current interaction.
    pub fn end_interaction(&mut self) -> Option<InteractionState> {
        if let Some(state) = self.interaction.take() {
            self.current_gizmo_mut().end_interaction(state.clone());
            Some(state)
        } else {
            None
        }
    }

    /// Update gizmo scale based on camera distance.
    pub fn update_scale(&mut self, camera_pos: [f32; 3], target_pos: [f32; 3]) {
        let dx = camera_pos[0] - target_pos[0];
        let dy = camera_pos[1] - target_pos[1];
        let dz = camera_pos[2] - target_pos[2];
        let distance = (dx * dx + dy * dy + dz * dz).sqrt();

        // Scale gizmo based on distance (constant screen size)
        self.gizmo_scale = distance * 0.15;
        self.gizmo_scale = self.gizmo_scale.clamp(0.1, 10.0);
    }

    /// Update renderer with view-projection matrix.
    pub fn update_renderer(
        &mut self,
        view_proj: [[f32; 4]; 4],
        viewport_size: [f32; 2],
        camera_pos: [f32; 3],
    ) {
        self.renderer.set_view_projection(view_proj);
        self.renderer.set_viewport_size(viewport_size[0], viewport_size[1]);
        self.renderer.set_camera_pos(camera_pos);
    }
}
