/// @file transform_system.hpp
/// @brief ECS-authoritative transform system
///
/// Architecture (from doc/review):
/// - ECS is authoritative for all entity transforms
/// - Scene TransformComponent is a convenience layer
/// - GlobalTransform (ECS) is the single source of truth for rendering
/// - RenderSceneGraph derives render data from ECS transforms
///
/// Transform flow:
/// 1. Scene loads → TransformComponent created
/// 2. TransformSyncSystem syncs to LocalTransform
/// 3. propagate_transforms() computes GlobalTransform from hierarchy
/// 4. RenderSceneGraph extracts GlobalTransform for rendering

#pragma once

#include <void_engine/ecs/world.hpp>
#include <void_engine/ecs/hierarchy.hpp>
#include <void_engine/ecs/entity.hpp>
#include <void_engine/core/error.hpp>

#include <vector>
#include <unordered_map>
#include <optional>
#include <string>
#include <cstdint>

namespace void_scene {

// Forward declarations
struct TransformComponent;

// =============================================================================
// Transform Conversion Utilities
// =============================================================================

/// Convert scene TransformComponent to ECS LocalTransform
[[nodiscard]] void_ecs::LocalTransform to_local_transform(const TransformComponent& scene_transform);

/// Convert ECS LocalTransform back to scene TransformComponent
[[nodiscard]] TransformComponent from_local_transform(const void_ecs::LocalTransform& local);

// =============================================================================
// TransformSyncSystem
// =============================================================================

/// System that synchronizes scene TransformComponent to ECS LocalTransform
///
/// This ensures ECS hierarchy transforms are authoritative:
/// - Reads TransformComponent (scene-level transform)
/// - Writes to LocalTransform (ECS authoritative)
/// - Triggers GlobalTransform propagation
///
/// Usage in game loop:
/// ```cpp
/// // During Update stage:
/// TransformSyncSystem::sync_scene_to_ecs(world);
///
/// // After sync, propagate through hierarchy:
/// void_ecs::propagate_transforms(world);
///
/// // Now GlobalTransform is ready for rendering
/// ```
class TransformSyncSystem {
public:
    /// Sync all TransformComponent → LocalTransform
    /// Call this before propagate_transforms()
    static void sync_scene_to_ecs(void_ecs::World& world);

    /// Sync specific entity's TransformComponent → LocalTransform
    static void sync_entity(void_ecs::World& world, void_ecs::Entity entity);

    /// Register required components with the world
    static void register_components(void_ecs::World& world);

    /// Check if entity has authoritative transform (GlobalTransform)
    [[nodiscard]] static bool has_authoritative_transform(
        const void_ecs::World& world,
        void_ecs::Entity entity);

    /// Get entity's world-space position from ECS GlobalTransform
    [[nodiscard]] static std::optional<void_ecs::Vec3> get_world_position(
        const void_ecs::World& world,
        void_ecs::Entity entity);

    /// Get entity's world-space transform matrix from ECS GlobalTransform
    [[nodiscard]] static std::optional<void_ecs::Mat4> get_world_matrix(
        const void_ecs::World& world,
        void_ecs::Entity entity);
};

// =============================================================================
// RenderTransformData
// =============================================================================

/// Render-ready transform data extracted from ECS
struct RenderTransformData {
    void_ecs::Entity entity;
    void_ecs::Mat4 world_matrix;
    void_ecs::Vec3 world_position;
    bool visible{true};

    /// Check if transform is valid
    [[nodiscard]] bool is_valid() const { return entity.is_valid(); }
};

// =============================================================================
// RenderSceneGraph
// =============================================================================

/// Derived scene graph for rendering
///
/// This is a **cached view** of ECS transforms, NOT authoritative data.
/// It's rebuilt each frame (or on demand) from ECS GlobalTransform components.
///
/// Architecture:
/// - ECS owns authoritative transforms (GlobalTransform)
/// - RenderSceneGraph is a derived/cached view for render submission
/// - Can be rebuilt at any time from ECS state
/// - Optimized for render system traversal
///
/// Usage:
/// ```cpp
/// RenderSceneGraph render_graph;
///
/// // Each frame, after transform propagation:
/// render_graph.rebuild(world);
///
/// // Query for rendering:
/// for (const auto& data : render_graph.visible_transforms()) {
///     submit_draw_call(data.world_matrix, ...);
/// }
/// ```
class RenderSceneGraph {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    RenderSceneGraph() = default;
    ~RenderSceneGraph() = default;

    // Non-copyable (large data structure)
    RenderSceneGraph(const RenderSceneGraph&) = delete;
    RenderSceneGraph& operator=(const RenderSceneGraph&) = delete;

    // Movable
    RenderSceneGraph(RenderSceneGraph&&) noexcept = default;
    RenderSceneGraph& operator=(RenderSceneGraph&&) noexcept = default;

    // =========================================================================
    // Rebuild (from ECS)
    // =========================================================================

    /// Rebuild the render graph from ECS state
    /// Call this after propagate_transforms() each frame
    void rebuild(const void_ecs::World& world);

    /// Rebuild only for specific entities (partial update)
    void rebuild_entities(const void_ecs::World& world,
                          const std::vector<void_ecs::Entity>& entities);

    /// Mark the graph as dirty (needs rebuild)
    void mark_dirty() { m_dirty = true; }

    /// Check if graph needs rebuild
    [[nodiscard]] bool is_dirty() const { return m_dirty; }

    /// Clear all cached data
    void clear();

    // =========================================================================
    // Query
    // =========================================================================

    /// Get all transforms (including invisible)
    [[nodiscard]] const std::vector<RenderTransformData>& all_transforms() const {
        return m_transforms;
    }

    /// Get visible transforms only
    [[nodiscard]] std::vector<RenderTransformData> visible_transforms() const;

    /// Get transform for specific entity
    [[nodiscard]] const RenderTransformData* get_transform(void_ecs::Entity entity) const;

    /// Get transform count
    [[nodiscard]] std::size_t transform_count() const { return m_transforms.size(); }

    /// Get visible transform count
    [[nodiscard]] std::size_t visible_count() const { return m_visible_count; }

    // =========================================================================
    // Frustum Culling Support
    // =========================================================================

    /// Filter transforms by frustum (for culling)
    /// Returns indices into all_transforms()
    [[nodiscard]] std::vector<std::size_t> cull_by_frustum(
        const void_ecs::Mat4& view_projection,
        float near_plane,
        float far_plane) const;

    // =========================================================================
    // Layer/Category Filtering
    // =========================================================================

    /// Get transforms by layer name
    [[nodiscard]] std::vector<const RenderTransformData*> get_by_layer(
        const void_ecs::World& world,
        const std::string& layer) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get time taken for last rebuild (nanoseconds)
    [[nodiscard]] std::uint64_t last_rebuild_time_ns() const { return m_last_rebuild_time_ns; }

    /// Get rebuild count
    [[nodiscard]] std::uint64_t rebuild_count() const { return m_rebuild_count; }

private:
    /// Extract transform data from entity
    RenderTransformData extract_transform(const void_ecs::World& world,
                                          void_ecs::Entity entity) const;

    std::vector<RenderTransformData> m_transforms;
    std::unordered_map<std::uint64_t, std::size_t> m_entity_to_index; // entity.to_bits() -> index
    std::size_t m_visible_count{0};
    bool m_dirty{true};

    // Statistics
    std::uint64_t m_last_rebuild_time_ns{0};
    std::uint64_t m_rebuild_count{0};
};

// =============================================================================
// Transform System Factory (for Kernel stage registration)
// =============================================================================

/// Create a system function that syncs and propagates transforms
/// Suitable for registration with Kernel::register_system()
[[nodiscard]] inline auto make_transform_sync_system() {
    return [](float /*dt*/) {
        // This is a placeholder - caller must provide the World reference
        // Use TransformSyncSystem::sync_scene_to_ecs(world) directly
    };
}

/// Run full transform pipeline on a world
/// 1. Sync scene transforms to ECS
/// 2. Propagate through hierarchy
/// 3. Optionally rebuild render graph
inline void run_transform_pipeline(void_ecs::World& world, RenderSceneGraph* render_graph = nullptr) {
    // Step 1: Sync scene → ECS
    TransformSyncSystem::sync_scene_to_ecs(world);

    // Step 2: Propagate through hierarchy
    void_ecs::propagate_transforms(world);

    // Step 3: Rebuild render graph if provided
    if (render_graph) {
        render_graph->rebuild(world);
    }
}

} // namespace void_scene
