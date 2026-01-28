/// @file core_widget.cpp
/// @brief Implementation of the core widget plugin

#include "core_widget.hpp"

#include <algorithm>
#include <cmath>

namespace core_widgets {

using namespace void_widget;

// =============================================================================
// Construction
// =============================================================================

CoreWidget::CoreWidget() = default;

// =============================================================================
// Lifecycle
// =============================================================================

void_core::Result<void> CoreWidget::on_widget_load() {
    // Core widgets don't need special initialization
    return void_core::Ok();
}

void CoreWidget::on_widget_unload() {
    // Cleanup
}

void CoreWidget::update(float dt) {
    // Update cursor blink timer
    m_cursor_blink_timer += dt;
    if (m_cursor_blink_timer >= CURSOR_BLINK_RATE) {
        m_cursor_blink_timer = 0;
        m_cursor_visible = !m_cursor_visible;
    }
}

// =============================================================================
// Rendering
// =============================================================================

void CoreWidget::render_widget(WidgetId id, const WidgetInstance& widget) {
    if (widget.type == "panel") {
        render_panel(id, widget);
    } else if (widget.type == "text") {
        render_text(id, widget);
    } else if (widget.type == "button") {
        render_button(id, widget);
    } else if (widget.type == "checkbox") {
        render_checkbox(id, widget);
    } else if (widget.type == "slider") {
        render_slider(id, widget);
    } else if (widget.type == "progress_bar") {
        render_progress_bar(id, widget);
    } else if (widget.type == "text_input") {
        render_text_input(id, widget);
    } else if (widget.type == "image") {
        render_image(id, widget);
    } else if (widget.type == "separator") {
        render_separator(id, widget);
    } else if (widget.type == "spacer") {
        render_spacer(id, widget);
    }
}

Vec2 CoreWidget::measure_widget(WidgetId id, const WidgetInstance& widget,
                                 Vec2 available_size) {
    auto* widget_api = api();
    if (!widget_api) return available_size;

    auto style = widget_api->get_computed_style(id);

    if (widget.type == "text") {
        auto text = widget.get_property<std::string>("text", "");
        // Approximate text measurement (8 pixels per character at size 14)
        float char_width = style.font_size * 0.6f;
        float width = text.length() * char_width;
        float height = style.font_size * 1.4f;
        return {width, height};
    }

    if (widget.type == "button") {
        auto text = widget.get_property<std::string>("text", "Button");
        float char_width = style.font_size * 0.6f;
        float width = text.length() * char_width + 32;  // Add padding
        float height = style.font_size * 1.4f + 16;     // Add padding
        return {std::max(width, 80.0f), std::max(height, 32.0f)};
    }

    if (widget.type == "checkbox") {
        auto text = widget.get_property<std::string>("text", "");
        float char_width = style.font_size * 0.6f;
        float box_size = style.font_size + 4;
        float width = box_size + 8 + text.length() * char_width;
        return {width, box_size};
    }

    if (widget.type == "separator") {
        return {available_size.x, 1.0f};
    }

    return available_size;
}

// =============================================================================
// Individual Widget Renderers
// =============================================================================

void CoreWidget::render_panel(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    float radius = widget.get_property<float>("border_radius", style.border_radius);

    // Draw background
    if (style.background_color.a > 0) {
        if (radius > 0) {
            widget_api->draw_rounded_rect(bounds, style.background_color, radius);
        } else {
            widget_api->draw_rect(bounds, style.background_color);
        }
    }

    // Draw border
    if (style.border_width > 0 && style.border_color.a > 0) {
        if (radius > 0) {
            widget_api->draw_rounded_rect_outline(bounds, style.border_color, radius, style.border_width);
        } else {
            widget_api->draw_rect_outline(bounds, style.border_color, style.border_width);
        }
    }

    // Draw title if present
    auto title = widget.get_property<std::string>("title", "");
    if (!title.empty()) {
        float title_height = style.font_size * 1.4f + 8;
        Rect title_bounds{bounds.x, bounds.y, bounds.width, title_height};

        // Title background
        auto title_color = style.background_color;
        title_color.r *= 0.8f;
        title_color.g *= 0.8f;
        title_color.b *= 0.8f;
        widget_api->draw_rect(title_bounds, title_color);

        // Title text
        widget_api->draw_text_aligned(title, title_bounds, TextAlign::Center, VerticalAlign::Middle,
                                       style.text_color, style.font_size);

        // Title separator
        widget_api->draw_line({bounds.x, bounds.y + title_height},
                               {bounds.x + bounds.width, bounds.y + title_height},
                               style.border_color, 1);
    }
}

void CoreWidget::render_text(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    auto text = widget.get_property<std::string>("text", "");
    auto h_align = widget.get_property<TextAlign>("align", TextAlign::Left);
    auto v_align = widget.get_property<VerticalAlign>("valign", VerticalAlign::Top);

    widget_api->draw_text_aligned(text, bounds, h_align, v_align,
                                   style.text_color, style.font_size);
}

void CoreWidget::render_button(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    bool disabled = has_state(widget.state, WidgetState::Disabled);
    bool pressed = widget_api->is_pressed(id);
    bool hovered = widget_api->is_hovered(id);

    // Determine button color based on state
    Color bg_color = style.background_color;
    if (disabled) {
        bg_color = bg_color.with_alpha(0.5f);
    } else if (pressed) {
        bg_color = Color::lerp(bg_color, Color::black(), 0.2f);
    } else if (hovered) {
        bg_color = Color::lerp(bg_color, Color::white(), 0.1f);
    }

    float radius = widget.get_property<float>("border_radius", style.border_radius);

    // Draw button background
    if (radius > 0) {
        widget_api->draw_rounded_rect(bounds, bg_color, radius);
    } else {
        widget_api->draw_rect(bounds, bg_color);
    }

    // Draw border
    if (style.border_width > 0) {
        Color border_color = style.border_color;
        if (hovered && !disabled) {
            border_color = Color::lerp(border_color, Color::white(), 0.2f);
        }
        if (radius > 0) {
            widget_api->draw_rounded_rect_outline(bounds, border_color, radius, style.border_width);
        } else {
            widget_api->draw_rect_outline(bounds, border_color, style.border_width);
        }
    }

    // Draw button text
    auto text = widget.get_property<std::string>("text", "Button");
    Color text_color = style.text_color;
    if (disabled) {
        text_color = text_color.with_alpha(0.5f);
    }
    widget_api->draw_text_aligned(text, bounds, TextAlign::Center, VerticalAlign::Middle,
                                   text_color, style.font_size);

    // Draw icon if present
    auto icon = widget.get_property<std::string>("icon", "");
    if (!icon.empty()) {
        float icon_size = style.font_size + 4;
        Vec2 icon_pos{bounds.x + 8, bounds.center().y - icon_size / 2};
        widget_api->draw_icon(icon, icon_pos, {icon_size, icon_size}, text_color);
    }
}

void CoreWidget::render_checkbox(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    bool checked = widget_api->is_checked(id);
    bool hovered = widget_api->is_hovered(id);
    bool disabled = has_state(widget.state, WidgetState::Disabled);

    float box_size = style.font_size + 4;
    Rect box_bounds{bounds.x, bounds.center().y - box_size / 2, box_size, box_size};

    // Draw checkbox box
    Color box_color = style.background_color;
    if (hovered && !disabled) {
        box_color = Color::lerp(box_color, Color::white(), 0.1f);
    }
    widget_api->draw_rounded_rect(box_bounds, box_color, 2);
    widget_api->draw_rounded_rect_outline(box_bounds, style.border_color, 2, style.border_width);

    // Draw checkmark if checked
    if (checked) {
        float padding = 3;
        Rect check_bounds{
            box_bounds.x + padding,
            box_bounds.y + padding,
            box_bounds.width - padding * 2,
            box_bounds.height - padding * 2
        };

        // Simple checkmark (two lines)
        Color check_color = Color::hex(0x007ACC);  // Accent color
        if (disabled) check_color = check_color.with_alpha(0.5f);

        Vec2 p1{check_bounds.x + check_bounds.width * 0.2f, check_bounds.center().y};
        Vec2 p2{check_bounds.x + check_bounds.width * 0.4f, check_bounds.bottom() - 2};
        Vec2 p3{check_bounds.right() - 2, check_bounds.y + 2};

        widget_api->draw_line(p1, p2, check_color, 2);
        widget_api->draw_line(p2, p3, check_color, 2);
    }

    // Draw label text
    auto text = widget.get_property<std::string>("text", "");
    if (!text.empty()) {
        Rect text_bounds{
            bounds.x + box_size + 8,
            bounds.y,
            bounds.width - box_size - 8,
            bounds.height
        };
        Color text_color = style.text_color;
        if (disabled) text_color = text_color.with_alpha(0.5f);
        widget_api->draw_text_aligned(text, text_bounds, TextAlign::Left, VerticalAlign::Middle,
                                       text_color, style.font_size);
    }
}

void CoreWidget::render_slider(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    float value = widget_api->get_value(id);
    float min_val = widget.get_property<float>("min", 0.0f);
    float max_val = widget.get_property<float>("max", 1.0f);
    float normalized = (value - min_val) / (max_val - min_val);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    bool hovered = widget_api->is_hovered(id);
    bool dragging = has_state(widget.state, WidgetState::Dragging);

    float track_height = 4;
    float thumb_width = 16;
    float thumb_height = bounds.height;

    // Track background
    Rect track_bounds{
        bounds.x + thumb_width / 2,
        bounds.center().y - track_height / 2,
        bounds.width - thumb_width,
        track_height
    };
    widget_api->draw_rounded_rect(track_bounds, style.background_color, 2);

    // Track fill
    float fill_width = track_bounds.width * normalized;
    Rect fill_bounds{track_bounds.x, track_bounds.y, fill_width, track_bounds.height};
    widget_api->draw_rounded_rect(fill_bounds, Color::hex(0x007ACC), 2);

    // Thumb
    float thumb_x = bounds.x + normalized * (bounds.width - thumb_width);
    Rect thumb_bounds{thumb_x, bounds.y, thumb_width, thumb_height};

    Color thumb_color = Color::hex(0xE0E0E0);
    if (dragging) {
        thumb_color = Color::hex(0xFFFFFF);
    } else if (hovered) {
        thumb_color = Color::hex(0xF0F0F0);
    }

    widget_api->draw_rounded_rect(thumb_bounds, thumb_color, 4);
    widget_api->draw_rounded_rect_outline(thumb_bounds, style.border_color, 4, 1);
}

void CoreWidget::render_progress_bar(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    float value = widget_api->get_value(id);
    float min_val = widget.get_property<float>("min", 0.0f);
    float max_val = widget.get_property<float>("max", 1.0f);
    float normalized = (value - min_val) / (max_val - min_val);
    normalized = std::clamp(normalized, 0.0f, 1.0f);

    auto fill_color = widget.get_property<Color>("fill_color", Color::hex(0x007ACC));
    bool show_text = widget.get_property<bool>("show_text", false);

    // Background
    widget_api->draw_rounded_rect(bounds, style.background_color, style.border_radius);

    // Fill
    float fill_width = bounds.width * normalized;
    Rect fill_bounds{bounds.x, bounds.y, fill_width, bounds.height};
    widget_api->draw_rounded_rect(fill_bounds, fill_color, style.border_radius);

    // Border
    if (style.border_width > 0) {
        widget_api->draw_rounded_rect_outline(bounds, style.border_color,
                                               style.border_radius, style.border_width);
    }

    // Percentage text
    if (show_text) {
        int percent = static_cast<int>(normalized * 100);
        std::string text = std::to_string(percent) + "%";
        widget_api->draw_text_aligned(text, bounds, TextAlign::Center, VerticalAlign::Middle,
                                       style.text_color, style.font_size);
    }
}

void CoreWidget::render_text_input(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    bool focused = widget_api->is_focused(id);
    bool disabled = has_state(widget.state, WidgetState::Disabled);

    // Background
    Color bg_color = style.background_color;
    if (disabled) {
        bg_color = bg_color.with_alpha(0.5f);
    }
    widget_api->draw_rounded_rect(bounds, bg_color, style.border_radius);

    // Border (highlighted when focused)
    Color border_color = style.border_color;
    float border_width = style.border_width;
    if (focused) {
        border_color = Color::hex(0x007ACC);
        border_width = 2;
    }
    widget_api->draw_rounded_rect_outline(bounds, border_color, style.border_radius, border_width);

    // Text content
    auto text = widget_api->get_text(id);
    auto placeholder = widget.get_property<std::string>("placeholder", "");

    float padding = 8;
    Rect text_bounds{bounds.x + padding, bounds.y, bounds.width - padding * 2, bounds.height};

    if (text.empty() && !placeholder.empty()) {
        // Show placeholder
        Color placeholder_color = style.text_color.with_alpha(0.5f);
        widget_api->draw_text_aligned(placeholder, text_bounds, TextAlign::Left, VerticalAlign::Middle,
                                       placeholder_color, style.font_size);
    } else {
        // Show text
        widget_api->draw_text_aligned(text, text_bounds, TextAlign::Left, VerticalAlign::Middle,
                                       style.text_color, style.font_size);

        // Draw cursor if focused
        if (focused && m_cursor_visible) {
            float char_width = style.font_size * 0.6f;
            float cursor_x = text_bounds.x + text.length() * char_width;
            float cursor_y = bounds.center().y - style.font_size * 0.6f;
            widget_api->draw_line({cursor_x, cursor_y},
                                   {cursor_x, cursor_y + style.font_size * 1.2f},
                                   style.text_color, 1);
        }
    }
}

void CoreWidget::render_image(WidgetId id, const WidgetInstance& widget) {
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);

    auto src = widget.get_property<std::string>("src", "");
    auto tint = widget.get_property<Color>("tint", Color::white());
    bool preserve_aspect = widget.get_property<bool>("preserve_aspect", true);

    if (!src.empty()) {
        if (preserve_aspect) {
            // TODO: Calculate aspect-correct bounds
            widget_api->draw_texture(src, bounds, {}, tint);
        } else {
            widget_api->draw_texture(src, bounds, {}, tint);
        }
    }
}

void CoreWidget::render_separator(WidgetId id, const WidgetInstance& widget) {
    (void)widget;
    auto* widget_api = api();
    if (!widget_api) return;

    auto bounds = widget_api->get_bounds(id);
    auto style = widget_api->get_computed_style(id);

    float y = bounds.center().y;
    widget_api->draw_line({bounds.x, y}, {bounds.right(), y}, style.border_color, 1);
}

void CoreWidget::render_spacer(WidgetId id, const WidgetInstance& widget) {
    (void)id;
    (void)widget;
    // Spacer is invisible - just takes up space
}

// =============================================================================
// Events
// =============================================================================

void CoreWidget::on_click(WidgetId id, const WidgetInstance& widget, Vec2 pos) {
    (void)pos;
    auto* widget_api = api();
    if (!widget_api) return;

    if (widget.type == "checkbox") {
        bool current = widget_api->is_checked(id);
        widget_api->set_checked(id, !current);
    } else if (widget.type == "button") {
        // Button click - emit event (handled by subscriber)
    }
}

void CoreWidget::on_hover_enter(WidgetId id, const WidgetInstance& widget) {
    (void)id;
    (void)widget;
    // Could trigger hover animation
}

void CoreWidget::on_hover_exit(WidgetId id, const WidgetInstance& widget) {
    (void)id;
    (void)widget;
    // Could end hover animation
}

void CoreWidget::on_key_press(WidgetId id, const WidgetInstance& widget, int key, int mods) {
    (void)mods;
    auto* widget_api = api();
    if (!widget_api) return;

    if (widget.type == "text_input") {
        auto text = widget_api->get_text(id);

        // Handle backspace
        if (key == 259 && !text.empty()) {  // GLFW_KEY_BACKSPACE
            text.pop_back();
            widget_api->set_text(id, text);
        }
        // Handle enter (could emit event)
        // Handle escape (could blur)
    }
}

void CoreWidget::on_text_input(WidgetId id, const WidgetInstance& widget, const std::string& text) {
    auto* widget_api = api();
    if (!widget_api) return;

    if (widget.type == "text_input") {
        auto current = widget_api->get_text(id);
        current += text;

        // Check max length
        auto max_length = widget.get_property<int>("max_length", 0);
        if (max_length > 0 && static_cast<int>(current.length()) > max_length) {
            current = current.substr(0, max_length);
        }

        widget_api->set_text(id, current);
    }
}

void CoreWidget::on_drag(WidgetId id, const WidgetInstance& widget, Vec2 delta) {
    auto* widget_api = api();
    if (!widget_api) return;

    if (widget.type == "slider") {
        auto bounds = widget_api->get_bounds(id);
        float min_val = widget.get_property<float>("min", 0.0f);
        float max_val = widget.get_property<float>("max", 1.0f);

        // Calculate new value based on drag
        float range = bounds.width - 16;  // Subtract thumb width
        float delta_normalized = delta.x / range;
        float current = widget_api->get_value(id);
        float new_value = current + delta_normalized * (max_val - min_val);
        new_value = std::clamp(new_value, min_val, max_val);

        widget_api->set_value(id, new_value);
    }
}

// =============================================================================
// Plugin Factory
// =============================================================================

} // namespace core_widgets

VOID_WIDGET_PLUGIN(core_widgets::CoreWidget)
