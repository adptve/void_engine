/// @file gl_renderer.hpp
/// @brief OpenGL helpers for render assets and shader management

#pragma once

#include <void_engine/core/hot_reload.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_render {

// =============================================================================
// OpenGL Types (to avoid including GL headers everywhere)
// =============================================================================

using GLuint = unsigned int;
using GLenum = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLboolean = unsigned char;

// =============================================================================
// GPU Mesh
// =============================================================================

/// @brief GPU mesh data
struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei index_count = 0;
    GLsizei vertex_count = 0;
    bool has_indices = false;

    // Bounding box
    std::array<float, 3> min_bounds = {0, 0, 0};
    std::array<float, 3> max_bounds = {0, 0, 0};

    void destroy();
    [[nodiscard]] bool is_valid() const noexcept { return vao != 0; }
};

// =============================================================================
// Shader Program with Hot-Reload
// =============================================================================

/// @brief Hot-reloadable shader program
class ShaderProgram : public void_core::HotReloadable {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    /// @brief Load shader from source strings
    bool load_from_source(const std::string& vertex_src, const std::string& fragment_src);

    /// @brief Load shader from files (enables hot-reload)
    bool load_from_files(const std::filesystem::path& vertex_path,
                         const std::filesystem::path& fragment_path);

    /// @brief Reload shaders from files
    bool reload();

    /// @brief Use this shader
    void use() const;

    /// @brief Get program ID
    GLuint id() const { return m_program; }

    /// @brief Check if valid
    bool is_valid() const { return m_program != 0; }

    // Uniform setters
    void set_bool(const std::string& name, bool value) const;
    void set_int(const std::string& name, int value) const;
    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, const glm::vec2& value) const;
    void set_vec3(const std::string& name, const glm::vec3& value) const;
    void set_vec4(const std::string& name, const glm::vec4& value) const;
    void set_mat3(const std::string& name, const glm::mat3& value) const;
    void set_mat4(const std::string& name, const glm::mat4& value) const;

    // HotReloadable interface
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() override;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot) override;
    [[nodiscard]] bool is_compatible(const void_core::Version& new_version) const override;
    [[nodiscard]] void_core::Version current_version() const override;
    [[nodiscard]] std::string type_name() const override { return "ShaderProgram"; }

    /// @brief Callback when shader is reloaded
    std::function<void()> on_reloaded;

private:
    GLuint m_program = 0;
    std::filesystem::path m_vertex_path;
    std::filesystem::path m_fragment_path;
    std::filesystem::file_time_type m_vertex_mtime;
    std::filesystem::file_time_type m_fragment_mtime;
    void_core::Version m_version{1, 0, 0};

    GLuint compile_shader(GLenum type, const std::string& source);
    bool link_program(GLuint vertex, GLuint fragment);
    GLint get_uniform_location(const std::string& name) const;
    mutable std::unordered_map<std::string, GLint> m_uniform_cache;
};

// =============================================================================
// OpenGL Loader
// =============================================================================

/// @brief Load OpenGL functions (call after context creation)
bool load_opengl_functions();

/// @brief Check if OpenGL is loaded
bool is_opengl_loaded();

} // namespace void_render
