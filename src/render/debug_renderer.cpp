/// @file debug_renderer.cpp
/// @brief Debug visualization and frame statistics

#include <void_engine/render/debug.hpp>
#include <void_engine/render/gl_renderer.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <numeric>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FLOAT 0x1406
#define GL_LINES 0x0001
#define GL_TRIANGLES 0x0004
#define GL_LINE_STRIP 0x0003
#define GL_POINTS 0x0000

#define DECLARE_GL_FUNC(ret, name, ...) \
    typedef ret (APIENTRY *PFN_##name)(__VA_ARGS__); \
    static PFN_##name pfn_##name = nullptr;

DECLARE_GL_FUNC(void, glGenBuffers, GLsizei n, GLuint* buffers)
DECLARE_GL_FUNC(void, glDeleteBuffers, GLsizei n, const GLuint* buffers)
DECLARE_GL_FUNC(void, glBindBuffer, GLenum target, GLuint buffer)
DECLARE_GL_FUNC(void, glBufferData, GLenum target, GLsizeiptr size, const void* data, GLenum usage)
DECLARE_GL_FUNC(void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
DECLARE_GL_FUNC(void, glGenVertexArrays, GLsizei n, GLuint* arrays)
DECLARE_GL_FUNC(void, glDeleteVertexArrays, GLsizei n, const GLuint* arrays)
DECLARE_GL_FUNC(void, glBindVertexArray, GLuint array)
DECLARE_GL_FUNC(void, glEnableVertexAttribArray, GLuint index)
DECLARE_GL_FUNC(void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
DECLARE_GL_FUNC(void, glDrawArrays, GLenum mode, GLint first, GLsizei count)
DECLARE_GL_FUNC(void, glLineWidth, GLfloat width)
DECLARE_GL_FUNC(void, glPointSize, GLfloat size)
DECLARE_GL_FUNC(void, glEnable, GLenum cap)
DECLARE_GL_FUNC(void, glDisable, GLenum cap)
DECLARE_GL_FUNC(void, glDepthMask, GLboolean flag)
DECLARE_GL_FUNC(void, glBlendFunc, GLenum sfactor, GLenum dfactor)

static bool s_debug_gl_loaded = false;

static bool load_debug_gl_functions() {
    if (s_debug_gl_loaded) return true;

#define LOAD_GL(name) pfn_##name = (PFN_##name)wglGetProcAddress(#name);

    LOAD_GL(glGenBuffers)
    LOAD_GL(glDeleteBuffers)
    LOAD_GL(glBindBuffer)
    LOAD_GL(glBufferData)
    LOAD_GL(glBufferSubData)
    LOAD_GL(glGenVertexArrays)
    LOAD_GL(glDeleteVertexArrays)
    LOAD_GL(glBindVertexArray)
    LOAD_GL(glEnableVertexAttribArray)
    LOAD_GL(glVertexAttribPointer)
    LOAD_GL(glDrawArrays)
    LOAD_GL(glLineWidth)
    LOAD_GL(glPointSize)
    LOAD_GL(glEnable)
    LOAD_GL(glDisable)
    LOAD_GL(glDepthMask)
    LOAD_GL(glBlendFunc)

#undef LOAD_GL

    s_debug_gl_loaded = true;
    return true;
}

#define GL_CALL(name, ...) (pfn_##name ? pfn_##name(__VA_ARGS__) : (void)0)

#else
#include <GL/gl.h>
#include <GL/glext.h>
#define GL_CALL(name, ...) name(__VA_ARGS__)
static bool load_debug_gl_functions() { return true; }
#endif

namespace void_render {

// =============================================================================
// DebugRenderer Implementation
// =============================================================================

DebugRenderer::DebugRenderer() = default;

DebugRenderer::~DebugRenderer() {
    shutdown();
}

bool DebugRenderer::initialize(std::size_t max_vertices) {
    if (!load_debug_gl_functions()) return false;

    m_max_vertices = max_vertices;

    // Create VAO
    if (pfn_glGenVertexArrays) {
        pfn_glGenVertexArrays(1, &m_vao);
    }

    // Create VBO
    if (pfn_glGenBuffers) {
        pfn_glGenBuffers(1, &m_vbo);
    }

    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(m_vao);
    }

    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    }

    if (pfn_glBufferData) {
        pfn_glBufferData(GL_ARRAY_BUFFER, max_vertices * sizeof(DebugVertex),
                         nullptr, GL_DYNAMIC_DRAW);
    }

    // Position attribute
    if (pfn_glEnableVertexAttribArray) {
        pfn_glEnableVertexAttribArray(0);
    }
    if (pfn_glVertexAttribPointer) {
        pfn_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                                   (void*)offsetof(DebugVertex, position));
    }

    // Color attribute
    if (pfn_glEnableVertexAttribArray) {
        pfn_glEnableVertexAttribArray(1);
    }
    if (pfn_glVertexAttribPointer) {
        pfn_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                                   (void*)offsetof(DebugVertex, color));
    }

    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(0);
    }

    // Create shader
    if (!create_shader()) {
        return false;
    }

    spdlog::debug("DebugRenderer initialized: max_vertices={}", max_vertices);
    return true;
}

void DebugRenderer::shutdown() {
    if (m_vao && pfn_glDeleteVertexArrays) {
        pfn_glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo && pfn_glDeleteBuffers) {
        pfn_glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    m_shader.reset();
    m_line_vertices.clear();
    m_triangle_vertices.clear();
}

bool DebugRenderer::create_shader() {
    static const char* DEBUG_VERTEX_SHADER = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

out vec4 vertColor;

uniform mat4 viewProjection;

void main() {
    vertColor = aColor;
    gl_Position = viewProjection * vec4(aPos, 1.0);
}
)";

    static const char* DEBUG_FRAGMENT_SHADER = R"(
#version 330 core

in vec4 vertColor;
out vec4 FragColor;

void main() {
    FragColor = vertColor;
}
)";

    m_shader = std::make_unique<ShaderProgram>();
    return m_shader->load_from_source(DEBUG_VERTEX_SHADER, DEBUG_FRAGMENT_SHADER);
}

void DebugRenderer::begin_frame() {
    m_line_vertices.clear();
    m_triangle_vertices.clear();
}

void DebugRenderer::end_frame() {
    // Nothing to do - data is uploaded in render()
}

void DebugRenderer::draw_line(const glm::vec3& start, const glm::vec3& end,
                               const glm::vec4& color) {
    if (m_line_vertices.size() + 2 > m_max_vertices) return;

    m_line_vertices.push_back({start, color});
    m_line_vertices.push_back({end, color});
}

void DebugRenderer::draw_box(const glm::vec3& min, const glm::vec3& max,
                              const glm::vec4& color) {
    // 12 edges of the box
    glm::vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z}
    };

    // Bottom face
    draw_line(corners[0], corners[1], color);
    draw_line(corners[1], corners[2], color);
    draw_line(corners[2], corners[3], color);
    draw_line(corners[3], corners[0], color);

    // Top face
    draw_line(corners[4], corners[5], color);
    draw_line(corners[5], corners[6], color);
    draw_line(corners[6], corners[7], color);
    draw_line(corners[7], corners[4], color);

    // Vertical edges
    draw_line(corners[0], corners[4], color);
    draw_line(corners[1], corners[5], color);
    draw_line(corners[2], corners[6], color);
    draw_line(corners[3], corners[7], color);
}

void DebugRenderer::draw_box(const AABB& aabb, const glm::vec4& color) {
    draw_box(aabb.min, aabb.max, color);
}

void DebugRenderer::draw_sphere(const glm::vec3& center, float radius,
                                 const glm::vec4& color, int segments) {
    const float pi = 3.14159265359f;

    // Draw circles in 3 planes
    for (int axis = 0; axis < 3; ++axis) {
        for (int i = 0; i < segments; ++i) {
            float angle1 = (static_cast<float>(i) / segments) * 2.0f * pi;
            float angle2 = (static_cast<float>(i + 1) / segments) * 2.0f * pi;

            glm::vec3 p1, p2;
            if (axis == 0) {  // YZ plane
                p1 = center + glm::vec3(0, std::cos(angle1), std::sin(angle1)) * radius;
                p2 = center + glm::vec3(0, std::cos(angle2), std::sin(angle2)) * radius;
            } else if (axis == 1) {  // XZ plane
                p1 = center + glm::vec3(std::cos(angle1), 0, std::sin(angle1)) * radius;
                p2 = center + glm::vec3(std::cos(angle2), 0, std::sin(angle2)) * radius;
            } else {  // XY plane
                p1 = center + glm::vec3(std::cos(angle1), std::sin(angle1), 0) * radius;
                p2 = center + glm::vec3(std::cos(angle2), std::sin(angle2), 0) * radius;
            }

            draw_line(p1, p2, color);
        }
    }
}

void DebugRenderer::draw_sphere(const BoundingSphere& sphere, const glm::vec4& color,
                                 int segments) {
    draw_sphere(sphere.center, sphere.radius, color, segments);
}

void DebugRenderer::draw_frustum(const Frustum& frustum, const glm::vec4& color) {
    // This requires extracting corners from the frustum planes
    // For simplicity, we'll skip the full implementation
}

void DebugRenderer::draw_ray(const Ray& ray, float length, const glm::vec4& color) {
    draw_line(ray.origin, ray.origin + ray.direction * length, color);
}

void DebugRenderer::draw_axis(const glm::vec3& position, float size) {
    draw_line(position, position + glm::vec3(size, 0, 0), glm::vec4(1, 0, 0, 1));
    draw_line(position, position + glm::vec3(0, size, 0), glm::vec4(0, 1, 0, 1));
    draw_line(position, position + glm::vec3(0, 0, size), glm::vec4(0, 0, 1, 1));
}

void DebugRenderer::draw_grid(const glm::vec3& center, float size, int divisions,
                               const glm::vec4& color) {
    float half = size * 0.5f;
    float step = size / divisions;

    for (int i = 0; i <= divisions; ++i) {
        float offset = -half + i * step;

        // Lines along X
        draw_line(center + glm::vec3(-half, 0, offset),
                  center + glm::vec3(half, 0, offset), color);

        // Lines along Z
        draw_line(center + glm::vec3(offset, 0, -half),
                  center + glm::vec3(offset, 0, half), color);
    }
}

void DebugRenderer::draw_transform(const glm::mat4& transform, float size) {
    glm::vec3 position = glm::vec3(transform[3]);
    glm::vec3 right = glm::vec3(transform[0]) * size;
    glm::vec3 up = glm::vec3(transform[1]) * size;
    glm::vec3 forward = glm::vec3(transform[2]) * size;

    draw_line(position, position + right, glm::vec4(1, 0, 0, 1));
    draw_line(position, position + up, glm::vec4(0, 1, 0, 1));
    draw_line(position, position + forward, glm::vec4(0, 0, 1, 1));
}

void DebugRenderer::draw_text_3d(const glm::vec3& position, const std::string& text,
                                  const glm::vec4& color) {
    // 3D text rendering requires a font system - store for later rendering
    m_text_requests.push_back({position, text, color});
}

void DebugRenderer::render(const glm::mat4& view_projection) {
    if (m_line_vertices.empty() && m_triangle_vertices.empty()) return;

    // Upload data
    if (pfn_glBindBuffer) {
        pfn_glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    }

    std::size_t line_size = m_line_vertices.size() * sizeof(DebugVertex);
    std::size_t tri_size = m_triangle_vertices.size() * sizeof(DebugVertex);

    if (pfn_glBufferSubData && line_size > 0) {
        pfn_glBufferSubData(GL_ARRAY_BUFFER, 0, line_size, m_line_vertices.data());
    }
    if (pfn_glBufferSubData && tri_size > 0) {
        pfn_glBufferSubData(GL_ARRAY_BUFFER, line_size, tri_size, m_triangle_vertices.data());
    }

    // Setup state
    m_shader->use();
    m_shader->set_mat4("viewProjection", view_projection);

    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(m_vao);
    }

    // Draw lines
    if (!m_line_vertices.empty()) {
        glLineWidth(1.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_line_vertices.size()));
    }

    // Draw triangles
    if (!m_triangle_vertices.empty()) {
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(m_line_vertices.size()),
                     static_cast<GLsizei>(m_triangle_vertices.size()));
    }

    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(0);
    }

    // Clear text requests (would be rendered by a separate text system)
    m_text_requests.clear();
}

// =============================================================================
// FrameStats Implementation
// =============================================================================

FrameStats::FrameStats() = default;

void FrameStats::begin_frame() {
    frame_start = std::chrono::steady_clock::now();
}

void FrameStats::end_frame() {
    auto frame_end = std::chrono::steady_clock::now();
    frame_time_ms = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();
    fps = 1000.0f / frame_time_ms;
}

// =============================================================================
// StatsCollector Implementation
// =============================================================================

StatsCollector::StatsCollector() = default;

void StatsCollector::begin_frame() {
    m_frame_start = std::chrono::steady_clock::now();
}

void StatsCollector::end_frame() {
    auto frame_end = std::chrono::steady_clock::now();
    float frame_time = std::chrono::duration<float, std::milli>(frame_end - m_frame_start).count();

    // Add to history
    m_frame_times.push_back(frame_time);
    if (m_frame_times.size() > m_history_size) {
        m_frame_times.erase(m_frame_times.begin());
    }

    m_frame_count++;
}

void StatsCollector::record_draw_call() {
    m_current_stats.draw_calls++;
}

void StatsCollector::record_triangles(std::uint32_t count) {
    m_current_stats.triangles += count;
}

void StatsCollector::record_vertices(std::uint32_t count) {
    m_current_stats.vertices += count;
}

void StatsCollector::record_gpu_time(float ms) {
    m_current_stats.gpu_time_ms = ms;
}

FrameStats StatsCollector::get_stats() const {
    FrameStats stats = m_current_stats;

    // Calculate averages
    if (!m_frame_times.empty()) {
        float sum = std::accumulate(m_frame_times.begin(), m_frame_times.end(), 0.0f);
        stats.frame_time_ms = sum / m_frame_times.size();
        stats.fps = 1000.0f / stats.frame_time_ms;

        // Find min/max
        auto [min_it, max_it] = std::minmax_element(m_frame_times.begin(), m_frame_times.end());
        stats.min_frame_time_ms = *min_it;
        stats.max_frame_time_ms = *max_it;
    }

    return stats;
}

std::span<const float> StatsCollector::frame_time_history() const {
    return std::span<const float>(m_frame_times.data(), m_frame_times.size());
}

void StatsCollector::reset() {
    m_current_stats = {};
    m_frame_times.clear();
    m_frame_count = 0;
}

// =============================================================================
// DebugOverlay Implementation
// =============================================================================

DebugOverlay::DebugOverlay() = default;

DebugOverlay::~DebugOverlay() = default;

bool DebugOverlay::initialize() {
    // Would initialize text rendering here
    spdlog::debug("DebugOverlay initialized");
    return true;
}

void DebugOverlay::shutdown() {
    // Cleanup
}

void DebugOverlay::set_stats(const FrameStats& stats) {
    m_stats = stats;
}

void DebugOverlay::add_text(const std::string& key, const std::string& value) {
    m_text_entries[key] = value;
}

void DebugOverlay::remove_text(const std::string& key) {
    m_text_entries.erase(key);
}

void DebugOverlay::clear_text() {
    m_text_entries.clear();
}

void DebugOverlay::render(std::uint32_t screen_width, std::uint32_t screen_height) {
    if (!m_visible) return;

    // Would render overlay text here using a font rendering system
    // For now, just log the stats
    if (m_show_fps) {
        spdlog::trace("FPS: {:.1f} ({:.2f}ms)", m_stats.fps, m_stats.frame_time_ms);
    }
}

// =============================================================================
// Global Debug State
// =============================================================================

static std::unique_ptr<DebugRenderer> g_debug_renderer;
static std::unique_ptr<StatsCollector> g_stats_collector;
static std::unique_ptr<DebugOverlay> g_debug_overlay;

bool init_debug_rendering(std::size_t max_vertices) {
    g_debug_renderer = std::make_unique<DebugRenderer>();
    if (!g_debug_renderer->initialize(max_vertices)) {
        g_debug_renderer.reset();
        return false;
    }

    g_stats_collector = std::make_unique<StatsCollector>();
    g_debug_overlay = std::make_unique<DebugOverlay>();
    g_debug_overlay->initialize();

    return true;
}

void shutdown_debug_rendering() {
    g_debug_overlay.reset();
    g_stats_collector.reset();
    g_debug_renderer.reset();
}

DebugRenderer* get_debug_renderer() {
    return g_debug_renderer.get();
}

StatsCollector* get_stats_collector() {
    return g_stats_collector.get();
}

DebugOverlay* get_debug_overlay() {
    return g_debug_overlay.get();
}

} // namespace void_render
