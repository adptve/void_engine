#pragma once

/// @file context.hpp
/// @brief UI Context for building and rendering UI
///
/// Provides immediate-mode style UI building with:
/// - Vertex/index buffer management
/// - Text rendering with bitmap fonts
/// - Theme-aware drawing
/// - Cursor management
/// - Clipping/scissor support

#include "fwd.hpp"
#include "types.hpp"
#include "theme.hpp"
#include "font.hpp"

#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace void_ui {

// =============================================================================
// UI Context
// =============================================================================

/// UI Context for building and rendering UI
class UiContext {
public:
    UiContext();
    ~UiContext();

    // Non-copyable but movable
    UiContext(const UiContext&) = delete;
    UiContext& operator=(const UiContext&) = delete;
    UiContext(UiContext&&) noexcept;
    UiContext& operator=(UiContext&&) noexcept;

    // =========================================================================
    // Frame Management
    // =========================================================================

    /// Begin a new frame
    void begin_frame();

    /// End the current frame
    void end_frame();

    /// Get the accumulated draw data
    [[nodiscard]] const UiDrawData& draw_data() const { return m_draw_data; }

    /// Get mutable draw data (for external rendering)
    [[nodiscard]] UiDrawData& draw_data() { return m_draw_data; }

    // =========================================================================
    // Screen/Viewport
    // =========================================================================

    /// Set screen dimensions
    void set_screen_size(float width, float height);

    /// Get screen width
    [[nodiscard]] float screen_width() const { return m_screen_width; }

    /// Get screen height
    [[nodiscard]] float screen_height() const { return m_screen_height; }

    /// Get screen size
    [[nodiscard]] Size screen_size() const { return {m_screen_width, m_screen_height}; }

    // =========================================================================
    // Theme & Font
    // =========================================================================

    /// Get current theme
    [[nodiscard]] const Theme& theme() const { return m_theme; }

    /// Get mutable theme reference
    [[nodiscard]] Theme& theme() { return m_theme; }

    /// Set theme
    void set_theme(const Theme& theme) { m_theme = theme; }

    /// Get current font
    [[nodiscard]] const BitmapFont& font() const { return m_font; }

    /// Set font
    void set_font(BitmapFont font);

    // =========================================================================
    // Cursor Management
    // =========================================================================

    /// Set cursor position
    void set_cursor(float x, float y);

    /// Set cursor position
    void set_cursor(Point pos) { set_cursor(pos.x, pos.y); }

    /// Get current cursor position
    [[nodiscard]] Point cursor() const { return {m_cursor_x, m_cursor_y}; }

    /// Get cursor X
    [[nodiscard]] float cursor_x() const { return m_cursor_x; }

    /// Get cursor Y
    [[nodiscard]] float cursor_y() const { return m_cursor_y; }

    /// Advance cursor by amount
    void advance_cursor(float dx, float dy);

    /// Move cursor to next line
    void newline();

    /// Move cursor to next line with custom height
    void newline(float line_height);

    /// Save cursor position
    void push_cursor();

    /// Restore cursor position
    void pop_cursor();

    // =========================================================================
    // Clipping
    // =========================================================================

    /// Push a clip rectangle
    void push_clip_rect(const Rect& rect);

    /// Pop the clip rectangle
    void pop_clip_rect();

    /// Get current clip rectangle
    [[nodiscard]] Rect current_clip_rect() const;

    // =========================================================================
    // Basic Drawing
    // =========================================================================

    /// Draw a filled rectangle
    void draw_rect(float x, float y, float width, float height, const Color& color);

    /// Draw a filled rectangle
    void draw_rect(const Rect& rect, const Color& color) {
        draw_rect(rect.x, rect.y, rect.width, rect.height, color);
    }

    /// Draw a rectangle border
    void draw_rect_border(float x, float y, float width, float height,
                          const Color& color, float border_width = 1.0f);

    /// Draw a rectangle border
    void draw_rect_border(const Rect& rect, const Color& color, float border_width = 1.0f) {
        draw_rect_border(rect.x, rect.y, rect.width, rect.height, color, border_width);
    }

    /// Draw a filled rectangle with border
    void draw_rect_filled_border(const Rect& rect, const Color& fill_color,
                                  const Color& border_color, float border_width = 1.0f);

    /// Draw a line
    void draw_line(Point from, Point to, const Color& color, float thickness = 1.0f);

    // =========================================================================
    // Text Drawing
    // =========================================================================

    /// Draw text at position
    void draw_text(const std::string& text, float x, float y,
                   const Color& color, float scale = 1.0f);

    /// Draw text at position
    void draw_text(const std::string& text, Point pos,
                   const Color& color, float scale = 1.0f) {
        draw_text(text, pos.x, pos.y, color, scale);
    }

    /// Draw text with theme default color and scale
    void draw_text(const std::string& text, float x, float y) {
        draw_text(text, x, y, m_theme.colors.text, m_theme.text_scale);
    }

    /// Draw text aligned within a rectangle
    void draw_text_aligned(const std::string& text, const Rect& rect,
                           Alignment h_align, const Color& color, float scale = 1.0f);

    /// Draw a single glyph
    void draw_glyph(const std::array<std::uint8_t, 16>& glyph,
                    float x, float y, float width, float height, const Color& color);

    // =========================================================================
    // Text Measurement
    // =========================================================================

    /// Measure text width
    [[nodiscard]] float measure_text(const std::string& text, float scale = 1.0f) const;

    /// Get text height (single line)
    [[nodiscard]] float text_height(float scale = 1.0f) const;

    /// Get line height (includes spacing)
    [[nodiscard]] float line_height() const;

    // =========================================================================
    // Input State (for interactive widgets)
    // =========================================================================

    /// Set mouse position
    void set_mouse_position(float x, float y);

    /// Get mouse position
    [[nodiscard]] Point mouse_position() const { return m_mouse_pos; }

    /// Set mouse button state
    void set_mouse_button(std::uint32_t button, bool pressed);

    /// Check if mouse button is down
    [[nodiscard]] bool is_mouse_down(std::uint32_t button = 0) const;

    /// Check if mouse button was just pressed
    [[nodiscard]] bool is_mouse_pressed(std::uint32_t button = 0) const;

    /// Check if mouse button was just released
    [[nodiscard]] bool is_mouse_released(std::uint32_t button = 0) const;

    /// Check if point is hovered
    [[nodiscard]] bool is_hovered(const Rect& rect) const;

    /// Check if point is clicked
    [[nodiscard]] bool is_clicked(const Rect& rect, std::uint32_t button = 0) const;

    // =========================================================================
    // Widget ID Management
    // =========================================================================

    /// Push widget ID onto stack
    void push_id(std::uint64_t id);

    /// Push widget ID from string
    void push_id(const std::string& str_id);

    /// Pop widget ID
    void pop_id();

    /// Get current combined widget ID
    [[nodiscard]] std::uint64_t current_id() const;

    // =========================================================================
    // Focus Management
    // =========================================================================

    /// Set focused widget
    void set_focus(std::uint64_t widget_id);

    /// Clear focus
    void clear_focus();

    /// Check if widget is focused
    [[nodiscard]] bool is_focused(std::uint64_t widget_id) const;

    /// Get focused widget ID
    [[nodiscard]] std::uint64_t focused_widget() const { return m_focused_widget; }

private:
    // Internal helpers
    void add_vertex(float x, float y, float u, float v, const Color& color);
    void add_quad_indices();
    bool is_clipped(float x, float y, float width, float height) const;

private:
    // Draw data
    UiDrawData m_draw_data;

    // Screen
    float m_screen_width = 1280.0f;
    float m_screen_height = 720.0f;

    // Theme & Font
    Theme m_theme;
    BitmapFont m_font;

    // Cursor
    float m_cursor_x = 0.0f;
    float m_cursor_y = 0.0f;
    std::stack<Point> m_cursor_stack;

    // Clipping
    std::stack<Rect> m_clip_stack;

    // Input
    Point m_mouse_pos{0.0f, 0.0f};
    std::array<bool, 8> m_mouse_down{};
    std::array<bool, 8> m_mouse_down_prev{};

    // Widget IDs
    std::stack<std::uint64_t> m_id_stack;
    std::uint64_t m_focused_widget = 0;
};

} // namespace void_ui
