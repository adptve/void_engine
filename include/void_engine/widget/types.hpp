/// @file types.hpp
/// @brief Core types for the widget system

#pragma once

#include "fwd.hpp"

#include <algorithm>
#include <any>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_widget {

// =============================================================================
// Basic Math Types
// =============================================================================

struct Vec2 {
    float x{0};
    float y{0};

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }

    constexpr bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
    constexpr bool operator!=(const Vec2& other) const { return !(*this == other); }

    static constexpr Vec2 zero() { return {0, 0}; }
    static constexpr Vec2 one() { return {1, 1}; }
};

struct Rect {
    float x{0};
    float y{0};
    float width{0};
    float height{0};

    constexpr Rect() = default;
    constexpr Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), width(w_), height(h_) {}
    constexpr Rect(Vec2 pos, Vec2 size) : x(pos.x), y(pos.y), width(size.x), height(size.y) {}

    [[nodiscard]] constexpr Vec2 position() const { return {x, y}; }
    [[nodiscard]] constexpr Vec2 size() const { return {width, height}; }
    [[nodiscard]] constexpr Vec2 center() const { return {x + width / 2, y + height / 2}; }

    [[nodiscard]] constexpr float left() const { return x; }
    [[nodiscard]] constexpr float right() const { return x + width; }
    [[nodiscard]] constexpr float top() const { return y; }
    [[nodiscard]] constexpr float bottom() const { return y + height; }

    [[nodiscard]] constexpr bool contains(Vec2 point) const {
        return point.x >= x && point.x < x + width &&
               point.y >= y && point.y < y + height;
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const {
        return x < other.x + other.width && x + width > other.x &&
               y < other.y + other.height && y + height > other.y;
    }

    [[nodiscard]] constexpr Rect intersection(const Rect& other) const {
        // Parentheses prevent Windows min/max macro interference
        float nx = (std::max)(x, other.x);
        float ny = (std::max)(y, other.y);
        float nw = (std::min)(x + width, other.x + other.width) - nx;
        float nh = (std::min)(y + height, other.y + other.height) - ny;
        if (nw <= 0 || nh <= 0) return {};
        return {nx, ny, nw, nh};
    }

    static constexpr Rect zero() { return {0, 0, 0, 0}; }
};

struct Insets {
    float left{0};
    float top{0};
    float right{0};
    float bottom{0};

    constexpr Insets() = default;
    constexpr Insets(float all) : left(all), top(all), right(all), bottom(all) {}
    constexpr Insets(float horizontal, float vertical)
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
    constexpr Insets(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}

    [[nodiscard]] constexpr float horizontal() const { return left + right; }
    [[nodiscard]] constexpr float vertical() const { return top + bottom; }
};

// =============================================================================
// Color
// =============================================================================

struct Color {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}

    /// Create from 0-255 values
    static constexpr Color rgb8(std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_ = 255) {
        return {r_ / 255.0f, g_ / 255.0f, b_ / 255.0f, a_ / 255.0f};
    }

    /// Create from hex value (0xRRGGBB or 0xRRGGBBAA)
    static constexpr Color hex(std::uint32_t value) {
        if (value <= 0xFFFFFF) {
            return rgb8((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
        }
        return rgb8((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    }

    [[nodiscard]] constexpr Color with_alpha(float alpha) const { return {r, g, b, alpha}; }

    [[nodiscard]] static constexpr Color lerp(const Color& a, const Color& b, float t) {
        return {
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        };
    }

    // Predefined colors
    static constexpr Color white() { return {1, 1, 1, 1}; }
    static constexpr Color black() { return {0, 0, 0, 1}; }
    static constexpr Color red() { return {1, 0, 0, 1}; }
    static constexpr Color green() { return {0, 1, 0, 1}; }
    static constexpr Color blue() { return {0, 0, 1, 1}; }
    static constexpr Color yellow() { return {1, 1, 0, 1}; }
    static constexpr Color cyan() { return {0, 1, 1, 1}; }
    static constexpr Color magenta() { return {1, 0, 1, 1}; }
    static constexpr Color transparent() { return {0, 0, 0, 0}; }
    static constexpr Color gray(float v = 0.5f) { return {v, v, v, 1}; }
};

// =============================================================================
// Enumerations
// =============================================================================

/// Widget visibility state
enum class Visibility : std::uint8_t {
    Visible,    // Rendered and interactive
    Hidden,     // Not rendered but takes space
    Collapsed   // Not rendered, no space
};

/// Position interpretation mode
enum class PositionMode : std::uint8_t {
    Absolute,   // Position in screen coordinates
    Relative,   // Position relative to parent
    Anchored,   // Position from anchor point
    WorldSpace  // 3D world position projected to screen
};

/// Size interpretation mode
enum class SizeMode : std::uint8_t {
    Fixed,      // Exact pixel size
    Relative,   // Percentage of parent
    FitContent, // Shrink to content
    Fill        // Expand to fill parent
};

/// Anchor points (0-1 normalized coordinates)
enum class Anchor : std::uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

/// Text alignment
enum class TextAlign : std::uint8_t {
    Left,
    Center,
    Right,
    Justify
};

/// Vertical alignment
enum class VerticalAlign : std::uint8_t {
    Top,
    Middle,
    Bottom
};

/// Animation easing functions
enum class EasingType : std::uint8_t {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInElastic,
    EaseOutElastic,
    EaseInOutElastic,
    EaseInBounce,
    EaseOutBounce,
    EaseInOutBounce
};

/// Animation play mode
enum class PlayMode : std::uint8_t {
    Once,       // Play once and stop
    Loop,       // Loop continuously
    PingPong,   // Play forward then backward
    Reverse     // Play backward once
};

/// Animation state
enum class AnimState : std::uint8_t {
    Idle,
    Playing,
    Paused,
    Finished
};

/// Data binding mode
enum class BindingMode : std::uint8_t {
    OneWay,         // Source -> Target only
    TwoWay,         // Source <-> Target
    OneTime,        // Read once at bind time
    OneWayToSource  // Target -> Source only
};

/// Widget interaction state flags
enum class WidgetState : std::uint32_t {
    None     = 0,
    Hovered  = 1 << 0,
    Pressed  = 1 << 1,
    Focused  = 1 << 2,
    Disabled = 1 << 3,
    Checked  = 1 << 4,
    Selected = 1 << 5,
    Dragging = 1 << 6,
    Error    = 1 << 7
};

inline WidgetState operator|(WidgetState a, WidgetState b) {
    return static_cast<WidgetState>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline WidgetState operator&(WidgetState a, WidgetState b) {
    return static_cast<WidgetState>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_state(WidgetState flags, WidgetState test) {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(test)) != 0;
}

// =============================================================================
// Layout Data (defined before WidgetTemplate which uses it)
// =============================================================================

/// @brief Layout data for a widget
struct LayoutData {
    Vec2 position{0, 0};
    Vec2 size{100, 100};
    Vec2 anchor{0, 0};          // Anchor point (0-1)
    Vec2 pivot{0.5f, 0.5f};     // Rotation/scale pivot (0-1)
    Insets margin;
    Insets padding;
    PositionMode position_mode{PositionMode::Relative};
    SizeMode width_mode{SizeMode::Fixed};
    SizeMode height_mode{SizeMode::Fixed};
    float rotation{0};
    Vec2 scale{1, 1};
    float min_width{0};
    float min_height{0};
    float max_width{0};         // 0 = unlimited
    float max_height{0};        // 0 = unlimited
};

/// @brief Layout constraints for flexible sizing
struct LayoutConstraints {
    float min_width{0};
    float min_height{0};
    // Parentheses prevent Windows max macro interference
    float max_width{(std::numeric_limits<float>::max)()};
    float max_height{(std::numeric_limits<float>::max)()};
    float flex_grow{0};
    float flex_shrink{1};
    float flex_basis{0};        // 0 = auto
};

// =============================================================================
// Widget Instance
// =============================================================================

/// @brief Instance data for a widget
struct WidgetInstance {
    WidgetId id;
    std::string type;           // Widget type name (e.g., "button", "panel")
    std::string name;           // Optional instance name for lookup
    LayerId layer;              // Layer this widget belongs to
    Visibility visibility{Visibility::Visible};
    WidgetState state{WidgetState::None};
    bool interactive{true};     // Can receive input
    bool clip_children{false};  // Clip children to bounds

    // Custom properties (widget-specific data)
    std::unordered_map<std::string, std::any> properties;

    // Helper to get typed property
    template<typename T>
    T get_property(const std::string& key, const T& default_value = T{}) const {
        auto it = properties.find(key);
        if (it != properties.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {}
        }
        return default_value;
    }

    template<typename T>
    void set_property(const std::string& key, T&& value) {
        properties[key] = std::forward<T>(value);
    }
};

/// @brief Widget template for creating instances
struct WidgetTemplate {
    std::string type;
    std::string default_style;
    LayoutData default_layout;
    std::unordered_map<std::string, std::any> default_properties;
    std::vector<WidgetTemplate> children;
};

/// @brief Widget layer for z-ordering
struct WidgetLayer {
    LayerId id;
    std::string name;
    int z_order{0};
    float opacity{1.0f};
    bool visible{true};
    bool interactive{true};
    std::vector<WidgetId> widgets;
};

// =============================================================================
// Style Data
// =============================================================================

/// @brief Complete theme definition
struct Theme {
    std::string name;

    // Panel colors
    Color panel_background{Color::hex(0x1E1E1E)};
    Color panel_background_alt{Color::hex(0x252525)};
    Color panel_border{Color::hex(0x3C3C3C)};
    Color panel_header{Color::hex(0x2D2D2D)};

    // Text colors
    Color text_primary{Color::hex(0xE0E0E0)};
    Color text_secondary{Color::hex(0xA0A0A0)};
    Color text_disabled{Color::hex(0x606060)};
    Color text_highlight{Color::hex(0xFFFFFF)};

    // Interactive colors
    Color button_normal{Color::hex(0x3C3C3C)};
    Color button_hovered{Color::hex(0x505050)};
    Color button_pressed{Color::hex(0x606060)};
    Color button_disabled{Color::hex(0x2A2A2A)};

    // Accent colors
    Color accent_primary{Color::hex(0x007ACC)};
    Color accent_secondary{Color::hex(0x1E90FF)};
    Color accent_success{Color::hex(0x4EC9B0)};
    Color accent_warning{Color::hex(0xDCDCAA)};
    Color accent_error{Color::hex(0xF44747)};

    // Input colors
    Color input_background{Color::hex(0x3C3C3C)};
    Color input_border{Color::hex(0x5A5A5A)};
    Color input_border_focused{Color::hex(0x007ACC)};

    // Selection
    Color selection{Color::hex(0x264F78)};
    Color highlight{Color::hex(0x3A3D41)};

    // Scrollbar
    Color scrollbar_track{Color::hex(0x1E1E1E)};
    Color scrollbar_thumb{Color::hex(0x424242)};
    Color scrollbar_thumb_hovered{Color::hex(0x4F4F4F)};

    // Styling
    float text_size{14.0f};
    float line_height{1.4f};
    float border_width{1.0f};
    float border_radius{4.0f};
    float padding{8.0f};
    float spacing{4.0f};
    float animation_duration{0.15f};

    // Built-in themes
    static Theme dark();
    static Theme light();
    static Theme high_contrast();
};

/// @brief Style overrides for individual widgets
struct StyleOverrides {
    std::optional<Color> background_color;
    std::optional<Color> border_color;
    std::optional<Color> text_color;
    std::optional<float> border_width;
    std::optional<float> border_radius;
    std::optional<float> opacity;
    std::optional<std::string> font;
    std::optional<float> font_size;
};

/// @brief Computed style (theme + overrides)
struct ComputedStyle {
    Color background_color;
    Color border_color;
    Color text_color;
    float border_width{1};
    float border_radius{0};
    float opacity{1};
    std::string font{"default"};
    float font_size{14};
};

/// @brief Font data
struct FontData {
    std::string name;
    std::string path;
    float default_size{14};
    // Glyph data would be loaded at runtime
};

/// @brief Texture region for icons/sprites
struct TextureRegion {
    std::string texture_path;
    Rect uv_rect{0, 0, 1, 1};  // Normalized UV coordinates
    Vec2 size;                  // Original size in pixels
};

// =============================================================================
// Animation Data
// =============================================================================

/// @brief Single keyframe in an animation
struct Keyframe {
    float time{0};              // Time in seconds
    std::any value;             // Property value at this time
    EasingType easing{EasingType::Linear};
};

/// @brief Animation definition
struct AnimationDef {
    std::string name;
    std::string target_property; // Property to animate
    std::vector<Keyframe> keyframes;
    float duration{1.0f};
    PlayMode play_mode{PlayMode::Once};
    int repeat_count{1};        // -1 = infinite
    float delay{0};
};

/// @brief Active animation instance
struct ActiveAnimation {
    AnimationId id;
    std::string definition_name;
    std::string target_property;
    float elapsed{0};
    float duration{1.0f};
    PlayMode play_mode{PlayMode::Once};
    AnimState state{AnimState::Playing};
    std::vector<Keyframe> keyframes;
    int current_keyframe{0};
    int play_direction{1};      // 1 = forward, -1 = backward
    int loops_completed{0};
    int max_loops{1};
};

/// @brief Style transition
struct StyleTransition {
    std::string property;
    std::any from_value;
    std::any to_value;
    float elapsed{0};
    float duration{0.15f};
    EasingType easing{EasingType::EaseOutQuad};
};

// =============================================================================
// Data Binding
// =============================================================================

/// @brief Data binding configuration
struct DataBinding {
    BindingId id;
    WidgetId widget;
    std::string target_property;    // Widget property to update
    std::string source_path;        // Path in data source (e.g., "player.health")
    BindingMode mode{BindingMode::OneWay};
    std::string converter;          // Optional converter name
    std::any fallback_value;        // Value if source unavailable
    bool enabled{true};
};

/// @brief Binding update notification
struct BindingUpdate {
    BindingId binding;
    std::any old_value;
    std::any new_value;
};

/// @brief Callback for binding changes
using BindingCallback = std::function<void(const BindingUpdate&)>;

// =============================================================================
// Rendering Data
// =============================================================================

/// @brief UI vertex for GPU rendering
struct UiVertex {
    float x, y;         // Position
    float u, v;         // Texture coordinates
    float r, g, b, a;   // Color
};

/// @brief Draw command types
enum class DrawCommandType : std::uint8_t {
    Rect,
    RectOutline,
    RoundedRect,
    Text,
    Texture,
    Line,
    Scissor,
    ScissorPop
};

/// @brief Single draw command
struct DrawCommand {
    DrawCommandType type;
    Rect bounds;
    Color color;
    float param1{0};    // border_width, radius, etc.
    float param2{0};
    std::string text;
    std::string texture;
    Rect texture_rect;  // Source rect for texture
};

/// @brief List of draw commands for a layer
struct DrawCommandList {
    LayerId layer;
    std::vector<DrawCommand> commands;
    std::vector<UiVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// =============================================================================
// Events
// =============================================================================

/// @brief Widget event types
enum class WidgetEventType : std::uint8_t {
    Click,
    DoubleClick,
    HoverEnter,
    HoverExit,
    Focus,
    Blur,
    KeyDown,
    KeyUp,
    TextInput,
    DragStart,
    Drag,
    DragEnd,
    Scroll,
    ValueChanged,
    SelectionChanged
};

/// @brief Widget event data
struct WidgetEvent {
    WidgetEventType type;
    WidgetId widget;
    Vec2 position;      // Mouse position
    Vec2 delta;         // Movement delta
    int button{0};      // Mouse button
    int key{0};         // Key code
    int modifiers{0};   // Key modifiers
    std::string text;   // Text input
    std::any value;     // Generic value for ValueChanged
};

/// @brief Event callback
using WidgetEventCallback = std::function<void(const WidgetEvent&)>;

// =============================================================================
// Utility Functions
// =============================================================================

/// Convert anchor enum to normalized coordinates
inline Vec2 anchor_to_vec2(Anchor anchor) {
    switch (anchor) {
        case Anchor::TopLeft:       return {0.0f, 0.0f};
        case Anchor::TopCenter:     return {0.5f, 0.0f};
        case Anchor::TopRight:      return {1.0f, 0.0f};
        case Anchor::CenterLeft:    return {0.0f, 0.5f};
        case Anchor::Center:        return {0.5f, 0.5f};
        case Anchor::CenterRight:   return {1.0f, 0.5f};
        case Anchor::BottomLeft:    return {0.0f, 1.0f};
        case Anchor::BottomCenter:  return {0.5f, 1.0f};
        case Anchor::BottomRight:   return {1.0f, 1.0f};
    }
    return {0.0f, 0.0f};
}

/// Apply easing function
float apply_easing(float t, EasingType easing);

} // namespace void_widget
