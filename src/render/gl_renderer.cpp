/// @file gl_renderer.cpp
/// @brief OpenGL helper implementation with hot-reload support

#include <void_engine/render/gl_renderer.hpp>

#include <spdlog/spdlog.h>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
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
    if (!fragment) return false;

    return link_program(vertex, fragment);
}

bool ShaderProgram::load_from_files(const std::filesystem::path& vertex_path,
                                   const std::filesystem::path& fragment_path) {
    std::ifstream vertex_file(vertex_path);
    std::ifstream fragment_file(fragment_path);

    if (!vertex_file.is_open() || !fragment_file.is_open()) {
        spdlog::error("Failed to open shader files: {} or {}",
                      vertex_path.string(), fragment_path.string());
        return false;
    }

    std::stringstream vertex_stream;
    std::stringstream fragment_stream;
    vertex_stream << vertex_file.rdbuf();
    fragment_stream << fragment_file.rdbuf();

    m_vertex_path = vertex_path;
    m_fragment_path = fragment_path;

    if (std::filesystem::exists(vertex_path)) {
        m_vertex_mtime = std::filesystem::last_write_time(vertex_path);
    }
    if (std::filesystem::exists(fragment_path)) {
        m_fragment_mtime = std::filesystem::last_write_time(fragment_path);
    }

    return load_from_source(vertex_stream.str(), fragment_stream.str());
}

bool ShaderProgram::reload() {
    if (m_vertex_path.empty() || m_fragment_path.empty()) {
        return false;
    }
    return load_from_files(m_vertex_path, m_fragment_path);
}

void ShaderProgram::use() const {
    if (m_program) {
        GL_CALL(glUseProgram, m_program);
    }
}

GLint ShaderProgram::get_uniform_location(const std::string& name) const {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }

    GLint location = GL_CALL(glGetUniformLocation, m_program, name.c_str());
    m_uniform_cache[name] = location;
    return location;
}

void ShaderProgram::set_bool(const std::string& name, bool value) const {
    GL_CALL(glUniform1i, get_uniform_location(name), static_cast<int>(value));
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

} // namespace void_render
