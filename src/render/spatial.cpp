/// @file spatial.cpp
/// @brief Spatial acceleration structures (BVH, spatial hash) and picking

#include <void_engine/render/spatial.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <stack>
#include <numeric>
#include <cmath>

namespace void_render {

// =============================================================================
// Ray Implementation
// =============================================================================

Ray Ray::from_points(const glm::vec3& start, const glm::vec3& end) {
    glm::vec3 dir = end - start;
    return Ray{start, glm::normalize(dir)};
}

Ray Ray::from_screen(const glm::vec2& screen_pos, const glm::mat4& inv_view_proj,
                      const glm::vec2& viewport_size) {
    // Convert screen position to NDC
    glm::vec2 ndc;
    ndc.x = (2.0f * screen_pos.x / viewport_size.x) - 1.0f;
    ndc.y = 1.0f - (2.0f * screen_pos.y / viewport_size.y);

    // Near and far points in clip space
    glm::vec4 ray_near(ndc.x, ndc.y, -1.0f, 1.0f);
    glm::vec4 ray_far(ndc.x, ndc.y, 1.0f, 1.0f);

    // Transform to world space
    glm::vec4 world_near = inv_view_proj * ray_near;
    glm::vec4 world_far = inv_view_proj * ray_far;

    world_near /= world_near.w;
    world_far /= world_far.w;

    glm::vec3 origin(world_near);
    glm::vec3 direction = glm::normalize(glm::vec3(world_far) - origin);

    return Ray{origin, direction};
}

// Note: Ray::point_at is defined inline in spatial.hpp

// =============================================================================
// AABB Implementation
// =============================================================================

AABB AABB::from_points(const glm::vec3* points, std::size_t count) {
    if (count == 0) return {};

    AABB result;
    result.min = points[0];
    result.max = points[0];

    for (std::size_t i = 1; i < count; ++i) {
        result.expand(points[i]);
    }

    return result;
}

AABB AABB::from_center_extents(const glm::vec3& center, const glm::vec3& extents) {
    return AABB{center - extents, center + extents};
}

AABB AABB::unit() {
    return AABB{glm::vec3(-0.5f), glm::vec3(0.5f)};
}

void AABB::expand(const glm::vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void AABB::expand(const AABB& other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
}

bool AABB::contains(const glm::vec3& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool AABB::contains(const AABB& other) const {
    return other.min.x >= min.x && other.max.x <= max.x &&
           other.min.y >= min.y && other.max.y <= max.y &&
           other.min.z >= min.z && other.max.z <= max.z;
}

bool AABB::intersects(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x &&
           min.y <= other.max.y && max.y >= other.min.y &&
           min.z <= other.max.z && max.z >= other.min.z;
}

std::optional<float> AABB::intersect_ray(const Ray& ray) const {
    // Slab method
    glm::vec3 inv_dir = 1.0f / ray.direction;

    glm::vec3 t0 = (min - ray.origin) * inv_dir;
    glm::vec3 t1 = (max - ray.origin) * inv_dir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float t_enter = std::max({tmin.x, tmin.y, tmin.z});
    float t_exit = std::min({tmax.x, tmax.y, tmax.z});

    if (t_enter > t_exit || t_exit < 0.0f) {
        return std::nullopt;
    }

    return t_enter > 0.0f ? t_enter : t_exit;
}

AABB AABB::transformed(const glm::mat4& transform) const {
    // Transform all 8 corners and build new AABB
    std::array<glm::vec3, 8> corners;
    corners[0] = glm::vec3(transform * glm::vec4(min.x, min.y, min.z, 1.0f));
    corners[1] = glm::vec3(transform * glm::vec4(max.x, min.y, min.z, 1.0f));
    corners[2] = glm::vec3(transform * glm::vec4(min.x, max.y, min.z, 1.0f));
    corners[3] = glm::vec3(transform * glm::vec4(max.x, max.y, min.z, 1.0f));
    corners[4] = glm::vec3(transform * glm::vec4(min.x, min.y, max.z, 1.0f));
    corners[5] = glm::vec3(transform * glm::vec4(max.x, min.y, max.z, 1.0f));
    corners[6] = glm::vec3(transform * glm::vec4(min.x, max.y, max.z, 1.0f));
    corners[7] = glm::vec3(transform * glm::vec4(max.x, max.y, max.z, 1.0f));

    return from_points(corners.data(), 8);
}

glm::vec3 AABB::center() const {
    return (min + max) * 0.5f;
}

glm::vec3 AABB::extents() const {
    return (max - min) * 0.5f;
}

glm::vec3 AABB::size() const {
    return max - min;
}

float AABB::surface_area() const {
    glm::vec3 s = size();
    return 2.0f * (s.x * s.y + s.y * s.z + s.z * s.x);
}

bool AABB::is_valid() const {
    return min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

// =============================================================================
// BoundingSphere Implementation
// =============================================================================

BoundingSphere BoundingSphere::from_points(const glm::vec3* points, std::size_t count) {
    if (count == 0) return {};

    // Ritter's bounding sphere algorithm
    // Find most separated pair of points
    glm::vec3 min_pt = points[0];
    glm::vec3 max_pt = points[0];

    for (std::size_t i = 1; i < count; ++i) {
        if (points[i].x < min_pt.x) min_pt = points[i];
        if (points[i].x > max_pt.x) max_pt = points[i];
    }

    // Initial sphere from these two points
    BoundingSphere sphere;
    sphere.center = (min_pt + max_pt) * 0.5f;
    sphere.radius = glm::length(max_pt - sphere.center);

    // Expand to include all points
    for (std::size_t i = 0; i < count; ++i) {
        float dist = glm::length(points[i] - sphere.center);
        if (dist > sphere.radius) {
            // Point is outside, expand sphere
            float new_radius = (sphere.radius + dist) * 0.5f;
            float k = (new_radius - sphere.radius) / dist;
            sphere.center += (points[i] - sphere.center) * k;
            sphere.radius = new_radius;
        }
    }

    return sphere;
}

BoundingSphere BoundingSphere::from_aabb(const AABB& aabb) {
    BoundingSphere sphere;
    sphere.center = aabb.center();
    sphere.radius = glm::length(aabb.extents());
    return sphere;
}

bool BoundingSphere::contains(const glm::vec3& point) const {
    return glm::length(point - center) <= radius;
}

bool BoundingSphere::intersects(const BoundingSphere& other) const {
    float dist = glm::length(other.center - center);
    return dist <= radius + other.radius;
}

std::optional<float> BoundingSphere::intersect_ray(const Ray& ray) const {
    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    float sqrt_d = std::sqrt(discriminant);
    float t1 = (-b - sqrt_d) / (2.0f * a);
    float t2 = (-b + sqrt_d) / (2.0f * a);

    if (t1 > 0.0f) return t1;
    if (t2 > 0.0f) return t2;
    return std::nullopt;
}

BoundingSphere BoundingSphere::transformed(const glm::mat4& transform) const {
    // Transform center
    glm::vec3 new_center = glm::vec3(transform * glm::vec4(center, 1.0f));

    // Scale radius by maximum scale factor
    glm::vec3 scale;
    scale.x = glm::length(glm::vec3(transform[0]));
    scale.y = glm::length(glm::vec3(transform[1]));
    scale.z = glm::length(glm::vec3(transform[2]));
    float max_scale = std::max({scale.x, scale.y, scale.z});

    return BoundingSphere{new_center, radius * max_scale};
}

// =============================================================================
// BVH Implementation
// =============================================================================

BVH::BVH() = default;

BVH::~BVH() = default;

void BVH::build(std::span<const BVHPrimitive> primitives) {
    if (primitives.empty()) {
        m_nodes.clear();
        m_primitive_indices.clear();
        return;
    }

    // Copy primitives
    std::vector<BVHPrimitive> prims(primitives.begin(), primitives.end());

    // Initialize primitive indices
    m_primitive_indices.resize(prims.size());
    std::iota(m_primitive_indices.begin(), m_primitive_indices.end(), 0);

    // Reserve nodes (worst case: 2n-1 for n primitives)
    m_nodes.clear();
    m_nodes.reserve(prims.size() * 2);

    // Build recursively using SAH
    build_recursive(prims, 0, prims.size());

    spdlog::debug("BVH built: {} primitives, {} nodes", prims.size(), m_nodes.size());
}

std::size_t BVH::build_recursive(std::vector<BVHPrimitive>& primitives,
                                   std::size_t start, std::size_t end) {
    std::size_t node_idx = m_nodes.size();
    m_nodes.emplace_back();
    BVHNode& node = m_nodes.back();

    // Compute bounds of all primitives in range
    AABB bounds;
    for (std::size_t i = start; i < end; ++i) {
        bounds.expand(primitives[i].bounds);
    }
    node.bounds = bounds;

    std::size_t count = end - start;

    // Leaf node if few primitives
    if (count <= 4) {
        node.first_primitive = static_cast<std::uint32_t>(start);
        node.primitive_count = static_cast<std::uint32_t>(count);
        return node_idx;
    }

    // Find best split using SAH
    AABB centroid_bounds;
    for (std::size_t i = start; i < end; ++i) {
        centroid_bounds.expand(primitives[i].centroid);
    }

    int best_axis = 0;
    float best_pos = 0.0f;
    float best_cost = std::numeric_limits<float>::max();

    glm::vec3 extent = centroid_bounds.size();

    // Try each axis
    for (int axis = 0; axis < 3; ++axis) {
        if (extent[axis] < 0.0001f) continue;

        // Binned SAH
        constexpr int NUM_BINS = 12;
        struct Bin {
            AABB bounds;
            std::size_t count = 0;
        };
        std::array<Bin, NUM_BINS> bins;

        float scale = NUM_BINS / extent[axis];

        for (std::size_t i = start; i < end; ++i) {
            int bin_idx = static_cast<int>((primitives[i].centroid[axis] -
                                            centroid_bounds.min[axis]) * scale);
            bin_idx = std::clamp(bin_idx, 0, NUM_BINS - 1);
            bins[bin_idx].bounds.expand(primitives[i].bounds);
            bins[bin_idx].count++;
        }

        // Compute costs for each split position
        std::array<float, NUM_BINS - 1> left_area, right_area;
        std::array<std::size_t, NUM_BINS - 1> left_count, right_count;

        AABB left_box, right_box;
        std::size_t left_sum = 0, right_sum = 0;

        for (int i = 0; i < NUM_BINS - 1; ++i) {
            left_sum += bins[i].count;
            left_box.expand(bins[i].bounds);
            left_count[i] = left_sum;
            left_area[i] = left_box.surface_area();
        }

        for (int i = NUM_BINS - 1; i > 0; --i) {
            right_sum += bins[i].count;
            right_box.expand(bins[i].bounds);
            right_count[i - 1] = right_sum;
            right_area[i - 1] = right_box.surface_area();
        }

        // Find best split for this axis
        float inv_total_area = 1.0f / bounds.surface_area();
        for (int i = 0; i < NUM_BINS - 1; ++i) {
            float cost = 1.0f + (left_count[i] * left_area[i] +
                                 right_count[i] * right_area[i]) * inv_total_area;
            if (cost < best_cost) {
                best_cost = cost;
                best_axis = axis;
                best_pos = centroid_bounds.min[axis] +
                           (i + 1) * extent[axis] / NUM_BINS;
            }
        }
    }

    // If no good split found, create leaf
    float leaf_cost = static_cast<float>(count);
    if (best_cost >= leaf_cost) {
        node.first_primitive = static_cast<std::uint32_t>(start);
        node.primitive_count = static_cast<std::uint32_t>(count);
        return node_idx;
    }

    // Partition primitives
    auto mid = std::partition(
        primitives.begin() + start,
        primitives.begin() + end,
        [best_axis, best_pos](const BVHPrimitive& p) {
            return p.centroid[best_axis] < best_pos;
        }
    );

    std::size_t mid_idx = std::distance(primitives.begin(), mid);

    // Handle degenerate case
    if (mid_idx == start || mid_idx == end) {
        mid_idx = start + count / 2;
    }

    // Build children
    std::size_t left_child = build_recursive(primitives, start, mid_idx);
    std::size_t right_child = build_recursive(primitives, mid_idx, end);

    // Update node (might have been invalidated by reallocation)
    m_nodes[node_idx].left_child = static_cast<std::uint32_t>(left_child);
    m_nodes[node_idx].right_child = static_cast<std::uint32_t>(right_child);
    m_nodes[node_idx].primitive_count = 0;  // Interior node

    return node_idx;
}

std::optional<BVH::HitResult> BVH::intersect(const Ray& ray, float max_distance) const {
    if (m_nodes.empty()) return std::nullopt;

    std::optional<HitResult> closest;
    float closest_t = max_distance;

    std::stack<std::size_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        std::size_t node_idx = stack.top();
        stack.pop();

        const BVHNode& node = m_nodes[node_idx];

        auto t_hit = node.bounds.intersect_ray(ray);
        if (!t_hit || *t_hit > closest_t) {
            continue;
        }

        if (node.is_leaf()) {
            // Test primitives
            for (std::uint32_t i = 0; i < node.primitive_count; ++i) {
                std::uint32_t prim_idx = m_primitive_indices[node.first_primitive + i];

                // Would call primitive intersection here
                // For now, just use bounds intersection as approximation
                // In a full implementation, you'd pass a callback for actual geometry intersection
            }
        } else {
            // Push children (near child first for better culling)
            stack.push(node.right_child);
            stack.push(node.left_child);
        }
    }

    return closest;
}

std::vector<std::uint32_t> BVH::query_frustum(const Frustum& frustum) const {
    std::vector<std::uint32_t> result;
    if (m_nodes.empty()) return result;

    std::stack<std::size_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        std::size_t node_idx = stack.top();
        stack.pop();

        const BVHNode& node = m_nodes[node_idx];

        // Test node bounds against frustum
        if (!frustum.intersects_aabb(node.bounds)) {
            continue;
        }

        if (node.is_leaf()) {
            for (std::uint32_t i = 0; i < node.primitive_count; ++i) {
                result.push_back(m_primitive_indices[node.first_primitive + i]);
            }
        } else {
            stack.push(node.right_child);
            stack.push(node.left_child);
        }
    }

    return result;
}

std::vector<std::uint32_t> BVH::query_sphere(const BoundingSphere& sphere) const {
    std::vector<std::uint32_t> result;
    if (m_nodes.empty()) return result;

    std::stack<std::size_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        std::size_t node_idx = stack.top();
        stack.pop();

        const BVHNode& node = m_nodes[node_idx];

        // Test node bounds against sphere
        // Simple sphere-AABB test
        glm::vec3 closest = glm::clamp(sphere.center, node.bounds.min, node.bounds.max);
        float dist_sq = glm::dot(closest - sphere.center, closest - sphere.center);
        if (dist_sq > sphere.radius * sphere.radius) {
            continue;
        }

        if (node.is_leaf()) {
            for (std::uint32_t i = 0; i < node.primitive_count; ++i) {
                result.push_back(m_primitive_indices[node.first_primitive + i]);
            }
        } else {
            stack.push(node.right_child);
            stack.push(node.left_child);
        }
    }

    return result;
}

void BVH::clear() {
    m_nodes.clear();
    m_primitive_indices.clear();
}

// =============================================================================
// Frustum Implementation
// =============================================================================

Frustum Frustum::from_view_projection(const glm::mat4& vp) {
    Frustum f;

    // Extract planes from view-projection matrix
    // Left
    f.planes[0] = glm::vec4(
        vp[0][3] + vp[0][0],
        vp[1][3] + vp[1][0],
        vp[2][3] + vp[2][0],
        vp[3][3] + vp[3][0]
    );
    // Right
    f.planes[1] = glm::vec4(
        vp[0][3] - vp[0][0],
        vp[1][3] - vp[1][0],
        vp[2][3] - vp[2][0],
        vp[3][3] - vp[3][0]
    );
    // Bottom
    f.planes[2] = glm::vec4(
        vp[0][3] + vp[0][1],
        vp[1][3] + vp[1][1],
        vp[2][3] + vp[2][1],
        vp[3][3] + vp[3][1]
    );
    // Top
    f.planes[3] = glm::vec4(
        vp[0][3] - vp[0][1],
        vp[1][3] - vp[1][1],
        vp[2][3] - vp[2][1],
        vp[3][3] - vp[3][1]
    );
    // Near
    f.planes[4] = glm::vec4(
        vp[0][3] + vp[0][2],
        vp[1][3] + vp[1][2],
        vp[2][3] + vp[2][2],
        vp[3][3] + vp[3][2]
    );
    // Far
    f.planes[5] = glm::vec4(
        vp[0][3] - vp[0][2],
        vp[1][3] - vp[1][2],
        vp[2][3] - vp[2][2],
        vp[3][3] - vp[3][2]
    );

    // Normalize planes
    for (auto& plane : f.planes) {
        float len = glm::length(glm::vec3(plane));
        plane /= len;
    }

    return f;
}

bool Frustum::contains_point(const glm::vec3& point) const {
    for (const auto& plane : planes) {
        if (glm::dot(glm::vec3(plane), point) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersects_aabb(const AABB& aabb) const {
    for (const auto& plane : planes) {
        // Find the positive vertex (furthest along plane normal)
        glm::vec3 p_vertex;
        p_vertex.x = plane.x > 0 ? aabb.max.x : aabb.min.x;
        p_vertex.y = plane.y > 0 ? aabb.max.y : aabb.min.y;
        p_vertex.z = plane.z > 0 ? aabb.max.z : aabb.min.z;

        if (glm::dot(glm::vec3(plane), p_vertex) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersects_sphere(const BoundingSphere& sphere) const {
    for (const auto& plane : planes) {
        float dist = glm::dot(glm::vec3(plane), sphere.center) + plane.w;
        if (dist < -sphere.radius) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// SpatialHash Implementation
// =============================================================================

SpatialHash::SpatialHash(float cell_size)
    : m_cell_size(cell_size)
    , m_inv_cell_size(1.0f / cell_size)
{
}

void SpatialHash::insert(std::uint32_t id, const AABB& bounds) {
    auto [min_cell, max_cell] = get_cell_range(bounds);

    for (int x = min_cell.x; x <= max_cell.x; ++x) {
        for (int y = min_cell.y; y <= max_cell.y; ++y) {
            for (int z = min_cell.z; z <= max_cell.z; ++z) {
                CellKey key{x, y, z};
                m_cells[key].push_back(id);
            }
        }
    }

    m_object_bounds[id] = bounds;
}

void SpatialHash::remove(std::uint32_t id) {
    auto it = m_object_bounds.find(id);
    if (it == m_object_bounds.end()) return;

    auto [min_cell, max_cell] = get_cell_range(it->second);

    for (int x = min_cell.x; x <= max_cell.x; ++x) {
        for (int y = min_cell.y; y <= max_cell.y; ++y) {
            for (int z = min_cell.z; z <= max_cell.z; ++z) {
                CellKey key{x, y, z};
                auto cell_it = m_cells.find(key);
                if (cell_it != m_cells.end()) {
                    auto& vec = cell_it->second;
                    vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
                    if (vec.empty()) {
                        m_cells.erase(cell_it);
                    }
                }
            }
        }
    }

    m_object_bounds.erase(it);
}

void SpatialHash::update(std::uint32_t id, const AABB& new_bounds) {
    remove(id);
    insert(id, new_bounds);
}

std::vector<std::uint32_t> SpatialHash::query(const AABB& bounds) const {
    std::unordered_set<std::uint32_t> result_set;
    auto [min_cell, max_cell] = get_cell_range(bounds);

    for (int x = min_cell.x; x <= max_cell.x; ++x) {
        for (int y = min_cell.y; y <= max_cell.y; ++y) {
            for (int z = min_cell.z; z <= max_cell.z; ++z) {
                CellKey key{x, y, z};
                auto it = m_cells.find(key);
                if (it != m_cells.end()) {
                    for (std::uint32_t id : it->second) {
                        auto bounds_it = m_object_bounds.find(id);
                        if (bounds_it != m_object_bounds.end() &&
                            bounds_it->second.intersects(bounds)) {
                            result_set.insert(id);
                        }
                    }
                }
            }
        }
    }

    return std::vector<std::uint32_t>(result_set.begin(), result_set.end());
}

std::vector<std::uint32_t> SpatialHash::query_point(const glm::vec3& point) const {
    CellKey key = get_cell_key(point);
    auto it = m_cells.find(key);

    if (it == m_cells.end()) {
        return {};
    }

    std::vector<std::uint32_t> result;
    for (std::uint32_t id : it->second) {
        auto bounds_it = m_object_bounds.find(id);
        if (bounds_it != m_object_bounds.end() &&
            bounds_it->second.contains(point)) {
            result.push_back(id);
        }
    }

    return result;
}

void SpatialHash::clear() {
    m_cells.clear();
    m_object_bounds.clear();
}

SpatialHash::CellKey SpatialHash::get_cell_key(const glm::vec3& point) const {
    return CellKey{
        static_cast<int>(std::floor(point.x * m_inv_cell_size)),
        static_cast<int>(std::floor(point.y * m_inv_cell_size)),
        static_cast<int>(std::floor(point.z * m_inv_cell_size))
    };
}

std::pair<SpatialHash::CellKey, SpatialHash::CellKey>
SpatialHash::get_cell_range(const AABB& bounds) const {
    CellKey min_key{
        static_cast<int>(std::floor(bounds.min.x * m_inv_cell_size)),
        static_cast<int>(std::floor(bounds.min.y * m_inv_cell_size)),
        static_cast<int>(std::floor(bounds.min.z * m_inv_cell_size))
    };
    CellKey max_key{
        static_cast<int>(std::floor(bounds.max.x * m_inv_cell_size)),
        static_cast<int>(std::floor(bounds.max.y * m_inv_cell_size)),
        static_cast<int>(std::floor(bounds.max.z * m_inv_cell_size))
    };
    return {min_key, max_key};
}

// =============================================================================
// PickingManager Implementation
// =============================================================================

PickingManager::PickingManager() = default;

PickingManager::~PickingManager() = default;

void PickingManager::register_object(std::uint32_t id, const AABB& bounds,
                                      std::uint32_t layer_mask) {
    m_objects[id] = {bounds, layer_mask};
}

void PickingManager::unregister_object(std::uint32_t id) {
    m_objects.erase(id);
}

void PickingManager::update_object(std::uint32_t id, const AABB& bounds) {
    auto it = m_objects.find(id);
    if (it != m_objects.end()) {
        it->second.bounds = bounds;
    }
}

std::optional<PickingManager::PickResult>
PickingManager::pick(const Ray& ray, std::uint32_t layer_mask, float max_distance) const {
    std::optional<PickResult> closest;
    float closest_t = max_distance;

    for (const auto& [id, obj] : m_objects) {
        if ((obj.layer_mask & layer_mask) == 0) continue;

        auto t = obj.bounds.intersect_ray(ray);
        if (t && *t < closest_t) {
            closest_t = *t;
            closest = PickResult{
                id,
                ray.point_at(*t),
                glm::vec3(0.0f, 1.0f, 0.0f),  // Would need actual normal
                *t
            };
        }
    }

    return closest;
}

std::vector<PickingManager::PickResult>
PickingManager::pick_all(const Ray& ray, std::uint32_t layer_mask, float max_distance) const {
    std::vector<PickResult> results;

    for (const auto& [id, obj] : m_objects) {
        if ((obj.layer_mask & layer_mask) == 0) continue;

        auto t = obj.bounds.intersect_ray(ray);
        if (t && *t <= max_distance) {
            results.push_back({
                id,
                ray.point_at(*t),
                glm::vec3(0.0f, 1.0f, 0.0f),
                *t
            });
        }
    }

    // Sort by distance
    std::sort(results.begin(), results.end(),
              [](const PickResult& a, const PickResult& b) {
                  return a.distance < b.distance;
              });

    return results;
}

std::vector<std::uint32_t>
PickingManager::query_frustum(const Frustum& frustum, std::uint32_t layer_mask) const {
    std::vector<std::uint32_t> results;

    for (const auto& [id, obj] : m_objects) {
        if ((obj.layer_mask & layer_mask) == 0) continue;

        if (frustum.intersects_aabb(obj.bounds)) {
            results.push_back(id);
        }
    }

    return results;
}

void PickingManager::clear() {
    m_objects.clear();
}

} // namespace void_render
