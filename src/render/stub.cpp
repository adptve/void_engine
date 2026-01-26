// void_render stub implementations
// TODO: Re-enable full implementations during migration

#include <void_engine/render/gl_renderer.hpp>
#include <spdlog/spdlog.h>

namespace void_render {

// =============================================================================
// GlCamera Stubs
// =============================================================================

glm::mat4 GlCamera::view_matrix() const {
    return glm::lookAt(position, target, up);
}

glm::mat4 GlCamera::projection_matrix() const {
    if (is_perspective) {
        return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
    } else {
        float half_size = ortho_size;
        return glm::ortho(-half_size * aspect, half_size * aspect,
                          -half_size, half_size, near_plane, far_plane);
    }
}

glm::mat4 GlCamera::view_projection() const {
    return projection_matrix() * view_matrix();
}

void GlCamera::orbit(float delta_yaw, float delta_pitch) {
    m_yaw += delta_yaw * 0.01f;
    m_pitch += delta_pitch * 0.01f;

    // Clamp pitch to avoid gimbal lock
    m_pitch = glm::clamp(m_pitch, -glm::half_pi<float>() + 0.1f,
                                   glm::half_pi<float>() - 0.1f);

    // Calculate new position
    position.x = target.x + m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    position.y = target.y + m_distance * std::sin(m_pitch);
    position.z = target.z + m_distance * std::cos(m_pitch) * std::cos(m_yaw);
}

void GlCamera::zoom(float delta) {
    m_distance -= delta * 0.5f;
    m_distance = glm::max(m_distance, 0.1f);

    // Update position
    position.x = target.x + m_distance * std::cos(m_pitch) * std::sin(m_yaw);
    position.y = target.y + m_distance * std::sin(m_pitch);
    position.z = target.z + m_distance * std::cos(m_pitch) * std::cos(m_yaw);
}

void GlCamera::pan(float delta_x, float delta_y) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 cam_up = glm::cross(right, forward);

    float scale = m_distance * 0.001f;
    glm::vec3 offset = right * delta_x * scale + cam_up * delta_y * scale;

    position += offset;
    target += offset;
}

// =============================================================================
// GpuMesh Stub
// =============================================================================

void GpuMesh::destroy() {
    // Stub - would delete GL resources
    vao = 0;
    vbo = 0;
    ebo = 0;
}

// =============================================================================
// ShaderProgram Stubs
// =============================================================================

ShaderProgram::~ShaderProgram() {
    // Stub - would delete GL program
}

bool ShaderProgram::load_from_source(const std::string&, const std::string&) {
    spdlog::warn("ShaderProgram::load_from_source - stub implementation");
    return true;
}

bool ShaderProgram::load_from_files(const std::filesystem::path&, const std::filesystem::path&) {
    spdlog::warn("ShaderProgram::load_from_files - stub implementation");
    return true;
}

bool ShaderProgram::reload() {
    return true;
}

void ShaderProgram::use() const {
    // Stub
}

void ShaderProgram::set_bool(const std::string&, bool) const {}
void ShaderProgram::set_int(const std::string&, int) const {}
void ShaderProgram::set_float(const std::string&, float) const {}
void ShaderProgram::set_vec2(const std::string&, const glm::vec2&) const {}
void ShaderProgram::set_vec3(const std::string&, const glm::vec3&) const {}
void ShaderProgram::set_vec4(const std::string&, const glm::vec4&) const {}
void ShaderProgram::set_mat3(const std::string&, const glm::mat3&) const {}
void ShaderProgram::set_mat4(const std::string&, const glm::mat4&) const {}

void_core::Result<void_core::HotReloadSnapshot> ShaderProgram::snapshot() {
    return void_core::Ok(void_core::HotReloadSnapshot{});
}

void_core::Result<void> ShaderProgram::restore(void_core::HotReloadSnapshot) {
    return void_core::Ok();
}

bool ShaderProgram::is_compatible(const void_core::Version&) const {
    return true;
}

void_core::Version ShaderProgram::current_version() const {
    return m_version;
}

GLuint ShaderProgram::compile_shader(GLenum, const std::string&) {
    return 0;
}

bool ShaderProgram::link_program(GLuint, GLuint) {
    return true;
}

GLint ShaderProgram::get_uniform_location(const std::string&) const {
    return -1;
}

// =============================================================================
// SceneRenderer Stubs
// =============================================================================

SceneRenderer::SceneRenderer() {
    spdlog::info("SceneRenderer created (stub implementation)");
}

SceneRenderer::~SceneRenderer() {
    spdlog::info("SceneRenderer destroyed (stub implementation)");
}

bool SceneRenderer::initialize(GLFWwindow* window) {
    m_window = window;
    spdlog::warn("SceneRenderer::initialize - stub implementation");
    return true;
}

void SceneRenderer::shutdown() {
    spdlog::info("SceneRenderer::shutdown (stub)");
}

void SceneRenderer::load_scene(const void_scene::SceneData& scene_data) {
    spdlog::info("SceneRenderer::load_scene - {} entities (stub)", scene_data.entities.size());

    // Convert camera data if available
    if (auto* cam = scene_data.active_camera()) {
        m_camera.position = glm::vec3(cam->transform.position[0],
                                       cam->transform.position[1],
                                       cam->transform.position[2]);
        m_camera.target = glm::vec3(cam->transform.target[0],
                                     cam->transform.target[1],
                                     cam->transform.target[2]);
        m_camera.fov = cam->perspective.fov;
    }
}

void SceneRenderer::render() {
    // Stub - would render the scene
    m_stats.draw_calls = 0;
    m_stats.triangles = 0;
    m_stats.entities = static_cast<int>(m_entities.size());
}

void SceneRenderer::update(float) {
    // Stub - would update animations and hot-reload
}

void SceneRenderer::on_resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_camera.aspect = static_cast<float>(width) / static_cast<float>(height);
}

void SceneRenderer::reload_shaders() {
    spdlog::info("SceneRenderer::reload_shaders (stub)");
}

void SceneRenderer::create_builtin_meshes() {}
GpuMesh SceneRenderer::create_sphere_mesh(int, int) { return {}; }
GpuMesh SceneRenderer::create_cube_mesh() { return {}; }
GpuMesh SceneRenderer::create_torus_mesh(float, float, int, int) { return {}; }
GpuMesh SceneRenderer::create_plane_mesh(float) { return {}; }
GpuMesh SceneRenderer::create_cylinder_mesh(float, float, int) { return {}; }
GpuMesh SceneRenderer::create_diamond_mesh() { return {}; }
GpuMesh SceneRenderer::create_quad_mesh() { return {}; }

bool SceneRenderer::create_shaders() { return true; }
void SceneRenderer::check_shader_reload() {}
void SceneRenderer::render_entity(const RenderEntity&) {}
void SceneRenderer::render_grid() {}
void SceneRenderer::upload_lights() {}

void SceneRenderer::convert_camera(const void_scene::CameraData&) {}
void SceneRenderer::convert_light(const void_scene::LightData&) {}
void SceneRenderer::convert_entity(const void_scene::EntityData&) {}

} // namespace void_render
