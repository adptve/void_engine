#pragma once

/// @file scene_instantiator.hpp
/// @brief Scene instantiation and hot-reload integration

#include "scene_data.hpp"
#include "scene_parser.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/ecs/world.hpp>
#include <void_engine/render/camera.hpp>
#include <void_engine/render/light.hpp>
#include <void_engine/render/material.hpp>

#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_scene {

// =============================================================================
// Scene Components (ECS components for scene entities)
// =============================================================================

/// Transform component for ECS entities
struct TransformComponent {
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
    std::array<float, 3> rotation = {0.0f, 0.0f, 0.0f};  // Euler angles (degrees)
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};

    /// Compute 4x4 transform matrix (column-major)
    [[nodiscard]] std::array<std::array<float, 4>, 4> matrix() const;
};

/// Mesh reference component (POD-safe for ECS storage)
struct MeshComponent {
    char mesh_name[64] = {};          // e.g., "sphere", "cube", "torus"
    char layer[32] = "world";
    bool visible = true;

    void set_mesh_name(const std::string& n) {
        std::strncpy(mesh_name, n.c_str(), sizeof(mesh_name) - 1);
        mesh_name[sizeof(mesh_name) - 1] = '\0';
    }

    void set_layer(const std::string& l) {
        std::strncpy(layer, l.c_str(), sizeof(layer) - 1);
        layer[sizeof(layer) - 1] = '\0';
    }
};

/// Material component (references scene material data)
struct MaterialComponent {
    void_render::GpuMaterial material;
};

/// Animation component for runtime animation state (POD-safe)
struct AnimationComponent {
    static constexpr std::size_t MAX_PATH_POINTS = 32;

    AnimationType type = AnimationType::None;
    std::array<float, 3> axis = {0.0f, 1.0f, 0.0f};
    float speed = 1.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float phase = 0.0f;

    // Orbit specific
    std::array<float, 3> center = {0.0f, 0.0f, 0.0f};
    float radius = 1.0f;
    float start_angle = 0.0f;
    bool face_center = false;

    // Pulse specific
    float min_scale = 1.0f;
    float max_scale = 1.0f;

    // Path animation (fixed-size array instead of vector)
    std::array<std::array<float, 3>, MAX_PATH_POINTS> points = {};
    std::size_t point_count = 0;
    float duration = 1.0f;
    bool loop = false;
    bool ping_pong = false;
    bool orient_to_path = false;

    // Runtime state
    float elapsed_time = 0.0f;
    std::size_t current_point = 0;
    bool reverse_direction = false;

    void add_point(const std::array<float, 3>& pt) {
        if (point_count < MAX_PATH_POINTS) {
            points[point_count++] = pt;
        }
    }
};

/// Camera component (POD-safe for ECS storage)
struct CameraComponent {
    char name[64] = {};
    bool active = false;
    float position[3] = {0.0f, 0.0f, 5.0f};
    float target[3] = {0.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect = 16.0f / 9.0f;
    bool is_perspective = true;

    void set_name(const std::string& n) {
        std::strncpy(name, n.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
};

/// Light component (POD-safe for ECS storage)
struct LightComponent {
    char name[64] = {};
    LightType type = LightType::Directional;
    bool enabled = true;

    // Light data (use appropriate based on type)
    void_render::GpuDirectionalLight directional;
    void_render::GpuPointLight point;
    void_render::GpuSpotLight spot;

    void set_name(const std::string& n) {
        std::strncpy(name, n.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
};

/// Particle emitter component (POD-safe for ECS storage)
struct ParticleEmitterComponent {
    char name[64] = {};
    std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
    float emit_rate = 100.0f;
    std::uint32_t max_particles = 1000;
    bool enabled = true;

    // Runtime state
    float emit_accumulator = 0.0f;

    void set_name(const std::string& n) {
        std::strncpy(name, n.c_str(), sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
};

/// Pickable component (for mouse interaction, POD-safe)
struct PickableComponent {
    bool enabled = true;
    int priority = 0;
    char bounds[32] = "mesh";
    bool highlight_on_hover = false;

    void set_bounds(const std::string& b) {
        std::strncpy(bounds, b.c_str(), sizeof(bounds) - 1);
        bounds[sizeof(bounds) - 1] = '\0';
    }
};

/// Scene tag component (marks entities as belonging to a scene, POD-safe)
struct SceneTagComponent {
    char scene_path[256] = {};
    char entity_name[64] = {};

    void set_scene_path(const std::filesystem::path& p) {
        std::strncpy(scene_path, p.string().c_str(), sizeof(scene_path) - 1);
        scene_path[sizeof(scene_path) - 1] = '\0';
    }

    void set_entity_name(const std::string& n) {
        std::strncpy(entity_name, n.c_str(), sizeof(entity_name) - 1);
        entity_name[sizeof(entity_name) - 1] = '\0';
    }
};

// =============================================================================
// SceneInstance
// =============================================================================

/// Represents an instantiated scene in the ECS world
class SceneInstance {
public:
    SceneInstance() = default;
    explicit SceneInstance(std::filesystem::path path)
        : m_scene_path(std::move(path)) {}

    /// Get scene path
    [[nodiscard]] const std::filesystem::path& path() const { return m_scene_path; }

    /// Get all entity IDs belonging to this scene
    [[nodiscard]] const std::vector<void_ecs::Entity>& entities() const { return m_entities; }

    /// Get camera entities
    [[nodiscard]] const std::vector<void_ecs::Entity>& cameras() const { return m_cameras; }

    /// Get light entities
    [[nodiscard]] const std::vector<void_ecs::Entity>& lights() const { return m_lights; }

    /// Add entity to scene
    void add_entity(void_ecs::Entity entity) { m_entities.push_back(entity); }

    /// Add camera entity
    void add_camera(void_ecs::Entity entity) {
        m_cameras.push_back(entity);
        m_entities.push_back(entity);
    }

    /// Add light entity
    void add_light(void_ecs::Entity entity) {
        m_lights.push_back(entity);
        m_entities.push_back(entity);
    }

    /// Clear all tracked entities
    void clear() {
        m_entities.clear();
        m_cameras.clear();
        m_lights.clear();
    }

private:
    std::filesystem::path m_scene_path;
    std::vector<void_ecs::Entity> m_entities;
    std::vector<void_ecs::Entity> m_cameras;
    std::vector<void_ecs::Entity> m_lights;
};

// =============================================================================
// SceneInstantiator
// =============================================================================

/// Instantiates scene data into ECS entities
class SceneInstantiator {
public:
    using EntityCreatedCallback = std::function<void(void_ecs::Entity, const EntityData&)>;
    using CameraCreatedCallback = std::function<void(void_ecs::Entity, const CameraData&)>;
    using LightCreatedCallback = std::function<void(void_ecs::Entity, const LightData&)>;

    SceneInstantiator() = default;
    explicit SceneInstantiator(void_ecs::World* world) : m_world(world) {}

    /// Set ECS world
    void set_world(void_ecs::World* world) { m_world = world; }

    /// Get ECS world
    [[nodiscard]] void_ecs::World* world() { return m_world; }
    [[nodiscard]] const void_ecs::World* world() const { return m_world; }

    /// Register component types with the world
    void register_components();

    /// Instantiate scene data into ECS entities
    [[nodiscard]] void_core::Result<SceneInstance> instantiate(
        const SceneData& scene,
        const std::filesystem::path& scene_path);

    /// Destroy all entities from a scene instance
    void destroy(SceneInstance& instance);

    /// Hot-reload: update existing scene instance with new data
    [[nodiscard]] void_core::Result<void> hot_reload(
        SceneInstance& instance,
        const SceneData& new_scene);

    /// Set callback for entity creation
    void on_entity_created(EntityCreatedCallback callback) {
        m_on_entity_created = std::move(callback);
    }

    /// Set callback for camera creation
    void on_camera_created(CameraCreatedCallback callback) {
        m_on_camera_created = std::move(callback);
    }

    /// Set callback for light creation
    void on_light_created(LightCreatedCallback callback) {
        m_on_light_created = std::move(callback);
    }

private:
    /// Create entity from EntityData
    void_ecs::Entity create_entity(const EntityData& data, const std::filesystem::path& scene_path);

    /// Create camera entity from CameraData
    void_ecs::Entity create_camera(const CameraData& data, const std::filesystem::path& scene_path);

    /// Create light entity from LightData
    void_ecs::Entity create_light(const LightData& data, const std::filesystem::path& scene_path);

    /// Convert scene TransformData to TransformComponent
    static TransformComponent convert_transform(const TransformData& data);

    /// Convert scene MaterialData to GpuMaterial
    static void_render::GpuMaterial convert_material(const MaterialData& data);

    /// Convert scene AnimationData to AnimationComponent
    static AnimationComponent convert_animation(const AnimationData& data);

    void_ecs::World* m_world = nullptr;
    EntityCreatedCallback m_on_entity_created;
    CameraCreatedCallback m_on_camera_created;
    LightCreatedCallback m_on_light_created;
};

// =============================================================================
// LiveSceneManager
// =============================================================================

/// Manages live scene instances with hot-reload support
class LiveSceneManager {
public:
    using SceneChangedCallback = std::function<void(const std::filesystem::path&, const SceneData&)>;

    LiveSceneManager() = default;
    explicit LiveSceneManager(void_ecs::World* world);

    // Non-copyable, movable
    LiveSceneManager(const LiveSceneManager&) = delete;
    LiveSceneManager& operator=(const LiveSceneManager&) = delete;
    LiveSceneManager(LiveSceneManager&&) = default;
    LiveSceneManager& operator=(LiveSceneManager&&) = default;

    /// Set ECS world (for post-construction initialization)
    void set_world(void_ecs::World* world) {
        m_world = world;
        m_instantiator.set_world(world);
    }

    /// Initialize the manager
    [[nodiscard]] void_core::Result<void> initialize();

    /// Shutdown and cleanup
    void shutdown();

    /// Load and instantiate a scene
    [[nodiscard]] void_core::Result<void> load_scene(const std::filesystem::path& path);

    /// Unload a scene (destroy its entities)
    void unload_scene(const std::filesystem::path& path);

    /// Unload all scenes
    void unload_all();

    /// Get current/active scene path
    [[nodiscard]] const std::filesystem::path& current_scene_path() const {
        return m_current_scene_path;
    }

    /// Get scene data
    [[nodiscard]] const SceneData* get_scene_data(const std::filesystem::path& path) const;

    /// Get scene instance
    [[nodiscard]] const SceneInstance* get_scene_instance(const std::filesystem::path& path) const;

    /// Set hot-reload enabled
    void set_hot_reload_enabled(bool enabled) { m_hot_reload_enabled = enabled; }

    /// Check if hot-reload is enabled
    [[nodiscard]] bool is_hot_reload_enabled() const { return m_hot_reload_enabled; }

    /// Update (poll for file changes, process hot-reload)
    void update(float delta_time);

    /// Force reload of a scene
    [[nodiscard]] void_core::Result<void> force_reload(const std::filesystem::path& path);

    /// Set callback for scene changes
    void on_scene_changed(SceneChangedCallback callback) {
        m_on_scene_changed = std::move(callback);
    }

    /// Get instantiator for custom entity setup
    [[nodiscard]] SceneInstantiator& instantiator() { return m_instantiator; }

    /// Get scene manager (for access to hot-reload system)
    [[nodiscard]] SceneManager& scene_manager() { return m_scene_manager; }

private:
    /// Handle scene reload callback
    void handle_scene_reload(const std::filesystem::path& path, const SceneData& data);

    void_ecs::World* m_world = nullptr;
    SceneManager m_scene_manager;
    SceneInstantiator m_instantiator;

    std::unordered_map<std::string, SceneInstance> m_instances;
    std::filesystem::path m_current_scene_path;
    bool m_hot_reload_enabled = true;

    SceneChangedCallback m_on_scene_changed;
};

// =============================================================================
// Animation System
// =============================================================================

/// Updates animation components
class AnimationSystem {
public:
    /// Update all animation components in the world
    static void update(void_ecs::World& world, float delta_time);

private:
    /// Update rotation animation
    static void update_rotation(TransformComponent& transform, AnimationComponent& anim, float dt);

    /// Update oscillation animation
    static void update_oscillation(TransformComponent& transform, AnimationComponent& anim, float dt);

    /// Update orbit animation
    static void update_orbit(TransformComponent& transform, AnimationComponent& anim, float dt);

    /// Update pulse animation
    static void update_pulse(TransformComponent& transform, AnimationComponent& anim, float dt);

    /// Update path animation
    static void update_path(TransformComponent& transform, AnimationComponent& anim, float dt);
};

} // namespace void_scene
