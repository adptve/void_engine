/// @file temporal_effects.cpp
/// @brief Temporal Anti-Aliasing, Motion Blur, and Depth of Field

#include "void_engine/render/fwd.hpp"

#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <random>
#include <memory>

// OpenGL function pointers (loaded in gl_renderer.cpp)
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

// OpenGL types and extensions
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA16F 0x881A
#define GL_RG16F 0x822F
#define GL_R32F 0x822E
#define GL_RGBA 0x1908
#define GL_RG 0x8227
#define GL_RED 0x1903
#define GL_FLOAT 0x1406
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT 0x1902
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE0 0x84C0
#endif

// GL function pointer types
typedef void (*PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void (*PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (*PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
typedef void (*PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void (*PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (*PFNGLDELETESHADERPROC)(GLuint);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void (*PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void (*PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void (*PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
typedef void (*PFNGLUNIFORM3FPROC)(GLint, GLfloat, GLfloat, GLfloat);
typedef void (*PFNGLUNIFORM4FPROC)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void (*PFNGLDRAWBUFFERSPROC)(GLsizei, const GLenum*);

namespace {
// GL function pointers - will be loaded at runtime
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ptr = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ptr = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ptr = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ptr = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ptr = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers_ptr = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer_ptr = nullptr;
PFNGLBUFFERDATAPROC glBufferData_ptr = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers_ptr = nullptr;
PFNGLCREATESHADERPROC glCreateShader_ptr = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource_ptr = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader_ptr = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv_ptr = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ptr = nullptr;
PFNGLDELETESHADERPROC glDeleteShader_ptr = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram_ptr = nullptr;
PFNGLATTACHSHADERPROC glAttachShader_ptr = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram_ptr = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv_ptr = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram_ptr = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram_ptr = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation_ptr = nullptr;
PFNGLUNIFORM1IPROC glUniform1i_ptr = nullptr;
PFNGLUNIFORM1FPROC glUniform1f_ptr = nullptr;
PFNGLUNIFORM2FPROC glUniform2f_ptr = nullptr;
PFNGLUNIFORM3FPROC glUniform3f_ptr = nullptr;
PFNGLUNIFORM4FPROC glUniform4f_ptr = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv_ptr = nullptr;
PFNGLACTIVETEXTUREPROC glActiveTexture_ptr = nullptr;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ptr = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ptr = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ptr = nullptr;
PFNGLDRAWBUFFERSPROC glDrawBuffers_ptr = nullptr;

bool g_gl_loaded = false;

void load_gl_functions() {
    if (g_gl_loaded) return;

#ifdef _WIN32
    #define LOAD_GL(name) name##_ptr = (decltype(name##_ptr))wglGetProcAddress(#name)
#else
    #define LOAD_GL(name) name##_ptr = (decltype(name##_ptr))glXGetProcAddress((const GLubyte*)#name)
#endif

    LOAD_GL(glGenFramebuffers);
    LOAD_GL(glBindFramebuffer);
    LOAD_GL(glFramebufferTexture2D);
    LOAD_GL(glCheckFramebufferStatus);
    LOAD_GL(glDeleteFramebuffers);
    LOAD_GL(glGenBuffers);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glBufferData);
    LOAD_GL(glDeleteBuffers);
    LOAD_GL(glCreateShader);
    LOAD_GL(glShaderSource);
    LOAD_GL(glCompileShader);
    LOAD_GL(glGetShaderiv);
    LOAD_GL(glGetShaderInfoLog);
    LOAD_GL(glDeleteShader);
    LOAD_GL(glCreateProgram);
    LOAD_GL(glAttachShader);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glUseProgram);
    LOAD_GL(glDeleteProgram);
    LOAD_GL(glGetUniformLocation);
    LOAD_GL(glUniform1i);
    LOAD_GL(glUniform1f);
    LOAD_GL(glUniform2f);
    LOAD_GL(glUniform3f);
    LOAD_GL(glUniform4f);
    LOAD_GL(glUniformMatrix4fv);
    LOAD_GL(glActiveTexture);
    LOAD_GL(glGenVertexArrays);
    LOAD_GL(glBindVertexArray);
    LOAD_GL(glDeleteVertexArrays);
    LOAD_GL(glDrawBuffers);

#undef LOAD_GL

    g_gl_loaded = true;
}

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader_ptr(type);
    glShaderSource_ptr(shader, 1, &source, nullptr);
    glCompileShader_ptr(shader);

    GLint success;
    glGetShaderiv_ptr(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog_ptr(shader, 512, nullptr, log);
        glDeleteShader_ptr(shader);
        return 0;
    }

    return shader;
}

GLuint create_program(const char* vs_source, const char* fs_source) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_source);

    if (!vs || !fs) {
        if (vs) glDeleteShader_ptr(vs);
        if (fs) glDeleteShader_ptr(fs);
        return 0;
    }

    GLuint program = glCreateProgram_ptr();
    glAttachShader_ptr(program, vs);
    glAttachShader_ptr(program, fs);
    glLinkProgram_ptr(program);

    glDeleteShader_ptr(vs);
    glDeleteShader_ptr(fs);

    GLint success;
    glGetProgramiv_ptr(program, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram_ptr(program);
        return 0;
    }

    return program;
}
} // anonymous namespace

namespace void_render {

// =============================================================================
// Halton Sequence - For TAA jitter
// =============================================================================

class HaltonSequence {
public:
    explicit HaltonSequence(std::size_t base = 2) : m_base(base), m_index(0) {}

    [[nodiscard]] float next() {
        float result = 0.0f;
        float f = 1.0f / static_cast<float>(m_base);
        std::size_t i = m_index++;

        while (i > 0) {
            result += f * static_cast<float>(i % m_base);
            i /= m_base;
            f /= static_cast<float>(m_base);
        }

        return result;
    }

    void reset() { m_index = 0; }
    [[nodiscard]] std::size_t index() const noexcept { return m_index; }

private:
    std::size_t m_base;
    std::size_t m_index;
};

// =============================================================================
// TAA Configuration
// =============================================================================

struct TaaConfig {
    std::size_t sample_count = 16;       // Jitter sample count (power of 2)
    float feedback_min = 0.88f;          // Minimum history blend (sharper, more aliasing)
    float feedback_max = 0.97f;          // Maximum history blend (smoother, more ghosting)
    float motion_scale = 1.0f;           // Motion vector scale for rejection
    float velocity_weight = 60.0f;       // Weight for velocity-based rejection
    bool use_ycocg = true;               // Use YCoCg color space for clamping
    bool use_variance_clipping = true;   // Use variance-based neighborhood clamping
    float variance_gamma = 1.0f;         // Variance clamp gamma
    bool sharpen_output = true;          // Apply sharpening filter
    float sharpen_amount = 0.5f;         // Sharpening strength
    bool anti_flicker = true;            // Anti-flicker for static scenes
};

// =============================================================================
// TAA - Temporal Anti-Aliasing
// =============================================================================

class TemporalAA {
public:
    TemporalAA() : m_halton2(2), m_halton3(3) {}

    ~TemporalAA() {
        destroy();
    }

    /// Initialize with resolution
    bool init(std::uint32_t width, std::uint32_t height) {
        load_gl_functions();

        m_width = width;
        m_height = height;

        // Create history buffers (double-buffered)
        for (int i = 0; i < 2; ++i) {
            glGenTextures(1, &m_history_textures[i]);
            glBindTexture(GL_TEXTURE_2D, m_history_textures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        // Create resolve framebuffer
        glGenFramebuffers_ptr(1, &m_resolve_fbo);

        // Create shader
        m_shader = create_taa_shader();
        m_sharpen_shader = create_sharpen_shader();

        // Generate jitter samples
        generate_jitter_samples();

        return m_shader != 0;
    }

    /// Destroy resources
    void destroy() {
        if (m_resolve_fbo) {
            glDeleteFramebuffers_ptr(1, &m_resolve_fbo);
            m_resolve_fbo = 0;
        }

        for (auto& tex : m_history_textures) {
            if (tex) {
                glDeleteTextures(1, &tex);
                tex = 0;
            }
        }

        if (m_shader) {
            glDeleteProgram_ptr(m_shader);
            m_shader = 0;
        }

        if (m_sharpen_shader) {
            glDeleteProgram_ptr(m_sharpen_shader);
            m_sharpen_shader = 0;
        }
    }

    /// Get current jitter offset in clip space (-1 to 1)
    [[nodiscard]] std::array<float, 2> get_jitter() const {
        if (m_jitter_samples.empty()) return {{0, 0}};

        std::size_t idx = m_frame_index % m_jitter_samples.size();
        return m_jitter_samples[idx];
    }

    /// Get current jitter in pixels
    [[nodiscard]] std::array<float, 2> get_jitter_pixels() const {
        auto jitter = get_jitter();
        return {{
            jitter[0] * static_cast<float>(m_width) * 0.5f,
            jitter[1] * static_cast<float>(m_height) * 0.5f
        }};
    }

    /// Resolve TAA
    void resolve(
        GLuint current_color,     // Current frame color
        GLuint velocity_buffer,   // Screen-space velocity (RG16F)
        GLuint depth_buffer,      // Depth buffer
        GLuint output_texture,    // Output resolved texture
        const std::array<float, 16>& prev_view_proj,
        const std::array<float, 16>& curr_view_proj_inv) {

        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_resolve_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture, 0);

        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);

        glUseProgram_ptr(m_shader);

        // Bind textures
        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_color);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_current"), 0);

        glActiveTexture_ptr(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, m_history_textures[m_read_index]);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_history"), 1);

        glActiveTexture_ptr(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, velocity_buffer);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_velocity"), 2);

        glActiveTexture_ptr(GL_TEXTURE0 + 3);
        glBindTexture(GL_TEXTURE_2D, depth_buffer);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_depth"), 3);

        // Set uniforms
        glUniform2f_ptr(glGetUniformLocation_ptr(m_shader, "u_resolution"),
            static_cast<float>(m_width), static_cast<float>(m_height));

        glUniform2f_ptr(glGetUniformLocation_ptr(m_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height));

        auto jitter = get_jitter();
        glUniform2f_ptr(glGetUniformLocation_ptr(m_shader, "u_jitter"), jitter[0], jitter[1]);

        glUniform1f_ptr(glGetUniformLocation_ptr(m_shader, "u_feedback_min"), m_config.feedback_min);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_shader, "u_feedback_max"), m_config.feedback_max);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_shader, "u_velocity_weight"), m_config.velocity_weight);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_shader, "u_variance_gamma"), m_config.variance_gamma);

        glUniformMatrix4fv_ptr(glGetUniformLocation_ptr(m_shader, "u_prev_view_proj"), 1, GL_FALSE, prev_view_proj.data());
        glUniformMatrix4fv_ptr(glGetUniformLocation_ptr(m_shader, "u_curr_view_proj_inv"), 1, GL_FALSE, curr_view_proj_inv.data());

        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_use_ycocg"), m_config.use_ycocg ? 1 : 0);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_use_variance_clip"), m_config.use_variance_clipping ? 1 : 0);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_shader, "u_frame_index"), static_cast<int>(m_frame_index));

        // Draw fullscreen quad
        draw_fullscreen_quad();

        // Copy to history for next frame
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_resolve_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_history_textures[m_write_index], 0);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, output_texture);

        // Simple copy shader would go here
        // For now, we just swap indices

        // Swap read/write indices
        std::swap(m_read_index, m_write_index);

        // Apply sharpening if enabled
        if (m_config.sharpen_output) {
            apply_sharpening(output_texture);
        }

        m_frame_index++;

        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
    }

    /// Reset history (call on camera cut or teleport)
    void reset_history() {
        m_frame_index = 0;
        // Clear history textures would go here
    }

    /// Resize buffers
    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == m_width && height == m_height) return;

        destroy();
        init(width, height);
    }

    /// Configuration
    TaaConfig& config() noexcept { return m_config; }
    const TaaConfig& config() const noexcept { return m_config; }

private:
    TaaConfig m_config;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;

    GLuint m_history_textures[2] = {0, 0};
    int m_read_index = 0;
    int m_write_index = 1;

    GLuint m_resolve_fbo = 0;
    GLuint m_shader = 0;
    GLuint m_sharpen_shader = 0;
    GLuint m_quad_vao = 0;

    HaltonSequence m_halton2;
    HaltonSequence m_halton3;
    std::vector<std::array<float, 2>> m_jitter_samples;
    std::size_t m_frame_index = 0;

    void generate_jitter_samples() {
        m_jitter_samples.clear();
        m_jitter_samples.reserve(m_config.sample_count);

        m_halton2.reset();
        m_halton3.reset();

        for (std::size_t i = 0; i < m_config.sample_count; ++i) {
            // Halton sequence samples in [0, 1], convert to [-0.5, 0.5] clip space offset
            float x = (m_halton2.next() - 0.5f) / static_cast<float>(m_width);
            float y = (m_halton3.next() - 0.5f) / static_cast<float>(m_height);

            m_jitter_samples.push_back({{x * 2.0f, y * 2.0f}});
        }
    }

    void draw_fullscreen_quad() {
        // Bind empty VAO and draw triangle strip
        if (m_quad_vao == 0) {
            glGenVertexArrays_ptr(1, &m_quad_vao);
        }
        glBindVertexArray_ptr(m_quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray_ptr(0);
    }

    void apply_sharpening(GLuint texture) {
        glUseProgram_ptr(m_sharpen_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_sharpen_shader, "u_input"), 0);

        glUniform2f_ptr(glGetUniformLocation_ptr(m_sharpen_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height));

        glUniform1f_ptr(glGetUniformLocation_ptr(m_sharpen_shader, "u_amount"), m_config.sharpen_amount);

        draw_fullscreen_quad();
    }

    [[nodiscard]] GLuint create_taa_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec4 frag_color;

            uniform sampler2D u_current;
            uniform sampler2D u_history;
            uniform sampler2D u_velocity;
            uniform sampler2D u_depth;

            uniform vec2 u_resolution;
            uniform vec2 u_texel_size;
            uniform vec2 u_jitter;

            uniform float u_feedback_min;
            uniform float u_feedback_max;
            uniform float u_velocity_weight;
            uniform float u_variance_gamma;

            uniform mat4 u_prev_view_proj;
            uniform mat4 u_curr_view_proj_inv;

            uniform int u_use_ycocg;
            uniform int u_use_variance_clip;
            uniform int u_frame_index;

            // RGB to YCoCg
            vec3 rgb_to_ycocg(vec3 rgb) {
                return vec3(
                    0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
                    0.5 * rgb.r - 0.5 * rgb.b,
                    -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
                );
            }

            // YCoCg to RGB
            vec3 ycocg_to_rgb(vec3 ycocg) {
                return vec3(
                    ycocg.x + ycocg.y - ycocg.z,
                    ycocg.x + ycocg.z,
                    ycocg.x - ycocg.y - ycocg.z
                );
            }

            // Neighborhood clamping
            vec3 clip_aabb(vec3 color, vec3 minimum, vec3 maximum) {
                vec3 center = (minimum + maximum) * 0.5;
                vec3 extents = (maximum - minimum) * 0.5;

                vec3 offset = color - center;
                vec3 ts = abs(extents / (offset + 0.0001));
                float t = min(min(ts.x, ts.y), ts.z);

                return center + offset * clamp(t, 0.0, 1.0);
            }

            void main() {
                // Remove jitter for current frame
                vec2 uv = v_uv;

                // Sample velocity
                vec2 velocity = texture(u_velocity, uv).rg;

                // Reproject to previous frame
                vec2 prev_uv = uv - velocity;

                // Check if previous UV is valid
                if (prev_uv.x < 0.0 || prev_uv.x > 1.0 || prev_uv.y < 0.0 || prev_uv.y > 1.0) {
                    frag_color = texture(u_current, uv);
                    return;
                }

                // Sample current frame
                vec3 current = texture(u_current, uv).rgb;

                // Sample history
                vec3 history = texture(u_history, prev_uv).rgb;

                // Neighborhood sampling (3x3)
                vec3 samples[9];
                vec2 offsets[9] = vec2[](
                    vec2(-1, -1), vec2(0, -1), vec2(1, -1),
                    vec2(-1, 0),  vec2(0, 0),  vec2(1, 0),
                    vec2(-1, 1),  vec2(0, 1),  vec2(1, 1)
                );

                for (int i = 0; i < 9; i++) {
                    samples[i] = texture(u_current, uv + offsets[i] * u_texel_size).rgb;
                    if (u_use_ycocg == 1) {
                        samples[i] = rgb_to_ycocg(samples[i]);
                    }
                }

                vec3 current_ycocg = u_use_ycocg == 1 ? rgb_to_ycocg(current) : current;
                vec3 history_ycocg = u_use_ycocg == 1 ? rgb_to_ycocg(history) : history;

                // Calculate neighborhood bounds
                vec3 min_color = samples[0];
                vec3 max_color = samples[0];

                for (int i = 1; i < 9; i++) {
                    min_color = min(min_color, samples[i]);
                    max_color = max(max_color, samples[i]);
                }

                // Variance clipping
                if (u_use_variance_clip == 1) {
                    vec3 mean = vec3(0.0);
                    vec3 sq_mean = vec3(0.0);

                    for (int i = 0; i < 9; i++) {
                        mean += samples[i];
                        sq_mean += samples[i] * samples[i];
                    }

                    mean /= 9.0;
                    sq_mean /= 9.0;

                    vec3 variance = sqrt(max(sq_mean - mean * mean, vec3(0.0)));

                    min_color = mean - variance * u_variance_gamma;
                    max_color = mean + variance * u_variance_gamma;
                }

                // Clip history to neighborhood
                vec3 clipped_history = clip_aabb(history_ycocg, min_color, max_color);

                // Convert back to RGB
                if (u_use_ycocg == 1) {
                    current = ycocg_to_rgb(current_ycocg);
                    clipped_history = ycocg_to_rgb(clipped_history);
                }

                // Calculate blend factor based on velocity
                float velocity_length = length(velocity * u_resolution);
                float feedback = mix(u_feedback_max, u_feedback_min,
                    clamp(velocity_length * u_velocity_weight, 0.0, 1.0));

                // Blend
                vec3 result = mix(current, clipped_history, feedback);

                frag_color = vec4(result, 1.0);
            }
        )";

        return create_program(vs_source, fs_source);
    }

    [[nodiscard]] GLuint create_sharpen_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec4 frag_color;

            uniform sampler2D u_input;
            uniform vec2 u_texel_size;
            uniform float u_amount;

            void main() {
                vec3 center = texture(u_input, v_uv).rgb;

                vec3 top = texture(u_input, v_uv + vec2(0, -u_texel_size.y)).rgb;
                vec3 bottom = texture(u_input, v_uv + vec2(0, u_texel_size.y)).rgb;
                vec3 left = texture(u_input, v_uv + vec2(-u_texel_size.x, 0)).rgb;
                vec3 right = texture(u_input, v_uv + vec2(u_texel_size.x, 0)).rgb;

                vec3 edge = 4.0 * center - top - bottom - left - right;
                vec3 sharpened = center + edge * u_amount;

                frag_color = vec4(max(sharpened, vec3(0.0)), 1.0);
            }
        )";

        return create_program(vs_source, fs_source);
    }
};

// =============================================================================
// Motion Blur Configuration
// =============================================================================

struct MotionBlurConfig {
    std::size_t sample_count = 16;        // Blur samples per pixel
    float intensity = 1.0f;               // Blur intensity multiplier
    float max_blur_radius = 32.0f;        // Maximum blur radius in pixels
    float shutter_angle = 180.0f;         // Camera shutter angle (degrees)
    bool object_blur = true;              // Enable per-object motion blur
    bool camera_blur = true;              // Enable camera motion blur
    float depth_scale = 0.1f;             // Depth-based blur scaling
    bool use_tile_max = true;             // Use tile-based max velocity
    std::uint32_t tile_size = 20;         // Tile size for max velocity
};

// =============================================================================
// Motion Blur
// =============================================================================

class MotionBlur {
public:
    ~MotionBlur() {
        destroy();
    }

    /// Initialize
    bool init(std::uint32_t width, std::uint32_t height) {
        load_gl_functions();

        m_width = width;
        m_height = height;

        // Create tile max textures
        m_tile_width = (width + m_config.tile_size - 1) / m_config.tile_size;
        m_tile_height = (height + m_config.tile_size - 1) / m_config.tile_size;

        glGenTextures(1, &m_tile_max_texture);
        glBindTexture(GL_TEXTURE_2D, m_tile_max_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, m_tile_width, m_tile_height, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glGenTextures(1, &m_neighbor_max_texture);
        glBindTexture(GL_TEXTURE_2D, m_neighbor_max_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, m_tile_width, m_tile_height, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Create framebuffers
        glGenFramebuffers_ptr(1, &m_tile_fbo);
        glGenFramebuffers_ptr(1, &m_blur_fbo);

        // Create shaders
        m_tile_max_shader = create_tile_max_shader();
        m_neighbor_max_shader = create_neighbor_max_shader();
        m_blur_shader = create_motion_blur_shader();

        return m_blur_shader != 0;
    }

    /// Destroy resources
    void destroy() {
        if (m_tile_max_texture) {
            glDeleteTextures(1, &m_tile_max_texture);
            m_tile_max_texture = 0;
        }
        if (m_neighbor_max_texture) {
            glDeleteTextures(1, &m_neighbor_max_texture);
            m_neighbor_max_texture = 0;
        }
        if (m_tile_fbo) {
            glDeleteFramebuffers_ptr(1, &m_tile_fbo);
            m_tile_fbo = 0;
        }
        if (m_blur_fbo) {
            glDeleteFramebuffers_ptr(1, &m_blur_fbo);
            m_blur_fbo = 0;
        }
        if (m_tile_max_shader) {
            glDeleteProgram_ptr(m_tile_max_shader);
            m_tile_max_shader = 0;
        }
        if (m_neighbor_max_shader) {
            glDeleteProgram_ptr(m_neighbor_max_shader);
            m_neighbor_max_shader = 0;
        }
        if (m_blur_shader) {
            glDeleteProgram_ptr(m_blur_shader);
            m_blur_shader = 0;
        }
    }

    /// Apply motion blur
    void apply(
        GLuint color_texture,
        GLuint velocity_texture,
        GLuint depth_texture,
        GLuint output_texture) {

        // Pass 1: Tile max velocity
        if (m_config.use_tile_max) {
            compute_tile_max(velocity_texture);
            compute_neighbor_max();
        }

        // Pass 2: Motion blur
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_blur_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture, 0);

        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);

        glUseProgram_ptr(m_blur_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_color"), 0);

        glActiveTexture_ptr(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, velocity_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_velocity"), 1);

        glActiveTexture_ptr(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_depth"), 2);

        if (m_config.use_tile_max) {
            glActiveTexture_ptr(GL_TEXTURE0 + 3);
            glBindTexture(GL_TEXTURE_2D, m_neighbor_max_texture);
            glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_tile_max"), 3);
        }

        glUniform2f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_resolution"),
            static_cast<float>(m_width), static_cast<float>(m_height));
        glUniform2f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height));

        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_sample_count"), static_cast<int>(m_config.sample_count));
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_intensity"), m_config.intensity);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_max_blur"), m_config.max_blur_radius);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_shutter_angle"), m_config.shutter_angle / 360.0f);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_depth_scale"), m_config.depth_scale);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_use_tile_max"), m_config.use_tile_max ? 1 : 0);

        draw_fullscreen_quad();

        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
    }

    /// Resize
    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == m_width && height == m_height) return;
        destroy();
        init(width, height);
    }

    /// Configuration
    MotionBlurConfig& config() noexcept { return m_config; }

private:
    MotionBlurConfig m_config;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    std::uint32_t m_tile_width = 0;
    std::uint32_t m_tile_height = 0;

    GLuint m_tile_max_texture = 0;
    GLuint m_neighbor_max_texture = 0;
    GLuint m_tile_fbo = 0;
    GLuint m_blur_fbo = 0;

    GLuint m_tile_max_shader = 0;
    GLuint m_neighbor_max_shader = 0;
    GLuint m_blur_shader = 0;

    GLuint m_quad_vao = 0;

    void draw_fullscreen_quad() {
        if (m_quad_vao == 0) {
            glGenVertexArrays_ptr(1, &m_quad_vao);
        }
        glBindVertexArray_ptr(m_quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray_ptr(0);
    }

    void compute_tile_max(GLuint velocity_texture) {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_tile_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tile_max_texture, 0);

        glViewport(0, 0, m_tile_width, m_tile_height);
        glUseProgram_ptr(m_tile_max_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, velocity_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_tile_max_shader, "u_velocity"), 0);

        glUniform2f_ptr(glGetUniformLocation_ptr(m_tile_max_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height));
        glUniform1i_ptr(glGetUniformLocation_ptr(m_tile_max_shader, "u_tile_size"), m_config.tile_size);

        draw_fullscreen_quad();
    }

    void compute_neighbor_max() {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_tile_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_neighbor_max_texture, 0);

        glViewport(0, 0, m_tile_width, m_tile_height);
        glUseProgram_ptr(m_neighbor_max_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_tile_max_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_neighbor_max_shader, "u_tile_max"), 0);

        glUniform2f_ptr(glGetUniformLocation_ptr(m_neighbor_max_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_tile_width), 1.0f / static_cast<float>(m_tile_height));

        draw_fullscreen_quad();
    }

    [[nodiscard]] GLuint create_tile_max_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec2 frag_velocity;

            uniform sampler2D u_velocity;
            uniform vec2 u_texel_size;
            uniform int u_tile_size;

            void main() {
                vec2 max_velocity = vec2(0.0);
                float max_len = 0.0;

                ivec2 base_coord = ivec2(v_uv / u_texel_size) * u_tile_size;

                for (int y = 0; y < u_tile_size; y++) {
                    for (int x = 0; x < u_tile_size; x++) {
                        vec2 coord = (vec2(base_coord + ivec2(x, y)) + 0.5) * u_texel_size;
                        vec2 vel = texture(u_velocity, coord).rg;
                        float len = dot(vel, vel);

                        if (len > max_len) {
                            max_len = len;
                            max_velocity = vel;
                        }
                    }
                }

                frag_velocity = max_velocity;
            }
        )";

        return create_program(vs_source, fs_source);
    }

    [[nodiscard]] GLuint create_neighbor_max_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec2 frag_velocity;

            uniform sampler2D u_tile_max;
            uniform vec2 u_texel_size;

            void main() {
                vec2 max_velocity = vec2(0.0);
                float max_len = 0.0;

                for (int y = -1; y <= 1; y++) {
                    for (int x = -1; x <= 1; x++) {
                        vec2 coord = v_uv + vec2(x, y) * u_texel_size;
                        vec2 vel = texture(u_tile_max, coord).rg;
                        float len = dot(vel, vel);

                        if (len > max_len) {
                            max_len = len;
                            max_velocity = vel;
                        }
                    }
                }

                frag_velocity = max_velocity;
            }
        )";

        return create_program(vs_source, fs_source);
    }

    [[nodiscard]] GLuint create_motion_blur_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec4 frag_color;

            uniform sampler2D u_color;
            uniform sampler2D u_velocity;
            uniform sampler2D u_depth;
            uniform sampler2D u_tile_max;

            uniform vec2 u_resolution;
            uniform vec2 u_texel_size;

            uniform int u_sample_count;
            uniform float u_intensity;
            uniform float u_max_blur;
            uniform float u_shutter_angle;
            uniform float u_depth_scale;
            uniform int u_use_tile_max;

            // Interleaved gradient noise for dithering
            float interleaved_gradient_noise(vec2 pos) {
                vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
                return fract(magic.z * fract(dot(pos, magic.xy)));
            }

            void main() {
                vec2 velocity = texture(u_velocity, v_uv).rg * u_intensity * u_shutter_angle;

                // Clamp velocity to max blur radius
                float vel_len = length(velocity * u_resolution);
                if (vel_len > u_max_blur) {
                    velocity *= u_max_blur / vel_len;
                }

                // Check tile max for early out
                if (u_use_tile_max == 1) {
                    vec2 tile_vel = texture(u_tile_max, v_uv).rg;
                    float tile_len = length(tile_vel * u_resolution);
                    if (tile_len < 1.0) {
                        frag_color = texture(u_color, v_uv);
                        return;
                    }
                }

                // Sample along velocity direction
                vec4 result = vec4(0.0);
                float total_weight = 0.0;

                float center_depth = texture(u_depth, v_uv).r;

                // Dithered offset
                float dither = interleaved_gradient_noise(gl_FragCoord.xy);

                for (int i = 0; i < u_sample_count; i++) {
                    float t = (float(i) + dither) / float(u_sample_count) - 0.5;
                    vec2 offset = velocity * t;
                    vec2 sample_uv = v_uv + offset;

                    // Depth weight (prefer samples at similar depth)
                    float sample_depth = texture(u_depth, sample_uv).r;
                    float depth_diff = abs(center_depth - sample_depth);
                    float depth_weight = 1.0 / (1.0 + depth_diff * u_depth_scale * 1000.0);

                    vec4 sample_color = texture(u_color, sample_uv);
                    float weight = depth_weight;

                    result += sample_color * weight;
                    total_weight += weight;
                }

                frag_color = result / max(total_weight, 0.0001);
            }
        )";

        return create_program(vs_source, fs_source);
    }
};

// =============================================================================
// Depth of Field Configuration
// =============================================================================

struct DofConfig {
    float focus_distance = 10.0f;         // Distance to focal plane
    float focus_range = 5.0f;             // Range of acceptable sharpness
    float aperture = 2.8f;                // F-stop (lower = more blur)
    float bokeh_threshold = 0.5f;         // Luminance threshold for bokeh highlights
    float bokeh_intensity = 1.0f;         // Bokeh highlight intensity
    bool use_physical = true;             // Use physically-based CoC calculation
    float sensor_height = 0.024f;         // Sensor height in meters (35mm = 0.024)
    float focal_length = 0.050f;          // Lens focal length in meters
    std::size_t blur_quality = 2;         // Blur quality (1-3)
    bool enable_near_blur = true;         // Blur objects in front of focus
    bool enable_far_blur = true;          // Blur objects behind focus
    float max_coc = 32.0f;                // Maximum circle of confusion (pixels)
    bool bokeh_shape_hex = false;         // Use hexagonal bokeh (vs circular)
};

// =============================================================================
// Depth of Field
// =============================================================================

class DepthOfField {
public:
    ~DepthOfField() {
        destroy();
    }

    /// Initialize
    bool init(std::uint32_t width, std::uint32_t height) {
        load_gl_functions();

        m_width = width;
        m_height = height;

        // Create CoC texture
        glGenTextures(1, &m_coc_texture);
        glBindTexture(GL_TEXTURE_2D, m_coc_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Create downsampled textures (half resolution)
        m_half_width = width / 2;
        m_half_height = height / 2;

        glGenTextures(1, &m_near_texture);
        glBindTexture(GL_TEXTURE_2D, m_near_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_half_width, m_half_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenTextures(1, &m_far_texture);
        glBindTexture(GL_TEXTURE_2D, m_far_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_half_width, m_half_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Blurred textures
        glGenTextures(1, &m_near_blur_texture);
        glBindTexture(GL_TEXTURE_2D, m_near_blur_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_half_width, m_half_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenTextures(1, &m_far_blur_texture);
        glBindTexture(GL_TEXTURE_2D, m_far_blur_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_half_width, m_half_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Framebuffers
        glGenFramebuffers_ptr(1, &m_coc_fbo);
        glGenFramebuffers_ptr(1, &m_downsample_fbo);
        glGenFramebuffers_ptr(1, &m_blur_fbo);
        glGenFramebuffers_ptr(1, &m_composite_fbo);

        // Shaders
        m_coc_shader = create_coc_shader();
        m_downsample_shader = create_downsample_shader();
        m_blur_shader = create_blur_shader();
        m_composite_shader = create_composite_shader();

        return m_composite_shader != 0;
    }

    /// Destroy resources
    void destroy() {
        GLuint textures[] = {m_coc_texture, m_near_texture, m_far_texture, m_near_blur_texture, m_far_blur_texture};
        for (auto tex : textures) {
            if (tex) glDeleteTextures(1, &tex);
        }
        m_coc_texture = m_near_texture = m_far_texture = m_near_blur_texture = m_far_blur_texture = 0;

        GLuint fbos[] = {m_coc_fbo, m_downsample_fbo, m_blur_fbo, m_composite_fbo};
        for (auto fbo : fbos) {
            if (fbo) glDeleteFramebuffers_ptr(1, &fbo);
        }
        m_coc_fbo = m_downsample_fbo = m_blur_fbo = m_composite_fbo = 0;

        GLuint shaders[] = {m_coc_shader, m_downsample_shader, m_blur_shader, m_composite_shader};
        for (auto shader : shaders) {
            if (shader) glDeleteProgram_ptr(shader);
        }
        m_coc_shader = m_downsample_shader = m_blur_shader = m_composite_shader = 0;
    }

    /// Apply depth of field
    void apply(
        GLuint color_texture,
        GLuint depth_texture,
        GLuint output_texture,
        float near_plane,
        float far_plane) {

        // Pass 1: Calculate CoC
        calculate_coc(depth_texture, near_plane, far_plane);

        // Pass 2: Downsample and separate near/far
        downsample_separate(color_texture);

        // Pass 3: Blur near and far
        blur_layer(m_near_texture, m_near_blur_texture, true);
        blur_layer(m_far_texture, m_far_blur_texture, false);

        // Pass 4: Composite
        composite(color_texture, output_texture);
    }

    /// Set focus to specific world position
    void focus_at(float distance) {
        m_config.focus_distance = distance;
    }

    /// Calculate focus distance for autofocus
    [[nodiscard]] float calculate_autofocus_distance(
        GLuint depth_texture,
        float near_plane,
        float far_plane,
        std::array<float, 2> focus_point = {{0.5f, 0.5f}}) const {

        // Sample depth at focus point
        float depth = 0.5f;  // Would read from texture in real implementation

        // Convert to linear depth
        float linear_depth = (2.0f * near_plane * far_plane) /
            (far_plane + near_plane - depth * (far_plane - near_plane));

        return linear_depth;
    }

    /// Resize
    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == m_width && height == m_height) return;
        destroy();
        init(width, height);
    }

    /// Configuration
    DofConfig& config() noexcept { return m_config; }

private:
    DofConfig m_config;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    std::uint32_t m_half_width = 0;
    std::uint32_t m_half_height = 0;

    GLuint m_coc_texture = 0;
    GLuint m_near_texture = 0;
    GLuint m_far_texture = 0;
    GLuint m_near_blur_texture = 0;
    GLuint m_far_blur_texture = 0;

    GLuint m_coc_fbo = 0;
    GLuint m_downsample_fbo = 0;
    GLuint m_blur_fbo = 0;
    GLuint m_composite_fbo = 0;

    GLuint m_coc_shader = 0;
    GLuint m_downsample_shader = 0;
    GLuint m_blur_shader = 0;
    GLuint m_composite_shader = 0;

    GLuint m_quad_vao = 0;

    void draw_fullscreen_quad() {
        if (m_quad_vao == 0) {
            glGenVertexArrays_ptr(1, &m_quad_vao);
        }
        glBindVertexArray_ptr(m_quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray_ptr(0);
    }

    void calculate_coc(GLuint depth_texture, float near_plane, float far_plane) {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_coc_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_coc_texture, 0);

        glViewport(0, 0, m_width, m_height);
        glUseProgram_ptr(m_coc_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_depth"), 0);

        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_focus_distance"), m_config.focus_distance);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_focus_range"), m_config.focus_range);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_aperture"), m_config.aperture);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_focal_length"), m_config.focal_length);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_sensor_height"), m_config.sensor_height);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_near_plane"), near_plane);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_far_plane"), far_plane);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_max_coc"), m_config.max_coc);
        glUniform2f_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_resolution"),
            static_cast<float>(m_width), static_cast<float>(m_height));
        glUniform1i_ptr(glGetUniformLocation_ptr(m_coc_shader, "u_use_physical"), m_config.use_physical ? 1 : 0);

        draw_fullscreen_quad();
    }

    void downsample_separate(GLuint color_texture) {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_downsample_fbo);

        GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1};
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_near_texture, 0);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, m_far_texture, 0);
        glDrawBuffers_ptr(2, buffers);

        glViewport(0, 0, m_half_width, m_half_height);
        glUseProgram_ptr(m_downsample_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_downsample_shader, "u_color"), 0);

        glActiveTexture_ptr(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, m_coc_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_downsample_shader, "u_coc"), 1);

        glUniform2f_ptr(glGetUniformLocation_ptr(m_downsample_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height));

        draw_fullscreen_quad();

        // Reset draw buffers
        GLenum single_buffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers_ptr(1, &single_buffer);
    }

    void blur_layer(GLuint input, GLuint output, bool is_near) {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_blur_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);

        glViewport(0, 0, m_half_width, m_half_height);
        glUseProgram_ptr(m_blur_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_input"), 0);

        glActiveTexture_ptr(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, m_coc_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_coc"), 1);

        glUniform2f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_texel_size"),
            1.0f / static_cast<float>(m_half_width), 1.0f / static_cast<float>(m_half_height));
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_max_coc"), m_config.max_coc * 0.5f);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_quality"), static_cast<int>(m_config.blur_quality));
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_is_near"), is_near ? 1 : 0);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_bokeh_threshold"), m_config.bokeh_threshold);
        glUniform1f_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_bokeh_intensity"), m_config.bokeh_intensity);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_blur_shader, "u_hex_bokeh"), m_config.bokeh_shape_hex ? 1 : 0);

        draw_fullscreen_quad();
    }

    void composite(GLuint color_texture, GLuint output) {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_composite_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);

        glViewport(0, 0, m_width, m_height);
        glUseProgram_ptr(m_composite_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_composite_shader, "u_color"), 0);

        glActiveTexture_ptr(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, m_coc_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_composite_shader, "u_coc"), 1);

        glActiveTexture_ptr(GL_TEXTURE0 + 2);
        glBindTexture(GL_TEXTURE_2D, m_near_blur_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_composite_shader, "u_near_blur"), 2);

        glActiveTexture_ptr(GL_TEXTURE0 + 3);
        glBindTexture(GL_TEXTURE_2D, m_far_blur_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_composite_shader, "u_far_blur"), 3);

        glUniform1i_ptr(glGetUniformLocation_ptr(m_composite_shader, "u_enable_near"), m_config.enable_near_blur ? 1 : 0);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_composite_shader, "u_enable_far"), m_config.enable_far_blur ? 1 : 0);

        draw_fullscreen_quad();

        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
    }

    [[nodiscard]] GLuint create_coc_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out float frag_coc;

            uniform sampler2D u_depth;
            uniform float u_focus_distance;
            uniform float u_focus_range;
            uniform float u_aperture;
            uniform float u_focal_length;
            uniform float u_sensor_height;
            uniform float u_near_plane;
            uniform float u_far_plane;
            uniform float u_max_coc;
            uniform vec2 u_resolution;
            uniform int u_use_physical;

            float linearize_depth(float d) {
                return (2.0 * u_near_plane * u_far_plane) /
                    (u_far_plane + u_near_plane - d * (u_far_plane - u_near_plane));
            }

            void main() {
                float depth = texture(u_depth, v_uv).r;
                float linear_depth = linearize_depth(depth);

                float coc;

                if (u_use_physical == 1) {
                    // Physical CoC calculation
                    // CoC = |A * f * (S - P) / (P * (S - f))|
                    // A = aperture diameter, f = focal length, S = subject distance, P = focus distance

                    float A = u_focal_length / u_aperture;  // Aperture diameter
                    float S = linear_depth;
                    float P = u_focus_distance;
                    float f = u_focal_length;

                    float numerator = abs(A * f * (S - P));
                    float denominator = S * (P - f);

                    if (abs(denominator) > 0.0001) {
                        coc = numerator / denominator;
                        // Convert to pixels
                        coc = coc * u_resolution.y / u_sensor_height;
                    } else {
                        coc = 0.0;
                    }
                } else {
                    // Simple artistic CoC
                    float diff = linear_depth - u_focus_distance;
                    coc = diff / u_focus_range;
                    coc = clamp(coc, -1.0, 1.0);
                    coc = coc * u_max_coc;
                }

                // Clamp to max CoC
                coc = clamp(coc, -u_max_coc, u_max_coc);

                // Output signed CoC (negative = near field, positive = far field)
                frag_coc = coc / u_max_coc;  // Normalize to [-1, 1]
            }
        )";

        return create_program(vs_source, fs_source);
    }

    [[nodiscard]] GLuint create_downsample_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;

            layout(location = 0) out vec4 frag_near;
            layout(location = 1) out vec4 frag_far;

            uniform sampler2D u_color;
            uniform sampler2D u_coc;
            uniform vec2 u_texel_size;

            void main() {
                // 4-tap bilinear downsample
                vec3 color = vec3(0.0);
                float coc_sum = 0.0;

                vec2 offsets[4] = vec2[](
                    vec2(-0.5, -0.5), vec2(0.5, -0.5),
                    vec2(-0.5, 0.5), vec2(0.5, 0.5)
                );

                for (int i = 0; i < 4; i++) {
                    vec2 sample_uv = v_uv + offsets[i] * u_texel_size;
                    color += texture(u_color, sample_uv).rgb;
                    coc_sum += texture(u_coc, sample_uv).r;
                }

                color *= 0.25;
                float coc = coc_sum * 0.25;

                // Separate near and far based on CoC sign
                float near_coc = max(-coc, 0.0);  // Near field (negative CoC made positive)
                float far_coc = max(coc, 0.0);    // Far field (positive CoC)

                frag_near = vec4(color, near_coc);
                frag_far = vec4(color, far_coc);
            }
        )";

        return create_program(vs_source, fs_source);
    }

    [[nodiscard]] GLuint create_blur_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec4 frag_color;

            uniform sampler2D u_input;
            uniform sampler2D u_coc;
            uniform vec2 u_texel_size;
            uniform float u_max_coc;
            uniform int u_quality;
            uniform int u_is_near;
            uniform float u_bokeh_threshold;
            uniform float u_bokeh_intensity;
            uniform int u_hex_bokeh;

            const float PI = 3.14159265;

            // Disk kernel (Poisson disk)
            vec2 disk_kernel[16] = vec2[](
                vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
                vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
                vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
                vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
                vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
                vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
                vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
                vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
            );

            void main() {
                vec4 center = texture(u_input, v_uv);
                float center_coc = center.a;

                if (center_coc < 0.01) {
                    frag_color = center;
                    return;
                }

                float blur_radius = center_coc * u_max_coc;

                // Calculate sample count based on quality
                int samples = u_quality == 1 ? 8 : (u_quality == 2 ? 16 : 32);

                vec4 result = vec4(0.0);
                float total_weight = 0.0;

                for (int i = 0; i < samples; i++) {
                    vec2 offset;
                    if (u_hex_bokeh == 1) {
                        // Hexagonal pattern
                        float angle = float(i) / float(samples) * PI * 2.0;
                        float r = sqrt(float(i + 1) / float(samples));
                        offset = vec2(cos(angle), sin(angle)) * r;
                    } else {
                        offset = disk_kernel[i % 16];
                    }

                    vec2 sample_uv = v_uv + offset * blur_radius * u_texel_size;
                    vec4 sample_color = texture(u_input, sample_uv);

                    // Weight based on CoC
                    float sample_coc = sample_color.a;
                    float weight = 1.0;

                    if (u_is_near == 1) {
                        // Near field: sample CoC must be >= center CoC
                        weight = step(center_coc * 0.5, sample_coc);
                    }

                    // Bokeh highlighting
                    float luma = dot(sample_color.rgb, vec3(0.299, 0.587, 0.114));
                    if (luma > u_bokeh_threshold) {
                        weight *= 1.0 + (luma - u_bokeh_threshold) * u_bokeh_intensity;
                    }

                    result += sample_color * weight;
                    total_weight += weight;
                }

                result /= max(total_weight, 0.0001);
                result.a = center_coc;

                frag_color = result;
            }
        )";

        return create_program(vs_source, fs_source);
    }

    [[nodiscard]] GLuint create_composite_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec4 frag_color;

            uniform sampler2D u_color;
            uniform sampler2D u_coc;
            uniform sampler2D u_near_blur;
            uniform sampler2D u_far_blur;

            uniform int u_enable_near;
            uniform int u_enable_far;

            void main() {
                vec3 color = texture(u_color, v_uv).rgb;
                float coc = texture(u_coc, v_uv).r;

                vec4 near_blur = texture(u_near_blur, v_uv);
                vec4 far_blur = texture(u_far_blur, v_uv);

                vec3 result = color;

                // Blend far field
                if (u_enable_far == 1 && coc > 0.0) {
                    float far_blend = smoothstep(0.0, 1.0, far_blur.a * 2.0);
                    result = mix(result, far_blur.rgb, far_blend);
                }

                // Blend near field (on top)
                if (u_enable_near == 1 && coc < 0.0) {
                    float near_blend = smoothstep(0.0, 1.0, near_blur.a * 2.0);
                    result = mix(result, near_blur.rgb, near_blend);
                }

                frag_color = vec4(result, 1.0);
            }
        )";

        return create_program(vs_source, fs_source);
    }
};

// =============================================================================
// Velocity Buffer Generator
// =============================================================================

class VelocityBuffer {
public:
    ~VelocityBuffer() {
        destroy();
    }

    /// Initialize
    bool init(std::uint32_t width, std::uint32_t height) {
        load_gl_functions();

        m_width = width;
        m_height = height;

        // Create velocity texture (RG16F - 2D velocity)
        glGenTextures(1, &m_velocity_texture);
        glBindTexture(GL_TEXTURE_2D, m_velocity_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create framebuffer
        glGenFramebuffers_ptr(1, &m_fbo);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D_ptr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_velocity_texture, 0);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);

        // Create shader for camera velocity
        m_camera_velocity_shader = create_camera_velocity_shader();

        return m_camera_velocity_shader != 0;
    }

    /// Destroy
    void destroy() {
        if (m_velocity_texture) {
            glDeleteTextures(1, &m_velocity_texture);
            m_velocity_texture = 0;
        }
        if (m_fbo) {
            glDeleteFramebuffers_ptr(1, &m_fbo);
            m_fbo = 0;
        }
        if (m_camera_velocity_shader) {
            glDeleteProgram_ptr(m_camera_velocity_shader);
            m_camera_velocity_shader = 0;
        }
    }

    /// Get velocity texture
    [[nodiscard]] GLuint texture() const noexcept { return m_velocity_texture; }

    /// Get framebuffer for rendering object velocities
    [[nodiscard]] GLuint framebuffer() const noexcept { return m_fbo; }

    /// Clear velocity buffer
    void clear() {
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_fbo);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
    }

    /// Generate camera-only velocity (for scenes without per-object velocity)
    void generate_camera_velocity(
        GLuint depth_texture,
        const std::array<float, 16>& curr_view_proj_inv,
        const std::array<float, 16>& prev_view_proj) {

        glBindFramebuffer_ptr(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);

        glUseProgram_ptr(m_camera_velocity_shader);

        glActiveTexture_ptr(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depth_texture);
        glUniform1i_ptr(glGetUniformLocation_ptr(m_camera_velocity_shader, "u_depth"), 0);

        glUniformMatrix4fv_ptr(glGetUniformLocation_ptr(m_camera_velocity_shader, "u_curr_view_proj_inv"),
            1, GL_FALSE, curr_view_proj_inv.data());
        glUniformMatrix4fv_ptr(glGetUniformLocation_ptr(m_camera_velocity_shader, "u_prev_view_proj"),
            1, GL_FALSE, prev_view_proj.data());

        draw_fullscreen_quad();

        glBindFramebuffer_ptr(GL_FRAMEBUFFER, 0);
    }

    /// Store previous frame matrices
    void store_matrices(const std::array<float, 16>& view_proj) {
        m_prev_view_proj = view_proj;
    }

    [[nodiscard]] const std::array<float, 16>& prev_view_proj() const noexcept {
        return m_prev_view_proj;
    }

    /// Resize
    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == m_width && height == m_height) return;
        destroy();
        init(width, height);
    }

private:
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;

    GLuint m_velocity_texture = 0;
    GLuint m_fbo = 0;
    GLuint m_camera_velocity_shader = 0;
    GLuint m_quad_vao = 0;

    std::array<float, 16> m_prev_view_proj = {{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    }};

    void draw_fullscreen_quad() {
        if (m_quad_vao == 0) {
            glGenVertexArrays_ptr(1, &m_quad_vao);
        }
        glBindVertexArray_ptr(m_quad_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray_ptr(0);
    }

    [[nodiscard]] GLuint create_camera_velocity_shader() {
        const char* vs_source = R"(
            #version 330 core
            out vec2 v_uv;
            void main() {
                vec2 pos = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;
                v_uv = pos * 0.5 + 0.5;
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        const char* fs_source = R"(
            #version 330 core
            in vec2 v_uv;
            out vec2 frag_velocity;

            uniform sampler2D u_depth;
            uniform mat4 u_curr_view_proj_inv;
            uniform mat4 u_prev_view_proj;

            void main() {
                float depth = texture(u_depth, v_uv).r;

                // Reconstruct world position
                vec4 clip_pos = vec4(v_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
                vec4 world_pos = u_curr_view_proj_inv * clip_pos;
                world_pos /= world_pos.w;

                // Project to previous frame
                vec4 prev_clip = u_prev_view_proj * world_pos;
                vec2 prev_uv = (prev_clip.xy / prev_clip.w) * 0.5 + 0.5;

                // Calculate velocity
                frag_velocity = v_uv - prev_uv;
            }
        )";

        return create_program(vs_source, fs_source);
    }
};

} // namespace void_render
