/// @file gl_renderer.hpp
/// @brief OpenGL-based scene renderer with hot-reload support

#pragma once

#include <void_engine/scene/scene_data.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declare GLFW
struct GLFWwindow;

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

    void destroy();
    bool is_valid() const { return vao != 0; }
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
// GlCamera (simple camera for GL renderer)
// =============================================================================

/// @brief Simple camera for 3D rendering (renamed to avoid conflict with camera.hpp)
struct GlCamera {
    glm::vec3 position{0.0f, 2.0f, 5.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect = 16.0f / 9.0f;

    bool is_perspective = true;
    float ortho_size = 10.0f;

    [[nodiscard]] glm::mat4 view_matrix() const;
    [[nodiscard]] glm::mat4 projection_matrix() const;
    [[nodiscard]] glm::mat4 view_projection() const;

    // Orbit camera controls
    void orbit(float delta_yaw, float delta_pitch);
    void zoom(float delta);
    void pan(float delta_x, float delta_y);

private:
    float m_distance = 5.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.3f;
};

// =============================================================================
// Light
// =============================================================================

/// @brief Light types
enum class LightType {
    Directional,
    Point,
    Spot
};

/// @brief Light data for rendering
struct Light {
    LightType type = LightType::Directional;
    glm::vec3 position{0.0f, 10.0f, 10.0f};
    glm::vec3 direction{0.0f, -1.0f, -1.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 100.0f;
    float inner_cone = 30.0f;
    float outer_cone = 45.0f;
    bool cast_shadows = true;
};

// =============================================================================
// Material
// =============================================================================

/// @brief PBR Material
struct Material {
    glm::vec3 albedo{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f, 0.0f, 0.0f};
    float emissive_strength = 0.0f;
    float alpha = 1.0f;
};

// =============================================================================
// Render Entity
// =============================================================================

/// @brief Animation state for runtime
struct AnimationState {
    float time = 0.0f;
    float normalized_time = 0.0f;  // 0.0 - 1.0 for path animations
    int current_path_segment = 0;
    bool ping_pong_forward = true;
};

/// @brief Entity ready for rendering
struct RenderEntity {
    std::string name;
    std::string mesh_type;  // "sphere", "cube", "torus", etc.
    glm::mat4 transform{1.0f};
    glm::mat4 base_transform{1.0f};  // Original transform before animation
    Material material;
    bool visible = true;

    // Animation data
    std::optional<void_scene::AnimationData> animation;
    AnimationState animation_state;
};

// =============================================================================
// Scene Renderer
// =============================================================================

/// @brief OpenGL scene renderer with hot-reload support
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    // Non-copyable
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    /// @brief Initialize the renderer
    bool initialize(GLFWwindow* window);

    /// @brief Shutdown the renderer
    void shutdown();

    /// @brief Load scene from parsed data
    void load_scene(const void_scene::SceneData& scene_data);

    /// @brief Render the current scene
    void render();

    /// @brief Update (check for hot-reload, etc.)
    void update(float delta_time);

    /// @brief Handle window resize
    void on_resize(int width, int height);

    /// @brief Get/Set camera
    GlCamera& camera() { return m_camera; }
    const GlCamera& camera() const { return m_camera; }

    /// @brief Enable/disable shader hot-reload
    void set_shader_hot_reload(bool enabled) { m_shader_hot_reload = enabled; }

    /// @brief Force reload all shaders
    void reload_shaders();

    /// @brief Get render statistics
    struct RenderStats {
        int draw_calls = 0;
        int triangles = 0;
        int entities = 0;
        float frame_time_ms = 0.0f;
    };
    const RenderStats& stats() const { return m_stats; }

private:
    GLFWwindow* m_window = nullptr;
    int m_width = 1280;
    int m_height = 720;

    // Shaders
    std::unique_ptr<ShaderProgram> m_pbr_shader;
    std::unique_ptr<ShaderProgram> m_grid_shader;

    // Meshes
    std::unordered_map<std::string, GpuMesh> m_meshes;

    // Scene data
    GlCamera m_camera;
    std::vector<Light> m_lights;
    std::vector<RenderEntity> m_entities;

    // Hot-reload
    bool m_shader_hot_reload = true;
    float m_shader_check_timer = 0.0f;
    static constexpr float SHADER_CHECK_INTERVAL = 0.5f;

    // Stats
    RenderStats m_stats;

    // Internal methods
    void create_builtin_meshes();
    GpuMesh create_sphere_mesh(int segments = 32, int rings = 16);
    GpuMesh create_cube_mesh();
    GpuMesh create_torus_mesh(float inner_radius = 0.3f, float outer_radius = 1.0f, int segments = 32, int rings = 16);
    GpuMesh create_plane_mesh(float size = 10.0f);
    GpuMesh create_cylinder_mesh(float radius = 0.5f, float height = 2.0f, int segments = 32);
    GpuMesh create_diamond_mesh();
    GpuMesh create_quad_mesh();

    bool create_shaders();
    void check_shader_reload();
    void render_entity(const RenderEntity& entity);
    void render_grid();
    void upload_lights();

    // Convert scene data
    void convert_camera(const void_scene::CameraData& data);
    void convert_light(const void_scene::LightData& data);
    void convert_entity(const void_scene::EntityData& data);
    Material convert_material(const void_scene::MaterialData& data);
    glm::mat4 convert_transform(const void_scene::TransformData& data);

    // Animation
    void update_animations(float delta_time);
    void update_entity_animation(RenderEntity& entity, float delta_time);
    glm::mat4 compute_animation_transform(const RenderEntity& entity) const;

    // Total time for animations
    float m_total_time = 0.0f;
};

// =============================================================================
// OpenGL Loader
// =============================================================================

/// @brief Load OpenGL functions (call after context creation)
bool load_opengl_functions();

/// @brief Check if OpenGL is loaded
bool is_opengl_loaded();

} // namespace void_render
