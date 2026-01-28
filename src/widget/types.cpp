/// @file types.cpp
/// @brief Implementation of widget types

#include <void_engine/widget/types.hpp>

#include <cmath>

namespace void_widget {

// =============================================================================
// Easing Functions
// =============================================================================

float apply_easing(float t, EasingType easing) {
    // Clamp t to [0, 1]
    t = std::clamp(t, 0.0f, 1.0f);

    switch (easing) {
        case EasingType::Linear:
            return t;

        case EasingType::EaseIn:
        case EasingType::EaseInQuad:
            return t * t;

        case EasingType::EaseOut:
        case EasingType::EaseOutQuad:
            return t * (2 - t);

        case EasingType::EaseInOut:
        case EasingType::EaseInOutQuad:
            return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;

        case EasingType::EaseInCubic:
            return t * t * t;

        case EasingType::EaseOutCubic: {
            float t1 = t - 1;
            return t1 * t1 * t1 + 1;
        }

        case EasingType::EaseInOutCubic:
            return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;

        case EasingType::EaseInElastic: {
            if (t == 0 || t == 1) return t;
            float p = 0.3f;
            float s = p / 4;
            float t1 = t - 1;
            return -std::pow(2.0f, 10.0f * t1) * std::sin((t1 - s) * (2 * 3.14159265f) / p);
        }

        case EasingType::EaseOutElastic: {
            if (t == 0 || t == 1) return t;
            float p = 0.3f;
            float s = p / 4;
            return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * (2 * 3.14159265f) / p) + 1;
        }

        case EasingType::EaseInOutElastic: {
            if (t == 0 || t == 1) return t;
            float p = 0.45f;
            float s = p / 4;
            float t1 = t * 2;
            if (t1 < 1) {
                t1 -= 1;
                return -0.5f * std::pow(2.0f, 10.0f * t1) * std::sin((t1 - s) * (2 * 3.14159265f) / p);
            }
            t1 -= 1;
            return std::pow(2.0f, -10.0f * t1) * std::sin((t1 - s) * (2 * 3.14159265f) / p) * 0.5f + 1;
        }

        case EasingType::EaseInBounce:
            return 1 - apply_easing(1 - t, EasingType::EaseOutBounce);

        case EasingType::EaseOutBounce: {
            if (t < 1 / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2 / 2.75f) {
                float t1 = t - 1.5f / 2.75f;
                return 7.5625f * t1 * t1 + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                float t1 = t - 2.25f / 2.75f;
                return 7.5625f * t1 * t1 + 0.9375f;
            } else {
                float t1 = t - 2.625f / 2.75f;
                return 7.5625f * t1 * t1 + 0.984375f;
            }
        }

        case EasingType::EaseInOutBounce:
            return t < 0.5f
                ? (1 - apply_easing(1 - 2 * t, EasingType::EaseOutBounce)) * 0.5f
                : (1 + apply_easing(2 * t - 1, EasingType::EaseOutBounce)) * 0.5f;
    }

    return t;
}

// =============================================================================
// Theme Presets
// =============================================================================

Theme Theme::dark() {
    Theme theme;
    theme.name = "dark";

    // Panel colors
    theme.panel_background = Color::hex(0x1E1E1E);
    theme.panel_background_alt = Color::hex(0x252525);
    theme.panel_border = Color::hex(0x3C3C3C);
    theme.panel_header = Color::hex(0x2D2D2D);

    // Text colors
    theme.text_primary = Color::hex(0xE0E0E0);
    theme.text_secondary = Color::hex(0xA0A0A0);
    theme.text_disabled = Color::hex(0x606060);
    theme.text_highlight = Color::hex(0xFFFFFF);

    // Interactive colors
    theme.button_normal = Color::hex(0x3C3C3C);
    theme.button_hovered = Color::hex(0x505050);
    theme.button_pressed = Color::hex(0x606060);
    theme.button_disabled = Color::hex(0x2A2A2A);

    // Accent colors
    theme.accent_primary = Color::hex(0x007ACC);
    theme.accent_secondary = Color::hex(0x1E90FF);
    theme.accent_success = Color::hex(0x4EC9B0);
    theme.accent_warning = Color::hex(0xDCDCAA);
    theme.accent_error = Color::hex(0xF44747);

    // Input colors
    theme.input_background = Color::hex(0x3C3C3C);
    theme.input_border = Color::hex(0x5A5A5A);
    theme.input_border_focused = Color::hex(0x007ACC);

    // Selection
    theme.selection = Color::hex(0x264F78);
    theme.highlight = Color::hex(0x3A3D41);

    // Scrollbar
    theme.scrollbar_track = Color::hex(0x1E1E1E);
    theme.scrollbar_thumb = Color::hex(0x424242);
    theme.scrollbar_thumb_hovered = Color::hex(0x4F4F4F);

    // Styling
    theme.text_size = 14.0f;
    theme.line_height = 1.4f;
    theme.border_width = 1.0f;
    theme.border_radius = 4.0f;
    theme.padding = 8.0f;
    theme.spacing = 4.0f;
    theme.animation_duration = 0.15f;

    return theme;
}

Theme Theme::light() {
    Theme theme;
    theme.name = "light";

    // Panel colors
    theme.panel_background = Color::hex(0xF3F3F3);
    theme.panel_background_alt = Color::hex(0xFFFFFF);
    theme.panel_border = Color::hex(0xD4D4D4);
    theme.panel_header = Color::hex(0xE8E8E8);

    // Text colors
    theme.text_primary = Color::hex(0x1E1E1E);
    theme.text_secondary = Color::hex(0x6E6E6E);
    theme.text_disabled = Color::hex(0xA0A0A0);
    theme.text_highlight = Color::hex(0x000000);

    // Interactive colors
    theme.button_normal = Color::hex(0xDDDDDD);
    theme.button_hovered = Color::hex(0xCCCCCC);
    theme.button_pressed = Color::hex(0xBBBBBB);
    theme.button_disabled = Color::hex(0xEEEEEE);

    // Accent colors
    theme.accent_primary = Color::hex(0x0078D4);
    theme.accent_secondary = Color::hex(0x106EBE);
    theme.accent_success = Color::hex(0x107C10);
    theme.accent_warning = Color::hex(0xCA5010);
    theme.accent_error = Color::hex(0xD13438);

    // Input colors
    theme.input_background = Color::hex(0xFFFFFF);
    theme.input_border = Color::hex(0xCCCCCC);
    theme.input_border_focused = Color::hex(0x0078D4);

    // Selection
    theme.selection = Color::hex(0xADD6FF);
    theme.highlight = Color::hex(0xE8E8E8);

    // Scrollbar
    theme.scrollbar_track = Color::hex(0xF3F3F3);
    theme.scrollbar_thumb = Color::hex(0xC1C1C1);
    theme.scrollbar_thumb_hovered = Color::hex(0xA8A8A8);

    // Styling
    theme.text_size = 14.0f;
    theme.line_height = 1.4f;
    theme.border_width = 1.0f;
    theme.border_radius = 4.0f;
    theme.padding = 8.0f;
    theme.spacing = 4.0f;
    theme.animation_duration = 0.15f;

    return theme;
}

Theme Theme::high_contrast() {
    Theme theme;
    theme.name = "high_contrast";

    // Panel colors
    theme.panel_background = Color::hex(0x000000);
    theme.panel_background_alt = Color::hex(0x1A1A1A);
    theme.panel_border = Color::hex(0xFFFFFF);
    theme.panel_header = Color::hex(0x000000);

    // Text colors
    theme.text_primary = Color::hex(0xFFFFFF);
    theme.text_secondary = Color::hex(0xFFFF00);
    theme.text_disabled = Color::hex(0x00FF00);
    theme.text_highlight = Color::hex(0xFFFFFF);

    // Interactive colors
    theme.button_normal = Color::hex(0x000000);
    theme.button_hovered = Color::hex(0x1A1A1A);
    theme.button_pressed = Color::hex(0x333333);
    theme.button_disabled = Color::hex(0x000000);

    // Accent colors
    theme.accent_primary = Color::hex(0x1AEBFF);
    theme.accent_secondary = Color::hex(0x3FF23F);
    theme.accent_success = Color::hex(0x00FF00);
    theme.accent_warning = Color::hex(0xFFFF00);
    theme.accent_error = Color::hex(0xFF0000);

    // Input colors
    theme.input_background = Color::hex(0x000000);
    theme.input_border = Color::hex(0xFFFFFF);
    theme.input_border_focused = Color::hex(0x1AEBFF);

    // Selection
    theme.selection = Color::hex(0x1AEBFF);
    theme.highlight = Color::hex(0x333333);

    // Scrollbar
    theme.scrollbar_track = Color::hex(0x000000);
    theme.scrollbar_thumb = Color::hex(0xFFFFFF);
    theme.scrollbar_thumb_hovered = Color::hex(0x1AEBFF);

    // Styling
    theme.text_size = 16.0f;
    theme.line_height = 1.5f;
    theme.border_width = 2.0f;
    theme.border_radius = 0.0f;
    theme.padding = 10.0f;
    theme.spacing = 6.0f;
    theme.animation_duration = 0.0f;  // No animations for accessibility

    return theme;
}

} // namespace void_widget
