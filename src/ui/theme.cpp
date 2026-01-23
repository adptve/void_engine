/// @file theme.cpp
/// @brief Theme system implementation

#include <void_engine/ui/theme.hpp>

#include <algorithm>
#include <fstream>
#include <mutex>

namespace void_ui {

// =============================================================================
// ThemeColors Implementation
// =============================================================================

ThemeColors ThemeColors::lerp(const ThemeColors& a, const ThemeColors& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    ThemeColors result;
    result.panel_bg = Color::lerp(a.panel_bg, b.panel_bg, t);
    result.panel_border = Color::lerp(a.panel_border, b.panel_border, t);
    result.text = Color::lerp(a.text, b.text, t);
    result.text_dim = Color::lerp(a.text_dim, b.text_dim, t);
    result.success = Color::lerp(a.success, b.success, t);
    result.warning = Color::lerp(a.warning, b.warning, t);
    result.error = Color::lerp(a.error, b.error, t);
    result.info = Color::lerp(a.info, b.info, t);
    result.accent = Color::lerp(a.accent, b.accent, t);

    result.button_bg = Color::lerp(a.button_bg, b.button_bg, t);
    result.button_hover = Color::lerp(a.button_hover, b.button_hover, t);
    result.button_pressed = Color::lerp(a.button_pressed, b.button_pressed, t);
    result.button_disabled = Color::lerp(a.button_disabled, b.button_disabled, t);

    result.input_bg = Color::lerp(a.input_bg, b.input_bg, t);
    result.input_border = Color::lerp(a.input_border, b.input_border, t);
    result.input_focus = Color::lerp(a.input_focus, b.input_focus, t);

    result.scrollbar_bg = Color::lerp(a.scrollbar_bg, b.scrollbar_bg, t);
    result.scrollbar_thumb = Color::lerp(a.scrollbar_thumb, b.scrollbar_thumb, t);
    result.scrollbar_thumb_hover = Color::lerp(a.scrollbar_thumb_hover, b.scrollbar_thumb_hover, t);

    result.selection = Color::lerp(a.selection, b.selection, t);
    result.highlight = Color::lerp(a.highlight, b.highlight, t);

    return result;
}

// =============================================================================
// Theme Built-in Themes
// =============================================================================

Theme Theme::dark() {
    Theme theme;
    theme.name = "dark";

    auto& c = theme.colors;
    c.panel_bg = Color{0.1f, 0.1f, 0.12f, 0.9f};
    c.panel_border = Color{0.3f, 0.3f, 0.35f, 1.0f};
    c.text = Color{1.0f, 1.0f, 1.0f, 1.0f};
    c.text_dim = Color{0.6f, 0.6f, 0.6f, 1.0f};
    c.success = Color{0.3f, 0.9f, 0.3f, 1.0f};
    c.warning = Color{0.9f, 0.8f, 0.2f, 1.0f};
    c.error = Color{0.9f, 0.3f, 0.3f, 1.0f};
    c.info = Color{0.3f, 0.7f, 0.9f, 1.0f};
    c.accent = Color{0.4f, 0.6f, 1.0f, 1.0f};

    c.button_bg = Color{0.2f, 0.2f, 0.25f, 1.0f};
    c.button_hover = Color{0.3f, 0.3f, 0.35f, 1.0f};
    c.button_pressed = Color{0.15f, 0.15f, 0.2f, 1.0f};
    c.button_disabled = Color{0.15f, 0.15f, 0.15f, 0.5f};

    c.input_bg = Color{0.05f, 0.05f, 0.08f, 1.0f};
    c.input_border = Color{0.3f, 0.3f, 0.35f, 1.0f};
    c.input_focus = Color{0.4f, 0.6f, 1.0f, 1.0f};

    c.scrollbar_bg = Color{0.1f, 0.1f, 0.12f, 1.0f};
    c.scrollbar_thumb = Color{0.4f, 0.4f, 0.45f, 1.0f};
    c.scrollbar_thumb_hover = Color{0.5f, 0.5f, 0.55f, 1.0f};

    c.selection = Color{0.2f, 0.4f, 0.8f, 0.5f};
    c.highlight = Color{1.0f, 1.0f, 0.0f, 0.3f};

    theme.text_scale = 1.0f;
    theme.line_height = 1.4f;
    theme.padding = 8.0f;
    theme.border_radius = 4.0f;
    theme.border_width = 1.0f;
    theme.animation_duration = 0.15f;
    theme.scrollbar_width = 8.0f;

    return theme;
}

Theme Theme::light() {
    Theme theme;
    theme.name = "light";

    auto& c = theme.colors;
    c.panel_bg = Color{0.95f, 0.95f, 0.95f, 0.95f};
    c.panel_border = Color{0.7f, 0.7f, 0.7f, 1.0f};
    c.text = Color{0.1f, 0.1f, 0.1f, 1.0f};
    c.text_dim = Color{0.4f, 0.4f, 0.4f, 1.0f};
    c.success = Color{0.1f, 0.7f, 0.1f, 1.0f};
    c.warning = Color{0.8f, 0.6f, 0.0f, 1.0f};
    c.error = Color{0.8f, 0.1f, 0.1f, 1.0f};
    c.info = Color{0.1f, 0.5f, 0.8f, 1.0f};
    c.accent = Color{0.2f, 0.4f, 0.8f, 1.0f};

    c.button_bg = Color{0.85f, 0.85f, 0.85f, 1.0f};
    c.button_hover = Color{0.8f, 0.8f, 0.8f, 1.0f};
    c.button_pressed = Color{0.7f, 0.7f, 0.7f, 1.0f};
    c.button_disabled = Color{0.9f, 0.9f, 0.9f, 0.5f};

    c.input_bg = Color{1.0f, 1.0f, 1.0f, 1.0f};
    c.input_border = Color{0.7f, 0.7f, 0.7f, 1.0f};
    c.input_focus = Color{0.2f, 0.4f, 0.8f, 1.0f};

    c.scrollbar_bg = Color{0.9f, 0.9f, 0.9f, 1.0f};
    c.scrollbar_thumb = Color{0.6f, 0.6f, 0.6f, 1.0f};
    c.scrollbar_thumb_hover = Color{0.5f, 0.5f, 0.5f, 1.0f};

    c.selection = Color{0.2f, 0.4f, 0.8f, 0.3f};
    c.highlight = Color{1.0f, 1.0f, 0.0f, 0.3f};

    theme.text_scale = 1.0f;
    theme.line_height = 1.4f;
    theme.padding = 8.0f;
    theme.border_radius = 4.0f;
    theme.border_width = 1.0f;

    return theme;
}

Theme Theme::high_contrast() {
    Theme theme;
    theme.name = "high_contrast";

    auto& c = theme.colors;
    c.panel_bg = Color{0.0f, 0.0f, 0.0f, 1.0f};
    c.panel_border = Color{1.0f, 1.0f, 1.0f, 1.0f};
    c.text = Color{1.0f, 1.0f, 1.0f, 1.0f};
    c.text_dim = Color{0.8f, 0.8f, 0.8f, 1.0f};
    c.success = Color{0.0f, 1.0f, 0.0f, 1.0f};
    c.warning = Color{1.0f, 1.0f, 0.0f, 1.0f};
    c.error = Color{1.0f, 0.0f, 0.0f, 1.0f};
    c.info = Color{0.0f, 1.0f, 1.0f, 1.0f};
    c.accent = Color{1.0f, 0.0f, 1.0f, 1.0f};

    c.button_bg = Color{0.0f, 0.0f, 0.0f, 1.0f};
    c.button_hover = Color{0.2f, 0.2f, 0.2f, 1.0f};
    c.button_pressed = Color{0.3f, 0.3f, 0.3f, 1.0f};
    c.button_disabled = Color{0.1f, 0.1f, 0.1f, 0.5f};

    c.input_bg = Color{0.0f, 0.0f, 0.0f, 1.0f};
    c.input_border = Color{1.0f, 1.0f, 1.0f, 1.0f};
    c.input_focus = Color{1.0f, 1.0f, 0.0f, 1.0f};

    c.scrollbar_bg = Color{0.0f, 0.0f, 0.0f, 1.0f};
    c.scrollbar_thumb = Color{1.0f, 1.0f, 1.0f, 1.0f};
    c.scrollbar_thumb_hover = Color{1.0f, 1.0f, 0.0f, 1.0f};

    c.selection = Color{1.0f, 1.0f, 0.0f, 0.5f};
    c.highlight = Color{1.0f, 1.0f, 0.0f, 0.5f};

    theme.text_scale = 1.2f;
    theme.line_height = 1.5f;
    theme.padding = 10.0f;
    theme.border_radius = 0.0f;
    theme.border_width = 2.0f;

    return theme;
}

Theme Theme::retro() {
    Theme theme;
    theme.name = "retro";

    auto& c = theme.colors;
    c.panel_bg = Color{0.0f, 0.05f, 0.0f, 0.95f};
    c.panel_border = Color{0.0f, 0.4f, 0.0f, 1.0f};
    c.text = Color{0.0f, 1.0f, 0.0f, 1.0f};
    c.text_dim = Color{0.0f, 0.5f, 0.0f, 1.0f};
    c.success = Color{0.0f, 1.0f, 0.0f, 1.0f};
    c.warning = Color{0.5f, 1.0f, 0.0f, 1.0f};
    c.error = Color{1.0f, 0.3f, 0.0f, 1.0f};
    c.info = Color{0.0f, 0.8f, 0.5f, 1.0f};
    c.accent = Color{0.0f, 1.0f, 0.5f, 1.0f};

    c.button_bg = Color{0.0f, 0.1f, 0.0f, 1.0f};
    c.button_hover = Color{0.0f, 0.2f, 0.0f, 1.0f};
    c.button_pressed = Color{0.0f, 0.05f, 0.0f, 1.0f};
    c.button_disabled = Color{0.0f, 0.05f, 0.0f, 0.5f};

    c.input_bg = Color{0.0f, 0.02f, 0.0f, 1.0f};
    c.input_border = Color{0.0f, 0.4f, 0.0f, 1.0f};
    c.input_focus = Color{0.0f, 1.0f, 0.0f, 1.0f};

    c.scrollbar_bg = Color{0.0f, 0.05f, 0.0f, 1.0f};
    c.scrollbar_thumb = Color{0.0f, 0.4f, 0.0f, 1.0f};
    c.scrollbar_thumb_hover = Color{0.0f, 0.6f, 0.0f, 1.0f};

    c.selection = Color{0.0f, 0.5f, 0.0f, 0.5f};
    c.highlight = Color{0.0f, 1.0f, 0.0f, 0.3f};

    theme.text_scale = 1.0f;
    theme.line_height = 1.3f;
    theme.padding = 6.0f;
    theme.border_radius = 0.0f;
    theme.border_width = 1.0f;

    return theme;
}

Theme Theme::solarized_dark() {
    Theme theme;
    theme.name = "solarized_dark";

    // Solarized base colors
    const Color base03{0.0f, 0.169f, 0.212f, 1.0f};   // #002b36
    const Color base02{0.027f, 0.212f, 0.259f, 1.0f}; // #073642
    const Color base01{0.345f, 0.431f, 0.459f, 1.0f}; // #586e75
    const Color base00{0.396f, 0.482f, 0.514f, 1.0f}; // #657b83
    const Color base0{0.514f, 0.58f, 0.588f, 1.0f};   // #839496
    const Color base1{0.576f, 0.631f, 0.631f, 1.0f};  // #93a1a1
    const Color yellow{0.71f, 0.537f, 0.0f, 1.0f};    // #b58900
    const Color orange{0.796f, 0.294f, 0.086f, 1.0f}; // #cb4b16
    const Color red{0.863f, 0.196f, 0.184f, 1.0f};    // #dc322f
    const Color magenta{0.827f, 0.212f, 0.51f, 1.0f}; // #d33682
    const Color violet{0.424f, 0.443f, 0.769f, 1.0f}; // #6c71c4
    const Color blue{0.149f, 0.545f, 0.824f, 1.0f};   // #268bd2
    const Color cyan{0.165f, 0.631f, 0.596f, 1.0f};   // #2aa198
    const Color green{0.522f, 0.6f, 0.0f, 1.0f};      // #859900

    auto& c = theme.colors;
    c.panel_bg = base03.with_alpha(0.95f);
    c.panel_border = base01;
    c.text = base0;
    c.text_dim = base01;
    c.success = green;
    c.warning = yellow;
    c.error = red;
    c.info = blue;
    c.accent = cyan;

    c.button_bg = base02;
    c.button_hover = base01;
    c.button_pressed = base03;
    c.button_disabled = base02.with_alpha(0.5f);

    c.input_bg = base03;
    c.input_border = base01;
    c.input_focus = blue;

    c.scrollbar_bg = base03;
    c.scrollbar_thumb = base01;
    c.scrollbar_thumb_hover = base00;

    c.selection = blue.with_alpha(0.3f);
    c.highlight = yellow.with_alpha(0.3f);

    theme.text_scale = 1.0f;
    theme.line_height = 1.4f;
    theme.padding = 8.0f;
    theme.border_radius = 4.0f;
    theme.border_width = 1.0f;

    return theme;
}

Theme Theme::solarized_light() {
    Theme theme;
    theme.name = "solarized_light";

    // Solarized base colors (inverted for light)
    const Color base3{0.992f, 0.965f, 0.89f, 1.0f};   // #fdf6e3
    const Color base2{0.933f, 0.91f, 0.835f, 1.0f};   // #eee8d5
    const Color base1{0.576f, 0.631f, 0.631f, 1.0f};  // #93a1a1
    const Color base0{0.514f, 0.58f, 0.588f, 1.0f};   // #839496
    const Color base00{0.396f, 0.482f, 0.514f, 1.0f}; // #657b83
    const Color base01{0.345f, 0.431f, 0.459f, 1.0f}; // #586e75
    const Color yellow{0.71f, 0.537f, 0.0f, 1.0f};
    const Color orange{0.796f, 0.294f, 0.086f, 1.0f};
    const Color red{0.863f, 0.196f, 0.184f, 1.0f};
    const Color blue{0.149f, 0.545f, 0.824f, 1.0f};
    const Color cyan{0.165f, 0.631f, 0.596f, 1.0f};
    const Color green{0.522f, 0.6f, 0.0f, 1.0f};

    auto& c = theme.colors;
    c.panel_bg = base3.with_alpha(0.95f);
    c.panel_border = base1;
    c.text = base00;
    c.text_dim = base1;
    c.success = green;
    c.warning = yellow;
    c.error = red;
    c.info = blue;
    c.accent = cyan;

    c.button_bg = base2;
    c.button_hover = base1;
    c.button_pressed = base3;
    c.button_disabled = base2.with_alpha(0.5f);

    c.input_bg = base3;
    c.input_border = base1;
    c.input_focus = blue;

    c.scrollbar_bg = base3;
    c.scrollbar_thumb = base1;
    c.scrollbar_thumb_hover = base0;

    c.selection = blue.with_alpha(0.3f);
    c.highlight = yellow.with_alpha(0.3f);

    theme.text_scale = 1.0f;
    theme.line_height = 1.4f;
    theme.padding = 8.0f;
    theme.border_radius = 4.0f;
    theme.border_width = 1.0f;

    return theme;
}

Theme Theme::lerp(const Theme& a, const Theme& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    Theme result;
    result.name = t < 0.5f ? a.name : b.name;
    result.colors = ThemeColors::lerp(a.colors, b.colors, t);
    result.text_scale = a.text_scale + (b.text_scale - a.text_scale) * t;
    result.line_height = a.line_height + (b.line_height - a.line_height) * t;
    result.padding = a.padding + (b.padding - a.padding) * t;
    result.border_radius = a.border_radius + (b.border_radius - a.border_radius) * t;
    result.border_width = a.border_width + (b.border_width - a.border_width) * t;
    result.animation_duration = a.animation_duration + (b.animation_duration - a.animation_duration) * t;
    result.scrollbar_width = a.scrollbar_width + (b.scrollbar_width - a.scrollbar_width) * t;

    return result;
}

// =============================================================================
// Theme Registry Implementation
// =============================================================================

struct ThemeRegistry::Impl {
    std::unordered_map<std::string, Theme> themes;
    std::string active_theme_name = "dark";
    mutable std::mutex mutex;

    // Transition state
    std::string transition_target;
    float transition_progress = 0.0f;
    float transition_duration = 0.0f;
    Theme transition_start;
    Theme current_theme;

    // Watch state
    std::string watch_path;
    bool watching = false;

    // Callback
    ThemeChangedCallback on_theme_changed;

    Impl() {
        // Register built-in themes
        themes["dark"] = Theme::dark();
        themes["light"] = Theme::light();
        themes["high_contrast"] = Theme::high_contrast();
        themes["retro"] = Theme::retro();
        themes["solarized_dark"] = Theme::solarized_dark();
        themes["solarized_light"] = Theme::solarized_light();

        current_theme = themes["dark"];
    }
};

ThemeRegistry::ThemeRegistry() : m_impl(std::make_unique<Impl>()) {}

ThemeRegistry::~ThemeRegistry() {
    stop_watching();
}

void ThemeRegistry::register_theme(const std::string& name, Theme theme) {
    std::lock_guard lock(m_impl->mutex);
    theme.name = name;
    m_impl->themes[name] = std::move(theme);
}

void ThemeRegistry::unregister_theme(const std::string& name) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->themes.erase(name);
}

const Theme* ThemeRegistry::get_theme(const std::string& name) const {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->themes.find(name);
    return it != m_impl->themes.end() ? &it->second : nullptr;
}

std::vector<std::string> ThemeRegistry::theme_names() const {
    std::lock_guard lock(m_impl->mutex);
    std::vector<std::string> names;
    names.reserve(m_impl->themes.size());
    for (const auto& [name, _] : m_impl->themes) {
        names.push_back(name);
    }
    return names;
}

bool ThemeRegistry::has_theme(const std::string& name) const {
    std::lock_guard lock(m_impl->mutex);
    return m_impl->themes.count(name) > 0;
}

void ThemeRegistry::set_active_theme(const std::string& name) {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->themes.find(name);
    if (it != m_impl->themes.end()) {
        m_impl->active_theme_name = name;
        m_impl->current_theme = it->second;
        m_impl->transition_progress = 0.0f;
        m_impl->transition_duration = 0.0f;

        if (m_impl->on_theme_changed) {
            m_impl->on_theme_changed(name);
        }
    }
}

const Theme& ThemeRegistry::active_theme() const {
    std::lock_guard lock(m_impl->mutex);
    return m_impl->current_theme;
}

const std::string& ThemeRegistry::active_theme_name() const {
    std::lock_guard lock(m_impl->mutex);
    return m_impl->active_theme_name;
}

void ThemeRegistry::transition_to(const std::string& name, float duration_seconds) {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->themes.find(name);
    if (it != m_impl->themes.end()) {
        m_impl->transition_target = name;
        m_impl->transition_start = m_impl->current_theme;
        m_impl->transition_progress = 0.0f;
        m_impl->transition_duration = duration_seconds;
    }
}

bool ThemeRegistry::update_transition(float delta_seconds) {
    std::lock_guard lock(m_impl->mutex);

    if (m_impl->transition_duration <= 0.0f || m_impl->transition_target.empty()) {
        return false;
    }

    m_impl->transition_progress += delta_seconds / m_impl->transition_duration;

    if (m_impl->transition_progress >= 1.0f) {
        // Transition complete
        m_impl->transition_progress = 0.0f;
        m_impl->transition_duration = 0.0f;
        set_active_theme(m_impl->transition_target);
        m_impl->transition_target.clear();
        return false;
    }

    // Interpolate
    auto it = m_impl->themes.find(m_impl->transition_target);
    if (it != m_impl->themes.end()) {
        m_impl->current_theme = Theme::lerp(
            m_impl->transition_start,
            it->second,
            m_impl->transition_progress
        );
    }

    return true;
}

bool ThemeRegistry::is_transitioning() const {
    std::lock_guard lock(m_impl->mutex);
    return m_impl->transition_duration > 0.0f && !m_impl->transition_target.empty();
}

bool ThemeRegistry::load_theme_from_file(const std::string& path) {
    // TODO: Implement JSON theme loading
    // For now, just return false
    (void)path;
    return false;
}

bool ThemeRegistry::save_theme_to_file(const std::string& name, const std::string& path) const {
    // TODO: Implement JSON theme saving
    (void)name;
    (void)path;
    return false;
}

void ThemeRegistry::watch_directory(const std::string& path) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->watch_path = path;
    m_impl->watching = true;
}

void ThemeRegistry::stop_watching() {
    std::lock_guard lock(m_impl->mutex);
    m_impl->watching = false;
    m_impl->watch_path.clear();
}

void ThemeRegistry::poll_changes() {
    // TODO: Implement file watching
    // For now, no-op
}

void ThemeRegistry::set_theme_changed_callback(ThemeChangedCallback callback) {
    std::lock_guard lock(m_impl->mutex);
    m_impl->on_theme_changed = std::move(callback);
}

} // namespace void_ui
