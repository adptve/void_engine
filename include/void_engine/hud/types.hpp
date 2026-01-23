/// @file types.hpp
/// @brief Core types and enumerations for void_hud module

#pragma once

#include "fwd.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_hud {

// =============================================================================
// Element Types
// =============================================================================

/// @brief Type of HUD element
enum class HudElementType : std::uint8_t {
    Panel,
    Text,
    ProgressBar,
    Icon,
    Image,
    Button,
    Minimap,
    Crosshair,
    Compass,
    ObjectiveMarker,
    DamageIndicator,
    Notification,
    Tooltip,
    Container,
    Custom
};

/// @brief Anchor point for positioning
enum class AnchorPoint : std::uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

/// @brief Position mode
enum class PositionMode : std::uint8_t {
    Absolute,       ///< Pixel coordinates
    Relative,       ///< Percentage of parent
    Anchored,       ///< Relative to anchor point
    WorldSpace      ///< Projected from 3D position
};

/// @brief Size mode
enum class SizeMode : std::uint8_t {
    Fixed,          ///< Fixed pixel size
    Relative,       ///< Percentage of parent
    FitContent,     ///< Size to content
    Fill            ///< Fill available space
};

/// @brief Element visibility
enum class Visibility : std::uint8_t {
    Visible,
    Hidden,
    Collapsed
};

// =============================================================================
// Animation Types
// =============================================================================

/// @brief Animation property type
enum class AnimProperty : std::uint8_t {
    PositionX,
    PositionY,
    Width,
    Height,
    Opacity,
    Rotation,
    Scale,
    ScaleX,
    ScaleY,
    Color,
    Custom
};

/// @brief Easing function type
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
    EaseInOutBounce,
    Custom
};

/// @brief Animation state
enum class AnimationState : std::uint8_t {
    Idle,
    Playing,
    Paused,
    Finished
};

/// @brief Animation play mode
enum class PlayMode : std::uint8_t {
    Once,
    Loop,
    PingPong,
    Reverse
};

// =============================================================================
// Binding Types
// =============================================================================

/// @brief Data binding mode
enum class BindingMode : std::uint8_t {
    OneWay,         ///< Source to target only
    TwoWay,         ///< Bidirectional
    OneTime,        ///< Initial value only
    OneWayToSource  ///< Target to source only
};

/// @brief Value converter for bindings
enum class ConverterType : std::uint8_t {
    None,
    ToString,
    ToInt,
    ToFloat,
    ToBool,
    Format,
    Clamp,
    Normalize,
    Custom
};

// =============================================================================
// Progress Bar Types
// =============================================================================

/// @brief Progress bar style
enum class ProgressStyle : std::uint8_t {
    Horizontal,
    Vertical,
    Circular,
    Segmented
};

/// @brief Progress bar fill direction
enum class FillDirection : std::uint8_t {
    LeftToRight,
    RightToLeft,
    BottomToTop,
    TopToBottom,
    Clockwise,
    CounterClockwise
};

// =============================================================================
// Notification Types
// =============================================================================

/// @brief Notification type
enum class NotificationType : std::uint8_t {
    Info,
    Warning,
    Error,
    Success,
    Achievement,
    Quest,
    Item,
    System
};

/// @brief Notification position
enum class NotificationPosition : std::uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Center
};

// =============================================================================
// Basic Structures
// =============================================================================

/// @brief 2D vector
struct Vec2 {
    float x{0}, y{0};

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    Vec2 operator/(float scalar) const { return {x / scalar, y / scalar}; }
};

/// @brief 3D vector (for world-space HUD)
struct Vec3 {
    float x{0}, y{0}, z{0};
};

/// @brief Rectangle
struct Rect {
    float x{0}, y{0};
    float width{0}, height{0};

    bool contains(const Vec2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }

    Vec2 center() const { return {x + width / 2, y + height / 2}; }
};

/// @brief Color with alpha
struct Color {
    float r{1}, g{1}, b{1}, a{1};

    Color() = default;
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    static Color white() { return {1, 1, 1, 1}; }
    static Color black() { return {0, 0, 0, 1}; }
    static Color red() { return {1, 0, 0, 1}; }
    static Color green() { return {0, 1, 0, 1}; }
    static Color blue() { return {0, 0, 1, 1}; }
    static Color yellow() { return {1, 1, 0, 1}; }
    static Color transparent() { return {0, 0, 0, 0}; }

    Color with_alpha(float alpha) const { return {r, g, b, alpha}; }

    Color lerp(const Color& other, float t) const {
        return {
            r + (other.r - r) * t,
            g + (other.g - g) * t,
            b + (other.b - b) * t,
            a + (other.a - a) * t
        };
    }
};

/// @brief Margin/padding
struct Insets {
    float left{0}, top{0}, right{0}, bottom{0};

    Insets() = default;
    Insets(float all) : left(all), top(all), right(all), bottom(all) {}
    Insets(float horizontal, float vertical)
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}
    Insets(float left, float top, float right, float bottom)
        : left(left), top(top), right(right), bottom(bottom) {}
};

// =============================================================================
// Element Properties
// =============================================================================

/// @brief Base element properties
struct ElementProperties {
    std::string name;
    HudElementType type{HudElementType::Panel};
    Visibility visibility{Visibility::Visible};

    // Transform
    Vec2 position;
    Vec2 size;
    float rotation{0};
    Vec2 scale{1, 1};
    Vec2 pivot{0.5f, 0.5f};

    // Positioning
    PositionMode position_mode{PositionMode::Absolute};
    SizeMode width_mode{SizeMode::Fixed};
    SizeMode height_mode{SizeMode::Fixed};
    AnchorPoint anchor{AnchorPoint::TopLeft};
    Vec2 anchor_offset;

    // Appearance
    Color color{Color::white()};
    float opacity{1.0f};
    Insets margin;
    Insets padding;

    // Interaction
    bool interactive{false};
    bool clips_children{false};

    // Z-order
    std::int32_t z_order{0};
};

/// @brief Text properties
struct TextProperties {
    std::string text;
    std::string font_name{"default"};
    float font_size{16.0f};
    Color text_color{Color::white()};
    bool bold{false};
    bool italic{false};
    bool wrap{false};
    float line_height{1.2f};

    enum class Alignment : std::uint8_t {
        Left, Center, Right
    } alignment{Alignment::Left};

    enum class VerticalAlignment : std::uint8_t {
        Top, Middle, Bottom
    } vertical_alignment{VerticalAlignment::Top};

    // Shadow
    bool has_shadow{false};
    Vec2 shadow_offset{2, 2};
    Color shadow_color{Color::black().with_alpha(0.5f)};

    // Outline
    bool has_outline{false};
    float outline_width{1.0f};
    Color outline_color{Color::black()};
};

/// @brief Progress bar properties
struct ProgressBarProperties {
    float value{0};           ///< Current value (0-1)
    float max_value{1.0f};
    float min_value{0};

    ProgressStyle style{ProgressStyle::Horizontal};
    FillDirection fill_direction{FillDirection::LeftToRight};

    Color background_color{Color(0.2f, 0.2f, 0.2f, 0.8f)};
    Color fill_color{Color::green()};
    Color border_color{Color::white()};
    float border_width{1.0f};

    // Segmented style
    std::uint32_t segments{1};
    float segment_gap{2.0f};

    // Animation
    bool animate_changes{true};
    float animation_speed{5.0f};

    // Display
    bool show_text{false};
    std::string text_format{"{0:.0%}"};
};

/// @brief Icon properties
struct IconProperties {
    std::string texture_path;
    Rect texture_rect;      ///< For sprite sheets
    Color tint{Color::white()};
    bool preserve_aspect{true};
};

// =============================================================================
// Animation Structures
// =============================================================================

/// @brief Animation keyframe
struct Keyframe {
    float time{0};
    float value{0};
    EasingType easing{EasingType::Linear};
};

/// @brief Animation definition
struct AnimationDef {
    HudAnimationId id;
    std::string name;
    AnimProperty property{AnimProperty::Opacity};
    std::vector<Keyframe> keyframes;
    float duration{1.0f};
    PlayMode play_mode{PlayMode::Once};
    float delay{0};
    std::uint32_t repeat_count{1}; ///< 0 = infinite
};

/// @brief Transition definition
struct TransitionDef {
    AnimProperty property{AnimProperty::Opacity};
    float duration{0.3f};
    EasingType easing{EasingType::EaseOutQuad};
    float delay{0};
};

// =============================================================================
// Notification Structure
// =============================================================================

/// @brief Notification definition
struct NotificationDef {
    std::string title;
    std::string message;
    std::string icon_path;
    NotificationType type{NotificationType::Info};
    float duration{3.0f};      ///< 0 = manual dismiss
    bool dismissable{true};
    std::function<void()> on_click;
    std::unordered_map<std::string, std::string> custom_data;
};

// =============================================================================
// Damage Indicator
// =============================================================================

/// @brief Damage indicator definition
struct DamageIndicatorDef {
    float direction{0};        ///< Angle in radians (0 = front)
    float intensity{1.0f};     ///< Damage amount normalized
    Color color{Color::red()};
    float duration{0.5f};
    bool is_critical{false};
};

// =============================================================================
// Objective Marker
// =============================================================================

/// @brief Objective marker definition
struct ObjectiveMarkerDef {
    std::uint64_t objective_id{0};
    std::string label;
    std::string icon_path;
    Vec3 world_position;
    float distance{0};         ///< Distance to marker
    bool show_distance{true};
    bool clamp_to_screen{true};
    Color color{Color::yellow()};
};

// =============================================================================
// Configuration
// =============================================================================

/// @brief HUD system configuration
struct HudConfig {
    // Screen
    float reference_width{1920.0f};
    float reference_height{1080.0f};
    bool scale_with_screen{true};
    float min_scale{0.5f};
    float max_scale{2.0f};

    // Performance
    bool enable_batching{true};
    std::uint32_t max_visible_elements{1000};
    bool cull_offscreen{true};

    // Animation
    float default_transition_duration{0.3f};
    EasingType default_easing{EasingType::EaseOutQuad};

    // Notifications
    NotificationPosition notification_position{NotificationPosition::TopRight};
    std::uint32_t max_notifications{5};
    float notification_spacing{10.0f};

    // Tooltips
    float tooltip_delay{0.5f};
    Vec2 tooltip_offset{10, 10};

    // Debug
    bool show_bounds{false};
    bool show_anchors{false};
};

// =============================================================================
// Callback Types
// =============================================================================

using ElementCallback = std::function<void(HudElementId)>;
using ValueChangeCallback = std::function<void(const std::any& old_value, const std::any& new_value)>;
using AnimationCallback = std::function<void(HudAnimationId)>;
using NotificationCallback = std::function<void(const NotificationDef&)>;

} // namespace void_hud
