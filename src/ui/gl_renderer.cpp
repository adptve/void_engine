/// @file gl_renderer.cpp
/// @brief OpenGL UI Renderer implementation

#include <void_engine/ui/renderer.hpp>
#include <void_engine/ui/types.hpp>

#include <cstring>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// OpenGL types and constants
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef intptr_t GLintptr;

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80CA
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CC

// Extension function pointers
#define DECLARE_GL_FUNC(ret, name, ...) \
    typedef ret (APIENTRY *PFN##name##PROC)(__VA_ARGS__); \
    static PFN##name##PROC name##Ptr = nullptr;

DECLARE_GL_FUNC(GLuint, glCreateShader, GLenum type)
DECLARE_GL_FUNC(void, glShaderSource, GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
DECLARE_GL_FUNC(void, glCompileShader, GLuint shader)
DECLARE_GL_FUNC(void, glGetShaderiv, GLuint shader, GLenum pname, GLint* params)
DECLARE_GL_FUNC(void, glGetShaderInfoLog, GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
DECLARE_GL_FUNC(GLuint, glCreateProgram, void)
DECLARE_GL_FUNC(void, glAttachShader, GLuint program, GLuint shader)
DECLARE_GL_FUNC(void, glLinkProgram, GLuint program)
DECLARE_GL_FUNC(void, glGetProgramiv, GLuint program, GLenum pname, GLint* params)
DECLARE_GL_FUNC(void, glGetProgramInfoLog, GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
DECLARE_GL_FUNC(void, glUseProgram, GLuint program)
DECLARE_GL_FUNC(void, glDeleteShader, GLuint shader)
DECLARE_GL_FUNC(void, glDeleteProgram, GLuint program)
DECLARE_GL_FUNC(void, glGenVertexArrays, GLsizei n, GLuint* arrays)
DECLARE_GL_FUNC(void, glBindVertexArray, GLuint array)
DECLARE_GL_FUNC(void, glDeleteVertexArrays, GLsizei n, const GLuint* arrays)
DECLARE_GL_FUNC(void, glGenBuffers, GLsizei n, GLuint* buffers)
DECLARE_GL_FUNC(void, glBindBuffer, GLenum target, GLuint buffer)
DECLARE_GL_FUNC(void, glBufferData, GLenum target, GLsizeiptr size, const void* data, GLenum usage)
DECLARE_GL_FUNC(void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
DECLARE_GL_FUNC(void, glDeleteBuffers, GLsizei n, const GLuint* buffers)
DECLARE_GL_FUNC(void, glEnableVertexAttribArray, GLuint index)
DECLARE_GL_FUNC(void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
DECLARE_GL_FUNC(GLint, glGetUniformLocation, GLuint program, const GLchar* name)
DECLARE_GL_FUNC(void, glUniform2f, GLint location, GLfloat v0, GLfloat v1)
DECLARE_GL_FUNC(void, glBlendFuncSeparate, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)

static bool g_ui_gl_loaded = false;

#define LOAD_GL_FUNC(name) \
    name##Ptr = (PFN##name##PROC)wglGetProcAddress(#name); \
    if (!name##Ptr) return false;

static bool load_ui_gl_functions() {
    if (g_ui_gl_loaded) return true;

    LOAD_GL_FUNC(glCreateShader);
    LOAD_GL_FUNC(glShaderSource);
    LOAD_GL_FUNC(glCompileShader);
    LOAD_GL_FUNC(glGetShaderiv);
    LOAD_GL_FUNC(glGetShaderInfoLog);
    LOAD_GL_FUNC(glCreateProgram);
    LOAD_GL_FUNC(glAttachShader);
    LOAD_GL_FUNC(glLinkProgram);
    LOAD_GL_FUNC(glGetProgramiv);
    LOAD_GL_FUNC(glGetProgramInfoLog);
    LOAD_GL_FUNC(glUseProgram);
    LOAD_GL_FUNC(glDeleteShader);
    LOAD_GL_FUNC(glDeleteProgram);
    LOAD_GL_FUNC(glGenVertexArrays);
    LOAD_GL_FUNC(glBindVertexArray);
    LOAD_GL_FUNC(glDeleteVertexArrays);
    LOAD_GL_FUNC(glGenBuffers);
    LOAD_GL_FUNC(glBindBuffer);
    LOAD_GL_FUNC(glBufferData);
    LOAD_GL_FUNC(glBufferSubData);
    LOAD_GL_FUNC(glDeleteBuffers);
    LOAD_GL_FUNC(glEnableVertexAttribArray);
    LOAD_GL_FUNC(glVertexAttribPointer);
    LOAD_GL_FUNC(glGetUniformLocation);
    LOAD_GL_FUNC(glUniform2f);
    LOAD_GL_FUNC(glBlendFuncSeparate);

    g_ui_gl_loaded = true;
    return true;
}

#define GL_CALL(func, ...) func##Ptr(__VA_ARGS__)

#define VOID_HAS_OPENGL 1

#elif defined(__APPLE__) || defined(__linux__)
// macOS/Linux - use system GL headers
#include <GL/gl.h>
#define VOID_HAS_OPENGL 1
static bool load_ui_gl_functions() { return true; }
#define GL_CALL(func, ...) func(__VA_ARGS__)

#else
#define VOID_HAS_OPENGL 0
#endif

namespace void_ui {

// =============================================================================
// OpenGL UI Renderer
// =============================================================================

/// OpenGL 3.3 shader source for UI rendering
static constexpr const char* UI_VERTEX_SHADER_SRC = R"(
#version 330 core

uniform vec2 u_screen_size;

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;

out vec2 v_uv;
out vec4 v_color;

void main() {
    // Convert pixel coordinates to clip space (-1 to 1)
    float x = (a_position.x / u_screen_size.x) * 2.0 - 1.0;
    float y = 1.0 - (a_position.y / u_screen_size.y) * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
}
)";

static constexpr const char* UI_FRAGMENT_SHADER_SRC = R"(
#version 330 core

in vec2 v_uv;
in vec4 v_color;

out vec4 frag_color;

void main() {
    frag_color = v_color;
}
)";

#if VOID_HAS_OPENGL

/// OpenGL UI Renderer implementation
class OpenGLUiRenderer : public IUiRenderer {
public:
    OpenGLUiRenderer() {
        if (load_ui_gl_functions()) {
            create_resources();
        }
    }

    ~OpenGLUiRenderer() override {
        destroy_resources();
    }

    void set_screen_size(float width, float height) override {
        m_screen_size = {width, height};
    }

    [[nodiscard]] Size screen_size() const override {
        return m_screen_size;
    }

    bool prepare(const UiDrawData& draw_data) override {
        if (draw_data.empty()) {
            m_vertex_count = 0;
            m_index_count = 0;
            return false;
        }

        // Update vertex buffer
        std::size_t vertex_size = draw_data.vertices.size() * sizeof(UiVertex);
        if (vertex_size > m_vertex_buffer_size) {
            GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, m_vbo);
            GL_CALL(glBufferData, GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertex_size),
                         draw_data.vertices.data(), GL_DYNAMIC_DRAW);
            m_vertex_buffer_size = vertex_size;
        } else {
            GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, m_vbo);
            GL_CALL(glBufferSubData, GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertex_size),
                            draw_data.vertices.data());
        }

        // Update index buffer
        std::size_t index_size = draw_data.indices.size() * sizeof(std::uint16_t);
        if (index_size > m_index_buffer_size) {
            GL_CALL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ebo);
            GL_CALL(glBufferData, GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(index_size),
                         draw_data.indices.data(), GL_DYNAMIC_DRAW);
            m_index_buffer_size = index_size;
        } else {
            GL_CALL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ebo);
            GL_CALL(glBufferSubData, GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(index_size),
                            draw_data.indices.data());
        }

        m_vertex_count = static_cast<std::uint32_t>(draw_data.vertices.size());
        m_index_count = static_cast<std::uint32_t>(draw_data.indices.size());

        return true;
    }

    void render(void* /*render_pass*/) override {
        if (m_index_count == 0) return;

        // Save GL state
        GLint last_program;
        glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        GLboolean last_blend_enabled = glIsEnabled(GL_BLEND);
        GLboolean last_depth_test = glIsEnabled(GL_DEPTH_TEST);
        GLboolean last_cull_face = glIsEnabled(GL_CULL_FACE);
        GLboolean last_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
        GLint last_blend_src_rgb, last_blend_dst_rgb;
        GLint last_blend_src_alpha, last_blend_dst_alpha;
        glGetIntegerv(GL_BLEND_SRC_RGB, &last_blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &last_blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);

        // Setup render state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);

        // Use our shader
        GL_CALL(glUseProgram, m_program);

        // Set uniforms
        GL_CALL(glUniform2f, m_uniform_screen_size,
                    m_screen_size.width, m_screen_size.height);

        // Bind VAO and draw
        GL_CALL(glBindVertexArray, m_vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count),
                       GL_UNSIGNED_SHORT, nullptr);

        // Restore GL state
        GL_CALL(glUseProgram, static_cast<GLuint>(last_program));
        if (last_blend_enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (last_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (last_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (last_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        GL_CALL(glBlendFuncSeparate,
            static_cast<GLenum>(last_blend_src_rgb),
            static_cast<GLenum>(last_blend_dst_rgb),
            static_cast<GLenum>(last_blend_src_alpha),
            static_cast<GLenum>(last_blend_dst_alpha));
    }

    [[nodiscard]] void* native_pipeline() const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(m_program));
    }

    [[nodiscard]] void* native_bind_group() const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(m_vao));
    }

    [[nodiscard]] bool is_valid() const override {
        return m_valid;
    }

private:
    void create_resources() {
        // Create and compile vertex shader
        GLuint vertex_shader = GL_CALL(glCreateShader, GL_VERTEX_SHADER);
        const char* vertex_src = UI_VERTEX_SHADER_SRC;
        GL_CALL(glShaderSource, vertex_shader, 1, &vertex_src, nullptr);
        GL_CALL(glCompileShader, vertex_shader);

        GLint success;
        GL_CALL(glGetShaderiv, vertex_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info_log[512];
            GL_CALL(glGetShaderInfoLog, vertex_shader, 512, nullptr, info_log);
            GL_CALL(glDeleteShader, vertex_shader);
            m_valid = false;
            return;
        }

        // Create and compile fragment shader
        GLuint fragment_shader = GL_CALL(glCreateShader, GL_FRAGMENT_SHADER);
        const char* fragment_src = UI_FRAGMENT_SHADER_SRC;
        GL_CALL(glShaderSource, fragment_shader, 1, &fragment_src, nullptr);
        GL_CALL(glCompileShader, fragment_shader);

        GL_CALL(glGetShaderiv, fragment_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info_log[512];
            GL_CALL(glGetShaderInfoLog, fragment_shader, 512, nullptr, info_log);
            GL_CALL(glDeleteShader, vertex_shader);
            GL_CALL(glDeleteShader, fragment_shader);
            m_valid = false;
            return;
        }

        // Create and link program
        m_program = GL_CALL(glCreateProgram);
        GL_CALL(glAttachShader, m_program, vertex_shader);
        GL_CALL(glAttachShader, m_program, fragment_shader);
        GL_CALL(glLinkProgram, m_program);

        GL_CALL(glGetProgramiv, m_program, GL_LINK_STATUS, &success);
        if (!success) {
            char info_log[512];
            GL_CALL(glGetProgramInfoLog, m_program, 512, nullptr, info_log);
            GL_CALL(glDeleteShader, vertex_shader);
            GL_CALL(glDeleteShader, fragment_shader);
            GL_CALL(glDeleteProgram, m_program);
            m_valid = false;
            return;
        }

        GL_CALL(glDeleteShader, vertex_shader);
        GL_CALL(glDeleteShader, fragment_shader);

        // Get uniform locations
        m_uniform_screen_size = GL_CALL(glGetUniformLocation, m_program, "u_screen_size");

        // Create VAO
        GL_CALL(glGenVertexArrays, 1, &m_vao);
        GL_CALL(glBindVertexArray, m_vao);

        // Create VBO
        GL_CALL(glGenBuffers, 1, &m_vbo);
        GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, m_vbo);
        GL_CALL(glBufferData, GL_ARRAY_BUFFER, 65536 * sizeof(UiVertex), nullptr, GL_DYNAMIC_DRAW);
        m_vertex_buffer_size = 65536 * sizeof(UiVertex);

        // Create EBO
        GL_CALL(glGenBuffers, 1, &m_ebo);
        GL_CALL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        GL_CALL(glBufferData, GL_ELEMENT_ARRAY_BUFFER, 65536 * sizeof(std::uint16_t), nullptr, GL_DYNAMIC_DRAW);
        m_index_buffer_size = 65536 * sizeof(std::uint16_t);

        // Setup vertex attributes
        // Position: 2 floats at offset 0
        GL_CALL(glVertexAttribPointer, 0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                              reinterpret_cast<void*>(offsetof(UiVertex, position)));
        GL_CALL(glEnableVertexAttribArray, 0);

        // UV: 2 floats at offset 8
        GL_CALL(glVertexAttribPointer, 1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                              reinterpret_cast<void*>(offsetof(UiVertex, uv)));
        GL_CALL(glEnableVertexAttribArray, 1);

        // Color: 4 floats at offset 16
        GL_CALL(glVertexAttribPointer, 2, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                              reinterpret_cast<void*>(offsetof(UiVertex, color)));
        GL_CALL(glEnableVertexAttribArray, 2);

        GL_CALL(glBindVertexArray, 0);

        m_valid = true;
    }

    void destroy_resources() {
        if (m_vao) {
            GL_CALL(glDeleteVertexArrays, 1, &m_vao);
            m_vao = 0;
        }
        if (m_vbo) {
            GL_CALL(glDeleteBuffers, 1, &m_vbo);
            m_vbo = 0;
        }
        if (m_ebo) {
            GL_CALL(glDeleteBuffers, 1, &m_ebo);
            m_ebo = 0;
        }
        if (m_program) {
            GL_CALL(glDeleteProgram, m_program);
            m_program = 0;
        }
    }

private:
    bool m_valid = false;
    Size m_screen_size{1280.0f, 720.0f};

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    GLint m_uniform_screen_size = -1;

    std::size_t m_vertex_buffer_size = 0;
    std::size_t m_index_buffer_size = 0;
    std::uint32_t m_vertex_count = 0;
    std::uint32_t m_index_count = 0;
};

/// Create an OpenGL UI renderer
std::unique_ptr<IUiRenderer> create_opengl_renderer() {
    return std::make_unique<OpenGLUiRenderer>();
}

#else

/// Fallback when OpenGL is not available
std::unique_ptr<IUiRenderer> create_opengl_renderer() {
    return create_null_renderer();
}

#endif // VOID_HAS_OPENGL

} // namespace void_ui
