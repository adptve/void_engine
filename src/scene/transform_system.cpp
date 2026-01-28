/// @file transform_system.cpp
/// @brief ECS-authoritative transform system implementation

#include <void_engine/scene/transform_system.hpp>
#include <void_engine/scene/scene_instantiator.hpp>
#include <void_engine/ecs/query.hpp>

#include <chrono>
#include <cmath>
#include <numbers>

namespace void_scene {

// =============================================================================
// Transform Conversion
// =============================================================================

void_ecs::LocalTransform to_local_transform(const TransformComponent& scene_transform) {
    void_ecs::LocalTransform local;

    // Position
    local.position = void_ecs::Vec3{
        scene_transform.position[0],
        scene_transform.position[1],
        scene_transform.position[2]
    };

    // Rotation: Convert Euler degrees to quaternion
    constexpr float deg2rad = std::numbers::pi_v<float> / 180.0f;
    void_ecs::Vec3 euler{
        scene_transform.rotation[0] * deg2rad,
        scene_transform.rotation[1] * deg2rad,
        scene_transform.rotation[2] * deg2rad
    };
    local.rotation = void_ecs::Quat::from_euler(euler);

    // Scale
    local.scale = void_ecs::Vec3{
        scene_transform.scale[0],
        scene_transform.scale[1],
        scene_transform.scale[2]
    };

    return local;
}

TransformComponent from_local_transform(const void_ecs::LocalTransform& local) {
    TransformComponent scene_transform;

    // Position
    scene_transform.position[0] = local.position.x;
    scene_transform.position[1] = local.position.y;
    scene_transform.position[2] = local.position.z;

    // Rotation: Convert quaternion to Euler degrees
    // This is an approximation - quaternion to euler can have gimbal lock issues
    // Using standard conversion for XYZ order
    const auto& q = local.rotation;
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    float pitch;
    if (std::abs(sinp) >= 1.0f) {
        pitch = std::copysign(std::numbers::pi_v<float> / 2.0f, sinp);
    } else {
        pitch = std::asin(sinp);
    }

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    constexpr float rad2deg = 180.0f / std::numbers::pi_v<float>;
    scene_transform.rotation[0] = roll * rad2deg;
    scene_transform.rotation[1] = pitch * rad2deg;
    scene_transform.rotation[2] = yaw * rad2deg;

    // Scale
    scene_transform.scale[0] = local.scale.x;
    scene_transform.scale[1] = local.scale.y;
    scene_transform.scale[2] = local.scale.z;

    return scene_transform;
}

// =============================================================================
// TransformSyncSystem
// =============================================================================

void TransformSyncSystem::register_components(void_ecs::World& world) {
    world.register_component<TransformComponent>();
    world.register_component<void_ecs::LocalTransform>();
    world.register_component<void_ecs::GlobalTransform>();
    world.register_component<void_ecs::Parent>();
    world.register_component<void_ecs::Children>();
    world.register_component<void_ecs::HierarchyDepth>();
    world.register_component<void_ecs::Visible>();
    world.register_component<void_ecs::InheritedVisibility>();
}

void TransformSyncSystem::sync_scene_to_ecs(void_ecs::World& world) {
    // Get component IDs
    auto scene_transform_id = world.component_id<TransformComponent>();
    auto local_transform_id = world.component_id<void_ecs::LocalTransform>();

    if (!scene_transform_id) {
        // TransformComponent not registered - nothing to sync
        return;
    }

    // Ensure LocalTransform is registered
    if (!local_transform_id) {
        world.register_component<void_ecs::LocalTransform>();
        local_transform_id = world.component_id<void_ecs::LocalTransform>();
    }

    // Ensure GlobalTransform is registered (for propagation later)
    if (!world.component_id<void_ecs::GlobalTransform>()) {
        world.register_component<void_ecs::GlobalTransform>();
    }

    // Query all entities with TransformComponent
    void_ecs::QueryDescriptor desc;
    desc.read(*scene_transform_id).build();

    void_ecs::QueryState state(std::move(desc));
    world.update_query(state);

    void_ecs::QueryIter iter = world.query_iter(state);

    while (!iter.empty()) {
        void_ecs::Entity entity = iter.entity();

        // Get scene transform
        const TransformComponent* scene_transform = world.get_component<TransformComponent>(entity);
        if (scene_transform) {
            // Convert to ECS LocalTransform
            void_ecs::LocalTransform local = to_local_transform(*scene_transform);

            // Add or update LocalTransform
            if (world.has_component<void_ecs::LocalTransform>(entity)) {
                void_ecs::LocalTransform* existing = world.get_component<void_ecs::LocalTransform>(entity);
                if (existing) {
                    *existing = local;
                }
            } else {
                world.add_component(entity, local);
            }

            // Ensure GlobalTransform exists (will be computed by propagate_transforms)
            if (!world.has_component<void_ecs::GlobalTransform>(entity)) {
                world.add_component(entity, void_ecs::GlobalTransform::identity());
            }
        }

        iter.next();
    }
}

void TransformSyncSystem::sync_entity(void_ecs::World& world, void_ecs::Entity entity) {
    if (!world.is_alive(entity)) {
        return;
    }

    const TransformComponent* scene_transform = world.get_component<TransformComponent>(entity);
    if (!scene_transform) {
        return;
    }

    // Convert to ECS LocalTransform
    void_ecs::LocalTransform local = to_local_transform(*scene_transform);

    // Add or update LocalTransform
    if (world.has_component<void_ecs::LocalTransform>(entity)) {
        void_ecs::LocalTransform* existing = world.get_component<void_ecs::LocalTransform>(entity);
        if (existing) {
            *existing = local;
        }
    } else {
        world.add_component(entity, local);
    }

    // Ensure GlobalTransform exists
    if (!world.has_component<void_ecs::GlobalTransform>(entity)) {
        world.add_component(entity, void_ecs::GlobalTransform::identity());
    }
}

bool TransformSyncSystem::has_authoritative_transform(
    const void_ecs::World& world,
    void_ecs::Entity entity) {

    return world.has_component<void_ecs::GlobalTransform>(entity);
}

std::optional<void_ecs::Vec3> TransformSyncSystem::get_world_position(
    const void_ecs::World& world,
    void_ecs::Entity entity) {

    const void_ecs::GlobalTransform* global = world.get_component<void_ecs::GlobalTransform>(entity);
    if (!global) {
        return std::nullopt;
    }

    return global->position();
}

std::optional<void_ecs::Mat4> TransformSyncSystem::get_world_matrix(
    const void_ecs::World& world,
    void_ecs::Entity entity) {

    const void_ecs::GlobalTransform* global = world.get_component<void_ecs::GlobalTransform>(entity);
    if (!global) {
        return std::nullopt;
    }

    return global->matrix;
}

// =============================================================================
// RenderSceneGraph
// =============================================================================

void RenderSceneGraph::rebuild(const void_ecs::World& world) {
    auto start_time = std::chrono::steady_clock::now();

    // Clear existing data
    m_transforms.clear();
    m_entity_to_index.clear();
    m_visible_count = 0;

    // Get component IDs
    auto global_id = world.component_id<void_ecs::GlobalTransform>();
    if (!global_id) {
        m_dirty = false;
        return;
    }

    // Query all entities with GlobalTransform
    void_ecs::QueryDescriptor desc;
    desc.read(*global_id).build();

    void_ecs::QueryState state(std::move(desc));
    const_cast<void_ecs::World&>(world).update_query(state);

    void_ecs::QueryIter iter = const_cast<void_ecs::World&>(world).query_iter(state);

    while (!iter.empty()) {
        void_ecs::Entity entity = iter.entity();

        RenderTransformData data = extract_transform(world, entity);
        if (data.is_valid()) {
            m_entity_to_index[entity.to_bits()] = m_transforms.size();
            m_transforms.push_back(data);

            if (data.visible) {
                ++m_visible_count;
            }
        }

        iter.next();
    }

    m_dirty = false;
    ++m_rebuild_count;

    auto end_time = std::chrono::steady_clock::now();
    m_last_rebuild_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time).count();
}

void RenderSceneGraph::rebuild_entities(
    const void_ecs::World& world,
    const std::vector<void_ecs::Entity>& entities) {

    for (const auto& entity : entities) {
        if (!world.is_alive(entity)) {
            continue;
        }

        RenderTransformData data = extract_transform(world, entity);

        auto it = m_entity_to_index.find(entity.to_bits());
        if (it != m_entity_to_index.end()) {
            // Update existing entry
            bool was_visible = m_transforms[it->second].visible;
            m_transforms[it->second] = data;

            // Update visible count
            if (was_visible && !data.visible) {
                --m_visible_count;
            } else if (!was_visible && data.visible) {
                ++m_visible_count;
            }
        } else if (data.is_valid()) {
            // Add new entry
            m_entity_to_index[entity.to_bits()] = m_transforms.size();
            m_transforms.push_back(data);

            if (data.visible) {
                ++m_visible_count;
            }
        }
    }
}

void RenderSceneGraph::clear() {
    m_transforms.clear();
    m_entity_to_index.clear();
    m_visible_count = 0;
    m_dirty = true;
}

std::vector<RenderTransformData> RenderSceneGraph::visible_transforms() const {
    std::vector<RenderTransformData> visible;
    visible.reserve(m_visible_count);

    for (const auto& data : m_transforms) {
        if (data.visible) {
            visible.push_back(data);
        }
    }

    return visible;
}

const RenderTransformData* RenderSceneGraph::get_transform(void_ecs::Entity entity) const {
    auto it = m_entity_to_index.find(entity.to_bits());
    if (it != m_entity_to_index.end() && it->second < m_transforms.size()) {
        return &m_transforms[it->second];
    }
    return nullptr;
}

std::vector<std::size_t> RenderSceneGraph::cull_by_frustum(
    const void_ecs::Mat4& view_projection,
    float near_plane,
    float far_plane) const {

    std::vector<std::size_t> visible_indices;
    visible_indices.reserve(m_visible_count);

    // Simple frustum culling based on world position
    // For production, you'd want proper AABB frustum tests
    for (std::size_t i = 0; i < m_transforms.size(); ++i) {
        const auto& data = m_transforms[i];
        if (!data.visible) {
            continue;
        }

        // Transform position to clip space
        void_ecs::Vec3 pos = data.world_position;
        float clip_x = view_projection.m[0] * pos.x + view_projection.m[4] * pos.y +
                       view_projection.m[8] * pos.z + view_projection.m[12];
        float clip_y = view_projection.m[1] * pos.x + view_projection.m[5] * pos.y +
                       view_projection.m[9] * pos.z + view_projection.m[13];
        float clip_z = view_projection.m[2] * pos.x + view_projection.m[6] * pos.y +
                       view_projection.m[10] * pos.z + view_projection.m[14];
        float clip_w = view_projection.m[3] * pos.x + view_projection.m[7] * pos.y +
                       view_projection.m[11] * pos.z + view_projection.m[15];

        // Simple clip test (point in frustum)
        if (clip_w > 0 &&
            clip_x >= -clip_w && clip_x <= clip_w &&
            clip_y >= -clip_w && clip_y <= clip_w &&
            clip_z >= 0 && clip_z <= clip_w) {
            visible_indices.push_back(i);
        }
    }

    return visible_indices;
}

std::vector<const RenderTransformData*> RenderSceneGraph::get_by_layer(
    const void_ecs::World& world,
    const std::string& layer) const {

    std::vector<const RenderTransformData*> result;

    // Get MeshComponent ID if registered
    auto mesh_id = world.component_id<MeshComponent>();
    if (!mesh_id) {
        return result;
    }

    for (const auto& data : m_transforms) {
        const MeshComponent* mesh = world.get_component<MeshComponent>(data.entity);
        if (mesh && std::string(mesh->layer) == layer) {
            result.push_back(&data);
        }
    }

    return result;
}

RenderTransformData RenderSceneGraph::extract_transform(
    const void_ecs::World& world,
    void_ecs::Entity entity) const {

    RenderTransformData data;
    data.entity = entity;

    // Get GlobalTransform (authoritative)
    const void_ecs::GlobalTransform* global = world.get_component<void_ecs::GlobalTransform>(entity);
    if (!global) {
        data.entity = void_ecs::Entity::null();
        return data;
    }

    data.world_matrix = global->matrix;
    data.world_position = global->position();

    // Check visibility
    const void_ecs::InheritedVisibility* inherited_vis =
        world.get_component<void_ecs::InheritedVisibility>(entity);
    if (inherited_vis) {
        data.visible = inherited_vis->visible;
    } else {
        const void_ecs::Visible* vis = world.get_component<void_ecs::Visible>(entity);
        data.visible = vis ? vis->visible : true;
    }

    // Also check MeshComponent visibility if present
    const MeshComponent* mesh = world.get_component<MeshComponent>(entity);
    if (mesh) {
        data.visible = data.visible && mesh->visible;
    }

    return data;
}

} // namespace void_scene
