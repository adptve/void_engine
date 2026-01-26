#pragma once

/// @file debug.hpp
/// @brief Debug visualization and statistics for void_render
///
/// Provides GPU-accelerated debug rendering with line/shape primitives,
/// frame statistics collection, and overlay display.

#include "fwd.hpp"
#include "spatial.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <deque>
#include <span>
#include <unordered_map>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace void_render {

// Forward declaration
class ShaderProgram;

// =============================================================================
// DebugColor
// =============================================================================

/// Predefined debug colors as glm::vec4
namespace debug_color {
    constexpr glm::vec4 Red     = {1.0f, 0.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Green   = {0.0f, 1.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Blue    = {0.0f, 0.0f, 1.0f, 1.0f};
    constexpr glm::vec4 Yellow  = {1.0f, 1.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Cyan    = {0.0f, 1.0f, 1.0f, 1.0f};
    constexpr glm::vec4 Magenta = {1.0f, 0.0f, 1.0f, 1.0f};
    constexpr glm::vec4 White   = {1.0f, 1.0f, 1.0f, 1.0f};
    constexpr glm::vec4 Black   = {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Gray    = {0.5f, 0.5f, 0.5f, 1.0f};
    constexpr glm::vec4 Orange  = {1.0f, 0.5f, 0.0f, 1.0f};
    constexpr glm::vec4 Purple  = {0.5f, 0.0f, 0.5f, 1.0f};
    constexpr glm::vec4 Pink    = {1.0f, 0.4f, 0.7f, 1.0f};
}

// =============================================================================
// DebugVertex
// =============================================================================

/// Vertex for GPU-accelerated debug rendering
struct DebugVertex {
    glm::vec3 position;
    glm::vec4 color;

    DebugVertex() = default;
    DebugVertex(const glm::vec3& pos, const glm::vec4& col)
        : position(pos), color(col) {}
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
// Text Request (internal)
// =============================================================================

/// 3D text rendering request
struct TextRequest {
    glm::vec3 position;
    std::string text;
    glm::vec4 color;
};

// =============================================================================
// DebugRenderer
// =============================================================================

/// GPU-accelerated immediate-mode debug renderer
///
/// Uses OpenGL VAO/VBO for efficient rendering of debug primitives.
/// Call initialize() before use, and shutdown() when done.
class DebugRenderer {
public:
    DebugRenderer();
    ~DebugRenderer();

    // Non-copyable
    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;

    /// Initialize GPU resources
    [[nodiscard]] bool initialize(std::size_t max_vertices = 65536);

    /// Release GPU resources
    void shutdown();

    /// Begin a new frame (clears previous primitives)
    void begin_frame();

    /// End frame preparation
    void end_frame();

    // -------------------------------------------------------------------------
    // Primitive Drawing
    // -------------------------------------------------------------------------

    /// Draw line between two points
    void draw_line(const glm::vec3& start, const glm::vec3& end,
                   const glm::vec4& color = debug_color::White);

    /// Draw wireframe box from min/max corners
    void draw_box(const glm::vec3& min, const glm::vec3& max,
                  const glm::vec4& color = debug_color::Green);

    /// Draw wireframe AABB
    void draw_box(const AABB& aabb, const glm::vec4& color = debug_color::Green);

    /// Draw wireframe sphere
    void draw_sphere(const glm::vec3& center, float radius,
                     const glm::vec4& color = debug_color::Cyan,
                     int segments = 16);

    /// Draw wireframe bounding sphere
    void draw_sphere(const BoundingSphere& sphere,
                     const glm::vec4& color = debug_color::Cyan,
                     int segments = 16);

    /// Draw frustum wireframe
    void draw_frustum(const Frustum& frustum,
                      const glm::vec4& color = debug_color::Yellow);

    /// Draw ray
    void draw_ray(const Ray& ray, float length,
                  const glm::vec4& color = debug_color::Yellow);

    /// Draw coordinate axes at position
    void draw_axis(const glm::vec3& position, float size = 1.0f);

    /// Draw grid on XZ plane
    void draw_grid(const glm::vec3& center, float size, int divisions,
                   const glm::vec4& color = debug_color::Gray);

    /// Draw transform axes from matrix
    void draw_transform(const glm::mat4& transform, float size = 1.0f);

    /// Queue 3D text for rendering
    void draw_text_3d(const glm::vec3& position, const std::string& text,
                      const glm::vec4& color = debug_color::White);

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    /// Render all queued primitives
    void render(const glm::mat4& view_projection);

private:
    bool create_shader();

private:
    std::uint32_t m_vao = 0;
    std::uint32_t m_vbo = 0;
    std::unique_ptr<ShaderProgram> m_shader;
    std::size_t m_max_vertices = 0;

    std::vector<DebugVertex> m_line_vertices;
    std::vector<DebugVertex> m_triangle_vertices;
    std::vector<TextRequest> m_text_requests;
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
    float fps = 0.0f;                // Frames per second

    // Frame time range
    float min_frame_time_ms = 0.0f;
    float max_frame_time_ms = 0.0f;

    // Draw call stats
    std::uint32_t draw_calls = 0;
    std::uint32_t triangles = 0;
    std::uint32_t vertices = 0;

    // Memory
    std::uint64_t gpu_memory_used = 0;
    std::uint64_t texture_memory = 0;
    std::uint64_t buffer_memory = 0;

    // Internal timing state
    std::chrono::steady_clock::time_point frame_start;

    FrameStats();

    /// Begin frame timing
    void begin_frame();

    /// End frame timing
    void end_frame();
};

// =============================================================================
// StatsCollector
// =============================================================================

/// Collects and averages frame statistics over time
class StatsCollector {
public:
    StatsCollector();

    // Non-copyable
    StatsCollector(const StatsCollector&) = delete;
    StatsCollector& operator=(const StatsCollector&) = delete;

    /// Begin frame timing
    void begin_frame();

    /// End frame and record timing
    void end_frame();

    /// Record a draw call
    void record_draw_call();

    /// Record triangle count
    void record_triangles(std::uint32_t count);

    /// Record vertex count
    void record_vertices(std::uint32_t count);

    /// Record GPU time
    void record_gpu_time(float ms);

    /// Get current/averaged stats
    [[nodiscard]] FrameStats get_stats() const;

    /// Get frame time history for graphing
    [[nodiscard]] std::span<const float> frame_time_history() const;

    /// Reset all statistics
    void reset();

private:
    FrameStats m_current_stats;
    std::chrono::steady_clock::time_point m_frame_start;
    std::vector<float> m_frame_times;
    std::size_t m_history_size = 120;  // 2 seconds at 60fps
    std::uint64_t m_frame_count = 0;
};

// =============================================================================
// DebugOverlayConfig
// =============================================================================

/// Configuration for debug overlay
struct DebugOverlayConfig {
    bool show_fps = true;
    bool show_frame_time = true;
    bool show_draw_calls = true;
    bool show_triangle_count = true;
    bool show_gpu_memory = false;
    glm::vec2 position = {10.0f, 10.0f};
    float scale = 1.0f;
    glm::vec4 text_color = debug_color::White;
    glm::vec4 background_color = {0.0f, 0.0f, 0.0f, 0.5f};
};

// =============================================================================
// DebugOverlay
// =============================================================================

/// On-screen debug information overlay
class DebugOverlay {
public:
    DebugOverlay();
    ~DebugOverlay();

    // Non-copyable
    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    /// Initialize overlay resources
    [[nodiscard]] bool initialize();

    /// Release resources
    void shutdown();

    /// Set current frame stats
    void set_stats(const FrameStats& stats);

    /// Add custom text entry
    void add_text(const std::string& key, const std::string& value);

    /// Remove custom text entry
    void remove_text(const std::string& key);

    /// Clear all custom text entries
    void clear_text();

    /// Render overlay to screen
    void render(std::uint32_t screen_width, std::uint32_t screen_height);

    /// Show/hide overlay
    void set_visible(bool visible) { m_visible = visible; }

    /// Check visibility
    [[nodiscard]] bool is_visible() const { return m_visible; }

    /// Show/hide FPS display
    void set_show_fps(bool show) { m_show_fps = show; }

private:
    bool m_visible = true;
    bool m_show_fps = true;
    FrameStats m_stats;
    std::unordered_map<std::string, std::string> m_text_entries;
};

// =============================================================================
// Global Debug Functions
// =============================================================================

/// Initialize global debug rendering system
[[nodiscard]] bool init_debug_rendering(std::size_t max_vertices = 65536);

/// Shutdown global debug rendering system
void shutdown_debug_rendering();

/// Get global debug renderer (nullptr if not initialized)
[[nodiscard]] DebugRenderer* get_debug_renderer();

/// Get global stats collector (nullptr if not initialized)
[[nodiscard]] StatsCollector* get_stats_collector();

/// Get global debug overlay (nullptr if not initialized)
[[nodiscard]] DebugOverlay* get_debug_overlay();

} // namespace void_render
