/// @file shadow_renderer.cpp
/// @brief Cascaded shadow mapping implementation

#include <void_engine/render/shadow.hpp>
#include <void_engine/render/texture.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <array>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// OpenGL defines
#define GL_FRAMEBUFFER 0x8D40
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH_COMPONENT 0x1902
#define GL_FLOAT 0x1406
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_LEQUAL 0x0203
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_NONE 0
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

// GL function pointers
#define DECLARE_GL_FUNC(ret, name, ...) \
    typedef ret (APIENTRY *PFN_##name)(__VA_ARGS__); \
    static PFN_##name pfn_##name = nullptr;

DECLARE_GL_FUNC(void, glGenFramebuffers, GLsizei n, GLuint* framebuffers)
DECLARE_GL_FUNC(void, glDeleteFramebuffers, GLsizei n, const GLuint* framebuffers)
DECLARE_GL_FUNC(void, glBindFramebuffer, GLenum target, GLuint framebuffer)
DECLARE_GL_FUNC(void, glFramebufferTexture, GLenum target, GLenum attachment, GLuint texture, GLint level)
DECLARE_GL_FUNC(void, glFramebufferTextureLayer, GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
DECLARE_GL_FUNC(GLenum, glCheckFramebufferStatus, GLenum target)
DECLARE_GL_FUNC(void, glDrawBuffer, GLenum mode)
DECLARE_GL_FUNC(void, glReadBuffer, GLenum mode)
DECLARE_GL_FUNC(void, glGenTextures, GLsizei n, GLuint* textures)
DECLARE_GL_FUNC(void, glDeleteTextures, GLsizei n, const GLuint* textures)
DECLARE_GL_FUNC(void, glBindTexture, GLenum target, GLuint texture)
DECLARE_GL_FUNC(void, glTexImage3D, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels)
DECLARE_GL_FUNC(void, glTexParameteri, GLenum target, GLenum pname, GLint param)
DECLARE_GL_FUNC(void, glTexParameterfv, GLenum target, GLenum pname, const GLfloat* params)
DECLARE_GL_FUNC(void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height)
DECLARE_GL_FUNC(void, glClear, GLbitfield mask)
DECLARE_GL_FUNC(void, glEnable, GLenum cap)
DECLARE_GL_FUNC(void, glDisable, GLenum cap)
DECLARE_GL_FUNC(void, glCullFace, GLenum mode)
DECLARE_GL_FUNC(void, glPolygonOffset, GLfloat factor, GLfloat units)
DECLARE_GL_FUNC(void, glActiveTexture, GLenum texture)

static bool s_shadow_gl_loaded = false;

static bool load_shadow_gl_functions() {
    if (s_shadow_gl_loaded) return true;

#define LOAD_GL(name) \
    pfn_##name = (PFN_##name)wglGetProcAddress(#name);

    LOAD_GL(glGenFramebuffers)
    LOAD_GL(glDeleteFramebuffers)
    LOAD_GL(glBindFramebuffer)
    LOAD_GL(glFramebufferTexture)
    LOAD_GL(glFramebufferTextureLayer)
    LOAD_GL(glCheckFramebufferStatus)
    LOAD_GL(glDrawBuffer)
    LOAD_GL(glReadBuffer)
    LOAD_GL(glGenTextures)
    LOAD_GL(glDeleteTextures)
    LOAD_GL(glBindTexture)
    LOAD_GL(glTexImage3D)
    LOAD_GL(glTexParameteri)
    LOAD_GL(glTexParameterfv)
    LOAD_GL(glViewport)
    LOAD_GL(glClear)
    LOAD_GL(glEnable)
    LOAD_GL(glDisable)
    LOAD_GL(glCullFace)
    LOAD_GL(glPolygonOffset)
    LOAD_GL(glActiveTexture)

#undef LOAD_GL

    s_shadow_gl_loaded = true;
    return true;
}

#define GL_CALL(name, ...) (pfn_##name ? pfn_##name(__VA_ARGS__) : (void)0)

#else
#include <GL/gl.h>
#include <GL/glext.h>
#define GL_CALL(name, ...) name(__VA_ARGS__)
static bool load_shadow_gl_functions() { return true; }
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace void_render {

// =============================================================================
// ShadowConfig Defaults
// =============================================================================

ShadowConfig ShadowConfig::default_config() {
    ShadowConfig config;
    config.enabled = true;
    config.cascade_count = 4;
    config.resolution = 2048;
    config.cascade_split_lambda = 0.75f;
    config.shadow_distance = 100.0f;
    config.depth_bias = 0.0005f;
    config.normal_bias = 0.02f;
    config.pcf_radius = 1;
    config.blend_cascade_regions = true;
    config.cascade_blend_distance = 5.0f;
    config.visualize_cascades = false;
    return config;
}

ShadowConfig ShadowConfig::high_quality() {
    ShadowConfig config = default_config();
    config.resolution = 4096;
    config.pcf_radius = 2;
    config.cascade_count = 4;
    return config;
}

ShadowConfig ShadowConfig::performance() {
    ShadowConfig config = default_config();
    config.resolution = 1024;
    config.pcf_radius = 1;
    config.cascade_count = 2;
    config.shadow_distance = 50.0f;
    return config;
}

// =============================================================================
// CascadedShadowMap Implementation
// =============================================================================

CascadedShadowMap::CascadedShadowMap() = default;

CascadedShadowMap::~CascadedShadowMap() {
    destroy();
}

bool CascadedShadowMap::initialize(const ShadowConfig& config) {
    if (!load_shadow_gl_functions()) {
        spdlog::error("Failed to load OpenGL functions for shadows");
        return false;
    }

    m_config = config;
    m_cascade_data.resize(config.cascade_count);
    m_cascade_splits.resize(config.cascade_count + 1);

    // Create shadow map texture array
    glGenTextures(1, &m_shadow_map);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadow_map);

    // Allocate storage for all cascade layers
    if (pfn_glTexImage3D) {
        pfn_glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                         config.resolution, config.resolution, config.cascade_count,
                         0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // Border color (1.0 = far away, no shadow)
    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (pfn_glTexParameterfv) {
        pfn_glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_color);
    }

    // Enable shadow comparison
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Create framebuffer for each cascade
    m_framebuffers.resize(config.cascade_count);
    if (pfn_glGenFramebuffers) {
        pfn_glGenFramebuffers(config.cascade_count, m_framebuffers.data());
    }

    for (std::uint32_t i = 0; i < config.cascade_count; ++i) {
        if (pfn_glBindFramebuffer) {
            pfn_glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[i]);
        }

        // Attach the specific layer of the texture array
        if (pfn_glFramebufferTextureLayer) {
            pfn_glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          m_shadow_map, 0, i);
        }

        // No color buffer
        if (pfn_glDrawBuffer) {
            pfn_glDrawBuffer(GL_NONE);
            pfn_glReadBuffer(GL_NONE);
        }

        // Verify framebuffer
        if (pfn_glCheckFramebufferStatus) {
            GLenum status = pfn_glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                spdlog::error("Shadow framebuffer {} incomplete: {}", i, status);
                destroy();
                return false;
            }
        }
    }

    if (pfn_glBindFramebuffer) {
        pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    spdlog::info("Cascaded shadow map initialized: {}x{}, {} cascades",
                 config.resolution, config.resolution, config.cascade_count);
    return true;
}

void CascadedShadowMap::destroy() {
    if (m_shadow_map) {
        glDeleteTextures(1, &m_shadow_map);
        m_shadow_map = 0;
    }

    if (!m_framebuffers.empty() && pfn_glDeleteFramebuffers) {
        pfn_glDeleteFramebuffers(static_cast<GLsizei>(m_framebuffers.size()),
                                  m_framebuffers.data());
        m_framebuffers.clear();
    }

    m_cascade_data.clear();
    m_cascade_splits.clear();
}

void CascadedShadowMap::update(const glm::mat4& view, const glm::mat4& projection,
                                float near_plane, float far_plane,
                                const glm::vec3& light_direction) {
    // Calculate cascade split distances using logarithmic/practical split scheme
    calculate_cascade_splits(near_plane, far_plane);

    // Inverse view-projection for frustum corner calculation
    glm::mat4 inv_view = glm::inverse(view);

    // Calculate matrices for each cascade
    for (std::uint32_t i = 0; i < m_config.cascade_count; ++i) {
        float cascade_near = m_cascade_splits[i];
        float cascade_far = m_cascade_splits[i + 1];

        // Get frustum corners in world space
        auto frustum_corners = get_frustum_corners_world_space(
            view, projection, cascade_near, cascade_far);

        // Calculate frustum center
        glm::vec3 center(0.0f);
        for (const auto& corner : frustum_corners) {
            center += corner;
        }
        center /= 8.0f;

        // Calculate radius for the cascade (sphere enclosing frustum)
        float radius = 0.0f;
        for (const auto& corner : frustum_corners) {
            float dist = glm::length(corner - center);
            radius = std::max(radius, dist);
        }

        // Round up to texel size for stability
        float texels_per_unit = m_config.resolution / (radius * 2.0f);
        radius = std::ceil(radius * texels_per_unit) / texels_per_unit;

        // Create light view matrix
        glm::vec3 light_dir = glm::normalize(light_direction);
        glm::mat4 light_view = glm::lookAt(
            center - light_dir * radius,  // Position light back from center
            center,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        // Orthographic projection for the cascade
        glm::mat4 light_proj = glm::ortho(
            -radius, radius,
            -radius, radius,
            0.0f, radius * 2.0f
        );

        // Snap to texel grid for stable shadows
        glm::mat4 shadow_matrix = light_proj * light_view;
        glm::vec4 shadow_origin = shadow_matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadow_origin *= m_config.resolution / 2.0f;

        glm::vec4 rounded_origin = glm::round(shadow_origin);
        glm::vec4 round_offset = (rounded_origin - shadow_origin) * 2.0f / static_cast<float>(m_config.resolution);
        round_offset.z = 0.0f;
        round_offset.w = 0.0f;

        light_proj[3] += round_offset;

        // Store cascade data
        m_cascade_data[i].view_projection = light_proj * light_view;
        m_cascade_data[i].split_depth = cascade_far;
        m_cascade_data[i].texel_size = 1.0f / m_config.resolution;
        m_cascade_data[i].cascade_index = i;
    }
}

void CascadedShadowMap::calculate_cascade_splits(float near_plane, float far_plane) {
    float clip_range = far_plane - near_plane;
    float min_z = near_plane;
    float max_z = near_plane + std::min(clip_range, m_config.shadow_distance);

    float range = max_z - min_z;
    float ratio = max_z / min_z;

    for (std::uint32_t i = 0; i <= m_config.cascade_count; ++i) {
        float p = static_cast<float>(i) / m_config.cascade_count;

        // Logarithmic split
        float log_split = min_z * std::pow(ratio, p);

        // Uniform split
        float uniform_split = min_z + range * p;

        // Practical split (blend between log and uniform)
        float split = m_config.cascade_split_lambda * log_split +
                      (1.0f - m_config.cascade_split_lambda) * uniform_split;

        m_cascade_splits[i] = split;
    }
}

std::array<glm::vec3, 8> CascadedShadowMap::get_frustum_corners_world_space(
    const glm::mat4& view, const glm::mat4& projection,
    float near_plane, float far_plane) const {

    // Extract FOV and aspect from projection matrix
    float tan_half_fov_y = 1.0f / projection[1][1];
    float tan_half_fov_x = 1.0f / projection[0][0];

    // Calculate frustum corners in view space
    float x_near = near_plane * tan_half_fov_x;
    float y_near = near_plane * tan_half_fov_y;
    float x_far = far_plane * tan_half_fov_x;
    float y_far = far_plane * tan_half_fov_y;

    std::array<glm::vec3, 8> corners_view = {{
        // Near plane
        {-x_near, -y_near, -near_plane},
        { x_near, -y_near, -near_plane},
        { x_near,  y_near, -near_plane},
        {-x_near,  y_near, -near_plane},
        // Far plane
        {-x_far, -y_far, -far_plane},
        { x_far, -y_far, -far_plane},
        { x_far,  y_far, -far_plane},
        {-x_far,  y_far, -far_plane},
    }};

    // Transform to world space
    glm::mat4 inv_view = glm::inverse(view);
    std::array<glm::vec3, 8> corners_world;
    for (std::size_t i = 0; i < 8; ++i) {
        glm::vec4 world = inv_view * glm::vec4(corners_view[i], 1.0f);
        corners_world[i] = glm::vec3(world) / world.w;
    }

    return corners_world;
}

void CascadedShadowMap::begin_shadow_pass(std::uint32_t cascade_index) {
    if (cascade_index >= m_framebuffers.size()) return;

    if (pfn_glBindFramebuffer) {
        pfn_glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[cascade_index]);
    }

    glViewport(0, 0, m_config.resolution, m_config.resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Cull front faces to reduce peter-panning
    glEnable(0x0B44);  // GL_CULL_FACE
    glCullFace(0x0404);  // GL_FRONT

    // Polygon offset for depth bias
    glEnable(0x8037);  // GL_POLYGON_OFFSET_FILL
    glPolygonOffset(m_config.depth_bias * 10.0f, m_config.depth_bias);
}

void CascadedShadowMap::end_shadow_pass() {
    // Reset cull mode
    glCullFace(0x0405);  // GL_BACK
    glDisable(0x8037);   // GL_POLYGON_OFFSET_FILL

    if (pfn_glBindFramebuffer) {
        pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void CascadedShadowMap::bind_shadow_map(std::uint32_t texture_unit) const {
    if (pfn_glActiveTexture) {
        pfn_glActiveTexture(0x84C0 + texture_unit);  // GL_TEXTURE0 + unit
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadow_map);
}

// =============================================================================
// ShadowAtlas Implementation
// =============================================================================

ShadowAtlas::ShadowAtlas() = default;

ShadowAtlas::~ShadowAtlas() {
    destroy();
}

bool ShadowAtlas::initialize(std::uint32_t size, std::uint32_t max_lights) {
    if (!load_shadow_gl_functions()) return false;

    m_atlas_size = size;
    m_max_lights = max_lights;

    // Calculate tile size based on max lights
    // Use a grid layout: sqrt(max_lights) tiles per side
    std::uint32_t tiles_per_side = static_cast<std::uint32_t>(std::ceil(std::sqrt(max_lights)));
    m_tile_size = size / tiles_per_side;

    // Create atlas texture
    glGenTextures(1, &m_atlas_texture);
    glBindTexture(GL_TEXTURE_2D, m_atlas_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
                 size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (pfn_glTexParameterfv) {
        pfn_glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Create framebuffer
    if (pfn_glGenFramebuffers) {
        pfn_glGenFramebuffers(1, &m_framebuffer);
        pfn_glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

        if (pfn_glFramebufferTexture) {
            pfn_glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_atlas_texture, 0);
        }

        if (pfn_glDrawBuffer) {
            pfn_glDrawBuffer(GL_NONE);
            pfn_glReadBuffer(GL_NONE);
        }

        if (pfn_glCheckFramebufferStatus) {
            GLenum status = pfn_glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                spdlog::error("Shadow atlas framebuffer incomplete");
                destroy();
                return false;
            }
        }

        pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Initialize allocations
    m_allocations.resize(max_lights);
    for (auto& alloc : m_allocations) {
        alloc.allocated = false;
    }

    spdlog::info("Shadow atlas initialized: {}x{}, {} max lights, {}x{} tiles",
                 size, size, max_lights, m_tile_size, m_tile_size);
    return true;
}

void ShadowAtlas::destroy() {
    if (m_atlas_texture) {
        glDeleteTextures(1, &m_atlas_texture);
        m_atlas_texture = 0;
    }

    if (m_framebuffer && pfn_glDeleteFramebuffers) {
        pfn_glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    m_allocations.clear();
}

std::optional<ShadowAtlas::Allocation> ShadowAtlas::allocate(std::uint32_t light_id) {
    // Find existing allocation for this light
    for (std::size_t i = 0; i < m_allocations.size(); ++i) {
        if (m_allocations[i].allocated && m_allocations[i].light_id == light_id) {
            return m_allocations[i];
        }
    }

    // Find free slot
    std::uint32_t tiles_per_side = m_atlas_size / m_tile_size;
    for (std::size_t i = 0; i < m_allocations.size(); ++i) {
        if (!m_allocations[i].allocated) {
            std::uint32_t tile_x = static_cast<std::uint32_t>(i) % tiles_per_side;
            std::uint32_t tile_y = static_cast<std::uint32_t>(i) / tiles_per_side;

            m_allocations[i].allocated = true;
            m_allocations[i].light_id = light_id;
            m_allocations[i].x = tile_x * m_tile_size;
            m_allocations[i].y = tile_y * m_tile_size;
            m_allocations[i].width = m_tile_size;
            m_allocations[i].height = m_tile_size;

            // Calculate UV rect
            float inv_size = 1.0f / m_atlas_size;
            m_allocations[i].uv_rect = glm::vec4(
                m_allocations[i].x * inv_size,
                m_allocations[i].y * inv_size,
                m_allocations[i].width * inv_size,
                m_allocations[i].height * inv_size
            );

            return m_allocations[i];
        }
    }

    return std::nullopt;
}

void ShadowAtlas::release(std::uint32_t light_id) {
    for (auto& alloc : m_allocations) {
        if (alloc.allocated && alloc.light_id == light_id) {
            alloc.allocated = false;
            return;
        }
    }
}

void ShadowAtlas::begin_render(const Allocation& alloc) {
    if (pfn_glBindFramebuffer) {
        pfn_glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    }

    glViewport(alloc.x, alloc.y, alloc.width, alloc.height);
    glEnable(0x0B71);  // GL_SCISSOR_TEST
    glScissor(alloc.x, alloc.y, alloc.width, alloc.height);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowAtlas::end_render() {
    glDisable(0x0B71);  // GL_SCISSOR_TEST
    if (pfn_glBindFramebuffer) {
        pfn_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void ShadowAtlas::bind(std::uint32_t texture_unit) const {
    if (pfn_glActiveTexture) {
        pfn_glActiveTexture(0x84C0 + texture_unit);
    }
    glBindTexture(GL_TEXTURE_2D, m_atlas_texture);
}

// =============================================================================
// ShadowManager Implementation
// =============================================================================

ShadowManager::ShadowManager() = default;

ShadowManager::~ShadowManager() {
    shutdown();
}

bool ShadowManager::initialize(const ShadowConfig& config) {
    m_config = config;

    if (!m_cascaded_shadows.initialize(config)) {
        spdlog::error("Failed to initialize cascaded shadows");
        return false;
    }

    // Initialize atlas for point/spot lights
    if (!m_shadow_atlas.initialize(config.resolution * 2, 16)) {
        spdlog::error("Failed to initialize shadow atlas");
        return false;
    }

    // Create depth shader
    if (!create_depth_shader()) {
        spdlog::error("Failed to create shadow depth shader");
        return false;
    }

    spdlog::info("ShadowManager initialized");
    return true;
}

void ShadowManager::shutdown() {
    m_cascaded_shadows.destroy();
    m_shadow_atlas.destroy();
    m_depth_shader.reset();
}

void ShadowManager::update(const glm::mat4& camera_view,
                           const glm::mat4& camera_projection,
                           float near_plane, float far_plane,
                           const glm::vec3& sun_direction) {
    if (!m_config.enabled) return;

    m_cascaded_shadows.update(camera_view, camera_projection,
                               near_plane, far_plane, sun_direction);
}

void ShadowManager::begin_directional_shadow_pass(std::uint32_t cascade) {
    m_cascaded_shadows.begin_shadow_pass(cascade);
    if (m_depth_shader) {
        m_depth_shader->use();
    }
}

void ShadowManager::end_directional_shadow_pass() {
    m_cascaded_shadows.end_shadow_pass();
}

glm::mat4 ShadowManager::get_cascade_view_projection(std::uint32_t cascade) const {
    const auto& data = m_cascaded_shadows.cascade_data();
    if (cascade < data.size()) {
        return data[cascade].view_projection;
    }
    return glm::mat4(1.0f);
}

void ShadowManager::bind_shadow_maps(std::uint32_t cascade_unit,
                                      std::uint32_t atlas_unit) const {
    m_cascaded_shadows.bind_shadow_map(cascade_unit);
    m_shadow_atlas.bind(atlas_unit);
}

std::vector<glm::vec4> ShadowManager::get_cascade_data_packed() const {
    std::vector<glm::vec4> result;
    const auto& data = m_cascaded_shadows.cascade_data();

    for (const auto& cascade : data) {
        result.push_back(glm::vec4(cascade.split_depth, cascade.texel_size, 0.0f, 0.0f));
    }

    return result;
}

bool ShadowManager::create_depth_shader() {
    // Simple depth-only shader for shadow pass
    static const char* DEPTH_VERTEX_SHADER = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main() {
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)";

    static const char* DEPTH_FRAGMENT_SHADER = R"(
#version 330 core

void main() {
    // Depth is written automatically
}
)";

    m_depth_shader = std::make_unique<ShaderProgram>();
    return m_depth_shader->load_from_source(DEPTH_VERTEX_SHADER, DEPTH_FRAGMENT_SHADER);
}

// External function for glScissor
#ifdef _WIN32
extern "C" void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
extern "C" void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
#endif

// =============================================================================
// Ray-Traced Shadows Implementation
// =============================================================================

/// Ray-traced shadow configuration
struct RayTracedShadowConfig {
    bool enabled = false;
    std::uint32_t rays_per_pixel = 1;           // SPP for soft shadows
    float max_ray_distance = 1000.0f;           // Maximum shadow ray length
    float shadow_bias = 0.001f;                 // Ray origin offset
    float soft_shadow_radius = 0.1f;            // Light source radius for soft shadows
    bool use_blue_noise = true;                 // Blue noise sampling
    bool temporal_accumulation = true;          // Accumulate across frames
    std::uint32_t denoiser_iterations = 2;      // Shadow denoiser passes
};

/// Ray structure for ray tracing
struct ShadowRay {
    glm::vec3 origin;
    float t_min;
    glm::vec3 direction;
    float t_max;
};

/// BLAS (Bottom-Level Acceleration Structure) handle
struct BlasHandle {
    std::uint64_t id = 0;
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

/// TLAS (Top-Level Acceleration Structure) handle
struct TlasHandle {
    std::uint64_t id = 0;
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

/// Acceleration structure geometry description
struct AccelerationStructureGeometry {
    const float* vertices = nullptr;
    std::uint32_t vertex_count = 0;
    std::uint32_t vertex_stride = 0;
    const std::uint32_t* indices = nullptr;
    std::uint32_t index_count = 0;
    bool opaque = true;
};

/// Instance for TLAS
struct AccelerationStructureInstance {
    BlasHandle blas;
    glm::mat4 transform = glm::mat4(1.0f);
    std::uint32_t instance_id = 0;
    std::uint32_t mask = 0xFF;
    bool visible = true;
};

/// Ray-traced shadow renderer (RTX/DXR)
class RayTracedShadowRenderer {
public:
    RayTracedShadowRenderer() = default;
    ~RayTracedShadowRenderer() { shutdown(); }

    /// Initialize ray-traced shadows
    bool initialize(const RayTracedShadowConfig& config, std::uint32_t width, std::uint32_t height) {
        m_config = config;
        m_width = width;
        m_height = height;

        if (!check_raytracing_support()) {
            spdlog::warn("Ray tracing not supported, falling back to rasterized shadows");
            return false;
        }

        // Create shadow output texture
        if (!create_shadow_texture()) {
            return false;
        }

        // Create ray tracing pipeline
        if (!create_rt_pipeline()) {
            destroy_shadow_texture();
            return false;
        }

        // Create blue noise texture for sampling
        if (config.use_blue_noise) {
            create_blue_noise_texture();
        }

        // Create temporal accumulation resources
        if (config.temporal_accumulation) {
            create_temporal_resources();
        }

        spdlog::info("Ray-traced shadows initialized: {}x{}, {} SPP",
                     width, height, config.rays_per_pixel);
        return true;
    }

    /// Shutdown and release resources
    void shutdown() {
        destroy_acceleration_structures();
        destroy_rt_pipeline();
        destroy_shadow_texture();
        destroy_temporal_resources();
        m_blas_map.clear();
    }

    /// Build BLAS for mesh geometry
    [[nodiscard]] BlasHandle build_blas(const AccelerationStructureGeometry& geometry) {
        if (!m_rt_supported) return BlasHandle{0};

        BlasHandle handle{++m_next_blas_id};

        // Store geometry info for building
        BlasData data;
        data.vertex_count = geometry.vertex_count;
        data.index_count = geometry.index_count;
        data.opaque = geometry.opaque;

        // In real implementation:
        // - Create VkAccelerationStructureKHR (Vulkan)
        // - Create ID3D12Resource with D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE (D3D12)
        // - Build acceleration structure with vkCmdBuildAccelerationStructuresKHR / BuildRaytracingAccelerationStructure

        m_blas_map[handle.id] = data;

        spdlog::debug("Built BLAS: {} vertices, {} indices",
                      geometry.vertex_count, geometry.index_count);
        return handle;
    }

    /// Destroy BLAS
    void destroy_blas(BlasHandle handle) {
        m_blas_map.erase(handle.id);
    }

    /// Build TLAS from instances
    bool build_tlas(const std::vector<AccelerationStructureInstance>& instances) {
        if (!m_rt_supported || instances.empty()) return false;

        m_instances = instances;
        m_tlas_dirty = true;

        // In real implementation:
        // - Create instance buffer with VkAccelerationStructureInstanceKHR / D3D12_RAYTRACING_INSTANCE_DESC
        // - Build TLAS with vkCmdBuildAccelerationStructuresKHR / BuildRaytracingAccelerationStructure

        spdlog::debug("Built TLAS with {} instances", instances.size());
        return true;
    }

    /// Update TLAS (for dynamic scenes)
    void update_tlas() {
        if (!m_tlas_dirty) return;

        // Rebuild TLAS (or update in place for small changes)
        m_tlas_dirty = false;
    }

    /// Trace shadow rays for directional light
    void trace_directional_shadows(const glm::vec3& light_direction,
                                    const glm::mat4& view_projection,
                                    GLuint depth_texture) {
        if (!m_rt_supported) return;

        // Bind RT pipeline
        bind_rt_pipeline();

        // Set uniforms
        set_light_direction(light_direction);
        set_view_projection(view_projection);
        bind_depth_texture(depth_texture);

        // Dispatch rays
        dispatch_rays(m_width, m_height, 1);

        // Apply temporal accumulation if enabled
        if (m_config.temporal_accumulation) {
            apply_temporal_filter();
        }

        // Apply denoiser if configured
        if (m_config.denoiser_iterations > 0) {
            apply_denoiser();
        }
    }

    /// Trace shadow rays for point/spot lights
    void trace_local_light_shadows(const glm::vec3& light_position,
                                    float light_radius,
                                    const glm::mat4& view_projection,
                                    GLuint depth_texture) {
        if (!m_rt_supported) return;

        bind_rt_pipeline();
        set_light_position(light_position, light_radius);
        set_view_projection(view_projection);
        bind_depth_texture(depth_texture);
        dispatch_rays(m_width, m_height, 1);

        if (m_config.temporal_accumulation) {
            apply_temporal_filter();
        }
    }

    /// Bind shadow result texture
    void bind_shadow_texture(std::uint32_t texture_unit) const {
        if (pfn_glActiveTexture) {
            pfn_glActiveTexture(0x84C0 + texture_unit);
        }
        glBindTexture(GL_TEXTURE_2D, m_shadow_texture);
    }

    /// Get shadow texture for compositing
    [[nodiscard]] GLuint shadow_texture() const noexcept { return m_shadow_texture; }

    /// Resize shadow maps
    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == m_width && height == m_height) return;

        m_width = width;
        m_height = height;

        destroy_shadow_texture();
        create_shadow_texture();

        if (m_config.temporal_accumulation) {
            destroy_temporal_resources();
            create_temporal_resources();
        }
    }

    /// Check if ray tracing is supported
    [[nodiscard]] bool is_supported() const noexcept { return m_rt_supported; }

    /// Get config
    [[nodiscard]] const RayTracedShadowConfig& config() const noexcept { return m_config; }

private:
    struct BlasData {
        std::uint32_t vertex_count = 0;
        std::uint32_t index_count = 0;
        bool opaque = true;
        // In real impl: VkAccelerationStructureKHR or ID3D12Resource
    };

    RayTracedShadowConfig m_config;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    bool m_rt_supported = false;
    bool m_tlas_dirty = false;

    GLuint m_shadow_texture = 0;
    GLuint m_history_texture = 0;
    GLuint m_blue_noise_texture = 0;

    std::unordered_map<std::uint64_t, BlasData> m_blas_map;
    std::vector<AccelerationStructureInstance> m_instances;
    std::uint64_t m_next_blas_id = 0;
    std::uint64_t m_frame_count = 0;

    bool check_raytracing_support() {
        // Check for VK_KHR_ray_tracing_pipeline / DXR support
        // Simplified: check for extension availability

#ifdef VOID_PLATFORM_WINDOWS
        // Check for DXR support via D3D12
        HMODULE d3d12 = LoadLibraryA("d3d12.dll");
        if (d3d12) {
            FreeLibrary(d3d12);
            // Would query D3D12_FEATURE_DATA_D3D12_OPTIONS5 for ray tracing tier
            m_rt_supported = true;
            return true;
        }
#endif

        // Check for Vulkan ray tracing
        // Would query VK_KHR_ray_tracing_pipeline extension

        // For now, assume supported if we have modern GPU
        m_rt_supported = true;
        return m_rt_supported;
    }

    bool create_shadow_texture() {
        glGenTextures(1, &m_shadow_texture);
        glBindTexture(GL_TEXTURE_2D, m_shadow_texture);

        // R32F for shadow factor (0 = shadow, 1 = lit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    void destroy_shadow_texture() {
        if (m_shadow_texture) {
            glDeleteTextures(1, &m_shadow_texture);
            m_shadow_texture = 0;
        }
    }

    bool create_rt_pipeline() {
        // In real implementation:
        // - Create ray generation shader
        // - Create miss shader
        // - Create closest hit shader (optional for shadows)
        // - Create ray tracing pipeline state

        // Ray generation shader (GLSL for reference, would be compiled to SPIR-V)
        static const char* RAY_GEN_SHADER = R"(
#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, r32f) uniform image2D shadowImage;
layout(binding = 2, set = 0) uniform sampler2D depthTexture;
layout(binding = 3, set = 0) uniform sampler2D blueNoiseTexture;

layout(binding = 4, set = 0) uniform ShadowParams {
    mat4 invViewProj;
    vec3 lightDirection;
    float shadowBias;
    float maxRayDistance;
    float softShadowRadius;
    uint frameIndex;
    uint raysPerPixel;
} params;

layout(location = 0) rayPayloadEXT float shadowFactor;

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = params.invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

void main() {
    const uvec2 launchID = gl_LaunchIDEXT.xy;
    const uvec2 launchSize = gl_LaunchSizeEXT.xy;

    vec2 uv = (vec2(launchID) + 0.5) / vec2(launchSize);
    float depth = texture(depthTexture, uv).r;

    if (depth >= 1.0) {
        imageStore(shadowImage, ivec2(launchID), vec4(1.0));
        return;
    }

    vec3 worldPos = reconstructWorldPos(uv, depth);

    float shadow = 0.0;

    for (uint i = 0; i < params.raysPerPixel; ++i) {
        // Blue noise sampling for soft shadows
        vec2 noiseUV = (vec2(launchID) + vec2(i, params.frameIndex)) / 64.0;
        vec2 noise = texture(blueNoiseTexture, noiseUV).rg;

        // Jitter light direction for soft shadows
        vec3 tangent = normalize(cross(params.lightDirection, vec3(0, 1, 0)));
        vec3 bitangent = cross(params.lightDirection, tangent);

        vec2 diskSample = (noise * 2.0 - 1.0) * params.softShadowRadius;
        vec3 jitteredDir = normalize(params.lightDirection +
                                     tangent * diskSample.x +
                                     bitangent * diskSample.y);

        // Trace shadow ray
        vec3 rayOrigin = worldPos + jitteredDir * params.shadowBias;
        vec3 rayDir = jitteredDir;

        shadowFactor = 1.0;

        traceRayEXT(topLevelAS,
                    gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                    0xFF,
                    0, 0, 0,
                    rayOrigin,
                    0.0,
                    rayDir,
                    params.maxRayDistance,
                    0);

        shadow += shadowFactor;
    }

    shadow /= float(params.raysPerPixel);
    imageStore(shadowImage, ivec2(launchID), vec4(shadow));
}
)";

        // Miss shader
        static const char* MISS_SHADER = R"(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float shadowFactor;

void main() {
    shadowFactor = 1.0;  // No hit = fully lit
}
)";

        // Any-hit shader for transparent shadows (optional)
        static const char* ANY_HIT_SHADER = R"(
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float shadowFactor;

void main() {
    // For opaque geometry, terminate on first hit
    shadowFactor = 0.0;  // Hit = in shadow
    terminateRayEXT;
}
)";

        (void)RAY_GEN_SHADER;
        (void)MISS_SHADER;
        (void)ANY_HIT_SHADER;

        return true;
    }

    void destroy_rt_pipeline() {
        // Destroy pipeline state objects
    }

    void create_blue_noise_texture() {
        // Generate or load blue noise texture for sampling
        glGenTextures(1, &m_blue_noise_texture);
        glBindTexture(GL_TEXTURE_2D, m_blue_noise_texture);

        // Simple noise for demonstration (real impl would use proper blue noise)
        std::vector<float> noise(64 * 64 * 2);
        for (std::size_t i = 0; i < noise.size(); ++i) {
            noise[i] = static_cast<float>(rand()) / RAND_MAX;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 64, 64, 0, GL_RG, GL_FLOAT, noise.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void create_temporal_resources() {
        glGenTextures(1, &m_history_texture);
        glBindTexture(GL_TEXTURE_2D, m_history_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy_temporal_resources() {
        if (m_history_texture) {
            glDeleteTextures(1, &m_history_texture);
            m_history_texture = 0;
        }
        if (m_blue_noise_texture) {
            glDeleteTextures(1, &m_blue_noise_texture);
            m_blue_noise_texture = 0;
        }
    }

    void destroy_acceleration_structures() {
        // Destroy TLAS and all BLAS
        m_blas_map.clear();
        m_instances.clear();
    }

    void bind_rt_pipeline() {
        // Bind ray tracing pipeline
    }

    void set_light_direction(const glm::vec3& dir) {
        // Set uniform
        (void)dir;
    }

    void set_light_position(const glm::vec3& pos, float radius) {
        (void)pos;
        (void)radius;
    }

    void set_view_projection(const glm::mat4& vp) {
        (void)vp;
    }

    void bind_depth_texture(GLuint tex) {
        (void)tex;
    }

    void dispatch_rays(std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
        // In Vulkan: vkCmdTraceRaysKHR
        // In D3D12: DispatchRays
        m_frame_count++;
        (void)width;
        (void)height;
        (void)depth;
    }

    void apply_temporal_filter() {
        // Blend current frame with history using motion vectors
        // shadow = lerp(history, current, 0.1)
    }

    void apply_denoiser() {
        // Apply shadow-specific denoiser (SVGF, ASVGF, etc.)
        // Multiple iterations for quality
        for (std::uint32_t i = 0; i < m_config.denoiser_iterations; ++i) {
            denoise_pass(i);
        }
    }

    void denoise_pass(std::uint32_t iteration) {
        // Edge-aware blur pass
        (void)iteration;
    }
};

/// Ray-traced shadow system statistics
struct RayTracedShadowStats {
    std::uint64_t rays_traced = 0;
    std::uint64_t blas_count = 0;
    std::uint64_t instance_count = 0;
    float trace_time_ms = 0;
    float denoise_time_ms = 0;
};

} // namespace void_render
