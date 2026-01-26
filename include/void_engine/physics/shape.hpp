/// @file shape.hpp
/// @brief Collision shape definitions for void_physics

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/math/vec.hpp>
#include <void_engine/math/quat.hpp>
#include <void_engine/math/bounds.hpp>
#include <void_engine/core/error.hpp>

#include <memory>
#include <vector>
#include <variant>

namespace void_physics {

// =============================================================================
// Shape Interface
// =============================================================================

/// Base interface for all collision shapes
class IShape {
public:
    virtual ~IShape() = default;

    /// Get shape type
    [[nodiscard]] virtual ShapeType type() const noexcept = 0;

    /// Get unique shape ID
    [[nodiscard]] virtual ShapeId id() const noexcept = 0;

    /// Get local bounds
    [[nodiscard]] virtual void_math::AABB local_bounds() const = 0;

    /// Get volume
    [[nodiscard]] virtual float volume() const = 0;

    /// Compute mass properties from density
    [[nodiscard]] virtual MassProperties compute_mass(float density) const = 0;

    /// Get local center of mass
    [[nodiscard]] virtual void_math::Vec3 center_of_mass() const = 0;

    /// Test if point is inside shape (local space)
    [[nodiscard]] virtual bool contains_point(const void_math::Vec3& point) const = 0;

    /// Get closest point on surface (local space)
    [[nodiscard]] virtual void_math::Vec3 closest_point(const void_math::Vec3& point) const = 0;

    /// Get support point in direction (for GJK)
    [[nodiscard]] virtual void_math::Vec3 support(const void_math::Vec3& direction) const = 0;

    /// Clone the shape
    [[nodiscard]] virtual std::unique_ptr<IShape> clone() const = 0;

    /// Check if shape is convex
    [[nodiscard]] virtual bool is_convex() const noexcept = 0;

    /// Get physics material
    [[nodiscard]] MaterialId material() const noexcept { return m_material; }

    /// Set physics material
    void set_material(MaterialId mat) { m_material = mat; }

    /// Set shape ID
    void set_id(ShapeId id) { m_id = id; }

    /// Get local transform offset
    [[nodiscard]] const void_math::Vec3& local_offset() const noexcept { return m_local_offset; }
    [[nodiscard]] const void_math::Quat& local_rotation() const noexcept { return m_local_rotation; }

    /// Get local transform as struct
    [[nodiscard]] void_math::Transform local_transform() const noexcept {
        return void_math::Transform{m_local_offset, m_local_rotation, void_math::Vec3{1, 1, 1}};
    }

    /// Set local transform
    void set_local_transform(const void_math::Vec3& offset, const void_math::Quat& rotation = {}) {
        m_local_offset = offset;
        m_local_rotation = rotation;
    }

    /// Set local transform from Transform struct
    void set_local_transform(const void_math::Transform& t) {
        m_local_offset = t.position;
        m_local_rotation = t.rotation;
    }

protected:
    ShapeId m_id{0};
    MaterialId m_material;
    void_math::Vec3 m_local_offset{0, 0, 0};
    void_math::Quat m_local_rotation{};
};

// =============================================================================
// Box Shape
// =============================================================================

/// Axis-aligned box collision shape
class BoxShape : public IShape {
public:
    /// Create box with half-extents
    explicit BoxShape(const void_math::Vec3& half_extents)
        : m_half_extents(half_extents) {}

    /// Create box with dimensions
    [[nodiscard]] static std::unique_ptr<BoxShape> from_dimensions(
        float width, float height, float depth)
    {
        return std::make_unique<BoxShape>(void_math::Vec3{width / 2, height / 2, depth / 2});
    }

    /// Create cube
    [[nodiscard]] static std::unique_ptr<BoxShape> cube(float size) {
        return std::make_unique<BoxShape>(void_math::Vec3{size / 2, size / 2, size / 2});
    }

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Box; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override {
        return void_math::AABB{-m_half_extents + m_local_offset, m_half_extents + m_local_offset};
    }

    [[nodiscard]] float volume() const override {
        return 8.0f * m_half_extents.x * m_half_extents.y * m_half_extents.z;
    }

    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_local_offset; }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return true; }

    /// Get half extents
    [[nodiscard]] const void_math::Vec3& half_extents() const noexcept { return m_half_extents; }

    /// Get full dimensions
    [[nodiscard]] void_math::Vec3 dimensions() const { return m_half_extents * 2.0f; }

private:
    void_math::Vec3 m_half_extents;
};

// =============================================================================
// Sphere Shape
// =============================================================================

/// Sphere collision shape
class SphereShape : public IShape {
public:
    explicit SphereShape(float radius) : m_radius(radius) {}

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Sphere; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override {
        void_math::Vec3 r{m_radius, m_radius, m_radius};
        return void_math::AABB{m_local_offset - r, m_local_offset + r};
    }

    [[nodiscard]] float volume() const override {
        return (4.0f / 3.0f) * 3.14159265359f * m_radius * m_radius * m_radius;
    }

    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_local_offset; }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return true; }

    /// Get radius
    [[nodiscard]] float radius() const noexcept { return m_radius; }

    /// Get center position (local space)
    [[nodiscard]] void_math::Vec3 center() const noexcept { return m_local_offset; }

private:
    float m_radius;
};

// =============================================================================
// Capsule Shape
// =============================================================================

/// Capsule collision shape (cylinder with hemispherical caps)
class CapsuleShape : public IShape {
public:
    /// Create capsule along axis
    /// @param radius Capsule radius
    /// @param height Total height (including caps)
    /// @param axis 0=X, 1=Y (default), 2=Z
    CapsuleShape(float radius, float height, int axis = 1)
        : m_radius(radius)
        , m_half_height((height - 2.0f * radius) * 0.5f)
        , m_axis(axis) {}

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Capsule; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override;
    [[nodiscard]] float volume() const override;
    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_local_offset; }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return true; }

    /// Get radius
    [[nodiscard]] float radius() const noexcept { return m_radius; }

    /// Get half height of cylinder part
    [[nodiscard]] float half_height() const noexcept { return m_half_height; }

    /// Get total height
    [[nodiscard]] float height() const noexcept { return 2.0f * (m_half_height + m_radius); }

    /// Get axis (0=X, 1=Y, 2=Z)
    [[nodiscard]] int axis() const noexcept { return m_axis; }

    /// Get the two endpoint centers
    [[nodiscard]] std::pair<void_math::Vec3, void_math::Vec3> endpoints() const;

private:
    float m_radius;
    float m_half_height;
    int m_axis;
};

// =============================================================================
// Cylinder Shape
// =============================================================================

/// Cylinder collision shape (flat ends, no caps)
class CylinderShape : public IShape {
public:
    /// Create cylinder along axis
    /// @param radius Cylinder radius
    /// @param height Total height
    /// @param axis 0=X, 1=Y (default), 2=Z
    CylinderShape(float radius, float height, int axis = 1)
        : m_radius(radius)
        , m_half_height(height * 0.5f)
        , m_axis(axis) {}

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Cylinder; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override {
        void_math::Vec3 ext{m_radius, m_radius, m_radius};
        ext[m_axis] = m_half_height;
        return void_math::AABB{m_local_offset - ext, m_local_offset + ext};
    }

    [[nodiscard]] float volume() const override {
        return 3.14159265359f * m_radius * m_radius * (2.0f * m_half_height);
    }

    [[nodiscard]] MassProperties compute_mass(float density) const override {
        float mass = volume() * density;
        // Moment of inertia for cylinder
        float r2 = m_radius * m_radius;
        float h2 = (2.0f * m_half_height) * (2.0f * m_half_height);
        float ix = mass * (3.0f * r2 + h2) / 12.0f;
        float iy = mass * r2 / 2.0f;
        void_math::Vec3 inertia{ix, iy, ix};
        if (m_axis == 0) inertia = {iy, ix, ix};
        else if (m_axis == 2) inertia = {ix, ix, iy};
        return MassProperties{mass, inertia, center_of_mass()};
    }

    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_local_offset; }

    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override {
        void_math::Vec3 local = point - m_local_offset;
        float axial = local[m_axis];
        if (std::abs(axial) > m_half_height) return false;
        local[m_axis] = 0;
        return void_math::length(local) <= m_radius;
    }

    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override {
        void_math::Vec3 local = point - m_local_offset;
        float axial = std::clamp(local[m_axis], -m_half_height, m_half_height);
        local[m_axis] = 0;
        float dist = void_math::length(local);
        if (dist > m_radius) {
            local = local * (m_radius / dist);
        }
        local[m_axis] = axial;
        return local + m_local_offset;
    }

    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override {
        void_math::Vec3 result = m_local_offset;
        // Axial component
        result[m_axis] += (direction[m_axis] > 0) ? m_half_height : -m_half_height;
        // Radial component
        void_math::Vec3 radial = direction;
        radial[m_axis] = 0;
        float len = void_math::length(radial);
        if (len > 0.0001f) {
            radial = radial * (m_radius / len);
            result = result + radial;
        }
        return result;
    }

    [[nodiscard]] std::unique_ptr<IShape> clone() const override {
        auto c = std::make_unique<CylinderShape>(m_radius, m_half_height * 2.0f, m_axis);
        c->set_material(m_material);
        c->set_local_transform(m_local_offset, m_local_rotation);
        c->set_id(m_id);
        return c;
    }

    [[nodiscard]] bool is_convex() const noexcept override { return true; }

    /// Get radius
    [[nodiscard]] float radius() const noexcept { return m_radius; }

    /// Get half height
    [[nodiscard]] float half_height() const noexcept { return m_half_height; }

    /// Get total height
    [[nodiscard]] float height() const noexcept { return 2.0f * m_half_height; }

    /// Get axis (0=X, 1=Y, 2=Z)
    [[nodiscard]] int axis() const noexcept { return m_axis; }

private:
    float m_radius;
    float m_half_height;
    int m_axis;
};

// =============================================================================
// Plane Shape
// =============================================================================

/// Infinite plane collision shape
class PlaneShape : public IShape {
public:
    /// Create plane from normal and distance
    PlaneShape(const void_math::Vec3& normal, float distance)
        : m_normal(void_math::normalize(normal))
        , m_distance(distance) {}

    /// Create plane from point and normal
    [[nodiscard]] static std::unique_ptr<PlaneShape> from_point_normal(
        const void_math::Vec3& point, const void_math::Vec3& normal)
    {
        auto n = void_math::normalize(normal);
        return std::make_unique<PlaneShape>(n, void_math::dot(point, n));
    }

    /// Create XZ ground plane at Y=0
    [[nodiscard]] static std::unique_ptr<PlaneShape> ground() {
        return std::make_unique<PlaneShape>(void_math::Vec3{0, 1, 0}, 0.0f);
    }

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Plane; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override {
        // Planes are infinite, return large bounds
        constexpr float inf = 1e10f;
        return void_math::AABB{
            void_math::Vec3{-inf, -inf, -inf},
            void_math::Vec3{inf, inf, inf}
        };
    }

    [[nodiscard]] float volume() const override { return 0.0f; } // Infinite
    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_normal * m_distance; }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return true; }

    /// Get plane normal
    [[nodiscard]] const void_math::Vec3& normal() const noexcept { return m_normal; }

    /// Get distance from origin
    [[nodiscard]] float distance() const noexcept { return m_distance; }

    /// Get signed distance to point
    [[nodiscard]] float signed_distance(const void_math::Vec3& point) const {
        return void_math::dot(point, m_normal) - m_distance;
    }

private:
    void_math::Vec3 m_normal;
    float m_distance;
};

// =============================================================================
// Convex Hull Shape
// =============================================================================

/// Convex hull collision shape
class ConvexHullShape : public IShape {
public:
    /// Create convex hull from points
    explicit ConvexHullShape(std::vector<void_math::Vec3> points);

    /// Create from indexed mesh (will compute convex hull)
    [[nodiscard]] static void_core::Result<std::unique_ptr<ConvexHullShape>> from_mesh(
        const void_math::Vec3* vertices,
        std::size_t vertex_count,
        const std::uint32_t* indices = nullptr,
        std::size_t index_count = 0);

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::ConvexHull; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override { return m_bounds; }
    [[nodiscard]] float volume() const override { return m_volume; }
    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_center_of_mass; }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return true; }

    /// Get vertices
    [[nodiscard]] const std::vector<void_math::Vec3>& vertices() const noexcept { return m_vertices; }

    /// Get face planes (precomputed)
    [[nodiscard]] const std::vector<std::pair<void_math::Vec3, float>>& planes() const noexcept {
        return m_planes;
    }

private:
    void compute_properties();

    std::vector<void_math::Vec3> m_vertices;
    std::vector<std::pair<void_math::Vec3, float>> m_planes; // Normal + distance
    void_math::AABB m_bounds;
    void_math::Vec3 m_center_of_mass;
    float m_volume = 0.0f;
};

// =============================================================================
// Triangle Mesh Shape
// =============================================================================

/// Triangle mesh collision shape (for static geometry)
class MeshShape : public IShape {
public:
    struct Triangle {
        std::uint32_t indices[3];
        void_math::Vec3 normal;
    };

    /// Create mesh shape from vertices and indices
    MeshShape(
        std::vector<void_math::Vec3> vertices,
        std::vector<std::uint32_t> indices);

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::TriangleMesh; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override { return m_bounds; }
    [[nodiscard]] float volume() const override { return 0.0f; } // Not well-defined for mesh
    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_bounds.center(); }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return false; }

    /// Get vertices
    [[nodiscard]] const std::vector<void_math::Vec3>& vertices() const noexcept { return m_vertices; }

    /// Get indices
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept { return m_indices; }

    /// Get triangle count
    [[nodiscard]] std::size_t triangle_count() const noexcept { return m_indices.size() / 3; }

    /// Get triangle by index
    [[nodiscard]] Triangle get_triangle(std::size_t index) const;

    /// Raycast against mesh
    [[nodiscard]] bool raycast(
        const void_math::Vec3& origin,
        const void_math::Vec3& direction,
        float max_distance,
        RaycastHit& hit) const;

private:
    void build_bvh();

    std::vector<void_math::Vec3> m_vertices;
    std::vector<std::uint32_t> m_indices;
    void_math::AABB m_bounds;

    // Simple BVH for acceleration
    struct BVHNode {
        void_math::AABB bounds;
        std::uint32_t first_triangle;
        std::uint32_t triangle_count;
        std::uint32_t left_child;  // 0 = leaf
    };
    std::vector<BVHNode> m_bvh;
};

// =============================================================================
// Heightfield Shape
// =============================================================================

/// Heightfield collision shape (for terrain)
class HeightfieldShape : public IShape {
public:
    /// Create heightfield
    /// @param width Number of samples in X
    /// @param height Number of samples in Z
    /// @param heights Height values (row-major, size = width * height)
    /// @param scale Scale in X, Y (height), Z
    HeightfieldShape(
        std::uint32_t width,
        std::uint32_t height,
        std::vector<float> heights,
        const void_math::Vec3& scale = {1, 1, 1});

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Heightfield; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override { return m_bounds; }
    [[nodiscard]] float volume() const override { return 0.0f; }
    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_bounds.center(); }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return false; }

    /// Get dimensions
    [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] std::uint32_t depth() const noexcept { return m_depth; }

    /// Get height at grid position
    [[nodiscard]] float get_height(std::uint32_t x, std::uint32_t z) const;

    /// Get interpolated height at world position
    [[nodiscard]] float get_height_at(float x, float z) const;

    /// Get normal at world position
    [[nodiscard]] void_math::Vec3 get_normal_at(float x, float z) const;

    /// Update heights (for terrain deformation)
    void set_heights(const std::vector<float>& heights);

    /// Update single height
    void set_height(std::uint32_t x, std::uint32_t z, float height);

private:
    void compute_bounds();

    std::uint32_t m_width;
    std::uint32_t m_depth;
    std::vector<float> m_heights;
    void_math::Vec3 m_scale;
    void_math::AABB m_bounds;
    float m_min_height = 0.0f;
    float m_max_height = 0.0f;
};

// =============================================================================
// Compound Shape
// =============================================================================

/// Compound shape combining multiple child shapes
class CompoundShape : public IShape {
public:
    struct ChildShape {
        std::unique_ptr<IShape> shape;
        void_math::Vec3 position;
        void_math::Quat rotation;
    };

    CompoundShape() = default;

    /// Add child shape
    void add_child(
        std::unique_ptr<IShape> shape,
        const void_math::Vec3& position = {},
        const void_math::Quat& rotation = {});

    /// Remove child at index
    void remove_child(std::size_t index);

    /// Get child count
    [[nodiscard]] std::size_t child_count() const noexcept { return m_children.size(); }

    /// Get child at index
    [[nodiscard]] const ChildShape& get_child(std::size_t index) const { return m_children[index]; }

    [[nodiscard]] ShapeType type() const noexcept override { return ShapeType::Compound; }
    [[nodiscard]] ShapeId id() const noexcept override { return m_id; }

    [[nodiscard]] void_math::AABB local_bounds() const override { return m_bounds; }
    [[nodiscard]] float volume() const override;
    [[nodiscard]] MassProperties compute_mass(float density) const override;
    [[nodiscard]] void_math::Vec3 center_of_mass() const override { return m_center_of_mass; }
    [[nodiscard]] bool contains_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 closest_point(const void_math::Vec3& point) const override;
    [[nodiscard]] void_math::Vec3 support(const void_math::Vec3& direction) const override;
    [[nodiscard]] std::unique_ptr<IShape> clone() const override;
    [[nodiscard]] bool is_convex() const noexcept override { return false; }

private:
    void recompute_properties();

    std::vector<ChildShape> m_children;
    void_math::AABB m_bounds;
    void_math::Vec3 m_center_of_mass{0, 0, 0};
};

// =============================================================================
// Shape Factory
// =============================================================================

/// Factory for creating collision shapes
class ShapeFactory {
public:
    /// Create box shape
    [[nodiscard]] static std::unique_ptr<BoxShape> box(const void_math::Vec3& half_extents);
    [[nodiscard]] static std::unique_ptr<BoxShape> box(float hx, float hy, float hz);

    /// Create sphere shape
    [[nodiscard]] static std::unique_ptr<SphereShape> sphere(float radius);

    /// Create capsule shape
    [[nodiscard]] static std::unique_ptr<CapsuleShape> capsule(float radius, float height, int axis = 1);

    /// Create plane shape
    [[nodiscard]] static std::unique_ptr<PlaneShape> plane(const void_math::Vec3& normal, float distance);

    /// Create convex hull from points
    [[nodiscard]] static void_core::Result<std::unique_ptr<ConvexHullShape>> convex_hull(
        const std::vector<void_math::Vec3>& points);

    /// Create mesh shape
    [[nodiscard]] static std::unique_ptr<MeshShape> mesh(
        const std::vector<void_math::Vec3>& vertices,
        const std::vector<std::uint32_t>& indices);

    /// Create heightfield
    [[nodiscard]] static std::unique_ptr<HeightfieldShape> heightfield(
        std::uint32_t width,
        std::uint32_t depth,
        const std::vector<float>& heights,
        const void_math::Vec3& scale = {1, 1, 1});

    /// Create compound shape
    [[nodiscard]] static std::unique_ptr<CompoundShape> compound();
};

} // namespace void_physics
