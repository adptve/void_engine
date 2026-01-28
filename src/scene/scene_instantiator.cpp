/// @file scene_instantiator.cpp
/// @brief Scene instantiation implementation

#include <void_engine/scene/scene_instantiator.hpp>
#include <void_engine/scene/transform_system.hpp>
#include <void_engine/ecs/hierarchy.hpp>

#include <cmath>
#include <numbers>

namespace void_scene {

// =============================================================================
// TransformComponent
// =============================================================================

std::array<std::array<float, 4>, 4> TransformComponent::matrix() const {
    // Convert degrees to radians
    const float deg2rad = std::numbers::pi_v<float> / 180.0f;
    float rx = rotation[0] * deg2rad;
    float ry = rotation[1] * deg2rad;
    float rz = rotation[2] * deg2rad;

    float cx = std::cos(rx), sx = std::sin(rx);
    float cy = std::cos(ry), sy = std::sin(ry);
    float cz = std::cos(rz), sz = std::sin(rz);

    // Rotation matrix (YXZ order like most game engines)
    std::array<std::array<float, 4>, 4> m = {};

    // Row 0
    m[0][0] = (cy * cz + sy * sx * sz) * scale[0];
    m[0][1] = (cz * sy * sx - cy * sz) * scale[0];
    m[0][2] = (cx * sy) * scale[0];
    m[0][3] = 0.0f;

    // Row 1
    m[1][0] = (cx * sz) * scale[1];
    m[1][1] = (cx * cz) * scale[1];
    m[1][2] = (-sx) * scale[1];
    m[1][3] = 0.0f;

    // Row 2
    m[2][0] = (cy * sx * sz - cz * sy) * scale[2];
    m[2][1] = (cy * cz * sx + sy * sz) * scale[2];
    m[2][2] = (cx * cy) * scale[2];
    m[2][3] = 0.0f;

    // Row 3 (translation)
    m[3][0] = position[0];
    m[3][1] = position[1];
    m[3][2] = position[2];
    m[3][3] = 1.0f;

    return m;
}

// =============================================================================
// SceneInstantiator
// =============================================================================

void SceneInstantiator::register_components() {
    if (!m_world) return;

    // Scene-level components
    m_world->register_component<TransformComponent>();
    m_world->register_component<MeshComponent>();
    m_world->register_component<MaterialComponent>();
    m_world->register_component<AnimationComponent>();
    m_world->register_component<CameraComponent>();
    m_world->register_component<LightComponent>();
    m_world->register_component<ParticleEmitterComponent>();
    m_world->register_component<PickableComponent>();
    m_world->register_component<SceneTagComponent>();

    // ECS hierarchy components (authoritative transforms)
    // These are registered by TransformSyncSystem but we ensure they exist
    TransformSyncSystem::register_components(*m_world);
}

void_core::Result<SceneInstance> SceneInstantiator::instantiate(
    const SceneData& scene,
    const std::filesystem::path& scene_path)
{
    if (!m_world) {
        return void_core::Err<SceneInstance>(void_core::Error("No ECS world set"));
    }

    SceneInstance instance(scene_path);

    // Create camera entities
    for (const auto& camera_data : scene.cameras) {
        auto entity = create_camera(camera_data, scene_path);
        instance.add_camera(entity);
    }

    // Create light entities
    for (const auto& light_data : scene.lights) {
        auto entity = create_light(light_data, scene_path);
        instance.add_light(entity);
    }

    // Create mesh entities
    for (const auto& entity_data : scene.entities) {
        auto entity = create_entity(entity_data, scene_path);
        instance.add_entity(entity);
    }

    return instance;
}

void SceneInstantiator::destroy(SceneInstance& instance) {
    if (!m_world) return;

    for (auto entity : instance.entities()) {
        m_world->despawn(entity);
    }

    instance.clear();
}

void_core::Result<void> SceneInstantiator::hot_reload(
    SceneInstance& instance,
    const SceneData& new_scene)
{
    if (!m_world) {
        return void_core::Err(void_core::Error("No ECS world set"));
    }

    // Strategy: destroy all entities and recreate
    // Future optimization: diff the scenes and update in-place
    auto path = instance.path();
    destroy(instance);

    auto result = instantiate(new_scene, path);
    if (!result) {
        return void_core::Err(result.error());
    }

    instance = std::move(*result);
    return void_core::Ok();
}

void_ecs::Entity SceneInstantiator::create_entity(
    const EntityData& data,
    const std::filesystem::path& scene_path)
{
    auto entity = m_world->spawn();

    // Add scene tag
    SceneTagComponent tag;
    tag.set_scene_path(scene_path);
    tag.set_entity_name(data.name);
    m_world->add_component(entity, tag);

    // Add scene transform (for animation and backwards compatibility)
    auto transform = convert_transform(data.transform);
    m_world->add_component(entity, transform);

    // Add ECS authoritative transform (LocalTransform + GlobalTransform)
    // This makes ECS the single source of truth for world-space transforms
    void_ecs::LocalTransform local = to_local_transform(transform);
    m_world->add_component(entity, local);
    m_world->add_component(entity, void_ecs::GlobalTransform{local.to_matrix()});

    // Add visibility components
    m_world->add_component(entity, void_ecs::Visible{data.visible});
    m_world->add_component(entity, void_ecs::InheritedVisibility{data.visible});

    // Add mesh component
    MeshComponent mesh;
    mesh.set_mesh_name(data.mesh);
    mesh.set_layer(data.layer);
    mesh.visible = data.visible;
    m_world->add_component(entity, mesh);

    // Add material if present
    if (data.material) {
        MaterialComponent mat;
        mat.material = convert_material(*data.material);
        m_world->add_component(entity, mat);
    }

    // Add animation if present
    if (data.animation && data.animation->type != AnimationType::None) {
        auto anim = convert_animation(*data.animation);
        m_world->add_component(entity, anim);
    }

    // Add pickable if present
    if (data.pickable) {
        PickableComponent pickable;
        pickable.enabled = data.pickable->enabled;
        pickable.priority = data.pickable->priority;
        pickable.set_bounds(data.pickable->bounds);
        pickable.highlight_on_hover = data.pickable->highlight_on_hover;
        m_world->add_component(entity, pickable);
    }

    // Invoke callback
    if (m_on_entity_created) {
        m_on_entity_created(entity, data);
    }

    return entity;
}

void_ecs::Entity SceneInstantiator::create_camera(
    const CameraData& data,
    const std::filesystem::path& scene_path)
{
    auto entity = m_world->spawn();

    // Add scene tag
    SceneTagComponent tag;
    tag.set_scene_path(scene_path);
    tag.set_entity_name(data.name);
    m_world->add_component(entity, tag);

    // Create camera component (POD-safe)
    CameraComponent cam;
    cam.set_name(data.name);
    cam.active = data.active;
    cam.position[0] = data.transform.position[0];
    cam.position[1] = data.transform.position[1];
    cam.position[2] = data.transform.position[2];
    cam.target[0] = data.transform.target[0];
    cam.target[1] = data.transform.target[1];
    cam.target[2] = data.transform.target[2];
    cam.up[0] = data.transform.up[0];
    cam.up[1] = data.transform.up[1];
    cam.up[2] = data.transform.up[2];

    // Set projection
    if (data.type == CameraType::Perspective) {
        cam.is_perspective = true;
        cam.fov = data.perspective.fov;
        cam.near_plane = data.perspective.near_plane;
        cam.far_plane = data.perspective.far_plane;
    } else {
        cam.is_perspective = false;
        cam.near_plane = data.orthographic.near_plane;
        cam.far_plane = data.orthographic.far_plane;
    }

    m_world->add_component(entity, cam);

    // Invoke callback
    if (m_on_camera_created) {
        m_on_camera_created(entity, data);
    }

    return entity;
}

void_ecs::Entity SceneInstantiator::create_light(
    const LightData& data,
    const std::filesystem::path& scene_path)
{
    auto entity = m_world->spawn();

    // Add scene tag
    SceneTagComponent tag;
    tag.set_scene_path(scene_path);
    tag.set_entity_name(data.name);
    m_world->add_component(entity, tag);

    // Create light component
    LightComponent light;
    light.set_name(data.name);
    light.type = data.type;
    light.enabled = data.enabled;

    switch (data.type) {
        case LightType::Directional: {
            light.directional.direction = data.directional.direction;
            light.directional.color = data.directional.color;
            light.directional.intensity = data.directional.intensity;
            light.directional.shadow_map_index = data.directional.cast_shadows ? 0 : -1;
            light.directional.normalize_direction();
            break;
        }
        case LightType::Point: {
            light.point.position = data.point.position;
            light.point.color = data.point.color;
            light.point.intensity = data.point.intensity;
            light.point.range = data.point.range;
            light.point.set_attenuation(
                data.point.attenuation.constant,
                data.point.attenuation.linear,
                data.point.attenuation.quadratic);
            light.point.shadow_map_index = data.point.cast_shadows ? 0 : -1;
            break;
        }
        case LightType::Spot: {
            light.spot.position = data.spot.position;
            light.spot.direction = data.spot.direction;
            light.spot.color = data.spot.color;
            light.spot.intensity = data.spot.intensity;
            light.spot.range = data.spot.range;
            light.spot.set_cone_angles(data.spot.inner_angle, data.spot.outer_angle);
            light.spot.normalize_direction();
            light.spot.shadow_map_index = data.spot.cast_shadows ? 0 : -1;
            break;
        }
    }

    m_world->add_component(entity, light);

    // Invoke callback
    if (m_on_light_created) {
        m_on_light_created(entity, data);
    }

    return entity;
}

TransformComponent SceneInstantiator::convert_transform(const TransformData& data) {
    TransformComponent t;
    t.position = data.position;
    t.rotation = data.rotation;
    t.scale = data.scale_vec3();
    return t;
}

void_render::GpuMaterial SceneInstantiator::convert_material(const MaterialData& data) {
    void_render::GpuMaterial mat = void_render::GpuMaterial::pbr_default();

    // Albedo
    if (data.albedo.has_color()) {
        const auto& c = *data.albedo.color;
        mat.base_color = {c[0], c[1], c[2], c[3]};
    }

    // Metallic
    if (data.metallic.has_value()) {
        mat.metallic = *data.metallic.value;
    }

    // Roughness
    if (data.roughness.has_value()) {
        mat.roughness = *data.roughness.value;
    }

    // Emissive
    if (data.emissive) {
        mat.emissive = *data.emissive;
    }

    // Transmission (glass-like)
    if (data.transmission) {
        mat.transmission = data.transmission->factor;
        mat.ior = data.transmission->ior;
        mat.thickness = data.transmission->thickness;
        mat.attenuation_color = data.transmission->attenuation_color;
        mat.attenuation_distance = data.transmission->attenuation_distance;
        mat.set_flag(void_render::GpuMaterial::FLAG_HAS_TRANSMISSION);
    }

    // Sheen (fabric/velvet)
    if (data.sheen) {
        mat.sheen = 1.0f;
        mat.sheen_color = data.sheen->color;
        mat.sheen_roughness = data.sheen->roughness;
        mat.set_flag(void_render::GpuMaterial::FLAG_HAS_SHEEN);
    }

    // Clearcoat (car paint)
    if (data.clearcoat) {
        mat.clearcoat = data.clearcoat->intensity;
        mat.clearcoat_roughness = data.clearcoat->roughness;
        mat.set_flag(void_render::GpuMaterial::FLAG_HAS_CLEARCOAT);
    }

    // Anisotropy
    if (data.anisotropy) {
        mat.anisotropy = data.anisotropy->strength;
        mat.anisotropy_rotation = data.anisotropy->rotation;
        mat.set_flag(void_render::GpuMaterial::FLAG_HAS_ANISOTROPY);
    }

    return mat;
}

AnimationComponent SceneInstantiator::convert_animation(const AnimationData& data) {
    AnimationComponent anim;

    anim.type = data.type;
    anim.axis = data.axis;
    anim.speed = data.speed;
    anim.amplitude = data.amplitude;
    anim.frequency = data.frequency;
    anim.phase = data.phase;

    // Orbit
    anim.center = data.center;
    anim.radius = data.radius;
    anim.start_angle = data.start_angle;
    anim.face_center = data.face_center;

    // Pulse
    anim.min_scale = data.min_scale;
    anim.max_scale = data.max_scale;

    // Path
    for (const auto& pt : data.points) {
        anim.add_point(pt);
    }
    anim.duration = data.duration;
    anim.loop = data.loop_animation;
    anim.ping_pong = data.ping_pong;
    anim.orient_to_path = data.orient_to_path;

    return anim;
}

// =============================================================================
// LiveSceneManager
// =============================================================================

LiveSceneManager::LiveSceneManager(void_ecs::World* world)
    : m_world(world)
    , m_instantiator(world)
{
}

void_core::Result<void> LiveSceneManager::initialize() {
    // Initialize underlying scene manager (file watcher, etc.)
    auto result = m_scene_manager.initialize();
    if (!result) {
        return result;
    }

    // Register ECS components
    if (m_world) {
        m_instantiator.set_world(m_world);
        m_instantiator.register_components();
    }

    // Set up hot-reload callback
    m_scene_manager.on_scene_loaded([this](const std::filesystem::path& path, const SceneData& data) {
        handle_scene_reload(path, data);
    });

    return void_core::Ok();
}

void LiveSceneManager::shutdown() {
    unload_all();
    m_scene_manager.shutdown();
}

void_core::Result<void> LiveSceneManager::load_scene(const std::filesystem::path& path) {
    // Load and parse the scene file
    auto result = m_scene_manager.load_scene(path);
    if (!result) {
        return result;
    }

    // Get the parsed scene data
    const auto* scene_data = m_scene_manager.current_scene();
    if (!scene_data) {
        return void_core::Err(void_core::Error("Scene data not available after load"));
    }

    // Instantiate into ECS
    auto instance_result = m_instantiator.instantiate(*scene_data, path);
    if (!instance_result) {
        return void_core::Err(instance_result.error());
    }

    // Store the instance
    std::string key = path.string();
    m_instances[key] = std::move(*instance_result);
    m_current_scene_path = path;

    // Invoke callback
    if (m_on_scene_changed) {
        m_on_scene_changed(path, *scene_data);
    }

    return void_core::Ok();
}

void LiveSceneManager::unload_scene(const std::filesystem::path& path) {
    std::string key = path.string();
    auto it = m_instances.find(key);
    if (it != m_instances.end()) {
        m_instantiator.destroy(it->second);
        m_instances.erase(it);
    }

    if (m_current_scene_path == path) {
        m_current_scene_path.clear();
    }
}

void LiveSceneManager::unload_all() {
    for (auto& [key, instance] : m_instances) {
        m_instantiator.destroy(instance);
    }
    m_instances.clear();
    m_current_scene_path.clear();
}

const SceneData* LiveSceneManager::get_scene_data(const std::filesystem::path& path) const {
    return m_scene_manager.get_scene(path);
}

const SceneInstance* LiveSceneManager::get_scene_instance(const std::filesystem::path& path) const {
    std::string key = path.string();
    auto it = m_instances.find(key);
    if (it != m_instances.end()) {
        return &it->second;
    }
    return nullptr;
}

void LiveSceneManager::update(float delta_time) {
    (void)delta_time;

    // Poll for file changes
    if (m_hot_reload_enabled) {
        m_scene_manager.update();
    }
}

void_core::Result<void> LiveSceneManager::force_reload(const std::filesystem::path& path) {
    // Get HotReloadableScene and force reload
    // This will trigger the callback which updates the instance
    const auto* scene_data = m_scene_manager.get_scene(path);
    if (!scene_data) {
        return void_core::Err(void_core::Error("Scene not loaded: " + path.string()));
    }

    // Re-parse the scene
    SceneParser parser;
    auto result = parser.parse(path);
    if (!result) {
        return void_core::Err(result.error());
    }

    // Update the instance
    handle_scene_reload(path, *result);

    return void_core::Ok();
}

void LiveSceneManager::handle_scene_reload(
    const std::filesystem::path& path,
    const SceneData& data)
{
    std::string key = path.string();
    auto it = m_instances.find(key);
    if (it != m_instances.end()) {
        // Hot-reload existing instance
        auto result = m_instantiator.hot_reload(it->second, data);
        if (!result) {
            // Log error but don't fail
            return;
        }
    }

    // Invoke callback
    if (m_on_scene_changed) {
        m_on_scene_changed(path, data);
    }
}

// =============================================================================
// AnimationSystem
// =============================================================================

void AnimationSystem::update(void_ecs::World& world, float delta_time) {
    // Get component IDs (register if not yet registered)
    auto transform_id_opt = world.component_id<TransformComponent>();
    auto anim_id_opt = world.component_id<AnimationComponent>();

    // If components aren't registered yet, nothing to animate
    if (!transform_id_opt || !anim_id_opt) {
        return;
    }

    void_ecs::ComponentId transform_id = *transform_id_opt;
    void_ecs::ComponentId anim_id = *anim_id_opt;

    // Build query: write access to both Transform and Animation
    void_ecs::QueryDescriptor desc;
    desc.write(transform_id)
        .write(anim_id)
        .build();

    // Create and update query state
    void_ecs::QueryState state(std::move(desc));
    world.update_query(state);

    // Collect entities that were animated (for ECS sync)
    std::vector<void_ecs::Entity> animated_entities;

    // Iterate over all matching entities
    void_ecs::QueryIter iter = world.query_iter(state);

    while (!iter.empty()) {
        // Get mutable access to archetype (need const_cast since QueryIter is const)
        void_ecs::Archetype* arch = const_cast<void_ecs::Archetype*>(iter.archetype());
        if (!arch) {
            iter.next();
            continue;
        }

        std::size_t row = iter.row();
        void_ecs::Entity entity = iter.entity();

        // Get component pointers
        TransformComponent* transform = arch->get_component<TransformComponent>(transform_id, row);
        AnimationComponent* anim = arch->get_component<AnimationComponent>(anim_id, row);

        if (transform && anim && anim->type != AnimationType::None) {
            // Update based on animation type
            switch (anim->type) {
                case AnimationType::Rotate:
                    update_rotation(*transform, *anim, delta_time);
                    break;
                case AnimationType::Oscillate:
                    update_oscillation(*transform, *anim, delta_time);
                    break;
                case AnimationType::Orbit:
                    update_orbit(*transform, *anim, delta_time);
                    break;
                case AnimationType::Pulse:
                    update_pulse(*transform, *anim, delta_time);
                    break;
                case AnimationType::Path:
                    update_path(*transform, *anim, delta_time);
                    break;
                case AnimationType::None:
                    break;
            }

            animated_entities.push_back(entity);
        }

        iter.next();
    }

    // Sync animated transforms to ECS (keeps ECS authoritative)
    for (void_ecs::Entity entity : animated_entities) {
        TransformSyncSystem::sync_entity(world, entity);
    }
}

void AnimationSystem::update_rotation(
    TransformComponent& transform,
    AnimationComponent& anim,
    float dt)
{
    anim.elapsed_time += dt;

    // Rotate around axis at specified speed (degrees per second)
    float angle_delta = anim.speed * dt;

    transform.rotation[0] += anim.axis[0] * angle_delta;
    transform.rotation[1] += anim.axis[1] * angle_delta;
    transform.rotation[2] += anim.axis[2] * angle_delta;

    // Keep angles in reasonable range
    for (int i = 0; i < 3; ++i) {
        while (transform.rotation[i] > 360.0f) transform.rotation[i] -= 360.0f;
        while (transform.rotation[i] < 0.0f) transform.rotation[i] += 360.0f;
    }
}

void AnimationSystem::update_oscillation(
    TransformComponent& transform,
    AnimationComponent& anim,
    float dt)
{
    anim.elapsed_time += dt;

    float wave = std::sin(anim.elapsed_time * anim.frequency * 2.0f * std::numbers::pi_v<float> + anim.phase);
    float offset = wave * anim.amplitude;

    // Apply oscillation along axis
    transform.position[0] += anim.axis[0] * offset;
    transform.position[1] += anim.axis[1] * offset;
    transform.position[2] += anim.axis[2] * offset;
}

void AnimationSystem::update_orbit(
    TransformComponent& transform,
    AnimationComponent& anim,
    float dt)
{
    anim.elapsed_time += dt;

    float angle = anim.start_angle + anim.elapsed_time * anim.speed;

    transform.position[0] = anim.center[0] + std::cos(angle) * anim.radius;
    transform.position[1] = anim.center[1];  // Orbit in XZ plane by default
    transform.position[2] = anim.center[2] + std::sin(angle) * anim.radius;

    if (anim.face_center) {
        // Face toward center
        float look_angle = angle + std::numbers::pi_v<float>;
        transform.rotation[1] = look_angle * 180.0f / std::numbers::pi_v<float>;
    }
}

void AnimationSystem::update_pulse(
    TransformComponent& transform,
    AnimationComponent& anim,
    float dt)
{
    anim.elapsed_time += dt;

    float t = (std::sin(anim.elapsed_time * anim.frequency * 2.0f * std::numbers::pi_v<float>) + 1.0f) * 0.5f;
    float scale = anim.min_scale + t * (anim.max_scale - anim.min_scale);

    transform.scale = {scale, scale, scale};
}

void AnimationSystem::update_path(
    TransformComponent& transform,
    AnimationComponent& anim,
    float dt)
{
    if (anim.points.size() < 2) return;

    anim.elapsed_time += dt;

    float total_duration = anim.duration;
    float normalized_time = std::fmod(anim.elapsed_time, total_duration) / total_duration;

    if (anim.ping_pong && anim.reverse_direction) {
        normalized_time = 1.0f - normalized_time;
    }

    // Find segment
    float segment_length = 1.0f / static_cast<float>(anim.points.size() - 1);
    std::size_t segment = static_cast<std::size_t>(normalized_time / segment_length);
    segment = std::min(segment, anim.points.size() - 2);

    float local_t = (normalized_time - segment * segment_length) / segment_length;

    // Linear interpolation between points
    const auto& p0 = anim.points[segment];
    const auto& p1 = anim.points[segment + 1];

    transform.position[0] = p0[0] + (p1[0] - p0[0]) * local_t;
    transform.position[1] = p0[1] + (p1[1] - p0[1]) * local_t;
    transform.position[2] = p0[2] + (p1[2] - p0[2]) * local_t;

    // Handle loop/ping-pong
    if (anim.elapsed_time >= total_duration) {
        if (anim.ping_pong) {
            anim.reverse_direction = !anim.reverse_direction;
        }
        if (!anim.loop && !anim.ping_pong) {
            anim.elapsed_time = total_duration;
        }
    }
}

} // namespace void_scene
