/// @file post_process.cpp
/// @brief Post-processing effects implementation (bloom, SSAO, tonemapping, FXAA)

#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/render/texture.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <array>
#include <random>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA16F 0x881A
#define GL_RGBA 0x1908
#define GL_FLOAT 0x1406
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TRIANGLES 0x0004

#define DECLARE_GL_FUNC(ret, name, ...) \
    typedef ret (APIENTRY *PFN_##name)(__VA_ARGS__); \
    static PFN_##name pfn_##name = nullptr;

DECLARE_GL_FUNC(void, glGenFramebuffers, GLsizei n, GLuint* framebuffers)
DECLARE_GL_FUNC(void, glDeleteFramebuffers, GLsizei n, const GLuint* framebuffers)
DECLARE_GL_FUNC(void, glBindFramebuffer, GLenum target, GLuint framebuffer)
DECLARE_GL_FUNC(void, glFramebufferTexture2D, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
DECLARE_GL_FUNC(GLenum, glCheckFramebufferStatus, GLenum target)
DECLARE_GL_FUNC(void, glGenTextures, GLsizei n, GLuint* textures)
DECLARE_GL_FUNC(void, glDeleteTextures, GLsizei n, const GLuint* textures)
DECLARE_GL_FUNC(void, glBindTexture, GLenum target, GLuint texture)
DECLARE_GL_FUNC(void, glTexImage2D, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
DECLARE_GL_FUNC(void, glTexParameteri, GLenum target, GLenum pname, GLint param)
DECLARE_GL_FUNC(void, glActiveTexture, GLenum texture)
DECLARE_GL_FUNC(void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height)
DECLARE_GL_FUNC(void, glClear, GLbitfield mask)
DECLARE_GL_FUNC(void, glDisable, GLenum cap)
DECLARE_GL_FUNC(void, glEnable, GLenum cap)
DECLARE_GL_FUNC(void, glDrawArrays, GLenum mode, GLint first, GLsizei count)
DECLARE_GL_FUNC(void, glBindVertexArray, GLuint array)
DECLARE_GL_FUNC(void, glGenVertexArrays, GLsizei n, GLuint* arrays)
DECLARE_GL_FUNC(void, glDeleteVertexArrays, GLsizei n, const GLuint* arrays)

static bool s_pp_gl_loaded = false;

static bool load_postprocess_gl_functions() {
    if (s_pp_gl_loaded) return true;

#define LOAD_GL(name) pfn_##name = (PFN_##name)wglGetProcAddress(#name);

    LOAD_GL(glGenFramebuffers)
    LOAD_GL(glDeleteFramebuffers)
    LOAD_GL(glBindFramebuffer)
    LOAD_GL(glFramebufferTexture2D)
    LOAD_GL(glCheckFramebufferStatus)
    LOAD_GL(glGenTextures)
    LOAD_GL(glDeleteTextures)
    LOAD_GL(glBindTexture)
    LOAD_GL(glTexImage2D)
    LOAD_GL(glTexParameteri)
    LOAD_GL(glActiveTexture)
    LOAD_GL(glViewport)
    LOAD_GL(glClear)
    LOAD_GL(glDisable)
    LOAD_GL(glEnable)
    LOAD_GL(glDrawArrays)
    LOAD_GL(glBindVertexArray)
    LOAD_GL(glGenVertexArrays)
    LOAD_GL(glDeleteVertexArrays)

#undef LOAD_GL

    s_pp_gl_loaded = true;
    return true;
}

#define GL_CALL(name, ...) (pfn_##name ? pfn_##name(__VA_ARGS__) : (void)0)

#else
#include <GL/gl.h>
#include <GL/glext.h>
#define GL_CALL(name, ...) name(__VA_ARGS__)
static bool load_postprocess_gl_functions() { return true; }
#endif

namespace void_render {

// =============================================================================
// PostProcessConfig
// =============================================================================

struct PostProcessConfig {
    // Bloom
    bool bloom_enabled = true;
    float bloom_threshold = 1.0f;
    float bloom_intensity = 0.5f;
    float bloom_radius = 5.0f;
    int bloom_mip_count = 5;

    // SSAO
    bool ssao_enabled = true;
    float ssao_radius = 0.5f;
    float ssao_bias = 0.025f;
    float ssao_intensity = 1.0f;
    int ssao_kernel_size = 64;

    // Tonemapping
    enum class TonemapOperator {
        None,
        Reinhard,
        ReinhardExtended,
        ACES,
        Uncharted2,
        AgX
    };
    TonemapOperator tonemap_operator = TonemapOperator::ACES;
    float exposure = 1.0f;
    float gamma = 2.2f;

    // FXAA
    bool fxaa_enabled = true;
    float fxaa_subpixel = 0.75f;
    float fxaa_edge_threshold = 0.166f;
    float fxaa_edge_threshold_min = 0.0833f;

    // Vignette
    bool vignette_enabled = false;
    float vignette_intensity = 0.5f;
    float vignette_smoothness = 0.5f;

    // Film grain
    bool grain_enabled = false;
    float grain_intensity = 0.1f;

    // Chromatic aberration
    bool chromatic_aberration_enabled = false;
    float chromatic_aberration_intensity = 0.01f;
};

// =============================================================================
// Framebuffer Helper
// =============================================================================

struct Framebuffer {
    GLuint fbo = 0;
    GLuint color_texture = 0;
    GLuint depth_texture = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool create(std::uint32_t w, std::uint32_t h, bool with_depth = false) {
        load_postprocess_gl_functions();

        width = w;
        height = h;

        // Create color texture
        glGenTextures(1, &color_texture);
        glBindTexture(GL_TEXTURE_2D, color_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, 0x2802, GL_CLAMP_TO_EDGE);  // WRAP_S
        glTexParameteri(GL_TEXTURE_2D, 0x2803, GL_CLAMP_TO_EDGE);  // WRAP_T

        // Create framebuffer
        if (pfn_glGenFramebuffers) {
            pfn_glGenFramebuffers(1, &fbo);
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            pfn_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_TEXTURE_2D, color_texture, 0);

            if (with_depth) {
                glGenTextures(1, &depth_texture);
                glBindTexture(GL_TEXTURE_2D, depth_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, 0x81A6, w, h, 0, 0x1902, 0x1405, nullptr);
                pfn_glFramebufferTexture2D(GL_FRAMEBUFFER, 0x8D00,
                                            GL_TEXTURE_2D, depth_texture, 0);
            }

            GLenum status = pfn_glCheckFramebufferStatus(GL_FRAMEBUFFER);
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);

            if (status != GL_FRAMEBUFFER_COMPLETE) {
                spdlog::error("Framebuffer incomplete: {}", status);
                return false;
            }
        }

        return true;
    }

    void destroy() {
        if (fbo && pfn_glDeleteFramebuffers) {
            pfn_glDeleteFramebuffers(1, &fbo);
        }
        if (color_texture) {
            glDeleteTextures(1, &color_texture);
        }
        if (depth_texture) {
            glDeleteTextures(1, &depth_texture);
        }
        fbo = 0;
        color_texture = 0;
        depth_texture = 0;
    }

    void bind() const {
        if (pfn_glBindFramebuffer) {
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        }
        glViewport(0, 0, width, height);
    }

    static void unbind() {
        if (pfn_glBindFramebuffer) {
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
};

// =============================================================================
// PostProcessPipeline
// =============================================================================

class PostProcessPipeline {
public:
    PostProcessPipeline() = default;
    ~PostProcessPipeline() { shutdown(); }

    bool initialize(std::uint32_t width, std::uint32_t height);
    void shutdown();
    void resize(std::uint32_t width, std::uint32_t height);

    void process(GLuint input_texture, GLuint output_fbo);

    PostProcessConfig& config() { return m_config; }
    const PostProcessConfig& config() const { return m_config; }

private:
    PostProcessConfig m_config;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;

    // Framebuffers
    std::vector<Framebuffer> m_bloom_mips;
    Framebuffer m_ssao_buffer;
    Framebuffer m_ssao_blur;
    Framebuffer m_temp_buffer;

    // Shaders
    std::unique_ptr<ShaderProgram> m_bloom_downsample_shader;
    std::unique_ptr<ShaderProgram> m_bloom_upsample_shader;
    std::unique_ptr<ShaderProgram> m_ssao_shader;
    std::unique_ptr<ShaderProgram> m_ssao_blur_shader;
    std::unique_ptr<ShaderProgram> m_tonemap_shader;
    std::unique_ptr<ShaderProgram> m_fxaa_shader;
    std::unique_ptr<ShaderProgram> m_composite_shader;

    // SSAO kernel and noise
    std::vector<glm::vec3> m_ssao_kernel;
    GLuint m_ssao_noise_texture = 0;

    // Fullscreen quad
    GLuint m_quad_vao = 0;

    bool create_shaders();
    void create_ssao_data();
    void create_fullscreen_quad();

    void apply_bloom(GLuint input);
    void apply_ssao(GLuint depth_texture, GLuint normal_texture);
    void apply_tonemapping(GLuint input, GLuint output_fbo);
    void apply_fxaa(GLuint input, GLuint output_fbo);

    void render_fullscreen_quad();
};

bool PostProcessPipeline::initialize(std::uint32_t width, std::uint32_t height) {
    if (!load_postprocess_gl_functions()) return false;

    m_width = width;
    m_height = height;

    // Create bloom mip chain
    m_bloom_mips.resize(m_config.bloom_mip_count);
    std::uint32_t mip_w = width / 2;
    std::uint32_t mip_h = height / 2;
    for (int i = 0; i < m_config.bloom_mip_count; ++i) {
        m_bloom_mips[i].create(mip_w, mip_h);
        mip_w = std::max(1u, mip_w / 2);
        mip_h = std::max(1u, mip_h / 2);
    }

    // Create SSAO buffers
    m_ssao_buffer.create(width, height);
    m_ssao_blur.create(width, height);

    // Create temp buffer
    m_temp_buffer.create(width, height);

    // Create shaders
    if (!create_shaders()) {
        spdlog::error("Failed to create post-process shaders");
        return false;
    }

    // Create SSAO data
    create_ssao_data();

    // Create fullscreen quad
    create_fullscreen_quad();

    spdlog::info("PostProcessPipeline initialized: {}x{}", width, height);
    return true;
}

void PostProcessPipeline::shutdown() {
    for (auto& fb : m_bloom_mips) {
        fb.destroy();
    }
    m_bloom_mips.clear();

    m_ssao_buffer.destroy();
    m_ssao_blur.destroy();
    m_temp_buffer.destroy();

    if (m_ssao_noise_texture) {
        glDeleteTextures(1, &m_ssao_noise_texture);
        m_ssao_noise_texture = 0;
    }

    if (m_quad_vao && pfn_glDeleteVertexArrays) {
        pfn_glDeleteVertexArrays(1, &m_quad_vao);
        m_quad_vao = 0;
    }

    m_bloom_downsample_shader.reset();
    m_bloom_upsample_shader.reset();
    m_ssao_shader.reset();
    m_ssao_blur_shader.reset();
    m_tonemap_shader.reset();
    m_fxaa_shader.reset();
    m_composite_shader.reset();
}

void PostProcessPipeline::resize(std::uint32_t width, std::uint32_t height) {
    if (m_width == width && m_height == height) return;

    shutdown();
    initialize(width, height);
}

bool PostProcessPipeline::create_shaders() {
    // Bloom downsample (13-tap filter)
    static const char* BLOOM_DOWNSAMPLE_FRAG = R"(
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D srcTexture;
uniform vec2 srcResolution;
uniform float threshold;

void main() {
    vec2 texelSize = 1.0 / srcResolution;

    // 13-tap downsampling (optimized for blur)
    vec3 a = texture(srcTexture, TexCoords + texelSize * vec2(-2, 2)).rgb;
    vec3 b = texture(srcTexture, TexCoords + texelSize * vec2(0, 2)).rgb;
    vec3 c = texture(srcTexture, TexCoords + texelSize * vec2(2, 2)).rgb;
    vec3 d = texture(srcTexture, TexCoords + texelSize * vec2(-2, 0)).rgb;
    vec3 e = texture(srcTexture, TexCoords).rgb;
    vec3 f = texture(srcTexture, TexCoords + texelSize * vec2(2, 0)).rgb;
    vec3 g = texture(srcTexture, TexCoords + texelSize * vec2(-2, -2)).rgb;
    vec3 h = texture(srcTexture, TexCoords + texelSize * vec2(0, -2)).rgb;
    vec3 i = texture(srcTexture, TexCoords + texelSize * vec2(2, -2)).rgb;
    vec3 j = texture(srcTexture, TexCoords + texelSize * vec2(-1, 1)).rgb;
    vec3 k = texture(srcTexture, TexCoords + texelSize * vec2(1, 1)).rgb;
    vec3 l = texture(srcTexture, TexCoords + texelSize * vec2(-1, -1)).rgb;
    vec3 m = texture(srcTexture, TexCoords + texelSize * vec2(1, -1)).rgb;

    vec3 color = e * 0.125;
    color += (a + c + g + i) * 0.03125;
    color += (b + d + f + h) * 0.0625;
    color += (j + k + l + m) * 0.125;

    // Apply threshold for first pass only
    if (threshold > 0.0) {
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color *= smoothstep(threshold - 0.1, threshold + 0.1, brightness);
    }

    FragColor = vec4(color, 1.0);
}
)";

    // Bloom upsample (tent filter)
    static const char* BLOOM_UPSAMPLE_FRAG = R"(
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D srcTexture;
uniform vec2 srcResolution;
uniform float filterRadius;

void main() {
    vec2 texelSize = 1.0 / srcResolution;
    float x = filterRadius * texelSize.x;
    float y = filterRadius * texelSize.y;

    // 9-tap tent filter
    vec3 a = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 b = texture(srcTexture, vec2(TexCoords.x,     TexCoords.y + y)).rgb;
    vec3 c = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
    vec3 d = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y)).rgb;
    vec3 e = texture(srcTexture, vec2(TexCoords.x,     TexCoords.y)).rgb;
    vec3 f = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y)).rgb;
    vec3 g = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 h = texture(srcTexture, vec2(TexCoords.x,     TexCoords.y - y)).rgb;
    vec3 i = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    vec3 color = e * 4.0;
    color += (b + d + f + h) * 2.0;
    color += (a + c + g + i);
    color *= 1.0 / 16.0;

    FragColor = vec4(color, 1.0);
}
)";

    // Tonemapping
    static const char* TONEMAP_FRAG = R"(
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D hdrBuffer;
uniform sampler2D bloomBuffer;
uniform float exposure;
uniform float gamma;
uniform float bloomIntensity;
uniform int tonemapOperator;

// ACES tonemapping
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard tonemapping
vec3 Reinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

// Uncharted 2 tonemapping
vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

void main() {
    vec3 hdr = texture(hdrBuffer, TexCoords).rgb;
    vec3 bloom = texture(bloomBuffer, TexCoords).rgb;

    // Add bloom
    hdr += bloom * bloomIntensity;

    // Apply exposure
    hdr *= exposure;

    // Tonemapping
    vec3 mapped;
    if (tonemapOperator == 0) {
        mapped = hdr;  // None
    } else if (tonemapOperator == 1) {
        mapped = Reinhard(hdr);
    } else if (tonemapOperator == 2) {
        mapped = ACESFilm(hdr);
    } else {
        vec3 W = vec3(11.2);
        mapped = Uncharted2Tonemap(hdr * 2.0) / Uncharted2Tonemap(W);
    }

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));

    FragColor = vec4(mapped, 1.0);
}
)";

    // FXAA
    static const char* FXAA_FRAG = R"(
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform vec2 inverseScreenSize;
uniform float subpixelQuality;
uniform float edgeThreshold;
uniform float edgeThresholdMin;

#define FXAA_REDUCE_MIN (1.0 / 128.0)
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_SPAN_MAX 8.0

void main() {
    vec2 texCoord = TexCoords;

    vec3 rgbNW = texture(screenTexture, texCoord + vec2(-1.0, -1.0) * inverseScreenSize).rgb;
    vec3 rgbNE = texture(screenTexture, texCoord + vec2(1.0, -1.0) * inverseScreenSize).rgb;
    vec3 rgbSW = texture(screenTexture, texCoord + vec2(-1.0, 1.0) * inverseScreenSize).rgb;
    vec3 rgbSE = texture(screenTexture, texCoord + vec2(1.0, 1.0) * inverseScreenSize).rgb;
    vec3 rgbM = texture(screenTexture, texCoord).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir * rcpDirMin)) * inverseScreenSize;

    vec3 rgbA = 0.5 * (
        texture(screenTexture, texCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(screenTexture, texCoord + dir * (2.0 / 3.0 - 0.5)).rgb
    );
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(screenTexture, texCoord + dir * -0.5).rgb +
        texture(screenTexture, texCoord + dir * 0.5).rgb
    );

    float lumaB = dot(rgbB, luma);

    if (lumaB < lumaMin || lumaB > lumaMax) {
        FragColor = vec4(rgbA, 1.0);
    } else {
        FragColor = vec4(rgbB, 1.0);
    }
}
)";

    // Common vertex shader for fullscreen quad
    static const char* FULLSCREEN_VERT = R"(
#version 330 core

out vec2 TexCoords;

void main() {
    float x = float((gl_VertexID & 1) << 2);
    float y = float((gl_VertexID & 2) << 1);
    TexCoords = vec2(x * 0.5, y * 0.5);
    gl_Position = vec4(x - 1.0, y - 1.0, 0.0, 1.0);
}
)";

    // Create shaders
    m_bloom_downsample_shader = std::make_unique<ShaderProgram>();
    if (!m_bloom_downsample_shader->load_from_source(FULLSCREEN_VERT, BLOOM_DOWNSAMPLE_FRAG)) {
        return false;
    }

    m_bloom_upsample_shader = std::make_unique<ShaderProgram>();
    if (!m_bloom_upsample_shader->load_from_source(FULLSCREEN_VERT, BLOOM_UPSAMPLE_FRAG)) {
        return false;
    }

    m_tonemap_shader = std::make_unique<ShaderProgram>();
    if (!m_tonemap_shader->load_from_source(FULLSCREEN_VERT, TONEMAP_FRAG)) {
        return false;
    }

    m_fxaa_shader = std::make_unique<ShaderProgram>();
    if (!m_fxaa_shader->load_from_source(FULLSCREEN_VERT, FXAA_FRAG)) {
        return false;
    }

    return true;
}

void PostProcessPipeline::create_ssao_data() {
    // Generate kernel samples (hemisphere)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    m_ssao_kernel.resize(m_config.ssao_kernel_size);
    for (int i = 0; i < m_config.ssao_kernel_size; ++i) {
        glm::vec3 sample(
            dist(gen) * 2.0f - 1.0f,
            dist(gen) * 2.0f - 1.0f,
            dist(gen)  // Hemisphere, positive Z
        );
        sample = glm::normalize(sample);
        sample *= dist(gen);

        // Scale samples to cluster near origin
        float scale = static_cast<float>(i) / m_config.ssao_kernel_size;
        scale = 0.1f + scale * scale * 0.9f;  // Lerp
        sample *= scale;

        m_ssao_kernel[i] = sample;
    }

    // Generate noise texture (4x4)
    std::vector<glm::vec3> noise(16);
    for (auto& n : noise) {
        n = glm::vec3(
            dist(gen) * 2.0f - 1.0f,
            dist(gen) * 2.0f - 1.0f,
            0.0f
        );
    }

    glGenTextures(1, &m_ssao_noise_texture);
    glBindTexture(GL_TEXTURE_2D, m_ssao_noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 0x8815, 4, 4, 0, 0x1907, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, 0x2801, 0x2600);  // MIN_FILTER, NEAREST
    glTexParameteri(GL_TEXTURE_2D, 0x2800, 0x2600);  // MAG_FILTER, NEAREST
    glTexParameteri(GL_TEXTURE_2D, 0x2802, 0x2901);  // WRAP_S, REPEAT
    glTexParameteri(GL_TEXTURE_2D, 0x2803, 0x2901);  // WRAP_T, REPEAT
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PostProcessPipeline::create_fullscreen_quad() {
    // Create a dummy VAO for attribute-less rendering
    if (pfn_glGenVertexArrays) {
        pfn_glGenVertexArrays(1, &m_quad_vao);
    }
}

void PostProcessPipeline::render_fullscreen_quad() {
    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(m_quad_vao);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (pfn_glBindVertexArray) {
        pfn_glBindVertexArray(0);
    }
}

void PostProcessPipeline::process(GLuint input_texture, GLuint output_fbo) {
    GLuint current_input = input_texture;

    // Apply bloom
    if (m_config.bloom_enabled && !m_bloom_mips.empty()) {
        apply_bloom(current_input);
    }

    // Apply tonemapping (combines bloom)
    if (m_tonemap_shader) {
        m_temp_buffer.bind();
        glClear(0x00004000);  // GL_COLOR_BUFFER_BIT

        m_tonemap_shader->use();
        m_tonemap_shader->set_int("hdrBuffer", 0);
        m_tonemap_shader->set_int("bloomBuffer", 1);
        m_tonemap_shader->set_float("exposure", m_config.exposure);
        m_tonemap_shader->set_float("gamma", m_config.gamma);
        m_tonemap_shader->set_float("bloomIntensity",
                                     m_config.bloom_enabled ? m_config.bloom_intensity : 0.0f);
        m_tonemap_shader->set_int("tonemapOperator", static_cast<int>(m_config.tonemap_operator));

        if (pfn_glActiveTexture) {
            pfn_glActiveTexture(0x84C0);  // GL_TEXTURE0
        }
        glBindTexture(GL_TEXTURE_2D, current_input);

        if (pfn_glActiveTexture) {
            pfn_glActiveTexture(0x84C1);  // GL_TEXTURE1
        }
        if (!m_bloom_mips.empty()) {
            glBindTexture(GL_TEXTURE_2D, m_bloom_mips[0].color_texture);
        }

        render_fullscreen_quad();
        current_input = m_temp_buffer.color_texture;
    }

    // Apply FXAA
    if (m_config.fxaa_enabled && m_fxaa_shader) {
        if (pfn_glBindFramebuffer) {
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
        }
        glViewport(0, 0, m_width, m_height);

        m_fxaa_shader->use();
        m_fxaa_shader->set_int("screenTexture", 0);
        m_fxaa_shader->set_vec2("inverseScreenSize",
                                 glm::vec2(1.0f / m_width, 1.0f / m_height));
        m_fxaa_shader->set_float("subpixelQuality", m_config.fxaa_subpixel);
        m_fxaa_shader->set_float("edgeThreshold", m_config.fxaa_edge_threshold);
        m_fxaa_shader->set_float("edgeThresholdMin", m_config.fxaa_edge_threshold_min);

        if (pfn_glActiveTexture) {
            pfn_glActiveTexture(0x84C0);
        }
        glBindTexture(GL_TEXTURE_2D, current_input);

        render_fullscreen_quad();
    } else {
        // Just copy to output
        if (pfn_glBindFramebuffer) {
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
        }
        // Would need a blit/copy shader here
    }

    Framebuffer::unbind();
}

void PostProcessPipeline::apply_bloom(GLuint input) {
    if (m_bloom_mips.empty()) return;

    // Downsample chain
    GLuint src = input;
    std::uint32_t src_w = m_width;
    std::uint32_t src_h = m_height;

    m_bloom_downsample_shader->use();

    for (std::size_t i = 0; i < m_bloom_mips.size(); ++i) {
        auto& mip = m_bloom_mips[i];
        mip.bind();
        glClear(0x00004000);

        m_bloom_downsample_shader->set_int("srcTexture", 0);
        m_bloom_downsample_shader->set_vec2("srcResolution", glm::vec2(src_w, src_h));
        m_bloom_downsample_shader->set_float("threshold", i == 0 ? m_config.bloom_threshold : 0.0f);

        if (pfn_glActiveTexture) {
            pfn_glActiveTexture(0x84C0);
        }
        glBindTexture(GL_TEXTURE_2D, src);

        render_fullscreen_quad();

        src = mip.color_texture;
        src_w = mip.width;
        src_h = mip.height;
    }

    // Upsample chain
    m_bloom_upsample_shader->use();
    glEnable(0x0BE2);  // GL_BLEND
    glBlendFunc(1, 1);  // GL_ONE, GL_ONE (additive)

    for (int i = static_cast<int>(m_bloom_mips.size()) - 2; i >= 0; --i) {
        auto& mip = m_bloom_mips[i];
        mip.bind();

        m_bloom_upsample_shader->set_int("srcTexture", 0);
        m_bloom_upsample_shader->set_vec2("srcResolution",
                                           glm::vec2(m_bloom_mips[i + 1].width,
                                                     m_bloom_mips[i + 1].height));
        m_bloom_upsample_shader->set_float("filterRadius", m_config.bloom_radius);

        if (pfn_glActiveTexture) {
            pfn_glActiveTexture(0x84C0);
        }
        glBindTexture(GL_TEXTURE_2D, m_bloom_mips[i + 1].color_texture);

        render_fullscreen_quad();
    }

    glDisable(0x0BE2);  // GL_BLEND
    Framebuffer::unbind();
}

void PostProcessPipeline::apply_ssao(GLuint depth_texture, GLuint normal_texture) {
    // SSAO implementation would go here
    // Requires depth and normal textures from GBuffer
}

void PostProcessPipeline::apply_tonemapping(GLuint input, GLuint output_fbo) {
    // Already handled in process()
}

void PostProcessPipeline::apply_fxaa(GLuint input, GLuint output_fbo) {
    // Already handled in process()
}

// =============================================================================
// Global instance for integration
// =============================================================================

static std::unique_ptr<PostProcessPipeline> g_post_process;

bool init_post_processing(std::uint32_t width, std::uint32_t height) {
    g_post_process = std::make_unique<PostProcessPipeline>();
    return g_post_process->initialize(width, height);
}

void shutdown_post_processing() {
    g_post_process.reset();
}

void resize_post_processing(std::uint32_t width, std::uint32_t height) {
    if (g_post_process) {
        g_post_process->resize(width, height);
    }
}

void apply_post_processing(GLuint input_texture, GLuint output_fbo) {
    if (g_post_process) {
        g_post_process->process(input_texture, output_fbo);
    }
}

} // namespace void_render
