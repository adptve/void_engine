/// @file render_systems.cpp
/// @brief ECS render systems implementation

#include <void_engine/render/render_systems.hpp>
#include <void_engine/render/components.hpp>
#include <void_engine/ecs/world.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

// GL constants that may not be in base headers
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT 0x1405
#endif

// GL extension function types
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);

namespace {
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray_fn = nullptr;
    bool gl_funcs_loaded = false;

    void ensure_gl_funcs() {
        if (gl_funcs_loaded) return;
        glBindVertexArray_fn = (PFNGLBINDVERTEXARRAYPROC)wglGetProcAddress("glBindVertexArray");
        gl_funcs_loaded = true;
    }

    // Wrapper to make code cleaner
    inline void glBindVertexArray(GLuint array) {
        if (glBindVertexArray_fn) glBindVertexArray_fn(array);
    }
}
#endif

namespace void_render {

// =============================================================================
// RenderQueue Implementation
// =============================================================================

void RenderQueue::clear() {
    m_commands.clear();
}

void RenderQueue::push(DrawCommand cmd) {
    m_commands.push_back(std::move(cmd));
}

void RenderQueue::sort() {
    std::sort(m_commands.begin(), m_commands.end(),
              [](const DrawCommand& a, const DrawCommand& b) {
                  return a.sort_key < b.sort_key;
              });
}

// =============================================================================
// RenderContext Implementation
// =============================================================================

RenderContext::RenderContext() : m_assets(std::make_unique<RenderAssetManager>()) {}
RenderContext::~RenderContext() { shutdown(); }

void_core::Result<void> RenderContext::initialize(std::uint32_t width, std::uint32_t height) {
    m_width = width;
    m_height = height;

    // Initialize asset manager
    auto result = m_assets->initialize(std::filesystem::current_path() / "assets");
    if (!result) {
        return result;
    }

    spdlog::info("RenderContext initialized: {}x{}", width, height);
    return void_core::Ok();
}

void RenderContext::shutdown() {
    if (m_assets) {
        m_assets->shutdown();
    }
    m_render_queue.clear();
    m_lights.clear();
}

void RenderContext::on_resize(std::uint32_t width, std::uint32_t height) {
    m_width = width;
    m_height = height;
}

// =============================================================================
// ModelLoaderSystem
// =============================================================================

void_ecs::SystemDescriptor ModelLoaderSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "ModelLoaderSystem";
    desc.stage = void_ecs::SystemStage::Update;
    return desc;
}

void ModelLoaderSystem::run(void_ecs::World& world, float) {
    auto* render_ctx = world.resource<RenderContext>();
    if (!render_ctx) return;

    auto& assets = render_ctx->assets();

    // Query entities with ModelComponent
    auto query = world.query_with<ModelComponent>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* model_comp = iter.get<ModelComponent>();
        if (!model_comp) continue;

        // Handle different loading states
        switch (model_comp->state) {
            case ModelComponent::State::Unloaded: {
                // Start loading
                if (!model_comp->path.empty()) {
                    model_comp->state = ModelComponent::State::Loading;

                    ModelLoadOptions options;
                    options.generate_tangents = model_comp->generate_tangents;
                    options.flip_uvs = model_comp->flip_uvs;
                    options.scale = model_comp->scale_factor;

                    model_comp->model_handle = assets.load_model(model_comp->path, options);

                    if (model_comp->model_handle.is_valid()) {
                        model_comp->state = ModelComponent::State::Loaded;
                    } else {
                        model_comp->state = ModelComponent::State::Failed;
                        model_comp->error = "Failed to load model";
                    }
                }
                break;
            }

            case ModelComponent::State::Loading: {
                // Check if async load completed (for future async support)
                if (assets.is_model_loaded(model_comp->model_handle)) {
                    model_comp->state = ModelComponent::State::Loaded;
                }
                break;
            }

            case ModelComponent::State::Loaded:
            case ModelComponent::State::Failed:
                // Nothing to do
                break;
        }
    }
}

// =============================================================================
// TransformSystem
// =============================================================================

void_ecs::SystemDescriptor TransformSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "TransformSystem";
    desc.stage = void_ecs::SystemStage::Update;
    return desc;
}

namespace {

void multiply_matrices(const std::array<float, 16>& a,
                       const std::array<float, 16>& b,
                       std::array<float, 16>& out) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
}

} // anonymous namespace

void TransformSystem::run(void_ecs::World& world, float) {
    // First pass: update local matrices for dirty transforms
    auto transform_query = world.query_with<TransformComponent>();

    for (auto iter = world.query_iter(transform_query); !iter.empty(); iter.next()) {
        auto* transform = iter.get<TransformComponent>();
        if (!transform) continue;

        if (transform->dirty) {
            transform->world_matrix = transform->local_matrix();
            transform->dirty = false;
        }
    }

    // Second pass: propagate through hierarchy
    auto hierarchy_query = world.query_with<TransformComponent, HierarchyComponent>();

    for (auto iter = world.query_iter(hierarchy_query); !iter.empty(); iter.next()) {
        auto* transform = iter.get<TransformComponent>();
        auto* hierarchy = iter.get<HierarchyComponent>();
        if (!transform || !hierarchy) continue;

        if (hierarchy->has_parent()) {
            // Get parent transform
            void_ecs::Entity parent_entity;
            parent_entity.index = static_cast<std::uint32_t>(hierarchy->parent_id);
            parent_entity.generation = hierarchy->parent_generation;

            if (world.is_alive(parent_entity)) {
                auto* parent_transform = world.get_component<TransformComponent>(parent_entity);
                if (parent_transform) {
                    auto local = transform->local_matrix();
                    multiply_matrices(parent_transform->world_matrix, local, transform->world_matrix);
                }
            }
        }
    }
}

// =============================================================================
// AnimationSystem
// =============================================================================

void_ecs::SystemDescriptor AnimationSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "AnimationSystem";
    desc.stage = void_ecs::SystemStage::Update;
    return desc;
}

void AnimationSystem::run(void_ecs::World& world, float delta_time) {
    auto query = world.query_with<AnimationComponent, TransformComponent>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* anim = iter.get<AnimationComponent>();
        auto* transform = iter.get<TransformComponent>();
        if (!anim || !transform || !anim->playing) continue;

        anim->elapsed_time += delta_time;

        switch (anim->type) {
            case AnimationComponent::Type::Rotate: {
                float angle = anim->elapsed_time * anim->speed;
                // Convert axis-angle to quaternion
                float half_angle = angle * 0.5f;
                float s = std::sin(half_angle);
                transform->rotation = {{
                    anim->axis[0] * s,
                    anim->axis[1] * s,
                    anim->axis[2] * s,
                    std::cos(half_angle)
                }};
                transform->dirty = true;
                break;
            }

            case AnimationComponent::Type::Oscillate: {
                float phase = anim->phase * std::numbers::pi_v<float> / 180.0f;
                float offset = std::sin(anim->elapsed_time * anim->frequency * 2.0f *
                                        std::numbers::pi_v<float> + phase) * anim->amplitude;
                // Apply offset along axis
                transform->position = {{
                    anim->axis[0] * offset,
                    anim->axis[1] * offset,
                    anim->axis[2] * offset
                }};
                transform->dirty = true;
                break;
            }

            case AnimationComponent::Type::Orbit: {
                float angle = anim->elapsed_time * anim->speed *
                              std::numbers::pi_v<float> / 180.0f;
                float x = anim->orbit_center[0] + anim->orbit_radius * std::cos(angle);
                float z = anim->orbit_center[2] + anim->orbit_radius * std::sin(angle);
                transform->position = {{x, anim->orbit_center[1], z}};
                transform->dirty = true;
                break;
            }

            case AnimationComponent::Type::Pulse: {
                float t = (std::sin(anim->elapsed_time * anim->frequency * 2.0f *
                                    std::numbers::pi_v<float>) + 1.0f) * 0.5f;
                float scale = anim->amplitude + t * (1.0f - anim->amplitude);
                transform->scale = {{scale, scale, scale}};
                transform->dirty = true;
                break;
            }

            default:
                break;
        }
    }
}

// =============================================================================
// CameraSystem
// =============================================================================

void_ecs::SystemDescriptor CameraSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "CameraSystem";
    desc.stage = void_ecs::SystemStage::PreUpdate;
    return desc;
}

namespace {

void compute_view_matrix(const TransformComponent& transform, float* out) {
    // Extract position from world matrix
    float px = transform.world_matrix[12];
    float py = transform.world_matrix[13];
    float pz = transform.world_matrix[14];

    // Extract forward direction (negative Z in OpenGL convention)
    float fx = -transform.world_matrix[8];
    float fy = -transform.world_matrix[9];
    float fz = -transform.world_matrix[10];

    // Up is Y axis
    float ux = transform.world_matrix[4];
    float uy = transform.world_matrix[5];
    float uz = transform.world_matrix[6];

    // Right is X axis
    float rx = transform.world_matrix[0];
    float ry = transform.world_matrix[1];
    float rz = transform.world_matrix[2];

    // View matrix is inverse of camera transform
    out[0] = rx;  out[4] = ry;  out[8]  = rz;  out[12] = -(rx*px + ry*py + rz*pz);
    out[1] = ux;  out[5] = uy;  out[9]  = uz;  out[13] = -(ux*px + uy*py + uz*pz);
    out[2] = fx;  out[6] = fy;  out[10] = fz;  out[14] = -(fx*px + fy*py + fz*pz);
    out[3] = 0;   out[7] = 0;   out[11] = 0;   out[15] = 1;
}

void compute_perspective_matrix(float fov, float aspect, float near_plane, float far_plane, float* out) {
    float tan_half_fov = std::tan(fov * std::numbers::pi_v<float> / 360.0f);
    float f = 1.0f / tan_half_fov;

    std::fill_n(out, 16, 0.0f);
    out[0] = f / aspect;
    out[5] = f;
    out[10] = -(far_plane + near_plane) / (far_plane - near_plane);
    out[11] = -1.0f;
    out[14] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
}

} // anonymous namespace

void CameraSystem::run(void_ecs::World& world, float) {
    auto* render_ctx = world.resource<RenderContext>();
    if (!render_ctx) return;

    auto query = world.query_with<CameraComponent, TransformComponent>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* camera = iter.get<CameraComponent>();
        auto* transform = iter.get<TransformComponent>();
        if (!camera || !transform || !camera->active) continue;

        CameraData data;

        // View matrix from transform
        compute_view_matrix(*transform, data.view_matrix.data());

        // Projection matrix
        if (camera->projection == CameraComponent::Projection::Perspective) {
            compute_perspective_matrix(camera->fov, render_ctx->aspect_ratio(),
                                       camera->near_plane, camera->far_plane,
                                       data.projection_matrix.data());
        } else {
            // Orthographic
            float size = camera->ortho_size * 0.5f;
            float aspect = render_ctx->aspect_ratio();
            float left = -size * aspect, right = size * aspect;
            float bottom = -size, top = size;
            float near_plane = camera->near_plane, far_plane = camera->far_plane;

            std::fill_n(data.projection_matrix.data(), 16, 0.0f);
            data.projection_matrix[0] = 2.0f / (right - left);
            data.projection_matrix[5] = 2.0f / (top - bottom);
            data.projection_matrix[10] = -2.0f / (far_plane - near_plane);
            data.projection_matrix[12] = -(right + left) / (right - left);
            data.projection_matrix[13] = -(top + bottom) / (top - bottom);
            data.projection_matrix[14] = -(far_plane + near_plane) / (far_plane - near_plane);
            data.projection_matrix[15] = 1.0f;
        }

        // View-projection matrix
        multiply_matrices(
            *reinterpret_cast<std::array<float, 16>*>(data.projection_matrix.data()),
            *reinterpret_cast<std::array<float, 16>*>(data.view_matrix.data()),
            *reinterpret_cast<std::array<float, 16>*>(data.view_projection.data()));

        data.position = {{transform->world_matrix[12],
                          transform->world_matrix[13],
                          transform->world_matrix[14]}};
        data.near_plane = camera->near_plane;
        data.far_plane = camera->far_plane;
        data.fov = camera->fov;
        data.aspect = render_ctx->aspect_ratio();

        render_ctx->set_camera_data(data);

        // Only use first active camera
        break;
    }
}

// =============================================================================
// LightSystem
// =============================================================================

void_ecs::SystemDescriptor LightSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "LightSystem";
    desc.stage = void_ecs::SystemStage::PreUpdate;
    return desc;
}

void LightSystem::run(void_ecs::World& world, float) {
    auto* render_ctx = world.resource<RenderContext>();
    if (!render_ctx) return;

    render_ctx->clear_lights();

    auto query = world.query_with<LightComponent, TransformComponent>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* light = iter.get<LightComponent>();
        auto* transform = iter.get<TransformComponent>();
        if (!light || !transform) continue;

        LightData data;
        data.position = {{transform->world_matrix[12],
                          transform->world_matrix[13],
                          transform->world_matrix[14]}};

        // Direction from transform's negative Z axis
        data.direction = {{-transform->world_matrix[8],
                           -transform->world_matrix[9],
                           -transform->world_matrix[10]}};

        data.color = light->color;
        data.intensity = light->intensity;
        data.range = light->range;
        data.inner_cone = light->inner_cone_angle;
        data.outer_cone = light->outer_cone_angle;
        data.type = static_cast<std::int32_t>(light->type);

        render_ctx->add_light(data);
    }
}

// =============================================================================
// RenderPrepareSystem
// =============================================================================

void_ecs::SystemDescriptor RenderPrepareSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "RenderPrepareSystem";
    desc.stage = void_ecs::SystemStage::PostUpdate;
    return desc;
}

void RenderPrepareSystem::run(void_ecs::World& world, float) {
    auto* render_ctx = world.resource<RenderContext>();
    if (!render_ctx) return;

    render_ctx->render_queue().clear();
    render_ctx->reset_stats();

    auto& assets = render_ctx->assets();

    // Query renderable entities
    auto query = world.query_with<RenderableTag, TransformComponent, MeshComponent>();

    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        auto* renderable = iter.get<RenderableTag>();
        auto* transform = iter.get<TransformComponent>();
        auto* mesh_comp = iter.get<MeshComponent>();
        if (!renderable || !transform || !mesh_comp || !renderable->visible) continue;

        DrawCommand cmd;

        // Get mesh
        if (mesh_comp->is_builtin()) {
            cmd.mesh = assets.get_builtin_mesh(mesh_comp->builtin_mesh);
        } else if (mesh_comp->mesh_handle.is_valid()) {
            // Would need to map handle to GPU mesh
            // For now, skip non-builtin meshes
            continue;
        }

        if (!cmd.mesh || !cmd.mesh->is_valid()) continue;

        // Copy transform
        cmd.model_matrix = transform->world_matrix;

        // Compute normal matrix (inverse transpose of upper-left 3x3)
        // Simplified: assume uniform scale, just copy rotation part
        cmd.normal_matrix = {{
            transform->world_matrix[0], transform->world_matrix[1], transform->world_matrix[2],
            transform->world_matrix[4], transform->world_matrix[5], transform->world_matrix[6],
            transform->world_matrix[8], transform->world_matrix[9], transform->world_matrix[10]
        }};

        // Get material
        auto* material = iter.get<MaterialComponent>();
        if (material) {
            cmd.albedo = material->albedo;
            cmd.metallic = material->metallic_value;
            cmd.roughness = material->roughness_value;
            cmd.ao = material->ao_value;
            cmd.emissive = material->emissive;
            cmd.emissive_strength = material->emissive_strength;
            cmd.double_sided = material->double_sided;
            cmd.alpha_blend = material->alpha_blend;
        }

        // Compute sort key
        // For now: opaque first, then by distance from camera
        cmd.sort_key = cmd.alpha_blend ? 0x8000000000000000ULL : 0;

        render_ctx->render_queue().push(cmd);
    }

    // Sort the queue
    render_ctx->render_queue().sort();

    (void)render_ctx->stats();
}

// =============================================================================
// RenderSystem
// =============================================================================

void_ecs::SystemDescriptor RenderSystem::descriptor() {
    void_ecs::SystemDescriptor desc;
    desc.name = "RenderSystem";
    desc.stage = void_ecs::SystemStage::Last;
    desc.exclusive = true;  // Must run alone (GPU access)
    return desc;
}

void RenderSystem::run(void_ecs::World& world, float) {
#ifdef _WIN32
    ensure_gl_funcs();
#endif

    auto* render_ctx = world.resource<RenderContext>();
    if (!render_ctx) return;

    auto& assets = render_ctx->assets();
    auto* shader = assets.get_default_shader();

    // Clear
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!shader || !shader->is_valid()) {
        return;  // No shader available
    }

    shader->gpu_shader.use();

    // Upload camera uniforms
    const auto& camera = render_ctx->camera_data();
    shader->gpu_shader.set_mat4("view", camera.view_matrix.data());
    shader->gpu_shader.set_mat4("projection", camera.projection_matrix.data());
    shader->gpu_shader.set_vec3("camPos", camera.position[0], camera.position[1], camera.position[2]);

    // Upload light uniforms
    const auto& lights = render_ctx->lights();
    int num_lights = std::min(static_cast<int>(lights.size()), 4);
    shader->gpu_shader.set_int("numLights", num_lights);

    for (int i = 0; i < num_lights; ++i) {
        std::string prefix = "lightPositions[" + std::to_string(i) + "]";
        shader->gpu_shader.set_vec3(prefix, lights[i].position[0], lights[i].position[1], lights[i].position[2]);

        prefix = "lightColors[" + std::to_string(i) + "]";
        shader->gpu_shader.set_vec3(prefix, lights[i].color[0], lights[i].color[1], lights[i].color[2]);

        prefix = "lightIntensities[" + std::to_string(i) + "]";
        shader->gpu_shader.set_float(prefix, lights[i].intensity);
    }

    // Ambient
    shader->gpu_shader.set_vec3("ambientColor", 0.3f, 0.35f, 0.4f);
    shader->gpu_shader.set_float("ambientIntensity", 0.3f);

    // Draw all commands
    for (const auto& cmd : render_ctx->render_queue().commands()) {
        if (!cmd.mesh || !cmd.mesh->is_valid()) continue;

        // Model matrix
        shader->gpu_shader.set_mat4("model", cmd.model_matrix.data());
        shader->gpu_shader.set_mat3("normalMatrix", cmd.normal_matrix.data());

        // Material
        shader->gpu_shader.set_vec3("albedo", cmd.albedo[0], cmd.albedo[1], cmd.albedo[2]);
        shader->gpu_shader.set_float("metallic", cmd.metallic);
        shader->gpu_shader.set_float("roughness", cmd.roughness);
        shader->gpu_shader.set_float("ao", cmd.ao);
        shader->gpu_shader.set_vec3("emissive", cmd.emissive[0], cmd.emissive[1], cmd.emissive[2]);
        shader->gpu_shader.set_float("emissiveStrength", cmd.emissive_strength);

        // Double-sided
        if (cmd.double_sided) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
        }

        // Alpha blend
        if (cmd.alpha_blend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        // Draw
        glBindVertexArray(cmd.mesh->vao);
        if (cmd.mesh->has_indices) {
            glDrawElements(GL_TRIANGLES, cmd.mesh->index_count, GL_UNSIGNED_INT, nullptr);
            render_ctx->add_draw_call(cmd.mesh->index_count / 3);
        } else {
            glDrawArrays(GL_TRIANGLES, 0, cmd.mesh->vertex_count);
            render_ctx->add_draw_call(cmd.mesh->vertex_count / 3);
        }
        glBindVertexArray(0);
    }

    // Restore state
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

// =============================================================================
// System Registration
// =============================================================================

void register_render_systems(void_ecs::World& world) {
    // Register all render components
    register_render_components(world);

    // Create and insert RenderContext resource
    // Using RenderContext directly (not unique_ptr) - our Resources class supports move-only types
    world.insert_resource(RenderContext{});

    spdlog::info("Render systems registered");
}

// =============================================================================
// Entity Spawning Helpers
// =============================================================================

void_ecs::Entity spawn_renderable(void_ecs::World& world,
                                   const std::string& mesh_name,
                                   const MaterialComponent& material) {
    auto entity = world.spawn();

    world.add_component(entity, TransformComponent{});
    world.add_component(entity, MeshComponent::builtin(mesh_name));
    world.add_component(entity, material);
    world.add_component(entity, RenderableTag{});

    return entity;
}

void_ecs::Entity spawn_model(void_ecs::World& world,
                              const std::string& model_path,
                              const ModelLoadOptions& options) {
    auto entity = world.spawn();

    world.add_component(entity, TransformComponent{});

    ModelComponent model = ModelComponent::from_path(model_path);
    model.generate_tangents = options.generate_tangents;
    model.flip_uvs = options.flip_uvs;
    model.scale_factor = options.scale;
    world.add_component(entity, model);

    world.add_component(entity, RenderableTag{});

    return entity;
}

void_ecs::Entity spawn_light(void_ecs::World& world, const LightComponent& light) {
    auto entity = world.spawn();

    world.add_component(entity, TransformComponent{});
    world.add_component(entity, light);

    return entity;
}

void_ecs::Entity spawn_camera(void_ecs::World& world,
                               const CameraComponent& camera,
                               bool make_active) {
    auto entity = world.spawn();

    world.add_component(entity, TransformComponent{});

    CameraComponent cam = camera;
    cam.active = make_active;
    world.add_component(entity, cam);

    return entity;
}

void clear_render_entities(void_ecs::World& world) {
    // Query and despawn all renderable entities
    auto query = world.query_with<RenderableTag>();

    std::vector<void_ecs::Entity> to_despawn;
    for (auto iter = world.query_iter(query); !iter.empty(); iter.next()) {
        to_despawn.push_back(iter.entity());
    }

    for (auto entity : to_despawn) {
        world.despawn(entity);
    }
}

} // namespace void_render
