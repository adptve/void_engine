/// @file widget.hpp
/// @brief Widget base class for hot-swappable widget plugins

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "widget_api.hpp"

#include <void_engine/core/plugin.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <void_engine/plugin_api/dynamic_library.hpp>

#include <memory>
#include <string>
#include <vector>

namespace void_widget {

// =============================================================================
// Widget Base Class
// =============================================================================

/// @brief Base class for hot-swappable widget plugins
///
/// Widget plugins provide rendering and interaction logic for widget types.
/// They do NOT own widget state - WidgetStateCore owns all persistent state.
///
/// To create a widget plugin:
/// 1. Inherit from Widget
/// 2. Override widget_type() and provided_widgets()
/// 3. Override on_widget_load() to register widget renderers
/// 4. Override render_widget() to draw widget instances
/// 5. Export create_widget/destroy_widget factory functions
///
/// Example:
/// @code
/// class MyWidget : public void_widget::Widget {
/// public:
///     std::string widget_type() const override { return "my_widget"; }
///
///     std::vector<std::string> provided_widgets() const override {
///         return {"custom_button", "custom_panel"};
///     }
///
///     void_core::Result<void> on_widget_load() override {
///         // Initialize resources
///         return void_core::Ok();
///     }
///
///     void render_widget(WidgetId id, const WidgetInstance& widget) override {
///         auto* api = this->api();
///         auto bounds = api->get_bounds(id);
///
///         if (widget.type == "custom_button") {
///             render_custom_button(id, widget, bounds);
///         } else if (widget.type == "custom_panel") {
///             render_custom_panel(id, widget, bounds);
///         }
///     }
/// };
/// @endcode
class Widget : public void_core::Plugin, public void_core::HotReloadable {
public:
    Widget() = default;
    ~Widget() override = default;

    // Non-copyable, movable
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = default;
    Widget& operator=(Widget&&) = default;

    // =========================================================================
    // Plugin Identity (Override in derived class)
    // =========================================================================

    /// Get the widget plugin type (e.g., "hud", "menu", "debug")
    [[nodiscard]] virtual std::string widget_type() const = 0;

    /// Get the list of widget types this plugin provides
    [[nodiscard]] virtual std::vector<std::string> provided_widgets() const = 0;

    // =========================================================================
    // Plugin Identity (from void_core::Plugin)
    // =========================================================================

    [[nodiscard]] void_core::PluginId id() const override {
        return void_core::PluginId(widget_type());
    }

    [[nodiscard]] void_core::Version version() const override {
        return widget_version();
    }

    [[nodiscard]] std::string type_name() const override {
        return widget_type();
    }

    /// Override to provide version
    [[nodiscard]] virtual void_core::Version widget_version() const {
        return void_core::Version{1, 0, 0};
    }

    // =========================================================================
    // Widget Lifecycle
    // =========================================================================

    /// Called when widget plugin loads - register widget types, load resources
    [[nodiscard]] virtual void_core::Result<void> on_widget_load() { return void_core::Ok(); }

    /// Called when widget plugin unloads - cleanup resources
    virtual void on_widget_unload() {}

    /// Called every frame before rendering
    virtual void update(float dt) { (void)dt; }

    /// Called at fixed timestep
    virtual void fixed_update(float dt) { (void)dt; }

    // =========================================================================
    // Widget Rendering
    // =========================================================================

    /// Called to render a widget instance
    /// Override this to draw your widget types
    virtual void render_widget(WidgetId id, const WidgetInstance& widget) = 0;

    /// Called to measure widget content size
    /// Override for widgets that need FitContent sizing
    [[nodiscard]] virtual Vec2 measure_widget(WidgetId id, const WidgetInstance& widget,
                                               Vec2 available_size) {
        (void)id; (void)widget;
        return available_size;
    }

    // =========================================================================
    // Widget Events
    // =========================================================================

    /// Called when widget is clicked
    virtual void on_click(WidgetId id, const WidgetInstance& widget, Vec2 pos) {
        (void)id; (void)widget; (void)pos;
    }

    /// Called when widget is double-clicked
    virtual void on_double_click(WidgetId id, const WidgetInstance& widget, Vec2 pos) {
        (void)id; (void)widget; (void)pos;
    }

    /// Called when pointer enters widget
    virtual void on_hover_enter(WidgetId id, const WidgetInstance& widget) {
        (void)id; (void)widget;
    }

    /// Called when pointer exits widget
    virtual void on_hover_exit(WidgetId id, const WidgetInstance& widget) {
        (void)id; (void)widget;
    }

    /// Called when widget gains focus
    virtual void on_focus(WidgetId id, const WidgetInstance& widget) {
        (void)id; (void)widget;
    }

    /// Called when widget loses focus
    virtual void on_blur(WidgetId id, const WidgetInstance& widget) {
        (void)id; (void)widget;
    }

    /// Called when key is pressed while widget is focused
    virtual void on_key_press(WidgetId id, const WidgetInstance& widget, int key, int mods) {
        (void)id; (void)widget; (void)key; (void)mods;
    }

    /// Called when key is released while widget is focused
    virtual void on_key_release(WidgetId id, const WidgetInstance& widget, int key, int mods) {
        (void)id; (void)widget; (void)key; (void)mods;
    }

    /// Called when text is input while widget is focused
    virtual void on_text_input(WidgetId id, const WidgetInstance& widget, const std::string& text) {
        (void)id; (void)widget; (void)text;
    }

    /// Called when drag starts on widget
    virtual void on_drag_start(WidgetId id, const WidgetInstance& widget, Vec2 pos) {
        (void)id; (void)widget; (void)pos;
    }

    /// Called during drag
    virtual void on_drag(WidgetId id, const WidgetInstance& widget, Vec2 delta) {
        (void)id; (void)widget; (void)delta;
    }

    /// Called when drag ends
    virtual void on_drag_end(WidgetId id, const WidgetInstance& widget, Vec2 pos) {
        (void)id; (void)widget; (void)pos;
    }

    /// Called on scroll
    virtual void on_scroll(WidgetId id, const WidgetInstance& widget, float delta) {
        (void)id; (void)widget; (void)delta;
    }

    // =========================================================================
    // Hot-Reload Support (void_core::HotReloadable)
    // =========================================================================

    [[nodiscard]] bool supports_hot_reload() const override { return true; }

    /// Widgets don't own persistent state - WidgetStateCore does
    /// Snapshot is minimal since core state lives in WidgetStateCore
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        void_core::HotReloadSnapshot snap;
        snap.version = current_version();
        snap.type_name = type_name();
        // Widgets don't store persistent state - it's all in WidgetStateCore
        return void_core::Ok(std::move(snap));
    }

    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override {
        (void)snapshot;
        // Widgets don't restore state - WidgetStateCore maintains all persistent state
        // Just reinitialize runtime caches
        return on_widget_load();
    }

    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override {
        // Compatible if major version matches
        return new_version.major == widget_version().major;
    }

    [[nodiscard]] void_core::Version current_version() const override {
        return widget_version();
    }

    // =========================================================================
    // void_core::Plugin Integration
    // =========================================================================

    void_core::Result<void> on_load(void_core::PluginContext& ctx) override {
        // Get the widget API from context
        auto* api_ptr = ctx.get_mut<IWidgetAPI*>("widget_api");
        if (api_ptr) {
            m_api = *api_ptr;
        }
        if (!m_api) {
            return void_core::Error("Failed to get widget API from context");
        }
        return on_widget_load();
    }

    void_core::Result<void_core::PluginState> on_unload(void_core::PluginContext& ctx) override {
        (void)ctx;
        on_widget_unload();
        return void_core::Ok(void_core::PluginState{});
    }

    /// Called every frame (from void_core::Plugin)
    void on_update(float dt) override {
        update(dt);
    }

    // =========================================================================
    // Widget API
    // =========================================================================

    /// Set the widget API (called by WidgetStateCore when loading plugin)
    void set_api(IWidgetAPI* api) { m_api = api; }

protected:
    /// Get the widget API
    [[nodiscard]] IWidgetAPI* api() { return m_api; }
    [[nodiscard]] const IWidgetAPI* api() const { return m_api; }

private:
    IWidgetAPI* m_api{nullptr};
};

// =============================================================================
// Widget Factory Functions
// =============================================================================

/// Create function type for widget plugins
using CreateWidgetFunc = Widget* (*)();

/// Destroy function type for widget plugins
using DestroyWidgetFunc = void (*)(Widget*);

// =============================================================================
// Loaded Widget Plugin
// =============================================================================

/// @brief Loaded widget plugin with its library handle
struct LoadedWidget {
    std::unique_ptr<void_plugin_api::DynamicLibrary> library;
    Widget* widget{nullptr};
    DestroyWidgetFunc destroy_func{nullptr};
    std::string name;

    ~LoadedWidget() {
        if (widget && destroy_func) {
            destroy_func(widget);
        }
        // Library unloads automatically via unique_ptr
    }

    // Non-copyable, movable
    LoadedWidget() = default;
    LoadedWidget(const LoadedWidget&) = delete;
    LoadedWidget& operator=(const LoadedWidget&) = delete;
    LoadedWidget(LoadedWidget&&) = default;
    LoadedWidget& operator=(LoadedWidget&&) = default;
};

} // namespace void_widget

// =============================================================================
// Widget Plugin Export Macros
// =============================================================================

/// Export macro for widget plugins
#ifdef _WIN32
#define VOID_WIDGET_EXPORT __declspec(dllexport)
#else
#define VOID_WIDGET_EXPORT __attribute__((visibility("default")))
#endif

/// Macro to define widget plugin factory functions
#define VOID_WIDGET_PLUGIN(WidgetClass) \
    extern "C" { \
        VOID_WIDGET_EXPORT void_widget::Widget* create_widget() { \
            return new WidgetClass(); \
        } \
        VOID_WIDGET_EXPORT void destroy_widget(void_widget::Widget* widget) { \
            delete widget; \
        } \
    }
