/// @file state_stores.hpp
/// @brief State stores for the widget system
///
/// These stores hold ALL persistent widget state. They are owned by
/// WidgetStateCore and persist across widget plugin hot-reloads.

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_widget {

// =============================================================================
// Widget Registry
// =============================================================================

/// @brief Registry of all widget instances and hierarchy
struct WidgetRegistry {
    // All widget instances
    std::unordered_map<WidgetId, WidgetInstance> widgets;

    // Widget hierarchy
    std::unordered_map<WidgetId, std::vector<WidgetId>> children;
    std::unordered_map<WidgetId, WidgetId> parent;

    // Root widgets (no parent)
    std::vector<WidgetId> roots;

    // Named widget lookup
    std::unordered_map<std::string, WidgetId> named_widgets;

    // Widget templates for instantiation
    std::unordered_map<std::string, WidgetTemplate> templates;

    // Layer management
    std::vector<WidgetLayer> layers;
    std::unordered_map<WidgetId, LayerId> widget_layer;

    // ID generation
    std::uint64_t next_widget_id{1};
    std::uint64_t next_layer_id{1};

    // -------------------------------------------------------------------------
    // Query Methods
    // -------------------------------------------------------------------------

    [[nodiscard]] const WidgetInstance* get(WidgetId id) const {
        auto it = widgets.find(id);
        return it != widgets.end() ? &it->second : nullptr;
    }

    [[nodiscard]] WidgetInstance* get_mut(WidgetId id) {
        auto it = widgets.find(id);
        return it != widgets.end() ? &it->second : nullptr;
    }

    [[nodiscard]] WidgetId find_by_name(std::string_view name) const {
        auto it = named_widgets.find(std::string(name));
        return it != named_widgets.end() ? it->second : WidgetId{0};
    }

    [[nodiscard]] std::vector<WidgetId> find_by_type(std::string_view type) const {
        std::vector<WidgetId> result;
        for (const auto& [id, widget] : widgets) {
            if (widget.type == type) {
                result.push_back(id);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<WidgetId> get_children(WidgetId id) const {
        auto it = children.find(id);
        return it != children.end() ? it->second : std::vector<WidgetId>{};
    }

    [[nodiscard]] WidgetId get_parent(WidgetId id) const {
        auto it = parent.find(id);
        return it != parent.end() ? it->second : WidgetId{0};
    }

    [[nodiscard]] bool exists(WidgetId id) const {
        return widgets.find(id) != widgets.end();
    }

    // -------------------------------------------------------------------------
    // Hierarchy Traversal
    // -------------------------------------------------------------------------

    /// Visit all widgets in depth-first order
    template<typename Func>
    void traverse_depth_first(Func&& func) const {
        for (WidgetId root : roots) {
            traverse_depth_first_impl(root, func);
        }
    }

    /// Visit all descendants of a widget
    template<typename Func>
    void traverse_descendants(WidgetId id, Func&& func) const {
        auto it = children.find(id);
        if (it != children.end()) {
            for (WidgetId child : it->second) {
                func(child);
                traverse_descendants(child, func);
            }
        }
    }

private:
    template<typename Func>
    void traverse_depth_first_impl(WidgetId id, Func&& func) const {
        func(id);
        auto it = children.find(id);
        if (it != children.end()) {
            for (WidgetId child : it->second) {
                traverse_depth_first_impl(child, func);
            }
        }
    }
};

// =============================================================================
// Layout State
// =============================================================================

/// @brief Layout state for all widgets
struct LayoutState {
    // Per-widget layout data
    std::unordered_map<WidgetId, LayoutData> layouts;

    // Computed bounds (after layout pass)
    std::unordered_map<WidgetId, Rect> computed_bounds;

    // Layout constraints
    std::unordered_map<WidgetId, LayoutConstraints> constraints;

    // Dirty tracking for layout recalculation
    std::unordered_set<WidgetId> dirty_widgets;

    // Screen metrics
    float screen_width{1920};
    float screen_height{1080};
    float ui_scale{1.0f};
    float safe_area_left{0};
    float safe_area_top{0};
    float safe_area_right{0};
    float safe_area_bottom{0};

    // -------------------------------------------------------------------------
    // Query Methods
    // -------------------------------------------------------------------------

    [[nodiscard]] const LayoutData* get(WidgetId id) const {
        auto it = layouts.find(id);
        return it != layouts.end() ? &it->second : nullptr;
    }

    [[nodiscard]] LayoutData* get_mut(WidgetId id) {
        auto it = layouts.find(id);
        return it != layouts.end() ? &it->second : nullptr;
    }

    [[nodiscard]] Rect get_bounds(WidgetId id) const {
        auto it = computed_bounds.find(id);
        return it != computed_bounds.end() ? it->second : Rect{};
    }

    [[nodiscard]] Vec2 screen_size() const {
        return {screen_width, screen_height};
    }

    [[nodiscard]] Rect safe_area() const {
        return {
            safe_area_left,
            safe_area_top,
            screen_width - safe_area_left - safe_area_right,
            screen_height - safe_area_top - safe_area_bottom
        };
    }

    // -------------------------------------------------------------------------
    // Dirty Tracking
    // -------------------------------------------------------------------------

    void mark_dirty(WidgetId id) {
        dirty_widgets.insert(id);
    }

    void clear_dirty() {
        dirty_widgets.clear();
    }

    [[nodiscard]] bool is_dirty(WidgetId id) const {
        return dirty_widgets.find(id) != dirty_widgets.end();
    }

    [[nodiscard]] bool has_dirty() const {
        return !dirty_widgets.empty();
    }
};

// =============================================================================
// Style State
// =============================================================================

/// @brief Style state for all widgets
struct StyleState {
    // Global themes
    std::unordered_map<std::string, Theme> themes;
    std::string active_theme{"dark"};

    // Per-widget style overrides
    std::unordered_map<WidgetId, StyleOverrides> overrides;

    // Computed styles (theme + overrides)
    std::unordered_map<WidgetId, ComputedStyle> computed_styles;

    // Font registry
    std::unordered_map<std::string, FontData> fonts;
    std::string default_font{"default"};

    // Icon/texture registry
    std::unordered_map<std::string, TextureRegion> icons;
    std::unordered_map<std::string, std::string> textures;  // name -> path

    // -------------------------------------------------------------------------
    // Query Methods
    // -------------------------------------------------------------------------

    [[nodiscard]] const Theme* current_theme() const {
        auto it = themes.find(active_theme);
        return it != themes.end() ? &it->second : nullptr;
    }

    [[nodiscard]] Theme* current_theme_mut() {
        auto it = themes.find(active_theme);
        return it != themes.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const ComputedStyle* get_computed(WidgetId id) const {
        auto it = computed_styles.find(id);
        return it != computed_styles.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const StyleOverrides* get_overrides(WidgetId id) const {
        auto it = overrides.find(id);
        return it != overrides.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const FontData* get_font(std::string_view name) const {
        auto it = fonts.find(std::string(name));
        return it != fonts.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const TextureRegion* get_icon(std::string_view name) const {
        auto it = icons.find(std::string(name));
        return it != icons.end() ? &it->second : nullptr;
    }
};

// =============================================================================
// Interaction State
// =============================================================================

/// @brief Interaction state for input handling
struct InteractionState {
    // Current interaction states
    WidgetId hovered_widget{0};
    WidgetId pressed_widget{0};
    WidgetId focused_widget{0};
    WidgetId dragging_widget{0};

    // Hot widget (about to receive input)
    WidgetId hot_widget{0};
    WidgetId active_widget{0};

    // Input state
    Vec2 mouse_position{0, 0};
    Vec2 mouse_position_prev{0, 0};
    Vec2 mouse_delta{0, 0};
    std::array<bool, 8> mouse_buttons{};
    std::array<bool, 8> mouse_buttons_prev{};
    float scroll_delta{0};

    // Keyboard state
    std::array<bool, 512> keys{};
    std::array<bool, 512> keys_prev{};
    std::uint32_t modifiers{0};

    // Focus chain for tab navigation
    std::vector<WidgetId> focus_chain;
    std::size_t focus_index{0};

    // Drag state
    Vec2 drag_start{0, 0};
    Vec2 drag_offset{0, 0};
    bool drag_active{false};

    // Click tracking
    WidgetId last_clicked_widget{0};
    double last_click_time{0};
    int click_count{0};
    double double_click_time{0.3};  // Max time between clicks

    // Text input
    std::string text_input_buffer;
    int cursor_position{0};
    int selection_start{0};
    int selection_end{0};

    // -------------------------------------------------------------------------
    // Query Methods
    // -------------------------------------------------------------------------

    [[nodiscard]] bool is_hovered(WidgetId id) const { return hovered_widget == id; }
    [[nodiscard]] bool is_pressed(WidgetId id) const { return pressed_widget == id; }
    [[nodiscard]] bool is_focused(WidgetId id) const { return focused_widget == id; }
    [[nodiscard]] bool is_dragging(WidgetId id) const { return dragging_widget == id; }

    [[nodiscard]] bool is_mouse_down(int button = 0) const {
        return button >= 0 && button < 8 && mouse_buttons[button];
    }

    [[nodiscard]] bool is_mouse_pressed(int button = 0) const {
        return button >= 0 && button < 8 && mouse_buttons[button] && !mouse_buttons_prev[button];
    }

    [[nodiscard]] bool is_mouse_released(int button = 0) const {
        return button >= 0 && button < 8 && !mouse_buttons[button] && mouse_buttons_prev[button];
    }

    [[nodiscard]] bool is_key_down(int key) const {
        return key >= 0 && key < 512 && keys[key];
    }

    [[nodiscard]] bool is_key_pressed(int key) const {
        return key >= 0 && key < 512 && keys[key] && !keys_prev[key];
    }

    [[nodiscard]] bool is_key_released(int key) const {
        return key >= 0 && key < 512 && !keys[key] && keys_prev[key];
    }

    [[nodiscard]] bool has_text_selection() const {
        return selection_start != selection_end;
    }
};

// =============================================================================
// Animation State
// =============================================================================

/// @brief Animation state for all widgets
struct AnimationState {
    // Active animations per widget
    std::unordered_map<WidgetId, std::vector<ActiveAnimation>> animations;

    // Animation definitions
    std::unordered_map<std::string, AnimationDef> definitions;

    // Style transitions
    std::unordered_map<WidgetId, std::vector<StyleTransition>> transitions;

    // Global animation time
    double current_time{0};

    // Animation ID generation
    std::uint64_t next_animation_id{1};

    // -------------------------------------------------------------------------
    // Query Methods
    // -------------------------------------------------------------------------

    [[nodiscard]] bool has_animations(WidgetId id) const {
        auto it = animations.find(id);
        return it != animations.end() && !it->second.empty();
    }

    [[nodiscard]] bool is_animating(WidgetId id) const {
        auto it = animations.find(id);
        if (it == animations.end()) return false;
        for (const auto& anim : it->second) {
            if (anim.state == AnimState::Playing) return true;
        }
        return false;
    }

    [[nodiscard]] const std::vector<ActiveAnimation>* get_animations(WidgetId id) const {
        auto it = animations.find(id);
        return it != animations.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const AnimationDef* get_definition(std::string_view name) const {
        auto it = definitions.find(std::string(name));
        return it != definitions.end() ? &it->second : nullptr;
    }
};

// =============================================================================
// Binding State
// =============================================================================

/// @brief Data source interface for bindings
class IDataSource {
public:
    virtual ~IDataSource() = default;

    virtual std::any get_value(std::string_view path) const = 0;
    virtual bool set_value(std::string_view path, const std::any& value) = 0;
    virtual bool has_path(std::string_view path) const = 0;

    using ChangeCallback = std::function<void(std::string_view path, const std::any& value)>;
    virtual void subscribe(std::string_view path, ChangeCallback callback) = 0;
    virtual void unsubscribe(std::string_view path) = 0;
};

/// @brief Binding state for all widgets
struct BindingState {
    // Data bindings per widget
    std::unordered_map<WidgetId, std::vector<DataBinding>> bindings;

    // All bindings indexed by ID
    std::unordered_map<BindingId, DataBinding*> bindings_by_id;

    // Data sources
    std::unordered_map<std::string, IDataSource*> sources;

    // Pending updates queue
    std::vector<BindingUpdate> pending_updates;

    // Change listeners
    std::unordered_map<std::string, std::vector<BindingCallback>> listeners;

    // Binding ID generation
    std::uint64_t next_binding_id{1};

    // -------------------------------------------------------------------------
    // Query Methods
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<DataBinding>* get_bindings(WidgetId id) const {
        auto it = bindings.find(id);
        return it != bindings.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const DataBinding* get_binding(BindingId id) const {
        auto it = bindings_by_id.find(id);
        return it != bindings_by_id.end() ? it->second : nullptr;
    }

    [[nodiscard]] IDataSource* get_source(std::string_view name) const {
        auto it = sources.find(std::string(name));
        return it != sources.end() ? it->second : nullptr;
    }
};

// =============================================================================
// Render State
// =============================================================================

/// @brief Render state for draw commands
struct RenderState {
    // Draw command lists per layer
    std::unordered_map<LayerId, DrawCommandList> layer_commands;

    // Shared vertex/index buffers
    std::vector<UiVertex> vertices;
    std::vector<std::uint32_t> indices;

    // Scissor stack
    std::vector<Rect> scissor_stack;

    // Current drawing state
    LayerId current_layer{0};
    Color current_color{Color::white()};
    float current_opacity{1.0f};

    // Render statistics
    std::uint32_t draw_calls{0};
    std::uint32_t triangles{0};
    std::uint32_t vertices_count{0};
    std::uint32_t widgets_rendered{0};

    // -------------------------------------------------------------------------
    // Methods
    // -------------------------------------------------------------------------

    void clear() {
        for (auto& [id, list] : layer_commands) {
            list.commands.clear();
            list.vertices.clear();
            list.indices.clear();
        }
        vertices.clear();
        indices.clear();
        scissor_stack.clear();
        draw_calls = 0;
        triangles = 0;
        vertices_count = 0;
        widgets_rendered = 0;
    }

    [[nodiscard]] Rect current_scissor() const {
        return scissor_stack.empty() ? Rect{0, 0, 100000, 100000} : scissor_stack.back();
    }
};

} // namespace void_widget
