/// @file widget_module.hpp
/// @brief Main include header for the widget system
///
/// Phase 10: Widget System
/// Hot-swappable widget plugins with centralized state management.

#pragma once

// Forward declarations
#include "fwd.hpp"

// Core types
#include "types.hpp"

// State stores
#include "state_stores.hpp"

// Widget API
#include "widget_api.hpp"

// Widget base class
#include "widget.hpp"

// Commands
#include "commands.hpp"

// Core state management
#include "widget_state_core.hpp"

namespace void_widget {

/// @brief Convenience namespace for common types
namespace prelude {
    using void_widget::WidgetId;
    using void_widget::LayerId;
    using void_widget::AnimationId;
    using void_widget::BindingId;
    using void_widget::Vec2;
    using void_widget::Rect;
    using void_widget::Color;
    using void_widget::Insets;
    using void_widget::Widget;
    using void_widget::IWidgetAPI;
    using void_widget::WidgetStateCore;
    using void_widget::WidgetInstance;
    using void_widget::Theme;
    using void_widget::EasingType;
    using void_widget::BindingMode;
    using void_widget::Anchor;
    using void_widget::TextAlign;
    using void_widget::VerticalAlign;
    using void_widget::Visibility;
    using void_widget::WidgetState;
    using void_widget::WidgetEvent;
    using void_widget::WidgetEventType;
} // namespace prelude

} // namespace void_widget

/*
 * WIDGET SYSTEM USAGE
 * ===================
 *
 * Phase 10 provides a hot-swappable widget system where:
 * - WidgetStateCore owns ALL widget state (positions, styles, bindings)
 * - Widget plugins render UI and handle events
 * - Plugins can be hot-reloaded without losing UI state
 *
 * CREATING A WIDGET PLUGIN
 * ------------------------
 *
 * 1. Create a class inheriting from Widget:
 *
 *    class MyWidget : public void_widget::Widget {
 *    public:
 *        std::string widget_type() const override {
 *            return "my_widget";
 *        }
 *
 *        std::vector<std::string> provided_widgets() const override {
 *            return {"my_button", "my_panel"};
 *        }
 *
 *        void_core::Result<void> on_widget_load() override {
 *            // Initialize resources
 *            return void_core::Ok();
 *        }
 *
 *        void render_widget(WidgetId id, const WidgetInstance& widget) override {
 *            auto* api = this->api();
 *            auto bounds = api->get_bounds(id);
 *            auto style = api->get_computed_style(id);
 *
 *            if (widget.type == "my_button") {
 *                // Draw button background
 *                if (api->is_pressed(id)) {
 *                    api->draw_rounded_rect(bounds, style.background_color, 4);
 *                } else if (api->is_hovered(id)) {
 *                    api->draw_rounded_rect(bounds, Color::lerp(style.background_color,
 *                                                                Color::white(), 0.1f), 4);
 *                } else {
 *                    api->draw_rounded_rect(bounds, style.background_color, 4);
 *                }
 *
 *                // Draw button text
 *                auto text = widget.get_property<std::string>("text", "Button");
 *                api->draw_text_aligned(text, bounds, TextAlign::Center, VerticalAlign::Middle,
 *                                       style.text_color, style.font_size);
 *            }
 *        }
 *
 *        void on_click(WidgetId id, const WidgetInstance& widget, Vec2 pos) override {
 *            // Handle button click
 *        }
 *    };
 *
 * 2. Export factory functions:
 *
 *    VOID_WIDGET_PLUGIN(MyWidget)
 *
 * 3. Create CMakeLists.txt:
 *
 *    add_library(my_widget SHARED
 *        my_widget.cpp
 *    )
 *    target_link_libraries(my_widget PRIVATE void_widget)
 *
 * USING WIDGETS IN GAME
 * ---------------------
 *
 * // Initialize widget system
 * WidgetStateCore widget_core;
 * widget_core.initialize();
 *
 * // Create widgets
 * auto button = widget_core.create_widget("my_button", "start_button");
 * widget_core.set_position(button, {100, 100});
 * widget_core.set_size(button, {200, 50});
 *
 * // Data binding (connects to GameStateCore)
 * auto health_bar = widget_core.create_widget("progress_bar", "health");
 * widget_core.bind(health_bar, "value", "player.health.current");
 * widget_core.bind(health_bar, "max_value", "player.health.max");
 *
 * // Game loop
 * while (running) {
 *     widget_core.begin_frame(dt);
 *     widget_core.process_input();
 *     widget_core.update(dt);
 *     widget_core.layout();
 *     widget_core.render();
 *     widget_core.end_frame();
 * }
 *
 * ANIMATION
 * ---------
 *
 * // Animate a property
 * widget_api->animate_property(widget, "opacity", 1.0f, 0.3f, EasingType::EaseOutQuad);
 *
 * // Quick animations
 * widget_api->fade_in(widget, 0.2f);
 * widget_api->slide_in(widget, {-100, 0}, 0.3f);
 *
 * // Named animations (defined in animation state)
 * widget_api->play_animation(widget, "pulse");
 *
 * DATA BINDING
 * ------------
 *
 * // One-way binding (source -> widget)
 * widget_api->bind(health_bar, "value", "player.vitals.health", BindingMode::OneWay);
 *
 * // Two-way binding (for input fields)
 * widget_api->bind(name_input, "text", "player.name", BindingMode::TwoWay);
 *
 * // Binding to game state
 * widget_api->bind(ammo_text, "text", "player.weapon.ammo");
 * widget_api->bind(objective_marker, "world_position", "objectives.current.position");
 */
