/// @file elements.cpp
/// @brief Implementation of HUD elements for void_hud module

#include "void_engine/hud/elements.hpp"

#include <algorithm>
#include <cmath>

namespace void_hud {

// =============================================================================
// HudElementBase
// =============================================================================

HudElementBase::HudElementBase() {
    m_properties.type = HudElementType::Panel;
}

HudElementBase::HudElementBase(const ElementProperties& props)
    : m_properties(props) {}

HudElementBase::~HudElementBase() {
    // Remove from parent if any
    if (m_parent) {
        m_parent->remove_child(this);
    }

    // Clear children
    for (auto* child : m_children) {
        if (auto* base = dynamic_cast<HudElementBase*>(child)) {
            base->m_parent = nullptr;
        }
    }
}

void HudElementBase::set_position(const Vec2& pos) {
    m_properties.position = pos;
    mark_dirty();
}

void HudElementBase::set_size(const Vec2& size) {
    m_properties.size = size;
    mark_dirty();
}

void HudElementBase::set_rotation(float rotation) {
    m_properties.rotation = rotation;
    mark_dirty();
}

void HudElementBase::set_scale(const Vec2& scale) {
    m_properties.scale = scale;
    mark_dirty();
}

void HudElementBase::set_pivot(const Vec2& pivot) {
    m_properties.pivot = pivot;
    mark_dirty();
}

Rect HudElementBase::bounds() const {
    return Rect{
        m_properties.position.x,
        m_properties.position.y,
        m_properties.size.x,
        m_properties.size.y
    };
}

Rect HudElementBase::global_bounds() const {
    Rect local = bounds();

    if (m_parent) {
        Rect parent_bounds = m_parent->global_bounds();
        local.x += parent_bounds.x;
        local.y += parent_bounds.y;
    }

    return local;
}

void HudElementBase::set_visible(bool visible) {
    m_properties.visibility = visible ? Visibility::Visible : Visibility::Hidden;
    mark_dirty();
}

bool HudElementBase::is_visible() const {
    if (m_properties.visibility != Visibility::Visible) {
        return false;
    }
    if (m_parent) {
        return m_parent->is_visible();
    }
    return true;
}

void HudElementBase::set_opacity(float opacity) {
    m_properties.opacity = std::clamp(opacity, 0.0f, 1.0f);
    mark_dirty();
}

void HudElementBase::add_child(IHudElement* child) {
    if (!child) return;

    // Remove from old parent
    if (child->parent()) {
        child->parent()->remove_child(child);
    }

    m_children.push_back(child);

    if (auto* base = dynamic_cast<HudElementBase*>(child)) {
        base->m_parent = this;
    }
}

void HudElementBase::remove_child(IHudElement* child) {
    m_children.erase(
        std::remove(m_children.begin(), m_children.end(), child),
        m_children.end());

    if (auto* base = dynamic_cast<HudElementBase*>(child)) {
        base->m_parent = nullptr;
    }
}

IHudElement* HudElementBase::find_child(std::string_view name) const {
    for (auto* child : m_children) {
        if (child->name() == name) {
            return child;
        }
    }
    return nullptr;
}

IHudElement* HudElementBase::find_child_recursive(std::string_view name) const {
    for (auto* child : m_children) {
        if (child->name() == name) {
            return child;
        }
        if (auto* found = child->find_child_recursive(name)) {
            return found;
        }
    }
    return nullptr;
}

void HudElementBase::update(float delta_time) {
    for (auto* child : m_children) {
        if (child->is_visible()) {
            child->update(delta_time);
        }
    }
}

void HudElementBase::render() {
    if (!is_visible()) return;

    render_self();

    for (auto* child : m_children) {
        child->render();
    }

    m_dirty = false;
}

bool HudElementBase::hit_test(const Vec2& point) const {
    if (!is_visible() || !m_properties.interactive) {
        return false;
    }
    return global_bounds().contains(point);
}

void HudElementBase::play_animation(HudAnimationId anim) {
    m_current_animation = anim;
}

void HudElementBase::stop_animation() {
    m_current_animation = HudAnimationId{};
}

// =============================================================================
// HudPanel
// =============================================================================

HudPanel::HudPanel() {
    m_properties.type = HudElementType::Panel;
}

HudPanel::HudPanel(const ElementProperties& props)
    : HudElementBase(props) {
    m_properties.type = HudElementType::Panel;
}

void HudPanel::render_self() {
    // Rendering would be done by graphics backend
    // This is a placeholder for the render call
}

// =============================================================================
// HudText
// =============================================================================

HudText::HudText() {
    m_properties.type = HudElementType::Text;
}

HudText::HudText(const std::string& text) {
    m_properties.type = HudElementType::Text;
    m_text_props.text = text;
}

HudText::HudText(const std::string& text, const TextProperties& props)
    : m_text_props(props) {
    m_properties.type = HudElementType::Text;
    m_text_props.text = text;
}

void HudText::set_text(const std::string& text) {
    m_text_props.text = text;
    mark_dirty();
}

void HudText::set_font(const std::string& font, float size) {
    m_text_props.font_name = font;
    m_text_props.font_size = size;
    mark_dirty();
}

void HudText::set_value(const std::any& value) {
    m_value = value;
    update_formatted_text();
}

void HudText::update_formatted_text() {
    if (m_format.empty() || !m_value.has_value()) {
        return;
    }

    // Simple format replacement
    std::string result = m_format;

    try {
        if (m_value.type() == typeid(int)) {
            auto pos = result.find("{}");
            if (pos != std::string::npos) {
                result.replace(pos, 2, std::to_string(std::any_cast<int>(m_value)));
            }
        } else if (m_value.type() == typeid(float)) {
            auto pos = result.find("{}");
            if (pos != std::string::npos) {
                result.replace(pos, 2, std::to_string(std::any_cast<float>(m_value)));
            }
        } else if (m_value.type() == typeid(std::string)) {
            auto pos = result.find("{}");
            if (pos != std::string::npos) {
                result.replace(pos, 2, std::any_cast<std::string>(m_value));
            }
        }
    } catch (...) {
        // Keep original format on error
    }

    m_text_props.text = result;
    mark_dirty();
}

void HudText::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudProgressBar
// =============================================================================

HudProgressBar::HudProgressBar() {
    m_properties.type = HudElementType::ProgressBar;
}

HudProgressBar::HudProgressBar(const ProgressBarProperties& props)
    : m_props(props) {
    m_properties.type = HudElementType::ProgressBar;
    m_displayed_value = props.value;
    m_target_value = props.value;
}

void HudProgressBar::set_value(float value) {
    m_props.value = std::clamp(value, m_props.min_value, m_props.max_value);
    m_target_value = m_props.value;
    if (!m_props.animate_changes) {
        m_displayed_value = m_target_value;
    }
    mark_dirty();
}

void HudProgressBar::set_range(float min_val, float max_val) {
    m_props.min_value = min_val;
    m_props.max_value = max_val;
    m_props.value = std::clamp(m_props.value, min_val, max_val);
    mark_dirty();
}

float HudProgressBar::normalized_value() const {
    float range = m_props.max_value - m_props.min_value;
    if (range <= 0) return 0;
    return (m_displayed_value - m_props.min_value) / range;
}

void HudProgressBar::set_normalized_value(float value) {
    float range = m_props.max_value - m_props.min_value;
    set_value(m_props.min_value + value * range);
}

void HudProgressBar::set_colors(const Color& fill, const Color& background) {
    m_props.fill_color = fill;
    m_props.background_color = background;
    mark_dirty();
}

void HudProgressBar::set_style(ProgressStyle style, FillDirection direction) {
    m_props.style = style;
    m_props.fill_direction = direction;
    mark_dirty();
}

void HudProgressBar::set_segments(std::uint32_t count, float gap) {
    m_props.segments = count;
    m_props.segment_gap = gap;
    m_props.style = ProgressStyle::Segmented;
    mark_dirty();
}

void HudProgressBar::set_show_text(bool show, const std::string& format) {
    m_props.show_text = show;
    m_props.text_format = format;
    mark_dirty();
}

void HudProgressBar::animate_to(float value, float duration) {
    m_target_value = std::clamp(value, m_props.min_value, m_props.max_value);
    m_animating = true;
}

void HudProgressBar::update(float delta_time) {
    HudElementBase::update(delta_time);

    if (m_props.animate_changes || m_animating) {
        float diff = m_target_value - m_displayed_value;
        if (std::abs(diff) > 0.001f) {
            m_displayed_value += diff * std::min(1.0f, m_props.animation_speed * delta_time);
            mark_dirty();
        } else {
            m_displayed_value = m_target_value;
            m_animating = false;
        }
    }
}

void HudProgressBar::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudIcon
// =============================================================================

HudIcon::HudIcon() {
    m_properties.type = HudElementType::Icon;
}

HudIcon::HudIcon(const std::string& texture_path) {
    m_properties.type = HudElementType::Icon;
    m_props.texture_path = texture_path;
}

HudIcon::HudIcon(const std::string& texture_path, const IconProperties& props)
    : m_props(props) {
    m_properties.type = HudElementType::Icon;
    m_props.texture_path = texture_path;
}

void HudIcon::set_texture(const std::string& path) {
    m_props.texture_path = path;
    mark_dirty();
}

void HudIcon::set_texture_rect(const Rect& rect) {
    m_props.texture_rect = rect;
    mark_dirty();
}

void HudIcon::set_frame(std::uint32_t frame, std::uint32_t columns, std::uint32_t rows) {
    m_columns = columns;
    m_rows = rows;

    float frame_width = 1.0f / columns;
    float frame_height = 1.0f / rows;

    std::uint32_t col = frame % columns;
    std::uint32_t row = frame / columns;

    m_props.texture_rect = Rect{
        col * frame_width,
        row * frame_height,
        frame_width,
        frame_height
    };
    mark_dirty();
}

void HudIcon::animate_frames(float fps, std::uint32_t start_frame, std::uint32_t end_frame, bool loop) {
    m_frame_animating = true;
    m_frame_duration = 1.0f / fps;
    m_frame_timer = 0;
    m_start_frame = start_frame;
    m_end_frame = end_frame;
    m_current_frame = start_frame;
    m_loop_frames = loop;
    set_frame(m_current_frame, m_columns, m_rows);
}

void HudIcon::stop_frame_animation() {
    m_frame_animating = false;
}

void HudIcon::update(float delta_time) {
    HudElementBase::update(delta_time);

    if (m_frame_animating) {
        m_frame_timer += delta_time;
        if (m_frame_timer >= m_frame_duration) {
            m_frame_timer -= m_frame_duration;
            ++m_current_frame;

            if (m_current_frame > m_end_frame) {
                if (m_loop_frames) {
                    m_current_frame = m_start_frame;
                } else {
                    m_current_frame = m_end_frame;
                    m_frame_animating = false;
                }
            }

            set_frame(m_current_frame, m_columns, m_rows);
        }
    }
}

void HudIcon::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudMinimap
// =============================================================================

HudMinimap::HudMinimap() {
    m_properties.type = HudElementType::Minimap;
}

HudMinimap::HudMinimap(float size) {
    m_properties.type = HudElementType::Minimap;
    m_properties.size = {size, size};
}

void HudMinimap::add_marker(const Marker& marker) {
    m_markers.push_back(marker);
    mark_dirty();
}

void HudMinimap::remove_marker(std::uint64_t id) {
    m_markers.erase(
        std::remove_if(m_markers.begin(), m_markers.end(),
                       [id](const Marker& m) { return m.id == id; }),
        m_markers.end());
    mark_dirty();
}

void HudMinimap::clear_markers() {
    m_markers.clear();
    mark_dirty();
}

void HudMinimap::update_marker_position(std::uint64_t id, const Vec2& pos) {
    for (auto& marker : m_markers) {
        if (marker.id == id) {
            marker.position = pos;
            mark_dirty();
            break;
        }
    }
}

Vec2 HudMinimap::world_to_minimap(const Vec2& world_pos) const {
    // Convert world position to minimap space
    float rel_x = (world_pos.x - m_player_pos.x) / m_zoom;
    float rel_y = (world_pos.y - m_player_pos.y) / m_zoom;

    Vec2 center = bounds().center();
    return {center.x + rel_x, center.y + rel_y};
}

void HudMinimap::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudCrosshair
// =============================================================================

HudCrosshair::HudCrosshair() {
    m_properties.type = HudElementType::Crosshair;
}

void HudCrosshair::show_hit_marker(float duration) {
    m_hit_marker_timer = duration;
    m_is_kill_marker = false;
}

void HudCrosshair::show_kill_marker(float duration) {
    m_hit_marker_timer = duration;
    m_is_kill_marker = true;
}

void HudCrosshair::update(float delta_time) {
    HudElementBase::update(delta_time);

    if (m_hit_marker_timer > 0) {
        m_hit_marker_timer -= delta_time;
        if (m_hit_marker_timer <= 0) {
            m_hit_marker_timer = 0;
            m_is_kill_marker = false;
        }
        mark_dirty();
    }
}

void HudCrosshair::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudCompass
// =============================================================================

HudCompass::HudCompass() {
    m_properties.type = HudElementType::Custom;
}

void HudCompass::add_marker(const CompassMarker& marker) {
    m_markers.push_back(marker);
    mark_dirty();
}

void HudCompass::remove_marker(std::uint64_t id) {
    m_markers.erase(
        std::remove_if(m_markers.begin(), m_markers.end(),
                       [id](const CompassMarker& m) { return m.id == id; }),
        m_markers.end());
    mark_dirty();
}

void HudCompass::clear_markers() {
    m_markers.clear();
    mark_dirty();
}

void HudCompass::update_marker_heading(std::uint64_t id, float heading) {
    for (auto& marker : m_markers) {
        if (marker.id == id) {
            marker.heading = heading;
            mark_dirty();
            break;
        }
    }
}

void HudCompass::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudObjectiveMarker
// =============================================================================

HudObjectiveMarker::HudObjectiveMarker() {
    m_properties.type = HudElementType::ObjectiveMarker;
}

HudObjectiveMarker::HudObjectiveMarker(const ObjectiveMarkerDef& def)
    : m_def(def) {
    m_properties.type = HudElementType::ObjectiveMarker;
}

void HudObjectiveMarker::update_screen_position(const Vec2& screen_pos, bool on_screen) {
    m_screen_pos = screen_pos;
    m_on_screen = on_screen;

    if (!on_screen && m_def.clamp_to_screen) {
        // Calculate edge angle for off-screen indicator
        Vec2 center{m_properties.size.x / 2, m_properties.size.y / 2};
        m_edge_angle = std::atan2(screen_pos.y - center.y, screen_pos.x - center.x);
    }

    mark_dirty();
}

void HudObjectiveMarker::update(float delta_time) {
    HudElementBase::update(delta_time);
}

void HudObjectiveMarker::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudDamageIndicator
// =============================================================================

HudDamageIndicator::HudDamageIndicator() {
    m_properties.type = HudElementType::DamageIndicator;
}

void HudDamageIndicator::add_damage(const DamageIndicatorDef& def) {
    // Remove oldest if at max
    while (m_indicators.size() >= m_max_indicators) {
        m_indicators.erase(m_indicators.begin());
    }

    ActiveIndicator indicator;
    indicator.def = def;
    indicator.time_remaining = def.duration;
    indicator.current_opacity = 1.0f;
    m_indicators.push_back(indicator);

    mark_dirty();
}

void HudDamageIndicator::add_damage(float direction, float intensity, bool critical) {
    DamageIndicatorDef def;
    def.direction = direction;
    def.intensity = intensity;
    def.is_critical = critical;
    def.color = critical ? Color::yellow() : Color::red();
    def.duration = m_fade_duration;
    add_damage(def);
}

void HudDamageIndicator::update(float delta_time) {
    HudElementBase::update(delta_time);

    bool changed = false;
    for (auto it = m_indicators.begin(); it != m_indicators.end();) {
        it->time_remaining -= delta_time;
        it->current_opacity = std::max(0.0f, it->time_remaining / it->def.duration);

        if (it->time_remaining <= 0) {
            it = m_indicators.erase(it);
            changed = true;
        } else {
            ++it;
            changed = true;
        }
    }

    if (changed) {
        mark_dirty();
    }
}

void HudDamageIndicator::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudNotification
// =============================================================================

HudNotification::HudNotification() {
    m_properties.type = HudElementType::Notification;
}

HudNotification::HudNotification(const NotificationDef& def)
    : m_def(def) {
    m_properties.type = HudElementType::Notification;
    m_time_remaining = def.duration;
}

void HudNotification::set_definition(const NotificationDef& def) {
    m_def = def;
    m_time_remaining = def.duration;
    m_dismissed = false;
    mark_dirty();
}

void HudNotification::dismiss() {
    m_dismissed = true;
    m_time_remaining = 0;
}

void HudNotification::on_click() {
    if (m_def.on_click) {
        m_def.on_click();
    }
    if (m_def.dismissable) {
        dismiss();
    }
}

void HudNotification::update(float delta_time) {
    HudElementBase::update(delta_time);

    if (m_def.duration > 0 && !m_dismissed) {
        m_time_remaining -= delta_time;
        if (m_time_remaining <= 0) {
            m_time_remaining = 0;
        }
        mark_dirty();
    }
}

void HudNotification::render_self() {
    // Rendering would be done by graphics backend
}

// =============================================================================
// HudTooltip
// =============================================================================

HudTooltip::HudTooltip() {
    m_properties.type = HudElementType::Tooltip;
    m_properties.visibility = Visibility::Hidden;
}

void HudTooltip::add_stat(const std::string& label, const std::string& value, const Color& color) {
    m_stats.push_back({label, value, color});
    mark_dirty();
}

void HudTooltip::clear_stats() {
    m_stats.clear();
    mark_dirty();
}

void HudTooltip::show_at(const Vec2& position) {
    m_properties.position = position;
    m_show_timer = 0;
    m_showing = true;
    mark_dirty();
}

void HudTooltip::hide() {
    m_showing = false;
    m_properties.visibility = Visibility::Hidden;
    mark_dirty();
}

void HudTooltip::update(float delta_time) {
    HudElementBase::update(delta_time);

    if (m_showing) {
        m_show_timer += delta_time;
        if (m_show_timer >= m_delay) {
            m_properties.visibility = Visibility::Visible;
            mark_dirty();
        }
    }
}

void HudTooltip::render_self() {
    // Rendering would be done by graphics backend
}

} // namespace void_hud
