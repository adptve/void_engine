/// @file render_systems.hpp
/// @brief ECS systems for rendering
///
/// These systems integrate with the kernel's stage scheduler:
/// - ModelLoaderSystem: Processes ModelComponent, loads assets (Update stage)
/// - TransformSystem: Updates world matrices from hierarchy (Update stage)
/// - AnimationSystem: Updates animations (Update stage)
/// - RenderPrepareSystem: Builds draw lists (RenderPrepare stage)
/// - RenderSystem: Executes draw calls (Render stage)

#pragma once

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/system.hpp>
#include <void_engine/render/components.hpp>
#include <void_engine/render/render_assets.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace void_render {

// Forward declarations
class RenderContext;

// =============================================================================
// Draw Command
// =============================================================================

/// @brief A single draw command for the render queue
struct DrawCommand {
    GpuMesh* mesh = nullptr;
    GpuShader* shader = nullptr;

    // Transform
    std::array<float, 16> model_matrix;
    std::array<float, 9> normal_matrix;

    // Material
    std::array<float, 4> albedo = {0.8f, 0.8f, 0.8f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    std::array<float, 3> emissive = {0.0f, 0.0f, 0.0f};
    float emissive_strength = 0.0f;

    // Textures
    GpuTexture* albedo_texture = nullptr;
    GpuTexture* normal_texture = nullptr;
    GpuTexture* metallic_roughness_texture = nullptr;

    // Sorting key
    std::uint64_t sort_key = 0;

    // Flags
    bool double_sided = false;
    bool alpha_blend = false;
};

/// @brief Light data for GPU upload
struct LightData {
    std::array<float, 3> position;
    float _pad0;
    std::array<float, 3> direction;
    float _pad1;
    std::array<float, 3> color;
    float intensity;
    float range;
    float inner_cone;
    float outer_cone;
    std::int32_t type;  // 0=dir, 1=point, 2=spot
};

/// @brief Camera data for rendering
struct CameraData {
    std::array<float, 16> view_matrix;
    std::array<float, 16> projection_matrix;
    std::array<float, 16> view_projection;
    std::array<float, 3> position;
    float _pad0;
    float near_plane;
    float far_plane;
    float fov;
    float aspect;
};

// =============================================================================
// RenderQueue
// =============================================================================

/// @brief Sorted queue of draw commands ready for execution
class RenderQueue {
public:
    /// Clear all commands
    void clear();

    /// Add a draw command
    void push(DrawCommand cmd);

    /// Sort commands by sort key
    void sort();

    /// Get commands
    [[nodiscard]] const std::vector<DrawCommand>& commands() const { return m_commands; }

    /// Get command count
    [[nodiscard]] std::size_t size() const { return m_commands.size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const { return m_commands.empty(); }

private:
    std::vector<DrawCommand> m_commands;
};

// =============================================================================
// RenderContext (ECS Resource)
// =============================================================================

/// @brief Shared render context as ECS resource
///
/// Contains GPU state, asset manager, and frame data.
/// Registered as a resource in void_ecs::World.
class RenderContext {
public:
    RenderContext();
    ~RenderContext();

    // Non-copyable
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    // Movable (required for ECS resource storage)
    RenderContext(RenderContext&&) noexcept = default;
    RenderContext& operator=(RenderContext&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /// @brief Initialize with window dimensions
    void_core::Result<void> initialize(std::uint32_t width, std::uint32_t height);

    /// @brief Shutdown
    void shutdown();

    /// @brief Handle window resize
    void on_resize(std::uint32_t width, std::uint32_t height);

    // =========================================================================
    // Asset Manager
    // =========================================================================

    /// @brief Get asset manager
    [[nodiscard]] RenderAssetManager& assets() { return *m_assets; }
    [[nodiscard]] const RenderAssetManager& assets() const { return *m_assets; }

    // =========================================================================
    // Frame State
    // =========================================================================

    /// @brief Get current camera data
    [[nodiscard]] const CameraData& camera_data() const { return m_camera_data; }

    /// @brief Set camera data for current frame
    void set_camera_data(const CameraData& data) { m_camera_data = data; }

    /// @brief Get light data for current frame
    [[nodiscard]] const std::vector<LightData>& lights() const { return m_lights; }

    /// @brief Clear lights for new frame
    void clear_lights() { m_lights.clear(); }

    /// @brief Add light for current frame
    void add_light(const LightData& light) { m_lights.push_back(light); }

    /// @brief Get main render queue
    [[nodiscard]] RenderQueue& render_queue() { return m_render_queue; }
    [[nodiscard]] const RenderQueue& render_queue() const { return m_render_queue; }

    // =========================================================================
    // Window
    // =========================================================================

    [[nodiscard]] std::uint32_t width() const { return m_width; }
    [[nodiscard]] std::uint32_t height() const { return m_height; }
    [[nodiscard]] float aspect_ratio() const {
        return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        std::uint32_t draw_calls = 0;
        std::uint32_t triangles = 0;
        std::uint32_t entities_rendered = 0;
        std::uint32_t entities_culled = 0;
        float frame_time_ms = 0.0f;
    };

    [[nodiscard]] const Stats& stats() const { return m_stats; }
    void reset_stats() { m_stats = {}; }
    void add_draw_call(std::uint32_t triangles) {
        m_stats.draw_calls++;
        m_stats.triangles += triangles;
    }

private:
    std::unique_ptr<RenderAssetManager> m_assets;

    std::uint32_t m_width = 1280;
    std::uint32_t m_height = 720;

    CameraData m_camera_data;
    std::vector<LightData> m_lights;
    RenderQueue m_render_queue;
    Stats m_stats;
};

// =============================================================================
// ModelLoaderSystem
// =============================================================================

/// @brief System that processes ModelComponent and loads assets
///
/// Queries entities with ModelComponent and:
/// - Triggers asset load if state is Unloaded
/// - Creates child entities for multi-mesh models
/// - Updates MeshComponent handles when model loads
class ModelLoaderSystem {
public:
    /// @brief System descriptor
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();

    /// @brief Run the system
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// TransformSystem
// =============================================================================

/// @brief System that updates world matrices from local transforms
///
/// Processes HierarchyComponent to propagate transforms down the tree.
/// Must run before RenderPrepareSystem.
class TransformSystem {
public:
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// AnimationSystem
// =============================================================================

/// @brief System that updates AnimationComponent state
class AnimationSystem {
public:
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// CameraSystem
// =============================================================================

/// @brief System that processes CameraComponent and updates RenderContext
class CameraSystem {
public:
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// LightSystem
// =============================================================================

/// @brief System that collects LightComponent data for rendering
class LightSystem {
public:
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// RenderPrepareSystem
// =============================================================================

/// @brief System that builds the render queue from entities
///
/// Queries entities with:
/// - RenderableTag (visible)
/// - TransformComponent
/// - MeshComponent
/// - MaterialComponent (optional)
///
/// Creates DrawCommands and sorts them for optimal rendering.
class RenderPrepareSystem {
public:
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// RenderSystem
// =============================================================================

/// @brief System that executes draw commands
///
/// Consumes the render queue built by RenderPrepareSystem and
/// issues OpenGL draw calls.
class RenderSystem {
public:
    [[nodiscard]] static void_ecs::SystemDescriptor descriptor();
    static void run(void_ecs::World& world, float delta_time);
};

// =============================================================================
// System Registration
// =============================================================================

/// @brief Register all render systems with the kernel
///
/// Call this during engine initialization to set up the render pipeline.
/// Systems are registered with appropriate stages and priorities.
void register_render_systems(void_ecs::World& world);

// =============================================================================
// Entity Spawning Helpers
// =============================================================================

/// @brief Spawn a renderable entity with transform, mesh, and material
[[nodiscard]] void_ecs::Entity spawn_renderable(
    void_ecs::World& world,
    const std::string& mesh_name,  // Built-in mesh name
    const MaterialComponent& material = MaterialComponent::pbr_default());

/// @brief Spawn a model entity that will load from path
[[nodiscard]] void_ecs::Entity spawn_model(
    void_ecs::World& world,
    const std::string& model_path,
    const ModelLoadOptions& options = ModelLoadOptions::defaults());

/// @brief Spawn a light entity
[[nodiscard]] void_ecs::Entity spawn_light(
    void_ecs::World& world,
    const LightComponent& light);

/// @brief Spawn a camera entity
[[nodiscard]] void_ecs::Entity spawn_camera(
    void_ecs::World& world,
    const CameraComponent& camera,
    bool make_active = true);

// =============================================================================
// Scene Loading
// =============================================================================

/// @brief Load scene JSON into ECS world
///
/// Parses scene.json and spawns entities with appropriate components.
/// Supports hot-reload - scene file changes trigger entity updates.
void_core::Result<void> load_scene_to_ecs(
    void_ecs::World& world,
    const std::filesystem::path& scene_path);

/// @brief Clear all renderable entities from world
void clear_render_entities(void_ecs::World& world);

} // namespace void_render
