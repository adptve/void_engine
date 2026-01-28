/// @file core_widget.hpp
/// @brief Core widget plugin providing basic UI elements
///
/// This plugin provides fundamental widget types:
/// - panel: Container with background and border
/// - text: Text display
/// - button: Clickable button
/// - checkbox: Boolean toggle
/// - slider: Value slider
/// - progress_bar: Progress indicator
/// - text_input: Text entry field
/// - image: Static image display

#pragma once

#include <void_engine/widget/widget.hpp>

#include <unordered_map>
#include <functional>

namespace core_widgets {

/// @brief Core widget plugin providing basic UI elements
class CoreWidget : public void_widget::Widget {
public:
    CoreWidget();
    ~CoreWidget() override = default;

    // =========================================================================
    // Widget Identity
    // =========================================================================

    [[nodiscard]] std::string widget_type() const override {
        return "core";
    }

    [[nodiscard]] std::vector<std::string> provided_widgets() const override {
        return {
            "panel",
            "text",
            "button",
            "checkbox",
            "slider",
            "progress_bar",
            "text_input",
            "image",
            "separator",
            "spacer"
        };
    }

    [[nodiscard]] void_core::Version widget_version() const override {
        return void_core::Version{1, 0, 0};
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    void_core::Result<void> on_widget_load() override;
    void on_widget_unload() override;
    void update(float dt) override;

    // =========================================================================
    // Rendering
    // =========================================================================

    void render_widget(void_widget::WidgetId id,
                       const void_widget::WidgetInstance& widget) override;

    [[nodiscard]] void_widget::Vec2 measure_widget(
        void_widget::WidgetId id,
        const void_widget::WidgetInstance& widget,
        void_widget::Vec2 available_size) override;

    // =========================================================================
    // Events
    // =========================================================================

    void on_click(void_widget::WidgetId id,
                  const void_widget::WidgetInstance& widget,
                  void_widget::Vec2 pos) override;

    void on_hover_enter(void_widget::WidgetId id,
                        const void_widget::WidgetInstance& widget) override;

    void on_hover_exit(void_widget::WidgetId id,
                       const void_widget::WidgetInstance& widget) override;

    void on_key_press(void_widget::WidgetId id,
                      const void_widget::WidgetInstance& widget,
                      int key, int mods) override;

    void on_text_input(void_widget::WidgetId id,
                       const void_widget::WidgetInstance& widget,
                       const std::string& text) override;

    void on_drag(void_widget::WidgetId id,
                 const void_widget::WidgetInstance& widget,
                 void_widget::Vec2 delta) override;

private:
    // Rendering helpers
    void render_panel(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_text(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_button(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_checkbox(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_slider(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_progress_bar(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_text_input(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_image(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_separator(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);
    void render_spacer(void_widget::WidgetId id, const void_widget::WidgetInstance& widget);

    // Text input cursor blink
    float m_cursor_blink_timer{0};
    bool m_cursor_visible{true};
    static constexpr float CURSOR_BLINK_RATE = 0.53f;
};

} // namespace core_widgets
