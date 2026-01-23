/// @file elements.hpp
/// @brief HUD element classes for void_hud module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_hud {

// =============================================================================
// IHudElement Interface
// =============================================================================

/// @brief Base interface for all HUD elements
class IHudElement {
public:
    virtual ~IHudElement() = default;

    // Identity
    virtual HudElementId id() const = 0;
    virtual HudElementType type() const = 0;
    virtual const std::string& name() const = 0;

    // Properties
    virtual const ElementProperties& properties() const = 0;
    virtual ElementProperties& properties_mut() = 0;

    // Transform
    virtual void set_position(const Vec2& pos) = 0;
    virtual void set_size(const Vec2& size) = 0;
    virtual void set_rotation(float rotation) = 0;
    virtual void set_scale(const Vec2& scale) = 0;
    virtual void set_pivot(const Vec2& pivot) = 0;

    virtual Vec2 position() const = 0;
    virtual Vec2 size() const = 0;
    virtual float rotation() const = 0;
    virtual Vec2 scale() const = 0;
    virtual Rect bounds() const = 0;
    virtual Rect global_bounds() const = 0;

    // Visibility
    virtual void set_visible(bool visible) = 0;
    virtual bool is_visible() const = 0;
    virtual void set_opacity(float opacity) = 0;
    virtual float opacity() const = 0;

    // Hierarchy
    virtual IHudElement* parent() const = 0;
    virtual const std::vector<IHudElement*>& children() const = 0;
    virtual void add_child(IHudElement* child) = 0;
    virtual void remove_child(IHudElement* child) = 0;
    virtual IHudElement* find_child(std::string_view name) const = 0;
    virtual IHudElement* find_child_recursive(std::string_view name) const = 0;

    // Update/Render
    virtual void update(float delta_time) = 0;
    virtual void render() = 0;

    // Interaction
    virtual bool hit_test(const Vec2& point) const = 0;
    virtual void on_pointer_enter() {}
    virtual void on_pointer_exit() {}
    virtual void on_pointer_down(const Vec2& point) {}
    virtual void on_pointer_up(const Vec2& point) {}
    virtual void on_click() {}

    // Animation
    virtual void play_animation(HudAnimationId anim) = 0;
    virtual void stop_animation() = 0;
    virtual bool is_animating() const = 0;

    // State
    virtual void set_enabled(bool enabled) = 0;
    virtual bool is_enabled() const = 0;
    virtual void mark_dirty() = 0;
    virtual bool is_dirty() const = 0;
};

// =============================================================================
// HudElementBase
// =============================================================================

/// @brief Base implementation for HUD elements
class HudElementBase : public IHudElement {
public:
    HudElementBase();
    explicit HudElementBase(const ElementProperties& props);
    ~HudElementBase() override;

    // Identity
    HudElementId id() const override { return m_id; }
    HudElementType type() const override { return m_properties.type; }
    const std::string& name() const override { return m_properties.name; }

    // Properties
    const ElementProperties& properties() const override { return m_properties; }
    ElementProperties& properties_mut() override { mark_dirty(); return m_properties; }

    // Transform
    void set_position(const Vec2& pos) override;
    void set_size(const Vec2& size) override;
    void set_rotation(float rotation) override;
    void set_scale(const Vec2& scale) override;
    void set_pivot(const Vec2& pivot) override;

    Vec2 position() const override { return m_properties.position; }
    Vec2 size() const override { return m_properties.size; }
    float rotation() const override { return m_properties.rotation; }
    Vec2 scale() const override { return m_properties.scale; }
    Rect bounds() const override;
    Rect global_bounds() const override;

    // Visibility
    void set_visible(bool visible) override;
    bool is_visible() const override;
    void set_opacity(float opacity) override;
    float opacity() const override { return m_properties.opacity; }

    // Hierarchy
    IHudElement* parent() const override { return m_parent; }
    const std::vector<IHudElement*>& children() const override { return m_children; }
    void add_child(IHudElement* child) override;
    void remove_child(IHudElement* child) override;
    IHudElement* find_child(std::string_view name) const override;
    IHudElement* find_child_recursive(std::string_view name) const override;

    // Update/Render
    void update(float delta_time) override;
    void render() override;

    // Interaction
    bool hit_test(const Vec2& point) const override;

    // Animation
    void play_animation(HudAnimationId anim) override;
    void stop_animation() override;
    bool is_animating() const override { return m_current_animation.value != 0; }

    // State
    void set_enabled(bool enabled) override { m_enabled = enabled; }
    bool is_enabled() const override { return m_enabled; }
    void mark_dirty() override { m_dirty = true; }
    bool is_dirty() const override { return m_dirty; }

    // Internal
    void set_id(HudElementId id) { m_id = id; }
    void set_parent(IHudElement* parent) { m_parent = parent; }

protected:
    virtual void on_property_changed() {}
    virtual void render_self() {}

    HudElementId m_id;
    ElementProperties m_properties;
    IHudElement* m_parent{nullptr};
    std::vector<IHudElement*> m_children;
    HudAnimationId m_current_animation;
    bool m_enabled{true};
    bool m_dirty{true};
};

// =============================================================================
// HudPanel
// =============================================================================

/// @brief Container panel element
class HudPanel : public HudElementBase {
public:
    HudPanel();
    explicit HudPanel(const ElementProperties& props);

    // Background
    void set_background_color(const Color& color) { m_background_color = color; mark_dirty(); }
    const Color& background_color() const { return m_background_color; }

    void set_background_image(const std::string& path) { m_background_image = path; mark_dirty(); }
    const std::string& background_image() const { return m_background_image; }

    // Border
    void set_border_color(const Color& color) { m_border_color = color; mark_dirty(); }
    const Color& border_color() const { return m_border_color; }

    void set_border_width(float width) { m_border_width = width; mark_dirty(); }
    float border_width() const { return m_border_width; }

    void set_corner_radius(float radius) { m_corner_radius = radius; mark_dirty(); }
    float corner_radius() const { return m_corner_radius; }

protected:
    void render_self() override;

private:
    Color m_background_color{Color::transparent()};
    std::string m_background_image;
    Color m_border_color{Color::transparent()};
    float m_border_width{0};
    float m_corner_radius{0};
};

// =============================================================================
// HudText
// =============================================================================

/// @brief Text display element
class HudText : public HudElementBase {
public:
    HudText();
    explicit HudText(const std::string& text);
    HudText(const std::string& text, const TextProperties& props);

    // Text content
    void set_text(const std::string& text);
    const std::string& text() const { return m_text_props.text; }

    // Text properties
    void set_text_properties(const TextProperties& props) { m_text_props = props; mark_dirty(); }
    const TextProperties& text_properties() const { return m_text_props; }
    TextProperties& text_properties_mut() { mark_dirty(); return m_text_props; }

    // Quick setters
    void set_font(const std::string& font, float size);
    void set_color(const Color& color) { m_text_props.text_color = color; mark_dirty(); }
    void set_alignment(TextProperties::Alignment align) { m_text_props.alignment = align; mark_dirty(); }

    // Formatting
    void set_format(const std::string& format) { m_format = format; }
    void set_value(const std::any& value);

protected:
    void render_self() override;

private:
    void update_formatted_text();

    TextProperties m_text_props;
    std::string m_format;
    std::any m_value;
};

// =============================================================================
// HudProgressBar
// =============================================================================

/// @brief Progress/health bar element
class HudProgressBar : public HudElementBase {
public:
    HudProgressBar();
    explicit HudProgressBar(const ProgressBarProperties& props);

    // Value
    void set_value(float value);
    float value() const { return m_props.value; }

    void set_range(float min_val, float max_val);
    float min_value() const { return m_props.min_value; }
    float max_value() const { return m_props.max_value; }

    // Normalized value (0-1)
    float normalized_value() const;
    void set_normalized_value(float value);

    // Properties
    void set_properties(const ProgressBarProperties& props) { m_props = props; mark_dirty(); }
    const ProgressBarProperties& progress_properties() const { return m_props; }

    // Quick setters
    void set_colors(const Color& fill, const Color& background);
    void set_style(ProgressStyle style, FillDirection direction);
    void set_segments(std::uint32_t count, float gap);
    void set_show_text(bool show, const std::string& format = "{0:.0%}");

    // Animation
    void animate_to(float value, float duration);

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    ProgressBarProperties m_props;
    float m_displayed_value{0};
    float m_target_value{0};
    bool m_animating{false};
};

// =============================================================================
// HudIcon
// =============================================================================

/// @brief Icon/image element
class HudIcon : public HudElementBase {
public:
    HudIcon();
    explicit HudIcon(const std::string& texture_path);
    HudIcon(const std::string& texture_path, const IconProperties& props);

    // Texture
    void set_texture(const std::string& path);
    const std::string& texture() const { return m_props.texture_path; }

    // Sprite sheet support
    void set_texture_rect(const Rect& rect);
    const Rect& texture_rect() const { return m_props.texture_rect; }

    // Appearance
    void set_tint(const Color& color) { m_props.tint = color; mark_dirty(); }
    const Color& tint() const { return m_props.tint; }

    void set_preserve_aspect(bool preserve) { m_props.preserve_aspect = preserve; mark_dirty(); }
    bool preserve_aspect() const { return m_props.preserve_aspect; }

    // Animation (for sprite sheets)
    void set_frame(std::uint32_t frame, std::uint32_t columns, std::uint32_t rows);
    void animate_frames(float fps, std::uint32_t start_frame, std::uint32_t end_frame, bool loop = true);
    void stop_frame_animation();

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    IconProperties m_props;

    // Frame animation
    bool m_frame_animating{false};
    float m_frame_timer{0};
    float m_frame_duration{0};
    std::uint32_t m_current_frame{0};
    std::uint32_t m_start_frame{0};
    std::uint32_t m_end_frame{0};
    std::uint32_t m_columns{1};
    std::uint32_t m_rows{1};
    bool m_loop_frames{false};
};

// =============================================================================
// HudMinimap
// =============================================================================

/// @brief Minimap display element
class HudMinimap : public HudElementBase {
public:
    HudMinimap();
    explicit HudMinimap(float size);

    // Map settings
    void set_map_texture(const std::string& path) { m_map_texture = path; mark_dirty(); }
    void set_map_bounds(const Rect& bounds) { m_map_bounds = bounds; }
    void set_zoom(float zoom) { m_zoom = zoom; mark_dirty(); }
    float zoom() const { return m_zoom; }

    // Player
    void set_player_position(const Vec2& pos) { m_player_pos = pos; mark_dirty(); }
    void set_player_rotation(float rotation) { m_player_rotation = rotation; mark_dirty(); }
    void set_player_icon(const std::string& path) { m_player_icon = path; }

    // Markers
    struct Marker {
        std::uint64_t id{0};
        Vec2 position;
        std::string icon_path;
        Color color{Color::white()};
        bool rotate{false};
        float rotation{0};
    };

    void add_marker(const Marker& marker);
    void remove_marker(std::uint64_t id);
    void clear_markers();
    void update_marker_position(std::uint64_t id, const Vec2& pos);

    // Shape
    void set_circular(bool circular) { m_circular = circular; mark_dirty(); }
    bool is_circular() const { return m_circular; }

    void set_rotate_map(bool rotate) { m_rotate_map = rotate; }
    bool rotates_map() const { return m_rotate_map; }

protected:
    void render_self() override;

private:
    Vec2 world_to_minimap(const Vec2& world_pos) const;

    std::string m_map_texture;
    Rect m_map_bounds;
    float m_zoom{1.0f};

    Vec2 m_player_pos;
    float m_player_rotation{0};
    std::string m_player_icon;

    std::vector<Marker> m_markers;
    bool m_circular{true};
    bool m_rotate_map{false};
};

// =============================================================================
// HudCrosshair
// =============================================================================

/// @brief Crosshair display element
class HudCrosshair : public HudElementBase {
public:
    HudCrosshair();

    // Style
    enum class Style : std::uint8_t {
        Dot,
        Cross,
        Circle,
        Chevron,
        Custom
    };

    void set_style(Style style) { m_style = style; mark_dirty(); }
    Style style() const { return m_style; }

    // Properties
    void set_gap(float gap) { m_gap = gap; mark_dirty(); }
    float gap() const { return m_gap; }

    void set_line_length(float length) { m_line_length = length; mark_dirty(); }
    float line_length() const { return m_line_length; }

    void set_line_width(float width) { m_line_width = width; mark_dirty(); }
    float line_width() const { return m_line_width; }

    void set_dot_size(float size) { m_dot_size = size; mark_dirty(); }
    float dot_size() const { return m_dot_size; }

    // Dynamic spread (for shooting games)
    void set_spread(float spread) { m_spread = spread; mark_dirty(); }
    float spread() const { return m_spread; }

    // Hit marker
    void show_hit_marker(float duration = 0.2f);
    void show_kill_marker(float duration = 0.3f);

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    Style m_style{Style::Cross};
    float m_gap{4.0f};
    float m_line_length{8.0f};
    float m_line_width{2.0f};
    float m_dot_size{2.0f};
    float m_spread{0};

    float m_hit_marker_timer{0};
    bool m_is_kill_marker{false};
};

// =============================================================================
// HudCompass
// =============================================================================

/// @brief Compass/heading indicator
class HudCompass : public HudElementBase {
public:
    HudCompass();

    // Heading
    void set_heading(float heading) { m_heading = heading; mark_dirty(); }
    float heading() const { return m_heading; }

    // Appearance
    void set_tick_spacing(float spacing) { m_tick_spacing = spacing; mark_dirty(); }
    void set_show_degrees(bool show) { m_show_degrees = show; mark_dirty(); }
    void set_show_cardinals(bool show) { m_show_cardinals = show; mark_dirty(); }

    // Markers
    struct CompassMarker {
        std::uint64_t id{0};
        float heading{0};
        std::string icon_path;
        std::string label;
        Color color{Color::white()};
    };

    void add_marker(const CompassMarker& marker);
    void remove_marker(std::uint64_t id);
    void clear_markers();
    void update_marker_heading(std::uint64_t id, float heading);

protected:
    void render_self() override;

private:
    float m_heading{0};
    float m_tick_spacing{15.0f};
    bool m_show_degrees{true};
    bool m_show_cardinals{true};
    std::vector<CompassMarker> m_markers;
};

// =============================================================================
// HudObjectiveMarker
// =============================================================================

/// @brief Objective/waypoint marker
class HudObjectiveMarker : public HudElementBase {
public:
    HudObjectiveMarker();
    explicit HudObjectiveMarker(const ObjectiveMarkerDef& def);

    // Definition
    void set_definition(const ObjectiveMarkerDef& def) { m_def = def; mark_dirty(); }
    const ObjectiveMarkerDef& definition() const { return m_def; }

    // World position
    void set_world_position(const Vec3& pos) { m_def.world_position = pos; mark_dirty(); }
    const Vec3& world_position() const { return m_def.world_position; }

    // Distance
    void set_distance(float distance) { m_def.distance = distance; mark_dirty(); }
    float distance() const { return m_def.distance; }

    // Screen position (calculated from world position)
    void update_screen_position(const Vec2& screen_pos, bool on_screen);

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    ObjectiveMarkerDef m_def;
    Vec2 m_screen_pos;
    bool m_on_screen{true};
    float m_edge_angle{0};
};

// =============================================================================
// HudDamageIndicator
// =============================================================================

/// @brief Damage direction indicator
class HudDamageIndicator : public HudElementBase {
public:
    HudDamageIndicator();

    // Add damage indicator
    void add_damage(const DamageIndicatorDef& def);
    void add_damage(float direction, float intensity, bool critical = false);

    // Settings
    void set_fade_duration(float duration) { m_fade_duration = duration; }
    void set_max_indicators(std::uint32_t max) { m_max_indicators = max; }

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    struct ActiveIndicator {
        DamageIndicatorDef def;
        float time_remaining{0};
        float current_opacity{1.0f};
    };

    std::vector<ActiveIndicator> m_indicators;
    float m_fade_duration{0.5f};
    std::uint32_t m_max_indicators{8};
};

// =============================================================================
// HudNotification
// =============================================================================

/// @brief Notification popup element
class HudNotification : public HudElementBase {
public:
    HudNotification();
    explicit HudNotification(const NotificationDef& def);

    // Definition
    void set_definition(const NotificationDef& def);
    const NotificationDef& definition() const { return m_def; }

    // State
    bool is_expired() const { return m_time_remaining <= 0 && m_def.duration > 0; }
    float time_remaining() const { return m_time_remaining; }

    // Dismiss
    void dismiss();

    // Interaction
    void on_click() override;

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    NotificationDef m_def;
    float m_time_remaining{0};
    bool m_dismissed{false};
};

// =============================================================================
// HudTooltip
// =============================================================================

/// @brief Tooltip popup element
class HudTooltip : public HudElementBase {
public:
    HudTooltip();

    // Content
    void set_title(const std::string& title) { m_title = title; mark_dirty(); }
    void set_description(const std::string& desc) { m_description = desc; mark_dirty(); }
    void set_icon(const std::string& path) { m_icon_path = path; mark_dirty(); }

    // Rich content
    void add_stat(const std::string& label, const std::string& value, const Color& color = Color::white());
    void clear_stats();

    // Appearance
    void set_max_width(float width) { m_max_width = width; mark_dirty(); }

    // Show/Hide
    void show_at(const Vec2& position);
    void hide();
    bool is_showing() const { return m_showing; }

    void update(float delta_time) override;

protected:
    void render_self() override;

private:
    struct Stat {
        std::string label;
        std::string value;
        Color color;
    };

    std::string m_title;
    std::string m_description;
    std::string m_icon_path;
    std::vector<Stat> m_stats;
    float m_max_width{300.0f};
    bool m_showing{false};
    float m_show_timer{0};
    float m_delay{0.5f};
};

} // namespace void_hud
