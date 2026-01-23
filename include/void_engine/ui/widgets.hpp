#pragma once

/// @file widgets.hpp
/// @brief UI Widgets - Reusable UI components
///
/// Provides immediate-mode widgets:
/// - DebugPanel: Debug info display
/// - Label: Text labels
/// - ProgressBar: Progress indication
/// - FrameTimeGraph: Performance visualization
/// - Toast: Notifications
/// - HelpModal: Help overlays
/// - Button: Clickable buttons
/// - Checkbox: Boolean toggles
/// - Slider: Value sliders
/// - TextInput: Text entry

#include "fwd.hpp"
#include "types.hpp"
#include "context.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace void_ui {

// =============================================================================
// Label Widget
// =============================================================================

/// Simple text label
class Label {
public:
    /// Draw a simple text label
    static void draw(UiContext& ctx, float x, float y, const std::string& text);

    /// Draw a colored label
    static void draw(UiContext& ctx, float x, float y, const std::string& text, const Color& color);

    /// Draw a label with custom scale
    static void draw(UiContext& ctx, float x, float y, const std::string& text,
                     const Color& color, float scale);
};

// =============================================================================
// Debug Panel Widget
// =============================================================================

/// Debug stat entry
struct DebugStat {
    std::string label;
    std::string value;
    StatType type = StatType::Normal;
};

/// Debug panel widget for displaying stats
class DebugPanel {
public:
    /// Draw a debug panel with the given stats
    static void draw(UiContext& ctx, float x, float y, const std::string& title,
                     const std::vector<DebugStat>& stats);

    /// Draw with legacy tuple format
    static void draw(UiContext& ctx, float x, float y, const std::string& title,
                     const std::vector<std::tuple<std::string, std::string, StatType>>& stats);
};

// =============================================================================
// Progress Bar Widget
// =============================================================================

/// Progress bar configuration
struct ProgressBarConfig {
    float width = 200.0f;
    float height = 20.0f;
    std::optional<Color> fill_color;  // None = use theme accent
    bool show_percentage = false;
};

/// Horizontal progress bar
class ProgressBar {
public:
    /// Draw a horizontal progress bar
    /// @param progress Value between 0.0 and 1.0
    static void draw(UiContext& ctx, float x, float y, float progress,
                     const ProgressBarConfig& config = {});

    /// Draw with explicit dimensions
    static void draw(UiContext& ctx, float x, float y, float width, float height,
                     float progress, std::optional<Color> color = std::nullopt);
};

// =============================================================================
// Frame Time Graph Widget
// =============================================================================

/// Frame time graph configuration
struct FrameTimeGraphConfig {
    float width = 300.0f;
    float height = 100.0f;
    float target_fps = 60.0f;
    float max_frame_time_mult = 3.0f;  // Show up to N times target frame time
    bool show_target_line = true;
    bool show_labels = true;
};

/// Frame time history graph
class FrameTimeGraph {
public:
    /// Draw a frame time history graph
    /// @param frame_times Frame times in milliseconds
    static void draw(UiContext& ctx, float x, float y,
                     const std::vector<float>& frame_times,
                     const FrameTimeGraphConfig& config = {});

    /// Draw with explicit dimensions
    static void draw(UiContext& ctx, float x, float y, float width, float height,
                     const std::vector<float>& frame_times, float target_fps = 60.0f);
};

// =============================================================================
// Toast Widget
// =============================================================================

/// Toast notification
class Toast {
public:
    /// Draw a centered toast notification
    static void draw(UiContext& ctx, float y, const std::string& message, ToastType type);

    /// Draw at specific position
    static void draw(UiContext& ctx, float x, float y, const std::string& message, ToastType type);
};

// =============================================================================
// Help Modal Widget
// =============================================================================

/// Control entry for help modal
struct HelpControl {
    std::string key;
    std::string description;
};

/// Help modal widget for displaying controls
class HelpModal {
public:
    /// Draw a centered help modal
    static void draw(UiContext& ctx, const std::string& title,
                     const std::vector<HelpControl>& controls,
                     const std::string& footer = "");

    /// Draw with legacy tuple format
    static void draw(UiContext& ctx, const std::string& title,
                     const std::vector<std::pair<std::string, std::string>>& controls,
                     const std::string& footer = "");
};

// =============================================================================
// Button Widget
// =============================================================================

/// Button result
struct ButtonResult {
    bool clicked = false;
    bool hovered = false;
    bool held = false;
};

/// Button configuration
struct ButtonConfig {
    float width = 0.0f;      // 0 = auto-size to text
    float height = 0.0f;     // 0 = use theme line height
    float padding = 0.0f;    // 0 = use theme padding
    bool enabled = true;
    std::optional<Color> bg_color;
    std::optional<Color> text_color;
};

/// Clickable button
class Button {
public:
    /// Draw a button and return interaction state
    static ButtonResult draw(UiContext& ctx, float x, float y,
                             const std::string& label,
                             const ButtonConfig& config = {});

    /// Draw with ID for stable interaction
    static ButtonResult draw(UiContext& ctx, std::uint64_t id, float x, float y,
                             const std::string& label,
                             const ButtonConfig& config = {});
};

// =============================================================================
// Checkbox Widget
// =============================================================================

/// Checkbox result
struct CheckboxResult {
    bool changed = false;
    bool checked = false;
    bool hovered = false;
};

/// Boolean toggle checkbox
class Checkbox {
public:
    /// Draw a checkbox and return interaction state
    static CheckboxResult draw(UiContext& ctx, float x, float y,
                               const std::string& label, bool checked);

    /// Draw with ID for stable interaction
    static CheckboxResult draw(UiContext& ctx, std::uint64_t id, float x, float y,
                               const std::string& label, bool checked);
};

// =============================================================================
// Slider Widget
// =============================================================================

/// Slider result
struct SliderResult {
    bool changed = false;
    float value = 0.0f;
    bool hovered = false;
    bool dragging = false;
};

/// Slider configuration
struct SliderConfig {
    float width = 200.0f;
    float min_value = 0.0f;
    float max_value = 1.0f;
    bool show_value = true;
    std::string format = "%.2f";
};

/// Value slider
class Slider {
public:
    /// Draw a horizontal slider and return interaction state
    static SliderResult draw(UiContext& ctx, float x, float y,
                             const std::string& label, float value,
                             const SliderConfig& config = {});

    /// Draw with ID for stable interaction
    static SliderResult draw(UiContext& ctx, std::uint64_t id, float x, float y,
                             const std::string& label, float value,
                             const SliderConfig& config = {});
};

// =============================================================================
// Text Input Widget
// =============================================================================

/// Text input result
struct TextInputResult {
    bool changed = false;
    bool submitted = false;  // Enter pressed
    std::string text;
    bool focused = false;
};

/// Text input configuration
struct TextInputConfig {
    float width = 200.0f;
    std::uint32_t max_length = 256;
    std::string placeholder = "";
    bool password = false;
    bool multiline = false;
};

/// Text input field
class TextInput {
public:
    /// Draw a text input field and return interaction state
    static TextInputResult draw(UiContext& ctx, float x, float y,
                                const std::string& text,
                                const TextInputConfig& config = {});

    /// Draw with ID for stable interaction
    static TextInputResult draw(UiContext& ctx, std::uint64_t id, float x, float y,
                                const std::string& text,
                                const TextInputConfig& config = {});
};

// =============================================================================
// Panel Widget
// =============================================================================

/// Panel configuration
struct PanelConfig {
    float width = 0.0f;       // 0 = auto
    float height = 0.0f;      // 0 = auto
    float padding = 0.0f;     // 0 = use theme
    bool show_border = true;
    bool show_title = false;
    std::string title = "";
    std::optional<Color> bg_color;
    std::optional<Color> border_color;
};

/// Container panel
class Panel {
public:
    /// Begin a panel - returns inner rect for content
    static Rect begin(UiContext& ctx, float x, float y, const PanelConfig& config);

    /// End the panel
    static void end(UiContext& ctx);
};

// =============================================================================
// Separator Widget
// =============================================================================

/// Horizontal separator line
class Separator {
public:
    /// Draw a horizontal separator at cursor, advances cursor
    static void draw(UiContext& ctx);

    /// Draw at specific position with width
    static void draw(UiContext& ctx, float x, float y, float width);
};

// =============================================================================
// Spacing Widget
// =============================================================================

/// Spacing/padding helper
class Spacing {
public:
    /// Add vertical spacing, advances cursor
    static void vertical(UiContext& ctx, float amount);

    /// Add horizontal spacing, advances cursor
    static void horizontal(UiContext& ctx, float amount);
};

} // namespace void_ui
