//! void_ui - Immediate-mode UI toolkit for Void Engine
//!
//! Provides a simple, themeable UI system with:
//! - Bitmap font text rendering
//! - Theming support
//! - Basic widgets (panels, labels, buttons)
//! - Debug overlay support

mod font;
mod theme;
mod renderer;
mod widgets;

pub use font::BitmapFont;
pub use theme::{Theme, ThemeColors};
pub use renderer::{UiRenderer, UiVertex, UiDrawData, UiUniforms};
pub use widgets::*;

/// UI Context for building and rendering UI
pub struct UiContext {
    /// Current theme
    pub theme: Theme,
    /// Accumulated vertices for this frame
    vertices: Vec<UiVertex>,
    /// Accumulated indices for this frame
    indices: Vec<u16>,
    /// Current cursor position
    cursor_x: f32,
    cursor_y: f32,
    /// Screen dimensions
    screen_width: f32,
    screen_height: f32,
}

impl UiContext {
    /// Create a new UI context with default theme
    pub fn new() -> Self {
        Self {
            theme: Theme::dark(),
            vertices: Vec::new(),
            indices: Vec::new(),
            cursor_x: 0.0,
            cursor_y: 0.0,
            screen_width: 1280.0,
            screen_height: 720.0,
        }
    }

    /// Set screen dimensions
    pub fn set_screen_size(&mut self, width: f32, height: f32) {
        self.screen_width = width;
        self.screen_height = height;
    }

    /// Begin a new frame
    pub fn begin_frame(&mut self) {
        self.vertices.clear();
        self.indices.clear();
        self.cursor_x = 0.0;
        self.cursor_y = 0.0;
    }

    /// Get the accumulated draw data
    pub fn get_draw_data(&self) -> (&[UiVertex], &[u16]) {
        (&self.vertices, &self.indices)
    }

    /// Set cursor position
    pub fn set_cursor(&mut self, x: f32, y: f32) {
        self.cursor_x = x;
        self.cursor_y = y;
    }

    /// Get current cursor position
    pub fn cursor(&self) -> (f32, f32) {
        (self.cursor_x, self.cursor_y)
    }

    /// Advance cursor by amount
    pub fn advance_cursor(&mut self, dx: f32, dy: f32) {
        self.cursor_x += dx;
        self.cursor_y += dy;
    }

    /// Newline - move cursor down and reset x
    pub fn newline(&mut self, line_height: f32) {
        self.cursor_y += line_height;
    }

    /// Draw a filled rectangle
    pub fn draw_rect(&mut self, x: f32, y: f32, width: f32, height: f32, color: [f32; 4]) {
        let base = self.vertices.len() as u16;

        self.vertices.push(UiVertex { position: [x, y], uv: [0.0, 0.0], color });
        self.vertices.push(UiVertex { position: [x + width, y], uv: [1.0, 0.0], color });
        self.vertices.push(UiVertex { position: [x + width, y + height], uv: [1.0, 1.0], color });
        self.vertices.push(UiVertex { position: [x, y + height], uv: [0.0, 1.0], color });

        self.indices.extend_from_slice(&[
            base, base + 1, base + 2,
            base, base + 2, base + 3,
        ]);
    }

    /// Draw text at position
    pub fn draw_text(&mut self, text: &str, x: f32, y: f32, color: [f32; 4], scale: f32) {
        let mut cursor_x = x;
        let glyph_width = 8.0 * scale;
        let glyph_height = 16.0 * scale;

        for ch in text.chars() {
            if ch == ' ' {
                cursor_x += glyph_width;
                continue;
            }
            if ch == '\n' {
                // Newline not handled here - caller should split lines
                continue;
            }

            let glyph = BitmapFont::get_glyph(ch);
            self.draw_glyph(glyph, cursor_x, y, glyph_width, glyph_height, color);
            cursor_x += glyph_width;
        }
    }

    /// Draw a single glyph from bitmap data
    fn draw_glyph(&mut self, glyph: &[u8; 16], x: f32, y: f32, width: f32, height: f32, color: [f32; 4]) {
        let pixel_width = width / 8.0;
        let pixel_height = height / 16.0;

        for row in 0..16 {
            let row_data = glyph[row];
            for col in 0..8 {
                if (row_data >> (7 - col)) & 1 == 1 {
                    let px = x + col as f32 * pixel_width;
                    let py = y + row as f32 * pixel_height;
                    self.draw_rect(px, py, pixel_width, pixel_height, color);
                }
            }
        }
    }

    /// Measure text width
    pub fn measure_text(&self, text: &str, scale: f32) -> f32 {
        let glyph_width = 8.0 * scale;
        text.chars().count() as f32 * glyph_width
    }

    /// Measure text height (single line)
    pub fn text_height(&self, scale: f32) -> f32 {
        16.0 * scale
    }
}

impl Default for UiContext {
    fn default() -> Self {
        Self::new()
    }
}
