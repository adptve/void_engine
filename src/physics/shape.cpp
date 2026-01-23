/// @file shape.cpp
/// @brief Collision shape implementations for void_physics

#include <void_engine/physics/shape.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace void_physics {

constexpr float PI = 3.14159265358979323846f;

// =============================================================================
// BoxShape Implementation
// =============================================================================

MassProperties BoxShape::compute_mass(float density) const {
    MassProperties props;
    props.mass = volume() * density;

    // Inertia tensor for a solid box
    float m = props.mass;
    float x2 = m_half_extents.x * m_half_extents.x;
    float y2 = m_half_extents.y * m_half_extents.y;
    float z2 = m_half_extents.z * m_half_extents.z;

    props.inertia_diagonal = {
        m * (y2 + z2) / 3.0f,
        m * (x2 + z2) / 3.0f,
        m * (x2 + y2) / 3.0f
    };
    props.center_of_mass = m_local_offset;
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};

    return props;
}

bool BoxShape::contains_point(const void_math::Vec3& point) const {
    auto local = point - m_local_offset;
    return std::abs(local.x) <= m_half_extents.x &&
           std::abs(local.y) <= m_half_extents.y &&
           std::abs(local.z) <= m_half_extents.z;
}

void_math::Vec3 BoxShape::closest_point(const void_math::Vec3& point) const {
    auto local = point - m_local_offset;
    return m_local_offset + void_math::Vec3{
        std::clamp(local.x, -m_half_extents.x, m_half_extents.x),
        std::clamp(local.y, -m_half_extents.y, m_half_extents.y),
        std::clamp(local.z, -m_half_extents.z, m_half_extents.z)
    };
}

void_math::Vec3 BoxShape::support(const void_math::Vec3& direction) const {
    return m_local_offset + void_math::Vec3{
        direction.x >= 0 ? m_half_extents.x : -m_half_extents.x,
        direction.y >= 0 ? m_half_extents.y : -m_half_extents.y,
        direction.z >= 0 ? m_half_extents.z : -m_half_extents.z
    };
}

std::unique_ptr<IShape> BoxShape::clone() const {
    auto cloned = std::make_unique<BoxShape>(m_half_extents);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

// =============================================================================
// SphereShape Implementation
// =============================================================================

MassProperties SphereShape::compute_mass(float density) const {
    MassProperties props;
    props.mass = volume() * density;

    // Inertia tensor for a solid sphere (uniform in all directions)
    float i = (2.0f / 5.0f) * props.mass * m_radius * m_radius;
    props.inertia_diagonal = {i, i, i};
    props.center_of_mass = m_local_offset;
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};

    return props;
}

bool SphereShape::contains_point(const void_math::Vec3& point) const {
    auto d = point - m_local_offset;
    return void_math::dot(d, d) <= m_radius * m_radius;
}

void_math::Vec3 SphereShape::closest_point(const void_math::Vec3& point) const {
    auto d = point - m_local_offset;
    float len = void_math::length(d);
    if (len > 0.0001f) {
        return m_local_offset + d * (m_radius / len);
    }
    return m_local_offset + void_math::Vec3{m_radius, 0, 0};
}

void_math::Vec3 SphereShape::support(const void_math::Vec3& direction) const {
    return m_local_offset + void_math::normalize(direction) * m_radius;
}

std::unique_ptr<IShape> SphereShape::clone() const {
    auto cloned = std::make_unique<SphereShape>(m_radius);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

// =============================================================================
// CapsuleShape Implementation
// =============================================================================

void_math::AABB CapsuleShape::local_bounds() const {
    void_math::Vec3 axis_vec{0, 0, 0};
    if (m_axis == 0) axis_vec.x = m_half_height;
    else if (m_axis == 1) axis_vec.y = m_half_height;
    else axis_vec.z = m_half_height;

    void_math::Vec3 radius_vec{m_radius, m_radius, m_radius};
    return void_math::AABB{
        m_local_offset - axis_vec - radius_vec,
        m_local_offset + axis_vec + radius_vec
    };
}

float CapsuleShape::volume() const {
    // Cylinder + two hemispheres = cylinder + sphere
    float cylinder = PI * m_radius * m_radius * (2.0f * m_half_height);
    float sphere = (4.0f / 3.0f) * PI * m_radius * m_radius * m_radius;
    return cylinder + sphere;
}

MassProperties CapsuleShape::compute_mass(float density) const {
    MassProperties props;
    props.mass = volume() * density;

    // Simplified inertia calculation
    float r2 = m_radius * m_radius;
    float h2 = m_half_height * m_half_height;
    float i_axial = (1.0f / 2.0f) * props.mass * r2;
    float i_transverse = props.mass * (r2 / 4.0f + h2 / 3.0f);

    if (m_axis == 0) {
        props.inertia_diagonal = {i_axial, i_transverse, i_transverse};
    } else if (m_axis == 1) {
        props.inertia_diagonal = {i_transverse, i_axial, i_transverse};
    } else {
        props.inertia_diagonal = {i_transverse, i_transverse, i_axial};
    }

    props.center_of_mass = m_local_offset;
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};

    return props;
}

bool CapsuleShape::contains_point(const void_math::Vec3& point) const {
    auto [p0, p1] = endpoints();
    // Find closest point on line segment
    auto d = p1 - p0;
    float t = std::clamp(void_math::dot(point - p0, d) / void_math::dot(d, d), 0.0f, 1.0f);
    auto closest = p0 + d * t;
    return void_math::length(point - closest) <= m_radius;
}

void_math::Vec3 CapsuleShape::closest_point(const void_math::Vec3& point) const {
    auto [p0, p1] = endpoints();
    auto d = p1 - p0;
    float t = std::clamp(void_math::dot(point - p0, d) / void_math::dot(d, d), 0.0f, 1.0f);
    auto axis_point = p0 + d * t;
    auto dir = point - axis_point;
    float len = void_math::length(dir);
    if (len > 0.0001f) {
        return axis_point + dir * (m_radius / len);
    }
    return axis_point + void_math::Vec3{m_radius, 0, 0};
}

void_math::Vec3 CapsuleShape::support(const void_math::Vec3& direction) const {
    auto [p0, p1] = endpoints();
    // Pick the endpoint most in the direction
    auto d0 = void_math::dot(p0, direction);
    auto d1 = void_math::dot(p1, direction);
    auto center = d0 > d1 ? p0 : p1;
    return center + void_math::normalize(direction) * m_radius;
}

std::unique_ptr<IShape> CapsuleShape::clone() const {
    auto cloned = std::make_unique<CapsuleShape>(m_radius, height(), m_axis);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

std::pair<void_math::Vec3, void_math::Vec3> CapsuleShape::endpoints() const {
    void_math::Vec3 axis_vec{0, 0, 0};
    if (m_axis == 0) axis_vec.x = m_half_height;
    else if (m_axis == 1) axis_vec.y = m_half_height;
    else axis_vec.z = m_half_height;

    return {m_local_offset - axis_vec, m_local_offset + axis_vec};
}

// =============================================================================
// PlaneShape Implementation
// =============================================================================

MassProperties PlaneShape::compute_mass(float /*density*/) const {
    // Planes are infinite, return zero mass
    MassProperties props;
    props.mass = 0.0f;
    props.inertia_diagonal = {0, 0, 0};
    props.center_of_mass = m_normal * m_distance;
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};
    return props;
}

bool PlaneShape::contains_point(const void_math::Vec3& point) const {
    return signed_distance(point) <= 0.0f;
}

void_math::Vec3 PlaneShape::closest_point(const void_math::Vec3& point) const {
    float d = signed_distance(point);
    return point - m_normal * d;
}

void_math::Vec3 PlaneShape::support(const void_math::Vec3& direction) const {
    // For a plane, support in a direction that points into the plane is the plane itself
    // Otherwise it's at infinity
    constexpr float inf = 1e10f;
    float d = void_math::dot(direction, m_normal);
    if (d < 0.0f) {
        return m_normal * m_distance;
    }
    return direction * inf;
}

std::unique_ptr<IShape> PlaneShape::clone() const {
    auto cloned = std::make_unique<PlaneShape>(m_normal, m_distance);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

// =============================================================================
// ConvexHullShape Implementation
// =============================================================================

ConvexHullShape::ConvexHullShape(std::vector<void_math::Vec3> points)
    : m_vertices(std::move(points)) {
    compute_properties();
}

void_core::Result<std::unique_ptr<ConvexHullShape>> ConvexHullShape::from_mesh(
    const void_math::Vec3* vertices,
    std::size_t vertex_count,
    const std::uint32_t* /*indices*/,
    std::size_t /*index_count*/) {

    if (vertex_count < 4) {
        return void_core::Error{void_core::ErrorCode::InvalidArgument, "Need at least 4 vertices"};
    }

    std::vector<void_math::Vec3> points(vertices, vertices + vertex_count);
    return std::make_unique<ConvexHullShape>(std::move(points));
}

void ConvexHullShape::compute_properties() {
    if (m_vertices.empty()) return;

    // Compute bounds
    m_bounds.min = m_vertices[0];
    m_bounds.max = m_vertices[0];
    for (const auto& v : m_vertices) {
        m_bounds.min.x = std::min(m_bounds.min.x, v.x);
        m_bounds.min.y = std::min(m_bounds.min.y, v.y);
        m_bounds.min.z = std::min(m_bounds.min.z, v.z);
        m_bounds.max.x = std::max(m_bounds.max.x, v.x);
        m_bounds.max.y = std::max(m_bounds.max.y, v.y);
        m_bounds.max.z = std::max(m_bounds.max.z, v.z);
    }

    // Compute center of mass (centroid)
    m_center_of_mass = {0, 0, 0};
    for (const auto& v : m_vertices) {
        m_center_of_mass = m_center_of_mass + v;
    }
    m_center_of_mass = m_center_of_mass * (1.0f / static_cast<float>(m_vertices.size()));

    // Approximate volume from bounding box
    auto size = m_bounds.max - m_bounds.min;
    m_volume = size.x * size.y * size.z * 0.5f; // Rough approximation
}

MassProperties ConvexHullShape::compute_mass(float density) const {
    MassProperties props;
    props.mass = m_volume * density;
    // Approximate inertia as a box
    auto size = m_bounds.max - m_bounds.min;
    float m = props.mass;
    props.inertia_diagonal = {
        m * (size.y * size.y + size.z * size.z) / 12.0f,
        m * (size.x * size.x + size.z * size.z) / 12.0f,
        m * (size.x * size.x + size.y * size.y) / 12.0f
    };
    props.center_of_mass = m_center_of_mass;
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};
    return props;
}

bool ConvexHullShape::contains_point(const void_math::Vec3& point) const {
    // Check all face planes
    for (const auto& [normal, dist] : m_planes) {
        if (void_math::dot(point, normal) > dist) {
            return false;
        }
    }
    return true;
}

void_math::Vec3 ConvexHullShape::closest_point(const void_math::Vec3& point) const {
    // Simple implementation: clamp to bounds
    return void_math::Vec3{
        std::clamp(point.x, m_bounds.min.x, m_bounds.max.x),
        std::clamp(point.y, m_bounds.min.y, m_bounds.max.y),
        std::clamp(point.z, m_bounds.min.z, m_bounds.max.z)
    };
}

void_math::Vec3 ConvexHullShape::support(const void_math::Vec3& direction) const {
    if (m_vertices.empty()) return {0, 0, 0};

    float max_dot = void_math::dot(m_vertices[0], direction);
    std::size_t max_idx = 0;

    for (std::size_t i = 1; i < m_vertices.size(); ++i) {
        float d = void_math::dot(m_vertices[i], direction);
        if (d > max_dot) {
            max_dot = d;
            max_idx = i;
        }
    }

    return m_vertices[max_idx];
}

std::unique_ptr<IShape> ConvexHullShape::clone() const {
    auto cloned = std::make_unique<ConvexHullShape>(m_vertices);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

// =============================================================================
// MeshShape Implementation
// =============================================================================

MeshShape::MeshShape(
    std::vector<void_math::Vec3> vertices,
    std::vector<std::uint32_t> indices)
    : m_vertices(std::move(vertices))
    , m_indices(std::move(indices)) {

    // Compute bounds
    if (!m_vertices.empty()) {
        m_bounds.min = m_vertices[0];
        m_bounds.max = m_vertices[0];
        for (const auto& v : m_vertices) {
            m_bounds.min.x = std::min(m_bounds.min.x, v.x);
            m_bounds.min.y = std::min(m_bounds.min.y, v.y);
            m_bounds.min.z = std::min(m_bounds.min.z, v.z);
            m_bounds.max.x = std::max(m_bounds.max.x, v.x);
            m_bounds.max.y = std::max(m_bounds.max.y, v.y);
            m_bounds.max.z = std::max(m_bounds.max.z, v.z);
        }
    }

    build_bvh();
}

MassProperties MeshShape::compute_mass(float /*density*/) const {
    // Meshes don't have well-defined mass
    MassProperties props;
    props.mass = 0.0f;
    props.inertia_diagonal = {0, 0, 0};
    props.center_of_mass = m_bounds.center();
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};
    return props;
}

bool MeshShape::contains_point(const void_math::Vec3& /*point*/) const {
    // Meshes are surface-only
    return false;
}

void_math::Vec3 MeshShape::closest_point(const void_math::Vec3& point) const {
    // Simple implementation: clamp to bounds
    return void_math::Vec3{
        std::clamp(point.x, m_bounds.min.x, m_bounds.max.x),
        std::clamp(point.y, m_bounds.min.y, m_bounds.max.y),
        std::clamp(point.z, m_bounds.min.z, m_bounds.max.z)
    };
}

void_math::Vec3 MeshShape::support(const void_math::Vec3& direction) const {
    if (m_vertices.empty()) return {0, 0, 0};

    float max_dot = void_math::dot(m_vertices[0], direction);
    std::size_t max_idx = 0;

    for (std::size_t i = 1; i < m_vertices.size(); ++i) {
        float d = void_math::dot(m_vertices[i], direction);
        if (d > max_dot) {
            max_dot = d;
            max_idx = i;
        }
    }

    return m_vertices[max_idx];
}

std::unique_ptr<IShape> MeshShape::clone() const {
    auto cloned = std::make_unique<MeshShape>(m_vertices, m_indices);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

MeshShape::Triangle MeshShape::get_triangle(std::size_t index) const {
    Triangle tri;
    std::size_t base = index * 3;
    tri.indices[0] = m_indices[base];
    tri.indices[1] = m_indices[base + 1];
    tri.indices[2] = m_indices[base + 2];

    auto& v0 = m_vertices[tri.indices[0]];
    auto& v1 = m_vertices[tri.indices[1]];
    auto& v2 = m_vertices[tri.indices[2]];

    tri.normal = void_math::normalize(void_math::cross(v1 - v0, v2 - v0));
    return tri;
}

bool MeshShape::raycast(
    const void_math::Vec3& origin,
    const void_math::Vec3& direction,
    float max_distance,
    RaycastHit& hit) const {

    bool found = false;
    float closest = max_distance;

    for (std::size_t i = 0; i < triangle_count(); ++i) {
        auto tri = get_triangle(i);
        auto& v0 = m_vertices[tri.indices[0]];
        auto& v1 = m_vertices[tri.indices[1]];
        auto& v2 = m_vertices[tri.indices[2]];

        // Möller–Trumbore intersection
        auto e1 = v1 - v0;
        auto e2 = v2 - v0;
        auto h = void_math::cross(direction, e2);
        float a = void_math::dot(e1, h);

        if (std::abs(a) < 0.0001f) continue;

        float f = 1.0f / a;
        auto s = origin - v0;
        float u = f * void_math::dot(s, h);

        if (u < 0.0f || u > 1.0f) continue;

        auto q = void_math::cross(s, e1);
        float v = f * void_math::dot(direction, q);

        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * void_math::dot(e2, q);

        if (t > 0.0001f && t < closest) {
            closest = t;
            hit.distance = t;
            hit.position = origin + direction * t;
            hit.normal = tri.normal;
            found = true;
        }
    }

    return found;
}

void MeshShape::build_bvh() {
    // Simple single-node BVH for now
    if (m_indices.empty()) return;

    BVHNode root;
    root.bounds = m_bounds;
    root.first_triangle = 0;
    root.triangle_count = static_cast<std::uint32_t>(m_indices.size() / 3);
    root.left_child = 0;
    m_bvh.push_back(root);
}

// =============================================================================
// HeightfieldShape Implementation
// =============================================================================

HeightfieldShape::HeightfieldShape(
    std::uint32_t width,
    std::uint32_t height,
    std::vector<float> heights,
    const void_math::Vec3& scale)
    : m_width(width)
    , m_depth(height)
    , m_heights(std::move(heights))
    , m_scale(scale) {
    compute_bounds();
}

void HeightfieldShape::compute_bounds() {
    if (m_heights.empty()) return;

    m_min_height = m_heights[0];
    m_max_height = m_heights[0];

    for (float h : m_heights) {
        m_min_height = std::min(m_min_height, h);
        m_max_height = std::max(m_max_height, h);
    }

    m_bounds.min = {0, m_min_height * m_scale.y, 0};
    m_bounds.max = {
        static_cast<float>(m_width - 1) * m_scale.x,
        m_max_height * m_scale.y,
        static_cast<float>(m_depth - 1) * m_scale.z
    };
}

MassProperties HeightfieldShape::compute_mass(float /*density*/) const {
    MassProperties props;
    props.mass = 0.0f; // Static terrain
    props.inertia_diagonal = {0, 0, 0};
    props.center_of_mass = m_bounds.center();
    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};
    return props;
}

bool HeightfieldShape::contains_point(const void_math::Vec3& point) const {
    float h = get_height_at(point.x, point.z);
    return point.y <= h;
}

void_math::Vec3 HeightfieldShape::closest_point(const void_math::Vec3& point) const {
    float x = std::clamp(point.x, m_bounds.min.x, m_bounds.max.x);
    float z = std::clamp(point.z, m_bounds.min.z, m_bounds.max.z);
    float h = get_height_at(x, z);
    return {x, h, z};
}

void_math::Vec3 HeightfieldShape::support(const void_math::Vec3& direction) const {
    // Support in Y direction
    if (direction.y > 0) {
        return {m_bounds.max.x / 2, m_max_height * m_scale.y, m_bounds.max.z / 2};
    }
    return {m_bounds.max.x / 2, m_min_height * m_scale.y, m_bounds.max.z / 2};
}

std::unique_ptr<IShape> HeightfieldShape::clone() const {
    auto cloned = std::make_unique<HeightfieldShape>(m_width, m_depth, m_heights, m_scale);
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

float HeightfieldShape::get_height(std::uint32_t x, std::uint32_t z) const {
    if (x >= m_width || z >= m_depth) return 0.0f;
    return m_heights[z * m_width + x];
}

float HeightfieldShape::get_height_at(float x, float z) const {
    // Convert to grid coordinates
    float gx = x / m_scale.x;
    float gz = z / m_scale.z;

    // Clamp to valid range
    gx = std::clamp(gx, 0.0f, static_cast<float>(m_width - 1));
    gz = std::clamp(gz, 0.0f, static_cast<float>(m_depth - 1));

    // Bilinear interpolation
    std::uint32_t x0 = static_cast<std::uint32_t>(gx);
    std::uint32_t z0 = static_cast<std::uint32_t>(gz);
    std::uint32_t x1 = std::min(x0 + 1, m_width - 1);
    std::uint32_t z1 = std::min(z0 + 1, m_depth - 1);

    float fx = gx - static_cast<float>(x0);
    float fz = gz - static_cast<float>(z0);

    float h00 = get_height(x0, z0);
    float h10 = get_height(x1, z0);
    float h01 = get_height(x0, z1);
    float h11 = get_height(x1, z1);

    float h0 = h00 * (1.0f - fx) + h10 * fx;
    float h1 = h01 * (1.0f - fx) + h11 * fx;

    return (h0 * (1.0f - fz) + h1 * fz) * m_scale.y;
}

void_math::Vec3 HeightfieldShape::get_normal_at(float x, float z) const {
    const float delta = 0.1f;
    float h = get_height_at(x, z);
    float hx = get_height_at(x + delta, z);
    float hz = get_height_at(x, z + delta);

    void_math::Vec3 dx{delta, hx - h, 0};
    void_math::Vec3 dz{0, hz - h, delta};

    return void_math::normalize(void_math::cross(dz, dx));
}

void HeightfieldShape::set_heights(const std::vector<float>& heights) {
    m_heights = heights;
    compute_bounds();
}

void HeightfieldShape::set_height(std::uint32_t x, std::uint32_t z, float height) {
    if (x < m_width && z < m_depth) {
        m_heights[z * m_width + x] = height;
        m_min_height = std::min(m_min_height, height);
        m_max_height = std::max(m_max_height, height);
    }
}

// =============================================================================
// CompoundShape Implementation
// =============================================================================

void CompoundShape::add_child(
    std::unique_ptr<IShape> shape,
    const void_math::Vec3& position,
    const void_math::Quat& rotation) {

    m_children.push_back({std::move(shape), position, rotation});
    recompute_properties();
}

void CompoundShape::remove_child(std::size_t index) {
    if (index < m_children.size()) {
        m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));
        recompute_properties();
    }
}

void CompoundShape::recompute_properties() {
    if (m_children.empty()) {
        m_bounds = void_math::AABB{};
        return;
    }

    // Get first child's transformed bounds
    auto& first = m_children[0];
    auto child_bounds = first.shape->local_bounds();
    m_bounds.min = child_bounds.min + first.position;
    m_bounds.max = child_bounds.max + first.position;

    // Expand to include all children
    for (std::size_t i = 1; i < m_children.size(); ++i) {
        auto& child = m_children[i];
        auto cb = child.shape->local_bounds();
        auto cmin = cb.min + child.position;
        auto cmax = cb.max + child.position;

        m_bounds.min.x = std::min(m_bounds.min.x, cmin.x);
        m_bounds.min.y = std::min(m_bounds.min.y, cmin.y);
        m_bounds.min.z = std::min(m_bounds.min.z, cmin.z);
        m_bounds.max.x = std::max(m_bounds.max.x, cmax.x);
        m_bounds.max.y = std::max(m_bounds.max.y, cmax.y);
        m_bounds.max.z = std::max(m_bounds.max.z, cmax.z);
    }
}

float CompoundShape::volume() const {
    float total = 0.0f;
    for (const auto& child : m_children) {
        total += child.shape->volume();
    }
    return total;
}

MassProperties CompoundShape::compute_mass(float density) const {
    MassProperties props;
    props.mass = 0.0f;
    props.center_of_mass = {0, 0, 0};
    props.inertia_diagonal = {0, 0, 0};

    for (const auto& child : m_children) {
        auto child_props = child.shape->compute_mass(density);
        props.mass += child_props.mass;
        props.center_of_mass = props.center_of_mass + (child_props.center_of_mass + child.position) * child_props.mass;
    }

    if (props.mass > 0.0f) {
        props.center_of_mass = props.center_of_mass * (1.0f / props.mass);
    }

    props.inertia_rotation = void_math::Quat{1.0f, 0.0f, 0.0f, 0.0f};
    return props;
}

bool CompoundShape::contains_point(const void_math::Vec3& point) const {
    for (const auto& child : m_children) {
        auto local_point = point - child.position;
        // TODO: Apply rotation
        if (child.shape->contains_point(local_point)) {
            return true;
        }
    }
    return false;
}

void_math::Vec3 CompoundShape::closest_point(const void_math::Vec3& point) const {
    if (m_children.empty()) return point;

    auto closest = m_children[0].shape->closest_point(point - m_children[0].position) + m_children[0].position;
    float closest_dist = void_math::length(point - closest);

    for (std::size_t i = 1; i < m_children.size(); ++i) {
        auto& child = m_children[i];
        auto cp = child.shape->closest_point(point - child.position) + child.position;
        float dist = void_math::length(point - cp);
        if (dist < closest_dist) {
            closest_dist = dist;
            closest = cp;
        }
    }

    return closest;
}

void_math::Vec3 CompoundShape::support(const void_math::Vec3& direction) const {
    if (m_children.empty()) return {0, 0, 0};

    auto sup = m_children[0].shape->support(direction) + m_children[0].position;
    float max_dot = void_math::dot(sup, direction);

    for (std::size_t i = 1; i < m_children.size(); ++i) {
        auto& child = m_children[i];
        auto s = child.shape->support(direction) + child.position;
        float d = void_math::dot(s, direction);
        if (d > max_dot) {
            max_dot = d;
            sup = s;
        }
    }

    return sup;
}

std::unique_ptr<IShape> CompoundShape::clone() const {
    auto cloned = std::make_unique<CompoundShape>();
    for (const auto& child : m_children) {
        cloned->add_child(child.shape->clone(), child.position, child.rotation);
    }
    cloned->set_material(m_material);
    cloned->set_local_transform(m_local_offset, m_local_rotation);
    return cloned;
}

} // namespace void_physics
