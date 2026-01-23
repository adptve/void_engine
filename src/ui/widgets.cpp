/// @file widgets.cpp
/// @brief UI Widget implementations

#include <void_engine/ui/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace void_ui {

// =============================================================================
// Helper Functions
// =============================================================================

static std::uint64_t hash_string(const std::string& str) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static std::uint64_t hash_position(float x, float y) {
    std::uint64_t hx = static_cast<std::uint64_t>(x * 1000.0f);
    std::uint64_t hy = static_cast<std::uint64_t>(y * 1000.0f);
    return (hx << 32) | hy;
}

static Color get_stat_color(const Theme& theme, StatType type) {
    switch (type) {
        case StatType::Normal: return theme.colors.text;
        case StatType::Good: return theme.colors.success;
        case StatType::Warning: return theme.colors.warning;
        case StatType::Bad: return theme.colors.error;
        case StatType::Info: return theme.colors.accent;
    }
    return theme.colors.text;
}

static Color get_toast_color(const Theme& theme, ToastType type) {
    switch (type) {
        case ToastType::Info: return theme.colors.accent;
        case ToastType::Success: return theme.colors.success;
        case ToastType::Warning: return theme.colors.warning;
        case ToastType::Error: return theme.colors.error;
    }
    return theme.colors.accent;
}

// =============================================================================
// Label Widget
// =============================================================================

void Label::draw(UiContext& ctx, float x, float y, const std::string& text) {
    ctx.draw_text(text, x, y, ctx.theme().colors.text, ctx.theme().text_scale);
}

void Label::draw(UiContext& ctx, float x, float y, const std::string& text, const Color& color) {
    ctx.draw_text(text, x, y, color, ctx.theme().text_scale);
}

void Label::draw(UiContext& ctx, float x, float y, const std::string& text,
                 const Color& color, float scale) {
    ctx.draw_text(text, x, y, color, scale);
}

// =============================================================================
// Debug Panel Widget
// =============================================================================

void DebugPanel::draw(UiContext& ctx, float x, float y, const std::string& title,
                      const std::vector<DebugStat>& stats) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float padding = theme.padding;
    float line_h = ctx.line_height();

    // Calculate panel dimensions
    float max_label_w = 0.0f;
    float max_value_w = 0.0f;

    for (const auto& stat : stats) {
        max_label_w = std::max(max_label_w, ctx.measure_text(stat.label, scale));
        max_value_w = std::max(max_value_w, ctx.measure_text(stat.value, scale));
    }

    float title_w = ctx.measure_text(title, scale);
    float content_w = max_label_w + max_value_w + padding * 2;
    float panel_w = std::max(title_w + padding * 2, content_w) + padding * 2;
    float panel_h = padding * 2 + line_h + static_cast<float>(stats.size()) * line_h + padding;

    // Draw panel background
    ctx.draw_rect(Rect{x, y, panel_w, panel_h}, theme.colors.panel_bg);

    // Draw border
    ctx.draw_rect_border(Rect{x, y, panel_w, panel_h}, theme.colors.panel_border);

    // Draw title
    float title_x = x + padding;
    float title_y = y + padding;
    ctx.draw_text(title, title_x, title_y, theme.colors.highlight, scale);

    // Draw separator line
    float sep_y = title_y + line_h;
    ctx.draw_rect(Rect{x + padding, sep_y, panel_w - padding * 2, 1.0f}, theme.colors.panel_border);

    // Draw stats
    float stat_y = sep_y + padding;
    for (const auto& stat : stats) {
        ctx.draw_text(stat.label, x + padding, stat_y, theme.colors.text_dim, scale);
        ctx.draw_text(stat.value, x + padding + max_label_w + padding,
                      stat_y, get_stat_color(theme, stat.type), scale);
        stat_y += line_h;
    }
}

void DebugPanel::draw(UiContext& ctx, float x, float y, const std::string& title,
                      const std::vector<std::tuple<std::string, std::string, StatType>>& stats) {
    std::vector<DebugStat> converted;
    converted.reserve(stats.size());
    for (const auto& [label, value, type] : stats) {
        converted.push_back({label, value, type});
    }
    draw(ctx, x, y, title, converted);
}

// =============================================================================
// Progress Bar Widget
// =============================================================================

void ProgressBar::draw(UiContext& ctx, float x, float y, float progress,
                       const ProgressBarConfig& config) {
    draw(ctx, x, y, config.width, config.height, progress, config.fill_color);
}

void ProgressBar::draw(UiContext& ctx, float x, float y, float width, float height,
                       float progress, std::optional<Color> color) {
    const auto& theme = ctx.theme();
    progress = std::clamp(progress, 0.0f, 1.0f);

    // Draw background
    ctx.draw_rect(Rect{x, y, width, height}, theme.colors.input_bg);

    // Draw border
    ctx.draw_rect_border(Rect{x, y, width, height}, theme.colors.panel_border);

    // Draw fill
    float fill_w = (width - 4.0f) * progress;
    Color fill_color = color.value_or(theme.colors.accent);
    ctx.draw_rect(Rect{x + 2.0f, y + 2.0f, fill_w, height - 4.0f}, fill_color);
}

// =============================================================================
// Frame Time Graph Widget
// =============================================================================

void FrameTimeGraph::draw(UiContext& ctx, float x, float y,
                          const std::vector<float>& frame_times,
                          const FrameTimeGraphConfig& config) {
    draw(ctx, x, y, config.width, config.height, frame_times, config.target_fps);
}

void FrameTimeGraph::draw(UiContext& ctx, float x, float y, float width, float height,
                          const std::vector<float>& frame_times, float target_fps) {
    const auto& theme = ctx.theme();

    // Draw background
    ctx.draw_rect(Rect{x, y, width, height}, theme.colors.panel_bg);
    ctx.draw_rect_border(Rect{x, y, width, height}, theme.colors.panel_border);

    if (frame_times.empty()) return;

    // Calculate scale
    float target_time = 1000.0f / target_fps;
    float max_time = target_time * 3.0f;

    // Draw target line
    float target_y = y + height - (height * (target_time / max_time));
    ctx.draw_rect(x + 1, target_y, width - 2, 1.0f, theme.colors.success.with_alpha(0.5f));

    // Draw frame time bars
    float bar_width = (width - 4.0f) / static_cast<float>(frame_times.size());
    float bar_x = x + 2.0f;

    for (float time : frame_times) {
        float bar_height = std::min(height - 4.0f, (height - 4.0f) * (time / max_time));
        float bar_y = y + height - 2.0f - bar_height;

        // Color based on performance
        Color bar_color;
        if (time <= target_time) {
            bar_color = theme.colors.success;
        } else if (time <= target_time * 2.0f) {
            bar_color = theme.colors.warning;
        } else {
            bar_color = theme.colors.error;
        }

        ctx.draw_rect(bar_x, bar_y, std::max(1.0f, bar_width - 1.0f), bar_height, bar_color);
        bar_x += bar_width;
    }

    // Draw labels
    float scale = theme.text_scale * 0.8f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0fms", target_time);
    ctx.draw_text(buf, x + 4, target_y - 10, theme.colors.text_dim, scale);
}

// =============================================================================
// Toast Widget
// =============================================================================

void Toast::draw(UiContext& ctx, float y, const std::string& message, ToastType type) {
    // Center on screen
    float text_w = ctx.measure_text(message, ctx.theme().text_scale);
    float x = (ctx.screen_width() - text_w) / 2.0f - ctx.theme().padding;
    draw(ctx, x, y, message, type);
}

void Toast::draw(UiContext& ctx, float x, float y, const std::string& message, ToastType type) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float padding = theme.padding;

    float text_w = ctx.measure_text(message, scale);
    float text_h = ctx.text_height(scale);

    float toast_w = text_w + padding * 2;
    float toast_h = text_h + padding * 2;

    Color bg_color = get_toast_color(theme, type).with_alpha(0.9f);

    // Draw background
    ctx.draw_rect(Rect{x, y, toast_w, toast_h}, bg_color);

    // Draw text
    ctx.draw_text(message, x + padding, y + padding, Color::white(), scale);
}

// =============================================================================
// Help Modal Widget
// =============================================================================

void HelpModal::draw(UiContext& ctx, const std::string& title,
                     const std::vector<HelpControl>& controls,
                     const std::string& footer) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float padding = theme.padding;
    float line_h = ctx.line_height();

    // Calculate dimensions
    float max_key_w = 0.0f;
    float max_desc_w = 0.0f;

    for (const auto& ctrl : controls) {
        max_key_w = std::max(max_key_w, ctx.measure_text(ctrl.key, scale));
        max_desc_w = std::max(max_desc_w, ctx.measure_text(ctrl.description, scale));
    }

    float title_w = ctx.measure_text(title, scale);
    float footer_w = footer.empty() ? 0.0f : ctx.measure_text(footer, scale);
    float content_w = max_key_w + max_desc_w + padding * 4;
    float modal_w = std::max({title_w, footer_w, content_w}) + padding * 4;

    float header_h = line_h + padding * 2;
    float content_h = controls.size() * line_h + padding * 2;
    float footer_h = footer.empty() ? 0.0f : line_h + padding;
    float modal_h = header_h + content_h + footer_h + padding;

    // Center on screen
    float modal_x = (ctx.screen_width() - modal_w) / 2.0f;
    float modal_y = (ctx.screen_height() - modal_h) / 2.0f;

    // Draw semi-transparent overlay
    ctx.draw_rect(Rect{0, 0, ctx.screen_width(), ctx.screen_height()},
                  Color::black().with_alpha(0.5f));

    // Draw modal background
    ctx.draw_rect(Rect{modal_x, modal_y, modal_w, modal_h}, theme.colors.panel_bg);
    ctx.draw_rect_border(Rect{modal_x, modal_y, modal_w, modal_h}, theme.colors.panel_border, 2.0f);

    // Draw title
    float title_x = modal_x + (modal_w - title_w) / 2.0f;
    float title_y = modal_y + padding;
    ctx.draw_text(title, title_x, title_y, theme.colors.highlight, scale);

    // Draw separator
    float sep_y = modal_y + header_h;
    ctx.draw_rect(Rect{modal_x + padding, sep_y, modal_w - padding * 2, 1.0f}, theme.colors.panel_border);

    // Draw controls
    float ctrl_y = sep_y + padding;
    for (const auto& ctrl : controls) {
        ctx.draw_text(ctrl.key, modal_x + padding * 2, ctrl_y, theme.colors.accent, scale);
        ctx.draw_text(ctrl.description, modal_x + padding * 2 + max_key_w + padding * 2,
                      ctrl_y, theme.colors.text, scale);
        ctrl_y += line_h;
    }

    // Draw footer
    if (!footer.empty()) {
        float footer_x = modal_x + (modal_w - footer_w) / 2.0f;
        float footer_y = modal_y + modal_h - line_h - padding;
        ctx.draw_text(footer, footer_x, footer_y, theme.colors.text_dim, scale);
    }
}

void HelpModal::draw(UiContext& ctx, const std::string& title,
                     const std::vector<std::pair<std::string, std::string>>& controls,
                     const std::string& footer) {
    std::vector<HelpControl> converted;
    converted.reserve(controls.size());
    for (const auto& [key, desc] : controls) {
        converted.push_back({key, desc});
    }
    draw(ctx, title, converted, footer);
}

// =============================================================================
// Button Widget
// =============================================================================

ButtonResult Button::draw(UiContext& ctx, float x, float y,
                          const std::string& label,
                          const ButtonConfig& config) {
    return draw(ctx, hash_string(label) ^ hash_position(x, y), x, y, label, config);
}

ButtonResult Button::draw(UiContext& ctx, std::uint64_t id, float x, float y,
                          const std::string& label,
                          const ButtonConfig& config) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float padding = config.padding > 0.0f ? config.padding : theme.padding;

    float text_w = ctx.measure_text(label, scale);
    float text_h = ctx.text_height(scale);

    float button_w = config.width > 0.0f ? config.width : text_w + padding * 2;
    float button_h = config.height > 0.0f ? config.height : text_h + padding * 2;

    Rect button_rect{x, y, button_w, button_h};

    ButtonResult result;
    result.hovered = ctx.is_hovered(button_rect);
    result.clicked = config.enabled && ctx.is_clicked(button_rect, 0);
    result.held = config.enabled && result.hovered && ctx.is_mouse_down(0);

    // Determine colors
    Color bg_color = config.bg_color.value_or(theme.colors.button_bg);
    Color text_color = config.text_color.value_or(theme.colors.text);

    if (!config.enabled) {
        bg_color = bg_color.with_alpha(0.5f);
        text_color = text_color.with_alpha(0.5f);
    } else if (result.held) {
        bg_color = theme.colors.button_pressed;
    } else if (result.hovered) {
        bg_color = theme.colors.button_hover;
    }

    // Draw button
    ctx.draw_rect(button_rect, bg_color);
    ctx.draw_rect_border(button_rect, theme.colors.panel_border);

    // Draw text centered
    float text_x = x + (button_w - text_w) / 2.0f;
    float text_y = y + (button_h - text_h) / 2.0f;
    ctx.draw_text(label, text_x, text_y, text_color, scale);

    return result;
}

// =============================================================================
// Checkbox Widget
// =============================================================================

CheckboxResult Checkbox::draw(UiContext& ctx, float x, float y,
                              const std::string& label, bool checked) {
    return draw(ctx, hash_string(label) ^ hash_position(x, y), x, y, label, checked);
}

CheckboxResult Checkbox::draw(UiContext& ctx, std::uint64_t id, float x, float y,
                              const std::string& label, bool checked) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float text_h = ctx.text_height(scale);

    float box_size = text_h;
    float padding = 4.0f;

    Rect box_rect{x, y, box_size, box_size};
    Rect full_rect{x, y, box_size + padding + ctx.measure_text(label, scale), box_size};

    CheckboxResult result;
    result.hovered = ctx.is_hovered(full_rect);
    result.changed = ctx.is_clicked(full_rect, 0);
    result.checked = result.changed ? !checked : checked;

    // Draw checkbox box
    Color bg_color = result.hovered ? theme.colors.input_bg.brighten(0.1f)
                                    : theme.colors.input_bg;
    ctx.draw_rect(box_rect, bg_color);
    ctx.draw_rect_border(box_rect, theme.colors.panel_border);

    // Draw checkmark if checked
    if (result.checked) {
        float inset = 3.0f;
        ctx.draw_rect(x + inset, y + inset, box_size - inset * 2, box_size - inset * 2,
                      theme.colors.accent);
    }

    // Draw label
    ctx.draw_text(label, x + box_size + padding, y + (box_size - text_h) / 2.0f,
                  theme.colors.text, scale);

    return result;
}

// =============================================================================
// Slider Widget
// =============================================================================

SliderResult Slider::draw(UiContext& ctx, float x, float y,
                          const std::string& label, float value,
                          const SliderConfig& config) {
    return draw(ctx, hash_string(label) ^ hash_position(x, y), x, y, label, value, config);
}

SliderResult Slider::draw(UiContext& ctx, std::uint64_t id, float x, float y,
                          const std::string& label, float value,
                          const SliderConfig& config) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float text_h = ctx.text_height(scale);
    float line_h = ctx.line_height();

    float label_w = ctx.measure_text(label, scale);
    float track_h = 8.0f;
    float handle_w = 16.0f;
    float handle_h = 20.0f;

    // Draw label
    ctx.draw_text(label, x, y, theme.colors.text, scale);

    // Track position
    float track_x = x;
    float track_y = y + text_h + 4.0f;
    float track_w = config.width;

    Rect track_rect{track_x, track_y, track_w, track_h};
    Rect full_rect{track_x, track_y - (handle_h - track_h) / 2.0f, track_w, handle_h};

    SliderResult result;
    result.hovered = ctx.is_hovered(full_rect);
    result.value = std::clamp(value, config.min_value, config.max_value);
    result.changed = false;
    result.dragging = false;

    // Handle dragging
    if (ctx.is_clicked(full_rect, 0) || (result.hovered && ctx.is_mouse_down(0))) {
        result.dragging = true;
        float mouse_x = ctx.mouse_position().x;
        float normalized = (mouse_x - track_x) / track_w;
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        float new_value = config.min_value + normalized * (config.max_value - config.min_value);

        if (std::abs(new_value - result.value) > 0.0001f) {
            result.value = new_value;
            result.changed = true;
        }
    }

    // Draw track background
    ctx.draw_rect(track_rect, theme.colors.input_bg);
    ctx.draw_rect_border(track_rect, theme.colors.panel_border);

    // Draw filled portion
    float normalized = (result.value - config.min_value) / (config.max_value - config.min_value);
    float fill_w = track_w * normalized;
    ctx.draw_rect(track_x + 1, track_y + 1, fill_w - 2, track_h - 2, theme.colors.accent);

    // Draw handle
    float handle_x = track_x + fill_w - handle_w / 2.0f;
    float handle_y = track_y - (handle_h - track_h) / 2.0f;
    Color handle_color = result.dragging ? theme.colors.button_pressed
                       : result.hovered ? theme.colors.button_hover
                       : theme.colors.button_bg;
    ctx.draw_rect(handle_x, handle_y, handle_w, handle_h, handle_color);
    ctx.draw_rect_border(handle_x, handle_y, handle_w, handle_h, theme.colors.panel_border);

    // Draw value if enabled
    if (config.show_value) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), config.format.c_str(), result.value);
        float value_w = ctx.measure_text(buf, scale);
        ctx.draw_text(buf, x + track_w - value_w, y, theme.colors.text_dim, scale);
    }

    return result;
}

// =============================================================================
// Text Input Widget
// =============================================================================

TextInputResult TextInput::draw(UiContext& ctx, float x, float y,
                                const std::string& text,
                                const TextInputConfig& config) {
    return draw(ctx, hash_position(x, y), x, y, text, config);
}

TextInputResult TextInput::draw(UiContext& ctx, std::uint64_t id, float x, float y,
                                const std::string& text,
                                const TextInputConfig& config) {
    const auto& theme = ctx.theme();
    float scale = theme.text_scale;
    float text_h = ctx.text_height(scale);
    float padding = 4.0f;

    float input_w = config.width;
    float input_h = text_h + padding * 2;

    Rect input_rect{x, y, input_w, input_h};

    bool was_focused = ctx.is_focused(id);
    bool clicked = ctx.is_clicked(input_rect, 0);

    TextInputResult result;
    result.text = text;
    result.changed = false;
    result.submitted = false;
    result.focused = was_focused;

    // Handle focus
    if (clicked) {
        ctx.set_focus(id);
        result.focused = true;
    }

    // Handle keyboard input when focused
    if (result.focused) {
        // Handle text input
        const auto& input = ctx.text_input();
        if (!input.empty()) {
            // Check max length
            if (config.max_length == 0 || result.text.length() + input.length() <= config.max_length) {
                result.text += input;
                result.changed = true;
            }
        }

        // Handle backspace
        if (ctx.is_key_pressed(UiContext::Key::Backspace) && !result.text.empty()) {
            // Handle UTF-8: find last character start
            std::size_t pos = result.text.length();
            while (pos > 0) {
                --pos;
                // UTF-8 continuation bytes start with 10xxxxxx
                auto byte = static_cast<unsigned char>(result.text[pos]);
                if ((byte & 0xC0) != 0x80) {
                    // Found start of character
                    break;
                }
            }
            result.text.erase(pos);
            result.changed = true;
        }

        // Handle delete key
        if (ctx.is_key_pressed(UiContext::Key::Delete) && !result.text.empty()) {
            // For simplicity, delete acts same as backspace (no cursor position tracking)
            std::size_t pos = result.text.length();
            while (pos > 0) {
                --pos;
                auto byte = static_cast<unsigned char>(result.text[pos]);
                if ((byte & 0xC0) != 0x80) {
                    break;
                }
            }
            result.text.erase(pos);
            result.changed = true;
        }

        // Handle enter/submit
        if (ctx.is_key_pressed(UiContext::Key::Enter)) {
            result.submitted = true;
            if (!config.multiline) {
                ctx.clear_focus();
                result.focused = false;
            }
        }

        // Handle escape to unfocus
        if (ctx.is_key_pressed(UiContext::Key::Escape)) {
            ctx.clear_focus();
            result.focused = false;
        }

        // Handle Ctrl+A to select all (clear and prepare for new input)
        if (ctx.is_ctrl_down() && ctx.is_key_pressed(UiContext::Key::A)) {
            // For now, just select all by clearing (simple implementation)
            // A full implementation would need selection state
        }

        // Handle Ctrl+V paste (platform-specific, not implemented here)
        // Would require clipboard access which is platform-dependent
    }

    // Draw background
    Color bg_color = result.focused ? theme.colors.input_bg.brighten(0.05f)
                                    : theme.colors.input_bg;
    ctx.draw_rect(input_rect, bg_color);

    // Draw border
    Color border_color = result.focused ? theme.colors.accent : theme.colors.panel_border;
    ctx.draw_rect_border(input_rect, border_color);

    // Draw text or placeholder
    float text_x = x + padding;
    float text_y = y + padding;

    if (result.text.empty() && !config.placeholder.empty()) {
        ctx.draw_text(config.placeholder, text_x, text_y, theme.colors.text_dim, scale);
    } else {
        std::string display_text = config.password
            ? std::string(result.text.length(), '*')
            : result.text;

        // Clip text if too wide (show end of text when editing)
        float max_text_w = input_w - padding * 2 - 4.0f; // Leave room for cursor
        float text_w = ctx.measure_text(display_text, scale);
        std::string visible_text = display_text;

        if (text_w > max_text_w && result.focused) {
            // Show the end of the text when focused and text is too long
            while (!visible_text.empty() && ctx.measure_text(visible_text, scale) > max_text_w) {
                // Remove first UTF-8 character
                std::size_t char_len = 1;
                auto first_byte = static_cast<unsigned char>(visible_text[0]);
                if ((first_byte & 0xF8) == 0xF0) char_len = 4;
                else if ((first_byte & 0xF0) == 0xE0) char_len = 3;
                else if ((first_byte & 0xE0) == 0xC0) char_len = 2;
                visible_text.erase(0, char_len);
            }
        }

        ctx.draw_text(visible_text, text_x, text_y, theme.colors.text, scale);

        // Draw cursor if focused (blinking effect based on time would be nice but not essential)
        if (result.focused) {
            float cursor_x = text_x + ctx.measure_text(visible_text, scale);
            ctx.draw_rect(cursor_x, text_y, 2.0f, text_h, theme.colors.text);
        }
    }

    return result;
}

// =============================================================================
// Panel Widget
// =============================================================================

Rect Panel::begin(UiContext& ctx, float x, float y, const PanelConfig& config) {
    const auto& theme = ctx.theme();
    float padding = config.padding > 0.0f ? config.padding : theme.padding;

    float panel_w = config.width > 0.0f ? config.width : 200.0f;
    float panel_h = config.height > 0.0f ? config.height : 100.0f;

    float header_h = 0.0f;
    if (config.show_title && !config.title.empty()) {
        header_h = ctx.line_height() + padding;
    }

    // Draw background
    Color bg_color = config.bg_color.value_or(theme.colors.panel_bg);
    ctx.draw_rect(x, y, panel_w, panel_h, bg_color);

    // Draw border
    if (config.show_border) {
        Color border_color = config.border_color.value_or(theme.colors.panel_border);
        ctx.draw_rect_border(x, y, panel_w, panel_h, border_color);
    }

    // Draw title
    if (config.show_title && !config.title.empty()) {
        ctx.draw_text(config.title, x + padding, y + padding,
                      theme.colors.highlight, theme.text_scale);
        ctx.draw_rect(x + padding, y + header_h, panel_w - padding * 2, 1.0f,
                      theme.colors.panel_border);
    }

    // Push clip rect for content
    Rect content_rect{
        x + padding,
        y + padding + header_h,
        panel_w - padding * 2,
        panel_h - padding * 2 - header_h
    };
    ctx.push_clip_rect(content_rect);

    // Set cursor to content area
    ctx.push_cursor();
    ctx.set_cursor(content_rect.x, content_rect.y);

    return content_rect;
}

void Panel::end(UiContext& ctx) {
    ctx.pop_cursor();
    ctx.pop_clip_rect();
}

// =============================================================================
// Separator Widget
// =============================================================================

void Separator::draw(UiContext& ctx) {
    const auto& theme = ctx.theme();
    float x = ctx.cursor_x();
    float y = ctx.cursor_y();
    float width = ctx.screen_width() - x;

    draw(ctx, x, y, width);

    ctx.advance_cursor(0, theme.padding);
}

void Separator::draw(UiContext& ctx, float x, float y, float width) {
    ctx.draw_rect(x, y, width, 1.0f, ctx.theme().colors.panel_border);
}

// =============================================================================
// Spacing Widget
// =============================================================================

void Spacing::vertical(UiContext& ctx, float amount) {
    ctx.advance_cursor(0, amount);
}

void Spacing::horizontal(UiContext& ctx, float amount) {
    ctx.advance_cursor(amount, 0);
}

} // namespace void_ui
