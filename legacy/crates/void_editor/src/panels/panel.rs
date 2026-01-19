//! Panel trait and registry.

use std::collections::HashMap;
use egui::Context as EguiContext;
use crate::core::EditorState;

/// Unique identifier for a panel.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct PanelId(pub &'static str);

impl std::fmt::Display for PanelId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// A dockable editor panel.
pub trait Panel: Send + Sync {
    /// Unique identifier for this panel.
    fn id(&self) -> PanelId;

    /// Display name shown in the UI.
    fn name(&self) -> &str;

    /// Icon for the panel tab (optional).
    fn icon(&self) -> Option<&str> {
        None
    }

    /// Whether the panel can be closed.
    fn closeable(&self) -> bool {
        true
    }

    /// Render the panel's UI.
    fn ui(&mut self, ctx: &EguiContext, state: &mut EditorState);

    /// Called when the panel becomes visible.
    fn on_show(&mut self, _state: &mut EditorState) {}

    /// Called when the panel becomes hidden.
    fn on_hide(&mut self, _state: &mut EditorState) {}

    /// Handle keyboard shortcuts when the panel is focused.
    fn handle_shortcut(&mut self, _key: &str, _state: &mut EditorState) -> bool {
        false
    }
}

/// Registry for managing panels.
pub struct PanelRegistry {
    panels: HashMap<PanelId, Box<dyn Panel>>,
    visibility: HashMap<PanelId, bool>,
    order: Vec<PanelId>,
}

impl Default for PanelRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl PanelRegistry {
    pub fn new() -> Self {
        Self {
            panels: HashMap::new(),
            visibility: HashMap::new(),
            order: Vec::new(),
        }
    }

    /// Register a panel.
    pub fn register(&mut self, panel: Box<dyn Panel>) {
        let id = panel.id();
        self.order.push(id);
        self.visibility.insert(id, true);
        self.panels.insert(id, panel);
    }

    /// Get a panel by ID.
    pub fn get(&self, id: PanelId) -> Option<&dyn Panel> {
        self.panels.get(&id).map(|p| p.as_ref())
    }

    /// Get a mutable panel by ID.
    pub fn get_mut(&mut self, id: PanelId) -> Option<&mut Box<dyn Panel>> {
        self.panels.get_mut(&id)
    }

    /// Check if a panel is visible.
    pub fn is_visible(&self, id: PanelId) -> bool {
        self.visibility.get(&id).copied().unwrap_or(false)
    }

    /// Show or hide a panel.
    pub fn set_visible(&mut self, id: PanelId, visible: bool, state: &mut EditorState) {
        if let Some(was_visible) = self.visibility.get(&id).copied() {
            if was_visible != visible {
                self.visibility.insert(id, visible);
                if let Some(panel) = self.panels.get_mut(&id) {
                    if visible {
                        panel.on_show(state);
                    } else {
                        panel.on_hide(state);
                    }
                }
            }
        }
    }

    /// Toggle panel visibility.
    pub fn toggle(&mut self, id: PanelId, state: &mut EditorState) {
        let visible = !self.is_visible(id);
        self.set_visible(id, visible, state);
    }

    /// Get all panel IDs in order.
    pub fn panel_ids(&self) -> &[PanelId] {
        &self.order
    }

    /// Get all visible panels.
    pub fn visible_panels(&self) -> impl Iterator<Item = PanelId> + '_ {
        self.order.iter().copied().filter(|id| self.is_visible(*id))
    }

    /// Render all visible panels.
    pub fn render_all(&mut self, ctx: &EguiContext, state: &mut EditorState) {
        let visible: Vec<PanelId> = self.visible_panels().collect();
        for id in visible {
            if let Some(panel) = self.panels.get_mut(&id) {
                panel.ui(ctx, state);
            }
        }
    }
}
