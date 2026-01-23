#pragma once

/// @file types.hpp
/// @brief Core types for void_ui

#include "fwd.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace void_ui {

// =============================================================================
// Color
// =============================================================================

/// RGBA color (0.0-1.0 range)
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    constexpr Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    /// Create from 0-255 integer values
    static constexpr Color from_rgb8(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
        return Color{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    /// Create from hex value (0xRRGGBB or 0xRRGGBBAA)
    static constexpr Color from_hex(std::uint32_t hex) {
        if (hex > 0xFFFFFF) {
            // Has alpha
            return Color{
                ((hex >> 24) & 0xFF) / 255.0f,
                ((hex >> 16) & 0xFF) / 255.0f,
                ((hex >> 8) & 0xFF) / 255.0f,
                (hex & 0xFF) / 255.0f,
            };
        }
        return Color{
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f,
            1.0f,
        };
    }

    /// Convert to array
    [[nodiscard]] constexpr std::array<float, 4> to_array() const {
        return {r, g, b, a};
    }

    /// Interpolate between two colors
    [[nodiscard]] static Color lerp(const Color& a, const Color& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color{
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t,
        };
    }

    /// Brighten color
    [[nodiscard]] Color brighten(float amount) const {
        return Color{
            std::min(1.0f, r + amount),
            std::min(1.0f, g + amount),
            std::min(1.0f, b + amount),
            a,
        };
    }

    /// Darken color
    [[nodiscard]] Color darken(float amount) const {
        return Color{
            std::max(0.0f, r - amount),
            std::max(0.0f, g - amount),
            std::max(0.0f, b - amount),
            a,
        };
    }

    /// With different alpha
    [[nodiscard]] constexpr Color with_alpha(float new_alpha) const {
        return Color{r, g, b, new_alpha};
    }

    // Common colors
    static constexpr Color white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static constexpr Color black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static constexpr Color red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static constexpr Color green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static constexpr Color blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static constexpr Color yellow() { return {1.0f, 1.0f, 0.0f, 1.0f}; }
    static constexpr Color cyan() { return {0.0f, 1.0f, 1.0f, 1.0f}; }
    static constexpr Color magenta() { return {1.0f, 0.0f, 1.0f, 1.0f}; }
    static constexpr Color transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
};

// =============================================================================
// Layout Types
// =============================================================================

/// 2D point
struct Point {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Point() = default;
    constexpr Point(float x_, float y_) : x(x_), y(y_) {}

    constexpr Point operator+(const Point& other) const { return {x + other.x, y + other.y}; }
    constexpr Point operator-(const Point& other) const { return {x - other.x, y - other.y}; }
    constexpr Point operator*(float s) const { return {x * s, y * s}; }
};

/// 2D size
struct Size {
    float width = 0.0f;
    float height = 0.0f;

    constexpr Size() = default;
    constexpr Size(float w, float h) : width(w), height(h) {}

    [[nodiscard]] constexpr float area() const { return width * height; }
    [[nodiscard]] constexpr bool is_empty() const { return width <= 0.0f || height <= 0.0f; }
};

/// Rectangle
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr Rect() = default;
    constexpr Rect(float x_, float y_, float w, float h) : x(x_), y(y_), width(w), height(h) {}
    constexpr Rect(Point pos, Size size) : x(pos.x), y(pos.y), width(size.width), height(size.height) {}

    [[nodiscard]] constexpr Point position() const { return {x, y}; }
    [[nodiscard]] constexpr Size size() const { return {width, height}; }

    [[nodiscard]] constexpr float left() const { return x; }
    [[nodiscard]] constexpr float right() const { return x + width; }
    [[nodiscard]] constexpr float top() const { return y; }
    [[nodiscard]] constexpr float bottom() const { return y + height; }

    [[nodiscard]] constexpr Point center() const {
        return {x + width / 2.0f, y + height / 2.0f};
    }

    [[nodiscard]] constexpr bool contains(Point p) const {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }

    [[nodiscard]] constexpr bool contains(float px, float py) const {
        return contains(Point{px, py});
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const {
        return !(right() < other.left() || left() > other.right() ||
                 bottom() < other.top() || top() > other.bottom());
    }

    /// Expand rect by padding
    [[nodiscard]] constexpr Rect expand(float padding) const {
        return {x - padding, y - padding, width + padding * 2, height + padding * 2};
    }

    /// Shrink rect by padding
    [[nodiscard]] constexpr Rect shrink(float padding) const {
        return {x + padding, y + padding, width - padding * 2, height - padding * 2};
    }
};

/// Anchor point for positioning
enum class Anchor : std::uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

/// Text alignment
enum class Alignment : std::uint8_t {
    Left,
    Center,
    Right,
};

/// Layout constraints
struct LayoutConstraints {
    float min_width = 0.0f;
    float max_width = 10000.0f;
    float min_height = 0.0f;
    float max_height = 10000.0f;

    [[nodiscard]] Size constrain(Size size) const {
        return Size{
            std::clamp(size.width, min_width, max_width),
            std::clamp(size.height, min_height, max_height),
        };
    }
};

// =============================================================================
// Vertex Types (for GPU rendering)
// =============================================================================

/// Vertex format for UI rendering
struct UiVertex {
    float position[2];
    float uv[2];
    float color[4];
};

/// Uniform buffer for UI shader
struct UiUniforms {
    float screen_size[2];
    float _padding[2];
};

/// Draw command for batched rendering
struct UiDrawCommand {
    std::uint32_t vertex_offset = 0;
    std::uint32_t index_offset = 0;
    std::uint32_t index_count = 0;
    std::uint64_t texture_id = 0;  // 0 = no texture
    Rect clip_rect;
};

/// UI draw data (vertices + indices + commands)
struct UiDrawData {
    std::vector<UiVertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<UiDrawCommand> commands;

    void clear() {
        vertices.clear();
        indices.clear();
        commands.clear();
    }

    [[nodiscard]] bool empty() const {
        return vertices.empty() || indices.empty();
    }
};

// =============================================================================
// Stat/Toast Types
// =============================================================================

/// Type of statistic (affects color)
enum class StatType : std::uint8_t {
    Normal,   // Normal text color
    Good,     // Good/positive (green)
    Warning,  // Warning (yellow)
    Bad,      // Bad/error (red)
    Info,     // Informational (blue)
};

/// Toast notification type
enum class ToastType : std::uint8_t {
    Info,
    Success,
    Warning,
    Error,
};

// =============================================================================
// UI Events
// =============================================================================

/// Click event
struct ClickEvent {
    Point position;
    std::uint32_t button = 0;
    bool double_click = false;
};

/// Hover event
struct HoverEvent {
    Point position;
    bool entered = false;
    bool exited = false;
};

/// Focus event
struct FocusEvent {
    bool gained = false;
    bool lost = false;
};

/// UI event variant
struct UiEvent {
    enum class Type : std::uint8_t {
        None,
        Click,
        Hover,
        Focus,
        KeyPress,
        TextInput,
    };

    Type type = Type::None;
    std::uint64_t widget_id = 0;

    union {
        ClickEvent click;
        HoverEvent hover;
        FocusEvent focus;
    };

    UiEvent() : click{} {}
};

} // namespace void_ui
