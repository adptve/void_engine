//! UI Widgets - Reusable UI components

use crate::UiContext;

/// Debug panel widget for displaying stats
pub struct DebugPanel;

impl DebugPanel {
    /// Draw a debug panel with the given stats
    pub fn draw(
        ctx: &mut UiContext,
        x: f32,
        y: f32,
        title: &str,
        stats: &[(&str, &str, StatType)],
    ) {
        // Extract all theme values upfront to avoid borrow conflicts
        let padding = ctx.theme.padding;
        let line_height = ctx.theme.line_height_px();
        let scale = ctx.theme.text_scale;
        let border_width = ctx.theme.border_width;
        let panel_bg = ctx.theme.colors.panel_bg;
        let panel_border = ctx.theme.colors.panel_border;
        let accent = ctx.theme.colors.accent;
        let text_color = ctx.theme.colors.text;
        let text_dim = ctx.theme.colors.text_dim;
        let success = ctx.theme.colors.success;
        let warning = ctx.theme.colors.warning;
        let error = ctx.theme.colors.error;
        let info = ctx.theme.colors.info;

        // Calculate panel dimensions
        let mut max_width: f32 = ctx.measure_text(title, scale);
        for (label, value, _) in stats {
            let line_width = ctx.measure_text(label, scale) + ctx.measure_text(value, scale) + 20.0;
            max_width = max_width.max(line_width);
        }

        let panel_width = max_width + padding * 2.0;
        let panel_height = line_height * (stats.len() + 1) as f32 + padding * 2.0 + 4.0;

        // Draw panel background
        ctx.draw_rect(x, y, panel_width, panel_height, panel_bg);

        // Draw border (top)
        ctx.draw_rect(x, y, panel_width, border_width, panel_border);
        // Draw border (bottom)
        ctx.draw_rect(x, y + panel_height - border_width, panel_width, border_width, panel_border);
        // Draw border (left)
        ctx.draw_rect(x, y, border_width, panel_height, panel_border);
        // Draw border (right)
        ctx.draw_rect(x + panel_width - border_width, y, border_width, panel_height, panel_border);

        // Draw title
        let text_x = x + padding;
        let mut text_y = y + padding;
        ctx.draw_text(title, text_x, text_y, accent, scale);
        text_y += line_height + 4.0;

        // Draw stats
        for (label, value, stat_type) in stats {
            let value_color = match stat_type {
                StatType::Normal => text_color,
                StatType::Good => success,
                StatType::Warning => warning,
                StatType::Bad => error,
                StatType::Info => info,
            };

            ctx.draw_text(label, text_x, text_y, text_dim, scale);
            let label_width = ctx.measure_text(label, scale);
            ctx.draw_text(value, text_x + label_width, text_y, value_color, scale);
            text_y += line_height;
        }
    }
}

/// Type of statistic (affects color)
#[derive(Clone, Copy, Debug)]
pub enum StatType {
    /// Normal text color
    Normal,
    /// Good/positive (green)
    Good,
    /// Warning (yellow)
    Warning,
    /// Bad/error (red)
    Bad,
    /// Informational (blue)
    Info,
}

/// Label widget
pub struct Label;

impl Label {
    /// Draw a simple text label
    pub fn draw(ctx: &mut UiContext, x: f32, y: f32, text: &str) {
        let color = ctx.theme.colors.text;
        let scale = ctx.theme.text_scale;
        ctx.draw_text(text, x, y, color, scale);
    }

    /// Draw a colored label
    pub fn draw_colored(ctx: &mut UiContext, x: f32, y: f32, text: &str, color: [f32; 4]) {
        let scale = ctx.theme.text_scale;
        ctx.draw_text(text, x, y, color, scale);
    }
}

/// Progress bar widget
pub struct ProgressBar;

impl ProgressBar {
    /// Draw a horizontal progress bar
    pub fn draw(
        ctx: &mut UiContext,
        x: f32,
        y: f32,
        width: f32,
        height: f32,
        progress: f32, // 0.0 to 1.0
        color: Option<[f32; 4]>,
    ) {
        // Extract theme values upfront
        let border_width = ctx.theme.border_width;
        let panel_border = ctx.theme.colors.panel_border;
        let accent = ctx.theme.colors.accent;

        let progress = progress.clamp(0.0, 1.0);

        // Background
        ctx.draw_rect(x, y, width, height, [0.2, 0.2, 0.2, 1.0]);

        // Fill
        let fill_color = color.unwrap_or(accent);
        let fill_width = width * progress;
        if fill_width > 0.0 {
            ctx.draw_rect(x, y, fill_width, height, fill_color);
        }

        // Border
        ctx.draw_rect(x, y, width, border_width, panel_border);
        ctx.draw_rect(x, y + height - border_width, width, border_width, panel_border);
        ctx.draw_rect(x, y, border_width, height, panel_border);
        ctx.draw_rect(x + width - border_width, y, border_width, height, panel_border);
    }
}

/// Frame time graph widget
pub struct FrameTimeGraph;

impl FrameTimeGraph {
    /// Draw a frame time history graph
    pub fn draw(
        ctx: &mut UiContext,
        x: f32,
        y: f32,
        width: f32,
        height: f32,
        frame_times: &[f32], // Frame times in ms
        target_fps: f32,
    ) {
        // Extract theme values upfront
        let panel_border = ctx.theme.colors.panel_border;
        let success = ctx.theme.colors.success;
        let warning = ctx.theme.colors.warning;
        let error = ctx.theme.colors.error;

        // Background
        ctx.draw_rect(x, y, width, height, [0.1, 0.1, 0.12, 0.9]);

        // Border
        ctx.draw_rect(x, y, width, 1.0, panel_border);
        ctx.draw_rect(x, y + height - 1.0, width, 1.0, panel_border);
        ctx.draw_rect(x, y, 1.0, height, panel_border);
        ctx.draw_rect(x + width - 1.0, y, 1.0, height, panel_border);

        if frame_times.is_empty() {
            return;
        }

        let target_frame_time = 1000.0 / target_fps;
        let max_frame_time = target_frame_time * 3.0; // Show up to 3x target

        // Draw target line
        let target_y = y + height - (target_frame_time / max_frame_time * height);
        ctx.draw_rect(x + 1.0, target_y, width - 2.0, 1.0, [0.3, 0.3, 0.3, 0.8]);

        // Draw frame time bars
        let bar_width = (width - 2.0) / frame_times.len() as f32;
        for (i, &frame_time) in frame_times.iter().enumerate() {
            let bar_height = (frame_time / max_frame_time * height).min(height - 2.0);
            let bar_x = x + 1.0 + i as f32 * bar_width;
            let bar_y = y + height - 1.0 - bar_height;

            // Color based on performance
            let color = if frame_time <= target_frame_time {
                success
            } else if frame_time <= target_frame_time * 2.0 {
                warning
            } else {
                error
            };

            ctx.draw_rect(bar_x, bar_y, bar_width.max(1.0), bar_height, color);
        }
    }
}

/// Toast notification widget
pub struct Toast;

impl Toast {
    /// Draw a toast notification
    pub fn draw(
        ctx: &mut UiContext,
        screen_width: f32,
        y: f32,
        message: &str,
        toast_type: ToastType,
    ) {
        // Extract theme values upfront
        let padding = ctx.theme.padding;
        let scale = ctx.theme.text_scale;
        let line_height = ctx.theme.line_height_px();
        let text_color = ctx.theme.colors.text;
        let info_color = ctx.theme.colors.info;
        let success_color = ctx.theme.colors.success;
        let warning_color = ctx.theme.colors.warning;
        let error_color = ctx.theme.colors.error;

        let text_width = ctx.measure_text(message, scale);
        let toast_width = text_width + padding * 2.0;
        let toast_height = line_height + padding * 2.0;
        let x = (screen_width - toast_width) / 2.0;

        let bg_color = match toast_type {
            ToastType::Info => [0.1, 0.3, 0.5, 0.95],
            ToastType::Success => [0.1, 0.4, 0.1, 0.95],
            ToastType::Warning => [0.5, 0.4, 0.1, 0.95],
            ToastType::Error => [0.5, 0.1, 0.1, 0.95],
        };

        let border_color = match toast_type {
            ToastType::Info => info_color,
            ToastType::Success => success_color,
            ToastType::Warning => warning_color,
            ToastType::Error => error_color,
        };

        // Background
        ctx.draw_rect(x, y, toast_width, toast_height, bg_color);

        // Border
        ctx.draw_rect(x, y, toast_width, 2.0, border_color);

        // Text
        ctx.draw_text(message, x + padding, y + padding, text_color, scale);
    }
}

/// Toast notification type
#[derive(Clone, Copy, Debug)]
pub enum ToastType {
    Info,
    Success,
    Warning,
    Error,
}

/// Help modal widget for displaying controls
pub struct HelpModal;

impl HelpModal {
    /// Draw a centered help modal with controls
    ///
    /// `controls` is a slice of (key, description) pairs
    pub fn draw(
        ctx: &mut UiContext,
        screen_width: f32,
        screen_height: f32,
        title: &str,
        controls: &[(&str, &str)],
        footer: Option<&str>,
    ) {
        // Extract theme values upfront
        let padding = ctx.theme.padding * 1.5;
        let line_height = ctx.theme.line_height_px();
        let scale = ctx.theme.text_scale;
        let border_width = ctx.theme.border_width;
        let panel_bg = [0.08, 0.08, 0.1, 0.95];
        let panel_border = ctx.theme.colors.panel_border;
        let accent = ctx.theme.colors.accent;
        let text_color = ctx.theme.colors.text;
        let text_dim = ctx.theme.colors.text_dim;
        let info_color = ctx.theme.colors.info;

        // Calculate modal dimensions
        let key_col_width = 120.0;
        let mut max_desc_width: f32 = 0.0;
        for (_, desc) in controls {
            max_desc_width = max_desc_width.max(ctx.measure_text(desc, scale));
        }
        let title_width = ctx.measure_text(title, scale);

        let content_width = (key_col_width + max_desc_width).max(title_width);
        let modal_width = content_width + padding * 2.0;

        let num_lines = controls.len() + 1 + if footer.is_some() { 2 } else { 0 };
        let modal_height = line_height * num_lines as f32 + padding * 2.0 + 8.0;

        // Center the modal
        let x = (screen_width - modal_width) / 2.0;
        let y = (screen_height - modal_height) / 2.0;

        // Draw semi-transparent overlay
        ctx.draw_rect(0.0, 0.0, screen_width, screen_height, [0.0, 0.0, 0.0, 0.6]);

        // Draw modal background
        ctx.draw_rect(x, y, modal_width, modal_height, panel_bg);

        // Draw border
        ctx.draw_rect(x, y, modal_width, border_width * 2.0, accent);
        ctx.draw_rect(x, y + modal_height - border_width, modal_width, border_width, panel_border);
        ctx.draw_rect(x, y, border_width, modal_height, panel_border);
        ctx.draw_rect(x + modal_width - border_width, y, border_width, modal_height, panel_border);

        // Draw title
        let text_x = x + padding;
        let mut text_y = y + padding;
        let title_x = x + (modal_width - ctx.measure_text(title, scale)) / 2.0;
        ctx.draw_text(title, title_x, text_y, accent, scale);
        text_y += line_height + 8.0;

        // Draw controls
        for (key, desc) in controls {
            // Key in accent color, right-aligned in key column
            let key_width = ctx.measure_text(key, scale);
            let key_x = text_x + key_col_width - key_width - 10.0;
            ctx.draw_text(key, key_x, text_y, info_color, scale);

            // Description in normal color
            ctx.draw_text(desc, text_x + key_col_width, text_y, text_color, scale);
            text_y += line_height;
        }

        // Draw footer if provided
        if let Some(footer_text) = footer {
            text_y += line_height * 0.5;
            let footer_x = x + (modal_width - ctx.measure_text(footer_text, scale)) / 2.0;
            ctx.draw_text(footer_text, footer_x, text_y, text_dim, scale);
        }
    }
}
