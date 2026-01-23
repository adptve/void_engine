/// @file texture.cpp
/// @brief Texture loading and management implementation

#include <void_engine/render/texture.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

// STB Image - single-file public domain image loading
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>
#include <stb_image_write.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// OpenGL extension defines
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_NEAREST 0x2600
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_MIRRORED_REPEAT 0x8370
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_RGBA8 0x8058
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_R16F 0x822D
#define GL_RG16F 0x822F
#define GL_R32F 0x822E
#define GL_RG32F 0x8230
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_RENDERBUFFER 0x8D41
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_LEQUAL 0x0203
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

// OpenGL function pointers
#define DECLARE_GL_FUNC(ret, name, ...) \
    typedef ret (APIENTRY *PFN##name##PROC)(__VA_ARGS__); \
    static PFN##name##PROC gl##name = nullptr;

DECLARE_GL_FUNC(void, GenTextures, GLsizei n, GLuint* textures)
DECLARE_GL_FUNC(void, DeleteTextures, GLsizei n, const GLuint* textures)
DECLARE_GL_FUNC(void, BindTexture, GLenum target, GLuint texture)
DECLARE_GL_FUNC(void, TexImage2D, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
DECLARE_GL_FUNC(void, TexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
DECLARE_GL_FUNC(void, TexParameteri, GLenum target, GLenum pname, GLint param)
DECLARE_GL_FUNC(void, TexParameterf, GLenum target, GLenum pname, GLfloat param)
DECLARE_GL_FUNC(void, GenerateMipmap, GLenum target)
DECLARE_GL_FUNC(void, ActiveTexture, GLenum texture)
DECLARE_GL_FUNC(void, GenSamplers, GLsizei count, GLuint* samplers)
DECLARE_GL_FUNC(void, DeleteSamplers, GLsizei count, const GLuint* samplers)
DECLARE_GL_FUNC(void, BindSampler, GLuint unit, GLuint sampler)
DECLARE_GL_FUNC(void, SamplerParameteri, GLuint sampler, GLenum pname, GLint param)
DECLARE_GL_FUNC(void, SamplerParameterf, GLuint sampler, GLenum pname, GLfloat param)
DECLARE_GL_FUNC(void, TexImage2DMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
DECLARE_GL_FUNC(void, GenFramebuffers, GLsizei n, GLuint* framebuffers)
DECLARE_GL_FUNC(void, DeleteFramebuffers, GLsizei n, const GLuint* framebuffers)
DECLARE_GL_FUNC(void, BindFramebuffer, GLenum target, GLuint framebuffer)
DECLARE_GL_FUNC(void, FramebufferTexture2D, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
DECLARE_GL_FUNC(GLenum, CheckFramebufferStatus, GLenum target)
DECLARE_GL_FUNC(void, GenRenderbuffers, GLsizei n, GLuint* renderbuffers)
DECLARE_GL_FUNC(void, DeleteRenderbuffers, GLsizei n, const GLuint* renderbuffers)
DECLARE_GL_FUNC(void, BindRenderbuffer, GLenum target, GLuint renderbuffer)
DECLARE_GL_FUNC(void, RenderbufferStorage, GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
DECLARE_GL_FUNC(void, RenderbufferStorageMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
DECLARE_GL_FUNC(void, FramebufferRenderbuffer, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)

static bool s_texture_gl_loaded = false;

static bool load_texture_gl_functions() {
    if (s_texture_gl_loaded) return true;

#define LOAD_GL(name) \
    gl##name = (PFN##name##PROC)wglGetProcAddress("gl" #name); \
    if (!gl##name) { spdlog::warn("Failed to load gl" #name); }

    LOAD_GL(GenTextures);
    LOAD_GL(DeleteTextures);
    LOAD_GL(BindTexture);
    LOAD_GL(TexImage2D);
    LOAD_GL(TexSubImage2D);
    LOAD_GL(TexParameteri);
    LOAD_GL(TexParameterf);
    LOAD_GL(GenerateMipmap);
    LOAD_GL(ActiveTexture);
    LOAD_GL(GenSamplers);
    LOAD_GL(DeleteSamplers);
    LOAD_GL(BindSampler);
    LOAD_GL(SamplerParameteri);
    LOAD_GL(SamplerParameterf);
    LOAD_GL(TexImage2DMultisample);
    LOAD_GL(GenFramebuffers);
    LOAD_GL(DeleteFramebuffers);
    LOAD_GL(BindFramebuffer);
    LOAD_GL(FramebufferTexture2D);
    LOAD_GL(CheckFramebufferStatus);
    LOAD_GL(GenRenderbuffers);
    LOAD_GL(DeleteRenderbuffers);
    LOAD_GL(BindRenderbuffer);
    LOAD_GL(RenderbufferStorage);
    LOAD_GL(RenderbufferStorageMultisample);
    LOAD_GL(FramebufferRenderbuffer);

#undef LOAD_GL

    s_texture_gl_loaded = true;
    return true;
}

// Fallback to standard GL functions if extensions not loaded
#define GL_CALL(name, ...) \
    (gl##name ? gl##name(__VA_ARGS__) : (void)0)

#else
// Linux/macOS - use standard GL
#include <GL/gl.h>
#include <GL/glext.h>
#define GL_CALL(name, ...) gl##name(__VA_ARGS__)
static bool load_texture_gl_functions() { return true; }
#endif

namespace void_render {

// =============================================================================
// Helper Functions
// =============================================================================

static GLenum format_to_gl_internal(TextureFormat format, bool srgb) {
    switch (format) {
        case TextureFormat::R8Unorm: return GL_R8;
        case TextureFormat::Rg8Unorm: return GL_RG8;
        case TextureFormat::Rgba8Unorm: return srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        case TextureFormat::Rgba8UnormSrgb: return GL_SRGB8_ALPHA8;
        case TextureFormat::R16Float: return GL_R16F;
        case TextureFormat::Rg16Float: return GL_RG16F;
        case TextureFormat::Rgba16Float: return GL_RGBA16F;
        case TextureFormat::R32Float: return GL_R32F;
        case TextureFormat::Rg32Float: return GL_RG32F;
        case TextureFormat::Rgba32Float: return GL_RGBA32F;
        case TextureFormat::Depth16Unorm: return GL_DEPTH_COMPONENT16;
        case TextureFormat::Depth24Plus: return GL_DEPTH_COMPONENT24;
        case TextureFormat::Depth32Float: return GL_DEPTH_COMPONENT32F;
        case TextureFormat::Depth24PlusStencil8: return GL_DEPTH24_STENCIL8;
        case TextureFormat::Depth32FloatStencil8: return GL_DEPTH32F_STENCIL8;
        default: return GL_RGBA8;
    }
}

static GLenum format_to_gl_format(TextureFormat format) {
    if (is_depth_format(format)) {
        if (has_stencil(format)) return GL_DEPTH_STENCIL;
        return GL_DEPTH_COMPONENT;
    }
    switch (format) {
        case TextureFormat::R8Unorm:
        case TextureFormat::R16Float:
        case TextureFormat::R32Float:
            return GL_RED;
        case TextureFormat::Rg8Unorm:
        case TextureFormat::Rg16Float:
        case TextureFormat::Rg32Float:
            return GL_RG;
        default:
            return GL_RGBA;
    }
}

static GLenum format_to_gl_type(TextureFormat format) {
    switch (format) {
        case TextureFormat::R16Float:
        case TextureFormat::Rg16Float:
        case TextureFormat::Rgba16Float:
        case TextureFormat::R32Float:
        case TextureFormat::Rg32Float:
        case TextureFormat::Rgba32Float:
        case TextureFormat::Depth32Float:
        case TextureFormat::Depth32FloatStencil8:
            return GL_FLOAT;
        case TextureFormat::Depth24PlusStencil8:
            return GL_UNSIGNED_INT_24_8;
        default:
            return GL_UNSIGNED_BYTE;
    }
}

static GLenum filter_to_gl(FilterMode filter, bool mipmaps) {
    if (mipmaps) {
        return filter == FilterMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    }
    return filter == FilterMode::Linear ? GL_LINEAR : GL_NEAREST;
}

static GLenum address_to_gl(AddressMode mode) {
    switch (mode) {
        case AddressMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case AddressMode::Repeat: return GL_REPEAT;
        case AddressMode::MirrorRepeat: return GL_MIRRORED_REPEAT;
        case AddressMode::ClampToBorder: return GL_CLAMP_TO_BORDER;
        default: return GL_CLAMP_TO_EDGE;
    }
}

static std::uint32_t calculate_mip_levels(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

// =============================================================================
// TextureData Implementation
// =============================================================================

TextureData TextureData::generate_mipmaps() const {
    if (!is_valid()) return {};

    TextureData result;
    result.width = width;
    result.height = height;
    result.channels = channels;
    result.format = format;
    result.is_srgb = is_srgb;
    result.mip_levels = calculate_mip_levels(width, height);

    // Calculate total size for all mip levels
    std::size_t total_size = 0;
    std::uint32_t w = width, h = height;
    for (std::uint32_t i = 0; i < result.mip_levels; ++i) {
        total_size += w * h * channels;
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }

    result.pixels.resize(total_size);

    // Copy base level
    std::memcpy(result.pixels.data(), pixels.data(), width * height * channels);

    // Generate subsequent levels using box filter
    w = width;
    h = height;
    std::size_t src_offset = 0;
    std::size_t dst_offset = w * h * channels;

    for (std::uint32_t level = 1; level < result.mip_levels; ++level) {
        std::uint32_t new_w = std::max(1u, w / 2);
        std::uint32_t new_h = std::max(1u, h / 2);

        for (std::uint32_t y = 0; y < new_h; ++y) {
            for (std::uint32_t x = 0; x < new_w; ++x) {
                for (std::uint32_t c = 0; c < channels; ++c) {
                    // Average 2x2 block
                    std::uint32_t sum = 0;
                    std::uint32_t count = 0;
                    for (std::uint32_t dy = 0; dy < 2 && y * 2 + dy < h; ++dy) {
                        for (std::uint32_t dx = 0; dx < 2 && x * 2 + dx < w; ++dx) {
                            std::size_t idx = src_offset + ((y * 2 + dy) * w + (x * 2 + dx)) * channels + c;
                            sum += result.pixels[idx];
                            count++;
                        }
                    }
                    result.pixels[dst_offset + (y * new_w + x) * channels + c] =
                        static_cast<std::uint8_t>(sum / count);
                }
            }
        }

        src_offset = dst_offset;
        dst_offset += new_w * new_h * channels;
        w = new_w;
        h = new_h;
    }

    return result;
}

TextureData TextureData::from_rgba(const std::uint8_t* data, std::uint32_t w, std::uint32_t h) {
    TextureData result;
    result.width = w;
    result.height = h;
    result.channels = 4;
    result.format = TextureFormat::Rgba8Unorm;
    result.pixels.resize(w * h * 4);
    std::memcpy(result.pixels.data(), data, w * h * 4);
    return result;
}

TextureData TextureData::solid_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    TextureData result;
    result.width = 1;
    result.height = 1;
    result.channels = 4;
    result.format = TextureFormat::Rgba8Unorm;
    result.pixels = {r, g, b, a};
    return result;
}

TextureData TextureData::checkerboard(std::uint32_t size, std::uint32_t cell_size) {
    TextureData result;
    result.width = size;
    result.height = size;
    result.channels = 4;
    result.format = TextureFormat::Rgba8Unorm;
    result.pixels.resize(size * size * 4);

    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            bool is_white = ((x / cell_size) + (y / cell_size)) % 2 == 0;
            std::uint8_t color = is_white ? 255 : 128;
            std::size_t idx = (y * size + x) * 4;
            result.pixels[idx + 0] = color;
            result.pixels[idx + 1] = color;
            result.pixels[idx + 2] = color;
            result.pixels[idx + 3] = 255;
        }
    }

    return result;
}

TextureData TextureData::default_normal() {
    // Flat normal pointing up (0, 0, 1) encoded as (128, 128, 255)
    TextureData result;
    result.width = 1;
    result.height = 1;
    result.channels = 4;
    result.format = TextureFormat::Rgba8Unorm;
    result.is_srgb = false;  // Normal maps are linear
    result.pixels = {128, 128, 255, 255};
    return result;
}

TextureData TextureData::default_white() {
    return solid_color(255, 255, 255, 255);
}

TextureData TextureData::default_black() {
    return solid_color(0, 0, 0, 255);
}

// =============================================================================
// HdrTextureData Implementation
// =============================================================================

TextureData HdrTextureData::to_ldr(float exposure) const {
    if (!is_valid()) return {};

    TextureData result;
    result.width = width;
    result.height = height;
    result.channels = 4;
    result.format = TextureFormat::Rgba8Unorm;
    result.pixels.resize(width * height * 4);

    for (std::uint32_t i = 0; i < width * height; ++i) {
        for (std::uint32_t c = 0; c < 3; ++c) {
            // Apply exposure and ACES tonemapping
            float hdr_value = pixels[i * channels + c] * exposure;

            // ACES approximation
            float a = 2.51f;
            float b = 0.03f;
            float cc = 2.43f;
            float d = 0.59f;
            float e = 0.14f;
            float ldr = (hdr_value * (a * hdr_value + b)) / (hdr_value * (cc * hdr_value + d) + e);

            // Gamma correction
            ldr = std::pow(std::clamp(ldr, 0.0f, 1.0f), 1.0f / 2.2f);

            result.pixels[i * 4 + c] = static_cast<std::uint8_t>(ldr * 255.0f);
        }
        result.pixels[i * 4 + 3] = 255;
    }

    return result;
}

// =============================================================================
// CubemapData Implementation
// =============================================================================

CubemapData CubemapData::from_equirectangular(const HdrTextureData& equirect, std::uint32_t face_size) {
    if (!equirect.is_valid()) return {};

    CubemapData result;
    result.is_hdr = true;

    // Direction vectors for each face
    const auto get_direction = [](Face face, float u, float v) -> std::array<float, 3> {
        // Map UV [0,1] to [-1,1]
        float s = u * 2.0f - 1.0f;
        float t = v * 2.0f - 1.0f;

        switch (face) {
            case Face::PositiveX: return { 1.0f, -t, -s};
            case Face::NegativeX: return {-1.0f, -t,  s};
            case Face::PositiveY: return { s,  1.0f,  t};
            case Face::NegativeY: return { s, -1.0f, -t};
            case Face::PositiveZ: return { s, -t,  1.0f};
            case Face::NegativeZ: return {-s, -t, -1.0f};
            default: return {0.0f, 0.0f, 1.0f};
        }
    };

    const float pi = 3.14159265359f;

    for (int f = 0; f < 6; ++f) {
        auto& face_data = result.faces[f];
        face_data.width = face_size;
        face_data.height = face_size;
        face_data.channels = 4;
        face_data.format = TextureFormat::Rgba8Unorm;
        face_data.pixels.resize(face_size * face_size * 4);

        for (std::uint32_t y = 0; y < face_size; ++y) {
            for (std::uint32_t x = 0; x < face_size; ++x) {
                float u = (x + 0.5f) / face_size;
                float v = (y + 0.5f) / face_size;

                auto dir = get_direction(static_cast<Face>(f), u, v);

                // Normalize direction
                float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
                dir[0] /= len; dir[1] /= len; dir[2] /= len;

                // Convert to equirectangular UV
                float theta = std::atan2(dir[0], dir[2]);
                float phi = std::asin(std::clamp(dir[1], -1.0f, 1.0f));

                float eq_u = (theta / pi + 1.0f) * 0.5f;
                float eq_v = (phi / (pi * 0.5f) + 1.0f) * 0.5f;

                // Sample equirectangular map
                std::uint32_t src_x = static_cast<std::uint32_t>(eq_u * equirect.width) % equirect.width;
                std::uint32_t src_y = static_cast<std::uint32_t>((1.0f - eq_v) * equirect.height) % equirect.height;

                auto rgb = equirect.get_pixel(src_x, src_y);

                // Convert HDR to LDR with simple tonemapping
                std::size_t dst_idx = (y * face_size + x) * 4;
                for (int c = 0; c < 3; ++c) {
                    float v_out = rgb[c] / (rgb[c] + 1.0f);  // Reinhard
                    v_out = std::pow(v_out, 1.0f / 2.2f);  // Gamma
                    face_data.pixels[dst_idx + c] = static_cast<std::uint8_t>(std::clamp(v_out, 0.0f, 1.0f) * 255.0f);
                }
                face_data.pixels[dst_idx + 3] = 255;
            }
        }
    }

    return result;
}

// =============================================================================
// Texture Implementation
// =============================================================================

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_mip_levels(other.m_mip_levels)
    , m_format(other.m_format)
    , m_is_hdr(other.m_is_hdr)
{
    other.m_id = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_mip_levels = other.m_mip_levels;
        m_format = other.m_format;
        m_is_hdr = other.m_is_hdr;
        other.m_id = 0;
    }
    return *this;
}

bool Texture::create(const TextureData& data, const TextureLoadOptions& options) {
    if (!data.is_valid()) {
        spdlog::error("Cannot create texture from invalid data");
        return false;
    }

    load_texture_gl_functions();
    destroy();

    glGenTextures(1, &m_id);
    if (!m_id) {
        spdlog::error("Failed to generate texture");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, m_id);

    m_width = data.width;
    m_height = data.height;
    m_format = data.format;
    m_is_hdr = data.is_hdr;

    GLenum internal_format = format_to_gl_internal(m_format, options.srgb && data.is_srgb);
    GLenum gl_format = format_to_gl_format(m_format);
    GLenum gl_type = format_to_gl_type(m_format);

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, m_width, m_height, 0,
                 gl_format, gl_type, data.pixels.data());

    // Generate mipmaps
    if (options.generate_mipmaps) {
        if (glGenerateMipmap) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        m_mip_levels = calculate_mip_levels(m_width, m_height);
    } else {
        m_mip_levels = 1;
    }

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    filter_to_gl(options.filter, options.generate_mipmaps));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    filter_to_gl(options.filter, false));

    // Set wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_to_gl(options.wrap));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_to_gl(options.wrap));

    // Set anisotropy
    if (options.anisotropy > 1 && glTexParameterf) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        static_cast<float>(options.anisotropy));
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::debug("Created texture {}x{} format={}", m_width, m_height,
                  static_cast<int>(m_format));
    return true;
}

bool Texture::create_hdr(const HdrTextureData& data, const TextureLoadOptions& options) {
    if (!data.is_valid()) {
        spdlog::error("Cannot create HDR texture from invalid data");
        return false;
    }

    load_texture_gl_functions();
    destroy();

    glGenTextures(1, &m_id);
    if (!m_id) {
        spdlog::error("Failed to generate HDR texture");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, m_id);

    m_width = data.width;
    m_height = data.height;
    m_format = TextureFormat::Rgba16Float;
    m_is_hdr = true;
    m_mip_levels = 1;

    // Convert to RGBA float
    std::vector<float> rgba_data(m_width * m_height * 4);
    for (std::size_t i = 0; i < m_width * m_height; ++i) {
        rgba_data[i * 4 + 0] = data.pixels[i * data.channels + 0];
        rgba_data[i * 4 + 1] = data.pixels[i * data.channels + 1];
        rgba_data[i * 4 + 2] = data.pixels[i * data.channels + 2];
        rgba_data[i * 4 + 3] = 1.0f;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
                 GL_RGBA, GL_FLOAT, rgba_data.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::debug("Created HDR texture {}x{}", m_width, m_height);
    return true;
}

bool Texture::create_render_target(std::uint32_t width, std::uint32_t height,
                                    TextureFormat format, std::uint32_t samples) {
    load_texture_gl_functions();
    destroy();

    glGenTextures(1, &m_id);
    if (!m_id) return false;

    m_width = width;
    m_height = height;
    m_format = format;
    m_mip_levels = 1;

    GLenum target = samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    glBindTexture(target, m_id);

    GLenum internal_format = format_to_gl_internal(format, false);

    if (samples > 1 && glTexImage2DMultisample) {
        glTexImage2DMultisample(target, samples, internal_format, width, height, GL_TRUE);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
                     format_to_gl_format(format), format_to_gl_type(format), nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(target, 0);
    return true;
}

bool Texture::create_depth(std::uint32_t width, std::uint32_t height,
                           TextureFormat format, std::uint32_t samples) {
    if (!is_depth_format(format)) {
        spdlog::error("Invalid depth format");
        return false;
    }
    return create_render_target(width, height, format, samples);
}

void Texture::destroy() {
    if (m_id) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_width = 0;
    m_height = 0;
    m_mip_levels = 1;
}

void Texture::bind(std::uint32_t unit) const {
    if (glActiveTexture) {
        glActiveTexture(GL_TEXTURE0 + unit);
    }
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::unbind(std::uint32_t unit) {
    if (glActiveTexture) {
        glActiveTexture(GL_TEXTURE0 + unit);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::update(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                     const void* data) {
    if (!m_id || !data) return;

    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                    format_to_gl_format(m_format),
                    format_to_gl_type(m_format), data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::generate_mipmaps() {
    if (!m_id) return;

    glBindTexture(GL_TEXTURE_2D, m_id);
    if (glGenerateMipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
        m_mip_levels = calculate_mip_levels(m_width, m_height);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::size_t Texture::gpu_memory_bytes() const noexcept {
    if (!is_valid()) return 0;

    std::size_t bpp = bytes_per_pixel(m_format);
    if (bpp == 0) bpp = 4;  // Assume 4 for compressed

    std::size_t total = 0;
    std::uint32_t w = m_width, h = m_height;
    for (std::uint32_t i = 0; i < m_mip_levels; ++i) {
        total += w * h * bpp;
        w = std::max(1u, w / 2);
        h = std::max(1u, h / 2);
    }
    return total;
}

// =============================================================================
// Cubemap Implementation
// =============================================================================

Cubemap::~Cubemap() {
    destroy();
}

Cubemap::Cubemap(Cubemap&& other) noexcept
    : m_id(other.m_id)
    , m_face_size(other.m_face_size)
    , m_is_hdr(other.m_is_hdr)
{
    other.m_id = 0;
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept {
    if (this != &other) {
        destroy();
        m_id = other.m_id;
        m_face_size = other.m_face_size;
        m_is_hdr = other.m_is_hdr;
        other.m_id = 0;
    }
    return *this;
}

bool Cubemap::create(const CubemapData& data, bool generate_mipmaps) {
    if (!data.is_valid()) {
        spdlog::error("Cannot create cubemap from invalid data");
        return false;
    }

    load_texture_gl_functions();
    destroy();

    glGenTextures(1, &m_id);
    if (!m_id) {
        spdlog::error("Failed to generate cubemap texture");
        return false;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);

    m_face_size = data.face_size();
    m_is_hdr = data.is_hdr;

    GLenum internal_format = data.is_hdr ? GL_RGBA16F : GL_SRGB8_ALPHA8;
    GLenum gl_type = data.is_hdr ? GL_FLOAT : GL_UNSIGNED_BYTE;

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format,
                     m_face_size, m_face_size, 0, GL_RGBA, gl_type,
                     data.faces[i].pixels.data());
    }

    if (generate_mipmaps && glGenerateMipmap) {
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    spdlog::debug("Created cubemap {}x{}", m_face_size, m_face_size);
    return true;
}

bool Cubemap::create_from_equirectangular(const HdrTextureData& equirect, std::uint32_t face_size) {
    auto cubemap_data = CubemapData::from_equirectangular(equirect, face_size);
    return create(cubemap_data, true);
}

void Cubemap::destroy() {
    if (m_id) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_face_size = 0;
}

void Cubemap::bind(std::uint32_t unit) const {
    if (glActiveTexture) {
        glActiveTexture(GL_TEXTURE0 + unit);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
}

// =============================================================================
// Sampler Implementation
// =============================================================================

Sampler::~Sampler() {
    destroy();
}

Sampler::Sampler(Sampler&& other) noexcept : m_id(other.m_id) {
    other.m_id = 0;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept {
    if (this != &other) {
        destroy();
        m_id = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

bool Sampler::create(const SamplerDesc& desc) {
    load_texture_gl_functions();
    destroy();

    if (!glGenSamplers) {
        spdlog::warn("Sampler objects not supported");
        return false;
    }

    glGenSamplers(1, &m_id);
    if (!m_id) return false;

    glSamplerParameteri(m_id, GL_TEXTURE_MIN_FILTER,
                        filter_to_gl(desc.min_filter, desc.mipmap_filter == FilterMode::Linear));
    glSamplerParameteri(m_id, GL_TEXTURE_MAG_FILTER,
                        filter_to_gl(desc.mag_filter, false));
    glSamplerParameteri(m_id, GL_TEXTURE_WRAP_S, address_to_gl(desc.address_mode_u));
    glSamplerParameteri(m_id, GL_TEXTURE_WRAP_T, address_to_gl(desc.address_mode_v));
    glSamplerParameteri(m_id, GL_TEXTURE_WRAP_R, address_to_gl(desc.address_mode_w));

    if (desc.anisotropy_clamp > 1) {
        glSamplerParameterf(m_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            static_cast<float>(desc.anisotropy_clamp));
    }

    if (desc.compare) {
        glSamplerParameteri(m_id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(m_id, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }

    return true;
}

void Sampler::destroy() {
    if (m_id && glDeleteSamplers) {
        glDeleteSamplers(1, &m_id);
        m_id = 0;
    }
}

void Sampler::bind(std::uint32_t unit) const {
    if (glBindSampler) {
        glBindSampler(unit, m_id);
    }
}

// =============================================================================
// TextureLoader Implementation
// =============================================================================

std::optional<TextureData> TextureLoader::load(const std::filesystem::path& path,
                                                const TextureLoadOptions& options) {
    if (!std::filesystem::exists(path)) {
        spdlog::error("Texture file not found: {}", path.string());
        return std::nullopt;
    }

    return load_stb(path, options);
}

std::optional<HdrTextureData> TextureLoader::load_hdr(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        spdlog::error("HDR file not found: {}", path.string());
        return std::nullopt;
    }

    return load_stb_hdr(path);
}

std::optional<TextureData> TextureLoader::load_stb(const std::filesystem::path& path,
                                                    const TextureLoadOptions& options) {
    stbi_set_flip_vertically_on_load(options.flip_y ? 1 : 0);

    int width, height, channels;
    int desired_channels = options.force_rgba ? 4 : 0;

    std::string path_str = path.string();
    unsigned char* data = stbi_load(path_str.c_str(), &width, &height, &channels, desired_channels);

    if (!data) {
        spdlog::error("Failed to load texture '{}': {}", path_str, stbi_failure_reason());
        return std::nullopt;
    }

    TextureData result;
    result.width = static_cast<std::uint32_t>(width);
    result.height = static_cast<std::uint32_t>(height);
    result.channels = options.force_rgba ? 4 : static_cast<std::uint32_t>(channels);
    result.format = options.srgb ? TextureFormat::Rgba8UnormSrgb : TextureFormat::Rgba8Unorm;
    result.is_srgb = options.srgb;
    result.is_hdr = false;

    std::size_t size = result.width * result.height * result.channels;
    result.pixels.resize(size);
    std::memcpy(result.pixels.data(), data, size);

    stbi_image_free(data);

    spdlog::debug("Loaded texture '{}' {}x{} {} channels",
                  path.filename().string(), width, height, result.channels);

    return result;
}

std::optional<HdrTextureData> TextureLoader::load_stb_hdr(const std::filesystem::path& path) {
    stbi_set_flip_vertically_on_load(1);

    int width, height, channels;
    std::string path_str = path.string();
    float* data = stbi_loadf(path_str.c_str(), &width, &height, &channels, 3);

    if (!data) {
        spdlog::error("Failed to load HDR '{}': {}", path_str, stbi_failure_reason());
        return std::nullopt;
    }

    HdrTextureData result;
    result.width = static_cast<std::uint32_t>(width);
    result.height = static_cast<std::uint32_t>(height);
    result.channels = 3;

    std::size_t size = result.width * result.height * result.channels;
    result.pixels.resize(size);
    std::memcpy(result.pixels.data(), data, size * sizeof(float));

    stbi_image_free(data);

    spdlog::debug("Loaded HDR '{}' {}x{}", path.filename().string(), width, height);

    return result;
}

bool TextureLoader::save(const std::filesystem::path& path, const TextureData& data) {
    if (!data.is_valid()) return false;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::string path_str = path.string();

    int result = 0;
    if (ext == ".png") {
        result = stbi_write_png(path_str.c_str(), data.width, data.height,
                                data.channels, data.pixels.data(), data.width * data.channels);
    } else if (ext == ".jpg" || ext == ".jpeg") {
        result = stbi_write_jpg(path_str.c_str(), data.width, data.height,
                                data.channels, data.pixels.data(), 95);
    } else if (ext == ".bmp") {
        result = stbi_write_bmp(path_str.c_str(), data.width, data.height,
                                data.channels, data.pixels.data());
    } else if (ext == ".tga") {
        result = stbi_write_tga(path_str.c_str(), data.width, data.height,
                                data.channels, data.pixels.data());
    } else {
        spdlog::error("Unsupported save format: {}", ext);
        return false;
    }

    return result != 0;
}

bool TextureLoader::save_hdr(const std::filesystem::path& path, const HdrTextureData& data) {
    if (!data.is_valid()) return false;

    std::string path_str = path.string();
    return stbi_write_hdr(path_str.c_str(), data.width, data.height,
                          data.channels, data.pixels.data()) != 0;
}

bool TextureLoader::is_supported_format(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tga" || ext == ".gif" ||
           ext == ".psd" || ext == ".hdr" || ext == ".pic" || ext == ".pnm";
}

bool TextureLoader::is_hdr_format(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".hdr";
}

// =============================================================================
// TextureManager Implementation
// =============================================================================

TextureManager::TextureManager() = default;

TextureManager::~TextureManager() {
    shutdown();
}

bool TextureManager::initialize() {
    create_default_textures();
    spdlog::info("TextureManager initialized");
    return true;
}

void TextureManager::shutdown() {
    std::lock_guard lock(m_mutex);
    m_textures.clear();
    m_cubemaps.clear();
    m_path_to_handle.clear();
    spdlog::info("TextureManager shutdown");
}

std::uint64_t TextureManager::allocate_handle() {
    return m_next_handle.fetch_add(1, std::memory_order_relaxed);
}

void TextureManager::create_default_textures() {
    // White texture
    {
        TextureEntry entry;
        entry.texture = std::make_unique<Texture>();
        entry.texture->create(TextureData::default_white());
        entry.ref_count = UINT32_MAX;  // Never release
        auto handle = allocate_handle();
        m_textures[handle] = std::move(entry);
        m_default_white = TextureHandle(handle);
    }

    // Black texture
    {
        TextureEntry entry;
        entry.texture = std::make_unique<Texture>();
        entry.texture->create(TextureData::default_black());
        entry.ref_count = UINT32_MAX;
        auto handle = allocate_handle();
        m_textures[handle] = std::move(entry);
        m_default_black = TextureHandle(handle);
    }

    // Normal texture
    {
        TextureEntry entry;
        entry.texture = std::make_unique<Texture>();
        TextureLoadOptions opts;
        opts.srgb = false;
        entry.texture->create(TextureData::default_normal(), opts);
        entry.ref_count = UINT32_MAX;
        auto handle = allocate_handle();
        m_textures[handle] = std::move(entry);
        m_default_normal = TextureHandle(handle);
    }

    // Checkerboard texture
    {
        TextureEntry entry;
        entry.texture = std::make_unique<Texture>();
        entry.texture->create(TextureData::checkerboard());
        entry.ref_count = UINT32_MAX;
        auto handle = allocate_handle();
        m_textures[handle] = std::move(entry);
        m_default_checker = TextureHandle(handle);
    }
}

TextureHandle TextureManager::load(const std::filesystem::path& path,
                                    const TextureLoadOptions& options) {
    std::lock_guard lock(m_mutex);

    // Check if already loaded
    std::string key = std::filesystem::absolute(path).string();
    auto it = m_path_to_handle.find(key);
    if (it != m_path_to_handle.end()) {
        auto& entry = m_textures[it->second];
        entry.ref_count++;
        return TextureHandle(it->second);
    }

    // Load the texture
    auto data = TextureLoader::load(path, options);
    if (!data) {
        spdlog::error("Failed to load texture: {}", path.string());
        return TextureHandle::invalid();
    }

    TextureEntry entry;
    entry.texture = std::make_unique<Texture>();
    if (!entry.texture->create(*data, options)) {
        return TextureHandle::invalid();
    }

    entry.path = std::filesystem::absolute(path);
    entry.options = options;

    std::error_code ec;
    entry.last_modified = std::filesystem::last_write_time(entry.path, ec);

    auto handle = allocate_handle();
    m_textures[handle] = std::move(entry);
    m_path_to_handle[key] = handle;

    return TextureHandle(handle);
}

TextureHandle TextureManager::load_from_memory(const std::string& name,
                                                const TextureData& data,
                                                const TextureLoadOptions& options) {
    std::lock_guard lock(m_mutex);

    TextureEntry entry;
    entry.texture = std::make_unique<Texture>();
    if (!entry.texture->create(data, options)) {
        return TextureHandle::invalid();
    }

    entry.options = options;

    auto handle = allocate_handle();
    m_textures[handle] = std::move(entry);

    if (!name.empty()) {
        m_path_to_handle[name] = handle;
    }

    return TextureHandle(handle);
}

TextureHandle TextureManager::load_cubemap(const std::filesystem::path& path,
                                            std::uint32_t face_size) {
    std::lock_guard lock(m_mutex);

    // Check if already loaded
    std::string key = "cubemap:" + std::filesystem::absolute(path).string();
    auto it = m_path_to_handle.find(key);
    if (it != m_path_to_handle.end()) {
        auto& entry = m_cubemaps[it->second];
        entry.ref_count++;
        return TextureHandle(it->second);
    }

    // Load HDR
    auto hdr_data = TextureLoader::load_hdr(path);
    if (!hdr_data) {
        spdlog::error("Failed to load HDR for cubemap: {}", path.string());
        return TextureHandle::invalid();
    }

    CubemapEntry entry;
    entry.cubemap = std::make_unique<Cubemap>();
    if (!entry.cubemap->create_from_equirectangular(*hdr_data, face_size)) {
        return TextureHandle::invalid();
    }

    entry.path = std::filesystem::absolute(path);
    entry.face_size = face_size;

    std::error_code ec;
    entry.last_modified = std::filesystem::last_write_time(entry.path, ec);

    auto handle = allocate_handle();
    m_cubemaps[handle] = std::move(entry);
    m_path_to_handle[key] = handle;

    return TextureHandle(handle);
}

Texture* TextureManager::get(TextureHandle handle) {
    std::lock_guard lock(m_mutex);
    auto it = m_textures.find(handle.id());
    return it != m_textures.end() ? it->second.texture.get() : nullptr;
}

const Texture* TextureManager::get(TextureHandle handle) const {
    std::lock_guard lock(m_mutex);
    auto it = m_textures.find(handle.id());
    return it != m_textures.end() ? it->second.texture.get() : nullptr;
}

Cubemap* TextureManager::get_cubemap(TextureHandle handle) {
    std::lock_guard lock(m_mutex);
    auto it = m_cubemaps.find(handle.id());
    return it != m_cubemaps.end() ? it->second.cubemap.get() : nullptr;
}

const Cubemap* TextureManager::get_cubemap(TextureHandle handle) const {
    std::lock_guard lock(m_mutex);
    auto it = m_cubemaps.find(handle.id());
    return it != m_cubemaps.end() ? it->second.cubemap.get() : nullptr;
}

bool TextureManager::is_valid(TextureHandle handle) const {
    std::lock_guard lock(m_mutex);
    return m_textures.contains(handle.id()) || m_cubemaps.contains(handle.id());
}

void TextureManager::release(TextureHandle handle) {
    std::lock_guard lock(m_mutex);

    auto it = m_textures.find(handle.id());
    if (it != m_textures.end()) {
        if (it->second.ref_count != UINT32_MAX) {
            if (--it->second.ref_count == 0) {
                // Remove from path lookup
                if (!it->second.path.empty()) {
                    m_path_to_handle.erase(it->second.path.string());
                }
                m_textures.erase(it);
            }
        }
        return;
    }

    auto cit = m_cubemaps.find(handle.id());
    if (cit != m_cubemaps.end()) {
        if (--cit->second.ref_count == 0) {
            std::string key = "cubemap:" + cit->second.path.string();
            m_path_to_handle.erase(key);
            m_cubemaps.erase(cit);
        }
    }
}

bool TextureManager::reload(TextureHandle handle) {
    std::lock_guard lock(m_mutex);

    auto it = m_textures.find(handle.id());
    if (it != m_textures.end() && !it->second.path.empty()) {
        auto data = TextureLoader::load(it->second.path, it->second.options);
        if (data) {
            it->second.texture->destroy();
            it->second.texture->create(*data, it->second.options);

            std::error_code ec;
            it->second.last_modified = std::filesystem::last_write_time(it->second.path, ec);

            m_reload_count++;

            if (on_texture_reloaded) {
                on_texture_reloaded(handle);
            }
            return true;
        }
    }
    return false;
}

void TextureManager::update() {
    // Note: In a real implementation, track delta time
    m_reload_timer += 0.016f;  // Approximate 60fps

    if (m_reload_timer >= m_reload_interval) {
        m_reload_timer = 0.0f;
        check_for_reloads();
    }
}

void TextureManager::check_for_reloads() {
    std::lock_guard lock(m_mutex);

    for (auto& [id, entry] : m_textures) {
        if (entry.path.empty()) continue;

        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(entry.path, ec);
        if (ec) continue;

        if (current_time != entry.last_modified) {
            spdlog::info("Hot-reloading texture: {}", entry.path.filename().string());

            auto data = TextureLoader::load(entry.path, entry.options);
            if (data) {
                entry.texture->destroy();
                entry.texture->create(*data, entry.options);
                entry.last_modified = current_time;
                m_reload_count++;

                if (on_texture_reloaded) {
                    on_texture_reloaded(TextureHandle(id));
                }
            }
        }
    }
}

TextureManager::Stats TextureManager::stats() const {
    std::lock_guard lock(m_mutex);

    Stats s;
    s.texture_count = m_textures.size();
    s.cubemap_count = m_cubemaps.size();
    s.reload_count = m_reload_count;

    for (const auto& [id, entry] : m_textures) {
        if (entry.texture) {
            s.total_gpu_memory += entry.texture->gpu_memory_bytes();
        }
    }

    return s;
}

// =============================================================================
// IBLProcessor Implementation
// =============================================================================

std::unique_ptr<Texture> IBLProcessor::generate_brdf_lut(std::uint32_t size) {
    // Pre-compute BRDF LUT for split-sum approximation
    // This integrates the BRDF over all view angles and roughness values

    TextureData lut_data;
    lut_data.width = size;
    lut_data.height = size;
    lut_data.channels = 2;  // Store scale and bias
    lut_data.format = TextureFormat::Rg16Float;
    lut_data.is_srgb = false;

    std::vector<float> pixels(size * size * 2);

    const float pi = 3.14159265359f;

    for (std::uint32_t y = 0; y < size; ++y) {
        float roughness = (y + 0.5f) / size;

        for (std::uint32_t x = 0; x < size; ++x) {
            float NdotV = (x + 0.5f) / size;
            NdotV = std::max(NdotV, 0.001f);

            // Importance sampling
            float scale = 0.0f;
            float bias = 0.0f;
            const std::uint32_t sample_count = 1024;

            for (std::uint32_t i = 0; i < sample_count; ++i) {
                // Hammersley sequence
                float xi1 = static_cast<float>(i) / sample_count;
                std::uint32_t bits = i;
                bits = (bits << 16u) | (bits >> 16u);
                bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                float xi2 = static_cast<float>(bits) * 2.3283064365386963e-10f;

                // GGX importance sampling
                float a = roughness * roughness;
                float phi = 2.0f * pi * xi1;
                float cos_theta = std::sqrt((1.0f - xi2) / (1.0f + (a * a - 1.0f) * xi2));
                float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

                // Half vector in tangent space
                float Hx = std::cos(phi) * sin_theta;
                float Hy = std::sin(phi) * sin_theta;
                float Hz = cos_theta;

                // View vector
                float Vx = std::sqrt(1.0f - NdotV * NdotV);
                float Vy = 0.0f;
                float Vz = NdotV;

                // Reflect V around H to get L
                float VdotH = Vx * Hx + Vy * Hy + Vz * Hz;
                float Lx = 2.0f * VdotH * Hx - Vx;
                float Ly = 2.0f * VdotH * Hy - Vy;
                float Lz = 2.0f * VdotH * Hz - Vz;

                float NdotL = std::max(Lz, 0.0f);
                float NdotH = std::max(Hz, 0.0f);
                VdotH = std::max(VdotH, 0.0f);

                if (NdotL > 0.0f) {
                    // Geometry term
                    float k = (a * a) / 2.0f;
                    float G1_V = NdotV / (NdotV * (1.0f - k) + k);
                    float G1_L = NdotL / (NdotL * (1.0f - k) + k);
                    float G = G1_V * G1_L;

                    float G_Vis = (G * VdotH) / (NdotH * NdotV);
                    float Fc = std::pow(1.0f - VdotH, 5.0f);

                    scale += (1.0f - Fc) * G_Vis;
                    bias += Fc * G_Vis;
                }
            }

            scale /= static_cast<float>(sample_count);
            bias /= static_cast<float>(sample_count);

            std::size_t idx = (y * size + x) * 2;
            pixels[idx + 0] = scale;
            pixels[idx + 1] = bias;
        }
    }

    // Convert float data to the texture
    lut_data.pixels.resize(size * size * 4);  // Store as RGBA8 for simplicity
    for (std::size_t i = 0; i < size * size; ++i) {
        lut_data.pixels[i * 4 + 0] = static_cast<std::uint8_t>(std::clamp(pixels[i * 2 + 0] * 255.0f, 0.0f, 255.0f));
        lut_data.pixels[i * 4 + 1] = static_cast<std::uint8_t>(std::clamp(pixels[i * 2 + 1] * 255.0f, 0.0f, 255.0f));
        lut_data.pixels[i * 4 + 2] = 0;
        lut_data.pixels[i * 4 + 3] = 255;
    }
    lut_data.channels = 4;
    lut_data.format = TextureFormat::Rgba8Unorm;

    auto texture = std::make_unique<Texture>();
    TextureLoadOptions opts;
    opts.generate_mipmaps = false;
    opts.srgb = false;
    opts.filter = FilterMode::Linear;
    opts.wrap = AddressMode::ClampToEdge;

    if (!texture->create(lut_data, opts)) {
        return nullptr;
    }

    return texture;
}

IBLProcessor::IBLMaps IBLProcessor::create_from_hdr(const std::filesystem::path& hdr_path,
                                                     std::uint32_t env_size) {
    IBLMaps maps;

    auto hdr_data = TextureLoader::load_hdr(hdr_path);
    if (!hdr_data) {
        spdlog::error("Failed to load HDR for IBL: {}", hdr_path.string());
        return maps;
    }

    // Create environment cubemap
    maps.environment = std::make_unique<Cubemap>();
    if (!maps.environment->create_from_equirectangular(*hdr_data, env_size)) {
        spdlog::error("Failed to create environment cubemap");
        return maps;
    }

    // Create BRDF LUT
    maps.brdf_lut = generate_brdf_lut(512);
    if (!maps.brdf_lut) {
        spdlog::error("Failed to create BRDF LUT");
        return maps;
    }

    // Note: Irradiance and prefiltered maps would require GPU compute
    // For now, return with just environment and BRDF LUT
    spdlog::info("IBL maps created from: {}", hdr_path.filename().string());

    return maps;
}

} // namespace void_render
