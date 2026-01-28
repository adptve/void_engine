/// @file widget_api.hpp
/// @brief IWidgetAPI interface for widget plugins

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "state_stores.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace void_gamestate {
class GameStateCore;
}

namespace void_widget {

// =============================================================================
// IWidgetAPI Interface
// =============================================================================

/// @brief Interface provided to widget plugins for state access and modification
class IWidgetAPI {
public:
    virtual ~IWidgetAPI() = default;

    // =========================================================================
    // Read-Only State Access
    // =========================================================================

    /// Get widget registry (all widget instances and hierarchy)
    [[nodiscard]] virtual const WidgetRegistry& registry() const = 0;

    /// Get layout state
    [[nodiscard]] virtual const LayoutState& layout() const = 0;

    /// Get style state
    [[nodiscard]] virtual const StyleState& style() const = 0;

    /// Get interaction state
    [[nodiscard]] virtual const InteractionState& interaction() const = 0;

    /// Get animation state
    [[nodiscard]] virtual const AnimationState& animation() const = 0;

    /// Get binding state
    [[nodiscard]] virtual const BindingState& bindings() const = 0;

    // =========================================================================
    // Widget Queries
    // =========================================================================

    /// Get widget instance by ID
    [[nodiscard]] virtual const WidgetInstance* get_widget(WidgetId id) const = 0;

    /// Find widget by name
    [[nodiscard]] virtual WidgetId find_widget(std::string_view name) const = 0;

    /// Find all widgets of a type
    [[nodiscard]] virtual std::vector<WidgetId> find_widgets_by_type(std::string_view type) const = 0;

    /// Get children of a widget
    [[nodiscard]] virtual std::vector<WidgetId> get_children(WidgetId parent) const = 0;

    /// Get parent of a widget
    [[nodiscard]] virtual WidgetId get_parent(WidgetId child) const = 0;

    /// Get computed bounds of a widget
    [[nodiscard]] virtual Rect get_bounds(WidgetId id) const = 0;

    /// Get computed style of a widget
    [[nodiscard]] virtual ComputedStyle get_computed_style(WidgetId id) const = 0;

    // =========================================================================
    // Interaction Queries
    // =========================================================================

    /// Check if widget is hovered
    [[nodiscard]] virtual bool is_hovered(WidgetId id) const = 0;

    /// Check if widget is pressed
    [[nodiscard]] virtual bool is_pressed(WidgetId id) const = 0;

    /// Check if widget is focused
    [[nodiscard]] virtual bool is_focused(WidgetId id) const = 0;

    /// Check if widget is visible
    [[nodiscard]] virtual bool is_visible(WidgetId id) const = 0;

    /// Hit test a point against a widget
    [[nodiscard]] virtual bool hit_test(WidgetId id, Vec2 point) const = 0;

    // =========================================================================
    // State Modification (Command Pattern)
    // =========================================================================

    /// Submit a command for execution
    virtual void submit_command(std::unique_ptr<IWidgetCommand> cmd) = 0;

    // =========================================================================
    // Widget Lifecycle (Convenience Methods)
    // =========================================================================

    /// Create a new widget
    virtual WidgetId create_widget(std::string_view type, std::string_view name = "") = 0;

    /// Create widget from template
    virtual WidgetId create_from_template(std::string_view template_name, std::string_view name = "") = 0;

    /// Destroy a widget
    virtual void destroy_widget(WidgetId id) = 0;

    /// Set widget parent
    virtual void set_parent(WidgetId child, WidgetId parent) = 0;

    /// Set widget layer
    virtual void set_layer(WidgetId id, LayerId layer) = 0;

    // =========================================================================
    // Layout (Convenience Methods)
    // =========================================================================

    /// Set widget position
    virtual void set_position(WidgetId id, Vec2 pos) = 0;

    /// Set widget size
    virtual void set_size(WidgetId id, Vec2 size) = 0;

    /// Set widget anchor
    virtual void set_anchor(WidgetId id, Vec2 anchor) = 0;

    /// Set widget anchor (from enum)
    virtual void set_anchor(WidgetId id, Anchor anchor) = 0;

    /// Set widget pivot
    virtual void set_pivot(WidgetId id, Vec2 pivot) = 0;

    /// Set widget margins
    virtual void set_margin(WidgetId id, Insets margin) = 0;

    /// Set widget padding
    virtual void set_padding(WidgetId id, Insets padding) = 0;

    /// Set widget visibility
    virtual void set_visible(WidgetId id, bool visible) = 0;

    /// Set widget rotation
    virtual void set_rotation(WidgetId id, float degrees) = 0;

    /// Set widget scale
    virtual void set_scale(WidgetId id, Vec2 scale) = 0;

    // =========================================================================
    // Style (Convenience Methods)
    // =========================================================================

    /// Set a style property
    virtual void set_style(WidgetId id, std::string_view property, const std::any& value) = 0;

    /// Set background color
    virtual void set_background_color(WidgetId id, const Color& color) = 0;

    /// Set border color
    virtual void set_border_color(WidgetId id, const Color& color) = 0;

    /// Set text color
    virtual void set_text_color(WidgetId id, const Color& color) = 0;

    /// Set opacity
    virtual void set_opacity(WidgetId id, float opacity) = 0;

    /// Apply a theme by name
    virtual void apply_theme(std::string_view theme_name) = 0;

    // =========================================================================
    // Animation (Convenience Methods)
    // =========================================================================

    /// Play a named animation
    virtual AnimationId play_animation(WidgetId id, std::string_view anim_name) = 0;

    /// Stop an animation
    virtual void stop_animation(WidgetId id, AnimationId anim) = 0;

    /// Stop all animations on a widget
    virtual void stop_all_animations(WidgetId id) = 0;

    /// Animate a property to a target value
    virtual AnimationId animate_property(WidgetId id, std::string_view property,
                                         const std::any& target, float duration,
                                         EasingType easing = EasingType::EaseOutQuad) = 0;

    /// Quick fade in
    virtual AnimationId fade_in(WidgetId id, float duration = 0.2f) = 0;

    /// Quick fade out
    virtual AnimationId fade_out(WidgetId id, float duration = 0.2f) = 0;

    /// Quick slide in
    virtual AnimationId slide_in(WidgetId id, Vec2 from, float duration = 0.3f) = 0;

    /// Quick slide out
    virtual AnimationId slide_out(WidgetId id, Vec2 to, float duration = 0.3f) = 0;

    // =========================================================================
    // Data Binding (Convenience Methods)
    // =========================================================================

    /// Bind a widget property to a data source
    virtual BindingId bind(WidgetId id, std::string_view property,
                           std::string_view source_path,
                           BindingMode mode = BindingMode::OneWay) = 0;

    /// Unbind a binding
    virtual void unbind(BindingId binding) = 0;

    /// Unbind all bindings on a widget
    virtual void unbind_all(WidgetId id) = 0;

    // =========================================================================
    // Widget Properties
    // =========================================================================

    /// Set widget text content
    virtual void set_text(WidgetId id, std::string_view text) = 0;

    /// Get widget text content
    [[nodiscard]] virtual std::string get_text(WidgetId id) const = 0;

    /// Set widget value (for sliders, progress bars, etc.)
    virtual void set_value(WidgetId id, float value) = 0;

    /// Get widget value
    [[nodiscard]] virtual float get_value(WidgetId id) const = 0;

    /// Set widget checked state
    virtual void set_checked(WidgetId id, bool checked) = 0;

    /// Get widget checked state
    [[nodiscard]] virtual bool is_checked(WidgetId id) const = 0;

    /// Set widget enabled state
    virtual void set_enabled(WidgetId id, bool enabled) = 0;

    /// Get widget enabled state
    [[nodiscard]] virtual bool is_enabled(WidgetId id) const = 0;

    /// Set custom property
    virtual void set_property(WidgetId id, std::string_view key, const std::any& value) = 0;

    /// Get custom property
    [[nodiscard]] virtual std::any get_property(WidgetId id, std::string_view key) const = 0;

    // =========================================================================
    // Drawing API (for custom widget rendering)
    // =========================================================================

    /// Draw a filled rectangle
    virtual void draw_rect(const Rect& rect, const Color& color) = 0;

    /// Draw a rectangle outline
    virtual void draw_rect_outline(const Rect& rect, const Color& color, float width = 1) = 0;

    /// Draw a rounded rectangle
    virtual void draw_rounded_rect(const Rect& rect, const Color& color, float radius) = 0;

    /// Draw a rounded rectangle outline
    virtual void draw_rounded_rect_outline(const Rect& rect, const Color& color,
                                            float radius, float width = 1) = 0;

    /// Draw text
    virtual void draw_text(const std::string& text, Vec2 pos, const Color& color,
                           float size = 16, std::string_view font = "default") = 0;

    /// Draw text with alignment
    virtual void draw_text_aligned(const std::string& text, const Rect& rect,
                                    TextAlign h_align, VerticalAlign v_align,
                                    const Color& color, float size = 16) = 0;

    /// Draw an icon
    virtual void draw_icon(std::string_view icon, Vec2 pos, Vec2 size,
                           const Color& tint = Color::white()) = 0;

    /// Draw a line
    virtual void draw_line(Vec2 from, Vec2 to, const Color& color, float width = 1) = 0;

    /// Draw a texture
    virtual void draw_texture(std::string_view texture, const Rect& dest,
                              const Rect& src = {}, const Color& tint = Color::white()) = 0;

    /// Draw a circle
    virtual void draw_circle(Vec2 center, float radius, const Color& color) = 0;

    /// Draw a circle outline
    virtual void draw_circle_outline(Vec2 center, float radius, const Color& color, float width = 1) = 0;

    /// Push scissor rectangle (clip future drawing)
    virtual void push_scissor(const Rect& rect) = 0;

    /// Pop scissor rectangle
    virtual void pop_scissor() = 0;

    // =========================================================================
    // Input State
    // =========================================================================

    /// Get mouse position
    [[nodiscard]] virtual Vec2 mouse_position() const = 0;

    /// Get mouse delta since last frame
    [[nodiscard]] virtual Vec2 mouse_delta() const = 0;

    /// Check if mouse button is down
    [[nodiscard]] virtual bool is_mouse_down(int button = 0) const = 0;

    /// Check if mouse button was just pressed
    [[nodiscard]] virtual bool is_mouse_pressed(int button = 0) const = 0;

    /// Check if mouse button was just released
    [[nodiscard]] virtual bool is_mouse_released(int button = 0) const = 0;

    /// Check if key is down
    [[nodiscard]] virtual bool is_key_down(int key) const = 0;

    /// Check if key was just pressed
    [[nodiscard]] virtual bool is_key_pressed(int key) const = 0;

    /// Get text input this frame
    [[nodiscard]] virtual const std::string& text_input() const = 0;

    // =========================================================================
    // Focus Management
    // =========================================================================

    /// Set focused widget
    virtual void set_focus(WidgetId id) = 0;

    /// Clear focus
    virtual void clear_focus() = 0;

    /// Move focus to next widget
    virtual void focus_next() = 0;

    /// Move focus to previous widget
    virtual void focus_prev() = 0;

    // =========================================================================
    // Event Subscription
    // =========================================================================

    /// Subscribe to widget events
    virtual void subscribe(WidgetId id, WidgetEventType event, WidgetEventCallback callback) = 0;

    /// Unsubscribe from widget events
    virtual void unsubscribe(WidgetId id, WidgetEventType event) = 0;

    // =========================================================================
    // Layer Management
    // =========================================================================

    /// Create a new layer
    virtual LayerId create_layer(std::string_view name, int z_order = 0) = 0;

    /// Destroy a layer
    virtual void destroy_layer(LayerId id) = 0;

    /// Set layer visibility
    virtual void set_layer_visible(LayerId id, bool visible) = 0;

    /// Set layer opacity
    virtual void set_layer_opacity(LayerId id, float opacity) = 0;

    // =========================================================================
    // Engine Services
    // =========================================================================

    /// Get delta time
    [[nodiscard]] virtual float delta_time() const = 0;

    /// Get current time
    [[nodiscard]] virtual double current_time() const = 0;

    /// Get screen size
    [[nodiscard]] virtual Vec2 screen_size() const = 0;

    /// Get UI scale factor
    [[nodiscard]] virtual float ui_scale() const = 0;

    /// Access to GameStateCore for data binding
    [[nodiscard]] virtual const void_gamestate::GameStateCore* game_state() const = 0;
};

// =============================================================================
// Widget Command Interface
// =============================================================================

/// @brief Base interface for widget state commands
class IWidgetCommand {
public:
    virtual ~IWidgetCommand() = default;

    /// Execute the command
    virtual void execute(WidgetStateCore& state) = 0;

    /// Get command name for debugging
    [[nodiscard]] virtual const char* name() const = 0;
};

/// @brief Command result
struct WidgetCommandResult {
    bool success{true};
    std::string error;

    static WidgetCommandResult ok() { return {true, ""}; }
    static WidgetCommandResult fail(std::string_view msg) { return {false, std::string(msg)}; }
};

} // namespace void_widget
