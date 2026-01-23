#pragma once

/// @file debug.hpp
/// @brief Debug visualization and statistics for void_render

#include "fwd.hpp"
#include "spatial.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <cmath>
#include <numbers>

namespace void_render {

// =============================================================================
// DebugColor
// =============================================================================

/// Predefined debug colors
namespace debug_color {
    constexpr std::array<float, 4> Red     = {1.0f, 0.0f, 0.0f, 1.0f};
    constexpr std::array<float, 4> Green   = {0.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 4> Blue    = {0.0f, 0.0f, 1.0f, 1.0f};
    constexpr std::array<float, 4> Yellow  = {1.0f, 1.0f, 0.0f, 1.0f};
    constexpr std::array<float, 4> Cyan    = {0.0f, 1.0f, 1.0f, 1.0f};
    constexpr std::array<float, 4> Magenta = {1.0f, 0.0f, 1.0f, 1.0f};
    constexpr std::array<float, 4> White   = {1.0f, 1.0f, 1.0f, 1.0f};
    constexpr std::array<float, 4> Black   = {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr std::array<float, 4> Gray    = {0.5f, 0.5f, 0.5f, 1.0f};
    constexpr std::array<float, 4> Orange  = {1.0f, 0.5f, 0.0f, 1.0f};
    constexpr std::array<float, 4> Purple  = {0.5f, 0.0f, 0.5f, 1.0f};
    constexpr std::array<float, 4> Pink    = {1.0f, 0.4f, 0.7f, 1.0f};
}

// =============================================================================
// DebugVertex
// =============================================================================

/// Vertex for debug rendering
struct DebugVertex {
    std::array<float, 3> position = {0, 0, 0};
    std::array<float, 4> color = {1, 1, 1, 1};

    DebugVertex() = default;
    DebugVertex(const std::array<float, 3>& pos, const std::array<float, 4>& col)
        : position(pos), color(col) {}
    DebugVertex(float x, float y, float z, const std::array<float, 4>& col)
        : position{x, y, z}, color(col) {}
};

// =============================================================================
// DebugLine
// =============================================================================

/// Debug line segment
struct DebugLine {
    DebugVertex start;
    DebugVertex end;
    float duration = 0.0f;  // 0 = single frame

    DebugLine() = default;
    DebugLine(const std::array<float, 3>& s, const std::array<float, 3>& e,
              const std::array<float, 4>& color, float dur = 0.0f)
        : start(s, color), end(e, color), duration(dur) {}
};

// =============================================================================
// DebugTriangle
// =============================================================================

/// Debug triangle (for filled shapes)
struct DebugTriangle {
    std::array<DebugVertex, 3> vertices;
    float duration = 0.0f;

    DebugTriangle() = default;
    DebugTriangle(const std::array<float, 3>& a, const std::array<float, 3>& b,
                  const std::array<float, 3>& c, const std::array<float, 4>& color,
                  float dur = 0.0f)
        : vertices{DebugVertex(a, color), DebugVertex(b, color), DebugVertex(c, color)}
        , duration(dur) {}
};

// =============================================================================
// DebugText
// =============================================================================

/// Debug text for screen-space rendering
struct DebugText {
    std::string text;
    std::array<float, 2> position = {0, 0};  // Screen position (pixels)
    std::array<float, 4> color = debug_color::White;
    float scale = 1.0f;
    float duration = 0.0f;
    bool world_space = false;
    std::array<float, 3> world_position = {0, 0, 0};
};

// =============================================================================
// DebugDrawFlags
// =============================================================================

/// Debug draw flags
enum class DebugDrawFlags : std::uint32_t {
    None            = 0,
    DepthTest       = 1 << 0,   // Test against depth buffer
    DepthWrite      = 1 << 1,   // Write to depth buffer
    Wireframe       = 1 << 2,   // Draw as wireframe
    DoubleSided     = 1 << 3,   // Don't cull back faces
    Persistent      = 1 << 4,   // Don't clear after frame
    ScreenSpace     = 1 << 5,   // Draw in screen space
};

inline DebugDrawFlags operator|(DebugDrawFlags a, DebugDrawFlags b) {
    return static_cast<DebugDrawFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline bool has_flag(DebugDrawFlags flags, DebugDrawFlags flag) {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

// =============================================================================
// DebugRenderer
// =============================================================================

/// Immediate-mode debug renderer
class DebugRenderer {
public:
    /// Constructor
    DebugRenderer() {
        m_lines.reserve(1024);
        m_triangles.reserve(256);
        m_texts.reserve(64);
    }

    // -------------------------------------------------------------------------
    // Line Drawing
    // -------------------------------------------------------------------------

    /// Draw line
    void line(const std::array<float, 3>& start, const std::array<float, 3>& end,
              const std::array<float, 4>& color = debug_color::White,
              float duration = 0.0f) {
        m_lines.emplace_back(start, end, color, duration);
    }

    /// Draw ray
    void ray(const Ray& r, float length, const std::array<float, 4>& color = debug_color::Yellow,
             float duration = 0.0f) {
        auto end = r.at(length);
        line(r.origin, end, color, duration);
    }

    /// Draw arrow
    void arrow(const std::array<float, 3>& start, const std::array<float, 3>& end,
               const std::array<float, 4>& color = debug_color::White,
               float head_size = 0.1f, float duration = 0.0f) {
        line(start, end, color, duration);

        // Calculate arrow head
        std::array<float, 3> dir = {
            end[0] - start[0],
            end[1] - start[1],
            end[2] - start[2]
        };
        float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        if (len > 1e-6f) {
            dir[0] /= len; dir[1] /= len; dir[2] /= len;
        }

        // Find perpendicular vectors
        std::array<float, 3> perp1 = {-dir[1], dir[0], 0};
        float perp_len = std::sqrt(perp1[0] * perp1[0] + perp1[1] * perp1[1]);
        if (perp_len < 0.1f) {
            perp1 = {0, -dir[2], dir[1]};
            perp_len = std::sqrt(perp1[1] * perp1[1] + perp1[2] * perp1[2]);
        }
        if (perp_len > 1e-6f) {
            perp1[0] /= perp_len; perp1[1] /= perp_len; perp1[2] /= perp_len;
        }

        std::array<float, 3> base = {
            end[0] - dir[0] * head_size,
            end[1] - dir[1] * head_size,
            end[2] - dir[2] * head_size
        };

        std::array<float, 3> tip1 = {
            base[0] + perp1[0] * head_size * 0.5f,
            base[1] + perp1[1] * head_size * 0.5f,
            base[2] + perp1[2] * head_size * 0.5f
        };
        std::array<float, 3> tip2 = {
            base[0] - perp1[0] * head_size * 0.5f,
            base[1] - perp1[1] * head_size * 0.5f,
            base[2] - perp1[2] * head_size * 0.5f
        };

        line(end, tip1, color, duration);
        line(end, tip2, color, duration);
    }

    // -------------------------------------------------------------------------
    // Shape Drawing
    // -------------------------------------------------------------------------

    /// Draw AABB wireframe
    void aabb(const AABB& box, const std::array<float, 4>& color = debug_color::Green,
              float duration = 0.0f) {
        auto& min = box.min;
        auto& max = box.max;

        // Bottom face
        line({min[0], min[1], min[2]}, {max[0], min[1], min[2]}, color, duration);
        line({max[0], min[1], min[2]}, {max[0], min[1], max[2]}, color, duration);
        line({max[0], min[1], max[2]}, {min[0], min[1], max[2]}, color, duration);
        line({min[0], min[1], max[2]}, {min[0], min[1], min[2]}, color, duration);

        // Top face
        line({min[0], max[1], min[2]}, {max[0], max[1], min[2]}, color, duration);
        line({max[0], max[1], min[2]}, {max[0], max[1], max[2]}, color, duration);
        line({max[0], max[1], max[2]}, {min[0], max[1], max[2]}, color, duration);
        line({min[0], max[1], max[2]}, {min[0], max[1], min[2]}, color, duration);

        // Vertical edges
        line({min[0], min[1], min[2]}, {min[0], max[1], min[2]}, color, duration);
        line({max[0], min[1], min[2]}, {max[0], max[1], min[2]}, color, duration);
        line({max[0], min[1], max[2]}, {max[0], max[1], max[2]}, color, duration);
        line({min[0], min[1], max[2]}, {min[0], max[1], max[2]}, color, duration);
    }

    /// Draw sphere wireframe
    void sphere(const std::array<float, 3>& center, float radius,
                const std::array<float, 4>& color = debug_color::Cyan,
                std::uint32_t segments = 16, float duration = 0.0f) {
        // Draw three circles (XY, XZ, YZ planes)
        circle_xy(center, radius, color, segments, duration);
        circle_xz(center, radius, color, segments, duration);
        circle_yz(center, radius, color, segments, duration);
    }

    /// Draw circle in XY plane
    void circle_xy(const std::array<float, 3>& center, float radius,
                   const std::array<float, 4>& color = debug_color::White,
                   std::uint32_t segments = 32, float duration = 0.0f) {
        float angle_step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segments);
        for (std::uint32_t i = 0; i < segments; ++i) {
            float a1 = static_cast<float>(i) * angle_step;
            float a2 = static_cast<float>(i + 1) * angle_step;
            std::array<float, 3> p1 = {
                center[0] + std::cos(a1) * radius,
                center[1] + std::sin(a1) * radius,
                center[2]
            };
            std::array<float, 3> p2 = {
                center[0] + std::cos(a2) * radius,
                center[1] + std::sin(a2) * radius,
                center[2]
            };
            line(p1, p2, color, duration);
        }
    }

    /// Draw circle in XZ plane (horizontal)
    void circle_xz(const std::array<float, 3>& center, float radius,
                   const std::array<float, 4>& color = debug_color::White,
                   std::uint32_t segments = 32, float duration = 0.0f) {
        float angle_step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segments);
        for (std::uint32_t i = 0; i < segments; ++i) {
            float a1 = static_cast<float>(i) * angle_step;
            float a2 = static_cast<float>(i + 1) * angle_step;
            std::array<float, 3> p1 = {
                center[0] + std::cos(a1) * radius,
                center[1],
                center[2] + std::sin(a1) * radius
            };
            std::array<float, 3> p2 = {
                center[0] + std::cos(a2) * radius,
                center[1],
                center[2] + std::sin(a2) * radius
            };
            line(p1, p2, color, duration);
        }
    }

    /// Draw circle in YZ plane
    void circle_yz(const std::array<float, 3>& center, float radius,
                   const std::array<float, 4>& color = debug_color::White,
                   std::uint32_t segments = 32, float duration = 0.0f) {
        float angle_step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segments);
        for (std::uint32_t i = 0; i < segments; ++i) {
            float a1 = static_cast<float>(i) * angle_step;
            float a2 = static_cast<float>(i + 1) * angle_step;
            std::array<float, 3> p1 = {
                center[0],
                center[1] + std::cos(a1) * radius,
                center[2] + std::sin(a1) * radius
            };
            std::array<float, 3> p2 = {
                center[0],
                center[1] + std::cos(a2) * radius,
                center[2] + std::sin(a2) * radius
            };
            line(p1, p2, color, duration);
        }
    }

    /// Draw frustum wireframe
    void frustum(const Frustum& f, const std::array<float, 4>& color = debug_color::Yellow,
                 float duration = 0.0f) {
        (void)f; (void)color; (void)duration;
        // TODO: Calculate frustum corners and draw
    }

    /// Draw grid
    void grid(const std::array<float, 3>& center, float size, std::uint32_t divisions,
              const std::array<float, 4>& color = debug_color::Gray,
              float duration = 0.0f) {
        float half_size = size * 0.5f;
        float step = size / static_cast<float>(divisions);

        for (std::uint32_t i = 0; i <= divisions; ++i) {
            float offset = -half_size + static_cast<float>(i) * step;

            // X-parallel lines
            line(
                {center[0] + offset, center[1], center[2] - half_size},
                {center[0] + offset, center[1], center[2] + half_size},
                color, duration
            );

            // Z-parallel lines
            line(
                {center[0] - half_size, center[1], center[2] + offset},
                {center[0] + half_size, center[1], center[2] + offset},
                color, duration
            );
        }
    }

    /// Draw coordinate axes
    void axes(const std::array<float, 3>& origin, float size = 1.0f, float duration = 0.0f) {
        arrow(origin, {origin[0] + size, origin[1], origin[2]}, debug_color::Red, size * 0.1f, duration);
        arrow(origin, {origin[0], origin[1] + size, origin[2]}, debug_color::Green, size * 0.1f, duration);
        arrow(origin, {origin[0], origin[1], origin[2] + size}, debug_color::Blue, size * 0.1f, duration);
    }

    /// Draw point (as small cross)
    void point(const std::array<float, 3>& pos, const std::array<float, 4>& color = debug_color::White,
               float size = 0.05f, float duration = 0.0f) {
        line({pos[0] - size, pos[1], pos[2]}, {pos[0] + size, pos[1], pos[2]}, color, duration);
        line({pos[0], pos[1] - size, pos[2]}, {pos[0], pos[1] + size, pos[2]}, color, duration);
        line({pos[0], pos[1], pos[2] - size}, {pos[0], pos[1], pos[2] + size}, color, duration);
    }

    // -------------------------------------------------------------------------
    // Text Drawing
    // -------------------------------------------------------------------------

    /// Draw screen-space text
    void text(const std::string& str, float x, float y,
              const std::array<float, 4>& color = debug_color::White,
              float scale = 1.0f, float duration = 0.0f) {
        DebugText dt;
        dt.text = str;
        dt.position = {x, y};
        dt.color = color;
        dt.scale = scale;
        dt.duration = duration;
        dt.world_space = false;
        m_texts.push_back(dt);
    }

    /// Draw world-space text (billboard)
    void text_3d(const std::string& str, const std::array<float, 3>& world_pos,
                 const std::array<float, 4>& color = debug_color::White,
                 float scale = 1.0f, float duration = 0.0f) {
        DebugText dt;
        dt.text = str;
        dt.world_position = world_pos;
        dt.color = color;
        dt.scale = scale;
        dt.duration = duration;
        dt.world_space = true;
        m_texts.push_back(dt);
    }

    // -------------------------------------------------------------------------
    // BVH Visualization
    // -------------------------------------------------------------------------

    /// Draw BVH nodes
    void bvh(const BVH& bvh_tree, std::uint32_t max_depth = UINT32_MAX,
             float duration = 0.0f) {
        const auto& nodes = bvh_tree.nodes();
        if (nodes.empty()) return;
        draw_bvh_node(nodes, 0, 0, max_depth, duration);
    }

    // -------------------------------------------------------------------------
    // Frame Management
    // -------------------------------------------------------------------------

    /// Update and remove expired primitives
    void update(float delta_time) {
        // Update line durations and remove expired
        for (auto it = m_lines.begin(); it != m_lines.end(); ) {
            if (it->duration > 0.0f) {
                it->duration -= delta_time;
                if (it->duration <= 0.0f) {
                    it = m_lines.erase(it);
                    continue;
                }
            }
            ++it;
        }

        // Update triangle durations
        for (auto it = m_triangles.begin(); it != m_triangles.end(); ) {
            if (it->duration > 0.0f) {
                it->duration -= delta_time;
                if (it->duration <= 0.0f) {
                    it = m_triangles.erase(it);
                    continue;
                }
            }
            ++it;
        }

        // Update text durations
        for (auto it = m_texts.begin(); it != m_texts.end(); ) {
            if (it->duration > 0.0f) {
                it->duration -= delta_time;
                if (it->duration <= 0.0f) {
                    it = m_texts.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    /// Clear single-frame primitives (duration == 0)
    void clear_frame() {
        m_lines.erase(
            std::remove_if(m_lines.begin(), m_lines.end(),
                [](const DebugLine& l) { return l.duration == 0.0f; }),
            m_lines.end()
        );
        m_triangles.erase(
            std::remove_if(m_triangles.begin(), m_triangles.end(),
                [](const DebugTriangle& t) { return t.duration == 0.0f; }),
            m_triangles.end()
        );
        m_texts.erase(
            std::remove_if(m_texts.begin(), m_texts.end(),
                [](const DebugText& t) { return t.duration == 0.0f; }),
            m_texts.end()
        );
    }

    /// Clear all primitives
    void clear_all() {
        m_lines.clear();
        m_triangles.clear();
        m_texts.clear();
    }

    // -------------------------------------------------------------------------
    // Getters
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<DebugLine>& lines() const noexcept { return m_lines; }
    [[nodiscard]] const std::vector<DebugTriangle>& triangles() const noexcept { return m_triangles; }
    [[nodiscard]] const std::vector<DebugText>& texts() const noexcept { return m_texts; }

    [[nodiscard]] std::size_t line_count() const noexcept { return m_lines.size(); }
    [[nodiscard]] std::size_t triangle_count() const noexcept { return m_triangles.size(); }
    [[nodiscard]] std::size_t text_count() const noexcept { return m_texts.size(); }

private:
    void draw_bvh_node(const std::vector<BVHNode>& nodes, std::uint32_t index,
                       std::uint32_t depth, std::uint32_t max_depth, float duration) {
        if (index >= nodes.size() || depth > max_depth) return;

        const auto& node = nodes[index];

        // Color based on depth
        float hue = static_cast<float>(depth % 6) / 6.0f;
        std::array<float, 4> color = {
            std::abs(hue * 6.0f - 3.0f) - 1.0f,
            2.0f - std::abs(hue * 6.0f - 2.0f),
            2.0f - std::abs(hue * 6.0f - 4.0f),
            0.5f
        };
        color[0] = std::clamp(color[0], 0.0f, 1.0f);
        color[1] = std::clamp(color[1], 0.0f, 1.0f);
        color[2] = std::clamp(color[2], 0.0f, 1.0f);

        aabb(node.bounds, color, duration);

        if (!node.is_leaf) {
            draw_bvh_node(nodes, node.left_child, depth + 1, max_depth, duration);
            draw_bvh_node(nodes, node.right_child, depth + 1, max_depth, duration);
        }
    }

private:
    std::vector<DebugLine> m_lines;
    std::vector<DebugTriangle> m_triangles;
    std::vector<DebugText> m_texts;
};

// =============================================================================
// FrameStats
// =============================================================================

/// Per-frame statistics
struct FrameStats {
    // Timing
    float frame_time_ms = 0.0f;      // Total frame time
    float cpu_time_ms = 0.0f;        // CPU time
    float gpu_time_ms = 0.0f;        // GPU time
    float present_time_ms = 0.0f;    // Present/swap time

    // Draw calls
    std::uint32_t draw_calls = 0;
    std::uint32_t compute_dispatches = 0;
    std::uint32_t triangles = 0;
    std::uint32_t vertices = 0;
    std::uint32_t instances = 0;

    // State changes
    std::uint32_t pipeline_binds = 0;
    std::uint32_t buffer_binds = 0;
    std::uint32_t texture_binds = 0;
    std::uint32_t descriptor_binds = 0;

    // Culling
    std::uint32_t objects_visible = 0;
    std::uint32_t objects_culled = 0;
    std::uint32_t lights_visible = 0;
    std::uint32_t lights_culled = 0;

    // Memory
    std::uint64_t gpu_memory_used = 0;
    std::uint64_t cpu_memory_used = 0;

    /// Reset all counters
    void reset() {
        frame_time_ms = cpu_time_ms = gpu_time_ms = present_time_ms = 0.0f;
        draw_calls = compute_dispatches = triangles = vertices = instances = 0;
        pipeline_binds = buffer_binds = texture_binds = descriptor_binds = 0;
        objects_visible = objects_culled = lights_visible = lights_culled = 0;
        gpu_memory_used = cpu_memory_used = 0;
    }

    /// Get frames per second
    [[nodiscard]] float fps() const {
        return frame_time_ms > 0.0f ? 1000.0f / frame_time_ms : 0.0f;
    }
};

// =============================================================================
// StatsHistory
// =============================================================================

/// Rolling history of frame stats
class StatsHistory {
public:
    /// Construct with history size
    explicit StatsHistory(std::size_t history_size = 120)
        : m_max_size(history_size) {}

    /// Add frame stats
    void add(const FrameStats& stats) {
        m_history.push_back(stats);
        if (m_history.size() > m_max_size) {
            m_history.pop_front();
        }
    }

    /// Get average frame time
    [[nodiscard]] float average_frame_time() const {
        if (m_history.empty()) return 0.0f;
        float sum = 0.0f;
        for (const auto& s : m_history) {
            sum += s.frame_time_ms;
        }
        return sum / static_cast<float>(m_history.size());
    }

    /// Get average FPS
    [[nodiscard]] float average_fps() const {
        float avg_time = average_frame_time();
        return avg_time > 0.0f ? 1000.0f / avg_time : 0.0f;
    }

    /// Get min/max frame time
    [[nodiscard]] std::pair<float, float> frame_time_range() const {
        if (m_history.empty()) return {0.0f, 0.0f};
        float min_t = FLT_MAX, max_t = 0.0f;
        for (const auto& s : m_history) {
            min_t = std::min(min_t, s.frame_time_ms);
            max_t = std::max(max_t, s.frame_time_ms);
        }
        return {min_t, max_t};
    }

    /// Get 1% low FPS
    [[nodiscard]] float percentile_1_low_fps() const {
        if (m_history.empty()) return 0.0f;

        std::vector<float> times;
        times.reserve(m_history.size());
        for (const auto& s : m_history) {
            times.push_back(s.frame_time_ms);
        }
        std::sort(times.begin(), times.end(), std::greater<float>());

        std::size_t index = std::max<std::size_t>(1, times.size() / 100);
        float slow_time = times[index - 1];
        return slow_time > 0.0f ? 1000.0f / slow_time : 0.0f;
    }

    /// Get recent stats
    [[nodiscard]] const std::deque<FrameStats>& history() const noexcept {
        return m_history;
    }

    /// Get latest stats
    [[nodiscard]] const FrameStats* latest() const {
        return m_history.empty() ? nullptr : &m_history.back();
    }

    /// Clear history
    void clear() {
        m_history.clear();
    }

private:
    std::deque<FrameStats> m_history;
    std::size_t m_max_size;
};

// =============================================================================
// ScopedTimer
// =============================================================================

/// RAII timer for measuring code sections
class ScopedTimer {
public:
    using Clock = std::chrono::high_resolution_clock;

    explicit ScopedTimer(float& out_ms)
        : m_out(out_ms)
        , m_start(Clock::now()) {}

    ~ScopedTimer() {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        m_out = static_cast<float>(duration.count()) / 1000.0f;
    }

private:
    float& m_out;
    Clock::time_point m_start;
};

// =============================================================================
// GpuTimerQuery
// =============================================================================

/// GPU timer query (placeholder - implementation depends on backend)
struct GpuTimerQuery {
    std::string name;
    std::uint64_t start_query = 0;
    std::uint64_t end_query = 0;
    float elapsed_ms = 0.0f;
    bool resolved = false;
};

// =============================================================================
// StatsCollector
// =============================================================================

/// Collects and reports render statistics
class StatsCollector {
public:
    /// Begin frame
    void begin_frame() {
        m_current.reset();
        m_frame_start = std::chrono::high_resolution_clock::now();
    }

    /// End frame
    void end_frame() {
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            frame_end - m_frame_start);
        m_current.frame_time_ms = static_cast<float>(duration.count()) / 1000.0f;

        m_history.add(m_current);
    }

    /// Record draw call
    void record_draw(std::uint32_t triangles, std::uint32_t vertices, std::uint32_t instances = 1) {
        m_current.draw_calls++;
        m_current.triangles += triangles;
        m_current.vertices += vertices;
        m_current.instances += instances;
    }

    /// Record compute dispatch
    void record_compute() {
        m_current.compute_dispatches++;
    }

    /// Record pipeline bind
    void record_pipeline_bind() {
        m_current.pipeline_binds++;
    }

    /// Record buffer bind
    void record_buffer_bind() {
        m_current.buffer_binds++;
    }

    /// Record texture bind
    void record_texture_bind() {
        m_current.texture_binds++;
    }

    /// Record culling result
    void record_culling(bool visible) {
        if (visible) {
            m_current.objects_visible++;
        } else {
            m_current.objects_culled++;
        }
    }

    /// Get current frame stats
    [[nodiscard]] FrameStats& current() noexcept { return m_current; }
    [[nodiscard]] const FrameStats& current() const noexcept { return m_current; }

    /// Get stats history
    [[nodiscard]] StatsHistory& history() noexcept { return m_history; }
    [[nodiscard]] const StatsHistory& history() const noexcept { return m_history; }

    /// Get formatted stats string
    [[nodiscard]] std::string format_stats() const {
        std::string result;
        result += "FPS: " + std::to_string(static_cast<int>(m_history.average_fps()));
        result += " | Frame: " + std::to_string(m_current.frame_time_ms) + "ms";
        result += " | Draw: " + std::to_string(m_current.draw_calls);
        result += " | Tris: " + std::to_string(m_current.triangles);
        result += " | Visible: " + std::to_string(m_current.objects_visible);
        result += "/" + std::to_string(m_current.objects_visible + m_current.objects_culled);
        return result;
    }

    /// Named GPU timer management
    void begin_gpu_timer(const std::string& name) {
        (void)name;
        // Implementation depends on graphics backend
    }

    void end_gpu_timer(const std::string& name) {
        (void)name;
        // Implementation depends on graphics backend
    }

private:
    FrameStats m_current;
    StatsHistory m_history;
    std::chrono::high_resolution_clock::time_point m_frame_start;
    std::unordered_map<std::string, GpuTimerQuery> m_gpu_timers;
};

// =============================================================================
// DebugOverlay
// =============================================================================

/// Configuration for debug overlay
struct DebugOverlayConfig {
    bool show_fps = true;
    bool show_frame_time = true;
    bool show_draw_calls = true;
    bool show_triangles = true;
    bool show_culling = true;
    bool show_memory = false;
    bool show_graph = true;

    std::array<float, 2> position = {10.0f, 10.0f};
    float line_height = 16.0f;
    std::array<float, 4> background_color = {0.0f, 0.0f, 0.0f, 0.7f};
    std::array<float, 4> text_color = debug_color::White;
};

/// Debug overlay renderer
class DebugOverlay {
public:
    explicit DebugOverlay(const DebugOverlayConfig& config = DebugOverlayConfig{})
        : m_config(config) {}

    /// Get config
    [[nodiscard]] DebugOverlayConfig& config() noexcept { return m_config; }
    [[nodiscard]] const DebugOverlayConfig& config() const noexcept { return m_config; }

    /// Render overlay to debug renderer
    void render(DebugRenderer& renderer, const StatsCollector& stats) const {
        float x = m_config.position[0];
        float y = m_config.position[1];

        const auto& current = stats.current();
        const auto& history = stats.history();

        if (m_config.show_fps) {
            std::string fps_text = "FPS: " + std::to_string(static_cast<int>(history.average_fps()));
            fps_text += " (1% low: " + std::to_string(static_cast<int>(history.percentile_1_low_fps())) + ")";
            renderer.text(fps_text, x, y, m_config.text_color);
            y += m_config.line_height;
        }

        if (m_config.show_frame_time) {
            auto [min_t, max_t] = history.frame_time_range();
            std::string time_text = "Frame: " + std::to_string(current.frame_time_ms) + "ms";
            time_text += " (min: " + std::to_string(min_t) + ", max: " + std::to_string(max_t) + ")";
            renderer.text(time_text, x, y, m_config.text_color);
            y += m_config.line_height;
        }

        if (m_config.show_draw_calls) {
            std::string draw_text = "Draws: " + std::to_string(current.draw_calls);
            draw_text += " | Compute: " + std::to_string(current.compute_dispatches);
            renderer.text(draw_text, x, y, m_config.text_color);
            y += m_config.line_height;
        }

        if (m_config.show_triangles) {
            std::string tri_text = "Triangles: " + std::to_string(current.triangles);
            tri_text += " | Vertices: " + std::to_string(current.vertices);
            renderer.text(tri_text, x, y, m_config.text_color);
            y += m_config.line_height;
        }

        if (m_config.show_culling) {
            std::uint32_t total = current.objects_visible + current.objects_culled;
            std::string cull_text = "Visible: " + std::to_string(current.objects_visible);
            cull_text += "/" + std::to_string(total);
            if (total > 0) {
                float pct = 100.0f * static_cast<float>(current.objects_culled) / static_cast<float>(total);
                cull_text += " (" + std::to_string(static_cast<int>(pct)) + "% culled)";
            }
            renderer.text(cull_text, x, y, m_config.text_color);
            y += m_config.line_height;
        }

        if (m_config.show_memory) {
            std::string mem_text = "GPU: " + format_bytes(current.gpu_memory_used);
            mem_text += " | CPU: " + format_bytes(current.cpu_memory_used);
            renderer.text(mem_text, x, y, m_config.text_color);
            y += m_config.line_height;
        }
    }

private:
    [[nodiscard]] static std::string format_bytes(std::uint64_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
        return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
    }

private:
    DebugOverlayConfig m_config;
};

} // namespace void_render
