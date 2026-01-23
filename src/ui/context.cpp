/// @file context.cpp
/// @brief UI Context implementation

#include <void_engine/ui/context.hpp>

#include <algorithm>
#include <cstring>
#include <functional>

namespace void_ui {

// =============================================================================
// Hash Helper
// =============================================================================

static std::uint64_t hash_string(const std::string& str) {
    // FNV-1a hash
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static std::uint64_t combine_ids(std::uint64_t a, std::uint64_t b) {
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

// =============================================================================
// UiContext Implementation
// =============================================================================

UiContext::UiContext()
    : m_theme(Theme::dark())
    , m_font(BitmapFont::create_builtin()) {
    m_mouse_down.fill(false);
    m_mouse_down_prev.fill(false);
    m_key_down.fill(false);
    m_key_down_prev.fill(false);
}

UiContext::~UiContext() = default;

UiContext::UiContext(UiContext&&) noexcept = default;
UiContext& UiContext::operator=(UiContext&&) noexcept = default;

// =============================================================================
// Frame Management
// =============================================================================

void UiContext::begin_frame() {
    m_draw_data.clear();

    // Reset cursor to origin
    m_cursor_x = 0.0f;
    m_cursor_y = 0.0f;

    // Clear stacks
    while (!m_cursor_stack.empty()) m_cursor_stack.pop();
    while (!m_clip_stack.empty()) m_clip_stack.pop();
    while (!m_id_stack.empty()) m_id_stack.pop();

    // Push root clip rect (full screen)
    m_clip_stack.push(Rect{0.0f, 0.0f, m_screen_width, m_screen_height});

    // Push root ID
    m_id_stack.push(0);
}

void UiContext::end_frame() {
    // Store previous input state for pressed/released detection
    m_mouse_down_prev = m_mouse_down;
    m_key_down_prev = m_key_down;

    // Clear text input for next frame
    m_text_input.clear();
}

// =============================================================================
// Screen/Viewport
// =============================================================================

void UiContext::set_screen_size(float width, float height) {
    m_screen_width = width;
    m_screen_height = height;
}

// =============================================================================
// Theme & Font
// =============================================================================

void UiContext::set_font(BitmapFont font) {
    m_font = std::move(font);
}

// =============================================================================
// Cursor Management
// =============================================================================

void UiContext::set_cursor(float x, float y) {
    m_cursor_x = x;
    m_cursor_y = y;
}

void UiContext::advance_cursor(float dx, float dy) {
    m_cursor_x += dx;
    m_cursor_y += dy;
}

void UiContext::newline() {
    m_cursor_x = 0.0f;
    m_cursor_y += line_height();
}

void UiContext::newline(float height) {
    m_cursor_x = 0.0f;
    m_cursor_y += height;
}

void UiContext::push_cursor() {
    m_cursor_stack.push(Point{m_cursor_x, m_cursor_y});
}

void UiContext::pop_cursor() {
    if (!m_cursor_stack.empty()) {
        auto pos = m_cursor_stack.top();
        m_cursor_stack.pop();
        m_cursor_x = pos.x;
        m_cursor_y = pos.y;
    }
}

// =============================================================================
// Clipping
// =============================================================================

void UiContext::push_clip_rect(const Rect& rect) {
    if (m_clip_stack.empty()) {
        m_clip_stack.push(rect);
    } else {
        // Intersect with current clip rect
        const auto& current = m_clip_stack.top();
        float x1 = std::max(rect.x, current.x);
        float y1 = std::max(rect.y, current.y);
        float x2 = std::min(rect.x + rect.width, current.x + current.width);
        float y2 = std::min(rect.y + rect.height, current.y + current.height);

        Rect clipped{
            x1, y1,
            std::max(0.0f, x2 - x1),
            std::max(0.0f, y2 - y1)
        };
        m_clip_stack.push(clipped);
    }
}

void UiContext::pop_clip_rect() {
    if (m_clip_stack.size() > 1) {
        m_clip_stack.pop();
    }
}

Rect UiContext::current_clip_rect() const {
    if (m_clip_stack.empty()) {
        return Rect{0.0f, 0.0f, m_screen_width, m_screen_height};
    }
    return m_clip_stack.top();
}

// =============================================================================
// Internal Helpers
// =============================================================================

void UiContext::add_vertex(float x, float y, float u, float v, const Color& color) {
    UiVertex vertex;
    vertex.position[0] = x;
    vertex.position[1] = y;
    vertex.uv[0] = u;
    vertex.uv[1] = v;
    vertex.color[0] = color.r;
    vertex.color[1] = color.g;
    vertex.color[2] = color.b;
    vertex.color[3] = color.a;
    m_draw_data.vertices.push_back(vertex);
}

void UiContext::add_quad_indices() {
    auto base = static_cast<std::uint16_t>(m_draw_data.vertices.size() - 4);
    m_draw_data.indices.push_back(base + 0);
    m_draw_data.indices.push_back(base + 1);
    m_draw_data.indices.push_back(base + 2);
    m_draw_data.indices.push_back(base + 2);
    m_draw_data.indices.push_back(base + 3);
    m_draw_data.indices.push_back(base + 0);
}

bool UiContext::is_clipped(float x, float y, float width, float height) const {
    if (m_clip_stack.empty()) return false;

    const auto& clip = m_clip_stack.top();
    return (x + width < clip.x ||
            y + height < clip.y ||
            x > clip.x + clip.width ||
            y > clip.y + clip.height);
}

// =============================================================================
// Basic Drawing
// =============================================================================

void UiContext::draw_rect(float x, float y, float width, float height, const Color& color) {
    if (is_clipped(x, y, width, height)) return;
    if (color.a <= 0.0f) return;

    // Add 4 vertices for quad
    add_vertex(x, y, 0.0f, 0.0f, color);
    add_vertex(x + width, y, 1.0f, 0.0f, color);
    add_vertex(x + width, y + height, 1.0f, 1.0f, color);
    add_vertex(x, y + height, 0.0f, 1.0f, color);

    add_quad_indices();
}

void UiContext::draw_rect_border(float x, float y, float width, float height,
                                  const Color& color, float border_width) {
    if (is_clipped(x, y, width, height)) return;
    if (color.a <= 0.0f) return;

    // Draw 4 rectangles for border
    // Top
    draw_rect(x, y, width, border_width, color);
    // Bottom
    draw_rect(x, y + height - border_width, width, border_width, color);
    // Left
    draw_rect(x, y + border_width, border_width, height - 2 * border_width, color);
    // Right
    draw_rect(x + width - border_width, y + border_width, border_width, height - 2 * border_width, color);
}

void UiContext::draw_rect_filled_border(const Rect& rect, const Color& fill_color,
                                         const Color& border_color, float border_width) {
    draw_rect(rect, fill_color);
    draw_rect_border(rect, border_color, border_width);
}

void UiContext::draw_line(Point from, Point to, const Color& color, float thickness) {
    if (color.a <= 0.0f) return;

    // Calculate perpendicular direction
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = std::sqrt(dx * dx + dy * dy);

    if (len < 0.0001f) return;

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    // Add 4 vertices for line quad
    add_vertex(from.x + nx, from.y + ny, 0.0f, 0.0f, color);
    add_vertex(to.x + nx, to.y + ny, 1.0f, 0.0f, color);
    add_vertex(to.x - nx, to.y - ny, 1.0f, 1.0f, color);
    add_vertex(from.x - nx, from.y - ny, 0.0f, 1.0f, color);

    add_quad_indices();
}

// =============================================================================
// Text Drawing
// =============================================================================

void UiContext::draw_glyph(const std::array<std::uint8_t, 16>& glyph,
                           float x, float y, float width, float height, const Color& color) {
    // Calculate pixel size
    float pixel_w = width / 8.0f;
    float pixel_h = height / 16.0f;

    // Draw each pixel as a small rectangle
    for (int row = 0; row < 16; ++row) {
        std::uint8_t row_data = glyph[static_cast<std::size_t>(row)];
        if (row_data == 0) continue;

        for (int col = 0; col < 8; ++col) {
            if (row_data & (1 << (7 - col))) {
                float px = x + col * pixel_w;
                float py = y + row * pixel_h;
                draw_rect(px, py, pixel_w, pixel_h, color);
            }
        }
    }
}

void UiContext::draw_text(const std::string& text, float x, float y,
                          const Color& color, float scale) {
    if (text.empty()) return;
    if (color.a <= 0.0f) return;

    float glyph_w = static_cast<float>(BitmapFont::GLYPH_WIDTH) * scale;
    float glyph_h = static_cast<float>(BitmapFont::GLYPH_HEIGHT) * scale;
    float cursor_x = x;
    float cursor_y = y;

    for (char ch : text) {
        if (ch == '\n') {
            cursor_x = x;
            cursor_y += glyph_h * m_theme.line_height;
            continue;
        }

        if (ch == '\t') {
            cursor_x += glyph_w * 4.0f;
            continue;
        }

        if (ch == ' ') {
            cursor_x += glyph_w;
            continue;
        }

        // Get glyph data
        const auto& glyph_data = BitmapFont::get_builtin_glyph(ch);

        // Check if visible before drawing
        if (!is_clipped(cursor_x, cursor_y, glyph_w, glyph_h)) {
            draw_glyph(glyph_data, cursor_x, cursor_y, glyph_w, glyph_h, color);
        }

        cursor_x += glyph_w;
    }
}

void UiContext::draw_text_aligned(const std::string& text, const Rect& rect,
                                  Alignment h_align, const Color& color, float scale) {
    float text_w = measure_text(text, scale);
    float text_h = text_height(scale);

    float x = rect.x;
    float y = rect.y + (rect.height - text_h) / 2.0f;

    switch (h_align) {
        case Alignment::Left:
            x = rect.x;
            break;
        case Alignment::Center:
            x = rect.x + (rect.width - text_w) / 2.0f;
            break;
        case Alignment::Right:
            x = rect.x + rect.width - text_w;
            break;
    }

    draw_text(text, x, y, color, scale);
}

// =============================================================================
// Text Measurement
// =============================================================================

float UiContext::measure_text(const std::string& text, float scale) const {
    return m_font.measure_text(text, scale * m_theme.text_scale);
}

float UiContext::text_height(float scale) const {
    return m_font.text_height(scale * m_theme.text_scale);
}

float UiContext::line_height() const {
    return m_font.line_height(m_theme.text_scale, m_theme.line_height);
}

// =============================================================================
// Input State
// =============================================================================

void UiContext::set_mouse_position(float x, float y) {
    m_mouse_pos = Point{x, y};
}

void UiContext::set_mouse_button(std::uint32_t button, bool pressed) {
    if (button < m_mouse_down.size()) {
        m_mouse_down[button] = pressed;
    }
}

bool UiContext::is_mouse_down(std::uint32_t button) const {
    if (button < m_mouse_down.size()) {
        return m_mouse_down[button];
    }
    return false;
}

bool UiContext::is_mouse_pressed(std::uint32_t button) const {
    if (button < m_mouse_down.size()) {
        return m_mouse_down[button] && !m_mouse_down_prev[button];
    }
    return false;
}

bool UiContext::is_mouse_released(std::uint32_t button) const {
    if (button < m_mouse_down.size()) {
        return !m_mouse_down[button] && m_mouse_down_prev[button];
    }
    return false;
}

bool UiContext::is_hovered(const Rect& rect) const {
    return rect.contains(m_mouse_pos);
}

bool UiContext::is_clicked(const Rect& rect, std::uint32_t button) const {
    return is_hovered(rect) && is_mouse_pressed(button);
}

// =============================================================================
// Keyboard Input
// =============================================================================

void UiContext::set_key(Key key, bool pressed) {
    auto idx = static_cast<std::size_t>(key);
    if (idx < m_key_down.size()) {
        m_key_down[idx] = pressed;
    }
}

void UiContext::set_modifiers(std::uint32_t mods) {
    m_modifiers = mods;
}

bool UiContext::is_key_down(Key key) const {
    auto idx = static_cast<std::size_t>(key);
    if (idx < m_key_down.size()) {
        return m_key_down[idx];
    }
    return false;
}

bool UiContext::is_key_pressed(Key key) const {
    auto idx = static_cast<std::size_t>(key);
    if (idx < m_key_down.size()) {
        return m_key_down[idx] && !m_key_down_prev[idx];
    }
    return false;
}

bool UiContext::is_key_released(Key key) const {
    auto idx = static_cast<std::size_t>(key);
    if (idx < m_key_down.size()) {
        return !m_key_down[idx] && m_key_down_prev[idx];
    }
    return false;
}

void UiContext::add_text_input(std::uint32_t codepoint) {
    // Convert UTF-32 codepoint to UTF-8
    if (codepoint < 0x80) {
        m_text_input += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        m_text_input += static_cast<char>(0xC0 | (codepoint >> 6));
        m_text_input += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        m_text_input += static_cast<char>(0xE0 | (codepoint >> 12));
        m_text_input += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        m_text_input += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x110000) {
        m_text_input += static_cast<char>(0xF0 | (codepoint >> 18));
        m_text_input += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        m_text_input += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        m_text_input += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

void UiContext::add_text_input(const std::string& text) {
    m_text_input += text;
}

void UiContext::clear_text_input() {
    m_text_input.clear();
}

// =============================================================================
// Widget ID Management
// =============================================================================

void UiContext::push_id(std::uint64_t id) {
    if (m_id_stack.empty()) {
        m_id_stack.push(id);
    } else {
        m_id_stack.push(combine_ids(m_id_stack.top(), id));
    }
}

void UiContext::push_id(const std::string& str_id) {
    push_id(hash_string(str_id));
}

void UiContext::pop_id() {
    if (m_id_stack.size() > 1) {
        m_id_stack.pop();
    }
}

std::uint64_t UiContext::current_id() const {
    if (m_id_stack.empty()) {
        return 0;
    }
    return m_id_stack.top();
}

// =============================================================================
// Focus Management
// =============================================================================

void UiContext::set_focus(std::uint64_t widget_id) {
    m_focused_widget = widget_id;
}

void UiContext::clear_focus() {
    m_focused_widget = 0;
}

bool UiContext::is_focused(std::uint64_t widget_id) const {
    return m_focused_widget == widget_id && widget_id != 0;
}

} // namespace void_ui
