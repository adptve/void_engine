//! Inspector panel - property editor for selected entities.

use egui::Context as EguiContext;
use crate::core::EditorState;
use super::{Panel, PanelId};

pub const INSPECTOR_PANEL_ID: PanelId = PanelId("inspector");

/// Inspector panel for editing entity properties.
pub struct InspectorPanel {
    /// Whether transform section is expanded
    transform_expanded: bool,
    /// Whether appearance section is expanded
    appearance_expanded: bool,
}

impl Default for InspectorPanel {
    fn default() -> Self {
        Self::new()
    }
}

impl InspectorPanel {
    pub fn new() -> Self {
        Self {
            transform_expanded: true,
            appearance_expanded: true,
        }
    }
}

impl Panel for InspectorPanel {
    fn id(&self) -> PanelId {
        INSPECTOR_PANEL_ID
    }

    fn name(&self) -> &str {
        "Inspector"
    }

    fn icon(&self) -> Option<&str> {
        Some("[I]")
    }

    fn ui(&mut self, ctx: &EguiContext, state: &mut EditorState) {
        egui::SidePanel::right("inspector_panel")
            .default_width(250.0)
            .show(ctx, |ui| {
                ui.heading("Inspector");
                ui.separator();

                let selected = state.selection.selected().to_vec();

                match selected.len() {
                    0 => {
                        ui.label("No entity selected");
                        ui.label("");
                        ui.label("Select an entity from the");
                        ui.label("Hierarchy panel or viewport.");
                    }
                    1 => {
                        // Single selection - full editor
                        let id = selected[0];
                        self.single_entity_inspector(ui, state, id);
                    }
                    n => {
                        // Multi-selection - limited editor
                        ui.label(format!("{} entities selected", n));
                        ui.separator();
                        self.multi_entity_inspector(ui, state, &selected);
                    }
                }
            });
    }
}

impl InspectorPanel {
    fn single_entity_inspector(&mut self, ui: &mut egui::Ui, state: &mut EditorState, id: crate::core::EntityId) {
        let mut modified = false;

        // We need to clone data to avoid borrow issues with egui
        let entity_data = state.get_entity(id).cloned();

        if let Some(mut entity) = entity_data {
            // Name
            ui.horizontal(|ui| {
                ui.label("Name:");
                if ui.text_edit_singleline(&mut entity.name).changed() {
                    modified = true;
                }
            });

            // ECS Entity info
            if let Some(ecs_entity) = entity.ecs_entity {
                ui.label(format!("ECS: {}", ecs_entity));
            }

            ui.separator();

            // Transform section
            egui::CollapsingHeader::new("Transform")
                .default_open(self.transform_expanded)
                .show(ui, |ui| {
                    // Position
                    ui.label("Position");
                    ui.horizontal(|ui| {
                        ui.label("X:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.position[0]).speed(0.1)).changed() {
                            modified = true;
                        }
                        ui.label("Y:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.position[1]).speed(0.1)).changed() {
                            modified = true;
                        }
                        ui.label("Z:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.position[2]).speed(0.1)).changed() {
                            modified = true;
                        }
                    });

                    // Rotation
                    ui.label("Rotation");
                    ui.horizontal(|ui| {
                        ui.label("X:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.rotation[0]).speed(0.01)).changed() {
                            modified = true;
                        }
                        ui.label("Y:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.rotation[1]).speed(0.01)).changed() {
                            modified = true;
                        }
                        ui.label("Z:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.rotation[2]).speed(0.01)).changed() {
                            modified = true;
                        }
                    });

                    // Scale
                    ui.label("Scale");
                    ui.horizontal(|ui| {
                        ui.label("X:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.scale[0]).speed(0.1)).changed() {
                            modified = true;
                        }
                        ui.label("Y:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.scale[1]).speed(0.1)).changed() {
                            modified = true;
                        }
                        ui.label("Z:");
                        if ui.add(egui::DragValue::new(&mut entity.transform.scale[2]).speed(0.1)).changed() {
                            modified = true;
                        }
                    });

                    // Reset buttons
                    ui.horizontal(|ui| {
                        if ui.small_button("Reset Pos").clicked() {
                            entity.transform.position = [0.0, 0.0, 0.0];
                            modified = true;
                        }
                        if ui.small_button("Reset Rot").clicked() {
                            entity.transform.rotation = [0.0, 0.0, 0.0];
                            modified = true;
                        }
                        if ui.small_button("Reset Scale").clicked() {
                            entity.transform.scale = [1.0, 1.0, 1.0];
                            modified = true;
                        }
                    });
                });

            ui.separator();

            // Appearance section
            egui::CollapsingHeader::new("Appearance")
                .default_open(self.appearance_expanded)
                .show(ui, |ui| {
                    // Mesh type
                    ui.horizontal(|ui| {
                        ui.label("Mesh:");
                        let current_mesh = entity.mesh_type;
                        egui::ComboBox::from_id_source("mesh_type")
                            .selected_text(current_mesh.name())
                            .show_ui(ui, |ui| {
                                for mesh_type in crate::core::editor_state::MeshType::all() {
                                    if ui.selectable_value(&mut entity.mesh_type, *mesh_type, mesh_type.name()).changed() {
                                        modified = true;
                                    }
                                }
                            });
                    });

                    // Color
                    ui.horizontal(|ui| {
                        ui.label("Color:");
                        let mut color = entity.color;
                        if ui.color_edit_button_rgb(&mut color).changed() {
                            entity.color = color;
                            modified = true;
                        }
                    });

                    // Visibility
                    ui.horizontal(|ui| {
                        if ui.checkbox(&mut entity.visible, "Visible").changed() {
                            modified = true;
                        }
                    });
                });

            ui.separator();

            // Actions
            ui.horizontal(|ui| {
                if ui.button("Duplicate").clicked() {
                    state.duplicate_selected();
                }
                if ui.button("Delete").clicked() {
                    state.delete_selected();
                }
            });

            // Apply modifications
            if modified {
                if let Some(e) = state.get_entity_mut(id) {
                    e.name = entity.name;
                    e.transform = entity.transform;
                    e.mesh_type = entity.mesh_type;
                    e.color = entity.color;
                    e.visible = entity.visible;
                }
                state.scene_modified = true;
            }
        }
    }

    fn multi_entity_inspector(&mut self, ui: &mut egui::Ui, state: &mut EditorState, selected: &[crate::core::EntityId]) {
        // For multi-selection, only show common operations
        ui.label("Multi-edit mode");
        ui.label("Editing multiple entities");

        ui.separator();

        // Relative move
        egui::CollapsingHeader::new("Move All")
            .default_open(false)
            .show(ui, |ui| {
                let mut delta = [0.0f32; 3];
                ui.horizontal(|ui| {
                    ui.label("X:");
                    ui.add(egui::DragValue::new(&mut delta[0]).speed(0.1));
                    ui.label("Y:");
                    ui.add(egui::DragValue::new(&mut delta[1]).speed(0.1));
                    ui.label("Z:");
                    ui.add(egui::DragValue::new(&mut delta[2]).speed(0.1));
                });

                if ui.button("Apply Move").clicked() {
                    for &id in selected {
                        if let Some(entity) = state.get_entity_mut(id) {
                            entity.transform.position[0] += delta[0];
                            entity.transform.position[1] += delta[1];
                            entity.transform.position[2] += delta[2];
                        }
                    }
                    state.scene_modified = true;
                }
            });

        ui.separator();

        // Batch actions
        ui.horizontal(|ui| {
            if ui.button("Delete All").clicked() {
                state.delete_selected();
            }
            if ui.button("Duplicate All").clicked() {
                state.duplicate_selected();
            }
        });
    }
}
