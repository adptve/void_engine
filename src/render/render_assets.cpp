/// @file render_assets.cpp
/// @brief Hot-reloadable render asset manager - integrates existing systems
///
/// This module integrates:
/// - GltfLoader/GltfSceneManager for model loading (gltf_loader.hpp)
/// - ShaderProgram for shader management (gl_renderer.hpp)
/// - Existing hot-reload infrastructure (core/hot_reload.hpp)

#include <void_engine/render/render_assets.hpp>
#include <void_engine/render/gltf_loader.hpp>
#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/render/mesh.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <mutex>
#include <numbers>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// GL constants
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_SRGB 0x8C40
#define GL_SRGB_ALPHA 0x8C42
#define GL_UNSIGNED_BYTE 0x1401
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FLOAT 0x1406

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

// Import GL function pointers from gl_renderer.cpp
extern "C" {
    typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
    typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
    typedef void (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
    typedef void (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
    typedef void (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
    typedef void (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    typedef void (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
    typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
    typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
    typedef void (APIENTRY *PFNGLGENERATEMIPMAPPROC)(GLenum target);
}

// GL function wrappers using gl_renderer's loaded functions
namespace {
    // Forward declare - these are loaded in gl_renderer.cpp
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_fn = nullptr;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray_fn = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_fn = nullptr;
    PFNGLGENBUFFERSPROC glGenBuffers_fn = nullptr;
    PFNGLBINDBUFFERPROC glBindBuffer_fn = nullptr;
    PFNGLBUFFERDATAPROC glBufferData_fn = nullptr;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers_fn = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_fn = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_fn = nullptr;
    PFNGLGENERATEMIPMAPPROC glGenerateMipmap_fn = nullptr;

    bool gl_funcs_loaded = false;

    void ensure_gl_funcs() {
        if (gl_funcs_loaded) return;

        // Get functions via wglGetProcAddress
        glGenVertexArrays_fn = (PFNGLGENVERTEXARRAYSPROC)wglGetProcAddress("glGenVertexArrays");
        glBindVertexArray_fn = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
        glDeleteVertexArrays_fn = (PFNGLDELETEVERTEXARRAYSPROC)wglGetProcAddress("glDeleteVertexArrays");
        glGenBuffers_fn = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
        glBindBuffer_fn = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
        glBufferData_fn = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
        glDeleteBuffers_fn = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
        glEnableVertexAttribArray_fn = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
        glVertexAttribPointer_fn = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
        glGenerateMipmap_fn = (PFNGLGENERATEMIPMAPPROC)wglGetProcAddress("glGenerateMipmap");

        gl_funcs_loaded = true;
    }
}

#endif // _WIN32

namespace void_render {

namespace {

constexpr const char* kDefaultPbrVertexShader = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aColor;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 VertexColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main() {
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    TexCoords = aUV;
    VertexColor = aColor;
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)";

constexpr const char* kDefaultPbrFragmentShader = R"(
#version 330 core

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 VertexColor;

out vec4 FragColor;

uniform vec3 albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform vec3 emissive;
uniform float emissiveStrength;

uniform vec3 camPos;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform float lightIntensities[4];
uniform int numLights;

uniform vec3 ambientColor;
uniform float ambientIntensity;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < numLights; ++i) {
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = ambientColor * ambientIntensity * albedo * ao;
    vec3 emission = emissive * emissiveStrength;
    vec3 color = ambient + Lo + emission;

    FragColor = vec4(color, 1.0);
}
)";

std::array<float, 3> vec3_sub(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

std::array<float, 3> vec3_cross(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

std::array<float, 3> vec3_normalize(const std::array<float, 3>& v) {
    float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len <= 0.0f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {v[0] / len, v[1] / len, v[2] / len};
}

void update_bounds(std::array<float, 3>& min_bounds,
                   std::array<float, 3>& max_bounds,
                   const std::array<float, 3>& position) {
    for (int i = 0; i < 3; ++i) {
        min_bounds[i] = std::min(min_bounds[i], position[i]);
        max_bounds[i] = std::max(max_bounds[i], position[i]);
    }
}

} // namespace

// GpuMesh::destroy() is implemented in gl_renderer.cpp
// We use the existing implementation from gl_renderer.hpp

// =============================================================================
// GpuTexture Implementation
// =============================================================================

void GpuTexture::destroy() {
    if (id) {
        glDeleteTextures(1, &id);
        id = 0;
    }
    width = height = channels = 0;
}

// =============================================================================
// GpuShader Implementation
// =============================================================================

void GpuShader::destroy() {
    // Will be managed by ShaderProgram wrapper
    program = 0;
    uniform_cache.clear();
}

void GpuShader::use() const {
    if (program) {
        // Use the gl_renderer's function
        auto use_prog = (void (APIENTRY*)(GLuint))wglGetProcAddress("glUseProgram");
        if (use_prog) use_prog(program);
    }
}

std::int32_t GpuShader::get_location(const std::string& name) const {
    auto it = uniform_cache.find(name);
    if (it != uniform_cache.end()) {
        return it->second;
    }
    auto get_loc = (GLint (APIENTRY*)(GLuint, const GLchar*))wglGetProcAddress("glGetUniformLocation");
    if (!get_loc) return -1;
    std::int32_t loc = get_loc(program, name.c_str());
    uniform_cache[name] = loc;
    return loc;
}

void GpuShader::set_int(const std::string& name, int value) const {
    auto uniform1i = (void (APIENTRY*)(GLint, GLint))wglGetProcAddress("glUniform1i");
    if (uniform1i) uniform1i(get_location(name), value);
}

void GpuShader::set_float(const std::string& name, float value) const {
    auto uniform1f = (void (APIENTRY*)(GLint, GLfloat))wglGetProcAddress("glUniform1f");
    if (uniform1f) uniform1f(get_location(name), value);
}

void GpuShader::set_vec2(const std::string& name, float x, float y) const {
    float v[2] = {x, y};
    auto uniform2fv = (void (APIENTRY*)(GLint, GLsizei, const GLfloat*))wglGetProcAddress("glUniform2fv");
    if (uniform2fv) uniform2fv(get_location(name), 1, v);
}

void GpuShader::set_vec3(const std::string& name, float x, float y, float z) const {
    float v[3] = {x, y, z};
    auto uniform3fv = (void (APIENTRY*)(GLint, GLsizei, const GLfloat*))wglGetProcAddress("glUniform3fv");
    if (uniform3fv) uniform3fv(get_location(name), 1, v);
}

void GpuShader::set_vec4(const std::string& name, float x, float y, float z, float w) const {
    float v[4] = {x, y, z, w};
    auto uniform4fv = (void (APIENTRY*)(GLint, GLsizei, const GLfloat*))wglGetProcAddress("glUniform4fv");
    if (uniform4fv) uniform4fv(get_location(name), 1, v);
}

void GpuShader::set_mat3(const std::string& name, const float* data) const {
    auto uniformMat3 = (void (APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*))wglGetProcAddress("glUniformMatrix3fv");
    if (uniformMat3) uniformMat3(get_location(name), 1, GL_FALSE, data);
}

void GpuShader::set_mat4(const std::string& name, const float* data) const {
    auto uniformMat4 = (void (APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*))wglGetProcAddress("glUniformMatrix4fv");
    if (uniformMat4) uniformMat4(get_location(name), 1, GL_FALSE, data);
}

// =============================================================================
// LoadedModel Implementation
// =============================================================================

void LoadedModel::destroy() {
    for (auto& mesh : meshes) {
        mesh.destroy();
    }
    meshes.clear();
    for (auto& tex : textures) {
        tex.destroy();
    }
    textures.clear();
    materials.clear();
    nodes.clear();
    root_nodes.clear();
}

// =============================================================================
// LoadedTexture Implementation
// =============================================================================

void LoadedTexture::destroy() {
    gpu_texture.destroy();
}

// =============================================================================
// LoadedShader Implementation
// =============================================================================

void LoadedShader::destroy() {
    gpu_shader.destroy();
}

// =============================================================================
// RenderAssetManager Implementation - Uses Existing Systems
// =============================================================================

class RenderAssetManager::Impl {
public:
    std::filesystem::path asset_root;
    bool hot_reload_enabled = true;

    // INTEGRATION: Use existing GltfSceneManager for model loading
    GltfSceneManager scene_manager;

    // INTEGRATION: Use existing ShaderProgram for shader management
    std::unordered_map<std::string, std::unique_ptr<ShaderProgram>> shader_programs;

    // Mutex for thread-safe access
    mutable std::mutex mutex;

    // ID counters
    std::uint64_t next_model_id = 1;
    std::uint64_t next_texture_id = 1;
    std::uint64_t next_shader_id = 1;

    // Asset storage - wraps existing scene manager data
    std::unordered_map<std::uint64_t, std::unique_ptr<LoadedModel>> models;
    std::unordered_map<std::uint64_t, std::unique_ptr<LoadedTexture>> textures;
    std::unordered_map<std::uint64_t, std::unique_ptr<LoadedShader>> shaders;

    // Path to handle mapping
    std::unordered_map<std::string, ModelHandle> model_path_to_handle;
    std::unordered_map<std::string, AssetTextureHandle> texture_path_to_handle;
    std::unordered_map<std::string, AssetShaderHandle> shader_name_to_handle;

    // Model index to scene manager index mapping
    std::unordered_map<std::uint64_t, std::size_t> model_to_scene_idx;

    // Built-in meshes for ECS render pipeline
    std::unordered_map<std::string, GpuMesh> builtin_meshes;

    // Default shader
    AssetShaderHandle default_shader_handle;

    // Callbacks
    OnModelLoaded on_model_loaded_cb;
    OnModelReloaded on_model_reloaded_cb;
    OnTextureLoaded on_texture_loaded_cb;
    OnTextureReloaded on_texture_reloaded_cb;
    OnShaderLoaded on_shader_loaded_cb;
    OnShaderReloaded on_shader_reloaded_cb;
    OnAssetError on_error_cb;

    // Statistics
    std::size_t gpu_memory = 0;

    // =========================================================================
    // Helper Methods
    // =========================================================================

    std::filesystem::path resolve_path(const std::string& path) const {
        std::filesystem::path p(path);
        if (p.is_absolute()) {
            return p;
        }
        return asset_root / p;
    }

    void report_error(const std::string& path, const std::string& error) {
        spdlog::error("Asset error [{}]: {}", path, error);
        if (on_error_cb) {
            on_error_cb(path, error);
        }
    }

    Vertex make_vertex(float px, float py, float pz,
                       float nx, float ny, float nz,
                       float u, float v) const {
        Vertex vertex;
        vertex.position = {px, py, pz};
        vertex.normal = {nx, ny, nz};
        vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        vertex.uv0 = {u, v};
        vertex.uv1 = {u, v};
        vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
        return vertex;
    }

    MeshData create_sphere_mesh_data(int segments, int rings) const {
        MeshData mesh(PrimitiveTopology::TriangleList);
        mesh.reserve_vertices(static_cast<std::size_t>((segments + 1) * (rings + 1)));
        mesh.reserve_indices(static_cast<std::size_t>(segments * rings * 6));

        for (int y = 0; y <= rings; ++y) {
            for (int x = 0; x <= segments; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(segments);
                float v = static_cast<float>(y) / static_cast<float>(rings);

                float theta = u * 2.0f * std::numbers::pi_v<float>;
                float phi = v * std::numbers::pi_v<float>;

                float px = std::sin(phi) * std::cos(theta);
                float py = std::cos(phi);
                float pz = std::sin(phi) * std::sin(theta);

                auto normal = vec3_normalize({px, py, pz});
                mesh.vertices().push_back(make_vertex(
                    px * 0.5f, py * 0.5f, pz * 0.5f,
                    normal[0], normal[1], normal[2],
                    u, 1.0f - v));
            }
        }

        for (int y = 0; y < rings; ++y) {
            for (int x = 0; x < segments; ++x) {
                std::uint32_t a = static_cast<std::uint32_t>(y * (segments + 1) + x);
                std::uint32_t b = a + 1;
                std::uint32_t c = static_cast<std::uint32_t>((y + 1) * (segments + 1) + x);
                std::uint32_t d = c + 1;

                mesh.indices().push_back(a);
                mesh.indices().push_back(c);
                mesh.indices().push_back(b);

                mesh.indices().push_back(b);
                mesh.indices().push_back(c);
                mesh.indices().push_back(d);
            }
        }

        return mesh;
    }

    MeshData create_cube_mesh_data() const {
        MeshData mesh(PrimitiveTopology::TriangleList);

        const std::array<std::array<float, 3>, 6> normals = {{
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}
        }};

        const std::array<std::array<float, 3>, 8> corners = {{
            {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
            {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
            {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
            {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}
        }};

        const int face_indices[6][4] = {
            {4, 5, 6, 7},  // Front
            {1, 0, 3, 2},  // Back
            {3, 7, 6, 2},  // Top
            {0, 4, 5, 1},  // Bottom
            {5, 1, 2, 6},  // Right
            {0, 4, 7, 3}   // Left
        };

        const std::array<std::array<float, 2>, 4> uvs = {{
            {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
        }};

        for (int f = 0; f < 6; ++f) {
            std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices().size());
            const auto& normal = normals[f];

            for (int i = 0; i < 4; ++i) {
                const auto& pos = corners[face_indices[f][i]];
                const auto& uv = uvs[i];
                mesh.vertices().push_back(make_vertex(
                    pos[0], pos[1], pos[2],
                    normal[0], normal[1], normal[2],
                    uv[0], uv[1]));
            }

            mesh.indices().push_back(base + 0);
            mesh.indices().push_back(base + 1);
            mesh.indices().push_back(base + 2);
            mesh.indices().push_back(base + 0);
            mesh.indices().push_back(base + 2);
            mesh.indices().push_back(base + 3);
        }

        return mesh;
    }

    MeshData create_torus_mesh_data(float inner_radius, float outer_radius, int segments, int rings) const {
        MeshData mesh(PrimitiveTopology::TriangleList);
        mesh.reserve_vertices(static_cast<std::size_t>((rings + 1) * (segments + 1)));
        mesh.reserve_indices(static_cast<std::size_t>(rings * segments * 6));

        float tube_radius = (outer_radius - inner_radius) * 0.5f;
        float ring_radius = inner_radius + tube_radius;

        for (int i = 0; i <= rings; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(rings) * 2.0f * std::numbers::pi_v<float>;
            float cos_theta = std::cos(theta);
            float sin_theta = std::sin(theta);

            for (int j = 0; j <= segments; ++j) {
                float phi = static_cast<float>(j) / static_cast<float>(segments) * 2.0f * std::numbers::pi_v<float>;
                float cos_phi = std::cos(phi);
                float sin_phi = std::sin(phi);

                float x = (ring_radius + tube_radius * cos_phi) * cos_theta;
                float y = tube_radius * sin_phi;
                float z = (ring_radius + tube_radius * cos_phi) * sin_theta;

                float nx = cos_phi * cos_theta;
                float ny = sin_phi;
                float nz = cos_phi * sin_theta;
                auto normal = vec3_normalize({nx, ny, nz});

                mesh.vertices().push_back(make_vertex(
                    x, y, z,
                    normal[0], normal[1], normal[2],
                    static_cast<float>(i) / static_cast<float>(rings),
                    static_cast<float>(j) / static_cast<float>(segments)));
            }
        }

        for (int i = 0; i < rings; ++i) {
            for (int j = 0; j < segments; ++j) {
                std::uint32_t a = static_cast<std::uint32_t>(i * (segments + 1) + j);
                std::uint32_t b = a + 1;
                std::uint32_t c = static_cast<std::uint32_t>((i + 1) * (segments + 1) + j);
                std::uint32_t d = c + 1;

                mesh.indices().push_back(a);
                mesh.indices().push_back(c);
                mesh.indices().push_back(b);

                mesh.indices().push_back(b);
                mesh.indices().push_back(c);
                mesh.indices().push_back(d);
            }
        }

        return mesh;
    }

    MeshData create_plane_mesh_data(float size) const {
        MeshData mesh(PrimitiveTopology::TriangleList);
        float h = size * 0.5f;

        mesh.vertices().push_back(make_vertex(-h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f));
        mesh.vertices().push_back(make_vertex(h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f));
        mesh.vertices().push_back(make_vertex(h, 0.0f, h, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f));
        mesh.vertices().push_back(make_vertex(-h, 0.0f, h, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f));

        mesh.indices() = {0, 1, 2, 0, 2, 3};

        return mesh;
    }

    MeshData create_cylinder_mesh_data(float radius, float height, int segments) const {
        MeshData mesh(PrimitiveTopology::TriangleList);
        float half_h = height * 0.5f;

        for (int i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * std::numbers::pi_v<float>;
            float c = std::cos(theta);
            float s = std::sin(theta);

            mesh.vertices().push_back(make_vertex(c * radius, -half_h, s * radius, c, 0.0f, s,
                                                 static_cast<float>(i) / static_cast<float>(segments), 0.0f));
            mesh.vertices().push_back(make_vertex(c * radius, half_h, s * radius, c, 0.0f, s,
                                                 static_cast<float>(i) / static_cast<float>(segments), 1.0f));
        }

        for (int i = 0; i < segments; ++i) {
            std::uint32_t base = static_cast<std::uint32_t>(i * 2);
            mesh.indices().push_back(base);
            mesh.indices().push_back(base + 1);
            mesh.indices().push_back(base + 3);
            mesh.indices().push_back(base);
            mesh.indices().push_back(base + 3);
            mesh.indices().push_back(base + 2);
        }

        std::uint32_t top_center = static_cast<std::uint32_t>(mesh.vertices().size());
        mesh.vertices().push_back(make_vertex(0.0f, half_h, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f));

        for (int i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * std::numbers::pi_v<float>;
            float c = std::cos(theta);
            float s = std::sin(theta);
            mesh.vertices().push_back(make_vertex(c * radius, half_h, s * radius, 0.0f, 1.0f, 0.0f,
                                                 c * 0.5f + 0.5f, s * 0.5f + 0.5f));
        }

        for (int i = 0; i < segments; ++i) {
            mesh.indices().push_back(top_center);
            mesh.indices().push_back(top_center + 1 + static_cast<std::uint32_t>(i));
            mesh.indices().push_back(top_center + 2 + static_cast<std::uint32_t>(i));
        }

        std::uint32_t bottom_center = static_cast<std::uint32_t>(mesh.vertices().size());
        mesh.vertices().push_back(make_vertex(0.0f, -half_h, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f));

        for (int i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * std::numbers::pi_v<float>;
            float c = std::cos(theta);
            float s = std::sin(theta);
            mesh.vertices().push_back(make_vertex(c * radius, -half_h, s * radius, 0.0f, -1.0f, 0.0f,
                                                 c * 0.5f + 0.5f, s * 0.5f + 0.5f));
        }

        for (int i = 0; i < segments; ++i) {
            mesh.indices().push_back(bottom_center);
            mesh.indices().push_back(bottom_center + 2 + static_cast<std::uint32_t>(i));
            mesh.indices().push_back(bottom_center + 1 + static_cast<std::uint32_t>(i));
        }

        return mesh;
    }

    MeshData create_diamond_mesh_data() const {
        MeshData mesh(PrimitiveTopology::TriangleList);

        const std::array<float, 3> top = {0.0f, 0.5f, 0.0f};
        const std::array<float, 3> bottom = {0.0f, -0.5f, 0.0f};
        const std::array<std::array<float, 3>, 4> mid = {{
            {0.5f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.5f},
            {-0.5f, 0.0f, 0.0f},
            {0.0f, 0.0f, -0.5f}
        }};

        for (int i = 0; i < 4; ++i) {
            int next = (i + 1) % 4;
            auto normal = vec3_normalize(vec3_cross(vec3_sub(mid[next], top), vec3_sub(mid[i], top)));

            mesh.vertices().push_back(make_vertex(top[0], top[1], top[2],
                                                 normal[0], normal[1], normal[2], 0.5f, 1.0f));
            mesh.vertices().push_back(make_vertex(mid[i][0], mid[i][1], mid[i][2],
                                                 normal[0], normal[1], normal[2], 0.0f, 0.0f));
            mesh.vertices().push_back(make_vertex(mid[next][0], mid[next][1], mid[next][2],
                                                 normal[0], normal[1], normal[2], 1.0f, 0.0f));

            std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices().size()) - 3;
            mesh.indices().push_back(base);
            mesh.indices().push_back(base + 1);
            mesh.indices().push_back(base + 2);
        }

        for (int i = 0; i < 4; ++i) {
            int next = (i + 1) % 4;
            auto normal = vec3_normalize(vec3_cross(vec3_sub(mid[i], bottom), vec3_sub(mid[next], bottom)));

            mesh.vertices().push_back(make_vertex(bottom[0], bottom[1], bottom[2],
                                                 normal[0], normal[1], normal[2], 0.5f, 0.0f));
            mesh.vertices().push_back(make_vertex(mid[next][0], mid[next][1], mid[next][2],
                                                 normal[0], normal[1], normal[2], 1.0f, 1.0f));
            mesh.vertices().push_back(make_vertex(mid[i][0], mid[i][1], mid[i][2],
                                                 normal[0], normal[1], normal[2], 0.0f, 1.0f));

            std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices().size()) - 3;
            mesh.indices().push_back(base);
            mesh.indices().push_back(base + 1);
            mesh.indices().push_back(base + 2);
        }

        return mesh;
    }

    MeshData create_quad_mesh_data() const {
        MeshData mesh(PrimitiveTopology::TriangleList);

        mesh.vertices().push_back(make_vertex(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f));
        mesh.vertices().push_back(make_vertex(1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f));
        mesh.vertices().push_back(make_vertex(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f));
        mesh.vertices().push_back(make_vertex(-1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f));

        mesh.indices() = {0, 1, 2, 0, 2, 3};

        return mesh;
    }

    GpuMesh upload_mesh_with_bounds(const MeshData& mesh_data) {
        GpuMesh mesh = upload_mesh_data(mesh_data);
        if (!mesh.is_valid()) {
            return mesh;
        }

        std::array<float, 3> min_bounds = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        std::array<float, 3> max_bounds = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        for (const auto& vertex : mesh_data.vertices()) {
            update_bounds(min_bounds, max_bounds, vertex.position);
        }

        mesh.min_bounds = min_bounds;
        mesh.max_bounds = max_bounds;
        return mesh;
    }

    void create_builtin_meshes() {
        for (auto& [name, mesh] : builtin_meshes) {
            mesh.destroy();
        }
        builtin_meshes.clear();

        builtin_meshes.emplace("sphere", upload_mesh_with_bounds(create_sphere_mesh_data(32, 16)));
        builtin_meshes.emplace("cube", upload_mesh_with_bounds(create_cube_mesh_data()));
        builtin_meshes.emplace("torus", upload_mesh_with_bounds(create_torus_mesh_data(0.3f, 1.0f, 32, 16)));
        builtin_meshes.emplace("plane", upload_mesh_with_bounds(create_plane_mesh_data(10.0f)));
        builtin_meshes.emplace("cylinder", upload_mesh_with_bounds(create_cylinder_mesh_data(0.5f, 2.0f, 32)));
        builtin_meshes.emplace("diamond", upload_mesh_with_bounds(create_diamond_mesh_data()));
        builtin_meshes.emplace("quad", upload_mesh_with_bounds(create_quad_mesh_data()));

        spdlog::info("Created {} built-in meshes", builtin_meshes.size());
    }

    // =========================================================================
    // Model Loading - Uses Existing GltfSceneManager
    // =========================================================================

    std::optional<LoadedModel> convert_gltf_scene(const GltfScene& scene, const std::string& path) {
        ensure_gl_funcs();

        LoadedModel result;
        result.source_path = path;
        result.generation = 1;

        // Convert textures
        result.textures.reserve(scene.textures.size());
        for (const auto& gltf_tex : scene.textures) {
            GpuTexture gpu_tex;
            if (!gltf_tex.data.pixels.empty()) {
                glGenTextures(1, &gpu_tex.id);
                glBindTexture(GL_TEXTURE_2D, gpu_tex.id);

                GLenum format = gltf_tex.data.channels == 4 ? GL_RGBA : GL_RGB;
                GLenum internal_format = gltf_tex.data.channels == 4 ? GL_SRGB_ALPHA : GL_SRGB;

                glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                             gltf_tex.data.width, gltf_tex.data.height, 0,
                             format, GL_UNSIGNED_BYTE, gltf_tex.data.pixels.data());

                if (glGenerateMipmap_fn) {
                    glGenerateMipmap_fn(GL_TEXTURE_2D);
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                gpu_tex.width = gltf_tex.data.width;
                gpu_tex.height = gltf_tex.data.height;
                gpu_tex.channels = gltf_tex.data.channels;
                gpu_tex.is_srgb = true;
                gpu_tex.has_mipmaps = true;

                gpu_memory += gltf_tex.data.width * gltf_tex.data.height * gltf_tex.data.channels;
            }
            result.textures.push_back(gpu_tex);
        }

        // Convert materials - directly use GpuMaterial from GltfMaterial
        result.materials.reserve(scene.materials.size());
        for (const auto& gltf_mat : scene.materials) {
            result.materials.push_back(gltf_mat.gpu_material);
        }

        // Ensure default material
        if (result.materials.empty()) {
            result.materials.push_back(GpuMaterial::pbr_default());
        }

        // Convert meshes - upload MeshData from GltfPrimitive to GPU
        for (const auto& gltf_mesh : scene.meshes) {
            for (const auto& prim : gltf_mesh.primitives) {
                GpuMesh gpu_mesh = upload_mesh_data(prim.mesh_data);
                gpu_mesh.min_bounds = prim.min_bounds;
                gpu_mesh.max_bounds = prim.max_bounds;

                result.meshes.push_back(gpu_mesh);
                result.mesh_names.push_back(gltf_mesh.name);

                result.total_vertices += gpu_mesh.vertex_count;
                result.total_triangles += gpu_mesh.index_count / 3;
            }
        }

        // Convert nodes
        result.nodes.reserve(scene.nodes.size());
        for (const auto& gltf_node : scene.nodes) {
            LoadedModel::Node node;
            node.name = gltf_node.name;
            node.mesh_index = gltf_node.mesh_index;
            node.material_index = -1;  // Set per-primitive in mesh
            node.local_transform = gltf_node.local_transform.to_matrix();
            node.world_transform = gltf_node.world_matrix;
            node.children = gltf_node.children;
            node.parent = gltf_node.parent;
            result.nodes.push_back(node);
        }

        result.root_nodes = scene.root_nodes;
        result.min_bounds = scene.min_bounds;
        result.max_bounds = scene.max_bounds;

        spdlog::info("Converted model: {} ({} meshes, {} materials, {} verts, {} tris)",
                     path, result.meshes.size(), result.materials.size(),
                     result.total_vertices, result.total_triangles);

        return result;
    }

    GpuMesh upload_mesh_data(const MeshData& mesh_data) {
        ensure_gl_funcs();

        GpuMesh mesh;
        if (!glGenVertexArrays_fn) return mesh;

        const auto& vertices = mesh_data.vertices();
        const auto& indices = mesh_data.indices();

        if (vertices.empty()) return mesh;

        glGenVertexArrays_fn(1, &mesh.vao);
        glGenBuffers_fn(1, &mesh.vbo);
        glBindVertexArray_fn(mesh.vao);

        glBindBuffer_fn(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData_fn(GL_ARRAY_BUFFER,
                        vertices.size() * sizeof(Vertex),
                        vertices.data(), GL_STATIC_DRAW);

        if (!indices.empty()) {
            glGenBuffers_fn(1, &mesh.ebo);
            glBindBuffer_fn(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
            glBufferData_fn(GL_ELEMENT_ARRAY_BUFFER,
                           indices.size() * sizeof(std::uint32_t),
                           indices.data(), GL_STATIC_DRAW);
            mesh.index_count = static_cast<std::int32_t>(indices.size());
            mesh.has_indices = true;
        }

        mesh.vertex_count = static_cast<std::int32_t>(vertices.size());

        // Vertex attributes matching Vertex struct from mesh.hpp
        // Position (location 0)
        glEnableVertexAttribArray_fn(0);
        glVertexAttribPointer_fn(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 reinterpret_cast<void*>(offsetof(Vertex, position)));

        // Normal (location 1)
        glEnableVertexAttribArray_fn(1);
        glVertexAttribPointer_fn(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 reinterpret_cast<void*>(offsetof(Vertex, normal)));

        // UV0 (location 2)
        glEnableVertexAttribArray_fn(2);
        glVertexAttribPointer_fn(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 reinterpret_cast<void*>(offsetof(Vertex, uv0)));

        // Color (location 3)
        glEnableVertexAttribArray_fn(3);
        glVertexAttribPointer_fn(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 reinterpret_cast<void*>(offsetof(Vertex, color)));

        // Tangent (location 4)
        glEnableVertexAttribArray_fn(4);
        glVertexAttribPointer_fn(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 reinterpret_cast<void*>(offsetof(Vertex, tangent)));

        glBindVertexArray_fn(0);

        gpu_memory += vertices.size() * sizeof(Vertex) + indices.size() * sizeof(std::uint32_t);

        return mesh;
    }

    // =========================================================================
    // Shader Loading - Uses Existing ShaderProgram
    // =========================================================================

    std::optional<LoadedShader> load_shader_via_program(
        const std::string& name,
        const std::string& vertex_path,
        const std::string& fragment_path) {

        auto program = std::make_unique<ShaderProgram>();
        auto vert_full = resolve_path(vertex_path);
        auto frag_full = resolve_path(fragment_path);

        if (!program->load_from_files(vert_full, frag_full)) {
            report_error(name, "Failed to compile shader");
            return std::nullopt;
        }

        // Set up hot-reload callback
        program->on_reloaded = [this, name]() {
            spdlog::info("Shader hot-reloaded: {}", name);
            auto it = shader_name_to_handle.find(name);
            if (it != shader_name_to_handle.end() && on_shader_reloaded_cb) {
                on_shader_reloaded_cb(it->second, shaders[it->second.id].get());
            }
        };

        LoadedShader result;
        result.name = name;
        result.vertex_path = vertex_path;
        result.fragment_path = fragment_path;
        result.gpu_shader.program = program->id();
        result.gpu_shader.name = name;
        result.generation = 1;

        // Store the ShaderProgram for hot-reload management
        shader_programs[name] = std::move(program);

        std::error_code ec;
        result.vertex_mtime = std::filesystem::last_write_time(vert_full, ec);
        result.fragment_mtime = std::filesystem::last_write_time(frag_full, ec);

        spdlog::info("Loaded shader: {}", name);
        return result;
    }

    std::optional<LoadedShader> load_shader_from_source_internal(
        const std::string& name,
        const std::string& vertex_source,
        const std::string& fragment_source) {

        auto program = std::make_unique<ShaderProgram>();

        if (!program->load_from_source(vertex_source, fragment_source)) {
            report_error(name, "Failed to compile shader from source");
            return std::nullopt;
        }

        LoadedShader result;
        result.name = name;
        result.gpu_shader.program = program->id();
        result.gpu_shader.name = name;
        result.generation = 1;

        shader_programs[name] = std::move(program);

        spdlog::info("Loaded shader from source: {}", name);
        return result;
    }
};

// =============================================================================
// RenderAssetManager Public Interface
// =============================================================================

RenderAssetManager::RenderAssetManager() : m_impl(std::make_unique<Impl>()) {}
RenderAssetManager::~RenderAssetManager() { shutdown(); }

RenderAssetManager::RenderAssetManager(RenderAssetManager&&) noexcept = default;
RenderAssetManager& RenderAssetManager::operator=(RenderAssetManager&&) noexcept = default;

void_core::Result<void> RenderAssetManager::initialize(const std::filesystem::path& asset_root_path) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->asset_root = asset_root_path;
        m_impl->create_builtin_meshes();
    }

    if (!m_impl->default_shader_handle.is_valid()) {
        auto handle = load_shader_from_source("default_pbr", kDefaultPbrVertexShader, kDefaultPbrFragmentShader);
        if (handle.is_valid()) {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            m_impl->default_shader_handle = handle;
        } else {
            spdlog::warn("RenderAssetManager failed to create default PBR shader");
        }
    }

    spdlog::info("RenderAssetManager initialized: {}", asset_root_path.string());
    return void_core::Ok();
}

void RenderAssetManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    for (auto& [id, model] : m_impl->models) {
        if (model) model->destroy();
    }
    m_impl->models.clear();

    for (auto& [id, tex] : m_impl->textures) {
        if (tex) tex->destroy();
    }
    m_impl->textures.clear();

    for (auto& [id, shader] : m_impl->shaders) {
        if (shader) shader->destroy();
    }
    m_impl->shaders.clear();

    // Clear ShaderProgram instances
    m_impl->shader_programs.clear();

    // Clear scene manager
    m_impl->scene_manager.clear();

    for (auto& [name, mesh] : m_impl->builtin_meshes) {
        mesh.destroy();
    }
    m_impl->builtin_meshes.clear();

    m_impl->model_path_to_handle.clear();
    m_impl->texture_path_to_handle.clear();
    m_impl->shader_name_to_handle.clear();
    m_impl->model_to_scene_idx.clear();
    m_impl->gpu_memory = 0;
}

// =============================================================================
// Model Loading - Delegates to GltfSceneManager
// =============================================================================

ModelHandle RenderAssetManager::load_model(const std::string& path, const ModelLoadOptions& options) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check if already loaded
    auto it = m_impl->model_path_to_handle.find(path);
    if (it != m_impl->model_path_to_handle.end()) {
        return it->second;
    }

    auto extension = std::filesystem::path(path).extension().string();
    std::string extension_lower = extension;
    std::transform(extension_lower.begin(), extension_lower.end(), extension_lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (extension_lower != ".gltf" && extension_lower != ".glb") {
        m_impl->report_error(
            path,
            "Unsupported model format '" + extension + "'. Only .gltf/.glb are supported.");
        return ModelHandle::invalid();
    }

    // Use existing GltfSceneManager
    GltfLoader::LoadOptions gltf_opts;
    gltf_opts.load_textures = options.load_textures;
    gltf_opts.generate_tangents = options.generate_tangents;
    gltf_opts.flip_uvs = options.flip_uvs;
    gltf_opts.scale = options.scale;

    auto full_path = m_impl->resolve_path(path);
    auto scene_idx = m_impl->scene_manager.load(full_path.string(), gltf_opts);

    if (!scene_idx) {
        m_impl->report_error(path, "Failed to load model");
        return ModelHandle::invalid();
    }

    // Get loaded scene and convert to LoadedModel
    const GltfScene* scene = m_impl->scene_manager.get(*scene_idx);
    if (!scene) {
        m_impl->report_error(path, "Failed to get loaded scene");
        return ModelHandle::invalid();
    }

    auto loaded = m_impl->convert_gltf_scene(*scene, path);
    if (!loaded) {
        return ModelHandle::invalid();
    }

    // Create handle
    ModelHandle handle;
    handle.id = m_impl->next_model_id++;
    handle.generation = 1;

    m_impl->models[handle.id] = std::make_unique<LoadedModel>(std::move(*loaded));
    m_impl->model_path_to_handle[path] = handle;
    m_impl->model_to_scene_idx[handle.id] = *scene_idx;

    if (m_impl->on_model_loaded_cb) {
        m_impl->on_model_loaded_cb(handle, m_impl->models[handle.id].get());
    }

    return handle;
}

LoadedModel* RenderAssetManager::get_model(ModelHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->models.find(handle.id);
    if (it == m_impl->models.end()) return nullptr;
    if (it->second->generation != handle.generation) return nullptr;
    return it->second.get();
}

const LoadedModel* RenderAssetManager::get_model(ModelHandle handle) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->models.find(handle.id);
    if (it == m_impl->models.end()) return nullptr;
    if (it->second->generation != handle.generation) return nullptr;
    return it->second.get();
}

bool RenderAssetManager::is_model_loaded(ModelHandle handle) const {
    return get_model(handle) != nullptr;
}

void RenderAssetManager::unload_model(ModelHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->models.find(handle.id);
    if (it != m_impl->models.end()) {
        // Remove from scene manager
        auto scene_it = m_impl->model_to_scene_idx.find(handle.id);
        if (scene_it != m_impl->model_to_scene_idx.end()) {
            m_impl->scene_manager.remove(scene_it->second);
            m_impl->model_to_scene_idx.erase(scene_it);
        }

        // Remove path mapping
        for (auto pit = m_impl->model_path_to_handle.begin();
             pit != m_impl->model_path_to_handle.end(); ++pit) {
            if (pit->second.id == handle.id) {
                m_impl->model_path_to_handle.erase(pit);
                break;
            }
        }
        it->second->destroy();
        m_impl->models.erase(it);
    }
}

void RenderAssetManager::reload_model(ModelHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->models.find(handle.id);
    if (it == m_impl->models.end()) return;

    std::string path = it->second->source_path;

    // Destroy old model GPU resources
    it->second->destroy();

    // Reload via scene manager
    auto scene_it = m_impl->model_to_scene_idx.find(handle.id);
    if (scene_it != m_impl->model_to_scene_idx.end()) {
        m_impl->scene_manager.remove(scene_it->second);
    }

    auto full_path = m_impl->resolve_path(path);
    auto scene_idx = m_impl->scene_manager.load(full_path.string());

    if (scene_idx) {
        const GltfScene* scene = m_impl->scene_manager.get(*scene_idx);
        if (scene) {
            auto loaded = m_impl->convert_gltf_scene(*scene, path);
            if (loaded) {
                loaded->generation = it->second->generation + 1;
                *it->second = std::move(*loaded);
                m_impl->model_to_scene_idx[handle.id] = *scene_idx;

                if (m_impl->on_model_reloaded_cb) {
                    ModelHandle new_handle = handle;
                    new_handle.generation = it->second->generation;
                    m_impl->on_model_reloaded_cb(new_handle, it->second.get());
                }
            }
        }
    }
}

// =============================================================================
// Texture Loading
// =============================================================================

AssetTextureHandle RenderAssetManager::load_texture(
    const std::string& path,
    const AssetTextureLoadOptions& options) {

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check if already loaded
    auto it = m_impl->texture_path_to_handle.find(path);
    if (it != m_impl->texture_path_to_handle.end()) {
        return it->second;
    }

    auto full_path = m_impl->resolve_path(path);

    if (!std::filesystem::exists(full_path)) {
        m_impl->report_error(path, "Texture file not found");
        return AssetTextureHandle::invalid();
    }

    // Load via stb_image (would use existing texture loading infrastructure)
    // For now, return invalid - texture loading can be added via existing systems
    m_impl->report_error(path, "Standalone texture loading not yet integrated");
    return AssetTextureHandle::invalid();
}

LoadedTexture* RenderAssetManager::get_texture(AssetTextureHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->textures.find(handle.id);
    if (it == m_impl->textures.end()) return nullptr;
    return it->second.get();
}

const LoadedTexture* RenderAssetManager::get_texture(AssetTextureHandle handle) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->textures.find(handle.id);
    if (it == m_impl->textures.end()) return nullptr;
    return it->second.get();
}

bool RenderAssetManager::is_texture_loaded(AssetTextureHandle handle) const {
    return get_texture(handle) != nullptr;
}

void RenderAssetManager::unload_texture(AssetTextureHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->textures.find(handle.id);
    if (it != m_impl->textures.end()) {
        it->second->destroy();
        m_impl->textures.erase(it);
    }
}

void RenderAssetManager::reload_texture(AssetTextureHandle handle) {
    // Would reload via existing texture system
}

// =============================================================================
// Shader Loading - Delegates to ShaderProgram
// =============================================================================

AssetShaderHandle RenderAssetManager::load_shader(
    const std::string& name,
    const std::string& vertex_path,
    const std::string& fragment_path,
    const ShaderLoadOptions& options) {

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check if already loaded
    auto it = m_impl->shader_name_to_handle.find(name);
    if (it != m_impl->shader_name_to_handle.end()) {
        return it->second;
    }

    auto loaded = m_impl->load_shader_via_program(name, vertex_path, fragment_path);
    if (!loaded) {
        return AssetShaderHandle::invalid();
    }

    AssetShaderHandle handle;
    handle.id = m_impl->next_shader_id++;
    handle.generation = 1;

    m_impl->shaders[handle.id] = std::make_unique<LoadedShader>(std::move(*loaded));
    m_impl->shader_name_to_handle[name] = handle;

    if (m_impl->on_shader_loaded_cb) {
        m_impl->on_shader_loaded_cb(handle, m_impl->shaders[handle.id].get());
    }

    return handle;
}

AssetShaderHandle RenderAssetManager::load_shader_from_source(
    const std::string& name,
    const std::string& vertex_source,
    const std::string& fragment_source) {

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto loaded = m_impl->load_shader_from_source_internal(name, vertex_source, fragment_source);
    if (!loaded) {
        return AssetShaderHandle::invalid();
    }

    AssetShaderHandle handle;
    handle.id = m_impl->next_shader_id++;
    handle.generation = 1;

    m_impl->shaders[handle.id] = std::make_unique<LoadedShader>(std::move(*loaded));
    m_impl->shader_name_to_handle[name] = handle;

    return handle;
}

LoadedShader* RenderAssetManager::get_shader(AssetShaderHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->shaders.find(handle.id);
    if (it == m_impl->shaders.end()) return nullptr;
    return it->second.get();
}

const LoadedShader* RenderAssetManager::get_shader(AssetShaderHandle handle) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->shaders.find(handle.id);
    if (it == m_impl->shaders.end()) return nullptr;
    return it->second.get();
}

LoadedShader* RenderAssetManager::get_shader_by_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->shader_name_to_handle.find(name);
    if (it == m_impl->shader_name_to_handle.end()) return nullptr;
    return get_shader(it->second);
}

bool RenderAssetManager::is_shader_loaded(AssetShaderHandle handle) const {
    return get_shader(handle) != nullptr;
}

void RenderAssetManager::unload_shader(AssetShaderHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->shaders.find(handle.id);
    if (it != m_impl->shaders.end()) {
        // Remove ShaderProgram
        m_impl->shader_programs.erase(it->second->name);

        for (auto sit = m_impl->shader_name_to_handle.begin();
             sit != m_impl->shader_name_to_handle.end(); ++sit) {
            if (sit->second.id == handle.id) {
                m_impl->shader_name_to_handle.erase(sit);
                break;
            }
        }
        it->second->destroy();
        m_impl->shaders.erase(it);
    }
}

void RenderAssetManager::reload_shader(AssetShaderHandle handle) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->shaders.find(handle.id);
    if (it == m_impl->shaders.end()) return;

    const std::string& name = it->second->name;
    auto prog_it = m_impl->shader_programs.find(name);
    if (prog_it != m_impl->shader_programs.end()) {
        // Use ShaderProgram's reload (has hot-reload callback)
        if (prog_it->second->reload()) {
            it->second->gpu_shader.program = prog_it->second->id();
            it->second->generation++;
        }
    }
}

// =============================================================================
// Built-in Assets
// =============================================================================

GpuMesh* RenderAssetManager::get_builtin_mesh(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    auto it = m_impl->builtin_meshes.find(name);
    if (it != m_impl->builtin_meshes.end()) {
        return &it->second;
    }
    return nullptr;
}

LoadedShader* RenderAssetManager::get_default_shader() {
    if (!m_impl->default_shader_handle.is_valid()) {
        return nullptr;
    }
    return get_shader(m_impl->default_shader_handle);
}

// =============================================================================
// Hot-Reload - Integrates with GltfSceneManager and ShaderProgram
// =============================================================================

void RenderAssetManager::poll_hot_reload() {
    if (!m_impl->hot_reload_enabled) return;

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check GltfSceneManager for model changes
    m_impl->scene_manager.check_hot_reload();

    // Check for dirty scenes and update LoadedModel
    for (auto& [model_id, scene_idx] : m_impl->model_to_scene_idx) {
        if (m_impl->scene_manager.is_dirty(scene_idx)) {
            m_impl->scene_manager.clear_dirty(scene_idx);

            const GltfScene* scene = m_impl->scene_manager.get(scene_idx);
            if (scene) {
                auto it = m_impl->models.find(model_id);
                if (it != m_impl->models.end()) {
                    std::string path = it->second->source_path;

                    // Destroy old GPU resources
                    it->second->destroy();

                    // Convert reloaded scene
                    auto loaded = m_impl->convert_gltf_scene(*scene, path);
                    if (loaded) {
                        loaded->generation = it->second->generation + 1;
                        *it->second = std::move(*loaded);

                        spdlog::info("Hot-reloaded model: {}", path);

                        if (m_impl->on_model_reloaded_cb) {
                            ModelHandle handle;
                            handle.id = model_id;
                            handle.generation = it->second->generation;
                            m_impl->on_model_reloaded_cb(handle, it->second.get());
                        }
                    }
                }
            }
        }
    }

    // ShaderProgram handles its own hot-reload via its reload() method
    // which is triggered by the callback we set up
    for (auto& [name, program] : m_impl->shader_programs) {
        // ShaderProgram can poll itself if loaded from files
        // For explicit polling, we'd check file timestamps here
    }
}

void RenderAssetManager::set_hot_reload_enabled(bool enabled) {
    m_impl->hot_reload_enabled = enabled;
}

bool RenderAssetManager::is_hot_reload_enabled() const {
    return m_impl->hot_reload_enabled;
}

// =============================================================================
// Callbacks
// =============================================================================

void RenderAssetManager::on_model_loaded(OnModelLoaded callback) {
    m_impl->on_model_loaded_cb = std::move(callback);
}

void RenderAssetManager::on_model_reloaded(OnModelReloaded callback) {
    m_impl->on_model_reloaded_cb = std::move(callback);
}

void RenderAssetManager::on_texture_loaded(OnTextureLoaded callback) {
    m_impl->on_texture_loaded_cb = std::move(callback);
}

void RenderAssetManager::on_texture_reloaded(OnTextureReloaded callback) {
    m_impl->on_texture_reloaded_cb = std::move(callback);
}

void RenderAssetManager::on_shader_loaded(OnShaderLoaded callback) {
    m_impl->on_shader_loaded_cb = std::move(callback);
}

void RenderAssetManager::on_shader_reloaded(OnShaderReloaded callback) {
    m_impl->on_shader_reloaded_cb = std::move(callback);
}

void RenderAssetManager::on_error(OnAssetError callback) {
    m_impl->on_error_cb = std::move(callback);
}

// =============================================================================
// Statistics
// =============================================================================

std::size_t RenderAssetManager::gpu_memory_usage() const {
    return m_impl->gpu_memory;
}

std::size_t RenderAssetManager::model_count() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->models.size();
}

std::size_t RenderAssetManager::texture_count() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->textures.size();
}

std::size_t RenderAssetManager::shader_count() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->shaders.size();
}

// =============================================================================
// HotReloadable Interface
// =============================================================================

void_core::Result<void_core::HotReloadSnapshot> RenderAssetManager::snapshot() {
    void_core::HotReloadSnapshot snap;
    snap.type_id = std::type_index(typeid(RenderAssetManager));
    snap.type_name = "RenderAssetManager";
    snap.version = current_version();

    // Serialize asset paths for restoration
    // In a full implementation, this would serialize all loaded asset paths

    return snap;
}

void_core::Result<void> RenderAssetManager::restore(void_core::HotReloadSnapshot snapshot) {
    // Re-load all assets from stored paths
    // In a full implementation, this would reload all assets
    return void_core::Ok();
}

bool RenderAssetManager::is_compatible(const void_core::Version&) const {
    return true;
}

void_core::Version RenderAssetManager::current_version() const {
    return void_core::Version{1, 0, 0};
}

// =============================================================================
// Utility Functions
// =============================================================================

bool is_model_file(const std::string& path) {
    auto ext = get_extension(path);
    return ext == "gltf" || ext == "glb" || ext == "obj" || ext == "fbx";
}

bool is_texture_file(const std::string& path) {
    auto ext = get_extension(path);
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" ||
           ext == "bmp" || ext == "hdr" || ext == "exr";
}

bool is_shader_file(const std::string& path) {
    auto ext = get_extension(path);
    return ext == "vert" || ext == "frag" || ext == "glsl" || ext == "vs" || ext == "fs";
}

std::string get_extension(const std::string& path) {
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return "";
    std::string ext = path.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

} // namespace void_render
