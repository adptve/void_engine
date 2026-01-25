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

namespace void_render {

// =============================================================================
// Ray
// =============================================================================

/// 3D ray for raycasting
struct Ray {
    std::array<float, 3> origin = {0, 0, 0};
    std::array<float, 3> direction = {0, 0, -1};  // Normalized direction

    /// Default constructor
    Ray() = default;

    /// Construct from origin and direction (direction will be normalized)
    Ray(const std::array<float, 3>& orig, const std::array<float, 3>& dir)
        : origin(orig) {
        float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        if (len > 1e-6f) {
            direction = {dir[0] / len, dir[1] / len, dir[2] / len};
        } else {
            direction = {0, 0, -1};
        }
    }

    /// Get point along ray at distance t
    [[nodiscard]] std::array<float, 3> at(float t) const {
        return {
            origin[0] + direction[0] * t,
            origin[1] + direction[1] * t,
            origin[2] + direction[2] * t
        };
    }

    /// Create ray from two points
    [[nodiscard]] static Ray from_points(const std::array<float, 3>& from, const std::array<float, 3>& to) {
        return Ray(from, {to[0] - from[0], to[1] - from[1], to[2] - from[2]});
    }

    /// Create ray from camera and screen coordinates (NDC: -1 to 1)
    [[nodiscard]] static Ray from_screen(const Camera& camera, float ndc_x, float ndc_y) {
        // Get inverse view-projection matrix
        const auto& inv_view = camera.inv_view_matrix();

        // Near point in world space (reverse-Z: near is at z=1 in NDC)
        std::array<float, 4> near_ndc = {ndc_x, ndc_y, 1.0f, 1.0f};

        // Far point in world space (reverse-Z: far is at z=0 in NDC)
        std::array<float, 4> far_ndc = {ndc_x, ndc_y, 0.0f, 1.0f};

        // Transform to world space
        auto transform = [&inv_view](const std::array<float, 4>& p) -> std::array<float, 3> {
            // Note: This is a simplified transform - proper implementation would use full inv_view_proj
            std::array<float, 4> result = {0, 0, 0, 0};
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    result[i] += inv_view[j][i] * p[j];
                }
            }
            float w = result[3];
            if (std::abs(w) > 1e-6f) {
                return {result[0] / w, result[1] / w, result[2] / w};
            }
            return {result[0], result[1], result[2]};
        };

        // For simplicity, use camera position as origin and compute direction from NDC
        auto pos = camera.position();
        auto fwd = camera.forward();
        auto rgt = camera.right();
        auto up = camera.up();

        // Calculate ray direction based on FOV
        float tan_half_fov = std::tan(camera.perspective().fov_y / 2.0f);
        float aspect = camera.perspective().aspect_ratio;

        std::array<float, 3> dir = {
            fwd[0] + rgt[0] * ndc_x * tan_half_fov * aspect + up[0] * ndc_y * tan_half_fov,
            fwd[1] + rgt[1] * ndc_x * tan_half_fov * aspect + up[1] * ndc_y * tan_half_fov,
            fwd[2] + rgt[2] * ndc_x * tan_half_fov * aspect + up[2] * ndc_y * tan_half_fov
        };

        return Ray(pos, dir);
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
    std::array<float, 3> min = {FLT_MAX, FLT_MAX, FLT_MAX};
    std::array<float, 3> max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    /// Default constructor (invalid/empty box)
    AABB() = default;

    /// Construct from min/max
    AABB(const std::array<float, 3>& min_pt, const std::array<float, 3>& max_pt)
        : min(min_pt), max(max_pt) {}

    /// Check if valid (non-empty)
    [[nodiscard]] bool is_valid() const {
        return min[0] <= max[0] && min[1] <= max[1] && min[2] <= max[2];
    }

    /// Get center
    [[nodiscard]] std::array<float, 3> center() const {
        return {
            (min[0] + max[0]) * 0.5f,
            (min[1] + max[1]) * 0.5f,
            (min[2] + max[2]) * 0.5f
        };
    }

    /// Get extents (half-size)
    [[nodiscard]] std::array<float, 3> extents() const {
        return {
            (max[0] - min[0]) * 0.5f,
            (max[1] - min[1]) * 0.5f,
            (max[2] - min[2]) * 0.5f
        };
    }

    /// Get size
    [[nodiscard]] std::array<float, 3> size() const {
        return {max[0] - min[0], max[1] - min[1], max[2] - min[2]};
    }

    /// Get surface area
    [[nodiscard]] float surface_area() const {
        auto s = size();
        return 2.0f * (s[0] * s[1] + s[1] * s[2] + s[2] * s[0]);
    }

    /// Get volume
    [[nodiscard]] float volume() const {
        auto s = size();
        return s[0] * s[1] * s[2];
    }

    /// Get longest axis (0=X, 1=Y, 2=Z)
    [[nodiscard]] int longest_axis() const {
        auto s = size();
        if (s[0] >= s[1] && s[0] >= s[2]) return 0;
        if (s[1] >= s[2]) return 1;
        return 2;
    }

    /// Expand to include point
    void expand(const std::array<float, 3>& point) {
        min[0] = std::min(min[0], point[0]);
        min[1] = std::min(min[1], point[1]);
        min[2] = std::min(min[2], point[2]);
        max[0] = std::max(max[0], point[0]);
        max[1] = std::max(max[1], point[1]);
        max[2] = std::max(max[2], point[2]);
    }

    /// Expand to include another AABB
    void expand(const AABB& other) {
        if (!other.is_valid()) return;
        min[0] = std::min(min[0], other.min[0]);
        min[1] = std::min(min[1], other.min[1]);
        min[2] = std::min(min[2], other.min[2]);
        max[0] = std::max(max[0], other.max[0]);
        max[1] = std::max(max[1], other.max[1]);
        max[2] = std::max(max[2], other.max[2]);
    }

    /// Check if contains point
    [[nodiscard]] bool contains(const std::array<float, 3>& point) const {
        return point[0] >= min[0] && point[0] <= max[0] &&
               point[1] >= min[1] && point[1] <= max[1] &&
               point[2] >= min[2] && point[2] <= max[2];
    }

    /// Check if intersects another AABB
    [[nodiscard]] bool intersects(const AABB& other) const {
        return min[0] <= other.max[0] && max[0] >= other.min[0] &&
               min[1] <= other.max[1] && max[1] >= other.min[1] &&
               min[2] <= other.max[2] && max[2] >= other.min[2];
    }

    /// Ray-AABB intersection test (slab method)
    /// Returns t value if hit, or negative if no hit
    [[nodiscard]] float ray_intersect(const Ray& ray, float t_min = 0.0f, float t_max = FLT_MAX) const {
        for (int i = 0; i < 3; ++i) {
            float inv_d = 1.0f / (ray.direction[i] + 1e-8f);  // Avoid division by zero
            float t0 = (min[i] - ray.origin[i]) * inv_d;
            float t1 = (max[i] - ray.origin[i]) * inv_d;
            if (inv_d < 0.0f) std::swap(t0, t1);
            t_min = std::max(t_min, t0);
            t_max = std::min(t_max, t1);
            if (t_max < t_min) return -1.0f;
        }
        return t_min;
    }

    /// Create from center and extents
    [[nodiscard]] static AABB from_center_extents(const std::array<float, 3>& center,
                                                   const std::array<float, 3>& extents) {
        return AABB(
            {center[0] - extents[0], center[1] - extents[1], center[2] - extents[2]},
            {center[0] + extents[0], center[1] + extents[1], center[2] + extents[2]}
        );
    }

    /// Create unit cube centered at origin
    [[nodiscard]] static AABB unit() {
        return AABB({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f});
    }

    /// Merge two AABBs
    [[nodiscard]] static AABB merge(const AABB& a, const AABB& b) {
        AABB result = a;
        result.expand(b);
        return result;
    }
};

// =============================================================================
// Sphere (for bounding)
// =============================================================================

/// Bounding sphere
struct BoundingSphere {
    std::array<float, 3> center = {0, 0, 0};
    float radius = 0.0f;

    /// Default constructor
    BoundingSphere() = default;

    /// Construct from center and radius
    BoundingSphere(const std::array<float, 3>& c, float r)
        : center(c), radius(r) {}

    /// Create from AABB
    [[nodiscard]] static BoundingSphere from_aabb(const AABB& aabb) {
        auto c = aabb.center();
        auto e = aabb.extents();
        float r = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
        return BoundingSphere(c, r);
    }

    /// Check if contains point
    [[nodiscard]] bool contains(const std::array<float, 3>& point) const {
        float dx = point[0] - center[0];
        float dy = point[1] - center[1];
        float dz = point[2] - center[2];
        return dx * dx + dy * dy + dz * dz <= radius * radius;
    }

    /// Ray-sphere intersection
    [[nodiscard]] float ray_intersect(const Ray& ray) const {
        std::array<float, 3> oc = {
            ray.origin[0] - center[0],
            ray.origin[1] - center[1],
            ray.origin[2] - center[2]
        };

        float a = ray.direction[0] * ray.direction[0] +
                  ray.direction[1] * ray.direction[1] +
                  ray.direction[2] * ray.direction[2];
        float b = 2.0f * (oc[0] * ray.direction[0] +
                          oc[1] * ray.direction[1] +
                          oc[2] * ray.direction[2]);
        float c = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - radius * radius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0) return -1.0f;

        float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
        if (t < 0) t = (-b + std::sqrt(discriminant)) / (2.0f * a);
        return t;
    }
};

// =============================================================================
// RayHit
// =============================================================================

/// Result of a ray intersection test
struct RayHit {
    bool hit = false;
    float distance = FLT_MAX;
    std::array<float, 3> point = {0, 0, 0};
    std::array<float, 3> normal = {0, 1, 0};
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
    std::uint32_t left_child = UINT32_MAX;   // Index of left child (or first primitive if leaf)
    std::uint32_t right_child = UINT32_MAX;  // Index of right child (or primitive count if leaf)
    bool is_leaf = true;

    /// Get primitive range for leaf node
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> primitive_range() const {
        return {left_child, right_child};
    }
};

// =============================================================================
// BVHPrimitive
// =============================================================================

/// Primitive stored in BVH
struct BVHPrimitive {
    AABB bounds;
    std::uint64_t entity_id = 0;
    std::uint32_t original_index = 0;

    /// Get centroid
    [[nodiscard]] std::array<float, 3> centroid() const {
        return bounds.center();
    }
};

// =============================================================================
// BVH (Bounding Volume Hierarchy)
// =============================================================================

/// Bounding Volume Hierarchy for spatial acceleration
class BVH {
public:
    /// Build BVH from primitives
    void build(std::vector<BVHPrimitive> primitives) {
        m_primitives = std::move(primitives);
        m_nodes.clear();

        if (m_primitives.empty()) return;

        // Create root node
        m_nodes.push_back(BVHNode{});
        build_recursive(0, 0, static_cast<std::uint32_t>(m_primitives.size()));
    }

    /// Ray intersection test
    [[nodiscard]] std::optional<RayHit> ray_intersect(const Ray& ray, float max_distance = FLT_MAX) const {
        if (m_nodes.empty()) return std::nullopt;

        RayHit closest_hit;
        closest_hit.distance = max_distance;

        ray_intersect_recursive(ray, 0, closest_hit);

        if (closest_hit.hit) {
            return closest_hit;
        }
        return std::nullopt;
    }

    /// Find all primitives intersecting ray
    void ray_intersect_all(const Ray& ray, std::vector<RayHit>& out_hits, float max_distance = FLT_MAX) const {
        if (m_nodes.empty()) return;
        ray_intersect_all_recursive(ray, 0, out_hits, max_distance);
    }

    /// Query primitives in AABB
    void query_aabb(const AABB& aabb, std::vector<std::uint64_t>& out_entities) const {
        if (m_nodes.empty()) return;
        query_aabb_recursive(aabb, 0, out_entities);
    }

    /// Query primitives in sphere
    void query_sphere(const BoundingSphere& sphere, std::vector<std::uint64_t>& out_entities) const {
        if (m_nodes.empty()) return;
        query_sphere_recursive(sphere, 0, out_entities);
    }

    /// Get node count
    [[nodiscard]] std::size_t node_count() const noexcept { return m_nodes.size(); }

    /// Get primitive count
    [[nodiscard]] std::size_t primitive_count() const noexcept { return m_primitives.size(); }

    /// Get nodes (for debug visualization)
    [[nodiscard]] const std::vector<BVHNode>& nodes() const noexcept { return m_nodes; }

    /// Get primitives
    [[nodiscard]] const std::vector<BVHPrimitive>& primitives() const noexcept { return m_primitives; }

    /// Clear BVH
    void clear() {
        m_nodes.clear();
        m_primitives.clear();
    }

private:
    void build_recursive(std::uint32_t node_index, std::uint32_t start, std::uint32_t end) {
        auto& node = m_nodes[node_index];

        // Calculate bounds
        node.bounds = AABB{};
        for (std::uint32_t i = start; i < end; ++i) {
            node.bounds.expand(m_primitives[i].bounds);
        }

        std::uint32_t primitive_count = end - start;

        // Leaf node if few primitives
        if (primitive_count <= MAX_LEAF_PRIMITIVES) {
            node.is_leaf = true;
            node.left_child = start;
            node.right_child = primitive_count;
            return;
        }

        // Find split axis and position
        int split_axis = node.bounds.longest_axis();
        std::uint32_t mid = start + primitive_count / 2;

        // Sort primitives along split axis
        std::nth_element(
            m_primitives.begin() + start,
            m_primitives.begin() + mid,
            m_primitives.begin() + end,
            [split_axis](const BVHPrimitive& a, const BVHPrimitive& b) {
                return a.centroid()[split_axis] < b.centroid()[split_axis];
            }
        );

        // Create child nodes
        node.is_leaf = false;
        node.left_child = static_cast<std::uint32_t>(m_nodes.size());
        m_nodes.push_back(BVHNode{});
        node.right_child = static_cast<std::uint32_t>(m_nodes.size());
        m_nodes.push_back(BVHNode{});

        // Recurse
        build_recursive(node.left_child, start, mid);
        build_recursive(node.right_child, mid, end);
    }

    void ray_intersect_recursive(const Ray& ray, std::uint32_t node_index, RayHit& closest_hit) const {
        const auto& node = m_nodes[node_index];

        // Test ray against node bounds
        float t = node.bounds.ray_intersect(ray, 0.0f, closest_hit.distance);
        if (t < 0) return;  // No intersection

        if (node.is_leaf) {
            // Test primitives
            auto [start, count] = node.primitive_range();
            for (std::uint32_t i = start; i < start + count; ++i) {
                const auto& prim = m_primitives[i];
                float prim_t = prim.bounds.ray_intersect(ray, 0.0f, closest_hit.distance);
                if (prim_t >= 0 && prim_t < closest_hit.distance) {
                    closest_hit.hit = true;
                    closest_hit.distance = prim_t;
                    closest_hit.point = ray.at(prim_t);
                    closest_hit.entity_id = prim.entity_id;
                    // Normal approximation (toward ray origin)
                    closest_hit.normal = {
                        -ray.direction[0],
                        -ray.direction[1],
                        -ray.direction[2]
                    };
                }
            }
        } else {
            // Recurse into children (front-to-back order)
            const auto& left = m_nodes[node.left_child];
            const auto& right = m_nodes[node.right_child];

            float t_left = left.bounds.ray_intersect(ray);
            float t_right = right.bounds.ray_intersect(ray);

            if (t_left >= 0 && t_right >= 0) {
                if (t_left < t_right) {
                    ray_intersect_recursive(ray, node.left_child, closest_hit);
                    if (t_right < closest_hit.distance) {
                        ray_intersect_recursive(ray, node.right_child, closest_hit);
                    }
                } else {
                    ray_intersect_recursive(ray, node.right_child, closest_hit);
                    if (t_left < closest_hit.distance) {
                        ray_intersect_recursive(ray, node.left_child, closest_hit);
                    }
                }
            } else if (t_left >= 0) {
                ray_intersect_recursive(ray, node.left_child, closest_hit);
            } else if (t_right >= 0) {
                ray_intersect_recursive(ray, node.right_child, closest_hit);
            }
        }
    }

    void ray_intersect_all_recursive(const Ray& ray, std::uint32_t node_index,
                                      std::vector<RayHit>& out_hits, float max_distance) const {
        const auto& node = m_nodes[node_index];

        float t = node.bounds.ray_intersect(ray, 0.0f, max_distance);
        if (t < 0) return;

        if (node.is_leaf) {
            auto [start, count] = node.primitive_range();
            for (std::uint32_t i = start; i < start + count; ++i) {
                const auto& prim = m_primitives[i];
                float prim_t = prim.bounds.ray_intersect(ray, 0.0f, max_distance);
                if (prim_t >= 0) {
                    RayHit hit;
                    hit.hit = true;
                    hit.distance = prim_t;
                    hit.point = ray.at(prim_t);
                    hit.entity_id = prim.entity_id;
                    out_hits.push_back(hit);
                }
            }
        } else {
            ray_intersect_all_recursive(ray, node.left_child, out_hits, max_distance);
            ray_intersect_all_recursive(ray, node.right_child, out_hits, max_distance);
        }
    }

    void query_aabb_recursive(const AABB& aabb, std::uint32_t node_index,
                               std::vector<std::uint64_t>& out_entities) const {
        const auto& node = m_nodes[node_index];

        if (!node.bounds.intersects(aabb)) return;

        if (node.is_leaf) {
            auto [start, count] = node.primitive_range();
            for (std::uint32_t i = start; i < start + count; ++i) {
                if (m_primitives[i].bounds.intersects(aabb)) {
                    out_entities.push_back(m_primitives[i].entity_id);
                }
            }
        } else {
            query_aabb_recursive(aabb, node.left_child, out_entities);
            query_aabb_recursive(aabb, node.right_child, out_entities);
        }
    }

    void query_sphere_recursive(const BoundingSphere& sphere, std::uint32_t node_index,
                                 std::vector<std::uint64_t>& out_entities) const {
        const auto& node = m_nodes[node_index];

        // Check if sphere intersects AABB
        float dist_sq = 0.0f;
        for (int i = 0; i < 3; ++i) {
            if (sphere.center[i] < node.bounds.min[i]) {
                float d = node.bounds.min[i] - sphere.center[i];
                dist_sq += d * d;
            } else if (sphere.center[i] > node.bounds.max[i]) {
                float d = sphere.center[i] - node.bounds.max[i];
                dist_sq += d * d;
            }
        }
        if (dist_sq > sphere.radius * sphere.radius) return;

        if (node.is_leaf) {
            auto [start, count] = node.primitive_range();
            for (std::uint32_t i = start; i < start + count; ++i) {
                // Check primitive center against sphere
                auto c = m_primitives[i].bounds.center();
                float dx = c[0] - sphere.center[0];
                float dy = c[1] - sphere.center[1];
                float dz = c[2] - sphere.center[2];
                if (dx * dx + dy * dy + dz * dz <= sphere.radius * sphere.radius) {
                    out_entities.push_back(m_primitives[i].entity_id);
                }
            }
        } else {
            query_sphere_recursive(sphere, node.left_child, out_entities);
            query_sphere_recursive(sphere, node.right_child, out_entities);
        }
    }

private:
    static constexpr std::uint32_t MAX_LEAF_PRIMITIVES = 4;

    std::vector<BVHNode> m_nodes;
    std::vector<BVHPrimitive> m_primitives;
};

// =============================================================================
// PickingResult
// =============================================================================

/// Result of a picking operation
struct PickingResult {
    bool hit = false;
    std::uint64_t entity_id = UINT64_MAX;
    std::array<float, 3> world_position = {0, 0, 0};
    std::array<float, 3> world_normal = {0, 1, 0};
    float distance = FLT_MAX;

    // Screen position (normalized 0-1)
    std::array<float, 2> screen_position = {0, 0};

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
    using HitCallback = std::function<void(const PickingResult&)>;

    /// Set BVH reference
    void set_bvh(const BVH* bvh) {
        m_bvh = bvh;
    }

    /// Pick at screen position
    [[nodiscard]] PickingResult pick(const Camera& camera, float pixel_x, float pixel_y,
                                      float viewport_width, float viewport_height,
                                      std::uint32_t layer_mask = 0xFFFFFFFF) const {
        (void)layer_mask;  // TODO: Implement layer filtering

        Ray ray = Ray::from_pixel(camera, pixel_x, pixel_y, viewport_width, viewport_height);

        PickingResult result;
        result.screen_position = {
            pixel_x / viewport_width,
            pixel_y / viewport_height
        };

        if (m_bvh) {
            auto hit = m_bvh->ray_intersect(ray);
            if (hit) {
                result.hit = true;
                result.entity_id = hit->entity_id;
                result.world_position = hit->point;
                result.world_normal = hit->normal;
                result.distance = hit->distance;
            }
        }

        return result;
    }

    /// Pick with ray
    [[nodiscard]] PickingResult pick_ray(const Ray& ray, float max_distance = FLT_MAX) const {
        PickingResult result;

        if (m_bvh) {
            auto hit = m_bvh->ray_intersect(ray, max_distance);
            if (hit) {
                result.hit = true;
                result.entity_id = hit->entity_id;
                result.world_position = hit->point;
                result.world_normal = hit->normal;
                result.distance = hit->distance;
            }
        }

        return result;
    }

    /// Pick all entities along ray
    void pick_all(const Ray& ray, std::vector<PickingResult>& out_results,
                  float max_distance = FLT_MAX) const {
        if (!m_bvh) return;

        std::vector<RayHit> hits;
        m_bvh->ray_intersect_all(ray, hits, max_distance);

        for (const auto& hit : hits) {
            PickingResult result;
            result.hit = true;
            result.entity_id = hit.entity_id;
            result.world_position = hit.point;
            result.world_normal = hit.normal;
            result.distance = hit.distance;
            out_results.push_back(result);
        }

        // Sort by distance
        std::sort(out_results.begin(), out_results.end(),
            [](const PickingResult& a, const PickingResult& b) {
                return a.distance < b.distance;
            });
    }

    /// Frustum query (get all entities in view)
    void query_frustum(const Frustum& frustum, std::vector<std::uint64_t>& out_entities) const {
        if (!m_bvh) return;

        // Query entire BVH bounds first, then filter
        for (const auto& prim : m_bvh->primitives()) {
            if (frustum.contains_aabb(prim.bounds.min, prim.bounds.max)) {
                out_entities.push_back(prim.entity_id);
            }
        }
    }

    /// Box select (get all entities in screen rect)
    void box_select(const Camera& camera,
                    float x1, float y1, float x2, float y2,
                    float viewport_width, float viewport_height,
                    std::vector<std::uint64_t>& out_entities) const {
        (void)camera;
        (void)x1; (void)y1; (void)x2; (void)y2;
        (void)viewport_width; (void)viewport_height;
        (void)out_entities;

        // TODO: Implement box selection
        // 1. Create frustum from screen rect
        // 2. Query BVH with frustum
    }

private:
    const BVH* m_bvh = nullptr;
};

// =============================================================================
// SpatialHash (for uniform grids)
// =============================================================================

/// Simple spatial hash grid for broad phase
class SpatialHash {
public:
    /// Construct with cell size
    explicit SpatialHash(float cell_size = 10.0f)
        : m_cell_size(cell_size)
        , m_inv_cell_size(1.0f / cell_size) {}

    /// Clear all entries
    void clear() {
        m_cells.clear();
    }

    /// Insert entity at position
    void insert(std::uint64_t entity_id, const std::array<float, 3>& position) {
        auto key = cell_key(position);
        m_cells[key].push_back(entity_id);
    }

    /// Insert entity with AABB (inserts into all overlapping cells)
    void insert(std::uint64_t entity_id, const AABB& aabb) {
        auto min_cell = cell_coords(aabb.min);
        auto max_cell = cell_coords(aabb.max);

        for (int x = min_cell[0]; x <= max_cell[0]; ++x) {
            for (int y = min_cell[1]; y <= max_cell[1]; ++y) {
                for (int z = min_cell[2]; z <= max_cell[2]; ++z) {
                    std::uint64_t key = hash_coords(x, y, z);
                    m_cells[key].push_back(entity_id);
                }
            }
        }
    }

    /// Query entities near position
    void query(const std::array<float, 3>& position, float radius,
               std::vector<std::uint64_t>& out_entities) const {
        AABB query_aabb = AABB::from_center_extents(position, {radius, radius, radius});
        query(query_aabb, out_entities);
    }

    /// Query entities in AABB
    void query(const AABB& aabb, std::vector<std::uint64_t>& out_entities) const {
        auto min_cell = cell_coords(aabb.min);
        auto max_cell = cell_coords(aabb.max);

        for (int x = min_cell[0]; x <= max_cell[0]; ++x) {
            for (int y = min_cell[1]; y <= max_cell[1]; ++y) {
                for (int z = min_cell[2]; z <= max_cell[2]; ++z) {
                    std::uint64_t key = hash_coords(x, y, z);
                    auto it = m_cells.find(key);
                    if (it != m_cells.end()) {
                        out_entities.insert(out_entities.end(),
                                           it->second.begin(),
                                           it->second.end());
                    }
                }
            }
        }

        // Remove duplicates
        std::sort(out_entities.begin(), out_entities.end());
        out_entities.erase(std::unique(out_entities.begin(), out_entities.end()),
                          out_entities.end());
    }

    /// Get cell size
    [[nodiscard]] float cell_size() const noexcept { return m_cell_size; }

    /// Set cell size (clears grid)
    void set_cell_size(float size) {
        m_cell_size = size;
        m_inv_cell_size = 1.0f / size;
        clear();
    }

private:
    [[nodiscard]] std::array<int, 3> cell_coords(const std::array<float, 3>& position) const {
        return {
            static_cast<int>(std::floor(position[0] * m_inv_cell_size)),
            static_cast<int>(std::floor(position[1] * m_inv_cell_size)),
            static_cast<int>(std::floor(position[2] * m_inv_cell_size))
        };
    }

    [[nodiscard]] std::uint64_t cell_key(const std::array<float, 3>& position) const {
        auto coords = cell_coords(position);
        return hash_coords(coords[0], coords[1], coords[2]);
    }

    [[nodiscard]] static std::uint64_t hash_coords(int x, int y, int z) {
        // Simple spatial hash
        constexpr std::uint64_t P1 = 73856093;
        constexpr std::uint64_t P2 = 19349663;
        constexpr std::uint64_t P3 = 83492791;
        return (static_cast<std::uint64_t>(x) * P1) ^
               (static_cast<std::uint64_t>(y) * P2) ^
               (static_cast<std::uint64_t>(z) * P3);
    }

private:
    float m_cell_size;
    float m_inv_cell_size;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> m_cells;
};

} // namespace void_render
