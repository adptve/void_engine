//! Transform editing widget.

use egui::Ui;
use crate::core::Transform;

/// Widget for editing transforms.
pub struct TransformWidget;

impl TransformWidget {
    /// Show position editor.
    pub fn position(ui: &mut Ui, position: &mut [f32; 3]) -> bool {
        let mut changed = false;

        ui.horizontal(|ui| {
            ui.label("X:");
            if ui.add(egui::DragValue::new(&mut position[0]).speed(0.1)).changed() {
                changed = true;
            }
            ui.label("Y:");
            if ui.add(egui::DragValue::new(&mut position[1]).speed(0.1)).changed() {
                changed = true;
            }
            ui.label("Z:");
            if ui.add(egui::DragValue::new(&mut position[2]).speed(0.1)).changed() {
                changed = true;
            }
        });

        changed
    }

    /// Show rotation editor (in degrees).
    pub fn rotation(ui: &mut Ui, rotation: &mut [f32; 3]) -> bool {
        let mut changed = false;

        // Convert radians to degrees for display
        let mut degrees = [
            rotation[0].to_degrees(),
            rotation[1].to_degrees(),
            rotation[2].to_degrees(),
        ];

        ui.horizontal(|ui| {
            ui.label("X:");
            if ui.add(egui::DragValue::new(&mut degrees[0]).speed(1.0).suffix("°")).changed() {
                rotation[0] = degrees[0].to_radians();
                changed = true;
            }
            ui.label("Y:");
            if ui.add(egui::DragValue::new(&mut degrees[1]).speed(1.0).suffix("°")).changed() {
                rotation[1] = degrees[1].to_radians();
                changed = true;
            }
            ui.label("Z:");
            if ui.add(egui::DragValue::new(&mut degrees[2]).speed(1.0).suffix("°")).changed() {
                rotation[2] = degrees[2].to_radians();
                changed = true;
            }
        });

        changed
    }

    /// Show scale editor.
    pub fn scale(ui: &mut Ui, scale: &mut [f32; 3], uniform: &mut bool) -> bool {
        let mut changed = false;

        ui.horizontal(|ui| {
            ui.checkbox(uniform, "Uniform");

            if *uniform {
                let mut uniform_scale = scale[0];
                if ui.add(egui::DragValue::new(&mut uniform_scale).speed(0.1)).changed() {
                    scale[0] = uniform_scale;
                    scale[1] = uniform_scale;
                    scale[2] = uniform_scale;
                    changed = true;
                }
            } else {
                ui.label("X:");
                if ui.add(egui::DragValue::new(&mut scale[0]).speed(0.1)).changed() {
                    changed = true;
                }
                ui.label("Y:");
                if ui.add(egui::DragValue::new(&mut scale[1]).speed(0.1)).changed() {
                    changed = true;
                }
                ui.label("Z:");
                if ui.add(egui::DragValue::new(&mut scale[2]).speed(0.1)).changed() {
                    changed = true;
                }
            }
        });

        changed
    }

    /// Show full transform editor.
    pub fn full(ui: &mut Ui, transform: &mut Transform) -> bool {
        let mut changed = false;

        ui.label("Position");
        if Self::position(ui, &mut transform.position) {
            changed = true;
        }

        ui.label("Rotation");
        if Self::rotation(ui, &mut transform.rotation) {
            changed = true;
        }

        ui.label("Scale");
        let mut uniform = false;
        if Self::scale(ui, &mut transform.scale, &mut uniform) {
            changed = true;
        }

        changed
    }
}
