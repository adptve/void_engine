#pragma once

/// @file spatial.hpp
/// @brief Spatial acceleration structures and picking for void_render

#include "fwd.hpp"
#include "camera.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <optional>
#include <limits>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <functional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace void_render {

// =============================================================================
// Ray
// =============================================================================

/// 3D ray for raycasting
struct Ray {
    glm::vec3 origin = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);  // Normalized direction

    /// Default constructor
    Ray() = default;

    /// Construct from origin and direction
    Ray(const glm::vec3& orig, const glm::vec3& dir)
        : origin(orig)
        , direction(glm::normalize(dir)) {}

    /// Get point along ray at distance t
    [[nodiscard]] glm::vec3 point_at(float t) const {
        return origin + direction * t;
    }

    /// Get point along ray at distance t (alias for point_at)
    [[nodiscard]] glm::vec3 at(float t) const {
        return point_at(t);
    }

    /// Get point as std::array<float, 3>
    [[nodiscard]] std::array<float, 3> at_array(float t) const {
        glm::vec3 p = point_at(t);
        return {p.x, p.y, p.z};
    }

    /// Create ray from two points
    [[nodiscard]] static Ray from_points(const glm::vec3& start, const glm::vec3& end);

    /// Create ray from screen coordinates
    [[nodiscard]] static Ray from_screen(const glm::vec2& screen_pos, const glm::mat4& inv_view_proj,
                                          const glm::vec2& viewport_size);

    /// Create ray from camera and screen coordinates (NDC: -1 to 1)
    [[nodiscard]] static Ray from_screen(const Camera& camera, float ndc_x, float ndc_y) {
        // Get inverse view-projection matrix
        const auto& inv_view = camera.inv_view_matrix();

        // For simplicity, use camera position as origin and compute direction from NDC
        auto pos = camera.position();
        auto fwd = camera.forward();
        auto rgt = camera.right();
        auto up = camera.up();

        // Calculate ray direction based on FOV
        float tan_half_fov = std::tan(camera.perspective().fov_y / 2.0f);
        float aspect = camera.perspective().aspect_ratio;

        glm::vec3 dir = {
            fwd[0] + rgt[0] * ndc_x * tan_half_fov * aspect + up[0] * ndc_y * tan_half_fov,
            fwd[1] + rgt[1] * ndc_x * tan_half_fov * aspect + up[1] * ndc_y * tan_half_fov,
            fwd[2] + rgt[2] * ndc_x * tan_half_fov * aspect + up[2] * ndc_y * tan_half_fov
        };

        return Ray(glm::vec3(pos[0], pos[1], pos[2]), dir);
    }

    /// Create ray from pixel coordinates
    [[nodiscard]] static Ray from_pixel(const Camera& camera, float pixel_x, float pixel_y,
                                        float viewport_width, float viewport_height) {
        // Convert pixel to NDC
        float ndc_x = (2.0f * pixel_x / viewport_width) - 1.0f;
        float ndc_y = 1.0f - (2.0f * pixel_y / viewport_height);  // Flip Y
        return from_screen(camera, ndc_x, ndc_y);
    }
};

// =============================================================================
// AABB (Axis-Aligned Bounding Box)
// =============================================================================

/// Axis-aligned bounding box
struct AABB {
    glm::vec3 min = glm::vec3(FLT_MAX);
    glm::vec3 max = glm::vec3(-FLT_MAX);

    /// Default constructor (invalid/empty box)
    AABB() = default;

    /// Construct from min/max
    AABB(const glm::vec3& min_pt, const glm::vec3& max_pt)
        : min(min_pt), max(max_pt) {}

    /// Check if valid (non-empty)
    [[nodiscard]] bool is_valid() const;

    /// Get center
    [[nodiscard]] glm::vec3 center() const;

    /// Get extents (half-size)
    [[nodiscard]] glm::vec3 extents() const;

    /// Get size
    [[nodiscard]] glm::vec3 size() const;

    /// Get surface area
    [[nodiscard]] float surface_area() const;

    /// Get volume
    [[nodiscard]] float volume() const {
        glm::vec3 s = size();
        return s.x * s.y * s.z;
    }

    /// Get longest axis (0=X, 1=Y, 2=Z)
    [[nodiscard]] int longest_axis() const {
        glm::vec3 s = size();
        if (s.x >= s.y && s.x >= s.z) return 0;
        if (s.y >= s.z) return 1;
        return 2;
    }

    /// Expand to include point
    void expand(const glm::vec3& point);

    /// Expand to include another AABB
    void expand(const AABB& other);

    /// Check if contains point
    [[nodiscard]] bool contains(const glm::vec3& point) const;

    /// Check if contains another AABB
    [[nodiscard]] bool contains(const AABB& other) const;

    /// Check if intersects another AABB
    [[nodiscard]] bool intersects(const AABB& other) const;

    /// Ray-AABB intersection test
    [[nodiscard]] std::optional<float> intersect_ray(const Ray& ray) const;

    /// Transform AABB by matrix
    [[nodiscard]] AABB transformed(const glm::mat4& transform) const;

    /// Create from center and extents
    [[nodiscard]] static AABB from_center_extents(const glm::vec3& center, const glm::vec3& extents);

    /// Create from array of points
    [[nodiscard]] static AABB from_points(const glm::vec3* points, std::size_t count);

    /// Create unit cube centered at origin
    [[nodiscard]] static AABB unit();

    /// Merge two AABBs
    [[nodiscard]] static AABB merge(const AABB& a, const AABB& b) {
        AABB result = a;
        result.expand(b);
        return result;
    }

    /// Ray-AABB intersection test (slab method) - legacy interface
    [[nodiscard]] float ray_intersect(const Ray& ray, float t_min = 0.0f, float t_max = FLT_MAX) const {
        auto result = intersect_ray(ray);
        if (!result || *result < t_min || *result > t_max) return -1.0f;
        return *result;
    }
};

// =============================================================================
// Bounding Sphere
// =============================================================================

/// Bounding sphere
struct BoundingSphere {
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 0.0f;

    /// Default constructor
    BoundingSphere() = default;

    /// Construct from center and radius
    BoundingSphere(const glm::vec3& c, float r)
        : center(c), radius(r) {}

    /// Create from array of points
    [[nodiscard]] static BoundingSphere from_points(const glm::vec3* points, std::size_t count);

    /// Create from AABB
    [[nodiscard]] static BoundingSphere from_aabb(const AABB& aabb);

    /// Check if contains point
    [[nodiscard]] bool contains(const glm::vec3& point) const;

    /// Check if intersects another sphere
    [[nodiscard]] bool intersects(const BoundingSphere& other) const;

    /// Ray-sphere intersection
    [[nodiscard]] std::optional<float> intersect_ray(const Ray& ray) const;

    /// Transform sphere by matrix
    [[nodiscard]] BoundingSphere transformed(const glm::mat4& transform) const;

    /// Legacy ray intersection interface
    [[nodiscard]] float ray_intersect(const Ray& ray) const {
        auto result = intersect_ray(ray);
        return result.value_or(-1.0f);
    }
};

// Note: Frustum is defined in camera.hpp

// =============================================================================
// RayHit
// =============================================================================

/// Result of a ray intersection test
struct RayHit {
    bool hit = false;
    float distance = FLT_MAX;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    std::uint64_t entity_id = UINT64_MAX;
    std::uint32_t mesh_index = UINT32_MAX;
    std::uint32_t triangle_index = UINT32_MAX;

    // Barycentric coordinates
    float u = 0.0f;
    float v = 0.0f;

    /// Check if this hit is closer than another
    [[nodiscard]] bool is_closer_than(const RayHit& other) const {
        return hit && (!other.hit || distance < other.distance);
    }

    /// Create miss result
    [[nodiscard]] static RayHit miss() {
        return RayHit{};
    }
};

// =============================================================================
// BVHNode
// =============================================================================

/// Node in a BVH tree
struct BVHNode {
    AABB bounds;
    std::uint32_t left_child = UINT32_MAX;
    std::uint32_t right_child = UINT32_MAX;
    std::uint32_t first_primitive = 0;
    std::uint32_t primitive_count = 0;

    /// Check if leaf node
    [[nodiscard]] bool is_leaf() const {
        return primitive_count > 0;
    }

    /// Get primitive range for leaf node
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> primitive_range() const {
        return {first_primitive, primitive_count};
    }
};

// =============================================================================
// BVHPrimitive
// =============================================================================

/// Primitive stored in BVH
struct BVHPrimitive {
    AABB bounds;
    glm::vec3 centroid = glm::vec3(0.0f);
    std::uint64_t entity_id = 0;
    std::uint32_t original_index = 0;
};

// =============================================================================
// BVH (Bounding Volume Hierarchy)
// =============================================================================

/// Bounding Volume Hierarchy for spatial acceleration
class BVH {
public:
    struct HitResult {
        bool hit = false;
        std::uint32_t primitive_index = UINT32_MAX;
        std::uint64_t entity_id = UINT64_MAX;
        float distance = FLT_MAX;
        glm::vec3 point = glm::vec3(0.0f);
    };

    BVH();
    ~BVH();

    /// Build BVH from primitives
    void build(std::span<const BVHPrimitive> primitives);

    /// Ray intersection test
    [[nodiscard]] std::optional<HitResult> intersect(const Ray& ray, float max_distance = FLT_MAX) const;

    /// Ray intersection test (legacy alias)
    [[nodiscard]] std::optional<HitResult> ray_intersect(const Ray& ray, float max_distance = FLT_MAX) const {
        return intersect(ray, max_distance);
    }

    /// Query primitives in AABB
    [[nodiscard]] std::vector<std::uint32_t> query_aabb(const AABB& aabb, std::vector<std::uint64_t>& results) const {
        // Simple implementation: iterate nodes and test intersection
        std::vector<std::uint32_t> indices;
        if (m_nodes.empty()) return indices;

        std::vector<std::size_t> stack;
        stack.reserve(64);
        stack.push_back(0);

        while (!stack.empty()) {
            std::size_t node_idx = stack.back();
            stack.pop_back();

            if (node_idx >= m_nodes.size()) continue;
            const auto& node = m_nodes[node_idx];

            if (!node.bounds.intersects(aabb)) continue;

            if (node.is_leaf()) {
                for (std::uint32_t i = 0; i < node.primitive_count; ++i) {
                    std::uint32_t prim_idx = node.first_primitive + i;
                    if (prim_idx < m_primitive_indices.size()) {
                        indices.push_back(m_primitive_indices[prim_idx]);
                        results.push_back(m_primitive_indices[prim_idx]);
                    }
                }
            } else {
                if (node.left_child != UINT32_MAX) stack.push_back(node.left_child);
                if (node.right_child != UINT32_MAX) stack.push_back(node.right_child);
            }
        }

        return indices;
    }

    /// Query primitives in frustum
    [[nodiscard]] std::vector<std::uint32_t> query_frustum(const Frustum& frustum) const;

    /// Query primitives in sphere
    [[nodiscard]] std::vector<std::uint32_t> query_sphere(const BoundingSphere& sphere) const;

    /// Get node count
    [[nodiscard]] std::size_t node_count() const noexcept { return m_nodes.size(); }

    /// Get primitive count
    [[nodiscard]] std::size_t primitive_count() const noexcept { return m_primitive_indices.size(); }

    /// Get nodes (for debug visualization)
    [[nodiscard]] const std::vector<BVHNode>& nodes() const noexcept { return m_nodes; }

    /// Get primitive indices
    [[nodiscard]] const std::vector<std::uint32_t>& primitive_indices() const noexcept { return m_primitive_indices; }

    /// Clear BVH
    void clear();

private:
    std::size_t build_recursive(std::vector<BVHPrimitive>& primitives,
                                 std::size_t start, std::size_t end);

    std::vector<BVHNode> m_nodes;
    std::vector<std::uint32_t> m_primitive_indices;
};

// =============================================================================
// PickingResult
// =============================================================================

/// Result of a picking operation
struct PickingResult {
    bool hit = false;
    std::uint64_t entity_id = UINT64_MAX;
    glm::vec3 world_position = glm::vec3(0.0f);
    glm::vec3 world_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance = FLT_MAX;

    // Screen position (normalized 0-1)
    glm::vec2 screen_position = glm::vec2(0.0f);

    // Depth buffer value
    float depth = 1.0f;

    /// Create miss result
    [[nodiscard]] static PickingResult miss() {
        return PickingResult{};
    }
};

// =============================================================================
// PickingManager
// =============================================================================

/// Manages object picking
class PickingManager {
public:
    struct PickResult {
        std::uint32_t id = UINT32_MAX;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        float distance = FLT_MAX;
    };

    PickingManager();
    ~PickingManager();

    /// Set BVH for ray picking
    void set_bvh(BVH* bvh) { m_bvh = bvh; }

    /// Get BVH
    [[nodiscard]] BVH* bvh() const { return m_bvh; }

    /// Pick using ray through BVH (returns PickingResult for compatibility)
    [[nodiscard]] PickingResult pick_ray(const Ray& ray, float max_distance = FLT_MAX) const {
        PickingResult result;
        if (!m_bvh) return result;

        auto hit = m_bvh->intersect(ray, max_distance);
        if (hit && hit->hit) {
            result.hit = true;
            result.entity_id = hit->entity_id;
            result.world_position = hit->point;
            result.distance = hit->distance;
        }
        return result;
    }

    /// Register object for picking
    void register_object(std::uint32_t id, const AABB& bounds, std::uint32_t layer_mask = 0xFFFFFFFF);

    /// Unregister object
    void unregister_object(std::uint32_t id);

    /// Update object bounds
    void update_object(std::uint32_t id, const AABB& bounds);

    /// Pick closest object along ray
    [[nodiscard]] std::optional<PickResult> pick(const Ray& ray, std::uint32_t layer_mask = 0xFFFFFFFF,
                                                  float max_distance = FLT_MAX) const;

    /// Pick all objects along ray
    [[nodiscard]] std::vector<PickResult> pick_all(const Ray& ray, std::uint32_t layer_mask = 0xFFFFFFFF,
                                                    float max_distance = FLT_MAX) const;

    /// Query objects in frustum
    [[nodiscard]] std::vector<std::uint32_t> query_frustum(const Frustum& frustum,
                                                           std::uint32_t layer_mask = 0xFFFFFFFF) const;

    /// Clear all objects
    void clear();

private:
    struct PickableObject {
        AABB bounds;
        std::uint32_t layer_mask;
    };

    std::unordered_map<std::uint32_t, PickableObject> m_objects;
    BVH* m_bvh = nullptr;
};

// =============================================================================
// SpatialHash (for uniform grids)
// =============================================================================

/// Simple spatial hash grid for broad phase
class SpatialHash {
public:
    struct CellKey {
        int x, y, z;
        bool operator==(const CellKey& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CellKeyHash {
        std::size_t operator()(const CellKey& key) const {
            constexpr std::size_t P1 = 73856093;
            constexpr std::size_t P2 = 19349663;
            constexpr std::size_t P3 = 83492791;
            return static_cast<std::size_t>(key.x) * P1 ^
                   static_cast<std::size_t>(key.y) * P2 ^
                   static_cast<std::size_t>(key.z) * P3;
        }
    };

    /// Construct with cell size
    explicit SpatialHash(float cell_size = 10.0f);

    /// Insert object with AABB bounds
    void insert(std::uint32_t id, const AABB& bounds);

    /// Insert object at point position (creates small AABB around point)
    void insert(std::uint32_t id, const glm::vec3& position) {
        glm::vec3 half_size(m_cell_size * 0.01f);
        insert(id, AABB(position - half_size, position + half_size));
    }

    /// Remove object
    void remove(std::uint32_t id);

    /// Update object bounds
    void update(std::uint32_t id, const AABB& new_bounds);

    /// Query objects in AABB
    [[nodiscard]] std::vector<std::uint32_t> query(const AABB& bounds) const;

    /// Query objects near point within radius (legacy test API)
    void query(const glm::vec3& point, float radius, std::vector<std::uint64_t>& results) const {
        AABB query_bounds(point - glm::vec3(radius), point + glm::vec3(radius));
        auto ids = query(query_bounds);
        for (auto id : ids) {
            results.push_back(id);
        }
    }

    /// Query objects at point
    [[nodiscard]] std::vector<std::uint32_t> query_point(const glm::vec3& point) const;

    /// Clear all entries
    void clear();

    /// Get cell size
    [[nodiscard]] float cell_size() const noexcept { return m_cell_size; }

private:
    [[nodiscard]] CellKey get_cell_key(const glm::vec3& point) const;
    [[nodiscard]] std::pair<CellKey, CellKey> get_cell_range(const AABB& bounds) const;

    float m_cell_size;
    float m_inv_cell_size;
    std::unordered_map<CellKey, std::vector<std::uint32_t>, CellKeyHash> m_cells;
    std::unordered_map<std::uint32_t, AABB> m_object_bounds;
};

} // namespace void_render
