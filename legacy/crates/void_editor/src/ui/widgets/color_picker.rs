//! Color picker widget.

use egui::Ui;

/// Widget for picking colors.
pub struct ColorPickerWidget;

impl ColorPickerWidget {
    /// Show RGB color picker.
    pub fn rgb(ui: &mut Ui, color: &mut [f32; 3]) -> bool {
        let mut changed = false;

        ui.horizontal(|ui| {
            if ui.color_edit_button_rgb(color).changed() {
                changed = true;
            }

            // Also show numeric values
            ui.label("R:");
            if ui.add(egui::DragValue::new(&mut color[0]).clamp_range(0.0..=1.0).speed(0.01)).changed() {
                changed = true;
            }
            ui.label("G:");
            if ui.add(egui::DragValue::new(&mut color[1]).clamp_range(0.0..=1.0).speed(0.01)).changed() {
                changed = true;
            }
            ui.label("B:");
            if ui.add(egui::DragValue::new(&mut color[2]).clamp_range(0.0..=1.0).speed(0.01)).changed() {
                changed = true;
            }
        });

        changed
    }

    /// Show RGBA color picker.
    pub fn rgba(ui: &mut Ui, color: &mut [f32; 4]) -> bool {
        let mut changed = false;

        ui.horizontal(|ui| {
            if ui.color_edit_button_rgba_unmultiplied(color).changed() {
                changed = true;
            }
        });

        changed
    }

    /// Show HDR color picker (allows values > 1.0).
    pub fn hdr(ui: &mut Ui, color: &mut [f32; 3], intensity: &mut f32) -> bool {
        let mut changed = false;

        ui.horizontal(|ui| {
            if ui.color_edit_button_rgb(color).changed() {
                changed = true;
            }

            ui.label("Intensity:");
            if ui.add(egui::DragValue::new(intensity).clamp_range(0.0..=100.0).speed(0.1)).changed() {
                changed = true;
            }
        });

        changed
    }
}
