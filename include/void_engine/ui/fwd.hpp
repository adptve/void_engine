#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_ui

#include <cstdint>

namespace void_ui {

// Color types
struct Color;
struct ThemeColors;
struct Theme;
class ThemeRegistry;

// Font types
struct Glyph;
class BitmapFont;
class FontRegistry;

// Layout types
struct Rect;
struct Size;
struct Point;
enum class Anchor : std::uint8_t;
enum class Alignment : std::uint8_t;
struct LayoutConstraints;

// Vertex types
struct UiVertex;
struct UiUniforms;
struct UiDrawData;
struct UiDrawCommand;

// Context
class UiContext;

// Widget types
enum class StatType : std::uint8_t;
enum class ToastType : std::uint8_t;
class Widget;
class DebugPanel;
class Label;
class ProgressBar;
class FrameTimeGraph;
class Toast;
class HelpModal;
class Button;
class Checkbox;
class Slider;
class TextInput;
class Panel;

// Renderer
class IUiRenderer;
class NullUiRenderer;

// Events
struct UiEvent;
struct ClickEvent;
struct HoverEvent;
struct FocusEvent;

} // namespace void_ui
