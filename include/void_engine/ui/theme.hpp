#pragma once

/// @file theme.hpp
/// @brief Theme system with hot-reload support
///
/// Provides customizable color schemes and styling with:
/// - Built-in themes (dark, light, high-contrast, retro)
/// - Hot-reloadable custom themes from JSON
/// - Theme interpolation for smooth transitions

#include "fwd.hpp"
#include "types.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace void_ui {

// =============================================================================
// Theme Colors
// =============================================================================

/// Color scheme for UI elements
struct ThemeColors {
    Color panel_bg;       // Background color for panels
    Color panel_border;   // Border color for panels
    Color text;           // Primary text color
    Color text_dim;       // Secondary/dimmed text color
    Color success;        // Success/positive (green)
    Color warning;        // Warning (yellow/orange)
    Color error;          // Error/negative (red)
    Color info;           // Info/highlight (cyan/blue)
    Color accent;         // Accent color

    // Interactive states
    Color button_bg;
    Color button_hover;
    Color button_pressed;
    Color button_disabled;

    Color input_bg;
    Color input_border;
    Color input_focus;

    Color scrollbar_bg;
    Color scrollbar_thumb;
    Color scrollbar_thumb_hover;

    Color selection;
    Color highlight;

    /// Interpolate between two color schemes
    [[nodiscard]] static ThemeColors lerp(const ThemeColors& a, const ThemeColors& b, float t);
};

// =============================================================================
// Theme
// =============================================================================

/// Complete theme configuration
struct Theme {
    /// Theme name/ID
    std::string name;

    /// Color scheme
    ThemeColors colors;

    /// Default text scale (1.0 = 100%)
    float text_scale = 1.0f;

    /// Line height multiplier
    float line_height = 1.4f;

    /// Panel padding
    float padding = 8.0f;

    /// Border radius (for rounded corners)
    float border_radius = 4.0f;

    /// Border width
    float border_width = 1.0f;

    /// Animation duration in seconds
    float animation_duration = 0.15f;

    /// Scrollbar width
    float scrollbar_width = 8.0f;

    // =========================================================================
    // Built-in Themes
    // =========================================================================

    /// Dark theme (default)
    static Theme dark();

    /// Light theme
    static Theme light();

    /// High contrast theme for accessibility
    static Theme high_contrast();

    /// Retro/terminal green theme
    static Theme retro();

    /// Solarized dark theme
    static Theme solarized_dark();

    /// Solarized light theme
    static Theme solarized_light();

    // =========================================================================
    // Helpers
    // =========================================================================

    /// Get scaled line height in pixels
    [[nodiscard]] float line_height_px() const {
        return 16.0f * text_scale * line_height;
    }

    /// Get color for stat type
    [[nodiscard]] Color stat_color(StatType type) const {
        switch (type) {
            case StatType::Normal: return colors.text;
            case StatType::Good: return colors.success;
            case StatType::Warning: return colors.warning;
            case StatType::Bad: return colors.error;
            case StatType::Info: return colors.info;
        }
        return colors.text;
    }

    /// Get background color for toast type
    [[nodiscard]] Color toast_bg_color(ToastType type) const {
        switch (type) {
            case ToastType::Info: return Color{0.1f, 0.3f, 0.5f, 0.95f};
            case ToastType::Success: return Color{0.1f, 0.4f, 0.1f, 0.95f};
            case ToastType::Warning: return Color{0.5f, 0.4f, 0.1f, 0.95f};
            case ToastType::Error: return Color{0.5f, 0.1f, 0.1f, 0.95f};
        }
        return colors.panel_bg;
    }

    /// Get border color for toast type
    [[nodiscard]] Color toast_border_color(ToastType type) const {
        switch (type) {
            case ToastType::Info: return colors.info;
            case ToastType::Success: return colors.success;
            case ToastType::Warning: return colors.warning;
            case ToastType::Error: return colors.error;
        }
        return colors.panel_border;
    }

    /// Interpolate between two themes
    [[nodiscard]] static Theme lerp(const Theme& a, const Theme& b, float t);
};

// =============================================================================
// Theme Registry (Hot-Reload Support)
// =============================================================================

/// Theme registry with hot-reload support
class ThemeRegistry {
public:
    using ThemeChangedCallback = std::function<void(const std::string& theme_name)>;

    ThemeRegistry();
    ~ThemeRegistry();

    // Non-copyable
    ThemeRegistry(const ThemeRegistry&) = delete;
    ThemeRegistry& operator=(const ThemeRegistry&) = delete;

    // =========================================================================
    // Theme Management
    // =========================================================================

    /// Register a theme
    void register_theme(const std::string& name, Theme theme);

    /// Unregister a theme
    void unregister_theme(const std::string& name);

    /// Get a theme by name
    [[nodiscard]] const Theme* get_theme(const std::string& name) const;

    /// Get all registered theme names
    [[nodiscard]] std::vector<std::string> theme_names() const;

    /// Check if a theme exists
    [[nodiscard]] bool has_theme(const std::string& name) const;

    // =========================================================================
    // Active Theme
    // =========================================================================

    /// Set the active theme
    void set_active_theme(const std::string& name);

    /// Get the active theme
    [[nodiscard]] const Theme& active_theme() const;

    /// Get active theme name
    [[nodiscard]] const std::string& active_theme_name() const;

    // =========================================================================
    // Theme Transitions
    // =========================================================================

    /// Transition to a new theme with animation
    void transition_to(const std::string& name, float duration_seconds = 0.3f);

    /// Update theme transition (call each frame)
    /// Returns true if a transition is in progress
    bool update_transition(float delta_seconds);

    /// Check if a transition is in progress
    [[nodiscard]] bool is_transitioning() const;

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Load a theme from JSON file
    bool load_theme_from_file(const std::string& path);

    /// Save a theme to JSON file
    bool save_theme_to_file(const std::string& name, const std::string& path) const;

    /// Watch a directory for theme changes
    void watch_directory(const std::string& path);

    /// Stop watching for changes
    void stop_watching();

    /// Check for file changes and reload (call periodically)
    void poll_changes();

    // =========================================================================
    // Callbacks
    // =========================================================================

    /// Set callback for theme changes
    void set_theme_changed_callback(ThemeChangedCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace void_ui
