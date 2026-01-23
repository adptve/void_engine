#pragma once

/// @file ui_module.hpp
/// @brief Main include header for void_ui
///
/// void_ui provides an immediate-mode UI toolkit:
///
/// ## Features
///
/// - **Theming**
///   - Built-in themes (dark, light, high-contrast, retro, solarized)
///   - Hot-reloadable custom themes from JSON
///   - Smooth theme transitions
///
/// - **Fonts**
///   - Built-in 8x16 bitmap font
///   - Custom font loading from bitmap files
///   - Hot-reload support
///
/// - **Widgets**
///   - DebugPanel, Label, ProgressBar
///   - FrameTimeGraph, Toast, HelpModal
///   - Button, Checkbox, Slider, TextInput
///   - Panel, Separator, Spacing
///
/// - **Rendering**
///   - Backend-agnostic renderer interface
///   - WGSL, GLSL, HLSL shader sources
///   - Efficient vertex batching
///
/// ## Quick Start
///
/// ```cpp
/// #include <void_engine/ui/ui_module.hpp>
///
/// using namespace void_ui;
///
/// // Create UI context
/// UiContext ctx;
/// ctx.set_screen_size(1920.0f, 1080.0f);
/// ctx.set_theme(Theme::dark());
///
/// // In render loop
/// ctx.begin_frame();
///
/// // Draw debug panel
/// DebugPanel::draw(ctx, 10, 10, "Stats", {
///     {"FPS:", "60.0", StatType::Good},
///     {"Frame:", "16.6ms", StatType::Normal},
///     {"Memory:", "256 MB", StatType::Info},
/// });
///
/// // Draw button
/// if (Button::draw(ctx, 10, 200, "Click Me").clicked) {
///     // Handle click
/// }
///
/// // Draw slider
/// static float value = 0.5f;
/// auto result = Slider::draw(ctx, 10, 250, "Volume", value);
/// if (result.changed) {
///     value = result.value;
/// }
///
/// ctx.end_frame();
///
/// // Render
/// auto& draw_data = ctx.draw_data();
/// renderer.prepare(draw_data);
/// renderer.render(render_pass);
/// ```
///
/// ## Hot-Reload Themes
///
/// ```cpp
/// // Create theme registry
/// ThemeRegistry registry;
///
/// // Watch directory for changes
/// registry.watch_directory("assets/themes/");
///
/// // In update loop
/// registry.poll_changes();
///
/// // Apply active theme to context
/// ctx.set_theme(registry.active_theme());
/// ```
///
/// ## Custom Fonts
///
/// ```cpp
/// // Create font registry
/// FontRegistry fonts;
///
/// // Load custom font
/// fonts.load_font_from_file("custom", "assets/fonts/myfont.png", 8, 16);
///
/// // Use in context
/// ctx.set_font(*fonts.get_font("custom"));
/// ```

// Core types
#include "fwd.hpp"
#include "types.hpp"

// Theme system
#include "theme.hpp"

// Font system
#include "font.hpp"

// UI Context
#include "context.hpp"

// Widgets
#include "widgets.hpp"

// Renderer
#include "renderer.hpp"

namespace void_ui {

/// Prelude - commonly used types for convenience
namespace prelude {
    // Types
    using void_ui::Color;
    using void_ui::Point;
    using void_ui::Size;
    using void_ui::Rect;
    using void_ui::Anchor;
    using void_ui::Alignment;
    using void_ui::LayoutConstraints;
    using void_ui::UiVertex;
    using void_ui::UiUniforms;
    using void_ui::UiDrawData;
    using void_ui::UiDrawCommand;
    using void_ui::StatType;
    using void_ui::ToastType;
    using void_ui::UiEvent;
    using void_ui::ClickEvent;
    using void_ui::HoverEvent;
    using void_ui::FocusEvent;

    // Theme
    using void_ui::ThemeColors;
    using void_ui::Theme;
    using void_ui::ThemeRegistry;

    // Font
    using void_ui::Glyph;
    using void_ui::BitmapFont;
    using void_ui::FontRegistry;

    // Context
    using void_ui::UiContext;

    // Widgets
    using void_ui::Label;
    using void_ui::DebugPanel;
    using void_ui::DebugStat;
    using void_ui::ProgressBar;
    using void_ui::ProgressBarConfig;
    using void_ui::FrameTimeGraph;
    using void_ui::FrameTimeGraphConfig;
    using void_ui::Toast;
    using void_ui::HelpModal;
    using void_ui::HelpControl;
    using void_ui::Button;
    using void_ui::ButtonResult;
    using void_ui::ButtonConfig;
    using void_ui::Checkbox;
    using void_ui::CheckboxResult;
    using void_ui::Slider;
    using void_ui::SliderResult;
    using void_ui::SliderConfig;
    using void_ui::TextInput;
    using void_ui::TextInputResult;
    using void_ui::TextInputConfig;
    using void_ui::Panel;
    using void_ui::PanelConfig;
    using void_ui::Separator;
    using void_ui::Spacing;

    // Renderer
    using void_ui::IUiRenderer;
    using void_ui::NullUiRenderer;
    using void_ui::UiGpuBuffers;
} // namespace prelude

} // namespace void_ui
