//! Hierarchy panel - scene entity tree view.

use egui::Context as EguiContext;
use crate::core::{EditorState, EntityId, SelectionMode};
use super::{Panel, PanelId};

pub const HIERARCHY_PANEL_ID: PanelId = PanelId("hierarchy");

/// Scene hierarchy panel showing all entities in a tree view.
pub struct HierarchyPanel {
    /// Search filter text
    filter_text: String,
    /// Whether to show search box
    show_search: bool,
}

impl Default for HierarchyPanel {
    fn default() -> Self {
        Self::new()
    }
}

impl HierarchyPanel {
    pub fn new() -> Self {
        Self {
            filter_text: String::new(),
            show_search: false,
        }
    }
}

impl Panel for HierarchyPanel {
    fn id(&self) -> PanelId {
        HIERARCHY_PANEL_ID
    }

    fn name(&self) -> &str {
        "Hierarchy"
    }

    fn icon(&self) -> Option<&str> {
        Some("[H]")
    }

    fn ui(&mut self, ctx: &EguiContext, state: &mut EditorState) {
        egui::SidePanel::left("hierarchy_panel")
            .default_width(200.0)
            .show(ctx, |ui| {
                ui.heading("Hierarchy");
                ui.separator();

                // Toolbar
                ui.horizontal(|ui| {
                    // Add entity dropdown
                    ui.menu_button("+ Add", |ui| {
                        for mesh_type in crate::core::editor_state::MeshType::all() {
                            if ui.button(mesh_type.name()).clicked() {
                                state.create_entity(mesh_type.name().to_string(), *mesh_type);
                                ui.close_menu();
                            }
                        }
                    });

                    // Search toggle
                    if ui.small_button("Search").clicked() {
                        self.show_search = !self.show_search;
                    }

                    // ECS info
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        ui.label(format!("{}", state.entities.len()));
                    });
                });

                // Search box
                if self.show_search {
                    ui.horizontal(|ui| {
                        ui.label("Filter:");
                        ui.text_edit_singleline(&mut self.filter_text);
                        if ui.small_button("X").clicked() {
                            self.filter_text.clear();
                        }
                    });
                }

                ui.separator();

                // Collect entity display data to avoid borrow issues
                #[derive(Clone)]
                struct EntityDisplayData {
                    id: EntityId,
                    name: String,
                    visible: bool,
                    mesh_type: crate::core::MeshType,
                    is_selected: bool,
                    is_primary: bool,
                }

                let entity_data: Vec<EntityDisplayData> = state.entities.iter()
                    .filter(|e| {
                        if self.filter_text.is_empty() {
                            true
                        } else {
                            e.name.to_lowercase().contains(&self.filter_text.to_lowercase())
                        }
                    })
                    .map(|e| EntityDisplayData {
                        id: e.id,
                        name: e.name.clone(),
                        visible: e.visible,
                        mesh_type: e.mesh_type,
                        is_selected: state.selection.is_selected(e.id),
                        is_primary: state.selection.is_primary(e.id),
                    })
                    .collect();

                let is_empty = state.entities.is_empty();

                // Entity list
                egui::ScrollArea::vertical().show(ui, |ui| {
                    let mut selection_action: Option<(EntityId, SelectionMode)> = None;
                    let mut duplicate_action: Option<EntityId> = None;
                    let mut delete_action: Option<EntityId> = None;

                    for entity in &entity_data {
                        ui.horizontal(|ui| {
                            // Visibility toggle
                            let vis_icon = if entity.visible { "O" } else { "-" };
                            if ui.small_button(vis_icon).clicked() {
                                // Toggle visibility would need a command
                            }

                            // Mesh type icon
                            let icon = match entity.mesh_type {
                                crate::core::MeshType::Cube => "[C]",
                                crate::core::MeshType::Sphere => "[S]",
                                crate::core::MeshType::Cylinder => "[Y]",
                                crate::core::MeshType::Diamond => "[D]",
                                crate::core::MeshType::Torus => "[T]",
                                crate::core::MeshType::Plane => "[P]",
                            };
                            ui.label(icon);

                            // Entity name - selectable
                            let label = if entity.is_primary {
                                egui::RichText::new(&entity.name).strong()
                            } else {
                                egui::RichText::new(&entity.name)
                            };

                            let response = ui.selectable_label(entity.is_selected, label);

                            if response.clicked() {
                                // Determine selection mode from modifiers
                                let modifiers = ui.input(|i| i.modifiers);
                                let mode = SelectionMode::from_modifiers(modifiers.shift, modifiers.ctrl);
                                selection_action = Some((entity.id, mode));
                            }

                            // Context menu
                            let entity_id = entity.id;
                            response.context_menu(|ui| {
                                if ui.button("Duplicate").clicked() {
                                    duplicate_action = Some(entity_id);
                                    ui.close_menu();
                                }
                                if ui.button("Delete").clicked() {
                                    delete_action = Some(entity_id);
                                    ui.close_menu();
                                }
                                ui.separator();
                                if ui.button("Focus").clicked() {
                                    // Would focus camera on entity
                                    ui.close_menu();
                                }
                            });
                        });
                    }

                    // Apply deferred actions
                    if let Some((id, mode)) = selection_action {
                        state.selection.select(id, mode);
                    }

                    if let Some(id) = duplicate_action {
                        state.selection.select(id, SelectionMode::Replace);
                        state.duplicate_selected();
                    }

                    if let Some(id) = delete_action {
                        state.selection.select(id, SelectionMode::Replace);
                        state.delete_selected();
                    }

                    // Empty state message
                    if is_empty {
                        ui.label("No entities in scene");
                        ui.label("");
                        ui.label("Use + Add or Create menu");
                        ui.label("to add entities.");
                    }
                });
            });
    }

    fn handle_shortcut(&mut self, key: &str, state: &mut EditorState) -> bool {
        match key {
            "Delete" => {
                state.delete_selected();
                true
            }
            "Ctrl+D" => {
                state.duplicate_selected();
                true
            }
            "Ctrl+A" => {
                state.select_all();
                true
            }
            "Escape" => {
                state.deselect_all();
                true
            }
            _ => false,
        }
    }
}
