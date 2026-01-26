/// @file gl_renderer.cpp
/// @brief OpenGL scene renderer implementation with hot-reload support

#include <void_engine/render/gl_renderer.hpp>

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// OpenGL function types and pointers
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef intptr_t GLintptr;

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8

// Extension function pointers
static PROC (*wglGetProcAddressPtr)(LPCSTR) = nullptr;

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
DECLARE_GL_FUNC(void, glDeleteBuffers, GLsizei n, const GLuint* buffers)
DECLARE_GL_FUNC(void, glEnableVertexAttribArray, GLuint index)
DECLARE_GL_FUNC(void, glDisableVertexAttribArray, GLuint index)
DECLARE_GL_FUNC(void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
DECLARE_GL_FUNC(GLint, glGetUniformLocation, GLuint program, const GLchar* name)
DECLARE_GL_FUNC(void, glUniform1i, GLint location, GLint v0)
DECLARE_GL_FUNC(void, glUniform1f, GLint location, GLfloat v0)
DECLARE_GL_FUNC(void, glUniform2fv, GLint location, GLsizei count, const GLfloat* value)
DECLARE_GL_FUNC(void, glUniform3fv, GLint location, GLsizei count, const GLfloat* value)
DECLARE_GL_FUNC(void, glUniform4fv, GLint location, GLsizei count, const GLfloat* value)
DECLARE_GL_FUNC(void, glUniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
DECLARE_GL_FUNC(void, glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)

static bool g_gl_loaded = false;

#define LOAD_GL_FUNC(name) \
    name##Ptr = (PFN##name##PROC)wglGetProcAddress(#name); \
    if (!name##Ptr) { spdlog::error("Failed to load " #name); return false; }

#else
// Linux/Mac - would use glXGetProcAddress or similar
static bool g_gl_loaded = false;
#endif

namespace void_render {

// =============================================================================
// OpenGL Function Loading
// =============================================================================

bool load_opengl_functions() {
    if (g_gl_loaded) return true;

#ifdef _WIN32
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
    LOAD_GL_FUNC(glDeleteBuffers);
    LOAD_GL_FUNC(glEnableVertexAttribArray);
    LOAD_GL_FUNC(glDisableVertexAttribArray);
    LOAD_GL_FUNC(glVertexAttribPointer);
    LOAD_GL_FUNC(glGetUniformLocation);
    LOAD_GL_FUNC(glUniform1i);
    LOAD_GL_FUNC(glUniform1f);
    LOAD_GL_FUNC(glUniform2fv);
    LOAD_GL_FUNC(glUniform3fv);
    LOAD_GL_FUNC(glUniform4fv);
    LOAD_GL_FUNC(glUniformMatrix3fv);
    LOAD_GL_FUNC(glUniformMatrix4fv);
#endif

    g_gl_loaded = true;
    spdlog::info("OpenGL functions loaded successfully");
    return true;
}

bool is_opengl_loaded() { return g_gl_loaded; }

// Wrapper macros for calling GL functions
#define GL_CALL(func, ...) func##Ptr(__VA_ARGS__)

// =============================================================================
// GpuMesh
// =============================================================================

void GpuMesh::destroy() {
    if (vao) { GL_CALL(glDeleteVertexArrays, 1, &vao); vao = 0; }
    if (vbo) { GL_CALL(glDeleteBuffers, 1, &vbo); vbo = 0; }
    if (ebo) { GL_CALL(glDeleteBuffers, 1, &ebo); ebo = 0; }
    index_count = 0;
    vertex_count = 0;
}

// =============================================================================
// ShaderProgram
// =============================================================================

ShaderProgram::~ShaderProgram() {
    if (m_program) {
        GL_CALL(glDeleteProgram, m_program);
    }
}

GLuint ShaderProgram::compile_shader(GLenum type, const std::string& source) {
    GLuint shader = GL_CALL(glCreateShader, type);
    const char* src = source.c_str();
    GL_CALL(glShaderSource, shader, 1, &src, nullptr);
    GL_CALL(glCompileShader, shader);

    GLint success;
    GL_CALL(glGetShaderiv, shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[1024];
        GL_CALL(glGetShaderInfoLog, shader, sizeof(info_log), nullptr, info_log);
        spdlog::error("Shader compilation failed: {}", info_log);
        GL_CALL(glDeleteShader, shader);
        return 0;
    }

    return shader;
}

bool ShaderProgram::link_program(GLuint vertex, GLuint fragment) {
    m_program = GL_CALL(glCreateProgram);
    GL_CALL(glAttachShader, m_program, vertex);
    GL_CALL(glAttachShader, m_program, fragment);
    GL_CALL(glLinkProgram, m_program);

    GLint success;
    GL_CALL(glGetProgramiv, m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[1024];
        GL_CALL(glGetProgramInfoLog, m_program, sizeof(info_log), nullptr, info_log);
        spdlog::error("Shader linking failed: {}", info_log);
        GL_CALL(glDeleteProgram, m_program);
        m_program = 0;
        return false;
    }

    GL_CALL(glDeleteShader, vertex);
    GL_CALL(glDeleteShader, fragment);

    m_uniform_cache.clear();
    return true;
}

bool ShaderProgram::load_from_source(const std::string& vertex_src, const std::string& fragment_src) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (!vertex) return false;

    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (!fragment) {
        GL_CALL(glDeleteShader, vertex);
        return false;
    }

    return link_program(vertex, fragment);
}

bool ShaderProgram::load_from_files(const std::filesystem::path& vertex_path,
                                    const std::filesystem::path& fragment_path) {
    m_vertex_path = vertex_path;
    m_fragment_path = fragment_path;

    // Read vertex shader
    std::ifstream vfile(vertex_path);
    if (!vfile) {
        spdlog::error("Cannot open vertex shader: {}", vertex_path.string());
        return false;
    }
    std::stringstream vss;
    vss << vfile.rdbuf();

    // Read fragment shader
    std::ifstream ffile(fragment_path);
    if (!ffile) {
        spdlog::error("Cannot open fragment shader: {}", fragment_path.string());
        return false;
    }
    std::stringstream fss;
    fss << ffile.rdbuf();

    // Track modification times for hot-reload
    std::error_code ec;
    m_vertex_mtime = std::filesystem::last_write_time(vertex_path, ec);
    m_fragment_mtime = std::filesystem::last_write_time(fragment_path, ec);

    return load_from_source(vss.str(), fss.str());
}

bool ShaderProgram::reload() {
    if (m_vertex_path.empty() || m_fragment_path.empty()) {
        return false;
    }

    // Delete old program
    if (m_program) {
        GL_CALL(glDeleteProgram, m_program);
        m_program = 0;
    }

    bool success = load_from_files(m_vertex_path, m_fragment_path);
    if (success) {
        m_version = void_core::Version{
            m_version.major,
            m_version.minor,
            static_cast<std::uint16_t>(m_version.patch + 1)
        };
        if (on_reloaded) {
            on_reloaded();
        }
        spdlog::info("Shader hot-reloaded: {} + {}",
                     m_vertex_path.filename().string(),
                     m_fragment_path.filename().string());
    }
    return success;
}

void ShaderProgram::use() const {
    GL_CALL(glUseProgram, m_program);
}

GLint ShaderProgram::get_uniform_location(const std::string& name) const {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }
    GLint loc = GL_CALL(glGetUniformLocation, m_program, name.c_str());
    m_uniform_cache[name] = loc;
    return loc;
}

void ShaderProgram::set_bool(const std::string& name, bool value) const {
    GL_CALL(glUniform1i, get_uniform_location(name), (int)value);
}

void ShaderProgram::set_int(const std::string& name, int value) const {
    GL_CALL(glUniform1i, get_uniform_location(name), value);
}

void ShaderProgram::set_float(const std::string& name, float value) const {
    GL_CALL(glUniform1f, get_uniform_location(name), value);
}

void ShaderProgram::set_vec2(const std::string& name, const glm::vec2& value) const {
    GL_CALL(glUniform2fv, get_uniform_location(name), 1, glm::value_ptr(value));
}

void ShaderProgram::set_vec3(const std::string& name, const glm::vec3& value) const {
    GL_CALL(glUniform3fv, get_uniform_location(name), 1, glm::value_ptr(value));
}

void ShaderProgram::set_vec4(const std::string& name, const glm::vec4& value) const {
    GL_CALL(glUniform4fv, get_uniform_location(name), 1, glm::value_ptr(value));
}

void ShaderProgram::set_mat3(const std::string& name, const glm::mat3& value) const {
    GL_CALL(glUniformMatrix3fv, get_uniform_location(name), 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderProgram::set_mat4(const std::string& name, const glm::mat4& value) const {
    GL_CALL(glUniformMatrix4fv, get_uniform_location(name), 1, GL_FALSE, glm::value_ptr(value));
}

void_core::Result<void_core::HotReloadSnapshot> ShaderProgram::snapshot() {
    void_core::HotReloadSnapshot snap;
    snap.type_id = std::type_index(typeid(ShaderProgram));
    snap.type_name = "ShaderProgram";
    snap.version = m_version;
    // Store paths for reload
    std::string path_str = m_vertex_path.string();
    snap.data = std::vector<std::uint8_t>(path_str.begin(), path_str.end());
    return snap;
}

void_core::Result<void> ShaderProgram::restore(void_core::HotReloadSnapshot) {
    return reload() ? void_core::Ok() : void_core::Err(void_core::Error("Shader reload failed"));
}

bool ShaderProgram::is_compatible(const void_core::Version&) const {
    return true;  // Shaders are always compatible
}

void_core::Version ShaderProgram::current_version() const {
    return m_version;
}

// =============================================================================
// Camera
// =============================================================================

glm::mat4 GlCamera::view_matrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 GlCamera::projection_matrix() const {
    if (is_perspective) {
        return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
    } else {
        float half = ortho_size * 0.5f;
        return glm::ortho(-half * aspect, half * aspect, -half, half, near_plane, far_plane);
    }
}

glm::mat4 GlCamera::view_projection() const {
    return projection_matrix() * view_matrix();
}

void GlCamera::orbit(float delta_yaw, float delta_pitch) {
    m_yaw += delta_yaw * 0.01f;
    m_pitch = glm::clamp(m_pitch + delta_pitch * 0.01f, -1.5f, 1.5f);

    // Update position based on orbit angles
    float x = m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    float y = m_distance * std::sin(m_pitch);
    float z = m_distance * std::cos(m_pitch) * std::cos(m_yaw);

    position = target + glm::vec3(x, y, z);
}

void GlCamera::zoom(float delta) {
    m_distance = glm::clamp(m_distance - delta * 0.5f, 1.0f, 100.0f);
    orbit(0, 0);  // Update position
}

void GlCamera::pan(float delta_x, float delta_y) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 cam_up = glm::cross(right, forward);

    target += right * delta_x * 0.01f + cam_up * delta_y * 0.01f;
    orbit(0, 0);  // Update position
}

// =============================================================================
// SceneRenderer
// =============================================================================

SceneRenderer::SceneRenderer() = default;

SceneRenderer::~SceneRenderer() {
    shutdown();
}

bool SceneRenderer::initialize(GLFWwindow* window) {
    m_window = window;

    if (!load_opengl_functions()) {
        spdlog::error("Failed to load OpenGL functions");
        return false;
    }

    glfwGetFramebufferSize(window, &m_width, &m_height);
    m_camera.aspect = static_cast<float>(m_width) / static_cast<float>(m_height);

    // Create built-in meshes
    create_builtin_meshes();

    // Create shaders
    if (!create_shaders()) {
        spdlog::error("Failed to create shaders");
        return false;
    }

    // Setup initial OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    spdlog::info("SceneRenderer initialized: {}x{}", m_width, m_height);
    return true;
}

void SceneRenderer::shutdown() {
    for (auto& [name, mesh] : m_meshes) {
        mesh.destroy();
    }
    m_meshes.clear();

    m_pbr_shader.reset();
    m_grid_shader.reset();
}

void SceneRenderer::create_builtin_meshes() {
    m_meshes["sphere"] = create_sphere_mesh(32, 16);
    m_meshes["cube"] = create_cube_mesh();
    m_meshes["torus"] = create_torus_mesh();
    m_meshes["plane"] = create_plane_mesh();
    m_meshes["cylinder"] = create_cylinder_mesh();
    m_meshes["diamond"] = create_diamond_mesh();
    m_meshes["quad"] = create_quad_mesh();

    spdlog::info("Created {} built-in meshes", m_meshes.size());
}

// =============================================================================
// Mesh Generation
// =============================================================================

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
};

static GpuMesh upload_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    GpuMesh mesh;

    GL_CALL(glGenVertexArrays, 1, &mesh.vao);
    GL_CALL(glGenBuffers, 1, &mesh.vbo);
    GL_CALL(glBindVertexArray, mesh.vao);

    GL_CALL(glBindBuffer, GL_ARRAY_BUFFER, mesh.vbo);
    GL_CALL(glBufferData, GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    if (!indices.empty()) {
        GL_CALL(glGenBuffers, 1, &mesh.ebo);
        GL_CALL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        GL_CALL(glBufferData, GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
        mesh.index_count = static_cast<GLsizei>(indices.size());
        mesh.has_indices = true;
    }

    mesh.vertex_count = static_cast<GLsizei>(vertices.size());

    // Vertex attributes
    // Position
    GL_CALL(glEnableVertexAttribArray, 0);
    GL_CALL(glVertexAttribPointer, 0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    // Normal
    GL_CALL(glEnableVertexAttribArray, 1);
    GL_CALL(glVertexAttribPointer, 1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    // UV
    GL_CALL(glEnableVertexAttribArray, 2);
    GL_CALL(glVertexAttribPointer, 2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    // Color
    GL_CALL(glEnableVertexAttribArray, 3);
    GL_CALL(glVertexAttribPointer, 3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    GL_CALL(glBindVertexArray, 0);

    return mesh;
}

GpuMesh SceneRenderer::create_sphere_mesh(int segments, int rings) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (int y = 0; y <= rings; ++y) {
        for (int x = 0; x <= segments; ++x) {
            float u = static_cast<float>(x) / segments;
            float v = static_cast<float>(y) / rings;

            float theta = u * 2.0f * std::numbers::pi_v<float>;
            float phi = v * std::numbers::pi_v<float>;

            float px = std::sin(phi) * std::cos(theta);
            float py = std::cos(phi);
            float pz = std::sin(phi) * std::sin(theta);

            Vertex vert;
            vert.position = glm::vec3(px, py, pz) * 0.5f;
            vert.normal = glm::normalize(glm::vec3(px, py, pz));
            vert.uv = glm::vec2(u, 1.0f - v);
            vert.color = glm::vec3(1.0f);
            vertices.push_back(vert);
        }
    }

    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            uint32_t a = y * (segments + 1) + x;
            uint32_t b = a + 1;
            uint32_t c = (y + 1) * (segments + 1) + x;
            uint32_t d = c + 1;

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    return upload_mesh(vertices, indices);
}

GpuMesh SceneRenderer::create_cube_mesh() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Face normals
    const glm::vec3 normals[] = {
        { 0,  0,  1}, { 0,  0, -1},
        { 0,  1,  0}, { 0, -1,  0},
        { 1,  0,  0}, {-1,  0,  0}
    };

    // Face vertices
    const glm::vec3 corners[] = {
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}
    };

    const int face_indices[6][4] = {
        {4, 5, 6, 7},  // Front (+Z)
        {1, 0, 3, 2},  // Back (-Z)
        {3, 7, 6, 2},  // Top (+Y)
        {0, 4, 5, 1},  // Bottom (-Y)
        {5, 1, 2, 6},  // Right (+X)
        {0, 4, 7, 3}   // Left (-X)
    };

    const glm::vec2 uvs[] = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t base = static_cast<uint32_t>(vertices.size());

        for (int i = 0; i < 4; ++i) {
            Vertex v;
            v.position = corners[face_indices[f][i]];
            v.normal = normals[f];
            v.uv = uvs[i];
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }

        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return upload_mesh(vertices, indices);
}

GpuMesh SceneRenderer::create_torus_mesh(float inner_radius, float outer_radius, int segments, int rings) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float tube_radius = (outer_radius - inner_radius) * 0.5f;
    float ring_radius = inner_radius + tube_radius;

    for (int i = 0; i <= rings; ++i) {
        float theta = static_cast<float>(i) / rings * 2.0f * std::numbers::pi_v<float>;
        float cos_theta = std::cos(theta);
        float sin_theta = std::sin(theta);

        for (int j = 0; j <= segments; ++j) {
            float phi = static_cast<float>(j) / segments * 2.0f * std::numbers::pi_v<float>;
            float cos_phi = std::cos(phi);
            float sin_phi = std::sin(phi);

            float x = (ring_radius + tube_radius * cos_phi) * cos_theta;
            float y = tube_radius * sin_phi;
            float z = (ring_radius + tube_radius * cos_phi) * sin_theta;

            float nx = cos_phi * cos_theta;
            float ny = sin_phi;
            float nz = cos_phi * sin_theta;

            Vertex v;
            v.position = glm::vec3(x, y, z);
            v.normal = glm::normalize(glm::vec3(nx, ny, nz));
            v.uv = glm::vec2(static_cast<float>(i) / rings, static_cast<float>(j) / segments);
            v.color = glm::vec3(1.0f);
            vertices.push_back(v);
        }
    }

    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            uint32_t a = i * (segments + 1) + j;
            uint32_t b = a + 1;
            uint32_t c = (i + 1) * (segments + 1) + j;
            uint32_t d = c + 1;

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    return upload_mesh(vertices, indices);
}

GpuMesh SceneRenderer::create_plane_mesh(float size) {
    float h = size * 0.5f;

    std::vector<Vertex> vertices = {
        {{-h, 0, -h}, {0, 1, 0}, {0, 0}, {1, 1, 1}},
        {{ h, 0, -h}, {0, 1, 0}, {1, 0}, {1, 1, 1}},
        {{ h, 0,  h}, {0, 1, 0}, {1, 1}, {1, 1, 1}},
        {{-h, 0,  h}, {0, 1, 0}, {0, 1}, {1, 1, 1}},
    };

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    return upload_mesh(vertices, indices);
}

GpuMesh SceneRenderer::create_cylinder_mesh(float radius, float height, int segments) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float half_h = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= segments; ++i) {
        float theta = static_cast<float>(i) / segments * 2.0f * std::numbers::pi_v<float>;
        float c = std::cos(theta);
        float s = std::sin(theta);

        Vertex top, bottom;
        top.position = glm::vec3(c * radius, half_h, s * radius);
        top.normal = glm::vec3(c, 0, s);
        top.uv = glm::vec2(static_cast<float>(i) / segments, 1);
        top.color = glm::vec3(1);

        bottom.position = glm::vec3(c * radius, -half_h, s * radius);
        bottom.normal = glm::vec3(c, 0, s);
        bottom.uv = glm::vec2(static_cast<float>(i) / segments, 0);
        bottom.color = glm::vec3(1);

        vertices.push_back(bottom);
        vertices.push_back(top);
    }

    // Side indices
    for (int i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    // Top cap
    uint32_t top_center = static_cast<uint32_t>(vertices.size());
    vertices.push_back({{0, half_h, 0}, {0, 1, 0}, {0.5f, 0.5f}, {1, 1, 1}});

    for (int i = 0; i <= segments; ++i) {
        float theta = static_cast<float>(i) / segments * 2.0f * std::numbers::pi_v<float>;
        float c = std::cos(theta);
        float s = std::sin(theta);
        vertices.push_back({{c * radius, half_h, s * radius}, {0, 1, 0}, {c * 0.5f + 0.5f, s * 0.5f + 0.5f}, {1, 1, 1}});
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(top_center);
        indices.push_back(top_center + 1 + i);
        indices.push_back(top_center + 2 + i);
    }

    // Bottom cap
    uint32_t bottom_center = static_cast<uint32_t>(vertices.size());
    vertices.push_back({{0, -half_h, 0}, {0, -1, 0}, {0.5f, 0.5f}, {1, 1, 1}});

    for (int i = 0; i <= segments; ++i) {
        float theta = static_cast<float>(i) / segments * 2.0f * std::numbers::pi_v<float>;
        float c = std::cos(theta);
        float s = std::sin(theta);
        vertices.push_back({{c * radius, -half_h, s * radius}, {0, -1, 0}, {c * 0.5f + 0.5f, s * 0.5f + 0.5f}, {1, 1, 1}});
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(bottom_center);
        indices.push_back(bottom_center + 2 + i);
        indices.push_back(bottom_center + 1 + i);
    }

    return upload_mesh(vertices, indices);
}

GpuMesh SceneRenderer::create_diamond_mesh() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec3 top(0, 0.5f, 0);
    glm::vec3 bottom(0, -0.5f, 0);
    glm::vec3 mid[4] = {
        { 0.5f, 0,  0.0f},
        { 0.0f, 0,  0.5f},
        {-0.5f, 0,  0.0f},
        { 0.0f, 0, -0.5f}
    };

    // Top pyramid
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        glm::vec3 normal = glm::normalize(glm::cross(mid[next] - top, mid[i] - top));

        vertices.push_back({top, normal, {0.5f, 1}, {1, 1, 1}});
        vertices.push_back({mid[i], normal, {0, 0}, {1, 1, 1}});
        vertices.push_back({mid[next], normal, {1, 0}, {1, 1, 1}});

        uint32_t base = static_cast<uint32_t>(vertices.size()) - 3;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    // Bottom pyramid
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        glm::vec3 normal = glm::normalize(glm::cross(mid[i] - bottom, mid[next] - bottom));

        vertices.push_back({bottom, normal, {0.5f, 0}, {1, 1, 1}});
        vertices.push_back({mid[next], normal, {1, 1}, {1, 1, 1}});
        vertices.push_back({mid[i], normal, {0, 1}, {1, 1, 1}});

        uint32_t base = static_cast<uint32_t>(vertices.size()) - 3;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    return upload_mesh(vertices, indices);
}

GpuMesh SceneRenderer::create_quad_mesh() {
    std::vector<Vertex> vertices = {
        {{-1, -1, 0}, {0, 0, 1}, {0, 0}, {1, 1, 1}},
        {{ 1, -1, 0}, {0, 0, 1}, {1, 0}, {1, 1, 1}},
        {{ 1,  1, 0}, {0, 0, 1}, {1, 1}, {1, 1, 1}},
        {{-1,  1, 0}, {0, 0, 1}, {0, 1}, {1, 1, 1}},
    };

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    return upload_mesh(vertices, indices);
}

// =============================================================================
// Shaders
// =============================================================================

// PBR Vertex Shader
static const char* PBR_VERTEX_SHADER = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aColor;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 VertexColor;

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

// PBR Fragment Shader (simplified but functional)
static const char* PBR_FRAGMENT_SHADER = R"(
#version 330 core

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 VertexColor;

out vec4 FragColor;

// Material
uniform vec3 albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform vec3 emissive;
uniform float emissiveStrength;

// Camera
uniform vec3 camPos;

// Lights
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform float lightIntensities[4];
uniform int numLights;

// Ambient
uniform vec3 ambientColor;
uniform float ambientIntensity;

const float PI = 3.14159265359;

// PBR functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
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

    // Reflectance equation
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < numLights; ++i) {
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient lighting
    vec3 ambient = ambientColor * ambientIntensity * albedo * ao;

    // Emissive
    vec3 emission = emissive * emissiveStrength;

    vec3 color = ambient + Lo + emission;

    // HDR tonemapping (ACES approximation)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}
)";

// Grid shader for ground visualization
static const char* GRID_VERTEX_SHADER = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 view;
uniform mat4 projection;

void main() {
    WorldPos = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
)";

static const char* GRID_FRAGMENT_SHADER = R"(
#version 330 core

in vec3 WorldPos;
out vec4 FragColor;

void main() {
    // Grid pattern
    vec2 coord = WorldPos.xz;
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);
    float alpha = 1.0 - min(line, 1.0);

    // Fade with distance
    float dist = length(WorldPos.xz);
    alpha *= exp(-dist * 0.02);

    FragColor = vec4(vec3(0.5), alpha * 0.5);
}
)";

bool SceneRenderer::create_shaders() {
    m_pbr_shader = std::make_unique<ShaderProgram>();
    if (!m_pbr_shader->load_from_source(PBR_VERTEX_SHADER, PBR_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create PBR shader");
        return false;
    }

    m_grid_shader = std::make_unique<ShaderProgram>();
    if (!m_grid_shader->load_from_source(GRID_VERTEX_SHADER, GRID_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create grid shader");
        return false;
    }

    spdlog::info("Shaders created successfully");
    return true;
}

void SceneRenderer::check_shader_reload() {
    // Check for shader file changes (if loaded from files)
    // For now, shaders are embedded - implement file-based hot-reload later
}

void SceneRenderer::reload_shaders() {
    if (m_pbr_shader) m_pbr_shader->reload();
    if (m_grid_shader) m_grid_shader->reload();
}

// =============================================================================
// Scene Loading
// =============================================================================

void SceneRenderer::load_scene(const void_scene::SceneData& scene_data) {
    m_entities.clear();
    m_lights.clear();

    // Load cameras
    for (const auto& cam_data : scene_data.cameras) {
        if (cam_data.active) {
            convert_camera(cam_data);
        }
    }

    // Load lights
    for (const auto& light_data : scene_data.lights) {
        convert_light(light_data);
    }

    // Load entities
    for (const auto& entity_data : scene_data.entities) {
        convert_entity(entity_data);
    }

    spdlog::info("Scene loaded: {} entities, {} lights", m_entities.size(), m_lights.size());
}

void SceneRenderer::convert_camera(const void_scene::CameraData& data) {
    m_camera.position = glm::vec3(
        data.transform.position[0],
        data.transform.position[1],
        data.transform.position[2]
    );
    m_camera.target = glm::vec3(
        data.transform.target[0],
        data.transform.target[1],
        data.transform.target[2]
    );
    m_camera.up = glm::vec3(
        data.transform.up[0],
        data.transform.up[1],
        data.transform.up[2]
    );

    if (data.type == void_scene::CameraType::Perspective) {
        m_camera.is_perspective = true;
        m_camera.fov = data.perspective.fov;
        m_camera.near_plane = data.perspective.near_plane;
        m_camera.far_plane = data.perspective.far_plane;
    } else {
        m_camera.is_perspective = false;
        m_camera.near_plane = data.orthographic.near_plane;
        m_camera.far_plane = data.orthographic.far_plane;
    }
}

void SceneRenderer::convert_light(const void_scene::LightData& data) {
    Light light;

    switch (data.type) {
        case void_scene::LightType::Directional:
            light.type = LightType::Directional;
            light.direction = glm::vec3(
                data.directional.direction[0],
                data.directional.direction[1],
                data.directional.direction[2]
            );
            light.color = glm::vec3(
                data.directional.color[0],
                data.directional.color[1],
                data.directional.color[2]
            );
            light.intensity = data.directional.intensity;
            light.cast_shadows = data.directional.cast_shadows;
            light.position = -light.direction * 100.0f;  // Far away for directional
            break;

        case void_scene::LightType::Point:
            light.type = LightType::Point;
            light.position = glm::vec3(
                data.point.position[0],
                data.point.position[1],
                data.point.position[2]
            );
            light.color = glm::vec3(
                data.point.color[0],
                data.point.color[1],
                data.point.color[2]
            );
            light.intensity = data.point.intensity;
            light.range = data.point.range;
            light.cast_shadows = data.point.cast_shadows;
            break;

        case void_scene::LightType::Spot:
            light.type = LightType::Spot;
            light.position = glm::vec3(
                data.spot.position[0],
                data.spot.position[1],
                data.spot.position[2]
            );
            light.direction = glm::vec3(
                data.spot.direction[0],
                data.spot.direction[1],
                data.spot.direction[2]
            );
            light.color = glm::vec3(
                data.spot.color[0],
                data.spot.color[1],
                data.spot.color[2]
            );
            light.intensity = data.spot.intensity;
            light.range = data.spot.range;
            light.inner_cone = data.spot.inner_angle;
            light.outer_cone = data.spot.outer_angle;
            light.cast_shadows = data.spot.cast_shadows;
            break;
    }

    m_lights.push_back(light);
}

void SceneRenderer::convert_entity(const void_scene::EntityData& data) {
    RenderEntity entity;
    entity.name = data.name;
    entity.mesh_type = data.mesh;
    entity.base_transform = convert_transform(data.transform);
    entity.transform = entity.base_transform;
    entity.visible = data.visible;

    // Convert material if present
    if (data.material) {
        entity.material = convert_material(*data.material);
    } else {
        // Default material
        entity.material.albedo = glm::vec3(0.8f, 0.8f, 0.8f);
        entity.material.metallic = 0.0f;
        entity.material.roughness = 0.5f;
    }

    // Store animation data
    if (data.animation) {
        entity.animation = data.animation;
        entity.animation_state = AnimationState{};
    }

    m_entities.push_back(entity);
}

Material SceneRenderer::convert_material(const void_scene::MaterialData& data) {
    Material mat;

    // Albedo (base color)
    if (data.albedo.has_color()) {
        const auto& c = data.albedo.color.value();
        mat.albedo = glm::vec3(c[0], c[1], c[2]);
        if (c.size() > 3) mat.alpha = c[3];
    } else {
        mat.albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    }

    // Metallic
    if (data.metallic.has_value()) {
        mat.metallic = data.metallic.value.value();
    } else {
        mat.metallic = 0.0f;
    }

    // Roughness
    if (data.roughness.has_value()) {
        mat.roughness = data.roughness.value.value();
    } else {
        mat.roughness = 0.5f;
    }

    // Emissive
    if (data.emissive) {
        const auto& e = *data.emissive;
        mat.emissive = glm::vec3(e[0], e[1], e[2]);
        // Assume emissive strength from magnitude
        float emissive_max = std::max({e[0], e[1], e[2]});
        if (emissive_max > 1.0f) {
            mat.emissive_strength = emissive_max;
            mat.emissive /= emissive_max;
        } else {
            mat.emissive_strength = emissive_max > 0.0f ? 1.0f : 0.0f;
        }
    }

    mat.ao = 1.0f;

    return mat;
}

glm::mat4 SceneRenderer::convert_transform(const void_scene::TransformData& data) {
    glm::mat4 model(1.0f);

    // Translation
    model = glm::translate(model, glm::vec3(data.position[0], data.position[1], data.position[2]));

    // Rotation (Euler angles in degrees -> convert to radians)
    glm::vec3 euler_rad = glm::radians(glm::vec3(data.rotation[0], data.rotation[1], data.rotation[2]));
    glm::mat4 rotation = glm::mat4_cast(glm::quat(euler_rad));
    model *= rotation;

    // Scale (can be uniform or non-uniform)
    auto scale = data.scale_vec3();
    model = glm::scale(model, glm::vec3(scale[0], scale[1], scale[2]));

    return model;
}

// =============================================================================
// Rendering
// =============================================================================

void SceneRenderer::render() {
    auto start = std::chrono::steady_clock::now();

    m_stats.draw_calls = 0;
    m_stats.triangles = 0;
    m_stats.entities = static_cast<int>(m_entities.size());

    // Clear
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = m_camera.view_matrix();
    glm::mat4 projection = m_camera.projection_matrix();

    // Render grid
    render_grid();

    // Render entities
    if (m_pbr_shader && m_pbr_shader->is_valid()) {
        m_pbr_shader->use();

        // Camera uniforms
        m_pbr_shader->set_mat4("view", view);
        m_pbr_shader->set_mat4("projection", projection);
        m_pbr_shader->set_vec3("camPos", m_camera.position);

        // Light uniforms
        upload_lights();

        // Ambient
        m_pbr_shader->set_vec3("ambientColor", glm::vec3(0.3f, 0.35f, 0.4f));
        m_pbr_shader->set_float("ambientIntensity", 0.3f);

        for (const auto& entity : m_entities) {
            if (entity.visible) {
                render_entity(entity);
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    m_stats.frame_time_ms = std::chrono::duration<float, std::milli>(end - start).count();
}

void SceneRenderer::render_entity(const RenderEntity& entity) {
    // Find mesh
    auto it = m_meshes.find(entity.mesh_type);
    if (it == m_meshes.end()) {
        // Try lowercase
        std::string lower = entity.mesh_type;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        it = m_meshes.find(lower);
        if (it == m_meshes.end()) {
            return;  // Mesh not found
        }
    }

    const GpuMesh& mesh = it->second;
    if (!mesh.is_valid()) return;

    // Model matrix
    m_pbr_shader->set_mat4("model", entity.transform);

    // Normal matrix (inverse transpose of model matrix for non-uniform scaling)
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(entity.transform)));
    m_pbr_shader->set_mat3("normalMatrix", normalMatrix);

    // Material
    m_pbr_shader->set_vec3("albedo", entity.material.albedo);
    m_pbr_shader->set_float("metallic", entity.material.metallic);
    m_pbr_shader->set_float("roughness", entity.material.roughness);
    m_pbr_shader->set_float("ao", entity.material.ao);
    m_pbr_shader->set_vec3("emissive", entity.material.emissive);
    m_pbr_shader->set_float("emissiveStrength", entity.material.emissive_strength);

    // Draw
    GL_CALL(glBindVertexArray, mesh.vao);
    if (mesh.has_indices) {
        glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
        m_stats.triangles += mesh.index_count / 3;
    } else {
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
        m_stats.triangles += mesh.vertex_count / 3;
    }
    GL_CALL(glBindVertexArray, 0);

    m_stats.draw_calls++;
}

void SceneRenderer::render_grid() {
    if (!m_grid_shader || !m_grid_shader->is_valid()) return;

    auto it = m_meshes.find("plane");
    if (it == m_meshes.end()) return;

    const GpuMesh& mesh = it->second;
    if (!mesh.is_valid()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    m_grid_shader->use();
    m_grid_shader->set_mat4("view", m_camera.view_matrix());
    m_grid_shader->set_mat4("projection", m_camera.projection_matrix());

    // Scale up the plane for grid
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 1.0f, 20.0f));
    m_grid_shader->set_mat4("model", model);

    GL_CALL(glBindVertexArray, mesh.vao);
    if (mesh.has_indices) {
        glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
    }
    GL_CALL(glBindVertexArray, 0);

    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void SceneRenderer::upload_lights() {
    int num_lights = std::min(static_cast<int>(m_lights.size()), 4);
    m_pbr_shader->set_int("numLights", num_lights);

    for (int i = 0; i < num_lights; ++i) {
        const Light& light = m_lights[i];
        std::string prefix = "lightPositions[" + std::to_string(i) + "]";
        m_pbr_shader->set_vec3(prefix, light.position);

        prefix = "lightColors[" + std::to_string(i) + "]";
        m_pbr_shader->set_vec3(prefix, light.color);

        prefix = "lightIntensities[" + std::to_string(i) + "]";
        m_pbr_shader->set_float(prefix, light.intensity);
    }
}

void SceneRenderer::update(float delta_time) {
    m_total_time += delta_time;

    // Update animations
    update_animations(delta_time);

    if (m_shader_hot_reload) {
        m_shader_check_timer += delta_time;
        if (m_shader_check_timer >= SHADER_CHECK_INTERVAL) {
            m_shader_check_timer = 0.0f;
            check_shader_reload();
        }
    }
}

// =============================================================================
// Animation System
// =============================================================================

void SceneRenderer::update_animations(float delta_time) {
    for (auto& entity : m_entities) {
        if (entity.animation) {
            update_entity_animation(entity, delta_time);
            entity.transform = compute_animation_transform(entity);
        }
    }
}

void SceneRenderer::update_entity_animation(RenderEntity& entity, float delta_time) {
    if (!entity.animation) return;

    const auto& anim = *entity.animation;
    auto& state = entity.animation_state;

    state.time += delta_time;

    // For path animation, update normalized time
    if (anim.type == void_scene::AnimationType::Path) {
        float duration = anim.duration > 0.0f ? anim.duration : 1.0f;
        state.normalized_time = state.time / duration;

        if (anim.ping_pong) {
            // Oscillate between 0 and 1
            float cycle_time = std::fmod(state.normalized_time, 2.0f);
            if (cycle_time > 1.0f) {
                state.normalized_time = 2.0f - cycle_time;
            } else {
                state.normalized_time = cycle_time;
            }
        } else if (anim.loop_animation) {
            // Loop back to start
            state.normalized_time = std::fmod(state.normalized_time, 1.0f);
        } else {
            // Clamp to end
            state.normalized_time = std::min(state.normalized_time, 1.0f);
        }
    }
}

glm::mat4 SceneRenderer::compute_animation_transform(const RenderEntity& entity) const {
    if (!entity.animation) return entity.base_transform;

    const auto& anim = *entity.animation;
    const auto& state = entity.animation_state;
    glm::mat4 result = entity.base_transform;

    // Extract base position from transform
    glm::vec3 base_pos = glm::vec3(entity.base_transform[3]);

    switch (anim.type) {
        case void_scene::AnimationType::Rotate: {
            // Continuous rotation around axis
            glm::vec3 axis = glm::normalize(glm::vec3(anim.axis[0], anim.axis[1], anim.axis[2]));
            float angle = state.time * anim.speed;
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, axis);

            // Apply rotation to the object (before translation)
            glm::mat4 centered = glm::translate(glm::mat4(1.0f), -base_pos) * entity.base_transform;
            result = glm::translate(glm::mat4(1.0f), base_pos) * rotation * centered;
            break;
        }

        case void_scene::AnimationType::Oscillate: {
            // Oscillate position along axis
            glm::vec3 axis = glm::normalize(glm::vec3(anim.axis[0], anim.axis[1], anim.axis[2]));
            float phase = anim.phase * std::numbers::pi_v<float> / 180.0f;  // Convert degrees to radians
            float offset = std::sin(state.time * anim.frequency * 2.0f * std::numbers::pi_v<float> + phase) * anim.amplitude;

            if (anim.rotate) {
                // Oscillate rotation
                glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), offset, axis);
                glm::mat4 centered = glm::translate(glm::mat4(1.0f), -base_pos) * entity.base_transform;
                result = glm::translate(glm::mat4(1.0f), base_pos) * rotation * centered;
            } else {
                // Oscillate position
                result = glm::translate(entity.base_transform, axis * offset);
            }
            break;
        }

        case void_scene::AnimationType::Orbit: {
            // Orbit around a center point
            glm::vec3 center = glm::vec3(anim.center[0], anim.center[1], anim.center[2]);
            float start_rad = anim.start_angle * std::numbers::pi_v<float> / 180.0f;
            float angle = start_rad + state.time * anim.speed;

            float x = center.x + anim.radius * std::cos(angle);
            float z = center.z + anim.radius * std::sin(angle);
            float y = base_pos.y;  // Maintain original Y

            glm::vec3 new_pos(x, y, z);
            glm::mat4 translation = glm::translate(glm::mat4(1.0f), new_pos);

            // Extract scale and rotation from base
            glm::vec3 scale;
            scale.x = glm::length(glm::vec3(entity.base_transform[0]));
            scale.y = glm::length(glm::vec3(entity.base_transform[1]));
            scale.z = glm::length(glm::vec3(entity.base_transform[2]));

            glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);

            if (anim.face_center) {
                // Face towards the center
                glm::vec3 dir = glm::normalize(center - new_pos);
                float yaw = std::atan2(dir.x, dir.z);
                glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0));
                result = translation * rotation * scale_mat;
            } else {
                result = translation * scale_mat;
            }
            break;
        }

        case void_scene::AnimationType::Pulse: {
            // Pulsing scale
            float phase = anim.phase * std::numbers::pi_v<float> / 180.0f;
            float t = (std::sin(state.time * anim.frequency * 2.0f * std::numbers::pi_v<float> + phase) + 1.0f) * 0.5f;
            float scale_factor = glm::mix(anim.min_scale, anim.max_scale, t);

            // Apply scale around the entity's center
            glm::mat4 to_origin = glm::translate(glm::mat4(1.0f), -base_pos);
            glm::mat4 from_origin = glm::translate(glm::mat4(1.0f), base_pos);
            glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(scale_factor));
            result = from_origin * scale_mat * to_origin * entity.base_transform;
            break;
        }

        case void_scene::AnimationType::Path: {
            // Follow a path
            if (anim.points.size() < 2) {
                result = entity.base_transform;
                break;
            }

            // Find the two points we're between
            float total_segments = static_cast<float>(anim.points.size() - 1);
            float segment_f = state.normalized_time * total_segments;
            int segment = static_cast<int>(std::floor(segment_f));
            float t = segment_f - static_cast<float>(segment);

            // Clamp segment
            segment = std::max(0, std::min(segment, static_cast<int>(anim.points.size()) - 2));

            const auto& p0 = anim.points[segment];
            const auto& p1 = anim.points[segment + 1];

            glm::vec3 start(p0[0], p0[1], p0[2]);
            glm::vec3 end(p1[0], p1[1], p1[2]);

            // Simple linear interpolation
            glm::vec3 pos = glm::mix(start, end, t);

            // Extract scale from base transform
            glm::vec3 scale;
            scale.x = glm::length(glm::vec3(entity.base_transform[0]));
            scale.y = glm::length(glm::vec3(entity.base_transform[1]));
            scale.z = glm::length(glm::vec3(entity.base_transform[2]));

            glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
            glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);

            if (anim.orient_to_path && glm::length(end - start) > 0.001f) {
                glm::vec3 dir = glm::normalize(end - start);
                float yaw = std::atan2(dir.x, dir.z);
                glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0));
                result = translation * rotation * scale_mat;
            } else {
                result = translation * scale_mat;
            }
            break;
        }

        default:
            result = entity.base_transform;
            break;
    }

    return result;
}

void SceneRenderer::on_resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_camera.aspect = static_cast<float>(width) / static_cast<float>(height);
    glViewport(0, 0, width, height);
}

} // namespace void_render
