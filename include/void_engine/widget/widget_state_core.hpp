/// @file widget_state_core.hpp
/// @brief Core widget state management
///
/// WidgetStateCore is the authoritative owner of all widget state.
/// It persists across widget plugin hot-reloads, ensuring UI state is never lost.

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "state_stores.hpp"
#include "widget_api.hpp"
#include "widget.hpp"

#include <void_engine/plugin_api/dynamic_library.hpp>

#include <any>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_gamestate {
class GameStateCore;
}

namespace void_widget {

// =============================================================================
// Widget Loader Interface
// =============================================================================

/// @brief Interface for widget watcher to load/unload widgets
class IWidgetLoader {
public:
    virtual ~IWidgetLoader() = default;

    virtual bool watcher_load_widget(const std::filesystem::path& path) = 0;
    virtual bool watcher_unload_widget(const std::string& name) = 0;
    virtual bool watcher_hot_reload_widget(const std::string& name, const std::filesystem::path& new_path) = 0;
    virtual bool watcher_is_widget_loaded(const std::string& name) const = 0;
    virtual std::vector<std::string> watcher_loaded_widgets() const = 0;
};

// =============================================================================
// WidgetStateCore Configuration
// =============================================================================

/// @brief Configuration for WidgetStateCore
struct WidgetStateCoreConfig {
    // Limits
    std::uint32_t max_widgets{100000};
    std::uint32_t max_layers{64};
    std::uint32_t max_animations{10000};
    std::uint32_t max_bindings{10000};

    // Screen settings
    float screen_width{1920};
    float screen_height{1080};
    float ui_scale{1.0f};

    // Hot-reload
    bool enable_hot_reload{true};
    bool validate_commands{true};
    std::string widget_directory{"widgets"};

    // Performance
    bool batch_draw_calls{true};
    bool cache_computed_styles{true};

    // Debug
    bool debug_bounds{false};
    bool debug_focus{false};
};

/// @brief Configuration for widget watcher
struct WidgetWatcherConfig {
    std::vector<std::filesystem::path> watch_paths;
    bool auto_load_new{true};
    bool auto_reload_changed{true};
    bool watch_sources{false};
    std::chrono::milliseconds debounce_time{500};
    std::chrono::milliseconds poll_interval{100};
    std::string build_command;
};

/// @brief Get native widget extension for current platform
inline std::string native_widget_extension() {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

// =============================================================================
// WidgetStateCore
// =============================================================================

/// @brief Central widget state management owning all persistent UI state
///
/// WidgetStateCore follows the same pattern as GameStateCore:
/// - Owns ALL persistent widget state (positions, styles, bindings, etc.)
/// - Widget plugins read state through IWidgetAPI
/// - Widget plugins submit commands to modify state
/// - State survives widget plugin hot-reloads
class WidgetStateCore : public IWidgetLoader {
public:
    WidgetStateCore();
    explicit WidgetStateCore(const WidgetStateCoreConfig& config);
    ~WidgetStateCore();

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /// Initialize the widget system
    void initialize();

    /// Shutdown and cleanup
    void shutdown();

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const { return m_initialized; }

    // -------------------------------------------------------------------------
    // State Store Access (for WidgetAPIImpl)
    // -------------------------------------------------------------------------

    /// Get widget registry
    [[nodiscard]] WidgetRegistry& widget_registry() { return m_widget_registry; }
    [[nodiscard]] const WidgetRegistry& widget_registry() const { return m_widget_registry; }

    /// Get layout state
    [[nodiscard]] LayoutState& layout_state() { return m_layout_state; }
    [[nodiscard]] const LayoutState& layout_state() const { return m_layout_state; }

    /// Get style state
    [[nodiscard]] StyleState& style_state() { return m_style_state; }
    [[nodiscard]] const StyleState& style_state() const { return m_style_state; }

    /// Get interaction state
    [[nodiscard]] InteractionState& interaction_state() { return m_interaction_state; }
    [[nodiscard]] const InteractionState& interaction_state() const { return m_interaction_state; }

    /// Get animation state
    [[nodiscard]] AnimationState& animation_state() { return m_animation_state; }
    [[nodiscard]] const AnimationState& animation_state() const { return m_animation_state; }

    /// Get binding state
    [[nodiscard]] BindingState& binding_state() { return m_binding_state; }
    [[nodiscard]] const BindingState& binding_state() const { return m_binding_state; }

    /// Get render state
    [[nodiscard]] RenderState& render_state() { return m_render_state; }
    [[nodiscard]] const RenderState& render_state() const { return m_render_state; }

    // -------------------------------------------------------------------------
    // Widget API
    // -------------------------------------------------------------------------

    /// Get the widget API (implemented in .cpp to avoid forward declaration issues)
    [[nodiscard]] IWidgetAPI* widget_api();

    // -------------------------------------------------------------------------
    // Update Loop
    // -------------------------------------------------------------------------

    /// Begin frame (call before input handling)
    void begin_frame(float dt);

    /// Process input
    void process_input();

    /// Update animations and bindings
    void update(float dt);

    /// Compute layout for dirty widgets
    void layout();

    /// Render all visible widgets
    void render();

    /// End frame
    void end_frame();

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /// Set mouse position
    void set_mouse_position(float x, float y);

    /// Set mouse button state
    void set_mouse_button(int button, bool pressed);

    /// Set scroll delta
    void set_scroll(float delta);

    /// Set key state
    void set_key(int key, bool pressed);

    /// Set key modifiers
    void set_modifiers(std::uint32_t mods);

    /// Add text input
    void add_text_input(const std::string& text);

    /// Clear text input buffer
    void clear_text_input();

    // -------------------------------------------------------------------------
    // Screen Management
    // -------------------------------------------------------------------------

    /// Set screen size
    void set_screen_size(float width, float height);

    /// Set UI scale
    void set_ui_scale(float scale);

    /// Get screen size
    [[nodiscard]] Vec2 screen_size() const;

    /// Get delta time (seconds since last frame)
    [[nodiscard]] float delta_time() const { return m_delta_time; }

    /// Get current time (seconds since start)
    [[nodiscard]] double current_time() const { return m_current_time; }

    /// Get UI scale
    [[nodiscard]] float ui_scale() const { return m_config.ui_scale; }

    // -------------------------------------------------------------------------
    // Widget Management
    // -------------------------------------------------------------------------

    /// Create a widget
    WidgetId create_widget(std::string_view type, std::string_view name = "");

    /// Destroy a widget and its children
    void destroy_widget(WidgetId id);

    /// Set widget parent
    void set_parent(WidgetId child, WidgetId parent);

    /// Get widget by ID
    [[nodiscard]] const WidgetInstance* get_widget(WidgetId id) const;

    /// Find widget by name
    [[nodiscard]] WidgetId find_widget(std::string_view name) const;

    // -------------------------------------------------------------------------
    // Layer Management
    // -------------------------------------------------------------------------

    /// Create a layer
    LayerId create_layer(std::string_view name, int z_order = 0);

    /// Destroy a layer
    void destroy_layer(LayerId id);

    /// Get layer by ID
    [[nodiscard]] const WidgetLayer* get_layer(LayerId id) const;

    // -------------------------------------------------------------------------
    // Theme Management
    // -------------------------------------------------------------------------

    /// Register a theme
    void register_theme(const Theme& theme);

    /// Apply a theme
    void apply_theme(std::string_view name);

    /// Get current theme
    [[nodiscard]] const Theme* current_theme() const;

    // -------------------------------------------------------------------------
    // Data Source Management
    // -------------------------------------------------------------------------

    /// Register a data source (interface)
    void register_data_source(std::string_view name, IDataSource* source);

    /// Register a data source (function callback)
    using DataSourceCallback = std::function<std::any(const std::string&)>;
    void register_data_source(std::string_view name, DataSourceCallback callback);

    /// Unregister a data source
    void unregister_data_source(std::string_view name);

    /// Set GameStateCore as data source (for gameplay bindings)
    void set_game_state(void_gamestate::GameStateCore* game_state);

    // -------------------------------------------------------------------------
    // Widget Plugin Management
    // -------------------------------------------------------------------------

    /// Load a widget plugin by name
    void_core::Result<void> load_widget_plugin(const std::string& name);

    /// Load a widget plugin from path
    bool load_widget_plugin(const std::filesystem::path& path);

    /// Unload a widget plugin
    void_core::Result<void> unload_widget_plugin(const std::string& name);

    /// Hot-reload a widget plugin
    void_core::Result<void> hot_reload_widget_plugin(const std::string& name);

    /// Get active widget instance count
    [[nodiscard]] std::size_t active_widget_count() const;

    /// Get active plugin count (number of loaded widget plugins)
    [[nodiscard]] std::size_t active_plugin_count() const;

    /// Get widget plugin by name
    [[nodiscard]] Widget* get_widget_plugin(std::string_view name) const;

    // -------------------------------------------------------------------------
    // IWidgetLoader Interface (for watcher)
    // -------------------------------------------------------------------------

    bool watcher_load_widget(const std::filesystem::path& path) override;
    bool watcher_unload_widget(const std::string& name) override;
    bool watcher_hot_reload_widget(const std::string& name, const std::filesystem::path& new_path) override;
    bool watcher_is_widget_loaded(const std::string& name) const override;
    std::vector<std::string> watcher_loaded_widgets() const override;

    // -------------------------------------------------------------------------
    // Widget Watcher
    // -------------------------------------------------------------------------

    /// Configure the widget watcher
    void configure_watcher(const WidgetWatcherConfig& config);

    /// Start watching for widget plugin changes
    void start_watching(const std::vector<std::filesystem::path>& paths = {});

    /// Stop watching
    void stop_watching();

    /// Check if watching
    [[nodiscard]] bool is_watching() const;

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /// Serialize all widget state
    [[nodiscard]] std::vector<std::uint8_t> serialize_state() const;

    /// Deserialize widget state
    void deserialize_state(const std::vector<std::uint8_t>& data);

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    struct Stats {
        std::uint64_t total_widgets{0};
        std::uint64_t visible_widgets{0};
        std::uint64_t layers{0};
        std::uint64_t active_animations{0};
        std::uint64_t active_bindings{0};
        std::uint32_t draw_calls{0};
        std::uint32_t triangles{0};
        std::uint32_t active_plugins{0};
    };

    [[nodiscard]] Stats stats() const;

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    using WidgetCallback = std::function<void(WidgetId, const WidgetEvent&)>;

    void on_click(WidgetCallback callback) { m_on_click = std::move(callback); }
    void on_hover(WidgetCallback callback) { m_on_hover = std::move(callback); }
    void on_focus_change(WidgetCallback callback) { m_on_focus_change = std::move(callback); }

private:
    void setup_default_themes();
    void setup_default_layer();
    void process_commands();
    void update_animations(float dt);
    void update_bindings();
    void compute_layout(WidgetId id);
    void render_widget(WidgetId id);
    WidgetId hit_test(Vec2 point) const;
    WidgetId hit_test_recursive(WidgetId id, Vec2 point) const;
    void dispatch_event(WidgetId id, const WidgetEvent& event);

    WidgetStateCoreConfig m_config;
    bool m_initialized{false};

    // State stores (OWNED - persist across hot-reloads)
    WidgetRegistry m_widget_registry;
    LayoutState m_layout_state;
    StyleState m_style_state;
    InteractionState m_interaction_state;
    AnimationState m_animation_state;
    BindingState m_binding_state;
    RenderState m_render_state;

    // Widget API
    std::unique_ptr<WidgetAPIImpl> m_widget_api;

    // Loaded widget plugins
    std::unordered_map<std::string, std::unique_ptr<LoadedWidget>> m_loaded_widgets;

    // Widget type to plugin mapping
    std::unordered_map<std::string, Widget*> m_widget_type_to_plugin;

    // Command queue
    std::vector<std::unique_ptr<IWidgetCommand>> m_command_queue;

    // Game state for data binding
    void_gamestate::GameStateCore* m_game_state{nullptr};

    // Timing
    float m_delta_time{0};
    double m_current_time{0};
    std::uint32_t m_frame_number{0};

    // Event callbacks
    WidgetCallback m_on_click;
    WidgetCallback m_on_hover;
    WidgetCallback m_on_focus_change;

    // Watching
    bool m_watching{false};
    std::vector<std::filesystem::path> m_watch_paths;
    WidgetWatcherConfig m_watcher_config;

    // Data source callbacks
    std::unordered_map<std::string, DataSourceCallback> m_data_source_callbacks;
};

// =============================================================================
// WidgetAPIImpl
// =============================================================================

/// @brief Implementation of IWidgetAPI
class WidgetAPIImpl : public IWidgetAPI {
public:
    WidgetAPIImpl(WidgetStateCore* core, void_gamestate::GameStateCore* game_state);
    ~WidgetAPIImpl() override;

    // IWidgetAPI implementation - see widget_api.hpp for documentation
    // All methods delegate to WidgetStateCore

    const WidgetRegistry& registry() const override;
    const LayoutState& layout() const override;
    const StyleState& style() const override;
    const InteractionState& interaction() const override;
    const AnimationState& animation() const override;
    const BindingState& bindings() const override;

    const WidgetInstance* get_widget(WidgetId id) const override;
    WidgetId find_widget(std::string_view name) const override;
    std::vector<WidgetId> find_widgets_by_type(std::string_view type) const override;
    std::vector<WidgetId> get_children(WidgetId parent) const override;
    WidgetId get_parent(WidgetId child) const override;
    Rect get_bounds(WidgetId id) const override;
    ComputedStyle get_computed_style(WidgetId id) const override;

    bool is_hovered(WidgetId id) const override;
    bool is_pressed(WidgetId id) const override;
    bool is_focused(WidgetId id) const override;
    bool is_visible(WidgetId id) const override;
    bool hit_test(WidgetId id, Vec2 point) const override;

    void submit_command(std::unique_ptr<IWidgetCommand> cmd) override;

    WidgetId create_widget(std::string_view type, std::string_view name) override;
    WidgetId create_from_template(std::string_view template_name, std::string_view name) override;
    void destroy_widget(WidgetId id) override;
    void set_parent(WidgetId child, WidgetId parent) override;
    void set_layer(WidgetId id, LayerId layer) override;

    void set_position(WidgetId id, Vec2 pos) override;
    void set_size(WidgetId id, Vec2 size) override;
    void set_anchor(WidgetId id, Vec2 anchor) override;
    void set_anchor(WidgetId id, Anchor anchor) override;
    void set_pivot(WidgetId id, Vec2 pivot) override;
    void set_margin(WidgetId id, Insets margin) override;
    void set_padding(WidgetId id, Insets padding) override;
    void set_visible(WidgetId id, bool visible) override;
    void set_rotation(WidgetId id, float degrees) override;
    void set_scale(WidgetId id, Vec2 scale) override;

    void set_style(WidgetId id, std::string_view property, const std::any& value) override;
    void set_background_color(WidgetId id, const Color& color) override;
    void set_border_color(WidgetId id, const Color& color) override;
    void set_text_color(WidgetId id, const Color& color) override;
    void set_opacity(WidgetId id, float opacity) override;
    void apply_theme(std::string_view theme_name) override;

    AnimationId play_animation(WidgetId id, std::string_view anim_name) override;
    void stop_animation(WidgetId id, AnimationId anim) override;
    void stop_all_animations(WidgetId id) override;
    AnimationId animate_property(WidgetId id, std::string_view property,
                                 const std::any& target, float duration,
                                 EasingType easing) override;
    AnimationId fade_in(WidgetId id, float duration) override;
    AnimationId fade_out(WidgetId id, float duration) override;
    AnimationId slide_in(WidgetId id, Vec2 from, float duration) override;
    AnimationId slide_out(WidgetId id, Vec2 to, float duration) override;

    BindingId bind(WidgetId id, std::string_view property,
                   std::string_view source_path, BindingMode mode) override;
    void unbind(BindingId binding) override;
    void unbind_all(WidgetId id) override;

    void set_text(WidgetId id, std::string_view text) override;
    std::string get_text(WidgetId id) const override;
    void set_value(WidgetId id, float value) override;
    float get_value(WidgetId id) const override;
    void set_checked(WidgetId id, bool checked) override;
    bool is_checked(WidgetId id) const override;
    void set_enabled(WidgetId id, bool enabled) override;
    bool is_enabled(WidgetId id) const override;
    void set_property(WidgetId id, std::string_view key, const std::any& value) override;
    std::any get_property(WidgetId id, std::string_view key) const override;

    void draw_rect(const Rect& rect, const Color& color) override;
    void draw_rect_outline(const Rect& rect, const Color& color, float width) override;
    void draw_rounded_rect(const Rect& rect, const Color& color, float radius) override;
    void draw_rounded_rect_outline(const Rect& rect, const Color& color,
                                    float radius, float width) override;
    void draw_text(const std::string& text, Vec2 pos, const Color& color,
                   float size, std::string_view font) override;
    void draw_text_aligned(const std::string& text, const Rect& rect,
                            TextAlign h_align, VerticalAlign v_align,
                            const Color& color, float size) override;
    void draw_icon(std::string_view icon, Vec2 pos, Vec2 size, const Color& tint) override;
    void draw_line(Vec2 from, Vec2 to, const Color& color, float width) override;
    void draw_texture(std::string_view texture, const Rect& dest,
                      const Rect& src, const Color& tint) override;
    void draw_circle(Vec2 center, float radius, const Color& color) override;
    void draw_circle_outline(Vec2 center, float radius, const Color& color, float width) override;
    void push_scissor(const Rect& rect) override;
    void pop_scissor() override;

    Vec2 mouse_position() const override;
    Vec2 mouse_delta() const override;
    bool is_mouse_down(int button) const override;
    bool is_mouse_pressed(int button) const override;
    bool is_mouse_released(int button) const override;
    bool is_key_down(int key) const override;
    bool is_key_pressed(int key) const override;
    const std::string& text_input() const override;

    void set_focus(WidgetId id) override;
    void clear_focus() override;
    void focus_next() override;
    void focus_prev() override;

    void subscribe(WidgetId id, WidgetEventType event, WidgetEventCallback callback) override;
    void unsubscribe(WidgetId id, WidgetEventType event) override;

    LayerId create_layer(std::string_view name, int z_order) override;
    void destroy_layer(LayerId id) override;
    void set_layer_visible(LayerId id, bool visible) override;
    void set_layer_opacity(LayerId id, float opacity) override;

    float delta_time() const override;
    double current_time() const override;
    Vec2 screen_size() const override;
    float ui_scale() const override;
    const void_gamestate::GameStateCore* game_state() const override;

private:
    WidgetStateCore* m_core;
    void_gamestate::GameStateCore* m_game_state;

    // Event subscriptions
    struct Subscription {
        WidgetId widget;
        WidgetEventType event;
        WidgetEventCallback callback;
    };
    std::vector<Subscription> m_subscriptions;
};

} // namespace void_widget
